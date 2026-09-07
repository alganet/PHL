# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 7098/8796 lines (80.70%)

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
|   9969762 |   163 | `static void GenStateInitBlock(` |
|         - |   164 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |   165 | `	GenBlock *pBlock,    /* Target block */` |
|         - |   166 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   167 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|         - |   168 | `	void *pUserData      /* Upper layer private data */` |
|         - |   169 | `	)` |
|         5 |   170 | `{` |
|         - |   171 | `	/* Initialize block fields */` |
|   9969767 |   172 | `	pBlock->nFirstInstr = nFirstInstr;` |
|   9969767 |   173 | `	pBlock->pUserData   = pUserData;` |
|   9969767 |   174 | `	pBlock->pGen        = pGen;` |
|   9969767 |   175 | `	pBlock->iFlags      = iType;` |
|   9969767 |   176 | `	pBlock->pParent     = 0;` |
|   9969767 |   177 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|   9969767 |   178 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|   9969767 |   179 | `}` |
|         - |   180 | `/*` |
|         - |   181 | ` * Allocate a new block instance.` |
|         - |   182 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|         - |   183 | ` * on success.Otherwise generate a compile-time error and abort` |
|         - |   184 | ` * processing on failure.` |
|         - |   185 | ` */` |
|   9965966 |   186 | `static sxi32 GenStateEnterBlock(` |
|         - |   187 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |   188 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   189 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|         - |   190 | `	void *pUserData,      /* Upper layer private data */` |
|         - |   191 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|         - |   192 | `	)` |
|         5 |   193 | `{` |
|         - |   194 | `	GenBlock *pBlock;` |
|         - |   195 | `	/* Allocate a new block instance */` |
|   9965971 |   196 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|   9965971 |   197 | `	if( pBlock == 0 ){` |
|         - |   198 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |   199 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |   200 | `		 */` |
|       ! 0 |   201 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|         - |   202 | `		/* Abort processing immediately */` |
|       ! 0 |   203 | `		return SXERR_ABORT;` |
|         - |   204 | `	}` |
|         - |   205 | `	/* Zero the structure */` |
|   9965971 |   206 | `	SyZero(pBlock,sizeof(GenBlock));` |
|   9965971 |   207 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|         - |   208 | `	/* Link to the parent block */` |
|   9965971 |   209 | `	pBlock->pParent = pGen->pCurrent;` |
|         - |   210 | `	/* A loop or switch gets an id, and remembers the loop it nests inside, so a goto's` |
|         - |   211 | `	 * and a label's positions can be compared after compilation (see aLoopParent). */` |
|   9965971 |   212 | `	if( iType & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    361655 |   213 | `		sxu32 nParent = pGen->nCurLoopId;` |
|    361655 |   214 | `		pGen->nLoopId++;` |
|    361655 |   215 | `		SySetPut(&pGen->aLoopParent,(const void *)&nParent);` |
|    361655 |   216 | `		pBlock->nLoopId = pGen->nLoopId;` |
|    361655 |   217 | `		pBlock->nOuterLoopId = nParent;` |
|    361655 |   218 | `		pGen->nCurLoopId = pGen->nLoopId;` |
|    180825 |   219 | `	}` |
|         - |   220 | `	/* Mark as the current block */` |
|   9965971 |   221 | `	pGen->pCurrent = pBlock;` |
|   9965971 |   222 | `	if( ppBlock ){` |
|         - |   223 | `		/* Write a pointer to the new instance */` |
|   4797475 |   224 | `		*ppBlock = pBlock;` |
|   2398735 |   225 | `	}` |
|   9965971 |   226 | `	return SXRET_OK;` |
|   4982988 |   227 | `}` |
|         - |   228 | `/*` |
|         - |   229 | ` * Release block fields without freeing the whole instance.` |
|         - |   230 | ` */` |
|   9965950 |   231 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|         5 |   232 | `{` |
|   9965955 |   233 | `	SySetRelease(&pBlock->aPostContFix);` |
|   9965955 |   234 | `	SySetRelease(&pBlock->aJumpFix);` |
|   9965955 |   235 | `}` |
|         - |   236 | `/*` |
|         - |   237 | ` * Release a block.` |
|         - |   238 | ` */` |
|   9965950 |   239 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|         5 |   240 | `{` |
|   9965955 |   241 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|   9965955 |   242 | `	GenStateReleaseBlock(&(*pBlock));` |
|         - |   243 | `	/* Free the instance */` |
|   9965955 |   244 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|   9965955 |   245 | `}` |
|         - |   246 | `/*` |
|         - |   247 | ` * POP and release a block from the stack of compiled blocks.` |
|         - |   248 | ` */` |
|   9965950 |   249 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|         5 |   250 | `{` |
|   9965955 |   251 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   9965955 |   252 | `	if( pBlock == 0 ){` |
|         - |   253 | `		/* No more block to pop */` |
|       ! 0 |   254 | `		return SXERR_EMPTY;` |
|         - |   255 | `	}` |
|   9965955 |   256 | `	if( pBlock->iFlags & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    361647 |   257 | `		pGen->nCurLoopId = pBlock->nOuterLoopId;` |
|    180821 |   258 | `	}` |
|         - |   259 | `	/* Point to the upper block */` |
|   9965955 |   260 | `	pGen->pCurrent = pBlock->pParent;` |
|   9965955 |   261 | `	if( ppBlock ){` |
|         - |   262 | `		/* Write a pointer to the popped block */` |
|       ! 0 |   263 | `		*ppBlock = pBlock;` |
|       ! 0 |   264 | `	}else{` |
|         - |   265 | `		/* Safely release the block */` |
|   9965955 |   266 | `		GenStateFreeBlock(&(*pBlock));` |
|         - |   267 | `	}` |
|   9965955 |   268 | `	return SXRET_OK;` |
|   4982980 |   269 | `}` |
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
|   3563430 |   280 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|         5 |   281 | `{` |
|         - |   282 | `	JumpFixup sJumpFix;` |
|         - |   283 | `	sxi32 rc;` |
|         - |   284 | `	/* Init the JumpFixup structure */` |
|   3563435 |   285 | `	sJumpFix.nJumpType = nJumpType;` |
|   3563435 |   286 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|         - |   287 | `	/* Insert in the jump fixup table */` |
|   3563435 |   288 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|   3563435 |   289 | `	return rc;` |
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
|   6931912 |   302 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|         5 |   303 | `{` |
|         - |   304 | `	JumpFixup *aFix;` |
|         - |   305 | `	VmInstr *pInstr;` |
|         - |   306 | `	sxu32 nFixed;` |
|         - |   307 | `	sxu32 n;` |
|         - |   308 | `	/* Point to the jump fixup table */` |
|   6931917 |   309 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|         - |   310 | `	/* Fix the desired jumps */` |
|  14637613 |   311 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|   7705701 |   312 | `		if( aFix[n].nJumpType < 0 ){` |
|         - |   313 | `			/* Already fixed */` |
|   2922911 |   314 | `			continue;` |
|         - |   315 | `		}` |
|   4782795 |   316 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|         - |   317 | `			/* Not of our interest */` |
|   1219367 |   318 | `			continue;` |
|         - |   319 | `		}` |
|         - |   320 | `		/* Point to the instruction to fix */` |
|   3563433 |   321 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|   3563433 |   322 | `		if( pInstr ){` |
|   3563433 |   323 | `			pInstr->iP2 = nJumpDest;` |
|   3563433 |   324 | `			nFixed++;` |
|         - |   325 | `			/* Mark as fixed */` |
|   3563433 |   326 | `			aFix[n].nJumpType = -1;` |
|   1781714 |   327 | `		}` |
|   1781719 |   328 | `	}` |
|         - |   329 | `	/* Total number of fixed jumps */` |
|   6931917 |   330 | `	return nFixed;` |
|         5 |   331 | `}` |
|         - |   332 | `/*` |
|         - |   333 | ` * Fix a 'goto' now the jump destination is resolved.` |
|         - |   334 | ` * The goto statement can be used to jump to another section` |
|         - |   335 | ` * in the program.` |
|         - |   336 | ` * Refer to the routine responsible of compiling the goto` |
|         - |   337 | ` * statement for more information.` |
|         - |   338 | ` */` |
|   2598890 |   339 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|         5 |   340 | `{` |
|         - |   341 | `	JumpFixup *pJump,*aJumps;` |
|         - |   342 | `	Label *pLabel;` |
|         - |   343 | `	VmInstr *pInstr;` |
|         - |   344 | `	sxi32 rc;` |
|         - |   345 | `	sxu32 n;` |
|         - |   346 | `	/* Point to the goto table */` |
|   2598895 |   347 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|         - |   348 | `	/* Fix */` |
|   2599041 |   349 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|   2598893 |   400 | `	return SXRET_OK;` |
|   1299450 |   401 | `}` |
|         - |   402 | `/*` |
|         - |   403 | ` * Check if a given token value is installed in the literal table.` |
|         - |   404 | ` */` |
|  13116380 |   405 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|         5 |   406 | `{` |
|         - |   407 | `	SyHashEntry *pEntry;` |
|  13116385 |   408 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  13116385 |   409 | `	if( pEntry == 0 ){` |
|   3434539 |   410 | `		return SXERR_NOTFOUND;` |
|         - |   411 | `	}` |
|   9681851 |   412 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|   9681851 |   413 | `	return SXRET_OK;` |
|   6558195 |   414 | `}` |
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
|   3434534 |   425 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|         5 |   426 | `{` |
|   3434539 |   427 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|   3434539 |   428 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   1717267 |   429 | `	}` |
|   3434539 |   430 | `	return SXRET_OK;` |
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
|   6127894 |   466 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|         5 |   467 | `{` |
|         - |   468 | `	VmCallArgMap *pMap;` |
|   6127899 |   469 | `	if( !pGen->bStrictTypes ) return p3;` |
|        39 |   470 | `	if( p3 == 0 ){` |
|        35 |   471 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|        35 |   472 | `		if( pMap == 0 ) return 0;` |
|        35 |   473 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|        35 |   474 | `		p3 = (void *)pMap;` |
|        16 |   475 | `	}` |
|        39 |   476 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|        39 |   477 | `	return p3;` |
|   3063952 |   478 | `}` |
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
|   4282960 |   832 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |   833 | `{` |
|   4282965 |   834 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|         - |   835 | `	const char *zIn,*zCur,*zEnd;` |
|         - |   836 | `	ph7_value *pObj;` |
|         - |   837 | `	sxu32 nIdx;` |
|   4282965 |   838 | `	nIdx = 0; /* Prevent compiler warning */` |
|         - |   839 | `	/* Delimit the string */` |
|   4282965 |   840 | `	zIn  = pStr->zString;` |
|   4282965 |   841 | `	zEnd = &zIn[pStr->nByte];` |
|   4282965 |   842 | `	if( zIn >= zEnd ){` |
|         - |   843 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|         - |   844 | `		 * rather than reserving a new object each time. */` |
|    201401 |   845 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    201401 |   846 | `		return SXRET_OK;` |
|         - |   847 | `	}` |
|   4081569 |   848 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|         - |   849 | `		/* Already processed,emit the load constant instruction` |
|         - |   850 | `		 * and return.` |
|         - |   851 | `		 */` |
|   2380707 |   852 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   2380707 |   853 | `		return SXRET_OK;` |
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
|   2141485 |   903 | `}` |
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
|     81780 |  1103 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|         5 |  1104 | `{` |
|         - |  1105 | `	ph7_value *pConstObj;` |
|     81785 |  1106 | `	sxu32 nIdx = 0;` |
|         - |  1107 | `	/* Reserve a new constant */` |
|     81785 |  1108 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     81785 |  1109 | `	if( pConstObj == 0 ){` |
|       ! 0 |  1110 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  1111 | `		return 0;` |
|         - |  1112 | `	}` |
|     81785 |  1113 | `	(*pCount)++;` |
|     81785 |  1114 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|         - |  1115 | `	/* Emit the load constant instruction */` |
|     81785 |  1116 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     81785 |  1117 | `	return pConstObj;` |
|     40895 |  1118 | `}` |
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
|     80220 |  1181 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|         5 |  1182 | `{` |
|     80225 |  1183 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|         - |  1184 | `	const char *zIn,*zCur,*zEnd;` |
|     80225 |  1185 | `	ph7_value *pObj = 0;` |
|         - |  1186 | `	sxi32 iCons;` |
|         - |  1187 | `	sxi32 rc;` |
|         - |  1188 | `	/* Delimit the string */` |
|     80225 |  1189 | `	zIn  = pStr->zString;` |
|     80225 |  1190 | `	zEnd = &zIn[pStr->nByte];` |
|     80225 |  1191 | `	if( zIn >= zEnd ){` |
|         - |  1192 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|         - |  1193 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|         - |  1194 | `		 * literal table from growing when many "" literals appear in the source.` |
|         - |  1195 | `		 */` |
|       381 |  1196 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|       381 |  1197 | `		return SXRET_OK;` |
|         - |  1198 | `	}` |
|     79849 |  1199 | `	zCur = 0;` |
|         - |  1200 | `	/* Compile the node */` |
|     79849 |  1201 | `	iCons = 0;` |
|     41206 |  1202 | `	for(;;){` |
|    114611 |  1203 | `		zCur = zIn;` |
|   1534075 |  1204 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|   1422037 |  1205 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|        72 |  1206 | `				break;` |
|   1421904 |  1207 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|      2440 |  1208 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|      1220 |  1209 | `					break;` |
|         - |  1210 | `			}` |
|   1419469 |  1211 | `			zIn++;` |
|         5 |  1212 | `		}` |
|    114611 |  1213 | `		if( zIn > zCur ){` |
|     55479 |  1214 | `			if( pObj == 0 ){` |
|     54885 |  1215 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|     54885 |  1216 | `				if( pObj == 0 ){` |
|       ! 0 |  1217 | `					return SXERR_ABORT;` |
|         - |  1218 | `				}` |
|     27440 |  1219 | `			}` |
|     55479 |  1220 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|     27737 |  1221 | `		}` |
|    114611 |  1222 | `		if( zIn >= zEnd ){` |
|     79847 |  1223 | `			break;` |
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
|     79849 |  1503 | `	if( iCons > 1 ){` |
|         - |  1504 | `		/* Concatenate all compiled constants */` |
|      1861 |  1505 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|       928 |  1506 | `	}` |
|         - |  1507 | `	/* Node successfully compiled */` |
|     79849 |  1508 | `	return SXRET_OK;` |
|     40115 |  1509 | `}` |
|         - |  1510 | `/*` |
|         - |  1511 | ` * Compile a double quoted string.` |
|         - |  1512 | ` *  See the block-comment above for more information.` |
|         - |  1513 | ` */` |
|     80158 |  1514 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1515 | `{` |
|         - |  1516 | `	sxi32 rc;` |
|     80163 |  1517 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|     40079 |  1518 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  1519 | `	/* Compilation result */` |
|     80163 |  1520 | `	return rc;` |
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
|    989250 |  1564 | `static sxi32 GenStateCompileArrayEntry(` |
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
|    989255 |  1575 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|         - |  1576 | `	/* Compile the expression*/` |
|    989255 |  1577 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|         - |  1578 | `	/* Restore token stream */` |
|    989255 |  1579 | `	RE_SWAP_DELIMITER(pGen);` |
|    989255 |  1580 | `	return rc;` |
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
|    931076 |  1619 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|         5 |  1620 | `{` |
|    931081 |  1621 | `	SyToken *pCur = pStart;` |
|    931081 |  1622 | `	sxi32 iNest = 0;` |
|   2567693 |  1623 | `	while( pCur < pEnd ){` |
|   2014727 |  1624 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    378111 |  1625 | `			return pCur;` |
|         - |  1626 | `		}` |
|         - |  1627 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|         - |  1628 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|         - |  1629 | `		 * not an entry separator. Skip past the signature.` |
|         - |  1630 | `		 */` |
|   1636621 |  1631 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|     22883 |  1632 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|     22883 |  1633 | `			SyToken *pFn = pCur;` |
|     22878 |  1634 | `			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd` |
|       ! 0 |  1635 | `				&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|         5 |  1636 | `				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|       ! 0 |  1637 | `				pFn = &pCur[1];` |
|       ! 0 |  1638 | `				nKw = PH7_TKWRD_FN;` |
|       ! 0 |  1639 | `			}` |
|     22883 |  1640 | `			if( nKw == PH7_TKWRD_FN ){` |
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
|     22879 |  1671 | `			if( nKw == PH7_TKWRD_MATCH ){` |
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
|     11436 |  1691 | `		}` |
|   1636615 |  1692 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     53599 |  1693 | `			iNest++;` |
|   1609818 |  1694 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|         - |  1695 | `			/* Don't worry about mismatched brackets here, the expression` |
|         - |  1696 | `			 * parser will shortly detect any syntax error. */` |
|     53599 |  1697 | `			iNest--;` |
|     26797 |  1698 | `		}` |
|   1636615 |  1699 | `		pCur++;` |
|         5 |  1700 | `	}` |
|    552971 |  1701 | `	return pEnd;` |
|    465543 |  1702 | `}` |
|         - |  1703 | `/*` |
|         - |  1704 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|         - |  1705 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|         - |  1706 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|         - |  1707 | ` */` |
|    497252 |  1708 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|         5 |  1709 | `{` |
|         - |  1710 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|         - |  1711 | `	SyToken *pKey,*pCur;` |
|    497257 |  1712 | `	sxi32 iEmitRef = 0;` |
|    497257 |  1713 | `	sxi32 iSpread = 0;` |
|    497257 |  1714 | `	sxi32 nPair = 0;` |
|         - |  1715 | `	sxi32 rc;` |
|    497257 |  1716 | `	xValidator = 0;` |
|    603586 |  1717 | `	for(;;){` |
|         - |  1718 | `		/* Jump leading commas */` |
|   1701365 |  1719 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    494193 |  1720 | `			pGen->pIn++;` |
|         5 |  1721 | `		}` |
|   1207177 |  1722 | `		pCur = pGen->pIn;` |
|   1207177 |  1723 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|         - |  1724 | `			/* No more entry to process */` |
|    497241 |  1725 | `			break;` |
|         - |  1726 | `		}` |
|    709941 |  1727 | `		if( pCur >= pGen->pIn ){` |
|       ! 0 |  1728 | `			continue;` |
|         - |  1729 | `		}` |
|         - |  1730 | `		/* Compile the key if available */` |
|    709941 |  1731 | `		pKey = pCur;` |
|    709941 |  1732 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|    709941 |  1733 | `		rc = SXERR_EMPTY;` |
|    709941 |  1734 | `		if( pCur < pGen->pIn ){` |
|    279061 |  1735 | `			if( &pCur[1] >= pGen->pIn ){` |
|         - |  1736 | `				/* Missing value */` |
|        13 |  1737 | `				rc = PH7_GenSyntaxError(&(*pGen),pCur,0);` |
|        13 |  1738 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  1739 | `					return SXERR_ABORT;` |
|         - |  1740 | `				}` |
|        13 |  1741 | `				return SXRET_OK;` |
|         - |  1742 | `			}` |
|         - |  1743 | `			/* Compile the expression holding the key */` |
|    279051 |  1744 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|         - |  1745 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    279051 |  1746 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1747 | `				return SXERR_ABORT;` |
|         - |  1748 | `			}` |
|    279051 |  1749 | `			pCur++; /* Jump the '=>' operator */` |
|    570408 |  1750 | `		}else if( pKey == pCur ){` |
|         - |  1751 | `			/* Key is omitted,emit a warning */` |
|       ! 0 |  1752 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|       ! 0 |  1753 | `			pCur++; /* Jump the '=>' operator */` |
|       ! 0 |  1754 | `		}else{` |
|         - |  1755 | `			/* Reset back the cursor and point to the entry value */` |
|    430885 |  1756 | `			pCur = pKey;` |
|         - |  1757 | `		}` |
|    709931 |  1758 | `		if( rc == SXERR_EMPTY ){` |
|         - |  1759 | `			/* No available key,load NULL */` |
|    430887 |  1760 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    215441 |  1761 | `		}` |
|    709931 |  1762 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|    709929 |  1781 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|    709929 |  1782 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
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
|   1064885 |  1799 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,` |
|    354960 |  1800 | `			iEmitRef ? EXPR_FLAG_LOAD_IDX_STORE` |
|         - |  1801 | `			         : EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,` |
|    354960 |  1802 | `			xValidator);` |
|    709925 |  1803 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  1804 | `			return SXERR_ABORT;` |
|         - |  1805 | `		}` |
|    709925 |  1806 | `		if( iSpread ){` |
|         - |  1807 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|        69 |  1808 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|    709892 |  1809 | `		}else if( iEmitRef ){` |
|         - |  1810 | `			/* Emit the load reference instruction */` |
|        40 |  1811 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|        18 |  1812 | `		}` |
|    709925 |  1813 | `		xValidator = 0;` |
|    709925 |  1814 | `		iEmitRef = 0;` |
|    709925 |  1815 | `		iSpread = 0;` |
|    709925 |  1816 | `		nPair++;` |
|         5 |  1817 | `	}` |
|         - |  1818 | `	/* Emit the load map instruction */` |
|    497241 |  1819 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|         - |  1820 | `	/* Node successfully compiled */` |
|    497241 |  1821 | `	return SXRET_OK;` |
|    248631 |  1822 | `}` |
|         - |  1823 | `/*` |
|         - |  1824 | ` * Compile the 'array' language construct.` |
|         - |  1825 | ` *	 According to the PHP language reference manual` |
|         - |  1826 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|         - |  1827 | ` *   values to keys. This type is optimized for several different uses; it can` |
|         - |  1828 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|         - |  1829 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|         - |  1830 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|         - |  1831 | ` */` |
|    282752 |  1832 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1833 | `{` |
|         - |  1834 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|    282757 |  1835 | `	pGen->pIn += 2;` |
|    282757 |  1836 | `	pGen->pEnd--;` |
|    141376 |  1837 | `	SXUNUSED(iCompileFlag);` |
|    282757 |  1838 | `	return GenStateCompileArrayBody(pGen);` |
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
|    214500 |  1937 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1938 | `{` |
|         - |  1939 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|    214505 |  1940 | `	pGen->pIn++;` |
|    214505 |  1941 | `	pGen->pEnd--;` |
|    107250 |  1942 | `	SXUNUSED(iCompileFlag);` |
|    214505 |  1943 | `	return GenStateCompileArrayBody(pGen);` |
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
|         3 |  2463 | `{` |
|       307 |  2464 | `	SyToken *pScan = pStart;` |
|         - |  2465 | `	sxi32 rc;` |
|      1739 |  2466 | `	while( pScan < pEnd ){` |
|      1435 |  2467 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
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
|      1379 |  2478 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
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
|      1361 |  2630 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|      1181 |  2631 | `			pScan++;` |
|      1181 |  2632 | `			continue;` |
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
|       307 |  2657 | `	return SXRET_OK;` |
|       155 |  2658 | `}` |
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
|       286 |  2733 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|       286 |  2734 | `	if( pFunc == 0 ){` |
|       ! 0 |  2735 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2736 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2737 | `		return SXERR_ABORT;` |
|         - |  2738 | `	}` |
|         - |  2739 | `	/* Generate a unique lambda name */` |
|       286 |  2740 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       286 |  2741 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 |  2742 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       ! 0 |  2743 | `	}` |
|       286 |  2744 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|       286 |  2745 | `	if( zDup == 0 ){` |
|       ! 0 |  2746 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2747 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2748 | `		return SXERR_ABORT;` |
|         - |  2749 | `	}` |
|       286 |  2750 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|         - |  2751 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|       286 |  2752 | `	pFunc->nLine = nLine;` |
|         - |  2753 | ``	/* Expression-position attributes (`$f = #[A] fn () => …`) */`` |
|       286 |  2754 | `	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  2755 | `		return SXERR_ABORT;` |
|         - |  2756 | `	}` |
|         - |  2757 | `	/* Collect function arguments */` |
|       286 |  2758 | `	if( pGen->pIn < pSigEnd ){` |
|       116 |  2759 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|       116 |  2760 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2761 | `			return SXERR_ABORT;` |
|         - |  2762 | `		}` |
|        56 |  2763 | `	}` |
|         - |  2764 | `	/* Point past ')' and parse optional return type */` |
|       286 |  2765 | `	pGen->pIn = &pSigEnd[1];` |
|       286 |  2766 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|       286 |  2767 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2768 | `		return SXERR_ABORT;` |
|       286 |  2769 | `	}else if( rc == SXERR_SYNTAX ){` |
|       ! 0 |  2770 | `		return SXERR_SYNTAX;` |
|         - |  2771 | `	}` |
|         - |  2772 | `	/* Expect '=>' */` |
|       286 |  2773 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
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
|       283 |  2784 | `	pGen->pIn++; /* Jump '=>' */` |
|       283 |  2785 | `	pBodyStart = pGen->pIn;` |
|       283 |  2786 | `	pBodyEnd = pGen->pEnd;` |
|         - |  2787 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|         - |  2788 | `	 * recursively collect free-variable references from the body. The scan` |
|         - |  2789 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|         - |  2790 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|       283 |  2791 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|         - |  2792 | `	{` |
|       283 |  2793 | `		SyString *aShadow = 0;` |
|       283 |  2794 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|       283 |  2795 | `		if( nShadow > 0 ){` |
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
|       423 |  2807 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|       140 |  2808 | `			aShadow,nShadow);` |
|       283 |  2809 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2810 | `			return SXERR_ABORT;` |
|         - |  2811 | `		}` |
|         - |  2812 | `	}` |
|         - |  2813 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|         - |  2814 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|         - |  2815 | `	 * captured value is silently dropped when the enclosing scope has no` |
|         - |  2816 | `	 * $this. */` |
|       283 |  2817 | `	if( !bStatic ){` |
|         - |  2818 | `		char *zThisDup;` |
|       277 |  2819 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|       277 |  2820 | `		if( zThisDup == 0 ){` |
|       ! 0 |  2821 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2822 | `				"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2823 | `			return SXERR_ABORT;` |
|         - |  2824 | `		}` |
|       277 |  2825 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       277 |  2826 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|       277 |  2827 | `		sEnv.nIdx = SXU32_HIGH;` |
|       277 |  2828 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       277 |  2829 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|       277 |  2830 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       137 |  2831 | `	}` |
|         - |  2832 | `	/* Arrow functions are always closures */` |
|       283 |  2833 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|         - |  2834 | `	/* Compile the body expression as an implicit return */` |
|       423 |  2835 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       140 |  2836 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|       283 |  2837 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  2838 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2839 | `			"PH7 engine is running out-of-memory");` |
|       ! 0 |  2840 | `		return SXERR_ABORT;` |
|         - |  2841 | `	}` |
|       283 |  2842 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       283 |  2843 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       283 |  2844 | `	pSavedEnd = pGen->pEnd;` |
|       283 |  2845 | `	pGen->pIn = pBodyStart;` |
|       283 |  2846 | `	pGen->pEnd = pBodyEnd;` |
|       283 |  2847 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       283 |  2848 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2849 | `		return SXERR_ABORT;` |
|         - |  2850 | `	}` |
|         - |  2851 | `	/* The cursor stopped just past the body expression */` |
|       283 |  2852 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|         - |  2853 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|         - |  2854 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|         - |  2855 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|         - |  2856 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|       283 |  2857 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       283 |  2858 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       283 |  2859 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       283 |  2860 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       283 |  2861 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - |  2862 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|       283 |  2863 | `	pGen->pIn = pBodyEnd;` |
|       283 |  2864 | `	pGen->pEnd = pSavedEnd;` |
|         - |  2865 | `	/* Emit the load-closure instruction */` |
|       283 |  2866 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|       283 |  2867 | `	return SXRET_OK;` |
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
|         - |  3114 | `	static const SyString sName = { "shell_exec", sizeof("shell_exec")-1 };` |
|         6 |  3115 | `	sxu32 nIdx = 0;` |
|         - |  3116 | `	sxi32 rc;` |
|         - |  3117 | `	/*` |
|         - |  3118 | ``	 * `cmd` IS shell_exec("cmd") in php — it interpolates like a double-quoted string,`` |
|         - |  3119 | `	 * runs the command and yields its output. PH7 refused to run it at all (TICKET` |
|         - |  3120 | `	 * 1433-40) and quietly evaluated to NULL. php 8.5 deprecates the syntax but still` |
|         - |  3121 | `	 * executes it, so compile it to the real call and say what php says.` |
|         - |  3122 | `	 */` |
|         6 |  3123 | `	PH7_GenCompileError(&(*pGen),8192 /* E_DEPRECATED */,pGen->pIn->nLine,` |
|         - |  3124 | ``		"The backtick (`) operator is deprecated, use shell_exec() instead");`` |
|         - |  3125 | `	/* The body interpolates exactly like a double-quoted string */` |
|         6 |  3126 | `	pGen->pIn->nType &= ~PH7_TK_BSTR;` |
|         6 |  3127 | `	pGen->pIn->nType \|= PH7_TK_DSTR;` |
|         6 |  3128 | `	rc = PH7_CompileString(&(*pGen),iCompileFlag);` |
|         6 |  3129 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3130 | `		return rc;` |
|         - |  3131 | `	}` |
|         - |  3132 | `	/* ... and the command string is then handed to shell_exec() */` |
|         6 |  3133 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|         6 |  3134 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|         6 |  3135 | `		if( pObj == 0 ){` |
|       ! 0 |  3136 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  3137 | `			return SXERR_ABORT;` |
|         - |  3138 | `		}` |
|         6 |  3139 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|         6 |  3140 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|         2 |  3141 | `	}` |
|         6 |  3142 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         6 |  3143 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|         6 |  3144 | `	return SXRET_OK;` |
|         4 |  3145 | `}` |
|         - |  3146 | `/*` |
|         - |  3147 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|         - |  3148 | ` * construct.` |
|         - |  3149 | ` */` |
|        70 |  3150 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3151 | `{` |
|         - |  3152 | `	SyString *pName;` |
|         - |  3153 | `	sxu32 nKeyID;` |
|         - |  3154 | `	sxi32 rc;` |
|         - |  3155 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|        75 |  3156 | `	pName = &pGen->pIn->sData;` |
|        75 |  3157 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        75 |  3158 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|        75 |  3159 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|         9 |  3160 | `		SyToken *pTmp,*pNext = 0;` |
|         - |  3161 | `		/* Compile arguments one after one */` |
|         9 |  3162 | `		pTmp = pGen->pEnd;` |
|         - |  3163 | `		/* Symisc eXtension to the PHP programming language:` |
|         - |  3164 | `		 * 'echo' can be used in the context of a function which` |
|         - |  3165 | `		 *  mean that the following expression is valid:` |
|         - |  3166 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|         - |  3167 | `		 */` |
|         9 |  3168 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|        17 |  3169 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|         9 |  3170 | `			if( pGen->pIn < pNext ){` |
|         9 |  3171 | `				pGen->pEnd = pNext;` |
|         9 |  3172 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|         9 |  3173 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  3174 | `					return SXERR_ABORT;` |
|         - |  3175 | `				}` |
|         9 |  3176 | `				if( rc != SXERR_EMPTY ){` |
|         - |  3177 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|         - |  3178 | `					 * without the overhead of a function call.` |
|         - |  3179 | `					 * This is a very powerful optimization that improve` |
|         - |  3180 | `					 * performance greatly.` |
|         - |  3181 | `					 */` |
|         9 |  3182 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|         4 |  3183 | `				}` |
|         4 |  3184 | `			}` |
|         - |  3185 | `			/* Jump trailing commas */` |
|         9 |  3186 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|       ! 0 |  3187 | `				pNext++;` |
|       ! 0 |  3188 | `			}` |
|         9 |  3189 | `			pGen->pIn = pNext;` |
|         1 |  3190 | `		}` |
|         - |  3191 | `		/* Restore token stream */` |
|         9 |  3192 | `		pGen->pEnd = pTmp;` |
|         5 |  3193 | `	}else{` |
|        67 |  3194 | `		sxi32 nArg = 0;` |
|        67 |  3195 | `		sxu32 nIdx = 0;` |
|        67 |  3196 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|        67 |  3197 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3198 | `			return SXERR_ABORT;` |
|        67 |  3199 | `		}else if(rc != SXERR_EMPTY ){` |
|        67 |  3200 | `			nArg = 1;` |
|        31 |  3201 | `		}` |
|        67 |  3202 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|         - |  3203 | `			ph7_value *pObj;` |
|         - |  3204 | `			/* Emit the call instruction */` |
|        31 |  3205 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        31 |  3206 | `			if( pObj == 0 ){` |
|       ! 0 |  3207 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3208 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  3209 | `				return SXERR_ABORT;` |
|         - |  3210 | `			}` |
|        31 |  3211 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|         - |  3212 | `			/* Install in the literal table */` |
|        31 |  3213 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|        13 |  3214 | `		}` |
|         - |  3215 | `		/* Emit the call instruction */` |
|        67 |  3216 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        67 |  3217 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|         - |  3218 | `	}` |
|         - |  3219 | `	/* Node successfully compiled */` |
|        75 |  3220 | `	return SXRET_OK;` |
|        40 |  3221 | `}` |
|         - |  3222 | `/*` |
|         - |  3223 | ` * Compile a node holding a variable declaration.` |
|         - |  3224 | ` * According to the PHP language reference` |
|         - |  3225 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|         - |  3226 | ` *  The variable name is case-sensitive.` |
|         - |  3227 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|         - |  3228 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|         - |  3229 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|         - |  3230 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|         - |  3231 | ` *  Note: $this is a special variable that can't be assigned.` |
|         - |  3232 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|         - |  3233 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|         - |  3234 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|         - |  3235 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|         - |  3236 | ` *  the chapter on Expressions.` |
|         - |  3237 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|         - |  3238 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|         - |  3239 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|         - |  3240 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|         - |  3241 | ` *  is being assigned (the source variable).` |
|         - |  3242 | ` */` |
|  15900198 |  3243 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3244 | `{` |
|  15900203 |  3245 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  3246 | `	sxi32 iVv;` |
|         - |  3247 | `	sxi32 iP1;` |
|         - |  3248 | `	void *p3;` |
|         - |  3249 | `	sxi32 rc;` |
|  15900203 |  3250 | `	iVv = -1; /* Variable variable counter */` |
|  31800413 |  3251 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  15900215 |  3252 | `		pGen->pIn++;` |
|  15900215 |  3253 | `		iVv++;` |
|         5 |  3254 | `	}` |
|  15900203 |  3255 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|         - |  3256 | `		/* Invalid variable name */` |
|       ! 0 |  3257 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable or \"{\" or \"$\"");` |
|       ! 0 |  3258 | `		if( rc == SXERR_ABORT ){` |
|         - |  3259 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3260 | `			return SXERR_ABORT;` |
|         - |  3261 | `		}` |
|       ! 0 |  3262 | `		return SXRET_OK;` |
|         - |  3263 | `	}` |
|  15900203 |  3264 | `	p3  = 0;` |
|  15900203 |  3265 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|         - |  3266 | `		/* Dynamic variable creation */` |
|        21 |  3267 | `		pGen->pIn++;  /* Jump the open curly */` |
|        21 |  3268 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|        21 |  3269 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - |  3270 | `			/* Empty expression */` |
|         - |  3271 | `			{` |
|         - |  3272 | `			/* php names the offending token and, for an empty "${}", stops there:` |
|         - |  3273 | `			 * the "expecting" tail only appears when something could still follow. */` |
|         3 |  3274 | `			SyToken *pBad = pGen->pIn < pGen->pEnd ? pGen->pIn : 0;` |
|         3 |  3275 | `			PH7_GenSyntaxError(&(*pGen),pBad,` |
|         1 |  3276 | `				(pBad && (pBad->nType & PH7_TK_CCB)) ? 0 : "variable or \"{\" or \"$\"");` |
|         - |  3277 | `			}` |
|         3 |  3278 | `			return SXRET_OK;` |
|         - |  3279 | `		}` |
|         - |  3280 | `		/* Compile the expression holding the variable name */` |
|        18 |  3281 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        18 |  3282 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3283 | `			return SXERR_ABORT;` |
|        18 |  3284 | `		}else if( rc == SXERR_EMPTY ){` |
|         3 |  3285 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|         3 |  3286 | `			return SXRET_OK;` |
|         - |  3287 | `		}` |
|         8 |  3288 | `	}else{` |
|         - |  3289 | `		SyHashEntry *pEntry;` |
|         - |  3290 | `		SyString *pName;` |
|  15900185 |  3291 | `		char *zName = 0;` |
|         - |  3292 | `		/* Extract variable name */` |
|  15900185 |  3293 | `		pName = &pGen->pIn->sData;` |
|         - |  3294 | `		/* Advance the stream cursor */` |
|  15900185 |  3295 | `		pGen->pIn++;` |
|  15900185 |  3296 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  15900185 |  3297 | `		if( pEntry == 0 ){` |
|         - |  3298 | `			/* Duplicate name */` |
|    918167 |  3299 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    918167 |  3300 | `			if( zName == 0 ){` |
|       ! 0 |  3301 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3302 | `				return SXERR_ABORT;` |
|         - |  3303 | `			}` |
|         - |  3304 | `			/* Install in the hashtable */` |
|    918167 |  3305 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|    459086 |  3306 | `		}else{` |
|         - |  3307 | `			/* Name already available */` |
|  14982023 |  3308 | `			zName = (char *)pEntry->pUserData;` |
|         - |  3309 | `		}` |
|  15900185 |  3310 | `		p3 = (void *)zName;` |
|         - |  3311 | `	}` |
|  15900199 |  3312 | `	iP1 = 0;` |
|  15900199 |  3313 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|   4734587 |  3314 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|         - |  3315 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|   4730767 |  3316 | `			iP1 = 1;` |
|   2365381 |  3317 | `		}` |
|   2367291 |  3318 | `	}` |
|         - |  3319 | `	/* Emit the load instruction */` |
|  15900199 |  3320 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  15900211 |  3321 | `	while( iVv > 0 ){` |
|        13 |  3322 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|        13 |  3323 | `		iVv--;` |
|         1 |  3324 | `	}` |
|         - |  3325 | `	/* Node successfully compiled */` |
|  15900199 |  3326 | `	return SXRET_OK;` |
|   7950104 |  3327 | `}` |
|         - |  3328 | `/*` |
|         - |  3329 | ` * Load a literal.` |
|         - |  3330 | ` */` |
|  10772874 |  3331 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|         5 |  3332 | `{` |
|  10772879 |  3333 | `	SyToken *pToken = pGen->pIn;` |
|         - |  3334 | `	ph7_value *pObj;` |
|         - |  3335 | `	SyString *pStr;` |
|         - |  3336 | `	sxu32 nIdx;` |
|         - |  3337 | `	/* Extract token value */` |
|  10772879 |  3338 | `	pStr = &pToken->sData;` |
|         - |  3339 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  10772879 |  3340 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   2080341 |  3341 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|         - |  3342 | `			/* NULL constant are always indexed at 0 */` |
|    866425 |  3343 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|    866425 |  3344 | `			return SXRET_OK;` |
|   1213921 |  3345 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|         - |  3346 | `			/* TRUE constant are always indexed at 1 */` |
|    274433 |  3347 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|    274433 |  3348 | `			return SXRET_OK;` |
|         5 |  3349 | `		}` |
|  10062036 |  3350 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   1799498 |  3351 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|         - |  3352 | `			/* FALSE constant are always indexed at 2 */` |
|    615601 |  3353 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|    615601 |  3354 | `			return SXRET_OK;` |
|   8463139 |  3355 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|    772384 |  3356 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|         - |  3357 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|     11399 |  3358 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     11399 |  3359 | `			if( pObj == 0 ){` |
|       ! 0 |  3360 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3361 | `				return SXERR_ABORT;` |
|         - |  3362 | `			}` |
|     11399 |  3363 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|         - |  3364 | `			/* Emit the load constant instruction */` |
|     11399 |  3365 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     11399 |  3366 | `			return SXRET_OK;` |
|   8157067 |  3367 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|    183028 |  3368 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|         - |  3369 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|         7 |  3370 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|         7 |  3371 | `			if( pObj == 0 ){` |
|       ! 0 |  3372 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3373 | `				return SXERR_ABORT;` |
|         - |  3374 | `			}` |
|         7 |  3375 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - |  3376 | `				SyString sNs;` |
|         7 |  3377 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         7 |  3378 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|         4 |  3379 | `			}else{` |
|       ! 0 |  3380 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|         - |  3381 | `			}` |
|         7 |  3382 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         7 |  3383 | `			return SXRET_OK;` |
|   8161058 |  3384 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|    376759 |  3385 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|   8251257 |  3386 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|    371444 |  3387 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|        11 |  3388 | `			GenBlock *pBlock = pGen->pCurrent;` |
|         - |  3389 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|        21 |  3390 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|         - |  3391 | `				/* Point to the upper block */` |
|        11 |  3392 | `				pBlock = pBlock->pParent;` |
|         1 |  3393 | `			}` |
|        11 |  3394 | `			if( pBlock == 0 ){` |
|         - |  3395 | `				/* Called in the global scope,load NULL */` |
|         5 |  3396 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         3 |  3397 | `			}else{` |
|         - |  3398 | `				/* Extract the target function/method */` |
|         7 |  3399 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|         7 |  3400 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|         - |  3401 | `					/* Not a class method,Load null */` |
|         3 |  3402 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         2 |  3403 | `				}else{` |
|         5 |  3404 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|         5 |  3405 | `					if( pObj == 0 ){` |
|       ! 0 |  3406 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3407 | `						return SXERR_ABORT;` |
|         - |  3408 | `					}` |
|         5 |  3409 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|         - |  3410 | `					/* Emit the load constant instruction */` |
|         5 |  3411 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |  3412 | `				}` |
|         - |  3413 | `			}` |
|        11 |  3414 | `			return SXRET_OK;` |
|         - |  3415 | `	}` |
|         - |  3416 | `	/* Query literal table */` |
|   9005025 |  3417 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|         - |  3418 | `		ph7_value *pLitObj;` |
|         - |  3419 | `		/* Unknown literal,install it in the literal table */` |
|   1725963 |  3420 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   1725963 |  3421 | `		if( pLitObj == 0 ){` |
|       ! 0 |  3422 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  3423 | `			return SXERR_ABORT;` |
|         - |  3424 | `		}` |
|   1725963 |  3425 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|   1725963 |  3426 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|    862979 |  3427 | `	}` |
|         - |  3428 | `	/* Emit the load constant instruction */` |
|   9005025 |  3429 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|   9005025 |  3430 | `	return SXRET_OK;` |
|   5386442 |  3431 | `}` |
|         - |  3432 | `/*` |
|         - |  3433 | ` * Resolve a namespace path or simply load a literal.` |
|         - |  3434 | ` * If the token stream contains namespace separators (backslashes),` |
|         - |  3435 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|         - |  3436 | ` * Otherwise, load the simple literal directly.` |
|         - |  3437 | ` */` |
|  10776718 |  3438 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|         5 |  3439 | `{` |
|         - |  3440 | `	sxi32 rc;` |
|  10776723 |  3441 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  3442 | `		return SXRET_OK;` |
|         - |  3443 | `	}` |
|         - |  3444 | `	/* Check if this is a multi-token namespace path */` |
|  10776723 |  3445 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|         - |  3446 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      3849 |  3447 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      3849 |  3448 | `		int isAbsolute = 0;` |
|      3849 |  3449 | `		SyBlobReset(pWorker);` |
|         - |  3450 | `		/* Check for leading backslash (absolute path) */` |
|      3849 |  3451 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      3847 |  3452 | `			isAbsolute = 1;` |
|      3847 |  3453 | `			pGen->pIn++; /* Skip leading backslash */` |
|      1921 |  3454 | `		}` |
|         - |  3455 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      3849 |  3456 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         3 |  3457 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         3 |  3458 | `			SyBlobAppend(pWorker,"\\",1);` |
|         1 |  3459 | `		}` |
|         - |  3460 | `		/* Collect all path components */` |
|      3957 |  3461 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|      3957 |  3462 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        58 |  3463 | `				SyBlobAppend(pWorker,"\\",1);` |
|        31 |  3464 | `			}else{` |
|      3903 |  3465 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|         - |  3466 | `			}` |
|      3957 |  3467 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      3849 |  3468 | `				pGen->pIn++;` |
|      3849 |  3469 | `				break;` |
|         - |  3470 | `			}` |
|       112 |  3471 | `			pGen->pIn++;` |
|         4 |  3472 | `		}` |
|      3849 |  3473 | `		if( SyBlobLength(pWorker) > 0 ){` |
|         - |  3474 | `			ph7_value *pObj;` |
|         - |  3475 | `			SyString sPath;` |
|         - |  3476 | `			sxu32 nIdx;` |
|      3849 |  3477 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|         - |  3478 | `			/* Install in the literal table */` |
|      3849 |  3479 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      3819 |  3480 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3819 |  3481 | `				if( pObj == 0 ){` |
|       ! 0 |  3482 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  3483 | `					return SXERR_ABORT;` |
|         - |  3484 | `				}` |
|      3819 |  3485 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      3819 |  3486 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1907 |  3487 | `			}` |
|         - |  3488 | `			/* Emit the load constant instruction.` |
|         - |  3489 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|         - |  3490 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|      5771 |  3491 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|      1922 |  3492 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|      1922 |  3493 | `				nIdx,0,0);` |
|      3849 |  3494 | `			return SXRET_OK;` |
|         - |  3495 | `		}` |
|       ! 0 |  3496 | `	}` |
|         - |  3497 | `	/* Single-token literal: load directly */` |
|  10772879 |  3498 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  10772879 |  3499 | `	return rc;` |
|   5388364 |  3500 | `}` |
|         - |  3501 | `/*` |
|         - |  3502 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|         - |  3503 | ` */` |
|         - |  3504 | `/*` |
|         - |  3505 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|         - |  3506 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|         - |  3507 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|         - |  3508 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|         - |  3509 | ` */` |
|       ! 0 |  3510 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       ! 0 |  3511 | `{` |
|       ! 0 |  3512 | `	SXUNUSED(iCompileFlag);` |
|       ! 0 |  3513 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|         - |  3514 | `		"Cannot use the first-class callable syntax '...' here");` |
|       ! 0 |  3515 | `	return SXERR_SYNTAX;` |
|       ! 0 |  3516 | `}` |
|  10776718 |  3517 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3518 | `{` |
|         - |  3519 | `	sxi32 rc;` |
|  10776723 |  3520 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  10776723 |  3521 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3522 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  3523 | `		return rc;` |
|         - |  3524 | `	}` |
|         - |  3525 | `	/* Node successfully compiled */` |
|  10776723 |  3526 | `	return SXRET_OK;` |
|   5388364 |  3527 | `}` |
|         - |  3528 | `/*` |
|         - |  3529 | ` * Recover from a compile-time error. In other words synchronize` |
|         - |  3530 | ` * the token stream cursor with the first semi-colon seen.` |
|         - |  3531 | ` */` |
|         8 |  3532 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|         1 |  3533 | `{` |
|         - |  3534 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|        17 |  3535 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|         9 |  3536 | `		pGen->pIn++;` |
|         1 |  3537 | `	}` |
|         9 |  3538 | `	return SXRET_OK;` |
|         1 |  3539 | `}` |
|         - |  3540 | `/*` |
|         - |  3541 | ` * Check if the given identifier name is reserved or not.` |
|         - |  3542 | ` * Return TRUE if reserved.FALSE otherwise.` |
|         - |  3543 | ` */` |
|    288716 |  3544 | `static int GenStateIsReservedConstant(SyString *pName)` |
|         5 |  3545 | `{` |
|    288721 |  3546 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      3845 |  3547 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|         3 |  3548 | `			return TRUE;` |
|      3843 |  3549 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|         6 |  3550 | `			return TRUE;` |
|         5 |  3551 | `		}` |
|    286798 |  3552 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|      7617 |  3553 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|         3 |  3554 | `			return TRUE;` |
|         - |  3555 | `		}` |
|      3805 |  3556 | `	}` |
|         - |  3557 | `	/* Not a reserved constant */` |
|    288713 |  3558 | `	return FALSE;` |
|    144363 |  3559 | `}` |
|         - |  3560 | `/*` |
|         - |  3561 | ` * Compile the 'const' statement.` |
|         - |  3562 | ` * According to the PHP language reference` |
|         - |  3563 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|         - |  3564 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|         - |  3565 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|         - |  3566 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|         - |  3567 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|         - |  3568 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|         - |  3569 | ` *  Syntax` |
|         - |  3570 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|         - |  3571 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|         - |  3572 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|         - |  3573 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|         - |  3574 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|         - |  3575 | ` *  to get a list of all defined constants.` |
|         - |  3576 | ` *` |
|         - |  3577 | ` * Symisc eXtension.` |
|         - |  3578 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|         - |  3579 | ` *  would allow only simple scalar value.` |
|         - |  3580 | ` *  Example` |
|         - |  3581 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|         - |  3582 | ` *    Refer to the official documentation for more information on this feature.` |
|         - |  3583 | ` */` |
|        48 |  3584 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|         5 |  3585 | `{` |
|         - |  3586 | `	SySet *pConsCode,*pInstrContainer;` |
|        53 |  3587 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  3588 | `	SyString *pName;` |
|         - |  3589 | `	sxi32 rc;` |
|        53 |  3590 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        53 |  3591 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  3592 | `		/* Invalid constant name */` |
|         9 |  3593 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"identifier");` |
|         9 |  3594 | `		if( rc == SXERR_ABORT ){` |
|         - |  3595 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3596 | `			return SXERR_ABORT;` |
|         - |  3597 | `		}` |
|         9 |  3598 | `		goto Synchronize;` |
|         - |  3599 | `	}` |
|         - |  3600 | `	/* Peek constant name */` |
|        46 |  3601 | `	pName = &pGen->pIn->sData;` |
|         - |  3602 | `	/* Make sure the constant name isn't reserved */` |
|        46 |  3603 | `	if( GenStateIsReservedConstant(pName) ){` |
|         - |  3604 | `		/* Reserved constant */` |
|        10 |  3605 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Cannot redeclare constant '%z'",pName);` |
|        10 |  3606 | `		if( rc == SXERR_ABORT ){` |
|         - |  3607 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3608 | `			return SXERR_ABORT;` |
|         - |  3609 | `		}` |
|        10 |  3610 | `		goto Synchronize;` |
|         - |  3611 | `	}` |
|        37 |  3612 | `	pGen->pIn++;` |
|        37 |  3613 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|         - |  3614 | `		/* Invalid statement*/` |
|         6 |  3615 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"=\"");` |
|         6 |  3616 | `		if( rc == SXERR_ABORT ){` |
|         - |  3617 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3618 | `			return SXERR_ABORT;` |
|         - |  3619 | `		}` |
|         6 |  3620 | `		goto Synchronize;` |
|         - |  3621 | `	}` |
|        32 |  3622 | `	pGen->pIn++; /*Jump the equal sign */` |
|         - |  3623 | `	/* Allocate a new constant value container */` |
|        32 |  3624 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|        32 |  3625 | `	if( pConsCode == 0 ){` |
|       ! 0 |  3626 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3627 | `		return SXERR_ABORT;` |
|         - |  3628 | `	}` |
|        32 |  3629 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - |  3630 | `	/* Swap bytecode container */` |
|        32 |  3631 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        32 |  3632 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|         - |  3633 | `	/* Compile constant value */` |
|        32 |  3634 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  3635 | `	/* Emit the done instruction */` |
|        32 |  3636 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        32 |  3637 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        32 |  3638 | `	if( rc == SXERR_ABORT ){` |
|         - |  3639 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  3640 | `		return SXERR_ABORT;` |
|         - |  3641 | `	}` |
|        32 |  3642 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|         - |  3643 | `	/* Register the constant with namespace-qualified name */` |
|         - |  3644 | `	{` |
|         - |  3645 | `		SyBlob sFQN;` |
|         - |  3646 | `		SyString sFQNStr;` |
|        32 |  3647 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        32 |  3648 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|        32 |  3649 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        47 |  3650 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|        30 |  3651 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|        32 |  3652 | `		if( rc == SXRET_OK && SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - |  3653 | ``			/* php 8.5: attributes on `const` statements — attach the pending`` |
|         - |  3654 | `			 * groups to the registered constant record for Reflection. */` |
|         7 |  3655 | `			SyHashEntry *pCEntry = SyHashGet(&pGen->pVm->hConstant,` |
|         4 |  3656 | `				SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|         5 |  3657 | `			if( pCEntry ){` |
|         5 |  3658 | `				ph7_constant *pRegCons = (ph7_constant *)pCEntry->pUserData;` |
|         5 |  3659 | `				if( GenStateConsumeAttrs(&(*pGen),&pRegCons->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  3660 | `					SyBlobRelease(&sFQN);` |
|       ! 0 |  3661 | `					return SXERR_ABORT;` |
|         - |  3662 | `				}` |
|         2 |  3663 | `			}` |
|         2 |  3664 | `		}` |
|        32 |  3665 | `		SyBlobRelease(&sFQN);` |
|         - |  3666 | `	}` |
|        32 |  3667 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3668 | `		SySetRelease(pConsCode);` |
|       ! 0 |  3669 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|       ! 0 |  3670 | `	}` |
|        32 |  3671 | `	return SXRET_OK;` |
|         9 |  3672 | `Synchronize:` |
|         - |  3673 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|        60 |  3674 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        41 |  3675 | `		pGen->pIn++;` |
|         3 |  3676 | `	}` |
|        22 |  3677 | `	return SXRET_OK;` |
|        29 |  3678 | `}` |
|         - |  3679 | `/*` |
|         - |  3680 | ` * Compile the 'continue' statement.` |
|         - |  3681 | ` * According to the PHP language reference` |
|         - |  3682 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|         - |  3683 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|         - |  3684 | ` *  iteration.` |
|         - |  3685 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|         - |  3686 | ` *  the purposes of continue.` |
|         - |  3687 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|         - |  3688 | ` *  of enclosing loops it should skip to the end of.` |
|         - |  3689 | ` *  Note:` |
|         - |  3690 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|         - |  3691 | ` */` |
|         - |  3692 | `/*` |
|         - |  3693 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|         - |  3694 | ` * block and the target loop block. This ensures finally blocks run when` |
|         - |  3695 | ` * break/continue crosses a try boundary.` |
|         - |  3696 | ` *` |
|         - |  3697 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|         - |  3698 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|         - |  3699 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|         - |  3700 | ` */` |
|    114034 |  3701 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|         5 |  3702 | `{` |
|    114039 |  3703 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    114039 |  3704 | `	int nInlineTry = 0;` |
|    531845 |  3705 | `	while( pBlock && pBlock != pTarget ){` |
|    417811 |  3706 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|         6 |  3707 | `			if( pBlock->pUserData ){` |
|         - |  3708 | `				/* A try block with an exception context. In a generator its catch/finally` |
|         - |  3709 | `				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that` |
|         - |  3710 | `				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.` |
|         - |  3711 | `				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */` |
|         6 |  3712 | `				if( pGen->bInGenerator ){` |
|         3 |  3713 | `					nInlineTry++;` |
|         2 |  3714 | `				}else{` |
|         3 |  3715 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|         - |  3716 | `				}` |
|         4 |  3717 | `			}else{` |
|         - |  3718 | `				/* A catch/finally block compiled into a separate bytecode container` |
|         - |  3719 | `				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */` |
|       ! 0 |  3720 | `				break;` |
|         - |  3721 | `			}` |
|         2 |  3722 | `		}` |
|    417811 |  3723 | `		pBlock = pBlock->pParent;` |
|         5 |  3724 | `	}` |
|    114039 |  3725 | `	return nInlineTry;` |
|         5 |  3726 | `}` |
|     56990 |  3727 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|         5 |  3728 | `{` |
|         - |  3729 | `	GenBlock *pLoop; /* Target loop */` |
|         - |  3730 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|         - |  3731 | `	sxu32 nLineLocal;` |
|         - |  3732 | `	sxi32 rc;` |
|     56995 |  3733 | `	nLineLocal = pGen->pIn->nLine;` |
|     56995 |  3734 | `	iLevel = 0;` |
|         - |  3735 | `	/* Jump the 'continue' keyword */` |
|     56995 |  3736 | `	pGen->pIn++;` |
|     56995 |  3737 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|         - |  3738 | `		/* optional numeric argument which tells us how many levels` |
|         - |  3739 | `		 * of enclosing loops we should skip to the end of.` |
|         - |  3740 | `		 */` |
|         - |  3741 | `		char zScratch[GEN_NUM_SCRATCH];` |
|        17 |  3742 | `		char *zAlloc = 0;` |
|         - |  3743 | `		SyString sNum;` |
|        17 |  3744 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|        17 |  3745 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3746 | `			return SXERR_ABORT;` |
|         - |  3747 | `		}` |
|        17 |  3748 | `		if( rc == SXRET_OK ){` |
|        20 |  3749 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|        12 |  3750 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|        14 |  3751 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  3752 | `				return SXERR_ABORT;` |
|         - |  3753 | `			}` |
|        14 |  3754 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|        14 |  3755 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         6 |  3756 | `		}` |
|        17 |  3757 | `		if( iLevel < 2 ){` |
|         3 |  3758 | `			iLevel = 0;` |
|         1 |  3759 | `		}` |
|        17 |  3760 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|         7 |  3761 | `	}` |
|         - |  3762 | `	/* Point to the target loop */` |
|     56995 |  3763 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     56995 |  3764 | `	if( pLoop == 0 ){` |
|         - |  3765 | `		/* Illegal continue */` |
|        12 |  3766 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"'continue' not in the 'loop' or 'switch' context");` |
|        12 |  3767 | `		if( rc == SXERR_ABORT ){` |
|         - |  3768 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3769 | `			return SXERR_ABORT;` |
|         - |  3770 | `		}` |
|         7 |  3771 | `	}else{` |
|     56985 |  3772 | `		sxu32 nInstrIdx = 0;` |
|         - |  3773 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     56985 |  3774 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|         - |  3775 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|         - |  3776 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|     56985 |  3777 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|     56985 |  3778 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|         - |  3779 | ``			/* `continue` inside a switch acts like `break` — which is almost never what`` |
|         - |  3780 | `			 * the author meant, so php 7.3+ says so at compile time. The generated jump` |
|         - |  3781 | `			 * is unchanged; only the diagnostic was missing. An explicit level` |
|         - |  3782 | ``			 * (`continue 2`) targets the enclosing loop and stays silent. */`` |
|         5 |  3783 | `			if( iLevel < 1 ){` |
|         5 |  3784 | `				PH7_GenCompileError(&(*pGen),E_WARNING,nLineLocal,` |
|         - |  3785 | `					"\"continue\" targeting switch is equivalent to \"break\"."` |
|         - |  3786 | `					" Did you mean to use \"continue 2\"?");` |
|         2 |  3787 | `			}` |
|         5 |  3788 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|         5 |  3789 | `			if( rc == SXRET_OK ){` |
|         5 |  3790 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|         2 |  3791 | `			}` |
|         3 |  3792 | `		}else{` |
|         - |  3793 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|     56981 |  3794 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|     56981 |  3795 | `			if( pLoop->bPostContinue == TRUE ){` |
|         - |  3796 | `				JumpFixup sJumpFix;` |
|         - |  3797 | `				/* Post-continue */` |
|     18997 |  3798 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|     18997 |  3799 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|     18997 |  3800 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|      9496 |  3801 | `			}` |
|         - |  3802 | `		}` |
|         - |  3803 | `	}` |
|     56995 |  3804 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  3805 | `		/* Not so fatal,emit a warning only */` |
|       ! 0 |  3806 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|       ! 0 |  3807 | `	}` |
|         - |  3808 | `	/* Statement successfully compiled */` |
|     56995 |  3809 | `	return SXRET_OK;` |
|     28500 |  3810 | `}` |
|         - |  3811 | `/*` |
|         - |  3812 | ` * Compile the 'break' statement.` |
|         - |  3813 | ` * According to the PHP language reference` |
|         - |  3814 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|         - |  3815 | ` *  structure.` |
|         - |  3816 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|         - |  3817 | ` *  enclosing structures are to be broken out of.` |
|         - |  3818 | ` */` |
|     57070 |  3819 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|         5 |  3820 | `{` |
|         - |  3821 | `	GenBlock *pLoop; /* Target loop */` |
|         - |  3822 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|         - |  3823 | `	sxi32 rc;` |
|     57075 |  3824 | `	iLevel = 0;` |
|         - |  3825 | `	/* Jump the 'break' keyword */` |
|     57075 |  3826 | `	pGen->pIn++;` |
|     57075 |  3827 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|         - |  3828 | `		/* optional numeric argument which tells us how many levels` |
|         - |  3829 | `		 * of enclosing loops we should skip to the end of.` |
|         - |  3830 | `		 */` |
|         - |  3831 | `		char zScratch[GEN_NUM_SCRATCH];` |
|        18 |  3832 | `		char *zAlloc = 0;` |
|         - |  3833 | `		SyString sNum;` |
|        18 |  3834 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|        18 |  3835 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3836 | `			return SXERR_ABORT;` |
|         - |  3837 | `		}` |
|        18 |  3838 | `		if( rc == SXRET_OK ){` |
|        21 |  3839 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|        12 |  3840 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|        15 |  3841 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  3842 | `				return SXERR_ABORT;` |
|         - |  3843 | `			}` |
|        15 |  3844 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|        15 |  3845 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         6 |  3846 | `		}` |
|        18 |  3847 | `		if( iLevel < 2 ){` |
|         3 |  3848 | `			iLevel = 0;` |
|         1 |  3849 | `		}` |
|        18 |  3850 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|         7 |  3851 | `	}` |
|         - |  3852 | `	/* Extract the target loop */` |
|     57075 |  3853 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     57075 |  3854 | `	if( pLoop == 0 ){` |
|         - |  3855 | `		/* Illegal break */` |
|        20 |  3856 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"'break' not in the 'loop' or 'switch' context");` |
|        20 |  3857 | `		if( rc == SXERR_ABORT ){` |
|         - |  3858 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3859 | `			return SXERR_ABORT;` |
|         - |  3860 | `		}` |
|        12 |  3861 | `	}else{` |
|         - |  3862 | `		sxu32 nInstrIdx;` |
|         - |  3863 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     57059 |  3864 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|         - |  3865 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|     57059 |  3866 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|     57059 |  3867 | `		if( rc == SXRET_OK ){` |
|         - |  3868 | `			/* Fix the jump later when the jump destination is resolved */` |
|     57059 |  3869 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|     28527 |  3870 | `		}` |
|         - |  3871 | `	}` |
|     57075 |  3872 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  3873 | `		/* Not so fatal,emit a warning only */` |
|       ! 0 |  3874 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|       ! 0 |  3875 | `	}` |
|         - |  3876 | `	/* Statement successfully compiled */` |
|     57075 |  3877 | `	return SXRET_OK;` |
|     28540 |  3878 | `}` |
|         - |  3879 | `/*` |
|         - |  3880 | ` * Compile or record a label.` |
|         - |  3881 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|         - |  3882 | ` * Example` |
|         - |  3883 | ` *  goto LABEL;` |
|         - |  3884 | ` *   echo 'Foo';` |
|         - |  3885 | ` *  LABEL:` |
|         - |  3886 | ` *   echo 'Bar';` |
|         - |  3887 | ` */` |
|       112 |  3888 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|         5 |  3889 | `{` |
|         - |  3890 | `	GenBlock *pBlock;` |
|         - |  3891 | `	Label sLabel;` |
|         - |  3892 | `	/* php places NO restriction on where a label may be DEFINED — inside a loop, a switch` |
|         - |  3893 | `	 * or a try{} is all fine. The only rule is on the jump: you may not goto INTO a loop` |
|         - |  3894 | `	 * or switch from outside it, which is checked once the labels are all known (see` |
|         - |  3895 | `	 * GenStateFixJumps). Record the loop this label sits in so that check can run. */` |
|         - |  3896 | `	{` |
|       117 |  3897 | `		SyString *pTarget = &pGen->pIn->sData;` |
|         - |  3898 | `		char *zDup;` |
|         - |  3899 | `		/* Initialize label fields */` |
|       117 |  3900 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|         - |  3901 | `		/* Duplicate label name */` |
|       117 |  3902 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|       117 |  3903 | `		if( zDup == 0 ){` |
|       ! 0 |  3904 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  3905 | `			return SXERR_ABORT;` |
|         - |  3906 | `		}` |
|       117 |  3907 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|       117 |  3908 | `		sLabel.bRef  = FALSE;` |
|       117 |  3909 | `		sLabel.nLine = pGen->pIn->nLine;` |
|       117 |  3910 | `		sLabel.nLoopId = pGen->nCurLoopId;` |
|       117 |  3911 | `		pBlock = pGen->pCurrent;` |
|       233 |  3912 | `		while( pBlock ){` |
|       143 |  3913 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|        27 |  3914 | `				break;` |
|         - |  3915 | `			}` |
|         - |  3916 | `			/* Point to the upper block */` |
|       121 |  3917 | `			pBlock = pBlock->pParent;` |
|         5 |  3918 | `		}` |
|       117 |  3919 | `		if( pBlock ){` |
|        27 |  3920 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        16 |  3921 | `		}else{` |
|        95 |  3922 | `			sLabel.pFunc = 0;` |
|         - |  3923 | `		}` |
|         - |  3924 | `		/* Insert in label set */` |
|       117 |  3925 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|         - |  3926 | `	}` |
|       117 |  3927 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|       117 |  3928 | `	return SXRET_OK;` |
|        61 |  3929 | `}` |
|         - |  3930 | `/*` |
|         - |  3931 | ` * Compile the so hated 'goto' statement.` |
|         - |  3932 | ` * You've probably been taught that gotos are bad, but this sort` |
|         - |  3933 | ` * of rewriting  happens all the time, in fact every time you run` |
|         - |  3934 | ` * a compiler it has to do this.` |
|         - |  3935 | ` * According to the PHP language reference manual` |
|         - |  3936 | ` *   The goto operator can be used to jump to another section in the program.` |
|         - |  3937 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|         - |  3938 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|         - |  3939 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|         - |  3940 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|         - |  3941 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|         - |  3942 | ` *   of a multi-level break` |
|         - |  3943 | ` */` |
|       152 |  3944 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|         5 |  3945 | `{` |
|         - |  3946 | `	JumpFixup sJump;` |
|         - |  3947 | `	sxi32 rc;` |
|       157 |  3948 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|       157 |  3949 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  3950 | `		/* Missing label */` |
|       ! 0 |  3951 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|       ! 0 |  3952 | `		if( rc == SXERR_ABORT ){` |
|         - |  3953 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3954 | `			return SXERR_ABORT;` |
|         - |  3955 | `		}` |
|       ! 0 |  3956 | `		return SXRET_OK;` |
|         - |  3957 | `	}` |
|       157 |  3958 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|         6 |  3959 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn,"identifier");` |
|         6 |  3960 | `		if( rc == SXERR_ABORT ){` |
|         - |  3961 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3962 | `			return SXERR_ABORT;` |
|         - |  3963 | `		}` |
|         4 |  3964 | `	}else{` |
|       153 |  3965 | `		SyString *pTarget = &pGen->pIn->sData;` |
|         - |  3966 | `		GenBlock *pBlock;` |
|         - |  3967 | `		char *zDup;` |
|         - |  3968 | `		/* Prepare the jump destination */` |
|       153 |  3969 | `		sJump.nJumpType = PH7_OP_JMP;` |
|       153 |  3970 | `		sJump.nLine = pGen->pIn->nLine;` |
|         - |  3971 | `		/* Duplicate label name */` |
|       153 |  3972 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|       153 |  3973 | `		if( zDup == 0 ){` |
|       ! 0 |  3974 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  3975 | `			return SXERR_ABORT;` |
|         - |  3976 | `		}` |
|       153 |  3977 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|         - |  3978 | `		/* The loop/switch this goto sits in, for the "goto into a loop" check later. */` |
|       153 |  3979 | `		sJump.nLoopId = pGen->nCurLoopId;` |
|         - |  3980 | `		/* A goto inside a try{}/catch{} is legal php (jumping OUT of the block is fine);` |
|         - |  3981 | `		 * only the owning function matters here, since a goto may not cross functions. */` |
|       153 |  3982 | `		pBlock = pGen->pCurrent;` |
|       327 |  3983 | `		while( pBlock ){` |
|       205 |  3984 | `			if( pBlock->iFlags & GEN_BLOCK_FUNC ){` |
|        30 |  3985 | `				break;` |
|         - |  3986 | `			}` |
|         - |  3987 | `			/* Point to the upper block */` |
|       179 |  3988 | `			pBlock = pBlock->pParent;` |
|         5 |  3989 | `		}` |
|       153 |  3990 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|        30 |  3991 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        17 |  3992 | `		}else{` |
|       127 |  3993 | `			sJump.pFunc = 0;` |
|         - |  3994 | `		}` |
|         - |  3995 | `		/* Emit the unconditional jump */` |
|       153 |  3996 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|       153 |  3997 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|        74 |  3998 | `		}` |
|         - |  3999 | `	}` |
|       157 |  4000 | `	pGen->pIn++; /* Jump the label name */` |
|       157 |  4001 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         3 |  4002 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|         1 |  4003 | `	}` |
|         - |  4004 | `	/* Statement successfully compiled */` |
|       157 |  4005 | `	return SXRET_OK;` |
|        81 |  4006 | `}` |
|         - |  4007 | `/*` |
|         - |  4008 | ` * Point to the next PHP chunk that will be processed shortly.` |
|         - |  4009 | ` * Return SXRET_OK on success. Any other return value indicates` |
|         - |  4010 | ` * failure.` |
|         - |  4011 | ` */` |
|        20 |  4012 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|         2 |  4013 | `{` |
|         - |  4014 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|         - |  4015 | `	sxu32 nRawObj;` |
|        10 |  4016 | `	sxu32 nObjIdx;` |
|         - |  4017 | `	/* Consume raw chunks verbatim without any processing until we get` |
|         - |  4018 | `	 * a PHP block.` |
|         - |  4019 | `	 */` |
|        10 |  4020 | `Consume:` |
|        22 |  4021 | `	nRawObj = nObjIdx = 0;` |
|        22 |  4022 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|       ! 0 |  4023 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|       ! 0 |  4024 | `		if( pRawObj == 0 ){` |
|       ! 0 |  4025 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  4026 | `			return SXERR_ABORT;` |
|         - |  4027 | `		}` |
|         - |  4028 | `		/* Mark as constant and emit the load constant instruction */` |
|       ! 0 |  4029 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|       ! 0 |  4030 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|       ! 0 |  4031 | `		++nRawObj;` |
|       ! 0 |  4032 | `		pGen->pRawIn++; /* Next chunk */` |
|       ! 0 |  4033 | `	}` |
|        22 |  4034 | `	if( nRawObj > 0 ){` |
|         - |  4035 | `		/* Emit the consume instruction */` |
|       ! 0 |  4036 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|       ! 0 |  4037 | `	}` |
|        22 |  4038 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|       ! 0 |  4039 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|         - |  4040 | `		/* Reset the token set (and its trivia sidecar) */` |
|       ! 0 |  4041 | `		SySetReset(pTokenSet);` |
|       ! 0 |  4042 | `		SySetReset(&pGen->aTrivia);` |
|         - |  4043 | `		/* Tokenize input */` |
|       ! 0 |  4044 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|       ! 0 |  4045 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|         - |  4046 | `		/* Point to the fresh token stream */` |
|       ! 0 |  4047 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|       ! 0 |  4048 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|         - |  4049 | `		/* Advance the stream cursor */` |
|       ! 0 |  4050 | `		pGen->pRawIn++;` |
|         - |  4051 | `		/* TICKET 1433-011 */` |
|       ! 0 |  4052 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - |  4053 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - |  4054 | `			sxi32 rc;` |
|         - |  4055 | `			/* Refer to TICKET 1433-009  */` |
|       ! 0 |  4056 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       ! 0 |  4057 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       ! 0 |  4058 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       ! 0 |  4059 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 |  4060 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4061 | `				return SXERR_ABORT;` |
|       ! 0 |  4062 | `			}else if( rc != SXERR_EMPTY ){` |
|       ! 0 |  4063 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       ! 0 |  4064 | `			}` |
|       ! 0 |  4065 | `			goto Consume;` |
|         - |  4066 | `		}` |
|       ! 0 |  4067 | `	}else{` |
|         - |  4068 | `		/* No more chunks to process */` |
|        22 |  4069 | `		pGen->pIn = pGen->pEnd;` |
|        22 |  4070 | `		return SXERR_EOF;` |
|         - |  4071 | `	}` |
|       ! 0 |  4072 | `	return SXRET_OK;` |
|        12 |  4073 | `}` |
|         - |  4074 | `/*` |
|         - |  4075 | ` * Compile a PHP block.` |
|         - |  4076 | ` * A block is simply one or more PHP statements and expressions to compile` |
|         - |  4077 | ` * optionally delimited by braces {}.` |
|         - |  4078 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  4079 | ` * and this function takes care of generating the appropriate error` |
|         - |  4080 | ` * message.` |
|         - |  4081 | ` */` |
|   5169640 |  4082 | `static sxi32 PH7_CompileBlock(` |
|         - |  4083 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  4084 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|         - |  4085 | `	)` |
|         5 |  4086 | `{` |
|         - |  4087 | `	sxi32 rc;` |
|         - |  4088 | `	sxu32 nLine;` |
|   5169645 |  4089 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|   5168501 |  4090 | `		nLine = pGen->pIn->nLine;` |
|   5168501 |  4091 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|   5168501 |  4092 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  4093 | `			return SXERR_ABORT;` |
|         - |  4094 | `		}` |
|   5168501 |  4095 | `		pGen->pIn++;` |
|         - |  4096 | `		/* Compile until we hit the closing braces '}' */` |
|   7561013 |  4097 | `		for(;;){` |
|  15122031 |  4098 | `			if( pGen->pIn >= pGen->pEnd ){` |
|        22 |  4099 | `				rc = GenStateNextChunk(&(*pGen));` |
|        22 |  4100 | `				if (rc == SXERR_ABORT ){` |
|       ! 0 |  4101 | `			 	   return SXERR_ABORT;` |
|         - |  4102 | `				}` |
|        22 |  4103 | `				if( rc == SXERR_EOF ){` |
|         - |  4104 | `					/* No more token to process: the block was never closed. php reports` |
|         - |  4105 | `					 * the line the '{' was opened on, not where the input ran out. */` |
|        22 |  4106 | `					PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"Unclosed '{' on line %u",nLine);` |
|        22 |  4107 | `					break;` |
|         - |  4108 | `				}` |
|       ! 0 |  4109 | `			}` |
|  15122011 |  4110 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|         - |  4111 | `				/* Closing braces found,break immediately*/` |
|   5168481 |  4112 | `				pGen->pIn++;` |
|   5168481 |  4113 | `				break;` |
|         - |  4114 | `			}` |
|         - |  4115 | `			/* Compile a single statement */` |
|   9953535 |  4116 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|   9953535 |  4117 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4118 | `				return SXERR_ABORT;` |
|         - |  4119 | `			}` |
|         5 |  4120 | `		}` |
|   5168501 |  4121 | `		GenStateLeaveBlock(&(*pGen),0);` |
|   2585397 |  4122 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|       ! 0 |  4123 | `		pGen->pIn++;` |
|       ! 0 |  4124 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|       ! 0 |  4125 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  4126 | `			return SXERR_ABORT;` |
|         - |  4127 | `		}` |
|         - |  4128 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|       ! 0 |  4129 | `		for(;;){` |
|       ! 0 |  4130 | `			if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  4131 | `				rc = GenStateNextChunk(&(*pGen));` |
|       ! 0 |  4132 | `				if (rc == SXERR_ABORT ){` |
|       ! 0 |  4133 | `			 	   return SXERR_ABORT;` |
|         - |  4134 | `				}` |
|       ! 0 |  4135 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|         - |  4136 | `					/* No more token to process */` |
|       ! 0 |  4137 | `					if( rc == SXERR_EOF ){` |
|       ! 0 |  4138 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|         - |  4139 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|       ! 0 |  4140 | `					}` |
|       ! 0 |  4141 | `					break;` |
|         - |  4142 | `				}` |
|       ! 0 |  4143 | `			}` |
|       ! 0 |  4144 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|         - |  4145 | `				sxi32 nKwrd;` |
|         - |  4146 | `				/* Keyword found */` |
|       ! 0 |  4147 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       ! 0 |  4148 | `				if( nKwrd == nKeywordEnd \|\|` |
|       ! 0 |  4149 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|         - |  4150 | `						/* Delimiter keyword found,break */` |
|       ! 0 |  4151 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|       ! 0 |  4152 | `							pGen->pIn++; /*  endif;endswitch... */` |
|       ! 0 |  4153 | `						}` |
|       ! 0 |  4154 | `						break;` |
|         - |  4155 | `				}` |
|       ! 0 |  4156 | `			}` |
|         - |  4157 | `			/* Compile a single statement */` |
|       ! 0 |  4158 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|       ! 0 |  4159 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4160 | `				return SXERR_ABORT;` |
|         - |  4161 | `			}` |
|       ! 0 |  4162 | `		}` |
|       ! 0 |  4163 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  4164 | `	}else{` |
|         - |  4165 | `		/* Compile a single statement */` |
|      1149 |  4166 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|      1149 |  4167 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4168 | `			return SXERR_ABORT;` |
|         - |  4169 | `		}` |
|         - |  4170 | `	}` |
|         - |  4171 | `	/* Jump trailing semi-colons ';' */` |
|   5169645 |  4172 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       ! 0 |  4173 | `		pGen->pIn++;` |
|       ! 0 |  4174 | `	}` |
|   5169645 |  4175 | `	return SXRET_OK;` |
|   2584825 |  4176 | `}` |
|         - |  4177 | `/*` |
|         - |  4178 | ` * Compile the gentle 'while' statement.` |
|         - |  4179 | ` * According to the PHP language reference` |
|         - |  4180 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|         - |  4181 | ` *  The basic form of a while statement is:` |
|         - |  4182 | ` *  while (expr)` |
|         - |  4183 | ` *   statement` |
|         - |  4184 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|         - |  4185 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|         - |  4186 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|         - |  4187 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|         - |  4188 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|         - |  4189 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|         - |  4190 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|         - |  4191 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|         - |  4192 | ` *  while (expr):` |
|         - |  4193 | ` *    statement` |
|         - |  4194 | ` *   endwhile;` |
|         - |  4195 | ` */` |
|     53286 |  4196 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|         5 |  4197 | `{` |
|     53291 |  4198 | `	GenBlock *pWhileBlock = 0;` |
|     53291 |  4199 | `	SyToken *pTmp,*pEnd = 0;` |
|         - |  4200 | `	sxu32 nFalseJump;` |
|         - |  4201 | `	sxu32 nLine;` |
|         - |  4202 | `	sxi32 rc;` |
|     53291 |  4203 | `	nLine = pGen->pIn->nLine;` |
|         - |  4204 | `	/* Jump the 'while' keyword */` |
|     53291 |  4205 | `	pGen->pIn++;` |
|     53291 |  4206 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4207 | `		/* Syntax error */` |
|       ! 0 |  4208 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|       ! 0 |  4209 | `		if( rc == SXERR_ABORT ){` |
|         - |  4210 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4211 | `			return SXERR_ABORT;` |
|         - |  4212 | `		}` |
|       ! 0 |  4213 | `		goto Synchronize;` |
|         - |  4214 | `	}` |
|         - |  4215 | `	/* Jump the left parenthesis '(' */` |
|     53291 |  4216 | `	pGen->pIn++;` |
|         - |  4217 | `	/* Create the loop block */` |
|     53291 |  4218 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|     53291 |  4219 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4220 | `		return SXERR_ABORT;` |
|         - |  4221 | `	}` |
|         - |  4222 | `	/* Delimit the condition */` |
|     53291 |  4223 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     53291 |  4224 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4225 | `		/* Empty expression */` |
|         3 |  4226 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|         3 |  4227 | `		if( rc == SXERR_ABORT ){` |
|         - |  4228 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4229 | `			return SXERR_ABORT;` |
|         - |  4230 | `		}` |
|         1 |  4231 | `	}` |
|         - |  4232 | `	/* Swap token streams */` |
|     53291 |  4233 | `	pTmp = pGen->pEnd;` |
|     53291 |  4234 | `	pGen->pEnd = pEnd;` |
|         - |  4235 | `	/* Compile the expression */` |
|     53291 |  4236 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     53291 |  4237 | `	if( rc == SXERR_ABORT ){` |
|         - |  4238 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4239 | `		return SXERR_ABORT;` |
|         - |  4240 | `	}` |
|         - |  4241 | `	/* Update token stream */` |
|     53291 |  4242 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 |  4243 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  4244 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4245 | `			return SXERR_ABORT;` |
|         - |  4246 | `		}` |
|       ! 0 |  4247 | `		pGen->pIn++;` |
|       ! 0 |  4248 | `	}` |
|         - |  4249 | `	/* Synchronize pointers */` |
|     53291 |  4250 | `	pGen->pIn  = &pEnd[1];` |
|     53291 |  4251 | `	pGen->pEnd = pTmp;` |
|         - |  4252 | `	/* Emit the false jump */` |
|     53291 |  4253 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|         - |  4254 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|     53291 |  4255 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|         - |  4256 | `	/* Compile the loop body */` |
|     53291 |  4257 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|     53291 |  4258 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4259 | `		return SXERR_ABORT;` |
|         - |  4260 | `	}` |
|         - |  4261 | `	/* Emit the unconditional jump to the start of the loop */` |
|     53291 |  4262 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|         - |  4263 | `	/* Fix all jumps now the destination is resolved */` |
|     53291 |  4264 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4265 | `	/* Release the loop block */` |
|     53291 |  4266 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4267 | `	/* Statement successfully compiled */` |
|     53291 |  4268 | `	return SXRET_OK;` |
|       ! 0 |  4269 | `Synchronize:` |
|         - |  4270 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4271 | `	 * compiling this erroneous block.` |
|         - |  4272 | `	 */` |
|       ! 0 |  4273 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4274 | `		pGen->pIn++;` |
|       ! 0 |  4275 | `	}` |
|       ! 0 |  4276 | `	return SXRET_OK;` |
|     26648 |  4277 | `}` |
|         - |  4278 | `/*` |
|         - |  4279 | ` * Compile the ugly do..while() statement.` |
|         - |  4280 | ` * According to the PHP language reference` |
|         - |  4281 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|         - |  4282 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|         - |  4283 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|         - |  4284 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|         - |  4285 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|         - |  4286 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|         - |  4287 | ` *  would end immediately).` |
|         - |  4288 | ` *  There is just one syntax for do-while loops:` |
|         - |  4289 | ` *  <?php` |
|         - |  4290 | ` *  $i = 0;` |
|         - |  4291 | ` *  do {` |
|         - |  4292 | ` *   echo $i;` |
|         - |  4293 | ` *  } while ($i > 0);` |
|         - |  4294 | ` * ?>` |
|         - |  4295 | ` */` |
|         2 |  4296 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|         1 |  4297 | `{` |
|         3 |  4298 | `	SyToken *pTmp,*pEnd = 0;` |
|         3 |  4299 | `	GenBlock *pDoBlock = 0;` |
|         - |  4300 | `	sxu32 nLine;` |
|         - |  4301 | `	sxi32 rc;` |
|         3 |  4302 | `	nLine = pGen->pIn->nLine;` |
|         - |  4303 | `	/* Jump the 'do' keyword */` |
|         3 |  4304 | `	pGen->pIn++;` |
|         - |  4305 | `	/* Create the loop block */` |
|         3 |  4306 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|         3 |  4307 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4308 | `		return SXERR_ABORT;` |
|         - |  4309 | `	}` |
|         - |  4310 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|         3 |  4311 | `	pDoBlock->bPostContinue = TRUE;` |
|         3 |  4312 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|         3 |  4313 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4314 | `		return SXERR_ABORT;` |
|         - |  4315 | `	}` |
|         3 |  4316 | `	if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4317 | `		nLine = pGen->pIn->nLine;` |
|       ! 0 |  4318 | `	}` |
|         3 |  4319 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|       ! 0 |  4320 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|         - |  4321 | `			/* Missing 'while' statement */` |
|         3 |  4322 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|         3 |  4323 | `			if( rc == SXERR_ABORT ){` |
|         - |  4324 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4325 | `				return SXERR_ABORT;` |
|         - |  4326 | `			}` |
|         3 |  4327 | `			goto Synchronize;` |
|         - |  4328 | `	}` |
|         - |  4329 | `	/* Jump the 'while' keyword */` |
|       ! 0 |  4330 | `	pGen->pIn++;` |
|       ! 0 |  4331 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4332 | `		/* Syntax error */` |
|       ! 0 |  4333 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|       ! 0 |  4334 | `		if( rc == SXERR_ABORT ){` |
|         - |  4335 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4336 | `			return SXERR_ABORT;` |
|         - |  4337 | `		}` |
|       ! 0 |  4338 | `		goto Synchronize;` |
|         - |  4339 | `	}` |
|         - |  4340 | `	/* Jump the left parenthesis '(' */` |
|       ! 0 |  4341 | `	pGen->pIn++;` |
|         - |  4342 | `	/* Delimit the condition */` |
|       ! 0 |  4343 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|       ! 0 |  4344 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4345 | `		/* Empty expression */` |
|       ! 0 |  4346 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       ! 0 |  4347 | `		if( rc == SXERR_ABORT ){` |
|         - |  4348 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4349 | `			return SXERR_ABORT;` |
|         - |  4350 | `		}` |
|       ! 0 |  4351 | `		goto Synchronize;` |
|         - |  4352 | `	}` |
|         - |  4353 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|       ! 0 |  4354 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|         - |  4355 | `		JumpFixup *aPost;` |
|         - |  4356 | `		VmInstr *pInstr;` |
|         - |  4357 | `		sxu32 nJumpDest;` |
|         - |  4358 | `		sxu32 n;` |
|       ! 0 |  4359 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|       ! 0 |  4360 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       ! 0 |  4361 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|       ! 0 |  4362 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|       ! 0 |  4363 | `			if( pInstr ){` |
|         - |  4364 | `				/* Fix */` |
|       ! 0 |  4365 | `				pInstr->iP2 = nJumpDest;` |
|       ! 0 |  4366 | `			}` |
|       ! 0 |  4367 | `		}` |
|       ! 0 |  4368 | `	}` |
|         - |  4369 | `	/* Swap token streams */` |
|       ! 0 |  4370 | `	pTmp = pGen->pEnd;` |
|       ! 0 |  4371 | `	pGen->pEnd = pEnd;` |
|         - |  4372 | `	/* Compile the expression */` |
|       ! 0 |  4373 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       ! 0 |  4374 | `	if( rc == SXERR_ABORT ){` |
|         - |  4375 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4376 | `		return SXERR_ABORT;` |
|         - |  4377 | `	}` |
|         - |  4378 | `	/* Update token stream */` |
|       ! 0 |  4379 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 |  4380 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  4381 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4382 | `			return SXERR_ABORT;` |
|         - |  4383 | `		}` |
|       ! 0 |  4384 | `		pGen->pIn++;` |
|       ! 0 |  4385 | `	}` |
|       ! 0 |  4386 | `	pGen->pIn  = &pEnd[1];` |
|       ! 0 |  4387 | `	pGen->pEnd = pTmp;` |
|         - |  4388 | `	/* Emit the true jump to the beginning of the loop */` |
|       ! 0 |  4389 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|         - |  4390 | `	/* Fix all jumps now the destination is resolved */` |
|       ! 0 |  4391 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4392 | `	/* Release the loop block */` |
|       ! 0 |  4393 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4394 | `	/* Statement successfully compiled */` |
|       ! 0 |  4395 | `	return SXRET_OK;` |
|         1 |  4396 | `Synchronize:` |
|         - |  4397 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4398 | `	 * compiling this erroneous block.` |
|         - |  4399 | `	 */` |
|         3 |  4400 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4401 | `		pGen->pIn++;` |
|       ! 0 |  4402 | `	}` |
|         3 |  4403 | `	return SXRET_OK;` |
|         2 |  4404 | `}` |
|         - |  4405 | `/*` |
|         - |  4406 | ` * Compile the complex and powerful 'for' statement.` |
|         - |  4407 | ` * According to the PHP language reference` |
|         - |  4408 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|         - |  4409 | ` *  The syntax of a for loop is:` |
|         - |  4410 | ` *  for (expr1; expr2; expr3)` |
|         - |  4411 | ` *   statement` |
|         - |  4412 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|         - |  4413 | ` *  the beginning of the loop.` |
|         - |  4414 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|         - |  4415 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|         - |  4416 | ` *  to FALSE, the execution of the loop ends.` |
|         - |  4417 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|         - |  4418 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|         - |  4419 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|         - |  4420 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|         - |  4421 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|         - |  4422 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|         - |  4423 | ` *  of using the for truth expression.` |
|         - |  4424 | ` */` |
|     87452 |  4425 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|         5 |  4426 | `{` |
|     87457 |  4427 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|     87457 |  4428 | `	GenBlock *pForBlock = 0;` |
|         - |  4429 | `	sxu32 nFalseJump;` |
|         - |  4430 | `	sxu32 nLine;` |
|         - |  4431 | `	sxi32 rc;` |
|     87457 |  4432 | `	nLine = pGen->pIn->nLine;` |
|         - |  4433 | `	/* Jump the 'for' keyword */` |
|     87457 |  4434 | `	pGen->pIn++;` |
|     87457 |  4435 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4436 | `		/* Syntax error */` |
|       ! 0 |  4437 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|       ! 0 |  4438 | `		if( rc == SXERR_ABORT ){` |
|         - |  4439 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4440 | `			return SXERR_ABORT;` |
|         - |  4441 | `		}` |
|       ! 0 |  4442 | `		return SXRET_OK;` |
|         - |  4443 | `	}` |
|         - |  4444 | `	/* Jump the left parenthesis '(' */` |
|     87457 |  4445 | `	pGen->pIn++;` |
|         - |  4446 | `	/* Delimit the init-expr;condition;post-expr */` |
|     87457 |  4447 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     87457 |  4448 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4449 | `		/* Empty expression */` |
|       ! 0 |  4450 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|       ! 0 |  4451 | `		if( rc == SXERR_ABORT ){` |
|         - |  4452 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4453 | `			return SXERR_ABORT;` |
|         - |  4454 | `		}` |
|         - |  4455 | `		/* Synchronize */` |
|       ! 0 |  4456 | `		pGen->pIn = pEnd;` |
|       ! 0 |  4457 | `		if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4458 | `			pGen->pIn++;` |
|       ! 0 |  4459 | `		}` |
|       ! 0 |  4460 | `		return SXRET_OK;` |
|         - |  4461 | `	}` |
|         - |  4462 | `	/* Swap token streams */` |
|     87457 |  4463 | `	pTmp = pGen->pEnd;` |
|     87457 |  4464 | `	pGen->pEnd = pEnd;` |
|         - |  4465 | `	/* for() clauses are the ONLY place php's grammar allows a comma-separated` |
|         - |  4466 | `	 * expression list, so the comma operator is permitted for their duration` |
|         - |  4467 | `	 * (see GenStateTreeHasComma). A closure body nested inside a clause is` |
|         - |  4468 | `	 * compiled through this same window — recorded as a known leniency. */` |
|     87457 |  4469 | `	pGen->nCommaExprOk++;` |
|         - |  4470 | `	/* Compile initialization expressions if available */` |
|     87457 |  4471 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  4472 | `	/* Pop operand lvalues */` |
|     87457 |  4473 | `	if( rc == SXERR_ABORT ){` |
|         - |  4474 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4475 | `		return SXERR_ABORT;` |
|     87457 |  4476 | `	}else if( rc != SXERR_EMPTY ){` |
|     76067 |  4477 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     38031 |  4478 | `	}` |
|     87457 |  4479 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4480 | `		/* Syntax error */` |
|       ! 0 |  4481 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|       ! 0 |  4482 | `		if( rc == SXERR_ABORT ){` |
|         - |  4483 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4484 | `			return SXERR_ABORT;` |
|         - |  4485 | `		}` |
|       ! 0 |  4486 | `		return SXRET_OK;` |
|         - |  4487 | `	}` |
|         - |  4488 | `	/* Jump the trailing ';' */` |
|     87457 |  4489 | `	pGen->pIn++;` |
|         - |  4490 | `	/* Create the loop block */` |
|     87457 |  4491 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|     87457 |  4492 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4493 | `		return SXERR_ABORT;` |
|         - |  4494 | `	}` |
|         - |  4495 | `	/* Deffer continue jumps */` |
|     87457 |  4496 | `	pForBlock->bPostContinue = TRUE;` |
|         - |  4497 | `	/* Compile the condition */` |
|     87457 |  4498 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     87457 |  4499 | `	if( rc == SXERR_ABORT ){` |
|         - |  4500 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4501 | `		return SXERR_ABORT;` |
|     87457 |  4502 | `	}else if( rc != SXERR_EMPTY ){` |
|         - |  4503 | `		/* Emit the false jump */` |
|     76067 |  4504 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|         - |  4505 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|     76067 |  4506 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|     38031 |  4507 | `	}` |
|     87457 |  4508 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4509 | `		/* Syntax error */` |
|         6 |  4510 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|         6 |  4511 | `		if( rc == SXERR_ABORT ){` |
|         - |  4512 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4513 | `			return SXERR_ABORT;` |
|         - |  4514 | `		}` |
|         6 |  4515 | `		return SXRET_OK;` |
|         - |  4516 | `	}` |
|         - |  4517 | `	/* Jump the trailing ';' */` |
|     87453 |  4518 | `	pGen->pIn++;` |
|         - |  4519 | `	/* Save the post condition stream */` |
|     87453 |  4520 | `	pPostStart = pGen->pIn;` |
|         - |  4521 | `	/* Compile the loop body — OUTSIDE the comma window (the body is ordinary` |
|         - |  4522 | ``	 * php, so `(1, 2)` inside it is the parse error it should be). */`` |
|     87453 |  4523 | `	pGen->nCommaExprOk--;` |
|     87453 |  4524 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|     87453 |  4525 | `	pGen->pEnd = pTmp;` |
|     87453 |  4526 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|     87453 |  4527 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4528 | `		return SXERR_ABORT;` |
|         - |  4529 | `	}` |
|         - |  4530 | `	/* Fix post-continue jumps */` |
|     87453 |  4531 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|         - |  4532 | `		JumpFixup *aPost;` |
|         - |  4533 | `		VmInstr *pInstr;` |
|         - |  4534 | `		sxu32 nJumpDest;` |
|         - |  4535 | `		sxu32 n;` |
|      7609 |  4536 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      7609 |  4537 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     26601 |  4538 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|     18997 |  4539 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     18997 |  4540 | `			if( pInstr ){` |
|         - |  4541 | `				/* Fix jump */` |
|     18997 |  4542 | `				pInstr->iP2 = nJumpDest;` |
|      9496 |  4543 | `			}` |
|      9501 |  4544 | `		}` |
|      3802 |  4545 | `	}` |
|         - |  4546 | `	/* compile the post-expressions if available */` |
|     87453 |  4547 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|       ! 0 |  4548 | `		pPostStart++;` |
|       ! 0 |  4549 | `	}` |
|     87453 |  4550 | `	if( pPostStart < pEnd ){` |
|         - |  4551 | `		SyToken *pTmpIn,*pTmpEnd;` |
|     76065 |  4552 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|     76065 |  4553 | `		pGen->nCommaExprOk++; /* post-expressions are a clause list again */` |
|     76065 |  4554 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     76065 |  4555 | `		pGen->nCommaExprOk--;` |
|     76065 |  4556 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - |  4557 | `			/* Syntax error */` |
|       ! 0 |  4558 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|       ! 0 |  4559 | `			if( rc == SXERR_ABORT ){` |
|         - |  4560 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4561 | `				return SXERR_ABORT;` |
|         - |  4562 | `			}` |
|       ! 0 |  4563 | `			return SXRET_OK;` |
|         - |  4564 | `		}` |
|     76065 |  4565 | `		RE_SWAP_DELIMITER(pGen);` |
|     76065 |  4566 | `		if( rc == SXERR_ABORT ){` |
|         - |  4567 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4568 | `			return SXERR_ABORT;` |
|     76065 |  4569 | `		}else if( rc != SXERR_EMPTY){` |
|         - |  4570 | `			/* Pop operand lvalue */` |
|     76065 |  4571 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     38030 |  4572 | `		}` |
|     38030 |  4573 | `	}` |
|         - |  4574 | `	/* Emit the unconditional jump to the start of the loop */` |
|     87453 |  4575 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|         - |  4576 | `	/* Fix all jumps now the destination is resolved */` |
|     87453 |  4577 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4578 | `	/* Release the loop block */` |
|     87453 |  4579 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4580 | `	/* Statement successfully compiled */` |
|     87453 |  4581 | `	return SXRET_OK;` |
|     43731 |  4582 | `}` |
|         - |  4583 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|         - |  4584 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|         - |  4585 | ` * are allowed.` |
|         - |  4586 | ` */` |
|    319852 |  4587 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 |  4588 | `{` |
|    319857 |  4589 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    319857 |  4590 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  4591 | `		/* Unexpected expression */` |
|       ! 0 |  4592 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  4593 | `			"foreach: Expecting a variable name");` |
|       ! 0 |  4594 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 |  4595 | `			rc = SXERR_INVALID;` |
|       ! 0 |  4596 | `		}` |
|       ! 0 |  4597 | `	}` |
|    319857 |  4598 | `	return rc;` |
|         5 |  4599 | `}` |
|         - |  4600 | `/*` |
|         - |  4601 | ` * Compile the 'foreach' statement.` |
|         - |  4602 | ` * According to the PHP language reference` |
|         - |  4603 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|         - |  4604 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|         - |  4605 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|         - |  4606 | ` *  is a minor but useful extension of the first:` |
|         - |  4607 | ` *  foreach (array_expression as $value)` |
|         - |  4608 | ` *    statement` |
|         - |  4609 | ` *  foreach (array_expression as $key => $value)` |
|         - |  4610 | ` *   statement` |
|         - |  4611 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|         - |  4612 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|         - |  4613 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|         - |  4614 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|         - |  4615 | ` *  to the variable $key on each loop.` |
|         - |  4616 | ` *  Note:` |
|         - |  4617 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|         - |  4618 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|         - |  4619 | ` *  Note:` |
|         - |  4620 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|         - |  4621 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|         - |  4622 | ` *  or after the foreach without resetting it.` |
|         - |  4623 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|         - |  4624 | ` *  of copying the value.` |
|         - |  4625 | ` */` |
|    220882 |  4626 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|         5 |  4627 | `{` |
|    220887 |  4628 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    220887 |  4629 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    220887 |  4630 | `	GenBlock *pForeachBlock = 0;` |
|         - |  4631 | `	ph7_foreach_info *pInfo;` |
|         - |  4632 | `	sxu32 nFalseJump;` |
|         - |  4633 | `	VmInstr *pInstr;` |
|         - |  4634 | `	sxu32 nLine;` |
|         - |  4635 | `	sxi32 rc;` |
|    220887 |  4636 | `	nLine = pGen->pIn->nLine;` |
|         - |  4637 | `	/* Jump the 'foreach' keyword */` |
|    220887 |  4638 | `	pGen->pIn++;` |
|    220887 |  4639 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4640 | `		/* Syntax error */` |
|       ! 0 |  4641 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|       ! 0 |  4642 | `		if( rc == SXERR_ABORT ){` |
|         - |  4643 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4644 | `			return SXERR_ABORT;` |
|         - |  4645 | `		}` |
|       ! 0 |  4646 | `		goto Synchronize;` |
|         - |  4647 | `	}` |
|         - |  4648 | `	/* Jump the left parenthesis '(' */` |
|    220887 |  4649 | `	pGen->pIn++;` |
|         - |  4650 | `	/* Create the loop block */` |
|    220887 |  4651 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    220887 |  4652 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4653 | `		return SXERR_ABORT;` |
|         - |  4654 | `	}` |
|         - |  4655 | `	/* Delimit the expression */` |
|    220887 |  4656 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    220887 |  4657 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4658 | `		/* Empty expression */` |
|       ! 0 |  4659 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|       ! 0 |  4660 | `		if( rc == SXERR_ABORT ){` |
|         - |  4661 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4662 | `			return SXERR_ABORT;` |
|         - |  4663 | `		}` |
|         - |  4664 | `		/* Synchronize */` |
|       ! 0 |  4665 | `		pGen->pIn = pEnd;` |
|       ! 0 |  4666 | `		if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4667 | `			pGen->pIn++;` |
|       ! 0 |  4668 | `		}` |
|       ! 0 |  4669 | `		return SXRET_OK;` |
|         - |  4670 | `	}` |
|         - |  4671 | `	/* Compile the array expression */` |
|    220887 |  4672 | `	pCur = pGen->pIn;` |
|   1192145 |  4673 | `	while( pCur < pEnd ){` |
|   1192145 |  4674 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    232289 |  4675 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    232289 |  4676 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|         - |  4677 | `				/* Break with the first 'as' found */` |
|    220887 |  4678 | `				break;` |
|         - |  4679 | `			}` |
|      5701 |  4680 | `		}` |
|         - |  4681 | `		/* Advance the stream cursor */` |
|    971263 |  4682 | `		pCur++;` |
|         5 |  4683 | `	}` |
|    220887 |  4684 | `	if( pCur <= pGen->pIn ){` |
|       ! 0 |  4685 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  4686 | `			"foreach: Missing array/object expression");` |
|       ! 0 |  4687 | `		if( rc == SXERR_ABORT ){` |
|         - |  4688 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4689 | `			return SXERR_ABORT;` |
|         - |  4690 | `		}` |
|       ! 0 |  4691 | `		goto Synchronize;` |
|         - |  4692 | `	}` |
|         - |  4693 | `	/* Swap token streams */` |
|    220887 |  4694 | `	pTmp = pGen->pEnd;` |
|    220887 |  4695 | `	pGen->pEnd = pCur;` |
|    220887 |  4696 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    220887 |  4697 | `	if( rc == SXERR_ABORT ){` |
|         - |  4698 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4699 | `		return SXERR_ABORT;` |
|         - |  4700 | `	}` |
|         - |  4701 | `	/* Update token stream */` |
|    220887 |  4702 | `	while(pGen->pIn < pCur ){` |
|       ! 0 |  4703 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  4704 | `		if( rc == SXERR_ABORT ){` |
|         - |  4705 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4706 | `			return SXERR_ABORT;` |
|         - |  4707 | `		}` |
|       ! 0 |  4708 | `		pGen->pIn++;` |
|       ! 0 |  4709 | `	}` |
|    220887 |  4710 | `	pCur++; /* Jump the 'as' keyword */` |
|    220887 |  4711 | `	pGen->pIn = pCur;` |
|    220887 |  4712 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 |  4713 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|       ! 0 |  4714 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4715 | `			return SXERR_ABORT;` |
|         - |  4716 | `		}` |
|       ! 0 |  4717 | `	}` |
|         - |  4718 | `	/* Create the foreach context */` |
|    220887 |  4719 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    220887 |  4720 | `	if( pInfo == 0 ){` |
|       ! 0 |  4721 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 |  4722 | `		return SXERR_ABORT;` |
|         - |  4723 | `	}` |
|         - |  4724 | `	/* Zero the structure */` |
|    220887 |  4725 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|         - |  4726 | `	/* Initialize structure fields */` |
|    220887 |  4727 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|         - |  4728 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|         - |  4729 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|         - |  4730 | `	 * '=>'. */` |
|    220887 |  4731 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    220887 |  4732 | `	if( pCur < pEnd ){` |
|         - |  4733 | `		/* Compile the expression holding the key name */` |
|     98995 |  4734 | `		if( pGen->pIn >= pCur ){` |
|       ! 0 |  4735 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|       ! 0 |  4736 | `			if( rc == SXERR_ABORT ){` |
|         - |  4737 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4738 | `				return SXERR_ABORT;` |
|         - |  4739 | `			}` |
|       ! 0 |  4740 | `		}else{` |
|     98995 |  4741 | `			pGen->pEnd = pCur;` |
|     98995 |  4742 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|     98995 |  4743 | `			if( rc == SXERR_ABORT ){` |
|         - |  4744 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4745 | `				return SXERR_ABORT;` |
|         - |  4746 | `			}` |
|     98995 |  4747 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|     98995 |  4748 | `			if( pInstr->p3 ){` |
|         - |  4749 | `				/* Record key name */` |
|     98995 |  4750 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|     49495 |  4751 | `			}` |
|     98995 |  4752 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|         - |  4753 | `		}` |
|     98995 |  4754 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|     49495 |  4755 | `	}` |
|    220887 |  4756 | `	pGen->pEnd = pEnd;` |
|    220887 |  4757 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 |  4758 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|       ! 0 |  4759 | `		if( rc == SXERR_ABORT ){` |
|         - |  4760 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4761 | `			return SXERR_ABORT;` |
|         - |  4762 | `		}` |
|       ! 0 |  4763 | `		goto Synchronize;` |
|         - |  4764 | `	}` |
|    220887 |  4765 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|        33 |  4766 | `		pGen->pIn++;` |
|         - |  4767 | `		/* Pass by reference  */` |
|        33 |  4768 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|        15 |  4769 | `	}` |
|         - |  4770 | `	/* Check if the value target is list() */` |
|    220887 |  4771 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         8 |  4772 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|         - |  4773 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|         - |  4774 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|         - |  4775 | `		 */` |
|         - |  4776 | `		static int iForeachListCnt = 0;` |
|         - |  4777 | `		char zTmp[128];` |
|         - |  4778 | `		sxu32 nLen;` |
|         - |  4779 | `		char *zDup;` |
|        10 |  4780 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|        10 |  4781 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|        10 |  4782 | `		if( zDup == 0 ){` |
|       ! 0 |  4783 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  4784 | `			return SXERR_ABORT;` |
|         - |  4785 | `		}` |
|        10 |  4786 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|         - |  4787 | `		/* Save list() token boundaries */` |
|        10 |  4788 | `		pListStart = pGen->pIn;` |
|         - |  4789 | `		/* Advance past list(...) — validate parentheses */` |
|        10 |  4790 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|        10 |  4791 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         3 |  4792 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|         - |  4793 | `				"foreach: Expected '(' after 'list'");` |
|         3 |  4794 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4795 | `				return SXERR_ABORT;` |
|         - |  4796 | `			}` |
|         3 |  4797 | `			goto Synchronize;` |
|         - |  4798 | `		}` |
|         7 |  4799 | `		pGen->pIn++; /* Jump '(' */` |
|         7 |  4800 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|         7 |  4801 | `		if( pListEnd >= pEnd ){` |
|       ! 0 |  4802 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  4803 | `				"foreach: Missing closing ')' after list");` |
|       ! 0 |  4804 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4805 | `				return SXERR_ABORT;` |
|         - |  4806 | `			}` |
|       ! 0 |  4807 | `			goto Synchronize;` |
|         - |  4808 | `		}` |
|         7 |  4809 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|         7 |  4810 | `		pListEnd = pGen->pIn;` |
|         7 |  4811 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    220882 |  4812 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|         - |  4813 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|         - |  4814 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|         - |  4815 | `		 */` |
|         - |  4816 | `		static int iForeachShortListCnt = 0;` |
|         - |  4817 | `		char zTmp[128];` |
|         - |  4818 | `		sxu32 nLen;` |
|         - |  4819 | `		char *zDup;` |
|        13 |  4820 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|        13 |  4821 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|        13 |  4822 | `		if( zDup == 0 ){` |
|       ! 0 |  4823 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  4824 | `			return SXERR_ABORT;` |
|         - |  4825 | `		}` |
|        13 |  4826 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|         - |  4827 | `		/* Save [...] token boundaries */` |
|        13 |  4828 | `		pListStart = pGen->pIn;` |
|         - |  4829 | `		/* Advance past [...] */` |
|        13 |  4830 | `		pGen->pIn++; /* Jump '[' */` |
|        13 |  4831 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|        13 |  4832 | `		if( pListEnd >= pEnd ){` |
|       ! 0 |  4833 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  4834 | `				"foreach: Missing closing ']' after short list");` |
|       ! 0 |  4835 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4836 | `				return SXERR_ABORT;` |
|         - |  4837 | `			}` |
|       ! 0 |  4838 | `			goto Synchronize;` |
|         - |  4839 | `		}` |
|        13 |  4840 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|        13 |  4841 | `		pListEnd = pGen->pIn;` |
|        13 |  4842 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|         7 |  4843 | `	}else{` |
|         - |  4844 | `		/* Compile the expression holding the value name */` |
|    220867 |  4845 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    220867 |  4846 | `		if( rc == SXERR_ABORT ){` |
|         - |  4847 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4848 | `			return SXERR_ABORT;` |
|         - |  4849 | `		}` |
|    220867 |  4850 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    220867 |  4851 | `		if( pInstr->p3 ){` |
|         - |  4852 | `			/* Record value name */` |
|    220867 |  4853 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    110431 |  4854 | `		}` |
|         - |  4855 | `	}` |
|         - |  4856 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    220885 |  4857 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|         - |  4858 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    220885 |  4859 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|         - |  4860 | `	/* Record the first instruction to execute */` |
|    220885 |  4861 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|         - |  4862 | `	/* Emit the FOREACH_STEP instruction */` |
|    220885 |  4863 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|         - |  4864 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    220885 |  4865 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|         - |  4866 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    220885 |  4867 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|         - |  4868 | `		SyToken *pSavedIn,*pSavedEnd;` |
|         - |  4869 | `		/* Load the temporary variable holding the current value onto the stack.` |
|         - |  4870 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|         - |  4871 | `		 */` |
|        19 |  4872 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|         - |  4873 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|         - |  4874 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|         - |  4875 | `		 * picks up the delimiter and the variable names inside.` |
|         - |  4876 | `		 */` |
|        19 |  4877 | `		pSavedIn = pGen->pIn;` |
|        19 |  4878 | `		pSavedEnd = pGen->pEnd;` |
|        19 |  4879 | `		pGen->pIn = pListStart;` |
|        19 |  4880 | `		pGen->pEnd = pListEnd;` |
|        19 |  4881 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|        13 |  4882 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|         7 |  4883 | `		}else{` |
|         7 |  4884 | `			rc = PH7_CompileList(&(*pGen),0);` |
|         - |  4885 | `		}` |
|        19 |  4886 | `		pGen->pIn = pSavedIn;` |
|        19 |  4887 | `		pGen->pEnd = pSavedEnd;` |
|        19 |  4888 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4889 | `			return SXERR_ABORT;` |
|         - |  4890 | `		}` |
|         - |  4891 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|        19 |  4892 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         9 |  4893 | `	}` |
|         - |  4894 | `	/* Compile the loop body */` |
|    220885 |  4895 | `	pGen->pIn = &pEnd[1];` |
|    220885 |  4896 | `	pGen->pEnd = pTmp;` |
|    220885 |  4897 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    220885 |  4898 | `	if( rc == SXERR_ABORT ){` |
|         - |  4899 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4900 | `		return SXERR_ABORT;` |
|         - |  4901 | `	}` |
|         - |  4902 | `	/* Emit the unconditional jump to the start of the loop */` |
|    220885 |  4903 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|         - |  4904 | `	/* Fix all jumps now the destination is resolved */` |
|    220885 |  4905 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4906 | `	/* Release the loop block */` |
|    220885 |  4907 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4908 | `	/* Statement successfully compiled */` |
|    220885 |  4909 | `	return SXRET_OK;` |
|         1 |  4910 | `Synchronize:` |
|         - |  4911 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4912 | `	 * compiling this erroneous block.` |
|         - |  4913 | `	 */` |
|         3 |  4914 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4915 | `		pGen->pIn++;` |
|       ! 0 |  4916 | `	}` |
|         3 |  4917 | `	return SXRET_OK;` |
|    110446 |  4918 | `}` |
|         - |  4919 | `/*` |
|         - |  4920 | ` * Compile the infamous if/elseif/else if/else statements.` |
|         - |  4921 | ` * According to the PHP language reference` |
|         - |  4922 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|         - |  4923 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|         - |  4924 | ` *  that is similar to that of C:` |
|         - |  4925 | ` *  if (expr)` |
|         - |  4926 | ` *   statement` |
|         - |  4927 | ` *  else construct:` |
|         - |  4928 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|         - |  4929 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|         - |  4930 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|         - |  4931 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|         - |  4932 | ` *   $b, and a is NOT greater than b otherwise.` |
|         - |  4933 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|         - |  4934 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|         - |  4935 | ` *  elseif` |
|         - |  4936 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|         - |  4937 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|         - |  4938 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|         - |  4939 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|         - |  4940 | ` *   than b, a equal to b or a is smaller than b:` |
|         - |  4941 | ` *   <?php` |
|         - |  4942 | ` *    if ($a > $b) {` |
|         - |  4943 | ` *     echo "a is bigger than b";` |
|         - |  4944 | ` *    } elseif ($a == $b) {` |
|         - |  4945 | ` *     echo "a is equal to b";` |
|         - |  4946 | ` *    } else {` |
|         - |  4947 | ` *     echo "a is smaller than b";` |
|         - |  4948 | ` *    }` |
|         - |  4949 | ` *    ?>` |
|         - |  4950 | ` */` |
|   1854556 |  4951 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|         5 |  4952 | `{` |
|   1854561 |  4953 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|   1854561 |  4954 | `	GenBlock *pCondBlock = 0;` |
|         - |  4955 | `	sxu32 nJumpIdx;` |
|         - |  4956 | `	sxu32 nKeyID;` |
|         - |  4957 | `	sxi32 rc;` |
|         - |  4958 | `	/* Jump the 'if' keyword */` |
|   1854561 |  4959 | `	pGen->pIn++;` |
|   1854561 |  4960 | `	pToken = pGen->pIn;` |
|         - |  4961 | `	/* Create the conditional block */` |
|   1854561 |  4962 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|   1854561 |  4963 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4964 | `		return SXERR_ABORT;` |
|         - |  4965 | `	}` |
|         - |  4966 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   1033625 |  4967 | `	for(;;){` |
|   2067255 |  4968 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4969 | `			/* Syntax error */` |
|       ! 0 |  4970 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 |  4971 | `				pToken--;` |
|       ! 0 |  4972 | `			}` |
|       ! 0 |  4973 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|       ! 0 |  4974 | `			if( rc == SXERR_ABORT ){` |
|         - |  4975 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4976 | `				return SXERR_ABORT;` |
|         - |  4977 | `			}` |
|       ! 0 |  4978 | `			goto Synchronize;` |
|         - |  4979 | `		}` |
|         - |  4980 | `		/* Jump the left parenthesis '(' */` |
|   2067255 |  4981 | `		pToken++;` |
|         - |  4982 | `		/* Delimit the condition */` |
|   2067255 |  4983 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   2067255 |  4984 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|         - |  4985 | `			/* Syntax error */` |
|        11 |  4986 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 |  4987 | `				pToken--;` |
|       ! 0 |  4988 | `			}` |
|        11 |  4989 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|        11 |  4990 | `			if( rc == SXERR_ABORT ){` |
|         - |  4991 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4992 | `				return SXERR_ABORT;` |
|         - |  4993 | `			}` |
|        11 |  4994 | `			goto Synchronize;` |
|         - |  4995 | `		}` |
|         - |  4996 | `		/* Swap token streams */` |
|   2067247 |  4997 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|         - |  4998 | `		/* Compile the condition */` |
|   2067247 |  4999 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  5000 | `		/* Update token stream */` |
|   2067247 |  5001 | `		while(pGen->pIn < pEnd ){` |
|       ! 0 |  5002 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  5003 | `			pGen->pIn++;` |
|       ! 0 |  5004 | `		}` |
|   2067247 |  5005 | `		pGen->pIn  = &pEnd[1];` |
|   2067247 |  5006 | `		pGen->pEnd = pTmp;` |
|   2067247 |  5007 | `		if( rc == SXERR_ABORT ){` |
|         - |  5008 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  5009 | `			return SXERR_ABORT;` |
|         - |  5010 | `		}` |
|         - |  5011 | `		/* Emit the false jump */` |
|   2067247 |  5012 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|         - |  5013 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   2067247 |  5014 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|         - |  5015 | `		/* Compile the body */` |
|   2067247 |  5016 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   2067247 |  5017 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5018 | `			return SXERR_ABORT;` |
|         - |  5019 | `		}` |
|   2067247 |  5020 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|    410627 |  5021 | `			break;` |
|         - |  5022 | `		}` |
|         - |  5023 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   1246003 |  5024 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1246003 |  5025 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|    877209 |  5026 | `			break;` |
|         - |  5027 | `		}` |
|         - |  5028 | `		/* Emit the unconditional jump */` |
|    368799 |  5029 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|         - |  5030 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    368799 |  5031 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|    368799 |  5032 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|    239617 |  5033 | `			pToken = &pGen->pIn[1];` |
|    239617 |  5034 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|     83550 |  5035 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|     78055 |  5036 | `					break;` |
|         - |  5037 | `			}` |
|     83517 |  5038 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|     41756 |  5039 | `		}` |
|    212699 |  5040 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|         - |  5041 | `		/* Synchronize cursors */` |
|    212699 |  5042 | `		pToken = pGen->pIn;` |
|         - |  5043 | `		/* Fix the false jump */` |
|    212699 |  5044 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|         5 |  5045 | `	} /* For(;;) */` |
|         - |  5046 | `	/* Fix the false jump */` |
|   1854553 |  5047 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|   1854553 |  5048 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   1033304 |  5049 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|         - |  5050 | `			/* Compile the else block */` |
|    156105 |  5051 | `			pGen->pIn++;` |
|    156105 |  5052 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    156105 |  5053 | `			if( rc == SXERR_ABORT ){` |
|         - |  5054 |  |
|       ! 0 |  5055 | `				return SXERR_ABORT;` |
|         - |  5056 | `			}` |
|     78050 |  5057 | `	}` |
|   1854553 |  5058 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|         - |  5059 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|   1854553 |  5060 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|         - |  5061 | `	/* Release the conditional block */` |
|   1854553 |  5062 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  5063 | `	/* Statement successfully compiled */` |
|   1854553 |  5064 | `	return SXRET_OK;` |
|         4 |  5065 | `Synchronize:` |
|         - |  5066 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|         - |  5067 | `	 */` |
|        67 |  5068 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        59 |  5069 | `		pGen->pIn++;` |
|         3 |  5070 | `	}` |
|        11 |  5071 | `	return SXRET_OK;` |
|    927283 |  5072 | `}` |
|         - |  5073 | `/*` |
|         - |  5074 | ` * Compile the global construct.` |
|         - |  5075 | ` * According to the PHP language reference` |
|         - |  5076 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|         - |  5077 | ` *  to be used in that function.` |
|         - |  5078 | ` *  Example #1 Using global` |
|         - |  5079 | ` *  <?php` |
|         - |  5080 | ` *   $a = 1;` |
|         - |  5081 | ` *   $b = 2;` |
|         - |  5082 | ` *   function Sum()` |
|         - |  5083 | ` *   {` |
|         - |  5084 | ` *    global $a, $b;` |
|         - |  5085 | ` *    $b = $a + $b;` |
|         - |  5086 | ` *   }` |
|         - |  5087 | ` *   Sum();` |
|         - |  5088 | ` *   echo $b;` |
|         - |  5089 | ` *  ?>` |
|         - |  5090 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|         - |  5091 | ` *  all references to either variable will refer to the global version. There is no limit` |
|         - |  5092 | ` *  to the number of global variables that can be manipulated by a function.` |
|         - |  5093 | ` */` |
|        38 |  5094 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|         5 |  5095 | `{` |
|        43 |  5096 | `	SyToken *pTmp,*pNext = 0;` |
|         - |  5097 | `	sxi32 nExpr;` |
|         - |  5098 | `	sxi32 rc;` |
|         - |  5099 | `	/* Jump the 'global' keyword */` |
|        43 |  5100 | `	pGen->pIn++;` |
|        43 |  5101 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|         - |  5102 | `		/* Nothing to process */` |
|       ! 0 |  5103 | `		return SXRET_OK;` |
|         - |  5104 | `	}` |
|        43 |  5105 | `	pTmp = pGen->pEnd;` |
|        43 |  5106 | `	nExpr = 0;` |
|        91 |  5107 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|        53 |  5108 | `		if( pGen->pIn < pNext ){` |
|        53 |  5109 | `			pGen->pEnd = pNext;` |
|        53 |  5110 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  5111 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|       ! 0 |  5112 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  5113 | `					return SXERR_ABORT;` |
|         - |  5114 | `				}` |
|       ! 0 |  5115 | `			}else{` |
|        53 |  5116 | `				pGen->pIn++;` |
|        53 |  5117 | `				if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5118 | `					/* Emit a warning */` |
|       ! 0 |  5119 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|       ! 0 |  5120 | `				}else{` |
|        53 |  5121 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        53 |  5122 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  5123 | `						return SXERR_ABORT;` |
|        53 |  5124 | `					}else if(rc != SXERR_EMPTY ){` |
|        53 |  5125 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|        53 |  5126 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|         - |  5127 | `							/* Variable name, not a constant */` |
|        53 |  5128 | `							pLast->iP1 = 0;` |
|        24 |  5129 | `						}` |
|        53 |  5130 | `						nExpr++;` |
|        24 |  5131 | `					}` |
|         - |  5132 | `				}` |
|         - |  5133 | `			}` |
|        24 |  5134 | `		}` |
|         - |  5135 | `		/* Next expression in the stream */` |
|        53 |  5136 | `		pGen->pIn = pNext;` |
|         - |  5137 | `		/* Jump trailing commas */` |
|        63 |  5138 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        15 |  5139 | `			pGen->pIn++;` |
|         5 |  5140 | `		}` |
|         5 |  5141 | `	}` |
|         - |  5142 | `	/* Restore token stream */` |
|        43 |  5143 | `	pGen->pEnd = pTmp;` |
|        43 |  5144 | `	if( nExpr > 0 ){` |
|         - |  5145 | `		/* Emit the uplink instruction */` |
|        43 |  5146 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|        19 |  5147 | `	}` |
|        43 |  5148 | `	return SXRET_OK;` |
|        24 |  5149 | `}` |
|         - |  5150 | `/*` |
|         - |  5151 | ` * Compile the return statement.` |
|         - |  5152 | ` * According to the PHP language reference` |
|         - |  5153 | ` *  If called from within a function, the return() statement immediately ends execution` |
|         - |  5154 | ` *  of the current function, and returns its argument as the value of the function call.` |
|         - |  5155 | ` *  return() will also end the execution of an eval() statement or script file.` |
|         - |  5156 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|         - |  5157 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|         - |  5158 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|         - |  5159 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|         - |  5160 | ` *  from within the main script file, then script execution end.` |
|         - |  5161 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|         - |  5162 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|         - |  5163 | ` *  should do so as PHP has less work to do in this case.` |
|         - |  5164 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|         - |  5165 | ` */` |
|   2644088 |  5166 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|         5 |  5167 | `{` |
|   2644093 |  5168 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|         - |  5169 | `	sxi32 rc;` |
|   2644093 |  5170 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   2644093 |  5171 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|         - |  5172 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|         - |  5173 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|         - |  5174 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|         - |  5175 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|         - |  5176 | `	 * normally below so token processing stays consistent. */` |
|   6852931 |  5177 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|   4208843 |  5178 | `		pFuncBlock = pFuncBlock->pParent;` |
|         5 |  5179 | `	}` |
|   2644088 |  5180 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|   2644061 |  5181 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|         3 |  5182 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  5183 | `			"A never-returning function must not return");` |
|         3 |  5184 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5185 | `			return SXERR_ABORT;` |
|         - |  5186 | `		}` |
|         1 |  5187 | `	}` |
|         - |  5188 | `	/* Jump the 'return' keyword */` |
|   2644093 |  5189 | `	pGen->pIn++;` |
|   2644093 |  5190 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  5191 | `		/* Compile the expression */` |
|   2560551 |  5192 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   2560551 |  5193 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5194 | `			return SXERR_ABORT;` |
|   2560551 |  5195 | `		}else if(rc != SXERR_EMPTY ){` |
|   2560551 |  5196 | `			nRet = 1;` |
|   1280273 |  5197 | `		}` |
|   1280273 |  5198 | `	}` |
|         - |  5199 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|         - |  5200 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|         - |  5201 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|         - |  5202 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|   2644093 |  5203 | `	if( pGen->bInGenerator ){` |
|      3829 |  5204 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|      3829 |  5205 | `		return SXRET_OK;` |
|         - |  5206 | `	}` |
|         - |  5207 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|         - |  5208 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|         - |  5209 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|         - |  5210 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|         - |  5211 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|   2640269 |  5212 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|   2640269 |  5213 | `	return SXRET_OK;` |
|   1322049 |  5214 | `}` |
|         - |  5215 | `/*` |
|         - |  5216 | ` * Compile a yield expression.` |
|         - |  5217 | ` * Called from the expression code generator when a yield node is encountered.` |
|         - |  5218 | ` * Handles: yield, yield $value, yield $key => $value` |
|         - |  5219 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|         - |  5220 | ` */` |
|     15568 |  5221 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|         5 |  5222 | `{` |
|         - |  5223 | `	SyToken *pTmp, *pSplit;` |
|     15573 |  5224 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     15573 |  5225 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|         - |  5226 | `	sxi32 rc;` |
|      7784 |  5227 | `	(void)iCompileFlag;` |
|         - |  5228 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     15573 |  5229 | `	pGen->pIn++;` |
|         - |  5230 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|         - |  5231 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|         - |  5232 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|         - |  5233 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|         - |  5234 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     15568 |  5235 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|      7819 |  5236 | `		&& pGen->pIn->sData.nByte == 4` |
|        72 |  5237 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|        67 |  5238 | `		pGen->pIn++; /* Skip 'from' */` |
|        67 |  5239 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|        67 |  5240 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5241 | `			return SXERR_ABORT;` |
|         - |  5242 | `		}` |
|        67 |  5243 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 |  5244 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|       ! 0 |  5245 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|         - |  5246 | `				"Missing expression after 'yield from'");` |
|       ! 0 |  5247 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5248 | `				return SXERR_ABORT;` |
|         - |  5249 | `			}` |
|       ! 0 |  5250 | `		}` |
|        67 |  5251 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|        67 |  5252 | `		return SXRET_OK;` |
|         - |  5253 | `	}` |
|     15511 |  5254 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5255 | `		/* Bare yield — no value */` |
|         3 |  5256 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|         3 |  5257 | `		return SXRET_OK;` |
|         - |  5258 | `	}` |
|         - |  5259 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     15509 |  5260 | `	pSplit = 0;` |
|         - |  5261 | `	{` |
|     15509 |  5262 | `		SyToken *pCur = pGen->pIn;` |
|     15509 |  5263 | `		sxi32 nNest = 0;` |
|     46333 |  5264 | `		while( pCur < pGen->pEnd ){` |
|     46027 |  5265 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        18 |  5266 | `				nNest++;` |
|     46019 |  5267 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        18 |  5268 | `				nNest--;` |
|     46003 |  5269 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|     15203 |  5270 | `				pSplit = pCur;` |
|     15203 |  5271 | `				break;` |
|         - |  5272 | `			}` |
|     30829 |  5273 | `			pCur++;` |
|         5 |  5274 | `		}` |
|         - |  5275 | `	}` |
|     15509 |  5276 | `	pTmp = pGen->pEnd;` |
|     15509 |  5277 | `	if( pSplit ){` |
|         - |  5278 | `		/* yield $key => $value */` |
|     15203 |  5279 | `		pGen->pEnd = pSplit;` |
|     15203 |  5280 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     15203 |  5281 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     15203 |  5282 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|     15203 |  5283 | `		pGen->pEnd = pTmp;` |
|     15203 |  5284 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     15203 |  5285 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     15203 |  5286 | `		iP1 = 1;` |
|     15203 |  5287 | `		iP2 = 1;` |
|      7604 |  5288 | `	}else{` |
|         - |  5289 | `		/* yield $value */` |
|       311 |  5290 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       311 |  5291 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       311 |  5292 | `		if( rc != SXERR_EMPTY ){` |
|       311 |  5293 | `			iP1 = 1;` |
|       153 |  5294 | `		}` |
|         - |  5295 | `	}` |
|     15509 |  5296 | `	pGen->pEnd = pTmp;` |
|     15509 |  5297 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     15509 |  5298 | `	return SXRET_OK;` |
|      7789 |  5299 | `}` |
|         - |  5300 | `/*` |
|         - |  5301 | ` * Compile the die/exit language construct.` |
|         - |  5302 | ` * The role of these constructs is to terminate execution of the script.` |
|         - |  5303 | ` * Shutdown functions will always be executed even if exit() is called.` |
|         - |  5304 | ` */` |
|       128 |  5305 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|         5 |  5306 | `{` |
|       133 |  5307 | `	sxi32 nExpr = 0;` |
|         - |  5308 | `	sxi32 rc;` |
|         - |  5309 | `	/* Jump the die/exit keyword */` |
|       133 |  5310 | `	pGen->pIn++;` |
|       133 |  5311 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  5312 | `		/* Compile the expression */` |
|       133 |  5313 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       133 |  5314 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5315 | `			return SXERR_ABORT;` |
|       133 |  5316 | `		}else if(rc != SXERR_EMPTY ){` |
|       133 |  5317 | `			nExpr = 1;` |
|        64 |  5318 | `		}` |
|        64 |  5319 | `	}` |
|         - |  5320 | `	/* Emit the HALT instruction */` |
|       133 |  5321 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|       133 |  5322 | `	return SXRET_OK;` |
|        69 |  5323 | `}` |
|         - |  5324 | `/*` |
|         - |  5325 | ` * Compile the 'echo' language construct.` |
|         - |  5326 | ` */` |
|     17592 |  5327 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|         5 |  5328 | `{` |
|     17597 |  5329 | `	SyToken *pTmp,*pNext = 0;` |
|     17597 |  5330 | `	sxu32 nLine = pGen->pIn->nLine;` |
|     17597 |  5331 | `	int nExpr = 0;      /* expressions actually compiled */` |
|     17597 |  5332 | `	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */` |
|         - |  5333 | `	sxi32 rc;` |
|         - |  5334 | `	/* Jump the 'echo' keyword */` |
|     17597 |  5335 | `	pGen->pIn++;` |
|         - |  5336 | `	/* Compile arguments one after one */` |
|     17597 |  5337 | `	pTmp = pGen->pEnd;` |
|     44131 |  5338 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|     26541 |  5339 | `		if( pGen->pIn < pNext ){` |
|     26541 |  5340 | `			pGen->pEnd = pNext;` |
|     26541 |  5341 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|     26541 |  5342 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5343 | `				return SXERR_ABORT;` |
|     26541 |  5344 | `			}else if( rc != SXERR_EMPTY ){` |
|         - |  5345 | `				/* Emit the consume instruction */` |
|     26515 |  5346 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|     26515 |  5347 | `				nExpr++;` |
|     26515 |  5348 | `				bExpectMore = 0;` |
|     13255 |  5349 | `			}` |
|     13268 |  5350 | `		}` |
|         - |  5351 | `		/* Jump trailing commas (php: exactly one between expressions; a` |
|         - |  5352 | `		 * dangling or doubled comma is a parse error, enforced below) */` |
|     35491 |  5353 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|      8957 |  5354 | `			if( bExpectMore ){` |
|         - |  5355 | `				/* two commas in a row */` |
|         3 |  5356 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,` |
|         - |  5357 | `					"syntax error, unexpected token \",\"");` |
|         3 |  5358 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  5359 | `			}` |
|      8955 |  5360 | `			bExpectMore = 1;` |
|      8955 |  5361 | `			pNext++;` |
|         5 |  5362 | `		}` |
|     26539 |  5363 | `		pGen->pIn = pNext;` |
|         5 |  5364 | `	}` |
|         - |  5365 | `	/* Restore token stream */` |
|     17595 |  5366 | `	pGen->pEnd = pTmp;` |
|     17595 |  5367 | `	if( nExpr == 0 \|\| bExpectMore ){` |
|         - |  5368 | ``		/* `echo ;` or `echo expr, ;` — php rejects both */`` |
|        34 |  5369 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  5370 | `			"syntax error, unexpected token \";\"");` |
|        34 |  5371 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  5372 | `	}` |
|     17565 |  5373 | `	return SXRET_OK;` |
|      8801 |  5374 | `}` |
|         - |  5375 | `/*` |
|         - |  5376 | ` * Compile the static statement.` |
|         - |  5377 | ` * According to the PHP language reference` |
|         - |  5378 | ` *  Another important feature of variable scoping is the static variable.` |
|         - |  5379 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|         - |  5380 | ` *  when program execution leaves this scope.` |
|         - |  5381 | ` *  Static variables also provide one way to deal with recursive functions.` |
|         - |  5382 | ` * Symisc eXtension.` |
|         - |  5383 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|         - |  5384 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  5385 | ` *  Example` |
|         - |  5386 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|         - |  5387 | ` *    Refer to the official documentation for more information on this feature.` |
|         - |  5388 | ` */` |
|        12 |  5389 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|         3 |  5390 | `{` |
|         - |  5391 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|         - |  5392 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|         - |  5393 | `	GenBlock *pBlock;` |
|         - |  5394 | `	SyString *pName;` |
|         - |  5395 | `	char *zDup;` |
|         - |  5396 | `	sxu32 nLine;` |
|         - |  5397 | `	sxi32 rc;` |
|         - |  5398 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|         - |  5399 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|         - |  5400 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|        12 |  5401 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|        10 |  5402 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|         1 |  5403 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|         3 |  5404 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         3 |  5405 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5406 | `			return SXERR_ABORT;` |
|         3 |  5407 | `		}else if( rc != SXERR_EMPTY ){` |
|         3 |  5408 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 |  5409 | `		}` |
|         3 |  5410 | `		return SXRET_OK;` |
|         - |  5411 | `	}` |
|         - |  5412 | `	/* Jump the static keyword */` |
|        13 |  5413 | `	nLine = pGen->pIn->nLine;` |
|        13 |  5414 | `	pGen->pIn++;` |
|         - |  5415 | `	/* Extract the enclosing function if any */` |
|        13 |  5416 | `	pBlock = pGen->pCurrent;` |
|        23 |  5417 | `	while( pBlock ){` |
|        23 |  5418 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|        13 |  5419 | `			break;` |
|         - |  5420 | `		}` |
|         - |  5421 | `		/* Point to the upper block */` |
|        13 |  5422 | `		pBlock = pBlock->pParent;` |
|         3 |  5423 | `	}` |
|        13 |  5424 | `	if( pBlock == 0 ){` |
|         - |  5425 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|       ! 0 |  5426 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  5427 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       ! 0 |  5428 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5429 | `				return SXERR_ABORT;` |
|         - |  5430 | `			}` |
|       ! 0 |  5431 | `			goto Synchronize;` |
|         - |  5432 | `		}` |
|         - |  5433 | `		/* Compile the expression holding the variable */` |
|       ! 0 |  5434 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       ! 0 |  5435 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5436 | `			return SXERR_ABORT;` |
|       ! 0 |  5437 | `		}else if( rc != SXERR_EMPTY ){` |
|         - |  5438 | `			/* Emit the POP instruction */` |
|       ! 0 |  5439 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       ! 0 |  5440 | `		}` |
|       ! 0 |  5441 | `		return SXRET_OK;` |
|         - |  5442 | `	}` |
|        13 |  5443 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|         - |  5444 | `	/* Make sure we are dealing with a valid statement */` |
|        13 |  5445 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|         8 |  5446 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         3 |  5447 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|         3 |  5448 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5449 | `				return SXERR_ABORT;` |
|         - |  5450 | `			}` |
|         3 |  5451 | `			goto Synchronize;` |
|         - |  5452 | `	}` |
|        10 |  5453 | `	pGen->pIn++;` |
|         - |  5454 | `	/* Extract variable name */` |
|        10 |  5455 | `	pName = &pGen->pIn->sData;` |
|        10 |  5456 | `	pGen->pIn++; /* Jump the var name */` |
|        10 |  5457 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|       ! 0 |  5458 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  5459 | `		goto Synchronize;` |
|         - |  5460 | `	}` |
|         - |  5461 | `	/* Initialize the structure describing the static variable */` |
|        10 |  5462 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        10 |  5463 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|         - |  5464 | `	/* Duplicate variable name */` |
|        10 |  5465 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|        10 |  5466 | `	if( zDup == 0 ){` |
|       ! 0 |  5467 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  5468 | `		return SXERR_ABORT;` |
|         - |  5469 | `	}` |
|        10 |  5470 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|         - |  5471 | `	/* Check if we have an expression to compile */` |
|        10 |  5472 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|         - |  5473 | `		SySet *pInstrContainer;` |
|         - |  5474 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|         - |  5475 | `		 * Static variable can take any complex expression including function` |
|         - |  5476 | `		 * call as their initialization value.` |
|         - |  5477 | `		 * Example:` |
|         - |  5478 | `		 *		static $var = foo(1,4+5,bar());` |
|         - |  5479 | `		 */` |
|        10 |  5480 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|         - |  5481 | `		/* Swap bytecode container */` |
|        10 |  5482 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        10 |  5483 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|         - |  5484 | `		/* Compile the expression */` |
|        10 |  5485 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  5486 | `		/* Emit the done instruction */` |
|        10 |  5487 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|         - |  5488 | `		/* Restore default bytecode container */` |
|        10 |  5489 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         4 |  5490 | `	}` |
|         - |  5491 | `	/* Finally save the compiled static variable in the appropriate container */` |
|        10 |  5492 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|        10 |  5493 | `	return SXRET_OK;` |
|         1 |  5494 | `Synchronize:` |
|         - |  5495 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|         - |  5496 | `	 * statement.` |
|         - |  5497 | `	 */` |
|         5 |  5498 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|         3 |  5499 | `		pGen->pIn++;` |
|         1 |  5500 | `	}` |
|         3 |  5501 | `	return SXRET_OK;` |
|         9 |  5502 | `}` |
|         - |  5503 | `/*` |
|         - |  5504 | ` * Compile the var statement.` |
|         - |  5505 | ` * Symisc Extension:` |
|         - |  5506 | ` *      var statement can be used outside of a class definition.` |
|         - |  5507 | ` */` |
|         4 |  5508 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|         1 |  5509 | `{` |
|         - |  5510 | `	sxu32 nLine;` |
|         - |  5511 | `	sxi32 rc;` |
|         5 |  5512 | `	nLine = pGen->pIn->nLine;` |
|         - |  5513 | `	/* Jump the 'var' keyword */` |
|         5 |  5514 | `	pGen->pIn++;` |
|         5 |  5515 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|       ! 0 |  5516 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|         - |  5517 | `		/* Synchronize with the first semi-colon */` |
|       ! 0 |  5518 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|       ! 0 |  5519 | `			pGen->pIn++;` |
|       ! 0 |  5520 | `		}` |
|       ! 0 |  5521 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5522 | `			return SXERR_ABORT;` |
|         - |  5523 | `		}` |
|       ! 0 |  5524 | `	}else{` |
|         - |  5525 | `		/* Compile the expression */` |
|         5 |  5526 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         5 |  5527 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5528 | `			return SXERR_ABORT;` |
|         5 |  5529 | `		}else if( rc != SXERR_EMPTY ){` |
|         5 |  5530 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 |  5531 | `		}` |
|         - |  5532 | `	}` |
|         5 |  5533 | `	return SXRET_OK;` |
|         3 |  5534 | `}` |
|         - |  5535 | `/*` |
|         - |  5536 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|         - |  5537 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|         - |  5538 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|         - |  5539 | ` */` |
|         - |  5540 | `/*` |
|         - |  5541 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|         - |  5542 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|         - |  5543 | ` * hash and any shared references), this creates a new literal entry with the` |
|         - |  5544 | ` * qualified name and updates the instruction's operand index.` |
|         - |  5545 | ` *` |
|         - |  5546 | ` * Resolution order:` |
|         - |  5547 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|         - |  5548 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|         - |  5549 | ` *   3. Otherwise return the original literal index unchanged.` |
|         - |  5550 | ` *` |
|         - |  5551 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|         - |  5552 | ` * came from an import (step 1) and 0 otherwise.` |
|         - |  5553 | ` * Returns the (possibly new) literal index.` |
|         - |  5554 | ` */` |
|   4807282 |  5555 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|         5 |  5556 | `{` |
|         - |  5557 | `	ph7_value *pLit;` |
|         - |  5558 | `	const char *zLit;` |
|         - |  5559 | `	SyString sQualified;` |
|         - |  5560 | `	sxu32 nLit;` |
|         - |  5561 | `	sxu32 k;` |
|         - |  5562 | `	sxu32 nNewIdx;` |
|         - |  5563 | `	int hasNsSep;` |
|         - |  5564 | `	SyHashEntry *pImport;` |
|         - |  5565 | `	ph7_value *pNew;` |
|   4807287 |  5566 | `	if( pFromImport ){` |
|   3793761 |  5567 | `		*pFromImport = 0;` |
|   1896878 |  5568 | `	}` |
|   4807287 |  5569 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|   4807287 |  5570 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|       ! 0 |  5571 | `		return nOrigIdx;` |
|         - |  5572 | `	}` |
|   4807287 |  5573 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|   4807287 |  5574 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|         - |  5575 | `	/* Skip if already qualified (contains backslash) */` |
|   4807287 |  5576 | `	hasNsSep = 0;` |
|  58851325 |  5577 | `	for( k = 0; k < nLit; k++ ){` |
|  54044051 |  5578 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
|  27022024 |  5579 | `	}` |
|   4807287 |  5580 | `	if( hasNsSep ){` |
|        10 |  5581 | `		return nOrigIdx;` |
|         - |  5582 | `	}` |
|         - |  5583 | `	/* Check use imports first (works even outside namespaces) */` |
|   4807279 |  5584 | `	SyBlobReset(&pGen->sWorker);` |
|   4807279 |  5585 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|   4807279 |  5586 | `	if( pImport ){` |
|        41 |  5587 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|        41 |  5588 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|        41 |  5589 | `		if( pFromImport ){` |
|        18 |  5590 | `			*pFromImport = 1;` |
|         8 |  5591 | `		}` |
|        23 |  5592 | `	}else{` |
|   4807243 |  5593 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|   4807153 |  5594 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|         - |  5595 | `		}` |
|         - |  5596 | `		/* Prepend current namespace */` |
|        95 |  5597 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        95 |  5598 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|        95 |  5599 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|         - |  5600 | `	}` |
|         - |  5601 | `	/* Look up or create a new literal for the qualified name */` |
|       131 |  5602 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|       131 |  5603 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|        57 |  5604 | `		return nNewIdx; /* Already interned */` |
|         - |  5605 | `	}` |
|        79 |  5606 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|        79 |  5607 | `	if( pNew == 0 ){` |
|       ! 0 |  5608 | `		return nOrigIdx; /* OOM, fall back to original */` |
|         - |  5609 | `	}` |
|        79 |  5610 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|        79 |  5611 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|        79 |  5612 | `	return nNewIdx;` |
|   2403646 |  5613 | `}` |
|         - |  5614 | `/*` |
|         - |  5615 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|         - |  5616 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|         - |  5617 | ` */` |
|    407692 |  5618 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|         5 |  5619 | `{` |
|         - |  5620 | `	SyHashEntry *pImport;` |
|         - |  5621 | `	/* Check use imports first */` |
|    407697 |  5622 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|    407697 |  5623 | `	if( pImport ){` |
|        20 |  5624 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|        20 |  5625 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|        20 |  5626 | `		return;` |
|         - |  5627 | `	}` |
|         - |  5628 | `	/* Prepend current namespace if active */` |
|    407681 |  5629 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         8 |  5630 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         8 |  5631 | `		SyBlobAppend(pOut,"\\",1);` |
|         3 |  5632 | `	}` |
|    407681 |  5633 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    203851 |  5634 | `}` |
|         - |  5635 | `/*` |
|         - |  5636 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|         - |  5637 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|         - |  5638 | ` * The caller must release pOut when done.` |
|         - |  5639 | ` */` |
|    427108 |  5640 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|         5 |  5641 | `{` |
|    427113 |  5642 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      3859 |  5643 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      3859 |  5644 | `		SyBlobAppend(pOut,"\\",1);` |
|      1927 |  5645 | `	}` |
|    427113 |  5646 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    427113 |  5647 | `}` |
|         - |  5648 | `/*` |
|         - |  5649 | ` * Compile a namespace statement` |
|         - |  5650 | ` * According to the PHP language reference manual` |
|         - |  5651 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|         - |  5652 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|         - |  5653 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|         - |  5654 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|         - |  5655 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|         - |  5656 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|         - |  5657 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|         - |  5658 | ` *  programming world.` |
|         - |  5659 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|         - |  5660 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|         - |  5661 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|         - |  5662 | ` *  classes/functions/constants.` |
|         - |  5663 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|         - |  5664 | ` *  readability of source code.` |
|         - |  5665 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|         - |  5666 | ` *  Here is an example of namespace syntax in PHP:` |
|         - |  5667 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|         - |  5668 | ` *       class MyClass {}` |
|         - |  5669 | ` *       function myfunction() {}` |
|         - |  5670 | ` *       const MYCONST = 1;` |
|         - |  5671 | ` *       $a = new MyClass;` |
|         - |  5672 | ` *       $c = new \my\name\MyClass;` |
|         - |  5673 | ` *       $a = strlen('hi');` |
|         - |  5674 | ` *       $d = namespace\MYCONST;` |
|         - |  5675 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|         - |  5676 | ` *       echo constant($d);` |
|         - |  5677 | ` * NOTE` |
|         - |  5678 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|         - |  5679 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|         - |  5680 | ` */` |
|         - |  5681 | `/*` |
|         - |  5682 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|         - |  5683 | ` */` |
|        14 |  5684 | `static const char * TokenTypeName(sxu32 nType)` |
|         3 |  5685 | `{` |
|        17 |  5686 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|        11 |  5687 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|        11 |  5688 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|        11 |  5689 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|        11 |  5690 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|        11 |  5691 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|         3 |  5692 | `	return "token";` |
|        10 |  5693 | `}` |
|      3902 |  5694 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|         5 |  5695 | `{` |
|         - |  5696 | `	sxu32 nLine;` |
|         - |  5697 | `	sxi32 rc;` |
|      3907 |  5698 | `	nLine = pGen->pIn->nLine;` |
|      3907 |  5699 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|         - |  5700 | `	/* Reset namespace and clear previous use imports */` |
|      3907 |  5701 | `	SyBlobReset(&pGen->sNamespace);` |
|      3907 |  5702 | `	SyHashRelease(&pGen->hUseImports);` |
|      3907 |  5703 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|      3907 |  5704 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|      3907 |  5705 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|      3907 |  5706 | `	SyHashRelease(&pGen->hUseConstImports);` |
|      3907 |  5707 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|      3907 |  5708 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5709 | `		/* Global namespace (bare "namespace;") */` |
|       ! 0 |  5710 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5711 | `		return SXRET_OK;` |
|         - |  5712 | `	}` |
|      3907 |  5713 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|         - |  5714 | `		/* namespace; — switch to global namespace */` |
|       ! 0 |  5715 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5716 | `		return SXRET_OK;` |
|         - |  5717 | `	}` |
|      3907 |  5718 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|         - |  5719 | `		/* namespace { } — global namespace block */` |
|       ! 0 |  5720 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5721 | `		return SXRET_OK;` |
|         - |  5722 | `	}` |
|         - |  5723 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|      7851 |  5724 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      3949 |  5725 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|         - |  5726 | `			/* Append backslash separator */` |
|        26 |  5727 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        26 |  5728 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|        11 |  5729 | `			}` |
|        15 |  5730 | `		}else{` |
|         - |  5731 | `			/* Append identifier */` |
|      3927 |  5732 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|         - |  5733 | `		}` |
|      3949 |  5734 | `		pGen->pIn++;` |
|         5 |  5735 | `	}` |
|         - |  5736 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|         - |  5737 | `	 * at the correct program counter, not just the last one compiled. */` |
|         - |  5738 | `	{` |
|      3907 |  5739 | `		char *zNsDup = 0;` |
|      3907 |  5740 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      5855 |  5741 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      3900 |  5742 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      1950 |  5743 | `		}` |
|      3907 |  5744 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|         - |  5745 | `	}` |
|      3907 |  5746 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|         8 |  5747 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  5748 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|         4 |  5749 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         6 |  5750 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5751 | `			return SXERR_ABORT;` |
|         - |  5752 | `		}` |
|         2 |  5753 | `	}` |
|      3907 |  5754 | `	return SXRET_OK;` |
|      1956 |  5755 | `}` |
|         - |  5756 | `/*` |
|         - |  5757 | ` * Compile the 'use' statement` |
|         - |  5758 | ` * According to the PHP language reference manual` |
|         - |  5759 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|         - |  5760 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|         - |  5761 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|         - |  5762 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|         - |  5763 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|         - |  5764 | ` *  a function or constant is not supported.` |
|         - |  5765 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|         - |  5766 | ` * NOTE` |
|         - |  5767 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|         - |  5768 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|         - |  5769 | ` */` |
|        72 |  5770 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|         5 |  5771 | `{` |
|         - |  5772 | `	sxu32 nLine;` |
|         - |  5773 | `	sxi32 rc;` |
|         - |  5774 | `	SyBlob sPath;` |
|         - |  5775 | `	SyString sAlias;` |
|         - |  5776 | `	SyToken *pLast;` |
|         - |  5777 | `	char *zDup;` |
|         - |  5778 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|         - |  5779 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|         - |  5780 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|        77 |  5781 | `	nLine = pGen->pIn->nLine;` |
|        77 |  5782 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|         - |  5783 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|        77 |  5784 | `	iUseType = 0;` |
|        77 |  5785 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        30 |  5786 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|        30 |  5787 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|        16 |  5788 | `			iUseType = 1;` |
|        16 |  5789 | `			pGen->pIn++;` |
|        23 |  5790 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|        16 |  5791 | `			iUseType = 2;` |
|        16 |  5792 | `			pGen->pIn++;` |
|         7 |  5793 | `		}` |
|        14 |  5794 | `	}` |
|         - |  5795 | `	/* Select target hash tables based on import type */` |
|        77 |  5796 | `	switch( iUseType ){` |
|         7 |  5797 | `		case 1:` |
|        16 |  5798 | `			pGenHash = &pGen->hUseFuncImports;` |
|        16 |  5799 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|        16 |  5800 | `			break;` |
|         7 |  5801 | `		case 2:` |
|        16 |  5802 | `			pGenHash = &pGen->hUseConstImports;` |
|        16 |  5803 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|        16 |  5804 | `			break;` |
|        22 |  5805 | `		default:` |
|        49 |  5806 | `			pGenHash = &pGen->hUseImports;` |
|        49 |  5807 | `			pVmHash = &pGen->pVm->hUseImports;` |
|        44 |  5808 | `			break;` |
|         - |  5809 | `	}` |
|        77 |  5810 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|         - |  5811 | `	/* Process one or more use declarations separated by commas */` |
|        37 |  5812 | `	for(;;){` |
|        79 |  5813 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  5814 | `			break;` |
|         - |  5815 | `		}` |
|        79 |  5816 | `		SyBlobReset(&sPath);` |
|        79 |  5817 | `		pLast = 0;` |
|         - |  5818 | `		/* Collect the full namespace path */` |
|       269 |  5819 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|       195 |  5820 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       135 |  5821 | `				pLast = pGen->pIn;` |
|       135 |  5822 | `				if( SyBlobLength(&sPath) > 0 ){` |
|        65 |  5823 | `					SyBlobAppend(&sPath,"\\",1);` |
|        30 |  5824 | `				}` |
|       135 |  5825 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        65 |  5826 | `			}` |
|       195 |  5827 | `			pGen->pIn++;` |
|         5 |  5828 | `		}` |
|        79 |  5829 | `		if( pLast == 0 ){` |
|         - |  5830 | `			/* Empty path */` |
|         6 |  5831 | `			break;` |
|         - |  5832 | `		}` |
|         - |  5833 | `		/* Default alias is the last component of the path */` |
|        75 |  5834 | `		sAlias = pLast->sData;` |
|         - |  5835 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|        70 |  5836 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|        50 |  5837 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|        23 |  5838 | `			pGen->pIn++; /* Jump 'as' */` |
|        23 |  5839 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|        23 |  5840 | `				sAlias = pGen->pIn->sData;` |
|        23 |  5841 | `				pGen->pIn++;` |
|        10 |  5842 | `			}` |
|        10 |  5843 | `		}` |
|         - |  5844 | `		/* Check for duplicate import alias (per-type) */` |
|        75 |  5845 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|         8 |  5846 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  5847 | `				"Cannot use %.*s as %z because the name is already in use",` |
|         4 |  5848 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|         6 |  5849 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5850 | `				SyBlobRelease(&sPath);` |
|       ! 0 |  5851 | `				return SXERR_ABORT;` |
|         - |  5852 | `			}` |
|         2 |  5853 | `		}` |
|         - |  5854 | `		/* Register the import: alias -> FQN.` |
|         - |  5855 | `		 * Strings are allocated from the VM pool allocator and freed` |
|         - |  5856 | `		 * when the entire VM is released. SyHashRelease does not free` |
|         - |  5857 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|       110 |  5858 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        70 |  5859 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        75 |  5860 | `		if( zDup ){` |
|        75 |  5861 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|        75 |  5862 | `			if( pVmHash ){` |
|         - |  5863 | `				/* Class imports: populate VM table directly (class resolution` |
|         - |  5864 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|        47 |  5865 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        47 |  5866 | `				if( zAliasDup ){` |
|        47 |  5867 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|        21 |  5868 | `				}` |
|        21 |  5869 | `			}` |
|        75 |  5870 | `			if( iUseType == 2 ){` |
|         - |  5871 | `				/* Const imports: emit a runtime instruction so imports are` |
|         - |  5872 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|        16 |  5873 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        16 |  5874 | `				if( zAliasDup ){` |
|         - |  5875 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|         - |  5876 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|         - |  5877 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|        16 |  5878 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|        16 |  5879 | `					if( azPair ){` |
|        16 |  5880 | `						azPair[0] = zAliasDup;` |
|        16 |  5881 | `						azPair[1] = zDup;` |
|        16 |  5882 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|         7 |  5883 | `					}` |
|         7 |  5884 | `				}` |
|         7 |  5885 | `			}` |
|        35 |  5886 | `		}` |
|         - |  5887 | `		/* Check for comma (multiple use declarations) */` |
|        75 |  5888 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|         3 |  5889 | `			pGen->pIn++;` |
|         2 |  5890 | `		}else{` |
|        39 |  5891 | `			break;` |
|         - |  5892 | `		}` |
|         1 |  5893 | `	}` |
|        77 |  5894 | `	SyBlobRelease(&sPath);` |
|        77 |  5895 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         4 |  5896 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|         2 |  5897 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         3 |  5898 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5899 | `			return SXERR_ABORT;` |
|         - |  5900 | `		}` |
|         1 |  5901 | `	}` |
|        77 |  5902 | `	return SXRET_OK;` |
|        41 |  5903 | `}` |
|         - |  5904 | `/*` |
|         - |  5905 | ` * Compile the stupid 'declare' language construct.` |
|         - |  5906 | ` *` |
|         - |  5907 | ` * According to the PHP language reference manual.` |
|         - |  5908 | ` *  The declare construct is used to set execution directives for a block of code.` |
|         - |  5909 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|         - |  5910 | ` *  declare (directive)` |
|         - |  5911 | ` *   statement` |
|         - |  5912 | ` * The directive section allows the behavior of the declare block to be set.` |
|         - |  5913 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|         - |  5914 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|         - |  5915 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|         - |  5916 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|         - |  5917 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|         - |  5918 | ` * <?php` |
|         - |  5919 | ` * // these are the same:` |
|         - |  5920 | ` * // you can use this:` |
|         - |  5921 | ` * declare(ticks=1) {` |
|         - |  5922 | ` *   // entire script here` |
|         - |  5923 | ` * }` |
|         - |  5924 | ` * // or you can use this:` |
|         - |  5925 | ` * declare(ticks=1);` |
|         - |  5926 | ` * // entire script here` |
|         - |  5927 | ` * ?>` |
|         - |  5928 | ` *` |
|         - |  5929 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|         - |  5930 | ` */` |
|         - |  5931 | `/*` |
|         - |  5932 | ` * Match a directive name against a known literal (case-insensitive).` |
|         - |  5933 | ` */` |
|        72 |  5934 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|         5 |  5935 | `{` |
|       109 |  5936 | `	return SyStringLength(pName) == nWant` |
|        72 |  5937 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|         5 |  5938 | `}` |
|         - |  5939 |  |
|        42 |  5940 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|         5 |  5941 | `{` |
|        47 |  5942 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        47 |  5943 | `	SyToken *pBodyEnd = 0;` |
|         - |  5944 | `	SyToken *pBodyStart;` |
|         - |  5945 | `	SyToken *pCursor;` |
|         - |  5946 | `	int bHasStrictTypes;` |
|         - |  5947 | `	int bBlockForm;` |
|         - |  5948 | `	int bPlacementOk;` |
|         - |  5949 | `	sxi32 rc;` |
|        47 |  5950 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|        47 |  5951 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|         6 |  5952 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         6 |  5953 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5954 | `			return SXERR_ABORT;` |
|         - |  5955 | `		}` |
|         6 |  5956 | `		goto Synchro;` |
|         - |  5957 | `	}` |
|        43 |  5958 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|        43 |  5959 | `	pBodyStart = pGen->pIn;` |
|         - |  5960 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|        43 |  5961 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|        43 |  5962 | `	if( pBodyEnd >= pGen->pEnd ){` |
|       ! 0 |  5963 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|       ! 0 |  5964 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5965 | `			return SXERR_ABORT;` |
|         - |  5966 | `		}` |
|       ! 0 |  5967 | `		return SXRET_OK;` |
|         - |  5968 | `	}` |
|         - |  5969 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|         - |  5970 | `	 * now delimits the comma-separated directive list. */` |
|        43 |  5971 | `	pGen->pIn = &pBodyEnd[1];` |
|        43 |  5972 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       ! 0 |  5973 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|       ! 0 |  5974 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5975 | `			return SXERR_ABORT;` |
|         - |  5976 | `		}` |
|       ! 0 |  5977 | `	}` |
|        43 |  5978 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|        43 |  5979 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|        43 |  5980 | `	bHasStrictTypes = 0;` |
|         - |  5981 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|         - |  5982 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|         - |  5983 | `	 * directive appears anywhere in the list, before validating values. */` |
|        43 |  5984 | `	pCursor = pBodyStart;` |
|        55 |  5985 | `	while( pCursor < pBodyEnd ){` |
|        51 |  5986 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|        43 |  5987 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|        39 |  5988 | `				bHasStrictTypes = 1;` |
|        39 |  5989 | `				break;` |
|         - |  5990 | `			}` |
|         2 |  5991 | `		}` |
|        14 |  5992 | `		pCursor++;` |
|         2 |  5993 | `	}` |
|        43 |  5994 | `	if( bHasStrictTypes && bBlockForm ){` |
|         3 |  5995 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5996 | `			"strict_types declaration must not use block mode");` |
|         3 |  5997 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         3 |  5998 | `		return SXRET_OK;` |
|         - |  5999 | `	}` |
|        41 |  6000 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|         6 |  6001 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6002 | `			"strict_types declaration must be the very first statement in the script");` |
|         6 |  6003 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         6 |  6004 | `		return SXRET_OK;` |
|         - |  6005 | `	}` |
|         - |  6006 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|        37 |  6007 | `	pCursor = pBodyStart;` |
|        69 |  6008 | `	while( pCursor < pBodyEnd ){` |
|         - |  6009 | `		SyToken *pNameTok;` |
|         - |  6010 | `		SyToken *pEqTok;` |
|         - |  6011 | `		SyToken *pValTok;` |
|         - |  6012 | `		SyString *pDirName;` |
|         - |  6013 | `		int bIsStrict;` |
|         - |  6014 | `		int iStrictValue;` |
|        39 |  6015 | `		pNameTok = pCursor;` |
|        39 |  6016 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  6017 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6018 | `				"declare: Expecting a directive name");` |
|       ! 0 |  6019 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6020 | `			return SXRET_OK;` |
|         - |  6021 | `		}` |
|        39 |  6022 | `		pEqTok = pNameTok + 1;` |
|        39 |  6023 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|       ! 0 |  6024 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6025 | `				"declare: Expecting '=' after directive name");` |
|       ! 0 |  6026 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6027 | `			return SXRET_OK;` |
|         - |  6028 | `		}` |
|        39 |  6029 | `		pValTok = pEqTok + 1;` |
|        39 |  6030 | `		if( pValTok >= pBodyEnd ){` |
|       ! 0 |  6031 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6032 | `				"declare: Expecting value after '='");` |
|       ! 0 |  6033 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6034 | `			return SXRET_OK;` |
|         - |  6035 | `		}` |
|        39 |  6036 | `		pDirName = &pNameTok->sData;` |
|        39 |  6037 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|        39 |  6038 | `		if( bIsStrict ){` |
|         - |  6039 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|         - |  6040 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|        35 |  6041 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       ! 0 |  6042 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6043 | `					"declare(strict_types) value must be a literal");` |
|       ! 0 |  6044 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6045 | `				return SXRET_OK;` |
|         - |  6046 | `			}` |
|        35 |  6047 | `			iStrictValue = -1;` |
|        35 |  6048 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|        35 |  6049 | `				const char *zv = SyStringData(&pValTok->sData);` |
|        35 |  6050 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|        35 |  6051 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|        33 |  6052 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|        15 |  6053 | `			}` |
|        35 |  6054 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|         3 |  6055 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6056 | `					"strict_types declaration must have 0 or 1 as its value");` |
|         3 |  6057 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         3 |  6058 | `				return SXRET_OK;` |
|         - |  6059 | `			}` |
|        32 |  6060 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|        18 |  6061 | `		}else{` |
|         - |  6062 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|         - |  6063 | `			 * preserve the legacy notice so callers relying on the old` |
|         - |  6064 | `			 * behavior don't regress. */` |
|         8 |  6065 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|         - |  6066 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|         2 |  6067 | `				ph7_lib_version()` |
|         - |  6068 | `				);` |
|         - |  6069 | `		}` |
|        36 |  6070 | `		pCursor = pValTok + 1;` |
|         - |  6071 | `		/* Consume separating comma (or end). */` |
|        36 |  6072 | `		if( pCursor < pBodyEnd ){` |
|         3 |  6073 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  6074 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6075 | `					"declare: Expecting ',' or ')' after directive value");` |
|       ! 0 |  6076 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6077 | `				return SXRET_OK;` |
|         - |  6078 | `			}` |
|         3 |  6079 | `			pCursor++;` |
|         1 |  6080 | `		}` |
|         4 |  6081 | `	}` |
|         - |  6082 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|         - |  6083 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|         - |  6084 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|        34 |  6085 | `	return SXRET_OK;` |
|         2 |  6086 | `Synchro:` |
|         - |  6087 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|        16 |  6088 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|        12 |  6089 | `		pGen->pIn++;` |
|         2 |  6090 | `	}` |
|         6 |  6091 | `	return SXRET_OK;` |
|        26 |  6092 | `}` |
|         - |  6093 | `/*` |
|         - |  6094 | ` * Process default argument values. That is,a function may define C++-style default value` |
|         - |  6095 | ` * as follows:` |
|         - |  6096 | ` * function makecoffee($type = "cappuccino")` |
|         - |  6097 | ` * {` |
|         - |  6098 | ` *   return "Making a cup of $type.\n";` |
|         - |  6099 | ` * }` |
|         - |  6100 | ` * Symisc eXtension.` |
|         - |  6101 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|         - |  6102 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|         - |  6103 | ` *      Example: Work only with PH7,generate error under zend` |
|         - |  6104 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|         - |  6105 | ` *      {` |
|         - |  6106 | ` *       var_dump($a);` |
|         - |  6107 | ` *      }` |
|         - |  6108 | ` *     //call test without args` |
|         - |  6109 | ` *      test();` |
|         - |  6110 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|         - |  6111 | ` *      Example:` |
|         - |  6112 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|         - |  6113 | ` * 3 -) Function overloading!!` |
|         - |  6114 | ` *      Example:` |
|         - |  6115 | ` *      function foo($a) {` |
|         - |  6116 | ` *   	  return $a.PHP_EOL;` |
|         - |  6117 | ` *	    }` |
|         - |  6118 | ` *	    function foo($a, $b) {` |
|         - |  6119 | ` *   	  return $a + $b;` |
|         - |  6120 | ` *	    }` |
|         - |  6121 | ` *	    echo foo(5); // Prints "5"` |
|         - |  6122 | ` *	    echo foo(5, 2); // Prints "7"` |
|         - |  6123 | ` *      // Same arg` |
|         - |  6124 | ` *	   function foo(string $a)` |
|         - |  6125 | ` *	   {` |
|         - |  6126 | ` *	     echo "a is a string\n";` |
|         - |  6127 | ` *	     var_dump($a);` |
|         - |  6128 | ` *	   }` |
|         - |  6129 | ` *	  function foo(int $a)` |
|         - |  6130 | ` *	  {` |
|         - |  6131 | ` *	    echo "a is integer\n";` |
|         - |  6132 | ` *	    var_dump($a);` |
|         - |  6133 | ` *	  }` |
|         - |  6134 | ` *	  function foo(array $a)` |
|         - |  6135 | ` *	  {` |
|         - |  6136 | ` * 	    echo "a is an array\n";` |
|         - |  6137 | ` * 	    var_dump($a);` |
|         - |  6138 | ` *	  }` |
|         - |  6139 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|         - |  6140 | ` *	  foo(52); // a is integer [second foo]` |
|         - |  6141 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|         - |  6142 | ` * Please refer to the official documentation for more information on the powerful extension` |
|         - |  6143 | ` * introduced by the PH7 engine.` |
|         - |  6144 | ` */` |
|    451882 |  6145 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|         5 |  6146 | `{` |
|         - |  6147 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  6148 | `	SySet *pInstrContainer;` |
|         - |  6149 | `	sxi32 rc;` |
|         - |  6150 | `	/* Swap token stream */` |
|    451887 |  6151 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|    451887 |  6152 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    451887 |  6153 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|         - |  6154 | `	/* Compile the expression holding the argument value */` |
|    451887 |  6155 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  6156 | `	/* Emit the done instruction */` |
|    451887 |  6157 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|    451887 |  6158 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    451887 |  6159 | `	RE_SWAP_DELIMITER(pGen);` |
|    451887 |  6160 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  6161 | `		return SXERR_ABORT;` |
|         - |  6162 | `	}` |
|    451887 |  6163 | `	return SXRET_OK;` |
|    225946 |  6164 | `}` |
|         - |  6165 | `/*` |
|         - |  6166 | ` * Collect function arguments one after one.` |
|         - |  6167 | ` * According to the PHP language reference manual.` |
|         - |  6168 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|         - |  6169 | ` * list of expressions.` |
|         - |  6170 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|         - |  6171 | ` * and default argument values. Variable-length argument lists are also supported,` |
|         - |  6172 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|         - |  6173 | ` * for more information.` |
|         - |  6174 | ` * Example #1 Passing arrays to functions` |
|         - |  6175 | ` * <?php` |
|         - |  6176 | ` * function takes_array($input)` |
|         - |  6177 | ` * {` |
|         - |  6178 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|         - |  6179 | ` * }` |
|         - |  6180 | ` * ?>` |
|         - |  6181 | ` * Making arguments be passed by reference` |
|         - |  6182 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|         - |  6183 | ` * within the function is changed, it does not get changed outside of the function).` |
|         - |  6184 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|         - |  6185 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|         - |  6186 | ` * to the argument name in the function definition:` |
|         - |  6187 | ` * Example #2 Passing function parameters by reference` |
|         - |  6188 | ` * <?php` |
|         - |  6189 | ` * function add_some_extra(&$string)` |
|         - |  6190 | ` * {` |
|         - |  6191 | ` *   $string .= 'and something extra.';` |
|         - |  6192 | ` * }` |
|         - |  6193 | ` * $str = 'This is a string, ';` |
|         - |  6194 | ` * add_some_extra($str);` |
|         - |  6195 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|         - |  6196 | ` * ?>` |
|         - |  6197 | ` *` |
|         - |  6198 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|         - |  6199 | ` * complex agrument values.Please refer to the official documentation for more information` |
|         - |  6200 | ` * on these extension.` |
|         - |  6201 | ` */` |
|   1091360 |  6202 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|         5 |  6203 | `{` |
|         - |  6204 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|         - |  6205 | `	SyToken *pIn;  /* Token stream */` |
|         - |  6206 | `	SyBlob sSig;         /* Function signature */` |
|         - |  6207 | `	char *zDup;          /* Copy of argument name */` |
|         - |  6208 | `	sxi32 rc;` |
|         - |  6209 |  |
|   1091365 |  6210 | `	pIn = pGen->pIn;` |
|   1091365 |  6211 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|         - |  6212 | `	/* Process arguments one after one */` |
|   1368938 |  6213 | `	for(;;){` |
|   2737881 |  6214 | `		if( pIn >= pEnd ){` |
|         - |  6215 | `			/* No more arguments to process */` |
|   1091349 |  6216 | `			break;` |
|         - |  6217 | `		}` |
|   1646537 |  6218 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   1646537 |  6219 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   1646537 |  6220 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   1646537 |  6221 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|   1646537 |  6222 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|         - |  6223 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|         - |  6224 | `		 * first token inside the main token stream */` |
|   1646537 |  6225 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  6226 | `			return SXERR_ABORT;` |
|         - |  6227 | `		}` |
|         - |  6228 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|         - |  6229 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|         - |  6230 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|         - |  6231 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|         - |  6232 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|         - |  6233 | `		{` |
|   1646537 |  6234 | `			int bReadonly = 0, bVisSeen = 0;` |
|   1646537 |  6235 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|   1646537 |  6236 | `			sxi32 iSetVisFlag = 0;` |
|         - |  6237 | `			int nSetTok;` |
|         - |  6238 | `			sxi32 nSetVis;` |
|   1646537 |  6239 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|         3 |  6240 | `				bReadonly = 1;` |
|         3 |  6241 | `				pIn++;` |
|         1 |  6242 | `			}` |
|   1646537 |  6243 | `			nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|   1646537 |  6244 | `			if( nSetVis ){` |
|         - |  6245 | ``				/* Leading `private(set)` etc: promoted with a public read side */`` |
|         3 |  6246 | `				iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  6247 | `				bVisSeen = 1;` |
|         3 |  6248 | `				pIn += nSetTok;` |
|         3 |  6249 | `				if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       ! 0 |  6250 | `					bReadonly = 1;` |
|       ! 0 |  6251 | `					pIn++;` |
|         1 |  6252 | `				}` |
|   1646536 |  6253 | `			}else if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|     87707 |  6254 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|     87707 |  6255 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|        89 |  6256 | `					bVisSeen = 1;` |
|        89 |  6257 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|       120 |  6258 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|        39 |  6259 | `						: PH7_CLASS_PROT_PUBLIC;` |
|        89 |  6260 | `					pIn++;` |
|        89 |  6261 | `					nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|        89 |  6262 | `					if( nSetVis ){` |
|         - |  6263 | ``						/* `public private(set) T $x` promoted form */`` |
|         3 |  6264 | `						iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  6265 | `						pIn += nSetTok;` |
|         1 |  6266 | `					}` |
|        89 |  6267 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|        18 |  6268 | `						bReadonly = 1;` |
|        18 |  6269 | `						pIn++;` |
|         7 |  6270 | `					}` |
|        42 |  6271 | `				}` |
|     43851 |  6272 | `			}` |
|   1646537 |  6273 | `			if( iSetVisFlag == PH7_CLASS_ATTR_PRIVATE_SET ){` |
|         5 |  6274 | `				sArg.iFlags \|= VM_FUNC_ARG_PRIV_SET;` |
|   1646535 |  6275 | `			}else if( iSetVisFlag == PH7_CLASS_ATTR_PROTECTED_SET ){` |
|       ! 0 |  6276 | `				sArg.iFlags \|= VM_FUNC_ARG_PROT_SET;` |
|       ! 0 |  6277 | `			}` |
|   1646537 |  6278 | `			if( bVisSeen \|\| bReadonly ){` |
|        93 |  6279 | `				if( !bCtorCtx ){` |
|         6 |  6280 | `					if( bAbstractCtx ){` |
|         3 |  6281 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  6282 | `							"Cannot declare promoted property in an abstract constructor");` |
|         2 |  6283 | `					}else{` |
|         3 |  6284 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  6285 | `							"Cannot declare promoted property outside a constructor");` |
|         - |  6286 | `					}` |
|         6 |  6287 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  6288 | `						return SXERR_ABORT;` |
|         - |  6289 | `					}` |
|         6 |  6290 | `					return SXERR_SYNTAX;` |
|         - |  6291 | `				}` |
|        89 |  6292 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|        89 |  6293 | `				sArg.iPromoteVis = iVis;` |
|        89 |  6294 | `				if( bReadonly ){` |
|        20 |  6295 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|         8 |  6296 | `				}` |
|        42 |  6297 | `			}` |
|         - |  6298 | `		}` |
|         - |  6299 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|   1646528 |  6300 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|    895702 |  6301 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|    139170 |  6302 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|    114397 |  6303 | `			sxu32 nLineLocal = pIn->nLine;` |
|    114397 |  6304 | `			sxi32 iTFlags = 0;` |
|    114397 |  6305 | `			pGen->pIn = pIn;` |
|    114397 |  6306 | `			rc = GenStateParseUnionTypeDecl(` |
|     57196 |  6307 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|     57196 |  6308 | `				&iTFlags, &sArg.sTypeName,` |
|         - |  6309 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|         - |  6310 | `				/* bAllowVoid */ 0,` |
|     57196 |  6311 | `						nLineLocal);` |
|    114397 |  6312 | `			pIn = pGen->pIn;` |
|    114397 |  6313 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  6314 | `				return SXERR_ABORT;` |
|    114397 |  6315 | `			}else if( rc == SXERR_CORRUPT ){` |
|         - |  6316 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|         3 |  6317 | `				return SXERR_SYNTAX;` |
|    114395 |  6318 | `			}else if( rc == SXERR_SYNTAX ){` |
|        11 |  6319 | `				if( pIn < pEnd ){` |
|        15 |  6320 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|         - |  6321 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|         4 |  6322 | `						&pIn->sData);` |
|         7 |  6323 | `				}else{` |
|       ! 0 |  6324 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|         - |  6325 | `						"syntax error, unexpected end of file");` |
|         - |  6326 | `				}` |
|        11 |  6327 | `				return SXERR_SYNTAX;` |
|         - |  6328 | `			}` |
|    114387 |  6329 | `			sArg.iFlags \|= iTFlags;` |
|     57191 |  6330 | `		}` |
|   1646523 |  6331 | `		if( pIn >= pEnd ){` |
|       ! 0 |  6332 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|       ! 0 |  6333 | `			return rc;` |
|         - |  6334 | `		}` |
|   1646523 |  6335 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|         - |  6336 | `			/* Pass by reference,record that */` |
|     11435 |  6337 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|     11435 |  6338 | `			pIn++;` |
|      5715 |  6339 | `		}` |
|   1646523 |  6340 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|         - |  6341 | `			/* Variadic parameter: ...$args */` |
|     19089 |  6342 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|     19089 |  6343 | `			pIn++;` |
|      9542 |  6344 | `		}` |
|   1646523 |  6345 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  6346 | `			/* Invalid argument */` |
|       ! 0 |  6347 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|       ! 0 |  6348 | `			return rc;` |
|         - |  6349 | `		}` |
|   1646523 |  6350 | `		pIn++; /* Jump the dollar sign */` |
|         - |  6351 | `		/* Copy argument name */` |
|   1646523 |  6352 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   1646523 |  6353 | `		if( zDup == 0 ){` |
|       ! 0 |  6354 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  6355 | `			return SXERR_ABORT;` |
|         - |  6356 | `		}` |
|   1646523 |  6357 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   1646523 |  6358 | `		pIn++;` |
|   1646523 |  6359 | `		if( pIn < pEnd ){` |
|    851381 |  6360 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|         - |  6361 | `				SyToken *pDefend;` |
|    451889 |  6362 | `				sxi32 iNest = 0;` |
|    451889 |  6363 | `				pIn++; /* Jump the equal sign */` |
|    451889 |  6364 | `				pDefend = pIn;` |
|         - |  6365 | `				/* Process the default value associated with this argument */` |
|    953147 |  6366 | `				while( pDefend < pEnd ){` |
|    656949 |  6367 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|    155691 |  6368 | `						break;` |
|         - |  6369 | `					}` |
|    501263 |  6370 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|         - |  6371 | `						/* Increment nesting level */` |
|     26585 |  6372 | `						iNest++;` |
|    487973 |  6373 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|         - |  6374 | `						/* Decrement nesting level */` |
|     26585 |  6375 | `						iNest--;` |
|     13290 |  6376 | `					}` |
|    501263 |  6377 | `					pDefend++;` |
|         5 |  6378 | `				}` |
|    451889 |  6379 | `				if( pIn >= pDefend ){` |
|         3 |  6380 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|         3 |  6381 | `					return rc;` |
|         - |  6382 | `				}` |
|         - |  6383 | `				/* Process default value */` |
|    451887 |  6384 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|    451887 |  6385 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  6386 | `					return rc;` |
|         - |  6387 | `				}` |
|         - |  6388 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|         - |  6389 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|         - |  6390 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|         - |  6391 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|         - |  6392 | `				 * arg-type check lets null through. */` |
|    451882 |  6393 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|    250638 |  6394 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|    250635 |  6395 | `					&& &pIn[1] == pDefend` |
|     45591 |  6396 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|     34186 |  6397 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|     20889 |  6398 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|     15195 |  6399 | `					sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|         - |  6400 | `					/* php 8.4: the implicit form is deprecated at COMPILE time —` |
|         - |  6401 | `` 					 * `f(): Implicitly marking parameter $x as nullable …` `` |
|         - |  6402 | `					 * (methods carry the Class:: prefix when the class link is` |
|         - |  6403 | `					 * already up at this point). */` |
|         - |  6404 | `					{` |
|     15195 |  6405 | `						const char *zSep = "";` |
|     15195 |  6406 | `						SyString sCls = { "", 0 };` |
|     15195 |  6407 | `						if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|     15189 |  6408 | `							sCls = ((ph7_class *)pFunc->pUserData)->sName;` |
|     15189 |  6409 | `							zSep = "::";` |
|      7592 |  6410 | `						}` |
|     22790 |  6411 | `						PH7_GenCompileError(&(*pGen),8192 /* E_DEPRECATED */,pIn->nLine,` |
|         - |  6412 | `							"%z%s%z(): Implicitly marking parameter $%z as nullable is deprecated, the explicit nullable type must be used instead",` |
|      7595 |  6413 | `							&sCls,zSep,&pFunc->sName,&sArg.sName);` |
|         - |  6414 | `					}` |
|      7595 |  6415 | `				}` |
|         - |  6416 | `				/* Point beyond the default value */` |
|    451887 |  6417 | `				pIn = pDefend;` |
|    225941 |  6418 | `			}` |
|    851379 |  6419 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  6420 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|       ! 0 |  6421 | `				return rc;` |
|         - |  6422 | `			}` |
|    851379 |  6423 | `			pIn++; /* Jump the trailing comma */` |
|    425687 |  6424 | `		}` |
|         - |  6425 | `		/* Append argument signature */` |
|   1646521 |  6426 | `		if( sArg.nType > 0 ){` |
|    114325 |  6427 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|         - |  6428 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|     26657 |  6429 | `				int marker = 'o';` |
|     26657 |  6430 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|     26657 |  6431 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|     13331 |  6432 | `			}else{` |
|         - |  6433 | `				int c;` |
|     87673 |  6434 | `				c = 'n'; /* cc warning */` |
|         - |  6435 | `				/* Type leading character */` |
|     87673 |  6436 | `				switch(sArg.nType){` |
|      5700 |  6437 | `				case MEMOBJ_HASHMAP:` |
|         - |  6438 | `					/* Hashmap aka 'array' */` |
|     11405 |  6439 | `					c = 'h';` |
|     11405 |  6440 | `					break;` |
|      9610 |  6441 | `				case MEMOBJ_INT:` |
|         - |  6442 | `					/* Integer */` |
|     19225 |  6443 | `					c = 'i';` |
|     19225 |  6444 | `					break;` |
|         2 |  6445 | `				case MEMOBJ_BOOL:` |
|         - |  6446 | `					/* Bool */` |
|         5 |  6447 | `					c = 'b';` |
|         5 |  6448 | `					break;` |
|         5 |  6449 | `				case MEMOBJ_REAL:` |
|         - |  6450 | `					/* Float */` |
|        12 |  6451 | `					c = 'f';` |
|        12 |  6452 | `					break;` |
|     28509 |  6453 | `				case MEMOBJ_STRING:` |
|         - |  6454 | `					/* String */` |
|     57023 |  6455 | `					c = 's';` |
|     57023 |  6456 | `					break;` |
|         7 |  6457 | `				case MEMOBJ_OBJ:` |
|         - |  6458 | `					/* Object */` |
|        16 |  6459 | `					c = 'o';` |
|        14 |  6460 | `					break;` |
|         1 |  6461 | `				default:` |
|         2 |  6462 | `					break;` |
|         - |  6463 | `				}` |
|     87673 |  6464 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|         - |  6465 | `			}` |
|     57165 |  6466 | `		}else{` |
|         - |  6467 | `			/* No type is associated with this parameter which mean` |
|         - |  6468 | `			 * that this function is not condidate for overloading.` |
|         - |  6469 | `			 */` |
|   1532201 |  6470 | `			SyBlobRelease(&sSig);` |
|         - |  6471 | `		}` |
|         - |  6472 | `		/* Save in the argument set */` |
|   1646521 |  6473 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|         5 |  6474 | `	}` |
|   1091349 |  6475 | `	if( SyBlobLength(&sSig) > 0 ){` |
|         - |  6476 | `		/* Save function signature */` |
|     83885 |  6477 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|     41940 |  6478 | `	}` |
|   1091349 |  6479 | `	return SXRET_OK;` |
|    545685 |  6480 | `}` |
|         - |  6481 | `/*` |
|         - |  6482 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|         - |  6483 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|         - |  6484 | ` * the enclosing function. Returns the token just past the nested construct.` |
|         - |  6485 | ` */` |
|     34206 |  6486 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|         5 |  6487 | `{` |
|     34211 |  6488 | `	sxi32 iParen = 0;` |
|     34211 |  6489 | `	pIn++; /* past 'function'/'fn' */` |
|         - |  6490 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|         - |  6491 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|         - |  6492 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|    152073 |  6493 | `	while( pIn < pEnd ){` |
|    152073 |  6494 | `		sxu32 t = pIn->nType;` |
|    152073 |  6495 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|    148223 |  6496 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|    102617 |  6497 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|     83595 |  6498 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|    117867 |  6499 | `		pIn++;` |
|         5 |  6500 | `	}` |
|     19027 |  6501 | `	if( pIn >= pEnd ){ return pIn; }` |
|         - |  6502 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|         - |  6503 | `	{` |
|     19027 |  6504 | `		sxi32 d = 0;` |
|    755829 |  6505 | `		while( pIn < pEnd ){` |
|    755829 |  6506 | `			sxu32 t = pIn->nType;` |
|    755829 |  6507 | `			if( t & PH7_TK_OCB ){ d++; }` |
|    725415 |  6508 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|    736807 |  6509 | `			pIn++;` |
|         5 |  6510 | `		}` |
|         - |  6511 | `	}` |
|     19027 |  6512 | `	return pIn;` |
|     17108 |  6513 | `}` |
|         - |  6514 | `/*` |
|         - |  6515 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|         - |  6516 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|         - |  6517 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|         - |  6518 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|         - |  6519 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|         - |  6520 | ` * detached-mini-program path untouched.` |
|         - |  6521 | ` */` |
|         - |  6522 | `/*` |
|         - |  6523 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|         - |  6524 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|         - |  6525 | ` * mixed, object.` |
|         - |  6526 | ` */` |
|     11416 |  6527 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|         5 |  6528 | `{` |
|         - |  6529 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|         - |  6530 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|         - |  6531 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|         - |  6532 | `	};` |
|         - |  6533 | `	sxu32 i;` |
|     11421 |  6534 | `	if( nName > 0 && zName[0] == '\\' ){` |
|       ! 0 |  6535 | `		zName++;` |
|       ! 0 |  6536 | `		nName--;` |
|       ! 0 |  6537 | `	}` |
|     11453 |  6538 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|     11449 |  6539 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|     11417 |  6540 | `			return 1;` |
|         - |  6541 | `		}` |
|        17 |  6542 | `	}` |
|         5 |  6543 | `	return 0;` |
|      5713 |  6544 | `}` |
|         - |  6545 | `/*` |
|         - |  6546 | ` * One atom of a generator's declared return type: is it a supertype of` |
|         - |  6547 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|         - |  6548 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|         - |  6549 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|         - |  6550 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|         - |  6551 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|         - |  6552 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|         - |  6553 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|         - |  6554 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|         - |  6555 | ` * both rather than fatal on valid code (a recorded divergence).` |
|         - |  6556 | ` */` |
|     11414 |  6557 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|         5 |  6558 | `{` |
|     11419 |  6559 | `	if( nType == MEMOBJ_OBJ ){` |
|       ! 0 |  6560 | ``		return 1; /* bare `object` */`` |
|         - |  6561 | `	}` |
|     11419 |  6562 | `	if( nType != SXU32_HIGH ){` |
|         3 |  6563 | `		return 0; /* scalar/array/void/never/null/... */` |
|         - |  6564 | `	}` |
|     11417 |  6565 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|     11413 |  6566 | `		return 1;` |
|         - |  6567 | `	}` |
|         - |  6568 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|         - |  6569 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|         - |  6570 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|         - |  6571 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|         - |  6572 | `	{` |
|         - |  6573 | `		SyBlob sFQN;` |
|         - |  6574 | `		int bOk;` |
|         5 |  6575 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|         5 |  6576 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|         5 |  6577 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|         5 |  6578 | `		SyBlobRelease(&sFQN);` |
|         5 |  6579 | `		return bOk;` |
|         - |  6580 | `	}` |
|      5712 |  6581 | `}` |
|         - |  6582 | `/*` |
|         - |  6583 | ` * php 8: a generator function may only declare a return type that is a` |
|         - |  6584 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|         - |  6585 | ` * group qualifies only if every member does. Anything else is php's exact` |
|         - |  6586 | ` * compile-time fatal "Generator return type must be a supertype of` |
|         - |  6587 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|         - |  6588 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|         - |  6589 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|         - |  6590 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|         - |  6591 | ` */` |
|     11652 |  6592 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|         5 |  6593 | `{` |
|     11657 |  6594 | `	int bOk = 0;` |
|         - |  6595 | `	sxu32 nLine;` |
|         - |  6596 | `	sxi32 rc;` |
|     11657 |  6597 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|       243 |  6598 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|         - |  6599 | `	}` |
|     11419 |  6600 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|       ! 0 |  6601 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|       ! 0 |  6602 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|         - |  6603 | `		sxu32 i,j;` |
|       ! 0 |  6604 | `		for( i = 0; i < n && !bOk; i++ ){` |
|         - |  6605 | `			int bGroupOk;` |
|       ! 0 |  6606 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|       ! 0 |  6607 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|         - |  6608 | `			}` |
|       ! 0 |  6609 | `			bGroupOk = 1;` |
|       ! 0 |  6610 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|       ! 0 |  6611 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|       ! 0 |  6612 | `					bGroupOk = 0;` |
|       ! 0 |  6613 | `					break;` |
|         - |  6614 | `				}` |
|       ! 0 |  6615 | `			}` |
|       ! 0 |  6616 | `			bOk = bGroupOk;` |
|       ! 0 |  6617 | `		}` |
|       ! 0 |  6618 | `	}else{` |
|     11419 |  6619 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|         - |  6620 | `	}` |
|     11419 |  6621 | `	if( bOk ){` |
|     11417 |  6622 | `		return SXRET_OK;` |
|         - |  6623 | `	}` |
|         - |  6624 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|         - |  6625 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|         - |  6626 | `	 * token of this stream — its line is the function's closing brace. php` |
|         - |  6627 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|         - |  6628 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|         3 |  6629 | `	nLine = pGen->pIn[-1].nLine;` |
|         - |  6630 | `	{` |
|         3 |  6631 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|         3 |  6632 | `		if( sGiven.nByte < 1 ){` |
|       ! 0 |  6633 | `			sGiven = pFunc->sReturnClass;` |
|       ! 0 |  6634 | `		}` |
|         3 |  6635 | `		if( sGiven.nByte < 1 ){` |
|         - |  6636 | ``			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the`` |
|         - |  6637 | `			 * rendered type text, so sReturnTypeName arrives empty for them —` |
|         - |  6638 | `			 * name them here (the root fix belongs to that renderer, §3.7). */` |
|       ! 0 |  6639 | `			const char *zScalar =` |
|       ! 0 |  6640 | `				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :` |
|       ! 0 |  6641 | `				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";` |
|       ! 0 |  6642 | `			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));` |
|       ! 0 |  6643 | `		}` |
|         3 |  6644 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  6645 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|         - |  6646 | `	}` |
|         3 |  6647 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|      5831 |  6648 | `}` |
|   2531674 |  6649 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|         5 |  6650 | `{` |
|   2531679 |  6651 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|   2531679 |  6652 | `	SyToken *pEnd = pGen->pEnd;` |
|   2531679 |  6653 | `	sxi32 iDepth = 0;` |
|   2531679 |  6654 | `	int bStarted = 0;` |
| 111726339 |  6655 | `	while( pIn < pEnd ){` |
| 111726339 |  6656 | `		sxu32 t = pIn->nType;` |
| 111726339 |  6657 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 106601031 |  6658 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 101510265 |  6659 | `		if( t & PH7_TK_KEYWORD ){` |
|   7407479 |  6660 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|   7407479 |  6661 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|   7395827 |  6662 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|         - |  6663 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|   3680808 |  6664 | `		}` |
| 101464407 |  6665 | `		pIn++;` |
|         5 |  6666 | `	}` |
|   2520027 |  6667 | `	return FALSE;` |
|   1265842 |  6668 | `}` |
|         - |  6669 | `/*` |
|         - |  6670 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|         - |  6671 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  6672 | ` * and this routine takes care of generating the appropriate error message.` |
|         - |  6673 | ` */` |
|   2531674 |  6674 | `static sxi32 GenStateCompileFuncBody(` |
|         - |  6675 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |  6676 | `	ph7_vm_func *pFunc    /* Function state */` |
|         - |  6677 | `	)` |
|         5 |  6678 | `{` |
|         - |  6679 | `	SySet *pInstrContainer; /* Instruction container */` |
|         - |  6680 | `	GenBlock *pBlock;` |
|         - |  6681 | `	sxu32 nGotoOfft;` |
|         - |  6682 | `	sxi32 rc;` |
|         - |  6683 | `	/* Attach the new function */` |
|   2531679 |  6684 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   2531679 |  6685 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  6686 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|         - |  6687 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  6688 | `		return SXERR_ABORT;` |
|         - |  6689 | `	}` |
|   2531679 |  6690 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|         - |  6691 | `	/* Swap bytecode containers */` |
|   2531679 |  6692 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   2531679 |  6693 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|         - |  6694 | `	/* Emit constructor property promotion prologue:` |
|         - |  6695 | `	 *   $this->NAME = $NAME;` |
|         - |  6696 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|         - |  6697 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|         - |  6698 | `	{` |
|   2531679 |  6699 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|         - |  6700 | `		sxu32 i;` |
|   4124925 |  6701 | `		for( i = 0; i < nArg; i++ ){` |
|   1593251 |  6702 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|         - |  6703 | `			char *zSrc;` |
|         - |  6704 | `			sxu32 nSrc,nName;` |
|         - |  6705 | `			SySet sToken;` |
|         - |  6706 | `			SyToken *pTmpIn,*pTmpEnd;` |
|         - |  6707 | `			sxi32 rcPromote;` |
|   1593251 |  6708 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   1593177 |  6709 | `				continue;` |
|         - |  6710 | `			}` |
|         - |  6711 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|         - |  6712 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|         - |  6713 | `			 * copied), so it must outlive the function — never free it. The` |
|         - |  6714 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|         - |  6715 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|        79 |  6716 | `			nName = SyStringLength(&pArg->sName);` |
|        79 |  6717 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|        79 |  6718 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|        79 |  6719 | `			if( zSrc == 0 ){` |
|       ! 0 |  6720 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  6721 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  6722 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  6723 | `				return SXERR_ABORT;` |
|         - |  6724 | `			}` |
|         - |  6725 | `			{` |
|        79 |  6726 | `				char *z = zSrc;` |
|        79 |  6727 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|        79 |  6728 | `				z += sizeof("$this->")-1;` |
|        79 |  6729 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        79 |  6730 | `				z += nName;` |
|        79 |  6731 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|        79 |  6732 | `				z += sizeof(" = $")-1;` |
|        79 |  6733 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        79 |  6734 | `				z += nName;` |
|        79 |  6735 | `				*z = 0;` |
|         - |  6736 | `			}` |
|        79 |  6737 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        79 |  6738 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);` |
|        79 |  6739 | `			pTmpIn = pGen->pIn;` |
|        79 |  6740 | `			pTmpEnd = pGen->pEnd;` |
|        79 |  6741 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|        79 |  6742 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|        79 |  6743 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|        79 |  6744 | `			pGen->pIn = pTmpIn;` |
|        79 |  6745 | `			pGen->pEnd = pTmpEnd;` |
|        79 |  6746 | `			SySetRelease(&sToken);` |
|        79 |  6747 | `			if( rcPromote == SXERR_ABORT ){` |
|       ! 0 |  6748 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  6749 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  6750 | `				return SXERR_ABORT;` |
|         - |  6751 | `			}` |
|         - |  6752 | `			/* Discard the assignment result — this is a statement expression. */` |
|        79 |  6753 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        42 |  6754 | `		}` |
|         - |  6755 | `	}` |
|         - |  6756 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|         - |  6757 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|         - |  6758 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|         - |  6759 | `	 * generator — and vice versa — is classified independently. */` |
|         - |  6760 | `	{` |
|   2531679 |  6761 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|   2531679 |  6762 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|         - |  6763 | `		/* Compile the body */` |
|   2531679 |  6764 | `		PH7_CompileBlock(&(*pGen),0);` |
|   2531679 |  6765 | `		pGen->bInGenerator = bSavedGen;` |
|         - |  6766 | `	}` |
|         - |  6767 | `	/* Fix exception jumps now the destination is resolved */` |
|   2531679 |  6768 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - |  6769 | `	/* Emit the final return if not yet done */` |
|   2531679 |  6770 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - |  6771 | `	/* Fix gotos jumps now the destination is resolved */` |
|   2531679 |  6772 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|       ! 0 |  6773 | `		rc = SXERR_ABORT;` |
|       ! 0 |  6774 | `	}` |
|   2531679 |  6775 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|         - |  6776 | `	/* Restore the default container */` |
|   2531679 |  6777 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - |  6778 | `	/* Leave function block */` |
|   2531679 |  6779 | `	GenStateLeaveBlock(&(*pGen),0);` |
|   2531679 |  6780 | `	if( rc == SXERR_ABORT ){` |
|         - |  6781 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  6782 | `		return SXERR_ABORT;` |
|         - |  6783 | `	}` |
|         - |  6784 | `	/* Scan for yield opcodes to detect generator functions */` |
|         - |  6785 | `	{` |
|   2531679 |  6786 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|         - |  6787 | `		sxu32 i;` |
|  69186103 |  6788 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
|  66666081 |  6789 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     11657 |  6790 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     11657 |  6791 | `				break;` |
|         - |  6792 | `			}` |
|  33327217 |  6793 | `		}` |
|         - |  6794 | `	}` |
|   2531679 |  6795 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|         - |  6796 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|     11657 |  6797 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|       ! 0 |  6798 | `			return SXERR_ABORT;` |
|         - |  6799 | `		}` |
|      5826 |  6800 | `	}` |
|         - |  6801 | `	/* All done, function body compiled */` |
|   2531679 |  6802 | `	return SXRET_OK;` |
|   1265842 |  6803 | `}` |
|         - |  6804 | `/*` |
|         - |  6805 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|         - |  6806 | ` * According to the PHP language reference manual.` |
|         - |  6807 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|         - |  6808 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|         - |  6809 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|         - |  6810 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - |  6811 | ` *  Functions need not be defined before they are referenced.` |
|         - |  6812 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|         - |  6813 | ` *  a function even if they were defined inside and vice versa.` |
|         - |  6814 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|         - |  6815 | ` *  calls with over 32-64 recursion levels.` |
|         - |  6816 | ` *` |
|         - |  6817 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|         - |  6818 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|         - |  6819 | ` * on these extension.` |
|         - |  6820 | ` */` |
|         - |  6821 | `/*` |
|         - |  6822 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|         - |  6823 | ` */` |
|       570 |  6824 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|         5 |  6825 | `{` |
|         - |  6826 | `	sxu32 i;` |
|      1611 |  6827 | `	for( i = 0; i < n; i++ ){` |
|      1381 |  6828 | `		int a = zA[i], b = zB[i];` |
|      1381 |  6829 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|      1381 |  6830 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|      1381 |  6831 | `		if( a != b ) return a - b;` |
|       523 |  6832 | `	}` |
|       235 |  6833 | `	return 0;` |
|       290 |  6834 | `}` |
|         - |  6835 | `/*` |
|         - |  6836 | ` * Internal type-atom kinds used during union type parsing.` |
|         - |  6837 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|         - |  6838 | ` * (which are positive bit values stored in sxu32).` |
|         - |  6839 | ` */` |
|         - |  6840 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|         - |  6841 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|         - |  6842 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|         - |  6843 |  |
|         - |  6844 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|         - |  6845 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|         - |  6846 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|         - |  6847 |  |
|         - |  6848 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|         - |  6849 | `struct PhlTypeAtom {` |
|         - |  6850 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|         - |  6851 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|         - |  6852 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|         - |  6853 | `	sxu32 nCanon;` |
|         - |  6854 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|         - |  6855 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|         - |  6856 | `};` |
|         - |  6857 |  |
|         - |  6858 | `/*` |
|         - |  6859 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|         - |  6860 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|         - |  6861 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|         - |  6862 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|         - |  6863 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|         - |  6864 | ` * already be consumed by the caller.` |
|         - |  6865 | ` */` |
|    126960 |  6866 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|         5 |  6867 | `{` |
|    126965 |  6868 | `	SyToken *pIn = pGen->pIn;` |
|    126965 |  6869 | `	SyZero(pOut, sizeof(*pOut));` |
|    126965 |  6870 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|    126965 |  6871 | `	if( pIn >= pGen->pEnd ){` |
|       ! 0 |  6872 | `		return SXERR_SYNTAX;` |
|         - |  6873 | `	}` |
|         - |  6874 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|    126965 |  6875 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|         8 |  6876 | `		pIn++;` |
|         8 |  6877 | `		if( pIn >= pGen->pEnd ){` |
|       ! 0 |  6878 | `			return SXERR_SYNTAX;` |
|         - |  6879 | `		}` |
|         3 |  6880 | `	}` |
|    126965 |  6881 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  6882 | `		return SXERR_SYNTAX;` |
|         - |  6883 | `	}` |
|    126965 |  6884 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|     88465 |  6885 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|     88465 |  6886 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|     11437 |  6887 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|     82749 |  6888 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|        81 |  6889 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|     76995 |  6890 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|     19631 |  6891 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|     67144 |  6892 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|     57249 |  6893 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|     28709 |  6894 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|        41 |  6895 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|        67 |  6896 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|        27 |  6897 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|        37 |  6898 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|        13 |  6899 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|        23 |  6900 | `			pOut->nType = SXU32_HIGH;` |
|        23 |  6901 | `			pOut->sClass = pIn->sData;` |
|        13 |  6902 | `		}else{` |
|         3 |  6903 | `			return SXERR_SYNTAX;` |
|         - |  6904 | `		}` |
|     88463 |  6905 | `		pIn++;` |
|     44234 |  6906 | `	}else{` |
|         - |  6907 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|         - |  6908 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|     38505 |  6909 | `		SyString *pT = &pIn->sData;` |
|     38505 |  6910 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|        34 |  6911 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|        34 |  6912 | `			pIn++;` |
|     38490 |  6913 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|       177 |  6914 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|       177 |  6915 | `			pIn++;` |
|     38389 |  6916 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|        26 |  6917 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|        26 |  6918 | `			pIn++;` |
|        15 |  6919 | `		}else{` |
|         - |  6920 | `			/* Class / interface name; consume namespace path a\b\c */` |
|     38281 |  6921 | `			SyToken *pFirst = pIn;` |
|     38281 |  6922 | `			SyToken *pLast = pIn;` |
|     38281 |  6923 | `			pOut->nType = SXU32_HIGH;` |
|     38281 |  6924 | `			pOut->sClass = pIn->sData;` |
|     38281 |  6925 | `			pIn++;` |
|     57417 |  6926 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|     38284 |  6927 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|         3 |  6928 | `				pLast = &pIn[1];` |
|         3 |  6929 | `				pIn += 2;` |
|         1 |  6930 | `			}` |
|     38281 |  6931 | `			if( pLast != pFirst ){` |
|         3 |  6932 | `				const char *zFirst = pFirst->sData.zString;` |
|         3 |  6933 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|         3 |  6934 | `				pOut->sClass.zString = zFirst;` |
|         3 |  6935 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|         1 |  6936 | `			}` |
|         - |  6937 | `		}` |
|         - |  6938 | `	}` |
|    126963 |  6939 | `	pGen->pIn = pIn;` |
|    126963 |  6940 | `	return SXRET_OK;` |
|     63485 |  6941 | `}` |
|         - |  6942 |  |
|         - |  6943 | `/*` |
|         - |  6944 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|         - |  6945 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|         - |  6946 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|         - |  6947 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|         - |  6948 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|         - |  6949 | ` */` |
|    126782 |  6950 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|         5 |  6951 | `{` |
|         - |  6952 | `	int i;` |
|    126787 |  6953 | `	int nNonNull = 0;` |
|    126787 |  6954 | `	int bAnyIntersection = 0;` |
|         - |  6955 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|    126787 |  6956 | `	sxu32 nMaxGroup = 0;` |
|   4183811 |  6957 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    253721 |  6958 | `	for( i = 0; i < nAtoms; i++ ){` |
|    126939 |  6959 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    126909 |  6960 | `			nNonNull++;` |
|    126909 |  6961 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|    126909 |  6962 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|    126909 |  6963 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|     63452 |  6964 | `			}` |
|     63452 |  6965 | `		}` |
|     63472 |  6966 | `	}` |
|    253669 |  6967 | `	for( i = 0; i < nAtoms; i++ ){` |
|    126911 |  6968 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        29 |  6969 | `			bAnyIntersection = 1;` |
|        29 |  6970 | `			break;` |
|         - |  6971 | `		}` |
|     63446 |  6972 | `	}` |
|    126787 |  6973 | `	if( bAnyIntersection ){` |
|         - |  6974 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|         - |  6975 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|         - |  6976 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|        29 |  6977 | `		sxu32 g, nGroups = 0;` |
|        29 |  6978 | `		int bFirstGroup = 1;` |
|        59 |  6979 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|        59 |  6980 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|        35 |  6981 | `			int bFirstMember = 1;` |
|         - |  6982 | `			int bWrap;` |
|        35 |  6983 | `			if( aGroupCount[g] == 0 ) continue;` |
|         - |  6984 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|         - |  6985 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|         - |  6986 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|         - |  6987 | `			 * parens, matching PHP's canonical text. */` |
|        47 |  6988 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|        35 |  6989 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|        35 |  6990 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|       107 |  6991 | `			for( i = 0; i < nAtoms; i++ ){` |
|        77 |  6992 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|        59 |  6993 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|        59 |  6994 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|        55 |  6995 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        30 |  6996 | `				}else{` |
|         6 |  6997 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  6998 | `				}` |
|        59 |  6999 | `				bFirstMember = 0;` |
|        32 |  7000 | `			}` |
|        35 |  7001 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|        35 |  7002 | `			bFirstGroup = 0;` |
|        20 |  7003 | `		}` |
|        29 |  7004 | `		if( bNullable ){` |
|       ! 0 |  7005 | `			SyBlobAppend(pBlob, "\|", 1);` |
|       ! 0 |  7006 | `			SyBlobAppend(pBlob, "null", 4);` |
|       ! 0 |  7007 | `		}` |
|        83 |  7008 | `		return;` |
|         - |  7009 | `	}` |
|    126763 |  7010 | `	if( nNonNull == 1 && bNullable ){` |
|         - |  7011 | `		/* Shorthand: ?T */` |
|       113 |  7012 | `		for( i = 0; i < nAtoms; i++ ){` |
|       113 |  7013 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       113 |  7014 | `			SyBlobAppend(pBlob, "?", 1);` |
|       113 |  7015 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|        23 |  7016 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        13 |  7017 | `			}else{` |
|        93 |  7018 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  7019 | `			}` |
|       113 |  7020 | `			return;` |
|       ! 0 |  7021 | `		}` |
|       ! 0 |  7022 | `	}` |
|         - |  7023 | `	{` |
|    126655 |  7024 | `		int bFirst = 1;` |
|         - |  7025 | `		/* 1) Classes in declaration order */` |
|    253413 |  7026 | `		for( i = 0; i < nAtoms; i++ ){` |
|    126763 |  7027 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|     38231 |  7028 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     38231 |  7029 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|     38231 |  7030 | `				bFirst = 0;` |
|     19113 |  7031 | `			}` |
|     63384 |  7032 | `		}` |
|         - |  7033 | `		/* 2) Built-ins in canonical order */` |
|         - |  7034 | `		{` |
|         - |  7035 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|         - |  7036 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|         - |  7037 | `			int k;` |
|    886555 |  7038 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|   1432011 |  7039 | `				for( i = 0; i < nAtoms; i++ ){` |
|    760441 |  7040 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|     88335 |  7041 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     88335 |  7042 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|     88335 |  7043 | `						bFirst = 0;` |
|     88335 |  7044 | `						break;` |
|         - |  7045 | `					}` |
|    336058 |  7046 | `				}` |
|    379955 |  7047 | `			}` |
|         - |  7048 | `		}` |
|         - |  7049 | `		/* 3) null suffix */` |
|    126655 |  7050 | `		if( bNullable ){` |
|        19 |  7051 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|        19 |  7052 | `			SyBlobAppend(pBlob, "null", 4);` |
|         8 |  7053 | `		}` |
|         - |  7054 | `	}` |
|     63396 |  7055 | `}` |
|         - |  7056 |  |
|         - |  7057 | `/*` |
|         - |  7058 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|         - |  7059 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|         - |  7060 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|         - |  7061 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|         - |  7062 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|         - |  7063 | ` * whether it was parenthesized.` |
|         - |  7064 | ` *` |
|         - |  7065 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|         - |  7066 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|         - |  7067 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|         - |  7068 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|         - |  7069 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|         - |  7070 | ` */` |
|    126934 |  7071 | `static sxi32 GenStateParsePart(` |
|         - |  7072 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|         - |  7073 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|         5 |  7074 | `{` |
|         - |  7075 | `	sxi32 rc;` |
|    126939 |  7076 | `	int nMembers = 0;` |
|    126939 |  7077 | `	int bParen = 0;` |
|    126939 |  7078 | `	*pnMembers = 0;` |
|    126939 |  7079 | `	*pbParen = 0;` |
|    126939 |  7080 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         9 |  7081 | `		bParen = 1;` |
|         9 |  7082 | `		pGen->pIn++; /* skip '(' */` |
|         3 |  7083 | `	}` |
|     63467 |  7084 | `	for(;;){` |
|    126965 |  7085 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|       ! 0 |  7086 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7087 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|       ! 0 |  7088 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7089 | `		}` |
|    126965 |  7090 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|    126965 |  7091 | `		if( rc != SXRET_OK ){` |
|         3 |  7092 | `			return rc;` |
|         - |  7093 | `		}` |
|    126963 |  7094 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|    126963 |  7095 | `		(*pnAtoms)++;` |
|    126963 |  7096 | `		nMembers++;` |
|         - |  7097 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|    126963 |  7098 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        39 |  7099 | `			SyToken *pNext = &pGen->pIn[1];` |
|        34 |  7100 | `			if( pNext < pGen->pEnd` |
|        39 |  7101 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        31 |  7102 | `				pGen->pIn++; /* skip '&' */` |
|        31 |  7103 | `				continue;` |
|         - |  7104 | `			}` |
|         4 |  7105 | `		}` |
|    126937 |  7106 | `		break;` |
|       ! 0 |  7107 | `	}` |
|    126937 |  7108 | `	if( bParen ){` |
|         9 |  7109 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  7110 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7111 | `				"Malformed DNF type: expecting ')'");` |
|       ! 0 |  7112 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7113 | `		}` |
|         9 |  7114 | `		pGen->pIn++; /* skip ')' */` |
|         9 |  7115 | `		if( nMembers < 2 ){` |
|       ! 0 |  7116 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7117 | `				"Parenthesized type must be an intersection of at least two types");` |
|       ! 0 |  7118 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7119 | `		}` |
|         3 |  7120 | `	}` |
|    126937 |  7121 | `	*pnMembers = nMembers;` |
|    126937 |  7122 | `	*pbParen = bParen;` |
|    126937 |  7123 | `	return SXRET_OK;` |
|     63472 |  7124 | `}` |
|         - |  7125 |  |
|         - |  7126 | `/*` |
|         - |  7127 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|         - |  7128 | ` *` |
|         - |  7129 | ` * Outputs:` |
|         - |  7130 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|         - |  7131 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|         - |  7132 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|         - |  7133 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|         - |  7134 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|         - |  7135 | ` *     already be initialized by the caller (allocator set, etc).` |
|         - |  7136 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|         - |  7137 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|         - |  7138 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|         - |  7139 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|         - |  7140 | ` *` |
|         - |  7141 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|         - |  7142 | ` * SXERR_ABORT on fatal compile errors.` |
|         - |  7143 | ` */` |
|    126798 |  7144 | `static sxi32 GenStateParseUnionTypeDecl(` |
|         - |  7145 | `	ph7_gen_state *pGen,` |
|         - |  7146 | `	sxu32 *pnType,` |
|         - |  7147 | `	SyString *pClass,` |
|         - |  7148 | `	SySet *pAlts,` |
|         - |  7149 | `	sxi32 *piTypeFlags,` |
|         - |  7150 | `	SyString *pTypeText,` |
|         - |  7151 | `	int iNullableFlag,` |
|         - |  7152 | `	int iUnionFlag,` |
|         - |  7153 | `	int bAllowVoid,` |
|         - |  7154 | `	sxu32 nLine` |
|         5 |  7155 | `){` |
|         - |  7156 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|    126803 |  7157 | `	int nAtoms = 0;` |
|    126803 |  7158 | `	int bShortNullable = 0;` |
|    126803 |  7159 | `	int bExplicitNull = 0;` |
|         - |  7160 | `	sxi32 rc;` |
|    126803 |  7161 | `	*pnType = 0;` |
|    126803 |  7162 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|    126803 |  7163 | `	*piTypeFlags = 0;` |
|    126803 |  7164 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|         - |  7165 |  |
|    126803 |  7166 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7167 | `		return SXRET_OK;` |
|         - |  7168 | `	}` |
|         - |  7169 | ``	/* Optional `?` shorthand prefix */`` |
|    126798 |  7170 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|       101 |  7171 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|       101 |  7172 | `		bShortNullable = 1;` |
|       101 |  7173 | `		pGen->pIn++;` |
|       101 |  7174 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7175 | `			return SXERR_SYNTAX;` |
|         - |  7176 | `		}` |
|        48 |  7177 | `	}` |
|         - |  7178 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|         - |  7179 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|         - |  7180 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|         - |  7181 | `	{` |
|         - |  7182 | `		int nMembers, bParen;` |
|    126803 |  7183 | `		sxu32 iGroup = 0;` |
|    126803 |  7184 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|    126803 |  7185 | `		if( rc != SXRET_OK ){` |
|         4 |  7186 | `			return rc;` |
|         - |  7187 | `		}` |
|         - |  7188 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|         - |  7189 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|         - |  7190 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|         - |  7191 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|         - |  7192 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|    190403 |  7193 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|    127010 |  7194 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       143 |  7195 | `			if( bShortNullable ){` |
|         - |  7196 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|         - |  7197 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|         - |  7198 | `				 * already reported" so callers skip their own error emission. */` |
|         3 |  7199 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - |  7200 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|         3 |  7201 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|         - |  7202 | `			}` |
|       141 |  7203 | `			if( nMembers >= 2 && !bParen ){` |
|       ! 0 |  7204 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|         - |  7205 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 |  7206 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7207 | `			}` |
|       141 |  7208 | ``			pGen->pIn++; /* skip `\|` */`` |
|       141 |  7209 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|       141 |  7210 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  7211 | `				return rc;` |
|         - |  7212 | `			}` |
|         5 |  7213 | `		}` |
|    126799 |  7214 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|       ! 0 |  7215 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7216 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 |  7217 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7218 | `		}` |
|         - |  7219 | `	}` |
|         - |  7220 | `	/* Validation pass.` |
|         - |  7221 | `	 *` |
|         - |  7222 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|         - |  7223 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|         - |  7224 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|         - |  7225 | `	 */` |
|         - |  7226 | `	{` |
|         - |  7227 | `		int i, j;` |
|    126799 |  7228 | `		int bHasNonNull = 0;` |
|    126799 |  7229 | `		int bAnyIntersection = 0;` |
|         - |  7230 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|         - |  7231 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|         - |  7232 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
|   4184207 |  7233 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    253755 |  7234 | `		for( i = 0; i < nAtoms; i++ ){` |
|    126961 |  7235 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|     63483 |  7236 | `		}` |
|    253699 |  7237 | `		for( i = 0; i < nAtoms; i++ ){` |
|    126931 |  7238 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|     63455 |  7239 | `		}` |
|         - |  7240 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|         - |  7241 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|    126799 |  7242 | `		if( bShortNullable && bAnyIntersection ){` |
|       ! 0 |  7243 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7244 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|       ! 0 |  7245 | `			return SXERR_SYNTAX;` |
|         - |  7246 | `		}` |
|    253741 |  7247 | `		for( i = 0; i < nAtoms; i++ ){` |
|         - |  7248 | `			/* Intersection members must be class/interface types (PHP rejects` |
|         - |  7249 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|         - |  7250 | ``			 * `true`/`false` in an intersection). */`` |
|    126959 |  7251 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        55 |  7252 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|        55 |  7253 | `				if( bClassLike ){` |
|        53 |  7254 | `					SyString *pC = &aAtoms[i].sClass;` |
|        48 |  7255 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|        48 |  7256 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|        48 |  7257 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|        53 |  7258 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|       ! 0 |  7259 | `						bClassLike = 0;` |
|       ! 0 |  7260 | `					}` |
|        24 |  7261 | `				}` |
|        55 |  7262 | `				if( !bClassLike ){` |
|         - |  7263 | `					const char *zName; sxu32 nName;` |
|         3 |  7264 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 |  7265 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|       ! 0 |  7266 | `					}else{` |
|         3 |  7267 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|         - |  7268 | `					}` |
|         4 |  7269 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7270 | `						"Type %.*s cannot be part of an intersection type",` |
|         1 |  7271 | `						(int)nName, zName);` |
|         3 |  7272 | `					return SXERR_SYNTAX;` |
|         - |  7273 | `				}` |
|        24 |  7274 | `			}` |
|    126957 |  7275 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|       177 |  7276 | `				if( nAtoms > 1 ){` |
|         3 |  7277 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7278 | `						"Void can only be used as a standalone type");` |
|         3 |  7279 | `					return SXERR_SYNTAX;` |
|         - |  7280 | `				}` |
|       175 |  7281 | `				if( !bAllowVoid ){` |
|       ! 0 |  7282 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7283 | `						"void cannot be used here");` |
|       ! 0 |  7284 | `					return SXERR_SYNTAX;` |
|         - |  7285 | `				}` |
|       175 |  7286 | `				if( bShortNullable ){` |
|       ! 0 |  7287 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7288 | `						"Void type cannot be nullable");` |
|       ! 0 |  7289 | `					return SXERR_SYNTAX;` |
|         - |  7290 | `				}` |
|        85 |  7291 | `			}` |
|    126955 |  7292 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|         - |  7293 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|         - |  7294 | `				 * type (never = the function does not return). Mirrors the void` |
|         - |  7295 | `				 * validation above; accepted here and enforced at compile time` |
|         - |  7296 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|        26 |  7297 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|         - |  7298 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|         - |  7299 | `					 * same as any other non-standalone use. */` |
|         5 |  7300 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7301 | `						"never can only be used as a standalone type");` |
|         5 |  7302 | `					return SXERR_SYNTAX;` |
|         - |  7303 | `				}` |
|        21 |  7304 | `				if( !bAllowVoid ){` |
|         - |  7305 | `					/* Return-only: params call with bAllowVoid=0. */` |
|         3 |  7306 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7307 | `						"never cannot be used as a parameter type");` |
|         3 |  7308 | `					return SXERR_SYNTAX;` |
|         - |  7309 | `				}` |
|         8 |  7310 | `			}` |
|    126949 |  7311 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|        34 |  7312 | `				bExplicitNull = 1;` |
|        19 |  7313 | `			}else{` |
|    126919 |  7314 | `				bHasNonNull = 1;` |
|         - |  7315 | `			}` |
|         - |  7316 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|         - |  7317 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|         - |  7318 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|         - |  7319 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|         - |  7320 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|    127149 |  7321 | `			for( j = 0; j < i; j++ ){` |
|       207 |  7322 | `				int bDup = 0;` |
|       207 |  7323 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|       395 |  7324 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|       202 |  7325 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|       207 |  7326 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|       195 |  7327 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|        51 |  7328 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|        44 |  7329 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|        44 |  7330 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|        17 |  7331 | `								aAtoms[j].sClass.zString,` |
|        34 |  7332 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|       ! 0 |  7333 | `							bDup = 1;` |
|       ! 0 |  7334 | `						}` |
|        27 |  7335 | `					}else{` |
|         3 |  7336 | `						bDup = 1;` |
|         - |  7337 | `					}` |
|        23 |  7338 | `				}` |
|       195 |  7339 | `				if( bDup ){` |
|         - |  7340 | `					const char *zName;` |
|         - |  7341 | `					sxu32 nName;` |
|         3 |  7342 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 |  7343 | `						zName = aAtoms[i].sClass.zString;` |
|       ! 0 |  7344 | `						nName = aAtoms[i].sClass.nByte;` |
|       ! 0 |  7345 | `					}else{` |
|         3 |  7346 | `						zName = aAtoms[i].zCanon;` |
|         3 |  7347 | `						nName = aAtoms[i].nCanon;` |
|         - |  7348 | `					}` |
|         4 |  7349 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         1 |  7350 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|         3 |  7351 | `					return SXERR_SYNTAX;` |
|         - |  7352 | `				}` |
|        99 |  7353 | `			}` |
|     63476 |  7354 | `		}` |
|    126787 |  7355 | `		if( !bHasNonNull && bExplicitNull ){` |
|         7 |  7356 | `			if( bShortNullable ){` |
|         - |  7357 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|       ! 0 |  7358 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7359 | `					"Null can not be used as a standalone type");` |
|       ! 0 |  7360 | `				return SXERR_SYNTAX;` |
|         - |  7361 | `			}` |
|         - |  7362 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|         - |  7363 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|         - |  7364 | `			 * path below leaves *pnType untouched when there is no non-null` |
|         - |  7365 | `			 * atom, so set it here. */` |
|         7 |  7366 | `			*pnType = MEMOBJ_NULL;` |
|         3 |  7367 | `		}` |
|         - |  7368 | `	}` |
|         - |  7369 | `	/* Compute nullability flag */` |
|    126787 |  7370 | `	if( bShortNullable \|\| bExplicitNull ){` |
|       129 |  7371 | `		*piTypeFlags \|= iNullableFlag;` |
|        62 |  7372 | `	}` |
|         - |  7373 | `	/* Build canonical type text */` |
|    126787 |  7374 | `	if( pTypeText ){` |
|         - |  7375 | `		SyBlob sBlob;` |
|    126787 |  7376 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|    190131 |  7377 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|     63391 |  7378 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|    126787 |  7379 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|    189899 |  7380 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    126596 |  7381 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|    126601 |  7382 | `			if( zDup ){` |
|    126601 |  7383 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|     63298 |  7384 | `			}` |
|     63298 |  7385 | `		}` |
|    126787 |  7386 | `		SyBlobRelease(&sBlob);` |
|     63391 |  7387 | `	}` |
|         - |  7388 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|         - |  7389 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|         - |  7390 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|         - |  7391 | `	{` |
|    126787 |  7392 | `		int nNonNull = 0;` |
|    126787 |  7393 | `		int iNonNullIdx = -1;` |
|         - |  7394 | `		int i;` |
|    253721 |  7395 | `		for( i = 0; i < nAtoms; i++ ){` |
|    126939 |  7396 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    126909 |  7397 | `				nNonNull++;` |
|    126909 |  7398 | `				iNonNullIdx = i;` |
|     63452 |  7399 | `			}` |
|     63472 |  7400 | `		}` |
|    126787 |  7401 | `		if( nNonNull <= 1 ){` |
|         - |  7402 | `			/* Fast path: store as single type. */` |
|    126681 |  7403 | `			if( iNonNullIdx >= 0 ){` |
|    126675 |  7404 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|    126675 |  7405 | `				if( pA->nType == SXU32_HIGH ){` |
|     57308 |  7406 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     19101 |  7407 | `						pA->sClass.zString, pA->sClass.nByte);` |
|     38207 |  7408 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|     38207 |  7409 | `					*pnType = SXU32_HIGH;` |
|     38207 |  7410 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|    107574 |  7411 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|       175 |  7412 | `					*pnType = MEMOBJ_VOID;` |
|     88388 |  7413 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|        18 |  7414 | `					*pnType = MEMOBJ_NEVER;` |
|        10 |  7415 | `				}else{` |
|     88287 |  7416 | `					*pnType = pA->nType;` |
|         - |  7417 | `				}` |
|     63335 |  7418 | `			}` |
|     63343 |  7419 | `		}else{` |
|         - |  7420 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|       111 |  7421 | `			*piTypeFlags \|= iUnionFlag;` |
|       355 |  7422 | `			for( i = 0; i < nAtoms; i++ ){` |
|         - |  7423 | `				ph7_type_alt sAlt;` |
|       249 |  7424 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       239 |  7425 | `				SyZero(&sAlt, sizeof(sAlt));` |
|       239 |  7426 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|       239 |  7427 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       146 |  7428 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        47 |  7429 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        99 |  7430 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|        99 |  7431 | `					sAlt.nType = SXU32_HIGH;` |
|        99 |  7432 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|        52 |  7433 | `				}else{` |
|       145 |  7434 | `					sAlt.nType = aAtoms[i].nType;` |
|       145 |  7435 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|         - |  7436 | `				}` |
|       239 |  7437 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|       122 |  7438 | `			}` |
|         - |  7439 | `		}` |
|         - |  7440 | `	}` |
|    126787 |  7441 | `	return SXRET_OK;` |
|     63404 |  7442 | `}` |
|         - |  7443 |  |
|         - |  7444 | `/*` |
|         - |  7445 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|         - |  7446 | `` * pGen->pIn should point to the token after `)`.`` |
|         - |  7447 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|         - |  7448 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|         - |  7449 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|         - |  7450 | `` *          and union types `: T\|U`.`` |
|         - |  7451 | ` */` |
|   2668628 |  7452 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|         5 |  7453 | `{` |
|   2668633 |  7454 | `	sxi32 iFlags = 0;` |
|         - |  7455 | `	sxi32 rc;` |
|         - |  7456 | `	sxu32 nLine;` |
|   2668633 |  7457 | `	pFunc->nReturnType = 0;` |
|   2668633 |  7458 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|   2668633 |  7459 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|         - |  7460 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|         - |  7461 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|         - |  7462 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|         - |  7463 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|         - |  7464 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|   2668633 |  7465 | `	SySetReset(&pFunc->aReturnUnion);` |
|   2668633 |  7466 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|   2668633 |  7467 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|   2656587 |  7468 | `		return SXRET_OK;` |
|         - |  7469 | `	}` |
|     12051 |  7470 | `	pGen->pIn++; /* Skip ':' */` |
|     12051 |  7471 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7472 | `		return SXRET_OK;` |
|         - |  7473 | `	}` |
|     12051 |  7474 | `	nLine = pGen->pIn->nLine;` |
|     12051 |  7475 | `	rc = GenStateParseUnionTypeDecl(` |
|      6023 |  7476 | `		pGen,` |
|      6023 |  7477 | `		&pFunc->nReturnType,` |
|      6023 |  7478 | `		&pFunc->sReturnClass,` |
|      6023 |  7479 | `		&pFunc->aReturnUnion,` |
|         - |  7480 | `		&iFlags,` |
|      6023 |  7481 | `		&pFunc->sReturnTypeName,` |
|         - |  7482 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|         - |  7483 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|         - |  7484 | `		/* iUnionFlag */ 0,` |
|         - |  7485 | `		/* bAllowVoid */ 1,` |
|      6023 |  7486 | `		nLine);` |
|     12051 |  7487 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  7488 | `		return SXERR_ABORT;` |
|         - |  7489 | `	}` |
|     12051 |  7490 | `	if( rc == SXERR_CORRUPT ){` |
|         - |  7491 | `		/* Error already reported */` |
|       ! 0 |  7492 | `		return SXERR_SYNTAX;` |
|         - |  7493 | `	}` |
|     12051 |  7494 | `	if( rc == SXERR_SYNTAX ){` |
|         8 |  7495 | `		if( pGen->pIn < pGen->pEnd ){` |
|        11 |  7496 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - |  7497 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|         6 |  7498 | `				&pGen->pIn->sData);` |
|         5 |  7499 | `		}else{` |
|       ! 0 |  7500 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|         - |  7501 | `				"syntax error, unexpected end of file in return type declaration");` |
|         - |  7502 | `		}` |
|         8 |  7503 | `		return SXERR_SYNTAX;` |
|         - |  7504 | `	}` |
|     12045 |  7505 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     12045 |  7506 | `	return SXRET_OK;` |
|   1334319 |  7507 | `}` |
|         - |  7508 |  |
|    298034 |  7509 | `static sxi32 GenStateCompileFunc(` |
|         - |  7510 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  7511 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|         - |  7512 | `	sxi32 iFlags,        /* Control flags */` |
|         - |  7513 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|         - |  7514 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|         - |  7515 | `	)` |
|         5 |  7516 | `{` |
|         - |  7517 | `	ph7_vm_func *pFunc;` |
|         - |  7518 | `	SyToken *pEnd;` |
|         - |  7519 | `	sxu32 nLine;` |
|         - |  7520 | `	char *zName;` |
|         - |  7521 | `	sxi32 rc;` |
|         - |  7522 | `	/* Extract line number */` |
|    298039 |  7523 | `	nLine = pGen->pIn->nLine;` |
|         - |  7524 | `	/* Jump the left parenthesis '(' */` |
|    298039 |  7525 | `	pGen->pIn++;` |
|         - |  7526 | `	/* Delimit the function signature */` |
|    298039 |  7527 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    298039 |  7528 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  7529 | `		/* Syntax error */` |
|         9 |  7530 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable");` |
|         3 |  7531 | `		(void)pName;` |
|         9 |  7532 | `		if( rc == SXERR_ABORT ){` |
|         - |  7533 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  7534 | `			return SXERR_ABORT;` |
|         - |  7535 | `		}` |
|         9 |  7536 | `		pGen->pIn = pGen->pEnd;` |
|         9 |  7537 | `		return SXRET_OK;` |
|         - |  7538 | `	}` |
|         - |  7539 | `	/* Create the function state */` |
|    298033 |  7540 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|    298033 |  7541 | `	if( pFunc == 0 ){` |
|       ! 0 |  7542 | `		goto OutOfMem;` |
|         - |  7543 | `	}` |
|         - |  7544 | `	/* Build the function name, prepending namespace if active */` |
|    298040 |  7545 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|         - |  7546 | `		SyBlob sFQN;` |
|         - |  7547 | `		sxu32 nLen;` |
|        16 |  7548 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        16 |  7549 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        16 |  7550 | `		SyBlobAppend(&sFQN,"\\",1);` |
|        16 |  7551 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|        16 |  7552 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|        16 |  7553 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|        16 |  7554 | `		SyBlobRelease(&sFQN);` |
|        16 |  7555 | `		if( zName == 0 ){` |
|       ! 0 |  7556 | `			goto OutOfMem;` |
|         - |  7557 | `		}` |
|        16 |  7558 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|         9 |  7559 | `	}else{` |
|    298019 |  7560 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    298019 |  7561 | `		if( zName == 0 ){` |
|       ! 0 |  7562 | `			goto OutOfMem;` |
|         - |  7563 | `		}` |
|    298019 |  7564 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|         - |  7565 | `	}` |
|         - |  7566 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|         - |  7567 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|    298033 |  7568 | `	pFunc->nLine = nLine;` |
|    298033 |  7569 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|    298033 |  7570 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  7571 | `		return SXERR_ABORT;` |
|         - |  7572 | `	}` |
|    298033 |  7573 | `	if( pGen->pIn < pEnd ){` |
|         - |  7574 | `		/* Collect function arguments */` |
|    240255 |  7575 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|    240255 |  7576 | `		if( rc == SXERR_ABORT ){` |
|         - |  7577 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  7578 | `			return SXERR_ABORT;` |
|         - |  7579 | `		}` |
|    120125 |  7580 | `	}` |
|         - |  7581 | `	/* Point past ')' and parse optional return type ': type' */` |
|    298033 |  7582 | `	pGen->pIn = &pEnd[1];` |
|         - |  7583 | `	{` |
|    298033 |  7584 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|    298033 |  7585 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 |  7586 | `			return SXERR_ABORT;` |
|    298033 |  7587 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|         8 |  7588 | `			return SXERR_SYNTAX;` |
|         - |  7589 | `		}` |
|         - |  7590 | `	}` |
|    298027 |  7591 | `	if( bHandleClosure ){` |
|         - |  7592 | `		ph7_vm_func_closure_env sEnv;` |
|       469 |  7593 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|       464 |  7594 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       280 |  7595 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|        91 |  7596 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  7597 | `				/* Closure,record environment variable */` |
|        91 |  7598 | `				pGen->pIn++;` |
|        91 |  7599 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  7600 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|       ! 0 |  7601 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  7602 | `						return SXERR_ABORT;` |
|         - |  7603 | `					}` |
|       ! 0 |  7604 | `				}` |
|        91 |  7605 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|         - |  7606 | `				/* Compile until we hit the first closing parenthesis */` |
|       187 |  7607 | `				while( pGen->pIn < pGen->pEnd ){` |
|       187 |  7608 | `					int iFlagsLocal = 0;` |
|       187 |  7609 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|        91 |  7610 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|        91 |  7611 | `						break;` |
|         - |  7612 | `					}` |
|       101 |  7613 | `					nLineLocal = pGen->pIn->nLine;` |
|       101 |  7614 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - |  7615 | `						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry` |
|         - |  7616 | `						 * to the variable's memory slot instead of copying its value. */` |
|        55 |  7617 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|        55 |  7618 | `						pGen->pIn++;` |
|        27 |  7619 | `					}` |
|        96 |  7620 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|       101 |  7621 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  7622 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|         - |  7623 | `								"Closure: Unexpected token. Expecting a variable name");` |
|       ! 0 |  7624 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 |  7625 | `								return SXERR_ABORT;` |
|         - |  7626 | `							}` |
|         - |  7627 | `							/* Find the closing parenthesis */` |
|       ! 0 |  7628 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  7629 | `								pGen->pIn++;` |
|       ! 0 |  7630 | `							}` |
|       ! 0 |  7631 | `							if(pGen->pIn < pGen->pEnd){` |
|       ! 0 |  7632 | `								pGen->pIn++;` |
|       ! 0 |  7633 | `							}` |
|       ! 0 |  7634 | `							break;` |
|         - |  7635 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|       ! 0 |  7636 | `					}else{` |
|         - |  7637 | `						SyString *pNameLocal;` |
|         - |  7638 | `						char *zDup;` |
|         - |  7639 | `						/* Duplicate variable name */` |
|       101 |  7640 | `						pNameLocal = &pGen->pIn[1].sData;` |
|       101 |  7641 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|       101 |  7642 | `						if( zDup ){` |
|         - |  7643 | `							/* Zero the structure */` |
|       101 |  7644 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       101 |  7645 | `							sEnv.iFlags = iFlagsLocal;` |
|       101 |  7646 | `							sEnv.nIdx = SXU32_HIGH;` |
|       101 |  7647 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       101 |  7648 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|       116 |  7649 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|        30 |  7650 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|       ! 0 |  7651 | `									got_this = 1;` |
|       ! 0 |  7652 | `							}` |
|         - |  7653 | `							/* Save imported variable */` |
|       101 |  7654 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|        53 |  7655 | `						}else{` |
|       ! 0 |  7656 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  7657 | `							 return SXERR_ABORT;` |
|         - |  7658 | `						}` |
|         - |  7659 | `					}` |
|       101 |  7660 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|       113 |  7661 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  7662 | `						/* Ignore trailing commas */` |
|        13 |  7663 | `						pGen->pIn++;` |
|         1 |  7664 | `					}` |
|         5 |  7665 | `				}` |
|         - |  7666 | `				/* php 7.1+: the return type follows the use clause —` |
|         - |  7667 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|         - |  7668 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|         - |  7669 | `				 * so an unconditional call would wipe a type parsed at the` |
|         - |  7670 | `				 * legacy pre-use position. */` |
|        91 |  7671 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|         7 |  7672 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|         7 |  7673 | `					if( rcRt2 == SXERR_ABORT ){` |
|       ! 0 |  7674 | `						return SXERR_ABORT;` |
|         7 |  7675 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|       ! 0 |  7676 | `						return SXERR_SYNTAX;` |
|         - |  7677 | `					}` |
|         3 |  7678 | `				}` |
|        43 |  7679 | `		}` |
|       469 |  7680 | `		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){` |
|         - |  7681 | `			/* Make the $this variable [Current processed Object (class instance)]` |
|         - |  7682 | `			 * available to the closure environment — for EVERY non-static` |
|         - |  7683 | `			 * anonymous function, use list or not (php binds $this to any` |
|         - |  7684 | ``			 * closure declared in a method; pre-fix only `use (...)` closures`` |
|         - |  7685 | `			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of` |
|         - |  7686 | `			 * a global-scope closure is silently dropped at install. A static` |
|         - |  7687 | `			 * closure never binds $this (php). */` |
|       461 |  7688 | `			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       461 |  7689 | `			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|       461 |  7690 | `			sEnv.nIdx = SXU32_HIGH;` |
|       461 |  7691 | `			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       461 |  7692 | `			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|       461 |  7693 | `			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       228 |  7694 | `		}` |
|       469 |  7695 | `		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|         - |  7696 | `			/* Mark as closure */` |
|       463 |  7697 | `			pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       229 |  7698 | `		}` |
|       232 |  7699 | `	}` |
|         - |  7700 | `	/* Compile the body */` |
|    298027 |  7701 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|    298027 |  7702 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  7703 | `		return SXERR_ABORT;` |
|         - |  7704 | `	}` |
|         - |  7705 | `	/* The cursor sits just past the body's closing brace */` |
|    298027 |  7706 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|    298027 |  7707 | `	if( ppFunc ){` |
|    298027 |  7708 | `		*ppFunc = pFunc;` |
|    149011 |  7709 | `	}` |
|    298027 |  7710 | `	rc = SXRET_OK;` |
|    298027 |  7711 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|         - |  7712 | `		/* Finally register the function */` |
|    297569 |  7713 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|    148782 |  7714 | `	}` |
|    298027 |  7715 | `	if( rc == SXRET_OK ){` |
|    298027 |  7716 | `		return SXRET_OK;` |
|         - |  7717 | `	}` |
|         - |  7718 | `	/* Fall through if something goes wrong */` |
|       ! 0 |  7719 | `OutOfMem:` |
|         - |  7720 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  7721 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  7722 | `	 */` |
|       ! 0 |  7723 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 |  7724 | `	return SXERR_ABORT;` |
|    149022 |  7725 | `}` |
|         - |  7726 | `/*` |
|         - |  7727 | ` * Compile a standard PHP function.` |
|         - |  7728 | ` *  Refer to the block-comment above for more information.` |
|         - |  7729 | ` */` |
|    297578 |  7730 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|         5 |  7731 | `{` |
|         - |  7732 | `	SyString *pName;` |
|         - |  7733 | `	sxi32 iFlags;` |
|         - |  7734 | `	sxu32 nKwLine;` |
|         - |  7735 | `	sxu32 nLine;` |
|         - |  7736 | `	sxi32 rc;` |
|         - |  7737 |  |
|    297583 |  7738 | `	nLine = pGen->pIn->nLine;` |
|    297583 |  7739 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|    297583 |  7740 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|    297583 |  7741 | `	iFlags = 0;` |
|    297583 |  7742 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - |  7743 | `		/* Return by reference,remember that */` |
|        12 |  7744 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|         - |  7745 | `		/* Jump the '&' token */` |
|        12 |  7746 | `		pGen->pIn++;` |
|         5 |  7747 | `	}` |
|    297583 |  7748 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  7749 | `		/* Invalid function name */` |
|         8 |  7750 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         8 |  7751 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  7752 | `			return SXERR_ABORT;` |
|         - |  7753 | `		}` |
|         - |  7754 | `		/* Sychronize with the next semi-colon or braces*/` |
|        22 |  7755 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        16 |  7756 | `			pGen->pIn++;` |
|         2 |  7757 | `		}` |
|         8 |  7758 | `		return SXRET_OK;` |
|         - |  7759 | `	}` |
|    297577 |  7760 | `	pName = &pGen->pIn->sData;` |
|    297577 |  7761 | `	nLine = pGen->pIn->nLine;` |
|         - |  7762 | `	/* Jump the function name */` |
|    297577 |  7763 | `	pGen->pIn++;` |
|    297577 |  7764 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  7765 | `		/* Syntax error */` |
|         3 |  7766 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         3 |  7767 | `		if( rc == SXERR_ABORT ){` |
|         - |  7768 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  7769 | `			return SXERR_ABORT;` |
|         - |  7770 | `		}` |
|         - |  7771 | `		/* Sychronize with the next semi-colon or '{' */` |
|         3 |  7772 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  7773 | `			pGen->pIn++;` |
|       ! 0 |  7774 | `		}` |
|         3 |  7775 | `		return SXRET_OK;` |
|         - |  7776 | `	}` |
|         - |  7777 | `	/* Compile function body */` |
|         - |  7778 | `	{` |
|    297575 |  7779 | `		ph7_vm_func *pFuncState = 0;` |
|    297575 |  7780 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);` |
|    297575 |  7781 | `		if( pFuncState ){` |
|         - |  7782 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|    297563 |  7783 | `			pFuncState->nLine = nKwLine;` |
|    148779 |  7784 | `		}` |
|         - |  7785 | `	}` |
|    297575 |  7786 | `	return rc;` |
|    148794 |  7787 | `}` |
|         - |  7788 | `/*` |
|         - |  7789 | ` * Extract the visibility level associated with a given keyword.` |
|         - |  7790 | ` * According to the PHP language reference manual` |
|         - |  7791 | ` *  Visibility:` |
|         - |  7792 | ` *  The visibility of a property or method can be defined by prefixing` |
|         - |  7793 | ` *  the declaration with the keywords public, protected or private.` |
|         - |  7794 | ` *  Class members declared public can be accessed everywhere.` |
|         - |  7795 | ` *  Members declared protected can be accessed only within the class` |
|         - |  7796 | ` *  itself and by inherited and parent classes. Members declared as private` |
|         - |  7797 | ` *  may only be accessed by the class that defines the member.` |
|         - |  7798 | ` */` |
|   3111690 |  7799 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|         5 |  7800 | `{` |
|   3111695 |  7801 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    250729 |  7802 | `		return PH7_CLASS_PROT_PRIVATE;` |
|   2860971 |  7803 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|    189887 |  7804 | `		return PH7_CLASS_PROT_PROTECTED;` |
|         - |  7805 | `	}` |
|         - |  7806 | `	/* Assume public by default */` |
|   2671089 |  7807 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   1555850 |  7808 | `}` |
|         - |  7809 | `/*` |
|         - |  7810 | ` * Compile a class constant.` |
|         - |  7811 | ` * According to the PHP language reference manual` |
|         - |  7812 | ` *  Class Constants` |
|         - |  7813 | ` *   It is possible to define constant values on a per-class basis remaining` |
|         - |  7814 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|         - |  7815 | ` *   you don't use the $ symbol to declare or use them.` |
|         - |  7816 | ` *   The value must be a constant expression, not (for example) a variable,` |
|         - |  7817 | ` *   a property, a result of a mathematical operation, or a function call.` |
|         - |  7818 | ` *   It's also possible for interfaces to have constants.` |
|         - |  7819 | ` * Symisc eXtension.` |
|         - |  7820 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|         - |  7821 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  7822 | ` *  Example:` |
|         - |  7823 | ` *   class Test{` |
|         - |  7824 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|         - |  7825 | ` *   };` |
|         - |  7826 | ` *   var_dump(TEST::MyConst);` |
|         - |  7827 | ` *   Refer to the official documentation for more information on the powerful extension` |
|         - |  7828 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|         - |  7829 | ` */` |
|         - |  7830 | `/*` |
|         - |  7831 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|         - |  7832 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|         - |  7833 | ` * token immediately followed by '='. Anything else with a leading type token` |
|         - |  7834 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|         - |  7835 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|         - |  7836 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|         - |  7837 | ` */` |
|    288672 |  7838 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|         5 |  7839 | `{` |
|         - |  7840 | `	SyToken *p0, *p1;` |
|    288677 |  7841 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7842 | `		return 0;` |
|         - |  7843 | `	}` |
|    288677 |  7844 | `	p0 = pGen->pIn;` |
|         - |  7845 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|    288677 |  7846 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|       ! 0 |  7847 | `		return 1;` |
|         - |  7848 | `	}` |
|    288677 |  7849 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|         5 |  7850 | `		return 1;` |
|         - |  7851 | `	}` |
|         - |  7852 | `	/* A name-like first token begins a type only when followed by another` |
|         - |  7853 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|         - |  7854 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|    288673 |  7855 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|    288673 |  7856 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|    288673 |  7857 | `		if( p1 ){` |
|    288673 |  7858 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|        34 |  7859 | `				return 1;` |
|         - |  7860 | `			}` |
|    288643 |  7861 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|         5 |  7862 | `				return 1;` |
|         - |  7863 | `			}` |
|    144317 |  7864 | `		}` |
|    144317 |  7865 | `	}` |
|    288639 |  7866 | `	return 0;` |
|    144341 |  7867 | `}` |
|         - |  7868 | `/*` |
|         - |  7869 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|         - |  7870 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|         - |  7871 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|         - |  7872 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|         - |  7873 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|         - |  7874 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|         - |  7875 | ` * Peek only; never consumes tokens.` |
|         - |  7876 | ` */` |
|        24 |  7877 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|         4 |  7878 | `{` |
|        28 |  7879 | `	SyToken *p = pGen->pIn;` |
|        39 |  7880 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        20 |  7881 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|         3 |  7882 | `		p++; /* skip leading unary sign(s) */` |
|         1 |  7883 | `	}` |
|        28 |  7884 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|        23 |  7885 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|         - |  7886 | `	}` |
|         6 |  7887 | `	p++;` |
|         - |  7888 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|         6 |  7889 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|        16 |  7890 | `}` |
|         - |  7891 | `/*` |
|         - |  7892 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|         - |  7893 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|         - |  7894 | `` * `$o->new`), not a `new` expression.`` |
|         - |  7895 | ` */` |
|       110 |  7896 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|         4 |  7897 | `{` |
|         - |  7898 | `	sxi32 iOp;` |
|       114 |  7899 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|        11 |  7900 | `		return 0;` |
|         - |  7901 | `	}` |
|       104 |  7902 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|       104 |  7903 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|        59 |  7904 | `}` |
|         - |  7905 | `/*` |
|         - |  7906 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|         - |  7907 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|         - |  7908 | ` * interface-constant and (instance/static) property-default initializers` |
|         - |  7909 | ` * ("New expressions are not supported in this context") while still allowing it` |
|         - |  7910 | ` * in global constants, parameter defaults and static-local initializers (which` |
|         - |  7911 | ` * are compiled by different functions and left untouched). The scan is` |
|         - |  7912 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|         - |  7913 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|         - |  7914 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|         - |  7915 | ` *` |
|         - |  7916 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|         - |  7917 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|         - |  7918 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|         - |  7919 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|         - |  7920 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|         - |  7921 | ` */` |
|    619606 |  7922 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|         5 |  7923 | `{` |
|    619611 |  7924 | `	SyToken *p = pGen->pIn;` |
|    619611 |  7925 | `	int iDepth = 0;` |
|   1623707 |  7926 | `	while( p < pGen->pEnd ){` |
|   1623707 |  7927 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    619559 |  7928 | `			break; /* end of this initializer */` |
|         - |  7929 | `		}` |
|   1004148 |  7930 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|    505891 |  7931 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      7624 |  7932 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|         - |  7933 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|         - |  7934 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|         - |  7935 | `			 * expression. */` |
|         3 |  7936 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|         3 |  7937 | `			p++;` |
|         3 |  7938 | `			if( bArrow ){` |
|         - |  7939 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|         - |  7940 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|         3 |  7941 | `				int iBase = iDepth;` |
|        17 |  7942 | `				while( p < pGen->pEnd ){` |
|        17 |  7943 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         5 |  7944 | `						iDepth++;` |
|        15 |  7945 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         5 |  7946 | `						if( iDepth <= iBase ){` |
|       ! 0 |  7947 | `							break; /* closes an enclosing group, not the fn's own */` |
|         - |  7948 | `						}` |
|         5 |  7949 | `						iDepth--;` |
|        11 |  7950 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|         3 |  7951 | `						break;` |
|         - |  7952 | `					}` |
|        15 |  7953 | `					p++;` |
|         1 |  7954 | `				}` |
|         2 |  7955 | `			}else{` |
|         - |  7956 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|         - |  7957 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|         - |  7958 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|         - |  7959 | `				 * then skip the balanced brace block. */` |
|       ! 0 |  7960 | `				int iLocal = 0;` |
|       ! 0 |  7961 | `				while( p < pGen->pEnd ){` |
|       ! 0 |  7962 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|       ! 0 |  7963 | `						break; /* body brace */` |
|         - |  7964 | `					}` |
|       ! 0 |  7965 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       ! 0 |  7966 | `						iLocal++;` |
|       ! 0 |  7967 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       ! 0 |  7968 | `						if( iLocal > 0 ){` |
|       ! 0 |  7969 | `							iLocal--;` |
|       ! 0 |  7970 | `						}` |
|       ! 0 |  7971 | `					}` |
|       ! 0 |  7972 | `					p++;` |
|       ! 0 |  7973 | `				}` |
|       ! 0 |  7974 | `				if( p < pGen->pEnd ){` |
|       ! 0 |  7975 | `					int iBrace = 0; /* p is on the body '{' */` |
|       ! 0 |  7976 | `					while( p < pGen->pEnd ){` |
|       ! 0 |  7977 | `						if( p->nType & PH7_TK_OCB ){` |
|       ! 0 |  7978 | `							iBrace++;` |
|       ! 0 |  7979 | `						}else if( p->nType & PH7_TK_CCB ){` |
|       ! 0 |  7980 | `							iBrace--;` |
|       ! 0 |  7981 | `							if( iBrace == 0 ){` |
|       ! 0 |  7982 | `								p++;` |
|       ! 0 |  7983 | `								break;` |
|         - |  7984 | `							}` |
|       ! 0 |  7985 | `						}` |
|       ! 0 |  7986 | `						p++;` |
|       ! 0 |  7987 | `					}` |
|       ! 0 |  7988 | `				}` |
|         - |  7989 | `			}` |
|         3 |  7990 | `			continue;` |
|         - |  7991 | `		}` |
|   1004151 |  7992 | `		if( p->nType & PH7_TK_OCB ){` |
|        45 |  7993 | `			if( iDepth == 0 ){` |
|         - |  7994 | `				/* A depth-0 '{' can only open a PHP 8.4 property-hook list` |
|         - |  7995 | ``				 * (`public T $x = default { get …; }`): the default expression`` |
|         - |  7996 | ``				 * ends here. A `new` inside a hook BODY runs at access time and`` |
|         - |  7997 | `				 * is legal — don't scan into it. */` |
|        45 |  7998 | `				break;` |
|         - |  7999 | `			}` |
|       ! 0 |  8000 | `			iDepth++;` |
|   1004107 |  8001 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     41845 |  8002 | `			iDepth++;` |
|    983187 |  8003 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     41843 |  8004 | `			if( iDepth > 0 ){` |
|     41843 |  8005 | `				iDepth--;` |
|     20919 |  8006 | `			}` |
|    941348 |  8007 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|    334837 |  8008 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|         - |  8009 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|         - |  8010 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|         - |  8011 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|        11 |  8012 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|        11 |  8013 | `				return 1;` |
|         - |  8014 | `			}` |
|       ! 0 |  8015 | `		}` |
|   1004099 |  8016 | `		p++;` |
|         5 |  8017 | `	}` |
|    619603 |  8018 | `	return 0;` |
|    309808 |  8019 | `}` |
|         - |  8020 | `/*` |
|         - |  8021 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|         - |  8022 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|         - |  8023 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|         - |  8024 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|         - |  8025 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|         - |  8026 | ` * share the same backing.` |
|         - |  8027 | ` */` |
|       350 |  8028 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|         - |  8029 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|         5 |  8030 | `{` |
|       355 |  8031 | `	pAttr->nType = nType;` |
|       355 |  8032 | `	pAttr->sClass = *pClass;` |
|       355 |  8033 | `	pAttr->sTypeName = *pTypeName;` |
|       355 |  8034 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  8035 | `		sxu32 i;` |
|        73 |  8036 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        51 |  8037 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|        51 |  8038 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|        28 |  8039 | `		}` |
|        11 |  8040 | `	}` |
|       355 |  8041 | `}` |
|    288672 |  8042 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  8043 | `{` |
|    288677 |  8044 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  8045 | `	SySet *pInstrContainer;` |
|         - |  8046 | `	ph7_class_attr *pCons;` |
|         - |  8047 | `	SyString *pName;` |
|         - |  8048 | `	sxi32 rc;` |
|    288677 |  8049 | `	sxu32 nType = 0;` |
|         - |  8050 | `	SyString sTypeClass;` |
|         - |  8051 | `	SyString sTypeText;` |
|         - |  8052 | `	SySet aUnionAlts;` |
|    288677 |  8053 | `	sxi32 iTypeFlags = 0;` |
|    288677 |  8054 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    288677 |  8055 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    288677 |  8056 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  8057 | `	/* Extract visibility level */` |
|    288677 |  8058 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  8059 | `	/* Mark as constant */` |
|    288677 |  8060 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|    288677 |  8061 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|         - |  8062 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|         - |  8063 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|    288696 |  8064 | `	if( GenStateClassConstHasType(pGen) ){` |
|        61 |  8065 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|        38 |  8066 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|         - |  8067 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|         - |  8068 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|         - |  8069 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|         - |  8070 | `		 * and success paths release. */` |
|        42 |  8071 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  8072 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  8073 | `			goto Synchronize;` |
|        42 |  8074 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  8075 | `			return SXERR_ABORT;` |
|        42 |  8076 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  8077 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  8078 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|       ! 0 |  8079 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8080 | `				return SXERR_ABORT;` |
|         - |  8081 | `			}` |
|       ! 0 |  8082 | `			goto Synchronize;` |
|         - |  8083 | `		}` |
|        42 |  8084 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        19 |  8085 | `	}` |
|    144336 |  8086 | `loop:` |
|    288679 |  8087 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - |  8088 | `		/* Invalid constant name */` |
|       ! 0 |  8089 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|       ! 0 |  8090 | `		if( rc == SXERR_ABORT ){` |
|         - |  8091 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8092 | `			return SXERR_ABORT;` |
|         - |  8093 | `		}` |
|       ! 0 |  8094 | `		goto Synchronize;` |
|         - |  8095 | `	}` |
|         - |  8096 | `	/* Peek constant name */` |
|    288679 |  8097 | `	pName = &pGen->pIn->sData;` |
|         - |  8098 | `	/* Make sure the constant name isn't reserved */` |
|    288679 |  8099 | `	if( GenStateIsReservedConstant(pName) ){` |
|         - |  8100 | `		/* Reserved constant name */` |
|       ! 0 |  8101 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|       ! 0 |  8102 | `		if( rc == SXERR_ABORT ){` |
|         - |  8103 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8104 | `			return SXERR_ABORT;` |
|         - |  8105 | `		}` |
|       ! 0 |  8106 | `		goto Synchronize;` |
|         - |  8107 | `	}` |
|         - |  8108 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|    288679 |  8109 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        61 |  8110 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|        38 |  8111 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|        19 |  8112 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|        42 |  8113 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8114 | `			return SXERR_ABORT;` |
|        42 |  8115 | `		}else if( rc != SXRET_OK ){` |
|         3 |  8116 | `			goto Synchronize;` |
|         - |  8117 | `		}` |
|        18 |  8118 | `	}` |
|         - |  8119 | `	/* Advance the stream cursor */` |
|    288677 |  8120 | `	pGen->pIn++;` |
|    288677 |  8121 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|         - |  8122 | `		/* Invalid declaration */` |
|       ! 0 |  8123 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|       ! 0 |  8124 | `		if( rc == SXERR_ABORT ){` |
|         - |  8125 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8126 | `			return SXERR_ABORT;` |
|         - |  8127 | `		}` |
|       ! 0 |  8128 | `		goto Synchronize;` |
|         - |  8129 | `	}` |
|    288677 |  8130 | `	pGen->pIn++; /* Jump the equal sign */` |
|         - |  8131 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|         - |  8132 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|         - |  8133 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|         - |  8134 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|    288672 |  8135 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|        39 |  8136 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|         8 |  8137 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8138 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|         2 |  8139 | `			&pClass->sName,pName,&sTypeText);` |
|         6 |  8140 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8141 | `			return SXERR_ABORT;` |
|         - |  8142 | `		}` |
|         6 |  8143 | `		goto Synchronize;` |
|         - |  8144 | `	}` |
|         - |  8145 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|         - |  8146 | `	 * constant initializer ("New expressions are not supported in this context").` |
|         - |  8147 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|    288673 |  8148 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|         5 |  8149 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8150 | `			"New expressions are not supported in this context");` |
|         5 |  8151 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8152 | `			return SXERR_ABORT;` |
|         - |  8153 | `		}` |
|         5 |  8154 | `		goto Synchronize;` |
|         - |  8155 | `	}` |
|         - |  8156 | `	/* Allocate a new class attribute */` |
|    288669 |  8157 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    288669 |  8158 | `	if( pCons ){` |
|    288669 |  8159 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|    288669 |  8160 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8161 | `			return SXERR_ABORT;` |
|         - |  8162 | `		}` |
|    144332 |  8163 | `	}` |
|    288669 |  8164 | `	if( pCons == 0 ){` |
|       ! 0 |  8165 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8166 | `		return SXERR_ABORT;` |
|         - |  8167 | `	}` |
|    288669 |  8168 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        35 |  8169 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|        16 |  8170 | `	}` |
|         - |  8171 | `	/* Swap bytecode container */` |
|    288669 |  8172 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    288669 |  8173 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|         - |  8174 | `	/* Compile constant value.` |
|         - |  8175 | `	 */` |
|    288669 |  8176 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    288669 |  8177 | `	if( rc == SXERR_EMPTY ){` |
|         3 |  8178 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|         3 |  8179 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8180 | `			return SXERR_ABORT;` |
|         - |  8181 | `		}` |
|         1 |  8182 | `	}` |
|         - |  8183 | `	/* Emit the done instruction */` |
|    288669 |  8184 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    288669 |  8185 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    288669 |  8186 | `	if( rc == SXERR_ABORT ){` |
|         - |  8187 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  8188 | `		return SXERR_ABORT;` |
|         - |  8189 | `	}` |
|         - |  8190 | `	/* All done,install the constant */` |
|    288669 |  8191 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|    288669 |  8192 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8193 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8194 | `		return SXERR_ABORT;` |
|         - |  8195 | `	}` |
|    288669 |  8196 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  8197 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|         3 |  8198 | `		pGen->pIn++; /* Jump the comma */` |
|         3 |  8199 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 |  8200 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 |  8201 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 |  8202 | `				pTok--;` |
|       ! 0 |  8203 | `			}` |
|       ! 0 |  8204 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  8205 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|       ! 0 |  8206 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 |  8207 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8208 | `				return SXERR_ABORT;` |
|         - |  8209 | `			}` |
|       ! 0 |  8210 | `		}else{` |
|         3 |  8211 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|         3 |  8212 | `				goto loop;` |
|         - |  8213 | `			}` |
|         - |  8214 | `		}` |
|       ! 0 |  8215 | `	}` |
|    288667 |  8216 | `	SySetRelease(&aUnionAlts);` |
|    288667 |  8217 | `	return SXRET_OK;` |
|         5 |  8218 | `Synchronize:` |
|        13 |  8219 | `	SySetRelease(&aUnionAlts);` |
|         - |  8220 | `	/* Synchronize with the first semi-colon */` |
|        45 |  8221 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        35 |  8222 | `		pGen->pIn++;` |
|         3 |  8223 | `	}` |
|        13 |  8224 | `	return SXERR_CORRUPT;` |
|    144341 |  8225 | `}` |
|         - |  8226 | `/*` |
|         - |  8227 | ` * complie a class attribute or Properties in the PHP jargon.` |
|         - |  8228 | ` * According to the PHP language reference manual` |
|         - |  8229 | ` *  Properties` |
|         - |  8230 | ` *  Class member variables are called "properties". You may also see them referred` |
|         - |  8231 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|         - |  8232 | ` *  of this reference we will use "properties". They are defined by using one` |
|         - |  8233 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|         - |  8234 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|         - |  8235 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|         - |  8236 | ` *  and must not depend on run-time information in order to be evaluated.` |
|         - |  8237 | ` * Symisc eXtension.` |
|         - |  8238 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|         - |  8239 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  8240 | ` *  Example:` |
|         - |  8241 | ` *   class Test{` |
|         - |  8242 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|         - |  8243 | ` *   };` |
|         - |  8244 | ` *   var_dump(TEST::myVar);` |
|         - |  8245 | ` *   Refer to the official documentation for more information on the powerful extension` |
|         - |  8246 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|         - |  8247 | ` */` |
|         - |  8248 | `/*` |
|         - |  8249 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|         - |  8250 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|         - |  8251 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|         - |  8252 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|         - |  8253 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|         - |  8254 | ` */` |
|   2324796 |  8255 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|         5 |  8256 | `{` |
|   2324801 |  8257 | `	SyToken *p = pStart;` |
|   2324801 |  8258 | `	int bFirst = 1;` |
|   2324801 |  8259 | `	if( p >= pEnd ) return 0;` |
|         - |  8260 | ``	/* Optional nullable `?` shorthand. */`` |
|   2324801 |  8261 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|        35 |  8262 | `		p++;` |
|        35 |  8263 | `		if( p >= pEnd ) return 0;` |
|        16 |  8264 | `	}` |
|         - |  8265 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|         - |  8266 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|         - |  8267 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|         - |  8268 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   1162398 |  8269 | `	for(;;){` |
|   2324821 |  8270 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|         - |  8271 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|         3 |  8272 | `			p++;` |
|         9 |  8273 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|         3 |  8274 | `			if( p >= pEnd ) return 0;` |
|         3 |  8275 | `			p++; /* skip ')' */` |
|         2 |  8276 | `		}else{` |
|         - |  8277 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|         - |  8278 | ``			 * then any `&`-joined intersection members. */`` |
|   2324819 |  8279 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|   2324819 |  8280 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  8281 | `				return 0;` |
|         - |  8282 | `			}` |
|         - |  8283 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|         - |  8284 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|         - |  8285 | `			 * may still appear at the initial dispatch site). */` |
|   2324819 |  8286 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|   2324771 |  8287 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|   2324766 |  8288 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    102902 |  8289 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|   2324489 |  8290 | `					return 0;` |
|         - |  8291 | `				}` |
|       141 |  8292 | `			}` |
|       335 |  8293 | `			p++;` |
|       337 |  8294 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  8295 | `				p += 2;` |
|         1 |  8296 | `			}` |
|       498 |  8297 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|       338 |  8298 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  8299 | `				p++; /* skip '&' */` |
|         3 |  8300 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|         3 |  8301 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|         3 |  8302 | `				p++;` |
|         3 |  8303 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       ! 0 |  8304 | `					p += 2;` |
|       ! 0 |  8305 | `				}` |
|         1 |  8306 | `			}` |
|         - |  8307 | `		}` |
|       337 |  8308 | `		bFirst = 0;` |
|       332 |  8309 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        25 |  8310 | `			&& p->sData.zString[0] == '\|' ){` |
|        25 |  8311 | ``			p++; /* next `\|`-separated part */`` |
|        25 |  8312 | `			continue;` |
|         - |  8313 | `		}` |
|       317 |  8314 | `		break;` |
|       ! 0 |  8315 | `	}` |
|       317 |  8316 | `	if( p >= pEnd ) return 0;` |
|       317 |  8317 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   1162403 |  8318 | `}` |
|         - |  8319 |  |
|         - |  8320 | `/*` |
|         - |  8321 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|         - |  8322 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|         - |  8323 | ` * if not). Recognized forms:` |
|         - |  8324 | ` *   ?Type, array, bool, int, float, string, object,` |
|         - |  8325 | ` *   self, parent, \Ns\ClassName, ClassName` |
|         - |  8326 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|         - |  8327 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|         - |  8328 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|         - |  8329 | ` * on unrecoverable error.` |
|         - |  8330 | ` *` |
|         - |  8331 | ` * When a type is parsed:` |
|         - |  8332 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|         - |  8333 | ` *   *pClass is set to the class name (for class types)` |
|         - |  8334 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|         - |  8335 | ` *   *pTypeText is set to the original text span of the type` |
|         - |  8336 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|         - |  8337 | ` */` |
|       322 |  8338 | `static sxi32 GenStateParsePropertyType(` |
|         - |  8339 | `	ph7_gen_state *pGen,` |
|         - |  8340 | `	sxu32 *pnType,` |
|         - |  8341 | `	SyString *pClass,` |
|         - |  8342 | `	sxi32 *piTypeFlags,` |
|         - |  8343 | `	SyString *pTypeText,` |
|         - |  8344 | `	SySet *pAlts` |
|         5 |  8345 | `){` |
|       327 |  8346 | `	sxi32 iFlags = 0;` |
|         - |  8347 | `	sxi32 rc;` |
|       327 |  8348 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  8349 | `		return SXRET_OK;` |
|         - |  8350 | `	}` |
|         - |  8351 | `	/* If the first token is '$', there's no type */` |
|       327 |  8352 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       ! 0 |  8353 | `		return SXRET_OK;` |
|         - |  8354 | `	}` |
|       327 |  8355 | `	rc = GenStateParseUnionTypeDecl(` |
|       161 |  8356 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|         - |  8357 | `		PH7_CLASS_ATTR_NULLABLE,` |
|         - |  8358 | `		PH7_CLASS_ATTR_UNION,` |
|         - |  8359 | `		/* bAllowVoid */ 0,` |
|       322 |  8360 | `		pGen->pIn->nLine);` |
|       327 |  8361 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8362 | `		return rc;` |
|         - |  8363 | `	}` |
|         - |  8364 | `	/* Verify next token is '$' (start of property name) */` |
|       327 |  8365 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  8366 | `		return SXERR_SYNTAX;` |
|         - |  8367 | `	}` |
|       327 |  8368 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|       327 |  8369 | `	return SXRET_OK;` |
|       166 |  8370 | `}` |
|         - |  8371 |  |
|         - |  8372 | `/*` |
|         - |  8373 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|         - |  8374 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|         - |  8375 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|         - |  8376 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|         - |  8377 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|         - |  8378 | ` * by the type parser itself before reaching here.` |
|         - |  8379 | ` *` |
|         - |  8380 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|         - |  8381 | ` * use in the error message.` |
|         - |  8382 | ` */` |
|       498 |  8383 | `static int GenStateIsDisallowedPropertyAtom(` |
|         - |  8384 | `	sxu32 nType,` |
|         - |  8385 | `	const SyString *pClass,` |
|         - |  8386 | `	const char **pzName,` |
|         - |  8387 | `	sxu32 *pnName)` |
|         5 |  8388 | `{` |
|         - |  8389 | `	const char *z;` |
|         - |  8390 | `	sxu32 n;` |
|       503 |  8391 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|       449 |  8392 | `		return 0;` |
|         - |  8393 | `	}` |
|        59 |  8394 | `	z = pClass->zString;` |
|        59 |  8395 | `	n = pClass->nByte;` |
|        59 |  8396 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|         8 |  8397 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|         - |  8398 | `	}` |
|         - |  8399 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|         - |  8400 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|         - |  8401 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|        52 |  8402 | `	return 0;` |
|       254 |  8403 | `}` |
|         - |  8404 |  |
|         - |  8405 | `/*` |
|         - |  8406 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|         - |  8407 | ` * constant) — the main atom plus any union alternatives — against the` |
|         - |  8408 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|         - |  8409 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|         - |  8410 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|         - |  8411 | ` * type T" vs "Class constant C::X cannot have type T").` |
|         - |  8412 | ` *` |
|         - |  8413 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|         - |  8414 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|         - |  8415 | ` */` |
|       436 |  8416 | `static sxi32 GenStateValidateMemberType(` |
|         - |  8417 | `	ph7_gen_state *pGen,` |
|         - |  8418 | `	ph7_class *pClass,` |
|         - |  8419 | `	const SyString *pMemberName,` |
|         - |  8420 | `	sxu32 nType,` |
|         - |  8421 | `	const SyString *pTypeClass,` |
|         - |  8422 | `	const SyString *pTypeText,` |
|         - |  8423 | `	SySet *pUnionAlts,` |
|         - |  8424 | `	const char *zErrFmt,` |
|         - |  8425 | `	sxu32 nLine)` |
|         5 |  8426 | `{` |
|       441 |  8427 | `	const char *zBad = 0;` |
|       441 |  8428 | `	sxu32 nBad = 0;` |
|         - |  8429 | `	SyString sFallback;` |
|         - |  8430 | `	const SyString *pBad;` |
|         - |  8431 | `	sxi32 rc;` |
|       441 |  8432 | `	int bDisallowed = 0;` |
|       441 |  8433 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|         5 |  8434 | `		bDisallowed = 1;` |
|       439 |  8435 | `	}else if( pUnionAlts ){` |
|         - |  8436 | `		sxu32 i;` |
|        95 |  8437 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|        67 |  8438 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|        67 |  8439 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|         3 |  8440 | `				bDisallowed = 1;` |
|         3 |  8441 | `				break;` |
|         - |  8442 | `			}` |
|        35 |  8443 | `		}` |
|        15 |  8444 | `	}` |
|       441 |  8445 | `	if( !bDisallowed ){` |
|       435 |  8446 | `		return SXRET_OK;` |
|         - |  8447 | `	}` |
|         - |  8448 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|         - |  8449 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|         - |  8450 | `	 * canonical spelling if the type text is unavailable. */` |
|         8 |  8451 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|         8 |  8452 | `		pBad = pTypeText;` |
|         5 |  8453 | `	}else{` |
|       ! 0 |  8454 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|       ! 0 |  8455 | `		pBad = &sFallback;` |
|         - |  8456 | `	}` |
|        11 |  8457 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         3 |  8458 | `		zErrFmt,` |
|         3 |  8459 | `		&pClass->sName,pMemberName,pBad);` |
|         8 |  8460 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  8461 | `		return SXERR_ABORT;` |
|         - |  8462 | `	}` |
|         8 |  8463 | `	return SXERR_SYNTAX;` |
|       223 |  8464 | `}` |
|         - |  8465 | `/*` |
|         - |  8466 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|         - |  8467 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|         - |  8468 | ` * matched as a plain identifier in the class-member modifier position rather` |
|         - |  8469 | ` * than promoted to a lexer keyword.` |
|         - |  8470 | ` */` |
|  17876348 |  8471 | `static int GenStateIsReadonly(SyToken *pTok)` |
|         5 |  8472 | `{` |
|  18070502 |  8473 | `	return (pTok->nType & PH7_TK_ID)` |
|   9132323 |  8474 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
|  18070497 |  8475 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|         5 |  8476 | `}` |
|         - |  8477 | `/*` |
|         - |  8478 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|         - |  8479 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|         - |  8480 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|         - |  8481 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|         - |  8482 | ` */` |
|   6828596 |  8483 | `static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|         5 |  8484 | `{` |
|   6828601 |  8485 | `	*pnTok = 0;` |
|   6828596 |  8486 | `	if( &pTok[3] < pEnd` |
|   6431057 |  8487 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|   5427213 |  8488 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|   2410462 |  8489 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        16 |  8490 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|        16 |  8491 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|        21 |  8492 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|        17 |  8493 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|        17 |  8494 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|        17 |  8495 | `			*pnTok = 4;` |
|        17 |  8496 | `			return nKw;` |
|         - |  8497 | `		}` |
|       ! 0 |  8498 | `	}` |
|   6828585 |  8499 | `	return 0;` |
|   3414303 |  8500 | `}` |
|         - |  8501 | `/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */` |
|        16 |  8502 | `static sxi32 GenStateSetVisFlag(sxi32 nKw)` |
|         1 |  8503 | `{` |
|        17 |  8504 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|        13 |  8505 | `		return PH7_CLASS_ATTR_PRIVATE_SET;` |
|         - |  8506 | `	}` |
|         5 |  8507 | `	if( nKw == PH7_TKWRD_PROTECTED ){` |
|         3 |  8508 | `		return PH7_CLASS_ATTR_PROTECTED_SET;` |
|         - |  8509 | `	}` |
|         3 |  8510 | `	return PH7_CLASS_ATTR_PUBLIC_SET;` |
|         9 |  8511 | `}` |
|    452704 |  8512 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  8513 | `{` |
|    452709 |  8514 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  8515 | `	ph7_class_attr *pAttr;` |
|         - |  8516 | `	SyString *pName;` |
|         - |  8517 | `	sxi32 rc;` |
|    452709 |  8518 | `	sxu32 nType = 0;` |
|         - |  8519 | `	SyString sTypeClass;` |
|         - |  8520 | `	SyString sTypeText;` |
|         - |  8521 | `	SySet aUnionAlts;` |
|    452709 |  8522 | `	sxi32 iTypeFlags = 0;` |
|    452709 |  8523 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    452709 |  8524 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    452709 |  8525 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  8526 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|         - |  8527 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|         - |  8528 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|    452709 |  8529 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|        21 |  8530 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|         9 |  8531 | `	}` |
|         - |  8532 | `	/* Extract visibility level */` |
|    452709 |  8533 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  8534 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|    452870 |  8535 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       327 |  8536 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|       327 |  8537 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  8538 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  8539 | `			goto Synchronize;` |
|       327 |  8540 | `		}else if( rc == SXERR_SYNTAX ){` |
|       ! 0 |  8541 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8542 | `				"Invalid property type or declaration near '%z'",` |
|       ! 0 |  8543 | `				&pGen->pIn->sData);` |
|       ! 0 |  8544 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8545 | `				return SXERR_ABORT;` |
|         - |  8546 | `			}` |
|       ! 0 |  8547 | `			goto Synchronize;` |
|       327 |  8548 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  8549 | `			return SXERR_ABORT;` |
|         - |  8550 | `		}` |
|       161 |  8551 | `	}` |
|       ! 0 |  8552 | `loop:` |
|    452713 |  8553 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  8554 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|       ! 0 |  8555 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8556 | `			return SXERR_ABORT;` |
|         - |  8557 | `		}` |
|       ! 0 |  8558 | `		goto Synchronize;` |
|         - |  8559 | `	}` |
|    452713 |  8560 | `	pGen->pIn++; /* Jump the dollar sign */` |
|    452713 |  8561 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|         - |  8562 | `		/* Invalid attribute name */` |
|       ! 0 |  8563 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|       ! 0 |  8564 | `		if( rc == SXERR_ABORT ){` |
|         - |  8565 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8566 | `			return SXERR_ABORT;` |
|         - |  8567 | `		}` |
|       ! 0 |  8568 | `		goto Synchronize;` |
|         - |  8569 | `	}` |
|         - |  8570 | `	/* Peek attribute name */` |
|    452713 |  8571 | `	pName = &pGen->pIn->sData;` |
|         - |  8572 | `	/* Advance the stream cursor */` |
|    452713 |  8573 | `	pGen->pIn++;` |
|    452713 |  8574 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/\|PH7_TK_OCB/*'{' hooks*/)) == 0 ){` |
|         - |  8575 | `		/* Invalid declaration */` |
|         3 |  8576 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|         3 |  8577 | `		if( rc == SXERR_ABORT ){` |
|         - |  8578 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8579 | `			return SXERR_ABORT;` |
|         - |  8580 | `		}` |
|         3 |  8581 | `		goto Synchronize;` |
|         - |  8582 | `	}` |
|         - |  8583 | `	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and` |
|         - |  8584 | `	 * the read visibility must not be narrower than the set visibility. */` |
|    452711 |  8585 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
|        13 |  8586 | `		const char *zAvErr = 0;` |
|        19 |  8587 | `		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE` |
|        10 |  8588 | `			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED` |
|         2 |  8589 | `			: PH7_CLASS_PROT_PUBLIC;` |
|        13 |  8590 | `		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 |  8591 | `			zAvErr = "Property with asymmetric visibility %z::$%z must have type";` |
|        13 |  8592 | `		}else if( iProtection > iSetLevel ){` |
|       ! 0 |  8593 | `			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";` |
|       ! 0 |  8594 | `		}` |
|        13 |  8595 | `		if( zAvErr ){` |
|       ! 0 |  8596 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sName,pName);` |
|       ! 0 |  8597 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8598 | `				return SXERR_ABORT;` |
|         - |  8599 | `			}` |
|       ! 0 |  8600 | `			goto Synchronize;` |
|         - |  8601 | `		}` |
|         6 |  8602 | `	}` |
|         - |  8603 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|         - |  8604 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|    452711 |  8605 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|        43 |  8606 | `		const char *zRoErr = 0;` |
|        43 |  8607 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|         3 |  8608 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|        42 |  8609 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         6 |  8610 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|        39 |  8611 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|         6 |  8612 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|         2 |  8613 | `		}` |
|        43 |  8614 | `		if( zRoErr ){` |
|        13 |  8615 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|        13 |  8616 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8617 | `				return SXERR_ABORT;` |
|         - |  8618 | `			}` |
|        13 |  8619 | `			goto Synchronize;` |
|         - |  8620 | `		}` |
|        14 |  8621 | `	}` |
|         - |  8622 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|         - |  8623 | `	 * type atom or any union alternative. void/never are already rejected` |
|         - |  8624 | `	 * by the type parser. */` |
|    452701 |  8625 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       485 |  8626 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|         - |  8627 | `			&sTypeText,` |
|       320 |  8628 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       160 |  8629 | `			"Property %z::$%z cannot have type %z",nLine);` |
|       325 |  8630 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8631 | `			return SXERR_ABORT;` |
|       325 |  8632 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  8633 | `			goto Synchronize;` |
|         - |  8634 | `		}` |
|       160 |  8635 | `	}` |
|         - |  8636 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|    452701 |  8637 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|         4 |  8638 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  8639 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|         3 |  8640 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8641 | `			return SXERR_ABORT;` |
|         - |  8642 | `		}` |
|         3 |  8643 | `		goto Synchronize;` |
|         - |  8644 | `	}` |
|         - |  8645 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|         - |  8646 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|         - |  8647 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|         - |  8648 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|         - |  8649 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|         - |  8650 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|    452699 |  8651 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|         6 |  8652 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8653 | `			"New expressions are not supported in this context");` |
|         6 |  8654 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8655 | `			return SXERR_ABORT;` |
|         - |  8656 | `		}` |
|         6 |  8657 | `		goto Synchronize;` |
|         - |  8658 | `	}` |
|         - |  8659 | `	/* Allocate a new class attribute */` |
|    452695 |  8660 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    452695 |  8661 | `	if( pAttr ){` |
|    452695 |  8662 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|    452695 |  8663 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8664 | `			return SXERR_ABORT;` |
|         - |  8665 | `		}` |
|    226345 |  8666 | `	}` |
|    452695 |  8667 | `	if( pAttr == 0 ){` |
|       ! 0 |  8668 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  8669 | `		return SXERR_ABORT;` |
|         - |  8670 | `	}` |
|    452695 |  8671 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       323 |  8672 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       159 |  8673 | `	}` |
|    452695 |  8674 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|         - |  8675 | `		SySet *pInstrContainer;` |
|    330939 |  8676 | `		SyToken *pSavedDefEnd = pGen->pEnd;` |
|    330939 |  8677 | `		pGen->pIn++; /*Jump the equal sign */` |
|         - |  8678 | `		{` |
|         - |  8679 | `			/* Delimit the default expression: it ends at the declaration's` |
|         - |  8680 | `			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list` |
|         - |  8681 | ``			 * (`public string $w = "init" { get => …; }`) — the expression`` |
|         - |  8682 | `			 * compiler would otherwise run into the hook tokens. */` |
|    330939 |  8683 | `			SyToken *pScan = pGen->pIn;` |
|    330939 |  8684 | `			sxi32 iNest = 0;` |
|    715363 |  8685 | `			while( pScan < pGen->pEnd ){` |
|    715363 |  8686 | `				if( pScan->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     41843 |  8687 | `					iNest++;` |
|    694444 |  8688 | `				}else if( pScan->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|     41843 |  8689 | `					iNest--;` |
|    652606 |  8690 | `				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI\|PH7_TK_COMMA\|PH7_TK_OCB)) ){` |
|    330939 |  8691 | `					break;` |
|         - |  8692 | `				}` |
|    384429 |  8693 | `				pScan++;` |
|         5 |  8694 | `			}` |
|    330939 |  8695 | `			pGen->pEnd = pScan;` |
|         - |  8696 | `		}` |
|         - |  8697 | `		/* Swap bytecode container */` |
|    330939 |  8698 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    330939 |  8699 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|         - |  8700 | `		/* Compile attribute value.` |
|         - |  8701 | `		 */` |
|    330939 |  8702 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    330939 |  8703 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 |  8704 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|       ! 0 |  8705 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8706 | `				return SXERR_ABORT;` |
|         - |  8707 | `			}` |
|       ! 0 |  8708 | `		}` |
|         - |  8709 | `		/* Emit the done instruction */` |
|    330939 |  8710 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    330939 |  8711 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    330939 |  8712 | `		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */` |
|    330939 |  8713 | `		pGen->pEnd = pSavedDefEnd;` |
|    165467 |  8714 | `	}` |
|         - |  8715 | `	/* All done,install the attribute */` |
|    452695 |  8716 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|    452695 |  8717 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8718 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8719 | `		return SXERR_ABORT;` |
|         - |  8720 | `	}` |
|    452695 |  8721 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){` |
|         - |  8722 | ``		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.`` |
|         - |  8723 | `		 * The list ends the declaration at '}' — no trailing ';', no comma list. */` |
|        95 |  8724 | `		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);` |
|        95 |  8725 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8726 | `			return SXERR_ABORT;` |
|         - |  8727 | `		}` |
|        95 |  8728 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  8729 | `			goto Synchronize;` |
|         - |  8730 | `		}` |
|        95 |  8731 | `		SySetRelease(&aUnionAlts);` |
|        95 |  8732 | `		return SXRET_OK;` |
|         - |  8733 | `	}` |
|    452601 |  8734 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - |  8735 | ``		/* php 8.4: `abstract` on a property requires a hook list (php's exact`` |
|         - |  8736 | `		 * wording differs per declaration site) */` |
|       ! 0 |  8737 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  8738 | `			(pClass->iFlags & PH7_CLASS_INTERFACE)` |
|         - |  8739 | `				? "Interfaces may only include hooked properties"` |
|         - |  8740 | `				: "Only hooked properties may be declared abstract");` |
|       ! 0 |  8741 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8742 | `			return SXERR_ABORT;` |
|         - |  8743 | `		}` |
|       ! 0 |  8744 | `		goto Synchronize;` |
|         - |  8745 | `	}` |
|    452601 |  8746 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  8747 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|         5 |  8748 | `		pGen->pIn++; /* Jump the comma */` |
|         5 |  8749 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|       ! 0 |  8750 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 |  8751 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 |  8752 | `				pTok--;` |
|       ! 0 |  8753 | `			}` |
|       ! 0 |  8754 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  8755 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|       ! 0 |  8756 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 |  8757 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8758 | `				return SXERR_ABORT;` |
|         - |  8759 | `			}` |
|       ! 0 |  8760 | `		}else{` |
|         5 |  8761 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         5 |  8762 | `				goto loop;` |
|         - |  8763 | `			}` |
|         - |  8764 | `		}` |
|       ! 0 |  8765 | `	}` |
|    452597 |  8766 | `	SySetRelease(&aUnionAlts);` |
|    452597 |  8767 | `	return SXRET_OK;` |
|         9 |  8768 | `Synchronize:` |
|         - |  8769 | `	/* Synchronize with the first semi-colon */` |
|        56 |  8770 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        37 |  8771 | `		pGen->pIn++;` |
|         3 |  8772 | `	}` |
|        22 |  8773 | `	SySetRelease(&aUnionAlts);` |
|        22 |  8774 | `	return SXERR_CORRUPT;` |
|    226357 |  8775 | `}` |
|         - |  8776 | `/*` |
|         - |  8777 | ` * Compile a class method.` |
|         - |  8778 | ` *` |
|         - |  8779 | ` * Refer to the official documentation for more information` |
|         - |  8780 | ` * on the powerful extension introduced by the PH7 engine` |
|         - |  8781 | ` * to the OO subsystem such as full type hinting,method` |
|         - |  8782 | ` * overloading and many more.` |
|         - |  8783 | ` */` |
|   2370314 |  8784 | `static sxi32 GenStateCompileClassMethod(` |
|         - |  8785 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  8786 | `	sxi32 iProtection,   /* Visibility level */` |
|         - |  8787 | `	sxi32 iFlags,        /* Configuration flags */` |
|         - |  8788 | `	int doBody,          /* TRUE to process method body */` |
|         - |  8789 | `	ph7_class *pClass    /* Class this method belongs */` |
|         - |  8790 | `	)` |
|         5 |  8791 | `{` |
|   2370319 |  8792 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   2370319 |  8793 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|         - |  8794 | `	ph7_class_method *pMeth;` |
|         - |  8795 | `	sxi32 iFuncFlags;` |
|         - |  8796 | `	SyString *pName;` |
|         - |  8797 | `	SyToken *pEnd;` |
|         - |  8798 | `	sxi32 rc;` |
|         - |  8799 | `	/* Extract visibility level */` |
|   2370319 |  8800 | `	iProtection = GetProtectionLevel(iProtection);` |
|   2370319 |  8801 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   2370319 |  8802 | `	iFuncFlags = 0;` |
|   2370319 |  8803 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  8804 | `		/* Invalid method name */` |
|       ! 0 |  8805 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|       ! 0 |  8806 | `		if( rc == SXERR_ABORT ){` |
|         - |  8807 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8808 | `			return SXERR_ABORT;` |
|         - |  8809 | `		}` |
|       ! 0 |  8810 | `		goto Synchronize;` |
|         - |  8811 | `	}` |
|   2370319 |  8812 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - |  8813 | `		/* Return by reference,remember that */` |
|       ! 0 |  8814 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|         - |  8815 | `		/* Jump the '&' token */` |
|       ! 0 |  8816 | `		pGen->pIn++;` |
|       ! 0 |  8817 | `	}` |
|   2370319 |  8818 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  8819 | `		/* Invalid method name */` |
|       ! 0 |  8820 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|       ! 0 |  8821 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8822 | `			return SXERR_ABORT;` |
|         - |  8823 | `		}` |
|       ! 0 |  8824 | `		goto Synchronize;` |
|         - |  8825 | `	}` |
|         - |  8826 | `	/* Peek method name */` |
|   2370319 |  8827 | `	pName = &pGen->pIn->sData;` |
|   2370319 |  8828 | `	nLine = pGen->pIn->nLine;` |
|         - |  8829 | `	/* Jump the method name */` |
|   2370319 |  8830 | `	pGen->pIn++;` |
|   2370319 |  8831 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - |  8832 | `		/* Abstract method */` |
|    136723 |  8833 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       ! 0 |  8834 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8835 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|       ! 0 |  8836 | `				&pClass->sName,pName);` |
|       ! 0 |  8837 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8838 | `				return SXERR_ABORT;` |
|         - |  8839 | `			}` |
|       ! 0 |  8840 | `		}` |
|         - |  8841 | `		/* Assemble method signature only */` |
|    136723 |  8842 | `		doBody = FALSE;` |
|     68359 |  8843 | `	}` |
|   2370319 |  8844 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  8845 | `		/* Syntax error */` |
|       ! 0 |  8846 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|       ! 0 |  8847 | `		if( rc == SXERR_ABORT ){` |
|         - |  8848 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8849 | `			return SXERR_ABORT;` |
|         - |  8850 | `		}` |
|       ! 0 |  8851 | `		goto Synchronize;` |
|         - |  8852 | `	}` |
|         - |  8853 | `	/* Allocate a new class_method instance */` |
|   2370319 |  8854 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|   2370319 |  8855 | `	if( pMeth == 0 ){` |
|       ! 0 |  8856 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8857 | `		return SXERR_ABORT;` |
|         - |  8858 | `	}` |
|   2370319 |  8859 | `	pMeth->sFunc.nLine = nKwLine;` |
|   2370319 |  8860 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|   2370319 |  8861 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8862 | `		return SXERR_ABORT;` |
|         - |  8863 | `	}` |
|         - |  8864 | `	/* Jump the left parenthesis '(' */` |
|   2370319 |  8865 | `	pGen->pIn++;` |
|   2370319 |  8866 | `	pEnd = 0; /* cc warning */` |
|         - |  8867 | `	/* Delimit the method signature */` |
|   2370319 |  8868 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   2370319 |  8869 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  8870 | `		/* Syntax error */` |
|         3 |  8871 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|         3 |  8872 | `		if( rc == SXERR_ABORT ){` |
|         - |  8873 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8874 | `			return SXERR_ABORT;` |
|         - |  8875 | `		}` |
|         3 |  8876 | `		goto Synchronize;` |
|         - |  8877 | `	}` |
|         - |  8878 | `	{` |
|   2370317 |  8879 | `		int bIsCtor = 0;` |
|   2370317 |  8880 | `		int bAbstractCtor = 0;` |
|   2370312 |  8881 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|   1384606 |  8882 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|   2288599 |  8883 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|    163441 |  8884 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         3 |  8885 | `				bAbstractCtor = 1;` |
|         2 |  8886 | `			}else{` |
|    163439 |  8887 | `				bIsCtor = 1;` |
|         - |  8888 | `			}` |
|     81718 |  8889 | `		}` |
|   2370317 |  8890 | `		if( pGen->pIn < pEnd ){` |
|         - |  8891 | `			/* Collect method arguments */` |
|    850987 |  8892 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|    850987 |  8893 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8894 | `				return SXERR_ABORT;` |
|         - |  8895 | `			}` |
|    425491 |  8896 | `		}` |
|         - |  8897 | `	}` |
|         - |  8898 | `	/* Point past ')' and parse optional return type ': type' */` |
|   2370317 |  8899 | `	pGen->pIn = &pEnd[1];` |
|         - |  8900 | `	{` |
|   2370317 |  8901 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|   2370317 |  8902 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 |  8903 | `			return SXERR_ABORT;` |
|   2370317 |  8904 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       ! 0 |  8905 | `			goto Synchronize;` |
|         - |  8906 | `		}` |
|         - |  8907 | `	}` |
|         - |  8908 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|         - |  8909 | `	 * property init/typecheck is handled by the generic typed-property path` |
|         - |  8910 | `	 * since we mint real ph7_class_attr entries. */` |
|         - |  8911 | `	{` |
|   2370317 |  8912 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|         - |  8913 | `		sxu32 i;` |
|   3646629 |  8914 | `		for( i = 0; i < nArg; i++ ){` |
|   1276327 |  8915 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|         - |  8916 | `			ph7_class_attr *pAttr;` |
|   1276327 |  8917 | `			sxi32 iAttrFlags = 0;` |
|         - |  8918 | `			int bArgTyped;` |
|   1276327 |  8919 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   1276243 |  8920 | `				continue;` |
|         - |  8921 | `			}` |
|         - |  8922 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|         - |  8923 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|         - |  8924 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|        59 |  8925 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|        90 |  8926 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|        89 |  8927 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|         3 |  8928 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8929 | `					"Cannot declare variadic promoted property");` |
|         3 |  8930 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  8931 | `					return SXERR_ABORT;` |
|         - |  8932 | `				}` |
|         3 |  8933 | `				goto Synchronize;` |
|         - |  8934 | `			}` |
|         - |  8935 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|         - |  8936 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|         - |  8937 | `			 * appear as an alternative of a union type. */` |
|        87 |  8938 | `			if( bArgTyped ){` |
|       122 |  8939 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|        78 |  8940 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|        78 |  8941 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|        39 |  8942 | `					"Property %z::$%z cannot have type %z",nLine);` |
|        83 |  8943 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  8944 | `					return SXERR_ABORT;` |
|        83 |  8945 | `				}else if( rc != SXRET_OK ){` |
|         6 |  8946 | `					goto Synchronize;` |
|         - |  8947 | `				}` |
|        37 |  8948 | `			}` |
|         - |  8949 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|        83 |  8950 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|         4 |  8951 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  8952 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|         3 |  8953 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  8954 | `					return SXERR_ABORT;` |
|         - |  8955 | `				}` |
|         3 |  8956 | `				goto Synchronize;` |
|         - |  8957 | `			}` |
|        81 |  8958 | `			if( bArgTyped ){` |
|        77 |  8959 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        36 |  8960 | `			}` |
|        81 |  8961 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|         3 |  8962 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|         1 |  8963 | `			}` |
|        81 |  8964 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|         8 |  8965 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|         3 |  8966 | `			}` |
|        81 |  8967 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|         - |  8968 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|         - |  8969 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|        26 |  8970 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         4 |  8971 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  8972 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|         3 |  8973 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  8974 | `						return SXERR_ABORT;` |
|         - |  8975 | `					}` |
|         3 |  8976 | `					goto Synchronize;` |
|         - |  8977 | `				}` |
|        24 |  8978 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|        10 |  8979 | `			}` |
|        79 |  8980 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|         - |  8981 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|         5 |  8982 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 |  8983 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8984 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|       ! 0 |  8985 | `						&pClass->sName,&pArg->sName);` |
|       ! 0 |  8986 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  8987 | `						return SXERR_ABORT;` |
|         - |  8988 | `					}` |
|       ! 0 |  8989 | `					goto Synchronize;` |
|         - |  8990 | `				}` |
|         5 |  8991 | `				iAttrFlags \|= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)` |
|         2 |  8992 | `					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;` |
|         2 |  8993 | `			}` |
|        79 |  8994 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|        79 |  8995 | `			if( pAttr == 0 ){` |
|       ! 0 |  8996 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8997 | `				return SXERR_ABORT;` |
|         - |  8998 | `			}` |
|        79 |  8999 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|        77 |  9000 | `				pAttr->nType = pArg->nType;` |
|        77 |  9001 | `				pAttr->sClass = pArg->sClass;` |
|        77 |  9002 | `				pAttr->sTypeName = pArg->sTypeName;` |
|        77 |  9003 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  9004 | `					sxu32 k;` |
|        20 |  9005 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|        14 |  9006 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|        14 |  9007 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|         8 |  9008 | `					}` |
|         3 |  9009 | `				}` |
|        36 |  9010 | `			}` |
|        79 |  9011 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|        79 |  9012 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9013 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9014 | `				return SXERR_ABORT;` |
|         - |  9015 | `			}` |
|        42 |  9016 | `		}` |
|         - |  9017 | `	}` |
|   2370307 |  9018 | `	if( doBody ){` |
|         - |  9019 | `		/* Compile method body */` |
|   2233589 |  9020 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|   2233589 |  9021 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  9022 | `			return SXERR_ABORT;` |
|         - |  9023 | `		}` |
|         - |  9024 | `		/* The cursor sits just past the body's closing brace */` |
|   2233589 |  9025 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|   1116797 |  9026 | `	}else{` |
|         - |  9027 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|    136723 |  9028 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|    136723 |  9029 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|     68359 |  9030 | `		}` |
|         - |  9031 | `		/* Only method signature is allowed */` |
|    136723 |  9032 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|       ! 0 |  9033 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9034 | `				"Expected ';' after method signature '%z'",pName);` |
|       ! 0 |  9035 | `				if( rc == SXERR_ABORT ){` |
|         - |  9036 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9037 | `					return SXERR_ABORT;` |
|         - |  9038 | `				}` |
|       ! 0 |  9039 | `				return SXERR_CORRUPT;` |
|         - |  9040 | `			}` |
|         - |  9041 | `	}` |
|         - |  9042 | `	/* All done,install the method */` |
|   2370307 |  9043 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|   2370307 |  9044 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  9045 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9046 | `		return SXERR_ABORT;` |
|         - |  9047 | `	}` |
|   2370307 |  9048 | `	return SXRET_OK;` |
|         6 |  9049 | `Synchronize:` |
|         - |  9050 | `	/* Synchronize with the first semi-colon */` |
|        40 |  9051 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        28 |  9052 | `		pGen->pIn++;` |
|         4 |  9053 | `	}` |
|        16 |  9054 | `	return SXERR_CORRUPT;` |
|   1185162 |  9055 | `}` |
|         - |  9056 | `/*` |
|         - |  9057 | `` * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a`` |
|         - |  9058 | ` * property declaration. Each hook body is synthesized into a hidden public` |
|         - |  9059 | ` * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,` |
|         - |  9060 | ` * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /` |
|         - |  9061 | ` * OP_STORE route reads and plain writes through them (a per-instance guard` |
|         - |  9062 | ` * makes $this->NAME inside a hook body address the raw backing slot — php's` |
|         - |  9063 | `` * rule that hooks see the backing store). `get => expr;` compiles as an`` |
|         - |  9064 | `` * implicit return (the arrow-fn pattern); `set => expr;` compiles the same`` |
|         - |  9065 | ` * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return` |
|         - |  9066 | `` * value to the backing slot. A `set` without a parameter list receives the`` |
|         - |  9067 | `` * implicit `$value` formal.`` |
|         - |  9068 | ` * On entry pGen->pIn sits on '{'; on success it sits just past '}'.` |
|         - |  9069 | ` */` |
|         - |  9070 | `/*` |
|         - |  9071 | `` * Whether any token in [pStart, pEnd) spells `$this->NAME` (this property's own`` |
|         - |  9072 | `` * name; `?->` and `::` member ops count too). php 8.4's virtual-vs-backed rule:`` |
|         - |  9073 | ` * a hooked property is BACKED iff any of its OWN hook bodies references it by` |
|         - |  9074 | ` * name through $this — otherwise it is VIRTUAL: no backing store, no default` |
|         - |  9075 | ` * allowed, excluded from the raw object surfaces.` |
|         - |  9076 | ` */` |
|        94 |  9077 | `static int GenStateHookBodyRefsProp(SyToken *pStart,SyToken *pEnd,const SyString *pName)` |
|         1 |  9078 | `{` |
|         - |  9079 | `	SyToken *p;` |
|       345 |  9080 | `	for( p = pStart ; p + 1 < pEnd ; p++ ){` |
|       303 |  9081 | `		if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|       223 |  9082 | `			continue;` |
|         - |  9083 | `		}` |
|         - |  9084 | ``		/* `$this->NAME` (also `?->`/`::`) */`` |
|        80 |  9085 | `		if( p + 3 < pEnd` |
|        80 |  9086 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        80 |  9087 | `		 && p[1].sData.nByte == sizeof("this")-1` |
|        73 |  9088 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0` |
|        66 |  9089 | `		 && GenStateTokenIsMemberOp(&p[2])` |
|        66 |  9090 | `		 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        66 |  9091 | `		 && p[3].sData.nByte == pName->nByte` |
|        60 |  9092 | `		 && SyMemcmp((const void *)p[3].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        51 |  9093 | `			return 1;` |
|         - |  9094 | `		}` |
|         - |  9095 | ``		/* `parent::$NAME` (the parent::$x::get() hook-call form): the parent`` |
|         - |  9096 | `		 * hook operates on the shared per-instance backing store, so the` |
|         - |  9097 | `		 * property is backed (php compiles a default alongside it). */` |
|        30 |  9098 | `		if( p > pStart` |
|        26 |  9099 | `		 && GenStateTokenIsMemberOp(&p[-1])` |
|        12 |  9100 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         2 |  9101 | `		 && p[1].sData.nByte == pName->nByte` |
|         3 |  9102 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|         3 |  9103 | `			return 1;` |
|         - |  9104 | `		}` |
|        15 |  9105 | `	}` |
|        43 |  9106 | `	return 0;` |
|        48 |  9107 | `}` |
|         - |  9108 | `/*` |
|         - |  9109 | ` * True when p opens php 8.4's parent-hook call form` |
|         - |  9110 | `` * `parent :: $ NAME :: get\|set (` (7 tokens through the '(').`` |
|         - |  9111 | ` */` |
|       990 |  9112 | `static int GenStateIsParentHookCallAt(SyToken *p,SyToken *pEnd)` |
|         1 |  9113 | `{` |
|      1167 |  9114 | `	return p + 6 < pEnd` |
|       671 |  9115 | `	 && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       250 |  9116 | `	 && p->sData.nByte == sizeof("parent")-1` |
|        81 |  9117 | `	 && SyMemcmp((const void *)p->sData.zString,(const void *)"parent",sizeof("parent")-1) == 0` |
|        11 |  9118 | `	 && GenStateTokenIsMemberOp(&p[1])` |
|         8 |  9119 | `	 && (p[2].nType & PH7_TK_DOLLAR) != 0` |
|         8 |  9120 | `	 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 |  9121 | `	 && GenStateTokenIsMemberOp(&p[4])` |
|         8 |  9122 | `	 && (p[5].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 |  9123 | `	 && p[5].sData.nByte == 3` |
|         8 |  9124 | `	 && (SyMemcmp((const void *)p[5].sData.zString,(const void *)"get",3) == 0` |
|         6 |  9125 | `	  \|\| SyMemcmp((const void *)p[5].sData.zString,(const void *)"set",3) == 0)` |
|      1166 |  9126 | `	 && (p[6].nType & PH7_TK_LPAREN) != 0;` |
|         1 |  9127 | `}` |
|         - |  9128 | `/*` |
|         - |  9129 | `` * Rewrite php 8.4 `parent::$x::get(...)` / `parent::$x::set(...)` calls in a`` |
|         - |  9130 | ` * hook body into calls of the parent class's synthesized hook method` |
|         - |  9131 | `` * (`parent::__phl_hook_get_x(...)`). Builds a token COPY into pCopy (only`` |
|         - |  9132 | ` * called when GenStateIsParentHookCallAt matched somewhere in the range);` |
|         - |  9133 | ` * copied tokens keep pointing at source-owned lexeme storage, and the` |
|         - |  9134 | ` * synthesized method-name lexemes are VM-allocator owned. Returns SXRET_OK` |
|         - |  9135 | ` * or SXERR_MEM.` |
|         - |  9136 | ` */` |
|         4 |  9137 | `static sxi32 GenStateRewriteParentHookCalls(ph7_gen_state *pGen,SySet *pCopy,` |
|         - |  9138 | `	SyToken *pStart,SyToken *pEnd)` |
|         1 |  9139 | `{` |
|         5 |  9140 | `	SyToken *p = pStart;` |
|        35 |  9141 | `	while( p < pEnd ){` |
|        31 |  9142 | `		if( GenStateIsParentHookCallAt(p,pEnd) ){` |
|         - |  9143 | `			SyToken sTok;` |
|         - |  9144 | `			char zName[384];` |
|         - |  9145 | `			sxu32 nName;` |
|         - |  9146 | `			char *zDup;` |
|         - |  9147 | ``			/* `parent` `::` */`` |
|         5 |  9148 | `			SySetPut(pCopy,(const void *)&p[0]);` |
|         5 |  9149 | `			SySetPut(pCopy,(const void *)&p[1]);` |
|         7 |  9150 | `			nName = SyBufferFormat(zName,sizeof(zName),"__phl_hook_%.3s_%.*s",` |
|         4 |  9151 | `				p[5].sData.zString,(int)p[3].sData.nByte,p[3].sData.zString);` |
|         5 |  9152 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|         5 |  9153 | `			if( zDup == 0 ){` |
|       ! 0 |  9154 | `				return SXERR_MEM;` |
|         - |  9155 | `			}` |
|         5 |  9156 | `			sTok = p[3]; /* keep the line info of the property name */` |
|         5 |  9157 | `			sTok.nType = PH7_TK_ID;` |
|         5 |  9158 | `			SyStringInitFromBuf(&sTok.sData,zDup,nName);` |
|         5 |  9159 | `			sTok.pUserData = 0;` |
|         5 |  9160 | `			SySetPut(pCopy,(const void *)&sTok);` |
|         5 |  9161 | `			p += 6; /* continue at the '(' — arguments copy through unchanged */` |
|         5 |  9162 | `			continue;` |
|         - |  9163 | `		}` |
|        27 |  9164 | `		SySetPut(pCopy,(const void *)p);` |
|        27 |  9165 | `		p++;` |
|         1 |  9166 | `	}` |
|         5 |  9167 | `	return SXRET_OK;` |
|         3 |  9168 | `}` |
|        94 |  9169 | `static sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)` |
|         1 |  9170 | `{` |
|        95 |  9171 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  9172 | `	sxi32 rc;` |
|        95 |  9173 | `	int bRefsSelf = 0;` |
|        95 |  9174 | `	pGen->pIn++; /* Jump '{' */` |
|       253 |  9175 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|         - |  9176 | `		char zHook[384];` |
|         - |  9177 | `		SyString sHookName;` |
|         - |  9178 | `		ph7_class_method *pMeth;` |
|         - |  9179 | `		int bGet;` |
|       159 |  9180 | `		sxu32 nHLine = pGen->pIn->nLine;` |
|       159 |  9181 | `		if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        15 |  9182 | `			pGen->pIn++; /* stray ';' between hooks */` |
|        22 |  9183 | `			continue;` |
|         - |  9184 | `		}` |
|       145 |  9185 | `		if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - |  9186 | `			/* by-reference get hook: not modeled (loud, recorded) */` |
|       ! 0 |  9187 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9188 | `				"By-reference property hooks are not supported for %z::$%z",` |
|       ! 0 |  9189 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9190 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9191 | `				return SXERR_ABORT;` |
|         - |  9192 | `			}` |
|       ! 0 |  9193 | `			return SXERR_CORRUPT;` |
|         - |  9194 | `		}` |
|       145 |  9195 | `		if( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  9196 | `			goto HookSyntax;` |
|         - |  9197 | `		}` |
|       144 |  9198 | `		if( pGen->pIn->sData.nByte == 3` |
|       145 |  9199 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"get",3) == 0 ){` |
|        79 |  9200 | `			bGet = 1;` |
|       106 |  9201 | `		}else if( pGen->pIn->sData.nByte == 3` |
|        67 |  9202 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"set",3) == 0 ){` |
|        67 |  9203 | `			bGet = 0;` |
|        34 |  9204 | `		}else{` |
|       ! 0 |  9205 | `			goto HookSyntax;` |
|         - |  9206 | `		}` |
|       145 |  9207 | `		pGen->pIn++; /* Jump 'get'/'set' */` |
|       145 |  9208 | `		sHookName.zString = zHook;` |
|       217 |  9209 | `		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",` |
|        72 |  9210 | `			bGet ? "get" : "set",&pAttr->sName);` |
|       145 |  9211 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB)) ){` |
|         - |  9212 | ``			/* Bare `get;` / `set;` — an ABSTRACT hook declaration (php 8.4):`` |
|         - |  9213 | ``			 * legal only on an `abstract` property or inside an interface. The`` |
|         - |  9214 | `			 * synthesized method carries PH7_CLASS_ATTR_ABSTRACT and rides the` |
|         - |  9215 | `			 * existing must-implement machinery; a concrete hook override (or a` |
|         - |  9216 | `			 * plain property, see GenStateCheckAbstractMethods) satisfies it. */` |
|        14 |  9217 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|         8 |  9218 | `			 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 |  9219 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9220 | `					"Non-abstract property hook must have a body");` |
|       ! 0 |  9221 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9222 | `					return SXERR_ABORT;` |
|         - |  9223 | `				}` |
|       ! 0 |  9224 | `				return SXERR_CORRUPT;` |
|         - |  9225 | `			}` |
|        15 |  9226 | `			pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - |  9227 | `				PH7_CLASS_PROT_PUBLIC,PH7_CLASS_ATTR_ABSTRACT,0);` |
|        15 |  9228 | `			if( pMeth == 0 ){` |
|       ! 0 |  9229 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9230 | `				return SXERR_ABORT;` |
|         - |  9231 | `			}` |
|        15 |  9232 | `			pMeth->sFunc.nLine = nHLine;` |
|        15 |  9233 | `			if( !bGet ){` |
|         - |  9234 | ``				/* The implicit `$value` formal keeps the stub's signature`` |
|         - |  9235 | `				 * compatible with concrete set-hook implementations (which` |
|         - |  9236 | `				 * always carry one parameter). It takes the PROPERTY's declared` |
|         - |  9237 | `				 * type (php: the abstract set's parameter type IS the property` |
|         - |  9238 | `				 * type), so the override contravariance check accepts a typed` |
|         - |  9239 | ``				 * `set(int $v)` implementation on an `int $x` requirement. */`` |
|         - |  9240 | `				ph7_vm_func_arg sVArg;` |
|         7 |  9241 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|         7 |  9242 | `				if( zVName == 0 ){` |
|       ! 0 |  9243 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9244 | `					return SXERR_ABORT;` |
|         - |  9245 | `				}` |
|         7 |  9246 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|         7 |  9247 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|         7 |  9248 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         7 |  9249 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         7 |  9250 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|         7 |  9251 | `				sVArg.nType = pAttr->nType;` |
|         7 |  9252 | `				sVArg.sClass = pAttr->sClass;` |
|         7 |  9253 | `				sVArg.sTypeName = pAttr->sTypeName;` |
|         7 |  9254 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       ! 0 |  9255 | `					sVArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|       ! 0 |  9256 | `				}` |
|         7 |  9257 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|         3 |  9258 | `			}` |
|        15 |  9259 | `			rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|        15 |  9260 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9261 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9262 | `				return SXERR_ABORT;` |
|         - |  9263 | `			}` |
|        15 |  9264 | `			pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|        15 |  9265 | `			continue; /* the loop consumes the ';' as a stray separator */` |
|         - |  9266 | `		}` |
|       130 |  9267 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|       131 |  9268 | `		 \|\| (pClass->iFlags & PH7_CLASS_INTERFACE) != 0 ){` |
|         - |  9269 | `			/* php: an abstract/interface property hook cannot carry a body */` |
|       ! 0 |  9270 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9271 | `				"Abstract property hook cannot have body");` |
|       ! 0 |  9272 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9273 | `				return SXERR_ABORT;` |
|         - |  9274 | `			}` |
|       ! 0 |  9275 | `			return SXERR_CORRUPT;` |
|         - |  9276 | `		}` |
|       131 |  9277 | `		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - |  9278 | `			PH7_CLASS_PROT_PUBLIC,0,0);` |
|       131 |  9279 | `		if( pMeth == 0 ){` |
|       ! 0 |  9280 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9281 | `			return SXERR_ABORT;` |
|         - |  9282 | `		}` |
|       131 |  9283 | `		pMeth->sFunc.nLine = nHLine;` |
|       131 |  9284 | `		if( !bGet ){` |
|         - |  9285 | ``			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */`` |
|        61 |  9286 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        17 |  9287 | `				SyToken *pRp = 0;` |
|        17 |  9288 | `				pGen->pIn++;` |
|        17 |  9289 | `				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|        17 |  9290 | `				if( pRp >= pGen->pEnd ){` |
|       ! 0 |  9291 | `					goto HookSyntax;` |
|         - |  9292 | `				}` |
|        17 |  9293 | `				if( pGen->pIn < pRp ){` |
|        17 |  9294 | `					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);` |
|        17 |  9295 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9296 | `						return SXERR_ABORT;` |
|         - |  9297 | `					}` |
|         8 |  9298 | `				}` |
|        17 |  9299 | `				pGen->pIn = &pRp[1];` |
|         8 |  9300 | `			}` |
|        61 |  9301 | `			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){` |
|         - |  9302 | `				/* Implicit $value formal */` |
|         - |  9303 | `				ph7_vm_func_arg sVArg;` |
|        45 |  9304 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|        45 |  9305 | `				if( zVName == 0 ){` |
|       ! 0 |  9306 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9307 | `					return SXERR_ABORT;` |
|         - |  9308 | `				}` |
|        45 |  9309 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|        45 |  9310 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|        45 |  9311 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        45 |  9312 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        45 |  9313 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|        45 |  9314 | `				SyStringInitFromBuf(&sVArg.sTypeName,0,0);` |
|        45 |  9315 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|        22 |  9316 | `			}` |
|        30 |  9317 | `		}` |
|       165 |  9318 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - |  9319 | `			/* Block body */` |
|        69 |  9320 | `			SyToken *pBodyStart = pGen->pIn;` |
|        69 |  9321 | `			SyToken *pCloser = 0;` |
|        69 |  9322 | `			int bParentCall = 0;` |
|        69 |  9323 | `			PH7_DelimitNestedTokens(&pBodyStart[1],pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);` |
|        69 |  9324 | `			if( pCloser < pGen->pEnd ){` |
|         - |  9325 | `				SyToken *pScan;` |
|       753 |  9326 | `				for( pScan = &pBodyStart[1] ; pScan < pCloser ; pScan++ ){` |
|       687 |  9327 | `					if( GenStateIsParentHookCallAt(pScan,pCloser) ){` |
|         3 |  9328 | `						bParentCall = 1;` |
|         3 |  9329 | `						break;` |
|         - |  9330 | `					}` |
|       343 |  9331 | `				}` |
|        34 |  9332 | `			}` |
|        69 |  9333 | `			if( bParentCall ){` |
|         - |  9334 | ``				/* `parent::$x::get()` inside the body: compile a REWRITTEN copy`` |
|         - |  9335 | `				 * of the body tokens (the call becomes the parent's synthesized` |
|         - |  9336 | `				 * hook method), then continue past the original body. */` |
|         - |  9337 | `				SySet sBody;` |
|         3 |  9338 | `				SyToken *pSavedEnd = pGen->pEnd;` |
|         3 |  9339 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 |  9340 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,&pCloser[1]);` |
|         3 |  9341 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9342 | `					SySetRelease(&sBody);` |
|       ! 0 |  9343 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9344 | `					return SXERR_ABORT;` |
|         - |  9345 | `				}` |
|         3 |  9346 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 |  9347 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         3 |  9348 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|         3 |  9349 | `				pGen->pIn = &pCloser[1];` |
|         3 |  9350 | `				pGen->pEnd = pSavedEnd;` |
|         3 |  9351 | `				SySetRelease(&sBody);` |
|         3 |  9352 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9353 | `					return SXERR_ABORT;` |
|         - |  9354 | `				}` |
|         3 |  9355 | `				pMeth->sFunc.nEndLine = pCloser->nLine;` |
|         2 |  9356 | `			}else{` |
|        67 |  9357 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|        67 |  9358 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9359 | `					return SXERR_ABORT;` |
|         - |  9360 | `				}` |
|        67 |  9361 | `				pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|         - |  9362 | `			}` |
|        69 |  9363 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        17 |  9364 | `				bRefsSelf = 1;` |
|         9 |  9365 | `			}` |
|       128 |  9366 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){` |
|         - |  9367 | ``			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */`` |
|         - |  9368 | `			GenBlock *pBlock;` |
|         - |  9369 | `			SySet *pInstrContainer;` |
|         - |  9370 | `			SyToken *pBodyStart;` |
|         - |  9371 | `			SyToken *pExprEnd;` |
|        63 |  9372 | `			SyToken *pSavedEnd = 0;` |
|         - |  9373 | `			SySet sBody;` |
|        63 |  9374 | `			int bParentCall = 0;` |
|        63 |  9375 | `			pGen->pIn++; /* Jump '=>' */` |
|        63 |  9376 | `			pBodyStart = pGen->pIn;` |
|         - |  9377 | `			/* Delimit the expression (first top-level ';', or a closer that` |
|         - |  9378 | `			 * would end the enclosing hook list) and rewrite any` |
|         - |  9379 | ``			 * `parent::$x::get()` calls into the parent's synthesized hook`` |
|         - |  9380 | `			 * method on a token copy. */` |
|         - |  9381 | `			{` |
|        63 |  9382 | `				sxi32 iNest = 0;` |
|        63 |  9383 | `				pExprEnd = pBodyStart;` |
|       355 |  9384 | `				while( pExprEnd < pGen->pEnd ){` |
|       355 |  9385 | `					if( pExprEnd->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         9 |  9386 | `						iNest++;` |
|       351 |  9387 | `					}else if( pExprEnd->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         9 |  9388 | `						if( iNest <= 0 ){` |
|       ! 0 |  9389 | `							break;` |
|         - |  9390 | `						}` |
|         9 |  9391 | `						iNest--;` |
|       343 |  9392 | `					}else if( iNest <= 0 && (pExprEnd->nType & PH7_TK_SEMI) ){` |
|        63 |  9393 | `						break;` |
|         - |  9394 | `					}` |
|       293 |  9395 | `					pExprEnd++;` |
|         1 |  9396 | `				}` |
|         - |  9397 | `			}` |
|         - |  9398 | `			{` |
|         - |  9399 | `				SyToken *pScan;` |
|       335 |  9400 | `				for( pScan = pBodyStart ; pScan < pExprEnd ; pScan++ ){` |
|       275 |  9401 | `					if( GenStateIsParentHookCallAt(pScan,pExprEnd) ){` |
|         3 |  9402 | `						bParentCall = 1;` |
|         3 |  9403 | `						break;` |
|         - |  9404 | `					}` |
|       137 |  9405 | `				}` |
|         - |  9406 | `			}` |
|        63 |  9407 | `			if( bParentCall ){` |
|         3 |  9408 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 |  9409 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,pExprEnd);` |
|         3 |  9410 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9411 | `					SySetRelease(&sBody);` |
|       ! 0 |  9412 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9413 | `					return SXERR_ABORT;` |
|         - |  9414 | `				}` |
|         3 |  9415 | `				pSavedEnd = pGen->pEnd;` |
|         3 |  9416 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 |  9417 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         1 |  9418 | `			}` |
|        94 |  9419 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|        62 |  9420 | `				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);` |
|        63 |  9421 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9422 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");` |
|       ! 0 |  9423 | `				return SXERR_ABORT;` |
|         - |  9424 | `			}` |
|        63 |  9425 | `			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        63 |  9426 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|        63 |  9427 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|        63 |  9428 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        63 |  9429 | `			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        63 |  9430 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        63 |  9431 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        63 |  9432 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        63 |  9433 | `			if( bParentCall ){` |
|         3 |  9434 | `				pGen->pIn = pExprEnd; /* land on the original ';' */` |
|         3 |  9435 | `				pGen->pEnd = pSavedEnd;` |
|         3 |  9436 | `				SySetRelease(&sBody);` |
|         1 |  9437 | `			}` |
|        63 |  9438 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9439 | `				return SXERR_ABORT;` |
|         - |  9440 | `			}` |
|        63 |  9441 | `			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;` |
|        63 |  9442 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        37 |  9443 | `				bRefsSelf = 1;` |
|        18 |  9444 | `			}` |
|        63 |  9445 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        63 |  9446 | `				pGen->pIn++; /* Jump ';' */` |
|        31 |  9447 | `			}` |
|        63 |  9448 | `			if( !bGet ){` |
|         - |  9449 | ``				/* `set => expr` assigns the expression to the backing store:`` |
|         - |  9450 | `				 * the dispatcher consumes the implicit return value — which` |
|         - |  9451 | `				 * also makes the property BACKED (php: the shorthand is sugar` |
|         - |  9452 | ``				 * for `$this->NAME = expr`). */`` |
|         3 |  9453 | `				pMeth->sFunc.iFlags \|= VM_FUNC_HOOK_SET_EXPR;` |
|         3 |  9454 | `				bRefsSelf = 1;` |
|         1 |  9455 | `			}` |
|        32 |  9456 | `		}else{` |
|       ! 0 |  9457 | `			goto HookSyntax;` |
|         - |  9458 | `		}` |
|       131 |  9459 | `		rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|       131 |  9460 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  9461 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9462 | `			return SXERR_ABORT;` |
|         - |  9463 | `		}` |
|       131 |  9464 | `		pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|         1 |  9465 | `	}` |
|        95 |  9466 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|       ! 0 |  9467 | `		goto HookSyntax;` |
|         - |  9468 | `	}` |
|        95 |  9469 | `	pGen->pIn++; /* Jump '}' */` |
|        95 |  9470 | `	if( !bRefsSelf ){` |
|         - |  9471 | ``		/* php 8.4 virtual-vs-backed: no hook body referenced `$this->NAME`, so`` |
|         - |  9472 | `		 * this property is VIRTUAL — php gives it no backing store and forbids` |
|         - |  9473 | `		 * a default value (compile fatal, php's exact wording). */` |
|        41 |  9474 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_HOOK_VIRTUAL;` |
|        41 |  9475 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|       ! 0 |  9476 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9477 | `				"Cannot specify default value for virtual hooked property %z::$%z",` |
|       ! 0 |  9478 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9479 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9480 | `				return SXERR_ABORT;` |
|         - |  9481 | `			}` |
|       ! 0 |  9482 | `			return SXERR_CORRUPT;` |
|         - |  9483 | `		}` |
|        20 |  9484 | `	}` |
|        95 |  9485 | `	return SXRET_OK;` |
|       ! 0 |  9486 | `HookSyntax:` |
|       ! 0 |  9487 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9488 | `		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",` |
|       ! 0 |  9489 | `		&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9490 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  9491 | `		return SXERR_ABORT;` |
|         - |  9492 | `	}` |
|       ! 0 |  9493 | `	return SXERR_CORRUPT;` |
|        48 |  9494 | `}` |
|         - |  9495 | `/*` |
|         - |  9496 | ` * Compile an object interface.` |
|         - |  9497 | ` *  According to the PHP language reference manual` |
|         - |  9498 | ` *   Object Interfaces:` |
|         - |  9499 | ` *   Object interfaces allow you to create code which specifies which methods` |
|         - |  9500 | ` *   a class must implement, without having to define how these methods are handled.` |
|         - |  9501 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|         - |  9502 | ` *   class, but without any of the methods having their contents defined.` |
|         - |  9503 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|         - |  9504 | ` */` |
|     68432 |  9505 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|         5 |  9506 | `{` |
|     68437 |  9507 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  9508 | `	ph7_class *pClass,*pBase;` |
|         - |  9509 | `	SyToken *pEnd,*pTmp;` |
|         - |  9510 | `	SyString *pName;` |
|         - |  9511 | `	sxi32 nKwrd;` |
|         - |  9512 | `	sxi32 rc;` |
|         - |  9513 | `	/* Jump the 'interface' keyword */` |
|     68437 |  9514 | `	pGen->pIn++;` |
|         - |  9515 | `	/* Extract interface name */` |
|     68437 |  9516 | `	pName = &pGen->pIn->sData;` |
|         - |  9517 | `	/* Advance the stream cursor */` |
|     68437 |  9518 | `	pGen->pIn++;` |
|         - |  9519 | `	/* Build FQN and obtain a raw class */ {` |
|         - |  9520 | `		SyBlob sFQN;` |
|         - |  9521 | `		SyString sFQNStr;` |
|     68437 |  9522 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     68437 |  9523 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     68437 |  9524 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|     68437 |  9525 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     68437 |  9526 | `		SyBlobRelease(&sFQN);` |
|         - |  9527 | `	}` |
|     68437 |  9528 | `	if( pClass == 0 ){` |
|       ! 0 |  9529 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9530 | `		return SXERR_ABORT;` |
|         - |  9531 | `	}` |
|     68437 |  9532 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     68437 |  9533 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  9534 | `		return SXERR_ABORT;` |
|         - |  9535 | `	}` |
|         - |  9536 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|     68437 |  9537 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|         - |  9538 | `	/* Assume no base class is given */` |
|     68437 |  9539 | `	pBase = 0;` |
|     68437 |  9540 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     26587 |  9541 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     26587 |  9542 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|         - |  9543 | `			SyBlob sResolved;` |
|         - |  9544 | `			SyString sBaseName;` |
|         - |  9545 | `			sxu32 nRefLine;` |
|         - |  9546 | `			/* Extract base interface */` |
|     26587 |  9547 | `			pGen->pIn++;` |
|     26587 |  9548 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|     26587 |  9549 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     26587 |  9550 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 |  9551 | `				SyBlobRelease(&sResolved);` |
|       ! 0 |  9552 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9553 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|       ! 0 |  9554 | `					pName);` |
|       ! 0 |  9555 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9556 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9557 | `					return SXERR_ABORT;` |
|         - |  9558 | `				}` |
|       ! 0 |  9559 | `				return SXRET_OK;` |
|         - |  9560 | `			}` |
|     39878 |  9561 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|     26582 |  9562 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     26587 |  9563 | `			SyStringInitFromBuf(&sBaseName,` |
|         - |  9564 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - |  9565 | `			/* Only interfaces is allowed */` |
|     26587 |  9566 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 |  9567 | `				pBase = pBase->pNextName;` |
|       ! 0 |  9568 | `			}` |
|     26587 |  9569 | `			if( pBase == 0 ){` |
|       ! 0 |  9570 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - |  9571 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|       ! 0 |  9572 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9573 | `					SyBlobRelease(&sResolved);` |
|       ! 0 |  9574 | `					return SXERR_ABORT;` |
|         - |  9575 | `				}` |
|       ! 0 |  9576 | `			}` |
|     26587 |  9577 | `			SyBlobRelease(&sResolved);` |
|     13291 |  9578 | `		}` |
|     13291 |  9579 | `	}` |
|     68437 |  9580 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - |  9581 | `		/* Syntax error */` |
|       ! 0 |  9582 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|       ! 0 |  9583 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9584 | `		if( rc == SXERR_ABORT ){` |
|         - |  9585 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9586 | `			return SXERR_ABORT;` |
|         - |  9587 | `		}` |
|       ! 0 |  9588 | `		return SXRET_OK;` |
|         - |  9589 | `	}` |
|     68437 |  9590 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     68437 |  9591 | `	pEnd = 0; /* cc warning */` |
|         - |  9592 | `	/* Delimit the interface body */` |
|     68437 |  9593 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|     68437 |  9594 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  9595 | `		/* Syntax error */` |
|       ! 0 |  9596 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|       ! 0 |  9597 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9598 | `		if( rc == SXERR_ABORT ){` |
|         - |  9599 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9600 | `			return SXERR_ABORT;` |
|         - |  9601 | `		}` |
|       ! 0 |  9602 | `		return SXRET_OK;` |
|         - |  9603 | `	}` |
|         - |  9604 | `	/* The delimiter token is the interface body's closing brace */` |
|     68437 |  9605 | `	pClass->nEndLine = pEnd->nLine;` |
|         - |  9606 | `	/* Swap token stream */` |
|     68437 |  9607 | `	pTmp = pGen->pEnd;` |
|     68437 |  9608 | `	pGen->pEnd = pEnd;` |
|         - |  9609 | `	/* Start the parse process` |
|         - |  9610 | `	 * Note (According to the PHP reference manual):` |
|         - |  9611 | `	 *  Only constants and function signatures(without body) are allowed.` |
|         - |  9612 | `	 *  Only 'public' visibility is allowed.` |
|         - |  9613 | `	 */` |
|    125349 |  9614 | `	for(;;){` |
|         - |  9615 | `		/* Jump leading/trailing semi-colons */` |
|    432973 |  9616 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    182271 |  9617 | `			pGen->pIn++;` |
|         5 |  9618 | `		}` |
|    250707 |  9619 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - |  9620 | `			/* End of interface body */` |
|     68433 |  9621 | `			break;` |
|         - |  9622 | `		}` |
|         - |  9623 | `		/* Bind a directly-preceding docblock to this member */` |
|    182279 |  9624 | `		GenStateSetPendingDoc(&(*pGen));` |
|    182279 |  9625 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 |  9626 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  9627 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|       ! 0 |  9628 | `				&pGen->pIn->sData,pName);` |
|       ! 0 |  9629 | `			if( rc == SXERR_ABORT ){` |
|         - |  9630 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  9631 | `				return SXERR_ABORT;` |
|         - |  9632 | `			}` |
|       ! 0 |  9633 | `			goto done;` |
|         - |  9634 | `		}` |
|         - |  9635 | `		/* Extract the current keyword */` |
|    182279 |  9636 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    182279 |  9637 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - |  9638 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|         - |  9639 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|         3 |  9640 | `			const char *zKind = "member";` |
|         3 |  9641 | `			SyString *pMemberName = 0;` |
|         3 |  9642 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|         3 |  9643 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|         3 |  9644 | `				if( nNext == PH7_TKWRD_CONST ){` |
|         3 |  9645 | `					zKind = "constant";` |
|         3 |  9646 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|         3 |  9647 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|         2 |  9648 | `					}` |
|         1 |  9649 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|       ! 0 |  9650 | `					zKind = "method";` |
|       ! 0 |  9651 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       ! 0 |  9652 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       ! 0 |  9653 | `					}` |
|       ! 0 |  9654 | `				}` |
|         1 |  9655 | `			}` |
|         3 |  9656 | `			if( pMemberName ){` |
|         4 |  9657 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         1 |  9658 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|         2 |  9659 | `			}else{` |
|       ! 0 |  9660 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9661 | `					"Access type for interface %s must be public",zKind);` |
|         - |  9662 | `			}` |
|         3 |  9663 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9664 | `				return SXERR_ABORT;` |
|         - |  9665 | `			}` |
|         3 |  9666 | `			goto done;` |
|         - |  9667 | `		}` |
|    182277 |  9668 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|       ! 0 |  9669 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9670 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 |  9671 | `			if( rc == SXERR_ABORT ){` |
|         - |  9672 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  9673 | `				return SXERR_ABORT;` |
|         - |  9674 | `			}` |
|       ! 0 |  9675 | `			goto done;` |
|         - |  9676 | `		}` |
|    182277 |  9677 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|         - |  9678 | `			/* Advance the stream cursor */` |
|    129115 |  9679 | `			pGen->pIn++;` |
|    129110 |  9680 | `			if( pGen->pIn < pGen->pEnd` |
|    129115 |  9681 | `			 && ((pGen->pIn->nType & PH7_TK_DOLLAR) != 0` |
|    129110 |  9682 | `			  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         - |  9683 | ``				/* PHP 8.4: `public [?T] $x { get; set; }` — a hooked-property`` |
|         - |  9684 | `				 * requirement. The attribute compiler + hook parser handle it` |
|         - |  9685 | `				 * (bare hooks are implicitly abstract inside an interface; a` |
|         - |  9686 | `				 * property without hooks is ITS "Interfaces may only include` |
|         - |  9687 | `				 * hooked properties" error). */` |
|       ! 0 |  9688 | `				rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 |  9689 | `					PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 |  9690 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9691 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9692 | `						return SXERR_ABORT;` |
|         - |  9693 | `					}` |
|       ! 0 |  9694 | `					goto done;` |
|         - |  9695 | `				}` |
|       ! 0 |  9696 | `				continue;` |
|         - |  9697 | `			}` |
|    129115 |  9698 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|         - |  9699 | `				/* A type NAME (a plain identifier, e.g. a class type) followed by` |
|         - |  9700 | `				 * '$' also opens a hooked-property requirement. */` |
|       ! 0 |  9701 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) != 0` |
|       ! 0 |  9702 | `				 && (pGen->pIn + 1) < pGen->pEnd` |
|       ! 0 |  9703 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|       ! 0 |  9704 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 |  9705 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 |  9706 | `					if( rc != SXRET_OK ){` |
|       ! 0 |  9707 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  9708 | `							return SXERR_ABORT;` |
|         - |  9709 | `						}` |
|       ! 0 |  9710 | `						goto done;` |
|         - |  9711 | `					}` |
|       ! 0 |  9712 | `					continue;` |
|         - |  9713 | `				}` |
|       ! 0 |  9714 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9715 | `					"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 |  9716 | `				if( rc == SXERR_ABORT ){` |
|         - |  9717 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9718 | `					return SXERR_ABORT;` |
|         - |  9719 | `				}` |
|       ! 0 |  9720 | `				goto done;` |
|         - |  9721 | `			}` |
|    129115 |  9722 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    129115 |  9723 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|         - |  9724 | `				/* A type KEYWORD (int/string/bool/…) followed by '$' opens a` |
|         - |  9725 | `				 * hooked-property requirement (PHP 8.4). */` |
|         4 |  9726 | `				if( (pGen->pIn + 1) < pGen->pEnd` |
|         5 |  9727 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|         7 |  9728 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|         2 |  9729 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|         5 |  9730 | `					if( rc != SXRET_OK ){` |
|       ! 0 |  9731 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  9732 | `							return SXERR_ABORT;` |
|         - |  9733 | `						}` |
|       ! 0 |  9734 | `						goto done;` |
|         - |  9735 | `					}` |
|         5 |  9736 | `					continue;` |
|         - |  9737 | `				}` |
|       ! 0 |  9738 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9739 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 |  9740 | `				if( rc == SXERR_ABORT ){` |
|         - |  9741 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9742 | `					return SXERR_ABORT;` |
|         - |  9743 | `				}` |
|       ! 0 |  9744 | `				goto done;` |
|         - |  9745 | `			}` |
|     64553 |  9746 | `		}` |
|    182273 |  9747 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|         - |  9748 | `			/* Parse constant */` |
|     53163 |  9749 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|     53163 |  9750 | `			if( rc != SXRET_OK ){` |
|         3 |  9751 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9752 | `					return SXERR_ABORT;` |
|         - |  9753 | `				}` |
|         3 |  9754 | `				goto done;` |
|         - |  9755 | `			}` |
|     26583 |  9756 | `		}else{` |
|    129115 |  9757 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|    129115 |  9758 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - |  9759 | `				/* Static method,record that */` |
|     11393 |  9760 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|         - |  9761 | `				/* Advance the stream cursor */` |
|     11393 |  9762 | `				pGen->pIn++;` |
|     11388 |  9763 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     11393 |  9764 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 |  9765 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9766 | `							"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 |  9767 | `						if( rc == SXERR_ABORT ){` |
|         - |  9768 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 |  9769 | `							return SXERR_ABORT;` |
|         - |  9770 | `						}` |
|       ! 0 |  9771 | `						goto done;` |
|         - |  9772 | `				}` |
|      5694 |  9773 | `			}` |
|         - |  9774 | `			/* Process method signature (no body for interface methods) */` |
|    129115 |  9775 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|    129115 |  9776 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9777 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9778 | `					return SXERR_ABORT;` |
|         - |  9779 | `				}` |
|       ! 0 |  9780 | `				goto done;` |
|         - |  9781 | `			}` |
|         - |  9782 | `		}` |
|         5 |  9783 | `	}` |
|         - |  9784 | `	/* Install the interface */` |
|     68433 |  9785 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     68433 |  9786 | `	if( rc == SXRET_OK && pBase ){` |
|         - |  9787 | `		/* Inherit from the base interface */` |
|     26587 |  9788 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|     13291 |  9789 | `	}` |
|     68433 |  9790 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  9791 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9792 | `		return SXERR_ABORT;` |
|         - |  9793 | `	}` |
|     34214 |  9794 | `done:` |
|         - |  9795 | `	/* Point beyond the interface body */` |
|     68437 |  9796 | `	pGen->pIn  = &pEnd[1];` |
|     68437 |  9797 | `	pGen->pEnd = pTmp;` |
|     68437 |  9798 | `	return PH7_OK;` |
|     34221 |  9799 | `}` |
|         - |  9800 | `/*` |
|         - |  9801 | ` * Compile a user-defined class.` |
|         - |  9802 | ` * According to the PHP language reference manual` |
|         - |  9803 | ` *  class` |
|         - |  9804 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|         - |  9805 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|         - |  9806 | ` *  of the properties and methods belonging to the class.` |
|         - |  9807 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|         - |  9808 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|         - |  9809 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|         - |  9810 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - |  9811 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|         - |  9812 | ` *  (called "methods").` |
|         - |  9813 | ` */` |
|         - |  9814 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|         - |  9815 | `typedef struct TraitUseEntry TraitUseEntry;` |
|         - |  9816 | `struct TraitUseEntry {` |
|         - |  9817 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|         - |  9818 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|         - |  9819 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|         - |  9820 | `};` |
|         - |  9821 | `/*` |
|         - |  9822 | ` * Validate that methods implementing interface contracts have compatible` |
|         - |  9823 | ` * signatures: public visibility and at least as many parameters as declared.` |
|         - |  9824 | ` */` |
|    350960 |  9825 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 |  9826 | `{` |
|         - |  9827 | `	ph7_class **apIface;` |
|         - |  9828 | `	sxu32 nIface,i;` |
|         - |  9829 | `	sxi32 rc;` |
|    350965 |  9830 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|       ! 0 |  9831 | `		return SXRET_OK;` |
|         - |  9832 | `	}` |
|    350965 |  9833 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    350965 |  9834 | `	nIface = SySetUsed(&pClass->aInterface);` |
|    704325 |  9835 | `	for(i = 0; i < nIface; i++){` |
|    353365 |  9836 | `		ph7_class *pIface = apIface[i];` |
|         - |  9837 | `		SyHashEntry *pEntry;` |
|    353365 |  9838 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   1018245 |  9839 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|    664885 |  9840 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|         - |  9841 | `			ph7_class_method *pImplMeth;` |
|    664885 |  9842 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|         - |  9843 | `			/* Find the implementing method in the class */` |
|    664885 |  9844 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|    664885 |  9845 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        23 |  9846 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|         - |  9847 | `			}` |
|         - |  9848 | `			/* Check visibility: interface methods must be implemented as public */` |
|    664867 |  9849 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|         4 |  9850 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - |  9851 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|         1 |  9852 | `					&pClass->sName,pMName,&pIface->sName);` |
|         3 |  9853 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9854 | `					return SXERR_ABORT;` |
|         - |  9855 | `				}` |
|         1 |  9856 | `			}` |
|         - |  9857 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|         - |  9858 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|         - |  9859 | `			 */` |
|         - |  9860 | `			{` |
|    664867 |  9861 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|    664867 |  9862 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|    664867 |  9863 | `				int sigError = 0;` |
|    664867 |  9864 | `				if( nImplArgs < nIfaceArgs ){` |
|         3 |  9865 | `					sigError = 1;` |
|    664866 |  9866 | `				}else if( nImplArgs > nIfaceArgs ){` |
|         - |  9867 | `					/* Extra parameters must all have default values */` |
|      3805 |  9868 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|         - |  9869 | `					sxu32 k;` |
|      7603 |  9870 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|      3805 |  9871 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|         3 |  9872 | `							sigError = 1;` |
|         3 |  9873 | `							break;` |
|         - |  9874 | `						}` |
|      1904 |  9875 | `					}` |
|      1900 |  9876 | `				}` |
|    664867 |  9877 | `				if( sigError ){` |
|         - |  9878 | `					SyBlob sImplSig, sIfaceSig;` |
|         - |  9879 | `					ph7_vm_func_arg *aArgs;` |
|         - |  9880 | `					sxu32 j;` |
|         6 |  9881 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|         6 |  9882 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|         - |  9883 | `					/* Build implementing method signature */` |
|         6 |  9884 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|        12 |  9885 | `					for(j = 0; j < nImplArgs; j++){` |
|         8 |  9886 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|         8 |  9887 | `						SyBlobAppend(&sImplSig,"$",1);` |
|         8 |  9888 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 |  9889 | `					}` |
|         - |  9890 | `					/* Build interface method signature */` |
|         6 |  9891 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|        12 |  9892 | `					for(j = 0; j < nIfaceArgs; j++){` |
|         8 |  9893 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|         8 |  9894 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|         8 |  9895 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 |  9896 | `					}` |
|         8 |  9897 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - |  9898 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|         2 |  9899 | `						&pClass->sName,pMName,` |
|         4 |  9900 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|         2 |  9901 | `						&pIface->sName,pMName,` |
|         4 |  9902 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|         6 |  9903 | `					SyBlobRelease(&sImplSig);` |
|         6 |  9904 | `					SyBlobRelease(&sIfaceSig);` |
|         6 |  9905 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9906 | `						return SXERR_ABORT;` |
|         - |  9907 | `					}` |
|         2 |  9908 | `				}` |
|         - |  9909 | `			}` |
|         5 |  9910 | `		}` |
|    176685 |  9911 | `	}` |
|    350965 |  9912 | `	return SXRET_OK;` |
|    175485 |  9913 | `}` |
|         - |  9914 | `/*` |
|         - |  9915 | ` * An abstract property-hook stub (__phl_hook_{get,set}_NAME) is satisfied by` |
|         - |  9916 | ` * the class declaring a PLAIN (non-abstract, non-hooked) property NAME: php` |
|         - |  9917 | `` * lets a plain property implement `{ get; set; }` requirements — its raw`` |
|         - |  9918 | ` * read/write IS the default get/set. A concrete hook override replaced the` |
|         - |  9919 | ` * stub in hMethod already, so a surviving stub next to a HOOKED property` |
|         - |  9920 | ` * means that specific hook is still missing.` |
|         - |  9921 | ` */` |
|        38 |  9922 | `static int GenStateAbstractHookSatisfied(ph7_class *pClass,const SyString *pMName)` |
|         5 |  9923 | `{` |
|         - |  9924 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|         - |  9925 | `	ph7_class_attr *pProp;` |
|        38 |  9926 | `	if( pMName->nByte <= nPfx` |
|        27 |  9927 | `	 \|\| (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) != 0` |
|         4 |  9928 | `	  && SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) != 0) ){` |
|        36 |  9929 | `		return 0; /* not a hook stub */` |
|         - |  9930 | `	}` |
|         7 |  9931 | `	pProp = PH7_ClassExtractAttribute(pClass,&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|         7 |  9932 | `	return pProp != 0` |
|         6 |  9933 | `		&& (pProp->iFlags & (PH7_CLASS_ATTR_ABSTRACT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|         3 |  9934 | `			\|PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) == 0;` |
|        24 |  9935 | `}` |
|         - |  9936 | `/*` |
|         - |  9937 | ` * Append an abstract member's display name to the message blob, translating a` |
|         - |  9938 | `` * property-hook stub (__phl_hook_get_x) to php's `$x::get` form.`` |
|         - |  9939 | ` */` |
|        16 |  9940 | `static void GenStateAppendAbstractMemberName(SyBlob *pMsg,const SyString *pMName)` |
|         4 |  9941 | `{` |
|         - |  9942 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|        16 |  9943 | `	if( pMName->nByte > nPfx` |
|        12 |  9944 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|       ! 0 |  9945 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0) ){` |
|       ! 0 |  9946 | `		SyBlobAppend(pMsg,"$",1);` |
|       ! 0 |  9947 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|       ! 0 |  9948 | `		SyBlobAppend(pMsg,"::",2);` |
|       ! 0 |  9949 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[sizeof("__phl_hook_")-1],3);` |
|       ! 0 |  9950 | `		return;` |
|         - |  9951 | `	}` |
|        20 |  9952 | `	SyBlobAppend(pMsg,(const void *)pMName->zString,pMName->nByte);` |
|        12 |  9953 | `}` |
|         - |  9954 | `/*` |
|         - |  9955 | ` * Check that a concrete class has no remaining abstract methods.` |
|         - |  9956 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|         - |  9957 | ` */` |
|    350960 |  9958 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 |  9959 | `{` |
|         - |  9960 | `	ph7_class_method *pMeth;` |
|         - |  9961 | `	SyHashEntry *pEntry;` |
|         - |  9962 | `	sxu32 nAbstract;` |
|         - |  9963 | `	SyBlob sMsg;` |
|         - |  9964 | `	sxi32 rc;` |
|         - |  9965 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|    350965 |  9966 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     15233 |  9967 | `		return SXRET_OK;` |
|         - |  9968 | `	}` |
|         - |  9969 | `	/* Count abstract methods */` |
|    335737 |  9970 | `	nAbstract = 0;` |
|    335737 |  9971 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   4954991 |  9972 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   4451393 |  9973 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   4451393 |  9974 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        27 |  9975 | `			if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|         7 |  9976 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - |  9977 | `			}` |
|        20 |  9978 | `			nAbstract++;` |
|         8 |  9979 | `		}` |
|         5 |  9980 | `	}` |
|    335737 |  9981 | `	if( nAbstract == 0 ){` |
|    335723 |  9982 | `		return SXRET_OK;` |
|         - |  9983 | `	}` |
|         - |  9984 | `	/* Build the error message listing all abstract methods with origins */` |
|        18 |  9985 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|        18 |  9986 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|         - |  9987 | `		"be declared abstract or implement the remaining method%s (",` |
|         7 |  9988 | `		&pClass->sName,nAbstract,` |
|         7 |  9989 | `		(nAbstract > 1 ? "s" : ""),` |
|         7 |  9990 | `		(nAbstract > 1 ? "s" : ""));` |
|         - |  9991 | `	/* Second pass: list methods with origins */` |
|         - |  9992 | `	{` |
|        18 |  9993 | `		sxu32 nListed = 0;` |
|        18 |  9994 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|        36 |  9995 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|        22 |  9996 | `			ph7_class *pOrigin = 0;` |
|         - |  9997 | `			SyString *pMName;` |
|        22 |  9998 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        22 |  9999 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|         3 | 10000 | `				continue;` |
|         - | 10001 | `			}` |
|        20 | 10002 | `			pMName = &pMeth->sFunc.sName;` |
|        20 | 10003 | `			if( GenStateAbstractHookSatisfied(pClass,pMName) ){` |
|       ! 0 | 10004 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - | 10005 | `			}` |
|        20 | 10006 | `			if( nListed > 0 ){` |
|         3 | 10007 | `				SyBlobAppend(&sMsg,", ",2);` |
|         1 | 10008 | `			}` |
|         - | 10009 | `			/* Find the origin of this abstract method.` |
|         - | 10010 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|         - | 10011 | `			 * inheritance chains) take precedence for interface-declared` |
|         - | 10012 | `			 * methods. Abstract class methods only win when the class` |
|         - | 10013 | `			 * itself declared the abstract method (not inherited from` |
|         - | 10014 | `			 * an interface). Trait methods are adopted into the using` |
|         - | 10015 | `			 * class's namespace.` |
|         - | 10016 | `			 */` |
|         - | 10017 | `			{` |
|         - | 10018 | `				ph7_class **apIface;` |
|         - | 10019 | `				ph7_class **apTrait;` |
|         - | 10020 | `				ph7_class *pWalk;` |
|         - | 10021 | `				sxu32 i;` |
|         - | 10022 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|         - | 10023 | `				 * (one that was written in the class body, not inherited from an` |
|         - | 10024 | `				 * interface). PHP attributes origin to the declaring class.` |
|         - | 10025 | `				 */` |
|        20 | 10026 | `				if( pClass->pBase ){` |
|        11 | 10027 | `					pWalk = pClass->pBase;` |
|        19 | 10028 | `					while( pWalk ){` |
|         - | 10029 | `						ph7_class_method *pParentMeth;` |
|        13 | 10030 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|        13 | 10031 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|         - | 10032 | `							/* Exclude methods that came from an interface anywhere` |
|         - | 10033 | `							 * in this class's ancestor chain.` |
|         - | 10034 | `							 */` |
|        13 | 10035 | `							int fromIface = 0;` |
|        13 | 10036 | `							ph7_class *pAnc = pWalk;` |
|        17 | 10037 | `							while( pAnc ){` |
|         - | 10038 | `								ph7_class **apPI;` |
|         - | 10039 | `								sxu32 j;` |
|        15 | 10040 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|        15 | 10041 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|        10 | 10042 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|        10 | 10043 | `										fromIface = 1;` |
|        10 | 10044 | `										break;` |
|         - | 10045 | `									}` |
|       ! 0 | 10046 | `								}` |
|        15 | 10047 | `								if( fromIface ) break;` |
|         6 | 10048 | `								pAnc = pAnc->pBase;` |
|         2 | 10049 | `							}` |
|        13 | 10050 | `							if( !fromIface ){` |
|         3 | 10051 | `								pOrigin = pWalk;` |
|         3 | 10052 | `								break;` |
|         - | 10053 | `							}` |
|         4 | 10054 | `						}` |
|        10 | 10055 | `						pWalk = pWalk->pBase;` |
|         2 | 10056 | `					}` |
|         4 | 10057 | `				}` |
|         - | 10058 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|         - | 10059 | `				 * each interface's own parent chain for the deepest origin.` |
|         - | 10060 | `				 */` |
|        20 | 10061 | `				if( !pOrigin ){` |
|        18 | 10062 | `					pWalk = pClass;` |
|        40 | 10063 | `					while( pWalk && !pOrigin ){` |
|        26 | 10064 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|        26 | 10065 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|        16 | 10066 | `							ph7_class *pIface = apIface[i];` |
|        16 | 10067 | `							ph7_class *pDeepest = 0;` |
|        28 | 10068 | `							while( pIface ){` |
|        16 | 10069 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|        16 | 10070 | `									pDeepest = pIface;` |
|         6 | 10071 | `								}` |
|        16 | 10072 | `								pIface = pIface->pBase;` |
|         4 | 10073 | `							}` |
|        16 | 10074 | `							if( pDeepest ){` |
|        16 | 10075 | `								pOrigin = pDeepest;` |
|        16 | 10076 | `								break;` |
|         - | 10077 | `							}` |
|       ! 0 | 10078 | `						}` |
|        26 | 10079 | `						pWalk = pWalk->pBase;` |
|         4 | 10080 | `					}` |
|         7 | 10081 | `				}` |
|         - | 10082 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|        20 | 10083 | `				if( !pOrigin ){` |
|         3 | 10084 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|         3 | 10085 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|         3 | 10086 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|         3 | 10087 | `							pOrigin = pClass;` |
|         3 | 10088 | `							break;` |
|         - | 10089 | `						}` |
|       ! 0 | 10090 | `					}` |
|         1 | 10091 | `				}` |
|         - | 10092 | `			}` |
|        20 | 10093 | `			if( pOrigin ){` |
|        20 | 10094 | `				SyBlobFormat(&sMsg,"%z::",&pOrigin->sName);` |
|        12 | 10095 | `			}else{` |
|         - | 10096 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|       ! 0 | 10097 | `				SyBlobFormat(&sMsg,"%z::",&pClass->sName);` |
|         - | 10098 | `			}` |
|        20 | 10099 | `			GenStateAppendAbstractMemberName(&sMsg,pMName);` |
|        20 | 10100 | `			nListed++;` |
|         4 | 10101 | `		}` |
|         - | 10102 | `	}` |
|        18 | 10103 | `	SyBlobAppend(&sMsg,")",1);` |
|        25 | 10104 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|        14 | 10105 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|        18 | 10106 | `	SyBlobRelease(&sMsg);` |
|        18 | 10107 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 10108 | `		return SXERR_ABORT;` |
|         - | 10109 | `	}` |
|        18 | 10110 | `	return SXRET_OK;` |
|    175485 | 10111 | `}` |
|         - | 10112 | `/*` |
|         - | 10113 | ` * Parse a class/interface name reference from the current token stream.` |
|         - | 10114 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|         - | 10115 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|         - | 10116 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|         - | 10117 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|         - | 10118 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|         - | 10119 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|         - | 10120 | ` */` |
|    396812 | 10121 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|         5 | 10122 | `{` |
|    396817 | 10123 | `	int isAbsolute = 0;` |
|    396817 | 10124 | `	SyToken *pStart = pGen->pIn;` |
|         - | 10125 | `	SyBlob sName;` |
|    396817 | 10126 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      4385 | 10127 | `		isAbsolute = 1;` |
|      4385 | 10128 | `		pGen->pIn++;` |
|      2190 | 10129 | `	}` |
|    396817 | 10130 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         8 | 10131 | `		pGen->pIn = pStart;` |
|         8 | 10132 | `		return SXERR_INVALID;` |
|         - | 10133 | `	}` |
|    396811 | 10134 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|    396811 | 10135 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|    396811 | 10136 | `	pGen->pIn++;` |
|    595230 | 10137 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|    198429 | 10138 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        16 | 10139 | `		SyBlobAppend(&sName,"\\",1);` |
|        16 | 10140 | `		pGen->pIn++;` |
|        16 | 10141 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        16 | 10142 | `		pGen->pIn++;` |
|         2 | 10143 | `	}` |
|    396811 | 10144 | `	if( isAbsolute ){` |
|      4383 | 10145 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      2194 | 10146 | `	}else{` |
|         - | 10147 | `		SyString sRaw;` |
|    392433 | 10148 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|    392433 | 10149 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|         - | 10150 | `	}` |
|    396811 | 10151 | `	SyBlobRelease(&sName);` |
|    396811 | 10152 | `	return SXRET_OK;` |
|    198411 | 10153 | `}` |
|         - | 10154 | `/*` |
|         - | 10155 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|         - | 10156 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|         - | 10157 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|         - | 10158 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|         - | 10159 | ` * either direction cannot run unbounded.` |
|         - | 10160 | ` */` |
|         - | 10161 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    163428 | 10162 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|         5 | 10163 | `{` |
|         - | 10164 | `	ph7_class **apParent;` |
|         - | 10165 | `	sxu32 n;` |
|    425595 | 10166 | `	while( pInterface ){` |
|    269769 | 10167 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|       ! 0 | 10168 | `			return FALSE;` |
|         - | 10169 | `		}` |
|    303952 | 10170 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|     68366 | 10171 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|      7607 | 10172 | `			return TRUE;` |
|         - | 10173 | `		}` |
|    262167 | 10174 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    262167 | 10175 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|       ! 0 | 10176 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|       ! 0 | 10177 | `				return TRUE;` |
|         - | 10178 | `			}` |
|       ! 0 | 10179 | `		}` |
|    262167 | 10180 | `		pInterface = pInterface->pBase;` |
|    262167 | 10181 | `		iDepth++;` |
|         5 | 10182 | `	}` |
|    155831 | 10183 | `	return FALSE;` |
|     81719 | 10184 | `}` |
|    163428 | 10185 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|         5 | 10186 | `{` |
|    163433 | 10187 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|         5 | 10188 | `}` |
|         - | 10189 | `/*` |
|         - | 10190 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|         - | 10191 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|         - | 10192 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|         - | 10193 | ` */` |
|      7602 | 10194 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|         5 | 10195 | `{` |
|      7611 | 10196 | `	while( pBase ){` |
|        10 | 10197 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|         2 | 10198 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|         3 | 10199 | `			return TRUE;` |
|         - | 10200 | `		}` |
|        10 | 10201 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|         6 | 10202 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|         3 | 10203 | `			return TRUE;` |
|         - | 10204 | `		}` |
|         5 | 10205 | `		pBase = pBase->pBase;` |
|         1 | 10206 | `	}` |
|      7603 | 10207 | `	return FALSE;` |
|      3806 | 10208 | `}` |
|         - | 10209 | `/*` |
|         - | 10210 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|         - | 10211 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|         - | 10212 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|         - | 10213 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|         - | 10214 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|         - | 10215 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|         - | 10216 | ` * pClass->aEnumCases for cases().` |
|         - | 10217 | ` */` |
|      7634 | 10218 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10219 | `{` |
|      7639 | 10220 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 10221 | `	SySet *pInstrContainer;` |
|         - | 10222 | `	ph7_class_attr *pCase;` |
|         - | 10223 | `	SyString *pName;` |
|         - | 10224 | `	sxi32 rc;` |
|      7639 | 10225 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|      7639 | 10226 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 10227 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10228 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|       ! 0 | 10229 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10230 | `			return SXERR_ABORT;` |
|         - | 10231 | `		}` |
|       ! 0 | 10232 | `		goto Synchronize;` |
|         - | 10233 | `	}` |
|      7639 | 10234 | `	pName = &pGen->pIn->sData;` |
|         - | 10235 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|      7639 | 10236 | `	if( SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte) != 0 ){` |
|       ! 0 | 10237 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10238 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|       ! 0 | 10239 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10240 | `			return SXERR_ABORT;` |
|         - | 10241 | `		}` |
|       ! 0 | 10242 | `		goto Synchronize;` |
|         - | 10243 | `	}` |
|      7639 | 10244 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10245 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|      7639 | 10246 | `	if( pCase == 0 ){` |
|       ! 0 | 10247 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10248 | `		return SXERR_ABORT;` |
|         - | 10249 | `	}` |
|      7639 | 10250 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|      7639 | 10251 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 10252 | `		return SXERR_ABORT;` |
|         - | 10253 | `	}` |
|      7639 | 10254 | `	pGen->pIn++; /* Jump the case name */` |
|      7639 | 10255 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|      7625 | 10256 | `		if( pClass->nEnumBacking == 0 ){` |
|         8 | 10257 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         2 | 10258 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|         6 | 10259 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10260 | `				return SXERR_ABORT;` |
|         - | 10261 | `			}` |
|         6 | 10262 | `			goto Synchronize;` |
|         - | 10263 | `		}` |
|      7621 | 10264 | `		pGen->pIn++; /* Jump the equal sign */` |
|         - | 10265 | `		/* Compile the backing value expression into the case's own container` |
|         - | 10266 | `		 * (same technique as class constants). */` |
|      7621 | 10267 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7621 | 10268 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|      7621 | 10269 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7621 | 10270 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 | 10271 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10272 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|       ! 0 | 10273 | `		}` |
|      7621 | 10274 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7621 | 10275 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7621 | 10276 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10277 | `			return SXERR_ABORT;` |
|         - | 10278 | `		}` |
|      3813 | 10279 | `	}else{` |
|        17 | 10280 | `		if( pClass->nEnumBacking != 0 ){` |
|       ! 0 | 10281 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10282 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|       ! 0 | 10283 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10284 | `				return SXERR_ABORT;` |
|         - | 10285 | `			}` |
|       ! 0 | 10286 | `			goto Synchronize;` |
|         - | 10287 | `		}` |
|         - | 10288 | `	}` |
|      7635 | 10289 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|      7635 | 10290 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 10291 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10292 | `		return SXERR_ABORT;` |
|         - | 10293 | `	}` |
|      7635 | 10294 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|      7635 | 10295 | `	return SXRET_OK;` |
|         2 | 10296 | `Synchronize:` |
|         - | 10297 | `	/* Synchronize with the first semi-colon */` |
|        14 | 10298 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|        10 | 10299 | `		pGen->pIn++;` |
|         2 | 10300 | `	}` |
|         6 | 10301 | `	return SXERR_CORRUPT;` |
|      3822 | 10302 | `}` |
|         - | 10303 | `/*` |
|         - | 10304 | ` * Synthesize the enum interface methods (PHP 8.1): cases() for every enum,` |
|         - | 10305 | ` * plus from()/tryFrom() for backed enums. Each is an ordinary public static` |
|         - | 10306 | ` * method whose body forwards to a __phl_enum_* engine thunk (vm.c) with the` |
|         - | 10307 | ` * enum's FQN embedded as a literal — the same forwarder pattern the` |
|         - | 10308 | ` * Generator/Fiber/Reflection builtins use. The source buffer is owned by the` |
|         - | 10309 | ` * VM allocator and never freed: tokens (method and parameter names) keep` |
|         - | 10310 | ` * pointers into it (see the constructor-promotion precedent above).` |
|         - | 10311 | ` */` |
|      3820 | 10312 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10313 | `{` |
|         - | 10314 | `	SyToken *pSaveIn,*pSaveEnd;` |
|         - | 10315 | `	const char *zBack;` |
|         - | 10316 | `	SySet sToken;` |
|         - | 10317 | `	char *zSrc;` |
|         - | 10318 | `	sxu32 nSrc,nMax;` |
|      3825 | 10319 | `	sxi32 rc = SXRET_OK;` |
|      3825 | 10320 | `	nMax = 3*(sxu32)sizeof("function tryFrom(string $value){return __phl_enum_tryfrom('',$value);}")` |
|      3820 | 10321 | `		+ 3*SyStringLength(&pClass->sName) + 64;` |
|      3825 | 10322 | `	zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nMax);` |
|      3825 | 10323 | `	if( zSrc == 0 ){` |
|       ! 0 | 10324 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10325 | `		return SXERR_ABORT;` |
|         - | 10326 | `	}` |
|      3825 | 10327 | `	zBack = (pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string";` |
|      3825 | 10328 | `	if( pClass->nEnumBacking != 0 ){` |
|      5717 | 10329 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         - | 10330 | `			"function cases(){return __phl_enum_cases('%z');}"` |
|         - | 10331 | `			"function from(%s $value){return __phl_enum_from('%z',$value);}"` |
|         - | 10332 | `			"function tryFrom(%s $value){return __phl_enum_tryfrom('%z',$value);}",` |
|      1904 | 10333 | `			&pClass->sName,zBack,&pClass->sName,zBack,&pClass->sName);` |
|      1909 | 10334 | `	}else{` |
|        21 | 10335 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         6 | 10336 | `			"function cases(){return __phl_enum_cases('%z');}",&pClass->sName);` |
|         - | 10337 | `	}` |
|      3825 | 10338 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      3825 | 10339 | `	PH7_TokenizePHP(zSrc,nSrc,pClass->nLine,&sToken,0);` |
|      3825 | 10340 | `	pSaveIn = pGen->pIn;` |
|      3825 | 10341 | `	pSaveEnd = pGen->pEnd;` |
|      3825 | 10342 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      3825 | 10343 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|     15261 | 10344 | `	while( pGen->pIn < pGen->pEnd && rc != SXERR_ABORT ){` |
|     11441 | 10345 | `		rc = GenStateCompileClassMethod(&(*pGen),PH7_TKWRD_PUBLIC,PH7_CLASS_ATTR_STATIC,TRUE,pClass);` |
|         5 | 10346 | `	}` |
|      3825 | 10347 | `	pGen->pIn = pSaveIn;` |
|      3825 | 10348 | `	pGen->pEnd = pSaveEnd;` |
|      3825 | 10349 | `	SySetRelease(&sToken);` |
|      3825 | 10350 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXRET_OK;` |
|      1915 | 10351 | `}` |
|         - | 10352 | `/*` |
|         - | 10353 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|         - | 10354 | ` * __call/__callStatic/__invoke stay allowed).` |
|         - | 10355 | ` */` |
|         - | 10356 | `static const char *azEnumBannedMagic[] = {` |
|         - | 10357 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|         - | 10358 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|         - | 10359 | `};` |
|         - | 10360 | `/*` |
|         - | 10361 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|         - | 10362 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|         - | 10363 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|         - | 10364 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|         - | 10365 | ` * and before the class is installed.` |
|         - | 10366 | ` */` |
|      3820 | 10367 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|         5 | 10368 | `{` |
|         - | 10369 | `	SyHashEntry *pEntry;` |
|         - | 10370 | `	sxi32 rc;` |
|         - | 10371 | `	sxu32 n;` |
|         - | 10372 | `	/* php: "Enum %s cannot include properties" */` |
|      3825 | 10373 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     11459 | 10374 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      7641 | 10375 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      7641 | 10376 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|         3 | 10377 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|         1 | 10378 | `				"Enum %z cannot include properties",&pClass->sName);` |
|         3 | 10379 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10380 | `				return SXERR_ABORT;` |
|         - | 10381 | `			}` |
|         3 | 10382 | `			break;` |
|         - | 10383 | `		}` |
|         5 | 10384 | `	}` |
|         - | 10385 | `	/* php: "Enum %s cannot include magic method %s" */` |
|     53485 | 10386 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|     74490 | 10387 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|     49665 | 10388 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|       ! 0 | 10389 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10390 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|       ! 0 | 10391 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10392 | `				return SXERR_ABORT;` |
|         - | 10393 | `			}` |
|       ! 0 | 10394 | `		}` |
|     24835 | 10395 | `	}` |
|         - | 10396 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|         - | 10397 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|         - | 10398 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|         - | 10399 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|         - | 10400 | `	{` |
|         - | 10401 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|         - | 10402 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|         - | 10403 | `		ph7_class_attr *pAttr;` |
|      3825 | 10404 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10405 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      3825 | 10406 | `		if( pAttr == 0 ){` |
|       ! 0 | 10407 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10408 | `			return SXERR_ABORT;` |
|         - | 10409 | `		}` |
|      3825 | 10410 | `		pAttr->nType = MEMOBJ_STRING;` |
|      3825 | 10411 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|      3825 | 10412 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|      3825 | 10413 | `		if( pClass->nEnumBacking != 0 ){` |
|      3813 | 10414 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10415 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      3813 | 10416 | `			if( pAttr == 0 ){` |
|       ! 0 | 10417 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10418 | `				return SXERR_ABORT;` |
|         - | 10419 | `			}` |
|      3813 | 10420 | `			pAttr->nType = pClass->nEnumBacking;` |
|      3813 | 10421 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|         7 | 10422 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|         4 | 10423 | `			}else{` |
|      3807 | 10424 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|         - | 10425 | `			}` |
|      3813 | 10426 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|      1904 | 10427 | `		}` |
|         - | 10428 | `	}` |
|      3825 | 10429 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|      1915 | 10430 | `}` |
|         - | 10431 | `/*` |
|         - | 10432 | ` * Compile a class declaration, named or anonymous.` |
|         - | 10433 | ` *` |
|         - | 10434 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|         - | 10435 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|         - | 10436 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|         - | 10437 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|         - | 10438 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|         - | 10439 | ` * implements, body, install) is shared by both paths.` |
|         - | 10440 | ` */` |
|    351004 | 10441 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|         - | 10442 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|         5 | 10443 | `{` |
|    351009 | 10444 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 10445 | `	ph7_class *pClass,*pBase;` |
|         - | 10446 | `	SyToken *pEnd,*pTmp;` |
|         - | 10447 | `	sxi32 iProtection;` |
|         - | 10448 | `	SySet aInterfaces;` |
|         - | 10449 | `	SySet aUseEntries;` |
|         - | 10450 | `	sxi32 iAttrflags;` |
|         - | 10451 | `	SyString *pName;` |
|         - | 10452 | `	sxi32 nKwrd;` |
|         - | 10453 | `	sxi32 rc;` |
|         - | 10454 | `	/* Jump the 'class' keyword */` |
|    351009 | 10455 | `	pGen->pIn++;` |
|    351009 | 10456 | `	if( pAnonName ){` |
|         - | 10457 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|         - | 10458 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|         - | 10459 | `		 * then use the synthesized name. */` |
|        32 | 10460 | `		*ppArgStart = *ppArgEnd = 0;` |
|        32 | 10461 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         7 | 10462 | `			pGen->pIn++; /* Jump '(' */` |
|         7 | 10463 | `			*ppArgStart = pGen->pIn;` |
|        10 | 10464 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|         3 | 10465 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|         7 | 10466 | `			pGen->pIn = *ppArgEnd;` |
|         7 | 10467 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|         3 | 10468 | `		}` |
|        32 | 10469 | `		pName = pAnonName;` |
|        32 | 10470 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|        18 | 10471 | `	}else{` |
|    350981 | 10472 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - | 10473 | `			/* Syntax error */` |
|       ! 0 | 10474 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|       ! 0 | 10475 | `			if( rc == SXERR_ABORT ){` |
|         - | 10476 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 10477 | `				return SXERR_ABORT;` |
|         - | 10478 | `			}` |
|         - | 10479 | `			/* Synchronize with the first semi-colon or curly braces */` |
|       ! 0 | 10480 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|       ! 0 | 10481 | `				pGen->pIn++;` |
|       ! 0 | 10482 | `			}` |
|       ! 0 | 10483 | `			return SXRET_OK;` |
|         - | 10484 | `		}` |
|         - | 10485 | `		/* Extract class name */` |
|    350981 | 10486 | `		pName = &pGen->pIn->sData;` |
|         - | 10487 | `		/* Advance the stream cursor */` |
|    350981 | 10488 | `		pGen->pIn++;` |
|         - | 10489 | `		/* Build FQN and obtain a raw class */ {` |
|         - | 10490 | `			SyBlob sFQN;` |
|         - | 10491 | `			SyString sFQNStr;` |
|    350981 | 10492 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    350981 | 10493 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|    350981 | 10494 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    350981 | 10495 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    350981 | 10496 | `			SyBlobRelease(&sFQN);` |
|         - | 10497 | `		}` |
|         - | 10498 | `	}` |
|    351009 | 10499 | `	if( pClass == 0 ){` |
|       ! 0 | 10500 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10501 | `		return SXERR_ABORT;` |
|         - | 10502 | `	}` |
|    351004 | 10503 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|      3829 | 10504 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|         - | 10505 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|      3815 | 10506 | `		pGen->pIn++; /* Jump ':' */` |
|      3810 | 10507 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      3815 | 10508 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|         7 | 10509 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|         7 | 10510 | `			pGen->pIn++;` |
|      3808 | 10511 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      3809 | 10512 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|      3807 | 10513 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|      3807 | 10514 | `			pGen->pIn++;` |
|      1906 | 10515 | `		}else{` |
|         3 | 10516 | `			SyToken *pTok = pGen->pIn;` |
|         3 | 10517 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|         4 | 10518 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|         1 | 10519 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|         3 | 10520 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10521 | `				return SXERR_ABORT;` |
|         - | 10522 | `			}` |
|         3 | 10523 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|         3 | 10524 | `				pGen->pIn++; /* Skip the bogus type token */` |
|         1 | 10525 | `			}` |
|         - | 10526 | `		}` |
|      1905 | 10527 | `	}` |
|    351009 | 10528 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    351009 | 10529 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 10530 | `		return SXERR_ABORT;` |
|         - | 10531 | `	}` |
|         - | 10532 | `	/* implemented interfaces and per-use-statement trait containers */` |
|    351009 | 10533 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|    351009 | 10534 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|         - | 10535 | `	/* Assume a standalone class */` |
|    351009 | 10536 | `	pBase = 0;` |
|    351009 | 10537 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    285135 | 10538 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    285135 | 10539 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|         - | 10540 | `			SyBlob sResolved;` |
|         - | 10541 | `			SyString sBaseName;` |
|         - | 10542 | `			sxu32 nRefLine;` |
|    182467 | 10543 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|         - | 10544 | `				/* php parse-fatals here (enums have no inheritance) */` |
|       ! 0 | 10545 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10546 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|       ! 0 | 10547 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10548 | `					return SXERR_ABORT;` |
|         - | 10549 | `				}` |
|       ! 0 | 10550 | `			}` |
|    182467 | 10551 | `			pGen->pIn++; /* Advance past 'extends' */` |
|    182467 | 10552 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    182467 | 10553 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    182467 | 10554 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         3 | 10555 | `				SyBlobRelease(&sResolved);` |
|         4 | 10556 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 10557 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|         1 | 10558 | `					pName);` |
|         3 | 10559 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|         3 | 10560 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10561 | `					return SXERR_ABORT;` |
|         - | 10562 | `				}` |
|         3 | 10563 | `				return SXRET_OK;` |
|         - | 10564 | `			}` |
|    273695 | 10565 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    182460 | 10566 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    182465 | 10567 | `			SyStringInitFromBuf(&sBaseName,` |
|         - | 10568 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 10569 | `			/* Interfaces are not allowed */` |
|    182465 | 10570 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|       ! 0 | 10571 | `				pBase = pBase->pNextName;` |
|       ! 0 | 10572 | `			}` |
|    182465 | 10573 | `			if( pBase == 0 ){` |
|       ! 0 | 10574 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 10575 | `					"Nonexistent base class '%z'",&sBaseName);` |
|       ! 0 | 10576 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10577 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 10578 | `					return SXERR_ABORT;` |
|         - | 10579 | `				}` |
|       ! 0 | 10580 | `			}else{` |
|    182465 | 10581 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|         4 | 10582 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 | 10583 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|         3 | 10584 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10585 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10586 | `						return SXERR_ABORT;` |
|         - | 10587 | `					}` |
|         3 | 10588 | `					pBase = 0; /* Never inherit from an enum */` |
|    182464 | 10589 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|       ! 0 | 10590 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10591 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|       ! 0 | 10592 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10593 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10594 | `						return SXERR_ABORT;` |
|         - | 10595 | `					}` |
|       ! 0 | 10596 | `				}` |
|         - | 10597 | `			}` |
|    182465 | 10598 | `			SyBlobRelease(&sResolved);` |
|    182465 | 10599 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|       ! 0 | 10600 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|       ! 0 | 10601 | `			}` |
|     91230 | 10602 | `		}` |
|    285133 | 10603 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|         - | 10604 | `			ph7_class *pInterface;` |
|         - | 10605 | `			/* Interface implementation */` |
|    106481 | 10606 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    110190 | 10607 | `			for(;;){` |
|         - | 10608 | `				SyBlob sResolved;` |
|         - | 10609 | `				SyString sIntName;` |
|         - | 10610 | `				sxu32 nRefLine;` |
|    163433 | 10611 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    163433 | 10612 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    163433 | 10613 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 10614 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 10615 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 10616 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|       ! 0 | 10617 | `						pName);` |
|       ! 0 | 10618 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10619 | `						return SXERR_ABORT;` |
|         - | 10620 | `					}` |
|       ! 0 | 10621 | `					break;` |
|         - | 10622 | `				}` |
|    326861 | 10623 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    163428 | 10624 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    163433 | 10625 | `				SyStringInitFromBuf(&sIntName,` |
|         - | 10626 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 10627 | `				/* Only interfaces are allowed */` |
|    163433 | 10628 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 10629 | `					pInterface = pInterface->pNextName;` |
|       ! 0 | 10630 | `				}` |
|    163433 | 10631 | `				if( pInterface == 0 ){` |
|       ! 0 | 10632 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 10633 | `						"Nonexistent base interface '%z'",&sIntName);` |
|       ! 0 | 10634 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10635 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10636 | `						return SXERR_ABORT;` |
|         - | 10637 | `					}` |
|       ! 0 | 10638 | `				}else{` |
|         - | 10639 | `					/* Reject user classes that try to implement Throwable` |
|         - | 10640 | `					 * directly (or via an interface that extends Throwable)` |
|         - | 10641 | `					 * unless they already extend Exception or Error.` |
|         - | 10642 | `					 * Exception and Error themselves are compiled from the` |
|         - | 10643 | `					 * built-in library and are exempt by FQN — a namespaced` |
|         - | 10644 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    163433 | 10645 | `					SyString *pFqn = &pClass->sName;` |
|    163433 | 10646 | `					int bIsExceptionOrError =` |
|     85514 | 10647 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|    247044 | 10648 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    161537 | 10649 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|      3810 | 10650 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|    167229 | 10651 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|     11406 | 10652 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|      3799 | 10653 | `						!bIsExceptionOrError ){` |
|        12 | 10654 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10655 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|         3 | 10656 | `							&pClass->sName);` |
|         9 | 10657 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10658 | `							SyBlobRelease(&sResolved);` |
|       ! 0 | 10659 | `							return SXERR_ABORT;` |
|         - | 10660 | `						}` |
|         - | 10661 | `						/* Skip registration so the follow-up abstract-method` |
|         - | 10662 | `						 * check does not produce a duplicate fatal. */` |
|         6 | 10663 | `					}else{` |
|    163427 | 10664 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|         - | 10665 | `					}` |
|         - | 10666 | `				}` |
|    163433 | 10667 | `				SyBlobRelease(&sResolved);` |
|    163433 | 10668 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     53243 | 10669 | `					break;` |
|         - | 10670 | `				}` |
|     56957 | 10671 | `				pGen->pIn++;/* Jump the comma */` |
|         5 | 10672 | `			}` |
|     53238 | 10673 | `		}` |
|    142564 | 10674 | `	}` |
|    351007 | 10675 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - | 10676 | `		/* Syntax error */` |
|       ! 0 | 10677 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|       ! 0 | 10678 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 10679 | `		if( rc == SXERR_ABORT ){` |
|         - | 10680 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 10681 | `			return SXERR_ABORT;` |
|         - | 10682 | `		}` |
|       ! 0 | 10683 | `		return SXRET_OK;` |
|         - | 10684 | `	}` |
|    351007 | 10685 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    351007 | 10686 | `	pEnd = 0; /* cc warning */` |
|         - | 10687 | `	/* Delimit the class body */` |
|    351007 | 10688 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    351007 | 10689 | `	if( pEnd >= pGen->pEnd ){` |
|         - | 10690 | `		/* Syntax error */` |
|       ! 0 | 10691 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|       ! 0 | 10692 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 10693 | `		if( rc == SXERR_ABORT ){` |
|         - | 10694 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 10695 | `			return SXERR_ABORT;` |
|         - | 10696 | `		}` |
|       ! 0 | 10697 | `		return SXRET_OK;` |
|         - | 10698 | `	}` |
|         - | 10699 | `	/* The delimiter token is the class body's closing brace */` |
|    351007 | 10700 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 10701 | `	/* Swap token stream */` |
|    351007 | 10702 | `	pTmp = pGen->pEnd;` |
|    351007 | 10703 | `	pGen->pEnd = pEnd;` |
|         - | 10704 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|    351007 | 10705 | `	pClass->iFlags \|= iFlags;` |
|         - | 10706 | `	/* Start the parse process */` |
|   1358741 | 10707 | `	for(;;){` |
|         - | 10708 | `		/* Jump leading/trailing semi-colons */` |
|   3873627 | 10709 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    699577 | 10710 | `			pGen->pIn++;` |
|         5 | 10711 | `		}` |
|   3174055 | 10712 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 10713 | `			/* End of class body */` |
|    350965 | 10714 | `			break;` |
|         - | 10715 | `		}` |
|         - | 10716 | `		/* Bind a directly-preceding docblock to this member */` |
|   2823095 | 10717 | `		GenStateSetPendingDoc(&(*pGen));` |
|   2823090 | 10718 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|   1411550 | 10719 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|       ! 0 | 10720 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10721 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 10722 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 10723 | `			if( rc == SXERR_ABORT ){` |
|         - | 10724 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 10725 | `				return SXERR_ABORT;` |
|         - | 10726 | `			}` |
|       ! 0 | 10727 | `			goto done;` |
|         - | 10728 | `		}` |
|         - | 10729 | `		/* Assume public visibility */` |
|   2823095 | 10730 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|   2823095 | 10731 | `		iAttrflags = 0;` |
|         - | 10732 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|         - | 10733 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|         - | 10734 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|         - | 10735 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|   2823095 | 10736 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 10737 | `			int bMod = 0;` |
|       ! 0 | 10738 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 10739 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|         - | 10740 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|         - | 10741 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|         - | 10742 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|         - | 10743 | `			 * that the generic keyword dispatch would misread as a method. */` |
|       ! 0 | 10744 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       ! 0 | 10745 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       ! 0 | 10746 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|       ! 0 | 10747 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|       ! 0 | 10748 | `			}` |
|       ! 0 | 10749 | `			if( !bMod ){` |
|       ! 0 | 10750 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 10751 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 10752 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10753 | `						return SXERR_ABORT;` |
|         - | 10754 | `					}` |
|       ! 0 | 10755 | `					goto done;` |
|         - | 10756 | `				}` |
|       ! 0 | 10757 | `				continue;` |
|         - | 10758 | `			}` |
|       ! 0 | 10759 | `		}` |
|   2823095 | 10760 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 10761 | `			/* Extract the current keyword */` |
|   2823095 | 10762 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   2823095 | 10763 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|         - | 10764 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|      7639 | 10765 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|      7639 | 10766 | `				if( rc != SXRET_OK ){` |
|         6 | 10767 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10768 | `						return SXERR_ABORT;` |
|         - | 10769 | `					}` |
|         6 | 10770 | `					goto done;` |
|         - | 10771 | `				}` |
|      7635 | 10772 | `				continue;` |
|         - | 10773 | `			}` |
|   2815461 | 10774 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 10775 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|         - | 10776 | `				TraitUseEntry sUse;` |
|     15253 | 10777 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|     15253 | 10778 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|     15253 | 10779 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      7632 | 10780 | `				for(;;){` |
|         - | 10781 | `					ph7_class *pTrait;` |
|         - | 10782 | `					SyString *pTraitName;` |
|     15261 | 10783 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 10784 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10785 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|       ! 0 | 10786 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10787 | `							return SXERR_ABORT;` |
|         - | 10788 | `						}` |
|       ! 0 | 10789 | `						break;` |
|         - | 10790 | `					}` |
|     15261 | 10791 | `					pTraitName = &pGen->pIn->sData;` |
|         - | 10792 | `					/* Resolve trait name through namespace/imports */ {` |
|         - | 10793 | `						SyBlob sResolved;` |
|     15261 | 10794 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     15261 | 10795 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     30517 | 10796 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|     15256 | 10797 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     15261 | 10798 | `						SyBlobRelease(&sResolved);` |
|         - | 10799 | `					}` |
|         - | 10800 | `					/* Only traits are allowed */` |
|     15261 | 10801 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 10802 | `						pTrait = pTrait->pNextName;` |
|       ! 0 | 10803 | `					}` |
|     15261 | 10804 | `					if( pTrait == 0 ){` |
|       ! 0 | 10805 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10806 | `							"'%z' is not a trait",pTraitName);` |
|       ! 0 | 10807 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10808 | `							return SXERR_ABORT;` |
|         - | 10809 | `						}` |
|       ! 0 | 10810 | `					}else{` |
|     15261 | 10811 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|         - | 10812 | `					}` |
|     15261 | 10813 | `					pGen->pIn++; /* Advance past trait name */` |
|     15261 | 10814 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      7629 | 10815 | `						break;` |
|         - | 10816 | `					}` |
|        10 | 10817 | `					pGen->pIn++; /* Jump the comma */` |
|         2 | 10818 | `				}` |
|         - | 10819 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|     15253 | 10820 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - | 10821 | `					SyToken *pBlock;` |
|        13 | 10822 | `					pGen->pIn++; /* Jump '{' */` |
|        13 | 10823 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|        13 | 10824 | `					sUse.pResolvStart = pGen->pIn;` |
|        13 | 10825 | `					sUse.pResolvEnd = pBlock;` |
|        13 | 10826 | `					if( pBlock < pGen->pEnd ){` |
|        13 | 10827 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|         8 | 10828 | `					}else{` |
|       ! 0 | 10829 | `						pGen->pIn = pGen->pEnd;` |
|         - | 10830 | `					}` |
|         5 | 10831 | `				}` |
|     15253 | 10832 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|         - | 10833 | `				/* The semicolon will be consumed by the outer loop */` |
|     15253 | 10834 | `				continue;` |
|         - | 10835 | `			}` |
|   2800213 | 10836 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - | 10837 | `				int nSetTok;` |
|   2556777 | 10838 | `				sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   2556777 | 10839 | `				if( nSetVis ){` |
|         - | 10840 | ``					/* Leading `private(set)`/`protected(set)` with no read`` |
|         - | 10841 | `					 * visibility: the read side defaults to public (php 8.4). */` |
|         3 | 10842 | `					iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 10843 | `					pGen->pIn += nSetTok;` |
|         2 | 10844 | `				}else{` |
|   2556775 | 10845 | `					iProtection = nKwrd;` |
|   2556775 | 10846 | `					pGen->pIn++; /* Jump the visibility token */` |
|         - | 10847 | `					/* Optional asymmetric set-visibility after the read` |
|         - | 10848 | ``					 * visibility: `public private(set) int $x`. */`` |
|   2556775 | 10849 | `					nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   2556775 | 10850 | `					if( nSetVis ){` |
|         9 | 10851 | `						iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         9 | 10852 | `						pGen->pIn += nSetTok;` |
|         4 | 10853 | `					}` |
|         - | 10854 | `				}` |
|         - | 10855 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`,`` |
|         - | 10856 | ``				 * `public private(set) readonly int $x`. */`` |
|   2556777 | 10857 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|        24 | 10858 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|        24 | 10859 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        10 | 10860 | `				}` |
|   2556772 | 10861 | `				if( pGen->pIn >= pGen->pEnd` |
|   2556777 | 10862 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 10863 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10864 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 10865 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 10866 | `					if( rc == SXERR_ABORT ){` |
|         - | 10867 | `						/* Error count limit reached,abort immediately */` |
|       ! 0 | 10868 | `						return SXERR_ABORT;` |
|         - | 10869 | `					}` |
|       ! 0 | 10870 | `					goto done;` |
|         - | 10871 | `				}` |
|   2556777 | 10872 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 10873 | `					/* Attribute declaration (untyped) */` |
|    406787 | 10874 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    406787 | 10875 | `					if( rc != SXRET_OK ){` |
|        11 | 10876 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10877 | `							return SXERR_ABORT;` |
|         - | 10878 | `						}` |
|        11 | 10879 | `						goto done;` |
|         - | 10880 | `					}` |
|    406923 | 10881 | `					continue;` |
|         - | 10882 | `				}` |
|   2149995 | 10883 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 10884 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|       299 | 10885 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       299 | 10886 | `					if( rc != SXRET_OK ){` |
|         8 | 10887 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10888 | `							return SXERR_ABORT;` |
|         - | 10889 | `						}` |
|         8 | 10890 | `						goto done;` |
|         - | 10891 | `					}` |
|       293 | 10892 | `					continue;` |
|         - | 10893 | `				}` |
|         - | 10894 | `				/* Extract the keyword */` |
|   2149701 | 10895 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1074848 | 10896 | `			}` |
|   2393137 | 10897 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|         - | 10898 | `				/* Process constant declaration */` |
|    235507 | 10899 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|    235507 | 10900 | `				if( rc != SXRET_OK ){` |
|        11 | 10901 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10902 | `						return SXERR_ABORT;` |
|         - | 10903 | `					}` |
|        11 | 10904 | `					goto done;` |
|         - | 10905 | `				}` |
|    117752 | 10906 | `			}else{` |
|   2157635 | 10907 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - | 10908 | `					/* Static method or attribute,record that */` |
|     95045 | 10909 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     95045 | 10910 | `					pGen->pIn++; /* Jump the static keyword */` |
|     95045 | 10911 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 10912 | `						int nSetTok;` |
|     68443 | 10913 | `						sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|     68443 | 10914 | `						if( nSetVis ){` |
|         - | 10915 | ``							/* `static private(set) int $x` — read side stays public */`` |
|         3 | 10916 | `							iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 10917 | `							pGen->pIn += nSetTok;` |
|         2 | 10918 | `						}else{` |
|         - | 10919 | `							/* Extract the keyword */` |
|     68441 | 10920 | `							nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     68441 | 10921 | `							if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 10922 | `								iProtection = nKwrd;` |
|       ! 0 | 10923 | `								pGen->pIn++; /* Jump the visibility token */` |
|       ! 0 | 10924 | `								nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|       ! 0 | 10925 | `								if( nSetVis ){` |
|       ! 0 | 10926 | `									iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|       ! 0 | 10927 | `									pGen->pIn += nSetTok;` |
|       ! 0 | 10928 | `								}` |
|       ! 0 | 10929 | `							}` |
|         - | 10930 | `						}` |
|     34219 | 10931 | `					}` |
|         - | 10932 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|         - | 10933 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|         - | 10934 | `					 * than a generic "expecting method" parse error. */` |
|     95045 | 10935 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 10936 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 10937 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       ! 0 | 10938 | `					}` |
|     95040 | 10939 | `					if( pGen->pIn >= pGen->pEnd` |
|     95045 | 10940 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 10941 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10942 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|       ! 0 | 10943 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 10944 | `						if( rc == SXERR_ABORT ){` |
|         - | 10945 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 10946 | `							return SXERR_ABORT;` |
|         - | 10947 | `						}` |
|       ! 0 | 10948 | `						goto done;` |
|         - | 10949 | `					}` |
|     95045 | 10950 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 10951 | `						/* Attribute declaration */` |
|     26605 | 10952 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     26605 | 10953 | `						if( rc != SXRET_OK ){` |
|         3 | 10954 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 10955 | `								return SXERR_ABORT;` |
|         - | 10956 | `							}` |
|         3 | 10957 | `							goto done;` |
|         - | 10958 | `						}` |
|     26603 | 10959 | `						continue;` |
|         - | 10960 | `					}` |
|     68445 | 10961 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 10962 | `						/* Typed static attribute declaration */` |
|        17 | 10963 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        17 | 10964 | `						if( rc != SXRET_OK ){` |
|         3 | 10965 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 10966 | `								return SXERR_ABORT;` |
|         - | 10967 | `							}` |
|         3 | 10968 | `							goto done;` |
|         - | 10969 | `						}` |
|        15 | 10970 | `						continue;` |
|         - | 10971 | `					}` |
|         - | 10972 | `					/* Extract the keyword */` |
|     68431 | 10973 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   2096808 | 10974 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         - | 10975 | `					/* Abstract method,record that */` |
|      7615 | 10976 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         - | 10977 | `					/* Mark the whole class as abstract */` |
|      7615 | 10978 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|         - | 10979 | `					/* Advance the stream cursor */` |
|      7615 | 10980 | `					pGen->pIn++;` |
|      7615 | 10981 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      7615 | 10982 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      7615 | 10983 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      7613 | 10984 | `							iProtection = nKwrd;` |
|      7613 | 10985 | `							pGen->pIn++; /* Jump the visibility token */` |
|      3804 | 10986 | `						}` |
|      3805 | 10987 | `					}` |
|      7615 | 10988 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      7610 | 10989 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 10990 | `							/* Static method */` |
|       ! 0 | 10991 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 10992 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 10993 | `					}` |
|      7615 | 10994 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      7610 | 10995 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|         - | 10996 | ``							/* PHP 8.4: `abstract public [T] $x { get; set; }` — an abstract`` |
|         - | 10997 | `							 * HOOKED property declaration. Route anything that is not a` |
|         - | 10998 | `							 * method through the attribute compiler with the ABSTRACT flag;` |
|         - | 10999 | ``							 * the hook parser accepts the bare `get;`/`set;` forms there`` |
|         - | 11000 | `							 * (and a non-hooked abstract property is ITS error to raise). */` |
|         6 | 11001 | `							if( pGen->pIn < pGen->pEnd` |
|         7 | 11002 | `							 && ((pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID\|PH7_TK_DOLLAR)) != 0` |
|         3 | 11003 | `							  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         7 | 11004 | `								rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         7 | 11005 | `								if( rc != SXRET_OK ){` |
|       ! 0 | 11006 | `									if( rc == SXERR_ABORT ){` |
|       ! 0 | 11007 | `										return SXERR_ABORT;` |
|         - | 11008 | `									}` |
|       ! 0 | 11009 | `									goto done;` |
|         - | 11010 | `								}` |
|         7 | 11011 | `								continue;` |
|         - | 11012 | `							}` |
|       ! 0 | 11013 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11014 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|       ! 0 | 11015 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 11016 | `							if( rc == SXERR_ABORT ){` |
|         - | 11017 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 11018 | `								return SXERR_ABORT;` |
|         - | 11019 | `							}` |
|       ! 0 | 11020 | `							goto done;` |
|         - | 11021 | `					}` |
|      7609 | 11022 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|   2058787 | 11023 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|         - | 11024 | `					/* final method ,record that */` |
|        21 | 11025 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|        21 | 11026 | `					pGen->pIn++; /* Jump the final keyword */` |
|        21 | 11027 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 11028 | `						/* Extract the keyword */` |
|        21 | 11029 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        21 | 11030 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        11 | 11031 | `							iProtection = nKwrd;` |
|        11 | 11032 | `							pGen->pIn++; /* Jump the visibility token */` |
|         4 | 11033 | `						}` |
|         9 | 11034 | `					}` |
|        21 | 11035 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        18 | 11036 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|         - | 11037 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|         - | 11038 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|         - | 11039 | `							 * child class is compiled (PH7_ClassInherit). */` |
|        14 | 11040 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|        14 | 11041 | `							if( rc != SXRET_OK ){` |
|       ! 0 | 11042 | `								if( rc == SXERR_ABORT ){` |
|       ! 0 | 11043 | `									return SXERR_ABORT;` |
|         - | 11044 | `								}` |
|       ! 0 | 11045 | `								goto done;` |
|         - | 11046 | `							}` |
|        14 | 11047 | `							continue;` |
|         - | 11048 | `					}` |
|         9 | 11049 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         6 | 11050 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 11051 | `							/* Static method */` |
|       ! 0 | 11052 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 11053 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 11054 | `					}` |
|         9 | 11055 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         6 | 11056 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 11057 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11058 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|       ! 0 | 11059 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 11060 | `							if( rc == SXERR_ABORT ){` |
|         - | 11061 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 11062 | `								return SXERR_ABORT;` |
|         - | 11063 | `							}` |
|       ! 0 | 11064 | `							goto done;` |
|         - | 11065 | `					}` |
|         9 | 11066 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         3 | 11067 | `				}` |
|   2131003 | 11068 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 11069 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11070 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|       ! 0 | 11071 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11072 | `						if( rc == SXERR_ABORT ){` |
|         - | 11073 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11074 | `							return SXERR_ABORT;` |
|         - | 11075 | `						}` |
|       ! 0 | 11076 | `						goto done;` |
|         - | 11077 | `				}` |
|   2131003 | 11078 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|         7 | 11079 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|         7 | 11080 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|       ! 0 | 11081 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11082 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 11083 | `						if( rc == SXERR_ABORT ){` |
|         - | 11084 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11085 | `							return SXERR_ABORT;` |
|         - | 11086 | `						}` |
|       ! 0 | 11087 | `						goto done;` |
|         - | 11088 | `					}` |
|         - | 11089 | `					/* Attribute declaration */` |
|         7 | 11090 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         4 | 11091 | `				}else{` |
|         - | 11092 | `					/* Process method declaration */` |
|   2130997 | 11093 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 11094 | `				}` |
|   2131003 | 11095 | `				if( rc != SXRET_OK ){` |
|        16 | 11096 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11097 | `						return SXERR_ABORT;` |
|         - | 11098 | `					}` |
|        16 | 11099 | `					goto done;` |
|         - | 11100 | `				}` |
|         - | 11101 | `			}` |
|   1183245 | 11102 | `		}else{` |
|         - | 11103 | `			/* Attribute declaration */` |
|       ! 0 | 11104 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11105 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11106 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11107 | `					return SXERR_ABORT;` |
|         - | 11108 | `				}` |
|       ! 0 | 11109 | `				goto done;` |
|         - | 11110 | `			}` |
|         - | 11111 | `		}` |
|         5 | 11112 | `	}` |
|         - | 11113 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|         - | 11114 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|         - | 11115 | `	 */` |
|         - | 11116 | `	{` |
|         - | 11117 | `		TraitUseEntry *apUse;` |
|         - | 11118 | `		sxu32 nU;` |
|    350965 | 11119 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|    366213 | 11120 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|     15253 | 11121 | `			TraitUseEntry *pUse = &apUse[nU];` |
|     15253 | 11122 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|     15253 | 11123 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|     15253 | 11124 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|         - | 11125 | `			sxu32 nT;` |
|     15253 | 11126 | `			if( !hasResolution ){` |
|         - | 11127 | `				/* No conflict resolution block: use standard trait application */` |
|     30487 | 11128 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|     15249 | 11129 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|     15249 | 11130 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11131 | `						break;` |
|         - | 11132 | `					}` |
|      7627 | 11133 | `				}` |
|      7624 | 11134 | `			}else{` |
|         - | 11135 | `				/* With resolution block: copy attributes, record traits,` |
|         - | 11136 | `				 * then use the block to resolve method conflicts.` |
|         - | 11137 | `				 */` |
|         - | 11138 | `				SyToken *pR;` |
|        25 | 11139 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        15 | 11140 | `					ph7_class *pTR = apTrait[nT];` |
|         - | 11141 | `					ph7_class_attr *pAR;` |
|         - | 11142 | `					SyHashEntry *pER;` |
|         - | 11143 | `					SyString *pNR;` |
|        15 | 11144 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|        21 | 11145 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|       ! 0 | 11146 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|       ! 0 | 11147 | `						pNR = &pAR->sName;` |
|       ! 0 | 11148 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       ! 0 | 11149 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|       ! 0 | 11150 | `						}` |
|       ! 0 | 11151 | `					}` |
|        15 | 11152 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|         9 | 11153 | `				}` |
|         - | 11154 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|        13 | 11155 | `				pR = pUse->pResolvStart;` |
|        27 | 11156 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 11157 | `					SyString sTrait,sMethod;` |
|         - | 11158 | `					ph7_class *pSrcTrait;` |
|         - | 11159 | `					ph7_class_method *pMeth;` |
|         - | 11160 | `					sxi32 nRKwrd;` |
|        41 | 11161 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        27 | 11162 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        17 | 11163 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        17 | 11164 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        17 | 11165 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        17 | 11166 | `					sMethod = pR->sData;` |
|        17 | 11167 | `					pR++;` |
|        17 | 11168 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|         7 | 11169 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|         7 | 11170 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|         7 | 11171 | `							sTrait = sMethod;` |
|         7 | 11172 | `							pR++;` |
|         7 | 11173 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|         7 | 11174 | `							sMethod = pR->sData;` |
|         7 | 11175 | `							pR++;` |
|         3 | 11176 | `						}` |
|         3 | 11177 | `					}` |
|        17 | 11178 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 11179 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 11180 | `						continue;` |
|         - | 11181 | `					}` |
|        17 | 11182 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        17 | 11183 | `					pR++;` |
|        17 | 11184 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|         5 | 11185 | `						pSrcTrait = 0;` |
|         7 | 11186 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         7 | 11187 | `							SyString *pTN = &apTrait[nT]->sName;` |
|        10 | 11188 | `							if( pTN->nByte >= sTrait.nByte &&` |
|         6 | 11189 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         5 | 11190 | `								pSrcTrait = apTrait[nT];` |
|         5 | 11191 | `								break;` |
|         - | 11192 | `							}` |
|         2 | 11193 | `						}` |
|         5 | 11194 | `						if( pSrcTrait ){` |
|         5 | 11195 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         5 | 11196 | `							if( pMeth ){` |
|         5 | 11197 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|         5 | 11198 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|         5 | 11199 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|         2 | 11200 | `								}` |
|         2 | 11201 | `							}` |
|         2 | 11202 | `						}` |
|         2 | 11203 | `					}` |
|        35 | 11204 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 11205 | `				}` |
|         - | 11206 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|        25 | 11207 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         - | 11208 | `					ph7_class_method *pMR;` |
|         - | 11209 | `					SyHashEntry *pER;` |
|         - | 11210 | `					SyString *pNR;` |
|        15 | 11211 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|        41 | 11212 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|        23 | 11213 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|        23 | 11214 | `						pNR = &pMR->sFunc.sName;` |
|        23 | 11215 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|        14 | 11216 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|         6 | 11217 | `						}` |
|         3 | 11218 | `					}` |
|         9 | 11219 | `				}` |
|         - | 11220 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|        13 | 11221 | `				pR = pUse->pResolvStart;` |
|        27 | 11222 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 11223 | `					SyString sTrait,sMethod,sAlias;` |
|         - | 11224 | `					ph7_class *pSrcTrait;` |
|         - | 11225 | `					ph7_class_method *pMeth;` |
|        27 | 11226 | `					int hasQual = 0;` |
|         - | 11227 | `					sxi32 nRKwrd;` |
|        41 | 11228 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        27 | 11229 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        17 | 11230 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        17 | 11231 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        17 | 11232 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|        17 | 11233 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        17 | 11234 | `					sMethod = pR->sData;` |
|        17 | 11235 | `					pR++;` |
|        17 | 11236 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|         7 | 11237 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|         7 | 11238 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|         7 | 11239 | `							sTrait = sMethod;` |
|         7 | 11240 | `							hasQual = 1;` |
|         7 | 11241 | `							pR++;` |
|         7 | 11242 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|         7 | 11243 | `							sMethod = pR->sData;` |
|         7 | 11244 | `							pR++;` |
|         3 | 11245 | `						}` |
|         3 | 11246 | `					}` |
|        17 | 11247 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 11248 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 11249 | `						continue;` |
|         - | 11250 | `					}` |
|        17 | 11251 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        17 | 11252 | `					pR++;` |
|        17 | 11253 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|        13 | 11254 | `						sxi32 iNewVis = -1;` |
|        13 | 11255 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|         7 | 11256 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|         7 | 11257 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|         7 | 11258 | `								iNewVis = nAK;` |
|         7 | 11259 | `								pR++;` |
|         3 | 11260 | `							}` |
|         3 | 11261 | `						}` |
|        13 | 11262 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|        11 | 11263 | `							sAlias = pR->sData;` |
|        11 | 11264 | `							pR++;` |
|         4 | 11265 | `						}` |
|        13 | 11266 | `						pMeth = 0;` |
|        13 | 11267 | `						if( hasQual ){` |
|         3 | 11268 | `							pSrcTrait = 0;` |
|         5 | 11269 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         5 | 11270 | `								SyString *pTN = &apTrait[nT]->sName;` |
|         7 | 11271 | `								if( pTN->nByte >= sTrait.nByte &&` |
|         4 | 11272 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         3 | 11273 | `									pSrcTrait = apTrait[nT];` |
|         3 | 11274 | `									break;` |
|         - | 11275 | `								}` |
|         2 | 11276 | `							}` |
|         3 | 11277 | `							if( pSrcTrait ){` |
|         3 | 11278 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         1 | 11279 | `							}` |
|         2 | 11280 | `						}else{` |
|        10 | 11281 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|         - | 11282 | `						}` |
|        13 | 11283 | `						if( pMeth ){` |
|        13 | 11284 | `							if( sAlias.nByte > 0 ){` |
|         - | 11285 | `								/* Create a shallow copy of the method struct for the alias` |
|         - | 11286 | `								 * so it can carry its own visibility without affecting the original.` |
|         - | 11287 | `								 */` |
|         - | 11288 | `								ph7_class_method *pAlias;` |
|         - | 11289 | `								char *zAliasDup;` |
|        11 | 11290 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        11 | 11291 | `								if( pAlias ){` |
|        11 | 11292 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|        11 | 11293 | `									if( iNewVis >= 0 ){` |
|         5 | 11294 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 11295 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 11296 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         2 | 11297 | `									}` |
|        11 | 11298 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        11 | 11299 | `									if( zAliasDup ){` |
|        11 | 11300 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|         4 | 11301 | `									}` |
|         7 | 11302 | `								}` |
|         7 | 11303 | `							}else if( iNewVis >= 0 ){` |
|         - | 11304 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|         - | 11305 | `								ph7_class_method *pCopy;` |
|         3 | 11306 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|         3 | 11307 | `								if( pCopy ){` |
|         3 | 11308 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|         3 | 11309 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|         3 | 11310 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 11311 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 11312 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         - | 11313 | `									/* Replace the method in the class hash */` |
|         3 | 11314 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|         3 | 11315 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|         1 | 11316 | `								}` |
|         1 | 11317 | `							}` |
|         5 | 11318 | `						}` |
|         5 | 11319 | `						SXUNUSED(hasQual);` |
|         5 | 11320 | `					}` |
|        21 | 11321 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 11322 | `				}` |
|         - | 11323 | `			}` |
|     15253 | 11324 | `			SySetRelease(&pUse->aTraits);` |
|      7629 | 11325 | `		}` |
|         - | 11326 | `	}` |
|    350965 | 11327 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|         - | 11328 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|         - | 11329 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|      3825 | 11330 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|      3825 | 11331 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11332 | `			SySetRelease(&aUseEntries);` |
|       ! 0 | 11333 | `			SySetRelease(&aInterfaces);` |
|       ! 0 | 11334 | `			return SXERR_ABORT;` |
|         - | 11335 | `		}` |
|      1910 | 11336 | `	}` |
|         - | 11337 | `	/* Install the class */` |
|    350965 | 11338 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    350965 | 11339 | `	if( rc == SXRET_OK ){` |
|         - | 11340 | `		ph7_class **apInterface;` |
|         - | 11341 | `		sxu32 n;` |
|    350965 | 11342 | `		if( pBase ){` |
|         - | 11343 | `			/* Inherit from base class and mark as a subclass */` |
|    182463 | 11344 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|     91229 | 11345 | `		}` |
|    350965 | 11346 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|    514387 | 11347 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|         - | 11348 | `			/* Implements one or more interface */` |
|    163427 | 11349 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    163427 | 11350 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11351 | `				break;` |
|         - | 11352 | `			}` |
|     81716 | 11353 | `		}` |
|         - | 11354 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|         - | 11355 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|    350965 | 11356 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|      3825 | 11357 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|      3825 | 11358 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 11359 | `				pIntf = pIntf->pNextName;` |
|       ! 0 | 11360 | `			}` |
|      3825 | 11361 | `			if( pIntf ){` |
|      3825 | 11362 | `				PH7_ClassImplement(pClass,pIntf);` |
|      1910 | 11363 | `			}` |
|      3825 | 11364 | `			if( pClass->nEnumBacking != 0 ){` |
|      3813 | 11365 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|      3813 | 11366 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 11367 | `					pIntf = pIntf->pNextName;` |
|       ! 0 | 11368 | `				}` |
|      3813 | 11369 | `				if( pIntf ){` |
|      3813 | 11370 | `					PH7_ClassImplement(pClass,pIntf);` |
|      1904 | 11371 | `				}` |
|      1904 | 11372 | `			}` |
|      1910 | 11373 | `		}` |
|         - | 11374 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|         - | 11375 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|    350960 | 11376 | `		if( rc == SXRET_OK` |
|    350960 | 11377 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|    350965 | 11378 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|    186113 | 11379 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|         - | 11380 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|    186113 | 11381 | `			if( pStringable ){` |
|    186113 | 11382 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    186113 | 11383 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|         - | 11384 | `				sxu32 i;` |
|    186113 | 11385 | `				int bAlready = 0;` |
|    224077 | 11386 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|     41767 | 11387 | `					if( apImpl[i] == pStringable ){` |
|      3803 | 11388 | `						bAlready = 1;` |
|      3803 | 11389 | `						break;` |
|         - | 11390 | `					}` |
|     18987 | 11391 | `				}` |
|    186113 | 11392 | `				if( !bAlready ){` |
|    182315 | 11393 | `					PH7_ClassImplement(pClass,pStringable);` |
|     91155 | 11394 | `				}` |
|     93054 | 11395 | `			}` |
|     93054 | 11396 | `		}` |
|         - | 11397 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|    350965 | 11398 | `		if( rc == SXRET_OK ){` |
|    350965 | 11399 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|    350965 | 11400 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 11401 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 11402 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 11403 | `				return SXERR_ABORT;` |
|         - | 11404 | `			}` |
|    175480 | 11405 | `		}` |
|         - | 11406 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|    350965 | 11407 | `		if( rc == SXRET_OK ){` |
|    350965 | 11408 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|    350965 | 11409 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 11410 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 11411 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 11412 | `				return SXERR_ABORT;` |
|         - | 11413 | `			}` |
|    175480 | 11414 | `		}` |
|    175480 | 11415 | `	}` |
|    350965 | 11416 | `	SySetRelease(&aUseEntries);` |
|    350965 | 11417 | `	SySetRelease(&aInterfaces);` |
|    350965 | 11418 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11419 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11420 | `		return SXERR_ABORT;` |
|         - | 11421 | `	}` |
|    175480 | 11422 | `done:` |
|         - | 11423 | `	/* Point beyond the class body */` |
|    351007 | 11424 | `	pGen->pIn = &pEnd[1];` |
|    351007 | 11425 | `	pGen->pEnd = pTmp;` |
|    351007 | 11426 | `	return PH7_OK;` |
|    175507 | 11427 | `}` |
|         - | 11428 | `/* Compile a named class declaration (the common case). */` |
|    350976 | 11429 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|         5 | 11430 | `{` |
|    350981 | 11431 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|         5 | 11432 | `}` |
|         - | 11433 | `/*` |
|         - | 11434 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|         - | 11435 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|         - | 11436 | ` * compile + install the class body once (at compile time, like every other` |
|         - | 11437 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|         - | 11438 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|         - | 11439 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|         - | 11440 | ` */` |
|        28 | 11441 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         4 | 11442 | `{` |
|         - | 11443 | `	char zName[128];         /* Synthesized class name */` |
|         - | 11444 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|         - | 11445 | `	SyString sName;` |
|         - | 11446 | `	SyToken *pArgStart,*pArgEnd;` |
|        32 | 11447 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|         - | 11448 | `	                              * is keyed to this 'class' token */` |
|         - | 11449 | `	ph7_value *pObj;` |
|        32 | 11450 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11451 | `	sxu32 nIdx,nLen;` |
|         - | 11452 | `	sxi32 nArg,rc;` |
|        14 | 11453 | `	SXUNUSED(iCompileFlag);` |
|         - | 11454 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|        32 | 11455 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|        32 | 11456 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 | 11457 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|       ! 0 | 11458 | `	}` |
|        32 | 11459 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|         - | 11460 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|         - | 11461 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|         - | 11462 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|        32 | 11463 | `	pArgStart = pArgEnd = 0;` |
|        32 | 11464 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|        32 | 11465 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11466 | `		return rc;` |
|         - | 11467 | `	}` |
|         - | 11468 | `	{` |
|         - | 11469 | ``		/* Expression-position attributes (`new #[A] class {…}`) */`` |
|        32 | 11470 | `		ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0);` |
|        28 | 11471 | `		if( pAnonClass` |
|        32 | 11472 | `		 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 11473 | `			return SXERR_ABORT;` |
|         - | 11474 | `		}` |
|         - | 11475 | `	}` |
|         - | 11476 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|         - | 11477 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|        32 | 11478 | `	nArg = 0;` |
|        32 | 11479 | `	if( pArgStart < pArgEnd ){` |
|         7 | 11480 | `		SyToken *pSavedIn = pGen->pIn;` |
|         7 | 11481 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|         - | 11482 | `		SyToken *pArgNext;` |
|         7 | 11483 | `		pGen->pIn = pArgStart;` |
|         7 | 11484 | `		pGen->pEnd = pArgEnd;` |
|        13 | 11485 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|         7 | 11486 | `			if( pGen->pIn < pArgNext ){` |
|         7 | 11487 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|         7 | 11488 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11489 | `					pGen->pIn = pSavedIn;` |
|       ! 0 | 11490 | `					pGen->pEnd = pSavedEnd;` |
|       ! 0 | 11491 | `					return SXERR_ABORT;` |
|         - | 11492 | `				}` |
|         7 | 11493 | `				nArg++;` |
|         3 | 11494 | `			}` |
|         7 | 11495 | `			pGen->pIn = &pArgNext[1];` |
|         1 | 11496 | `		}` |
|         7 | 11497 | `		pGen->pIn = pSavedIn;` |
|         7 | 11498 | `		pGen->pEnd = pSavedEnd;` |
|         3 | 11499 | `	}` |
|         - | 11500 | `	/* Load the synthesized class name */` |
|        32 | 11501 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        32 | 11502 | `	if( pObj == 0 ){` |
|       ! 0 | 11503 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 11504 | `		return SXERR_ABORT;` |
|         - | 11505 | `	}` |
|        32 | 11506 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|        32 | 11507 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - | 11508 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|        32 | 11509 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        32 | 11510 | `	return SXRET_OK;` |
|        18 | 11511 | `}` |
|         - | 11512 | `/*` |
|         - | 11513 | ` * Compile a user-defined abstract class.` |
|         - | 11514 | ` *  According to the PHP language reference manual` |
|         - | 11515 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|         - | 11516 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|         - | 11517 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|         - | 11518 | ` *   the method's signature - they cannot define the implementation.` |
|         - | 11519 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|         - | 11520 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|         - | 11521 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|         - | 11522 | ` *   method is defined as protected, the function implementation must be defined as either` |
|         - | 11523 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|         - | 11524 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|         - | 11525 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|         - | 11526 | ` *   could differ.` |
|         - | 11527 | ` */` |
|         - | 11528 | `/*` |
|         - | 11529 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|         - | 11530 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|         - | 11531 | ` * receives the corresponding PH7_CLASS_* bit.` |
|         - | 11532 | ` */` |
|  10830860 | 11533 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|         5 | 11534 | `{` |
|  10830865 | 11535 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|   6398587 | 11536 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|   6398587 | 11537 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|   6353009 | 11538 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|   3161275 | 11539 | `	}` |
|  10754833 | 11540 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  10754773 | 11541 | `	return FALSE;` |
|   5415435 | 11542 | `}` |
|         - | 11543 | `/*` |
|         - | 11544 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|         - | 11545 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|         - | 11546 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|         - | 11547 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|         - | 11548 | ` */` |
|  10754768 | 11549 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|         5 | 11550 | `{` |
|  10754773 | 11551 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  10754773 | 11552 | `	sxi32 iFlags = 0,iFlag;` |
|  10830865 | 11553 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|     76097 | 11554 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|         5 | 11555 | `			pDup = pIn;` |
|         2 | 11556 | `		}` |
|     76097 | 11557 | `		iFlags \|= iFlag;` |
|     76097 | 11558 | `		pIn++;` |
|         5 | 11559 | `	}` |
|  10754773 | 11560 | `	*ppIn = pIn;` |
|  10754773 | 11561 | `	if( ppDup ){ *ppDup = pDup; }` |
|  10754773 | 11562 | `	return iFlags;` |
|         5 | 11563 | `}` |
|         - | 11564 | `/*` |
|         - | 11565 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|         - | 11566 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|         - | 11567 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|         - | 11568 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|         - | 11569 | `` * `readonly`) to their existing handlers.`` |
|         - | 11570 | ` */` |
|  10720528 | 11571 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|         5 | 11572 | `{` |
|  10720533 | 11573 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|   5402103 | 11574 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  10741446 | 11575 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|         5 | 11576 | `}` |
|         - | 11577 | `/*` |
|         - | 11578 | ` * Compile a class declaration carrying one or more leading modifiers` |
|         - | 11579 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|         - | 11580 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|         - | 11581 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|         - | 11582 | `` * `abstract`+`final` pair, like PHP.`` |
|         - | 11583 | ` */` |
|     34240 | 11584 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|         5 | 11585 | `{` |
|         - | 11586 | `	SyToken *pDup;` |
|     34245 | 11587 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|         - | 11588 | `	sxi32 rc;` |
|     34245 | 11589 | `	if( pDup ){` |
|         4 | 11590 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|         2 | 11591 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|         3 | 11592 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11593 | `			return SXERR_ABORT;` |
|         - | 11594 | `		}` |
|         1 | 11595 | `	}` |
|     34240 | 11596 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|     17125 | 11597 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|         3 | 11598 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11599 | `			"Cannot use the final modifier on an abstract class");` |
|         3 | 11600 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11601 | `			return SXERR_ABORT;` |
|         - | 11602 | `		}` |
|         1 | 11603 | `	}` |
|     34245 | 11604 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|     17125 | 11605 | `}` |
|         - | 11606 | `/*` |
|         - | 11607 | ` * Compile a user-defined trait.` |
|         - | 11608 | ` *  Traits are similar to classes, but only intended to group functionality` |
|         - | 11609 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|         - | 11610 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|         - | 11611 | ` */` |
|      7670 | 11612 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|         5 | 11613 | `{` |
|      7675 | 11614 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11615 | `	ph7_class *pClass;` |
|         - | 11616 | `	SyToken *pEnd,*pTmp;` |
|         - | 11617 | `	sxi32 iProtection;` |
|         - | 11618 | `	sxi32 iAttrflags;` |
|         - | 11619 | `	SyString *pName;` |
|         - | 11620 | `	sxi32 nKwrd;` |
|         - | 11621 | `	sxi32 rc;` |
|         - | 11622 | `	/* Jump the 'trait' keyword */` |
|      7675 | 11623 | `	pGen->pIn++;` |
|      7675 | 11624 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 11625 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|       ! 0 | 11626 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11627 | `			return SXERR_ABORT;` |
|         - | 11628 | `		}` |
|       ! 0 | 11629 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|       ! 0 | 11630 | `			pGen->pIn++;` |
|       ! 0 | 11631 | `		}` |
|       ! 0 | 11632 | `		return SXRET_OK;` |
|         - | 11633 | `	}` |
|         - | 11634 | `	/* Extract trait name */` |
|      7675 | 11635 | `	pName = &pGen->pIn->sData;` |
|      7675 | 11636 | `	pGen->pIn++;` |
|         - | 11637 | `	/* Build FQN and obtain a raw class */ {` |
|         - | 11638 | `		SyBlob sFQN;` |
|         - | 11639 | `		SyString sFQNStr;` |
|      7675 | 11640 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7675 | 11641 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      7675 | 11642 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      7675 | 11643 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      7675 | 11644 | `		SyBlobRelease(&sFQN);` |
|         - | 11645 | `	}` |
|      7675 | 11646 | `	if( pClass == 0 ){` |
|       ! 0 | 11647 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11648 | `		return SXERR_ABORT;` |
|         - | 11649 | `	}` |
|      7675 | 11650 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|      7675 | 11651 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 11652 | `		return SXERR_ABORT;` |
|         - | 11653 | `	}` |
|         - | 11654 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      7675 | 11655 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|       ! 0 | 11656 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|       ! 0 | 11657 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 11658 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11659 | `			return SXERR_ABORT;` |
|         - | 11660 | `		}` |
|       ! 0 | 11661 | `		return SXRET_OK;` |
|         - | 11662 | `	}` |
|      7675 | 11663 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      7675 | 11664 | `	pEnd = 0;` |
|      7675 | 11665 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      7675 | 11666 | `	if( pEnd >= pGen->pEnd ){` |
|       ! 0 | 11667 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|       ! 0 | 11668 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 11669 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11670 | `			return SXERR_ABORT;` |
|         - | 11671 | `		}` |
|       ! 0 | 11672 | `		return SXRET_OK;` |
|         - | 11673 | `	}` |
|         - | 11674 | `	/* The delimiter token is the trait body's closing brace */` |
|      7675 | 11675 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 11676 | `	/* Swap token stream */` |
|      7675 | 11677 | `	pTmp = pGen->pEnd;` |
|      7675 | 11678 | `	pGen->pEnd = pEnd;` |
|         - | 11679 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|      7675 | 11680 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|         - | 11681 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|     53223 | 11682 | `	for(;;){` |
|    144459 | 11683 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     19011 | 11684 | `			pGen->pIn++;` |
|         5 | 11685 | `		}` |
|    125453 | 11686 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      7675 | 11687 | `			break;` |
|         - | 11688 | `		}` |
|         - | 11689 | `		/* Bind a directly-preceding docblock to this member */` |
|    117783 | 11690 | `		GenStateSetPendingDoc(&(*pGen));` |
|    117783 | 11691 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|       ! 0 | 11692 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11693 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 11694 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 11695 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 11696 | `				return SXERR_ABORT;` |
|         - | 11697 | `			}` |
|       ! 0 | 11698 | `			goto done;` |
|         - | 11699 | `		}` |
|    117783 | 11700 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|    117783 | 11701 | `		iAttrflags = 0;` |
|    117783 | 11702 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|    117783 | 11703 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    117783 | 11704 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 11705 | `				/* Trait uses another trait: use OtherTrait; */` |
|         5 | 11706 | `				pGen->pIn++; /* Jump 'use' */` |
|         2 | 11707 | `				for(;;){` |
|         - | 11708 | `					ph7_class *pUsedTrait;` |
|         - | 11709 | `					SyString *pUsedName;` |
|         5 | 11710 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 11711 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 11712 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|       ! 0 | 11713 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11714 | `							return SXERR_ABORT;` |
|         - | 11715 | `						}` |
|       ! 0 | 11716 | `						break;` |
|         - | 11717 | `					}` |
|         5 | 11718 | `					pUsedName = &pGen->pIn->sData;` |
|         - | 11719 | `					{` |
|         - | 11720 | `						SyBlob sResolved;` |
|         5 | 11721 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|         5 | 11722 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|         7 | 11723 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|         4 | 11724 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|         5 | 11725 | `						SyBlobRelease(&sResolved);` |
|         - | 11726 | `					}` |
|         5 | 11727 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 11728 | `						pUsedTrait = pUsedTrait->pNextName;` |
|       ! 0 | 11729 | `					}` |
|         5 | 11730 | `					if( pUsedTrait == 0 ){` |
|         4 | 11731 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         1 | 11732 | `							"'%z' is not a trait",pUsedName);` |
|         3 | 11733 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11734 | `							return SXERR_ABORT;` |
|         - | 11735 | `						}` |
|         2 | 11736 | `					}else{` |
|         3 | 11737 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|         - | 11738 | `					}` |
|         5 | 11739 | `					pGen->pIn++;` |
|         5 | 11740 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|         3 | 11741 | `						break;` |
|         - | 11742 | `					}` |
|       ! 0 | 11743 | `					pGen->pIn++;` |
|       ! 0 | 11744 | `				}` |
|         5 | 11745 | `				continue;` |
|         - | 11746 | `			}` |
|    117779 | 11747 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|    117763 | 11748 | `				iProtection = nKwrd;` |
|    117763 | 11749 | `				pGen->pIn++;` |
|    117758 | 11750 | `				if( pGen->pIn >= pGen->pEnd` |
|    117763 | 11751 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11752 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11753 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 11754 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 11755 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11756 | `						return SXERR_ABORT;` |
|         - | 11757 | `					}` |
|       ! 0 | 11758 | `					goto done;` |
|         - | 11759 | `				}` |
|    117763 | 11760 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     18997 | 11761 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     18997 | 11762 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11763 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11764 | `							return SXERR_ABORT;` |
|         - | 11765 | `						}` |
|       ! 0 | 11766 | `						goto done;` |
|         - | 11767 | `					}` |
|     18997 | 11768 | `					continue;` |
|         - | 11769 | `				}` |
|     98771 | 11770 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         5 | 11771 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         5 | 11772 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11773 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11774 | `							return SXERR_ABORT;` |
|         - | 11775 | `						}` |
|       ! 0 | 11776 | `						goto done;` |
|         - | 11777 | `					}` |
|         5 | 11778 | `					continue;` |
|         - | 11779 | `				}` |
|     98767 | 11780 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     49381 | 11781 | `			}` |
|     98783 | 11782 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       ! 0 | 11783 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11784 | `					"Traits cannot have constants");` |
|       ! 0 | 11785 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11786 | `					return SXERR_ABORT;` |
|         - | 11787 | `				}` |
|       ! 0 | 11788 | `				goto done;` |
|       ! 0 | 11789 | `			}else{` |
|     98783 | 11790 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|      7607 | 11791 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      7607 | 11792 | `					pGen->pIn++;` |
|      7607 | 11793 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      7605 | 11794 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      7605 | 11795 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 11796 | `							iProtection = nKwrd;` |
|       ! 0 | 11797 | `							pGen->pIn++;` |
|       ! 0 | 11798 | `						}` |
|      3800 | 11799 | `					}` |
|      7602 | 11800 | `					if( pGen->pIn >= pGen->pEnd` |
|      7607 | 11801 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11802 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11803 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|       ! 0 | 11804 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11805 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11806 | `							return SXERR_ABORT;` |
|         - | 11807 | `						}` |
|       ! 0 | 11808 | `						goto done;` |
|         - | 11809 | `					}` |
|      7607 | 11810 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         3 | 11811 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         3 | 11812 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 11813 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11814 | `								return SXERR_ABORT;` |
|         - | 11815 | `							}` |
|       ! 0 | 11816 | `							goto done;` |
|         - | 11817 | `						}` |
|         3 | 11818 | `						continue;` |
|         - | 11819 | `					}` |
|      7605 | 11820 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       ! 0 | 11821 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11822 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 11823 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11824 | `								return SXERR_ABORT;` |
|         - | 11825 | `							}` |
|       ! 0 | 11826 | `							goto done;` |
|         - | 11827 | `						}` |
|       ! 0 | 11828 | `						continue;` |
|         - | 11829 | `					}` |
|      7605 | 11830 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     94981 | 11831 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         6 | 11832 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         6 | 11833 | `					pGen->pIn++;` |
|         6 | 11834 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         6 | 11835 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         6 | 11836 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         6 | 11837 | `							iProtection = nKwrd;` |
|         6 | 11838 | `							pGen->pIn++;` |
|         2 | 11839 | `						}` |
|         2 | 11840 | `					}` |
|         6 | 11841 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         4 | 11842 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 11843 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11844 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|       ! 0 | 11845 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11846 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11847 | `							return SXERR_ABORT;` |
|         - | 11848 | `						}` |
|       ! 0 | 11849 | `						goto done;` |
|         - | 11850 | `					}` |
|         6 | 11851 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         2 | 11852 | `				}` |
|     98781 | 11853 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 11854 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11855 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|       ! 0 | 11856 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 11857 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11858 | `						return SXERR_ABORT;` |
|         - | 11859 | `					}` |
|       ! 0 | 11860 | `					goto done;` |
|         - | 11861 | `				}` |
|     98781 | 11862 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       ! 0 | 11863 | `					pGen->pIn++;` |
|       ! 0 | 11864 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 | 11865 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11866 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 11867 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11868 | `							return SXERR_ABORT;` |
|         - | 11869 | `						}` |
|       ! 0 | 11870 | `						goto done;` |
|         - | 11871 | `					}` |
|       ! 0 | 11872 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11873 | `				}else{` |
|     98781 | 11874 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 11875 | `				}` |
|     98781 | 11876 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 11877 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11878 | `						return SXERR_ABORT;` |
|         - | 11879 | `					}` |
|       ! 0 | 11880 | `					goto done;` |
|         - | 11881 | `				}` |
|         - | 11882 | `			}` |
|     49393 | 11883 | `		}else{` |
|       ! 0 | 11884 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11885 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11886 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11887 | `					return SXERR_ABORT;` |
|         - | 11888 | `				}` |
|       ! 0 | 11889 | `				goto done;` |
|         - | 11890 | `			}` |
|         - | 11891 | `		}` |
|         5 | 11892 | `	}` |
|         - | 11893 | `	/* Install the trait */` |
|      7675 | 11894 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      7675 | 11895 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11896 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11897 | `		return SXERR_ABORT;` |
|         - | 11898 | `	}` |
|      3835 | 11899 | `done:` |
|         - | 11900 | `	/* Point beyond the trait body */` |
|      7675 | 11901 | `	pGen->pIn = &pEnd[1];` |
|      7675 | 11902 | `	pGen->pEnd = pTmp;` |
|      7675 | 11903 | `	return PH7_OK;` |
|      3840 | 11904 | `}` |
|         - | 11905 | `/*` |
|         - | 11906 | ` * Compile a user-defined class.` |
|         - | 11907 | ` *  According to the PHP language reference manual` |
|         - | 11908 | ` *   Basic class definitions begin with the keyword class, followed` |
|         - | 11909 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|         - | 11910 | ` *   the definitions of the properties and methods belonging to the class.` |
|         - | 11911 | ` *   A class may contain its own constants, variables (called "properties")` |
|         - | 11912 | ` *   and functions (called "methods").` |
|         - | 11913 | ` */` |
|    312912 | 11914 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|         5 | 11915 | `{` |
|         - | 11916 | `	sxi32 rc;` |
|    312917 | 11917 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|    312917 | 11918 | `	return rc;` |
|         5 | 11919 | `}` |
|         - | 11920 | `/*` |
|         - | 11921 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|         - | 11922 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|         - | 11923 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|         - | 11924 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|         - | 11925 | `` * meaning; `enum Name` can never start a valid expression.`` |
|         - | 11926 | ` */` |
|  10678696 | 11927 | `static int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|         5 | 11928 | `{` |
|  10859462 | 11929 | `	return (pIn->nType & PH7_TK_ID)` |
|   5520109 | 11930 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|    190378 | 11931 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
|  10859457 | 11932 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|         5 | 11933 | `}` |
|         - | 11934 | `/*` |
|         - | 11935 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|         - | 11936 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|         - | 11937 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|         - | 11938 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|         - | 11939 | ` */` |
|      3824 | 11940 | `static sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|         5 | 11941 | `{` |
|      3829 | 11942 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|         5 | 11943 | `}` |
|         - | 11944 | `/*` |
|         - | 11945 | ` * Exception handling.` |
|         - | 11946 | ` *  According to the PHP language reference manual` |
|         - | 11947 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|         - | 11948 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|         - | 11949 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|         - | 11950 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|         - | 11951 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|         - | 11952 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|         - | 11953 | ` *    (or re-thrown) within a catch block.` |
|         - | 11954 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|         - | 11955 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|         - | 11956 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|         - | 11957 | ` *    been defined with set_exception_handler().` |
|         - | 11958 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|         - | 11959 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|         - | 11960 | ` */` |
|         - | 11961 | `/*` |
|         - | 11962 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|         - | 11963 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|         - | 11964 | ` * indicates failure.` |
|         - | 11965 | ` */` |
|    474912 | 11966 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 | 11967 | `{` |
|    474917 | 11968 | `	sxi32 rc = SXRET_OK;` |
|    474917 | 11969 | `	if( pRoot->pOp ){` |
|    474905 | 11970 | `		switch( pRoot->pOp->iOp ){` |
|    237450 | 11971 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|         - | 11972 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|         - | 11973 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|         - | 11974 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|         - | 11975 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|         - | 11976 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|    474905 | 11977 | `			break;` |
|       ! 0 | 11978 | `		default:` |
|         - | 11979 | `			/* Runtime will still reject non-Throwable values; the set above` |
|         - | 11980 | `			 * covers the common shapes and gives a friendlier compile error` |
|         - | 11981 | ``			 * for obvious mistakes like `throw 5`. */`` |
|       ! 0 | 11982 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - | 11983 | `				"throw: Expecting an exception class instance");` |
|       ! 0 | 11984 | `			if( rc != SXERR_ABORT ){` |
|       ! 0 | 11985 | `				rc = SXERR_INVALID;` |
|       ! 0 | 11986 | `			}` |
|       ! 0 | 11987 | `			break;` |
|         - | 11988 | `		}` |
|    237467 | 11989 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - | 11990 | `		/* Unexpected expression */` |
|       ! 0 | 11991 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - | 11992 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 11993 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 | 11994 | `			rc = SXERR_INVALID;` |
|       ! 0 | 11995 | `		}` |
|       ! 0 | 11996 | `	}` |
|    474917 | 11997 | `	return rc;` |
|         5 | 11998 | `}` |
|         - | 11999 | `/*` |
|         - | 12000 | ` * Compile a 'throw' statement.` |
|         - | 12001 | ` * throw: This is how you trigger an exception.` |
|         - | 12002 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|         - | 12003 | ` */` |
|    474876 | 12004 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|         5 | 12005 | `{` |
|    474881 | 12006 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12007 | `	GenBlock *pBlock;` |
|         - | 12008 | `	sxu32 nIdx;` |
|         - | 12009 | `	sxi32 rc;` |
|    474881 | 12010 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|         - | 12011 | `	/* Compile the expression */` |
|    474881 | 12012 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    474881 | 12013 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 | 12014 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|       ! 0 | 12015 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12016 | `			return SXERR_ABORT;` |
|         - | 12017 | `		}` |
|       ! 0 | 12018 | `		return SXRET_OK;` |
|         - | 12019 | `	}` |
|    474881 | 12020 | `	pBlock = pGen->pCurrent;` |
|         - | 12021 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   1876001 | 12022 | `	while(pBlock->pParent){` |
|   1875997 | 12023 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    474877 | 12024 | `			break;` |
|         - | 12025 | `		}` |
|         - | 12026 | `		/* Point to the parent block */` |
|   1401125 | 12027 | `		pBlock = pBlock->pParent;` |
|         5 | 12028 | `	}` |
|         - | 12029 | `	/* Emit the throw instruction */` |
|    474881 | 12030 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|         - | 12031 | `	/* Emit the jump */` |
|    474881 | 12032 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    474881 | 12033 | `	return SXRET_OK;` |
|    237443 | 12034 | `}` |
|         - | 12035 | `/*` |
|         - | 12036 | ` * Compile a PHP 8.0 'throw' expression.` |
|         - | 12037 | ` * Called from the expression code generator when a 'throw' keyword is` |
|         - | 12038 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|         - | 12039 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|         - | 12040 | ` * the validator guarantees the operand is a valid exception target.` |
|         - | 12041 | ` */` |
|        36 | 12042 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|         2 | 12043 | `{` |
|        38 | 12044 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12045 | `	GenBlock *pBlock;` |
|         - | 12046 | `	sxu32 nIdx;` |
|         - | 12047 | `	sxi32 rc;` |
|        18 | 12048 | `	(void)iCompileFlag;` |
|        38 | 12049 | `	pGen->pIn++; /* Skip 'throw' */` |
|        38 | 12050 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 | 12051 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12052 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12053 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12054 | `			return SXERR_ABORT;` |
|         - | 12055 | `		}` |
|       ! 0 | 12056 | `		return SXRET_OK;` |
|         - | 12057 | `	}` |
|        38 | 12058 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|        38 | 12059 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12060 | `		return SXERR_ABORT;` |
|         - | 12061 | `	}` |
|        38 | 12062 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 | 12063 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12064 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12065 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12066 | `			return SXERR_ABORT;` |
|         - | 12067 | `		}` |
|       ! 0 | 12068 | `		return SXRET_OK;` |
|         - | 12069 | `	}` |
|         - | 12070 | `	/* Walk up to nearest exception/function block for the jump target */` |
|        38 | 12071 | `	pBlock = pGen->pCurrent;` |
|        60 | 12072 | `	while( pBlock->pParent ){` |
|        49 | 12073 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|        27 | 12074 | `			break;` |
|         - | 12075 | `		}` |
|        23 | 12076 | `		pBlock = pBlock->pParent;` |
|         1 | 12077 | `	}` |
|        38 | 12078 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        38 | 12079 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|        38 | 12080 | `	return SXRET_OK;` |
|        20 | 12081 | `}` |
|         - | 12082 | `/*` |
|         - | 12083 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|         - | 12084 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|         - | 12085 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|         - | 12086 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|         - | 12087 | ` * compile error propagated from the parser.` |
|         - | 12088 | ` */` |
|        54 | 12089 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|         5 | 12090 | `{` |
|         - | 12091 | `	SyString sClassName;` |
|         - | 12092 | `	SyToken *pToken;` |
|         - | 12093 | `	SyString *pName;` |
|         - | 12094 | `	char *zDup;` |
|         - | 12095 | `	sxi32 rc;` |
|        59 | 12096 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        59 | 12097 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|        59 | 12098 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|        59 | 12099 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        59 | 12100 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 | 12101 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12102 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12103 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12104 | `		return SXERR_INVALID;` |
|         - | 12105 | `	}` |
|        59 | 12106 | `	pGen->pIn++; /* '(' */` |
|        27 | 12107 | `	for(;;){` |
|         - | 12108 | `		SyBlob sResolved;` |
|        59 | 12109 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|        59 | 12110 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 12111 | `			SyBlobRelease(&sResolved);` |
|       ! 0 | 12112 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12113 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12114 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12115 | `			return SXERR_INVALID;` |
|         - | 12116 | `		}` |
|        86 | 12117 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        54 | 12118 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        59 | 12119 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|        59 | 12120 | `		SyBlobRelease(&sResolved);` |
|        59 | 12121 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|        59 | 12122 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|        59 | 12123 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        54 | 12124 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|         5 | 12125 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       ! 0 | 12126 | `			pGen->pIn++; continue;` |
|         - | 12127 | `		}` |
|        59 | 12128 | `		break;` |
|       ! 0 | 12129 | `	}` |
|        54 | 12130 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|        59 | 12131 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 12132 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12133 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12134 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12135 | `		return SXERR_INVALID;` |
|         - | 12136 | `	}` |
|        59 | 12137 | `	pGen->pIn++; /* '$' */` |
|        59 | 12138 | `	pName = &pGen->pIn->sData;` |
|        59 | 12139 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|        59 | 12140 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|        59 | 12141 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|        59 | 12142 | `	pGen->pIn++;` |
|        59 | 12143 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 | 12144 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12145 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12146 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12147 | `		return SXERR_INVALID;` |
|         - | 12148 | `	}` |
|        59 | 12149 | `	pGen->pIn++; /* ')' */` |
|        59 | 12150 | `	return SXRET_OK;` |
|        32 | 12151 | `}` |
|         - | 12152 | `/*` |
|         - | 12153 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|         - | 12154 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|         - | 12155 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|         - | 12156 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|         - | 12157 | ` * VmThrowException):` |
|         - | 12158 | ` *` |
|         - | 12159 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|         - | 12160 | ` *    <try body>` |
|         - | 12161 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|         - | 12162 | ` *    JMP  -> finally\|end` |
|         - | 12163 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|         - | 12164 | ` *    <catch body>` |
|         - | 12165 | ` *    JMP  -> finally\|end` |
|         - | 12166 | ` *    ... more catches ...` |
|         - | 12167 | ` *  Lfin: <finally body>` |
|         - | 12168 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|         - | 12169 | ` *  Lend:` |
|         - | 12170 | ` */` |
|        98 | 12171 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|         5 | 12172 | `{` |
|       103 | 12173 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12174 | `	GenBlock *pTry;` |
|         - | 12175 | `	VmInstr *pInstr;` |
|       103 | 12176 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|         - | 12177 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|         - | 12178 | `	sxi32 rc;` |
|       103 | 12179 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|         - | 12180 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|       103 | 12181 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|       103 | 12182 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       103 | 12183 | `	pTry->pUserData = pException;` |
|       103 | 12184 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|       103 | 12185 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       103 | 12186 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       103 | 12187 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       103 | 12188 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       103 | 12189 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|         - | 12190 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|       103 | 12191 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|       103 | 12192 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|       103 | 12193 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       103 | 12194 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12195 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|       103 | 12196 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|         - | 12197 | `	/* Catch clauses (inline) */` |
|       103 | 12198 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        98 | 12199 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        59 | 12200 | `		sxu32 k = 0;` |
|        81 | 12201 | `		for(;;){` |
|         - | 12202 | `			ph7_exception_block sCatch;` |
|         - | 12203 | `			GenBlock *pCatchBlk;` |
|       113 | 12204 | `			sxu32 idxJmp = 0;` |
|       108 | 12205 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|       104 | 12206 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|        32 | 12207 | `				break;` |
|         - | 12208 | `			}` |
|        59 | 12209 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|        59 | 12210 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        59 | 12211 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|        59 | 12212 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|        59 | 12213 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|        59 | 12214 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|        59 | 12215 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|         - | 12216 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|         - | 12217 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|         - | 12218 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|        59 | 12219 | `			pCatchBlk->pUserData = pException;` |
|        59 | 12220 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|        59 | 12221 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        59 | 12222 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|        59 | 12223 | `			GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12224 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|         - | 12225 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|        59 | 12226 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        59 | 12227 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|        59 | 12228 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|        59 | 12229 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|        59 | 12230 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        59 | 12231 | `			k++;` |
|         5 | 12232 | `		}` |
|        27 | 12233 | `	}` |
|         - | 12234 | `	/* Finally (inline) */` |
|       103 | 12235 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        80 | 12236 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|         - | 12237 | `		GenBlock *pFinBlk;` |
|        52 | 12238 | `		pGen->pIn++; /* Jump 'finally' */` |
|        52 | 12239 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|        52 | 12240 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|        52 | 12241 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        52 | 12242 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|        52 | 12243 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        52 | 12244 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|        52 | 12245 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        52 | 12246 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|        52 | 12247 | `		pException->iHasFinally = 1;` |
|        24 | 12248 | `	}` |
|       103 | 12249 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|       103 | 12250 | `	pException->iInlined = 1;` |
|         - | 12251 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|         - | 12252 | `	{` |
|       103 | 12253 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|         - | 12254 | `		sxu32 *aJ; sxu32 n;` |
|       103 | 12255 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|       103 | 12256 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       103 | 12257 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|       157 | 12258 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|        59 | 12259 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|        59 | 12260 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|        32 | 12261 | `		}` |
|         - | 12262 | `	}` |
|       103 | 12263 | `	SySetRelease(&aCatchJmp);` |
|       103 | 12264 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       ! 0 | 12265 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|       ! 0 | 12266 | `	}` |
|       103 | 12267 | `	return SXRET_OK;` |
|        54 | 12268 | `}` |
|         - | 12269 | `/*` |
|         - | 12270 | ` * Compile a 'catch' block.` |
|         - | 12271 | ` * Catch: A "catch" block retrieves an exception and creates` |
|         - | 12272 | ` * an object containing the exception information.` |
|         - | 12273 | ` */` |
|     24258 | 12274 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|         5 | 12275 | `{` |
|     24263 | 12276 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12277 | `	ph7_exception_block sCatch;` |
|         - | 12278 | `	SySet *pInstrContainer;` |
|         - | 12279 | `	SyString sClassName;` |
|         - | 12280 | `	GenBlock *pCatch;` |
|         - | 12281 | `	SyToken *pToken;` |
|         - | 12282 | `	SyString *pName;` |
|         - | 12283 | `	char *zDup;` |
|         - | 12284 | `	sxi32 rc;` |
|     24263 | 12285 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|         - | 12286 | `	/* Zero the structure */` |
|     24263 | 12287 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|         - | 12288 | `	/* Initialize fields */` |
|     24263 | 12289 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     24263 | 12290 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     24263 | 12291 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|         - | 12292 | `			/* Unexpected token,break immediately */` |
|       ! 0 | 12293 | `			pToken = pGen->pIn;` |
|       ! 0 | 12294 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12295 | `				pToken--;` |
|       ! 0 | 12296 | `			}` |
|       ! 0 | 12297 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12298 | `				"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12299 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12300 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12301 | `				return SXERR_ABORT;` |
|         - | 12302 | `			}` |
|       ! 0 | 12303 | `			return SXERR_INVALID;` |
|         - | 12304 | `	}` |
|         - | 12305 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     24263 | 12306 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     12143 | 12307 | `	for(;;){` |
|         - | 12308 | `		SyBlob sResolved;` |
|     24291 | 12309 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     24291 | 12310 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         6 | 12311 | `			SyBlobRelease(&sResolved);` |
|         6 | 12312 | `			pToken = pGen->pIn;` |
|         6 | 12313 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12314 | `				pToken--;` |
|       ! 0 | 12315 | `			}` |
|         8 | 12316 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12317 | `				"syntax error, unexpected %s \"%z\"",` |
|         2 | 12318 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|         6 | 12319 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12320 | `				return SXERR_ABORT;` |
|         - | 12321 | `			}` |
|         6 | 12322 | `			return SXERR_INVALID;` |
|         - | 12323 | `		}` |
|         - | 12324 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|         - | 12325 | `		 * transient SyBlob allocation. */` |
|     36428 | 12326 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     24282 | 12327 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     24287 | 12328 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     24287 | 12329 | `		SyBlobRelease(&sResolved);` |
|     24287 | 12330 | `		if( zDup == 0 ){` |
|       ! 0 | 12331 | `			goto Mem;` |
|         - | 12332 | `		}` |
|     24287 | 12333 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     24287 | 12334 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 12335 | `			goto Mem;` |
|         - | 12336 | `		}` |
|         - | 12337 | `		/* Check for '\|' (multi-catch separator) */` |
|     24282 | 12338 | `		if( pGen->pIn < pGen->pEnd &&` |
|     24282 | 12339 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|        33 | 12340 | `			pGen->pIn->sData.nByte == 1 &&` |
|        28 | 12341 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|        30 | 12342 | `			pGen->pIn++; /* Consume the '\|' */` |
|        30 | 12343 | `			continue;` |
|         - | 12344 | `		}` |
|     24259 | 12345 | `		break;` |
|       ! 0 | 12346 | `	}` |
|     24254 | 12347 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     24259 | 12348 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - | 12349 | `			/* Unexpected token,break immediately */` |
|       ! 0 | 12350 | `			pToken = pGen->pIn;` |
|       ! 0 | 12351 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12352 | `				pToken--;` |
|       ! 0 | 12353 | `			}` |
|       ! 0 | 12354 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12355 | `				"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12356 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12357 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12358 | `				return SXERR_ABORT;` |
|         - | 12359 | `			}` |
|       ! 0 | 12360 | `			return SXERR_INVALID;` |
|         - | 12361 | `	}` |
|     24259 | 12362 | `	pGen->pIn++; /* Jump the dollar sign */` |
|         - | 12363 | `	/* Duplicate instance name */` |
|     24259 | 12364 | `	pName = &pGen->pIn->sData;` |
|     24259 | 12365 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     24259 | 12366 | `	if( zDup == 0 ){` |
|       ! 0 | 12367 | `		goto Mem;` |
|         - | 12368 | `	}` |
|     24259 | 12369 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     24259 | 12370 | `	pGen->pIn++;` |
|     24259 | 12371 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|         - | 12372 | `		/* Unexpected token,break immediately */` |
|       ! 0 | 12373 | `		pToken = pGen->pIn;` |
|       ! 0 | 12374 | `		if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12375 | `			pToken--;` |
|       ! 0 | 12376 | `		}` |
|       ! 0 | 12377 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12378 | `			"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12379 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12380 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12381 | `			return SXERR_ABORT;` |
|         - | 12382 | `		}` |
|       ! 0 | 12383 | `		return SXERR_INVALID;` |
|         - | 12384 | `	}` |
|         - | 12385 | `	/* Compile the block */` |
|     24259 | 12386 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|         - | 12387 | `	/* Create the catch block */` |
|     24259 | 12388 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     24259 | 12389 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12390 | `		return SXERR_ABORT;` |
|         - | 12391 | `	}` |
|         - | 12392 | `	/* Swap bytecode container */` |
|     24259 | 12393 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     24259 | 12394 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|         - | 12395 | `	/* Compile the block */` |
|     24259 | 12396 | `	PH7_CompileBlock(&(*pGen),0);` |
|         - | 12397 | `	/* Fix forward jumps now the destination is resolved  */` |
|     24259 | 12398 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12399 | `	/* Emit the DONE instruction */` |
|     24259 | 12400 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - | 12401 | `	/* Leave the block */` |
|     24259 | 12402 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12403 | `	/* Restore the default container */` |
|     24259 | 12404 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - | 12405 | `	/* Install the catch block */` |
|     24259 | 12406 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     24259 | 12407 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12408 | `		goto Mem;` |
|         - | 12409 | `	}` |
|     24259 | 12410 | `	return SXRET_OK;` |
|       ! 0 | 12411 | `Mem:` |
|       ! 0 | 12412 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 12413 | `	return SXERR_ABORT;` |
|     12134 | 12414 | `}` |
|         - | 12415 | `/*` |
|         - | 12416 | ` * Compile a 'try' block.` |
|         - | 12417 | ` * A function using an exception should be in a "try" block.` |
|         - | 12418 | ` * If the exception does not trigger, the code will continue` |
|         - | 12419 | ` * as normal. However if the exception triggers, an exception` |
|         - | 12420 | ` * is "thrown".` |
|         - | 12421 | ` */` |
|     24414 | 12422 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|         5 | 12423 | `{` |
|         - | 12424 | `	ph7_exception *pException;` |
|     24419 | 12425 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12426 | `	GenBlock *pTry;` |
|         - | 12427 | `	sxu32 nJmpIdx;` |
|         - | 12428 | `	sxi32 rc;` |
|         - | 12429 | `	/* Create the exception container */` |
|     24419 | 12430 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     24419 | 12431 | `	if( pException == 0 ){` |
|       ! 0 | 12432 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|       ! 0 | 12433 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 12434 | `		return SXERR_ABORT;` |
|         - | 12435 | `	}` |
|         - | 12436 | `	/* Zero the structure */` |
|     24419 | 12437 | `	SyZero(pException,sizeof(ph7_exception));` |
|         - | 12438 | `	/* Initialize fields */` |
|     24419 | 12439 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     24419 | 12440 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     24419 | 12441 | `	pException->iHasFinally = 0;` |
|     24419 | 12442 | `	pException->iFinallyDone = 0;` |
|     24419 | 12443 | `	pException->pVm = pGen->pVm;` |
|         - | 12444 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|         - | 12445 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|         - | 12446 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|         - | 12447 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|         - | 12448 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|         - | 12449 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|     24419 | 12450 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|       103 | 12451 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|         - | 12452 | `	}` |
|         - | 12453 | `	/* Create the try block */` |
|     24321 | 12454 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     24321 | 12455 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12456 | `		return SXERR_ABORT;` |
|         - | 12457 | `	}` |
|         - | 12458 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     24321 | 12459 | `	pTry->pUserData = pException;` |
|         - | 12460 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     24321 | 12461 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|         - | 12462 | `	/* Fix the jump later when the destination is resolved */` |
|     24321 | 12463 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     24321 | 12464 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|         - | 12465 | `	/* Compile the block */` |
|     24321 | 12466 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     24321 | 12467 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12468 | `		return SXERR_ABORT;` |
|         - | 12469 | `	}` |
|         - | 12470 | `	/* Fix forward jumps now the destination is resolved */` |
|     24321 | 12471 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12472 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     24321 | 12473 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|         - | 12474 | `	/* Leave the block */` |
|     24321 | 12475 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12476 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     24321 | 12477 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     24314 | 12478 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|         - | 12479 | `		/* Compile one or more catch blocks */` |
|     24254 | 12480 | `		for(;;){` |
|     48508 | 12481 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     36441 | 12482 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     12130 | 12483 | `					break;` |
|         - | 12484 | `			}` |
|     24263 | 12485 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     24263 | 12486 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12487 | `				return SXERR_ABORT;` |
|         - | 12488 | `			}` |
|         5 | 12489 | `		}` |
|     12125 | 12490 | `	}` |
|         - | 12491 | `	/* Compile optional finally block */` |
|     24321 | 12492 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       728 | 12493 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|         - | 12494 | `		SySet *pInstrContainer;` |
|         - | 12495 | `		GenBlock *pFinBlock;` |
|       129 | 12496 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|         - | 12497 | `		/* Create the finally block for jump fixup bookkeeping */` |
|       129 | 12498 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|       129 | 12499 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 12500 | `			return SXERR_ABORT;` |
|         - | 12501 | `		}` |
|         - | 12502 | `		/* Swap bytecode container */` |
|       129 | 12503 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       129 | 12504 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|         - | 12505 | `		/* Compile the finally body */` |
|       129 | 12506 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       129 | 12507 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12508 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 | 12509 | `			return SXERR_ABORT;` |
|         - | 12510 | `		}` |
|         - | 12511 | `		/* Fix forward jumps now the destination is resolved */` |
|       129 | 12512 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12513 | `		/* Emit DONE to terminate the finally block */` |
|       129 | 12514 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - | 12515 | `		/* Leave the block */` |
|       129 | 12516 | `		GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12517 | `		/* Restore the default container */` |
|       129 | 12518 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       129 | 12519 | `		pException->iHasFinally = 1;` |
|        62 | 12520 | `	}` |
|         - | 12521 | `	/* Must have at least one catch or finally */` |
|     24321 | 12522 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|         8 | 12523 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12524 | `			"Cannot use try without catch or finally");` |
|         8 | 12525 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12526 | `			return SXERR_ABORT;` |
|         - | 12527 | `		}` |
|         3 | 12528 | `	}` |
|     24321 | 12529 | `	return SXRET_OK;` |
|     12212 | 12530 | `}` |
|         - | 12531 | `/*` |
|         - | 12532 | ` * Compile a switch block.` |
|         - | 12533 | ` *  (See block-comment below for more information)` |
|         - | 12534 | ` */` |
|       112 | 12535 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|         5 | 12536 | `{` |
|       117 | 12537 | `	sxi32 rc = SXRET_OK;` |
|       117 | 12538 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|         - | 12539 | `		/* Unexpected token */` |
|       ! 0 | 12540 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 | 12541 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12542 | `			return SXERR_ABORT;` |
|         - | 12543 | `		}` |
|       ! 0 | 12544 | `		pGen->pIn++;` |
|       ! 0 | 12545 | `	}` |
|       117 | 12546 | `	pGen->pIn++;` |
|         - | 12547 | `	/* First instruction to execute in this block. */` |
|       117 | 12548 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|         - | 12549 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|         - | 12550 | `	 * or the '}' token */` |
|       206 | 12551 | `	for(;;){` |
|       417 | 12552 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 12553 | `			/* No more input to process */` |
|       ! 0 | 12554 | `			break;` |
|         - | 12555 | `		}` |
|       417 | 12556 | `		rc = SXRET_OK;` |
|       417 | 12557 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|        85 | 12558 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|        31 | 12559 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|         - | 12560 | `					/* Unexpected token */` |
|       ! 0 | 12561 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|       ! 0 | 12562 | `						&pGen->pIn->sData);` |
|       ! 0 | 12563 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12564 | `						return SXERR_ABORT;` |
|         - | 12565 | `					}` |
|         - | 12566 | `					/* FALL THROUGH */` |
|       ! 0 | 12567 | `				}` |
|        31 | 12568 | `				rc = SXERR_EOF;` |
|        31 | 12569 | `				break;` |
|         - | 12570 | `			}` |
|        32 | 12571 | `		}else{` |
|         - | 12572 | `			sxi32 nKwrd;` |
|         - | 12573 | `			/* Extract the keyword */` |
|       337 | 12574 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       337 | 12575 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|        47 | 12576 | `				break;` |
|         - | 12577 | `			}` |
|       253 | 12578 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|         3 | 12579 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|         - | 12580 | `					/* Unexpected token */` |
|       ! 0 | 12581 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|       ! 0 | 12582 | `						&pGen->pIn->sData);` |
|       ! 0 | 12583 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12584 | `						return SXERR_ABORT;` |
|         - | 12585 | `					}` |
|         - | 12586 | `					/* FALL THROUGH */` |
|       ! 0 | 12587 | `				}` |
|         - | 12588 | `				/* Block compiled */` |
|         3 | 12589 | `				break;` |
|         - | 12590 | `			}` |
|         - | 12591 | `		}` |
|         - | 12592 | `		/* Compile block */` |
|       305 | 12593 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       305 | 12594 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12595 | `			return SXERR_ABORT;` |
|         - | 12596 | `		}` |
|         5 | 12597 | `	}` |
|       117 | 12598 | `	return rc;` |
|        61 | 12599 | `}` |
|         - | 12600 | `/*` |
|         - | 12601 | ` * Compile a case eXpression.` |
|         - | 12602 | ` *  (See block-comment below for more information)` |
|         - | 12603 | ` */` |
|        92 | 12604 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|         5 | 12605 | `{` |
|         - | 12606 | `	SySet *pInstrContainer;` |
|         - | 12607 | `	SyToken *pEnd,*pTmp;` |
|        97 | 12608 | `	sxi32 iNest = 0;` |
|         - | 12609 | `	sxi32 rc;` |
|         - | 12610 | `	/* Delimit the expression */` |
|        97 | 12611 | `	pEnd = pGen->pIn;` |
|       197 | 12612 | `	while( pEnd < pGen->pEnd ){` |
|       197 | 12613 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|         - | 12614 | `			/* Increment nesting level */` |
|         3 | 12615 | `			iNest++;` |
|       196 | 12616 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|         - | 12617 | `			/* Decrement nesting level */` |
|         3 | 12618 | `			iNest--;` |
|       194 | 12619 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|        97 | 12620 | `			break;` |
|         - | 12621 | `		}` |
|       105 | 12622 | `		pEnd++;` |
|         5 | 12623 | `	}` |
|        97 | 12624 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 | 12625 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|       ! 0 | 12626 | `		if( rc == SXERR_ABORT ){` |
|         - | 12627 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12628 | `			return SXERR_ABORT;` |
|         - | 12629 | `		}` |
|       ! 0 | 12630 | `	}` |
|         - | 12631 | `	/* Swap token stream */` |
|        97 | 12632 | `	pTmp = pGen->pEnd;` |
|        97 | 12633 | `	pGen->pEnd = pEnd;` |
|        97 | 12634 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        97 | 12635 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|        97 | 12636 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - | 12637 | `	/* Emit the done instruction */` |
|        97 | 12638 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        97 | 12639 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - | 12640 | `	/* Update token stream */` |
|        97 | 12641 | `	pGen->pIn  = pEnd;` |
|        97 | 12642 | `	pGen->pEnd = pTmp;` |
|        97 | 12643 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12644 | `		return SXERR_ABORT;` |
|         - | 12645 | `	}` |
|        97 | 12646 | `	return SXRET_OK;` |
|        51 | 12647 | `}` |
|         - | 12648 | `/*` |
|         - | 12649 | ` * Compile the smart switch statement.` |
|         - | 12650 | ` * According to the PHP language reference manual` |
|         - | 12651 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|         - | 12652 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|         - | 12653 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|         - | 12654 | ` *  This is exactly what the switch statement is for.` |
|         - | 12655 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|         - | 12656 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|         - | 12657 | ` *  of the outer loop, use continue 2.` |
|         - | 12658 | ` *  Note that switch/case does loose comparision.` |
|         - | 12659 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|         - | 12660 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|         - | 12661 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|         - | 12662 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|         - | 12663 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|         - | 12664 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|         - | 12665 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|         - | 12666 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|         - | 12667 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|         - | 12668 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|         - | 12669 | ` *  list for the next case.` |
|         - | 12670 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|         - | 12671 | ` *  or floating-point numbers and strings.` |
|         - | 12672 | ` */` |
|        28 | 12673 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|         5 | 12674 | `{` |
|         - | 12675 | `	GenBlock *pSwitchBlock;` |
|         - | 12676 | `	SyToken *pTmp,*pEnd;` |
|         - | 12677 | `	ph7_switch *pSwitch;` |
|         - | 12678 | `	sxu32 nToken;` |
|         - | 12679 | `	sxu32 nLine;` |
|         - | 12680 | `	sxi32 rc;` |
|        33 | 12681 | `	nLine = pGen->pIn->nLine;` |
|         - | 12682 | `	/* Jump the 'switch' keyword */` |
|        33 | 12683 | `	pGen->pIn++;` |
|        33 | 12684 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 12685 | `		/* Syntax error */` |
|       ! 0 | 12686 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|       ! 0 | 12687 | `		if( rc == SXERR_ABORT ){` |
|         - | 12688 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12689 | `			return SXERR_ABORT;` |
|         - | 12690 | `		}` |
|       ! 0 | 12691 | `		goto Synchronize;` |
|         - | 12692 | `	}` |
|         - | 12693 | `	/* Jump the left parenthesis '(' */` |
|        33 | 12694 | `	pGen->pIn++;` |
|        33 | 12695 | `	pEnd = 0; /* cc warning */` |
|         - | 12696 | `	/* Create the loop block */` |
|        47 | 12697 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|        14 | 12698 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|        33 | 12699 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12700 | `		return SXERR_ABORT;` |
|         - | 12701 | `	}` |
|         - | 12702 | `	/* Delimit the condition */` |
|        33 | 12703 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|        33 | 12704 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - | 12705 | `		/* Empty expression */` |
|       ! 0 | 12706 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|       ! 0 | 12707 | `		if( rc == SXERR_ABORT ){` |
|         - | 12708 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12709 | `			return SXERR_ABORT;` |
|         - | 12710 | `		}` |
|       ! 0 | 12711 | `	}` |
|         - | 12712 | `	/* Swap token streams */` |
|        33 | 12713 | `	pTmp = pGen->pEnd;` |
|        33 | 12714 | `	pGen->pEnd = pEnd;` |
|         - | 12715 | `	/* Compile the expression */` |
|        33 | 12716 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        33 | 12717 | `	if( rc == SXERR_ABORT ){` |
|         - | 12718 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 | 12719 | `		return SXERR_ABORT;` |
|         - | 12720 | `	}` |
|         - | 12721 | `	/* Update token stream */` |
|        33 | 12722 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 | 12723 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 12724 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 | 12725 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12726 | `			return SXERR_ABORT;` |
|         - | 12727 | `		}` |
|       ! 0 | 12728 | `		pGen->pIn++;` |
|       ! 0 | 12729 | `	}` |
|        33 | 12730 | `	pGen->pIn  = &pEnd[1];` |
|        33 | 12731 | `	pGen->pEnd = pTmp;` |
|        33 | 12732 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|        28 | 12733 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|       ! 0 | 12734 | `			pTmp = pGen->pIn;` |
|       ! 0 | 12735 | `			if( pTmp >= pGen->pEnd ){` |
|       ! 0 | 12736 | `				pTmp--;` |
|       ! 0 | 12737 | `			}` |
|         - | 12738 | `			/* Unexpected token */` |
|       ! 0 | 12739 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|       ! 0 | 12740 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12741 | `				return SXERR_ABORT;` |
|         - | 12742 | `			}` |
|       ! 0 | 12743 | `			goto Synchronize;` |
|         - | 12744 | `	}` |
|         - | 12745 | `	/* Set the delimiter token */` |
|        33 | 12746 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|         3 | 12747 | `		nToken = PH7_TK_KEYWORD;` |
|         - | 12748 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|         2 | 12749 | `	}else{` |
|        31 | 12750 | `		nToken = PH7_TK_CCB; /* '}' */` |
|         - | 12751 | `	}` |
|        33 | 12752 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|         - | 12753 | `	/* Create the switch blocks container */` |
|        33 | 12754 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|        33 | 12755 | `	if( pSwitch == 0 ){` |
|         - | 12756 | `		/* Abort compilation */` |
|       ! 0 | 12757 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 12758 | `		return SXERR_ABORT;` |
|         - | 12759 | `	}` |
|         - | 12760 | `	/* Zero the structure */` |
|        33 | 12761 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|         - | 12762 | `	/* Initialize fields */` |
|        33 | 12763 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|         - | 12764 | `	/* Emit the switch instruction */` |
|        33 | 12765 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|         - | 12766 | `	/* Compile case blocks */` |
|       100 | 12767 | `	for(;;){` |
|         - | 12768 | `		sxu32 nKwrd;` |
|       119 | 12769 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 12770 | `			/* No more input to process */` |
|       ! 0 | 12771 | `			break;` |
|         - | 12772 | `		}` |
|       119 | 12773 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 12774 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|         - | 12775 | `				/* Unexpected token */` |
|       ! 0 | 12776 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 12777 | `					&pGen->pIn->sData);` |
|       ! 0 | 12778 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12779 | `					return SXERR_ABORT;` |
|         - | 12780 | `				}` |
|         - | 12781 | `				/* FALL THROUGH */` |
|       ! 0 | 12782 | `			}` |
|         - | 12783 | `			/* Block compiled */` |
|       ! 0 | 12784 | `			break;` |
|         - | 12785 | `		}` |
|         - | 12786 | `		/* Extract the keyword */` |
|       119 | 12787 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       119 | 12788 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|         3 | 12789 | `			if( nToken != PH7_TK_KEYWORD ){` |
|         - | 12790 | `				/* Unexpected token */` |
|       ! 0 | 12791 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 12792 | `					&pGen->pIn->sData);` |
|       ! 0 | 12793 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12794 | `					return SXERR_ABORT;` |
|         - | 12795 | `				}` |
|         - | 12796 | `				/* FALL THROUGH */` |
|       ! 0 | 12797 | `			}` |
|         - | 12798 | `			/* Block compiled */` |
|         3 | 12799 | `			break;` |
|         - | 12800 | `		}` |
|       117 | 12801 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|         - | 12802 | `			/*` |
|         - | 12803 | `			 * Accroding to the PHP language reference manual` |
|         - | 12804 | `			 *  A special case is the default case. This case matches anything` |
|         - | 12805 | `			 *  that wasn't matched by the other cases.` |
|         - | 12806 | `			 */` |
|        25 | 12807 | `			if( pSwitch->nDefault > 0 ){` |
|         - | 12808 | `				/* Default case already compiled */` |
|       ! 0 | 12809 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|       ! 0 | 12810 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12811 | `					return SXERR_ABORT;` |
|         - | 12812 | `				}` |
|       ! 0 | 12813 | `			}` |
|        25 | 12814 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|         - | 12815 | `			/* Compile the default block */` |
|        25 | 12816 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|        25 | 12817 | `			if( rc == SXERR_ABORT){` |
|       ! 0 | 12818 | `				return SXERR_ABORT;` |
|        25 | 12819 | `			}else if( rc == SXERR_EOF ){` |
|        23 | 12820 | `				break;` |
|         1 | 12821 | `			}` |
|        98 | 12822 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|         - | 12823 | `			ph7_case_expr sCase;` |
|         - | 12824 | `			/* Standard case block */` |
|        97 | 12825 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|         - | 12826 | `			/* initialize the structure */` |
|        97 | 12827 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - | 12828 | `			/* Compile the case expression */` |
|        97 | 12829 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|        97 | 12830 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12831 | `				return SXERR_ABORT;` |
|         - | 12832 | `			}` |
|         - | 12833 | `			/* Compile the case block */` |
|        97 | 12834 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|         - | 12835 | `			/* Insert in the switch container */` |
|        97 | 12836 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|        97 | 12837 | `			if( rc == SXERR_ABORT){` |
|       ! 0 | 12838 | `				return SXERR_ABORT;` |
|        97 | 12839 | `			}else if( rc == SXERR_EOF ){` |
|         9 | 12840 | `				break;` |
|         - | 12841 | `			}` |
|        47 | 12842 | `		}else{` |
|         - | 12843 | `			/* Unexpected token */` |
|       ! 0 | 12844 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 12845 | `				&pGen->pIn->sData);` |
|       ! 0 | 12846 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12847 | `				return SXERR_ABORT;` |
|         - | 12848 | `			}` |
|       ! 0 | 12849 | `			break;` |
|         - | 12850 | `		}` |
|         5 | 12851 | `	}` |
|         - | 12852 | `	/* Fix all jumps now the destination is resolved */` |
|        33 | 12853 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|        33 | 12854 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12855 | `	/* Release the loop block */` |
|        33 | 12856 | `	GenStateLeaveBlock(pGen,0);` |
|        33 | 12857 | `	if( pGen->pIn < pGen->pEnd ){` |
|         - | 12858 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|        33 | 12859 | `		pGen->pIn++;` |
|        14 | 12860 | `	}` |
|         - | 12861 | `	/* Statement successfully compiled */` |
|        33 | 12862 | `	return SXRET_OK;` |
|       ! 0 | 12863 | `Synchronize:` |
|         - | 12864 | `	/* Synchronize with the first semi-colon */` |
|       ! 0 | 12865 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       ! 0 | 12866 | `		pGen->pIn++;` |
|       ! 0 | 12867 | `	}` |
|       ! 0 | 12868 | `	return SXRET_OK;` |
|        19 | 12869 | `}` |
|         - | 12870 | `/*` |
|         - | 12871 | ` * Chain operators participate in a postfix member-access chain.` |
|         - | 12872 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|         - | 12873 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|         - | 12874 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|         - | 12875 | ` */` |
|         - | 12876 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|         - | 12877 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|         - | 12878 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|         - | 12879 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|         - | 12880 |  |
|         - | 12881 | `/*` |
|         - | 12882 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|         - | 12883 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|         - | 12884 | ` * patched entries from the pending set.` |
|         - | 12885 | ` */` |
|  39637074 | 12886 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|         5 | 12887 | `{` |
|  39637079 | 12888 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|         - | 12889 | `	sxu32 nTarget;` |
|         - | 12890 | `	sxu32 *aIdx;` |
|         - | 12891 | `	sxu32 i;` |
|  39637079 | 12892 | `	if( nCur <= nBaseline ){` |
|  39636983 | 12893 | `		return;` |
|         - | 12894 | `	}` |
|       100 | 12895 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|       100 | 12896 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|       204 | 12897 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|       108 | 12898 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|       108 | 12899 | `		if( pInstr ){` |
|       108 | 12900 | `			pInstr->iP2 = (sxi32)nTarget;` |
|        52 | 12901 | `		}` |
|        56 | 12902 | `	}` |
|       100 | 12903 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
|  19818542 | 12904 | `}` |
|         - | 12905 |  |
|         - | 12906 | `/*` |
|         - | 12907 | ` * By-reference out-parameters of builtin functions.` |
|         - | 12908 | ` *` |
|         - | 12909 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|         - | 12910 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|         - | 12911 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|         - | 12912 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|         - | 12913 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|         - | 12914 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|         - | 12915 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|         - | 12916 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|         - | 12917 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|         - | 12918 | ` * creates it" behaviour).` |
|         - | 12919 | ` *` |
|         - | 12920 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|         - | 12921 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|         - | 12922 | ` */` |
|   5384836 | 12923 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|         5 | 12924 | `{` |
|         - | 12925 | `	static const struct {` |
|         - | 12926 | `		const char *zName;` |
|         - | 12927 | `		sxu32 nByte;` |
|         - | 12928 | `		sxu32 mask;` |
|         - | 12929 | `	} aByRef[] = {` |
|         - | 12930 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - | 12931 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - | 12932 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - | 12933 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - | 12934 | `		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */` |
|         - | 12935 | `		{ "fsockopen",              9, (1u<<2)\|(1u<<3) },  /* &$error_code, &$error_message */` |
|         - | 12936 | `		{ "pfsockopen",            10, (1u<<2)\|(1u<<3) },  /* same */` |
|         - | 12937 | `		{ "stream_socket_client",  20, (1u<<1)\|(1u<<2) },  /* &$error_code, &$error_message */` |
|         - | 12938 | `	};` |
|         - | 12939 | `	sxu32 i;` |
|   5384841 | 12940 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|   1587537 | 12941 | `		return 0;` |
|         - | 12942 | `	}` |
|  33882659 | 12943 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
|  30123440 | 12944 | `		if( pName->nByte == aByRef[i].nByte` |
|  15741064 | 12945 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|     38095 | 12946 | `			return aByRef[i].mask;` |
|         - | 12947 | `		}` |
|  15042680 | 12948 | `	}` |
|   3759219 | 12949 | `	return 0;` |
|   2692423 | 12950 | `}` |
|         - | 12951 | `/*` |
|         - | 12952 | ` * Recover the bare global-builtin name from a call's callee node.` |
|         - | 12953 | ` *` |
|         - | 12954 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|         - | 12955 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|         - | 12956 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|         - | 12957 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|         - | 12958 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|         - | 12959 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|         - | 12960 | ` */` |
|   5384836 | 12961 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|         5 | 12962 | `{` |
|         - | 12963 | `	SyToken *p, *pEnd;` |
|   5384841 | 12964 | `	pOut->zString = 0;` |
|   5384841 | 12965 | `	pOut->nByte = 0;` |
|   5384841 | 12966 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|       ! 0 | 12967 | `		return;` |
|         - | 12968 | `	}` |
|   5384841 | 12969 | `	p = pLeft->pStart;` |
|   5384841 | 12970 | `	pEnd = pLeft->pEnd;` |
|         - | 12971 | `	/* Optional single leading namespace separator (absolute path). */` |
|   5384841 | 12972 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      3829 | 12973 | `		p++;` |
|      1912 | 12974 | `	}` |
|   5384841 | 12975 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|   1587501 | 12976 | `		return;` |
|         - | 12977 | `	}` |
|         - | 12978 | `	/* Must be a single component: nothing follows the name token. */` |
|   3797345 | 12979 | `	if( p + 1 != pEnd ){` |
|        40 | 12980 | `		return;` |
|         - | 12981 | `	}` |
|   3797309 | 12982 | `	*pOut = p->sData;` |
|   2692423 | 12983 | `}` |
|         - | 12984 | `/*` |
|         - | 12985 | ` * Generate bytecode for a given expression tree.` |
|         - | 12986 | ` * If something goes wrong while generating bytecode` |
|         - | 12987 | ` * for the expression tree (A very unlikely scenario)` |
|         - | 12988 | ` * this function takes care of generating the appropriate` |
|         - | 12989 | ` * error message.` |
|         - | 12990 | ` */` |
|  57262422 | 12991 | `static sxi32 GenStateEmitExprCode(` |
|         - | 12992 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 12993 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|         - | 12994 | `	sxi32 iFlags /* Control flags */` |
|         - | 12995 | `	)` |
|         5 | 12996 | `{` |
|         - | 12997 | `	VmInstr *pInstr;` |
|         - | 12998 | `	sxu32 nJmpIdx;` |
|  57262427 | 12999 | `	sxi32 iP1 = 0;` |
|  57262427 | 13000 | `	sxu32 iP2 = 0;` |
|  57262427 | 13001 | `	void *p3  = 0;` |
|         - | 13002 | `	sxi32 iVmOp;` |
|         - | 13003 | `	sxi32 rc;` |
|  57262427 | 13004 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
|  57262427 | 13005 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
|  57262427 | 13006 | `	sxu32 nRhsNsBase = 0;` |
|  57262427 | 13007 | `	if( pNode->xCode ){` |
|         - | 13008 | `		SyToken *pTmpIn,*pTmpEnd;` |
|         - | 13009 | `		/* Compile node */` |
|  34249921 | 13010 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|  34249921 | 13011 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|  34249921 | 13012 | `		RE_SWAP_DELIMITER(pGen);` |
|  34249921 | 13013 | `		return rc;` |
|         - | 13014 | `	}` |
|  23012511 | 13015 | `	if( pNode->pOp == 0 ){` |
|       ! 0 | 13016 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13017 | `			"Invalid expression node,PH7 is aborting compilation");` |
|       ! 0 | 13018 | `		return SXERR_ABORT;` |
|         - | 13019 | `	}` |
|  23012511 | 13020 | `	iVmOp = pNode->pOp->iVmOp;` |
|  23012511 | 13021 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|         - | 13022 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|         - | 13023 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|         - | 13024 | `		 * and later errors are still reported. */` |
|         3 | 13025 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13026 | `			"The (unset) cast is no longer supported");` |
|         3 | 13027 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 13028 | `			return SXERR_ABORT;` |
|         - | 13029 | `		}` |
|         1 | 13030 | `	}` |
|  23012511 | 13031 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        91 | 13032 | `		sxu32 nJmp = 0;` |
|         - | 13033 | `		sxu32 nNcNsBase;` |
|         - | 13034 | `		VmInstr *pInstrFix;` |
|         - | 13035 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|         - | 13036 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|         - | 13037 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|         - | 13038 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|         - | 13039 | `		 * stack slot carries a writable nIdx. */` |
|        91 | 13040 | `		if( pNode->pRight ){` |
|        91 | 13041 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        91 | 13042 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|        91 | 13043 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13044 | `				return rc;` |
|         - | 13045 | `			}` |
|        91 | 13046 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|         - | 13047 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|         - | 13048 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|         - | 13049 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|         - | 13050 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|         - | 13051 | `			 * the store, so the parent array does not need to be copied at` |
|         - | 13052 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|         - | 13053 | `			 * cascade for the actual write path stays correct. */` |
|        91 | 13054 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|        91 | 13055 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|        33 | 13056 | `				pInstrFix->iP2 = 3;` |
|        15 | 13057 | `			}` |
|        44 | 13058 | `		}` |
|         - | 13059 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|        91 | 13060 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|         - | 13061 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|        91 | 13062 | `		if( pNode->pLeft ){` |
|        91 | 13063 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        91 | 13064 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|        91 | 13065 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13066 | `				return rc;` |
|         - | 13067 | `			}` |
|        91 | 13068 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        44 | 13069 | `		}` |
|         - | 13070 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|        91 | 13071 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|         - | 13072 | `		/* Patch the short-circuit jump to land after the store. */` |
|        91 | 13073 | `		if( nJmp > 0 ){` |
|        91 | 13074 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|        91 | 13075 | `			if( pInstrFix ){` |
|        91 | 13076 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|        44 | 13077 | `			}` |
|        44 | 13078 | `		}` |
|        91 | 13079 | `		return SXRET_OK;` |
|         - | 13080 | `	}` |
|  23012423 | 13081 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|         - | 13082 | `		sxu32 nJz,nJmp;` |
|         - | 13083 | `		sxu32 nTernaryNsBase;` |
|         - | 13084 | `		/* Ternary operator require special handling */` |
|         - | 13085 | `		/* Phase#1: Compile the condition */` |
|    367711 | 13086 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    367711 | 13087 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    367711 | 13088 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13089 | `			return rc;` |
|         - | 13090 | `		}` |
|         - | 13091 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|         - | 13092 | `		 * compiling the condition must short-circuit to the end of the` |
|         - | 13093 | `		 * condition expression, not leak past the ternary. */` |
|    367711 | 13094 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    367711 | 13095 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    367711 | 13096 | `		if( pNode->pLeft ){` |
|         - | 13097 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|         - | 13098 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    363847 | 13099 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 13100 | `			/* Phase#3: Compile the 'then' expression  */` |
|    363847 | 13101 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    363847 | 13102 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    363847 | 13103 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13104 | `				return rc;` |
|         - | 13105 | `			}` |
|    363847 | 13106 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    181926 | 13107 | `		}else{` |
|         - | 13108 | `			/* Elvis operator: (expr) ?: (else)` |
|         - | 13109 | `			 * Duplicate condition so original value is the 'then' result.` |
|         - | 13110 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      3869 | 13111 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      3869 | 13112 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 13113 | `		}` |
|         - | 13114 | `		/* Phase#4: Emit the unconditional jump */` |
|    367711 | 13115 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|         - | 13116 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    367711 | 13117 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    367711 | 13118 | `		if( pInstr ){` |
|    367711 | 13119 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    183853 | 13120 | `		}` |
|    367711 | 13121 | `		if( !pNode->pLeft ){` |
|         - | 13122 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      3869 | 13123 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      1932 | 13124 | `		}` |
|         - | 13125 | `		/* Phase#6: Compile the 'else' expression */` |
|    367711 | 13126 | `		if( pNode->pRight ){` |
|    367711 | 13127 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    367711 | 13128 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    367711 | 13129 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13130 | `				return rc;` |
|         - | 13131 | `			}` |
|    367711 | 13132 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    183853 | 13133 | `		}` |
|    367711 | 13134 | `		if( nJmp > 0 ){` |
|         - | 13135 | `			/* Phase#7: Fix the unconditional jump */` |
|    367711 | 13136 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    367711 | 13137 | `			if( pInstr ){` |
|    367711 | 13138 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    183853 | 13139 | `			}` |
|    183853 | 13140 | `		}` |
|         - | 13141 | `		/* All done */` |
|    367711 | 13142 | `		return SXRET_OK;` |
|         - | 13143 | `	}` |
|  22644717 | 13144 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|         - | 13145 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|         - | 13146 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|         - | 13147 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|         - | 13148 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|         - | 13149 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|         - | 13150 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|         - | 13151 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|         - | 13152 | `		sxu32 nPipeNsBase;` |
|        27 | 13153 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|        27 | 13154 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|       ! 0 | 13155 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13156 | `				"'\|>': Missing operand");` |
|       ! 0 | 13157 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 13158 | `		}` |
|         - | 13159 | `		/* Argument: the LHS value. */` |
|        27 | 13160 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 13161 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|        27 | 13162 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13163 | `			return rc;` |
|         - | 13164 | `		}` |
|        27 | 13165 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 13166 | `		/* Callable: the RHS. */` |
|        27 | 13167 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 13168 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|        27 | 13169 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13170 | `			return rc;` |
|         - | 13171 | `		}` |
|        27 | 13172 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 13173 | `		/* Invoke the callable with the single piped argument. */` |
|        27 | 13174 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        27 | 13175 | `		return SXRET_OK;` |
|         - | 13176 | `	}` |
|  22644691 | 13177 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|         - | 13178 | `	/* Generate code for the left tree */` |
|  22644691 | 13179 | `	if( pNode->pLeft ){` |
|  22621913 | 13180 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  22621913 | 13181 | `		if( iVmOp == PH7_OP_CALL ){` |
|         - | 13182 | `			ph7_expr_node **apNode;` |
|   5388953 | 13183 | `			int hasSpread = 0;` |
|   5388953 | 13184 | `			int hasNamed = 0;` |
|   5388953 | 13185 | `			int bAnySpread = 0;` |
|   5388953 | 13186 | `			sxu32 byRefMask = 0;` |
|         - | 13187 | `			sxi32 nArgs;` |
|         - | 13188 | `			sxi32 n;` |
|         - | 13189 | `			/* Recurse and generate bytecodes for function arguments */` |
|   5388953 | 13190 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   5388953 | 13191 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|         - | 13192 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|         - | 13193 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|         - | 13194 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|   5388953 | 13195 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|        81 | 13196 | `				bFcc = 1;` |
|        81 | 13197 | `				nArgs = 0;` |
|        40 | 13198 | `			}` |
|         - | 13199 | `			/* Validate argument order like php: no positional argument after a` |
|         - | 13200 | ``			 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|         - | 13201 | `			{` |
|   5388953 | 13202 | `				int seenNamed = 0;` |
|   5388953 | 13203 | `				int seenSpread = 0;` |
|  11004115 | 13204 | `				for( n = 0; n < nArgs; ++n ){` |
|   5615169 | 13205 | `					if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      3985 | 13206 | `						bAnySpread = 1;` |
|      3985 | 13207 | `						seenSpread = 1;` |
|      3985 | 13208 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       ! 0 | 13209 | `							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13210 | `								"syntax error, unexpected token \"...\"");` |
|       ! 0 | 13211 | `							return SXERR_SYNTAX;` |
|         5 | 13212 | `						}` |
|   5613179 | 13213 | `					}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       289 | 13214 | `						seenNamed = 1;` |
|       289 | 13215 | `						hasNamed = 1;` |
|   5611047 | 13216 | `					}else if( seenNamed ){` |
|         3 | 13217 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13218 | `							"Cannot use positional argument after named argument");` |
|         3 | 13219 | `						return SXERR_SYNTAX;` |
|   5610903 | 13220 | `					}else if( seenSpread ){` |
|       ! 0 | 13221 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13222 | `							"Cannot use positional argument after argument unpacking");` |
|       ! 0 | 13223 | `						return SXERR_SYNTAX;` |
|         - | 13224 | `					}` |
|   2807586 | 13225 | `				}` |
|         - | 13226 | `			}` |
|         - | 13227 | `			/* Read-only load */` |
|   5388951 | 13228 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|         - | 13229 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|         - | 13230 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|         - | 13231 | `			 * objects dispatch to the right method (offsetExists for both;` |
|         - | 13232 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|   5388951 | 13233 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|   5388951 | 13234 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|   5388946 | 13235 | `				if( pCallName->nByte == 5` |
|   3029858 | 13236 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|    273697 | 13237 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|   5252105 | 13238 | `				}else if( pCallName->nByte == 5` |
|   2756166 | 13239 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|       107 | 13240 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|        51 | 13241 | `				}` |
|         - | 13242 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|         - | 13243 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|         - | 13244 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|         - | 13245 | `				 * write back through. Skipped when spread/named args are present:` |
|         - | 13246 | `				 * the compile-time positional index no longer maps to the` |
|         - | 13247 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|   5388951 | 13248 | `				if( !bAnySpread && !hasNamed ){` |
|         - | 13249 | `					SyString sBuiltin;` |
|   5384841 | 13250 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|   5384841 | 13251 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|   2692418 | 13252 | `				}` |
|   2694473 | 13253 | `			}` |
|  11004111 | 13254 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|   5615165 | 13255 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   5615165 | 13256 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 13257 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|         - | 13258 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|         - | 13259 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|         - | 13260 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|         - | 13261 | `				 * builtin to write back through. A plain $var target is unaffected` |
|         - | 13262 | `				 * (iP1=0 either way). */` |
|   5615165 | 13263 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|     26645 | 13264 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|     26645 | 13265 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|     13320 | 13266 | `				}` |
|   5615165 | 13267 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|   5615165 | 13268 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 13269 | `					return rc;` |
|         - | 13270 | `				}` |
|         - | 13271 | `				/* Each argument is an independent nullsafe scope. */` |
|   5615165 | 13272 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|   5615165 | 13273 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|         - | 13274 | `					/* Emit spread opcode to unpack this array argument */` |
|      3985 | 13275 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      3985 | 13276 | `					hasSpread = 1;` |
|      1990 | 13277 | `				}` |
|   2807585 | 13278 | `			}` |
|         - | 13279 | `			/* Total number of given arguments */` |
|   5388951 | 13280 | `			iP1 = nArgs;` |
|   5388951 | 13281 | `			iP2 = hasSpread;` |
|         - | 13282 | `			/* Build VmCallArgMap if named arguments are present.` |
|         - | 13283 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|   5388951 | 13284 | `			if( hasNamed ){` |
|       178 | 13285 | `				sxu32 nStrBytes = 0;` |
|         - | 13286 | `				char *zBuf;` |
|       534 | 13287 | `				for( n = 0; n < nArgs; ++n ){` |
|       360 | 13288 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       286 | 13289 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|       141 | 13290 | `					}` |
|       182 | 13291 | `				}` |
|         - | 13292 | `				{` |
|       178 | 13293 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|       178 | 13294 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       174 | 13295 | `					&pGen->pVm->sAllocator, mapSize);` |
|       178 | 13296 | `				if( pMap ){` |
|       178 | 13297 | `					SyZero(pMap, mapSize);` |
|       178 | 13298 | `					pMap->bHasNamed = 1;` |
|       178 | 13299 | `					pMap->nTotal = (sxu32)nArgs;` |
|       178 | 13300 | `					pMap->aNames = (SyString *)&pMap[1];` |
|       178 | 13301 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|       534 | 13302 | `					for( n = 0; n < nArgs; ++n ){` |
|       360 | 13303 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       286 | 13304 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|       286 | 13305 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|       286 | 13306 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|       286 | 13307 | `							zBuf += nb;` |
|       141 | 13308 | `						}` |
|         - | 13309 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|       182 | 13310 | `					}` |
|       178 | 13311 | `					p3 = (void *)pMap;` |
|        87 | 13312 | `				}` |
|         - | 13313 | `				}` |
|        87 | 13314 | `			}` |
|         - | 13315 | `			/* Remove stale flags now */` |
|   5388951 | 13316 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|   2694473 | 13317 | `		}` |
|         - | 13318 | `		{` |
|         - | 13319 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|         - | 13320 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|         - | 13321 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|         - | 13322 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|         - | 13323 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|         - | 13324 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|         - | 13325 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|         - | 13326 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
|  22621911 | 13327 | `			sxi32 iLeftFlags = iFlags;` |
|  22621906 | 13328 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  18611890 | 13329 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|   7300963 | 13330 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   6194239 | 13331 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   2384769 | 13332 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   1192382 | 13333 | `			}` |
|         - | 13334 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|         - | 13335 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|         - | 13336 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|         - | 13337 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|         - | 13338 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|         - | 13339 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|         - | 13340 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
|  22621906 | 13341 | `			if( pNode->pOp` |
|  31682575 | 13342 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
|  20371669 | 13343 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  18121380 | 13344 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|   4858289 | 13345 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   2429142 | 13346 | `			}` |
|         - | 13347 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|         - | 13348 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|         - | 13349 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|         - | 13350 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|         - | 13351 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|         - | 13352 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
|  22621906 | 13353 | `			if( pNode->pOp` |
|  22621911 | 13354 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|    144745 | 13355 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|     72370 | 13356 | `			}` |
|  22621911 | 13357 | `			if( iVmOp == PH7_OP_ERR_CTRL ){` |
|         - | 13358 | `				/* '@' must suppress the diagnostics raised WHILE its operand runs, so` |
|         - | 13359 | `				 * open the window here; the trailing emit below closes it (iP1 = 0). */` |
|       211 | 13360 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);` |
|       103 | 13361 | `			}` |
|  22621911 | 13362 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|         - | 13363 | `		}` |
|  22621911 | 13364 | `		if( rc != SXRET_OK ){` |
|        34 | 13365 | `			return rc;` |
|         - | 13366 | `		}` |
|  22621881 | 13367 | `		if( !bIsChainOp ){` |
|         - | 13368 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|         - | 13369 | `			 * target the end of that LHS chain, which is right here. */` |
|   9877567 | 13370 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|   4938781 | 13371 | `		}` |
|  22621881 | 13372 | `		if( iVmOp == PH7_OP_CALL ){` |
|   5388951 | 13373 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   5388951 | 13374 | `			if( pInstr ){` |
|   5388951 | 13375 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|   3797585 | 13376 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|         - | 13377 | `					sxu32 nQual;` |
|   3797585 | 13378 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 13379 | `					/* Prevent constant expansion but preserve the absolute flag` |
|         - | 13380 | `					 * so the later NEW handler (if any) can see it. */` |
|   3797585 | 13381 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|         - | 13382 | `					/* Namespace-qualify the function name for CALL, unless the` |
|         - | 13383 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|         - | 13384 | `					 * imports — class imports must NOT affect function` |
|         - | 13385 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|         - | 13386 | `					 * before NEW; we store the original literal index in the` |
|         - | 13387 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|         - | 13388 | `					 * the unqualified name and re-qualify with class imports. */` |
|   3797585 | 13389 | `					if( bAbsolute ){` |
|      3829 | 13390 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      1917 | 13391 | `					}else{` |
|   3793761 | 13392 | `						int fromImport = 0;` |
|   3793761 | 13393 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|   3793761 | 13394 | `						pInstr->iP2 = (sxi32)nQual;` |
|   3793761 | 13395 | `						if( nQual != nOrig ){` |
|         - | 13396 | `							/* Record the original literal index in the arg map` |
|         - | 13397 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|         - | 13398 | `							 * flag) so the NEW handler can recover the` |
|         - | 13399 | `							 * unqualified name and re-qualify with CLASS` |
|         - | 13400 | `							 * imports. */` |
|        77 | 13401 | `							if( p3 == 0 ){` |
|        77 | 13402 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|        72 | 13403 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|        77 | 13404 | `								if( pMap ){` |
|        77 | 13405 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|        77 | 13406 | `									p3 = (void *)pMap;` |
|        36 | 13407 | `								}` |
|        36 | 13408 | `							}` |
|        77 | 13409 | `							if( p3 ){` |
|        77 | 13410 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|        77 | 13411 | `								if( !fromImport ){` |
|         - | 13412 | `									/* Mark as namespace-qualified */` |
|        67 | 13413 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|        31 | 13414 | `								}` |
|        36 | 13415 | `							}` |
|        36 | 13416 | `						}` |
|         5 | 13417 | `					}` |
|   3490161 | 13418 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|         - | 13419 | `					/* Method call,flag that */` |
|   1571779 | 13420 | `					pInstr->iP2 = 1;` |
|    785887 | 13421 | `				}` |
|   2694478 | 13422 | `			}` |
|  19927408 | 13423 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|         - | 13424 | `			ph7_expr_node **apNode;` |
|         - | 13425 | `			sxi32 n;` |
|   2497089 | 13426 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|         - | 13427 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|         - | 13428 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 13429 | `			/* Recurse and generate bytecodes for array index */` |
|   2497089 | 13430 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   4838317 | 13431 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   2341233 | 13432 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   2341233 | 13433 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   2341233 | 13434 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 13435 | `					return rc;` |
|         - | 13436 | `				}` |
|         - | 13437 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   2341233 | 13438 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   1170619 | 13439 | `			}` |
|   2497089 | 13440 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   2341233 | 13441 | `				iP1 = 1; /* Node have an index associated with it */` |
|   1170614 | 13442 | `			}` |
|   2497089 | 13443 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|         - | 13444 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|    311541 | 13445 | `				iP2 = 4;` |
|   2341321 | 13446 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|         - | 13447 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|         - | 13448 | `				 * so the trailing unset() builtin can drop the slot. */` |
|     22857 | 13449 | `				iP2 = 5;` |
|   2174127 | 13450 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|         - | 13451 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|         - | 13452 | `				 * short-circuit on missing keys without invoking offsetGet` |
|         - | 13453 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|        29 | 13454 | `				iP2 = 6;` |
|   2162689 | 13455 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|         - | 13456 | `				/* Create an empty entry when the desired index is not found */` |
|    365163 | 13457 | `				iP2 = 1;` |
|    182584 | 13458 | `			}` |
|  15984393 | 13459 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         - | 13460 | `			/* POP the left node */` |
|         5 | 13461 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 | 13462 | `		}` |
|  11310938 | 13463 | `	}` |
|  22644659 | 13464 | `	rc = SXRET_OK;` |
|  22644659 | 13465 | `	nJmpIdx = 0;` |
|         - | 13466 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|         - | 13467 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|         - | 13468 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  22644659 | 13469 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|    380489 | 13470 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    380489 | 13471 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    380489 | 13472 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    380489 | 13473 | `			int isSpecial = 0;` |
|    380489 | 13474 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|    334905 | 13475 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|    334905 | 13476 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|    334900 | 13477 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    304472 | 13478 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    165524 | 13479 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|    102641 | 13480 | `					isSpecial = 1;` |
|     51318 | 13481 | `				}` |
|    178846 | 13482 | `			}` |
|    403281 | 13483 | `			pInstr->iP1 = 0;` |
|    403281 | 13484 | `			if( !isSpecial ){` |
|    255061 | 13485 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    127528 | 13486 | `			}` |
|         - | 13487 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|         - | 13488 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|    357697 | 13489 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    255061 | 13490 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    255061 | 13491 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|        60 | 13492 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|        62 | 13493 | `					return SXRET_OK;` |
|         - | 13494 | `				}` |
|    127499 | 13495 | `			}` |
|    178817 | 13496 | `		}` |
|    224380 | 13497 | `	}` |
|         - | 13498 | `	/* Generate code for the right tree */` |
|  22621823 | 13499 | `	if( pNode->pRight ){` |
|  13003319 | 13500 | `		if( iVmOp == PH7_OP_LAND ){` |
|         - | 13501 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    311793 | 13502 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  12847425 | 13503 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|         - | 13504 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    212751 | 13505 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  12585158 | 13506 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|         - | 13507 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     53299 | 13508 | `			iVmOp = 0; /* No binary operator to emit */` |
|     53299 | 13509 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  12452190 | 13510 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         - | 13511 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|         - | 13512 | `			 * the entire containing postfix chain to null. The jump target is` |
|         - | 13513 | `			 * patched later by the innermost non-chain ancestor (or by` |
|         - | 13514 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|         - | 13515 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|         - | 13516 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|       108 | 13517 | `			sxu32 nNsJmp = 0;` |
|       108 | 13518 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|       108 | 13519 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  12425439 | 13520 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|         - | 13521 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|         - | 13522 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|         - | 13523 | `			 * auto-created — PHP auto-vivifies on write. */` |
|   3948097 | 13524 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|   1974046 | 13525 | `		}` |
|  13003319 | 13526 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  13003319 | 13527 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  13003319 | 13528 | `		if( !bIsChainOp ){` |
|         - | 13529 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|         - | 13530 | `			 * operator instruction is emitted. */` |
|   8145093 | 13531 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|   4072544 | 13532 | `		}` |
|  13003319 | 13533 | `		if( iVmOp == PH7_OP_STORE ){` |
|   3537803 | 13534 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|   3537766 | 13535 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|         - | 13536 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|         - | 13537 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|         - | 13538 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|         - | 13539 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|         - | 13540 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|         - | 13541 | `				 */` |
|        91 | 13542 | `				iVmOp = 0;` |
|   3537760 | 13543 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|   3537717 | 13544 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 13545 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|    763709 | 13546 | `					iP2 = 1;` |
|    381857 | 13547 | `				}else{` |
|   2774013 | 13548 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 13549 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|    346085 | 13550 | `						iVmOp = PH7_OP_STORE_IDX;` |
|    346085 | 13551 | `						iP1 = pInstr->iP1;` |
|    173045 | 13552 | `					}else{` |
|   2427933 | 13553 | `						p3 = pInstr->p3;` |
|         - | 13554 | `					}` |
|         - | 13555 | `					/* POP the last dynamic load instruction */` |
|   2774013 | 13556 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - | 13557 | `				}` |
|   1768861 | 13558 | `			}` |
|  11234420 | 13559 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|        63 | 13560 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|        63 | 13561 | `			if( pInstr ){` |
|        63 | 13562 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 13563 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|         - | 13564 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|         - | 13565 | `					 */` |
|        19 | 13566 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|        19 | 13567 | `					iP1 = pInstr->iP1;` |
|        19 | 13568 | `					iP2 = pInstr->iP2;` |
|        19 | 13569 | `					p3  = pInstr->p3;` |
|        10 | 13570 | `				}else{` |
|        45 | 13571 | `					p3 = pInstr->p3;` |
|         - | 13572 | `				}` |
|        30 | 13573 | `			}` |
|        30 | 13574 | `		}` |
|   6501657 | 13575 | `	}` |
|  22621818 | 13576 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|    354640 | 13577 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|         - | 13578 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|         - | 13579 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|        32 | 13580 | `		iVmOp = 0;` |
|        14 | 13581 | `	}` |
|  22621823 | 13582 | `	if( iVmOp > 0 ){` |
|  22568411 | 13583 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    144745 | 13584 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|         - | 13585 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|     11425 | 13586 | `				iP1 = 1;` |
|      5715 | 13587 | `			}` |
|  22496041 | 13588 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|         - | 13589 | `			/* Namespace-qualify the class name for NEW */ {` |
|    708923 | 13590 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|    708923 | 13591 | `				VmInstr *pCallInstr = 0;` |
|    708923 | 13592 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|    708627 | 13593 | `					pCallInstr = pPeek;` |
|    708627 | 13594 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    354311 | 13595 | `				}` |
|    708923 | 13596 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|    693735 | 13597 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 13598 | `					sxu32 nLitForClass;` |
|    693735 | 13599 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|         - | 13600 | `					/* If the CALL handler qualified the name with FUNCTION` |
|         - | 13601 | `					 * imports, recover the original literal (recorded in the` |
|         - | 13602 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|         - | 13603 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|         - | 13604 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|         - | 13605 | `					 * with class imports. */` |
|    693735 | 13606 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|        37 | 13607 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|        21 | 13608 | `					}else{` |
|    693703 | 13609 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|         - | 13610 | `					}` |
|    693735 | 13611 | `					pPeek->iP1 = 0;` |
|    693735 | 13612 | `					if( !bAbsolute ){` |
|    689915 | 13613 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    344960 | 13614 | `					}else{` |
|      3825 | 13615 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|         - | 13616 | `					}` |
|    346865 | 13617 | `				}` |
|         - | 13618 | `			}` |
|    708923 | 13619 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    708923 | 13620 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|         - | 13621 | `				VmInstr *pPrev;` |
|    708627 | 13622 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|    708627 | 13623 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|         - | 13624 | `					/* Pop the call instruction, preserve named-arg map and` |
|         - | 13625 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|         - | 13626 | `					 * accumulator exactly like OP_CALL would have). */` |
|    708627 | 13627 | `					iP1 = pInstr->iP1;` |
|    708627 | 13628 | `					iP2 = pInstr->iP2;` |
|    708627 | 13629 | `					if( pInstr->p3 ){` |
|        47 | 13630 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|        21 | 13631 | `					}` |
|    708627 | 13632 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    354311 | 13633 | `				}` |
|    354316 | 13634 | `			}` |
|  22069212 | 13635 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|         - | 13636 | `			/* instanceof: right operand is a class name, not a constant.` |
|         - | 13637 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     68585 | 13638 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     68585 | 13639 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     68585 | 13640 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     68585 | 13641 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     68585 | 13642 | `				int isSpecialIs = 0;` |
|     68585 | 13643 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     68585 | 13644 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     68585 | 13645 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     68580 | 13646 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     68583 | 13647 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     34290 | 13648 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        12 | 13649 | `						isSpecialIs = 1;` |
|         5 | 13650 | `					}` |
|     34290 | 13651 | `				}` |
|     68585 | 13652 | `				pInstr->iP1 = 0;` |
|     68585 | 13653 | `				if( !isSpecialIs && !bAbsolute ){` |
|     68565 | 13654 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     34280 | 13655 | `				}` |
|     34295 | 13656 | `			}` |
|  21680463 | 13657 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|         - | 13658 | `			/* Prevent constant expansion for member/property names.` |
|         - | 13659 | `			 * The right child (member name) was just compiled — its LOADC` |
|         - | 13660 | `			 * should not trigger constant lookup. */` |
|   4858231 | 13661 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   4858231 | 13662 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   4626597 | 13663 | `				pInstr->iP1 = 0;` |
|   2313296 | 13664 | `			}` |
|   4858231 | 13665 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|         - | 13666 | `				/* Static member access,remember that */` |
|    357653 | 13667 | `				iP1 = 1;` |
|    357653 | 13668 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    357653 | 13669 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|    227827 | 13670 | `					p3 = pInstr->p3;` |
|    227827 | 13671 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    113911 | 13672 | `				}` |
|    178824 | 13673 | `			}` |
|         - | 13674 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|         - | 13675 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|         - | 13676 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|         - | 13677 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|   4858231 | 13678 | `			if( iP2 == PH7_MEMBER_READ ){` |
|   4858231 | 13679 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        42 | 13680 | `					iP2 = PH7_MEMBER_UNSET;` |
|   4858211 | 13681 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|     60839 | 13682 | `					iP2 = PH7_MEMBER_ISSET;` |
|   4827774 | 13683 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        17 | 13684 | `					iP2 = PH7_MEMBER_EMPTY;` |
|   4797349 | 13685 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|         - | 13686 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|    930917 | 13687 | `					iP2 = PH7_MEMBER_WRITE;` |
|    465456 | 13688 | `				}` |
|   2429113 | 13689 | `			}` |
|   2429113 | 13690 | `		}` |
|         - | 13691 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|         - | 13692 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|         - | 13693 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|         - | 13694 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|         - | 13695 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
|  22568411 | 13696 | `		if( bFcc ){` |
|        81 | 13697 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|        81 | 13698 | `			iP2 = 0;` |
|        81 | 13699 | `			p3 = 0;` |
|        81 | 13700 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|        81 | 13701 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 13702 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|         - | 13703 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|         - | 13704 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|         - | 13705 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|        37 | 13706 | `				void *pMemberName = pInstr->p3;` |
|        37 | 13707 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|        37 | 13708 | `				if( pMemberName ){` |
|         3 | 13709 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|         1 | 13710 | `				}` |
|        37 | 13711 | `				iP1 = 2;` |
|        19 | 13712 | `			}else{` |
|        45 | 13713 | `				iP1 = 1;` |
|         - | 13714 | `			}` |
|        40 | 13715 | `		}` |
|         - | 13716 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|         - | 13717 | `		 * This is the primary emit path for user-visible calls. */` |
|  22568411 | 13718 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|   6097789 | 13719 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|   3048892 | 13720 | `		}` |
|         - | 13721 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  22568411 | 13722 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  11284203 | 13723 | `	}` |
|  22621823 | 13724 | `	if( nJmpIdx > 0 ){` |
|         - | 13725 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|    577833 | 13726 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|    577833 | 13727 | `		if( pInstr ){` |
|    577833 | 13728 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    288914 | 13729 | `		}` |
|    288914 | 13730 | `	}` |
|  22621823 | 13731 | `	return rc;` |
|  28619827 | 13732 | `}` |
|         - | 13733 | `/*` |
|         - | 13734 | ` * Compile a PHP expression.` |
|         - | 13735 | ` * According to the PHP language reference manual:` |
|         - | 13736 | ` *  Expressions are the most important building stones of PHP.` |
|         - | 13737 | ` *  In PHP, almost anything you write is an expression.` |
|         - | 13738 | ` *  The simplest yet most accurate way to define an expression` |
|         - | 13739 | ` *  is "anything that has a value".` |
|         - | 13740 | ` * If something goes wrong while compiling the expression,this` |
|         - | 13741 | ` * function takes care of generating the appropriate error` |
|         - | 13742 | ` * message.` |
|         - | 13743 | ` */` |
|         - | 13744 | `/*` |
|         - | 13745 | ` * Does this expression tree contain a comma OPERATOR node?` |
|         - | 13746 | ` *` |
|         - | 13747 | `` * PH7 shipped `,` as a lowest-precedence binary operator (IMP-0139-COMMA), so`` |
|         - | 13748 | `` * `(1, 2)` and `$x = (f(), $y)` compile and evaluate to the right operand.`` |
|         - | 13749 | ` * php 8 has no comma operator: its grammar only allows comma-separated` |
|         - | 13750 | ` * expression LISTS inside for(...) clauses (call arguments, array literals and` |
|         - | 13751 | ` * list() are split by the parser, never by this node). Accepting it changes the` |
|         - | 13752 | ` * meaning of source php rejects, which §10 classes as a bug — so every context` |
|         - | 13753 | ` * except for() now reports php's parse error.` |
|         - | 13754 | ` */` |
| 189858026 | 13755 | `static int GenStateTreeHasComma(ph7_expr_node *pNode)` |
|         5 | 13756 | `{` |
|         - | 13757 | `	ph7_expr_node **apArg;` |
|         - | 13758 | `	sxu32 n;` |
| 189858031 | 13759 | `	if( pNode == 0 ){` |
| 133329235 | 13760 | `		return 0;` |
|         - | 13761 | `	}` |
|  56528801 | 13762 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         6 | 13763 | `		return 1;` |
|         - | 13764 | `	}` |
|  56528792 | 13765 | `	if( GenStateTreeHasComma(pNode->pLeft) \|\| GenStateTreeHasComma(pNode->pRight)` |
|  56528793 | 13766 | `	 \|\| GenStateTreeHasComma(pNode->pCond) ){` |
|         6 | 13767 | `		return 1;` |
|         - | 13768 | `	}` |
|  56528793 | 13769 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  64470077 | 13770 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; n++ ){` |
|   7941289 | 13771 | `		if( GenStateTreeHasComma(apArg[n]) ){` |
|       ! 0 | 13772 | `			return 1;` |
|         - | 13773 | `		}` |
|   3970647 | 13774 | `	}` |
|  56528793 | 13775 | `	return 0;` |
|  94929018 | 13776 | `}` |
|  12581528 | 13777 | `static sxi32 PH7_CompileExpr(` |
|         - | 13778 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 13779 | `	sxi32 iFlags,        /* Control flags */` |
|         - | 13780 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|         - | 13781 | `	)` |
|         5 | 13782 | `{` |
|         - | 13783 | `	ph7_expr_node *pRoot;` |
|         - | 13784 | `	SySet sExprNode;` |
|         - | 13785 | `	SyToken *pEnd;` |
|         - | 13786 | `	sxi32 nExpr;` |
|         - | 13787 | `	sxi32 iNest;` |
|         - | 13788 | `	sxi32 rc;` |
|         - | 13789 | `	sxu32 nNullsafeBase;` |
|         - | 13790 | `	/* Initialize worker variables */` |
|  12581533 | 13791 | `	nExpr = 0;` |
|  12581533 | 13792 | `	pRoot = 0;` |
|         - | 13793 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|         - | 13794 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  12581533 | 13795 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  12581533 | 13796 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  12581533 | 13797 | `	SySetAlloc(&sExprNode,0x10);` |
|  12581533 | 13798 | `	rc = SXRET_OK;` |
|         - | 13799 | `	/* Delimit the expression */` |
|  12581533 | 13800 | `	pEnd = pGen->pIn;` |
|  12581533 | 13801 | `	iNest = 0;` |
|  99921389 | 13802 | `	while( pEnd < pGen->pEnd ){` |
|  95313391 | 13803 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 13804 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      4507 | 13805 | `			iNest++;` |
|  95311140 | 13806 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      4515 | 13807 | `			iNest--;` |
|  95306634 | 13808 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|   7974151 | 13809 | `			if( iNest <= 0 ){` |
|   7973535 | 13810 | `				break;` |
|         - | 13811 | `			}` |
|       308 | 13812 | `		}` |
|  87339861 | 13813 | `		pEnd++;` |
|         5 | 13814 | `	}` |
|  12581533 | 13815 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|    634941 | 13816 | `		SyToken *pEnd2 = pGen->pIn;` |
|    634941 | 13817 | `		iNest = 0;` |
|         - | 13818 | `		/* Stop at the first comma */` |
|   1384485 | 13819 | `		while( pEnd2 < pEnd ){` |
|    749551 | 13820 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|     41865 | 13821 | `				iNest++;` |
|    728621 | 13822 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|     41865 | 13823 | `				iNest--;` |
|    686761 | 13824 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|        63 | 13825 | `				if( iNest <= 0 ){` |
|         3 | 13826 | `					break;` |
|         - | 13827 | `				}` |
|        28 | 13828 | `			}` |
|    749549 | 13829 | `			pEnd2++;` |
|         5 | 13830 | `		}` |
|    634941 | 13831 | `		if( pEnd2 <pEnd ){` |
|         3 | 13832 | `			pEnd = pEnd2;` |
|         1 | 13833 | `		}` |
|    317468 | 13834 | `	}` |
|  12581533 | 13835 | `	if( pEnd > pGen->pIn ){` |
|  12558747 | 13836 | `		SyToken *pTmp = pGen->pEnd;` |
|         - | 13837 | `		/* Swap delimiter */` |
|  12558747 | 13838 | `		pGen->pEnd = pEnd;` |
|         - | 13839 | `		/* Try to get an expression tree */` |
|  12558747 | 13840 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  12558742 | 13841 | `		if( rc == SXRET_OK && pRoot && pGen->nCommaExprOk < 1` |
|  12444471 | 13842 | `		 && GenStateTreeHasComma(pRoot) ){` |
|         - | 13843 | `			/* php has no comma operator outside a for() clause */` |
|         6 | 13844 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pRoot->pStart->nLine,` |
|         - | 13845 | `				"syntax error, unexpected token \",\"");` |
|         6 | 13846 | `			pGen->pEnd = pTmp;` |
|         6 | 13847 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 13848 | `				SySetRelease(&sExprNode);` |
|       ! 0 | 13849 | `				return SXERR_ABORT;` |
|         - | 13850 | `			}` |
|         6 | 13851 | `			pGen->pIn = pEnd;` |
|         6 | 13852 | `			SySetRelease(&sExprNode);` |
|         6 | 13853 | `			SySetTruncate(&pGen->aNullsafeJmp,nNullsafeBase);` |
|         6 | 13854 | `			return SXRET_OK;` |
|         - | 13855 | `		}` |
|  12558743 | 13856 | `		if( rc == SXRET_OK && pRoot ){` |
|  12558559 | 13857 | `			rc = SXRET_OK;` |
|  12558559 | 13858 | `			if( xTreeValidator ){` |
|         - | 13859 | `				/* Call the upper layer validator callback */` |
|    825005 | 13860 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    412500 | 13861 | `			}` |
|  12558559 | 13862 | `			if( rc != SXERR_ABORT ){` |
|         - | 13863 | `				/* Generate code for the given tree */` |
|  12558559 | 13864 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|         - | 13865 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|         - | 13866 | `				 * expression so they short-circuit to its end. */` |
|  12558559 | 13867 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|   6279277 | 13868 | `			}` |
|  12558559 | 13869 | `			nExpr = 1;` |
|   6279277 | 13870 | `		}` |
|         - | 13871 | `		/* Release the whole tree */` |
|  12558743 | 13872 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|         - | 13873 | `		/* Synchronize token stream */` |
|  12558743 | 13874 | `		pGen->pEnd = pTmp;` |
|  12558743 | 13875 | `		pGen->pIn  = pEnd;` |
|  12558743 | 13876 | `		if( rc == SXERR_ABORT ){` |
|        13 | 13877 | `			SySetRelease(&sExprNode);` |
|        13 | 13878 | `			return SXERR_ABORT;` |
|         - | 13879 | `		}` |
|   6279364 | 13880 | `	}` |
|  12581519 | 13881 | `	SySetRelease(&sExprNode);` |
|  12581519 | 13882 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|   6290769 | 13883 | `}` |
|         - | 13884 | `/*` |
|         - | 13885 | ` * Return a pointer to the node construct handler associated` |
|         - | 13886 | ` * with a given node type [i.e: string,integer,float,...].` |
|         - | 13887 | ` */` |
|   7059220 | 13888 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|         5 | 13889 | `{` |
|   7059225 | 13890 | `	if( nNodeType & PH7_TK_NUM ){` |
|         - | 13891 | `		/* Numeric literal: Either real or integer */` |
|   2695985 | 13892 | `		return PH7_CompileNumLiteral;` |
|   4363245 | 13893 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|         - | 13894 | `		/* Double quoted string */` |
|     80165 | 13895 | `		return PH7_CompileString;` |
|   4283085 | 13896 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|         - | 13897 | `		/* Single quoted string */` |
|   4282965 | 13898 | `		return PH7_CompileSimpleString;` |
|       124 | 13899 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|         - | 13900 | `		/* Heredoc */` |
|        70 | 13901 | `		return PH7_CompileHereDoc;` |
|        58 | 13902 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|         - | 13903 | `		/* Nowdoc */` |
|        51 | 13904 | `		return PH7_CompileNowDoc;` |
|         9 | 13905 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|         - | 13906 | `		/* Backtick quoted string */` |
|         6 | 13907 | `		return PH7_CompileBacktic;` |
|         - | 13908 | `	}` |
|         3 | 13909 | `	return 0;` |
|   3529615 | 13910 | `}` |
|         - | 13911 | `/*` |
|         - | 13912 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|         - | 13913 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|         - | 13914 | ` * in write context" parse error.` |
|         - | 13915 | ` */` |
|     29990 | 13916 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|         5 | 13917 | `{` |
|         - | 13918 | `	sxi32 rc;` |
|     29995 | 13919 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|     29993 | 13920 | `		return SXRET_OK;` |
|         - | 13921 | `	}` |
|         5 | 13922 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|         2 | 13923 | `		pNode ? pNode->pStart->nLine : 1,` |
|         - | 13924 | `		"Can't use nullsafe operator in write context");` |
|         3 | 13925 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     15000 | 13926 | `}` |
|         - | 13927 | `/*` |
|         - | 13928 | ` * Compile an unset() statement.` |
|         - | 13929 | ` * unset($var, $arr[$key], ...);` |
|         - | 13930 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|         - | 13931 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|         - | 13932 | ` * parent array before extracting the element to unset.` |
|         - | 13933 | ` */` |
|     25760 | 13934 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|         5 | 13935 | `{` |
|     25765 | 13936 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     25765 | 13937 | `	sxu32 nIdx = 0;` |
|         - | 13938 | `	SyString sName;` |
|         - | 13939 | `	sxi32 rc;` |
|         - | 13940 | `	/* Jump the 'unset' keyword */` |
|     25765 | 13941 | `	pGen->pIn++;` |
|         - | 13942 | `	/* Save delimiter */` |
|     25765 | 13943 | `	pTmp = pGen->pEnd;` |
|         - | 13944 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     25765 | 13945 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     25765 | 13946 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 13947 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|         - | 13948 | `		SyToken *pClose;` |
|     25765 | 13949 | `		pGen->pIn++;   /* Skip '(' */` |
|     25765 | 13950 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     25765 | 13951 | `		pEnd = pClose; /* Stop at ')' */` |
|     12880 | 13952 | `	}` |
|     25765 | 13953 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|         - | 13954 | `	/* Resolve the 'unset' builtin name once */` |
|     25765 | 13955 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      3801 | 13956 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3801 | 13957 | `		if( pObj == 0 ){` |
|       ! 0 | 13958 | `			return SXERR_ABORT;` |
|         - | 13959 | `		}` |
|      3801 | 13960 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      3801 | 13961 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1898 | 13962 | `	}` |
|         - | 13963 | `	/* Compile each comma-separated argument */` |
|     55757 | 13964 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     29997 | 13965 | `		if( pGen->pIn < pNext ){` |
|     29997 | 13966 | `			pGen->pEnd = pNext;` |
|     29997 | 13967 | `			rc = PH7_CompileExpr(&(*pGen),` |
|         - | 13968 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|         - | 13969 | `				GenStateUnsetValidator);` |
|     29997 | 13970 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 13971 | `				return SXERR_ABORT;` |
|         - | 13972 | `			}` |
|     29997 | 13973 | `			if( rc != SXERR_EMPTY ){` |
|         - | 13974 | `				/* Emit call for this single argument */` |
|     29995 | 13975 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     29995 | 13976 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|     29995 | 13977 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     14995 | 13978 | `			}` |
|     14996 | 13979 | `		}` |
|         - | 13980 | `		/* Jump trailing commas */` |
|     34231 | 13981 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|      4239 | 13982 | `			pNext++;` |
|         5 | 13983 | `		}` |
|     29997 | 13984 | `		pGen->pIn = pNext;` |
|         5 | 13985 | `	}` |
|         - | 13986 | `	/* Skip past the closing ')' if present */` |
|     25765 | 13987 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     25765 | 13988 | `		pGen->pIn++;` |
|     12880 | 13989 | `	}` |
|         - | 13990 | `	/* Restore token stream */` |
|     25765 | 13991 | `	pGen->pEnd = pTmp;` |
|     25765 | 13992 | `	return SXRET_OK;` |
|     12885 | 13993 | `}` |
|         - | 13994 | `/*` |
|         - | 13995 | ` * PHP Language construct table.` |
|         - | 13996 | ` */` |
|         - | 13997 | `static const LangConstruct aLangConstruct[] = {` |
|         - | 13998 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|         - | 13999 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|         - | 14000 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|         - | 14001 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|         - | 14002 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|         - | 14003 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|         - | 14004 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|         - | 14005 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|         - | 14006 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|         - | 14007 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|         - | 14008 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|         - | 14009 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|         - | 14010 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|         - | 14011 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|         - | 14012 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|         - | 14013 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|         - | 14014 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|         - | 14015 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|         - | 14016 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|         - | 14017 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|         - | 14018 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|         - | 14019 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|         - | 14020 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|         - | 14021 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|         - | 14022 | `};` |
|         - | 14023 | `/*` |
|         - | 14024 | ` * Return a pointer to the statement handler routine associated` |
|         - | 14025 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|         - | 14026 | ` */` |
|   6246424 | 14027 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|         - | 14028 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|         - | 14029 | `	SyToken *pLookahed  /* Look-ahead token */` |
|         - | 14030 | `	)` |
|         5 | 14031 | `{` |
|   6246429 | 14032 | `	sxu32 n = 0;` |
|  25801704 | 14033 | `	for(;;){` |
|  51603413 | 14034 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|    427455 | 14035 | `			break;` |
|         - | 14036 | `		}` |
|  51175963 | 14037 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|   5818979 | 14038 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|       ! 0 | 14039 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|       ! 0 | 14040 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|         - | 14041 | `					/* 'static' (class context),return null */` |
|       ! 0 | 14042 | `					return 0;` |
|         - | 14043 | `				}` |
|       ! 0 | 14044 | `			}` |
|   5818974 | 14045 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|        14 | 14046 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|        14 | 14047 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|         - | 14048 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|         3 | 14049 | `				return 0;` |
|         - | 14050 | `			}` |
|         - | 14051 | `			/* Return a pointer to the handler.` |
|         - | 14052 | `			*/` |
|   5818977 | 14053 | `			return aLangConstruct[n].xConstruct;` |
|         - | 14054 | `		}` |
|  45356989 | 14055 | `		n++;` |
|         5 | 14056 | `	}` |
|    427455 | 14057 | `	if( pLookahed ){` |
|    427455 | 14058 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|     68437 | 14059 | `			return PH7_CompileClassInterface;` |
|    359023 | 14060 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|    312917 | 14061 | `			return PH7_CompileClass;` |
|     46111 | 14062 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      7675 | 14063 | `			return PH7_CompileTrait;` |
|         - | 14064 | `		}` |
|         - | 14065 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|         - | 14066 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|         - | 14067 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|         - | 14068 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     19218 | 14069 | `	}` |
|         - | 14070 | `	/* Not a language construct */` |
|     38441 | 14071 | `	return 0;` |
|   3123217 | 14072 | `}` |
|         - | 14073 | `/*` |
|         - | 14074 | ` * Check if the given keyword is in fact a PHP language construct.` |
|         - | 14075 | ` * Return TRUE on success. FALSE otheriwse.` |
|         - | 14076 | ` */` |
|     38438 | 14077 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|         5 | 14078 | `{` |
|         - | 14079 | `	int rc;` |
|     38443 | 14080 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     38443 | 14081 | `	if( rc == FALSE ){` |
|     38334 | 14082 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     15550 | 14083 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|         - | 14084 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|         - | 14085 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|         - | 14086 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|         - | 14087 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|         - | 14088 | `			*/` |
|         - | 14089 | `			){` |
|     38331 | 14090 | `				rc = TRUE;` |
|     19163 | 14091 | `		}` |
|     19167 | 14092 | `	}` |
|     38443 | 14093 | `	return rc;` |
|         5 | 14094 | `}` |
|         - | 14095 | `/*` |
|         - | 14096 | ` * Compile a PHP chunk.` |
|         - | 14097 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 14098 | ` * takes care of generating the appropriate error message.` |
|         - | 14099 | ` */` |
|         - | 14100 | `/*` |
|         - | 14101 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|         - | 14102 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|         - | 14103 | ` * the chunk token set it becomes the pending docblock. An existing` |
|         - | 14104 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|         - | 14105 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|         - | 14106 | ` * intervening non-declaration statements.` |
|         - | 14107 | ` */` |
|  13839834 | 14108 | `static void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|         5 | 14109 | `{` |
|  13839839 | 14110 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  13839839 | 14111 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  13839839 | 14112 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 14113 | `	sxu32 nIdx, n;` |
|  13839834 | 14114 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   1502329 | 14115 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|         - | 14116 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|         - | 14117 | `		 * indexes do not map to the sidecar */` |
|  12337517 | 14118 | `		return;` |
|         - | 14119 | `	}` |
|   1502327 | 14120 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|         - | 14121 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|         - | 14122 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|   1502327 | 14123 | `	SySetReset(&pGen->aPendingAttrs);` |
|   4508465 | 14124 | `	for( n = 0 ; n < nT ; n++ ){` |
|   3006143 | 14125 | `		if( aT[n].nTokIdx != nIdx ){` |
|   2998387 | 14126 | `			continue;` |
|         - | 14127 | `		}` |
|      7761 | 14128 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|        29 | 14129 | `			pGen->sPendingDoc = aT[n].sText;` |
|      7749 | 14130 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|      7737 | 14131 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|      3866 | 14132 | `		}` |
|      3883 | 14133 | `	}` |
|   6919922 | 14134 | `}` |
|         - | 14135 | `/*` |
|         - | 14136 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|         - | 14137 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|         - | 14138 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|         - | 14139 | ` */` |
|   3844436 | 14140 | `static void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|         5 | 14141 | `{` |
|         - | 14142 | `	char *zDup;` |
|   3844441 | 14143 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|   3844421 | 14144 | `		return;` |
|         - | 14145 | `	}` |
|        35 | 14146 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 | 14147 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|        25 | 14148 | `	if( zDup ){` |
|        25 | 14149 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|        10 | 14150 | `	}` |
|        25 | 14151 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|   1922223 | 14152 | `}` |
|         - | 14153 | `/*` |
|         - | 14154 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|         - | 14155 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|         - | 14156 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|         - | 14157 | ` * names may point into the token text, which must outlive the raw script` |
|         - | 14158 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|         - | 14159 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|         - | 14160 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|         - | 14161 | ` */` |
|      7744 | 14162 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|         5 | 14163 | `{` |
|         - | 14164 | `	SySet *pToken;` |
|         - | 14165 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|         - | 14166 | `	char *zSpan;` |
|      7749 | 14167 | `	sxi32 rc = SXRET_OK;` |
|      7749 | 14168 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|       ! 0 | 14169 | `		return SXRET_OK;` |
|         - | 14170 | `	}` |
|     11621 | 14171 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      3872 | 14172 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|      7749 | 14173 | `	if( zSpan == 0 ){` |
|       ! 0 | 14174 | `		return SXRET_OK;` |
|         - | 14175 | `	}` |
|         - | 14176 | `	/* The token set must outlive compilation too: interned operands may` |
|         - | 14177 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|         - | 14178 | `	 * the number of attribute declarations in the program. */` |
|      7749 | 14179 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      7749 | 14180 | `	if( pToken == 0 ){` |
|       ! 0 | 14181 | `		return SXRET_OK;` |
|         - | 14182 | `	}` |
|      7749 | 14183 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      7749 | 14184 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|      7749 | 14185 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|      7749 | 14186 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|      7749 | 14187 | `	pSavedIn = pGen->pIn;` |
|      7749 | 14188 | `	pSavedEnd = pGen->pEnd;` |
|      7753 | 14189 | `	while( pIn < pEnd ){` |
|         - | 14190 | `		ph7_attribute sAttr;` |
|         - | 14191 | `		SyBlob sFQN;` |
|      7753 | 14192 | `		int bAbsolute = 0;` |
|      7753 | 14193 | `		SyZero(&sAttr,sizeof(sAttr));` |
|      7753 | 14194 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|      7753 | 14195 | `		sAttr.nLine = pIn->nLine;` |
|      7753 | 14196 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|        75 | 14197 | `			bAbsolute = 1;` |
|        75 | 14198 | `			pIn++;` |
|        35 | 14199 | `		}` |
|      7753 | 14200 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7753 | 14201 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      7753 | 14202 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|      7753 | 14203 | `			pIn++;` |
|      7753 | 14204 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       ! 0 | 14205 | `				SyBlobAppend(&sFQN,"\\",1);` |
|       ! 0 | 14206 | `				pIn++;` |
|       ! 0 | 14207 | `				continue;` |
|         - | 14208 | `			}` |
|      7753 | 14209 | `			break;` |
|       ! 0 | 14210 | `		}` |
|      7753 | 14211 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|         - | 14212 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|         - | 14213 | `			 * this feature; never turn it into a new fatal) */` |
|       ! 0 | 14214 | `			SyBlobRelease(&sFQN);` |
|       ! 0 | 14215 | `			break;` |
|         - | 14216 | `		}` |
|         - | 14217 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|         - | 14218 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|         - | 14219 | `		{` |
|      7753 | 14220 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|      7753 | 14221 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|      7753 | 14222 | `			char *zDup = 0;` |
|      7753 | 14223 | `			if( !bAbsolute ){` |
|      7683 | 14224 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|      7683 | 14225 | `				if( pImp ){` |
|       ! 0 | 14226 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|       ! 0 | 14227 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|       ! 0 | 14228 | `					if( zDup ){` |
|       ! 0 | 14229 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|       ! 0 | 14230 | `					}` |
|      7683 | 14231 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - | 14232 | `					SyBlob sTmp;` |
|       ! 0 | 14233 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|       ! 0 | 14234 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       ! 0 | 14235 | `					SyBlobAppend(&sTmp,"\\",1);` |
|       ! 0 | 14236 | `					SyBlobAppend(&sTmp,zName,nName);` |
|       ! 0 | 14237 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       ! 0 | 14238 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|       ! 0 | 14239 | `					if( zDup ){` |
|       ! 0 | 14240 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|       ! 0 | 14241 | `					}` |
|       ! 0 | 14242 | `					SyBlobRelease(&sTmp);` |
|       ! 0 | 14243 | `				}` |
|      3839 | 14244 | `			}` |
|      7753 | 14245 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|      7753 | 14246 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|      7753 | 14247 | `				if( zDup ){` |
|      7753 | 14248 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|      3874 | 14249 | `				}` |
|      3874 | 14250 | `			}` |
|         - | 14251 | `		}` |
|      7753 | 14252 | `		SyBlobRelease(&sFQN);` |
|      7753 | 14253 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 14254 | `			SyToken *pArgsEnd;` |
|      7651 | 14255 | `			pIn++;` |
|      7651 | 14256 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|     15311 | 14257 | `			while( pIn < pArgsEnd ){` |
|      7665 | 14258 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|      7665 | 14259 | `				sxi32 iDepth = 0;` |
|         - | 14260 | `				ph7_attr_arg sArgRec;` |
|     76165 | 14261 | `				while( pArgStop < pArgsEnd ){` |
|     68521 | 14262 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        11 | 14263 | `						iDepth++;` |
|     68516 | 14264 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        11 | 14265 | `						iDepth--;` |
|     68506 | 14266 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|        17 | 14267 | `						break;` |
|         - | 14268 | `					}` |
|     68505 | 14269 | `					pArgStop++;` |
|         5 | 14270 | `				}` |
|      7665 | 14271 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|      7665 | 14272 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      7660 | 14273 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      7644 | 14274 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        28 | 14275 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|         9 | 14276 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|        19 | 14277 | `					if( zN ){` |
|        19 | 14278 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|         9 | 14279 | `					}` |
|        19 | 14280 | `					pArgStart += 2;` |
|         9 | 14281 | `				}` |
|      7665 | 14282 | `				if( pArgStart < pArgStop ){` |
|         - | 14283 | `					SySet *pInstrContainer;` |
|      7665 | 14284 | `					pGen->pIn = pArgStart;` |
|      7665 | 14285 | `					pGen->pEnd = pArgStop;` |
|      7665 | 14286 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7665 | 14287 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|      7665 | 14288 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7665 | 14289 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7665 | 14290 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7665 | 14291 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 14292 | `						pGen->pIn = pSavedIn;` |
|       ! 0 | 14293 | `						pGen->pEnd = pSavedEnd;` |
|       ! 0 | 14294 | `						return SXERR_ABORT;` |
|         - | 14295 | `					}` |
|      7665 | 14296 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|      3830 | 14297 | `				}` |
|      7665 | 14298 | `				pIn = pArgStop;` |
|      7665 | 14299 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 | 14300 | `					pIn++;` |
|         8 | 14301 | `				}` |
|         5 | 14302 | `			}` |
|      7651 | 14303 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|      3823 | 14304 | `		}` |
|      7753 | 14305 | `		SySetPut(pOut,(const void *)&sAttr);` |
|      7753 | 14306 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|         5 | 14307 | `			pIn++;` |
|         5 | 14308 | `			continue;` |
|         - | 14309 | `		}` |
|      7749 | 14310 | `		break;` |
|       ! 0 | 14311 | `	}` |
|      7749 | 14312 | `	pGen->pIn = pSavedIn;` |
|      7749 | 14313 | `	pGen->pEnd = pSavedEnd;` |
|      7749 | 14314 | `	return SXRET_OK;` |
|      3877 | 14315 | `}` |
|         - | 14316 | `/*` |
|         - | 14317 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|         - | 14318 | ` * every recorded group into pOut and clear the pending list.` |
|         - | 14319 | ` */` |
|   3844440 | 14320 | `static sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|         5 | 14321 | `{` |
|   3844445 | 14322 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|         - | 14323 | `	sxu32 n;` |
|         - | 14324 | `	sxi32 rc;` |
|   3852177 | 14325 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|      7737 | 14326 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|      7737 | 14327 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 14328 | `			return SXERR_ABORT;` |
|         - | 14329 | `		}` |
|      3871 | 14330 | `	}` |
|   3844445 | 14331 | `	SySetReset(&pGen->aPendingAttrs);` |
|   3844445 | 14332 | `	return SXRET_OK;` |
|   1922225 | 14333 | `}` |
|         - | 14334 | `/*` |
|         - | 14335 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|         - | 14336 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|         - | 14337 | ` * the main token stream, so the sidecar indexes map directly.` |
|         - | 14338 | ` */` |
|   1647306 | 14339 | `static sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|         5 | 14340 | `{` |
|   1647311 | 14341 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   1647311 | 14342 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   1647311 | 14343 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 14344 | `	sxu32 nIdx, n;` |
|         - | 14345 | `	sxi32 rc;` |
|   1647306 | 14346 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|    190135 | 14347 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   1457181 | 14348 | `		return SXRET_OK;` |
|         - | 14349 | `	}` |
|    190135 | 14350 | `	nIdx = (sxu32)(pTok - pBase);` |
|    570393 | 14351 | `	for( n = 0 ; n < nT ; n++ ){` |
|    380263 | 14352 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|        13 | 14353 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|        13 | 14354 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14355 | `				return SXERR_ABORT;` |
|         - | 14356 | `			}` |
|         6 | 14357 | `		}` |
|    190134 | 14358 | `	}` |
|    190135 | 14359 | `	return SXRET_OK;` |
|    823658 | 14360 | `}` |
|  10021890 | 14361 | `static sxi32 GenStateCompileChunk(` |
|         - | 14362 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 14363 | `	sxi32 iFlags         /* Compile flags */` |
|         - | 14364 | `	)` |
|         5 | 14365 | `{` |
|         - | 14366 | `	ProcLangConstruct xCons;` |
|         - | 14367 | `	sxi32 rc;` |
|  10021895 | 14368 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|   5772953 | 14369 | `	for(;;){` |
|  10783903 | 14370 | `		int bStmtIsDeclare = 0;` |
|  10783903 | 14371 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 14372 | `			/* No more input to process */` |
|     67211 | 14373 | `			break;` |
|         - | 14374 | `		}` |
|         - | 14375 | `		/* Bind a directly-preceding docblock to this statement */` |
|  10716697 | 14376 | `		GenStateSetPendingDoc(&(*pGen));` |
|  10716697 | 14377 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - | 14378 | `			/* php: a statement-position attribute group must be followed by a` |
|         - | 14379 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|         - | 14380 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|         - | 14381 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|         - | 14382 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|      7655 | 14383 | `			int bAttrTarget = 0;` |
|      7650 | 14384 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|      3859 | 14385 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|      7597 | 14386 | `				bAttrTarget = 1;` |
|      3855 | 14387 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        59 | 14388 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        58 | 14389 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|        15 | 14390 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|         4 | 14391 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|         4 | 14392 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|         1 | 14393 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|        59 | 14394 | `					bAttrTarget = 1;` |
|        29 | 14395 | `				}` |
|        29 | 14396 | `			}` |
|      7655 | 14397 | `			if( !bAttrTarget ){` |
|       ! 0 | 14398 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 14399 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|       ! 0 | 14400 | `					&pGen->pIn->sData);` |
|       ! 0 | 14401 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 14402 | `					break;` |
|         - | 14403 | `				}` |
|       ! 0 | 14404 | `				SySetReset(&pGen->aPendingAttrs);` |
|       ! 0 | 14405 | `			}` |
|      3825 | 14406 | `		}` |
|         - | 14407 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|         - | 14408 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  10716697 | 14409 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   6280643 | 14410 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   6280643 | 14411 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|        47 | 14412 | `				bStmtIsDeclare = 1;` |
|        21 | 14413 | `			}` |
|   3140319 | 14414 | `		}` |
|  10716697 | 14415 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|         - | 14416 | `			/* Any non-declare top-level statement locks the strict_types` |
|         - | 14417 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|    761981 | 14418 | `			pGen->bStrictTypesLocked = 1;` |
|    380988 | 14419 | `		}` |
|  10716697 | 14420 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 14421 | `			/* Compile block */` |
|      3819 | 14422 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      3819 | 14423 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14424 | `				break;` |
|         - | 14425 | `			}` |
|      1912 | 14426 | `		}else{` |
|  10712883 | 14427 | `			xCons = 0;` |
|  10712883 | 14428 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|         - | 14429 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|         - | 14430 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|         - | 14431 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|     34245 | 14432 | `				xCons = PH7_CompileClassModifiers;` |
|  10695763 | 14433 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|         - | 14434 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|         - | 14435 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|      3829 | 14436 | `				xCons = PH7_CompileEnum;` |
|  10676731 | 14437 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   6246429 | 14438 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - | 14439 | `				/* Try to extract a language construct handler */` |
|   6246429 | 14440 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|   6246429 | 14441 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|        13 | 14442 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 14443 | `						"Syntax error: Unexpected keyword '%z'",` |
|         8 | 14444 | `						&pGen->pIn->sData);` |
|         9 | 14445 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 14446 | `						break;` |
|         - | 14447 | `					}` |
|         - | 14448 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|         - | 14449 | `					 * this erroneous statement.` |
|         - | 14450 | `					 */` |
|         9 | 14451 | `					xCons = PH7_ErrorRecover;` |
|         4 | 14452 | `				}` |
|   7551607 | 14453 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    357703 | 14454 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|         - | 14455 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|       117 | 14456 | `				xCons = PH7_CompileLabel;` |
|        56 | 14457 | `			}` |
|  10712883 | 14458 | `			if( xCons == 0 ){` |
|         - | 14459 | `				/* Assume an expression an try to compile it */` |
|   4466713 | 14460 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   4466713 | 14461 | `				if(  rc != SXERR_EMPTY ){` |
|         - | 14462 | `					/* Pop l-value */` |
|   4466563 | 14463 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   2233279 | 14464 | `				}` |
|   2233359 | 14465 | `			}else{` |
|         - | 14466 | `				/* Go compile the sucker */` |
|   6246175 | 14467 | `				rc = xCons(&(*pGen));` |
|         - | 14468 | `			}` |
|  10712883 | 14469 | `			if( rc == SXERR_ABORT ){` |
|         - | 14470 | `				/* Request to abort compilation */` |
|        13 | 14471 | `				break;` |
|         - | 14472 | `			}` |
|         - | 14473 | `		}` |
|         - | 14474 | `		/* Ignore trailing semi-colons ';' */` |
|  18460337 | 14475 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|   7743655 | 14476 | `			pGen->pIn++;` |
|         5 | 14477 | `		}` |
|  10716687 | 14478 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|         - | 14479 | `			/* Compile a single statement and return */` |
|   9954679 | 14480 | `			break;` |
|         - | 14481 | `		}` |
|         - | 14482 | `		/* LOOP ONE */` |
|         - | 14483 | `		/* LOOP TWO */` |
|         - | 14484 | `		/* LOOP THREE */` |
|         - | 14485 | `		/* LOOP FOUR */` |
|         5 | 14486 | `	}` |
|         - | 14487 | `	/* Return compilation status */` |
|  10021895 | 14488 | `	return rc;` |
|         5 | 14489 | `}` |
|         - | 14490 | `/*` |
|         - | 14491 | ` * Compile a Raw PHP chunk.` |
|         - | 14492 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 14493 | ` * takes care of generating the appropriate error message.` |
|         - | 14494 | ` */` |
|     67218 | 14495 | `static sxi32 PH7_CompilePHP(` |
|         - | 14496 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 14497 | `	SySet *pTokenSet,     /* Token set */` |
|         - | 14498 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|         - | 14499 | `	)` |
|         5 | 14500 | `{` |
|     67223 | 14501 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|         - | 14502 | `	sxi32 rc;` |
|         - | 14503 | `	/* Reset the token set (and its trivia sidecar) */` |
|     67223 | 14504 | `	SySetReset(&(*pTokenSet));` |
|     67223 | 14505 | `	SySetReset(&pGen->aTrivia);` |
|         - | 14506 | `	/* Mark as the default token set */` |
|     67223 | 14507 | `	pGen->pTokenSet = &(*pTokenSet);` |
|         - | 14508 | `	/* Advance the stream cursor */` |
|     67223 | 14509 | `	pGen->pRawIn++;` |
|         - | 14510 | `	/* Tokenize the PHP chunk first */` |
|     67223 | 14511 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|         - | 14512 | `	/* Point to the head and tail of the token stream. */` |
|     67223 | 14513 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     67223 | 14514 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|     67223 | 14515 | `	if( is_expr ){` |
|       ! 0 | 14516 | `		rc = SXERR_EMPTY;` |
|       ! 0 | 14517 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - | 14518 | `			/* A simple expression,compile it */` |
|       ! 0 | 14519 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 | 14520 | `		}` |
|         - | 14521 | `		/* Emit the DONE instruction */` |
|       ! 0 | 14522 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       ! 0 | 14523 | `		return SXRET_OK;` |
|         - | 14524 | `	}` |
|     67223 | 14525 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - | 14526 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - | 14527 | `		/*` |
|         - | 14528 | `		 * Shortcut syntax for the 'echo' language construct.` |
|         - | 14529 | `		 * According to the PHP reference manual:` |
|         - | 14530 | `		 *  echo() also has a shortcut syntax, where you can` |
|         - | 14531 | `		 *  immediately follow` |
|         - | 14532 | `		 *  the opening tag with an equals sign as follows:` |
|         - | 14533 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|         - | 14534 | `		 * Symisc extension:` |
|         - | 14535 | `		 *   This short syntax works with all PHP opening` |
|         - | 14536 | `		 *   tags unlike the default PHP engine that handle` |
|         - | 14537 | `		 *   only short tag.` |
|         - | 14538 | `		 */` |
|         - | 14539 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|         3 | 14540 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|         3 | 14541 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|         3 | 14542 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|         3 | 14543 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|         3 | 14544 | `		if( rc != SXERR_EMPTY ){` |
|         3 | 14545 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 | 14546 | `		}` |
|         3 | 14547 | `		return SXRET_OK;` |
|         - | 14548 | `	}` |
|         - | 14549 | `	/* Compile the PHP chunk */` |
|     67221 | 14550 | `	rc = GenStateCompileChunk(pGen,0);` |
|         - | 14551 | `	/* Fix exceptions jumps */` |
|     67221 | 14552 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - | 14553 | `	/* Fix gotos now, the jump destination is resolved */` |
|     67221 | 14554 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|         3 | 14555 | `		rc = SXERR_ABORT;` |
|         1 | 14556 | `	}` |
|         - | 14557 | `	/* Reset container */` |
|     67221 | 14558 | `	SySetReset(&pGen->aGoto);` |
|     67221 | 14559 | `	SySetReset(&pGen->aLabel);` |
|     67221 | 14560 | `	SySetReset(&pGen->aNullsafeJmp);` |
|         - | 14561 | `	/* Compilation result */` |
|     67221 | 14562 | `	return rc;` |
|     33614 | 14563 | `}` |
|         - | 14564 | `/*` |
|         - | 14565 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|         - | 14566 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|         - | 14567 | ` * This is the only compile interface exported from this file.` |
|         - | 14568 | ` */` |
|     70300 | 14569 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|         - | 14570 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|         - | 14571 | `	SyString *pScript,  /* Script to compile */` |
|         - | 14572 | `	sxi32 iFlags        /* Compile flags */` |
|         - | 14573 | `	)` |
|         5 | 14574 | `{` |
|         - | 14575 | `	SySet aPhpToken,aRawToken;` |
|         - | 14576 | `	ph7_gen_state *pCodeGen;` |
|         - | 14577 | `	ph7_value *pRawObj;` |
|         - | 14578 | `	sxu32 nObjIdx;` |
|         - | 14579 | `	sxi32 nRawObj;` |
|         - | 14580 | `	int is_expr;` |
|         - | 14581 | `	sxi8 bSavedStrict;` |
|         - | 14582 | `	sxi8 bSavedStrictLocked;` |
|         - | 14583 | `	sxi32 rc;` |
|     70305 | 14584 | `	if( pScript->nByte < 1 ){` |
|         - | 14585 | `		/* Nothing to compile */` |
|       ! 0 | 14586 | `		return PH7_OK;` |
|         - | 14587 | `	}` |
|         - | 14588 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|         - | 14589 | `	 * file's flags so include/require restore them on return. */` |
|     70305 | 14590 | `	pCodeGen = &pVm->sCodeGen;` |
|     70305 | 14591 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|     70305 | 14592 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|     70305 | 14593 | `	pCodeGen->bStrictTypes = 0;` |
|     70305 | 14594 | `	pCodeGen->bStrictTypesLocked = 0;` |
|         - | 14595 | `	/* Initialize the tokens containers */` |
|     70305 | 14596 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|     70305 | 14597 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|     70305 | 14598 | `	SySetAlloc(&aPhpToken,0xc0);` |
|     70305 | 14599 | `	is_expr = 0;` |
|     70305 | 14600 | `	if( iFlags & PH7_PHP_ONLY ){` |
|         - | 14601 | `		SyToken sTmp;` |
|         - | 14602 | `		/* PHP only: -*/` |
|     57057 | 14603 | `		sTmp.nLine = 1;` |
|     57057 | 14604 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|     57057 | 14605 | `		sTmp.pUserData = 0;` |
|     57057 | 14606 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|     57057 | 14607 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|     57057 | 14608 | `		if( iFlags & PH7_PHP_EXPR ){` |
|         - | 14609 | `			/* A simple PHP expression */` |
|       ! 0 | 14610 | `			is_expr = 1;` |
|       ! 0 | 14611 | `		}` |
|     28531 | 14612 | `	}else{` |
|         - | 14613 | `		/* Tokenize raw text */` |
|     13253 | 14614 | `		SySetAlloc(&aRawToken,32);` |
|     13253 | 14615 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|         - | 14616 | `	}` |
|         - | 14617 | `	/* Process high-level tokens */` |
|     70305 | 14618 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|     70305 | 14619 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|     70305 | 14620 | `	rc = PH7_OK;` |
|     70305 | 14621 | `	if( is_expr ){` |
|         - | 14622 | `		/* Compile the expression */` |
|       ! 0 | 14623 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|       ! 0 | 14624 | `		goto cleanup;` |
|         - | 14625 | `	}` |
|     70305 | 14626 | `	nObjIdx = 0;` |
|         - | 14627 | `	/* Each compilation unit starts in the global namespace.` |
|         - | 14628 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|         - | 14629 | `	 * preventing namespace bleeding across include()d files. */` |
|     70305 | 14630 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|         - | 14631 | `	/* Start the compilation process */` |
|     41779 | 14632 | `	for(;;){` |
|    150769 | 14633 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|     70293 | 14634 | `			break; /* No more tokens to process */` |
|         - | 14635 | `		}` |
|     80481 | 14636 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|         - | 14637 | `			/* Compile the PHP chunk */` |
|     67223 | 14638 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|     67223 | 14639 | `			if( rc == SXERR_ABORT ){` |
|        16 | 14640 | `				break;` |
|         - | 14641 | `			}` |
|     67211 | 14642 | `			continue;` |
|         - | 14643 | `		}` |
|         - | 14644 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|     13263 | 14645 | `		nRawObj = 0;` |
|     26521 | 14646 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|         - | 14647 | `			/* Consume the raw chunk without any processing */` |
|     13263 | 14648 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|     13263 | 14649 | `			if( pRawObj == 0 ){` |
|       ! 0 | 14650 | `				rc = SXERR_MEM;` |
|       ! 0 | 14651 | `				break;` |
|         - | 14652 | `			}` |
|         - | 14653 | `			/* Mark as constant and emit the load constant instruction */` |
|     13263 | 14654 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|     13263 | 14655 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     13263 | 14656 | `			++nRawObj;` |
|     13263 | 14657 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|         5 | 14658 | `		}` |
|     13263 | 14659 | `		if( nRawObj > 0 ){` |
|         - | 14660 | `			/* Emit the consume instruction */` |
|     13263 | 14661 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      6629 | 14662 | `		}` |
|     35155 | 14663 | `	}` |
|     35150 | 14664 | `cleanup:` |
|     70305 | 14665 | `	SySetRelease(&aRawToken);` |
|     70305 | 14666 | `	SySetRelease(&aPhpToken);` |
|         - | 14667 | `	/* Restore outer file's strict_types scope */` |
|     70305 | 14668 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|     70305 | 14669 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|     70305 | 14670 | `	return rc;` |
|     35155 | 14671 | `}` |
|         - | 14672 | `/*` |
|         - | 14673 | ` * Utility routines.Initialize the code generator.` |
|         - | 14674 | ` */` |
|      3796 | 14675 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|         - | 14676 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 14677 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 14678 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 14679 | `	)` |
|         5 | 14680 | `{` |
|      3801 | 14681 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 14682 | `	/* Zero the structure */` |
|      3801 | 14683 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|         - | 14684 | `	/* Initial state */` |
|      3801 | 14685 | `	pGen->pVm  = &(*pVm);` |
|      3801 | 14686 | `	pGen->xErr = xErr;` |
|      3801 | 14687 | `	pGen->pErrData = pErrData;` |
|      3801 | 14688 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|      3801 | 14689 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|      3801 | 14690 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|      3801 | 14691 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|      3801 | 14692 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|      3801 | 14693 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3801 | 14694 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3801 | 14695 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|      3801 | 14696 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|         - | 14697 | `	/* Error log buffer */` |
|      3801 | 14698 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         - | 14699 | `	/* General purpose working buffer */` |
|      3801 | 14700 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         - | 14701 | `	/* Namespace state */` |
|      3801 | 14702 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|      3801 | 14703 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|      3801 | 14704 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|      3801 | 14705 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 14706 | `	/* Create the global scope */` |
|      3801 | 14707 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|         - | 14708 | `	/* Point to the global scope */` |
|      3801 | 14709 | `	pGen->pCurrent = &pGen->sGlobal;` |
|      3801 | 14710 | `	return SXRET_OK;` |
|         5 | 14711 | `}` |
|         - | 14712 | `/*` |
|         - | 14713 | ` * Utility routines. Reset the code generator to it's initial state.` |
|         - | 14714 | ` */` |
|     73642 | 14715 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|         - | 14716 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 14717 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 14718 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 14719 | `	)` |
|         5 | 14720 | `{` |
|     73647 | 14721 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 14722 | `	GenBlock *pBlock,*pParent;` |
|         - | 14723 | `	/* Reset state */` |
|     73647 | 14724 | `	SySetReset(&pGen->aLabel);` |
|     73647 | 14725 | `	SySetReset(&pGen->aGoto);` |
|     73647 | 14726 | `	SySetReset(&pGen->aNullsafeJmp);` |
|     73647 | 14727 | `	SySetReset(&pGen->aTrivia);` |
|     73647 | 14728 | `	SySetReset(&pGen->aPendingAttrs);` |
|     73647 | 14729 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|     73647 | 14730 | `	SyBlobRelease(&pGen->sErrBuf);` |
|     73647 | 14731 | `	SyBlobRelease(&pGen->sWorker);` |
|     73647 | 14732 | `	SyBlobRelease(&pGen->sNamespace);` |
|     73647 | 14733 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     73647 | 14734 | `	SyHashRelease(&pGen->hUseImports);` |
|     73647 | 14735 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|     73647 | 14736 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     73647 | 14737 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|     73647 | 14738 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     73647 | 14739 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 14740 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|         - | 14741 | `	 * They intern variable names and literal strings that are referenced by` |
|         - | 14742 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|         - | 14743 | `	 * Releasing them would either leak the interned strings or require freeing` |
|         - | 14744 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|         - | 14745 | `	 * number of unique names, which is acceptable. */` |
|         - | 14746 | `	/* Point to the global scope */` |
|     73647 | 14747 | `	pBlock = pGen->pCurrent;` |
|     73647 | 14748 | `	while( pBlock->pParent != 0 ){` |
|       ! 0 | 14749 | `		pParent = pBlock->pParent;` |
|       ! 0 | 14750 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 14751 | `		pBlock = pParent;` |
|       ! 0 | 14752 | `	}` |
|     73647 | 14753 | `	pGen->xErr = xErr;` |
|     73647 | 14754 | `	pGen->pErrData = pErrData;` |
|     73647 | 14755 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     73647 | 14756 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|     73647 | 14757 | `	pGen->pIn = pGen->pEnd = 0;` |
|     73647 | 14758 | `	pGen->nErr = 0;` |
|     73647 | 14759 | `	return SXRET_OK;` |
|         5 | 14760 | `}` |
|         - | 14761 | `/*` |
|         - | 14762 | ` * Raise php's parse error for an unexpected token: E_PARSE with the exact text` |
|         - | 14763 | ` * php's parser prints, e.g.` |
|         - | 14764 | ` *` |
|         - | 14765 | ` *   syntax error, unexpected token ";", expecting "{"` |
|         - | 14766 | ` *   syntax error, unexpected identifier "invalid", expecting "("` |
|         - | 14767 | ` *   syntax error, unexpected end of file` |
|         - | 14768 | ` *` |
|         - | 14769 | ` * php names the token by CLASS, not just by text: an identifier, a variable and` |
|         - | 14770 | ` * a number each get their own noun, while everything else (keywords, operators,` |
|         - | 14771 | ` * punctuation) is a "token". zExpecting is the optional ", expecting ..." tail` |
|         - | 14772 | ` * — pass NULL when the site cannot say what it wanted (php often can't either).` |
|         - | 14773 | ` * pTok == NULL means the input ran out: "unexpected end of file".` |
|         - | 14774 | ` *` |
|         - | 14775 | ` * PHL's hand-written recursive-descent parser has no bison expectation sets, so` |
|         - | 14776 | ` * a site can only claim an "expecting" clause it genuinely knows; every clause` |
|         - | 14777 | ` * emitted here was verified against php 8.5.7 for the construct in question.` |
|         - | 14778 | ` */` |
|       180 | 14779 | `PH7_PRIVATE sxi32 PH7_GenSyntaxError(` |
|         - | 14780 | `	ph7_gen_state *pGen,   /* Code generator state */` |
|         - | 14781 | `	SyToken *pTok,         /* Offending token, or NULL for end of file */` |
|         - | 14782 | `	const char *zExpecting /* ", expecting <this>" tail, or NULL */` |
|         - | 14783 | `	)` |
|         5 | 14784 | `{` |
|       185 | 14785 | `	const char *zNoun = "token";` |
|         - | 14786 | `	sxu32 nLine;` |
|       185 | 14787 | `	if( pTok == 0 && pGen->pTokenSet ){` |
|         - | 14788 | `		/* The caller ran out of tokens inside its own slice — but a statement's slice stops` |
|         - | 14789 | `		 * BEFORE its terminator, so the token php actually names (typically the ';') is the` |
|         - | 14790 | `		 * one sitting just past the slice, still inside the chunk's token stream. Reach for` |
|         - | 14791 | `		 * it before concluding "end of file". */` |
|        82 | 14792 | `		SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        82 | 14793 | `		SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        82 | 14794 | `		if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|        82 | 14795 | `			pTok = pGen->pEnd;` |
|        39 | 14796 | `		}` |
|        39 | 14797 | `	}` |
|       185 | 14798 | `	nLine = pTok ? pTok->nLine : (pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ? pGen->pIn[-1].nLine : 1);` |
|       185 | 14799 | `	if( pTok == 0 ){` |
|       ! 0 | 14800 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       ! 0 | 14801 | `			zExpecting ? "syntax error, unexpected end of file, expecting %s"` |
|         - | 14802 | `			           : "syntax error, unexpected end of file",` |
|       ! 0 | 14803 | `			zExpecting);` |
|         - | 14804 | `	}` |
|       185 | 14805 | `	if( pTok->nType & PH7_TK_ID ){` |
|        16 | 14806 | `		zNoun = "identifier";` |
|       178 | 14807 | `	}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|         8 | 14808 | `		zNoun = "variable";` |
|       169 | 14809 | `	}else if( pTok->nType & PH7_TK_INTEGER ){` |
|        24 | 14810 | `		zNoun = "integer";` |
|       156 | 14811 | `	}else if( pTok->nType & PH7_TK_REAL ){` |
|       ! 0 | 14812 | `		zNoun = "float";` |
|       ! 0 | 14813 | `	}` |
|       185 | 14814 | `	if( zExpecting ){` |
|       115 | 14815 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        37 | 14816 | `			"syntax error, unexpected %s \"%z\", expecting %s",zNoun,&pTok->sData,zExpecting);` |
|         - | 14817 | `	}` |
|       164 | 14818 | `	return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        53 | 14819 | `		"syntax error, unexpected %s \"%z\"",zNoun,&pTok->sData);` |
|        95 | 14820 | `}` |
|         - | 14821 | `/*` |
|         - | 14822 | ` * Generate a compile-time error message.` |
|         - | 14823 | ` * If the error count limit is reached (usually 15 error message)` |
|         - | 14824 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|         - | 14825 | ` * abort compilation immediately.` |
|         - | 14826 | ` */` |
|     15858 | 14827 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|         5 | 14828 | `{` |
|     15863 | 14829 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     15863 | 14830 | `	const char *zErr = "Error";` |
|         - | 14831 | `	SyString *pFile;` |
|         - | 14832 | `	va_list ap;` |
|         - | 14833 | `	sxi32 rc;` |
|         - | 14834 | `	/* Reset the working buffer */` |
|     15863 | 14835 | `	SyBlobReset(pWorker);` |
|         - | 14836 | `	/* Peek the processed file path if available */` |
|     15863 | 14837 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     15863 | 14838 | `	if( nErrType == E_ERROR \|\| nErrType == E_PARSE ){` |
|         - | 14839 | `		/* Increment the error counter. A PARSE error is every bit as fatal as an` |
|         - | 14840 | `		 * E_ERROR one: php compiles nothing, runs nothing and exits 255. Counting` |
|         - | 14841 | `		 * only E_ERROR let a parse error print its diagnostic and then fall through` |
|         - | 14842 | `		 * into execution with a 0 exit status. */` |
|       657 | 14843 | `		pGen->nErr++;` |
|       657 | 14844 | `		if( pGen->nErr > 15 ){` |
|         - | 14845 | `			/* Error count limit reached */` |
|         6 | 14846 | `			if( pGen->xErr ){` |
|         6 | 14847 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|         6 | 14848 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|         6 | 14849 | `				if( pFile ){` |
|         6 | 14850 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|         2 | 14851 | `				}` |
|         6 | 14852 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|         6 | 14853 | `				if( SyBlobLength(pWorker) > 0 ){` |
|         6 | 14854 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|         2 | 14855 | `				}` |
|         2 | 14856 | `			}` |
|         - | 14857 | `			/* Abort immediately */` |
|         6 | 14858 | `			return SXERR_ABORT;` |
|         - | 14859 | `		}` |
|       324 | 14860 | `	}` |
|     15859 | 14861 | `	if( pGen->xErr == 0 ){` |
|         - | 14862 | `		/* No available error consumer,return immediately */` |
|     15191 | 14863 | `		return SXRET_OK;` |
|         - | 14864 | `	}` |
|       673 | 14865 | `	switch(nErrType){` |
|       310 | 14866 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|        11 | 14867 | `	case E_WARNING: zErr = "Warning";     break;` |
|       344 | 14868 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|         6 | 14869 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|       ! 0 | 14870 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|       ! 0 | 14871 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|       ! 0 | 14872 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|        13 | 14873 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|       ! 0 | 14874 | `	default:` |
|       ! 0 | 14875 | `		break;` |
|         - | 14876 | `	}` |
|       673 | 14877 | `	rc = SXRET_OK;` |
|         - | 14878 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|       673 | 14879 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|       673 | 14880 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|       673 | 14881 | `	va_start(ap,zFormat);` |
|       673 | 14882 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|       673 | 14883 | `	va_end(ap);` |
|       673 | 14884 | `	if( pFile ){` |
|       673 | 14885 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       334 | 14886 | `	}` |
|         - | 14887 | `	/* Append a new line */` |
|       673 | 14888 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       673 | 14889 | `	if( SyBlobLength(pWorker) > 0 ){` |
|         - | 14890 | `		/* Consume the generated error message */` |
|       673 | 14891 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       334 | 14892 | `	}` |
|       673 | 14893 | `	return rc;` |
|      7934 | 14894 | `}` |
|         - | 14895 |  |
