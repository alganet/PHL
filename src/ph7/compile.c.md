# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 7341/9067 lines (80.96%)

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
|    149160 |   139 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|         5 |   140 | `{` |
|    149165 |   141 | `	GenBlock *pBlock = pCurrent;` |
|    336379 |   142 | `	for(;;){` |
|    672763 |   143 | `		if( pBlock->iFlags & iBlockType ){` |
|    149165 |   144 | `			iCount--; /* Decrement nesting level */` |
|    149165 |   145 | `			if( iCount < 1 ){` |
|         - |   146 | `				/* Block meet with the desired criteria */` |
|    149139 |   147 | `				return pBlock;` |
|         - |   148 | `			}` |
|        13 |   149 | `		}` |
|         - |   150 | `		/* Point to the upper block */` |
|    523629 |   151 | `		pBlock = pBlock->pParent;` |
|    523629 |   152 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|         - |   153 | `			/* Forbidden */` |
|        16 |   154 | `			break;` |
|         - |   155 | `		}` |
|         5 |   156 | `	}` |
|         - |   157 | `	/* No such block */` |
|        29 |   158 | `	return 0;` |
|     74585 |   159 | `}` |
|         - |   160 | `/*` |
|         - |   161 | ` * Initialize a freshly allocated block instance.` |
|         - |   162 | ` */` |
|  11530856 |   163 | `static void GenStateInitBlock(` |
|         - |   164 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |   165 | `	GenBlock *pBlock,    /* Target block */` |
|         - |   166 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   167 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|         - |   168 | `	void *pUserData      /* Upper layer private data */` |
|         - |   169 | `	)` |
|         5 |   170 | `{` |
|         - |   171 | `	/* Initialize block fields */` |
|  11530861 |   172 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  11530861 |   173 | `	pBlock->pUserData   = pUserData;` |
|  11530861 |   174 | `	pBlock->pGen        = pGen;` |
|  11530861 |   175 | `	pBlock->iFlags      = iType;` |
|  11530861 |   176 | `	pBlock->pParent     = 0;` |
|  11530861 |   177 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  11530861 |   178 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  11530861 |   179 | `}` |
|         - |   180 | `/*` |
|         - |   181 | ` * Allocate a new block instance.` |
|         - |   182 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|         - |   183 | ` * on success.Otherwise generate a compile-time error and abort` |
|         - |   184 | ` * processing on failure.` |
|         - |   185 | ` */` |
|  11527032 |   186 | `static sxi32 GenStateEnterBlock(` |
|         - |   187 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |   188 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   189 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|         - |   190 | `	void *pUserData,      /* Upper layer private data */` |
|         - |   191 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|         - |   192 | `	)` |
|         5 |   193 | `{` |
|         - |   194 | `	GenBlock *pBlock;` |
|         - |   195 | `	/* Allocate a new block instance */` |
|  11527037 |   196 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  11527037 |   197 | `	if( pBlock == 0 ){` |
|         - |   198 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |   199 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |   200 | `		 */` |
|       ! 0 |   201 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|         - |   202 | `		/* Abort processing immediately */` |
|       ! 0 |   203 | `		return SXERR_ABORT;` |
|         - |   204 | `	}` |
|         - |   205 | `	/* Zero the structure */` |
|  11527037 |   206 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  11527037 |   207 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|         - |   208 | `	/* Link to the parent block */` |
|  11527037 |   209 | `	pBlock->pParent = pGen->pCurrent;` |
|         - |   210 | `	/* A loop or switch gets an id, and remembers the loop it nests inside, so a goto's` |
|         - |   211 | `	 * and a label's positions can be compared after compilation (see aLoopParent). */` |
|  11527037 |   212 | `	if( iType & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    501513 |   213 | `		sxu32 nParent = pGen->nCurLoopId;` |
|    501513 |   214 | `		pGen->nLoopId++;` |
|    501513 |   215 | `		SySetPut(&pGen->aLoopParent,(const void *)&nParent);` |
|    501513 |   216 | `		pBlock->nLoopId = pGen->nLoopId;` |
|    501513 |   217 | `		pBlock->nOuterLoopId = nParent;` |
|    501513 |   218 | `		pGen->nCurLoopId = pGen->nLoopId;` |
|    250754 |   219 | `	}` |
|         - |   220 | `	/* Mark as the current block */` |
|  11527037 |   221 | `	pGen->pCurrent = pBlock;` |
|  11527037 |   222 | `	if( ppBlock ){` |
|         - |   223 | `		/* Write a pointer to the new instance */` |
|   5519593 |   224 | `		*ppBlock = pBlock;` |
|   2759794 |   225 | `	}` |
|  11527037 |   226 | `	return SXRET_OK;` |
|   5763521 |   227 | `}` |
|         - |   228 | `/*` |
|         - |   229 | ` * Release block fields without freeing the whole instance.` |
|         - |   230 | ` */` |
|  11527020 |   231 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|         5 |   232 | `{` |
|  11527025 |   233 | `	SySetRelease(&pBlock->aPostContFix);` |
|  11527025 |   234 | `	SySetRelease(&pBlock->aJumpFix);` |
|  11527025 |   235 | `}` |
|         - |   236 | `/*` |
|         - |   237 | ` * Release a block.` |
|         - |   238 | ` */` |
|  11527016 |   239 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|         5 |   240 | `{` |
|  11527021 |   241 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  11527021 |   242 | `	GenStateReleaseBlock(&(*pBlock));` |
|         - |   243 | `	/* Free the instance */` |
|  11527021 |   244 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  11527021 |   245 | `}` |
|         - |   246 | `/*` |
|         - |   247 | ` * POP and release a block from the stack of compiled blocks.` |
|         - |   248 | ` */` |
|  11527016 |   249 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|         5 |   250 | `{` |
|  11527021 |   251 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  11527021 |   252 | `	if( pBlock == 0 ){` |
|         - |   253 | `		/* No more block to pop */` |
|       ! 0 |   254 | `		return SXERR_EMPTY;` |
|         - |   255 | `	}` |
|  11527021 |   256 | `	if( pBlock->iFlags & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    501505 |   257 | `		pGen->nCurLoopId = pBlock->nOuterLoopId;` |
|    250750 |   258 | `	}` |
|         - |   259 | `	/* Point to the upper block */` |
|  11527021 |   260 | `	pGen->pCurrent = pBlock->pParent;` |
|  11527021 |   261 | `	if( ppBlock ){` |
|         - |   262 | `		/* Write a pointer to the popped block */` |
|       ! 0 |   263 | `		*ppBlock = pBlock;` |
|       ! 0 |   264 | `	}else{` |
|         - |   265 | `		/* Safely release the block */` |
|  11527021 |   266 | `		GenStateFreeBlock(&(*pBlock));` |
|         - |   267 | `	}` |
|  11527021 |   268 | `	return SXRET_OK;` |
|   5763513 |   269 | `}` |
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
|   4380518 |   280 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|         5 |   281 | `{` |
|         - |   282 | `	JumpFixup sJumpFix;` |
|         - |   283 | `	sxi32 rc;` |
|         - |   284 | `	/* Init the JumpFixup structure */` |
|   4380523 |   285 | `	sJumpFix.nJumpType = nJumpType;` |
|   4380523 |   286 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|         - |   287 | `	/* Insert in the jump fixup table */` |
|   4380523 |   288 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|   4380523 |   289 | `	return rc;` |
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
|   8083732 |   302 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|         5 |   303 | `{` |
|         - |   304 | `	JumpFixup *aFix;` |
|         - |   305 | `	VmInstr *pInstr;` |
|         - |   306 | `	sxu32 nFixed;` |
|         - |   307 | `	sxu32 n;` |
|         - |   308 | `	/* Point to the jump fixup table */` |
|   8083737 |   309 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|         - |   310 | `	/* Fix the desired jumps */` |
|  17411911 |   311 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|   9328179 |   312 | `		if( aFix[n].nJumpType < 0 ){` |
|         - |   313 | `			/* Already fixed */` |
|   3479935 |   314 | `			continue;` |
|         - |   315 | `		}` |
|   5848249 |   316 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|         - |   317 | `			/* Not of our interest */` |
|   1467733 |   318 | `			continue;` |
|         - |   319 | `		}` |
|         - |   320 | `		/* Point to the instruction to fix */` |
|   4380521 |   321 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|   4380521 |   322 | `		if( pInstr ){` |
|   4380521 |   323 | `			pInstr->iP2 = nJumpDest;` |
|   4380521 |   324 | `			nFixed++;` |
|         - |   325 | `			/* Mark as fixed */` |
|   4380521 |   326 | `			aFix[n].nJumpType = -1;` |
|   2190258 |   327 | `		}` |
|   2190263 |   328 | `	}` |
|         - |   329 | `	/* Total number of fixed jumps */` |
|   8083737 |   330 | `	return nFixed;` |
|         5 |   331 | `}` |
|         - |   332 | `/*` |
|         - |   333 | ` * Fix a 'goto' now the jump destination is resolved.` |
|         - |   334 | ` * The goto statement can be used to jump to another section` |
|         - |   335 | ` * in the program.` |
|         - |   336 | ` * Refer to the routine responsible of compiling the goto` |
|         - |   337 | ` * statement for more information.` |
|         - |   338 | ` */` |
|   2817940 |   339 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|         5 |   340 | `{` |
|         - |   341 | `	JumpFixup *pJump,*aJumps;` |
|         - |   342 | `	Label *pLabel;` |
|         - |   343 | `	VmInstr *pInstr;` |
|         - |   344 | `	sxi32 rc;` |
|         - |   345 | `	sxu32 n;` |
|         - |   346 | `	/* Point to the goto table */` |
|   2817945 |   347 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|         - |   348 | `	/* Fix */` |
|   2818091 |   349 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|        10 |   387 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"'goto' to undefined label '%z'",&pJump->sLabel);` |
|        10 |   388 | `			if( rc == SXERR_ABORT ){` |
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
|   2817943 |   400 | `	return SXRET_OK;` |
|   1408975 |   401 | `}` |
|         - |   402 | `/*` |
|         - |   403 | ` * Check if a given token value is installed in the literal table.` |
|         - |   404 | ` */` |
|  14702342 |   405 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|         5 |   406 | `{` |
|         - |   407 | `	SyHashEntry *pEntry;` |
|  14702347 |   408 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  14702347 |   409 | `	if( pEntry == 0 ){` |
|   3838517 |   410 | `		return SXERR_NOTFOUND;` |
|         - |   411 | `	}` |
|  10863835 |   412 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  10863835 |   413 | `	return SXRET_OK;` |
|   7351176 |   414 | `}` |
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
|   3838512 |   425 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|         5 |   426 | `{` |
|   3838517 |   427 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|   3838517 |   428 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   1919256 |   429 | `	}` |
|   3838517 |   430 | `	return SXRET_OK;` |
|         5 |   431 | `}` |
|         - |   432 | `/*` |
|         - |   433 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|         - |   434 | ` * in the constant table.` |
|         - |   435 | ` */` |
|   3551214 |   436 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|         5 |   437 | `{` |
|         - |   438 | `	ph7_value *pObj;` |
|   3551219 |   439 | `	sxu32 nIdx = 0; /* cc warning */` |
|         - |   440 | `	/* Reserve a new constant */` |
|   3551219 |   441 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   3551219 |   442 | `	if( pObj == 0 ){` |
|       ! 0 |   443 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   444 | `		return 0;` |
|         - |   445 | `	}` |
|   3551219 |   446 | `	*pIdx = nIdx;` |
|         - |   447 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|         - |   448 | `	 * the constant string iterals table [optimization purposes].` |
|         - |   449 | `	 */` |
|   3551219 |   450 | `	return pObj;` |
|   1775612 |   451 | `}` |
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
|   6947822 |   466 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|         5 |   467 | `{` |
|         - |   468 | `	VmCallArgMap *pMap;` |
|   6947827 |   469 | `	if( !pGen->bStrictTypes ) return p3;` |
|        39 |   470 | `	if( p3 == 0 ){` |
|        35 |   471 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|        35 |   472 | `		if( pMap == 0 ) return 0;` |
|        35 |   473 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|        35 |   474 | `		p3 = (void *)pMap;` |
|        16 |   475 | `	}` |
|        39 |   476 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|        39 |   477 | `	return p3;` |
|   3473916 |   478 | `}` |
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
|   3559880 |   542 | `static int GenStateFindBadNumericSeparator(` |
|         - |   543 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|         5 |   544 | `{` |
|   3559885 |   545 | `	const char *z = pRaw->zString;` |
|   3559885 |   546 | `	sxu32 n = pRaw->nByte;` |
|   3559885 |   547 | `	int base = 10;` |
|         - |   548 | `	sxu32 i, start;` |
|   3559885 |   549 | `	if( n < 2 ) return 0;` |
|    743837 |   550 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|    103237 |   551 | `		base = 16;` |
|    692221 |   552 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|       286 |   553 | `		base = 2;` |
|       142 |   554 | `	}` |
|   2803997 |   555 | `	for( i = 0; i < n; ++i ){` |
|   2060179 |   556 | `		if( z[i] != '_' ) continue;` |
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
|    743823 |   573 | `	return 0;` |
|   1779945 |   574 | `}` |
|         - |   575 | `/*` |
|         - |   576 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|         - |   577 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|         - |   578 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|         - |   579 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|         - |   580 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|         - |   581 | ` * so callers can bail from the current construct).` |
|         - |   582 | ` */` |
|   3559880 |   583 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|         5 |   584 | `{` |
|   3559885 |   585 | `	const char *zBad = 0;` |
|   3559885 |   586 | `	sxu32 nBad = 0;` |
|         - |   587 | `	SyString sBad;` |
|         - |   588 | `	sxi32 rc;` |
|   3559885 |   589 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|   3559871 |   590 | `		return SXRET_OK;` |
|         - |   591 | `	}` |
|        18 |   592 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|        18 |   593 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|         - |   594 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|        18 |   595 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |   596 | `		return SXERR_ABORT;` |
|         - |   597 | `	}` |
|        18 |   598 | `	return SXERR_SYNTAX;` |
|   1779945 |   599 | `}` |
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
|   3559866 |   616 | `static sxi32 GenStateStripNumericSeparators(` |
|         - |   617 | `	SyMemBackend *pAlloc,` |
|         - |   618 | `	const SyString *pToken,` |
|         - |   619 | `	char *zScratch, sxu32 nScratch,` |
|         - |   620 | `	SyString *pOut, char **pzAlloc)` |
|         5 |   621 | `{` |
|         - |   622 | `	sxu32 i, j;` |
|   3559871 |   623 | `	int hasUnderscore = 0;` |
|         - |   624 | `	char *zBuf;` |
|   3559871 |   625 | `	*pzAlloc = 0;` |
|   8434003 |   626 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|   4874391 |   627 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   2437071 |   628 | `	}` |
|   3559871 |   629 | `	if( !hasUnderscore ){` |
|   3559617 |   630 | `		SyStringDupPtr(pOut, pToken);` |
|   3559617 |   631 | `		return SXRET_OK;` |
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
|   1779938 |   648 | `}` |
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
|   3551248 |   684 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|         5 |   685 | `{` |
|   3551253 |   686 | `	const char *z = pNum->zString;` |
|   3551253 |   687 | `	const char *zEnd = z + pNum->nByte;` |
|         - |   688 | `	const char *p, *q;` |
|         - |   689 | `	int n;` |
|   3551253 |   690 | `	*pbDecimal = FALSE;` |
|   3551253 |   691 | `	if( z >= zEnd ){` |
|       ! 0 |   692 | `		return FALSE;` |
|         - |   693 | `	}` |
|   3551253 |   694 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|         - |   695 | `		/* Hexadecimal: INT64_MAX == 0x7FFF...F (16 digits, leading nibble 7). */` |
|    103235 |   696 | `		p = z + 2;` |
|    129983 |   697 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|    420769 |   698 | `		for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){ n++; }` |
|    103235 |   699 | `		if( n < 16 \|\| (n == 16 && SyHexToint(p[0]) < 8) ){` |
|    103229 |   700 | `			return FALSE;` |
|         - |   701 | `		}` |
|         7 |   702 | `		{ ph7_real dv = 0;` |
|       103 |   703 | `		  for( q = p; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){` |
|        97 |   704 | `			dv = dv * 16 + (ph7_real)SyHexToint(q[0]);` |
|        49 |   705 | `		  }` |
|         7 |   706 | `		  *pReal = dv;` |
|         - |   707 | `		}` |
|         7 |   708 | `		return TRUE;` |
|   3448023 |   709 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
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
|   3447741 |   724 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'o' \|\| z[1] == 'O') ){` |
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
|   3447725 |   739 | `	}else if( z[0] == '0' ){` |
|         - |   740 | `		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the` |
|         - |   741 | `		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1` |
|         - |   742 | `		 * "0o" marker ends the run and leaves it to the int path (as today). */` |
|   1278269 |   743 | `		p = z;` |
|   2556535 |   744 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|   1289985 |   745 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|   1278269 |   746 | `		if( n <= 21 ){` |
|   1278267 |   747 | `			return FALSE;` |
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
|   2169461 |   760 | `	p = z;` |
|   2169461 |   761 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
|   5174013 |   762 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|   2169461 |   763 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|        25 |   764 | `		*pbDecimal = TRUE;` |
|        25 |   765 | `		return TRUE;` |
|         - |   766 | `	}` |
|   2169437 |   767 | `	return FALSE;` |
|   1775629 |   768 | `}` |
|   3559852 |   769 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |   770 | `{` |
|   3559857 |   771 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   3559857 |   772 | `	sxu32 nIdx = 0;` |
|         - |   773 | `	char zScratch[GEN_NUM_SCRATCH];` |
|   3559857 |   774 | `	char *zAlloc = 0;` |
|         - |   775 | `	SyString sNum;` |
|         - |   776 | `	sxi32 rc;` |
|   1779926 |   777 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|   3559857 |   778 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|   3559857 |   779 | `	if( rc != SXRET_OK ){` |
|        14 |   780 | `		return rc;` |
|         - |   781 | `	}` |
|   5339768 |   782 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   1779921 |   783 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|   3559847 |   784 | `	if( rc != SXRET_OK ){` |
|       ! 0 |   785 | `		return SXERR_ABORT;` |
|         - |   786 | `	}` |
|   3559847 |   787 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|         - |   788 | `		ph7_value *pObj;` |
|         - |   789 | `		sxi64 iValue;` |
|   3551253 |   790 | `		ph7_real rOverflow = 0;` |
|   3551253 |   791 | `		int bDecimalOverflow = 0;` |
|   3551253 |   792 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
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
|   3551219 |   809 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|   3551219 |   810 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   3551219 |   811 | `			if( pObj == 0 ){` |
|       ! 0 |   812 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       ! 0 |   813 | `				return SXERR_ABORT;` |
|         - |   814 | `			}` |
|   3551219 |   815 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|         - |   816 | `		}` |
|   1775629 |   817 | `	}else{` |
|         - |   818 | `		/* Real number */` |
|         - |   819 | `		ph7_value *pObj;` |
|         - |   820 | `		/* Reserve a new constant */` |
|      8599 |   821 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      8599 |   822 | `		if( pObj == 0 ){` |
|       ! 0 |   823 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   824 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       ! 0 |   825 | `			return SXERR_ABORT;` |
|         - |   826 | `		}` |
|      8599 |   827 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|      8599 |   828 | `		PH7_MemObjToReal(pObj);` |
|         - |   829 | `	}` |
|   3559847 |   830 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         - |   831 | `	/* Emit the load constant instruction */` |
|   3559847 |   832 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |   833 | `	/* Node successfully compiled */` |
|   3559847 |   834 | `	return SXRET_OK;` |
|   1779931 |   835 | `}` |
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
|   5097350 |   847 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |   848 | `{` |
|   5097355 |   849 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|         - |   850 | `	const char *zIn,*zCur,*zEnd;` |
|         - |   851 | `	ph7_value *pObj;` |
|         - |   852 | `	sxu32 nIdx;` |
|   5097355 |   853 | `	nIdx = 0; /* Prevent compiler warning */` |
|         - |   854 | `	/* Delimit the string */` |
|   5097355 |   855 | `	zIn  = pStr->zString;` |
|   5097355 |   856 | `	zEnd = &zIn[pStr->nByte];` |
|   5097355 |   857 | `	if( zIn >= zEnd ){` |
|         - |   858 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|         - |   859 | `		 * rather than reserving a new object each time. */` |
|    332559 |   860 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    332559 |   861 | `		return SXRET_OK;` |
|         - |   862 | `	}` |
|   4764801 |   863 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|         - |   864 | `		/* Already processed,emit the load constant instruction` |
|         - |   865 | `		 * and return.` |
|         - |   866 | `		 */` |
|   2823777 |   867 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   2823777 |   868 | `		return SXRET_OK;` |
|         - |   869 | `	}` |
|         - |   870 | `	/* Reserve a new constant */` |
|   1941029 |   871 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   1941029 |   872 | `	if( pObj == 0 ){` |
|       ! 0 |   873 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   874 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |   875 | `		return SXERR_ABORT;` |
|         - |   876 | `	}` |
|   1941029 |   877 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|         - |   878 | `	/* Compile the node */` |
|   1988843 |   879 | `	for(;;){` |
|   3977691 |   880 | `		if( zIn >= zEnd ){` |
|         - |   881 | `			/* End of input */` |
|   1941029 |   882 | `			break;` |
|         - |   883 | `		}` |
|   2036667 |   884 | `		zCur = zIn;` |
|  40437017 |   885 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  38400355 |   886 | `			zIn++;` |
|         5 |   887 | `		}` |
|   2036667 |   888 | `		if( zIn > zCur ){` |
|         - |   889 | `			/* Append raw contents*/` |
|   1998423 |   890 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    999209 |   891 | `		}` |
|   2036667 |   892 | `		zIn++;` |
|   2036667 |   893 | `		if( zIn < zEnd ){` |
|    130051 |   894 | `			if( zIn[0] == '\\' ){` |
|         - |   895 | `				/* A literal backslash */` |
|     30605 |   896 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|    114751 |   897 | `			}else if( zIn[0] == '\'' ){` |
|         - |   898 | `				/* A single quote */` |
|        11 |   899 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|         6 |   900 | `			}else{` |
|         - |   901 | `				/* verbatim copy */` |
|     99441 |   902 | `				zIn--;` |
|     99441 |   903 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|     99441 |   904 | `				zIn++;` |
|         - |   905 | `			}` |
|     65023 |   906 | `		}` |
|         - |   907 | `		/* Advance the stream cursor */` |
|   2036667 |   908 | `		zIn++;` |
|         5 |   909 | `	}` |
|         - |   910 | `	/* Emit the load constant instruction */` |
|   1941029 |   911 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   1941029 |   912 | `	if( pStr->nByte < 1024 ){` |
|         - |   913 | `		/* Install in the literal table */` |
|   1941029 |   914 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|    970512 |   915 | `	}` |
|         - |   916 | `	/* Node successfully compiled */` |
|   1941029 |   917 | `	return SXRET_OK;` |
|   2548680 |   918 | `}` |
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
|      2634 |  1085 | `static sxi32 GenStateProcessStringExpression(` |
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
|      2639 |  1096 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         - |  1097 | `	/* Preallocate some slots */` |
|      2639 |  1098 | `	SySetAlloc(&sToken,0x08);` |
|         - |  1099 | `	/* Tokenize the text */` |
|      2639 |  1100 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken,0);` |
|         - |  1101 | `	/* Swap delimiter */` |
|      2639 |  1102 | `	pTmpIn  = pGen->pIn;` |
|      2639 |  1103 | `	pTmpEnd = pGen->pEnd;` |
|      2639 |  1104 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      2639 |  1105 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|         - |  1106 | `	/* Compile the expression */` |
|      2639 |  1107 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  1108 | `	/* Restore token stream */` |
|      2639 |  1109 | `	pGen->pIn  = pTmpIn;` |
|      2639 |  1110 | `	pGen->pEnd = pTmpEnd;` |
|         - |  1111 | `	/* Release the token set */` |
|      2639 |  1112 | `	SySetRelease(&sToken);` |
|         - |  1113 | `	/* Compilation result */` |
|      2639 |  1114 | `	return rc;` |
|         5 |  1115 | `}` |
|         - |  1116 | `/*` |
|         - |  1117 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|         - |  1118 | ` */` |
|    121788 |  1119 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|         5 |  1120 | `{` |
|         - |  1121 | `	ph7_value *pConstObj;` |
|    121793 |  1122 | `	sxu32 nIdx = 0;` |
|         - |  1123 | `	/* Reserve a new constant */` |
|    121793 |  1124 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    121793 |  1125 | `	if( pConstObj == 0 ){` |
|       ! 0 |  1126 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  1127 | `		return 0;` |
|         - |  1128 | `	}` |
|    121793 |  1129 | `	(*pCount)++;` |
|    121793 |  1130 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|         - |  1131 | `	/* Emit the load constant instruction */` |
|    121793 |  1132 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    121793 |  1133 | `	return pConstObj;` |
|     60899 |  1134 | `}` |
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
|    120224 |  1197 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|         5 |  1198 | `{` |
|    120229 |  1199 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|         - |  1200 | `	const char *zIn,*zCur,*zEnd;` |
|    120229 |  1201 | `	ph7_value *pObj = 0;` |
|         - |  1202 | `	sxi32 iCons;` |
|         - |  1203 | `	sxi32 rc;` |
|         - |  1204 | `	/* Delimit the string */` |
|    120229 |  1205 | `	zIn  = pStr->zString;` |
|    120229 |  1206 | `	zEnd = &zIn[pStr->nByte];` |
|    120229 |  1207 | `	if( zIn >= zEnd ){` |
|         - |  1208 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|         - |  1209 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|         - |  1210 | `		 * literal table from growing when many "" literals appear in the source.` |
|         - |  1211 | `		 */` |
|       413 |  1212 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|       413 |  1213 | `		return SXRET_OK;` |
|         - |  1214 | `	}` |
|    119821 |  1215 | `	zCur = 0;` |
|         - |  1216 | `	/* Compile the node */` |
|    119821 |  1217 | `	iCons = 0;` |
|     61223 |  1218 | `	for(;;){` |
|    162935 |  1219 | `		zCur = zIn;` |
|   1659223 |  1220 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|   1498927 |  1221 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|        69 |  1222 | `				break;` |
|   1498800 |  1223 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|      2512 |  1224 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|      1256 |  1225 | `					break;` |
|         - |  1226 | `			}` |
|   1496293 |  1227 | `			zIn++;` |
|         5 |  1228 | `		}` |
|    162935 |  1229 | `		if( zIn > zCur ){` |
|     95065 |  1230 | `			if( pObj == 0 ){` |
|     94443 |  1231 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|     94443 |  1232 | `				if( pObj == 0 ){` |
|       ! 0 |  1233 | `					return SXERR_ABORT;` |
|         - |  1234 | `				}` |
|     47219 |  1235 | `			}` |
|     95065 |  1236 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|     47530 |  1237 | `		}` |
|    162935 |  1238 | `		if( zIn >= zEnd ){` |
|    119819 |  1239 | `			break;` |
|         - |  1240 | `		}` |
|     43121 |  1241 | `		if( zIn[0] == '\\' ){` |
|     40487 |  1242 | `			const char *zPtr = 0;` |
|         - |  1243 | `			sxu32 n;` |
|     40487 |  1244 | `			zIn++;` |
|     40487 |  1245 | `			if( pObj == 0 ){` |
|     27355 |  1246 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|     27355 |  1247 | `				if( pObj == 0 ){` |
|       ! 0 |  1248 | `					return SXERR_ABORT;` |
|         - |  1249 | `				}` |
|     13675 |  1250 | `			}` |
|     40487 |  1251 | `			if( zIn >= zEnd ){` |
|         - |  1252 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|         3 |  1253 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|         3 |  1254 | `				break;` |
|         - |  1255 | `			}` |
|     40485 |  1256 | `			n = sizeof(char); /* size of conversion */` |
|     40485 |  1257 | `			switch( zIn[0] ){` |
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
|     17653 |  1274 | `			case 'n':` |
|         - |  1275 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|     35311 |  1276 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|     35311 |  1277 | `				break;` |
|        27 |  1278 | `			case 'r':` |
|         - |  1279 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|        59 |  1280 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|        59 |  1281 | `				break;` |
|      1941 |  1282 | `			case 't':` |
|         - |  1283 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      3887 |  1284 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      3887 |  1285 | `				break;` |
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
|     40485 |  1400 | `			zIn += n;` |
|     40485 |  1401 | `			continue;` |
|         - |  1402 | `		}` |
|      2639 |  1403 | `		if( zIn[0] == '{' ){` |
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
|      2507 |  1437 | `			const char *zExpr = zIn;` |
|         - |  1438 | `			/* Assemble variable name */` |
|      1276 |  1439 | `			for(;;){` |
|         - |  1440 | `				/* Jump leading dollars */` |
|      5059 |  1441 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|      2507 |  1442 | `					zIn++;` |
|         5 |  1443 | `				}` |
|      1276 |  1444 | `				for(;;){` |
|     13077 |  1445 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|      9249 |  1446 | `						zIn++;` |
|         5 |  1447 | `					}` |
|      2557 |  1448 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|         - |  1449 | `						/* UTF-8 stream */` |
|       ! 0 |  1450 | `						zIn++;` |
|       ! 0 |  1451 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|       ! 0 |  1452 | `							zIn++;` |
|       ! 0 |  1453 | `						}` |
|       ! 0 |  1454 | `						continue;` |
|         - |  1455 | `					}` |
|      2557 |  1456 | `					break;` |
|       ! 0 |  1457 | `				}` |
|      2557 |  1458 | `				if( zIn >= zEnd ){` |
|       269 |  1459 | `					break;` |
|         - |  1460 | `				}` |
|      2293 |  1461 | `				if( zIn[0] == '[' ){` |
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
|      2283 |  1479 | `				}else if(zIn[0] == '{' ){` |
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
|      2279 |  1497 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|         - |  1498 | `					/* Member access operator '->' */` |
|        53 |  1499 | `					zIn += 2;` |
|      2254 |  1500 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|         - |  1501 | `					/* Static member access operator '::' */` |
|       ! 0 |  1502 | `					zIn += 2;` |
|       ! 0 |  1503 | `				}else{` |
|      1117 |  1504 | `					break;` |
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
|      2507 |  1516 | `				const char *zBr = zExpr;` |
|     14371 |  1517 | `				while( zBr < zIn && zBr[0] != '[' ){` |
|     11869 |  1518 | `					zBr++;` |
|         5 |  1519 | `				}` |
|      2507 |  1520 | `				if( zBr < zIn && zIn[-1] == ']' ){` |
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
|      2500 |  1561 | `			if( &zExpr[1] < zIn && zExpr[0] == '$' && zExpr[1] == '{' && zIn[-1] == '}'` |
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
|      2503 |  1597 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|      2503 |  1598 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1599 | `				return SXERR_ABORT;` |
|         - |  1600 | `			}` |
|      2503 |  1601 | `			if( rc != SXERR_EMPTY ){` |
|      2501 |  1602 | `				++iCons;` |
|      1248 |  1603 | `			}` |
|         - |  1604 | `		}` |
|         - |  1605 | `		/* Invalidate the previously used constant */` |
|      2635 |  1606 | `		pObj = 0;` |
|         5 |  1607 | `	}/*for(;;)*/` |
|    119821 |  1608 | `	if( iCons > 1 ){` |
|         - |  1609 | `		/* Concatenate all compiled constants */` |
|      1907 |  1610 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|       951 |  1611 | `	}` |
|         - |  1612 | `	/* Node successfully compiled */` |
|    119821 |  1613 | `	return SXRET_OK;` |
|     60117 |  1614 | `}` |
|         - |  1615 | `/*` |
|         - |  1616 | ` * Compile a double quoted string.` |
|         - |  1617 | ` *  See the block-comment above for more information.` |
|         - |  1618 | ` */` |
|    120162 |  1619 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1620 | `{` |
|         - |  1621 | `	sxi32 rc;` |
|    120167 |  1622 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|     60081 |  1623 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  1624 | `	/* Compilation result */` |
|    120167 |  1625 | `	return rc;` |
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
|   1437938 |  1669 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   1437943 |  1680 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|         - |  1681 | `	/* Compile the expression*/` |
|   1437943 |  1682 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|         - |  1683 | `	/* Restore token stream */` |
|   1437943 |  1684 | `	RE_SWAP_DELIMITER(pGen);` |
|   1437943 |  1685 | `	return rc;` |
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
|   1379366 |  1724 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|         5 |  1725 | `{` |
|   1379371 |  1726 | `	SyToken *pCur = pStart;` |
|   1379371 |  1727 | `	sxi32 iNest = 0;` |
|   3560977 |  1728 | `	while( pCur < pEnd ){` |
|   2684439 |  1729 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    502829 |  1730 | `			return pCur;` |
|         - |  1731 | `		}` |
|         - |  1732 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|         - |  1733 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|         - |  1734 | `		 * not an entry separator. Skip past the signature.` |
|         - |  1735 | `		 */` |
|   2181615 |  1736 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|     23031 |  1737 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|     23031 |  1738 | `			SyToken *pFn = pCur;` |
|     23026 |  1739 | `			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd` |
|       ! 0 |  1740 | `				&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|         5 |  1741 | `				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|       ! 0 |  1742 | `				pFn = &pCur[1];` |
|       ! 0 |  1743 | `				nKw = PH7_TKWRD_FN;` |
|       ! 0 |  1744 | `			}` |
|     23031 |  1745 | `			if( nKw == PH7_TKWRD_FN ){` |
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
|     23027 |  1776 | `			if( nKw == PH7_TKWRD_MATCH ){` |
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
|     11510 |  1796 | `		}` |
|   2181609 |  1797 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     54049 |  1798 | `			iNest++;` |
|   2154587 |  1799 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|         - |  1800 | `			/* Don't worry about mismatched brackets here, the expression` |
|         - |  1801 | `			 * parser will shortly detect any syntax error. */` |
|     54049 |  1802 | `			iNest--;` |
|     27022 |  1803 | `		}` |
|   2181609 |  1804 | `		pCur++;` |
|         5 |  1805 | `	}` |
|    876543 |  1806 | `	return pEnd;` |
|    689688 |  1807 | `}` |
|         - |  1808 | `/*` |
|         - |  1809 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|         - |  1810 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|         - |  1811 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|         - |  1812 | ` */` |
|    615298 |  1813 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|         5 |  1814 | `{` |
|         - |  1815 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|         - |  1816 | `	SyToken *pKey,*pCur;` |
|    615303 |  1817 | `	sxi32 iEmitRef = 0;` |
|    615303 |  1818 | `	sxi32 iSpread = 0;` |
|    615303 |  1819 | `	sxi32 nPair = 0;` |
|         - |  1820 | `	sxi32 rc;` |
|    615303 |  1821 | `	xValidator = 0;` |
|    842099 |  1822 | `	for(;;){` |
|         - |  1823 | `		/* Jump leading commas. Exactly ONE separates two entries; a second one (or a comma` |
|         - |  1824 | `		 * at the very start) means an EMPTY element, which php rejects outright — PH7 just` |
|         - |  1825 | ``		 * skipped them, so `array(,)` and `array(1,,2)` compiled silently. A TRAILING comma`` |
|         - |  1826 | `		 * is legal and is handled by the loop exiting on the next pass. */` |
|    534450 |  1827 | `		{` |
|   1684203 |  1828 | `			int nSkip = 0;` |
|   2505241 |  1829 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    821043 |  1830 | `				nSkip++;` |
|    821043 |  1831 | `				pGen->pIn++;` |
|         5 |  1832 | `			}` |
|   1684203 |  1833 | `			if( nSkip > 1 \|\| (nSkip > 0 && nPair < 1) ){` |
|       ! 0 |  1834 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn[-1].nLine,` |
|         - |  1835 | `					"Cannot use empty array elements in arrays");` |
|       ! 0 |  1836 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  1837 | `					return SXERR_ABORT;` |
|         - |  1838 | `				}` |
|       ! 0 |  1839 | `				return SXRET_OK;` |
|         - |  1840 | `			}` |
|         - |  1841 | `		}` |
|   1684203 |  1842 | `		pCur = pGen->pIn;` |
|   1684203 |  1843 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|         - |  1844 | `			/* No more entry to process */` |
|    615285 |  1845 | `			break;` |
|         - |  1846 | `		}` |
|   1068923 |  1847 | `		if( pCur >= pGen->pIn ){` |
|       ! 0 |  1848 | `			continue;` |
|         - |  1849 | `		}` |
|         - |  1850 | `		/* Compile the key if available */` |
|   1068923 |  1851 | `		pKey = pCur;` |
|   1068923 |  1852 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   1068923 |  1853 | `		rc = SXERR_EMPTY;` |
|   1068923 |  1854 | `		if( pCur < pGen->pIn ){` |
|    368767 |  1855 | `			if( pKey == pCur ){` |
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
|    368765 |  1869 | `			if( &pCur[1] >= pGen->pIn ){` |
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
|    368755 |  1880 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|         - |  1881 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    368755 |  1882 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1883 | `				return SXERR_ABORT;` |
|         - |  1884 | `			}` |
|    368755 |  1885 | `			pCur++; /* Jump the '=>' operator */` |
|    184380 |  1886 | `		}else{` |
|         - |  1887 | `			/* Reset back the cursor and point to the entry value */` |
|    700161 |  1888 | `			pCur = pKey;` |
|         - |  1889 | `		}` |
|   1068911 |  1890 | `		if( rc == SXERR_EMPTY ){` |
|         - |  1891 | `			/* No key given: load the nil, TAGGED so LOAD_MAP knows this is an absent key` |
|         - |  1892 | ``			 * (auto-index) rather than an explicit `null =>` one, which php deprecates. */`` |
|    700161 |  1893 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,PH7_LOADC_NOKEY,0 /* nil index */,0,0);` |
|    350078 |  1894 | `		}` |
|   1068911 |  1895 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|   1068909 |  1914 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   1068909 |  1915 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
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
|   1603355 |  1932 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,` |
|    534450 |  1933 | `			iEmitRef ? EXPR_FLAG_LOAD_IDX_STORE` |
|         - |  1934 | `			         : EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,` |
|    534450 |  1935 | `			xValidator);` |
|   1068905 |  1936 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  1937 | `			return SXERR_ABORT;` |
|         - |  1938 | `		}` |
|   1068905 |  1939 | `		if( iSpread ){` |
|         - |  1940 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|        73 |  1941 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   1068870 |  1942 | `		}else if( iEmitRef ){` |
|         - |  1943 | `			/* Emit the load reference instruction */` |
|        40 |  1944 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|        18 |  1945 | `		}` |
|   1068905 |  1946 | `		xValidator = 0;` |
|   1068905 |  1947 | `		iEmitRef = 0;` |
|   1068905 |  1948 | `		iSpread = 0;` |
|   1068905 |  1949 | `		nPair++;` |
|         5 |  1950 | `	}` |
|         - |  1951 | `	/* Emit the load map instruction */` |
|    615285 |  1952 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|         - |  1953 | `	/* Node successfully compiled */` |
|    615285 |  1954 | `	return SXRET_OK;` |
|    307654 |  1955 | `}` |
|         - |  1956 | `/*` |
|         - |  1957 | ` * Compile the 'array' language construct.` |
|         - |  1958 | ` *	 According to the PHP language reference manual` |
|         - |  1959 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|         - |  1960 | ` *   values to keys. This type is optimized for several different uses; it can` |
|         - |  1961 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|         - |  1962 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|         - |  1963 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|         - |  1964 | ` */` |
|    399134 |  1965 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1966 | `{` |
|         - |  1967 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|    399139 |  1968 | `	pGen->pIn += 2;` |
|    399139 |  1969 | `	pGen->pEnd--;` |
|    199567 |  1970 | `	SXUNUSED(iCompileFlag);` |
|    399139 |  1971 | `	return GenStateCompileArrayBody(pGen);` |
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
|    216164 |  2070 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2071 | `{` |
|         - |  2072 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|    216169 |  2073 | `	pGen->pIn++;` |
|    216169 |  2074 | `	pGen->pEnd--;` |
|    108082 |  2075 | `	SXUNUSED(iCompileFlag);` |
|    216169 |  2076 | `	return GenStateCompileArrayBody(pGen);` |
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
|         2 |  2483 | `{` |
|         - |  2484 | `	ph7_vm_func_closure_env sEnv;` |
|         - |  2485 | `	ph7_vm_func_closure_env *aEnv;` |
|         - |  2486 | `	sxu32 n, nEnv;` |
|         - |  2487 | `	char *zDup;` |
|       220 |  2488 | `	if( nByte == 0 ){` |
|       ! 0 |  2489 | `		return SXRET_OK;` |
|         - |  2490 | `	}` |
|       218 |  2491 | `	if( nByte == sizeof("this")-1` |
|       117 |  2492 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|         3 |  2493 | `		return SXRET_OK;` |
|         - |  2494 | `	}` |
|       272 |  2495 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|       204 |  2496 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|       197 |  2497 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|       152 |  2498 | `			return SXRET_OK;` |
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
|       111 |  2520 | `}` |
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
|         3 |  2534 | `{` |
|         - |  2535 | `	sxi32 rc;` |
|       579 |  2536 | `	while( zIn < zEnd ){` |
|       473 |  2537 | `		if( zIn[0] == '\\' ){` |
|        14 |  2538 | `			zIn++;` |
|        14 |  2539 | `			if( zIn < zEnd ){` |
|        14 |  2540 | `				zIn++;` |
|         6 |  2541 | `			}` |
|        14 |  2542 | `			continue;` |
|         - |  2543 | `		}` |
|       458 |  2544 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
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
|       437 |  2574 | `		zIn++;` |
|         3 |  2575 | `	}` |
|       109 |  2576 | `	return SXRET_OK;` |
|        56 |  2577 | `}` |
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
|       162 |  2601 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|        53 |  2602 | `				pScan->sData.zString,` |
|       106 |  2603 | `				pScan->sData.zString + pScan->sData.nByte,` |
|        53 |  2604 | `				aShadow,nShadow);` |
|       109 |  2605 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2606 | `				return SXERR_ABORT;` |
|         - |  2607 | `			}` |
|       109 |  2608 | `			pScan++;` |
|       109 |  2609 | `			continue;` |
|         - |  2610 | `		}` |
|      2740 |  2611 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|        36 |  2612 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|        36 |  2613 | `			SyToken *pFnKw = pScan;` |
|        34 |  2614 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|       ! 0 |  2615 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|         2 |  2616 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|       ! 0 |  2617 | `				pFnKw = &pScan[1];` |
|       ! 0 |  2618 | `				nKw = PH7_TKWRD_FN;` |
|       ! 0 |  2619 | `			}` |
|        36 |  2620 | `			if( nKw == PH7_TKWRD_FN ){` |
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
|       196 |  2769 | `			SyToken *pDollar = pScan;` |
|       291 |  2770 | `			while( &pDollar[1] < pEnd` |
|       196 |  2771 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|       ! 0 |  2772 | `				pDollar++;` |
|       ! 0 |  2773 | `			}` |
|       196 |  2774 | `			if( &pDollar[1] >= pEnd ){` |
|       ! 0 |  2775 | `				break;` |
|         - |  2776 | `			}` |
|       196 |  2777 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  2778 | `				pScan = pDollar + 1;` |
|       ! 0 |  2779 | `				continue;` |
|         - |  2780 | `			}` |
|       293 |  2781 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|       194 |  2782 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|        97 |  2783 | `				aShadow,nShadow);` |
|       196 |  2784 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2785 | `				return SXERR_ABORT;` |
|         - |  2786 | `			}` |
|       196 |  2787 | `			pScan = pDollar + 2;` |
|         - |  2788 | `		}` |
|         2 |  2789 | `	}` |
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
|       470 |  2866 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|       470 |  2867 | `	if( pFunc == 0 ){` |
|       ! 0 |  2868 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2869 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2870 | `		return SXERR_ABORT;` |
|         - |  2871 | `	}` |
|         - |  2872 | `	/* Generate a unique lambda name */` |
|       470 |  2873 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       470 |  2874 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 |  2875 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       ! 0 |  2876 | `	}` |
|       470 |  2877 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|       470 |  2878 | `	if( zDup == 0 ){` |
|       ! 0 |  2879 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2880 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2881 | `		return SXERR_ABORT;` |
|         - |  2882 | `	}` |
|       470 |  2883 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|         - |  2884 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|       470 |  2885 | `	pFunc->nLine = nLine;` |
|         - |  2886 | ``	/* Expression-position attributes (`$f = #[A] fn () => …`) */`` |
|       470 |  2887 | `	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  2888 | `		return SXERR_ABORT;` |
|         - |  2889 | `	}` |
|         - |  2890 | `	/* Collect function arguments */` |
|       470 |  2891 | `	if( pGen->pIn < pSigEnd ){` |
|       125 |  2892 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|       125 |  2893 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2894 | `			return SXERR_ABORT;` |
|         - |  2895 | `		}` |
|        61 |  2896 | `	}` |
|         - |  2897 | `	/* Point past ')' and parse optional return type */` |
|       470 |  2898 | `	pGen->pIn = &pSigEnd[1];` |
|       470 |  2899 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|       470 |  2900 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2901 | `		return SXERR_ABORT;` |
|       470 |  2902 | `	}else if( rc == SXERR_SYNTAX ){` |
|       ! 0 |  2903 | `		return SXERR_SYNTAX;` |
|         - |  2904 | `	}` |
|         - |  2905 | `	/* Expect '=>' */` |
|       470 |  2906 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
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
|       122 |  2929 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|       120 |  2930 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|       122 |  2931 | `			if( aShadow == 0 ){` |
|       ! 0 |  2932 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2933 | `					"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2934 | `				return SXERR_ABORT;` |
|         - |  2935 | `			}` |
|       278 |  2936 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|       158 |  2937 | `				aShadow[n] = aArgs[n].sName;` |
|        80 |  2938 | `			}` |
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
|  19314252 |  3376 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3377 | `{` |
|  19314257 |  3378 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  3379 | `	sxi32 iVv;` |
|         - |  3380 | `	sxi32 iP1;` |
|         - |  3381 | `	void *p3;` |
|         - |  3382 | `	sxi32 rc;` |
|  19314257 |  3383 | `	iVv = -1; /* Variable variable counter */` |
|  38628521 |  3384 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  19314269 |  3385 | `		pGen->pIn++;` |
|  19314269 |  3386 | `		iVv++;` |
|         5 |  3387 | `	}` |
|  19314257 |  3388 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|         - |  3389 | `		/* Invalid variable name */` |
|       ! 0 |  3390 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable or \"{\" or \"$\"");` |
|       ! 0 |  3391 | `		if( rc == SXERR_ABORT ){` |
|         - |  3392 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3393 | `			return SXERR_ABORT;` |
|         - |  3394 | `		}` |
|       ! 0 |  3395 | `		return SXRET_OK;` |
|         - |  3396 | `	}` |
|  19314257 |  3397 | `	p3  = 0;` |
|  19314257 |  3398 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
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
|  19314241 |  3424 | `		char *zName = 0;` |
|         - |  3425 | `		/* Extract variable name */` |
|  19314241 |  3426 | `		pName = &pGen->pIn->sData;` |
|         - |  3427 | `		/* Advance the stream cursor */` |
|  19314241 |  3428 | `		pGen->pIn++;` |
|  19314241 |  3429 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  19314241 |  3430 | `		if( pEntry == 0 ){` |
|         - |  3431 | `			/* Duplicate name */` |
|   1160277 |  3432 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   1160277 |  3433 | `			if( zName == 0 ){` |
|       ! 0 |  3434 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3435 | `				return SXERR_ABORT;` |
|         - |  3436 | `			}` |
|         - |  3437 | `			/* Install in the hashtable */` |
|   1160277 |  3438 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|    580141 |  3439 | `		}else{` |
|         - |  3440 | `			/* Name already available */` |
|  18153969 |  3441 | `			zName = (char *)pEntry->pUserData;` |
|         - |  3442 | `		}` |
|  19314241 |  3443 | `		p3 = (void *)zName;` |
|         - |  3444 | `	}` |
|  19314253 |  3445 | `	iP1 = 0;` |
|  19314253 |  3446 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|   5708825 |  3447 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|         - |  3448 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|   5704981 |  3449 | `			iP1 = 1;` |
|   2852488 |  3450 | `		}` |
|   2854410 |  3451 | `	}` |
|         - |  3452 | `	/* Emit the load instruction */` |
|  19314253 |  3453 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  19314265 |  3454 | `	while( iVv > 0 ){` |
|        13 |  3455 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|        13 |  3456 | `		iVv--;` |
|         1 |  3457 | `	}` |
|         - |  3458 | `	/* Node successfully compiled */` |
|  19314253 |  3459 | `	return SXRET_OK;` |
|   9657131 |  3460 | `}` |
|         - |  3461 | `/*` |
|         - |  3462 | ` * Load a literal.` |
|         - |  3463 | ` */` |
|  11938884 |  3464 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|         5 |  3465 | `{` |
|  11938889 |  3466 | `	SyToken *pToken = pGen->pIn;` |
|         - |  3467 | `	ph7_value *pObj;` |
|         - |  3468 | `	SyString *pStr;` |
|         - |  3469 | `	sxu32 nIdx;` |
|         - |  3470 | `	/* Extract token value */` |
|  11938889 |  3471 | `	pStr = &pToken->sData;` |
|         - |  3472 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  11938889 |  3473 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   2246479 |  3474 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|         - |  3475 | `			/* NULL constant are always indexed at 0 */` |
|    982709 |  3476 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|    982709 |  3477 | `			return SXRET_OK;` |
|   1263775 |  3478 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|         - |  3479 | `			/* TRUE constant are always indexed at 1 */` |
|    325913 |  3480 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|    325913 |  3481 | `			return SXRET_OK;` |
|         5 |  3482 | `		}` |
|  11156648 |  3483 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   1990604 |  3484 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|         - |  3485 | `			/* FALSE constant are always indexed at 2 */` |
|    715001 |  3486 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|    715001 |  3487 | `			return SXRET_OK;` |
|   9389045 |  3488 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|    823252 |  3489 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|         - |  3490 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|      3833 |  3491 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3833 |  3492 | `			if( pObj == 0 ){` |
|       ! 0 |  3493 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3494 | `				return SXERR_ABORT;` |
|         - |  3495 | `			}` |
|      3833 |  3496 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|         - |  3497 | `			/* Emit the load constant instruction */` |
|      3833 |  3498 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      3833 |  3499 | `			return SXRET_OK;` |
|   9383298 |  3500 | `	}else if( (pStr->nByte == sizeof("__FILE__") - 1 &&` |
|   1277774 |  3501 | `		SyMemcmp(pStr->zString,"__FILE__",sizeof("__FILE__")-1) == 0) \|\|` |
|   9428043 |  3502 | `		(pStr->nByte == sizeof("__DIR__") - 1 &&` |
|    916690 |  3503 | `		SyMemcmp(pStr->zString,"__DIR__",sizeof("__DIR__")-1) == 0) ){` |
|         - |  3504 | `			/* __FILE__ / __DIR__ are magic constants resolved at COMPILE time to the` |
|         - |  3505 | `			 * file being compiled (where the token is written), NOT the runtime` |
|         - |  3506 | `			 * execution file. A function defined in a.php reporting __FILE__ must say` |
|         - |  3507 | `			 * a.php even when called from b.php — php semantics, and what Composer's` |
|         - |  3508 | ``			 * autoloader (loadClassLoader's `require __DIR__ . '/ClassLoader.php'`)`` |
|         - |  3509 | `			 * relies on. The runtime-constant path returned the caller's file. */` |
|      3915 |  3510 | `			int bDir = (pStr->zString[2] == 'D'); /* __DIR__ vs __FILE__ */` |
|      3915 |  3511 | `			SyString *pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|      3915 |  3512 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3915 |  3513 | `			if( pObj == 0 ){` |
|       ! 0 |  3514 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3515 | `				return SXERR_ABORT;` |
|         - |  3516 | `			}` |
|      3915 |  3517 | `			if( pFile && pFile->nByte > 0 ){` |
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
|      3825 |  3530 | `				SyStringInitFromBuf(&sMem,":MEMORY:",sizeof(":MEMORY:")-1);` |
|      3825 |  3531 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sMem);` |
|         - |  3532 | `			}` |
|      3915 |  3533 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      3915 |  3534 | `			return SXRET_OK;` |
|   9086623 |  3535 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|    233884 |  3536 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
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
|   9117395 |  3552 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|    511047 |  3553 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|   9185255 |  3554 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|    431184 |  3555 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
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
|   9907527 |  3598 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|         - |  3599 | `		ph7_value *pLitObj;` |
|         - |  3600 | `		/* Unknown literal,install it in the literal table */` |
|   1889699 |  3601 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   1889699 |  3602 | `		if( pLitObj == 0 ){` |
|       ! 0 |  3603 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  3604 | `			return SXERR_ABORT;` |
|         - |  3605 | `		}` |
|   1889699 |  3606 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|   1889699 |  3607 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|    944847 |  3608 | `	}` |
|         - |  3609 | `	/* Emit the load constant instruction */` |
|   9907527 |  3610 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|   9907527 |  3611 | `	return SXRET_OK;` |
|   5969447 |  3612 | `}` |
|         - |  3613 | `/*` |
|         - |  3614 | ` * Resolve a namespace path or simply load a literal.` |
|         - |  3615 | ` * If the token stream contains namespace separators (backslashes),` |
|         - |  3616 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|         - |  3617 | ` * Otherwise, load the simple literal directly.` |
|         - |  3618 | ` */` |
|  11942788 |  3619 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|         5 |  3620 | `{` |
|         - |  3621 | `	sxi32 rc;` |
|  11942793 |  3622 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  3623 | `		return SXRET_OK;` |
|         - |  3624 | `	}` |
|         - |  3625 | `	/* Check if this is a multi-token namespace path */` |
|  11942793 |  3626 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|         - |  3627 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      3909 |  3628 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      3909 |  3629 | `		int isAbsolute = 0;` |
|      3909 |  3630 | `		SyBlobReset(pWorker);` |
|         - |  3631 | `		/* Check for leading backslash (absolute path) */` |
|      3909 |  3632 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      3893 |  3633 | `			isAbsolute = 1;` |
|      3893 |  3634 | `			pGen->pIn++; /* Skip leading backslash */` |
|      1944 |  3635 | `		}` |
|         - |  3636 | `		/* Collect the raw path (no prefix yet) so its FIRST segment can be resolved` |
|         - |  3637 | ``		 * against use-imports below — php resolves `A\B\C` by mapping the leading`` |
|         - |  3638 | ``		 * `A` through the imports (`use X\A;` makes it `X\A\B\C`), and only prepends`` |
|         - |  3639 | ``		 * the current namespace when `A` matches no import. Blindly prefixing the`` |
|         - |  3640 | ``		 * namespace here produced e.g. `Ns\Ev\Bus` for `use ...\Event as Ev; Ev\Bus`. */`` |
|         - |  3641 | `		{` |
|         - |  3642 | `			SyBlob sRaw;` |
|      3909 |  3643 | `			SyBlobInit(&sRaw,&pGen->pVm->sAllocator);` |
|      4061 |  3644 | `			while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|      4061 |  3645 | `				if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        81 |  3646 | `					SyBlobAppend(&sRaw,"\\",1);` |
|        43 |  3647 | `				}else{` |
|      3985 |  3648 | `					SyBlobAppend(&sRaw,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|         - |  3649 | `				}` |
|      4061 |  3650 | `				if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      3909 |  3651 | `					pGen->pIn++;` |
|      3909 |  3652 | `					break;` |
|         - |  3653 | `				}` |
|       157 |  3654 | `				pGen->pIn++;` |
|         5 |  3655 | `			}` |
|      3909 |  3656 | `			if( isAbsolute ){` |
|      3893 |  3657 | `				SyBlobAppend(pWorker,SyBlobData(&sRaw),SyBlobLength(&sRaw)); /* FQN as written */` |
|      1949 |  3658 | `			}else{` |
|        18 |  3659 | `				const char *zRaw = (const char *)SyBlobData(&sRaw);` |
|        18 |  3660 | `				sxu32 nRaw = SyBlobLength(&sRaw);` |
|        18 |  3661 | `				sxu32 nFirst = 0;` |
|         - |  3662 | `				SyHashEntry *pNsImp;` |
|        84 |  3663 | `				while( nFirst < nRaw && zRaw[nFirst] != '\\' ){ nFirst++; }` |
|        18 |  3664 | `				pNsImp = SyHashGet(&pGen->hUseImports,(const void *)zRaw,nFirst);` |
|        18 |  3665 | `				if( pNsImp ){` |
|         - |  3666 | `					/* Leading segment is an imported alias: substitute its FQN. */` |
|        15 |  3667 | `					const char *zFQN = (const char *)pNsImp->pUserData;` |
|        15 |  3668 | `					SyBlobAppend(pWorker,zFQN,SyStrlen(zFQN));` |
|        15 |  3669 | `					SyBlobAppend(pWorker,zRaw + nFirst,nRaw - nFirst);` |
|        10 |  3670 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         3 |  3671 | `					SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         3 |  3672 | `					SyBlobAppend(pWorker,"\\",1);` |
|         3 |  3673 | `					SyBlobAppend(pWorker,zRaw,nRaw);` |
|         2 |  3674 | `				}else{` |
|       ! 0 |  3675 | `					SyBlobAppend(pWorker,zRaw,nRaw); /* global scope, no import */` |
|         - |  3676 | `				}` |
|         - |  3677 | `			}` |
|      3909 |  3678 | `			SyBlobRelease(&sRaw);` |
|         - |  3679 | `		}` |
|      3909 |  3680 | `		if( SyBlobLength(pWorker) > 0 ){` |
|         - |  3681 | `			ph7_value *pObj;` |
|         - |  3682 | `			SyString sPath;` |
|         - |  3683 | `			sxu32 nIdx;` |
|      3909 |  3684 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|         - |  3685 | `			/* Install in the literal table */` |
|      3909 |  3686 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      3855 |  3687 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3855 |  3688 | `				if( pObj == 0 ){` |
|       ! 0 |  3689 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  3690 | `					return SXERR_ABORT;` |
|         - |  3691 | `				}` |
|      3855 |  3692 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      3855 |  3693 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1925 |  3694 | `			}` |
|         - |  3695 | `			/* Emit the load constant instruction.` |
|         - |  3696 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|         - |  3697 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|      5861 |  3698 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|      1952 |  3699 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|      1952 |  3700 | `				nIdx,0,0);` |
|      3909 |  3701 | `			return SXRET_OK;` |
|         - |  3702 | `		}` |
|       ! 0 |  3703 | `	}` |
|         - |  3704 | `	/* Single-token literal: load directly */` |
|  11938889 |  3705 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  11938889 |  3706 | `	return rc;` |
|   5971399 |  3707 | `}` |
|         - |  3708 | `/*` |
|         - |  3709 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|         - |  3710 | ` */` |
|         - |  3711 | `/*` |
|         - |  3712 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|         - |  3713 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|         - |  3714 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|         - |  3715 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|         - |  3716 | ` */` |
|       ! 0 |  3717 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       ! 0 |  3718 | `{` |
|       ! 0 |  3719 | `	SXUNUSED(iCompileFlag);` |
|       ! 0 |  3720 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|         - |  3721 | `		"Cannot use the first-class callable syntax '...' here");` |
|       ! 0 |  3722 | `	return SXERR_SYNTAX;` |
|       ! 0 |  3723 | `}` |
|  11942788 |  3724 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3725 | `{` |
|         - |  3726 | `	sxi32 rc;` |
|  11942793 |  3727 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  11942793 |  3728 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3729 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  3730 | `		return rc;` |
|         - |  3731 | `	}` |
|         - |  3732 | `	/* Node successfully compiled */` |
|  11942793 |  3733 | `	return SXRET_OK;` |
|   5971399 |  3734 | `}` |
|         - |  3735 | `/*` |
|         - |  3736 | ` * Recover from a compile-time error. In other words synchronize` |
|         - |  3737 | ` * the token stream cursor with the first semi-colon seen.` |
|         - |  3738 | ` */` |
|         8 |  3739 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|         1 |  3740 | `{` |
|         - |  3741 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|        17 |  3742 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|         9 |  3743 | `		pGen->pIn++;` |
|         1 |  3744 | `	}` |
|         9 |  3745 | `	return SXRET_OK;` |
|         1 |  3746 | `}` |
|         - |  3747 | `/*` |
|         - |  3748 | ` * Check if the given identifier name is reserved or not.` |
|         - |  3749 | ` * Return TRUE if reserved.FALSE otherwise.` |
|         - |  3750 | ` */` |
|    290550 |  3751 | `static int GenStateIsReservedConstant(SyString *pName)` |
|         5 |  3752 | `{` |
|    290555 |  3753 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      3869 |  3754 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|         3 |  3755 | `			return TRUE;` |
|      3867 |  3756 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|         6 |  3757 | `			return TRUE;` |
|         5 |  3758 | `		}` |
|    288620 |  3759 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|      7665 |  3760 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|         3 |  3761 | `			return TRUE;` |
|         - |  3762 | `		}` |
|      3829 |  3763 | `	}` |
|         - |  3764 | `	/* Not a reserved constant */` |
|    290547 |  3765 | `	return FALSE;` |
|    145280 |  3766 | `}` |
|         - |  3767 | `/*` |
|         - |  3768 | ` * Compile the 'const' statement.` |
|         - |  3769 | ` * According to the PHP language reference` |
|         - |  3770 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|         - |  3771 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|         - |  3772 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|         - |  3773 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|         - |  3774 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|         - |  3775 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|         - |  3776 | ` *  Syntax` |
|         - |  3777 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|         - |  3778 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|         - |  3779 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|         - |  3780 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|         - |  3781 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|         - |  3782 | ` *  to get a list of all defined constants.` |
|         - |  3783 | ` *` |
|         - |  3784 | ` * Symisc eXtension.` |
|         - |  3785 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|         - |  3786 | ` *  would allow only simple scalar value.` |
|         - |  3787 | ` *  Example` |
|         - |  3788 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|         - |  3789 | ` *    Refer to the official documentation for more information on this feature.` |
|         - |  3790 | ` */` |
|        50 |  3791 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|         5 |  3792 | `{` |
|         - |  3793 | `	SySet *pConsCode,*pInstrContainer;` |
|        55 |  3794 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  3795 | `	SyString *pName;` |
|         - |  3796 | `	sxi32 rc;` |
|        55 |  3797 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        55 |  3798 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  3799 | `		/* Invalid constant name */` |
|         8 |  3800 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"identifier");` |
|         8 |  3801 | `		if( rc == SXERR_ABORT ){` |
|         - |  3802 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3803 | `			return SXERR_ABORT;` |
|         - |  3804 | `		}` |
|         8 |  3805 | `		goto Synchronize;` |
|         - |  3806 | `	}` |
|         - |  3807 | `	/* Peek constant name */` |
|        49 |  3808 | `	pName = &pGen->pIn->sData;` |
|         - |  3809 | `	/* Make sure the constant name isn't reserved */` |
|        49 |  3810 | `	if( GenStateIsReservedConstant(pName) ){` |
|         - |  3811 | `		/* Reserved constant */` |
|        10 |  3812 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Cannot redeclare constant '%z'",pName);` |
|        10 |  3813 | `		if( rc == SXERR_ABORT ){` |
|         - |  3814 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3815 | `			return SXERR_ABORT;` |
|         - |  3816 | `		}` |
|        10 |  3817 | `		goto Synchronize;` |
|         - |  3818 | `	}` |
|        40 |  3819 | `	pGen->pIn++;` |
|        40 |  3820 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|         - |  3821 | `		/* Invalid statement*/` |
|         6 |  3822 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"=\"");` |
|         6 |  3823 | `		if( rc == SXERR_ABORT ){` |
|         - |  3824 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3825 | `			return SXERR_ABORT;` |
|         - |  3826 | `		}` |
|         6 |  3827 | `		goto Synchronize;` |
|         - |  3828 | `	}` |
|        34 |  3829 | `	pGen->pIn++; /*Jump the equal sign */` |
|         - |  3830 | `	/* Allocate a new constant value container */` |
|        34 |  3831 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|        34 |  3832 | `	if( pConsCode == 0 ){` |
|       ! 0 |  3833 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3834 | `		return SXERR_ABORT;` |
|         - |  3835 | `	}` |
|        34 |  3836 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - |  3837 | `	/* Swap bytecode container */` |
|        34 |  3838 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        34 |  3839 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|         - |  3840 | `	/* Compile constant value */` |
|        34 |  3841 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  3842 | `	/* Emit the done instruction */` |
|        34 |  3843 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        34 |  3844 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        34 |  3845 | `	if( rc == SXERR_ABORT ){` |
|         - |  3846 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  3847 | `		return SXERR_ABORT;` |
|         - |  3848 | `	}` |
|        34 |  3849 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|         - |  3850 | `	/* Register the constant with namespace-qualified name */` |
|         - |  3851 | `	{` |
|         - |  3852 | `		SyBlob sFQN;` |
|         - |  3853 | `		SyString sFQNStr;` |
|        34 |  3854 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        34 |  3855 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|        34 |  3856 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        50 |  3857 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|        32 |  3858 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|        34 |  3859 | `		if( rc == SXRET_OK && SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - |  3860 | ``			/* php 8.5: attributes on `const` statements — attach the pending`` |
|         - |  3861 | `			 * groups to the registered constant record for Reflection. */` |
|         7 |  3862 | `			SyHashEntry *pCEntry = SyHashGet(&pGen->pVm->hConstant,` |
|         4 |  3863 | `				SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|         5 |  3864 | `			if( pCEntry ){` |
|         5 |  3865 | `				ph7_constant *pRegCons = (ph7_constant *)pCEntry->pUserData;` |
|         5 |  3866 | `				if( GenStateConsumeAttrs(&(*pGen),&pRegCons->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  3867 | `					SyBlobRelease(&sFQN);` |
|       ! 0 |  3868 | `					return SXERR_ABORT;` |
|         - |  3869 | `				}` |
|         2 |  3870 | `			}` |
|         2 |  3871 | `		}` |
|        34 |  3872 | `		SyBlobRelease(&sFQN);` |
|         - |  3873 | `	}` |
|        34 |  3874 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3875 | `		SySetRelease(pConsCode);` |
|       ! 0 |  3876 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|       ! 0 |  3877 | `	}` |
|        34 |  3878 | `	return SXRET_OK;` |
|         9 |  3879 | `Synchronize:` |
|         - |  3880 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|        60 |  3881 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        41 |  3882 | `		pGen->pIn++;` |
|         3 |  3883 | `	}` |
|        22 |  3884 | `	return SXRET_OK;` |
|        30 |  3885 | `}` |
|         - |  3886 | `/*` |
|         - |  3887 | ` * Compile the 'continue' statement.` |
|         - |  3888 | ` * According to the PHP language reference` |
|         - |  3889 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|         - |  3890 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|         - |  3891 | ` *  iteration.` |
|         - |  3892 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|         - |  3893 | ` *  the purposes of continue.` |
|         - |  3894 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|         - |  3895 | ` *  of enclosing loops it should skip to the end of.` |
|         - |  3896 | ` *  Note:` |
|         - |  3897 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|         - |  3898 | ` */` |
|         - |  3899 | `/*` |
|         - |  3900 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|         - |  3901 | ` * block and the target loop block. This ensures finally blocks run when` |
|         - |  3902 | ` * break/continue crosses a try boundary.` |
|         - |  3903 | ` *` |
|         - |  3904 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|         - |  3905 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|         - |  3906 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|         - |  3907 | ` */` |
|    149134 |  3908 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|         5 |  3909 | `{` |
|    149139 |  3910 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    149139 |  3911 | `	int nInlineTry = 0;` |
|    672725 |  3912 | `	while( pBlock && pBlock != pTarget ){` |
|    523591 |  3913 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|         6 |  3914 | `			if( pBlock->pUserData ){` |
|         - |  3915 | `				/* A try block with an exception context. In a generator its catch/finally` |
|         - |  3916 | `				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that` |
|         - |  3917 | `				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.` |
|         - |  3918 | `				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */` |
|         6 |  3919 | `				if( pGen->bInGenerator ){` |
|         3 |  3920 | `					nInlineTry++;` |
|         2 |  3921 | `				}else{` |
|         3 |  3922 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|         - |  3923 | `				}` |
|         4 |  3924 | `			}else{` |
|         - |  3925 | `				/* A catch/finally block compiled into a separate bytecode container` |
|         - |  3926 | `				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */` |
|       ! 0 |  3927 | `				break;` |
|         - |  3928 | `			}` |
|         2 |  3929 | `		}` |
|    523591 |  3930 | `		pBlock = pBlock->pParent;` |
|         5 |  3931 | `	}` |
|    149139 |  3932 | `	return nInlineTry;` |
|         5 |  3933 | `}` |
|     84090 |  3934 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|         5 |  3935 | `{` |
|         - |  3936 | `	GenBlock *pLoop; /* Target loop */` |
|         - |  3937 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|         - |  3938 | `	sxu32 nLineLocal;` |
|         - |  3939 | `	sxi32 rc;` |
|     84095 |  3940 | `	nLineLocal = pGen->pIn->nLine;` |
|     84095 |  3941 | `	iLevel = 0;` |
|         - |  3942 | `	/* Jump the 'continue' keyword */` |
|     84095 |  3943 | `	pGen->pIn++;` |
|     84095 |  3944 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|         - |  3945 | `		/* optional numeric argument which tells us how many levels` |
|         - |  3946 | `		 * of enclosing loops we should skip to the end of.` |
|         - |  3947 | `		 */` |
|         - |  3948 | `		char zScratch[GEN_NUM_SCRATCH];` |
|        17 |  3949 | `		char *zAlloc = 0;` |
|         - |  3950 | `		SyString sNum;` |
|        17 |  3951 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|        17 |  3952 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3953 | `			return SXERR_ABORT;` |
|         - |  3954 | `		}` |
|        17 |  3955 | `		if( rc == SXRET_OK ){` |
|        20 |  3956 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|        12 |  3957 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|        14 |  3958 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  3959 | `				return SXERR_ABORT;` |
|         - |  3960 | `			}` |
|        14 |  3961 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|        14 |  3962 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         6 |  3963 | `		}` |
|        17 |  3964 | `		if( iLevel < 2 ){` |
|         3 |  3965 | `			iLevel = 0;` |
|         1 |  3966 | `		}` |
|        17 |  3967 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|         7 |  3968 | `	}` |
|         - |  3969 | `	/* Point to the target loop */` |
|     84095 |  3970 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     84095 |  3971 | `	if( pLoop == 0 ){` |
|         - |  3972 | `		/* Illegal continue */` |
|        12 |  3973 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"'continue' not in the 'loop' or 'switch' context");` |
|        12 |  3974 | `		if( rc == SXERR_ABORT ){` |
|         - |  3975 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3976 | `			return SXERR_ABORT;` |
|         - |  3977 | `		}` |
|         7 |  3978 | `	}else{` |
|     84085 |  3979 | `		sxu32 nInstrIdx = 0;` |
|         - |  3980 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     84085 |  3981 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|         - |  3982 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|         - |  3983 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|     84085 |  3984 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|     84085 |  3985 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|         - |  3986 | ``			/* `continue` inside a switch acts like `break` — which is almost never what`` |
|         - |  3987 | `			 * the author meant, so php 7.3+ says so at compile time. The generated jump` |
|         - |  3988 | `			 * is unchanged; only the diagnostic was missing. An explicit level` |
|         - |  3989 | ``			 * (`continue 2`) targets the enclosing loop and stays silent. */`` |
|         5 |  3990 | `			if( iLevel < 1 ){` |
|         5 |  3991 | `				PH7_GenCompileError(&(*pGen),E_WARNING,nLineLocal,` |
|         - |  3992 | `					"\"continue\" targeting switch is equivalent to \"break\"."` |
|         - |  3993 | `					" Did you mean to use \"continue 2\"?");` |
|         2 |  3994 | `			}` |
|         5 |  3995 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|         5 |  3996 | `			if( rc == SXRET_OK ){` |
|         5 |  3997 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|         2 |  3998 | `			}` |
|         3 |  3999 | `		}else{` |
|         - |  4000 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|     84081 |  4001 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|     84081 |  4002 | `			if( pLoop->bPostContinue == TRUE ){` |
|         - |  4003 | `				JumpFixup sJumpFix;` |
|         - |  4004 | `				/* Post-continue */` |
|     26757 |  4005 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|     26757 |  4006 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|     26757 |  4007 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|     13376 |  4008 | `			}` |
|         - |  4009 | `		}` |
|         - |  4010 | `	}` |
|     84095 |  4011 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4012 | `		/* Not so fatal,emit a warning only */` |
|       ! 0 |  4013 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|       ! 0 |  4014 | `	}` |
|         - |  4015 | `	/* Statement successfully compiled */` |
|     84095 |  4016 | `	return SXRET_OK;` |
|     42050 |  4017 | `}` |
|         - |  4018 | `/*` |
|         - |  4019 | ` * Compile the 'break' statement.` |
|         - |  4020 | ` * According to the PHP language reference` |
|         - |  4021 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|         - |  4022 | ` *  structure.` |
|         - |  4023 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|         - |  4024 | ` *  enclosing structures are to be broken out of.` |
|         - |  4025 | ` */` |
|     65070 |  4026 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|         5 |  4027 | `{` |
|         - |  4028 | `	GenBlock *pLoop; /* Target loop */` |
|         - |  4029 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|         - |  4030 | `	sxi32 rc;` |
|     65075 |  4031 | `	iLevel = 0;` |
|         - |  4032 | `	/* Jump the 'break' keyword */` |
|     65075 |  4033 | `	pGen->pIn++;` |
|     65075 |  4034 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|         - |  4035 | `		/* optional numeric argument which tells us how many levels` |
|         - |  4036 | `		 * of enclosing loops we should skip to the end of.` |
|         - |  4037 | `		 */` |
|         - |  4038 | `		char zScratch[GEN_NUM_SCRATCH];` |
|        18 |  4039 | `		char *zAlloc = 0;` |
|         - |  4040 | `		SyString sNum;` |
|        18 |  4041 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|        18 |  4042 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4043 | `			return SXERR_ABORT;` |
|         - |  4044 | `		}` |
|        18 |  4045 | `		if( rc == SXRET_OK ){` |
|        21 |  4046 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|        12 |  4047 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|        15 |  4048 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  4049 | `				return SXERR_ABORT;` |
|         - |  4050 | `			}` |
|        15 |  4051 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|        15 |  4052 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         6 |  4053 | `		}` |
|        18 |  4054 | `		if( iLevel < 2 ){` |
|         3 |  4055 | `			iLevel = 0;` |
|         1 |  4056 | `		}` |
|        18 |  4057 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|         7 |  4058 | `	}` |
|         - |  4059 | `	/* Extract the target loop */` |
|     65075 |  4060 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     65075 |  4061 | `	if( pLoop == 0 ){` |
|         - |  4062 | `		/* Illegal break */` |
|        18 |  4063 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"'break' not in the 'loop' or 'switch' context");` |
|        18 |  4064 | `		if( rc == SXERR_ABORT ){` |
|         - |  4065 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4066 | `			return SXERR_ABORT;` |
|         - |  4067 | `		}` |
|        10 |  4068 | `	}else{` |
|         - |  4069 | `		sxu32 nInstrIdx;` |
|         - |  4070 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     65059 |  4071 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|         - |  4072 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|     65059 |  4073 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|     65059 |  4074 | `		if( rc == SXRET_OK ){` |
|         - |  4075 | `			/* Fix the jump later when the jump destination is resolved */` |
|     65059 |  4076 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|     32527 |  4077 | `		}` |
|         - |  4078 | `	}` |
|     65075 |  4079 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4080 | `		/* Not so fatal,emit a warning only */` |
|       ! 0 |  4081 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|       ! 0 |  4082 | `	}` |
|         - |  4083 | `	/* Statement successfully compiled */` |
|     65075 |  4084 | `	return SXRET_OK;` |
|     32540 |  4085 | `}` |
|         - |  4086 | `/*` |
|         - |  4087 | ` * Compile or record a label.` |
|         - |  4088 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|         - |  4089 | ` * Example` |
|         - |  4090 | ` *  goto LABEL;` |
|         - |  4091 | ` *   echo 'Foo';` |
|         - |  4092 | ` *  LABEL:` |
|         - |  4093 | ` *   echo 'Bar';` |
|         - |  4094 | ` */` |
|       112 |  4095 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|         5 |  4096 | `{` |
|         - |  4097 | `	GenBlock *pBlock;` |
|         - |  4098 | `	Label sLabel;` |
|         - |  4099 | `	/* php places NO restriction on where a label may be DEFINED — inside a loop, a switch` |
|         - |  4100 | `	 * or a try{} is all fine. The only rule is on the jump: you may not goto INTO a loop` |
|         - |  4101 | `	 * or switch from outside it, which is checked once the labels are all known (see` |
|         - |  4102 | `	 * GenStateFixJumps). Record the loop this label sits in so that check can run. */` |
|         - |  4103 | `	{` |
|       117 |  4104 | `		SyString *pTarget = &pGen->pIn->sData;` |
|         - |  4105 | `		char *zDup;` |
|         - |  4106 | `		/* Initialize label fields */` |
|       117 |  4107 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|         - |  4108 | `		/* Duplicate label name */` |
|       117 |  4109 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|       117 |  4110 | `		if( zDup == 0 ){` |
|       ! 0 |  4111 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  4112 | `			return SXERR_ABORT;` |
|         - |  4113 | `		}` |
|       117 |  4114 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|       117 |  4115 | `		sLabel.bRef  = FALSE;` |
|       117 |  4116 | `		sLabel.nLine = pGen->pIn->nLine;` |
|       117 |  4117 | `		sLabel.nLoopId = pGen->nCurLoopId;` |
|       117 |  4118 | `		pBlock = pGen->pCurrent;` |
|       233 |  4119 | `		while( pBlock ){` |
|       143 |  4120 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|        26 |  4121 | `				break;` |
|         - |  4122 | `			}` |
|         - |  4123 | `			/* Point to the upper block */` |
|       121 |  4124 | `			pBlock = pBlock->pParent;` |
|         5 |  4125 | `		}` |
|       117 |  4126 | `		if( pBlock ){` |
|        26 |  4127 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        15 |  4128 | `		}else{` |
|        95 |  4129 | `			sLabel.pFunc = 0;` |
|         - |  4130 | `		}` |
|         - |  4131 | `		/* Insert in label set */` |
|       117 |  4132 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|         - |  4133 | `	}` |
|       117 |  4134 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|       117 |  4135 | `	return SXRET_OK;` |
|        61 |  4136 | `}` |
|         - |  4137 | `/*` |
|         - |  4138 | ` * Compile the so hated 'goto' statement.` |
|         - |  4139 | ` * You've probably been taught that gotos are bad, but this sort` |
|         - |  4140 | ` * of rewriting  happens all the time, in fact every time you run` |
|         - |  4141 | ` * a compiler it has to do this.` |
|         - |  4142 | ` * According to the PHP language reference manual` |
|         - |  4143 | ` *   The goto operator can be used to jump to another section in the program.` |
|         - |  4144 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|         - |  4145 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|         - |  4146 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|         - |  4147 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|         - |  4148 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|         - |  4149 | ` *   of a multi-level break` |
|         - |  4150 | ` */` |
|       152 |  4151 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|         5 |  4152 | `{` |
|         - |  4153 | `	JumpFixup sJump;` |
|         - |  4154 | `	sxi32 rc;` |
|       157 |  4155 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|       157 |  4156 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  4157 | `		/* Missing label */` |
|       ! 0 |  4158 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|       ! 0 |  4159 | `		if( rc == SXERR_ABORT ){` |
|         - |  4160 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4161 | `			return SXERR_ABORT;` |
|         - |  4162 | `		}` |
|       ! 0 |  4163 | `		return SXRET_OK;` |
|         - |  4164 | `	}` |
|       157 |  4165 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|         6 |  4166 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn,"identifier");` |
|         6 |  4167 | `		if( rc == SXERR_ABORT ){` |
|         - |  4168 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4169 | `			return SXERR_ABORT;` |
|         - |  4170 | `		}` |
|         4 |  4171 | `	}else{` |
|       153 |  4172 | `		SyString *pTarget = &pGen->pIn->sData;` |
|         - |  4173 | `		GenBlock *pBlock;` |
|         - |  4174 | `		char *zDup;` |
|         - |  4175 | `		/* Prepare the jump destination */` |
|       153 |  4176 | `		sJump.nJumpType = PH7_OP_JMP;` |
|       153 |  4177 | `		sJump.nLine = pGen->pIn->nLine;` |
|         - |  4178 | `		/* Duplicate label name */` |
|       153 |  4179 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|       153 |  4180 | `		if( zDup == 0 ){` |
|       ! 0 |  4181 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  4182 | `			return SXERR_ABORT;` |
|         - |  4183 | `		}` |
|       153 |  4184 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|         - |  4185 | `		/* The loop/switch this goto sits in, for the "goto into a loop" check later. */` |
|       153 |  4186 | `		sJump.nLoopId = pGen->nCurLoopId;` |
|         - |  4187 | `		/* A goto inside a try{}/catch{} is legal php (jumping OUT of the block is fine);` |
|         - |  4188 | `		 * only the owning function matters here, since a goto may not cross functions. */` |
|       153 |  4189 | `		pBlock = pGen->pCurrent;` |
|       327 |  4190 | `		while( pBlock ){` |
|       205 |  4191 | `			if( pBlock->iFlags & GEN_BLOCK_FUNC ){` |
|        30 |  4192 | `				break;` |
|         - |  4193 | `			}` |
|         - |  4194 | `			/* Point to the upper block */` |
|       179 |  4195 | `			pBlock = pBlock->pParent;` |
|         5 |  4196 | `		}` |
|       153 |  4197 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|        30 |  4198 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        17 |  4199 | `		}else{` |
|       127 |  4200 | `			sJump.pFunc = 0;` |
|         - |  4201 | `		}` |
|         - |  4202 | `		/* Emit the unconditional jump */` |
|       153 |  4203 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|       153 |  4204 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|        74 |  4205 | `		}` |
|         - |  4206 | `	}` |
|       157 |  4207 | `	pGen->pIn++; /* Jump the label name */` |
|       157 |  4208 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         3 |  4209 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|         1 |  4210 | `	}` |
|         - |  4211 | `	/* Statement successfully compiled */` |
|       157 |  4212 | `	return SXRET_OK;` |
|        81 |  4213 | `}` |
|         - |  4214 | `/*` |
|         - |  4215 | ` * Point to the next PHP chunk that will be processed shortly.` |
|         - |  4216 | ` * Return SXRET_OK on success. Any other return value indicates` |
|         - |  4217 | ` * failure.` |
|         - |  4218 | ` */` |
|        20 |  4219 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|         1 |  4220 | `{` |
|         - |  4221 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|         - |  4222 | `	sxu32 nRawObj;` |
|        10 |  4223 | `	sxu32 nObjIdx;` |
|         - |  4224 | `	/* Consume raw chunks verbatim without any processing until we get` |
|         - |  4225 | `	 * a PHP block.` |
|         - |  4226 | `	 */` |
|        10 |  4227 | `Consume:` |
|        21 |  4228 | `	nRawObj = nObjIdx = 0;` |
|        21 |  4229 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|       ! 0 |  4230 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|       ! 0 |  4231 | `		if( pRawObj == 0 ){` |
|       ! 0 |  4232 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  4233 | `			return SXERR_ABORT;` |
|         - |  4234 | `		}` |
|         - |  4235 | `		/* Mark as constant and emit the load constant instruction */` |
|       ! 0 |  4236 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|       ! 0 |  4237 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|       ! 0 |  4238 | `		++nRawObj;` |
|       ! 0 |  4239 | `		pGen->pRawIn++; /* Next chunk */` |
|       ! 0 |  4240 | `	}` |
|        21 |  4241 | `	if( nRawObj > 0 ){` |
|         - |  4242 | `		/* Emit the consume instruction */` |
|       ! 0 |  4243 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|       ! 0 |  4244 | `	}` |
|        21 |  4245 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|       ! 0 |  4246 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|         - |  4247 | `		/* Reset the token set (and its trivia sidecar) */` |
|       ! 0 |  4248 | `		SySetReset(pTokenSet);` |
|       ! 0 |  4249 | `		SySetReset(&pGen->aTrivia);` |
|         - |  4250 | `		/* Tokenize input */` |
|       ! 0 |  4251 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|       ! 0 |  4252 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|         - |  4253 | `		/* Point to the fresh token stream */` |
|       ! 0 |  4254 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|       ! 0 |  4255 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|         - |  4256 | `		/* Advance the stream cursor */` |
|       ! 0 |  4257 | `		pGen->pRawIn++;` |
|         - |  4258 | `		/* TICKET 1433-011 */` |
|       ! 0 |  4259 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - |  4260 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - |  4261 | `			sxi32 rc;` |
|         - |  4262 | `			/* Refer to TICKET 1433-009  */` |
|       ! 0 |  4263 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       ! 0 |  4264 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       ! 0 |  4265 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       ! 0 |  4266 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 |  4267 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4268 | `				return SXERR_ABORT;` |
|       ! 0 |  4269 | `			}else if( rc != SXERR_EMPTY ){` |
|       ! 0 |  4270 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       ! 0 |  4271 | `			}` |
|       ! 0 |  4272 | `			goto Consume;` |
|         - |  4273 | `		}` |
|       ! 0 |  4274 | `	}else{` |
|         - |  4275 | `		/* No more chunks to process */` |
|        21 |  4276 | `		pGen->pIn = pGen->pEnd;` |
|        21 |  4277 | `		return SXERR_EOF;` |
|         - |  4278 | `	}` |
|       ! 0 |  4279 | `	return SXRET_OK;` |
|        11 |  4280 | `}` |
|         - |  4281 | `/*` |
|         - |  4282 | ` * Compile a PHP block.` |
|         - |  4283 | ` * A block is simply one or more PHP statements and expressions to compile` |
|         - |  4284 | ` * optionally delimited by braces {}.` |
|         - |  4285 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  4286 | ` * and this function takes care of generating the appropriate error` |
|         - |  4287 | ` * message.` |
|         - |  4288 | ` */` |
|   6031454 |  4289 | `static sxi32 PH7_CompileBlock(` |
|         - |  4290 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  4291 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|         - |  4292 | `	)` |
|         5 |  4293 | `{` |
|         - |  4294 | `	sxi32 rc;` |
|         - |  4295 | `	sxu32 nLine;` |
|   6031459 |  4296 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|   6007449 |  4297 | `		nLine = pGen->pIn->nLine;` |
|   6007449 |  4298 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|   6007449 |  4299 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  4300 | `			return SXERR_ABORT;` |
|         - |  4301 | `		}` |
|   6007449 |  4302 | `		pGen->pIn++;` |
|         - |  4303 | `		/* Compile until we hit the closing braces '}' */` |
|   8885161 |  4304 | `		for(;;){` |
|  17770327 |  4305 | `			if( pGen->pIn >= pGen->pEnd ){` |
|        21 |  4306 | `				rc = GenStateNextChunk(&(*pGen));` |
|        21 |  4307 | `				if (rc == SXERR_ABORT ){` |
|       ! 0 |  4308 | `			 	   return SXERR_ABORT;` |
|         - |  4309 | `				}` |
|        21 |  4310 | `				if( rc == SXERR_EOF ){` |
|         - |  4311 | `					/* No more token to process: the block was never closed. php reports` |
|         - |  4312 | `					 * the line the '{' was opened on, not where the input ran out. */` |
|        21 |  4313 | `					PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"Unclosed '{' on line %u",nLine);` |
|        21 |  4314 | `					break;` |
|         - |  4315 | `				}` |
|       ! 0 |  4316 | `			}` |
|  17770307 |  4317 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|         - |  4318 | `				/* Closing braces found,break immediately*/` |
|   6007429 |  4319 | `				pGen->pIn++;` |
|   6007429 |  4320 | `				break;` |
|         - |  4321 | `			}` |
|         - |  4322 | `			/* Compile a single statement */` |
|  11762883 |  4323 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  11762883 |  4324 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4325 | `				return SXERR_ABORT;` |
|         - |  4326 | `			}` |
|         5 |  4327 | `		}` |
|   6007449 |  4328 | `		GenStateLeaveBlock(&(*pGen),0);` |
|   3027737 |  4329 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|       ! 0 |  4330 | `		pGen->pIn++;` |
|       ! 0 |  4331 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|       ! 0 |  4332 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  4333 | `			return SXERR_ABORT;` |
|         - |  4334 | `		}` |
|         - |  4335 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|       ! 0 |  4336 | `		for(;;){` |
|       ! 0 |  4337 | `			if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  4338 | `				rc = GenStateNextChunk(&(*pGen));` |
|       ! 0 |  4339 | `				if (rc == SXERR_ABORT ){` |
|       ! 0 |  4340 | `			 	   return SXERR_ABORT;` |
|         - |  4341 | `				}` |
|       ! 0 |  4342 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|         - |  4343 | `					/* No more token to process */` |
|       ! 0 |  4344 | `					if( rc == SXERR_EOF ){` |
|       ! 0 |  4345 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|         - |  4346 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|       ! 0 |  4347 | `					}` |
|       ! 0 |  4348 | `					break;` |
|         - |  4349 | `				}` |
|       ! 0 |  4350 | `			}` |
|       ! 0 |  4351 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|         - |  4352 | `				sxi32 nKwrd;` |
|         - |  4353 | `				/* Keyword found */` |
|       ! 0 |  4354 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       ! 0 |  4355 | `				if( nKwrd == nKeywordEnd \|\|` |
|       ! 0 |  4356 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|         - |  4357 | `						/* Delimiter keyword found,break */` |
|       ! 0 |  4358 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|       ! 0 |  4359 | `							pGen->pIn++; /*  endif;endswitch... */` |
|       ! 0 |  4360 | `						}` |
|       ! 0 |  4361 | `						break;` |
|         - |  4362 | `				}` |
|       ! 0 |  4363 | `			}` |
|         - |  4364 | `			/* Compile a single statement */` |
|       ! 0 |  4365 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|       ! 0 |  4366 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4367 | `				return SXERR_ABORT;` |
|         - |  4368 | `			}` |
|       ! 0 |  4369 | `		}` |
|       ! 0 |  4370 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  4371 | `	}else{` |
|         - |  4372 | `		/* Compile a single statement */` |
|     24015 |  4373 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     24015 |  4374 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4375 | `			return SXERR_ABORT;` |
|         - |  4376 | `		}` |
|         - |  4377 | `	}` |
|         - |  4378 | `	/* Jump trailing semi-colons ';' */` |
|   6031459 |  4379 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       ! 0 |  4380 | `		pGen->pIn++;` |
|       ! 0 |  4381 | `	}` |
|   6031459 |  4382 | `	return SXRET_OK;` |
|   3015732 |  4383 | `}` |
|         - |  4384 | `/*` |
|         - |  4385 | ` * Compile the gentle 'while' statement.` |
|         - |  4386 | ` * According to the PHP language reference` |
|         - |  4387 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|         - |  4388 | ` *  The basic form of a while statement is:` |
|         - |  4389 | ` *  while (expr)` |
|         - |  4390 | ` *   statement` |
|         - |  4391 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|         - |  4392 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|         - |  4393 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|         - |  4394 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|         - |  4395 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|         - |  4396 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|         - |  4397 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|         - |  4398 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|         - |  4399 | ` *  while (expr):` |
|         - |  4400 | ` *    statement` |
|         - |  4401 | ` *   endwhile;` |
|         - |  4402 | ` */` |
|     65084 |  4403 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|         5 |  4404 | `{` |
|     65089 |  4405 | `	GenBlock *pWhileBlock = 0;` |
|     65089 |  4406 | `	SyToken *pTmp,*pEnd = 0;` |
|         - |  4407 | `	sxu32 nFalseJump;` |
|         - |  4408 | `	sxu32 nLine;` |
|         - |  4409 | `	sxi32 rc;` |
|     65089 |  4410 | `	nLine = pGen->pIn->nLine;` |
|         - |  4411 | `	/* Jump the 'while' keyword */` |
|     65089 |  4412 | `	pGen->pIn++;` |
|     65089 |  4413 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4414 | `		/* Syntax error */` |
|       ! 0 |  4415 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|       ! 0 |  4416 | `		if( rc == SXERR_ABORT ){` |
|         - |  4417 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4418 | `			return SXERR_ABORT;` |
|         - |  4419 | `		}` |
|       ! 0 |  4420 | `		goto Synchronize;` |
|         - |  4421 | `	}` |
|         - |  4422 | `	/* Jump the left parenthesis '(' */` |
|     65089 |  4423 | `	pGen->pIn++;` |
|         - |  4424 | `	/* Create the loop block */` |
|     65089 |  4425 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|     65089 |  4426 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4427 | `		return SXERR_ABORT;` |
|         - |  4428 | `	}` |
|         - |  4429 | `	/* Delimit the condition */` |
|     65089 |  4430 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     65089 |  4431 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4432 | `		/* Empty expression */` |
|         3 |  4433 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|         3 |  4434 | `		if( rc == SXERR_ABORT ){` |
|         - |  4435 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4436 | `			return SXERR_ABORT;` |
|         - |  4437 | `		}` |
|         1 |  4438 | `	}` |
|         - |  4439 | `	/* Swap token streams */` |
|     65089 |  4440 | `	pTmp = pGen->pEnd;` |
|     65089 |  4441 | `	pGen->pEnd = pEnd;` |
|         - |  4442 | `	/* Compile the expression */` |
|     65089 |  4443 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     65089 |  4444 | `	if( rc == SXERR_ABORT ){` |
|         - |  4445 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4446 | `		return SXERR_ABORT;` |
|         - |  4447 | `	}` |
|         - |  4448 | `	/* Update token stream */` |
|     65089 |  4449 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 |  4450 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  4451 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4452 | `			return SXERR_ABORT;` |
|         - |  4453 | `		}` |
|       ! 0 |  4454 | `		pGen->pIn++;` |
|       ! 0 |  4455 | `	}` |
|         - |  4456 | `	/* Synchronize pointers */` |
|     65089 |  4457 | `	pGen->pIn  = &pEnd[1];` |
|     65089 |  4458 | `	pGen->pEnd = pTmp;` |
|         - |  4459 | `	/* Emit the false jump */` |
|     65089 |  4460 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|         - |  4461 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|     65089 |  4462 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|         - |  4463 | `	/* Compile the loop body */` |
|     65089 |  4464 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|     65089 |  4465 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4466 | `		return SXERR_ABORT;` |
|         - |  4467 | `	}` |
|         - |  4468 | `	/* Emit the unconditional jump to the start of the loop */` |
|     65089 |  4469 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|         - |  4470 | `	/* Fix all jumps now the destination is resolved */` |
|     65089 |  4471 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4472 | `	/* Release the loop block */` |
|     65089 |  4473 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4474 | `	/* Statement successfully compiled */` |
|     65089 |  4475 | `	return SXRET_OK;` |
|       ! 0 |  4476 | `Synchronize:` |
|         - |  4477 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4478 | `	 * compiling this erroneous block.` |
|         - |  4479 | `	 */` |
|       ! 0 |  4480 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4481 | `		pGen->pIn++;` |
|       ! 0 |  4482 | `	}` |
|       ! 0 |  4483 | `	return SXRET_OK;` |
|     32547 |  4484 | `}` |
|         - |  4485 | `/*` |
|         - |  4486 | ` * Compile the ugly do..while() statement.` |
|         - |  4487 | ` * According to the PHP language reference` |
|         - |  4488 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|         - |  4489 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|         - |  4490 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|         - |  4491 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|         - |  4492 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|         - |  4493 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|         - |  4494 | ` *  would end immediately).` |
|         - |  4495 | ` *  There is just one syntax for do-while loops:` |
|         - |  4496 | ` *  <?php` |
|         - |  4497 | ` *  $i = 0;` |
|         - |  4498 | ` *  do {` |
|         - |  4499 | ` *   echo $i;` |
|         - |  4500 | ` *  } while ($i > 0);` |
|         - |  4501 | ` * ?>` |
|         - |  4502 | ` */` |
|         2 |  4503 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|         1 |  4504 | `{` |
|         3 |  4505 | `	SyToken *pTmp,*pEnd = 0;` |
|         3 |  4506 | `	GenBlock *pDoBlock = 0;` |
|         - |  4507 | `	sxu32 nLine;` |
|         - |  4508 | `	sxi32 rc;` |
|         3 |  4509 | `	nLine = pGen->pIn->nLine;` |
|         - |  4510 | `	/* Jump the 'do' keyword */` |
|         3 |  4511 | `	pGen->pIn++;` |
|         - |  4512 | `	/* Create the loop block */` |
|         3 |  4513 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|         3 |  4514 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4515 | `		return SXERR_ABORT;` |
|         - |  4516 | `	}` |
|         - |  4517 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|         3 |  4518 | `	pDoBlock->bPostContinue = TRUE;` |
|         3 |  4519 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|         3 |  4520 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4521 | `		return SXERR_ABORT;` |
|         - |  4522 | `	}` |
|         3 |  4523 | `	if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4524 | `		nLine = pGen->pIn->nLine;` |
|       ! 0 |  4525 | `	}` |
|         3 |  4526 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|       ! 0 |  4527 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|         - |  4528 | `			/* Missing 'while' statement */` |
|         3 |  4529 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|         3 |  4530 | `			if( rc == SXERR_ABORT ){` |
|         - |  4531 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4532 | `				return SXERR_ABORT;` |
|         - |  4533 | `			}` |
|         3 |  4534 | `			goto Synchronize;` |
|         - |  4535 | `	}` |
|         - |  4536 | `	/* Jump the 'while' keyword */` |
|       ! 0 |  4537 | `	pGen->pIn++;` |
|       ! 0 |  4538 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4539 | `		/* Syntax error */` |
|       ! 0 |  4540 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|       ! 0 |  4541 | `		if( rc == SXERR_ABORT ){` |
|         - |  4542 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4543 | `			return SXERR_ABORT;` |
|         - |  4544 | `		}` |
|       ! 0 |  4545 | `		goto Synchronize;` |
|         - |  4546 | `	}` |
|         - |  4547 | `	/* Jump the left parenthesis '(' */` |
|       ! 0 |  4548 | `	pGen->pIn++;` |
|         - |  4549 | `	/* Delimit the condition */` |
|       ! 0 |  4550 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|       ! 0 |  4551 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4552 | `		/* Empty expression */` |
|       ! 0 |  4553 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       ! 0 |  4554 | `		if( rc == SXERR_ABORT ){` |
|         - |  4555 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4556 | `			return SXERR_ABORT;` |
|         - |  4557 | `		}` |
|       ! 0 |  4558 | `		goto Synchronize;` |
|         - |  4559 | `	}` |
|         - |  4560 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|       ! 0 |  4561 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|         - |  4562 | `		JumpFixup *aPost;` |
|         - |  4563 | `		VmInstr *pInstr;` |
|         - |  4564 | `		sxu32 nJumpDest;` |
|         - |  4565 | `		sxu32 n;` |
|       ! 0 |  4566 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|       ! 0 |  4567 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       ! 0 |  4568 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|       ! 0 |  4569 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|       ! 0 |  4570 | `			if( pInstr ){` |
|         - |  4571 | `				/* Fix */` |
|       ! 0 |  4572 | `				pInstr->iP2 = nJumpDest;` |
|       ! 0 |  4573 | `			}` |
|       ! 0 |  4574 | `		}` |
|       ! 0 |  4575 | `	}` |
|         - |  4576 | `	/* Swap token streams */` |
|       ! 0 |  4577 | `	pTmp = pGen->pEnd;` |
|       ! 0 |  4578 | `	pGen->pEnd = pEnd;` |
|         - |  4579 | `	/* Compile the expression */` |
|       ! 0 |  4580 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       ! 0 |  4581 | `	if( rc == SXERR_ABORT ){` |
|         - |  4582 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4583 | `		return SXERR_ABORT;` |
|         - |  4584 | `	}` |
|         - |  4585 | `	/* Update token stream */` |
|       ! 0 |  4586 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 |  4587 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  4588 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4589 | `			return SXERR_ABORT;` |
|         - |  4590 | `		}` |
|       ! 0 |  4591 | `		pGen->pIn++;` |
|       ! 0 |  4592 | `	}` |
|       ! 0 |  4593 | `	pGen->pIn  = &pEnd[1];` |
|       ! 0 |  4594 | `	pGen->pEnd = pTmp;` |
|         - |  4595 | `	/* Emit the true jump to the beginning of the loop */` |
|       ! 0 |  4596 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|         - |  4597 | `	/* Fix all jumps now the destination is resolved */` |
|       ! 0 |  4598 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4599 | `	/* Release the loop block */` |
|       ! 0 |  4600 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4601 | `	/* Statement successfully compiled */` |
|       ! 0 |  4602 | `	return SXRET_OK;` |
|         1 |  4603 | `Synchronize:` |
|         - |  4604 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4605 | `	 * compiling this erroneous block.` |
|         - |  4606 | `	 */` |
|         3 |  4607 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4608 | `		pGen->pIn++;` |
|       ! 0 |  4609 | `	}` |
|         3 |  4610 | `	return SXRET_OK;` |
|         2 |  4611 | `}` |
|         - |  4612 | `/*` |
|         - |  4613 | ` * Compile the complex and powerful 'for' statement.` |
|         - |  4614 | ` * According to the PHP language reference` |
|         - |  4615 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|         - |  4616 | ` *  The syntax of a for loop is:` |
|         - |  4617 | ` *  for (expr1; expr2; expr3)` |
|         - |  4618 | ` *   statement` |
|         - |  4619 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|         - |  4620 | ` *  the beginning of the loop.` |
|         - |  4621 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|         - |  4622 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|         - |  4623 | ` *  to FALSE, the execution of the loop ends.` |
|         - |  4624 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|         - |  4625 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|         - |  4626 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|         - |  4627 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|         - |  4628 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|         - |  4629 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|         - |  4630 | ` *  of using the for truth expression.` |
|         - |  4631 | ` */` |
|    122388 |  4632 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|         5 |  4633 | `{` |
|    122393 |  4634 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|    122393 |  4635 | `	GenBlock *pForBlock = 0;` |
|         - |  4636 | `	sxu32 nFalseJump;` |
|         - |  4637 | `	sxu32 nLine;` |
|         - |  4638 | `	sxi32 rc;` |
|    122393 |  4639 | `	nLine = pGen->pIn->nLine;` |
|         - |  4640 | `	/* Jump the 'for' keyword */` |
|    122393 |  4641 | `	pGen->pIn++;` |
|    122393 |  4642 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4643 | `		/* Syntax error */` |
|       ! 0 |  4644 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|       ! 0 |  4645 | `		if( rc == SXERR_ABORT ){` |
|         - |  4646 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4647 | `			return SXERR_ABORT;` |
|         - |  4648 | `		}` |
|       ! 0 |  4649 | `		return SXRET_OK;` |
|         - |  4650 | `	}` |
|         - |  4651 | `	/* Jump the left parenthesis '(' */` |
|    122393 |  4652 | `	pGen->pIn++;` |
|         - |  4653 | `	/* Delimit the init-expr;condition;post-expr */` |
|    122393 |  4654 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    122393 |  4655 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4656 | `		/* Empty expression */` |
|       ! 0 |  4657 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|       ! 0 |  4658 | `		if( rc == SXERR_ABORT ){` |
|         - |  4659 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4660 | `			return SXERR_ABORT;` |
|         - |  4661 | `		}` |
|         - |  4662 | `		/* Synchronize */` |
|       ! 0 |  4663 | `		pGen->pIn = pEnd;` |
|       ! 0 |  4664 | `		if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4665 | `			pGen->pIn++;` |
|       ! 0 |  4666 | `		}` |
|       ! 0 |  4667 | `		return SXRET_OK;` |
|         - |  4668 | `	}` |
|         - |  4669 | `	/* Swap token streams */` |
|    122393 |  4670 | `	pTmp = pGen->pEnd;` |
|    122393 |  4671 | `	pGen->pEnd = pEnd;` |
|         - |  4672 | `	/* for() clauses are the ONLY place php's grammar allows a comma-separated` |
|         - |  4673 | `	 * expression list, so the comma operator is permitted for their duration` |
|         - |  4674 | `	 * (see GenStateTreeHasComma). A closure body nested inside a clause is` |
|         - |  4675 | `	 * compiled through this same window — recorded as a known leniency. */` |
|    122393 |  4676 | `	pGen->nCommaExprOk++;` |
|         - |  4677 | `	/* Compile initialization expressions if available */` |
|    122393 |  4678 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  4679 | `	/* Pop operand lvalues */` |
|    122393 |  4680 | `	if( rc == SXERR_ABORT ){` |
|         - |  4681 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4682 | `		return SXERR_ABORT;` |
|    122393 |  4683 | `	}else if( rc != SXERR_EMPTY ){` |
|    110931 |  4684 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     55463 |  4685 | `	}` |
|    122393 |  4686 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4687 | `		/* Syntax error */` |
|       ! 0 |  4688 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|       ! 0 |  4689 | `		if( rc == SXERR_ABORT ){` |
|         - |  4690 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4691 | `			return SXERR_ABORT;` |
|         - |  4692 | `		}` |
|       ! 0 |  4693 | `		return SXRET_OK;` |
|         - |  4694 | `	}` |
|         - |  4695 | `	/* Jump the trailing ';' */` |
|    122393 |  4696 | `	pGen->pIn++;` |
|         - |  4697 | `	/* Create the loop block */` |
|    122393 |  4698 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|    122393 |  4699 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4700 | `		return SXERR_ABORT;` |
|         - |  4701 | `	}` |
|         - |  4702 | `	/* Deffer continue jumps */` |
|    122393 |  4703 | `	pForBlock->bPostContinue = TRUE;` |
|         - |  4704 | `	/* Compile the condition */` |
|    122393 |  4705 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    122393 |  4706 | `	if( rc == SXERR_ABORT ){` |
|         - |  4707 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4708 | `		return SXERR_ABORT;` |
|    122393 |  4709 | `	}else if( rc != SXERR_EMPTY ){` |
|         - |  4710 | `		/* Emit the false jump */` |
|    110931 |  4711 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|         - |  4712 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    110931 |  4713 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|     55463 |  4714 | `	}` |
|    122393 |  4715 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4716 | `		/* Syntax error */` |
|         6 |  4717 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|         6 |  4718 | `		if( rc == SXERR_ABORT ){` |
|         - |  4719 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4720 | `			return SXERR_ABORT;` |
|         - |  4721 | `		}` |
|         6 |  4722 | `		return SXRET_OK;` |
|         - |  4723 | `	}` |
|         - |  4724 | `	/* Jump the trailing ';' */` |
|    122389 |  4725 | `	pGen->pIn++;` |
|         - |  4726 | `	/* Save the post condition stream */` |
|    122389 |  4727 | `	pPostStart = pGen->pIn;` |
|         - |  4728 | `	/* Compile the loop body — OUTSIDE the comma window (the body is ordinary` |
|         - |  4729 | ``	 * php, so `(1, 2)` inside it is the parse error it should be). */`` |
|    122389 |  4730 | `	pGen->nCommaExprOk--;` |
|    122389 |  4731 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|    122389 |  4732 | `	pGen->pEnd = pTmp;` |
|    122389 |  4733 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|    122389 |  4734 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4735 | `		return SXERR_ABORT;` |
|         - |  4736 | `	}` |
|         - |  4737 | `	/* Fix post-continue jumps */` |
|    122389 |  4738 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|         - |  4739 | `		JumpFixup *aPost;` |
|         - |  4740 | `		VmInstr *pInstr;` |
|         - |  4741 | `		sxu32 nJumpDest;` |
|         - |  4742 | `		sxu32 n;` |
|     11477 |  4743 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|     11477 |  4744 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     38229 |  4745 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|     26757 |  4746 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     26757 |  4747 | `			if( pInstr ){` |
|         - |  4748 | `				/* Fix jump */` |
|     26757 |  4749 | `				pInstr->iP2 = nJumpDest;` |
|     13376 |  4750 | `			}` |
|     13381 |  4751 | `		}` |
|      5736 |  4752 | `	}` |
|         - |  4753 | `	/* compile the post-expressions if available */` |
|    122389 |  4754 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|       ! 0 |  4755 | `		pPostStart++;` |
|       ! 0 |  4756 | `	}` |
|    122389 |  4757 | `	if( pPostStart < pEnd ){` |
|         - |  4758 | `		SyToken *pTmpIn,*pTmpEnd;` |
|    110929 |  4759 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|    110929 |  4760 | `		pGen->nCommaExprOk++; /* post-expressions are a clause list again */` |
|    110929 |  4761 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    110929 |  4762 | `		pGen->nCommaExprOk--;` |
|    110929 |  4763 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - |  4764 | `			/* Syntax error */` |
|       ! 0 |  4765 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|       ! 0 |  4766 | `			if( rc == SXERR_ABORT ){` |
|         - |  4767 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4768 | `				return SXERR_ABORT;` |
|         - |  4769 | `			}` |
|       ! 0 |  4770 | `			return SXRET_OK;` |
|         - |  4771 | `		}` |
|    110929 |  4772 | `		RE_SWAP_DELIMITER(pGen);` |
|    110929 |  4773 | `		if( rc == SXERR_ABORT ){` |
|         - |  4774 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4775 | `			return SXERR_ABORT;` |
|    110929 |  4776 | `		}else if( rc != SXERR_EMPTY){` |
|         - |  4777 | `			/* Pop operand lvalue */` |
|    110929 |  4778 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     55462 |  4779 | `		}` |
|     55462 |  4780 | `	}` |
|         - |  4781 | `	/* Emit the unconditional jump to the start of the loop */` |
|    122389 |  4782 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|         - |  4783 | `	/* Fix all jumps now the destination is resolved */` |
|    122389 |  4784 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4785 | `	/* Release the loop block */` |
|    122389 |  4786 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4787 | `	/* Statement successfully compiled */` |
|    122389 |  4788 | `	return SXRET_OK;` |
|     61199 |  4789 | `}` |
|         - |  4790 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|         - |  4791 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|         - |  4792 | ` * are allowed.` |
|         - |  4793 | ` */` |
|    444166 |  4794 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 |  4795 | `{` |
|    444171 |  4796 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    444171 |  4797 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  4798 | `		/* Unexpected expression */` |
|       ! 0 |  4799 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  4800 | `			"foreach: Expecting a variable name");` |
|       ! 0 |  4801 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 |  4802 | `			rc = SXERR_INVALID;` |
|       ! 0 |  4803 | `		}` |
|       ! 0 |  4804 | `	}` |
|    444171 |  4805 | `	return rc;` |
|         5 |  4806 | `}` |
|         - |  4807 | `/*` |
|         - |  4808 | ` * Compile the 'foreach' statement.` |
|         - |  4809 | ` * According to the PHP language reference` |
|         - |  4810 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|         - |  4811 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|         - |  4812 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|         - |  4813 | ` *  is a minor but useful extension of the first:` |
|         - |  4814 | ` *  foreach (array_expression as $value)` |
|         - |  4815 | ` *    statement` |
|         - |  4816 | ` *  foreach (array_expression as $key => $value)` |
|         - |  4817 | ` *   statement` |
|         - |  4818 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|         - |  4819 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|         - |  4820 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|         - |  4821 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|         - |  4822 | ` *  to the variable $key on each loop.` |
|         - |  4823 | ` *  Note:` |
|         - |  4824 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|         - |  4825 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|         - |  4826 | ` *  Note:` |
|         - |  4827 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|         - |  4828 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|         - |  4829 | ` *  or after the foreach without resetting it.` |
|         - |  4830 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|         - |  4831 | ` *  of copying the value.` |
|         - |  4832 | ` */` |
|    310186 |  4833 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|         5 |  4834 | `{` |
|    310191 |  4835 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    310191 |  4836 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    310191 |  4837 | `	GenBlock *pForeachBlock = 0;` |
|         - |  4838 | `	ph7_foreach_info *pInfo;` |
|         - |  4839 | `	sxu32 nFalseJump;` |
|         - |  4840 | `	VmInstr *pInstr;` |
|         - |  4841 | `	sxu32 nLine;` |
|         - |  4842 | `	sxi32 rc;` |
|    310191 |  4843 | `	nLine = pGen->pIn->nLine;` |
|         - |  4844 | `	/* Jump the 'foreach' keyword */` |
|    310191 |  4845 | `	pGen->pIn++;` |
|    310191 |  4846 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4847 | `		/* Syntax error */` |
|       ! 0 |  4848 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|       ! 0 |  4849 | `		if( rc == SXERR_ABORT ){` |
|         - |  4850 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4851 | `			return SXERR_ABORT;` |
|         - |  4852 | `		}` |
|       ! 0 |  4853 | `		goto Synchronize;` |
|         - |  4854 | `	}` |
|         - |  4855 | `	/* Jump the left parenthesis '(' */` |
|    310191 |  4856 | `	pGen->pIn++;` |
|         - |  4857 | `	/* Create the loop block */` |
|    310191 |  4858 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    310191 |  4859 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4860 | `		return SXERR_ABORT;` |
|         - |  4861 | `	}` |
|         - |  4862 | `	/* Delimit the expression */` |
|    310191 |  4863 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    310191 |  4864 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4865 | `		/* Empty expression */` |
|       ! 0 |  4866 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|       ! 0 |  4867 | `		if( rc == SXERR_ABORT ){` |
|         - |  4868 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4869 | `			return SXERR_ABORT;` |
|         - |  4870 | `		}` |
|         - |  4871 | `		/* Synchronize */` |
|       ! 0 |  4872 | `		pGen->pIn = pEnd;` |
|       ! 0 |  4873 | `		if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4874 | `			pGen->pIn++;` |
|       ! 0 |  4875 | `		}` |
|       ! 0 |  4876 | `		return SXRET_OK;` |
|         - |  4877 | `	}` |
|         - |  4878 | `	/* Compile the array expression */` |
|    310191 |  4879 | `	pCur = pGen->pIn;` |
|   1784559 |  4880 | `	while( pCur < pEnd ){` |
|   1784559 |  4881 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    340765 |  4882 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    340765 |  4883 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|         - |  4884 | `				/* Break with the first 'as' found */` |
|    310191 |  4885 | `				break;` |
|         - |  4886 | `			}` |
|     15287 |  4887 | `		}` |
|         - |  4888 | `		/* Advance the stream cursor */` |
|   1474373 |  4889 | `		pCur++;` |
|         5 |  4890 | `	}` |
|    310191 |  4891 | `	if( pCur <= pGen->pIn ){` |
|       ! 0 |  4892 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  4893 | `			"foreach: Missing array/object expression");` |
|       ! 0 |  4894 | `		if( rc == SXERR_ABORT ){` |
|         - |  4895 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4896 | `			return SXERR_ABORT;` |
|         - |  4897 | `		}` |
|       ! 0 |  4898 | `		goto Synchronize;` |
|         - |  4899 | `	}` |
|         - |  4900 | `	/* Swap token streams */` |
|    310191 |  4901 | `	pTmp = pGen->pEnd;` |
|    310191 |  4902 | `	pGen->pEnd = pCur;` |
|    310191 |  4903 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    310191 |  4904 | `	if( rc == SXERR_ABORT ){` |
|         - |  4905 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4906 | `		return SXERR_ABORT;` |
|         - |  4907 | `	}` |
|         - |  4908 | `	/* Update token stream */` |
|    310191 |  4909 | `	while(pGen->pIn < pCur ){` |
|       ! 0 |  4910 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  4911 | `		if( rc == SXERR_ABORT ){` |
|         - |  4912 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4913 | `			return SXERR_ABORT;` |
|         - |  4914 | `		}` |
|       ! 0 |  4915 | `		pGen->pIn++;` |
|       ! 0 |  4916 | `	}` |
|    310191 |  4917 | `	pCur++; /* Jump the 'as' keyword */` |
|    310191 |  4918 | `	pGen->pIn = pCur;` |
|    310191 |  4919 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 |  4920 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|       ! 0 |  4921 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4922 | `			return SXERR_ABORT;` |
|         - |  4923 | `		}` |
|       ! 0 |  4924 | `	}` |
|         - |  4925 | `	/* Create the foreach context */` |
|    310191 |  4926 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    310191 |  4927 | `	if( pInfo == 0 ){` |
|       ! 0 |  4928 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 |  4929 | `		return SXERR_ABORT;` |
|         - |  4930 | `	}` |
|         - |  4931 | `	/* Zero the structure */` |
|    310191 |  4932 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|         - |  4933 | `	/* Initialize structure fields */` |
|    310191 |  4934 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|         - |  4935 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|         - |  4936 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|         - |  4937 | `	 * '=>'. */` |
|    310191 |  4938 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    310191 |  4939 | `	if( pCur < pEnd ){` |
|         - |  4940 | `		/* Compile the expression holding the key name */` |
|    134007 |  4941 | `		if( pGen->pIn >= pCur ){` |
|       ! 0 |  4942 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|       ! 0 |  4943 | `			if( rc == SXERR_ABORT ){` |
|         - |  4944 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4945 | `				return SXERR_ABORT;` |
|         - |  4946 | `			}` |
|       ! 0 |  4947 | `		}else{` |
|    134007 |  4948 | `			pGen->pEnd = pCur;` |
|    134007 |  4949 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    134007 |  4950 | `			if( rc == SXERR_ABORT ){` |
|         - |  4951 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4952 | `				return SXERR_ABORT;` |
|         - |  4953 | `			}` |
|    134007 |  4954 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    134007 |  4955 | `			if( pInstr->p3 ){` |
|         - |  4956 | `				/* Record key name */` |
|    134007 |  4957 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|     67001 |  4958 | `			}` |
|    134007 |  4959 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|         - |  4960 | `		}` |
|    134007 |  4961 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|     67001 |  4962 | `	}` |
|    310191 |  4963 | `	pGen->pEnd = pEnd;` |
|    310191 |  4964 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 |  4965 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|       ! 0 |  4966 | `		if( rc == SXERR_ABORT ){` |
|         - |  4967 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4968 | `			return SXERR_ABORT;` |
|         - |  4969 | `		}` |
|       ! 0 |  4970 | `		goto Synchronize;` |
|         - |  4971 | `	}` |
|    310191 |  4972 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|        33 |  4973 | `		pGen->pIn++;` |
|         - |  4974 | `		/* Pass by reference  */` |
|        33 |  4975 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|        15 |  4976 | `	}` |
|         - |  4977 | `	/* Check if the value target is list() */` |
|    310191 |  4978 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         8 |  4979 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|         - |  4980 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|         - |  4981 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|         - |  4982 | `		 */` |
|         - |  4983 | `		static int iForeachListCnt = 0;` |
|         - |  4984 | `		char zTmp[128];` |
|         - |  4985 | `		sxu32 nLen;` |
|         - |  4986 | `		char *zDup;` |
|        10 |  4987 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|        10 |  4988 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|        10 |  4989 | `		if( zDup == 0 ){` |
|       ! 0 |  4990 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  4991 | `			return SXERR_ABORT;` |
|         - |  4992 | `		}` |
|        10 |  4993 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|         - |  4994 | `		/* Save list() token boundaries */` |
|        10 |  4995 | `		pListStart = pGen->pIn;` |
|         - |  4996 | `		/* Advance past list(...) — validate parentheses */` |
|        10 |  4997 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|        10 |  4998 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         3 |  4999 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|         - |  5000 | `				"foreach: Expected '(' after 'list'");` |
|         3 |  5001 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5002 | `				return SXERR_ABORT;` |
|         - |  5003 | `			}` |
|         3 |  5004 | `			goto Synchronize;` |
|         - |  5005 | `		}` |
|         7 |  5006 | `		pGen->pIn++; /* Jump '(' */` |
|         7 |  5007 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|         7 |  5008 | `		if( pListEnd >= pEnd ){` |
|       ! 0 |  5009 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5010 | `				"foreach: Missing closing ')' after list");` |
|       ! 0 |  5011 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5012 | `				return SXERR_ABORT;` |
|         - |  5013 | `			}` |
|       ! 0 |  5014 | `			goto Synchronize;` |
|         - |  5015 | `		}` |
|         7 |  5016 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|         7 |  5017 | `		pListEnd = pGen->pIn;` |
|         7 |  5018 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    310186 |  5019 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|         - |  5020 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|         - |  5021 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|         - |  5022 | `		 */` |
|         - |  5023 | `		static int iForeachShortListCnt = 0;` |
|         - |  5024 | `		char zTmp[128];` |
|         - |  5025 | `		sxu32 nLen;` |
|         - |  5026 | `		char *zDup;` |
|        15 |  5027 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|        15 |  5028 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|        15 |  5029 | `		if( zDup == 0 ){` |
|       ! 0 |  5030 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  5031 | `			return SXERR_ABORT;` |
|         - |  5032 | `		}` |
|        15 |  5033 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|         - |  5034 | `		/* Save [...] token boundaries */` |
|        15 |  5035 | `		pListStart = pGen->pIn;` |
|         - |  5036 | `		/* Advance past [...] */` |
|        15 |  5037 | `		pGen->pIn++; /* Jump '[' */` |
|        15 |  5038 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|        15 |  5039 | `		if( pListEnd >= pEnd ){` |
|       ! 0 |  5040 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5041 | `				"foreach: Missing closing ']' after short list");` |
|       ! 0 |  5042 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5043 | `				return SXERR_ABORT;` |
|         - |  5044 | `			}` |
|       ! 0 |  5045 | `			goto Synchronize;` |
|         - |  5046 | `		}` |
|        15 |  5047 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|        15 |  5048 | `		pListEnd = pGen->pIn;` |
|        15 |  5049 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|         8 |  5050 | `	}else{` |
|         - |  5051 | `		/* Compile the expression holding the value name */` |
|    310169 |  5052 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    310169 |  5053 | `		if( rc == SXERR_ABORT ){` |
|         - |  5054 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  5055 | `			return SXERR_ABORT;` |
|         - |  5056 | `		}` |
|    310169 |  5057 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    310169 |  5058 | `		if( pInstr->p3 ){` |
|         - |  5059 | `			/* Record value name */` |
|    310169 |  5060 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    155082 |  5061 | `		}` |
|         - |  5062 | `	}` |
|         - |  5063 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    310189 |  5064 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|         - |  5065 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    310189 |  5066 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|         - |  5067 | `	/* Record the first instruction to execute */` |
|    310189 |  5068 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|         - |  5069 | `	/* Emit the FOREACH_STEP instruction */` |
|    310189 |  5070 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|         - |  5071 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    310189 |  5072 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|         - |  5073 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    310189 |  5074 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|         - |  5075 | `		SyToken *pSavedIn,*pSavedEnd;` |
|         - |  5076 | `		/* Load the temporary variable holding the current value onto the stack.` |
|         - |  5077 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|         - |  5078 | `		 */` |
|        21 |  5079 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|         - |  5080 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|         - |  5081 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|         - |  5082 | `		 * picks up the delimiter and the variable names inside.` |
|         - |  5083 | `		 */` |
|        21 |  5084 | `		pSavedIn = pGen->pIn;` |
|        21 |  5085 | `		pSavedEnd = pGen->pEnd;` |
|        21 |  5086 | `		pGen->pIn = pListStart;` |
|        21 |  5087 | `		pGen->pEnd = pListEnd;` |
|        21 |  5088 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|        15 |  5089 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|         8 |  5090 | `		}else{` |
|         7 |  5091 | `			rc = PH7_CompileList(&(*pGen),0);` |
|         - |  5092 | `		}` |
|        21 |  5093 | `		pGen->pIn = pSavedIn;` |
|        21 |  5094 | `		pGen->pEnd = pSavedEnd;` |
|        21 |  5095 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5096 | `			return SXERR_ABORT;` |
|         - |  5097 | `		}` |
|         - |  5098 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|        21 |  5099 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        10 |  5100 | `	}` |
|         - |  5101 | `	/* Compile the loop body */` |
|    310189 |  5102 | `	pGen->pIn = &pEnd[1];` |
|    310189 |  5103 | `	pGen->pEnd = pTmp;` |
|    310189 |  5104 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    310189 |  5105 | `	if( rc == SXERR_ABORT ){` |
|         - |  5106 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  5107 | `		return SXERR_ABORT;` |
|         - |  5108 | `	}` |
|         - |  5109 | `	/* Emit the unconditional jump to the start of the loop */` |
|    310189 |  5110 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|         - |  5111 | `	/* Fix all jumps now the destination is resolved */` |
|    310189 |  5112 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  5113 | `	/* Release the loop block */` |
|    310189 |  5114 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  5115 | `	/* Statement successfully compiled */` |
|    310189 |  5116 | `	return SXRET_OK;` |
|         1 |  5117 | `Synchronize:` |
|         - |  5118 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  5119 | `	 * compiling this erroneous block.` |
|         - |  5120 | `	 */` |
|         3 |  5121 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  5122 | `		pGen->pIn++;` |
|       ! 0 |  5123 | `	}` |
|         3 |  5124 | `	return SXRET_OK;` |
|    155098 |  5125 | `}` |
|         - |  5126 | `/*` |
|         - |  5127 | ` * Compile the infamous if/elseif/else if/else statements.` |
|         - |  5128 | ` * According to the PHP language reference` |
|         - |  5129 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|         - |  5130 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|         - |  5131 | ` *  that is similar to that of C:` |
|         - |  5132 | ` *  if (expr)` |
|         - |  5133 | ` *   statement` |
|         - |  5134 | ` *  else construct:` |
|         - |  5135 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|         - |  5136 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|         - |  5137 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|         - |  5138 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|         - |  5139 | ` *   $b, and a is NOT greater than b otherwise.` |
|         - |  5140 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|         - |  5141 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|         - |  5142 | ` *  elseif` |
|         - |  5143 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|         - |  5144 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|         - |  5145 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|         - |  5146 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|         - |  5147 | ` *   than b, a equal to b or a is smaller than b:` |
|         - |  5148 | ` *   <?php` |
|         - |  5149 | ` *    if ($a > $b) {` |
|         - |  5150 | ` *     echo "a is bigger than b";` |
|         - |  5151 | ` *    } elseif ($a == $b) {` |
|         - |  5152 | ` *     echo "a is equal to b";` |
|         - |  5153 | ` *    } else {` |
|         - |  5154 | ` *     echo "a is smaller than b";` |
|         - |  5155 | ` *    }` |
|         - |  5156 | ` *    ?>` |
|         - |  5157 | ` */` |
|   2217588 |  5158 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|         5 |  5159 | `{` |
|   2217593 |  5160 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|   2217593 |  5161 | `	GenBlock *pCondBlock = 0;` |
|         - |  5162 | `	sxu32 nJumpIdx;` |
|         - |  5163 | `	sxu32 nKeyID;` |
|         - |  5164 | `	sxi32 rc;` |
|         - |  5165 | `	/* Jump the 'if' keyword */` |
|   2217593 |  5166 | `	pGen->pIn++;` |
|   2217593 |  5167 | `	pToken = pGen->pIn;` |
|         - |  5168 | `	/* Create the conditional block */` |
|   2217593 |  5169 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|   2217593 |  5170 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  5171 | `		return SXERR_ABORT;` |
|         - |  5172 | `	}` |
|         - |  5173 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   1248285 |  5174 | `	for(;;){` |
|   2496575 |  5175 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  5176 | `			/* Syntax error */` |
|       ! 0 |  5177 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 |  5178 | `				pToken--;` |
|       ! 0 |  5179 | `			}` |
|       ! 0 |  5180 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|       ! 0 |  5181 | `			if( rc == SXERR_ABORT ){` |
|         - |  5182 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  5183 | `				return SXERR_ABORT;` |
|         - |  5184 | `			}` |
|       ! 0 |  5185 | `			goto Synchronize;` |
|         - |  5186 | `		}` |
|         - |  5187 | `		/* Jump the left parenthesis '(' */` |
|   2496575 |  5188 | `		pToken++;` |
|         - |  5189 | `		/* Delimit the condition */` |
|   2496575 |  5190 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   2496575 |  5191 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|         - |  5192 | `			/* Syntax error */` |
|        11 |  5193 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 |  5194 | `				pToken--;` |
|       ! 0 |  5195 | `			}` |
|        11 |  5196 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|        11 |  5197 | `			if( rc == SXERR_ABORT ){` |
|         - |  5198 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  5199 | `				return SXERR_ABORT;` |
|         - |  5200 | `			}` |
|        11 |  5201 | `			goto Synchronize;` |
|         - |  5202 | `		}` |
|         - |  5203 | `		/* Swap token streams */` |
|   2496567 |  5204 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|         - |  5205 | `		/* Compile the condition */` |
|   2496567 |  5206 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  5207 | `		/* Update token stream */` |
|   2496567 |  5208 | `		while(pGen->pIn < pEnd ){` |
|       ! 0 |  5209 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  5210 | `			pGen->pIn++;` |
|       ! 0 |  5211 | `		}` |
|   2496567 |  5212 | `		pGen->pIn  = &pEnd[1];` |
|   2496567 |  5213 | `		pGen->pEnd = pTmp;` |
|   2496567 |  5214 | `		if( rc == SXERR_ABORT ){` |
|         - |  5215 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  5216 | `			return SXERR_ABORT;` |
|         - |  5217 | `		}` |
|         - |  5218 | `		/* Emit the false jump */` |
|   2496567 |  5219 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|         - |  5220 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   2496567 |  5221 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|         - |  5222 | `		/* Compile the body */` |
|   2496567 |  5223 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   2496567 |  5224 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5225 | `			return SXERR_ABORT;` |
|         - |  5226 | `		}` |
|   2496567 |  5227 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|    493399 |  5228 | `			break;` |
|         - |  5229 | `		}` |
|         - |  5230 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   1509779 |  5231 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1509779 |  5232 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   1020243 |  5233 | `			break;` |
|         - |  5234 | `		}` |
|         - |  5235 | `		/* Emit the unconditional jump */` |
|    489541 |  5236 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|         - |  5237 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    489541 |  5238 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|    489541 |  5239 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|    294599 |  5240 | `			pToken = &pGen->pIn[1];` |
|    294599 |  5241 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|     84078 |  5242 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    105282 |  5243 | `					break;` |
|         - |  5244 | `			}` |
|     84045 |  5245 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|     42020 |  5246 | `		}` |
|    278987 |  5247 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|         - |  5248 | `		/* Synchronize cursors */` |
|    278987 |  5249 | `		pToken = pGen->pIn;` |
|         - |  5250 | `		/* Fix the false jump */` |
|    278987 |  5251 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|         5 |  5252 | `	} /* For(;;) */` |
|         - |  5253 | `	/* Fix the false jump */` |
|   2217585 |  5254 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|   2217585 |  5255 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   1230792 |  5256 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|         - |  5257 | `			/* Compile the else block */` |
|    210559 |  5258 | `			pGen->pIn++;` |
|    210559 |  5259 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    210559 |  5260 | `			if( rc == SXERR_ABORT ){` |
|         - |  5261 |  |
|       ! 0 |  5262 | `				return SXERR_ABORT;` |
|         - |  5263 | `			}` |
|    105277 |  5264 | `	}` |
|   2217585 |  5265 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|         - |  5266 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|   2217585 |  5267 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|         - |  5268 | `	/* Release the conditional block */` |
|   2217585 |  5269 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  5270 | `	/* Statement successfully compiled */` |
|   2217585 |  5271 | `	return SXRET_OK;` |
|         4 |  5272 | `Synchronize:` |
|         - |  5273 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|         - |  5274 | `	 */` |
|        67 |  5275 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        59 |  5276 | `		pGen->pIn++;` |
|         3 |  5277 | `	}` |
|        11 |  5278 | `	return SXRET_OK;` |
|   1108799 |  5279 | `}` |
|         - |  5280 | `/*` |
|         - |  5281 | ` * Compile the global construct.` |
|         - |  5282 | ` * According to the PHP language reference` |
|         - |  5283 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|         - |  5284 | ` *  to be used in that function.` |
|         - |  5285 | ` *  Example #1 Using global` |
|         - |  5286 | ` *  <?php` |
|         - |  5287 | ` *   $a = 1;` |
|         - |  5288 | ` *   $b = 2;` |
|         - |  5289 | ` *   function Sum()` |
|         - |  5290 | ` *   {` |
|         - |  5291 | ` *    global $a, $b;` |
|         - |  5292 | ` *    $b = $a + $b;` |
|         - |  5293 | ` *   }` |
|         - |  5294 | ` *   Sum();` |
|         - |  5295 | ` *   echo $b;` |
|         - |  5296 | ` *  ?>` |
|         - |  5297 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|         - |  5298 | ` *  all references to either variable will refer to the global version. There is no limit` |
|         - |  5299 | ` *  to the number of global variables that can be manipulated by a function.` |
|         - |  5300 | ` */` |
|        38 |  5301 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|         5 |  5302 | `{` |
|        43 |  5303 | `	SyToken *pTmp,*pNext = 0;` |
|         - |  5304 | `	sxi32 nExpr;` |
|         - |  5305 | `	sxi32 rc;` |
|         - |  5306 | `	/* Jump the 'global' keyword */` |
|        43 |  5307 | `	pGen->pIn++;` |
|        43 |  5308 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|         - |  5309 | `		/* Nothing to process */` |
|       ! 0 |  5310 | `		return SXRET_OK;` |
|         - |  5311 | `	}` |
|        43 |  5312 | `	pTmp = pGen->pEnd;` |
|        43 |  5313 | `	nExpr = 0;` |
|        91 |  5314 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|        53 |  5315 | `		if( pGen->pIn < pNext ){` |
|        53 |  5316 | `			pGen->pEnd = pNext;` |
|        53 |  5317 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  5318 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|       ! 0 |  5319 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  5320 | `					return SXERR_ABORT;` |
|         - |  5321 | `				}` |
|       ! 0 |  5322 | `			}else{` |
|        53 |  5323 | `				pGen->pIn++;` |
|        53 |  5324 | `				if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5325 | `					/* Emit a warning */` |
|       ! 0 |  5326 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|       ! 0 |  5327 | `				}else{` |
|        53 |  5328 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        53 |  5329 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  5330 | `						return SXERR_ABORT;` |
|        53 |  5331 | `					}else if(rc != SXERR_EMPTY ){` |
|        53 |  5332 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|        53 |  5333 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|         - |  5334 | `							/* Variable name, not a constant */` |
|        53 |  5335 | `							pLast->iP1 = 0;` |
|        24 |  5336 | `						}` |
|        53 |  5337 | `						nExpr++;` |
|        24 |  5338 | `					}` |
|         - |  5339 | `				}` |
|         - |  5340 | `			}` |
|        24 |  5341 | `		}` |
|         - |  5342 | `		/* Next expression in the stream */` |
|        53 |  5343 | `		pGen->pIn = pNext;` |
|         - |  5344 | `		/* Jump trailing commas */` |
|        63 |  5345 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        15 |  5346 | `			pGen->pIn++;` |
|         5 |  5347 | `		}` |
|         5 |  5348 | `	}` |
|         - |  5349 | `	/* Restore token stream */` |
|        43 |  5350 | `	pGen->pEnd = pTmp;` |
|        43 |  5351 | `	if( nExpr > 0 ){` |
|         - |  5352 | `		/* Emit the uplink instruction */` |
|        43 |  5353 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|        19 |  5354 | `	}` |
|        43 |  5355 | `	return SXRET_OK;` |
|        24 |  5356 | `}` |
|         - |  5357 | `/*` |
|         - |  5358 | ` * Compile the return statement.` |
|         - |  5359 | ` * According to the PHP language reference` |
|         - |  5360 | ` *  If called from within a function, the return() statement immediately ends execution` |
|         - |  5361 | ` *  of the current function, and returns its argument as the value of the function call.` |
|         - |  5362 | ` *  return() will also end the execution of an eval() statement or script file.` |
|         - |  5363 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|         - |  5364 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|         - |  5365 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|         - |  5366 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|         - |  5367 | ` *  from within the main script file, then script execution end.` |
|         - |  5368 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|         - |  5369 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|         - |  5370 | ` *  should do so as PHP has less work to do in this case.` |
|         - |  5371 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|         - |  5372 | ` */` |
|   3000968 |  5373 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|         5 |  5374 | `{` |
|   3000973 |  5375 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|         - |  5376 | `	sxi32 rc;` |
|   3000973 |  5377 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   3000973 |  5378 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|         - |  5379 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|         - |  5380 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|         - |  5381 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|         - |  5382 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|         - |  5383 | `	 * normally below so token processing stays consistent. */` |
|   7912747 |  5384 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|   4911779 |  5385 | `		pFuncBlock = pFuncBlock->pParent;` |
|         5 |  5386 | `	}` |
|   3000968 |  5387 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|   3000939 |  5388 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|         3 |  5389 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  5390 | `			"A never-returning function must not return");` |
|         3 |  5391 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5392 | `			return SXERR_ABORT;` |
|         - |  5393 | `		}` |
|         1 |  5394 | `	}` |
|         - |  5395 | `	/* Jump the 'return' keyword */` |
|   3000973 |  5396 | `	pGen->pIn++;` |
|   3000973 |  5397 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  5398 | `		/* Compile the expression */` |
|   2905443 |  5399 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   2905443 |  5400 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5401 | `			return SXERR_ABORT;` |
|   2905443 |  5402 | `		}else if(rc != SXERR_EMPTY ){` |
|   2905443 |  5403 | `			nRet = 1;` |
|   1452719 |  5404 | `		}` |
|   1452719 |  5405 | `	}` |
|         - |  5406 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|         - |  5407 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|         - |  5408 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|         - |  5409 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|   3000973 |  5410 | `	if( pGen->bInGenerator ){` |
|      3853 |  5411 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|      3853 |  5412 | `		return SXRET_OK;` |
|         - |  5413 | `	}` |
|         - |  5414 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|         - |  5415 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|         - |  5416 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|         - |  5417 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|         - |  5418 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|   2997125 |  5419 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|   2997125 |  5420 | `	return SXRET_OK;` |
|   1500489 |  5421 | `}` |
|         - |  5422 | `/*` |
|         - |  5423 | ` * Compile a yield expression.` |
|         - |  5424 | ` * Called from the expression code generator when a yield node is encountered.` |
|         - |  5425 | ` * Handles: yield, yield $value, yield $key => $value` |
|         - |  5426 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|         - |  5427 | ` */` |
|     15666 |  5428 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|         5 |  5429 | `{` |
|         - |  5430 | `	SyToken *pTmp, *pSplit;` |
|     15671 |  5431 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     15671 |  5432 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|         - |  5433 | `	sxi32 rc;` |
|      7833 |  5434 | `	(void)iCompileFlag;` |
|         - |  5435 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     15671 |  5436 | `	pGen->pIn++;` |
|         - |  5437 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|         - |  5438 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|         - |  5439 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|         - |  5440 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|         - |  5441 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     15666 |  5442 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|      7868 |  5443 | `		&& pGen->pIn->sData.nByte == 4` |
|        72 |  5444 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|        67 |  5445 | `		pGen->pIn++; /* Skip 'from' */` |
|        67 |  5446 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|        67 |  5447 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5448 | `			return SXERR_ABORT;` |
|         - |  5449 | `		}` |
|        67 |  5450 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 |  5451 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|       ! 0 |  5452 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|         - |  5453 | `				"Missing expression after 'yield from'");` |
|       ! 0 |  5454 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5455 | `				return SXERR_ABORT;` |
|         - |  5456 | `			}` |
|       ! 0 |  5457 | `		}` |
|        67 |  5458 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|        67 |  5459 | `		return SXRET_OK;` |
|         - |  5460 | `	}` |
|     15609 |  5461 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5462 | `		/* Bare yield — no value */` |
|         3 |  5463 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|         3 |  5464 | `		return SXRET_OK;` |
|         - |  5465 | `	}` |
|         - |  5466 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     15607 |  5467 | `	pSplit = 0;` |
|         - |  5468 | `	{` |
|     15607 |  5469 | `		SyToken *pCur = pGen->pIn;` |
|     15607 |  5470 | `		sxi32 nNest = 0;` |
|     46625 |  5471 | `		while( pCur < pGen->pEnd ){` |
|     46317 |  5472 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        18 |  5473 | `				nNest++;` |
|     46309 |  5474 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        18 |  5475 | `				nNest--;` |
|     46293 |  5476 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|     15299 |  5477 | `				pSplit = pCur;` |
|     15299 |  5478 | `				break;` |
|         - |  5479 | `			}` |
|     31023 |  5480 | `			pCur++;` |
|         5 |  5481 | `		}` |
|         - |  5482 | `	}` |
|     15607 |  5483 | `	pTmp = pGen->pEnd;` |
|     15607 |  5484 | `	if( pSplit ){` |
|         - |  5485 | `		/* yield $key => $value */` |
|     15299 |  5486 | `		pGen->pEnd = pSplit;` |
|     15299 |  5487 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     15299 |  5488 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     15299 |  5489 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|     15299 |  5490 | `		pGen->pEnd = pTmp;` |
|     15299 |  5491 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     15299 |  5492 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     15299 |  5493 | `		iP1 = 1;` |
|     15299 |  5494 | `		iP2 = 1;` |
|      7652 |  5495 | `	}else{` |
|         - |  5496 | `		/* yield $value */` |
|       313 |  5497 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       313 |  5498 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       313 |  5499 | `		if( rc != SXERR_EMPTY ){` |
|       313 |  5500 | `			iP1 = 1;` |
|       154 |  5501 | `		}` |
|         - |  5502 | `	}` |
|     15607 |  5503 | `	pGen->pEnd = pTmp;` |
|     15607 |  5504 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     15607 |  5505 | `	return SXRET_OK;` |
|      7838 |  5506 | `}` |
|         - |  5507 | `/*` |
|         - |  5508 | ` * Compile the die/exit language construct.` |
|         - |  5509 | ` * The role of these constructs is to terminate execution of the script.` |
|         - |  5510 | ` * Shutdown functions will always be executed even if exit() is called.` |
|         - |  5511 | ` */` |
|       128 |  5512 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|         5 |  5513 | `{` |
|       133 |  5514 | `	sxi32 nExpr = 0;` |
|         - |  5515 | `	sxi32 rc;` |
|         - |  5516 | `	/* Jump the die/exit keyword */` |
|       133 |  5517 | `	pGen->pIn++;` |
|       133 |  5518 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  5519 | `		/* Compile the expression */` |
|       133 |  5520 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       133 |  5521 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5522 | `			return SXERR_ABORT;` |
|       133 |  5523 | `		}else if(rc != SXERR_EMPTY ){` |
|       133 |  5524 | `			nExpr = 1;` |
|        64 |  5525 | `		}` |
|        64 |  5526 | `	}` |
|         - |  5527 | `	/* Emit the HALT instruction */` |
|       133 |  5528 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|       133 |  5529 | `	return SXRET_OK;` |
|        69 |  5530 | `}` |
|         - |  5531 | `/*` |
|         - |  5532 | ` * Compile the 'echo' language construct.` |
|         - |  5533 | ` */` |
|     17856 |  5534 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|         5 |  5535 | `{` |
|     17861 |  5536 | `	SyToken *pTmp,*pNext = 0;` |
|     17861 |  5537 | `	sxu32 nLine = pGen->pIn->nLine;` |
|     17861 |  5538 | `	int nExpr = 0;      /* expressions actually compiled */` |
|     17861 |  5539 | `	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */` |
|         - |  5540 | `	sxi32 rc;` |
|         - |  5541 | `	/* Jump the 'echo' keyword */` |
|     17861 |  5542 | `	pGen->pIn++;` |
|         - |  5543 | `	/* Compile arguments one after one */` |
|     17861 |  5544 | `	pTmp = pGen->pEnd;` |
|     45129 |  5545 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|     27275 |  5546 | `		if( pGen->pIn < pNext ){` |
|     27275 |  5547 | `			pGen->pEnd = pNext;` |
|     27275 |  5548 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|     27275 |  5549 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5550 | `				return SXERR_ABORT;` |
|     27275 |  5551 | `			}else if( rc != SXERR_EMPTY ){` |
|         - |  5552 | `				/* Emit the consume instruction */` |
|     27249 |  5553 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|     27249 |  5554 | `				nExpr++;` |
|     27249 |  5555 | `				bExpectMore = 0;` |
|     13622 |  5556 | `			}` |
|     13635 |  5557 | `		}` |
|         - |  5558 | `		/* Jump trailing commas (php: exactly one between expressions; a` |
|         - |  5559 | `		 * dangling or doubled comma is a parse error, enforced below) */` |
|     36695 |  5560 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|      9427 |  5561 | `			if( bExpectMore ){` |
|         - |  5562 | `				/* two commas in a row */` |
|         3 |  5563 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,` |
|         - |  5564 | `					"syntax error, unexpected token \",\"");` |
|         3 |  5565 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  5566 | `			}` |
|      9425 |  5567 | `			bExpectMore = 1;` |
|      9425 |  5568 | `			pNext++;` |
|         5 |  5569 | `		}` |
|     27273 |  5570 | `		pGen->pIn = pNext;` |
|         5 |  5571 | `	}` |
|         - |  5572 | `	/* Restore token stream */` |
|     17859 |  5573 | `	pGen->pEnd = pTmp;` |
|     17859 |  5574 | `	if( nExpr == 0 \|\| bExpectMore ){` |
|         - |  5575 | ``		/* `echo ;` or `echo expr, ;` — php rejects both */`` |
|        34 |  5576 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  5577 | `			"syntax error, unexpected token \";\"");` |
|        34 |  5578 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  5579 | `	}` |
|     17829 |  5580 | `	return SXRET_OK;` |
|      8933 |  5581 | `}` |
|         - |  5582 | `/*` |
|         - |  5583 | ` * Compile the static statement.` |
|         - |  5584 | ` * According to the PHP language reference` |
|         - |  5585 | ` *  Another important feature of variable scoping is the static variable.` |
|         - |  5586 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|         - |  5587 | ` *  when program execution leaves this scope.` |
|         - |  5588 | ` *  Static variables also provide one way to deal with recursive functions.` |
|         - |  5589 | ` * Symisc eXtension.` |
|         - |  5590 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|         - |  5591 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  5592 | ` *  Example` |
|         - |  5593 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|         - |  5594 | ` *    Refer to the official documentation for more information on this feature.` |
|         - |  5595 | ` */` |
|     11472 |  5596 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|         5 |  5597 | `{` |
|         - |  5598 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|         - |  5599 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|         - |  5600 | `	GenBlock *pBlock;` |
|         - |  5601 | `	SyString *pName;` |
|         - |  5602 | `	char *zDup;` |
|         - |  5603 | `	sxu32 nLine;` |
|         - |  5604 | `	sxi32 rc;` |
|         - |  5605 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|         - |  5606 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|         - |  5607 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|     11472 |  5608 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|      5742 |  5609 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|         1 |  5610 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|         3 |  5611 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         3 |  5612 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5613 | `			return SXERR_ABORT;` |
|         3 |  5614 | `		}else if( rc != SXERR_EMPTY ){` |
|         3 |  5615 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 |  5616 | `		}` |
|         3 |  5617 | `		return SXRET_OK;` |
|         - |  5618 | `	}` |
|         - |  5619 | `	/* Jump the static keyword */` |
|     11475 |  5620 | `	nLine = pGen->pIn->nLine;` |
|     11475 |  5621 | `	pGen->pIn++;` |
|         - |  5622 | `	/* Extract the enclosing function if any */` |
|     11475 |  5623 | `	pBlock = pGen->pCurrent;` |
|     22945 |  5624 | `	while( pBlock ){` |
|     22945 |  5625 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|     11475 |  5626 | `			break;` |
|         - |  5627 | `		}` |
|         - |  5628 | `		/* Point to the upper block */` |
|     11475 |  5629 | `		pBlock = pBlock->pParent;` |
|         5 |  5630 | `	}` |
|     11475 |  5631 | `	if( pBlock == 0 ){` |
|         - |  5632 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|       ! 0 |  5633 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  5634 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       ! 0 |  5635 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5636 | `				return SXERR_ABORT;` |
|         - |  5637 | `			}` |
|       ! 0 |  5638 | `			goto Synchronize;` |
|         - |  5639 | `		}` |
|         - |  5640 | `		/* Compile the expression holding the variable */` |
|       ! 0 |  5641 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       ! 0 |  5642 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5643 | `			return SXERR_ABORT;` |
|       ! 0 |  5644 | `		}else if( rc != SXERR_EMPTY ){` |
|         - |  5645 | `			/* Emit the POP instruction */` |
|       ! 0 |  5646 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       ! 0 |  5647 | `		}` |
|       ! 0 |  5648 | `		return SXRET_OK;` |
|         - |  5649 | `	}` |
|     11475 |  5650 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|         - |  5651 | `	/* Make sure we are dealing with a valid statement */` |
|     11475 |  5652 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|     11468 |  5653 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         3 |  5654 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|         3 |  5655 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5656 | `				return SXERR_ABORT;` |
|         - |  5657 | `			}` |
|         3 |  5658 | `			goto Synchronize;` |
|         - |  5659 | `	}` |
|     11473 |  5660 | `	pGen->pIn++;` |
|         - |  5661 | `	/* Extract variable name */` |
|     11473 |  5662 | `	pName = &pGen->pIn->sData;` |
|     11473 |  5663 | `	pGen->pIn++; /* Jump the var name */` |
|     11473 |  5664 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|       ! 0 |  5665 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  5666 | `		goto Synchronize;` |
|         - |  5667 | `	}` |
|         - |  5668 | `	/* Initialize the structure describing the static variable */` |
|     11473 |  5669 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     11473 |  5670 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|         - |  5671 | `	/* Duplicate variable name */` |
|     11473 |  5672 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     11473 |  5673 | `	if( zDup == 0 ){` |
|       ! 0 |  5674 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  5675 | `		return SXERR_ABORT;` |
|         - |  5676 | `	}` |
|     11473 |  5677 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|         - |  5678 | `	/* Check if we have an expression to compile */` |
|     11473 |  5679 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|         - |  5680 | `		SySet *pInstrContainer;` |
|         - |  5681 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|         - |  5682 | `		 * Static variable can take any complex expression including function` |
|         - |  5683 | `		 * call as their initialization value.` |
|         - |  5684 | `		 * Example:` |
|         - |  5685 | `		 *		static $var = foo(1,4+5,bar());` |
|         - |  5686 | `		 */` |
|     11473 |  5687 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|         - |  5688 | `		/* Swap bytecode container */` |
|     11473 |  5689 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     11473 |  5690 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|         - |  5691 | `		/* Compile the expression */` |
|     11473 |  5692 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  5693 | `		/* Emit the done instruction */` |
|     11473 |  5694 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|         - |  5695 | `		/* Restore default bytecode container */` |
|     11473 |  5696 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      5734 |  5697 | `	}` |
|         - |  5698 | `	/* Finally save the compiled static variable in the appropriate container */` |
|     11473 |  5699 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|     11473 |  5700 | `	return SXRET_OK;` |
|         1 |  5701 | `Synchronize:` |
|         - |  5702 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|         - |  5703 | `	 * statement.` |
|         - |  5704 | `	 */` |
|         5 |  5705 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|         3 |  5706 | `		pGen->pIn++;` |
|         1 |  5707 | `	}` |
|         3 |  5708 | `	return SXRET_OK;` |
|      5741 |  5709 | `}` |
|         - |  5710 | `/*` |
|         - |  5711 | ` * Compile the var statement.` |
|         - |  5712 | ` * Symisc Extension:` |
|         - |  5713 | ` *      var statement can be used outside of a class definition.` |
|         - |  5714 | ` */` |
|         4 |  5715 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|         1 |  5716 | `{` |
|         - |  5717 | `	sxu32 nLine;` |
|         - |  5718 | `	sxi32 rc;` |
|         5 |  5719 | `	nLine = pGen->pIn->nLine;` |
|         - |  5720 | `	/* Jump the 'var' keyword */` |
|         5 |  5721 | `	pGen->pIn++;` |
|         5 |  5722 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|       ! 0 |  5723 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|         - |  5724 | `		/* Synchronize with the first semi-colon */` |
|       ! 0 |  5725 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|       ! 0 |  5726 | `			pGen->pIn++;` |
|       ! 0 |  5727 | `		}` |
|       ! 0 |  5728 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5729 | `			return SXERR_ABORT;` |
|         - |  5730 | `		}` |
|       ! 0 |  5731 | `	}else{` |
|         - |  5732 | `		/* Compile the expression */` |
|         5 |  5733 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         5 |  5734 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5735 | `			return SXERR_ABORT;` |
|         5 |  5736 | `		}else if( rc != SXERR_EMPTY ){` |
|         5 |  5737 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 |  5738 | `		}` |
|         - |  5739 | `	}` |
|         5 |  5740 | `	return SXRET_OK;` |
|         3 |  5741 | `}` |
|         - |  5742 | `/*` |
|         - |  5743 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|         - |  5744 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|         - |  5745 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|         - |  5746 | ` */` |
|         - |  5747 | `/*` |
|         - |  5748 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|         - |  5749 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|         - |  5750 | ` * hash and any shared references), this creates a new literal entry with the` |
|         - |  5751 | ` * qualified name and updates the instruction's operand index.` |
|         - |  5752 | ` *` |
|         - |  5753 | ` * Resolution order:` |
|         - |  5754 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|         - |  5755 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|         - |  5756 | ` *   3. Otherwise return the original literal index unchanged.` |
|         - |  5757 | ` *` |
|         - |  5758 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|         - |  5759 | ` * came from an import (step 1) and 0 otherwise.` |
|         - |  5760 | ` * Returns the (possibly new) literal index.` |
|         - |  5761 | ` */` |
|   5618154 |  5762 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|         5 |  5763 | `{` |
|         - |  5764 | `	ph7_value *pLit;` |
|         - |  5765 | `	const char *zLit;` |
|         - |  5766 | `	SyString sQualified;` |
|         - |  5767 | `	sxu32 nLit;` |
|         - |  5768 | `	sxu32 k;` |
|         - |  5769 | `	sxu32 nNewIdx;` |
|         - |  5770 | `	int hasNsSep;` |
|         - |  5771 | `	SyHashEntry *pImport;` |
|         - |  5772 | `	ph7_value *pNew;` |
|   5618159 |  5773 | `	if( pFromImport ){` |
|   4544609 |  5774 | `		*pFromImport = 0;` |
|   2272302 |  5775 | `	}` |
|   5618159 |  5776 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|   5618159 |  5777 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|       ! 0 |  5778 | `		return nOrigIdx;` |
|         - |  5779 | `	}` |
|   5618159 |  5780 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|   5618159 |  5781 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|         - |  5782 | `	/* Skip if already qualified (contains backslash) */` |
|   5618159 |  5783 | `	hasNsSep = 0;` |
|  66466679 |  5784 | `	for( k = 0; k < nLit; k++ ){` |
|  60848543 |  5785 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
|  30424265 |  5786 | `	}` |
|   5618159 |  5787 | `	if( hasNsSep ){` |
|        20 |  5788 | `		return nOrigIdx;` |
|         - |  5789 | `	}` |
|         - |  5790 | `	/* Check use imports first (works even outside namespaces) */` |
|   5618141 |  5791 | `	SyBlobReset(&pGen->sWorker);` |
|   5618141 |  5792 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|   5618141 |  5793 | `	if( pImport ){` |
|        41 |  5794 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|        41 |  5795 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|        41 |  5796 | `		if( pFromImport ){` |
|        18 |  5797 | `			*pFromImport = 1;` |
|         8 |  5798 | `		}` |
|        23 |  5799 | `	}else{` |
|   5618105 |  5800 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|   5617975 |  5801 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|         - |  5802 | `		}` |
|         - |  5803 | `		/* Prepend current namespace */` |
|       135 |  5804 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       135 |  5805 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|       135 |  5806 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|         - |  5807 | `	}` |
|         - |  5808 | `	/* Look up or create a new literal for the qualified name */` |
|       171 |  5809 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|       171 |  5810 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|        77 |  5811 | `		return nNewIdx; /* Already interned */` |
|         - |  5812 | `	}` |
|        99 |  5813 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|        99 |  5814 | `	if( pNew == 0 ){` |
|       ! 0 |  5815 | `		return nOrigIdx; /* OOM, fall back to original */` |
|         - |  5816 | `	}` |
|        99 |  5817 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|        99 |  5818 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|        99 |  5819 | `	return nNewIdx;` |
|   2809082 |  5820 | `}` |
|         - |  5821 | `/*` |
|         - |  5822 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|         - |  5823 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|         - |  5824 | ` */` |
|    448730 |  5825 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|         5 |  5826 | `{` |
|         - |  5827 | `	SyHashEntry *pImport;` |
|         - |  5828 | `	/* Check use imports first */` |
|    448735 |  5829 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|    448735 |  5830 | `	if( pImport ){` |
|        21 |  5831 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|        21 |  5832 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|        21 |  5833 | `		return;` |
|         - |  5834 | `	}` |
|         - |  5835 | `	/* Prepend current namespace if active */` |
|    448717 |  5836 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        14 |  5837 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        14 |  5838 | `		SyBlobAppend(pOut,"\\",1);` |
|         6 |  5839 | `	}` |
|    448717 |  5840 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    224370 |  5841 | `}` |
|         - |  5842 | `/*` |
|         - |  5843 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|         - |  5844 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|         - |  5845 | ` * The caller must release pOut when done.` |
|         - |  5846 | ` */` |
|    429872 |  5847 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|         5 |  5848 | `{` |
|    429877 |  5849 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      3903 |  5850 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      3903 |  5851 | `		SyBlobAppend(pOut,"\\",1);` |
|      1949 |  5852 | `	}` |
|    429877 |  5853 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    429877 |  5854 | `}` |
|         - |  5855 | `/*` |
|         - |  5856 | ` * Compile a namespace statement` |
|         - |  5857 | ` * According to the PHP language reference manual` |
|         - |  5858 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|         - |  5859 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|         - |  5860 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|         - |  5861 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|         - |  5862 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|         - |  5863 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|         - |  5864 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|         - |  5865 | ` *  programming world.` |
|         - |  5866 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|         - |  5867 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|         - |  5868 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|         - |  5869 | ` *  classes/functions/constants.` |
|         - |  5870 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|         - |  5871 | ` *  readability of source code.` |
|         - |  5872 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|         - |  5873 | ` *  Here is an example of namespace syntax in PHP:` |
|         - |  5874 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|         - |  5875 | ` *       class MyClass {}` |
|         - |  5876 | ` *       function myfunction() {}` |
|         - |  5877 | ` *       const MYCONST = 1;` |
|         - |  5878 | ` *       $a = new MyClass;` |
|         - |  5879 | ` *       $c = new \my\name\MyClass;` |
|         - |  5880 | ` *       $a = strlen('hi');` |
|         - |  5881 | ` *       $d = namespace\MYCONST;` |
|         - |  5882 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|         - |  5883 | ` *       echo constant($d);` |
|         - |  5884 | ` * NOTE` |
|         - |  5885 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|         - |  5886 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|         - |  5887 | ` */` |
|         - |  5888 | `/*` |
|         - |  5889 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|         - |  5890 | ` */` |
|        14 |  5891 | `static const char * TokenTypeName(sxu32 nType)` |
|         4 |  5892 | `{` |
|        18 |  5893 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|        11 |  5894 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|        11 |  5895 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|        11 |  5896 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|        11 |  5897 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|        11 |  5898 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|         3 |  5899 | `	return "token";` |
|        11 |  5900 | `}` |
|      3944 |  5901 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|         5 |  5902 | `{` |
|         - |  5903 | `	sxu32 nLine;` |
|         - |  5904 | `	sxi32 rc;` |
|      3949 |  5905 | `	nLine = pGen->pIn->nLine;` |
|      3949 |  5906 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|         - |  5907 | `	/* Reset namespace and clear previous use imports */` |
|      3949 |  5908 | `	SyBlobReset(&pGen->sNamespace);` |
|      3949 |  5909 | `	SyHashRelease(&pGen->hUseImports);` |
|      3949 |  5910 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|      3949 |  5911 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|      3949 |  5912 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|      3949 |  5913 | `	SyHashRelease(&pGen->hUseConstImports);` |
|      3949 |  5914 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|      3949 |  5915 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5916 | `		/* Global namespace (bare "namespace;") */` |
|       ! 0 |  5917 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5918 | `		return SXRET_OK;` |
|         - |  5919 | `	}` |
|      3949 |  5920 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|         - |  5921 | `		/* namespace; — switch to global namespace */` |
|       ! 0 |  5922 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5923 | `		return SXRET_OK;` |
|         - |  5924 | `	}` |
|      3949 |  5925 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|         - |  5926 | `		/* namespace { } — global namespace block */` |
|         3 |  5927 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|         3 |  5928 | `		return SXRET_OK;` |
|         - |  5929 | `	}` |
|         - |  5930 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|      7959 |  5931 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      4017 |  5932 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|         - |  5933 | `			/* Append backslash separator */` |
|        40 |  5934 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        40 |  5935 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|        18 |  5936 | `			}` |
|        22 |  5937 | `		}else{` |
|         - |  5938 | `			/* Append identifier */` |
|      3981 |  5939 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|         - |  5940 | `		}` |
|      4017 |  5941 | `		pGen->pIn++;` |
|         5 |  5942 | `	}` |
|         - |  5943 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|         - |  5944 | `	 * at the correct program counter, not just the last one compiled. */` |
|         - |  5945 | `	{` |
|      3947 |  5946 | `		char *zNsDup = 0;` |
|      3947 |  5947 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      5915 |  5948 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      3940 |  5949 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      1970 |  5950 | `		}` |
|      3947 |  5951 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|         - |  5952 | `	}` |
|      3947 |  5953 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|         8 |  5954 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  5955 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|         4 |  5956 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         6 |  5957 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5958 | `			return SXERR_ABORT;` |
|         - |  5959 | `		}` |
|         2 |  5960 | `	}` |
|      3947 |  5961 | `	return SXRET_OK;` |
|      1977 |  5962 | `}` |
|         - |  5963 | `/*` |
|         - |  5964 | ` * Compile the 'use' statement` |
|         - |  5965 | ` * According to the PHP language reference manual` |
|         - |  5966 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|         - |  5967 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|         - |  5968 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|         - |  5969 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|         - |  5970 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|         - |  5971 | ` *  a function or constant is not supported.` |
|         - |  5972 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|         - |  5973 | ` * NOTE` |
|         - |  5974 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|         - |  5975 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|         - |  5976 | ` */` |
|        78 |  5977 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|         5 |  5978 | `{` |
|         - |  5979 | `	sxu32 nLine;` |
|         - |  5980 | `	sxi32 rc;` |
|         - |  5981 | `	SyBlob sPath;` |
|         - |  5982 | `	SyString sAlias;` |
|         - |  5983 | `	SyToken *pLast;` |
|         - |  5984 | `	char *zDup;` |
|         - |  5985 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|         - |  5986 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|         - |  5987 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|        83 |  5988 | `	nLine = pGen->pIn->nLine;` |
|        83 |  5989 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|         - |  5990 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|        83 |  5991 | `	iUseType = 0;` |
|        83 |  5992 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        30 |  5993 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|        30 |  5994 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|        16 |  5995 | `			iUseType = 1;` |
|        16 |  5996 | `			pGen->pIn++;` |
|        23 |  5997 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|        16 |  5998 | `			iUseType = 2;` |
|        16 |  5999 | `			pGen->pIn++;` |
|         7 |  6000 | `		}` |
|        14 |  6001 | `	}` |
|         - |  6002 | `	/* Select target hash tables based on import type */` |
|        83 |  6003 | `	switch( iUseType ){` |
|         7 |  6004 | `		case 1:` |
|        16 |  6005 | `			pGenHash = &pGen->hUseFuncImports;` |
|        16 |  6006 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|        16 |  6007 | `			break;` |
|         7 |  6008 | `		case 2:` |
|        16 |  6009 | `			pGenHash = &pGen->hUseConstImports;` |
|        16 |  6010 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|        16 |  6011 | `			break;` |
|        25 |  6012 | `		default:` |
|        55 |  6013 | `			pGenHash = &pGen->hUseImports;` |
|        55 |  6014 | `			pVmHash = &pGen->pVm->hUseImports;` |
|        50 |  6015 | `			break;` |
|         - |  6016 | `	}` |
|        83 |  6017 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|         - |  6018 | `	/* Process one or more use declarations separated by commas */` |
|        40 |  6019 | `	for(;;){` |
|        85 |  6020 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  6021 | `			break;` |
|         - |  6022 | `		}` |
|        85 |  6023 | `		SyBlobReset(&sPath);` |
|        85 |  6024 | `		pLast = 0;` |
|         - |  6025 | `		/* Collect the full namespace path */` |
|       293 |  6026 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|       213 |  6027 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       147 |  6028 | `				pLast = pGen->pIn;` |
|       147 |  6029 | `				if( SyBlobLength(&sPath) > 0 ){` |
|        71 |  6030 | `					SyBlobAppend(&sPath,"\\",1);` |
|        33 |  6031 | `				}` |
|       147 |  6032 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        71 |  6033 | `			}` |
|       213 |  6034 | `			pGen->pIn++;` |
|         5 |  6035 | `		}` |
|        85 |  6036 | `		if( pLast == 0 ){` |
|         - |  6037 | `			/* Empty path */` |
|         6 |  6038 | `			break;` |
|         - |  6039 | `		}` |
|         - |  6040 | `		/* Default alias is the last component of the path */` |
|        81 |  6041 | `		sAlias = pLast->sData;` |
|         - |  6042 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|        76 |  6043 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|        55 |  6044 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|        27 |  6045 | `			pGen->pIn++; /* Jump 'as' */` |
|        27 |  6046 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|        27 |  6047 | `				sAlias = pGen->pIn->sData;` |
|        27 |  6048 | `				pGen->pIn++;` |
|        12 |  6049 | `			}` |
|        12 |  6050 | `		}` |
|         - |  6051 | `		/* Check for duplicate import alias (per-type) */` |
|        81 |  6052 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|         8 |  6053 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  6054 | `				"Cannot use %.*s as %z because the name is already in use",` |
|         4 |  6055 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|         6 |  6056 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  6057 | `				SyBlobRelease(&sPath);` |
|       ! 0 |  6058 | `				return SXERR_ABORT;` |
|         - |  6059 | `			}` |
|         2 |  6060 | `		}` |
|         - |  6061 | `		/* Register the import: alias -> FQN.` |
|         - |  6062 | `		 * Strings are allocated from the VM pool allocator and freed` |
|         - |  6063 | `		 * when the entire VM is released. SyHashRelease does not free` |
|         - |  6064 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|       119 |  6065 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        76 |  6066 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        81 |  6067 | `		if( zDup ){` |
|        81 |  6068 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|        81 |  6069 | `			if( pVmHash ){` |
|         - |  6070 | `				/* Class imports: populate VM table directly (class resolution` |
|         - |  6071 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|        53 |  6072 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        53 |  6073 | `				if( zAliasDup ){` |
|        53 |  6074 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|        24 |  6075 | `				}` |
|        24 |  6076 | `			}` |
|        81 |  6077 | `			if( iUseType == 2 ){` |
|         - |  6078 | `				/* Const imports: emit a runtime instruction so imports are` |
|         - |  6079 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|        16 |  6080 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        16 |  6081 | `				if( zAliasDup ){` |
|         - |  6082 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|         - |  6083 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|         - |  6084 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|        16 |  6085 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|        16 |  6086 | `					if( azPair ){` |
|        16 |  6087 | `						azPair[0] = zAliasDup;` |
|        16 |  6088 | `						azPair[1] = zDup;` |
|        16 |  6089 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|         7 |  6090 | `					}` |
|         7 |  6091 | `				}` |
|         7 |  6092 | `			}` |
|        38 |  6093 | `		}` |
|         - |  6094 | `		/* Check for comma (multiple use declarations) */` |
|        81 |  6095 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|         3 |  6096 | `			pGen->pIn++;` |
|         2 |  6097 | `		}else{` |
|        42 |  6098 | `			break;` |
|         - |  6099 | `		}` |
|         1 |  6100 | `	}` |
|        83 |  6101 | `	SyBlobRelease(&sPath);` |
|        83 |  6102 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         4 |  6103 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|         2 |  6104 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         3 |  6105 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6106 | `			return SXERR_ABORT;` |
|         - |  6107 | `		}` |
|         1 |  6108 | `	}` |
|        83 |  6109 | `	return SXRET_OK;` |
|        44 |  6110 | `}` |
|         - |  6111 | `/*` |
|         - |  6112 | ` * Compile the stupid 'declare' language construct.` |
|         - |  6113 | ` *` |
|         - |  6114 | ` * According to the PHP language reference manual.` |
|         - |  6115 | ` *  The declare construct is used to set execution directives for a block of code.` |
|         - |  6116 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|         - |  6117 | ` *  declare (directive)` |
|         - |  6118 | ` *   statement` |
|         - |  6119 | ` * The directive section allows the behavior of the declare block to be set.` |
|         - |  6120 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|         - |  6121 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|         - |  6122 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|         - |  6123 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|         - |  6124 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|         - |  6125 | ` * <?php` |
|         - |  6126 | ` * // these are the same:` |
|         - |  6127 | ` * // you can use this:` |
|         - |  6128 | ` * declare(ticks=1) {` |
|         - |  6129 | ` *   // entire script here` |
|         - |  6130 | ` * }` |
|         - |  6131 | ` * // or you can use this:` |
|         - |  6132 | ` * declare(ticks=1);` |
|         - |  6133 | ` * // entire script here` |
|         - |  6134 | ` * ?>` |
|         - |  6135 | ` *` |
|         - |  6136 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|         - |  6137 | ` */` |
|         - |  6138 | `/*` |
|         - |  6139 | ` * Match a directive name against a known literal (case-insensitive).` |
|         - |  6140 | ` */` |
|        72 |  6141 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|         5 |  6142 | `{` |
|       109 |  6143 | `	return SyStringLength(pName) == nWant` |
|        72 |  6144 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|         5 |  6145 | `}` |
|         - |  6146 |  |
|        42 |  6147 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|         5 |  6148 | `{` |
|        47 |  6149 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        47 |  6150 | `	SyToken *pBodyEnd = 0;` |
|         - |  6151 | `	SyToken *pBodyStart;` |
|         - |  6152 | `	SyToken *pCursor;` |
|         - |  6153 | `	int bHasStrictTypes;` |
|         - |  6154 | `	int bBlockForm;` |
|         - |  6155 | `	int bPlacementOk;` |
|         - |  6156 | `	sxi32 rc;` |
|        47 |  6157 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|        47 |  6158 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|         6 |  6159 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         6 |  6160 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6161 | `			return SXERR_ABORT;` |
|         - |  6162 | `		}` |
|         6 |  6163 | `		goto Synchro;` |
|         - |  6164 | `	}` |
|        43 |  6165 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|        43 |  6166 | `	pBodyStart = pGen->pIn;` |
|         - |  6167 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|        43 |  6168 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|        43 |  6169 | `	if( pBodyEnd >= pGen->pEnd ){` |
|       ! 0 |  6170 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|       ! 0 |  6171 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6172 | `			return SXERR_ABORT;` |
|         - |  6173 | `		}` |
|       ! 0 |  6174 | `		return SXRET_OK;` |
|         - |  6175 | `	}` |
|         - |  6176 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|         - |  6177 | `	 * now delimits the comma-separated directive list. */` |
|        43 |  6178 | `	pGen->pIn = &pBodyEnd[1];` |
|        43 |  6179 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       ! 0 |  6180 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|       ! 0 |  6181 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6182 | `			return SXERR_ABORT;` |
|         - |  6183 | `		}` |
|       ! 0 |  6184 | `	}` |
|        43 |  6185 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|        43 |  6186 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|        43 |  6187 | `	bHasStrictTypes = 0;` |
|         - |  6188 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|         - |  6189 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|         - |  6190 | `	 * directive appears anywhere in the list, before validating values. */` |
|        43 |  6191 | `	pCursor = pBodyStart;` |
|        55 |  6192 | `	while( pCursor < pBodyEnd ){` |
|        51 |  6193 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|        43 |  6194 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|        39 |  6195 | `				bHasStrictTypes = 1;` |
|        39 |  6196 | `				break;` |
|         - |  6197 | `			}` |
|         2 |  6198 | `		}` |
|        14 |  6199 | `		pCursor++;` |
|         2 |  6200 | `	}` |
|        43 |  6201 | `	if( bHasStrictTypes && bBlockForm ){` |
|         3 |  6202 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6203 | `			"strict_types declaration must not use block mode");` |
|         3 |  6204 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         3 |  6205 | `		return SXRET_OK;` |
|         - |  6206 | `	}` |
|        41 |  6207 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|         6 |  6208 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6209 | `			"strict_types declaration must be the very first statement in the script");` |
|         6 |  6210 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         6 |  6211 | `		return SXRET_OK;` |
|         - |  6212 | `	}` |
|         - |  6213 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|        37 |  6214 | `	pCursor = pBodyStart;` |
|        69 |  6215 | `	while( pCursor < pBodyEnd ){` |
|         - |  6216 | `		SyToken *pNameTok;` |
|         - |  6217 | `		SyToken *pEqTok;` |
|         - |  6218 | `		SyToken *pValTok;` |
|         - |  6219 | `		SyString *pDirName;` |
|         - |  6220 | `		int bIsStrict;` |
|         - |  6221 | `		int iStrictValue;` |
|        39 |  6222 | `		pNameTok = pCursor;` |
|        39 |  6223 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  6224 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6225 | `				"declare: Expecting a directive name");` |
|       ! 0 |  6226 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6227 | `			return SXRET_OK;` |
|         - |  6228 | `		}` |
|        39 |  6229 | `		pEqTok = pNameTok + 1;` |
|        39 |  6230 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|       ! 0 |  6231 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6232 | `				"declare: Expecting '=' after directive name");` |
|       ! 0 |  6233 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6234 | `			return SXRET_OK;` |
|         - |  6235 | `		}` |
|        39 |  6236 | `		pValTok = pEqTok + 1;` |
|        39 |  6237 | `		if( pValTok >= pBodyEnd ){` |
|       ! 0 |  6238 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6239 | `				"declare: Expecting value after '='");` |
|       ! 0 |  6240 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6241 | `			return SXRET_OK;` |
|         - |  6242 | `		}` |
|        39 |  6243 | `		pDirName = &pNameTok->sData;` |
|        39 |  6244 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|        39 |  6245 | `		if( bIsStrict ){` |
|         - |  6246 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|         - |  6247 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|        35 |  6248 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       ! 0 |  6249 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6250 | `					"declare(strict_types) value must be a literal");` |
|       ! 0 |  6251 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6252 | `				return SXRET_OK;` |
|         - |  6253 | `			}` |
|        35 |  6254 | `			iStrictValue = -1;` |
|        35 |  6255 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|        35 |  6256 | `				const char *zv = SyStringData(&pValTok->sData);` |
|        35 |  6257 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|        35 |  6258 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|        33 |  6259 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|        15 |  6260 | `			}` |
|        35 |  6261 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|         3 |  6262 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6263 | `					"strict_types declaration must have 0 or 1 as its value");` |
|         3 |  6264 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         3 |  6265 | `				return SXRET_OK;` |
|         - |  6266 | `			}` |
|        32 |  6267 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|        18 |  6268 | `		}else{` |
|         - |  6269 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|         - |  6270 | `			 * preserve the legacy notice so callers relying on the old` |
|         - |  6271 | `			 * behavior don't regress. */` |
|         8 |  6272 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|         - |  6273 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|         2 |  6274 | `				ph7_lib_version()` |
|         - |  6275 | `				);` |
|         - |  6276 | `		}` |
|        37 |  6277 | `		pCursor = pValTok + 1;` |
|         - |  6278 | `		/* Consume separating comma (or end). */` |
|        37 |  6279 | `		if( pCursor < pBodyEnd ){` |
|         3 |  6280 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  6281 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6282 | `					"declare: Expecting ',' or ')' after directive value");` |
|       ! 0 |  6283 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6284 | `				return SXRET_OK;` |
|         - |  6285 | `			}` |
|         3 |  6286 | `			pCursor++;` |
|         1 |  6287 | `		}` |
|         5 |  6288 | `	}` |
|         - |  6289 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|         - |  6290 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|         - |  6291 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|        35 |  6292 | `	return SXRET_OK;` |
|         2 |  6293 | `Synchro:` |
|         - |  6294 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|        16 |  6295 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|        12 |  6296 | `		pGen->pIn++;` |
|         2 |  6297 | `	}` |
|         6 |  6298 | `	return SXRET_OK;` |
|        26 |  6299 | `}` |
|         - |  6300 | `/*` |
|         - |  6301 | ` * Process default argument values. That is,a function may define C++-style default value` |
|         - |  6302 | ` * as follows:` |
|         - |  6303 | ` * function makecoffee($type = "cappuccino")` |
|         - |  6304 | ` * {` |
|         - |  6305 | ` *   return "Making a cup of $type.\n";` |
|         - |  6306 | ` * }` |
|         - |  6307 | ` * Symisc eXtension.` |
|         - |  6308 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|         - |  6309 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|         - |  6310 | ` *      Example: Work only with PH7,generate error under zend` |
|         - |  6311 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|         - |  6312 | ` *      {` |
|         - |  6313 | ` *       var_dump($a);` |
|         - |  6314 | ` *      }` |
|         - |  6315 | ` *     //call test without args` |
|         - |  6316 | ` *      test();` |
|         - |  6317 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|         - |  6318 | ` *      Example:` |
|         - |  6319 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|         - |  6320 | ` * 3 -) Function overloading!!` |
|         - |  6321 | ` *      Example:` |
|         - |  6322 | ` *      function foo($a) {` |
|         - |  6323 | ` *   	  return $a.PHP_EOL;` |
|         - |  6324 | ` *	    }` |
|         - |  6325 | ` *	    function foo($a, $b) {` |
|         - |  6326 | ` *   	  return $a + $b;` |
|         - |  6327 | ` *	    }` |
|         - |  6328 | ` *	    echo foo(5); // Prints "5"` |
|         - |  6329 | ` *	    echo foo(5, 2); // Prints "7"` |
|         - |  6330 | ` *      // Same arg` |
|         - |  6331 | ` *	   function foo(string $a)` |
|         - |  6332 | ` *	   {` |
|         - |  6333 | ` *	     echo "a is a string\n";` |
|         - |  6334 | ` *	     var_dump($a);` |
|         - |  6335 | ` *	   }` |
|         - |  6336 | ` *	  function foo(int $a)` |
|         - |  6337 | ` *	  {` |
|         - |  6338 | ` *	    echo "a is integer\n";` |
|         - |  6339 | ` *	    var_dump($a);` |
|         - |  6340 | ` *	  }` |
|         - |  6341 | ` *	  function foo(array $a)` |
|         - |  6342 | ` *	  {` |
|         - |  6343 | ` * 	    echo "a is an array\n";` |
|         - |  6344 | ` * 	    var_dump($a);` |
|         - |  6345 | ` *	  }` |
|         - |  6346 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|         - |  6347 | ` *	  foo(52); // a is integer [second foo]` |
|         - |  6348 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|         - |  6349 | ` * Please refer to the official documentation for more information on the powerful extension` |
|         - |  6350 | ` * introduced by the PH7 engine.` |
|         - |  6351 | ` */` |
|    565522 |  6352 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|         5 |  6353 | `{` |
|         - |  6354 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  6355 | `	SySet *pInstrContainer;` |
|         - |  6356 | `	sxi32 rc;` |
|         - |  6357 | `	/* Swap token stream */` |
|    565527 |  6358 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|    565527 |  6359 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    565527 |  6360 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|         - |  6361 | `	/* Compile the expression holding the argument value */` |
|    565527 |  6362 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  6363 | `	/* Emit the done instruction */` |
|    565527 |  6364 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|    565527 |  6365 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    565527 |  6366 | `	RE_SWAP_DELIMITER(pGen);` |
|    565527 |  6367 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  6368 | `		return SXERR_ABORT;` |
|         - |  6369 | `	}` |
|    565527 |  6370 | `	return SXRET_OK;` |
|    282766 |  6371 | `}` |
|         - |  6372 | `/*` |
|         - |  6373 | ` * Collect function arguments one after one.` |
|         - |  6374 | ` * According to the PHP language reference manual.` |
|         - |  6375 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|         - |  6376 | ` * list of expressions.` |
|         - |  6377 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|         - |  6378 | ` * and default argument values. Variable-length argument lists are also supported,` |
|         - |  6379 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|         - |  6380 | ` * for more information.` |
|         - |  6381 | ` * Example #1 Passing arrays to functions` |
|         - |  6382 | ` * <?php` |
|         - |  6383 | ` * function takes_array($input)` |
|         - |  6384 | ` * {` |
|         - |  6385 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|         - |  6386 | ` * }` |
|         - |  6387 | ` * ?>` |
|         - |  6388 | ` * Making arguments be passed by reference` |
|         - |  6389 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|         - |  6390 | ` * within the function is changed, it does not get changed outside of the function).` |
|         - |  6391 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|         - |  6392 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|         - |  6393 | ` * to the argument name in the function definition:` |
|         - |  6394 | ` * Example #2 Passing function parameters by reference` |
|         - |  6395 | ` * <?php` |
|         - |  6396 | ` * function add_some_extra(&$string)` |
|         - |  6397 | ` * {` |
|         - |  6398 | ` *   $string .= 'and something extra.';` |
|         - |  6399 | ` * }` |
|         - |  6400 | ` * $str = 'This is a string, ';` |
|         - |  6401 | ` * add_some_extra($str);` |
|         - |  6402 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|         - |  6403 | ` * ?>` |
|         - |  6404 | ` *` |
|         - |  6405 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|         - |  6406 | ` * complex agrument values.Please refer to the official documentation for more information` |
|         - |  6407 | ` * on these extension.` |
|         - |  6408 | ` */` |
|   1289314 |  6409 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|         5 |  6410 | `{` |
|         - |  6411 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|         - |  6412 | `	SyToken *pIn;  /* Token stream */` |
|         - |  6413 | `	SyBlob sSig;         /* Function signature */` |
|         - |  6414 | `	char *zDup;          /* Copy of argument name */` |
|         - |  6415 | `	sxi32 rc;` |
|         - |  6416 |  |
|   1289319 |  6417 | `	pIn = pGen->pIn;` |
|   1289319 |  6418 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|         - |  6419 | `	/* Process arguments one after one */` |
|   1671808 |  6420 | `	for(;;){` |
|   3343621 |  6421 | `		if( pIn >= pEnd ){` |
|         - |  6422 | `			/* No more arguments to process */` |
|   1289303 |  6423 | `			break;` |
|         - |  6424 | `		}` |
|   2054323 |  6425 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   2054323 |  6426 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   2054323 |  6427 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   2054323 |  6428 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|   2054323 |  6429 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|         - |  6430 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|         - |  6431 | `		 * first token inside the main token stream */` |
|   2054323 |  6432 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  6433 | `			return SXERR_ABORT;` |
|         - |  6434 | `		}` |
|         - |  6435 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|         - |  6436 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|         - |  6437 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|         - |  6438 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|         - |  6439 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|         - |  6440 | `		{` |
|   2054323 |  6441 | `			int bReadonly = 0, bVisSeen = 0;` |
|   2054323 |  6442 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|   2054323 |  6443 | `			sxi32 iSetVisFlag = 0;` |
|         - |  6444 | `			int nSetTok;` |
|         - |  6445 | `			sxi32 nSetVis;` |
|   2054323 |  6446 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|         3 |  6447 | `				bReadonly = 1;` |
|         3 |  6448 | `				pIn++;` |
|         1 |  6449 | `			}` |
|   2054323 |  6450 | `			nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|   2054323 |  6451 | `			if( nSetVis ){` |
|         - |  6452 | ``				/* Leading `private(set)` etc: promoted with a public read side */`` |
|         3 |  6453 | `				iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  6454 | `				bVisSeen = 1;` |
|         3 |  6455 | `				pIn += nSetTok;` |
|         3 |  6456 | `				if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       ! 0 |  6457 | `					bReadonly = 1;` |
|       ! 0 |  6458 | `					pIn++;` |
|         1 |  6459 | `				}` |
|   2054322 |  6460 | `			}else if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|     88269 |  6461 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|     88269 |  6462 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|        89 |  6463 | `					bVisSeen = 1;` |
|        89 |  6464 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|       120 |  6465 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|        39 |  6466 | `						: PH7_CLASS_PROT_PUBLIC;` |
|        89 |  6467 | `					pIn++;` |
|        89 |  6468 | `					nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|        89 |  6469 | `					if( nSetVis ){` |
|         - |  6470 | ``						/* `public private(set) T $x` promoted form */`` |
|         3 |  6471 | `						iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  6472 | `						pIn += nSetTok;` |
|         1 |  6473 | `					}` |
|        89 |  6474 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|        18 |  6475 | `						bReadonly = 1;` |
|        18 |  6476 | `						pIn++;` |
|         7 |  6477 | `					}` |
|        42 |  6478 | `				}` |
|     44132 |  6479 | `			}` |
|   2054323 |  6480 | `			if( iSetVisFlag == PH7_CLASS_ATTR_PRIVATE_SET ){` |
|         5 |  6481 | `				sArg.iFlags \|= VM_FUNC_ARG_PRIV_SET;` |
|   2054321 |  6482 | `			}else if( iSetVisFlag == PH7_CLASS_ATTR_PROTECTED_SET ){` |
|       ! 0 |  6483 | `				sArg.iFlags \|= VM_FUNC_ARG_PROT_SET;` |
|       ! 0 |  6484 | `			}` |
|   2054323 |  6485 | `			if( bVisSeen \|\| bReadonly ){` |
|        93 |  6486 | `				if( !bCtorCtx ){` |
|         6 |  6487 | `					if( bAbstractCtx ){` |
|         3 |  6488 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  6489 | `							"Cannot declare promoted property in an abstract constructor");` |
|         2 |  6490 | `					}else{` |
|         3 |  6491 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  6492 | `							"Cannot declare promoted property outside a constructor");` |
|         - |  6493 | `					}` |
|         6 |  6494 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  6495 | `						return SXERR_ABORT;` |
|         - |  6496 | `					}` |
|         6 |  6497 | `					return SXERR_SYNTAX;` |
|         - |  6498 | `				}` |
|        89 |  6499 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|        89 |  6500 | `				sArg.iPromoteVis = iVis;` |
|        89 |  6501 | `				if( bReadonly ){` |
|        20 |  6502 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|         8 |  6503 | `				}` |
|        42 |  6504 | `			}` |
|         - |  6505 | `		}` |
|         - |  6506 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|   2054314 |  6507 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   1107700 |  6508 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|    149614 |  6509 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|    115135 |  6510 | `			sxu32 nLineLocal = pIn->nLine;` |
|    115135 |  6511 | `			sxi32 iTFlags = 0;` |
|    115135 |  6512 | `			pGen->pIn = pIn;` |
|    115135 |  6513 | `			rc = GenStateParseUnionTypeDecl(` |
|     57565 |  6514 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|     57565 |  6515 | `				&iTFlags, &sArg.sTypeName,` |
|         - |  6516 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|         - |  6517 | `				/* bAllowVoid */ 0,` |
|     57565 |  6518 | `						nLineLocal);` |
|    115135 |  6519 | `			pIn = pGen->pIn;` |
|    115135 |  6520 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  6521 | `				return SXERR_ABORT;` |
|    115135 |  6522 | `			}else if( rc == SXERR_CORRUPT ){` |
|         - |  6523 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|         3 |  6524 | `				return SXERR_SYNTAX;` |
|    115133 |  6525 | `			}else if( rc == SXERR_SYNTAX ){` |
|        11 |  6526 | `				if( pIn < pEnd ){` |
|        15 |  6527 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|         - |  6528 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|         4 |  6529 | `						&pIn->sData);` |
|         7 |  6530 | `				}else{` |
|       ! 0 |  6531 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|         - |  6532 | `						"syntax error, unexpected end of file");` |
|         - |  6533 | `				}` |
|        11 |  6534 | `				return SXERR_SYNTAX;` |
|         - |  6535 | `			}` |
|    115125 |  6536 | `			sArg.iFlags \|= iTFlags;` |
|     57560 |  6537 | `		}` |
|   2054309 |  6538 | `		if( pIn >= pEnd ){` |
|       ! 0 |  6539 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|       ! 0 |  6540 | `			return rc;` |
|         - |  6541 | `		}` |
|   2054309 |  6542 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|         - |  6543 | `			/* Pass by reference,record that */` |
|     22967 |  6544 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|     22967 |  6545 | `			pIn++;` |
|     11481 |  6546 | `		}` |
|   2054309 |  6547 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|         - |  6548 | `			/* Variadic parameter: ...$args */` |
|     23029 |  6549 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|     23029 |  6550 | `			pIn++;` |
|     11512 |  6551 | `		}` |
|   2054309 |  6552 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  6553 | `			/* Invalid argument */` |
|       ! 0 |  6554 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|       ! 0 |  6555 | `			return rc;` |
|         - |  6556 | `		}` |
|   2054309 |  6557 | `		pIn++; /* Jump the dollar sign */` |
|         - |  6558 | `		/* Copy argument name */` |
|   2054309 |  6559 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   2054309 |  6560 | `		if( zDup == 0 ){` |
|       ! 0 |  6561 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  6562 | `			return SXERR_ABORT;` |
|         - |  6563 | `		}` |
|   2054309 |  6564 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   2054309 |  6565 | `		pIn++;` |
|   2054309 |  6566 | `		if( pIn < pEnd ){` |
|   1143305 |  6567 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|         - |  6568 | `				SyToken *pDefend;` |
|    565529 |  6569 | `				sxi32 iNest = 0;` |
|    565529 |  6570 | `				pIn++; /* Jump the equal sign */` |
|    565529 |  6571 | `				pDefend = pIn;` |
|         - |  6572 | `				/* Process the default value associated with this argument */` |
|   1188387 |  6573 | `				while( pDefend < pEnd ){` |
|    810097 |  6574 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|    187239 |  6575 | `						break;` |
|         - |  6576 | `					}` |
|    622863 |  6577 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|         - |  6578 | `						/* Increment nesting level */` |
|     26757 |  6579 | `						iNest++;` |
|    609487 |  6580 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|         - |  6581 | `						/* Decrement nesting level */` |
|     26757 |  6582 | `						iNest--;` |
|     13376 |  6583 | `					}` |
|    622863 |  6584 | `					pDefend++;` |
|         5 |  6585 | `				}` |
|    565529 |  6586 | `				if( pIn >= pDefend ){` |
|         3 |  6587 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|         3 |  6588 | `					return rc;` |
|         - |  6589 | `				}` |
|         - |  6590 | `				/* Process default value */` |
|    565527 |  6591 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|    565527 |  6592 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  6593 | `					return rc;` |
|         - |  6594 | `				}` |
|         - |  6595 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|         - |  6596 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|         - |  6597 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|         - |  6598 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|         - |  6599 | `				 * arg-type check lets null through. */` |
|    565522 |  6600 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|    307614 |  6601 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|    307611 |  6602 | `					&& &pIn[1] == pDefend` |
|     45879 |  6603 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|     34402 |  6604 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|     21021 |  6605 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|     15291 |  6606 | `					sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|         - |  6607 | `					/* php 8.4: the implicit form is deprecated at COMPILE time —` |
|         - |  6608 | `` 					 * `f(): Implicitly marking parameter $x as nullable …` `` |
|         - |  6609 | `					 * (methods carry the Class:: prefix when the class link is` |
|         - |  6610 | `					 * already up at this point). */` |
|         - |  6611 | `					{` |
|     15291 |  6612 | `						const char *zSep = "";` |
|     15291 |  6613 | `						SyString sCls = { "", 0 };` |
|     15291 |  6614 | `						if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|     15285 |  6615 | `							sCls = ((ph7_class *)pFunc->pUserData)->sName;` |
|     15285 |  6616 | `							zSep = "::";` |
|      7640 |  6617 | `						}` |
|     22934 |  6618 | `						PH7_GenCompileError(&(*pGen),8192 /* E_DEPRECATED */,pIn->nLine,` |
|         - |  6619 | `							"%z%s%z(): Implicitly marking parameter $%z as nullable is deprecated, the explicit nullable type must be used instead",` |
|      7643 |  6620 | `							&sCls,zSep,&pFunc->sName,&sArg.sName);` |
|         - |  6621 | `					}` |
|      7643 |  6622 | `				}` |
|         - |  6623 | `				/* Point beyond the default value */` |
|    565527 |  6624 | `				pIn = pDefend;` |
|    282761 |  6625 | `			}` |
|   1143303 |  6626 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  6627 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|       ! 0 |  6628 | `				return rc;` |
|         - |  6629 | `			}` |
|   1143303 |  6630 | `			pIn++; /* Jump the trailing comma */` |
|    571649 |  6631 | `		}` |
|         - |  6632 | `		/* Append argument signature */` |
|   2054307 |  6633 | `		if( sArg.nType > 0 ){` |
|    115063 |  6634 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|         - |  6635 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|     26833 |  6636 | `				int marker = 'o';` |
|     26833 |  6637 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|     26833 |  6638 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|     13419 |  6639 | `			}else{` |
|         - |  6640 | `				int c;` |
|     88235 |  6641 | `				c = 'n'; /* cc warning */` |
|         - |  6642 | `				/* Type leading character */` |
|     88235 |  6643 | `				switch(sArg.nType){` |
|      5736 |  6644 | `				case MEMOBJ_HASHMAP:` |
|         - |  6645 | `					/* Hashmap aka 'array' */` |
|     11477 |  6646 | `					c = 'h';` |
|     11477 |  6647 | `					break;` |
|      9675 |  6648 | `				case MEMOBJ_INT:` |
|         - |  6649 | `					/* Integer */` |
|     19355 |  6650 | `					c = 'i';` |
|     19355 |  6651 | `					break;` |
|         2 |  6652 | `				case MEMOBJ_BOOL:` |
|         - |  6653 | `					/* Bool */` |
|         5 |  6654 | `					c = 'b';` |
|         5 |  6655 | `					break;` |
|         5 |  6656 | `				case MEMOBJ_REAL:` |
|         - |  6657 | `					/* Float */` |
|        12 |  6658 | `					c = 'f';` |
|        12 |  6659 | `					break;` |
|     28689 |  6660 | `				case MEMOBJ_STRING:` |
|         - |  6661 | `					/* String */` |
|     57383 |  6662 | `					c = 's';` |
|     57383 |  6663 | `					break;` |
|         7 |  6664 | `				case MEMOBJ_OBJ:` |
|         - |  6665 | `					/* Object */` |
|        16 |  6666 | `					c = 'o';` |
|        14 |  6667 | `					break;` |
|         1 |  6668 | `				default:` |
|         2 |  6669 | `					break;` |
|         - |  6670 | `				}` |
|     88235 |  6671 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|         - |  6672 | `			}` |
|     57534 |  6673 | `		}else{` |
|         - |  6674 | `			/* No type is associated with this parameter which mean` |
|         - |  6675 | `			 * that this function is not condidate for overloading.` |
|         - |  6676 | `			 */` |
|   1939249 |  6677 | `			SyBlobRelease(&sSig);` |
|         - |  6678 | `		}` |
|         - |  6679 | `		/* Save in the argument set */` |
|   2054307 |  6680 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|         5 |  6681 | `	}` |
|   1289303 |  6682 | `	if( SyBlobLength(&sSig) > 0 ){` |
|         - |  6683 | `		/* Save function signature */` |
|     84427 |  6684 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|     42211 |  6685 | `	}` |
|   1289303 |  6686 | `	return SXRET_OK;` |
|    644662 |  6687 | `}` |
|         - |  6688 | `/*` |
|         - |  6689 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|         - |  6690 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|         - |  6691 | ` * the enclosing function. Returns the token just past the nested construct.` |
|         - |  6692 | ` */` |
|     34424 |  6693 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|         5 |  6694 | `{` |
|     34429 |  6695 | `	sxi32 iParen = 0;` |
|     34429 |  6696 | `	pIn++; /* past 'function'/'fn' */` |
|         - |  6697 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|         - |  6698 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|         - |  6699 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|    153049 |  6700 | `	while( pIn < pEnd ){` |
|    153049 |  6701 | `		sxu32 t = pIn->nType;` |
|    153049 |  6702 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|    149173 |  6703 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|    103277 |  6704 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|     84133 |  6705 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|    118625 |  6706 | `		pIn++;` |
|         5 |  6707 | `	}` |
|     19149 |  6708 | `	if( pIn >= pEnd ){ return pIn; }` |
|         - |  6709 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|         - |  6710 | `	{` |
|     19149 |  6711 | `		sxi32 d = 0;` |
|    760681 |  6712 | `		while( pIn < pEnd ){` |
|    760681 |  6713 | `			sxu32 t = pIn->nType;` |
|    760681 |  6714 | `			if( t & PH7_TK_OCB ){ d++; }` |
|    730069 |  6715 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|    741537 |  6716 | `			pIn++;` |
|         5 |  6717 | `		}` |
|         - |  6718 | `	}` |
|     19149 |  6719 | `	return pIn;` |
|     17217 |  6720 | `}` |
|         - |  6721 | `/*` |
|         - |  6722 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|         - |  6723 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|         - |  6724 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|         - |  6725 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|         - |  6726 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|         - |  6727 | ` * detached-mini-program path untouched.` |
|         - |  6728 | ` */` |
|         - |  6729 | `/*` |
|         - |  6730 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|         - |  6731 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|         - |  6732 | ` * mixed, object.` |
|         - |  6733 | ` */` |
|     11484 |  6734 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|         5 |  6735 | `{` |
|         - |  6736 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|         - |  6737 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|         - |  6738 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|         - |  6739 | `	};` |
|         - |  6740 | `	sxu32 i;` |
|     11489 |  6741 | `	if( nName > 0 && zName[0] == '\\' ){` |
|       ! 0 |  6742 | `		zName++;` |
|       ! 0 |  6743 | `		nName--;` |
|       ! 0 |  6744 | `	}` |
|     11497 |  6745 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|     11497 |  6746 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|     11489 |  6747 | `			return 1;` |
|         - |  6748 | `		}` |
|         5 |  6749 | `	}` |
|       ! 0 |  6750 | `	return 0;` |
|      5747 |  6751 | `}` |
|         - |  6752 | `/*` |
|         - |  6753 | ` * One atom of a generator's declared return type: is it a supertype of` |
|         - |  6754 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|         - |  6755 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|         - |  6756 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|         - |  6757 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|         - |  6758 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|         - |  6759 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|         - |  6760 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|         - |  6761 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|         - |  6762 | ` * both rather than fatal on valid code (a recorded divergence).` |
|         - |  6763 | ` */` |
|     11486 |  6764 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|         5 |  6765 | `{` |
|     11491 |  6766 | `	if( nType == MEMOBJ_OBJ ){` |
|       ! 0 |  6767 | ``		return 1; /* bare `object` */`` |
|         - |  6768 | `	}` |
|     11491 |  6769 | `	if( nType != SXU32_HIGH ){` |
|         3 |  6770 | `		return 0; /* scalar/array/void/never/null/... */` |
|         - |  6771 | `	}` |
|     11489 |  6772 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|     11489 |  6773 | `		return 1;` |
|         - |  6774 | `	}` |
|         - |  6775 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|         - |  6776 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|         - |  6777 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|         - |  6778 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|         - |  6779 | `	{` |
|         - |  6780 | `		SyBlob sFQN;` |
|         - |  6781 | `		int bOk;` |
|       ! 0 |  6782 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       ! 0 |  6783 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|       ! 0 |  6784 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|       ! 0 |  6785 | `		SyBlobRelease(&sFQN);` |
|       ! 0 |  6786 | `		return bOk;` |
|         - |  6787 | `	}` |
|      5748 |  6788 | `}` |
|         - |  6789 | `/*` |
|         - |  6790 | ` * php 8: a generator function may only declare a return type that is a` |
|         - |  6791 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|         - |  6792 | ` * group qualifies only if every member does. Anything else is php's exact` |
|         - |  6793 | ` * compile-time fatal "Generator return type must be a supertype of` |
|         - |  6794 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|         - |  6795 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|         - |  6796 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|         - |  6797 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|         - |  6798 | ` */` |
|     11726 |  6799 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|         5 |  6800 | `{` |
|     11731 |  6801 | `	int bOk = 0;` |
|         - |  6802 | `	sxu32 nLine;` |
|         - |  6803 | `	sxi32 rc;` |
|     11731 |  6804 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|       245 |  6805 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|         - |  6806 | `	}` |
|     11491 |  6807 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|       ! 0 |  6808 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|       ! 0 |  6809 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|         - |  6810 | `		sxu32 i,j;` |
|       ! 0 |  6811 | `		for( i = 0; i < n && !bOk; i++ ){` |
|         - |  6812 | `			int bGroupOk;` |
|       ! 0 |  6813 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|       ! 0 |  6814 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|         - |  6815 | `			}` |
|       ! 0 |  6816 | `			bGroupOk = 1;` |
|       ! 0 |  6817 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|       ! 0 |  6818 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|       ! 0 |  6819 | `					bGroupOk = 0;` |
|       ! 0 |  6820 | `					break;` |
|         - |  6821 | `				}` |
|       ! 0 |  6822 | `			}` |
|       ! 0 |  6823 | `			bOk = bGroupOk;` |
|       ! 0 |  6824 | `		}` |
|       ! 0 |  6825 | `	}else{` |
|     11491 |  6826 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|         - |  6827 | `	}` |
|     11491 |  6828 | `	if( bOk ){` |
|     11489 |  6829 | `		return SXRET_OK;` |
|         - |  6830 | `	}` |
|         - |  6831 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|         - |  6832 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|         - |  6833 | `	 * token of this stream — its line is the function's closing brace. php` |
|         - |  6834 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|         - |  6835 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|         3 |  6836 | `	nLine = pGen->pIn[-1].nLine;` |
|         - |  6837 | `	{` |
|         3 |  6838 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|         3 |  6839 | `		if( sGiven.nByte < 1 ){` |
|       ! 0 |  6840 | `			sGiven = pFunc->sReturnClass;` |
|       ! 0 |  6841 | `		}` |
|         3 |  6842 | `		if( sGiven.nByte < 1 ){` |
|         - |  6843 | ``			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the`` |
|         - |  6844 | `			 * rendered type text, so sReturnTypeName arrives empty for them —` |
|         - |  6845 | `			 * name them here (the root fix belongs to that renderer, §3.7). */` |
|       ! 0 |  6846 | `			const char *zScalar =` |
|       ! 0 |  6847 | `				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :` |
|       ! 0 |  6848 | `				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";` |
|       ! 0 |  6849 | `			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));` |
|       ! 0 |  6850 | `		}` |
|         3 |  6851 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  6852 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|         - |  6853 | `	}` |
|         3 |  6854 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|      5868 |  6855 | `}` |
|   2750342 |  6856 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|         5 |  6857 | `{` |
|   2750347 |  6858 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|   2750347 |  6859 | `	SyToken *pEnd = pGen->pEnd;` |
|   2750347 |  6860 | `	sxi32 iDepth = 0;` |
|   2750347 |  6861 | `	int bStarted = 0;` |
| 133266461 |  6862 | `	while( pIn < pEnd ){` |
| 133266461 |  6863 | `		sxu32 t = pIn->nType;` |
| 133266461 |  6864 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 127298665 |  6865 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 121365631 |  6866 | `		if( t & PH7_TK_KEYWORD ){` |
|   9043763 |  6867 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|   9043763 |  6868 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|   9032037 |  6869 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|         - |  6870 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|   4498804 |  6871 | `		}` |
| 121319481 |  6872 | `		pIn++;` |
|         5 |  6873 | `	}` |
|   2738621 |  6874 | `	return FALSE;` |
|   1375176 |  6875 | `}` |
|         - |  6876 | `/*` |
|         - |  6877 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|         - |  6878 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  6879 | ` * and this routine takes care of generating the appropriate error message.` |
|         - |  6880 | ` */` |
|   2750342 |  6881 | `static sxi32 GenStateCompileFuncBody(` |
|         - |  6882 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |  6883 | `	ph7_vm_func *pFunc    /* Function state */` |
|         - |  6884 | `	)` |
|         5 |  6885 | `{` |
|         - |  6886 | `	SySet *pInstrContainer; /* Instruction container */` |
|         - |  6887 | `	GenBlock *pBlock;` |
|         - |  6888 | `	sxu32 nGotoOfft;` |
|         - |  6889 | `	sxi32 rc;` |
|         - |  6890 | `	/* Attach the new function */` |
|   2750347 |  6891 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   2750347 |  6892 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  6893 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|         - |  6894 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  6895 | `		return SXERR_ABORT;` |
|         - |  6896 | `	}` |
|   2750347 |  6897 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|         - |  6898 | `	/* Swap bytecode containers */` |
|   2750347 |  6899 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   2750347 |  6900 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|         - |  6901 | `	/* Emit constructor property promotion prologue:` |
|         - |  6902 | `	 *   $this->NAME = $NAME;` |
|         - |  6903 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|         - |  6904 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|         - |  6905 | `	{` |
|   2750347 |  6906 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|         - |  6907 | `		sxu32 i;` |
|   4751031 |  6908 | `		for( i = 0; i < nArg; i++ ){` |
|   2000689 |  6909 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|         - |  6910 | `			char *zSrc;` |
|         - |  6911 | `			sxu32 nSrc,nName;` |
|         - |  6912 | `			SySet sToken;` |
|         - |  6913 | `			SyToken *pTmpIn,*pTmpEnd;` |
|         - |  6914 | `			sxi32 rcPromote;` |
|   2000689 |  6915 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   2000615 |  6916 | `				continue;` |
|         - |  6917 | `			}` |
|         - |  6918 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|         - |  6919 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|         - |  6920 | `			 * copied), so it must outlive the function — never free it. The` |
|         - |  6921 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|         - |  6922 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|        79 |  6923 | `			nName = SyStringLength(&pArg->sName);` |
|        79 |  6924 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|        79 |  6925 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|        79 |  6926 | `			if( zSrc == 0 ){` |
|       ! 0 |  6927 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  6928 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  6929 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  6930 | `				return SXERR_ABORT;` |
|         - |  6931 | `			}` |
|         - |  6932 | `			{` |
|        79 |  6933 | `				char *z = zSrc;` |
|        79 |  6934 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|        79 |  6935 | `				z += sizeof("$this->")-1;` |
|        79 |  6936 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        79 |  6937 | `				z += nName;` |
|        79 |  6938 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|        79 |  6939 | `				z += sizeof(" = $")-1;` |
|        79 |  6940 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        79 |  6941 | `				z += nName;` |
|        79 |  6942 | `				*z = 0;` |
|         - |  6943 | `			}` |
|        79 |  6944 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        79 |  6945 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);` |
|        79 |  6946 | `			pTmpIn = pGen->pIn;` |
|        79 |  6947 | `			pTmpEnd = pGen->pEnd;` |
|        79 |  6948 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|        79 |  6949 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|        79 |  6950 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|        79 |  6951 | `			pGen->pIn = pTmpIn;` |
|        79 |  6952 | `			pGen->pEnd = pTmpEnd;` |
|        79 |  6953 | `			SySetRelease(&sToken);` |
|        79 |  6954 | `			if( rcPromote == SXERR_ABORT ){` |
|       ! 0 |  6955 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  6956 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  6957 | `				return SXERR_ABORT;` |
|         - |  6958 | `			}` |
|         - |  6959 | `			/* Discard the assignment result — this is a statement expression. */` |
|        79 |  6960 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        42 |  6961 | `		}` |
|         - |  6962 | `	}` |
|         - |  6963 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|         - |  6964 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|         - |  6965 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|         - |  6966 | `	 * generator — and vice versa — is classified independently. */` |
|         - |  6967 | `	{` |
|   2750347 |  6968 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|   2750347 |  6969 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|         - |  6970 | `		/* Compile the body */` |
|   2750347 |  6971 | `		PH7_CompileBlock(&(*pGen),0);` |
|   2750347 |  6972 | `		pGen->bInGenerator = bSavedGen;` |
|         - |  6973 | `	}` |
|         - |  6974 | `	/* Fix exception jumps now the destination is resolved */` |
|   2750347 |  6975 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - |  6976 | `	/* Emit the final return if not yet done */` |
|   2750347 |  6977 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - |  6978 | `	/* Fix gotos jumps now the destination is resolved */` |
|   2750347 |  6979 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|       ! 0 |  6980 | `		rc = SXERR_ABORT;` |
|       ! 0 |  6981 | `	}` |
|   2750347 |  6982 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|         - |  6983 | `	/* Restore the default container */` |
|   2750347 |  6984 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - |  6985 | `	/* Leave function block */` |
|   2750347 |  6986 | `	GenStateLeaveBlock(&(*pGen),0);` |
|   2750347 |  6987 | `	if( rc == SXERR_ABORT ){` |
|         - |  6988 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  6989 | `		return SXERR_ABORT;` |
|         - |  6990 | `	}` |
|         - |  6991 | `	/* Scan for yield opcodes to detect generator functions */` |
|         - |  6992 | `	{` |
|   2750347 |  6993 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|         - |  6994 | `		sxu32 i;` |
|  81317183 |  6995 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
|  78578567 |  6996 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     11731 |  6997 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     11731 |  6998 | `				break;` |
|         - |  6999 | `			}` |
|  39283423 |  7000 | `		}` |
|         - |  7001 | `	}` |
|   2750347 |  7002 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|         - |  7003 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|     11731 |  7004 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|       ! 0 |  7005 | `			return SXERR_ABORT;` |
|         - |  7006 | `		}` |
|      5863 |  7007 | `	}` |
|         - |  7008 | `	/* All done, function body compiled */` |
|   2750347 |  7009 | `	return SXRET_OK;` |
|   1375176 |  7010 | `}` |
|         - |  7011 | `/*` |
|         - |  7012 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|         - |  7013 | ` * According to the PHP language reference manual.` |
|         - |  7014 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|         - |  7015 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|         - |  7016 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|         - |  7017 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - |  7018 | ` *  Functions need not be defined before they are referenced.` |
|         - |  7019 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|         - |  7020 | ` *  a function even if they were defined inside and vice versa.` |
|         - |  7021 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|         - |  7022 | ` *  calls with over 32-64 recursion levels.` |
|         - |  7023 | ` *` |
|         - |  7024 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|         - |  7025 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|         - |  7026 | ` * on these extension.` |
|         - |  7027 | ` */` |
|         - |  7028 | `/*` |
|         - |  7029 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|         - |  7030 | ` */` |
|       570 |  7031 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|         5 |  7032 | `{` |
|         - |  7033 | `	sxu32 i;` |
|      1611 |  7034 | `	for( i = 0; i < n; i++ ){` |
|      1381 |  7035 | `		int a = zA[i], b = zB[i];` |
|      1381 |  7036 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|      1381 |  7037 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|      1381 |  7038 | `		if( a != b ) return a - b;` |
|       523 |  7039 | `	}` |
|       235 |  7040 | `	return 0;` |
|       290 |  7041 | `}` |
|         - |  7042 | `/*` |
|         - |  7043 | ` * Internal type-atom kinds used during union type parsing.` |
|         - |  7044 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|         - |  7045 | ` * (which are positive bit values stored in sxu32).` |
|         - |  7046 | ` */` |
|         - |  7047 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|         - |  7048 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|         - |  7049 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|         - |  7050 |  |
|         - |  7051 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|         - |  7052 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|         - |  7053 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|         - |  7054 |  |
|         - |  7055 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|         - |  7056 | `struct PhlTypeAtom {` |
|         - |  7057 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|         - |  7058 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|         - |  7059 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|         - |  7060 | `	sxu32 nCanon;` |
|         - |  7061 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|         - |  7062 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|         - |  7063 | `};` |
|         - |  7064 |  |
|         - |  7065 | `/*` |
|         - |  7066 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|         - |  7067 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|         - |  7068 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|         - |  7069 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|         - |  7070 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|         - |  7071 | ` * already be consumed by the caller.` |
|         - |  7072 | ` */` |
|         - |  7073 | `/*` |
|         - |  7074 | ` * TRUE if pName is a reserved PHP type keyword (never a class name), so a type` |
|         - |  7075 | ` * hint that spells it must not be namespace-qualified. Only the words that can` |
|         - |  7076 | ` * reach GenStateParseOneTypeAtom's identifier branch as a bare name matter here` |
|         - |  7077 | ` * (bool/int/float/string/array/object/self/static/parent arrive as keywords, and` |
|         - |  7078 | ` * null/void/never are matched before the class path), but the full set is listed` |
|         - |  7079 | ` * so the guard is robust to lexer changes.` |
|         - |  7080 | ` */` |
|     38518 |  7081 | `static int GenStateIsReservedTypeWord(const SyString *pName)` |
|         5 |  7082 | `{` |
|         - |  7083 | `	static const char *azWords[] = {` |
|         - |  7084 | `		"false","true","mixed","iterable","callable","null","void","never",` |
|         - |  7085 | `		"bool","boolean","int","integer","float","double","string","array",` |
|         - |  7086 | `		"object","self","static","parent"` |
|         - |  7087 | `	};` |
|         - |  7088 | `	sxu32 i;` |
|    807085 |  7089 | `	for( i = 0 ; i < SX_ARRAYSIZE(azWords) ; i++ ){` |
|    768669 |  7090 | `		sxu32 n = (sxu32)SyStrlen(azWords[i]);` |
|    768669 |  7091 | `		if( pName->nByte == n && SyStrnicmp(pName->zString,azWords[i],n) == 0 ){` |
|       107 |  7092 | `			return 1;` |
|         - |  7093 | `		}` |
|    384286 |  7094 | `	}` |
|     38421 |  7095 | `	return 0;` |
|     19264 |  7096 | `}` |
|    127860 |  7097 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|         5 |  7098 | `{` |
|    127865 |  7099 | `	SyToken *pIn = pGen->pIn;` |
|    127865 |  7100 | `	int bAbsolute = 0;` |
|    127865 |  7101 | `	SyZero(pOut, sizeof(*pOut));` |
|    127865 |  7102 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|    127865 |  7103 | `	if( pIn >= pGen->pEnd ){` |
|       ! 0 |  7104 | `		return SXERR_SYNTAX;` |
|         - |  7105 | `	}` |
|         - |  7106 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|    127865 |  7107 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|        10 |  7108 | `		bAbsolute = 1; /* fully-qualified: never prefix the current namespace */` |
|        10 |  7109 | `		pIn++;` |
|        10 |  7110 | `		if( pIn >= pGen->pEnd ){` |
|       ! 0 |  7111 | `			return SXERR_SYNTAX;` |
|         - |  7112 | `		}` |
|         4 |  7113 | `	}` |
|    127865 |  7114 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  7115 | `		return SXERR_SYNTAX;` |
|         - |  7116 | `	}` |
|    127865 |  7117 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|     89115 |  7118 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|     89115 |  7119 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|     11539 |  7120 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|     83348 |  7121 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|        83 |  7122 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|     77542 |  7123 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|     19767 |  7124 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|     67622 |  7125 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|     57649 |  7126 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|     28919 |  7127 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|        41 |  7128 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|        79 |  7129 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|        28 |  7130 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|        48 |  7131 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|        16 |  7132 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|        33 |  7133 | `			pOut->nType = SXU32_HIGH;` |
|        33 |  7134 | `			pOut->sClass = pIn->sData;` |
|        18 |  7135 | `		}else{` |
|         3 |  7136 | `			return SXERR_SYNTAX;` |
|         - |  7137 | `		}` |
|     89113 |  7138 | `		pIn++;` |
|     44559 |  7139 | `	}else{` |
|         - |  7140 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|         - |  7141 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|     38755 |  7142 | `		SyString *pT = &pIn->sData;` |
|     38755 |  7143 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|        34 |  7144 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|        34 |  7145 | `			pIn++;` |
|     38740 |  7146 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|       177 |  7147 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|       177 |  7148 | `			pIn++;` |
|     38639 |  7149 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|        27 |  7150 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|        27 |  7151 | `			pIn++;` |
|        16 |  7152 | `		}else{` |
|         - |  7153 | `			/* Class / interface name; consume namespace path a\b\c */` |
|     38531 |  7154 | `			SyToken *pFirst = pIn;` |
|     38531 |  7155 | `			SyToken *pLast = pIn;` |
|     38531 |  7156 | `			pOut->nType = SXU32_HIGH;` |
|     38531 |  7157 | `			pOut->sClass = pIn->sData;` |
|     38531 |  7158 | `			pIn++;` |
|     57792 |  7159 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|     38534 |  7160 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|         3 |  7161 | `				pLast = &pIn[1];` |
|         3 |  7162 | `				pIn += 2;` |
|         1 |  7163 | `			}` |
|     38531 |  7164 | `			if( pLast != pFirst ){` |
|         3 |  7165 | `				const char *zFirst = pFirst->sData.zString;` |
|         3 |  7166 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|         3 |  7167 | `				pOut->sClass.zString = zFirst;` |
|         3 |  7168 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|         1 |  7169 | `			}` |
|         - |  7170 | `			/* Namespace-qualify a bare (single-segment, non-absolute) class type so` |
|         - |  7171 | ``			 * a `: Base` / `Base $x` hint in namespace N resolves to N\Base (or a`` |
|         - |  7172 | ``			 * `use` alias) at type-check time instead of the global \Base — mirrors`` |
|         - |  7173 | `			 * the NEW/CALL/instanceof qualification. Absolute (\Base) and already-` |
|         - |  7174 | `			 * qualified (A\B) names are left as written, matching GenStateNsQualifyName. */` |
|         - |  7175 | `			/* Reserved type words that reach this identifier branch (false, true,` |
|         - |  7176 | `			 * mixed, iterable, callable) are NOT classes and must not be qualified` |
|         - |  7177 | ``			 * (else `false\|string` becomes `Ns\false\|string`). */`` |
|     38531 |  7178 | `			if( !bAbsolute && pLast == pFirst && !GenStateIsReservedTypeWord(&pOut->sClass) ){` |
|         - |  7179 | `				SyBlob sFqn;` |
|     38421 |  7180 | `				SyBlobInit(&sFqn,&pGen->pVm->sAllocator);` |
|     38421 |  7181 | `				GenStateResolveName(pGen,&pOut->sClass,&sFqn);` |
|     38416 |  7182 | `				if( SyBlobLength(&sFqn) != pOut->sClass.nByte` |
|     38416 |  7183 | `				 \|\| SyMemcmp(SyBlobData(&sFqn),(const void *)pOut->sClass.zString,pOut->sClass.nByte) != 0 ){` |
|        17 |  7184 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 |  7185 | `						(const char *)SyBlobData(&sFqn),SyBlobLength(&sFqn));` |
|        12 |  7186 | `					if( zDup ){` |
|        12 |  7187 | `						SyStringInitFromBuf(&pOut->sClass,zDup,SyBlobLength(&sFqn));` |
|         5 |  7188 | `					}` |
|         5 |  7189 | `				}` |
|     38421 |  7190 | `				SyBlobRelease(&sFqn);` |
|     19208 |  7191 | `			}` |
|         - |  7192 | `		}` |
|         - |  7193 | `	}` |
|    127863 |  7194 | `	pGen->pIn = pIn;` |
|    127863 |  7195 | `	return SXRET_OK;` |
|     63935 |  7196 | `}` |
|         - |  7197 |  |
|         - |  7198 | `/*` |
|         - |  7199 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|         - |  7200 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|         - |  7201 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|         - |  7202 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|         - |  7203 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|         - |  7204 | ` */` |
|    127682 |  7205 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|         5 |  7206 | `{` |
|         - |  7207 | `	int i;` |
|    127687 |  7208 | `	int nNonNull = 0;` |
|    127687 |  7209 | `	int bAnyIntersection = 0;` |
|         - |  7210 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|    127687 |  7211 | `	sxu32 nMaxGroup = 0;` |
|   4213511 |  7212 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    255521 |  7213 | `	for( i = 0; i < nAtoms; i++ ){` |
|    127839 |  7214 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    127809 |  7215 | `			nNonNull++;` |
|    127809 |  7216 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|    127809 |  7217 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|    127809 |  7218 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|     63902 |  7219 | `			}` |
|     63902 |  7220 | `		}` |
|     63922 |  7221 | `	}` |
|    255469 |  7222 | `	for( i = 0; i < nAtoms; i++ ){` |
|    127811 |  7223 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        29 |  7224 | `			bAnyIntersection = 1;` |
|        29 |  7225 | `			break;` |
|         - |  7226 | `		}` |
|     63896 |  7227 | `	}` |
|    127687 |  7228 | `	if( bAnyIntersection ){` |
|         - |  7229 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|         - |  7230 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|         - |  7231 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|        29 |  7232 | `		sxu32 g, nGroups = 0;` |
|        29 |  7233 | `		int bFirstGroup = 1;` |
|        59 |  7234 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|        59 |  7235 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|        35 |  7236 | `			int bFirstMember = 1;` |
|         - |  7237 | `			int bWrap;` |
|        35 |  7238 | `			if( aGroupCount[g] == 0 ) continue;` |
|         - |  7239 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|         - |  7240 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|         - |  7241 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|         - |  7242 | `			 * parens, matching PHP's canonical text. */` |
|        47 |  7243 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|        35 |  7244 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|        35 |  7245 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|       107 |  7246 | `			for( i = 0; i < nAtoms; i++ ){` |
|        77 |  7247 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|        59 |  7248 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|        59 |  7249 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|        55 |  7250 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        30 |  7251 | `				}else{` |
|         6 |  7252 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  7253 | `				}` |
|        59 |  7254 | `				bFirstMember = 0;` |
|        32 |  7255 | `			}` |
|        35 |  7256 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|        35 |  7257 | `			bFirstGroup = 0;` |
|        20 |  7258 | `		}` |
|        29 |  7259 | `		if( bNullable ){` |
|       ! 0 |  7260 | `			SyBlobAppend(pBlob, "\|", 1);` |
|       ! 0 |  7261 | `			SyBlobAppend(pBlob, "null", 4);` |
|       ! 0 |  7262 | `		}` |
|        85 |  7263 | `		return;` |
|         - |  7264 | `	}` |
|    127663 |  7265 | `	if( nNonNull == 1 && bNullable ){` |
|         - |  7266 | `		/* Shorthand: ?T */` |
|       117 |  7267 | `		for( i = 0; i < nAtoms; i++ ){` |
|       117 |  7268 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       117 |  7269 | `			SyBlobAppend(pBlob, "?", 1);` |
|       117 |  7270 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|        24 |  7271 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        13 |  7272 | `			}else{` |
|        95 |  7273 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  7274 | `			}` |
|       117 |  7275 | `			return;` |
|       ! 0 |  7276 | `		}` |
|       ! 0 |  7277 | `	}` |
|         - |  7278 | `	{` |
|    127551 |  7279 | `		int bFirst = 1;` |
|         - |  7280 | `		/* 1) Classes in declaration order */` |
|    255205 |  7281 | `		for( i = 0; i < nAtoms; i++ ){` |
|    127659 |  7282 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|     38489 |  7283 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     38489 |  7284 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|     38489 |  7285 | `				bFirst = 0;` |
|     19242 |  7286 | `			}` |
|     63832 |  7287 | `		}` |
|         - |  7288 | `		/* 2) Built-ins in canonical order */` |
|         - |  7289 | `		{` |
|         - |  7290 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|         - |  7291 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|         - |  7292 | `			int k;` |
|    892827 |  7293 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|   1442125 |  7294 | `				for( i = 0; i < nAtoms; i++ ){` |
|    765817 |  7295 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|     88973 |  7296 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     88973 |  7297 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|     88973 |  7298 | `						bFirst = 0;` |
|     88973 |  7299 | `						break;` |
|         - |  7300 | `					}` |
|    338427 |  7301 | `				}` |
|    382643 |  7302 | `			}` |
|         - |  7303 | `		}` |
|         - |  7304 | `		/* 3) null suffix */` |
|    127551 |  7305 | `		if( bNullable ){` |
|        20 |  7306 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|        20 |  7307 | `			SyBlobAppend(pBlob, "null", 4);` |
|         8 |  7308 | `		}` |
|         - |  7309 | `	}` |
|     63846 |  7310 | `}` |
|         - |  7311 |  |
|         - |  7312 | `/*` |
|         - |  7313 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|         - |  7314 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|         - |  7315 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|         - |  7316 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|         - |  7317 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|         - |  7318 | ` * whether it was parenthesized.` |
|         - |  7319 | ` *` |
|         - |  7320 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|         - |  7321 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|         - |  7322 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|         - |  7323 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|         - |  7324 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|         - |  7325 | ` */` |
|    127834 |  7326 | `static sxi32 GenStateParsePart(` |
|         - |  7327 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|         - |  7328 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|         5 |  7329 | `{` |
|         - |  7330 | `	sxi32 rc;` |
|    127839 |  7331 | `	int nMembers = 0;` |
|    127839 |  7332 | `	int bParen = 0;` |
|    127839 |  7333 | `	*pnMembers = 0;` |
|    127839 |  7334 | `	*pbParen = 0;` |
|    127839 |  7335 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         9 |  7336 | `		bParen = 1;` |
|         9 |  7337 | `		pGen->pIn++; /* skip '(' */` |
|         3 |  7338 | `	}` |
|     63917 |  7339 | `	for(;;){` |
|    127865 |  7340 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|       ! 0 |  7341 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7342 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|       ! 0 |  7343 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7344 | `		}` |
|    127865 |  7345 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|    127865 |  7346 | `		if( rc != SXRET_OK ){` |
|         3 |  7347 | `			return rc;` |
|         - |  7348 | `		}` |
|    127863 |  7349 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|    127863 |  7350 | `		(*pnAtoms)++;` |
|    127863 |  7351 | `		nMembers++;` |
|         - |  7352 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|    127863 |  7353 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        39 |  7354 | `			SyToken *pNext = &pGen->pIn[1];` |
|        34 |  7355 | `			if( pNext < pGen->pEnd` |
|        39 |  7356 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        31 |  7357 | `				pGen->pIn++; /* skip '&' */` |
|        31 |  7358 | `				continue;` |
|         - |  7359 | `			}` |
|         4 |  7360 | `		}` |
|    127837 |  7361 | `		break;` |
|       ! 0 |  7362 | `	}` |
|    127837 |  7363 | `	if( bParen ){` |
|         9 |  7364 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  7365 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7366 | `				"Malformed DNF type: expecting ')'");` |
|       ! 0 |  7367 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7368 | `		}` |
|         9 |  7369 | `		pGen->pIn++; /* skip ')' */` |
|         9 |  7370 | `		if( nMembers < 2 ){` |
|       ! 0 |  7371 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7372 | `				"Parenthesized type must be an intersection of at least two types");` |
|       ! 0 |  7373 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7374 | `		}` |
|         3 |  7375 | `	}` |
|    127837 |  7376 | `	*pnMembers = nMembers;` |
|    127837 |  7377 | `	*pbParen = bParen;` |
|    127837 |  7378 | `	return SXRET_OK;` |
|     63922 |  7379 | `}` |
|         - |  7380 |  |
|         - |  7381 | `/*` |
|         - |  7382 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|         - |  7383 | ` *` |
|         - |  7384 | ` * Outputs:` |
|         - |  7385 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|         - |  7386 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|         - |  7387 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|         - |  7388 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|         - |  7389 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|         - |  7390 | ` *     already be initialized by the caller (allocator set, etc).` |
|         - |  7391 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|         - |  7392 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|         - |  7393 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|         - |  7394 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|         - |  7395 | ` *` |
|         - |  7396 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|         - |  7397 | ` * SXERR_ABORT on fatal compile errors.` |
|         - |  7398 | ` */` |
|    127698 |  7399 | `static sxi32 GenStateParseUnionTypeDecl(` |
|         - |  7400 | `	ph7_gen_state *pGen,` |
|         - |  7401 | `	sxu32 *pnType,` |
|         - |  7402 | `	SyString *pClass,` |
|         - |  7403 | `	SySet *pAlts,` |
|         - |  7404 | `	sxi32 *piTypeFlags,` |
|         - |  7405 | `	SyString *pTypeText,` |
|         - |  7406 | `	int iNullableFlag,` |
|         - |  7407 | `	int iUnionFlag,` |
|         - |  7408 | `	int bAllowVoid,` |
|         - |  7409 | `	sxu32 nLine` |
|         5 |  7410 | `){` |
|         - |  7411 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|    127703 |  7412 | `	int nAtoms = 0;` |
|    127703 |  7413 | `	int bShortNullable = 0;` |
|    127703 |  7414 | `	int bExplicitNull = 0;` |
|         - |  7415 | `	sxi32 rc;` |
|    127703 |  7416 | `	*pnType = 0;` |
|    127703 |  7417 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|    127703 |  7418 | `	*piTypeFlags = 0;` |
|    127703 |  7419 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|         - |  7420 |  |
|    127703 |  7421 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7422 | `		return SXRET_OK;` |
|         - |  7423 | `	}` |
|         - |  7424 | ``	/* Optional `?` shorthand prefix */`` |
|    127698 |  7425 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|       105 |  7426 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|       105 |  7427 | `		bShortNullable = 1;` |
|       105 |  7428 | `		pGen->pIn++;` |
|       105 |  7429 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7430 | `			return SXERR_SYNTAX;` |
|         - |  7431 | `		}` |
|        50 |  7432 | `	}` |
|         - |  7433 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|         - |  7434 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|         - |  7435 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|         - |  7436 | `	{` |
|         - |  7437 | `		int nMembers, bParen;` |
|    127703 |  7438 | `		sxu32 iGroup = 0;` |
|    127703 |  7439 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|    127703 |  7440 | `		if( rc != SXRET_OK ){` |
|         4 |  7441 | `			return rc;` |
|         - |  7442 | `		}` |
|         - |  7443 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|         - |  7444 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|         - |  7445 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|         - |  7446 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|         - |  7447 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|    191753 |  7448 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|    127910 |  7449 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       143 |  7450 | `			if( bShortNullable ){` |
|         - |  7451 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|         - |  7452 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|         - |  7453 | `				 * already reported" so callers skip their own error emission. */` |
|         3 |  7454 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - |  7455 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|         3 |  7456 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|         - |  7457 | `			}` |
|       141 |  7458 | `			if( nMembers >= 2 && !bParen ){` |
|       ! 0 |  7459 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|         - |  7460 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 |  7461 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7462 | `			}` |
|       141 |  7463 | ``			pGen->pIn++; /* skip `\|` */`` |
|       141 |  7464 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|       141 |  7465 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  7466 | `				return rc;` |
|         - |  7467 | `			}` |
|         5 |  7468 | `		}` |
|    127699 |  7469 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|       ! 0 |  7470 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7471 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 |  7472 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7473 | `		}` |
|         - |  7474 | `	}` |
|         - |  7475 | `	/* Validation pass.` |
|         - |  7476 | `	 *` |
|         - |  7477 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|         - |  7478 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|         - |  7479 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|         - |  7480 | `	 */` |
|         - |  7481 | `	{` |
|         - |  7482 | `		int i, j;` |
|    127699 |  7483 | `		int bHasNonNull = 0;` |
|    127699 |  7484 | `		int bAnyIntersection = 0;` |
|         - |  7485 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|         - |  7486 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|         - |  7487 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
|   4213907 |  7488 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    255555 |  7489 | `		for( i = 0; i < nAtoms; i++ ){` |
|    127861 |  7490 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|     63933 |  7491 | `		}` |
|    255499 |  7492 | `		for( i = 0; i < nAtoms; i++ ){` |
|    127831 |  7493 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|     63905 |  7494 | `		}` |
|         - |  7495 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|         - |  7496 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|    127699 |  7497 | `		if( bShortNullable && bAnyIntersection ){` |
|       ! 0 |  7498 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7499 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|       ! 0 |  7500 | `			return SXERR_SYNTAX;` |
|         - |  7501 | `		}` |
|    255541 |  7502 | `		for( i = 0; i < nAtoms; i++ ){` |
|         - |  7503 | `			/* Intersection members must be class/interface types (PHP rejects` |
|         - |  7504 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|         - |  7505 | ``			 * `true`/`false` in an intersection). */`` |
|    127859 |  7506 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        55 |  7507 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|        55 |  7508 | `				if( bClassLike ){` |
|        53 |  7509 | `					SyString *pC = &aAtoms[i].sClass;` |
|        48 |  7510 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|        48 |  7511 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|        48 |  7512 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|        53 |  7513 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|       ! 0 |  7514 | `						bClassLike = 0;` |
|       ! 0 |  7515 | `					}` |
|        24 |  7516 | `				}` |
|        55 |  7517 | `				if( !bClassLike ){` |
|         - |  7518 | `					const char *zName; sxu32 nName;` |
|         3 |  7519 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 |  7520 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|       ! 0 |  7521 | `					}else{` |
|         3 |  7522 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|         - |  7523 | `					}` |
|         4 |  7524 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7525 | `						"Type %.*s cannot be part of an intersection type",` |
|         1 |  7526 | `						(int)nName, zName);` |
|         3 |  7527 | `					return SXERR_SYNTAX;` |
|         - |  7528 | `				}` |
|        24 |  7529 | `			}` |
|    127857 |  7530 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|       177 |  7531 | `				if( nAtoms > 1 ){` |
|         3 |  7532 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7533 | `						"Void can only be used as a standalone type");` |
|         3 |  7534 | `					return SXERR_SYNTAX;` |
|         - |  7535 | `				}` |
|       175 |  7536 | `				if( !bAllowVoid ){` |
|       ! 0 |  7537 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7538 | `						"void cannot be used here");` |
|       ! 0 |  7539 | `					return SXERR_SYNTAX;` |
|         - |  7540 | `				}` |
|       175 |  7541 | `				if( bShortNullable ){` |
|       ! 0 |  7542 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7543 | `						"Void type cannot be nullable");` |
|       ! 0 |  7544 | `					return SXERR_SYNTAX;` |
|         - |  7545 | `				}` |
|        85 |  7546 | `			}` |
|    127855 |  7547 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|         - |  7548 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|         - |  7549 | `				 * type (never = the function does not return). Mirrors the void` |
|         - |  7550 | `				 * validation above; accepted here and enforced at compile time` |
|         - |  7551 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|        27 |  7552 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|         - |  7553 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|         - |  7554 | `					 * same as any other non-standalone use. */` |
|         6 |  7555 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7556 | `						"never can only be used as a standalone type");` |
|         6 |  7557 | `					return SXERR_SYNTAX;` |
|         - |  7558 | `				}` |
|        21 |  7559 | `				if( !bAllowVoid ){` |
|         - |  7560 | `					/* Return-only: params call with bAllowVoid=0. */` |
|         3 |  7561 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7562 | `						"never cannot be used as a parameter type");` |
|         3 |  7563 | `					return SXERR_SYNTAX;` |
|         - |  7564 | `				}` |
|         8 |  7565 | `			}` |
|    127849 |  7566 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|        34 |  7567 | `				bExplicitNull = 1;` |
|        19 |  7568 | `			}else{` |
|    127819 |  7569 | `				bHasNonNull = 1;` |
|         - |  7570 | `			}` |
|         - |  7571 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|         - |  7572 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|         - |  7573 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|         - |  7574 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|         - |  7575 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|    128049 |  7576 | `			for( j = 0; j < i; j++ ){` |
|       207 |  7577 | `				int bDup = 0;` |
|       207 |  7578 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|       395 |  7579 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|       202 |  7580 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|       207 |  7581 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|       195 |  7582 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|        51 |  7583 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|        44 |  7584 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|        44 |  7585 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|        17 |  7586 | `								aAtoms[j].sClass.zString,` |
|        34 |  7587 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|       ! 0 |  7588 | `							bDup = 1;` |
|       ! 0 |  7589 | `						}` |
|        27 |  7590 | `					}else{` |
|         3 |  7591 | `						bDup = 1;` |
|         - |  7592 | `					}` |
|        23 |  7593 | `				}` |
|       195 |  7594 | `				if( bDup ){` |
|         - |  7595 | `					const char *zName;` |
|         - |  7596 | `					sxu32 nName;` |
|         3 |  7597 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 |  7598 | `						zName = aAtoms[i].sClass.zString;` |
|       ! 0 |  7599 | `						nName = aAtoms[i].sClass.nByte;` |
|       ! 0 |  7600 | `					}else{` |
|         3 |  7601 | `						zName = aAtoms[i].zCanon;` |
|         3 |  7602 | `						nName = aAtoms[i].nCanon;` |
|         - |  7603 | `					}` |
|         4 |  7604 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         1 |  7605 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|         3 |  7606 | `					return SXERR_SYNTAX;` |
|         - |  7607 | `				}` |
|        99 |  7608 | `			}` |
|     63926 |  7609 | `		}` |
|    127687 |  7610 | `		if( !bHasNonNull && bExplicitNull ){` |
|         7 |  7611 | `			if( bShortNullable ){` |
|         - |  7612 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|       ! 0 |  7613 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7614 | `					"Null can not be used as a standalone type");` |
|       ! 0 |  7615 | `				return SXERR_SYNTAX;` |
|         - |  7616 | `			}` |
|         - |  7617 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|         - |  7618 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|         - |  7619 | `			 * path below leaves *pnType untouched when there is no non-null` |
|         - |  7620 | `			 * atom, so set it here. */` |
|         7 |  7621 | `			*pnType = MEMOBJ_NULL;` |
|         3 |  7622 | `		}` |
|         - |  7623 | `	}` |
|         - |  7624 | `	/* Compute nullability flag */` |
|    127687 |  7625 | `	if( bShortNullable \|\| bExplicitNull ){` |
|       133 |  7626 | `		*piTypeFlags \|= iNullableFlag;` |
|        64 |  7627 | `	}` |
|         - |  7628 | `	/* Build canonical type text */` |
|    127687 |  7629 | `	if( pTypeText ){` |
|         - |  7630 | `		SyBlob sBlob;` |
|    127687 |  7631 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|    191479 |  7632 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|     63841 |  7633 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|    127687 |  7634 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|    191249 |  7635 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    127496 |  7636 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|    127501 |  7637 | `			if( zDup ){` |
|    127501 |  7638 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|     63748 |  7639 | `			}` |
|     63748 |  7640 | `		}` |
|    127687 |  7641 | `		SyBlobRelease(&sBlob);` |
|     63841 |  7642 | `	}` |
|         - |  7643 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|         - |  7644 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|         - |  7645 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|         - |  7646 | `	{` |
|    127687 |  7647 | `		int nNonNull = 0;` |
|    127687 |  7648 | `		int iNonNullIdx = -1;` |
|         - |  7649 | `		int i;` |
|    255521 |  7650 | `		for( i = 0; i < nAtoms; i++ ){` |
|    127839 |  7651 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    127809 |  7652 | `				nNonNull++;` |
|    127809 |  7653 | `				iNonNullIdx = i;` |
|     63902 |  7654 | `			}` |
|     63922 |  7655 | `		}` |
|    127687 |  7656 | `		if( nNonNull <= 1 ){` |
|         - |  7657 | `			/* Fast path: store as single type. */` |
|    127581 |  7658 | `			if( iNonNullIdx >= 0 ){` |
|    127575 |  7659 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|    127575 |  7660 | `				if( pA->nType == SXU32_HIGH ){` |
|     57698 |  7661 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     19231 |  7662 | `						pA->sClass.zString, pA->sClass.nByte);` |
|     38467 |  7663 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|     38467 |  7664 | `					*pnType = SXU32_HIGH;` |
|     38467 |  7665 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|    108344 |  7666 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|       175 |  7667 | `					*pnType = MEMOBJ_VOID;` |
|     89028 |  7668 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|        18 |  7669 | `					*pnType = MEMOBJ_NEVER;` |
|        10 |  7670 | `				}else{` |
|     88927 |  7671 | `					*pnType = pA->nType;` |
|         - |  7672 | `				}` |
|     63785 |  7673 | `			}` |
|     63793 |  7674 | `		}else{` |
|         - |  7675 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|       111 |  7676 | `			*piTypeFlags \|= iUnionFlag;` |
|       355 |  7677 | `			for( i = 0; i < nAtoms; i++ ){` |
|         - |  7678 | `				ph7_type_alt sAlt;` |
|       249 |  7679 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       239 |  7680 | `				SyZero(&sAlt, sizeof(sAlt));` |
|       239 |  7681 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|       239 |  7682 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       146 |  7683 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        47 |  7684 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        99 |  7685 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|        99 |  7686 | `					sAlt.nType = SXU32_HIGH;` |
|        99 |  7687 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|        52 |  7688 | `				}else{` |
|       145 |  7689 | `					sAlt.nType = aAtoms[i].nType;` |
|       145 |  7690 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|         - |  7691 | `				}` |
|       239 |  7692 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|       122 |  7693 | `			}` |
|         - |  7694 | `		}` |
|         - |  7695 | `	}` |
|    127687 |  7696 | `	return SXRET_OK;` |
|     63854 |  7697 | `}` |
|         - |  7698 |  |
|         - |  7699 | `/*` |
|         - |  7700 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|         - |  7701 | `` * pGen->pIn should point to the token after `)`.`` |
|         - |  7702 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|         - |  7703 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|         - |  7704 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|         - |  7705 | `` *          and union types `: T\|U`.`` |
|         - |  7706 | ` */` |
|   2888350 |  7707 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|         5 |  7708 | `{` |
|   2888355 |  7709 | `	sxi32 iFlags = 0;` |
|         - |  7710 | `	sxi32 rc;` |
|         - |  7711 | `	sxu32 nLine;` |
|   2888355 |  7712 | `	pFunc->nReturnType = 0;` |
|   2888355 |  7713 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|   2888355 |  7714 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|         - |  7715 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|         - |  7716 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|         - |  7717 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|         - |  7718 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|         - |  7719 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|   2888355 |  7720 | `	SySetReset(&pFunc->aReturnUnion);` |
|   2888355 |  7721 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|   2888355 |  7722 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|   2876159 |  7723 | `		return SXRET_OK;` |
|         - |  7724 | `	}` |
|     12201 |  7725 | `	pGen->pIn++; /* Skip ':' */` |
|     12201 |  7726 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7727 | `		return SXRET_OK;` |
|         - |  7728 | `	}` |
|     12201 |  7729 | `	nLine = pGen->pIn->nLine;` |
|     12201 |  7730 | `	rc = GenStateParseUnionTypeDecl(` |
|      6098 |  7731 | `		pGen,` |
|      6098 |  7732 | `		&pFunc->nReturnType,` |
|      6098 |  7733 | `		&pFunc->sReturnClass,` |
|      6098 |  7734 | `		&pFunc->aReturnUnion,` |
|         - |  7735 | `		&iFlags,` |
|      6098 |  7736 | `		&pFunc->sReturnTypeName,` |
|         - |  7737 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|         - |  7738 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|         - |  7739 | `		/* iUnionFlag */ 0,` |
|         - |  7740 | `		/* bAllowVoid */ 1,` |
|      6098 |  7741 | `		nLine);` |
|     12201 |  7742 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  7743 | `		return SXERR_ABORT;` |
|         - |  7744 | `	}` |
|     12201 |  7745 | `	if( rc == SXERR_CORRUPT ){` |
|         - |  7746 | `		/* Error already reported */` |
|       ! 0 |  7747 | `		return SXERR_SYNTAX;` |
|         - |  7748 | `	}` |
|     12201 |  7749 | `	if( rc == SXERR_SYNTAX ){` |
|         9 |  7750 | `		if( pGen->pIn < pGen->pEnd ){` |
|        12 |  7751 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - |  7752 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|         6 |  7753 | `				&pGen->pIn->sData);` |
|         6 |  7754 | `		}else{` |
|       ! 0 |  7755 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|         - |  7756 | `				"syntax error, unexpected end of file in return type declaration");` |
|         - |  7757 | `		}` |
|         9 |  7758 | `		return SXERR_SYNTAX;` |
|         - |  7759 | `	}` |
|     12195 |  7760 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     12195 |  7761 | `	return SXRET_OK;` |
|   1444180 |  7762 | `}` |
|         - |  7763 |  |
|    491058 |  7764 | `static sxi32 GenStateCompileFunc(` |
|         - |  7765 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  7766 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|         - |  7767 | `	sxi32 iFlags,        /* Control flags */` |
|         - |  7768 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|         - |  7769 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|         - |  7770 | `	)` |
|         5 |  7771 | `{` |
|         - |  7772 | `	ph7_vm_func *pFunc;` |
|         - |  7773 | `	SyToken *pEnd;` |
|         - |  7774 | `	sxu32 nLine;` |
|         - |  7775 | `	char *zName;` |
|         - |  7776 | `	sxi32 rc;` |
|         - |  7777 | `	/* Extract line number */` |
|    491063 |  7778 | `	nLine = pGen->pIn->nLine;` |
|         - |  7779 | `	/* Jump the left parenthesis '(' */` |
|    491063 |  7780 | `	pGen->pIn++;` |
|         - |  7781 | `	/* Delimit the function signature */` |
|    491063 |  7782 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    491063 |  7783 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  7784 | `		/* Syntax error */` |
|         9 |  7785 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable");` |
|         3 |  7786 | `		(void)pName;` |
|         9 |  7787 | `		if( rc == SXERR_ABORT ){` |
|         - |  7788 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  7789 | `			return SXERR_ABORT;` |
|         - |  7790 | `		}` |
|         9 |  7791 | `		pGen->pIn = pGen->pEnd;` |
|         9 |  7792 | `		return SXRET_OK;` |
|         - |  7793 | `	}` |
|         - |  7794 | `	/* Create the function state */` |
|    491057 |  7795 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|    491057 |  7796 | `	if( pFunc == 0 ){` |
|       ! 0 |  7797 | `		goto OutOfMem;` |
|         - |  7798 | `	}` |
|         - |  7799 | `	/* Build the function name, prepending namespace if active */` |
|    491065 |  7800 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|         - |  7801 | `		SyBlob sFQN;` |
|         - |  7802 | `		sxu32 nLen;` |
|        18 |  7803 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        18 |  7804 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        18 |  7805 | `		SyBlobAppend(&sFQN,"\\",1);` |
|        18 |  7806 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|        18 |  7807 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|        18 |  7808 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|        18 |  7809 | `		SyBlobRelease(&sFQN);` |
|        18 |  7810 | `		if( zName == 0 ){` |
|       ! 0 |  7811 | `			goto OutOfMem;` |
|         - |  7812 | `		}` |
|        18 |  7813 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|        10 |  7814 | `	}else{` |
|    491041 |  7815 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    491041 |  7816 | `		if( zName == 0 ){` |
|       ! 0 |  7817 | `			goto OutOfMem;` |
|         - |  7818 | `		}` |
|    491041 |  7819 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|         - |  7820 | `	}` |
|         - |  7821 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|         - |  7822 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|    491057 |  7823 | `	pFunc->nLine = nLine;` |
|    491057 |  7824 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|    491057 |  7825 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  7826 | `		return SXERR_ABORT;` |
|         - |  7827 | `	}` |
|    491057 |  7828 | `	if( pGen->pIn < pEnd ){` |
|         - |  7829 | `		/* Collect function arguments */` |
|    425171 |  7830 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|    425171 |  7831 | `		if( rc == SXERR_ABORT ){` |
|         - |  7832 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  7833 | `			return SXERR_ABORT;` |
|         - |  7834 | `		}` |
|    212583 |  7835 | `	}` |
|         - |  7836 | `	/* Point past ')' and parse optional return type ': type' */` |
|    491057 |  7837 | `	pGen->pIn = &pEnd[1];` |
|         - |  7838 | `	{` |
|    491057 |  7839 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|    491057 |  7840 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 |  7841 | `			return SXERR_ABORT;` |
|    491057 |  7842 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|         9 |  7843 | `			return SXERR_SYNTAX;` |
|         - |  7844 | `		}` |
|         - |  7845 | `	}` |
|    491051 |  7846 | `	if( bHandleClosure ){` |
|         - |  7847 | `		ph7_vm_func_closure_env sEnv;` |
|       575 |  7848 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|       570 |  7849 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       334 |  7850 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|        93 |  7851 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  7852 | `				/* Closure,record environment variable */` |
|        93 |  7853 | `				pGen->pIn++;` |
|        93 |  7854 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  7855 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|       ! 0 |  7856 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  7857 | `						return SXERR_ABORT;` |
|         - |  7858 | `					}` |
|       ! 0 |  7859 | `				}` |
|        93 |  7860 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|         - |  7861 | `				/* Compile until we hit the first closing parenthesis */` |
|       191 |  7862 | `				while( pGen->pIn < pGen->pEnd ){` |
|       191 |  7863 | `					int iFlagsLocal = 0;` |
|       191 |  7864 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|        93 |  7865 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|        93 |  7866 | `						break;` |
|         - |  7867 | `					}` |
|       103 |  7868 | `					nLineLocal = pGen->pIn->nLine;` |
|       103 |  7869 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - |  7870 | `						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry` |
|         - |  7871 | `						 * to the variable's memory slot instead of copying its value. */` |
|        55 |  7872 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|        55 |  7873 | `						pGen->pIn++;` |
|        27 |  7874 | `					}` |
|        98 |  7875 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|       103 |  7876 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  7877 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|         - |  7878 | `								"Closure: Unexpected token. Expecting a variable name");` |
|       ! 0 |  7879 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 |  7880 | `								return SXERR_ABORT;` |
|         - |  7881 | `							}` |
|         - |  7882 | `							/* Find the closing parenthesis */` |
|       ! 0 |  7883 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  7884 | `								pGen->pIn++;` |
|       ! 0 |  7885 | `							}` |
|       ! 0 |  7886 | `							if(pGen->pIn < pGen->pEnd){` |
|       ! 0 |  7887 | `								pGen->pIn++;` |
|       ! 0 |  7888 | `							}` |
|       ! 0 |  7889 | `							break;` |
|         - |  7890 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|       ! 0 |  7891 | `					}else{` |
|         - |  7892 | `						SyString *pNameLocal;` |
|         - |  7893 | `						char *zDup;` |
|         - |  7894 | `						/* Duplicate variable name */` |
|       103 |  7895 | `						pNameLocal = &pGen->pIn[1].sData;` |
|       103 |  7896 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|       103 |  7897 | `						if( zDup ){` |
|         - |  7898 | `							/* Zero the structure */` |
|       103 |  7899 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       103 |  7900 | `							sEnv.iFlags = iFlagsLocal;` |
|       103 |  7901 | `							sEnv.nIdx = SXU32_HIGH;` |
|       103 |  7902 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       103 |  7903 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|       118 |  7904 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|        30 |  7905 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|       ! 0 |  7906 | `									got_this = 1;` |
|       ! 0 |  7907 | `							}` |
|         - |  7908 | `							/* Save imported variable */` |
|       103 |  7909 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|        54 |  7910 | `						}else{` |
|       ! 0 |  7911 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  7912 | `							 return SXERR_ABORT;` |
|         - |  7913 | `						}` |
|         - |  7914 | `					}` |
|       103 |  7915 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|       115 |  7916 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  7917 | `						/* Ignore trailing commas */` |
|        13 |  7918 | `						pGen->pIn++;` |
|         1 |  7919 | `					}` |
|         5 |  7920 | `				}` |
|         - |  7921 | `				/* php 7.1+: the return type follows the use clause —` |
|         - |  7922 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|         - |  7923 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|         - |  7924 | `				 * so an unconditional call would wipe a type parsed at the` |
|         - |  7925 | `				 * legacy pre-use position. */` |
|        93 |  7926 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|         7 |  7927 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|         7 |  7928 | `					if( rcRt2 == SXERR_ABORT ){` |
|       ! 0 |  7929 | `						return SXERR_ABORT;` |
|         7 |  7930 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|       ! 0 |  7931 | `						return SXERR_SYNTAX;` |
|         - |  7932 | `					}` |
|         3 |  7933 | `				}` |
|        44 |  7934 | `		}` |
|       575 |  7935 | `		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){` |
|         - |  7936 | `			/* Make the $this variable [Current processed Object (class instance)]` |
|         - |  7937 | `			 * available to the closure environment — for EVERY non-static` |
|         - |  7938 | `			 * anonymous function, use list or not (php binds $this to any` |
|         - |  7939 | ``			 * closure declared in a method; pre-fix only `use (...)` closures`` |
|         - |  7940 | `			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of` |
|         - |  7941 | `			 * a global-scope closure is silently dropped at install. A static` |
|         - |  7942 | `			 * closure never binds $this (php). */` |
|       565 |  7943 | `			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       565 |  7944 | `			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|       565 |  7945 | `			sEnv.nIdx = SXU32_HIGH;` |
|       565 |  7946 | `			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       565 |  7947 | `			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|       565 |  7948 | `			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       280 |  7949 | `		}` |
|       575 |  7950 | `		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|         - |  7951 | `			/* Mark as closure */` |
|       567 |  7952 | `			pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       281 |  7953 | `		}` |
|       285 |  7954 | `	}` |
|         - |  7955 | `	/* Compile the body */` |
|    491051 |  7956 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|    491051 |  7957 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  7958 | `		return SXERR_ABORT;` |
|         - |  7959 | `	}` |
|         - |  7960 | `	/* The cursor sits just past the body's closing brace */` |
|    491051 |  7961 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|    491051 |  7962 | `	if( ppFunc ){` |
|    491051 |  7963 | `		*ppFunc = pFunc;` |
|    245523 |  7964 | `	}` |
|    491051 |  7965 | `	rc = SXRET_OK;` |
|    491051 |  7966 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|         - |  7967 | `		/* Finally register the function */` |
|    490489 |  7968 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|    245242 |  7969 | `	}` |
|    491051 |  7970 | `	if( rc == SXRET_OK ){` |
|    491051 |  7971 | `		return SXRET_OK;` |
|         - |  7972 | `	}` |
|         - |  7973 | `	/* Fall through if something goes wrong */` |
|       ! 0 |  7974 | `OutOfMem:` |
|         - |  7975 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  7976 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  7977 | `	 */` |
|       ! 0 |  7978 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 |  7979 | `	return SXERR_ABORT;` |
|    245534 |  7980 | `}` |
|         - |  7981 | `/*` |
|         - |  7982 | ` * Compile a standard PHP function.` |
|         - |  7983 | ` *  Refer to the block-comment above for more information.` |
|         - |  7984 | ` */` |
|    490496 |  7985 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|         5 |  7986 | `{` |
|         - |  7987 | `	SyString *pName;` |
|         - |  7988 | `	sxi32 iFlags;` |
|         - |  7989 | `	sxu32 nKwLine;` |
|         - |  7990 | `	sxu32 nLine;` |
|         - |  7991 | `	sxi32 rc;` |
|         - |  7992 |  |
|    490501 |  7993 | `	nLine = pGen->pIn->nLine;` |
|    490501 |  7994 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|    490501 |  7995 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|    490501 |  7996 | `	iFlags = 0;` |
|    490501 |  7997 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - |  7998 | `		/* Return by reference,remember that */` |
|        12 |  7999 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|         - |  8000 | `		/* Jump the '&' token */` |
|        12 |  8001 | `		pGen->pIn++;` |
|         5 |  8002 | `	}` |
|    490501 |  8003 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  8004 | `		/* Invalid function name */` |
|         8 |  8005 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         8 |  8006 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8007 | `			return SXERR_ABORT;` |
|         - |  8008 | `		}` |
|         - |  8009 | `		/* Sychronize with the next semi-colon or braces*/` |
|        22 |  8010 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        16 |  8011 | `			pGen->pIn++;` |
|         2 |  8012 | `		}` |
|         8 |  8013 | `		return SXRET_OK;` |
|         - |  8014 | `	}` |
|    490495 |  8015 | `	pName = &pGen->pIn->sData;` |
|    490495 |  8016 | `	nLine = pGen->pIn->nLine;` |
|         - |  8017 | `	/* Jump the function name */` |
|    490495 |  8018 | `	pGen->pIn++;` |
|    490495 |  8019 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  8020 | `		/* Syntax error */` |
|         3 |  8021 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         3 |  8022 | `		if( rc == SXERR_ABORT ){` |
|         - |  8023 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8024 | `			return SXERR_ABORT;` |
|         - |  8025 | `		}` |
|         - |  8026 | `		/* Sychronize with the next semi-colon or '{' */` |
|         3 |  8027 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  8028 | `			pGen->pIn++;` |
|       ! 0 |  8029 | `		}` |
|         3 |  8030 | `		return SXRET_OK;` |
|         - |  8031 | `	}` |
|         - |  8032 | `	/* Compile function body */` |
|         - |  8033 | `	{` |
|    490493 |  8034 | `		ph7_vm_func *pFuncState = 0;` |
|    490493 |  8035 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);` |
|    490493 |  8036 | `		if( pFuncState ){` |
|         - |  8037 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|    490481 |  8038 | `			pFuncState->nLine = nKwLine;` |
|    245238 |  8039 | `		}` |
|         - |  8040 | `	}` |
|    490493 |  8041 | `	return rc;` |
|    245253 |  8042 | `}` |
|         - |  8043 | `/*` |
|         - |  8044 | ` * Extract the visibility level associated with a given keyword.` |
|         - |  8045 | ` * According to the PHP language reference manual` |
|         - |  8046 | ` *  Visibility:` |
|         - |  8047 | ` *  The visibility of a property or method can be defined by prefixing` |
|         - |  8048 | ` *  the declaration with the keywords public, protected or private.` |
|         - |  8049 | ` *  Class members declared public can be accessed everywhere.` |
|         - |  8050 | ` *  Members declared protected can be accessed only within the class` |
|         - |  8051 | ` *  itself and by inherited and parent classes. Members declared as private` |
|         - |  8052 | ` *  may only be accessed by the class that defines the member.` |
|         - |  8053 | ` */` |
|   3146738 |  8054 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|         5 |  8055 | `{` |
|   3146743 |  8056 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    256143 |  8057 | `		return PH7_CLASS_PROT_PRIVATE;` |
|   2890605 |  8058 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|    191089 |  8059 | `		return PH7_CLASS_PROT_PROTECTED;` |
|         - |  8060 | `	}` |
|         - |  8061 | `	/* Assume public by default */` |
|   2699521 |  8062 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   1573374 |  8063 | `}` |
|         - |  8064 | `/*` |
|         - |  8065 | ` * Compile a class constant.` |
|         - |  8066 | ` * According to the PHP language reference manual` |
|         - |  8067 | ` *  Class Constants` |
|         - |  8068 | ` *   It is possible to define constant values on a per-class basis remaining` |
|         - |  8069 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|         - |  8070 | ` *   you don't use the $ symbol to declare or use them.` |
|         - |  8071 | ` *   The value must be a constant expression, not (for example) a variable,` |
|         - |  8072 | ` *   a property, a result of a mathematical operation, or a function call.` |
|         - |  8073 | ` *   It's also possible for interfaces to have constants.` |
|         - |  8074 | ` * Symisc eXtension.` |
|         - |  8075 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|         - |  8076 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  8077 | ` *  Example:` |
|         - |  8078 | ` *   class Test{` |
|         - |  8079 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|         - |  8080 | ` *   };` |
|         - |  8081 | ` *   var_dump(TEST::MyConst);` |
|         - |  8082 | ` *   Refer to the official documentation for more information on the powerful extension` |
|         - |  8083 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|         - |  8084 | ` */` |
|         - |  8085 | `/*` |
|         - |  8086 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|         - |  8087 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|         - |  8088 | ` * token immediately followed by '='. Anything else with a leading type token` |
|         - |  8089 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|         - |  8090 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|         - |  8091 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|         - |  8092 | ` */` |
|    290504 |  8093 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|         5 |  8094 | `{` |
|         - |  8095 | `	SyToken *p0, *p1;` |
|    290509 |  8096 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  8097 | `		return 0;` |
|         - |  8098 | `	}` |
|    290509 |  8099 | `	p0 = pGen->pIn;` |
|         - |  8100 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|    290509 |  8101 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|       ! 0 |  8102 | `		return 1;` |
|         - |  8103 | `	}` |
|    290509 |  8104 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|         5 |  8105 | `		return 1;` |
|         - |  8106 | `	}` |
|         - |  8107 | `	/* A name-like first token begins a type only when followed by another` |
|         - |  8108 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|         - |  8109 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|    290505 |  8110 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|    290505 |  8111 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|    290505 |  8112 | `		if( p1 ){` |
|    290505 |  8113 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|        34 |  8114 | `				return 1;` |
|         - |  8115 | `			}` |
|    290475 |  8116 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|         5 |  8117 | `				return 1;` |
|         - |  8118 | `			}` |
|    145233 |  8119 | `		}` |
|    145233 |  8120 | `	}` |
|    290471 |  8121 | `	return 0;` |
|    145257 |  8122 | `}` |
|         - |  8123 | `/*` |
|         - |  8124 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|         - |  8125 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|         - |  8126 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|         - |  8127 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|         - |  8128 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|         - |  8129 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|         - |  8130 | ` * Peek only; never consumes tokens.` |
|         - |  8131 | ` */` |
|        24 |  8132 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|         4 |  8133 | `{` |
|        28 |  8134 | `	SyToken *p = pGen->pIn;` |
|        39 |  8135 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        20 |  8136 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|         3 |  8137 | `		p++; /* skip leading unary sign(s) */` |
|         1 |  8138 | `	}` |
|        28 |  8139 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|        23 |  8140 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|         - |  8141 | `	}` |
|         6 |  8142 | `	p++;` |
|         - |  8143 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|         6 |  8144 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|        16 |  8145 | `}` |
|         - |  8146 | `/*` |
|         - |  8147 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|         - |  8148 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|         - |  8149 | `` * `$o->new`), not a `new` expression.`` |
|         - |  8150 | ` */` |
|       110 |  8151 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|         4 |  8152 | `{` |
|         - |  8153 | `	sxi32 iOp;` |
|       114 |  8154 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|        11 |  8155 | `		return 0;` |
|         - |  8156 | `	}` |
|       104 |  8157 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|       104 |  8158 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|        59 |  8159 | `}` |
|         - |  8160 | `/*` |
|         - |  8161 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|         - |  8162 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|         - |  8163 | ` * interface-constant and (instance/static) property-default initializers` |
|         - |  8164 | ` * ("New expressions are not supported in this context") while still allowing it` |
|         - |  8165 | ` * in global constants, parameter defaults and static-local initializers (which` |
|         - |  8166 | ` * are compiled by different functions and left untouched). The scan is` |
|         - |  8167 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|         - |  8168 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|         - |  8169 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|         - |  8170 | ` *` |
|         - |  8171 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|         - |  8172 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|         - |  8173 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|         - |  8174 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|         - |  8175 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|         - |  8176 | ` */` |
|    627372 |  8177 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|         5 |  8178 | `{` |
|    627377 |  8179 | `	SyToken *p = pGen->pIn;` |
|    627377 |  8180 | `	int iDepth = 0;` |
|   1657525 |  8181 | `	while( p < pGen->pEnd ){` |
|   1657525 |  8182 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    627325 |  8183 | `			break; /* end of this initializer */` |
|         - |  8184 | `		}` |
|   1030200 |  8185 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|    518942 |  8186 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      7674 |  8187 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|         - |  8188 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|         - |  8189 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|         - |  8190 | `			 * expression. */` |
|         3 |  8191 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|         3 |  8192 | `			p++;` |
|         3 |  8193 | `			if( bArrow ){` |
|         - |  8194 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|         - |  8195 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|         3 |  8196 | `				int iBase = iDepth;` |
|        17 |  8197 | `				while( p < pGen->pEnd ){` |
|        17 |  8198 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         5 |  8199 | `						iDepth++;` |
|        15 |  8200 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         5 |  8201 | `						if( iDepth <= iBase ){` |
|       ! 0 |  8202 | `							break; /* closes an enclosing group, not the fn's own */` |
|         - |  8203 | `						}` |
|         5 |  8204 | `						iDepth--;` |
|        11 |  8205 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|         3 |  8206 | `						break;` |
|         - |  8207 | `					}` |
|        15 |  8208 | `					p++;` |
|         1 |  8209 | `				}` |
|         2 |  8210 | `			}else{` |
|         - |  8211 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|         - |  8212 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|         - |  8213 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|         - |  8214 | `				 * then skip the balanced brace block. */` |
|       ! 0 |  8215 | `				int iLocal = 0;` |
|       ! 0 |  8216 | `				while( p < pGen->pEnd ){` |
|       ! 0 |  8217 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|       ! 0 |  8218 | `						break; /* body brace */` |
|         - |  8219 | `					}` |
|       ! 0 |  8220 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       ! 0 |  8221 | `						iLocal++;` |
|       ! 0 |  8222 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       ! 0 |  8223 | `						if( iLocal > 0 ){` |
|       ! 0 |  8224 | `							iLocal--;` |
|       ! 0 |  8225 | `						}` |
|       ! 0 |  8226 | `					}` |
|       ! 0 |  8227 | `					p++;` |
|       ! 0 |  8228 | `				}` |
|       ! 0 |  8229 | `				if( p < pGen->pEnd ){` |
|       ! 0 |  8230 | `					int iBrace = 0; /* p is on the body '{' */` |
|       ! 0 |  8231 | `					while( p < pGen->pEnd ){` |
|       ! 0 |  8232 | `						if( p->nType & PH7_TK_OCB ){` |
|       ! 0 |  8233 | `							iBrace++;` |
|       ! 0 |  8234 | `						}else if( p->nType & PH7_TK_CCB ){` |
|       ! 0 |  8235 | `							iBrace--;` |
|       ! 0 |  8236 | `							if( iBrace == 0 ){` |
|       ! 0 |  8237 | `								p++;` |
|       ! 0 |  8238 | `								break;` |
|         - |  8239 | `							}` |
|       ! 0 |  8240 | `						}` |
|       ! 0 |  8241 | `						p++;` |
|       ! 0 |  8242 | `					}` |
|       ! 0 |  8243 | `				}` |
|         - |  8244 | `			}` |
|         3 |  8245 | `			continue;` |
|         - |  8246 | `		}` |
|   1030203 |  8247 | `		if( p->nType & PH7_TK_OCB ){` |
|        45 |  8248 | `			if( iDepth == 0 ){` |
|         - |  8249 | `				/* A depth-0 '{' can only open a PHP 8.4 property-hook list` |
|         - |  8250 | ``				 * (`public T $x = default { get …; }`): the default expression`` |
|         - |  8251 | ``				 * ends here. A `new` inside a hook BODY runs at access time and`` |
|         - |  8252 | `				 * is legal — don't scan into it. */` |
|        45 |  8253 | `				break;` |
|         - |  8254 | `			}` |
|       ! 0 |  8255 | `			iDepth++;` |
|   1030159 |  8256 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     42117 |  8257 | `			iDepth++;` |
|   1009103 |  8258 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     42115 |  8259 | `			if( iDepth > 0 ){` |
|     42115 |  8260 | `				iDepth--;` |
|     21055 |  8261 | `			}` |
|    966992 |  8262 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|    346795 |  8263 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|         - |  8264 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|         - |  8265 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|         - |  8266 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|        11 |  8267 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|        11 |  8268 | `				return 1;` |
|         - |  8269 | `			}` |
|       ! 0 |  8270 | `		}` |
|   1030151 |  8271 | `		p++;` |
|         5 |  8272 | `	}` |
|    627369 |  8273 | `	return 0;` |
|    313691 |  8274 | `}` |
|         - |  8275 | `/*` |
|         - |  8276 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|         - |  8277 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|         - |  8278 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|         - |  8279 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|         - |  8280 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|         - |  8281 | ` * share the same backing.` |
|         - |  8282 | ` */` |
|       362 |  8283 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|         - |  8284 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|         5 |  8285 | `{` |
|       367 |  8286 | `	pAttr->nType = nType;` |
|       367 |  8287 | `	pAttr->sClass = *pClass;` |
|       367 |  8288 | `	pAttr->sTypeName = *pTypeName;` |
|       367 |  8289 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  8290 | `		sxu32 i;` |
|        73 |  8291 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        51 |  8292 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|        51 |  8293 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|        28 |  8294 | `		}` |
|        11 |  8295 | `	}` |
|       367 |  8296 | `}` |
|    290504 |  8297 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  8298 | `{` |
|    290509 |  8299 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  8300 | `	SySet *pInstrContainer;` |
|         - |  8301 | `	ph7_class_attr *pCons;` |
|         - |  8302 | `	SyString *pName;` |
|         - |  8303 | `	sxi32 rc;` |
|    290509 |  8304 | `	sxu32 nType = 0;` |
|         - |  8305 | `	SyString sTypeClass;` |
|         - |  8306 | `	SyString sTypeText;` |
|         - |  8307 | `	SySet aUnionAlts;` |
|    290509 |  8308 | `	sxi32 iTypeFlags = 0;` |
|    290509 |  8309 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    290509 |  8310 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    290509 |  8311 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  8312 | `	/* Extract visibility level */` |
|    290509 |  8313 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  8314 | `	/* Mark as constant */` |
|    290509 |  8315 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|    290509 |  8316 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|         - |  8317 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|         - |  8318 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|    290528 |  8319 | `	if( GenStateClassConstHasType(pGen) ){` |
|        61 |  8320 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|        38 |  8321 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|         - |  8322 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|         - |  8323 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|         - |  8324 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|         - |  8325 | `		 * and success paths release. */` |
|        42 |  8326 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  8327 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  8328 | `			goto Synchronize;` |
|        42 |  8329 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  8330 | `			return SXERR_ABORT;` |
|        42 |  8331 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  8332 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  8333 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|       ! 0 |  8334 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8335 | `				return SXERR_ABORT;` |
|         - |  8336 | `			}` |
|       ! 0 |  8337 | `			goto Synchronize;` |
|         - |  8338 | `		}` |
|        42 |  8339 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        19 |  8340 | `	}` |
|    145252 |  8341 | `loop:` |
|    290511 |  8342 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - |  8343 | `		/* Invalid constant name */` |
|       ! 0 |  8344 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|       ! 0 |  8345 | `		if( rc == SXERR_ABORT ){` |
|         - |  8346 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8347 | `			return SXERR_ABORT;` |
|         - |  8348 | `		}` |
|       ! 0 |  8349 | `		goto Synchronize;` |
|         - |  8350 | `	}` |
|         - |  8351 | `	/* Peek constant name */` |
|    290511 |  8352 | `	pName = &pGen->pIn->sData;` |
|         - |  8353 | `	/* Make sure the constant name isn't reserved */` |
|    290511 |  8354 | `	if( GenStateIsReservedConstant(pName) ){` |
|         - |  8355 | `		/* Reserved constant name */` |
|       ! 0 |  8356 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|       ! 0 |  8357 | `		if( rc == SXERR_ABORT ){` |
|         - |  8358 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8359 | `			return SXERR_ABORT;` |
|         - |  8360 | `		}` |
|       ! 0 |  8361 | `		goto Synchronize;` |
|         - |  8362 | `	}` |
|         - |  8363 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|    290511 |  8364 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        61 |  8365 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|        38 |  8366 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|        19 |  8367 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|        42 |  8368 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8369 | `			return SXERR_ABORT;` |
|        42 |  8370 | `		}else if( rc != SXRET_OK ){` |
|         3 |  8371 | `			goto Synchronize;` |
|         - |  8372 | `		}` |
|        18 |  8373 | `	}` |
|         - |  8374 | `	/* Advance the stream cursor */` |
|    290509 |  8375 | `	pGen->pIn++;` |
|    290509 |  8376 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|         - |  8377 | `		/* Invalid declaration */` |
|       ! 0 |  8378 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|       ! 0 |  8379 | `		if( rc == SXERR_ABORT ){` |
|         - |  8380 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8381 | `			return SXERR_ABORT;` |
|         - |  8382 | `		}` |
|       ! 0 |  8383 | `		goto Synchronize;` |
|         - |  8384 | `	}` |
|    290509 |  8385 | `	pGen->pIn++; /* Jump the equal sign */` |
|         - |  8386 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|         - |  8387 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|         - |  8388 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|         - |  8389 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|    290504 |  8390 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|        39 |  8391 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|         8 |  8392 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8393 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|         2 |  8394 | `			&pClass->sName,pName,&sTypeText);` |
|         6 |  8395 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8396 | `			return SXERR_ABORT;` |
|         - |  8397 | `		}` |
|         6 |  8398 | `		goto Synchronize;` |
|         - |  8399 | `	}` |
|         - |  8400 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|         - |  8401 | `	 * constant initializer ("New expressions are not supported in this context").` |
|         - |  8402 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|    290505 |  8403 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|         5 |  8404 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8405 | `			"New expressions are not supported in this context");` |
|         5 |  8406 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8407 | `			return SXERR_ABORT;` |
|         - |  8408 | `		}` |
|         5 |  8409 | `		goto Synchronize;` |
|         - |  8410 | `	}` |
|         - |  8411 | `	/* Allocate a new class attribute */` |
|    290501 |  8412 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    290501 |  8413 | `	if( pCons ){` |
|    290501 |  8414 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|    290501 |  8415 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8416 | `			return SXERR_ABORT;` |
|         - |  8417 | `		}` |
|    145248 |  8418 | `	}` |
|    290501 |  8419 | `	if( pCons == 0 ){` |
|       ! 0 |  8420 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8421 | `		return SXERR_ABORT;` |
|         - |  8422 | `	}` |
|    290501 |  8423 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        35 |  8424 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|        16 |  8425 | `	}` |
|         - |  8426 | `	/* Swap bytecode container */` |
|    290501 |  8427 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    290501 |  8428 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|         - |  8429 | `	/* Compile constant value.` |
|         - |  8430 | `	 */` |
|    290501 |  8431 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    290501 |  8432 | `	if( rc == SXERR_EMPTY ){` |
|         3 |  8433 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|         3 |  8434 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8435 | `			return SXERR_ABORT;` |
|         - |  8436 | `		}` |
|         1 |  8437 | `	}` |
|         - |  8438 | `	/* Emit the done instruction */` |
|    290501 |  8439 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    290501 |  8440 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    290501 |  8441 | `	if( rc == SXERR_ABORT ){` |
|         - |  8442 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  8443 | `		return SXERR_ABORT;` |
|         - |  8444 | `	}` |
|         - |  8445 | `	/* All done,install the constant */` |
|    290501 |  8446 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|    290501 |  8447 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8448 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8449 | `		return SXERR_ABORT;` |
|         - |  8450 | `	}` |
|    290501 |  8451 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  8452 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|         3 |  8453 | `		pGen->pIn++; /* Jump the comma */` |
|         3 |  8454 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 |  8455 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 |  8456 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 |  8457 | `				pTok--;` |
|       ! 0 |  8458 | `			}` |
|       ! 0 |  8459 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  8460 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|       ! 0 |  8461 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 |  8462 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8463 | `				return SXERR_ABORT;` |
|         - |  8464 | `			}` |
|       ! 0 |  8465 | `		}else{` |
|         3 |  8466 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|         3 |  8467 | `				goto loop;` |
|         - |  8468 | `			}` |
|         - |  8469 | `		}` |
|       ! 0 |  8470 | `	}` |
|    290499 |  8471 | `	SySetRelease(&aUnionAlts);` |
|    290499 |  8472 | `	return SXRET_OK;` |
|         5 |  8473 | `Synchronize:` |
|        13 |  8474 | `	SySetRelease(&aUnionAlts);` |
|         - |  8475 | `	/* Synchronize with the first semi-colon */` |
|        45 |  8476 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        35 |  8477 | `		pGen->pIn++;` |
|         3 |  8478 | `	}` |
|        13 |  8479 | `	return SXERR_CORRUPT;` |
|    145257 |  8480 | `}` |
|         - |  8481 | `/*` |
|         - |  8482 | ` * complie a class attribute or Properties in the PHP jargon.` |
|         - |  8483 | ` * According to the PHP language reference manual` |
|         - |  8484 | ` *  Properties` |
|         - |  8485 | ` *  Class member variables are called "properties". You may also see them referred` |
|         - |  8486 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|         - |  8487 | ` *  of this reference we will use "properties". They are defined by using one` |
|         - |  8488 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|         - |  8489 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|         - |  8490 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|         - |  8491 | ` *  and must not depend on run-time information in order to be evaluated.` |
|         - |  8492 | ` * Symisc eXtension.` |
|         - |  8493 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|         - |  8494 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  8495 | ` *  Example:` |
|         - |  8496 | ` *   class Test{` |
|         - |  8497 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|         - |  8498 | ` *   };` |
|         - |  8499 | ` *   var_dump(TEST::myVar);` |
|         - |  8500 | ` *   Refer to the official documentation for more information on the powerful extension` |
|         - |  8501 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|         - |  8502 | ` */` |
|         - |  8503 | `/*` |
|         - |  8504 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|         - |  8505 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|         - |  8506 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|         - |  8507 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|         - |  8508 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|         - |  8509 | ` */` |
|   2351064 |  8510 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|         5 |  8511 | `{` |
|   2351069 |  8512 | `	SyToken *p = pStart;` |
|   2351069 |  8513 | `	int bFirst = 1;` |
|   2351069 |  8514 | `	if( p >= pEnd ) return 0;` |
|         - |  8515 | ``	/* Optional nullable `?` shorthand. */`` |
|   2351069 |  8516 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|        39 |  8517 | `		p++;` |
|        39 |  8518 | `		if( p >= pEnd ) return 0;` |
|        18 |  8519 | `	}` |
|         - |  8520 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|         - |  8521 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|         - |  8522 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|         - |  8523 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   1175532 |  8524 | `	for(;;){` |
|   2351089 |  8525 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|         - |  8526 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|         3 |  8527 | `			p++;` |
|         9 |  8528 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|         3 |  8529 | `			if( p >= pEnd ) return 0;` |
|         3 |  8530 | `			p++; /* skip ')' */` |
|         2 |  8531 | `		}else{` |
|         - |  8532 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|         - |  8533 | ``			 * then any `&`-joined intersection members. */`` |
|   2351087 |  8534 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|   2351087 |  8535 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  8536 | `				return 0;` |
|         - |  8537 | `			}` |
|         - |  8538 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|         - |  8539 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|         - |  8540 | `			 * may still appear at the initial dispatch site). */` |
|   2351087 |  8541 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|   2351039 |  8542 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|   2351034 |  8543 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    103596 |  8544 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|   2350745 |  8545 | `					return 0;` |
|         - |  8546 | `				}` |
|       147 |  8547 | `			}` |
|       347 |  8548 | `			p++;` |
|       349 |  8549 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  8550 | `				p += 2;` |
|         1 |  8551 | `			}` |
|       516 |  8552 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|       350 |  8553 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  8554 | `				p++; /* skip '&' */` |
|         3 |  8555 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|         3 |  8556 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|         3 |  8557 | `				p++;` |
|         3 |  8558 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       ! 0 |  8559 | `					p += 2;` |
|       ! 0 |  8560 | `				}` |
|         1 |  8561 | `			}` |
|         - |  8562 | `		}` |
|       349 |  8563 | `		bFirst = 0;` |
|       344 |  8564 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        25 |  8565 | `			&& p->sData.zString[0] == '\|' ){` |
|        25 |  8566 | ``			p++; /* next `\|`-separated part */`` |
|        25 |  8567 | `			continue;` |
|         - |  8568 | `		}` |
|       329 |  8569 | `		break;` |
|       ! 0 |  8570 | `	}` |
|       329 |  8571 | `	if( p >= pEnd ) return 0;` |
|       329 |  8572 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   1175537 |  8573 | `}` |
|         - |  8574 |  |
|         - |  8575 | `/*` |
|         - |  8576 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|         - |  8577 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|         - |  8578 | ` * if not). Recognized forms:` |
|         - |  8579 | ` *   ?Type, array, bool, int, float, string, object,` |
|         - |  8580 | ` *   self, parent, \Ns\ClassName, ClassName` |
|         - |  8581 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|         - |  8582 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|         - |  8583 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|         - |  8584 | ` * on unrecoverable error.` |
|         - |  8585 | ` *` |
|         - |  8586 | ` * When a type is parsed:` |
|         - |  8587 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|         - |  8588 | ` *   *pClass is set to the class name (for class types)` |
|         - |  8589 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|         - |  8590 | ` *   *pTypeText is set to the original text span of the type` |
|         - |  8591 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|         - |  8592 | ` */` |
|       334 |  8593 | `static sxi32 GenStateParsePropertyType(` |
|         - |  8594 | `	ph7_gen_state *pGen,` |
|         - |  8595 | `	sxu32 *pnType,` |
|         - |  8596 | `	SyString *pClass,` |
|         - |  8597 | `	sxi32 *piTypeFlags,` |
|         - |  8598 | `	SyString *pTypeText,` |
|         - |  8599 | `	SySet *pAlts` |
|         5 |  8600 | `){` |
|       339 |  8601 | `	sxi32 iFlags = 0;` |
|         - |  8602 | `	sxi32 rc;` |
|       339 |  8603 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  8604 | `		return SXRET_OK;` |
|         - |  8605 | `	}` |
|         - |  8606 | `	/* If the first token is '$', there's no type */` |
|       339 |  8607 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       ! 0 |  8608 | `		return SXRET_OK;` |
|         - |  8609 | `	}` |
|       339 |  8610 | `	rc = GenStateParseUnionTypeDecl(` |
|       167 |  8611 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|         - |  8612 | `		PH7_CLASS_ATTR_NULLABLE,` |
|         - |  8613 | `		PH7_CLASS_ATTR_UNION,` |
|         - |  8614 | `		/* bAllowVoid */ 0,` |
|       334 |  8615 | `		pGen->pIn->nLine);` |
|       339 |  8616 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8617 | `		return rc;` |
|         - |  8618 | `	}` |
|         - |  8619 | `	/* Verify next token is '$' (start of property name) */` |
|       339 |  8620 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  8621 | `		return SXERR_SYNTAX;` |
|         - |  8622 | `	}` |
|       339 |  8623 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|       339 |  8624 | `	return SXRET_OK;` |
|       172 |  8625 | `}` |
|         - |  8626 |  |
|         - |  8627 | `/*` |
|         - |  8628 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|         - |  8629 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|         - |  8630 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|         - |  8631 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|         - |  8632 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|         - |  8633 | ` * by the type parser itself before reaching here.` |
|         - |  8634 | ` *` |
|         - |  8635 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|         - |  8636 | ` * use in the error message.` |
|         - |  8637 | ` */` |
|       510 |  8638 | `static int GenStateIsDisallowedPropertyAtom(` |
|         - |  8639 | `	sxu32 nType,` |
|         - |  8640 | `	const SyString *pClass,` |
|         - |  8641 | `	const char **pzName,` |
|         - |  8642 | `	sxu32 *pnName)` |
|         5 |  8643 | `{` |
|         - |  8644 | `	const char *z;` |
|         - |  8645 | `	sxu32 n;` |
|       515 |  8646 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|       459 |  8647 | `		return 0;` |
|         - |  8648 | `	}` |
|        60 |  8649 | `	z = pClass->zString;` |
|        60 |  8650 | `	n = pClass->nByte;` |
|        60 |  8651 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|         8 |  8652 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|         - |  8653 | `	}` |
|         - |  8654 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|         - |  8655 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|         - |  8656 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|        54 |  8657 | `	return 0;` |
|       260 |  8658 | `}` |
|         - |  8659 |  |
|         - |  8660 | `/*` |
|         - |  8661 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|         - |  8662 | ` * constant) — the main atom plus any union alternatives — against the` |
|         - |  8663 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|         - |  8664 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|         - |  8665 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|         - |  8666 | ` * type T" vs "Class constant C::X cannot have type T").` |
|         - |  8667 | ` *` |
|         - |  8668 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|         - |  8669 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|         - |  8670 | ` */` |
|       448 |  8671 | `static sxi32 GenStateValidateMemberType(` |
|         - |  8672 | `	ph7_gen_state *pGen,` |
|         - |  8673 | `	ph7_class *pClass,` |
|         - |  8674 | `	const SyString *pMemberName,` |
|         - |  8675 | `	sxu32 nType,` |
|         - |  8676 | `	const SyString *pTypeClass,` |
|         - |  8677 | `	const SyString *pTypeText,` |
|         - |  8678 | `	SySet *pUnionAlts,` |
|         - |  8679 | `	const char *zErrFmt,` |
|         - |  8680 | `	sxu32 nLine)` |
|         5 |  8681 | `{` |
|       453 |  8682 | `	const char *zBad = 0;` |
|       453 |  8683 | `	sxu32 nBad = 0;` |
|         - |  8684 | `	SyString sFallback;` |
|         - |  8685 | `	const SyString *pBad;` |
|         - |  8686 | `	sxi32 rc;` |
|       453 |  8687 | `	int bDisallowed = 0;` |
|       453 |  8688 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|         5 |  8689 | `		bDisallowed = 1;` |
|       451 |  8690 | `	}else if( pUnionAlts ){` |
|         - |  8691 | `		sxu32 i;` |
|        95 |  8692 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|        67 |  8693 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|        67 |  8694 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|         3 |  8695 | `				bDisallowed = 1;` |
|         3 |  8696 | `				break;` |
|         - |  8697 | `			}` |
|        35 |  8698 | `		}` |
|        15 |  8699 | `	}` |
|       453 |  8700 | `	if( !bDisallowed ){` |
|       447 |  8701 | `		return SXRET_OK;` |
|         - |  8702 | `	}` |
|         - |  8703 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|         - |  8704 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|         - |  8705 | `	 * canonical spelling if the type text is unavailable. */` |
|         8 |  8706 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|         8 |  8707 | `		pBad = pTypeText;` |
|         5 |  8708 | `	}else{` |
|       ! 0 |  8709 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|       ! 0 |  8710 | `		pBad = &sFallback;` |
|         - |  8711 | `	}` |
|        11 |  8712 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         3 |  8713 | `		zErrFmt,` |
|         3 |  8714 | `		&pClass->sName,pMemberName,pBad);` |
|         8 |  8715 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  8716 | `		return SXERR_ABORT;` |
|         - |  8717 | `	}` |
|         8 |  8718 | `	return SXERR_SYNTAX;` |
|       229 |  8719 | `}` |
|         - |  8720 | `/*` |
|         - |  8721 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|         - |  8722 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|         - |  8723 | ` * matched as a plain identifier in the class-member modifier position rather` |
|         - |  8724 | ` * than promoted to a lexer keyword.` |
|         - |  8725 | ` */` |
|  20363142 |  8726 | `static int GenStateIsReadonly(SyToken *pTok)` |
|         5 |  8727 | `{` |
|  20581653 |  8728 | `	return (pTok->nType & PH7_TK_ID)` |
|  10400077 |  8729 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
|  20581648 |  8730 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|         5 |  8731 | `}` |
|         - |  8732 | `/*` |
|         - |  8733 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|         - |  8734 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|         - |  8735 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|         - |  8736 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|         - |  8737 | ` */` |
|   7284628 |  8738 | `static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|         5 |  8739 | `{` |
|   7284633 |  8740 | `	*pnTok = 0;` |
|   7284628 |  8741 | `	if( &pTok[3] < pEnd` |
|   6829159 |  8742 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|   5620278 |  8743 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|   2433441 |  8744 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        16 |  8745 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|        16 |  8746 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|        21 |  8747 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|        17 |  8748 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|        17 |  8749 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|        17 |  8750 | `			*pnTok = 4;` |
|        17 |  8751 | `			return nKw;` |
|         - |  8752 | `		}` |
|       ! 0 |  8753 | `	}` |
|   7284617 |  8754 | `	return 0;` |
|   3642319 |  8755 | `}` |
|         - |  8756 | `/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */` |
|        16 |  8757 | `static sxi32 GenStateSetVisFlag(sxi32 nKw)` |
|         1 |  8758 | `{` |
|        17 |  8759 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|        13 |  8760 | `		return PH7_CLASS_ATTR_PRIVATE_SET;` |
|         - |  8761 | `	}` |
|         5 |  8762 | `	if( nKw == PH7_TKWRD_PROTECTED ){` |
|         3 |  8763 | `		return PH7_CLASS_ATTR_PROTECTED_SET;` |
|         - |  8764 | `	}` |
|         3 |  8765 | `	return PH7_CLASS_ATTR_PUBLIC_SET;` |
|         9 |  8766 | `}` |
|    459406 |  8767 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  8768 | `{` |
|    459411 |  8769 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  8770 | `	ph7_class_attr *pAttr;` |
|         - |  8771 | `	SyString *pName;` |
|         - |  8772 | `	sxi32 rc;` |
|    459411 |  8773 | `	sxu32 nType = 0;` |
|         - |  8774 | `	SyString sTypeClass;` |
|         - |  8775 | `	SyString sTypeText;` |
|         - |  8776 | `	SySet aUnionAlts;` |
|    459411 |  8777 | `	sxi32 iTypeFlags = 0;` |
|    459411 |  8778 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    459411 |  8779 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    459411 |  8780 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  8781 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|         - |  8782 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|         - |  8783 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|    459411 |  8784 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|        21 |  8785 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|         9 |  8786 | `	}` |
|         - |  8787 | `	/* Extract visibility level */` |
|    459411 |  8788 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  8789 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|    459578 |  8790 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       339 |  8791 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|       339 |  8792 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  8793 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  8794 | `			goto Synchronize;` |
|       339 |  8795 | `		}else if( rc == SXERR_SYNTAX ){` |
|       ! 0 |  8796 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8797 | `				"Invalid property type or declaration near '%z'",` |
|       ! 0 |  8798 | `				&pGen->pIn->sData);` |
|       ! 0 |  8799 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8800 | `				return SXERR_ABORT;` |
|         - |  8801 | `			}` |
|       ! 0 |  8802 | `			goto Synchronize;` |
|       339 |  8803 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  8804 | `			return SXERR_ABORT;` |
|         - |  8805 | `		}` |
|       167 |  8806 | `	}` |
|       ! 0 |  8807 | `loop:` |
|    459415 |  8808 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  8809 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|       ! 0 |  8810 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8811 | `			return SXERR_ABORT;` |
|         - |  8812 | `		}` |
|       ! 0 |  8813 | `		goto Synchronize;` |
|         - |  8814 | `	}` |
|    459415 |  8815 | `	pGen->pIn++; /* Jump the dollar sign */` |
|    459415 |  8816 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|         - |  8817 | `		/* Invalid attribute name */` |
|       ! 0 |  8818 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|       ! 0 |  8819 | `		if( rc == SXERR_ABORT ){` |
|         - |  8820 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8821 | `			return SXERR_ABORT;` |
|         - |  8822 | `		}` |
|       ! 0 |  8823 | `		goto Synchronize;` |
|         - |  8824 | `	}` |
|         - |  8825 | `	/* Peek attribute name */` |
|    459415 |  8826 | `	pName = &pGen->pIn->sData;` |
|         - |  8827 | `	/* Advance the stream cursor */` |
|    459415 |  8828 | `	pGen->pIn++;` |
|    459415 |  8829 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/\|PH7_TK_OCB/*'{' hooks*/)) == 0 ){` |
|         - |  8830 | `		/* Invalid declaration */` |
|         3 |  8831 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|         3 |  8832 | `		if( rc == SXERR_ABORT ){` |
|         - |  8833 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8834 | `			return SXERR_ABORT;` |
|         - |  8835 | `		}` |
|         3 |  8836 | `		goto Synchronize;` |
|         - |  8837 | `	}` |
|         - |  8838 | `	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and` |
|         - |  8839 | `	 * the read visibility must not be narrower than the set visibility. */` |
|    459413 |  8840 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
|        13 |  8841 | `		const char *zAvErr = 0;` |
|        19 |  8842 | `		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE` |
|        10 |  8843 | `			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED` |
|         2 |  8844 | `			: PH7_CLASS_PROT_PUBLIC;` |
|        13 |  8845 | `		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 |  8846 | `			zAvErr = "Property with asymmetric visibility %z::$%z must have type";` |
|        13 |  8847 | `		}else if( iProtection > iSetLevel ){` |
|       ! 0 |  8848 | `			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";` |
|       ! 0 |  8849 | `		}` |
|        13 |  8850 | `		if( zAvErr ){` |
|       ! 0 |  8851 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sName,pName);` |
|       ! 0 |  8852 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8853 | `				return SXERR_ABORT;` |
|         - |  8854 | `			}` |
|       ! 0 |  8855 | `			goto Synchronize;` |
|         - |  8856 | `		}` |
|         6 |  8857 | `	}` |
|         - |  8858 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|         - |  8859 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|    459413 |  8860 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|        43 |  8861 | `		const char *zRoErr = 0;` |
|        43 |  8862 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|         3 |  8863 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|        42 |  8864 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         6 |  8865 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|        39 |  8866 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|         6 |  8867 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|         2 |  8868 | `		}` |
|        43 |  8869 | `		if( zRoErr ){` |
|        13 |  8870 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|        13 |  8871 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8872 | `				return SXERR_ABORT;` |
|         - |  8873 | `			}` |
|        13 |  8874 | `			goto Synchronize;` |
|         - |  8875 | `		}` |
|        14 |  8876 | `	}` |
|         - |  8877 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|         - |  8878 | `	 * type atom or any union alternative. void/never are already rejected` |
|         - |  8879 | `	 * by the type parser. */` |
|    459403 |  8880 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       503 |  8881 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|         - |  8882 | `			&sTypeText,` |
|       332 |  8883 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       166 |  8884 | `			"Property %z::$%z cannot have type %z",nLine);` |
|       337 |  8885 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8886 | `			return SXERR_ABORT;` |
|       337 |  8887 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  8888 | `			goto Synchronize;` |
|         - |  8889 | `		}` |
|       166 |  8890 | `	}` |
|         - |  8891 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|    459403 |  8892 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|         4 |  8893 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  8894 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|         3 |  8895 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8896 | `			return SXERR_ABORT;` |
|         - |  8897 | `		}` |
|         3 |  8898 | `		goto Synchronize;` |
|         - |  8899 | `	}` |
|         - |  8900 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|         - |  8901 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|         - |  8902 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|         - |  8903 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|         - |  8904 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|         - |  8905 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|    459401 |  8906 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|         6 |  8907 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8908 | `			"New expressions are not supported in this context");` |
|         6 |  8909 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8910 | `			return SXERR_ABORT;` |
|         - |  8911 | `		}` |
|         6 |  8912 | `		goto Synchronize;` |
|         - |  8913 | `	}` |
|         - |  8914 | `	/* Allocate a new class attribute */` |
|    459397 |  8915 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    459397 |  8916 | `	if( pAttr ){` |
|    459397 |  8917 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|    459397 |  8918 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8919 | `			return SXERR_ABORT;` |
|         - |  8920 | `		}` |
|    229696 |  8921 | `	}` |
|    459397 |  8922 | `	if( pAttr == 0 ){` |
|       ! 0 |  8923 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  8924 | `		return SXERR_ABORT;` |
|         - |  8925 | `	}` |
|    459397 |  8926 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       335 |  8927 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       165 |  8928 | `	}` |
|    459397 |  8929 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|         - |  8930 | `		SySet *pInstrContainer;` |
|    336873 |  8931 | `		SyToken *pSavedDefEnd = pGen->pEnd;` |
|    336873 |  8932 | `		pGen->pIn++; /*Jump the equal sign */` |
|         - |  8933 | `		{` |
|         - |  8934 | `			/* Delimit the default expression: it ends at the declaration's` |
|         - |  8935 | `			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list` |
|         - |  8936 | ``			 * (`public string $w = "init" { get => …; }`) — the expression`` |
|         - |  8937 | `			 * compiler would otherwise run into the hook tokens. */` |
|    336873 |  8938 | `			SyToken *pScan = pGen->pIn;` |
|    336873 |  8939 | `			sxi32 iNest = 0;` |
|    735567 |  8940 | `			while( pScan < pGen->pEnd ){` |
|    735567 |  8941 | `				if( pScan->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     42111 |  8942 | `					iNest++;` |
|    714514 |  8943 | `				}else if( pScan->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|     42111 |  8944 | `					iNest--;` |
|    672408 |  8945 | `				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI\|PH7_TK_COMMA\|PH7_TK_OCB)) ){` |
|    336873 |  8946 | `					break;` |
|         - |  8947 | `				}` |
|    398699 |  8948 | `				pScan++;` |
|         5 |  8949 | `			}` |
|    336873 |  8950 | `			pGen->pEnd = pScan;` |
|         - |  8951 | `		}` |
|         - |  8952 | `		/* Swap bytecode container */` |
|    336873 |  8953 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    336873 |  8954 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|         - |  8955 | `		/* Compile attribute value.` |
|         - |  8956 | `		 */` |
|    336873 |  8957 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    336873 |  8958 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 |  8959 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|       ! 0 |  8960 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8961 | `				return SXERR_ABORT;` |
|         - |  8962 | `			}` |
|       ! 0 |  8963 | `		}` |
|         - |  8964 | `		/* Emit the done instruction */` |
|    336873 |  8965 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    336873 |  8966 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    336873 |  8967 | `		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */` |
|    336873 |  8968 | `		pGen->pEnd = pSavedDefEnd;` |
|    168434 |  8969 | `	}` |
|         - |  8970 | `	/* All done,install the attribute */` |
|    459397 |  8971 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|    459397 |  8972 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8973 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8974 | `		return SXERR_ABORT;` |
|         - |  8975 | `	}` |
|    459397 |  8976 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){` |
|         - |  8977 | ``		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.`` |
|         - |  8978 | `		 * The list ends the declaration at '}' — no trailing ';', no comma list. */` |
|        95 |  8979 | `		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);` |
|        95 |  8980 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8981 | `			return SXERR_ABORT;` |
|         - |  8982 | `		}` |
|        95 |  8983 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  8984 | `			goto Synchronize;` |
|         - |  8985 | `		}` |
|        95 |  8986 | `		SySetRelease(&aUnionAlts);` |
|        95 |  8987 | `		return SXRET_OK;` |
|         - |  8988 | `	}` |
|    459303 |  8989 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - |  8990 | ``		/* php 8.4: `abstract` on a property requires a hook list (php's exact`` |
|         - |  8991 | `		 * wording differs per declaration site) */` |
|       ! 0 |  8992 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  8993 | `			(pClass->iFlags & PH7_CLASS_INTERFACE)` |
|         - |  8994 | `				? "Interfaces may only include hooked properties"` |
|         - |  8995 | `				: "Only hooked properties may be declared abstract");` |
|       ! 0 |  8996 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8997 | `			return SXERR_ABORT;` |
|         - |  8998 | `		}` |
|       ! 0 |  8999 | `		goto Synchronize;` |
|         - |  9000 | `	}` |
|    459303 |  9001 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  9002 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|         5 |  9003 | `		pGen->pIn++; /* Jump the comma */` |
|         5 |  9004 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|       ! 0 |  9005 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 |  9006 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 |  9007 | `				pTok--;` |
|       ! 0 |  9008 | `			}` |
|       ! 0 |  9009 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  9010 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|       ! 0 |  9011 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 |  9012 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9013 | `				return SXERR_ABORT;` |
|         - |  9014 | `			}` |
|       ! 0 |  9015 | `		}else{` |
|         5 |  9016 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         5 |  9017 | `				goto loop;` |
|         - |  9018 | `			}` |
|         - |  9019 | `		}` |
|       ! 0 |  9020 | `	}` |
|    459299 |  9021 | `	SySetRelease(&aUnionAlts);` |
|    459299 |  9022 | `	return SXRET_OK;` |
|         9 |  9023 | `Synchronize:` |
|         - |  9024 | `	/* Synchronize with the first semi-colon */` |
|        56 |  9025 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        37 |  9026 | `		pGen->pIn++;` |
|         3 |  9027 | `	}` |
|        22 |  9028 | `	SySetRelease(&aUnionAlts);` |
|        22 |  9029 | `	return SXERR_CORRUPT;` |
|    229708 |  9030 | `}` |
|         - |  9031 | `/*` |
|         - |  9032 | ` * Compile a class method.` |
|         - |  9033 | ` *` |
|         - |  9034 | ` * Refer to the official documentation for more information` |
|         - |  9035 | ` * on the powerful extension introduced by the PH7 engine` |
|         - |  9036 | ` * to the OO subsystem such as full type hinting,method` |
|         - |  9037 | ` * overloading and many more.` |
|         - |  9038 | ` */` |
|   2396828 |  9039 | `static sxi32 GenStateCompileClassMethod(` |
|         - |  9040 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  9041 | `	sxi32 iProtection,   /* Visibility level */` |
|         - |  9042 | `	sxi32 iFlags,        /* Configuration flags */` |
|         - |  9043 | `	int doBody,          /* TRUE to process method body */` |
|         - |  9044 | `	ph7_class *pClass    /* Class this method belongs */` |
|         - |  9045 | `	)` |
|         5 |  9046 | `{` |
|   2396833 |  9047 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   2396833 |  9048 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|         - |  9049 | `	ph7_class_method *pMeth;` |
|         - |  9050 | `	sxi32 iFuncFlags;` |
|         - |  9051 | `	SyString *pName;` |
|         - |  9052 | `	SyToken *pEnd;` |
|         - |  9053 | `	sxi32 rc;` |
|         - |  9054 | `	/* Extract visibility level */` |
|   2396833 |  9055 | `	iProtection = GetProtectionLevel(iProtection);` |
|   2396833 |  9056 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   2396833 |  9057 | `	iFuncFlags = 0;` |
|   2396833 |  9058 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  9059 | `		/* Invalid method name */` |
|       ! 0 |  9060 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|       ! 0 |  9061 | `		if( rc == SXERR_ABORT ){` |
|         - |  9062 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9063 | `			return SXERR_ABORT;` |
|         - |  9064 | `		}` |
|       ! 0 |  9065 | `		goto Synchronize;` |
|         - |  9066 | `	}` |
|   2396833 |  9067 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - |  9068 | `		/* Return by reference,remember that */` |
|       ! 0 |  9069 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|         - |  9070 | `		/* Jump the '&' token */` |
|       ! 0 |  9071 | `		pGen->pIn++;` |
|       ! 0 |  9072 | `	}` |
|   2396833 |  9073 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  9074 | `		/* Invalid method name */` |
|       ! 0 |  9075 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|       ! 0 |  9076 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  9077 | `			return SXERR_ABORT;` |
|         - |  9078 | `		}` |
|       ! 0 |  9079 | `		goto Synchronize;` |
|         - |  9080 | `	}` |
|         - |  9081 | `	/* Peek method name */` |
|   2396833 |  9082 | `	pName = &pGen->pIn->sData;` |
|   2396833 |  9083 | `	nLine = pGen->pIn->nLine;` |
|         - |  9084 | `	/* Jump the method name */` |
|   2396833 |  9085 | `	pGen->pIn++;` |
|   2396833 |  9086 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - |  9087 | `		/* Abstract method */` |
|    137593 |  9088 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       ! 0 |  9089 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9090 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|       ! 0 |  9091 | `				&pClass->sName,pName);` |
|       ! 0 |  9092 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9093 | `				return SXERR_ABORT;` |
|         - |  9094 | `			}` |
|       ! 0 |  9095 | `		}` |
|         - |  9096 | `		/* Assemble method signature only */` |
|    137593 |  9097 | `		doBody = FALSE;` |
|     68794 |  9098 | `	}` |
|   2396833 |  9099 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  9100 | `		/* Syntax error */` |
|       ! 0 |  9101 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|       ! 0 |  9102 | `		if( rc == SXERR_ABORT ){` |
|         - |  9103 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9104 | `			return SXERR_ABORT;` |
|         - |  9105 | `		}` |
|       ! 0 |  9106 | `		goto Synchronize;` |
|         - |  9107 | `	}` |
|         - |  9108 | `	/* Allocate a new class_method instance */` |
|   2396833 |  9109 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|   2396833 |  9110 | `	if( pMeth == 0 ){` |
|       ! 0 |  9111 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9112 | `		return SXERR_ABORT;` |
|         - |  9113 | `	}` |
|   2396833 |  9114 | `	pMeth->sFunc.nLine = nKwLine;` |
|   2396833 |  9115 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|   2396833 |  9116 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  9117 | `		return SXERR_ABORT;` |
|         - |  9118 | `	}` |
|         - |  9119 | `	/* Jump the left parenthesis '(' */` |
|   2396833 |  9120 | `	pGen->pIn++;` |
|   2396833 |  9121 | `	pEnd = 0; /* cc warning */` |
|         - |  9122 | `	/* Delimit the method signature */` |
|   2396833 |  9123 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   2396833 |  9124 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  9125 | `		/* Syntax error */` |
|         3 |  9126 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|         3 |  9127 | `		if( rc == SXERR_ABORT ){` |
|         - |  9128 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9129 | `			return SXERR_ABORT;` |
|         - |  9130 | `		}` |
|         3 |  9131 | `		goto Synchronize;` |
|         - |  9132 | `	}` |
|         - |  9133 | `	{` |
|   2396831 |  9134 | `		int bIsCtor = 0;` |
|   2396831 |  9135 | `		int bAbstractCtor = 0;` |
|   2396826 |  9136 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|   1399124 |  9137 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|   2314595 |  9138 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|    164477 |  9139 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         3 |  9140 | `				bAbstractCtor = 1;` |
|         2 |  9141 | `			}else{` |
|    164475 |  9142 | `				bIsCtor = 1;` |
|         - |  9143 | `			}` |
|     82236 |  9144 | `		}` |
|   2396831 |  9145 | `		if( pGen->pIn < pEnd ){` |
|         - |  9146 | `			/* Collect method arguments */` |
|    864015 |  9147 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|    864015 |  9148 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9149 | `				return SXERR_ABORT;` |
|         - |  9150 | `			}` |
|    432005 |  9151 | `		}` |
|         - |  9152 | `	}` |
|         - |  9153 | `	/* Point past ')' and parse optional return type ': type' */` |
|   2396831 |  9154 | `	pGen->pIn = &pEnd[1];` |
|         - |  9155 | `	{` |
|   2396831 |  9156 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|   2396831 |  9157 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 |  9158 | `			return SXERR_ABORT;` |
|   2396831 |  9159 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       ! 0 |  9160 | `			goto Synchronize;` |
|         - |  9161 | `		}` |
|         - |  9162 | `	}` |
|         - |  9163 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|         - |  9164 | `	 * property init/typecheck is handled by the generic typed-property path` |
|         - |  9165 | `	 * since we mint real ph7_class_attr entries. */` |
|         - |  9166 | `	{` |
|   2396831 |  9167 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|         - |  9168 | `		sxu32 i;` |
|   3688863 |  9169 | `		for( i = 0; i < nArg; i++ ){` |
|   1292047 |  9170 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|         - |  9171 | `			ph7_class_attr *pAttr;` |
|   1292047 |  9172 | `			sxi32 iAttrFlags = 0;` |
|         - |  9173 | `			int bArgTyped;` |
|   1292047 |  9174 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   1291963 |  9175 | `				continue;` |
|         - |  9176 | `			}` |
|         - |  9177 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|         - |  9178 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|         - |  9179 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|        59 |  9180 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|        90 |  9181 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|        89 |  9182 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|         3 |  9183 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9184 | `					"Cannot declare variadic promoted property");` |
|         3 |  9185 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9186 | `					return SXERR_ABORT;` |
|         - |  9187 | `				}` |
|         3 |  9188 | `				goto Synchronize;` |
|         - |  9189 | `			}` |
|         - |  9190 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|         - |  9191 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|         - |  9192 | `			 * appear as an alternative of a union type. */` |
|        87 |  9193 | `			if( bArgTyped ){` |
|       122 |  9194 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|        78 |  9195 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|        78 |  9196 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|        39 |  9197 | `					"Property %z::$%z cannot have type %z",nLine);` |
|        83 |  9198 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9199 | `					return SXERR_ABORT;` |
|        83 |  9200 | `				}else if( rc != SXRET_OK ){` |
|         6 |  9201 | `					goto Synchronize;` |
|         - |  9202 | `				}` |
|        37 |  9203 | `			}` |
|         - |  9204 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|        83 |  9205 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|         4 |  9206 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  9207 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|         3 |  9208 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9209 | `					return SXERR_ABORT;` |
|         - |  9210 | `				}` |
|         3 |  9211 | `				goto Synchronize;` |
|         - |  9212 | `			}` |
|        81 |  9213 | `			if( bArgTyped ){` |
|        77 |  9214 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        36 |  9215 | `			}` |
|        81 |  9216 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|         3 |  9217 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|         1 |  9218 | `			}` |
|        81 |  9219 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|         8 |  9220 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|         3 |  9221 | `			}` |
|        81 |  9222 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|         - |  9223 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|         - |  9224 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|        26 |  9225 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         4 |  9226 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  9227 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|         3 |  9228 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9229 | `						return SXERR_ABORT;` |
|         - |  9230 | `					}` |
|         3 |  9231 | `					goto Synchronize;` |
|         - |  9232 | `				}` |
|        24 |  9233 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|        10 |  9234 | `			}` |
|        79 |  9235 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|         - |  9236 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|         5 |  9237 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 |  9238 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9239 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|       ! 0 |  9240 | `						&pClass->sName,&pArg->sName);` |
|       ! 0 |  9241 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9242 | `						return SXERR_ABORT;` |
|         - |  9243 | `					}` |
|       ! 0 |  9244 | `					goto Synchronize;` |
|         - |  9245 | `				}` |
|         5 |  9246 | `				iAttrFlags \|= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)` |
|         2 |  9247 | `					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;` |
|         2 |  9248 | `			}` |
|        79 |  9249 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|        79 |  9250 | `			if( pAttr == 0 ){` |
|       ! 0 |  9251 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9252 | `				return SXERR_ABORT;` |
|         - |  9253 | `			}` |
|        79 |  9254 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|        77 |  9255 | `				pAttr->nType = pArg->nType;` |
|        77 |  9256 | `				pAttr->sClass = pArg->sClass;` |
|        77 |  9257 | `				pAttr->sTypeName = pArg->sTypeName;` |
|        77 |  9258 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  9259 | `					sxu32 k;` |
|        20 |  9260 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|        14 |  9261 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|        14 |  9262 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|         8 |  9263 | `					}` |
|         3 |  9264 | `				}` |
|        36 |  9265 | `			}` |
|        79 |  9266 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|        79 |  9267 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9268 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9269 | `				return SXERR_ABORT;` |
|         - |  9270 | `			}` |
|        42 |  9271 | `		}` |
|         - |  9272 | `	}` |
|   2396821 |  9273 | `	if( doBody ){` |
|         - |  9274 | `		/* Compile method body */` |
|   2259233 |  9275 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|   2259233 |  9276 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  9277 | `			return SXERR_ABORT;` |
|         - |  9278 | `		}` |
|         - |  9279 | `		/* The cursor sits just past the body's closing brace */` |
|   2259233 |  9280 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|   1129619 |  9281 | `	}else{` |
|         - |  9282 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|    137593 |  9283 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|    137593 |  9284 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|     68794 |  9285 | `		}` |
|         - |  9286 | `		/* Only method signature is allowed */` |
|    137593 |  9287 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|       ! 0 |  9288 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9289 | `				"Expected ';' after method signature '%z'",pName);` |
|       ! 0 |  9290 | `				if( rc == SXERR_ABORT ){` |
|         - |  9291 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9292 | `					return SXERR_ABORT;` |
|         - |  9293 | `				}` |
|       ! 0 |  9294 | `				return SXERR_CORRUPT;` |
|         - |  9295 | `			}` |
|         - |  9296 | `	}` |
|         - |  9297 | `	/* All done,install the method */` |
|   2396821 |  9298 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|   2396821 |  9299 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  9300 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9301 | `		return SXERR_ABORT;` |
|         - |  9302 | `	}` |
|   2396821 |  9303 | `	return SXRET_OK;` |
|         6 |  9304 | `Synchronize:` |
|         - |  9305 | `	/* Synchronize with the first semi-colon */` |
|        40 |  9306 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        28 |  9307 | `		pGen->pIn++;` |
|         4 |  9308 | `	}` |
|        16 |  9309 | `	return SXERR_CORRUPT;` |
|   1198419 |  9310 | `}` |
|         - |  9311 | `/*` |
|         - |  9312 | `` * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a`` |
|         - |  9313 | ` * property declaration. Each hook body is synthesized into a hidden public` |
|         - |  9314 | ` * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,` |
|         - |  9315 | ` * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /` |
|         - |  9316 | ` * OP_STORE route reads and plain writes through them (a per-instance guard` |
|         - |  9317 | ` * makes $this->NAME inside a hook body address the raw backing slot — php's` |
|         - |  9318 | `` * rule that hooks see the backing store). `get => expr;` compiles as an`` |
|         - |  9319 | `` * implicit return (the arrow-fn pattern); `set => expr;` compiles the same`` |
|         - |  9320 | ` * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return` |
|         - |  9321 | `` * value to the backing slot. A `set` without a parameter list receives the`` |
|         - |  9322 | `` * implicit `$value` formal.`` |
|         - |  9323 | ` * On entry pGen->pIn sits on '{'; on success it sits just past '}'.` |
|         - |  9324 | ` */` |
|         - |  9325 | `/*` |
|         - |  9326 | `` * Whether any token in [pStart, pEnd) spells `$this->NAME` (this property's own`` |
|         - |  9327 | `` * name; `?->` and `::` member ops count too). php 8.4's virtual-vs-backed rule:`` |
|         - |  9328 | ` * a hooked property is BACKED iff any of its OWN hook bodies references it by` |
|         - |  9329 | ` * name through $this — otherwise it is VIRTUAL: no backing store, no default` |
|         - |  9330 | ` * allowed, excluded from the raw object surfaces.` |
|         - |  9331 | ` */` |
|        94 |  9332 | `static int GenStateHookBodyRefsProp(SyToken *pStart,SyToken *pEnd,const SyString *pName)` |
|         1 |  9333 | `{` |
|         - |  9334 | `	SyToken *p;` |
|       345 |  9335 | `	for( p = pStart ; p + 1 < pEnd ; p++ ){` |
|       303 |  9336 | `		if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|       223 |  9337 | `			continue;` |
|         - |  9338 | `		}` |
|         - |  9339 | ``		/* `$this->NAME` (also `?->`/`::`) */`` |
|        80 |  9340 | `		if( p + 3 < pEnd` |
|        80 |  9341 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        80 |  9342 | `		 && p[1].sData.nByte == sizeof("this")-1` |
|        73 |  9343 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0` |
|        66 |  9344 | `		 && GenStateTokenIsMemberOp(&p[2])` |
|        66 |  9345 | `		 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        66 |  9346 | `		 && p[3].sData.nByte == pName->nByte` |
|        60 |  9347 | `		 && SyMemcmp((const void *)p[3].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        51 |  9348 | `			return 1;` |
|         - |  9349 | `		}` |
|         - |  9350 | ``		/* `parent::$NAME` (the parent::$x::get() hook-call form): the parent`` |
|         - |  9351 | `		 * hook operates on the shared per-instance backing store, so the` |
|         - |  9352 | `		 * property is backed (php compiles a default alongside it). */` |
|        30 |  9353 | `		if( p > pStart` |
|        26 |  9354 | `		 && GenStateTokenIsMemberOp(&p[-1])` |
|        12 |  9355 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         2 |  9356 | `		 && p[1].sData.nByte == pName->nByte` |
|         3 |  9357 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|         3 |  9358 | `			return 1;` |
|         - |  9359 | `		}` |
|        15 |  9360 | `	}` |
|        43 |  9361 | `	return 0;` |
|        48 |  9362 | `}` |
|         - |  9363 | `/*` |
|         - |  9364 | ` * True when p opens php 8.4's parent-hook call form` |
|         - |  9365 | `` * `parent :: $ NAME :: get\|set (` (7 tokens through the '(').`` |
|         - |  9366 | ` */` |
|       990 |  9367 | `static int GenStateIsParentHookCallAt(SyToken *p,SyToken *pEnd)` |
|         1 |  9368 | `{` |
|      1167 |  9369 | `	return p + 6 < pEnd` |
|       671 |  9370 | `	 && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       250 |  9371 | `	 && p->sData.nByte == sizeof("parent")-1` |
|        81 |  9372 | `	 && SyMemcmp((const void *)p->sData.zString,(const void *)"parent",sizeof("parent")-1) == 0` |
|        11 |  9373 | `	 && GenStateTokenIsMemberOp(&p[1])` |
|         8 |  9374 | `	 && (p[2].nType & PH7_TK_DOLLAR) != 0` |
|         8 |  9375 | `	 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 |  9376 | `	 && GenStateTokenIsMemberOp(&p[4])` |
|         8 |  9377 | `	 && (p[5].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 |  9378 | `	 && p[5].sData.nByte == 3` |
|         8 |  9379 | `	 && (SyMemcmp((const void *)p[5].sData.zString,(const void *)"get",3) == 0` |
|         6 |  9380 | `	  \|\| SyMemcmp((const void *)p[5].sData.zString,(const void *)"set",3) == 0)` |
|      1166 |  9381 | `	 && (p[6].nType & PH7_TK_LPAREN) != 0;` |
|         1 |  9382 | `}` |
|         - |  9383 | `/*` |
|         - |  9384 | `` * Rewrite php 8.4 `parent::$x::get(...)` / `parent::$x::set(...)` calls in a`` |
|         - |  9385 | ` * hook body into calls of the parent class's synthesized hook method` |
|         - |  9386 | `` * (`parent::__phl_hook_get_x(...)`). Builds a token COPY into pCopy (only`` |
|         - |  9387 | ` * called when GenStateIsParentHookCallAt matched somewhere in the range);` |
|         - |  9388 | ` * copied tokens keep pointing at source-owned lexeme storage, and the` |
|         - |  9389 | ` * synthesized method-name lexemes are VM-allocator owned. Returns SXRET_OK` |
|         - |  9390 | ` * or SXERR_MEM.` |
|         - |  9391 | ` */` |
|         4 |  9392 | `static sxi32 GenStateRewriteParentHookCalls(ph7_gen_state *pGen,SySet *pCopy,` |
|         - |  9393 | `	SyToken *pStart,SyToken *pEnd)` |
|         1 |  9394 | `{` |
|         5 |  9395 | `	SyToken *p = pStart;` |
|        35 |  9396 | `	while( p < pEnd ){` |
|        31 |  9397 | `		if( GenStateIsParentHookCallAt(p,pEnd) ){` |
|         - |  9398 | `			SyToken sTok;` |
|         - |  9399 | `			char zName[384];` |
|         - |  9400 | `			sxu32 nName;` |
|         - |  9401 | `			char *zDup;` |
|         - |  9402 | ``			/* `parent` `::` */`` |
|         5 |  9403 | `			SySetPut(pCopy,(const void *)&p[0]);` |
|         5 |  9404 | `			SySetPut(pCopy,(const void *)&p[1]);` |
|         7 |  9405 | `			nName = SyBufferFormat(zName,sizeof(zName),"__phl_hook_%.3s_%.*s",` |
|         4 |  9406 | `				p[5].sData.zString,(int)p[3].sData.nByte,p[3].sData.zString);` |
|         5 |  9407 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|         5 |  9408 | `			if( zDup == 0 ){` |
|       ! 0 |  9409 | `				return SXERR_MEM;` |
|         - |  9410 | `			}` |
|         5 |  9411 | `			sTok = p[3]; /* keep the line info of the property name */` |
|         5 |  9412 | `			sTok.nType = PH7_TK_ID;` |
|         5 |  9413 | `			SyStringInitFromBuf(&sTok.sData,zDup,nName);` |
|         5 |  9414 | `			sTok.pUserData = 0;` |
|         5 |  9415 | `			SySetPut(pCopy,(const void *)&sTok);` |
|         5 |  9416 | `			p += 6; /* continue at the '(' — arguments copy through unchanged */` |
|         5 |  9417 | `			continue;` |
|         - |  9418 | `		}` |
|        27 |  9419 | `		SySetPut(pCopy,(const void *)p);` |
|        27 |  9420 | `		p++;` |
|         1 |  9421 | `	}` |
|         5 |  9422 | `	return SXRET_OK;` |
|         3 |  9423 | `}` |
|        94 |  9424 | `static sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)` |
|         1 |  9425 | `{` |
|        95 |  9426 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  9427 | `	sxi32 rc;` |
|        95 |  9428 | `	int bRefsSelf = 0;` |
|        95 |  9429 | `	pGen->pIn++; /* Jump '{' */` |
|       253 |  9430 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|         - |  9431 | `		char zHook[384];` |
|         - |  9432 | `		SyString sHookName;` |
|         - |  9433 | `		ph7_class_method *pMeth;` |
|         - |  9434 | `		int bGet;` |
|       159 |  9435 | `		sxu32 nHLine = pGen->pIn->nLine;` |
|       159 |  9436 | `		if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        15 |  9437 | `			pGen->pIn++; /* stray ';' between hooks */` |
|        22 |  9438 | `			continue;` |
|         - |  9439 | `		}` |
|       145 |  9440 | `		if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - |  9441 | `			/* by-reference get hook: not modeled (loud, recorded) */` |
|       ! 0 |  9442 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9443 | `				"By-reference property hooks are not supported for %z::$%z",` |
|       ! 0 |  9444 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9445 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9446 | `				return SXERR_ABORT;` |
|         - |  9447 | `			}` |
|       ! 0 |  9448 | `			return SXERR_CORRUPT;` |
|         - |  9449 | `		}` |
|       145 |  9450 | `		if( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  9451 | `			goto HookSyntax;` |
|         - |  9452 | `		}` |
|       144 |  9453 | `		if( pGen->pIn->sData.nByte == 3` |
|       145 |  9454 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"get",3) == 0 ){` |
|        79 |  9455 | `			bGet = 1;` |
|       106 |  9456 | `		}else if( pGen->pIn->sData.nByte == 3` |
|        67 |  9457 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"set",3) == 0 ){` |
|        67 |  9458 | `			bGet = 0;` |
|        34 |  9459 | `		}else{` |
|       ! 0 |  9460 | `			goto HookSyntax;` |
|         - |  9461 | `		}` |
|       145 |  9462 | `		pGen->pIn++; /* Jump 'get'/'set' */` |
|       145 |  9463 | `		sHookName.zString = zHook;` |
|       217 |  9464 | `		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",` |
|        72 |  9465 | `			bGet ? "get" : "set",&pAttr->sName);` |
|       145 |  9466 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB)) ){` |
|         - |  9467 | ``			/* Bare `get;` / `set;` — an ABSTRACT hook declaration (php 8.4):`` |
|         - |  9468 | ``			 * legal only on an `abstract` property or inside an interface. The`` |
|         - |  9469 | `			 * synthesized method carries PH7_CLASS_ATTR_ABSTRACT and rides the` |
|         - |  9470 | `			 * existing must-implement machinery; a concrete hook override (or a` |
|         - |  9471 | `			 * plain property, see GenStateCheckAbstractMethods) satisfies it. */` |
|        14 |  9472 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|         8 |  9473 | `			 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 |  9474 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9475 | `					"Non-abstract property hook must have a body");` |
|       ! 0 |  9476 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9477 | `					return SXERR_ABORT;` |
|         - |  9478 | `				}` |
|       ! 0 |  9479 | `				return SXERR_CORRUPT;` |
|         - |  9480 | `			}` |
|        15 |  9481 | `			pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - |  9482 | `				PH7_CLASS_PROT_PUBLIC,PH7_CLASS_ATTR_ABSTRACT,0);` |
|        15 |  9483 | `			if( pMeth == 0 ){` |
|       ! 0 |  9484 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9485 | `				return SXERR_ABORT;` |
|         - |  9486 | `			}` |
|        15 |  9487 | `			pMeth->sFunc.nLine = nHLine;` |
|        15 |  9488 | `			if( !bGet ){` |
|         - |  9489 | ``				/* The implicit `$value` formal keeps the stub's signature`` |
|         - |  9490 | `				 * compatible with concrete set-hook implementations (which` |
|         - |  9491 | `				 * always carry one parameter). It takes the PROPERTY's declared` |
|         - |  9492 | `				 * type (php: the abstract set's parameter type IS the property` |
|         - |  9493 | `				 * type), so the override contravariance check accepts a typed` |
|         - |  9494 | ``				 * `set(int $v)` implementation on an `int $x` requirement. */`` |
|         - |  9495 | `				ph7_vm_func_arg sVArg;` |
|         7 |  9496 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|         7 |  9497 | `				if( zVName == 0 ){` |
|       ! 0 |  9498 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9499 | `					return SXERR_ABORT;` |
|         - |  9500 | `				}` |
|         7 |  9501 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|         7 |  9502 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|         7 |  9503 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         7 |  9504 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         7 |  9505 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|         7 |  9506 | `				sVArg.nType = pAttr->nType;` |
|         7 |  9507 | `				sVArg.sClass = pAttr->sClass;` |
|         7 |  9508 | `				sVArg.sTypeName = pAttr->sTypeName;` |
|         7 |  9509 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       ! 0 |  9510 | `					sVArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|       ! 0 |  9511 | `				}` |
|         7 |  9512 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|         3 |  9513 | `			}` |
|        15 |  9514 | `			rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|        15 |  9515 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9516 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9517 | `				return SXERR_ABORT;` |
|         - |  9518 | `			}` |
|        15 |  9519 | `			pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|        15 |  9520 | `			continue; /* the loop consumes the ';' as a stray separator */` |
|         - |  9521 | `		}` |
|       130 |  9522 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|       131 |  9523 | `		 \|\| (pClass->iFlags & PH7_CLASS_INTERFACE) != 0 ){` |
|         - |  9524 | `			/* php: an abstract/interface property hook cannot carry a body */` |
|       ! 0 |  9525 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9526 | `				"Abstract property hook cannot have body");` |
|       ! 0 |  9527 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9528 | `				return SXERR_ABORT;` |
|         - |  9529 | `			}` |
|       ! 0 |  9530 | `			return SXERR_CORRUPT;` |
|         - |  9531 | `		}` |
|       131 |  9532 | `		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - |  9533 | `			PH7_CLASS_PROT_PUBLIC,0,0);` |
|       131 |  9534 | `		if( pMeth == 0 ){` |
|       ! 0 |  9535 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9536 | `			return SXERR_ABORT;` |
|         - |  9537 | `		}` |
|       131 |  9538 | `		pMeth->sFunc.nLine = nHLine;` |
|       131 |  9539 | `		if( !bGet ){` |
|         - |  9540 | ``			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */`` |
|        61 |  9541 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        17 |  9542 | `				SyToken *pRp = 0;` |
|        17 |  9543 | `				pGen->pIn++;` |
|        17 |  9544 | `				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|        17 |  9545 | `				if( pRp >= pGen->pEnd ){` |
|       ! 0 |  9546 | `					goto HookSyntax;` |
|         - |  9547 | `				}` |
|        17 |  9548 | `				if( pGen->pIn < pRp ){` |
|        17 |  9549 | `					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);` |
|        17 |  9550 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9551 | `						return SXERR_ABORT;` |
|         - |  9552 | `					}` |
|         8 |  9553 | `				}` |
|        17 |  9554 | `				pGen->pIn = &pRp[1];` |
|         8 |  9555 | `			}` |
|        61 |  9556 | `			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){` |
|         - |  9557 | `				/* Implicit $value formal */` |
|         - |  9558 | `				ph7_vm_func_arg sVArg;` |
|        45 |  9559 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|        45 |  9560 | `				if( zVName == 0 ){` |
|       ! 0 |  9561 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9562 | `					return SXERR_ABORT;` |
|         - |  9563 | `				}` |
|        45 |  9564 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|        45 |  9565 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|        45 |  9566 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        45 |  9567 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        45 |  9568 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|        45 |  9569 | `				SyStringInitFromBuf(&sVArg.sTypeName,0,0);` |
|        45 |  9570 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|        22 |  9571 | `			}` |
|        30 |  9572 | `		}` |
|       165 |  9573 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - |  9574 | `			/* Block body */` |
|        69 |  9575 | `			SyToken *pBodyStart = pGen->pIn;` |
|        69 |  9576 | `			SyToken *pCloser = 0;` |
|        69 |  9577 | `			int bParentCall = 0;` |
|        69 |  9578 | `			PH7_DelimitNestedTokens(&pBodyStart[1],pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);` |
|        69 |  9579 | `			if( pCloser < pGen->pEnd ){` |
|         - |  9580 | `				SyToken *pScan;` |
|       753 |  9581 | `				for( pScan = &pBodyStart[1] ; pScan < pCloser ; pScan++ ){` |
|       687 |  9582 | `					if( GenStateIsParentHookCallAt(pScan,pCloser) ){` |
|         3 |  9583 | `						bParentCall = 1;` |
|         3 |  9584 | `						break;` |
|         - |  9585 | `					}` |
|       343 |  9586 | `				}` |
|        34 |  9587 | `			}` |
|        69 |  9588 | `			if( bParentCall ){` |
|         - |  9589 | ``				/* `parent::$x::get()` inside the body: compile a REWRITTEN copy`` |
|         - |  9590 | `				 * of the body tokens (the call becomes the parent's synthesized` |
|         - |  9591 | `				 * hook method), then continue past the original body. */` |
|         - |  9592 | `				SySet sBody;` |
|         3 |  9593 | `				SyToken *pSavedEnd = pGen->pEnd;` |
|         3 |  9594 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 |  9595 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,&pCloser[1]);` |
|         3 |  9596 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9597 | `					SySetRelease(&sBody);` |
|       ! 0 |  9598 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9599 | `					return SXERR_ABORT;` |
|         - |  9600 | `				}` |
|         3 |  9601 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 |  9602 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         3 |  9603 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|         3 |  9604 | `				pGen->pIn = &pCloser[1];` |
|         3 |  9605 | `				pGen->pEnd = pSavedEnd;` |
|         3 |  9606 | `				SySetRelease(&sBody);` |
|         3 |  9607 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9608 | `					return SXERR_ABORT;` |
|         - |  9609 | `				}` |
|         3 |  9610 | `				pMeth->sFunc.nEndLine = pCloser->nLine;` |
|         2 |  9611 | `			}else{` |
|        67 |  9612 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|        67 |  9613 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9614 | `					return SXERR_ABORT;` |
|         - |  9615 | `				}` |
|        67 |  9616 | `				pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|         - |  9617 | `			}` |
|        69 |  9618 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        17 |  9619 | `				bRefsSelf = 1;` |
|         9 |  9620 | `			}` |
|       128 |  9621 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){` |
|         - |  9622 | ``			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */`` |
|         - |  9623 | `			GenBlock *pBlock;` |
|         - |  9624 | `			SySet *pInstrContainer;` |
|         - |  9625 | `			SyToken *pBodyStart;` |
|         - |  9626 | `			SyToken *pExprEnd;` |
|        63 |  9627 | `			SyToken *pSavedEnd = 0;` |
|         - |  9628 | `			SySet sBody;` |
|        63 |  9629 | `			int bParentCall = 0;` |
|        63 |  9630 | `			pGen->pIn++; /* Jump '=>' */` |
|        63 |  9631 | `			pBodyStart = pGen->pIn;` |
|         - |  9632 | `			/* Delimit the expression (first top-level ';', or a closer that` |
|         - |  9633 | `			 * would end the enclosing hook list) and rewrite any` |
|         - |  9634 | ``			 * `parent::$x::get()` calls into the parent's synthesized hook`` |
|         - |  9635 | `			 * method on a token copy. */` |
|         - |  9636 | `			{` |
|        63 |  9637 | `				sxi32 iNest = 0;` |
|        63 |  9638 | `				pExprEnd = pBodyStart;` |
|       355 |  9639 | `				while( pExprEnd < pGen->pEnd ){` |
|       355 |  9640 | `					if( pExprEnd->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         9 |  9641 | `						iNest++;` |
|       351 |  9642 | `					}else if( pExprEnd->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         9 |  9643 | `						if( iNest <= 0 ){` |
|       ! 0 |  9644 | `							break;` |
|         - |  9645 | `						}` |
|         9 |  9646 | `						iNest--;` |
|       343 |  9647 | `					}else if( iNest <= 0 && (pExprEnd->nType & PH7_TK_SEMI) ){` |
|        63 |  9648 | `						break;` |
|         - |  9649 | `					}` |
|       293 |  9650 | `					pExprEnd++;` |
|         1 |  9651 | `				}` |
|         - |  9652 | `			}` |
|         - |  9653 | `			{` |
|         - |  9654 | `				SyToken *pScan;` |
|       335 |  9655 | `				for( pScan = pBodyStart ; pScan < pExprEnd ; pScan++ ){` |
|       275 |  9656 | `					if( GenStateIsParentHookCallAt(pScan,pExprEnd) ){` |
|         3 |  9657 | `						bParentCall = 1;` |
|         3 |  9658 | `						break;` |
|         - |  9659 | `					}` |
|       137 |  9660 | `				}` |
|         - |  9661 | `			}` |
|        63 |  9662 | `			if( bParentCall ){` |
|         3 |  9663 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 |  9664 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,pExprEnd);` |
|         3 |  9665 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9666 | `					SySetRelease(&sBody);` |
|       ! 0 |  9667 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9668 | `					return SXERR_ABORT;` |
|         - |  9669 | `				}` |
|         3 |  9670 | `				pSavedEnd = pGen->pEnd;` |
|         3 |  9671 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 |  9672 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         1 |  9673 | `			}` |
|        94 |  9674 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|        62 |  9675 | `				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);` |
|        63 |  9676 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9677 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");` |
|       ! 0 |  9678 | `				return SXERR_ABORT;` |
|         - |  9679 | `			}` |
|        63 |  9680 | `			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        63 |  9681 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|        63 |  9682 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|        63 |  9683 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        63 |  9684 | `			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        63 |  9685 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        63 |  9686 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        63 |  9687 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        63 |  9688 | `			if( bParentCall ){` |
|         3 |  9689 | `				pGen->pIn = pExprEnd; /* land on the original ';' */` |
|         3 |  9690 | `				pGen->pEnd = pSavedEnd;` |
|         3 |  9691 | `				SySetRelease(&sBody);` |
|         1 |  9692 | `			}` |
|        63 |  9693 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9694 | `				return SXERR_ABORT;` |
|         - |  9695 | `			}` |
|        63 |  9696 | `			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;` |
|        63 |  9697 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        37 |  9698 | `				bRefsSelf = 1;` |
|        18 |  9699 | `			}` |
|        63 |  9700 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        63 |  9701 | `				pGen->pIn++; /* Jump ';' */` |
|        31 |  9702 | `			}` |
|        63 |  9703 | `			if( !bGet ){` |
|         - |  9704 | ``				/* `set => expr` assigns the expression to the backing store:`` |
|         - |  9705 | `				 * the dispatcher consumes the implicit return value — which` |
|         - |  9706 | `				 * also makes the property BACKED (php: the shorthand is sugar` |
|         - |  9707 | ``				 * for `$this->NAME = expr`). */`` |
|         3 |  9708 | `				pMeth->sFunc.iFlags \|= VM_FUNC_HOOK_SET_EXPR;` |
|         3 |  9709 | `				bRefsSelf = 1;` |
|         1 |  9710 | `			}` |
|        32 |  9711 | `		}else{` |
|       ! 0 |  9712 | `			goto HookSyntax;` |
|         - |  9713 | `		}` |
|       131 |  9714 | `		rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|       131 |  9715 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  9716 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9717 | `			return SXERR_ABORT;` |
|         - |  9718 | `		}` |
|       131 |  9719 | `		pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|         1 |  9720 | `	}` |
|        95 |  9721 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|       ! 0 |  9722 | `		goto HookSyntax;` |
|         - |  9723 | `	}` |
|        95 |  9724 | `	pGen->pIn++; /* Jump '}' */` |
|        95 |  9725 | `	if( !bRefsSelf ){` |
|         - |  9726 | ``		/* php 8.4 virtual-vs-backed: no hook body referenced `$this->NAME`, so`` |
|         - |  9727 | `		 * this property is VIRTUAL — php gives it no backing store and forbids` |
|         - |  9728 | `		 * a default value (compile fatal, php's exact wording). */` |
|        41 |  9729 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_HOOK_VIRTUAL;` |
|        41 |  9730 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|       ! 0 |  9731 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9732 | `				"Cannot specify default value for virtual hooked property %z::$%z",` |
|       ! 0 |  9733 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9734 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9735 | `				return SXERR_ABORT;` |
|         - |  9736 | `			}` |
|       ! 0 |  9737 | `			return SXERR_CORRUPT;` |
|         - |  9738 | `		}` |
|        20 |  9739 | `	}` |
|        95 |  9740 | `	return SXRET_OK;` |
|       ! 0 |  9741 | `HookSyntax:` |
|       ! 0 |  9742 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9743 | `		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",` |
|       ! 0 |  9744 | `		&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9745 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  9746 | `		return SXERR_ABORT;` |
|         - |  9747 | `	}` |
|       ! 0 |  9748 | `	return SXERR_CORRUPT;` |
|        48 |  9749 | `}` |
|         - |  9750 | `/*` |
|         - |  9751 | ` * Compile an object interface.` |
|         - |  9752 | ` *  According to the PHP language reference manual` |
|         - |  9753 | ` *   Object Interfaces:` |
|         - |  9754 | ` *   Object interfaces allow you to create code which specifies which methods` |
|         - |  9755 | ` *   a class must implement, without having to define how these methods are handled.` |
|         - |  9756 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|         - |  9757 | ` *   class, but without any of the methods having their contents defined.` |
|         - |  9758 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|         - |  9759 | ` */` |
|     68870 |  9760 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|         5 |  9761 | `{` |
|     68875 |  9762 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  9763 | `	ph7_class *pClass,*pBase;` |
|         - |  9764 | `	SyToken *pEnd,*pTmp;` |
|         - |  9765 | `	SyString *pName;` |
|         - |  9766 | `	sxi32 nKwrd;` |
|         - |  9767 | `	sxi32 rc;` |
|         - |  9768 | `	/* Jump the 'interface' keyword */` |
|     68875 |  9769 | `	pGen->pIn++;` |
|         - |  9770 | `	/* Extract interface name */` |
|     68875 |  9771 | `	pName = &pGen->pIn->sData;` |
|         - |  9772 | `	/* Advance the stream cursor */` |
|     68875 |  9773 | `	pGen->pIn++;` |
|         - |  9774 | `	/* Build FQN and obtain a raw class */ {` |
|         - |  9775 | `		SyBlob sFQN;` |
|         - |  9776 | `		SyString sFQNStr;` |
|     68875 |  9777 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     68875 |  9778 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     68875 |  9779 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|     68875 |  9780 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     68875 |  9781 | `		SyBlobRelease(&sFQN);` |
|         - |  9782 | `	}` |
|     68875 |  9783 | `	if( pClass == 0 ){` |
|       ! 0 |  9784 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9785 | `		return SXERR_ABORT;` |
|         - |  9786 | `	}` |
|     68875 |  9787 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     68875 |  9788 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  9789 | `		return SXERR_ABORT;` |
|         - |  9790 | `	}` |
|         - |  9791 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|     68875 |  9792 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|         - |  9793 | `	/* Assume no base class is given */` |
|     68875 |  9794 | `	pBase = 0;` |
|     68875 |  9795 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     26755 |  9796 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     26755 |  9797 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|         - |  9798 | `			SyBlob sResolved;` |
|         - |  9799 | `			SyString sBaseName;` |
|         - |  9800 | `			sxu32 nRefLine;` |
|         - |  9801 | `			/* Extract base interface */` |
|     26755 |  9802 | `			pGen->pIn++;` |
|     26755 |  9803 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|     26755 |  9804 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     26755 |  9805 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 |  9806 | `				SyBlobRelease(&sResolved);` |
|       ! 0 |  9807 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9808 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|       ! 0 |  9809 | `					pName);` |
|       ! 0 |  9810 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9811 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9812 | `					return SXERR_ABORT;` |
|         - |  9813 | `				}` |
|       ! 0 |  9814 | `				return SXRET_OK;` |
|         - |  9815 | `			}` |
|     40130 |  9816 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|     26750 |  9817 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     26755 |  9818 | `			SyStringInitFromBuf(&sBaseName,` |
|         - |  9819 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - |  9820 | `			/* Only interfaces is allowed */` |
|     26755 |  9821 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 |  9822 | `				pBase = pBase->pNextName;` |
|       ! 0 |  9823 | `			}` |
|     26755 |  9824 | `			if( pBase == 0 ){` |
|       ! 0 |  9825 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - |  9826 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|       ! 0 |  9827 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9828 | `					SyBlobRelease(&sResolved);` |
|       ! 0 |  9829 | `					return SXERR_ABORT;` |
|         - |  9830 | `				}` |
|       ! 0 |  9831 | `			}` |
|     26755 |  9832 | `			SyBlobRelease(&sResolved);` |
|     13375 |  9833 | `		}` |
|     13375 |  9834 | `	}` |
|     68875 |  9835 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - |  9836 | `		/* Syntax error */` |
|       ! 0 |  9837 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|       ! 0 |  9838 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9839 | `		if( rc == SXERR_ABORT ){` |
|         - |  9840 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9841 | `			return SXERR_ABORT;` |
|         - |  9842 | `		}` |
|       ! 0 |  9843 | `		return SXRET_OK;` |
|         - |  9844 | `	}` |
|     68875 |  9845 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     68875 |  9846 | `	pEnd = 0; /* cc warning */` |
|         - |  9847 | `	/* Delimit the interface body */` |
|     68875 |  9848 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|     68875 |  9849 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  9850 | `		/* Syntax error */` |
|       ! 0 |  9851 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|       ! 0 |  9852 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9853 | `		if( rc == SXERR_ABORT ){` |
|         - |  9854 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9855 | `			return SXERR_ABORT;` |
|         - |  9856 | `		}` |
|       ! 0 |  9857 | `		return SXRET_OK;` |
|         - |  9858 | `	}` |
|         - |  9859 | `	/* The delimiter token is the interface body's closing brace */` |
|     68875 |  9860 | `	pClass->nEndLine = pEnd->nLine;` |
|         - |  9861 | `	/* Swap token stream */` |
|     68875 |  9862 | `	pTmp = pGen->pEnd;` |
|     68875 |  9863 | `	pGen->pEnd = pEnd;` |
|         - |  9864 | `	/* Start the parse process` |
|         - |  9865 | `	 * Note (According to the PHP reference manual):` |
|         - |  9866 | `	 *  Only constants and function signatures(without body) are allowed.` |
|         - |  9867 | `	 *  Only 'public' visibility is allowed.` |
|         - |  9868 | `	 */` |
|    126145 |  9869 | `	for(;;){` |
|         - |  9870 | `		/* Jump leading/trailing semi-colons */` |
|    435719 |  9871 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    183425 |  9872 | `			pGen->pIn++;` |
|         5 |  9873 | `		}` |
|    252299 |  9874 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - |  9875 | `			/* End of interface body */` |
|     68871 |  9876 | `			break;` |
|         - |  9877 | `		}` |
|         - |  9878 | `		/* Bind a directly-preceding docblock to this member */` |
|    183433 |  9879 | `		GenStateSetPendingDoc(&(*pGen));` |
|    183433 |  9880 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 |  9881 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  9882 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|       ! 0 |  9883 | `				&pGen->pIn->sData,pName);` |
|       ! 0 |  9884 | `			if( rc == SXERR_ABORT ){` |
|         - |  9885 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  9886 | `				return SXERR_ABORT;` |
|         - |  9887 | `			}` |
|       ! 0 |  9888 | `			goto done;` |
|         - |  9889 | `		}` |
|         - |  9890 | `		/* Extract the current keyword */` |
|    183433 |  9891 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    183433 |  9892 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - |  9893 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|         - |  9894 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|         3 |  9895 | `			const char *zKind = "member";` |
|         3 |  9896 | `			SyString *pMemberName = 0;` |
|         3 |  9897 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|         3 |  9898 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|         3 |  9899 | `				if( nNext == PH7_TKWRD_CONST ){` |
|         3 |  9900 | `					zKind = "constant";` |
|         3 |  9901 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|         3 |  9902 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|         2 |  9903 | `					}` |
|         1 |  9904 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|       ! 0 |  9905 | `					zKind = "method";` |
|       ! 0 |  9906 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       ! 0 |  9907 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       ! 0 |  9908 | `					}` |
|       ! 0 |  9909 | `				}` |
|         1 |  9910 | `			}` |
|         3 |  9911 | `			if( pMemberName ){` |
|         4 |  9912 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         1 |  9913 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|         2 |  9914 | `			}else{` |
|       ! 0 |  9915 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9916 | `					"Access type for interface %s must be public",zKind);` |
|         - |  9917 | `			}` |
|         3 |  9918 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9919 | `				return SXERR_ABORT;` |
|         - |  9920 | `			}` |
|         3 |  9921 | `			goto done;` |
|         - |  9922 | `		}` |
|    183431 |  9923 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|       ! 0 |  9924 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9925 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 |  9926 | `			if( rc == SXERR_ABORT ){` |
|         - |  9927 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  9928 | `				return SXERR_ABORT;` |
|         - |  9929 | `			}` |
|       ! 0 |  9930 | `			goto done;` |
|         - |  9931 | `		}` |
|    183431 |  9932 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|         - |  9933 | `			/* Advance the stream cursor */` |
|    129933 |  9934 | `			pGen->pIn++;` |
|    129928 |  9935 | `			if( pGen->pIn < pGen->pEnd` |
|    129933 |  9936 | `			 && ((pGen->pIn->nType & PH7_TK_DOLLAR) != 0` |
|    129928 |  9937 | `			  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         - |  9938 | ``				/* PHP 8.4: `public [?T] $x { get; set; }` — a hooked-property`` |
|         - |  9939 | `				 * requirement. The attribute compiler + hook parser handle it` |
|         - |  9940 | `				 * (bare hooks are implicitly abstract inside an interface; a` |
|         - |  9941 | `				 * property without hooks is ITS "Interfaces may only include` |
|         - |  9942 | `				 * hooked properties" error). */` |
|       ! 0 |  9943 | `				rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 |  9944 | `					PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 |  9945 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9946 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9947 | `						return SXERR_ABORT;` |
|         - |  9948 | `					}` |
|       ! 0 |  9949 | `					goto done;` |
|         - |  9950 | `				}` |
|       ! 0 |  9951 | `				continue;` |
|         - |  9952 | `			}` |
|    129933 |  9953 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|         - |  9954 | `				/* A type NAME (a plain identifier, e.g. a class type) followed by` |
|         - |  9955 | `				 * '$' also opens a hooked-property requirement. */` |
|       ! 0 |  9956 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) != 0` |
|       ! 0 |  9957 | `				 && (pGen->pIn + 1) < pGen->pEnd` |
|       ! 0 |  9958 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|       ! 0 |  9959 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 |  9960 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 |  9961 | `					if( rc != SXRET_OK ){` |
|       ! 0 |  9962 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  9963 | `							return SXERR_ABORT;` |
|         - |  9964 | `						}` |
|       ! 0 |  9965 | `						goto done;` |
|         - |  9966 | `					}` |
|       ! 0 |  9967 | `					continue;` |
|         - |  9968 | `				}` |
|       ! 0 |  9969 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9970 | `					"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 |  9971 | `				if( rc == SXERR_ABORT ){` |
|         - |  9972 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9973 | `					return SXERR_ABORT;` |
|         - |  9974 | `				}` |
|       ! 0 |  9975 | `				goto done;` |
|         - |  9976 | `			}` |
|    129933 |  9977 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    129933 |  9978 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|         - |  9979 | `				/* A type KEYWORD (int/string/bool/…) followed by '$' opens a` |
|         - |  9980 | `				 * hooked-property requirement (PHP 8.4). */` |
|         4 |  9981 | `				if( (pGen->pIn + 1) < pGen->pEnd` |
|         5 |  9982 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|         7 |  9983 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|         2 |  9984 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|         5 |  9985 | `					if( rc != SXRET_OK ){` |
|       ! 0 |  9986 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  9987 | `							return SXERR_ABORT;` |
|         - |  9988 | `						}` |
|       ! 0 |  9989 | `						goto done;` |
|         - |  9990 | `					}` |
|         5 |  9991 | `					continue;` |
|         - |  9992 | `				}` |
|       ! 0 |  9993 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9994 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 |  9995 | `				if( rc == SXERR_ABORT ){` |
|         - |  9996 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9997 | `					return SXERR_ABORT;` |
|         - |  9998 | `				}` |
|       ! 0 |  9999 | `				goto done;` |
|         - | 10000 | `			}` |
|     64962 | 10001 | `		}` |
|    183427 | 10002 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|         - | 10003 | `			/* Parse constant */` |
|     53499 | 10004 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|     53499 | 10005 | `			if( rc != SXRET_OK ){` |
|         3 | 10006 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10007 | `					return SXERR_ABORT;` |
|         - | 10008 | `				}` |
|         3 | 10009 | `				goto done;` |
|         - | 10010 | `			}` |
|     26751 | 10011 | `		}else{` |
|    129933 | 10012 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|    129933 | 10013 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - | 10014 | `				/* Static method,record that */` |
|     11465 | 10015 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|         - | 10016 | `				/* Advance the stream cursor */` |
|     11465 | 10017 | `				pGen->pIn++;` |
|     11460 | 10018 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     11465 | 10019 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 10020 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10021 | `							"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 | 10022 | `						if( rc == SXERR_ABORT ){` |
|         - | 10023 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 10024 | `							return SXERR_ABORT;` |
|         - | 10025 | `						}` |
|       ! 0 | 10026 | `						goto done;` |
|         - | 10027 | `				}` |
|      5730 | 10028 | `			}` |
|         - | 10029 | `			/* Process method signature (no body for interface methods) */` |
|    129933 | 10030 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|    129933 | 10031 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 10032 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10033 | `					return SXERR_ABORT;` |
|         - | 10034 | `				}` |
|       ! 0 | 10035 | `				goto done;` |
|         - | 10036 | `			}` |
|         - | 10037 | `		}` |
|         5 | 10038 | `	}` |
|         - | 10039 | `	/* Install the interface */` |
|     68871 | 10040 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     68871 | 10041 | `	if( rc == SXRET_OK && pBase ){` |
|         - | 10042 | `		/* Inherit from the base interface */` |
|     26755 | 10043 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|     13375 | 10044 | `	}` |
|     68871 | 10045 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 10046 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10047 | `		return SXERR_ABORT;` |
|         - | 10048 | `	}` |
|     34433 | 10049 | `done:` |
|         - | 10050 | `	/* Point beyond the interface body */` |
|     68875 | 10051 | `	pGen->pIn  = &pEnd[1];` |
|     68875 | 10052 | `	pGen->pEnd = pTmp;` |
|     68875 | 10053 | `	return PH7_OK;` |
|     34440 | 10054 | `}` |
|         - | 10055 | `/*` |
|         - | 10056 | ` * Compile a user-defined class.` |
|         - | 10057 | ` * According to the PHP language reference manual` |
|         - | 10058 | ` *  class` |
|         - | 10059 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|         - | 10060 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|         - | 10061 | ` *  of the properties and methods belonging to the class.` |
|         - | 10062 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|         - | 10063 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|         - | 10064 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|         - | 10065 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - | 10066 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|         - | 10067 | ` *  (called "methods").` |
|         - | 10068 | ` */` |
|         - | 10069 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|         - | 10070 | `typedef struct TraitUseEntry TraitUseEntry;` |
|         - | 10071 | `struct TraitUseEntry {` |
|         - | 10072 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|         - | 10073 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|         - | 10074 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|         - | 10075 | `};` |
|         - | 10076 | `/*` |
|         - | 10077 | ` * Validate that methods implementing interface contracts have compatible` |
|         - | 10078 | ` * signatures: public visibility and at least as many parameters as declared.` |
|         - | 10079 | ` */` |
|    353236 | 10080 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10081 | `{` |
|         - | 10082 | `	ph7_class **apIface;` |
|         - | 10083 | `	sxu32 nIface,i;` |
|         - | 10084 | `	sxi32 rc;` |
|    353241 | 10085 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|       ! 0 | 10086 | `		return SXRET_OK;` |
|         - | 10087 | `	}` |
|    353241 | 10088 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    353241 | 10089 | `	nIface = SySetUsed(&pClass->aInterface);` |
|    708841 | 10090 | `	for(i = 0; i < nIface; i++){` |
|    355605 | 10091 | `		ph7_class *pIface = apIface[i];` |
|         - | 10092 | `		SyHashEntry *pEntry;` |
|    355605 | 10093 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   1024691 | 10094 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|    669091 | 10095 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|         - | 10096 | `			ph7_class_method *pImplMeth;` |
|    669091 | 10097 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|         - | 10098 | `			/* Find the implementing method in the class */` |
|    669091 | 10099 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|    669091 | 10100 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        23 | 10101 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|         - | 10102 | `			}` |
|         - | 10103 | `			/* Check visibility: interface methods must be implemented as public */` |
|    669073 | 10104 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|         4 | 10105 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - | 10106 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|         1 | 10107 | `					&pClass->sName,pMName,&pIface->sName);` |
|         3 | 10108 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10109 | `					return SXERR_ABORT;` |
|         - | 10110 | `				}` |
|         1 | 10111 | `			}` |
|         - | 10112 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|         - | 10113 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|         - | 10114 | `			 */` |
|         - | 10115 | `			{` |
|    669073 | 10116 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|    669073 | 10117 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|    669073 | 10118 | `				int sigError = 0;` |
|    669073 | 10119 | `				if( nImplArgs < nIfaceArgs ){` |
|         3 | 10120 | `					sigError = 1;` |
|    669072 | 10121 | `				}else if( nImplArgs > nIfaceArgs ){` |
|         - | 10122 | `					/* Extra parameters must all have default values */` |
|      3829 | 10123 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|         - | 10124 | `					sxu32 k;` |
|      7651 | 10125 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|      3829 | 10126 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|         3 | 10127 | `							sigError = 1;` |
|         3 | 10128 | `							break;` |
|         - | 10129 | `						}` |
|      1916 | 10130 | `					}` |
|      1912 | 10131 | `				}` |
|    669073 | 10132 | `				if( sigError ){` |
|         - | 10133 | `					SyBlob sImplSig, sIfaceSig;` |
|         - | 10134 | `					ph7_vm_func_arg *aArgs;` |
|         - | 10135 | `					sxu32 j;` |
|         6 | 10136 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|         6 | 10137 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|         - | 10138 | `					/* Build implementing method signature */` |
|         6 | 10139 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|        12 | 10140 | `					for(j = 0; j < nImplArgs; j++){` |
|         8 | 10141 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|         8 | 10142 | `						SyBlobAppend(&sImplSig,"$",1);` |
|         8 | 10143 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 | 10144 | `					}` |
|         - | 10145 | `					/* Build interface method signature */` |
|         6 | 10146 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|        12 | 10147 | `					for(j = 0; j < nIfaceArgs; j++){` |
|         8 | 10148 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|         8 | 10149 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|         8 | 10150 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 | 10151 | `					}` |
|         8 | 10152 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - | 10153 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|         2 | 10154 | `						&pClass->sName,pMName,` |
|         4 | 10155 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|         2 | 10156 | `						&pIface->sName,pMName,` |
|         4 | 10157 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|         6 | 10158 | `					SyBlobRelease(&sImplSig);` |
|         6 | 10159 | `					SyBlobRelease(&sIfaceSig);` |
|         6 | 10160 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10161 | `						return SXERR_ABORT;` |
|         - | 10162 | `					}` |
|         2 | 10163 | `				}` |
|         - | 10164 | `			}` |
|         5 | 10165 | `		}` |
|    177805 | 10166 | `	}` |
|    353241 | 10167 | `	return SXRET_OK;` |
|    176623 | 10168 | `}` |
|         - | 10169 | `/*` |
|         - | 10170 | ` * An abstract property-hook stub (__phl_hook_{get,set}_NAME) is satisfied by` |
|         - | 10171 | ` * the class declaring a PLAIN (non-abstract, non-hooked) property NAME: php` |
|         - | 10172 | `` * lets a plain property implement `{ get; set; }` requirements — its raw`` |
|         - | 10173 | ` * read/write IS the default get/set. A concrete hook override replaced the` |
|         - | 10174 | ` * stub in hMethod already, so a surviving stub next to a HOOKED property` |
|         - | 10175 | ` * means that specific hook is still missing.` |
|         - | 10176 | ` */` |
|        38 | 10177 | `static int GenStateAbstractHookSatisfied(ph7_class *pClass,const SyString *pMName)` |
|         5 | 10178 | `{` |
|         - | 10179 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|         - | 10180 | `	ph7_class_attr *pProp;` |
|        38 | 10181 | `	if( pMName->nByte <= nPfx` |
|        27 | 10182 | `	 \|\| (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) != 0` |
|         4 | 10183 | `	  && SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) != 0) ){` |
|        36 | 10184 | `		return 0; /* not a hook stub */` |
|         - | 10185 | `	}` |
|         7 | 10186 | `	pProp = PH7_ClassExtractAttribute(pClass,&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|         7 | 10187 | `	return pProp != 0` |
|         6 | 10188 | `		&& (pProp->iFlags & (PH7_CLASS_ATTR_ABSTRACT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|         3 | 10189 | `			\|PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) == 0;` |
|        24 | 10190 | `}` |
|         - | 10191 | `/*` |
|         - | 10192 | ` * Append an abstract member's display name to the message blob, translating a` |
|         - | 10193 | `` * property-hook stub (__phl_hook_get_x) to php's `$x::get` form.`` |
|         - | 10194 | ` */` |
|        16 | 10195 | `static void GenStateAppendAbstractMemberName(SyBlob *pMsg,const SyString *pMName)` |
|         4 | 10196 | `{` |
|         - | 10197 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|        16 | 10198 | `	if( pMName->nByte > nPfx` |
|        12 | 10199 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|       ! 0 | 10200 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0) ){` |
|       ! 0 | 10201 | `		SyBlobAppend(pMsg,"$",1);` |
|       ! 0 | 10202 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|       ! 0 | 10203 | `		SyBlobAppend(pMsg,"::",2);` |
|       ! 0 | 10204 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[sizeof("__phl_hook_")-1],3);` |
|       ! 0 | 10205 | `		return;` |
|         - | 10206 | `	}` |
|        20 | 10207 | `	SyBlobAppend(pMsg,(const void *)pMName->zString,pMName->nByte);` |
|        12 | 10208 | `}` |
|         - | 10209 | `/*` |
|         - | 10210 | ` * Check that a concrete class has no remaining abstract methods.` |
|         - | 10211 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|         - | 10212 | ` */` |
|    353236 | 10213 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10214 | `{` |
|         - | 10215 | `	ph7_class_method *pMeth;` |
|         - | 10216 | `	SyHashEntry *pEntry;` |
|         - | 10217 | `	sxu32 nAbstract;` |
|         - | 10218 | `	SyBlob sMsg;` |
|         - | 10219 | `	sxi32 rc;` |
|         - | 10220 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|    353241 | 10221 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     15337 | 10222 | `		return SXRET_OK;` |
|         - | 10223 | `	}` |
|         - | 10224 | `	/* Count abstract methods */` |
|    337909 | 10225 | `	nAbstract = 0;` |
|    337909 | 10226 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   5001761 | 10227 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   4494905 | 10228 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   4494905 | 10229 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        27 | 10230 | `			if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|         7 | 10231 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - | 10232 | `			}` |
|        20 | 10233 | `			nAbstract++;` |
|         8 | 10234 | `		}` |
|         5 | 10235 | `	}` |
|    337909 | 10236 | `	if( nAbstract == 0 ){` |
|    337895 | 10237 | `		return SXRET_OK;` |
|         - | 10238 | `	}` |
|         - | 10239 | `	/* Build the error message listing all abstract methods with origins */` |
|        18 | 10240 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|        18 | 10241 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|         - | 10242 | `		"be declared abstract or implement the remaining method%s (",` |
|         7 | 10243 | `		&pClass->sName,nAbstract,` |
|         7 | 10244 | `		(nAbstract > 1 ? "s" : ""),` |
|         7 | 10245 | `		(nAbstract > 1 ? "s" : ""));` |
|         - | 10246 | `	/* Second pass: list methods with origins */` |
|         - | 10247 | `	{` |
|        18 | 10248 | `		sxu32 nListed = 0;` |
|        18 | 10249 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|        36 | 10250 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|        22 | 10251 | `			ph7_class *pOrigin = 0;` |
|         - | 10252 | `			SyString *pMName;` |
|        22 | 10253 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        22 | 10254 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|         3 | 10255 | `				continue;` |
|         - | 10256 | `			}` |
|        20 | 10257 | `			pMName = &pMeth->sFunc.sName;` |
|        20 | 10258 | `			if( GenStateAbstractHookSatisfied(pClass,pMName) ){` |
|       ! 0 | 10259 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - | 10260 | `			}` |
|        20 | 10261 | `			if( nListed > 0 ){` |
|         3 | 10262 | `				SyBlobAppend(&sMsg,", ",2);` |
|         1 | 10263 | `			}` |
|         - | 10264 | `			/* Find the origin of this abstract method.` |
|         - | 10265 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|         - | 10266 | `			 * inheritance chains) take precedence for interface-declared` |
|         - | 10267 | `			 * methods. Abstract class methods only win when the class` |
|         - | 10268 | `			 * itself declared the abstract method (not inherited from` |
|         - | 10269 | `			 * an interface). Trait methods are adopted into the using` |
|         - | 10270 | `			 * class's namespace.` |
|         - | 10271 | `			 */` |
|         - | 10272 | `			{` |
|         - | 10273 | `				ph7_class **apIface;` |
|         - | 10274 | `				ph7_class **apTrait;` |
|         - | 10275 | `				ph7_class *pWalk;` |
|         - | 10276 | `				sxu32 i;` |
|         - | 10277 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|         - | 10278 | `				 * (one that was written in the class body, not inherited from an` |
|         - | 10279 | `				 * interface). PHP attributes origin to the declaring class.` |
|         - | 10280 | `				 */` |
|        20 | 10281 | `				if( pClass->pBase ){` |
|        11 | 10282 | `					pWalk = pClass->pBase;` |
|        19 | 10283 | `					while( pWalk ){` |
|         - | 10284 | `						ph7_class_method *pParentMeth;` |
|        13 | 10285 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|        13 | 10286 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|         - | 10287 | `							/* Exclude methods that came from an interface anywhere` |
|         - | 10288 | `							 * in this class's ancestor chain.` |
|         - | 10289 | `							 */` |
|        13 | 10290 | `							int fromIface = 0;` |
|        13 | 10291 | `							ph7_class *pAnc = pWalk;` |
|        17 | 10292 | `							while( pAnc ){` |
|         - | 10293 | `								ph7_class **apPI;` |
|         - | 10294 | `								sxu32 j;` |
|        15 | 10295 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|        15 | 10296 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|        10 | 10297 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|        10 | 10298 | `										fromIface = 1;` |
|        10 | 10299 | `										break;` |
|         - | 10300 | `									}` |
|       ! 0 | 10301 | `								}` |
|        15 | 10302 | `								if( fromIface ) break;` |
|         6 | 10303 | `								pAnc = pAnc->pBase;` |
|         2 | 10304 | `							}` |
|        13 | 10305 | `							if( !fromIface ){` |
|         3 | 10306 | `								pOrigin = pWalk;` |
|         3 | 10307 | `								break;` |
|         - | 10308 | `							}` |
|         4 | 10309 | `						}` |
|        10 | 10310 | `						pWalk = pWalk->pBase;` |
|         2 | 10311 | `					}` |
|         4 | 10312 | `				}` |
|         - | 10313 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|         - | 10314 | `				 * each interface's own parent chain for the deepest origin.` |
|         - | 10315 | `				 */` |
|        20 | 10316 | `				if( !pOrigin ){` |
|        18 | 10317 | `					pWalk = pClass;` |
|        40 | 10318 | `					while( pWalk && !pOrigin ){` |
|        26 | 10319 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|        26 | 10320 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|        16 | 10321 | `							ph7_class *pIface = apIface[i];` |
|        16 | 10322 | `							ph7_class *pDeepest = 0;` |
|        28 | 10323 | `							while( pIface ){` |
|        16 | 10324 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|        16 | 10325 | `									pDeepest = pIface;` |
|         6 | 10326 | `								}` |
|        16 | 10327 | `								pIface = pIface->pBase;` |
|         4 | 10328 | `							}` |
|        16 | 10329 | `							if( pDeepest ){` |
|        16 | 10330 | `								pOrigin = pDeepest;` |
|        16 | 10331 | `								break;` |
|         - | 10332 | `							}` |
|       ! 0 | 10333 | `						}` |
|        26 | 10334 | `						pWalk = pWalk->pBase;` |
|         4 | 10335 | `					}` |
|         7 | 10336 | `				}` |
|         - | 10337 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|        20 | 10338 | `				if( !pOrigin ){` |
|         3 | 10339 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|         3 | 10340 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|         3 | 10341 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|         3 | 10342 | `							pOrigin = pClass;` |
|         3 | 10343 | `							break;` |
|         - | 10344 | `						}` |
|       ! 0 | 10345 | `					}` |
|         1 | 10346 | `				}` |
|         - | 10347 | `			}` |
|        20 | 10348 | `			if( pOrigin ){` |
|        20 | 10349 | `				SyBlobFormat(&sMsg,"%z::",&pOrigin->sName);` |
|        12 | 10350 | `			}else{` |
|         - | 10351 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|       ! 0 | 10352 | `				SyBlobFormat(&sMsg,"%z::",&pClass->sName);` |
|         - | 10353 | `			}` |
|        20 | 10354 | `			GenStateAppendAbstractMemberName(&sMsg,pMName);` |
|        20 | 10355 | `			nListed++;` |
|         4 | 10356 | `		}` |
|         - | 10357 | `	}` |
|        18 | 10358 | `	SyBlobAppend(&sMsg,")",1);` |
|        25 | 10359 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|        14 | 10360 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|        18 | 10361 | `	SyBlobRelease(&sMsg);` |
|        18 | 10362 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 10363 | `		return SXERR_ABORT;` |
|         - | 10364 | `	}` |
|        18 | 10365 | `	return SXRET_OK;` |
|    176623 | 10366 | `}` |
|         - | 10367 | `/*` |
|         - | 10368 | ` * Parse a class/interface name reference from the current token stream.` |
|         - | 10369 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|         - | 10370 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|         - | 10371 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|         - | 10372 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|         - | 10373 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|         - | 10374 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|         - | 10375 | ` */` |
|    399378 | 10376 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|         5 | 10377 | `{` |
|    399383 | 10378 | `	int isAbsolute = 0;` |
|    399383 | 10379 | `	SyToken *pStart = pGen->pIn;` |
|         - | 10380 | `	SyBlob sName;` |
|    399383 | 10381 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      4421 | 10382 | `		isAbsolute = 1;` |
|      4421 | 10383 | `		pGen->pIn++;` |
|      2208 | 10384 | `	}` |
|    399383 | 10385 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         9 | 10386 | `		pGen->pIn = pStart;` |
|         9 | 10387 | `		return SXERR_INVALID;` |
|         - | 10388 | `	}` |
|    399377 | 10389 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|    399377 | 10390 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|    399377 | 10391 | `	pGen->pIn++;` |
|    599079 | 10392 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|    199712 | 10393 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        16 | 10394 | `		SyBlobAppend(&sName,"\\",1);` |
|        16 | 10395 | `		pGen->pIn++;` |
|        16 | 10396 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        16 | 10397 | `		pGen->pIn++;` |
|         2 | 10398 | `	}` |
|    399377 | 10399 | `	if( isAbsolute ){` |
|      4419 | 10400 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      2212 | 10401 | `	}else{` |
|         - | 10402 | `		SyString sRaw;` |
|    394963 | 10403 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|    394963 | 10404 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|         - | 10405 | `	}` |
|    399377 | 10406 | `	SyBlobRelease(&sName);` |
|    399377 | 10407 | `	return SXRET_OK;` |
|    199694 | 10408 | `}` |
|         - | 10409 | `/*` |
|         - | 10410 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|         - | 10411 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|         - | 10412 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|         - | 10413 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|         - | 10414 | ` * either direction cannot run unbounded.` |
|         - | 10415 | ` */` |
|         - | 10416 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    164466 | 10417 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|         5 | 10418 | `{` |
|         - | 10419 | `	ph7_class **apParent;` |
|         - | 10420 | `	sxu32 n;` |
|    428295 | 10421 | `	while( pInterface ){` |
|    271479 | 10422 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|       ! 0 | 10423 | `			return FALSE;` |
|         - | 10424 | `		}` |
|    305879 | 10425 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|     68800 | 10426 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|      7655 | 10427 | `			return TRUE;` |
|         - | 10428 | `		}` |
|    263829 | 10429 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    263829 | 10430 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|       ! 0 | 10431 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|       ! 0 | 10432 | `				return TRUE;` |
|         - | 10433 | `			}` |
|       ! 0 | 10434 | `		}` |
|    263829 | 10435 | `		pInterface = pInterface->pBase;` |
|    263829 | 10436 | `		iDepth++;` |
|         5 | 10437 | `	}` |
|    156821 | 10438 | `	return FALSE;` |
|     82238 | 10439 | `}` |
|    164466 | 10440 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|         5 | 10441 | `{` |
|    164471 | 10442 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|         5 | 10443 | `}` |
|         - | 10444 | `/*` |
|         - | 10445 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|         - | 10446 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|         - | 10447 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|         - | 10448 | ` */` |
|      7650 | 10449 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|         5 | 10450 | `{` |
|      7659 | 10451 | `	while( pBase ){` |
|        10 | 10452 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|         2 | 10453 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|         3 | 10454 | `			return TRUE;` |
|         - | 10455 | `		}` |
|        10 | 10456 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|         6 | 10457 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|         3 | 10458 | `			return TRUE;` |
|         - | 10459 | `		}` |
|         5 | 10460 | `		pBase = pBase->pBase;` |
|         1 | 10461 | `	}` |
|      7651 | 10462 | `	return FALSE;` |
|      3830 | 10463 | `}` |
|         - | 10464 | `/*` |
|         - | 10465 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|         - | 10466 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|         - | 10467 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|         - | 10468 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|         - | 10469 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|         - | 10470 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|         - | 10471 | ` * pClass->aEnumCases for cases().` |
|         - | 10472 | ` */` |
|      7682 | 10473 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10474 | `{` |
|      7687 | 10475 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 10476 | `	SySet *pInstrContainer;` |
|         - | 10477 | `	ph7_class_attr *pCase;` |
|         - | 10478 | `	SyString *pName;` |
|         - | 10479 | `	sxi32 rc;` |
|      7687 | 10480 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|      7687 | 10481 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 10482 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10483 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|       ! 0 | 10484 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10485 | `			return SXERR_ABORT;` |
|         - | 10486 | `		}` |
|       ! 0 | 10487 | `		goto Synchronize;` |
|         - | 10488 | `	}` |
|      7687 | 10489 | `	pName = &pGen->pIn->sData;` |
|         - | 10490 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|      7687 | 10491 | `	if( SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte) != 0 ){` |
|       ! 0 | 10492 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10493 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|       ! 0 | 10494 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10495 | `			return SXERR_ABORT;` |
|         - | 10496 | `		}` |
|       ! 0 | 10497 | `		goto Synchronize;` |
|         - | 10498 | `	}` |
|      7687 | 10499 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10500 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|      7687 | 10501 | `	if( pCase == 0 ){` |
|       ! 0 | 10502 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10503 | `		return SXERR_ABORT;` |
|         - | 10504 | `	}` |
|      7687 | 10505 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|      7687 | 10506 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 10507 | `		return SXERR_ABORT;` |
|         - | 10508 | `	}` |
|      7687 | 10509 | `	pGen->pIn++; /* Jump the case name */` |
|      7687 | 10510 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|      7673 | 10511 | `		if( pClass->nEnumBacking == 0 ){` |
|         8 | 10512 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         2 | 10513 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|         6 | 10514 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10515 | `				return SXERR_ABORT;` |
|         - | 10516 | `			}` |
|         6 | 10517 | `			goto Synchronize;` |
|         - | 10518 | `		}` |
|      7669 | 10519 | `		pGen->pIn++; /* Jump the equal sign */` |
|         - | 10520 | `		/* Compile the backing value expression into the case's own container` |
|         - | 10521 | `		 * (same technique as class constants). */` |
|      7669 | 10522 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7669 | 10523 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|      7669 | 10524 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7669 | 10525 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 | 10526 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10527 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|       ! 0 | 10528 | `		}` |
|      7669 | 10529 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7669 | 10530 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7669 | 10531 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10532 | `			return SXERR_ABORT;` |
|         - | 10533 | `		}` |
|      3837 | 10534 | `	}else{` |
|        17 | 10535 | `		if( pClass->nEnumBacking != 0 ){` |
|       ! 0 | 10536 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10537 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|       ! 0 | 10538 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10539 | `				return SXERR_ABORT;` |
|         - | 10540 | `			}` |
|       ! 0 | 10541 | `			goto Synchronize;` |
|         - | 10542 | `		}` |
|         - | 10543 | `	}` |
|      7683 | 10544 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|      7683 | 10545 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 10546 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10547 | `		return SXERR_ABORT;` |
|         - | 10548 | `	}` |
|      7683 | 10549 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|      7683 | 10550 | `	return SXRET_OK;` |
|         2 | 10551 | `Synchronize:` |
|         - | 10552 | `	/* Synchronize with the first semi-colon */` |
|        14 | 10553 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|        10 | 10554 | `		pGen->pIn++;` |
|         2 | 10555 | `	}` |
|         6 | 10556 | `	return SXERR_CORRUPT;` |
|      3846 | 10557 | `}` |
|         - | 10558 | `/*` |
|         - | 10559 | ` * Synthesize the enum interface methods (PHP 8.1): cases() for every enum,` |
|         - | 10560 | ` * plus from()/tryFrom() for backed enums. Each is an ordinary public static` |
|         - | 10561 | ` * method whose body forwards to a __phl_enum_* engine thunk (vm.c) with the` |
|         - | 10562 | ` * enum's FQN embedded as a literal — the same forwarder pattern the` |
|         - | 10563 | ` * Generator/Fiber/Reflection builtins use. The source buffer is owned by the` |
|         - | 10564 | ` * VM allocator and never freed: tokens (method and parameter names) keep` |
|         - | 10565 | ` * pointers into it (see the constructor-promotion precedent above).` |
|         - | 10566 | ` */` |
|      3844 | 10567 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10568 | `{` |
|         - | 10569 | `	SyToken *pSaveIn,*pSaveEnd;` |
|         - | 10570 | `	const char *zBack;` |
|         - | 10571 | `	SySet sToken;` |
|         - | 10572 | `	char *zSrc;` |
|         - | 10573 | `	sxu32 nSrc,nMax;` |
|      3849 | 10574 | `	sxi32 rc = SXRET_OK;` |
|      3849 | 10575 | `	nMax = 3*(sxu32)sizeof("function tryFrom(string $value){return __phl_enum_tryfrom('',$value);}")` |
|      3844 | 10576 | `		+ 3*SyStringLength(&pClass->sName) + 64;` |
|      3849 | 10577 | `	zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nMax);` |
|      3849 | 10578 | `	if( zSrc == 0 ){` |
|       ! 0 | 10579 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10580 | `		return SXERR_ABORT;` |
|         - | 10581 | `	}` |
|      3849 | 10582 | `	zBack = (pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string";` |
|      3849 | 10583 | `	if( pClass->nEnumBacking != 0 ){` |
|      5753 | 10584 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         - | 10585 | `			"function cases(){return __phl_enum_cases('%z');}"` |
|         - | 10586 | `			"function from(%s $value){return __phl_enum_from('%z',$value);}"` |
|         - | 10587 | `			"function tryFrom(%s $value){return __phl_enum_tryfrom('%z',$value);}",` |
|      1916 | 10588 | `			&pClass->sName,zBack,&pClass->sName,zBack,&pClass->sName);` |
|      1921 | 10589 | `	}else{` |
|        21 | 10590 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         6 | 10591 | `			"function cases(){return __phl_enum_cases('%z');}",&pClass->sName);` |
|         - | 10592 | `	}` |
|      3849 | 10593 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      3849 | 10594 | `	PH7_TokenizePHP(zSrc,nSrc,pClass->nLine,&sToken,0);` |
|      3849 | 10595 | `	pSaveIn = pGen->pIn;` |
|      3849 | 10596 | `	pSaveEnd = pGen->pEnd;` |
|      3849 | 10597 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      3849 | 10598 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|     15357 | 10599 | `	while( pGen->pIn < pGen->pEnd && rc != SXERR_ABORT ){` |
|     11513 | 10600 | `		rc = GenStateCompileClassMethod(&(*pGen),PH7_TKWRD_PUBLIC,PH7_CLASS_ATTR_STATIC,TRUE,pClass);` |
|         5 | 10601 | `	}` |
|      3849 | 10602 | `	pGen->pIn = pSaveIn;` |
|      3849 | 10603 | `	pGen->pEnd = pSaveEnd;` |
|      3849 | 10604 | `	SySetRelease(&sToken);` |
|      3849 | 10605 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXRET_OK;` |
|      1927 | 10606 | `}` |
|         - | 10607 | `/*` |
|         - | 10608 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|         - | 10609 | ` * __call/__callStatic/__invoke stay allowed).` |
|         - | 10610 | ` */` |
|         - | 10611 | `static const char *azEnumBannedMagic[] = {` |
|         - | 10612 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|         - | 10613 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|         - | 10614 | `};` |
|         - | 10615 | `/*` |
|         - | 10616 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|         - | 10617 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|         - | 10618 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|         - | 10619 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|         - | 10620 | ` * and before the class is installed.` |
|         - | 10621 | ` */` |
|      3844 | 10622 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|         5 | 10623 | `{` |
|         - | 10624 | `	SyHashEntry *pEntry;` |
|         - | 10625 | `	sxi32 rc;` |
|         - | 10626 | `	sxu32 n;` |
|         - | 10627 | `	/* php: "Enum %s cannot include properties" */` |
|      3849 | 10628 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     11531 | 10629 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      7689 | 10630 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      7689 | 10631 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|         3 | 10632 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|         1 | 10633 | `				"Enum %z cannot include properties",&pClass->sName);` |
|         3 | 10634 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10635 | `				return SXERR_ABORT;` |
|         - | 10636 | `			}` |
|         3 | 10637 | `			break;` |
|         - | 10638 | `		}` |
|         5 | 10639 | `	}` |
|         - | 10640 | `	/* php: "Enum %s cannot include magic method %s" */` |
|     53821 | 10641 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|     74958 | 10642 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|     49977 | 10643 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|       ! 0 | 10644 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10645 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|       ! 0 | 10646 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10647 | `				return SXERR_ABORT;` |
|         - | 10648 | `			}` |
|       ! 0 | 10649 | `		}` |
|     24991 | 10650 | `	}` |
|         - | 10651 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|         - | 10652 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|         - | 10653 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|         - | 10654 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|         - | 10655 | `	{` |
|         - | 10656 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|         - | 10657 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|         - | 10658 | `		ph7_class_attr *pAttr;` |
|      3849 | 10659 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10660 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      3849 | 10661 | `		if( pAttr == 0 ){` |
|       ! 0 | 10662 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10663 | `			return SXERR_ABORT;` |
|         - | 10664 | `		}` |
|      3849 | 10665 | `		pAttr->nType = MEMOBJ_STRING;` |
|      3849 | 10666 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|      3849 | 10667 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|      3849 | 10668 | `		if( pClass->nEnumBacking != 0 ){` |
|      3837 | 10669 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10670 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      3837 | 10671 | `			if( pAttr == 0 ){` |
|       ! 0 | 10672 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10673 | `				return SXERR_ABORT;` |
|         - | 10674 | `			}` |
|      3837 | 10675 | `			pAttr->nType = pClass->nEnumBacking;` |
|      3837 | 10676 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|         7 | 10677 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|         4 | 10678 | `			}else{` |
|      3831 | 10679 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|         - | 10680 | `			}` |
|      3837 | 10681 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|      1916 | 10682 | `		}` |
|         - | 10683 | `	}` |
|      3849 | 10684 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|      1927 | 10685 | `}` |
|         - | 10686 | `/*` |
|         - | 10687 | ` * Compile a class declaration, named or anonymous.` |
|         - | 10688 | ` *` |
|         - | 10689 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|         - | 10690 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|         - | 10691 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|         - | 10692 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|         - | 10693 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|         - | 10694 | ` * implements, body, install) is shared by both paths.` |
|         - | 10695 | ` */` |
|    353280 | 10696 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|         - | 10697 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|         5 | 10698 | `{` |
|    353285 | 10699 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 10700 | `	ph7_class *pClass,*pBase;` |
|         - | 10701 | `	SyToken *pEnd,*pTmp;` |
|         - | 10702 | `	sxi32 iProtection;` |
|         - | 10703 | `	SySet aInterfaces;` |
|         - | 10704 | `	SySet aUseEntries;` |
|         - | 10705 | `	sxi32 iAttrflags;` |
|         - | 10706 | `	SyString *pName;` |
|         - | 10707 | `	sxi32 nKwrd;` |
|         - | 10708 | `	sxi32 rc;` |
|         - | 10709 | `	/* Jump the 'class' keyword */` |
|    353285 | 10710 | `	pGen->pIn++;` |
|    353285 | 10711 | `	if( pAnonName ){` |
|         - | 10712 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|         - | 10713 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|         - | 10714 | `		 * then use the synthesized name. */` |
|        32 | 10715 | `		*ppArgStart = *ppArgEnd = 0;` |
|        32 | 10716 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         7 | 10717 | `			pGen->pIn++; /* Jump '(' */` |
|         7 | 10718 | `			*ppArgStart = pGen->pIn;` |
|        10 | 10719 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|         3 | 10720 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|         7 | 10721 | `			pGen->pIn = *ppArgEnd;` |
|         7 | 10722 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|         3 | 10723 | `		}` |
|        32 | 10724 | `		pName = pAnonName;` |
|        32 | 10725 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|        18 | 10726 | `	}else{` |
|    353257 | 10727 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - | 10728 | `			/* Syntax error */` |
|       ! 0 | 10729 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|       ! 0 | 10730 | `			if( rc == SXERR_ABORT ){` |
|         - | 10731 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 10732 | `				return SXERR_ABORT;` |
|         - | 10733 | `			}` |
|         - | 10734 | `			/* Synchronize with the first semi-colon or curly braces */` |
|       ! 0 | 10735 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|       ! 0 | 10736 | `				pGen->pIn++;` |
|       ! 0 | 10737 | `			}` |
|       ! 0 | 10738 | `			return SXRET_OK;` |
|         - | 10739 | `		}` |
|         - | 10740 | `		/* Extract class name */` |
|    353257 | 10741 | `		pName = &pGen->pIn->sData;` |
|         - | 10742 | `		/* Advance the stream cursor */` |
|    353257 | 10743 | `		pGen->pIn++;` |
|         - | 10744 | `		/* Build FQN and obtain a raw class */ {` |
|         - | 10745 | `			SyBlob sFQN;` |
|         - | 10746 | `			SyString sFQNStr;` |
|    353257 | 10747 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    353257 | 10748 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|    353257 | 10749 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    353257 | 10750 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    353257 | 10751 | `			SyBlobRelease(&sFQN);` |
|         - | 10752 | `		}` |
|         - | 10753 | `	}` |
|    353285 | 10754 | `	if( pClass == 0 ){` |
|       ! 0 | 10755 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10756 | `		return SXERR_ABORT;` |
|         - | 10757 | `	}` |
|    353280 | 10758 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|      3853 | 10759 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|         - | 10760 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|      3839 | 10761 | `		pGen->pIn++; /* Jump ':' */` |
|      3834 | 10762 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      3839 | 10763 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|         7 | 10764 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|         7 | 10765 | `			pGen->pIn++;` |
|      3832 | 10766 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      3833 | 10767 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|      3831 | 10768 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|      3831 | 10769 | `			pGen->pIn++;` |
|      1918 | 10770 | `		}else{` |
|         3 | 10771 | `			SyToken *pTok = pGen->pIn;` |
|         3 | 10772 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|         4 | 10773 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|         1 | 10774 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|         3 | 10775 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10776 | `				return SXERR_ABORT;` |
|         - | 10777 | `			}` |
|         3 | 10778 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|         3 | 10779 | `				pGen->pIn++; /* Skip the bogus type token */` |
|         1 | 10780 | `			}` |
|         - | 10781 | `		}` |
|      1917 | 10782 | `	}` |
|    353285 | 10783 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    353285 | 10784 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 10785 | `		return SXERR_ABORT;` |
|         - | 10786 | `	}` |
|         - | 10787 | `	/* implemented interfaces and per-use-statement trait containers */` |
|    353285 | 10788 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|    353285 | 10789 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|         - | 10790 | `	/* Assume a standalone class */` |
|    353285 | 10791 | `	pBase = 0;` |
|    353285 | 10792 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    286955 | 10793 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    286955 | 10794 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|         - | 10795 | `			SyBlob sResolved;` |
|         - | 10796 | `			SyString sBaseName;` |
|         - | 10797 | `			sxu32 nRefLine;` |
|    183637 | 10798 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|         - | 10799 | `				/* php parse-fatals here (enums have no inheritance) */` |
|       ! 0 | 10800 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10801 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|       ! 0 | 10802 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10803 | `					return SXERR_ABORT;` |
|         - | 10804 | `				}` |
|       ! 0 | 10805 | `			}` |
|    183637 | 10806 | `			pGen->pIn++; /* Advance past 'extends' */` |
|    183637 | 10807 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    183637 | 10808 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    183637 | 10809 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         3 | 10810 | `				SyBlobRelease(&sResolved);` |
|         4 | 10811 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 10812 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|         1 | 10813 | `					pName);` |
|         3 | 10814 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|         3 | 10815 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10816 | `					return SXERR_ABORT;` |
|         - | 10817 | `				}` |
|         3 | 10818 | `				return SXRET_OK;` |
|         - | 10819 | `			}` |
|    275450 | 10820 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    183630 | 10821 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    183635 | 10822 | `			SyStringInitFromBuf(&sBaseName,` |
|         - | 10823 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 10824 | `			/* Interfaces are not allowed */` |
|    183635 | 10825 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|       ! 0 | 10826 | `				pBase = pBase->pNextName;` |
|       ! 0 | 10827 | `			}` |
|    183635 | 10828 | `			if( pBase == 0 ){` |
|       ! 0 | 10829 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 10830 | `					"Nonexistent base class '%z'",&sBaseName);` |
|       ! 0 | 10831 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10832 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 10833 | `					return SXERR_ABORT;` |
|         - | 10834 | `				}` |
|       ! 0 | 10835 | `			}else{` |
|    183635 | 10836 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|         4 | 10837 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 | 10838 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|         3 | 10839 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10840 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10841 | `						return SXERR_ABORT;` |
|         - | 10842 | `					}` |
|         3 | 10843 | `					pBase = 0; /* Never inherit from an enum */` |
|    183634 | 10844 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|       ! 0 | 10845 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10846 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|       ! 0 | 10847 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10848 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10849 | `						return SXERR_ABORT;` |
|         - | 10850 | `					}` |
|       ! 0 | 10851 | `				}` |
|         - | 10852 | `			}` |
|    183635 | 10853 | `			SyBlobRelease(&sResolved);` |
|    183635 | 10854 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|       ! 0 | 10855 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|       ! 0 | 10856 | `			}` |
|     91815 | 10857 | `		}` |
|    286953 | 10858 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|         - | 10859 | `			ph7_class *pInterface;` |
|         - | 10860 | `			/* Interface implementation */` |
|    107157 | 10861 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    110890 | 10862 | `			for(;;){` |
|         - | 10863 | `				SyBlob sResolved;` |
|         - | 10864 | `				SyString sIntName;` |
|         - | 10865 | `				sxu32 nRefLine;` |
|    164471 | 10866 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    164471 | 10867 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    164471 | 10868 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 10869 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 10870 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 10871 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|       ! 0 | 10872 | `						pName);` |
|       ! 0 | 10873 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10874 | `						return SXERR_ABORT;` |
|         - | 10875 | `					}` |
|       ! 0 | 10876 | `					break;` |
|         - | 10877 | `				}` |
|    328937 | 10878 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    164466 | 10879 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    164471 | 10880 | `				SyStringInitFromBuf(&sIntName,` |
|         - | 10881 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 10882 | `				/* Only interfaces are allowed */` |
|    164471 | 10883 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 10884 | `					pInterface = pInterface->pNextName;` |
|       ! 0 | 10885 | `				}` |
|    164471 | 10886 | `				if( pInterface == 0 ){` |
|       ! 0 | 10887 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 10888 | `						"Nonexistent base interface '%z'",&sIntName);` |
|       ! 0 | 10889 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10890 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10891 | `						return SXERR_ABORT;` |
|         - | 10892 | `					}` |
|       ! 0 | 10893 | `				}else{` |
|         - | 10894 | `					/* Reject user classes that try to implement Throwable` |
|         - | 10895 | `					 * directly (or via an interface that extends Throwable)` |
|         - | 10896 | `					 * unless they already extend Exception or Error.` |
|         - | 10897 | `					 * Exception and Error themselves are compiled from the` |
|         - | 10898 | `					 * built-in library and are exempt by FQN — a namespaced` |
|         - | 10899 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    164471 | 10900 | `					SyString *pFqn = &pClass->sName;` |
|    164471 | 10901 | `					int bIsExceptionOrError =` |
|     86057 | 10902 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|    248613 | 10903 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    162563 | 10904 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|      3834 | 10905 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|    168291 | 10906 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|     11478 | 10907 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|      3823 | 10908 | `						!bIsExceptionOrError ){` |
|        12 | 10909 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10910 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|         3 | 10911 | `							&pClass->sName);` |
|         9 | 10912 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10913 | `							SyBlobRelease(&sResolved);` |
|       ! 0 | 10914 | `							return SXERR_ABORT;` |
|         - | 10915 | `						}` |
|         - | 10916 | `						/* Skip registration so the follow-up abstract-method` |
|         - | 10917 | `						 * check does not produce a duplicate fatal. */` |
|         6 | 10918 | `					}else{` |
|    164465 | 10919 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|         - | 10920 | `					}` |
|         - | 10921 | `				}` |
|    164471 | 10922 | `				SyBlobRelease(&sResolved);` |
|    164471 | 10923 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     53581 | 10924 | `					break;` |
|         - | 10925 | `				}` |
|     57319 | 10926 | `				pGen->pIn++;/* Jump the comma */` |
|         5 | 10927 | `			}` |
|     53576 | 10928 | `		}` |
|    143474 | 10929 | `	}` |
|    353283 | 10930 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - | 10931 | `		/* Syntax error */` |
|       ! 0 | 10932 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|       ! 0 | 10933 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 10934 | `		if( rc == SXERR_ABORT ){` |
|         - | 10935 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 10936 | `			return SXERR_ABORT;` |
|         - | 10937 | `		}` |
|       ! 0 | 10938 | `		return SXRET_OK;` |
|         - | 10939 | `	}` |
|    353283 | 10940 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    353283 | 10941 | `	pEnd = 0; /* cc warning */` |
|         - | 10942 | `	/* Delimit the class body */` |
|    353283 | 10943 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    353283 | 10944 | `	if( pEnd >= pGen->pEnd ){` |
|         - | 10945 | `		/* Syntax error */` |
|       ! 0 | 10946 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|       ! 0 | 10947 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 10948 | `		if( rc == SXERR_ABORT ){` |
|         - | 10949 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 10950 | `			return SXERR_ABORT;` |
|         - | 10951 | `		}` |
|       ! 0 | 10952 | `		return SXRET_OK;` |
|         - | 10953 | `	}` |
|         - | 10954 | `	/* The delimiter token is the class body's closing brace */` |
|    353283 | 10955 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 10956 | `	/* Swap token stream */` |
|    353283 | 10957 | `	pTmp = pGen->pEnd;` |
|    353283 | 10958 | `	pGen->pEnd = pEnd;` |
|         - | 10959 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|    353283 | 10960 | `	pClass->iFlags \|= iFlags;` |
|         - | 10961 | `	/* Start the parse process */` |
|   1371217 | 10962 | `	for(;;){` |
|         - | 10963 | `		/* Jump leading/trailing semi-colons */` |
|   3905939 | 10964 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    704031 | 10965 | `			pGen->pIn++;` |
|         5 | 10966 | `		}` |
|   3201913 | 10967 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 10968 | `			/* End of class body */` |
|    353241 | 10969 | `			break;` |
|         - | 10970 | `		}` |
|         - | 10971 | `		/* Bind a directly-preceding docblock to this member */` |
|   2848677 | 10972 | `		GenStateSetPendingDoc(&(*pGen));` |
|   2848672 | 10973 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|   1424341 | 10974 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|       ! 0 | 10975 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10976 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 10977 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 10978 | `			if( rc == SXERR_ABORT ){` |
|         - | 10979 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 10980 | `				return SXERR_ABORT;` |
|         - | 10981 | `			}` |
|       ! 0 | 10982 | `			goto done;` |
|         - | 10983 | `		}` |
|         - | 10984 | `		/* Assume public visibility */` |
|   2848677 | 10985 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|   2848677 | 10986 | `		iAttrflags = 0;` |
|         - | 10987 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|         - | 10988 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|         - | 10989 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|         - | 10990 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|   2848677 | 10991 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 10992 | `			int bMod = 0;` |
|       ! 0 | 10993 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 10994 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|         - | 10995 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|         - | 10996 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|         - | 10997 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|         - | 10998 | `			 * that the generic keyword dispatch would misread as a method. */` |
|       ! 0 | 10999 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       ! 0 | 11000 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       ! 0 | 11001 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|       ! 0 | 11002 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|       ! 0 | 11003 | `			}` |
|       ! 0 | 11004 | `			if( !bMod ){` |
|       ! 0 | 11005 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11006 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 11007 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11008 | `						return SXERR_ABORT;` |
|         - | 11009 | `					}` |
|       ! 0 | 11010 | `					goto done;` |
|         - | 11011 | `				}` |
|       ! 0 | 11012 | `				continue;` |
|         - | 11013 | `			}` |
|       ! 0 | 11014 | `		}` |
|   2848677 | 11015 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 11016 | `			/* Extract the current keyword */` |
|   2848677 | 11017 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   2848677 | 11018 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|         - | 11019 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|      7687 | 11020 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|      7687 | 11021 | `				if( rc != SXRET_OK ){` |
|         6 | 11022 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11023 | `						return SXERR_ABORT;` |
|         - | 11024 | `					}` |
|         6 | 11025 | `					goto done;` |
|         - | 11026 | `				}` |
|      7683 | 11027 | `				continue;` |
|         - | 11028 | `			}` |
|   2840995 | 11029 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 11030 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|         - | 11031 | `				TraitUseEntry sUse;` |
|     15349 | 11032 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|     15349 | 11033 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|     15349 | 11034 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      7680 | 11035 | `				for(;;){` |
|         - | 11036 | `					ph7_class *pTrait;` |
|         - | 11037 | `					SyString *pTraitName;` |
|     15357 | 11038 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 11039 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 11040 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|       ! 0 | 11041 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11042 | `							return SXERR_ABORT;` |
|         - | 11043 | `						}` |
|       ! 0 | 11044 | `						break;` |
|         - | 11045 | `					}` |
|     15357 | 11046 | `					pTraitName = &pGen->pIn->sData;` |
|         - | 11047 | `					/* Resolve trait name through namespace/imports */ {` |
|         - | 11048 | `						SyBlob sResolved;` |
|     15357 | 11049 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     15357 | 11050 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     30709 | 11051 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|     15352 | 11052 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     15357 | 11053 | `						SyBlobRelease(&sResolved);` |
|         - | 11054 | `					}` |
|         - | 11055 | `					/* Only traits are allowed */` |
|     15357 | 11056 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 11057 | `						pTrait = pTrait->pNextName;` |
|       ! 0 | 11058 | `					}` |
|     15357 | 11059 | `					if( pTrait == 0 ){` |
|       ! 0 | 11060 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 11061 | `							"'%z' is not a trait",pTraitName);` |
|       ! 0 | 11062 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11063 | `							return SXERR_ABORT;` |
|         - | 11064 | `						}` |
|       ! 0 | 11065 | `					}else{` |
|     15357 | 11066 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|         - | 11067 | `					}` |
|     15357 | 11068 | `					pGen->pIn++; /* Advance past trait name */` |
|     15357 | 11069 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      7677 | 11070 | `						break;` |
|         - | 11071 | `					}` |
|        10 | 11072 | `					pGen->pIn++; /* Jump the comma */` |
|         2 | 11073 | `				}` |
|         - | 11074 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|     15349 | 11075 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - | 11076 | `					SyToken *pBlock;` |
|        13 | 11077 | `					pGen->pIn++; /* Jump '{' */` |
|        13 | 11078 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|        13 | 11079 | `					sUse.pResolvStart = pGen->pIn;` |
|        13 | 11080 | `					sUse.pResolvEnd = pBlock;` |
|        13 | 11081 | `					if( pBlock < pGen->pEnd ){` |
|        13 | 11082 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|         8 | 11083 | `					}else{` |
|       ! 0 | 11084 | `						pGen->pIn = pGen->pEnd;` |
|         - | 11085 | `					}` |
|         5 | 11086 | `				}` |
|     15349 | 11087 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|         - | 11088 | `				/* The semicolon will be consumed by the outer loop */` |
|     15349 | 11089 | `				continue;` |
|         - | 11090 | `			}` |
|   2825651 | 11091 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - | 11092 | `				int nSetTok;` |
|   2580671 | 11093 | `				sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   2580671 | 11094 | `				if( nSetVis ){` |
|         - | 11095 | ``					/* Leading `private(set)`/`protected(set)` with no read`` |
|         - | 11096 | `					 * visibility: the read side defaults to public (php 8.4). */` |
|         3 | 11097 | `					iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 11098 | `					pGen->pIn += nSetTok;` |
|         2 | 11099 | `				}else{` |
|   2580669 | 11100 | `					iProtection = nKwrd;` |
|   2580669 | 11101 | `					pGen->pIn++; /* Jump the visibility token */` |
|         - | 11102 | `					/* Optional asymmetric set-visibility after the read` |
|         - | 11103 | ``					 * visibility: `public private(set) int $x`. */`` |
|   2580669 | 11104 | `					nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   2580669 | 11105 | `					if( nSetVis ){` |
|         9 | 11106 | `						iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         9 | 11107 | `						pGen->pIn += nSetTok;` |
|         4 | 11108 | `					}` |
|         - | 11109 | `				}` |
|         - | 11110 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`,`` |
|         - | 11111 | ``				 * `public private(set) readonly int $x`. */`` |
|   2580671 | 11112 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|        24 | 11113 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|        24 | 11114 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        10 | 11115 | `				}` |
|   2580666 | 11116 | `				if( pGen->pIn >= pGen->pEnd` |
|   2580671 | 11117 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11118 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11119 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 11120 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 11121 | `					if( rc == SXERR_ABORT ){` |
|         - | 11122 | `						/* Error count limit reached,abort immediately */` |
|       ! 0 | 11123 | `						return SXERR_ABORT;` |
|         - | 11124 | `					}` |
|       ! 0 | 11125 | `					goto done;` |
|         - | 11126 | `				}` |
|   2580671 | 11127 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 11128 | `					/* Attribute declaration (untyped) */` |
|    409365 | 11129 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    409365 | 11130 | `					if( rc != SXRET_OK ){` |
|        11 | 11131 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11132 | `							return SXERR_ABORT;` |
|         - | 11133 | `						}` |
|        11 | 11134 | `						goto done;` |
|         - | 11135 | `					}` |
|    409506 | 11136 | `					continue;` |
|         - | 11137 | `				}` |
|   2171311 | 11138 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 11139 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|       309 | 11140 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       309 | 11141 | `					if( rc != SXRET_OK ){` |
|         8 | 11142 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11143 | `							return SXERR_ABORT;` |
|         - | 11144 | `						}` |
|         8 | 11145 | `						goto done;` |
|         - | 11146 | `					}` |
|       303 | 11147 | `					continue;` |
|         - | 11148 | `				}` |
|         - | 11149 | `				/* Extract the keyword */` |
|   2171007 | 11150 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1085501 | 11151 | `			}` |
|   2415987 | 11152 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|         - | 11153 | `				/* Process constant declaration */` |
|    237003 | 11154 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|    237003 | 11155 | `				if( rc != SXRET_OK ){` |
|        11 | 11156 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11157 | `						return SXERR_ABORT;` |
|         - | 11158 | `					}` |
|        11 | 11159 | `					goto done;` |
|         - | 11160 | `				}` |
|    118500 | 11161 | `			}else{` |
|   2178989 | 11162 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - | 11163 | `					/* Static method or attribute,record that */` |
|     95677 | 11164 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     95677 | 11165 | `					pGen->pIn++; /* Jump the static keyword */` |
|     95677 | 11166 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 11167 | `						int nSetTok;` |
|     68901 | 11168 | `						sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|     68901 | 11169 | `						if( nSetVis ){` |
|         - | 11170 | ``							/* `static private(set) int $x` — read side stays public */`` |
|         3 | 11171 | `							iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 11172 | `							pGen->pIn += nSetTok;` |
|         2 | 11173 | `						}else{` |
|         - | 11174 | `							/* Extract the keyword */` |
|     68899 | 11175 | `							nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     68899 | 11176 | `							if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 11177 | `								iProtection = nKwrd;` |
|       ! 0 | 11178 | `								pGen->pIn++; /* Jump the visibility token */` |
|       ! 0 | 11179 | `								nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|       ! 0 | 11180 | `								if( nSetVis ){` |
|       ! 0 | 11181 | `									iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|       ! 0 | 11182 | `									pGen->pIn += nSetTok;` |
|       ! 0 | 11183 | `								}` |
|       ! 0 | 11184 | `							}` |
|         - | 11185 | `						}` |
|     34448 | 11186 | `					}` |
|         - | 11187 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|         - | 11188 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|         - | 11189 | `					 * than a generic "expecting method" parse error. */` |
|     95677 | 11190 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 11191 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 11192 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       ! 0 | 11193 | `					}` |
|     95672 | 11194 | `					if( pGen->pIn >= pGen->pEnd` |
|     95677 | 11195 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11196 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11197 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|       ! 0 | 11198 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11199 | `						if( rc == SXERR_ABORT ){` |
|         - | 11200 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11201 | `							return SXERR_ABORT;` |
|         - | 11202 | `						}` |
|       ! 0 | 11203 | `						goto done;` |
|         - | 11204 | `					}` |
|     95677 | 11205 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 11206 | `						/* Attribute declaration */` |
|     26777 | 11207 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     26777 | 11208 | `						if( rc != SXRET_OK ){` |
|         3 | 11209 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11210 | `								return SXERR_ABORT;` |
|         - | 11211 | `							}` |
|         3 | 11212 | `							goto done;` |
|         - | 11213 | `						}` |
|     26775 | 11214 | `						continue;` |
|         - | 11215 | `					}` |
|     68905 | 11216 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 11217 | `						/* Typed static attribute declaration */` |
|        19 | 11218 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        19 | 11219 | `						if( rc != SXRET_OK ){` |
|         3 | 11220 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11221 | `								return SXERR_ABORT;` |
|         - | 11222 | `							}` |
|         3 | 11223 | `							goto done;` |
|         - | 11224 | `						}` |
|        17 | 11225 | `						continue;` |
|         - | 11226 | `					}` |
|         - | 11227 | `					/* Extract the keyword */` |
|     68889 | 11228 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   2117759 | 11229 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         - | 11230 | `					/* Abstract method,record that */` |
|      7667 | 11231 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         - | 11232 | `					/* Mark the whole class as abstract */` |
|      7667 | 11233 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|         - | 11234 | `					/* Advance the stream cursor */` |
|      7667 | 11235 | `					pGen->pIn++;` |
|      7667 | 11236 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      7667 | 11237 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      7667 | 11238 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      7665 | 11239 | `							iProtection = nKwrd;` |
|      7665 | 11240 | `							pGen->pIn++; /* Jump the visibility token */` |
|      3830 | 11241 | `						}` |
|      3831 | 11242 | `					}` |
|      7667 | 11243 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      7662 | 11244 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 11245 | `							/* Static method */` |
|       ! 0 | 11246 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 11247 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 11248 | `					}` |
|      7667 | 11249 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      7662 | 11250 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|         - | 11251 | ``							/* PHP 8.4: `abstract public [T] $x { get; set; }` — an abstract`` |
|         - | 11252 | `							 * HOOKED property declaration. Route anything that is not a` |
|         - | 11253 | `							 * method through the attribute compiler with the ABSTRACT flag;` |
|         - | 11254 | ``							 * the hook parser accepts the bare `get;`/`set;` forms there`` |
|         - | 11255 | `							 * (and a non-hooked abstract property is ITS error to raise). */` |
|         6 | 11256 | `							if( pGen->pIn < pGen->pEnd` |
|         7 | 11257 | `							 && ((pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID\|PH7_TK_DOLLAR)) != 0` |
|         3 | 11258 | `							  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         7 | 11259 | `								rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         7 | 11260 | `								if( rc != SXRET_OK ){` |
|       ! 0 | 11261 | `									if( rc == SXERR_ABORT ){` |
|       ! 0 | 11262 | `										return SXERR_ABORT;` |
|         - | 11263 | `									}` |
|       ! 0 | 11264 | `									goto done;` |
|         - | 11265 | `								}` |
|         7 | 11266 | `								continue;` |
|         - | 11267 | `							}` |
|       ! 0 | 11268 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11269 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|       ! 0 | 11270 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 11271 | `							if( rc == SXERR_ABORT ){` |
|         - | 11272 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 11273 | `								return SXERR_ABORT;` |
|         - | 11274 | `							}` |
|       ! 0 | 11275 | `							goto done;` |
|         - | 11276 | `					}` |
|      7661 | 11277 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|   2079483 | 11278 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|         - | 11279 | `					/* final method ,record that */` |
|        21 | 11280 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|        21 | 11281 | `					pGen->pIn++; /* Jump the final keyword */` |
|        21 | 11282 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 11283 | `						/* Extract the keyword */` |
|        21 | 11284 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        21 | 11285 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        11 | 11286 | `							iProtection = nKwrd;` |
|        11 | 11287 | `							pGen->pIn++; /* Jump the visibility token */` |
|         4 | 11288 | `						}` |
|         9 | 11289 | `					}` |
|        21 | 11290 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        18 | 11291 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|         - | 11292 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|         - | 11293 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|         - | 11294 | `							 * child class is compiled (PH7_ClassInherit). */` |
|        14 | 11295 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|        14 | 11296 | `							if( rc != SXRET_OK ){` |
|       ! 0 | 11297 | `								if( rc == SXERR_ABORT ){` |
|       ! 0 | 11298 | `									return SXERR_ABORT;` |
|         - | 11299 | `								}` |
|       ! 0 | 11300 | `								goto done;` |
|         - | 11301 | `							}` |
|        14 | 11302 | `							continue;` |
|         - | 11303 | `					}` |
|         9 | 11304 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         6 | 11305 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 11306 | `							/* Static method */` |
|       ! 0 | 11307 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 11308 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 11309 | `					}` |
|         9 | 11310 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         6 | 11311 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 11312 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11313 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|       ! 0 | 11314 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 11315 | `							if( rc == SXERR_ABORT ){` |
|         - | 11316 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 11317 | `								return SXERR_ABORT;` |
|         - | 11318 | `							}` |
|       ! 0 | 11319 | `							goto done;` |
|         - | 11320 | `					}` |
|         9 | 11321 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         3 | 11322 | `				}` |
|   2152183 | 11323 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 11324 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11325 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|       ! 0 | 11326 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11327 | `						if( rc == SXERR_ABORT ){` |
|         - | 11328 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11329 | `							return SXERR_ABORT;` |
|         - | 11330 | `						}` |
|       ! 0 | 11331 | `						goto done;` |
|         - | 11332 | `				}` |
|   2152183 | 11333 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|         7 | 11334 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|         7 | 11335 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|       ! 0 | 11336 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11337 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 11338 | `						if( rc == SXERR_ABORT ){` |
|         - | 11339 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11340 | `							return SXERR_ABORT;` |
|         - | 11341 | `						}` |
|       ! 0 | 11342 | `						goto done;` |
|         - | 11343 | `					}` |
|         - | 11344 | `					/* Attribute declaration */` |
|         7 | 11345 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         4 | 11346 | `				}else{` |
|         - | 11347 | `					/* Process method declaration */` |
|   2152177 | 11348 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 11349 | `				}` |
|   2152183 | 11350 | `				if( rc != SXRET_OK ){` |
|        16 | 11351 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11352 | `						return SXERR_ABORT;` |
|         - | 11353 | `					}` |
|        16 | 11354 | `					goto done;` |
|         - | 11355 | `				}` |
|         - | 11356 | `			}` |
|   1194583 | 11357 | `		}else{` |
|         - | 11358 | `			/* Attribute declaration */` |
|       ! 0 | 11359 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11360 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11361 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11362 | `					return SXERR_ABORT;` |
|         - | 11363 | `				}` |
|       ! 0 | 11364 | `				goto done;` |
|         - | 11365 | `			}` |
|         - | 11366 | `		}` |
|         5 | 11367 | `	}` |
|         - | 11368 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|         - | 11369 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|         - | 11370 | `	 */` |
|         - | 11371 | `	{` |
|         - | 11372 | `		TraitUseEntry *apUse;` |
|         - | 11373 | `		sxu32 nU;` |
|    353241 | 11374 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|    368585 | 11375 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|     15349 | 11376 | `			TraitUseEntry *pUse = &apUse[nU];` |
|     15349 | 11377 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|     15349 | 11378 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|     15349 | 11379 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|         - | 11380 | `			sxu32 nT;` |
|     15349 | 11381 | `			if( !hasResolution ){` |
|         - | 11382 | `				/* No conflict resolution block: use standard trait application */` |
|     30679 | 11383 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|     15345 | 11384 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|     15345 | 11385 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11386 | `						break;` |
|         - | 11387 | `					}` |
|      7675 | 11388 | `				}` |
|      7672 | 11389 | `			}else{` |
|         - | 11390 | `				/* With resolution block: copy attributes, record traits,` |
|         - | 11391 | `				 * then use the block to resolve method conflicts.` |
|         - | 11392 | `				 */` |
|         - | 11393 | `				SyToken *pR;` |
|        25 | 11394 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        15 | 11395 | `					ph7_class *pTR = apTrait[nT];` |
|         - | 11396 | `					ph7_class_attr *pAR;` |
|         - | 11397 | `					SyHashEntry *pER;` |
|         - | 11398 | `					SyString *pNR;` |
|        15 | 11399 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|        21 | 11400 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|       ! 0 | 11401 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|       ! 0 | 11402 | `						pNR = &pAR->sName;` |
|       ! 0 | 11403 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       ! 0 | 11404 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|       ! 0 | 11405 | `						}` |
|       ! 0 | 11406 | `					}` |
|        15 | 11407 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|         9 | 11408 | `				}` |
|         - | 11409 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|        13 | 11410 | `				pR = pUse->pResolvStart;` |
|        27 | 11411 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 11412 | `					SyString sTrait,sMethod;` |
|         - | 11413 | `					ph7_class *pSrcTrait;` |
|         - | 11414 | `					ph7_class_method *pMeth;` |
|         - | 11415 | `					sxi32 nRKwrd;` |
|        41 | 11416 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        27 | 11417 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        17 | 11418 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        17 | 11419 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        17 | 11420 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        17 | 11421 | `					sMethod = pR->sData;` |
|        17 | 11422 | `					pR++;` |
|        17 | 11423 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|         7 | 11424 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|         7 | 11425 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|         7 | 11426 | `							sTrait = sMethod;` |
|         7 | 11427 | `							pR++;` |
|         7 | 11428 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|         7 | 11429 | `							sMethod = pR->sData;` |
|         7 | 11430 | `							pR++;` |
|         3 | 11431 | `						}` |
|         3 | 11432 | `					}` |
|        17 | 11433 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 11434 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 11435 | `						continue;` |
|         - | 11436 | `					}` |
|        17 | 11437 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        17 | 11438 | `					pR++;` |
|        17 | 11439 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|         5 | 11440 | `						pSrcTrait = 0;` |
|         7 | 11441 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         7 | 11442 | `							SyString *pTN = &apTrait[nT]->sName;` |
|        10 | 11443 | `							if( pTN->nByte >= sTrait.nByte &&` |
|         6 | 11444 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         5 | 11445 | `								pSrcTrait = apTrait[nT];` |
|         5 | 11446 | `								break;` |
|         - | 11447 | `							}` |
|         2 | 11448 | `						}` |
|         5 | 11449 | `						if( pSrcTrait ){` |
|         5 | 11450 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         5 | 11451 | `							if( pMeth ){` |
|         5 | 11452 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|         5 | 11453 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|         5 | 11454 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|         2 | 11455 | `								}` |
|         2 | 11456 | `							}` |
|         2 | 11457 | `						}` |
|         2 | 11458 | `					}` |
|        35 | 11459 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 11460 | `				}` |
|         - | 11461 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|        25 | 11462 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         - | 11463 | `					ph7_class_method *pMR;` |
|         - | 11464 | `					SyHashEntry *pER;` |
|         - | 11465 | `					SyString *pNR;` |
|        15 | 11466 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|        41 | 11467 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|        23 | 11468 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|        23 | 11469 | `						pNR = &pMR->sFunc.sName;` |
|        23 | 11470 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|        14 | 11471 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|         6 | 11472 | `						}` |
|         3 | 11473 | `					}` |
|         9 | 11474 | `				}` |
|         - | 11475 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|        13 | 11476 | `				pR = pUse->pResolvStart;` |
|        27 | 11477 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 11478 | `					SyString sTrait,sMethod,sAlias;` |
|         - | 11479 | `					ph7_class *pSrcTrait;` |
|         - | 11480 | `					ph7_class_method *pMeth;` |
|        27 | 11481 | `					int hasQual = 0;` |
|         - | 11482 | `					sxi32 nRKwrd;` |
|        41 | 11483 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        27 | 11484 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        17 | 11485 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        17 | 11486 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        17 | 11487 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|        17 | 11488 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        17 | 11489 | `					sMethod = pR->sData;` |
|        17 | 11490 | `					pR++;` |
|        17 | 11491 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|         7 | 11492 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|         7 | 11493 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|         7 | 11494 | `							sTrait = sMethod;` |
|         7 | 11495 | `							hasQual = 1;` |
|         7 | 11496 | `							pR++;` |
|         7 | 11497 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|         7 | 11498 | `							sMethod = pR->sData;` |
|         7 | 11499 | `							pR++;` |
|         3 | 11500 | `						}` |
|         3 | 11501 | `					}` |
|        17 | 11502 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 11503 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 11504 | `						continue;` |
|         - | 11505 | `					}` |
|        17 | 11506 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        17 | 11507 | `					pR++;` |
|        17 | 11508 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|        13 | 11509 | `						sxi32 iNewVis = -1;` |
|        13 | 11510 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|         7 | 11511 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|         7 | 11512 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|         7 | 11513 | `								iNewVis = nAK;` |
|         7 | 11514 | `								pR++;` |
|         3 | 11515 | `							}` |
|         3 | 11516 | `						}` |
|        13 | 11517 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|        11 | 11518 | `							sAlias = pR->sData;` |
|        11 | 11519 | `							pR++;` |
|         4 | 11520 | `						}` |
|        13 | 11521 | `						pMeth = 0;` |
|        13 | 11522 | `						if( hasQual ){` |
|         3 | 11523 | `							pSrcTrait = 0;` |
|         5 | 11524 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         5 | 11525 | `								SyString *pTN = &apTrait[nT]->sName;` |
|         7 | 11526 | `								if( pTN->nByte >= sTrait.nByte &&` |
|         4 | 11527 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         3 | 11528 | `									pSrcTrait = apTrait[nT];` |
|         3 | 11529 | `									break;` |
|         - | 11530 | `								}` |
|         2 | 11531 | `							}` |
|         3 | 11532 | `							if( pSrcTrait ){` |
|         3 | 11533 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         1 | 11534 | `							}` |
|         2 | 11535 | `						}else{` |
|        10 | 11536 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|         - | 11537 | `						}` |
|        13 | 11538 | `						if( pMeth ){` |
|        13 | 11539 | `							if( sAlias.nByte > 0 ){` |
|         - | 11540 | `								/* Create a shallow copy of the method struct for the alias` |
|         - | 11541 | `								 * so it can carry its own visibility without affecting the original.` |
|         - | 11542 | `								 */` |
|         - | 11543 | `								ph7_class_method *pAlias;` |
|         - | 11544 | `								char *zAliasDup;` |
|        11 | 11545 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        11 | 11546 | `								if( pAlias ){` |
|        11 | 11547 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|        11 | 11548 | `									if( iNewVis >= 0 ){` |
|         5 | 11549 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 11550 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 11551 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         2 | 11552 | `									}` |
|        11 | 11553 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        11 | 11554 | `									if( zAliasDup ){` |
|        11 | 11555 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|         4 | 11556 | `									}` |
|         7 | 11557 | `								}` |
|         7 | 11558 | `							}else if( iNewVis >= 0 ){` |
|         - | 11559 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|         - | 11560 | `								ph7_class_method *pCopy;` |
|         3 | 11561 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|         3 | 11562 | `								if( pCopy ){` |
|         3 | 11563 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|         3 | 11564 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|         3 | 11565 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 11566 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 11567 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         - | 11568 | `									/* Replace the method in the class hash */` |
|         3 | 11569 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|         3 | 11570 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|         1 | 11571 | `								}` |
|         1 | 11572 | `							}` |
|         5 | 11573 | `						}` |
|         5 | 11574 | `						SXUNUSED(hasQual);` |
|         5 | 11575 | `					}` |
|        21 | 11576 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 11577 | `				}` |
|         - | 11578 | `			}` |
|     15349 | 11579 | `			SySetRelease(&pUse->aTraits);` |
|      7677 | 11580 | `		}` |
|         - | 11581 | `	}` |
|    353241 | 11582 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|         - | 11583 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|         - | 11584 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|      3849 | 11585 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|      3849 | 11586 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11587 | `			SySetRelease(&aUseEntries);` |
|       ! 0 | 11588 | `			SySetRelease(&aInterfaces);` |
|       ! 0 | 11589 | `			return SXERR_ABORT;` |
|         - | 11590 | `		}` |
|      1922 | 11591 | `	}` |
|         - | 11592 | `	/* Install the class */` |
|    353241 | 11593 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    353241 | 11594 | `	if( rc == SXRET_OK ){` |
|         - | 11595 | `		ph7_class **apInterface;` |
|         - | 11596 | `		sxu32 n;` |
|    353241 | 11597 | `		if( pBase ){` |
|         - | 11598 | `			/* Inherit from base class and mark as a subclass */` |
|    183633 | 11599 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|     91814 | 11600 | `		}` |
|    353241 | 11601 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|    517701 | 11602 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|         - | 11603 | `			/* Implements one or more interface */` |
|    164465 | 11604 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    164465 | 11605 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11606 | `				break;` |
|         - | 11607 | `			}` |
|     82235 | 11608 | `		}` |
|         - | 11609 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|         - | 11610 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|    353241 | 11611 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|      3849 | 11612 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|      3849 | 11613 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 11614 | `				pIntf = pIntf->pNextName;` |
|       ! 0 | 11615 | `			}` |
|      3849 | 11616 | `			if( pIntf ){` |
|      3849 | 11617 | `				PH7_ClassImplement(pClass,pIntf);` |
|      1922 | 11618 | `			}` |
|      3849 | 11619 | `			if( pClass->nEnumBacking != 0 ){` |
|      3837 | 11620 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|      3837 | 11621 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 11622 | `					pIntf = pIntf->pNextName;` |
|       ! 0 | 11623 | `				}` |
|      3837 | 11624 | `				if( pIntf ){` |
|      3837 | 11625 | `					PH7_ClassImplement(pClass,pIntf);` |
|      1916 | 11626 | `				}` |
|      1916 | 11627 | `			}` |
|      1922 | 11628 | `		}` |
|         - | 11629 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|         - | 11630 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|    353236 | 11631 | `		if( rc == SXRET_OK` |
|    353236 | 11632 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|    353241 | 11633 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|    187291 | 11634 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|         - | 11635 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|    187291 | 11636 | `			if( pStringable ){` |
|    187291 | 11637 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    187291 | 11638 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|         - | 11639 | `				sxu32 i;` |
|    187291 | 11640 | `				int bAlready = 0;` |
|    225495 | 11641 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|     42031 | 11642 | `					if( apImpl[i] == pStringable ){` |
|      3827 | 11643 | `						bAlready = 1;` |
|      3827 | 11644 | `						break;` |
|         - | 11645 | `					}` |
|     19107 | 11646 | `				}` |
|    187291 | 11647 | `				if( !bAlready ){` |
|    183469 | 11648 | `					PH7_ClassImplement(pClass,pStringable);` |
|     91732 | 11649 | `				}` |
|     93643 | 11650 | `			}` |
|     93643 | 11651 | `		}` |
|         - | 11652 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|    353241 | 11653 | `		if( rc == SXRET_OK ){` |
|    353241 | 11654 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|    353241 | 11655 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 11656 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 11657 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 11658 | `				return SXERR_ABORT;` |
|         - | 11659 | `			}` |
|    176618 | 11660 | `		}` |
|         - | 11661 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|    353241 | 11662 | `		if( rc == SXRET_OK ){` |
|    353241 | 11663 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|    353241 | 11664 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 11665 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 11666 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 11667 | `				return SXERR_ABORT;` |
|         - | 11668 | `			}` |
|    176618 | 11669 | `		}` |
|    176618 | 11670 | `	}` |
|    353241 | 11671 | `	SySetRelease(&aUseEntries);` |
|    353241 | 11672 | `	SySetRelease(&aInterfaces);` |
|    353241 | 11673 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11674 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11675 | `		return SXERR_ABORT;` |
|         - | 11676 | `	}` |
|    176618 | 11677 | `done:` |
|         - | 11678 | `	/* Point beyond the class body */` |
|    353283 | 11679 | `	pGen->pIn = &pEnd[1];` |
|    353283 | 11680 | `	pGen->pEnd = pTmp;` |
|    353283 | 11681 | `	return PH7_OK;` |
|    176645 | 11682 | `}` |
|         - | 11683 | `/* Compile a named class declaration (the common case). */` |
|    353252 | 11684 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|         5 | 11685 | `{` |
|    353257 | 11686 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|         5 | 11687 | `}` |
|         - | 11688 | `/*` |
|         - | 11689 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|         - | 11690 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|         - | 11691 | ` * compile + install the class body once (at compile time, like every other` |
|         - | 11692 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|         - | 11693 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|         - | 11694 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|         - | 11695 | ` */` |
|        28 | 11696 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         4 | 11697 | `{` |
|         - | 11698 | `	char zName[128];         /* Synthesized class name */` |
|         - | 11699 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|         - | 11700 | `	SyString sName;` |
|         - | 11701 | `	SyToken *pArgStart,*pArgEnd;` |
|        32 | 11702 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|         - | 11703 | `	                              * is keyed to this 'class' token */` |
|         - | 11704 | `	ph7_value *pObj;` |
|        32 | 11705 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11706 | `	sxu32 nIdx,nLen;` |
|         - | 11707 | `	sxi32 nArg,rc;` |
|        14 | 11708 | `	SXUNUSED(iCompileFlag);` |
|         - | 11709 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|        32 | 11710 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|        32 | 11711 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 | 11712 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|       ! 0 | 11713 | `	}` |
|        32 | 11714 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|         - | 11715 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|         - | 11716 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|         - | 11717 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|        32 | 11718 | `	pArgStart = pArgEnd = 0;` |
|        32 | 11719 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|        32 | 11720 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11721 | `		return rc;` |
|         - | 11722 | `	}` |
|         - | 11723 | `	{` |
|         - | 11724 | ``		/* Expression-position attributes (`new #[A] class {…}`) */`` |
|        32 | 11725 | `		ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0);` |
|        28 | 11726 | `		if( pAnonClass` |
|        32 | 11727 | `		 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 11728 | `			return SXERR_ABORT;` |
|         - | 11729 | `		}` |
|         - | 11730 | `	}` |
|         - | 11731 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|         - | 11732 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|        32 | 11733 | `	nArg = 0;` |
|        32 | 11734 | `	if( pArgStart < pArgEnd ){` |
|         7 | 11735 | `		SyToken *pSavedIn = pGen->pIn;` |
|         7 | 11736 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|         - | 11737 | `		SyToken *pArgNext;` |
|         7 | 11738 | `		pGen->pIn = pArgStart;` |
|         7 | 11739 | `		pGen->pEnd = pArgEnd;` |
|        13 | 11740 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|         7 | 11741 | `			if( pGen->pIn < pArgNext ){` |
|         7 | 11742 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|         7 | 11743 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11744 | `					pGen->pIn = pSavedIn;` |
|       ! 0 | 11745 | `					pGen->pEnd = pSavedEnd;` |
|       ! 0 | 11746 | `					return SXERR_ABORT;` |
|         - | 11747 | `				}` |
|         7 | 11748 | `				nArg++;` |
|         3 | 11749 | `			}` |
|         7 | 11750 | `			pGen->pIn = &pArgNext[1];` |
|         1 | 11751 | `		}` |
|         7 | 11752 | `		pGen->pIn = pSavedIn;` |
|         7 | 11753 | `		pGen->pEnd = pSavedEnd;` |
|         3 | 11754 | `	}` |
|         - | 11755 | `	/* Load the synthesized class name */` |
|        32 | 11756 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        32 | 11757 | `	if( pObj == 0 ){` |
|       ! 0 | 11758 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 11759 | `		return SXERR_ABORT;` |
|         - | 11760 | `	}` |
|        32 | 11761 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|        32 | 11762 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - | 11763 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|        32 | 11764 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        32 | 11765 | `	return SXRET_OK;` |
|        18 | 11766 | `}` |
|         - | 11767 | `/*` |
|         - | 11768 | ` * Compile a user-defined abstract class.` |
|         - | 11769 | ` *  According to the PHP language reference manual` |
|         - | 11770 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|         - | 11771 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|         - | 11772 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|         - | 11773 | ` *   the method's signature - they cannot define the implementation.` |
|         - | 11774 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|         - | 11775 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|         - | 11776 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|         - | 11777 | ` *   method is defined as protected, the function implementation must be defined as either` |
|         - | 11778 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|         - | 11779 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|         - | 11780 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|         - | 11781 | ` *   could differ.` |
|         - | 11782 | ` */` |
|         - | 11783 | `/*` |
|         - | 11784 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|         - | 11785 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|         - | 11786 | ` * receives the corresponding PH7_CLASS_* bit.` |
|         - | 11787 | ` */` |
|  12860264 | 11788 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|         5 | 11789 | `{` |
|  12860269 | 11790 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|   7535799 | 11791 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|   7535799 | 11792 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|   7489925 | 11793 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|   3729629 | 11794 | `	}` |
|  12783733 | 11795 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  12783673 | 11796 | `	return FALSE;` |
|   6430137 | 11797 | `}` |
|         - | 11798 | `/*` |
|         - | 11799 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|         - | 11800 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|         - | 11801 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|         - | 11802 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|         - | 11803 | ` */` |
|  12783668 | 11804 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|         5 | 11805 | `{` |
|  12783673 | 11806 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  12783673 | 11807 | `	sxi32 iFlags = 0,iFlag;` |
|  12860269 | 11808 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|     76601 | 11809 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|         5 | 11810 | `			pDup = pIn;` |
|         2 | 11811 | `		}` |
|     76601 | 11812 | `		iFlags \|= iFlag;` |
|     76601 | 11813 | `		pIn++;` |
|         5 | 11814 | `	}` |
|  12783673 | 11815 | `	*ppIn = pIn;` |
|  12783673 | 11816 | `	if( ppDup ){ *ppDup = pDup; }` |
|  12783673 | 11817 | `	return iFlags;` |
|         5 | 11818 | `}` |
|         - | 11819 | `/*` |
|         - | 11820 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|         - | 11821 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|         - | 11822 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|         - | 11823 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|         - | 11824 | `` * `readonly`) to their existing handlers.`` |
|         - | 11825 | ` */` |
|  12749200 | 11826 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|         5 | 11827 | `{` |
|  12749205 | 11828 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|   6416715 | 11829 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  12770256 | 11830 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|         5 | 11831 | `}` |
|         - | 11832 | `/*` |
|         - | 11833 | ` * Compile a class declaration carrying one or more leading modifiers` |
|         - | 11834 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|         - | 11835 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|         - | 11836 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|         - | 11837 | `` * `abstract`+`final` pair, like PHP.`` |
|         - | 11838 | ` */` |
|     34468 | 11839 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|         5 | 11840 | `{` |
|         - | 11841 | `	SyToken *pDup;` |
|     34473 | 11842 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|         - | 11843 | `	sxi32 rc;` |
|     34473 | 11844 | `	if( pDup ){` |
|         4 | 11845 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|         2 | 11846 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|         3 | 11847 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11848 | `			return SXERR_ABORT;` |
|         - | 11849 | `		}` |
|         1 | 11850 | `	}` |
|     34468 | 11851 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|     17239 | 11852 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|         3 | 11853 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11854 | `			"Cannot use the final modifier on an abstract class");` |
|         3 | 11855 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11856 | `			return SXERR_ABORT;` |
|         - | 11857 | `		}` |
|         1 | 11858 | `	}` |
|     34473 | 11859 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|     17239 | 11860 | `}` |
|         - | 11861 | `/*` |
|         - | 11862 | ` * Compile a user-defined trait.` |
|         - | 11863 | ` *  Traits are similar to classes, but only intended to group functionality` |
|         - | 11864 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|         - | 11865 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|         - | 11866 | ` */` |
|      7718 | 11867 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|         5 | 11868 | `{` |
|      7723 | 11869 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11870 | `	ph7_class *pClass;` |
|         - | 11871 | `	SyToken *pEnd,*pTmp;` |
|         - | 11872 | `	sxi32 iProtection;` |
|         - | 11873 | `	sxi32 iAttrflags;` |
|         - | 11874 | `	SyString *pName;` |
|         - | 11875 | `	sxi32 nKwrd;` |
|         - | 11876 | `	sxi32 rc;` |
|         - | 11877 | `	/* Jump the 'trait' keyword */` |
|      7723 | 11878 | `	pGen->pIn++;` |
|      7723 | 11879 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 11880 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|       ! 0 | 11881 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11882 | `			return SXERR_ABORT;` |
|         - | 11883 | `		}` |
|       ! 0 | 11884 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|       ! 0 | 11885 | `			pGen->pIn++;` |
|       ! 0 | 11886 | `		}` |
|       ! 0 | 11887 | `		return SXRET_OK;` |
|         - | 11888 | `	}` |
|         - | 11889 | `	/* Extract trait name */` |
|      7723 | 11890 | `	pName = &pGen->pIn->sData;` |
|      7723 | 11891 | `	pGen->pIn++;` |
|         - | 11892 | `	/* Build FQN and obtain a raw class */ {` |
|         - | 11893 | `		SyBlob sFQN;` |
|         - | 11894 | `		SyString sFQNStr;` |
|      7723 | 11895 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7723 | 11896 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      7723 | 11897 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      7723 | 11898 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      7723 | 11899 | `		SyBlobRelease(&sFQN);` |
|         - | 11900 | `	}` |
|      7723 | 11901 | `	if( pClass == 0 ){` |
|       ! 0 | 11902 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11903 | `		return SXERR_ABORT;` |
|         - | 11904 | `	}` |
|      7723 | 11905 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|      7723 | 11906 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 11907 | `		return SXERR_ABORT;` |
|         - | 11908 | `	}` |
|         - | 11909 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      7723 | 11910 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|       ! 0 | 11911 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|       ! 0 | 11912 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 11913 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11914 | `			return SXERR_ABORT;` |
|         - | 11915 | `		}` |
|       ! 0 | 11916 | `		return SXRET_OK;` |
|         - | 11917 | `	}` |
|      7723 | 11918 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      7723 | 11919 | `	pEnd = 0;` |
|      7723 | 11920 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      7723 | 11921 | `	if( pEnd >= pGen->pEnd ){` |
|       ! 0 | 11922 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|       ! 0 | 11923 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 11924 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11925 | `			return SXERR_ABORT;` |
|         - | 11926 | `		}` |
|       ! 0 | 11927 | `		return SXRET_OK;` |
|         - | 11928 | `	}` |
|         - | 11929 | `	/* The delimiter token is the trait body's closing brace */` |
|      7723 | 11930 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 11931 | `	/* Swap token stream */` |
|      7723 | 11932 | `	pTmp = pGen->pEnd;` |
|      7723 | 11933 | `	pGen->pEnd = pEnd;` |
|         - | 11934 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|      7723 | 11935 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|         - | 11936 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|     55469 | 11937 | `	for(;;){` |
|    156831 | 11938 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     22951 | 11939 | `			pGen->pIn++;` |
|         5 | 11940 | `		}` |
|    133885 | 11941 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      7723 | 11942 | `			break;` |
|         - | 11943 | `		}` |
|         - | 11944 | `		/* Bind a directly-preceding docblock to this member */` |
|    126167 | 11945 | `		GenStateSetPendingDoc(&(*pGen));` |
|    126167 | 11946 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|       ! 0 | 11947 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11948 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 11949 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 11950 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 11951 | `				return SXERR_ABORT;` |
|         - | 11952 | `			}` |
|       ! 0 | 11953 | `			goto done;` |
|         - | 11954 | `		}` |
|    126167 | 11955 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|    126167 | 11956 | `		iAttrflags = 0;` |
|    126167 | 11957 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|    126167 | 11958 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    126167 | 11959 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 11960 | `				/* Trait uses another trait: use OtherTrait; */` |
|         5 | 11961 | `				pGen->pIn++; /* Jump 'use' */` |
|         2 | 11962 | `				for(;;){` |
|         - | 11963 | `					ph7_class *pUsedTrait;` |
|         - | 11964 | `					SyString *pUsedName;` |
|         5 | 11965 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 11966 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 11967 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|       ! 0 | 11968 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11969 | `							return SXERR_ABORT;` |
|         - | 11970 | `						}` |
|       ! 0 | 11971 | `						break;` |
|         - | 11972 | `					}` |
|         5 | 11973 | `					pUsedName = &pGen->pIn->sData;` |
|         - | 11974 | `					{` |
|         - | 11975 | `						SyBlob sResolved;` |
|         5 | 11976 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|         5 | 11977 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|         7 | 11978 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|         4 | 11979 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|         5 | 11980 | `						SyBlobRelease(&sResolved);` |
|         - | 11981 | `					}` |
|         5 | 11982 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 11983 | `						pUsedTrait = pUsedTrait->pNextName;` |
|       ! 0 | 11984 | `					}` |
|         5 | 11985 | `					if( pUsedTrait == 0 ){` |
|         4 | 11986 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         1 | 11987 | `							"'%z' is not a trait",pUsedName);` |
|         3 | 11988 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11989 | `							return SXERR_ABORT;` |
|         - | 11990 | `						}` |
|         2 | 11991 | `					}else{` |
|         3 | 11992 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|         - | 11993 | `					}` |
|         5 | 11994 | `					pGen->pIn++;` |
|         5 | 11995 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|         3 | 11996 | `						break;` |
|         - | 11997 | `					}` |
|       ! 0 | 11998 | `					pGen->pIn++;` |
|       ! 0 | 11999 | `				}` |
|         5 | 12000 | `				continue;` |
|         - | 12001 | `			}` |
|    126163 | 12002 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|    126147 | 12003 | `				iProtection = nKwrd;` |
|    126147 | 12004 | `				pGen->pIn++;` |
|    126142 | 12005 | `				if( pGen->pIn >= pGen->pEnd` |
|    126147 | 12006 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 12007 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12008 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 12009 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 12010 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12011 | `						return SXERR_ABORT;` |
|         - | 12012 | `					}` |
|       ! 0 | 12013 | `					goto done;` |
|         - | 12014 | `				}` |
|    126147 | 12015 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     22937 | 12016 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     22937 | 12017 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 12018 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12019 | `							return SXERR_ABORT;` |
|         - | 12020 | `						}` |
|       ! 0 | 12021 | `						goto done;` |
|         - | 12022 | `					}` |
|     22937 | 12023 | `					continue;` |
|         - | 12024 | `				}` |
|    103215 | 12025 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         5 | 12026 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         5 | 12027 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 12028 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12029 | `							return SXERR_ABORT;` |
|         - | 12030 | `						}` |
|       ! 0 | 12031 | `						goto done;` |
|         - | 12032 | `					}` |
|         5 | 12033 | `					continue;` |
|         - | 12034 | `				}` |
|    103211 | 12035 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     51603 | 12036 | `			}` |
|    103227 | 12037 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       ! 0 | 12038 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12039 | `					"Traits cannot have constants");` |
|       ! 0 | 12040 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12041 | `					return SXERR_ABORT;` |
|         - | 12042 | `				}` |
|       ! 0 | 12043 | `				goto done;` |
|       ! 0 | 12044 | `			}else{` |
|    103227 | 12045 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|      7655 | 12046 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      7655 | 12047 | `					pGen->pIn++;` |
|      7655 | 12048 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      7653 | 12049 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      7653 | 12050 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 12051 | `							iProtection = nKwrd;` |
|       ! 0 | 12052 | `							pGen->pIn++;` |
|       ! 0 | 12053 | `						}` |
|      3824 | 12054 | `					}` |
|      7650 | 12055 | `					if( pGen->pIn >= pGen->pEnd` |
|      7655 | 12056 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 12057 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12058 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|       ! 0 | 12059 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 12060 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12061 | `							return SXERR_ABORT;` |
|         - | 12062 | `						}` |
|       ! 0 | 12063 | `						goto done;` |
|         - | 12064 | `					}` |
|      7655 | 12065 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         3 | 12066 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         3 | 12067 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 12068 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 12069 | `								return SXERR_ABORT;` |
|         - | 12070 | `							}` |
|       ! 0 | 12071 | `							goto done;` |
|         - | 12072 | `						}` |
|         3 | 12073 | `						continue;` |
|         - | 12074 | `					}` |
|      7653 | 12075 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       ! 0 | 12076 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 12077 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 12078 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 12079 | `								return SXERR_ABORT;` |
|         - | 12080 | `							}` |
|       ! 0 | 12081 | `							goto done;` |
|         - | 12082 | `						}` |
|       ! 0 | 12083 | `						continue;` |
|         - | 12084 | `					}` |
|      7653 | 12085 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     99401 | 12086 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         6 | 12087 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         6 | 12088 | `					pGen->pIn++;` |
|         6 | 12089 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         6 | 12090 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         6 | 12091 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         6 | 12092 | `							iProtection = nKwrd;` |
|         6 | 12093 | `							pGen->pIn++;` |
|         2 | 12094 | `						}` |
|         2 | 12095 | `					}` |
|         6 | 12096 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         4 | 12097 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 12098 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12099 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|       ! 0 | 12100 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 12101 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12102 | `							return SXERR_ABORT;` |
|         - | 12103 | `						}` |
|       ! 0 | 12104 | `						goto done;` |
|         - | 12105 | `					}` |
|         6 | 12106 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         2 | 12107 | `				}` |
|    103225 | 12108 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 12109 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12110 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|       ! 0 | 12111 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 12112 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12113 | `						return SXERR_ABORT;` |
|         - | 12114 | `					}` |
|       ! 0 | 12115 | `					goto done;` |
|         - | 12116 | `				}` |
|    103225 | 12117 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       ! 0 | 12118 | `					pGen->pIn++;` |
|       ! 0 | 12119 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 | 12120 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12121 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 12122 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12123 | `							return SXERR_ABORT;` |
|         - | 12124 | `						}` |
|       ! 0 | 12125 | `						goto done;` |
|         - | 12126 | `					}` |
|       ! 0 | 12127 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 12128 | `				}else{` |
|    103225 | 12129 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 12130 | `				}` |
|    103225 | 12131 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 12132 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12133 | `						return SXERR_ABORT;` |
|         - | 12134 | `					}` |
|       ! 0 | 12135 | `					goto done;` |
|         - | 12136 | `				}` |
|         - | 12137 | `			}` |
|     51615 | 12138 | `		}else{` |
|       ! 0 | 12139 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 12140 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 12141 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12142 | `					return SXERR_ABORT;` |
|         - | 12143 | `				}` |
|       ! 0 | 12144 | `				goto done;` |
|         - | 12145 | `			}` |
|         - | 12146 | `		}` |
|         5 | 12147 | `	}` |
|         - | 12148 | `	/* Install the trait */` |
|      7723 | 12149 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      7723 | 12150 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12151 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 12152 | `		return SXERR_ABORT;` |
|         - | 12153 | `	}` |
|      3859 | 12154 | `done:` |
|         - | 12155 | `	/* Point beyond the trait body */` |
|      7723 | 12156 | `	pGen->pIn = &pEnd[1];` |
|      7723 | 12157 | `	pGen->pEnd = pTmp;` |
|      7723 | 12158 | `	return PH7_OK;` |
|      3864 | 12159 | `}` |
|         - | 12160 | `/*` |
|         - | 12161 | ` * Compile a user-defined class.` |
|         - | 12162 | ` *  According to the PHP language reference manual` |
|         - | 12163 | ` *   Basic class definitions begin with the keyword class, followed` |
|         - | 12164 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|         - | 12165 | ` *   the definitions of the properties and methods belonging to the class.` |
|         - | 12166 | ` *   A class may contain its own constants, variables (called "properties")` |
|         - | 12167 | ` *   and functions (called "methods").` |
|         - | 12168 | ` */` |
|    314936 | 12169 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|         5 | 12170 | `{` |
|         - | 12171 | `	sxi32 rc;` |
|    314941 | 12172 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|    314941 | 12173 | `	return rc;` |
|         5 | 12174 | `}` |
|         - | 12175 | `/*` |
|         - | 12176 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|         - | 12177 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|         - | 12178 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|         - | 12179 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|         - | 12180 | `` * meaning; `enum Name` can never start a valid expression.`` |
|         - | 12181 | ` */` |
|  12707092 | 12182 | `static int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|         5 | 12183 | `{` |
|  12912128 | 12184 | `	return (pIn->nType & PH7_TK_ID)` |
|   6558577 | 12185 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|    214714 | 12186 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
|  12912123 | 12187 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|         5 | 12188 | `}` |
|         - | 12189 | `/*` |
|         - | 12190 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|         - | 12191 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|         - | 12192 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|         - | 12193 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|         - | 12194 | ` */` |
|      3848 | 12195 | `static sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|         5 | 12196 | `{` |
|      3853 | 12197 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|         5 | 12198 | `}` |
|         - | 12199 | `/*` |
|         - | 12200 | ` * Exception handling.` |
|         - | 12201 | ` *  According to the PHP language reference manual` |
|         - | 12202 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|         - | 12203 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|         - | 12204 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|         - | 12205 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|         - | 12206 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|         - | 12207 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|         - | 12208 | ` *    (or re-thrown) within a catch block.` |
|         - | 12209 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|         - | 12210 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|         - | 12211 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|         - | 12212 | ` *    been defined with set_exception_handler().` |
|         - | 12213 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|         - | 12214 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|         - | 12215 | ` */` |
|         - | 12216 | `/*` |
|         - | 12217 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|         - | 12218 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|         - | 12219 | ` * indicates failure.` |
|         - | 12220 | ` */` |
|    508482 | 12221 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 | 12222 | `{` |
|    508487 | 12223 | `	sxi32 rc = SXRET_OK;` |
|    508487 | 12224 | `	if( pRoot->pOp ){` |
|    508475 | 12225 | `		switch( pRoot->pOp->iOp ){` |
|    254235 | 12226 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|         - | 12227 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|         - | 12228 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|         - | 12229 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|         - | 12230 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|         - | 12231 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|    508475 | 12232 | `			break;` |
|       ! 0 | 12233 | `		default:` |
|         - | 12234 | `			/* Runtime will still reject non-Throwable values; the set above` |
|         - | 12235 | `			 * covers the common shapes and gives a friendlier compile error` |
|         - | 12236 | ``			 * for obvious mistakes like `throw 5`. */`` |
|       ! 0 | 12237 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - | 12238 | `				"throw: Expecting an exception class instance");` |
|       ! 0 | 12239 | `			if( rc != SXERR_ABORT ){` |
|       ! 0 | 12240 | `				rc = SXERR_INVALID;` |
|       ! 0 | 12241 | `			}` |
|       ! 0 | 12242 | `			break;` |
|         - | 12243 | `		}` |
|    254252 | 12244 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - | 12245 | `		/* Unexpected expression */` |
|       ! 0 | 12246 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - | 12247 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12248 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 | 12249 | `			rc = SXERR_INVALID;` |
|       ! 0 | 12250 | `		}` |
|       ! 0 | 12251 | `	}` |
|    508487 | 12252 | `	return rc;` |
|         5 | 12253 | `}` |
|         - | 12254 | `/*` |
|         - | 12255 | ` * Compile a 'throw' statement.` |
|         - | 12256 | ` * throw: This is how you trigger an exception.` |
|         - | 12257 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|         - | 12258 | ` */` |
|    508446 | 12259 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|         5 | 12260 | `{` |
|    508451 | 12261 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12262 | `	GenBlock *pBlock;` |
|         - | 12263 | `	sxu32 nIdx;` |
|         - | 12264 | `	sxi32 rc;` |
|    508451 | 12265 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|         - | 12266 | `	/* Compile the expression */` |
|    508451 | 12267 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    508451 | 12268 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 | 12269 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|       ! 0 | 12270 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12271 | `			return SXERR_ABORT;` |
|         - | 12272 | `		}` |
|       ! 0 | 12273 | `		return SXRET_OK;` |
|         - | 12274 | `	}` |
|    508451 | 12275 | `	pBlock = pGen->pCurrent;` |
|         - | 12276 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   2025397 | 12277 | `	while(pBlock->pParent){` |
|   2025393 | 12278 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    508447 | 12279 | `			break;` |
|         - | 12280 | `		}` |
|         - | 12281 | `		/* Point to the parent block */` |
|   1516951 | 12282 | `		pBlock = pBlock->pParent;` |
|         5 | 12283 | `	}` |
|         - | 12284 | `	/* Emit the throw instruction */` |
|    508451 | 12285 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|         - | 12286 | `	/* Emit the jump */` |
|    508451 | 12287 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    508451 | 12288 | `	return SXRET_OK;` |
|    254228 | 12289 | `}` |
|         - | 12290 | `/*` |
|         - | 12291 | ` * Compile a PHP 8.0 'throw' expression.` |
|         - | 12292 | ` * Called from the expression code generator when a 'throw' keyword is` |
|         - | 12293 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|         - | 12294 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|         - | 12295 | ` * the validator guarantees the operand is a valid exception target.` |
|         - | 12296 | ` */` |
|        36 | 12297 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|         2 | 12298 | `{` |
|        38 | 12299 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12300 | `	GenBlock *pBlock;` |
|         - | 12301 | `	sxu32 nIdx;` |
|         - | 12302 | `	sxi32 rc;` |
|        18 | 12303 | `	(void)iCompileFlag;` |
|        38 | 12304 | `	pGen->pIn++; /* Skip 'throw' */` |
|        38 | 12305 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 | 12306 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12307 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12308 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12309 | `			return SXERR_ABORT;` |
|         - | 12310 | `		}` |
|       ! 0 | 12311 | `		return SXRET_OK;` |
|         - | 12312 | `	}` |
|        38 | 12313 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|        38 | 12314 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12315 | `		return SXERR_ABORT;` |
|         - | 12316 | `	}` |
|        38 | 12317 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 | 12318 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12319 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12320 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12321 | `			return SXERR_ABORT;` |
|         - | 12322 | `		}` |
|       ! 0 | 12323 | `		return SXRET_OK;` |
|         - | 12324 | `	}` |
|         - | 12325 | `	/* Walk up to nearest exception/function block for the jump target */` |
|        38 | 12326 | `	pBlock = pGen->pCurrent;` |
|        60 | 12327 | `	while( pBlock->pParent ){` |
|        49 | 12328 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|        27 | 12329 | `			break;` |
|         - | 12330 | `		}` |
|        23 | 12331 | `		pBlock = pBlock->pParent;` |
|         1 | 12332 | `	}` |
|        38 | 12333 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        38 | 12334 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|        38 | 12335 | `	return SXRET_OK;` |
|        20 | 12336 | `}` |
|         - | 12337 | `/*` |
|         - | 12338 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|         - | 12339 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|         - | 12340 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|         - | 12341 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|         - | 12342 | ` * compile error propagated from the parser.` |
|         - | 12343 | ` */` |
|        56 | 12344 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|         5 | 12345 | `{` |
|         - | 12346 | `	SyString sClassName;` |
|         - | 12347 | `	SyToken *pToken;` |
|         - | 12348 | `	SyString *pName;` |
|         - | 12349 | `	char *zDup;` |
|         - | 12350 | `	sxi32 rc;` |
|        61 | 12351 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        61 | 12352 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|        61 | 12353 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|        61 | 12354 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        61 | 12355 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 | 12356 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12357 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12358 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12359 | `		return SXERR_INVALID;` |
|         - | 12360 | `	}` |
|        61 | 12361 | `	pGen->pIn++; /* '(' */` |
|        28 | 12362 | `	for(;;){` |
|         - | 12363 | `		SyBlob sResolved;` |
|        61 | 12364 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|        61 | 12365 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 12366 | `			SyBlobRelease(&sResolved);` |
|       ! 0 | 12367 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12368 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12369 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12370 | `			return SXERR_INVALID;` |
|         - | 12371 | `		}` |
|        89 | 12372 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        56 | 12373 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        61 | 12374 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|        61 | 12375 | `		SyBlobRelease(&sResolved);` |
|        61 | 12376 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|        61 | 12377 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|        61 | 12378 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        56 | 12379 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|         5 | 12380 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       ! 0 | 12381 | `			pGen->pIn++; continue;` |
|         - | 12382 | `		}` |
|        61 | 12383 | `		break;` |
|       ! 0 | 12384 | `	}` |
|         - | 12385 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|         - | 12386 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding. */` |
|        61 | 12387 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|         3 | 12388 | `		pGen->pIn++; /* ')' */` |
|         3 | 12389 | `		return SXRET_OK;` |
|         - | 12390 | `	}` |
|        54 | 12391 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|        59 | 12392 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 12393 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12394 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12395 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12396 | `		return SXERR_INVALID;` |
|         - | 12397 | `	}` |
|        59 | 12398 | `	pGen->pIn++; /* '$' */` |
|        59 | 12399 | `	pName = &pGen->pIn->sData;` |
|        59 | 12400 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|        59 | 12401 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|        59 | 12402 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|        59 | 12403 | `	pGen->pIn++;` |
|        59 | 12404 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 | 12405 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12406 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12407 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12408 | `		return SXERR_INVALID;` |
|         - | 12409 | `	}` |
|        59 | 12410 | `	pGen->pIn++; /* ')' */` |
|        59 | 12411 | `	return SXRET_OK;` |
|        33 | 12412 | `}` |
|         - | 12413 | `/*` |
|         - | 12414 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|         - | 12415 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|         - | 12416 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|         - | 12417 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|         - | 12418 | ` * VmThrowException):` |
|         - | 12419 | ` *` |
|         - | 12420 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|         - | 12421 | ` *    <try body>` |
|         - | 12422 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|         - | 12423 | ` *    JMP  -> finally\|end` |
|         - | 12424 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|         - | 12425 | ` *    <catch body>` |
|         - | 12426 | ` *    JMP  -> finally\|end` |
|         - | 12427 | ` *    ... more catches ...` |
|         - | 12428 | ` *  Lfin: <finally body>` |
|         - | 12429 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|         - | 12430 | ` *  Lend:` |
|         - | 12431 | ` */` |
|       100 | 12432 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|         5 | 12433 | `{` |
|       105 | 12434 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12435 | `	GenBlock *pTry;` |
|         - | 12436 | `	VmInstr *pInstr;` |
|       105 | 12437 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|         - | 12438 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|         - | 12439 | `	sxi32 rc;` |
|       105 | 12440 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|         - | 12441 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|       105 | 12442 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|       105 | 12443 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       105 | 12444 | `	pTry->pUserData = pException;` |
|       105 | 12445 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|       105 | 12446 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       105 | 12447 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       105 | 12448 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       105 | 12449 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       105 | 12450 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|         - | 12451 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|       105 | 12452 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|       105 | 12453 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|       105 | 12454 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       105 | 12455 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12456 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|       105 | 12457 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|         - | 12458 | `	/* Catch clauses (inline) */` |
|       105 | 12459 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       100 | 12460 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        61 | 12461 | `		sxu32 k = 0;` |
|        84 | 12462 | `		for(;;){` |
|         - | 12463 | `			ph7_exception_block sCatch;` |
|         - | 12464 | `			GenBlock *pCatchBlk;` |
|       117 | 12465 | `			sxu32 idxJmp = 0;` |
|       112 | 12466 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|       107 | 12467 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|        33 | 12468 | `				break;` |
|         - | 12469 | `			}` |
|        61 | 12470 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|        61 | 12471 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        61 | 12472 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|        61 | 12473 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|        61 | 12474 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|        61 | 12475 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|        61 | 12476 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|         - | 12477 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|         - | 12478 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|         - | 12479 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|        61 | 12480 | `			pCatchBlk->pUserData = pException;` |
|        61 | 12481 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|        61 | 12482 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        61 | 12483 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|        61 | 12484 | `			GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12485 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|         - | 12486 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|        61 | 12487 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        61 | 12488 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|        61 | 12489 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|        61 | 12490 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|        61 | 12491 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        61 | 12492 | `			k++;` |
|         5 | 12493 | `		}` |
|        28 | 12494 | `	}` |
|         - | 12495 | `	/* Finally (inline) */` |
|       105 | 12496 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        80 | 12497 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|         - | 12498 | `		GenBlock *pFinBlk;` |
|        52 | 12499 | `		pGen->pIn++; /* Jump 'finally' */` |
|        52 | 12500 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|        52 | 12501 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|        52 | 12502 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        52 | 12503 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|        52 | 12504 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        52 | 12505 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|        52 | 12506 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        52 | 12507 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|        52 | 12508 | `		pException->iHasFinally = 1;` |
|        24 | 12509 | `	}` |
|       105 | 12510 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|       105 | 12511 | `	pException->iInlined = 1;` |
|         - | 12512 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|         - | 12513 | `	{` |
|       105 | 12514 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|         - | 12515 | `		sxu32 *aJ; sxu32 n;` |
|       105 | 12516 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|       105 | 12517 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       105 | 12518 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|       161 | 12519 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|        61 | 12520 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|        61 | 12521 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|        33 | 12522 | `		}` |
|         - | 12523 | `	}` |
|       105 | 12524 | `	SySetRelease(&aCatchJmp);` |
|       105 | 12525 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       ! 0 | 12526 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|       ! 0 | 12527 | `	}` |
|       105 | 12528 | `	return SXRET_OK;` |
|        55 | 12529 | `}` |
|         - | 12530 | `/*` |
|         - | 12531 | ` * Compile a 'catch' block.` |
|         - | 12532 | ` * Catch: A "catch" block retrieves an exception and creates` |
|         - | 12533 | ` * an object containing the exception information.` |
|         - | 12534 | ` */` |
|     24444 | 12535 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|         5 | 12536 | `{` |
|     24449 | 12537 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12538 | `	ph7_exception_block sCatch;` |
|         - | 12539 | `	SySet *pInstrContainer;` |
|         - | 12540 | `	SyString sClassName;` |
|         - | 12541 | `	GenBlock *pCatch;` |
|         - | 12542 | `	SyToken *pToken;` |
|         - | 12543 | `	SyString *pName;` |
|         - | 12544 | `	char *zDup;` |
|         - | 12545 | `	sxi32 rc;` |
|     24449 | 12546 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|         - | 12547 | `	/* Zero the structure */` |
|     24449 | 12548 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|         - | 12549 | `	/* Initialize fields */` |
|     24449 | 12550 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     24449 | 12551 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     24449 | 12552 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|         - | 12553 | `			/* Unexpected token,break immediately */` |
|       ! 0 | 12554 | `			pToken = pGen->pIn;` |
|       ! 0 | 12555 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12556 | `				pToken--;` |
|       ! 0 | 12557 | `			}` |
|       ! 0 | 12558 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12559 | `				"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12560 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12561 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12562 | `				return SXERR_ABORT;` |
|         - | 12563 | `			}` |
|       ! 0 | 12564 | `			return SXERR_INVALID;` |
|         - | 12565 | `	}` |
|         - | 12566 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     24449 | 12567 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     12237 | 12568 | `	for(;;){` |
|         - | 12569 | `		SyBlob sResolved;` |
|     24479 | 12570 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     24479 | 12571 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         6 | 12572 | `			SyBlobRelease(&sResolved);` |
|         6 | 12573 | `			pToken = pGen->pIn;` |
|         6 | 12574 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12575 | `				pToken--;` |
|       ! 0 | 12576 | `			}` |
|         8 | 12577 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12578 | `				"syntax error, unexpected %s \"%z\"",` |
|         2 | 12579 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|         6 | 12580 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12581 | `				return SXERR_ABORT;` |
|         - | 12582 | `			}` |
|         6 | 12583 | `			return SXERR_INVALID;` |
|         - | 12584 | `		}` |
|         - | 12585 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|         - | 12586 | `		 * transient SyBlob allocation. */` |
|     36710 | 12587 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     24470 | 12588 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     24475 | 12589 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     24475 | 12590 | `		SyBlobRelease(&sResolved);` |
|     24475 | 12591 | `		if( zDup == 0 ){` |
|       ! 0 | 12592 | `			goto Mem;` |
|         - | 12593 | `		}` |
|     24475 | 12594 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     24475 | 12595 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 12596 | `			goto Mem;` |
|         - | 12597 | `		}` |
|         - | 12598 | `		/* Check for '\|' (multi-catch separator) */` |
|     24470 | 12599 | `		if( pGen->pIn < pGen->pEnd &&` |
|     24470 | 12600 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|        35 | 12601 | `			pGen->pIn->sData.nByte == 1 &&` |
|        30 | 12602 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|        32 | 12603 | `			pGen->pIn++; /* Consume the '\|' */` |
|        32 | 12604 | `			continue;` |
|         - | 12605 | `		}` |
|     24445 | 12606 | `		break;` |
|       ! 0 | 12607 | `	}` |
|         - | 12608 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|         - | 12609 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding;` |
|         - | 12610 | `	 * jump straight to compiling the block below. */` |
|     24445 | 12611 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) /*)*/ ){` |
|         5 | 12612 | `		goto CatchBody;` |
|         - | 12613 | `	}` |
|     24436 | 12614 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     24441 | 12615 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - | 12616 | `			/* Unexpected token,break immediately */` |
|       ! 0 | 12617 | `			pToken = pGen->pIn;` |
|       ! 0 | 12618 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12619 | `				pToken--;` |
|       ! 0 | 12620 | `			}` |
|       ! 0 | 12621 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12622 | `				"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12623 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12624 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12625 | `				return SXERR_ABORT;` |
|         - | 12626 | `			}` |
|       ! 0 | 12627 | `			return SXERR_INVALID;` |
|         - | 12628 | `	}` |
|     24441 | 12629 | `	pGen->pIn++; /* Jump the dollar sign */` |
|         - | 12630 | `	/* Duplicate instance name */` |
|     24441 | 12631 | `	pName = &pGen->pIn->sData;` |
|     24441 | 12632 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     24441 | 12633 | `	if( zDup == 0 ){` |
|       ! 0 | 12634 | `		goto Mem;` |
|         - | 12635 | `	}` |
|     24441 | 12636 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     24441 | 12637 | `	pGen->pIn++;` |
|     12220 | 12638 | `CatchBody:` |
|     24445 | 12639 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|         - | 12640 | `		/* Unexpected token,break immediately */` |
|       ! 0 | 12641 | `		pToken = pGen->pIn;` |
|       ! 0 | 12642 | `		if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12643 | `			pToken--;` |
|       ! 0 | 12644 | `		}` |
|       ! 0 | 12645 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12646 | `			"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12647 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12648 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12649 | `			return SXERR_ABORT;` |
|         - | 12650 | `		}` |
|       ! 0 | 12651 | `		return SXERR_INVALID;` |
|         - | 12652 | `	}` |
|         - | 12653 | `	/* Compile the block */` |
|     24445 | 12654 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|         - | 12655 | `	/* Create the catch block */` |
|     24445 | 12656 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     24445 | 12657 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12658 | `		return SXERR_ABORT;` |
|         - | 12659 | `	}` |
|         - | 12660 | `	/* Swap bytecode container */` |
|     24445 | 12661 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     24445 | 12662 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|         - | 12663 | `	/* Compile the block */` |
|     24445 | 12664 | `	PH7_CompileBlock(&(*pGen),0);` |
|         - | 12665 | `	/* Fix forward jumps now the destination is resolved  */` |
|     24445 | 12666 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12667 | `	/* Emit the DONE instruction */` |
|     24445 | 12668 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - | 12669 | `	/* Leave the block */` |
|     24445 | 12670 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12671 | `	/* Restore the default container */` |
|     24445 | 12672 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - | 12673 | `	/* Install the catch block */` |
|     24445 | 12674 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     24445 | 12675 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12676 | `		goto Mem;` |
|         - | 12677 | `	}` |
|     24445 | 12678 | `	return SXRET_OK;` |
|       ! 0 | 12679 | `Mem:` |
|       ! 0 | 12680 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 12681 | `	return SXERR_ABORT;` |
|     12227 | 12682 | `}` |
|         - | 12683 | `/*` |
|         - | 12684 | ` * Compile a 'try' block.` |
|         - | 12685 | ` * A function using an exception should be in a "try" block.` |
|         - | 12686 | ` * If the exception does not trigger, the code will continue` |
|         - | 12687 | ` * as normal. However if the exception triggers, an exception` |
|         - | 12688 | ` * is "thrown".` |
|         - | 12689 | ` */` |
|     24602 | 12690 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|         5 | 12691 | `{` |
|         - | 12692 | `	ph7_exception *pException;` |
|     24607 | 12693 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12694 | `	GenBlock *pTry;` |
|         - | 12695 | `	sxu32 nJmpIdx;` |
|         - | 12696 | `	sxi32 rc;` |
|         - | 12697 | `	/* Create the exception container */` |
|     24607 | 12698 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     24607 | 12699 | `	if( pException == 0 ){` |
|       ! 0 | 12700 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|       ! 0 | 12701 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 12702 | `		return SXERR_ABORT;` |
|         - | 12703 | `	}` |
|         - | 12704 | `	/* Zero the structure */` |
|     24607 | 12705 | `	SyZero(pException,sizeof(ph7_exception));` |
|         - | 12706 | `	/* Initialize fields */` |
|     24607 | 12707 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     24607 | 12708 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     24607 | 12709 | `	pException->iHasFinally = 0;` |
|     24607 | 12710 | `	pException->iFinallyDone = 0;` |
|     24607 | 12711 | `	pException->pVm = pGen->pVm;` |
|         - | 12712 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|         - | 12713 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|         - | 12714 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|         - | 12715 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|         - | 12716 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|         - | 12717 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|     24607 | 12718 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|       105 | 12719 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|         - | 12720 | `	}` |
|         - | 12721 | `	/* Create the try block */` |
|     24507 | 12722 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     24507 | 12723 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12724 | `		return SXERR_ABORT;` |
|         - | 12725 | `	}` |
|         - | 12726 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     24507 | 12727 | `	pTry->pUserData = pException;` |
|         - | 12728 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     24507 | 12729 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|         - | 12730 | `	/* Fix the jump later when the destination is resolved */` |
|     24507 | 12731 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     24507 | 12732 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|         - | 12733 | `	/* Compile the block */` |
|     24507 | 12734 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     24507 | 12735 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12736 | `		return SXERR_ABORT;` |
|         - | 12737 | `	}` |
|         - | 12738 | `	/* Fix forward jumps now the destination is resolved */` |
|     24507 | 12739 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12740 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     24507 | 12741 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|         - | 12742 | `	/* Leave the block */` |
|     24507 | 12743 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12744 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     24507 | 12745 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     24500 | 12746 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|         - | 12747 | `		/* Compile one or more catch blocks */` |
|     24440 | 12748 | `		for(;;){` |
|     48880 | 12749 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     36717 | 12750 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     12223 | 12751 | `					break;` |
|         - | 12752 | `			}` |
|     24449 | 12753 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     24449 | 12754 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12755 | `				return SXERR_ABORT;` |
|         - | 12756 | `			}` |
|         5 | 12757 | `		}` |
|     12218 | 12758 | `	}` |
|         - | 12759 | `	/* Compile optional finally block */` |
|     24507 | 12760 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       736 | 12761 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|         - | 12762 | `		SySet *pInstrContainer;` |
|         - | 12763 | `		GenBlock *pFinBlock;` |
|       129 | 12764 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|         - | 12765 | `		/* Create the finally block for jump fixup bookkeeping */` |
|       129 | 12766 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|       129 | 12767 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 12768 | `			return SXERR_ABORT;` |
|         - | 12769 | `		}` |
|         - | 12770 | `		/* Swap bytecode container */` |
|       129 | 12771 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       129 | 12772 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|         - | 12773 | `		/* Compile the finally body */` |
|       129 | 12774 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       129 | 12775 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12776 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 | 12777 | `			return SXERR_ABORT;` |
|         - | 12778 | `		}` |
|         - | 12779 | `		/* Fix forward jumps now the destination is resolved */` |
|       129 | 12780 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12781 | `		/* Emit DONE to terminate the finally block */` |
|       129 | 12782 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - | 12783 | `		/* Leave the block */` |
|       129 | 12784 | `		GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12785 | `		/* Restore the default container */` |
|       129 | 12786 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       129 | 12787 | `		pException->iHasFinally = 1;` |
|        62 | 12788 | `	}` |
|         - | 12789 | `	/* Must have at least one catch or finally */` |
|     24507 | 12790 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|         9 | 12791 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12792 | `			"Cannot use try without catch or finally");` |
|         9 | 12793 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12794 | `			return SXERR_ABORT;` |
|         - | 12795 | `		}` |
|         3 | 12796 | `	}` |
|     24507 | 12797 | `	return SXRET_OK;` |
|     12306 | 12798 | `}` |
|         - | 12799 | `/*` |
|         - | 12800 | ` * Compile a switch block.` |
|         - | 12801 | ` *  (See block-comment below for more information)` |
|         - | 12802 | ` */` |
|     53592 | 12803 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|         5 | 12804 | `{` |
|     53597 | 12805 | `	sxi32 rc = SXRET_OK;` |
|     53597 | 12806 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|         - | 12807 | `		/* Unexpected token */` |
|       ! 0 | 12808 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 | 12809 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12810 | `			return SXERR_ABORT;` |
|         - | 12811 | `		}` |
|       ! 0 | 12812 | `		pGen->pIn++;` |
|       ! 0 | 12813 | `	}` |
|     53597 | 12814 | `	pGen->pIn++;` |
|         - | 12815 | `	/* First instruction to execute in this block. */` |
|     53597 | 12816 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|         - | 12817 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|         - | 12818 | `	 * or the '}' token */` |
|     38406 | 12819 | `	for(;;){` |
|     76817 | 12820 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 12821 | `			/* No more input to process */` |
|       ! 0 | 12822 | `			break;` |
|         - | 12823 | `		}` |
|     76817 | 12824 | `		rc = SXRET_OK;` |
|     76817 | 12825 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      3905 | 12826 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      3851 | 12827 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|         - | 12828 | `					/* Unexpected token */` |
|       ! 0 | 12829 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|       ! 0 | 12830 | `						&pGen->pIn->sData);` |
|       ! 0 | 12831 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12832 | `						return SXERR_ABORT;` |
|         - | 12833 | `					}` |
|         - | 12834 | `					/* FALL THROUGH */` |
|       ! 0 | 12835 | `				}` |
|      3851 | 12836 | `				rc = SXERR_EOF;` |
|      3851 | 12837 | `				break;` |
|         - | 12838 | `			}` |
|        32 | 12839 | `		}else{` |
|         - | 12840 | `			sxi32 nKwrd;` |
|         - | 12841 | `			/* Extract the keyword */` |
|     72917 | 12842 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     72917 | 12843 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|     24877 | 12844 | `				break;` |
|         - | 12845 | `			}` |
|     23173 | 12846 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|         3 | 12847 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|         - | 12848 | `					/* Unexpected token */` |
|       ! 0 | 12849 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|       ! 0 | 12850 | `						&pGen->pIn->sData);` |
|       ! 0 | 12851 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12852 | `						return SXERR_ABORT;` |
|         - | 12853 | `					}` |
|         - | 12854 | `					/* FALL THROUGH */` |
|       ! 0 | 12855 | `				}` |
|         - | 12856 | `				/* Block compiled */` |
|         3 | 12857 | `				break;` |
|         - | 12858 | `			}` |
|         - | 12859 | `		}` |
|         - | 12860 | `		/* Compile block */` |
|     23225 | 12861 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     23225 | 12862 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12863 | `			return SXERR_ABORT;` |
|         - | 12864 | `		}` |
|         5 | 12865 | `	}` |
|     53597 | 12866 | `	return rc;` |
|     26801 | 12867 | `}` |
|         - | 12868 | `/*` |
|         - | 12869 | ` * Compile a case eXpression.` |
|         - | 12870 | ` *  (See block-comment below for more information)` |
|         - | 12871 | ` */` |
|     53572 | 12872 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|         5 | 12873 | `{` |
|         - | 12874 | `	SySet *pInstrContainer;` |
|         - | 12875 | `	SyToken *pEnd,*pTmp;` |
|     53577 | 12876 | `	sxi32 iNest = 0;` |
|         - | 12877 | `	sxi32 rc;` |
|         - | 12878 | `	/* Delimit the expression */` |
|     53577 | 12879 | `	pEnd = pGen->pIn;` |
|    107157 | 12880 | `	while( pEnd < pGen->pEnd ){` |
|    107157 | 12881 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|         - | 12882 | `			/* Increment nesting level */` |
|         3 | 12883 | `			iNest++;` |
|    107156 | 12884 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|         - | 12885 | `			/* Decrement nesting level */` |
|         3 | 12886 | `			iNest--;` |
|    107154 | 12887 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|     53577 | 12888 | `			break;` |
|         - | 12889 | `		}` |
|     53585 | 12890 | `		pEnd++;` |
|         5 | 12891 | `	}` |
|     53577 | 12892 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 | 12893 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|       ! 0 | 12894 | `		if( rc == SXERR_ABORT ){` |
|         - | 12895 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12896 | `			return SXERR_ABORT;` |
|         - | 12897 | `		}` |
|       ! 0 | 12898 | `	}` |
|         - | 12899 | `	/* Swap token stream */` |
|     53577 | 12900 | `	pTmp = pGen->pEnd;` |
|     53577 | 12901 | `	pGen->pEnd = pEnd;` |
|     53577 | 12902 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     53577 | 12903 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|     53577 | 12904 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - | 12905 | `	/* Emit the done instruction */` |
|     53577 | 12906 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     53577 | 12907 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - | 12908 | `	/* Update token stream */` |
|     53577 | 12909 | `	pGen->pIn  = pEnd;` |
|     53577 | 12910 | `	pGen->pEnd = pTmp;` |
|     53577 | 12911 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12912 | `		return SXERR_ABORT;` |
|         - | 12913 | `	}` |
|     53577 | 12914 | `	return SXRET_OK;` |
|     26791 | 12915 | `}` |
|         - | 12916 | `/*` |
|         - | 12917 | ` * Compile the smart switch statement.` |
|         - | 12918 | ` * According to the PHP language reference manual` |
|         - | 12919 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|         - | 12920 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|         - | 12921 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|         - | 12922 | ` *  This is exactly what the switch statement is for.` |
|         - | 12923 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|         - | 12924 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|         - | 12925 | ` *  of the outer loop, use continue 2.` |
|         - | 12926 | ` *  Note that switch/case does loose comparision.` |
|         - | 12927 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|         - | 12928 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|         - | 12929 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|         - | 12930 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|         - | 12931 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|         - | 12932 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|         - | 12933 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|         - | 12934 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|         - | 12935 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|         - | 12936 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|         - | 12937 | ` *  list for the next case.` |
|         - | 12938 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|         - | 12939 | ` *  or floating-point numbers and strings.` |
|         - | 12940 | ` */` |
|      3848 | 12941 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|         5 | 12942 | `{` |
|         - | 12943 | `	GenBlock *pSwitchBlock;` |
|         - | 12944 | `	SyToken *pTmp,*pEnd;` |
|         - | 12945 | `	ph7_switch *pSwitch;` |
|         - | 12946 | `	sxu32 nToken;` |
|         - | 12947 | `	sxu32 nLine;` |
|         - | 12948 | `	sxi32 rc;` |
|      3853 | 12949 | `	nLine = pGen->pIn->nLine;` |
|         - | 12950 | `	/* Jump the 'switch' keyword */` |
|      3853 | 12951 | `	pGen->pIn++;` |
|      3853 | 12952 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 12953 | `		/* Syntax error */` |
|       ! 0 | 12954 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|       ! 0 | 12955 | `		if( rc == SXERR_ABORT ){` |
|         - | 12956 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12957 | `			return SXERR_ABORT;` |
|         - | 12958 | `		}` |
|       ! 0 | 12959 | `		goto Synchronize;` |
|         - | 12960 | `	}` |
|         - | 12961 | `	/* Jump the left parenthesis '(' */` |
|      3853 | 12962 | `	pGen->pIn++;` |
|      3853 | 12963 | `	pEnd = 0; /* cc warning */` |
|         - | 12964 | `	/* Create the loop block */` |
|      5777 | 12965 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      1924 | 12966 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      3853 | 12967 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12968 | `		return SXERR_ABORT;` |
|         - | 12969 | `	}` |
|         - | 12970 | `	/* Delimit the condition */` |
|      3853 | 12971 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      3853 | 12972 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - | 12973 | `		/* Empty expression */` |
|       ! 0 | 12974 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|       ! 0 | 12975 | `		if( rc == SXERR_ABORT ){` |
|         - | 12976 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12977 | `			return SXERR_ABORT;` |
|         - | 12978 | `		}` |
|       ! 0 | 12979 | `	}` |
|         - | 12980 | `	/* Swap token streams */` |
|      3853 | 12981 | `	pTmp = pGen->pEnd;` |
|      3853 | 12982 | `	pGen->pEnd = pEnd;` |
|         - | 12983 | `	/* Compile the expression */` |
|      3853 | 12984 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      3853 | 12985 | `	if( rc == SXERR_ABORT ){` |
|         - | 12986 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 | 12987 | `		return SXERR_ABORT;` |
|         - | 12988 | `	}` |
|         - | 12989 | `	/* Update token stream */` |
|      3853 | 12990 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 | 12991 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 12992 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 | 12993 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12994 | `			return SXERR_ABORT;` |
|         - | 12995 | `		}` |
|       ! 0 | 12996 | `		pGen->pIn++;` |
|       ! 0 | 12997 | `	}` |
|      3853 | 12998 | `	pGen->pIn  = &pEnd[1];` |
|      3853 | 12999 | `	pGen->pEnd = pTmp;` |
|      3853 | 13000 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      3848 | 13001 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|       ! 0 | 13002 | `			pTmp = pGen->pIn;` |
|       ! 0 | 13003 | `			if( pTmp >= pGen->pEnd ){` |
|       ! 0 | 13004 | `				pTmp--;` |
|       ! 0 | 13005 | `			}` |
|         - | 13006 | `			/* Unexpected token */` |
|       ! 0 | 13007 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|       ! 0 | 13008 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 13009 | `				return SXERR_ABORT;` |
|         - | 13010 | `			}` |
|       ! 0 | 13011 | `			goto Synchronize;` |
|         - | 13012 | `	}` |
|         - | 13013 | `	/* Set the delimiter token */` |
|      3853 | 13014 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|         3 | 13015 | `		nToken = PH7_TK_KEYWORD;` |
|         - | 13016 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|         2 | 13017 | `	}else{` |
|      3851 | 13018 | `		nToken = PH7_TK_CCB; /* '}' */` |
|         - | 13019 | `	}` |
|      3853 | 13020 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|         - | 13021 | `	/* Create the switch blocks container */` |
|      3853 | 13022 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      3853 | 13023 | `	if( pSwitch == 0 ){` |
|         - | 13024 | `		/* Abort compilation */` |
|       ! 0 | 13025 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 13026 | `		return SXERR_ABORT;` |
|         - | 13027 | `	}` |
|         - | 13028 | `	/* Zero the structure */` |
|      3853 | 13029 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|         - | 13030 | `	/* Initialize fields */` |
|      3853 | 13031 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|         - | 13032 | `	/* Emit the switch instruction */` |
|      3853 | 13033 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|         - | 13034 | `	/* Compile case blocks */` |
|     51670 | 13035 | `	for(;;){` |
|         - | 13036 | `		sxu32 nKwrd;` |
|     53599 | 13037 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 13038 | `			/* No more input to process */` |
|       ! 0 | 13039 | `			break;` |
|         - | 13040 | `		}` |
|     53599 | 13041 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 13042 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|         - | 13043 | `				/* Unexpected token */` |
|       ! 0 | 13044 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 13045 | `					&pGen->pIn->sData);` |
|       ! 0 | 13046 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 13047 | `					return SXERR_ABORT;` |
|         - | 13048 | `				}` |
|         - | 13049 | `				/* FALL THROUGH */` |
|       ! 0 | 13050 | `			}` |
|         - | 13051 | `			/* Block compiled */` |
|       ! 0 | 13052 | `			break;` |
|         - | 13053 | `		}` |
|         - | 13054 | `		/* Extract the keyword */` |
|     53599 | 13055 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     53599 | 13056 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|         3 | 13057 | `			if( nToken != PH7_TK_KEYWORD ){` |
|         - | 13058 | `				/* Unexpected token */` |
|       ! 0 | 13059 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 13060 | `					&pGen->pIn->sData);` |
|       ! 0 | 13061 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 13062 | `					return SXERR_ABORT;` |
|         - | 13063 | `				}` |
|         - | 13064 | `				/* FALL THROUGH */` |
|       ! 0 | 13065 | `			}` |
|         - | 13066 | `			/* Block compiled */` |
|         3 | 13067 | `			break;` |
|         - | 13068 | `		}` |
|     53597 | 13069 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|         - | 13070 | `			/*` |
|         - | 13071 | `			 * Accroding to the PHP language reference manual` |
|         - | 13072 | `			 *  A special case is the default case. This case matches anything` |
|         - | 13073 | `			 *  that wasn't matched by the other cases.` |
|         - | 13074 | `			 */` |
|        25 | 13075 | `			if( pSwitch->nDefault > 0 ){` |
|         - | 13076 | `				/* Default case already compiled */` |
|       ! 0 | 13077 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|       ! 0 | 13078 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 13079 | `					return SXERR_ABORT;` |
|         - | 13080 | `				}` |
|       ! 0 | 13081 | `			}` |
|        25 | 13082 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|         - | 13083 | `			/* Compile the default block */` |
|        25 | 13084 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|        25 | 13085 | `			if( rc == SXERR_ABORT){` |
|       ! 0 | 13086 | `				return SXERR_ABORT;` |
|        25 | 13087 | `			}else if( rc == SXERR_EOF ){` |
|        23 | 13088 | `				break;` |
|         1 | 13089 | `			}` |
|     53578 | 13090 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|         - | 13091 | `			ph7_case_expr sCase;` |
|         - | 13092 | `			/* Standard case block */` |
|     53577 | 13093 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|         - | 13094 | `			/* initialize the structure */` |
|     53577 | 13095 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - | 13096 | `			/* Compile the case expression */` |
|     53577 | 13097 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|     53577 | 13098 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 13099 | `				return SXERR_ABORT;` |
|         - | 13100 | `			}` |
|         - | 13101 | `			/* Compile the case block */` |
|     53577 | 13102 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|         - | 13103 | `			/* Insert in the switch container */` |
|     53577 | 13104 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|     53577 | 13105 | `			if( rc == SXERR_ABORT){` |
|       ! 0 | 13106 | `				return SXERR_ABORT;` |
|     53577 | 13107 | `			}else if( rc == SXERR_EOF ){` |
|      3833 | 13108 | `				break;` |
|         - | 13109 | `			}` |
|     24877 | 13110 | `		}else{` |
|         - | 13111 | `			/* Unexpected token */` |
|       ! 0 | 13112 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 13113 | `				&pGen->pIn->sData);` |
|       ! 0 | 13114 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 13115 | `				return SXERR_ABORT;` |
|         - | 13116 | `			}` |
|       ! 0 | 13117 | `			break;` |
|         - | 13118 | `		}` |
|         5 | 13119 | `	}` |
|         - | 13120 | `	/* Fix all jumps now the destination is resolved */` |
|      3853 | 13121 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      3853 | 13122 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 13123 | `	/* Release the loop block */` |
|      3853 | 13124 | `	GenStateLeaveBlock(pGen,0);` |
|      3853 | 13125 | `	if( pGen->pIn < pGen->pEnd ){` |
|         - | 13126 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      3853 | 13127 | `		pGen->pIn++;` |
|      1924 | 13128 | `	}` |
|         - | 13129 | `	/* Statement successfully compiled */` |
|      3853 | 13130 | `	return SXRET_OK;` |
|       ! 0 | 13131 | `Synchronize:` |
|         - | 13132 | `	/* Synchronize with the first semi-colon */` |
|       ! 0 | 13133 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       ! 0 | 13134 | `		pGen->pIn++;` |
|       ! 0 | 13135 | `	}` |
|       ! 0 | 13136 | `	return SXRET_OK;` |
|      1929 | 13137 | `}` |
|         - | 13138 | `/*` |
|         - | 13139 | ` * Chain operators participate in a postfix member-access chain.` |
|         - | 13140 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|         - | 13141 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|         - | 13142 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|         - | 13143 | ` */` |
|         - | 13144 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|         - | 13145 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|         - | 13146 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|         - | 13147 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|         - | 13148 |  |
|         - | 13149 | `/*` |
|         - | 13150 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|         - | 13151 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|         - | 13152 | ` * patched entries from the pending set.` |
|         - | 13153 | ` */` |
|  48449382 | 13154 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|         5 | 13155 | `{` |
|  48449387 | 13156 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|         - | 13157 | `	sxu32 nTarget;` |
|         - | 13158 | `	sxu32 *aIdx;` |
|         - | 13159 | `	sxu32 i;` |
|  48449387 | 13160 | `	if( nCur <= nBaseline ){` |
|  48449291 | 13161 | `		return;` |
|         - | 13162 | `	}` |
|       100 | 13163 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|       100 | 13164 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|       204 | 13165 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|       108 | 13166 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|       108 | 13167 | `		if( pInstr ){` |
|       108 | 13168 | `			pInstr->iP2 = (sxi32)nTarget;` |
|        52 | 13169 | `		}` |
|        56 | 13170 | `	}` |
|       100 | 13171 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
|  24224696 | 13172 | `}` |
|         - | 13173 |  |
|         - | 13174 | `/*` |
|         - | 13175 | ` * By-reference out-parameters of builtin functions.` |
|         - | 13176 | ` *` |
|         - | 13177 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|         - | 13178 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|         - | 13179 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|         - | 13180 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|         - | 13181 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|         - | 13182 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|         - | 13183 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|         - | 13184 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|         - | 13185 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|         - | 13186 | ` * creates it" behaviour).` |
|         - | 13187 | ` *` |
|         - | 13188 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|         - | 13189 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|         - | 13190 | ` */` |
|   6168892 | 13191 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|         5 | 13192 | `{` |
|         - | 13193 | `	static const struct {` |
|         - | 13194 | `		const char *zName;` |
|         - | 13195 | `		sxu32 nByte;` |
|         - | 13196 | `		sxu32 mask;` |
|         - | 13197 | `	} aByRef[] = {` |
|         - | 13198 | `		{ "parse_str",              9, 1u<<1 },  /* &$result (apArg[1]) */` |
|         - | 13199 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - | 13200 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - | 13201 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - | 13202 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - | 13203 | `		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */` |
|         - | 13204 | `		{ "fsockopen",              9, (1u<<2)\|(1u<<3) },  /* &$error_code, &$error_message */` |
|         - | 13205 | `		{ "pfsockopen",            10, (1u<<2)\|(1u<<3) },  /* same */` |
|         - | 13206 | `		{ "stream_socket_client",  20, (1u<<1)\|(1u<<2) },  /* &$error_code, &$error_message */` |
|         - | 13207 | `	};` |
|         - | 13208 | `	sxu32 i;` |
|   6168897 | 13209 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|   1620727 | 13210 | `		return 0;` |
|         - | 13211 | `	}` |
|  45064415 | 13212 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
|  40573684 | 13213 | `		if( pName->nByte == aByRef[i].nByte` |
|  21405106 | 13214 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|     57449 | 13215 | `			return aByRef[i].mask;` |
|         - | 13216 | `		}` |
|  20258125 | 13217 | `	}` |
|   4490731 | 13218 | `	return 0;` |
|   3084451 | 13219 | `}` |
|         - | 13220 | `/*` |
|         - | 13221 | ` * Recover the bare global-builtin name from a call's callee node.` |
|         - | 13222 | ` *` |
|         - | 13223 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|         - | 13224 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|         - | 13225 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|         - | 13226 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|         - | 13227 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|         - | 13228 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|         - | 13229 | ` */` |
|   6168892 | 13230 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|         5 | 13231 | `{` |
|         - | 13232 | `	SyToken *p, *pEnd;` |
|   6168897 | 13233 | `	pOut->zString = 0;` |
|   6168897 | 13234 | `	pOut->nByte = 0;` |
|   6168897 | 13235 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|       ! 0 | 13236 | `		return;` |
|         - | 13237 | `	}` |
|   6168897 | 13238 | `	p = pLeft->pStart;` |
|   6168897 | 13239 | `	pEnd = pLeft->pEnd;` |
|         - | 13240 | `	/* Optional single leading namespace separator (absolute path). */` |
|   6168897 | 13241 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      3863 | 13242 | `		p++;` |
|      1929 | 13243 | `	}` |
|   6168897 | 13244 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|   1620685 | 13245 | `		return;` |
|         - | 13246 | `	}` |
|         - | 13247 | `	/* Must be a single component: nothing follows the name token. */` |
|   4548217 | 13248 | `	if( p + 1 != pEnd ){` |
|        47 | 13249 | `		return;` |
|         - | 13250 | `	}` |
|   4548175 | 13251 | `	*pOut = p->sData;` |
|   3084451 | 13252 | `}` |
|         - | 13253 | `/*` |
|         - | 13254 | ` * Generate bytecode for a given expression tree.` |
|         - | 13255 | ` * If something goes wrong while generating bytecode` |
|         - | 13256 | ` * for the expression tree (A very unlikely scenario)` |
|         - | 13257 | ` * this function takes care of generating the appropriate` |
|         - | 13258 | ` * error message.` |
|         - | 13259 | ` */` |
|  67410532 | 13260 | `static sxi32 GenStateEmitExprCode(` |
|         - | 13261 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 13262 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|         - | 13263 | `	sxi32 iFlags /* Control flags */` |
|         - | 13264 | `	)` |
|         5 | 13265 | `{` |
|         - | 13266 | `	VmInstr *pInstr;` |
|         - | 13267 | `	sxu32 nJmpIdx;` |
|  67410537 | 13268 | `	sxi32 iP1 = 0;` |
|  67410537 | 13269 | `	sxu32 iP2 = 0;` |
|  67410537 | 13270 | `	void *p3  = 0;` |
|         - | 13271 | `	sxi32 iVmOp;` |
|         - | 13272 | `	sxi32 rc;` |
|  67410537 | 13273 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
|  67410537 | 13274 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
|  67410537 | 13275 | `	sxu32 nRhsNsBase = 0;` |
|  67410537 | 13276 | `	if( pNode->xCode ){` |
|         - | 13277 | `		SyToken *pTmpIn,*pTmpEnd;` |
|         - | 13278 | `		/* Compile node */` |
|  40666839 | 13279 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|  40666839 | 13280 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|  40666839 | 13281 | `		RE_SWAP_DELIMITER(pGen);` |
|  40666839 | 13282 | `		return rc;` |
|         - | 13283 | `	}` |
|  26743703 | 13284 | `	if( pNode->pOp == 0 ){` |
|       ! 0 | 13285 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13286 | `			"Invalid expression node,PH7 is aborting compilation");` |
|       ! 0 | 13287 | `		return SXERR_ABORT;` |
|         - | 13288 | `	}` |
|  26743703 | 13289 | `	iVmOp = pNode->pOp->iVmOp;` |
|  26743703 | 13290 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|         - | 13291 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|         - | 13292 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|         - | 13293 | `		 * and later errors are still reported. */` |
|         3 | 13294 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13295 | `			"The (unset) cast is no longer supported");` |
|         3 | 13296 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 13297 | `			return SXERR_ABORT;` |
|         - | 13298 | `		}` |
|         1 | 13299 | `	}` |
|  26743703 | 13300 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        93 | 13301 | `		sxu32 nJmp = 0;` |
|         - | 13302 | `		sxu32 nNcNsBase;` |
|         - | 13303 | `		VmInstr *pInstrFix;` |
|         - | 13304 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|         - | 13305 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|         - | 13306 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|         - | 13307 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|         - | 13308 | `		 * stack slot carries a writable nIdx. */` |
|        93 | 13309 | `		if( pNode->pRight ){` |
|        93 | 13310 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        93 | 13311 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|        93 | 13312 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13313 | `				return rc;` |
|         - | 13314 | `			}` |
|        93 | 13315 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|         - | 13316 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|         - | 13317 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|         - | 13318 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|         - | 13319 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|         - | 13320 | `			 * the store, so the parent array does not need to be copied at` |
|         - | 13321 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|         - | 13322 | `			 * cascade for the actual write path stays correct. */` |
|        93 | 13323 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|        93 | 13324 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|        33 | 13325 | `				pInstrFix->iP2 = 3;` |
|        15 | 13326 | `			}` |
|        45 | 13327 | `		}` |
|         - | 13328 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|        93 | 13329 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|         - | 13330 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|        93 | 13331 | `		if( pNode->pLeft ){` |
|        93 | 13332 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        93 | 13333 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|        93 | 13334 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13335 | `				return rc;` |
|         - | 13336 | `			}` |
|        93 | 13337 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        45 | 13338 | `		}` |
|         - | 13339 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|        93 | 13340 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|         - | 13341 | `		/* Patch the short-circuit jump to land after the store. */` |
|        93 | 13342 | `		if( nJmp > 0 ){` |
|        93 | 13343 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|        93 | 13344 | `			if( pInstrFix ){` |
|        93 | 13345 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|        45 | 13346 | `			}` |
|        45 | 13347 | `		}` |
|        93 | 13348 | `		return SXRET_OK;` |
|         - | 13349 | `	}` |
|  26743613 | 13350 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|         - | 13351 | `		sxu32 nJz,nJmp;` |
|         - | 13352 | `		sxu32 nTernaryNsBase;` |
|         - | 13353 | `		/* Ternary operator require special handling */` |
|         - | 13354 | `		/* Phase#1: Compile the condition */` |
|    457941 | 13355 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    457941 | 13356 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    457941 | 13357 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13358 | `			return rc;` |
|         - | 13359 | `		}` |
|         - | 13360 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|         - | 13361 | `		 * compiling the condition must short-circuit to the end of the` |
|         - | 13362 | `		 * condition expression, not leak past the ternary. */` |
|    457941 | 13363 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    457941 | 13364 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    457941 | 13365 | `		if( pNode->pLeft ){` |
|         - | 13366 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|         - | 13367 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    454053 | 13368 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 13369 | `			/* Phase#3: Compile the 'then' expression  */` |
|    454053 | 13370 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    454053 | 13371 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    454053 | 13372 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13373 | `				return rc;` |
|         - | 13374 | `			}` |
|    454053 | 13375 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    227029 | 13376 | `		}else{` |
|         - | 13377 | `			/* Elvis operator: (expr) ?: (else)` |
|         - | 13378 | `			 * Duplicate condition so original value is the 'then' result.` |
|         - | 13379 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      3893 | 13380 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      3893 | 13381 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 13382 | `		}` |
|         - | 13383 | `		/* Phase#4: Emit the unconditional jump */` |
|    457941 | 13384 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|         - | 13385 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    457941 | 13386 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    457941 | 13387 | `		if( pInstr ){` |
|    457941 | 13388 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    228968 | 13389 | `		}` |
|    457941 | 13390 | `		if( !pNode->pLeft ){` |
|         - | 13391 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      3893 | 13392 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      1944 | 13393 | `		}` |
|         - | 13394 | `		/* Phase#6: Compile the 'else' expression */` |
|    457941 | 13395 | `		if( pNode->pRight ){` |
|    457941 | 13396 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    457941 | 13397 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    457941 | 13398 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13399 | `				return rc;` |
|         - | 13400 | `			}` |
|    457941 | 13401 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    228968 | 13402 | `		}` |
|    457941 | 13403 | `		if( nJmp > 0 ){` |
|         - | 13404 | `			/* Phase#7: Fix the unconditional jump */` |
|    457941 | 13405 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    457941 | 13406 | `			if( pInstr ){` |
|    457941 | 13407 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    228968 | 13408 | `			}` |
|    228968 | 13409 | `		}` |
|         - | 13410 | `		/* All done */` |
|    457941 | 13411 | `		return SXRET_OK;` |
|         - | 13412 | `	}` |
|  26285677 | 13413 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|         - | 13414 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|         - | 13415 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|         - | 13416 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|         - | 13417 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|         - | 13418 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|         - | 13419 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|         - | 13420 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|         - | 13421 | `		sxu32 nPipeNsBase;` |
|        27 | 13422 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|        27 | 13423 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|       ! 0 | 13424 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13425 | `				"'\|>': Missing operand");` |
|       ! 0 | 13426 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 13427 | `		}` |
|         - | 13428 | `		/* Argument: the LHS value. */` |
|        27 | 13429 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 13430 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|        27 | 13431 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13432 | `			return rc;` |
|         - | 13433 | `		}` |
|        27 | 13434 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 13435 | `		/* Callable: the RHS. */` |
|        27 | 13436 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 13437 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|        27 | 13438 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13439 | `			return rc;` |
|         - | 13440 | `		}` |
|        27 | 13441 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 13442 | `		/* Invoke the callable with the single piped argument. */` |
|        27 | 13443 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        27 | 13444 | `		return SXRET_OK;` |
|         - | 13445 | `	}` |
|  26285651 | 13446 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|         - | 13447 | `	/* Generate code for the left tree */` |
|  26285651 | 13448 | `	if( pNode->pLeft ){` |
|  26262741 | 13449 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  26262741 | 13450 | `		if( iVmOp == PH7_OP_CALL ){` |
|         - | 13451 | `			ph7_expr_node **apNode;` |
|   6173043 | 13452 | `			int hasSpread = 0;` |
|   6173043 | 13453 | `			int hasNamed = 0;` |
|   6173043 | 13454 | `			int bAnySpread = 0;` |
|   6173043 | 13455 | `			sxu32 byRefMask = 0;` |
|         - | 13456 | `			sxi32 nArgs;` |
|         - | 13457 | `			sxi32 n;` |
|         - | 13458 | `			/* Recurse and generate bytecodes for function arguments */` |
|   6173043 | 13459 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   6173043 | 13460 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|         - | 13461 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|         - | 13462 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|         - | 13463 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|   6173043 | 13464 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|        81 | 13465 | `				bFcc = 1;` |
|        81 | 13466 | `				nArgs = 0;` |
|        40 | 13467 | `			}` |
|         - | 13468 | `			/* Validate argument order like php: no positional argument after a` |
|         - | 13469 | ``			 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|         - | 13470 | `			{` |
|   6173043 | 13471 | `				int seenNamed = 0;` |
|   6173043 | 13472 | `				int seenSpread = 0;` |
|  12979061 | 13473 | `				for( n = 0; n < nArgs; ++n ){` |
|   6806025 | 13474 | `					if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      4019 | 13475 | `						bAnySpread = 1;` |
|      4019 | 13476 | `						seenSpread = 1;` |
|      4019 | 13477 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       ! 0 | 13478 | `							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13479 | `								"syntax error, unexpected token \"...\"");` |
|       ! 0 | 13480 | `							return SXERR_SYNTAX;` |
|         5 | 13481 | `						}` |
|   6804018 | 13482 | `					}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       289 | 13483 | `						seenNamed = 1;` |
|       289 | 13484 | `						hasNamed = 1;` |
|   6801869 | 13485 | `					}else if( seenNamed ){` |
|         3 | 13486 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13487 | `							"Cannot use positional argument after named argument");` |
|         3 | 13488 | `						return SXERR_SYNTAX;` |
|   6801725 | 13489 | `					}else if( seenSpread ){` |
|       ! 0 | 13490 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13491 | `							"Cannot use positional argument after argument unpacking");` |
|       ! 0 | 13492 | `						return SXERR_SYNTAX;` |
|         - | 13493 | `					}` |
|   3403014 | 13494 | `				}` |
|         - | 13495 | `			}` |
|         - | 13496 | `			/* Read-only load */` |
|   6173041 | 13497 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|         - | 13498 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|         - | 13499 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|         - | 13500 | `			 * objects dispatch to the right method (offsetExists for both;` |
|         - | 13501 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|   6173041 | 13502 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|   6173041 | 13503 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|   6173036 | 13504 | `				if( pCallName->nByte == 5` |
|   3462312 | 13505 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|    313627 | 13506 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|   6016230 | 13507 | `				}else if( pCallName->nByte == 5` |
|   3148690 | 13508 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|       117 | 13509 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|        56 | 13510 | `				}` |
|         - | 13511 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|         - | 13512 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|         - | 13513 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|         - | 13514 | `				 * write back through. Skipped when spread/named args are present:` |
|         - | 13515 | `				 * the compile-time positional index no longer maps to the` |
|         - | 13516 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|   6173041 | 13517 | `				if( !bAnySpread && !hasNamed ){` |
|         - | 13518 | `					SyString sBuiltin;` |
|   6168897 | 13519 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|   6168897 | 13520 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|   3084446 | 13521 | `				}` |
|   3086518 | 13522 | `			}` |
|  12979057 | 13523 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|   6806021 | 13524 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   6806021 | 13525 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 13526 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|         - | 13527 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|         - | 13528 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|         - | 13529 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|         - | 13530 | `				 * builtin to write back through. A plain $var target is unaffected` |
|         - | 13531 | `				 * (iP1=0 either way). */` |
|   6806021 | 13532 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|     38285 | 13533 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|     38285 | 13534 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|     19140 | 13535 | `				}` |
|   6806021 | 13536 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|   6806021 | 13537 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 13538 | `					return rc;` |
|         - | 13539 | `				}` |
|         - | 13540 | `				/* Each argument is an independent nullsafe scope. */` |
|   6806021 | 13541 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|   6806021 | 13542 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|         - | 13543 | `					/* Emit spread opcode to unpack this array argument */` |
|      4019 | 13544 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      4019 | 13545 | `					hasSpread = 1;` |
|      2007 | 13546 | `				}` |
|   3403013 | 13547 | `			}` |
|         - | 13548 | `			/* Total number of given arguments */` |
|   6173041 | 13549 | `			iP1 = nArgs;` |
|   6173041 | 13550 | `			iP2 = hasSpread;` |
|         - | 13551 | `			/* Build VmCallArgMap if named arguments are present.` |
|         - | 13552 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|   6173041 | 13553 | `			if( hasNamed ){` |
|       178 | 13554 | `				sxu32 nStrBytes = 0;` |
|         - | 13555 | `				char *zBuf;` |
|       534 | 13556 | `				for( n = 0; n < nArgs; ++n ){` |
|       360 | 13557 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       286 | 13558 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|       141 | 13559 | `					}` |
|       182 | 13560 | `				}` |
|         - | 13561 | `				{` |
|       178 | 13562 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|       178 | 13563 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       174 | 13564 | `					&pGen->pVm->sAllocator, mapSize);` |
|       178 | 13565 | `				if( pMap ){` |
|       178 | 13566 | `					SyZero(pMap, mapSize);` |
|       178 | 13567 | `					pMap->bHasNamed = 1;` |
|       178 | 13568 | `					pMap->nTotal = (sxu32)nArgs;` |
|       178 | 13569 | `					pMap->aNames = (SyString *)&pMap[1];` |
|       178 | 13570 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|       534 | 13571 | `					for( n = 0; n < nArgs; ++n ){` |
|       360 | 13572 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       286 | 13573 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|       286 | 13574 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|       286 | 13575 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|       286 | 13576 | `							zBuf += nb;` |
|       141 | 13577 | `						}` |
|         - | 13578 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|       182 | 13579 | `					}` |
|       178 | 13580 | `					p3 = (void *)pMap;` |
|        87 | 13581 | `				}` |
|         - | 13582 | `				}` |
|        87 | 13583 | `			}` |
|         - | 13584 | `			/* Remove stale flags now */` |
|   6173041 | 13585 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|   3086518 | 13586 | `		}` |
|         - | 13587 | `		{` |
|         - | 13588 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|         - | 13589 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|         - | 13590 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|         - | 13591 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|         - | 13592 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|         - | 13593 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|         - | 13594 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|         - | 13595 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
|  26262739 | 13596 | `			sxi32 iLeftFlags = iFlags;` |
|  26262734 | 13597 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  21583047 | 13598 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|   8451706 | 13599 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   7330277 | 13600 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   2422957 | 13601 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   1211476 | 13602 | `			}` |
|         - | 13603 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|         - | 13604 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|         - | 13605 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|         - | 13606 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|         - | 13607 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|         - | 13608 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|         - | 13609 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
|  26262734 | 13610 | `			if( pNode->pOp` |
|  37104690 | 13611 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
|  23973370 | 13612 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  21683954 | 13613 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|   4946519 | 13614 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   2473257 | 13615 | `			}` |
|         - | 13616 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|         - | 13617 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|         - | 13618 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|         - | 13619 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|         - | 13620 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|         - | 13621 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
|  26262734 | 13622 | `			if( pNode->pOp` |
|  26262739 | 13623 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|    195325 | 13624 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|     97660 | 13625 | `			}` |
|  26262739 | 13626 | `			if( iVmOp == PH7_OP_ERR_CTRL ){` |
|         - | 13627 | `				/* '@' must suppress the diagnostics raised WHILE its operand runs, so` |
|         - | 13628 | `				 * open the window here; the trailing emit below closes it (iP1 = 0). */` |
|     11683 | 13629 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);` |
|      5839 | 13630 | `			}` |
|  26262739 | 13631 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|         - | 13632 | `		}` |
|  26262739 | 13633 | `		if( rc != SXRET_OK ){` |
|        34 | 13634 | `			return rc;` |
|         - | 13635 | `		}` |
|  26262709 | 13636 | `		if( !bIsChainOp ){` |
|         - | 13637 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|         - | 13638 | `			 * target the end of that LHS chain, which is right here. */` |
|  12270947 | 13639 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|   6135471 | 13640 | `		}` |
|  26262709 | 13641 | `		if( iVmOp == PH7_OP_CALL ){` |
|   6173041 | 13642 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   6173041 | 13643 | `			if( pInstr ){` |
|   6173041 | 13644 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|   4548467 | 13645 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|         - | 13646 | `					sxu32 nQual;` |
|   4548467 | 13647 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 13648 | `					/* Prevent constant expansion but preserve the absolute flag` |
|         - | 13649 | `					 * so the later NEW handler (if any) can see it. */` |
|   4548467 | 13650 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|         - | 13651 | `					/* Namespace-qualify the function name for CALL, unless the` |
|         - | 13652 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|         - | 13653 | `					 * imports — class imports must NOT affect function` |
|         - | 13654 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|         - | 13655 | `					 * before NEW; we store the original literal index in the` |
|         - | 13656 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|         - | 13657 | `					 * the unqualified name and re-qualify with class imports. */` |
|   4548467 | 13658 | `					if( bAbsolute ){` |
|      3863 | 13659 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      1934 | 13660 | `					}else{` |
|   4544609 | 13661 | `						int fromImport = 0;` |
|   4544609 | 13662 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|   4544609 | 13663 | `						pInstr->iP2 = (sxi32)nQual;` |
|   4544609 | 13664 | `						if( nQual != nOrig ){` |
|         - | 13665 | `							/* Record the original literal index in the arg map` |
|         - | 13666 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|         - | 13667 | `							 * flag) so the NEW handler can recover the` |
|         - | 13668 | `							 * unqualified name and re-qualify with CLASS` |
|         - | 13669 | `							 * imports. */` |
|        97 | 13670 | `							if( p3 == 0 ){` |
|        97 | 13671 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|        92 | 13672 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|        97 | 13673 | `								if( pMap ){` |
|        97 | 13674 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|        97 | 13675 | `									p3 = (void *)pMap;` |
|        46 | 13676 | `								}` |
|        46 | 13677 | `							}` |
|        97 | 13678 | `							if( p3 ){` |
|        97 | 13679 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|        97 | 13680 | `								if( !fromImport ){` |
|         - | 13681 | `									/* Mark as namespace-qualified */` |
|        87 | 13682 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|        41 | 13683 | `								}` |
|        46 | 13684 | `							}` |
|        46 | 13685 | `						}` |
|         5 | 13686 | `					}` |
|   3898810 | 13687 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|         - | 13688 | `					/* Method call,flag that */` |
|   1604819 | 13689 | `					pInstr->iP2 = 1;` |
|         - | 13690 | ``					/* A static call with a DYNAMIC method name (`C::$m(...)`): the`` |
|         - | 13691 | ``					 * static-`::` codegen folded the variable NAME into OP_MEMBER->p3`` |
|         - | 13692 | ``					 * as if it were a static-PROPERTY read (`C::$m`), but in a CALL the`` |
|         - | 13693 | `					 * method name is the variable's VALUE. Rebuild the sequence` |
|         - | 13694 | `					 * [class, OP_LOAD $m -> value, OP_MEMBER(static, method)] so the` |
|         - | 13695 | `					 * dynamic name is read off the stack, matching the instance` |
|         - | 13696 | ``					 * (`$o->$m()`) path. iP1==1 marks a static member. */`` |
|   1604819 | 13697 | `					if( pInstr->iOp == PH7_OP_MEMBER && pInstr->iP1 == 1 && pInstr->p3 ){` |
|        11 | 13698 | `						void *pDynName = pInstr->p3;` |
|        11 | 13699 | `						(void)PH7_VmPopInstr(pGen->pVm);` |
|        11 | 13700 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,pDynName,0);` |
|        11 | 13701 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_MEMBER,1,PH7_MEMBER_METHOD,0,0);` |
|         5 | 13702 | `					}` |
|    802407 | 13703 | `				}` |
|   3086523 | 13704 | `			}` |
|  23176191 | 13705 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|         - | 13706 | `			ph7_expr_node **apNode;` |
|         - | 13707 | `			sxi32 n;` |
|   2872217 | 13708 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|         - | 13709 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|         - | 13710 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 13711 | `			/* Recurse and generate bytecodes for array index */` |
|   2872217 | 13712 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   5522475 | 13713 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   2650263 | 13714 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   2650263 | 13715 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   2650263 | 13716 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 13717 | `					return rc;` |
|         - | 13718 | `				}` |
|         - | 13719 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   2650263 | 13720 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   1325134 | 13721 | `			}` |
|   2872217 | 13722 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   2650263 | 13723 | `				iP1 = 1; /* Node have an index associated with it */` |
|   1325129 | 13724 | `			}` |
|   2872217 | 13725 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|         - | 13726 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|    351723 | 13727 | `				iP2 = 4;` |
|   2696358 | 13728 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|         - | 13729 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|         - | 13730 | `				 * so the trailing unset() builtin can drop the slot. */` |
|     23001 | 13731 | `				iP2 = 5;` |
|   2509001 | 13732 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|         - | 13733 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|         - | 13734 | `				 * short-circuit on missing keys without invoking offsetGet` |
|         - | 13735 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|        31 | 13736 | `				iP2 = 6;` |
|   2497490 | 13737 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|         - | 13738 | `				/* Create an empty entry when the desired index is not found */` |
|    531921 | 13739 | `				iP2 = 1;` |
|    265963 | 13740 | `			}` |
|  18653567 | 13741 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         - | 13742 | `			/* POP the left node */` |
|         5 | 13743 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 | 13744 | `		}` |
|  13131352 | 13745 | `	}` |
|  26285619 | 13746 | `	rc = SXRET_OK;` |
|  26285619 | 13747 | `	nJmpIdx = 0;` |
|         - | 13748 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|         - | 13749 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|         - | 13750 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  26285619 | 13751 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|    390597 | 13752 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    390597 | 13753 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    390597 | 13754 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    390597 | 13755 | `			int isSpecial = 0;` |
|    390597 | 13756 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|    344741 | 13757 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|    344741 | 13758 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|    344736 | 13759 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    314100 | 13760 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    170422 | 13761 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|    103297 | 13762 | `					isSpecial = 1;` |
|     51646 | 13763 | `				}` |
|    183832 | 13764 | `			}` |
|    413525 | 13765 | `			pInstr->iP1 = 0;` |
|         - | 13766 | ``			/* A leading `\` (NSSEP) makes the class name ABSOLUTE: it names the`` |
|         - | 13767 | `			 * global class, so it must NOT be re-qualified with the current namespace.` |
|         - | 13768 | ``			 * `\Closure::bind(...)` inside `namespace X` is Closure, not X\Closure.`` |
|         - | 13769 | ``			 * (Multi-component `\A\B::m` already resolves absolutely because its`` |
|         - | 13770 | ``			 * literal keeps a backslash; the single-component `\Closure` lost it.) */`` |
|         - | 13771 | `			{` |
|    597357 | 13772 | `				int bAbsolute = (pNode->pLeft && pNode->pLeft->pStart` |
|    551496 | 13773 | `					&& (pNode->pLeft->pStart->nType & PH7_TK_NSSEP));` |
|    367669 | 13774 | `				if( !isSpecial && !bAbsolute ){` |
|    264359 | 13775 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    132177 | 13776 | `				}` |
|         - | 13777 | `			}` |
|         - | 13778 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|         - | 13779 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|    367669 | 13780 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    264377 | 13781 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    264377 | 13782 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|        68 | 13783 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|        70 | 13784 | `					return SXRET_OK;` |
|         - | 13785 | `				}` |
|    132153 | 13786 | `			}` |
|    183799 | 13787 | `		}` |
|    229628 | 13788 | `	}` |
|         - | 13789 | `	/* Generate code for the right tree */` |
|  26262643 | 13790 | `	if( pNode->pRight ){` |
|  15076535 | 13791 | `		if( iVmOp == PH7_OP_LAND ){` |
|         - | 13792 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    413093 | 13793 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  14869991 | 13794 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|         - | 13795 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    286671 | 13796 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  14520114 | 13797 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|         - | 13798 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     57463 | 13799 | `			iVmOp = 0; /* No binary operator to emit */` |
|     57463 | 13800 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  14348104 | 13801 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         - | 13802 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|         - | 13803 | `			 * the entire containing postfix chain to null. The jump target is` |
|         - | 13804 | `			 * patched later by the innermost non-chain ancestor (or by` |
|         - | 13805 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|         - | 13806 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|         - | 13807 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|       108 | 13808 | `			sxu32 nNsJmp = 0;` |
|       108 | 13809 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|       108 | 13810 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  14319271 | 13811 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|         - | 13812 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|         - | 13813 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|         - | 13814 | `			 * auto-created — PHP auto-vivifies on write. */` |
|   4810073 | 13815 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|   2405034 | 13816 | `		}` |
|  15076535 | 13817 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  15076535 | 13818 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  15076535 | 13819 | `		if( !bIsChainOp ){` |
|         - | 13820 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|         - | 13821 | `			 * operator instruction is emitted. */` |
|  10130087 | 13822 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|   5065041 | 13823 | `		}` |
|  15076535 | 13824 | `		if( iVmOp == PH7_OP_STORE ){` |
|   4374251 | 13825 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|   4374214 | 13826 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|         - | 13827 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|         - | 13828 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|         - | 13829 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|         - | 13830 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|         - | 13831 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|         - | 13832 | `				 */` |
|        91 | 13833 | `				iVmOp = 0;` |
|   4374208 | 13834 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|   4374165 | 13835 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 13836 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|    779997 | 13837 | `					iP2 = 1;` |
|    390001 | 13838 | `				}else{` |
|   3594173 | 13839 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 13840 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|    512723 | 13841 | `						iVmOp = PH7_OP_STORE_IDX;` |
|    512723 | 13842 | `						iP1 = pInstr->iP1;` |
|    256364 | 13843 | `					}else{` |
|   3081455 | 13844 | `						p3 = pInstr->p3;` |
|         - | 13845 | `					}` |
|         - | 13846 | `					/* POP the last dynamic load instruction */` |
|   3594173 | 13847 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - | 13848 | `				}` |
|   2187085 | 13849 | `			}` |
|  12889412 | 13850 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|        63 | 13851 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|        63 | 13852 | `			if( pInstr ){` |
|        63 | 13853 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 13854 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|         - | 13855 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|         - | 13856 | `					 */` |
|        19 | 13857 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|        19 | 13858 | `					iP1 = pInstr->iP1;` |
|        19 | 13859 | `					iP2 = pInstr->iP2;` |
|        19 | 13860 | `					p3  = pInstr->p3;` |
|        10 | 13861 | `				}else{` |
|        45 | 13862 | `					p3 = pInstr->p3;` |
|         - | 13863 | `				}` |
|        30 | 13864 | `			}` |
|        30 | 13865 | `		}` |
|   7538265 | 13866 | `	}` |
|  26262638 | 13867 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|    376044 | 13868 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|         - | 13869 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|         - | 13870 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|        32 | 13871 | `		iVmOp = 0;` |
|        14 | 13872 | `	}` |
|  26262643 | 13873 | `	if( iVmOp > 0 ){` |
|  26205067 | 13874 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    195325 | 13875 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|         - | 13876 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|     15317 | 13877 | `				iP1 = 1;` |
|      7661 | 13878 | `			}` |
|  26107407 | 13879 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|         - | 13880 | `			/* Namespace-qualify the class name for NEW */ {` |
|    751715 | 13881 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|    751715 | 13882 | `				VmInstr *pCallInstr = 0;` |
|    751715 | 13883 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|    751403 | 13884 | `					pCallInstr = pPeek;` |
|    751403 | 13885 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    375699 | 13886 | `				}` |
|    751715 | 13887 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|    736431 | 13888 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 13889 | `					sxu32 nLitForClass;` |
|    736431 | 13890 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|         - | 13891 | `					/* If the CALL handler qualified the name with FUNCTION` |
|         - | 13892 | `					 * imports, recover the original literal (recorded in the` |
|         - | 13893 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|         - | 13894 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|         - | 13895 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|         - | 13896 | `					 * with class imports. */` |
|    736431 | 13897 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|        53 | 13898 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|        29 | 13899 | `					}else{` |
|    736383 | 13900 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|         - | 13901 | `					}` |
|    736431 | 13902 | `					pPeek->iP1 = 0;` |
|    736431 | 13903 | `					if( !bAbsolute ){` |
|         - | 13904 | `						/* self/static/parent are resolved at runtime against the` |
|         - | 13905 | `						 * current class — never namespace-qualify them (else` |
|         - | 13906 | ``						 * `new self` in namespace N becomes "N\self"). Mirrors the`` |
|         - | 13907 | `						 * instanceof (IS_A) guard below. */` |
|    732583 | 13908 | `						ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nLitForClass);` |
|    732583 | 13909 | `						int isSpecialNew = 0;` |
|    732583 | 13910 | `						if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    732583 | 13911 | `							const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    732583 | 13912 | `							sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    732578 | 13913 | `							if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    732623 | 13914 | `								(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    366335 | 13915 | `								(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        31 | 13916 | `								isSpecialNew = 1;` |
|        15 | 13917 | `							}` |
|    366289 | 13918 | `						}` |
|    732583 | 13919 | `						if( isSpecialNew ){` |
|        31 | 13920 | `							pPeek->iP2 = (sxi32)nLitForClass;` |
|        16 | 13921 | `						}else{` |
|    732553 | 13922 | `							pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|         - | 13923 | `						}` |
|    366294 | 13924 | `					}else{` |
|      3853 | 13925 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|         - | 13926 | `					}` |
|    368213 | 13927 | `				}` |
|         - | 13928 | `			}` |
|    751715 | 13929 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    751715 | 13930 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|         - | 13931 | `				VmInstr *pPrev;` |
|    751403 | 13932 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|    751403 | 13933 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|         - | 13934 | `					/* Pop the call instruction, preserve named-arg map and` |
|         - | 13935 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|         - | 13936 | `					 * accumulator exactly like OP_CALL would have). */` |
|    751403 | 13937 | `					iP1 = pInstr->iP1;` |
|    751403 | 13938 | `					iP2 = pInstr->iP2;` |
|    751403 | 13939 | `					if( pInstr->p3 ){` |
|        63 | 13940 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|        29 | 13941 | `					}` |
|    751403 | 13942 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    375699 | 13943 | `				}` |
|    375704 | 13944 | `			}` |
|  25633892 | 13945 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|         - | 13946 | `			/* instanceof: right operand is a class name, not a constant.` |
|         - | 13947 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     76673 | 13948 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     76673 | 13949 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     76673 | 13950 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     76673 | 13951 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     76673 | 13952 | `				int isSpecialIs = 0;` |
|     76673 | 13953 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     76673 | 13954 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     76673 | 13955 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     76668 | 13956 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     76671 | 13957 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     38334 | 13958 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        12 | 13959 | `						isSpecialIs = 1;` |
|         5 | 13960 | `					}` |
|     38334 | 13961 | `				}` |
|     76673 | 13962 | `				pInstr->iP1 = 0;` |
|     76673 | 13963 | `				if( !isSpecialIs && !bAbsolute ){` |
|     76653 | 13964 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     38324 | 13965 | `				}` |
|     38339 | 13966 | `			}` |
|  25219703 | 13967 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|         - | 13968 | `			/* Prevent constant expansion for member/property names.` |
|         - | 13969 | `			 * The right child (member name) was just compiled — its LOADC` |
|         - | 13970 | `			 * should not trigger constant lookup. */` |
|   4946453 | 13971 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   4946453 | 13972 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   4713341 | 13973 | `				pInstr->iP1 = 0;` |
|   2356668 | 13974 | `			}` |
|   4946453 | 13975 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|         - | 13976 | `				/* Static member access,remember that */` |
|    367621 | 13977 | `				iP1 = 1;` |
|    367621 | 13978 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    367621 | 13979 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|    229281 | 13980 | `					p3 = pInstr->p3;` |
|    229281 | 13981 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    114638 | 13982 | `				}` |
|    183808 | 13983 | `			}` |
|         - | 13984 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|         - | 13985 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|         - | 13986 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|         - | 13987 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|   4946453 | 13988 | `			if( iP2 == PH7_MEMBER_READ ){` |
|   4946453 | 13989 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        42 | 13990 | `					iP2 = PH7_MEMBER_UNSET;` |
|   4946433 | 13991 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|     61223 | 13992 | `					iP2 = PH7_MEMBER_ISSET;` |
|   4915804 | 13993 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        17 | 13994 | `					iP2 = PH7_MEMBER_EMPTY;` |
|   4885187 | 13995 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|         - | 13996 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|    948263 | 13997 | `					iP2 = PH7_MEMBER_WRITE;` |
|    474129 | 13998 | `				}` |
|   2473224 | 13999 | `			}` |
|   2473224 | 14000 | `		}` |
|         - | 14001 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|         - | 14002 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|         - | 14003 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|         - | 14004 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|         - | 14005 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
|  26205067 | 14006 | `		if( bFcc ){` |
|        81 | 14007 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|        81 | 14008 | `			iP2 = 0;` |
|        81 | 14009 | `			p3 = 0;` |
|        81 | 14010 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|        81 | 14011 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 14012 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|         - | 14013 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|         - | 14014 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|         - | 14015 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|        37 | 14016 | `				void *pMemberName = pInstr->p3;` |
|        37 | 14017 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|        37 | 14018 | `				if( pMemberName ){` |
|       ! 0 | 14019 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|       ! 0 | 14020 | `				}` |
|        37 | 14021 | `				iP1 = 2;` |
|        19 | 14022 | `			}else{` |
|        45 | 14023 | `				iP1 = 1;` |
|         - | 14024 | `			}` |
|        40 | 14025 | `		}` |
|         - | 14026 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|         - | 14027 | `		 * This is the primary emit path for user-visible calls. */` |
|  26205067 | 14028 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|   6924671 | 14029 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|   3462333 | 14030 | `		}` |
|         - | 14031 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  26205067 | 14032 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  13102531 | 14033 | `	}` |
|  26262643 | 14034 | `	if( nJmpIdx > 0 ){` |
|         - | 14035 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|    757217 | 14036 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|    757217 | 14037 | `		if( pInstr ){` |
|    757217 | 14038 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    378606 | 14039 | `		}` |
|    378606 | 14040 | `	}` |
|  26262643 | 14041 | `	return rc;` |
|  33693816 | 14042 | `}` |
|         - | 14043 | `/*` |
|         - | 14044 | ` * Compile a PHP expression.` |
|         - | 14045 | ` * According to the PHP language reference manual:` |
|         - | 14046 | ` *  Expressions are the most important building stones of PHP.` |
|         - | 14047 | ` *  In PHP, almost anything you write is an expression.` |
|         - | 14048 | ` *  The simplest yet most accurate way to define an expression` |
|         - | 14049 | ` *  is "anything that has a value".` |
|         - | 14050 | ` * If something goes wrong while compiling the expression,this` |
|         - | 14051 | ` * function takes care of generating the appropriate error` |
|         - | 14052 | ` * message.` |
|         - | 14053 | ` */` |
|         - | 14054 | `/*` |
|         - | 14055 | ` * Does this expression tree contain a comma OPERATOR node?` |
|         - | 14056 | ` *` |
|         - | 14057 | `` * PH7 shipped `,` as a lowest-precedence binary operator (IMP-0139-COMMA), so`` |
|         - | 14058 | `` * `(1, 2)` and `$x = (f(), $y)` compile and evaluate to the right operand.`` |
|         - | 14059 | ` * php 8 has no comma operator: its grammar only allows comma-separated` |
|         - | 14060 | ` * expression LISTS inside for(...) clauses (call arguments, array literals and` |
|         - | 14061 | ` * list() are split by the parser, never by this node). Accepting it changes the` |
|         - | 14062 | ` * meaning of source php rejects, which §10 classes as a bug — so every context` |
|         - | 14063 | ` * except for() now reports php's parse error.` |
|         - | 14064 | ` */` |
| 223388180 | 14065 | `static int GenStateTreeHasComma(ph7_expr_node *pNode)` |
|         5 | 14066 | `{` |
|         - | 14067 | `	ph7_expr_node **apArg;` |
|         - | 14068 | `	sxu32 n;` |
| 223388185 | 14069 | `	if( pNode == 0 ){` |
| 157032983 | 14070 | `		return 0;` |
|         - | 14071 | `	}` |
|  66355207 | 14072 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         6 | 14073 | `		return 1;` |
|         - | 14074 | `	}` |
|  66355198 | 14075 | `	if( GenStateTreeHasComma(pNode->pLeft) \|\| GenStateTreeHasComma(pNode->pRight)` |
|  66355199 | 14076 | `	 \|\| GenStateTreeHasComma(pNode->pCond) ){` |
|         6 | 14077 | `		return 1;` |
|         - | 14078 | `	}` |
|  66355199 | 14079 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  75788633 | 14080 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; n++ ){` |
|   9433439 | 14081 | `		if( GenStateTreeHasComma(apArg[n]) ){` |
|       ! 0 | 14082 | `			return 1;` |
|         - | 14083 | `		}` |
|   4716722 | 14084 | `	}` |
|  66355199 | 14085 | `	return 0;` |
| 111694095 | 14086 | `}` |
|  15245048 | 14087 | `static sxi32 PH7_CompileExpr(` |
|         - | 14088 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 14089 | `	sxi32 iFlags,        /* Control flags */` |
|         - | 14090 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|         - | 14091 | `	)` |
|         5 | 14092 | `{` |
|         - | 14093 | `	ph7_expr_node *pRoot;` |
|         - | 14094 | `	SySet sExprNode;` |
|         - | 14095 | `	SyToken *pEnd;` |
|         - | 14096 | `	sxi32 nExpr;` |
|         - | 14097 | `	sxi32 iNest;` |
|         - | 14098 | `	sxi32 rc;` |
|         - | 14099 | `	sxu32 nNullsafeBase;` |
|         - | 14100 | `	/* Initialize worker variables */` |
|  15245053 | 14101 | `	nExpr = 0;` |
|  15245053 | 14102 | `	pRoot = 0;` |
|         - | 14103 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|         - | 14104 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  15245053 | 14105 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  15245053 | 14106 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  15245053 | 14107 | `	SySetAlloc(&sExprNode,0x10);` |
|  15245053 | 14108 | `	rc = SXRET_OK;` |
|         - | 14109 | `	/* Delimit the expression */` |
|  15245053 | 14110 | `	pEnd = pGen->pIn;` |
|  15245053 | 14111 | `	iNest = 0;` |
| 119183991 | 14112 | `	while( pEnd < pGen->pEnd ){` |
| 113266561 | 14113 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 14114 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      4645 | 14115 | `			iNest++;` |
| 113264241 | 14116 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      4653 | 14117 | `			iNest--;` |
| 113259597 | 14118 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|   9328481 | 14119 | `			if( iNest <= 0 ){` |
|   9327623 | 14120 | `				break;` |
|         - | 14121 | `			}` |
|       429 | 14122 | `		}` |
| 103938943 | 14123 | `		pEnd++;` |
|         5 | 14124 | `	}` |
|  15245053 | 14125 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|    642803 | 14126 | `		SyToken *pEnd2 = pGen->pIn;` |
|    642803 | 14127 | `		iNest = 0;` |
|         - | 14128 | `		/* Stop at the first comma */` |
|   1412945 | 14129 | `		while( pEnd2 < pEnd ){` |
|    770149 | 14130 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|     42137 | 14131 | `				iNest++;` |
|    749083 | 14132 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|     42137 | 14133 | `				iNest--;` |
|    706951 | 14134 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      6061 | 14135 | `				if( iNest <= 0 ){` |
|         3 | 14136 | `					break;` |
|         - | 14137 | `				}` |
|      3027 | 14138 | `			}` |
|    770147 | 14139 | `			pEnd2++;` |
|         5 | 14140 | `		}` |
|    642803 | 14141 | `		if( pEnd2 <pEnd ){` |
|         3 | 14142 | `			pEnd = pEnd2;` |
|         1 | 14143 | `		}` |
|    321399 | 14144 | `	}` |
|  15245053 | 14145 | `	if( pEnd > pGen->pIn ){` |
|  15222125 | 14146 | `		SyToken *pTmp = pGen->pEnd;` |
|         - | 14147 | `		/* Swap delimiter */` |
|  15222125 | 14148 | `		pGen->pEnd = pEnd;` |
|         - | 14149 | `		/* Try to get an expression tree */` |
|  15222125 | 14150 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  15222120 | 14151 | `		if( rc == SXRET_OK && pRoot && pGen->nCommaExprOk < 1` |
|  15055553 | 14152 | `		 && GenStateTreeHasComma(pRoot) ){` |
|         - | 14153 | `			/* php has no comma operator outside a for() clause */` |
|         6 | 14154 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pRoot->pStart->nLine,` |
|         - | 14155 | `				"syntax error, unexpected token \",\"");` |
|         6 | 14156 | `			pGen->pEnd = pTmp;` |
|         6 | 14157 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14158 | `				SySetRelease(&sExprNode);` |
|       ! 0 | 14159 | `				return SXERR_ABORT;` |
|         - | 14160 | `			}` |
|         6 | 14161 | `			pGen->pIn = pEnd;` |
|         6 | 14162 | `			SySetRelease(&sExprNode);` |
|         6 | 14163 | `			SySetTruncate(&pGen->aNullsafeJmp,nNullsafeBase);` |
|         6 | 14164 | `			return SXRET_OK;` |
|         - | 14165 | `		}` |
|  15222121 | 14166 | `		if( rc == SXRET_OK && pRoot ){` |
|  15221937 | 14167 | `			rc = SXRET_OK;` |
|  15221937 | 14168 | `			if( xTreeValidator ){` |
|         - | 14169 | `				/* Call the upper layer validator callback */` |
|    975941 | 14170 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    487968 | 14171 | `			}` |
|  15221937 | 14172 | `			if( rc != SXERR_ABORT ){` |
|         - | 14173 | `				/* Generate code for the given tree */` |
|  15221937 | 14174 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|         - | 14175 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|         - | 14176 | `				 * expression so they short-circuit to its end. */` |
|  15221937 | 14177 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|   7610966 | 14178 | `			}` |
|  15221937 | 14179 | `			nExpr = 1;` |
|   7610966 | 14180 | `		}` |
|         - | 14181 | `		/* Release the whole tree */` |
|  15222121 | 14182 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|         - | 14183 | `		/* Synchronize token stream */` |
|  15222121 | 14184 | `		pGen->pEnd = pTmp;` |
|  15222121 | 14185 | `		pGen->pIn  = pEnd;` |
|  15222121 | 14186 | `		if( rc == SXERR_ABORT ){` |
|        12 | 14187 | `			SySetRelease(&sExprNode);` |
|        12 | 14188 | `			return SXERR_ABORT;` |
|         - | 14189 | `		}` |
|   7611053 | 14190 | `	}` |
|  15245039 | 14191 | `	SySetRelease(&sExprNode);` |
|  15245039 | 14192 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|   7622529 | 14193 | `}` |
|         - | 14194 | `/*` |
|         - | 14195 | ` * Return a pointer to the node construct handler associated` |
|         - | 14196 | ` * with a given node type [i.e: string,integer,float,...].` |
|         - | 14197 | ` */` |
|   8777582 | 14198 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|         5 | 14199 | `{` |
|   8777587 | 14200 | `	if( nNodeType & PH7_TK_NUM ){` |
|         - | 14201 | `		/* Numeric literal: Either real or integer */` |
|   3559953 | 14202 | `		return PH7_CompileNumLiteral;` |
|   5217639 | 14203 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|         - | 14204 | `		/* Double quoted string */` |
|    120169 | 14205 | `		return PH7_CompileString;` |
|   5097475 | 14206 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|         - | 14207 | `		/* Single quoted string */` |
|   5097355 | 14208 | `		return PH7_CompileSimpleString;` |
|       124 | 14209 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|         - | 14210 | `		/* Heredoc */` |
|        70 | 14211 | `		return PH7_CompileHereDoc;` |
|        58 | 14212 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|         - | 14213 | `		/* Nowdoc */` |
|        52 | 14214 | `		return PH7_CompileNowDoc;` |
|         8 | 14215 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|         - | 14216 | `		/* Backtick quoted string */` |
|         6 | 14217 | `		return PH7_CompileBacktic;` |
|         - | 14218 | `	}` |
|         3 | 14219 | `	return 0;` |
|   4388796 | 14220 | `}` |
|         - | 14221 | `/*` |
|         - | 14222 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|         - | 14223 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|         - | 14224 | ` * in write context" parse error.` |
|         - | 14225 | ` */` |
|     23038 | 14226 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|         5 | 14227 | `{` |
|         - | 14228 | `	sxi32 rc;` |
|     23043 | 14229 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|     23041 | 14230 | `		return SXRET_OK;` |
|         - | 14231 | `	}` |
|         5 | 14232 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|         2 | 14233 | `		pNode ? pNode->pStart->nLine : 1,` |
|         - | 14234 | `		"Can't use nullsafe operator in write context");` |
|         3 | 14235 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     11524 | 14236 | `}` |
|         - | 14237 | `/*` |
|         - | 14238 | ` * Compile an unset() statement.` |
|         - | 14239 | ` * unset($var, $arr[$key], ...);` |
|         - | 14240 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|         - | 14241 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|         - | 14242 | ` * parent array before extracting the element to unset.` |
|         - | 14243 | ` */` |
|     25890 | 14244 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|         5 | 14245 | `{` |
|     25895 | 14246 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     25895 | 14247 | `	sxu32 nIdx = 0;` |
|         - | 14248 | `	SyString sName;` |
|         - | 14249 | `	sxi32 rc;` |
|         - | 14250 | `	/* Jump the 'unset' keyword */` |
|     25895 | 14251 | `	pGen->pIn++;` |
|         - | 14252 | `	/* Save delimiter */` |
|     25895 | 14253 | `	pTmp = pGen->pEnd;` |
|         - | 14254 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     25895 | 14255 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     25895 | 14256 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 14257 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|         - | 14258 | `		SyToken *pClose;` |
|     25895 | 14259 | `		pGen->pIn++;   /* Skip '(' */` |
|     25895 | 14260 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     25895 | 14261 | `		pEnd = pClose; /* Stop at ')' */` |
|     12945 | 14262 | `	}` |
|     25895 | 14263 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|         - | 14264 | `	/* Resolve the 'unset' builtin name once */` |
|     25895 | 14265 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      3825 | 14266 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3825 | 14267 | `		if( pObj == 0 ){` |
|       ! 0 | 14268 | `			return SXERR_ABORT;` |
|         - | 14269 | `		}` |
|      3825 | 14270 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      3825 | 14271 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1910 | 14272 | `	}` |
|         - | 14273 | `	/* Compile each comma-separated argument */` |
|     56011 | 14274 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     30121 | 14275 | `		if( pGen->pIn < pNext ){` |
|         - | 14276 | `			/*` |
|         - | 14277 | ``			 * A PLAIN variable (`unset($x)`, exactly two tokens: '$' and the name) drops a`` |
|         - | 14278 | `			 * single NAME binding, which the unset() builtin cannot express — all it ever` |
|         - | 14279 | `			 * receives is the value's slot index, and unsetting the slot destroys whatever` |
|         - | 14280 | `			 * else aliases it. Emit OP_UNSET_VAR with the name instead. Subscripts and` |
|         - | 14281 | ``			 * properties (`unset($a[k])`, `unset($o->p)`) keep the existing path, which`` |
|         - | 14282 | `			 * already removes just the element/property.` |
|         - | 14283 | `			 */` |
|     30116 | 14284 | `			if( &pGen->pIn[2] == pNext` |
|     18597 | 14285 | `				&& (pGen->pIn[0].nType & PH7_TK_DOLLAR)` |
|      7083 | 14286 | `				&& (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         - | 14287 | `				SyString *pVarName;` |
|     10619 | 14288 | `				char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      7076 | 14289 | `					pGen->pIn[1].sData.zString,pGen->pIn[1].sData.nByte);` |
|      7081 | 14290 | `				pVarName = (SyString *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(SyString));` |
|      7081 | 14291 | `				if( zDup == 0 \|\| pVarName == 0 ){` |
|       ! 0 | 14292 | `					PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - | 14293 | `						"Fatal, PH7 is running out of memory");` |
|       ! 0 | 14294 | `					return SXERR_ABORT;` |
|         - | 14295 | `				}` |
|      7081 | 14296 | `				SyStringInitFromBuf(pVarName,zDup,pGen->pIn[1].sData.nByte);` |
|      7081 | 14297 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,pVarName,0);` |
|      7081 | 14298 | `				pGen->pIn = pNext;` |
|      7081 | 14299 | `				if( pGen->pIn < pEnd ){` |
|      4227 | 14300 | `					pGen->pIn++; /* Jump the trailing comma */` |
|      2111 | 14301 | `				}` |
|      7081 | 14302 | `				continue;` |
|         - | 14303 | `			}` |
|     23045 | 14304 | `			pGen->pEnd = pNext;` |
|     23045 | 14305 | `			rc = PH7_CompileExpr(&(*pGen),` |
|         - | 14306 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|         - | 14307 | `				GenStateUnsetValidator);` |
|     23045 | 14308 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14309 | `				return SXERR_ABORT;` |
|         - | 14310 | `			}` |
|     23045 | 14311 | `			if( rc != SXERR_EMPTY ){` |
|         - | 14312 | `				/* Emit call for this single argument */` |
|     23043 | 14313 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     23043 | 14314 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|     23043 | 14315 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     11519 | 14316 | `			}` |
|     11520 | 14317 | `		}` |
|         - | 14318 | `		/* Jump trailing commas */` |
|     23051 | 14319 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|         7 | 14320 | `			pNext++;` |
|         1 | 14321 | `		}` |
|     23045 | 14322 | `		pGen->pIn = pNext;` |
|         5 | 14323 | `	}` |
|         - | 14324 | `	/* Skip past the closing ')' if present */` |
|     25895 | 14325 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     25895 | 14326 | `		pGen->pIn++;` |
|     12945 | 14327 | `	}` |
|         - | 14328 | `	/* Restore token stream */` |
|     25895 | 14329 | `	pGen->pEnd = pTmp;` |
|     25895 | 14330 | `	return SXRET_OK;` |
|     12950 | 14331 | `}` |
|         - | 14332 | `/*` |
|         - | 14333 | ` * PHP Language construct table.` |
|         - | 14334 | ` */` |
|         - | 14335 | `static const LangConstruct aLangConstruct[] = {` |
|         - | 14336 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|         - | 14337 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|         - | 14338 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|         - | 14339 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|         - | 14340 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|         - | 14341 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|         - | 14342 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|         - | 14343 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|         - | 14344 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|         - | 14345 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|         - | 14346 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|         - | 14347 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|         - | 14348 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|         - | 14349 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|         - | 14350 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|         - | 14351 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|         - | 14352 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|         - | 14353 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|         - | 14354 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|         - | 14355 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|         - | 14356 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|         - | 14357 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|         - | 14358 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|         - | 14359 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|         - | 14360 | `};` |
|         - | 14361 | `/*` |
|         - | 14362 | ` * Return a pointer to the statement handler routine associated` |
|         - | 14363 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|         - | 14364 | ` */` |
|   7382628 | 14365 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|         - | 14366 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|         - | 14367 | `	SyToken *pLookahed  /* Look-ahead token */` |
|         - | 14368 | `	)` |
|         5 | 14369 | `{` |
|   7382633 | 14370 | `	sxu32 n = 0;` |
|  29192642 | 14371 | `	for(;;){` |
|  58385289 | 14372 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|    430209 | 14373 | `			break;` |
|         - | 14374 | `		}` |
|  57955085 | 14375 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|   6952429 | 14376 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|       ! 0 | 14377 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|       ! 0 | 14378 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|         - | 14379 | `					/* 'static' (class context),return null */` |
|       ! 0 | 14380 | `					return 0;` |
|         - | 14381 | `				}` |
|       ! 0 | 14382 | `			}` |
|   6952424 | 14383 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|     11474 | 14384 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|      5744 | 14385 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|         - | 14386 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|         3 | 14387 | `				return 0;` |
|         - | 14388 | `			}` |
|         - | 14389 | `			/* Return a pointer to the handler.` |
|         - | 14390 | `			*/` |
|   6952427 | 14391 | `			return aLangConstruct[n].xConstruct;` |
|         - | 14392 | `		}` |
|  51002661 | 14393 | `		n++;` |
|         5 | 14394 | `	}` |
|    430209 | 14395 | `	if( pLookahed ){` |
|    430209 | 14396 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|     68875 | 14397 | `			return PH7_CompileClassInterface;` |
|    361339 | 14398 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|    314941 | 14399 | `			return PH7_CompileClass;` |
|     46403 | 14400 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      7723 | 14401 | `			return PH7_CompileTrait;` |
|         - | 14402 | `		}` |
|         - | 14403 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|         - | 14404 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|         - | 14405 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|         - | 14406 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     19340 | 14407 | `	}` |
|         - | 14408 | `	/* Not a language construct */` |
|     38685 | 14409 | `	return 0;` |
|   3691319 | 14410 | `}` |
|         - | 14411 | `/*` |
|         - | 14412 | ` * Check if the given keyword is in fact a PHP language construct.` |
|         - | 14413 | ` * Return TRUE on success. FALSE otheriwse.` |
|         - | 14414 | ` */` |
|     38682 | 14415 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|         5 | 14416 | `{` |
|         - | 14417 | `	int rc;` |
|     38687 | 14418 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     38687 | 14419 | `	if( rc == FALSE ){` |
|     38576 | 14420 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     15648 | 14421 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|         - | 14422 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|         - | 14423 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|         - | 14424 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|         - | 14425 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|         - | 14426 | `			*/` |
|         - | 14427 | `			){` |
|     38573 | 14428 | `				rc = TRUE;` |
|     19284 | 14429 | `		}` |
|     19288 | 14430 | `	}` |
|     38687 | 14431 | `	return rc;` |
|         5 | 14432 | `}` |
|         - | 14433 | `/*` |
|         - | 14434 | ` * Compile a PHP chunk.` |
|         - | 14435 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 14436 | ` * takes care of generating the appropriate error message.` |
|         - | 14437 | ` */` |
|         - | 14438 | `/*` |
|         - | 14439 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|         - | 14440 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|         - | 14441 | ` * the chunk token set it becomes the pending docblock. An existing` |
|         - | 14442 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|         - | 14443 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|         - | 14444 | ` * intervening non-declaration statements.` |
|         - | 14445 | ` */` |
|  15903616 | 14446 | `static void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|         5 | 14447 | `{` |
|  15903621 | 14448 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  15903621 | 14449 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  15903621 | 14450 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 14451 | `	sxu32 nIdx, n;` |
|  15903616 | 14452 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   3345449 | 14453 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|         - | 14454 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|         - | 14455 | `		 * indexes do not map to the sidecar */` |
|  12558179 | 14456 | `		return;` |
|         - | 14457 | `	}` |
|   3345447 | 14458 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|         - | 14459 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|         - | 14460 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|   3345447 | 14461 | `	SySetReset(&pGen->aPendingAttrs);` |
|  10037825 | 14462 | `	for( n = 0 ; n < nT ; n++ ){` |
|   6692383 | 14463 | `		if( aT[n].nTokIdx != nIdx ){` |
|   6684579 | 14464 | `			continue;` |
|         - | 14465 | `		}` |
|      7809 | 14466 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|        29 | 14467 | `			pGen->sPendingDoc = aT[n].sText;` |
|      7797 | 14468 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|      7785 | 14469 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|      3890 | 14470 | `		}` |
|      3907 | 14471 | `	}` |
|   7951813 | 14472 | `}` |
|         - | 14473 | `/*` |
|         - | 14474 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|         - | 14475 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|         - | 14476 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|         - | 14477 | ` */` |
|   4075318 | 14478 | `static void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|         5 | 14479 | `{` |
|         - | 14480 | `	char *zDup;` |
|   4075323 | 14481 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|   4075303 | 14482 | `		return;` |
|         - | 14483 | `	}` |
|        35 | 14484 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 | 14485 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|        25 | 14486 | `	if( zDup ){` |
|        25 | 14487 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|        10 | 14488 | `	}` |
|        25 | 14489 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|   2037664 | 14490 | `}` |
|         - | 14491 | `/*` |
|         - | 14492 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|         - | 14493 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|         - | 14494 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|         - | 14495 | ` * names may point into the token text, which must outlive the raw script` |
|         - | 14496 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|         - | 14497 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|         - | 14498 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|         - | 14499 | ` */` |
|      7792 | 14500 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|         5 | 14501 | `{` |
|         - | 14502 | `	SySet *pToken;` |
|         - | 14503 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|         - | 14504 | `	char *zSpan;` |
|      7797 | 14505 | `	sxi32 rc = SXRET_OK;` |
|      7797 | 14506 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|       ! 0 | 14507 | `		return SXRET_OK;` |
|         - | 14508 | `	}` |
|     11693 | 14509 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      3896 | 14510 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|      7797 | 14511 | `	if( zSpan == 0 ){` |
|       ! 0 | 14512 | `		return SXRET_OK;` |
|         - | 14513 | `	}` |
|         - | 14514 | `	/* The token set must outlive compilation too: interned operands may` |
|         - | 14515 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|         - | 14516 | `	 * the number of attribute declarations in the program. */` |
|      7797 | 14517 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      7797 | 14518 | `	if( pToken == 0 ){` |
|       ! 0 | 14519 | `		return SXRET_OK;` |
|         - | 14520 | `	}` |
|      7797 | 14521 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      7797 | 14522 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|      7797 | 14523 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|      7797 | 14524 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|      7797 | 14525 | `	pSavedIn = pGen->pIn;` |
|      7797 | 14526 | `	pSavedEnd = pGen->pEnd;` |
|      7801 | 14527 | `	while( pIn < pEnd ){` |
|         - | 14528 | `		ph7_attribute sAttr;` |
|         - | 14529 | `		SyBlob sFQN;` |
|      7801 | 14530 | `		int bAbsolute = 0;` |
|      7801 | 14531 | `		SyZero(&sAttr,sizeof(sAttr));` |
|      7801 | 14532 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|      7801 | 14533 | `		sAttr.nLine = pIn->nLine;` |
|      7801 | 14534 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|        75 | 14535 | `			bAbsolute = 1;` |
|        75 | 14536 | `			pIn++;` |
|        35 | 14537 | `		}` |
|      7801 | 14538 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7801 | 14539 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      7801 | 14540 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|      7801 | 14541 | `			pIn++;` |
|      7801 | 14542 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       ! 0 | 14543 | `				SyBlobAppend(&sFQN,"\\",1);` |
|       ! 0 | 14544 | `				pIn++;` |
|       ! 0 | 14545 | `				continue;` |
|         - | 14546 | `			}` |
|      7801 | 14547 | `			break;` |
|       ! 0 | 14548 | `		}` |
|      7801 | 14549 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|         - | 14550 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|         - | 14551 | `			 * this feature; never turn it into a new fatal) */` |
|       ! 0 | 14552 | `			SyBlobRelease(&sFQN);` |
|       ! 0 | 14553 | `			break;` |
|         - | 14554 | `		}` |
|         - | 14555 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|         - | 14556 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|         - | 14557 | `		{` |
|      7801 | 14558 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|      7801 | 14559 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|      7801 | 14560 | `			char *zDup = 0;` |
|      7801 | 14561 | `			if( !bAbsolute ){` |
|      7731 | 14562 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|      7731 | 14563 | `				if( pImp ){` |
|       ! 0 | 14564 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|       ! 0 | 14565 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|       ! 0 | 14566 | `					if( zDup ){` |
|       ! 0 | 14567 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|       ! 0 | 14568 | `					}` |
|      7731 | 14569 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - | 14570 | `					SyBlob sTmp;` |
|       ! 0 | 14571 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|       ! 0 | 14572 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       ! 0 | 14573 | `					SyBlobAppend(&sTmp,"\\",1);` |
|       ! 0 | 14574 | `					SyBlobAppend(&sTmp,zName,nName);` |
|       ! 0 | 14575 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       ! 0 | 14576 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|       ! 0 | 14577 | `					if( zDup ){` |
|       ! 0 | 14578 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|       ! 0 | 14579 | `					}` |
|       ! 0 | 14580 | `					SyBlobRelease(&sTmp);` |
|       ! 0 | 14581 | `				}` |
|      3863 | 14582 | `			}` |
|      7801 | 14583 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|      7801 | 14584 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|      7801 | 14585 | `				if( zDup ){` |
|      7801 | 14586 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|      3898 | 14587 | `				}` |
|      3898 | 14588 | `			}` |
|         - | 14589 | `		}` |
|      7801 | 14590 | `		SyBlobRelease(&sFQN);` |
|      7801 | 14591 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 14592 | `			SyToken *pArgsEnd;` |
|      7699 | 14593 | `			pIn++;` |
|      7699 | 14594 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|     15407 | 14595 | `			while( pIn < pArgsEnd ){` |
|      7713 | 14596 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|      7713 | 14597 | `				sxi32 iDepth = 0;` |
|         - | 14598 | `				ph7_attr_arg sArgRec;` |
|     76645 | 14599 | `				while( pArgStop < pArgsEnd ){` |
|     68953 | 14600 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        11 | 14601 | `						iDepth++;` |
|     68948 | 14602 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        11 | 14603 | `						iDepth--;` |
|     68938 | 14604 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|        17 | 14605 | `						break;` |
|         - | 14606 | `					}` |
|     68937 | 14607 | `					pArgStop++;` |
|         5 | 14608 | `				}` |
|      7713 | 14609 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|      7713 | 14610 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      7708 | 14611 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      7692 | 14612 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        28 | 14613 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|         9 | 14614 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|        19 | 14615 | `					if( zN ){` |
|        19 | 14616 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|         9 | 14617 | `					}` |
|        19 | 14618 | `					pArgStart += 2;` |
|         9 | 14619 | `				}` |
|      7713 | 14620 | `				if( pArgStart < pArgStop ){` |
|         - | 14621 | `					SySet *pInstrContainer;` |
|      7713 | 14622 | `					pGen->pIn = pArgStart;` |
|      7713 | 14623 | `					pGen->pEnd = pArgStop;` |
|      7713 | 14624 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7713 | 14625 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|      7713 | 14626 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7713 | 14627 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7713 | 14628 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7713 | 14629 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 14630 | `						pGen->pIn = pSavedIn;` |
|       ! 0 | 14631 | `						pGen->pEnd = pSavedEnd;` |
|       ! 0 | 14632 | `						return SXERR_ABORT;` |
|         - | 14633 | `					}` |
|      7713 | 14634 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|      3854 | 14635 | `				}` |
|      7713 | 14636 | `				pIn = pArgStop;` |
|      7713 | 14637 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 | 14638 | `					pIn++;` |
|         8 | 14639 | `				}` |
|         5 | 14640 | `			}` |
|      7699 | 14641 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|      3847 | 14642 | `		}` |
|      7801 | 14643 | `		SySetPut(pOut,(const void *)&sAttr);` |
|      7801 | 14644 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|         5 | 14645 | `			pIn++;` |
|         5 | 14646 | `			continue;` |
|         - | 14647 | `		}` |
|      7797 | 14648 | `		break;` |
|       ! 0 | 14649 | `	}` |
|      7797 | 14650 | `	pGen->pIn = pSavedIn;` |
|      7797 | 14651 | `	pGen->pEnd = pSavedEnd;` |
|      7797 | 14652 | `	return SXRET_OK;` |
|      3901 | 14653 | `}` |
|         - | 14654 | `/*` |
|         - | 14655 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|         - | 14656 | ` * every recorded group into pOut and clear the pending list.` |
|         - | 14657 | ` */` |
|   4075322 | 14658 | `static sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|         5 | 14659 | `{` |
|   4075327 | 14660 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|         - | 14661 | `	sxu32 n;` |
|         - | 14662 | `	sxi32 rc;` |
|   4083107 | 14663 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|      7785 | 14664 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|      7785 | 14665 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 14666 | `			return SXERR_ABORT;` |
|         - | 14667 | `		}` |
|      3895 | 14668 | `	}` |
|   4075327 | 14669 | `	SySetReset(&pGen->aPendingAttrs);` |
|   4075327 | 14670 | `	return SXRET_OK;` |
|   2037666 | 14671 | `}` |
|         - | 14672 | `/*` |
|         - | 14673 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|         - | 14674 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|         - | 14675 | ` * the main token stream, so the sidecar indexes map directly.` |
|         - | 14676 | ` */` |
|   2055382 | 14677 | `static sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|         5 | 14678 | `{` |
|   2055387 | 14679 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   2055387 | 14680 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   2055387 | 14681 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 14682 | `	sxu32 nIdx, n;` |
|         - | 14683 | `	sxi32 rc;` |
|   2055382 | 14684 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|    546599 | 14685 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   1508793 | 14686 | `		return SXRET_OK;` |
|         - | 14687 | `	}` |
|    546599 | 14688 | `	nIdx = (sxu32)(pTok - pBase);` |
|   1639785 | 14689 | `	for( n = 0 ; n < nT ; n++ ){` |
|   1093191 | 14690 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|        13 | 14691 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|        13 | 14692 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14693 | `				return SXERR_ABORT;` |
|         - | 14694 | `			}` |
|         6 | 14695 | `		}` |
|    546598 | 14696 | `	}` |
|    546599 | 14697 | `	return SXRET_OK;` |
|   1027696 | 14698 | `}` |
|  11854486 | 14699 | `static sxi32 GenStateCompileChunk(` |
|         - | 14700 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 14701 | `	sxi32 iFlags         /* Compile flags */` |
|         - | 14702 | `	)` |
|         5 | 14703 | `{` |
|         - | 14704 | `	ProcLangConstruct xCons;` |
|         - | 14705 | `	sxi32 rc;` |
|  11854491 | 14706 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|   6885699 | 14707 | `	for(;;){` |
|  12812947 | 14708 | `		int bStmtIsDeclare = 0;` |
|  12812947 | 14709 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 14710 | `			/* No more input to process */` |
|     67593 | 14711 | `			break;` |
|         - | 14712 | `		}` |
|         - | 14713 | `		/* Bind a directly-preceding docblock to this statement */` |
|  12745359 | 14714 | `		GenStateSetPendingDoc(&(*pGen));` |
|  12745359 | 14715 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - | 14716 | `			/* php: a statement-position attribute group must be followed by a` |
|         - | 14717 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|         - | 14718 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|         - | 14719 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|         - | 14720 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|      7703 | 14721 | `			int bAttrTarget = 0;` |
|      7698 | 14722 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|      3883 | 14723 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|      7645 | 14724 | `				bAttrTarget = 1;` |
|      3879 | 14725 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        59 | 14726 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        58 | 14727 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|        15 | 14728 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|         4 | 14729 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|         4 | 14730 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|         1 | 14731 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|        59 | 14732 | `					bAttrTarget = 1;` |
|        29 | 14733 | `				}` |
|        29 | 14734 | `			}` |
|      7703 | 14735 | `			if( !bAttrTarget ){` |
|       ! 0 | 14736 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 14737 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|       ! 0 | 14738 | `					&pGen->pIn->sData);` |
|       ! 0 | 14739 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 14740 | `					break;` |
|         - | 14741 | `				}` |
|       ! 0 | 14742 | `				SySetReset(&pGen->aPendingAttrs);` |
|       ! 0 | 14743 | `			}` |
|      3849 | 14744 | `		}` |
|         - | 14745 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|         - | 14746 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  12745359 | 14747 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   7417075 | 14748 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   7417075 | 14749 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|        47 | 14750 | `				bStmtIsDeclare = 1;` |
|        21 | 14751 | `			}` |
|   3708535 | 14752 | `		}` |
|  12745359 | 14753 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|         - | 14754 | `			/* Any non-declare top-level statement locks the strict_types` |
|         - | 14755 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|    958429 | 14756 | `			pGen->bStrictTypesLocked = 1;` |
|    479212 | 14757 | `		}` |
|  12745359 | 14758 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 14759 | `			/* Compile block */` |
|      3857 | 14760 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      3857 | 14761 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14762 | `				break;` |
|         - | 14763 | `			}` |
|      1931 | 14764 | `		}else{` |
|  12741507 | 14765 | `			xCons = 0;` |
|  12741507 | 14766 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|         - | 14767 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|         - | 14768 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|         - | 14769 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|     34473 | 14770 | `				xCons = PH7_CompileClassModifiers;` |
|  12724273 | 14771 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|         - | 14772 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|         - | 14773 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|      3853 | 14774 | `				xCons = PH7_CompileEnum;` |
|  12705115 | 14775 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   7382633 | 14776 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - | 14777 | `				/* Try to extract a language construct handler */` |
|   7382633 | 14778 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|   7382633 | 14779 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|        13 | 14780 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 14781 | `						"Syntax error: Unexpected keyword '%z'",` |
|         8 | 14782 | `						&pGen->pIn->sData);` |
|         9 | 14783 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 14784 | `						break;` |
|         - | 14785 | `					}` |
|         - | 14786 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|         - | 14787 | `					 * this erroneous statement.` |
|         - | 14788 | `					 */` |
|         9 | 14789 | `					xCons = PH7_ErrorRecover;` |
|         4 | 14790 | `				}` |
|   9011877 | 14791 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    406219 | 14792 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|         - | 14793 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|       117 | 14794 | `				xCons = PH7_CompileLabel;` |
|        56 | 14795 | `			}` |
|  12741507 | 14796 | `			if( xCons == 0 ){` |
|         - | 14797 | `				/* Assume an expression an try to compile it */` |
|   5359125 | 14798 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   5359125 | 14799 | `				if(  rc != SXERR_EMPTY ){` |
|         - | 14800 | `					/* Pop l-value */` |
|   5358975 | 14801 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   2679485 | 14802 | `				}` |
|   2679565 | 14803 | `			}else{` |
|         - | 14804 | `				/* Go compile the sucker */` |
|   7382387 | 14805 | `				rc = xCons(&(*pGen));` |
|         - | 14806 | `			}` |
|  12741507 | 14807 | `			if( rc == SXERR_ABORT ){` |
|         - | 14808 | `				/* Request to abort compilation */` |
|        12 | 14809 | `				break;` |
|         - | 14810 | `			}` |
|         - | 14811 | `		}` |
|         - | 14812 | `		/* Ignore trailing semi-colons ';' */` |
|  21818827 | 14813 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|   9073483 | 14814 | `			pGen->pIn++;` |
|         5 | 14815 | `		}` |
|  12745349 | 14816 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|         - | 14817 | `			/* Compile a single statement and return */` |
|  11786893 | 14818 | `			break;` |
|         - | 14819 | `		}` |
|         - | 14820 | `		/* LOOP ONE */` |
|         - | 14821 | `		/* LOOP TWO */` |
|         - | 14822 | `		/* LOOP THREE */` |
|         - | 14823 | `		/* LOOP FOUR */` |
|         5 | 14824 | `	}` |
|         - | 14825 | `	/* Return compilation status */` |
|  11854491 | 14826 | `	return rc;` |
|         5 | 14827 | `}` |
|         - | 14828 | `/*` |
|         - | 14829 | ` * Compile a Raw PHP chunk.` |
|         - | 14830 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 14831 | ` * takes care of generating the appropriate error message.` |
|         - | 14832 | ` */` |
|     67600 | 14833 | `static sxi32 PH7_CompilePHP(` |
|         - | 14834 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 14835 | `	SySet *pTokenSet,     /* Token set */` |
|         - | 14836 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|         - | 14837 | `	)` |
|         5 | 14838 | `{` |
|     67605 | 14839 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|         - | 14840 | `	sxi32 rc;` |
|         - | 14841 | `	/* Reset the token set (and its trivia sidecar) */` |
|     67605 | 14842 | `	SySetReset(&(*pTokenSet));` |
|     67605 | 14843 | `	SySetReset(&pGen->aTrivia);` |
|         - | 14844 | `	/* Mark as the default token set */` |
|     67605 | 14845 | `	pGen->pTokenSet = &(*pTokenSet);` |
|         - | 14846 | `	/* Advance the stream cursor */` |
|     67605 | 14847 | `	pGen->pRawIn++;` |
|         - | 14848 | `	/* Tokenize the PHP chunk first */` |
|     67605 | 14849 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|         - | 14850 | `	/* Point to the head and tail of the token stream. */` |
|     67605 | 14851 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     67605 | 14852 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|     67605 | 14853 | `	if( is_expr ){` |
|       ! 0 | 14854 | `		rc = SXERR_EMPTY;` |
|       ! 0 | 14855 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - | 14856 | `			/* A simple expression,compile it */` |
|       ! 0 | 14857 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 | 14858 | `		}` |
|         - | 14859 | `		/* Emit the DONE instruction */` |
|       ! 0 | 14860 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       ! 0 | 14861 | `		return SXRET_OK;` |
|         - | 14862 | `	}` |
|     67605 | 14863 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - | 14864 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - | 14865 | `		/*` |
|         - | 14866 | `		 * Shortcut syntax for the 'echo' language construct.` |
|         - | 14867 | `		 * According to the PHP reference manual:` |
|         - | 14868 | `		 *  echo() also has a shortcut syntax, where you can` |
|         - | 14869 | `		 *  immediately follow` |
|         - | 14870 | `		 *  the opening tag with an equals sign as follows:` |
|         - | 14871 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|         - | 14872 | `		 * Symisc extension:` |
|         - | 14873 | `		 *   This short syntax works with all PHP opening` |
|         - | 14874 | `		 *   tags unlike the default PHP engine that handle` |
|         - | 14875 | `		 *   only short tag.` |
|         - | 14876 | `		 */` |
|         - | 14877 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|         3 | 14878 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|         3 | 14879 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|         3 | 14880 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|         3 | 14881 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|         3 | 14882 | `		if( rc != SXERR_EMPTY ){` |
|         3 | 14883 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 | 14884 | `		}` |
|         3 | 14885 | `		return SXRET_OK;` |
|         - | 14886 | `	}` |
|         - | 14887 | `	/* Compile the PHP chunk */` |
|     67603 | 14888 | `	rc = GenStateCompileChunk(pGen,0);` |
|         - | 14889 | `	/* Fix exceptions jumps */` |
|     67603 | 14890 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - | 14891 | `	/* Fix gotos now, the jump destination is resolved */` |
|     67603 | 14892 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|         3 | 14893 | `		rc = SXERR_ABORT;` |
|         1 | 14894 | `	}` |
|         - | 14895 | `	/* Reset container */` |
|     67603 | 14896 | `	SySetReset(&pGen->aGoto);` |
|     67603 | 14897 | `	SySetReset(&pGen->aLabel);` |
|     67603 | 14898 | `	SySetReset(&pGen->aNullsafeJmp);` |
|         - | 14899 | `	/* Compilation result */` |
|     67603 | 14900 | `	return rc;` |
|     33805 | 14901 | `}` |
|         - | 14902 | `/*` |
|         - | 14903 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|         - | 14904 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|         - | 14905 | ` * This is the only compile interface exported from this file.` |
|         - | 14906 | ` */` |
|     70808 | 14907 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|         - | 14908 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|         - | 14909 | `	SyString *pScript,  /* Script to compile */` |
|         - | 14910 | `	sxi32 iFlags        /* Compile flags */` |
|         - | 14911 | `	)` |
|         5 | 14912 | `{` |
|         - | 14913 | `	SySet aPhpToken,aRawToken;` |
|         - | 14914 | `	ph7_gen_state *pCodeGen;` |
|         - | 14915 | `	ph7_value *pRawObj;` |
|         - | 14916 | `	sxu32 nObjIdx;` |
|         - | 14917 | `	sxi32 nRawObj;` |
|         - | 14918 | `	int is_expr;` |
|         - | 14919 | `	sxi8 bSavedStrict;` |
|         - | 14920 | `	sxi8 bSavedStrictLocked;` |
|         - | 14921 | `	SyToken *pSavedIn,*pSavedEnd;` |
|         - | 14922 | `	sxi32 rc;` |
|     70813 | 14923 | `	sxu32 nBaseLine = 1;` |
|     70813 | 14924 | `	if( pScript->nByte < 1 ){` |
|         - | 14925 | `		/* Nothing to compile */` |
|       ! 0 | 14926 | `		return PH7_OK;` |
|         - | 14927 | `	}` |
|         - | 14928 | `	/* php skips a "#!" shebang on the first line of a CLI script: consume it` |
|         - | 14929 | `	 * (including its newline) so it is not echoed as inline text, and bump the` |
|         - | 14930 | `	 * base line to 2 so the code below still reports php-matching line numbers. */` |
|     70813 | 14931 | `	if( pScript->nByte >= 2 && pScript->zString[0]=='#' && pScript->zString[1]=='!' ){` |
|         3 | 14932 | `		const char *z = pScript->zString;` |
|         3 | 14933 | `		const char *zEnd = &z[pScript->nByte];` |
|        39 | 14934 | `		while( z < zEnd && z[0] != '\n' ){ z++; }` |
|         3 | 14935 | `		if( z < zEnd ){ z++; } /* consume the newline too */` |
|         3 | 14936 | `		pScript->nByte -= (sxu32)(z - pScript->zString);` |
|         3 | 14937 | `		pScript->zString = z;` |
|         3 | 14938 | `		nBaseLine = 2;` |
|         3 | 14939 | `		if( pScript->nByte < 1 ){` |
|       ! 0 | 14940 | `			return PH7_OK;` |
|         - | 14941 | `		}` |
|         1 | 14942 | `	}` |
|         - | 14943 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|         - | 14944 | `	 * file's flags so include/require restore them on return. */` |
|     70813 | 14945 | `	pCodeGen = &pVm->sCodeGen;` |
|         - | 14946 | ``	/* aPhpToken below is a LOCAL token set: once it is released at `cleanup`, any`` |
|         - | 14947 | `	 * pGen->pIn/pEnd still pointing into it DANGLE. PH7_VmEmitInstr reads pIn to stamp` |
|         - | 14948 | `	 * each instruction's source line, and instructions are still emitted after this` |
|         - | 14949 | `	 * function returns (VmEvalChunk's trailing OP_DONE) -- a use-after-free ASan caught` |
|         - | 14950 | `	 * immediately. Save the caller's cursor and restore it on the way out, so an` |
|         - | 14951 | `	 * enclosing compile keeps its (live) tokens and the top level goes back to NULL. */` |
|     70813 | 14952 | `	pSavedIn = pCodeGen->pIn;` |
|     70813 | 14953 | `	pSavedEnd = pCodeGen->pEnd;` |
|     70813 | 14954 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|     70813 | 14955 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|     70813 | 14956 | `	pCodeGen->bStrictTypes = 0;` |
|     70813 | 14957 | `	pCodeGen->bStrictTypesLocked = 0;` |
|         - | 14958 | `	/* Initialize the tokens containers */` |
|     70813 | 14959 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|     70813 | 14960 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|     70813 | 14961 | `	SySetAlloc(&aPhpToken,0xc0);` |
|     70813 | 14962 | `	is_expr = 0;` |
|     70813 | 14963 | `	if( iFlags & PH7_PHP_ONLY ){` |
|         - | 14964 | `		SyToken sTmp;` |
|         - | 14965 | `		/* PHP only: -*/` |
|     57423 | 14966 | `		sTmp.nLine = 1;` |
|     57423 | 14967 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|     57423 | 14968 | `		sTmp.pUserData = 0;` |
|     57423 | 14969 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|     57423 | 14970 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|     57423 | 14971 | `		if( iFlags & PH7_PHP_EXPR ){` |
|         - | 14972 | `			/* A simple PHP expression */` |
|       ! 0 | 14973 | `			is_expr = 1;` |
|       ! 0 | 14974 | `		}` |
|     28714 | 14975 | `	}else{` |
|         - | 14976 | `		/* Tokenize raw text */` |
|     13395 | 14977 | `		SySetAlloc(&aRawToken,32);` |
|     13395 | 14978 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken,nBaseLine);` |
|         - | 14979 | `	}` |
|         - | 14980 | `	/* Process high-level tokens */` |
|     70813 | 14981 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|     70813 | 14982 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|     70813 | 14983 | `	rc = PH7_OK;` |
|     70813 | 14984 | `	if( is_expr ){` |
|         - | 14985 | `		/* Compile the expression */` |
|       ! 0 | 14986 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|       ! 0 | 14987 | `		goto cleanup;` |
|         - | 14988 | `	}` |
|     70813 | 14989 | `	nObjIdx = 0;` |
|         - | 14990 | `	/* Each compilation unit starts in the global namespace.` |
|         - | 14991 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|         - | 14992 | `	 * preventing namespace bleeding across include()d files. */` |
|     70813 | 14993 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|         - | 14994 | `	/* Start the compilation process */` |
|     42104 | 14995 | `	for(;;){` |
|    151801 | 14996 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|     70801 | 14997 | `			break; /* No more tokens to process */` |
|         - | 14998 | `		}` |
|     81005 | 14999 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|         - | 15000 | `			/* Compile the PHP chunk */` |
|     67605 | 15001 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|     67605 | 15002 | `			if( rc == SXERR_ABORT ){` |
|        15 | 15003 | `				break;` |
|         - | 15004 | `			}` |
|     67593 | 15005 | `			continue;` |
|         - | 15006 | `		}` |
|         - | 15007 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|     13405 | 15008 | `		nRawObj = 0;` |
|     26805 | 15009 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|         - | 15010 | `			/* Consume the raw chunk without any processing */` |
|     13405 | 15011 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|     13405 | 15012 | `			if( pRawObj == 0 ){` |
|       ! 0 | 15013 | `				rc = SXERR_MEM;` |
|       ! 0 | 15014 | `				break;` |
|         - | 15015 | `			}` |
|         - | 15016 | `			/* Mark as constant and emit the load constant instruction */` |
|     13405 | 15017 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|     13405 | 15018 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     13405 | 15019 | `			++nRawObj;` |
|     13405 | 15020 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|         5 | 15021 | `		}` |
|     13405 | 15022 | `		if( nRawObj > 0 ){` |
|         - | 15023 | `			/* Emit the consume instruction */` |
|     13405 | 15024 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      6700 | 15025 | `		}` |
|     35409 | 15026 | `	}` |
|     35404 | 15027 | `cleanup:` |
|         - | 15028 | `	/* Drop the cursor into the token set BEFORE the set is freed (see above). */` |
|     70813 | 15029 | `	pCodeGen->pIn = pSavedIn;` |
|     70813 | 15030 | `	pCodeGen->pEnd = pSavedEnd;` |
|     70813 | 15031 | `	SySetRelease(&aRawToken);` |
|     70813 | 15032 | `	SySetRelease(&aPhpToken);` |
|         - | 15033 | `	/* Restore outer file's strict_types scope */` |
|     70813 | 15034 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|     70813 | 15035 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|     70813 | 15036 | `	return rc;` |
|     35409 | 15037 | `}` |
|         - | 15038 | `/*` |
|         - | 15039 | ` * Utility routines.Initialize the code generator.` |
|         - | 15040 | ` */` |
|      3820 | 15041 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|         - | 15042 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 15043 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 15044 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 15045 | `	)` |
|         5 | 15046 | `{` |
|      3825 | 15047 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 15048 | `	/* Zero the structure */` |
|      3825 | 15049 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|         - | 15050 | `	/* Initial state */` |
|      3825 | 15051 | `	pGen->pVm  = &(*pVm);` |
|      3825 | 15052 | `	pGen->xErr = xErr;` |
|      3825 | 15053 | `	pGen->pErrData = pErrData;` |
|      3825 | 15054 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|      3825 | 15055 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|      3825 | 15056 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|      3825 | 15057 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|      3825 | 15058 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|      3825 | 15059 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3825 | 15060 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3825 | 15061 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|      3825 | 15062 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|         - | 15063 | `	/* Error log buffer */` |
|      3825 | 15064 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         - | 15065 | `	/* General purpose working buffer */` |
|      3825 | 15066 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         - | 15067 | `	/* Namespace state */` |
|      3825 | 15068 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|      3825 | 15069 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|      3825 | 15070 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|      3825 | 15071 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 15072 | `	/* Create the global scope */` |
|      3825 | 15073 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|         - | 15074 | `	/* Point to the global scope */` |
|      3825 | 15075 | `	pGen->pCurrent = &pGen->sGlobal;` |
|      3825 | 15076 | `	return SXRET_OK;` |
|         5 | 15077 | `}` |
|         - | 15078 | `/*` |
|         - | 15079 | ` * Utility routines. Reset the code generator to it's initial state.` |
|         - | 15080 | ` */` |
|     74168 | 15081 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|         - | 15082 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 15083 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 15084 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 15085 | `	)` |
|         5 | 15086 | `{` |
|     74173 | 15087 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 15088 | `	GenBlock *pBlock,*pParent;` |
|         - | 15089 | `	/* Reset state */` |
|     74173 | 15090 | `	SySetReset(&pGen->aLabel);` |
|     74173 | 15091 | `	SySetReset(&pGen->aGoto);` |
|     74173 | 15092 | `	SySetReset(&pGen->aNullsafeJmp);` |
|     74173 | 15093 | `	SySetReset(&pGen->aTrivia);` |
|     74173 | 15094 | `	SySetReset(&pGen->aPendingAttrs);` |
|     74173 | 15095 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|     74173 | 15096 | `	SyBlobRelease(&pGen->sErrBuf);` |
|     74173 | 15097 | `	SyBlobRelease(&pGen->sWorker);` |
|     74173 | 15098 | `	SyBlobRelease(&pGen->sNamespace);` |
|     74173 | 15099 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     74173 | 15100 | `	SyHashRelease(&pGen->hUseImports);` |
|     74173 | 15101 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|     74173 | 15102 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     74173 | 15103 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|     74173 | 15104 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     74173 | 15105 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 15106 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|         - | 15107 | `	 * They intern variable names and literal strings that are referenced by` |
|         - | 15108 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|         - | 15109 | `	 * Releasing them would either leak the interned strings or require freeing` |
|         - | 15110 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|         - | 15111 | `	 * number of unique names, which is acceptable. */` |
|         - | 15112 | `	/* Point to the global scope */` |
|     74173 | 15113 | `	pBlock = pGen->pCurrent;` |
|     74173 | 15114 | `	while( pBlock->pParent != 0 ){` |
|       ! 0 | 15115 | `		pParent = pBlock->pParent;` |
|       ! 0 | 15116 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 15117 | `		pBlock = pParent;` |
|       ! 0 | 15118 | `	}` |
|     74173 | 15119 | `	pGen->xErr = xErr;` |
|     74173 | 15120 | `	pGen->pErrData = pErrData;` |
|     74173 | 15121 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     74173 | 15122 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|     74173 | 15123 | `	pGen->pIn = pGen->pEnd = 0;` |
|     74173 | 15124 | `	pGen->nErr = 0;` |
|     74173 | 15125 | `	return SXRET_OK;` |
|         5 | 15126 | `}` |
|         - | 15127 | `/*` |
|         - | 15128 | ` * Save the code generator's compile-position state and hand the live generator a` |
|         - | 15129 | ` * fresh, empty one for a NESTED compilation unit.` |
|         - | 15130 | ` *` |
|         - | 15131 | ` * A require/include normally runs at execution time, when no compile is in flight,` |
|         - | 15132 | ` * so VmEvalChunk can call PH7_ResetCodeGenerator to wipe the generator. But an` |
|         - | 15133 | `` * autoload can fire in the MIDDLE of compiling a class: resolving `class Child`` |
|         - | 15134 | `` * extends Base` calls PH7_VmExtractClass, which triggers the autoloader, whose`` |
|         - | 15135 | `` * body `require`s Base's file — a nested compile while the outer one is mid-token.`` |
|         - | 15136 | ` * PH7_ResetCodeGenerator would then blow away the outer compile's cursor` |
|         - | 15137 | ` * (pIn/pEnd), current block, use-imports, labels, etc.; the outer parse resumes` |
|         - | 15138 | ` * with pIn == NULL and reports a bogus "Expected '{' after class 'Child'". (Common` |
|         - | 15139 | ` * in every real framework: Composer autoloads parent classes on demand.)` |
|         - | 15140 | ` *` |
|         - | 15141 | ` * This snapshots the position/scope fields into *pSaved (a caller-stack` |
|         - | 15142 | ` * ph7_gen_state used purely as storage) and re-initializes the live generator's` |
|         - | 15143 | ` * position containers to FRESH, empty ones WITHOUT releasing the outer's (the` |
|         - | 15144 | ` * snapshot now owns those). hLiteral/hNumLiteral/hVar are intentionally left LIVE` |
|         - | 15145 | ` * and shared — compiled bytecode interns names/literals into them and they grow` |
|         - | 15146 | ` * monotonically (exactly why PH7_ResetCodeGenerator keeps them); restoring an old` |
|         - | 15147 | ` * copy of those headers after a nested grow would use-after-free the bucket array.` |
|         - | 15148 | ` */` |
|         4 | 15149 | `PH7_PRIVATE void PH7_CompilerSaveState(ph7_vm *pVm,ph7_gen_state *pSaved,ProcConsumer xErr,void *pErrData)` |
|         1 | 15150 | `{` |
|         5 | 15151 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 15152 | `	/* Shallow-copy every field; the position containers below are then replaced` |
|         - | 15153 | `	 * with fresh ones on the live struct, so *pSaved keeps the outer's memory. */` |
|         5 | 15154 | `	*pSaved = *pGen;` |
|         5 | 15155 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|         5 | 15156 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|         5 | 15157 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|         5 | 15158 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|         5 | 15159 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|         5 | 15160 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|         5 | 15161 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         5 | 15162 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         5 | 15163 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|         5 | 15164 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|         5 | 15165 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|         5 | 15166 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 15167 | `	/* Fresh global scope for the nested unit (address of the embedded sGlobal is` |
|         - | 15168 | `	 * stable, so any outer block still parented to it stays valid across restore). */` |
|         5 | 15169 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(pVm),0);` |
|         5 | 15170 | `	pGen->pCurrent = &pGen->sGlobal;` |
|         5 | 15171 | `	pGen->pIn = pGen->pEnd = 0;` |
|         5 | 15172 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|         5 | 15173 | `	pGen->pTokenSet = 0;` |
|         5 | 15174 | `	pGen->nErr = 0;` |
|         5 | 15175 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|         5 | 15176 | `	pGen->nCommaExprOk = 0;` |
|         5 | 15177 | `	pGen->bInGenerator = 0;` |
|         5 | 15178 | `	pGen->bStrictTypes = 0;` |
|         5 | 15179 | `	pGen->bStrictTypesLocked = 0;` |
|         5 | 15180 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|         5 | 15181 | `	pGen->xErr = xErr;` |
|         5 | 15182 | `	pGen->pErrData = pErrData;` |
|         5 | 15183 | `}` |
|         - | 15184 | `/*` |
|         - | 15185 | ` * Restore the outer compile-position state saved by PH7_CompilerSaveState,` |
|         - | 15186 | ` * releasing the nested unit's position containers first. The shared` |
|         - | 15187 | ` * hLiteral/hNumLiteral/hVar tables (grown by the nested compile) are carried` |
|         - | 15188 | ` * forward, NOT rolled back to the snapshot's stale headers.` |
|         - | 15189 | ` */` |
|         4 | 15190 | `PH7_PRIVATE void PH7_CompilerRestoreState(ph7_vm *pVm,ph7_gen_state *pSaved)` |
|         1 | 15191 | `{` |
|         5 | 15192 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 15193 | `	GenBlock *pBlock,*pParent;` |
|         - | 15194 | `	SyHash hVar,hLiteral,hNumLiteral;` |
|         - | 15195 | `	/* Free any nested blocks left open (e.g. an aborted nested compile), then the` |
|         - | 15196 | `	 * nested global block's own fixup sets. */` |
|         5 | 15197 | `	pBlock = pGen->pCurrent;` |
|         5 | 15198 | `	while( pBlock && pBlock->pParent != 0 ){` |
|       ! 0 | 15199 | `		pParent = pBlock->pParent;` |
|       ! 0 | 15200 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 15201 | `		pBlock = pParent;` |
|       ! 0 | 15202 | `	}` |
|         5 | 15203 | `	GenStateReleaseBlock(&pGen->sGlobal);` |
|         - | 15204 | `	/* Release the nested unit's position containers. */` |
|         5 | 15205 | `	SySetRelease(&pGen->aLabel);` |
|         5 | 15206 | `	SySetRelease(&pGen->aGoto);` |
|         5 | 15207 | `	SySetRelease(&pGen->aNullsafeJmp);` |
|         5 | 15208 | `	SySetRelease(&pGen->aLoopParent);` |
|         5 | 15209 | `	SySetRelease(&pGen->aTrivia);` |
|         5 | 15210 | `	SySetRelease(&pGen->aPendingAttrs);` |
|         5 | 15211 | `	SyBlobRelease(&pGen->sWorker);` |
|         5 | 15212 | `	SyBlobRelease(&pGen->sErrBuf);` |
|         5 | 15213 | `	SyBlobRelease(&pGen->sNamespace);` |
|         5 | 15214 | `	SyHashRelease(&pGen->hUseImports);` |
|         5 | 15215 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|         5 | 15216 | `	SyHashRelease(&pGen->hUseConstImports);` |
|         - | 15217 | `	/* Preserve the (possibly grown) shared intern tables across the restore. */` |
|         5 | 15218 | `	hVar = pGen->hVar;` |
|         5 | 15219 | `	hLiteral = pGen->hLiteral;` |
|         5 | 15220 | `	hNumLiteral = pGen->hNumLiteral;` |
|         5 | 15221 | `	*pGen = *pSaved;` |
|         5 | 15222 | `	pGen->hVar = hVar;` |
|         5 | 15223 | `	pGen->hLiteral = hLiteral;` |
|         5 | 15224 | `	pGen->hNumLiteral = hNumLiteral;` |
|         5 | 15225 | `}` |
|         - | 15226 | `/*` |
|         - | 15227 | ` * Raise php's parse error for an unexpected token: E_PARSE with the exact text` |
|         - | 15228 | ` * php's parser prints, e.g.` |
|         - | 15229 | ` *` |
|         - | 15230 | ` *   syntax error, unexpected token ";", expecting "{"` |
|         - | 15231 | ` *   syntax error, unexpected identifier "invalid", expecting "("` |
|         - | 15232 | ` *   syntax error, unexpected end of file` |
|         - | 15233 | ` *` |
|         - | 15234 | ` * php names the token by CLASS, not just by text: an identifier, a variable and` |
|         - | 15235 | ` * a number each get their own noun, while everything else (keywords, operators,` |
|         - | 15236 | ` * punctuation) is a "token". zExpecting is the optional ", expecting ..." tail` |
|         - | 15237 | ` * — pass NULL when the site cannot say what it wanted (php often can't either).` |
|         - | 15238 | ` * pTok == NULL means the input ran out: "unexpected end of file".` |
|         - | 15239 | ` *` |
|         - | 15240 | ` * PHL's hand-written recursive-descent parser has no bison expectation sets, so` |
|         - | 15241 | ` * a site can only claim an "expecting" clause it genuinely knows; every clause` |
|         - | 15242 | ` * emitted here was verified against php 8.5.7 for the construct in question.` |
|         - | 15243 | ` */` |
|       182 | 15244 | `PH7_PRIVATE sxi32 PH7_GenSyntaxError(` |
|         - | 15245 | `	ph7_gen_state *pGen,   /* Code generator state */` |
|         - | 15246 | `	SyToken *pTok,         /* Offending token, or NULL for end of file */` |
|         - | 15247 | `	const char *zExpecting /* ", expecting <this>" tail, or NULL */` |
|         - | 15248 | `	)` |
|         5 | 15249 | `{` |
|       187 | 15250 | `	const char *zNoun = "token";` |
|         - | 15251 | `	sxu32 nLine;` |
|       187 | 15252 | `	if( pTok == 0 && pGen->pTokenSet ){` |
|         - | 15253 | `		/* The caller ran out of tokens inside its own slice — but a statement's slice stops` |
|         - | 15254 | `		 * BEFORE its terminator, so the token php actually names (typically the ';') is the` |
|         - | 15255 | `		 * one sitting just past the slice, still inside the chunk's token stream. Reach for` |
|         - | 15256 | `		 * it before concluding "end of file". */` |
|        92 | 15257 | `		SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        92 | 15258 | `		SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        92 | 15259 | `		if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|        92 | 15260 | `			pTok = pGen->pEnd;` |
|        44 | 15261 | `		}` |
|        44 | 15262 | `	}` |
|       187 | 15263 | `	nLine = pTok ? pTok->nLine : (pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ? pGen->pIn[-1].nLine : 1);` |
|       187 | 15264 | `	if( pTok == 0 ){` |
|       ! 0 | 15265 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       ! 0 | 15266 | `			zExpecting ? "syntax error, unexpected end of file, expecting %s"` |
|         - | 15267 | `			           : "syntax error, unexpected end of file",` |
|       ! 0 | 15268 | `			zExpecting);` |
|         - | 15269 | `	}` |
|       187 | 15270 | `	if( pTok->nType & PH7_TK_ID ){` |
|        16 | 15271 | `		zNoun = "identifier";` |
|       180 | 15272 | `	}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|         9 | 15273 | `		zNoun = "variable";` |
|       171 | 15274 | `	}else if( pTok->nType & PH7_TK_INTEGER ){` |
|        23 | 15275 | `		zNoun = "integer";` |
|       158 | 15276 | `	}else if( pTok->nType & PH7_TK_REAL ){` |
|       ! 0 | 15277 | `		zNoun = "float";` |
|       ! 0 | 15278 | `	}` |
|       187 | 15279 | `	if( zExpecting ){` |
|       118 | 15280 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        38 | 15281 | `			"syntax error, unexpected %s \"%z\", expecting %s",zNoun,&pTok->sData,zExpecting);` |
|         - | 15282 | `	}` |
|       164 | 15283 | `	return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        53 | 15284 | `		"syntax error, unexpected %s \"%z\"",zNoun,&pTok->sData);` |
|        96 | 15285 | `}` |
|         - | 15286 | `/*` |
|         - | 15287 | ` * Generate a compile-time error message.` |
|         - | 15288 | ` * If the error count limit is reached (usually 15 error message)` |
|         - | 15289 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|         - | 15290 | ` * abort compilation immediately.` |
|         - | 15291 | ` */` |
|     15956 | 15292 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|         5 | 15293 | `{` |
|     15961 | 15294 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     15961 | 15295 | `	const char *zErr = "Error";` |
|         - | 15296 | `	SyString *pFile;` |
|         - | 15297 | `	va_list ap;` |
|         - | 15298 | `	sxi32 rc;` |
|         - | 15299 | `	/* Reset the working buffer */` |
|     15961 | 15300 | `	SyBlobReset(pWorker);` |
|         - | 15301 | `	/* Peek the processed file path if available */` |
|     15961 | 15302 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     15961 | 15303 | `	if( nErrType == E_ERROR \|\| nErrType == E_PARSE ){` |
|         - | 15304 | `		/* Increment the error counter. A PARSE error is every bit as fatal as an` |
|         - | 15305 | `		 * E_ERROR one: php compiles nothing, runs nothing and exits 255. Counting` |
|         - | 15306 | `		 * only E_ERROR let a parse error print its diagnostic and then fall through` |
|         - | 15307 | `		 * into execution with a 0 exit status. */` |
|       659 | 15308 | `		pGen->nErr++;` |
|       659 | 15309 | `		if( pGen->nErr > 15 ){` |
|         - | 15310 | `			/* Error count limit reached */` |
|         6 | 15311 | `			if( pGen->xErr ){` |
|         6 | 15312 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|         6 | 15313 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|         6 | 15314 | `				if( pFile ){` |
|         6 | 15315 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|         2 | 15316 | `				}` |
|         6 | 15317 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|         6 | 15318 | `				if( SyBlobLength(pWorker) > 0 ){` |
|         6 | 15319 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|         2 | 15320 | `				}` |
|         2 | 15321 | `			}` |
|         - | 15322 | `			/* Abort immediately */` |
|         6 | 15323 | `			return SXERR_ABORT;` |
|         - | 15324 | `		}` |
|       325 | 15325 | `	}` |
|     15957 | 15326 | `	if( pGen->xErr == 0 ){` |
|         - | 15327 | `		/* No available error consumer,return immediately */` |
|     15287 | 15328 | `		return SXRET_OK;` |
|         - | 15329 | `	}` |
|       675 | 15330 | `	switch(nErrType){` |
|       310 | 15331 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|         8 | 15332 | `	case E_WARNING: zErr = "Warning";     break;` |
|       346 | 15333 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|         6 | 15334 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|       ! 0 | 15335 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|       ! 0 | 15336 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|       ! 0 | 15337 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|        16 | 15338 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|       ! 0 | 15339 | `	default:` |
|       ! 0 | 15340 | `		break;` |
|         - | 15341 | `	}` |
|       675 | 15342 | `	rc = SXRET_OK;` |
|         - | 15343 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|       675 | 15344 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|       675 | 15345 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|       675 | 15346 | `	va_start(ap,zFormat);` |
|       675 | 15347 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|       675 | 15348 | `	va_end(ap);` |
|       675 | 15349 | `	if( pFile ){` |
|       675 | 15350 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       335 | 15351 | `	}` |
|         - | 15352 | `	/* Append a new line */` |
|       675 | 15353 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       675 | 15354 | `	if( SyBlobLength(pWorker) > 0 ){` |
|         - | 15355 | `		/* Consume the generated error message */` |
|       675 | 15356 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       335 | 15357 | `	}` |
|       675 | 15358 | `	return rc;` |
|      7983 | 15359 | `}` |
|         - | 15360 |  |
