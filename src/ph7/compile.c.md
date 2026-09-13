# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 7377/9102 lines (81.05%)

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
|    155700 |   139 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|         5 |   140 | `{` |
|    155705 |   141 | `	GenBlock *pBlock = pCurrent;` |
|    350139 |   142 | `	for(;;){` |
|    700283 |   143 | `		if( pBlock->iFlags & iBlockType ){` |
|    155705 |   144 | `			iCount--; /* Decrement nesting level */` |
|    155705 |   145 | `			if( iCount < 1 ){` |
|         - |   146 | `				/* Block meet with the desired criteria */` |
|    155679 |   147 | `				return pBlock;` |
|         - |   148 | `			}` |
|        13 |   149 | `		}` |
|         - |   150 | `		/* Point to the upper block */` |
|    544609 |   151 | `		pBlock = pBlock->pParent;` |
|    544609 |   152 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|         - |   153 | `			/* Forbidden */` |
|        15 |   154 | `			break;` |
|         - |   155 | `		}` |
|         5 |   156 | `	}` |
|         - |   157 | `	/* No such block */` |
|        28 |   158 | `	return 0;` |
|     77855 |   159 | `}` |
|         - |   160 | `/*` |
|         - |   161 | ` * Initialize a freshly allocated block instance.` |
|         - |   162 | ` */` |
|  12331220 |   163 | `static void GenStateInitBlock(` |
|         - |   164 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |   165 | `	GenBlock *pBlock,    /* Target block */` |
|         - |   166 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   167 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|         - |   168 | `	void *pUserData      /* Upper layer private data */` |
|         - |   169 | `	)` |
|         5 |   170 | `{` |
|         - |   171 | `	/* Initialize block fields */` |
|  12331225 |   172 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  12331225 |   173 | `	pBlock->pUserData   = pUserData;` |
|  12331225 |   174 | `	pBlock->pGen        = pGen;` |
|  12331225 |   175 | `	pBlock->iFlags      = iType;` |
|  12331225 |   176 | `	pBlock->pParent     = 0;` |
|  12331225 |   177 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  12331225 |   178 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  12331225 |   179 | `}` |
|         - |   180 | `/*` |
|         - |   181 | ` * Allocate a new block instance.` |
|         - |   182 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|         - |   183 | ` * on success.Otherwise generate a compile-time error and abort` |
|         - |   184 | ` * processing on failure.` |
|         - |   185 | ` */` |
|  12327328 |   186 | `static sxi32 GenStateEnterBlock(` |
|         - |   187 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |   188 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   189 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|         - |   190 | `	void *pUserData,      /* Upper layer private data */` |
|         - |   191 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|         - |   192 | `	)` |
|         5 |   193 | `{` |
|         - |   194 | `	GenBlock *pBlock;` |
|         - |   195 | `	/* Allocate a new block instance */` |
|  12327333 |   196 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  12327333 |   197 | `	if( pBlock == 0 ){` |
|         - |   198 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |   199 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |   200 | `		 */` |
|       ! 0 |   201 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|         - |   202 | `		/* Abort processing immediately */` |
|       ! 0 |   203 | `		return SXERR_ABORT;` |
|         - |   204 | `	}` |
|         - |   205 | `	/* Zero the structure */` |
|  12327333 |   206 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  12327333 |   207 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|         - |   208 | `	/* Link to the parent block */` |
|  12327333 |   209 | `	pBlock->pParent = pGen->pCurrent;` |
|         - |   210 | `	/* A loop or switch gets an id, and remembers the loop it nests inside, so a goto's` |
|         - |   211 | `	 * and a label's positions can be compared after compilation (see aLoopParent). */` |
|  12327333 |   212 | `	if( iType & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    525991 |   213 | `		sxu32 nParent = pGen->nCurLoopId;` |
|    525991 |   214 | `		pGen->nLoopId++;` |
|    525991 |   215 | `		SySetPut(&pGen->aLoopParent,(const void *)&nParent);` |
|    525991 |   216 | `		pBlock->nLoopId = pGen->nLoopId;` |
|    525991 |   217 | `		pBlock->nOuterLoopId = nParent;` |
|    525991 |   218 | `		pGen->nCurLoopId = pGen->nLoopId;` |
|    262993 |   219 | `	}` |
|         - |   220 | `	/* Mark as the current block */` |
|  12327333 |   221 | `	pGen->pCurrent = pBlock;` |
|  12327333 |   222 | `	if( ppBlock ){` |
|         - |   223 | `		/* Write a pointer to the new instance */` |
|   5909573 |   224 | `		*ppBlock = pBlock;` |
|   2954784 |   225 | `	}` |
|  12327333 |   226 | `	return SXRET_OK;` |
|   6163669 |   227 | `}` |
|         - |   228 | `/*` |
|         - |   229 | ` * Release block fields without freeing the whole instance.` |
|         - |   230 | ` */` |
|  12327316 |   231 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|         5 |   232 | `{` |
|  12327321 |   233 | `	SySetRelease(&pBlock->aPostContFix);` |
|  12327321 |   234 | `	SySetRelease(&pBlock->aJumpFix);` |
|  12327321 |   235 | `}` |
|         - |   236 | `/*` |
|         - |   237 | ` * Release a block.` |
|         - |   238 | ` */` |
|  12327312 |   239 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|         5 |   240 | `{` |
|  12327317 |   241 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  12327317 |   242 | `	GenStateReleaseBlock(&(*pBlock));` |
|         - |   243 | `	/* Free the instance */` |
|  12327317 |   244 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  12327317 |   245 | `}` |
|         - |   246 | `/*` |
|         - |   247 | ` * POP and release a block from the stack of compiled blocks.` |
|         - |   248 | ` */` |
|  12327312 |   249 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|         5 |   250 | `{` |
|  12327317 |   251 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  12327317 |   252 | `	if( pBlock == 0 ){` |
|         - |   253 | `		/* No more block to pop */` |
|       ! 0 |   254 | `		return SXERR_EMPTY;` |
|         - |   255 | `	}` |
|  12327317 |   256 | `	if( pBlock->iFlags & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    525983 |   257 | `		pGen->nCurLoopId = pBlock->nOuterLoopId;` |
|    262989 |   258 | `	}` |
|         - |   259 | `	/* Point to the upper block */` |
|  12327317 |   260 | `	pGen->pCurrent = pBlock->pParent;` |
|  12327317 |   261 | `	if( ppBlock ){` |
|         - |   262 | `		/* Write a pointer to the popped block */` |
|       ! 0 |   263 | `		*ppBlock = pBlock;` |
|       ! 0 |   264 | `	}else{` |
|         - |   265 | `		/* Safely release the block */` |
|  12327317 |   266 | `		GenStateFreeBlock(&(*pBlock));` |
|         - |   267 | `	}` |
|  12327317 |   268 | `	return SXRET_OK;` |
|   6163661 |   269 | `}` |
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
|   4563432 |   280 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|         5 |   281 | `{` |
|         - |   282 | `	JumpFixup sJumpFix;` |
|         - |   283 | `	sxi32 rc;` |
|         - |   284 | `	/* Init the JumpFixup structure */` |
|   4563437 |   285 | `	sJumpFix.nJumpType = nJumpType;` |
|   4563437 |   286 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|         - |   287 | `	/* Insert in the jump fixup table */` |
|   4563437 |   288 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|   4563437 |   289 | `	return rc;` |
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
|   8593158 |   302 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|         5 |   303 | `{` |
|         - |   304 | `	JumpFixup *aFix;` |
|         - |   305 | `	VmInstr *pInstr;` |
|         - |   306 | `	sxu32 nFixed;` |
|         - |   307 | `	sxu32 n;` |
|         - |   308 | `	/* Point to the jump fixup table */` |
|   8593163 |   309 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|         - |   310 | `	/* Fix the desired jumps */` |
|  18281731 |   311 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|   9688573 |   312 | `		if( aFix[n].nJumpType < 0 ){` |
|         - |   313 | `			/* Already fixed */` |
|   3615751 |   314 | `			continue;` |
|         - |   315 | `		}` |
|   6072827 |   316 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|         - |   317 | `			/* Not of our interest */` |
|   1509397 |   318 | `			continue;` |
|         - |   319 | `		}` |
|         - |   320 | `		/* Point to the instruction to fix */` |
|   4563435 |   321 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|   4563435 |   322 | `		if( pInstr ){` |
|   4563435 |   323 | `			pInstr->iP2 = nJumpDest;` |
|   4563435 |   324 | `			nFixed++;` |
|         - |   325 | `			/* Mark as fixed */` |
|   4563435 |   326 | `			aFix[n].nJumpType = -1;` |
|   2281715 |   327 | `		}` |
|   2281720 |   328 | `	}` |
|         - |   329 | `	/* Total number of fixed jumps */` |
|   8593163 |   330 | `	return nFixed;` |
|         5 |   331 | `}` |
|         - |   332 | `/*` |
|         - |   333 | ` * Fix a 'goto' now the jump destination is resolved.` |
|         - |   334 | ` * The goto statement can be used to jump to another section` |
|         - |   335 | ` * in the program.` |
|         - |   336 | ` * Refer to the routine responsible of compiling the goto` |
|         - |   337 | ` * statement for more information.` |
|         - |   338 | ` */` |
|   3082022 |   339 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|         5 |   340 | `{` |
|         - |   341 | `	JumpFixup *pJump,*aJumps;` |
|         - |   342 | `	Label *pLabel;` |
|         - |   343 | `	VmInstr *pInstr;` |
|         - |   344 | `	sxi32 rc;` |
|         - |   345 | `	sxu32 n;` |
|         - |   346 | `	/* Point to the goto table */` |
|   3082027 |   347 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|         - |   348 | `	/* Fix */` |
|   3082173 |   349 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|   3082025 |   400 | `	return SXRET_OK;` |
|   1541016 |   401 | `}` |
|         - |   402 | `/*` |
|         - |   403 | ` * Check if a given token value is installed in the literal table.` |
|         - |   404 | ` */` |
|  15598462 |   405 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|         5 |   406 | `{` |
|         - |   407 | `	SyHashEntry *pEntry;` |
|  15598467 |   408 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  15598467 |   409 | `	if( pEntry == 0 ){` |
|   4000341 |   410 | `		return SXERR_NOTFOUND;` |
|         - |   411 | `	}` |
|  11598131 |   412 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  11598131 |   413 | `	return SXRET_OK;` |
|   7799236 |   414 | `}` |
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
|   4000336 |   425 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|         5 |   426 | `{` |
|   4000341 |   427 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|   4000341 |   428 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   2000168 |   429 | `	}` |
|   4000341 |   430 | `	return SXRET_OK;` |
|         5 |   431 | `}` |
|         - |   432 | `/*` |
|         - |   433 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|         - |   434 | ` * in the constant table.` |
|         - |   435 | ` */` |
|   3765910 |   436 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|         5 |   437 | `{` |
|         - |   438 | `	ph7_value *pObj;` |
|   3765915 |   439 | `	sxu32 nIdx = 0; /* cc warning */` |
|         - |   440 | `	/* Reserve a new constant */` |
|   3765915 |   441 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   3765915 |   442 | `	if( pObj == 0 ){` |
|       ! 0 |   443 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   444 | `		return 0;` |
|         - |   445 | `	}` |
|   3765915 |   446 | `	*pIdx = nIdx;` |
|         - |   447 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|         - |   448 | `	 * the constant string iterals table [optimization purposes].` |
|         - |   449 | `	 */` |
|   3765915 |   450 | `	return pObj;` |
|   1882960 |   451 | `}` |
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
|   7394694 |   466 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|         5 |   467 | `{` |
|         - |   468 | `	VmCallArgMap *pMap;` |
|   7394699 |   469 | `	if( !pGen->bStrictTypes ) return p3;` |
|        39 |   470 | `	if( p3 == 0 ){` |
|        35 |   471 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|        35 |   472 | `		if( pMap == 0 ) return 0;` |
|        35 |   473 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|        35 |   474 | `		p3 = (void *)pMap;` |
|        16 |   475 | `	}` |
|        39 |   476 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|        39 |   477 | `	return p3;` |
|   3697352 |   478 | `}` |
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
|   3774726 |   542 | `static int GenStateFindBadNumericSeparator(` |
|         - |   543 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|         5 |   544 | `{` |
|   3774731 |   545 | `	const char *z = pRaw->zString;` |
|   3774731 |   546 | `	sxu32 n = pRaw->nByte;` |
|   3774731 |   547 | `	int base = 10;` |
|         - |   548 | `	sxu32 i, start;` |
|   3774731 |   549 | `	if( n < 2 ) return 0;` |
|    807471 |   550 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|    105073 |   551 | `		base = 16;` |
|    754937 |   552 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|       286 |   553 | `		base = 2;` |
|       142 |   554 | `	}` |
|   3074837 |   555 | `	for( i = 0; i < n; ++i ){` |
|   2267385 |   556 | `		if( z[i] != '_' ) continue;` |
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
|    807457 |   573 | `	return 0;` |
|   1887368 |   574 | `}` |
|         - |   575 | `/*` |
|         - |   576 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|         - |   577 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|         - |   578 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|         - |   579 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|         - |   580 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|         - |   581 | ` * so callers can bail from the current construct).` |
|         - |   582 | ` */` |
|   3774726 |   583 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|         5 |   584 | `{` |
|   3774731 |   585 | `	const char *zBad = 0;` |
|   3774731 |   586 | `	sxu32 nBad = 0;` |
|         - |   587 | `	SyString sBad;` |
|         - |   588 | `	sxi32 rc;` |
|   3774731 |   589 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|   3774717 |   590 | `		return SXRET_OK;` |
|         - |   591 | `	}` |
|        18 |   592 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|        18 |   593 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|         - |   594 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|        18 |   595 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |   596 | `		return SXERR_ABORT;` |
|         - |   597 | `	}` |
|        18 |   598 | `	return SXERR_SYNTAX;` |
|   1887368 |   599 | `}` |
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
|   3774712 |   616 | `static sxi32 GenStateStripNumericSeparators(` |
|         - |   617 | `	SyMemBackend *pAlloc,` |
|         - |   618 | `	const SyString *pToken,` |
|         - |   619 | `	char *zScratch, sxu32 nScratch,` |
|         - |   620 | `	SyString *pOut, char **pzAlloc)` |
|         5 |   621 | `{` |
|         - |   622 | `	sxu32 i, j;` |
|   3774717 |   623 | `	int hasUnderscore = 0;` |
|         - |   624 | `	char *zBuf;` |
|   3774717 |   625 | `	*pzAlloc = 0;` |
|   9007267 |   626 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|   5232809 |   627 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   2616280 |   628 | `	}` |
|   3774717 |   629 | `	if( !hasUnderscore ){` |
|   3774463 |   630 | `		SyStringDupPtr(pOut, pToken);` |
|   3774463 |   631 | `		return SXRET_OK;` |
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
|   1887361 |   648 | `}` |
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
|   3765944 |   684 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|         5 |   685 | `{` |
|   3765949 |   686 | `	const char *z = pNum->zString;` |
|   3765949 |   687 | `	const char *zEnd = z + pNum->nByte;` |
|         - |   688 | `	const char *p, *q;` |
|         - |   689 | `	int n;` |
|   3765949 |   690 | `	*pbDecimal = FALSE;` |
|   3765949 |   691 | `	if( z >= zEnd ){` |
|       ! 0 |   692 | `		return FALSE;` |
|         - |   693 | `	}` |
|   3765949 |   694 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|         - |   695 | `		/* Hexadecimal: INT64_MAX == 0x7FFF...F (16 digits, leading nibble 7). */` |
|    105071 |   696 | `		p = z + 2;` |
|    132295 |   697 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|    428249 |   698 | `		for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){ n++; }` |
|    105071 |   699 | `		if( n < 16 \|\| (n == 16 && SyHexToint(p[0]) < 8) ){` |
|    105065 |   700 | `			return FALSE;` |
|         - |   701 | `		}` |
|         7 |   702 | `		{ ph7_real dv = 0;` |
|       103 |   703 | `		  for( q = p; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){` |
|        97 |   704 | `			dv = dv * 16 + (ph7_real)SyHexToint(q[0]);` |
|        49 |   705 | `		  }` |
|         7 |   706 | `		  *pReal = dv;` |
|         - |   707 | `		}` |
|         7 |   708 | `		return TRUE;` |
|   3660883 |   709 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
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
|   3660601 |   724 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'o' \|\| z[1] == 'O') ){` |
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
|   3660585 |   739 | `	}else if( z[0] == '0' ){` |
|         - |   740 | `		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the` |
|         - |   741 | `		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1` |
|         - |   742 | `		 * "0o" marker ends the run and leaves it to the int path (as today). */` |
|   1367107 |   743 | `		p = z;` |
|   2734211 |   744 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|   1379027 |   745 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|   1367107 |   746 | `		if( n <= 21 ){` |
|   1367105 |   747 | `			return FALSE;` |
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
|   2293483 |   760 | `	p = z;` |
|   2293483 |   761 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
|   5557167 |   762 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|   2293483 |   763 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|        25 |   764 | `		*pbDecimal = TRUE;` |
|        25 |   765 | `		return TRUE;` |
|         - |   766 | `	}` |
|   2293459 |   767 | `	return FALSE;` |
|   1882977 |   768 | `}` |
|   3774698 |   769 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |   770 | `{` |
|   3774703 |   771 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   3774703 |   772 | `	sxu32 nIdx = 0;` |
|         - |   773 | `	char zScratch[GEN_NUM_SCRATCH];` |
|   3774703 |   774 | `	char *zAlloc = 0;` |
|         - |   775 | `	SyString sNum;` |
|         - |   776 | `	sxi32 rc;` |
|   1887349 |   777 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|   3774703 |   778 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|   3774703 |   779 | `	if( rc != SXRET_OK ){` |
|        14 |   780 | `		return rc;` |
|         - |   781 | `	}` |
|   5662037 |   782 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   1887344 |   783 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|   3774693 |   784 | `	if( rc != SXRET_OK ){` |
|       ! 0 |   785 | `		return SXERR_ABORT;` |
|         - |   786 | `	}` |
|   3774693 |   787 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|         - |   788 | `		ph7_value *pObj;` |
|         - |   789 | `		sxi64 iValue;` |
|   3765949 |   790 | `		ph7_real rOverflow = 0;` |
|   3765949 |   791 | `		int bDecimalOverflow = 0;` |
|   3765949 |   792 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
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
|   3765915 |   809 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|   3765915 |   810 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   3765915 |   811 | `			if( pObj == 0 ){` |
|       ! 0 |   812 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       ! 0 |   813 | `				return SXERR_ABORT;` |
|         - |   814 | `			}` |
|   3765915 |   815 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|         - |   816 | `		}` |
|   1882977 |   817 | `	}else{` |
|         - |   818 | `		/* Real number */` |
|         - |   819 | `		ph7_value *pObj;` |
|         - |   820 | `		/* Reserve a new constant */` |
|      8749 |   821 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      8749 |   822 | `		if( pObj == 0 ){` |
|       ! 0 |   823 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   824 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       ! 0 |   825 | `			return SXERR_ABORT;` |
|         - |   826 | `		}` |
|      8749 |   827 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|      8749 |   828 | `		PH7_MemObjToReal(pObj);` |
|         - |   829 | `	}` |
|   3774693 |   830 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         - |   831 | `	/* Emit the load constant instruction */` |
|   3774693 |   832 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |   833 | `	/* Node successfully compiled */` |
|   3774693 |   834 | `	return SXRET_OK;` |
|   1887354 |   835 | `}` |
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
|   5301170 |   847 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |   848 | `{` |
|   5301175 |   849 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|         - |   850 | `	const char *zIn,*zCur,*zEnd;` |
|         - |   851 | `	ph7_value *pObj;` |
|         - |   852 | `	sxu32 nIdx;` |
|         - |   853 | `	sxi32 bHasEsc;` |
|   5301175 |   854 | `	nIdx = 0; /* Prevent compiler warning */` |
|         - |   855 | `	/* Delimit the string */` |
|   5301175 |   856 | `	zIn  = pStr->zString;` |
|   5301175 |   857 | `	zEnd = &zIn[pStr->nByte];` |
|   5301175 |   858 | `	if( zIn >= zEnd ){` |
|         - |   859 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|         - |   860 | `		 * rather than reserving a new object each time. */` |
|    385139 |   861 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    385139 |   862 | `		return SXRET_OK;` |
|         - |   863 | `	}` |
|         - |   864 | `	/* A single-quoted literal whose raw source holds a backslash unescapes to a` |
|         - |   865 | `	 * value that differs from that source (\\ -> \, \' -> '). The literal cache` |
|         - |   866 | `	 * keys FIND on the raw source text but INSTALL on the unescaped value, so` |
|         - |   867 | `	 * caching such a literal lets an unrelated one collide with it — e.g. '\\'` |
|         - |   868 | `	 * (raw \\, value \) would find the entry installed for '\\\\' (raw \\\\,` |
|         - |   869 | `	 * value \\) and load two backslashes. Only cache literals whose value equals` |
|         - |   870 | `	 * their source, i.e. those with no backslash to unescape. */` |
|   4916041 |   871 | `	bHasEsc = 0;` |
|         - |   872 | `	{` |
|         - |   873 | `		const char *zScan;` |
|  58715661 |   874 | `		for( zScan = zIn ; zScan < zEnd ; zScan++ ){` |
|  53881457 |   875 | `			if( zScan[0] == '\\' ){ bHasEsc = 1; break; }` |
|  26899815 |   876 | `		}` |
|         - |   877 | `	}` |
|   4916041 |   878 | `	if( !bHasEsc && SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|         - |   879 | `		/* Already processed,emit the load constant instruction` |
|         - |   880 | `		 * and return.` |
|         - |   881 | `		 */` |
|   2920817 |   882 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   2920817 |   883 | `		return SXRET_OK;` |
|         - |   884 | `	}` |
|         - |   885 | `	/* Reserve a new constant */` |
|   1995229 |   886 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   1995229 |   887 | `	if( pObj == 0 ){` |
|       ! 0 |   888 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   889 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |   890 | `		return SXERR_ABORT;` |
|         - |   891 | `	}` |
|   1995229 |   892 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|         - |   893 | `	/* Compile the node */` |
|   2047821 |   894 | `	for(;;){` |
|   4095647 |   895 | `		if( zIn >= zEnd ){` |
|         - |   896 | `			/* End of input */` |
|   1995229 |   897 | `			break;` |
|         - |   898 | `		}` |
|   2100423 |   899 | `		zCur = zIn;` |
|  41314455 |   900 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  39214037 |   901 | `			zIn++;` |
|         5 |   902 | `		}` |
|   2100423 |   903 | `		if( zIn > zCur ){` |
|         - |   904 | `			/* Append raw contents*/` |
|   2057577 |   905 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   1028786 |   906 | `		}` |
|   2100423 |   907 | `		zIn++;` |
|   2100423 |   908 | `		if( zIn < zEnd ){` |
|    144127 |   909 | `			if( zIn[0] == '\\' ){` |
|         - |   910 | `				/* A literal backslash */` |
|     35065 |   911 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|    126597 |   912 | `			}else if( zIn[0] == '\'' ){` |
|         - |   913 | `				/* A single quote */` |
|        15 |   914 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|         8 |   915 | `			}else{` |
|         - |   916 | `				/* verbatim copy */` |
|    109053 |   917 | `				zIn--;` |
|    109053 |   918 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|    109053 |   919 | `				zIn++;` |
|         - |   920 | `			}` |
|     72061 |   921 | `		}` |
|         - |   922 | `		/* Advance the stream cursor */` |
|   2100423 |   923 | `		zIn++;` |
|         5 |   924 | `	}` |
|         - |   925 | `	/* Emit the load constant instruction */` |
|   1995229 |   926 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   1995229 |   927 | `	if( !bHasEsc && pStr->nByte < 1024 ){` |
|         - |   928 | `		/* Install in the literal table (only when value == source; see above) */` |
|   1913397 |   929 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|    956696 |   930 | `	}` |
|         - |   931 | `	/* Node successfully compiled */` |
|   1995229 |   932 | `	return SXRET_OK;` |
|   2650590 |   933 | `}` |
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
|       118 |   952 | `static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)` |
|         5 |   953 | `{` |
|       123 |   954 | `	SyString *pIn = &pGen->pIn->sData;` |
|       123 |   955 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - |   956 | `	const char *zPrefix;` |
|         - |   957 | `	const char *z, *zEnd;` |
|         - |   958 | `	char *zBuf, *zDst;` |
|       123 |   959 | `	if( nIndent == 0 ){` |
|         - |   960 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|        78 |   961 | `		*pOut = *pIn;` |
|        78 |   962 | `		return SXRET_OK;` |
|         - |   963 | `	}` |
|         - |   964 | `	/* Recover the marker indent prefix from the original source buffer.` |
|         - |   965 | `	 * Skip the terminator the lexer stripped: one '\n' plus an optional` |
|         - |   966 | `	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the` |
|         - |   967 | `	 * lexer stripped nothing, so this offset is one byte past the true` |
|         - |   968 | `	 * marker-indent start. That is harmless — the strip loop below never` |
|         - |   969 | `	 * runs (z == zEnd), and zPrefix is never dereferenced. */` |
|        48 |   970 | `	zPrefix = pIn->zString + pIn->nByte;` |
|        48 |   971 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|       ! 0 |   972 | `		zPrefix += 2;` |
|       ! 0 |   973 | `	}else{` |
|        48 |   974 | `		zPrefix += 1;` |
|         - |   975 | `	}` |
|         - |   976 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|        48 |   977 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|        48 |   978 | `	if( zBuf == 0 ){` |
|       ! 0 |   979 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |   980 | `		return SXERR_ABORT;` |
|         - |   981 | `	}` |
|        48 |   982 | `	zDst = zBuf;` |
|        48 |   983 | `	z = pIn->zString;` |
|        48 |   984 | `	zEnd = z + pIn->nByte;` |
|       130 |   985 | `	while( z < zEnd ){` |
|        72 |   986 | `		const char *zLine = z;` |
|         - |   987 | `		sxu32 nLine;` |
|         - |   988 | `		int bEmpty;` |
|       800 |   989 | `		while( z < zEnd && z[0] != '\n' ){` |
|       732 |   990 | `			z++;` |
|         4 |   991 | `		}` |
|        72 |   992 | `		nLine = (sxu32)(z - zLine);` |
|        72 |   993 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|        72 |   994 | `		if( !bEmpty ){` |
|         - |   995 | `			sxu32 i;` |
|        68 |   996 | `			if( nLine < nIndent ){` |
|       ! 0 |   997 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |   998 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|       ! 0 |   999 | `					nIndent);` |
|       ! 0 |  1000 | `				return SXERR_ABORT;` |
|         - |  1001 | `			}` |
|       270 |  1002 | `			for( i = 0; i < nIndent; i++ ){` |
|       214 |  1003 | `				if( zLine[i] != zPrefix[i] ){` |
|        11 |  1004 | `					unsigned char c = (unsigned char)zLine[i];` |
|        11 |  1005 | `					if( c == ' ' \|\| c == '\t' ){` |
|         6 |  1006 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  1007 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|         4 |  1008 | `					}else{` |
|         8 |  1009 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  1010 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|         2 |  1011 | `							nIndent);` |
|         - |  1012 | `					}` |
|        11 |  1013 | `					return SXERR_ABORT;` |
|         - |  1014 | `				}` |
|       104 |  1015 | `			}` |
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
|        64 |  1030 | `}` |
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
|        50 |  1045 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1046 | `{` |
|         - |  1047 | `	SyString sStripped;` |
|         - |  1048 | `	SyString *pStr;` |
|         - |  1049 | `	ph7_value *pObj;` |
|         - |  1050 | `	sxu32 nIdx;` |
|         - |  1051 | `	sxi32 rc;` |
|        55 |  1052 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|        55 |  1053 | `	if( rc != SXRET_OK ){` |
|         6 |  1054 | `		return rc;` |
|         - |  1055 | `	}` |
|        49 |  1056 | `	pStr = &sStripped;` |
|        49 |  1057 | `	nIdx = 0; /* Prevent compiler warning */` |
|        49 |  1058 | `	if( pStr->nByte <= 0 ){` |
|         - |  1059 | `		/* An empty nowdoc is the empty STRING, like '' -- loading NULL here made` |
|         - |  1060 | `		 * strlen(<<<'EOD'EOD;) deprecation-warn about a null argument. */` |
|         7 |  1061 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|         7 |  1062 | `		return SXRET_OK;` |
|         - |  1063 | `	}` |
|         - |  1064 | `	/* Reserve a new constant */` |
|        43 |  1065 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        43 |  1066 | `	if( pObj == 0 ){` |
|       ! 0 |  1067 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  1068 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  1069 | `		return SXERR_ABORT;` |
|         - |  1070 | `	}` |
|         - |  1071 | `	/* No processing is done here, simply a memcpy() operation */` |
|        43 |  1072 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|         - |  1073 | `	/* Emit the load constant instruction */` |
|        43 |  1074 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |  1075 | `	/* Node successfully compiled */` |
|        43 |  1076 | `	return SXRET_OK;` |
|        30 |  1077 | `}` |
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
|      2650 |  1100 | `static sxi32 GenStateProcessStringExpression(` |
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
|      2655 |  1111 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         - |  1112 | `	/* Preallocate some slots */` |
|      2655 |  1113 | `	SySetAlloc(&sToken,0x08);` |
|         - |  1114 | `	/* Tokenize the text */` |
|      2655 |  1115 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken,0);` |
|         - |  1116 | `	/* Swap delimiter */` |
|      2655 |  1117 | `	pTmpIn  = pGen->pIn;` |
|      2655 |  1118 | `	pTmpEnd = pGen->pEnd;` |
|      2655 |  1119 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      2655 |  1120 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|         - |  1121 | `	/* Compile the expression */` |
|      2655 |  1122 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  1123 | `	/* Restore token stream */` |
|      2655 |  1124 | `	pGen->pIn  = pTmpIn;` |
|      2655 |  1125 | `	pGen->pEnd = pTmpEnd;` |
|         - |  1126 | `	/* Release the token set */` |
|      2655 |  1127 | `	SySetRelease(&sToken);` |
|         - |  1128 | `	/* Compilation result */` |
|      2655 |  1129 | `	return rc;` |
|         5 |  1130 | `}` |
|         - |  1131 | `/*` |
|         - |  1132 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|         - |  1133 | ` */` |
|    127900 |  1134 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|         5 |  1135 | `{` |
|         - |  1136 | `	ph7_value *pConstObj;` |
|    127905 |  1137 | `	sxu32 nIdx = 0;` |
|         - |  1138 | `	/* Reserve a new constant */` |
|    127905 |  1139 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    127905 |  1140 | `	if( pConstObj == 0 ){` |
|       ! 0 |  1141 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  1142 | `		return 0;` |
|         - |  1143 | `	}` |
|    127905 |  1144 | `	(*pCount)++;` |
|    127905 |  1145 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|         - |  1146 | `	/* Emit the load constant instruction */` |
|    127905 |  1147 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    127905 |  1148 | `	return pConstObj;` |
|     63955 |  1149 | `}` |
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
|    126328 |  1212 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|         5 |  1213 | `{` |
|    126333 |  1214 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|         - |  1215 | `	const char *zIn,*zCur,*zEnd;` |
|    126333 |  1216 | `	ph7_value *pObj = 0;` |
|         - |  1217 | `	sxi32 iCons;` |
|         - |  1218 | `	sxi32 rc;` |
|         - |  1219 | `	/* Delimit the string */` |
|    126333 |  1220 | `	zIn  = pStr->zString;` |
|    126333 |  1221 | `	zEnd = &zIn[pStr->nByte];` |
|    126333 |  1222 | `	if( zIn >= zEnd ){` |
|         - |  1223 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|         - |  1224 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|         - |  1225 | `		 * literal table from growing when many "" literals appear in the source.` |
|         - |  1226 | `		 */` |
|       415 |  1227 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|       415 |  1228 | `		return SXRET_OK;` |
|         - |  1229 | `	}` |
|    125923 |  1230 | `	zCur = 0;` |
|         - |  1231 | `	/* Compile the node */` |
|    125923 |  1232 | `	iCons = 0;` |
|     64282 |  1233 | `	for(;;){` |
|    173757 |  1234 | `		zCur = zIn;` |
|   1695697 |  1235 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|   1524595 |  1236 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|        69 |  1237 | `				break;` |
|   1524468 |  1238 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|      2528 |  1239 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|      1264 |  1240 | `					break;` |
|         - |  1241 | `			}` |
|   1521945 |  1242 | `			zIn++;` |
|         5 |  1243 | `		}` |
|    173757 |  1244 | `		if( zIn > zCur ){` |
|     96719 |  1245 | `			if( pObj == 0 ){` |
|     96073 |  1246 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|     96073 |  1247 | `				if( pObj == 0 ){` |
|       ! 0 |  1248 | `					return SXERR_ABORT;` |
|         - |  1249 | `				}` |
|     48034 |  1250 | `			}` |
|     96719 |  1251 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|     48357 |  1252 | `		}` |
|    173757 |  1253 | `		if( zIn >= zEnd ){` |
|    125921 |  1254 | `			break;` |
|         - |  1255 | `		}` |
|     47841 |  1256 | `		if( zIn[0] == '\\' ){` |
|     45191 |  1257 | `			const char *zPtr = 0;` |
|         - |  1258 | `			sxu32 n;` |
|     45191 |  1259 | `			zIn++;` |
|     45191 |  1260 | `			if( pObj == 0 ){` |
|     31837 |  1261 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|     31837 |  1262 | `				if( pObj == 0 ){` |
|       ! 0 |  1263 | `					return SXERR_ABORT;` |
|         - |  1264 | `				}` |
|     15916 |  1265 | `			}` |
|     45191 |  1266 | `			if( zIn >= zEnd ){` |
|         - |  1267 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|         3 |  1268 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|         3 |  1269 | `				break;` |
|         - |  1270 | `			}` |
|     45189 |  1271 | `			n = sizeof(char); /* size of conversion */` |
|     45189 |  1272 | `			switch( zIn[0] ){` |
|        17 |  1273 | `			case '$':` |
|         - |  1274 | `				/* Dollar sign */` |
|        38 |  1275 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|        38 |  1276 | `				break;` |
|        62 |  1277 | `			case '\\':` |
|         - |  1278 | `				/* A literal backslash */` |
|       129 |  1279 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|       129 |  1280 | `				break;` |
|         1 |  1281 | `			case 'e':` |
|         - |  1282 | `				/* Escape (ESC) ASCII code 27 */` |
|         3 |  1283 | `				PH7_MemObjStringAppend(pObj,"\x1b",sizeof(char));` |
|         3 |  1284 | `				break;` |
|         4 |  1285 | `			case 'f':` |
|         - |  1286 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|         9 |  1287 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|         9 |  1288 | `				break;` |
|     19950 |  1289 | `			case 'n':` |
|         - |  1290 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|     39905 |  1291 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|     39905 |  1292 | `				break;` |
|        27 |  1293 | `			case 'r':` |
|         - |  1294 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|        59 |  1295 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|        59 |  1296 | `				break;` |
|      1975 |  1297 | `			case 't':` |
|         - |  1298 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      3955 |  1299 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      3955 |  1300 | `				break;` |
|         3 |  1301 | `			case 'v':` |
|         - |  1302 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|         7 |  1303 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|         7 |  1304 | `				break;` |
|       149 |  1305 | `			case '"':` |
|       303 |  1306 | `				if( bHeredoc ){` |
|         - |  1307 | `					/* No active quote char in a heredoc: php keeps \" verbatim */` |
|         5 |  1308 | `					PH7_MemObjStringAppend(pObj,"\\\"",sizeof(char)*2);` |
|         3 |  1309 | `				}else{` |
|         - |  1310 | `					/* Double quote */` |
|       299 |  1311 | `					PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|         - |  1312 | `				}` |
|       303 |  1313 | `				break;` |
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
|       355 |  1338 | `			case 'x':` |
|      1065 |  1339 | `				if( &zIn[1] < zEnd && SyisHex((unsigned char)zIn[1]) ){` |
|         - |  1340 | `					/* \x[0-9A-Fa-f]{1,2}: a character in hexadecimal notation */` |
|       708 |  1341 | `					int c = SyHexToint(zIn[1]);` |
|         - |  1342 | `					char cOut;` |
|       708 |  1343 | `					n += sizeof(char);` |
|       708 |  1344 | `					if( &zIn[2] < zEnd && SyisHex((unsigned char)zIn[2]) ){` |
|       704 |  1345 | `						c = (c << 4) + SyHexToint(zIn[2]);` |
|       704 |  1346 | `						n += sizeof(char);` |
|       351 |  1347 | `					}` |
|       708 |  1348 | `					cOut = (char)c; /* value byte, independent of host endianness */` |
|       708 |  1349 | `					PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|       355 |  1350 | `				}else{` |
|         - |  1351 | `					/* Not an escape: keep the backslash, as php does */` |
|         5 |  1352 | `					PH7_MemObjStringAppend(pObj,"\\x",sizeof(char)*2);` |
|         - |  1353 | `				}` |
|       712 |  1354 | `				break;` |
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
|     45189 |  1415 | `			zIn += n;` |
|     45189 |  1416 | `			continue;` |
|         - |  1417 | `		}` |
|      2655 |  1418 | `		if( zIn[0] == '{' ){` |
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
|      2523 |  1452 | `			const char *zExpr = zIn;` |
|         - |  1453 | `			/* Assemble variable name */` |
|      1284 |  1454 | `			for(;;){` |
|         - |  1455 | `				/* Jump leading dollars */` |
|      5091 |  1456 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|      2523 |  1457 | `					zIn++;` |
|         5 |  1458 | `				}` |
|      1284 |  1459 | `				for(;;){` |
|     13135 |  1460 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|      9283 |  1461 | `						zIn++;` |
|         5 |  1462 | `					}` |
|      2573 |  1463 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|         - |  1464 | `						/* UTF-8 stream */` |
|       ! 0 |  1465 | `						zIn++;` |
|       ! 0 |  1466 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|       ! 0 |  1467 | `							zIn++;` |
|       ! 0 |  1468 | `						}` |
|       ! 0 |  1469 | `						continue;` |
|         - |  1470 | `					}` |
|      2573 |  1471 | `					break;` |
|       ! 0 |  1472 | `				}` |
|      2573 |  1473 | `				if( zIn >= zEnd ){` |
|       273 |  1474 | `					break;` |
|         - |  1475 | `				}` |
|      2305 |  1476 | `				if( zIn[0] == '[' ){` |
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
|      2295 |  1494 | `				}else if(zIn[0] == '{' ){` |
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
|      2291 |  1512 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|         - |  1513 | `					/* Member access operator '->' */` |
|        53 |  1514 | `					zIn += 2;` |
|      2266 |  1515 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|         - |  1516 | `					/* Static member access operator '::' */` |
|       ! 0 |  1517 | `					zIn += 2;` |
|       ! 0 |  1518 | `				}else{` |
|      1123 |  1519 | `					break;` |
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
|      2523 |  1531 | `				const char *zBr = zExpr;` |
|     14437 |  1532 | `				while( zBr < zIn && zBr[0] != '[' ){` |
|     11919 |  1533 | `					zBr++;` |
|         5 |  1534 | `				}` |
|      2523 |  1535 | `				if( zBr < zIn && zIn[-1] == ']' ){` |
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
|      2516 |  1576 | `			if( &zExpr[1] < zIn && zExpr[0] == '$' && zExpr[1] == '{' && zIn[-1] == '}'` |
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
|      2519 |  1612 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|      2519 |  1613 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1614 | `				return SXERR_ABORT;` |
|         - |  1615 | `			}` |
|      2519 |  1616 | `			if( rc != SXERR_EMPTY ){` |
|      2517 |  1617 | `				++iCons;` |
|      1256 |  1618 | `			}` |
|         - |  1619 | `		}` |
|         - |  1620 | `		/* Invalidate the previously used constant */` |
|      2651 |  1621 | `		pObj = 0;` |
|         5 |  1622 | `	}/*for(;;)*/` |
|    125923 |  1623 | `	if( iCons > 1 ){` |
|         - |  1624 | `		/* Concatenate all compiled constants */` |
|      1917 |  1625 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|       956 |  1626 | `	}` |
|         - |  1627 | `	/* Node successfully compiled */` |
|    125923 |  1628 | `	return SXRET_OK;` |
|     63169 |  1629 | `}` |
|         - |  1630 | `/*` |
|         - |  1631 | ` * Compile a double quoted string.` |
|         - |  1632 | ` *  See the block-comment above for more information.` |
|         - |  1633 | ` */` |
|    126264 |  1634 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1635 | `{` |
|         - |  1636 | `	sxi32 rc;` |
|    126269 |  1637 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|     63132 |  1638 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  1639 | `	/* Compilation result */` |
|    126269 |  1640 | `	return rc;` |
|         5 |  1641 | `}` |
|         - |  1642 | `/*` |
|         - |  1643 | ` * Compile a Heredoc string.` |
|         - |  1644 | ` *  See the block-comment above for more information.` |
|         - |  1645 | ` */` |
|        68 |  1646 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1647 | `{` |
|         - |  1648 | `	SyString sOrig, sStripped;` |
|         - |  1649 | `	sxi32 rc;` |
|        73 |  1650 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|        73 |  1651 | `	if( rc != SXRET_OK ){` |
|         6 |  1652 | `		return rc;` |
|         - |  1653 | `	}` |
|         - |  1654 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|         - |  1655 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|         - |  1656 | `	 * Restore before returning so downstream code that references pIn is` |
|         - |  1657 | `	 * unaffected, including on the error path. */` |
|        68 |  1658 | `	sOrig = pGen->pIn->sData;` |
|        68 |  1659 | `	pGen->pIn->sData = sStripped;` |
|        68 |  1660 | `	rc = GenStateCompileString(&(*pGen),1/*bHeredoc*/);` |
|        68 |  1661 | `	pGen->pIn->sData = sOrig;` |
|        32 |  1662 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        68 |  1663 | `	return rc;` |
|        39 |  1664 | `}` |
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
|   1463578 |  1684 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   1463583 |  1695 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|         - |  1696 | `	/* Compile the expression*/` |
|   1463583 |  1697 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|         - |  1698 | `	/* Restore token stream */` |
|   1463583 |  1699 | `	RE_SWAP_DELIMITER(pGen);` |
|   1463583 |  1700 | `	return rc;` |
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
|        17 |  1716 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|         - |  1717 | `			/* Unexpected expression */` |
|        14 |  1718 | `			rc = PH7_GenSyntaxError(&(*pGen),pRoot->pStart,"\"->\" or \"?->\" or \"[\"");` |
|        14 |  1719 | `			if( rc != SXERR_ABORT ){` |
|        14 |  1720 | `				rc = SXERR_INVALID;` |
|         5 |  1721 | `			}` |
|        10 |  1722 | `		}` |
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
|   1411694 |  1739 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|         5 |  1740 | `{` |
|   1411699 |  1741 | `	SyToken *pCur = pStart;` |
|   1411699 |  1742 | `	sxi32 iNest = 0;` |
|   3647679 |  1743 | `	while( pCur < pEnd ){` |
|   2747805 |  1744 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    511821 |  1745 | `			return pCur;` |
|         - |  1746 | `		}` |
|         - |  1747 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|         - |  1748 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|         - |  1749 | `		 * not an entry separator. Skip past the signature.` |
|         - |  1750 | `		 */` |
|   2235989 |  1751 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|     23439 |  1752 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|     23439 |  1753 | `			SyToken *pFn = pCur;` |
|     23434 |  1754 | `			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd` |
|       ! 0 |  1755 | `				&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|         5 |  1756 | `				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|       ! 0 |  1757 | `				pFn = &pCur[1];` |
|       ! 0 |  1758 | `				nKw = PH7_TKWRD_FN;` |
|       ! 0 |  1759 | `			}` |
|     23439 |  1760 | `			if( nKw == PH7_TKWRD_FN ){` |
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
|     23435 |  1791 | `			if( nKw == PH7_TKWRD_MATCH ){` |
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
|     11714 |  1811 | `		}` |
|   2235983 |  1812 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     55013 |  1813 | `			iNest++;` |
|   2208479 |  1814 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|         - |  1815 | `			/* Don't worry about mismatched brackets here, the expression` |
|         - |  1816 | `			 * parser will shortly detect any syntax error. */` |
|     55013 |  1817 | `			iNest--;` |
|     27504 |  1818 | `		}` |
|   2235983 |  1819 | `		pCur++;` |
|         5 |  1820 | `	}` |
|    899879 |  1821 | `	return pEnd;` |
|    705852 |  1822 | `}` |
|         - |  1823 | `/*` |
|         - |  1824 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|         - |  1825 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|         - |  1826 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|         - |  1827 | ` */` |
|    637966 |  1828 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|         5 |  1829 | `{` |
|         - |  1830 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|         - |  1831 | `	SyToken *pKey,*pCur;` |
|    637971 |  1832 | `	sxi32 iEmitRef = 0;` |
|    637971 |  1833 | `	sxi32 iSpread = 0;` |
|    637971 |  1834 | `	sxi32 nPair = 0;` |
|         - |  1835 | `	sxi32 rc;` |
|    637971 |  1836 | `	xValidator = 0;` |
|    862948 |  1837 | `	for(;;){` |
|         - |  1838 | `		/* Jump leading commas. Exactly ONE separates two entries; a second one (or a comma` |
|         - |  1839 | `		 * at the very start) means an EMPTY element, which php rejects outright — PH7 just` |
|         - |  1840 | ``		 * skipped them, so `array(,)` and `array(1,,2)` compiled silently. A TRAILING comma`` |
|         - |  1841 | `		 * is legal and is handled by the loop exiting on the next pass. */` |
|    543965 |  1842 | `		{` |
|   1725901 |  1843 | `			int nSkip = 0;` |
|   2561517 |  1844 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    835621 |  1845 | `				nSkip++;` |
|    835621 |  1846 | `				pGen->pIn++;` |
|         5 |  1847 | `			}` |
|   1725901 |  1848 | `			if( nSkip > 1 \|\| (nSkip > 0 && nPair < 1) ){` |
|       ! 0 |  1849 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn[-1].nLine,` |
|         - |  1850 | `					"Cannot use empty array elements in arrays");` |
|       ! 0 |  1851 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  1852 | `					return SXERR_ABORT;` |
|         - |  1853 | `				}` |
|       ! 0 |  1854 | `				return SXRET_OK;` |
|         - |  1855 | `			}` |
|         - |  1856 | `		}` |
|   1725901 |  1857 | `		pCur = pGen->pIn;` |
|   1725901 |  1858 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|         - |  1859 | `			/* No more entry to process */` |
|    637953 |  1860 | `			break;` |
|         - |  1861 | `		}` |
|   1087953 |  1862 | `		if( pCur >= pGen->pIn ){` |
|       ! 0 |  1863 | `			continue;` |
|         - |  1864 | `		}` |
|         - |  1865 | `		/* Compile the key if available */` |
|   1087953 |  1866 | `		pKey = pCur;` |
|   1087953 |  1867 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   1087953 |  1868 | `		rc = SXERR_EMPTY;` |
|   1087953 |  1869 | `		if( pCur < pGen->pIn ){` |
|    375377 |  1870 | `			if( pKey == pCur ){` |
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
|    375375 |  1884 | `			if( &pCur[1] >= pGen->pIn ){` |
|         - |  1885 | ``				/* `array(1 => )`: php names the token that SHOULD have started the value —`` |
|         - |  1886 | `				 * the ')' or ']' closing the literal — not the '=>' it just read. Passing 0` |
|         - |  1887 | `				 * makes the helper reach for the token past this entry's slice. */` |
|        13 |  1888 | `				rc = PH7_GenSyntaxError(&(*pGen),0,0);` |
|        13 |  1889 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  1890 | `					return SXERR_ABORT;` |
|         - |  1891 | `				}` |
|        13 |  1892 | `				return SXRET_OK;` |
|         - |  1893 | `			}` |
|         - |  1894 | `			/* Compile the expression holding the key */` |
|    375365 |  1895 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|         - |  1896 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    375365 |  1897 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1898 | `				return SXERR_ABORT;` |
|         - |  1899 | `			}` |
|    375365 |  1900 | `			pCur++; /* Jump the '=>' operator */` |
|    187685 |  1901 | `		}else{` |
|         - |  1902 | `			/* Reset back the cursor and point to the entry value */` |
|    712581 |  1903 | `			pCur = pKey;` |
|         - |  1904 | `		}` |
|   1087941 |  1905 | `		if( rc == SXERR_EMPTY ){` |
|         - |  1906 | `			/* No key given: load the nil, TAGGED so LOAD_MAP knows this is an absent key` |
|         - |  1907 | ``			 * (auto-index) rather than an explicit `null =>` one, which php deprecates. */`` |
|    712581 |  1908 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,PH7_LOADC_NOKEY,0 /* nil index */,0,0);` |
|    356288 |  1909 | `		}` |
|   1087941 |  1910 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|   1087939 |  1929 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   1087939 |  1930 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
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
|   1631900 |  1947 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,` |
|    543965 |  1948 | `			iEmitRef ? EXPR_FLAG_LOAD_IDX_STORE` |
|         - |  1949 | `			         : EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,` |
|    543965 |  1950 | `			xValidator);` |
|   1087935 |  1951 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  1952 | `			return SXERR_ABORT;` |
|         - |  1953 | `		}` |
|   1087935 |  1954 | `		if( iSpread ){` |
|         - |  1955 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|        73 |  1956 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   1087900 |  1957 | `		}else if( iEmitRef ){` |
|         - |  1958 | `			/* Emit the load reference instruction */` |
|        41 |  1959 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|        18 |  1960 | `		}` |
|   1087935 |  1961 | `		xValidator = 0;` |
|   1087935 |  1962 | `		iEmitRef = 0;` |
|   1087935 |  1963 | `		iSpread = 0;` |
|   1087935 |  1964 | `		nPair++;` |
|         5 |  1965 | `	}` |
|         - |  1966 | `	/* Emit the load map instruction */` |
|    637953 |  1967 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|         - |  1968 | `	/* Node successfully compiled */` |
|    637953 |  1969 | `	return SXRET_OK;` |
|    318988 |  1970 | `}` |
|         - |  1971 | `/*` |
|         - |  1972 | ` * Compile the 'array' language construct.` |
|         - |  1973 | ` *	 According to the PHP language reference manual` |
|         - |  1974 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|         - |  1975 | ` *   values to keys. This type is optimized for several different uses; it can` |
|         - |  1976 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|         - |  1977 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|         - |  1978 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|         - |  1979 | ` */` |
|    417870 |  1980 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1981 | `{` |
|         - |  1982 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|    417875 |  1983 | `	pGen->pIn += 2;` |
|    417875 |  1984 | `	pGen->pEnd--;` |
|    208935 |  1985 | `	SXUNUSED(iCompileFlag);` |
|    417875 |  1986 | `	return GenStateCompileArrayBody(pGen);` |
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
|    220096 |  2085 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2086 | `{` |
|         - |  2087 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|    220101 |  2088 | `	pGen->pIn++;` |
|    220101 |  2089 | `	pGen->pEnd--;` |
|    110048 |  2090 | `	SXUNUSED(iCompileFlag);` |
|    220101 |  2091 | `	return GenStateCompileArrayBody(pGen);` |
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
|         3 |  2395 | `{` |
|         - |  2396 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|        87 |  2397 | `	pGen->pIn++;` |
|        87 |  2398 | `	pGen->pEnd--;` |
|        42 |  2399 | `	SXUNUSED(iCompileFlag);` |
|        87 |  2400 | `	return GenStateCompileListBody(pGen);` |
|         3 |  2401 | `}` |
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
|       594 |  2432 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2433 | `{` |
|       599 |  2434 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|         - |  2435 | `	char zName[512];         /* Unique lambda name */` |
|         - |  2436 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|         - |  2437 | `							  * one thread is allowed to compile the script.` |
|         - |  2438 | `						      */` |
|         - |  2439 | `	SyString sName;` |
|       599 |  2440 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `$f = #[A] function…` trivia`` |
|         - |  2441 | `	                              * is keyed to this ['static'] 'function' token */` |
|         - |  2442 | `	sxu32 nKwLine;` |
|       599 |  2443 | `	sxi32 iFlags = 0;` |
|         - |  2444 | `	sxu32 nLen;` |
|         - |  2445 | `	sxi32 rc;` |
|       297 |  2446 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  2447 |  |
|       599 |  2448 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|       594 |  2449 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       599 |  2450 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - |  2451 | `		/* Static closure: no $this auto-capture, bind refused */` |
|        23 |  2452 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|        23 |  2453 | `		pGen->pIn++; /* Jump the 'static' keyword */` |
|        11 |  2454 | `	}` |
|       599 |  2455 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|       599 |  2456 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|       ! 0 |  2457 | `		pGen->pIn++;` |
|       ! 0 |  2458 | `	}` |
|         - |  2459 | `	/* Generate a unique name */` |
|       599 |  2460 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|         - |  2461 | `	/* Make sure the generated name is unique */` |
|       599 |  2462 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 |  2463 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       ! 0 |  2464 | `	}` |
|       599 |  2465 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|         - |  2466 | `	/* Compile the lambda body */` |
|       599 |  2467 | `	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,&pAnnonFunc);` |
|       599 |  2468 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2469 | `		return SXERR_ABORT;` |
|         - |  2470 | `	}` |
|       599 |  2471 | `	if( pAnnonFunc ){` |
|       599 |  2472 | `		pAnnonFunc->nLine = nKwLine;` |
|         - |  2473 | ``		/* Expression-position attributes (`$f = #[A] function () {}`): the trivia`` |
|         - |  2474 | `		 * sidecar keys them to the closure's first keyword token. */` |
|       599 |  2475 | `		if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnnonFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  2476 | `			return SXERR_ABORT;` |
|         - |  2477 | `		}` |
|       297 |  2478 | `	}` |
|         - |  2479 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|         - |  2480 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|         - |  2481 | `	 * the handler wraps either in a Closure instance. */` |
|       599 |  2482 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|         - |  2483 | `	/* Node successfully compiled */` |
|       599 |  2484 | `	return SXRET_OK;` |
|       302 |  2485 | `}` |
|         - |  2486 | `/*` |
|         - |  2487 | ` * Add a free variable to the arrow function's closure environment, unless` |
|         - |  2488 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|         - |  2489 | ` * enclosing arrow level, or has already been captured.` |
|         - |  2490 | ` */` |
|       250 |  2491 | `static sxi32 GenStateArrowAddCapture(` |
|         - |  2492 | `	ph7_gen_state *pGen,` |
|         - |  2493 | `	ph7_vm_func *pFunc,` |
|         - |  2494 | `	const char *zName,` |
|         - |  2495 | `	sxu32 nByte,` |
|         - |  2496 | `	SyString *aShadow,` |
|         - |  2497 | `	sxu32 nShadow)` |
|         4 |  2498 | `{` |
|         - |  2499 | `	ph7_vm_func_closure_env sEnv;` |
|         - |  2500 | `	ph7_vm_func_closure_env *aEnv;` |
|         - |  2501 | `	sxu32 n, nEnv;` |
|         - |  2502 | `	char *zDup;` |
|       254 |  2503 | `	if( nByte == 0 ){` |
|       ! 0 |  2504 | `		return SXRET_OK;` |
|         - |  2505 | `	}` |
|       250 |  2506 | `	if( nByte == sizeof("this")-1` |
|       138 |  2507 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|         9 |  2508 | `		return SXRET_OK;` |
|         - |  2509 | `	}` |
|       306 |  2510 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|       226 |  2511 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|       218 |  2512 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|       170 |  2513 | `			return SXRET_OK;` |
|         - |  2514 | `		}` |
|        32 |  2515 | `	}` |
|        77 |  2516 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        77 |  2517 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|       105 |  2518 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|        30 |  2519 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|        29 |  2520 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|         3 |  2521 | `			return SXRET_OK;` |
|         - |  2522 | `		}` |
|        15 |  2523 | `	}` |
|        75 |  2524 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|        75 |  2525 | `	if( zDup == 0 ){` |
|       ! 0 |  2526 | `		return SXERR_ABORT;` |
|         - |  2527 | `	}` |
|        75 |  2528 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|        75 |  2529 | `	sEnv.iFlags = 0;` |
|        75 |  2530 | `	sEnv.nIdx = SXU32_HIGH;` |
|        75 |  2531 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|        75 |  2532 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|        75 |  2533 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|        75 |  2534 | `	return SXRET_OK;` |
|       129 |  2535 | `}` |
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
|         2 |  2549 | `{` |
|         - |  2550 | `	sxi32 rc;` |
|       588 |  2551 | `	while( zIn < zEnd ){` |
|       480 |  2552 | `		if( zIn[0] == '\\' ){` |
|        13 |  2553 | `			zIn++;` |
|        13 |  2554 | `			if( zIn < zEnd ){` |
|        13 |  2555 | `				zIn++;` |
|         6 |  2556 | `			}` |
|        13 |  2557 | `			continue;` |
|         - |  2558 | `		}` |
|       466 |  2559 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|        26 |  2560 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
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
|       444 |  2589 | `		zIn++;` |
|         2 |  2590 | `	}` |
|       110 |  2591 | `	return SXRET_OK;` |
|        56 |  2592 | `}` |
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
|       522 |  2604 | `static sxi32 GenStateArrowCaptureScan(` |
|         - |  2605 | `	ph7_gen_state *pGen,` |
|         - |  2606 | `	ph7_vm_func *pFunc,` |
|         - |  2607 | `	SyToken *pStart,` |
|         - |  2608 | `	SyToken *pEnd,` |
|         - |  2609 | `	SyString *aShadow,` |
|         - |  2610 | `	sxu32 nShadow)` |
|         5 |  2611 | `{` |
|       527 |  2612 | `	SyToken *pScan = pStart;` |
|         - |  2613 | `	sxi32 rc;` |
|      3535 |  2614 | `	while( pScan < pEnd ){` |
|      3013 |  2615 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|       164 |  2616 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|        54 |  2617 | `				pScan->sData.zString,` |
|       108 |  2618 | `				pScan->sData.zString + pScan->sData.nByte,` |
|        54 |  2619 | `				aShadow,nShadow);` |
|       110 |  2620 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2621 | `				return SXERR_ABORT;` |
|         - |  2622 | `			}` |
|       110 |  2623 | `			pScan++;` |
|       110 |  2624 | `			continue;` |
|         - |  2625 | `		}` |
|      2905 |  2626 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|        41 |  2627 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|        41 |  2628 | `			SyToken *pFnKw = pScan;` |
|        38 |  2629 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|         2 |  2630 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|         4 |  2631 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|       ! 0 |  2632 | `				pFnKw = &pScan[1];` |
|       ! 0 |  2633 | `				nKw = PH7_TKWRD_FN;` |
|       ! 0 |  2634 | `			}` |
|        41 |  2635 | `			if( nKw == PH7_TKWRD_FN ){` |
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
|         7 |  2777 | `		}` |
|      2881 |  2778 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|      2655 |  2779 | `			pScan++;` |
|      2655 |  2780 | `			continue;` |
|         - |  2781 | `		}` |
|         - |  2782 | `		{` |
|         - |  2783 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|       230 |  2784 | `			SyToken *pDollar = pScan;` |
|       339 |  2785 | `			while( &pDollar[1] < pEnd` |
|       230 |  2786 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|       ! 0 |  2787 | `				pDollar++;` |
|       ! 0 |  2788 | `			}` |
|       230 |  2789 | `			if( &pDollar[1] >= pEnd ){` |
|       ! 0 |  2790 | `				break;` |
|         - |  2791 | `			}` |
|       230 |  2792 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  2793 | `				pScan = pDollar + 1;` |
|       ! 0 |  2794 | `				continue;` |
|         - |  2795 | `			}` |
|       343 |  2796 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|       226 |  2797 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|       113 |  2798 | `				aShadow,nShadow);` |
|       230 |  2799 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2800 | `				return SXERR_ABORT;` |
|         - |  2801 | `			}` |
|       230 |  2802 | `			pScan = pDollar + 2;` |
|         - |  2803 | `		}` |
|         4 |  2804 | `	}` |
|       527 |  2805 | `	return SXRET_OK;` |
|       266 |  2806 | `}` |
|         - |  2807 | `/*` |
|         - |  2808 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|         - |  2809 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|         - |  2810 | ` * variables by value. The body is a single expression that acts as an` |
|         - |  2811 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|         - |  2812 | ` * $this is also made available.` |
|         - |  2813 | ` */` |
|       498 |  2814 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
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
|       503 |  2831 | `	sxi32 iFlags = 0;` |
|       503 |  2832 | `	int bStatic = 0;` |
|         - |  2833 | `	sxi32 rc;` |
|         - |  2834 | `	sxu32 n;` |
|       249 |  2835 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  2836 |  |
|       503 |  2837 | `	nLine = pGen->pIn->nLine;` |
|         - |  2838 | ``	/* Attribute-sidecar key: `#[A] [static] fn` trivia is keyed to this token */`` |
|       503 |  2839 | `	pTokKw = pGen->pIn;` |
|         - |  2840 | `	/* Optional 'static' prefix */` |
|       498 |  2841 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       503 |  2842 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         9 |  2843 | `		bStatic = 1;` |
|         9 |  2844 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|         9 |  2845 | `		pGen->pIn++;` |
|         4 |  2846 | `	}` |
|         - |  2847 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|       498 |  2848 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|       503 |  2849 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|       ! 0 |  2850 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2851 | `			"Arrow function: expected 'fn' keyword");` |
|       ! 0 |  2852 | `		return SXERR_SYNTAX;` |
|         - |  2853 | `	}` |
|       503 |  2854 | `	pGen->pIn++; /* Jump 'fn' */` |
|         - |  2855 | `	/* Optional '&' — return by reference */` |
|       503 |  2856 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  2857 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       ! 0 |  2858 | `		pGen->pIn++;` |
|       ! 0 |  2859 | `	}` |
|         - |  2860 | `	/* Expect '(' */` |
|       503 |  2861 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
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
|       501 |  2872 | `	pGen->pIn++; /* Jump '(' */` |
|         - |  2873 | `	/* Delimit the parameter list */` |
|       501 |  2874 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|       501 |  2875 | `	if( pSigEnd >= pGen->pEnd ){` |
|         3 |  2876 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  2877 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|         3 |  2878 | `		return SXERR_SYNTAX;` |
|         - |  2879 | `	}` |
|         - |  2880 | `	/* Allocate the function state */` |
|       499 |  2881 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|       499 |  2882 | `	if( pFunc == 0 ){` |
|       ! 0 |  2883 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2884 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2885 | `		return SXERR_ABORT;` |
|         - |  2886 | `	}` |
|         - |  2887 | `	/* Generate a unique lambda name */` |
|       499 |  2888 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       499 |  2889 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 |  2890 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       ! 0 |  2891 | `	}` |
|       499 |  2892 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|       499 |  2893 | `	if( zDup == 0 ){` |
|       ! 0 |  2894 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2895 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2896 | `		return SXERR_ABORT;` |
|         - |  2897 | `	}` |
|       499 |  2898 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|         - |  2899 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|       499 |  2900 | `	pFunc->nLine = nLine;` |
|         - |  2901 | ``	/* Expression-position attributes (`$f = #[A] fn () => …`) */`` |
|       499 |  2902 | `	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  2903 | `		return SXERR_ABORT;` |
|         - |  2904 | `	}` |
|         - |  2905 | `	/* Collect function arguments */` |
|       499 |  2906 | `	if( pGen->pIn < pSigEnd ){` |
|       142 |  2907 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|       142 |  2908 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2909 | `			return SXERR_ABORT;` |
|         - |  2910 | `		}` |
|        69 |  2911 | `	}` |
|         - |  2912 | `	/* Point past ')' and parse optional return type */` |
|       499 |  2913 | `	pGen->pIn = &pSigEnd[1];` |
|       499 |  2914 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|       499 |  2915 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2916 | `		return SXERR_ABORT;` |
|       499 |  2917 | `	}else if( rc == SXERR_SYNTAX ){` |
|       ! 0 |  2918 | `		return SXERR_SYNTAX;` |
|         - |  2919 | `	}` |
|         - |  2920 | `	/* Expect '=>' */` |
|       499 |  2921 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
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
|       497 |  2932 | `	pGen->pIn++; /* Jump '=>' */` |
|       497 |  2933 | `	pBodyStart = pGen->pIn;` |
|       497 |  2934 | `	pBodyEnd = pGen->pEnd;` |
|         - |  2935 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|         - |  2936 | `	 * recursively collect free-variable references from the body. The scan` |
|         - |  2937 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|         - |  2938 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|       497 |  2939 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|         - |  2940 | `	{` |
|       497 |  2941 | `		SyString *aShadow = 0;` |
|       497 |  2942 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|       497 |  2943 | `		if( nShadow > 0 ){` |
|       140 |  2944 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|       136 |  2945 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|       140 |  2946 | `			if( aShadow == 0 ){` |
|       ! 0 |  2947 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2948 | `					"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2949 | `				return SXERR_ABORT;` |
|         - |  2950 | `			}` |
|       312 |  2951 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|       176 |  2952 | `				aShadow[n] = aArgs[n].sName;` |
|        90 |  2953 | `			}` |
|        68 |  2954 | `		}` |
|       743 |  2955 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|       246 |  2956 | `			aShadow,nShadow);` |
|       497 |  2957 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2958 | `			return SXERR_ABORT;` |
|         - |  2959 | `		}` |
|         - |  2960 | `	}` |
|         - |  2961 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|         - |  2962 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|         - |  2963 | `	 * captured value is silently dropped when the enclosing scope has no` |
|         - |  2964 | `	 * $this. */` |
|       497 |  2965 | `	if( !bStatic ){` |
|         - |  2966 | `		char *zThisDup;` |
|       489 |  2967 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|       489 |  2968 | `		if( zThisDup == 0 ){` |
|       ! 0 |  2969 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2970 | `				"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2971 | `			return SXERR_ABORT;` |
|         - |  2972 | `		}` |
|       489 |  2973 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       489 |  2974 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|       489 |  2975 | `		sEnv.nIdx = SXU32_HIGH;` |
|       489 |  2976 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       489 |  2977 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|       489 |  2978 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       242 |  2979 | `	}` |
|         - |  2980 | `	/* Arrow functions are always closures */` |
|       497 |  2981 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|         - |  2982 | `	/* Compile the body expression as an implicit return */` |
|       743 |  2983 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       246 |  2984 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|       497 |  2985 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  2986 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2987 | `			"PH7 engine is running out-of-memory");` |
|       ! 0 |  2988 | `		return SXERR_ABORT;` |
|         - |  2989 | `	}` |
|       497 |  2990 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       497 |  2991 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       497 |  2992 | `	pSavedEnd = pGen->pEnd;` |
|       497 |  2993 | `	pGen->pIn = pBodyStart;` |
|       497 |  2994 | `	pGen->pEnd = pBodyEnd;` |
|       497 |  2995 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       497 |  2996 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2997 | `		return SXERR_ABORT;` |
|         - |  2998 | `	}` |
|         - |  2999 | `	/* The cursor stopped just past the body expression */` |
|       497 |  3000 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|         - |  3001 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|         - |  3002 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|         - |  3003 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|         - |  3004 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|       497 |  3005 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       497 |  3006 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       497 |  3007 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       497 |  3008 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       497 |  3009 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - |  3010 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|       497 |  3011 | `	pGen->pIn = pBodyEnd;` |
|       497 |  3012 | `	pGen->pEnd = pSavedEnd;` |
|         - |  3013 | `	/* Emit the load-closure instruction */` |
|       497 |  3014 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|       497 |  3015 | `	return SXRET_OK;` |
|       254 |  3016 | `}` |
|         - |  3017 | `/*` |
|         - |  3018 | ` * Compile a single arm's expression range into a freshly-allocated` |
|         - |  3019 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|         - |  3020 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|         - |  3021 | ` * expression's value.` |
|         - |  3022 | ` */` |
|       364 |  3023 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|         - |  3024 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|         3 |  3025 | `{` |
|         - |  3026 | `	SySet *pInstrContainer;` |
|         - |  3027 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  3028 | `	GenBlock *pArmBlock;` |
|         - |  3029 | `	sxi32 rc;` |
|       367 |  3030 | `	pTmpIn  = pGen->pIn;` |
|       367 |  3031 | `	pTmpEnd = pGen->pEnd;` |
|       367 |  3032 | `	pGen->pIn  = pStart;` |
|       367 |  3033 | `	pGen->pEnd = pStop;` |
|       367 |  3034 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       367 |  3035 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|         - |  3036 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|         - |  3037 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|         - |  3038 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|         - |  3039 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|         - |  3040 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|       549 |  3041 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       182 |  3042 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|       367 |  3043 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3044 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  3045 | `		pGen->pIn  = pTmpIn;` |
|       ! 0 |  3046 | `		pGen->pEnd = pTmpEnd;` |
|       ! 0 |  3047 | `		return SXERR_ABORT;` |
|         - |  3048 | `	}` |
|       367 |  3049 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       367 |  3050 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       367 |  3051 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       367 |  3052 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       367 |  3053 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       367 |  3054 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       367 |  3055 | `	pGen->pIn  = pTmpIn;` |
|       367 |  3056 | `	pGen->pEnd = pTmpEnd;` |
|       367 |  3057 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  3058 | `		return SXERR_ABORT;` |
|         - |  3059 | `	}` |
|       367 |  3060 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 |  3061 | `		return SXERR_EMPTY;` |
|         - |  3062 | `	}` |
|       367 |  3063 | `	return SXRET_OK;` |
|       185 |  3064 | `}` |
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
|       366 |  3100 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|         4 |  3101 | `{` |
|       370 |  3102 | `	SyToken *pCur = pStart;` |
|       370 |  3103 | `	int iNest = 0;` |
|       890 |  3104 | `	while( pCur < pEnd ){` |
|       854 |  3105 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        17 |  3106 | `			iNest++;` |
|       846 |  3107 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        17 |  3108 | `			iNest--;` |
|       830 |  3109 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|       333 |  3110 | `			return pCur;` |
|         - |  3111 | `		}` |
|       524 |  3112 | `		pCur++;` |
|         4 |  3113 | `	}` |
|        39 |  3114 | `	return pEnd;` |
|       187 |  3115 | `}` |
|        74 |  3116 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3117 | `{` |
|         - |  3118 | `	ph7_match *pMatch;` |
|         - |  3119 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|        79 |  3120 | `	int bHasDefault = 0;` |
|         - |  3121 | `	sxu32 nLine;` |
|         - |  3122 | `	sxi32 rc;` |
|        37 |  3123 | `	SXUNUSED(iCompileFlag);` |
|        79 |  3124 | `	nLine = pGen->pIn->nLine;` |
|        79 |  3125 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|         - |  3126 | `	/* Expect '(' */` |
|        79 |  3127 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  3128 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  3129 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|       ! 0 |  3130 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|         - |  3131 | `	}` |
|        79 |  3132 | `	pGen->pIn++; /* Jump '(' */` |
|        79 |  3133 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|        79 |  3134 | `	if( pSubjEnd >= pGen->pEnd ){` |
|       ! 0 |  3135 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  3136 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|         - |  3137 | `	}` |
|        79 |  3138 | `	if( pGen->pIn >= pSubjEnd ){` |
|       ! 0 |  3139 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  3140 | `			"syntax error, unexpected \")\", expecting match subject");` |
|         - |  3141 | `	}` |
|         - |  3142 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|        79 |  3143 | `	pSavedEnd = pGen->pEnd;` |
|        79 |  3144 | `	pGen->pEnd = pSubjEnd;` |
|        79 |  3145 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        79 |  3146 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  3147 | `		return SXERR_ABORT;` |
|         - |  3148 | `	}` |
|        79 |  3149 | `	pGen->pEnd = pSavedEnd;` |
|        79 |  3150 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|         - |  3151 | `	/* Expect '{' */` |
|        79 |  3152 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|       ! 0 |  3153 | `		return GenStateMatchError(pGen,` |
|       ! 0 |  3154 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|         - |  3155 | `			"syntax error, expecting \"{\" after match subject");` |
|         - |  3156 | `	}` |
|        79 |  3157 | `	pGen->pIn++; /* Jump '{' */` |
|        79 |  3158 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|        79 |  3159 | `	if( pBodyEnd >= pGen->pEnd ){` |
|       ! 0 |  3160 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  3161 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|         - |  3162 | `	}` |
|         - |  3163 | `	/* Allocate ph7_match container */` |
|        79 |  3164 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|        79 |  3165 | `	if( pMatch == 0 ){` |
|       ! 0 |  3166 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  3167 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3168 | `		return SXERR_ABORT;` |
|         - |  3169 | `	}` |
|        79 |  3170 | `	SyZero(pMatch,sizeof(ph7_match));` |
|        79 |  3171 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|         - |  3172 | `	/* Iterate arms */` |
|       267 |  3173 | `	while( pGen->pIn < pBodyEnd ){` |
|         - |  3174 | `		ph7_match_arm sArm;` |
|         - |  3175 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|       196 |  3176 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|       196 |  3177 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|       196 |  3178 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|       196 |  3179 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - |  3180 | `		/* 'default' arm? */` |
|       192 |  3181 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       111 |  3182 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|        24 |  3183 | `			if( bHasDefault ){` |
|         3 |  3184 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|         - |  3185 | `					"Match expressions may only contain one default arm");` |
|         4 |  3186 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  3187 | `			}` |
|        22 |  3188 | `			sArm.bDefault = 1;` |
|        22 |  3189 | `			bHasDefault = 1;` |
|        22 |  3190 | `			pGen->pIn++;` |
|        22 |  3191 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       ! 0 |  3192 | `				return GenStateMatchError(pGen,nArmLine,` |
|         - |  3193 | `					"syntax error, expecting \"=>\" after 'default'");` |
|         - |  3194 | `			}` |
|        22 |  3195 | `			pGen->pIn++; /* Jump '=>' */` |
|        12 |  3196 | `		}else{` |
|         - |  3197 | `			/* Condition list: cond (',' cond)* '=>' */` |
|       174 |  3198 | `			pCondStart = pGen->pIn;` |
|       174 |  3199 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|         - |  3200 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       182 |  3201 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
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
|       174 |  3217 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|         3 |  3218 | `				return GenStateMatchError(pGen,nArmLine,` |
|         - |  3219 | `					"syntax error, expecting \"=>\" in match arm");` |
|         - |  3220 | `			}` |
|       171 |  3221 | `			if( pCondStart >= pArrow ){` |
|       ! 0 |  3222 | `				return GenStateMatchError(pGen,nArmLine,` |
|         - |  3223 | `					"syntax error, empty match condition expression");` |
|         - |  3224 | `			}` |
|         - |  3225 | `			{` |
|         - |  3226 | `				SySet sCondBc;` |
|       171 |  3227 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       171 |  3228 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       171 |  3229 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  3230 | `					return SXERR_ABORT;` |
|         - |  3231 | `				}` |
|       171 |  3232 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|         - |  3233 | `			}` |
|       171 |  3234 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|         - |  3235 | `		}` |
|         - |  3236 | `		/* Compile result expression: up to top-level ',' or body end */` |
|       191 |  3237 | `		pResStart = pGen->pIn;` |
|       191 |  3238 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|       191 |  3239 | `		if( pResStart >= pResEnd ){` |
|       ! 0 |  3240 | `			return GenStateMatchError(pGen,nArmLine,` |
|         - |  3241 | `				"syntax error, expected expression after \"=>\"");` |
|         - |  3242 | `		}` |
|       191 |  3243 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|       191 |  3244 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3245 | `			return SXERR_ABORT;` |
|         - |  3246 | `		}` |
|       191 |  3247 | `		pGen->pIn = pResEnd;` |
|       191 |  3248 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       157 |  3249 | `			pGen->pIn++; /* Skip trailing ',' */` |
|        77 |  3250 | `		}` |
|       191 |  3251 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|         3 |  3252 | `	}` |
|        73 |  3253 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|        73 |  3254 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|        73 |  3255 | `	return SXRET_OK;` |
|        42 |  3256 | `}` |
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
|        78 |  3298 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3299 | `{` |
|         - |  3300 | `	SyString *pName;` |
|         - |  3301 | `	sxu32 nKeyID;` |
|         - |  3302 | `	sxi32 rc;` |
|         - |  3303 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|        83 |  3304 | `	pName = &pGen->pIn->sData;` |
|        83 |  3305 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        83 |  3306 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|        83 |  3307 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
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
|        75 |  3342 | `		sxi32 nArg = 0;` |
|        75 |  3343 | `		sxu32 nIdx = 0;` |
|        75 |  3344 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|        75 |  3345 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3346 | `			return SXERR_ABORT;` |
|        75 |  3347 | `		}else if(rc != SXERR_EMPTY ){` |
|        75 |  3348 | `			nArg = 1;` |
|        35 |  3349 | `		}` |
|        75 |  3350 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|         - |  3351 | `			ph7_value *pObj;` |
|         - |  3352 | `			/* Emit the call instruction */` |
|        35 |  3353 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        35 |  3354 | `			if( pObj == 0 ){` |
|       ! 0 |  3355 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3356 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  3357 | `				return SXERR_ABORT;` |
|         - |  3358 | `			}` |
|        35 |  3359 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|         - |  3360 | `			/* Install in the literal table */` |
|        35 |  3361 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|        15 |  3362 | `		}` |
|         - |  3363 | `		/* Emit the call instruction */` |
|        75 |  3364 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        75 |  3365 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|         - |  3366 | `	}` |
|         - |  3367 | `	/* Node successfully compiled */` |
|        83 |  3368 | `	return SXRET_OK;` |
|        44 |  3369 | `}` |
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
|  20482616 |  3391 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3392 | `{` |
|  20482621 |  3393 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  3394 | `	sxi32 iVv;` |
|         - |  3395 | `	sxi32 iP1;` |
|         - |  3396 | `	void *p3;` |
|         - |  3397 | `	sxi32 rc;` |
|  20482621 |  3398 | `	iVv = -1; /* Variable variable counter */` |
|  40965249 |  3399 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  20482633 |  3400 | `		pGen->pIn++;` |
|  20482633 |  3401 | `		iVv++;` |
|         5 |  3402 | `	}` |
|  20482621 |  3403 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|         - |  3404 | `		/* Invalid variable name */` |
|       ! 0 |  3405 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable or \"{\" or \"$\"");` |
|       ! 0 |  3406 | `		if( rc == SXERR_ABORT ){` |
|         - |  3407 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3408 | `			return SXERR_ABORT;` |
|         - |  3409 | `		}` |
|       ! 0 |  3410 | `		return SXRET_OK;` |
|         - |  3411 | `	}` |
|  20482621 |  3412 | `	p3  = 0;` |
|  20482621 |  3413 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
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
|  20482605 |  3439 | `		char *zName = 0;` |
|         - |  3440 | `		/* Extract variable name */` |
|  20482605 |  3441 | `		pName = &pGen->pIn->sData;` |
|         - |  3442 | `		/* Advance the stream cursor */` |
|  20482605 |  3443 | `		pGen->pIn++;` |
|  20482605 |  3444 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  20482605 |  3445 | `		if( pEntry == 0 ){` |
|         - |  3446 | `			/* Duplicate name */` |
|   1216023 |  3447 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   1216023 |  3448 | `			if( zName == 0 ){` |
|       ! 0 |  3449 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3450 | `				return SXERR_ABORT;` |
|         - |  3451 | `			}` |
|         - |  3452 | `			/* Install in the hashtable */` |
|   1216023 |  3453 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|    608014 |  3454 | `		}else{` |
|         - |  3455 | `			/* Name already available */` |
|  19266587 |  3456 | `			zName = (char *)pEntry->pUserData;` |
|         - |  3457 | `		}` |
|  20482605 |  3458 | `		p3 = (void *)zName;` |
|         - |  3459 | `	}` |
|  20482617 |  3460 | `	iP1 = 0;` |
|  20482617 |  3461 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|   6039969 |  3462 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|         - |  3463 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|   6036057 |  3464 | `			iP1 = 1;` |
|   3018026 |  3465 | `		}` |
|   3019982 |  3466 | `	}` |
|         - |  3467 | `	/* Emit the load instruction */` |
|  20482617 |  3468 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  20482629 |  3469 | `	while( iVv > 0 ){` |
|        13 |  3470 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|        13 |  3471 | `		iVv--;` |
|         1 |  3472 | `	}` |
|         - |  3473 | `	/* Node successfully compiled */` |
|  20482617 |  3474 | `	return SXRET_OK;` |
|  10241313 |  3475 | `}` |
|         - |  3476 | `/*` |
|         - |  3477 | ` * Load a literal.` |
|         - |  3478 | ` */` |
|  12832444 |  3479 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|         5 |  3480 | `{` |
|  12832449 |  3481 | `	SyToken *pToken = pGen->pIn;` |
|         - |  3482 | `	ph7_value *pObj;` |
|         - |  3483 | `	SyString *pStr;` |
|         - |  3484 | `	sxu32 nIdx;` |
|         - |  3485 | `	/* Extract token value */` |
|  12832449 |  3486 | `	pStr = &pToken->sData;` |
|         - |  3487 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first. A reserved` |
|         - |  3488 | `	 * word used as a member NAME (Enum::Null, C::Array, $o->list()) is a plain` |
|         - |  3489 | `	 * identifier — the parser flagged its token so the whole value-folding chain is` |
|         - |  3490 | `	 * skipped and it falls through to the ordinary string-literal emit below. */` |
|  12832449 |  3491 | `	if( pToken->nType & PH7_TK_MEMBER_NAME ){` |
|         - |  3492 | `		/* fall through to the plain-string literal path */` |
|  10231579 |  3493 | `	}else if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   1516071 |  3494 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|         - |  3495 | `			/* NULL constant are always indexed at 0 */` |
|   1004093 |  3496 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   1004093 |  3497 | `			return SXRET_OK;` |
|    511983 |  3498 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|         - |  3499 | `			/* TRUE constant are always indexed at 1 */` |
|    339565 |  3500 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|    339565 |  3501 | `			return SXRET_OK;` |
|         5 |  3502 | `		}` |
|   6968651 |  3503 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   1535598 |  3504 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|         - |  3505 | `			/* FALSE constant are always indexed at 2 */` |
|    747173 |  3506 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|    747173 |  3507 | `			return SXRET_OK;` |
|   5514206 |  3508 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|    293462 |  3509 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|         - |  3510 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|      3901 |  3511 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3901 |  3512 | `			if( pObj == 0 ){` |
|       ! 0 |  3513 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3514 | `				return SXERR_ABORT;` |
|         - |  3515 | `			}` |
|      3901 |  3516 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|         - |  3517 | `			/* Emit the load constant instruction */` |
|      3901 |  3518 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      3901 |  3519 | `			return SXRET_OK;` |
|   5508357 |  3520 | `	}else if( (pStr->nByte == sizeof("__FILE__") - 1 &&` |
|    508947 |  3521 | `		SyMemcmp(pStr->zString,"__FILE__",sizeof("__FILE__")-1) == 0) \|\|` |
|   5578986 |  3522 | `		(pStr->nByte == sizeof("__DIR__") - 1 &&` |
|    438752 |  3523 | `		SyMemcmp(pStr->zString,"__DIR__",sizeof("__DIR__")-1) == 0) ){` |
|         - |  3524 | `			/* __FILE__ / __DIR__ are magic constants resolved at COMPILE time to the` |
|         - |  3525 | `			 * file being compiled (where the token is written), NOT the runtime` |
|         - |  3526 | `			 * execution file. A function defined in a.php reporting __FILE__ must say` |
|         - |  3527 | `			 * a.php even when called from b.php — php semantics, and what Composer's` |
|         - |  3528 | ``			 * autoloader (loadClassLoader's `require __DIR__ . '/ClassLoader.php'`)`` |
|         - |  3529 | `			 * relies on. The runtime-constant path returned the caller's file. */` |
|      3995 |  3530 | `			int bDir = (pStr->zString[2] == 'D'); /* __DIR__ vs __FILE__ */` |
|      3995 |  3531 | `			SyString *pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|      3995 |  3532 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3995 |  3533 | `			if( pObj == 0 ){` |
|       ! 0 |  3534 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3535 | `				return SXERR_ABORT;` |
|         - |  3536 | `			}` |
|      3995 |  3537 | `			if( pFile && pFile->nByte > 0 ){` |
|       107 |  3538 | `				if( bDir ){` |
|         - |  3539 | `					const char *zDir;` |
|         - |  3540 | `					int nLen;` |
|         - |  3541 | `					SyString sDir;` |
|        56 |  3542 | `					zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|        56 |  3543 | `					SyStringInitFromBuf(&sDir,zDir,nLen);` |
|        56 |  3544 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&sDir);` |
|        30 |  3545 | `				}else{` |
|        55 |  3546 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,pFile);` |
|         - |  3547 | `				}` |
|        56 |  3548 | `			}else{` |
|         - |  3549 | `				SyString sMem;` |
|      3893 |  3550 | `				SyStringInitFromBuf(&sMem,":MEMORY:",sizeof(":MEMORY:")-1);` |
|      3893 |  3551 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sMem);` |
|         - |  3552 | `			}` |
|      3995 |  3553 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      3995 |  3554 | `			return SXRET_OK;` |
|   5466855 |  3555 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|    214532 |  3556 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|         - |  3557 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|         8 |  3558 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|         8 |  3559 | `			if( pObj == 0 ){` |
|       ! 0 |  3560 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3561 | `				return SXERR_ABORT;` |
|         - |  3562 | `			}` |
|         8 |  3563 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - |  3564 | `				SyString sNs;` |
|         8 |  3565 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         8 |  3566 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|         5 |  3567 | `			}else{` |
|       ! 0 |  3568 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|         - |  3569 | `			}` |
|         8 |  3570 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         8 |  3571 | `			return SXRET_OK;` |
|   5478736 |  3572 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|    394849 |  3573 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|   5516099 |  3574 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|    313056 |  3575 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
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
|  10733731 |  3618 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|         - |  3619 | `		ph7_value *pLitObj;` |
|         - |  3620 | `		/* Unknown literal,install it in the literal table */` |
|   2079001 |  3621 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   2079001 |  3622 | `		if( pLitObj == 0 ){` |
|       ! 0 |  3623 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  3624 | `			return SXERR_ABORT;` |
|         - |  3625 | `		}` |
|   2079001 |  3626 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|   2079001 |  3627 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|   1039498 |  3628 | `	}` |
|         - |  3629 | `	/* Emit the load constant instruction */` |
|  10733731 |  3630 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  10733731 |  3631 | `	return SXRET_OK;` |
|   6416227 |  3632 | `}` |
|         - |  3633 | `/*` |
|         - |  3634 | ` * Resolve a namespace path or simply load a literal.` |
|         - |  3635 | ` * If the token stream contains namespace separators (backslashes),` |
|         - |  3636 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|         - |  3637 | ` * Otherwise, load the simple literal directly.` |
|         - |  3638 | ` */` |
|  12836422 |  3639 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|         5 |  3640 | `{` |
|         - |  3641 | `	sxi32 rc;` |
|  12836427 |  3642 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  3643 | `		return SXRET_OK;` |
|         - |  3644 | `	}` |
|         - |  3645 | `	/* Check if this is a multi-token namespace path */` |
|  12836427 |  3646 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|         - |  3647 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      3983 |  3648 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      3983 |  3649 | `		int isAbsolute = 0;` |
|      3983 |  3650 | `		SyBlobReset(pWorker);` |
|         - |  3651 | `		/* Check for leading backslash (absolute path) */` |
|      3983 |  3652 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      3961 |  3653 | `			isAbsolute = 1;` |
|      3961 |  3654 | `			pGen->pIn++; /* Skip leading backslash */` |
|      1978 |  3655 | `		}` |
|         - |  3656 | `		/* Collect the raw path (no prefix yet) so its FIRST segment can be resolved` |
|         - |  3657 | ``		 * against use-imports below — php resolves `A\B\C` by mapping the leading`` |
|         - |  3658 | ``		 * `A` through the imports (`use X\A;` makes it `X\A\B\C`), and only prepends`` |
|         - |  3659 | ``		 * the current namespace when `A` matches no import. Blindly prefixing the`` |
|         - |  3660 | ``		 * namespace here produced e.g. `Ns\Ev\Bus` for `use ...\Event as Ev; Ev\Bus`. */`` |
|         - |  3661 | `		{` |
|         - |  3662 | `			SyBlob sRaw;` |
|      3983 |  3663 | `			SyBlobInit(&sRaw,&pGen->pVm->sAllocator);` |
|      4147 |  3664 | `			while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|      4147 |  3665 | `				if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        87 |  3666 | `					SyBlobAppend(&sRaw,"\\",1);` |
|        46 |  3667 | `				}else{` |
|      4065 |  3668 | `					SyBlobAppend(&sRaw,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|         - |  3669 | `				}` |
|      4147 |  3670 | `				if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      3983 |  3671 | `					pGen->pIn++;` |
|      3983 |  3672 | `					break;` |
|         - |  3673 | `				}` |
|       169 |  3674 | `				pGen->pIn++;` |
|         5 |  3675 | `			}` |
|      3983 |  3676 | `			if( isAbsolute ){` |
|      3961 |  3677 | `				SyBlobAppend(pWorker,SyBlobData(&sRaw),SyBlobLength(&sRaw)); /* FQN as written */` |
|      1983 |  3678 | `			}else{` |
|        24 |  3679 | `				const char *zRaw = (const char *)SyBlobData(&sRaw);` |
|        24 |  3680 | `				sxu32 nRaw = SyBlobLength(&sRaw);` |
|        24 |  3681 | `				sxu32 nFirst = 0;` |
|         - |  3682 | `				SyHashEntry *pNsImp;` |
|       108 |  3683 | `				while( nFirst < nRaw && zRaw[nFirst] != '\\' ){ nFirst++; }` |
|        24 |  3684 | `				pNsImp = SyHashGet(&pGen->hUseImports,(const void *)zRaw,nFirst);` |
|        24 |  3685 | `				if( pNsImp ){` |
|         - |  3686 | `					/* Leading segment is an imported alias: substitute its FQN. */` |
|        21 |  3687 | `					const char *zFQN = (const char *)pNsImp->pUserData;` |
|        21 |  3688 | `					SyBlobAppend(pWorker,zFQN,SyStrlen(zFQN));` |
|        21 |  3689 | `					SyBlobAppend(pWorker,zRaw + nFirst,nRaw - nFirst);` |
|        13 |  3690 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         3 |  3691 | `					SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         3 |  3692 | `					SyBlobAppend(pWorker,"\\",1);` |
|         3 |  3693 | `					SyBlobAppend(pWorker,zRaw,nRaw);` |
|         2 |  3694 | `				}else{` |
|       ! 0 |  3695 | `					SyBlobAppend(pWorker,zRaw,nRaw); /* global scope, no import */` |
|         - |  3696 | `				}` |
|         - |  3697 | `			}` |
|      3983 |  3698 | `			SyBlobRelease(&sRaw);` |
|         - |  3699 | `		}` |
|      3983 |  3700 | `		if( SyBlobLength(pWorker) > 0 ){` |
|         - |  3701 | `			ph7_value *pObj;` |
|         - |  3702 | `			SyString sPath;` |
|         - |  3703 | `			sxu32 nIdx;` |
|      3983 |  3704 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|         - |  3705 | `			/* Install in the literal table */` |
|      3983 |  3706 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      3931 |  3707 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3931 |  3708 | `				if( pObj == 0 ){` |
|       ! 0 |  3709 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  3710 | `					return SXERR_ABORT;` |
|         - |  3711 | `				}` |
|      3931 |  3712 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      3931 |  3713 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1963 |  3714 | `			}` |
|         - |  3715 | `			/* Emit the load constant instruction.` |
|         - |  3716 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|         - |  3717 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|      5972 |  3718 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|      1989 |  3719 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|      1989 |  3720 | `				nIdx,0,0);` |
|      3983 |  3721 | `			return SXRET_OK;` |
|         - |  3722 | `		}` |
|       ! 0 |  3723 | `	}` |
|         - |  3724 | `	/* Single-token literal: load directly */` |
|  12832449 |  3725 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  12832449 |  3726 | `	return rc;` |
|   6418216 |  3727 | `}` |
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
|  12836422 |  3744 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3745 | `{` |
|         - |  3746 | `	sxi32 rc;` |
|  12836427 |  3747 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  12836427 |  3748 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3749 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  3750 | `		return rc;` |
|         - |  3751 | `	}` |
|         - |  3752 | `	/* Node successfully compiled */` |
|  12836427 |  3753 | `	return SXRET_OK;` |
|   6418216 |  3754 | `}` |
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
|    342382 |  3771 | `static int GenStateIsReservedConstant(SyString *pName)` |
|         5 |  3772 | `{` |
|    342387 |  3773 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      3937 |  3774 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|         3 |  3775 | `			return TRUE;` |
|      3935 |  3776 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|         6 |  3777 | `			return TRUE;` |
|         5 |  3778 | `		}` |
|    340418 |  3779 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|      7801 |  3780 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|         3 |  3781 | `			return TRUE;` |
|         - |  3782 | `		}` |
|      3897 |  3783 | `	}` |
|         - |  3784 | `	/* Not a reserved constant */` |
|    342379 |  3785 | `	return FALSE;` |
|    171196 |  3786 | `}` |
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
|    155674 |  3928 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|         5 |  3929 | `{` |
|    155679 |  3930 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    155679 |  3931 | `	int nInlineTry = 0;` |
|    700245 |  3932 | `	while( pBlock && pBlock != pTarget ){` |
|    544571 |  3933 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
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
|    544571 |  3950 | `		pBlock = pBlock->pParent;` |
|         5 |  3951 | `	}` |
|    155679 |  3952 | `	return nInlineTry;` |
|         5 |  3953 | `}` |
|     89474 |  3954 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|         5 |  3955 | `{` |
|         - |  3956 | `	GenBlock *pLoop; /* Target loop */` |
|         - |  3957 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|         - |  3958 | `	sxu32 nLineLocal;` |
|         - |  3959 | `	sxi32 rc;` |
|     89479 |  3960 | `	nLineLocal = pGen->pIn->nLine;` |
|     89479 |  3961 | `	iLevel = 0;` |
|         - |  3962 | `	/* Jump the 'continue' keyword */` |
|     89479 |  3963 | `	pGen->pIn++;` |
|     89479 |  3964 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
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
|     89479 |  3990 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     89479 |  3991 | `	if( pLoop == 0 ){` |
|         - |  3992 | `		/* Illegal continue */` |
|        12 |  3993 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"'continue' not in the 'loop' or 'switch' context");` |
|        12 |  3994 | `		if( rc == SXERR_ABORT ){` |
|         - |  3995 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3996 | `			return SXERR_ABORT;` |
|         - |  3997 | `		}` |
|         7 |  3998 | `	}else{` |
|     89469 |  3999 | `		sxu32 nInstrIdx = 0;` |
|         - |  4000 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     89469 |  4001 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|         - |  4002 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|         - |  4003 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|     89469 |  4004 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|     89469 |  4005 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
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
|     89465 |  4021 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|     89465 |  4022 | `			if( pLoop->bPostContinue == TRUE ){` |
|         - |  4023 | `				JumpFixup sJumpFix;` |
|         - |  4024 | `				/* Post-continue */` |
|     27233 |  4025 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|     27233 |  4026 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|     27233 |  4027 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|     13614 |  4028 | `			}` |
|         - |  4029 | `		}` |
|         - |  4030 | `	}` |
|     89479 |  4031 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4032 | `		/* Not so fatal,emit a warning only */` |
|       ! 0 |  4033 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|       ! 0 |  4034 | `	}` |
|         - |  4035 | `	/* Statement successfully compiled */` |
|     89479 |  4036 | `	return SXRET_OK;` |
|     44742 |  4037 | `}` |
|         - |  4038 | `/*` |
|         - |  4039 | ` * Compile the 'break' statement.` |
|         - |  4040 | ` * According to the PHP language reference` |
|         - |  4041 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|         - |  4042 | ` *  structure.` |
|         - |  4043 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|         - |  4044 | ` *  enclosing structures are to be broken out of.` |
|         - |  4045 | ` */` |
|     66226 |  4046 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|         5 |  4047 | `{` |
|         - |  4048 | `	GenBlock *pLoop; /* Target loop */` |
|         - |  4049 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|         - |  4050 | `	sxi32 rc;` |
|     66231 |  4051 | `	iLevel = 0;` |
|         - |  4052 | `	/* Jump the 'break' keyword */` |
|     66231 |  4053 | `	pGen->pIn++;` |
|     66231 |  4054 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
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
|     66231 |  4080 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     66231 |  4081 | `	if( pLoop == 0 ){` |
|         - |  4082 | `		/* Illegal break */` |
|        18 |  4083 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"'break' not in the 'loop' or 'switch' context");` |
|        18 |  4084 | `		if( rc == SXERR_ABORT ){` |
|         - |  4085 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4086 | `			return SXERR_ABORT;` |
|         - |  4087 | `		}` |
|        10 |  4088 | `	}else{` |
|         - |  4089 | `		sxu32 nInstrIdx;` |
|         - |  4090 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     66215 |  4091 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|         - |  4092 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|     66215 |  4093 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|     66215 |  4094 | `		if( rc == SXRET_OK ){` |
|         - |  4095 | `			/* Fix the jump later when the jump destination is resolved */` |
|     66215 |  4096 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|     33105 |  4097 | `		}` |
|         - |  4098 | `	}` |
|     66231 |  4099 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4100 | `		/* Not so fatal,emit a warning only */` |
|       ! 0 |  4101 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|       ! 0 |  4102 | `	}` |
|         - |  4103 | `	/* Statement successfully compiled */` |
|     66231 |  4104 | `	return SXRET_OK;` |
|     33118 |  4105 | `}` |
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
|        27 |  4141 | `				break;` |
|         - |  4142 | `			}` |
|         - |  4143 | `			/* Point to the upper block */` |
|       121 |  4144 | `			pBlock = pBlock->pParent;` |
|         5 |  4145 | `		}` |
|       117 |  4146 | `		if( pBlock ){` |
|        27 |  4147 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        16 |  4148 | `		}else{` |
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
|         2 |  4240 | `{` |
|         - |  4241 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|         - |  4242 | `	sxu32 nRawObj;` |
|        10 |  4243 | `	sxu32 nObjIdx;` |
|         - |  4244 | `	/* Consume raw chunks verbatim without any processing until we get` |
|         - |  4245 | `	 * a PHP block.` |
|         - |  4246 | `	 */` |
|        10 |  4247 | `Consume:` |
|        22 |  4248 | `	nRawObj = nObjIdx = 0;` |
|        22 |  4249 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
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
|        22 |  4261 | `	if( nRawObj > 0 ){` |
|         - |  4262 | `		/* Emit the consume instruction */` |
|       ! 0 |  4263 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|       ! 0 |  4264 | `	}` |
|        22 |  4265 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
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
|        22 |  4296 | `		pGen->pIn = pGen->pEnd;` |
|        22 |  4297 | `		return SXERR_EOF;` |
|         - |  4298 | `	}` |
|       ! 0 |  4299 | `	return SXRET_OK;` |
|        12 |  4300 | `}` |
|         - |  4301 | `/*` |
|         - |  4302 | ` * Compile a PHP block.` |
|         - |  4303 | ` * A block is simply one or more PHP statements and expressions to compile` |
|         - |  4304 | ` * optionally delimited by braces {}.` |
|         - |  4305 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  4306 | ` * and this function takes care of generating the appropriate error` |
|         - |  4307 | ` * message.` |
|         - |  4308 | ` */` |
|   6442180 |  4309 | `static sxi32 PH7_CompileBlock(` |
|         - |  4310 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  4311 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|         - |  4312 | `	)` |
|         5 |  4313 | `{` |
|         - |  4314 | `	sxi32 rc;` |
|         - |  4315 | `	sxu32 nLine;` |
|   6442185 |  4316 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|   6417765 |  4317 | `		nLine = pGen->pIn->nLine;` |
|   6417765 |  4318 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|   6417765 |  4319 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  4320 | `			return SXERR_ABORT;` |
|         - |  4321 | `		}` |
|   6417765 |  4322 | `		pGen->pIn++;` |
|         - |  4323 | `		/* Compile until we hit the closing braces '}' */` |
|   9457543 |  4324 | `		for(;;){` |
|  18915091 |  4325 | `			if( pGen->pIn >= pGen->pEnd ){` |
|        22 |  4326 | `				rc = GenStateNextChunk(&(*pGen));` |
|        22 |  4327 | `				if (rc == SXERR_ABORT ){` |
|       ! 0 |  4328 | `			 	   return SXERR_ABORT;` |
|         - |  4329 | `				}` |
|        22 |  4330 | `				if( rc == SXERR_EOF ){` |
|         - |  4331 | `					/* No more token to process: the block was never closed. php reports` |
|         - |  4332 | `					 * the line the '{' was opened on, not where the input ran out. */` |
|        22 |  4333 | `					PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"Unclosed '{' on line %u",nLine);` |
|        22 |  4334 | `					break;` |
|         - |  4335 | `				}` |
|       ! 0 |  4336 | `			}` |
|  18915071 |  4337 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|         - |  4338 | `				/* Closing braces found,break immediately*/` |
|   6417745 |  4339 | `				pGen->pIn++;` |
|   6417745 |  4340 | `				break;` |
|         - |  4341 | `			}` |
|         - |  4342 | `			/* Compile a single statement */` |
|  12497331 |  4343 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  12497331 |  4344 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4345 | `				return SXERR_ABORT;` |
|         - |  4346 | `			}` |
|         5 |  4347 | `		}` |
|   6417765 |  4348 | `		GenStateLeaveBlock(&(*pGen),0);` |
|   3233305 |  4349 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
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
|     24425 |  4393 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     24425 |  4394 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4395 | `			return SXERR_ABORT;` |
|         - |  4396 | `		}` |
|         - |  4397 | `	}` |
|         - |  4398 | `	/* Jump trailing semi-colons ';' */` |
|   6442185 |  4399 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       ! 0 |  4400 | `		pGen->pIn++;` |
|       ! 0 |  4401 | `	}` |
|   6442185 |  4402 | `	return SXRET_OK;` |
|   3221095 |  4403 | `}` |
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
|     74020 |  4423 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|         5 |  4424 | `{` |
|     74025 |  4425 | `	GenBlock *pWhileBlock = 0;` |
|     74025 |  4426 | `	SyToken *pTmp,*pEnd = 0;` |
|         - |  4427 | `	sxu32 nFalseJump;` |
|         - |  4428 | `	sxu32 nLine;` |
|         - |  4429 | `	sxi32 rc;` |
|     74025 |  4430 | `	nLine = pGen->pIn->nLine;` |
|         - |  4431 | `	/* Jump the 'while' keyword */` |
|     74025 |  4432 | `	pGen->pIn++;` |
|     74025 |  4433 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4434 | `		/* Syntax error */` |
|       ! 0 |  4435 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|       ! 0 |  4436 | `		if( rc == SXERR_ABORT ){` |
|         - |  4437 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4438 | `			return SXERR_ABORT;` |
|         - |  4439 | `		}` |
|       ! 0 |  4440 | `		goto Synchronize;` |
|         - |  4441 | `	}` |
|         - |  4442 | `	/* Jump the left parenthesis '(' */` |
|     74025 |  4443 | `	pGen->pIn++;` |
|         - |  4444 | `	/* Create the loop block */` |
|     74025 |  4445 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|     74025 |  4446 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4447 | `		return SXERR_ABORT;` |
|         - |  4448 | `	}` |
|         - |  4449 | `	/* Delimit the condition */` |
|     74025 |  4450 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     74025 |  4451 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4452 | `		/* Empty expression */` |
|         3 |  4453 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|         3 |  4454 | `		if( rc == SXERR_ABORT ){` |
|         - |  4455 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4456 | `			return SXERR_ABORT;` |
|         - |  4457 | `		}` |
|         1 |  4458 | `	}` |
|         - |  4459 | `	/* Swap token streams */` |
|     74025 |  4460 | `	pTmp = pGen->pEnd;` |
|     74025 |  4461 | `	pGen->pEnd = pEnd;` |
|         - |  4462 | `	/* Compile the expression */` |
|     74025 |  4463 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     74025 |  4464 | `	if( rc == SXERR_ABORT ){` |
|         - |  4465 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4466 | `		return SXERR_ABORT;` |
|         - |  4467 | `	}` |
|         - |  4468 | `	/* Update token stream */` |
|     74025 |  4469 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 |  4470 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  4471 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4472 | `			return SXERR_ABORT;` |
|         - |  4473 | `		}` |
|       ! 0 |  4474 | `		pGen->pIn++;` |
|       ! 0 |  4475 | `	}` |
|         - |  4476 | `	/* Synchronize pointers */` |
|     74025 |  4477 | `	pGen->pIn  = &pEnd[1];` |
|     74025 |  4478 | `	pGen->pEnd = pTmp;` |
|         - |  4479 | `	/* Emit the false jump */` |
|     74025 |  4480 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|         - |  4481 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|     74025 |  4482 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|         - |  4483 | `	/* Compile the loop body */` |
|     74025 |  4484 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|     74025 |  4485 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4486 | `		return SXERR_ABORT;` |
|         - |  4487 | `	}` |
|         - |  4488 | `	/* Emit the unconditional jump to the start of the loop */` |
|     74025 |  4489 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|         - |  4490 | `	/* Fix all jumps now the destination is resolved */` |
|     74025 |  4491 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4492 | `	/* Release the loop block */` |
|     74025 |  4493 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4494 | `	/* Statement successfully compiled */` |
|     74025 |  4495 | `	return SXRET_OK;` |
|       ! 0 |  4496 | `Synchronize:` |
|         - |  4497 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4498 | `	 * compiling this erroneous block.` |
|         - |  4499 | `	 */` |
|       ! 0 |  4500 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4501 | `		pGen->pIn++;` |
|       ! 0 |  4502 | `	}` |
|       ! 0 |  4503 | `	return SXRET_OK;` |
|     37015 |  4504 | `}` |
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
|    124564 |  4652 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|         5 |  4653 | `{` |
|    124569 |  4654 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|    124569 |  4655 | `	GenBlock *pForBlock = 0;` |
|         - |  4656 | `	sxu32 nFalseJump;` |
|         - |  4657 | `	sxu32 nLine;` |
|         - |  4658 | `	sxi32 rc;` |
|    124569 |  4659 | `	nLine = pGen->pIn->nLine;` |
|         - |  4660 | `	/* Jump the 'for' keyword */` |
|    124569 |  4661 | `	pGen->pIn++;` |
|    124569 |  4662 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4663 | `		/* Syntax error */` |
|       ! 0 |  4664 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|       ! 0 |  4665 | `		if( rc == SXERR_ABORT ){` |
|         - |  4666 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4667 | `			return SXERR_ABORT;` |
|         - |  4668 | `		}` |
|       ! 0 |  4669 | `		return SXRET_OK;` |
|         - |  4670 | `	}` |
|         - |  4671 | `	/* Jump the left parenthesis '(' */` |
|    124569 |  4672 | `	pGen->pIn++;` |
|         - |  4673 | `	/* Delimit the init-expr;condition;post-expr */` |
|    124569 |  4674 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    124569 |  4675 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
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
|    124569 |  4690 | `	pTmp = pGen->pEnd;` |
|    124569 |  4691 | `	pGen->pEnd = pEnd;` |
|         - |  4692 | `	/* for() clauses are the ONLY place php's grammar allows a comma-separated` |
|         - |  4693 | `	 * expression list, so the comma operator is permitted for their duration` |
|         - |  4694 | `	 * (see GenStateTreeHasComma). A closure body nested inside a clause is` |
|         - |  4695 | `	 * compiled through this same window — recorded as a known leniency. */` |
|    124569 |  4696 | `	pGen->nCommaExprOk++;` |
|         - |  4697 | `	/* Compile initialization expressions if available */` |
|    124569 |  4698 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  4699 | `	/* Pop operand lvalues */` |
|    124569 |  4700 | `	if( rc == SXERR_ABORT ){` |
|         - |  4701 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4702 | `		return SXERR_ABORT;` |
|    124569 |  4703 | `	}else if( rc != SXERR_EMPTY ){` |
|    112903 |  4704 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     56449 |  4705 | `	}` |
|    124569 |  4706 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4707 | `		/* Syntax error */` |
|       ! 0 |  4708 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|       ! 0 |  4709 | `		if( rc == SXERR_ABORT ){` |
|         - |  4710 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4711 | `			return SXERR_ABORT;` |
|         - |  4712 | `		}` |
|       ! 0 |  4713 | `		return SXRET_OK;` |
|         - |  4714 | `	}` |
|         - |  4715 | `	/* Jump the trailing ';' */` |
|    124569 |  4716 | `	pGen->pIn++;` |
|         - |  4717 | `	/* Create the loop block */` |
|    124569 |  4718 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|    124569 |  4719 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4720 | `		return SXERR_ABORT;` |
|         - |  4721 | `	}` |
|         - |  4722 | `	/* Deffer continue jumps */` |
|    124569 |  4723 | `	pForBlock->bPostContinue = TRUE;` |
|         - |  4724 | `	/* Compile the condition */` |
|    124569 |  4725 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    124569 |  4726 | `	if( rc == SXERR_ABORT ){` |
|         - |  4727 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4728 | `		return SXERR_ABORT;` |
|    124569 |  4729 | `	}else if( rc != SXERR_EMPTY ){` |
|         - |  4730 | `		/* Emit the false jump */` |
|    112903 |  4731 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|         - |  4732 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    112903 |  4733 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|     56449 |  4734 | `	}` |
|    124569 |  4735 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4736 | `		/* Syntax error */` |
|         6 |  4737 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|         6 |  4738 | `		if( rc == SXERR_ABORT ){` |
|         - |  4739 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4740 | `			return SXERR_ABORT;` |
|         - |  4741 | `		}` |
|         6 |  4742 | `		return SXRET_OK;` |
|         - |  4743 | `	}` |
|         - |  4744 | `	/* Jump the trailing ';' */` |
|    124565 |  4745 | `	pGen->pIn++;` |
|         - |  4746 | `	/* Save the post condition stream */` |
|    124565 |  4747 | `	pPostStart = pGen->pIn;` |
|         - |  4748 | `	/* Compile the loop body — OUTSIDE the comma window (the body is ordinary` |
|         - |  4749 | ``	 * php, so `(1, 2)` inside it is the parse error it should be). */`` |
|    124565 |  4750 | `	pGen->nCommaExprOk--;` |
|    124565 |  4751 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|    124565 |  4752 | `	pGen->pEnd = pTmp;` |
|    124565 |  4753 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|    124565 |  4754 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4755 | `		return SXERR_ABORT;` |
|         - |  4756 | `	}` |
|         - |  4757 | `	/* Fix post-continue jumps */` |
|    124565 |  4758 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|         - |  4759 | `		JumpFixup *aPost;` |
|         - |  4760 | `		VmInstr *pInstr;` |
|         - |  4761 | `		sxu32 nJumpDest;` |
|         - |  4762 | `		sxu32 n;` |
|     11681 |  4763 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|     11681 |  4764 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     38909 |  4765 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|     27233 |  4766 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     27233 |  4767 | `			if( pInstr ){` |
|         - |  4768 | `				/* Fix jump */` |
|     27233 |  4769 | `				pInstr->iP2 = nJumpDest;` |
|     13614 |  4770 | `			}` |
|     13619 |  4771 | `		}` |
|      5838 |  4772 | `	}` |
|         - |  4773 | `	/* compile the post-expressions if available */` |
|    124565 |  4774 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|       ! 0 |  4775 | `		pPostStart++;` |
|       ! 0 |  4776 | `	}` |
|    124565 |  4777 | `	if( pPostStart < pEnd ){` |
|         - |  4778 | `		SyToken *pTmpIn,*pTmpEnd;` |
|    112901 |  4779 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|    112901 |  4780 | `		pGen->nCommaExprOk++; /* post-expressions are a clause list again */` |
|    112901 |  4781 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    112901 |  4782 | `		pGen->nCommaExprOk--;` |
|    112901 |  4783 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - |  4784 | `			/* Syntax error */` |
|       ! 0 |  4785 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|       ! 0 |  4786 | `			if( rc == SXERR_ABORT ){` |
|         - |  4787 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4788 | `				return SXERR_ABORT;` |
|         - |  4789 | `			}` |
|       ! 0 |  4790 | `			return SXRET_OK;` |
|         - |  4791 | `		}` |
|    112901 |  4792 | `		RE_SWAP_DELIMITER(pGen);` |
|    112901 |  4793 | `		if( rc == SXERR_ABORT ){` |
|         - |  4794 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4795 | `			return SXERR_ABORT;` |
|    112901 |  4796 | `		}else if( rc != SXERR_EMPTY){` |
|         - |  4797 | `			/* Pop operand lvalue */` |
|    112901 |  4798 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     56448 |  4799 | `		}` |
|     56448 |  4800 | `	}` |
|         - |  4801 | `	/* Emit the unconditional jump to the start of the loop */` |
|    124565 |  4802 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|         - |  4803 | `	/* Fix all jumps now the destination is resolved */` |
|    124565 |  4804 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4805 | `	/* Release the loop block */` |
|    124565 |  4806 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4807 | `	/* Statement successfully compiled */` |
|    124565 |  4808 | `	return SXRET_OK;` |
|     62287 |  4809 | `}` |
|         - |  4810 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|         - |  4811 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|         - |  4812 | ` * are allowed.` |
|         - |  4813 | ` */` |
|    459846 |  4814 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 |  4815 | `{` |
|    459851 |  4816 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    459851 |  4817 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  4818 | `		/* Unexpected expression */` |
|       ! 0 |  4819 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  4820 | `			"foreach: Expecting a variable name");` |
|       ! 0 |  4821 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 |  4822 | `			rc = SXERR_INVALID;` |
|       ! 0 |  4823 | `		}` |
|       ! 0 |  4824 | `	}` |
|    459851 |  4825 | `	return rc;` |
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
|    323484 |  4853 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|         5 |  4854 | `{` |
|    323489 |  4855 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    323489 |  4856 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    323489 |  4857 | `	GenBlock *pForeachBlock = 0;` |
|         - |  4858 | `	ph7_foreach_info *pInfo;` |
|         - |  4859 | `	sxu32 nFalseJump;` |
|         - |  4860 | `	VmInstr *pInstr;` |
|         - |  4861 | `	sxu32 nLine;` |
|         - |  4862 | `	sxi32 rc;` |
|    323489 |  4863 | `	nLine = pGen->pIn->nLine;` |
|         - |  4864 | `	/* Jump the 'foreach' keyword */` |
|    323489 |  4865 | `	pGen->pIn++;` |
|    323489 |  4866 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4867 | `		/* Syntax error */` |
|       ! 0 |  4868 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|       ! 0 |  4869 | `		if( rc == SXERR_ABORT ){` |
|         - |  4870 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4871 | `			return SXERR_ABORT;` |
|         - |  4872 | `		}` |
|       ! 0 |  4873 | `		goto Synchronize;` |
|         - |  4874 | `	}` |
|         - |  4875 | `	/* Jump the left parenthesis '(' */` |
|    323489 |  4876 | `	pGen->pIn++;` |
|         - |  4877 | `	/* Create the loop block */` |
|    323489 |  4878 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    323489 |  4879 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4880 | `		return SXERR_ABORT;` |
|         - |  4881 | `	}` |
|         - |  4882 | `	/* Delimit the expression */` |
|    323489 |  4883 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    323489 |  4884 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
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
|    323489 |  4899 | `	pCur = pGen->pIn;` |
|   1839671 |  4900 | `	while( pCur < pEnd ){` |
|   1839671 |  4901 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    354607 |  4902 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    354607 |  4903 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|         - |  4904 | `				/* Break with the first 'as' found */` |
|    323489 |  4905 | `				break;` |
|         - |  4906 | `			}` |
|     15559 |  4907 | `		}` |
|         - |  4908 | `		/* Advance the stream cursor */` |
|   1516187 |  4909 | `		pCur++;` |
|         5 |  4910 | `	}` |
|    323489 |  4911 | `	if( pCur <= pGen->pIn ){` |
|       ! 0 |  4912 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  4913 | `			"foreach: Missing array/object expression");` |
|       ! 0 |  4914 | `		if( rc == SXERR_ABORT ){` |
|         - |  4915 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4916 | `			return SXERR_ABORT;` |
|         - |  4917 | `		}` |
|       ! 0 |  4918 | `		goto Synchronize;` |
|         - |  4919 | `	}` |
|         - |  4920 | `	/* Swap token streams */` |
|    323489 |  4921 | `	pTmp = pGen->pEnd;` |
|    323489 |  4922 | `	pGen->pEnd = pCur;` |
|    323489 |  4923 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    323489 |  4924 | `	if( rc == SXERR_ABORT ){` |
|         - |  4925 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4926 | `		return SXERR_ABORT;` |
|         - |  4927 | `	}` |
|         - |  4928 | `	/* Update token stream */` |
|    323489 |  4929 | `	while(pGen->pIn < pCur ){` |
|       ! 0 |  4930 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  4931 | `		if( rc == SXERR_ABORT ){` |
|         - |  4932 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4933 | `			return SXERR_ABORT;` |
|         - |  4934 | `		}` |
|       ! 0 |  4935 | `		pGen->pIn++;` |
|       ! 0 |  4936 | `	}` |
|    323489 |  4937 | `	pCur++; /* Jump the 'as' keyword */` |
|    323489 |  4938 | `	pGen->pIn = pCur;` |
|    323489 |  4939 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 |  4940 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|       ! 0 |  4941 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4942 | `			return SXERR_ABORT;` |
|         - |  4943 | `		}` |
|       ! 0 |  4944 | `	}` |
|         - |  4945 | `	/* Create the foreach context */` |
|    323489 |  4946 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    323489 |  4947 | `	if( pInfo == 0 ){` |
|       ! 0 |  4948 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 |  4949 | `		return SXERR_ABORT;` |
|         - |  4950 | `	}` |
|         - |  4951 | `	/* Zero the structure */` |
|    323489 |  4952 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|         - |  4953 | `	/* Initialize structure fields */` |
|    323489 |  4954 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|         - |  4955 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|         - |  4956 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|         - |  4957 | `	 * '=>'. */` |
|    323489 |  4958 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    323489 |  4959 | `	if( pCur < pEnd ){` |
|         - |  4960 | `		/* Compile the expression holding the key name */` |
|    136389 |  4961 | `		if( pGen->pIn >= pCur ){` |
|       ! 0 |  4962 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|       ! 0 |  4963 | `			if( rc == SXERR_ABORT ){` |
|         - |  4964 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4965 | `				return SXERR_ABORT;` |
|         - |  4966 | `			}` |
|       ! 0 |  4967 | `		}else{` |
|    136389 |  4968 | `			pGen->pEnd = pCur;` |
|    136389 |  4969 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    136389 |  4970 | `			if( rc == SXERR_ABORT ){` |
|         - |  4971 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4972 | `				return SXERR_ABORT;` |
|         - |  4973 | `			}` |
|    136389 |  4974 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    136389 |  4975 | `			if( pInstr->p3 ){` |
|         - |  4976 | `				/* Record key name */` |
|    136389 |  4977 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|     68192 |  4978 | `			}` |
|    136389 |  4979 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|         - |  4980 | `		}` |
|    136389 |  4981 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|     68192 |  4982 | `	}` |
|    323489 |  4983 | `	pGen->pEnd = pEnd;` |
|    323489 |  4984 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 |  4985 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|       ! 0 |  4986 | `		if( rc == SXERR_ABORT ){` |
|         - |  4987 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4988 | `			return SXERR_ABORT;` |
|         - |  4989 | `		}` |
|       ! 0 |  4990 | `		goto Synchronize;` |
|         - |  4991 | `	}` |
|    323489 |  4992 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|        33 |  4993 | `		pGen->pIn++;` |
|         - |  4994 | `		/* Pass by reference  */` |
|        33 |  4995 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|        15 |  4996 | `	}` |
|         - |  4997 | `	/* Check if the value target is list() */` |
|    323489 |  4998 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
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
|    323484 |  5039 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
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
|    323467 |  5072 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    323467 |  5073 | `		if( rc == SXERR_ABORT ){` |
|         - |  5074 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  5075 | `			return SXERR_ABORT;` |
|         - |  5076 | `		}` |
|    323467 |  5077 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    323467 |  5078 | `		if( pInstr->p3 ){` |
|         - |  5079 | `			/* Record value name */` |
|    323467 |  5080 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    161731 |  5081 | `		}` |
|         - |  5082 | `	}` |
|         - |  5083 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    323487 |  5084 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|         - |  5085 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    323487 |  5086 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|         - |  5087 | `	/* Record the first instruction to execute */` |
|    323487 |  5088 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|         - |  5089 | `	/* Emit the FOREACH_STEP instruction */` |
|    323487 |  5090 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|         - |  5091 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    323487 |  5092 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|         - |  5093 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    323487 |  5094 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
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
|    323487 |  5122 | `	pGen->pIn = &pEnd[1];` |
|    323487 |  5123 | `	pGen->pEnd = pTmp;` |
|    323487 |  5124 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    323487 |  5125 | `	if( rc == SXERR_ABORT ){` |
|         - |  5126 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  5127 | `		return SXERR_ABORT;` |
|         - |  5128 | `	}` |
|         - |  5129 | `	/* Emit the unconditional jump to the start of the loop */` |
|    323487 |  5130 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|         - |  5131 | `	/* Fix all jumps now the destination is resolved */` |
|    323487 |  5132 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  5133 | `	/* Release the loop block */` |
|    323487 |  5134 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  5135 | `	/* Statement successfully compiled */` |
|    323487 |  5136 | `	return SXRET_OK;` |
|         1 |  5137 | `Synchronize:` |
|         - |  5138 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  5139 | `	 * compiling this erroneous block.` |
|         - |  5140 | `	 */` |
|         3 |  5141 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  5142 | `		pGen->pIn++;` |
|       ! 0 |  5143 | `	}` |
|         3 |  5144 | `	return SXRET_OK;` |
|    161747 |  5145 | `}` |
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
|   2323144 |  5178 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|         5 |  5179 | `{` |
|   2323149 |  5180 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|   2323149 |  5181 | `	GenBlock *pCondBlock = 0;` |
|         - |  5182 | `	sxu32 nJumpIdx;` |
|         - |  5183 | `	sxu32 nKeyID;` |
|         - |  5184 | `	sxi32 rc;` |
|         - |  5185 | `	/* Jump the 'if' keyword */` |
|   2323149 |  5186 | `	pGen->pIn++;` |
|   2323149 |  5187 | `	pToken = pGen->pIn;` |
|         - |  5188 | `	/* Create the conditional block */` |
|   2323149 |  5189 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|   2323149 |  5190 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  5191 | `		return SXERR_ABORT;` |
|         - |  5192 | `	}` |
|         - |  5193 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   1305489 |  5194 | `	for(;;){` |
|   2610983 |  5195 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
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
|   2610983 |  5208 | `		pToken++;` |
|         - |  5209 | `		/* Delimit the condition */` |
|   2610983 |  5210 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   2610983 |  5211 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
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
|   2610975 |  5224 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|         - |  5225 | `		/* Compile the condition */` |
|   2610975 |  5226 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  5227 | `		/* Update token stream */` |
|   2610975 |  5228 | `		while(pGen->pIn < pEnd ){` |
|       ! 0 |  5229 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 |  5230 | `			pGen->pIn++;` |
|       ! 0 |  5231 | `		}` |
|   2610975 |  5232 | `		pGen->pIn  = &pEnd[1];` |
|   2610975 |  5233 | `		pGen->pEnd = pTmp;` |
|   2610975 |  5234 | `		if( rc == SXERR_ABORT ){` |
|         - |  5235 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  5236 | `			return SXERR_ABORT;` |
|         - |  5237 | `		}` |
|         - |  5238 | `		/* Emit the false jump */` |
|   2610975 |  5239 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|         - |  5240 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   2610975 |  5241 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|         - |  5242 | `		/* Compile the body */` |
|   2610975 |  5243 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   2610975 |  5244 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5245 | `			return SXERR_ABORT;` |
|         - |  5246 | `		}` |
|   2610975 |  5247 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|    515786 |  5248 | `			break;` |
|         - |  5249 | `		}` |
|         - |  5250 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   1579413 |  5251 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1579413 |  5252 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   1069509 |  5253 | `			break;` |
|         - |  5254 | `		}` |
|         - |  5255 | `		/* Emit the unconditional jump */` |
|    509909 |  5256 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|         - |  5257 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    509909 |  5258 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|    509909 |  5259 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|    307611 |  5260 | `			pToken = &pGen->pIn[1];` |
|    307611 |  5261 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|     85574 |  5262 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    111040 |  5263 | `					break;` |
|         - |  5264 | `			}` |
|     85541 |  5265 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|     42768 |  5266 | `		}` |
|    287839 |  5267 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|         - |  5268 | `		/* Synchronize cursors */` |
|    287839 |  5269 | `		pToken = pGen->pIn;` |
|         - |  5270 | `		/* Fix the false jump */` |
|    287839 |  5271 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|         5 |  5272 | `	} /* For(;;) */` |
|         - |  5273 | `	/* Fix the false jump */` |
|   2323141 |  5274 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|   2323141 |  5275 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   1291574 |  5276 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|         - |  5277 | `			/* Compile the else block */` |
|    222075 |  5278 | `			pGen->pIn++;` |
|    222075 |  5279 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    222075 |  5280 | `			if( rc == SXERR_ABORT ){` |
|         - |  5281 |  |
|       ! 0 |  5282 | `				return SXERR_ABORT;` |
|         - |  5283 | `			}` |
|    111035 |  5284 | `	}` |
|   2323141 |  5285 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|         - |  5286 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|   2323141 |  5287 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|         - |  5288 | `	/* Release the conditional block */` |
|   2323141 |  5289 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  5290 | `	/* Statement successfully compiled */` |
|   2323141 |  5291 | `	return SXRET_OK;` |
|         4 |  5292 | `Synchronize:` |
|         - |  5293 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|         - |  5294 | `	 */` |
|        67 |  5295 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        59 |  5296 | `		pGen->pIn++;` |
|         3 |  5297 | `	}` |
|        11 |  5298 | `	return SXRET_OK;` |
|   1161577 |  5299 | `}` |
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
|   3248922 |  5393 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|         5 |  5394 | `{` |
|   3248927 |  5395 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|         - |  5396 | `	sxi32 rc;` |
|   3248927 |  5397 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   3248927 |  5398 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|         - |  5399 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|         - |  5400 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|         - |  5401 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|         - |  5402 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|         - |  5403 | `	 * normally below so token processing stays consistent. */` |
|   8559311 |  5404 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|   5310389 |  5405 | `		pFuncBlock = pFuncBlock->pParent;` |
|         5 |  5406 | `	}` |
|   3248922 |  5407 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|   3248893 |  5408 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|         3 |  5409 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  5410 | `			"A never-returning function must not return");` |
|         3 |  5411 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5412 | `			return SXERR_ABORT;` |
|         - |  5413 | `		}` |
|         1 |  5414 | `	}` |
|         - |  5415 | `	/* Jump the 'return' keyword */` |
|   3248927 |  5416 | `	pGen->pIn++;` |
|   3248927 |  5417 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  5418 | `		/* Compile the expression */` |
|   3151695 |  5419 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   3151695 |  5420 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5421 | `			return SXERR_ABORT;` |
|   3151695 |  5422 | `		}else if(rc != SXERR_EMPTY ){` |
|   3151695 |  5423 | `			nRet = 1;` |
|   1575845 |  5424 | `		}` |
|   1575845 |  5425 | `	}` |
|         - |  5426 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|         - |  5427 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|         - |  5428 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|         - |  5429 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|   3248927 |  5430 | `	if( pGen->bInGenerator ){` |
|      3921 |  5431 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|      3921 |  5432 | `		return SXRET_OK;` |
|         - |  5433 | `	}` |
|         - |  5434 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|         - |  5435 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|         - |  5436 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|         - |  5437 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|         - |  5438 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|   3245011 |  5439 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|   3245011 |  5440 | `	return SXRET_OK;` |
|   1624466 |  5441 | `}` |
|         - |  5442 | `/*` |
|         - |  5443 | ` * Compile a yield expression.` |
|         - |  5444 | ` * Called from the expression code generator when a yield node is encountered.` |
|         - |  5445 | ` * Handles: yield, yield $value, yield $key => $value` |
|         - |  5446 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|         - |  5447 | ` */` |
|     15938 |  5448 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|         5 |  5449 | `{` |
|         - |  5450 | `	SyToken *pTmp, *pSplit;` |
|     15943 |  5451 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     15943 |  5452 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|         - |  5453 | `	sxi32 rc;` |
|      7969 |  5454 | `	(void)iCompileFlag;` |
|         - |  5455 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     15943 |  5456 | `	pGen->pIn++;` |
|         - |  5457 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|         - |  5458 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|         - |  5459 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|         - |  5460 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|         - |  5461 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     15938 |  5462 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|      8004 |  5463 | `		&& pGen->pIn->sData.nByte == 4` |
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
|     15881 |  5481 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5482 | `		/* Bare yield — no value */` |
|         3 |  5483 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|         3 |  5484 | `		return SXRET_OK;` |
|         - |  5485 | `	}` |
|         - |  5486 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     15879 |  5487 | `	pSplit = 0;` |
|         - |  5488 | `	{` |
|     15879 |  5489 | `		SyToken *pCur = pGen->pIn;` |
|     15879 |  5490 | `		sxi32 nNest = 0;` |
|     47441 |  5491 | `		while( pCur < pGen->pEnd ){` |
|     47133 |  5492 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        18 |  5493 | `				nNest++;` |
|     47125 |  5494 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        18 |  5495 | `				nNest--;` |
|     47109 |  5496 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|     15571 |  5497 | `				pSplit = pCur;` |
|     15571 |  5498 | `				break;` |
|         - |  5499 | `			}` |
|     31567 |  5500 | `			pCur++;` |
|         5 |  5501 | `		}` |
|         - |  5502 | `	}` |
|     15879 |  5503 | `	pTmp = pGen->pEnd;` |
|     15879 |  5504 | `	if( pSplit ){` |
|         - |  5505 | `		/* yield $key => $value */` |
|     15571 |  5506 | `		pGen->pEnd = pSplit;` |
|     15571 |  5507 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     15571 |  5508 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     15571 |  5509 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|     15571 |  5510 | `		pGen->pEnd = pTmp;` |
|     15571 |  5511 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     15571 |  5512 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     15571 |  5513 | `		iP1 = 1;` |
|     15571 |  5514 | `		iP2 = 1;` |
|      7788 |  5515 | `	}else{` |
|         - |  5516 | `		/* yield $value */` |
|       313 |  5517 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       313 |  5518 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       313 |  5519 | `		if( rc != SXERR_EMPTY ){` |
|       313 |  5520 | `			iP1 = 1;` |
|       154 |  5521 | `		}` |
|         - |  5522 | `	}` |
|     15879 |  5523 | `	pGen->pEnd = pTmp;` |
|     15879 |  5524 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     15879 |  5525 | `	return SXRET_OK;` |
|      7974 |  5526 | `}` |
|         - |  5527 | `/*` |
|         - |  5528 | ` * Compile the die/exit language construct.` |
|         - |  5529 | ` * The role of these constructs is to terminate execution of the script.` |
|         - |  5530 | ` * Shutdown functions will always be executed even if exit() is called.` |
|         - |  5531 | ` */` |
|       128 |  5532 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|         5 |  5533 | `{` |
|       133 |  5534 | `	sxi32 nExpr = 0;` |
|         - |  5535 | `	sxi32 rc;` |
|         - |  5536 | `	/* Jump the die/exit keyword */` |
|       133 |  5537 | `	pGen->pIn++;` |
|       133 |  5538 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  5539 | `		/* Compile the expression */` |
|       133 |  5540 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       133 |  5541 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5542 | `			return SXERR_ABORT;` |
|       133 |  5543 | `		}else if(rc != SXERR_EMPTY ){` |
|       133 |  5544 | `			nExpr = 1;` |
|        64 |  5545 | `		}` |
|        64 |  5546 | `	}` |
|         - |  5547 | `	/* Emit the HALT instruction */` |
|       133 |  5548 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|       133 |  5549 | `	return SXRET_OK;` |
|        69 |  5550 | `}` |
|         - |  5551 | `/*` |
|         - |  5552 | ` * Compile the 'echo' language construct.` |
|         - |  5553 | ` */` |
|     18224 |  5554 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|         5 |  5555 | `{` |
|     18229 |  5556 | `	SyToken *pTmp,*pNext = 0;` |
|     18229 |  5557 | `	sxu32 nLine = pGen->pIn->nLine;` |
|     18229 |  5558 | `	int nExpr = 0;      /* expressions actually compiled */` |
|     18229 |  5559 | `	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */` |
|         - |  5560 | `	sxi32 rc;` |
|         - |  5561 | `	/* Jump the 'echo' keyword */` |
|     18229 |  5562 | `	pGen->pIn++;` |
|         - |  5563 | `	/* Compile arguments one after one */` |
|     18229 |  5564 | `	pTmp = pGen->pEnd;` |
|     46381 |  5565 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|     28159 |  5566 | `		if( pGen->pIn < pNext ){` |
|     28159 |  5567 | `			pGen->pEnd = pNext;` |
|     28159 |  5568 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|     28159 |  5569 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5570 | `				return SXERR_ABORT;` |
|     28159 |  5571 | `			}else if( rc != SXERR_EMPTY ){` |
|         - |  5572 | `				/* Emit the consume instruction */` |
|     28133 |  5573 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|     28133 |  5574 | `				nExpr++;` |
|     28133 |  5575 | `				bExpectMore = 0;` |
|     14064 |  5576 | `			}` |
|     14077 |  5577 | `		}` |
|         - |  5578 | `		/* Jump trailing commas (php: exactly one between expressions; a` |
|         - |  5579 | `		 * dangling or doubled comma is a parse error, enforced below) */` |
|     38095 |  5580 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|      9943 |  5581 | `			if( bExpectMore ){` |
|         - |  5582 | `				/* two commas in a row */` |
|         3 |  5583 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,` |
|         - |  5584 | `					"syntax error, unexpected token \",\"");` |
|         3 |  5585 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  5586 | `			}` |
|      9941 |  5587 | `			bExpectMore = 1;` |
|      9941 |  5588 | `			pNext++;` |
|         5 |  5589 | `		}` |
|     28157 |  5590 | `		pGen->pIn = pNext;` |
|         5 |  5591 | `	}` |
|         - |  5592 | `	/* Restore token stream */` |
|     18227 |  5593 | `	pGen->pEnd = pTmp;` |
|     18227 |  5594 | `	if( nExpr == 0 \|\| bExpectMore ){` |
|         - |  5595 | ``		/* `echo ;` or `echo expr, ;` — php rejects both */`` |
|        34 |  5596 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  5597 | `			"syntax error, unexpected token \";\"");` |
|        34 |  5598 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  5599 | `	}` |
|     18197 |  5600 | `	return SXRET_OK;` |
|      9117 |  5601 | `}` |
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
|     11676 |  5616 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
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
|     11676 |  5628 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|      5844 |  5629 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
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
|     11679 |  5640 | `	nLine = pGen->pIn->nLine;` |
|     11679 |  5641 | `	pGen->pIn++;` |
|         - |  5642 | `	/* Extract the enclosing function if any */` |
|     11679 |  5643 | `	pBlock = pGen->pCurrent;` |
|     23353 |  5644 | `	while( pBlock ){` |
|     23353 |  5645 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|     11679 |  5646 | `			break;` |
|         - |  5647 | `		}` |
|         - |  5648 | `		/* Point to the upper block */` |
|     11679 |  5649 | `		pBlock = pBlock->pParent;` |
|         5 |  5650 | `	}` |
|     11679 |  5651 | `	if( pBlock == 0 ){` |
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
|     11679 |  5670 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|         - |  5671 | `	/* Make sure we are dealing with a valid statement */` |
|     11679 |  5672 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|     11672 |  5673 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         3 |  5674 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|         3 |  5675 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5676 | `				return SXERR_ABORT;` |
|         - |  5677 | `			}` |
|         3 |  5678 | `			goto Synchronize;` |
|         - |  5679 | `	}` |
|     11677 |  5680 | `	pGen->pIn++;` |
|         - |  5681 | `	/* Extract variable name */` |
|     11677 |  5682 | `	pName = &pGen->pIn->sData;` |
|     11677 |  5683 | `	pGen->pIn++; /* Jump the var name */` |
|     11677 |  5684 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|       ! 0 |  5685 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  5686 | `		goto Synchronize;` |
|         - |  5687 | `	}` |
|         - |  5688 | `	/* Initialize the structure describing the static variable */` |
|     11677 |  5689 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     11677 |  5690 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|         - |  5691 | `	/* Duplicate variable name */` |
|     11677 |  5692 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     11677 |  5693 | `	if( zDup == 0 ){` |
|       ! 0 |  5694 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  5695 | `		return SXERR_ABORT;` |
|         - |  5696 | `	}` |
|     11677 |  5697 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|         - |  5698 | `	/* Check if we have an expression to compile */` |
|     11677 |  5699 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|         - |  5700 | `		SySet *pInstrContainer;` |
|         - |  5701 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|         - |  5702 | `		 * Static variable can take any complex expression including function` |
|         - |  5703 | `		 * call as their initialization value.` |
|         - |  5704 | `		 * Example:` |
|         - |  5705 | `		 *		static $var = foo(1,4+5,bar());` |
|         - |  5706 | `		 */` |
|     11677 |  5707 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|         - |  5708 | `		/* Swap bytecode container */` |
|     11677 |  5709 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     11677 |  5710 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|         - |  5711 | `		/* Compile the expression */` |
|     11677 |  5712 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  5713 | `		/* Emit the done instruction */` |
|     11677 |  5714 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|         - |  5715 | `		/* Restore default bytecode container */` |
|     11677 |  5716 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      5836 |  5717 | `	}` |
|         - |  5718 | `	/* Finally save the compiled static variable in the appropriate container */` |
|     11677 |  5719 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|     11677 |  5720 | `	return SXRET_OK;` |
|         1 |  5721 | `Synchronize:` |
|         - |  5722 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|         - |  5723 | `	 * statement.` |
|         - |  5724 | `	 */` |
|         5 |  5725 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|         3 |  5726 | `		pGen->pIn++;` |
|         1 |  5727 | `	}` |
|         3 |  5728 | `	return SXRET_OK;` |
|      5843 |  5729 | `}` |
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
|   5928498 |  5782 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
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
|   5928503 |  5793 | `	if( pFromImport ){` |
|   4820215 |  5794 | `		*pFromImport = 0;` |
|   2410105 |  5795 | `	}` |
|   5928503 |  5796 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|   5928503 |  5797 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|       ! 0 |  5798 | `		return nOrigIdx;` |
|         - |  5799 | `	}` |
|   5928503 |  5800 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|   5928503 |  5801 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|         - |  5802 | `	/* Skip if already qualified (contains backslash) */` |
|   5928503 |  5803 | `	hasNsSep = 0;` |
|  69590163 |  5804 | `	for( k = 0; k < nLit; k++ ){` |
|  63661691 |  5805 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
|  31830835 |  5806 | `	}` |
|   5928503 |  5807 | `	if( hasNsSep ){` |
|        28 |  5808 | `		return nOrigIdx;` |
|         - |  5809 | `	}` |
|         - |  5810 | `	/* Check use imports first (works even outside namespaces) */` |
|   5928477 |  5811 | `	SyBlobReset(&pGen->sWorker);` |
|   5928477 |  5812 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|   5928477 |  5813 | `	if( pImport ){` |
|        41 |  5814 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|        41 |  5815 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|        41 |  5816 | `		if( pFromImport ){` |
|        18 |  5817 | `			*pFromImport = 1;` |
|         8 |  5818 | `		}` |
|        23 |  5819 | `	}else{` |
|   5928441 |  5820 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|   5928303 |  5821 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|         - |  5822 | `		}` |
|         - |  5823 | `		/* Prepend current namespace */` |
|       143 |  5824 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       143 |  5825 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|       143 |  5826 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|         - |  5827 | `	}` |
|         - |  5828 | `	/* Look up or create a new literal for the qualified name */` |
|       179 |  5829 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|       179 |  5830 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|        79 |  5831 | `		return nNewIdx; /* Already interned */` |
|         - |  5832 | `	}` |
|       105 |  5833 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|       105 |  5834 | `	if( pNew == 0 ){` |
|       ! 0 |  5835 | `		return nOrigIdx; /* OOM, fall back to original */` |
|         - |  5836 | `	}` |
|       105 |  5837 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|       105 |  5838 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|       105 |  5839 | `	return nNewIdx;` |
|   2964254 |  5840 | `}` |
|         - |  5841 | `/*` |
|         - |  5842 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|         - |  5843 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|         - |  5844 | ` */` |
|    499526 |  5845 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|         5 |  5846 | `{` |
|         - |  5847 | `	SyHashEntry *pImport;` |
|    499531 |  5848 | `	const char *zName = pName->zString;` |
|    499531 |  5849 | `	sxu32 nName = pName->nByte;` |
|    499531 |  5850 | `	sxu32 nFirst = 0;` |
|         - |  5851 | `	/* php resolves a name through use-imports on its LEADING segment: an` |
|         - |  5852 | ``	 * unqualified `C` maps the whole name (`use X\C;` -> X\C), while a QUALIFIED`` |
|         - |  5853 | ``	 * `A\B\C` maps just `A` (`use X\A;` -> X\A\B\C) and keeps the `\B\C` tail.`` |
|         - |  5854 | `	 * The old code looked up the whole qualified string (which never matches a` |
|         - |  5855 | `	 * single-segment import alias) and then blindly prefixed the current` |
|         - |  5856 | ``	 * namespace, so `use PHPUnit\Framework; extends Framework\TestCase` resolved`` |
|         - |  5857 | `	 * to Ns\Framework\TestCase. Mirror GenStateResolveNamespaceLiteral here. */` |
|   6508127 |  5858 | `	while( nFirst < nName && zName[nFirst] != '\\' ){ nFirst++; }` |
|    499531 |  5859 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)zName,nFirst);` |
|    499531 |  5860 | `	if( pImport ){` |
|        25 |  5861 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|        25 |  5862 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|        25 |  5863 | `		SyBlobAppend(pOut,zName + nFirst,nName - nFirst); /* the \B\C tail, if any */` |
|        25 |  5864 | `		return;` |
|         - |  5865 | `	}` |
|         - |  5866 | `	/* Prepend current namespace if active */` |
|    499509 |  5867 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        14 |  5868 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        14 |  5869 | `		SyBlobAppend(pOut,"\\",1);` |
|         6 |  5870 | `	}` |
|    499509 |  5871 | `	SyBlobAppend(pOut,zName,nName);` |
|    249768 |  5872 | `}` |
|         - |  5873 | `/*` |
|         - |  5874 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|         - |  5875 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|         - |  5876 | ` * The caller must release pOut when done.` |
|         - |  5877 | ` */` |
|    464810 |  5878 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|         5 |  5879 | `{` |
|    464815 |  5880 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      3981 |  5881 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      3981 |  5882 | `		SyBlobAppend(pOut,"\\",1);` |
|      1988 |  5883 | `	}` |
|    464815 |  5884 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    464815 |  5885 | `}` |
|         - |  5886 | `/*` |
|         - |  5887 | ` * Compile a namespace statement` |
|         - |  5888 | ` * According to the PHP language reference manual` |
|         - |  5889 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|         - |  5890 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|         - |  5891 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|         - |  5892 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|         - |  5893 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|         - |  5894 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|         - |  5895 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|         - |  5896 | ` *  programming world.` |
|         - |  5897 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|         - |  5898 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|         - |  5899 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|         - |  5900 | ` *  classes/functions/constants.` |
|         - |  5901 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|         - |  5902 | ` *  readability of source code.` |
|         - |  5903 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|         - |  5904 | ` *  Here is an example of namespace syntax in PHP:` |
|         - |  5905 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|         - |  5906 | ` *       class MyClass {}` |
|         - |  5907 | ` *       function myfunction() {}` |
|         - |  5908 | ` *       const MYCONST = 1;` |
|         - |  5909 | ` *       $a = new MyClass;` |
|         - |  5910 | ` *       $c = new \my\name\MyClass;` |
|         - |  5911 | ` *       $a = strlen('hi');` |
|         - |  5912 | ` *       $d = namespace\MYCONST;` |
|         - |  5913 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|         - |  5914 | ` *       echo constant($d);` |
|         - |  5915 | ` * NOTE` |
|         - |  5916 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|         - |  5917 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|         - |  5918 | ` */` |
|         - |  5919 | `/*` |
|         - |  5920 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|         - |  5921 | ` */` |
|        14 |  5922 | `static const char * TokenTypeName(sxu32 nType)` |
|         4 |  5923 | `{` |
|        18 |  5924 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|        10 |  5925 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|        10 |  5926 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|        10 |  5927 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|        10 |  5928 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|        10 |  5929 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|         3 |  5930 | `	return "token";` |
|        11 |  5931 | `}` |
|      4020 |  5932 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|         5 |  5933 | `{` |
|         - |  5934 | `	sxu32 nLine;` |
|         - |  5935 | `	sxi32 rc;` |
|      4025 |  5936 | `	nLine = pGen->pIn->nLine;` |
|      4025 |  5937 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|         - |  5938 | `	/* Reset namespace and clear previous use imports */` |
|      4025 |  5939 | `	SyBlobReset(&pGen->sNamespace);` |
|      4025 |  5940 | `	SyHashRelease(&pGen->hUseImports);` |
|      4025 |  5941 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|      4025 |  5942 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|      4025 |  5943 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|      4025 |  5944 | `	SyHashRelease(&pGen->hUseConstImports);` |
|      4025 |  5945 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|      4025 |  5946 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5947 | `		/* Global namespace (bare "namespace;") */` |
|       ! 0 |  5948 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5949 | `		return SXRET_OK;` |
|         - |  5950 | `	}` |
|      4025 |  5951 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|         - |  5952 | `		/* namespace; — switch to global namespace */` |
|       ! 0 |  5953 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5954 | `		return SXRET_OK;` |
|         - |  5955 | `	}` |
|      4025 |  5956 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|         - |  5957 | `		/* namespace { } — global namespace block */` |
|         5 |  5958 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|         5 |  5959 | `		return SXRET_OK;` |
|         - |  5960 | `	}` |
|         - |  5961 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|      8119 |  5962 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      4103 |  5963 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|         - |  5964 | `			/* Append backslash separator */` |
|        47 |  5965 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        47 |  5966 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|        21 |  5967 | `			}` |
|        26 |  5968 | `		}else{` |
|         - |  5969 | `			/* Append identifier */` |
|      4061 |  5970 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|         - |  5971 | `		}` |
|      4103 |  5972 | `		pGen->pIn++;` |
|         5 |  5973 | `	}` |
|         - |  5974 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|         - |  5975 | `	 * at the correct program counter, not just the last one compiled. */` |
|         - |  5976 | `	{` |
|      4021 |  5977 | `		char *zNsDup = 0;` |
|      4021 |  5978 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      6026 |  5979 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      4014 |  5980 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      2007 |  5981 | `		}` |
|      4021 |  5982 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|         - |  5983 | `	}` |
|      4021 |  5984 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|         8 |  5985 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  5986 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|         4 |  5987 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         6 |  5988 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5989 | `			return SXERR_ABORT;` |
|         - |  5990 | `		}` |
|         2 |  5991 | `	}` |
|      4021 |  5992 | `	return SXRET_OK;` |
|      2015 |  5993 | `}` |
|         - |  5994 | `/*` |
|         - |  5995 | ` * Compile the 'use' statement` |
|         - |  5996 | ` * According to the PHP language reference manual` |
|         - |  5997 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|         - |  5998 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|         - |  5999 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|         - |  6000 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|         - |  6001 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|         - |  6002 | ` *  a function or constant is not supported.` |
|         - |  6003 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|         - |  6004 | ` * NOTE` |
|         - |  6005 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|         - |  6006 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|         - |  6007 | ` */` |
|        80 |  6008 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|         5 |  6009 | `{` |
|         - |  6010 | `	sxu32 nLine;` |
|         - |  6011 | `	sxi32 rc;` |
|         - |  6012 | `	SyBlob sPath;` |
|         - |  6013 | `	SyString sAlias;` |
|         - |  6014 | `	SyToken *pLast;` |
|         - |  6015 | `	char *zDup;` |
|         - |  6016 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|         - |  6017 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|         - |  6018 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|        85 |  6019 | `	nLine = pGen->pIn->nLine;` |
|        85 |  6020 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|         - |  6021 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|        85 |  6022 | `	iUseType = 0;` |
|        85 |  6023 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        30 |  6024 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|        30 |  6025 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|        16 |  6026 | `			iUseType = 1;` |
|        16 |  6027 | `			pGen->pIn++;` |
|        23 |  6028 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|        16 |  6029 | `			iUseType = 2;` |
|        16 |  6030 | `			pGen->pIn++;` |
|         7 |  6031 | `		}` |
|        14 |  6032 | `	}` |
|         - |  6033 | `	/* Select target hash tables based on import type */` |
|        85 |  6034 | `	switch( iUseType ){` |
|         7 |  6035 | `		case 1:` |
|        16 |  6036 | `			pGenHash = &pGen->hUseFuncImports;` |
|        16 |  6037 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|        16 |  6038 | `			break;` |
|         7 |  6039 | `		case 2:` |
|        16 |  6040 | `			pGenHash = &pGen->hUseConstImports;` |
|        16 |  6041 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|        16 |  6042 | `			break;` |
|        26 |  6043 | `		default:` |
|        57 |  6044 | `			pGenHash = &pGen->hUseImports;` |
|        57 |  6045 | `			pVmHash = &pGen->pVm->hUseImports;` |
|        52 |  6046 | `			break;` |
|         - |  6047 | `	}` |
|        85 |  6048 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|         - |  6049 | `	/* Process one or more use declarations separated by commas */` |
|        41 |  6050 | `	for(;;){` |
|        87 |  6051 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  6052 | `			break;` |
|         - |  6053 | `		}` |
|        87 |  6054 | `		SyBlobReset(&sPath);` |
|        87 |  6055 | `		pLast = 0;` |
|         - |  6056 | `		/* Collect the full namespace path */` |
|       301 |  6057 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|       219 |  6058 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       151 |  6059 | `				pLast = pGen->pIn;` |
|       151 |  6060 | `				if( SyBlobLength(&sPath) > 0 ){` |
|        73 |  6061 | `					SyBlobAppend(&sPath,"\\",1);` |
|        34 |  6062 | `				}` |
|       151 |  6063 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        73 |  6064 | `			}` |
|       219 |  6065 | `			pGen->pIn++;` |
|         5 |  6066 | `		}` |
|        87 |  6067 | `		if( pLast == 0 ){` |
|         - |  6068 | `			/* Empty path */` |
|         6 |  6069 | `			break;` |
|         - |  6070 | `		}` |
|         - |  6071 | `		/* Default alias is the last component of the path */` |
|        83 |  6072 | `		sAlias = pLast->sData;` |
|         - |  6073 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|        78 |  6074 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|        56 |  6075 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|        27 |  6076 | `			pGen->pIn++; /* Jump 'as' */` |
|        27 |  6077 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|        27 |  6078 | `				sAlias = pGen->pIn->sData;` |
|        27 |  6079 | `				pGen->pIn++;` |
|        12 |  6080 | `			}` |
|        12 |  6081 | `		}` |
|         - |  6082 | `		/* Check for duplicate import alias (per-type) */` |
|        83 |  6083 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|         8 |  6084 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  6085 | `				"Cannot use %.*s as %z because the name is already in use",` |
|         4 |  6086 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|         6 |  6087 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  6088 | `				SyBlobRelease(&sPath);` |
|       ! 0 |  6089 | `				return SXERR_ABORT;` |
|         - |  6090 | `			}` |
|         2 |  6091 | `		}` |
|         - |  6092 | `		/* Register the import: alias -> FQN.` |
|         - |  6093 | `		 * Strings are allocated from the VM pool allocator and freed` |
|         - |  6094 | `		 * when the entire VM is released. SyHashRelease does not free` |
|         - |  6095 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|       122 |  6096 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        78 |  6097 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        83 |  6098 | `		if( zDup ){` |
|        83 |  6099 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|        83 |  6100 | `			if( pVmHash ){` |
|         - |  6101 | `				/* Class imports: populate VM table directly (class resolution` |
|         - |  6102 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|        55 |  6103 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        55 |  6104 | `				if( zAliasDup ){` |
|        55 |  6105 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|        25 |  6106 | `				}` |
|        25 |  6107 | `			}` |
|        83 |  6108 | `			if( iUseType == 2 ){` |
|         - |  6109 | `				/* Const imports: emit a runtime instruction so imports are` |
|         - |  6110 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|        16 |  6111 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        16 |  6112 | `				if( zAliasDup ){` |
|         - |  6113 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|         - |  6114 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|         - |  6115 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|        16 |  6116 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|        16 |  6117 | `					if( azPair ){` |
|        16 |  6118 | `						azPair[0] = zAliasDup;` |
|        16 |  6119 | `						azPair[1] = zDup;` |
|        16 |  6120 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|         7 |  6121 | `					}` |
|         7 |  6122 | `				}` |
|         7 |  6123 | `			}` |
|        39 |  6124 | `		}` |
|         - |  6125 | `		/* Check for comma (multiple use declarations) */` |
|        83 |  6126 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|         3 |  6127 | `			pGen->pIn++;` |
|         2 |  6128 | `		}else{` |
|        43 |  6129 | `			break;` |
|         - |  6130 | `		}` |
|         1 |  6131 | `	}` |
|        85 |  6132 | `	SyBlobRelease(&sPath);` |
|        85 |  6133 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         4 |  6134 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|         2 |  6135 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         3 |  6136 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6137 | `			return SXERR_ABORT;` |
|         - |  6138 | `		}` |
|         1 |  6139 | `	}` |
|        85 |  6140 | `	return SXRET_OK;` |
|        45 |  6141 | `}` |
|         - |  6142 | `/*` |
|         - |  6143 | ` * Compile the stupid 'declare' language construct.` |
|         - |  6144 | ` *` |
|         - |  6145 | ` * According to the PHP language reference manual.` |
|         - |  6146 | ` *  The declare construct is used to set execution directives for a block of code.` |
|         - |  6147 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|         - |  6148 | ` *  declare (directive)` |
|         - |  6149 | ` *   statement` |
|         - |  6150 | ` * The directive section allows the behavior of the declare block to be set.` |
|         - |  6151 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|         - |  6152 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|         - |  6153 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|         - |  6154 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|         - |  6155 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|         - |  6156 | ` * <?php` |
|         - |  6157 | ` * // these are the same:` |
|         - |  6158 | ` * // you can use this:` |
|         - |  6159 | ` * declare(ticks=1) {` |
|         - |  6160 | ` *   // entire script here` |
|         - |  6161 | ` * }` |
|         - |  6162 | ` * // or you can use this:` |
|         - |  6163 | ` * declare(ticks=1);` |
|         - |  6164 | ` * // entire script here` |
|         - |  6165 | ` * ?>` |
|         - |  6166 | ` *` |
|         - |  6167 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|         - |  6168 | ` */` |
|         - |  6169 | `/*` |
|         - |  6170 | ` * Match a directive name against a known literal (case-insensitive).` |
|         - |  6171 | ` */` |
|        72 |  6172 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|         5 |  6173 | `{` |
|       109 |  6174 | `	return SyStringLength(pName) == nWant` |
|        72 |  6175 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|         5 |  6176 | `}` |
|         - |  6177 |  |
|        42 |  6178 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|         5 |  6179 | `{` |
|        47 |  6180 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        47 |  6181 | `	SyToken *pBodyEnd = 0;` |
|         - |  6182 | `	SyToken *pBodyStart;` |
|         - |  6183 | `	SyToken *pCursor;` |
|         - |  6184 | `	int bHasStrictTypes;` |
|         - |  6185 | `	int bBlockForm;` |
|         - |  6186 | `	int bPlacementOk;` |
|         - |  6187 | `	sxi32 rc;` |
|        47 |  6188 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|        47 |  6189 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|         6 |  6190 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         6 |  6191 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6192 | `			return SXERR_ABORT;` |
|         - |  6193 | `		}` |
|         6 |  6194 | `		goto Synchro;` |
|         - |  6195 | `	}` |
|        43 |  6196 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|        43 |  6197 | `	pBodyStart = pGen->pIn;` |
|         - |  6198 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|        43 |  6199 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|        43 |  6200 | `	if( pBodyEnd >= pGen->pEnd ){` |
|       ! 0 |  6201 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|       ! 0 |  6202 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6203 | `			return SXERR_ABORT;` |
|         - |  6204 | `		}` |
|       ! 0 |  6205 | `		return SXRET_OK;` |
|         - |  6206 | `	}` |
|         - |  6207 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|         - |  6208 | `	 * now delimits the comma-separated directive list. */` |
|        43 |  6209 | `	pGen->pIn = &pBodyEnd[1];` |
|        43 |  6210 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       ! 0 |  6211 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|       ! 0 |  6212 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  6213 | `			return SXERR_ABORT;` |
|         - |  6214 | `		}` |
|       ! 0 |  6215 | `	}` |
|        43 |  6216 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|        43 |  6217 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|        43 |  6218 | `	bHasStrictTypes = 0;` |
|         - |  6219 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|         - |  6220 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|         - |  6221 | `	 * directive appears anywhere in the list, before validating values. */` |
|        43 |  6222 | `	pCursor = pBodyStart;` |
|        55 |  6223 | `	while( pCursor < pBodyEnd ){` |
|        51 |  6224 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|        43 |  6225 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|        39 |  6226 | `				bHasStrictTypes = 1;` |
|        39 |  6227 | `				break;` |
|         - |  6228 | `			}` |
|         2 |  6229 | `		}` |
|        14 |  6230 | `		pCursor++;` |
|         2 |  6231 | `	}` |
|        43 |  6232 | `	if( bHasStrictTypes && bBlockForm ){` |
|         3 |  6233 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6234 | `			"strict_types declaration must not use block mode");` |
|         3 |  6235 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         3 |  6236 | `		return SXRET_OK;` |
|         - |  6237 | `	}` |
|        41 |  6238 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|         6 |  6239 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6240 | `			"strict_types declaration must be the very first statement in the script");` |
|         6 |  6241 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         6 |  6242 | `		return SXRET_OK;` |
|         - |  6243 | `	}` |
|         - |  6244 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|        37 |  6245 | `	pCursor = pBodyStart;` |
|        69 |  6246 | `	while( pCursor < pBodyEnd ){` |
|         - |  6247 | `		SyToken *pNameTok;` |
|         - |  6248 | `		SyToken *pEqTok;` |
|         - |  6249 | `		SyToken *pValTok;` |
|         - |  6250 | `		SyString *pDirName;` |
|         - |  6251 | `		int bIsStrict;` |
|         - |  6252 | `		int iStrictValue;` |
|        39 |  6253 | `		pNameTok = pCursor;` |
|        39 |  6254 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  6255 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6256 | `				"declare: Expecting a directive name");` |
|       ! 0 |  6257 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6258 | `			return SXRET_OK;` |
|         - |  6259 | `		}` |
|        39 |  6260 | `		pEqTok = pNameTok + 1;` |
|        39 |  6261 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|       ! 0 |  6262 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6263 | `				"declare: Expecting '=' after directive name");` |
|       ! 0 |  6264 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6265 | `			return SXRET_OK;` |
|         - |  6266 | `		}` |
|        39 |  6267 | `		pValTok = pEqTok + 1;` |
|        39 |  6268 | `		if( pValTok >= pBodyEnd ){` |
|       ! 0 |  6269 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6270 | `				"declare: Expecting value after '='");` |
|       ! 0 |  6271 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6272 | `			return SXRET_OK;` |
|         - |  6273 | `		}` |
|        39 |  6274 | `		pDirName = &pNameTok->sData;` |
|        39 |  6275 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|        39 |  6276 | `		if( bIsStrict ){` |
|         - |  6277 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|         - |  6278 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|        35 |  6279 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       ! 0 |  6280 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6281 | `					"declare(strict_types) value must be a literal");` |
|       ! 0 |  6282 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6283 | `				return SXRET_OK;` |
|         - |  6284 | `			}` |
|        35 |  6285 | `			iStrictValue = -1;` |
|        35 |  6286 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|        35 |  6287 | `				const char *zv = SyStringData(&pValTok->sData);` |
|        35 |  6288 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|        35 |  6289 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|        33 |  6290 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|        15 |  6291 | `			}` |
|        35 |  6292 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|         3 |  6293 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6294 | `					"strict_types declaration must have 0 or 1 as its value");` |
|         3 |  6295 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         3 |  6296 | `				return SXRET_OK;` |
|         - |  6297 | `			}` |
|        32 |  6298 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|        18 |  6299 | `		}else{` |
|         - |  6300 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|         - |  6301 | `			 * preserve the legacy notice so callers relying on the old` |
|         - |  6302 | `			 * behavior don't regress. */` |
|         8 |  6303 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|         - |  6304 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|         2 |  6305 | `				ph7_lib_version()` |
|         - |  6306 | `				);` |
|         - |  6307 | `		}` |
|        37 |  6308 | `		pCursor = pValTok + 1;` |
|         - |  6309 | `		/* Consume separating comma (or end). */` |
|        37 |  6310 | `		if( pCursor < pBodyEnd ){` |
|         3 |  6311 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  6312 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6313 | `					"declare: Expecting ',' or ')' after directive value");` |
|       ! 0 |  6314 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6315 | `				return SXRET_OK;` |
|         - |  6316 | `			}` |
|         3 |  6317 | `			pCursor++;` |
|         1 |  6318 | `		}` |
|         5 |  6319 | `	}` |
|         - |  6320 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|         - |  6321 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|         - |  6322 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|        35 |  6323 | `	return SXRET_OK;` |
|         2 |  6324 | `Synchro:` |
|         - |  6325 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|        16 |  6326 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|        12 |  6327 | `		pGen->pIn++;` |
|         2 |  6328 | `	}` |
|         6 |  6329 | `	return SXRET_OK;` |
|        26 |  6330 | `}` |
|         - |  6331 | `/*` |
|         - |  6332 | ` * Process default argument values. That is,a function may define C++-style default value` |
|         - |  6333 | ` * as follows:` |
|         - |  6334 | ` * function makecoffee($type = "cappuccino")` |
|         - |  6335 | ` * {` |
|         - |  6336 | ` *   return "Making a cup of $type.\n";` |
|         - |  6337 | ` * }` |
|         - |  6338 | ` * Symisc eXtension.` |
|         - |  6339 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|         - |  6340 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|         - |  6341 | ` *      Example: Work only with PH7,generate error under zend` |
|         - |  6342 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|         - |  6343 | ` *      {` |
|         - |  6344 | ` *       var_dump($a);` |
|         - |  6345 | ` *      }` |
|         - |  6346 | ` *     //call test without args` |
|         - |  6347 | ` *      test();` |
|         - |  6348 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|         - |  6349 | ` *      Example:` |
|         - |  6350 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|         - |  6351 | ` * 3 -) Function overloading!!` |
|         - |  6352 | ` *      Example:` |
|         - |  6353 | ` *      function foo($a) {` |
|         - |  6354 | ` *   	  return $a.PHP_EOL;` |
|         - |  6355 | ` *	    }` |
|         - |  6356 | ` *	    function foo($a, $b) {` |
|         - |  6357 | ` *   	  return $a + $b;` |
|         - |  6358 | ` *	    }` |
|         - |  6359 | ` *	    echo foo(5); // Prints "5"` |
|         - |  6360 | ` *	    echo foo(5, 2); // Prints "7"` |
|         - |  6361 | ` *      // Same arg` |
|         - |  6362 | ` *	   function foo(string $a)` |
|         - |  6363 | ` *	   {` |
|         - |  6364 | ` *	     echo "a is a string\n";` |
|         - |  6365 | ` *	     var_dump($a);` |
|         - |  6366 | ` *	   }` |
|         - |  6367 | ` *	  function foo(int $a)` |
|         - |  6368 | ` *	  {` |
|         - |  6369 | ` *	    echo "a is integer\n";` |
|         - |  6370 | ` *	    var_dump($a);` |
|         - |  6371 | ` *	  }` |
|         - |  6372 | ` *	  function foo(array $a)` |
|         - |  6373 | ` *	  {` |
|         - |  6374 | ` * 	    echo "a is an array\n";` |
|         - |  6375 | ` * 	    var_dump($a);` |
|         - |  6376 | ` *	  }` |
|         - |  6377 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|         - |  6378 | ` *	  foo(52); // a is integer [second foo]` |
|         - |  6379 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|         - |  6380 | ` * Please refer to the official documentation for more information on the powerful extension` |
|         - |  6381 | ` * introduced by the PH7 engine.` |
|         - |  6382 | ` */` |
|    595032 |  6383 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|         5 |  6384 | `{` |
|         - |  6385 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  6386 | `	SySet *pInstrContainer;` |
|         - |  6387 | `	sxi32 rc;` |
|         - |  6388 | `	/* Swap token stream */` |
|    595037 |  6389 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|    595037 |  6390 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    595037 |  6391 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|         - |  6392 | `	/* Compile the expression holding the argument value */` |
|    595037 |  6393 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  6394 | `	/* Emit the done instruction */` |
|    595037 |  6395 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|    595037 |  6396 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    595037 |  6397 | `	RE_SWAP_DELIMITER(pGen);` |
|    595037 |  6398 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  6399 | `		return SXERR_ABORT;` |
|         - |  6400 | `	}` |
|    595037 |  6401 | `	return SXRET_OK;` |
|    297521 |  6402 | `}` |
|         - |  6403 | `/*` |
|         - |  6404 | ` * Collect function arguments one after one.` |
|         - |  6405 | ` * According to the PHP language reference manual.` |
|         - |  6406 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|         - |  6407 | ` * list of expressions.` |
|         - |  6408 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|         - |  6409 | ` * and default argument values. Variable-length argument lists are also supported,` |
|         - |  6410 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|         - |  6411 | ` * for more information.` |
|         - |  6412 | ` * Example #1 Passing arrays to functions` |
|         - |  6413 | ` * <?php` |
|         - |  6414 | ` * function takes_array($input)` |
|         - |  6415 | ` * {` |
|         - |  6416 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|         - |  6417 | ` * }` |
|         - |  6418 | ` * ?>` |
|         - |  6419 | ` * Making arguments be passed by reference` |
|         - |  6420 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|         - |  6421 | ` * within the function is changed, it does not get changed outside of the function).` |
|         - |  6422 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|         - |  6423 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|         - |  6424 | ` * to the argument name in the function definition:` |
|         - |  6425 | ` * Example #2 Passing function parameters by reference` |
|         - |  6426 | ` * <?php` |
|         - |  6427 | ` * function add_some_extra(&$string)` |
|         - |  6428 | ` * {` |
|         - |  6429 | ` *   $string .= 'and something extra.';` |
|         - |  6430 | ` * }` |
|         - |  6431 | ` * $str = 'This is a string, ';` |
|         - |  6432 | ` * add_some_extra($str);` |
|         - |  6433 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|         - |  6434 | ` * ?>` |
|         - |  6435 | ` *` |
|         - |  6436 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|         - |  6437 | ` * complex agrument values.Please refer to the official documentation for more information` |
|         - |  6438 | ` * on these extension.` |
|         - |  6439 | ` */` |
|   1355072 |  6440 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|         5 |  6441 | `{` |
|         - |  6442 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|         - |  6443 | `	SyToken *pIn;  /* Token stream */` |
|         - |  6444 | `	SyBlob sSig;         /* Function signature */` |
|         - |  6445 | `	char *zDup;          /* Copy of argument name */` |
|         - |  6446 | `	sxi32 rc;` |
|         - |  6447 |  |
|   1355077 |  6448 | `	pIn = pGen->pIn;` |
|   1355077 |  6449 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|         - |  6450 | `	/* Process arguments one after one */` |
|   1754095 |  6451 | `	for(;;){` |
|   3508195 |  6452 | `		if( pIn >= pEnd ){` |
|         - |  6453 | `			/* No more arguments to process */` |
|   1355061 |  6454 | `			break;` |
|         - |  6455 | `		}` |
|   2153139 |  6456 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   2153139 |  6457 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   2153139 |  6458 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   2153139 |  6459 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|   2153139 |  6460 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|         - |  6461 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|         - |  6462 | `		 * first token inside the main token stream */` |
|   2153139 |  6463 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  6464 | `			return SXERR_ABORT;` |
|         - |  6465 | `		}` |
|         - |  6466 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|         - |  6467 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|         - |  6468 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|         - |  6469 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|         - |  6470 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|         - |  6471 | `		{` |
|   2153139 |  6472 | `			int bReadonly = 0, bVisSeen = 0;` |
|   2153139 |  6473 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|   2153139 |  6474 | `			sxi32 iSetVisFlag = 0;` |
|         - |  6475 | `			int nSetTok;` |
|         - |  6476 | `			sxi32 nSetVis;` |
|   2153139 |  6477 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|         3 |  6478 | `				bReadonly = 1;` |
|         3 |  6479 | `				pIn++;` |
|         1 |  6480 | `			}` |
|   2153139 |  6481 | `			nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|   2153139 |  6482 | `			if( nSetVis ){` |
|         - |  6483 | ``				/* Leading `private(set)` etc: promoted with a public read side */`` |
|         3 |  6484 | `				iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  6485 | `				bVisSeen = 1;` |
|         3 |  6486 | `				pIn += nSetTok;` |
|         3 |  6487 | `				if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       ! 0 |  6488 | `					bReadonly = 1;` |
|       ! 0 |  6489 | `					pIn++;` |
|         1 |  6490 | `				}` |
|   2153138 |  6491 | `			}else if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|    113195 |  6492 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|    113195 |  6493 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|        91 |  6494 | `					bVisSeen = 1;` |
|        91 |  6495 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|       121 |  6496 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|        39 |  6497 | `						: PH7_CLASS_PROT_PUBLIC;` |
|        91 |  6498 | `					pIn++;` |
|        91 |  6499 | `					nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|        91 |  6500 | `					if( nSetVis ){` |
|         - |  6501 | ``						/* `public private(set) T $x` promoted form */`` |
|         3 |  6502 | `						iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  6503 | `						pIn += nSetTok;` |
|         1 |  6504 | `					}` |
|        91 |  6505 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|        18 |  6506 | `						bReadonly = 1;` |
|        18 |  6507 | `						pIn++;` |
|         7 |  6508 | `					}` |
|        43 |  6509 | `				}` |
|     56595 |  6510 | `			}` |
|   2153139 |  6511 | `			if( iSetVisFlag == PH7_CLASS_ATTR_PRIVATE_SET ){` |
|         5 |  6512 | `				sArg.iFlags \|= VM_FUNC_ARG_PRIV_SET;` |
|   2153137 |  6513 | `			}else if( iSetVisFlag == PH7_CLASS_ATTR_PROTECTED_SET ){` |
|       ! 0 |  6514 | `				sArg.iFlags \|= VM_FUNC_ARG_PROT_SET;` |
|       ! 0 |  6515 | `			}` |
|   2153139 |  6516 | `			if( bVisSeen \|\| bReadonly ){` |
|        95 |  6517 | `				if( !bCtorCtx ){` |
|         6 |  6518 | `					if( bAbstractCtx ){` |
|         3 |  6519 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  6520 | `							"Cannot declare promoted property in an abstract constructor");` |
|         2 |  6521 | `					}else{` |
|         3 |  6522 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  6523 | `							"Cannot declare promoted property outside a constructor");` |
|         - |  6524 | `					}` |
|         6 |  6525 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  6526 | `						return SXERR_ABORT;` |
|         - |  6527 | `					}` |
|         6 |  6528 | `					return SXERR_SYNTAX;` |
|         - |  6529 | `				}` |
|        91 |  6530 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|        91 |  6531 | `				sArg.iPromoteVis = iVis;` |
|        91 |  6532 | `				if( bReadonly ){` |
|        20 |  6533 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|         8 |  6534 | `				}` |
|        43 |  6535 | `			}` |
|         - |  6536 | `		}` |
|         - |  6537 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|   2153130 |  6538 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   1172169 |  6539 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|    179532 |  6540 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|    144441 |  6541 | `			sxu32 nLineLocal = pIn->nLine;` |
|    144441 |  6542 | `			sxi32 iTFlags = 0;` |
|    144441 |  6543 | `			pGen->pIn = pIn;` |
|    144441 |  6544 | `			rc = GenStateParseUnionTypeDecl(` |
|     72218 |  6545 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|     72218 |  6546 | `				&iTFlags, &sArg.sTypeName,` |
|         - |  6547 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|         - |  6548 | `				/* bAllowVoid */ 0,` |
|     72218 |  6549 | `						nLineLocal);` |
|    144441 |  6550 | `			pIn = pGen->pIn;` |
|    144441 |  6551 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  6552 | `				return SXERR_ABORT;` |
|    144441 |  6553 | `			}else if( rc == SXERR_CORRUPT ){` |
|         - |  6554 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|         3 |  6555 | `				return SXERR_SYNTAX;` |
|    144439 |  6556 | `			}else if( rc == SXERR_SYNTAX ){` |
|        11 |  6557 | `				if( pIn < pEnd ){` |
|        15 |  6558 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|         - |  6559 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|         4 |  6560 | `						&pIn->sData);` |
|         7 |  6561 | `				}else{` |
|       ! 0 |  6562 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|         - |  6563 | `						"syntax error, unexpected end of file");` |
|         - |  6564 | `				}` |
|        11 |  6565 | `				return SXERR_SYNTAX;` |
|         - |  6566 | `			}` |
|    144431 |  6567 | `			sArg.iFlags \|= iTFlags;` |
|     72213 |  6568 | `		}` |
|   2153125 |  6569 | `		if( pIn >= pEnd ){` |
|       ! 0 |  6570 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|       ! 0 |  6571 | `			return rc;` |
|         - |  6572 | `		}` |
|   2153125 |  6573 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|         - |  6574 | `			/* Pass by reference,record that */` |
|     23375 |  6575 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|     23375 |  6576 | `			pIn++;` |
|     11685 |  6577 | `		}` |
|   2153125 |  6578 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|         - |  6579 | `			/* Variadic parameter: ...$args */` |
|     23439 |  6580 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|     23439 |  6581 | `			pIn++;` |
|     11717 |  6582 | `		}` |
|   2153125 |  6583 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  6584 | `			/* Invalid argument */` |
|       ! 0 |  6585 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|       ! 0 |  6586 | `			return rc;` |
|         - |  6587 | `		}` |
|   2153125 |  6588 | `		pIn++; /* Jump the dollar sign */` |
|         - |  6589 | `		/* Copy argument name */` |
|   2153125 |  6590 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   2153125 |  6591 | `		if( zDup == 0 ){` |
|       ! 0 |  6592 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  6593 | `			return SXERR_ABORT;` |
|         - |  6594 | `		}` |
|   2153125 |  6595 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   2153125 |  6596 | `		pIn++;` |
|   2153125 |  6597 | `		if( pIn < pEnd ){` |
|   1198653 |  6598 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|         - |  6599 | `				SyToken *pDefend;` |
|    595039 |  6600 | `				sxi32 iNest = 0;` |
|    595039 |  6601 | `				pIn++; /* Jump the equal sign */` |
|    595039 |  6602 | `				pDefend = pIn;` |
|         - |  6603 | `				/* Process the default value associated with this argument */` |
|   1256203 |  6604 | `				while( pDefend < pEnd ){` |
|    855623 |  6605 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|    194459 |  6606 | `						break;` |
|         - |  6607 | `					}` |
|    661169 |  6608 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|         - |  6609 | `						/* Increment nesting level */` |
|     27233 |  6610 | `						iNest++;` |
|    647555 |  6611 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|         - |  6612 | `						/* Decrement nesting level */` |
|     27233 |  6613 | `						iNest--;` |
|     13614 |  6614 | `					}` |
|    661169 |  6615 | `					pDefend++;` |
|         5 |  6616 | `				}` |
|    595039 |  6617 | `				if( pIn >= pDefend ){` |
|         3 |  6618 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|         3 |  6619 | `					return rc;` |
|         - |  6620 | `				}` |
|         - |  6621 | `				/* Process default value */` |
|    595037 |  6622 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|    595037 |  6623 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  6624 | `					return rc;` |
|         - |  6625 | `				}` |
|         - |  6626 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|         - |  6627 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|         - |  6628 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|         - |  6629 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|         - |  6630 | `				 * arg-type check lets null through. */` |
|    595032 |  6631 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|    328645 |  6632 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|    328642 |  6633 | `					&& &pIn[1] == pDefend` |
|     54475 |  6634 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|     36961 |  6635 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|     21397 |  6636 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|     15565 |  6637 | `					sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|         - |  6638 | `					/* php 8.4: the implicit form is deprecated at COMPILE time —` |
|         - |  6639 | `` 					 * `f(): Implicitly marking parameter $x as nullable …` `` |
|         - |  6640 | `					 * (methods carry the Class:: prefix when the class link is` |
|         - |  6641 | `					 * already up at this point). But NOT when the declared type` |
|         - |  6642 | ``					 * already accepts null: `mixed $x = null` is fine because mixed`` |
|         - |  6643 | `					 * includes null (explicit ?T / T\|null are already excluded via` |
|         - |  6644 | `					 * VM_FUNC_ARG_NULLABLE above). */` |
|     15566 |  6645 | `					if( !(sArg.sClass.nByte == sizeof("mixed")-1` |
|      7781 |  6646 | `						&& SyStrnicmp(SyStringData(&sArg.sClass),"mixed",sizeof("mixed")-1) == 0) ){` |
|     15563 |  6647 | `						const char *zSep = "";` |
|     15563 |  6648 | `						SyString sCls = { "", 0 };` |
|     15563 |  6649 | `						if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|     15557 |  6650 | `							sCls = ((ph7_class *)pFunc->pUserData)->sName;` |
|     15557 |  6651 | `							zSep = "::";` |
|      7776 |  6652 | `						}` |
|     23342 |  6653 | `						PH7_GenCompileError(&(*pGen),8192 /* E_DEPRECATED */,pIn->nLine,` |
|         - |  6654 | `							"%z%s%z(): Implicitly marking parameter $%z as nullable is deprecated, the explicit nullable type must be used instead",` |
|      7779 |  6655 | `							&sCls,zSep,&pFunc->sName,&sArg.sName);` |
|      7779 |  6656 | `					}` |
|      7780 |  6657 | `				}` |
|         - |  6658 | `				/* Point beyond the default value */` |
|    595037 |  6659 | `				pIn = pDefend;` |
|    297516 |  6660 | `			}` |
|   1198651 |  6661 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  6662 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|       ! 0 |  6663 | `				return rc;` |
|         - |  6664 | `			}` |
|   1198651 |  6665 | `			pIn++; /* Jump the trailing comma */` |
|    599323 |  6666 | `		}` |
|         - |  6667 | `		/* Append argument signature */` |
|   2153123 |  6668 | `		if( sArg.nType > 0 ){` |
|    144369 |  6669 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|         - |  6670 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|     31215 |  6671 | `				int marker = 'o';` |
|     31215 |  6672 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|     31215 |  6673 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|     15610 |  6674 | `			}else{` |
|         - |  6675 | `				int c;` |
|    113159 |  6676 | `				c = 'n'; /* cc warning */` |
|         - |  6677 | `				/* Type leading character */` |
|    113159 |  6678 | `				switch(sArg.nType){` |
|      5840 |  6679 | `				case MEMOBJ_HASHMAP:` |
|         - |  6680 | `					/* Hashmap aka 'array' */` |
|     11685 |  6681 | `					c = 'h';` |
|     11685 |  6682 | `					break;` |
|     17629 |  6683 | `				case MEMOBJ_INT:` |
|         - |  6684 | `					/* Integer */` |
|     35263 |  6685 | `					c = 'i';` |
|     35263 |  6686 | `					break;` |
|         2 |  6687 | `				case MEMOBJ_BOOL:` |
|         - |  6688 | `					/* Bool */` |
|         5 |  6689 | `					c = 'b';` |
|         5 |  6690 | `					break;` |
|         6 |  6691 | `				case MEMOBJ_REAL:` |
|         - |  6692 | `					/* Float */` |
|        14 |  6693 | `					c = 'f';` |
|        14 |  6694 | `					break;` |
|     33092 |  6695 | `				case MEMOBJ_STRING:` |
|         - |  6696 | `					/* String */` |
|     66189 |  6697 | `					c = 's';` |
|     66189 |  6698 | `					break;` |
|         7 |  6699 | `				case MEMOBJ_OBJ:` |
|         - |  6700 | `					/* Object */` |
|        16 |  6701 | `					c = 'o';` |
|        14 |  6702 | `					break;` |
|         1 |  6703 | `				default:` |
|         2 |  6704 | `					break;` |
|         - |  6705 | `				}` |
|    113159 |  6706 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|         - |  6707 | `			}` |
|     72187 |  6708 | `		}else{` |
|         - |  6709 | `			/* No type is associated with this parameter which mean` |
|         - |  6710 | `			 * that this function is not condidate for overloading.` |
|         - |  6711 | `			 */` |
|   2008759 |  6712 | `			SyBlobRelease(&sSig);` |
|         - |  6713 | `		}` |
|         - |  6714 | `		/* Save in the argument set */` |
|   2153123 |  6715 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|         5 |  6716 | `	}` |
|   1355061 |  6717 | `	if( SyBlobLength(&sSig) > 0 ){` |
|         - |  6718 | `		/* Save function signature */` |
|     97623 |  6719 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|     48809 |  6720 | `	}` |
|   1355061 |  6721 | `	return SXRET_OK;` |
|    677541 |  6722 | `}` |
|         - |  6723 | `/*` |
|         - |  6724 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|         - |  6725 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|         - |  6726 | ` * the enclosing function. Returns the token just past the nested construct.` |
|         - |  6727 | ` */` |
|     35050 |  6728 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|         5 |  6729 | `{` |
|     35055 |  6730 | `	sxi32 iParen = 0;` |
|     35055 |  6731 | `	pIn++; /* past 'function'/'fn' */` |
|         - |  6732 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|         - |  6733 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|         - |  6734 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|    155817 |  6735 | `	while( pIn < pEnd ){` |
|    155817 |  6736 | `		sxu32 t = pIn->nType;` |
|    155817 |  6737 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|    151859 |  6738 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|    105133 |  6739 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|     85635 |  6740 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|    120767 |  6741 | `		pIn++;` |
|         5 |  6742 | `	}` |
|     19503 |  6743 | `	if( pIn >= pEnd ){ return pIn; }` |
|         - |  6744 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|         - |  6745 | `	{` |
|     19503 |  6746 | `		sxi32 d = 0;` |
|    774375 |  6747 | `		while( pIn < pEnd ){` |
|    774375 |  6748 | `			sxu32 t = pIn->nType;` |
|    774375 |  6749 | `			if( t & PH7_TK_OCB ){ d++; }` |
|    743205 |  6750 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|    754877 |  6751 | `			pIn++;` |
|         5 |  6752 | `		}` |
|         - |  6753 | `	}` |
|     19503 |  6754 | `	return pIn;` |
|     17530 |  6755 | `}` |
|         - |  6756 | `/*` |
|         - |  6757 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|         - |  6758 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|         - |  6759 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|         - |  6760 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|         - |  6761 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|         - |  6762 | ` * detached-mini-program path untouched.` |
|         - |  6763 | ` */` |
|         - |  6764 | `/*` |
|         - |  6765 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|         - |  6766 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|         - |  6767 | ` * mixed, object.` |
|         - |  6768 | ` */` |
|     11688 |  6769 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|         5 |  6770 | `{` |
|         - |  6771 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|         - |  6772 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|         - |  6773 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|         - |  6774 | `	};` |
|         - |  6775 | `	sxu32 i;` |
|     11693 |  6776 | `	if( nName > 0 && zName[0] == '\\' ){` |
|       ! 0 |  6777 | `		zName++;` |
|       ! 0 |  6778 | `		nName--;` |
|       ! 0 |  6779 | `	}` |
|     11701 |  6780 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|     11701 |  6781 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|     11693 |  6782 | `			return 1;` |
|         - |  6783 | `		}` |
|         5 |  6784 | `	}` |
|       ! 0 |  6785 | `	return 0;` |
|      5849 |  6786 | `}` |
|         - |  6787 | `/*` |
|         - |  6788 | ` * One atom of a generator's declared return type: is it a supertype of` |
|         - |  6789 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|         - |  6790 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|         - |  6791 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|         - |  6792 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|         - |  6793 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|         - |  6794 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|         - |  6795 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|         - |  6796 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|         - |  6797 | ` * both rather than fatal on valid code (a recorded divergence).` |
|         - |  6798 | ` */` |
|     11690 |  6799 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|         5 |  6800 | `{` |
|     11695 |  6801 | `	if( nType == MEMOBJ_OBJ ){` |
|       ! 0 |  6802 | ``		return 1; /* bare `object` */`` |
|         - |  6803 | `	}` |
|     11695 |  6804 | `	if( nType != SXU32_HIGH ){` |
|         3 |  6805 | `		return 0; /* scalar/array/void/never/null/... */` |
|         - |  6806 | `	}` |
|     11693 |  6807 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|     11693 |  6808 | `		return 1;` |
|         - |  6809 | `	}` |
|         - |  6810 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|         - |  6811 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|         - |  6812 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|         - |  6813 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|         - |  6814 | `	{` |
|         - |  6815 | `		SyBlob sFQN;` |
|         - |  6816 | `		int bOk;` |
|       ! 0 |  6817 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       ! 0 |  6818 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|       ! 0 |  6819 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|       ! 0 |  6820 | `		SyBlobRelease(&sFQN);` |
|       ! 0 |  6821 | `		return bOk;` |
|         - |  6822 | `	}` |
|      5850 |  6823 | `}` |
|         - |  6824 | `/*` |
|         - |  6825 | ` * php 8: a generator function may only declare a return type that is a` |
|         - |  6826 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|         - |  6827 | ` * group qualifies only if every member does. Anything else is php's exact` |
|         - |  6828 | ` * compile-time fatal "Generator return type must be a supertype of` |
|         - |  6829 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|         - |  6830 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|         - |  6831 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|         - |  6832 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|         - |  6833 | ` */` |
|     11930 |  6834 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|         5 |  6835 | `{` |
|     11935 |  6836 | `	int bOk = 0;` |
|         - |  6837 | `	sxu32 nLine;` |
|         - |  6838 | `	sxi32 rc;` |
|     11935 |  6839 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|       245 |  6840 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|         - |  6841 | `	}` |
|     11695 |  6842 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|       ! 0 |  6843 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|       ! 0 |  6844 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|         - |  6845 | `		sxu32 i,j;` |
|       ! 0 |  6846 | `		for( i = 0; i < n && !bOk; i++ ){` |
|         - |  6847 | `			int bGroupOk;` |
|       ! 0 |  6848 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|       ! 0 |  6849 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|         - |  6850 | `			}` |
|       ! 0 |  6851 | `			bGroupOk = 1;` |
|       ! 0 |  6852 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|       ! 0 |  6853 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|       ! 0 |  6854 | `					bGroupOk = 0;` |
|       ! 0 |  6855 | `					break;` |
|         - |  6856 | `				}` |
|       ! 0 |  6857 | `			}` |
|       ! 0 |  6858 | `			bOk = bGroupOk;` |
|       ! 0 |  6859 | `		}` |
|       ! 0 |  6860 | `	}else{` |
|     11695 |  6861 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|         - |  6862 | `	}` |
|     11695 |  6863 | `	if( bOk ){` |
|     11693 |  6864 | `		return SXRET_OK;` |
|         - |  6865 | `	}` |
|         - |  6866 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|         - |  6867 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|         - |  6868 | `	 * token of this stream — its line is the function's closing brace. php` |
|         - |  6869 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|         - |  6870 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|         3 |  6871 | `	nLine = pGen->pIn[-1].nLine;` |
|         - |  6872 | `	{` |
|         3 |  6873 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|         3 |  6874 | `		if( sGiven.nByte < 1 ){` |
|       ! 0 |  6875 | `			sGiven = pFunc->sReturnClass;` |
|       ! 0 |  6876 | `		}` |
|         3 |  6877 | `		if( sGiven.nByte < 1 ){` |
|         - |  6878 | ``			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the`` |
|         - |  6879 | `			 * rendered type text, so sReturnTypeName arrives empty for them —` |
|         - |  6880 | `			 * name them here (the root fix belongs to that renderer, §3.7). */` |
|       ! 0 |  6881 | `			const char *zScalar =` |
|       ! 0 |  6882 | `				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :` |
|       ! 0 |  6883 | `				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";` |
|       ! 0 |  6884 | `			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));` |
|       ! 0 |  6885 | `		}` |
|         3 |  6886 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  6887 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|         - |  6888 | `	}` |
|         3 |  6889 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|      5970 |  6890 | `}` |
|   3009386 |  6891 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|         5 |  6892 | `{` |
|   3009391 |  6893 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|   3009391 |  6894 | `	SyToken *pEnd = pGen->pEnd;` |
|   3009391 |  6895 | `	sxi32 iDepth = 0;` |
|   3009391 |  6896 | `	int bStarted = 0;` |
| 141425959 |  6897 | `	while( pIn < pEnd ){` |
| 141425959 |  6898 | `		sxu32 t = pIn->nType;` |
| 141425959 |  6899 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 135048527 |  6900 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 128706469 |  6901 | `		if( t & PH7_TK_KEYWORD ){` |
|   9558765 |  6902 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|   9558765 |  6903 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|   9546835 |  6904 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|         - |  6905 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|   4755890 |  6906 | `		}` |
| 128659489 |  6907 | `		pIn++;` |
|         5 |  6908 | `	}` |
|   2997461 |  6909 | `	return FALSE;` |
|   1504698 |  6910 | `}` |
|         - |  6911 | `/*` |
|         - |  6912 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|         - |  6913 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  6914 | ` * and this routine takes care of generating the appropriate error message.` |
|         - |  6915 | ` */` |
|   3009386 |  6916 | `static sxi32 GenStateCompileFuncBody(` |
|         - |  6917 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |  6918 | `	ph7_vm_func *pFunc    /* Function state */` |
|         - |  6919 | `	)` |
|         5 |  6920 | `{` |
|         - |  6921 | `	SySet *pInstrContainer; /* Instruction container */` |
|         - |  6922 | `	GenBlock *pBlock;` |
|         - |  6923 | `	sxu32 nGotoOfft;` |
|         - |  6924 | `	sxi32 rc;` |
|         - |  6925 | `	/* Attach the new function */` |
|   3009391 |  6926 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   3009391 |  6927 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  6928 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|         - |  6929 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  6930 | `		return SXERR_ABORT;` |
|         - |  6931 | `	}` |
|   3009391 |  6932 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|         - |  6933 | `	/* Swap bytecode containers */` |
|   3009391 |  6934 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   3009391 |  6935 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|         - |  6936 | `	/* Emit constructor property promotion prologue:` |
|         - |  6937 | `	 *   $this->NAME = $NAME;` |
|         - |  6938 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|         - |  6939 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|         - |  6940 | `	{` |
|   3009391 |  6941 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|         - |  6942 | `		sxu32 i;` |
|   5107923 |  6943 | `		for( i = 0; i < nArg; i++ ){` |
|   2098537 |  6944 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|         - |  6945 | `			char *zSrc;` |
|         - |  6946 | `			sxu32 nSrc,nName;` |
|         - |  6947 | `			SySet sToken;` |
|         - |  6948 | `			SyToken *pTmpIn,*pTmpEnd;` |
|         - |  6949 | `			sxi32 rcPromote;` |
|   2098537 |  6950 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   2098461 |  6951 | `				continue;` |
|         - |  6952 | `			}` |
|         - |  6953 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|         - |  6954 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|         - |  6955 | `			 * copied), so it must outlive the function — never free it. The` |
|         - |  6956 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|         - |  6957 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|        81 |  6958 | `			nName = SyStringLength(&pArg->sName);` |
|        81 |  6959 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|        81 |  6960 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|        81 |  6961 | `			if( zSrc == 0 ){` |
|       ! 0 |  6962 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  6963 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  6964 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  6965 | `				return SXERR_ABORT;` |
|         - |  6966 | `			}` |
|         - |  6967 | `			{` |
|        81 |  6968 | `				char *z = zSrc;` |
|        81 |  6969 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|        81 |  6970 | `				z += sizeof("$this->")-1;` |
|        81 |  6971 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        81 |  6972 | `				z += nName;` |
|        81 |  6973 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|        81 |  6974 | `				z += sizeof(" = $")-1;` |
|        81 |  6975 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        81 |  6976 | `				z += nName;` |
|        81 |  6977 | `				*z = 0;` |
|         - |  6978 | `			}` |
|        81 |  6979 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        81 |  6980 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);` |
|        81 |  6981 | `			pTmpIn = pGen->pIn;` |
|        81 |  6982 | `			pTmpEnd = pGen->pEnd;` |
|        81 |  6983 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|        81 |  6984 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|        81 |  6985 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|        81 |  6986 | `			pGen->pIn = pTmpIn;` |
|        81 |  6987 | `			pGen->pEnd = pTmpEnd;` |
|        81 |  6988 | `			SySetRelease(&sToken);` |
|        81 |  6989 | `			if( rcPromote == SXERR_ABORT ){` |
|       ! 0 |  6990 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  6991 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  6992 | `				return SXERR_ABORT;` |
|         - |  6993 | `			}` |
|         - |  6994 | `			/* Discard the assignment result — this is a statement expression. */` |
|        81 |  6995 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        43 |  6996 | `		}` |
|         - |  6997 | `	}` |
|         - |  6998 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|         - |  6999 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|         - |  7000 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|         - |  7001 | `	 * generator — and vice versa — is classified independently. */` |
|         - |  7002 | `	{` |
|   3009391 |  7003 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|   3009391 |  7004 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|         - |  7005 | `		/* Compile the body */` |
|   3009391 |  7006 | `		PH7_CompileBlock(&(*pGen),0);` |
|   3009391 |  7007 | `		pGen->bInGenerator = bSavedGen;` |
|         - |  7008 | `	}` |
|         - |  7009 | `	/* Fix exception jumps now the destination is resolved */` |
|   3009391 |  7010 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - |  7011 | `	/* Emit the final return if not yet done */` |
|   3009391 |  7012 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - |  7013 | `	/* Fix gotos jumps now the destination is resolved */` |
|   3009391 |  7014 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|       ! 0 |  7015 | `		rc = SXERR_ABORT;` |
|       ! 0 |  7016 | `	}` |
|   3009391 |  7017 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|         - |  7018 | `	/* Restore the default container */` |
|   3009391 |  7019 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - |  7020 | `	/* Leave function block */` |
|   3009391 |  7021 | `	GenStateLeaveBlock(&(*pGen),0);` |
|   3009391 |  7022 | `	if( rc == SXERR_ABORT ){` |
|         - |  7023 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  7024 | `		return SXERR_ABORT;` |
|         - |  7025 | `	}` |
|         - |  7026 | `	/* Scan for yield opcodes to detect generator functions */` |
|         - |  7027 | `	{` |
|   3009391 |  7028 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|         - |  7029 | `		sxu32 i;` |
|  86618901 |  7030 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
|  83621445 |  7031 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     11935 |  7032 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     11935 |  7033 | `				break;` |
|         - |  7034 | `			}` |
|  41804760 |  7035 | `		}` |
|         - |  7036 | `	}` |
|   3009391 |  7037 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|         - |  7038 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|     11935 |  7039 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|       ! 0 |  7040 | `			return SXERR_ABORT;` |
|         - |  7041 | `		}` |
|      5965 |  7042 | `	}` |
|         - |  7043 | `	/* All done, function body compiled */` |
|   3009391 |  7044 | `	return SXRET_OK;` |
|   1504698 |  7045 | `}` |
|         - |  7046 | `/*` |
|         - |  7047 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|         - |  7048 | ` * According to the PHP language reference manual.` |
|         - |  7049 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|         - |  7050 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|         - |  7051 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|         - |  7052 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - |  7053 | ` *  Functions need not be defined before they are referenced.` |
|         - |  7054 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|         - |  7055 | ` *  a function even if they were defined inside and vice versa.` |
|         - |  7056 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|         - |  7057 | ` *  calls with over 32-64 recursion levels.` |
|         - |  7058 | ` *` |
|         - |  7059 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|         - |  7060 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|         - |  7061 | ` * on these extension.` |
|         - |  7062 | ` */` |
|         - |  7063 | `/*` |
|         - |  7064 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|         - |  7065 | ` */` |
|       594 |  7066 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|         5 |  7067 | `{` |
|         - |  7068 | `	sxu32 i;` |
|      1659 |  7069 | `	for( i = 0; i < n; i++ ){` |
|      1423 |  7070 | `		int a = zA[i], b = zB[i];` |
|      1423 |  7071 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|      1423 |  7072 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|      1423 |  7073 | `		if( a != b ) return a - b;` |
|       535 |  7074 | `	}` |
|       241 |  7075 | `	return 0;` |
|       302 |  7076 | `}` |
|         - |  7077 | `/*` |
|         - |  7078 | ` * Internal type-atom kinds used during union type parsing.` |
|         - |  7079 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|         - |  7080 | ` * (which are positive bit values stored in sxu32).` |
|         - |  7081 | ` */` |
|         - |  7082 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|         - |  7083 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|         - |  7084 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|         - |  7085 |  |
|         - |  7086 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|         - |  7087 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|         - |  7088 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|         - |  7089 |  |
|         - |  7090 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|         - |  7091 | `struct PhlTypeAtom {` |
|         - |  7092 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|         - |  7093 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|         - |  7094 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|         - |  7095 | `	sxu32 nCanon;` |
|         - |  7096 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|         - |  7097 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|         - |  7098 | `};` |
|         - |  7099 |  |
|         - |  7100 | `/*` |
|         - |  7101 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|         - |  7102 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|         - |  7103 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|         - |  7104 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|         - |  7105 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|         - |  7106 | ` * already be consumed by the caller.` |
|         - |  7107 | ` */` |
|         - |  7108 | `/*` |
|         - |  7109 | ` * TRUE if pName is a reserved PHP type keyword (never a class name), so a type` |
|         - |  7110 | ` * hint that spells it must not be namespace-qualified. Only the words that can` |
|         - |  7111 | ` * reach GenStateParseOneTypeAtom's identifier branch as a bare name matter here` |
|         - |  7112 | ` * (bool/int/float/string/array/object/self/static/parent arrive as keywords, and` |
|         - |  7113 | ` * null/void/never are matched before the class path), but the full set is listed` |
|         - |  7114 | ` * so the guard is robust to lexer changes.` |
|         - |  7115 | ` */` |
|     43112 |  7116 | `static int GenStateIsReservedTypeWord(const SyString *pName)` |
|         5 |  7117 | `{` |
|         - |  7118 | `	static const char *azWords[] = {` |
|         - |  7119 | `		"false","true","mixed","iterable","callable","null","void","never",` |
|         - |  7120 | `		"bool","boolean","int","integer","float","double","string","array",` |
|         - |  7121 | `		"object","self","static","parent"` |
|         - |  7122 | `	};` |
|         - |  7123 | `	sxu32 i;` |
|    903415 |  7124 | `	for( i = 0 ; i < SX_ARRAYSIZE(azWords) ; i++ ){` |
|    860413 |  7125 | `		sxu32 n = (sxu32)SyStrlen(azWords[i]);` |
|    860413 |  7126 | `		if( pName->nByte == n && SyStrnicmp(pName->zString,azWords[i],n) == 0 ){` |
|       115 |  7127 | `			return 1;` |
|         - |  7128 | `		}` |
|    430154 |  7129 | `	}` |
|     43007 |  7130 | `	return 0;` |
|     21561 |  7131 | `}` |
|    192478 |  7132 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|         5 |  7133 | `{` |
|    192483 |  7134 | `	SyToken *pIn = pGen->pIn;` |
|    192483 |  7135 | `	int bAbsolute = 0;` |
|    192483 |  7136 | `	SyZero(pOut, sizeof(*pOut));` |
|    192483 |  7137 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|    192483 |  7138 | `	if( pIn >= pGen->pEnd ){` |
|       ! 0 |  7139 | `		return SXERR_SYNTAX;` |
|         - |  7140 | `	}` |
|         - |  7141 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|    192483 |  7142 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|        10 |  7143 | `		bAbsolute = 1; /* fully-qualified: never prefix the current namespace */` |
|        10 |  7144 | `		pIn++;` |
|        10 |  7145 | `		if( pIn >= pGen->pEnd ){` |
|       ! 0 |  7146 | `			return SXERR_SYNTAX;` |
|         - |  7147 | `		}` |
|         4 |  7148 | `	}` |
|    192483 |  7149 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  7150 | `		return SXERR_SYNTAX;` |
|         - |  7151 | `	}` |
|    192483 |  7152 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|    149133 |  7153 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|    149133 |  7154 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|     15637 |  7155 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|    141317 |  7156 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|      7863 |  7157 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|    129572 |  7158 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|     47357 |  7159 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|    101967 |  7160 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|     78193 |  7161 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|     39197 |  7162 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|        45 |  7163 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|        82 |  7164 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|        27 |  7165 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|        49 |  7166 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|        18 |  7167 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|        35 |  7168 | `			pOut->nType = SXU32_HIGH;` |
|        35 |  7169 | `			pOut->sClass = pIn->sData;` |
|        19 |  7170 | `		}else{` |
|         3 |  7171 | `			return SXERR_SYNTAX;` |
|         - |  7172 | `		}` |
|    149131 |  7173 | `		pIn++;` |
|     74568 |  7174 | `	}else{` |
|         - |  7175 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|         - |  7176 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|     43355 |  7177 | `		SyString *pT = &pIn->sData;` |
|     43355 |  7178 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|        34 |  7179 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|        34 |  7180 | `			pIn++;` |
|     43340 |  7181 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|       183 |  7182 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|       183 |  7183 | `			pIn++;` |
|     43236 |  7184 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|        27 |  7185 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|        27 |  7186 | `			pIn++;` |
|        16 |  7187 | `		}else{` |
|         - |  7188 | `			/* Class / interface name; consume namespace path a\b\c */` |
|     43125 |  7189 | `			SyToken *pFirst = pIn;` |
|     43125 |  7190 | `			SyToken *pLast = pIn;` |
|     43125 |  7191 | `			pOut->nType = SXU32_HIGH;` |
|     43125 |  7192 | `			pOut->sClass = pIn->sData;` |
|     43125 |  7193 | `			pIn++;` |
|     64683 |  7194 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|     43128 |  7195 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|         3 |  7196 | `				pLast = &pIn[1];` |
|         3 |  7197 | `				pIn += 2;` |
|         1 |  7198 | `			}` |
|     43125 |  7199 | `			if( pLast != pFirst ){` |
|         3 |  7200 | `				const char *zFirst = pFirst->sData.zString;` |
|         3 |  7201 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|         3 |  7202 | `				pOut->sClass.zString = zFirst;` |
|         3 |  7203 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|         1 |  7204 | `			}` |
|         - |  7205 | `			/* Namespace-qualify a bare (single-segment, non-absolute) class type so` |
|         - |  7206 | ``			 * a `: Base` / `Base $x` hint in namespace N resolves to N\Base (or a`` |
|         - |  7207 | ``			 * `use` alias) at type-check time instead of the global \Base — mirrors`` |
|         - |  7208 | `			 * the NEW/CALL/instanceof qualification. Absolute (\Base) and already-` |
|         - |  7209 | `			 * qualified (A\B) names are left as written, matching GenStateNsQualifyName. */` |
|         - |  7210 | `			/* Reserved type words that reach this identifier branch (false, true,` |
|         - |  7211 | `			 * mixed, iterable, callable) are NOT classes and must not be qualified` |
|         - |  7212 | ``			 * (else `false\|string` becomes `Ns\false\|string`). */`` |
|     43125 |  7213 | `			if( !bAbsolute && pLast == pFirst && !GenStateIsReservedTypeWord(&pOut->sClass) ){` |
|         - |  7214 | `				SyBlob sFqn;` |
|     43007 |  7215 | `				SyBlobInit(&sFqn,&pGen->pVm->sAllocator);` |
|     43007 |  7216 | `				GenStateResolveName(pGen,&pOut->sClass,&sFqn);` |
|     43002 |  7217 | `				if( SyBlobLength(&sFqn) != pOut->sClass.nByte` |
|     43002 |  7218 | `				 \|\| SyMemcmp(SyBlobData(&sFqn),(const void *)pOut->sClass.zString,pOut->sClass.nByte) != 0 ){` |
|        17 |  7219 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 |  7220 | `						(const char *)SyBlobData(&sFqn),SyBlobLength(&sFqn));` |
|        12 |  7221 | `					if( zDup ){` |
|        12 |  7222 | `						SyStringInitFromBuf(&pOut->sClass,zDup,SyBlobLength(&sFqn));` |
|         5 |  7223 | `					}` |
|         5 |  7224 | `				}` |
|     43007 |  7225 | `				SyBlobRelease(&sFqn);` |
|     21501 |  7226 | `			}` |
|         - |  7227 | `		}` |
|         - |  7228 | `	}` |
|    192481 |  7229 | `	pGen->pIn = pIn;` |
|    192481 |  7230 | `	return SXRET_OK;` |
|     96244 |  7231 | `}` |
|         - |  7232 |  |
|         - |  7233 | `/*` |
|         - |  7234 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|         - |  7235 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|         - |  7236 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|         - |  7237 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|         - |  7238 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|         - |  7239 | ` */` |
|    192300 |  7240 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|         5 |  7241 | `{` |
|         - |  7242 | `	int i;` |
|    192305 |  7243 | `	int nNonNull = 0;` |
|    192305 |  7244 | `	int bAnyIntersection = 0;` |
|         - |  7245 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|    192305 |  7246 | `	sxu32 nMaxGroup = 0;` |
|   6345905 |  7247 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    384757 |  7248 | `	for( i = 0; i < nAtoms; i++ ){` |
|    192457 |  7249 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    192427 |  7250 | `			nNonNull++;` |
|    192427 |  7251 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|    192427 |  7252 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|    192427 |  7253 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|     96211 |  7254 | `			}` |
|     96211 |  7255 | `		}` |
|     96231 |  7256 | `	}` |
|    384705 |  7257 | `	for( i = 0; i < nAtoms; i++ ){` |
|    192429 |  7258 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        29 |  7259 | `			bAnyIntersection = 1;` |
|        29 |  7260 | `			break;` |
|         - |  7261 | `		}` |
|     96205 |  7262 | `	}` |
|    192305 |  7263 | `	if( bAnyIntersection ){` |
|         - |  7264 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|         - |  7265 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|         - |  7266 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|        29 |  7267 | `		sxu32 g, nGroups = 0;` |
|        29 |  7268 | `		int bFirstGroup = 1;` |
|        59 |  7269 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|        59 |  7270 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|        35 |  7271 | `			int bFirstMember = 1;` |
|         - |  7272 | `			int bWrap;` |
|        35 |  7273 | `			if( aGroupCount[g] == 0 ) continue;` |
|         - |  7274 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|         - |  7275 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|         - |  7276 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|         - |  7277 | `			 * parens, matching PHP's canonical text. */` |
|        47 |  7278 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|        35 |  7279 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|        35 |  7280 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|       107 |  7281 | `			for( i = 0; i < nAtoms; i++ ){` |
|        77 |  7282 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|        59 |  7283 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|        59 |  7284 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|        55 |  7285 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        30 |  7286 | `				}else{` |
|         6 |  7287 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  7288 | `				}` |
|        59 |  7289 | `				bFirstMember = 0;` |
|        32 |  7290 | `			}` |
|        35 |  7291 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|        35 |  7292 | `			bFirstGroup = 0;` |
|        20 |  7293 | `		}` |
|        29 |  7294 | `		if( bNullable ){` |
|       ! 0 |  7295 | `			SyBlobAppend(pBlob, "\|", 1);` |
|       ! 0 |  7296 | `			SyBlobAppend(pBlob, "null", 4);` |
|       ! 0 |  7297 | `		}` |
|      2031 |  7298 | `		return;` |
|         - |  7299 | `	}` |
|    192281 |  7300 | `	if( nNonNull == 1 && bNullable ){` |
|         - |  7301 | `		/* Shorthand: ?T */` |
|      4009 |  7302 | `		for( i = 0; i < nAtoms; i++ ){` |
|      4009 |  7303 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      4009 |  7304 | `			SyBlobAppend(pBlob, "?", 1);` |
|      4009 |  7305 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|        30 |  7306 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        17 |  7307 | `			}else{` |
|      3983 |  7308 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  7309 | `			}` |
|      4009 |  7310 | `			return;` |
|       ! 0 |  7311 | `		}` |
|       ! 0 |  7312 | `	}` |
|         - |  7313 | `	{` |
|    188277 |  7314 | `		int bFirst = 1;` |
|         - |  7315 | `		/* 1) Classes in declaration order */` |
|    376657 |  7316 | `		for( i = 0; i < nAtoms; i++ ){` |
|    188385 |  7317 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|     43081 |  7318 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     43081 |  7319 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|     43081 |  7320 | `				bFirst = 0;` |
|     21538 |  7321 | `			}` |
|     94195 |  7322 | `		}` |
|         - |  7323 | `		/* 2) Built-ins in canonical order */` |
|         - |  7324 | `		{` |
|         - |  7325 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|         - |  7326 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|         - |  7327 | `			int k;` |
|   1317909 |  7328 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|   2114709 |  7329 | `				for( i = 0; i < nAtoms; i++ ){` |
|   1130173 |  7330 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|    145101 |  7331 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    145101 |  7332 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|    145101 |  7333 | `						bFirst = 0;` |
|    145101 |  7334 | `						break;` |
|         - |  7335 | `					}` |
|    492541 |  7336 | `				}` |
|    564821 |  7337 | `			}` |
|         - |  7338 | `		}` |
|         - |  7339 | `		/* 3) null suffix */` |
|    188277 |  7340 | `		if( bNullable ){` |
|        20 |  7341 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|        20 |  7342 | `			SyBlobAppend(pBlob, "null", 4);` |
|         8 |  7343 | `		}` |
|         - |  7344 | `	}` |
|     96155 |  7345 | `}` |
|         - |  7346 |  |
|         - |  7347 | `/*` |
|         - |  7348 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|         - |  7349 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|         - |  7350 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|         - |  7351 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|         - |  7352 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|         - |  7353 | ` * whether it was parenthesized.` |
|         - |  7354 | ` *` |
|         - |  7355 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|         - |  7356 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|         - |  7357 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|         - |  7358 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|         - |  7359 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|         - |  7360 | ` */` |
|    192452 |  7361 | `static sxi32 GenStateParsePart(` |
|         - |  7362 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|         - |  7363 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|         5 |  7364 | `{` |
|         - |  7365 | `	sxi32 rc;` |
|    192457 |  7366 | `	int nMembers = 0;` |
|    192457 |  7367 | `	int bParen = 0;` |
|    192457 |  7368 | `	*pnMembers = 0;` |
|    192457 |  7369 | `	*pbParen = 0;` |
|    192457 |  7370 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         9 |  7371 | `		bParen = 1;` |
|         9 |  7372 | `		pGen->pIn++; /* skip '(' */` |
|         3 |  7373 | `	}` |
|     96226 |  7374 | `	for(;;){` |
|    192483 |  7375 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|       ! 0 |  7376 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7377 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|       ! 0 |  7378 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7379 | `		}` |
|    192483 |  7380 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|    192483 |  7381 | `		if( rc != SXRET_OK ){` |
|         3 |  7382 | `			return rc;` |
|         - |  7383 | `		}` |
|    192481 |  7384 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|    192481 |  7385 | `		(*pnAtoms)++;` |
|    192481 |  7386 | `		nMembers++;` |
|         - |  7387 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|    192481 |  7388 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        39 |  7389 | `			SyToken *pNext = &pGen->pIn[1];` |
|        34 |  7390 | `			if( pNext < pGen->pEnd` |
|        39 |  7391 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        31 |  7392 | `				pGen->pIn++; /* skip '&' */` |
|        31 |  7393 | `				continue;` |
|         - |  7394 | `			}` |
|         4 |  7395 | `		}` |
|    192455 |  7396 | `		break;` |
|       ! 0 |  7397 | `	}` |
|    192455 |  7398 | `	if( bParen ){` |
|         9 |  7399 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  7400 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7401 | `				"Malformed DNF type: expecting ')'");` |
|       ! 0 |  7402 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7403 | `		}` |
|         9 |  7404 | `		pGen->pIn++; /* skip ')' */` |
|         9 |  7405 | `		if( nMembers < 2 ){` |
|       ! 0 |  7406 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7407 | `				"Parenthesized type must be an intersection of at least two types");` |
|       ! 0 |  7408 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7409 | `		}` |
|         3 |  7410 | `	}` |
|    192455 |  7411 | `	*pnMembers = nMembers;` |
|    192455 |  7412 | `	*pbParen = bParen;` |
|    192455 |  7413 | `	return SXRET_OK;` |
|     96231 |  7414 | `}` |
|         - |  7415 |  |
|         - |  7416 | `/*` |
|         - |  7417 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|         - |  7418 | ` *` |
|         - |  7419 | ` * Outputs:` |
|         - |  7420 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|         - |  7421 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|         - |  7422 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|         - |  7423 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|         - |  7424 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|         - |  7425 | ` *     already be initialized by the caller (allocator set, etc).` |
|         - |  7426 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|         - |  7427 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|         - |  7428 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|         - |  7429 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|         - |  7430 | ` *` |
|         - |  7431 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|         - |  7432 | ` * SXERR_ABORT on fatal compile errors.` |
|         - |  7433 | ` */` |
|    192316 |  7434 | `static sxi32 GenStateParseUnionTypeDecl(` |
|         - |  7435 | `	ph7_gen_state *pGen,` |
|         - |  7436 | `	sxu32 *pnType,` |
|         - |  7437 | `	SyString *pClass,` |
|         - |  7438 | `	SySet *pAlts,` |
|         - |  7439 | `	sxi32 *piTypeFlags,` |
|         - |  7440 | `	SyString *pTypeText,` |
|         - |  7441 | `	int iNullableFlag,` |
|         - |  7442 | `	int iUnionFlag,` |
|         - |  7443 | `	int bAllowVoid,` |
|         - |  7444 | `	sxu32 nLine` |
|         5 |  7445 | `){` |
|         - |  7446 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|    192321 |  7447 | `	int nAtoms = 0;` |
|    192321 |  7448 | `	int bShortNullable = 0;` |
|    192321 |  7449 | `	int bExplicitNull = 0;` |
|         - |  7450 | `	sxi32 rc;` |
|    192321 |  7451 | `	*pnType = 0;` |
|    192321 |  7452 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|    192321 |  7453 | `	*piTypeFlags = 0;` |
|    192321 |  7454 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|         - |  7455 |  |
|    192321 |  7456 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7457 | `		return SXRET_OK;` |
|         - |  7458 | `	}` |
|         - |  7459 | ``	/* Optional `?` shorthand prefix */`` |
|    192316 |  7460 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      3997 |  7461 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      3997 |  7462 | `		bShortNullable = 1;` |
|      3997 |  7463 | `		pGen->pIn++;` |
|      3997 |  7464 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7465 | `			return SXERR_SYNTAX;` |
|         - |  7466 | `		}` |
|      1996 |  7467 | `	}` |
|         - |  7468 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|         - |  7469 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|         - |  7470 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|         - |  7471 | `	{` |
|         - |  7472 | `		int nMembers, bParen;` |
|    192321 |  7473 | `		sxu32 iGroup = 0;` |
|    192321 |  7474 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|    192321 |  7475 | `		if( rc != SXRET_OK ){` |
|         4 |  7476 | `			return rc;` |
|         - |  7477 | `		}` |
|         - |  7478 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|         - |  7479 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|         - |  7480 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|         - |  7481 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|         - |  7482 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|    288680 |  7483 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|    192528 |  7484 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       143 |  7485 | `			if( bShortNullable ){` |
|         - |  7486 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|         - |  7487 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|         - |  7488 | `				 * already reported" so callers skip their own error emission. */` |
|         3 |  7489 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - |  7490 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|         3 |  7491 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|         - |  7492 | `			}` |
|       141 |  7493 | `			if( nMembers >= 2 && !bParen ){` |
|       ! 0 |  7494 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|         - |  7495 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 |  7496 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7497 | `			}` |
|       141 |  7498 | ``			pGen->pIn++; /* skip `\|` */`` |
|       141 |  7499 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|       141 |  7500 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  7501 | `				return rc;` |
|         - |  7502 | `			}` |
|         5 |  7503 | `		}` |
|    192317 |  7504 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|       ! 0 |  7505 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7506 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 |  7507 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7508 | `		}` |
|         - |  7509 | `	}` |
|         - |  7510 | `	/* Validation pass.` |
|         - |  7511 | `	 *` |
|         - |  7512 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|         - |  7513 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|         - |  7514 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|         - |  7515 | `	 */` |
|         - |  7516 | `	{` |
|         - |  7517 | `		int i, j;` |
|    192317 |  7518 | `		int bHasNonNull = 0;` |
|    192317 |  7519 | `		int bAnyIntersection = 0;` |
|         - |  7520 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|         - |  7521 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|         - |  7522 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
|   6346301 |  7523 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    384791 |  7524 | `		for( i = 0; i < nAtoms; i++ ){` |
|    192479 |  7525 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|     96242 |  7526 | `		}` |
|    384735 |  7527 | `		for( i = 0; i < nAtoms; i++ ){` |
|    192449 |  7528 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|     96214 |  7529 | `		}` |
|         - |  7530 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|         - |  7531 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|    192317 |  7532 | `		if( bShortNullable && bAnyIntersection ){` |
|       ! 0 |  7533 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7534 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|       ! 0 |  7535 | `			return SXERR_SYNTAX;` |
|         - |  7536 | `		}` |
|    384777 |  7537 | `		for( i = 0; i < nAtoms; i++ ){` |
|         - |  7538 | `			/* Intersection members must be class/interface types (PHP rejects` |
|         - |  7539 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|         - |  7540 | ``			 * `true`/`false` in an intersection). */`` |
|    192477 |  7541 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        55 |  7542 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|        55 |  7543 | `				if( bClassLike ){` |
|        53 |  7544 | `					SyString *pC = &aAtoms[i].sClass;` |
|        48 |  7545 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|        48 |  7546 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|        48 |  7547 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|        53 |  7548 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|       ! 0 |  7549 | `						bClassLike = 0;` |
|       ! 0 |  7550 | `					}` |
|        24 |  7551 | `				}` |
|        55 |  7552 | `				if( !bClassLike ){` |
|         - |  7553 | `					const char *zName; sxu32 nName;` |
|         3 |  7554 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 |  7555 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|       ! 0 |  7556 | `					}else{` |
|         3 |  7557 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|         - |  7558 | `					}` |
|         4 |  7559 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7560 | `						"Type %.*s cannot be part of an intersection type",` |
|         1 |  7561 | `						(int)nName, zName);` |
|         3 |  7562 | `					return SXERR_SYNTAX;` |
|         - |  7563 | `				}` |
|        24 |  7564 | `			}` |
|    192475 |  7565 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|       183 |  7566 | `				if( nAtoms > 1 ){` |
|         3 |  7567 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7568 | `						"Void can only be used as a standalone type");` |
|         3 |  7569 | `					return SXERR_SYNTAX;` |
|         - |  7570 | `				}` |
|       181 |  7571 | `				if( !bAllowVoid ){` |
|       ! 0 |  7572 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7573 | `						"void cannot be used here");` |
|       ! 0 |  7574 | `					return SXERR_SYNTAX;` |
|         - |  7575 | `				}` |
|       181 |  7576 | `				if( bShortNullable ){` |
|       ! 0 |  7577 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7578 | `						"Void type cannot be nullable");` |
|       ! 0 |  7579 | `					return SXERR_SYNTAX;` |
|         - |  7580 | `				}` |
|        88 |  7581 | `			}` |
|    192473 |  7582 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|         - |  7583 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|         - |  7584 | `				 * type (never = the function does not return). Mirrors the void` |
|         - |  7585 | `				 * validation above; accepted here and enforced at compile time` |
|         - |  7586 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|        27 |  7587 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|         - |  7588 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|         - |  7589 | `					 * same as any other non-standalone use. */` |
|         6 |  7590 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7591 | `						"never can only be used as a standalone type");` |
|         6 |  7592 | `					return SXERR_SYNTAX;` |
|         - |  7593 | `				}` |
|        21 |  7594 | `				if( !bAllowVoid ){` |
|         - |  7595 | `					/* Return-only: params call with bAllowVoid=0. */` |
|         3 |  7596 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7597 | `						"never cannot be used as a parameter type");` |
|         3 |  7598 | `					return SXERR_SYNTAX;` |
|         - |  7599 | `				}` |
|         8 |  7600 | `			}` |
|    192467 |  7601 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|        34 |  7602 | `				bExplicitNull = 1;` |
|        19 |  7603 | `			}else{` |
|    192437 |  7604 | `				bHasNonNull = 1;` |
|         - |  7605 | `			}` |
|         - |  7606 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|         - |  7607 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|         - |  7608 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|         - |  7609 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|         - |  7610 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|    192667 |  7611 | `			for( j = 0; j < i; j++ ){` |
|       207 |  7612 | `				int bDup = 0;` |
|       207 |  7613 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|       395 |  7614 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|       202 |  7615 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|       207 |  7616 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|       195 |  7617 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|        51 |  7618 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|        44 |  7619 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|        44 |  7620 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|        17 |  7621 | `								aAtoms[j].sClass.zString,` |
|        34 |  7622 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|       ! 0 |  7623 | `							bDup = 1;` |
|       ! 0 |  7624 | `						}` |
|        27 |  7625 | `					}else{` |
|         3 |  7626 | `						bDup = 1;` |
|         - |  7627 | `					}` |
|        23 |  7628 | `				}` |
|       195 |  7629 | `				if( bDup ){` |
|         - |  7630 | `					const char *zName;` |
|         - |  7631 | `					sxu32 nName;` |
|         3 |  7632 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 |  7633 | `						zName = aAtoms[i].sClass.zString;` |
|       ! 0 |  7634 | `						nName = aAtoms[i].sClass.nByte;` |
|       ! 0 |  7635 | `					}else{` |
|         3 |  7636 | `						zName = aAtoms[i].zCanon;` |
|         3 |  7637 | `						nName = aAtoms[i].nCanon;` |
|         - |  7638 | `					}` |
|         4 |  7639 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         1 |  7640 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|         3 |  7641 | `					return SXERR_SYNTAX;` |
|         - |  7642 | `				}` |
|        99 |  7643 | `			}` |
|     96235 |  7644 | `		}` |
|    192305 |  7645 | `		if( !bHasNonNull && bExplicitNull ){` |
|         7 |  7646 | `			if( bShortNullable ){` |
|         - |  7647 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|       ! 0 |  7648 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7649 | `					"Null can not be used as a standalone type");` |
|       ! 0 |  7650 | `				return SXERR_SYNTAX;` |
|         - |  7651 | `			}` |
|         - |  7652 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|         - |  7653 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|         - |  7654 | `			 * path below leaves *pnType untouched when there is no non-null` |
|         - |  7655 | `			 * atom, so set it here. */` |
|         7 |  7656 | `			*pnType = MEMOBJ_NULL;` |
|         3 |  7657 | `		}` |
|         - |  7658 | `	}` |
|         - |  7659 | `	/* Compute nullability flag */` |
|    192305 |  7660 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      4025 |  7661 | `		*piTypeFlags \|= iNullableFlag;` |
|      2010 |  7662 | `	}` |
|         - |  7663 | `	/* Build canonical type text */` |
|    192305 |  7664 | `	if( pTypeText ){` |
|         - |  7665 | `		SyBlob sBlob;` |
|    192305 |  7666 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|    286460 |  7667 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|     96150 |  7668 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|    192305 |  7669 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|    288167 |  7670 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    192108 |  7671 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|    192113 |  7672 | `			if( zDup ){` |
|    192113 |  7673 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|     96054 |  7674 | `			}` |
|     96054 |  7675 | `		}` |
|    192305 |  7676 | `		SyBlobRelease(&sBlob);` |
|     96150 |  7677 | `	}` |
|         - |  7678 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|         - |  7679 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|         - |  7680 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|         - |  7681 | `	{` |
|    192305 |  7682 | `		int nNonNull = 0;` |
|    192305 |  7683 | `		int iNonNullIdx = -1;` |
|         - |  7684 | `		int i;` |
|    384757 |  7685 | `		for( i = 0; i < nAtoms; i++ ){` |
|    192457 |  7686 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    192427 |  7687 | `				nNonNull++;` |
|    192427 |  7688 | `				iNonNullIdx = i;` |
|     96211 |  7689 | `			}` |
|     96231 |  7690 | `		}` |
|    192305 |  7691 | `		if( nNonNull <= 1 ){` |
|         - |  7692 | `			/* Fast path: store as single type. */` |
|    192199 |  7693 | `			if( iNonNullIdx >= 0 ){` |
|    192193 |  7694 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|    192193 |  7695 | `				if( pA->nType == SXU32_HIGH ){` |
|     64592 |  7696 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     21529 |  7697 | `						pA->sClass.zString, pA->sClass.nByte);` |
|     43063 |  7698 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|     43063 |  7699 | `					*pnType = SXU32_HIGH;` |
|     43063 |  7700 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|    170664 |  7701 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|       181 |  7702 | `					*pnType = MEMOBJ_VOID;` |
|    149047 |  7703 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|        18 |  7704 | `					*pnType = MEMOBJ_NEVER;` |
|        10 |  7705 | `				}else{` |
|    148943 |  7706 | `					*pnType = pA->nType;` |
|         - |  7707 | `				}` |
|     96094 |  7708 | `			}` |
|     96102 |  7709 | `		}else{` |
|         - |  7710 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|       111 |  7711 | `			*piTypeFlags \|= iUnionFlag;` |
|       355 |  7712 | `			for( i = 0; i < nAtoms; i++ ){` |
|         - |  7713 | `				ph7_type_alt sAlt;` |
|       249 |  7714 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       239 |  7715 | `				SyZero(&sAlt, sizeof(sAlt));` |
|       239 |  7716 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|       239 |  7717 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       146 |  7718 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        47 |  7719 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        99 |  7720 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|        99 |  7721 | `					sAlt.nType = SXU32_HIGH;` |
|        99 |  7722 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|        52 |  7723 | `				}else{` |
|       145 |  7724 | `					sAlt.nType = aAtoms[i].nType;` |
|       145 |  7725 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|         - |  7726 | `				}` |
|       239 |  7727 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|       122 |  7728 | `			}` |
|         - |  7729 | `		}` |
|         - |  7730 | `	}` |
|    192305 |  7731 | `	return SXRET_OK;` |
|     96163 |  7732 | `}` |
|         - |  7733 |  |
|         - |  7734 | `/*` |
|         - |  7735 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|         - |  7736 | `` * pGen->pIn should point to the token after `)`.`` |
|         - |  7737 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|         - |  7738 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|         - |  7739 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|         - |  7740 | `` *          and union types `: T\|U`.`` |
|         - |  7741 | ` */` |
|   3149878 |  7742 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|         5 |  7743 | `{` |
|   3149883 |  7744 | `	sxi32 iFlags = 0;` |
|         - |  7745 | `	sxi32 rc;` |
|         - |  7746 | `	sxu32 nLine;` |
|   3149883 |  7747 | `	pFunc->nReturnType = 0;` |
|   3149883 |  7748 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|   3149883 |  7749 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|         - |  7750 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|         - |  7751 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|         - |  7752 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|         - |  7753 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|         - |  7754 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|   3149883 |  7755 | `	SySetReset(&pFunc->aReturnUnion);` |
|   3149883 |  7756 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|   3149883 |  7757 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|   3117953 |  7758 | `		return SXRET_OK;` |
|         - |  7759 | `	}` |
|     31935 |  7760 | `	pGen->pIn++; /* Skip ':' */` |
|     31935 |  7761 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7762 | `		return SXRET_OK;` |
|         - |  7763 | `	}` |
|     31935 |  7764 | `	nLine = pGen->pIn->nLine;` |
|     31935 |  7765 | `	rc = GenStateParseUnionTypeDecl(` |
|     15965 |  7766 | `		pGen,` |
|     15965 |  7767 | `		&pFunc->nReturnType,` |
|     15965 |  7768 | `		&pFunc->sReturnClass,` |
|     15965 |  7769 | `		&pFunc->aReturnUnion,` |
|         - |  7770 | `		&iFlags,` |
|     15965 |  7771 | `		&pFunc->sReturnTypeName,` |
|         - |  7772 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|         - |  7773 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|         - |  7774 | `		/* iUnionFlag */ 0,` |
|         - |  7775 | `		/* bAllowVoid */ 1,` |
|     15965 |  7776 | `		nLine);` |
|     31935 |  7777 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  7778 | `		return SXERR_ABORT;` |
|         - |  7779 | `	}` |
|     31935 |  7780 | `	if( rc == SXERR_CORRUPT ){` |
|         - |  7781 | `		/* Error already reported */` |
|       ! 0 |  7782 | `		return SXERR_SYNTAX;` |
|         - |  7783 | `	}` |
|     31935 |  7784 | `	if( rc == SXERR_SYNTAX ){` |
|         9 |  7785 | `		if( pGen->pIn < pGen->pEnd ){` |
|        12 |  7786 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - |  7787 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|         6 |  7788 | `				&pGen->pIn->sData);` |
|         6 |  7789 | `		}else{` |
|       ! 0 |  7790 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|         - |  7791 | `				"syntax error, unexpected end of file in return type declaration");` |
|         - |  7792 | `		}` |
|         9 |  7793 | `		return SXERR_SYNTAX;` |
|         - |  7794 | `	}` |
|     31929 |  7795 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     31929 |  7796 | `	return SXRET_OK;` |
|   1574944 |  7797 | `}` |
|         - |  7798 |  |
|    499808 |  7799 | `static sxi32 GenStateCompileFunc(` |
|         - |  7800 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  7801 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|         - |  7802 | `	sxi32 iFlags,        /* Control flags */` |
|         - |  7803 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|         - |  7804 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|         - |  7805 | `	)` |
|         5 |  7806 | `{` |
|         - |  7807 | `	ph7_vm_func *pFunc;` |
|         - |  7808 | `	SyToken *pEnd;` |
|         - |  7809 | `	sxu32 nLine;` |
|         - |  7810 | `	char *zName;` |
|         - |  7811 | `	sxi32 rc;` |
|         - |  7812 | `	/* Extract line number */` |
|    499813 |  7813 | `	nLine = pGen->pIn->nLine;` |
|         - |  7814 | `	/* Jump the left parenthesis '(' */` |
|    499813 |  7815 | `	pGen->pIn++;` |
|         - |  7816 | `	/* Delimit the function signature */` |
|    499813 |  7817 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    499813 |  7818 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  7819 | `		/* Syntax error */` |
|         9 |  7820 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable");` |
|         3 |  7821 | `		(void)pName;` |
|         9 |  7822 | `		if( rc == SXERR_ABORT ){` |
|         - |  7823 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  7824 | `			return SXERR_ABORT;` |
|         - |  7825 | `		}` |
|         9 |  7826 | `		pGen->pIn = pGen->pEnd;` |
|         9 |  7827 | `		return SXRET_OK;` |
|         - |  7828 | `	}` |
|         - |  7829 | `	/* Create the function state */` |
|    499807 |  7830 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|    499807 |  7831 | `	if( pFunc == 0 ){` |
|       ! 0 |  7832 | `		goto OutOfMem;` |
|         - |  7833 | `	}` |
|         - |  7834 | `	/* Build the function name, prepending namespace if active */` |
|    499815 |  7835 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|         - |  7836 | `		SyBlob sFQN;` |
|         - |  7837 | `		sxu32 nLen;` |
|        18 |  7838 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        18 |  7839 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        18 |  7840 | `		SyBlobAppend(&sFQN,"\\",1);` |
|        18 |  7841 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|        18 |  7842 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|        18 |  7843 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|        18 |  7844 | `		SyBlobRelease(&sFQN);` |
|        18 |  7845 | `		if( zName == 0 ){` |
|       ! 0 |  7846 | `			goto OutOfMem;` |
|         - |  7847 | `		}` |
|        18 |  7848 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|        10 |  7849 | `	}else{` |
|    499791 |  7850 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    499791 |  7851 | `		if( zName == 0 ){` |
|       ! 0 |  7852 | `			goto OutOfMem;` |
|         - |  7853 | `		}` |
|    499791 |  7854 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|         - |  7855 | `	}` |
|         - |  7856 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|         - |  7857 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|    499807 |  7858 | `	pFunc->nLine = nLine;` |
|    499807 |  7859 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|    499807 |  7860 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  7861 | `		return SXERR_ABORT;` |
|         - |  7862 | `	}` |
|    499807 |  7863 | `	if( pGen->pIn < pEnd ){` |
|         - |  7864 | `		/* Collect function arguments */` |
|    432733 |  7865 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|    432733 |  7866 | `		if( rc == SXERR_ABORT ){` |
|         - |  7867 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  7868 | `			return SXERR_ABORT;` |
|         - |  7869 | `		}` |
|    216364 |  7870 | `	}` |
|         - |  7871 | `	/* Point past ')' and parse optional return type ': type' */` |
|    499807 |  7872 | `	pGen->pIn = &pEnd[1];` |
|         - |  7873 | `	{` |
|    499807 |  7874 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|    499807 |  7875 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 |  7876 | `			return SXERR_ABORT;` |
|    499807 |  7877 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|         9 |  7878 | `			return SXERR_SYNTAX;` |
|         - |  7879 | `		}` |
|         - |  7880 | `	}` |
|    499801 |  7881 | `	if( bHandleClosure ){` |
|         - |  7882 | `		ph7_vm_func_closure_env sEnv;` |
|       599 |  7883 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|       594 |  7884 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       348 |  7885 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|        97 |  7886 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  7887 | `				/* Closure,record environment variable */` |
|        97 |  7888 | `				pGen->pIn++;` |
|        97 |  7889 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  7890 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|       ! 0 |  7891 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  7892 | `						return SXERR_ABORT;` |
|         - |  7893 | `					}` |
|       ! 0 |  7894 | `				}` |
|        97 |  7895 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|         - |  7896 | `				/* Compile until we hit the first closing parenthesis */` |
|       199 |  7897 | `				while( pGen->pIn < pGen->pEnd ){` |
|       199 |  7898 | `					int iFlagsLocal = 0;` |
|       199 |  7899 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|        97 |  7900 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|        97 |  7901 | `						break;` |
|         - |  7902 | `					}` |
|       107 |  7903 | `					nLineLocal = pGen->pIn->nLine;` |
|       107 |  7904 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - |  7905 | `						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry` |
|         - |  7906 | `						 * to the variable's memory slot instead of copying its value. */` |
|        60 |  7907 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|        60 |  7908 | `						pGen->pIn++;` |
|        29 |  7909 | `					}` |
|       102 |  7910 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|       107 |  7911 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  7912 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|         - |  7913 | `								"Closure: Unexpected token. Expecting a variable name");` |
|       ! 0 |  7914 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 |  7915 | `								return SXERR_ABORT;` |
|         - |  7916 | `							}` |
|         - |  7917 | `							/* Find the closing parenthesis */` |
|       ! 0 |  7918 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  7919 | `								pGen->pIn++;` |
|       ! 0 |  7920 | `							}` |
|       ! 0 |  7921 | `							if(pGen->pIn < pGen->pEnd){` |
|       ! 0 |  7922 | `								pGen->pIn++;` |
|       ! 0 |  7923 | `							}` |
|       ! 0 |  7924 | `							break;` |
|         - |  7925 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|       ! 0 |  7926 | `					}else{` |
|         - |  7927 | `						SyString *pNameLocal;` |
|         - |  7928 | `						char *zDup;` |
|         - |  7929 | `						/* Duplicate variable name */` |
|       107 |  7930 | `						pNameLocal = &pGen->pIn[1].sData;` |
|       107 |  7931 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|       107 |  7932 | `						if( zDup ){` |
|         - |  7933 | `							/* Zero the structure */` |
|       107 |  7934 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       107 |  7935 | `							sEnv.iFlags = iFlagsLocal;` |
|       107 |  7936 | `							sEnv.nIdx = SXU32_HIGH;` |
|       107 |  7937 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       107 |  7938 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|       122 |  7939 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|        30 |  7940 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|       ! 0 |  7941 | `									got_this = 1;` |
|       ! 0 |  7942 | `							}` |
|         - |  7943 | `							/* Save imported variable */` |
|       107 |  7944 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|        56 |  7945 | `						}else{` |
|       ! 0 |  7946 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  7947 | `							 return SXERR_ABORT;` |
|         - |  7948 | `						}` |
|         - |  7949 | `					}` |
|       107 |  7950 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|       119 |  7951 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  7952 | `						/* Ignore trailing commas */` |
|        13 |  7953 | `						pGen->pIn++;` |
|         1 |  7954 | `					}` |
|         5 |  7955 | `				}` |
|         - |  7956 | `				/* php 7.1+: the return type follows the use clause —` |
|         - |  7957 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|         - |  7958 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|         - |  7959 | `				 * so an unconditional call would wipe a type parsed at the` |
|         - |  7960 | `				 * legacy pre-use position. */` |
|        97 |  7961 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|         7 |  7962 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|         7 |  7963 | `					if( rcRt2 == SXERR_ABORT ){` |
|       ! 0 |  7964 | `						return SXERR_ABORT;` |
|         7 |  7965 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|       ! 0 |  7966 | `						return SXERR_SYNTAX;` |
|         - |  7967 | `					}` |
|         3 |  7968 | `				}` |
|        46 |  7969 | `		}` |
|       599 |  7970 | `		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){` |
|         - |  7971 | `			/* Make the $this variable [Current processed Object (class instance)]` |
|         - |  7972 | `			 * available to the closure environment — for EVERY non-static` |
|         - |  7973 | `			 * anonymous function, use list or not (php binds $this to any` |
|         - |  7974 | ``			 * closure declared in a method; pre-fix only `use (...)` closures`` |
|         - |  7975 | `			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of` |
|         - |  7976 | `			 * a global-scope closure is silently dropped at install. A static` |
|         - |  7977 | `			 * closure never binds $this (php). */` |
|       577 |  7978 | `			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       577 |  7979 | `			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|       577 |  7980 | `			sEnv.nIdx = SXU32_HIGH;` |
|       577 |  7981 | `			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       577 |  7982 | `			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|       577 |  7983 | `			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       286 |  7984 | `		}` |
|       599 |  7985 | `		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|         - |  7986 | `			/* Mark as closure */` |
|       579 |  7987 | `			pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       287 |  7988 | `		}` |
|       297 |  7989 | `	}` |
|         - |  7990 | `	/* Compile the body */` |
|    499801 |  7991 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|    499801 |  7992 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  7993 | `		return SXERR_ABORT;` |
|         - |  7994 | `	}` |
|         - |  7995 | `	/* The cursor sits just past the body's closing brace */` |
|    499801 |  7996 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|    499801 |  7997 | `	if( ppFunc ){` |
|    499801 |  7998 | `		*ppFunc = pFunc;` |
|    249898 |  7999 | `	}` |
|    499801 |  8000 | `	rc = SXRET_OK;` |
|    499801 |  8001 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|         - |  8002 | `		/* Finally register the function */` |
|    499227 |  8003 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|    249611 |  8004 | `	}` |
|    499801 |  8005 | `	if( rc == SXRET_OK ){` |
|    499801 |  8006 | `		return SXRET_OK;` |
|         - |  8007 | `	}` |
|         - |  8008 | `	/* Fall through if something goes wrong */` |
|       ! 0 |  8009 | `OutOfMem:` |
|         - |  8010 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  8011 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  8012 | `	 */` |
|       ! 0 |  8013 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 |  8014 | `	return SXERR_ABORT;` |
|    249909 |  8015 | `}` |
|         - |  8016 | `/*` |
|         - |  8017 | ` * Compile a standard PHP function.` |
|         - |  8018 | ` *  Refer to the block-comment above for more information.` |
|         - |  8019 | ` */` |
|    499222 |  8020 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|         5 |  8021 | `{` |
|         - |  8022 | `	SyString *pName;` |
|         - |  8023 | `	sxi32 iFlags;` |
|         - |  8024 | `	sxu32 nKwLine;` |
|         - |  8025 | `	sxu32 nLine;` |
|         - |  8026 | `	sxi32 rc;` |
|         - |  8027 |  |
|    499227 |  8028 | `	nLine = pGen->pIn->nLine;` |
|    499227 |  8029 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|    499227 |  8030 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|    499227 |  8031 | `	iFlags = 0;` |
|    499227 |  8032 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - |  8033 | `		/* Return by reference,remember that */` |
|        12 |  8034 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|         - |  8035 | `		/* Jump the '&' token */` |
|        12 |  8036 | `		pGen->pIn++;` |
|         5 |  8037 | `	}` |
|    499227 |  8038 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  8039 | `		/* Invalid function name */` |
|         8 |  8040 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         8 |  8041 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8042 | `			return SXERR_ABORT;` |
|         - |  8043 | `		}` |
|         - |  8044 | `		/* Sychronize with the next semi-colon or braces*/` |
|        22 |  8045 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        16 |  8046 | `			pGen->pIn++;` |
|         2 |  8047 | `		}` |
|         8 |  8048 | `		return SXRET_OK;` |
|         - |  8049 | `	}` |
|    499221 |  8050 | `	pName = &pGen->pIn->sData;` |
|    499221 |  8051 | `	nLine = pGen->pIn->nLine;` |
|         - |  8052 | `	/* Jump the function name */` |
|    499221 |  8053 | `	pGen->pIn++;` |
|    499221 |  8054 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  8055 | `		/* Syntax error */` |
|         3 |  8056 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         3 |  8057 | `		if( rc == SXERR_ABORT ){` |
|         - |  8058 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8059 | `			return SXERR_ABORT;` |
|         - |  8060 | `		}` |
|         - |  8061 | `		/* Sychronize with the next semi-colon or '{' */` |
|         3 |  8062 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  8063 | `			pGen->pIn++;` |
|       ! 0 |  8064 | `		}` |
|         3 |  8065 | `		return SXRET_OK;` |
|         - |  8066 | `	}` |
|         - |  8067 | `	/* Compile function body */` |
|         - |  8068 | `	{` |
|    499219 |  8069 | `		ph7_vm_func *pFuncState = 0;` |
|    499219 |  8070 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);` |
|    499219 |  8071 | `		if( pFuncState ){` |
|         - |  8072 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|    499207 |  8073 | `			pFuncState->nLine = nKwLine;` |
|    249601 |  8074 | `		}` |
|         - |  8075 | `	}` |
|    499219 |  8076 | `	return rc;` |
|    249616 |  8077 | `}` |
|         - |  8078 | `/*` |
|         - |  8079 | ` * Extract the visibility level associated with a given keyword.` |
|         - |  8080 | ` * According to the PHP language reference manual` |
|         - |  8081 | ` *  Visibility:` |
|         - |  8082 | ` *  The visibility of a property or method can be defined by prefixing` |
|         - |  8083 | ` *  the declaration with the keywords public, protected or private.` |
|         - |  8084 | ` *  Class members declared public can be accessed everywhere.` |
|         - |  8085 | ` *  Members declared protected can be accessed only within the class` |
|         - |  8086 | ` *  itself and by inherited and parent classes. Members declared as private` |
|         - |  8087 | ` *  may only be accessed by the class that defines the member.` |
|         - |  8088 | ` */` |
|   3498408 |  8089 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|         5 |  8090 | `{` |
|   3498413 |  8091 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    260721 |  8092 | `		return PH7_CLASS_PROT_PRIVATE;` |
|   3237697 |  8093 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|    233385 |  8094 | `		return PH7_CLASS_PROT_PROTECTED;` |
|         - |  8095 | `	}` |
|         - |  8096 | `	/* Assume public by default */` |
|   3004317 |  8097 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   1749209 |  8098 | `}` |
|         - |  8099 | `/*` |
|         - |  8100 | ` * Compile a class constant.` |
|         - |  8101 | ` * According to the PHP language reference manual` |
|         - |  8102 | ` *  Class Constants` |
|         - |  8103 | ` *   It is possible to define constant values on a per-class basis remaining` |
|         - |  8104 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|         - |  8105 | ` *   you don't use the $ symbol to declare or use them.` |
|         - |  8106 | ` *   The value must be a constant expression, not (for example) a variable,` |
|         - |  8107 | ` *   a property, a result of a mathematical operation, or a function call.` |
|         - |  8108 | ` *   It's also possible for interfaces to have constants.` |
|         - |  8109 | ` * Symisc eXtension.` |
|         - |  8110 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|         - |  8111 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  8112 | ` *  Example:` |
|         - |  8113 | ` *   class Test{` |
|         - |  8114 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|         - |  8115 | ` *   };` |
|         - |  8116 | ` *   var_dump(TEST::MyConst);` |
|         - |  8117 | ` *   Refer to the official documentation for more information on the powerful extension` |
|         - |  8118 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|         - |  8119 | ` */` |
|         - |  8120 | `/*` |
|         - |  8121 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|         - |  8122 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|         - |  8123 | ` * token immediately followed by '='. Anything else with a leading type token` |
|         - |  8124 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|         - |  8125 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|         - |  8126 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|         - |  8127 | ` */` |
|    342336 |  8128 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|         5 |  8129 | `{` |
|         - |  8130 | `	SyToken *p0, *p1;` |
|    342341 |  8131 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  8132 | `		return 0;` |
|         - |  8133 | `	}` |
|    342341 |  8134 | `	p0 = pGen->pIn;` |
|         - |  8135 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|    342341 |  8136 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|       ! 0 |  8137 | `		return 1;` |
|         - |  8138 | `	}` |
|    342341 |  8139 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|         5 |  8140 | `		return 1;` |
|         - |  8141 | `	}` |
|         - |  8142 | `	/* A name-like first token begins a type only when followed by another` |
|         - |  8143 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|         - |  8144 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|    342337 |  8145 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|    342337 |  8146 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|    342337 |  8147 | `		if( p1 ){` |
|    342337 |  8148 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|        34 |  8149 | `				return 1;` |
|         - |  8150 | `			}` |
|    342307 |  8151 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|         5 |  8152 | `				return 1;` |
|         - |  8153 | `			}` |
|    171149 |  8154 | `		}` |
|    171149 |  8155 | `	}` |
|    342303 |  8156 | `	return 0;` |
|    171173 |  8157 | `}` |
|         - |  8158 | `/*` |
|         - |  8159 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|         - |  8160 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|         - |  8161 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|         - |  8162 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|         - |  8163 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|         - |  8164 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|         - |  8165 | ` * Peek only; never consumes tokens.` |
|         - |  8166 | ` */` |
|        24 |  8167 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|         4 |  8168 | `{` |
|        28 |  8169 | `	SyToken *p = pGen->pIn;` |
|        39 |  8170 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        20 |  8171 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|         3 |  8172 | `		p++; /* skip leading unary sign(s) */` |
|         1 |  8173 | `	}` |
|        28 |  8174 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|        23 |  8175 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|         - |  8176 | `	}` |
|         6 |  8177 | `	p++;` |
|         - |  8178 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|         6 |  8179 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|        16 |  8180 | `}` |
|         - |  8181 | `/*` |
|         - |  8182 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|         - |  8183 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|         - |  8184 | `` * `$o->new`), not a `new` expression.`` |
|         - |  8185 | ` */` |
|       110 |  8186 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|         3 |  8187 | `{` |
|         - |  8188 | `	sxi32 iOp;` |
|       113 |  8189 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|        11 |  8190 | `		return 0;` |
|         - |  8191 | `	}` |
|       103 |  8192 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|       103 |  8193 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|        58 |  8194 | `}` |
|         - |  8195 | `/*` |
|         - |  8196 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|         - |  8197 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|         - |  8198 | ` * interface-constant and (instance/static) property-default initializers` |
|         - |  8199 | ` * ("New expressions are not supported in this context") while still allowing it` |
|         - |  8200 | ` * in global constants, parameter defaults and static-local initializers (which` |
|         - |  8201 | ` * are compiled by different functions and left untouched). The scan is` |
|         - |  8202 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|         - |  8203 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|         - |  8204 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|         - |  8205 | ` *` |
|         - |  8206 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|         - |  8207 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|         - |  8208 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|         - |  8209 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|         - |  8210 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|         - |  8211 | ` */` |
|    708536 |  8212 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|         5 |  8213 | `{` |
|    708541 |  8214 | `	SyToken *p = pGen->pIn;` |
|    708541 |  8215 | `	int iDepth = 0;` |
|   1857913 |  8216 | `	while( p < pGen->pEnd ){` |
|   1857913 |  8217 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    708489 |  8218 | `			break; /* end of this initializer */` |
|         - |  8219 | `		}` |
|   1149424 |  8220 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|    580566 |  8221 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|     11698 |  8222 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|         - |  8223 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|         - |  8224 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|         - |  8225 | `			 * expression. */` |
|         3 |  8226 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|         3 |  8227 | `			p++;` |
|         3 |  8228 | `			if( bArrow ){` |
|         - |  8229 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|         - |  8230 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|         3 |  8231 | `				int iBase = iDepth;` |
|        17 |  8232 | `				while( p < pGen->pEnd ){` |
|        17 |  8233 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         5 |  8234 | `						iDepth++;` |
|        15 |  8235 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         5 |  8236 | `						if( iDepth <= iBase ){` |
|       ! 0 |  8237 | `							break; /* closes an enclosing group, not the fn's own */` |
|         - |  8238 | `						}` |
|         5 |  8239 | `						iDepth--;` |
|        11 |  8240 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|         3 |  8241 | `						break;` |
|         - |  8242 | `					}` |
|        15 |  8243 | `					p++;` |
|         1 |  8244 | `				}` |
|         2 |  8245 | `			}else{` |
|         - |  8246 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|         - |  8247 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|         - |  8248 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|         - |  8249 | `				 * then skip the balanced brace block. */` |
|       ! 0 |  8250 | `				int iLocal = 0;` |
|       ! 0 |  8251 | `				while( p < pGen->pEnd ){` |
|       ! 0 |  8252 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|       ! 0 |  8253 | `						break; /* body brace */` |
|         - |  8254 | `					}` |
|       ! 0 |  8255 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       ! 0 |  8256 | `						iLocal++;` |
|       ! 0 |  8257 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       ! 0 |  8258 | `						if( iLocal > 0 ){` |
|       ! 0 |  8259 | `							iLocal--;` |
|       ! 0 |  8260 | `						}` |
|       ! 0 |  8261 | `					}` |
|       ! 0 |  8262 | `					p++;` |
|       ! 0 |  8263 | `				}` |
|       ! 0 |  8264 | `				if( p < pGen->pEnd ){` |
|       ! 0 |  8265 | `					int iBrace = 0; /* p is on the body '{' */` |
|       ! 0 |  8266 | `					while( p < pGen->pEnd ){` |
|       ! 0 |  8267 | `						if( p->nType & PH7_TK_OCB ){` |
|       ! 0 |  8268 | `							iBrace++;` |
|       ! 0 |  8269 | `						}else if( p->nType & PH7_TK_CCB ){` |
|       ! 0 |  8270 | `							iBrace--;` |
|       ! 0 |  8271 | `							if( iBrace == 0 ){` |
|       ! 0 |  8272 | `								p++;` |
|       ! 0 |  8273 | `								break;` |
|         - |  8274 | `							}` |
|       ! 0 |  8275 | `						}` |
|       ! 0 |  8276 | `						p++;` |
|       ! 0 |  8277 | `					}` |
|       ! 0 |  8278 | `				}` |
|         - |  8279 | `			}` |
|         3 |  8280 | `			continue;` |
|         - |  8281 | `		}` |
|   1149427 |  8282 | `		if( p->nType & PH7_TK_OCB ){` |
|        45 |  8283 | `			if( iDepth == 0 ){` |
|         - |  8284 | `				/* A depth-0 '{' can only open a PHP 8.4 property-hook list` |
|         - |  8285 | ``				 * (`public T $x = default { get …; }`): the default expression`` |
|         - |  8286 | ``				 * ends here. A `new` inside a hook BODY runs at access time and`` |
|         - |  8287 | `				 * is legal — don't scan into it. */` |
|        45 |  8288 | `				break;` |
|         - |  8289 | `			}` |
|       ! 0 |  8290 | `			iDepth++;` |
|   1149383 |  8291 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     46753 |  8292 | `			iDepth++;` |
|   1126009 |  8293 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     46751 |  8294 | `			if( iDepth > 0 ){` |
|     46751 |  8295 | `				iDepth--;` |
|     23373 |  8296 | `			}` |
|   1079262 |  8297 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|    376195 |  8298 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|         - |  8299 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|         - |  8300 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|         - |  8301 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|        10 |  8302 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|        10 |  8303 | `				return 1;` |
|         - |  8304 | `			}` |
|       ! 0 |  8305 | `		}` |
|   1149375 |  8306 | `		p++;` |
|         5 |  8307 | `	}` |
|    708533 |  8308 | `	return 0;` |
|    354273 |  8309 | `}` |
|         - |  8310 | `/*` |
|         - |  8311 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|         - |  8312 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|         - |  8313 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|         - |  8314 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|         - |  8315 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|         - |  8316 | ` * share the same backing.` |
|         - |  8317 | ` */` |
|     15940 |  8318 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|         - |  8319 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|         5 |  8320 | `{` |
|     15945 |  8321 | `	pAttr->nType = nType;` |
|     15945 |  8322 | `	pAttr->sClass = *pClass;` |
|     15945 |  8323 | `	pAttr->sTypeName = *pTypeName;` |
|     15945 |  8324 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  8325 | `		sxu32 i;` |
|        72 |  8326 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        50 |  8327 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|        50 |  8328 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|        27 |  8329 | `		}` |
|        11 |  8330 | `	}` |
|     15945 |  8331 | `}` |
|    342336 |  8332 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  8333 | `{` |
|    342341 |  8334 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  8335 | `	SySet *pInstrContainer;` |
|         - |  8336 | `	ph7_class_attr *pCons;` |
|         - |  8337 | `	SyString *pName;` |
|         - |  8338 | `	sxi32 rc;` |
|    342341 |  8339 | `	sxu32 nType = 0;` |
|         - |  8340 | `	SyString sTypeClass;` |
|         - |  8341 | `	SyString sTypeText;` |
|         - |  8342 | `	SySet aUnionAlts;` |
|    342341 |  8343 | `	sxi32 iTypeFlags = 0;` |
|    342341 |  8344 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    342341 |  8345 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    342341 |  8346 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  8347 | `	/* Extract visibility level */` |
|    342341 |  8348 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  8349 | `	/* Mark as constant */` |
|    342341 |  8350 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|    342341 |  8351 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|         - |  8352 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|         - |  8353 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|    342360 |  8354 | `	if( GenStateClassConstHasType(pGen) ){` |
|        61 |  8355 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|        38 |  8356 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|         - |  8357 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|         - |  8358 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|         - |  8359 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|         - |  8360 | `		 * and success paths release. */` |
|        42 |  8361 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  8362 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  8363 | `			goto Synchronize;` |
|        42 |  8364 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  8365 | `			return SXERR_ABORT;` |
|        42 |  8366 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  8367 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  8368 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|       ! 0 |  8369 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8370 | `				return SXERR_ABORT;` |
|         - |  8371 | `			}` |
|       ! 0 |  8372 | `			goto Synchronize;` |
|         - |  8373 | `		}` |
|        42 |  8374 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        19 |  8375 | `	}` |
|    171168 |  8376 | `loop:` |
|    342343 |  8377 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - |  8378 | `		/* Invalid constant name */` |
|       ! 0 |  8379 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|       ! 0 |  8380 | `		if( rc == SXERR_ABORT ){` |
|         - |  8381 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8382 | `			return SXERR_ABORT;` |
|         - |  8383 | `		}` |
|       ! 0 |  8384 | `		goto Synchronize;` |
|         - |  8385 | `	}` |
|         - |  8386 | `	/* Peek constant name */` |
|    342343 |  8387 | `	pName = &pGen->pIn->sData;` |
|         - |  8388 | `	/* Make sure the constant name isn't reserved */` |
|    342343 |  8389 | `	if( GenStateIsReservedConstant(pName) ){` |
|         - |  8390 | `		/* Reserved constant name */` |
|       ! 0 |  8391 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|       ! 0 |  8392 | `		if( rc == SXERR_ABORT ){` |
|         - |  8393 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8394 | `			return SXERR_ABORT;` |
|         - |  8395 | `		}` |
|       ! 0 |  8396 | `		goto Synchronize;` |
|         - |  8397 | `	}` |
|         - |  8398 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|    342343 |  8399 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        61 |  8400 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|        38 |  8401 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|        19 |  8402 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|        42 |  8403 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8404 | `			return SXERR_ABORT;` |
|        42 |  8405 | `		}else if( rc != SXRET_OK ){` |
|         3 |  8406 | `			goto Synchronize;` |
|         - |  8407 | `		}` |
|        18 |  8408 | `	}` |
|         - |  8409 | `	/* Advance the stream cursor */` |
|    342341 |  8410 | `	pGen->pIn++;` |
|    342341 |  8411 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|         - |  8412 | `		/* Invalid declaration */` |
|       ! 0 |  8413 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|       ! 0 |  8414 | `		if( rc == SXERR_ABORT ){` |
|         - |  8415 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8416 | `			return SXERR_ABORT;` |
|         - |  8417 | `		}` |
|       ! 0 |  8418 | `		goto Synchronize;` |
|         - |  8419 | `	}` |
|    342341 |  8420 | `	pGen->pIn++; /* Jump the equal sign */` |
|         - |  8421 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|         - |  8422 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|         - |  8423 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|         - |  8424 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|    342336 |  8425 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|        39 |  8426 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|         8 |  8427 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8428 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|         2 |  8429 | `			&pClass->sName,pName,&sTypeText);` |
|         6 |  8430 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8431 | `			return SXERR_ABORT;` |
|         - |  8432 | `		}` |
|         6 |  8433 | `		goto Synchronize;` |
|         - |  8434 | `	}` |
|         - |  8435 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|         - |  8436 | `	 * constant initializer ("New expressions are not supported in this context").` |
|         - |  8437 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|    342337 |  8438 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|         5 |  8439 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8440 | `			"New expressions are not supported in this context");` |
|         5 |  8441 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8442 | `			return SXERR_ABORT;` |
|         - |  8443 | `		}` |
|         5 |  8444 | `		goto Synchronize;` |
|         - |  8445 | `	}` |
|         - |  8446 | `	/* Allocate a new class attribute */` |
|    342333 |  8447 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    342333 |  8448 | `	if( pCons ){` |
|    342333 |  8449 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|    342333 |  8450 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8451 | `			return SXERR_ABORT;` |
|         - |  8452 | `		}` |
|    171164 |  8453 | `	}` |
|    342333 |  8454 | `	if( pCons == 0 ){` |
|       ! 0 |  8455 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8456 | `		return SXERR_ABORT;` |
|         - |  8457 | `	}` |
|    342333 |  8458 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        35 |  8459 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|        16 |  8460 | `	}` |
|         - |  8461 | `	/* Swap bytecode container */` |
|    342333 |  8462 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    342333 |  8463 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|         - |  8464 | `	/* Compile constant value.` |
|         - |  8465 | `	 */` |
|    342333 |  8466 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    342333 |  8467 | `	if( rc == SXERR_EMPTY ){` |
|         3 |  8468 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|         3 |  8469 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8470 | `			return SXERR_ABORT;` |
|         - |  8471 | `		}` |
|         1 |  8472 | `	}` |
|         - |  8473 | `	/* Emit the done instruction */` |
|    342333 |  8474 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    342333 |  8475 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    342333 |  8476 | `	if( rc == SXERR_ABORT ){` |
|         - |  8477 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  8478 | `		return SXERR_ABORT;` |
|         - |  8479 | `	}` |
|         - |  8480 | `	/* All done,install the constant */` |
|    342333 |  8481 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|    342333 |  8482 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8483 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8484 | `		return SXERR_ABORT;` |
|         - |  8485 | `	}` |
|    342333 |  8486 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  8487 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|         3 |  8488 | `		pGen->pIn++; /* Jump the comma */` |
|         3 |  8489 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 |  8490 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 |  8491 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 |  8492 | `				pTok--;` |
|       ! 0 |  8493 | `			}` |
|       ! 0 |  8494 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  8495 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|       ! 0 |  8496 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 |  8497 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8498 | `				return SXERR_ABORT;` |
|         - |  8499 | `			}` |
|       ! 0 |  8500 | `		}else{` |
|         3 |  8501 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|         3 |  8502 | `				goto loop;` |
|         - |  8503 | `			}` |
|         - |  8504 | `		}` |
|       ! 0 |  8505 | `	}` |
|    342331 |  8506 | `	SySetRelease(&aUnionAlts);` |
|    342331 |  8507 | `	return SXRET_OK;` |
|         5 |  8508 | `Synchronize:` |
|        13 |  8509 | `	SySetRelease(&aUnionAlts);` |
|         - |  8510 | `	/* Synchronize with the first semi-colon */` |
|        45 |  8511 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        35 |  8512 | `		pGen->pIn++;` |
|         3 |  8513 | `	}` |
|        13 |  8514 | `	return SXERR_CORRUPT;` |
|    171173 |  8515 | `}` |
|         - |  8516 | `/*` |
|         - |  8517 | ` * complie a class attribute or Properties in the PHP jargon.` |
|         - |  8518 | ` * According to the PHP language reference manual` |
|         - |  8519 | ` *  Properties` |
|         - |  8520 | ` *  Class member variables are called "properties". You may also see them referred` |
|         - |  8521 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|         - |  8522 | ` *  of this reference we will use "properties". They are defined by using one` |
|         - |  8523 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|         - |  8524 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|         - |  8525 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|         - |  8526 | ` *  and must not depend on run-time information in order to be evaluated.` |
|         - |  8527 | ` * Symisc eXtension.` |
|         - |  8528 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|         - |  8529 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  8530 | ` *  Example:` |
|         - |  8531 | ` *   class Test{` |
|         - |  8532 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|         - |  8533 | ` *   };` |
|         - |  8534 | ` *   var_dump(TEST::myVar);` |
|         - |  8535 | ` *   Refer to the official documentation for more information on the powerful extension` |
|         - |  8536 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|         - |  8537 | ` */` |
|         - |  8538 | `/*` |
|         - |  8539 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|         - |  8540 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|         - |  8541 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|         - |  8542 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|         - |  8543 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|         - |  8544 | ` */` |
|   2622464 |  8545 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|         5 |  8546 | `{` |
|   2622469 |  8547 | `	SyToken *p = pStart;` |
|   2622469 |  8548 | `	int bFirst = 1;` |
|   2622469 |  8549 | `	if( p >= pEnd ) return 0;` |
|         - |  8550 | ``	/* Optional nullable `?` shorthand. */`` |
|   2622469 |  8551 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|        41 |  8552 | `		p++;` |
|        41 |  8553 | `		if( p >= pEnd ) return 0;` |
|        19 |  8554 | `	}` |
|         - |  8555 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|         - |  8556 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|         - |  8557 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|         - |  8558 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   1311232 |  8559 | `	for(;;){` |
|   2622489 |  8560 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|         - |  8561 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|         3 |  8562 | `			p++;` |
|         9 |  8563 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|         3 |  8564 | `			if( p >= pEnd ) return 0;` |
|         3 |  8565 | `			p++; /* skip ')' */` |
|         2 |  8566 | `		}else{` |
|         - |  8567 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|         - |  8568 | ``			 * then any `&`-joined intersection members. */`` |
|   2622487 |  8569 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|   2622487 |  8570 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  8571 | `				return 0;` |
|         - |  8572 | `			}` |
|         - |  8573 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|         - |  8574 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|         - |  8575 | `			 * may still appear at the initial dispatch site). */` |
|   2622487 |  8576 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|   2622435 |  8577 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|   2622430 |  8578 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    124910 |  8579 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|   2606567 |  8580 | `					return 0;` |
|         - |  8581 | `				}` |
|      7934 |  8582 | `			}` |
|     15925 |  8583 | `			p++;` |
|     15927 |  8584 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  8585 | `				p += 2;` |
|         1 |  8586 | `			}` |
|     23883 |  8587 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|     15928 |  8588 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  8589 | `				p++; /* skip '&' */` |
|         3 |  8590 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|         3 |  8591 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|         3 |  8592 | `				p++;` |
|         3 |  8593 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       ! 0 |  8594 | `					p += 2;` |
|       ! 0 |  8595 | `				}` |
|         1 |  8596 | `			}` |
|         - |  8597 | `		}` |
|     15927 |  8598 | `		bFirst = 0;` |
|     15922 |  8599 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        25 |  8600 | `			&& p->sData.zString[0] == '\|' ){` |
|        24 |  8601 | ``			p++; /* next `\|`-separated part */`` |
|        24 |  8602 | `			continue;` |
|         - |  8603 | `		}` |
|     15907 |  8604 | `		break;` |
|       ! 0 |  8605 | `	}` |
|     15907 |  8606 | `	if( p >= pEnd ) return 0;` |
|     15907 |  8607 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   1311237 |  8608 | `}` |
|         - |  8609 |  |
|         - |  8610 | `/*` |
|         - |  8611 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|         - |  8612 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|         - |  8613 | ` * if not). Recognized forms:` |
|         - |  8614 | ` *   ?Type, array, bool, int, float, string, object,` |
|         - |  8615 | ` *   self, parent, \Ns\ClassName, ClassName` |
|         - |  8616 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|         - |  8617 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|         - |  8618 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|         - |  8619 | ` * on unrecoverable error.` |
|         - |  8620 | ` *` |
|         - |  8621 | ` * When a type is parsed:` |
|         - |  8622 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|         - |  8623 | ` *   *pClass is set to the class name (for class types)` |
|         - |  8624 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|         - |  8625 | ` *   *pTypeText is set to the original text span of the type` |
|         - |  8626 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|         - |  8627 | ` */` |
|     15912 |  8628 | `static sxi32 GenStateParsePropertyType(` |
|         - |  8629 | `	ph7_gen_state *pGen,` |
|         - |  8630 | `	sxu32 *pnType,` |
|         - |  8631 | `	SyString *pClass,` |
|         - |  8632 | `	sxi32 *piTypeFlags,` |
|         - |  8633 | `	SyString *pTypeText,` |
|         - |  8634 | `	SySet *pAlts` |
|         5 |  8635 | `){` |
|     15917 |  8636 | `	sxi32 iFlags = 0;` |
|         - |  8637 | `	sxi32 rc;` |
|     15917 |  8638 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  8639 | `		return SXRET_OK;` |
|         - |  8640 | `	}` |
|         - |  8641 | `	/* If the first token is '$', there's no type */` |
|     15917 |  8642 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       ! 0 |  8643 | `		return SXRET_OK;` |
|         - |  8644 | `	}` |
|     15917 |  8645 | `	rc = GenStateParseUnionTypeDecl(` |
|      7956 |  8646 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|         - |  8647 | `		PH7_CLASS_ATTR_NULLABLE,` |
|         - |  8648 | `		PH7_CLASS_ATTR_UNION,` |
|         - |  8649 | `		/* bAllowVoid */ 0,` |
|     15912 |  8650 | `		pGen->pIn->nLine);` |
|     15917 |  8651 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8652 | `		return rc;` |
|         - |  8653 | `	}` |
|         - |  8654 | `	/* Verify next token is '$' (start of property name) */` |
|     15917 |  8655 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  8656 | `		return SXERR_SYNTAX;` |
|         - |  8657 | `	}` |
|     15917 |  8658 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     15917 |  8659 | `	return SXRET_OK;` |
|      7961 |  8660 | `}` |
|         - |  8661 |  |
|         - |  8662 | `/*` |
|         - |  8663 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|         - |  8664 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|         - |  8665 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|         - |  8666 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|         - |  8667 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|         - |  8668 | ` * by the type parser itself before reaching here.` |
|         - |  8669 | ` *` |
|         - |  8670 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|         - |  8671 | ` * use in the error message.` |
|         - |  8672 | ` */` |
|     16090 |  8673 | `static int GenStateIsDisallowedPropertyAtom(` |
|         - |  8674 | `	sxu32 nType,` |
|         - |  8675 | `	const SyString *pClass,` |
|         - |  8676 | `	const char **pzName,` |
|         - |  8677 | `	sxu32 *pnName)` |
|         5 |  8678 | `{` |
|         - |  8679 | `	const char *z;` |
|         - |  8680 | `	sxu32 n;` |
|     16095 |  8681 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     16033 |  8682 | `		return 0;` |
|         - |  8683 | `	}` |
|        65 |  8684 | `	z = pClass->zString;` |
|        65 |  8685 | `	n = pClass->nByte;` |
|        65 |  8686 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|         8 |  8687 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|         - |  8688 | `	}` |
|         - |  8689 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|         - |  8690 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|         - |  8691 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|        59 |  8692 | `	return 0;` |
|      8050 |  8693 | `}` |
|         - |  8694 |  |
|         - |  8695 | `/*` |
|         - |  8696 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|         - |  8697 | ` * constant) — the main atom plus any union alternatives — against the` |
|         - |  8698 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|         - |  8699 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|         - |  8700 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|         - |  8701 | ` * type T" vs "Class constant C::X cannot have type T").` |
|         - |  8702 | ` *` |
|         - |  8703 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|         - |  8704 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|         - |  8705 | ` */` |
|     16028 |  8706 | `static sxi32 GenStateValidateMemberType(` |
|         - |  8707 | `	ph7_gen_state *pGen,` |
|         - |  8708 | `	ph7_class *pClass,` |
|         - |  8709 | `	const SyString *pMemberName,` |
|         - |  8710 | `	sxu32 nType,` |
|         - |  8711 | `	const SyString *pTypeClass,` |
|         - |  8712 | `	const SyString *pTypeText,` |
|         - |  8713 | `	SySet *pUnionAlts,` |
|         - |  8714 | `	const char *zErrFmt,` |
|         - |  8715 | `	sxu32 nLine)` |
|         5 |  8716 | `{` |
|     16033 |  8717 | `	const char *zBad = 0;` |
|     16033 |  8718 | `	sxu32 nBad = 0;` |
|         - |  8719 | `	SyString sFallback;` |
|         - |  8720 | `	const SyString *pBad;` |
|         - |  8721 | `	sxi32 rc;` |
|     16033 |  8722 | `	int bDisallowed = 0;` |
|     16033 |  8723 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|         5 |  8724 | `		bDisallowed = 1;` |
|     16031 |  8725 | `	}else if( pUnionAlts ){` |
|         - |  8726 | `		sxu32 i;` |
|        95 |  8727 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|        67 |  8728 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|        67 |  8729 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|         3 |  8730 | `				bDisallowed = 1;` |
|         3 |  8731 | `				break;` |
|         - |  8732 | `			}` |
|        35 |  8733 | `		}` |
|        15 |  8734 | `	}` |
|     16033 |  8735 | `	if( !bDisallowed ){` |
|     16027 |  8736 | `		return SXRET_OK;` |
|         - |  8737 | `	}` |
|         - |  8738 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|         - |  8739 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|         - |  8740 | `	 * canonical spelling if the type text is unavailable. */` |
|         8 |  8741 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|         8 |  8742 | `		pBad = pTypeText;` |
|         5 |  8743 | `	}else{` |
|       ! 0 |  8744 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|       ! 0 |  8745 | `		pBad = &sFallback;` |
|         - |  8746 | `	}` |
|        11 |  8747 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         3 |  8748 | `		zErrFmt,` |
|         3 |  8749 | `		&pClass->sName,pMemberName,pBad);` |
|         8 |  8750 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  8751 | `		return SXERR_ABORT;` |
|         - |  8752 | `	}` |
|         8 |  8753 | `	return SXERR_SYNTAX;` |
|      8019 |  8754 | `}` |
|         - |  8755 | `/*` |
|         - |  8756 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|         - |  8757 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|         - |  8758 | ` * matched as a plain identifier in the class-member modifier position rather` |
|         - |  8759 | ` * than promoted to a lexer keyword.` |
|         - |  8760 | ` */` |
|  22025010 |  8761 | `static int GenStateIsReadonly(SyToken *pTok)` |
|         5 |  8762 | `{` |
|  22251333 |  8763 | `	return (pTok->nType & PH7_TK_ID)` |
|  11238823 |  8764 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
|  22251328 |  8765 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|         5 |  8766 | `}` |
|         - |  8767 | `/*` |
|         - |  8768 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|         - |  8769 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|         - |  8770 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|         - |  8771 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|         - |  8772 | ` */` |
|   7978398 |  8773 | `static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|         5 |  8774 | `{` |
|   7978403 |  8775 | `	*pnTok = 0;` |
|   7978398 |  8776 | `	if( &pTok[3] < pEnd` |
|   7501196 |  8777 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|   6239674 |  8778 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|   2727685 |  8779 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        16 |  8780 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|        16 |  8781 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|        21 |  8782 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|        17 |  8783 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|        17 |  8784 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|        17 |  8785 | `			*pnTok = 4;` |
|        17 |  8786 | `			return nKw;` |
|         - |  8787 | `		}` |
|       ! 0 |  8788 | `	}` |
|   7978387 |  8789 | `	return 0;` |
|   3989204 |  8790 | `}` |
|         - |  8791 | `/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */` |
|        16 |  8792 | `static sxi32 GenStateSetVisFlag(sxi32 nKw)` |
|         1 |  8793 | `{` |
|        17 |  8794 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|        13 |  8795 | `		return PH7_CLASS_ATTR_PRIVATE_SET;` |
|         - |  8796 | `	}` |
|         5 |  8797 | `	if( nKw == PH7_TKWRD_PROTECTED ){` |
|         3 |  8798 | `		return PH7_CLASS_ATTR_PROTECTED_SET;` |
|         - |  8799 | `	}` |
|         3 |  8800 | `	return PH7_CLASS_ATTR_PUBLIC_SET;` |
|         9 |  8801 | `}` |
|    506494 |  8802 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  8803 | `{` |
|    506499 |  8804 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  8805 | `	ph7_class_attr *pAttr;` |
|         - |  8806 | `	SyString *pName;` |
|         - |  8807 | `	sxi32 rc;` |
|    506499 |  8808 | `	sxu32 nType = 0;` |
|         - |  8809 | `	SyString sTypeClass;` |
|         - |  8810 | `	SyString sTypeText;` |
|         - |  8811 | `	SySet aUnionAlts;` |
|    506499 |  8812 | `	sxi32 iTypeFlags = 0;` |
|    506499 |  8813 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    506499 |  8814 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    506499 |  8815 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  8816 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|         - |  8817 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|         - |  8818 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|    506499 |  8819 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|        21 |  8820 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|         9 |  8821 | `	}` |
|         - |  8822 | `	/* Extract visibility level */` |
|    506499 |  8823 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  8824 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|    514455 |  8825 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     15917 |  8826 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     15917 |  8827 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  8828 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  8829 | `			goto Synchronize;` |
|     15917 |  8830 | `		}else if( rc == SXERR_SYNTAX ){` |
|       ! 0 |  8831 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8832 | `				"Invalid property type or declaration near '%z'",` |
|       ! 0 |  8833 | `				&pGen->pIn->sData);` |
|       ! 0 |  8834 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8835 | `				return SXERR_ABORT;` |
|         - |  8836 | `			}` |
|       ! 0 |  8837 | `			goto Synchronize;` |
|     15917 |  8838 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  8839 | `			return SXERR_ABORT;` |
|         - |  8840 | `		}` |
|      7956 |  8841 | `	}` |
|       ! 0 |  8842 | `loop:` |
|    506503 |  8843 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  8844 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|       ! 0 |  8845 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8846 | `			return SXERR_ABORT;` |
|         - |  8847 | `		}` |
|       ! 0 |  8848 | `		goto Synchronize;` |
|         - |  8849 | `	}` |
|    506503 |  8850 | `	pGen->pIn++; /* Jump the dollar sign */` |
|    506503 |  8851 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|         - |  8852 | `		/* Invalid attribute name */` |
|       ! 0 |  8853 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|       ! 0 |  8854 | `		if( rc == SXERR_ABORT ){` |
|         - |  8855 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8856 | `			return SXERR_ABORT;` |
|         - |  8857 | `		}` |
|       ! 0 |  8858 | `		goto Synchronize;` |
|         - |  8859 | `	}` |
|         - |  8860 | `	/* Peek attribute name */` |
|    506503 |  8861 | `	pName = &pGen->pIn->sData;` |
|         - |  8862 | `	/* Advance the stream cursor */` |
|    506503 |  8863 | `	pGen->pIn++;` |
|    506503 |  8864 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/\|PH7_TK_OCB/*'{' hooks*/)) == 0 ){` |
|         - |  8865 | `		/* Invalid declaration */` |
|         3 |  8866 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|         3 |  8867 | `		if( rc == SXERR_ABORT ){` |
|         - |  8868 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8869 | `			return SXERR_ABORT;` |
|         - |  8870 | `		}` |
|         3 |  8871 | `		goto Synchronize;` |
|         - |  8872 | `	}` |
|         - |  8873 | `	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and` |
|         - |  8874 | `	 * the read visibility must not be narrower than the set visibility. */` |
|    506501 |  8875 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
|        13 |  8876 | `		const char *zAvErr = 0;` |
|        19 |  8877 | `		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE` |
|        10 |  8878 | `			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED` |
|         2 |  8879 | `			: PH7_CLASS_PROT_PUBLIC;` |
|        13 |  8880 | `		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 |  8881 | `			zAvErr = "Property with asymmetric visibility %z::$%z must have type";` |
|        13 |  8882 | `		}else if( iProtection > iSetLevel ){` |
|       ! 0 |  8883 | `			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";` |
|       ! 0 |  8884 | `		}` |
|        13 |  8885 | `		if( zAvErr ){` |
|       ! 0 |  8886 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sName,pName);` |
|       ! 0 |  8887 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8888 | `				return SXERR_ABORT;` |
|         - |  8889 | `			}` |
|       ! 0 |  8890 | `			goto Synchronize;` |
|         - |  8891 | `		}` |
|         6 |  8892 | `	}` |
|         - |  8893 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|         - |  8894 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|    506501 |  8895 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|        49 |  8896 | `		const char *zRoErr = 0;` |
|        49 |  8897 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|         3 |  8898 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|        48 |  8899 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         6 |  8900 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|        45 |  8901 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|         6 |  8902 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|         2 |  8903 | `		}` |
|        49 |  8904 | `		if( zRoErr ){` |
|        13 |  8905 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|        13 |  8906 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8907 | `				return SXERR_ABORT;` |
|         - |  8908 | `			}` |
|        13 |  8909 | `			goto Synchronize;` |
|         - |  8910 | `		}` |
|        17 |  8911 | `	}` |
|         - |  8912 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|         - |  8913 | `	 * type atom or any union alternative. void/never are already rejected` |
|         - |  8914 | `	 * by the type parser. */` |
|    506491 |  8915 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     23870 |  8916 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|         - |  8917 | `			&sTypeText,` |
|     15910 |  8918 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      7955 |  8919 | `			"Property %z::$%z cannot have type %z",nLine);` |
|     15915 |  8920 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8921 | `			return SXERR_ABORT;` |
|     15915 |  8922 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  8923 | `			goto Synchronize;` |
|         - |  8924 | `		}` |
|      7955 |  8925 | `	}` |
|         - |  8926 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|    506491 |  8927 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|         4 |  8928 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  8929 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|         3 |  8930 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8931 | `			return SXERR_ABORT;` |
|         - |  8932 | `		}` |
|         3 |  8933 | `		goto Synchronize;` |
|         - |  8934 | `	}` |
|         - |  8935 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|         - |  8936 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|         - |  8937 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|         - |  8938 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|         - |  8939 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|         - |  8940 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|    506489 |  8941 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|         6 |  8942 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8943 | `			"New expressions are not supported in this context");` |
|         6 |  8944 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8945 | `			return SXERR_ABORT;` |
|         - |  8946 | `		}` |
|         6 |  8947 | `		goto Synchronize;` |
|         - |  8948 | `	}` |
|         - |  8949 | `	/* Allocate a new class attribute */` |
|    506485 |  8950 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    506485 |  8951 | `	if( pAttr ){` |
|    506485 |  8952 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|    506485 |  8953 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8954 | `			return SXERR_ABORT;` |
|         - |  8955 | `		}` |
|    253240 |  8956 | `	}` |
|    506485 |  8957 | `	if( pAttr == 0 ){` |
|       ! 0 |  8958 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  8959 | `		return SXERR_ABORT;` |
|         - |  8960 | `	}` |
|    506485 |  8961 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     15913 |  8962 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      7954 |  8963 | `	}` |
|    506485 |  8964 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|         - |  8965 | `		SySet *pInstrContainer;` |
|    366205 |  8966 | `		SyToken *pSavedDefEnd = pGen->pEnd;` |
|    366205 |  8967 | `		pGen->pIn++; /*Jump the equal sign */` |
|         - |  8968 | `		{` |
|         - |  8969 | `			/* Delimit the default expression: it ends at the declaration's` |
|         - |  8970 | `			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list` |
|         - |  8971 | ``			 * (`public string $w = "init" { get => …; }`) — the expression`` |
|         - |  8972 | `			 * compiler would otherwise run into the hook tokens. */` |
|    366205 |  8973 | `			SyToken *pScan = pGen->pIn;` |
|    366205 |  8974 | `			sxi32 iNest = 0;` |
|    802959 |  8975 | `			while( pScan < pGen->pEnd ){` |
|    802959 |  8976 | `				if( pScan->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     46747 |  8977 | `					iNest++;` |
|    779588 |  8978 | `				}else if( pScan->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|     46747 |  8979 | `					iNest--;` |
|    732846 |  8980 | `				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI\|PH7_TK_COMMA\|PH7_TK_OCB)) ){` |
|    366205 |  8981 | `					break;` |
|         - |  8982 | `				}` |
|    436759 |  8983 | `				pScan++;` |
|         5 |  8984 | `			}` |
|    366205 |  8985 | `			pGen->pEnd = pScan;` |
|         - |  8986 | `		}` |
|         - |  8987 | `		/* Swap bytecode container */` |
|    366205 |  8988 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    366205 |  8989 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|         - |  8990 | `		/* Compile attribute value.` |
|         - |  8991 | `		 */` |
|    366205 |  8992 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    366205 |  8993 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 |  8994 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|       ! 0 |  8995 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8996 | `				return SXERR_ABORT;` |
|         - |  8997 | `			}` |
|       ! 0 |  8998 | `		}` |
|         - |  8999 | `		/* Emit the done instruction */` |
|    366205 |  9000 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    366205 |  9001 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    366205 |  9002 | `		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */` |
|    366205 |  9003 | `		pGen->pEnd = pSavedDefEnd;` |
|    183100 |  9004 | `	}` |
|         - |  9005 | `	/* All done,install the attribute */` |
|    506485 |  9006 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|    506485 |  9007 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  9008 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9009 | `		return SXERR_ABORT;` |
|         - |  9010 | `	}` |
|    506485 |  9011 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){` |
|         - |  9012 | ``		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.`` |
|         - |  9013 | `		 * The list ends the declaration at '}' — no trailing ';', no comma list. */` |
|        95 |  9014 | `		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);` |
|        95 |  9015 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  9016 | `			return SXERR_ABORT;` |
|         - |  9017 | `		}` |
|        95 |  9018 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  9019 | `			goto Synchronize;` |
|         - |  9020 | `		}` |
|        95 |  9021 | `		SySetRelease(&aUnionAlts);` |
|        95 |  9022 | `		return SXRET_OK;` |
|         - |  9023 | `	}` |
|    506391 |  9024 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - |  9025 | ``		/* php 8.4: `abstract` on a property requires a hook list (php's exact`` |
|         - |  9026 | `		 * wording differs per declaration site) */` |
|       ! 0 |  9027 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  9028 | `			(pClass->iFlags & PH7_CLASS_INTERFACE)` |
|         - |  9029 | `				? "Interfaces may only include hooked properties"` |
|         - |  9030 | `				: "Only hooked properties may be declared abstract");` |
|       ! 0 |  9031 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  9032 | `			return SXERR_ABORT;` |
|         - |  9033 | `		}` |
|       ! 0 |  9034 | `		goto Synchronize;` |
|         - |  9035 | `	}` |
|    506391 |  9036 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  9037 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|         5 |  9038 | `		pGen->pIn++; /* Jump the comma */` |
|         5 |  9039 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|       ! 0 |  9040 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 |  9041 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 |  9042 | `				pTok--;` |
|       ! 0 |  9043 | `			}` |
|       ! 0 |  9044 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  9045 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|       ! 0 |  9046 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 |  9047 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9048 | `				return SXERR_ABORT;` |
|         - |  9049 | `			}` |
|       ! 0 |  9050 | `		}else{` |
|         5 |  9051 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         5 |  9052 | `				goto loop;` |
|         - |  9053 | `			}` |
|         - |  9054 | `		}` |
|       ! 0 |  9055 | `	}` |
|    506387 |  9056 | `	SySetRelease(&aUnionAlts);` |
|    506387 |  9057 | `	return SXRET_OK;` |
|         9 |  9058 | `Synchronize:` |
|         - |  9059 | `	/* Synchronize with the first semi-colon */` |
|        56 |  9060 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        37 |  9061 | `		pGen->pIn++;` |
|         3 |  9062 | `	}` |
|        22 |  9063 | `	SySetRelease(&aUnionAlts);` |
|        22 |  9064 | `	return SXERR_CORRUPT;` |
|    253252 |  9065 | `}` |
|         - |  9066 | `/*` |
|         - |  9067 | ` * Compile a class method.` |
|         - |  9068 | ` *` |
|         - |  9069 | ` * Refer to the official documentation for more information` |
|         - |  9070 | ` * on the powerful extension introduced by the PH7 engine` |
|         - |  9071 | ` * to the OO subsystem such as full type hinting,method` |
|         - |  9072 | ` * overloading and many more.` |
|         - |  9073 | ` */` |
|   2649578 |  9074 | `static sxi32 GenStateCompileClassMethod(` |
|         - |  9075 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  9076 | `	sxi32 iProtection,   /* Visibility level */` |
|         - |  9077 | `	sxi32 iFlags,        /* Configuration flags */` |
|         - |  9078 | `	int doBody,          /* TRUE to process method body */` |
|         - |  9079 | `	ph7_class *pClass    /* Class this method belongs */` |
|         - |  9080 | `	)` |
|         5 |  9081 | `{` |
|   2649583 |  9082 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   2649583 |  9083 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|         - |  9084 | `	ph7_class_method *pMeth;` |
|         - |  9085 | `	sxi32 iFuncFlags;` |
|         - |  9086 | `	SyString *pName;` |
|         - |  9087 | `	SyToken *pEnd;` |
|         - |  9088 | `	sxi32 rc;` |
|         - |  9089 | `	/* Extract visibility level */` |
|   2649583 |  9090 | `	iProtection = GetProtectionLevel(iProtection);` |
|   2649583 |  9091 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   2649583 |  9092 | `	iFuncFlags = 0;` |
|   2649583 |  9093 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  9094 | `		/* Invalid method name */` |
|       ! 0 |  9095 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|       ! 0 |  9096 | `		if( rc == SXERR_ABORT ){` |
|         - |  9097 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9098 | `			return SXERR_ABORT;` |
|         - |  9099 | `		}` |
|       ! 0 |  9100 | `		goto Synchronize;` |
|         - |  9101 | `	}` |
|   2649583 |  9102 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - |  9103 | `		/* Return by reference,remember that */` |
|       ! 0 |  9104 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|         - |  9105 | `		/* Jump the '&' token */` |
|       ! 0 |  9106 | `		pGen->pIn++;` |
|       ! 0 |  9107 | `	}` |
|   2649583 |  9108 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  9109 | `		/* Invalid method name */` |
|       ! 0 |  9110 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|       ! 0 |  9111 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  9112 | `			return SXERR_ABORT;` |
|         - |  9113 | `		}` |
|       ! 0 |  9114 | `		goto Synchronize;` |
|         - |  9115 | `	}` |
|         - |  9116 | `	/* Peek method name */` |
|   2649583 |  9117 | `	pName = &pGen->pIn->sData;` |
|   2649583 |  9118 | `	nLine = pGen->pIn->nLine;` |
|         - |  9119 | `	/* Jump the method name */` |
|   2649583 |  9120 | `	pGen->pIn++;` |
|   2649583 |  9121 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - |  9122 | `		/* Abstract method */` |
|    140049 |  9123 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       ! 0 |  9124 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9125 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|       ! 0 |  9126 | `				&pClass->sName,pName);` |
|       ! 0 |  9127 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9128 | `				return SXERR_ABORT;` |
|         - |  9129 | `			}` |
|       ! 0 |  9130 | `		}` |
|         - |  9131 | `		/* Assemble method signature only */` |
|    140049 |  9132 | `		doBody = FALSE;` |
|     70022 |  9133 | `	}` |
|   2649583 |  9134 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  9135 | `		/* Syntax error */` |
|       ! 0 |  9136 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|       ! 0 |  9137 | `		if( rc == SXERR_ABORT ){` |
|         - |  9138 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9139 | `			return SXERR_ABORT;` |
|         - |  9140 | `		}` |
|       ! 0 |  9141 | `		goto Synchronize;` |
|         - |  9142 | `	}` |
|         - |  9143 | `	/* Allocate a new class_method instance */` |
|   2649583 |  9144 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|   2649583 |  9145 | `	if( pMeth == 0 ){` |
|       ! 0 |  9146 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9147 | `		return SXERR_ABORT;` |
|         - |  9148 | `	}` |
|   2649583 |  9149 | `	pMeth->sFunc.nLine = nKwLine;` |
|   2649583 |  9150 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|   2649583 |  9151 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  9152 | `		return SXERR_ABORT;` |
|         - |  9153 | `	}` |
|         - |  9154 | `	/* Jump the left parenthesis '(' */` |
|   2649583 |  9155 | `	pGen->pIn++;` |
|   2649583 |  9156 | `	pEnd = 0; /* cc warning */` |
|         - |  9157 | `	/* Delimit the method signature */` |
|   2649583 |  9158 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   2649583 |  9159 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  9160 | `		/* Syntax error */` |
|         3 |  9161 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|         3 |  9162 | `		if( rc == SXERR_ABORT ){` |
|         - |  9163 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9164 | `			return SXERR_ABORT;` |
|         - |  9165 | `		}` |
|         3 |  9166 | `		goto Synchronize;` |
|         - |  9167 | `	}` |
|         - |  9168 | `	{` |
|   2649581 |  9169 | `		int bIsCtor = 0;` |
|   2649581 |  9170 | `		int bAbstractCtor = 0;` |
|   2649576 |  9171 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|   1562125 |  9172 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|   2556157 |  9173 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|    186853 |  9174 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         3 |  9175 | `				bAbstractCtor = 1;` |
|         2 |  9176 | `			}else{` |
|    186851 |  9177 | `				bIsCtor = 1;` |
|         - |  9178 | `			}` |
|     93424 |  9179 | `		}` |
|   2649581 |  9180 | `		if( pGen->pIn < pEnd ){` |
|         - |  9181 | `			/* Collect method arguments */` |
|    922195 |  9182 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|    922195 |  9183 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9184 | `				return SXERR_ABORT;` |
|         - |  9185 | `			}` |
|    461095 |  9186 | `		}` |
|         - |  9187 | `	}` |
|         - |  9188 | `	/* Point past ')' and parse optional return type ': type' */` |
|   2649581 |  9189 | `	pGen->pIn = &pEnd[1];` |
|         - |  9190 | `	{` |
|   2649581 |  9191 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|   2649581 |  9192 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 |  9193 | `			return SXERR_ABORT;` |
|   2649581 |  9194 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       ! 0 |  9195 | `			goto Synchronize;` |
|         - |  9196 | `		}` |
|         - |  9197 | `	}` |
|         - |  9198 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|         - |  9199 | `	 * property init/typecheck is handled by the generic typed-property path` |
|         - |  9200 | `	 * since we mint real ph7_class_attr entries. */` |
|         - |  9201 | `	{` |
|   2649581 |  9202 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|         - |  9203 | `		sxu32 i;` |
|   4026861 |  9204 | `		for( i = 0; i < nArg; i++ ){` |
|   1377295 |  9205 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|         - |  9206 | `			ph7_class_attr *pAttr;` |
|   1377295 |  9207 | `			sxi32 iAttrFlags = 0;` |
|         - |  9208 | `			int bArgTyped;` |
|   1377295 |  9209 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   1377209 |  9210 | `				continue;` |
|         - |  9211 | `			}` |
|         - |  9212 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|         - |  9213 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|         - |  9214 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|        60 |  9215 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|        92 |  9216 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|        91 |  9217 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|         3 |  9218 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9219 | `					"Cannot declare variadic promoted property");` |
|         3 |  9220 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9221 | `					return SXERR_ABORT;` |
|         - |  9222 | `				}` |
|         3 |  9223 | `				goto Synchronize;` |
|         - |  9224 | `			}` |
|         - |  9225 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|         - |  9226 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|         - |  9227 | `			 * appear as an alternative of a union type. */` |
|        89 |  9228 | `			if( bArgTyped ){` |
|       125 |  9229 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|        80 |  9230 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|        80 |  9231 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|        40 |  9232 | `					"Property %z::$%z cannot have type %z",nLine);` |
|        85 |  9233 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9234 | `					return SXERR_ABORT;` |
|        85 |  9235 | `				}else if( rc != SXRET_OK ){` |
|         6 |  9236 | `					goto Synchronize;` |
|         - |  9237 | `				}` |
|        38 |  9238 | `			}` |
|         - |  9239 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|        85 |  9240 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|         4 |  9241 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  9242 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|         3 |  9243 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9244 | `					return SXERR_ABORT;` |
|         - |  9245 | `				}` |
|         3 |  9246 | `				goto Synchronize;` |
|         - |  9247 | `			}` |
|        83 |  9248 | `			if( bArgTyped ){` |
|        79 |  9249 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        37 |  9250 | `			}` |
|        83 |  9251 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|         3 |  9252 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|         1 |  9253 | `			}` |
|        83 |  9254 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|         8 |  9255 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|         3 |  9256 | `			}` |
|        83 |  9257 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|         - |  9258 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|         - |  9259 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|        26 |  9260 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         4 |  9261 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  9262 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|         3 |  9263 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9264 | `						return SXERR_ABORT;` |
|         - |  9265 | `					}` |
|         3 |  9266 | `					goto Synchronize;` |
|         - |  9267 | `				}` |
|        24 |  9268 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|        10 |  9269 | `			}` |
|        81 |  9270 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|         - |  9271 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|         5 |  9272 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 |  9273 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9274 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|       ! 0 |  9275 | `						&pClass->sName,&pArg->sName);` |
|       ! 0 |  9276 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9277 | `						return SXERR_ABORT;` |
|         - |  9278 | `					}` |
|       ! 0 |  9279 | `					goto Synchronize;` |
|         - |  9280 | `				}` |
|         5 |  9281 | `				iAttrFlags \|= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)` |
|         2 |  9282 | `					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;` |
|         2 |  9283 | `			}` |
|        81 |  9284 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|        81 |  9285 | `			if( pAttr == 0 ){` |
|       ! 0 |  9286 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9287 | `				return SXERR_ABORT;` |
|         - |  9288 | `			}` |
|        81 |  9289 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|        79 |  9290 | `				pAttr->nType = pArg->nType;` |
|        79 |  9291 | `				pAttr->sClass = pArg->sClass;` |
|        79 |  9292 | `				pAttr->sTypeName = pArg->sTypeName;` |
|        79 |  9293 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  9294 | `					sxu32 k;` |
|        20 |  9295 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|        14 |  9296 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|        14 |  9297 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|         8 |  9298 | `					}` |
|         3 |  9299 | `				}` |
|        37 |  9300 | `			}` |
|        81 |  9301 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|        81 |  9302 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9303 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9304 | `				return SXERR_ABORT;` |
|         - |  9305 | `			}` |
|        43 |  9306 | `		}` |
|         - |  9307 | `	}` |
|   2649571 |  9308 | `	if( doBody ){` |
|         - |  9309 | `		/* Compile method body */` |
|   2509527 |  9310 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|   2509527 |  9311 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  9312 | `			return SXERR_ABORT;` |
|         - |  9313 | `		}` |
|         - |  9314 | `		/* The cursor sits just past the body's closing brace */` |
|   2509527 |  9315 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|   1254766 |  9316 | `	}else{` |
|         - |  9317 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|    140049 |  9318 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|    140049 |  9319 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|     70022 |  9320 | `		}` |
|         - |  9321 | `		/* Only method signature is allowed */` |
|    140049 |  9322 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|       ! 0 |  9323 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9324 | `				"Expected ';' after method signature '%z'",pName);` |
|       ! 0 |  9325 | `				if( rc == SXERR_ABORT ){` |
|         - |  9326 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9327 | `					return SXERR_ABORT;` |
|         - |  9328 | `				}` |
|       ! 0 |  9329 | `				return SXERR_CORRUPT;` |
|         - |  9330 | `			}` |
|         - |  9331 | `	}` |
|         - |  9332 | `	/* All done,install the method */` |
|   2649571 |  9333 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|   2649571 |  9334 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  9335 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9336 | `		return SXERR_ABORT;` |
|         - |  9337 | `	}` |
|   2649571 |  9338 | `	return SXRET_OK;` |
|         6 |  9339 | `Synchronize:` |
|         - |  9340 | `	/* Synchronize with the first semi-colon */` |
|        40 |  9341 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        28 |  9342 | `		pGen->pIn++;` |
|         4 |  9343 | `	}` |
|        16 |  9344 | `	return SXERR_CORRUPT;` |
|   1324794 |  9345 | `}` |
|         - |  9346 | `/*` |
|         - |  9347 | `` * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a`` |
|         - |  9348 | ` * property declaration. Each hook body is synthesized into a hidden public` |
|         - |  9349 | ` * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,` |
|         - |  9350 | ` * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /` |
|         - |  9351 | ` * OP_STORE route reads and plain writes through them (a per-instance guard` |
|         - |  9352 | ` * makes $this->NAME inside a hook body address the raw backing slot — php's` |
|         - |  9353 | `` * rule that hooks see the backing store). `get => expr;` compiles as an`` |
|         - |  9354 | `` * implicit return (the arrow-fn pattern); `set => expr;` compiles the same`` |
|         - |  9355 | ` * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return` |
|         - |  9356 | `` * value to the backing slot. A `set` without a parameter list receives the`` |
|         - |  9357 | `` * implicit `$value` formal.`` |
|         - |  9358 | ` * On entry pGen->pIn sits on '{'; on success it sits just past '}'.` |
|         - |  9359 | ` */` |
|         - |  9360 | `/*` |
|         - |  9361 | `` * Whether any token in [pStart, pEnd) spells `$this->NAME` (this property's own`` |
|         - |  9362 | `` * name; `?->` and `::` member ops count too). php 8.4's virtual-vs-backed rule:`` |
|         - |  9363 | ` * a hooked property is BACKED iff any of its OWN hook bodies references it by` |
|         - |  9364 | ` * name through $this — otherwise it is VIRTUAL: no backing store, no default` |
|         - |  9365 | ` * allowed, excluded from the raw object surfaces.` |
|         - |  9366 | ` */` |
|        94 |  9367 | `static int GenStateHookBodyRefsProp(SyToken *pStart,SyToken *pEnd,const SyString *pName)` |
|         1 |  9368 | `{` |
|         - |  9369 | `	SyToken *p;` |
|       345 |  9370 | `	for( p = pStart ; p + 1 < pEnd ; p++ ){` |
|       303 |  9371 | `		if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|       223 |  9372 | `			continue;` |
|         - |  9373 | `		}` |
|         - |  9374 | ``		/* `$this->NAME` (also `?->`/`::`) */`` |
|        80 |  9375 | `		if( p + 3 < pEnd` |
|        80 |  9376 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        80 |  9377 | `		 && p[1].sData.nByte == sizeof("this")-1` |
|        73 |  9378 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0` |
|        66 |  9379 | `		 && GenStateTokenIsMemberOp(&p[2])` |
|        66 |  9380 | `		 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        66 |  9381 | `		 && p[3].sData.nByte == pName->nByte` |
|        60 |  9382 | `		 && SyMemcmp((const void *)p[3].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        51 |  9383 | `			return 1;` |
|         - |  9384 | `		}` |
|         - |  9385 | ``		/* `parent::$NAME` (the parent::$x::get() hook-call form): the parent`` |
|         - |  9386 | `		 * hook operates on the shared per-instance backing store, so the` |
|         - |  9387 | `		 * property is backed (php compiles a default alongside it). */` |
|        30 |  9388 | `		if( p > pStart` |
|        26 |  9389 | `		 && GenStateTokenIsMemberOp(&p[-1])` |
|        12 |  9390 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         2 |  9391 | `		 && p[1].sData.nByte == pName->nByte` |
|         3 |  9392 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|         3 |  9393 | `			return 1;` |
|         - |  9394 | `		}` |
|        15 |  9395 | `	}` |
|        43 |  9396 | `	return 0;` |
|        48 |  9397 | `}` |
|         - |  9398 | `/*` |
|         - |  9399 | ` * True when p opens php 8.4's parent-hook call form` |
|         - |  9400 | `` * `parent :: $ NAME :: get\|set (` (7 tokens through the '(').`` |
|         - |  9401 | ` */` |
|       990 |  9402 | `static int GenStateIsParentHookCallAt(SyToken *p,SyToken *pEnd)` |
|         1 |  9403 | `{` |
|      1167 |  9404 | `	return p + 6 < pEnd` |
|       671 |  9405 | `	 && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       250 |  9406 | `	 && p->sData.nByte == sizeof("parent")-1` |
|        81 |  9407 | `	 && SyMemcmp((const void *)p->sData.zString,(const void *)"parent",sizeof("parent")-1) == 0` |
|        11 |  9408 | `	 && GenStateTokenIsMemberOp(&p[1])` |
|         8 |  9409 | `	 && (p[2].nType & PH7_TK_DOLLAR) != 0` |
|         8 |  9410 | `	 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 |  9411 | `	 && GenStateTokenIsMemberOp(&p[4])` |
|         8 |  9412 | `	 && (p[5].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 |  9413 | `	 && p[5].sData.nByte == 3` |
|         8 |  9414 | `	 && (SyMemcmp((const void *)p[5].sData.zString,(const void *)"get",3) == 0` |
|         6 |  9415 | `	  \|\| SyMemcmp((const void *)p[5].sData.zString,(const void *)"set",3) == 0)` |
|      1166 |  9416 | `	 && (p[6].nType & PH7_TK_LPAREN) != 0;` |
|         1 |  9417 | `}` |
|         - |  9418 | `/*` |
|         - |  9419 | `` * Rewrite php 8.4 `parent::$x::get(...)` / `parent::$x::set(...)` calls in a`` |
|         - |  9420 | ` * hook body into calls of the parent class's synthesized hook method` |
|         - |  9421 | `` * (`parent::__phl_hook_get_x(...)`). Builds a token COPY into pCopy (only`` |
|         - |  9422 | ` * called when GenStateIsParentHookCallAt matched somewhere in the range);` |
|         - |  9423 | ` * copied tokens keep pointing at source-owned lexeme storage, and the` |
|         - |  9424 | ` * synthesized method-name lexemes are VM-allocator owned. Returns SXRET_OK` |
|         - |  9425 | ` * or SXERR_MEM.` |
|         - |  9426 | ` */` |
|         4 |  9427 | `static sxi32 GenStateRewriteParentHookCalls(ph7_gen_state *pGen,SySet *pCopy,` |
|         - |  9428 | `	SyToken *pStart,SyToken *pEnd)` |
|         1 |  9429 | `{` |
|         5 |  9430 | `	SyToken *p = pStart;` |
|        35 |  9431 | `	while( p < pEnd ){` |
|        31 |  9432 | `		if( GenStateIsParentHookCallAt(p,pEnd) ){` |
|         - |  9433 | `			SyToken sTok;` |
|         - |  9434 | `			char zName[384];` |
|         - |  9435 | `			sxu32 nName;` |
|         - |  9436 | `			char *zDup;` |
|         - |  9437 | ``			/* `parent` `::` */`` |
|         5 |  9438 | `			SySetPut(pCopy,(const void *)&p[0]);` |
|         5 |  9439 | `			SySetPut(pCopy,(const void *)&p[1]);` |
|         7 |  9440 | `			nName = SyBufferFormat(zName,sizeof(zName),"__phl_hook_%.3s_%.*s",` |
|         4 |  9441 | `				p[5].sData.zString,(int)p[3].sData.nByte,p[3].sData.zString);` |
|         5 |  9442 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|         5 |  9443 | `			if( zDup == 0 ){` |
|       ! 0 |  9444 | `				return SXERR_MEM;` |
|         - |  9445 | `			}` |
|         5 |  9446 | `			sTok = p[3]; /* keep the line info of the property name */` |
|         5 |  9447 | `			sTok.nType = PH7_TK_ID;` |
|         5 |  9448 | `			SyStringInitFromBuf(&sTok.sData,zDup,nName);` |
|         5 |  9449 | `			sTok.pUserData = 0;` |
|         5 |  9450 | `			SySetPut(pCopy,(const void *)&sTok);` |
|         5 |  9451 | `			p += 6; /* continue at the '(' — arguments copy through unchanged */` |
|         5 |  9452 | `			continue;` |
|         - |  9453 | `		}` |
|        27 |  9454 | `		SySetPut(pCopy,(const void *)p);` |
|        27 |  9455 | `		p++;` |
|         1 |  9456 | `	}` |
|         5 |  9457 | `	return SXRET_OK;` |
|         3 |  9458 | `}` |
|        94 |  9459 | `static sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)` |
|         1 |  9460 | `{` |
|        95 |  9461 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  9462 | `	sxi32 rc;` |
|        95 |  9463 | `	int bRefsSelf = 0;` |
|        95 |  9464 | `	pGen->pIn++; /* Jump '{' */` |
|       253 |  9465 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|         - |  9466 | `		char zHook[384];` |
|         - |  9467 | `		SyString sHookName;` |
|         - |  9468 | `		ph7_class_method *pMeth;` |
|         - |  9469 | `		int bGet;` |
|       159 |  9470 | `		sxu32 nHLine = pGen->pIn->nLine;` |
|       159 |  9471 | `		if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        15 |  9472 | `			pGen->pIn++; /* stray ';' between hooks */` |
|        22 |  9473 | `			continue;` |
|         - |  9474 | `		}` |
|       145 |  9475 | `		if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - |  9476 | `			/* by-reference get hook: not modeled (loud, recorded) */` |
|       ! 0 |  9477 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9478 | `				"By-reference property hooks are not supported for %z::$%z",` |
|       ! 0 |  9479 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9480 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9481 | `				return SXERR_ABORT;` |
|         - |  9482 | `			}` |
|       ! 0 |  9483 | `			return SXERR_CORRUPT;` |
|         - |  9484 | `		}` |
|       145 |  9485 | `		if( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  9486 | `			goto HookSyntax;` |
|         - |  9487 | `		}` |
|       144 |  9488 | `		if( pGen->pIn->sData.nByte == 3` |
|       145 |  9489 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"get",3) == 0 ){` |
|        79 |  9490 | `			bGet = 1;` |
|       106 |  9491 | `		}else if( pGen->pIn->sData.nByte == 3` |
|        67 |  9492 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"set",3) == 0 ){` |
|        67 |  9493 | `			bGet = 0;` |
|        34 |  9494 | `		}else{` |
|       ! 0 |  9495 | `			goto HookSyntax;` |
|         - |  9496 | `		}` |
|       145 |  9497 | `		pGen->pIn++; /* Jump 'get'/'set' */` |
|       145 |  9498 | `		sHookName.zString = zHook;` |
|       217 |  9499 | `		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",` |
|        72 |  9500 | `			bGet ? "get" : "set",&pAttr->sName);` |
|       145 |  9501 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB)) ){` |
|         - |  9502 | ``			/* Bare `get;` / `set;` — an ABSTRACT hook declaration (php 8.4):`` |
|         - |  9503 | ``			 * legal only on an `abstract` property or inside an interface. The`` |
|         - |  9504 | `			 * synthesized method carries PH7_CLASS_ATTR_ABSTRACT and rides the` |
|         - |  9505 | `			 * existing must-implement machinery; a concrete hook override (or a` |
|         - |  9506 | `			 * plain property, see GenStateCheckAbstractMethods) satisfies it. */` |
|        14 |  9507 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|         8 |  9508 | `			 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 |  9509 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9510 | `					"Non-abstract property hook must have a body");` |
|       ! 0 |  9511 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9512 | `					return SXERR_ABORT;` |
|         - |  9513 | `				}` |
|       ! 0 |  9514 | `				return SXERR_CORRUPT;` |
|         - |  9515 | `			}` |
|        15 |  9516 | `			pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - |  9517 | `				PH7_CLASS_PROT_PUBLIC,PH7_CLASS_ATTR_ABSTRACT,0);` |
|        15 |  9518 | `			if( pMeth == 0 ){` |
|       ! 0 |  9519 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9520 | `				return SXERR_ABORT;` |
|         - |  9521 | `			}` |
|        15 |  9522 | `			pMeth->sFunc.nLine = nHLine;` |
|        15 |  9523 | `			if( !bGet ){` |
|         - |  9524 | ``				/* The implicit `$value` formal keeps the stub's signature`` |
|         - |  9525 | `				 * compatible with concrete set-hook implementations (which` |
|         - |  9526 | `				 * always carry one parameter). It takes the PROPERTY's declared` |
|         - |  9527 | `				 * type (php: the abstract set's parameter type IS the property` |
|         - |  9528 | `				 * type), so the override contravariance check accepts a typed` |
|         - |  9529 | ``				 * `set(int $v)` implementation on an `int $x` requirement. */`` |
|         - |  9530 | `				ph7_vm_func_arg sVArg;` |
|         7 |  9531 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|         7 |  9532 | `				if( zVName == 0 ){` |
|       ! 0 |  9533 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9534 | `					return SXERR_ABORT;` |
|         - |  9535 | `				}` |
|         7 |  9536 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|         7 |  9537 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|         7 |  9538 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         7 |  9539 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         7 |  9540 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|         7 |  9541 | `				sVArg.nType = pAttr->nType;` |
|         7 |  9542 | `				sVArg.sClass = pAttr->sClass;` |
|         7 |  9543 | `				sVArg.sTypeName = pAttr->sTypeName;` |
|         7 |  9544 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       ! 0 |  9545 | `					sVArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|       ! 0 |  9546 | `				}` |
|         7 |  9547 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|         3 |  9548 | `			}` |
|        15 |  9549 | `			rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|        15 |  9550 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9551 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9552 | `				return SXERR_ABORT;` |
|         - |  9553 | `			}` |
|        15 |  9554 | `			pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|        15 |  9555 | `			continue; /* the loop consumes the ';' as a stray separator */` |
|         - |  9556 | `		}` |
|       130 |  9557 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|       131 |  9558 | `		 \|\| (pClass->iFlags & PH7_CLASS_INTERFACE) != 0 ){` |
|         - |  9559 | `			/* php: an abstract/interface property hook cannot carry a body */` |
|       ! 0 |  9560 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9561 | `				"Abstract property hook cannot have body");` |
|       ! 0 |  9562 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9563 | `				return SXERR_ABORT;` |
|         - |  9564 | `			}` |
|       ! 0 |  9565 | `			return SXERR_CORRUPT;` |
|         - |  9566 | `		}` |
|       131 |  9567 | `		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - |  9568 | `			PH7_CLASS_PROT_PUBLIC,0,0);` |
|       131 |  9569 | `		if( pMeth == 0 ){` |
|       ! 0 |  9570 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9571 | `			return SXERR_ABORT;` |
|         - |  9572 | `		}` |
|       131 |  9573 | `		pMeth->sFunc.nLine = nHLine;` |
|       131 |  9574 | `		if( !bGet ){` |
|         - |  9575 | ``			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */`` |
|        61 |  9576 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        17 |  9577 | `				SyToken *pRp = 0;` |
|        17 |  9578 | `				pGen->pIn++;` |
|        17 |  9579 | `				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|        17 |  9580 | `				if( pRp >= pGen->pEnd ){` |
|       ! 0 |  9581 | `					goto HookSyntax;` |
|         - |  9582 | `				}` |
|        17 |  9583 | `				if( pGen->pIn < pRp ){` |
|        17 |  9584 | `					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);` |
|        17 |  9585 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9586 | `						return SXERR_ABORT;` |
|         - |  9587 | `					}` |
|         8 |  9588 | `				}` |
|        17 |  9589 | `				pGen->pIn = &pRp[1];` |
|         8 |  9590 | `			}` |
|        61 |  9591 | `			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){` |
|         - |  9592 | `				/* Implicit $value formal */` |
|         - |  9593 | `				ph7_vm_func_arg sVArg;` |
|        45 |  9594 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|        45 |  9595 | `				if( zVName == 0 ){` |
|       ! 0 |  9596 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9597 | `					return SXERR_ABORT;` |
|         - |  9598 | `				}` |
|        45 |  9599 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|        45 |  9600 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|        45 |  9601 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        45 |  9602 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        45 |  9603 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|        45 |  9604 | `				SyStringInitFromBuf(&sVArg.sTypeName,0,0);` |
|        45 |  9605 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|        22 |  9606 | `			}` |
|        30 |  9607 | `		}` |
|       165 |  9608 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - |  9609 | `			/* Block body */` |
|        69 |  9610 | `			SyToken *pBodyStart = pGen->pIn;` |
|        69 |  9611 | `			SyToken *pCloser = 0;` |
|        69 |  9612 | `			int bParentCall = 0;` |
|        69 |  9613 | `			PH7_DelimitNestedTokens(&pBodyStart[1],pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);` |
|        69 |  9614 | `			if( pCloser < pGen->pEnd ){` |
|         - |  9615 | `				SyToken *pScan;` |
|       753 |  9616 | `				for( pScan = &pBodyStart[1] ; pScan < pCloser ; pScan++ ){` |
|       687 |  9617 | `					if( GenStateIsParentHookCallAt(pScan,pCloser) ){` |
|         3 |  9618 | `						bParentCall = 1;` |
|         3 |  9619 | `						break;` |
|         - |  9620 | `					}` |
|       343 |  9621 | `				}` |
|        34 |  9622 | `			}` |
|        69 |  9623 | `			if( bParentCall ){` |
|         - |  9624 | ``				/* `parent::$x::get()` inside the body: compile a REWRITTEN copy`` |
|         - |  9625 | `				 * of the body tokens (the call becomes the parent's synthesized` |
|         - |  9626 | `				 * hook method), then continue past the original body. */` |
|         - |  9627 | `				SySet sBody;` |
|         3 |  9628 | `				SyToken *pSavedEnd = pGen->pEnd;` |
|         3 |  9629 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 |  9630 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,&pCloser[1]);` |
|         3 |  9631 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9632 | `					SySetRelease(&sBody);` |
|       ! 0 |  9633 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9634 | `					return SXERR_ABORT;` |
|         - |  9635 | `				}` |
|         3 |  9636 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 |  9637 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         3 |  9638 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|         3 |  9639 | `				pGen->pIn = &pCloser[1];` |
|         3 |  9640 | `				pGen->pEnd = pSavedEnd;` |
|         3 |  9641 | `				SySetRelease(&sBody);` |
|         3 |  9642 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9643 | `					return SXERR_ABORT;` |
|         - |  9644 | `				}` |
|         3 |  9645 | `				pMeth->sFunc.nEndLine = pCloser->nLine;` |
|         2 |  9646 | `			}else{` |
|        67 |  9647 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|        67 |  9648 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9649 | `					return SXERR_ABORT;` |
|         - |  9650 | `				}` |
|        67 |  9651 | `				pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|         - |  9652 | `			}` |
|        69 |  9653 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        17 |  9654 | `				bRefsSelf = 1;` |
|         9 |  9655 | `			}` |
|       128 |  9656 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){` |
|         - |  9657 | ``			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */`` |
|         - |  9658 | `			GenBlock *pBlock;` |
|         - |  9659 | `			SySet *pInstrContainer;` |
|         - |  9660 | `			SyToken *pBodyStart;` |
|         - |  9661 | `			SyToken *pExprEnd;` |
|        63 |  9662 | `			SyToken *pSavedEnd = 0;` |
|         - |  9663 | `			SySet sBody;` |
|        63 |  9664 | `			int bParentCall = 0;` |
|        63 |  9665 | `			pGen->pIn++; /* Jump '=>' */` |
|        63 |  9666 | `			pBodyStart = pGen->pIn;` |
|         - |  9667 | `			/* Delimit the expression (first top-level ';', or a closer that` |
|         - |  9668 | `			 * would end the enclosing hook list) and rewrite any` |
|         - |  9669 | ``			 * `parent::$x::get()` calls into the parent's synthesized hook`` |
|         - |  9670 | `			 * method on a token copy. */` |
|         - |  9671 | `			{` |
|        63 |  9672 | `				sxi32 iNest = 0;` |
|        63 |  9673 | `				pExprEnd = pBodyStart;` |
|       355 |  9674 | `				while( pExprEnd < pGen->pEnd ){` |
|       355 |  9675 | `					if( pExprEnd->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         9 |  9676 | `						iNest++;` |
|       351 |  9677 | `					}else if( pExprEnd->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         9 |  9678 | `						if( iNest <= 0 ){` |
|       ! 0 |  9679 | `							break;` |
|         - |  9680 | `						}` |
|         9 |  9681 | `						iNest--;` |
|       343 |  9682 | `					}else if( iNest <= 0 && (pExprEnd->nType & PH7_TK_SEMI) ){` |
|        63 |  9683 | `						break;` |
|         - |  9684 | `					}` |
|       293 |  9685 | `					pExprEnd++;` |
|         1 |  9686 | `				}` |
|         - |  9687 | `			}` |
|         - |  9688 | `			{` |
|         - |  9689 | `				SyToken *pScan;` |
|       335 |  9690 | `				for( pScan = pBodyStart ; pScan < pExprEnd ; pScan++ ){` |
|       275 |  9691 | `					if( GenStateIsParentHookCallAt(pScan,pExprEnd) ){` |
|         3 |  9692 | `						bParentCall = 1;` |
|         3 |  9693 | `						break;` |
|         - |  9694 | `					}` |
|       137 |  9695 | `				}` |
|         - |  9696 | `			}` |
|        63 |  9697 | `			if( bParentCall ){` |
|         3 |  9698 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 |  9699 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,pExprEnd);` |
|         3 |  9700 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9701 | `					SySetRelease(&sBody);` |
|       ! 0 |  9702 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9703 | `					return SXERR_ABORT;` |
|         - |  9704 | `				}` |
|         3 |  9705 | `				pSavedEnd = pGen->pEnd;` |
|         3 |  9706 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 |  9707 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         1 |  9708 | `			}` |
|        94 |  9709 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|        62 |  9710 | `				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);` |
|        63 |  9711 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9712 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");` |
|       ! 0 |  9713 | `				return SXERR_ABORT;` |
|         - |  9714 | `			}` |
|        63 |  9715 | `			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        63 |  9716 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|        63 |  9717 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|        63 |  9718 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        63 |  9719 | `			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        63 |  9720 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        63 |  9721 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        63 |  9722 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        63 |  9723 | `			if( bParentCall ){` |
|         3 |  9724 | `				pGen->pIn = pExprEnd; /* land on the original ';' */` |
|         3 |  9725 | `				pGen->pEnd = pSavedEnd;` |
|         3 |  9726 | `				SySetRelease(&sBody);` |
|         1 |  9727 | `			}` |
|        63 |  9728 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9729 | `				return SXERR_ABORT;` |
|         - |  9730 | `			}` |
|        63 |  9731 | `			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;` |
|        63 |  9732 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        37 |  9733 | `				bRefsSelf = 1;` |
|        18 |  9734 | `			}` |
|        63 |  9735 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        63 |  9736 | `				pGen->pIn++; /* Jump ';' */` |
|        31 |  9737 | `			}` |
|        63 |  9738 | `			if( !bGet ){` |
|         - |  9739 | ``				/* `set => expr` assigns the expression to the backing store:`` |
|         - |  9740 | `				 * the dispatcher consumes the implicit return value — which` |
|         - |  9741 | `				 * also makes the property BACKED (php: the shorthand is sugar` |
|         - |  9742 | ``				 * for `$this->NAME = expr`). */`` |
|         3 |  9743 | `				pMeth->sFunc.iFlags \|= VM_FUNC_HOOK_SET_EXPR;` |
|         3 |  9744 | `				bRefsSelf = 1;` |
|         1 |  9745 | `			}` |
|        32 |  9746 | `		}else{` |
|       ! 0 |  9747 | `			goto HookSyntax;` |
|         - |  9748 | `		}` |
|       131 |  9749 | `		rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|       131 |  9750 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  9751 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9752 | `			return SXERR_ABORT;` |
|         - |  9753 | `		}` |
|       131 |  9754 | `		pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|         1 |  9755 | `	}` |
|        95 |  9756 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|       ! 0 |  9757 | `		goto HookSyntax;` |
|         - |  9758 | `	}` |
|        95 |  9759 | `	pGen->pIn++; /* Jump '}' */` |
|        95 |  9760 | `	if( !bRefsSelf ){` |
|         - |  9761 | ``		/* php 8.4 virtual-vs-backed: no hook body referenced `$this->NAME`, so`` |
|         - |  9762 | `		 * this property is VIRTUAL — php gives it no backing store and forbids` |
|         - |  9763 | `		 * a default value (compile fatal, php's exact wording). */` |
|        41 |  9764 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_HOOK_VIRTUAL;` |
|        41 |  9765 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|       ! 0 |  9766 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9767 | `				"Cannot specify default value for virtual hooked property %z::$%z",` |
|       ! 0 |  9768 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9769 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9770 | `				return SXERR_ABORT;` |
|         - |  9771 | `			}` |
|       ! 0 |  9772 | `			return SXERR_CORRUPT;` |
|         - |  9773 | `		}` |
|        20 |  9774 | `	}` |
|        95 |  9775 | `	return SXRET_OK;` |
|       ! 0 |  9776 | `HookSyntax:` |
|       ! 0 |  9777 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9778 | `		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",` |
|       ! 0 |  9779 | `		&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9780 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  9781 | `		return SXERR_ABORT;` |
|         - |  9782 | `	}` |
|       ! 0 |  9783 | `	return SXERR_CORRUPT;` |
|        48 |  9784 | `}` |
|         - |  9785 | `/*` |
|         - |  9786 | ` * Compile an object interface.` |
|         - |  9787 | ` *  According to the PHP language reference manual` |
|         - |  9788 | ` *   Object Interfaces:` |
|         - |  9789 | ` *   Object interfaces allow you to create code which specifies which methods` |
|         - |  9790 | ` *   a class must implement, without having to define how these methods are handled.` |
|         - |  9791 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|         - |  9792 | ` *   class, but without any of the methods having their contents defined.` |
|         - |  9793 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|         - |  9794 | ` */` |
|     70104 |  9795 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|         5 |  9796 | `{` |
|     70109 |  9797 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  9798 | `	ph7_class *pClass,*pBase;` |
|         - |  9799 | `	SyToken *pEnd,*pTmp;` |
|         - |  9800 | `	SyString *pName;` |
|         - |  9801 | `	sxi32 nKwrd;` |
|         - |  9802 | `	sxi32 rc;` |
|         - |  9803 | `	/* Jump the 'interface' keyword */` |
|     70109 |  9804 | `	pGen->pIn++;` |
|         - |  9805 | `	/* Extract interface name */` |
|     70109 |  9806 | `	pName = &pGen->pIn->sData;` |
|         - |  9807 | `	/* Advance the stream cursor */` |
|     70109 |  9808 | `	pGen->pIn++;` |
|         - |  9809 | `	/* Build FQN and obtain a raw class */ {` |
|         - |  9810 | `		SyBlob sFQN;` |
|         - |  9811 | `		SyString sFQNStr;` |
|     70109 |  9812 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     70109 |  9813 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     70109 |  9814 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|     70109 |  9815 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     70109 |  9816 | `		SyBlobRelease(&sFQN);` |
|         - |  9817 | `	}` |
|     70109 |  9818 | `	if( pClass == 0 ){` |
|       ! 0 |  9819 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9820 | `		return SXERR_ABORT;` |
|         - |  9821 | `	}` |
|     70109 |  9822 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     70109 |  9823 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  9824 | `		return SXERR_ABORT;` |
|         - |  9825 | `	}` |
|         - |  9826 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|     70109 |  9827 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|         - |  9828 | `	/* Assume no base class is given */` |
|     70109 |  9829 | `	pBase = 0;` |
|     70109 |  9830 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     27233 |  9831 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     27233 |  9832 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a, c, … */ ){` |
|         - |  9833 | `			/* php lets an interface extend SEVERAL parent interfaces. The first` |
|         - |  9834 | `			 * becomes pBase (single-inheritance chain, hDerived); every extra one` |
|         - |  9835 | `			 * is recorded via PH7_ClassImplement so it lands in aInterface — the` |
|         - |  9836 | `			 * runtime subtype walk (VmInterfaceReaches) follows both. */` |
|     27233 |  9837 | `			pGen->pIn++;` |
|     13615 |  9838 | `			for(;;){` |
|         - |  9839 | `				SyBlob sResolved;` |
|         - |  9840 | `				SyString sBaseName;` |
|         - |  9841 | `				sxu32 nRefLine;` |
|         - |  9842 | `				ph7_class *pParent;` |
|     27235 |  9843 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|     27235 |  9844 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     27235 |  9845 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 |  9846 | `					SyBlobRelease(&sResolved);` |
|       ! 0 |  9847 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9848 | `						"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|       ! 0 |  9849 | `						pName);` |
|       ! 0 |  9850 | `					SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9851 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9852 | `						return SXERR_ABORT;` |
|         - |  9853 | `					}` |
|       ! 0 |  9854 | `					return SXRET_OK;` |
|         - |  9855 | `				}` |
|     40850 |  9856 | `				pParent = PH7_VmExtractClass(pGen->pVm,` |
|     27230 |  9857 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     27235 |  9858 | `				SyStringInitFromBuf(&sBaseName,` |
|         - |  9859 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - |  9860 | `				/* Only interfaces is allowed */` |
|     27235 |  9861 | `				while( pParent && (pParent->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 |  9862 | `					pParent = pParent->pNextName;` |
|       ! 0 |  9863 | `				}` |
|     27235 |  9864 | `				if( pParent == 0 ){` |
|       ! 0 |  9865 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - |  9866 | `						"Nonexistent base interface '%z'",&sBaseName);` |
|       ! 0 |  9867 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9868 | `						SyBlobRelease(&sResolved);` |
|       ! 0 |  9869 | `						return SXERR_ABORT;` |
|       ! 0 |  9870 | `					}` |
|     27235 |  9871 | `				}else if( pBase == 0 ){` |
|         - |  9872 | `					/* First parent → single-inheritance base */` |
|     27233 |  9873 | `					pBase = pParent;` |
|     13619 |  9874 | `				}else{` |
|         - |  9875 | `					/* Additional parent → record it in aInterface (+ copy its` |
|         - |  9876 | `					 * constants/method stubs) so instanceof reaches it too. */` |
|         3 |  9877 | `					PH7_ClassImplement(pClass,pParent);` |
|         - |  9878 | `				}` |
|     27235 |  9879 | `				SyBlobRelease(&sResolved);` |
|         - |  9880 | `				/* Continue on a comma-separated list */` |
|     27235 |  9881 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|         3 |  9882 | `					pGen->pIn++;` |
|         3 |  9883 | `					continue;` |
|         - |  9884 | `				}` |
|     27233 |  9885 | `				break;` |
|       ! 0 |  9886 | `			}` |
|     13614 |  9887 | `		}` |
|     13614 |  9888 | `	}` |
|     70109 |  9889 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - |  9890 | `		/* Syntax error */` |
|       ! 0 |  9891 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|       ! 0 |  9892 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9893 | `		if( rc == SXERR_ABORT ){` |
|         - |  9894 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9895 | `			return SXERR_ABORT;` |
|         - |  9896 | `		}` |
|       ! 0 |  9897 | `		return SXRET_OK;` |
|         - |  9898 | `	}` |
|     70109 |  9899 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     70109 |  9900 | `	pEnd = 0; /* cc warning */` |
|         - |  9901 | `	/* Delimit the interface body */` |
|     70109 |  9902 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|     70109 |  9903 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  9904 | `		/* Syntax error */` |
|       ! 0 |  9905 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|       ! 0 |  9906 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9907 | `		if( rc == SXERR_ABORT ){` |
|         - |  9908 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9909 | `			return SXERR_ABORT;` |
|         - |  9910 | `		}` |
|       ! 0 |  9911 | `		return SXRET_OK;` |
|         - |  9912 | `	}` |
|         - |  9913 | `	/* The delimiter token is the interface body's closing brace */` |
|     70109 |  9914 | `	pClass->nEndLine = pEnd->nLine;` |
|         - |  9915 | `	/* Swap token stream */` |
|     70109 |  9916 | `	pTmp = pGen->pEnd;` |
|     70109 |  9917 | `	pGen->pEnd = pEnd;` |
|         - |  9918 | `	/* Start the parse process` |
|         - |  9919 | `	 * Note (According to the PHP reference manual):` |
|         - |  9920 | `	 *  Only constants and function signatures(without body) are allowed.` |
|         - |  9921 | `	 *  Only 'public' visibility is allowed.` |
|         - |  9922 | `	 */` |
|    128397 |  9923 | `	for(;;){` |
|         - |  9924 | `		/* Jump leading/trailing semi-colons */` |
|    443493 |  9925 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    186695 |  9926 | `			pGen->pIn++;` |
|         5 |  9927 | `		}` |
|    256803 |  9928 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - |  9929 | `			/* End of interface body */` |
|     70105 |  9930 | `			break;` |
|         - |  9931 | `		}` |
|         - |  9932 | `		/* Bind a directly-preceding docblock to this member */` |
|    186703 |  9933 | `		GenStateSetPendingDoc(&(*pGen));` |
|    186703 |  9934 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 |  9935 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  9936 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|       ! 0 |  9937 | `				&pGen->pIn->sData,pName);` |
|       ! 0 |  9938 | `			if( rc == SXERR_ABORT ){` |
|         - |  9939 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  9940 | `				return SXERR_ABORT;` |
|         - |  9941 | `			}` |
|       ! 0 |  9942 | `			goto done;` |
|         - |  9943 | `		}` |
|         - |  9944 | `		/* Extract the current keyword */` |
|    186703 |  9945 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    186703 |  9946 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - |  9947 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|         - |  9948 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|         3 |  9949 | `			const char *zKind = "member";` |
|         3 |  9950 | `			SyString *pMemberName = 0;` |
|         3 |  9951 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|         3 |  9952 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|         3 |  9953 | `				if( nNext == PH7_TKWRD_CONST ){` |
|         3 |  9954 | `					zKind = "constant";` |
|         3 |  9955 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|         3 |  9956 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|         2 |  9957 | `					}` |
|         1 |  9958 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|       ! 0 |  9959 | `					zKind = "method";` |
|       ! 0 |  9960 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       ! 0 |  9961 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       ! 0 |  9962 | `					}` |
|       ! 0 |  9963 | `				}` |
|         1 |  9964 | `			}` |
|         3 |  9965 | `			if( pMemberName ){` |
|         4 |  9966 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         1 |  9967 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|         2 |  9968 | `			}else{` |
|       ! 0 |  9969 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9970 | `					"Access type for interface %s must be public",zKind);` |
|         - |  9971 | `			}` |
|         3 |  9972 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9973 | `				return SXERR_ABORT;` |
|         - |  9974 | `			}` |
|         3 |  9975 | `			goto done;` |
|         - |  9976 | `		}` |
|    186701 |  9977 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|       ! 0 |  9978 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9979 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 |  9980 | `			if( rc == SXERR_ABORT ){` |
|         - |  9981 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  9982 | `				return SXERR_ABORT;` |
|         - |  9983 | `			}` |
|       ! 0 |  9984 | `			goto done;` |
|         - |  9985 | `		}` |
|    186701 |  9986 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|         - |  9987 | `			/* Advance the stream cursor */` |
|    132251 |  9988 | `			pGen->pIn++;` |
|    132246 |  9989 | `			if( pGen->pIn < pGen->pEnd` |
|    132251 |  9990 | `			 && ((pGen->pIn->nType & PH7_TK_DOLLAR) != 0` |
|    132246 |  9991 | `			  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         - |  9992 | ``				/* PHP 8.4: `public [?T] $x { get; set; }` — a hooked-property`` |
|         - |  9993 | `				 * requirement. The attribute compiler + hook parser handle it` |
|         - |  9994 | `				 * (bare hooks are implicitly abstract inside an interface; a` |
|         - |  9995 | `				 * property without hooks is ITS "Interfaces may only include` |
|         - |  9996 | `				 * hooked properties" error). */` |
|       ! 0 |  9997 | `				rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 |  9998 | `					PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 |  9999 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 10000 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10001 | `						return SXERR_ABORT;` |
|         - | 10002 | `					}` |
|       ! 0 | 10003 | `					goto done;` |
|         - | 10004 | `				}` |
|       ! 0 | 10005 | `				continue;` |
|         - | 10006 | `			}` |
|    132251 | 10007 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|         - | 10008 | `				/* A type NAME (a plain identifier, e.g. a class type) followed by` |
|         - | 10009 | `				 * '$' also opens a hooked-property requirement. */` |
|       ! 0 | 10010 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) != 0` |
|       ! 0 | 10011 | `				 && (pGen->pIn + 1) < pGen->pEnd` |
|       ! 0 | 10012 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|       ! 0 | 10013 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 | 10014 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 | 10015 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 10016 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10017 | `							return SXERR_ABORT;` |
|         - | 10018 | `						}` |
|       ! 0 | 10019 | `						goto done;` |
|         - | 10020 | `					}` |
|       ! 0 | 10021 | `					continue;` |
|         - | 10022 | `				}` |
|       ! 0 | 10023 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10024 | `					"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 | 10025 | `				if( rc == SXERR_ABORT ){` |
|         - | 10026 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 | 10027 | `					return SXERR_ABORT;` |
|         - | 10028 | `				}` |
|       ! 0 | 10029 | `				goto done;` |
|         - | 10030 | `			}` |
|    132251 | 10031 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    132251 | 10032 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|         - | 10033 | `				/* A type KEYWORD (int/string/bool/…) followed by '$' opens a` |
|         - | 10034 | `				 * hooked-property requirement (PHP 8.4). */` |
|         4 | 10035 | `				if( (pGen->pIn + 1) < pGen->pEnd` |
|         5 | 10036 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|         7 | 10037 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|         2 | 10038 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|         5 | 10039 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 10040 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10041 | `							return SXERR_ABORT;` |
|         - | 10042 | `						}` |
|       ! 0 | 10043 | `						goto done;` |
|         - | 10044 | `					}` |
|         5 | 10045 | `					continue;` |
|         - | 10046 | `				}` |
|       ! 0 | 10047 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10048 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 | 10049 | `				if( rc == SXERR_ABORT ){` |
|         - | 10050 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 | 10051 | `					return SXERR_ABORT;` |
|         - | 10052 | `				}` |
|       ! 0 | 10053 | `				goto done;` |
|         - | 10054 | `			}` |
|     66121 | 10055 | `		}` |
|    186697 | 10056 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|         - | 10057 | `			/* Parse constant */` |
|     54451 | 10058 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|     54451 | 10059 | `			if( rc != SXRET_OK ){` |
|         3 | 10060 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10061 | `					return SXERR_ABORT;` |
|         - | 10062 | `				}` |
|         3 | 10063 | `				goto done;` |
|         - | 10064 | `			}` |
|     27227 | 10065 | `		}else{` |
|    132251 | 10066 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|    132251 | 10067 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - | 10068 | `				/* Static method,record that */` |
|     11669 | 10069 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|         - | 10070 | `				/* Advance the stream cursor */` |
|     11669 | 10071 | `				pGen->pIn++;` |
|     11664 | 10072 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     11669 | 10073 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 10074 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10075 | `							"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 | 10076 | `						if( rc == SXERR_ABORT ){` |
|         - | 10077 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 10078 | `							return SXERR_ABORT;` |
|         - | 10079 | `						}` |
|       ! 0 | 10080 | `						goto done;` |
|         - | 10081 | `				}` |
|      5832 | 10082 | `			}` |
|         - | 10083 | `			/* Process method signature (no body for interface methods) */` |
|    132251 | 10084 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|    132251 | 10085 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 10086 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10087 | `					return SXERR_ABORT;` |
|         - | 10088 | `				}` |
|       ! 0 | 10089 | `				goto done;` |
|         - | 10090 | `			}` |
|         - | 10091 | `		}` |
|         5 | 10092 | `	}` |
|         - | 10093 | `	/* Install the interface */` |
|     70105 | 10094 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     70105 | 10095 | `	if( rc == SXRET_OK && pBase ){` |
|         - | 10096 | `		/* Inherit from the base interface */` |
|     27233 | 10097 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|     13614 | 10098 | `	}` |
|     70105 | 10099 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 10100 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10101 | `		return SXERR_ABORT;` |
|         - | 10102 | `	}` |
|     35050 | 10103 | `done:` |
|         - | 10104 | `	/* Point beyond the interface body */` |
|     70109 | 10105 | `	pGen->pIn  = &pEnd[1];` |
|     70109 | 10106 | `	pGen->pEnd = pTmp;` |
|     70109 | 10107 | `	return PH7_OK;` |
|     35057 | 10108 | `}` |
|         - | 10109 | `/*` |
|         - | 10110 | ` * Compile a user-defined class.` |
|         - | 10111 | ` * According to the PHP language reference manual` |
|         - | 10112 | ` *  class` |
|         - | 10113 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|         - | 10114 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|         - | 10115 | ` *  of the properties and methods belonging to the class.` |
|         - | 10116 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|         - | 10117 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|         - | 10118 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|         - | 10119 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - | 10120 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|         - | 10121 | ` *  (called "methods").` |
|         - | 10122 | ` */` |
|         - | 10123 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|         - | 10124 | `typedef struct TraitUseEntry TraitUseEntry;` |
|         - | 10125 | `struct TraitUseEntry {` |
|         - | 10126 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|         - | 10127 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|         - | 10128 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|         - | 10129 | `};` |
|         - | 10130 | `/*` |
|         - | 10131 | ` * Validate that methods implementing interface contracts have compatible` |
|         - | 10132 | ` * signatures: public visibility and at least as many parameters as declared.` |
|         - | 10133 | ` */` |
|    386796 | 10134 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10135 | `{` |
|         - | 10136 | `	ph7_class **apIface;` |
|         - | 10137 | `	sxu32 nIface,i;` |
|         - | 10138 | `	sxi32 rc;` |
|    386801 | 10139 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|       ! 0 | 10140 | `		return SXRET_OK;` |
|         - | 10141 | `	}` |
|    386801 | 10142 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    386801 | 10143 | `	nIface = SySetUsed(&pClass->aInterface);` |
|    783727 | 10144 | `	for(i = 0; i < nIface; i++){` |
|    396931 | 10145 | `		ph7_class *pIface = apIface[i];` |
|         - | 10146 | `		SyHashEntry *pEntry;` |
|    396931 | 10147 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   1179021 | 10148 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|    782095 | 10149 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|         - | 10150 | `			ph7_class_method *pImplMeth;` |
|    782095 | 10151 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|         - | 10152 | `			/* Find the implementing method in the class */` |
|    782095 | 10153 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|    782095 | 10154 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        23 | 10155 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|         - | 10156 | `			}` |
|         - | 10157 | `			/* Check visibility: interface methods must be implemented as public */` |
|    782077 | 10158 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|         4 | 10159 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - | 10160 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|         1 | 10161 | `					&pClass->sName,pMName,&pIface->sName);` |
|         3 | 10162 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10163 | `					return SXERR_ABORT;` |
|         - | 10164 | `				}` |
|         1 | 10165 | `			}` |
|         - | 10166 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|         - | 10167 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|         - | 10168 | `			 */` |
|         - | 10169 | `			{` |
|    782077 | 10170 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|    782077 | 10171 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|    782077 | 10172 | `				int sigError = 0;` |
|    782077 | 10173 | `				if( nImplArgs < nIfaceArgs ){` |
|         3 | 10174 | `					sigError = 1;` |
|    782076 | 10175 | `				}else if( nImplArgs > nIfaceArgs ){` |
|         - | 10176 | `					/* Extra parameters must all have default values */` |
|      3897 | 10177 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|         - | 10178 | `					sxu32 k;` |
|      7787 | 10179 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|      3897 | 10180 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|         3 | 10181 | `							sigError = 1;` |
|         3 | 10182 | `							break;` |
|         - | 10183 | `						}` |
|      1950 | 10184 | `					}` |
|      1946 | 10185 | `				}` |
|    782077 | 10186 | `				if( sigError ){` |
|         - | 10187 | `					SyBlob sImplSig, sIfaceSig;` |
|         - | 10188 | `					ph7_vm_func_arg *aArgs;` |
|         - | 10189 | `					sxu32 j;` |
|         6 | 10190 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|         6 | 10191 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|         - | 10192 | `					/* Build implementing method signature */` |
|         6 | 10193 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|        12 | 10194 | `					for(j = 0; j < nImplArgs; j++){` |
|         8 | 10195 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|         8 | 10196 | `						SyBlobAppend(&sImplSig,"$",1);` |
|         8 | 10197 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 | 10198 | `					}` |
|         - | 10199 | `					/* Build interface method signature */` |
|         6 | 10200 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|        12 | 10201 | `					for(j = 0; j < nIfaceArgs; j++){` |
|         8 | 10202 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|         8 | 10203 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|         8 | 10204 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 | 10205 | `					}` |
|         8 | 10206 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - | 10207 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|         2 | 10208 | `						&pClass->sName,pMName,` |
|         4 | 10209 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|         2 | 10210 | `						&pIface->sName,pMName,` |
|         4 | 10211 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|         6 | 10212 | `					SyBlobRelease(&sImplSig);` |
|         6 | 10213 | `					SyBlobRelease(&sIfaceSig);` |
|         6 | 10214 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10215 | `						return SXERR_ABORT;` |
|         - | 10216 | `					}` |
|         2 | 10217 | `				}` |
|         - | 10218 | `			}` |
|         5 | 10219 | `		}` |
|    198468 | 10220 | `	}` |
|    386801 | 10221 | `	return SXRET_OK;` |
|    193403 | 10222 | `}` |
|         - | 10223 | `/*` |
|         - | 10224 | ` * An abstract property-hook stub (__phl_hook_{get,set}_NAME) is satisfied by` |
|         - | 10225 | ` * the class declaring a PLAIN (non-abstract, non-hooked) property NAME: php` |
|         - | 10226 | `` * lets a plain property implement `{ get; set; }` requirements — its raw`` |
|         - | 10227 | ` * read/write IS the default get/set. A concrete hook override replaced the` |
|         - | 10228 | ` * stub in hMethod already, so a surviving stub next to a HOOKED property` |
|         - | 10229 | ` * means that specific hook is still missing.` |
|         - | 10230 | ` */` |
|        38 | 10231 | `static int GenStateAbstractHookSatisfied(ph7_class *pClass,const SyString *pMName)` |
|         5 | 10232 | `{` |
|         - | 10233 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|         - | 10234 | `	ph7_class_attr *pProp;` |
|        38 | 10235 | `	if( pMName->nByte <= nPfx` |
|        27 | 10236 | `	 \|\| (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) != 0` |
|         4 | 10237 | `	  && SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) != 0) ){` |
|        36 | 10238 | `		return 0; /* not a hook stub */` |
|         - | 10239 | `	}` |
|         7 | 10240 | `	pProp = PH7_ClassExtractAttribute(pClass,&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|         7 | 10241 | `	return pProp != 0` |
|         6 | 10242 | `		&& (pProp->iFlags & (PH7_CLASS_ATTR_ABSTRACT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|         3 | 10243 | `			\|PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) == 0;` |
|        24 | 10244 | `}` |
|         - | 10245 | `/*` |
|         - | 10246 | ` * Append an abstract member's display name to the message blob, translating a` |
|         - | 10247 | `` * property-hook stub (__phl_hook_get_x) to php's `$x::get` form.`` |
|         - | 10248 | ` */` |
|        16 | 10249 | `static void GenStateAppendAbstractMemberName(SyBlob *pMsg,const SyString *pMName)` |
|         4 | 10250 | `{` |
|         - | 10251 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|        16 | 10252 | `	if( pMName->nByte > nPfx` |
|        12 | 10253 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|       ! 0 | 10254 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0) ){` |
|       ! 0 | 10255 | `		SyBlobAppend(pMsg,"$",1);` |
|       ! 0 | 10256 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|       ! 0 | 10257 | `		SyBlobAppend(pMsg,"::",2);` |
|       ! 0 | 10258 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[sizeof("__phl_hook_")-1],3);` |
|       ! 0 | 10259 | `		return;` |
|         - | 10260 | `	}` |
|        20 | 10261 | `	SyBlobAppend(pMsg,(const void *)pMName->zString,pMName->nByte);` |
|        12 | 10262 | `}` |
|         - | 10263 | `/*` |
|         - | 10264 | ` * Check that a concrete class has no remaining abstract methods.` |
|         - | 10265 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|         - | 10266 | ` */` |
|    386796 | 10267 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10268 | `{` |
|         - | 10269 | `	ph7_class_method *pMeth;` |
|         - | 10270 | `	SyHashEntry *pEntry;` |
|         - | 10271 | `	sxu32 nAbstract;` |
|         - | 10272 | `	SyBlob sMsg;` |
|         - | 10273 | `	sxi32 rc;` |
|         - | 10274 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|    386801 | 10275 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     19499 | 10276 | `		return SXRET_OK;` |
|         - | 10277 | `	}` |
|         - | 10278 | `	/* Count abstract methods */` |
|    367307 | 10279 | `	nAbstract = 0;` |
|    367307 | 10280 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   5654792 | 10281 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   5103839 | 10282 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   5103839 | 10283 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        27 | 10284 | `			if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|         7 | 10285 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - | 10286 | `			}` |
|        20 | 10287 | `			nAbstract++;` |
|         8 | 10288 | `		}` |
|         5 | 10289 | `	}` |
|    367307 | 10290 | `	if( nAbstract == 0 ){` |
|    367293 | 10291 | `		return SXRET_OK;` |
|         - | 10292 | `	}` |
|         - | 10293 | `	/* Build the error message listing all abstract methods with origins */` |
|        18 | 10294 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|        18 | 10295 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|         - | 10296 | `		"be declared abstract or implement the remaining method%s (",` |
|         7 | 10297 | `		&pClass->sName,nAbstract,` |
|         7 | 10298 | `		(nAbstract > 1 ? "s" : ""),` |
|         7 | 10299 | `		(nAbstract > 1 ? "s" : ""));` |
|         - | 10300 | `	/* Second pass: list methods with origins */` |
|         - | 10301 | `	{` |
|        18 | 10302 | `		sxu32 nListed = 0;` |
|        18 | 10303 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|        36 | 10304 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|        22 | 10305 | `			ph7_class *pOrigin = 0;` |
|         - | 10306 | `			SyString *pMName;` |
|        22 | 10307 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        22 | 10308 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|         3 | 10309 | `				continue;` |
|         - | 10310 | `			}` |
|        20 | 10311 | `			pMName = &pMeth->sFunc.sName;` |
|        20 | 10312 | `			if( GenStateAbstractHookSatisfied(pClass,pMName) ){` |
|       ! 0 | 10313 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - | 10314 | `			}` |
|        20 | 10315 | `			if( nListed > 0 ){` |
|         3 | 10316 | `				SyBlobAppend(&sMsg,", ",2);` |
|         1 | 10317 | `			}` |
|         - | 10318 | `			/* Find the origin of this abstract method.` |
|         - | 10319 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|         - | 10320 | `			 * inheritance chains) take precedence for interface-declared` |
|         - | 10321 | `			 * methods. Abstract class methods only win when the class` |
|         - | 10322 | `			 * itself declared the abstract method (not inherited from` |
|         - | 10323 | `			 * an interface). Trait methods are adopted into the using` |
|         - | 10324 | `			 * class's namespace.` |
|         - | 10325 | `			 */` |
|         - | 10326 | `			{` |
|         - | 10327 | `				ph7_class **apIface;` |
|         - | 10328 | `				ph7_class **apTrait;` |
|         - | 10329 | `				ph7_class *pWalk;` |
|         - | 10330 | `				sxu32 i;` |
|         - | 10331 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|         - | 10332 | `				 * (one that was written in the class body, not inherited from an` |
|         - | 10333 | `				 * interface). PHP attributes origin to the declaring class.` |
|         - | 10334 | `				 */` |
|        20 | 10335 | `				if( pClass->pBase ){` |
|        11 | 10336 | `					pWalk = pClass->pBase;` |
|        19 | 10337 | `					while( pWalk ){` |
|         - | 10338 | `						ph7_class_method *pParentMeth;` |
|        13 | 10339 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|        13 | 10340 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|         - | 10341 | `							/* Exclude methods that came from an interface anywhere` |
|         - | 10342 | `							 * in this class's ancestor chain.` |
|         - | 10343 | `							 */` |
|        13 | 10344 | `							int fromIface = 0;` |
|        13 | 10345 | `							ph7_class *pAnc = pWalk;` |
|        17 | 10346 | `							while( pAnc ){` |
|         - | 10347 | `								ph7_class **apPI;` |
|         - | 10348 | `								sxu32 j;` |
|        15 | 10349 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|        15 | 10350 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|        10 | 10351 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|        10 | 10352 | `										fromIface = 1;` |
|        10 | 10353 | `										break;` |
|         - | 10354 | `									}` |
|       ! 0 | 10355 | `								}` |
|        15 | 10356 | `								if( fromIface ) break;` |
|         6 | 10357 | `								pAnc = pAnc->pBase;` |
|         2 | 10358 | `							}` |
|        13 | 10359 | `							if( !fromIface ){` |
|         3 | 10360 | `								pOrigin = pWalk;` |
|         3 | 10361 | `								break;` |
|         - | 10362 | `							}` |
|         4 | 10363 | `						}` |
|        10 | 10364 | `						pWalk = pWalk->pBase;` |
|         2 | 10365 | `					}` |
|         4 | 10366 | `				}` |
|         - | 10367 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|         - | 10368 | `				 * each interface's own parent chain for the deepest origin.` |
|         - | 10369 | `				 */` |
|        20 | 10370 | `				if( !pOrigin ){` |
|        18 | 10371 | `					pWalk = pClass;` |
|        40 | 10372 | `					while( pWalk && !pOrigin ){` |
|        26 | 10373 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|        26 | 10374 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|        16 | 10375 | `							ph7_class *pIface = apIface[i];` |
|        16 | 10376 | `							ph7_class *pDeepest = 0;` |
|        28 | 10377 | `							while( pIface ){` |
|        16 | 10378 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|        16 | 10379 | `									pDeepest = pIface;` |
|         6 | 10380 | `								}` |
|        16 | 10381 | `								pIface = pIface->pBase;` |
|         4 | 10382 | `							}` |
|        16 | 10383 | `							if( pDeepest ){` |
|        16 | 10384 | `								pOrigin = pDeepest;` |
|        16 | 10385 | `								break;` |
|         - | 10386 | `							}` |
|       ! 0 | 10387 | `						}` |
|        26 | 10388 | `						pWalk = pWalk->pBase;` |
|         4 | 10389 | `					}` |
|         7 | 10390 | `				}` |
|         - | 10391 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|        20 | 10392 | `				if( !pOrigin ){` |
|         3 | 10393 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|         3 | 10394 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|         3 | 10395 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|         3 | 10396 | `							pOrigin = pClass;` |
|         3 | 10397 | `							break;` |
|         - | 10398 | `						}` |
|       ! 0 | 10399 | `					}` |
|         1 | 10400 | `				}` |
|         - | 10401 | `			}` |
|        20 | 10402 | `			if( pOrigin ){` |
|        20 | 10403 | `				SyBlobFormat(&sMsg,"%z::",&pOrigin->sName);` |
|        12 | 10404 | `			}else{` |
|         - | 10405 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|       ! 0 | 10406 | `				SyBlobFormat(&sMsg,"%z::",&pClass->sName);` |
|         - | 10407 | `			}` |
|        20 | 10408 | `			GenStateAppendAbstractMemberName(&sMsg,pMName);` |
|        20 | 10409 | `			nListed++;` |
|         4 | 10410 | `		}` |
|         - | 10411 | `	}` |
|        18 | 10412 | `	SyBlobAppend(&sMsg,")",1);` |
|        25 | 10413 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|        14 | 10414 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|        18 | 10415 | `	SyBlobRelease(&sMsg);` |
|        18 | 10416 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 10417 | `		return SXERR_ABORT;` |
|         - | 10418 | `	}` |
|        18 | 10419 | `	return SXRET_OK;` |
|    193403 | 10420 | `}` |
|         - | 10421 | `/*` |
|         - | 10422 | ` * Parse a class/interface name reference from the current token stream.` |
|         - | 10423 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|         - | 10424 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|         - | 10425 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|         - | 10426 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|         - | 10427 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|         - | 10428 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|         - | 10429 | ` */` |
|    461020 | 10430 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|         5 | 10431 | `{` |
|    461025 | 10432 | `	int isAbsolute = 0;` |
|    461025 | 10433 | `	SyToken *pStart = pGen->pIn;` |
|         - | 10434 | `	SyBlob sName;` |
|    461025 | 10435 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      4501 | 10436 | `		isAbsolute = 1;` |
|      4501 | 10437 | `		pGen->pIn++;` |
|      2248 | 10438 | `	}` |
|    461025 | 10439 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         8 | 10440 | `		pGen->pIn = pStart;` |
|         8 | 10441 | `		return SXERR_INVALID;` |
|         - | 10442 | `	}` |
|    461019 | 10443 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|    461019 | 10444 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|    461019 | 10445 | `	pGen->pIn++;` |
|    691560 | 10446 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|    230551 | 10447 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        28 | 10448 | `		SyBlobAppend(&sName,"\\",1);` |
|        28 | 10449 | `		pGen->pIn++;` |
|        28 | 10450 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        28 | 10451 | `		pGen->pIn++;` |
|         2 | 10452 | `	}` |
|    461019 | 10453 | `	if( isAbsolute ){` |
|      4499 | 10454 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      2252 | 10455 | `	}else{` |
|         - | 10456 | `		SyString sRaw;` |
|    456525 | 10457 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|    456525 | 10458 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|         - | 10459 | `	}` |
|    461019 | 10460 | `	SyBlobRelease(&sName);` |
|    461019 | 10461 | `	return SXRET_OK;` |
|    230515 | 10462 | `}` |
|         - | 10463 | `/*` |
|         - | 10464 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|         - | 10465 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|         - | 10466 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|         - | 10467 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|         - | 10468 | ` * either direction cannot run unbounded.` |
|         - | 10469 | ` */` |
|         - | 10470 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    186836 | 10471 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|         5 | 10472 | `{` |
|         - | 10473 | `	ph7_class **apParent;` |
|         - | 10474 | `	sxu32 n;` |
|    498133 | 10475 | `	while( pInterface ){` |
|    319083 | 10476 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|       ! 0 | 10477 | `			return FALSE;` |
|         - | 10478 | `		}` |
|    354095 | 10479 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|     70024 | 10480 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|      7791 | 10481 | `			return TRUE;` |
|         - | 10482 | `		}` |
|    311297 | 10483 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    311299 | 10484 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|         3 | 10485 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|       ! 0 | 10486 | `				return TRUE;` |
|         - | 10487 | `			}` |
|         2 | 10488 | `		}` |
|    311297 | 10489 | `		pInterface = pInterface->pBase;` |
|    311297 | 10490 | `		iDepth++;` |
|         5 | 10491 | `	}` |
|    179055 | 10492 | `	return FALSE;` |
|     93423 | 10493 | `}` |
|    186834 | 10494 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|         5 | 10495 | `{` |
|    186839 | 10496 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|         5 | 10497 | `}` |
|         - | 10498 | `/*` |
|         - | 10499 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|         - | 10500 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|         - | 10501 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|         - | 10502 | ` */` |
|      7786 | 10503 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|         5 | 10504 | `{` |
|      7795 | 10505 | `	while( pBase ){` |
|        10 | 10506 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|         2 | 10507 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|         3 | 10508 | `			return TRUE;` |
|         - | 10509 | `		}` |
|        10 | 10510 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|         6 | 10511 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|         3 | 10512 | `			return TRUE;` |
|         - | 10513 | `		}` |
|         5 | 10514 | `		pBase = pBase->pBase;` |
|         1 | 10515 | `	}` |
|      7787 | 10516 | `	return FALSE;` |
|      3898 | 10517 | `}` |
|         - | 10518 | `/*` |
|         - | 10519 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|         - | 10520 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|         - | 10521 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|         - | 10522 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|         - | 10523 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|         - | 10524 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|         - | 10525 | ` * pClass->aEnumCases for cases().` |
|         - | 10526 | ` */` |
|      7830 | 10527 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10528 | `{` |
|      7835 | 10529 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 10530 | `	SySet *pInstrContainer;` |
|         - | 10531 | `	ph7_class_attr *pCase;` |
|         - | 10532 | `	SyString *pName;` |
|         - | 10533 | `	sxi32 rc;` |
|      7835 | 10534 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|      7835 | 10535 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 10536 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10537 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|       ! 0 | 10538 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10539 | `			return SXERR_ABORT;` |
|         - | 10540 | `		}` |
|       ! 0 | 10541 | `		goto Synchronize;` |
|         - | 10542 | `	}` |
|      7835 | 10543 | `	pName = &pGen->pIn->sData;` |
|         - | 10544 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|      7835 | 10545 | `	if( SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte) != 0 ){` |
|       ! 0 | 10546 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10547 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|       ! 0 | 10548 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10549 | `			return SXERR_ABORT;` |
|         - | 10550 | `		}` |
|       ! 0 | 10551 | `		goto Synchronize;` |
|         - | 10552 | `	}` |
|      7835 | 10553 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10554 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|      7835 | 10555 | `	if( pCase == 0 ){` |
|       ! 0 | 10556 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10557 | `		return SXERR_ABORT;` |
|         - | 10558 | `	}` |
|      7835 | 10559 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|      7835 | 10560 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 10561 | `		return SXERR_ABORT;` |
|         - | 10562 | `	}` |
|      7835 | 10563 | `	pGen->pIn++; /* Jump the case name */` |
|      7835 | 10564 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|      7821 | 10565 | `		if( pClass->nEnumBacking == 0 ){` |
|         8 | 10566 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         2 | 10567 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|         6 | 10568 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10569 | `				return SXERR_ABORT;` |
|         - | 10570 | `			}` |
|         6 | 10571 | `			goto Synchronize;` |
|         - | 10572 | `		}` |
|      7817 | 10573 | `		pGen->pIn++; /* Jump the equal sign */` |
|         - | 10574 | `		/* Compile the backing value expression into the case's own container` |
|         - | 10575 | `		 * (same technique as class constants). */` |
|      7817 | 10576 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7817 | 10577 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|      7817 | 10578 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7817 | 10579 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 | 10580 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10581 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|       ! 0 | 10582 | `		}` |
|      7817 | 10583 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7817 | 10584 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7817 | 10585 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10586 | `			return SXERR_ABORT;` |
|         - | 10587 | `		}` |
|      3911 | 10588 | `	}else{` |
|        17 | 10589 | `		if( pClass->nEnumBacking != 0 ){` |
|       ! 0 | 10590 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10591 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|       ! 0 | 10592 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10593 | `				return SXERR_ABORT;` |
|         - | 10594 | `			}` |
|       ! 0 | 10595 | `			goto Synchronize;` |
|         - | 10596 | `		}` |
|         - | 10597 | `	}` |
|      7831 | 10598 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|      7831 | 10599 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 10600 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10601 | `		return SXERR_ABORT;` |
|         - | 10602 | `	}` |
|      7831 | 10603 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|      7831 | 10604 | `	return SXRET_OK;` |
|         2 | 10605 | `Synchronize:` |
|         - | 10606 | `	/* Synchronize with the first semi-colon */` |
|        14 | 10607 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|        10 | 10608 | `		pGen->pIn++;` |
|         2 | 10609 | `	}` |
|         6 | 10610 | `	return SXERR_CORRUPT;` |
|      3920 | 10611 | `}` |
|         - | 10612 | `/*` |
|         - | 10613 | ` * Synthesize the enum interface methods (PHP 8.1): cases() for every enum,` |
|         - | 10614 | ` * plus from()/tryFrom() for backed enums. Each is an ordinary public static` |
|         - | 10615 | ` * method whose body forwards to a __phl_enum_* engine thunk (vm.c) with the` |
|         - | 10616 | ` * enum's FQN embedded as a literal — the same forwarder pattern the` |
|         - | 10617 | ` * Generator/Fiber/Reflection builtins use. The source buffer is owned by the` |
|         - | 10618 | ` * VM allocator and never freed: tokens (method and parameter names) keep` |
|         - | 10619 | ` * pointers into it (see the constructor-promotion precedent above).` |
|         - | 10620 | ` */` |
|      3916 | 10621 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10622 | `{` |
|         - | 10623 | `	SyToken *pSaveIn,*pSaveEnd;` |
|         - | 10624 | `	const char *zBack;` |
|         - | 10625 | `	SySet sToken;` |
|         - | 10626 | `	char *zSrc;` |
|         - | 10627 | `	sxu32 nSrc,nMax;` |
|      3921 | 10628 | `	sxi32 rc = SXRET_OK;` |
|      3921 | 10629 | `	nMax = 3*(sxu32)sizeof("function tryFrom(string $value){return __phl_enum_tryfrom('',$value);}")` |
|      3916 | 10630 | `		+ 3*SyStringLength(&pClass->sName) + 64;` |
|      3921 | 10631 | `	zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nMax);` |
|      3921 | 10632 | `	if( zSrc == 0 ){` |
|       ! 0 | 10633 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10634 | `		return SXERR_ABORT;` |
|         - | 10635 | `	}` |
|      3921 | 10636 | `	zBack = (pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string";` |
|      3921 | 10637 | `	if( pClass->nEnumBacking != 0 ){` |
|      5858 | 10638 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         - | 10639 | `			"function cases(){return __phl_enum_cases('%z');}"` |
|         - | 10640 | `			"function from(%s $value){return __phl_enum_from('%z',$value);}"` |
|         - | 10641 | `			"function tryFrom(%s $value){return __phl_enum_tryfrom('%z',$value);}",` |
|      1951 | 10642 | `			&pClass->sName,zBack,&pClass->sName,zBack,&pClass->sName);` |
|      1956 | 10643 | `	}else{` |
|        24 | 10644 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         7 | 10645 | `			"function cases(){return __phl_enum_cases('%z');}",&pClass->sName);` |
|         - | 10646 | `	}` |
|      3921 | 10647 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      3921 | 10648 | `	PH7_TokenizePHP(zSrc,nSrc,pClass->nLine,&sToken,0);` |
|      3921 | 10649 | `	pSaveIn = pGen->pIn;` |
|      3921 | 10650 | `	pSaveEnd = pGen->pEnd;` |
|      3921 | 10651 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      3921 | 10652 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|     15641 | 10653 | `	while( pGen->pIn < pGen->pEnd && rc != SXERR_ABORT ){` |
|     11725 | 10654 | `		rc = GenStateCompileClassMethod(&(*pGen),PH7_TKWRD_PUBLIC,PH7_CLASS_ATTR_STATIC,TRUE,pClass);` |
|         5 | 10655 | `	}` |
|      3921 | 10656 | `	pGen->pIn = pSaveIn;` |
|      3921 | 10657 | `	pGen->pEnd = pSaveEnd;` |
|      3921 | 10658 | `	SySetRelease(&sToken);` |
|      3921 | 10659 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXRET_OK;` |
|      1963 | 10660 | `}` |
|         - | 10661 | `/*` |
|         - | 10662 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|         - | 10663 | ` * __call/__callStatic/__invoke stay allowed).` |
|         - | 10664 | ` */` |
|         - | 10665 | `static const char *azEnumBannedMagic[] = {` |
|         - | 10666 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|         - | 10667 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|         - | 10668 | `};` |
|         - | 10669 | `/*` |
|         - | 10670 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|         - | 10671 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|         - | 10672 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|         - | 10673 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|         - | 10674 | ` * and before the class is installed.` |
|         - | 10675 | ` */` |
|      3916 | 10676 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|         5 | 10677 | `{` |
|         - | 10678 | `	SyHashEntry *pEntry;` |
|         - | 10679 | `	sxi32 rc;` |
|         - | 10680 | `	sxu32 n;` |
|         - | 10681 | `	/* php: "Enum %s cannot include properties" */` |
|      3921 | 10682 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     11751 | 10683 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      7837 | 10684 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      7837 | 10685 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|         3 | 10686 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|         1 | 10687 | `				"Enum %z cannot include properties",&pClass->sName);` |
|         3 | 10688 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10689 | `				return SXERR_ABORT;` |
|         - | 10690 | `			}` |
|         3 | 10691 | `			break;` |
|         - | 10692 | `		}` |
|         5 | 10693 | `	}` |
|         - | 10694 | `	/* php: "Enum %s cannot include magic method %s" */` |
|     54829 | 10695 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|     76362 | 10696 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|     50913 | 10697 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|       ! 0 | 10698 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10699 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|       ! 0 | 10700 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10701 | `				return SXERR_ABORT;` |
|         - | 10702 | `			}` |
|       ! 0 | 10703 | `		}` |
|     25459 | 10704 | `	}` |
|         - | 10705 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|         - | 10706 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|         - | 10707 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|         - | 10708 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|         - | 10709 | `	{` |
|         - | 10710 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|         - | 10711 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|         - | 10712 | `		ph7_class_attr *pAttr;` |
|      3921 | 10713 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10714 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      3921 | 10715 | `		if( pAttr == 0 ){` |
|       ! 0 | 10716 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10717 | `			return SXERR_ABORT;` |
|         - | 10718 | `		}` |
|      3921 | 10719 | `		pAttr->nType = MEMOBJ_STRING;` |
|      3921 | 10720 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|      3921 | 10721 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|      3921 | 10722 | `		if( pClass->nEnumBacking != 0 ){` |
|      3907 | 10723 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10724 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      3907 | 10725 | `			if( pAttr == 0 ){` |
|       ! 0 | 10726 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10727 | `				return SXERR_ABORT;` |
|         - | 10728 | `			}` |
|      3907 | 10729 | `			pAttr->nType = pClass->nEnumBacking;` |
|      3907 | 10730 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|         7 | 10731 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|         4 | 10732 | `			}else{` |
|      3901 | 10733 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|         - | 10734 | `			}` |
|      3907 | 10735 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|      1951 | 10736 | `		}` |
|         - | 10737 | `	}` |
|      3921 | 10738 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|      1963 | 10739 | `}` |
|         - | 10740 | `/*` |
|         - | 10741 | ` * Compile a class declaration, named or anonymous.` |
|         - | 10742 | ` *` |
|         - | 10743 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|         - | 10744 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|         - | 10745 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|         - | 10746 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|         - | 10747 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|         - | 10748 | ` * implements, body, install) is shared by both paths.` |
|         - | 10749 | ` */` |
|    386840 | 10750 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|         - | 10751 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|         5 | 10752 | `{` |
|    386845 | 10753 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 10754 | `	ph7_class *pClass,*pBase;` |
|         - | 10755 | `	SyToken *pEnd,*pTmp;` |
|         - | 10756 | `	sxi32 iProtection;` |
|         - | 10757 | `	SySet aInterfaces;` |
|         - | 10758 | `	SySet aUseEntries;` |
|         - | 10759 | `	sxi32 iAttrflags;` |
|         - | 10760 | `	SyString *pName;` |
|         - | 10761 | `	sxi32 nKwrd;` |
|         - | 10762 | `	sxi32 rc;` |
|         - | 10763 | `	/* Jump the 'class' keyword */` |
|    386845 | 10764 | `	pGen->pIn++;` |
|    386845 | 10765 | `	if( pAnonName ){` |
|         - | 10766 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|         - | 10767 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|         - | 10768 | `		 * then use the synthesized name. */` |
|        34 | 10769 | `		*ppArgStart = *ppArgEnd = 0;` |
|        34 | 10770 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         7 | 10771 | `			pGen->pIn++; /* Jump '(' */` |
|         7 | 10772 | `			*ppArgStart = pGen->pIn;` |
|        10 | 10773 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|         3 | 10774 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|         7 | 10775 | `			pGen->pIn = *ppArgEnd;` |
|         7 | 10776 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|         3 | 10777 | `		}` |
|        34 | 10778 | `		pName = pAnonName;` |
|        34 | 10779 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|        19 | 10780 | `	}else{` |
|    386815 | 10781 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - | 10782 | `			/* Syntax error */` |
|       ! 0 | 10783 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|       ! 0 | 10784 | `			if( rc == SXERR_ABORT ){` |
|         - | 10785 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 10786 | `				return SXERR_ABORT;` |
|         - | 10787 | `			}` |
|         - | 10788 | `			/* Synchronize with the first semi-colon or curly braces */` |
|       ! 0 | 10789 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|       ! 0 | 10790 | `				pGen->pIn++;` |
|       ! 0 | 10791 | `			}` |
|       ! 0 | 10792 | `			return SXRET_OK;` |
|         - | 10793 | `		}` |
|         - | 10794 | `		/* Extract class name */` |
|    386815 | 10795 | `		pName = &pGen->pIn->sData;` |
|         - | 10796 | `		/* Advance the stream cursor */` |
|    386815 | 10797 | `		pGen->pIn++;` |
|         - | 10798 | `		/* Build FQN and obtain a raw class */ {` |
|         - | 10799 | `			SyBlob sFQN;` |
|         - | 10800 | `			SyString sFQNStr;` |
|    386815 | 10801 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    386815 | 10802 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|    386815 | 10803 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    386815 | 10804 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    386815 | 10805 | `			SyBlobRelease(&sFQN);` |
|         - | 10806 | `		}` |
|         - | 10807 | `	}` |
|    386845 | 10808 | `	if( pClass == 0 ){` |
|       ! 0 | 10809 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10810 | `		return SXERR_ABORT;` |
|         - | 10811 | `	}` |
|    386840 | 10812 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|      3925 | 10813 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|         - | 10814 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|      3909 | 10815 | `		pGen->pIn++; /* Jump ':' */` |
|      3904 | 10816 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      3909 | 10817 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|         7 | 10818 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|         7 | 10819 | `			pGen->pIn++;` |
|      3902 | 10820 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      3903 | 10821 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|      3901 | 10822 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|      3901 | 10823 | `			pGen->pIn++;` |
|      1953 | 10824 | `		}else{` |
|         3 | 10825 | `			SyToken *pTok = pGen->pIn;` |
|         3 | 10826 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|         4 | 10827 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|         1 | 10828 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|         3 | 10829 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10830 | `				return SXERR_ABORT;` |
|         - | 10831 | `			}` |
|         3 | 10832 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|         3 | 10833 | `				pGen->pIn++; /* Skip the bogus type token */` |
|         1 | 10834 | `			}` |
|         - | 10835 | `		}` |
|      1952 | 10836 | `	}` |
|    386845 | 10837 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    386845 | 10838 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 10839 | `		return SXERR_ABORT;` |
|         - | 10840 | `	}` |
|         - | 10841 | `	/* implemented interfaces and per-use-statement trait containers */` |
|    386845 | 10842 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|    386845 | 10843 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|         - | 10844 | `	/* Assume a standalone class */` |
|    386845 | 10845 | `	pBase = 0;` |
|    386845 | 10846 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    319291 | 10847 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    319291 | 10848 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|         - | 10849 | `			SyBlob sResolved;` |
|         - | 10850 | `			SyString sBaseName;` |
|         - | 10851 | `			sxu32 nRefLine;` |
|    206359 | 10852 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|         - | 10853 | `				/* php parse-fatals here (enums have no inheritance) */` |
|       ! 0 | 10854 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10855 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|       ! 0 | 10856 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10857 | `					return SXERR_ABORT;` |
|         - | 10858 | `				}` |
|       ! 0 | 10859 | `			}` |
|    206359 | 10860 | `			pGen->pIn++; /* Advance past 'extends' */` |
|    206359 | 10861 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    206359 | 10862 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    206359 | 10863 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         3 | 10864 | `				SyBlobRelease(&sResolved);` |
|         4 | 10865 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 10866 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|         1 | 10867 | `					pName);` |
|         3 | 10868 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|         3 | 10869 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10870 | `					return SXERR_ABORT;` |
|         - | 10871 | `				}` |
|         3 | 10872 | `				return SXRET_OK;` |
|         - | 10873 | `			}` |
|    309533 | 10874 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    206352 | 10875 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    206357 | 10876 | `			SyStringInitFromBuf(&sBaseName,` |
|         - | 10877 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 10878 | `			/* Interfaces are not allowed */` |
|    206357 | 10879 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|       ! 0 | 10880 | `				pBase = pBase->pNextName;` |
|       ! 0 | 10881 | `			}` |
|    206357 | 10882 | `			if( pBase == 0 ){` |
|       ! 0 | 10883 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 10884 | `					"Nonexistent base class '%z'",&sBaseName);` |
|       ! 0 | 10885 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10886 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 10887 | `					return SXERR_ABORT;` |
|         - | 10888 | `				}` |
|       ! 0 | 10889 | `			}else{` |
|    206357 | 10890 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|         4 | 10891 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 | 10892 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|         3 | 10893 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10894 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10895 | `						return SXERR_ABORT;` |
|         - | 10896 | `					}` |
|         3 | 10897 | `					pBase = 0; /* Never inherit from an enum */` |
|    206356 | 10898 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|       ! 0 | 10899 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10900 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|       ! 0 | 10901 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10902 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10903 | `						return SXERR_ABORT;` |
|         - | 10904 | `					}` |
|       ! 0 | 10905 | `				}` |
|         - | 10906 | `			}` |
|    206357 | 10907 | `			SyBlobRelease(&sResolved);` |
|    206357 | 10908 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|       ! 0 | 10909 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|       ! 0 | 10910 | `			}` |
|    103176 | 10911 | `		}` |
|    319289 | 10912 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|         - | 10913 | `			ph7_class *pInterface;` |
|         - | 10914 | `			/* Interface implementation */` |
|    128505 | 10915 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    122584 | 10916 | `			for(;;){` |
|         - | 10917 | `				SyBlob sResolved;` |
|         - | 10918 | `				SyString sIntName;` |
|         - | 10919 | `				sxu32 nRefLine;` |
|    186839 | 10920 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    186839 | 10921 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    186839 | 10922 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 10923 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 10924 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 10925 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|       ! 0 | 10926 | `						pName);` |
|       ! 0 | 10927 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10928 | `						return SXERR_ABORT;` |
|         - | 10929 | `					}` |
|       ! 0 | 10930 | `					break;` |
|         - | 10931 | `				}` |
|    373673 | 10932 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    186834 | 10933 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    186839 | 10934 | `				SyStringInitFromBuf(&sIntName,` |
|         - | 10935 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 10936 | `				/* Only interfaces are allowed */` |
|    186839 | 10937 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 10938 | `					pInterface = pInterface->pNextName;` |
|       ! 0 | 10939 | `				}` |
|    186839 | 10940 | `				if( pInterface == 0 ){` |
|       ! 0 | 10941 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 10942 | `						"Nonexistent base interface '%z'",&sIntName);` |
|       ! 0 | 10943 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10944 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10945 | `						return SXERR_ABORT;` |
|         - | 10946 | `					}` |
|       ! 0 | 10947 | `				}else{` |
|         - | 10948 | `					/* Reject user classes that try to implement Throwable` |
|         - | 10949 | `					 * directly (or via an interface that extends Throwable)` |
|         - | 10950 | `					 * unless they already extend Exception or Error.` |
|         - | 10951 | `					 * Exception and Error themselves are compiled from the` |
|         - | 10952 | `					 * built-in library and are exempt by FQN — a namespaced` |
|         - | 10953 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    186839 | 10954 | `					SyString *pFqn = &pClass->sName;` |
|    186839 | 10955 | `					int bIsExceptionOrError =` |
|     97309 | 10956 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|    282199 | 10957 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    184897 | 10958 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|      3902 | 10959 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|    190727 | 10960 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|     11682 | 10961 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|      3891 | 10962 | `						!bIsExceptionOrError ){` |
|        12 | 10963 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10964 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|         3 | 10965 | `							&pClass->sName);` |
|         9 | 10966 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10967 | `							SyBlobRelease(&sResolved);` |
|       ! 0 | 10968 | `							return SXERR_ABORT;` |
|         - | 10969 | `						}` |
|         - | 10970 | `						/* Skip registration so the follow-up abstract-method` |
|         - | 10971 | `						 * check does not produce a duplicate fatal. */` |
|         6 | 10972 | `					}else{` |
|    186833 | 10973 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|         - | 10974 | `					}` |
|         - | 10975 | `				}` |
|    186839 | 10976 | `				SyBlobRelease(&sResolved);` |
|    186839 | 10977 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     64255 | 10978 | `					break;` |
|         - | 10979 | `				}` |
|     58339 | 10980 | `				pGen->pIn++;/* Jump the comma */` |
|         5 | 10981 | `			}` |
|     64250 | 10982 | `		}` |
|    159642 | 10983 | `	}` |
|    386843 | 10984 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - | 10985 | `		/* Syntax error */` |
|       ! 0 | 10986 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|       ! 0 | 10987 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 10988 | `		if( rc == SXERR_ABORT ){` |
|         - | 10989 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 10990 | `			return SXERR_ABORT;` |
|         - | 10991 | `		}` |
|       ! 0 | 10992 | `		return SXRET_OK;` |
|         - | 10993 | `	}` |
|    386843 | 10994 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    386843 | 10995 | `	pEnd = 0; /* cc warning */` |
|         - | 10996 | `	/* Delimit the class body */` |
|    386843 | 10997 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    386843 | 10998 | `	if( pEnd >= pGen->pEnd ){` |
|         - | 10999 | `		/* Syntax error */` |
|       ! 0 | 11000 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|       ! 0 | 11001 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 11002 | `		if( rc == SXERR_ABORT ){` |
|         - | 11003 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 11004 | `			return SXERR_ABORT;` |
|         - | 11005 | `		}` |
|       ! 0 | 11006 | `		return SXRET_OK;` |
|         - | 11007 | `	}` |
|         - | 11008 | `	/* The delimiter token is the class body's closing brace */` |
|    386843 | 11009 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 11010 | `	/* Swap token stream */` |
|    386843 | 11011 | `	pTmp = pGen->pEnd;` |
|    386843 | 11012 | `	pGen->pEnd = pEnd;` |
|         - | 11013 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|    386843 | 11014 | `	pClass->iFlags \|= iFlags;` |
|         - | 11015 | `	/* Start the parse process */` |
|   1537623 | 11016 | `	for(;;){` |
|         - | 11017 | `		/* Jump leading/trailing semi-colons */` |
|   4383995 | 11018 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    802161 | 11019 | `			pGen->pIn++;` |
|         5 | 11020 | `		}` |
|   3581839 | 11021 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 11022 | `			/* End of class body */` |
|    386801 | 11023 | `			break;` |
|         - | 11024 | `		}` |
|         - | 11025 | `		/* Bind a directly-preceding docblock to this member */` |
|   3195043 | 11026 | `		GenStateSetPendingDoc(&(*pGen));` |
|   3195038 | 11027 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|   1597524 | 11028 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|       ! 0 | 11029 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11030 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 11031 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 11032 | `			if( rc == SXERR_ABORT ){` |
|         - | 11033 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 11034 | `				return SXERR_ABORT;` |
|         - | 11035 | `			}` |
|       ! 0 | 11036 | `			goto done;` |
|         - | 11037 | `		}` |
|         - | 11038 | `		/* Assume public visibility */` |
|   3195043 | 11039 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|   3195043 | 11040 | `		iAttrflags = 0;` |
|         - | 11041 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|         - | 11042 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|         - | 11043 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|         - | 11044 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|   3195043 | 11045 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 11046 | `			int bMod = 0;` |
|       ! 0 | 11047 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 11048 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|         - | 11049 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|         - | 11050 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|         - | 11051 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|         - | 11052 | `			 * that the generic keyword dispatch would misread as a method. */` |
|       ! 0 | 11053 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       ! 0 | 11054 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       ! 0 | 11055 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|       ! 0 | 11056 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|       ! 0 | 11057 | `			}` |
|       ! 0 | 11058 | `			if( !bMod ){` |
|       ! 0 | 11059 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11060 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 11061 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11062 | `						return SXERR_ABORT;` |
|         - | 11063 | `					}` |
|       ! 0 | 11064 | `					goto done;` |
|         - | 11065 | `				}` |
|       ! 0 | 11066 | `				continue;` |
|         - | 11067 | `			}` |
|       ! 0 | 11068 | `		}` |
|   3195043 | 11069 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 11070 | `			/* Extract the current keyword */` |
|   3195043 | 11071 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   3195043 | 11072 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|         - | 11073 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|      7835 | 11074 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|      7835 | 11075 | `				if( rc != SXRET_OK ){` |
|         6 | 11076 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11077 | `						return SXERR_ABORT;` |
|         - | 11078 | `					}` |
|         6 | 11079 | `					goto done;` |
|         - | 11080 | `				}` |
|      7831 | 11081 | `				continue;` |
|         - | 11082 | `			}` |
|   3187213 | 11083 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 11084 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|         - | 11085 | `				TraitUseEntry sUse;` |
|     15637 | 11086 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|     15637 | 11087 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|     15637 | 11088 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      7824 | 11089 | `				for(;;){` |
|         - | 11090 | `					ph7_class *pTrait;` |
|         - | 11091 | `					SyBlob sResolved;` |
|         - | 11092 | `					SyString sTraitName;` |
|     15645 | 11093 | `					sxu32 nUseLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|         - | 11094 | `					/* A trait name is a full class reference: it may be qualified or` |
|         - | 11095 | ``					 * fully-qualified (`use Foo\Bar\T;`, `use \Foo\Bar\T;`) — the generated`` |
|         - | 11096 | `					 * PHPUnit mocks name their traits absolutely. Parse it with the shared` |
|         - | 11097 | `					 * class-reference reader (handles the leading '\', every '\'-segment and` |
|         - | 11098 | `					 * namespace/import resolution) instead of a single-identifier read, which` |
|         - | 11099 | `					 * choked on the first '\'. */` |
|     15645 | 11100 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     15645 | 11101 | `					if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 11102 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 11103 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|       ! 0 | 11104 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|       ! 0 | 11105 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11106 | `							return SXERR_ABORT;` |
|         - | 11107 | `						}` |
|       ! 0 | 11108 | `						break;` |
|         - | 11109 | `					}` |
|     31285 | 11110 | `					pTrait = PH7_VmExtractClass(pGen->pVm,` |
|     15640 | 11111 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     15645 | 11112 | `					SyStringInitFromBuf(&sTraitName,` |
|         - | 11113 | `						(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 11114 | `					/* Only traits are allowed */` |
|     15645 | 11115 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 11116 | `						pTrait = pTrait->pNextName;` |
|       ! 0 | 11117 | `					}` |
|     15645 | 11118 | `					if( pTrait == 0 ){` |
|       ! 0 | 11119 | `						rc = PH7_GenCompileError(pGen,E_ERROR,nUseLine,` |
|         - | 11120 | `							"'%z' is not a trait",&sTraitName);` |
|       ! 0 | 11121 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11122 | `							SyBlobRelease(&sResolved);` |
|       ! 0 | 11123 | `							return SXERR_ABORT;` |
|         - | 11124 | `						}` |
|       ! 0 | 11125 | `					}else{` |
|     15645 | 11126 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|         - | 11127 | `					}` |
|     15645 | 11128 | `					SyBlobRelease(&sResolved);` |
|         - | 11129 | `					/* GenStateParseClassReference already advanced past the whole name —` |
|         - | 11130 | `					 * continue only across a comma-separated trait list. */` |
|     15645 | 11131 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      7821 | 11132 | `						break;` |
|         - | 11133 | `					}` |
|        10 | 11134 | `					pGen->pIn++; /* Jump the comma */` |
|         2 | 11135 | `				}` |
|         - | 11136 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|     15637 | 11137 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - | 11138 | `					SyToken *pBlock;` |
|        12 | 11139 | `					pGen->pIn++; /* Jump '{' */` |
|        12 | 11140 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|        12 | 11141 | `					sUse.pResolvStart = pGen->pIn;` |
|        12 | 11142 | `					sUse.pResolvEnd = pBlock;` |
|        12 | 11143 | `					if( pBlock < pGen->pEnd ){` |
|        12 | 11144 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|         7 | 11145 | `					}else{` |
|       ! 0 | 11146 | `						pGen->pIn = pGen->pEnd;` |
|         - | 11147 | `					}` |
|         5 | 11148 | `				}` |
|     15637 | 11149 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|         - | 11150 | `				/* The semicolon will be consumed by the outer loop */` |
|     15637 | 11151 | `				continue;` |
|         - | 11152 | `			}` |
|   3171581 | 11153 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - | 11154 | `				int nSetTok;` |
|   2875585 | 11155 | `				sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   2875585 | 11156 | `				if( nSetVis ){` |
|         - | 11157 | ``					/* Leading `private(set)`/`protected(set)` with no read`` |
|         - | 11158 | `					 * visibility: the read side defaults to public (php 8.4). */` |
|         3 | 11159 | `					iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 11160 | `					pGen->pIn += nSetTok;` |
|         2 | 11161 | `				}else{` |
|   2875583 | 11162 | `					iProtection = nKwrd;` |
|   2875583 | 11163 | `					pGen->pIn++; /* Jump the visibility token */` |
|         - | 11164 | `					/* Optional asymmetric set-visibility after the read` |
|         - | 11165 | ``					 * visibility: `public private(set) int $x`. */`` |
|   2875583 | 11166 | `					nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   2875583 | 11167 | `					if( nSetVis ){` |
|         9 | 11168 | `						iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         9 | 11169 | `						pGen->pIn += nSetTok;` |
|         4 | 11170 | `					}` |
|         - | 11171 | `				}` |
|         - | 11172 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`,`` |
|         - | 11173 | ``				 * `public private(set) readonly int $x`. */`` |
|   2875585 | 11174 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|        28 | 11175 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|        28 | 11176 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        12 | 11177 | `				}` |
|   2875580 | 11178 | `				if( pGen->pIn >= pGen->pEnd` |
|   2875585 | 11179 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11180 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11181 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 11182 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 11183 | `					if( rc == SXERR_ABORT ){` |
|         - | 11184 | `						/* Error count limit reached,abort immediately */` |
|       ! 0 | 11185 | `						return SXERR_ABORT;` |
|         - | 11186 | `					}` |
|       ! 0 | 11187 | `					goto done;` |
|         - | 11188 | `				}` |
|   2875585 | 11189 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 11190 | `					/* Attribute declaration (untyped) */` |
|    439987 | 11191 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    439987 | 11192 | `					if( rc != SXRET_OK ){` |
|        12 | 11193 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11194 | `							return SXERR_ABORT;` |
|         - | 11195 | `						}` |
|        12 | 11196 | `						goto done;` |
|         - | 11197 | `					}` |
|    447916 | 11198 | `					continue;` |
|         - | 11199 | `				}` |
|   2435603 | 11200 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 11201 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     15885 | 11202 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     15885 | 11203 | `					if( rc != SXRET_OK ){` |
|         8 | 11204 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11205 | `							return SXERR_ABORT;` |
|         - | 11206 | `						}` |
|         8 | 11207 | `						goto done;` |
|         - | 11208 | `					}` |
|     15879 | 11209 | `					continue;` |
|         - | 11210 | `				}` |
|         - | 11211 | `				/* Extract the keyword */` |
|   2419723 | 11212 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1209859 | 11213 | `			}` |
|   2715719 | 11214 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|         - | 11215 | `				/* Process constant declaration */` |
|    287883 | 11216 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|    287883 | 11217 | `				if( rc != SXRET_OK ){` |
|        11 | 11218 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11219 | `						return SXERR_ABORT;` |
|         - | 11220 | `					}` |
|        11 | 11221 | `					goto done;` |
|         - | 11222 | `				}` |
|    143940 | 11223 | `			}else{` |
|   2427841 | 11224 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - | 11225 | `					/* Static method or attribute,record that */` |
|    101281 | 11226 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    101281 | 11227 | `					pGen->pIn++; /* Jump the static keyword */` |
|    101281 | 11228 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 11229 | `						int nSetTok;` |
|     74025 | 11230 | `						sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|     74025 | 11231 | `						if( nSetVis ){` |
|         - | 11232 | ``							/* `static private(set) int $x` — read side stays public */`` |
|         3 | 11233 | `							iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 11234 | `							pGen->pIn += nSetTok;` |
|         2 | 11235 | `						}else{` |
|         - | 11236 | `							/* Extract the keyword */` |
|     74023 | 11237 | `							nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     74023 | 11238 | `							if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 11239 | `								iProtection = nKwrd;` |
|       ! 0 | 11240 | `								pGen->pIn++; /* Jump the visibility token */` |
|       ! 0 | 11241 | `								nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|       ! 0 | 11242 | `								if( nSetVis ){` |
|       ! 0 | 11243 | `									iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|       ! 0 | 11244 | `									pGen->pIn += nSetTok;` |
|       ! 0 | 11245 | `								}` |
|       ! 0 | 11246 | `							}` |
|         - | 11247 | `						}` |
|     37010 | 11248 | `					}` |
|         - | 11249 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|         - | 11250 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|         - | 11251 | `					 * than a generic "expecting method" parse error. */` |
|    101281 | 11252 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 11253 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 11254 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       ! 0 | 11255 | `					}` |
|    101276 | 11256 | `					if( pGen->pIn >= pGen->pEnd` |
|    101281 | 11257 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11258 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11259 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|       ! 0 | 11260 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11261 | `						if( rc == SXERR_ABORT ){` |
|         - | 11262 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11263 | `							return SXERR_ABORT;` |
|         - | 11264 | `						}` |
|       ! 0 | 11265 | `						goto done;` |
|         - | 11266 | `					}` |
|    101281 | 11267 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 11268 | `						/* Attribute declaration */` |
|     27257 | 11269 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     27257 | 11270 | `						if( rc != SXRET_OK ){` |
|         3 | 11271 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11272 | `								return SXERR_ABORT;` |
|         - | 11273 | `							}` |
|         3 | 11274 | `							goto done;` |
|         - | 11275 | `						}` |
|     27255 | 11276 | `						continue;` |
|         - | 11277 | `					}` |
|     74029 | 11278 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 11279 | `						/* Typed static attribute declaration */` |
|        19 | 11280 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        19 | 11281 | `						if( rc != SXRET_OK ){` |
|         3 | 11282 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11283 | `								return SXERR_ABORT;` |
|         - | 11284 | `							}` |
|         3 | 11285 | `							goto done;` |
|         - | 11286 | `						}` |
|        17 | 11287 | `						continue;` |
|         - | 11288 | `					}` |
|         - | 11289 | `					/* Extract the keyword */` |
|     74013 | 11290 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   2363569 | 11291 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         - | 11292 | `					/* Abstract method,record that */` |
|      7803 | 11293 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         - | 11294 | `					/* Mark the whole class as abstract */` |
|      7803 | 11295 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|         - | 11296 | `					/* Advance the stream cursor */` |
|      7803 | 11297 | `					pGen->pIn++;` |
|      7803 | 11298 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      7803 | 11299 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      7803 | 11300 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      7801 | 11301 | `							iProtection = nKwrd;` |
|      7801 | 11302 | `							pGen->pIn++; /* Jump the visibility token */` |
|      3898 | 11303 | `						}` |
|      3899 | 11304 | `					}` |
|      7803 | 11305 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      7798 | 11306 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 11307 | `							/* Static method */` |
|       ! 0 | 11308 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 11309 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 11310 | `					}` |
|      7803 | 11311 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      7798 | 11312 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|         - | 11313 | ``							/* PHP 8.4: `abstract public [T] $x { get; set; }` — an abstract`` |
|         - | 11314 | `							 * HOOKED property declaration. Route anything that is not a` |
|         - | 11315 | `							 * method through the attribute compiler with the ABSTRACT flag;` |
|         - | 11316 | ``							 * the hook parser accepts the bare `get;`/`set;` forms there`` |
|         - | 11317 | `							 * (and a non-hooked abstract property is ITS error to raise). */` |
|         6 | 11318 | `							if( pGen->pIn < pGen->pEnd` |
|         7 | 11319 | `							 && ((pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID\|PH7_TK_DOLLAR)) != 0` |
|         3 | 11320 | `							  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         7 | 11321 | `								rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         7 | 11322 | `								if( rc != SXRET_OK ){` |
|       ! 0 | 11323 | `									if( rc == SXERR_ABORT ){` |
|       ! 0 | 11324 | `										return SXERR_ABORT;` |
|         - | 11325 | `									}` |
|       ! 0 | 11326 | `									goto done;` |
|         - | 11327 | `								}` |
|         7 | 11328 | `								continue;` |
|         - | 11329 | `							}` |
|       ! 0 | 11330 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11331 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|       ! 0 | 11332 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 11333 | `							if( rc == SXERR_ABORT ){` |
|         - | 11334 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 11335 | `								return SXERR_ABORT;` |
|         - | 11336 | `							}` |
|       ! 0 | 11337 | `							goto done;` |
|         - | 11338 | `					}` |
|      7797 | 11339 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|   2322663 | 11340 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|         - | 11341 | `					/* final method ,record that */` |
|        20 | 11342 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|        20 | 11343 | `					pGen->pIn++; /* Jump the final keyword */` |
|        20 | 11344 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 11345 | `						/* Extract the keyword */` |
|        20 | 11346 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        20 | 11347 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        10 | 11348 | `							iProtection = nKwrd;` |
|        10 | 11349 | `							pGen->pIn++; /* Jump the visibility token */` |
|         4 | 11350 | `						}` |
|         9 | 11351 | `					}` |
|        20 | 11352 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        18 | 11353 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|         - | 11354 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|         - | 11355 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|         - | 11356 | `							 * child class is compiled (PH7_ClassInherit). */` |
|        14 | 11357 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|        14 | 11358 | `							if( rc != SXRET_OK ){` |
|       ! 0 | 11359 | `								if( rc == SXERR_ABORT ){` |
|       ! 0 | 11360 | `									return SXERR_ABORT;` |
|         - | 11361 | `								}` |
|       ! 0 | 11362 | `								goto done;` |
|         - | 11363 | `							}` |
|        14 | 11364 | `							continue;` |
|         - | 11365 | `					}` |
|         8 | 11366 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         6 | 11367 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 11368 | `							/* Static method */` |
|       ! 0 | 11369 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 11370 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 11371 | `					}` |
|         8 | 11372 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         6 | 11373 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 11374 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11375 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|       ! 0 | 11376 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 11377 | `							if( rc == SXERR_ABORT ){` |
|         - | 11378 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 11379 | `								return SXERR_ABORT;` |
|         - | 11380 | `							}` |
|       ! 0 | 11381 | `							goto done;` |
|         - | 11382 | `					}` |
|         8 | 11383 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         3 | 11384 | `				}` |
|   2400555 | 11385 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 11386 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11387 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|       ! 0 | 11388 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11389 | `						if( rc == SXERR_ABORT ){` |
|         - | 11390 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11391 | `							return SXERR_ABORT;` |
|         - | 11392 | `						}` |
|       ! 0 | 11393 | `						goto done;` |
|         - | 11394 | `				}` |
|   2400555 | 11395 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|         7 | 11396 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|         7 | 11397 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|       ! 0 | 11398 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11399 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 11400 | `						if( rc == SXERR_ABORT ){` |
|         - | 11401 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11402 | `							return SXERR_ABORT;` |
|         - | 11403 | `						}` |
|       ! 0 | 11404 | `						goto done;` |
|         - | 11405 | `					}` |
|         - | 11406 | `					/* Attribute declaration */` |
|         7 | 11407 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         4 | 11408 | `				}else{` |
|         - | 11409 | `					/* Process method declaration */` |
|   2400549 | 11410 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 11411 | `				}` |
|   2400555 | 11412 | `				if( rc != SXRET_OK ){` |
|        16 | 11413 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11414 | `						return SXERR_ABORT;` |
|         - | 11415 | `					}` |
|        16 | 11416 | `					goto done;` |
|         - | 11417 | `				}` |
|         - | 11418 | `			}` |
|   1344209 | 11419 | `		}else{` |
|         - | 11420 | `			/* Attribute declaration */` |
|       ! 0 | 11421 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11422 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11423 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11424 | `					return SXERR_ABORT;` |
|         - | 11425 | `				}` |
|       ! 0 | 11426 | `				goto done;` |
|         - | 11427 | `			}` |
|         - | 11428 | `		}` |
|         5 | 11429 | `	}` |
|         - | 11430 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|         - | 11431 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|         - | 11432 | `	 */` |
|         - | 11433 | `	{` |
|         - | 11434 | `		TraitUseEntry *apUse;` |
|         - | 11435 | `		sxu32 nU;` |
|    386801 | 11436 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|    402433 | 11437 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|     15637 | 11438 | `			TraitUseEntry *pUse = &apUse[nU];` |
|     15637 | 11439 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|     15637 | 11440 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|     15637 | 11441 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|         - | 11442 | `			sxu32 nT;` |
|     15637 | 11443 | `			if( !hasResolution ){` |
|         - | 11444 | `				/* No conflict resolution block: use standard trait application */` |
|     31255 | 11445 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|     15633 | 11446 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|     15633 | 11447 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11448 | `						break;` |
|         - | 11449 | `					}` |
|      7819 | 11450 | `				}` |
|      7816 | 11451 | `			}else{` |
|         - | 11452 | `				/* With resolution block: copy attributes, record traits,` |
|         - | 11453 | `				 * then use the block to resolve method conflicts.` |
|         - | 11454 | `				 */` |
|         - | 11455 | `				SyToken *pR;` |
|        24 | 11456 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        14 | 11457 | `					ph7_class *pTR = apTrait[nT];` |
|         - | 11458 | `					ph7_class_attr *pAR;` |
|         - | 11459 | `					SyHashEntry *pER;` |
|         - | 11460 | `					SyString *pNR;` |
|        14 | 11461 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|        20 | 11462 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|       ! 0 | 11463 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|       ! 0 | 11464 | `						pNR = &pAR->sName;` |
|       ! 0 | 11465 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       ! 0 | 11466 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|       ! 0 | 11467 | `						}` |
|       ! 0 | 11468 | `					}` |
|        14 | 11469 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|         8 | 11470 | `				}` |
|         - | 11471 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|        12 | 11472 | `				pR = pUse->pResolvStart;` |
|        26 | 11473 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 11474 | `					SyString sTrait,sMethod;` |
|         - | 11475 | `					ph7_class *pSrcTrait;` |
|         - | 11476 | `					ph7_class_method *pMeth;` |
|         - | 11477 | `					sxi32 nRKwrd;` |
|        40 | 11478 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        26 | 11479 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        16 | 11480 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        16 | 11481 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        16 | 11482 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        16 | 11483 | `					sMethod = pR->sData;` |
|        16 | 11484 | `					pR++;` |
|        16 | 11485 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|         7 | 11486 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|         7 | 11487 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|         7 | 11488 | `							sTrait = sMethod;` |
|         7 | 11489 | `							pR++;` |
|         7 | 11490 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|         7 | 11491 | `							sMethod = pR->sData;` |
|         7 | 11492 | `							pR++;` |
|         3 | 11493 | `						}` |
|         3 | 11494 | `					}` |
|        16 | 11495 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 11496 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 11497 | `						continue;` |
|         - | 11498 | `					}` |
|        16 | 11499 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        16 | 11500 | `					pR++;` |
|        16 | 11501 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|         5 | 11502 | `						pSrcTrait = 0;` |
|         7 | 11503 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         7 | 11504 | `							SyString *pTN = &apTrait[nT]->sName;` |
|        10 | 11505 | `							if( pTN->nByte >= sTrait.nByte &&` |
|         6 | 11506 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         5 | 11507 | `								pSrcTrait = apTrait[nT];` |
|         5 | 11508 | `								break;` |
|         - | 11509 | `							}` |
|         2 | 11510 | `						}` |
|         5 | 11511 | `						if( pSrcTrait ){` |
|         5 | 11512 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         5 | 11513 | `							if( pMeth ){` |
|         5 | 11514 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|         5 | 11515 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|         5 | 11516 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|         2 | 11517 | `								}` |
|         2 | 11518 | `							}` |
|         2 | 11519 | `						}` |
|         2 | 11520 | `					}` |
|        34 | 11521 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         2 | 11522 | `				}` |
|         - | 11523 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|        24 | 11524 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         - | 11525 | `					ph7_class_method *pMR;` |
|         - | 11526 | `					SyHashEntry *pER;` |
|         - | 11527 | `					SyString *pNR;` |
|        14 | 11528 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|        40 | 11529 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|        22 | 11530 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|        22 | 11531 | `						pNR = &pMR->sFunc.sName;` |
|        22 | 11532 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|        14 | 11533 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|         6 | 11534 | `						}` |
|         2 | 11535 | `					}` |
|         8 | 11536 | `				}` |
|         - | 11537 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|        12 | 11538 | `				pR = pUse->pResolvStart;` |
|        26 | 11539 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 11540 | `					SyString sTrait,sMethod,sAlias;` |
|         - | 11541 | `					ph7_class *pSrcTrait;` |
|         - | 11542 | `					ph7_class_method *pMeth;` |
|        26 | 11543 | `					int hasQual = 0;` |
|         - | 11544 | `					sxi32 nRKwrd;` |
|        40 | 11545 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        26 | 11546 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        16 | 11547 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        16 | 11548 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        16 | 11549 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|        16 | 11550 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        16 | 11551 | `					sMethod = pR->sData;` |
|        16 | 11552 | `					pR++;` |
|        16 | 11553 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|         7 | 11554 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|         7 | 11555 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|         7 | 11556 | `							sTrait = sMethod;` |
|         7 | 11557 | `							hasQual = 1;` |
|         7 | 11558 | `							pR++;` |
|         7 | 11559 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|         7 | 11560 | `							sMethod = pR->sData;` |
|         7 | 11561 | `							pR++;` |
|         3 | 11562 | `						}` |
|         3 | 11563 | `					}` |
|        16 | 11564 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 11565 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 11566 | `						continue;` |
|         - | 11567 | `					}` |
|        16 | 11568 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        16 | 11569 | `					pR++;` |
|        16 | 11570 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|        12 | 11571 | `						sxi32 iNewVis = -1;` |
|        12 | 11572 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|         7 | 11573 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|         7 | 11574 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|         7 | 11575 | `								iNewVis = nAK;` |
|         7 | 11576 | `								pR++;` |
|         3 | 11577 | `							}` |
|         3 | 11578 | `						}` |
|        12 | 11579 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|        10 | 11580 | `							sAlias = pR->sData;` |
|        10 | 11581 | `							pR++;` |
|         4 | 11582 | `						}` |
|        12 | 11583 | `						pMeth = 0;` |
|        12 | 11584 | `						if( hasQual ){` |
|         3 | 11585 | `							pSrcTrait = 0;` |
|         5 | 11586 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         5 | 11587 | `								SyString *pTN = &apTrait[nT]->sName;` |
|         7 | 11588 | `								if( pTN->nByte >= sTrait.nByte &&` |
|         4 | 11589 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         3 | 11590 | `									pSrcTrait = apTrait[nT];` |
|         3 | 11591 | `									break;` |
|         - | 11592 | `								}` |
|         2 | 11593 | `							}` |
|         3 | 11594 | `							if( pSrcTrait ){` |
|         3 | 11595 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         1 | 11596 | `							}` |
|         2 | 11597 | `						}else{` |
|        10 | 11598 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|         - | 11599 | `						}` |
|        12 | 11600 | `						if( pMeth ){` |
|        12 | 11601 | `							if( sAlias.nByte > 0 ){` |
|         - | 11602 | `								/* Create a shallow copy of the method struct for the alias` |
|         - | 11603 | `								 * so it can carry its own visibility without affecting the original.` |
|         - | 11604 | `								 */` |
|         - | 11605 | `								ph7_class_method *pAlias;` |
|         - | 11606 | `								char *zAliasDup;` |
|        10 | 11607 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        10 | 11608 | `								if( pAlias ){` |
|        10 | 11609 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|        10 | 11610 | `									if( iNewVis >= 0 ){` |
|         5 | 11611 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 11612 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 11613 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         2 | 11614 | `									}` |
|        10 | 11615 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        10 | 11616 | `									if( zAliasDup ){` |
|        10 | 11617 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|         4 | 11618 | `									}` |
|         6 | 11619 | `								}` |
|         7 | 11620 | `							}else if( iNewVis >= 0 ){` |
|         - | 11621 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|         - | 11622 | `								ph7_class_method *pCopy;` |
|         3 | 11623 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|         3 | 11624 | `								if( pCopy ){` |
|         3 | 11625 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|         3 | 11626 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|         3 | 11627 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 11628 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 11629 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         - | 11630 | `									/* Replace the method in the class hash */` |
|         3 | 11631 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|         3 | 11632 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|         1 | 11633 | `								}` |
|         1 | 11634 | `							}` |
|         5 | 11635 | `						}` |
|         5 | 11636 | `						SXUNUSED(hasQual);` |
|         5 | 11637 | `					}` |
|        20 | 11638 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         2 | 11639 | `				}` |
|         - | 11640 | `			}` |
|     15637 | 11641 | `			SySetRelease(&pUse->aTraits);` |
|      7821 | 11642 | `		}` |
|         - | 11643 | `	}` |
|    386801 | 11644 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|         - | 11645 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|         - | 11646 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|      3921 | 11647 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|      3921 | 11648 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11649 | `			SySetRelease(&aUseEntries);` |
|       ! 0 | 11650 | `			SySetRelease(&aInterfaces);` |
|       ! 0 | 11651 | `			return SXERR_ABORT;` |
|         - | 11652 | `		}` |
|      1958 | 11653 | `	}` |
|         - | 11654 | `	/* Install the class */` |
|    386801 | 11655 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    386801 | 11656 | `	if( rc == SXRET_OK ){` |
|         - | 11657 | `		ph7_class **apInterface;` |
|         - | 11658 | `		sxu32 n;` |
|    386801 | 11659 | `		if( pBase ){` |
|         - | 11660 | `			/* Inherit from base class and mark as a subclass */` |
|    206355 | 11661 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|    103175 | 11662 | `		}` |
|    386801 | 11663 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|    573629 | 11664 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|         - | 11665 | `			/* Implements one or more interface */` |
|    186833 | 11666 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    186833 | 11667 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11668 | `				break;` |
|         - | 11669 | `			}` |
|     93419 | 11670 | `		}` |
|         - | 11671 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|         - | 11672 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|    386801 | 11673 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|      3921 | 11674 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|      3921 | 11675 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 11676 | `				pIntf = pIntf->pNextName;` |
|       ! 0 | 11677 | `			}` |
|      3921 | 11678 | `			if( pIntf ){` |
|      3921 | 11679 | `				PH7_ClassImplement(pClass,pIntf);` |
|      1958 | 11680 | `			}` |
|      3921 | 11681 | `			if( pClass->nEnumBacking != 0 ){` |
|      3907 | 11682 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|      3907 | 11683 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 11684 | `					pIntf = pIntf->pNextName;` |
|       ! 0 | 11685 | `				}` |
|      3907 | 11686 | `				if( pIntf ){` |
|      3907 | 11687 | `					PH7_ClassImplement(pClass,pIntf);` |
|      1951 | 11688 | `				}` |
|      1951 | 11689 | `			}` |
|      1958 | 11690 | `		}` |
|         - | 11691 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|         - | 11692 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|    386796 | 11693 | `		if( rc == SXRET_OK` |
|    386796 | 11694 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|    386801 | 11695 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|    213951 | 11696 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|         - | 11697 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|    213951 | 11698 | `			if( pStringable ){` |
|    213951 | 11699 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    213951 | 11700 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|         - | 11701 | `				sxu32 i;` |
|    213951 | 11702 | `				int bAlready = 0;` |
|    260611 | 11703 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|     58331 | 11704 | `					if( apImpl[i] == pStringable ){` |
|     11671 | 11705 | `						bAlready = 1;` |
|     11671 | 11706 | `						break;` |
|         - | 11707 | `					}` |
|     23335 | 11708 | `				}` |
|    213951 | 11709 | `				if( !bAlready ){` |
|    202285 | 11710 | `					PH7_ClassImplement(pClass,pStringable);` |
|    101140 | 11711 | `				}` |
|    106973 | 11712 | `			}` |
|    106973 | 11713 | `		}` |
|         - | 11714 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|    386801 | 11715 | `		if( rc == SXRET_OK ){` |
|    386801 | 11716 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|    386801 | 11717 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 11718 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 11719 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 11720 | `				return SXERR_ABORT;` |
|         - | 11721 | `			}` |
|    193398 | 11722 | `		}` |
|         - | 11723 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|    386801 | 11724 | `		if( rc == SXRET_OK ){` |
|    386801 | 11725 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|    386801 | 11726 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 11727 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 11728 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 11729 | `				return SXERR_ABORT;` |
|         - | 11730 | `			}` |
|    193398 | 11731 | `		}` |
|    193398 | 11732 | `	}` |
|    386801 | 11733 | `	SySetRelease(&aUseEntries);` |
|    386801 | 11734 | `	SySetRelease(&aInterfaces);` |
|    386801 | 11735 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11736 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11737 | `		return SXERR_ABORT;` |
|         - | 11738 | `	}` |
|    193398 | 11739 | `done:` |
|         - | 11740 | `	/* Point beyond the class body */` |
|    386843 | 11741 | `	pGen->pIn = &pEnd[1];` |
|    386843 | 11742 | `	pGen->pEnd = pTmp;` |
|    386843 | 11743 | `	return PH7_OK;` |
|    193425 | 11744 | `}` |
|         - | 11745 | `/* Compile a named class declaration (the common case). */` |
|    386810 | 11746 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|         5 | 11747 | `{` |
|    386815 | 11748 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|         5 | 11749 | `}` |
|         - | 11750 | `/*` |
|         - | 11751 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|         - | 11752 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|         - | 11753 | ` * compile + install the class body once (at compile time, like every other` |
|         - | 11754 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|         - | 11755 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|         - | 11756 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|         - | 11757 | ` */` |
|        30 | 11758 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         4 | 11759 | `{` |
|         - | 11760 | `	char zName[128];         /* Synthesized class name */` |
|         - | 11761 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|         - | 11762 | `	SyString sName;` |
|         - | 11763 | `	SyToken *pArgStart,*pArgEnd;` |
|        34 | 11764 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|         - | 11765 | `	                              * is keyed to this 'class' token */` |
|         - | 11766 | `	ph7_value *pObj;` |
|        34 | 11767 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11768 | `	sxu32 nIdx,nLen;` |
|         - | 11769 | `	sxi32 nArg,rc;` |
|        15 | 11770 | `	SXUNUSED(iCompileFlag);` |
|         - | 11771 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|        34 | 11772 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|        34 | 11773 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 | 11774 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|       ! 0 | 11775 | `	}` |
|        34 | 11776 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|         - | 11777 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|         - | 11778 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|         - | 11779 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|        34 | 11780 | `	pArgStart = pArgEnd = 0;` |
|        34 | 11781 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|        34 | 11782 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11783 | `		return rc;` |
|         - | 11784 | `	}` |
|         - | 11785 | `	{` |
|         - | 11786 | ``		/* Expression-position attributes (`new #[A] class {…}`) */`` |
|        34 | 11787 | `		ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0);` |
|        30 | 11788 | `		if( pAnonClass` |
|        34 | 11789 | `		 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 11790 | `			return SXERR_ABORT;` |
|         - | 11791 | `		}` |
|         - | 11792 | `	}` |
|         - | 11793 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|         - | 11794 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|        34 | 11795 | `	nArg = 0;` |
|        34 | 11796 | `	if( pArgStart < pArgEnd ){` |
|         7 | 11797 | `		SyToken *pSavedIn = pGen->pIn;` |
|         7 | 11798 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|         - | 11799 | `		SyToken *pArgNext;` |
|         7 | 11800 | `		pGen->pIn = pArgStart;` |
|         7 | 11801 | `		pGen->pEnd = pArgEnd;` |
|        13 | 11802 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|         7 | 11803 | `			if( pGen->pIn < pArgNext ){` |
|         7 | 11804 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|         7 | 11805 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11806 | `					pGen->pIn = pSavedIn;` |
|       ! 0 | 11807 | `					pGen->pEnd = pSavedEnd;` |
|       ! 0 | 11808 | `					return SXERR_ABORT;` |
|         - | 11809 | `				}` |
|         7 | 11810 | `				nArg++;` |
|         3 | 11811 | `			}` |
|         7 | 11812 | `			pGen->pIn = &pArgNext[1];` |
|         1 | 11813 | `		}` |
|         7 | 11814 | `		pGen->pIn = pSavedIn;` |
|         7 | 11815 | `		pGen->pEnd = pSavedEnd;` |
|         3 | 11816 | `	}` |
|         - | 11817 | `	/* Load the synthesized class name */` |
|        34 | 11818 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        34 | 11819 | `	if( pObj == 0 ){` |
|       ! 0 | 11820 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 11821 | `		return SXERR_ABORT;` |
|         - | 11822 | `	}` |
|        34 | 11823 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|        34 | 11824 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - | 11825 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|        34 | 11826 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        34 | 11827 | `	return SXRET_OK;` |
|        19 | 11828 | `}` |
|         - | 11829 | `/*` |
|         - | 11830 | ` * Compile a user-defined abstract class.` |
|         - | 11831 | ` *  According to the PHP language reference manual` |
|         - | 11832 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|         - | 11833 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|         - | 11834 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|         - | 11835 | ` *   the method's signature - they cannot define the implementation.` |
|         - | 11836 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|         - | 11837 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|         - | 11838 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|         - | 11839 | ` *   method is defined as protected, the function implementation must be defined as either` |
|         - | 11840 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|         - | 11841 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|         - | 11842 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|         - | 11843 | ` *   could differ.` |
|         - | 11844 | ` */` |
|         - | 11845 | `/*` |
|         - | 11846 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|         - | 11847 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|         - | 11848 | ` * receives the corresponding PH7_CLASS_* bit.` |
|         - | 11849 | ` */` |
|  13664960 | 11850 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|         5 | 11851 | `{` |
|  13664965 | 11852 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|   8012283 | 11853 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|   8012283 | 11854 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|   7957805 | 11855 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|   3959407 | 11856 | `	}` |
|  13571501 | 11857 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  13571441 | 11858 | `	return FALSE;` |
|   6832485 | 11859 | `}` |
|         - | 11860 | `/*` |
|         - | 11861 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|         - | 11862 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|         - | 11863 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|         - | 11864 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|         - | 11865 | ` */` |
|  13571436 | 11866 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|         5 | 11867 | `{` |
|  13571441 | 11868 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  13571441 | 11869 | `	sxi32 iFlags = 0,iFlag;` |
|  13664965 | 11870 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|     93529 | 11871 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|         5 | 11872 | `			pDup = pIn;` |
|         2 | 11873 | `		}` |
|     93529 | 11874 | `		iFlags \|= iFlag;` |
|     93529 | 11875 | `		pIn++;` |
|         5 | 11876 | `	}` |
|  13571441 | 11877 | `	*ppIn = pIn;` |
|  13571441 | 11878 | `	if( ppDup ){ *ppDup = pDup; }` |
|  13571441 | 11879 | `	return iFlags;` |
|         5 | 11880 | `}` |
|         - | 11881 | `/*` |
|         - | 11882 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|         - | 11883 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|         - | 11884 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|         - | 11885 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|         - | 11886 | `` * `readonly`) to their existing handlers.`` |
|         - | 11887 | ` */` |
|  13528572 | 11888 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|         5 | 11889 | `{` |
|  13528577 | 11890 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|   6814933 | 11891 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  13553894 | 11892 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|         5 | 11893 | `}` |
|         - | 11894 | `/*` |
|         - | 11895 | ` * Compile a class declaration carrying one or more leading modifiers` |
|         - | 11896 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|         - | 11897 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|         - | 11898 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|         - | 11899 | `` * `abstract`+`final` pair, like PHP.`` |
|         - | 11900 | ` */` |
|     42864 | 11901 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|         5 | 11902 | `{` |
|         - | 11903 | `	SyToken *pDup;` |
|     42869 | 11904 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|         - | 11905 | `	sxi32 rc;` |
|     42869 | 11906 | `	if( pDup ){` |
|         4 | 11907 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|         2 | 11908 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|         3 | 11909 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11910 | `			return SXERR_ABORT;` |
|         - | 11911 | `		}` |
|         1 | 11912 | `	}` |
|     42864 | 11913 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|     21437 | 11914 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|         3 | 11915 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11916 | `			"Cannot use the final modifier on an abstract class");` |
|         3 | 11917 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11918 | `			return SXERR_ABORT;` |
|         - | 11919 | `		}` |
|         1 | 11920 | `	}` |
|     42869 | 11921 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|     21437 | 11922 | `}` |
|         - | 11923 | `/*` |
|         - | 11924 | ` * Compile a user-defined trait.` |
|         - | 11925 | ` *  Traits are similar to classes, but only intended to group functionality` |
|         - | 11926 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|         - | 11927 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|         - | 11928 | ` */` |
|      7864 | 11929 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|         5 | 11930 | `{` |
|      7869 | 11931 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11932 | `	ph7_class *pClass;` |
|         - | 11933 | `	SyToken *pEnd,*pTmp;` |
|         - | 11934 | `	sxi32 iProtection;` |
|         - | 11935 | `	sxi32 iAttrflags;` |
|         - | 11936 | `	SyString *pName;` |
|         - | 11937 | `	sxi32 nKwrd;` |
|         - | 11938 | `	sxi32 rc;` |
|         - | 11939 | `	/* Jump the 'trait' keyword */` |
|      7869 | 11940 | `	pGen->pIn++;` |
|      7869 | 11941 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 11942 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|       ! 0 | 11943 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11944 | `			return SXERR_ABORT;` |
|         - | 11945 | `		}` |
|       ! 0 | 11946 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|       ! 0 | 11947 | `			pGen->pIn++;` |
|       ! 0 | 11948 | `		}` |
|       ! 0 | 11949 | `		return SXRET_OK;` |
|         - | 11950 | `	}` |
|         - | 11951 | `	/* Extract trait name */` |
|      7869 | 11952 | `	pName = &pGen->pIn->sData;` |
|      7869 | 11953 | `	pGen->pIn++;` |
|         - | 11954 | `	/* Build FQN and obtain a raw class */ {` |
|         - | 11955 | `		SyBlob sFQN;` |
|         - | 11956 | `		SyString sFQNStr;` |
|      7869 | 11957 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7869 | 11958 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      7869 | 11959 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      7869 | 11960 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      7869 | 11961 | `		SyBlobRelease(&sFQN);` |
|         - | 11962 | `	}` |
|      7869 | 11963 | `	if( pClass == 0 ){` |
|       ! 0 | 11964 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11965 | `		return SXERR_ABORT;` |
|         - | 11966 | `	}` |
|      7869 | 11967 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|      7869 | 11968 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 11969 | `		return SXERR_ABORT;` |
|         - | 11970 | `	}` |
|         - | 11971 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      7869 | 11972 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|       ! 0 | 11973 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|       ! 0 | 11974 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 11975 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11976 | `			return SXERR_ABORT;` |
|         - | 11977 | `		}` |
|       ! 0 | 11978 | `		return SXRET_OK;` |
|         - | 11979 | `	}` |
|      7869 | 11980 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      7869 | 11981 | `	pEnd = 0;` |
|      7869 | 11982 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      7869 | 11983 | `	if( pEnd >= pGen->pEnd ){` |
|       ! 0 | 11984 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|       ! 0 | 11985 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 11986 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11987 | `			return SXERR_ABORT;` |
|         - | 11988 | `		}` |
|       ! 0 | 11989 | `		return SXRET_OK;` |
|         - | 11990 | `	}` |
|         - | 11991 | `	/* The delimiter token is the trait body's closing brace */` |
|      7869 | 11992 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 11993 | `	/* Swap token stream */` |
|      7869 | 11994 | `	pTmp = pGen->pEnd;` |
|      7869 | 11995 | `	pGen->pEnd = pEnd;` |
|         - | 11996 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|      7869 | 11997 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|         - | 11998 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|     56466 | 11999 | `	for(;;){` |
|    159647 | 12000 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     23363 | 12001 | `			pGen->pIn++;` |
|         5 | 12002 | `		}` |
|    136289 | 12003 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      7869 | 12004 | `			break;` |
|         - | 12005 | `		}` |
|         - | 12006 | `		/* Bind a directly-preceding docblock to this member */` |
|    128425 | 12007 | `		GenStateSetPendingDoc(&(*pGen));` |
|    128425 | 12008 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|       ! 0 | 12009 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12010 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 12011 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 12012 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12013 | `				return SXERR_ABORT;` |
|         - | 12014 | `			}` |
|       ! 0 | 12015 | `			goto done;` |
|         - | 12016 | `		}` |
|    128425 | 12017 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|    128425 | 12018 | `		iAttrflags = 0;` |
|    128425 | 12019 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|    128425 | 12020 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    128425 | 12021 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 12022 | `				/* Trait uses another trait: use OtherTrait; */` |
|         5 | 12023 | `				pGen->pIn++; /* Jump 'use' */` |
|         2 | 12024 | `				for(;;){` |
|         - | 12025 | `					ph7_class *pUsedTrait;` |
|         - | 12026 | `					SyString *pUsedName;` |
|         5 | 12027 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 12028 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 12029 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|       ! 0 | 12030 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12031 | `							return SXERR_ABORT;` |
|         - | 12032 | `						}` |
|       ! 0 | 12033 | `						break;` |
|         - | 12034 | `					}` |
|         5 | 12035 | `					pUsedName = &pGen->pIn->sData;` |
|         - | 12036 | `					{` |
|         - | 12037 | `						SyBlob sResolved;` |
|         5 | 12038 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|         5 | 12039 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|         7 | 12040 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|         4 | 12041 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|         5 | 12042 | `						SyBlobRelease(&sResolved);` |
|         - | 12043 | `					}` |
|         5 | 12044 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 12045 | `						pUsedTrait = pUsedTrait->pNextName;` |
|       ! 0 | 12046 | `					}` |
|         5 | 12047 | `					if( pUsedTrait == 0 ){` |
|         4 | 12048 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         1 | 12049 | `							"'%z' is not a trait",pUsedName);` |
|         3 | 12050 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12051 | `							return SXERR_ABORT;` |
|         - | 12052 | `						}` |
|         2 | 12053 | `					}else{` |
|         3 | 12054 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|         - | 12055 | `					}` |
|         5 | 12056 | `					pGen->pIn++;` |
|         5 | 12057 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|         3 | 12058 | `						break;` |
|         - | 12059 | `					}` |
|       ! 0 | 12060 | `					pGen->pIn++;` |
|       ! 0 | 12061 | `				}` |
|         5 | 12062 | `				continue;` |
|         - | 12063 | `			}` |
|    128421 | 12064 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|    128403 | 12065 | `				iProtection = nKwrd;` |
|    128403 | 12066 | `				pGen->pIn++;` |
|         - | 12067 | ``				/* Optional `readonly` after the visibility (PHP 8.1): `private readonly T`` |
|         - | 12068 | ``				 * $x` — the generated PHPUnit runtime traits (StubApi, …) declare their`` |
|         - | 12069 | `				 * readonly state this way. Mirrors the class-body attribute parser. */` |
|    128403 | 12070 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|         3 | 12071 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|         3 | 12072 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|         1 | 12073 | `				}` |
|    128398 | 12074 | `				if( pGen->pIn >= pGen->pEnd` |
|    128403 | 12075 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 12076 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12077 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 12078 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 12079 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12080 | `						return SXERR_ABORT;` |
|         - | 12081 | `					}` |
|       ! 0 | 12082 | `					goto done;` |
|         - | 12083 | `				}` |
|    128403 | 12084 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     23345 | 12085 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     23345 | 12086 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 12087 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12088 | `							return SXERR_ABORT;` |
|         - | 12089 | `						}` |
|       ! 0 | 12090 | `						goto done;` |
|         - | 12091 | `					}` |
|     23345 | 12092 | `					continue;` |
|         - | 12093 | `				}` |
|    105063 | 12094 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         7 | 12095 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         7 | 12096 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 12097 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12098 | `							return SXERR_ABORT;` |
|         - | 12099 | `						}` |
|       ! 0 | 12100 | `						goto done;` |
|         - | 12101 | `					}` |
|         7 | 12102 | `					continue;` |
|         - | 12103 | `				}` |
|    105057 | 12104 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     52526 | 12105 | `			}` |
|    105075 | 12106 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       ! 0 | 12107 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12108 | `					"Traits cannot have constants");` |
|       ! 0 | 12109 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12110 | `					return SXERR_ABORT;` |
|         - | 12111 | `				}` |
|       ! 0 | 12112 | `				goto done;` |
|       ! 0 | 12113 | `			}else{` |
|    105075 | 12114 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|      7791 | 12115 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      7791 | 12116 | `					pGen->pIn++;` |
|      7791 | 12117 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      7789 | 12118 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      7789 | 12119 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 12120 | `							iProtection = nKwrd;` |
|       ! 0 | 12121 | `							pGen->pIn++;` |
|       ! 0 | 12122 | `						}` |
|      3892 | 12123 | `					}` |
|      7786 | 12124 | `					if( pGen->pIn >= pGen->pEnd` |
|      7791 | 12125 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 12126 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12127 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|       ! 0 | 12128 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 12129 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12130 | `							return SXERR_ABORT;` |
|         - | 12131 | `						}` |
|       ! 0 | 12132 | `						goto done;` |
|         - | 12133 | `					}` |
|      7791 | 12134 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         3 | 12135 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         3 | 12136 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 12137 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 12138 | `								return SXERR_ABORT;` |
|         - | 12139 | `							}` |
|       ! 0 | 12140 | `							goto done;` |
|         - | 12141 | `						}` |
|         3 | 12142 | `						continue;` |
|         - | 12143 | `					}` |
|      7789 | 12144 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       ! 0 | 12145 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 12146 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 12147 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 12148 | `								return SXERR_ABORT;` |
|         - | 12149 | `							}` |
|       ! 0 | 12150 | `							goto done;` |
|         - | 12151 | `						}` |
|       ! 0 | 12152 | `						continue;` |
|         - | 12153 | `					}` |
|      7789 | 12154 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    101181 | 12155 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         9 | 12156 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         9 | 12157 | `					pGen->pIn++;` |
|         9 | 12158 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         9 | 12159 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         9 | 12160 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         9 | 12161 | `							iProtection = nKwrd;` |
|         9 | 12162 | `							pGen->pIn++;` |
|         3 | 12163 | `						}` |
|         3 | 12164 | `					}` |
|         9 | 12165 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         6 | 12166 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 12167 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12168 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|       ! 0 | 12169 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 12170 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12171 | `							return SXERR_ABORT;` |
|         - | 12172 | `						}` |
|       ! 0 | 12173 | `						goto done;` |
|         - | 12174 | `					}` |
|         9 | 12175 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         3 | 12176 | `				}` |
|    105073 | 12177 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 12178 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12179 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|       ! 0 | 12180 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 12181 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12182 | `						return SXERR_ABORT;` |
|         - | 12183 | `					}` |
|       ! 0 | 12184 | `					goto done;` |
|         - | 12185 | `				}` |
|    105073 | 12186 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       ! 0 | 12187 | `					pGen->pIn++;` |
|       ! 0 | 12188 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 | 12189 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 12190 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 12191 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 12192 | `							return SXERR_ABORT;` |
|         - | 12193 | `						}` |
|       ! 0 | 12194 | `						goto done;` |
|         - | 12195 | `					}` |
|       ! 0 | 12196 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 12197 | `				}else{` |
|    105073 | 12198 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 12199 | `				}` |
|    105073 | 12200 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 12201 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12202 | `						return SXERR_ABORT;` |
|         - | 12203 | `					}` |
|       ! 0 | 12204 | `					goto done;` |
|         - | 12205 | `				}` |
|         - | 12206 | `			}` |
|     52539 | 12207 | `		}else{` |
|       ! 0 | 12208 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 12209 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 12210 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12211 | `					return SXERR_ABORT;` |
|         - | 12212 | `				}` |
|       ! 0 | 12213 | `				goto done;` |
|         - | 12214 | `			}` |
|         - | 12215 | `		}` |
|         5 | 12216 | `	}` |
|         - | 12217 | `	/* Install the trait */` |
|      7869 | 12218 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      7869 | 12219 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12220 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 12221 | `		return SXERR_ABORT;` |
|         - | 12222 | `	}` |
|      3932 | 12223 | `done:` |
|         - | 12224 | `	/* Point beyond the trait body */` |
|      7869 | 12225 | `	pGen->pIn = &pEnd[1];` |
|      7869 | 12226 | `	pGen->pEnd = pTmp;` |
|      7869 | 12227 | `	return PH7_OK;` |
|      3937 | 12228 | `}` |
|         - | 12229 | `/*` |
|         - | 12230 | ` * Compile a user-defined class.` |
|         - | 12231 | ` *  According to the PHP language reference manual` |
|         - | 12232 | ` *   Basic class definitions begin with the keyword class, followed` |
|         - | 12233 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|         - | 12234 | ` *   the definitions of the properties and methods belonging to the class.` |
|         - | 12235 | ` *   A class may contain its own constants, variables (called "properties")` |
|         - | 12236 | ` *   and functions (called "methods").` |
|         - | 12237 | ` */` |
|    340026 | 12238 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|         5 | 12239 | `{` |
|         - | 12240 | `	sxi32 rc;` |
|    340031 | 12241 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|    340031 | 12242 | `	return rc;` |
|         5 | 12243 | `}` |
|         - | 12244 | `/*` |
|         - | 12245 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|         - | 12246 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|         - | 12247 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|         - | 12248 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|         - | 12249 | `` * meaning; `enum Name` can never start a valid expression.`` |
|         - | 12250 | ` */` |
|  13477932 | 12251 | `static int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|         5 | 12252 | `{` |
|  13688587 | 12253 | `	return (pIn->nType & PH7_TK_ID)` |
|   6949616 | 12254 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|    220508 | 12255 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
|  13688582 | 12256 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|         5 | 12257 | `}` |
|         - | 12258 | `/*` |
|         - | 12259 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|         - | 12260 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|         - | 12261 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|         - | 12262 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|         - | 12263 | ` */` |
|      3920 | 12264 | `static sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|         5 | 12265 | `{` |
|      3925 | 12266 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|         5 | 12267 | `}` |
|         - | 12268 | `/*` |
|         - | 12269 | ` * Exception handling.` |
|         - | 12270 | ` *  According to the PHP language reference manual` |
|         - | 12271 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|         - | 12272 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|         - | 12273 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|         - | 12274 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|         - | 12275 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|         - | 12276 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|         - | 12277 | ` *    (or re-thrown) within a catch block.` |
|         - | 12278 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|         - | 12279 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|         - | 12280 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|         - | 12281 | ` *    been defined with set_exception_handler().` |
|         - | 12282 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|         - | 12283 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|         - | 12284 | ` */` |
|         - | 12285 | `/*` |
|         - | 12286 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|         - | 12287 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|         - | 12288 | ` * indicates failure.` |
|         - | 12289 | ` */` |
|    517528 | 12290 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 | 12291 | `{` |
|    517533 | 12292 | `	sxi32 rc = SXRET_OK;` |
|    517533 | 12293 | `	if( pRoot->pOp ){` |
|    517521 | 12294 | `		switch( pRoot->pOp->iOp ){` |
|    258758 | 12295 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|         - | 12296 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|         - | 12297 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|         - | 12298 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|         - | 12299 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|         - | 12300 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|    517521 | 12301 | `			break;` |
|       ! 0 | 12302 | `		default:` |
|         - | 12303 | `			/* Runtime will still reject non-Throwable values; the set above` |
|         - | 12304 | `			 * covers the common shapes and gives a friendlier compile error` |
|         - | 12305 | ``			 * for obvious mistakes like `throw 5`. */`` |
|       ! 0 | 12306 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - | 12307 | `				"throw: Expecting an exception class instance");` |
|       ! 0 | 12308 | `			if( rc != SXERR_ABORT ){` |
|       ! 0 | 12309 | `				rc = SXERR_INVALID;` |
|       ! 0 | 12310 | `			}` |
|       ! 0 | 12311 | `			break;` |
|         - | 12312 | `		}` |
|    258775 | 12313 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - | 12314 | `		/* Unexpected expression */` |
|       ! 0 | 12315 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - | 12316 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12317 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 | 12318 | `			rc = SXERR_INVALID;` |
|       ! 0 | 12319 | `		}` |
|       ! 0 | 12320 | `	}` |
|    517533 | 12321 | `	return rc;` |
|         5 | 12322 | `}` |
|         - | 12323 | `/*` |
|         - | 12324 | ` * Compile a 'throw' statement.` |
|         - | 12325 | ` * throw: This is how you trigger an exception.` |
|         - | 12326 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|         - | 12327 | ` */` |
|    517492 | 12328 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|         5 | 12329 | `{` |
|    517497 | 12330 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12331 | `	GenBlock *pBlock;` |
|         - | 12332 | `	sxu32 nIdx;` |
|         - | 12333 | `	sxi32 rc;` |
|    517497 | 12334 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|         - | 12335 | `	/* Compile the expression */` |
|    517497 | 12336 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    517497 | 12337 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 | 12338 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|       ! 0 | 12339 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12340 | `			return SXERR_ABORT;` |
|         - | 12341 | `		}` |
|       ! 0 | 12342 | `		return SXRET_OK;` |
|         - | 12343 | `	}` |
|    517497 | 12344 | `	pBlock = pGen->pCurrent;` |
|         - | 12345 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   2061441 | 12346 | `	while(pBlock->pParent){` |
|   2061437 | 12347 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    517493 | 12348 | `			break;` |
|         - | 12349 | `		}` |
|         - | 12350 | `		/* Point to the parent block */` |
|   1543949 | 12351 | `		pBlock = pBlock->pParent;` |
|         5 | 12352 | `	}` |
|         - | 12353 | `	/* Emit the throw instruction */` |
|    517497 | 12354 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|         - | 12355 | `	/* Emit the jump */` |
|    517497 | 12356 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    517497 | 12357 | `	return SXRET_OK;` |
|    258751 | 12358 | `}` |
|         - | 12359 | `/*` |
|         - | 12360 | ` * Compile a PHP 8.0 'throw' expression.` |
|         - | 12361 | ` * Called from the expression code generator when a 'throw' keyword is` |
|         - | 12362 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|         - | 12363 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|         - | 12364 | ` * the validator guarantees the operand is a valid exception target.` |
|         - | 12365 | ` */` |
|        36 | 12366 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|         2 | 12367 | `{` |
|        38 | 12368 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12369 | `	GenBlock *pBlock;` |
|         - | 12370 | `	sxu32 nIdx;` |
|         - | 12371 | `	sxi32 rc;` |
|        18 | 12372 | `	(void)iCompileFlag;` |
|        38 | 12373 | `	pGen->pIn++; /* Skip 'throw' */` |
|        38 | 12374 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 | 12375 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12376 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12377 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12378 | `			return SXERR_ABORT;` |
|         - | 12379 | `		}` |
|       ! 0 | 12380 | `		return SXRET_OK;` |
|         - | 12381 | `	}` |
|        38 | 12382 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|        38 | 12383 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12384 | `		return SXERR_ABORT;` |
|         - | 12385 | `	}` |
|        38 | 12386 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 | 12387 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12388 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 12389 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12390 | `			return SXERR_ABORT;` |
|         - | 12391 | `		}` |
|       ! 0 | 12392 | `		return SXRET_OK;` |
|         - | 12393 | `	}` |
|         - | 12394 | `	/* Walk up to nearest exception/function block for the jump target */` |
|        38 | 12395 | `	pBlock = pGen->pCurrent;` |
|        60 | 12396 | `	while( pBlock->pParent ){` |
|        49 | 12397 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|        27 | 12398 | `			break;` |
|         - | 12399 | `		}` |
|        23 | 12400 | `		pBlock = pBlock->pParent;` |
|         1 | 12401 | `	}` |
|        38 | 12402 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        38 | 12403 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|        38 | 12404 | `	return SXRET_OK;` |
|        20 | 12405 | `}` |
|         - | 12406 | `/*` |
|         - | 12407 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|         - | 12408 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|         - | 12409 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|         - | 12410 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|         - | 12411 | ` * compile error propagated from the parser.` |
|         - | 12412 | ` */` |
|        56 | 12413 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|         5 | 12414 | `{` |
|         - | 12415 | `	SyString sClassName;` |
|         - | 12416 | `	SyToken *pToken;` |
|         - | 12417 | `	SyString *pName;` |
|         - | 12418 | `	char *zDup;` |
|         - | 12419 | `	sxi32 rc;` |
|        61 | 12420 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        61 | 12421 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|        61 | 12422 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|        61 | 12423 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        61 | 12424 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 | 12425 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12426 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12427 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12428 | `		return SXERR_INVALID;` |
|         - | 12429 | `	}` |
|        61 | 12430 | `	pGen->pIn++; /* '(' */` |
|        28 | 12431 | `	for(;;){` |
|         - | 12432 | `		SyBlob sResolved;` |
|        61 | 12433 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|        61 | 12434 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 12435 | `			SyBlobRelease(&sResolved);` |
|       ! 0 | 12436 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12437 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12438 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12439 | `			return SXERR_INVALID;` |
|         - | 12440 | `		}` |
|        89 | 12441 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        56 | 12442 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        61 | 12443 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|        61 | 12444 | `		SyBlobRelease(&sResolved);` |
|        61 | 12445 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|        61 | 12446 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|        61 | 12447 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        56 | 12448 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|         5 | 12449 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       ! 0 | 12450 | `			pGen->pIn++; continue;` |
|         - | 12451 | `		}` |
|        61 | 12452 | `		break;` |
|       ! 0 | 12453 | `	}` |
|         - | 12454 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|         - | 12455 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding. */` |
|        61 | 12456 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|         3 | 12457 | `		pGen->pIn++; /* ')' */` |
|         3 | 12458 | `		return SXRET_OK;` |
|         - | 12459 | `	}` |
|        54 | 12460 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|        59 | 12461 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 12462 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12463 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12464 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12465 | `		return SXERR_INVALID;` |
|         - | 12466 | `	}` |
|        59 | 12467 | `	pGen->pIn++; /* '$' */` |
|        59 | 12468 | `	pName = &pGen->pIn->sData;` |
|        59 | 12469 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|        59 | 12470 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|        59 | 12471 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|        59 | 12472 | `	pGen->pIn++;` |
|        59 | 12473 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 | 12474 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12475 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12476 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12477 | `		return SXERR_INVALID;` |
|         - | 12478 | `	}` |
|        59 | 12479 | `	pGen->pIn++; /* ')' */` |
|        59 | 12480 | `	return SXRET_OK;` |
|        33 | 12481 | `}` |
|         - | 12482 | `/*` |
|         - | 12483 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|         - | 12484 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|         - | 12485 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|         - | 12486 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|         - | 12487 | ` * VmThrowException):` |
|         - | 12488 | ` *` |
|         - | 12489 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|         - | 12490 | ` *    <try body>` |
|         - | 12491 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|         - | 12492 | ` *    JMP  -> finally\|end` |
|         - | 12493 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|         - | 12494 | ` *    <catch body>` |
|         - | 12495 | ` *    JMP  -> finally\|end` |
|         - | 12496 | ` *    ... more catches ...` |
|         - | 12497 | ` *  Lfin: <finally body>` |
|         - | 12498 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|         - | 12499 | ` *  Lend:` |
|         - | 12500 | ` */` |
|       100 | 12501 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|         5 | 12502 | `{` |
|       105 | 12503 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12504 | `	GenBlock *pTry;` |
|         - | 12505 | `	VmInstr *pInstr;` |
|       105 | 12506 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|         - | 12507 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|         - | 12508 | `	sxi32 rc;` |
|       105 | 12509 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|         - | 12510 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|       105 | 12511 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|       105 | 12512 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       105 | 12513 | `	pTry->pUserData = pException;` |
|       105 | 12514 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|       105 | 12515 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       105 | 12516 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       105 | 12517 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       105 | 12518 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       105 | 12519 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|         - | 12520 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|       105 | 12521 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|       105 | 12522 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|       105 | 12523 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       105 | 12524 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12525 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|       105 | 12526 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|         - | 12527 | `	/* Catch clauses (inline) */` |
|       105 | 12528 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       100 | 12529 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        61 | 12530 | `		sxu32 k = 0;` |
|        84 | 12531 | `		for(;;){` |
|         - | 12532 | `			ph7_exception_block sCatch;` |
|         - | 12533 | `			GenBlock *pCatchBlk;` |
|       117 | 12534 | `			sxu32 idxJmp = 0;` |
|       112 | 12535 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|       107 | 12536 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|        33 | 12537 | `				break;` |
|         - | 12538 | `			}` |
|        61 | 12539 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|        61 | 12540 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        61 | 12541 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|        61 | 12542 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|        61 | 12543 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|        61 | 12544 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|        61 | 12545 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|         - | 12546 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|         - | 12547 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|         - | 12548 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|        61 | 12549 | `			pCatchBlk->pUserData = pException;` |
|        61 | 12550 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|        61 | 12551 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        61 | 12552 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|        61 | 12553 | `			GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12554 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|         - | 12555 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|        61 | 12556 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        61 | 12557 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|        61 | 12558 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|        61 | 12559 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|        61 | 12560 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        61 | 12561 | `			k++;` |
|         5 | 12562 | `		}` |
|        28 | 12563 | `	}` |
|         - | 12564 | `	/* Finally (inline) */` |
|       105 | 12565 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        80 | 12566 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|         - | 12567 | `		GenBlock *pFinBlk;` |
|        52 | 12568 | `		pGen->pIn++; /* Jump 'finally' */` |
|        52 | 12569 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|        52 | 12570 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|        52 | 12571 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        52 | 12572 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|        52 | 12573 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        52 | 12574 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|        52 | 12575 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        52 | 12576 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|        52 | 12577 | `		pException->iHasFinally = 1;` |
|        24 | 12578 | `	}` |
|       105 | 12579 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|       105 | 12580 | `	pException->iInlined = 1;` |
|         - | 12581 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|         - | 12582 | `	{` |
|       105 | 12583 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|         - | 12584 | `		sxu32 *aJ; sxu32 n;` |
|       105 | 12585 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|       105 | 12586 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       105 | 12587 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|       161 | 12588 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|        61 | 12589 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|        61 | 12590 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|        33 | 12591 | `		}` |
|         - | 12592 | `	}` |
|       105 | 12593 | `	SySetRelease(&aCatchJmp);` |
|       105 | 12594 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       ! 0 | 12595 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|       ! 0 | 12596 | `	}` |
|       105 | 12597 | `	return SXRET_OK;` |
|        55 | 12598 | `}` |
|         - | 12599 | `/*` |
|         - | 12600 | ` * Compile a 'catch' block.` |
|         - | 12601 | ` * Catch: A "catch" block retrieves an exception and creates` |
|         - | 12602 | ` * an object containing the exception information.` |
|         - | 12603 | ` */` |
|     24876 | 12604 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|         5 | 12605 | `{` |
|     24881 | 12606 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12607 | `	ph7_exception_block sCatch;` |
|         - | 12608 | `	SySet *pInstrContainer;` |
|         - | 12609 | `	SyString sClassName;` |
|         - | 12610 | `	GenBlock *pCatch;` |
|         - | 12611 | `	SyToken *pToken;` |
|         - | 12612 | `	SyString *pName;` |
|         - | 12613 | `	char *zDup;` |
|         - | 12614 | `	sxi32 rc;` |
|     24881 | 12615 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|         - | 12616 | `	/* Zero the structure */` |
|     24881 | 12617 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|         - | 12618 | `	/* Initialize fields */` |
|     24881 | 12619 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     24881 | 12620 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     24881 | 12621 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|         - | 12622 | `			/* Unexpected token,break immediately */` |
|       ! 0 | 12623 | `			pToken = pGen->pIn;` |
|       ! 0 | 12624 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12625 | `				pToken--;` |
|       ! 0 | 12626 | `			}` |
|       ! 0 | 12627 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12628 | `				"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12629 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12630 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12631 | `				return SXERR_ABORT;` |
|         - | 12632 | `			}` |
|       ! 0 | 12633 | `			return SXERR_INVALID;` |
|         - | 12634 | `	}` |
|         - | 12635 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     24881 | 12636 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     12453 | 12637 | `	for(;;){` |
|         - | 12638 | `		SyBlob sResolved;` |
|     24911 | 12639 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     24911 | 12640 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         6 | 12641 | `			SyBlobRelease(&sResolved);` |
|         6 | 12642 | `			pToken = pGen->pIn;` |
|         6 | 12643 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12644 | `				pToken--;` |
|       ! 0 | 12645 | `			}` |
|         8 | 12646 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12647 | `				"syntax error, unexpected %s \"%z\"",` |
|         2 | 12648 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|         6 | 12649 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12650 | `				return SXERR_ABORT;` |
|         - | 12651 | `			}` |
|         6 | 12652 | `			return SXERR_INVALID;` |
|         - | 12653 | `		}` |
|         - | 12654 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|         - | 12655 | `		 * transient SyBlob allocation. */` |
|     37358 | 12656 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     24902 | 12657 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     24907 | 12658 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     24907 | 12659 | `		SyBlobRelease(&sResolved);` |
|     24907 | 12660 | `		if( zDup == 0 ){` |
|       ! 0 | 12661 | `			goto Mem;` |
|         - | 12662 | `		}` |
|     24907 | 12663 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     24907 | 12664 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 12665 | `			goto Mem;` |
|         - | 12666 | `		}` |
|         - | 12667 | `		/* Check for '\|' (multi-catch separator) */` |
|     24902 | 12668 | `		if( pGen->pIn < pGen->pEnd &&` |
|     24902 | 12669 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|        35 | 12670 | `			pGen->pIn->sData.nByte == 1 &&` |
|        30 | 12671 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|        32 | 12672 | `			pGen->pIn++; /* Consume the '\|' */` |
|        32 | 12673 | `			continue;` |
|         - | 12674 | `		}` |
|     24877 | 12675 | `		break;` |
|       ! 0 | 12676 | `	}` |
|         - | 12677 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|         - | 12678 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding;` |
|         - | 12679 | `	 * jump straight to compiling the block below. */` |
|     24877 | 12680 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) /*)*/ ){` |
|         5 | 12681 | `		goto CatchBody;` |
|         - | 12682 | `	}` |
|     24868 | 12683 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     24873 | 12684 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - | 12685 | `			/* Unexpected token,break immediately */` |
|       ! 0 | 12686 | `			pToken = pGen->pIn;` |
|       ! 0 | 12687 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12688 | `				pToken--;` |
|       ! 0 | 12689 | `			}` |
|       ! 0 | 12690 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12691 | `				"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12692 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12693 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12694 | `				return SXERR_ABORT;` |
|         - | 12695 | `			}` |
|       ! 0 | 12696 | `			return SXERR_INVALID;` |
|         - | 12697 | `	}` |
|     24873 | 12698 | `	pGen->pIn++; /* Jump the dollar sign */` |
|         - | 12699 | `	/* Duplicate instance name */` |
|     24873 | 12700 | `	pName = &pGen->pIn->sData;` |
|     24873 | 12701 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     24873 | 12702 | `	if( zDup == 0 ){` |
|       ! 0 | 12703 | `		goto Mem;` |
|         - | 12704 | `	}` |
|     24873 | 12705 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     24873 | 12706 | `	pGen->pIn++;` |
|     12436 | 12707 | `CatchBody:` |
|     24877 | 12708 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|         - | 12709 | `		/* Unexpected token,break immediately */` |
|       ! 0 | 12710 | `		pToken = pGen->pIn;` |
|       ! 0 | 12711 | `		if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12712 | `			pToken--;` |
|       ! 0 | 12713 | `		}` |
|       ! 0 | 12714 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12715 | `			"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12716 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12717 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12718 | `			return SXERR_ABORT;` |
|         - | 12719 | `		}` |
|       ! 0 | 12720 | `		return SXERR_INVALID;` |
|         - | 12721 | `	}` |
|         - | 12722 | `	/* Compile the block */` |
|     24877 | 12723 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|         - | 12724 | `	/* Create the catch block */` |
|     24877 | 12725 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     24877 | 12726 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12727 | `		return SXERR_ABORT;` |
|         - | 12728 | `	}` |
|         - | 12729 | `	/* Swap bytecode container */` |
|     24877 | 12730 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     24877 | 12731 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|         - | 12732 | `	/* Compile the block */` |
|     24877 | 12733 | `	PH7_CompileBlock(&(*pGen),0);` |
|         - | 12734 | `	/* Fix forward jumps now the destination is resolved  */` |
|     24877 | 12735 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12736 | `	/* Emit the DONE instruction */` |
|     24877 | 12737 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - | 12738 | `	/* Leave the block */` |
|     24877 | 12739 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12740 | `	/* Restore the default container */` |
|     24877 | 12741 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - | 12742 | `	/* Install the catch block */` |
|     24877 | 12743 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     24877 | 12744 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12745 | `		goto Mem;` |
|         - | 12746 | `	}` |
|     24877 | 12747 | `	return SXRET_OK;` |
|       ! 0 | 12748 | `Mem:` |
|       ! 0 | 12749 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 12750 | `	return SXERR_ABORT;` |
|     12443 | 12751 | `}` |
|         - | 12752 | `/*` |
|         - | 12753 | ` * Compile a 'try' block.` |
|         - | 12754 | ` * A function using an exception should be in a "try" block.` |
|         - | 12755 | ` * If the exception does not trigger, the code will continue` |
|         - | 12756 | ` * as normal. However if the exception triggers, an exception` |
|         - | 12757 | ` * is "thrown".` |
|         - | 12758 | ` */` |
|     25034 | 12759 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|         5 | 12760 | `{` |
|         - | 12761 | `	ph7_exception *pException;` |
|     25039 | 12762 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12763 | `	GenBlock *pTry;` |
|         - | 12764 | `	sxu32 nJmpIdx;` |
|         - | 12765 | `	sxi32 rc;` |
|         - | 12766 | `	/* Create the exception container */` |
|     25039 | 12767 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     25039 | 12768 | `	if( pException == 0 ){` |
|       ! 0 | 12769 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|       ! 0 | 12770 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 12771 | `		return SXERR_ABORT;` |
|         - | 12772 | `	}` |
|         - | 12773 | `	/* Zero the structure */` |
|     25039 | 12774 | `	SyZero(pException,sizeof(ph7_exception));` |
|         - | 12775 | `	/* Initialize fields */` |
|     25039 | 12776 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     25039 | 12777 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     25039 | 12778 | `	pException->iHasFinally = 0;` |
|     25039 | 12779 | `	pException->iFinallyDone = 0;` |
|     25039 | 12780 | `	pException->pVm = pGen->pVm;` |
|         - | 12781 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|         - | 12782 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|         - | 12783 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|         - | 12784 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|         - | 12785 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|         - | 12786 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|     25039 | 12787 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|       105 | 12788 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|         - | 12789 | `	}` |
|         - | 12790 | `	/* Create the try block */` |
|     24939 | 12791 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     24939 | 12792 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12793 | `		return SXERR_ABORT;` |
|         - | 12794 | `	}` |
|         - | 12795 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     24939 | 12796 | `	pTry->pUserData = pException;` |
|         - | 12797 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     24939 | 12798 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|         - | 12799 | `	/* Fix the jump later when the destination is resolved */` |
|     24939 | 12800 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     24939 | 12801 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|         - | 12802 | `	/* Compile the block */` |
|     24939 | 12803 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     24939 | 12804 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12805 | `		return SXERR_ABORT;` |
|         - | 12806 | `	}` |
|         - | 12807 | `	/* Fix forward jumps now the destination is resolved */` |
|     24939 | 12808 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12809 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     24939 | 12810 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|         - | 12811 | `	/* Leave the block */` |
|     24939 | 12812 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12813 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     24939 | 12814 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     24932 | 12815 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|         - | 12816 | `		/* Compile one or more catch blocks */` |
|     24872 | 12817 | `		for(;;){` |
|     49744 | 12818 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     37367 | 12819 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     12439 | 12820 | `					break;` |
|         - | 12821 | `			}` |
|     24881 | 12822 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     24881 | 12823 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12824 | `				return SXERR_ABORT;` |
|         - | 12825 | `			}` |
|         5 | 12826 | `		}` |
|     12434 | 12827 | `	}` |
|         - | 12828 | `	/* Compile optional finally block */` |
|     24939 | 12829 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       748 | 12830 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|         - | 12831 | `		SySet *pInstrContainer;` |
|         - | 12832 | `		GenBlock *pFinBlock;` |
|       129 | 12833 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|         - | 12834 | `		/* Create the finally block for jump fixup bookkeeping */` |
|       129 | 12835 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|       129 | 12836 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 12837 | `			return SXERR_ABORT;` |
|         - | 12838 | `		}` |
|         - | 12839 | `		/* Swap bytecode container */` |
|       129 | 12840 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       129 | 12841 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|         - | 12842 | `		/* Compile the finally body */` |
|       129 | 12843 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       129 | 12844 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12845 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 | 12846 | `			return SXERR_ABORT;` |
|         - | 12847 | `		}` |
|         - | 12848 | `		/* Fix forward jumps now the destination is resolved */` |
|       129 | 12849 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12850 | `		/* Emit DONE to terminate the finally block */` |
|       129 | 12851 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - | 12852 | `		/* Leave the block */` |
|       129 | 12853 | `		GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12854 | `		/* Restore the default container */` |
|       129 | 12855 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       129 | 12856 | `		pException->iHasFinally = 1;` |
|        62 | 12857 | `	}` |
|         - | 12858 | `	/* Must have at least one catch or finally */` |
|     24939 | 12859 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|         8 | 12860 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12861 | `			"Cannot use try without catch or finally");` |
|         8 | 12862 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12863 | `			return SXERR_ABORT;` |
|         - | 12864 | `		}` |
|         3 | 12865 | `	}` |
|     24939 | 12866 | `	return SXRET_OK;` |
|     12522 | 12867 | `}` |
|         - | 12868 | `/*` |
|         - | 12869 | ` * Compile a switch block.` |
|         - | 12870 | ` *  (See block-comment below for more information)` |
|         - | 12871 | ` */` |
|     54544 | 12872 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|         5 | 12873 | `{` |
|     54549 | 12874 | `	sxi32 rc = SXRET_OK;` |
|     54549 | 12875 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|         - | 12876 | `		/* Unexpected token */` |
|       ! 0 | 12877 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|       ! 0 | 12878 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12879 | `			return SXERR_ABORT;` |
|         - | 12880 | `		}` |
|       ! 0 | 12881 | `		pGen->pIn++;` |
|       ! 0 | 12882 | `	}` |
|     54549 | 12883 | `	pGen->pIn++;` |
|         - | 12884 | `	/* First instruction to execute in this block. */` |
|     54549 | 12885 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|         - | 12886 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|         - | 12887 | `	 * or the '}' token */` |
|     39086 | 12888 | `	for(;;){` |
|     78177 | 12889 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 12890 | `			/* No more input to process */` |
|       ! 0 | 12891 | `			break;` |
|         - | 12892 | `		}` |
|     78177 | 12893 | `		rc = SXRET_OK;` |
|     78177 | 12894 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      3973 | 12895 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      3919 | 12896 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|         - | 12897 | `					/* Unexpected token */` |
|       ! 0 | 12898 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|       ! 0 | 12899 | `						&pGen->pIn->sData);` |
|       ! 0 | 12900 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12901 | `						return SXERR_ABORT;` |
|         - | 12902 | `					}` |
|         - | 12903 | `					/* FALL THROUGH */` |
|       ! 0 | 12904 | `				}` |
|      3919 | 12905 | `				rc = SXERR_EOF;` |
|      3919 | 12906 | `				break;` |
|         - | 12907 | `			}` |
|        32 | 12908 | `		}else{` |
|         - | 12909 | `			sxi32 nKwrd;` |
|         - | 12910 | `			/* Extract the keyword */` |
|     74209 | 12911 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     74209 | 12912 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|     25319 | 12913 | `				break;` |
|         - | 12914 | `			}` |
|     23581 | 12915 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|         3 | 12916 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|         - | 12917 | `					/* Unexpected token */` |
|       ! 0 | 12918 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|       ! 0 | 12919 | `						&pGen->pIn->sData);` |
|       ! 0 | 12920 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12921 | `						return SXERR_ABORT;` |
|         - | 12922 | `					}` |
|         - | 12923 | `					/* FALL THROUGH */` |
|       ! 0 | 12924 | `				}` |
|         - | 12925 | `				/* Block compiled */` |
|         3 | 12926 | `				break;` |
|         - | 12927 | `			}` |
|         - | 12928 | `		}` |
|         - | 12929 | `		/* Compile block */` |
|     23633 | 12930 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     23633 | 12931 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12932 | `			return SXERR_ABORT;` |
|         - | 12933 | `		}` |
|         5 | 12934 | `	}` |
|     54549 | 12935 | `	return rc;` |
|     27277 | 12936 | `}` |
|         - | 12937 | `/*` |
|         - | 12938 | ` * Compile a case eXpression.` |
|         - | 12939 | ` *  (See block-comment below for more information)` |
|         - | 12940 | ` */` |
|     54524 | 12941 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|         5 | 12942 | `{` |
|         - | 12943 | `	SySet *pInstrContainer;` |
|         - | 12944 | `	SyToken *pEnd,*pTmp;` |
|     54529 | 12945 | `	sxi32 iNest = 0;` |
|         - | 12946 | `	sxi32 rc;` |
|         - | 12947 | `	/* Delimit the expression */` |
|     54529 | 12948 | `	pEnd = pGen->pIn;` |
|    109061 | 12949 | `	while( pEnd < pGen->pEnd ){` |
|    109061 | 12950 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|         - | 12951 | `			/* Increment nesting level */` |
|         3 | 12952 | `			iNest++;` |
|    109060 | 12953 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|         - | 12954 | `			/* Decrement nesting level */` |
|         3 | 12955 | `			iNest--;` |
|    109058 | 12956 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|     54529 | 12957 | `			break;` |
|         - | 12958 | `		}` |
|     54537 | 12959 | `		pEnd++;` |
|         5 | 12960 | `	}` |
|     54529 | 12961 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 | 12962 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|       ! 0 | 12963 | `		if( rc == SXERR_ABORT ){` |
|         - | 12964 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12965 | `			return SXERR_ABORT;` |
|         - | 12966 | `		}` |
|       ! 0 | 12967 | `	}` |
|         - | 12968 | `	/* Swap token stream */` |
|     54529 | 12969 | `	pTmp = pGen->pEnd;` |
|     54529 | 12970 | `	pGen->pEnd = pEnd;` |
|     54529 | 12971 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     54529 | 12972 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|     54529 | 12973 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - | 12974 | `	/* Emit the done instruction */` |
|     54529 | 12975 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     54529 | 12976 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - | 12977 | `	/* Update token stream */` |
|     54529 | 12978 | `	pGen->pIn  = pEnd;` |
|     54529 | 12979 | `	pGen->pEnd = pTmp;` |
|     54529 | 12980 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12981 | `		return SXERR_ABORT;` |
|         - | 12982 | `	}` |
|     54529 | 12983 | `	return SXRET_OK;` |
|     27267 | 12984 | `}` |
|         - | 12985 | `/*` |
|         - | 12986 | ` * Compile the smart switch statement.` |
|         - | 12987 | ` * According to the PHP language reference manual` |
|         - | 12988 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|         - | 12989 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|         - | 12990 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|         - | 12991 | ` *  This is exactly what the switch statement is for.` |
|         - | 12992 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|         - | 12993 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|         - | 12994 | ` *  of the outer loop, use continue 2.` |
|         - | 12995 | ` *  Note that switch/case does loose comparision.` |
|         - | 12996 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|         - | 12997 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|         - | 12998 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|         - | 12999 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|         - | 13000 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|         - | 13001 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|         - | 13002 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|         - | 13003 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|         - | 13004 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|         - | 13005 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|         - | 13006 | ` *  list for the next case.` |
|         - | 13007 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|         - | 13008 | ` *  or floating-point numbers and strings.` |
|         - | 13009 | ` */` |
|      3916 | 13010 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|         5 | 13011 | `{` |
|         - | 13012 | `	GenBlock *pSwitchBlock;` |
|         - | 13013 | `	SyToken *pTmp,*pEnd;` |
|         - | 13014 | `	ph7_switch *pSwitch;` |
|         - | 13015 | `	sxu32 nToken;` |
|         - | 13016 | `	sxu32 nLine;` |
|         - | 13017 | `	sxi32 rc;` |
|      3921 | 13018 | `	nLine = pGen->pIn->nLine;` |
|         - | 13019 | `	/* Jump the 'switch' keyword */` |
|      3921 | 13020 | `	pGen->pIn++;` |
|      3921 | 13021 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 13022 | `		/* Syntax error */` |
|       ! 0 | 13023 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|       ! 0 | 13024 | `		if( rc == SXERR_ABORT ){` |
|         - | 13025 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 13026 | `			return SXERR_ABORT;` |
|         - | 13027 | `		}` |
|       ! 0 | 13028 | `		goto Synchronize;` |
|         - | 13029 | `	}` |
|         - | 13030 | `	/* Jump the left parenthesis '(' */` |
|      3921 | 13031 | `	pGen->pIn++;` |
|      3921 | 13032 | `	pEnd = 0; /* cc warning */` |
|         - | 13033 | `	/* Create the loop block */` |
|      5879 | 13034 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      1958 | 13035 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      3921 | 13036 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 13037 | `		return SXERR_ABORT;` |
|         - | 13038 | `	}` |
|         - | 13039 | `	/* Delimit the condition */` |
|      3921 | 13040 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      3921 | 13041 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - | 13042 | `		/* Empty expression */` |
|       ! 0 | 13043 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|       ! 0 | 13044 | `		if( rc == SXERR_ABORT ){` |
|         - | 13045 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 13046 | `			return SXERR_ABORT;` |
|         - | 13047 | `		}` |
|       ! 0 | 13048 | `	}` |
|         - | 13049 | `	/* Swap token streams */` |
|      3921 | 13050 | `	pTmp = pGen->pEnd;` |
|      3921 | 13051 | `	pGen->pEnd = pEnd;` |
|         - | 13052 | `	/* Compile the expression */` |
|      3921 | 13053 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      3921 | 13054 | `	if( rc == SXERR_ABORT ){` |
|         - | 13055 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 | 13056 | `		return SXERR_ABORT;` |
|         - | 13057 | `	}` |
|         - | 13058 | `	/* Update token stream */` |
|      3921 | 13059 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 | 13060 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 13061 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 | 13062 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 13063 | `			return SXERR_ABORT;` |
|         - | 13064 | `		}` |
|       ! 0 | 13065 | `		pGen->pIn++;` |
|       ! 0 | 13066 | `	}` |
|      3921 | 13067 | `	pGen->pIn  = &pEnd[1];` |
|      3921 | 13068 | `	pGen->pEnd = pTmp;` |
|      3921 | 13069 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      3916 | 13070 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|       ! 0 | 13071 | `			pTmp = pGen->pIn;` |
|       ! 0 | 13072 | `			if( pTmp >= pGen->pEnd ){` |
|       ! 0 | 13073 | `				pTmp--;` |
|       ! 0 | 13074 | `			}` |
|         - | 13075 | `			/* Unexpected token */` |
|       ! 0 | 13076 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|       ! 0 | 13077 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 13078 | `				return SXERR_ABORT;` |
|         - | 13079 | `			}` |
|       ! 0 | 13080 | `			goto Synchronize;` |
|         - | 13081 | `	}` |
|         - | 13082 | `	/* Set the delimiter token */` |
|      3921 | 13083 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|         3 | 13084 | `		nToken = PH7_TK_KEYWORD;` |
|         - | 13085 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|         2 | 13086 | `	}else{` |
|      3919 | 13087 | `		nToken = PH7_TK_CCB; /* '}' */` |
|         - | 13088 | `	}` |
|      3921 | 13089 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|         - | 13090 | `	/* Create the switch blocks container */` |
|      3921 | 13091 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      3921 | 13092 | `	if( pSwitch == 0 ){` |
|         - | 13093 | `		/* Abort compilation */` |
|       ! 0 | 13094 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 13095 | `		return SXERR_ABORT;` |
|         - | 13096 | `	}` |
|         - | 13097 | `	/* Zero the structure */` |
|      3921 | 13098 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|         - | 13099 | `	/* Initialize fields */` |
|      3921 | 13100 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|         - | 13101 | `	/* Emit the switch instruction */` |
|      3921 | 13102 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|         - | 13103 | `	/* Compile case blocks */` |
|     52588 | 13104 | `	for(;;){` |
|         - | 13105 | `		sxu32 nKwrd;` |
|     54551 | 13106 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 13107 | `			/* No more input to process */` |
|       ! 0 | 13108 | `			break;` |
|         - | 13109 | `		}` |
|     54551 | 13110 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 13111 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|         - | 13112 | `				/* Unexpected token */` |
|       ! 0 | 13113 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 13114 | `					&pGen->pIn->sData);` |
|       ! 0 | 13115 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 13116 | `					return SXERR_ABORT;` |
|         - | 13117 | `				}` |
|         - | 13118 | `				/* FALL THROUGH */` |
|       ! 0 | 13119 | `			}` |
|         - | 13120 | `			/* Block compiled */` |
|       ! 0 | 13121 | `			break;` |
|         - | 13122 | `		}` |
|         - | 13123 | `		/* Extract the keyword */` |
|     54551 | 13124 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     54551 | 13125 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|         3 | 13126 | `			if( nToken != PH7_TK_KEYWORD ){` |
|         - | 13127 | `				/* Unexpected token */` |
|       ! 0 | 13128 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 13129 | `					&pGen->pIn->sData);` |
|       ! 0 | 13130 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 13131 | `					return SXERR_ABORT;` |
|         - | 13132 | `				}` |
|         - | 13133 | `				/* FALL THROUGH */` |
|       ! 0 | 13134 | `			}` |
|         - | 13135 | `			/* Block compiled */` |
|         3 | 13136 | `			break;` |
|         - | 13137 | `		}` |
|     54549 | 13138 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|         - | 13139 | `			/*` |
|         - | 13140 | `			 * Accroding to the PHP language reference manual` |
|         - | 13141 | `			 *  A special case is the default case. This case matches anything` |
|         - | 13142 | `			 *  that wasn't matched by the other cases.` |
|         - | 13143 | `			 */` |
|        25 | 13144 | `			if( pSwitch->nDefault > 0 ){` |
|         - | 13145 | `				/* Default case already compiled */` |
|       ! 0 | 13146 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|       ! 0 | 13147 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 13148 | `					return SXERR_ABORT;` |
|         - | 13149 | `				}` |
|       ! 0 | 13150 | `			}` |
|        25 | 13151 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|         - | 13152 | `			/* Compile the default block */` |
|        25 | 13153 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|        25 | 13154 | `			if( rc == SXERR_ABORT){` |
|       ! 0 | 13155 | `				return SXERR_ABORT;` |
|        25 | 13156 | `			}else if( rc == SXERR_EOF ){` |
|        23 | 13157 | `				break;` |
|         1 | 13158 | `			}` |
|     54530 | 13159 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|         - | 13160 | `			ph7_case_expr sCase;` |
|         - | 13161 | `			/* Standard case block */` |
|     54529 | 13162 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|         - | 13163 | `			/* initialize the structure */` |
|     54529 | 13164 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - | 13165 | `			/* Compile the case expression */` |
|     54529 | 13166 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|     54529 | 13167 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 13168 | `				return SXERR_ABORT;` |
|         - | 13169 | `			}` |
|         - | 13170 | `			/* Compile the case block */` |
|     54529 | 13171 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|         - | 13172 | `			/* Insert in the switch container */` |
|     54529 | 13173 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|     54529 | 13174 | `			if( rc == SXERR_ABORT){` |
|       ! 0 | 13175 | `				return SXERR_ABORT;` |
|     54529 | 13176 | `			}else if( rc == SXERR_EOF ){` |
|      3901 | 13177 | `				break;` |
|         - | 13178 | `			}` |
|     25319 | 13179 | `		}else{` |
|         - | 13180 | `			/* Unexpected token */` |
|       ! 0 | 13181 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 13182 | `				&pGen->pIn->sData);` |
|       ! 0 | 13183 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 13184 | `				return SXERR_ABORT;` |
|         - | 13185 | `			}` |
|       ! 0 | 13186 | `			break;` |
|         - | 13187 | `		}` |
|         5 | 13188 | `	}` |
|         - | 13189 | `	/* Fix all jumps now the destination is resolved */` |
|      3921 | 13190 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      3921 | 13191 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 13192 | `	/* Release the loop block */` |
|      3921 | 13193 | `	GenStateLeaveBlock(pGen,0);` |
|      3921 | 13194 | `	if( pGen->pIn < pGen->pEnd ){` |
|         - | 13195 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      3921 | 13196 | `		pGen->pIn++;` |
|      1958 | 13197 | `	}` |
|         - | 13198 | `	/* Statement successfully compiled */` |
|      3921 | 13199 | `	return SXRET_OK;` |
|       ! 0 | 13200 | `Synchronize:` |
|         - | 13201 | `	/* Synchronize with the first semi-colon */` |
|       ! 0 | 13202 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       ! 0 | 13203 | `		pGen->pIn++;` |
|       ! 0 | 13204 | `	}` |
|       ! 0 | 13205 | `	return SXRET_OK;` |
|      1963 | 13206 | `}` |
|         - | 13207 | `/*` |
|         - | 13208 | ` * Chain operators participate in a postfix member-access chain.` |
|         - | 13209 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|         - | 13210 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|         - | 13211 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|         - | 13212 | ` */` |
|         - | 13213 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|         - | 13214 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|         - | 13215 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|         - | 13216 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|         - | 13217 |  |
|         - | 13218 | `/*` |
|         - | 13219 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|         - | 13220 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|         - | 13221 | ` * patched entries from the pending set.` |
|         - | 13222 | ` */` |
|  51194816 | 13223 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|         5 | 13224 | `{` |
|  51194821 | 13225 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|         - | 13226 | `	sxu32 nTarget;` |
|         - | 13227 | `	sxu32 *aIdx;` |
|         - | 13228 | `	sxu32 i;` |
|  51194821 | 13229 | `	if( nCur <= nBaseline ){` |
|  51194725 | 13230 | `		return;` |
|         - | 13231 | `	}` |
|       100 | 13232 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|       100 | 13233 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|       204 | 13234 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|       108 | 13235 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|       108 | 13236 | `		if( pInstr ){` |
|       108 | 13237 | `			pInstr->iP2 = (sxi32)nTarget;` |
|        52 | 13238 | `		}` |
|        56 | 13239 | `	}` |
|       100 | 13240 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
|  25597413 | 13241 | `}` |
|         - | 13242 |  |
|         - | 13243 | `/*` |
|         - | 13244 | ` * By-reference out-parameters of builtin functions.` |
|         - | 13245 | ` *` |
|         - | 13246 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|         - | 13247 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|         - | 13248 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|         - | 13249 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|         - | 13250 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|         - | 13251 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|         - | 13252 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|         - | 13253 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|         - | 13254 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|         - | 13255 | ` * creates it" behaviour).` |
|         - | 13256 | ` *` |
|         - | 13257 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|         - | 13258 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|         - | 13259 | ` */` |
|   6578520 | 13260 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|         5 | 13261 | `{` |
|         - | 13262 | `	static const struct {` |
|         - | 13263 | `		const char *zName;` |
|         - | 13264 | `		sxu32 nByte;` |
|         - | 13265 | `		sxu32 mask;` |
|         - | 13266 | `	} aByRef[] = {` |
|         - | 13267 | `		{ "parse_str",              9, 1u<<1 },  /* &$result (apArg[1]) */` |
|         - | 13268 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - | 13269 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - | 13270 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - | 13271 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - | 13272 | `		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */` |
|         - | 13273 | `		{ "fsockopen",              9, (1u<<2)\|(1u<<3) },  /* &$error_code, &$error_message */` |
|         - | 13274 | `		{ "pfsockopen",            10, (1u<<2)\|(1u<<3) },  /* same */` |
|         - | 13275 | `		{ "stream_socket_client",  20, (1u<<1)\|(1u<<2) },  /* &$error_code, &$error_message */` |
|         - | 13276 | `		{ "proc_open",              9, 1u<<2 },  /* &$pipes (apArg[2]) */` |
|         - | 13277 | `	};` |
|         - | 13278 | `	sxu32 i;` |
|   6578525 | 13279 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|   1754683 | 13280 | `		return 0;` |
|         - | 13281 | `	}` |
|  52579031 | 13282 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
|  47813658 | 13283 | `		if( pName->nByte == aByRef[i].nByte` |
|  25323961 | 13284 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|     58479 | 13285 | `			return aByRef[i].mask;` |
|         - | 13286 | `		}` |
|  23877597 | 13287 | `	}` |
|   4765373 | 13288 | `	return 0;` |
|   3289265 | 13289 | `}` |
|         - | 13290 | `/*` |
|         - | 13291 | ` * Recover the bare global-builtin name from a call's callee node.` |
|         - | 13292 | ` *` |
|         - | 13293 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|         - | 13294 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|         - | 13295 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|         - | 13296 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|         - | 13297 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|         - | 13298 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|         - | 13299 | ` */` |
|   6578520 | 13300 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|         5 | 13301 | `{` |
|         - | 13302 | `	SyToken *p, *pEnd;` |
|   6578525 | 13303 | `	pOut->zString = 0;` |
|   6578525 | 13304 | `	pOut->nByte = 0;` |
|   6578525 | 13305 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|       ! 0 | 13306 | `		return;` |
|         - | 13307 | `	}` |
|   6578525 | 13308 | `	p = pLeft->pStart;` |
|   6578525 | 13309 | `	pEnd = pLeft->pEnd;` |
|         - | 13310 | `	/* Optional single leading namespace separator (absolute path). */` |
|   6578525 | 13311 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      3931 | 13312 | `		p++;` |
|      1963 | 13313 | `	}` |
|   6578525 | 13314 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|   1754637 | 13315 | `		return;` |
|         - | 13316 | `	}` |
|         - | 13317 | `	/* Must be a single component: nothing follows the name token. */` |
|   4823893 | 13318 | `	if( p + 1 != pEnd ){` |
|        51 | 13319 | `		return;` |
|         - | 13320 | `	}` |
|   4823847 | 13321 | `	*pOut = p->sData;` |
|   3289265 | 13322 | `}` |
|         - | 13323 | `/*` |
|         - | 13324 | ` * Generate bytecode for a given expression tree.` |
|         - | 13325 | ` * If something goes wrong while generating bytecode` |
|         - | 13326 | ` * for the expression tree (A very unlikely scenario)` |
|         - | 13327 | ` * this function takes care of generating the appropriate` |
|         - | 13328 | ` * error message.` |
|         - | 13329 | ` */` |
|  71676076 | 13330 | `static sxi32 GenStateEmitExprCode(` |
|         - | 13331 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 13332 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|         - | 13333 | `	sxi32 iFlags /* Control flags */` |
|         - | 13334 | `	)` |
|         5 | 13335 | `{` |
|         - | 13336 | `	VmInstr *pInstr;` |
|         - | 13337 | `	sxu32 nJmpIdx;` |
|  71676081 | 13338 | `	sxi32 iP1 = 0;` |
|  71676081 | 13339 | `	sxu32 iP2 = 0;` |
|  71676081 | 13340 | `	void *p3  = 0;` |
|         - | 13341 | `	sxi32 iVmOp;` |
|         - | 13342 | `	sxi32 rc;` |
|  71676081 | 13343 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
|  71676081 | 13344 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
|  71676081 | 13345 | `	sxu32 nRhsNsBase = 0;` |
|  71676081 | 13346 | `	if( pNode->xCode ){` |
|         - | 13347 | `		SyToken *pTmpIn,*pTmpEnd;` |
|         - | 13348 | `		/* Compile node */` |
|  43176615 | 13349 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|  43176615 | 13350 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|  43176615 | 13351 | `		RE_SWAP_DELIMITER(pGen);` |
|  43176615 | 13352 | `		return rc;` |
|         - | 13353 | `	}` |
|  28499471 | 13354 | `	if( pNode->pOp == 0 ){` |
|       ! 0 | 13355 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13356 | `			"Invalid expression node,PH7 is aborting compilation");` |
|       ! 0 | 13357 | `		return SXERR_ABORT;` |
|         - | 13358 | `	}` |
|  28499471 | 13359 | `	iVmOp = pNode->pOp->iVmOp;` |
|  28499471 | 13360 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|         - | 13361 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|         - | 13362 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|         - | 13363 | `		 * and later errors are still reported. */` |
|         3 | 13364 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13365 | `			"The (unset) cast is no longer supported");` |
|         3 | 13366 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 13367 | `			return SXERR_ABORT;` |
|         - | 13368 | `		}` |
|         1 | 13369 | `	}` |
|  28499471 | 13370 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        93 | 13371 | `		sxu32 nJmp = 0;` |
|         - | 13372 | `		sxu32 nNcNsBase;` |
|         - | 13373 | `		VmInstr *pInstrFix;` |
|         - | 13374 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|         - | 13375 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|         - | 13376 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|         - | 13377 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|         - | 13378 | `		 * stack slot carries a writable nIdx. */` |
|        93 | 13379 | `		if( pNode->pRight ){` |
|        93 | 13380 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        93 | 13381 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|        93 | 13382 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13383 | `				return rc;` |
|         - | 13384 | `			}` |
|        93 | 13385 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|         - | 13386 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|         - | 13387 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|         - | 13388 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|         - | 13389 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|         - | 13390 | `			 * the store, so the parent array does not need to be copied at` |
|         - | 13391 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|         - | 13392 | `			 * cascade for the actual write path stays correct. */` |
|        93 | 13393 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|        93 | 13394 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|        33 | 13395 | `				pInstrFix->iP2 = 3;` |
|        15 | 13396 | `			}` |
|        45 | 13397 | `		}` |
|         - | 13398 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|        93 | 13399 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|         - | 13400 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|        93 | 13401 | `		if( pNode->pLeft ){` |
|        93 | 13402 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        93 | 13403 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|        93 | 13404 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13405 | `				return rc;` |
|         - | 13406 | `			}` |
|        93 | 13407 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        45 | 13408 | `		}` |
|         - | 13409 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|        93 | 13410 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|         - | 13411 | `		/* Patch the short-circuit jump to land after the store. */` |
|        93 | 13412 | `		if( nJmp > 0 ){` |
|        93 | 13413 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|        93 | 13414 | `			if( pInstrFix ){` |
|        93 | 13415 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|        45 | 13416 | `			}` |
|        45 | 13417 | `		}` |
|        93 | 13418 | `		return SXRET_OK;` |
|         - | 13419 | `	}` |
|  28499381 | 13420 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|         - | 13421 | `		sxu32 nJz,nJmp;` |
|         - | 13422 | `		sxu32 nTernaryNsBase;` |
|         - | 13423 | `		/* Ternary operator require special handling */` |
|         - | 13424 | `		/* Phase#1: Compile the condition */` |
|    477739 | 13425 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    477739 | 13426 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    477739 | 13427 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13428 | `			return rc;` |
|         - | 13429 | `		}` |
|         - | 13430 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|         - | 13431 | `		 * compiling the condition must short-circuit to the end of the` |
|         - | 13432 | `		 * condition expression, not leak past the ternary. */` |
|    477739 | 13433 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    477739 | 13434 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    477739 | 13435 | `		if( pNode->pLeft ){` |
|         - | 13436 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|         - | 13437 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    473781 | 13438 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 13439 | `			/* Phase#3: Compile the 'then' expression  */` |
|    473781 | 13440 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    473781 | 13441 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    473781 | 13442 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13443 | `				return rc;` |
|         - | 13444 | `			}` |
|    473781 | 13445 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    236893 | 13446 | `		}else{` |
|         - | 13447 | `			/* Elvis operator: (expr) ?: (else)` |
|         - | 13448 | `			 * Duplicate condition so original value is the 'then' result.` |
|         - | 13449 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      3963 | 13450 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      3963 | 13451 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 13452 | `		}` |
|         - | 13453 | `		/* Phase#4: Emit the unconditional jump */` |
|    477739 | 13454 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|         - | 13455 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    477739 | 13456 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    477739 | 13457 | `		if( pInstr ){` |
|    477739 | 13458 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    238867 | 13459 | `		}` |
|    477739 | 13460 | `		if( !pNode->pLeft ){` |
|         - | 13461 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      3963 | 13462 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      1979 | 13463 | `		}` |
|         - | 13464 | `		/* Phase#6: Compile the 'else' expression */` |
|    477739 | 13465 | `		if( pNode->pRight ){` |
|    477739 | 13466 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    477739 | 13467 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    477739 | 13468 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13469 | `				return rc;` |
|         - | 13470 | `			}` |
|    477739 | 13471 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    238867 | 13472 | `		}` |
|    477739 | 13473 | `		if( nJmp > 0 ){` |
|         - | 13474 | `			/* Phase#7: Fix the unconditional jump */` |
|    477739 | 13475 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    477739 | 13476 | `			if( pInstr ){` |
|    477739 | 13477 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    238867 | 13478 | `			}` |
|    238867 | 13479 | `		}` |
|         - | 13480 | `		/* All done */` |
|    477739 | 13481 | `		return SXRET_OK;` |
|         - | 13482 | `	}` |
|  28021647 | 13483 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|         - | 13484 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|         - | 13485 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|         - | 13486 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|         - | 13487 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|         - | 13488 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|         - | 13489 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|         - | 13490 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|         - | 13491 | `		sxu32 nPipeNsBase;` |
|        27 | 13492 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|        27 | 13493 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|       ! 0 | 13494 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13495 | `				"'\|>': Missing operand");` |
|       ! 0 | 13496 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 13497 | `		}` |
|         - | 13498 | `		/* Argument: the LHS value. */` |
|        27 | 13499 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 13500 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|        27 | 13501 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13502 | `			return rc;` |
|         - | 13503 | `		}` |
|        27 | 13504 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 13505 | `		/* Callable: the RHS. */` |
|        27 | 13506 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 13507 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|        27 | 13508 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13509 | `			return rc;` |
|         - | 13510 | `		}` |
|        27 | 13511 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 13512 | `		/* Invoke the callable with the single piped argument. */` |
|        27 | 13513 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        27 | 13514 | `		return SXRET_OK;` |
|         - | 13515 | `	}` |
|  28021621 | 13516 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|         - | 13517 | `	/* Generate code for the left tree */` |
|  28021621 | 13518 | `	if( pNode->pLeft ){` |
|  27979049 | 13519 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  27979049 | 13520 | `		if( iVmOp == PH7_OP_CALL ){` |
|         - | 13521 | `			ph7_expr_node **apNode;` |
|   6582739 | 13522 | `			int hasSpread = 0;` |
|   6582739 | 13523 | `			int hasNamed = 0;` |
|   6582739 | 13524 | `			int bAnySpread = 0;` |
|   6582739 | 13525 | `			sxu32 byRefMask = 0;` |
|         - | 13526 | `			sxi32 nArgs;` |
|         - | 13527 | `			sxi32 n;` |
|         - | 13528 | `			/* Recurse and generate bytecodes for function arguments */` |
|   6582739 | 13529 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   6582739 | 13530 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|         - | 13531 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|         - | 13532 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|         - | 13533 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|   6582739 | 13534 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|        81 | 13535 | `				bFcc = 1;` |
|        81 | 13536 | `				nArgs = 0;` |
|        40 | 13537 | `			}` |
|         - | 13538 | `			/* Validate argument order like php: no positional argument after a` |
|         - | 13539 | ``			 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|         - | 13540 | `			{` |
|   6582739 | 13541 | `				int seenNamed = 0;` |
|   6582739 | 13542 | `				int seenSpread = 0;` |
|  13786311 | 13543 | `				for( n = 0; n < nArgs; ++n ){` |
|   7203579 | 13544 | `					if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      4087 | 13545 | `						bAnySpread = 1;` |
|      4087 | 13546 | `						seenSpread = 1;` |
|      4087 | 13547 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       ! 0 | 13548 | `							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13549 | `								"syntax error, unexpected token \"...\"");` |
|       ! 0 | 13550 | `							return SXERR_SYNTAX;` |
|         5 | 13551 | `						}` |
|   7201538 | 13552 | `					}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       289 | 13553 | `						seenNamed = 1;` |
|       289 | 13554 | `						hasNamed = 1;` |
|   7199355 | 13555 | `					}else if( seenNamed ){` |
|         3 | 13556 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13557 | `							"Cannot use positional argument after named argument");` |
|         3 | 13558 | `						return SXERR_SYNTAX;` |
|   7199211 | 13559 | `					}else if( seenSpread ){` |
|       ! 0 | 13560 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13561 | `							"Cannot use positional argument after argument unpacking");` |
|       ! 0 | 13562 | `						return SXERR_SYNTAX;` |
|         - | 13563 | `					}` |
|   3601791 | 13564 | `				}` |
|         - | 13565 | `			}` |
|         - | 13566 | `			/* Read-only load */` |
|   6582737 | 13567 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|         - | 13568 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|         - | 13569 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|         - | 13570 | `			 * objects dispatch to the right method (offsetExists for both;` |
|         - | 13571 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|   6582737 | 13572 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|   6582737 | 13573 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|   6582732 | 13574 | `				if( pCallName->nByte == 5` |
|   3683567 | 13575 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|    326985 | 13576 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|   6419247 | 13577 | `				}else if( pCallName->nByte == 5` |
|   3356587 | 13578 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|       117 | 13579 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|        56 | 13580 | `				}` |
|         - | 13581 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|         - | 13582 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|         - | 13583 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|         - | 13584 | `				 * write back through. Skipped when spread/named args are present:` |
|         - | 13585 | `				 * the compile-time positional index no longer maps to the` |
|         - | 13586 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|   6582737 | 13587 | `				if( !bAnySpread && !hasNamed ){` |
|         - | 13588 | `					SyString sBuiltin;` |
|   6578525 | 13589 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|   6578525 | 13590 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|   3289260 | 13591 | `				}` |
|   3291366 | 13592 | `			}` |
|  13786307 | 13593 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|   7203575 | 13594 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   7203575 | 13595 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 13596 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|         - | 13597 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|         - | 13598 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|         - | 13599 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|         - | 13600 | `				 * builtin to write back through. A plain $var target is unaffected` |
|         - | 13601 | `				 * (iP1=0 either way). */` |
|   7203575 | 13602 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|     38971 | 13603 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|     38971 | 13604 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|     19483 | 13605 | `				}` |
|   7203575 | 13606 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|   7203575 | 13607 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 13608 | `					return rc;` |
|         - | 13609 | `				}` |
|         - | 13610 | `				/* Each argument is an independent nullsafe scope. */` |
|   7203575 | 13611 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|   7203575 | 13612 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|         - | 13613 | `					/* Emit spread opcode to unpack this array argument */` |
|      4087 | 13614 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      4087 | 13615 | `					hasSpread = 1;` |
|      2041 | 13616 | `				}` |
|   3601790 | 13617 | `			}` |
|         - | 13618 | `			/* Total number of given arguments */` |
|   6582737 | 13619 | `			iP1 = nArgs;` |
|   6582737 | 13620 | `			iP2 = hasSpread;` |
|         - | 13621 | `			/* Build VmCallArgMap if named arguments are present.` |
|         - | 13622 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|   6582737 | 13623 | `			if( hasNamed ){` |
|       178 | 13624 | `				sxu32 nStrBytes = 0;` |
|         - | 13625 | `				char *zBuf;` |
|       534 | 13626 | `				for( n = 0; n < nArgs; ++n ){` |
|       360 | 13627 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       286 | 13628 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|       141 | 13629 | `					}` |
|       182 | 13630 | `				}` |
|         - | 13631 | `				{` |
|       178 | 13632 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|       178 | 13633 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       174 | 13634 | `					&pGen->pVm->sAllocator, mapSize);` |
|       178 | 13635 | `				if( pMap ){` |
|       178 | 13636 | `					SyZero(pMap, mapSize);` |
|       178 | 13637 | `					pMap->bHasNamed = 1;` |
|       178 | 13638 | `					pMap->nTotal = (sxu32)nArgs;` |
|       178 | 13639 | `					pMap->aNames = (SyString *)&pMap[1];` |
|       178 | 13640 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|       534 | 13641 | `					for( n = 0; n < nArgs; ++n ){` |
|       360 | 13642 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       286 | 13643 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|       286 | 13644 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|       286 | 13645 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|       286 | 13646 | `							zBuf += nb;` |
|       141 | 13647 | `						}` |
|         - | 13648 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|       182 | 13649 | `					}` |
|       178 | 13650 | `					p3 = (void *)pMap;` |
|        87 | 13651 | `				}` |
|         - | 13652 | `				}` |
|        87 | 13653 | `			}` |
|         - | 13654 | `			/* Remove stale flags now */` |
|   6582737 | 13655 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|   3291366 | 13656 | `		}` |
|         - | 13657 | `		{` |
|         - | 13658 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|         - | 13659 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|         - | 13660 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|         - | 13661 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|         - | 13662 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|         - | 13663 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|         - | 13664 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|         - | 13665 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
|  27979047 | 13666 | `			sxi32 iLeftFlags = iFlags;` |
|  27979042 | 13667 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  22941779 | 13668 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|   8952284 | 13669 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   7717529 | 13670 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   2664507 | 13671 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   1332251 | 13672 | `			}` |
|         - | 13673 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|         - | 13674 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|         - | 13675 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|         - | 13676 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|         - | 13677 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|         - | 13678 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|         - | 13679 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
|  27979042 | 13680 | `			if( pNode->pOp` |
|  39451707 | 13681 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
|  25462233 | 13682 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  22945372 | 13683 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|   5439109 | 13684 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   2719552 | 13685 | `			}` |
|         - | 13686 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|         - | 13687 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|         - | 13688 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|         - | 13689 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|         - | 13690 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|         - | 13691 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
|  27979042 | 13692 | `			if( pNode->pOp` |
|  27979047 | 13693 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|    210457 | 13694 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|    105226 | 13695 | `			}` |
|         - | 13696 | ``			/* `??` reads its LEFT operand in isset-context: an undefined or`` |
|         - | 13697 | `			 * UNINITIALIZED typed PROPERTY must yield the default rather than a` |
|         - | 13698 | `			 * warning/Error (php). Tag a member-access LHS so its OP_MEMBER takes` |
|         - | 13699 | `			 * the silent-lookup path (iP2 = ISSET), which still loads a present` |
|         - | 13700 | `			 * value. A SUBSCRIPT LHS is left untagged: LOAD_IDX's ISSET mode means` |
|         - | 13701 | ``			 * offsetExists (a bool), but `$o[$k] ?? d` needs the offsetGet value —`` |
|         - | 13702 | `			 * that path is already handled correctly by OP_NULLC. */` |
|  27979042 | 13703 | `			if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC` |
|  14018764 | 13704 | `				&& pNode->pLeft && pNode->pLeft->pOp` |
|     87666 | 13705 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|     58429 | 13706 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|     58411 | 13707 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|        39 | 13708 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|        19 | 13709 | `			}` |
|  27979047 | 13710 | `			if( iVmOp == PH7_OP_ERR_CTRL ){` |
|         - | 13711 | `				/* '@' must suppress the diagnostics raised WHILE its operand runs, so` |
|         - | 13712 | `				 * open the window here; the trailing emit below closes it (iP1 = 0). */` |
|     15817 | 13713 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);` |
|      7906 | 13714 | `			}` |
|  27979047 | 13715 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|         - | 13716 | `		}` |
|  27979047 | 13717 | `		if( rc != SXRET_OK ){` |
|        34 | 13718 | `			return rc;` |
|         - | 13719 | `		}` |
|  27979017 | 13720 | `		if( !bIsChainOp ){` |
|         - | 13721 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|         - | 13722 | `			 * target the end of that LHS chain, which is right here. */` |
|  12979389 | 13723 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|   6489692 | 13724 | `		}` |
|  27979017 | 13725 | `		if( iVmOp == PH7_OP_CALL ){` |
|   6582737 | 13726 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   6582737 | 13727 | `			if( pInstr ){` |
|   6582737 | 13728 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|   4824141 | 13729 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|         - | 13730 | `					sxu32 nQual;` |
|   4824141 | 13731 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 13732 | `					/* Prevent constant expansion but preserve the absolute flag` |
|         - | 13733 | `					 * so the later NEW handler (if any) can see it. */` |
|   4824141 | 13734 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|         - | 13735 | `					/* Namespace-qualify the function name for CALL, unless the` |
|         - | 13736 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|         - | 13737 | `					 * imports — class imports must NOT affect function` |
|         - | 13738 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|         - | 13739 | `					 * before NEW; we store the original literal index in the` |
|         - | 13740 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|         - | 13741 | `					 * the unqualified name and re-qualify with class imports. */` |
|   4824141 | 13742 | `					if( bAbsolute ){` |
|      3931 | 13743 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      1968 | 13744 | `					}else{` |
|   4820215 | 13745 | `						int fromImport = 0;` |
|   4820215 | 13746 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|   4820215 | 13747 | `						pInstr->iP2 = (sxi32)nQual;` |
|   4820215 | 13748 | `						if( nQual != nOrig ){` |
|         - | 13749 | `							/* Record the original literal index in the arg map` |
|         - | 13750 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|         - | 13751 | `							 * flag) so the NEW handler can recover the` |
|         - | 13752 | `							 * unqualified name and re-qualify with CLASS` |
|         - | 13753 | `							 * imports. */` |
|       103 | 13754 | `							if( p3 == 0 ){` |
|       103 | 13755 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|        98 | 13756 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|       103 | 13757 | `								if( pMap ){` |
|       103 | 13758 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|       103 | 13759 | `									p3 = (void *)pMap;` |
|        49 | 13760 | `								}` |
|        49 | 13761 | `							}` |
|       103 | 13762 | `							if( p3 ){` |
|       103 | 13763 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|       103 | 13764 | `								if( !fromImport ){` |
|         - | 13765 | `									/* Mark as namespace-qualified */` |
|        93 | 13766 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|        44 | 13767 | `								}` |
|        49 | 13768 | `							}` |
|        49 | 13769 | `						}` |
|         - | 13770 | `					}` |
|   4170669 | 13771 | `				}else if( (pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */` |
|   1748526 | 13772 | `						&& !(pNode->pLeft && (pNode->pLeft->iFlags & EXPR_NODE_PARENS)))` |
|    889375 | 13773 | `					\|\| pInstr->iOp == PH7_OP_NEW ){` |
|         - | 13774 | `					/* Method call,flag that. But NOT when the callee was an explicitly` |
|         - | 13775 | ``					 * PARENTHESISED member access: `($o->p)(...)` / `($o::$p)(...)` invokes`` |
|         - | 13776 | `					 * the VALUE of the property (php's variable-invocation), so the OP_MEMBER` |
|         - | 13777 | `					 * must stay a plain property READ (leaving the callable on the stack for` |
|         - | 13778 | `					 * OP_CALL to invoke) rather than being rewritten into a method-name` |
|         - | 13779 | ``					 * resolution — the parens are exactly what distinguishes `($o->p)()` from`` |
|         - | 13780 | ``					 * the method call `$o->p()`. */`` |
|   1738463 | 13781 | `					pInstr->iP2 = 1;` |
|         - | 13782 | ``					/* A static call with a DYNAMIC method name (`C::$m(...)`): the`` |
|         - | 13783 | ``					 * static-`::` codegen folded the variable NAME into OP_MEMBER->p3`` |
|         - | 13784 | ``					 * as if it were a static-PROPERTY read (`C::$m`), but in a CALL the`` |
|         - | 13785 | `					 * method name is the variable's VALUE. Rebuild the sequence` |
|         - | 13786 | `					 * [class, OP_LOAD $m -> value, OP_MEMBER(static, method)] so the` |
|         - | 13787 | `					 * dynamic name is read off the stack, matching the instance` |
|         - | 13788 | ``					 * (`$o->$m()`) path. iP1==1 marks a static member. */`` |
|   1738463 | 13789 | `					if( pInstr->iOp == PH7_OP_MEMBER && pInstr->iP1 == 1 && pInstr->p3 ){` |
|        11 | 13790 | `						void *pDynName = pInstr->p3;` |
|        11 | 13791 | `						(void)PH7_VmPopInstr(pGen->pVm);` |
|        11 | 13792 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,pDynName,0);` |
|        11 | 13793 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_MEMBER,1,PH7_MEMBER_METHOD,0,0);` |
|         5 | 13794 | `					}` |
|    869229 | 13795 | `				}` |
|   3291371 | 13796 | `			}` |
|  24687651 | 13797 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|         - | 13798 | `			ph7_expr_node **apNode;` |
|         - | 13799 | `			sxi32 n;` |
|   2977797 | 13800 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|         - | 13801 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|         - | 13802 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 13803 | `			/* Recurse and generate bytecodes for array index */` |
|   2977797 | 13804 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   5721889 | 13805 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   2744097 | 13806 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   2744097 | 13807 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   2744097 | 13808 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 13809 | `					return rc;` |
|         - | 13810 | `				}` |
|         - | 13811 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   2744097 | 13812 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   1372051 | 13813 | `			}` |
|   2977797 | 13814 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   2744097 | 13815 | `				iP1 = 1; /* Node have an index associated with it */` |
|   1372046 | 13816 | `			}` |
|   2977797 | 13817 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|         - | 13818 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|    365755 | 13819 | `				iP2 = 4;` |
|   2794922 | 13820 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|         - | 13821 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|         - | 13822 | `				 * so the trailing unset() builtin can drop the slot. */` |
|     23409 | 13823 | `				iP2 = 5;` |
|   2600345 | 13824 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|         - | 13825 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|         - | 13826 | `				 * short-circuit on missing keys without invoking offsetGet` |
|         - | 13827 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|        31 | 13828 | `				iP2 = 6;` |
|   2588630 | 13829 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|         - | 13830 | `				/* Create an empty entry when the desired index is not found */` |
|    549181 | 13831 | `				iP2 = 1;` |
|    274593 | 13832 | `			}` |
|  19907389 | 13833 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         - | 13834 | `			/* POP the left node */` |
|         5 | 13835 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 | 13836 | `		}` |
|  13989506 | 13837 | `	}` |
|  28021589 | 13838 | `	rc = SXRET_OK;` |
|  28021589 | 13839 | `	nJmpIdx = 0;` |
|         - | 13840 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|         - | 13841 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|         - | 13842 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  28021589 | 13843 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|    440381 | 13844 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    440381 | 13845 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    440381 | 13846 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    440381 | 13847 | `			int isSpecial = 0;` |
|    440381 | 13848 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|    370357 | 13849 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|    370357 | 13850 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|    370352 | 13851 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    331384 | 13852 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    179298 | 13853 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|    136273 | 13854 | `					isSpecial = 1;` |
|     68134 | 13855 | `				}` |
|    202682 | 13856 | `			}` |
|    475393 | 13857 | `			pInstr->iP1 = 0;` |
|         - | 13858 | ``			/* A leading `\` (NSSEP) makes the class name ABSOLUTE: it names the`` |
|         - | 13859 | `			 * global class, so it must NOT be re-qualified with the current namespace.` |
|         - | 13860 | ``			 * `\Closure::bind(...)` inside `namespace X` is Closure, not X\Closure.`` |
|         - | 13861 | ``			 * (Multi-component `\A\B::m` already resolves absolutely because its`` |
|         - | 13862 | ``			 * literal keeps a backslash; the single-component `\Closure` lost it.) */`` |
|         - | 13863 | `			{` |
|    678075 | 13864 | `				int bAbsolute = (pNode->pLeft && pNode->pLeft->pStart` |
|    608046 | 13865 | `					&& (pNode->pLeft->pStart->nType & PH7_TK_NSSEP));` |
|    405369 | 13866 | `				if( !isSpecial && !bAbsolute ){` |
|    269083 | 13867 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    134539 | 13868 | `				}` |
|         - | 13869 | `			}` |
|         - | 13870 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|         - | 13871 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|    405369 | 13872 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    269101 | 13873 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    269101 | 13874 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|        78 | 13875 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|        78 | 13876 | `					return SXRET_OK;` |
|         - | 13877 | `				}` |
|    134511 | 13878 | `			}` |
|    202645 | 13879 | `		}` |
|    272642 | 13880 | `	}` |
|         - | 13881 | `	/* Generate code for the right tree */` |
|  27986521 | 13882 | `	if( pNode->pRight ){` |
|  16161629 | 13883 | `		if( iVmOp == PH7_OP_LAND ){` |
|         - | 13884 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    443761 | 13885 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  15939751 | 13886 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|         - | 13887 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    318983 | 13888 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  15558384 | 13889 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|         - | 13890 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     58491 | 13891 | `			iVmOp = 0; /* No binary operator to emit */` |
|     58491 | 13892 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  15369704 | 13893 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         - | 13894 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|         - | 13895 | `			 * the entire containing postfix chain to null. The jump target is` |
|         - | 13896 | `			 * patched later by the innermost non-chain ancestor (or by` |
|         - | 13897 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|         - | 13898 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|         - | 13899 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|       108 | 13900 | `			sxu32 nNsJmp = 0;` |
|       108 | 13901 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|       108 | 13902 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  15340357 | 13903 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|         - | 13904 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|         - | 13905 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|         - | 13906 | `			 * auto-created — PHP auto-vivifies on write. */` |
|   5078521 | 13907 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|   2539258 | 13908 | `		}` |
|  16161629 | 13909 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  16161629 | 13910 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  16161629 | 13911 | `		if( !bIsChainOp ){` |
|         - | 13912 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|         - | 13913 | `			 * operator instruction is emitted. */` |
|  10722599 | 13914 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|   5361297 | 13915 | `		}` |
|  16161629 | 13916 | `		if( iVmOp == PH7_OP_STORE ){` |
|   4627169 | 13917 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|   4627132 | 13918 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|         - | 13919 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|         - | 13920 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|         - | 13921 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|         - | 13922 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|         - | 13923 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|         - | 13924 | `				 */` |
|        91 | 13925 | `				iVmOp = 0;` |
|   4627126 | 13926 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|   4627083 | 13927 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 13928 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|    856101 | 13929 | `					iP2 = 1;` |
|    428053 | 13930 | `				}else{` |
|   3770987 | 13931 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 13932 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|    529643 | 13933 | `						iVmOp = PH7_OP_STORE_IDX;` |
|    529643 | 13934 | `						iP1 = pInstr->iP1;` |
|    264824 | 13935 | `					}else{` |
|   3241349 | 13936 | `						p3 = pInstr->p3;` |
|         - | 13937 | `					}` |
|         - | 13938 | `					/* POP the last dynamic load instruction */` |
|   3770987 | 13939 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - | 13940 | `				}` |
|   2313544 | 13941 | `			}` |
|  13848047 | 13942 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|        64 | 13943 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|        64 | 13944 | `			if( pInstr ){` |
|        64 | 13945 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 13946 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|         - | 13947 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|         - | 13948 | `					 */` |
|        19 | 13949 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|        19 | 13950 | `					iP1 = pInstr->iP1;` |
|        19 | 13951 | `					iP2 = pInstr->iP2;` |
|        19 | 13952 | `					p3  = pInstr->p3;` |
|        10 | 13953 | `				}else{` |
|        46 | 13954 | `					p3 = pInstr->p3;` |
|         - | 13955 | `				}` |
|        30 | 13956 | `			}` |
|        30 | 13957 | `		}` |
|   8080812 | 13958 | `	}` |
|  27986516 | 13959 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|    394427 | 13960 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|         - | 13961 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|         - | 13962 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|        34 | 13963 | `		iVmOp = 0;` |
|        15 | 13964 | `	}` |
|  27986521 | 13965 | `	if( iVmOp > 0 ){` |
|  27927915 | 13966 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    210457 | 13967 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|         - | 13968 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|     15589 | 13969 | `				iP1 = 1;` |
|      7797 | 13970 | `			}` |
|  27822689 | 13971 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|         - | 13972 | `			/* Namespace-qualify the class name for NEW */ {` |
|    796049 | 13973 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|    796049 | 13974 | `				VmInstr *pCallInstr = 0;` |
|    796049 | 13975 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|    788153 | 13976 | `					pCallInstr = pPeek;` |
|    788153 | 13977 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    394074 | 13978 | `				}` |
|    796049 | 13979 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|    780491 | 13980 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 13981 | `					sxu32 nLitForClass;` |
|    780491 | 13982 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|         - | 13983 | `					/* If the CALL handler qualified the name with FUNCTION` |
|         - | 13984 | `					 * imports, recover the original literal (recorded in the` |
|         - | 13985 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|         - | 13986 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|         - | 13987 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|         - | 13988 | `					 * with class imports. */` |
|    780491 | 13989 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|        55 | 13990 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|        30 | 13991 | `					}else{` |
|    780441 | 13992 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|         - | 13993 | `					}` |
|    780491 | 13994 | `					pPeek->iP1 = 0;` |
|    780491 | 13995 | `					if( !bAbsolute ){` |
|         - | 13996 | `						/* self/static/parent are resolved at runtime against the` |
|         - | 13997 | `						 * current class — never namespace-qualify them (else` |
|         - | 13998 | ``						 * `new self` in namespace N becomes "N\self"). Mirrors the`` |
|         - | 13999 | `						 * instanceof (IS_A) guard below. */` |
|    776575 | 14000 | `						ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nLitForClass);` |
|    776575 | 14001 | `						int isSpecialNew = 0;` |
|    776575 | 14002 | `						if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    761419 | 14003 | `							const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    761419 | 14004 | `							sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    761414 | 14005 | `							if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    765149 | 14006 | `								(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    380654 | 14007 | `								(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      7815 | 14008 | `								isSpecialNew = 1;` |
|      3905 | 14009 | `							}` |
|    384496 | 14010 | `						}` |
|    784153 | 14011 | `						if( isSpecialNew ){` |
|      7815 | 14012 | `							pPeek->iP2 = (sxi32)nLitForClass;` |
|      3910 | 14013 | `						}else{` |
|    761187 | 14014 | `							pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|         - | 14015 | `						}` |
|    384501 | 14016 | `					}else{` |
|      3921 | 14017 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|         - | 14018 | `					}` |
|    386454 | 14019 | `				}` |
|         - | 14020 | `			}` |
|    788471 | 14021 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    788471 | 14022 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|         - | 14023 | `				VmInstr *pPrev;` |
|    788153 | 14024 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|    788153 | 14025 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|         - | 14026 | `					/* Pop the call instruction, preserve named-arg map and` |
|         - | 14027 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|         - | 14028 | `					 * accumulator exactly like OP_CALL would have). */` |
|    788153 | 14029 | `					iP1 = pInstr->iP1;` |
|    788153 | 14030 | `					iP2 = pInstr->iP2;` |
|    788153 | 14031 | `					if( pInstr->p3 ){` |
|        65 | 14032 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|        30 | 14033 | `					}` |
|    788153 | 14034 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    394074 | 14035 | `				}` |
|    394079 | 14036 | `			}` |
|  27315652 | 14037 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|         - | 14038 | `			/* instanceof: right operand is a class name, not a constant.` |
|         - | 14039 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     78053 | 14040 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     78053 | 14041 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     78053 | 14042 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     78053 | 14043 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     78053 | 14044 | `				int isSpecialIs = 0;` |
|     78053 | 14045 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     78053 | 14046 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     78053 | 14047 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     78048 | 14048 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     78051 | 14049 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     39024 | 14050 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        12 | 14051 | `						isSpecialIs = 1;` |
|         5 | 14052 | `					}` |
|     39024 | 14053 | `				}` |
|     78053 | 14054 | `				pInstr->iP1 = 0;` |
|     78053 | 14055 | `				if( !isSpecialIs && !bAbsolute ){` |
|     78033 | 14056 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     39014 | 14057 | `				}` |
|     39029 | 14058 | `			}` |
|  26882395 | 14059 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|         - | 14060 | `			/* Prevent constant expansion for member/property names.` |
|         - | 14061 | `			 * The right child (member name) was just compiled — its LOADC` |
|         - | 14062 | `			 * should not trigger constant lookup. */` |
|   5439035 | 14063 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   5439035 | 14064 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   5201769 | 14065 | `				pInstr->iP1 = 0;` |
|   2600882 | 14066 | `			}` |
|   5439035 | 14067 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|         - | 14068 | `				/* Static member access,remember that */` |
|    405313 | 14069 | `				iP1 = 1;` |
|    405313 | 14070 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    405313 | 14071 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|    233367 | 14072 | `					p3 = pInstr->p3;` |
|    233367 | 14073 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    116681 | 14074 | `				}` |
|    202654 | 14075 | `			}` |
|         - | 14076 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|         - | 14077 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|         - | 14078 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|         - | 14079 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|   5439035 | 14080 | `			if( iP2 == PH7_MEMBER_READ ){` |
|   5439035 | 14081 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        42 | 14082 | `					iP2 = PH7_MEMBER_UNSET;` |
|   5439015 | 14083 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|     62353 | 14084 | `					iP2 = PH7_MEMBER_ISSET;` |
|   5407821 | 14085 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        17 | 14086 | `					iP2 = PH7_MEMBER_EMPTY;` |
|   5376639 | 14087 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|         - | 14088 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|   1042911 | 14089 | `					iP2 = PH7_MEMBER_WRITE;` |
|    521453 | 14090 | `				}` |
|   2719515 | 14091 | `			}` |
|   2719515 | 14092 | `		}` |
|         - | 14093 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|         - | 14094 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|         - | 14095 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|         - | 14096 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|         - | 14097 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
|  27920337 | 14098 | `		if( bFcc ){` |
|        81 | 14099 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|        81 | 14100 | `			iP2 = 0;` |
|        81 | 14101 | `			p3 = 0;` |
|        81 | 14102 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|        81 | 14103 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 14104 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|         - | 14105 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|         - | 14106 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|         - | 14107 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|        37 | 14108 | `				void *pMemberName = pInstr->p3;` |
|        37 | 14109 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|        37 | 14110 | `				if( pMemberName ){` |
|       ! 0 | 14111 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|       ! 0 | 14112 | `				}` |
|        37 | 14113 | `				iP1 = 2;` |
|        19 | 14114 | `			}else{` |
|        45 | 14115 | `				iP1 = 1;` |
|         - | 14116 | `			}` |
|        40 | 14117 | `		}` |
|         - | 14118 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|         - | 14119 | `		 * This is the primary emit path for user-visible calls. */` |
|  27920337 | 14120 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|   7371123 | 14121 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|   3685559 | 14122 | `		}` |
|         - | 14123 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  27920337 | 14124 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  13960166 | 14125 | `	}` |
|  27978943 | 14126 | `	if( nJmpIdx > 0 ){` |
|         - | 14127 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|    821225 | 14128 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|    821225 | 14129 | `		if( pInstr ){` |
|    821225 | 14130 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    410610 | 14131 | `		}` |
|    410610 | 14132 | `	}` |
|  27978943 | 14133 | `	return rc;` |
|  35816757 | 14134 | `}` |
|         - | 14135 | `/*` |
|         - | 14136 | ` * Compile a PHP expression.` |
|         - | 14137 | ` * According to the PHP language reference manual:` |
|         - | 14138 | ` *  Expressions are the most important building stones of PHP.` |
|         - | 14139 | ` *  In PHP, almost anything you write is an expression.` |
|         - | 14140 | ` *  The simplest yet most accurate way to define an expression` |
|         - | 14141 | ` *  is "anything that has a value".` |
|         - | 14142 | ` * If something goes wrong while compiling the expression,this` |
|         - | 14143 | ` * function takes care of generating the appropriate error` |
|         - | 14144 | ` * message.` |
|         - | 14145 | ` */` |
|         - | 14146 | `/*` |
|         - | 14147 | ` * Does this expression tree contain a comma OPERATOR node?` |
|         - | 14148 | ` *` |
|         - | 14149 | `` * PH7 shipped `,` as a lowest-precedence binary operator (IMP-0139-COMMA), so`` |
|         - | 14150 | `` * `(1, 2)` and `$x = (f(), $y)` compile and evaluate to the right operand.`` |
|         - | 14151 | ` * php 8 has no comma operator: its grammar only allows comma-separated` |
|         - | 14152 | ` * expression LISTS inside for(...) clauses (call arguments, array literals and` |
|         - | 14153 | ` * list() are split by the parser, never by this node). Accepting it changes the` |
|         - | 14154 | ` * meaning of source php rejects, which §10 classes as a bug — so every context` |
|         - | 14155 | ` * except for() now reports php's parse error.` |
|         - | 14156 | ` */` |
| 237449602 | 14157 | `static int GenStateTreeHasComma(ph7_expr_node *pNode)` |
|         5 | 14158 | `{` |
|         - | 14159 | `	ph7_expr_node **apArg;` |
|         - | 14160 | `	sxu32 n;` |
| 237449607 | 14161 | `	if( pNode == 0 ){` |
| 166866875 | 14162 | `		return 0;` |
|         - | 14163 | `	}` |
|  70582737 | 14164 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         6 | 14165 | `		return 1;` |
|         - | 14166 | `	}` |
|  70582728 | 14167 | `	if( GenStateTreeHasComma(pNode->pLeft) \|\| GenStateTreeHasComma(pNode->pRight)` |
|  70582729 | 14168 | `	 \|\| GenStateTreeHasComma(pNode->pCond) ){` |
|         6 | 14169 | `		return 1;` |
|         - | 14170 | `	}` |
|  70582729 | 14171 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  80507143 | 14172 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; n++ ){` |
|   9924419 | 14173 | `		if( GenStateTreeHasComma(apArg[n]) ){` |
|       ! 0 | 14174 | `			return 1;` |
|         - | 14175 | `		}` |
|   4962212 | 14176 | `	}` |
|  70582729 | 14177 | `	return 0;` |
| 118724806 | 14178 | `}` |
|  16139224 | 14179 | `static sxi32 PH7_CompileExpr(` |
|         - | 14180 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 14181 | `	sxi32 iFlags,        /* Control flags */` |
|         - | 14182 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|         - | 14183 | `	)` |
|         5 | 14184 | `{` |
|         - | 14185 | `	ph7_expr_node *pRoot;` |
|         - | 14186 | `	SySet sExprNode;` |
|         - | 14187 | `	SyToken *pEnd;` |
|         - | 14188 | `	sxi32 nExpr;` |
|         - | 14189 | `	sxi32 iNest;` |
|         - | 14190 | `	sxi32 rc;` |
|         - | 14191 | `	sxu32 nNullsafeBase;` |
|         - | 14192 | `	/* Initialize worker variables */` |
|  16139229 | 14193 | `	nExpr = 0;` |
|  16139229 | 14194 | `	pRoot = 0;` |
|         - | 14195 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|         - | 14196 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  16139229 | 14197 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  16139229 | 14198 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  16139229 | 14199 | `	SySetAlloc(&sExprNode,0x10);` |
|  16139229 | 14200 | `	rc = SXRET_OK;` |
|         - | 14201 | `	/* Delimit the expression */` |
|  16139229 | 14202 | `	pEnd = pGen->pIn;` |
|  16139229 | 14203 | `	iNest = 0;` |
| 126302013 | 14204 | `	while( pEnd < pGen->pEnd ){` |
| 120142737 | 14205 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 14206 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      4745 | 14207 | `			iNest++;` |
| 120140367 | 14208 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      4753 | 14209 | `			iNest--;` |
| 120135623 | 14210 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|   9980841 | 14211 | `			if( iNest <= 0 ){` |
|   9979953 | 14212 | `				break;` |
|         - | 14213 | `			}` |
|       444 | 14214 | `		}` |
| 110162789 | 14215 | `		pEnd++;` |
|         5 | 14216 | `	}` |
|  16139229 | 14217 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|    724261 | 14218 | `		SyToken *pEnd2 = pGen->pIn;` |
|    724261 | 14219 | `		iNest = 0;` |
|         - | 14220 | `		/* Stop at the first comma */` |
|   1585689 | 14221 | `		while( pEnd2 < pEnd ){` |
|    861435 | 14222 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|     46773 | 14223 | `				iNest++;` |
|    838051 | 14224 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|     46773 | 14225 | `				iNest--;` |
|    791283 | 14226 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      6061 | 14227 | `				if( iNest <= 0 ){` |
|         3 | 14228 | `					break;` |
|         - | 14229 | `				}` |
|      3027 | 14230 | `			}` |
|    861433 | 14231 | `			pEnd2++;` |
|         5 | 14232 | `		}` |
|    724261 | 14233 | `		if( pEnd2 <pEnd ){` |
|         3 | 14234 | `			pEnd = pEnd2;` |
|         1 | 14235 | `		}` |
|    362128 | 14236 | `	}` |
|  16139229 | 14237 | `	if( pEnd > pGen->pIn ){` |
|  16115893 | 14238 | `		SyToken *pTmp = pGen->pEnd;` |
|         - | 14239 | `		/* Swap delimiter */` |
|  16115893 | 14240 | `		pGen->pEnd = pEnd;` |
|         - | 14241 | `		/* Try to get an expression tree */` |
|  16115893 | 14242 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  16115888 | 14243 | `		if( rc == SXRET_OK && pRoot && pGen->nCommaExprOk < 1` |
|  15946363 | 14244 | `		 && GenStateTreeHasComma(pRoot) ){` |
|         - | 14245 | `			/* php has no comma operator outside a for() clause */` |
|         6 | 14246 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pRoot->pStart->nLine,` |
|         - | 14247 | `				"syntax error, unexpected token \",\"");` |
|         6 | 14248 | `			pGen->pEnd = pTmp;` |
|         6 | 14249 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14250 | `				SySetRelease(&sExprNode);` |
|       ! 0 | 14251 | `				return SXERR_ABORT;` |
|         - | 14252 | `			}` |
|         6 | 14253 | `			pGen->pIn = pEnd;` |
|         6 | 14254 | `			SySetRelease(&sExprNode);` |
|         6 | 14255 | `			SySetTruncate(&pGen->aNullsafeJmp,nNullsafeBase);` |
|         6 | 14256 | `			return SXRET_OK;` |
|         - | 14257 | `		}` |
|  16115889 | 14258 | `		if( rc == SXRET_OK && pRoot ){` |
|  16115705 | 14259 | `			rc = SXRET_OK;` |
|  16115705 | 14260 | `			if( xTreeValidator ){` |
|         - | 14261 | `				/* Call the upper layer validator callback */` |
|   1001075 | 14262 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    500535 | 14263 | `			}` |
|  16115705 | 14264 | `			if( rc != SXERR_ABORT ){` |
|         - | 14265 | `				/* Generate code for the given tree */` |
|  16115705 | 14266 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|         - | 14267 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|         - | 14268 | `				 * expression so they short-circuit to its end. */` |
|  16115705 | 14269 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|   8057850 | 14270 | `			}` |
|  16115705 | 14271 | `			nExpr = 1;` |
|   8057850 | 14272 | `		}` |
|         - | 14273 | `		/* Release the whole tree */` |
|  16115889 | 14274 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|         - | 14275 | `		/* Synchronize token stream */` |
|  16115889 | 14276 | `		pGen->pEnd = pTmp;` |
|  16115889 | 14277 | `		pGen->pIn  = pEnd;` |
|  16115889 | 14278 | `		if( rc == SXERR_ABORT ){` |
|        13 | 14279 | `			SySetRelease(&sExprNode);` |
|        13 | 14280 | `			return SXERR_ABORT;` |
|         - | 14281 | `		}` |
|   8057937 | 14282 | `	}` |
|  16139215 | 14283 | `	SySetRelease(&sExprNode);` |
|  16139215 | 14284 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|   8069617 | 14285 | `}` |
|         - | 14286 | `/*` |
|         - | 14287 | ` * Return a pointer to the node construct handler associated` |
|         - | 14288 | ` * with a given node type [i.e: string,integer,float,...].` |
|         - | 14289 | ` */` |
|   9202354 | 14290 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|         5 | 14291 | `{` |
|   9202359 | 14292 | `	if( nNodeType & PH7_TK_NUM ){` |
|         - | 14293 | `		/* Numeric literal: Either real or integer */` |
|   3774799 | 14294 | `		return PH7_CompileNumLiteral;` |
|   5427565 | 14295 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|         - | 14296 | `		/* Double quoted string */` |
|    126271 | 14297 | `		return PH7_CompileString;` |
|   5301299 | 14298 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|         - | 14299 | `		/* Single quoted string */` |
|   5301175 | 14300 | `		return PH7_CompileSimpleString;` |
|       129 | 14301 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|         - | 14302 | `		/* Heredoc */` |
|        73 | 14303 | `		return PH7_CompileHereDoc;` |
|        61 | 14304 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|         - | 14305 | `		/* Nowdoc */` |
|        55 | 14306 | `		return PH7_CompileNowDoc;` |
|         8 | 14307 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|         - | 14308 | `		/* Backtick quoted string */` |
|         6 | 14309 | `		return PH7_CompileBacktic;` |
|         - | 14310 | `	}` |
|         3 | 14311 | `	return 0;` |
|   4601182 | 14312 | `}` |
|         - | 14313 | `/*` |
|         - | 14314 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|         - | 14315 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|         - | 14316 | ` * in write context" parse error.` |
|         - | 14317 | ` */` |
|     23446 | 14318 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|         5 | 14319 | `{` |
|         - | 14320 | `	sxi32 rc;` |
|     23451 | 14321 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|     23449 | 14322 | `		return SXRET_OK;` |
|         - | 14323 | `	}` |
|         5 | 14324 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|         2 | 14325 | `		pNode ? pNode->pStart->nLine : 1,` |
|         - | 14326 | `		"Can't use nullsafe operator in write context");` |
|         3 | 14327 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     11728 | 14328 | `}` |
|         - | 14329 | `/*` |
|         - | 14330 | ` * Compile an unset() statement.` |
|         - | 14331 | ` * unset($var, $arr[$key], ...);` |
|         - | 14332 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|         - | 14333 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|         - | 14334 | ` * parent array before extracting the element to unset.` |
|         - | 14335 | ` */` |
|     26306 | 14336 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|         5 | 14337 | `{` |
|     26311 | 14338 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     26311 | 14339 | `	sxu32 nIdx = 0;` |
|         - | 14340 | `	SyString sName;` |
|         - | 14341 | `	sxi32 rc;` |
|         - | 14342 | `	/* Jump the 'unset' keyword */` |
|     26311 | 14343 | `	pGen->pIn++;` |
|         - | 14344 | `	/* Save delimiter */` |
|     26311 | 14345 | `	pTmp = pGen->pEnd;` |
|         - | 14346 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     26311 | 14347 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     26311 | 14348 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 14349 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|         - | 14350 | `		SyToken *pClose;` |
|     26311 | 14351 | `		pGen->pIn++;   /* Skip '(' */` |
|     26311 | 14352 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     26311 | 14353 | `		pEnd = pClose; /* Stop at ')' */` |
|     13153 | 14354 | `	}` |
|     26311 | 14355 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|         - | 14356 | `	/* Resolve the 'unset' builtin name once */` |
|     26311 | 14357 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      3893 | 14358 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3893 | 14359 | `		if( pObj == 0 ){` |
|       ! 0 | 14360 | `			return SXERR_ABORT;` |
|         - | 14361 | `		}` |
|      3893 | 14362 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      3893 | 14363 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1944 | 14364 | `	}` |
|         - | 14365 | `	/* Compile each comma-separated argument */` |
|     56847 | 14366 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     30541 | 14367 | `		if( pGen->pIn < pNext ){` |
|         - | 14368 | `			/*` |
|         - | 14369 | ``			 * A PLAIN variable (`unset($x)`, exactly two tokens: '$' and the name) drops a`` |
|         - | 14370 | `			 * single NAME binding, which the unset() builtin cannot express — all it ever` |
|         - | 14371 | `			 * receives is the value's slot index, and unsetting the slot destroys whatever` |
|         - | 14372 | `			 * else aliases it. Emit OP_UNSET_VAR with the name instead. Subscripts and` |
|         - | 14373 | ``			 * properties (`unset($a[k])`, `unset($o->p)`) keep the existing path, which`` |
|         - | 14374 | `			 * already removes just the element/property.` |
|         - | 14375 | `			 */` |
|     30536 | 14376 | `			if( &pGen->pIn[2] == pNext` |
|     18813 | 14377 | `				&& (pGen->pIn[0].nType & PH7_TK_DOLLAR)` |
|      7095 | 14378 | `				&& (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         - | 14379 | `				SyString *pVarName;` |
|     10637 | 14380 | `				char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      7088 | 14381 | `					pGen->pIn[1].sData.zString,pGen->pIn[1].sData.nByte);` |
|      7093 | 14382 | `				pVarName = (SyString *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(SyString));` |
|      7093 | 14383 | `				if( zDup == 0 \|\| pVarName == 0 ){` |
|       ! 0 | 14384 | `					PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - | 14385 | `						"Fatal, PH7 is running out of memory");` |
|       ! 0 | 14386 | `					return SXERR_ABORT;` |
|         - | 14387 | `				}` |
|      7093 | 14388 | `				SyStringInitFromBuf(pVarName,zDup,pGen->pIn[1].sData.nByte);` |
|      7093 | 14389 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,pVarName,0);` |
|      7093 | 14390 | `				pGen->pIn = pNext;` |
|      7093 | 14391 | `				if( pGen->pIn < pEnd ){` |
|      4231 | 14392 | `					pGen->pIn++; /* Jump the trailing comma */` |
|      2113 | 14393 | `				}` |
|      7093 | 14394 | `				continue;` |
|         - | 14395 | `			}` |
|     23453 | 14396 | `			pGen->pEnd = pNext;` |
|     23453 | 14397 | `			rc = PH7_CompileExpr(&(*pGen),` |
|         - | 14398 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|         - | 14399 | `				GenStateUnsetValidator);` |
|     23453 | 14400 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14401 | `				return SXERR_ABORT;` |
|         - | 14402 | `			}` |
|     23453 | 14403 | `			if( rc != SXERR_EMPTY ){` |
|         - | 14404 | `				/* Emit call for this single argument */` |
|     23451 | 14405 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     23451 | 14406 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|     23451 | 14407 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     11723 | 14408 | `			}` |
|     11724 | 14409 | `		}` |
|         - | 14410 | `		/* Jump trailing commas */` |
|     23459 | 14411 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|         7 | 14412 | `			pNext++;` |
|         1 | 14413 | `		}` |
|     23453 | 14414 | `		pGen->pIn = pNext;` |
|         5 | 14415 | `	}` |
|         - | 14416 | `	/* Skip past the closing ')' if present */` |
|     26311 | 14417 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     26311 | 14418 | `		pGen->pIn++;` |
|     13153 | 14419 | `	}` |
|         - | 14420 | `	/* Restore token stream */` |
|     26311 | 14421 | `	pGen->pEnd = pTmp;` |
|     26311 | 14422 | `	return SXRET_OK;` |
|     13158 | 14423 | `}` |
|         - | 14424 | `/*` |
|         - | 14425 | ` * PHP Language construct table.` |
|         - | 14426 | ` */` |
|         - | 14427 | `static const LangConstruct aLangConstruct[] = {` |
|         - | 14428 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|         - | 14429 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|         - | 14430 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|         - | 14431 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|         - | 14432 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|         - | 14433 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|         - | 14434 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|         - | 14435 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|         - | 14436 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|         - | 14437 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|         - | 14438 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|         - | 14439 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|         - | 14440 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|         - | 14441 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|         - | 14442 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|         - | 14443 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|         - | 14444 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|         - | 14445 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|         - | 14446 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|         - | 14447 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|         - | 14448 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|         - | 14449 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|         - | 14450 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|         - | 14451 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|         - | 14452 | `};` |
|         - | 14453 | `/*` |
|         - | 14454 | ` * Return a pointer to the statement handler routine associated` |
|         - | 14455 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|         - | 14456 | ` */` |
|   7825252 | 14457 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|         - | 14458 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|         - | 14459 | `	SyToken *pLookahed  /* Look-ahead token */` |
|         - | 14460 | `	)` |
|         5 | 14461 | `{` |
|   7825257 | 14462 | `	sxu32 n = 0;` |
|  31091354 | 14463 | `	for(;;){` |
|  62182713 | 14464 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|    469035 | 14465 | `			break;` |
|         - | 14466 | `		}` |
|  61713683 | 14467 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|   7356227 | 14468 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|       ! 0 | 14469 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|       ! 0 | 14470 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|         - | 14471 | `					/* 'static' (class context),return null */` |
|       ! 0 | 14472 | `					return 0;` |
|         - | 14473 | `				}` |
|       ! 0 | 14474 | `			}` |
|   7356222 | 14475 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|     11678 | 14476 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|      5846 | 14477 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|         - | 14478 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|         3 | 14479 | `				return 0;` |
|         - | 14480 | `			}` |
|         - | 14481 | `			/* Return a pointer to the handler.` |
|         - | 14482 | `			*/` |
|   7356225 | 14483 | `			return aLangConstruct[n].xConstruct;` |
|         - | 14484 | `		}` |
|  54357461 | 14485 | `		n++;` |
|         5 | 14486 | `	}` |
|    469035 | 14487 | `	if( pLookahed ){` |
|    469035 | 14488 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|     70109 | 14489 | `			return PH7_CompileClassInterface;` |
|    398931 | 14490 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|    340031 | 14491 | `			return PH7_CompileClass;` |
|     58905 | 14492 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      7869 | 14493 | `			return PH7_CompileTrait;` |
|         - | 14494 | `		}` |
|         - | 14495 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|         - | 14496 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|         - | 14497 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|         - | 14498 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     25518 | 14499 | `	}` |
|         - | 14500 | `	/* Not a language construct */` |
|     51041 | 14501 | `	return 0;` |
|   3912631 | 14502 | `}` |
|         - | 14503 | `/*` |
|         - | 14504 | ` * Check if the given keyword is in fact a PHP language construct.` |
|         - | 14505 | ` * Return TRUE on success. FALSE otheriwse.` |
|         - | 14506 | ` */` |
|     51038 | 14507 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|         5 | 14508 | `{` |
|         - | 14509 | `	int rc;` |
|     51043 | 14510 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     51043 | 14511 | `	if( rc == FALSE ){` |
|     50922 | 14512 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     15920 | 14513 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|         - | 14514 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|         - | 14515 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|         - | 14516 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|         - | 14517 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|         - | 14518 | `			*/` |
|         - | 14519 | `			){` |
|     50919 | 14520 | `				rc = TRUE;` |
|     25457 | 14521 | `		}` |
|     25461 | 14522 | `	}` |
|     51043 | 14523 | `	return rc;` |
|         5 | 14524 | `}` |
|         - | 14525 | `/*` |
|         - | 14526 | ` * Compile a PHP chunk.` |
|         - | 14527 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 14528 | ` * takes care of generating the appropriate error message.` |
|         - | 14529 | ` */` |
|         - | 14530 | `/*` |
|         - | 14531 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|         - | 14532 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|         - | 14533 | ` * the chunk token set it becomes the pending docblock. An existing` |
|         - | 14534 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|         - | 14535 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|         - | 14536 | ` * intervening non-declaration statements.` |
|         - | 14537 | ` */` |
|  17034818 | 14538 | `static void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|         5 | 14539 | `{` |
|  17034823 | 14540 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  17034823 | 14541 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  17034823 | 14542 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 14543 | `	sxu32 nIdx, n;` |
|  17034818 | 14544 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   3443887 | 14545 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|         - | 14546 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|         - | 14547 | `		 * indexes do not map to the sidecar */` |
|  13590943 | 14548 | `		return;` |
|         - | 14549 | `	}` |
|   3443885 | 14550 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|         - | 14551 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|         - | 14552 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|   3443885 | 14553 | `	SySetReset(&pGen->aPendingAttrs);` |
|  10333193 | 14554 | `	for( n = 0 ; n < nT ; n++ ){` |
|   6889313 | 14555 | `		if( aT[n].nTokIdx != nIdx ){` |
|   6881363 | 14556 | `			continue;` |
|         - | 14557 | `		}` |
|      7955 | 14558 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|        29 | 14559 | `			pGen->sPendingDoc = aT[n].sText;` |
|      7943 | 14560 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|      7931 | 14561 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|      3963 | 14562 | `		}` |
|      3980 | 14563 | `	}` |
|   8517414 | 14564 | `}` |
|         - | 14565 | `/*` |
|         - | 14566 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|         - | 14567 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|         - | 14568 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|         - | 14569 | ` */` |
|   4470826 | 14570 | `static void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|         5 | 14571 | `{` |
|         - | 14572 | `	char *zDup;` |
|   4470831 | 14573 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|   4470811 | 14574 | `		return;` |
|         - | 14575 | `	}` |
|        35 | 14576 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 | 14577 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|        25 | 14578 | `	if( zDup ){` |
|        25 | 14579 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|        10 | 14580 | `	}` |
|        25 | 14581 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|   2235418 | 14582 | `}` |
|         - | 14583 | `/*` |
|         - | 14584 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|         - | 14585 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|         - | 14586 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|         - | 14587 | ` * names may point into the token text, which must outlive the raw script` |
|         - | 14588 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|         - | 14589 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|         - | 14590 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|         - | 14591 | ` */` |
|      7940 | 14592 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|         5 | 14593 | `{` |
|         - | 14594 | `	SySet *pToken;` |
|         - | 14595 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|         - | 14596 | `	char *zSpan;` |
|      7945 | 14597 | `	sxi32 rc = SXRET_OK;` |
|      7945 | 14598 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|       ! 0 | 14599 | `		return SXRET_OK;` |
|         - | 14600 | `	}` |
|     11915 | 14601 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      3970 | 14602 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|      7945 | 14603 | `	if( zSpan == 0 ){` |
|       ! 0 | 14604 | `		return SXRET_OK;` |
|         - | 14605 | `	}` |
|         - | 14606 | `	/* The token set must outlive compilation too: interned operands may` |
|         - | 14607 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|         - | 14608 | `	 * the number of attribute declarations in the program. */` |
|      7945 | 14609 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      7945 | 14610 | `	if( pToken == 0 ){` |
|       ! 0 | 14611 | `		return SXRET_OK;` |
|         - | 14612 | `	}` |
|      7945 | 14613 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      7945 | 14614 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|      7945 | 14615 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|      7945 | 14616 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|      7945 | 14617 | `	pSavedIn = pGen->pIn;` |
|      7945 | 14618 | `	pSavedEnd = pGen->pEnd;` |
|      7949 | 14619 | `	while( pIn < pEnd ){` |
|         - | 14620 | `		ph7_attribute sAttr;` |
|         - | 14621 | `		SyBlob sFQN;` |
|      7949 | 14622 | `		int bAbsolute = 0;` |
|      7949 | 14623 | `		SyZero(&sAttr,sizeof(sAttr));` |
|      7949 | 14624 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|      7949 | 14625 | `		sAttr.nLine = pIn->nLine;` |
|      7949 | 14626 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|        75 | 14627 | `			bAbsolute = 1;` |
|        75 | 14628 | `			pIn++;` |
|        35 | 14629 | `		}` |
|      7949 | 14630 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7949 | 14631 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      7949 | 14632 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|      7949 | 14633 | `			pIn++;` |
|      7949 | 14634 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       ! 0 | 14635 | `				SyBlobAppend(&sFQN,"\\",1);` |
|       ! 0 | 14636 | `				pIn++;` |
|       ! 0 | 14637 | `				continue;` |
|         - | 14638 | `			}` |
|      7949 | 14639 | `			break;` |
|       ! 0 | 14640 | `		}` |
|      7949 | 14641 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|         - | 14642 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|         - | 14643 | `			 * this feature; never turn it into a new fatal) */` |
|       ! 0 | 14644 | `			SyBlobRelease(&sFQN);` |
|       ! 0 | 14645 | `			break;` |
|         - | 14646 | `		}` |
|         - | 14647 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|         - | 14648 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|         - | 14649 | `		{` |
|      7949 | 14650 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|      7949 | 14651 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|      7949 | 14652 | `			char *zDup = 0;` |
|      7949 | 14653 | `			if( !bAbsolute ){` |
|      7879 | 14654 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|      7879 | 14655 | `				if( pImp ){` |
|       ! 0 | 14656 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|       ! 0 | 14657 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|       ! 0 | 14658 | `					if( zDup ){` |
|       ! 0 | 14659 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|       ! 0 | 14660 | `					}` |
|      7879 | 14661 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - | 14662 | `					SyBlob sTmp;` |
|       ! 0 | 14663 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|       ! 0 | 14664 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       ! 0 | 14665 | `					SyBlobAppend(&sTmp,"\\",1);` |
|       ! 0 | 14666 | `					SyBlobAppend(&sTmp,zName,nName);` |
|       ! 0 | 14667 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       ! 0 | 14668 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|       ! 0 | 14669 | `					if( zDup ){` |
|       ! 0 | 14670 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|       ! 0 | 14671 | `					}` |
|       ! 0 | 14672 | `					SyBlobRelease(&sTmp);` |
|       ! 0 | 14673 | `				}` |
|      3937 | 14674 | `			}` |
|      7949 | 14675 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|      7949 | 14676 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|      7949 | 14677 | `				if( zDup ){` |
|      7949 | 14678 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|      3972 | 14679 | `				}` |
|      3972 | 14680 | `			}` |
|         - | 14681 | `		}` |
|      7949 | 14682 | `		SyBlobRelease(&sFQN);` |
|      7949 | 14683 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 14684 | `			SyToken *pArgsEnd;` |
|      7845 | 14685 | `			pIn++;` |
|      7845 | 14686 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|     15699 | 14687 | `			while( pIn < pArgsEnd ){` |
|      7859 | 14688 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|      7859 | 14689 | `				sxi32 iDepth = 0;` |
|         - | 14690 | `				ph7_attr_arg sArgRec;` |
|     78037 | 14691 | `				while( pArgStop < pArgsEnd ){` |
|     70199 | 14692 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        11 | 14693 | `						iDepth++;` |
|     70194 | 14694 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        11 | 14695 | `						iDepth--;` |
|     70184 | 14696 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|        17 | 14697 | `						break;` |
|         - | 14698 | `					}` |
|     70183 | 14699 | `					pArgStop++;` |
|         5 | 14700 | `				}` |
|      7859 | 14701 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|      7859 | 14702 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      7854 | 14703 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      7836 | 14704 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        28 | 14705 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|         9 | 14706 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|        19 | 14707 | `					if( zN ){` |
|        19 | 14708 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|         9 | 14709 | `					}` |
|        19 | 14710 | `					pArgStart += 2;` |
|         9 | 14711 | `				}` |
|      7859 | 14712 | `				if( pArgStart < pArgStop ){` |
|         - | 14713 | `					SySet *pInstrContainer;` |
|      7859 | 14714 | `					pGen->pIn = pArgStart;` |
|      7859 | 14715 | `					pGen->pEnd = pArgStop;` |
|      7859 | 14716 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7859 | 14717 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|      7859 | 14718 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7859 | 14719 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7859 | 14720 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7859 | 14721 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 14722 | `						pGen->pIn = pSavedIn;` |
|       ! 0 | 14723 | `						pGen->pEnd = pSavedEnd;` |
|       ! 0 | 14724 | `						return SXERR_ABORT;` |
|         - | 14725 | `					}` |
|      7859 | 14726 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|      3927 | 14727 | `				}` |
|      7859 | 14728 | `				pIn = pArgStop;` |
|      7859 | 14729 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 | 14730 | `					pIn++;` |
|         8 | 14731 | `				}` |
|         5 | 14732 | `			}` |
|      7845 | 14733 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|      3920 | 14734 | `		}` |
|      7949 | 14735 | `		SySetPut(pOut,(const void *)&sAttr);` |
|      7949 | 14736 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|         5 | 14737 | `			pIn++;` |
|         5 | 14738 | `			continue;` |
|         - | 14739 | `		}` |
|      7945 | 14740 | `		break;` |
|       ! 0 | 14741 | `	}` |
|      7945 | 14742 | `	pGen->pIn = pSavedIn;` |
|      7945 | 14743 | `	pGen->pEnd = pSavedEnd;` |
|      7945 | 14744 | `	return SXRET_OK;` |
|      3975 | 14745 | `}` |
|         - | 14746 | `/*` |
|         - | 14747 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|         - | 14748 | ` * every recorded group into pOut and clear the pending list.` |
|         - | 14749 | ` */` |
|   4470830 | 14750 | `static sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|         5 | 14751 | `{` |
|   4470835 | 14752 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|         - | 14753 | `	sxu32 n;` |
|         - | 14754 | `	sxi32 rc;` |
|   4478761 | 14755 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|      7931 | 14756 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|      7931 | 14757 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 14758 | `			return SXERR_ABORT;` |
|         - | 14759 | `		}` |
|      3968 | 14760 | `	}` |
|   4470835 | 14761 | `	SySetReset(&pGen->aPendingAttrs);` |
|   4470835 | 14762 | `	return SXRET_OK;` |
|   2235420 | 14763 | `}` |
|         - | 14764 | `/*` |
|         - | 14765 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|         - | 14766 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|         - | 14767 | ` * the main token stream, so the sidecar indexes map directly.` |
|         - | 14768 | ` */` |
|   2154252 | 14769 | `static sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|         5 | 14770 | `{` |
|   2154257 | 14771 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   2154257 | 14772 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   2154257 | 14773 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 14774 | `	sxu32 nIdx, n;` |
|         - | 14775 | `	sxi32 rc;` |
|   2154252 | 14776 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|    556327 | 14777 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   1597935 | 14778 | `		return SXRET_OK;` |
|         - | 14779 | `	}` |
|    556327 | 14780 | `	nIdx = (sxu32)(pTok - pBase);` |
|   1668973 | 14781 | `	for( n = 0 ; n < nT ; n++ ){` |
|   1112651 | 14782 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|        16 | 14783 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|        16 | 14784 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14785 | `				return SXERR_ABORT;` |
|         - | 14786 | `			}` |
|         7 | 14787 | `		}` |
|    556328 | 14788 | `	}` |
|    556327 | 14789 | `	return SXRET_OK;` |
|   1077131 | 14790 | `}` |
|  12594382 | 14791 | `static sxi32 GenStateCompileChunk(` |
|         - | 14792 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 14793 | `	sxi32 iFlags         /* Compile flags */` |
|         - | 14794 | `	)` |
|         5 | 14795 | `{` |
|         - | 14796 | `	ProcLangConstruct xCons;` |
|         - | 14797 | `	sxi32 rc;` |
|  12594387 | 14798 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|   7300097 | 14799 | `	for(;;){` |
|  13597293 | 14800 | `		int bStmtIsDeclare = 0;` |
|  13597293 | 14801 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 14802 | `			/* No more input to process */` |
|     72631 | 14803 | `			break;` |
|         - | 14804 | `		}` |
|         - | 14805 | `		/* Bind a directly-preceding docblock to this statement */` |
|  13524667 | 14806 | `		GenStateSetPendingDoc(&(*pGen));` |
|  13524667 | 14807 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - | 14808 | `			/* php: a statement-position attribute group must be followed by a` |
|         - | 14809 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|         - | 14810 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|         - | 14811 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|         - | 14812 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|      7843 | 14813 | `			int bAttrTarget = 0;` |
|      7838 | 14814 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|      3955 | 14815 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|      7781 | 14816 | `				bAttrTarget = 1;` |
|      3951 | 14817 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        63 | 14818 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        62 | 14819 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|        17 | 14820 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|         4 | 14821 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|         4 | 14822 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|         1 | 14823 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|        63 | 14824 | `					bAttrTarget = 1;` |
|        31 | 14825 | `				}` |
|        31 | 14826 | `			}` |
|      7843 | 14827 | `			if( !bAttrTarget ){` |
|       ! 0 | 14828 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 14829 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|       ! 0 | 14830 | `					&pGen->pIn->sData);` |
|       ! 0 | 14831 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 14832 | `					break;` |
|         - | 14833 | `				}` |
|       ! 0 | 14834 | `				SySetReset(&pGen->aPendingAttrs);` |
|       ! 0 | 14835 | `			}` |
|      3919 | 14836 | `		}` |
|         - | 14837 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|         - | 14838 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  13524667 | 14839 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   7868095 | 14840 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   7868095 | 14841 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|        47 | 14842 | `				bStmtIsDeclare = 1;` |
|        21 | 14843 | `			}` |
|   3934045 | 14844 | `		}` |
|  13524667 | 14845 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|         - | 14846 | `			/* Any non-declare top-level statement locks the strict_types` |
|         - | 14847 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|   1002879 | 14848 | `			pGen->bStrictTypesLocked = 1;` |
|    501437 | 14849 | `		}` |
|  13524667 | 14850 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 14851 | `			/* Compile block */` |
|      3933 | 14852 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      3933 | 14853 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14854 | `				break;` |
|         - | 14855 | `			}` |
|      1969 | 14856 | `		}else{` |
|  13520739 | 14857 | `			xCons = 0;` |
|  13520739 | 14858 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|         - | 14859 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|         - | 14860 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|         - | 14861 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|     42869 | 14862 | `				xCons = PH7_CompileClassModifiers;` |
|  13499307 | 14863 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|         - | 14864 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|         - | 14865 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|      3925 | 14866 | `				xCons = PH7_CompileEnum;` |
|  13475915 | 14867 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   7825257 | 14868 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - | 14869 | `				/* Try to extract a language construct handler */` |
|   7825257 | 14870 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|   7825257 | 14871 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|        13 | 14872 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 14873 | `						"Syntax error: Unexpected keyword '%z'",` |
|         8 | 14874 | `						&pGen->pIn->sData);` |
|         9 | 14875 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 14876 | `						break;` |
|         - | 14877 | `					}` |
|         - | 14878 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|         - | 14879 | `					 * this erroneous statement.` |
|         - | 14880 | `					 */` |
|         9 | 14881 | `					xCons = PH7_ErrorRecover;` |
|         4 | 14882 | `				}` |
|   9561329 | 14883 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    417385 | 14884 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|         - | 14885 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|       117 | 14886 | `				xCons = PH7_CompileLabel;` |
|        56 | 14887 | `			}` |
|  13520739 | 14888 | `			if( xCons == 0 ){` |
|         - | 14889 | `				/* Assume an expression an try to compile it */` |
|   5699621 | 14890 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   5699621 | 14891 | `				if(  rc != SXERR_EMPTY ){` |
|         - | 14892 | `					/* Pop l-value */` |
|   5699471 | 14893 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   2849733 | 14894 | `				}` |
|   2849813 | 14895 | `			}else{` |
|         - | 14896 | `				/* Go compile the sucker */` |
|   7821123 | 14897 | `				rc = xCons(&(*pGen));` |
|         - | 14898 | `			}` |
|  13520739 | 14899 | `			if( rc == SXERR_ABORT ){` |
|         - | 14900 | `				/* Request to abort compilation */` |
|        13 | 14901 | `				break;` |
|         - | 14902 | `			}` |
|         - | 14903 | `		}` |
|         - | 14904 | `		/* Ignore trailing semi-colons ';' */` |
|  23203161 | 14905 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|   9678509 | 14906 | `			pGen->pIn++;` |
|         5 | 14907 | `		}` |
|  13524657 | 14908 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|         - | 14909 | `			/* Compile a single statement and return */` |
|  12521751 | 14910 | `			break;` |
|         - | 14911 | `		}` |
|         - | 14912 | `		/* LOOP ONE */` |
|         - | 14913 | `		/* LOOP TWO */` |
|         - | 14914 | `		/* LOOP THREE */` |
|         - | 14915 | `		/* LOOP FOUR */` |
|         5 | 14916 | `	}` |
|         - | 14917 | `	/* Return compilation status */` |
|  12594387 | 14918 | `	return rc;` |
|         5 | 14919 | `}` |
|         - | 14920 | `/*` |
|         - | 14921 | ` * Compile a Raw PHP chunk.` |
|         - | 14922 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 14923 | ` * takes care of generating the appropriate error message.` |
|         - | 14924 | ` */` |
|     72638 | 14925 | `static sxi32 PH7_CompilePHP(` |
|         - | 14926 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 14927 | `	SySet *pTokenSet,     /* Token set */` |
|         - | 14928 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|         - | 14929 | `	)` |
|         5 | 14930 | `{` |
|     72643 | 14931 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|         - | 14932 | `	sxi32 rc;` |
|         - | 14933 | `	/* Reset the token set (and its trivia sidecar) */` |
|     72643 | 14934 | `	SySetReset(&(*pTokenSet));` |
|     72643 | 14935 | `	SySetReset(&pGen->aTrivia);` |
|         - | 14936 | `	/* Mark as the default token set */` |
|     72643 | 14937 | `	pGen->pTokenSet = &(*pTokenSet);` |
|         - | 14938 | `	/* Advance the stream cursor */` |
|     72643 | 14939 | `	pGen->pRawIn++;` |
|         - | 14940 | `	/* Tokenize the PHP chunk first */` |
|     72643 | 14941 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|         - | 14942 | `	/* Point to the head and tail of the token stream. */` |
|     72643 | 14943 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     72643 | 14944 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|     72643 | 14945 | `	if( is_expr ){` |
|       ! 0 | 14946 | `		rc = SXERR_EMPTY;` |
|       ! 0 | 14947 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - | 14948 | `			/* A simple expression,compile it */` |
|       ! 0 | 14949 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 | 14950 | `		}` |
|         - | 14951 | `		/* Emit the DONE instruction */` |
|       ! 0 | 14952 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       ! 0 | 14953 | `		return SXRET_OK;` |
|         - | 14954 | `	}` |
|     72643 | 14955 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - | 14956 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - | 14957 | `		/*` |
|         - | 14958 | `		 * Shortcut syntax for the 'echo' language construct.` |
|         - | 14959 | `		 * According to the PHP reference manual:` |
|         - | 14960 | `		 *  echo() also has a shortcut syntax, where you can` |
|         - | 14961 | `		 *  immediately follow` |
|         - | 14962 | `		 *  the opening tag with an equals sign as follows:` |
|         - | 14963 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|         - | 14964 | `		 * Symisc extension:` |
|         - | 14965 | `		 *   This short syntax works with all PHP opening` |
|         - | 14966 | `		 *   tags unlike the default PHP engine that handle` |
|         - | 14967 | `		 *   only short tag.` |
|         - | 14968 | `		 */` |
|         - | 14969 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|         3 | 14970 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|         3 | 14971 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|         3 | 14972 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|         3 | 14973 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|         3 | 14974 | `		if( rc != SXERR_EMPTY ){` |
|         3 | 14975 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 | 14976 | `		}` |
|         3 | 14977 | `		return SXRET_OK;` |
|         - | 14978 | `	}` |
|         - | 14979 | `	/* Compile the PHP chunk */` |
|     72641 | 14980 | `	rc = GenStateCompileChunk(pGen,0);` |
|         - | 14981 | `	/* Fix exceptions jumps */` |
|     72641 | 14982 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - | 14983 | `	/* Fix gotos now, the jump destination is resolved */` |
|     72641 | 14984 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|         3 | 14985 | `		rc = SXERR_ABORT;` |
|         1 | 14986 | `	}` |
|         - | 14987 | `	/* Reset container */` |
|     72641 | 14988 | `	SySetReset(&pGen->aGoto);` |
|     72641 | 14989 | `	SySetReset(&pGen->aLabel);` |
|     72641 | 14990 | `	SySetReset(&pGen->aNullsafeJmp);` |
|         - | 14991 | `	/* Compilation result */` |
|     72641 | 14992 | `	return rc;` |
|     36324 | 14993 | `}` |
|         - | 14994 | `/*` |
|         - | 14995 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|         - | 14996 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|         - | 14997 | ` * This is the only compile interface exported from this file.` |
|         - | 14998 | ` */` |
|     75876 | 14999 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|         - | 15000 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|         - | 15001 | `	SyString *pScript,  /* Script to compile */` |
|         - | 15002 | `	sxi32 iFlags        /* Compile flags */` |
|         - | 15003 | `	)` |
|         5 | 15004 | `{` |
|         - | 15005 | `	SySet aPhpToken,aRawToken;` |
|         - | 15006 | `	ph7_gen_state *pCodeGen;` |
|         - | 15007 | `	ph7_value *pRawObj;` |
|         - | 15008 | `	sxu32 nObjIdx;` |
|         - | 15009 | `	sxi32 nRawObj;` |
|         - | 15010 | `	int is_expr;` |
|         - | 15011 | `	sxi8 bSavedStrict;` |
|         - | 15012 | `	sxi8 bSavedStrictLocked;` |
|         - | 15013 | `	SyToken *pSavedIn,*pSavedEnd;` |
|         - | 15014 | `	sxi32 rc;` |
|     75881 | 15015 | `	sxu32 nBaseLine = 1;` |
|     75881 | 15016 | `	if( pScript->nByte < 1 ){` |
|         - | 15017 | `		/* Nothing to compile */` |
|       ! 0 | 15018 | `		return PH7_OK;` |
|         - | 15019 | `	}` |
|         - | 15020 | `	/* php skips a "#!" shebang on the first line of a CLI script: consume it` |
|         - | 15021 | `	 * (including its newline) so it is not echoed as inline text, and bump the` |
|         - | 15022 | `	 * base line to 2 so the code below still reports php-matching line numbers. */` |
|     75881 | 15023 | `	if( pScript->nByte >= 2 && pScript->zString[0]=='#' && pScript->zString[1]=='!' ){` |
|         3 | 15024 | `		const char *z = pScript->zString;` |
|         3 | 15025 | `		const char *zEnd = &z[pScript->nByte];` |
|        39 | 15026 | `		while( z < zEnd && z[0] != '\n' ){ z++; }` |
|         3 | 15027 | `		if( z < zEnd ){ z++; } /* consume the newline too */` |
|         3 | 15028 | `		pScript->nByte -= (sxu32)(z - pScript->zString);` |
|         3 | 15029 | `		pScript->zString = z;` |
|         3 | 15030 | `		nBaseLine = 2;` |
|         3 | 15031 | `		if( pScript->nByte < 1 ){` |
|       ! 0 | 15032 | `			return PH7_OK;` |
|         - | 15033 | `		}` |
|         1 | 15034 | `	}` |
|         - | 15035 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|         - | 15036 | `	 * file's flags so include/require restore them on return. */` |
|     75881 | 15037 | `	pCodeGen = &pVm->sCodeGen;` |
|         - | 15038 | ``	/* aPhpToken below is a LOCAL token set: once it is released at `cleanup`, any`` |
|         - | 15039 | `	 * pGen->pIn/pEnd still pointing into it DANGLE. PH7_VmEmitInstr reads pIn to stamp` |
|         - | 15040 | `	 * each instruction's source line, and instructions are still emitted after this` |
|         - | 15041 | `	 * function returns (VmEvalChunk's trailing OP_DONE) -- a use-after-free ASan caught` |
|         - | 15042 | `	 * immediately. Save the caller's cursor and restore it on the way out, so an` |
|         - | 15043 | `	 * enclosing compile keeps its (live) tokens and the top level goes back to NULL. */` |
|     75881 | 15044 | `	pSavedIn = pCodeGen->pIn;` |
|     75881 | 15045 | `	pSavedEnd = pCodeGen->pEnd;` |
|     75881 | 15046 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|     75881 | 15047 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|     75881 | 15048 | `	pCodeGen->bStrictTypes = 0;` |
|     75881 | 15049 | `	pCodeGen->bStrictTypesLocked = 0;` |
|         - | 15050 | `	/* Initialize the tokens containers */` |
|     75881 | 15051 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|     75881 | 15052 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|     75881 | 15053 | `	SySetAlloc(&aPhpToken,0xc0);` |
|     75881 | 15054 | `	is_expr = 0;` |
|     75881 | 15055 | `	if( iFlags & PH7_PHP_ONLY ){` |
|         - | 15056 | `		SyToken sTmp;` |
|         - | 15057 | `		/* PHP only: -*/` |
|     62335 | 15058 | `		sTmp.nLine = 1;` |
|     62335 | 15059 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|     62335 | 15060 | `		sTmp.pUserData = 0;` |
|     62335 | 15061 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|     62335 | 15062 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|     62335 | 15063 | `		if( iFlags & PH7_PHP_EXPR ){` |
|         - | 15064 | `			/* A simple PHP expression */` |
|       ! 0 | 15065 | `			is_expr = 1;` |
|       ! 0 | 15066 | `		}` |
|     31170 | 15067 | `	}else{` |
|         - | 15068 | `		/* Tokenize raw text */` |
|     13551 | 15069 | `		SySetAlloc(&aRawToken,32);` |
|     13551 | 15070 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken,nBaseLine);` |
|         - | 15071 | `	}` |
|         - | 15072 | `	/* Process high-level tokens */` |
|     75881 | 15073 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|     75881 | 15074 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|     75881 | 15075 | `	rc = PH7_OK;` |
|     75881 | 15076 | `	if( is_expr ){` |
|         - | 15077 | `		/* Compile the expression */` |
|       ! 0 | 15078 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|       ! 0 | 15079 | `		goto cleanup;` |
|         - | 15080 | `	}` |
|     75881 | 15081 | `	nObjIdx = 0;` |
|         - | 15082 | `	/* Each compilation unit starts in the global namespace.` |
|         - | 15083 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|         - | 15084 | `	 * preventing namespace bleeding across include()d files. */` |
|     75881 | 15085 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|         - | 15086 | `	/* Start the compilation process */` |
|     44716 | 15087 | `	for(;;){` |
|    162063 | 15088 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|     75869 | 15089 | `			break; /* No more tokens to process */` |
|         - | 15090 | `		}` |
|     86199 | 15091 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|         - | 15092 | `			/* Compile the PHP chunk */` |
|     72643 | 15093 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|     72643 | 15094 | `			if( rc == SXERR_ABORT ){` |
|        16 | 15095 | `				break;` |
|         - | 15096 | `			}` |
|     72631 | 15097 | `			continue;` |
|         - | 15098 | `		}` |
|         - | 15099 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|     13561 | 15100 | `		nRawObj = 0;` |
|     27117 | 15101 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|         - | 15102 | `			/* Consume the raw chunk without any processing */` |
|     13561 | 15103 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|     13561 | 15104 | `			if( pRawObj == 0 ){` |
|       ! 0 | 15105 | `				rc = SXERR_MEM;` |
|       ! 0 | 15106 | `				break;` |
|         - | 15107 | `			}` |
|         - | 15108 | `			/* Mark as constant and emit the load constant instruction */` |
|     13561 | 15109 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|     13561 | 15110 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     13561 | 15111 | `			++nRawObj;` |
|     13561 | 15112 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|         5 | 15113 | `		}` |
|     13561 | 15114 | `		if( nRawObj > 0 ){` |
|         - | 15115 | `			/* Emit the consume instruction */` |
|     13561 | 15116 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      6778 | 15117 | `		}` |
|     37943 | 15118 | `	}` |
|     37938 | 15119 | `cleanup:` |
|         - | 15120 | `	/* Drop the cursor into the token set BEFORE the set is freed (see above). */` |
|     75881 | 15121 | `	pCodeGen->pIn = pSavedIn;` |
|     75881 | 15122 | `	pCodeGen->pEnd = pSavedEnd;` |
|     75881 | 15123 | `	SySetRelease(&aRawToken);` |
|     75881 | 15124 | `	SySetRelease(&aPhpToken);` |
|         - | 15125 | `	/* Restore outer file's strict_types scope */` |
|     75881 | 15126 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|     75881 | 15127 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|     75881 | 15128 | `	return rc;` |
|     37943 | 15129 | `}` |
|         - | 15130 | `/*` |
|         - | 15131 | ` * Utility routines.Initialize the code generator.` |
|         - | 15132 | ` */` |
|      3888 | 15133 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|         - | 15134 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 15135 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 15136 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 15137 | `	)` |
|         5 | 15138 | `{` |
|      3893 | 15139 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 15140 | `	/* Zero the structure */` |
|      3893 | 15141 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|         - | 15142 | `	/* Initial state */` |
|      3893 | 15143 | `	pGen->pVm  = &(*pVm);` |
|      3893 | 15144 | `	pGen->xErr = xErr;` |
|      3893 | 15145 | `	pGen->pErrData = pErrData;` |
|      3893 | 15146 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|      3893 | 15147 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|      3893 | 15148 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|      3893 | 15149 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|      3893 | 15150 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|      3893 | 15151 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3893 | 15152 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3893 | 15153 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|      3893 | 15154 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|         - | 15155 | `	/* Error log buffer */` |
|      3893 | 15156 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         - | 15157 | `	/* General purpose working buffer */` |
|      3893 | 15158 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         - | 15159 | `	/* Namespace state */` |
|      3893 | 15160 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|      3893 | 15161 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|      3893 | 15162 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|      3893 | 15163 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 15164 | `	/* Create the global scope */` |
|      3893 | 15165 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|         - | 15166 | `	/* Point to the global scope */` |
|      3893 | 15167 | `	pGen->pCurrent = &pGen->sGlobal;` |
|      3893 | 15168 | `	return SXRET_OK;` |
|         5 | 15169 | `}` |
|         - | 15170 | `/*` |
|         - | 15171 | ` * Utility routines. Reset the code generator to it's initial state.` |
|         - | 15172 | ` */` |
|     79304 | 15173 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|         - | 15174 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 15175 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 15176 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 15177 | `	)` |
|         5 | 15178 | `{` |
|     79309 | 15179 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 15180 | `	GenBlock *pBlock,*pParent;` |
|         - | 15181 | `	/* Reset state */` |
|     79309 | 15182 | `	SySetReset(&pGen->aLabel);` |
|     79309 | 15183 | `	SySetReset(&pGen->aGoto);` |
|     79309 | 15184 | `	SySetReset(&pGen->aNullsafeJmp);` |
|     79309 | 15185 | `	SySetReset(&pGen->aTrivia);` |
|     79309 | 15186 | `	SySetReset(&pGen->aPendingAttrs);` |
|     79309 | 15187 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|     79309 | 15188 | `	SyBlobRelease(&pGen->sErrBuf);` |
|     79309 | 15189 | `	SyBlobRelease(&pGen->sWorker);` |
|     79309 | 15190 | `	SyBlobRelease(&pGen->sNamespace);` |
|     79309 | 15191 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     79309 | 15192 | `	SyHashRelease(&pGen->hUseImports);` |
|     79309 | 15193 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|     79309 | 15194 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     79309 | 15195 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|     79309 | 15196 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     79309 | 15197 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 15198 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|         - | 15199 | `	 * They intern variable names and literal strings that are referenced by` |
|         - | 15200 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|         - | 15201 | `	 * Releasing them would either leak the interned strings or require freeing` |
|         - | 15202 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|         - | 15203 | `	 * number of unique names, which is acceptable. */` |
|         - | 15204 | `	/* Point to the global scope */` |
|     79309 | 15205 | `	pBlock = pGen->pCurrent;` |
|     79309 | 15206 | `	while( pBlock->pParent != 0 ){` |
|       ! 0 | 15207 | `		pParent = pBlock->pParent;` |
|       ! 0 | 15208 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 15209 | `		pBlock = pParent;` |
|       ! 0 | 15210 | `	}` |
|     79309 | 15211 | `	pGen->xErr = xErr;` |
|     79309 | 15212 | `	pGen->pErrData = pErrData;` |
|     79309 | 15213 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     79309 | 15214 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|     79309 | 15215 | `	pGen->pIn = pGen->pEnd = 0;` |
|     79309 | 15216 | `	pGen->nErr = 0;` |
|     79309 | 15217 | `	return SXRET_OK;` |
|         5 | 15218 | `}` |
|         - | 15219 | `/*` |
|         - | 15220 | ` * Save the code generator's compile-position state and hand the live generator a` |
|         - | 15221 | ` * fresh, empty one for a NESTED compilation unit.` |
|         - | 15222 | ` *` |
|         - | 15223 | ` * A require/include normally runs at execution time, when no compile is in flight,` |
|         - | 15224 | ` * so VmEvalChunk can call PH7_ResetCodeGenerator to wipe the generator. But an` |
|         - | 15225 | `` * autoload can fire in the MIDDLE of compiling a class: resolving `class Child`` |
|         - | 15226 | `` * extends Base` calls PH7_VmExtractClass, which triggers the autoloader, whose`` |
|         - | 15227 | `` * body `require`s Base's file — a nested compile while the outer one is mid-token.`` |
|         - | 15228 | ` * PH7_ResetCodeGenerator would then blow away the outer compile's cursor` |
|         - | 15229 | ` * (pIn/pEnd), current block, use-imports, labels, etc.; the outer parse resumes` |
|         - | 15230 | ` * with pIn == NULL and reports a bogus "Expected '{' after class 'Child'". (Common` |
|         - | 15231 | ` * in every real framework: Composer autoloads parent classes on demand.)` |
|         - | 15232 | ` *` |
|         - | 15233 | ` * This snapshots the position/scope fields into *pSaved (a caller-stack` |
|         - | 15234 | ` * ph7_gen_state used purely as storage) and re-initializes the live generator's` |
|         - | 15235 | ` * position containers to FRESH, empty ones WITHOUT releasing the outer's (the` |
|         - | 15236 | ` * snapshot now owns those). hLiteral/hNumLiteral/hVar are intentionally left LIVE` |
|         - | 15237 | ` * and shared — compiled bytecode interns names/literals into them and they grow` |
|         - | 15238 | ` * monotonically (exactly why PH7_ResetCodeGenerator keeps them); restoring an old` |
|         - | 15239 | ` * copy of those headers after a nested grow would use-after-free the bucket array.` |
|         - | 15240 | ` */` |
|         4 | 15241 | `PH7_PRIVATE void PH7_CompilerSaveState(ph7_vm *pVm,ph7_gen_state *pSaved,ProcConsumer xErr,void *pErrData)` |
|         1 | 15242 | `{` |
|         5 | 15243 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 15244 | `	/* Shallow-copy every field; the position containers below are then replaced` |
|         - | 15245 | `	 * with fresh ones on the live struct, so *pSaved keeps the outer's memory. */` |
|         5 | 15246 | `	*pSaved = *pGen;` |
|         5 | 15247 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|         5 | 15248 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|         5 | 15249 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|         5 | 15250 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|         5 | 15251 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|         5 | 15252 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|         5 | 15253 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         5 | 15254 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         5 | 15255 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|         5 | 15256 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|         5 | 15257 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|         5 | 15258 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 15259 | `	/* Fresh global scope for the nested unit (address of the embedded sGlobal is` |
|         - | 15260 | `	 * stable, so any outer block still parented to it stays valid across restore). */` |
|         5 | 15261 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(pVm),0);` |
|         5 | 15262 | `	pGen->pCurrent = &pGen->sGlobal;` |
|         5 | 15263 | `	pGen->pIn = pGen->pEnd = 0;` |
|         5 | 15264 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|         5 | 15265 | `	pGen->pTokenSet = 0;` |
|         5 | 15266 | `	pGen->nErr = 0;` |
|         5 | 15267 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|         5 | 15268 | `	pGen->nCommaExprOk = 0;` |
|         5 | 15269 | `	pGen->bInGenerator = 0;` |
|         5 | 15270 | `	pGen->bStrictTypes = 0;` |
|         5 | 15271 | `	pGen->bStrictTypesLocked = 0;` |
|         5 | 15272 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|         5 | 15273 | `	pGen->xErr = xErr;` |
|         5 | 15274 | `	pGen->pErrData = pErrData;` |
|         5 | 15275 | `}` |
|         - | 15276 | `/*` |
|         - | 15277 | ` * Restore the outer compile-position state saved by PH7_CompilerSaveState,` |
|         - | 15278 | ` * releasing the nested unit's position containers first. The shared` |
|         - | 15279 | ` * hLiteral/hNumLiteral/hVar tables (grown by the nested compile) are carried` |
|         - | 15280 | ` * forward, NOT rolled back to the snapshot's stale headers.` |
|         - | 15281 | ` */` |
|         4 | 15282 | `PH7_PRIVATE void PH7_CompilerRestoreState(ph7_vm *pVm,ph7_gen_state *pSaved)` |
|         1 | 15283 | `{` |
|         5 | 15284 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 15285 | `	GenBlock *pBlock,*pParent;` |
|         - | 15286 | `	SyHash hVar,hLiteral,hNumLiteral;` |
|         - | 15287 | `	/* Free any nested blocks left open (e.g. an aborted nested compile), then the` |
|         - | 15288 | `	 * nested global block's own fixup sets. */` |
|         5 | 15289 | `	pBlock = pGen->pCurrent;` |
|         5 | 15290 | `	while( pBlock && pBlock->pParent != 0 ){` |
|       ! 0 | 15291 | `		pParent = pBlock->pParent;` |
|       ! 0 | 15292 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 15293 | `		pBlock = pParent;` |
|       ! 0 | 15294 | `	}` |
|         5 | 15295 | `	GenStateReleaseBlock(&pGen->sGlobal);` |
|         - | 15296 | `	/* Release the nested unit's position containers. */` |
|         5 | 15297 | `	SySetRelease(&pGen->aLabel);` |
|         5 | 15298 | `	SySetRelease(&pGen->aGoto);` |
|         5 | 15299 | `	SySetRelease(&pGen->aNullsafeJmp);` |
|         5 | 15300 | `	SySetRelease(&pGen->aLoopParent);` |
|         5 | 15301 | `	SySetRelease(&pGen->aTrivia);` |
|         5 | 15302 | `	SySetRelease(&pGen->aPendingAttrs);` |
|         5 | 15303 | `	SyBlobRelease(&pGen->sWorker);` |
|         5 | 15304 | `	SyBlobRelease(&pGen->sErrBuf);` |
|         5 | 15305 | `	SyBlobRelease(&pGen->sNamespace);` |
|         5 | 15306 | `	SyHashRelease(&pGen->hUseImports);` |
|         5 | 15307 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|         5 | 15308 | `	SyHashRelease(&pGen->hUseConstImports);` |
|         - | 15309 | `	/* Preserve the (possibly grown) shared intern tables across the restore. */` |
|         5 | 15310 | `	hVar = pGen->hVar;` |
|         5 | 15311 | `	hLiteral = pGen->hLiteral;` |
|         5 | 15312 | `	hNumLiteral = pGen->hNumLiteral;` |
|         5 | 15313 | `	*pGen = *pSaved;` |
|         5 | 15314 | `	pGen->hVar = hVar;` |
|         5 | 15315 | `	pGen->hLiteral = hLiteral;` |
|         5 | 15316 | `	pGen->hNumLiteral = hNumLiteral;` |
|         5 | 15317 | `}` |
|         - | 15318 | `/*` |
|         - | 15319 | ` * Raise php's parse error for an unexpected token: E_PARSE with the exact text` |
|         - | 15320 | ` * php's parser prints, e.g.` |
|         - | 15321 | ` *` |
|         - | 15322 | ` *   syntax error, unexpected token ";", expecting "{"` |
|         - | 15323 | ` *   syntax error, unexpected identifier "invalid", expecting "("` |
|         - | 15324 | ` *   syntax error, unexpected end of file` |
|         - | 15325 | ` *` |
|         - | 15326 | ` * php names the token by CLASS, not just by text: an identifier, a variable and` |
|         - | 15327 | ` * a number each get their own noun, while everything else (keywords, operators,` |
|         - | 15328 | ` * punctuation) is a "token". zExpecting is the optional ", expecting ..." tail` |
|         - | 15329 | ` * — pass NULL when the site cannot say what it wanted (php often can't either).` |
|         - | 15330 | ` * pTok == NULL means the input ran out: "unexpected end of file".` |
|         - | 15331 | ` *` |
|         - | 15332 | ` * PHL's hand-written recursive-descent parser has no bison expectation sets, so` |
|         - | 15333 | ` * a site can only claim an "expecting" clause it genuinely knows; every clause` |
|         - | 15334 | ` * emitted here was verified against php 8.5.7 for the construct in question.` |
|         - | 15335 | ` */` |
|       182 | 15336 | `PH7_PRIVATE sxi32 PH7_GenSyntaxError(` |
|         - | 15337 | `	ph7_gen_state *pGen,   /* Code generator state */` |
|         - | 15338 | `	SyToken *pTok,         /* Offending token, or NULL for end of file */` |
|         - | 15339 | `	const char *zExpecting /* ", expecting <this>" tail, or NULL */` |
|         - | 15340 | `	)` |
|         5 | 15341 | `{` |
|       187 | 15342 | `	const char *zNoun = "token";` |
|         - | 15343 | `	sxu32 nLine;` |
|       187 | 15344 | `	if( pTok == 0 && pGen->pTokenSet ){` |
|         - | 15345 | `		/* The caller ran out of tokens inside its own slice — but a statement's slice stops` |
|         - | 15346 | `		 * BEFORE its terminator, so the token php actually names (typically the ';') is the` |
|         - | 15347 | `		 * one sitting just past the slice, still inside the chunk's token stream. Reach for` |
|         - | 15348 | `		 * it before concluding "end of file". */` |
|        92 | 15349 | `		SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        92 | 15350 | `		SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        92 | 15351 | `		if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|        92 | 15352 | `			pTok = pGen->pEnd;` |
|        44 | 15353 | `		}` |
|        44 | 15354 | `	}` |
|       187 | 15355 | `	nLine = pTok ? pTok->nLine : (pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ? pGen->pIn[-1].nLine : 1);` |
|       187 | 15356 | `	if( pTok == 0 ){` |
|       ! 0 | 15357 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       ! 0 | 15358 | `			zExpecting ? "syntax error, unexpected end of file, expecting %s"` |
|         - | 15359 | `			           : "syntax error, unexpected end of file",` |
|       ! 0 | 15360 | `			zExpecting);` |
|         - | 15361 | `	}` |
|       187 | 15362 | `	if( pTok->nType & PH7_TK_ID ){` |
|        16 | 15363 | `		zNoun = "identifier";` |
|       180 | 15364 | `	}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|         9 | 15365 | `		zNoun = "variable";` |
|       171 | 15366 | `	}else if( pTok->nType & PH7_TK_INTEGER ){` |
|        24 | 15367 | `		zNoun = "integer";` |
|       158 | 15368 | `	}else if( pTok->nType & PH7_TK_REAL ){` |
|       ! 0 | 15369 | `		zNoun = "float";` |
|       ! 0 | 15370 | `	}` |
|       187 | 15371 | `	if( zExpecting ){` |
|       118 | 15372 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        38 | 15373 | `			"syntax error, unexpected %s \"%z\", expecting %s",zNoun,&pTok->sData,zExpecting);` |
|         - | 15374 | `	}` |
|       164 | 15375 | `	return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        53 | 15376 | `		"syntax error, unexpected %s \"%z\"",zNoun,&pTok->sData);` |
|        96 | 15377 | `}` |
|         - | 15378 | `/*` |
|         - | 15379 | ` * Generate a compile-time error message.` |
|         - | 15380 | ` * If the error count limit is reached (usually 15 error message)` |
|         - | 15381 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|         - | 15382 | ` * abort compilation immediately.` |
|         - | 15383 | ` */` |
|     16228 | 15384 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|         5 | 15385 | `{` |
|     16233 | 15386 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     16233 | 15387 | `	const char *zErr = "Error";` |
|         - | 15388 | `	SyString *pFile;` |
|         - | 15389 | `	va_list ap;` |
|         - | 15390 | `	sxi32 rc;` |
|         - | 15391 | `	/* Reset the working buffer */` |
|     16233 | 15392 | `	SyBlobReset(pWorker);` |
|         - | 15393 | `	/* Peek the processed file path if available */` |
|     16233 | 15394 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     16233 | 15395 | `	if( nErrType == E_ERROR \|\| nErrType == E_PARSE ){` |
|         - | 15396 | `		/* Increment the error counter. A PARSE error is every bit as fatal as an` |
|         - | 15397 | `		 * E_ERROR one: php compiles nothing, runs nothing and exits 255. Counting` |
|         - | 15398 | `		 * only E_ERROR let a parse error print its diagnostic and then fall through` |
|         - | 15399 | `		 * into execution with a 0 exit status. */` |
|       659 | 15400 | `		pGen->nErr++;` |
|       659 | 15401 | `		if( pGen->nErr > 15 ){` |
|         - | 15402 | `			/* Error count limit reached */` |
|         6 | 15403 | `			if( pGen->xErr ){` |
|         6 | 15404 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|         6 | 15405 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|         6 | 15406 | `				if( pFile ){` |
|         6 | 15407 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|         2 | 15408 | `				}` |
|         6 | 15409 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|         6 | 15410 | `				if( SyBlobLength(pWorker) > 0 ){` |
|         6 | 15411 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|         2 | 15412 | `				}` |
|         2 | 15413 | `			}` |
|         - | 15414 | `			/* Abort immediately */` |
|         6 | 15415 | `			return SXERR_ABORT;` |
|         - | 15416 | `		}` |
|       325 | 15417 | `	}` |
|     16229 | 15418 | `	if( pGen->xErr == 0 ){` |
|         - | 15419 | `		/* No available error consumer,return immediately */` |
|     15559 | 15420 | `		return SXRET_OK;` |
|         - | 15421 | `	}` |
|       675 | 15422 | `	switch(nErrType){` |
|       310 | 15423 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|         8 | 15424 | `	case E_WARNING: zErr = "Warning";     break;` |
|       346 | 15425 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|         6 | 15426 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|       ! 0 | 15427 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|       ! 0 | 15428 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|       ! 0 | 15429 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|        16 | 15430 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|       ! 0 | 15431 | `	default:` |
|       ! 0 | 15432 | `		break;` |
|         - | 15433 | `	}` |
|       675 | 15434 | `	rc = SXRET_OK;` |
|         - | 15435 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|       675 | 15436 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|       675 | 15437 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|       675 | 15438 | `	va_start(ap,zFormat);` |
|       675 | 15439 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|       675 | 15440 | `	va_end(ap);` |
|       675 | 15441 | `	if( pFile ){` |
|       675 | 15442 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       335 | 15443 | `	}` |
|         - | 15444 | `	/* Append a new line */` |
|       675 | 15445 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       675 | 15446 | `	if( SyBlobLength(pWorker) > 0 ){` |
|         - | 15447 | `		/* Consume the generated error message */` |
|       675 | 15448 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       335 | 15449 | `	}` |
|       675 | 15450 | `	return rc;` |
|      8119 | 15451 | `}` |
|         - | 15452 |  |
