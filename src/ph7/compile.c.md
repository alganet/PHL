# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 7025/8699 lines (80.76%)

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
|         - |    42 | `	sxu8 bRef;           /* True if the label was referenced */` |
|         - |    43 | `};` |
|         - |    44 | `/*` |
|         - |    45 | ` * Compilation of some PHP constructs such as if, for, while, the logical or` |
|         - |    46 | ` * (\|\|) and logical and (&&) operators in expressions requires the` |
|         - |    47 | ` * generation of forward jumps.` |
|         - |    48 | ` * Since the destination PC target of these jumps isn't known when the jumps` |
|         - |    49 | ` * are emitted, we record each forward jump in an instance of the following` |
|         - |    50 | ` * structure. Those jumps are fixed later when the jump destination is resolved.` |
|         - |    51 | ` */` |
|         - |    52 | `struct JumpFixup` |
|         - |    53 | `{` |
|         - |    54 | `	sxi32 nJumpType;     /* Jump type. Either TRUE jump, FALSE jump or Unconditional jump */` |
|         - |    55 | `	sxu32 nInstrIdx;     /* Instruction index to fix later when the jump destination is resolved. */` |
|         - |    56 | `	/* The following fields are only used by the goto statement */` |
|         - |    57 | `	SyString sLabel;    /* Label name */` |
|         - |    58 | `	ph7_vm_func *pFunc; /* Compiled function inside which the goto was emitted. NULL otherwise */` |
|         - |    59 | `	sxu32 nLine;        /* Track line number */` |
|         - |    60 | `};` |
|         - |    61 | `/*` |
|         - |    62 | ` * Each language construct is represented by an instance` |
|         - |    63 | ` * of the following structure.` |
|         - |    64 | ` */` |
|         - |    65 | `struct LangConstruct` |
|         - |    66 | `{` |
|         - |    67 | `	sxu32 nID;                     /* Language construct ID [i.e: PH7_TKWRD_WHILE,PH7_TKWRD_FOR,PH7_TKWRD_IF...] */` |
|         - |    68 | `	ProcLangConstruct xConstruct;  /* C function implementing the language construct */` |
|         - |    69 | `};` |
|         - |    70 | `/* Compilation flags */` |
|         - |    71 | `#define PH7_COMPILE_SINGLE_STMT 0x001 /* Compile a single statement */` |
|         - |    72 | `/* Token stream synchronization macros */` |
|         - |    73 | `#define SWAP_TOKEN_STREAM(GEN,START,END)\` |
|         - |    74 | `	pTmp  = GEN->pEnd;\` |
|         - |    75 | `	pGen->pIn  = START;\` |
|         - |    76 | `	pGen->pEnd = END` |
|         - |    77 | `#define UPDATE_TOKEN_STREAM(GEN)\` |
|         - |    78 | `	if( GEN->pIn < pTmp ){\` |
|         - |    79 | `	    GEN->pIn++;\` |
|         - |    80 | `	}\` |
|         - |    81 | `	GEN->pEnd = pTmp` |
|         - |    82 | `#define SWAP_DELIMITER(GEN,START,END)\` |
|         - |    83 | `	pTmpIn  = GEN->pIn;\` |
|         - |    84 | `	pTmpEnd = GEN->pEnd;\` |
|         - |    85 | `	GEN->pIn = START;\` |
|         - |    86 | `	GEN->pEnd = END` |
|         - |    87 | `#define RE_SWAP_DELIMITER(GEN)\` |
|         - |    88 | `	GEN->pIn  = pTmpIn;\` |
|         - |    89 | `	GEN->pEnd = pTmpEnd` |
|         - |    90 | `/* Flags related to expression compilation */` |
|         - |    91 | `#define EXPR_FLAG_LOAD_IDX_STORE    0x001 /* Set the iP2 flag when dealing with the LOAD_IDX instruction */` |
|         - |    92 | `#define EXPR_FLAG_RDONLY_LOAD       0x002 /* Read-only load, refer to the 'PH7_OP_LOAD' VM instruction for more information */` |
|         - |    93 | `#define EXPR_FLAG_COMMA_STATEMENT   0x004 /* Treat comma expression as a single statement (used by class attributes) */` |
|         - |    94 | `#define EXPR_FLAG_LOAD_IDX_ISSET    0x008 /* LOAD_IDX argument is the LHS of isset() — emit iP2=4 (offsetExists) */` |
|         - |    95 | `#define EXPR_FLAG_LOAD_IDX_UNSET    0x010 /* LOAD_IDX argument is the LHS of unset() — emit iP2=5 (offsetUnset) */` |
|         - |    96 | `#define EXPR_FLAG_LOAD_IDX_EMPTY    0x020 /* LOAD_IDX argument is the LHS of empty() — emit iP2=6 (offsetExists+offsetGet) */` |
|         - |    97 | `#define EXPR_FLAG_MEMBER_WRITE      0x040 /* Sub-tree is the write lvalue of an assignment: tag a target` |
|         - |    98 | `                                           * OP_MEMBER iP2=PH7_MEMBER_WRITE so the VM auto-creates a missing` |
|         - |    99 | ``                                           * property (e.g. `$o->arr[$k] = v`, `$o->p ??= v`). Propagated`` |
|         - |   100 | `                                           * from the precedence-18 lvalue through SUBSCRIPT to the base` |
|         - |   101 | ``                                            * member; stripped when descending into an intermediate `->` `` |
|         - |   102 | `                                           * container (the container is read, not the write target). */` |
|         - |   103 | `/* Forward declaration */` |
|         - |   104 | `static sxi32 PH7_CompileExpr(ph7_gen_state *pGen,sxi32 iFlags,sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *));` |
|         - |   105 | `/*` |
|         - |   106 | ` * Local utility routines used in the code generation phase.` |
|         - |   107 | ` */` |
|         - |   108 | `/*` |
|         - |   109 | ` * Check if the given name refer to a valid label.` |
|         - |   110 | ` * Return SXRET_OK and write a pointer to that label on success.` |
|         - |   111 | ` * Any other return value indicates no such label.` |
|         - |   112 | ` */` |
|       148 |   113 | `static sxi32 GenStateGetLabel(ph7_gen_state *pGen,SyString *pName,Label **ppOut)` |
|         5 |   114 | `{` |
|         - |   115 | `	Label *aLabel;` |
|         - |   116 | `	sxu32 n;` |
|         - |   117 | `	/* Perform a linear scan on the label table */` |
|       153 |   118 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|       333 |   119 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|       276 |   120 | `		if( SyStringCmp(&aLabel[n].sName,pName,SyMemcmp) == 0 ){` |
|         - |   121 | `			/* Jump destination found */` |
|        96 |   122 | `			aLabel[n].bRef = TRUE;` |
|        96 |   123 | `			if( ppOut ){` |
|        96 |   124 | `				*ppOut = &aLabel[n];` |
|        46 |   125 | `			}` |
|        96 |   126 | `			return SXRET_OK;` |
|         - |   127 | `		}` |
|        93 |   128 | `	}` |
|         - |   129 | `	/* No such destination */` |
|        60 |   130 | `	return SXERR_NOTFOUND;` |
|        79 |   131 | `}` |
|         - |   132 | `/*` |
|         - |   133 | ` * Fetch a block that correspond to the given criteria from the stack of` |
|         - |   134 | ` * compiled blocks.` |
|         - |   135 | ` * Return a pointer to that block on success. NULL otherwise.` |
|         - |   136 | ` */` |
|    118432 |   137 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|         5 |   138 | `{` |
|    118437 |   139 | `	GenBlock *pBlock = pCurrent;` |
|    275937 |   140 | `	for(;;){` |
|    551879 |   141 | `		if( pBlock->iFlags & iBlockType ){` |
|    118329 |   142 | `			iCount--; /* Decrement nesting level */` |
|    118329 |   143 | `			if( iCount < 1 ){` |
|         - |   144 | `				/* Block meet with the desired criteria */` |
|    118303 |   145 | `				return pBlock;` |
|         - |   146 | `			}` |
|        13 |   147 | `		}` |
|         - |   148 | `		/* Point to the upper block */` |
|    433581 |   149 | `		pBlock = pBlock->pParent;` |
|    433581 |   150 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|         - |   151 | `			/* Forbidden */` |
|        72 |   152 | `			break;` |
|         - |   153 | `		}` |
|         5 |   154 | `	}` |
|         - |   155 | `	/* No such block */` |
|       139 |   156 | `	return 0;` |
|     59221 |   157 | `}` |
|         - |   158 | `/*` |
|         - |   159 | ` * Initialize a freshly allocated block instance.` |
|         - |   160 | ` */` |
|  10342474 |   161 | `static void GenStateInitBlock(` |
|         - |   162 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |   163 | `	GenBlock *pBlock,    /* Target block */` |
|         - |   164 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   165 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|         - |   166 | `	void *pUserData      /* Upper layer private data */` |
|         - |   167 | `	)` |
|         5 |   168 | `{` |
|         - |   169 | `	/* Initialize block fields */` |
|  10342479 |   170 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  10342479 |   171 | `	pBlock->pUserData   = pUserData;` |
|  10342479 |   172 | `	pBlock->pGen        = pGen;` |
|  10342479 |   173 | `	pBlock->iFlags      = iType;` |
|  10342479 |   174 | `	pBlock->pParent     = 0;` |
|  10342479 |   175 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  10342479 |   176 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  10342479 |   177 | `}` |
|         - |   178 | `/*` |
|         - |   179 | ` * Allocate a new block instance.` |
|         - |   180 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|         - |   181 | ` * on success.Otherwise generate a compile-time error and abort` |
|         - |   182 | ` * processing on failure.` |
|         - |   183 | ` */` |
|  10338536 |   184 | `static sxi32 GenStateEnterBlock(` |
|         - |   185 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |   186 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   187 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|         - |   188 | `	void *pUserData,      /* Upper layer private data */` |
|         - |   189 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|         - |   190 | `	)` |
|         5 |   191 | `{` |
|         - |   192 | `	GenBlock *pBlock;` |
|         - |   193 | `	/* Allocate a new block instance */` |
|  10338541 |   194 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  10338541 |   195 | `	if( pBlock == 0 ){` |
|         - |   196 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |   197 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |   198 | `		 */` |
|       ! 0 |   199 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|         - |   200 | `		/* Abort processing immediately */` |
|       ! 0 |   201 | `		return SXERR_ABORT;` |
|         - |   202 | `	}` |
|         - |   203 | `	/* Zero the structure */` |
|  10338541 |   204 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  10338541 |   205 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|         - |   206 | `	/* Link to the parent block */` |
|  10338541 |   207 | `	pBlock->pParent = pGen->pCurrent;` |
|         - |   208 | `	/* Mark as the current block */` |
|  10338541 |   209 | `	pGen->pCurrent = pBlock;` |
|  10338541 |   210 | `	if( ppBlock ){` |
|         - |   211 | `		/* Write a pointer to the new instance */` |
|   4976973 |   212 | `		*ppBlock = pBlock;` |
|   2488484 |   213 | `	}` |
|  10338541 |   214 | `	return SXRET_OK;` |
|   5169273 |   215 | `}` |
|         - |   216 | `/*` |
|         - |   217 | ` * Release block fields without freeing the whole instance.` |
|         - |   218 | ` */` |
|  10338520 |   219 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|         5 |   220 | `{` |
|  10338525 |   221 | `	SySetRelease(&pBlock->aPostContFix);` |
|  10338525 |   222 | `	SySetRelease(&pBlock->aJumpFix);` |
|  10338525 |   223 | `}` |
|         - |   224 | `/*` |
|         - |   225 | ` * Release a block.` |
|         - |   226 | ` */` |
|  10338520 |   227 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|         5 |   228 | `{` |
|  10338525 |   229 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  10338525 |   230 | `	GenStateReleaseBlock(&(*pBlock));` |
|         - |   231 | `	/* Free the instance */` |
|  10338525 |   232 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  10338525 |   233 | `}` |
|         - |   234 | `/*` |
|         - |   235 | ` * POP and release a block from the stack of compiled blocks.` |
|         - |   236 | ` */` |
|  10338520 |   237 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|         5 |   238 | `{` |
|  10338525 |   239 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  10338525 |   240 | `	if( pBlock == 0 ){` |
|         - |   241 | `		/* No more block to pop */` |
|       ! 0 |   242 | `		return SXERR_EMPTY;` |
|         - |   243 | `	}` |
|         - |   244 | `	/* Point to the upper block */` |
|  10338525 |   245 | `	pGen->pCurrent = pBlock->pParent;` |
|  10338525 |   246 | `	if( ppBlock ){` |
|         - |   247 | `		/* Write a pointer to the popped block */` |
|       ! 0 |   248 | `		*ppBlock = pBlock;` |
|       ! 0 |   249 | `	}else{` |
|         - |   250 | `		/* Safely release the block */` |
|  10338525 |   251 | `		GenStateFreeBlock(&(*pBlock));` |
|         - |   252 | `	}` |
|  10338525 |   253 | `	return SXRET_OK;` |
|   5169265 |   254 | `}` |
|         - |   255 | `/*` |
|         - |   256 | ` * Emit a forward jump.` |
|         - |   257 | ` * Notes on forward jumps` |
|         - |   258 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|         - |   259 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|         - |   260 | ` *  generation of forward jumps.` |
|         - |   261 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|         - |   262 | ` *  are emitted, we record each forward jump in an instance of the following` |
|         - |   263 | ` *  structure. Those jumps are fixed later when the jump destination is resolved.` |
|         - |   264 | ` */` |
|   3696958 |   265 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|         5 |   266 | `{` |
|         - |   267 | `	JumpFixup sJumpFix;` |
|         - |   268 | `	sxi32 rc;` |
|         - |   269 | `	/* Init the JumpFixup structure */` |
|   3696963 |   270 | `	sJumpFix.nJumpType = nJumpType;` |
|   3696963 |   271 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|         - |   272 | `	/* Insert in the jump fixup table */` |
|   3696963 |   273 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|   3696963 |   274 | `	return rc;` |
|         5 |   275 | `}` |
|         - |   276 | `/*` |
|         - |   277 | ` * Fix a forward jump now the jump destination is resolved.` |
|         - |   278 | ` * Return the total number of fixed jumps.` |
|         - |   279 | ` * Notes on forward jumps:` |
|         - |   280 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|         - |   281 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|         - |   282 | ` *  generation of forward jumps.` |
|         - |   283 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|         - |   284 | ` *  are emitted, we record each forward jump in an instance of the following` |
|         - |   285 | ` *  structure.Those jumps are fixed later when the jump destination is resolved.` |
|         - |   286 | ` */` |
|   7191754 |   287 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|         5 |   288 | `{` |
|         - |   289 | `	JumpFixup *aFix;` |
|         - |   290 | `	VmInstr *pInstr;` |
|         - |   291 | `	sxu32 nFixed;` |
|         - |   292 | `	sxu32 n;` |
|         - |   293 | `	/* Point to the jump fixup table */` |
|   7191759 |   294 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|         - |   295 | `	/* Fix the desired jumps */` |
|  15186319 |   296 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|   7994565 |   297 | `		if( aFix[n].nJumpType < 0 ){` |
|         - |   298 | `			/* Already fixed */` |
|   3032657 |   299 | `			continue;` |
|         - |   300 | `		}` |
|   4961913 |   301 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|         - |   302 | `			/* Not of our interest */` |
|   1264957 |   303 | `			continue;` |
|         - |   304 | `		}` |
|         - |   305 | `		/* Point to the instruction to fix */` |
|   3696961 |   306 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|   3696961 |   307 | `		if( pInstr ){` |
|   3696961 |   308 | `			pInstr->iP2 = nJumpDest;` |
|   3696961 |   309 | `			nFixed++;` |
|         - |   310 | `			/* Mark as fixed */` |
|   3696961 |   311 | `			aFix[n].nJumpType = -1;` |
|   1848478 |   312 | `		}` |
|   1848483 |   313 | `	}` |
|         - |   314 | `	/* Total number of fixed jumps */` |
|   7191759 |   315 | `	return nFixed;` |
|         5 |   316 | `}` |
|         - |   317 | `/*` |
|         - |   318 | ` * Fix a 'goto' now the jump destination is resolved.` |
|         - |   319 | ` * The goto statement can be used to jump to another section` |
|         - |   320 | ` * in the program.` |
|         - |   321 | ` * Refer to the routine responsible of compiling the goto` |
|         - |   322 | ` * statement for more information.` |
|         - |   323 | ` */` |
|   2696058 |   324 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|         5 |   325 | `{` |
|         - |   326 | `	JumpFixup *pJump,*aJumps;` |
|         - |   327 | `	Label *pLabel,*aLabel;` |
|         - |   328 | `	VmInstr *pInstr;` |
|         - |   329 | `	sxi32 rc;` |
|         - |   330 | `	sxu32 n;` |
|         - |   331 | `	/* Point to the goto table */` |
|   2696063 |   332 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|         - |   333 | `	/* Fix */` |
|   2696209 |   334 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
|       153 |   335 | `		pJump = &aJumps[n];` |
|         - |   336 | `		/* Extract the target label */` |
|       153 |   337 | `		rc = GenStateGetLabel(&(*pGen),&pJump->sLabel,&pLabel);` |
|       153 |   338 | `		if( rc != SXRET_OK ){` |
|         - |   339 | `			/* No such label */` |
|        60 |   340 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' was referenced but not defined",&pJump->sLabel);` |
|        60 |   341 | `			if( rc == SXERR_ABORT ){` |
|         3 |   342 | `				return SXERR_ABORT;` |
|         - |   343 | `			}` |
|        58 |   344 | `			continue;` |
|         - |   345 | `		}` |
|         - |   346 | `		/* Make sure the target label is reachable */` |
|        96 |   347 | `		if( pLabel->pFunc != pJump->pFunc ){` |
|        11 |   348 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' is unreachable",&pJump->sLabel);` |
|        11 |   349 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |   350 | `				return SXERR_ABORT;` |
|         - |   351 | `			}` |
|         4 |   352 | `		}` |
|         - |   353 | `		/* Fix the jump now the destination is resolved */` |
|        96 |   354 | `		pInstr = PH7_VmGetInstr(pGen->pVm,pJump->nInstrIdx);` |
|        96 |   355 | `		if( pInstr ){` |
|        96 |   356 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|        46 |   357 | `		}` |
|        50 |   358 | `	}` |
|   2696061 |   359 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|   2696193 |   360 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|       137 |   361 | `		if( aLabel[n].bRef == FALSE ){` |
|         - |   362 | `			/* Emit a warning */` |
|        40 |   363 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|        24 |   364 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|        12 |   365 | `		}` |
|        71 |   366 | `	}` |
|   2696061 |   367 | `	return SXRET_OK;` |
|   1348034 |   368 | `}` |
|         - |   369 | `/*` |
|         - |   370 | ` * Check if a given token value is installed in the literal table.` |
|         - |   371 | ` */` |
|  13606582 |   372 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|         5 |   373 | `{` |
|         - |   374 | `	SyHashEntry *pEntry;` |
|  13606587 |   375 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  13606587 |   376 | `	if( pEntry == 0 ){` |
|   3562885 |   377 | `		return SXERR_NOTFOUND;` |
|         - |   378 | `	}` |
|  10043707 |   379 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  10043707 |   380 | `	return SXRET_OK;` |
|   6803296 |   381 | `}` |
|         - |   382 | `/*` |
|         - |   383 | ` * Install a given constant index in the literal table.` |
|         - |   384 | ` * In order to be installed, the ph7_value must be of type string.` |
|         - |   385 | ` *` |
|         - |   386 | ` * NOTE: empty strings are deliberately omitted here.  The VM reserves a` |
|         - |   387 | ` * single shared constant for "" during initialization (pVm->nEmptyStringIdx)` |
|         - |   388 | ` * and the compiler emits a LOADC referencing that slot whenever an empty` |
|         - |   389 | ` * literal is encountered.  This keeps the literal hash from growing when` |
|         - |   390 | ` * many "" literals appear in user code.` |
|         - |   391 | ` */` |
|   3562880 |   392 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|         5 |   393 | `{` |
|   3562885 |   394 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|   3562885 |   395 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   1781440 |   396 | `	}` |
|   3562885 |   397 | `	return SXRET_OK;` |
|         5 |   398 | `}` |
|         - |   399 | `/*` |
|         - |   400 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|         - |   401 | ` * in the constant table.` |
|         - |   402 | ` */` |
|   2795178 |   403 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|         5 |   404 | `{` |
|         - |   405 | `	ph7_value *pObj;` |
|   2795183 |   406 | `	sxu32 nIdx = 0; /* cc warning */` |
|         - |   407 | `	/* Reserve a new constant */` |
|   2795183 |   408 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   2795183 |   409 | `	if( pObj == 0 ){` |
|       ! 0 |   410 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   411 | `		return 0;` |
|         - |   412 | `	}` |
|   2795183 |   413 | `	*pIdx = nIdx;` |
|         - |   414 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|         - |   415 | `	 * the constant string iterals table [optimization purposes].` |
|         - |   416 | `	 */` |
|   2795183 |   417 | `	return pObj;` |
|   1397594 |   418 | `}` |
|         - |   419 | `/*` |
|         - |   420 | ` * Implementation of the PHP language constructs.` |
|         - |   421 | ` */` |
|         - |   422 | `/*` |
|         - |   423 | ` * Ensure the about-to-be-emitted CALL/NEW opcode carries a VmCallArgMap` |
|         - |   424 | ` * that reflects the caller file's strict_types mode. Returns the (possibly` |
|         - |   425 | ` * newly allocated and zero-initialized) map pointer. In weak-mode files` |
|         - |   426 | ` * this is a no-op and the caller's p3 is returned unchanged.` |
|         - |   427 | ` *` |
|         - |   428 | ` * NOTE: on allocation failure the call reverts to weak semantics rather` |
|         - |   429 | ` * than aborting compilation — out-of-memory during a map allocation is` |
|         - |   430 | ` * vanishingly unlikely and silently dropping to weak mode matches the` |
|         - |   431 | ` * surrounding callsites' zero-check fallback pattern.` |
|         - |   432 | ` */` |
|   6356240 |   433 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|         5 |   434 | `{` |
|         - |   435 | `	VmCallArgMap *pMap;` |
|   6356245 |   436 | `	if( !pGen->bStrictTypes ) return p3;` |
|        39 |   437 | `	if( p3 == 0 ){` |
|        35 |   438 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|        35 |   439 | `		if( pMap == 0 ) return 0;` |
|        35 |   440 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|        35 |   441 | `		p3 = (void *)pMap;` |
|        16 |   442 | `	}` |
|        39 |   443 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|        39 |   444 | `	return p3;` |
|   3178125 |   445 | `}` |
|         - |   446 | `/* Forward declaration */` |
|         - |   447 | `static sxi32 GenStateCompileChunk(ph7_gen_state *pGen,sxi32 iFlags);` |
|         - |   448 | `static void GenStateSetPendingDoc(ph7_gen_state *pGen);` |
|         - |   449 | `static void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut);` |
|         - |   450 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut);` |
|         - |   451 | `static sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut);` |
|         - |   452 | `static sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut);` |
|         - |   453 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx);` |
|         - |   454 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn);` |
|         - |   455 | `/* Forward decl: union type parser is defined later in this file. */` |
|         - |   456 | `static sxi32 GenStateParseUnionTypeDecl(` |
|         - |   457 | `	ph7_gen_state *pGen,` |
|         - |   458 | `	sxu32 *pnType,` |
|         - |   459 | `	SyString *pClass,` |
|         - |   460 | `	SySet *pAlts,` |
|         - |   461 | `	sxi32 *piTypeFlags,` |
|         - |   462 | `	SyString *pTypeText,` |
|         - |   463 | `	int iNullableFlag,` |
|         - |   464 | `	int iUnionFlag,` |
|         - |   465 | `	int bAllowVoid,` |
|         - |   466 | `	sxu32 nLine` |
|         - |   467 | `);` |
|         - |   468 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc);` |
|         - |   469 | `static const char * TokenTypeName(sxu32 nType);` |
|         - |   470 | `/*` |
|         - |   471 | ` * Stack-scratch size for stripping PHP 7.4 numeric separators. A typical` |
|         - |   472 | ` * literal (INT64_MAX decimal is 19 digits, binary 64-bit with per-nibble` |
|         - |   473 | ` * separators is ~80 chars) fits comfortably, so the fast path never touches` |
|         - |   474 | ` * the heap. The language itself imposes no upper bound on the length of a` |
|         - |   475 | ` * well-formed literal — the stripper falls back to a VM-allocator buffer` |
|         - |   476 | ` * for anything larger, so correctness is preserved even for pathological` |
|         - |   477 | ` * inputs like a thousand-digit number.` |
|         - |   478 | ` */` |
|         - |   479 | `#define GEN_NUM_SCRATCH 128` |
|         - |   480 | `/*` |
|         - |   481 | ` * Return TRUE if c is a valid digit for the given numeric base.` |
|         - |   482 | ` *   base 16 => SyisHex (0-9, a-f, A-F)` |
|         - |   483 | ` *   base  2 => 0 or 1` |
|         - |   484 | ` *   base 10 => SyisDigit (0-9, also used for octal literals which share the` |
|         - |   485 | ` *              decimal scan in the lexer)` |
|         - |   486 | ` */` |
|      1076 |   487 | `static int GenStateIsBaseDigit(int c, int base)` |
|         5 |   488 | `{` |
|      1081 |   489 | `	if( base == 16 ){ return SyisHex(c); }` |
|       982 |   490 | `	if( base == 2 ){ return c == '0' \|\| c == '1'; }` |
|       703 |   491 | `	return SyisDigit(c);` |
|       543 |   492 | `}` |
|         - |   493 | `/*` |
|         - |   494 | ` * Given the raw text of a numeric literal token, locate a misplaced PHP 7.4` |
|         - |   495 | ` * underscore separator so the caller can report the malformed portion with` |
|         - |   496 | ` * the exact wording PHP uses:` |
|         - |   497 | ` *` |
|         - |   498 | ` *   syntax error, unexpected identifier "X"` |
|         - |   499 | ` *` |
|         - |   500 | ` * The lexer guarantees that every underscore it consumed as a separator is` |
|         - |   501 | ` * surrounded by valid base digits; anything else sits in the trailing run` |
|         - |   502 | ` * absorbed by the lexer specifically to let this validator see and report` |
|         - |   503 | ` * it. That invariant means the malformed span is exactly [bad .. nByte) —` |
|         - |   504 | ` * no forward rescan needed.` |
|         - |   505 | ` *` |
|         - |   506 | ` * Returns 1 and fills pBadStart / pBadLen when the literal is malformed;` |
|         - |   507 | ` * returns 0 when it is well-formed.` |
|         - |   508 | ` */` |
|   2796174 |   509 | `static int GenStateFindBadNumericSeparator(` |
|         - |   510 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|         5 |   511 | `{` |
|   2796179 |   512 | `	const char *z = pRaw->zString;` |
|   2796179 |   513 | `	sxu32 n = pRaw->nByte;` |
|   2796179 |   514 | `	int base = 10;` |
|         - |   515 | `	sxu32 i, start;` |
|   2796179 |   516 | `	if( n < 2 ) return 0;` |
|    457159 |   517 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|        80 |   518 | `		base = 16;` |
|    457120 |   519 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|       284 |   520 | `		base = 2;` |
|       141 |   521 | `	}` |
|   1530033 |   522 | `	for( i = 0; i < n; ++i ){` |
|   1072893 |   523 | `		if( z[i] != '_' ) continue;` |
|       546 |   524 | `		if( i > 0 && i + 1 < n` |
|       543 |   525 | `			&& GenStateIsBaseDigit((unsigned char)z[i-1], base)` |
|       543 |   526 | `			&& GenStateIsBaseDigit((unsigned char)z[i+1], base) ){` |
|       533 |   527 | `			continue; /* well-placed separator */` |
|         - |   528 | `		}` |
|         - |   529 | `		/* First misplaced underscore — the lexer already absorbed the full` |
|         - |   530 | `		 * malformed tail, so it runs from here to the end of the token. */` |
|        18 |   531 | `		start = i;` |
|        23 |   532 | `		if( start > 0 && (z[start-1] == 'x' \|\| z[start-1] == 'X'` |
|        12 |   533 | `			\|\| z[start-1] == 'b' \|\| z[start-1] == 'B') ){` |
|         6 |   534 | `			start--; /* include the base letter for 0x_... / 0b_... */` |
|         2 |   535 | `		}` |
|        18 |   536 | `		*pBadStart = &z[start];` |
|        18 |   537 | `		*pBadLen = n - start;` |
|        18 |   538 | `		return 1;` |
|       ! 0 |   539 | `	}` |
|    457145 |   540 | `	return 0;` |
|   1398092 |   541 | `}` |
|         - |   542 | `/*` |
|         - |   543 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|         - |   544 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|         - |   545 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|         - |   546 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|         - |   547 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|         - |   548 | ` * so callers can bail from the current construct).` |
|         - |   549 | ` */` |
|   2796174 |   550 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|         5 |   551 | `{` |
|   2796179 |   552 | `	const char *zBad = 0;` |
|   2796179 |   553 | `	sxu32 nBad = 0;` |
|         - |   554 | `	SyString sBad;` |
|         - |   555 | `	sxi32 rc;` |
|   2796179 |   556 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|   2796165 |   557 | `		return SXRET_OK;` |
|         - |   558 | `	}` |
|        18 |   559 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|        18 |   560 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|         - |   561 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|        18 |   562 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |   563 | `		return SXERR_ABORT;` |
|         - |   564 | `	}` |
|        18 |   565 | `	return SXERR_SYNTAX;` |
|   1398092 |   566 | `}` |
|         - |   567 | `/*` |
|         - |   568 | ` * Strip PHP 7.4 numeric literal separators (underscores between digits) from` |
|         - |   569 | ` * a numeric token's text and yield a SyString suitable for the low-level` |
|         - |   570 | ` * converters (SyStrToInt64 / SyStrToReal / etc.).` |
|         - |   571 | ` *` |
|         - |   572 | ` * Fast path: if the token contains no '_', *pOut aliases pToken with no copy` |
|         - |   573 | ` * and *pzAlloc is set to NULL.` |
|         - |   574 | ` * Stack path: if the cleaned bytes fit in zScratch, they are written there` |
|         - |   575 | ` * and *pzAlloc is set to NULL.` |
|         - |   576 | ` * Heap path: for literals larger than the scratch buffer, a fresh buffer is` |
|         - |   577 | ` * allocated from pAlloc, returned via *pzAlloc, and must be released by the` |
|         - |   578 | ` * caller with SyMemBackendFree once the converter is done.` |
|         - |   579 | ` *` |
|         - |   580 | ` * Returns SXRET_OK on success, SXERR_ABORT on allocator failure (in which` |
|         - |   581 | ` * case *pOut is left untouched and the caller must not read it).` |
|         - |   582 | ` */` |
|   2796160 |   583 | `static sxi32 GenStateStripNumericSeparators(` |
|         - |   584 | `	SyMemBackend *pAlloc,` |
|         - |   585 | `	const SyString *pToken,` |
|         - |   586 | `	char *zScratch, sxu32 nScratch,` |
|         - |   587 | `	SyString *pOut, char **pzAlloc)` |
|         5 |   588 | `{` |
|         - |   589 | `	sxu32 i, j;` |
|   2796165 |   590 | `	int hasUnderscore = 0;` |
|         - |   591 | `	char *zBuf;` |
|   2796165 |   592 | `	*pzAlloc = 0;` |
|   6205993 |   593 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|   3410085 |   594 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   1704919 |   595 | `	}` |
|   2796165 |   596 | `	if( !hasUnderscore ){` |
|   2795913 |   597 | `		SyStringDupPtr(pOut, pToken);` |
|   2795913 |   598 | `		return SXRET_OK;` |
|         - |   599 | `	}` |
|       253 |   600 | `	if( pToken->nByte <= nScratch ){` |
|       251 |   601 | `		zBuf = zScratch;` |
|       126 |   602 | `	}else{` |
|         3 |   603 | `		zBuf = (char *)SyMemBackendAlloc(pAlloc, pToken->nByte);` |
|         3 |   604 | `		if( zBuf == 0 ){` |
|       ! 0 |   605 | `			return SXERR_ABORT;` |
|         - |   606 | `		}` |
|         3 |   607 | `		*pzAlloc = zBuf;` |
|         - |   608 | `	}` |
|       253 |   609 | `	j = 0;` |
|      2895 |   610 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|      2643 |   611 | `		if( pToken->zString[i] != '_' ){ zBuf[j++] = pToken->zString[i]; }` |
|      1322 |   612 | `	}` |
|       253 |   613 | `	SyStringInitFromBuf(pOut, zBuf, j);` |
|       253 |   614 | `	return SXRET_OK;` |
|   1398085 |   615 | `}` |
|         - |   616 | `/*` |
|         - |   617 | ` * Compile a numeric [i.e: integer or real] literal.` |
|         - |   618 | ` * Notes on the integer type.` |
|         - |   619 | ` *  According to the PHP language reference manual` |
|         - |   620 | ` *  Integers can be specified in decimal (base 10), hexadecimal (base 16), octal (base 8)` |
|         - |   621 | ` *  or binary (base 2) notation, optionally preceded by a sign (- or +).` |
|         - |   622 | ` *  To use octal notation, precede the number with a 0 (zero). To use hexadecimal` |
|         - |   623 | ` *  notation precede the number with 0x. To use binary notation precede the number with 0b.` |
|         - |   624 | ` * Symisc eXtension to the integer type.` |
|         - |   625 | ` *  PH7 introduced platform-independant 64-bit integer unlike the standard PHP engine` |
|         - |   626 | ` *  where the size of an integer is platform-dependent.That is,the size of an integer` |
|         - |   627 | ` *  is 8 bytes and the maximum integer size is 0x7FFFFFFFFFFFFFFF for all platforms` |
|         - |   628 | ` *  [i.e: either 32bit or 64bit].` |
|         - |   629 | ` *  For more information on this powerfull extension please refer to the official` |
|         - |   630 | ` *  documentation.` |
|         - |   631 | ` */` |
|         - |   632 | `/*` |
|         - |   633 | ` * Determine whether an integer literal token exceeds the signed 64-bit range.` |
|         - |   634 | ` * PHP promotes such a literal to a float (e.g. 9223372036854775808 ->` |
|         - |   635 | ` * float(9.22...E+18), 0xFFFFFFFFFFFFFFFF -> float) rather than wrapping or` |
|         - |   636 | ` * dropping digits. pNum is the separator-stripped token (unsigned; the sign of` |
|         - |   637 | ` * a "-1" is a separate unary operator). Base detection mirrors` |
|         - |   638 | ` * PH7_TokenValueToInt64. Returns TRUE on overflow: for a non-decimal base the` |
|         - |   639 | ` * float value is accumulated into *pReal (dv = dv*base + digit); for decimal` |
|         - |   640 | ` * *pbDecimal is set so the caller reuses strtod on the token for a` |
|         - |   641 | ` * correctly-rounded value. Returns FALSE (value fits) for anything it cannot` |
|         - |   642 | ` * confidently classify, so the int path stays in charge.` |
|         - |   643 | ` *` |
|         - |   644 | ` * The int/float CLASSIFICATION is php-exact for every base. VALUES are byte-exact` |
|         - |   645 | ` * for decimal (strtod) and hex (php's zend_hex_strtod uses the same dv*16+digit` |
|         - |   646 | ` * doubling). Octal/binary overflow values can differ from php by the low bit(s):` |
|         - |   647 | ` * php's zend_{oct,bin}_strtod rounds differently than this doubling — e.g. php's` |
|         - |   648 | ` * binary 2**63 is 2**63-1024 whereas this returns the exact 2**63. Recorded as a` |
|         - |   649 | ` * residual; matching php exactly would need a port of those functions.` |
|         - |   650 | ` */` |
|   2795212 |   651 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|         5 |   652 | `{` |
|   2795217 |   653 | `	const char *z = pNum->zString;` |
|   2795217 |   654 | `	const char *zEnd = z + pNum->nByte;` |
|         - |   655 | `	const char *p, *q;` |
|         - |   656 | `	int n;` |
|   2795217 |   657 | `	*pbDecimal = FALSE;` |
|   2795217 |   658 | `	if( z >= zEnd ){` |
|       ! 0 |   659 | `		return FALSE;` |
|         - |   660 | `	}` |
|   2795217 |   661 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|         - |   662 | `		/* Hexadecimal: INT64_MAX == 0x7FFF...F (16 digits, leading nibble 7). */` |
|        77 |   663 | `		p = z + 2;` |
|        85 |   664 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|       493 |   665 | `		for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){ n++; }` |
|        77 |   666 | `		if( n < 16 \|\| (n == 16 && SyHexToint(p[0]) < 8) ){` |
|        71 |   667 | `			return FALSE;` |
|         - |   668 | `		}` |
|         7 |   669 | `		{ ph7_real dv = 0;` |
|       103 |   670 | `		  for( q = p; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){` |
|        97 |   671 | `			dv = dv * 16 + (ph7_real)SyHexToint(q[0]);` |
|        49 |   672 | `		  }` |
|         7 |   673 | `		  *pReal = dv;` |
|         - |   674 | `		}` |
|         7 |   675 | `		return TRUE;` |
|   2795141 |   676 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|         - |   677 | `		/* Binary: INT64_MAX needs 63 significant bits. */` |
|       281 |   678 | `		p = z + 2;` |
|       329 |   679 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|      2149 |   680 | `		for( q = p, n = 0; q < zEnd && (q[0] == '0' \|\| q[0] == '1'); q++ ){ n++; }` |
|       281 |   681 | `		if( n <= 63 ){` |
|       279 |   682 | `			return FALSE;` |
|         - |   683 | `		}` |
|         3 |   684 | `		{ ph7_real dv = 0;` |
|       195 |   685 | `		  for( q = p; q < zEnd && (q[0] == '0' \|\| q[0] == '1'); q++ ){` |
|       129 |   686 | `			dv = dv * 2 + (ph7_real)(q[0] - '0');` |
|        65 |   687 | `		  }` |
|         3 |   688 | `		  *pReal = dv;` |
|         - |   689 | `		}` |
|         3 |   690 | `		return TRUE;` |
|   2794861 |   691 | `	}else if( z[0] == '0' ){` |
|         - |   692 | `		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the` |
|         - |   693 | `		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1` |
|         - |   694 | `		 * "0o" marker ends the run and leaves it to the int path (as today). */` |
|   1057659 |   695 | `		p = z;` |
|   2115315 |   696 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|   1057887 |   697 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|   1057659 |   698 | `		if( n <= 21 ){` |
|   1057657 |   699 | `			return FALSE;` |
|         - |   700 | `		}` |
|         3 |   701 | `		{ ph7_real dv = 0;` |
|        47 |   702 | `		  for( q = p; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){` |
|        45 |   703 | `			dv = dv * 8 + (ph7_real)(q[0] - '0');` |
|        23 |   704 | `		  }` |
|         3 |   705 | `		  *pReal = dv;` |
|         - |   706 | `		}` |
|         3 |   707 | `		return TRUE;` |
|         - |   708 | `	}` |
|         - |   709 | `	/* Decimal: overflow iff more than 19 significant digits, or exactly 19 that` |
|         - |   710 | `	 * compare greater than INT64_MAX. Defer the value to strtod (via the caller)` |
|         - |   711 | `	 * for php-exact rounding. */` |
|   1737207 |   712 | `	p = z;` |
|   1737207 |   713 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
|   4082893 |   714 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|   1737207 |   715 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|        25 |   716 | `		*pbDecimal = TRUE;` |
|        25 |   717 | `		return TRUE;` |
|         - |   718 | `	}` |
|   1737183 |   719 | `	return FALSE;` |
|   1397611 |   720 | `}` |
|   2796146 |   721 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |   722 | `{` |
|   2796151 |   723 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   2796151 |   724 | `	sxu32 nIdx = 0;` |
|         - |   725 | `	char zScratch[GEN_NUM_SCRATCH];` |
|   2796151 |   726 | `	char *zAlloc = 0;` |
|         - |   727 | `	SyString sNum;` |
|         - |   728 | `	sxi32 rc;` |
|   1398073 |   729 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|   2796151 |   730 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|   2796151 |   731 | `	if( rc != SXRET_OK ){` |
|        14 |   732 | `		return rc;` |
|         - |   733 | `	}` |
|   4194209 |   734 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   1398068 |   735 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|   2796141 |   736 | `	if( rc != SXRET_OK ){` |
|       ! 0 |   737 | `		return SXERR_ABORT;` |
|         - |   738 | `	}` |
|   2796141 |   739 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|         - |   740 | `		ph7_value *pObj;` |
|         - |   741 | `		sxi64 iValue;` |
|   2795217 |   742 | `		ph7_real rOverflow = 0;` |
|   2795217 |   743 | `		int bDecimalOverflow = 0;` |
|   2795217 |   744 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
|         - |   745 | `			/* Literal exceeds the signed 64-bit range: PHP represents it as a` |
|         - |   746 | `			 * float instead of wrapping/dropping digits. */` |
|        35 |   747 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        35 |   748 | `			if( pObj == 0 ){` |
|       ! 0 |   749 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   750 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       ! 0 |   751 | `				return SXERR_ABORT;` |
|         - |   752 | `			}` |
|        35 |   753 | `			if( bDecimalOverflow ){` |
|         - |   754 | `				/* strtod on the decimal token yields php-exact rounding. */` |
|        25 |   755 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|        25 |   756 | `				PH7_MemObjToReal(pObj);` |
|        13 |   757 | `			}else{` |
|        11 |   758 | `				PH7_MemObjInitFromReal(pGen->pVm,pObj,rOverflow);` |
|         - |   759 | `			}` |
|        18 |   760 | `		}else{` |
|   2795183 |   761 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|   2795183 |   762 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   2795183 |   763 | `			if( pObj == 0 ){` |
|       ! 0 |   764 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       ! 0 |   765 | `				return SXERR_ABORT;` |
|         - |   766 | `			}` |
|   2795183 |   767 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|         - |   768 | `		}` |
|   1397611 |   769 | `	}else{` |
|         - |   770 | `		/* Real number */` |
|         - |   771 | `		ph7_value *pObj;` |
|         - |   772 | `		/* Reserve a new constant */` |
|       927 |   773 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       927 |   774 | `		if( pObj == 0 ){` |
|       ! 0 |   775 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   776 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       ! 0 |   777 | `			return SXERR_ABORT;` |
|         - |   778 | `		}` |
|       927 |   779 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|       927 |   780 | `		PH7_MemObjToReal(pObj);` |
|         - |   781 | `	}` |
|   2796141 |   782 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         - |   783 | `	/* Emit the load constant instruction */` |
|   2796141 |   784 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |   785 | `	/* Node successfully compiled */` |
|   2796141 |   786 | `	return SXRET_OK;` |
|   1398078 |   787 | `}` |
|         - |   788 | `/*` |
|         - |   789 | ` * Compile a single quoted string.` |
|         - |   790 | ` * According to the PHP language reference manual:` |
|         - |   791 | ` *` |
|         - |   792 | ` *   The simplest way to specify a string is to enclose it in single quotes (the character ' ).` |
|         - |   793 | ` *   To specify a literal single quote, escape it with a backslash (\). To specify a literal` |
|         - |   794 | ` *   backslash, double it (\\). All other instances of backslash will be treated as a literal` |
|         - |   795 | ` *   backslash: this means that the other escape sequences you might be used to, such as \r` |
|         - |   796 | ` *   or \n, will be output literally as specified rather than having any special meaning.` |
|         - |   797 | ` *` |
|         - |   798 | ` */` |
|   4443554 |   799 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |   800 | `{` |
|   4443559 |   801 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|         - |   802 | `	const char *zIn,*zCur,*zEnd;` |
|         - |   803 | `	ph7_value *pObj;` |
|         - |   804 | `	sxu32 nIdx;` |
|   4443559 |   805 | `	nIdx = 0; /* Prevent compiler warning */` |
|         - |   806 | `	/* Delimit the string */` |
|   4443559 |   807 | `	zIn  = pStr->zString;` |
|   4443559 |   808 | `	zEnd = &zIn[pStr->nByte];` |
|   4443559 |   809 | `	if( zIn >= zEnd ){` |
|         - |   810 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|         - |   811 | `		 * rather than reserving a new object each time. */` |
|    208923 |   812 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    208923 |   813 | `		return SXRET_OK;` |
|         - |   814 | `	}` |
|   4234641 |   815 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|         - |   816 | `		/* Already processed,emit the load constant instruction` |
|         - |   817 | `		 * and return.` |
|         - |   818 | `		 */` |
|   2470001 |   819 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   2470001 |   820 | `		return SXRET_OK;` |
|         - |   821 | `	}` |
|         - |   822 | `	/* Reserve a new constant */` |
|   1764645 |   823 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   1764645 |   824 | `	if( pObj == 0 ){` |
|       ! 0 |   825 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |   826 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |   827 | `		return SXERR_ABORT;` |
|         - |   828 | `	}` |
|   1764645 |   829 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|         - |   830 | `	/* Compile the node */` |
|   1808018 |   831 | `	for(;;){` |
|   3616041 |   832 | `		if( zIn >= zEnd ){` |
|         - |   833 | `			/* End of input */` |
|   1764645 |   834 | `			break;` |
|         - |   835 | `		}` |
|   1851401 |   836 | `		zCur = zIn;` |
|  37929345 |   837 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  36077949 |   838 | `			zIn++;` |
|         5 |   839 | `		}` |
|   1851401 |   840 | `		if( zIn > zCur ){` |
|         - |   841 | `			/* Append raw contents*/` |
|   1811991 |   842 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    905993 |   843 | `		}` |
|   1851401 |   844 | `		zIn++;` |
|   1851401 |   845 | `		if( zIn < zEnd ){` |
|    122229 |   846 | `			if( zIn[0] == '\\' ){` |
|         - |   847 | `				/* A literal backslash */` |
|     31543 |   848 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|    106460 |   849 | `			}else if( zIn[0] == '\'' ){` |
|         - |   850 | `				/* A single quote */` |
|        11 |   851 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|         6 |   852 | `			}else{` |
|         - |   853 | `				/* verbatim copy */` |
|     90681 |   854 | `				zIn--;` |
|     90681 |   855 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|     90681 |   856 | `				zIn++;` |
|         - |   857 | `			}` |
|     61112 |   858 | `		}` |
|         - |   859 | `		/* Advance the stream cursor */` |
|   1851401 |   860 | `		zIn++;` |
|         5 |   861 | `	}` |
|         - |   862 | `	/* Emit the load constant instruction */` |
|   1764645 |   863 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   1764645 |   864 | `	if( pStr->nByte < 1024 ){` |
|         - |   865 | `		/* Install in the literal table */` |
|   1764645 |   866 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|    882320 |   867 | `	}` |
|         - |   868 | `	/* Node successfully compiled */` |
|   1764645 |   869 | `	return SXRET_OK;` |
|   2221782 |   870 | `}` |
|         - |   871 | `/*` |
|         - |   872 | ` * PHP 7.3 flexible heredoc/nowdoc closing-marker indent stripping.` |
|         - |   873 | ` *` |
|         - |   874 | ` * When the lexer matched the closing marker with leading whitespace on its` |
|         - |   875 | ` * own line, it stored the indent count in pGen->pIn->pUserData. The marker's` |
|         - |   876 | ` * indent prefix bytes sit immediately after the stripped body (at` |
|         - |   877 | ` * pIn->sData.zString + pIn->sData.nByte + 1 for LF, +2 for CRLF) in the` |
|         - |   878 | ` * original source buffer — the buffer is stable through compilation.` |
|         - |   879 | ` *` |
|         - |   880 | `` * For each body line, we remove exactly `nIndent` leading bytes that must`` |
|         - |   881 | ` * byte-for-byte match the marker's prefix. Empty lines (0 bytes or bare \r)` |
|         - |   882 | ` * bypass validation. Mismatches raise the exact PHP 7.3+ parse errors:` |
|         - |   883 | ` *   - "Invalid body indentation level (expecting an indentation level of` |
|         - |   884 | ` *     at least N)" — line too short, or first differing byte is not` |
|         - |   885 | ` *     whitespace.` |
|         - |   886 | ` *   - "Invalid indentation - tabs and spaces cannot be mixed" — first` |
|         - |   887 | ` *     differing byte is whitespace but differs from the marker prefix.` |
|         - |   888 | ` */` |
|       114 |   889 | `static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)` |
|         4 |   890 | `{` |
|       118 |   891 | `	SyString *pIn = &pGen->pIn->sData;` |
|       118 |   892 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - |   893 | `	const char *zPrefix;` |
|         - |   894 | `	const char *z, *zEnd;` |
|         - |   895 | `	char *zBuf, *zDst;` |
|       118 |   896 | `	if( nIndent == 0 ){` |
|         - |   897 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|        73 |   898 | `		*pOut = *pIn;` |
|        73 |   899 | `		return SXRET_OK;` |
|         - |   900 | `	}` |
|         - |   901 | `	/* Recover the marker indent prefix from the original source buffer.` |
|         - |   902 | `	 * Skip the terminator the lexer stripped: one '\n' plus an optional` |
|         - |   903 | `	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the` |
|         - |   904 | `	 * lexer stripped nothing, so this offset is one byte past the true` |
|         - |   905 | `	 * marker-indent start. That is harmless — the strip loop below never` |
|         - |   906 | `	 * runs (z == zEnd), and zPrefix is never dereferenced. */` |
|        47 |   907 | `	zPrefix = pIn->zString + pIn->nByte;` |
|        47 |   908 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|       ! 0 |   909 | `		zPrefix += 2;` |
|       ! 0 |   910 | `	}else{` |
|        47 |   911 | `		zPrefix += 1;` |
|         - |   912 | `	}` |
|         - |   913 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|        47 |   914 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|        47 |   915 | `	if( zBuf == 0 ){` |
|       ! 0 |   916 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |   917 | `		return SXERR_ABORT;` |
|         - |   918 | `	}` |
|        47 |   919 | `	zDst = zBuf;` |
|        47 |   920 | `	z = pIn->zString;` |
|        47 |   921 | `	zEnd = z + pIn->nByte;` |
|       129 |   922 | `	while( z < zEnd ){` |
|        71 |   923 | `		const char *zLine = z;` |
|         - |   924 | `		sxu32 nLine;` |
|         - |   925 | `		int bEmpty;` |
|       799 |   926 | `		while( z < zEnd && z[0] != '\n' ){` |
|       731 |   927 | `			z++;` |
|         3 |   928 | `		}` |
|        71 |   929 | `		nLine = (sxu32)(z - zLine);` |
|        71 |   930 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|        71 |   931 | `		if( !bEmpty ){` |
|         - |   932 | `			sxu32 i;` |
|        67 |   933 | `			if( nLine < nIndent ){` |
|       ! 0 |   934 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |   935 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|       ! 0 |   936 | `					nIndent);` |
|       ! 0 |   937 | `				return SXERR_ABORT;` |
|         - |   938 | `			}` |
|       269 |   939 | `			for( i = 0; i < nIndent; i++ ){` |
|       213 |   940 | `				if( zLine[i] != zPrefix[i] ){` |
|        10 |   941 | `					unsigned char c = (unsigned char)zLine[i];` |
|        10 |   942 | `					if( c == ' ' \|\| c == '\t' ){` |
|         5 |   943 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |   944 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|         3 |   945 | `					}else{` |
|         7 |   946 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |   947 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|         2 |   948 | `							nIndent);` |
|         - |   949 | `					}` |
|        10 |   950 | `					return SXERR_ABORT;` |
|         - |   951 | `				}` |
|       103 |   952 | `			}` |
|        57 |   953 | `			SyMemcpy((const void *)(zLine + nIndent), (void *)zDst, nLine - nIndent);` |
|        57 |   954 | `			zDst += nLine - nIndent;` |
|        33 |   955 | `		}else if( nLine == 1 ){` |
|         - |   956 | `			/* Preserve the stray '\r' on an otherwise empty line */` |
|       ! 0 |   957 | `			*zDst++ = '\r';` |
|       ! 0 |   958 | `		}` |
|        61 |   959 | `		if( z < zEnd ){` |
|        25 |   960 | `			*zDst++ = '\n';` |
|        25 |   961 | `			z++;` |
|        12 |   962 | `		}` |
|         1 |   963 | `	}` |
|        37 |   964 | `	pOut->zString = zBuf;` |
|        37 |   965 | `	pOut->nByte = (sxu32)(zDst - zBuf);` |
|        37 |   966 | `	return SXRET_OK;` |
|        61 |   967 | `}` |
|         - |   968 | `/*` |
|         - |   969 | ` * Compile a nowdoc string.` |
|         - |   970 | ` * According to the PHP language reference manual:` |
|         - |   971 | ` *` |
|         - |   972 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|         - |   973 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|         - |   974 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the` |
|         - |   975 | ` *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]>` |
|         - |   976 | ` *  construct, in that it declares a block of text which is not for parsing.` |
|         - |   977 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier` |
|         - |   978 | ` *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc` |
|         - |   979 | ` *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance` |
|         - |   980 | ` *  of the closing identifier.` |
|         - |   981 | ` */` |
|        48 |   982 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         3 |   983 | `{` |
|         - |   984 | `	SyString sStripped;` |
|         - |   985 | `	SyString *pStr;` |
|         - |   986 | `	ph7_value *pObj;` |
|         - |   987 | `	sxu32 nIdx;` |
|         - |   988 | `	sxi32 rc;` |
|        51 |   989 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|        51 |   990 | `	if( rc != SXRET_OK ){` |
|         6 |   991 | `		return rc;` |
|         - |   992 | `	}` |
|        46 |   993 | `	pStr = &sStripped;` |
|        46 |   994 | `	nIdx = 0; /* Prevent compiler warning */` |
|        46 |   995 | `	if( pStr->nByte <= 0 ){` |
|         - |   996 | `		/* Empty string,load NULL */` |
|         7 |   997 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         7 |   998 | `		return SXRET_OK;` |
|         - |   999 | `	}` |
|         - |  1000 | `	/* Reserve a new constant */` |
|        40 |  1001 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        40 |  1002 | `	if( pObj == 0 ){` |
|       ! 0 |  1003 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  1004 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  1005 | `		return SXERR_ABORT;` |
|         - |  1006 | `	}` |
|         - |  1007 | `	/* No processing is done here, simply a memcpy() operation */` |
|        40 |  1008 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|         - |  1009 | `	/* Emit the load constant instruction */` |
|        40 |  1010 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |  1011 | `	/* Node successfully compiled */` |
|        40 |  1012 | `	return SXRET_OK;` |
|        27 |  1013 | `}` |
|         - |  1014 | `/*` |
|         - |  1015 | ` * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.` |
|         - |  1016 | ` * According to the PHP language reference manual` |
|         - |  1017 | ` *   When a string is specified in double quotes or with heredoc,variables are parsed within it.` |
|         - |  1018 | ` *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most` |
|         - |  1019 | ` *  common and convenient. It provides a way to embed a variable, an array value, or an object` |
|         - |  1020 | ` *  property in a string with a minimum of effort.` |
|         - |  1021 | ` *  Simple syntax` |
|         - |  1022 | ` *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible` |
|         - |  1023 | ` *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify` |
|         - |  1024 | ` *   the end of the name.` |
|         - |  1025 | ` *   Similarly, an array index or an object property can be parsed. With array indices, the closing` |
|         - |  1026 | ` *   square bracket (]) marks the end of the index. The same rules apply to object properties` |
|         - |  1027 | ` *   as to simple variables.` |
|         - |  1028 | ` *  Complex (curly) syntax` |
|         - |  1029 | ` *   This isn't called complex because the syntax is complex, but because it allows for the use` |
|         - |  1030 | ` *   of complex expressions.` |
|         - |  1031 | ` *   Any scalar variable, array element or object property with a string representation can be` |
|         - |  1032 | ` *   included via this syntax. Simply write the expression the same way as it would appear outside` |
|         - |  1033 | ` *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only` |
|         - |  1034 | ` *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$` |
|         - |  1035 | ` */` |
|      2582 |  1036 | `static sxi32 GenStateProcessStringExpression(` |
|         - |  1037 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  1038 | `	sxu32 nLine,         /* Line number */` |
|         - |  1039 | `	const char *zIn,     /* Raw expression */` |
|         - |  1040 | `	const char *zEnd     /* End of the expression */` |
|         - |  1041 | `	)` |
|         5 |  1042 | `{` |
|         - |  1043 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  1044 | `	SySet sToken;` |
|         - |  1045 | `	sxi32 rc;` |
|         - |  1046 | `	/* Initialize the token set */` |
|      2587 |  1047 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         - |  1048 | `	/* Preallocate some slots */` |
|      2587 |  1049 | `	SySetAlloc(&sToken,0x08);` |
|         - |  1050 | `	/* Tokenize the text */` |
|      2587 |  1051 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken,0);` |
|         - |  1052 | `	/* Swap delimiter */` |
|      2587 |  1053 | `	pTmpIn  = pGen->pIn;` |
|      2587 |  1054 | `	pTmpEnd = pGen->pEnd;` |
|      2587 |  1055 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      2587 |  1056 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|         - |  1057 | `	/* Compile the expression */` |
|      2587 |  1058 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  1059 | `	/* Restore token stream */` |
|      2587 |  1060 | `	pGen->pIn  = pTmpIn;` |
|      2587 |  1061 | `	pGen->pEnd = pTmpEnd;` |
|         - |  1062 | `	/* Release the token set */` |
|      2587 |  1063 | `	SySetRelease(&sToken);` |
|         - |  1064 | `	/* Compilation result */` |
|      2587 |  1065 | `	return rc;` |
|         5 |  1066 | `}` |
|         - |  1067 | `/*` |
|         - |  1068 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|         - |  1069 | ` */` |
|     83760 |  1070 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|         5 |  1071 | `{` |
|         - |  1072 | `	ph7_value *pConstObj;` |
|     83765 |  1073 | `	sxu32 nIdx = 0;` |
|         - |  1074 | `	/* Reserve a new constant */` |
|     83765 |  1075 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     83765 |  1076 | `	if( pConstObj == 0 ){` |
|       ! 0 |  1077 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  1078 | `		return 0;` |
|         - |  1079 | `	}` |
|     83765 |  1080 | `	(*pCount)++;` |
|     83765 |  1081 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|         - |  1082 | `	/* Emit the load constant instruction */` |
|     83765 |  1083 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     83765 |  1084 | `	return pConstObj;` |
|     41885 |  1085 | `}` |
|         - |  1086 | `/*` |
|         - |  1087 | ` * Compile a double quoted/heredoc string.` |
|         - |  1088 | ` * According to the PHP language reference manual` |
|         - |  1089 | ` * Heredoc` |
|         - |  1090 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|         - |  1091 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|         - |  1092 | ` *  to close the quotation.` |
|         - |  1093 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|         - |  1094 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|         - |  1095 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|         - |  1096 | ` *  Warning` |
|         - |  1097 | ` *  It is very important to note that the line with the closing identifier must contain` |
|         - |  1098 | ` *  no other characters, except possibly a semicolon (;). That means especially that the identifier` |
|         - |  1099 | ` *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.` |
|         - |  1100 | ` *  It's also important to realize that the first character before the closing identifier must` |
|         - |  1101 | ` *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.` |
|         - |  1102 | ` *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.` |
|         - |  1103 | ` *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing` |
|         - |  1104 | ` *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before` |
|         - |  1105 | ` *  the end of the current file, a parse error will result at the last line.` |
|         - |  1106 | ` *  Heredocs can not be used for initializing class properties.` |
|         - |  1107 | ` * Double quoted` |
|         - |  1108 | ` *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:` |
|         - |  1109 | ` *  Escaped characters Sequence 	Meaning` |
|         - |  1110 | ` *  \n linefeed (LF or 0x0A (10) in ASCII)` |
|         - |  1111 | ` *  \r carriage return (CR or 0x0D (13) in ASCII)` |
|         - |  1112 | ` *  \t horizontal tab (HT or 0x09 (9) in ASCII)` |
|         - |  1113 | ` *  \v vertical tab (VT or 0x0B (11) in ASCII)` |
|         - |  1114 | ` *  \e escape (ESC or 0x1B (27) in ASCII)` |
|         - |  1115 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|         - |  1116 | ` *  \\ backslash` |
|         - |  1117 | ` *  \$ dollar sign` |
|         - |  1118 | ` *  \" double-quote` |
|         - |  1119 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation,` |
|         - |  1120 | ` *      which silently overflows to fit in a byte (e.g. "\400" === "\000")` |
|         - |  1121 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|         - |  1122 | ` *  \u{[0-9A-Fa-f]+} 	the sequence of characters matching the regular expression is a Unicode codepoint,` |
|         - |  1123 | ` *      which will be output to the string as that codepoint's UTF-8 representation` |
|         - |  1124 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|         - |  1125 | ` * (The PH7-ism "\oNNN" octal form is gone: a literal "\o" now round-trips like php 8.)` |
|         - |  1126 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|         - |  1127 | ` * See string parsing for details.` |
|         - |  1128 | ` */` |
|         - |  1129 | `/*` |
|         - |  1130 | ` * Line number of an escape sequence inside the string body being compiled:` |
|         - |  1131 | ` * the token's line plus every newline before the escape (php reports the` |
|         - |  1132 | ` * escape's own line, not the string's opening line). A heredoc body starts` |
|         - |  1133 | ` * on the line after the '<<<' marker, hence the +1.` |
|         - |  1134 | ` */` |
|         6 |  1135 | `static sxu32 GenStateStringEscLine(ph7_gen_state *pGen,const char *zPos,int bHeredoc)` |
|         3 |  1136 | `{` |
|         9 |  1137 | `	const char *z = pGen->pIn->sData.zString;` |
|         9 |  1138 | `	sxu32 nLine = pGen->pIn->nLine + (bHeredoc ? 1 : 0);` |
|        15 |  1139 | `	for( ; z < zPos ; z++ ){` |
|         9 |  1140 | `		if( z[0] == '\n' ){` |
|       ! 0 |  1141 | `			nLine++;` |
|       ! 0 |  1142 | `		}` |
|         6 |  1143 | `	}` |
|         9 |  1144 | `	return nLine;` |
|         3 |  1145 | `}` |
|         - |  1146 | `/* bHeredoc: php strips the backslash from '\"' only when '"' is the active` |
|         - |  1147 | ` * quote character; a heredoc has none, so '\"' stays verbatim there. */` |
|     82190 |  1148 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|         5 |  1149 | `{` |
|     82195 |  1150 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|         - |  1151 | `	const char *zIn,*zCur,*zEnd;` |
|     82195 |  1152 | `	ph7_value *pObj = 0;` |
|         - |  1153 | `	sxi32 iCons;` |
|         - |  1154 | `	sxi32 rc;` |
|         - |  1155 | `	/* Delimit the string */` |
|     82195 |  1156 | `	zIn  = pStr->zString;` |
|     82195 |  1157 | `	zEnd = &zIn[pStr->nByte];` |
|     82195 |  1158 | `	if( zIn >= zEnd ){` |
|         - |  1159 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|         - |  1160 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|         - |  1161 | `		 * literal table from growing when many "" literals appear in the source.` |
|         - |  1162 | `		 */` |
|       385 |  1163 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|       385 |  1164 | `		return SXRET_OK;` |
|         - |  1165 | `	}` |
|     81815 |  1166 | `	zCur = 0;` |
|         - |  1167 | `	/* Compile the node */` |
|     81815 |  1168 | `	iCons = 0;` |
|     42196 |  1169 | `	for(;;){` |
|    117169 |  1170 | `		zCur = zIn;` |
|   1585661 |  1171 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|   1471079 |  1172 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|        72 |  1173 | `				break;` |
|   1470946 |  1174 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|      2454 |  1175 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|      1227 |  1176 | `					break;` |
|         - |  1177 | `			}` |
|   1468497 |  1178 | `			zIn++;` |
|         5 |  1179 | `		}` |
|    117169 |  1180 | `		if( zIn > zCur ){` |
|     56931 |  1181 | `			if( pObj == 0 ){` |
|     56337 |  1182 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|     56337 |  1183 | `				if( pObj == 0 ){` |
|       ! 0 |  1184 | `					return SXERR_ABORT;` |
|         - |  1185 | `				}` |
|     28166 |  1186 | `			}` |
|     56931 |  1187 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|     28463 |  1188 | `		}` |
|    117169 |  1189 | `		if( zIn >= zEnd ){` |
|     81813 |  1190 | `			break;` |
|         - |  1191 | `		}` |
|     35361 |  1192 | `		if( zIn[0] == '\\' ){` |
|     32779 |  1193 | `			const char *zPtr = 0;` |
|         - |  1194 | `			sxu32 n;` |
|     32779 |  1195 | `			zIn++;` |
|     32779 |  1196 | `			if( pObj == 0 ){` |
|     27433 |  1197 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|     27433 |  1198 | `				if( pObj == 0 ){` |
|       ! 0 |  1199 | `					return SXERR_ABORT;` |
|         - |  1200 | `				}` |
|     13714 |  1201 | `			}` |
|     32779 |  1202 | `			if( zIn >= zEnd ){` |
|         - |  1203 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|         3 |  1204 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|         3 |  1205 | `				break;` |
|         - |  1206 | `			}` |
|     32777 |  1207 | `			n = sizeof(char); /* size of conversion */` |
|     32777 |  1208 | `			switch( zIn[0] ){` |
|        11 |  1209 | `			case '$':` |
|         - |  1210 | `				/* Dollar sign */` |
|        25 |  1211 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|        25 |  1212 | `				break;` |
|        52 |  1213 | `			case '\\':` |
|         - |  1214 | `				/* A literal backslash */` |
|       109 |  1215 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|       109 |  1216 | `				break;` |
|         1 |  1217 | `			case 'e':` |
|         - |  1218 | `				/* Escape (ESC) ASCII code 27 */` |
|         3 |  1219 | `				PH7_MemObjStringAppend(pObj,"\x1b",sizeof(char));` |
|         3 |  1220 | `				break;` |
|         4 |  1221 | `			case 'f':` |
|         - |  1222 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|         9 |  1223 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|         9 |  1224 | `				break;` |
|     13824 |  1225 | `			case 'n':` |
|         - |  1226 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|     27653 |  1227 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|     27653 |  1228 | `				break;` |
|        27 |  1229 | `			case 'r':` |
|         - |  1230 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|        59 |  1231 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|        59 |  1232 | `				break;` |
|      2000 |  1233 | `			case 't':` |
|         - |  1234 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      4005 |  1235 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      4005 |  1236 | `				break;` |
|         3 |  1237 | `			case 'v':` |
|         - |  1238 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|         7 |  1239 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|         7 |  1240 | `				break;` |
|       141 |  1241 | `			case '"':` |
|       287 |  1242 | `				if( bHeredoc ){` |
|         - |  1243 | `					/* No active quote char in a heredoc: php keeps \" verbatim */` |
|         5 |  1244 | `					PH7_MemObjStringAppend(pObj,"\\\"",sizeof(char)*2);` |
|         3 |  1245 | `				}else{` |
|         - |  1246 | `					/* Double quote */` |
|       283 |  1247 | `					PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|         - |  1248 | `				}` |
|       287 |  1249 | `				break;` |
|        25 |  1250 | `			case '0': case '1': case '2': case '3':` |
|         - |  1251 | `			case '4': case '5': case '6': case '7': {` |
|         - |  1252 | `				/* \[0-7]{1,3}: a character in octal notation. A value above \377` |
|         - |  1253 | `				 * warns and wraps to the low byte, matching php 8. */` |
|        52 |  1254 | `				int c = 0;` |
|         - |  1255 | `				char cOut;` |
|       148 |  1256 | `				for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|       126 |  1257 | `					if( zPtr >= zEnd \|\| zPtr[0] < '0' \|\| zPtr[0] > '7' ){` |
|        15 |  1258 | `						break;` |
|         - |  1259 | `					}` |
|        98 |  1260 | `					c = c * 8 + (zPtr[0] - '0');` |
|        50 |  1261 | `				}` |
|        52 |  1262 | `				if( c > 0xFF ){` |
|         - |  1263 | `					SyString sSeq;` |
|         3 |  1264 | `					SyStringInitFromBuf(&sSeq,zIn,(sxu32)(zPtr-zIn));` |
|         3 |  1265 | `					PH7_GenCompileError(&(*pGen),E_WARNING,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|         - |  1266 | `						"Octal escape sequence overflow \\%z is greater than \\377",&sSeq);` |
|         3 |  1267 | `					c &= 0xFF;` |
|         1 |  1268 | `				}` |
|        52 |  1269 | `				cOut = (char)c; /* value byte, independent of host endianness */` |
|        52 |  1270 | `				PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|        52 |  1271 | `				n = (sxu32)(zPtr-zIn);` |
|        52 |  1272 | `				break;` |
|         - |  1273 | `			}` |
|       273 |  1274 | `			case 'x':` |
|       818 |  1275 | `				if( &zIn[1] < zEnd && SyisHex((unsigned char)zIn[1]) ){` |
|         - |  1276 | `					/* \x[0-9A-Fa-f]{1,2}: a character in hexadecimal notation */` |
|       543 |  1277 | `					int c = SyHexToint(zIn[1]);` |
|         - |  1278 | `					char cOut;` |
|       543 |  1279 | `					n += sizeof(char);` |
|       543 |  1280 | `					if( &zIn[2] < zEnd && SyisHex((unsigned char)zIn[2]) ){` |
|       539 |  1281 | `						c = (c << 4) + SyHexToint(zIn[2]);` |
|       539 |  1282 | `						n += sizeof(char);` |
|       269 |  1283 | `					}` |
|       543 |  1284 | `					cOut = (char)c; /* value byte, independent of host endianness */` |
|       543 |  1285 | `					PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|       272 |  1286 | `				}else{` |
|         - |  1287 | `					/* Not an escape: keep the backslash, as php does */` |
|         5 |  1288 | `					PH7_MemObjStringAppend(pObj,"\\x",sizeof(char)*2);` |
|         - |  1289 | `				}` |
|       547 |  1290 | `				break;` |
|         9 |  1291 | `			case 'u':` |
|        18 |  1292 | `				if( &zIn[1] < zEnd && zIn[1] == '{'` |
|        22 |  1293 | `				 && !(&zIn[2] < zEnd && zIn[2] == '$') ){` |
|         - |  1294 | `					/* \u{codepoint}: UTF-8 encoding of the given codepoint (php 7+).` |
|         - |  1295 | `					 * php encodes surrogates verbatim, so the only invalid value` |
|         - |  1296 | `					 * is > U+10FFFF; malformed/empty braces are a compile error.` |
|         - |  1297 | `					 * "\u{$..." is excluded above: php treats it as a literal \u` |
|         - |  1298 | `					 * followed by {$...} curly interpolation. */` |
|        15 |  1299 | `					sxu32 nCp = 0;` |
|        15 |  1300 | `					zPtr = &zIn[2];` |
|        59 |  1301 | `					while( zPtr < zEnd && SyisHex((unsigned char)zPtr[0]) ){` |
|        46 |  1302 | `						if( nCp <= 0x10FFFF ){` |
|         - |  1303 | `							/* stop accumulating once out of range: keeps a long` |
|         - |  1304 | `							 * digit run from wrapping sxu32 */` |
|        46 |  1305 | `							nCp = nCp * 16 + (sxu32)SyHexToint(zPtr[0]);` |
|        22 |  1306 | `						}` |
|        46 |  1307 | `						zPtr++;` |
|         2 |  1308 | `					}` |
|        15 |  1309 | `					if( zPtr == &zIn[2] \|\| zPtr >= zEnd \|\| zPtr[0] != '}' ){` |
|         - |  1310 | `						/* Error recorded (nErr>0 fails the whole compile); consume the` |
|         - |  1311 | `						 * malformed sequence so later errors are still reported. */` |
|         3 |  1312 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|         - |  1313 | `							"Invalid UTF-8 codepoint escape sequence");` |
|         3 |  1314 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  1315 | `							return SXERR_ABORT;` |
|         - |  1316 | `						}` |
|         3 |  1317 | `						n = (sxu32)(zPtr-zIn);` |
|         3 |  1318 | `						if( zPtr < zEnd && zPtr[0] == '}' ){` |
|         3 |  1319 | `							n += sizeof(char);` |
|         1 |  1320 | `						}` |
|         3 |  1321 | `						break;` |
|         - |  1322 | `					}` |
|        12 |  1323 | `					n = (sxu32)(&zPtr[1]-zIn); /* 'u{...}' incl. closing brace */` |
|        12 |  1324 | `					if( nCp > 0x10FFFF ){` |
|         3 |  1325 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|         - |  1326 | `							"Invalid UTF-8 codepoint escape sequence: Codepoint too large");` |
|         3 |  1327 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  1328 | `							return SXERR_ABORT;` |
|         - |  1329 | `						}` |
|         3 |  1330 | `						break;` |
|         - |  1331 | `					}` |
|         - |  1332 | `					{` |
|         - |  1333 | `						char zUtf[4];` |
|         9 |  1334 | `						sxu8 *zOut = (sxu8 *)zUtf;` |
|         9 |  1335 | `						SX_WRITE_UTF8(zOut,nCp);` |
|         9 |  1336 | `						PH7_MemObjStringAppend(pObj,zUtf,(sxu32)(zOut-(sxu8 *)zUtf));` |
|         - |  1337 | `					}` |
|         5 |  1338 | `				}else{` |
|         - |  1339 | `					/* Not an escape: keep the backslash, as php does */` |
|         7 |  1340 | `					PH7_MemObjStringAppend(pObj,"\\u",sizeof(char)*2);` |
|         - |  1341 | `				}` |
|        15 |  1342 | `				break;` |
|        16 |  1343 | `			default:` |
|         - |  1344 | `				/* Unrecognized escape: keep the backslash, as php does.` |
|         - |  1345 | `				 * zIn[-1] is the backslash itself, so both bytes are contiguous` |
|         - |  1346 | `				 * in the source buffer — one batched append. */` |
|        33 |  1347 | `				PH7_MemObjStringAppend(pObj,&zIn[-1],sizeof(char)*2);` |
|        32 |  1348 | `				break;` |
|         - |  1349 | `			}` |
|         - |  1350 | `			/* Advance the stream cursor */` |
|     32777 |  1351 | `			zIn += n;` |
|     32777 |  1352 | `			continue;` |
|         - |  1353 | `		}` |
|      2587 |  1354 | `		if( zIn[0] == '{' ){` |
|         - |  1355 | `			/* Curly syntax */` |
|         - |  1356 | `			const char *zExpr;` |
|       141 |  1357 | `			sxi32 iNest = 1;` |
|       141 |  1358 | `			zIn++;` |
|       141 |  1359 | `			zExpr = zIn;` |
|         - |  1360 | `			/* Synchronize with the next closing curly braces */` |
|      1419 |  1361 | `			while( zIn < zEnd ){` |
|      1419 |  1362 | `				if( zIn[0] == '{' ){` |
|         - |  1363 | `					/* Increment nesting level */` |
|         9 |  1364 | `					iNest++;` |
|      1415 |  1365 | `				}else if(zIn[0] == '}' ){` |
|         - |  1366 | `					/* Decrement nesting level */` |
|       149 |  1367 | `					iNest--;` |
|       149 |  1368 | `					if( iNest <= 0 ){` |
|       141 |  1369 | `						break;` |
|         - |  1370 | `					}` |
|         4 |  1371 | `				}` |
|      1281 |  1372 | `				zIn++;` |
|         3 |  1373 | `			}` |
|         - |  1374 | `			/* Process the expression */` |
|       141 |  1375 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|       141 |  1376 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1377 | `				return SXERR_ABORT;` |
|         - |  1378 | `			}` |
|       141 |  1379 | `			if( rc != SXERR_EMPTY ){` |
|       141 |  1380 | `				++iCons;` |
|        69 |  1381 | `			}` |
|       141 |  1382 | `			if( zIn < zEnd ){` |
|         - |  1383 | `				/* Jump the trailing curly */` |
|       141 |  1384 | `				zIn++;` |
|        69 |  1385 | `			}` |
|        72 |  1386 | `		}else{` |
|         - |  1387 | `			/* Simple syntax */` |
|      2449 |  1388 | `			const char *zExpr = zIn;` |
|         - |  1389 | `			/* Assemble variable name */` |
|      1247 |  1390 | `			for(;;){` |
|         - |  1391 | `				/* Jump leading dollars */` |
|      4943 |  1392 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|      2449 |  1393 | `					zIn++;` |
|         5 |  1394 | `				}` |
|      1247 |  1395 | `				for(;;){` |
|     12902 |  1396 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|      9161 |  1397 | `						zIn++;` |
|         5 |  1398 | `					}` |
|      2499 |  1399 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|         - |  1400 | `						/* UTF-8 stream */` |
|       ! 0 |  1401 | `						zIn++;` |
|       ! 0 |  1402 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|       ! 0 |  1403 | `							zIn++;` |
|       ! 0 |  1404 | `						}` |
|       ! 0 |  1405 | `						continue;` |
|         - |  1406 | `					}` |
|      2499 |  1407 | `					break;` |
|       ! 0 |  1408 | `				}` |
|      2499 |  1409 | `				if( zIn >= zEnd ){` |
|       263 |  1410 | `					break;` |
|         - |  1411 | `				}` |
|      2241 |  1412 | `				if( zIn[0] == '[' ){` |
|        12 |  1413 | `					sxi32 iSquare = 1;` |
|        12 |  1414 | `					zIn++;` |
|        28 |  1415 | `					while( zIn < zEnd ){` |
|        28 |  1416 | `						if( zIn[0] == '[' ){` |
|       ! 0 |  1417 | `							iSquare++;` |
|        28 |  1418 | `						}else if (zIn[0] == ']' ){` |
|        12 |  1419 | `							iSquare--;` |
|        12 |  1420 | `							if( iSquare <= 0 ){` |
|        12 |  1421 | `								break;` |
|         - |  1422 | `							}` |
|       ! 0 |  1423 | `						}` |
|        18 |  1424 | `						zIn++;` |
|         2 |  1425 | `					}` |
|        12 |  1426 | `					if( zIn < zEnd ){` |
|        12 |  1427 | `						zIn++;` |
|         5 |  1428 | `					}` |
|        12 |  1429 | `					break;` |
|      2231 |  1430 | `				}else if(zIn[0] == '{' ){` |
|         6 |  1431 | `					sxi32 iCurly = 1;` |
|         6 |  1432 | `					zIn++;` |
|        18 |  1433 | `					while( zIn < zEnd ){` |
|        16 |  1434 | `						if( zIn[0] == '{' ){` |
|       ! 0 |  1435 | `							iCurly++;` |
|        16 |  1436 | `						}else if (zIn[0] == '}' ){` |
|         3 |  1437 | `							iCurly--;` |
|         3 |  1438 | `							if( iCurly <= 0 ){` |
|         3 |  1439 | `								break;` |
|         - |  1440 | `							}` |
|       ! 0 |  1441 | `						}` |
|        14 |  1442 | `						zIn++;` |
|         2 |  1443 | `					}` |
|         6 |  1444 | `					if( zIn < zEnd ){` |
|         3 |  1445 | `						zIn++;` |
|         1 |  1446 | `					}` |
|         6 |  1447 | `					break;` |
|      2227 |  1448 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|         - |  1449 | `					/* Member access operator '->' */` |
|        53 |  1450 | `					zIn += 2;` |
|      2202 |  1451 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|         - |  1452 | `					/* Static member access operator '::' */` |
|       ! 0 |  1453 | `					zIn += 2;` |
|       ! 0 |  1454 | `				}else{` |
|      1091 |  1455 | `					break;` |
|         - |  1456 | `				}` |
|         3 |  1457 | `			}` |
|         - |  1458 | `			/* Process the expression */` |
|      2449 |  1459 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|      2449 |  1460 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1461 | `				return SXERR_ABORT;` |
|         - |  1462 | `			}` |
|      2449 |  1463 | `			if( rc != SXERR_EMPTY ){` |
|      2447 |  1464 | `				++iCons;` |
|      1221 |  1465 | `			}` |
|         - |  1466 | `		}` |
|         - |  1467 | `		/* Invalidate the previously used constant */` |
|      2587 |  1468 | `		pObj = 0;` |
|         5 |  1469 | `	}/*for(;;)*/` |
|     81815 |  1470 | `	if( iCons > 1 ){` |
|         - |  1471 | `		/* Concatenate all compiled constants */` |
|      1875 |  1472 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|       935 |  1473 | `	}` |
|         - |  1474 | `	/* Node successfully compiled */` |
|     81815 |  1475 | `	return SXRET_OK;` |
|     41100 |  1476 | `}` |
|         - |  1477 | `/*` |
|         - |  1478 | ` * Compile a double quoted string.` |
|         - |  1479 | ` *  See the block-comment above for more information.` |
|         - |  1480 | ` */` |
|     82128 |  1481 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1482 | `{` |
|         - |  1483 | `	sxi32 rc;` |
|     82133 |  1484 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|     41064 |  1485 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  1486 | `	/* Compilation result */` |
|     82133 |  1487 | `	return rc;` |
|         5 |  1488 | `}` |
|         - |  1489 | `/*` |
|         - |  1490 | ` * Compile a Heredoc string.` |
|         - |  1491 | ` *  See the block-comment above for more information.` |
|         - |  1492 | ` */` |
|        66 |  1493 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         4 |  1494 | `{` |
|         - |  1495 | `	SyString sOrig, sStripped;` |
|         - |  1496 | `	sxi32 rc;` |
|        70 |  1497 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|        70 |  1498 | `	if( rc != SXRET_OK ){` |
|         6 |  1499 | `		return rc;` |
|         - |  1500 | `	}` |
|         - |  1501 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|         - |  1502 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|         - |  1503 | `	 * Restore before returning so downstream code that references pIn is` |
|         - |  1504 | `	 * unaffected, including on the error path. */` |
|        65 |  1505 | `	sOrig = pGen->pIn->sData;` |
|        65 |  1506 | `	pGen->pIn->sData = sStripped;` |
|        65 |  1507 | `	rc = GenStateCompileString(&(*pGen),1/*bHeredoc*/);` |
|        65 |  1508 | `	pGen->pIn->sData = sOrig;` |
|        31 |  1509 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        65 |  1510 | `	return rc;` |
|        37 |  1511 | `}` |
|         - |  1512 | `/*` |
|         - |  1513 | ` * Compile an array entry whether it is a key or a value.` |
|         - |  1514 | ` *  Notes on array entries.` |
|         - |  1515 | ` *  According to the PHP language reference manual` |
|         - |  1516 | ` *  An array can be created by the array() language construct.` |
|         - |  1517 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|         - |  1518 | ` *  array(  key =>  value` |
|         - |  1519 | ` *    , ...` |
|         - |  1520 | ` *    )` |
|         - |  1521 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|         - |  1522 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|         - |  1523 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|         - |  1524 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|         - |  1525 | ` *  contain integer and string indices.` |
|         - |  1526 | ` *  A value can be any PHP type.` |
|         - |  1527 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|         - |  1528 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|         - |  1529 | ` *  is specified, that value will be overwritten.` |
|         - |  1530 | ` */` |
|   1025852 |  1531 | `static sxi32 GenStateCompileArrayEntry(` |
|         - |  1532 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  1533 | `	SyToken *pIn,        /* Token stream */` |
|         - |  1534 | `	SyToken *pEnd,       /* End of the token stream */` |
|         - |  1535 | `	sxi32 iFlags,        /* Compilation flags */` |
|         - |  1536 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|         - |  1537 | `	)` |
|         5 |  1538 | `{` |
|         - |  1539 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  1540 | `	sxi32 rc;` |
|         - |  1541 | `	/* Swap token stream */` |
|   1025857 |  1542 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|         - |  1543 | `	/* Compile the expression*/` |
|   1025857 |  1544 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|         - |  1545 | `	/* Restore token stream */` |
|   1025857 |  1546 | `	RE_SWAP_DELIMITER(pGen);` |
|   1025857 |  1547 | `	return rc;` |
|         5 |  1548 | `}` |
|         - |  1549 | `/*` |
|         - |  1550 | ` * Expression tree validator callback for the 'array' language construct.` |
|         - |  1551 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|         - |  1552 | ` * an invalid expression tree and this function will generate the appropriate` |
|         - |  1553 | ` * error message.` |
|         - |  1554 | ` * See the routine responible of compiling the array language construct` |
|         - |  1555 | ` * for more inforation.` |
|         - |  1556 | ` */` |
|        36 |  1557 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         4 |  1558 | `{` |
|        40 |  1559 | `	sxi32 rc = SXRET_OK;` |
|        40 |  1560 | `	if( pRoot->pOp ){` |
|        14 |  1561 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|        12 |  1562 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|        16 |  1563 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|         - |  1564 | `			/* Unexpected expression */` |
|        13 |  1565 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  1566 | `				"array(): Expecting a variable/array member/function call after reference operator '&'");` |
|        13 |  1567 | `			if( rc != SXERR_ABORT ){` |
|        13 |  1568 | `				rc = SXERR_INVALID;` |
|         5 |  1569 | `			}` |
|         9 |  1570 | `		}` |
|        31 |  1571 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  1572 | `		/* Unexpected expression */` |
|         3 |  1573 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  1574 | `			"array(): Expecting a variable after reference operator '&'");` |
|         3 |  1575 | `		if( rc != SXERR_ABORT ){` |
|         3 |  1576 | `			rc = SXERR_INVALID;` |
|         1 |  1577 | `		}` |
|         1 |  1578 | `	}` |
|        40 |  1579 | `	return rc;` |
|         4 |  1580 | `}` |
|         - |  1581 | `/*` |
|         - |  1582 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|         - |  1583 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|         - |  1584 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|         - |  1585 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|         - |  1586 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|         - |  1587 | ` */` |
|    965544 |  1588 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|         5 |  1589 | `{` |
|    965549 |  1590 | `	SyToken *pCur = pStart;` |
|    965549 |  1591 | `	sxi32 iNest = 0;` |
|   2662885 |  1592 | `	while( pCur < pEnd ){` |
|   2089513 |  1593 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    392173 |  1594 | `			return pCur;` |
|         - |  1595 | `		}` |
|         - |  1596 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|         - |  1597 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|         - |  1598 | `		 * not an entry separator. Skip past the signature.` |
|         - |  1599 | `		 */` |
|   1697345 |  1600 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|     23735 |  1601 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|     23735 |  1602 | `			SyToken *pFn = pCur;` |
|     23730 |  1603 | `			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd` |
|       ! 0 |  1604 | `				&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|         5 |  1605 | `				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|       ! 0 |  1606 | `				pFn = &pCur[1];` |
|       ! 0 |  1607 | `				nKw = PH7_TKWRD_FN;` |
|       ! 0 |  1608 | `			}` |
|     23735 |  1609 | `			if( nKw == PH7_TKWRD_FN ){` |
|         5 |  1610 | `				pCur = pFn + 1; /* past 'fn' */` |
|         5 |  1611 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  1612 | `					pCur++;` |
|       ! 0 |  1613 | `				}` |
|         5 |  1614 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|         5 |  1615 | `					pCur++;` |
|         5 |  1616 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|         - |  1617 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|         5 |  1618 | `					if( pCur < pEnd ){` |
|         5 |  1619 | `						pCur++;` |
|         2 |  1620 | `					}` |
|         2 |  1621 | `				}` |
|         5 |  1622 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_COLON) ){` |
|       ! 0 |  1623 | `					pCur++;` |
|       ! 0 |  1624 | `					if( pCur < pEnd && (pCur->nType & PH7_TK_OP)` |
|       ! 0 |  1625 | `						&& pCur->sData.nByte == 1` |
|       ! 0 |  1626 | `						&& pCur->sData.zString[0] == '?' ){` |
|       ! 0 |  1627 | `						pCur++;` |
|       ! 0 |  1628 | `					}` |
|       ! 0 |  1629 | `					if( pCur < pEnd` |
|       ! 0 |  1630 | `						&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|       ! 0 |  1631 | `						pCur++;` |
|       ! 0 |  1632 | `					}` |
|       ! 0 |  1633 | `				}` |
|         - |  1634 | `				/* The rest of the entry is the arrow-function body — no outer` |
|         - |  1635 | `				 * key to extract. */` |
|         5 |  1636 | `				return pEnd;` |
|         - |  1637 | `			}` |
|         - |  1638 | `			/* Match expression (PHP 8.0): the '=>' inside match arms is not an` |
|         - |  1639 | `			 * entry separator. Skip past the full match span. */` |
|     23731 |  1640 | `			if( nKw == PH7_TKWRD_MATCH ){` |
|         3 |  1641 | `				pCur++; /* past 'match' */` |
|         3 |  1642 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|         3 |  1643 | `					pCur++;` |
|         3 |  1644 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|         - |  1645 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|         3 |  1646 | `					if( pCur < pEnd ){` |
|         3 |  1647 | `						pCur++;` |
|         1 |  1648 | `					}` |
|         1 |  1649 | `				}` |
|         3 |  1650 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_OCB) ){` |
|         3 |  1651 | `					pCur++;` |
|         3 |  1652 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|         - |  1653 | `						PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|         3 |  1654 | `					if( pCur < pEnd ){` |
|         3 |  1655 | `						pCur++;` |
|         1 |  1656 | `					}` |
|         1 |  1657 | `				}` |
|         3 |  1658 | `				continue;` |
|         - |  1659 | `			}` |
|     11862 |  1660 | `		}` |
|   1697339 |  1661 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     55583 |  1662 | `			iNest++;` |
|   1669550 |  1663 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|         - |  1664 | `			/* Don't worry about mismatched brackets here, the expression` |
|         - |  1665 | `			 * parser will shortly detect any syntax error. */` |
|     55583 |  1666 | `			iNest--;` |
|     27789 |  1667 | `		}` |
|   1697339 |  1668 | `		pCur++;` |
|         5 |  1669 | `	}` |
|    573377 |  1670 | `	return pEnd;` |
|    482777 |  1671 | `}` |
|         - |  1672 | `/*` |
|         - |  1673 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|         - |  1674 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|         - |  1675 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|         - |  1676 | ` */` |
|    515698 |  1677 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|         5 |  1678 | `{` |
|         - |  1679 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|         - |  1680 | `	SyToken *pKey,*pCur;` |
|    515703 |  1681 | `	sxi32 iEmitRef = 0;` |
|    515703 |  1682 | `	sxi32 iSpread = 0;` |
|    515703 |  1683 | `	sxi32 nPair = 0;` |
|         - |  1684 | `	sxi32 rc;` |
|    515703 |  1685 | `	xValidator = 0;` |
|    625927 |  1686 | `	for(;;){` |
|         - |  1687 | `		/* Jump leading commas */` |
|   1764343 |  1688 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    512489 |  1689 | `			pGen->pIn++;` |
|         5 |  1690 | `		}` |
|   1251859 |  1691 | `		pCur = pGen->pIn;` |
|   1251859 |  1692 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|         - |  1693 | `			/* No more entry to process */` |
|    515687 |  1694 | `			break;` |
|         - |  1695 | `		}` |
|    736177 |  1696 | `		if( pCur >= pGen->pIn ){` |
|       ! 0 |  1697 | `			continue;` |
|         - |  1698 | `		}` |
|         - |  1699 | `		/* Compile the key if available */` |
|    736177 |  1700 | `		pKey = pCur;` |
|    736177 |  1701 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|    736177 |  1702 | `		rc = SXERR_EMPTY;` |
|    736177 |  1703 | `		if( pCur < pGen->pIn ){` |
|    289431 |  1704 | `			if( &pCur[1] >= pGen->pIn ){` |
|         - |  1705 | `				/* Missing value */` |
|        12 |  1706 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|        12 |  1707 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  1708 | `					return SXERR_ABORT;` |
|         - |  1709 | `				}` |
|        12 |  1710 | `				return SXRET_OK;` |
|         - |  1711 | `			}` |
|         - |  1712 | `			/* Compile the expression holding the key */` |
|    289421 |  1713 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|         - |  1714 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    289421 |  1715 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1716 | `				return SXERR_ABORT;` |
|         - |  1717 | `			}` |
|    289421 |  1718 | `			pCur++; /* Jump the '=>' operator */` |
|    591459 |  1719 | `		}else if( pKey == pCur ){` |
|         - |  1720 | `			/* Key is omitted,emit a warning */` |
|       ! 0 |  1721 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|       ! 0 |  1722 | `			pCur++; /* Jump the '=>' operator */` |
|       ! 0 |  1723 | `		}else{` |
|         - |  1724 | `			/* Reset back the cursor and point to the entry value */` |
|    446751 |  1725 | `			pCur = pKey;` |
|         - |  1726 | `		}` |
|    736167 |  1727 | `		if( rc == SXERR_EMPTY ){` |
|         - |  1728 | `			/* No available key,load NULL */` |
|    446753 |  1729 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    223374 |  1730 | `		}` |
|    736167 |  1731 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|         - |  1732 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|        44 |  1733 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|        44 |  1734 | `			iEmitRef = 1;` |
|        44 |  1735 | `			pCur++; /* Jump the '&' token */` |
|        44 |  1736 | `			if( pCur >= pGen->pIn ){` |
|         - |  1737 | `				/* Missing value */` |
|         3 |  1738 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|         3 |  1739 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  1740 | `					return SXERR_ABORT;` |
|         - |  1741 | `				}` |
|         3 |  1742 | `				return SXRET_OK;` |
|         - |  1743 | `			}` |
|        19 |  1744 | `		}` |
|         - |  1745 | `		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with` |
|         - |  1746 | `		 * string-key support since PHP 8.1). The parser strips the '...' inside` |
|         - |  1747 | `		 * ExprExtractNode; we only need to know it's there so we can emit` |
|         - |  1748 | `		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the` |
|         - |  1749 | `		 * resulting hashmap rather than insert it as a scalar entry. */` |
|    736165 |  1750 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|    736165 |  1751 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
|         - |  1752 | `			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the` |
|         - |  1753 | `			 * '...' token cannot follow either '=>' or '&' inside an array` |
|         - |  1754 | `			 * literal. Emit the same Parse-error wording PHP uses so the` |
|         - |  1755 | `			 * output is engine-portable. */` |
|         6 |  1756 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|         - |  1757 | `				"syntax error, unexpected token \"...\"");` |
|         6 |  1758 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  1759 | `				return SXERR_ABORT;` |
|         - |  1760 | `			}` |
|         6 |  1761 | `			return SXRET_OK;` |
|         - |  1762 | `		}` |
|         - |  1763 | `		/* Compile indice value */` |
|    736161 |  1764 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|    736161 |  1765 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  1766 | `			return SXERR_ABORT;` |
|         - |  1767 | `		}` |
|    736161 |  1768 | `		if( iSpread ){` |
|         - |  1769 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|        69 |  1770 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|    736128 |  1771 | `		}else if( iEmitRef ){` |
|         - |  1772 | `			/* Emit the load reference instruction */` |
|        40 |  1773 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|        18 |  1774 | `		}` |
|    736161 |  1775 | `		xValidator = 0;` |
|    736161 |  1776 | `		iEmitRef = 0;` |
|    736161 |  1777 | `		iSpread = 0;` |
|    736161 |  1778 | `		nPair++;` |
|         5 |  1779 | `	}` |
|         - |  1780 | `	/* Emit the load map instruction */` |
|    515687 |  1781 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|         - |  1782 | `	/* Node successfully compiled */` |
|    515687 |  1783 | `	return SXRET_OK;` |
|    257854 |  1784 | `}` |
|         - |  1785 | `/*` |
|         - |  1786 | ` * Compile the 'array' language construct.` |
|         - |  1787 | ` *	 According to the PHP language reference manual` |
|         - |  1788 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|         - |  1789 | ` *   values to keys. This type is optimized for several different uses; it can` |
|         - |  1790 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|         - |  1791 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|         - |  1792 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|         - |  1793 | ` */` |
|    293268 |  1794 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1795 | `{` |
|         - |  1796 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|    293273 |  1797 | `	pGen->pIn += 2;` |
|    293273 |  1798 | `	pGen->pEnd--;` |
|    146634 |  1799 | `	SXUNUSED(iCompileFlag);` |
|    293273 |  1800 | `	return GenStateCompileArrayBody(pGen);` |
|         5 |  1801 | `}` |
|         - |  1802 | `/*` |
|         - |  1803 | ` * Compile the PHP 8.5 clone(...) call form:` |
|         - |  1804 | `` *   clone($object)                          -> identical to the `clone $object` operator`` |
|         - |  1805 | ` *   clone($object, ['prop' => value, ...])  -> clone, run __clone(), then apply the` |
|         - |  1806 | ` *                                              property updates as scope-aware writes` |
|         - |  1807 | ` *   clone(object: $o, withProperties: [..]) -> the named-argument spelling` |
|         - |  1808 | ` * Codegen: compile the object argument and emit OP_CLONE (which clones and runs` |
|         - |  1809 | ` * __clone()); if a withProperties argument is present, compile it and emit` |
|         - |  1810 | ` * OP_CLONE_APPLY, which applies each update to the fresh clone AFTER __clone(),` |
|         - |  1811 | ` * honouring visibility / readonly-set-scope / typed-property enforcement in the` |
|         - |  1812 | ` * calling scope. The parser (ExprExtractNode) delimited this node's tokens as` |
|         - |  1813 | `` * `clone ( ... )`; pGen->pIn/pEnd point at the first/one-past-last of that range.`` |
|         - |  1814 | ` */` |
|        22 |  1815 | `PH7_PRIVATE sxi32 PH7_CompileCloneCall(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         2 |  1816 | `{` |
|         - |  1817 | `	SyToken *pIn,*pEnd,*pNext;` |
|        24 |  1818 | `	SyToken *pObjStart = 0,*pObjEnd = 0;` |
|        24 |  1819 | `	SyToken *pUpdStart = 0,*pUpdEnd = 0;` |
|        24 |  1820 | `	int nArg = 0;` |
|         - |  1821 | `	sxi32 rc;` |
|        11 |  1822 | `	SXUNUSED(iCompileFlag);` |
|         - |  1823 | `	/* pGen->pIn -> 'clone', pGen->pIn[1] -> '(', pGen->pEnd -> one past ')'. */` |
|        24 |  1824 | `	pIn  = pGen->pIn + 2;   /* skip 'clone' and the opening '(' */` |
|        24 |  1825 | `	pEnd = pGen->pEnd - 1;  /* exclude the closing ')' */` |
|         - |  1826 | `	/* clone(...) first-class-callable form: a lone ellipsis is the whole list. */` |
|        24 |  1827 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       ! 0 |  1828 | `		return PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  1829 | `			"clone(...) first-class callable form is not yet supported");` |
|         - |  1830 | `	}` |
|         - |  1831 | `	/* Split the (at most two) comma-separated arguments, tolerating named labels. */` |
|        62 |  1832 | `	while( pIn < pEnd ){` |
|        40 |  1833 | `		SyToken *pArgStart,*pArgEnd,*pName = 0;` |
|        40 |  1834 | `		if( PH7_GetNextExpr(pIn,pEnd,&pNext) != SXRET_OK ){` |
|       ! 0 |  1835 | `			break;` |
|         - |  1836 | `		}` |
|        40 |  1837 | `		pArgStart = pIn;` |
|        40 |  1838 | `		pArgEnd   = pNext;` |
|         - |  1839 | `		/* Named-argument label: <ID\|keyword> ':' expr. A single ':' is PH7_TK_COLON;` |
|         - |  1840 | ``		 * '::' is a distinct operator token, so this never mis-fires on `A::B`. */`` |
|        38 |  1841 | `		if( (pArgEnd - pArgStart) >= 2` |
|        37 |  1842 | `			&& (pArgStart[0].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        23 |  1843 | `			&& (pArgStart[1].nType & PH7_TK_COLON) ){` |
|         5 |  1844 | `			pName = pArgStart;` |
|         5 |  1845 | `			pArgStart += 2;` |
|         2 |  1846 | `		}` |
|        40 |  1847 | `		if( pName ){` |
|         - |  1848 | `` 			/* PHP named parameters are case-SENSITIVE, so `Object:`/`WITHPROPERTIES:` `` |
|         - |  1849 | `			 * must be rejected as unknown (SyMemcmp, not SyStrnicmp). */` |
|         4 |  1850 | `			if( pName->sData.nByte == sizeof("object")-1` |
|         4 |  1851 | `				&& SyMemcmp(pName->sData.zString,"object",sizeof("object")-1) == 0 ){` |
|         3 |  1852 | `				pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|         4 |  1853 | `			}else if( pName->sData.nByte == sizeof("withProperties")-1` |
|         3 |  1854 | `				&& SyMemcmp(pName->sData.zString,"withProperties",sizeof("withProperties")-1) == 0 ){` |
|         3 |  1855 | `				pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|         2 |  1856 | `			}else{` |
|       ! 0 |  1857 | `				return PH7_GenCompileError(pGen,E_ERROR,pName->nLine,` |
|       ! 0 |  1858 | `					"Unknown named parameter $%z",&pName->sData);` |
|         1 |  1859 | `			}` |
|        38 |  1860 | `		}else if( nArg == 0 ){` |
|        22 |  1861 | `			pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|        25 |  1862 | `		}else if( nArg == 1 ){` |
|        15 |  1863 | `			pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|         8 |  1864 | `		}else{` |
|       ! 0 |  1865 | `			return PH7_GenCompileError(pGen,E_ERROR,pArgStart->nLine,` |
|         - |  1866 | `				"clone() expects at most 2 arguments");` |
|         - |  1867 | `		}` |
|        40 |  1868 | `		nArg++;` |
|        40 |  1869 | `		pIn = pNext;` |
|        40 |  1870 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 |  1871 | `			pIn++; /* step over the argument separator */` |
|         8 |  1872 | `		}` |
|         2 |  1873 | `	}` |
|        24 |  1874 | `	if( pObjStart == 0 \|\| pObjStart >= pObjEnd ){` |
|       ! 0 |  1875 | `		return PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  1876 | `			"clone() expects at least 1 argument, 0 given");` |
|         - |  1877 | `	}` |
|         - |  1878 | `	/* Object argument -> clone (+ __clone()). */` |
|        24 |  1879 | `	rc = GenStateCompileArrayEntry(pGen,pObjStart,pObjEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|        24 |  1880 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  1881 | `		return SXERR_ABORT;` |
|         - |  1882 | `	}` |
|        24 |  1883 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE,0,0,0,0);` |
|         - |  1884 | `	/* Property updates (evaluated after __clone runs). */` |
|        24 |  1885 | `	if( pUpdStart && pUpdStart < pUpdEnd ){` |
|        17 |  1886 | `		rc = GenStateCompileArrayEntry(pGen,pUpdStart,pUpdEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|        17 |  1887 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  1888 | `			return SXERR_ABORT;` |
|         - |  1889 | `		}` |
|        17 |  1890 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE_APPLY,0,0,0,0);` |
|         8 |  1891 | `	}` |
|        24 |  1892 | `	return SXRET_OK;` |
|        13 |  1893 | `}` |
|         - |  1894 | `/*` |
|         - |  1895 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|         - |  1896 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|         - |  1897 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|         - |  1898 | ` */` |
|    222430 |  1899 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  1900 | `{` |
|         - |  1901 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|    222435 |  1902 | `	pGen->pIn++;` |
|    222435 |  1903 | `	pGen->pEnd--;` |
|    111215 |  1904 | `	SXUNUSED(iCompileFlag);` |
|    222435 |  1905 | `	return GenStateCompileArrayBody(pGen);` |
|         5 |  1906 | `}` |
|         - |  1907 | `/*` |
|         - |  1908 | ` * Expression tree validator callback for the 'list' language construct.` |
|         - |  1909 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|         - |  1910 | ` * an invalid expression tree and this function will generate the appropriate` |
|         - |  1911 | ` * error message.` |
|         - |  1912 | ` * See the routine responible of compiling the list language construct` |
|         - |  1913 | ` * for more inforation.` |
|         - |  1914 | ` */` |
|       206 |  1915 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 |  1916 | `{` |
|       211 |  1917 | `	sxi32 rc = SXRET_OK;` |
|       211 |  1918 | `	if( pRoot->pOp ){` |
|         4 |  1919 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|         2 |  1920 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|         - |  1921 | `				/* Unexpected expression */` |
|       ! 0 |  1922 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  1923 | `					"list(): Expecting a variable not an expression");` |
|       ! 0 |  1924 | `				if( rc != SXERR_ABORT ){` |
|       ! 0 |  1925 | `					rc = SXERR_INVALID;` |
|       ! 0 |  1926 | `				}` |
|         1 |  1927 | `		}` |
|       209 |  1928 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  1929 | `		/* Unexpected expression */` |
|         6 |  1930 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  1931 | `			"list(): Expecting a variable not an expression");` |
|         6 |  1932 | `		if( rc != SXERR_ABORT ){` |
|         6 |  1933 | `			rc = SXERR_INVALID;` |
|         2 |  1934 | `		}` |
|         2 |  1935 | `	}` |
|       211 |  1936 | `	return rc;` |
|         5 |  1937 | `}` |
|         - |  1938 | `/*` |
|         - |  1939 | ` * Compile the 'list' language construct.` |
|         - |  1940 | ` *  According to the PHP language reference` |
|         - |  1941 | ` *  list(): Assign variables as if they were an array.` |
|         - |  1942 | ` *  list() is used to assign a list of variables in one operation.` |
|         - |  1943 | ` *  Description` |
|         - |  1944 | ` *   array list (mixed $varname [, mixed $... ] )` |
|         - |  1945 | ` *   Like array(), this is not really a function, but a language construct.` |
|         - |  1946 | ` *   list() is used to assign a list of variables in one operation.` |
|         - |  1947 | ` *  Parameters` |
|         - |  1948 | ` *   $varname: A variable.` |
|         - |  1949 | ` *  Return Values` |
|         - |  1950 | ` *   The assigned array.` |
|         - |  1951 | ` */` |
|         - |  1952 | `/* Nested list entry recorded during first pass of list body compilation */` |
|         - |  1953 | `struct NestedListEntry {` |
|         - |  1954 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|         - |  1955 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|         - |  1956 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|         - |  1957 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|         - |  1958 | `};` |
|         - |  1959 | `/*` |
|         - |  1960 | ` * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where` |
|         - |  1961 | `` * every entry has the form `keyExpr => target`. The source array is on the stack`` |
|         - |  1962 | ` * top on entry and remains there on exit, mirroring the positional LOAD_LIST` |
|         - |  1963 | ` * path so the caller's teardown is unchanged. For each entry: DUP the source,` |
|         - |  1964 | ` * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,` |
|         - |  1965 | ` * like a normal subscript read), then assign the fetched value to the target — a` |
|         - |  1966 | ` * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a` |
|         - |  1967 | ` * normal assignment (the value sits below the lvalue-load, exactly as in` |
|         - |  1968 | ` * GenStateEmitExprCode where the assignment RHS precedes the LHS load).` |
|         - |  1969 | ` */` |
|        22 |  1970 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|         1 |  1971 | `{` |
|         - |  1972 | `	SyToken *pNext;` |
|         - |  1973 | `	sxi32 rc;` |
|        53 |  1974 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|         - |  1975 | `		SyToken *pArrow,*pTarget;` |
|         - |  1976 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|        31 |  1977 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|        31 |  1978 | `		pTarget = &pArrow[1];` |
|        31 |  1979 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|         - |  1980 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|         - |  1981 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|       ! 0 |  1982 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  1983 | `				"Cannot use empty array entries in keyed array assignment");` |
|       ! 0 |  1984 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  1985 | `		}` |
|         - |  1986 | `		/* DUP the source array (it is on the stack top) */` |
|        31 |  1987 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|         - |  1988 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|        31 |  1989 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|        31 |  1990 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  1991 | `			return SXERR_ABORT;` |
|         - |  1992 | `		}` |
|         - |  1993 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|         - |  1994 | `		 * iP2=7 is the keyed-destructuring read context: an array source reads like` |
|         - |  1995 | ``		 * iP2=0 (missing key loads NULL silently, matching a normal `$arr[$k]` read;`` |
|         - |  1996 | `		 * PHP also emits an "Undefined array key" warning here, PHL omits it — §3.7),` |
|         - |  1997 | `		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"` |
|         - |  1998 | `		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */` |
|        31 |  1999 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);` |
|        31 |  2000 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|        28 |  2001 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|        15 |  2002 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|         - |  2003 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|         - |  2004 | `			 * Treat source[key] as the inner body's source, then drop the` |
|         - |  2005 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|         5 |  2006 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|         5 |  2007 | `			SyToken *pSavedIn = pGen->pIn;` |
|         5 |  2008 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|         5 |  2009 | `			pGen->pIn = pTarget;` |
|         5 |  2010 | `			pGen->pEnd = pNext;` |
|         5 |  2011 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|         2 |  2012 | `			             : PH7_CompileList(&(*pGen),0);` |
|         5 |  2013 | `			pGen->pIn = pSavedIn;` |
|         5 |  2014 | `			pGen->pEnd = pSavedEnd;` |
|         5 |  2015 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2016 | `				return SXERR_ABORT;` |
|         - |  2017 | `			}` |
|         5 |  2018 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         3 |  2019 | `		}else{` |
|         - |  2020 | `			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]` |
|         - |  2021 | `			 * is already on the stack as the value; compiling the target appends` |
|         - |  2022 | `			 * its lvalue-load, which we fold into a STORE just as a normal` |
|         - |  2023 | `			 * assignment does. */` |
|         - |  2024 | `			VmInstr *pInstr;` |
|        27 |  2025 | `			sxi32 iVmOp = PH7_OP_STORE;` |
|        27 |  2026 | `			sxi32 iP1 = 0, iP2 = 0;` |
|        27 |  2027 | `			void *p3 = 0;` |
|        27 |  2028 | `			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,` |
|         - |  2029 | `				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|        27 |  2030 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  2031 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2032 | `			}` |
|        27 |  2033 | `			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|        27 |  2034 | `				if( pInstr->iOp == PH7_OP_MEMBER ){` |
|         3 |  2035 | `					iP2 = 1; /* member store: keep MEMBER, store value below it */` |
|        26 |  2036 | `				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         3 |  2037 | `					iVmOp = PH7_OP_STORE_IDX;` |
|         3 |  2038 | `					iP1 = pInstr->iP1;` |
|         3 |  2039 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         2 |  2040 | `				}else{` |
|        23 |  2041 | `					p3 = pInstr->p3; /* named store: $v = value */` |
|        23 |  2042 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - |  2043 | `				}` |
|        13 |  2044 | `			}` |
|        27 |  2045 | `			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|         - |  2046 | `			/* STORE leaves the assigned value on the stack top; drop it so the` |
|         - |  2047 | `			 * source array is back on top for the next entry. */` |
|        27 |  2048 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         - |  2049 | `		}` |
|        31 |  2050 | `		pGen->pIn = &pNext[1];` |
|         1 |  2051 | `	}` |
|        23 |  2052 | `	return SXRET_OK;` |
|        12 |  2053 | `}` |
|         - |  2054 | `/*` |
|         - |  2055 | ` * Shared body for list() and short list [...] compilation.` |
|         - |  2056 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|         - |  2057 | ` * the opening delimiter and before the closing delimiter.` |
|         - |  2058 | ` */` |
|       120 |  2059 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|         5 |  2060 | `{` |
|         - |  2061 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|         - |  2062 | `	SyToken *pNext;` |
|         - |  2063 | `	SyToken *pClassifyIn;` |
|       125 |  2064 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|         - |  2065 | `	sxi32 nExpr;` |
|         - |  2066 | `	sxi32 rc;` |
|         - |  2067 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|         - |  2068 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|         - |  2069 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|         - |  2070 | `	 * list. */` |
|       125 |  2071 | `	pClassifyIn = pGen->pIn;` |
|       361 |  2072 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       241 |  2073 | `		if( pGen->pIn >= pNext ){` |
|        13 |  2074 | `			nEmpty++;` |
|       235 |  2075 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|        31 |  2076 | `			nKeyed++;` |
|        16 |  2077 | `		}else{` |
|       199 |  2078 | `			nPositional++;` |
|         - |  2079 | `		}` |
|       241 |  2080 | `		pGen->pIn = &pNext[1];` |
|         5 |  2081 | `	}` |
|       125 |  2082 | `	pGen->pIn = pClassifyIn;` |
|       125 |  2083 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|       ! 0 |  2084 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  2085 | `			"Cannot use empty array entries in keyed array assignment");` |
|       ! 0 |  2086 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2087 | `	}` |
|       125 |  2088 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|       ! 0 |  2089 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  2090 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|       ! 0 |  2091 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  2092 | `	}` |
|       125 |  2093 | `	if( nKeyed > 0 ){` |
|        23 |  2094 | `		return GenStateCompileKeyedListBody(pGen);` |
|         - |  2095 | `	}` |
|       103 |  2096 | `	nExpr = 0;` |
|       103 |  2097 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|       309 |  2098 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       211 |  2099 | `		if( pGen->pIn < pNext ){` |
|         - |  2100 | `			/* Check for nested list() */` |
|       199 |  2101 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         3 |  2102 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|         - |  2103 | `				/* Record this nested list for post-processing */` |
|         3 |  2104 | `				SyToken *pListEnd = 0;` |
|         3 |  2105 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|         3 |  2106 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|         1 |  2107 | `				}` |
|         3 |  2108 | `				if( pListEnd ){` |
|         - |  2109 | `					struct NestedListEntry sEntry;` |
|         3 |  2110 | `					sEntry.nIndex = nExpr;` |
|         3 |  2111 | `					sEntry.pStart = pGen->pIn;` |
|         3 |  2112 | `					sEntry.pEnd = pListEnd + 1;` |
|         3 |  2113 | `					sEntry.isShort = 0;` |
|         3 |  2114 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|         1 |  2115 | `				}` |
|         - |  2116 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|         3 |  2117 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       198 |  2118 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|         - |  2119 | `				/* Nested short destructuring [...] */` |
|        13 |  2120 | `				SyToken *pBracketEnd = 0;` |
|        13 |  2121 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|        13 |  2122 | `				if( pBracketEnd ){` |
|         - |  2123 | `					struct NestedListEntry sEntry;` |
|        13 |  2124 | `					sEntry.nIndex = nExpr;` |
|        13 |  2125 | `					sEntry.pStart = pGen->pIn;` |
|        13 |  2126 | `					sEntry.pEnd = pBracketEnd + 1;` |
|        13 |  2127 | `					sEntry.isShort = 1;` |
|        13 |  2128 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|         6 |  2129 | `				}` |
|         - |  2130 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|        13 |  2131 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         7 |  2132 | `			}else{` |
|         - |  2133 | `				/* Compile the expression holding the variable */` |
|       185 |  2134 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|       185 |  2135 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  2136 | `					SySetRelease(&sNested);` |
|       ! 0 |  2137 | `					return SXRET_OK;` |
|         - |  2138 | `				}` |
|         - |  2139 | `			}` |
|       102 |  2140 | `		}else{` |
|         - |  2141 | `			/* Empty entry,load NULL */` |
|        13 |  2142 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|         - |  2143 | `		}` |
|       211 |  2144 | `		nExpr++;` |
|         - |  2145 | `		/* Advance the stream cursor */` |
|       211 |  2146 | `		pGen->pIn = &pNext[1];` |
|         5 |  2147 | `	}` |
|         - |  2148 | `	/* Emit the LOAD_LIST instruction */` |
|       103 |  2149 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|         - |  2150 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|         - |  2151 | `	 * For each nested entry, emit code to extract the sub-array` |
|         - |  2152 | `	 * at the corresponding index and recursively destructure it.` |
|         - |  2153 | `	 */` |
|       103 |  2154 | `	if( SySetUsed(&sNested) > 0 ){` |
|        13 |  2155 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|         - |  2156 | `		sxu32 i;` |
|        27 |  2157 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|        15 |  2158 | `			SyToken *pSavedIn = pGen->pIn;` |
|        15 |  2159 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|         - |  2160 | `			ph7_value *pIdx;` |
|         - |  2161 | `			sxu32 nConstIdx;` |
|         - |  2162 | `			/* DUP the source array (it's on stack top) */` |
|        15 |  2163 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|         - |  2164 | `			/* Push the integer index for this nested entry */` |
|        15 |  2165 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|        15 |  2166 | `			if( pIdx == 0 ){` |
|       ! 0 |  2167 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2168 | `				SySetRelease(&sNested);` |
|       ! 0 |  2169 | `				return SXERR_ABORT;` |
|         - |  2170 | `			}` |
|        15 |  2171 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|        15 |  2172 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|         - |  2173 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|         - |  2174 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|         - |  2175 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|         - |  2176 | `			 */` |
|        15 |  2177 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|         - |  2178 | `			/* Recursively compile the inner list */` |
|        15 |  2179 | `			pGen->pIn = apNested[i].pStart;` |
|        15 |  2180 | `			pGen->pEnd = apNested[i].pEnd;` |
|        15 |  2181 | `			if( apNested[i].isShort ){` |
|        13 |  2182 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|         7 |  2183 | `			}else{` |
|         3 |  2184 | `				rc = PH7_CompileList(&(*pGen),0);` |
|         - |  2185 | `			}` |
|        15 |  2186 | `			pGen->pIn = pSavedIn;` |
|        15 |  2187 | `			pGen->pEnd = pSavedEnd;` |
|        15 |  2188 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2189 | `				SySetRelease(&sNested);` |
|       ! 0 |  2190 | `				return SXERR_ABORT;` |
|         - |  2191 | `			}` |
|         - |  2192 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|        15 |  2193 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         8 |  2194 | `		}` |
|         6 |  2195 | `	}` |
|       103 |  2196 | `	SySetRelease(&sNested);` |
|         - |  2197 | `	/* Node successfully compiled */` |
|       103 |  2198 | `	return SXRET_OK;` |
|        65 |  2199 | `}` |
|        38 |  2200 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2201 | `{` |
|         - |  2202 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|        43 |  2203 | `	pGen->pIn += 2;` |
|        43 |  2204 | `	pGen->pEnd--;` |
|        19 |  2205 | `	SXUNUSED(iCompileFlag);` |
|        43 |  2206 | `	return GenStateCompileListBody(pGen);` |
|         5 |  2207 | `}` |
|        82 |  2208 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         2 |  2209 | `{` |
|         - |  2210 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|        84 |  2211 | `	pGen->pIn++;` |
|        84 |  2212 | `	pGen->pEnd--;` |
|        41 |  2213 | `	SXUNUSED(iCompileFlag);` |
|        84 |  2214 | `	return GenStateCompileListBody(pGen);` |
|         2 |  2215 | `}` |
|         - |  2216 | `/* Forward declarations */` |
|         - |  2217 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|         - |  2218 | `static int GenStateIsReservedConstant(SyString *pName);` |
|         - |  2219 | `static int GenStateIsReadonly(SyToken *pTok);` |
|         - |  2220 | `static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok);` |
|         - |  2221 | `static sxi32 GenStateSetVisFlag(sxi32 nKw);` |
|         - |  2222 | `static sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr);` |
|         - |  2223 | `static sxi32 GenStateValidateMemberType(ph7_gen_state *pGen,ph7_class *pClass,const SyString *pMemberName,` |
|         - |  2224 | `	sxu32 nType,const SyString *pTypeClass,const SyString *pTypeText,SySet *pUnionAlts,const char *zErrFmt,sxu32 nLine);` |
|         - |  2225 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|         - |  2226 | `/*` |
|         - |  2227 | ` * Compile an annoynmous function or a closure.` |
|         - |  2228 | ` * According to the PHP language reference` |
|         - |  2229 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|         - |  2230 | ` *  which have no specified name. They are most useful as the value of callback` |
|         - |  2231 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|         - |  2232 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|         - |  2233 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|         - |  2234 | ` *  Example Anonymous function variable assignment example` |
|         - |  2235 | ` * <?php` |
|         - |  2236 | ` * $greet = function($name)` |
|         - |  2237 | ` * {` |
|         - |  2238 | ` *    printf("Hello %s\r\n", $name);` |
|         - |  2239 | ` * };` |
|         - |  2240 | ` * $greet('World');` |
|         - |  2241 | ` * $greet('PHP');` |
|         - |  2242 | ` * ?>` |
|         - |  2243 | ` * Note that the implementation of annoynmous function and closure under` |
|         - |  2244 | ` * PH7 is completely different from the one used by the zend engine.` |
|         - |  2245 | ` */` |
|       466 |  2246 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2247 | `{` |
|       471 |  2248 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|         - |  2249 | `	char zName[512];         /* Unique lambda name */` |
|         - |  2250 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|         - |  2251 | `							  * one thread is allowed to compile the script.` |
|         - |  2252 | `						      */` |
|         - |  2253 | `	SyString sName;` |
|       471 |  2254 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `$f = #[A] function…` trivia`` |
|         - |  2255 | `	                              * is keyed to this ['static'] 'function' token */` |
|         - |  2256 | `	sxu32 nKwLine;` |
|       471 |  2257 | `	sxi32 iFlags = 0;` |
|         - |  2258 | `	sxu32 nLen;` |
|         - |  2259 | `	sxi32 rc;` |
|       233 |  2260 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  2261 |  |
|       471 |  2262 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|       466 |  2263 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       471 |  2264 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - |  2265 | `		/* Static closure: no $this auto-capture, bind refused */` |
|         9 |  2266 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|         9 |  2267 | `		pGen->pIn++; /* Jump the 'static' keyword */` |
|         4 |  2268 | `	}` |
|       471 |  2269 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|       471 |  2270 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|       ! 0 |  2271 | `		pGen->pIn++;` |
|       ! 0 |  2272 | `	}` |
|         - |  2273 | `	/* Generate a unique name */` |
|       471 |  2274 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|         - |  2275 | `	/* Make sure the generated name is unique */` |
|       471 |  2276 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 |  2277 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       ! 0 |  2278 | `	}` |
|       471 |  2279 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|         - |  2280 | `	/* Compile the lambda body */` |
|       471 |  2281 | `	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,&pAnnonFunc);` |
|       471 |  2282 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2283 | `		return SXERR_ABORT;` |
|         - |  2284 | `	}` |
|       471 |  2285 | `	if( pAnnonFunc ){` |
|       471 |  2286 | `		pAnnonFunc->nLine = nKwLine;` |
|         - |  2287 | ``		/* Expression-position attributes (`$f = #[A] function () {}`): the trivia`` |
|         - |  2288 | `		 * sidecar keys them to the closure's first keyword token. */` |
|       471 |  2289 | `		if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnnonFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  2290 | `			return SXERR_ABORT;` |
|         - |  2291 | `		}` |
|       233 |  2292 | `	}` |
|         - |  2293 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|         - |  2294 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|         - |  2295 | `	 * the handler wraps either in a Closure instance. */` |
|       471 |  2296 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|         - |  2297 | `	/* Node successfully compiled */` |
|       471 |  2298 | `	return SXRET_OK;` |
|       238 |  2299 | `}` |
|         - |  2300 | `/*` |
|         - |  2301 | ` * Add a free variable to the arrow function's closure environment, unless` |
|         - |  2302 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|         - |  2303 | ` * enclosing arrow level, or has already been captured.` |
|         - |  2304 | ` */` |
|       204 |  2305 | `static sxi32 GenStateArrowAddCapture(` |
|         - |  2306 | `	ph7_gen_state *pGen,` |
|         - |  2307 | `	ph7_vm_func *pFunc,` |
|         - |  2308 | `	const char *zName,` |
|         - |  2309 | `	sxu32 nByte,` |
|         - |  2310 | `	SyString *aShadow,` |
|         - |  2311 | `	sxu32 nShadow)` |
|         3 |  2312 | `{` |
|         - |  2313 | `	ph7_vm_func_closure_env sEnv;` |
|         - |  2314 | `	ph7_vm_func_closure_env *aEnv;` |
|         - |  2315 | `	sxu32 n, nEnv;` |
|         - |  2316 | `	char *zDup;` |
|       207 |  2317 | `	if( nByte == 0 ){` |
|       ! 0 |  2318 | `		return SXRET_OK;` |
|         - |  2319 | `	}` |
|       204 |  2320 | `	if( nByte == sizeof("this")-1` |
|       111 |  2321 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|         3 |  2322 | `		return SXRET_OK;` |
|         - |  2323 | `	}` |
|       257 |  2324 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|       192 |  2325 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|       186 |  2326 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|       143 |  2327 | `			return SXRET_OK;` |
|         - |  2328 | `		}` |
|        28 |  2329 | `	}` |
|        63 |  2330 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        63 |  2331 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|        91 |  2332 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|        30 |  2333 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|        29 |  2334 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|         3 |  2335 | `			return SXRET_OK;` |
|         - |  2336 | `		}` |
|        15 |  2337 | `	}` |
|        61 |  2338 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|        61 |  2339 | `	if( zDup == 0 ){` |
|       ! 0 |  2340 | `		return SXERR_ABORT;` |
|         - |  2341 | `	}` |
|        61 |  2342 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|        61 |  2343 | `	sEnv.iFlags = 0;` |
|        61 |  2344 | `	sEnv.nIdx = SXU32_HIGH;` |
|        61 |  2345 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|        61 |  2346 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|        61 |  2347 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|        61 |  2348 | `	return SXRET_OK;` |
|       105 |  2349 | `}` |
|         - |  2350 | `/*` |
|         - |  2351 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|         - |  2352 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|         - |  2353 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|         - |  2354 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|         - |  2355 | ` */` |
|        56 |  2356 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|         - |  2357 | `	ph7_gen_state *pGen,` |
|         - |  2358 | `	ph7_vm_func *pFunc,` |
|         - |  2359 | `	const char *zIn,` |
|         - |  2360 | `	const char *zEnd,` |
|         - |  2361 | `	SyString *aShadow,` |
|         - |  2362 | `	sxu32 nShadow)` |
|         2 |  2363 | `{` |
|         - |  2364 | `	sxi32 rc;` |
|       370 |  2365 | `	while( zIn < zEnd ){` |
|       314 |  2366 | `		if( zIn[0] == '\\' ){` |
|         5 |  2367 | `			zIn++;` |
|         5 |  2368 | `			if( zIn < zEnd ){` |
|         5 |  2369 | `				zIn++;` |
|         2 |  2370 | `			}` |
|         5 |  2371 | `			continue;` |
|         - |  2372 | `		}` |
|       308 |  2373 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|        26 |  2374 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|        24 |  2375 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|         - |  2376 | `			const char *zName;` |
|        26 |  2377 | `			zIn++; /* skip '$' */` |
|        26 |  2378 | `			zName = zIn;` |
|        82 |  2379 | `			while( zIn < zEnd ){` |
|        76 |  2380 | `				unsigned char c = (unsigned char)zIn[0];` |
|        76 |  2381 | `				if( c >= 0xc0 ){` |
|       ! 0 |  2382 | `					zIn++;` |
|       ! 0 |  2383 | `					while( zIn < zEnd` |
|       ! 0 |  2384 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|       ! 0 |  2385 | `						zIn++;` |
|       ! 0 |  2386 | `					}` |
|       ! 0 |  2387 | `					continue;` |
|         - |  2388 | `				}` |
|        76 |  2389 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|        20 |  2390 | `					break;` |
|         - |  2391 | `				}` |
|        58 |  2392 | `				zIn++;` |
|         2 |  2393 | `			}` |
|        26 |  2394 | `			if( zIn > zName ){` |
|        38 |  2395 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|        24 |  2396 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|        26 |  2397 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  2398 | `					return SXERR_ABORT;` |
|         - |  2399 | `				}` |
|        12 |  2400 | `			}` |
|        26 |  2401 | `			continue;` |
|         - |  2402 | `		}` |
|       286 |  2403 | `		zIn++;` |
|         2 |  2404 | `	}` |
|        58 |  2405 | `	return SXRET_OK;` |
|        30 |  2406 | `}` |
|         - |  2407 | `/*` |
|         - |  2408 | ` * Scan the body token range of an arrow function for free-variable` |
|         - |  2409 | ` * references and record them in pFunc's closure environment. Handles:` |
|         - |  2410 | ` *   - plain $<id> pairs` |
|         - |  2411 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|         - |  2412 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|         - |  2413 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|         - |  2414 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|         - |  2415 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|         - |  2416 | ` *     are never mistakenly captured.` |
|         - |  2417 | ` */` |
|       304 |  2418 | `static sxi32 GenStateArrowCaptureScan(` |
|         - |  2419 | `	ph7_gen_state *pGen,` |
|         - |  2420 | `	ph7_vm_func *pFunc,` |
|         - |  2421 | `	SyToken *pStart,` |
|         - |  2422 | `	SyToken *pEnd,` |
|         - |  2423 | `	SyString *aShadow,` |
|         - |  2424 | `	sxu32 nShadow)` |
|         4 |  2425 | `{` |
|       308 |  2426 | `	SyToken *pScan = pStart;` |
|         - |  2427 | `	sxi32 rc;` |
|      1740 |  2428 | `	while( pScan < pEnd ){` |
|      1436 |  2429 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|        86 |  2430 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|        28 |  2431 | `				pScan->sData.zString,` |
|        56 |  2432 | `				pScan->sData.zString + pScan->sData.nByte,` |
|        28 |  2433 | `				aShadow,nShadow);` |
|        58 |  2434 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2435 | `				return SXERR_ABORT;` |
|         - |  2436 | `			}` |
|        58 |  2437 | `			pScan++;` |
|        58 |  2438 | `			continue;` |
|         - |  2439 | `		}` |
|      1380 |  2440 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|        30 |  2441 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|        30 |  2442 | `			SyToken *pFnKw = pScan;` |
|        28 |  2443 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|       ! 0 |  2444 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|         2 |  2445 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|       ! 0 |  2446 | `				pFnKw = &pScan[1];` |
|       ! 0 |  2447 | `				nKw = PH7_TKWRD_FN;` |
|       ! 0 |  2448 | `			}` |
|        30 |  2449 | `			if( nKw == PH7_TKWRD_FN ){` |
|         - |  2450 | `				SyToken *pInnerSigStart;` |
|         - |  2451 | `				SyToken *pInnerSigEnd;` |
|         - |  2452 | `				SyToken *pInnerBodyEnd;` |
|         - |  2453 | `				SyString *aInnerShadow;` |
|         - |  2454 | `				sxu32 nInnerShadow;` |
|         - |  2455 | `				sxu32 nInnerParamMax;` |
|         - |  2456 | `				SyToken *p;` |
|         - |  2457 | `				int iNestInner;` |
|        19 |  2458 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|        19 |  2459 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  2460 | `					pScan++;` |
|       ! 0 |  2461 | `				}` |
|        19 |  2462 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  2463 | `					pScan++;` |
|       ! 0 |  2464 | `					continue;` |
|         - |  2465 | `				}` |
|        19 |  2466 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|        19 |  2467 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|         - |  2468 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|        19 |  2469 | `				if( pInnerSigEnd >= pEnd ){` |
|       ! 0 |  2470 | `					pScan = pEnd;` |
|       ! 0 |  2471 | `					continue;` |
|         - |  2472 | `				}` |
|         - |  2473 | `				/* Build an augmented shadow list: inherited + inner params */` |
|        19 |  2474 | `				nInnerParamMax = 0;` |
|        57 |  2475 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|        39 |  2476 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|        13 |  2477 | `						nInnerParamMax++;` |
|         6 |  2478 | `					}` |
|        20 |  2479 | `				}` |
|        19 |  2480 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|        18 |  2481 | `					&pGen->pVm->sAllocator,` |
|        18 |  2482 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|        19 |  2483 | `				if( aInnerShadow == 0 ){` |
|       ! 0 |  2484 | `					return SXERR_ABORT;` |
|         - |  2485 | `				}` |
|        19 |  2486 | `				nInnerShadow = 0;` |
|        25 |  2487 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|         7 |  2488 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|         4 |  2489 | `				}` |
|        57 |  2490 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|        39 |  2491 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|        27 |  2492 | `						continue;` |
|         - |  2493 | `					}` |
|        13 |  2494 | `					if( &p[1] >= pInnerSigEnd ){` |
|       ! 0 |  2495 | `						break;` |
|         - |  2496 | `					}` |
|        13 |  2497 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  2498 | `						continue;` |
|         - |  2499 | `					}` |
|        13 |  2500 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|         7 |  2501 | `				}` |
|        19 |  2502 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|        19 |  2503 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|       ! 0 |  2504 | `					pScan++;` |
|       ! 0 |  2505 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|       ! 0 |  2506 | `						&& pScan->sData.nByte == 1` |
|       ! 0 |  2507 | `						&& pScan->sData.zString[0] == '?' ){` |
|       ! 0 |  2508 | `						pScan++;` |
|       ! 0 |  2509 | `					}` |
|       ! 0 |  2510 | `					if( pScan < pEnd` |
|       ! 0 |  2511 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|       ! 0 |  2512 | `						pScan++;` |
|       ! 0 |  2513 | `					}` |
|       ! 0 |  2514 | `				}` |
|        19 |  2515 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|        19 |  2516 | `					pScan++; /* past '=>' */` |
|         9 |  2517 | `				}` |
|        19 |  2518 | `				pInnerBodyEnd = pScan;` |
|        19 |  2519 | `				iNestInner = 0;` |
|       131 |  2520 | `				while( pInnerBodyEnd < pEnd ){` |
|       113 |  2521 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|         - |  2522 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|         - |  2523 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|       ! 0 |  2524 | `						break;` |
|         - |  2525 | `					}` |
|       113 |  2526 | `					if( pInnerBodyEnd->nType &` |
|         - |  2527 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         3 |  2528 | `						iNestInner++;` |
|       112 |  2529 | `					}else if( pInnerBodyEnd->nType &` |
|         - |  2530 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         3 |  2531 | `						iNestInner--;` |
|         1 |  2532 | `					}` |
|       113 |  2533 | `					pInnerBodyEnd++;` |
|         1 |  2534 | `				}` |
|         - |  2535 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|         - |  2536 | `				 * the outer's body: a default value is evaluated at call time` |
|         - |  2537 | `				 * in the outer frame, so any free variable it references is` |
|         - |  2538 | `				 * an outer capture. We must NOT scan the parameter-name` |
|         - |  2539 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|         - |  2540 | `				 * or those names leak into the outer's closure environment.` |
|         - |  2541 | `				 *` |
|         - |  2542 | `				 * Walk the signature argument-by-argument, splitting on` |
|         - |  2543 | `				 * top-level commas, and for each argument scan only the token` |
|         - |  2544 | `				 * range after the '=' sign. */` |
|         - |  2545 | `				{` |
|        19 |  2546 | `					SyToken *pArgStart = pInnerSigStart;` |
|        31 |  2547 | `					while( pArgStart < pInnerSigEnd ){` |
|        13 |  2548 | `						SyToken *pArgEnd = pArgStart;` |
|        13 |  2549 | `						SyToken *pEq = 0;` |
|        13 |  2550 | `						int iNestArg = 0;` |
|        49 |  2551 | `						while( pArgEnd < pInnerSigEnd ){` |
|        38 |  2552 | `							if( iNestArg == 0` |
|        39 |  2553 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|         3 |  2554 | `								break;` |
|         - |  2555 | `							}` |
|        37 |  2556 | `							if( pArgEnd->nType &` |
|         - |  2557 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       ! 0 |  2558 | `								iNestArg++;` |
|        37 |  2559 | `							}else if( pArgEnd->nType &` |
|         - |  2560 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       ! 0 |  2561 | `								iNestArg--;` |
|       ! 0 |  2562 | `							}` |
|        36 |  2563 | `							if( pEq == 0 && iNestArg == 0` |
|        31 |  2564 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|         7 |  2565 | `								pEq = pArgEnd;` |
|         3 |  2566 | `							}` |
|        37 |  2567 | `							pArgEnd++;` |
|         1 |  2568 | `						}` |
|        13 |  2569 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|        10 |  2570 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|         3 |  2571 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|         7 |  2572 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 |  2573 | `								return SXERR_ABORT;` |
|         - |  2574 | `							}` |
|         3 |  2575 | `						}` |
|        13 |  2576 | `						pArgStart = pArgEnd;` |
|        12 |  2577 | `						if( pArgStart < pInnerSigEnd` |
|         8 |  2578 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|         3 |  2579 | `							pArgStart++;` |
|         1 |  2580 | `						}` |
|         1 |  2581 | `					}` |
|         - |  2582 | `				}` |
|        28 |  2583 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|         9 |  2584 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|        19 |  2585 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  2586 | `					return SXERR_ABORT;` |
|         - |  2587 | `				}` |
|        19 |  2588 | `				pScan = pInnerBodyEnd;` |
|        19 |  2589 | `				continue;` |
|         - |  2590 | `			}` |
|         5 |  2591 | `		}` |
|      1362 |  2592 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|      1182 |  2593 | `			pScan++;` |
|      1182 |  2594 | `			continue;` |
|         - |  2595 | `		}` |
|         - |  2596 | `		{` |
|         - |  2597 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|       183 |  2598 | `			SyToken *pDollar = pScan;` |
|       270 |  2599 | `			while( &pDollar[1] < pEnd` |
|       183 |  2600 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|       ! 0 |  2601 | `				pDollar++;` |
|       ! 0 |  2602 | `			}` |
|       183 |  2603 | `			if( &pDollar[1] >= pEnd ){` |
|       ! 0 |  2604 | `				break;` |
|         - |  2605 | `			}` |
|       183 |  2606 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  2607 | `				pScan = pDollar + 1;` |
|       ! 0 |  2608 | `				continue;` |
|         - |  2609 | `			}` |
|       273 |  2610 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|       180 |  2611 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|        90 |  2612 | `				aShadow,nShadow);` |
|       183 |  2613 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  2614 | `				return SXERR_ABORT;` |
|         - |  2615 | `			}` |
|       183 |  2616 | `			pScan = pDollar + 2;` |
|         - |  2617 | `		}` |
|         3 |  2618 | `	}` |
|       308 |  2619 | `	return SXRET_OK;` |
|       156 |  2620 | `}` |
|         - |  2621 | `/*` |
|         - |  2622 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|         - |  2623 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|         - |  2624 | ` * variables by value. The body is a single expression that acts as an` |
|         - |  2625 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|         - |  2626 | ` * $this is also made available.` |
|         - |  2627 | ` */` |
|       286 |  2628 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2629 | `{` |
|         - |  2630 | `	ph7_vm_func *pFunc;` |
|         - |  2631 | `	ph7_vm_func_closure_env sEnv;` |
|         - |  2632 | `	GenBlock *pBlock;` |
|         - |  2633 | `	SySet *pInstrContainer;` |
|         - |  2634 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|         - |  2635 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|         - |  2636 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|         - |  2637 | `	SyToken *pSavedEnd;` |
|         - |  2638 | `	ph7_vm_func_arg *aArgs;` |
|         - |  2639 | `	char zName[512];` |
|         - |  2640 | `	static int iCnt = 1;` |
|         - |  2641 | `	char *zDup;` |
|         - |  2642 | `	SyToken *pTokKw;` |
|         - |  2643 | `	sxu32 nLen;` |
|         - |  2644 | `	sxu32 nLine;` |
|       291 |  2645 | `	sxi32 iFlags = 0;` |
|       291 |  2646 | `	int bStatic = 0;` |
|         - |  2647 | `	sxi32 rc;` |
|         - |  2648 | `	sxu32 n;` |
|       143 |  2649 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  2650 |  |
|       291 |  2651 | `	nLine = pGen->pIn->nLine;` |
|         - |  2652 | ``	/* Attribute-sidecar key: `#[A] [static] fn` trivia is keyed to this token */`` |
|       291 |  2653 | `	pTokKw = pGen->pIn;` |
|         - |  2654 | `	/* Optional 'static' prefix */` |
|       286 |  2655 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       291 |  2656 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         7 |  2657 | `		bStatic = 1;` |
|         7 |  2658 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|         7 |  2659 | `		pGen->pIn++;` |
|         3 |  2660 | `	}` |
|         - |  2661 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|       286 |  2662 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|       291 |  2663 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|       ! 0 |  2664 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2665 | `			"Arrow function: expected 'fn' keyword");` |
|       ! 0 |  2666 | `		return SXERR_SYNTAX;` |
|         - |  2667 | `	}` |
|       291 |  2668 | `	pGen->pIn++; /* Jump 'fn' */` |
|         - |  2669 | `	/* Optional '&' — return by reference */` |
|       291 |  2670 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       ! 0 |  2671 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       ! 0 |  2672 | `		pGen->pIn++;` |
|       ! 0 |  2673 | `	}` |
|         - |  2674 | `	/* Expect '(' */` |
|       291 |  2675 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         3 |  2676 | `		if( pGen->pIn < pGen->pEnd ){` |
|         4 |  2677 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|         - |  2678 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|         2 |  2679 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         2 |  2680 | `		}else{` |
|       ! 0 |  2681 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  2682 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|         - |  2683 | `		}` |
|         3 |  2684 | `		return SXERR_SYNTAX;` |
|         - |  2685 | `	}` |
|       288 |  2686 | `	pGen->pIn++; /* Jump '(' */` |
|         - |  2687 | `	/* Delimit the parameter list */` |
|       288 |  2688 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|       288 |  2689 | `	if( pSigEnd >= pGen->pEnd ){` |
|         3 |  2690 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  2691 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|         3 |  2692 | `		return SXERR_SYNTAX;` |
|         - |  2693 | `	}` |
|         - |  2694 | `	/* Allocate the function state */` |
|       286 |  2695 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|       286 |  2696 | `	if( pFunc == 0 ){` |
|       ! 0 |  2697 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2698 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2699 | `		return SXERR_ABORT;` |
|         - |  2700 | `	}` |
|         - |  2701 | `	/* Generate a unique lambda name */` |
|       286 |  2702 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       286 |  2703 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 |  2704 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       ! 0 |  2705 | `	}` |
|       286 |  2706 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|       286 |  2707 | `	if( zDup == 0 ){` |
|       ! 0 |  2708 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2709 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2710 | `		return SXERR_ABORT;` |
|         - |  2711 | `	}` |
|       286 |  2712 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|         - |  2713 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|       286 |  2714 | `	pFunc->nLine = nLine;` |
|         - |  2715 | ``	/* Expression-position attributes (`$f = #[A] fn () => …`) */`` |
|       286 |  2716 | `	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  2717 | `		return SXERR_ABORT;` |
|         - |  2718 | `	}` |
|         - |  2719 | `	/* Collect function arguments */` |
|       286 |  2720 | `	if( pGen->pIn < pSigEnd ){` |
|       115 |  2721 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|       115 |  2722 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2723 | `			return SXERR_ABORT;` |
|         - |  2724 | `		}` |
|        56 |  2725 | `	}` |
|         - |  2726 | `	/* Point past ')' and parse optional return type */` |
|       286 |  2727 | `	pGen->pIn = &pSigEnd[1];` |
|       286 |  2728 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|       286 |  2729 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2730 | `		return SXERR_ABORT;` |
|       286 |  2731 | `	}else if( rc == SXERR_SYNTAX ){` |
|       ! 0 |  2732 | `		return SXERR_SYNTAX;` |
|         - |  2733 | `	}` |
|         - |  2734 | `	/* Expect '=>' */` |
|       286 |  2735 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|         3 |  2736 | `		if( pGen->pIn < pGen->pEnd ){` |
|         4 |  2737 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|         - |  2738 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|         2 |  2739 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         2 |  2740 | `		}else{` |
|       ! 0 |  2741 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  2742 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|         - |  2743 | `		}` |
|         3 |  2744 | `		return SXERR_SYNTAX;` |
|         - |  2745 | `	}` |
|       284 |  2746 | `	pGen->pIn++; /* Jump '=>' */` |
|       284 |  2747 | `	pBodyStart = pGen->pIn;` |
|       284 |  2748 | `	pBodyEnd = pGen->pEnd;` |
|         - |  2749 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|         - |  2750 | `	 * recursively collect free-variable references from the body. The scan` |
|         - |  2751 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|         - |  2752 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|       284 |  2753 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|         - |  2754 | `	{` |
|       284 |  2755 | `		SyString *aShadow = 0;` |
|       284 |  2756 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|       284 |  2757 | `		if( nShadow > 0 ){` |
|       113 |  2758 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|       110 |  2759 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|       113 |  2760 | `			if( aShadow == 0 ){` |
|       ! 0 |  2761 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2762 | `					"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2763 | `				return SXERR_ABORT;` |
|         - |  2764 | `			}` |
|       257 |  2765 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|       147 |  2766 | `				aShadow[n] = aArgs[n].sName;` |
|        75 |  2767 | `			}` |
|        55 |  2768 | `		}` |
|       424 |  2769 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|       140 |  2770 | `			aShadow,nShadow);` |
|       284 |  2771 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  2772 | `			return SXERR_ABORT;` |
|         - |  2773 | `		}` |
|         - |  2774 | `	}` |
|         - |  2775 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|         - |  2776 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|         - |  2777 | `	 * captured value is silently dropped when the enclosing scope has no` |
|         - |  2778 | `	 * $this. */` |
|       284 |  2779 | `	if( !bStatic ){` |
|         - |  2780 | `		char *zThisDup;` |
|       278 |  2781 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|       278 |  2782 | `		if( zThisDup == 0 ){` |
|       ! 0 |  2783 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2784 | `				"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2785 | `			return SXERR_ABORT;` |
|         - |  2786 | `		}` |
|       278 |  2787 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       278 |  2788 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|       278 |  2789 | `		sEnv.nIdx = SXU32_HIGH;` |
|       278 |  2790 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       278 |  2791 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|       278 |  2792 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       137 |  2793 | `	}` |
|         - |  2794 | `	/* Arrow functions are always closures */` |
|       284 |  2795 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|         - |  2796 | `	/* Compile the body expression as an implicit return */` |
|       424 |  2797 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       140 |  2798 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|       284 |  2799 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  2800 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2801 | `			"PH7 engine is running out-of-memory");` |
|       ! 0 |  2802 | `		return SXERR_ABORT;` |
|         - |  2803 | `	}` |
|       284 |  2804 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       284 |  2805 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       284 |  2806 | `	pSavedEnd = pGen->pEnd;` |
|       284 |  2807 | `	pGen->pIn = pBodyStart;` |
|       284 |  2808 | `	pGen->pEnd = pBodyEnd;` |
|       284 |  2809 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       284 |  2810 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2811 | `		return SXERR_ABORT;` |
|         - |  2812 | `	}` |
|         - |  2813 | `	/* The cursor stopped just past the body expression */` |
|       284 |  2814 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|         - |  2815 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|         - |  2816 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|         - |  2817 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|         - |  2818 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|       284 |  2819 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       284 |  2820 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       284 |  2821 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       284 |  2822 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       284 |  2823 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - |  2824 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|       284 |  2825 | `	pGen->pIn = pBodyEnd;` |
|       284 |  2826 | `	pGen->pEnd = pSavedEnd;` |
|         - |  2827 | `	/* Emit the load-closure instruction */` |
|       284 |  2828 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|       284 |  2829 | `	return SXRET_OK;` |
|       148 |  2830 | `}` |
|         - |  2831 | `/*` |
|         - |  2832 | ` * Compile a single arm's expression range into a freshly-allocated` |
|         - |  2833 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|         - |  2834 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|         - |  2835 | ` * expression's value.` |
|         - |  2836 | ` */` |
|       354 |  2837 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|         - |  2838 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|         3 |  2839 | `{` |
|         - |  2840 | `	SySet *pInstrContainer;` |
|         - |  2841 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  2842 | `	GenBlock *pArmBlock;` |
|         - |  2843 | `	sxi32 rc;` |
|       357 |  2844 | `	pTmpIn  = pGen->pIn;` |
|       357 |  2845 | `	pTmpEnd = pGen->pEnd;` |
|       357 |  2846 | `	pGen->pIn  = pStart;` |
|       357 |  2847 | `	pGen->pEnd = pStop;` |
|       357 |  2848 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       357 |  2849 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|         - |  2850 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|         - |  2851 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|         - |  2852 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|         - |  2853 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|         - |  2854 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|       534 |  2855 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       177 |  2856 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|       357 |  2857 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  2858 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  2859 | `		pGen->pIn  = pTmpIn;` |
|       ! 0 |  2860 | `		pGen->pEnd = pTmpEnd;` |
|       ! 0 |  2861 | `		return SXERR_ABORT;` |
|         - |  2862 | `	}` |
|       357 |  2863 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       357 |  2864 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       357 |  2865 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       357 |  2866 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       357 |  2867 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       357 |  2868 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       357 |  2869 | `	pGen->pIn  = pTmpIn;` |
|       357 |  2870 | `	pGen->pEnd = pTmpEnd;` |
|       357 |  2871 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2872 | `		return SXERR_ABORT;` |
|         - |  2873 | `	}` |
|       357 |  2874 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 |  2875 | `		return SXERR_EMPTY;` |
|         - |  2876 | `	}` |
|       357 |  2877 | `	return SXRET_OK;` |
|       180 |  2878 | `}` |
|         - |  2879 | `/*` |
|         - |  2880 | ` * Compile a PHP 8.0 match expression:` |
|         - |  2881 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|         - |  2882 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|         - |  2883 | ` * Strict comparison (===) is used between the subject and each condition.` |
|         - |  2884 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|         - |  2885 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|         - |  2886 | ` */` |
|         - |  2887 | `/*` |
|         - |  2888 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|         - |  2889 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|         - |  2890 | ` * caller can bail out of the current expression.` |
|         - |  2891 | ` */` |
|         2 |  2892 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|         1 |  2893 | `{` |
|         - |  2894 | `	va_list ap;` |
|         - |  2895 | `	sxi32 rc;` |
|         - |  2896 | `	SyBlob sMsg;` |
|         3 |  2897 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|         3 |  2898 | `	va_start(ap,zFmt);` |
|         3 |  2899 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|         3 |  2900 | `	va_end(ap);` |
|         3 |  2901 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|         3 |  2902 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|         3 |  2903 | `	SyBlobRelease(&sMsg);` |
|         3 |  2904 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2905 | `		return SXERR_ABORT;` |
|         - |  2906 | `	}` |
|         3 |  2907 | `	return SXERR_SYNTAX;` |
|         2 |  2908 | `}` |
|         - |  2909 | `/*` |
|         - |  2910 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|         - |  2911 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|         - |  2912 | ` * Returns the stop token pointer (or pEnd if none found).` |
|         - |  2913 | ` */` |
|       356 |  2914 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|         4 |  2915 | `{` |
|       360 |  2916 | `	SyToken *pCur = pStart;` |
|       360 |  2917 | `	int iNest = 0;` |
|       838 |  2918 | `	while( pCur < pEnd ){` |
|       802 |  2919 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        13 |  2920 | `			iNest++;` |
|       796 |  2921 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        13 |  2922 | `			iNest--;` |
|       784 |  2923 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|       323 |  2924 | `			return pCur;` |
|         - |  2925 | `		}` |
|       482 |  2926 | `		pCur++;` |
|         4 |  2927 | `	}` |
|        39 |  2928 | `	return pEnd;` |
|       182 |  2929 | `}` |
|        72 |  2930 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  2931 | `{` |
|         - |  2932 | `	ph7_match *pMatch;` |
|         - |  2933 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|        77 |  2934 | `	int bHasDefault = 0;` |
|         - |  2935 | `	sxu32 nLine;` |
|         - |  2936 | `	sxi32 rc;` |
|        36 |  2937 | `	SXUNUSED(iCompileFlag);` |
|        77 |  2938 | `	nLine = pGen->pIn->nLine;` |
|        77 |  2939 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|         - |  2940 | `	/* Expect '(' */` |
|        77 |  2941 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  2942 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  2943 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|       ! 0 |  2944 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|         - |  2945 | `	}` |
|        77 |  2946 | `	pGen->pIn++; /* Jump '(' */` |
|        77 |  2947 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|        77 |  2948 | `	if( pSubjEnd >= pGen->pEnd ){` |
|       ! 0 |  2949 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  2950 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|         - |  2951 | `	}` |
|        77 |  2952 | `	if( pGen->pIn >= pSubjEnd ){` |
|       ! 0 |  2953 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  2954 | `			"syntax error, unexpected \")\", expecting match subject");` |
|         - |  2955 | `	}` |
|         - |  2956 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|        77 |  2957 | `	pSavedEnd = pGen->pEnd;` |
|        77 |  2958 | `	pGen->pEnd = pSubjEnd;` |
|        77 |  2959 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        77 |  2960 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  2961 | `		return SXERR_ABORT;` |
|         - |  2962 | `	}` |
|        77 |  2963 | `	pGen->pEnd = pSavedEnd;` |
|        77 |  2964 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|         - |  2965 | `	/* Expect '{' */` |
|        77 |  2966 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|       ! 0 |  2967 | `		return GenStateMatchError(pGen,` |
|       ! 0 |  2968 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|         - |  2969 | `			"syntax error, expecting \"{\" after match subject");` |
|         - |  2970 | `	}` |
|        77 |  2971 | `	pGen->pIn++; /* Jump '{' */` |
|        77 |  2972 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|        77 |  2973 | `	if( pBodyEnd >= pGen->pEnd ){` |
|       ! 0 |  2974 | `		return GenStateMatchError(pGen,nLine,` |
|         - |  2975 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|         - |  2976 | `	}` |
|         - |  2977 | `	/* Allocate ph7_match container */` |
|        77 |  2978 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|        77 |  2979 | `	if( pMatch == 0 ){` |
|       ! 0 |  2980 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  2981 | `			"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  2982 | `		return SXERR_ABORT;` |
|         - |  2983 | `	}` |
|        77 |  2984 | `	SyZero(pMatch,sizeof(ph7_match));` |
|        77 |  2985 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|         - |  2986 | `	/* Iterate arms */` |
|       259 |  2987 | `	while( pGen->pIn < pBodyEnd ){` |
|         - |  2988 | `		ph7_match_arm sArm;` |
|         - |  2989 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|       190 |  2990 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|       190 |  2991 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|       190 |  2992 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|       190 |  2993 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - |  2994 | `		/* 'default' arm? */` |
|       186 |  2995 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       107 |  2996 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|        22 |  2997 | `			if( bHasDefault ){` |
|         3 |  2998 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|         - |  2999 | `					"Match expressions may only contain one default arm");` |
|         4 |  3000 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  3001 | `			}` |
|        20 |  3002 | `			sArm.bDefault = 1;` |
|        20 |  3003 | `			bHasDefault = 1;` |
|        20 |  3004 | `			pGen->pIn++;` |
|        20 |  3005 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       ! 0 |  3006 | `				return GenStateMatchError(pGen,nArmLine,` |
|         - |  3007 | `					"syntax error, expecting \"=>\" after 'default'");` |
|         - |  3008 | `			}` |
|        20 |  3009 | `			pGen->pIn++; /* Jump '=>' */` |
|        11 |  3010 | `		}else{` |
|         - |  3011 | `			/* Condition list: cond (',' cond)* '=>' */` |
|       170 |  3012 | `			pCondStart = pGen->pIn;` |
|       170 |  3013 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|         - |  3014 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       178 |  3015 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|         - |  3016 | `				SySet sCondBc;` |
|         9 |  3017 | `				if( pCondStart >= pArrow ){` |
|       ! 0 |  3018 | `					return GenStateMatchError(pGen,nArmLine,` |
|         - |  3019 | `						"syntax error, empty match condition expression");` |
|         - |  3020 | `				}` |
|         9 |  3021 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         9 |  3022 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|         9 |  3023 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  3024 | `					return SXERR_ABORT;` |
|         - |  3025 | `				}` |
|         9 |  3026 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|         9 |  3027 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|         9 |  3028 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|         - |  3029 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|         1 |  3030 | `			}` |
|       170 |  3031 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|         3 |  3032 | `				return GenStateMatchError(pGen,nArmLine,` |
|         - |  3033 | `					"syntax error, expecting \"=>\" in match arm");` |
|         - |  3034 | `			}` |
|       167 |  3035 | `			if( pCondStart >= pArrow ){` |
|       ! 0 |  3036 | `				return GenStateMatchError(pGen,nArmLine,` |
|         - |  3037 | `					"syntax error, empty match condition expression");` |
|         - |  3038 | `			}` |
|         - |  3039 | `			{` |
|         - |  3040 | `				SySet sCondBc;` |
|       167 |  3041 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       167 |  3042 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       167 |  3043 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  3044 | `					return SXERR_ABORT;` |
|         - |  3045 | `				}` |
|       167 |  3046 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|         - |  3047 | `			}` |
|       167 |  3048 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|         - |  3049 | `		}` |
|         - |  3050 | `		/* Compile result expression: up to top-level ',' or body end */` |
|       185 |  3051 | `		pResStart = pGen->pIn;` |
|       185 |  3052 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|       185 |  3053 | `		if( pResStart >= pResEnd ){` |
|       ! 0 |  3054 | `			return GenStateMatchError(pGen,nArmLine,` |
|         - |  3055 | `				"syntax error, expected expression after \"=>\"");` |
|         - |  3056 | `		}` |
|       185 |  3057 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|       185 |  3058 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3059 | `			return SXERR_ABORT;` |
|         - |  3060 | `		}` |
|       185 |  3061 | `		pGen->pIn = pResEnd;` |
|       185 |  3062 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       151 |  3063 | `			pGen->pIn++; /* Skip trailing ',' */` |
|        74 |  3064 | `		}` |
|       185 |  3065 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|         3 |  3066 | `	}` |
|        71 |  3067 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|        71 |  3068 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|        71 |  3069 | `	return SXRET_OK;` |
|        41 |  3070 | `}` |
|         - |  3071 | `/*` |
|         - |  3072 | ` * Compile a backtick quoted string.` |
|         - |  3073 | ` */` |
|         4 |  3074 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         2 |  3075 | `{` |
|         - |  3076 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|         - |  3077 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|         - |  3078 | `	 */` |
|         8 |  3079 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|         - |  3080 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|         2 |  3081 | `		ph7_lib_version()` |
|         - |  3082 | `		);` |
|         - |  3083 | `	/* Load NULL */` |
|         6 |  3084 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         2 |  3085 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|         - |  3086 | `	/* Node successfully compiled */` |
|         6 |  3087 | `	return SXRET_OK;` |
|         2 |  3088 | `}` |
|         - |  3089 | `/*` |
|         - |  3090 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|         - |  3091 | ` * construct.` |
|         - |  3092 | ` */` |
|        82 |  3093 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3094 | `{` |
|         - |  3095 | `	SyString *pName;` |
|         - |  3096 | `	sxu32 nKeyID;` |
|         - |  3097 | `	sxi32 rc;` |
|         - |  3098 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|        87 |  3099 | `	pName = &pGen->pIn->sData;` |
|        87 |  3100 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        87 |  3101 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|        87 |  3102 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|         9 |  3103 | `		SyToken *pTmp,*pNext = 0;` |
|         - |  3104 | `		/* Compile arguments one after one */` |
|         9 |  3105 | `		pTmp = pGen->pEnd;` |
|         - |  3106 | `		/* Symisc eXtension to the PHP programming language:` |
|         - |  3107 | `		 * 'echo' can be used in the context of a function which` |
|         - |  3108 | `		 *  mean that the following expression is valid:` |
|         - |  3109 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|         - |  3110 | `		 */` |
|         9 |  3111 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|        17 |  3112 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|         9 |  3113 | `			if( pGen->pIn < pNext ){` |
|         9 |  3114 | `				pGen->pEnd = pNext;` |
|         9 |  3115 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|         9 |  3116 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  3117 | `					return SXERR_ABORT;` |
|         - |  3118 | `				}` |
|         9 |  3119 | `				if( rc != SXERR_EMPTY ){` |
|         - |  3120 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|         - |  3121 | `					 * without the overhead of a function call.` |
|         - |  3122 | `					 * This is a very powerful optimization that improve` |
|         - |  3123 | `					 * performance greatly.` |
|         - |  3124 | `					 */` |
|         9 |  3125 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|         4 |  3126 | `				}` |
|         4 |  3127 | `			}` |
|         - |  3128 | `			/* Jump trailing commas */` |
|         9 |  3129 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|       ! 0 |  3130 | `				pNext++;` |
|       ! 0 |  3131 | `			}` |
|         9 |  3132 | `			pGen->pIn = pNext;` |
|         1 |  3133 | `		}` |
|         - |  3134 | `		/* Restore token stream */` |
|         9 |  3135 | `		pGen->pEnd = pTmp;` |
|         5 |  3136 | `	}else{` |
|        79 |  3137 | `		sxi32 nArg = 0;` |
|        79 |  3138 | `		sxu32 nIdx = 0;` |
|        79 |  3139 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|        79 |  3140 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3141 | `			return SXERR_ABORT;` |
|        79 |  3142 | `		}else if(rc != SXERR_EMPTY ){` |
|        79 |  3143 | `			nArg = 1;` |
|        37 |  3144 | `		}` |
|        79 |  3145 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|         - |  3146 | `			ph7_value *pObj;` |
|         - |  3147 | `			/* Emit the call instruction */` |
|        31 |  3148 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        31 |  3149 | `			if( pObj == 0 ){` |
|       ! 0 |  3150 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3151 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  3152 | `				return SXERR_ABORT;` |
|         - |  3153 | `			}` |
|        31 |  3154 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|         - |  3155 | `			/* Install in the literal table */` |
|        31 |  3156 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|        13 |  3157 | `		}` |
|         - |  3158 | `		/* Emit the call instruction */` |
|        79 |  3159 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        79 |  3160 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|         - |  3161 | `	}` |
|         - |  3162 | `	/* Node successfully compiled */` |
|        87 |  3163 | `	return SXRET_OK;` |
|        46 |  3164 | `}` |
|         - |  3165 | `/*` |
|         - |  3166 | ` * Compile a node holding a variable declaration.` |
|         - |  3167 | ` * According to the PHP language reference` |
|         - |  3168 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|         - |  3169 | ` *  The variable name is case-sensitive.` |
|         - |  3170 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|         - |  3171 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|         - |  3172 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|         - |  3173 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|         - |  3174 | ` *  Note: $this is a special variable that can't be assigned.` |
|         - |  3175 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|         - |  3176 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|         - |  3177 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|         - |  3178 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|         - |  3179 | ` *  the chapter on Expressions.` |
|         - |  3180 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|         - |  3181 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|         - |  3182 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|         - |  3183 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|         - |  3184 | ` *  is being assigned (the source variable).` |
|         - |  3185 | ` */` |
|  16493382 |  3186 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3187 | `{` |
|  16493387 |  3188 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  3189 | `	sxi32 iVv;` |
|         - |  3190 | `	sxi32 iP1;` |
|         - |  3191 | `	void *p3;` |
|         - |  3192 | `	sxi32 rc;` |
|  16493387 |  3193 | `	iVv = -1; /* Variable variable counter */` |
|  32986781 |  3194 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  16493399 |  3195 | `		pGen->pIn++;` |
|  16493399 |  3196 | `		iVv++;` |
|         5 |  3197 | `	}` |
|  16493387 |  3198 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|         - |  3199 | `		/* Invalid variable name */` |
|       ! 0 |  3200 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|       ! 0 |  3201 | `		if( rc == SXERR_ABORT ){` |
|         - |  3202 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3203 | `			return SXERR_ABORT;` |
|         - |  3204 | `		}` |
|       ! 0 |  3205 | `		return SXRET_OK;` |
|         - |  3206 | `	}` |
|  16493387 |  3207 | `	p3  = 0;` |
|  16493387 |  3208 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|         - |  3209 | `		/* Dynamic variable creation */` |
|        21 |  3210 | `		pGen->pIn++;  /* Jump the open curly */` |
|        21 |  3211 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|        21 |  3212 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - |  3213 | `			/* Empty expression */` |
|         3 |  3214 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|         3 |  3215 | `			return SXRET_OK;` |
|         - |  3216 | `		}` |
|         - |  3217 | `		/* Compile the expression holding the variable name */` |
|        18 |  3218 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        18 |  3219 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3220 | `			return SXERR_ABORT;` |
|        18 |  3221 | `		}else if( rc == SXERR_EMPTY ){` |
|         3 |  3222 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|         3 |  3223 | `			return SXRET_OK;` |
|         - |  3224 | `		}` |
|         8 |  3225 | `	}else{` |
|         - |  3226 | `		SyHashEntry *pEntry;` |
|         - |  3227 | `		SyString *pName;` |
|  16493369 |  3228 | `		char *zName = 0;` |
|         - |  3229 | `		/* Extract variable name */` |
|  16493369 |  3230 | `		pName = &pGen->pIn->sData;` |
|         - |  3231 | `		/* Advance the stream cursor */` |
|  16493369 |  3232 | `		pGen->pIn++;` |
|  16493369 |  3233 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  16493369 |  3234 | `		if( pEntry == 0 ){` |
|         - |  3235 | `			/* Duplicate name */` |
|    952387 |  3236 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    952387 |  3237 | `			if( zName == 0 ){` |
|       ! 0 |  3238 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3239 | `				return SXERR_ABORT;` |
|         - |  3240 | `			}` |
|         - |  3241 | `			/* Install in the hashtable */` |
|    952387 |  3242 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|    476196 |  3243 | `		}else{` |
|         - |  3244 | `			/* Name already available */` |
|  15540987 |  3245 | `			zName = (char *)pEntry->pUserData;` |
|         - |  3246 | `		}` |
|  16493369 |  3247 | `		p3 = (void *)zName;` |
|         - |  3248 | `	}` |
|  16493383 |  3249 | `	iP1 = 0;` |
|  16493383 |  3250 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|   4910999 |  3251 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|         - |  3252 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|   4907031 |  3253 | `			iP1 = 1;` |
|   2453513 |  3254 | `		}` |
|   2455497 |  3255 | `	}` |
|         - |  3256 | `	/* Emit the load instruction */` |
|  16493383 |  3257 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  16493395 |  3258 | `	while( iVv > 0 ){` |
|        13 |  3259 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|        13 |  3260 | `		iVv--;` |
|         1 |  3261 | `	}` |
|         - |  3262 | `	/* Node successfully compiled */` |
|  16493383 |  3263 | `	return SXRET_OK;` |
|   8246696 |  3264 | `}` |
|         - |  3265 | `/*` |
|         - |  3266 | ` * Load a literal.` |
|         - |  3267 | ` */` |
|  11171072 |  3268 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|         5 |  3269 | `{` |
|  11171077 |  3270 | `	SyToken *pToken = pGen->pIn;` |
|         - |  3271 | `	ph7_value *pObj;` |
|         - |  3272 | `	SyString *pStr;` |
|         - |  3273 | `	sxu32 nIdx;` |
|         - |  3274 | `	/* Extract token value */` |
|  11171077 |  3275 | `	pStr = &pToken->sData;` |
|         - |  3276 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  11171077 |  3277 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   2154061 |  3278 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|         - |  3279 | `			/* NULL constant are always indexed at 0 */` |
|    894867 |  3280 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|    894867 |  3281 | `			return SXRET_OK;` |
|   1259199 |  3282 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|         - |  3283 | `			/* TRUE constant are always indexed at 1 */` |
|    284635 |  3284 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|    284635 |  3285 | `			return SXRET_OK;` |
|         5 |  3286 | `		}` |
|  10437636 |  3287 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   1866666 |  3288 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|         - |  3289 | `			/* FALSE constant are always indexed at 2 */` |
|    638611 |  3290 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|    638611 |  3291 | `			return SXRET_OK;` |
|   8779025 |  3292 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|    801220 |  3293 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|         - |  3294 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|     11825 |  3295 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     11825 |  3296 | `			if( pObj == 0 ){` |
|       ! 0 |  3297 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3298 | `				return SXERR_ABORT;` |
|         - |  3299 | `			}` |
|     11825 |  3300 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|         - |  3301 | `			/* Emit the load constant instruction */` |
|     11825 |  3302 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     11825 |  3303 | `			return SXRET_OK;` |
|   8461515 |  3304 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|    189840 |  3305 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|         - |  3306 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|         8 |  3307 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|         8 |  3308 | `			if( pObj == 0 ){` |
|       ! 0 |  3309 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3310 | `				return SXERR_ABORT;` |
|         - |  3311 | `			}` |
|         8 |  3312 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - |  3313 | `				SyString sNs;` |
|         8 |  3314 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         8 |  3315 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|         5 |  3316 | `			}else{` |
|       ! 0 |  3317 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|         - |  3318 | `			}` |
|         8 |  3319 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         8 |  3320 | `			return SXRET_OK;` |
|   8465651 |  3321 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|    390718 |  3322 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|   8559156 |  3323 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|    385158 |  3324 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|        11 |  3325 | `			GenBlock *pBlock = pGen->pCurrent;` |
|         - |  3326 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|        21 |  3327 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|         - |  3328 | `				/* Point to the upper block */` |
|        11 |  3329 | `				pBlock = pBlock->pParent;` |
|         1 |  3330 | `			}` |
|        11 |  3331 | `			if( pBlock == 0 ){` |
|         - |  3332 | `				/* Called in the global scope,load NULL */` |
|         5 |  3333 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         3 |  3334 | `			}else{` |
|         - |  3335 | `				/* Extract the target function/method */` |
|         7 |  3336 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|         7 |  3337 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|         - |  3338 | `					/* Not a class method,Load null */` |
|         3 |  3339 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|         2 |  3340 | `				}else{` |
|         5 |  3341 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|         5 |  3342 | `					if( pObj == 0 ){` |
|       ! 0 |  3343 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3344 | `						return SXERR_ABORT;` |
|         - |  3345 | `					}` |
|         5 |  3346 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|         - |  3347 | `					/* Emit the load constant instruction */` |
|         5 |  3348 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - |  3349 | `				}` |
|         - |  3350 | `			}` |
|        11 |  3351 | `			return SXRET_OK;` |
|         - |  3352 | `	}` |
|         - |  3353 | `	/* Query literal table */` |
|   9341143 |  3354 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|         - |  3355 | `		ph7_value *pLitObj;` |
|         - |  3356 | `		/* Unknown literal,install it in the literal table */` |
|   1790251 |  3357 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   1790251 |  3358 | `		if( pLitObj == 0 ){` |
|       ! 0 |  3359 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  3360 | `			return SXERR_ABORT;` |
|         - |  3361 | `		}` |
|   1790251 |  3362 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|   1790251 |  3363 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|    895123 |  3364 | `	}` |
|         - |  3365 | `	/* Emit the load constant instruction */` |
|   9341143 |  3366 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|   9341143 |  3367 | `	return SXRET_OK;` |
|   5585541 |  3368 | `}` |
|         - |  3369 | `/*` |
|         - |  3370 | ` * Resolve a namespace path or simply load a literal.` |
|         - |  3371 | ` * If the token stream contains namespace separators (backslashes),` |
|         - |  3372 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|         - |  3373 | ` * Otherwise, load the simple literal directly.` |
|         - |  3374 | ` */` |
|  11175058 |  3375 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|         5 |  3376 | `{` |
|         - |  3377 | `	sxi32 rc;` |
|  11175063 |  3378 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  3379 | `		return SXRET_OK;` |
|         - |  3380 | `	}` |
|         - |  3381 | `	/* Check if this is a multi-token namespace path */` |
|  11175063 |  3382 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|         - |  3383 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      3991 |  3384 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      3991 |  3385 | `		int isAbsolute = 0;` |
|      3991 |  3386 | `		SyBlobReset(pWorker);` |
|         - |  3387 | `		/* Check for leading backslash (absolute path) */` |
|      3991 |  3388 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      3989 |  3389 | `			isAbsolute = 1;` |
|      3989 |  3390 | `			pGen->pIn++; /* Skip leading backslash */` |
|      1992 |  3391 | `		}` |
|         - |  3392 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      3991 |  3393 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         3 |  3394 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         3 |  3395 | `			SyBlobAppend(pWorker,"\\",1);` |
|         1 |  3396 | `		}` |
|         - |  3397 | `		/* Collect all path components */` |
|      4099 |  3398 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|      4099 |  3399 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        58 |  3400 | `				SyBlobAppend(pWorker,"\\",1);` |
|        31 |  3401 | `			}else{` |
|      4045 |  3402 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|         - |  3403 | `			}` |
|      4099 |  3404 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      3991 |  3405 | `				pGen->pIn++;` |
|      3991 |  3406 | `				break;` |
|         - |  3407 | `			}` |
|       112 |  3408 | `			pGen->pIn++;` |
|         4 |  3409 | `		}` |
|      3991 |  3410 | `		if( SyBlobLength(pWorker) > 0 ){` |
|         - |  3411 | `			ph7_value *pObj;` |
|         - |  3412 | `			SyString sPath;` |
|         - |  3413 | `			sxu32 nIdx;` |
|      3991 |  3414 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|         - |  3415 | `			/* Install in the literal table */` |
|      3991 |  3416 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      3961 |  3417 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3961 |  3418 | `				if( pObj == 0 ){` |
|       ! 0 |  3419 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  3420 | `					return SXERR_ABORT;` |
|         - |  3421 | `				}` |
|      3961 |  3422 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      3961 |  3423 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1978 |  3424 | `			}` |
|         - |  3425 | `			/* Emit the load constant instruction.` |
|         - |  3426 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|         - |  3427 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|      5984 |  3428 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|      1993 |  3429 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|      1993 |  3430 | `				nIdx,0,0);` |
|      3991 |  3431 | `			return SXRET_OK;` |
|         - |  3432 | `		}` |
|       ! 0 |  3433 | `	}` |
|         - |  3434 | `	/* Single-token literal: load directly */` |
|  11171077 |  3435 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  11171077 |  3436 | `	return rc;` |
|   5587534 |  3437 | `}` |
|         - |  3438 | `/*` |
|         - |  3439 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|         - |  3440 | ` */` |
|         - |  3441 | `/*` |
|         - |  3442 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|         - |  3443 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|         - |  3444 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|         - |  3445 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|         - |  3446 | ` */` |
|       ! 0 |  3447 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       ! 0 |  3448 | `{` |
|       ! 0 |  3449 | `	SXUNUSED(iCompileFlag);` |
|       ! 0 |  3450 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|         - |  3451 | `		"Cannot use the first-class callable syntax '...' here");` |
|       ! 0 |  3452 | `	return SXERR_SYNTAX;` |
|       ! 0 |  3453 | `}` |
|  11175058 |  3454 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         5 |  3455 | `{` |
|         - |  3456 | `	sxi32 rc;` |
|  11175063 |  3457 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  11175063 |  3458 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3459 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|       ! 0 |  3460 | `		return rc;` |
|         - |  3461 | `	}` |
|         - |  3462 | `	/* Node successfully compiled */` |
|  11175063 |  3463 | `	return SXRET_OK;` |
|   5587534 |  3464 | `}` |
|         - |  3465 | `/*` |
|         - |  3466 | ` * Recover from a compile-time error. In other words synchronize` |
|         - |  3467 | ` * the token stream cursor with the first semi-colon seen.` |
|         - |  3468 | ` */` |
|         8 |  3469 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|         1 |  3470 | `{` |
|         - |  3471 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|        17 |  3472 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|         9 |  3473 | `		pGen->pIn++;` |
|         1 |  3474 | `	}` |
|         9 |  3475 | `	return SXRET_OK;` |
|         1 |  3476 | `}` |
|         - |  3477 | `/*` |
|         - |  3478 | ` * Check if the given identifier name is reserved or not.` |
|         - |  3479 | ` * Return TRUE if reserved.FALSE otherwise.` |
|         - |  3480 | ` */` |
|    299508 |  3481 | `static int GenStateIsReservedConstant(SyString *pName)` |
|         5 |  3482 | `{` |
|    299513 |  3483 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      3987 |  3484 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|         3 |  3485 | `			return TRUE;` |
|      3985 |  3486 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|         6 |  3487 | `			return TRUE;` |
|         5 |  3488 | `		}` |
|    297519 |  3489 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|      7901 |  3490 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|         3 |  3491 | `			return TRUE;` |
|         - |  3492 | `		}` |
|      3947 |  3493 | `	}` |
|         - |  3494 | `	/* Not a reserved constant */` |
|    299505 |  3495 | `	return FALSE;` |
|    149759 |  3496 | `}` |
|         - |  3497 | `/*` |
|         - |  3498 | ` * Compile the 'const' statement.` |
|         - |  3499 | ` * According to the PHP language reference` |
|         - |  3500 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|         - |  3501 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|         - |  3502 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|         - |  3503 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|         - |  3504 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|         - |  3505 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|         - |  3506 | ` *  Syntax` |
|         - |  3507 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|         - |  3508 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|         - |  3509 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|         - |  3510 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|         - |  3511 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|         - |  3512 | ` *  to get a list of all defined constants.` |
|         - |  3513 | ` *` |
|         - |  3514 | ` * Symisc eXtension.` |
|         - |  3515 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|         - |  3516 | ` *  would allow only simple scalar value.` |
|         - |  3517 | ` *  Example` |
|         - |  3518 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|         - |  3519 | ` *    Refer to the official documentation for more information on this feature.` |
|         - |  3520 | ` */` |
|        48 |  3521 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|         5 |  3522 | `{` |
|         - |  3523 | `	SySet *pConsCode,*pInstrContainer;` |
|        53 |  3524 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  3525 | `	SyString *pName;` |
|         - |  3526 | `	sxi32 rc;` |
|        53 |  3527 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        53 |  3528 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  3529 | `		/* Invalid constant name */` |
|         8 |  3530 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|         8 |  3531 | `		if( rc == SXERR_ABORT ){` |
|         - |  3532 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3533 | `			return SXERR_ABORT;` |
|         - |  3534 | `		}` |
|         8 |  3535 | `		goto Synchronize;` |
|         - |  3536 | `	}` |
|         - |  3537 | `	/* Peek constant name */` |
|        47 |  3538 | `	pName = &pGen->pIn->sData;` |
|         - |  3539 | `	/* Make sure the constant name isn't reserved */` |
|        47 |  3540 | `	if( GenStateIsReservedConstant(pName) ){` |
|         - |  3541 | `		/* Reserved constant */` |
|        10 |  3542 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|        10 |  3543 | `		if( rc == SXERR_ABORT ){` |
|         - |  3544 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3545 | `			return SXERR_ABORT;` |
|         - |  3546 | `		}` |
|        10 |  3547 | `		goto Synchronize;` |
|         - |  3548 | `	}` |
|        38 |  3549 | `	pGen->pIn++;` |
|        38 |  3550 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|         - |  3551 | `		/* Invalid statement*/` |
|         6 |  3552 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|         6 |  3553 | `		if( rc == SXERR_ABORT ){` |
|         - |  3554 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3555 | `			return SXERR_ABORT;` |
|         - |  3556 | `		}` |
|         6 |  3557 | `		goto Synchronize;` |
|         - |  3558 | `	}` |
|        32 |  3559 | `	pGen->pIn++; /*Jump the equal sign */` |
|         - |  3560 | `	/* Allocate a new constant value container */` |
|        32 |  3561 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|        32 |  3562 | `	if( pConsCode == 0 ){` |
|       ! 0 |  3563 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3564 | `		return SXERR_ABORT;` |
|         - |  3565 | `	}` |
|        32 |  3566 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - |  3567 | `	/* Swap bytecode container */` |
|        32 |  3568 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        32 |  3569 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|         - |  3570 | `	/* Compile constant value */` |
|        32 |  3571 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  3572 | `	/* Emit the done instruction */` |
|        32 |  3573 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        32 |  3574 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        32 |  3575 | `	if( rc == SXERR_ABORT ){` |
|         - |  3576 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  3577 | `		return SXERR_ABORT;` |
|         - |  3578 | `	}` |
|        32 |  3579 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|         - |  3580 | `	/* Register the constant with namespace-qualified name */` |
|         - |  3581 | `	{` |
|         - |  3582 | `		SyBlob sFQN;` |
|         - |  3583 | `		SyString sFQNStr;` |
|        32 |  3584 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        32 |  3585 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|        32 |  3586 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        47 |  3587 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|        30 |  3588 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|        32 |  3589 | `		if( rc == SXRET_OK && SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - |  3590 | ``			/* php 8.5: attributes on `const` statements — attach the pending`` |
|         - |  3591 | `			 * groups to the registered constant record for Reflection. */` |
|         7 |  3592 | `			SyHashEntry *pCEntry = SyHashGet(&pGen->pVm->hConstant,` |
|         4 |  3593 | `				SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|         5 |  3594 | `			if( pCEntry ){` |
|         5 |  3595 | `				ph7_constant *pRegCons = (ph7_constant *)pCEntry->pUserData;` |
|         5 |  3596 | `				if( GenStateConsumeAttrs(&(*pGen),&pRegCons->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  3597 | `					SyBlobRelease(&sFQN);` |
|       ! 0 |  3598 | `					return SXERR_ABORT;` |
|         - |  3599 | `				}` |
|         2 |  3600 | `			}` |
|         2 |  3601 | `		}` |
|        32 |  3602 | `		SyBlobRelease(&sFQN);` |
|         - |  3603 | `	}` |
|        32 |  3604 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3605 | `		SySetRelease(pConsCode);` |
|       ! 0 |  3606 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|       ! 0 |  3607 | `	}` |
|        32 |  3608 | `	return SXRET_OK;` |
|         9 |  3609 | `Synchronize:` |
|         - |  3610 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|        60 |  3611 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        42 |  3612 | `		pGen->pIn++;` |
|         4 |  3613 | `	}` |
|        22 |  3614 | `	return SXRET_OK;` |
|        29 |  3615 | `}` |
|         - |  3616 | `/*` |
|         - |  3617 | ` * Compile the 'continue' statement.` |
|         - |  3618 | ` * According to the PHP language reference` |
|         - |  3619 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|         - |  3620 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|         - |  3621 | ` *  iteration.` |
|         - |  3622 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|         - |  3623 | ` *  the purposes of continue.` |
|         - |  3624 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|         - |  3625 | ` *  of enclosing loops it should skip to the end of.` |
|         - |  3626 | ` *  Note:` |
|         - |  3627 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|         - |  3628 | ` */` |
|         - |  3629 | `/*` |
|         - |  3630 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|         - |  3631 | ` * block and the target loop block. This ensures finally blocks run when` |
|         - |  3632 | ` * break/continue crosses a try boundary.` |
|         - |  3633 | ` *` |
|         - |  3634 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|         - |  3635 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|         - |  3636 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|         - |  3637 | ` */` |
|    118294 |  3638 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|         5 |  3639 | `{` |
|    118299 |  3640 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    118299 |  3641 | `	int nInlineTry = 0;` |
|    551725 |  3642 | `	while( pBlock && pBlock != pTarget ){` |
|    433431 |  3643 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|         6 |  3644 | `			if( pBlock->pUserData ){` |
|         - |  3645 | `				/* A try block with an exception context. In a generator its catch/finally` |
|         - |  3646 | `				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that` |
|         - |  3647 | `				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.` |
|         - |  3648 | `				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */` |
|         6 |  3649 | `				if( pGen->bInGenerator ){` |
|         3 |  3650 | `					nInlineTry++;` |
|         2 |  3651 | `				}else{` |
|         3 |  3652 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|         - |  3653 | `				}` |
|         4 |  3654 | `			}else{` |
|         - |  3655 | `				/* A catch/finally block compiled into a separate bytecode container` |
|         - |  3656 | `				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */` |
|       ! 0 |  3657 | `				break;` |
|         - |  3658 | `			}` |
|         2 |  3659 | `		}` |
|    433431 |  3660 | `		pBlock = pBlock->pParent;` |
|         5 |  3661 | `	}` |
|    118299 |  3662 | `	return nInlineTry;` |
|         5 |  3663 | `}` |
|     59120 |  3664 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|         5 |  3665 | `{` |
|         - |  3666 | `	GenBlock *pLoop; /* Target loop */` |
|         - |  3667 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|         - |  3668 | `	sxu32 nLineLocal;` |
|         - |  3669 | `	sxi32 rc;` |
|     59125 |  3670 | `	nLineLocal = pGen->pIn->nLine;` |
|     59125 |  3671 | `	iLevel = 0;` |
|         - |  3672 | `	/* Jump the 'continue' keyword */` |
|     59125 |  3673 | `	pGen->pIn++;` |
|     59125 |  3674 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|         - |  3675 | `		/* optional numeric argument which tells us how many levels` |
|         - |  3676 | `		 * of enclosing loops we should skip to the end of.` |
|         - |  3677 | `		 */` |
|         - |  3678 | `		char zScratch[GEN_NUM_SCRATCH];` |
|        17 |  3679 | `		char *zAlloc = 0;` |
|         - |  3680 | `		SyString sNum;` |
|        17 |  3681 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|        17 |  3682 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3683 | `			return SXERR_ABORT;` |
|         - |  3684 | `		}` |
|        17 |  3685 | `		if( rc == SXRET_OK ){` |
|        20 |  3686 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|        12 |  3687 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|        14 |  3688 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  3689 | `				return SXERR_ABORT;` |
|         - |  3690 | `			}` |
|        14 |  3691 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|        14 |  3692 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         6 |  3693 | `		}` |
|        17 |  3694 | `		if( iLevel < 2 ){` |
|         3 |  3695 | `			iLevel = 0;` |
|         1 |  3696 | `		}` |
|        17 |  3697 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|         7 |  3698 | `	}` |
|         - |  3699 | `	/* Point to the target loop */` |
|     59125 |  3700 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     59125 |  3701 | `	if( pLoop == 0 ){` |
|         - |  3702 | `		/* Illegal continue */` |
|        12 |  3703 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|        12 |  3704 | `		if( rc == SXERR_ABORT ){` |
|         - |  3705 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3706 | `			return SXERR_ABORT;` |
|         - |  3707 | `		}` |
|         7 |  3708 | `	}else{` |
|     59115 |  3709 | `		sxu32 nInstrIdx = 0;` |
|         - |  3710 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     59115 |  3711 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|         - |  3712 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|         - |  3713 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|     59115 |  3714 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|     59115 |  3715 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|         - |  3716 | `			/* According to the PHP language reference manual` |
|         - |  3717 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|         - |  3718 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|         - |  3719 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|         - |  3720 | `			 */` |
|         5 |  3721 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|         5 |  3722 | `			if( rc == SXRET_OK ){` |
|         5 |  3723 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|         2 |  3724 | `			}` |
|         3 |  3725 | `		}else{` |
|         - |  3726 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|     59111 |  3727 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|     59111 |  3728 | `			if( pLoop->bPostContinue == TRUE ){` |
|         - |  3729 | `				JumpFixup sJumpFix;` |
|         - |  3730 | `				/* Post-continue */` |
|     19707 |  3731 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|     19707 |  3732 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|     19707 |  3733 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|      9851 |  3734 | `			}` |
|         - |  3735 | `		}` |
|         - |  3736 | `	}` |
|     59125 |  3737 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  3738 | `		/* Not so fatal,emit a warning only */` |
|       ! 0 |  3739 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|       ! 0 |  3740 | `	}` |
|         - |  3741 | `	/* Statement successfully compiled */` |
|     59125 |  3742 | `	return SXRET_OK;` |
|     29565 |  3743 | `}` |
|         - |  3744 | `/*` |
|         - |  3745 | ` * Compile the 'break' statement.` |
|         - |  3746 | ` * According to the PHP language reference` |
|         - |  3747 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|         - |  3748 | ` *  structure.` |
|         - |  3749 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|         - |  3750 | ` *  enclosing structures are to be broken out of.` |
|         - |  3751 | ` */` |
|     59200 |  3752 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|         5 |  3753 | `{` |
|         - |  3754 | `	GenBlock *pLoop; /* Target loop */` |
|         - |  3755 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|         - |  3756 | `	sxi32 rc;` |
|     59205 |  3757 | `	iLevel = 0;` |
|         - |  3758 | `	/* Jump the 'break' keyword */` |
|     59205 |  3759 | `	pGen->pIn++;` |
|     59205 |  3760 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|         - |  3761 | `		/* optional numeric argument which tells us how many levels` |
|         - |  3762 | `		 * of enclosing loops we should skip to the end of.` |
|         - |  3763 | `		 */` |
|         - |  3764 | `		char zScratch[GEN_NUM_SCRATCH];` |
|        17 |  3765 | `		char *zAlloc = 0;` |
|         - |  3766 | `		SyString sNum;` |
|        17 |  3767 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|        17 |  3768 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3769 | `			return SXERR_ABORT;` |
|         - |  3770 | `		}` |
|        17 |  3771 | `		if( rc == SXRET_OK ){` |
|        20 |  3772 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|        12 |  3773 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|        14 |  3774 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  3775 | `				return SXERR_ABORT;` |
|         - |  3776 | `			}` |
|        14 |  3777 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|        14 |  3778 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|         6 |  3779 | `		}` |
|        17 |  3780 | `		if( iLevel < 2 ){` |
|         3 |  3781 | `			iLevel = 0;` |
|         1 |  3782 | `		}` |
|        17 |  3783 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|         7 |  3784 | `	}` |
|         - |  3785 | `	/* Extract the target loop */` |
|     59205 |  3786 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     59205 |  3787 | `	if( pLoop == 0 ){` |
|         - |  3788 | `		/* Illegal break */` |
|        18 |  3789 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|        18 |  3790 | `		if( rc == SXERR_ABORT ){` |
|         - |  3791 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3792 | `			return SXERR_ABORT;` |
|         - |  3793 | `		}` |
|        10 |  3794 | `	}else{` |
|         - |  3795 | `		sxu32 nInstrIdx;` |
|         - |  3796 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     59189 |  3797 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|         - |  3798 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|     59189 |  3799 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|     59189 |  3800 | `		if( rc == SXRET_OK ){` |
|         - |  3801 | `			/* Fix the jump later when the jump destination is resolved */` |
|     59189 |  3802 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|     29592 |  3803 | `		}` |
|         - |  3804 | `	}` |
|     59205 |  3805 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  3806 | `		/* Not so fatal,emit a warning only */` |
|       ! 0 |  3807 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|       ! 0 |  3808 | `	}` |
|         - |  3809 | `	/* Statement successfully compiled */` |
|     59205 |  3810 | `	return SXRET_OK;` |
|     29605 |  3811 | `}` |
|         - |  3812 | `/*` |
|         - |  3813 | ` * Compile or record a label.` |
|         - |  3814 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|         - |  3815 | ` * Example` |
|         - |  3816 | ` *  goto LABEL;` |
|         - |  3817 | ` *   echo 'Foo';` |
|         - |  3818 | ` *  LABEL:` |
|         - |  3819 | ` *   echo 'Bar';` |
|         - |  3820 | ` */` |
|       112 |  3821 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|         5 |  3822 | `{` |
|         - |  3823 | `	GenBlock *pBlock;` |
|         - |  3824 | `	Label sLabel;` |
|         - |  3825 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|       117 |  3826 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|       117 |  3827 | `	if( pBlock ){` |
|         - |  3828 | `		sxi32 rc;` |
|         8 |  3829 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         4 |  3830 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|         6 |  3831 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  3832 | `			return SXERR_ABORT;` |
|         - |  3833 | `		}` |
|         4 |  3834 | `	}else{` |
|       113 |  3835 | `		SyString *pTarget = &pGen->pIn->sData;` |
|         - |  3836 | `		char *zDup;` |
|         - |  3837 | `		/* Initialize label fields */` |
|       113 |  3838 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|         - |  3839 | `		/* Duplicate label name */` |
|       113 |  3840 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|       113 |  3841 | `		if( zDup == 0 ){` |
|       ! 0 |  3842 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  3843 | `			return SXERR_ABORT;` |
|         - |  3844 | `		}` |
|       113 |  3845 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|       113 |  3846 | `		sLabel.bRef  = FALSE;` |
|       113 |  3847 | `		sLabel.nLine = pGen->pIn->nLine;` |
|       113 |  3848 | `		pBlock = pGen->pCurrent;` |
|       221 |  3849 | `		while( pBlock ){` |
|       133 |  3850 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|        24 |  3851 | `				break;` |
|         - |  3852 | `			}` |
|         - |  3853 | `			/* Point to the upper block */` |
|       113 |  3854 | `			pBlock = pBlock->pParent;` |
|         5 |  3855 | `		}` |
|       113 |  3856 | `		if( pBlock ){` |
|        24 |  3857 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        14 |  3858 | `		}else{` |
|        93 |  3859 | `			sLabel.pFunc = 0;` |
|         - |  3860 | `		}` |
|         - |  3861 | `		/* Insert in label set */` |
|       113 |  3862 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|         - |  3863 | `	}` |
|       117 |  3864 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|       117 |  3865 | `	return SXRET_OK;` |
|        61 |  3866 | `}` |
|         - |  3867 | `/*` |
|         - |  3868 | ` * Compile the so hated 'goto' statement.` |
|         - |  3869 | ` * You've probably been taught that gotos are bad, but this sort` |
|         - |  3870 | ` * of rewriting  happens all the time, in fact every time you run` |
|         - |  3871 | ` * a compiler it has to do this.` |
|         - |  3872 | ` * According to the PHP language reference manual` |
|         - |  3873 | ` *   The goto operator can be used to jump to another section in the program.` |
|         - |  3874 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|         - |  3875 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|         - |  3876 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|         - |  3877 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|         - |  3878 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|         - |  3879 | ` *   of a multi-level break` |
|         - |  3880 | ` */` |
|       152 |  3881 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|         5 |  3882 | `{` |
|         - |  3883 | `	JumpFixup sJump;` |
|         - |  3884 | `	sxi32 rc;` |
|       157 |  3885 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|       157 |  3886 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  3887 | `		/* Missing label */` |
|       ! 0 |  3888 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|       ! 0 |  3889 | `		if( rc == SXERR_ABORT ){` |
|         - |  3890 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3891 | `			return SXERR_ABORT;` |
|         - |  3892 | `		}` |
|       ! 0 |  3893 | `		return SXRET_OK;` |
|         - |  3894 | `	}` |
|       157 |  3895 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|         5 |  3896 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|         5 |  3897 | `		if( rc == SXERR_ABORT ){` |
|         - |  3898 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  3899 | `			return SXERR_ABORT;` |
|         - |  3900 | `		}` |
|         3 |  3901 | `	}else{` |
|       153 |  3902 | `		SyString *pTarget = &pGen->pIn->sData;` |
|         - |  3903 | `		GenBlock *pBlock;` |
|         - |  3904 | `		char *zDup;` |
|         - |  3905 | `		/* Prepare the jump destination */` |
|       153 |  3906 | `		sJump.nJumpType = PH7_OP_JMP;` |
|       153 |  3907 | `		sJump.nLine = pGen->pIn->nLine;` |
|         - |  3908 | `		/* Duplicate label name */` |
|       153 |  3909 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|       153 |  3910 | `		if( zDup == 0 ){` |
|       ! 0 |  3911 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  3912 | `			return SXERR_ABORT;` |
|         - |  3913 | `		}` |
|       153 |  3914 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|       153 |  3915 | `		pBlock = pGen->pCurrent;` |
|       315 |  3916 | `		while( pBlock ){` |
|       199 |  3917 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|        36 |  3918 | `				break;` |
|         - |  3919 | `			}` |
|         - |  3920 | `			/* Point to the upper block */` |
|       167 |  3921 | `			pBlock = pBlock->pParent;` |
|         5 |  3922 | `		}` |
|       153 |  3923 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|         8 |  3924 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|         8 |  3925 | `			if( rc == SXERR_ABORT ){` |
|         - |  3926 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  3927 | `				return SXERR_ABORT;` |
|         - |  3928 | `			}` |
|         3 |  3929 | `		}` |
|       153 |  3930 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|        29 |  3931 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        16 |  3932 | `		}else{` |
|       127 |  3933 | `			sJump.pFunc = 0;` |
|         - |  3934 | `		}` |
|         - |  3935 | `		/* Emit the unconditional jump */` |
|       153 |  3936 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|       153 |  3937 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|        74 |  3938 | `		}` |
|         - |  3939 | `	}` |
|       157 |  3940 | `	pGen->pIn++; /* Jump the label name */` |
|       157 |  3941 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         3 |  3942 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|         1 |  3943 | `	}` |
|         - |  3944 | `	/* Statement successfully compiled */` |
|       157 |  3945 | `	return SXRET_OK;` |
|        81 |  3946 | `}` |
|         - |  3947 | `/*` |
|         - |  3948 | ` * Point to the next PHP chunk that will be processed shortly.` |
|         - |  3949 | ` * Return SXRET_OK on success. Any other return value indicates` |
|         - |  3950 | ` * failure.` |
|         - |  3951 | ` */` |
|        20 |  3952 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|         2 |  3953 | `{` |
|         - |  3954 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|         - |  3955 | `	sxu32 nRawObj;` |
|        10 |  3956 | `	sxu32 nObjIdx;` |
|         - |  3957 | `	/* Consume raw chunks verbatim without any processing until we get` |
|         - |  3958 | `	 * a PHP block.` |
|         - |  3959 | `	 */` |
|        10 |  3960 | `Consume:` |
|        22 |  3961 | `	nRawObj = nObjIdx = 0;` |
|        22 |  3962 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|       ! 0 |  3963 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|       ! 0 |  3964 | `		if( pRawObj == 0 ){` |
|       ! 0 |  3965 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  3966 | `			return SXERR_ABORT;` |
|         - |  3967 | `		}` |
|         - |  3968 | `		/* Mark as constant and emit the load constant instruction */` |
|       ! 0 |  3969 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|       ! 0 |  3970 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|       ! 0 |  3971 | `		++nRawObj;` |
|       ! 0 |  3972 | `		pGen->pRawIn++; /* Next chunk */` |
|       ! 0 |  3973 | `	}` |
|        22 |  3974 | `	if( nRawObj > 0 ){` |
|         - |  3975 | `		/* Emit the consume instruction */` |
|       ! 0 |  3976 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|       ! 0 |  3977 | `	}` |
|        22 |  3978 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|       ! 0 |  3979 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|         - |  3980 | `		/* Reset the token set (and its trivia sidecar) */` |
|       ! 0 |  3981 | `		SySetReset(pTokenSet);` |
|       ! 0 |  3982 | `		SySetReset(&pGen->aTrivia);` |
|         - |  3983 | `		/* Tokenize input */` |
|       ! 0 |  3984 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|       ! 0 |  3985 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|         - |  3986 | `		/* Point to the fresh token stream */` |
|       ! 0 |  3987 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|       ! 0 |  3988 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|         - |  3989 | `		/* Advance the stream cursor */` |
|       ! 0 |  3990 | `		pGen->pRawIn++;` |
|         - |  3991 | `		/* TICKET 1433-011 */` |
|       ! 0 |  3992 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - |  3993 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - |  3994 | `			sxi32 rc;` |
|         - |  3995 | `			/* Refer to TICKET 1433-009  */` |
|       ! 0 |  3996 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       ! 0 |  3997 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       ! 0 |  3998 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       ! 0 |  3999 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 |  4000 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4001 | `				return SXERR_ABORT;` |
|       ! 0 |  4002 | `			}else if( rc != SXERR_EMPTY ){` |
|       ! 0 |  4003 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       ! 0 |  4004 | `			}` |
|       ! 0 |  4005 | `			goto Consume;` |
|         - |  4006 | `		}` |
|       ! 0 |  4007 | `	}else{` |
|         - |  4008 | `		/* No more chunks to process */` |
|        22 |  4009 | `		pGen->pIn = pGen->pEnd;` |
|        22 |  4010 | `		return SXERR_EOF;` |
|         - |  4011 | `	}` |
|       ! 0 |  4012 | `	return SXRET_OK;` |
|        12 |  4013 | `}` |
|         - |  4014 | `/*` |
|         - |  4015 | ` * Compile a PHP block.` |
|         - |  4016 | ` * A block is simply one or more PHP statements and expressions to compile` |
|         - |  4017 | ` * optionally delimited by braces {}.` |
|         - |  4018 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  4019 | ` * and this function takes care of generating the appropriate error` |
|         - |  4020 | ` * message.` |
|         - |  4021 | ` */` |
|   5363062 |  4022 | `static sxi32 PH7_CompileBlock(` |
|         - |  4023 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  4024 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|         - |  4025 | `	)` |
|         5 |  4026 | `{` |
|         - |  4027 | `	sxi32 rc;` |
|         - |  4028 | `	sxu32 nLine;` |
|   5363067 |  4029 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|   5361573 |  4030 | `		nLine = pGen->pIn->nLine;` |
|   5361573 |  4031 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|   5361573 |  4032 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  4033 | `			return SXERR_ABORT;` |
|         - |  4034 | `		}` |
|   5361573 |  4035 | `		pGen->pIn++;` |
|         - |  4036 | `		/* Compile until we hit the closing braces '}' */` |
|   7843544 |  4037 | `		for(;;){` |
|  15687093 |  4038 | `			if( pGen->pIn >= pGen->pEnd ){` |
|        22 |  4039 | `				rc = GenStateNextChunk(&(*pGen));` |
|        22 |  4040 | `				if (rc == SXERR_ABORT ){` |
|       ! 0 |  4041 | `			 	   return SXERR_ABORT;` |
|         - |  4042 | `				}` |
|        22 |  4043 | `				if( rc == SXERR_EOF ){` |
|         - |  4044 | `					/* No more token to process. Missing closing braces */` |
|        22 |  4045 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|        22 |  4046 | `					break;` |
|         - |  4047 | `				}` |
|       ! 0 |  4048 | `			}` |
|  15687073 |  4049 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|         - |  4050 | `				/* Closing braces found,break immediately*/` |
|   5361553 |  4051 | `				pGen->pIn++;` |
|   5361553 |  4052 | `				break;` |
|         - |  4053 | `			}` |
|         - |  4054 | `			/* Compile a single statement */` |
|  10325525 |  4055 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  10325525 |  4056 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4057 | `				return SXERR_ABORT;` |
|         - |  4058 | `			}` |
|         5 |  4059 | `		}` |
|   5361573 |  4060 | `		GenStateLeaveBlock(&(*pGen),0);` |
|   2682283 |  4061 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|       ! 0 |  4062 | `		pGen->pIn++;` |
|       ! 0 |  4063 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|       ! 0 |  4064 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  4065 | `			return SXERR_ABORT;` |
|         - |  4066 | `		}` |
|         - |  4067 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|       ! 0 |  4068 | `		for(;;){` |
|       ! 0 |  4069 | `			if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  4070 | `				rc = GenStateNextChunk(&(*pGen));` |
|       ! 0 |  4071 | `				if (rc == SXERR_ABORT ){` |
|       ! 0 |  4072 | `			 	   return SXERR_ABORT;` |
|         - |  4073 | `				}` |
|       ! 0 |  4074 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|         - |  4075 | `					/* No more token to process */` |
|       ! 0 |  4076 | `					if( rc == SXERR_EOF ){` |
|       ! 0 |  4077 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|         - |  4078 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|       ! 0 |  4079 | `					}` |
|       ! 0 |  4080 | `					break;` |
|         - |  4081 | `				}` |
|       ! 0 |  4082 | `			}` |
|       ! 0 |  4083 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|         - |  4084 | `				sxi32 nKwrd;` |
|         - |  4085 | `				/* Keyword found */` |
|       ! 0 |  4086 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       ! 0 |  4087 | `				if( nKwrd == nKeywordEnd \|\|` |
|       ! 0 |  4088 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|         - |  4089 | `						/* Delimiter keyword found,break */` |
|       ! 0 |  4090 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|       ! 0 |  4091 | `							pGen->pIn++; /*  endif;endswitch... */` |
|       ! 0 |  4092 | `						}` |
|       ! 0 |  4093 | `						break;` |
|         - |  4094 | `				}` |
|       ! 0 |  4095 | `			}` |
|         - |  4096 | `			/* Compile a single statement */` |
|       ! 0 |  4097 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|       ! 0 |  4098 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4099 | `				return SXERR_ABORT;` |
|         - |  4100 | `			}` |
|       ! 0 |  4101 | `		}` |
|       ! 0 |  4102 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  4103 | `	}else{` |
|         - |  4104 | `		/* Compile a single statement */` |
|      1499 |  4105 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|      1499 |  4106 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4107 | `			return SXERR_ABORT;` |
|         - |  4108 | `		}` |
|         - |  4109 | `	}` |
|         - |  4110 | `	/* Jump trailing semi-colons ';' */` |
|   5363067 |  4111 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       ! 0 |  4112 | `		pGen->pIn++;` |
|       ! 0 |  4113 | `	}` |
|   5363067 |  4114 | `	return SXRET_OK;` |
|   2681536 |  4115 | `}` |
|         - |  4116 | `/*` |
|         - |  4117 | ` * Compile the gentle 'while' statement.` |
|         - |  4118 | ` * According to the PHP language reference` |
|         - |  4119 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|         - |  4120 | ` *  The basic form of a while statement is:` |
|         - |  4121 | ` *  while (expr)` |
|         - |  4122 | ` *   statement` |
|         - |  4123 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|         - |  4124 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|         - |  4125 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|         - |  4126 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|         - |  4127 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|         - |  4128 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|         - |  4129 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|         - |  4130 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|         - |  4131 | ` *  while (expr):` |
|         - |  4132 | ` *    statement` |
|         - |  4133 | ` *   endwhile;` |
|         - |  4134 | ` */` |
|     55274 |  4135 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|         5 |  4136 | `{` |
|     55279 |  4137 | `	GenBlock *pWhileBlock = 0;` |
|     55279 |  4138 | `	SyToken *pTmp,*pEnd = 0;` |
|         - |  4139 | `	sxu32 nFalseJump;` |
|         - |  4140 | `	sxu32 nLine;` |
|         - |  4141 | `	sxi32 rc;` |
|     55279 |  4142 | `	nLine = pGen->pIn->nLine;` |
|         - |  4143 | `	/* Jump the 'while' keyword */` |
|     55279 |  4144 | `	pGen->pIn++;` |
|     55279 |  4145 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4146 | `		/* Syntax error */` |
|       ! 0 |  4147 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|       ! 0 |  4148 | `		if( rc == SXERR_ABORT ){` |
|         - |  4149 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4150 | `			return SXERR_ABORT;` |
|         - |  4151 | `		}` |
|       ! 0 |  4152 | `		goto Synchronize;` |
|         - |  4153 | `	}` |
|         - |  4154 | `	/* Jump the left parenthesis '(' */` |
|     55279 |  4155 | `	pGen->pIn++;` |
|         - |  4156 | `	/* Create the loop block */` |
|     55279 |  4157 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|     55279 |  4158 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4159 | `		return SXERR_ABORT;` |
|         - |  4160 | `	}` |
|         - |  4161 | `	/* Delimit the condition */` |
|     55279 |  4162 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     55279 |  4163 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4164 | `		/* Empty expression */` |
|         3 |  4165 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|         3 |  4166 | `		if( rc == SXERR_ABORT ){` |
|         - |  4167 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4168 | `			return SXERR_ABORT;` |
|         - |  4169 | `		}` |
|         1 |  4170 | `	}` |
|         - |  4171 | `	/* Swap token streams */` |
|     55279 |  4172 | `	pTmp = pGen->pEnd;` |
|     55279 |  4173 | `	pGen->pEnd = pEnd;` |
|         - |  4174 | `	/* Compile the expression */` |
|     55279 |  4175 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     55279 |  4176 | `	if( rc == SXERR_ABORT ){` |
|         - |  4177 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4178 | `		return SXERR_ABORT;` |
|         - |  4179 | `	}` |
|         - |  4180 | `	/* Update token stream */` |
|     55279 |  4181 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 |  4182 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  4183 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4184 | `			return SXERR_ABORT;` |
|         - |  4185 | `		}` |
|       ! 0 |  4186 | `		pGen->pIn++;` |
|       ! 0 |  4187 | `	}` |
|         - |  4188 | `	/* Synchronize pointers */` |
|     55279 |  4189 | `	pGen->pIn  = &pEnd[1];` |
|     55279 |  4190 | `	pGen->pEnd = pTmp;` |
|         - |  4191 | `	/* Emit the false jump */` |
|     55279 |  4192 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|         - |  4193 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|     55279 |  4194 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|         - |  4195 | `	/* Compile the loop body */` |
|     55279 |  4196 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|     55279 |  4197 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4198 | `		return SXERR_ABORT;` |
|         - |  4199 | `	}` |
|         - |  4200 | `	/* Emit the unconditional jump to the start of the loop */` |
|     55279 |  4201 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|         - |  4202 | `	/* Fix all jumps now the destination is resolved */` |
|     55279 |  4203 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4204 | `	/* Release the loop block */` |
|     55279 |  4205 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4206 | `	/* Statement successfully compiled */` |
|     55279 |  4207 | `	return SXRET_OK;` |
|       ! 0 |  4208 | `Synchronize:` |
|         - |  4209 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4210 | `	 * compiling this erroneous block.` |
|         - |  4211 | `	 */` |
|       ! 0 |  4212 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4213 | `		pGen->pIn++;` |
|       ! 0 |  4214 | `	}` |
|       ! 0 |  4215 | `	return SXRET_OK;` |
|     27642 |  4216 | `}` |
|         - |  4217 | `/*` |
|         - |  4218 | ` * Compile the ugly do..while() statement.` |
|         - |  4219 | ` * According to the PHP language reference` |
|         - |  4220 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|         - |  4221 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|         - |  4222 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|         - |  4223 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|         - |  4224 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|         - |  4225 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|         - |  4226 | ` *  would end immediately).` |
|         - |  4227 | ` *  There is just one syntax for do-while loops:` |
|         - |  4228 | ` *  <?php` |
|         - |  4229 | ` *  $i = 0;` |
|         - |  4230 | ` *  do {` |
|         - |  4231 | ` *   echo $i;` |
|         - |  4232 | ` *  } while ($i > 0);` |
|         - |  4233 | ` * ?>` |
|         - |  4234 | ` */` |
|         2 |  4235 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|         1 |  4236 | `{` |
|         3 |  4237 | `	SyToken *pTmp,*pEnd = 0;` |
|         3 |  4238 | `	GenBlock *pDoBlock = 0;` |
|         - |  4239 | `	sxu32 nLine;` |
|         - |  4240 | `	sxi32 rc;` |
|         3 |  4241 | `	nLine = pGen->pIn->nLine;` |
|         - |  4242 | `	/* Jump the 'do' keyword */` |
|         3 |  4243 | `	pGen->pIn++;` |
|         - |  4244 | `	/* Create the loop block */` |
|         3 |  4245 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|         3 |  4246 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4247 | `		return SXERR_ABORT;` |
|         - |  4248 | `	}` |
|         - |  4249 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|         3 |  4250 | `	pDoBlock->bPostContinue = TRUE;` |
|         3 |  4251 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|         3 |  4252 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4253 | `		return SXERR_ABORT;` |
|         - |  4254 | `	}` |
|         3 |  4255 | `	if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4256 | `		nLine = pGen->pIn->nLine;` |
|       ! 0 |  4257 | `	}` |
|         3 |  4258 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|       ! 0 |  4259 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|         - |  4260 | `			/* Missing 'while' statement */` |
|         3 |  4261 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|         3 |  4262 | `			if( rc == SXERR_ABORT ){` |
|         - |  4263 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4264 | `				return SXERR_ABORT;` |
|         - |  4265 | `			}` |
|         3 |  4266 | `			goto Synchronize;` |
|         - |  4267 | `	}` |
|         - |  4268 | `	/* Jump the 'while' keyword */` |
|       ! 0 |  4269 | `	pGen->pIn++;` |
|       ! 0 |  4270 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4271 | `		/* Syntax error */` |
|       ! 0 |  4272 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|       ! 0 |  4273 | `		if( rc == SXERR_ABORT ){` |
|         - |  4274 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4275 | `			return SXERR_ABORT;` |
|         - |  4276 | `		}` |
|       ! 0 |  4277 | `		goto Synchronize;` |
|         - |  4278 | `	}` |
|         - |  4279 | `	/* Jump the left parenthesis '(' */` |
|       ! 0 |  4280 | `	pGen->pIn++;` |
|         - |  4281 | `	/* Delimit the condition */` |
|       ! 0 |  4282 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|       ! 0 |  4283 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4284 | `		/* Empty expression */` |
|       ! 0 |  4285 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       ! 0 |  4286 | `		if( rc == SXERR_ABORT ){` |
|         - |  4287 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4288 | `			return SXERR_ABORT;` |
|         - |  4289 | `		}` |
|       ! 0 |  4290 | `		goto Synchronize;` |
|         - |  4291 | `	}` |
|         - |  4292 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|       ! 0 |  4293 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|         - |  4294 | `		JumpFixup *aPost;` |
|         - |  4295 | `		VmInstr *pInstr;` |
|         - |  4296 | `		sxu32 nJumpDest;` |
|         - |  4297 | `		sxu32 n;` |
|       ! 0 |  4298 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|       ! 0 |  4299 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       ! 0 |  4300 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|       ! 0 |  4301 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|       ! 0 |  4302 | `			if( pInstr ){` |
|         - |  4303 | `				/* Fix */` |
|       ! 0 |  4304 | `				pInstr->iP2 = nJumpDest;` |
|       ! 0 |  4305 | `			}` |
|       ! 0 |  4306 | `		}` |
|       ! 0 |  4307 | `	}` |
|         - |  4308 | `	/* Swap token streams */` |
|       ! 0 |  4309 | `	pTmp = pGen->pEnd;` |
|       ! 0 |  4310 | `	pGen->pEnd = pEnd;` |
|         - |  4311 | `	/* Compile the expression */` |
|       ! 0 |  4312 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       ! 0 |  4313 | `	if( rc == SXERR_ABORT ){` |
|         - |  4314 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4315 | `		return SXERR_ABORT;` |
|         - |  4316 | `	}` |
|         - |  4317 | `	/* Update token stream */` |
|       ! 0 |  4318 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 |  4319 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  4320 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4321 | `			return SXERR_ABORT;` |
|         - |  4322 | `		}` |
|       ! 0 |  4323 | `		pGen->pIn++;` |
|       ! 0 |  4324 | `	}` |
|       ! 0 |  4325 | `	pGen->pIn  = &pEnd[1];` |
|       ! 0 |  4326 | `	pGen->pEnd = pTmp;` |
|         - |  4327 | `	/* Emit the true jump to the beginning of the loop */` |
|       ! 0 |  4328 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|         - |  4329 | `	/* Fix all jumps now the destination is resolved */` |
|       ! 0 |  4330 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4331 | `	/* Release the loop block */` |
|       ! 0 |  4332 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4333 | `	/* Statement successfully compiled */` |
|       ! 0 |  4334 | `	return SXRET_OK;` |
|         1 |  4335 | `Synchronize:` |
|         - |  4336 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4337 | `	 * compiling this erroneous block.` |
|         - |  4338 | `	 */` |
|         3 |  4339 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4340 | `		pGen->pIn++;` |
|       ! 0 |  4341 | `	}` |
|         3 |  4342 | `	return SXRET_OK;` |
|         2 |  4343 | `}` |
|         - |  4344 | `/*` |
|         - |  4345 | ` * Compile the complex and powerful 'for' statement.` |
|         - |  4346 | ` * According to the PHP language reference` |
|         - |  4347 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|         - |  4348 | ` *  The syntax of a for loop is:` |
|         - |  4349 | ` *  for (expr1; expr2; expr3)` |
|         - |  4350 | ` *   statement` |
|         - |  4351 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|         - |  4352 | ` *  the beginning of the loop.` |
|         - |  4353 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|         - |  4354 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|         - |  4355 | ` *  to FALSE, the execution of the loop ends.` |
|         - |  4356 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|         - |  4357 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|         - |  4358 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|         - |  4359 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|         - |  4360 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|         - |  4361 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|         - |  4362 | ` *  of using the for truth expression.` |
|         - |  4363 | ` */` |
|     90714 |  4364 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|         5 |  4365 | `{` |
|     90719 |  4366 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|     90719 |  4367 | `	GenBlock *pForBlock = 0;` |
|         - |  4368 | `	sxu32 nFalseJump;` |
|         - |  4369 | `	sxu32 nLine;` |
|         - |  4370 | `	sxi32 rc;` |
|     90719 |  4371 | `	nLine = pGen->pIn->nLine;` |
|         - |  4372 | `	/* Jump the 'for' keyword */` |
|     90719 |  4373 | `	pGen->pIn++;` |
|     90719 |  4374 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4375 | `		/* Syntax error */` |
|       ! 0 |  4376 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|       ! 0 |  4377 | `		if( rc == SXERR_ABORT ){` |
|         - |  4378 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4379 | `			return SXERR_ABORT;` |
|         - |  4380 | `		}` |
|       ! 0 |  4381 | `		return SXRET_OK;` |
|         - |  4382 | `	}` |
|         - |  4383 | `	/* Jump the left parenthesis '(' */` |
|     90719 |  4384 | `	pGen->pIn++;` |
|         - |  4385 | `	/* Delimit the init-expr;condition;post-expr */` |
|     90719 |  4386 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     90719 |  4387 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4388 | `		/* Empty expression */` |
|       ! 0 |  4389 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|       ! 0 |  4390 | `		if( rc == SXERR_ABORT ){` |
|         - |  4391 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4392 | `			return SXERR_ABORT;` |
|         - |  4393 | `		}` |
|         - |  4394 | `		/* Synchronize */` |
|       ! 0 |  4395 | `		pGen->pIn = pEnd;` |
|       ! 0 |  4396 | `		if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4397 | `			pGen->pIn++;` |
|       ! 0 |  4398 | `		}` |
|       ! 0 |  4399 | `		return SXRET_OK;` |
|         - |  4400 | `	}` |
|         - |  4401 | `	/* Swap token streams */` |
|     90719 |  4402 | `	pTmp = pGen->pEnd;` |
|     90719 |  4403 | `	pGen->pEnd = pEnd;` |
|         - |  4404 | `	/* Compile initialization expressions if available */` |
|     90719 |  4405 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  4406 | `	/* Pop operand lvalues */` |
|     90719 |  4407 | `	if( rc == SXERR_ABORT ){` |
|         - |  4408 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4409 | `		return SXERR_ABORT;` |
|     90719 |  4410 | `	}else if( rc != SXERR_EMPTY ){` |
|     78903 |  4411 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     39449 |  4412 | `	}` |
|     90719 |  4413 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4414 | `		/* Syntax error */` |
|       ! 0 |  4415 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  4416 | `			"for: Expected ';' after initialization expressions");` |
|       ! 0 |  4417 | `		if( rc == SXERR_ABORT ){` |
|         - |  4418 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4419 | `			return SXERR_ABORT;` |
|         - |  4420 | `		}` |
|       ! 0 |  4421 | `		return SXRET_OK;` |
|         - |  4422 | `	}` |
|         - |  4423 | `	/* Jump the trailing ';' */` |
|     90719 |  4424 | `	pGen->pIn++;` |
|         - |  4425 | `	/* Create the loop block */` |
|     90719 |  4426 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|     90719 |  4427 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4428 | `		return SXERR_ABORT;` |
|         - |  4429 | `	}` |
|         - |  4430 | `	/* Deffer continue jumps */` |
|     90719 |  4431 | `	pForBlock->bPostContinue = TRUE;` |
|         - |  4432 | `	/* Compile the condition */` |
|     90719 |  4433 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     90719 |  4434 | `	if( rc == SXERR_ABORT ){` |
|         - |  4435 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4436 | `		return SXERR_ABORT;` |
|     90719 |  4437 | `	}else if( rc != SXERR_EMPTY ){` |
|         - |  4438 | `		/* Emit the false jump */` |
|     78903 |  4439 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|         - |  4440 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|     78903 |  4441 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|     39449 |  4442 | `	}` |
|     90719 |  4443 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  4444 | `		/* Syntax error */` |
|         6 |  4445 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  4446 | `			"for: Expected ';' after conditionals expressions");` |
|         6 |  4447 | `		if( rc == SXERR_ABORT ){` |
|         - |  4448 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4449 | `			return SXERR_ABORT;` |
|         - |  4450 | `		}` |
|         6 |  4451 | `		return SXRET_OK;` |
|         - |  4452 | `	}` |
|         - |  4453 | `	/* Jump the trailing ';' */` |
|     90715 |  4454 | `	pGen->pIn++;` |
|         - |  4455 | `	/* Save the post condition stream */` |
|     90715 |  4456 | `	pPostStart = pGen->pIn;` |
|         - |  4457 | `	/* Compile the loop body */` |
|     90715 |  4458 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|     90715 |  4459 | `	pGen->pEnd = pTmp;` |
|     90715 |  4460 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|     90715 |  4461 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  4462 | `		return SXERR_ABORT;` |
|         - |  4463 | `	}` |
|         - |  4464 | `	/* Fix post-continue jumps */` |
|     90715 |  4465 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|         - |  4466 | `		JumpFixup *aPost;` |
|         - |  4467 | `		VmInstr *pInstr;` |
|         - |  4468 | `		sxu32 nJumpDest;` |
|         - |  4469 | `		sxu32 n;` |
|      7893 |  4470 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      7893 |  4471 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     27595 |  4472 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|     19707 |  4473 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     19707 |  4474 | `			if( pInstr ){` |
|         - |  4475 | `				/* Fix jump */` |
|     19707 |  4476 | `				pInstr->iP2 = nJumpDest;` |
|      9851 |  4477 | `			}` |
|      9856 |  4478 | `		}` |
|      3944 |  4479 | `	}` |
|         - |  4480 | `	/* compile the post-expressions if available */` |
|     90715 |  4481 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|       ! 0 |  4482 | `		pPostStart++;` |
|       ! 0 |  4483 | `	}` |
|     90715 |  4484 | `	if( pPostStart < pEnd ){` |
|         - |  4485 | `		SyToken *pTmpIn,*pTmpEnd;` |
|     78901 |  4486 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|     78901 |  4487 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     78901 |  4488 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - |  4489 | `			/* Syntax error */` |
|       ! 0 |  4490 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|       ! 0 |  4491 | `			if( rc == SXERR_ABORT ){` |
|         - |  4492 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4493 | `				return SXERR_ABORT;` |
|         - |  4494 | `			}` |
|       ! 0 |  4495 | `			return SXRET_OK;` |
|         - |  4496 | `		}` |
|     78901 |  4497 | `		RE_SWAP_DELIMITER(pGen);` |
|     78901 |  4498 | `		if( rc == SXERR_ABORT ){` |
|         - |  4499 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4500 | `			return SXERR_ABORT;` |
|     78901 |  4501 | `		}else if( rc != SXERR_EMPTY){` |
|         - |  4502 | `			/* Pop operand lvalue */` |
|     78901 |  4503 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     39448 |  4504 | `		}` |
|     39448 |  4505 | `	}` |
|         - |  4506 | `	/* Emit the unconditional jump to the start of the loop */` |
|     90715 |  4507 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|         - |  4508 | `	/* Fix all jumps now the destination is resolved */` |
|     90715 |  4509 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4510 | `	/* Release the loop block */` |
|     90715 |  4511 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4512 | `	/* Statement successfully compiled */` |
|     90715 |  4513 | `	return SXRET_OK;` |
|     45362 |  4514 | `}` |
|         - |  4515 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|         - |  4516 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|         - |  4517 | ` * are allowed.` |
|         - |  4518 | ` */` |
|    331780 |  4519 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 |  4520 | `{` |
|    331785 |  4521 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    331785 |  4522 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|         - |  4523 | `		/* Unexpected expression */` |
|       ! 0 |  4524 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - |  4525 | `			"foreach: Expecting a variable name");` |
|       ! 0 |  4526 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 |  4527 | `			rc = SXERR_INVALID;` |
|       ! 0 |  4528 | `		}` |
|       ! 0 |  4529 | `	}` |
|    331785 |  4530 | `	return rc;` |
|         5 |  4531 | `}` |
|         - |  4532 | `/*` |
|         - |  4533 | ` * Compile the 'foreach' statement.` |
|         - |  4534 | ` * According to the PHP language reference` |
|         - |  4535 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|         - |  4536 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|         - |  4537 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|         - |  4538 | ` *  is a minor but useful extension of the first:` |
|         - |  4539 | ` *  foreach (array_expression as $value)` |
|         - |  4540 | ` *    statement` |
|         - |  4541 | ` *  foreach (array_expression as $key => $value)` |
|         - |  4542 | ` *   statement` |
|         - |  4543 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|         - |  4544 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|         - |  4545 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|         - |  4546 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|         - |  4547 | ` *  to the variable $key on each loop.` |
|         - |  4548 | ` *  Note:` |
|         - |  4549 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|         - |  4550 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|         - |  4551 | ` *  Note:` |
|         - |  4552 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|         - |  4553 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|         - |  4554 | ` *  or after the foreach without resetting it.` |
|         - |  4555 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|         - |  4556 | ` *  of copying the value.` |
|         - |  4557 | ` */` |
|    229118 |  4558 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|         5 |  4559 | `{` |
|    229123 |  4560 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    229123 |  4561 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    229123 |  4562 | `	GenBlock *pForeachBlock = 0;` |
|         - |  4563 | `	ph7_foreach_info *pInfo;` |
|         - |  4564 | `	sxu32 nFalseJump;` |
|         - |  4565 | `	VmInstr *pInstr;` |
|         - |  4566 | `	sxu32 nLine;` |
|         - |  4567 | `	sxi32 rc;` |
|    229123 |  4568 | `	nLine = pGen->pIn->nLine;` |
|         - |  4569 | `	/* Jump the 'foreach' keyword */` |
|    229123 |  4570 | `	pGen->pIn++;` |
|    229123 |  4571 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4572 | `		/* Syntax error */` |
|       ! 0 |  4573 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|       ! 0 |  4574 | `		if( rc == SXERR_ABORT ){` |
|         - |  4575 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4576 | `			return SXERR_ABORT;` |
|         - |  4577 | `		}` |
|       ! 0 |  4578 | `		goto Synchronize;` |
|         - |  4579 | `	}` |
|         - |  4580 | `	/* Jump the left parenthesis '(' */` |
|    229123 |  4581 | `	pGen->pIn++;` |
|         - |  4582 | `	/* Create the loop block */` |
|    229123 |  4583 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    229123 |  4584 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4585 | `		return SXERR_ABORT;` |
|         - |  4586 | `	}` |
|         - |  4587 | `	/* Delimit the expression */` |
|    229123 |  4588 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    229123 |  4589 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - |  4590 | `		/* Empty expression */` |
|       ! 0 |  4591 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|       ! 0 |  4592 | `		if( rc == SXERR_ABORT ){` |
|         - |  4593 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  4594 | `			return SXERR_ABORT;` |
|         - |  4595 | `		}` |
|         - |  4596 | `		/* Synchronize */` |
|       ! 0 |  4597 | `		pGen->pIn = pEnd;` |
|       ! 0 |  4598 | `		if( pGen->pIn < pGen->pEnd ){` |
|       ! 0 |  4599 | `			pGen->pIn++;` |
|       ! 0 |  4600 | `		}` |
|       ! 0 |  4601 | `		return SXRET_OK;` |
|         - |  4602 | `	}` |
|         - |  4603 | `	/* Compile the array expression */` |
|    229123 |  4604 | `	pCur = pGen->pIn;` |
|   1236591 |  4605 | `	while( pCur < pEnd ){` |
|   1236591 |  4606 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    240951 |  4607 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    240951 |  4608 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|         - |  4609 | `				/* Break with the first 'as' found */` |
|    229123 |  4610 | `				break;` |
|         - |  4611 | `			}` |
|      5914 |  4612 | `		}` |
|         - |  4613 | `		/* Advance the stream cursor */` |
|   1007473 |  4614 | `		pCur++;` |
|         5 |  4615 | `	}` |
|    229123 |  4616 | `	if( pCur <= pGen->pIn ){` |
|       ! 0 |  4617 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - |  4618 | `			"foreach: Missing array/object expression");` |
|       ! 0 |  4619 | `		if( rc == SXERR_ABORT ){` |
|         - |  4620 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4621 | `			return SXERR_ABORT;` |
|         - |  4622 | `		}` |
|       ! 0 |  4623 | `		goto Synchronize;` |
|         - |  4624 | `	}` |
|         - |  4625 | `	/* Swap token streams */` |
|    229123 |  4626 | `	pTmp = pGen->pEnd;` |
|    229123 |  4627 | `	pGen->pEnd = pCur;` |
|    229123 |  4628 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    229123 |  4629 | `	if( rc == SXERR_ABORT ){` |
|         - |  4630 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4631 | `		return SXERR_ABORT;` |
|         - |  4632 | `	}` |
|         - |  4633 | `	/* Update token stream */` |
|    229123 |  4634 | `	while(pGen->pIn < pCur ){` |
|       ! 0 |  4635 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  4636 | `		if( rc == SXERR_ABORT ){` |
|         - |  4637 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4638 | `			return SXERR_ABORT;` |
|         - |  4639 | `		}` |
|       ! 0 |  4640 | `		pGen->pIn++;` |
|       ! 0 |  4641 | `	}` |
|    229123 |  4642 | `	pCur++; /* Jump the 'as' keyword */` |
|    229123 |  4643 | `	pGen->pIn = pCur;` |
|    229123 |  4644 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 |  4645 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|       ! 0 |  4646 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4647 | `			return SXERR_ABORT;` |
|         - |  4648 | `		}` |
|       ! 0 |  4649 | `	}` |
|         - |  4650 | `	/* Create the foreach context */` |
|    229123 |  4651 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    229123 |  4652 | `	if( pInfo == 0 ){` |
|       ! 0 |  4653 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 |  4654 | `		return SXERR_ABORT;` |
|         - |  4655 | `	}` |
|         - |  4656 | `	/* Zero the structure */` |
|    229123 |  4657 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|         - |  4658 | `	/* Initialize structure fields */` |
|    229123 |  4659 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|         - |  4660 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|         - |  4661 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|         - |  4662 | `	 * '=>'. */` |
|    229123 |  4663 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    229123 |  4664 | `	if( pCur < pEnd ){` |
|         - |  4665 | `		/* Compile the expression holding the key name */` |
|    102687 |  4666 | `		if( pGen->pIn >= pCur ){` |
|       ! 0 |  4667 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|       ! 0 |  4668 | `			if( rc == SXERR_ABORT ){` |
|         - |  4669 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4670 | `				return SXERR_ABORT;` |
|         - |  4671 | `			}` |
|       ! 0 |  4672 | `		}else{` |
|    102687 |  4673 | `			pGen->pEnd = pCur;` |
|    102687 |  4674 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    102687 |  4675 | `			if( rc == SXERR_ABORT ){` |
|         - |  4676 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4677 | `				return SXERR_ABORT;` |
|         - |  4678 | `			}` |
|    102687 |  4679 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    102687 |  4680 | `			if( pInstr->p3 ){` |
|         - |  4681 | `				/* Record key name */` |
|    102687 |  4682 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|     51341 |  4683 | `			}` |
|    102687 |  4684 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|         - |  4685 | `		}` |
|    102687 |  4686 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|     51341 |  4687 | `	}` |
|    229123 |  4688 | `	pGen->pEnd = pEnd;` |
|    229123 |  4689 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 |  4690 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|       ! 0 |  4691 | `		if( rc == SXERR_ABORT ){` |
|         - |  4692 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4693 | `			return SXERR_ABORT;` |
|         - |  4694 | `		}` |
|       ! 0 |  4695 | `		goto Synchronize;` |
|         - |  4696 | `	}` |
|    229123 |  4697 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|        33 |  4698 | `		pGen->pIn++;` |
|         - |  4699 | `		/* Pass by reference  */` |
|        33 |  4700 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|        15 |  4701 | `	}` |
|         - |  4702 | `	/* Check if the value target is list() */` |
|    229123 |  4703 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         8 |  4704 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|         - |  4705 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|         - |  4706 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|         - |  4707 | `		 */` |
|         - |  4708 | `		static int iForeachListCnt = 0;` |
|         - |  4709 | `		char zTmp[128];` |
|         - |  4710 | `		sxu32 nLen;` |
|         - |  4711 | `		char *zDup;` |
|        10 |  4712 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|        10 |  4713 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|        10 |  4714 | `		if( zDup == 0 ){` |
|       ! 0 |  4715 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  4716 | `			return SXERR_ABORT;` |
|         - |  4717 | `		}` |
|        10 |  4718 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|         - |  4719 | `		/* Save list() token boundaries */` |
|        10 |  4720 | `		pListStart = pGen->pIn;` |
|         - |  4721 | `		/* Advance past list(...) — validate parentheses */` |
|        10 |  4722 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|        10 |  4723 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         3 |  4724 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|         - |  4725 | `				"foreach: Expected '(' after 'list'");` |
|         3 |  4726 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4727 | `				return SXERR_ABORT;` |
|         - |  4728 | `			}` |
|         3 |  4729 | `			goto Synchronize;` |
|         - |  4730 | `		}` |
|         7 |  4731 | `		pGen->pIn++; /* Jump '(' */` |
|         7 |  4732 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|         7 |  4733 | `		if( pListEnd >= pEnd ){` |
|       ! 0 |  4734 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  4735 | `				"foreach: Missing closing ')' after list");` |
|       ! 0 |  4736 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4737 | `				return SXERR_ABORT;` |
|         - |  4738 | `			}` |
|       ! 0 |  4739 | `			goto Synchronize;` |
|         - |  4740 | `		}` |
|         7 |  4741 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|         7 |  4742 | `		pListEnd = pGen->pIn;` |
|         7 |  4743 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    229118 |  4744 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|         - |  4745 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|         - |  4746 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|         - |  4747 | `		 */` |
|         - |  4748 | `		static int iForeachShortListCnt = 0;` |
|         - |  4749 | `		char zTmp[128];` |
|         - |  4750 | `		sxu32 nLen;` |
|         - |  4751 | `		char *zDup;` |
|        13 |  4752 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|        13 |  4753 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|        13 |  4754 | `		if( zDup == 0 ){` |
|       ! 0 |  4755 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  4756 | `			return SXERR_ABORT;` |
|         - |  4757 | `		}` |
|        13 |  4758 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|         - |  4759 | `		/* Save [...] token boundaries */` |
|        13 |  4760 | `		pListStart = pGen->pIn;` |
|         - |  4761 | `		/* Advance past [...] */` |
|        13 |  4762 | `		pGen->pIn++; /* Jump '[' */` |
|        13 |  4763 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|        13 |  4764 | `		if( pListEnd >= pEnd ){` |
|       ! 0 |  4765 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  4766 | `				"foreach: Missing closing ']' after short list");` |
|       ! 0 |  4767 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  4768 | `				return SXERR_ABORT;` |
|         - |  4769 | `			}` |
|       ! 0 |  4770 | `			goto Synchronize;` |
|         - |  4771 | `		}` |
|        13 |  4772 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|        13 |  4773 | `		pListEnd = pGen->pIn;` |
|        13 |  4774 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|         7 |  4775 | `	}else{` |
|         - |  4776 | `		/* Compile the expression holding the value name */` |
|    229103 |  4777 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    229103 |  4778 | `		if( rc == SXERR_ABORT ){` |
|         - |  4779 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4780 | `			return SXERR_ABORT;` |
|         - |  4781 | `		}` |
|    229103 |  4782 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    229103 |  4783 | `		if( pInstr->p3 ){` |
|         - |  4784 | `			/* Record value name */` |
|    229103 |  4785 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    114549 |  4786 | `		}` |
|         - |  4787 | `	}` |
|         - |  4788 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    229121 |  4789 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|         - |  4790 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    229121 |  4791 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|         - |  4792 | `	/* Record the first instruction to execute */` |
|    229121 |  4793 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|         - |  4794 | `	/* Emit the FOREACH_STEP instruction */` |
|    229121 |  4795 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|         - |  4796 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    229121 |  4797 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|         - |  4798 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    229121 |  4799 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|         - |  4800 | `		SyToken *pSavedIn,*pSavedEnd;` |
|         - |  4801 | `		/* Load the temporary variable holding the current value onto the stack.` |
|         - |  4802 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|         - |  4803 | `		 */` |
|        19 |  4804 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|         - |  4805 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|         - |  4806 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|         - |  4807 | `		 * picks up the delimiter and the variable names inside.` |
|         - |  4808 | `		 */` |
|        19 |  4809 | `		pSavedIn = pGen->pIn;` |
|        19 |  4810 | `		pSavedEnd = pGen->pEnd;` |
|        19 |  4811 | `		pGen->pIn = pListStart;` |
|        19 |  4812 | `		pGen->pEnd = pListEnd;` |
|        19 |  4813 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|        13 |  4814 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|         7 |  4815 | `		}else{` |
|         7 |  4816 | `			rc = PH7_CompileList(&(*pGen),0);` |
|         - |  4817 | `		}` |
|        19 |  4818 | `		pGen->pIn = pSavedIn;` |
|        19 |  4819 | `		pGen->pEnd = pSavedEnd;` |
|        19 |  4820 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4821 | `			return SXERR_ABORT;` |
|         - |  4822 | `		}` |
|         - |  4823 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|        19 |  4824 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         9 |  4825 | `	}` |
|         - |  4826 | `	/* Compile the loop body */` |
|    229121 |  4827 | `	pGen->pIn = &pEnd[1];` |
|    229121 |  4828 | `	pGen->pEnd = pTmp;` |
|    229121 |  4829 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    229121 |  4830 | `	if( rc == SXERR_ABORT ){` |
|         - |  4831 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4832 | `		return SXERR_ABORT;` |
|         - |  4833 | `	}` |
|         - |  4834 | `	/* Emit the unconditional jump to the start of the loop */` |
|    229121 |  4835 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|         - |  4836 | `	/* Fix all jumps now the destination is resolved */` |
|    229121 |  4837 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - |  4838 | `	/* Release the loop block */` |
|    229121 |  4839 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4840 | `	/* Statement successfully compiled */` |
|    229121 |  4841 | `	return SXRET_OK;` |
|         1 |  4842 | `Synchronize:` |
|         - |  4843 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|         - |  4844 | `	 * compiling this erroneous block.` |
|         - |  4845 | `	 */` |
|         3 |  4846 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  4847 | `		pGen->pIn++;` |
|       ! 0 |  4848 | `	}` |
|         3 |  4849 | `	return SXRET_OK;` |
|    114564 |  4850 | `}` |
|         - |  4851 | `/*` |
|         - |  4852 | ` * Compile the infamous if/elseif/else if/else statements.` |
|         - |  4853 | ` * According to the PHP language reference` |
|         - |  4854 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|         - |  4855 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|         - |  4856 | ` *  that is similar to that of C:` |
|         - |  4857 | ` *  if (expr)` |
|         - |  4858 | ` *   statement` |
|         - |  4859 | ` *  else construct:` |
|         - |  4860 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|         - |  4861 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|         - |  4862 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|         - |  4863 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|         - |  4864 | ` *   $b, and a is NOT greater than b otherwise.` |
|         - |  4865 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|         - |  4866 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|         - |  4867 | ` *  elseif` |
|         - |  4868 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|         - |  4869 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|         - |  4870 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|         - |  4871 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|         - |  4872 | ` *   than b, a equal to b or a is smaller than b:` |
|         - |  4873 | ` *   <?php` |
|         - |  4874 | ` *    if ($a > $b) {` |
|         - |  4875 | ` *     echo "a is bigger than b";` |
|         - |  4876 | ` *    } elseif ($a == $b) {` |
|         - |  4877 | ` *     echo "a is equal to b";` |
|         - |  4878 | ` *    } else {` |
|         - |  4879 | ` *     echo "a is smaller than b";` |
|         - |  4880 | ` *    }` |
|         - |  4881 | ` *    ?>` |
|         - |  4882 | ` */` |
|   1924350 |  4883 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|         5 |  4884 | `{` |
|   1924355 |  4885 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|   1924355 |  4886 | `	GenBlock *pCondBlock = 0;` |
|         - |  4887 | `	sxu32 nJumpIdx;` |
|         - |  4888 | `	sxu32 nKeyID;` |
|         - |  4889 | `	sxi32 rc;` |
|         - |  4890 | `	/* Jump the 'if' keyword */` |
|   1924355 |  4891 | `	pGen->pIn++;` |
|   1924355 |  4892 | `	pToken = pGen->pIn;` |
|         - |  4893 | `	/* Create the conditional block */` |
|   1924355 |  4894 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|   1924355 |  4895 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4896 | `		return SXERR_ABORT;` |
|         - |  4897 | `	}` |
|         - |  4898 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   1072498 |  4899 | `	for(;;){` |
|   2145001 |  4900 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  4901 | `			/* Syntax error */` |
|       ! 0 |  4902 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 |  4903 | `				pToken--;` |
|       ! 0 |  4904 | `			}` |
|       ! 0 |  4905 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|       ! 0 |  4906 | `			if( rc == SXERR_ABORT ){` |
|         - |  4907 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4908 | `				return SXERR_ABORT;` |
|         - |  4909 | `			}` |
|       ! 0 |  4910 | `			goto Synchronize;` |
|         - |  4911 | `		}` |
|         - |  4912 | `		/* Jump the left parenthesis '(' */` |
|   2145001 |  4913 | `		pToken++;` |
|         - |  4914 | `		/* Delimit the condition */` |
|   2145001 |  4915 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   2145001 |  4916 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|         - |  4917 | `			/* Syntax error */` |
|        11 |  4918 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 |  4919 | `				pToken--;` |
|       ! 0 |  4920 | `			}` |
|        11 |  4921 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|        11 |  4922 | `			if( rc == SXERR_ABORT ){` |
|         - |  4923 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  4924 | `				return SXERR_ABORT;` |
|         - |  4925 | `			}` |
|        11 |  4926 | `			goto Synchronize;` |
|         - |  4927 | `		}` |
|         - |  4928 | `		/* Swap token streams */` |
|   2144993 |  4929 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|         - |  4930 | `		/* Compile the condition */` |
|   2144993 |  4931 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  4932 | `		/* Update token stream */` |
|   2144993 |  4933 | `		while(pGen->pIn < pEnd ){` |
|       ! 0 |  4934 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  4935 | `			pGen->pIn++;` |
|       ! 0 |  4936 | `		}` |
|   2144993 |  4937 | `		pGen->pIn  = &pEnd[1];` |
|   2144993 |  4938 | `		pGen->pEnd = pTmp;` |
|   2144993 |  4939 | `		if( rc == SXERR_ABORT ){` |
|         - |  4940 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 |  4941 | `			return SXERR_ABORT;` |
|         - |  4942 | `		}` |
|         - |  4943 | `		/* Emit the false jump */` |
|   2144993 |  4944 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|         - |  4945 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   2144993 |  4946 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|         - |  4947 | `		/* Compile the body */` |
|   2144993 |  4948 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   2144993 |  4949 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  4950 | `			return SXERR_ABORT;` |
|         - |  4951 | `		}` |
|   2144993 |  4952 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|    426191 |  4953 | `			break;` |
|         - |  4954 | `		}` |
|         - |  4955 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   1292621 |  4956 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1292621 |  4957 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|    910045 |  4958 | `			break;` |
|         - |  4959 | `		}` |
|         - |  4960 | `		/* Emit the unconditional jump */` |
|    382581 |  4961 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|         - |  4962 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    382581 |  4963 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|    382581 |  4964 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|    248571 |  4965 | `			pToken = &pGen->pIn[1];` |
|    248571 |  4966 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|     86674 |  4967 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|     80970 |  4968 | `					break;` |
|         - |  4969 | `			}` |
|     86641 |  4970 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|     43318 |  4971 | `		}` |
|    220651 |  4972 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|         - |  4973 | `		/* Synchronize cursors */` |
|    220651 |  4974 | `		pToken = pGen->pIn;` |
|         - |  4975 | `		/* Fix the false jump */` |
|    220651 |  4976 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|         5 |  4977 | `	} /* For(;;) */` |
|         - |  4978 | `	/* Fix the false jump */` |
|   1924347 |  4979 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|   1924347 |  4980 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   1071970 |  4981 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|         - |  4982 | `			/* Compile the else block */` |
|    161935 |  4983 | `			pGen->pIn++;` |
|    161935 |  4984 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    161935 |  4985 | `			if( rc == SXERR_ABORT ){` |
|         - |  4986 |  |
|       ! 0 |  4987 | `				return SXERR_ABORT;` |
|         - |  4988 | `			}` |
|     80965 |  4989 | `	}` |
|   1924347 |  4990 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|         - |  4991 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|   1924347 |  4992 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|         - |  4993 | `	/* Release the conditional block */` |
|   1924347 |  4994 | `	GenStateLeaveBlock(pGen,0);` |
|         - |  4995 | `	/* Statement successfully compiled */` |
|   1924347 |  4996 | `	return SXRET_OK;` |
|         4 |  4997 | `Synchronize:` |
|         - |  4998 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|         - |  4999 | `	 */` |
|        67 |  5000 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        59 |  5001 | `		pGen->pIn++;` |
|         3 |  5002 | `	}` |
|        11 |  5003 | `	return SXRET_OK;` |
|    962180 |  5004 | `}` |
|         - |  5005 | `/*` |
|         - |  5006 | ` * Compile the global construct.` |
|         - |  5007 | ` * According to the PHP language reference` |
|         - |  5008 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|         - |  5009 | ` *  to be used in that function.` |
|         - |  5010 | ` *  Example #1 Using global` |
|         - |  5011 | ` *  <?php` |
|         - |  5012 | ` *   $a = 1;` |
|         - |  5013 | ` *   $b = 2;` |
|         - |  5014 | ` *   function Sum()` |
|         - |  5015 | ` *   {` |
|         - |  5016 | ` *    global $a, $b;` |
|         - |  5017 | ` *    $b = $a + $b;` |
|         - |  5018 | ` *   }` |
|         - |  5019 | ` *   Sum();` |
|         - |  5020 | ` *   echo $b;` |
|         - |  5021 | ` *  ?>` |
|         - |  5022 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|         - |  5023 | ` *  all references to either variable will refer to the global version. There is no limit` |
|         - |  5024 | ` *  to the number of global variables that can be manipulated by a function.` |
|         - |  5025 | ` */` |
|        38 |  5026 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|         5 |  5027 | `{` |
|        43 |  5028 | `	SyToken *pTmp,*pNext = 0;` |
|         - |  5029 | `	sxi32 nExpr;` |
|         - |  5030 | `	sxi32 rc;` |
|         - |  5031 | `	/* Jump the 'global' keyword */` |
|        43 |  5032 | `	pGen->pIn++;` |
|        43 |  5033 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|         - |  5034 | `		/* Nothing to process */` |
|       ! 0 |  5035 | `		return SXRET_OK;` |
|         - |  5036 | `	}` |
|        43 |  5037 | `	pTmp = pGen->pEnd;` |
|        43 |  5038 | `	nExpr = 0;` |
|        91 |  5039 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|        53 |  5040 | `		if( pGen->pIn < pNext ){` |
|        53 |  5041 | `			pGen->pEnd = pNext;` |
|        53 |  5042 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  5043 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|       ! 0 |  5044 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  5045 | `					return SXERR_ABORT;` |
|         - |  5046 | `				}` |
|       ! 0 |  5047 | `			}else{` |
|        53 |  5048 | `				pGen->pIn++;` |
|        53 |  5049 | `				if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5050 | `					/* Emit a warning */` |
|       ! 0 |  5051 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|       ! 0 |  5052 | `				}else{` |
|        53 |  5053 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        53 |  5054 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  5055 | `						return SXERR_ABORT;` |
|        53 |  5056 | `					}else if(rc != SXERR_EMPTY ){` |
|        53 |  5057 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|        53 |  5058 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|         - |  5059 | `							/* Variable name, not a constant */` |
|        53 |  5060 | `							pLast->iP1 = 0;` |
|        24 |  5061 | `						}` |
|        53 |  5062 | `						nExpr++;` |
|        24 |  5063 | `					}` |
|         - |  5064 | `				}` |
|         - |  5065 | `			}` |
|        24 |  5066 | `		}` |
|         - |  5067 | `		/* Next expression in the stream */` |
|        53 |  5068 | `		pGen->pIn = pNext;` |
|         - |  5069 | `		/* Jump trailing commas */` |
|        63 |  5070 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        15 |  5071 | `			pGen->pIn++;` |
|         5 |  5072 | `		}` |
|         5 |  5073 | `	}` |
|         - |  5074 | `	/* Restore token stream */` |
|        43 |  5075 | `	pGen->pEnd = pTmp;` |
|        43 |  5076 | `	if( nExpr > 0 ){` |
|         - |  5077 | `		/* Emit the uplink instruction */` |
|        43 |  5078 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|        19 |  5079 | `	}` |
|        43 |  5080 | `	return SXRET_OK;` |
|        24 |  5081 | `}` |
|         - |  5082 | `/*` |
|         - |  5083 | ` * Compile the return statement.` |
|         - |  5084 | ` * According to the PHP language reference` |
|         - |  5085 | ` *  If called from within a function, the return() statement immediately ends execution` |
|         - |  5086 | ` *  of the current function, and returns its argument as the value of the function call.` |
|         - |  5087 | ` *  return() will also end the execution of an eval() statement or script file.` |
|         - |  5088 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|         - |  5089 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|         - |  5090 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|         - |  5091 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|         - |  5092 | ` *  from within the main script file, then script execution end.` |
|         - |  5093 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|         - |  5094 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|         - |  5095 | ` *  should do so as PHP has less work to do in this case.` |
|         - |  5096 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|         - |  5097 | ` */` |
|   2742920 |  5098 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|         5 |  5099 | `{` |
|   2742925 |  5100 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|         - |  5101 | `	sxi32 rc;` |
|   2742925 |  5102 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   2742925 |  5103 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|         - |  5104 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|         - |  5105 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|         - |  5106 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|         - |  5107 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|         - |  5108 | `	 * normally below so token processing stays consistent. */` |
|   7109099 |  5109 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|   4366179 |  5110 | `		pFuncBlock = pFuncBlock->pParent;` |
|         5 |  5111 | `	}` |
|   2742920 |  5112 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|   2742893 |  5113 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|         3 |  5114 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  5115 | `			"A never-returning function must not return");` |
|         3 |  5116 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5117 | `			return SXERR_ABORT;` |
|         - |  5118 | `		}` |
|         1 |  5119 | `	}` |
|         - |  5120 | `	/* Jump the 'return' keyword */` |
|   2742925 |  5121 | `	pGen->pIn++;` |
|   2742925 |  5122 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  5123 | `		/* Compile the expression */` |
|   2656259 |  5124 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   2656259 |  5125 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5126 | `			return SXERR_ABORT;` |
|   2656259 |  5127 | `		}else if(rc != SXERR_EMPTY ){` |
|   2656259 |  5128 | `			nRet = 1;` |
|   1328127 |  5129 | `		}` |
|   1328127 |  5130 | `	}` |
|         - |  5131 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|         - |  5132 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|         - |  5133 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|         - |  5134 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|   2742925 |  5135 | `	if( pGen->bInGenerator ){` |
|      3971 |  5136 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|      3971 |  5137 | `		return SXRET_OK;` |
|         - |  5138 | `	}` |
|         - |  5139 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|         - |  5140 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|         - |  5141 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|         - |  5142 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|         - |  5143 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|   2738959 |  5144 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|   2738959 |  5145 | `	return SXRET_OK;` |
|   1371465 |  5146 | `}` |
|         - |  5147 | `/*` |
|         - |  5148 | ` * Compile a yield expression.` |
|         - |  5149 | ` * Called from the expression code generator when a yield node is encountered.` |
|         - |  5150 | ` * Handles: yield, yield $value, yield $key => $value` |
|         - |  5151 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|         - |  5152 | ` */` |
|     16136 |  5153 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|         5 |  5154 | `{` |
|         - |  5155 | `	SyToken *pTmp, *pSplit;` |
|     16141 |  5156 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     16141 |  5157 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|         - |  5158 | `	sxi32 rc;` |
|      8068 |  5159 | `	(void)iCompileFlag;` |
|         - |  5160 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     16141 |  5161 | `	pGen->pIn++;` |
|         - |  5162 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|         - |  5163 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|         - |  5164 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|         - |  5165 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|         - |  5166 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     16136 |  5167 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|      8103 |  5168 | `		&& pGen->pIn->sData.nByte == 4` |
|        72 |  5169 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|        67 |  5170 | `		pGen->pIn++; /* Skip 'from' */` |
|        67 |  5171 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|        67 |  5172 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5173 | `			return SXERR_ABORT;` |
|         - |  5174 | `		}` |
|        67 |  5175 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 |  5176 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|       ! 0 |  5177 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|         - |  5178 | `				"Missing expression after 'yield from'");` |
|       ! 0 |  5179 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5180 | `				return SXERR_ABORT;` |
|         - |  5181 | `			}` |
|       ! 0 |  5182 | `		}` |
|        67 |  5183 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|        67 |  5184 | `		return SXRET_OK;` |
|         - |  5185 | `	}` |
|     16079 |  5186 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5187 | `		/* Bare yield — no value */` |
|         3 |  5188 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|         3 |  5189 | `		return SXRET_OK;` |
|         - |  5190 | `	}` |
|         - |  5191 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     16077 |  5192 | `	pSplit = 0;` |
|         - |  5193 | `	{` |
|     16077 |  5194 | `		SyToken *pCur = pGen->pIn;` |
|     16077 |  5195 | `		sxi32 nNest = 0;` |
|     48037 |  5196 | `		while( pCur < pGen->pEnd ){` |
|     47731 |  5197 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        18 |  5198 | `				nNest++;` |
|     47723 |  5199 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        18 |  5200 | `				nNest--;` |
|     47707 |  5201 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|     15771 |  5202 | `				pSplit = pCur;` |
|     15771 |  5203 | `				break;` |
|         - |  5204 | `			}` |
|     31965 |  5205 | `			pCur++;` |
|         5 |  5206 | `		}` |
|         - |  5207 | `	}` |
|     16077 |  5208 | `	pTmp = pGen->pEnd;` |
|     16077 |  5209 | `	if( pSplit ){` |
|         - |  5210 | `		/* yield $key => $value */` |
|     15771 |  5211 | `		pGen->pEnd = pSplit;` |
|     15771 |  5212 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     15771 |  5213 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     15771 |  5214 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|     15771 |  5215 | `		pGen->pEnd = pTmp;` |
|     15771 |  5216 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     15771 |  5217 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     15771 |  5218 | `		iP1 = 1;` |
|     15771 |  5219 | `		iP2 = 1;` |
|      7888 |  5220 | `	}else{` |
|         - |  5221 | `		/* yield $value */` |
|       311 |  5222 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       311 |  5223 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       311 |  5224 | `		if( rc != SXERR_EMPTY ){` |
|       311 |  5225 | `			iP1 = 1;` |
|       153 |  5226 | `		}` |
|         - |  5227 | `	}` |
|     16077 |  5228 | `	pGen->pEnd = pTmp;` |
|     16077 |  5229 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     16077 |  5230 | `	return SXRET_OK;` |
|      8073 |  5231 | `}` |
|         - |  5232 | `/*` |
|         - |  5233 | ` * Compile the die/exit language construct.` |
|         - |  5234 | ` * The role of these constructs is to terminate execution of the script.` |
|         - |  5235 | ` * Shutdown functions will always be executed even if exit() is called.` |
|         - |  5236 | ` */` |
|       128 |  5237 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|         5 |  5238 | `{` |
|       133 |  5239 | `	sxi32 nExpr = 0;` |
|         - |  5240 | `	sxi32 rc;` |
|         - |  5241 | `	/* Jump the die/exit keyword */` |
|       133 |  5242 | `	pGen->pIn++;` |
|       133 |  5243 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         - |  5244 | `		/* Compile the expression */` |
|       133 |  5245 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       133 |  5246 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5247 | `			return SXERR_ABORT;` |
|       133 |  5248 | `		}else if(rc != SXERR_EMPTY ){` |
|       133 |  5249 | `			nExpr = 1;` |
|        64 |  5250 | `		}` |
|        64 |  5251 | `	}` |
|         - |  5252 | `	/* Emit the HALT instruction */` |
|       133 |  5253 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|       133 |  5254 | `	return SXRET_OK;` |
|        69 |  5255 | `}` |
|         - |  5256 | `/*` |
|         - |  5257 | ` * Compile the 'echo' language construct.` |
|         - |  5258 | ` */` |
|     18048 |  5259 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|         5 |  5260 | `{` |
|     18053 |  5261 | `	SyToken *pTmp,*pNext = 0;` |
|     18053 |  5262 | `	sxu32 nLine = pGen->pIn->nLine;` |
|     18053 |  5263 | `	int nExpr = 0;      /* expressions actually compiled */` |
|     18053 |  5264 | `	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */` |
|         - |  5265 | `	sxi32 rc;` |
|         - |  5266 | `	/* Jump the 'echo' keyword */` |
|     18053 |  5267 | `	pGen->pIn++;` |
|         - |  5268 | `	/* Compile arguments one after one */` |
|     18053 |  5269 | `	pTmp = pGen->pEnd;` |
|     44949 |  5270 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|     26903 |  5271 | `		if( pGen->pIn < pNext ){` |
|     26903 |  5272 | `			pGen->pEnd = pNext;` |
|     26903 |  5273 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|     26903 |  5274 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5275 | `				return SXERR_ABORT;` |
|     26903 |  5276 | `			}else if( rc != SXERR_EMPTY ){` |
|         - |  5277 | `				/* Emit the consume instruction */` |
|     26879 |  5278 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|     26879 |  5279 | `				nExpr++;` |
|     26879 |  5280 | `				bExpectMore = 0;` |
|     13437 |  5281 | `			}` |
|     13449 |  5282 | `		}` |
|         - |  5283 | `		/* Jump trailing commas (php: exactly one between expressions; a` |
|         - |  5284 | `		 * dangling or doubled comma is a parse error, enforced below) */` |
|     35759 |  5285 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|      8863 |  5286 | `			if( bExpectMore ){` |
|         - |  5287 | `				/* two commas in a row */` |
|         3 |  5288 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,` |
|         - |  5289 | `					"syntax error, unexpected token \",\"");` |
|         3 |  5290 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  5291 | `			}` |
|      8861 |  5292 | `			bExpectMore = 1;` |
|      8861 |  5293 | `			pNext++;` |
|         5 |  5294 | `		}` |
|     26901 |  5295 | `		pGen->pIn = pNext;` |
|         5 |  5296 | `	}` |
|         - |  5297 | `	/* Restore token stream */` |
|     18051 |  5298 | `	pGen->pEnd = pTmp;` |
|     18051 |  5299 | `	if( nExpr == 0 \|\| bExpectMore ){` |
|         - |  5300 | ``		/* `echo ;` or `echo expr, ;` — php rejects both */`` |
|        32 |  5301 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  5302 | `			"syntax error, unexpected token \";\"");` |
|        32 |  5303 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - |  5304 | `	}` |
|     18023 |  5305 | `	return SXRET_OK;` |
|      9029 |  5306 | `}` |
|         - |  5307 | `/*` |
|         - |  5308 | ` * Compile the static statement.` |
|         - |  5309 | ` * According to the PHP language reference` |
|         - |  5310 | ` *  Another important feature of variable scoping is the static variable.` |
|         - |  5311 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|         - |  5312 | ` *  when program execution leaves this scope.` |
|         - |  5313 | ` *  Static variables also provide one way to deal with recursive functions.` |
|         - |  5314 | ` * Symisc eXtension.` |
|         - |  5315 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|         - |  5316 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  5317 | ` *  Example` |
|         - |  5318 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|         - |  5319 | ` *    Refer to the official documentation for more information on this feature.` |
|         - |  5320 | ` */` |
|        12 |  5321 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|         3 |  5322 | `{` |
|         - |  5323 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|         - |  5324 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|         - |  5325 | `	GenBlock *pBlock;` |
|         - |  5326 | `	SyString *pName;` |
|         - |  5327 | `	char *zDup;` |
|         - |  5328 | `	sxu32 nLine;` |
|         - |  5329 | `	sxi32 rc;` |
|         - |  5330 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|         - |  5331 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|         - |  5332 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|        12 |  5333 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|        10 |  5334 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|         1 |  5335 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|         3 |  5336 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         3 |  5337 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5338 | `			return SXERR_ABORT;` |
|         3 |  5339 | `		}else if( rc != SXERR_EMPTY ){` |
|         3 |  5340 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 |  5341 | `		}` |
|         3 |  5342 | `		return SXRET_OK;` |
|         - |  5343 | `	}` |
|         - |  5344 | `	/* Jump the static keyword */` |
|        13 |  5345 | `	nLine = pGen->pIn->nLine;` |
|        13 |  5346 | `	pGen->pIn++;` |
|         - |  5347 | `	/* Extract the enclosing function if any */` |
|        13 |  5348 | `	pBlock = pGen->pCurrent;` |
|        23 |  5349 | `	while( pBlock ){` |
|        23 |  5350 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|        13 |  5351 | `			break;` |
|         - |  5352 | `		}` |
|         - |  5353 | `		/* Point to the upper block */` |
|        13 |  5354 | `		pBlock = pBlock->pParent;` |
|         3 |  5355 | `	}` |
|        13 |  5356 | `	if( pBlock == 0 ){` |
|         - |  5357 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|       ! 0 |  5358 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  5359 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       ! 0 |  5360 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5361 | `				return SXERR_ABORT;` |
|         - |  5362 | `			}` |
|       ! 0 |  5363 | `			goto Synchronize;` |
|         - |  5364 | `		}` |
|         - |  5365 | `		/* Compile the expression holding the variable */` |
|       ! 0 |  5366 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       ! 0 |  5367 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5368 | `			return SXERR_ABORT;` |
|       ! 0 |  5369 | `		}else if( rc != SXERR_EMPTY ){` |
|         - |  5370 | `			/* Emit the POP instruction */` |
|       ! 0 |  5371 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       ! 0 |  5372 | `		}` |
|       ! 0 |  5373 | `		return SXRET_OK;` |
|         - |  5374 | `	}` |
|        13 |  5375 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|         - |  5376 | `	/* Make sure we are dealing with a valid statement */` |
|        13 |  5377 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|         8 |  5378 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         3 |  5379 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|         3 |  5380 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5381 | `				return SXERR_ABORT;` |
|         - |  5382 | `			}` |
|         3 |  5383 | `			goto Synchronize;` |
|         - |  5384 | `	}` |
|        10 |  5385 | `	pGen->pIn++;` |
|         - |  5386 | `	/* Extract variable name */` |
|        10 |  5387 | `	pName = &pGen->pIn->sData;` |
|        10 |  5388 | `	pGen->pIn++; /* Jump the var name */` |
|        10 |  5389 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|       ! 0 |  5390 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 |  5391 | `		goto Synchronize;` |
|         - |  5392 | `	}` |
|         - |  5393 | `	/* Initialize the structure describing the static variable */` |
|        10 |  5394 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        10 |  5395 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|         - |  5396 | `	/* Duplicate variable name */` |
|        10 |  5397 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|        10 |  5398 | `	if( zDup == 0 ){` |
|       ! 0 |  5399 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  5400 | `		return SXERR_ABORT;` |
|         - |  5401 | `	}` |
|        10 |  5402 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|         - |  5403 | `	/* Check if we have an expression to compile */` |
|        10 |  5404 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|         - |  5405 | `		SySet *pInstrContainer;` |
|         - |  5406 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|         - |  5407 | `		 * Static variable can take any complex expression including function` |
|         - |  5408 | `		 * call as their initialization value.` |
|         - |  5409 | `		 * Example:` |
|         - |  5410 | `		 *		static $var = foo(1,4+5,bar());` |
|         - |  5411 | `		 */` |
|        10 |  5412 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|         - |  5413 | `		/* Swap bytecode container */` |
|        10 |  5414 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        10 |  5415 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|         - |  5416 | `		/* Compile the expression */` |
|        10 |  5417 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  5418 | `		/* Emit the done instruction */` |
|        10 |  5419 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|         - |  5420 | `		/* Restore default bytecode container */` |
|        10 |  5421 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         4 |  5422 | `	}` |
|         - |  5423 | `	/* Finally save the compiled static variable in the appropriate container */` |
|        10 |  5424 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|        10 |  5425 | `	return SXRET_OK;` |
|         1 |  5426 | `Synchronize:` |
|         - |  5427 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|         - |  5428 | `	 * statement.` |
|         - |  5429 | `	 */` |
|         5 |  5430 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|         3 |  5431 | `		pGen->pIn++;` |
|         1 |  5432 | `	}` |
|         3 |  5433 | `	return SXRET_OK;` |
|         9 |  5434 | `}` |
|         - |  5435 | `/*` |
|         - |  5436 | ` * Compile the var statement.` |
|         - |  5437 | ` * Symisc Extension:` |
|         - |  5438 | ` *      var statement can be used outside of a class definition.` |
|         - |  5439 | ` */` |
|         4 |  5440 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|         1 |  5441 | `{` |
|         - |  5442 | `	sxu32 nLine;` |
|         - |  5443 | `	sxi32 rc;` |
|         5 |  5444 | `	nLine = pGen->pIn->nLine;` |
|         - |  5445 | `	/* Jump the 'var' keyword */` |
|         5 |  5446 | `	pGen->pIn++;` |
|         5 |  5447 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|       ! 0 |  5448 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|         - |  5449 | `		/* Synchronize with the first semi-colon */` |
|       ! 0 |  5450 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|       ! 0 |  5451 | `			pGen->pIn++;` |
|       ! 0 |  5452 | `		}` |
|       ! 0 |  5453 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5454 | `			return SXERR_ABORT;` |
|         - |  5455 | `		}` |
|       ! 0 |  5456 | `	}else{` |
|         - |  5457 | `		/* Compile the expression */` |
|         5 |  5458 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         5 |  5459 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5460 | `			return SXERR_ABORT;` |
|         5 |  5461 | `		}else if( rc != SXERR_EMPTY ){` |
|         5 |  5462 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 |  5463 | `		}` |
|         - |  5464 | `	}` |
|         5 |  5465 | `	return SXRET_OK;` |
|         3 |  5466 | `}` |
|         - |  5467 | `/*` |
|         - |  5468 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|         - |  5469 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|         - |  5470 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|         - |  5471 | ` */` |
|         - |  5472 | `/*` |
|         - |  5473 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|         - |  5474 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|         - |  5475 | ` * hash and any shared references), this creates a new literal entry with the` |
|         - |  5476 | ` * qualified name and updates the instruction's operand index.` |
|         - |  5477 | ` *` |
|         - |  5478 | ` * Resolution order:` |
|         - |  5479 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|         - |  5480 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|         - |  5481 | ` *   3. Otherwise return the original literal index unchanged.` |
|         - |  5482 | ` *` |
|         - |  5483 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|         - |  5484 | ` * came from an import (step 1) and 0 otherwise.` |
|         - |  5485 | ` * Returns the (possibly new) literal index.` |
|         - |  5486 | ` */` |
|   4986640 |  5487 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|         5 |  5488 | `{` |
|         - |  5489 | `	ph7_value *pLit;` |
|         - |  5490 | `	const char *zLit;` |
|         - |  5491 | `	SyString sQualified;` |
|         - |  5492 | `	sxu32 nLit;` |
|         - |  5493 | `	sxu32 k;` |
|         - |  5494 | `	sxu32 nNewIdx;` |
|         - |  5495 | `	int hasNsSep;` |
|         - |  5496 | `	SyHashEntry *pImport;` |
|         - |  5497 | `	ph7_value *pNew;` |
|   4986645 |  5498 | `	if( pFromImport ){` |
|   3935353 |  5499 | `		*pFromImport = 0;` |
|   1967674 |  5500 | `	}` |
|   4986645 |  5501 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|   4986645 |  5502 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|       ! 0 |  5503 | `		return nOrigIdx;` |
|         - |  5504 | `	}` |
|   4986645 |  5505 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|   4986645 |  5506 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|         - |  5507 | `	/* Skip if already qualified (contains backslash) */` |
|   4986645 |  5508 | `	hasNsSep = 0;` |
|  61050421 |  5509 | `	for( k = 0; k < nLit; k++ ){` |
|  56063789 |  5510 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
|  28031893 |  5511 | `	}` |
|   4986645 |  5512 | `	if( hasNsSep ){` |
|        11 |  5513 | `		return nOrigIdx;` |
|         - |  5514 | `	}` |
|         - |  5515 | `	/* Check use imports first (works even outside namespaces) */` |
|   4986637 |  5516 | `	SyBlobReset(&pGen->sWorker);` |
|   4986637 |  5517 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|   4986637 |  5518 | `	if( pImport ){` |
|        41 |  5519 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|        41 |  5520 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|        41 |  5521 | `		if( pFromImport ){` |
|        18 |  5522 | `			*pFromImport = 1;` |
|         8 |  5523 | `		}` |
|        23 |  5524 | `	}else{` |
|   4986601 |  5525 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|   4986511 |  5526 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|         - |  5527 | `		}` |
|         - |  5528 | `		/* Prepend current namespace */` |
|        95 |  5529 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        95 |  5530 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|        95 |  5531 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|         - |  5532 | `	}` |
|         - |  5533 | `	/* Look up or create a new literal for the qualified name */` |
|       131 |  5534 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|       131 |  5535 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|        57 |  5536 | `		return nNewIdx; /* Already interned */` |
|         - |  5537 | `	}` |
|        79 |  5538 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|        79 |  5539 | `	if( pNew == 0 ){` |
|       ! 0 |  5540 | `		return nOrigIdx; /* OOM, fall back to original */` |
|         - |  5541 | `	}` |
|        79 |  5542 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|        79 |  5543 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|        79 |  5544 | `	return nNewIdx;` |
|   2493325 |  5545 | `}` |
|         - |  5546 | `/*` |
|         - |  5547 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|         - |  5548 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|         - |  5549 | ` */` |
|    422858 |  5550 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|         5 |  5551 | `{` |
|         - |  5552 | `	SyHashEntry *pImport;` |
|         - |  5553 | `	/* Check use imports first */` |
|    422863 |  5554 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|    422863 |  5555 | `	if( pImport ){` |
|        20 |  5556 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|        20 |  5557 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|        20 |  5558 | `		return;` |
|         - |  5559 | `	}` |
|         - |  5560 | `	/* Prepend current namespace if active */` |
|    422847 |  5561 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         8 |  5562 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|         8 |  5563 | `		SyBlobAppend(pOut,"\\",1);` |
|         3 |  5564 | `	}` |
|    422847 |  5565 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    211434 |  5566 | `}` |
|         - |  5567 | `/*` |
|         - |  5568 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|         - |  5569 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|         - |  5570 | ` * The caller must release pOut when done.` |
|         - |  5571 | ` */` |
|    443010 |  5572 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|         5 |  5573 | `{` |
|    443015 |  5574 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      4001 |  5575 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      4001 |  5576 | `		SyBlobAppend(pOut,"\\",1);` |
|      1998 |  5577 | `	}` |
|    443015 |  5578 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    443015 |  5579 | `}` |
|         - |  5580 | `/*` |
|         - |  5581 | ` * Compile a namespace statement` |
|         - |  5582 | ` * According to the PHP language reference manual` |
|         - |  5583 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|         - |  5584 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|         - |  5585 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|         - |  5586 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|         - |  5587 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|         - |  5588 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|         - |  5589 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|         - |  5590 | ` *  programming world.` |
|         - |  5591 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|         - |  5592 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|         - |  5593 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|         - |  5594 | ` *  classes/functions/constants.` |
|         - |  5595 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|         - |  5596 | ` *  readability of source code.` |
|         - |  5597 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|         - |  5598 | ` *  Here is an example of namespace syntax in PHP:` |
|         - |  5599 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|         - |  5600 | ` *       class MyClass {}` |
|         - |  5601 | ` *       function myfunction() {}` |
|         - |  5602 | ` *       const MYCONST = 1;` |
|         - |  5603 | ` *       $a = new MyClass;` |
|         - |  5604 | ` *       $c = new \my\name\MyClass;` |
|         - |  5605 | ` *       $a = strlen('hi');` |
|         - |  5606 | ` *       $d = namespace\MYCONST;` |
|         - |  5607 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|         - |  5608 | ` *       echo constant($d);` |
|         - |  5609 | ` * NOTE` |
|         - |  5610 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|         - |  5611 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|         - |  5612 | ` */` |
|         - |  5613 | `/*` |
|         - |  5614 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|         - |  5615 | ` */` |
|        14 |  5616 | `static const char * TokenTypeName(sxu32 nType)` |
|         4 |  5617 | `{` |
|        18 |  5618 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|        12 |  5619 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|        12 |  5620 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|        12 |  5621 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|        12 |  5622 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|        12 |  5623 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|         3 |  5624 | `	return "token";` |
|        11 |  5625 | `}` |
|      4044 |  5626 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|         5 |  5627 | `{` |
|         - |  5628 | `	sxu32 nLine;` |
|         - |  5629 | `	sxi32 rc;` |
|      4049 |  5630 | `	nLine = pGen->pIn->nLine;` |
|      4049 |  5631 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|         - |  5632 | `	/* Reset namespace and clear previous use imports */` |
|      4049 |  5633 | `	SyBlobReset(&pGen->sNamespace);` |
|      4049 |  5634 | `	SyHashRelease(&pGen->hUseImports);` |
|      4049 |  5635 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|      4049 |  5636 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|      4049 |  5637 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|      4049 |  5638 | `	SyHashRelease(&pGen->hUseConstImports);` |
|      4049 |  5639 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|      4049 |  5640 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  5641 | `		/* Global namespace (bare "namespace;") */` |
|       ! 0 |  5642 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5643 | `		return SXRET_OK;` |
|         - |  5644 | `	}` |
|      4049 |  5645 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|         - |  5646 | `		/* namespace; — switch to global namespace */` |
|       ! 0 |  5647 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5648 | `		return SXRET_OK;` |
|         - |  5649 | `	}` |
|      4049 |  5650 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|         - |  5651 | `		/* namespace { } — global namespace block */` |
|       ! 0 |  5652 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       ! 0 |  5653 | `		return SXRET_OK;` |
|         - |  5654 | `	}` |
|         - |  5655 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|      8135 |  5656 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      4091 |  5657 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|         - |  5658 | `			/* Append backslash separator */` |
|        27 |  5659 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        27 |  5660 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|        11 |  5661 | `			}` |
|        16 |  5662 | `		}else{` |
|         - |  5663 | `			/* Append identifier */` |
|      4069 |  5664 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|         - |  5665 | `		}` |
|      4091 |  5666 | `		pGen->pIn++;` |
|         5 |  5667 | `	}` |
|         - |  5668 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|         - |  5669 | `	 * at the correct program counter, not just the last one compiled. */` |
|         - |  5670 | `	{` |
|      4049 |  5671 | `		char *zNsDup = 0;` |
|      4049 |  5672 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      6068 |  5673 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      4042 |  5674 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      2021 |  5675 | `		}` |
|      4049 |  5676 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|         - |  5677 | `	}` |
|      4049 |  5678 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|         8 |  5679 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|         - |  5680 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|         4 |  5681 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         6 |  5682 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5683 | `			return SXERR_ABORT;` |
|         - |  5684 | `		}` |
|         2 |  5685 | `	}` |
|      4049 |  5686 | `	return SXRET_OK;` |
|      2027 |  5687 | `}` |
|         - |  5688 | `/*` |
|         - |  5689 | ` * Compile the 'use' statement` |
|         - |  5690 | ` * According to the PHP language reference manual` |
|         - |  5691 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|         - |  5692 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|         - |  5693 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|         - |  5694 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|         - |  5695 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|         - |  5696 | ` *  a function or constant is not supported.` |
|         - |  5697 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|         - |  5698 | ` * NOTE` |
|         - |  5699 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|         - |  5700 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|         - |  5701 | ` */` |
|        72 |  5702 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|         5 |  5703 | `{` |
|         - |  5704 | `	sxu32 nLine;` |
|         - |  5705 | `	sxi32 rc;` |
|         - |  5706 | `	SyBlob sPath;` |
|         - |  5707 | `	SyString sAlias;` |
|         - |  5708 | `	SyToken *pLast;` |
|         - |  5709 | `	char *zDup;` |
|         - |  5710 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|         - |  5711 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|         - |  5712 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|        77 |  5713 | `	nLine = pGen->pIn->nLine;` |
|        77 |  5714 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|         - |  5715 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|        77 |  5716 | `	iUseType = 0;` |
|        77 |  5717 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        30 |  5718 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|        30 |  5719 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|        16 |  5720 | `			iUseType = 1;` |
|        16 |  5721 | `			pGen->pIn++;` |
|        23 |  5722 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|        16 |  5723 | `			iUseType = 2;` |
|        16 |  5724 | `			pGen->pIn++;` |
|         7 |  5725 | `		}` |
|        14 |  5726 | `	}` |
|         - |  5727 | `	/* Select target hash tables based on import type */` |
|        77 |  5728 | `	switch( iUseType ){` |
|         7 |  5729 | `		case 1:` |
|        16 |  5730 | `			pGenHash = &pGen->hUseFuncImports;` |
|        16 |  5731 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|        16 |  5732 | `			break;` |
|         7 |  5733 | `		case 2:` |
|        16 |  5734 | `			pGenHash = &pGen->hUseConstImports;` |
|        16 |  5735 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|        16 |  5736 | `			break;` |
|        22 |  5737 | `		default:` |
|        49 |  5738 | `			pGenHash = &pGen->hUseImports;` |
|        49 |  5739 | `			pVmHash = &pGen->pVm->hUseImports;` |
|        44 |  5740 | `			break;` |
|         - |  5741 | `	}` |
|        77 |  5742 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|         - |  5743 | `	/* Process one or more use declarations separated by commas */` |
|        37 |  5744 | `	for(;;){` |
|        79 |  5745 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  5746 | `			break;` |
|         - |  5747 | `		}` |
|        79 |  5748 | `		SyBlobReset(&sPath);` |
|        79 |  5749 | `		pLast = 0;` |
|         - |  5750 | `		/* Collect the full namespace path */` |
|       269 |  5751 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|       195 |  5752 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       135 |  5753 | `				pLast = pGen->pIn;` |
|       135 |  5754 | `				if( SyBlobLength(&sPath) > 0 ){` |
|        65 |  5755 | `					SyBlobAppend(&sPath,"\\",1);` |
|        30 |  5756 | `				}` |
|       135 |  5757 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        65 |  5758 | `			}` |
|       195 |  5759 | `			pGen->pIn++;` |
|         5 |  5760 | `		}` |
|        79 |  5761 | `		if( pLast == 0 ){` |
|         - |  5762 | `			/* Empty path */` |
|         6 |  5763 | `			break;` |
|         - |  5764 | `		}` |
|         - |  5765 | `		/* Default alias is the last component of the path */` |
|        75 |  5766 | `		sAlias = pLast->sData;` |
|         - |  5767 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|        70 |  5768 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|        50 |  5769 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|        23 |  5770 | `			pGen->pIn++; /* Jump 'as' */` |
|        23 |  5771 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|        23 |  5772 | `				sAlias = pGen->pIn->sData;` |
|        23 |  5773 | `				pGen->pIn++;` |
|        10 |  5774 | `			}` |
|        10 |  5775 | `		}` |
|         - |  5776 | `		/* Check for duplicate import alias (per-type) */` |
|        75 |  5777 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|         8 |  5778 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  5779 | `				"Cannot use %.*s as %z because the name is already in use",` |
|         4 |  5780 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|         6 |  5781 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  5782 | `				SyBlobRelease(&sPath);` |
|       ! 0 |  5783 | `				return SXERR_ABORT;` |
|         - |  5784 | `			}` |
|         2 |  5785 | `		}` |
|         - |  5786 | `		/* Register the import: alias -> FQN.` |
|         - |  5787 | `		 * Strings are allocated from the VM pool allocator and freed` |
|         - |  5788 | `		 * when the entire VM is released. SyHashRelease does not free` |
|         - |  5789 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|       110 |  5790 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        70 |  5791 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        75 |  5792 | `		if( zDup ){` |
|        75 |  5793 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|        75 |  5794 | `			if( pVmHash ){` |
|         - |  5795 | `				/* Class imports: populate VM table directly (class resolution` |
|         - |  5796 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|        47 |  5797 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        47 |  5798 | `				if( zAliasDup ){` |
|        47 |  5799 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|        21 |  5800 | `				}` |
|        21 |  5801 | `			}` |
|        75 |  5802 | `			if( iUseType == 2 ){` |
|         - |  5803 | `				/* Const imports: emit a runtime instruction so imports are` |
|         - |  5804 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|        16 |  5805 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        16 |  5806 | `				if( zAliasDup ){` |
|         - |  5807 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|         - |  5808 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|         - |  5809 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|        16 |  5810 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|        16 |  5811 | `					if( azPair ){` |
|        16 |  5812 | `						azPair[0] = zAliasDup;` |
|        16 |  5813 | `						azPair[1] = zDup;` |
|        16 |  5814 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|         7 |  5815 | `					}` |
|         7 |  5816 | `				}` |
|         7 |  5817 | `			}` |
|        35 |  5818 | `		}` |
|         - |  5819 | `		/* Check for comma (multiple use declarations) */` |
|        75 |  5820 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|         3 |  5821 | `			pGen->pIn++;` |
|         2 |  5822 | `		}else{` |
|        39 |  5823 | `			break;` |
|         - |  5824 | `		}` |
|         1 |  5825 | `	}` |
|        77 |  5826 | `	SyBlobRelease(&sPath);` |
|        77 |  5827 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|         4 |  5828 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|         2 |  5829 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|         3 |  5830 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5831 | `			return SXERR_ABORT;` |
|         - |  5832 | `		}` |
|         1 |  5833 | `	}` |
|        77 |  5834 | `	return SXRET_OK;` |
|        41 |  5835 | `}` |
|         - |  5836 | `/*` |
|         - |  5837 | ` * Compile the stupid 'declare' language construct.` |
|         - |  5838 | ` *` |
|         - |  5839 | ` * According to the PHP language reference manual.` |
|         - |  5840 | ` *  The declare construct is used to set execution directives for a block of code.` |
|         - |  5841 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|         - |  5842 | ` *  declare (directive)` |
|         - |  5843 | ` *   statement` |
|         - |  5844 | ` * The directive section allows the behavior of the declare block to be set.` |
|         - |  5845 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|         - |  5846 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|         - |  5847 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|         - |  5848 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|         - |  5849 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|         - |  5850 | ` * <?php` |
|         - |  5851 | ` * // these are the same:` |
|         - |  5852 | ` * // you can use this:` |
|         - |  5853 | ` * declare(ticks=1) {` |
|         - |  5854 | ` *   // entire script here` |
|         - |  5855 | ` * }` |
|         - |  5856 | ` * // or you can use this:` |
|         - |  5857 | ` * declare(ticks=1);` |
|         - |  5858 | ` * // entire script here` |
|         - |  5859 | ` * ?>` |
|         - |  5860 | ` *` |
|         - |  5861 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|         - |  5862 | ` */` |
|         - |  5863 | `/*` |
|         - |  5864 | ` * Match a directive name against a known literal (case-insensitive).` |
|         - |  5865 | ` */` |
|        72 |  5866 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|         5 |  5867 | `{` |
|       109 |  5868 | `	return SyStringLength(pName) == nWant` |
|        72 |  5869 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|         5 |  5870 | `}` |
|         - |  5871 |  |
|        42 |  5872 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|         5 |  5873 | `{` |
|        47 |  5874 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        47 |  5875 | `	SyToken *pBodyEnd = 0;` |
|         - |  5876 | `	SyToken *pBodyStart;` |
|         - |  5877 | `	SyToken *pCursor;` |
|         - |  5878 | `	int bHasStrictTypes;` |
|         - |  5879 | `	int bBlockForm;` |
|         - |  5880 | `	int bPlacementOk;` |
|         - |  5881 | `	sxi32 rc;` |
|        47 |  5882 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|        47 |  5883 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|         5 |  5884 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|         5 |  5885 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5886 | `			return SXERR_ABORT;` |
|         - |  5887 | `		}` |
|         5 |  5888 | `		goto Synchro;` |
|         - |  5889 | `	}` |
|        43 |  5890 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|        43 |  5891 | `	pBodyStart = pGen->pIn;` |
|         - |  5892 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|        43 |  5893 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|        43 |  5894 | `	if( pBodyEnd >= pGen->pEnd ){` |
|       ! 0 |  5895 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|       ! 0 |  5896 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5897 | `			return SXERR_ABORT;` |
|         - |  5898 | `		}` |
|       ! 0 |  5899 | `		return SXRET_OK;` |
|         - |  5900 | `	}` |
|         - |  5901 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|         - |  5902 | `	 * now delimits the comma-separated directive list. */` |
|        43 |  5903 | `	pGen->pIn = &pBodyEnd[1];` |
|        43 |  5904 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       ! 0 |  5905 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|       ! 0 |  5906 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  5907 | `			return SXERR_ABORT;` |
|         - |  5908 | `		}` |
|       ! 0 |  5909 | `	}` |
|        43 |  5910 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|        43 |  5911 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|        43 |  5912 | `	bHasStrictTypes = 0;` |
|         - |  5913 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|         - |  5914 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|         - |  5915 | `	 * directive appears anywhere in the list, before validating values. */` |
|        43 |  5916 | `	pCursor = pBodyStart;` |
|        55 |  5917 | `	while( pCursor < pBodyEnd ){` |
|        51 |  5918 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|        43 |  5919 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|        39 |  5920 | `				bHasStrictTypes = 1;` |
|        39 |  5921 | `				break;` |
|         - |  5922 | `			}` |
|         2 |  5923 | `		}` |
|        14 |  5924 | `		pCursor++;` |
|         2 |  5925 | `	}` |
|        43 |  5926 | `	if( bHasStrictTypes && bBlockForm ){` |
|         3 |  5927 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5928 | `			"strict_types declaration must not use block mode");` |
|         3 |  5929 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         3 |  5930 | `		return SXRET_OK;` |
|         - |  5931 | `	}` |
|        41 |  5932 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|         6 |  5933 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5934 | `			"strict_types declaration must be the very first statement in the script");` |
|         6 |  5935 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         6 |  5936 | `		return SXRET_OK;` |
|         - |  5937 | `	}` |
|         - |  5938 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|        37 |  5939 | `	pCursor = pBodyStart;` |
|        69 |  5940 | `	while( pCursor < pBodyEnd ){` |
|         - |  5941 | `		SyToken *pNameTok;` |
|         - |  5942 | `		SyToken *pEqTok;` |
|         - |  5943 | `		SyToken *pValTok;` |
|         - |  5944 | `		SyString *pDirName;` |
|         - |  5945 | `		int bIsStrict;` |
|         - |  5946 | `		int iStrictValue;` |
|        39 |  5947 | `		pNameTok = pCursor;` |
|        39 |  5948 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  5949 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5950 | `				"declare: Expecting a directive name");` |
|       ! 0 |  5951 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  5952 | `			return SXRET_OK;` |
|         - |  5953 | `		}` |
|        39 |  5954 | `		pEqTok = pNameTok + 1;` |
|        39 |  5955 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|       ! 0 |  5956 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5957 | `				"declare: Expecting '=' after directive name");` |
|       ! 0 |  5958 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  5959 | `			return SXRET_OK;` |
|         - |  5960 | `		}` |
|        39 |  5961 | `		pValTok = pEqTok + 1;` |
|        39 |  5962 | `		if( pValTok >= pBodyEnd ){` |
|       ! 0 |  5963 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5964 | `				"declare: Expecting value after '='");` |
|       ! 0 |  5965 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  5966 | `			return SXRET_OK;` |
|         - |  5967 | `		}` |
|        39 |  5968 | `		pDirName = &pNameTok->sData;` |
|        39 |  5969 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|        39 |  5970 | `		if( bIsStrict ){` |
|         - |  5971 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|         - |  5972 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|        35 |  5973 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       ! 0 |  5974 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5975 | `					"declare(strict_types) value must be a literal");` |
|       ! 0 |  5976 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  5977 | `				return SXRET_OK;` |
|         - |  5978 | `			}` |
|        35 |  5979 | `			iStrictValue = -1;` |
|        35 |  5980 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|        35 |  5981 | `				const char *zv = SyStringData(&pValTok->sData);` |
|        35 |  5982 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|        35 |  5983 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|        33 |  5984 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|        15 |  5985 | `			}` |
|        35 |  5986 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|         3 |  5987 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  5988 | `					"strict_types declaration must have 0 or 1 as its value");` |
|         3 |  5989 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|         3 |  5990 | `				return SXRET_OK;` |
|         - |  5991 | `			}` |
|        32 |  5992 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|        18 |  5993 | `		}else{` |
|         - |  5994 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|         - |  5995 | `			 * preserve the legacy notice so callers relying on the old` |
|         - |  5996 | `			 * behavior don't regress. */` |
|         8 |  5997 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|         - |  5998 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|         2 |  5999 | `				ph7_lib_version()` |
|         - |  6000 | `				);` |
|         - |  6001 | `		}` |
|        36 |  6002 | `		pCursor = pValTok + 1;` |
|         - |  6003 | `		/* Consume separating comma (or end). */` |
|        36 |  6004 | `		if( pCursor < pBodyEnd ){` |
|         3 |  6005 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  6006 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  6007 | `					"declare: Expecting ',' or ')' after directive value");` |
|       ! 0 |  6008 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       ! 0 |  6009 | `				return SXRET_OK;` |
|         - |  6010 | `			}` |
|         3 |  6011 | `			pCursor++;` |
|         1 |  6012 | `		}` |
|         4 |  6013 | `	}` |
|         - |  6014 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|         - |  6015 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|         - |  6016 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|        34 |  6017 | `	return SXRET_OK;` |
|         2 |  6018 | `Synchro:` |
|         - |  6019 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|        15 |  6020 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|        11 |  6021 | `		pGen->pIn++;` |
|         1 |  6022 | `	}` |
|         5 |  6023 | `	return SXRET_OK;` |
|        26 |  6024 | `}` |
|         - |  6025 | `/*` |
|         - |  6026 | ` * Process default argument values. That is,a function may define C++-style default value` |
|         - |  6027 | ` * as follows:` |
|         - |  6028 | ` * function makecoffee($type = "cappuccino")` |
|         - |  6029 | ` * {` |
|         - |  6030 | ` *   return "Making a cup of $type.\n";` |
|         - |  6031 | ` * }` |
|         - |  6032 | ` * Symisc eXtension.` |
|         - |  6033 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|         - |  6034 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|         - |  6035 | ` *      Example: Work only with PH7,generate error under zend` |
|         - |  6036 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|         - |  6037 | ` *      {` |
|         - |  6038 | ` *       var_dump($a);` |
|         - |  6039 | ` *      }` |
|         - |  6040 | ` *     //call test without args` |
|         - |  6041 | ` *      test();` |
|         - |  6042 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|         - |  6043 | ` *      Example:` |
|         - |  6044 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|         - |  6045 | ` * 3 -) Function overloading!!` |
|         - |  6046 | ` *      Example:` |
|         - |  6047 | ` *      function foo($a) {` |
|         - |  6048 | ` *   	  return $a.PHP_EOL;` |
|         - |  6049 | ` *	    }` |
|         - |  6050 | ` *	    function foo($a, $b) {` |
|         - |  6051 | ` *   	  return $a + $b;` |
|         - |  6052 | ` *	    }` |
|         - |  6053 | ` *	    echo foo(5); // Prints "5"` |
|         - |  6054 | ` *	    echo foo(5, 2); // Prints "7"` |
|         - |  6055 | ` *      // Same arg` |
|         - |  6056 | ` *	   function foo(string $a)` |
|         - |  6057 | ` *	   {` |
|         - |  6058 | ` *	     echo "a is a string\n";` |
|         - |  6059 | ` *	     var_dump($a);` |
|         - |  6060 | ` *	   }` |
|         - |  6061 | ` *	  function foo(int $a)` |
|         - |  6062 | ` *	  {` |
|         - |  6063 | ` *	    echo "a is integer\n";` |
|         - |  6064 | ` *	    var_dump($a);` |
|         - |  6065 | ` *	  }` |
|         - |  6066 | ` *	  function foo(array $a)` |
|         - |  6067 | ` *	  {` |
|         - |  6068 | ` * 	    echo "a is an array\n";` |
|         - |  6069 | ` * 	    var_dump($a);` |
|         - |  6070 | ` *	  }` |
|         - |  6071 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|         - |  6072 | ` *	  foo(52); // a is integer [second foo]` |
|         - |  6073 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|         - |  6074 | ` * Please refer to the official documentation for more information on the powerful extension` |
|         - |  6075 | ` * introduced by the PH7 engine.` |
|         - |  6076 | ` */` |
|    468780 |  6077 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|         5 |  6078 | `{` |
|         - |  6079 | `	SyToken *pTmpIn,*pTmpEnd;` |
|         - |  6080 | `	SySet *pInstrContainer;` |
|         - |  6081 | `	sxi32 rc;` |
|         - |  6082 | `	/* Swap token stream */` |
|    468785 |  6083 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|    468785 |  6084 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    468785 |  6085 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|         - |  6086 | `	/* Compile the expression holding the argument value */` |
|    468785 |  6087 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - |  6088 | `	/* Emit the done instruction */` |
|    468785 |  6089 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|    468785 |  6090 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    468785 |  6091 | `	RE_SWAP_DELIMITER(pGen);` |
|    468785 |  6092 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  6093 | `		return SXERR_ABORT;` |
|         - |  6094 | `	}` |
|    468785 |  6095 | `	return SXRET_OK;` |
|    234395 |  6096 | `}` |
|         - |  6097 | `/*` |
|         - |  6098 | ` * Collect function arguments one after one.` |
|         - |  6099 | ` * According to the PHP language reference manual.` |
|         - |  6100 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|         - |  6101 | ` * list of expressions.` |
|         - |  6102 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|         - |  6103 | ` * and default argument values. Variable-length argument lists are also supported,` |
|         - |  6104 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|         - |  6105 | ` * for more information.` |
|         - |  6106 | ` * Example #1 Passing arrays to functions` |
|         - |  6107 | ` * <?php` |
|         - |  6108 | ` * function takes_array($input)` |
|         - |  6109 | ` * {` |
|         - |  6110 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|         - |  6111 | ` * }` |
|         - |  6112 | ` * ?>` |
|         - |  6113 | ` * Making arguments be passed by reference` |
|         - |  6114 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|         - |  6115 | ` * within the function is changed, it does not get changed outside of the function).` |
|         - |  6116 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|         - |  6117 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|         - |  6118 | ` * to the argument name in the function definition:` |
|         - |  6119 | ` * Example #2 Passing function parameters by reference` |
|         - |  6120 | ` * <?php` |
|         - |  6121 | ` * function add_some_extra(&$string)` |
|         - |  6122 | ` * {` |
|         - |  6123 | ` *   $string .= 'and something extra.';` |
|         - |  6124 | ` * }` |
|         - |  6125 | ` * $str = 'This is a string, ';` |
|         - |  6126 | ` * add_some_extra($str);` |
|         - |  6127 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|         - |  6128 | ` * ?>` |
|         - |  6129 | ` *` |
|         - |  6130 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|         - |  6131 | ` * complex agrument values.Please refer to the official documentation for more information` |
|         - |  6132 | ` * on these extension.` |
|         - |  6133 | ` */` |
|   1132110 |  6134 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|         5 |  6135 | `{` |
|         - |  6136 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|         - |  6137 | `	SyToken *pIn;  /* Token stream */` |
|         - |  6138 | `	SyBlob sSig;         /* Function signature */` |
|         - |  6139 | `	char *zDup;          /* Copy of argument name */` |
|         - |  6140 | `	sxi32 rc;` |
|         - |  6141 |  |
|   1132115 |  6142 | `	pIn = pGen->pIn;` |
|   1132115 |  6143 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|         - |  6144 | `	/* Process arguments one after one */` |
|   1420054 |  6145 | `	for(;;){` |
|   2840113 |  6146 | `		if( pIn >= pEnd ){` |
|         - |  6147 | `			/* No more arguments to process */` |
|   1132099 |  6148 | `			break;` |
|         - |  6149 | `		}` |
|   1708019 |  6150 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   1708019 |  6151 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   1708019 |  6152 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   1708019 |  6153 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|   1708019 |  6154 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|         - |  6155 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|         - |  6156 | `		 * first token inside the main token stream */` |
|   1708019 |  6157 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  6158 | `			return SXERR_ABORT;` |
|         - |  6159 | `		}` |
|         - |  6160 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|         - |  6161 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|         - |  6162 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|         - |  6163 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|         - |  6164 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|         - |  6165 | `		{` |
|   1708019 |  6166 | `			int bReadonly = 0, bVisSeen = 0;` |
|   1708019 |  6167 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|   1708019 |  6168 | `			sxi32 iSetVisFlag = 0;` |
|         - |  6169 | `			int nSetTok;` |
|         - |  6170 | `			sxi32 nSetVis;` |
|   1708019 |  6171 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|         3 |  6172 | `				bReadonly = 1;` |
|         3 |  6173 | `				pIn++;` |
|         1 |  6174 | `			}` |
|   1708019 |  6175 | `			nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|   1708019 |  6176 | `			if( nSetVis ){` |
|         - |  6177 | ``				/* Leading `private(set)` etc: promoted with a public read side */`` |
|         3 |  6178 | `				iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  6179 | `				bVisSeen = 1;` |
|         3 |  6180 | `				pIn += nSetTok;` |
|         3 |  6181 | `				if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       ! 0 |  6182 | `					bReadonly = 1;` |
|       ! 0 |  6183 | `					pIn++;` |
|         1 |  6184 | `				}` |
|   1708018 |  6185 | `			}else if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|     90973 |  6186 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|     90973 |  6187 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|        89 |  6188 | `					bVisSeen = 1;` |
|        89 |  6189 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|       120 |  6190 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|        39 |  6191 | `						: PH7_CLASS_PROT_PUBLIC;` |
|        89 |  6192 | `					pIn++;` |
|        89 |  6193 | `					nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|        89 |  6194 | `					if( nSetVis ){` |
|         - |  6195 | ``						/* `public private(set) T $x` promoted form */`` |
|         3 |  6196 | `						iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|         3 |  6197 | `						pIn += nSetTok;` |
|         1 |  6198 | `					}` |
|        89 |  6199 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|        18 |  6200 | `						bReadonly = 1;` |
|        18 |  6201 | `						pIn++;` |
|         7 |  6202 | `					}` |
|        42 |  6203 | `				}` |
|     45484 |  6204 | `			}` |
|   1708019 |  6205 | `			if( iSetVisFlag == PH7_CLASS_ATTR_PRIVATE_SET ){` |
|         5 |  6206 | `				sArg.iFlags \|= VM_FUNC_ARG_PRIV_SET;` |
|   1708017 |  6207 | `			}else if( iSetVisFlag == PH7_CLASS_ATTR_PROTECTED_SET ){` |
|       ! 0 |  6208 | `				sArg.iFlags \|= VM_FUNC_ARG_PROT_SET;` |
|       ! 0 |  6209 | `			}` |
|   1708019 |  6210 | `			if( bVisSeen \|\| bReadonly ){` |
|        93 |  6211 | `				if( !bCtorCtx ){` |
|         6 |  6212 | `					if( bAbstractCtx ){` |
|         3 |  6213 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  6214 | `							"Cannot declare promoted property in an abstract constructor");` |
|         2 |  6215 | `					}else{` |
|         3 |  6216 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|         - |  6217 | `							"Cannot declare promoted property outside a constructor");` |
|         - |  6218 | `					}` |
|         6 |  6219 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  6220 | `						return SXERR_ABORT;` |
|         - |  6221 | `					}` |
|         6 |  6222 | `					return SXERR_SYNTAX;` |
|         - |  6223 | `				}` |
|        89 |  6224 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|        89 |  6225 | `				sArg.iPromoteVis = iVis;` |
|        89 |  6226 | `				if( bReadonly ){` |
|        20 |  6227 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|         8 |  6228 | `				}` |
|        42 |  6229 | `			}` |
|         - |  6230 | `		}` |
|         - |  6231 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|   1708010 |  6232 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|    929141 |  6233 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|    144353 |  6234 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|    118657 |  6235 | `			sxu32 nLineLocal = pIn->nLine;` |
|    118657 |  6236 | `			sxi32 iTFlags = 0;` |
|    118657 |  6237 | `			pGen->pIn = pIn;` |
|    118657 |  6238 | `			rc = GenStateParseUnionTypeDecl(` |
|     59326 |  6239 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|     59326 |  6240 | `				&iTFlags, &sArg.sTypeName,` |
|         - |  6241 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|         - |  6242 | `				/* bAllowVoid */ 0,` |
|     59326 |  6243 | `						nLineLocal);` |
|    118657 |  6244 | `			pIn = pGen->pIn;` |
|    118657 |  6245 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  6246 | `				return SXERR_ABORT;` |
|    118657 |  6247 | `			}else if( rc == SXERR_CORRUPT ){` |
|         - |  6248 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|         3 |  6249 | `				return SXERR_SYNTAX;` |
|    118655 |  6250 | `			}else if( rc == SXERR_SYNTAX ){` |
|        11 |  6251 | `				if( pIn < pEnd ){` |
|        15 |  6252 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|         - |  6253 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|         4 |  6254 | `						&pIn->sData);` |
|         7 |  6255 | `				}else{` |
|       ! 0 |  6256 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|         - |  6257 | `						"syntax error, unexpected end of file");` |
|         - |  6258 | `				}` |
|        11 |  6259 | `				return SXERR_SYNTAX;` |
|         - |  6260 | `			}` |
|    118647 |  6261 | `			sArg.iFlags \|= iTFlags;` |
|     59321 |  6262 | `		}` |
|   1708005 |  6263 | `		if( pIn >= pEnd ){` |
|       ! 0 |  6264 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|       ! 0 |  6265 | `			return rc;` |
|         - |  6266 | `		}` |
|   1708005 |  6267 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|         - |  6268 | `			/* Pass by reference,record that */` |
|     11861 |  6269 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|     11861 |  6270 | `			pIn++;` |
|      5928 |  6271 | `		}` |
|   1708005 |  6272 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|         - |  6273 | `			/* Variadic parameter: ...$args */` |
|     19799 |  6274 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|     19799 |  6275 | `			pIn++;` |
|      9897 |  6276 | `		}` |
|   1708005 |  6277 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  6278 | `			/* Invalid argument */` |
|       ! 0 |  6279 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|       ! 0 |  6280 | `			return rc;` |
|         - |  6281 | `		}` |
|   1708005 |  6282 | `		pIn++; /* Jump the dollar sign */` |
|         - |  6283 | `		/* Copy argument name */` |
|   1708005 |  6284 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   1708005 |  6285 | `		if( zDup == 0 ){` |
|       ! 0 |  6286 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|       ! 0 |  6287 | `			return SXERR_ABORT;` |
|         - |  6288 | `		}` |
|   1708005 |  6289 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   1708005 |  6290 | `		pIn++;` |
|   1708005 |  6291 | `		if( pIn < pEnd ){` |
|    883189 |  6292 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|         - |  6293 | `				SyToken *pDefend;` |
|    468787 |  6294 | `				sxi32 iNest = 0;` |
|    468787 |  6295 | `				pIn++; /* Jump the equal sign */` |
|    468787 |  6296 | `				pDefend = pIn;` |
|         - |  6297 | `				/* Process the default value associated with this argument */` |
|    988789 |  6298 | `				while( pDefend < pEnd ){` |
|    681515 |  6299 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|    161513 |  6300 | `						break;` |
|         - |  6301 | `					}` |
|    520007 |  6302 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|         - |  6303 | `						/* Increment nesting level */` |
|     27579 |  6304 | `						iNest++;` |
|    506220 |  6305 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|         - |  6306 | `						/* Decrement nesting level */` |
|     27579 |  6307 | `						iNest--;` |
|     13787 |  6308 | `					}` |
|    520007 |  6309 | `					pDefend++;` |
|         5 |  6310 | `				}` |
|    468787 |  6311 | `				if( pIn >= pDefend ){` |
|         3 |  6312 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|         3 |  6313 | `					return rc;` |
|         - |  6314 | `				}` |
|         - |  6315 | `				/* Process default value */` |
|    468785 |  6316 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|    468785 |  6317 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  6318 | `					return rc;` |
|         - |  6319 | `				}` |
|         - |  6320 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|         - |  6321 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|         - |  6322 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|         - |  6323 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|         - |  6324 | `				 * arg-type check lets null through. */` |
|    468780 |  6325 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|    260010 |  6326 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|    260007 |  6327 | `					&& &pIn[1] == pDefend` |
|     47295 |  6328 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|     35464 |  6329 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|     21670 |  6330 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|     15763 |  6331 | `					sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|         - |  6332 | `					/* php 8.4: the implicit form is deprecated at COMPILE time —` |
|         - |  6333 | `` 					 * `f(): Implicitly marking parameter $x as nullable …` `` |
|         - |  6334 | `					 * (methods carry the Class:: prefix when the class link is` |
|         - |  6335 | `					 * already up at this point). */` |
|         - |  6336 | `					{` |
|     15763 |  6337 | `						const char *zSep = "";` |
|     15763 |  6338 | `						SyString sCls = { "", 0 };` |
|     15763 |  6339 | `						if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|     15757 |  6340 | `							sCls = ((ph7_class *)pFunc->pUserData)->sName;` |
|     15757 |  6341 | `							zSep = "::";` |
|      7876 |  6342 | `						}` |
|     23642 |  6343 | `						PH7_GenCompileError(&(*pGen),8192 /* E_DEPRECATED */,pIn->nLine,` |
|         - |  6344 | `							"%z%s%z(): Implicitly marking parameter $%z as nullable is deprecated, the explicit nullable type must be used instead",` |
|      7879 |  6345 | `							&sCls,zSep,&pFunc->sName,&sArg.sName);` |
|         - |  6346 | `					}` |
|      7879 |  6347 | `				}` |
|         - |  6348 | `				/* Point beyond the default value */` |
|    468785 |  6349 | `				pIn = pDefend;` |
|    234390 |  6350 | `			}` |
|    883187 |  6351 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       ! 0 |  6352 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|       ! 0 |  6353 | `				return rc;` |
|         - |  6354 | `			}` |
|    883187 |  6355 | `			pIn++; /* Jump the trailing comma */` |
|    441591 |  6356 | `		}` |
|         - |  6357 | `		/* Append argument signature */` |
|   1708003 |  6358 | `		if( sArg.nType > 0 ){` |
|    118585 |  6359 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|         - |  6360 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|     27651 |  6361 | `				int marker = 'o';` |
|     27651 |  6362 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|     27651 |  6363 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|     13828 |  6364 | `			}else{` |
|         - |  6365 | `				int c;` |
|     90939 |  6366 | `				c = 'n'; /* cc warning */` |
|         - |  6367 | `				/* Type leading character */` |
|     90939 |  6368 | `				switch(sArg.nType){` |
|      5913 |  6369 | `				case MEMOBJ_HASHMAP:` |
|         - |  6370 | `					/* Hashmap aka 'array' */` |
|     11831 |  6371 | `					c = 'h';` |
|     11831 |  6372 | `					break;` |
|      9965 |  6373 | `				case MEMOBJ_INT:` |
|         - |  6374 | `					/* Integer */` |
|     19935 |  6375 | `					c = 'i';` |
|     19935 |  6376 | `					break;` |
|         2 |  6377 | `				case MEMOBJ_BOOL:` |
|         - |  6378 | `					/* Bool */` |
|         5 |  6379 | `					c = 'b';` |
|         5 |  6380 | `					break;` |
|         5 |  6381 | `				case MEMOBJ_REAL:` |
|         - |  6382 | `					/* Float */` |
|        12 |  6383 | `					c = 'f';` |
|        12 |  6384 | `					break;` |
|     29574 |  6385 | `				case MEMOBJ_STRING:` |
|         - |  6386 | `					/* String */` |
|     59153 |  6387 | `					c = 's';` |
|     59153 |  6388 | `					break;` |
|         7 |  6389 | `				case MEMOBJ_OBJ:` |
|         - |  6390 | `					/* Object */` |
|        16 |  6391 | `					c = 'o';` |
|        14 |  6392 | `					break;` |
|         1 |  6393 | `				default:` |
|         2 |  6394 | `					break;` |
|         - |  6395 | `				}` |
|     90939 |  6396 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|         - |  6397 | `			}` |
|     59295 |  6398 | `		}else{` |
|         - |  6399 | `			/* No type is associated with this parameter which mean` |
|         - |  6400 | `			 * that this function is not condidate for overloading.` |
|         - |  6401 | `			 */` |
|   1589423 |  6402 | `			SyBlobRelease(&sSig);` |
|         - |  6403 | `		}` |
|         - |  6404 | `		/* Save in the argument set */` |
|   1708003 |  6405 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|         5 |  6406 | `	}` |
|   1132099 |  6407 | `	if( SyBlobLength(&sSig) > 0 ){` |
|         - |  6408 | `		/* Save function signature */` |
|     87009 |  6409 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|     43502 |  6410 | `	}` |
|   1132099 |  6411 | `	return SXRET_OK;` |
|    566060 |  6412 | `}` |
|         - |  6413 | `/*` |
|         - |  6414 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|         - |  6415 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|         - |  6416 | ` * the enclosing function. Returns the token just past the nested construct.` |
|         - |  6417 | ` */` |
|     35484 |  6418 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|         5 |  6419 | `{` |
|     35489 |  6420 | `	sxi32 iParen = 0;` |
|     35489 |  6421 | `	pIn++; /* past 'function'/'fn' */` |
|         - |  6422 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|         - |  6423 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|         - |  6424 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|    157753 |  6425 | `	while( pIn < pEnd ){` |
|    157753 |  6426 | `		sxu32 t = pIn->nType;` |
|    157753 |  6427 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|    153761 |  6428 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|    106451 |  6429 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|     86719 |  6430 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|    122269 |  6431 | `		pIn++;` |
|         5 |  6432 | `	}` |
|     19737 |  6433 | `	if( pIn >= pEnd ){ return pIn; }` |
|         - |  6434 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|         - |  6435 | `	{` |
|     19737 |  6436 | `		sxi32 d = 0;` |
|    784087 |  6437 | `		while( pIn < pEnd ){` |
|    784087 |  6438 | `			sxu32 t = pIn->nType;` |
|    784087 |  6439 | `			if( t & PH7_TK_OCB ){ d++; }` |
|    752537 |  6440 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|    764355 |  6441 | `			pIn++;` |
|         5 |  6442 | `		}` |
|         - |  6443 | `	}` |
|     19737 |  6444 | `	return pIn;` |
|     17747 |  6445 | `}` |
|         - |  6446 | `/*` |
|         - |  6447 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|         - |  6448 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|         - |  6449 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|         - |  6450 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|         - |  6451 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|         - |  6452 | ` * detached-mini-program path untouched.` |
|         - |  6453 | ` */` |
|         - |  6454 | `/*` |
|         - |  6455 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|         - |  6456 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|         - |  6457 | ` * mixed, object.` |
|         - |  6458 | ` */` |
|     11842 |  6459 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|         5 |  6460 | `{` |
|         - |  6461 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|         - |  6462 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|         - |  6463 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|         - |  6464 | `	};` |
|         - |  6465 | `	sxu32 i;` |
|     11847 |  6466 | `	if( nName > 0 && zName[0] == '\\' ){` |
|       ! 0 |  6467 | `		zName++;` |
|       ! 0 |  6468 | `		nName--;` |
|       ! 0 |  6469 | `	}` |
|     11879 |  6470 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|     11875 |  6471 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|     11843 |  6472 | `			return 1;` |
|         - |  6473 | `		}` |
|        17 |  6474 | `	}` |
|         5 |  6475 | `	return 0;` |
|      5926 |  6476 | `}` |
|         - |  6477 | `/*` |
|         - |  6478 | ` * One atom of a generator's declared return type: is it a supertype of` |
|         - |  6479 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|         - |  6480 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|         - |  6481 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|         - |  6482 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|         - |  6483 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|         - |  6484 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|         - |  6485 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|         - |  6486 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|         - |  6487 | ` * both rather than fatal on valid code (a recorded divergence).` |
|         - |  6488 | ` */` |
|     11840 |  6489 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|         5 |  6490 | `{` |
|     11845 |  6491 | `	if( nType == MEMOBJ_OBJ ){` |
|       ! 0 |  6492 | ``		return 1; /* bare `object` */`` |
|         - |  6493 | `	}` |
|     11845 |  6494 | `	if( nType != SXU32_HIGH ){` |
|         3 |  6495 | `		return 0; /* scalar/array/void/never/null/... */` |
|         - |  6496 | `	}` |
|     11843 |  6497 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|     11839 |  6498 | `		return 1;` |
|         - |  6499 | `	}` |
|         - |  6500 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|         - |  6501 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|         - |  6502 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|         - |  6503 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|         - |  6504 | `	{` |
|         - |  6505 | `		SyBlob sFQN;` |
|         - |  6506 | `		int bOk;` |
|         5 |  6507 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|         5 |  6508 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|         5 |  6509 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|         5 |  6510 | `		SyBlobRelease(&sFQN);` |
|         5 |  6511 | `		return bOk;` |
|         - |  6512 | `	}` |
|      5925 |  6513 | `}` |
|         - |  6514 | `/*` |
|         - |  6515 | ` * php 8: a generator function may only declare a return type that is a` |
|         - |  6516 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|         - |  6517 | ` * group qualifies only if every member does. Anything else is php's exact` |
|         - |  6518 | ` * compile-time fatal "Generator return type must be a supertype of` |
|         - |  6519 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|         - |  6520 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|         - |  6521 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|         - |  6522 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|         - |  6523 | ` */` |
|     12078 |  6524 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|         5 |  6525 | `{` |
|     12083 |  6526 | `	int bOk = 0;` |
|         - |  6527 | `	sxu32 nLine;` |
|         - |  6528 | `	sxi32 rc;` |
|     12083 |  6529 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|       243 |  6530 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|         - |  6531 | `	}` |
|     11845 |  6532 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|       ! 0 |  6533 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|       ! 0 |  6534 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|         - |  6535 | `		sxu32 i,j;` |
|       ! 0 |  6536 | `		for( i = 0; i < n && !bOk; i++ ){` |
|         - |  6537 | `			int bGroupOk;` |
|       ! 0 |  6538 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|       ! 0 |  6539 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|         - |  6540 | `			}` |
|       ! 0 |  6541 | `			bGroupOk = 1;` |
|       ! 0 |  6542 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|       ! 0 |  6543 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|       ! 0 |  6544 | `					bGroupOk = 0;` |
|       ! 0 |  6545 | `					break;` |
|         - |  6546 | `				}` |
|       ! 0 |  6547 | `			}` |
|       ! 0 |  6548 | `			bOk = bGroupOk;` |
|       ! 0 |  6549 | `		}` |
|       ! 0 |  6550 | `	}else{` |
|     11845 |  6551 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|         - |  6552 | `	}` |
|     11845 |  6553 | `	if( bOk ){` |
|     11843 |  6554 | `		return SXRET_OK;` |
|         - |  6555 | `	}` |
|         - |  6556 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|         - |  6557 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|         - |  6558 | `	 * token of this stream — its line is the function's closing brace. php` |
|         - |  6559 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|         - |  6560 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|         3 |  6561 | `	nLine = pGen->pIn[-1].nLine;` |
|         - |  6562 | `	{` |
|         3 |  6563 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|         3 |  6564 | `		if( sGiven.nByte < 1 ){` |
|       ! 0 |  6565 | `			sGiven = pFunc->sReturnClass;` |
|       ! 0 |  6566 | `		}` |
|         3 |  6567 | `		if( sGiven.nByte < 1 ){` |
|         - |  6568 | ``			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the`` |
|         - |  6569 | `			 * rendered type text, so sReturnTypeName arrives empty for them —` |
|         - |  6570 | `			 * name them here (the root fix belongs to that renderer, §3.7). */` |
|       ! 0 |  6571 | `			const char *zScalar =` |
|       ! 0 |  6572 | `				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :` |
|       ! 0 |  6573 | `				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";` |
|       ! 0 |  6574 | `			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));` |
|       ! 0 |  6575 | `		}` |
|         3 |  6576 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - |  6577 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|         - |  6578 | `	}` |
|         3 |  6579 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|      6044 |  6580 | `}` |
|   2626244 |  6581 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|         5 |  6582 | `{` |
|   2626249 |  6583 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|   2626249 |  6584 | `	SyToken *pEnd = pGen->pEnd;` |
|   2626249 |  6585 | `	sxi32 iDepth = 0;` |
|   2626249 |  6586 | `	int bStarted = 0;` |
| 115896179 |  6587 | `	while( pIn < pEnd ){` |
| 115896179 |  6588 | `		sxu32 t = pIn->nType;` |
| 115896179 |  6589 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 110579317 |  6590 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 105298275 |  6591 | `		if( t & PH7_TK_KEYWORD ){` |
|   7684375 |  6592 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|   7684375 |  6593 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|   7672297 |  6594 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|         - |  6595 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|   3818404 |  6596 | `		}` |
| 105250713 |  6597 | `		pIn++;` |
|         5 |  6598 | `	}` |
|   2614171 |  6599 | `	return FALSE;` |
|   1313127 |  6600 | `}` |
|         - |  6601 | `/*` |
|         - |  6602 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|         - |  6603 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|         - |  6604 | ` * and this routine takes care of generating the appropriate error message.` |
|         - |  6605 | ` */` |
|   2626244 |  6606 | `static sxi32 GenStateCompileFuncBody(` |
|         - |  6607 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |  6608 | `	ph7_vm_func *pFunc    /* Function state */` |
|         - |  6609 | `	)` |
|         5 |  6610 | `{` |
|         - |  6611 | `	SySet *pInstrContainer; /* Instruction container */` |
|         - |  6612 | `	GenBlock *pBlock;` |
|         - |  6613 | `	sxu32 nGotoOfft;` |
|         - |  6614 | `	sxi32 rc;` |
|         - |  6615 | `	/* Attach the new function */` |
|   2626249 |  6616 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   2626249 |  6617 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  6618 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|         - |  6619 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  6620 | `		return SXERR_ABORT;` |
|         - |  6621 | `	}` |
|   2626249 |  6622 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|         - |  6623 | `	/* Swap bytecode containers */` |
|   2626249 |  6624 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   2626249 |  6625 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|         - |  6626 | `	/* Emit constructor property promotion prologue:` |
|         - |  6627 | `	 *   $this->NAME = $NAME;` |
|         - |  6628 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|         - |  6629 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|         - |  6630 | `	{` |
|   2626249 |  6631 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|         - |  6632 | `		sxu32 i;` |
|   4278989 |  6633 | `		for( i = 0; i < nArg; i++ ){` |
|   1652745 |  6634 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|         - |  6635 | `			char *zSrc;` |
|         - |  6636 | `			sxu32 nSrc,nName;` |
|         - |  6637 | `			SySet sToken;` |
|         - |  6638 | `			SyToken *pTmpIn,*pTmpEnd;` |
|         - |  6639 | `			sxi32 rcPromote;` |
|   1652745 |  6640 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   1652671 |  6641 | `				continue;` |
|         - |  6642 | `			}` |
|         - |  6643 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|         - |  6644 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|         - |  6645 | `			 * copied), so it must outlive the function — never free it. The` |
|         - |  6646 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|         - |  6647 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|        79 |  6648 | `			nName = SyStringLength(&pArg->sName);` |
|        79 |  6649 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|        79 |  6650 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|        79 |  6651 | `			if( zSrc == 0 ){` |
|       ! 0 |  6652 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  6653 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  6654 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  6655 | `				return SXERR_ABORT;` |
|         - |  6656 | `			}` |
|         - |  6657 | `			{` |
|        79 |  6658 | `				char *z = zSrc;` |
|        79 |  6659 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|        79 |  6660 | `				z += sizeof("$this->")-1;` |
|        79 |  6661 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        79 |  6662 | `				z += nName;` |
|        79 |  6663 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|        79 |  6664 | `				z += sizeof(" = $")-1;` |
|        79 |  6665 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|        79 |  6666 | `				z += nName;` |
|        79 |  6667 | `				*z = 0;` |
|         - |  6668 | `			}` |
|        79 |  6669 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        79 |  6670 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);` |
|        79 |  6671 | `			pTmpIn = pGen->pIn;` |
|        79 |  6672 | `			pTmpEnd = pGen->pEnd;` |
|        79 |  6673 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|        79 |  6674 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|        79 |  6675 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|        79 |  6676 | `			pGen->pIn = pTmpIn;` |
|        79 |  6677 | `			pGen->pEnd = pTmpEnd;` |
|        79 |  6678 | `			SySetRelease(&sToken);` |
|        79 |  6679 | `			if( rcPromote == SXERR_ABORT ){` |
|       ! 0 |  6680 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 |  6681 | `				GenStateLeaveBlock(&(*pGen),0);` |
|       ! 0 |  6682 | `				return SXERR_ABORT;` |
|         - |  6683 | `			}` |
|         - |  6684 | `			/* Discard the assignment result — this is a statement expression. */` |
|        79 |  6685 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        42 |  6686 | `		}` |
|         - |  6687 | `	}` |
|         - |  6688 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|         - |  6689 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|         - |  6690 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|         - |  6691 | `	 * generator — and vice versa — is classified independently. */` |
|         - |  6692 | `	{` |
|   2626249 |  6693 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|   2626249 |  6694 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|         - |  6695 | `		/* Compile the body */` |
|   2626249 |  6696 | `		PH7_CompileBlock(&(*pGen),0);` |
|   2626249 |  6697 | `		pGen->bInGenerator = bSavedGen;` |
|         - |  6698 | `	}` |
|         - |  6699 | `	/* Fix exception jumps now the destination is resolved */` |
|   2626249 |  6700 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - |  6701 | `	/* Emit the final return if not yet done */` |
|   2626249 |  6702 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - |  6703 | `	/* Fix gotos jumps now the destination is resolved */` |
|   2626249 |  6704 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|       ! 0 |  6705 | `		rc = SXERR_ABORT;` |
|       ! 0 |  6706 | `	}` |
|   2626249 |  6707 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|         - |  6708 | `	/* Restore the default container */` |
|   2626249 |  6709 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - |  6710 | `	/* Leave function block */` |
|   2626249 |  6711 | `	GenStateLeaveBlock(&(*pGen),0);` |
|   2626249 |  6712 | `	if( rc == SXERR_ABORT ){` |
|         - |  6713 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  6714 | `		return SXERR_ABORT;` |
|         - |  6715 | `	}` |
|         - |  6716 | `	/* Scan for yield opcodes to detect generator functions */` |
|         - |  6717 | `	{` |
|   2626249 |  6718 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|         - |  6719 | `		sxu32 i;` |
|  71765143 |  6720 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
|  69150977 |  6721 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     12083 |  6722 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     12083 |  6723 | `				break;` |
|         - |  6724 | `			}` |
|  34569452 |  6725 | `		}` |
|         - |  6726 | `	}` |
|   2626249 |  6727 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|         - |  6728 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|     12083 |  6729 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|       ! 0 |  6730 | `			return SXERR_ABORT;` |
|         - |  6731 | `		}` |
|      6039 |  6732 | `	}` |
|         - |  6733 | `	/* All done, function body compiled */` |
|   2626249 |  6734 | `	return SXRET_OK;` |
|   1313127 |  6735 | `}` |
|         - |  6736 | `/*` |
|         - |  6737 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|         - |  6738 | ` * According to the PHP language reference manual.` |
|         - |  6739 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|         - |  6740 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|         - |  6741 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|         - |  6742 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - |  6743 | ` *  Functions need not be defined before they are referenced.` |
|         - |  6744 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|         - |  6745 | ` *  a function even if they were defined inside and vice versa.` |
|         - |  6746 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|         - |  6747 | ` *  calls with over 32-64 recursion levels.` |
|         - |  6748 | ` *` |
|         - |  6749 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|         - |  6750 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|         - |  6751 | ` * on these extension.` |
|         - |  6752 | ` */` |
|         - |  6753 | `/*` |
|         - |  6754 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|         - |  6755 | ` */` |
|       570 |  6756 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|         5 |  6757 | `{` |
|         - |  6758 | `	sxu32 i;` |
|      1611 |  6759 | `	for( i = 0; i < n; i++ ){` |
|      1381 |  6760 | `		int a = zA[i], b = zB[i];` |
|      1381 |  6761 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|      1381 |  6762 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|      1381 |  6763 | `		if( a != b ) return a - b;` |
|       523 |  6764 | `	}` |
|       235 |  6765 | `	return 0;` |
|       290 |  6766 | `}` |
|         - |  6767 | `/*` |
|         - |  6768 | ` * Internal type-atom kinds used during union type parsing.` |
|         - |  6769 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|         - |  6770 | ` * (which are positive bit values stored in sxu32).` |
|         - |  6771 | ` */` |
|         - |  6772 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|         - |  6773 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|         - |  6774 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|         - |  6775 |  |
|         - |  6776 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|         - |  6777 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|         - |  6778 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|         - |  6779 |  |
|         - |  6780 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|         - |  6781 | `struct PhlTypeAtom {` |
|         - |  6782 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|         - |  6783 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|         - |  6784 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|         - |  6785 | `	sxu32 nCanon;` |
|         - |  6786 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|         - |  6787 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|         - |  6788 | `};` |
|         - |  6789 |  |
|         - |  6790 | `/*` |
|         - |  6791 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|         - |  6792 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|         - |  6793 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|         - |  6794 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|         - |  6795 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|         - |  6796 | ` * already be consumed by the caller.` |
|         - |  6797 | ` */` |
|    131646 |  6798 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|         5 |  6799 | `{` |
|    131651 |  6800 | `	SyToken *pIn = pGen->pIn;` |
|    131651 |  6801 | `	SyZero(pOut, sizeof(*pOut));` |
|    131651 |  6802 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|    131651 |  6803 | `	if( pIn >= pGen->pEnd ){` |
|       ! 0 |  6804 | `		return SXERR_SYNTAX;` |
|         - |  6805 | `	}` |
|         - |  6806 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|    131651 |  6807 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|         8 |  6808 | `		pIn++;` |
|         8 |  6809 | `		if( pIn >= pGen->pEnd ){` |
|       ! 0 |  6810 | `			return SXERR_SYNTAX;` |
|         - |  6811 | `		}` |
|         3 |  6812 | `	}` |
|    131651 |  6813 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  6814 | `		return SXERR_SYNTAX;` |
|         - |  6815 | `	}` |
|    131651 |  6816 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|     91731 |  6817 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|     91731 |  6818 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|     11863 |  6819 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|     85802 |  6820 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|        81 |  6821 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|     79835 |  6822 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|     20341 |  6823 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|     69629 |  6824 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|     59379 |  6825 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|     29774 |  6826 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|        41 |  6827 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|        67 |  6828 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|        27 |  6829 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|        37 |  6830 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|        13 |  6831 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|        23 |  6832 | `			pOut->nType = SXU32_HIGH;` |
|        23 |  6833 | `			pOut->sClass = pIn->sData;` |
|        13 |  6834 | `		}else{` |
|         3 |  6835 | `			return SXERR_SYNTAX;` |
|         - |  6836 | `		}` |
|     91729 |  6837 | `		pIn++;` |
|     45867 |  6838 | `	}else{` |
|         - |  6839 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|         - |  6840 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|     39925 |  6841 | `		SyString *pT = &pIn->sData;` |
|     39925 |  6842 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|        34 |  6843 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|        34 |  6844 | `			pIn++;` |
|     39910 |  6845 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|       177 |  6846 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|       177 |  6847 | `			pIn++;` |
|     39809 |  6848 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|        26 |  6849 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|        26 |  6850 | `			pIn++;` |
|        15 |  6851 | `		}else{` |
|         - |  6852 | `			/* Class / interface name; consume namespace path a\b\c */` |
|     39701 |  6853 | `			SyToken *pFirst = pIn;` |
|     39701 |  6854 | `			SyToken *pLast = pIn;` |
|     39701 |  6855 | `			pOut->nType = SXU32_HIGH;` |
|     39701 |  6856 | `			pOut->sClass = pIn->sData;` |
|     39701 |  6857 | `			pIn++;` |
|     59547 |  6858 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|     39704 |  6859 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|         3 |  6860 | `				pLast = &pIn[1];` |
|         3 |  6861 | `				pIn += 2;` |
|         1 |  6862 | `			}` |
|     39701 |  6863 | `			if( pLast != pFirst ){` |
|         3 |  6864 | `				const char *zFirst = pFirst->sData.zString;` |
|         3 |  6865 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|         3 |  6866 | `				pOut->sClass.zString = zFirst;` |
|         3 |  6867 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|         1 |  6868 | `			}` |
|         - |  6869 | `		}` |
|         - |  6870 | `	}` |
|    131649 |  6871 | `	pGen->pIn = pIn;` |
|    131649 |  6872 | `	return SXRET_OK;` |
|     65828 |  6873 | `}` |
|         - |  6874 |  |
|         - |  6875 | `/*` |
|         - |  6876 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|         - |  6877 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|         - |  6878 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|         - |  6879 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|         - |  6880 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|         - |  6881 | ` */` |
|    131468 |  6882 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|         5 |  6883 | `{` |
|         - |  6884 | `	int i;` |
|    131473 |  6885 | `	int nNonNull = 0;` |
|    131473 |  6886 | `	int bAnyIntersection = 0;` |
|         - |  6887 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|    131473 |  6888 | `	sxu32 nMaxGroup = 0;` |
|   4338449 |  6889 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    263093 |  6890 | `	for( i = 0; i < nAtoms; i++ ){` |
|    131625 |  6891 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    131595 |  6892 | `			nNonNull++;` |
|    131595 |  6893 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|    131595 |  6894 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|    131595 |  6895 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|     65795 |  6896 | `			}` |
|     65795 |  6897 | `		}` |
|     65815 |  6898 | `	}` |
|    263041 |  6899 | `	for( i = 0; i < nAtoms; i++ ){` |
|    131597 |  6900 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        29 |  6901 | `			bAnyIntersection = 1;` |
|        29 |  6902 | `			break;` |
|         - |  6903 | `		}` |
|     65789 |  6904 | `	}` |
|    131473 |  6905 | `	if( bAnyIntersection ){` |
|         - |  6906 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|         - |  6907 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|         - |  6908 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|        29 |  6909 | `		sxu32 g, nGroups = 0;` |
|        29 |  6910 | `		int bFirstGroup = 1;` |
|        59 |  6911 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|        59 |  6912 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|        35 |  6913 | `			int bFirstMember = 1;` |
|         - |  6914 | `			int bWrap;` |
|        35 |  6915 | `			if( aGroupCount[g] == 0 ) continue;` |
|         - |  6916 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|         - |  6917 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|         - |  6918 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|         - |  6919 | `			 * parens, matching PHP's canonical text. */` |
|        47 |  6920 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|        35 |  6921 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|        35 |  6922 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|       107 |  6923 | `			for( i = 0; i < nAtoms; i++ ){` |
|        77 |  6924 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|        59 |  6925 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|        59 |  6926 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|        55 |  6927 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        30 |  6928 | `				}else{` |
|         6 |  6929 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  6930 | `				}` |
|        59 |  6931 | `				bFirstMember = 0;` |
|        32 |  6932 | `			}` |
|        35 |  6933 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|        35 |  6934 | `			bFirstGroup = 0;` |
|        20 |  6935 | `		}` |
|        29 |  6936 | `		if( bNullable ){` |
|       ! 0 |  6937 | `			SyBlobAppend(pBlob, "\|", 1);` |
|       ! 0 |  6938 | `			SyBlobAppend(pBlob, "null", 4);` |
|       ! 0 |  6939 | `		}` |
|        83 |  6940 | `		return;` |
|         - |  6941 | `	}` |
|    131449 |  6942 | `	if( nNonNull == 1 && bNullable ){` |
|         - |  6943 | `		/* Shorthand: ?T */` |
|       112 |  6944 | `		for( i = 0; i < nAtoms; i++ ){` |
|       112 |  6945 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       112 |  6946 | `			SyBlobAppend(pBlob, "?", 1);` |
|       112 |  6947 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|        23 |  6948 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        13 |  6949 | `			}else{` |
|        92 |  6950 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|         - |  6951 | `			}` |
|       112 |  6952 | `			return;` |
|       ! 0 |  6953 | `		}` |
|       ! 0 |  6954 | `	}` |
|         - |  6955 | `	{` |
|    131341 |  6956 | `		int bFirst = 1;` |
|         - |  6957 | `		/* 1) Classes in declaration order */` |
|    262785 |  6958 | `		for( i = 0; i < nAtoms; i++ ){` |
|    131449 |  6959 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|     39651 |  6960 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     39651 |  6961 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|     39651 |  6962 | `				bFirst = 0;` |
|     19823 |  6963 | `			}` |
|     65727 |  6964 | `		}` |
|         - |  6965 | `		/* 2) Built-ins in canonical order */` |
|         - |  6966 | `		{` |
|         - |  6967 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|         - |  6968 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|         - |  6969 | `			int k;` |
|    919357 |  6970 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|   1484977 |  6971 | `				for( i = 0; i < nAtoms; i++ ){` |
|    788557 |  6972 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|     91601 |  6973 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|     91601 |  6974 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|     91601 |  6975 | `						bFirst = 0;` |
|     91601 |  6976 | `						break;` |
|         - |  6977 | `					}` |
|    348483 |  6978 | `				}` |
|    394013 |  6979 | `			}` |
|         - |  6980 | `		}` |
|         - |  6981 | `		/* 3) null suffix */` |
|    131341 |  6982 | `		if( bNullable ){` |
|        19 |  6983 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|        19 |  6984 | `			SyBlobAppend(pBlob, "null", 4);` |
|         8 |  6985 | `		}` |
|         - |  6986 | `	}` |
|     65739 |  6987 | `}` |
|         - |  6988 |  |
|         - |  6989 | `/*` |
|         - |  6990 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|         - |  6991 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|         - |  6992 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|         - |  6993 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|         - |  6994 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|         - |  6995 | ` * whether it was parenthesized.` |
|         - |  6996 | ` *` |
|         - |  6997 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|         - |  6998 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|         - |  6999 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|         - |  7000 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|         - |  7001 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|         - |  7002 | ` */` |
|    131620 |  7003 | `static sxi32 GenStateParsePart(` |
|         - |  7004 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|         - |  7005 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|         5 |  7006 | `{` |
|         - |  7007 | `	sxi32 rc;` |
|    131625 |  7008 | `	int nMembers = 0;` |
|    131625 |  7009 | `	int bParen = 0;` |
|    131625 |  7010 | `	*pnMembers = 0;` |
|    131625 |  7011 | `	*pbParen = 0;` |
|    131625 |  7012 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         9 |  7013 | `		bParen = 1;` |
|         9 |  7014 | `		pGen->pIn++; /* skip '(' */` |
|         3 |  7015 | `	}` |
|     65810 |  7016 | `	for(;;){` |
|    131651 |  7017 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|       ! 0 |  7018 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7019 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|       ! 0 |  7020 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7021 | `		}` |
|    131651 |  7022 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|    131651 |  7023 | `		if( rc != SXRET_OK ){` |
|         3 |  7024 | `			return rc;` |
|         - |  7025 | `		}` |
|    131649 |  7026 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|    131649 |  7027 | `		(*pnAtoms)++;` |
|    131649 |  7028 | `		nMembers++;` |
|         - |  7029 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|    131649 |  7030 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        39 |  7031 | `			SyToken *pNext = &pGen->pIn[1];` |
|        34 |  7032 | `			if( pNext < pGen->pEnd` |
|        39 |  7033 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        31 |  7034 | `				pGen->pIn++; /* skip '&' */` |
|        31 |  7035 | `				continue;` |
|         - |  7036 | `			}` |
|         4 |  7037 | `		}` |
|    131623 |  7038 | `		break;` |
|       ! 0 |  7039 | `	}` |
|    131623 |  7040 | `	if( bParen ){` |
|         9 |  7041 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  7042 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7043 | `				"Malformed DNF type: expecting ')'");` |
|       ! 0 |  7044 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7045 | `		}` |
|         9 |  7046 | `		pGen->pIn++; /* skip ')' */` |
|         9 |  7047 | `		if( nMembers < 2 ){` |
|       ! 0 |  7048 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7049 | `				"Parenthesized type must be an intersection of at least two types");` |
|       ! 0 |  7050 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7051 | `		}` |
|         3 |  7052 | `	}` |
|    131623 |  7053 | `	*pnMembers = nMembers;` |
|    131623 |  7054 | `	*pbParen = bParen;` |
|    131623 |  7055 | `	return SXRET_OK;` |
|     65815 |  7056 | `}` |
|         - |  7057 |  |
|         - |  7058 | `/*` |
|         - |  7059 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|         - |  7060 | ` *` |
|         - |  7061 | ` * Outputs:` |
|         - |  7062 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|         - |  7063 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|         - |  7064 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|         - |  7065 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|         - |  7066 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|         - |  7067 | ` *     already be initialized by the caller (allocator set, etc).` |
|         - |  7068 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|         - |  7069 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|         - |  7070 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|         - |  7071 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|         - |  7072 | ` *` |
|         - |  7073 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|         - |  7074 | ` * SXERR_ABORT on fatal compile errors.` |
|         - |  7075 | ` */` |
|    131484 |  7076 | `static sxi32 GenStateParseUnionTypeDecl(` |
|         - |  7077 | `	ph7_gen_state *pGen,` |
|         - |  7078 | `	sxu32 *pnType,` |
|         - |  7079 | `	SyString *pClass,` |
|         - |  7080 | `	SySet *pAlts,` |
|         - |  7081 | `	sxi32 *piTypeFlags,` |
|         - |  7082 | `	SyString *pTypeText,` |
|         - |  7083 | `	int iNullableFlag,` |
|         - |  7084 | `	int iUnionFlag,` |
|         - |  7085 | `	int bAllowVoid,` |
|         - |  7086 | `	sxu32 nLine` |
|         5 |  7087 | `){` |
|         - |  7088 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|    131489 |  7089 | `	int nAtoms = 0;` |
|    131489 |  7090 | `	int bShortNullable = 0;` |
|    131489 |  7091 | `	int bExplicitNull = 0;` |
|         - |  7092 | `	sxi32 rc;` |
|    131489 |  7093 | `	*pnType = 0;` |
|    131489 |  7094 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|    131489 |  7095 | `	*piTypeFlags = 0;` |
|    131489 |  7096 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|         - |  7097 |  |
|    131489 |  7098 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7099 | `		return SXRET_OK;` |
|         - |  7100 | `	}` |
|         - |  7101 | ``	/* Optional `?` shorthand prefix */`` |
|    131484 |  7102 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|       101 |  7103 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|       100 |  7104 | `		bShortNullable = 1;` |
|       100 |  7105 | `		pGen->pIn++;` |
|       100 |  7106 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7107 | `			return SXERR_SYNTAX;` |
|         - |  7108 | `		}` |
|        48 |  7109 | `	}` |
|         - |  7110 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|         - |  7111 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|         - |  7112 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|         - |  7113 | `	{` |
|         - |  7114 | `		int nMembers, bParen;` |
|    131489 |  7115 | `		sxu32 iGroup = 0;` |
|    131489 |  7116 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|    131489 |  7117 | `		if( rc != SXRET_OK ){` |
|         4 |  7118 | `			return rc;` |
|         - |  7119 | `		}` |
|         - |  7120 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|         - |  7121 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|         - |  7122 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|         - |  7123 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|         - |  7124 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|    197432 |  7125 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|    131696 |  7126 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       143 |  7127 | `			if( bShortNullable ){` |
|         - |  7128 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|         - |  7129 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|         - |  7130 | `				 * already reported" so callers skip their own error emission. */` |
|         3 |  7131 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - |  7132 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|         3 |  7133 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|         - |  7134 | `			}` |
|       141 |  7135 | `			if( nMembers >= 2 && !bParen ){` |
|       ! 0 |  7136 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|         - |  7137 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 |  7138 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7139 | `			}` |
|       141 |  7140 | ``			pGen->pIn++; /* skip `\|` */`` |
|       141 |  7141 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|       141 |  7142 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  7143 | `				return rc;` |
|         - |  7144 | `			}` |
|         5 |  7145 | `		}` |
|    131485 |  7146 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|       ! 0 |  7147 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7148 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|       ! 0 |  7149 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  7150 | `		}` |
|         - |  7151 | `	}` |
|         - |  7152 | `	/* Validation pass.` |
|         - |  7153 | `	 *` |
|         - |  7154 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|         - |  7155 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|         - |  7156 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|         - |  7157 | `	 */` |
|         - |  7158 | `	{` |
|         - |  7159 | `		int i, j;` |
|    131485 |  7160 | `		int bHasNonNull = 0;` |
|    131485 |  7161 | `		int bAnyIntersection = 0;` |
|         - |  7162 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|         - |  7163 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|         - |  7164 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
|   4338845 |  7165 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|    263127 |  7166 | `		for( i = 0; i < nAtoms; i++ ){` |
|    131647 |  7167 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|     65826 |  7168 | `		}` |
|    263071 |  7169 | `		for( i = 0; i < nAtoms; i++ ){` |
|    131617 |  7170 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|     65798 |  7171 | `		}` |
|         - |  7172 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|         - |  7173 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|    131485 |  7174 | `		if( bShortNullable && bAnyIntersection ){` |
|       ! 0 |  7175 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7176 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|       ! 0 |  7177 | `			return SXERR_SYNTAX;` |
|         - |  7178 | `		}` |
|    263113 |  7179 | `		for( i = 0; i < nAtoms; i++ ){` |
|         - |  7180 | `			/* Intersection members must be class/interface types (PHP rejects` |
|         - |  7181 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|         - |  7182 | ``			 * `true`/`false` in an intersection). */`` |
|    131645 |  7183 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|        55 |  7184 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|        55 |  7185 | `				if( bClassLike ){` |
|        53 |  7186 | `					SyString *pC = &aAtoms[i].sClass;` |
|        48 |  7187 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|        48 |  7188 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|        48 |  7189 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|        53 |  7190 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|       ! 0 |  7191 | `						bClassLike = 0;` |
|       ! 0 |  7192 | `					}` |
|        24 |  7193 | `				}` |
|        55 |  7194 | `				if( !bClassLike ){` |
|         - |  7195 | `					const char *zName; sxu32 nName;` |
|         3 |  7196 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 |  7197 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|       ! 0 |  7198 | `					}else{` |
|         3 |  7199 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|         - |  7200 | `					}` |
|         4 |  7201 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7202 | `						"Type %.*s cannot be part of an intersection type",` |
|         1 |  7203 | `						(int)nName, zName);` |
|         3 |  7204 | `					return SXERR_SYNTAX;` |
|         - |  7205 | `				}` |
|        24 |  7206 | `			}` |
|    131643 |  7207 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|       177 |  7208 | `				if( nAtoms > 1 ){` |
|         3 |  7209 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7210 | `						"Void can only be used as a standalone type");` |
|         3 |  7211 | `					return SXERR_SYNTAX;` |
|         - |  7212 | `				}` |
|       175 |  7213 | `				if( !bAllowVoid ){` |
|       ! 0 |  7214 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7215 | `						"void cannot be used here");` |
|       ! 0 |  7216 | `					return SXERR_SYNTAX;` |
|         - |  7217 | `				}` |
|       175 |  7218 | `				if( bShortNullable ){` |
|       ! 0 |  7219 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7220 | `						"Void type cannot be nullable");` |
|       ! 0 |  7221 | `					return SXERR_SYNTAX;` |
|         - |  7222 | `				}` |
|        85 |  7223 | `			}` |
|    131641 |  7224 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|         - |  7225 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|         - |  7226 | `				 * type (never = the function does not return). Mirrors the void` |
|         - |  7227 | `				 * validation above; accepted here and enforced at compile time` |
|         - |  7228 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|        26 |  7229 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|         - |  7230 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|         - |  7231 | `					 * same as any other non-standalone use. */` |
|         5 |  7232 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7233 | `						"never can only be used as a standalone type");` |
|         5 |  7234 | `					return SXERR_SYNTAX;` |
|         - |  7235 | `				}` |
|        21 |  7236 | `				if( !bAllowVoid ){` |
|         - |  7237 | `					/* Return-only: params call with bAllowVoid=0. */` |
|         3 |  7238 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7239 | `						"never cannot be used as a parameter type");` |
|         3 |  7240 | `					return SXERR_SYNTAX;` |
|         - |  7241 | `				}` |
|         8 |  7242 | `			}` |
|    131635 |  7243 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|        34 |  7244 | `				bExplicitNull = 1;` |
|        19 |  7245 | `			}else{` |
|    131605 |  7246 | `				bHasNonNull = 1;` |
|         - |  7247 | `			}` |
|         - |  7248 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|         - |  7249 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|         - |  7250 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|         - |  7251 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|         - |  7252 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|    131835 |  7253 | `			for( j = 0; j < i; j++ ){` |
|       207 |  7254 | `				int bDup = 0;` |
|       207 |  7255 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|       395 |  7256 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|       202 |  7257 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|       207 |  7258 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|       195 |  7259 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|        51 |  7260 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|        44 |  7261 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|        44 |  7262 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|        17 |  7263 | `								aAtoms[j].sClass.zString,` |
|        34 |  7264 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|       ! 0 |  7265 | `							bDup = 1;` |
|       ! 0 |  7266 | `						}` |
|        27 |  7267 | `					}else{` |
|         3 |  7268 | `						bDup = 1;` |
|         - |  7269 | `					}` |
|        23 |  7270 | `				}` |
|       195 |  7271 | `				if( bDup ){` |
|         - |  7272 | `					const char *zName;` |
|         - |  7273 | `					sxu32 nName;` |
|         3 |  7274 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       ! 0 |  7275 | `						zName = aAtoms[i].sClass.zString;` |
|       ! 0 |  7276 | `						nName = aAtoms[i].sClass.nByte;` |
|       ! 0 |  7277 | `					}else{` |
|         3 |  7278 | `						zName = aAtoms[i].zCanon;` |
|         3 |  7279 | `						nName = aAtoms[i].nCanon;` |
|         - |  7280 | `					}` |
|         4 |  7281 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         1 |  7282 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|         3 |  7283 | `					return SXERR_SYNTAX;` |
|         - |  7284 | `				}` |
|        99 |  7285 | `			}` |
|     65819 |  7286 | `		}` |
|    131473 |  7287 | `		if( !bHasNonNull && bExplicitNull ){` |
|         7 |  7288 | `			if( bShortNullable ){` |
|         - |  7289 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|       ! 0 |  7290 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|         - |  7291 | `					"Null can not be used as a standalone type");` |
|       ! 0 |  7292 | `				return SXERR_SYNTAX;` |
|         - |  7293 | `			}` |
|         - |  7294 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|         - |  7295 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|         - |  7296 | `			 * path below leaves *pnType untouched when there is no non-null` |
|         - |  7297 | `			 * atom, so set it here. */` |
|         7 |  7298 | `			*pnType = MEMOBJ_NULL;` |
|         3 |  7299 | `		}` |
|         - |  7300 | `	}` |
|         - |  7301 | `	/* Compute nullability flag */` |
|    131473 |  7302 | `	if( bShortNullable \|\| bExplicitNull ){` |
|       128 |  7303 | `		*piTypeFlags \|= iNullableFlag;` |
|        62 |  7304 | `	}` |
|         - |  7305 | `	/* Build canonical type text */` |
|    131473 |  7306 | `	if( pTypeText ){` |
|         - |  7307 | `		SyBlob sBlob;` |
|    131473 |  7308 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|    197160 |  7309 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|     65734 |  7310 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|    131473 |  7311 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|    196928 |  7312 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    131282 |  7313 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|    131287 |  7314 | `			if( zDup ){` |
|    131287 |  7315 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|     65641 |  7316 | `			}` |
|     65641 |  7317 | `		}` |
|    131473 |  7318 | `		SyBlobRelease(&sBlob);` |
|     65734 |  7319 | `	}` |
|         - |  7320 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|         - |  7321 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|         - |  7322 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|         - |  7323 | `	{` |
|    131473 |  7324 | `		int nNonNull = 0;` |
|    131473 |  7325 | `		int iNonNullIdx = -1;` |
|         - |  7326 | `		int i;` |
|    263093 |  7327 | `		for( i = 0; i < nAtoms; i++ ){` |
|    131625 |  7328 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    131595 |  7329 | `				nNonNull++;` |
|    131595 |  7330 | `				iNonNullIdx = i;` |
|     65795 |  7331 | `			}` |
|     65815 |  7332 | `		}` |
|    131473 |  7333 | `		if( nNonNull <= 1 ){` |
|         - |  7334 | `			/* Fast path: store as single type. */` |
|    131367 |  7335 | `			if( iNonNullIdx >= 0 ){` |
|    131361 |  7336 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|    131361 |  7337 | `				if( pA->nType == SXU32_HIGH ){` |
|     59438 |  7338 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     19811 |  7339 | `						pA->sClass.zString, pA->sClass.nByte);` |
|     39627 |  7340 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|     39627 |  7341 | `					*pnType = SXU32_HIGH;` |
|     39627 |  7342 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|    111550 |  7343 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|       175 |  7344 | `					*pnType = MEMOBJ_VOID;` |
|     91654 |  7345 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|        18 |  7346 | `					*pnType = MEMOBJ_NEVER;` |
|        10 |  7347 | `				}else{` |
|     91553 |  7348 | `					*pnType = pA->nType;` |
|         - |  7349 | `				}` |
|     65678 |  7350 | `			}` |
|     65686 |  7351 | `		}else{` |
|         - |  7352 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|       111 |  7353 | `			*piTypeFlags \|= iUnionFlag;` |
|       355 |  7354 | `			for( i = 0; i < nAtoms; i++ ){` |
|         - |  7355 | `				ph7_type_alt sAlt;` |
|       249 |  7356 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|       239 |  7357 | `				SyZero(&sAlt, sizeof(sAlt));` |
|       239 |  7358 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|       239 |  7359 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       146 |  7360 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        47 |  7361 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|        99 |  7362 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|        99 |  7363 | `					sAlt.nType = SXU32_HIGH;` |
|        99 |  7364 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|        52 |  7365 | `				}else{` |
|       145 |  7366 | `					sAlt.nType = aAtoms[i].nType;` |
|       145 |  7367 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|         - |  7368 | `				}` |
|       239 |  7369 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|       122 |  7370 | `			}` |
|         - |  7371 | `		}` |
|         - |  7372 | `	}` |
|    131473 |  7373 | `	return SXRET_OK;` |
|     65747 |  7374 | `}` |
|         - |  7375 |  |
|         - |  7376 | `/*` |
|         - |  7377 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|         - |  7378 | `` * pGen->pIn should point to the token after `)`.`` |
|         - |  7379 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|         - |  7380 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|         - |  7381 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|         - |  7382 | `` *          and union types `: T\|U`.`` |
|         - |  7383 | ` */` |
|   2768310 |  7384 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|         5 |  7385 | `{` |
|   2768315 |  7386 | `	sxi32 iFlags = 0;` |
|         - |  7387 | `	sxi32 rc;` |
|         - |  7388 | `	sxu32 nLine;` |
|   2768315 |  7389 | `	pFunc->nReturnType = 0;` |
|   2768315 |  7390 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|   2768315 |  7391 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|         - |  7392 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|         - |  7393 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|         - |  7394 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|         - |  7395 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|         - |  7396 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|   2768315 |  7397 | `	SySetReset(&pFunc->aReturnUnion);` |
|   2768315 |  7398 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|   2768315 |  7399 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|   2755843 |  7400 | `		return SXRET_OK;` |
|         - |  7401 | `	}` |
|     12477 |  7402 | `	pGen->pIn++; /* Skip ':' */` |
|     12477 |  7403 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7404 | `		return SXRET_OK;` |
|         - |  7405 | `	}` |
|     12477 |  7406 | `	nLine = pGen->pIn->nLine;` |
|     12477 |  7407 | `	rc = GenStateParseUnionTypeDecl(` |
|      6236 |  7408 | `		pGen,` |
|      6236 |  7409 | `		&pFunc->nReturnType,` |
|      6236 |  7410 | `		&pFunc->sReturnClass,` |
|      6236 |  7411 | `		&pFunc->aReturnUnion,` |
|         - |  7412 | `		&iFlags,` |
|      6236 |  7413 | `		&pFunc->sReturnTypeName,` |
|         - |  7414 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|         - |  7415 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|         - |  7416 | `		/* iUnionFlag */ 0,` |
|         - |  7417 | `		/* bAllowVoid */ 1,` |
|      6236 |  7418 | `		nLine);` |
|     12477 |  7419 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  7420 | `		return SXERR_ABORT;` |
|         - |  7421 | `	}` |
|     12477 |  7422 | `	if( rc == SXERR_CORRUPT ){` |
|         - |  7423 | `		/* Error already reported */` |
|       ! 0 |  7424 | `		return SXERR_SYNTAX;` |
|         - |  7425 | `	}` |
|     12477 |  7426 | `	if( rc == SXERR_SYNTAX ){` |
|         8 |  7427 | `		if( pGen->pIn < pGen->pEnd ){` |
|        11 |  7428 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|         - |  7429 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|         6 |  7430 | `				&pGen->pIn->sData);` |
|         5 |  7431 | `		}else{` |
|       ! 0 |  7432 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|         - |  7433 | `				"syntax error, unexpected end of file in return type declaration");` |
|         - |  7434 | `		}` |
|         8 |  7435 | `		return SXERR_SYNTAX;` |
|         - |  7436 | `	}` |
|     12471 |  7437 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     12471 |  7438 | `	return SXRET_OK;` |
|   1384160 |  7439 | `}` |
|         - |  7440 |  |
|    309108 |  7441 | `static sxi32 GenStateCompileFunc(` |
|         - |  7442 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  7443 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|         - |  7444 | `	sxi32 iFlags,        /* Control flags */` |
|         - |  7445 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|         - |  7446 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|         - |  7447 | `	)` |
|         5 |  7448 | `{` |
|         - |  7449 | `	ph7_vm_func *pFunc;` |
|         - |  7450 | `	SyToken *pEnd;` |
|         - |  7451 | `	sxu32 nLine;` |
|         - |  7452 | `	char *zName;` |
|         - |  7453 | `	sxi32 rc;` |
|         - |  7454 | `	/* Extract line number */` |
|    309113 |  7455 | `	nLine = pGen->pIn->nLine;` |
|         - |  7456 | `	/* Jump the left parenthesis '(' */` |
|    309113 |  7457 | `	pGen->pIn++;` |
|         - |  7458 | `	/* Delimit the function signature */` |
|    309113 |  7459 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    309113 |  7460 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  7461 | `		/* Syntax error */` |
|         8 |  7462 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|         8 |  7463 | `		if( rc == SXERR_ABORT ){` |
|         - |  7464 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  7465 | `			return SXERR_ABORT;` |
|         - |  7466 | `		}` |
|         8 |  7467 | `		pGen->pIn = pGen->pEnd;` |
|         8 |  7468 | `		return SXRET_OK;` |
|         - |  7469 | `	}` |
|         - |  7470 | `	/* Create the function state */` |
|    309107 |  7471 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|    309107 |  7472 | `	if( pFunc == 0 ){` |
|       ! 0 |  7473 | `		goto OutOfMem;` |
|         - |  7474 | `	}` |
|         - |  7475 | `	/* Build the function name, prepending namespace if active */` |
|    309114 |  7476 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|         - |  7477 | `		SyBlob sFQN;` |
|         - |  7478 | `		sxu32 nLen;` |
|        16 |  7479 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        16 |  7480 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        16 |  7481 | `		SyBlobAppend(&sFQN,"\\",1);` |
|        16 |  7482 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|        16 |  7483 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|        16 |  7484 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|        16 |  7485 | `		SyBlobRelease(&sFQN);` |
|        16 |  7486 | `		if( zName == 0 ){` |
|       ! 0 |  7487 | `			goto OutOfMem;` |
|         - |  7488 | `		}` |
|        16 |  7489 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|         9 |  7490 | `	}else{` |
|    309093 |  7491 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    309093 |  7492 | `		if( zName == 0 ){` |
|       ! 0 |  7493 | `			goto OutOfMem;` |
|         - |  7494 | `		}` |
|    309093 |  7495 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|         - |  7496 | `	}` |
|         - |  7497 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|         - |  7498 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|    309107 |  7499 | `	pFunc->nLine = nLine;` |
|    309107 |  7500 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|    309107 |  7501 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  7502 | `		return SXERR_ABORT;` |
|         - |  7503 | `	}` |
|    309107 |  7504 | `	if( pGen->pIn < pEnd ){` |
|         - |  7505 | `		/* Collect function arguments */` |
|    249197 |  7506 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|    249197 |  7507 | `		if( rc == SXERR_ABORT ){` |
|         - |  7508 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  7509 | `			return SXERR_ABORT;` |
|         - |  7510 | `		}` |
|    124596 |  7511 | `	}` |
|         - |  7512 | `	/* Point past ')' and parse optional return type ': type' */` |
|    309107 |  7513 | `	pGen->pIn = &pEnd[1];` |
|         - |  7514 | `	{` |
|    309107 |  7515 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|    309107 |  7516 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 |  7517 | `			return SXERR_ABORT;` |
|    309107 |  7518 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|         8 |  7519 | `			return SXERR_SYNTAX;` |
|         - |  7520 | `		}` |
|         - |  7521 | `	}` |
|    309101 |  7522 | `	if( bHandleClosure ){` |
|         - |  7523 | `		ph7_vm_func_closure_env sEnv;` |
|       471 |  7524 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|       466 |  7525 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       281 |  7526 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|        91 |  7527 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|         - |  7528 | `				/* Closure,record environment variable */` |
|        91 |  7529 | `				pGen->pIn++;` |
|        91 |  7530 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 |  7531 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|       ! 0 |  7532 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  7533 | `						return SXERR_ABORT;` |
|         - |  7534 | `					}` |
|       ! 0 |  7535 | `				}` |
|        91 |  7536 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|         - |  7537 | `				/* Compile until we hit the first closing parenthesis */` |
|       187 |  7538 | `				while( pGen->pIn < pGen->pEnd ){` |
|       187 |  7539 | `					int iFlagsLocal = 0;` |
|       187 |  7540 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|        91 |  7541 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|        91 |  7542 | `						break;` |
|         - |  7543 | `					}` |
|       101 |  7544 | `					nLineLocal = pGen->pIn->nLine;` |
|       101 |  7545 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - |  7546 | `						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry` |
|         - |  7547 | `						 * to the variable's memory slot instead of copying its value. */` |
|        55 |  7548 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|        55 |  7549 | `						pGen->pIn++;` |
|        27 |  7550 | `					}` |
|        96 |  7551 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|       101 |  7552 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  7553 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|         - |  7554 | `								"Closure: Unexpected token. Expecting a variable name");` |
|       ! 0 |  7555 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 |  7556 | `								return SXERR_ABORT;` |
|         - |  7557 | `							}` |
|         - |  7558 | `							/* Find the closing parenthesis */` |
|       ! 0 |  7559 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  7560 | `								pGen->pIn++;` |
|       ! 0 |  7561 | `							}` |
|       ! 0 |  7562 | `							if(pGen->pIn < pGen->pEnd){` |
|       ! 0 |  7563 | `								pGen->pIn++;` |
|       ! 0 |  7564 | `							}` |
|       ! 0 |  7565 | `							break;` |
|         - |  7566 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|       ! 0 |  7567 | `					}else{` |
|         - |  7568 | `						SyString *pNameLocal;` |
|         - |  7569 | `						char *zDup;` |
|         - |  7570 | `						/* Duplicate variable name */` |
|       101 |  7571 | `						pNameLocal = &pGen->pIn[1].sData;` |
|       101 |  7572 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|       101 |  7573 | `						if( zDup ){` |
|         - |  7574 | `							/* Zero the structure */` |
|       101 |  7575 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       101 |  7576 | `							sEnv.iFlags = iFlagsLocal;` |
|       101 |  7577 | `							sEnv.nIdx = SXU32_HIGH;` |
|       101 |  7578 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       101 |  7579 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|       116 |  7580 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|        30 |  7581 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|       ! 0 |  7582 | `									got_this = 1;` |
|       ! 0 |  7583 | `							}` |
|         - |  7584 | `							/* Save imported variable */` |
|       101 |  7585 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|        53 |  7586 | `						}else{` |
|       ! 0 |  7587 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  7588 | `							 return SXERR_ABORT;` |
|         - |  7589 | `						}` |
|         - |  7590 | `					}` |
|       101 |  7591 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|       113 |  7592 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  7593 | `						/* Ignore trailing commas */` |
|        13 |  7594 | `						pGen->pIn++;` |
|         1 |  7595 | `					}` |
|         5 |  7596 | `				}` |
|         - |  7597 | `				/* php 7.1+: the return type follows the use clause —` |
|         - |  7598 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|         - |  7599 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|         - |  7600 | `				 * so an unconditional call would wipe a type parsed at the` |
|         - |  7601 | `				 * legacy pre-use position. */` |
|        91 |  7602 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|         7 |  7603 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|         7 |  7604 | `					if( rcRt2 == SXERR_ABORT ){` |
|       ! 0 |  7605 | `						return SXERR_ABORT;` |
|         7 |  7606 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|       ! 0 |  7607 | `						return SXERR_SYNTAX;` |
|         - |  7608 | `					}` |
|         3 |  7609 | `				}` |
|        43 |  7610 | `		}` |
|       471 |  7611 | `		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){` |
|         - |  7612 | `			/* Make the $this variable [Current processed Object (class instance)]` |
|         - |  7613 | `			 * available to the closure environment — for EVERY non-static` |
|         - |  7614 | `			 * anonymous function, use list or not (php binds $this to any` |
|         - |  7615 | ``			 * closure declared in a method; pre-fix only `use (...)` closures`` |
|         - |  7616 | `			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of` |
|         - |  7617 | `			 * a global-scope closure is silently dropped at install. A static` |
|         - |  7618 | `			 * closure never binds $this (php). */` |
|       463 |  7619 | `			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       463 |  7620 | `			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|       463 |  7621 | `			sEnv.nIdx = SXU32_HIGH;` |
|       463 |  7622 | `			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       463 |  7623 | `			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|       463 |  7624 | `			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       229 |  7625 | `		}` |
|       471 |  7626 | `		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|         - |  7627 | `			/* Mark as closure */` |
|       465 |  7628 | `			pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       230 |  7629 | `		}` |
|       233 |  7630 | `	}` |
|         - |  7631 | `	/* Compile the body */` |
|    309101 |  7632 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|    309101 |  7633 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  7634 | `		return SXERR_ABORT;` |
|         - |  7635 | `	}` |
|         - |  7636 | `	/* The cursor sits just past the body's closing brace */` |
|    309101 |  7637 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|    309101 |  7638 | `	if( ppFunc ){` |
|    309101 |  7639 | `		*ppFunc = pFunc;` |
|    154548 |  7640 | `	}` |
|    309101 |  7641 | `	rc = SXRET_OK;` |
|    309101 |  7642 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|         - |  7643 | `		/* Finally register the function */` |
|    308641 |  7644 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|    154318 |  7645 | `	}` |
|    309101 |  7646 | `	if( rc == SXRET_OK ){` |
|    309101 |  7647 | `		return SXRET_OK;` |
|         - |  7648 | `	}` |
|         - |  7649 | `	/* Fall through if something goes wrong */` |
|       ! 0 |  7650 | `OutOfMem:` |
|         - |  7651 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  7652 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  7653 | `	 */` |
|       ! 0 |  7654 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       ! 0 |  7655 | `	return SXERR_ABORT;` |
|    154559 |  7656 | `}` |
|         - |  7657 | `/*` |
|         - |  7658 | ` * Compile a standard PHP function.` |
|         - |  7659 | ` *  Refer to the block-comment above for more information.` |
|         - |  7660 | ` */` |
|    308650 |  7661 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|         5 |  7662 | `{` |
|         - |  7663 | `	SyString *pName;` |
|         - |  7664 | `	sxi32 iFlags;` |
|         - |  7665 | `	sxu32 nKwLine;` |
|         - |  7666 | `	sxu32 nLine;` |
|         - |  7667 | `	sxi32 rc;` |
|         - |  7668 |  |
|    308655 |  7669 | `	nLine = pGen->pIn->nLine;` |
|    308655 |  7670 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|    308655 |  7671 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|    308655 |  7672 | `	iFlags = 0;` |
|    308655 |  7673 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - |  7674 | `		/* Return by reference,remember that */` |
|        12 |  7675 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|         - |  7676 | `		/* Jump the '&' token */` |
|        12 |  7677 | `		pGen->pIn++;` |
|         5 |  7678 | `	}` |
|    308655 |  7679 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  7680 | `		/* Invalid function name */` |
|         8 |  7681 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|         8 |  7682 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  7683 | `			return SXERR_ABORT;` |
|         - |  7684 | `		}` |
|         - |  7685 | `		/* Sychronize with the next semi-colon or braces*/` |
|        22 |  7686 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        16 |  7687 | `			pGen->pIn++;` |
|         2 |  7688 | `		}` |
|         8 |  7689 | `		return SXRET_OK;` |
|         - |  7690 | `	}` |
|    308649 |  7691 | `	pName = &pGen->pIn->sData;` |
|    308649 |  7692 | `	nLine = pGen->pIn->nLine;` |
|         - |  7693 | `	/* Jump the function name */` |
|    308649 |  7694 | `	pGen->pIn++;` |
|    308649 |  7695 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  7696 | `		/* Syntax error */` |
|         3 |  7697 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|         3 |  7698 | `		if( rc == SXERR_ABORT ){` |
|         - |  7699 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  7700 | `			return SXERR_ABORT;` |
|         - |  7701 | `		}` |
|         - |  7702 | `		/* Sychronize with the next semi-colon or '{' */` |
|         3 |  7703 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       ! 0 |  7704 | `			pGen->pIn++;` |
|       ! 0 |  7705 | `		}` |
|         3 |  7706 | `		return SXRET_OK;` |
|         - |  7707 | `	}` |
|         - |  7708 | `	/* Compile function body */` |
|         - |  7709 | `	{` |
|    308647 |  7710 | `		ph7_vm_func *pFuncState = 0;` |
|    308647 |  7711 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);` |
|    308647 |  7712 | `		if( pFuncState ){` |
|         - |  7713 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|    308635 |  7714 | `			pFuncState->nLine = nKwLine;` |
|    154315 |  7715 | `		}` |
|         - |  7716 | `	}` |
|    308647 |  7717 | `	return rc;` |
|    154330 |  7718 | `}` |
|         - |  7719 | `/*` |
|         - |  7720 | ` * Extract the visibility level associated with a given keyword.` |
|         - |  7721 | ` * According to the PHP language reference manual` |
|         - |  7722 | ` *  Visibility:` |
|         - |  7723 | ` *  The visibility of a property or method can be defined by prefixing` |
|         - |  7724 | ` *  the declaration with the keywords public, protected or private.` |
|         - |  7725 | ` *  Class members declared public can be accessed everywhere.` |
|         - |  7726 | ` *  Members declared protected can be accessed only within the class` |
|         - |  7727 | ` *  itself and by inherited and parent classes. Members declared as private` |
|         - |  7728 | ` *  may only be accessed by the class that defines the member.` |
|         - |  7729 | ` */` |
|   3227986 |  7730 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|         5 |  7731 | `{` |
|   3227991 |  7732 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    260101 |  7733 | `		return PH7_CLASS_PROT_PRIVATE;` |
|   2967895 |  7734 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|    196987 |  7735 | `		return PH7_CLASS_PROT_PROTECTED;` |
|         - |  7736 | `	}` |
|         - |  7737 | `	/* Assume public by default */` |
|   2770913 |  7738 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   1613998 |  7739 | `}` |
|         - |  7740 | `/*` |
|         - |  7741 | ` * Compile a class constant.` |
|         - |  7742 | ` * According to the PHP language reference manual` |
|         - |  7743 | ` *  Class Constants` |
|         - |  7744 | ` *   It is possible to define constant values on a per-class basis remaining` |
|         - |  7745 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|         - |  7746 | ` *   you don't use the $ symbol to declare or use them.` |
|         - |  7747 | ` *   The value must be a constant expression, not (for example) a variable,` |
|         - |  7748 | ` *   a property, a result of a mathematical operation, or a function call.` |
|         - |  7749 | ` *   It's also possible for interfaces to have constants.` |
|         - |  7750 | ` * Symisc eXtension.` |
|         - |  7751 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|         - |  7752 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  7753 | ` *  Example:` |
|         - |  7754 | ` *   class Test{` |
|         - |  7755 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|         - |  7756 | ` *   };` |
|         - |  7757 | ` *   var_dump(TEST::MyConst);` |
|         - |  7758 | ` *   Refer to the official documentation for more information on the powerful extension` |
|         - |  7759 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|         - |  7760 | ` */` |
|         - |  7761 | `/*` |
|         - |  7762 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|         - |  7763 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|         - |  7764 | ` * token immediately followed by '='. Anything else with a leading type token` |
|         - |  7765 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|         - |  7766 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|         - |  7767 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|         - |  7768 | ` */` |
|    299464 |  7769 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|         5 |  7770 | `{` |
|         - |  7771 | `	SyToken *p0, *p1;` |
|    299469 |  7772 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  7773 | `		return 0;` |
|         - |  7774 | `	}` |
|    299469 |  7775 | `	p0 = pGen->pIn;` |
|         - |  7776 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|    299469 |  7777 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|       ! 0 |  7778 | `		return 1;` |
|         - |  7779 | `	}` |
|    299469 |  7780 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|         5 |  7781 | `		return 1;` |
|         - |  7782 | `	}` |
|         - |  7783 | `	/* A name-like first token begins a type only when followed by another` |
|         - |  7784 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|         - |  7785 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|    299465 |  7786 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|    299465 |  7787 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|    299465 |  7788 | `		if( p1 ){` |
|    299465 |  7789 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|        34 |  7790 | `				return 1;` |
|         - |  7791 | `			}` |
|    299435 |  7792 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|         5 |  7793 | `				return 1;` |
|         - |  7794 | `			}` |
|    149713 |  7795 | `		}` |
|    149713 |  7796 | `	}` |
|    299431 |  7797 | `	return 0;` |
|    149737 |  7798 | `}` |
|         - |  7799 | `/*` |
|         - |  7800 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|         - |  7801 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|         - |  7802 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|         - |  7803 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|         - |  7804 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|         - |  7805 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|         - |  7806 | ` * Peek only; never consumes tokens.` |
|         - |  7807 | ` */` |
|        24 |  7808 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|         4 |  7809 | `{` |
|        28 |  7810 | `	SyToken *p = pGen->pIn;` |
|        39 |  7811 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        20 |  7812 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|         3 |  7813 | `		p++; /* skip leading unary sign(s) */` |
|         1 |  7814 | `	}` |
|        28 |  7815 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|        23 |  7816 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|         - |  7817 | `	}` |
|         6 |  7818 | `	p++;` |
|         - |  7819 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|         6 |  7820 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|        16 |  7821 | `}` |
|         - |  7822 | `/*` |
|         - |  7823 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|         - |  7824 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|         - |  7825 | `` * `$o->new`), not a `new` expression.`` |
|         - |  7826 | ` */` |
|       110 |  7827 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|         4 |  7828 | `{` |
|         - |  7829 | `	sxi32 iOp;` |
|       114 |  7830 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|        11 |  7831 | `		return 0;` |
|         - |  7832 | `	}` |
|       104 |  7833 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|       104 |  7834 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|        59 |  7835 | `}` |
|         - |  7836 | `/*` |
|         - |  7837 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|         - |  7838 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|         - |  7839 | ` * interface-constant and (instance/static) property-default initializers` |
|         - |  7840 | ` * ("New expressions are not supported in this context") while still allowing it` |
|         - |  7841 | ` * in global constants, parameter defaults and static-local initializers (which` |
|         - |  7842 | ` * are compiled by different functions and left untouched). The scan is` |
|         - |  7843 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|         - |  7844 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|         - |  7845 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|         - |  7846 | ` *` |
|         - |  7847 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|         - |  7848 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|         - |  7849 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|         - |  7850 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|         - |  7851 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|         - |  7852 | ` */` |
|    642752 |  7853 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|         5 |  7854 | `{` |
|    642757 |  7855 | `	SyToken *p = pGen->pIn;` |
|    642757 |  7856 | `	int iDepth = 0;` |
|   1684341 |  7857 | `	while( p < pGen->pEnd ){` |
|   1684341 |  7858 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|    642705 |  7859 | `			break; /* end of this initializer */` |
|         - |  7860 | `		}` |
|   1041636 |  7861 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|    524777 |  7862 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      7908 |  7863 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|         - |  7864 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|         - |  7865 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|         - |  7866 | `			 * expression. */` |
|         3 |  7867 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|         3 |  7868 | `			p++;` |
|         3 |  7869 | `			if( bArrow ){` |
|         - |  7870 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|         - |  7871 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|         3 |  7872 | `				int iBase = iDepth;` |
|        17 |  7873 | `				while( p < pGen->pEnd ){` |
|        17 |  7874 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         5 |  7875 | `						iDepth++;` |
|        15 |  7876 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         5 |  7877 | `						if( iDepth <= iBase ){` |
|       ! 0 |  7878 | `							break; /* closes an enclosing group, not the fn's own */` |
|         - |  7879 | `						}` |
|         5 |  7880 | `						iDepth--;` |
|        11 |  7881 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|         3 |  7882 | `						break;` |
|         - |  7883 | `					}` |
|        15 |  7884 | `					p++;` |
|         1 |  7885 | `				}` |
|         2 |  7886 | `			}else{` |
|         - |  7887 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|         - |  7888 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|         - |  7889 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|         - |  7890 | `				 * then skip the balanced brace block. */` |
|       ! 0 |  7891 | `				int iLocal = 0;` |
|       ! 0 |  7892 | `				while( p < pGen->pEnd ){` |
|       ! 0 |  7893 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|       ! 0 |  7894 | `						break; /* body brace */` |
|         - |  7895 | `					}` |
|       ! 0 |  7896 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       ! 0 |  7897 | `						iLocal++;` |
|       ! 0 |  7898 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       ! 0 |  7899 | `						if( iLocal > 0 ){` |
|       ! 0 |  7900 | `							iLocal--;` |
|       ! 0 |  7901 | `						}` |
|       ! 0 |  7902 | `					}` |
|       ! 0 |  7903 | `					p++;` |
|       ! 0 |  7904 | `				}` |
|       ! 0 |  7905 | `				if( p < pGen->pEnd ){` |
|       ! 0 |  7906 | `					int iBrace = 0; /* p is on the body '{' */` |
|       ! 0 |  7907 | `					while( p < pGen->pEnd ){` |
|       ! 0 |  7908 | `						if( p->nType & PH7_TK_OCB ){` |
|       ! 0 |  7909 | `							iBrace++;` |
|       ! 0 |  7910 | `						}else if( p->nType & PH7_TK_CCB ){` |
|       ! 0 |  7911 | `							iBrace--;` |
|       ! 0 |  7912 | `							if( iBrace == 0 ){` |
|       ! 0 |  7913 | `								p++;` |
|       ! 0 |  7914 | `								break;` |
|         - |  7915 | `							}` |
|       ! 0 |  7916 | `						}` |
|       ! 0 |  7917 | `						p++;` |
|       ! 0 |  7918 | `					}` |
|       ! 0 |  7919 | `				}` |
|         - |  7920 | `			}` |
|         3 |  7921 | `			continue;` |
|         - |  7922 | `		}` |
|   1041639 |  7923 | `		if( p->nType & PH7_TK_OCB ){` |
|        45 |  7924 | `			if( iDepth == 0 ){` |
|         - |  7925 | `				/* A depth-0 '{' can only open a PHP 8.4 property-hook list` |
|         - |  7926 | ``				 * (`public T $x = default { get …; }`): the default expression`` |
|         - |  7927 | ``				 * ends here. A `new` inside a hook BODY runs at access time and`` |
|         - |  7928 | `				 * is legal — don't scan into it. */` |
|        45 |  7929 | `				break;` |
|         - |  7930 | `			}` |
|       ! 0 |  7931 | `			iDepth++;` |
|   1041595 |  7932 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     43407 |  7933 | `			iDepth++;` |
|   1019894 |  7934 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     43405 |  7935 | `			if( iDepth > 0 ){` |
|     43405 |  7936 | `				iDepth--;` |
|     21700 |  7937 | `			}` |
|    976493 |  7938 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|    347333 |  7939 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|         - |  7940 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|         - |  7941 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|         - |  7942 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|        11 |  7943 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|        11 |  7944 | `				return 1;` |
|         - |  7945 | `			}` |
|       ! 0 |  7946 | `		}` |
|   1041587 |  7947 | `		p++;` |
|         5 |  7948 | `	}` |
|    642749 |  7949 | `	return 0;` |
|    321381 |  7950 | `}` |
|         - |  7951 | `/*` |
|         - |  7952 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|         - |  7953 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|         - |  7954 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|         - |  7955 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|         - |  7956 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|         - |  7957 | ` * share the same backing.` |
|         - |  7958 | ` */` |
|       350 |  7959 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|         - |  7960 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|         5 |  7961 | `{` |
|       355 |  7962 | `	pAttr->nType = nType;` |
|       355 |  7963 | `	pAttr->sClass = *pClass;` |
|       355 |  7964 | `	pAttr->sTypeName = *pTypeName;` |
|       355 |  7965 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  7966 | `		sxu32 i;` |
|        73 |  7967 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        51 |  7968 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|        51 |  7969 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|        28 |  7970 | `		}` |
|        11 |  7971 | `	}` |
|       355 |  7972 | `}` |
|    299464 |  7973 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  7974 | `{` |
|    299469 |  7975 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  7976 | `	SySet *pInstrContainer;` |
|         - |  7977 | `	ph7_class_attr *pCons;` |
|         - |  7978 | `	SyString *pName;` |
|         - |  7979 | `	sxi32 rc;` |
|    299469 |  7980 | `	sxu32 nType = 0;` |
|         - |  7981 | `	SyString sTypeClass;` |
|         - |  7982 | `	SyString sTypeText;` |
|         - |  7983 | `	SySet aUnionAlts;` |
|    299469 |  7984 | `	sxi32 iTypeFlags = 0;` |
|    299469 |  7985 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    299469 |  7986 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    299469 |  7987 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  7988 | `	/* Extract visibility level */` |
|    299469 |  7989 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  7990 | `	/* Mark as constant */` |
|    299469 |  7991 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|    299469 |  7992 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|         - |  7993 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|         - |  7994 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|    299488 |  7995 | `	if( GenStateClassConstHasType(pGen) ){` |
|        61 |  7996 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|        38 |  7997 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|         - |  7998 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|         - |  7999 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|         - |  8000 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|         - |  8001 | `		 * and success paths release. */` |
|        42 |  8002 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  8003 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  8004 | `			goto Synchronize;` |
|        42 |  8005 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  8006 | `			return SXERR_ABORT;` |
|        42 |  8007 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  8008 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  8009 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|       ! 0 |  8010 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8011 | `				return SXERR_ABORT;` |
|         - |  8012 | `			}` |
|       ! 0 |  8013 | `			goto Synchronize;` |
|         - |  8014 | `		}` |
|        42 |  8015 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        19 |  8016 | `	}` |
|    149732 |  8017 | `loop:` |
|    299471 |  8018 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - |  8019 | `		/* Invalid constant name */` |
|       ! 0 |  8020 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|       ! 0 |  8021 | `		if( rc == SXERR_ABORT ){` |
|         - |  8022 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8023 | `			return SXERR_ABORT;` |
|         - |  8024 | `		}` |
|       ! 0 |  8025 | `		goto Synchronize;` |
|         - |  8026 | `	}` |
|         - |  8027 | `	/* Peek constant name */` |
|    299471 |  8028 | `	pName = &pGen->pIn->sData;` |
|         - |  8029 | `	/* Make sure the constant name isn't reserved */` |
|    299471 |  8030 | `	if( GenStateIsReservedConstant(pName) ){` |
|         - |  8031 | `		/* Reserved constant name */` |
|       ! 0 |  8032 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|       ! 0 |  8033 | `		if( rc == SXERR_ABORT ){` |
|         - |  8034 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8035 | `			return SXERR_ABORT;` |
|         - |  8036 | `		}` |
|       ! 0 |  8037 | `		goto Synchronize;` |
|         - |  8038 | `	}` |
|         - |  8039 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|    299471 |  8040 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        61 |  8041 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|        38 |  8042 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|        19 |  8043 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|        42 |  8044 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8045 | `			return SXERR_ABORT;` |
|        42 |  8046 | `		}else if( rc != SXRET_OK ){` |
|         3 |  8047 | `			goto Synchronize;` |
|         - |  8048 | `		}` |
|        18 |  8049 | `	}` |
|         - |  8050 | `	/* Advance the stream cursor */` |
|    299469 |  8051 | `	pGen->pIn++;` |
|    299469 |  8052 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|         - |  8053 | `		/* Invalid declaration */` |
|       ! 0 |  8054 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|       ! 0 |  8055 | `		if( rc == SXERR_ABORT ){` |
|         - |  8056 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8057 | `			return SXERR_ABORT;` |
|         - |  8058 | `		}` |
|       ! 0 |  8059 | `		goto Synchronize;` |
|         - |  8060 | `	}` |
|    299469 |  8061 | `	pGen->pIn++; /* Jump the equal sign */` |
|         - |  8062 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|         - |  8063 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|         - |  8064 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|         - |  8065 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|    299464 |  8066 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|        39 |  8067 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|         8 |  8068 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8069 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|         2 |  8070 | `			&pClass->sName,pName,&sTypeText);` |
|         6 |  8071 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8072 | `			return SXERR_ABORT;` |
|         - |  8073 | `		}` |
|         6 |  8074 | `		goto Synchronize;` |
|         - |  8075 | `	}` |
|         - |  8076 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|         - |  8077 | `	 * constant initializer ("New expressions are not supported in this context").` |
|         - |  8078 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|    299465 |  8079 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|         5 |  8080 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8081 | `			"New expressions are not supported in this context");` |
|         5 |  8082 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8083 | `			return SXERR_ABORT;` |
|         - |  8084 | `		}` |
|         5 |  8085 | `		goto Synchronize;` |
|         - |  8086 | `	}` |
|         - |  8087 | `	/* Allocate a new class attribute */` |
|    299461 |  8088 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    299461 |  8089 | `	if( pCons ){` |
|    299461 |  8090 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|    299461 |  8091 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8092 | `			return SXERR_ABORT;` |
|         - |  8093 | `		}` |
|    149728 |  8094 | `	}` |
|    299461 |  8095 | `	if( pCons == 0 ){` |
|       ! 0 |  8096 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8097 | `		return SXERR_ABORT;` |
|         - |  8098 | `	}` |
|    299461 |  8099 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|        35 |  8100 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|        16 |  8101 | `	}` |
|         - |  8102 | `	/* Swap bytecode container */` |
|    299461 |  8103 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    299461 |  8104 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|         - |  8105 | `	/* Compile constant value.` |
|         - |  8106 | `	 */` |
|    299461 |  8107 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    299461 |  8108 | `	if( rc == SXERR_EMPTY ){` |
|         3 |  8109 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|         3 |  8110 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8111 | `			return SXERR_ABORT;` |
|         - |  8112 | `		}` |
|         1 |  8113 | `	}` |
|         - |  8114 | `	/* Emit the done instruction */` |
|    299461 |  8115 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    299461 |  8116 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    299461 |  8117 | `	if( rc == SXERR_ABORT ){` |
|         - |  8118 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  8119 | `		return SXERR_ABORT;` |
|         - |  8120 | `	}` |
|         - |  8121 | `	/* All done,install the constant */` |
|    299461 |  8122 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|    299461 |  8123 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8124 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8125 | `		return SXERR_ABORT;` |
|         - |  8126 | `	}` |
|    299461 |  8127 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  8128 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|         3 |  8129 | `		pGen->pIn++; /* Jump the comma */` |
|         3 |  8130 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 |  8131 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 |  8132 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 |  8133 | `				pTok--;` |
|       ! 0 |  8134 | `			}` |
|       ! 0 |  8135 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  8136 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|       ! 0 |  8137 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 |  8138 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8139 | `				return SXERR_ABORT;` |
|         - |  8140 | `			}` |
|       ! 0 |  8141 | `		}else{` |
|         3 |  8142 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|         3 |  8143 | `				goto loop;` |
|         - |  8144 | `			}` |
|         - |  8145 | `		}` |
|       ! 0 |  8146 | `	}` |
|    299459 |  8147 | `	SySetRelease(&aUnionAlts);` |
|    299459 |  8148 | `	return SXRET_OK;` |
|         5 |  8149 | `Synchronize:` |
|        13 |  8150 | `	SySetRelease(&aUnionAlts);` |
|         - |  8151 | `	/* Synchronize with the first semi-colon */` |
|        45 |  8152 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        35 |  8153 | `		pGen->pIn++;` |
|         3 |  8154 | `	}` |
|        13 |  8155 | `	return SXERR_CORRUPT;` |
|    149737 |  8156 | `}` |
|         - |  8157 | `/*` |
|         - |  8158 | ` * complie a class attribute or Properties in the PHP jargon.` |
|         - |  8159 | ` * According to the PHP language reference manual` |
|         - |  8160 | ` *  Properties` |
|         - |  8161 | ` *  Class member variables are called "properties". You may also see them referred` |
|         - |  8162 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|         - |  8163 | ` *  of this reference we will use "properties". They are defined by using one` |
|         - |  8164 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|         - |  8165 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|         - |  8166 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|         - |  8167 | ` *  and must not depend on run-time information in order to be evaluated.` |
|         - |  8168 | ` * Symisc eXtension.` |
|         - |  8169 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|         - |  8170 | ` *  the zend engine would allow only simple scalar value.` |
|         - |  8171 | ` *  Example:` |
|         - |  8172 | ` *   class Test{` |
|         - |  8173 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|         - |  8174 | ` *   };` |
|         - |  8175 | ` *   var_dump(TEST::myVar);` |
|         - |  8176 | ` *   Refer to the official documentation for more information on the powerful extension` |
|         - |  8177 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|         - |  8178 | ` */` |
|         - |  8179 | `/*` |
|         - |  8180 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|         - |  8181 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|         - |  8182 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|         - |  8183 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|         - |  8184 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|         - |  8185 | ` */` |
|   2411700 |  8186 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|         5 |  8187 | `{` |
|   2411705 |  8188 | `	SyToken *p = pStart;` |
|   2411705 |  8189 | `	int bFirst = 1;` |
|   2411705 |  8190 | `	if( p >= pEnd ) return 0;` |
|         - |  8191 | ``	/* Optional nullable `?` shorthand. */`` |
|   2411705 |  8192 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|        35 |  8193 | `		p++;` |
|        35 |  8194 | `		if( p >= pEnd ) return 0;` |
|        16 |  8195 | `	}` |
|         - |  8196 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|         - |  8197 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|         - |  8198 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|         - |  8199 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   1205850 |  8200 | `	for(;;){` |
|   2411725 |  8201 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|         - |  8202 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|         3 |  8203 | `			p++;` |
|         9 |  8204 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|         3 |  8205 | `			if( p >= pEnd ) return 0;` |
|         3 |  8206 | `			p++; /* skip ')' */` |
|         2 |  8207 | `		}else{` |
|         - |  8208 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|         - |  8209 | ``			 * then any `&`-joined intersection members. */`` |
|   2411723 |  8210 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|   2411723 |  8211 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  8212 | `				return 0;` |
|         - |  8213 | `			}` |
|         - |  8214 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|         - |  8215 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|         - |  8216 | `			 * may still appear at the initial dispatch site). */` |
|   2411723 |  8217 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|   2411675 |  8218 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|   2411670 |  8219 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    106736 |  8220 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|   2411393 |  8221 | `					return 0;` |
|         - |  8222 | `				}` |
|       141 |  8223 | `			}` |
|       335 |  8224 | `			p++;` |
|       337 |  8225 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  8226 | `				p += 2;` |
|         1 |  8227 | `			}` |
|       498 |  8228 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|       338 |  8229 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         3 |  8230 | `				p++; /* skip '&' */` |
|         3 |  8231 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|         3 |  8232 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|         3 |  8233 | `				p++;` |
|         3 |  8234 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       ! 0 |  8235 | `					p += 2;` |
|       ! 0 |  8236 | `				}` |
|         1 |  8237 | `			}` |
|         - |  8238 | `		}` |
|       337 |  8239 | `		bFirst = 0;` |
|       332 |  8240 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|        25 |  8241 | `			&& p->sData.zString[0] == '\|' ){` |
|        25 |  8242 | ``			p++; /* next `\|`-separated part */`` |
|        25 |  8243 | `			continue;` |
|         - |  8244 | `		}` |
|       317 |  8245 | `		break;` |
|       ! 0 |  8246 | `	}` |
|       317 |  8247 | `	if( p >= pEnd ) return 0;` |
|       317 |  8248 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   1205855 |  8249 | `}` |
|         - |  8250 |  |
|         - |  8251 | `/*` |
|         - |  8252 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|         - |  8253 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|         - |  8254 | ` * if not). Recognized forms:` |
|         - |  8255 | ` *   ?Type, array, bool, int, float, string, object,` |
|         - |  8256 | ` *   self, parent, \Ns\ClassName, ClassName` |
|         - |  8257 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|         - |  8258 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|         - |  8259 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|         - |  8260 | ` * on unrecoverable error.` |
|         - |  8261 | ` *` |
|         - |  8262 | ` * When a type is parsed:` |
|         - |  8263 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|         - |  8264 | ` *   *pClass is set to the class name (for class types)` |
|         - |  8265 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|         - |  8266 | ` *   *pTypeText is set to the original text span of the type` |
|         - |  8267 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|         - |  8268 | ` */` |
|       322 |  8269 | `static sxi32 GenStateParsePropertyType(` |
|         - |  8270 | `	ph7_gen_state *pGen,` |
|         - |  8271 | `	sxu32 *pnType,` |
|         - |  8272 | `	SyString *pClass,` |
|         - |  8273 | `	sxi32 *piTypeFlags,` |
|         - |  8274 | `	SyString *pTypeText,` |
|         - |  8275 | `	SySet *pAlts` |
|         5 |  8276 | `){` |
|       327 |  8277 | `	sxi32 iFlags = 0;` |
|         - |  8278 | `	sxi32 rc;` |
|       327 |  8279 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 |  8280 | `		return SXRET_OK;` |
|         - |  8281 | `	}` |
|         - |  8282 | `	/* If the first token is '$', there's no type */` |
|       327 |  8283 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       ! 0 |  8284 | `		return SXRET_OK;` |
|         - |  8285 | `	}` |
|       327 |  8286 | `	rc = GenStateParseUnionTypeDecl(` |
|       161 |  8287 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|         - |  8288 | `		PH7_CLASS_ATTR_NULLABLE,` |
|         - |  8289 | `		PH7_CLASS_ATTR_UNION,` |
|         - |  8290 | `		/* bAllowVoid */ 0,` |
|       322 |  8291 | `		pGen->pIn->nLine);` |
|       327 |  8292 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8293 | `		return rc;` |
|         - |  8294 | `	}` |
|         - |  8295 | `	/* Verify next token is '$' (start of property name) */` |
|       327 |  8296 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  8297 | `		return SXERR_SYNTAX;` |
|         - |  8298 | `	}` |
|       327 |  8299 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|       327 |  8300 | `	return SXRET_OK;` |
|       166 |  8301 | `}` |
|         - |  8302 |  |
|         - |  8303 | `/*` |
|         - |  8304 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|         - |  8305 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|         - |  8306 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|         - |  8307 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|         - |  8308 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|         - |  8309 | ` * by the type parser itself before reaching here.` |
|         - |  8310 | ` *` |
|         - |  8311 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|         - |  8312 | ` * use in the error message.` |
|         - |  8313 | ` */` |
|       498 |  8314 | `static int GenStateIsDisallowedPropertyAtom(` |
|         - |  8315 | `	sxu32 nType,` |
|         - |  8316 | `	const SyString *pClass,` |
|         - |  8317 | `	const char **pzName,` |
|         - |  8318 | `	sxu32 *pnName)` |
|         5 |  8319 | `{` |
|         - |  8320 | `	const char *z;` |
|         - |  8321 | `	sxu32 n;` |
|       503 |  8322 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|       449 |  8323 | `		return 0;` |
|         - |  8324 | `	}` |
|        59 |  8325 | `	z = pClass->zString;` |
|        59 |  8326 | `	n = pClass->nByte;` |
|        59 |  8327 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|         8 |  8328 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|         - |  8329 | `	}` |
|         - |  8330 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|         - |  8331 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|         - |  8332 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|        52 |  8333 | `	return 0;` |
|       254 |  8334 | `}` |
|         - |  8335 |  |
|         - |  8336 | `/*` |
|         - |  8337 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|         - |  8338 | ` * constant) — the main atom plus any union alternatives — against the` |
|         - |  8339 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|         - |  8340 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|         - |  8341 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|         - |  8342 | ` * type T" vs "Class constant C::X cannot have type T").` |
|         - |  8343 | ` *` |
|         - |  8344 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|         - |  8345 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|         - |  8346 | ` */` |
|       436 |  8347 | `static sxi32 GenStateValidateMemberType(` |
|         - |  8348 | `	ph7_gen_state *pGen,` |
|         - |  8349 | `	ph7_class *pClass,` |
|         - |  8350 | `	const SyString *pMemberName,` |
|         - |  8351 | `	sxu32 nType,` |
|         - |  8352 | `	const SyString *pTypeClass,` |
|         - |  8353 | `	const SyString *pTypeText,` |
|         - |  8354 | `	SySet *pUnionAlts,` |
|         - |  8355 | `	const char *zErrFmt,` |
|         - |  8356 | `	sxu32 nLine)` |
|         5 |  8357 | `{` |
|       441 |  8358 | `	const char *zBad = 0;` |
|       441 |  8359 | `	sxu32 nBad = 0;` |
|         - |  8360 | `	SyString sFallback;` |
|         - |  8361 | `	const SyString *pBad;` |
|         - |  8362 | `	sxi32 rc;` |
|       441 |  8363 | `	int bDisallowed = 0;` |
|       441 |  8364 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|         5 |  8365 | `		bDisallowed = 1;` |
|       439 |  8366 | `	}else if( pUnionAlts ){` |
|         - |  8367 | `		sxu32 i;` |
|        95 |  8368 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|        67 |  8369 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|        67 |  8370 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|         3 |  8371 | `				bDisallowed = 1;` |
|         3 |  8372 | `				break;` |
|         - |  8373 | `			}` |
|        35 |  8374 | `		}` |
|        15 |  8375 | `	}` |
|       441 |  8376 | `	if( !bDisallowed ){` |
|       435 |  8377 | `		return SXRET_OK;` |
|         - |  8378 | `	}` |
|         - |  8379 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|         - |  8380 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|         - |  8381 | `	 * canonical spelling if the type text is unavailable. */` |
|         8 |  8382 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|         8 |  8383 | `		pBad = pTypeText;` |
|         5 |  8384 | `	}else{` |
|       ! 0 |  8385 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|       ! 0 |  8386 | `		pBad = &sFallback;` |
|         - |  8387 | `	}` |
|        11 |  8388 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         3 |  8389 | `		zErrFmt,` |
|         3 |  8390 | `		&pClass->sName,pMemberName,pBad);` |
|         8 |  8391 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  8392 | `		return SXERR_ABORT;` |
|         - |  8393 | `	}` |
|         8 |  8394 | `	return SXERR_SYNTAX;` |
|       223 |  8395 | `}` |
|         - |  8396 | `/*` |
|         - |  8397 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|         - |  8398 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|         - |  8399 | ` * matched as a plain identifier in the class-member modifier position rather` |
|         - |  8400 | ` * than promoted to a lexer keyword.` |
|         - |  8401 | ` */` |
|  18543824 |  8402 | `static int GenStateIsReadonly(SyToken *pTok)` |
|         5 |  8403 | `{` |
|  18745141 |  8404 | `	return (pTok->nType & PH7_TK_ID)` |
|   9473224 |  8405 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
|  18745136 |  8406 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|         5 |  8407 | `}` |
|         - |  8408 | `/*` |
|         - |  8409 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|         - |  8410 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|         - |  8411 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|         - |  8412 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|         - |  8413 | ` */` |
|   7083762 |  8414 | `static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|         5 |  8415 | `{` |
|   7083767 |  8416 | `	*pnTok = 0;` |
|   7083762 |  8417 | `	if( &pTok[3] < pEnd` |
|   6671387 |  8418 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|   5630058 |  8419 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|   2500560 |  8420 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        16 |  8421 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|        16 |  8422 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|        21 |  8423 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|        17 |  8424 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|        17 |  8425 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|        17 |  8426 | `			*pnTok = 4;` |
|        17 |  8427 | `			return nKw;` |
|         - |  8428 | `		}` |
|       ! 0 |  8429 | `	}` |
|   7083751 |  8430 | `	return 0;` |
|   3541886 |  8431 | `}` |
|         - |  8432 | `/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */` |
|        16 |  8433 | `static sxi32 GenStateSetVisFlag(sxi32 nKw)` |
|         1 |  8434 | `{` |
|        17 |  8435 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|        13 |  8436 | `		return PH7_CLASS_ATTR_PRIVATE_SET;` |
|         - |  8437 | `	}` |
|         5 |  8438 | `	if( nKw == PH7_TKWRD_PROTECTED ){` |
|         3 |  8439 | `		return PH7_CLASS_ATTR_PROTECTED_SET;` |
|         - |  8440 | `	}` |
|         3 |  8441 | `	return PH7_CLASS_ATTR_PUBLIC_SET;` |
|         9 |  8442 | `}` |
|    469600 |  8443 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|         5 |  8444 | `{` |
|    469605 |  8445 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  8446 | `	ph7_class_attr *pAttr;` |
|         - |  8447 | `	SyString *pName;` |
|         - |  8448 | `	sxi32 rc;` |
|    469605 |  8449 | `	sxu32 nType = 0;` |
|         - |  8450 | `	SyString sTypeClass;` |
|         - |  8451 | `	SyString sTypeText;` |
|         - |  8452 | `	SySet aUnionAlts;` |
|    469605 |  8453 | `	sxi32 iTypeFlags = 0;` |
|    469605 |  8454 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|    469605 |  8455 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|    469605 |  8456 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  8457 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|         - |  8458 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|         - |  8459 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|    469605 |  8460 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|        21 |  8461 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|         9 |  8462 | `	}` |
|         - |  8463 | `	/* Extract visibility level */` |
|    469605 |  8464 | `	iProtection = GetProtectionLevel(iProtection);` |
|         - |  8465 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|    469766 |  8466 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       327 |  8467 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|       327 |  8468 | `		if( rc == SXERR_CORRUPT ){` |
|         - |  8469 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|       ! 0 |  8470 | `			goto Synchronize;` |
|       327 |  8471 | `		}else if( rc == SXERR_SYNTAX ){` |
|       ! 0 |  8472 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8473 | `				"Invalid property type or declaration near '%z'",` |
|       ! 0 |  8474 | `				&pGen->pIn->sData);` |
|       ! 0 |  8475 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8476 | `				return SXERR_ABORT;` |
|         - |  8477 | `			}` |
|       ! 0 |  8478 | `			goto Synchronize;` |
|       327 |  8479 | `		}else if( rc == SXERR_ABORT ){` |
|       ! 0 |  8480 | `			return SXERR_ABORT;` |
|         - |  8481 | `		}` |
|       161 |  8482 | `	}` |
|       ! 0 |  8483 | `loop:` |
|    469609 |  8484 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 |  8485 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|       ! 0 |  8486 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8487 | `			return SXERR_ABORT;` |
|         - |  8488 | `		}` |
|       ! 0 |  8489 | `		goto Synchronize;` |
|         - |  8490 | `	}` |
|    469609 |  8491 | `	pGen->pIn++; /* Jump the dollar sign */` |
|    469609 |  8492 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|         - |  8493 | `		/* Invalid attribute name */` |
|       ! 0 |  8494 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|       ! 0 |  8495 | `		if( rc == SXERR_ABORT ){` |
|         - |  8496 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8497 | `			return SXERR_ABORT;` |
|         - |  8498 | `		}` |
|       ! 0 |  8499 | `		goto Synchronize;` |
|         - |  8500 | `	}` |
|         - |  8501 | `	/* Peek attribute name */` |
|    469609 |  8502 | `	pName = &pGen->pIn->sData;` |
|         - |  8503 | `	/* Advance the stream cursor */` |
|    469609 |  8504 | `	pGen->pIn++;` |
|    469609 |  8505 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/\|PH7_TK_OCB/*'{' hooks*/)) == 0 ){` |
|         - |  8506 | `		/* Invalid declaration */` |
|         3 |  8507 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|         3 |  8508 | `		if( rc == SXERR_ABORT ){` |
|         - |  8509 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8510 | `			return SXERR_ABORT;` |
|         - |  8511 | `		}` |
|         3 |  8512 | `		goto Synchronize;` |
|         - |  8513 | `	}` |
|         - |  8514 | `	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and` |
|         - |  8515 | `	 * the read visibility must not be narrower than the set visibility. */` |
|    469607 |  8516 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
|        13 |  8517 | `		const char *zAvErr = 0;` |
|        19 |  8518 | `		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE` |
|        10 |  8519 | `			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED` |
|         2 |  8520 | `			: PH7_CLASS_PROT_PUBLIC;` |
|        13 |  8521 | `		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 |  8522 | `			zAvErr = "Property with asymmetric visibility %z::$%z must have type";` |
|        13 |  8523 | `		}else if( iProtection > iSetLevel ){` |
|       ! 0 |  8524 | `			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";` |
|       ! 0 |  8525 | `		}` |
|        13 |  8526 | `		if( zAvErr ){` |
|       ! 0 |  8527 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sName,pName);` |
|       ! 0 |  8528 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8529 | `				return SXERR_ABORT;` |
|         - |  8530 | `			}` |
|       ! 0 |  8531 | `			goto Synchronize;` |
|         - |  8532 | `		}` |
|         6 |  8533 | `	}` |
|         - |  8534 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|         - |  8535 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|    469607 |  8536 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|        43 |  8537 | `		const char *zRoErr = 0;` |
|        43 |  8538 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|         3 |  8539 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|        42 |  8540 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         6 |  8541 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|        39 |  8542 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|         6 |  8543 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|         2 |  8544 | `		}` |
|        43 |  8545 | `		if( zRoErr ){` |
|        13 |  8546 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|        13 |  8547 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8548 | `				return SXERR_ABORT;` |
|         - |  8549 | `			}` |
|        13 |  8550 | `			goto Synchronize;` |
|         - |  8551 | `		}` |
|        14 |  8552 | `	}` |
|         - |  8553 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|         - |  8554 | `	 * type atom or any union alternative. void/never are already rejected` |
|         - |  8555 | `	 * by the type parser. */` |
|    469597 |  8556 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       485 |  8557 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|         - |  8558 | `			&sTypeText,` |
|       320 |  8559 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       160 |  8560 | `			"Property %z::$%z cannot have type %z",nLine);` |
|       325 |  8561 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8562 | `			return SXERR_ABORT;` |
|       325 |  8563 | `		}else if( rc != SXRET_OK ){` |
|       ! 0 |  8564 | `			goto Synchronize;` |
|         - |  8565 | `		}` |
|       160 |  8566 | `	}` |
|         - |  8567 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|    469597 |  8568 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|         4 |  8569 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  8570 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|         3 |  8571 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8572 | `			return SXERR_ABORT;` |
|         - |  8573 | `		}` |
|         3 |  8574 | `		goto Synchronize;` |
|         - |  8575 | `	}` |
|         - |  8576 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|         - |  8577 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|         - |  8578 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|         - |  8579 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|         - |  8580 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|         - |  8581 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|    469595 |  8582 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|         6 |  8583 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8584 | `			"New expressions are not supported in this context");` |
|         6 |  8585 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8586 | `			return SXERR_ABORT;` |
|         - |  8587 | `		}` |
|         6 |  8588 | `		goto Synchronize;` |
|         - |  8589 | `	}` |
|         - |  8590 | `	/* Allocate a new class attribute */` |
|    469591 |  8591 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|    469591 |  8592 | `	if( pAttr ){` |
|    469591 |  8593 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|    469591 |  8594 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8595 | `			return SXERR_ABORT;` |
|         - |  8596 | `		}` |
|    234793 |  8597 | `	}` |
|    469591 |  8598 | `	if( pAttr == 0 ){` |
|       ! 0 |  8599 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 |  8600 | `		return SXERR_ABORT;` |
|         - |  8601 | `	}` |
|    469591 |  8602 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       323 |  8603 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       159 |  8604 | `	}` |
|    469591 |  8605 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|         - |  8606 | `		SySet *pInstrContainer;` |
|    343293 |  8607 | `		SyToken *pSavedDefEnd = pGen->pEnd;` |
|    343293 |  8608 | `		pGen->pIn++; /*Jump the equal sign */` |
|         - |  8609 | `		{` |
|         - |  8610 | `			/* Delimit the default expression: it ends at the declaration's` |
|         - |  8611 | `			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list` |
|         - |  8612 | ``			 * (`public string $w = "init" { get => …; }`) — the expression`` |
|         - |  8613 | `			 * compiler would otherwise run into the hook tokens. */` |
|    343293 |  8614 | `			SyToken *pScan = pGen->pIn;` |
|    343293 |  8615 | `			sxi32 iNest = 0;` |
|    742059 |  8616 | `			while( pScan < pGen->pEnd ){` |
|    742059 |  8617 | `				if( pScan->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     43405 |  8618 | `					iNest++;` |
|    720359 |  8619 | `				}else if( pScan->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|     43405 |  8620 | `					iNest--;` |
|    676959 |  8621 | `				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI\|PH7_TK_COMMA\|PH7_TK_OCB)) ){` |
|    343293 |  8622 | `					break;` |
|         - |  8623 | `				}` |
|    398771 |  8624 | `				pScan++;` |
|         5 |  8625 | `			}` |
|    343293 |  8626 | `			pGen->pEnd = pScan;` |
|         - |  8627 | `		}` |
|         - |  8628 | `		/* Swap bytecode container */` |
|    343293 |  8629 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    343293 |  8630 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|         - |  8631 | `		/* Compile attribute value.` |
|         - |  8632 | `		 */` |
|    343293 |  8633 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    343293 |  8634 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 |  8635 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|       ! 0 |  8636 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8637 | `				return SXERR_ABORT;` |
|         - |  8638 | `			}` |
|       ! 0 |  8639 | `		}` |
|         - |  8640 | `		/* Emit the done instruction */` |
|    343293 |  8641 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    343293 |  8642 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    343293 |  8643 | `		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */` |
|    343293 |  8644 | `		pGen->pEnd = pSavedDefEnd;` |
|    171644 |  8645 | `	}` |
|         - |  8646 | `	/* All done,install the attribute */` |
|    469591 |  8647 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|    469591 |  8648 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8649 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8650 | `		return SXERR_ABORT;` |
|         - |  8651 | `	}` |
|    469591 |  8652 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){` |
|         - |  8653 | ``		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.`` |
|         - |  8654 | `		 * The list ends the declaration at '}' — no trailing ';', no comma list. */` |
|        95 |  8655 | `		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);` |
|        95 |  8656 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8657 | `			return SXERR_ABORT;` |
|         - |  8658 | `		}` |
|        95 |  8659 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  8660 | `			goto Synchronize;` |
|         - |  8661 | `		}` |
|        95 |  8662 | `		SySetRelease(&aUnionAlts);` |
|        95 |  8663 | `		return SXRET_OK;` |
|         - |  8664 | `	}` |
|    469497 |  8665 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - |  8666 | ``		/* php 8.4: `abstract` on a property requires a hook list (php's exact`` |
|         - |  8667 | `		 * wording differs per declaration site) */` |
|       ! 0 |  8668 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 |  8669 | `			(pClass->iFlags & PH7_CLASS_INTERFACE)` |
|         - |  8670 | `				? "Interfaces may only include hooked properties"` |
|         - |  8671 | `				: "Only hooked properties may be declared abstract");` |
|       ! 0 |  8672 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8673 | `			return SXERR_ABORT;` |
|         - |  8674 | `		}` |
|       ! 0 |  8675 | `		goto Synchronize;` |
|         - |  8676 | `	}` |
|    469497 |  8677 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|         - |  8678 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|         5 |  8679 | `		pGen->pIn++; /* Jump the comma */` |
|         5 |  8680 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|       ! 0 |  8681 | `			SyToken *pTok = pGen->pIn;` |
|       ! 0 |  8682 | `			if( pTok >= pGen->pEnd ){` |
|       ! 0 |  8683 | `				pTok--;` |
|       ! 0 |  8684 | `			}` |
|       ! 0 |  8685 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  8686 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|       ! 0 |  8687 | `				&pTok->sData,&pClass->sName);` |
|       ! 0 |  8688 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8689 | `				return SXERR_ABORT;` |
|         - |  8690 | `			}` |
|       ! 0 |  8691 | `		}else{` |
|         5 |  8692 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         5 |  8693 | `				goto loop;` |
|         - |  8694 | `			}` |
|         - |  8695 | `		}` |
|       ! 0 |  8696 | `	}` |
|    469493 |  8697 | `	SySetRelease(&aUnionAlts);` |
|    469493 |  8698 | `	return SXRET_OK;` |
|         9 |  8699 | `Synchronize:` |
|         - |  8700 | `	/* Synchronize with the first semi-colon */` |
|        56 |  8701 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        37 |  8702 | `		pGen->pIn++;` |
|         3 |  8703 | `	}` |
|        22 |  8704 | `	SySetRelease(&aUnionAlts);` |
|        22 |  8705 | `	return SXERR_CORRUPT;` |
|    234805 |  8706 | `}` |
|         - |  8707 | `/*` |
|         - |  8708 | ` * Compile a class method.` |
|         - |  8709 | ` *` |
|         - |  8710 | ` * Refer to the official documentation for more information` |
|         - |  8711 | ` * on the powerful extension introduced by the PH7 engine` |
|         - |  8712 | ` * to the OO subsystem such as full type hinting,method` |
|         - |  8713 | ` * overloading and many more.` |
|         - |  8714 | ` */` |
|   2458922 |  8715 | `static sxi32 GenStateCompileClassMethod(` |
|         - |  8716 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  8717 | `	sxi32 iProtection,   /* Visibility level */` |
|         - |  8718 | `	sxi32 iFlags,        /* Configuration flags */` |
|         - |  8719 | `	int doBody,          /* TRUE to process method body */` |
|         - |  8720 | `	ph7_class *pClass    /* Class this method belongs */` |
|         - |  8721 | `	)` |
|         5 |  8722 | `{` |
|   2458927 |  8723 | `	sxu32 nLine = pGen->pIn->nLine;` |
|   2458927 |  8724 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|         - |  8725 | `	ph7_class_method *pMeth;` |
|         - |  8726 | `	sxi32 iFuncFlags;` |
|         - |  8727 | `	SyString *pName;` |
|         - |  8728 | `	SyToken *pEnd;` |
|         - |  8729 | `	sxi32 rc;` |
|         - |  8730 | `	/* Extract visibility level */` |
|   2458927 |  8731 | `	iProtection = GetProtectionLevel(iProtection);` |
|   2458927 |  8732 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   2458927 |  8733 | `	iFuncFlags = 0;` |
|   2458927 |  8734 | `	if( pGen->pIn >= pGen->pEnd ){` |
|         - |  8735 | `		/* Invalid method name */` |
|       ! 0 |  8736 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|       ! 0 |  8737 | `		if( rc == SXERR_ABORT ){` |
|         - |  8738 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8739 | `			return SXERR_ABORT;` |
|         - |  8740 | `		}` |
|       ! 0 |  8741 | `		goto Synchronize;` |
|         - |  8742 | `	}` |
|   2458927 |  8743 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|         - |  8744 | `		/* Return by reference,remember that */` |
|       ! 0 |  8745 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|         - |  8746 | `		/* Jump the '&' token */` |
|       ! 0 |  8747 | `		pGen->pIn++;` |
|       ! 0 |  8748 | `	}` |
|   2458927 |  8749 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - |  8750 | `		/* Invalid method name */` |
|       ! 0 |  8751 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|       ! 0 |  8752 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8753 | `			return SXERR_ABORT;` |
|         - |  8754 | `		}` |
|       ! 0 |  8755 | `		goto Synchronize;` |
|         - |  8756 | `	}` |
|         - |  8757 | `	/* Peek method name */` |
|   2458927 |  8758 | `	pName = &pGen->pIn->sData;` |
|   2458927 |  8759 | `	nLine = pGen->pIn->nLine;` |
|         - |  8760 | `	/* Jump the method name */` |
|   2458927 |  8761 | `	pGen->pIn++;` |
|   2458927 |  8762 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         - |  8763 | `		/* Abstract method */` |
|    141835 |  8764 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       ! 0 |  8765 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8766 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|       ! 0 |  8767 | `				&pClass->sName,pName);` |
|       ! 0 |  8768 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8769 | `				return SXERR_ABORT;` |
|         - |  8770 | `			}` |
|       ! 0 |  8771 | `		}` |
|         - |  8772 | `		/* Assemble method signature only */` |
|    141835 |  8773 | `		doBody = FALSE;` |
|     70915 |  8774 | `	}` |
|   2458927 |  8775 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - |  8776 | `		/* Syntax error */` |
|       ! 0 |  8777 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|       ! 0 |  8778 | `		if( rc == SXERR_ABORT ){` |
|         - |  8779 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8780 | `			return SXERR_ABORT;` |
|         - |  8781 | `		}` |
|       ! 0 |  8782 | `		goto Synchronize;` |
|         - |  8783 | `	}` |
|         - |  8784 | `	/* Allocate a new class_method instance */` |
|   2458927 |  8785 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|   2458927 |  8786 | `	if( pMeth == 0 ){` |
|       ! 0 |  8787 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8788 | `		return SXERR_ABORT;` |
|         - |  8789 | `	}` |
|   2458927 |  8790 | `	pMeth->sFunc.nLine = nKwLine;` |
|   2458927 |  8791 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|   2458927 |  8792 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  8793 | `		return SXERR_ABORT;` |
|         - |  8794 | `	}` |
|         - |  8795 | `	/* Jump the left parenthesis '(' */` |
|   2458927 |  8796 | `	pGen->pIn++;` |
|   2458927 |  8797 | `	pEnd = 0; /* cc warning */` |
|         - |  8798 | `	/* Delimit the method signature */` |
|   2458927 |  8799 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   2458927 |  8800 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  8801 | `		/* Syntax error */` |
|         3 |  8802 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|         3 |  8803 | `		if( rc == SXERR_ABORT ){` |
|         - |  8804 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  8805 | `			return SXERR_ABORT;` |
|         - |  8806 | `		}` |
|         3 |  8807 | `		goto Synchronize;` |
|         - |  8808 | `	}` |
|         - |  8809 | `	{` |
|   2458925 |  8810 | `		int bIsCtor = 0;` |
|   2458925 |  8811 | `		int bAbstractCtor = 0;` |
|   2458920 |  8812 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|   1436365 |  8813 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|   2374154 |  8814 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|    169547 |  8815 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|         3 |  8816 | `				bAbstractCtor = 1;` |
|         2 |  8817 | `			}else{` |
|    169545 |  8818 | `				bIsCtor = 1;` |
|         - |  8819 | `			}` |
|     84771 |  8820 | `		}` |
|   2458925 |  8821 | `		if( pGen->pIn < pEnd ){` |
|         - |  8822 | `			/* Collect method arguments */` |
|    882795 |  8823 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|    882795 |  8824 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  8825 | `				return SXERR_ABORT;` |
|         - |  8826 | `			}` |
|    441395 |  8827 | `		}` |
|         - |  8828 | `	}` |
|         - |  8829 | `	/* Point past ')' and parse optional return type ': type' */` |
|   2458925 |  8830 | `	pGen->pIn = &pEnd[1];` |
|         - |  8831 | `	{` |
|   2458925 |  8832 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|   2458925 |  8833 | `		if( rcRt == SXERR_ABORT ){` |
|       ! 0 |  8834 | `			return SXERR_ABORT;` |
|   2458925 |  8835 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       ! 0 |  8836 | `			goto Synchronize;` |
|         - |  8837 | `		}` |
|         - |  8838 | `	}` |
|         - |  8839 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|         - |  8840 | `	 * property init/typecheck is handled by the generic typed-property path` |
|         - |  8841 | `	 * since we mint real ph7_class_attr entries. */` |
|         - |  8842 | `	{` |
|   2458925 |  8843 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|         - |  8844 | `		sxu32 i;` |
|   3782949 |  8845 | `		for( i = 0; i < nArg; i++ ){` |
|   1324039 |  8846 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|         - |  8847 | `			ph7_class_attr *pAttr;` |
|   1324039 |  8848 | `			sxi32 iAttrFlags = 0;` |
|         - |  8849 | `			int bArgTyped;` |
|   1324039 |  8850 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   1323955 |  8851 | `				continue;` |
|         - |  8852 | `			}` |
|         - |  8853 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|         - |  8854 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|         - |  8855 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|        59 |  8856 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|        90 |  8857 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|        89 |  8858 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|         3 |  8859 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8860 | `					"Cannot declare variadic promoted property");` |
|         3 |  8861 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  8862 | `					return SXERR_ABORT;` |
|         - |  8863 | `				}` |
|         3 |  8864 | `				goto Synchronize;` |
|         - |  8865 | `			}` |
|         - |  8866 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|         - |  8867 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|         - |  8868 | `			 * appear as an alternative of a union type. */` |
|        87 |  8869 | `			if( bArgTyped ){` |
|       122 |  8870 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|        78 |  8871 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|        78 |  8872 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|        39 |  8873 | `					"Property %z::$%z cannot have type %z",nLine);` |
|        83 |  8874 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  8875 | `					return SXERR_ABORT;` |
|        83 |  8876 | `				}else if( rc != SXRET_OK ){` |
|         6 |  8877 | `					goto Synchronize;` |
|         - |  8878 | `				}` |
|        37 |  8879 | `			}` |
|         - |  8880 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|        83 |  8881 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|         4 |  8882 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  8883 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|         3 |  8884 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  8885 | `					return SXERR_ABORT;` |
|         - |  8886 | `				}` |
|         3 |  8887 | `				goto Synchronize;` |
|         - |  8888 | `			}` |
|        81 |  8889 | `			if( bArgTyped ){` |
|        77 |  8890 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|        36 |  8891 | `			}` |
|        81 |  8892 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|         3 |  8893 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|         1 |  8894 | `			}` |
|        81 |  8895 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|         8 |  8896 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|         3 |  8897 | `			}` |
|        81 |  8898 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|         - |  8899 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|         - |  8900 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|        26 |  8901 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|         4 |  8902 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 |  8903 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|         3 |  8904 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  8905 | `						return SXERR_ABORT;` |
|         - |  8906 | `					}` |
|         3 |  8907 | `					goto Synchronize;` |
|         - |  8908 | `				}` |
|        24 |  8909 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|        10 |  8910 | `			}` |
|        79 |  8911 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|         - |  8912 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|         5 |  8913 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       ! 0 |  8914 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  8915 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|       ! 0 |  8916 | `						&pClass->sName,&pArg->sName);` |
|       ! 0 |  8917 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  8918 | `						return SXERR_ABORT;` |
|         - |  8919 | `					}` |
|       ! 0 |  8920 | `					goto Synchronize;` |
|         - |  8921 | `				}` |
|         5 |  8922 | `				iAttrFlags \|= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)` |
|         2 |  8923 | `					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;` |
|         2 |  8924 | `			}` |
|        79 |  8925 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|        79 |  8926 | `			if( pAttr == 0 ){` |
|       ! 0 |  8927 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8928 | `				return SXERR_ABORT;` |
|         - |  8929 | `			}` |
|        79 |  8930 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|        77 |  8931 | `				pAttr->nType = pArg->nType;` |
|        77 |  8932 | `				pAttr->sClass = pArg->sClass;` |
|        77 |  8933 | `				pAttr->sTypeName = pArg->sTypeName;` |
|        77 |  8934 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|         - |  8935 | `					sxu32 k;` |
|        20 |  8936 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|        14 |  8937 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|        14 |  8938 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|         8 |  8939 | `					}` |
|         3 |  8940 | `				}` |
|        36 |  8941 | `			}` |
|        79 |  8942 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|        79 |  8943 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  8944 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8945 | `				return SXERR_ABORT;` |
|         - |  8946 | `			}` |
|        42 |  8947 | `		}` |
|         - |  8948 | `	}` |
|   2458915 |  8949 | `	if( doBody ){` |
|         - |  8950 | `		/* Compile method body */` |
|   2317085 |  8951 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|   2317085 |  8952 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  8953 | `			return SXERR_ABORT;` |
|         - |  8954 | `		}` |
|         - |  8955 | `		/* The cursor sits just past the body's closing brace */` |
|   2317085 |  8956 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|   1158545 |  8957 | `	}else{` |
|         - |  8958 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|    141835 |  8959 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|    141835 |  8960 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|     70915 |  8961 | `		}` |
|         - |  8962 | `		/* Only method signature is allowed */` |
|    141835 |  8963 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|       ! 0 |  8964 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  8965 | `				"Expected ';' after method signature '%z'",pName);` |
|       ! 0 |  8966 | `				if( rc == SXERR_ABORT ){` |
|         - |  8967 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  8968 | `					return SXERR_ABORT;` |
|         - |  8969 | `				}` |
|       ! 0 |  8970 | `				return SXERR_CORRUPT;` |
|         - |  8971 | `			}` |
|         - |  8972 | `	}` |
|         - |  8973 | `	/* All done,install the method */` |
|   2458915 |  8974 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|   2458915 |  8975 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  8976 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  8977 | `		return SXERR_ABORT;` |
|         - |  8978 | `	}` |
|   2458915 |  8979 | `	return SXRET_OK;` |
|         6 |  8980 | `Synchronize:` |
|         - |  8981 | `	/* Synchronize with the first semi-colon */` |
|        40 |  8982 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|        28 |  8983 | `		pGen->pIn++;` |
|         4 |  8984 | `	}` |
|        16 |  8985 | `	return SXERR_CORRUPT;` |
|   1229466 |  8986 | `}` |
|         - |  8987 | `/*` |
|         - |  8988 | `` * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a`` |
|         - |  8989 | ` * property declaration. Each hook body is synthesized into a hidden public` |
|         - |  8990 | ` * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,` |
|         - |  8991 | ` * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /` |
|         - |  8992 | ` * OP_STORE route reads and plain writes through them (a per-instance guard` |
|         - |  8993 | ` * makes $this->NAME inside a hook body address the raw backing slot — php's` |
|         - |  8994 | `` * rule that hooks see the backing store). `get => expr;` compiles as an`` |
|         - |  8995 | `` * implicit return (the arrow-fn pattern); `set => expr;` compiles the same`` |
|         - |  8996 | ` * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return` |
|         - |  8997 | `` * value to the backing slot. A `set` without a parameter list receives the`` |
|         - |  8998 | `` * implicit `$value` formal.`` |
|         - |  8999 | ` * On entry pGen->pIn sits on '{'; on success it sits just past '}'.` |
|         - |  9000 | ` */` |
|         - |  9001 | `/*` |
|         - |  9002 | `` * Whether any token in [pStart, pEnd) spells `$this->NAME` (this property's own`` |
|         - |  9003 | `` * name; `?->` and `::` member ops count too). php 8.4's virtual-vs-backed rule:`` |
|         - |  9004 | ` * a hooked property is BACKED iff any of its OWN hook bodies references it by` |
|         - |  9005 | ` * name through $this — otherwise it is VIRTUAL: no backing store, no default` |
|         - |  9006 | ` * allowed, excluded from the raw object surfaces.` |
|         - |  9007 | ` */` |
|        94 |  9008 | `static int GenStateHookBodyRefsProp(SyToken *pStart,SyToken *pEnd,const SyString *pName)` |
|         1 |  9009 | `{` |
|         - |  9010 | `	SyToken *p;` |
|       345 |  9011 | `	for( p = pStart ; p + 1 < pEnd ; p++ ){` |
|       303 |  9012 | `		if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|       223 |  9013 | `			continue;` |
|         - |  9014 | `		}` |
|         - |  9015 | ``		/* `$this->NAME` (also `?->`/`::`) */`` |
|        80 |  9016 | `		if( p + 3 < pEnd` |
|        80 |  9017 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        80 |  9018 | `		 && p[1].sData.nByte == sizeof("this")-1` |
|        73 |  9019 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0` |
|        66 |  9020 | `		 && GenStateTokenIsMemberOp(&p[2])` |
|        66 |  9021 | `		 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        66 |  9022 | `		 && p[3].sData.nByte == pName->nByte` |
|        60 |  9023 | `		 && SyMemcmp((const void *)p[3].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        51 |  9024 | `			return 1;` |
|         - |  9025 | `		}` |
|         - |  9026 | ``		/* `parent::$NAME` (the parent::$x::get() hook-call form): the parent`` |
|         - |  9027 | `		 * hook operates on the shared per-instance backing store, so the` |
|         - |  9028 | `		 * property is backed (php compiles a default alongside it). */` |
|        30 |  9029 | `		if( p > pStart` |
|        26 |  9030 | `		 && GenStateTokenIsMemberOp(&p[-1])` |
|        12 |  9031 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         2 |  9032 | `		 && p[1].sData.nByte == pName->nByte` |
|         3 |  9033 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|         3 |  9034 | `			return 1;` |
|         - |  9035 | `		}` |
|        15 |  9036 | `	}` |
|        43 |  9037 | `	return 0;` |
|        48 |  9038 | `}` |
|         - |  9039 | `/*` |
|         - |  9040 | ` * True when p opens php 8.4's parent-hook call form` |
|         - |  9041 | `` * `parent :: $ NAME :: get\|set (` (7 tokens through the '(').`` |
|         - |  9042 | ` */` |
|       990 |  9043 | `static int GenStateIsParentHookCallAt(SyToken *p,SyToken *pEnd)` |
|         1 |  9044 | `{` |
|      1167 |  9045 | `	return p + 6 < pEnd` |
|       671 |  9046 | `	 && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       250 |  9047 | `	 && p->sData.nByte == sizeof("parent")-1` |
|        81 |  9048 | `	 && SyMemcmp((const void *)p->sData.zString,(const void *)"parent",sizeof("parent")-1) == 0` |
|        11 |  9049 | `	 && GenStateTokenIsMemberOp(&p[1])` |
|         8 |  9050 | `	 && (p[2].nType & PH7_TK_DOLLAR) != 0` |
|         8 |  9051 | `	 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 |  9052 | `	 && GenStateTokenIsMemberOp(&p[4])` |
|         8 |  9053 | `	 && (p[5].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|         8 |  9054 | `	 && p[5].sData.nByte == 3` |
|         8 |  9055 | `	 && (SyMemcmp((const void *)p[5].sData.zString,(const void *)"get",3) == 0` |
|         6 |  9056 | `	  \|\| SyMemcmp((const void *)p[5].sData.zString,(const void *)"set",3) == 0)` |
|      1166 |  9057 | `	 && (p[6].nType & PH7_TK_LPAREN) != 0;` |
|         1 |  9058 | `}` |
|         - |  9059 | `/*` |
|         - |  9060 | `` * Rewrite php 8.4 `parent::$x::get(...)` / `parent::$x::set(...)` calls in a`` |
|         - |  9061 | ` * hook body into calls of the parent class's synthesized hook method` |
|         - |  9062 | `` * (`parent::__phl_hook_get_x(...)`). Builds a token COPY into pCopy (only`` |
|         - |  9063 | ` * called when GenStateIsParentHookCallAt matched somewhere in the range);` |
|         - |  9064 | ` * copied tokens keep pointing at source-owned lexeme storage, and the` |
|         - |  9065 | ` * synthesized method-name lexemes are VM-allocator owned. Returns SXRET_OK` |
|         - |  9066 | ` * or SXERR_MEM.` |
|         - |  9067 | ` */` |
|         4 |  9068 | `static sxi32 GenStateRewriteParentHookCalls(ph7_gen_state *pGen,SySet *pCopy,` |
|         - |  9069 | `	SyToken *pStart,SyToken *pEnd)` |
|         1 |  9070 | `{` |
|         5 |  9071 | `	SyToken *p = pStart;` |
|        35 |  9072 | `	while( p < pEnd ){` |
|        31 |  9073 | `		if( GenStateIsParentHookCallAt(p,pEnd) ){` |
|         - |  9074 | `			SyToken sTok;` |
|         - |  9075 | `			char zName[384];` |
|         - |  9076 | `			sxu32 nName;` |
|         - |  9077 | `			char *zDup;` |
|         - |  9078 | ``			/* `parent` `::` */`` |
|         5 |  9079 | `			SySetPut(pCopy,(const void *)&p[0]);` |
|         5 |  9080 | `			SySetPut(pCopy,(const void *)&p[1]);` |
|         7 |  9081 | `			nName = SyBufferFormat(zName,sizeof(zName),"__phl_hook_%.3s_%.*s",` |
|         4 |  9082 | `				p[5].sData.zString,(int)p[3].sData.nByte,p[3].sData.zString);` |
|         5 |  9083 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|         5 |  9084 | `			if( zDup == 0 ){` |
|       ! 0 |  9085 | `				return SXERR_MEM;` |
|         - |  9086 | `			}` |
|         5 |  9087 | `			sTok = p[3]; /* keep the line info of the property name */` |
|         5 |  9088 | `			sTok.nType = PH7_TK_ID;` |
|         5 |  9089 | `			SyStringInitFromBuf(&sTok.sData,zDup,nName);` |
|         5 |  9090 | `			sTok.pUserData = 0;` |
|         5 |  9091 | `			SySetPut(pCopy,(const void *)&sTok);` |
|         5 |  9092 | `			p += 6; /* continue at the '(' — arguments copy through unchanged */` |
|         5 |  9093 | `			continue;` |
|         - |  9094 | `		}` |
|        27 |  9095 | `		SySetPut(pCopy,(const void *)p);` |
|        27 |  9096 | `		p++;` |
|         1 |  9097 | `	}` |
|         5 |  9098 | `	return SXRET_OK;` |
|         3 |  9099 | `}` |
|        94 |  9100 | `static sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)` |
|         1 |  9101 | `{` |
|        95 |  9102 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  9103 | `	sxi32 rc;` |
|        95 |  9104 | `	int bRefsSelf = 0;` |
|        95 |  9105 | `	pGen->pIn++; /* Jump '{' */` |
|       253 |  9106 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|         - |  9107 | `		char zHook[384];` |
|         - |  9108 | `		SyString sHookName;` |
|         - |  9109 | `		ph7_class_method *pMeth;` |
|         - |  9110 | `		int bGet;` |
|       159 |  9111 | `		sxu32 nHLine = pGen->pIn->nLine;` |
|       159 |  9112 | `		if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        15 |  9113 | `			pGen->pIn++; /* stray ';' between hooks */` |
|        22 |  9114 | `			continue;` |
|         - |  9115 | `		}` |
|       145 |  9116 | `		if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|         - |  9117 | `			/* by-reference get hook: not modeled (loud, recorded) */` |
|       ! 0 |  9118 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9119 | `				"By-reference property hooks are not supported for %z::$%z",` |
|       ! 0 |  9120 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9121 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9122 | `				return SXERR_ABORT;` |
|         - |  9123 | `			}` |
|       ! 0 |  9124 | `			return SXERR_CORRUPT;` |
|         - |  9125 | `		}` |
|       145 |  9126 | `		if( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 |  9127 | `			goto HookSyntax;` |
|         - |  9128 | `		}` |
|       144 |  9129 | `		if( pGen->pIn->sData.nByte == 3` |
|       145 |  9130 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"get",3) == 0 ){` |
|        79 |  9131 | `			bGet = 1;` |
|       106 |  9132 | `		}else if( pGen->pIn->sData.nByte == 3` |
|        67 |  9133 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"set",3) == 0 ){` |
|        67 |  9134 | `			bGet = 0;` |
|        34 |  9135 | `		}else{` |
|       ! 0 |  9136 | `			goto HookSyntax;` |
|         - |  9137 | `		}` |
|       145 |  9138 | `		pGen->pIn++; /* Jump 'get'/'set' */` |
|       145 |  9139 | `		sHookName.zString = zHook;` |
|       217 |  9140 | `		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",` |
|        72 |  9141 | `			bGet ? "get" : "set",&pAttr->sName);` |
|       145 |  9142 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB)) ){` |
|         - |  9143 | ``			/* Bare `get;` / `set;` — an ABSTRACT hook declaration (php 8.4):`` |
|         - |  9144 | ``			 * legal only on an `abstract` property or inside an interface. The`` |
|         - |  9145 | `			 * synthesized method carries PH7_CLASS_ATTR_ABSTRACT and rides the` |
|         - |  9146 | `			 * existing must-implement machinery; a concrete hook override (or a` |
|         - |  9147 | `			 * plain property, see GenStateCheckAbstractMethods) satisfies it. */` |
|        14 |  9148 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|         8 |  9149 | `			 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 |  9150 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9151 | `					"Non-abstract property hook must have a body");` |
|       ! 0 |  9152 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9153 | `					return SXERR_ABORT;` |
|         - |  9154 | `				}` |
|       ! 0 |  9155 | `				return SXERR_CORRUPT;` |
|         - |  9156 | `			}` |
|        15 |  9157 | `			pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - |  9158 | `				PH7_CLASS_PROT_PUBLIC,PH7_CLASS_ATTR_ABSTRACT,0);` |
|        15 |  9159 | `			if( pMeth == 0 ){` |
|       ! 0 |  9160 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9161 | `				return SXERR_ABORT;` |
|         - |  9162 | `			}` |
|        15 |  9163 | `			pMeth->sFunc.nLine = nHLine;` |
|        15 |  9164 | `			if( !bGet ){` |
|         - |  9165 | ``				/* The implicit `$value` formal keeps the stub's signature`` |
|         - |  9166 | `				 * compatible with concrete set-hook implementations (which` |
|         - |  9167 | `				 * always carry one parameter). It takes the PROPERTY's declared` |
|         - |  9168 | `				 * type (php: the abstract set's parameter type IS the property` |
|         - |  9169 | `				 * type), so the override contravariance check accepts a typed` |
|         - |  9170 | ``				 * `set(int $v)` implementation on an `int $x` requirement. */`` |
|         - |  9171 | `				ph7_vm_func_arg sVArg;` |
|         7 |  9172 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|         7 |  9173 | `				if( zVName == 0 ){` |
|       ! 0 |  9174 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9175 | `					return SXERR_ABORT;` |
|         - |  9176 | `				}` |
|         7 |  9177 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|         7 |  9178 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|         7 |  9179 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         7 |  9180 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|         7 |  9181 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|         7 |  9182 | `				sVArg.nType = pAttr->nType;` |
|         7 |  9183 | `				sVArg.sClass = pAttr->sClass;` |
|         7 |  9184 | `				sVArg.sTypeName = pAttr->sTypeName;` |
|         7 |  9185 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       ! 0 |  9186 | `					sVArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|       ! 0 |  9187 | `				}` |
|         7 |  9188 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|         3 |  9189 | `			}` |
|        15 |  9190 | `			rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|        15 |  9191 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9192 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9193 | `				return SXERR_ABORT;` |
|         - |  9194 | `			}` |
|        15 |  9195 | `			pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|        15 |  9196 | `			continue; /* the loop consumes the ';' as a stray separator */` |
|         - |  9197 | `		}` |
|       130 |  9198 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|       131 |  9199 | `		 \|\| (pClass->iFlags & PH7_CLASS_INTERFACE) != 0 ){` |
|         - |  9200 | `			/* php: an abstract/interface property hook cannot carry a body */` |
|       ! 0 |  9201 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|         - |  9202 | `				"Abstract property hook cannot have body");` |
|       ! 0 |  9203 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9204 | `				return SXERR_ABORT;` |
|         - |  9205 | `			}` |
|       ! 0 |  9206 | `			return SXERR_CORRUPT;` |
|         - |  9207 | `		}` |
|       131 |  9208 | `		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|         - |  9209 | `			PH7_CLASS_PROT_PUBLIC,0,0);` |
|       131 |  9210 | `		if( pMeth == 0 ){` |
|       ! 0 |  9211 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9212 | `			return SXERR_ABORT;` |
|         - |  9213 | `		}` |
|       131 |  9214 | `		pMeth->sFunc.nLine = nHLine;` |
|       131 |  9215 | `		if( !bGet ){` |
|         - |  9216 | ``			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */`` |
|        61 |  9217 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        17 |  9218 | `				SyToken *pRp = 0;` |
|        17 |  9219 | `				pGen->pIn++;` |
|        17 |  9220 | `				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|        17 |  9221 | `				if( pRp >= pGen->pEnd ){` |
|       ! 0 |  9222 | `					goto HookSyntax;` |
|         - |  9223 | `				}` |
|        17 |  9224 | `				if( pGen->pIn < pRp ){` |
|        17 |  9225 | `					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);` |
|        17 |  9226 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9227 | `						return SXERR_ABORT;` |
|         - |  9228 | `					}` |
|         8 |  9229 | `				}` |
|        17 |  9230 | `				pGen->pIn = &pRp[1];` |
|         8 |  9231 | `			}` |
|        61 |  9232 | `			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){` |
|         - |  9233 | `				/* Implicit $value formal */` |
|         - |  9234 | `				ph7_vm_func_arg sVArg;` |
|        45 |  9235 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|        45 |  9236 | `				if( zVName == 0 ){` |
|       ! 0 |  9237 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9238 | `					return SXERR_ABORT;` |
|         - |  9239 | `				}` |
|        45 |  9240 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|        45 |  9241 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|        45 |  9242 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        45 |  9243 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        45 |  9244 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|        45 |  9245 | `				SyStringInitFromBuf(&sVArg.sTypeName,0,0);` |
|        45 |  9246 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|        22 |  9247 | `			}` |
|        30 |  9248 | `		}` |
|       165 |  9249 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - |  9250 | `			/* Block body */` |
|        69 |  9251 | `			SyToken *pBodyStart = pGen->pIn;` |
|        69 |  9252 | `			SyToken *pCloser = 0;` |
|        69 |  9253 | `			int bParentCall = 0;` |
|        69 |  9254 | `			PH7_DelimitNestedTokens(&pBodyStart[1],pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);` |
|        69 |  9255 | `			if( pCloser < pGen->pEnd ){` |
|         - |  9256 | `				SyToken *pScan;` |
|       753 |  9257 | `				for( pScan = &pBodyStart[1] ; pScan < pCloser ; pScan++ ){` |
|       687 |  9258 | `					if( GenStateIsParentHookCallAt(pScan,pCloser) ){` |
|         3 |  9259 | `						bParentCall = 1;` |
|         3 |  9260 | `						break;` |
|         - |  9261 | `					}` |
|       343 |  9262 | `				}` |
|        34 |  9263 | `			}` |
|        69 |  9264 | `			if( bParentCall ){` |
|         - |  9265 | ``				/* `parent::$x::get()` inside the body: compile a REWRITTEN copy`` |
|         - |  9266 | `				 * of the body tokens (the call becomes the parent's synthesized` |
|         - |  9267 | `				 * hook method), then continue past the original body. */` |
|         - |  9268 | `				SySet sBody;` |
|         3 |  9269 | `				SyToken *pSavedEnd = pGen->pEnd;` |
|         3 |  9270 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 |  9271 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,&pCloser[1]);` |
|         3 |  9272 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9273 | `					SySetRelease(&sBody);` |
|       ! 0 |  9274 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9275 | `					return SXERR_ABORT;` |
|         - |  9276 | `				}` |
|         3 |  9277 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 |  9278 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         3 |  9279 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|         3 |  9280 | `				pGen->pIn = &pCloser[1];` |
|         3 |  9281 | `				pGen->pEnd = pSavedEnd;` |
|         3 |  9282 | `				SySetRelease(&sBody);` |
|         3 |  9283 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9284 | `					return SXERR_ABORT;` |
|         - |  9285 | `				}` |
|         3 |  9286 | `				pMeth->sFunc.nEndLine = pCloser->nLine;` |
|         2 |  9287 | `			}else{` |
|        67 |  9288 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|        67 |  9289 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9290 | `					return SXERR_ABORT;` |
|         - |  9291 | `				}` |
|        67 |  9292 | `				pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|         - |  9293 | `			}` |
|        69 |  9294 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        17 |  9295 | `				bRefsSelf = 1;` |
|         9 |  9296 | `			}` |
|       128 |  9297 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){` |
|         - |  9298 | ``			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */`` |
|         - |  9299 | `			GenBlock *pBlock;` |
|         - |  9300 | `			SySet *pInstrContainer;` |
|         - |  9301 | `			SyToken *pBodyStart;` |
|         - |  9302 | `			SyToken *pExprEnd;` |
|        63 |  9303 | `			SyToken *pSavedEnd = 0;` |
|         - |  9304 | `			SySet sBody;` |
|        63 |  9305 | `			int bParentCall = 0;` |
|        63 |  9306 | `			pGen->pIn++; /* Jump '=>' */` |
|        63 |  9307 | `			pBodyStart = pGen->pIn;` |
|         - |  9308 | `			/* Delimit the expression (first top-level ';', or a closer that` |
|         - |  9309 | `			 * would end the enclosing hook list) and rewrite any` |
|         - |  9310 | ``			 * `parent::$x::get()` calls into the parent's synthesized hook`` |
|         - |  9311 | `			 * method on a token copy. */` |
|         - |  9312 | `			{` |
|        63 |  9313 | `				sxi32 iNest = 0;` |
|        63 |  9314 | `				pExprEnd = pBodyStart;` |
|       355 |  9315 | `				while( pExprEnd < pGen->pEnd ){` |
|       355 |  9316 | `					if( pExprEnd->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|         9 |  9317 | `						iNest++;` |
|       351 |  9318 | `					}else if( pExprEnd->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|         9 |  9319 | `						if( iNest <= 0 ){` |
|       ! 0 |  9320 | `							break;` |
|         - |  9321 | `						}` |
|         9 |  9322 | `						iNest--;` |
|       343 |  9323 | `					}else if( iNest <= 0 && (pExprEnd->nType & PH7_TK_SEMI) ){` |
|        63 |  9324 | `						break;` |
|         - |  9325 | `					}` |
|       293 |  9326 | `					pExprEnd++;` |
|         1 |  9327 | `				}` |
|         - |  9328 | `			}` |
|         - |  9329 | `			{` |
|         - |  9330 | `				SyToken *pScan;` |
|       335 |  9331 | `				for( pScan = pBodyStart ; pScan < pExprEnd ; pScan++ ){` |
|       275 |  9332 | `					if( GenStateIsParentHookCallAt(pScan,pExprEnd) ){` |
|         3 |  9333 | `						bParentCall = 1;` |
|         3 |  9334 | `						break;` |
|         - |  9335 | `					}` |
|       137 |  9336 | `				}` |
|         - |  9337 | `			}` |
|        63 |  9338 | `			if( bParentCall ){` |
|         3 |  9339 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|         3 |  9340 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,pExprEnd);` |
|         3 |  9341 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9342 | `					SySetRelease(&sBody);` |
|       ! 0 |  9343 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9344 | `					return SXERR_ABORT;` |
|         - |  9345 | `				}` |
|         3 |  9346 | `				pSavedEnd = pGen->pEnd;` |
|         3 |  9347 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|         3 |  9348 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|         1 |  9349 | `			}` |
|        94 |  9350 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|        62 |  9351 | `				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);` |
|        63 |  9352 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9353 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");` |
|       ! 0 |  9354 | `				return SXERR_ABORT;` |
|         - |  9355 | `			}` |
|        63 |  9356 | `			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        63 |  9357 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|        63 |  9358 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|        63 |  9359 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        63 |  9360 | `			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        63 |  9361 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        63 |  9362 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        63 |  9363 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        63 |  9364 | `			if( bParentCall ){` |
|         3 |  9365 | `				pGen->pIn = pExprEnd; /* land on the original ';' */` |
|         3 |  9366 | `				pGen->pEnd = pSavedEnd;` |
|         3 |  9367 | `				SySetRelease(&sBody);` |
|         1 |  9368 | `			}` |
|        63 |  9369 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9370 | `				return SXERR_ABORT;` |
|         - |  9371 | `			}` |
|        63 |  9372 | `			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;` |
|        63 |  9373 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|        37 |  9374 | `				bRefsSelf = 1;` |
|        18 |  9375 | `			}` |
|        63 |  9376 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        63 |  9377 | `				pGen->pIn++; /* Jump ';' */` |
|        31 |  9378 | `			}` |
|        63 |  9379 | `			if( !bGet ){` |
|         - |  9380 | ``				/* `set => expr` assigns the expression to the backing store:`` |
|         - |  9381 | `				 * the dispatcher consumes the implicit return value — which` |
|         - |  9382 | `				 * also makes the property BACKED (php: the shorthand is sugar` |
|         - |  9383 | ``				 * for `$this->NAME = expr`). */`` |
|         3 |  9384 | `				pMeth->sFunc.iFlags \|= VM_FUNC_HOOK_SET_EXPR;` |
|         3 |  9385 | `				bRefsSelf = 1;` |
|         1 |  9386 | `			}` |
|        32 |  9387 | `		}else{` |
|       ! 0 |  9388 | `			goto HookSyntax;` |
|         - |  9389 | `		}` |
|       131 |  9390 | `		rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|       131 |  9391 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  9392 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9393 | `			return SXERR_ABORT;` |
|         - |  9394 | `		}` |
|       131 |  9395 | `		pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|         1 |  9396 | `	}` |
|        95 |  9397 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|       ! 0 |  9398 | `		goto HookSyntax;` |
|         - |  9399 | `	}` |
|        95 |  9400 | `	pGen->pIn++; /* Jump '}' */` |
|        95 |  9401 | `	if( !bRefsSelf ){` |
|         - |  9402 | ``		/* php 8.4 virtual-vs-backed: no hook body referenced `$this->NAME`, so`` |
|         - |  9403 | `		 * this property is VIRTUAL — php gives it no backing store and forbids` |
|         - |  9404 | `		 * a default value (compile fatal, php's exact wording). */` |
|        41 |  9405 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_HOOK_VIRTUAL;` |
|        41 |  9406 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|       ! 0 |  9407 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9408 | `				"Cannot specify default value for virtual hooked property %z::$%z",` |
|       ! 0 |  9409 | `				&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9410 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9411 | `				return SXERR_ABORT;` |
|         - |  9412 | `			}` |
|       ! 0 |  9413 | `			return SXERR_CORRUPT;` |
|         - |  9414 | `		}` |
|        20 |  9415 | `	}` |
|        95 |  9416 | `	return SXRET_OK;` |
|       ! 0 |  9417 | `HookSyntax:` |
|       ! 0 |  9418 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9419 | `		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",` |
|       ! 0 |  9420 | `		&pClass->sName,&pAttr->sName);` |
|       ! 0 |  9421 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 |  9422 | `		return SXERR_ABORT;` |
|         - |  9423 | `	}` |
|       ! 0 |  9424 | `	return SXERR_CORRUPT;` |
|        48 |  9425 | `}` |
|         - |  9426 | `/*` |
|         - |  9427 | ` * Compile an object interface.` |
|         - |  9428 | ` *  According to the PHP language reference manual` |
|         - |  9429 | ` *   Object Interfaces:` |
|         - |  9430 | ` *   Object interfaces allow you to create code which specifies which methods` |
|         - |  9431 | ` *   a class must implement, without having to define how these methods are handled.` |
|         - |  9432 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|         - |  9433 | ` *   class, but without any of the methods having their contents defined.` |
|         - |  9434 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|         - |  9435 | ` */` |
|     70988 |  9436 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|         5 |  9437 | `{` |
|     70993 |  9438 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - |  9439 | `	ph7_class *pClass,*pBase;` |
|         - |  9440 | `	SyToken *pEnd,*pTmp;` |
|         - |  9441 | `	SyString *pName;` |
|         - |  9442 | `	sxi32 nKwrd;` |
|         - |  9443 | `	sxi32 rc;` |
|         - |  9444 | `	/* Jump the 'interface' keyword */` |
|     70993 |  9445 | `	pGen->pIn++;` |
|         - |  9446 | `	/* Extract interface name */` |
|     70993 |  9447 | `	pName = &pGen->pIn->sData;` |
|         - |  9448 | `	/* Advance the stream cursor */` |
|     70993 |  9449 | `	pGen->pIn++;` |
|         - |  9450 | `	/* Build FQN and obtain a raw class */ {` |
|         - |  9451 | `		SyBlob sFQN;` |
|         - |  9452 | `		SyString sFQNStr;` |
|     70993 |  9453 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     70993 |  9454 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     70993 |  9455 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|     70993 |  9456 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     70993 |  9457 | `		SyBlobRelease(&sFQN);` |
|         - |  9458 | `	}` |
|     70993 |  9459 | `	if( pClass == 0 ){` |
|       ! 0 |  9460 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9461 | `		return SXERR_ABORT;` |
|         - |  9462 | `	}` |
|     70993 |  9463 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     70993 |  9464 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 |  9465 | `		return SXERR_ABORT;` |
|         - |  9466 | `	}` |
|         - |  9467 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|     70993 |  9468 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|         - |  9469 | `	/* Assume no base class is given */` |
|     70993 |  9470 | `	pBase = 0;` |
|     70993 |  9471 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     27581 |  9472 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     27581 |  9473 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|         - |  9474 | `			SyBlob sResolved;` |
|         - |  9475 | `			SyString sBaseName;` |
|         - |  9476 | `			sxu32 nRefLine;` |
|         - |  9477 | `			/* Extract base interface */` |
|     27581 |  9478 | `			pGen->pIn++;` |
|     27581 |  9479 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|     27581 |  9480 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     27581 |  9481 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 |  9482 | `				SyBlobRelease(&sResolved);` |
|       ! 0 |  9483 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - |  9484 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|       ! 0 |  9485 | `					pName);` |
|       ! 0 |  9486 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9487 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9488 | `					return SXERR_ABORT;` |
|         - |  9489 | `				}` |
|       ! 0 |  9490 | `				return SXRET_OK;` |
|         - |  9491 | `			}` |
|     41369 |  9492 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|     27576 |  9493 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     27581 |  9494 | `			SyStringInitFromBuf(&sBaseName,` |
|         - |  9495 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - |  9496 | `			/* Only interfaces is allowed */` |
|     27581 |  9497 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 |  9498 | `				pBase = pBase->pNextName;` |
|       ! 0 |  9499 | `			}` |
|     27581 |  9500 | `			if( pBase == 0 ){` |
|       ! 0 |  9501 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - |  9502 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|       ! 0 |  9503 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9504 | `					SyBlobRelease(&sResolved);` |
|       ! 0 |  9505 | `					return SXERR_ABORT;` |
|         - |  9506 | `				}` |
|       ! 0 |  9507 | `			}` |
|     27581 |  9508 | `			SyBlobRelease(&sResolved);` |
|     13788 |  9509 | `		}` |
|     13788 |  9510 | `	}` |
|     70993 |  9511 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - |  9512 | `		/* Syntax error */` |
|       ! 0 |  9513 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|       ! 0 |  9514 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9515 | `		if( rc == SXERR_ABORT ){` |
|         - |  9516 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9517 | `			return SXERR_ABORT;` |
|         - |  9518 | `		}` |
|       ! 0 |  9519 | `		return SXRET_OK;` |
|         - |  9520 | `	}` |
|     70993 |  9521 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     70993 |  9522 | `	pEnd = 0; /* cc warning */` |
|         - |  9523 | `	/* Delimit the interface body */` |
|     70993 |  9524 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|     70993 |  9525 | `	if( pEnd >= pGen->pEnd ){` |
|         - |  9526 | `		/* Syntax error */` |
|       ! 0 |  9527 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|       ! 0 |  9528 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 |  9529 | `		if( rc == SXERR_ABORT ){` |
|         - |  9530 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 |  9531 | `			return SXERR_ABORT;` |
|         - |  9532 | `		}` |
|       ! 0 |  9533 | `		return SXRET_OK;` |
|         - |  9534 | `	}` |
|         - |  9535 | `	/* The delimiter token is the interface body's closing brace */` |
|     70993 |  9536 | `	pClass->nEndLine = pEnd->nLine;` |
|         - |  9537 | `	/* Swap token stream */` |
|     70993 |  9538 | `	pTmp = pGen->pEnd;` |
|     70993 |  9539 | `	pGen->pEnd = pEnd;` |
|         - |  9540 | `	/* Start the parse process` |
|         - |  9541 | `	 * Note (According to the PHP reference manual):` |
|         - |  9542 | `	 *  Only constants and function signatures(without body) are allowed.` |
|         - |  9543 | `	 *  Only 'public' visibility is allowed.` |
|         - |  9544 | `	 */` |
|    130035 |  9545 | `	for(;;){` |
|         - |  9546 | `		/* Jump leading/trailing semi-colons */` |
|    449161 |  9547 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    189087 |  9548 | `			pGen->pIn++;` |
|         5 |  9549 | `		}` |
|    260079 |  9550 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - |  9551 | `			/* End of interface body */` |
|     70989 |  9552 | `			break;` |
|         - |  9553 | `		}` |
|         - |  9554 | `		/* Bind a directly-preceding docblock to this member */` |
|    189095 |  9555 | `		GenStateSetPendingDoc(&(*pGen));` |
|    189095 |  9556 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 |  9557 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - |  9558 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|       ! 0 |  9559 | `				&pGen->pIn->sData,pName);` |
|       ! 0 |  9560 | `			if( rc == SXERR_ABORT ){` |
|         - |  9561 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  9562 | `				return SXERR_ABORT;` |
|         - |  9563 | `			}` |
|       ! 0 |  9564 | `			goto done;` |
|         - |  9565 | `		}` |
|         - |  9566 | `		/* Extract the current keyword */` |
|    189095 |  9567 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    189095 |  9568 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - |  9569 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|         - |  9570 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|         3 |  9571 | `			const char *zKind = "member";` |
|         3 |  9572 | `			SyString *pMemberName = 0;` |
|         3 |  9573 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|         3 |  9574 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|         3 |  9575 | `				if( nNext == PH7_TKWRD_CONST ){` |
|         3 |  9576 | `					zKind = "constant";` |
|         3 |  9577 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|         3 |  9578 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|         2 |  9579 | `					}` |
|         1 |  9580 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|       ! 0 |  9581 | `					zKind = "method";` |
|       ! 0 |  9582 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       ! 0 |  9583 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       ! 0 |  9584 | `					}` |
|       ! 0 |  9585 | `				}` |
|         1 |  9586 | `			}` |
|         3 |  9587 | `			if( pMemberName ){` |
|         4 |  9588 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         1 |  9589 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|         2 |  9590 | `			}else{` |
|       ! 0 |  9591 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9592 | `					"Access type for interface %s must be public",zKind);` |
|         - |  9593 | `			}` |
|         3 |  9594 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  9595 | `				return SXERR_ABORT;` |
|         - |  9596 | `			}` |
|         3 |  9597 | `			goto done;` |
|         - |  9598 | `		}` |
|    189093 |  9599 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|       ! 0 |  9600 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9601 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 |  9602 | `			if( rc == SXERR_ABORT ){` |
|         - |  9603 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 |  9604 | `				return SXERR_ABORT;` |
|         - |  9605 | `			}` |
|       ! 0 |  9606 | `			goto done;` |
|         - |  9607 | `		}` |
|    189093 |  9608 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|         - |  9609 | `			/* Advance the stream cursor */` |
|    133943 |  9610 | `			pGen->pIn++;` |
|    133938 |  9611 | `			if( pGen->pIn < pGen->pEnd` |
|    133943 |  9612 | `			 && ((pGen->pIn->nType & PH7_TK_DOLLAR) != 0` |
|    133938 |  9613 | `			  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         - |  9614 | ``				/* PHP 8.4: `public [?T] $x { get; set; }` — a hooked-property`` |
|         - |  9615 | `				 * requirement. The attribute compiler + hook parser handle it` |
|         - |  9616 | `				 * (bare hooks are implicitly abstract inside an interface; a` |
|         - |  9617 | `				 * property without hooks is ITS "Interfaces may only include` |
|         - |  9618 | `				 * hooked properties" error). */` |
|       ! 0 |  9619 | `				rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 |  9620 | `					PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 |  9621 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  9622 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9623 | `						return SXERR_ABORT;` |
|         - |  9624 | `					}` |
|       ! 0 |  9625 | `					goto done;` |
|         - |  9626 | `				}` |
|       ! 0 |  9627 | `				continue;` |
|         - |  9628 | `			}` |
|    133943 |  9629 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|         - |  9630 | `				/* A type NAME (a plain identifier, e.g. a class type) followed by` |
|         - |  9631 | `				 * '$' also opens a hooked-property requirement. */` |
|       ! 0 |  9632 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) != 0` |
|       ! 0 |  9633 | `				 && (pGen->pIn + 1) < pGen->pEnd` |
|       ! 0 |  9634 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|       ! 0 |  9635 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|       ! 0 |  9636 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|       ! 0 |  9637 | `					if( rc != SXRET_OK ){` |
|       ! 0 |  9638 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  9639 | `							return SXERR_ABORT;` |
|         - |  9640 | `						}` |
|       ! 0 |  9641 | `						goto done;` |
|         - |  9642 | `					}` |
|       ! 0 |  9643 | `					continue;` |
|         - |  9644 | `				}` |
|       ! 0 |  9645 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9646 | `					"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 |  9647 | `				if( rc == SXERR_ABORT ){` |
|         - |  9648 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9649 | `					return SXERR_ABORT;` |
|         - |  9650 | `				}` |
|       ! 0 |  9651 | `				goto done;` |
|         - |  9652 | `			}` |
|    133943 |  9653 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    133943 |  9654 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|         - |  9655 | `				/* A type KEYWORD (int/string/bool/…) followed by '$' opens a` |
|         - |  9656 | `				 * hooked-property requirement (PHP 8.4). */` |
|         4 |  9657 | `				if( (pGen->pIn + 1) < pGen->pEnd` |
|         5 |  9658 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|         7 |  9659 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|         2 |  9660 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|         5 |  9661 | `					if( rc != SXRET_OK ){` |
|       ! 0 |  9662 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 |  9663 | `							return SXERR_ABORT;` |
|         - |  9664 | `						}` |
|       ! 0 |  9665 | `						goto done;` |
|         - |  9666 | `					}` |
|         5 |  9667 | `					continue;` |
|         - |  9668 | `				}` |
|       ! 0 |  9669 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9670 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|       ! 0 |  9671 | `				if( rc == SXERR_ABORT ){` |
|         - |  9672 | `					/* Error count limit reached,abort immediately */` |
|       ! 0 |  9673 | `					return SXERR_ABORT;` |
|         - |  9674 | `				}` |
|       ! 0 |  9675 | `				goto done;` |
|         - |  9676 | `			}` |
|     66967 |  9677 | `		}` |
|    189089 |  9678 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|         - |  9679 | `			/* Parse constant */` |
|     55151 |  9680 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|     55151 |  9681 | `			if( rc != SXRET_OK ){` |
|         3 |  9682 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9683 | `					return SXERR_ABORT;` |
|         - |  9684 | `				}` |
|         3 |  9685 | `				goto done;` |
|         - |  9686 | `			}` |
|     27577 |  9687 | `		}else{` |
|    133943 |  9688 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|    133943 |  9689 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - |  9690 | `				/* Static method,record that */` |
|     11819 |  9691 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|         - |  9692 | `				/* Advance the stream cursor */` |
|     11819 |  9693 | `				pGen->pIn++;` |
|     11814 |  9694 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     11819 |  9695 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 |  9696 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 |  9697 | `							"Expecting method signature inside interface '%z'",pName);` |
|       ! 0 |  9698 | `						if( rc == SXERR_ABORT ){` |
|         - |  9699 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 |  9700 | `							return SXERR_ABORT;` |
|         - |  9701 | `						}` |
|       ! 0 |  9702 | `						goto done;` |
|         - |  9703 | `				}` |
|      5907 |  9704 | `			}` |
|         - |  9705 | `			/* Process method signature (no body for interface methods) */` |
|    133943 |  9706 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|    133943 |  9707 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  9708 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9709 | `					return SXERR_ABORT;` |
|         - |  9710 | `				}` |
|       ! 0 |  9711 | `				goto done;` |
|         - |  9712 | `			}` |
|         - |  9713 | `		}` |
|         5 |  9714 | `	}` |
|         - |  9715 | `	/* Install the interface */` |
|     70989 |  9716 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     70989 |  9717 | `	if( rc == SXRET_OK && pBase ){` |
|         - |  9718 | `		/* Inherit from the base interface */` |
|     27581 |  9719 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|     13788 |  9720 | `	}` |
|     70989 |  9721 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  9722 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 |  9723 | `		return SXERR_ABORT;` |
|         - |  9724 | `	}` |
|     35492 |  9725 | `done:` |
|         - |  9726 | `	/* Point beyond the interface body */` |
|     70993 |  9727 | `	pGen->pIn  = &pEnd[1];` |
|     70993 |  9728 | `	pGen->pEnd = pTmp;` |
|     70993 |  9729 | `	return PH7_OK;` |
|     35499 |  9730 | `}` |
|         - |  9731 | `/*` |
|         - |  9732 | ` * Compile a user-defined class.` |
|         - |  9733 | ` * According to the PHP language reference manual` |
|         - |  9734 | ` *  class` |
|         - |  9735 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|         - |  9736 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|         - |  9737 | ` *  of the properties and methods belonging to the class.` |
|         - |  9738 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|         - |  9739 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|         - |  9740 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|         - |  9741 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|         - |  9742 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|         - |  9743 | ` *  (called "methods").` |
|         - |  9744 | ` */` |
|         - |  9745 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|         - |  9746 | `typedef struct TraitUseEntry TraitUseEntry;` |
|         - |  9747 | `struct TraitUseEntry {` |
|         - |  9748 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|         - |  9749 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|         - |  9750 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|         - |  9751 | `};` |
|         - |  9752 | `/*` |
|         - |  9753 | ` * Validate that methods implementing interface contracts have compatible` |
|         - |  9754 | ` * signatures: public visibility and at least as many parameters as declared.` |
|         - |  9755 | ` */` |
|    364022 |  9756 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 |  9757 | `{` |
|         - |  9758 | `	ph7_class **apIface;` |
|         - |  9759 | `	sxu32 nIface,i;` |
|         - |  9760 | `	sxi32 rc;` |
|    364027 |  9761 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|       ! 0 |  9762 | `		return SXRET_OK;` |
|         - |  9763 | `	}` |
|    364027 |  9764 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    364027 |  9765 | `	nIface = SySetUsed(&pClass->aInterface);` |
|    730593 |  9766 | `	for(i = 0; i < nIface; i++){` |
|    366571 |  9767 | `		ph7_class *pIface = apIface[i];` |
|         - |  9768 | `		SyHashEntry *pEntry;` |
|    366571 |  9769 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   1056301 |  9770 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|    689735 |  9771 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|         - |  9772 | `			ph7_class_method *pImplMeth;` |
|    689735 |  9773 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|         - |  9774 | `			/* Find the implementing method in the class */` |
|    689735 |  9775 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|    689735 |  9776 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        23 |  9777 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|         - |  9778 | `			}` |
|         - |  9779 | `			/* Check visibility: interface methods must be implemented as public */` |
|    689717 |  9780 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|         4 |  9781 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - |  9782 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|         1 |  9783 | `					&pClass->sName,pMName,&pIface->sName);` |
|         3 |  9784 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  9785 | `					return SXERR_ABORT;` |
|         - |  9786 | `				}` |
|         1 |  9787 | `			}` |
|         - |  9788 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|         - |  9789 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|         - |  9790 | `			 */` |
|         - |  9791 | `			{` |
|    689717 |  9792 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|    689717 |  9793 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|    689717 |  9794 | `				int sigError = 0;` |
|    689717 |  9795 | `				if( nImplArgs < nIfaceArgs ){` |
|         3 |  9796 | `					sigError = 1;` |
|    689716 |  9797 | `				}else if( nImplArgs > nIfaceArgs ){` |
|         - |  9798 | `					/* Extra parameters must all have default values */` |
|      3947 |  9799 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|         - |  9800 | `					sxu32 k;` |
|      7887 |  9801 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|      3947 |  9802 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|         3 |  9803 | `							sigError = 1;` |
|         3 |  9804 | `							break;` |
|         - |  9805 | `						}` |
|      1975 |  9806 | `					}` |
|      1971 |  9807 | `				}` |
|    689717 |  9808 | `				if( sigError ){` |
|         - |  9809 | `					SyBlob sImplSig, sIfaceSig;` |
|         - |  9810 | `					ph7_vm_func_arg *aArgs;` |
|         - |  9811 | `					sxu32 j;` |
|         6 |  9812 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|         6 |  9813 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|         - |  9814 | `					/* Build implementing method signature */` |
|         6 |  9815 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|        12 |  9816 | `					for(j = 0; j < nImplArgs; j++){` |
|         8 |  9817 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|         8 |  9818 | `						SyBlobAppend(&sImplSig,"$",1);` |
|         8 |  9819 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 |  9820 | `					}` |
|         - |  9821 | `					/* Build interface method signature */` |
|         6 |  9822 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|        12 |  9823 | `					for(j = 0; j < nIfaceArgs; j++){` |
|         8 |  9824 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|         8 |  9825 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|         8 |  9826 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|         5 |  9827 | `					}` |
|         8 |  9828 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|         - |  9829 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|         2 |  9830 | `						&pClass->sName,pMName,` |
|         4 |  9831 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|         2 |  9832 | `						&pIface->sName,pMName,` |
|         4 |  9833 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|         6 |  9834 | `					SyBlobRelease(&sImplSig);` |
|         6 |  9835 | `					SyBlobRelease(&sIfaceSig);` |
|         6 |  9836 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 |  9837 | `						return SXERR_ABORT;` |
|         - |  9838 | `					}` |
|         2 |  9839 | `				}` |
|         - |  9840 | `			}` |
|         5 |  9841 | `		}` |
|    183288 |  9842 | `	}` |
|    364027 |  9843 | `	return SXRET_OK;` |
|    182016 |  9844 | `}` |
|         - |  9845 | `/*` |
|         - |  9846 | ` * An abstract property-hook stub (__phl_hook_{get,set}_NAME) is satisfied by` |
|         - |  9847 | ` * the class declaring a PLAIN (non-abstract, non-hooked) property NAME: php` |
|         - |  9848 | `` * lets a plain property implement `{ get; set; }` requirements — its raw`` |
|         - |  9849 | ` * read/write IS the default get/set. A concrete hook override replaced the` |
|         - |  9850 | ` * stub in hMethod already, so a surviving stub next to a HOOKED property` |
|         - |  9851 | ` * means that specific hook is still missing.` |
|         - |  9852 | ` */` |
|        38 |  9853 | `static int GenStateAbstractHookSatisfied(ph7_class *pClass,const SyString *pMName)` |
|         5 |  9854 | `{` |
|         - |  9855 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|         - |  9856 | `	ph7_class_attr *pProp;` |
|        38 |  9857 | `	if( pMName->nByte <= nPfx` |
|        27 |  9858 | `	 \|\| (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) != 0` |
|         4 |  9859 | `	  && SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) != 0) ){` |
|        36 |  9860 | `		return 0; /* not a hook stub */` |
|         - |  9861 | `	}` |
|         7 |  9862 | `	pProp = PH7_ClassExtractAttribute(pClass,&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|         7 |  9863 | `	return pProp != 0` |
|         6 |  9864 | `		&& (pProp->iFlags & (PH7_CLASS_ATTR_ABSTRACT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|         3 |  9865 | `			\|PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) == 0;` |
|        24 |  9866 | `}` |
|         - |  9867 | `/*` |
|         - |  9868 | ` * Append an abstract member's display name to the message blob, translating a` |
|         - |  9869 | `` * property-hook stub (__phl_hook_get_x) to php's `$x::get` form.`` |
|         - |  9870 | ` */` |
|        16 |  9871 | `static void GenStateAppendAbstractMemberName(SyBlob *pMsg,const SyString *pMName)` |
|         4 |  9872 | `{` |
|         - |  9873 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|        16 |  9874 | `	if( pMName->nByte > nPfx` |
|        12 |  9875 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|       ! 0 |  9876 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0) ){` |
|       ! 0 |  9877 | `		SyBlobAppend(pMsg,"$",1);` |
|       ! 0 |  9878 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|       ! 0 |  9879 | `		SyBlobAppend(pMsg,"::",2);` |
|       ! 0 |  9880 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[sizeof("__phl_hook_")-1],3);` |
|       ! 0 |  9881 | `		return;` |
|         - |  9882 | `	}` |
|        20 |  9883 | `	SyBlobAppend(pMsg,(const void *)pMName->zString,pMName->nByte);` |
|        12 |  9884 | `}` |
|         - |  9885 | `/*` |
|         - |  9886 | ` * Check that a concrete class has no remaining abstract methods.` |
|         - |  9887 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|         - |  9888 | ` */` |
|    364022 |  9889 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 |  9890 | `{` |
|         - |  9891 | `	ph7_class_method *pMeth;` |
|         - |  9892 | `	SyHashEntry *pEntry;` |
|         - |  9893 | `	sxu32 nAbstract;` |
|         - |  9894 | `	SyBlob sMsg;` |
|         - |  9895 | `	sxi32 rc;` |
|         - |  9896 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|    364027 |  9897 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     15801 |  9898 | `		return SXRET_OK;` |
|         - |  9899 | `	}` |
|         - |  9900 | `	/* Count abstract methods */` |
|    348231 |  9901 | `	nAbstract = 0;` |
|    348231 |  9902 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   5140156 |  9903 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   4617817 |  9904 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   4617817 |  9905 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        27 |  9906 | `			if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|         7 |  9907 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - |  9908 | `			}` |
|        20 |  9909 | `			nAbstract++;` |
|         8 |  9910 | `		}` |
|         5 |  9911 | `	}` |
|    348231 |  9912 | `	if( nAbstract == 0 ){` |
|    348217 |  9913 | `		return SXRET_OK;` |
|         - |  9914 | `	}` |
|         - |  9915 | `	/* Build the error message listing all abstract methods with origins */` |
|        18 |  9916 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|        18 |  9917 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|         - |  9918 | `		"be declared abstract or implement the remaining method%s (",` |
|         7 |  9919 | `		&pClass->sName,nAbstract,` |
|         7 |  9920 | `		(nAbstract > 1 ? "s" : ""),` |
|         7 |  9921 | `		(nAbstract > 1 ? "s" : ""));` |
|         - |  9922 | `	/* Second pass: list methods with origins */` |
|         - |  9923 | `	{` |
|        18 |  9924 | `		sxu32 nListed = 0;` |
|        18 |  9925 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|        36 |  9926 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|        22 |  9927 | `			ph7_class *pOrigin = 0;` |
|         - |  9928 | `			SyString *pMName;` |
|        22 |  9929 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        22 |  9930 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|         3 |  9931 | `				continue;` |
|         - |  9932 | `			}` |
|        20 |  9933 | `			pMName = &pMeth->sFunc.sName;` |
|        20 |  9934 | `			if( GenStateAbstractHookSatisfied(pClass,pMName) ){` |
|       ! 0 |  9935 | `				continue; /* hook requirement met by a plain property (php) */` |
|         - |  9936 | `			}` |
|        20 |  9937 | `			if( nListed > 0 ){` |
|         3 |  9938 | `				SyBlobAppend(&sMsg,", ",2);` |
|         1 |  9939 | `			}` |
|         - |  9940 | `			/* Find the origin of this abstract method.` |
|         - |  9941 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|         - |  9942 | `			 * inheritance chains) take precedence for interface-declared` |
|         - |  9943 | `			 * methods. Abstract class methods only win when the class` |
|         - |  9944 | `			 * itself declared the abstract method (not inherited from` |
|         - |  9945 | `			 * an interface). Trait methods are adopted into the using` |
|         - |  9946 | `			 * class's namespace.` |
|         - |  9947 | `			 */` |
|         - |  9948 | `			{` |
|         - |  9949 | `				ph7_class **apIface;` |
|         - |  9950 | `				ph7_class **apTrait;` |
|         - |  9951 | `				ph7_class *pWalk;` |
|         - |  9952 | `				sxu32 i;` |
|         - |  9953 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|         - |  9954 | `				 * (one that was written in the class body, not inherited from an` |
|         - |  9955 | `				 * interface). PHP attributes origin to the declaring class.` |
|         - |  9956 | `				 */` |
|        20 |  9957 | `				if( pClass->pBase ){` |
|        11 |  9958 | `					pWalk = pClass->pBase;` |
|        19 |  9959 | `					while( pWalk ){` |
|         - |  9960 | `						ph7_class_method *pParentMeth;` |
|        13 |  9961 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|        13 |  9962 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|         - |  9963 | `							/* Exclude methods that came from an interface anywhere` |
|         - |  9964 | `							 * in this class's ancestor chain.` |
|         - |  9965 | `							 */` |
|        13 |  9966 | `							int fromIface = 0;` |
|        13 |  9967 | `							ph7_class *pAnc = pWalk;` |
|        17 |  9968 | `							while( pAnc ){` |
|         - |  9969 | `								ph7_class **apPI;` |
|         - |  9970 | `								sxu32 j;` |
|        15 |  9971 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|        15 |  9972 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|        10 |  9973 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|        10 |  9974 | `										fromIface = 1;` |
|        10 |  9975 | `										break;` |
|         - |  9976 | `									}` |
|       ! 0 |  9977 | `								}` |
|        15 |  9978 | `								if( fromIface ) break;` |
|         6 |  9979 | `								pAnc = pAnc->pBase;` |
|         2 |  9980 | `							}` |
|        13 |  9981 | `							if( !fromIface ){` |
|         3 |  9982 | `								pOrigin = pWalk;` |
|         3 |  9983 | `								break;` |
|         - |  9984 | `							}` |
|         4 |  9985 | `						}` |
|        10 |  9986 | `						pWalk = pWalk->pBase;` |
|         2 |  9987 | `					}` |
|         4 |  9988 | `				}` |
|         - |  9989 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|         - |  9990 | `				 * each interface's own parent chain for the deepest origin.` |
|         - |  9991 | `				 */` |
|        20 |  9992 | `				if( !pOrigin ){` |
|        18 |  9993 | `					pWalk = pClass;` |
|        40 |  9994 | `					while( pWalk && !pOrigin ){` |
|        26 |  9995 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|        26 |  9996 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|        16 |  9997 | `							ph7_class *pIface = apIface[i];` |
|        16 |  9998 | `							ph7_class *pDeepest = 0;` |
|        28 |  9999 | `							while( pIface ){` |
|        16 | 10000 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|        16 | 10001 | `									pDeepest = pIface;` |
|         6 | 10002 | `								}` |
|        16 | 10003 | `								pIface = pIface->pBase;` |
|         4 | 10004 | `							}` |
|        16 | 10005 | `							if( pDeepest ){` |
|        16 | 10006 | `								pOrigin = pDeepest;` |
|        16 | 10007 | `								break;` |
|         - | 10008 | `							}` |
|       ! 0 | 10009 | `						}` |
|        26 | 10010 | `						pWalk = pWalk->pBase;` |
|         4 | 10011 | `					}` |
|         7 | 10012 | `				}` |
|         - | 10013 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|        20 | 10014 | `				if( !pOrigin ){` |
|         3 | 10015 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|         3 | 10016 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|         3 | 10017 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|         3 | 10018 | `							pOrigin = pClass;` |
|         3 | 10019 | `							break;` |
|         - | 10020 | `						}` |
|       ! 0 | 10021 | `					}` |
|         1 | 10022 | `				}` |
|         - | 10023 | `			}` |
|        20 | 10024 | `			if( pOrigin ){` |
|        20 | 10025 | `				SyBlobFormat(&sMsg,"%z::",&pOrigin->sName);` |
|        12 | 10026 | `			}else{` |
|         - | 10027 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|       ! 0 | 10028 | `				SyBlobFormat(&sMsg,"%z::",&pClass->sName);` |
|         - | 10029 | `			}` |
|        20 | 10030 | `			GenStateAppendAbstractMemberName(&sMsg,pMName);` |
|        20 | 10031 | `			nListed++;` |
|         4 | 10032 | `		}` |
|         - | 10033 | `	}` |
|        18 | 10034 | `	SyBlobAppend(&sMsg,")",1);` |
|        25 | 10035 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|        14 | 10036 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|        18 | 10037 | `	SyBlobRelease(&sMsg);` |
|        18 | 10038 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 10039 | `		return SXERR_ABORT;` |
|         - | 10040 | `	}` |
|        18 | 10041 | `	return SXRET_OK;` |
|    182016 | 10042 | `}` |
|         - | 10043 | `/*` |
|         - | 10044 | ` * Parse a class/interface name reference from the current token stream.` |
|         - | 10045 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|         - | 10046 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|         - | 10047 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|         - | 10048 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|         - | 10049 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|         - | 10050 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|         - | 10051 | ` */` |
|    411552 | 10052 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|         5 | 10053 | `{` |
|    411557 | 10054 | `	int isAbsolute = 0;` |
|    411557 | 10055 | `	SyToken *pStart = pGen->pIn;` |
|         - | 10056 | `	SyBlob sName;` |
|    411557 | 10057 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      4527 | 10058 | `		isAbsolute = 1;` |
|      4527 | 10059 | `		pGen->pIn++;` |
|      2261 | 10060 | `	}` |
|    411557 | 10061 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         8 | 10062 | `		pGen->pIn = pStart;` |
|         8 | 10063 | `		return SXERR_INVALID;` |
|         - | 10064 | `	}` |
|    411551 | 10065 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|    411551 | 10066 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|    411551 | 10067 | `	pGen->pIn++;` |
|    617340 | 10068 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|    205799 | 10069 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        16 | 10070 | `		SyBlobAppend(&sName,"\\",1);` |
|        16 | 10071 | `		pGen->pIn++;` |
|        16 | 10072 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        16 | 10073 | `		pGen->pIn++;` |
|         2 | 10074 | `	}` |
|    411551 | 10075 | `	if( isAbsolute ){` |
|      4525 | 10076 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      2265 | 10077 | `	}else{` |
|         - | 10078 | `		SyString sRaw;` |
|    407031 | 10079 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|    407031 | 10080 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|         - | 10081 | `	}` |
|    411551 | 10082 | `	SyBlobRelease(&sName);` |
|    411551 | 10083 | `	return SXRET_OK;` |
|    205781 | 10084 | `}` |
|         - | 10085 | `/*` |
|         - | 10086 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|         - | 10087 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|         - | 10088 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|         - | 10089 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|         - | 10090 | ` * either direction cannot run unbounded.` |
|         - | 10091 | ` */` |
|         - | 10092 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    169534 | 10093 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|         5 | 10094 | `{` |
|         - | 10095 | `	ph7_class **apParent;` |
|         - | 10096 | `	sxu32 n;` |
|    441499 | 10097 | `	while( pInterface ){` |
|    279851 | 10098 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|       ! 0 | 10099 | `			return FALSE;` |
|         - | 10100 | `		}` |
|    315312 | 10101 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|     70922 | 10102 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|      7891 | 10103 | `			return TRUE;` |
|         - | 10104 | `		}` |
|    271965 | 10105 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    271965 | 10106 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|       ! 0 | 10107 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|       ! 0 | 10108 | `				return TRUE;` |
|         - | 10109 | `			}` |
|       ! 0 | 10110 | `		}` |
|    271965 | 10111 | `		pInterface = pInterface->pBase;` |
|    271965 | 10112 | `		iDepth++;` |
|         5 | 10113 | `	}` |
|    161653 | 10114 | `	return FALSE;` |
|     84772 | 10115 | `}` |
|    169534 | 10116 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|         5 | 10117 | `{` |
|    169539 | 10118 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|         5 | 10119 | `}` |
|         - | 10120 | `/*` |
|         - | 10121 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|         - | 10122 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|         - | 10123 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|         - | 10124 | ` */` |
|      7886 | 10125 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|         5 | 10126 | `{` |
|      7895 | 10127 | `	while( pBase ){` |
|        10 | 10128 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|         2 | 10129 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|         3 | 10130 | `			return TRUE;` |
|         - | 10131 | `		}` |
|        10 | 10132 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|         6 | 10133 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|         3 | 10134 | `			return TRUE;` |
|         - | 10135 | `		}` |
|         5 | 10136 | `		pBase = pBase->pBase;` |
|         1 | 10137 | `	}` |
|      7887 | 10138 | `	return FALSE;` |
|      3948 | 10139 | `}` |
|         - | 10140 | `/*` |
|         - | 10141 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|         - | 10142 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|         - | 10143 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|         - | 10144 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|         - | 10145 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|         - | 10146 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|         - | 10147 | ` * pClass->aEnumCases for cases().` |
|         - | 10148 | ` */` |
|      7918 | 10149 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10150 | `{` |
|      7923 | 10151 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 10152 | `	SySet *pInstrContainer;` |
|         - | 10153 | `	ph7_class_attr *pCase;` |
|         - | 10154 | `	SyString *pName;` |
|         - | 10155 | `	sxi32 rc;` |
|      7923 | 10156 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|      7923 | 10157 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 10158 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10159 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|       ! 0 | 10160 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10161 | `			return SXERR_ABORT;` |
|         - | 10162 | `		}` |
|       ! 0 | 10163 | `		goto Synchronize;` |
|         - | 10164 | `	}` |
|      7923 | 10165 | `	pName = &pGen->pIn->sData;` |
|         - | 10166 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|      7923 | 10167 | `	if( SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte) != 0 ){` |
|       ! 0 | 10168 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10169 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|       ! 0 | 10170 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10171 | `			return SXERR_ABORT;` |
|         - | 10172 | `		}` |
|       ! 0 | 10173 | `		goto Synchronize;` |
|         - | 10174 | `	}` |
|      7923 | 10175 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10176 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|      7923 | 10177 | `	if( pCase == 0 ){` |
|       ! 0 | 10178 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10179 | `		return SXERR_ABORT;` |
|         - | 10180 | `	}` |
|      7923 | 10181 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|      7923 | 10182 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 10183 | `		return SXERR_ABORT;` |
|         - | 10184 | `	}` |
|      7923 | 10185 | `	pGen->pIn++; /* Jump the case name */` |
|      7923 | 10186 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|      7909 | 10187 | `		if( pClass->nEnumBacking == 0 ){` |
|         8 | 10188 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         2 | 10189 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|         6 | 10190 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10191 | `				return SXERR_ABORT;` |
|         - | 10192 | `			}` |
|         6 | 10193 | `			goto Synchronize;` |
|         - | 10194 | `		}` |
|      7905 | 10195 | `		pGen->pIn++; /* Jump the equal sign */` |
|         - | 10196 | `		/* Compile the backing value expression into the case's own container` |
|         - | 10197 | `		 * (same technique as class constants). */` |
|      7905 | 10198 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7905 | 10199 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|      7905 | 10200 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7905 | 10201 | `		if( rc == SXERR_EMPTY ){` |
|       ! 0 | 10202 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10203 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|       ! 0 | 10204 | `		}` |
|      7905 | 10205 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7905 | 10206 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7905 | 10207 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 10208 | `			return SXERR_ABORT;` |
|         - | 10209 | `		}` |
|      3955 | 10210 | `	}else{` |
|        17 | 10211 | `		if( pClass->nEnumBacking != 0 ){` |
|       ! 0 | 10212 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10213 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|       ! 0 | 10214 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10215 | `				return SXERR_ABORT;` |
|         - | 10216 | `			}` |
|       ! 0 | 10217 | `			goto Synchronize;` |
|         - | 10218 | `		}` |
|         - | 10219 | `	}` |
|      7919 | 10220 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|      7919 | 10221 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 10222 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10223 | `		return SXERR_ABORT;` |
|         - | 10224 | `	}` |
|      7919 | 10225 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|      7919 | 10226 | `	return SXRET_OK;` |
|         2 | 10227 | `Synchronize:` |
|         - | 10228 | `	/* Synchronize with the first semi-colon */` |
|        14 | 10229 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|        10 | 10230 | `		pGen->pIn++;` |
|         2 | 10231 | `	}` |
|         6 | 10232 | `	return SXERR_CORRUPT;` |
|      3964 | 10233 | `}` |
|         - | 10234 | `/*` |
|         - | 10235 | ` * Synthesize the enum interface methods (PHP 8.1): cases() for every enum,` |
|         - | 10236 | ` * plus from()/tryFrom() for backed enums. Each is an ordinary public static` |
|         - | 10237 | ` * method whose body forwards to a __phl_enum_* engine thunk (vm.c) with the` |
|         - | 10238 | ` * enum's FQN embedded as a literal — the same forwarder pattern the` |
|         - | 10239 | ` * Generator/Fiber/Reflection builtins use. The source buffer is owned by the` |
|         - | 10240 | ` * VM allocator and never freed: tokens (method and parameter names) keep` |
|         - | 10241 | ` * pointers into it (see the constructor-promotion precedent above).` |
|         - | 10242 | ` */` |
|      3962 | 10243 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|         5 | 10244 | `{` |
|         - | 10245 | `	SyToken *pSaveIn,*pSaveEnd;` |
|         - | 10246 | `	const char *zBack;` |
|         - | 10247 | `	SySet sToken;` |
|         - | 10248 | `	char *zSrc;` |
|         - | 10249 | `	sxu32 nSrc,nMax;` |
|      3967 | 10250 | `	sxi32 rc = SXRET_OK;` |
|      3967 | 10251 | `	nMax = 3*(sxu32)sizeof("function tryFrom(string $value){return __phl_enum_tryfrom('',$value);}")` |
|      3962 | 10252 | `		+ 3*SyStringLength(&pClass->sName) + 64;` |
|      3967 | 10253 | `	zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nMax);` |
|      3967 | 10254 | `	if( zSrc == 0 ){` |
|       ! 0 | 10255 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10256 | `		return SXERR_ABORT;` |
|         - | 10257 | `	}` |
|      3967 | 10258 | `	zBack = (pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string";` |
|      3967 | 10259 | `	if( pClass->nEnumBacking != 0 ){` |
|      5930 | 10260 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         - | 10261 | `			"function cases(){return __phl_enum_cases('%z');}"` |
|         - | 10262 | `			"function from(%s $value){return __phl_enum_from('%z',$value);}"` |
|         - | 10263 | `			"function tryFrom(%s $value){return __phl_enum_tryfrom('%z',$value);}",` |
|      1975 | 10264 | `			&pClass->sName,zBack,&pClass->sName,zBack,&pClass->sName);` |
|      1980 | 10265 | `	}else{` |
|        21 | 10266 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|         6 | 10267 | `			"function cases(){return __phl_enum_cases('%z');}",&pClass->sName);` |
|         - | 10268 | `	}` |
|      3967 | 10269 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      3967 | 10270 | `	PH7_TokenizePHP(zSrc,nSrc,pClass->nLine,&sToken,0);` |
|      3967 | 10271 | `	pSaveIn = pGen->pIn;` |
|      3967 | 10272 | `	pSaveEnd = pGen->pEnd;` |
|      3967 | 10273 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      3967 | 10274 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|     15829 | 10275 | `	while( pGen->pIn < pGen->pEnd && rc != SXERR_ABORT ){` |
|     11867 | 10276 | `		rc = GenStateCompileClassMethod(&(*pGen),PH7_TKWRD_PUBLIC,PH7_CLASS_ATTR_STATIC,TRUE,pClass);` |
|         5 | 10277 | `	}` |
|      3967 | 10278 | `	pGen->pIn = pSaveIn;` |
|      3967 | 10279 | `	pGen->pEnd = pSaveEnd;` |
|      3967 | 10280 | `	SySetRelease(&sToken);` |
|      3967 | 10281 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXRET_OK;` |
|      1986 | 10282 | `}` |
|         - | 10283 | `/*` |
|         - | 10284 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|         - | 10285 | ` * __call/__callStatic/__invoke stay allowed).` |
|         - | 10286 | ` */` |
|         - | 10287 | `static const char *azEnumBannedMagic[] = {` |
|         - | 10288 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|         - | 10289 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|         - | 10290 | `};` |
|         - | 10291 | `/*` |
|         - | 10292 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|         - | 10293 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|         - | 10294 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|         - | 10295 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|         - | 10296 | ` * and before the class is installed.` |
|         - | 10297 | ` */` |
|      3962 | 10298 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|         5 | 10299 | `{` |
|         - | 10300 | `	SyHashEntry *pEntry;` |
|         - | 10301 | `	sxi32 rc;` |
|         - | 10302 | `	sxu32 n;` |
|         - | 10303 | `	/* php: "Enum %s cannot include properties" */` |
|      3967 | 10304 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     11885 | 10305 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      7925 | 10306 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      7925 | 10307 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|         3 | 10308 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|         1 | 10309 | `				"Enum %z cannot include properties",&pClass->sName);` |
|         3 | 10310 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10311 | `				return SXERR_ABORT;` |
|         - | 10312 | `			}` |
|         3 | 10313 | `			break;` |
|         - | 10314 | `		}` |
|         5 | 10315 | `	}` |
|         - | 10316 | `	/* php: "Enum %s cannot include magic method %s" */` |
|     55473 | 10317 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|     77259 | 10318 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|     51511 | 10319 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|       ! 0 | 10320 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10321 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|       ! 0 | 10322 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10323 | `				return SXERR_ABORT;` |
|         - | 10324 | `			}` |
|       ! 0 | 10325 | `		}` |
|     25758 | 10326 | `	}` |
|         - | 10327 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|         - | 10328 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|         - | 10329 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|         - | 10330 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|         - | 10331 | `	{` |
|         - | 10332 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|         - | 10333 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|         - | 10334 | `		ph7_class_attr *pAttr;` |
|      3967 | 10335 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10336 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      3967 | 10337 | `		if( pAttr == 0 ){` |
|       ! 0 | 10338 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10339 | `			return SXERR_ABORT;` |
|         - | 10340 | `		}` |
|      3967 | 10341 | `		pAttr->nType = MEMOBJ_STRING;` |
|      3967 | 10342 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|      3967 | 10343 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|      3967 | 10344 | `		if( pClass->nEnumBacking != 0 ){` |
|      3955 | 10345 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|         - | 10346 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|      3955 | 10347 | `			if( pAttr == 0 ){` |
|       ! 0 | 10348 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10349 | `				return SXERR_ABORT;` |
|         - | 10350 | `			}` |
|      3955 | 10351 | `			pAttr->nType = pClass->nEnumBacking;` |
|      3955 | 10352 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|         7 | 10353 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|         4 | 10354 | `			}else{` |
|      3949 | 10355 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|         - | 10356 | `			}` |
|      3955 | 10357 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|      1975 | 10358 | `		}` |
|         - | 10359 | `	}` |
|      3967 | 10360 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|      1986 | 10361 | `}` |
|         - | 10362 | `/*` |
|         - | 10363 | ` * Compile a class declaration, named or anonymous.` |
|         - | 10364 | ` *` |
|         - | 10365 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|         - | 10366 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|         - | 10367 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|         - | 10368 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|         - | 10369 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|         - | 10370 | ` * implements, body, install) is shared by both paths.` |
|         - | 10371 | ` */` |
|    364066 | 10372 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|         - | 10373 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|         5 | 10374 | `{` |
|    364071 | 10375 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 10376 | `	ph7_class *pClass,*pBase;` |
|         - | 10377 | `	SyToken *pEnd,*pTmp;` |
|         - | 10378 | `	sxi32 iProtection;` |
|         - | 10379 | `	SySet aInterfaces;` |
|         - | 10380 | `	SySet aUseEntries;` |
|         - | 10381 | `	sxi32 iAttrflags;` |
|         - | 10382 | `	SyString *pName;` |
|         - | 10383 | `	sxi32 nKwrd;` |
|         - | 10384 | `	sxi32 rc;` |
|         - | 10385 | `	/* Jump the 'class' keyword */` |
|    364071 | 10386 | `	pGen->pIn++;` |
|    364071 | 10387 | `	if( pAnonName ){` |
|         - | 10388 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|         - | 10389 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|         - | 10390 | `		 * then use the synthesized name. */` |
|        32 | 10391 | `		*ppArgStart = *ppArgEnd = 0;` |
|        32 | 10392 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         7 | 10393 | `			pGen->pIn++; /* Jump '(' */` |
|         7 | 10394 | `			*ppArgStart = pGen->pIn;` |
|        10 | 10395 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|         3 | 10396 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|         7 | 10397 | `			pGen->pIn = *ppArgEnd;` |
|         7 | 10398 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|         3 | 10399 | `		}` |
|        32 | 10400 | `		pName = pAnonName;` |
|        32 | 10401 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|        18 | 10402 | `	}else{` |
|    364043 | 10403 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|         - | 10404 | `			/* Syntax error */` |
|       ! 0 | 10405 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|       ! 0 | 10406 | `			if( rc == SXERR_ABORT ){` |
|         - | 10407 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 10408 | `				return SXERR_ABORT;` |
|         - | 10409 | `			}` |
|         - | 10410 | `			/* Synchronize with the first semi-colon or curly braces */` |
|       ! 0 | 10411 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|       ! 0 | 10412 | `				pGen->pIn++;` |
|       ! 0 | 10413 | `			}` |
|       ! 0 | 10414 | `			return SXRET_OK;` |
|         - | 10415 | `		}` |
|         - | 10416 | `		/* Extract class name */` |
|    364043 | 10417 | `		pName = &pGen->pIn->sData;` |
|         - | 10418 | `		/* Advance the stream cursor */` |
|    364043 | 10419 | `		pGen->pIn++;` |
|         - | 10420 | `		/* Build FQN and obtain a raw class */ {` |
|         - | 10421 | `			SyBlob sFQN;` |
|         - | 10422 | `			SyString sFQNStr;` |
|    364043 | 10423 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    364043 | 10424 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|    364043 | 10425 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    364043 | 10426 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    364043 | 10427 | `			SyBlobRelease(&sFQN);` |
|         - | 10428 | `		}` |
|         - | 10429 | `	}` |
|    364071 | 10430 | `	if( pClass == 0 ){` |
|       ! 0 | 10431 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 10432 | `		return SXERR_ABORT;` |
|         - | 10433 | `	}` |
|    364066 | 10434 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|      3971 | 10435 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|         - | 10436 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|      3957 | 10437 | `		pGen->pIn++; /* Jump ':' */` |
|      3952 | 10438 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      3957 | 10439 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|         7 | 10440 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|         7 | 10441 | `			pGen->pIn++;` |
|      3950 | 10442 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      3951 | 10443 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|      3949 | 10444 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|      3949 | 10445 | `			pGen->pIn++;` |
|      1977 | 10446 | `		}else{` |
|         3 | 10447 | `			SyToken *pTok = pGen->pIn;` |
|         3 | 10448 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|         4 | 10449 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|         1 | 10450 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|         3 | 10451 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 10452 | `				return SXERR_ABORT;` |
|         - | 10453 | `			}` |
|         3 | 10454 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|         3 | 10455 | `				pGen->pIn++; /* Skip the bogus type token */` |
|         1 | 10456 | `			}` |
|         - | 10457 | `		}` |
|      1976 | 10458 | `	}` |
|    364071 | 10459 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    364071 | 10460 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 10461 | `		return SXERR_ABORT;` |
|         - | 10462 | `	}` |
|         - | 10463 | `	/* implemented interfaces and per-use-statement trait containers */` |
|    364071 | 10464 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|    364071 | 10465 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|         - | 10466 | `	/* Assume a standalone class */` |
|    364071 | 10467 | `	pBase = 0;` |
|    364071 | 10468 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    295785 | 10469 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    295785 | 10470 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|         - | 10471 | `			SyBlob sResolved;` |
|         - | 10472 | `			SyString sBaseName;` |
|         - | 10473 | `			sxu32 nRefLine;` |
|    189283 | 10474 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|         - | 10475 | `				/* php parse-fatals here (enums have no inheritance) */` |
|       ! 0 | 10476 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10477 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|       ! 0 | 10478 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10479 | `					return SXERR_ABORT;` |
|         - | 10480 | `				}` |
|       ! 0 | 10481 | `			}` |
|    189283 | 10482 | `			pGen->pIn++; /* Advance past 'extends' */` |
|    189283 | 10483 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    189283 | 10484 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    189283 | 10485 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         3 | 10486 | `				SyBlobRelease(&sResolved);` |
|         4 | 10487 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 10488 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|         1 | 10489 | `					pName);` |
|         3 | 10490 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|         3 | 10491 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10492 | `					return SXERR_ABORT;` |
|         - | 10493 | `				}` |
|         3 | 10494 | `				return SXRET_OK;` |
|         - | 10495 | `			}` |
|    283919 | 10496 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    189276 | 10497 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    189281 | 10498 | `			SyStringInitFromBuf(&sBaseName,` |
|         - | 10499 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 10500 | `			/* Interfaces are not allowed */` |
|    189281 | 10501 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|       ! 0 | 10502 | `				pBase = pBase->pNextName;` |
|       ! 0 | 10503 | `			}` |
|    189281 | 10504 | `			if( pBase == 0 ){` |
|       ! 0 | 10505 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 10506 | `					"Nonexistent base class '%z'",&sBaseName);` |
|       ! 0 | 10507 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 10508 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 10509 | `					return SXERR_ABORT;` |
|         - | 10510 | `				}` |
|       ! 0 | 10511 | `			}else{` |
|    189281 | 10512 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|         4 | 10513 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         1 | 10514 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|         3 | 10515 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10516 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10517 | `						return SXERR_ABORT;` |
|         - | 10518 | `					}` |
|         3 | 10519 | `					pBase = 0; /* Never inherit from an enum */` |
|    189280 | 10520 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|       ! 0 | 10521 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       ! 0 | 10522 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|       ! 0 | 10523 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10524 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10525 | `						return SXERR_ABORT;` |
|         - | 10526 | `					}` |
|       ! 0 | 10527 | `				}` |
|         - | 10528 | `			}` |
|    189281 | 10529 | `			SyBlobRelease(&sResolved);` |
|    189281 | 10530 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|       ! 0 | 10531 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|       ! 0 | 10532 | `			}` |
|     94638 | 10533 | `		}` |
|    295783 | 10534 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|         - | 10535 | `			ph7_class *pInterface;` |
|         - | 10536 | `			/* Interface implementation */` |
|    110457 | 10537 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    114308 | 10538 | `			for(;;){` |
|         - | 10539 | `				SyBlob sResolved;` |
|         - | 10540 | `				SyString sIntName;` |
|         - | 10541 | `				sxu32 nRefLine;` |
|    169539 | 10542 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    169539 | 10543 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    169539 | 10544 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 10545 | `					SyBlobRelease(&sResolved);` |
|       ! 0 | 10546 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|         - | 10547 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|       ! 0 | 10548 | `						pName);` |
|       ! 0 | 10549 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10550 | `						return SXERR_ABORT;` |
|         - | 10551 | `					}` |
|       ! 0 | 10552 | `					break;` |
|         - | 10553 | `				}` |
|    339073 | 10554 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    169534 | 10555 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    169539 | 10556 | `				SyStringInitFromBuf(&sIntName,` |
|         - | 10557 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|         - | 10558 | `				/* Only interfaces are allowed */` |
|    169539 | 10559 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 10560 | `					pInterface = pInterface->pNextName;` |
|       ! 0 | 10561 | `				}` |
|    169539 | 10562 | `				if( pInterface == 0 ){` |
|       ! 0 | 10563 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|         - | 10564 | `						"Nonexistent base interface '%z'",&sIntName);` |
|       ! 0 | 10565 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10566 | `						SyBlobRelease(&sResolved);` |
|       ! 0 | 10567 | `						return SXERR_ABORT;` |
|         - | 10568 | `					}` |
|       ! 0 | 10569 | `				}else{` |
|         - | 10570 | `					/* Reject user classes that try to implement Throwable` |
|         - | 10571 | `					 * directly (or via an interface that extends Throwable)` |
|         - | 10572 | `					 * unless they already extend Exception or Error.` |
|         - | 10573 | `					 * Exception and Error themselves are compiled from the` |
|         - | 10574 | `					 * built-in library and are exempt by FQN — a namespaced` |
|         - | 10575 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    169539 | 10576 | `					SyString *pFqn = &pClass->sName;` |
|    169539 | 10577 | `					int bIsExceptionOrError =` |
|     88709 | 10578 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|    256274 | 10579 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    167572 | 10580 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|      3952 | 10581 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|    173477 | 10582 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|     11832 | 10583 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|      3941 | 10584 | `						!bIsExceptionOrError ){` |
|        12 | 10585 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10586 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|         3 | 10587 | `							&pClass->sName);` |
|         9 | 10588 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10589 | `							SyBlobRelease(&sResolved);` |
|       ! 0 | 10590 | `							return SXERR_ABORT;` |
|         - | 10591 | `						}` |
|         - | 10592 | `						/* Skip registration so the follow-up abstract-method` |
|         - | 10593 | `						 * check does not produce a duplicate fatal. */` |
|         6 | 10594 | `					}else{` |
|    169533 | 10595 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|         - | 10596 | `					}` |
|         - | 10597 | `				}` |
|    169539 | 10598 | `				SyBlobRelease(&sResolved);` |
|    169539 | 10599 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     55231 | 10600 | `					break;` |
|         - | 10601 | `				}` |
|     59087 | 10602 | `				pGen->pIn++;/* Jump the comma */` |
|         5 | 10603 | `			}` |
|     55226 | 10604 | `		}` |
|    147889 | 10605 | `	}` |
|    364069 | 10606 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|         - | 10607 | `		/* Syntax error */` |
|       ! 0 | 10608 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|       ! 0 | 10609 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 10610 | `		if( rc == SXERR_ABORT ){` |
|         - | 10611 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 10612 | `			return SXERR_ABORT;` |
|         - | 10613 | `		}` |
|       ! 0 | 10614 | `		return SXRET_OK;` |
|         - | 10615 | `	}` |
|    364069 | 10616 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    364069 | 10617 | `	pEnd = 0; /* cc warning */` |
|         - | 10618 | `	/* Delimit the class body */` |
|    364069 | 10619 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    364069 | 10620 | `	if( pEnd >= pGen->pEnd ){` |
|         - | 10621 | `		/* Syntax error */` |
|       ! 0 | 10622 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|       ! 0 | 10623 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 10624 | `		if( rc == SXERR_ABORT ){` |
|         - | 10625 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 10626 | `			return SXERR_ABORT;` |
|         - | 10627 | `		}` |
|       ! 0 | 10628 | `		return SXRET_OK;` |
|         - | 10629 | `	}` |
|         - | 10630 | `	/* The delimiter token is the class body's closing brace */` |
|    364069 | 10631 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 10632 | `	/* Swap token stream */` |
|    364069 | 10633 | `	pTmp = pGen->pEnd;` |
|    364069 | 10634 | `	pGen->pEnd = pEnd;` |
|         - | 10635 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|    364069 | 10636 | `	pClass->iFlags \|= iFlags;` |
|         - | 10637 | `	/* Start the parse process */` |
|   1409505 | 10638 | `	for(;;){` |
|         - | 10639 | `		/* Jump leading/trailing semi-colons */` |
|   4018319 | 10640 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    725703 | 10641 | `			pGen->pIn++;` |
|         5 | 10642 | `		}` |
|   3292621 | 10643 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 10644 | `			/* End of class body */` |
|    364027 | 10645 | `			break;` |
|         - | 10646 | `		}` |
|         - | 10647 | `		/* Bind a directly-preceding docblock to this member */` |
|   2928599 | 10648 | `		GenStateSetPendingDoc(&(*pGen));` |
|   2928594 | 10649 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|   1464302 | 10650 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|       ! 0 | 10651 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10652 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 10653 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 10654 | `			if( rc == SXERR_ABORT ){` |
|         - | 10655 | `				/* Error count limit reached,abort immediately */` |
|       ! 0 | 10656 | `				return SXERR_ABORT;` |
|         - | 10657 | `			}` |
|       ! 0 | 10658 | `			goto done;` |
|         - | 10659 | `		}` |
|         - | 10660 | `		/* Assume public visibility */` |
|   2928599 | 10661 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|   2928599 | 10662 | `		iAttrflags = 0;` |
|         - | 10663 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|         - | 10664 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|         - | 10665 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|         - | 10666 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|   2928599 | 10667 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 10668 | `			int bMod = 0;` |
|       ! 0 | 10669 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 10670 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|         - | 10671 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|         - | 10672 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|         - | 10673 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|         - | 10674 | `			 * that the generic keyword dispatch would misread as a method. */` |
|       ! 0 | 10675 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       ! 0 | 10676 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       ! 0 | 10677 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|       ! 0 | 10678 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|       ! 0 | 10679 | `			}` |
|       ! 0 | 10680 | `			if( !bMod ){` |
|       ! 0 | 10681 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 10682 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 10683 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10684 | `						return SXERR_ABORT;` |
|         - | 10685 | `					}` |
|       ! 0 | 10686 | `					goto done;` |
|         - | 10687 | `				}` |
|       ! 0 | 10688 | `				continue;` |
|         - | 10689 | `			}` |
|       ! 0 | 10690 | `		}` |
|   2928599 | 10691 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 10692 | `			/* Extract the current keyword */` |
|   2928599 | 10693 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   2928599 | 10694 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|         - | 10695 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|      7923 | 10696 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|      7923 | 10697 | `				if( rc != SXRET_OK ){` |
|         6 | 10698 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10699 | `						return SXERR_ABORT;` |
|         - | 10700 | `					}` |
|         6 | 10701 | `					goto done;` |
|         - | 10702 | `				}` |
|      7919 | 10703 | `				continue;` |
|         - | 10704 | `			}` |
|   2920681 | 10705 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 10706 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|         - | 10707 | `				TraitUseEntry sUse;` |
|     15821 | 10708 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|     15821 | 10709 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|     15821 | 10710 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      7916 | 10711 | `				for(;;){` |
|         - | 10712 | `					ph7_class *pTrait;` |
|         - | 10713 | `					SyString *pTraitName;` |
|     15829 | 10714 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 10715 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10716 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|       ! 0 | 10717 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10718 | `							return SXERR_ABORT;` |
|         - | 10719 | `						}` |
|       ! 0 | 10720 | `						break;` |
|         - | 10721 | `					}` |
|     15829 | 10722 | `					pTraitName = &pGen->pIn->sData;` |
|         - | 10723 | `					/* Resolve trait name through namespace/imports */ {` |
|         - | 10724 | `						SyBlob sResolved;` |
|     15829 | 10725 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     15829 | 10726 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     31653 | 10727 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|     15824 | 10728 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|     15829 | 10729 | `						SyBlobRelease(&sResolved);` |
|         - | 10730 | `					}` |
|         - | 10731 | `					/* Only traits are allowed */` |
|     15829 | 10732 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 10733 | `						pTrait = pTrait->pNextName;` |
|       ! 0 | 10734 | `					}` |
|     15829 | 10735 | `					if( pTrait == 0 ){` |
|       ! 0 | 10736 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 10737 | `							"'%z' is not a trait",pTraitName);` |
|       ! 0 | 10738 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10739 | `							return SXERR_ABORT;` |
|         - | 10740 | `						}` |
|       ! 0 | 10741 | `					}else{` |
|     15829 | 10742 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|         - | 10743 | `					}` |
|     15829 | 10744 | `					pGen->pIn++; /* Advance past trait name */` |
|     15829 | 10745 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      7913 | 10746 | `						break;` |
|         - | 10747 | `					}` |
|        10 | 10748 | `					pGen->pIn++; /* Jump the comma */` |
|         2 | 10749 | `				}` |
|         - | 10750 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|     15821 | 10751 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|         - | 10752 | `					SyToken *pBlock;` |
|        13 | 10753 | `					pGen->pIn++; /* Jump '{' */` |
|        13 | 10754 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|        13 | 10755 | `					sUse.pResolvStart = pGen->pIn;` |
|        13 | 10756 | `					sUse.pResolvEnd = pBlock;` |
|        13 | 10757 | `					if( pBlock < pGen->pEnd ){` |
|        13 | 10758 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|         8 | 10759 | `					}else{` |
|       ! 0 | 10760 | `						pGen->pIn = pGen->pEnd;` |
|         - | 10761 | `					}` |
|         5 | 10762 | `				}` |
|     15821 | 10763 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|         - | 10764 | `				/* The semicolon will be consumed by the outer loop */` |
|     15821 | 10765 | `				continue;` |
|         - | 10766 | `			}` |
|   2904865 | 10767 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         - | 10768 | `				int nSetTok;` |
|   2652341 | 10769 | `				sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   2652341 | 10770 | `				if( nSetVis ){` |
|         - | 10771 | ``					/* Leading `private(set)`/`protected(set)` with no read`` |
|         - | 10772 | `					 * visibility: the read side defaults to public (php 8.4). */` |
|         3 | 10773 | `					iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 10774 | `					pGen->pIn += nSetTok;` |
|         2 | 10775 | `				}else{` |
|   2652339 | 10776 | `					iProtection = nKwrd;` |
|   2652339 | 10777 | `					pGen->pIn++; /* Jump the visibility token */` |
|         - | 10778 | `					/* Optional asymmetric set-visibility after the read` |
|         - | 10779 | ``					 * visibility: `public private(set) int $x`. */`` |
|   2652339 | 10780 | `					nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|   2652339 | 10781 | `					if( nSetVis ){` |
|         9 | 10782 | `						iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         9 | 10783 | `						pGen->pIn += nSetTok;` |
|         4 | 10784 | `					}` |
|         - | 10785 | `				}` |
|         - | 10786 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`,`` |
|         - | 10787 | ``				 * `public private(set) readonly int $x`. */`` |
|   2652341 | 10788 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|        24 | 10789 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|        24 | 10790 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        10 | 10791 | `				}` |
|   2652336 | 10792 | `				if( pGen->pIn >= pGen->pEnd` |
|   2652341 | 10793 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 10794 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10795 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|       ! 0 | 10796 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 10797 | `					if( rc == SXERR_ABORT ){` |
|         - | 10798 | `						/* Error count limit reached,abort immediately */` |
|       ! 0 | 10799 | `						return SXERR_ABORT;` |
|         - | 10800 | `					}` |
|       ! 0 | 10801 | `					goto done;` |
|         - | 10802 | `				}` |
|   2652341 | 10803 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 10804 | `					/* Attribute declaration (untyped) */` |
|    421979 | 10805 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    421979 | 10806 | `					if( rc != SXRET_OK ){` |
|        11 | 10807 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10808 | `							return SXERR_ABORT;` |
|         - | 10809 | `						}` |
|        11 | 10810 | `						goto done;` |
|         - | 10811 | `					}` |
|    422115 | 10812 | `					continue;` |
|         - | 10813 | `				}` |
|   2230367 | 10814 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 10815 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|       299 | 10816 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       299 | 10817 | `					if( rc != SXRET_OK ){` |
|         8 | 10818 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 10819 | `							return SXERR_ABORT;` |
|         - | 10820 | `						}` |
|         8 | 10821 | `						goto done;` |
|         - | 10822 | `					}` |
|       293 | 10823 | `					continue;` |
|         - | 10824 | `				}` |
|         - | 10825 | `				/* Extract the keyword */` |
|   2230073 | 10826 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1115034 | 10827 | `			}` |
|   2482597 | 10828 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|         - | 10829 | `				/* Process constant declaration */` |
|    244311 | 10830 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|    244311 | 10831 | `				if( rc != SXRET_OK ){` |
|        11 | 10832 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 10833 | `						return SXERR_ABORT;` |
|         - | 10834 | `					}` |
|        11 | 10835 | `					goto done;` |
|         - | 10836 | `				}` |
|    122154 | 10837 | `			}else{` |
|   2238291 | 10838 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|         - | 10839 | `					/* Static method or attribute,record that */` |
|     98595 | 10840 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     98595 | 10841 | `					pGen->pIn++; /* Jump the static keyword */` |
|     98595 | 10842 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 10843 | `						int nSetTok;` |
|     70999 | 10844 | `						sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|     70999 | 10845 | `						if( nSetVis ){` |
|         - | 10846 | ``							/* `static private(set) int $x` — read side stays public */`` |
|         3 | 10847 | `							iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|         3 | 10848 | `							pGen->pIn += nSetTok;` |
|         2 | 10849 | `						}else{` |
|         - | 10850 | `							/* Extract the keyword */` |
|     70997 | 10851 | `							nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     70997 | 10852 | `							if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 10853 | `								iProtection = nKwrd;` |
|       ! 0 | 10854 | `								pGen->pIn++; /* Jump the visibility token */` |
|       ! 0 | 10855 | `								nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|       ! 0 | 10856 | `								if( nSetVis ){` |
|       ! 0 | 10857 | `									iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|       ! 0 | 10858 | `									pGen->pIn += nSetTok;` |
|       ! 0 | 10859 | `								}` |
|       ! 0 | 10860 | `							}` |
|         - | 10861 | `						}` |
|     35497 | 10862 | `					}` |
|         - | 10863 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|         - | 10864 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|         - | 10865 | `					 * than a generic "expecting method" parse error. */` |
|     98595 | 10866 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       ! 0 | 10867 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       ! 0 | 10868 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       ! 0 | 10869 | `					}` |
|     98590 | 10870 | `					if( pGen->pIn >= pGen->pEnd` |
|     98595 | 10871 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 10872 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10873 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|       ! 0 | 10874 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 10875 | `						if( rc == SXERR_ABORT ){` |
|         - | 10876 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 10877 | `							return SXERR_ABORT;` |
|         - | 10878 | `						}` |
|       ! 0 | 10879 | `						goto done;` |
|         - | 10880 | `					}` |
|     98595 | 10881 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         - | 10882 | `						/* Attribute declaration */` |
|     27599 | 10883 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     27599 | 10884 | `						if( rc != SXRET_OK ){` |
|         3 | 10885 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 10886 | `								return SXERR_ABORT;` |
|         - | 10887 | `							}` |
|         3 | 10888 | `							goto done;` |
|         - | 10889 | `						}` |
|     27597 | 10890 | `						continue;` |
|         - | 10891 | `					}` |
|     71001 | 10892 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         - | 10893 | `						/* Typed static attribute declaration */` |
|        17 | 10894 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        17 | 10895 | `						if( rc != SXRET_OK ){` |
|         3 | 10896 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 10897 | `								return SXERR_ABORT;` |
|         - | 10898 | `							}` |
|         3 | 10899 | `							goto done;` |
|         - | 10900 | `						}` |
|        15 | 10901 | `						continue;` |
|         - | 10902 | `					}` |
|         - | 10903 | `					/* Extract the keyword */` |
|     70987 | 10904 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   2175192 | 10905 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         - | 10906 | `					/* Abstract method,record that */` |
|      7899 | 10907 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         - | 10908 | `					/* Mark the whole class as abstract */` |
|      7899 | 10909 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|         - | 10910 | `					/* Advance the stream cursor */` |
|      7899 | 10911 | `					pGen->pIn++;` |
|      7899 | 10912 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      7899 | 10913 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      7899 | 10914 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      7897 | 10915 | `							iProtection = nKwrd;` |
|      7897 | 10916 | `							pGen->pIn++; /* Jump the visibility token */` |
|      3946 | 10917 | `						}` |
|      3947 | 10918 | `					}` |
|      7899 | 10919 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      7894 | 10920 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 10921 | `							/* Static method */` |
|       ! 0 | 10922 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 10923 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 10924 | `					}` |
|      7899 | 10925 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      7894 | 10926 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|         - | 10927 | ``							/* PHP 8.4: `abstract public [T] $x { get; set; }` — an abstract`` |
|         - | 10928 | `							 * HOOKED property declaration. Route anything that is not a` |
|         - | 10929 | `							 * method through the attribute compiler with the ABSTRACT flag;` |
|         - | 10930 | ``							 * the hook parser accepts the bare `get;`/`set;` forms there`` |
|         - | 10931 | `							 * (and a non-hooked abstract property is ITS error to raise). */` |
|         6 | 10932 | `							if( pGen->pIn < pGen->pEnd` |
|         7 | 10933 | `							 && ((pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID\|PH7_TK_DOLLAR)) != 0` |
|         3 | 10934 | `							  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|         7 | 10935 | `								rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         7 | 10936 | `								if( rc != SXRET_OK ){` |
|       ! 0 | 10937 | `									if( rc == SXERR_ABORT ){` |
|       ! 0 | 10938 | `										return SXERR_ABORT;` |
|         - | 10939 | `									}` |
|       ! 0 | 10940 | `									goto done;` |
|         - | 10941 | `								}` |
|         7 | 10942 | `								continue;` |
|         - | 10943 | `							}` |
|       ! 0 | 10944 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10945 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|       ! 0 | 10946 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 10947 | `							if( rc == SXERR_ABORT ){` |
|         - | 10948 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 10949 | `								return SXERR_ABORT;` |
|         - | 10950 | `							}` |
|       ! 0 | 10951 | `							goto done;` |
|         - | 10952 | `					}` |
|      7893 | 10953 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|   2135751 | 10954 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|         - | 10955 | `					/* final method ,record that */` |
|        20 | 10956 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|        20 | 10957 | `					pGen->pIn++; /* Jump the final keyword */` |
|        20 | 10958 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         - | 10959 | `						/* Extract the keyword */` |
|        20 | 10960 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        20 | 10961 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        10 | 10962 | `							iProtection = nKwrd;` |
|        10 | 10963 | `							pGen->pIn++; /* Jump the visibility token */` |
|         4 | 10964 | `						}` |
|         9 | 10965 | `					}` |
|        20 | 10966 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        18 | 10967 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|         - | 10968 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|         - | 10969 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|         - | 10970 | `							 * child class is compiled (PH7_ClassInherit). */` |
|        14 | 10971 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|        14 | 10972 | `							if( rc != SXRET_OK ){` |
|       ! 0 | 10973 | `								if( rc == SXERR_ABORT ){` |
|       ! 0 | 10974 | `									return SXERR_ABORT;` |
|         - | 10975 | `								}` |
|       ! 0 | 10976 | `								goto done;` |
|         - | 10977 | `							}` |
|        14 | 10978 | `							continue;` |
|         - | 10979 | `					}` |
|         8 | 10980 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|         6 | 10981 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|         - | 10982 | `							/* Static method */` |
|       ! 0 | 10983 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       ! 0 | 10984 | `							pGen->pIn++; /* Jump the static keyword */` |
|       ! 0 | 10985 | `					}` |
|         8 | 10986 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         6 | 10987 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 10988 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 10989 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|       ! 0 | 10990 | `								&pGen->pIn->sData,pName);` |
|       ! 0 | 10991 | `							if( rc == SXERR_ABORT ){` |
|         - | 10992 | `								/* Error count limit reached,abort immediately */` |
|       ! 0 | 10993 | `								return SXERR_ABORT;` |
|         - | 10994 | `							}` |
|       ! 0 | 10995 | `							goto done;` |
|         - | 10996 | `					}` |
|         8 | 10997 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         3 | 10998 | `				}` |
|   2210665 | 10999 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 11000 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11001 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|       ! 0 | 11002 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11003 | `						if( rc == SXERR_ABORT ){` |
|         - | 11004 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11005 | `							return SXERR_ABORT;` |
|         - | 11006 | `						}` |
|       ! 0 | 11007 | `						goto done;` |
|         - | 11008 | `				}` |
|   2210665 | 11009 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|         7 | 11010 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|         7 | 11011 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|       ! 0 | 11012 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11013 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 11014 | `						if( rc == SXERR_ABORT ){` |
|         - | 11015 | `							/* Error count limit reached,abort immediately */` |
|       ! 0 | 11016 | `							return SXERR_ABORT;` |
|         - | 11017 | `						}` |
|       ! 0 | 11018 | `						goto done;` |
|         - | 11019 | `					}` |
|         - | 11020 | `					/* Attribute declaration */` |
|         7 | 11021 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         4 | 11022 | `				}else{` |
|         - | 11023 | `					/* Process method declaration */` |
|   2210659 | 11024 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 11025 | `				}` |
|   2210665 | 11026 | `				if( rc != SXRET_OK ){` |
|        16 | 11027 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11028 | `						return SXERR_ABORT;` |
|         - | 11029 | `					}` |
|        16 | 11030 | `					goto done;` |
|         - | 11031 | `				}` |
|         - | 11032 | `			}` |
|   1227478 | 11033 | `		}else{` |
|         - | 11034 | `			/* Attribute declaration */` |
|       ! 0 | 11035 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11036 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11037 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11038 | `					return SXERR_ABORT;` |
|         - | 11039 | `				}` |
|       ! 0 | 11040 | `				goto done;` |
|         - | 11041 | `			}` |
|         - | 11042 | `		}` |
|         5 | 11043 | `	}` |
|         - | 11044 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|         - | 11045 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|         - | 11046 | `	 */` |
|         - | 11047 | `	{` |
|         - | 11048 | `		TraitUseEntry *apUse;` |
|         - | 11049 | `		sxu32 nU;` |
|    364027 | 11050 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|    379843 | 11051 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|     15821 | 11052 | `			TraitUseEntry *pUse = &apUse[nU];` |
|     15821 | 11053 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|     15821 | 11054 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|     15821 | 11055 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|         - | 11056 | `			sxu32 nT;` |
|     15821 | 11057 | `			if( !hasResolution ){` |
|         - | 11058 | `				/* No conflict resolution block: use standard trait application */` |
|     31623 | 11059 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|     15817 | 11060 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|     15817 | 11061 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11062 | `						break;` |
|         - | 11063 | `					}` |
|      7911 | 11064 | `				}` |
|      7908 | 11065 | `			}else{` |
|         - | 11066 | `				/* With resolution block: copy attributes, record traits,` |
|         - | 11067 | `				 * then use the block to resolve method conflicts.` |
|         - | 11068 | `				 */` |
|         - | 11069 | `				SyToken *pR;` |
|        25 | 11070 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        15 | 11071 | `					ph7_class *pTR = apTrait[nT];` |
|         - | 11072 | `					ph7_class_attr *pAR;` |
|         - | 11073 | `					SyHashEntry *pER;` |
|         - | 11074 | `					SyString *pNR;` |
|        15 | 11075 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|        21 | 11076 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|       ! 0 | 11077 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|       ! 0 | 11078 | `						pNR = &pAR->sName;` |
|       ! 0 | 11079 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       ! 0 | 11080 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|       ! 0 | 11081 | `						}` |
|       ! 0 | 11082 | `					}` |
|        15 | 11083 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|         9 | 11084 | `				}` |
|         - | 11085 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|        13 | 11086 | `				pR = pUse->pResolvStart;` |
|        27 | 11087 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 11088 | `					SyString sTrait,sMethod;` |
|         - | 11089 | `					ph7_class *pSrcTrait;` |
|         - | 11090 | `					ph7_class_method *pMeth;` |
|         - | 11091 | `					sxi32 nRKwrd;` |
|        41 | 11092 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        27 | 11093 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        17 | 11094 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        17 | 11095 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        17 | 11096 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        17 | 11097 | `					sMethod = pR->sData;` |
|        17 | 11098 | `					pR++;` |
|        17 | 11099 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|         7 | 11100 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|         7 | 11101 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|         7 | 11102 | `							sTrait = sMethod;` |
|         7 | 11103 | `							pR++;` |
|         7 | 11104 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|         7 | 11105 | `							sMethod = pR->sData;` |
|         7 | 11106 | `							pR++;` |
|         3 | 11107 | `						}` |
|         3 | 11108 | `					}` |
|        17 | 11109 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 11110 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       ! 0 | 11111 | `						continue;` |
|         - | 11112 | `					}` |
|        17 | 11113 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|        17 | 11114 | `					pR++;` |
|        17 | 11115 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|         5 | 11116 | `						pSrcTrait = 0;` |
|         7 | 11117 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         7 | 11118 | `							SyString *pTN = &apTrait[nT]->sName;` |
|        10 | 11119 | `							if( pTN->nByte >= sTrait.nByte &&` |
|         6 | 11120 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         5 | 11121 | `								pSrcTrait = apTrait[nT];` |
|         5 | 11122 | `								break;` |
|         - | 11123 | `							}` |
|         2 | 11124 | `						}` |
|         5 | 11125 | `						if( pSrcTrait ){` |
|         5 | 11126 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         5 | 11127 | `							if( pMeth ){` |
|         5 | 11128 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|         5 | 11129 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|         5 | 11130 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|         2 | 11131 | `								}` |
|         2 | 11132 | `							}` |
|         2 | 11133 | `						}` |
|         2 | 11134 | `					}` |
|        35 | 11135 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 11136 | `				}` |
|         - | 11137 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|        25 | 11138 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         - | 11139 | `					ph7_class_method *pMR;` |
|         - | 11140 | `					SyHashEntry *pER;` |
|         - | 11141 | `					SyString *pNR;` |
|        15 | 11142 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|        41 | 11143 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|        23 | 11144 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|        23 | 11145 | `						pNR = &pMR->sFunc.sName;` |
|        23 | 11146 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|        14 | 11147 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|         6 | 11148 | `						}` |
|         3 | 11149 | `					}` |
|         9 | 11150 | `				}` |
|         - | 11151 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|        13 | 11152 | `				pR = pUse->pResolvStart;` |
|        27 | 11153 | `				while( pR < pUse->pResolvEnd ){` |
|         - | 11154 | `					SyString sTrait,sMethod,sAlias;` |
|         - | 11155 | `					ph7_class *pSrcTrait;` |
|         - | 11156 | `					ph7_class_method *pMeth;` |
|        27 | 11157 | `					int hasQual = 0;` |
|         - | 11158 | `					sxi32 nRKwrd;` |
|        41 | 11159 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|        27 | 11160 | `					if( pR >= pUse->pResolvEnd ) break;` |
|        17 | 11161 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|        17 | 11162 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|        17 | 11163 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|        17 | 11164 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|        17 | 11165 | `					sMethod = pR->sData;` |
|        17 | 11166 | `					pR++;` |
|        17 | 11167 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|         7 | 11168 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|         7 | 11169 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|         7 | 11170 | `							sTrait = sMethod;` |
|         7 | 11171 | `							hasQual = 1;` |
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
|        17 | 11184 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|        13 | 11185 | `						sxi32 iNewVis = -1;` |
|        13 | 11186 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|         7 | 11187 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|         7 | 11188 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|         7 | 11189 | `								iNewVis = nAK;` |
|         7 | 11190 | `								pR++;` |
|         3 | 11191 | `							}` |
|         3 | 11192 | `						}` |
|        13 | 11193 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|        11 | 11194 | `							sAlias = pR->sData;` |
|        11 | 11195 | `							pR++;` |
|         4 | 11196 | `						}` |
|        13 | 11197 | `						pMeth = 0;` |
|        13 | 11198 | `						if( hasQual ){` |
|         3 | 11199 | `							pSrcTrait = 0;` |
|         5 | 11200 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|         5 | 11201 | `								SyString *pTN = &apTrait[nT]->sName;` |
|         7 | 11202 | `								if( pTN->nByte >= sTrait.nByte &&` |
|         4 | 11203 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|         3 | 11204 | `									pSrcTrait = apTrait[nT];` |
|         3 | 11205 | `									break;` |
|         - | 11206 | `								}` |
|         2 | 11207 | `							}` |
|         3 | 11208 | `							if( pSrcTrait ){` |
|         3 | 11209 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|         1 | 11210 | `							}` |
|         2 | 11211 | `						}else{` |
|        10 | 11212 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|         - | 11213 | `						}` |
|        13 | 11214 | `						if( pMeth ){` |
|        13 | 11215 | `							if( sAlias.nByte > 0 ){` |
|         - | 11216 | `								/* Create a shallow copy of the method struct for the alias` |
|         - | 11217 | `								 * so it can carry its own visibility without affecting the original.` |
|         - | 11218 | `								 */` |
|         - | 11219 | `								ph7_class_method *pAlias;` |
|         - | 11220 | `								char *zAliasDup;` |
|        11 | 11221 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        11 | 11222 | `								if( pAlias ){` |
|        11 | 11223 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|        11 | 11224 | `									if( iNewVis >= 0 ){` |
|         5 | 11225 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 11226 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 11227 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         2 | 11228 | `									}` |
|        11 | 11229 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|        11 | 11230 | `									if( zAliasDup ){` |
|        11 | 11231 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|         4 | 11232 | `									}` |
|         7 | 11233 | `								}` |
|         7 | 11234 | `							}else if( iNewVis >= 0 ){` |
|         - | 11235 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|         - | 11236 | `								ph7_class_method *pCopy;` |
|         3 | 11237 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|         3 | 11238 | `								if( pCopy ){` |
|         3 | 11239 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|         3 | 11240 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|         3 | 11241 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|         3 | 11242 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|       ! 0 | 11243 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|         - | 11244 | `									/* Replace the method in the class hash */` |
|         3 | 11245 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|         3 | 11246 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|         1 | 11247 | `								}` |
|         1 | 11248 | `							}` |
|         5 | 11249 | `						}` |
|         5 | 11250 | `						SXUNUSED(hasQual);` |
|         5 | 11251 | `					}` |
|        21 | 11252 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|         3 | 11253 | `				}` |
|         - | 11254 | `			}` |
|     15821 | 11255 | `			SySetRelease(&pUse->aTraits);` |
|      7913 | 11256 | `		}` |
|         - | 11257 | `	}` |
|    364027 | 11258 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|         - | 11259 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|         - | 11260 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|      3967 | 11261 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|      3967 | 11262 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11263 | `			SySetRelease(&aUseEntries);` |
|       ! 0 | 11264 | `			SySetRelease(&aInterfaces);` |
|       ! 0 | 11265 | `			return SXERR_ABORT;` |
|         - | 11266 | `		}` |
|      1981 | 11267 | `	}` |
|         - | 11268 | `	/* Install the class */` |
|    364027 | 11269 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    364027 | 11270 | `	if( rc == SXRET_OK ){` |
|         - | 11271 | `		ph7_class **apInterface;` |
|         - | 11272 | `		sxu32 n;` |
|    364027 | 11273 | `		if( pBase ){` |
|         - | 11274 | `			/* Inherit from base class and mark as a subclass */` |
|    189279 | 11275 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|     94637 | 11276 | `		}` |
|    364027 | 11277 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|    533555 | 11278 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|         - | 11279 | `			/* Implements one or more interface */` |
|    169533 | 11280 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    169533 | 11281 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11282 | `				break;` |
|         - | 11283 | `			}` |
|     84769 | 11284 | `		}` |
|         - | 11285 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|         - | 11286 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|    364027 | 11287 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|      3967 | 11288 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|      3967 | 11289 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 11290 | `				pIntf = pIntf->pNextName;` |
|       ! 0 | 11291 | `			}` |
|      3967 | 11292 | `			if( pIntf ){` |
|      3967 | 11293 | `				PH7_ClassImplement(pClass,pIntf);` |
|      1981 | 11294 | `			}` |
|      3967 | 11295 | `			if( pClass->nEnumBacking != 0 ){` |
|      3955 | 11296 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|      3955 | 11297 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|       ! 0 | 11298 | `					pIntf = pIntf->pNextName;` |
|       ! 0 | 11299 | `				}` |
|      3955 | 11300 | `				if( pIntf ){` |
|      3955 | 11301 | `					PH7_ClassImplement(pClass,pIntf);` |
|      1975 | 11302 | `				}` |
|      1975 | 11303 | `			}` |
|      1981 | 11304 | `		}` |
|         - | 11305 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|         - | 11306 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|    364022 | 11307 | `		if( rc == SXRET_OK` |
|    364022 | 11308 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|    364027 | 11309 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|    193071 | 11310 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|         - | 11311 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|    193071 | 11312 | `			if( pStringable ){` |
|    193071 | 11313 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    193071 | 11314 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|         - | 11315 | `				sxu32 i;` |
|    193071 | 11316 | `				int bAlready = 0;` |
|    232455 | 11317 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|     43329 | 11318 | `					if( apImpl[i] == pStringable ){` |
|      3945 | 11319 | `						bAlready = 1;` |
|      3945 | 11320 | `						break;` |
|         - | 11321 | `					}` |
|     19697 | 11322 | `				}` |
|    193071 | 11323 | `				if( !bAlready ){` |
|    189131 | 11324 | `					PH7_ClassImplement(pClass,pStringable);` |
|     94563 | 11325 | `				}` |
|     96533 | 11326 | `			}` |
|     96533 | 11327 | `		}` |
|         - | 11328 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|    364027 | 11329 | `		if( rc == SXRET_OK ){` |
|    364027 | 11330 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|    364027 | 11331 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 11332 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 11333 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 11334 | `				return SXERR_ABORT;` |
|         - | 11335 | `			}` |
|    182011 | 11336 | `		}` |
|         - | 11337 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|    364027 | 11338 | `		if( rc == SXRET_OK ){` |
|    364027 | 11339 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|    364027 | 11340 | `			if( rcCheck == SXERR_ABORT ){` |
|       ! 0 | 11341 | `				SySetRelease(&aUseEntries);` |
|       ! 0 | 11342 | `				SySetRelease(&aInterfaces);` |
|       ! 0 | 11343 | `				return SXERR_ABORT;` |
|         - | 11344 | `			}` |
|    182011 | 11345 | `		}` |
|    182011 | 11346 | `	}` |
|    364027 | 11347 | `	SySetRelease(&aUseEntries);` |
|    364027 | 11348 | `	SySetRelease(&aInterfaces);` |
|    364027 | 11349 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11350 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11351 | `		return SXERR_ABORT;` |
|         - | 11352 | `	}` |
|    182011 | 11353 | `done:` |
|         - | 11354 | `	/* Point beyond the class body */` |
|    364069 | 11355 | `	pGen->pIn = &pEnd[1];` |
|    364069 | 11356 | `	pGen->pEnd = pTmp;` |
|    364069 | 11357 | `	return PH7_OK;` |
|    182038 | 11358 | `}` |
|         - | 11359 | `/* Compile a named class declaration (the common case). */` |
|    364038 | 11360 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|         5 | 11361 | `{` |
|    364043 | 11362 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|         5 | 11363 | `}` |
|         - | 11364 | `/*` |
|         - | 11365 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|         - | 11366 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|         - | 11367 | ` * compile + install the class body once (at compile time, like every other` |
|         - | 11368 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|         - | 11369 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|         - | 11370 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|         - | 11371 | ` */` |
|        28 | 11372 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|         4 | 11373 | `{` |
|         - | 11374 | `	char zName[128];         /* Synthesized class name */` |
|         - | 11375 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|         - | 11376 | `	SyString sName;` |
|         - | 11377 | `	SyToken *pArgStart,*pArgEnd;` |
|        32 | 11378 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|         - | 11379 | `	                              * is keyed to this 'class' token */` |
|         - | 11380 | `	ph7_value *pObj;` |
|        32 | 11381 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11382 | `	sxu32 nIdx,nLen;` |
|         - | 11383 | `	sxi32 nArg,rc;` |
|        14 | 11384 | `	SXUNUSED(iCompileFlag);` |
|         - | 11385 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|        32 | 11386 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|        32 | 11387 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|       ! 0 | 11388 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|       ! 0 | 11389 | `	}` |
|        32 | 11390 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|         - | 11391 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|         - | 11392 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|         - | 11393 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|        32 | 11394 | `	pArgStart = pArgEnd = 0;` |
|        32 | 11395 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|        32 | 11396 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11397 | `		return rc;` |
|         - | 11398 | `	}` |
|         - | 11399 | `	{` |
|         - | 11400 | ``		/* Expression-position attributes (`new #[A] class {…}`) */`` |
|        32 | 11401 | `		ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0);` |
|        28 | 11402 | `		if( pAnonClass` |
|        32 | 11403 | `		 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 11404 | `			return SXERR_ABORT;` |
|         - | 11405 | `		}` |
|         - | 11406 | `	}` |
|         - | 11407 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|         - | 11408 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|        32 | 11409 | `	nArg = 0;` |
|        32 | 11410 | `	if( pArgStart < pArgEnd ){` |
|         7 | 11411 | `		SyToken *pSavedIn = pGen->pIn;` |
|         7 | 11412 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|         - | 11413 | `		SyToken *pArgNext;` |
|         7 | 11414 | `		pGen->pIn = pArgStart;` |
|         7 | 11415 | `		pGen->pEnd = pArgEnd;` |
|        13 | 11416 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|         7 | 11417 | `			if( pGen->pIn < pArgNext ){` |
|         7 | 11418 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|         7 | 11419 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11420 | `					pGen->pIn = pSavedIn;` |
|       ! 0 | 11421 | `					pGen->pEnd = pSavedEnd;` |
|       ! 0 | 11422 | `					return SXERR_ABORT;` |
|         - | 11423 | `				}` |
|         7 | 11424 | `				nArg++;` |
|         3 | 11425 | `			}` |
|         7 | 11426 | `			pGen->pIn = &pArgNext[1];` |
|         1 | 11427 | `		}` |
|         7 | 11428 | `		pGen->pIn = pSavedIn;` |
|         7 | 11429 | `		pGen->pEnd = pSavedEnd;` |
|         3 | 11430 | `	}` |
|         - | 11431 | `	/* Load the synthesized class name */` |
|        32 | 11432 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        32 | 11433 | `	if( pObj == 0 ){` |
|       ! 0 | 11434 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 11435 | `		return SXERR_ABORT;` |
|         - | 11436 | `	}` |
|        32 | 11437 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|        32 | 11438 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|         - | 11439 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|        32 | 11440 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        32 | 11441 | `	return SXRET_OK;` |
|        18 | 11442 | `}` |
|         - | 11443 | `/*` |
|         - | 11444 | ` * Compile a user-defined abstract class.` |
|         - | 11445 | ` *  According to the PHP language reference manual` |
|         - | 11446 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|         - | 11447 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|         - | 11448 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|         - | 11449 | ` *   the method's signature - they cannot define the implementation.` |
|         - | 11450 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|         - | 11451 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|         - | 11452 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|         - | 11453 | ` *   method is defined as protected, the function implementation must be defined as either` |
|         - | 11454 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|         - | 11455 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|         - | 11456 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|         - | 11457 | ` *   could differ.` |
|         - | 11458 | ` */` |
|         - | 11459 | `/*` |
|         - | 11460 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|         - | 11461 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|         - | 11462 | ` * receives the corresponding PH7_CLASS_* bit.` |
|         - | 11463 | ` */` |
|  11235076 | 11464 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|         5 | 11465 | `{` |
|  11235081 | 11466 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|   6637657 | 11467 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|   6637657 | 11468 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|   6590375 | 11469 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|   3279390 | 11470 | `	}` |
|  11156209 | 11471 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  11156149 | 11472 | `	return FALSE;` |
|   5617543 | 11473 | `}` |
|         - | 11474 | `/*` |
|         - | 11475 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|         - | 11476 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|         - | 11477 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|         - | 11478 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|         - | 11479 | ` */` |
|  11156144 | 11480 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|         5 | 11481 | `{` |
|  11156149 | 11482 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  11156149 | 11483 | `	sxi32 iFlags = 0,iFlag;` |
|  11235081 | 11484 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|     78937 | 11485 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|         5 | 11486 | `			pDup = pIn;` |
|         2 | 11487 | `		}` |
|     78937 | 11488 | `		iFlags \|= iFlag;` |
|     78937 | 11489 | `		pIn++;` |
|         5 | 11490 | `	}` |
|  11156149 | 11491 | `	*ppIn = pIn;` |
|  11156149 | 11492 | `	if( ppDup ){ *ppDup = pDup; }` |
|  11156149 | 11493 | `	return iFlags;` |
|         5 | 11494 | `}` |
|         - | 11495 | `/*` |
|         - | 11496 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|         - | 11497 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|         - | 11498 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|         - | 11499 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|         - | 11500 | `` * `readonly`) to their existing handlers.`` |
|         - | 11501 | ` */` |
|  11120626 | 11502 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|         5 | 11503 | `{` |
|  11120631 | 11504 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|   5603714 | 11505 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  11142325 | 11506 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|         5 | 11507 | `}` |
|         - | 11508 | `/*` |
|         - | 11509 | ` * Compile a class declaration carrying one or more leading modifiers` |
|         - | 11510 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|         - | 11511 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|         - | 11512 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|         - | 11513 | `` * `abstract`+`final` pair, like PHP.`` |
|         - | 11514 | ` */` |
|     35518 | 11515 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|         5 | 11516 | `{` |
|         - | 11517 | `	SyToken *pDup;` |
|     35523 | 11518 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|         - | 11519 | `	sxi32 rc;` |
|     35523 | 11520 | `	if( pDup ){` |
|         4 | 11521 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|         2 | 11522 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|         3 | 11523 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11524 | `			return SXERR_ABORT;` |
|         - | 11525 | `		}` |
|         1 | 11526 | `	}` |
|     35518 | 11527 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|     17764 | 11528 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|         3 | 11529 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11530 | `			"Cannot use the final modifier on an abstract class");` |
|         3 | 11531 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11532 | `			return SXERR_ABORT;` |
|         - | 11533 | `		}` |
|         1 | 11534 | `	}` |
|     35523 | 11535 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|     17764 | 11536 | `}` |
|         - | 11537 | `/*` |
|         - | 11538 | ` * Compile a user-defined trait.` |
|         - | 11539 | ` *  Traits are similar to classes, but only intended to group functionality` |
|         - | 11540 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|         - | 11541 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|         - | 11542 | ` */` |
|      7954 | 11543 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|         5 | 11544 | `{` |
|      7959 | 11545 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11546 | `	ph7_class *pClass;` |
|         - | 11547 | `	SyToken *pEnd,*pTmp;` |
|         - | 11548 | `	sxi32 iProtection;` |
|         - | 11549 | `	sxi32 iAttrflags;` |
|         - | 11550 | `	SyString *pName;` |
|         - | 11551 | `	sxi32 nKwrd;` |
|         - | 11552 | `	sxi32 rc;` |
|         - | 11553 | `	/* Jump the 'trait' keyword */` |
|      7959 | 11554 | `	pGen->pIn++;` |
|      7959 | 11555 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 11556 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|       ! 0 | 11557 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11558 | `			return SXERR_ABORT;` |
|         - | 11559 | `		}` |
|       ! 0 | 11560 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|       ! 0 | 11561 | `			pGen->pIn++;` |
|       ! 0 | 11562 | `		}` |
|       ! 0 | 11563 | `		return SXRET_OK;` |
|         - | 11564 | `	}` |
|         - | 11565 | `	/* Extract trait name */` |
|      7959 | 11566 | `	pName = &pGen->pIn->sData;` |
|      7959 | 11567 | `	pGen->pIn++;` |
|         - | 11568 | `	/* Build FQN and obtain a raw class */ {` |
|         - | 11569 | `		SyBlob sFQN;` |
|         - | 11570 | `		SyString sFQNStr;` |
|      7959 | 11571 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7959 | 11572 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      7959 | 11573 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      7959 | 11574 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      7959 | 11575 | `		SyBlobRelease(&sFQN);` |
|         - | 11576 | `	}` |
|      7959 | 11577 | `	if( pClass == 0 ){` |
|       ! 0 | 11578 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11579 | `		return SXERR_ABORT;` |
|         - | 11580 | `	}` |
|      7959 | 11581 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|      7959 | 11582 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|       ! 0 | 11583 | `		return SXERR_ABORT;` |
|         - | 11584 | `	}` |
|         - | 11585 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      7959 | 11586 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|       ! 0 | 11587 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|       ! 0 | 11588 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 11589 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11590 | `			return SXERR_ABORT;` |
|         - | 11591 | `		}` |
|       ! 0 | 11592 | `		return SXRET_OK;` |
|         - | 11593 | `	}` |
|      7959 | 11594 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      7959 | 11595 | `	pEnd = 0;` |
|      7959 | 11596 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      7959 | 11597 | `	if( pEnd >= pGen->pEnd ){` |
|       ! 0 | 11598 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|       ! 0 | 11599 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       ! 0 | 11600 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11601 | `			return SXERR_ABORT;` |
|         - | 11602 | `		}` |
|       ! 0 | 11603 | `		return SXRET_OK;` |
|         - | 11604 | `	}` |
|         - | 11605 | `	/* The delimiter token is the trait body's closing brace */` |
|      7959 | 11606 | `	pClass->nEndLine = pEnd->nLine;` |
|         - | 11607 | `	/* Swap token stream */` |
|      7959 | 11608 | `	pTmp = pGen->pEnd;` |
|      7959 | 11609 | `	pGen->pEnd = pEnd;` |
|         - | 11610 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|      7959 | 11611 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|         - | 11612 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|     55211 | 11613 | `	for(;;){` |
|    149855 | 11614 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     19721 | 11615 | `			pGen->pIn++;` |
|         5 | 11616 | `		}` |
|    130139 | 11617 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      7959 | 11618 | `			break;` |
|         - | 11619 | `		}` |
|         - | 11620 | `		/* Bind a directly-preceding docblock to this member */` |
|    122185 | 11621 | `		GenStateSetPendingDoc(&(*pGen));` |
|    122185 | 11622 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|       ! 0 | 11623 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11624 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 11625 | `				&pGen->pIn->sData,pName);` |
|       ! 0 | 11626 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 11627 | `				return SXERR_ABORT;` |
|         - | 11628 | `			}` |
|       ! 0 | 11629 | `			goto done;` |
|         - | 11630 | `		}` |
|    122185 | 11631 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|    122185 | 11632 | `		iAttrflags = 0;` |
|    122185 | 11633 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|    122185 | 11634 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    122185 | 11635 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|         - | 11636 | `				/* Trait uses another trait: use OtherTrait; */` |
|         5 | 11637 | `				pGen->pIn++; /* Jump 'use' */` |
|         2 | 11638 | `				for(;;){` |
|         - | 11639 | `					ph7_class *pUsedTrait;` |
|         - | 11640 | `					SyString *pUsedName;` |
|         5 | 11641 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       ! 0 | 11642 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 11643 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|       ! 0 | 11644 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11645 | `							return SXERR_ABORT;` |
|         - | 11646 | `						}` |
|       ! 0 | 11647 | `						break;` |
|         - | 11648 | `					}` |
|         5 | 11649 | `					pUsedName = &pGen->pIn->sData;` |
|         - | 11650 | `					{` |
|         - | 11651 | `						SyBlob sResolved;` |
|         5 | 11652 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|         5 | 11653 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|         7 | 11654 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|         4 | 11655 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|         5 | 11656 | `						SyBlobRelease(&sResolved);` |
|         - | 11657 | `					}` |
|         5 | 11658 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       ! 0 | 11659 | `						pUsedTrait = pUsedTrait->pNextName;` |
|       ! 0 | 11660 | `					}` |
|         5 | 11661 | `					if( pUsedTrait == 0 ){` |
|         4 | 11662 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         1 | 11663 | `							"'%z' is not a trait",pUsedName);` |
|         3 | 11664 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11665 | `							return SXERR_ABORT;` |
|         - | 11666 | `						}` |
|         2 | 11667 | `					}else{` |
|         3 | 11668 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|         - | 11669 | `					}` |
|         5 | 11670 | `					pGen->pIn++;` |
|         5 | 11671 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|         3 | 11672 | `						break;` |
|         - | 11673 | `					}` |
|       ! 0 | 11674 | `					pGen->pIn++;` |
|       ! 0 | 11675 | `				}` |
|         5 | 11676 | `				continue;` |
|         - | 11677 | `			}` |
|    122181 | 11678 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|    122165 | 11679 | `				iProtection = nKwrd;` |
|    122165 | 11680 | `				pGen->pIn++;` |
|    122160 | 11681 | `				if( pGen->pIn >= pGen->pEnd` |
|    122165 | 11682 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11683 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11684 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|       ! 0 | 11685 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 11686 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11687 | `						return SXERR_ABORT;` |
|         - | 11688 | `					}` |
|       ! 0 | 11689 | `					goto done;` |
|         - | 11690 | `				}` |
|    122165 | 11691 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     19707 | 11692 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     19707 | 11693 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11694 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11695 | `							return SXERR_ABORT;` |
|         - | 11696 | `						}` |
|       ! 0 | 11697 | `						goto done;` |
|         - | 11698 | `					}` |
|     19707 | 11699 | `					continue;` |
|         - | 11700 | `				}` |
|    102463 | 11701 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|         5 | 11702 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         5 | 11703 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 11704 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11705 | `							return SXERR_ABORT;` |
|         - | 11706 | `						}` |
|       ! 0 | 11707 | `						goto done;` |
|         - | 11708 | `					}` |
|         5 | 11709 | `					continue;` |
|         - | 11710 | `				}` |
|    102459 | 11711 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     51227 | 11712 | `			}` |
|    102475 | 11713 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       ! 0 | 11714 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11715 | `					"Traits cannot have constants");` |
|       ! 0 | 11716 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11717 | `					return SXERR_ABORT;` |
|         - | 11718 | `				}` |
|       ! 0 | 11719 | `				goto done;` |
|       ! 0 | 11720 | `			}else{` |
|    102475 | 11721 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|      7891 | 11722 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      7891 | 11723 | `					pGen->pIn++;` |
|      7891 | 11724 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      7889 | 11725 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      7889 | 11726 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       ! 0 | 11727 | `							iProtection = nKwrd;` |
|       ! 0 | 11728 | `							pGen->pIn++;` |
|       ! 0 | 11729 | `						}` |
|      3942 | 11730 | `					}` |
|      7886 | 11731 | `					if( pGen->pIn >= pGen->pEnd` |
|      7891 | 11732 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|       ! 0 | 11733 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11734 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|       ! 0 | 11735 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11736 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11737 | `							return SXERR_ABORT;` |
|         - | 11738 | `						}` |
|       ! 0 | 11739 | `						goto done;` |
|         - | 11740 | `					}` |
|      7891 | 11741 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|         3 | 11742 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|         3 | 11743 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 11744 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11745 | `								return SXERR_ABORT;` |
|         - | 11746 | `							}` |
|       ! 0 | 11747 | `							goto done;` |
|         - | 11748 | `						}` |
|         3 | 11749 | `						continue;` |
|         - | 11750 | `					}` |
|      7889 | 11751 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       ! 0 | 11752 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11753 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 11754 | `							if( rc == SXERR_ABORT ){` |
|       ! 0 | 11755 | `								return SXERR_ABORT;` |
|         - | 11756 | `							}` |
|       ! 0 | 11757 | `							goto done;` |
|         - | 11758 | `						}` |
|       ! 0 | 11759 | `						continue;` |
|         - | 11760 | `					}` |
|      7889 | 11761 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     98531 | 11762 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|         6 | 11763 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|         6 | 11764 | `					pGen->pIn++;` |
|         6 | 11765 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|         6 | 11766 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         6 | 11767 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|         6 | 11768 | `							iProtection = nKwrd;` |
|         6 | 11769 | `							pGen->pIn++;` |
|         2 | 11770 | `						}` |
|         2 | 11771 | `					}` |
|         6 | 11772 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|         4 | 11773 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|       ! 0 | 11774 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11775 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|       ! 0 | 11776 | `							&pGen->pIn->sData,pName);` |
|       ! 0 | 11777 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11778 | `							return SXERR_ABORT;` |
|         - | 11779 | `						}` |
|       ! 0 | 11780 | `						goto done;` |
|         - | 11781 | `					}` |
|         6 | 11782 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|         2 | 11783 | `				}` |
|    102473 | 11784 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|       ! 0 | 11785 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11786 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|       ! 0 | 11787 | `						&pGen->pIn->sData,pName);` |
|       ! 0 | 11788 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11789 | `						return SXERR_ABORT;` |
|         - | 11790 | `					}` |
|       ! 0 | 11791 | `					goto done;` |
|         - | 11792 | `				}` |
|    102473 | 11793 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       ! 0 | 11794 | `					pGen->pIn++;` |
|       ! 0 | 11795 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|       ! 0 | 11796 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 11797 | `							"Expecting attribute declaration after 'var' keyword");` |
|       ! 0 | 11798 | `						if( rc == SXERR_ABORT ){` |
|       ! 0 | 11799 | `							return SXERR_ABORT;` |
|         - | 11800 | `						}` |
|       ! 0 | 11801 | `						goto done;` |
|         - | 11802 | `					}` |
|       ! 0 | 11803 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11804 | `				}else{` |
|    102473 | 11805 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|         - | 11806 | `				}` |
|    102473 | 11807 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 11808 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 11809 | `						return SXERR_ABORT;` |
|         - | 11810 | `					}` |
|       ! 0 | 11811 | `					goto done;` |
|         - | 11812 | `				}` |
|         - | 11813 | `			}` |
|     51239 | 11814 | `		}else{` |
|       ! 0 | 11815 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       ! 0 | 11816 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 11817 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 11818 | `					return SXERR_ABORT;` |
|         - | 11819 | `				}` |
|       ! 0 | 11820 | `				goto done;` |
|         - | 11821 | `			}` |
|         - | 11822 | `		}` |
|         5 | 11823 | `	}` |
|         - | 11824 | `	/* Install the trait */` |
|      7959 | 11825 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      7959 | 11826 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 11827 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 11828 | `		return SXERR_ABORT;` |
|         - | 11829 | `	}` |
|      3977 | 11830 | `done:` |
|         - | 11831 | `	/* Point beyond the trait body */` |
|      7959 | 11832 | `	pGen->pIn = &pEnd[1];` |
|      7959 | 11833 | `	pGen->pEnd = pTmp;` |
|      7959 | 11834 | `	return PH7_OK;` |
|      3982 | 11835 | `}` |
|         - | 11836 | `/*` |
|         - | 11837 | ` * Compile a user-defined class.` |
|         - | 11838 | ` *  According to the PHP language reference manual` |
|         - | 11839 | ` *   Basic class definitions begin with the keyword class, followed` |
|         - | 11840 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|         - | 11841 | ` *   the definitions of the properties and methods belonging to the class.` |
|         - | 11842 | ` *   A class may contain its own constants, variables (called "properties")` |
|         - | 11843 | ` *   and functions (called "methods").` |
|         - | 11844 | ` */` |
|    324554 | 11845 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|         5 | 11846 | `{` |
|         - | 11847 | `	sxi32 rc;` |
|    324559 | 11848 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|    324559 | 11849 | `	return rc;` |
|         5 | 11850 | `}` |
|         - | 11851 | `/*` |
|         - | 11852 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|         - | 11853 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|         - | 11854 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|         - | 11855 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|         - | 11856 | `` * meaning; `enum Name` can never start a valid expression.`` |
|         - | 11857 | ` */` |
|  11077232 | 11858 | `static int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|         5 | 11859 | `{` |
|  11264664 | 11860 | `	return (pIn->nType & PH7_TK_ID)` |
|   5726043 | 11861 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|    197395 | 11862 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
|  11264659 | 11863 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|         5 | 11864 | `}` |
|         - | 11865 | `/*` |
|         - | 11866 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|         - | 11867 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|         - | 11868 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|         - | 11869 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|         - | 11870 | ` */` |
|      3966 | 11871 | `static sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|         5 | 11872 | `{` |
|      3971 | 11873 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|         5 | 11874 | `}` |
|         - | 11875 | `/*` |
|         - | 11876 | ` * Exception handling.` |
|         - | 11877 | ` *  According to the PHP language reference manual` |
|         - | 11878 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|         - | 11879 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|         - | 11880 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|         - | 11881 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|         - | 11882 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|         - | 11883 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|         - | 11884 | ` *    (or re-thrown) within a catch block.` |
|         - | 11885 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|         - | 11886 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|         - | 11887 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|         - | 11888 | ` *    been defined with set_exception_handler().` |
|         - | 11889 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|         - | 11890 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|         - | 11891 | ` */` |
|         - | 11892 | `/*` |
|         - | 11893 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|         - | 11894 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|         - | 11895 | ` * indicates failure.` |
|         - | 11896 | ` */` |
|    492662 | 11897 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|         5 | 11898 | `{` |
|    492667 | 11899 | `	sxi32 rc = SXRET_OK;` |
|    492667 | 11900 | `	if( pRoot->pOp ){` |
|    492655 | 11901 | `		switch( pRoot->pOp->iOp ){` |
|    246325 | 11902 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|         - | 11903 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|         - | 11904 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|         - | 11905 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|         - | 11906 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|         - | 11907 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|    492655 | 11908 | `			break;` |
|       ! 0 | 11909 | `		default:` |
|         - | 11910 | `			/* Runtime will still reject non-Throwable values; the set above` |
|         - | 11911 | `			 * covers the common shapes and gives a friendlier compile error` |
|         - | 11912 | ``			 * for obvious mistakes like `throw 5`. */`` |
|       ! 0 | 11913 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - | 11914 | `				"throw: Expecting an exception class instance");` |
|       ! 0 | 11915 | `			if( rc != SXERR_ABORT ){` |
|       ! 0 | 11916 | `				rc = SXERR_INVALID;` |
|       ! 0 | 11917 | `			}` |
|       ! 0 | 11918 | `			break;` |
|         - | 11919 | `		}` |
|    246342 | 11920 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|         - | 11921 | `		/* Unexpected expression */` |
|       ! 0 | 11922 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|         - | 11923 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 11924 | `		if( rc != SXERR_ABORT ){` |
|       ! 0 | 11925 | `			rc = SXERR_INVALID;` |
|       ! 0 | 11926 | `		}` |
|       ! 0 | 11927 | `	}` |
|    492667 | 11928 | `	return rc;` |
|         5 | 11929 | `}` |
|         - | 11930 | `/*` |
|         - | 11931 | ` * Compile a 'throw' statement.` |
|         - | 11932 | ` * throw: This is how you trigger an exception.` |
|         - | 11933 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|         - | 11934 | ` */` |
|    492626 | 11935 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|         5 | 11936 | `{` |
|    492631 | 11937 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11938 | `	GenBlock *pBlock;` |
|         - | 11939 | `	sxu32 nIdx;` |
|         - | 11940 | `	sxi32 rc;` |
|    492631 | 11941 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|         - | 11942 | `	/* Compile the expression */` |
|    492631 | 11943 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    492631 | 11944 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 | 11945 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|       ! 0 | 11946 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11947 | `			return SXERR_ABORT;` |
|         - | 11948 | `		}` |
|       ! 0 | 11949 | `		return SXRET_OK;` |
|         - | 11950 | `	}` |
|    492631 | 11951 | `	pBlock = pGen->pCurrent;` |
|         - | 11952 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   1946149 | 11953 | `	while(pBlock->pParent){` |
|   1946145 | 11954 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    492627 | 11955 | `			break;` |
|         - | 11956 | `		}` |
|         - | 11957 | `		/* Point to the parent block */` |
|   1453523 | 11958 | `		pBlock = pBlock->pParent;` |
|         5 | 11959 | `	}` |
|         - | 11960 | `	/* Emit the throw instruction */` |
|    492631 | 11961 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|         - | 11962 | `	/* Emit the jump */` |
|    492631 | 11963 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    492631 | 11964 | `	return SXRET_OK;` |
|    246318 | 11965 | `}` |
|         - | 11966 | `/*` |
|         - | 11967 | ` * Compile a PHP 8.0 'throw' expression.` |
|         - | 11968 | ` * Called from the expression code generator when a 'throw' keyword is` |
|         - | 11969 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|         - | 11970 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|         - | 11971 | ` * the validator guarantees the operand is a valid exception target.` |
|         - | 11972 | ` */` |
|        36 | 11973 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|         2 | 11974 | `{` |
|        38 | 11975 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 11976 | `	GenBlock *pBlock;` |
|         - | 11977 | `	sxu32 nIdx;` |
|         - | 11978 | `	sxi32 rc;` |
|        18 | 11979 | `	(void)iCompileFlag;` |
|        38 | 11980 | `	pGen->pIn++; /* Skip 'throw' */` |
|        38 | 11981 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 | 11982 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 11983 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 11984 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11985 | `			return SXERR_ABORT;` |
|         - | 11986 | `		}` |
|       ! 0 | 11987 | `		return SXRET_OK;` |
|         - | 11988 | `	}` |
|        38 | 11989 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|        38 | 11990 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 11991 | `		return SXERR_ABORT;` |
|         - | 11992 | `	}` |
|        38 | 11993 | `	if( rc == SXERR_EMPTY ){` |
|       ! 0 | 11994 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 11995 | `			"throw: Expecting an exception class instance");` |
|       ! 0 | 11996 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 11997 | `			return SXERR_ABORT;` |
|         - | 11998 | `		}` |
|       ! 0 | 11999 | `		return SXRET_OK;` |
|         - | 12000 | `	}` |
|         - | 12001 | `	/* Walk up to nearest exception/function block for the jump target */` |
|        38 | 12002 | `	pBlock = pGen->pCurrent;` |
|        60 | 12003 | `	while( pBlock->pParent ){` |
|        49 | 12004 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|        27 | 12005 | `			break;` |
|         - | 12006 | `		}` |
|        23 | 12007 | `		pBlock = pBlock->pParent;` |
|         1 | 12008 | `	}` |
|        38 | 12009 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        38 | 12010 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|        38 | 12011 | `	return SXRET_OK;` |
|        20 | 12012 | `}` |
|         - | 12013 | `/*` |
|         - | 12014 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|         - | 12015 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|         - | 12016 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|         - | 12017 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|         - | 12018 | ` * compile error propagated from the parser.` |
|         - | 12019 | ` */` |
|        54 | 12020 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|         5 | 12021 | `{` |
|         - | 12022 | `	SyString sClassName;` |
|         - | 12023 | `	SyToken *pToken;` |
|         - | 12024 | `	SyString *pName;` |
|         - | 12025 | `	char *zDup;` |
|         - | 12026 | `	sxi32 rc;` |
|        59 | 12027 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        59 | 12028 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|        59 | 12029 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|        59 | 12030 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        59 | 12031 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       ! 0 | 12032 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12033 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12034 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12035 | `		return SXERR_INVALID;` |
|         - | 12036 | `	}` |
|        59 | 12037 | `	pGen->pIn++; /* '(' */` |
|        27 | 12038 | `	for(;;){` |
|         - | 12039 | `		SyBlob sResolved;` |
|        59 | 12040 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|        59 | 12041 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       ! 0 | 12042 | `			SyBlobRelease(&sResolved);` |
|       ! 0 | 12043 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12044 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12045 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12046 | `			return SXERR_INVALID;` |
|         - | 12047 | `		}` |
|        86 | 12048 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        54 | 12049 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        59 | 12050 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|        59 | 12051 | `		SyBlobRelease(&sResolved);` |
|        59 | 12052 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|        59 | 12053 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|        59 | 12054 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        54 | 12055 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|         5 | 12056 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|       ! 0 | 12057 | `			pGen->pIn++; continue;` |
|         - | 12058 | `		}` |
|        59 | 12059 | `		break;` |
|       ! 0 | 12060 | `	}` |
|        54 | 12061 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|        59 | 12062 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       ! 0 | 12063 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12064 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12065 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12066 | `		return SXERR_INVALID;` |
|         - | 12067 | `	}` |
|        59 | 12068 | `	pGen->pIn++; /* '$' */` |
|        59 | 12069 | `	pName = &pGen->pIn->sData;` |
|        59 | 12070 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|        59 | 12071 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|        59 | 12072 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|        59 | 12073 | `	pGen->pIn++;` |
|        59 | 12074 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 | 12075 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|       ! 0 | 12076 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12077 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12078 | `		return SXERR_INVALID;` |
|         - | 12079 | `	}` |
|        59 | 12080 | `	pGen->pIn++; /* ')' */` |
|        59 | 12081 | `	return SXRET_OK;` |
|        32 | 12082 | `}` |
|         - | 12083 | `/*` |
|         - | 12084 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|         - | 12085 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|         - | 12086 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|         - | 12087 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|         - | 12088 | ` * VmThrowException):` |
|         - | 12089 | ` *` |
|         - | 12090 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|         - | 12091 | ` *    <try body>` |
|         - | 12092 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|         - | 12093 | ` *    JMP  -> finally\|end` |
|         - | 12094 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|         - | 12095 | ` *    <catch body>` |
|         - | 12096 | ` *    JMP  -> finally\|end` |
|         - | 12097 | ` *    ... more catches ...` |
|         - | 12098 | ` *  Lfin: <finally body>` |
|         - | 12099 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|         - | 12100 | ` *  Lend:` |
|         - | 12101 | ` */` |
|        98 | 12102 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|         5 | 12103 | `{` |
|       103 | 12104 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12105 | `	GenBlock *pTry;` |
|         - | 12106 | `	VmInstr *pInstr;` |
|       103 | 12107 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|         - | 12108 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|         - | 12109 | `	sxi32 rc;` |
|       103 | 12110 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|         - | 12111 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|       103 | 12112 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|       103 | 12113 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       103 | 12114 | `	pTry->pUserData = pException;` |
|       103 | 12115 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|       103 | 12116 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       103 | 12117 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       103 | 12118 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       103 | 12119 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       103 | 12120 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|         - | 12121 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|       103 | 12122 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|       103 | 12123 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|       103 | 12124 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       103 | 12125 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12126 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|       103 | 12127 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|         - | 12128 | `	/* Catch clauses (inline) */` |
|       103 | 12129 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        98 | 12130 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        59 | 12131 | `		sxu32 k = 0;` |
|        81 | 12132 | `		for(;;){` |
|         - | 12133 | `			ph7_exception_block sCatch;` |
|         - | 12134 | `			GenBlock *pCatchBlk;` |
|       113 | 12135 | `			sxu32 idxJmp = 0;` |
|       108 | 12136 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|       104 | 12137 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|        32 | 12138 | `				break;` |
|         - | 12139 | `			}` |
|        59 | 12140 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|        59 | 12141 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        59 | 12142 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|        59 | 12143 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|        59 | 12144 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|        59 | 12145 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|        59 | 12146 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|         - | 12147 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|         - | 12148 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|         - | 12149 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|        59 | 12150 | `			pCatchBlk->pUserData = pException;` |
|        59 | 12151 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|        59 | 12152 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        59 | 12153 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|        59 | 12154 | `			GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12155 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|         - | 12156 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|        59 | 12157 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        59 | 12158 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|        59 | 12159 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|        59 | 12160 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|        59 | 12161 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        59 | 12162 | `			k++;` |
|         5 | 12163 | `		}` |
|        27 | 12164 | `	}` |
|         - | 12165 | `	/* Finally (inline) */` |
|       103 | 12166 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        80 | 12167 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|         - | 12168 | `		GenBlock *pFinBlk;` |
|        52 | 12169 | `		pGen->pIn++; /* Jump 'finally' */` |
|        52 | 12170 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|        52 | 12171 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|        52 | 12172 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        52 | 12173 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|        52 | 12174 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|        52 | 12175 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|        52 | 12176 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        52 | 12177 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|        52 | 12178 | `		pException->iHasFinally = 1;` |
|        24 | 12179 | `	}` |
|       103 | 12180 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|       103 | 12181 | `	pException->iInlined = 1;` |
|         - | 12182 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|         - | 12183 | `	{` |
|       103 | 12184 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|         - | 12185 | `		sxu32 *aJ; sxu32 n;` |
|       103 | 12186 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|       103 | 12187 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       103 | 12188 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|       157 | 12189 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|        59 | 12190 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|        59 | 12191 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|        32 | 12192 | `		}` |
|         - | 12193 | `	}` |
|       103 | 12194 | `	SySetRelease(&aCatchJmp);` |
|       103 | 12195 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       ! 0 | 12196 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|       ! 0 | 12197 | `	}` |
|       103 | 12198 | `	return SXRET_OK;` |
|        54 | 12199 | `}` |
|         - | 12200 | `/*` |
|         - | 12201 | ` * Compile a 'catch' block.` |
|         - | 12202 | ` * Catch: A "catch" block retrieves an exception and creates` |
|         - | 12203 | ` * an object containing the exception information.` |
|         - | 12204 | ` */` |
|     25082 | 12205 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|         5 | 12206 | `{` |
|     25087 | 12207 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12208 | `	ph7_exception_block sCatch;` |
|         - | 12209 | `	SySet *pInstrContainer;` |
|         - | 12210 | `	SyString sClassName;` |
|         - | 12211 | `	GenBlock *pCatch;` |
|         - | 12212 | `	SyToken *pToken;` |
|         - | 12213 | `	SyString *pName;` |
|         - | 12214 | `	char *zDup;` |
|         - | 12215 | `	sxi32 rc;` |
|     25087 | 12216 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|         - | 12217 | `	/* Zero the structure */` |
|     25087 | 12218 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|         - | 12219 | `	/* Initialize fields */` |
|     25087 | 12220 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     25087 | 12221 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     25087 | 12222 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|         - | 12223 | `			/* Unexpected token,break immediately */` |
|       ! 0 | 12224 | `			pToken = pGen->pIn;` |
|       ! 0 | 12225 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12226 | `				pToken--;` |
|       ! 0 | 12227 | `			}` |
|       ! 0 | 12228 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12229 | `				"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12230 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12231 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12232 | `				return SXERR_ABORT;` |
|         - | 12233 | `			}` |
|       ! 0 | 12234 | `			return SXERR_INVALID;` |
|         - | 12235 | `	}` |
|         - | 12236 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     25087 | 12237 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     12555 | 12238 | `	for(;;){` |
|         - | 12239 | `		SyBlob sResolved;` |
|     25115 | 12240 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     25115 | 12241 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|         6 | 12242 | `			SyBlobRelease(&sResolved);` |
|         6 | 12243 | `			pToken = pGen->pIn;` |
|         6 | 12244 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12245 | `				pToken--;` |
|       ! 0 | 12246 | `			}` |
|         8 | 12247 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12248 | `				"syntax error, unexpected %s \"%z\"",` |
|         2 | 12249 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|         6 | 12250 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12251 | `				return SXERR_ABORT;` |
|         - | 12252 | `			}` |
|         6 | 12253 | `			return SXERR_INVALID;` |
|         - | 12254 | `		}` |
|         - | 12255 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|         - | 12256 | `		 * transient SyBlob allocation. */` |
|     37664 | 12257 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     25106 | 12258 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     25111 | 12259 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     25111 | 12260 | `		SyBlobRelease(&sResolved);` |
|     25111 | 12261 | `		if( zDup == 0 ){` |
|       ! 0 | 12262 | `			goto Mem;` |
|         - | 12263 | `		}` |
|     25111 | 12264 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     25111 | 12265 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 12266 | `			goto Mem;` |
|         - | 12267 | `		}` |
|         - | 12268 | `		/* Check for '\|' (multi-catch separator) */` |
|     25106 | 12269 | `		if( pGen->pIn < pGen->pEnd &&` |
|     25106 | 12270 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|        33 | 12271 | `			pGen->pIn->sData.nByte == 1 &&` |
|        28 | 12272 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|        30 | 12273 | `			pGen->pIn++; /* Consume the '\|' */` |
|        30 | 12274 | `			continue;` |
|         - | 12275 | `		}` |
|     25083 | 12276 | `		break;` |
|       ! 0 | 12277 | `	}` |
|     25078 | 12278 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     25083 | 12279 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|         - | 12280 | `			/* Unexpected token,break immediately */` |
|       ! 0 | 12281 | `			pToken = pGen->pIn;` |
|       ! 0 | 12282 | `			if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12283 | `				pToken--;` |
|       ! 0 | 12284 | `			}` |
|       ! 0 | 12285 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12286 | `				"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12287 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12288 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12289 | `				return SXERR_ABORT;` |
|         - | 12290 | `			}` |
|       ! 0 | 12291 | `			return SXERR_INVALID;` |
|         - | 12292 | `	}` |
|     25083 | 12293 | `	pGen->pIn++; /* Jump the dollar sign */` |
|         - | 12294 | `	/* Duplicate instance name */` |
|     25083 | 12295 | `	pName = &pGen->pIn->sData;` |
|     25083 | 12296 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     25083 | 12297 | `	if( zDup == 0 ){` |
|       ! 0 | 12298 | `		goto Mem;` |
|         - | 12299 | `	}` |
|     25083 | 12300 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     25083 | 12301 | `	pGen->pIn++;` |
|     25083 | 12302 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|         - | 12303 | `		/* Unexpected token,break immediately */` |
|       ! 0 | 12304 | `		pToken = pGen->pIn;` |
|       ! 0 | 12305 | `		if( pToken >= pGen->pEnd ){` |
|       ! 0 | 12306 | `			pToken--;` |
|       ! 0 | 12307 | `		}` |
|       ! 0 | 12308 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|         - | 12309 | `			"syntax error, unexpected %s \"%z\"",` |
|       ! 0 | 12310 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|       ! 0 | 12311 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12312 | `			return SXERR_ABORT;` |
|         - | 12313 | `		}` |
|       ! 0 | 12314 | `		return SXERR_INVALID;` |
|         - | 12315 | `	}` |
|         - | 12316 | `	/* Compile the block */` |
|     25083 | 12317 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|         - | 12318 | `	/* Create the catch block */` |
|     25083 | 12319 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     25083 | 12320 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12321 | `		return SXERR_ABORT;` |
|         - | 12322 | `	}` |
|         - | 12323 | `	/* Swap bytecode container */` |
|     25083 | 12324 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     25083 | 12325 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|         - | 12326 | `	/* Compile the block */` |
|     25083 | 12327 | `	PH7_CompileBlock(&(*pGen),0);` |
|         - | 12328 | `	/* Fix forward jumps now the destination is resolved  */` |
|     25083 | 12329 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12330 | `	/* Emit the DONE instruction */` |
|     25083 | 12331 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - | 12332 | `	/* Leave the block */` |
|     25083 | 12333 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12334 | `	/* Restore the default container */` |
|     25083 | 12335 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - | 12336 | `	/* Install the catch block */` |
|     25083 | 12337 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     25083 | 12338 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12339 | `		goto Mem;` |
|         - | 12340 | `	}` |
|     25083 | 12341 | `	return SXRET_OK;` |
|       ! 0 | 12342 | `Mem:` |
|       ! 0 | 12343 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 12344 | `	return SXERR_ABORT;` |
|     12546 | 12345 | `}` |
|         - | 12346 | `/*` |
|         - | 12347 | ` * Compile a 'try' block.` |
|         - | 12348 | ` * A function using an exception should be in a "try" block.` |
|         - | 12349 | ` * If the exception does not trigger, the code will continue` |
|         - | 12350 | ` * as normal. However if the exception triggers, an exception` |
|         - | 12351 | ` * is "thrown".` |
|         - | 12352 | ` */` |
|     25238 | 12353 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|         5 | 12354 | `{` |
|         - | 12355 | `	ph7_exception *pException;` |
|     25243 | 12356 | `	sxu32 nLine = pGen->pIn->nLine;` |
|         - | 12357 | `	GenBlock *pTry;` |
|         - | 12358 | `	sxu32 nJmpIdx;` |
|         - | 12359 | `	sxi32 rc;` |
|         - | 12360 | `	/* Create the exception container */` |
|     25243 | 12361 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     25243 | 12362 | `	if( pException == 0 ){` |
|       ! 0 | 12363 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|       ! 0 | 12364 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|       ! 0 | 12365 | `		return SXERR_ABORT;` |
|         - | 12366 | `	}` |
|         - | 12367 | `	/* Zero the structure */` |
|     25243 | 12368 | `	SyZero(pException,sizeof(ph7_exception));` |
|         - | 12369 | `	/* Initialize fields */` |
|     25243 | 12370 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     25243 | 12371 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     25243 | 12372 | `	pException->iHasFinally = 0;` |
|     25243 | 12373 | `	pException->iFinallyDone = 0;` |
|     25243 | 12374 | `	pException->pVm = pGen->pVm;` |
|         - | 12375 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|         - | 12376 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|         - | 12377 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|         - | 12378 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|         - | 12379 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|         - | 12380 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|     25243 | 12381 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|       103 | 12382 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|         - | 12383 | `	}` |
|         - | 12384 | `	/* Create the try block */` |
|     25145 | 12385 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     25145 | 12386 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12387 | `		return SXERR_ABORT;` |
|         - | 12388 | `	}` |
|         - | 12389 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     25145 | 12390 | `	pTry->pUserData = pException;` |
|         - | 12391 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     25145 | 12392 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|         - | 12393 | `	/* Fix the jump later when the destination is resolved */` |
|     25145 | 12394 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     25145 | 12395 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|         - | 12396 | `	/* Compile the block */` |
|     25145 | 12397 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     25145 | 12398 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12399 | `		return SXERR_ABORT;` |
|         - | 12400 | `	}` |
|         - | 12401 | `	/* Fix forward jumps now the destination is resolved */` |
|     25145 | 12402 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12403 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     25145 | 12404 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|         - | 12405 | `	/* Leave the block */` |
|     25145 | 12406 | `	GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12407 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     25145 | 12408 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     25138 | 12409 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|         - | 12410 | `		/* Compile one or more catch blocks */` |
|     25078 | 12411 | `		for(;;){` |
|     50156 | 12412 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     37688 | 12413 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     12542 | 12414 | `					break;` |
|         - | 12415 | `			}` |
|     25087 | 12416 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     25087 | 12417 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12418 | `				return SXERR_ABORT;` |
|         - | 12419 | `			}` |
|         5 | 12420 | `		}` |
|     12537 | 12421 | `	}` |
|         - | 12422 | `	/* Compile optional finally block */` |
|     25145 | 12423 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       726 | 12424 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|         - | 12425 | `		SySet *pInstrContainer;` |
|         - | 12426 | `		GenBlock *pFinBlock;` |
|       129 | 12427 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|         - | 12428 | `		/* Create the finally block for jump fixup bookkeeping */` |
|       129 | 12429 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|       129 | 12430 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 12431 | `			return SXERR_ABORT;` |
|         - | 12432 | `		}` |
|         - | 12433 | `		/* Swap bytecode container */` |
|       129 | 12434 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       129 | 12435 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|         - | 12436 | `		/* Compile the finally body */` |
|       129 | 12437 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       129 | 12438 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12439 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       ! 0 | 12440 | `			return SXERR_ABORT;` |
|         - | 12441 | `		}` |
|         - | 12442 | `		/* Fix forward jumps now the destination is resolved */` |
|       129 | 12443 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12444 | `		/* Emit DONE to terminate the finally block */` |
|       129 | 12445 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|         - | 12446 | `		/* Leave the block */` |
|       129 | 12447 | `		GenStateLeaveBlock(&(*pGen),0);` |
|         - | 12448 | `		/* Restore the default container */` |
|       129 | 12449 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       129 | 12450 | `		pException->iHasFinally = 1;` |
|        62 | 12451 | `	}` |
|         - | 12452 | `	/* Must have at least one catch or finally */` |
|     25145 | 12453 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|         9 | 12454 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 12455 | `			"Cannot use try without catch or finally");` |
|         9 | 12456 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12457 | `			return SXERR_ABORT;` |
|         - | 12458 | `		}` |
|         3 | 12459 | `	}` |
|     25145 | 12460 | `	return SXRET_OK;` |
|     12624 | 12461 | `}` |
|         - | 12462 | `/*` |
|         - | 12463 | ` * Compile a switch block.` |
|         - | 12464 | ` *  (See block-comment below for more information)` |
|         - | 12465 | ` */` |
|       112 | 12466 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|         5 | 12467 | `{` |
|       117 | 12468 | `	sxi32 rc = SXRET_OK;` |
|       117 | 12469 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|         - | 12470 | `		/* Unexpected token */` |
|       ! 0 | 12471 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 | 12472 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12473 | `			return SXERR_ABORT;` |
|         - | 12474 | `		}` |
|       ! 0 | 12475 | `		pGen->pIn++;` |
|       ! 0 | 12476 | `	}` |
|       117 | 12477 | `	pGen->pIn++;` |
|         - | 12478 | `	/* First instruction to execute in this block. */` |
|       117 | 12479 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|         - | 12480 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|         - | 12481 | `	 * or the '}' token */` |
|       206 | 12482 | `	for(;;){` |
|       417 | 12483 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 12484 | `			/* No more input to process */` |
|       ! 0 | 12485 | `			break;` |
|         - | 12486 | `		}` |
|       417 | 12487 | `		rc = SXRET_OK;` |
|       417 | 12488 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|        85 | 12489 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|        31 | 12490 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|         - | 12491 | `					/* Unexpected token */` |
|       ! 0 | 12492 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|       ! 0 | 12493 | `						&pGen->pIn->sData);` |
|       ! 0 | 12494 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12495 | `						return SXERR_ABORT;` |
|         - | 12496 | `					}` |
|         - | 12497 | `					/* FALL THROUGH */` |
|       ! 0 | 12498 | `				}` |
|        31 | 12499 | `				rc = SXERR_EOF;` |
|        31 | 12500 | `				break;` |
|         - | 12501 | `			}` |
|        32 | 12502 | `		}else{` |
|         - | 12503 | `			sxi32 nKwrd;` |
|         - | 12504 | `			/* Extract the keyword */` |
|       337 | 12505 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       337 | 12506 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|        47 | 12507 | `				break;` |
|         - | 12508 | `			}` |
|       253 | 12509 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|         3 | 12510 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|         - | 12511 | `					/* Unexpected token */` |
|       ! 0 | 12512 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|       ! 0 | 12513 | `						&pGen->pIn->sData);` |
|       ! 0 | 12514 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 12515 | `						return SXERR_ABORT;` |
|         - | 12516 | `					}` |
|         - | 12517 | `					/* FALL THROUGH */` |
|       ! 0 | 12518 | `				}` |
|         - | 12519 | `				/* Block compiled */` |
|         3 | 12520 | `				break;` |
|         - | 12521 | `			}` |
|         - | 12522 | `		}` |
|         - | 12523 | `		/* Compile block */` |
|       305 | 12524 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       305 | 12525 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12526 | `			return SXERR_ABORT;` |
|         - | 12527 | `		}` |
|         5 | 12528 | `	}` |
|       117 | 12529 | `	return rc;` |
|        61 | 12530 | `}` |
|         - | 12531 | `/*` |
|         - | 12532 | ` * Compile a case eXpression.` |
|         - | 12533 | ` *  (See block-comment below for more information)` |
|         - | 12534 | ` */` |
|        92 | 12535 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|         5 | 12536 | `{` |
|         - | 12537 | `	SySet *pInstrContainer;` |
|         - | 12538 | `	SyToken *pEnd,*pTmp;` |
|        97 | 12539 | `	sxi32 iNest = 0;` |
|         - | 12540 | `	sxi32 rc;` |
|         - | 12541 | `	/* Delimit the expression */` |
|        97 | 12542 | `	pEnd = pGen->pIn;` |
|       197 | 12543 | `	while( pEnd < pGen->pEnd ){` |
|       197 | 12544 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|         - | 12545 | `			/* Increment nesting level */` |
|         3 | 12546 | `			iNest++;` |
|       196 | 12547 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|         - | 12548 | `			/* Decrement nesting level */` |
|         3 | 12549 | `			iNest--;` |
|       194 | 12550 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|        97 | 12551 | `			break;` |
|         - | 12552 | `		}` |
|       105 | 12553 | `		pEnd++;` |
|         5 | 12554 | `	}` |
|        97 | 12555 | `	if( pGen->pIn >= pEnd ){` |
|       ! 0 | 12556 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|       ! 0 | 12557 | `		if( rc == SXERR_ABORT ){` |
|         - | 12558 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12559 | `			return SXERR_ABORT;` |
|         - | 12560 | `		}` |
|       ! 0 | 12561 | `	}` |
|         - | 12562 | `	/* Swap token stream */` |
|        97 | 12563 | `	pTmp = pGen->pEnd;` |
|        97 | 12564 | `	pGen->pEnd = pEnd;` |
|        97 | 12565 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|        97 | 12566 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|        97 | 12567 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|         - | 12568 | `	/* Emit the done instruction */` |
|        97 | 12569 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        97 | 12570 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|         - | 12571 | `	/* Update token stream */` |
|        97 | 12572 | `	pGen->pIn  = pEnd;` |
|        97 | 12573 | `	pGen->pEnd = pTmp;` |
|        97 | 12574 | `	if( rc == SXERR_ABORT ){` |
|       ! 0 | 12575 | `		return SXERR_ABORT;` |
|         - | 12576 | `	}` |
|        97 | 12577 | `	return SXRET_OK;` |
|        51 | 12578 | `}` |
|         - | 12579 | `/*` |
|         - | 12580 | ` * Compile the smart switch statement.` |
|         - | 12581 | ` * According to the PHP language reference manual` |
|         - | 12582 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|         - | 12583 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|         - | 12584 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|         - | 12585 | ` *  This is exactly what the switch statement is for.` |
|         - | 12586 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|         - | 12587 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|         - | 12588 | ` *  of the outer loop, use continue 2.` |
|         - | 12589 | ` *  Note that switch/case does loose comparision.` |
|         - | 12590 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|         - | 12591 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|         - | 12592 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|         - | 12593 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|         - | 12594 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|         - | 12595 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|         - | 12596 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|         - | 12597 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|         - | 12598 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|         - | 12599 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|         - | 12600 | ` *  list for the next case.` |
|         - | 12601 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|         - | 12602 | ` *  or floating-point numbers and strings.` |
|         - | 12603 | ` */` |
|        28 | 12604 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|         5 | 12605 | `{` |
|         - | 12606 | `	GenBlock *pSwitchBlock;` |
|         - | 12607 | `	SyToken *pTmp,*pEnd;` |
|         - | 12608 | `	ph7_switch *pSwitch;` |
|         - | 12609 | `	sxu32 nToken;` |
|         - | 12610 | `	sxu32 nLine;` |
|         - | 12611 | `	sxi32 rc;` |
|        33 | 12612 | `	nLine = pGen->pIn->nLine;` |
|         - | 12613 | `	/* Jump the 'switch' keyword */` |
|        33 | 12614 | `	pGen->pIn++;` |
|        33 | 12615 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         - | 12616 | `		/* Syntax error */` |
|       ! 0 | 12617 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|       ! 0 | 12618 | `		if( rc == SXERR_ABORT ){` |
|         - | 12619 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12620 | `			return SXERR_ABORT;` |
|         - | 12621 | `		}` |
|       ! 0 | 12622 | `		goto Synchronize;` |
|         - | 12623 | `	}` |
|         - | 12624 | `	/* Jump the left parenthesis '(' */` |
|        33 | 12625 | `	pGen->pIn++;` |
|        33 | 12626 | `	pEnd = 0; /* cc warning */` |
|         - | 12627 | `	/* Create the loop block */` |
|        47 | 12628 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|        14 | 12629 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|        33 | 12630 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 12631 | `		return SXERR_ABORT;` |
|         - | 12632 | `	}` |
|         - | 12633 | `	/* Delimit the condition */` |
|        33 | 12634 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|        33 | 12635 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|         - | 12636 | `		/* Empty expression */` |
|       ! 0 | 12637 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|       ! 0 | 12638 | `		if( rc == SXERR_ABORT ){` |
|         - | 12639 | `			/* Error count limit reached,abort immediately */` |
|       ! 0 | 12640 | `			return SXERR_ABORT;` |
|         - | 12641 | `		}` |
|       ! 0 | 12642 | `	}` |
|         - | 12643 | `	/* Swap token streams */` |
|        33 | 12644 | `	pTmp = pGen->pEnd;` |
|        33 | 12645 | `	pGen->pEnd = pEnd;` |
|         - | 12646 | `	/* Compile the expression */` |
|        33 | 12647 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        33 | 12648 | `	if( rc == SXERR_ABORT ){` |
|         - | 12649 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|       ! 0 | 12650 | `		return SXERR_ABORT;` |
|         - | 12651 | `	}` |
|         - | 12652 | `	/* Update token stream */` |
|        33 | 12653 | `	while(pGen->pIn < pEnd ){` |
|       ! 0 | 12654 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       ! 0 | 12655 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|       ! 0 | 12656 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12657 | `			return SXERR_ABORT;` |
|         - | 12658 | `		}` |
|       ! 0 | 12659 | `		pGen->pIn++;` |
|       ! 0 | 12660 | `	}` |
|        33 | 12661 | `	pGen->pIn  = &pEnd[1];` |
|        33 | 12662 | `	pGen->pEnd = pTmp;` |
|        33 | 12663 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|        28 | 12664 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|       ! 0 | 12665 | `			pTmp = pGen->pIn;` |
|       ! 0 | 12666 | `			if( pTmp >= pGen->pEnd ){` |
|       ! 0 | 12667 | `				pTmp--;` |
|       ! 0 | 12668 | `			}` |
|         - | 12669 | `			/* Unexpected token */` |
|       ! 0 | 12670 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|       ! 0 | 12671 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12672 | `				return SXERR_ABORT;` |
|         - | 12673 | `			}` |
|       ! 0 | 12674 | `			goto Synchronize;` |
|         - | 12675 | `	}` |
|         - | 12676 | `	/* Set the delimiter token */` |
|        33 | 12677 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|         3 | 12678 | `		nToken = PH7_TK_KEYWORD;` |
|         - | 12679 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|         2 | 12680 | `	}else{` |
|        31 | 12681 | `		nToken = PH7_TK_CCB; /* '}' */` |
|         - | 12682 | `	}` |
|        33 | 12683 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|         - | 12684 | `	/* Create the switch blocks container */` |
|        33 | 12685 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|        33 | 12686 | `	if( pSwitch == 0 ){` |
|         - | 12687 | `		/* Abort compilation */` |
|       ! 0 | 12688 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|       ! 0 | 12689 | `		return SXERR_ABORT;` |
|         - | 12690 | `	}` |
|         - | 12691 | `	/* Zero the structure */` |
|        33 | 12692 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|         - | 12693 | `	/* Initialize fields */` |
|        33 | 12694 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|         - | 12695 | `	/* Emit the switch instruction */` |
|        33 | 12696 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|         - | 12697 | `	/* Compile case blocks */` |
|       100 | 12698 | `	for(;;){` |
|         - | 12699 | `		sxu32 nKwrd;` |
|       119 | 12700 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 12701 | `			/* No more input to process */` |
|       ! 0 | 12702 | `			break;` |
|         - | 12703 | `		}` |
|       119 | 12704 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       ! 0 | 12705 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|         - | 12706 | `				/* Unexpected token */` |
|       ! 0 | 12707 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 12708 | `					&pGen->pIn->sData);` |
|       ! 0 | 12709 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12710 | `					return SXERR_ABORT;` |
|         - | 12711 | `				}` |
|         - | 12712 | `				/* FALL THROUGH */` |
|       ! 0 | 12713 | `			}` |
|         - | 12714 | `			/* Block compiled */` |
|       ! 0 | 12715 | `			break;` |
|         - | 12716 | `		}` |
|         - | 12717 | `		/* Extract the keyword */` |
|       119 | 12718 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       119 | 12719 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|         3 | 12720 | `			if( nToken != PH7_TK_KEYWORD ){` |
|         - | 12721 | `				/* Unexpected token */` |
|       ! 0 | 12722 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 12723 | `					&pGen->pIn->sData);` |
|       ! 0 | 12724 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12725 | `					return SXERR_ABORT;` |
|         - | 12726 | `				}` |
|         - | 12727 | `				/* FALL THROUGH */` |
|       ! 0 | 12728 | `			}` |
|         - | 12729 | `			/* Block compiled */` |
|         3 | 12730 | `			break;` |
|         - | 12731 | `		}` |
|       117 | 12732 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|         - | 12733 | `			/*` |
|         - | 12734 | `			 * Accroding to the PHP language reference manual` |
|         - | 12735 | `			 *  A special case is the default case. This case matches anything` |
|         - | 12736 | `			 *  that wasn't matched by the other cases.` |
|         - | 12737 | `			 */` |
|        25 | 12738 | `			if( pSwitch->nDefault > 0 ){` |
|         - | 12739 | `				/* Default case already compiled */` |
|       ! 0 | 12740 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|       ! 0 | 12741 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 12742 | `					return SXERR_ABORT;` |
|         - | 12743 | `				}` |
|       ! 0 | 12744 | `			}` |
|        25 | 12745 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|         - | 12746 | `			/* Compile the default block */` |
|        25 | 12747 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|        25 | 12748 | `			if( rc == SXERR_ABORT){` |
|       ! 0 | 12749 | `				return SXERR_ABORT;` |
|        25 | 12750 | `			}else if( rc == SXERR_EOF ){` |
|        23 | 12751 | `				break;` |
|         1 | 12752 | `			}` |
|        98 | 12753 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|         - | 12754 | `			ph7_case_expr sCase;` |
|         - | 12755 | `			/* Standard case block */` |
|        97 | 12756 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|         - | 12757 | `			/* initialize the structure */` |
|        97 | 12758 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|         - | 12759 | `			/* Compile the case expression */` |
|        97 | 12760 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|        97 | 12761 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12762 | `				return SXERR_ABORT;` |
|         - | 12763 | `			}` |
|         - | 12764 | `			/* Compile the case block */` |
|        97 | 12765 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|         - | 12766 | `			/* Insert in the switch container */` |
|        97 | 12767 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|        97 | 12768 | `			if( rc == SXERR_ABORT){` |
|       ! 0 | 12769 | `				return SXERR_ABORT;` |
|        97 | 12770 | `			}else if( rc == SXERR_EOF ){` |
|         9 | 12771 | `				break;` |
|         - | 12772 | `			}` |
|        47 | 12773 | `		}else{` |
|         - | 12774 | `			/* Unexpected token */` |
|       ! 0 | 12775 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|       ! 0 | 12776 | `				&pGen->pIn->sData);` |
|       ! 0 | 12777 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 12778 | `				return SXERR_ABORT;` |
|         - | 12779 | `			}` |
|       ! 0 | 12780 | `			break;` |
|         - | 12781 | `		}` |
|         5 | 12782 | `	}` |
|         - | 12783 | `	/* Fix all jumps now the destination is resolved */` |
|        33 | 12784 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|        33 | 12785 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|         - | 12786 | `	/* Release the loop block */` |
|        33 | 12787 | `	GenStateLeaveBlock(pGen,0);` |
|        33 | 12788 | `	if( pGen->pIn < pGen->pEnd ){` |
|         - | 12789 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|        33 | 12790 | `		pGen->pIn++;` |
|        14 | 12791 | `	}` |
|         - | 12792 | `	/* Statement successfully compiled */` |
|        33 | 12793 | `	return SXRET_OK;` |
|       ! 0 | 12794 | `Synchronize:` |
|         - | 12795 | `	/* Synchronize with the first semi-colon */` |
|       ! 0 | 12796 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       ! 0 | 12797 | `		pGen->pIn++;` |
|       ! 0 | 12798 | `	}` |
|       ! 0 | 12799 | `	return SXRET_OK;` |
|        19 | 12800 | `}` |
|         - | 12801 | `/*` |
|         - | 12802 | ` * Chain operators participate in a postfix member-access chain.` |
|         - | 12803 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|         - | 12804 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|         - | 12805 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|         - | 12806 | ` */` |
|         - | 12807 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|         - | 12808 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|         - | 12809 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|         - | 12810 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|         - | 12811 |  |
|         - | 12812 | `/*` |
|         - | 12813 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|         - | 12814 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|         - | 12815 | ` * patched entries from the pending set.` |
|         - | 12816 | ` */` |
|  41107252 | 12817 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|         5 | 12818 | `{` |
|  41107257 | 12819 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|         - | 12820 | `	sxu32 nTarget;` |
|         - | 12821 | `	sxu32 *aIdx;` |
|         - | 12822 | `	sxu32 i;` |
|  41107257 | 12823 | `	if( nCur <= nBaseline ){` |
|  41107161 | 12824 | `		return;` |
|         - | 12825 | `	}` |
|       100 | 12826 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|       100 | 12827 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|       204 | 12828 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|       108 | 12829 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|       108 | 12830 | `		if( pInstr ){` |
|       108 | 12831 | `			pInstr->iP2 = (sxi32)nTarget;` |
|        52 | 12832 | `		}` |
|        56 | 12833 | `	}` |
|       100 | 12834 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
|  20553631 | 12835 | `}` |
|         - | 12836 |  |
|         - | 12837 | `/*` |
|         - | 12838 | ` * By-reference out-parameters of builtin functions.` |
|         - | 12839 | ` *` |
|         - | 12840 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|         - | 12841 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|         - | 12842 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|         - | 12843 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|         - | 12844 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|         - | 12845 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|         - | 12846 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|         - | 12847 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|         - | 12848 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|         - | 12849 | ` * creates it" behaviour).` |
|         - | 12850 | ` *` |
|         - | 12851 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|         - | 12852 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|         - | 12853 | ` */` |
|   5585756 | 12854 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|         5 | 12855 | `{` |
|         - | 12856 | `	static const struct {` |
|         - | 12857 | `		const char *zName;` |
|         - | 12858 | `		sxu32 nByte;` |
|         - | 12859 | `		sxu32 mask;` |
|         - | 12860 | `	} aByRef[] = {` |
|         - | 12861 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - | 12862 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - | 12863 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - | 12864 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - | 12865 | `		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */` |
|         - | 12866 | `		{ "fsockopen",              9, (1u<<2)\|(1u<<3) },  /* &$error_code, &$error_message */` |
|         - | 12867 | `		{ "pfsockopen",            10, (1u<<2)\|(1u<<3) },  /* same */` |
|         - | 12868 | `		{ "stream_socket_client",  20, (1u<<1)\|(1u<<2) },  /* &$error_code, &$error_message */` |
|         - | 12869 | `	};` |
|         - | 12870 | `	sxu32 i;` |
|   5585761 | 12871 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|   1646723 | 12872 | `		return 0;` |
|         - | 12873 | `	}` |
|  35147331 | 12874 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
|  31247798 | 12875 | `		if( pName->nByte == aByRef[i].nByte` |
|  16328482 | 12876 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|     39515 | 12877 | `			return aByRef[i].mask;` |
|         - | 12878 | `		}` |
|  15604149 | 12879 | `	}` |
|   3899533 | 12880 | `	return 0;` |
|   2792883 | 12881 | `}` |
|         - | 12882 | `/*` |
|         - | 12883 | ` * Recover the bare global-builtin name from a call's callee node.` |
|         - | 12884 | ` *` |
|         - | 12885 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|         - | 12886 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|         - | 12887 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|         - | 12888 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|         - | 12889 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|         - | 12890 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|         - | 12891 | ` */` |
|   5585756 | 12892 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|         5 | 12893 | `{` |
|         - | 12894 | `	SyToken *p, *pEnd;` |
|   5585761 | 12895 | `	pOut->zString = 0;` |
|   5585761 | 12896 | `	pOut->nByte = 0;` |
|   5585761 | 12897 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|       ! 0 | 12898 | `		return;` |
|         - | 12899 | `	}` |
|   5585761 | 12900 | `	p = pLeft->pStart;` |
|   5585761 | 12901 | `	pEnd = pLeft->pEnd;` |
|         - | 12902 | `	/* Optional single leading namespace separator (absolute path). */` |
|   5585761 | 12903 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      3971 | 12904 | `		p++;` |
|      1983 | 12905 | `	}` |
|   5585761 | 12906 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|   1646687 | 12907 | `		return;` |
|         - | 12908 | `	}` |
|         - | 12909 | `	/* Must be a single component: nothing follows the name token. */` |
|   3939079 | 12910 | `	if( p + 1 != pEnd ){` |
|        40 | 12911 | `		return;` |
|         - | 12912 | `	}` |
|   3939043 | 12913 | `	*pOut = p->sData;` |
|   2792883 | 12914 | `}` |
|         - | 12915 | `/*` |
|         - | 12916 | ` * Generate bytecode for a given expression tree.` |
|         - | 12917 | ` * If something goes wrong while generating bytecode` |
|         - | 12918 | ` * for the expression tree (A very unlikely scenario)` |
|         - | 12919 | ` * this function takes care of generating the appropriate` |
|         - | 12920 | ` * error message.` |
|         - | 12921 | ` */` |
|  59390698 | 12922 | `static sxi32 GenStateEmitExprCode(` |
|         - | 12923 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 12924 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|         - | 12925 | `	sxi32 iFlags /* Control flags */` |
|         - | 12926 | `	)` |
|         5 | 12927 | `{` |
|         - | 12928 | `	VmInstr *pInstr;` |
|         - | 12929 | `	sxu32 nJmpIdx;` |
|  59390703 | 12930 | `	sxi32 iP1 = 0;` |
|  59390703 | 12931 | `	sxu32 iP2 = 0;` |
|  59390703 | 12932 | `	void *p3  = 0;` |
|         - | 12933 | `	sxi32 iVmOp;` |
|         - | 12934 | `	sxi32 rc;` |
|  59390703 | 12935 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
|  59390703 | 12936 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
|  59390703 | 12937 | `	sxu32 nRhsNsBase = 0;` |
|  59390703 | 12938 | `	if( pNode->xCode ){` |
|         - | 12939 | `		SyToken *pTmpIn,*pTmpEnd;` |
|         - | 12940 | `		/* Compile node */` |
|  35523301 | 12941 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|  35523301 | 12942 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|  35523301 | 12943 | `		RE_SWAP_DELIMITER(pGen);` |
|  35523301 | 12944 | `		return rc;` |
|         - | 12945 | `	}` |
|  23867407 | 12946 | `	if( pNode->pOp == 0 ){` |
|       ! 0 | 12947 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 12948 | `			"Invalid expression node,PH7 is aborting compilation");` |
|       ! 0 | 12949 | `		return SXERR_ABORT;` |
|         - | 12950 | `	}` |
|  23867407 | 12951 | `	iVmOp = pNode->pOp->iVmOp;` |
|  23867407 | 12952 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|         - | 12953 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|         - | 12954 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|         - | 12955 | `		 * and later errors are still reported. */` |
|         3 | 12956 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 12957 | `			"The (unset) cast is no longer supported");` |
|         3 | 12958 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 12959 | `			return SXERR_ABORT;` |
|         - | 12960 | `		}` |
|         1 | 12961 | `	}` |
|  23867407 | 12962 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        91 | 12963 | `		sxu32 nJmp = 0;` |
|         - | 12964 | `		sxu32 nNcNsBase;` |
|         - | 12965 | `		VmInstr *pInstrFix;` |
|         - | 12966 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|         - | 12967 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|         - | 12968 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|         - | 12969 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|         - | 12970 | `		 * stack slot carries a writable nIdx. */` |
|        91 | 12971 | `		if( pNode->pRight ){` |
|        91 | 12972 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        91 | 12973 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|        91 | 12974 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 12975 | `				return rc;` |
|         - | 12976 | `			}` |
|        91 | 12977 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|         - | 12978 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|         - | 12979 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|         - | 12980 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|         - | 12981 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|         - | 12982 | `			 * the store, so the parent array does not need to be copied at` |
|         - | 12983 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|         - | 12984 | `			 * cascade for the actual write path stays correct. */` |
|        91 | 12985 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|        91 | 12986 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|        33 | 12987 | `				pInstrFix->iP2 = 3;` |
|        15 | 12988 | `			}` |
|        44 | 12989 | `		}` |
|         - | 12990 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|        91 | 12991 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|         - | 12992 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|        91 | 12993 | `		if( pNode->pLeft ){` |
|        91 | 12994 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        91 | 12995 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|        91 | 12996 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 12997 | `				return rc;` |
|         - | 12998 | `			}` |
|        91 | 12999 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        44 | 13000 | `		}` |
|         - | 13001 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|        91 | 13002 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|         - | 13003 | `		/* Patch the short-circuit jump to land after the store. */` |
|        91 | 13004 | `		if( nJmp > 0 ){` |
|        91 | 13005 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|        91 | 13006 | `			if( pInstrFix ){` |
|        91 | 13007 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|        44 | 13008 | `			}` |
|        44 | 13009 | `		}` |
|        91 | 13010 | `		return SXRET_OK;` |
|         - | 13011 | `	}` |
|  23867319 | 13012 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|         - | 13013 | `		sxu32 nJz,nJmp;` |
|         - | 13014 | `		sxu32 nTernaryNsBase;` |
|         - | 13015 | `		/* Ternary operator require special handling */` |
|         - | 13016 | `		/* Phase#1: Compile the condition */` |
|    381351 | 13017 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    381351 | 13018 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    381351 | 13019 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13020 | `			return rc;` |
|         - | 13021 | `		}` |
|         - | 13022 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|         - | 13023 | `		 * compiling the condition must short-circuit to the end of the` |
|         - | 13024 | `		 * condition expression, not leak past the ternary. */` |
|    381351 | 13025 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    381351 | 13026 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    381351 | 13027 | `		if( pNode->pLeft ){` |
|         - | 13028 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|         - | 13029 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    377345 | 13030 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 13031 | `			/* Phase#3: Compile the 'then' expression  */` |
|    377345 | 13032 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    377345 | 13033 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    377345 | 13034 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13035 | `				return rc;` |
|         - | 13036 | `			}` |
|    377345 | 13037 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    188675 | 13038 | `		}else{` |
|         - | 13039 | `			/* Elvis operator: (expr) ?: (else)` |
|         - | 13040 | `			 * Duplicate condition so original value is the 'then' result.` |
|         - | 13041 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      4011 | 13042 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      4011 | 13043 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 13044 | `		}` |
|         - | 13045 | `		/* Phase#4: Emit the unconditional jump */` |
|    381351 | 13046 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|         - | 13047 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    381351 | 13048 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    381351 | 13049 | `		if( pInstr ){` |
|    381351 | 13050 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    190673 | 13051 | `		}` |
|    381351 | 13052 | `		if( !pNode->pLeft ){` |
|         - | 13053 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      4011 | 13054 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      2003 | 13055 | `		}` |
|         - | 13056 | `		/* Phase#6: Compile the 'else' expression */` |
|    381351 | 13057 | `		if( pNode->pRight ){` |
|    381351 | 13058 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    381351 | 13059 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    381351 | 13060 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 13061 | `				return rc;` |
|         - | 13062 | `			}` |
|    381351 | 13063 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    190673 | 13064 | `		}` |
|    381351 | 13065 | `		if( nJmp > 0 ){` |
|         - | 13066 | `			/* Phase#7: Fix the unconditional jump */` |
|    381351 | 13067 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    381351 | 13068 | `			if( pInstr ){` |
|    381351 | 13069 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    190673 | 13070 | `			}` |
|    190673 | 13071 | `		}` |
|         - | 13072 | `		/* All done */` |
|    381351 | 13073 | `		return SXRET_OK;` |
|         - | 13074 | `	}` |
|  23485973 | 13075 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|         - | 13076 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|         - | 13077 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|         - | 13078 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|         - | 13079 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|         - | 13080 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|         - | 13081 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|         - | 13082 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|         - | 13083 | `		sxu32 nPipeNsBase;` |
|        27 | 13084 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|        27 | 13085 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|       ! 0 | 13086 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 13087 | `				"'\|>': Missing operand");` |
|       ! 0 | 13088 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 13089 | `		}` |
|         - | 13090 | `		/* Argument: the LHS value. */` |
|        27 | 13091 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 13092 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|        27 | 13093 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13094 | `			return rc;` |
|         - | 13095 | `		}` |
|        27 | 13096 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 13097 | `		/* Callable: the RHS. */` |
|        27 | 13098 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 13099 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|        27 | 13100 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 13101 | `			return rc;` |
|         - | 13102 | `		}` |
|        27 | 13103 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 13104 | `		/* Invoke the callable with the single piped argument. */` |
|        27 | 13105 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        27 | 13106 | `		return SXRET_OK;` |
|         - | 13107 | `	}` |
|  23485947 | 13108 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|         - | 13109 | `	/* Generate code for the left tree */` |
|  23485947 | 13110 | `	if( pNode->pLeft ){` |
|  23462317 | 13111 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  23462317 | 13112 | `		if( iVmOp == PH7_OP_CALL ){` |
|         - | 13113 | `			ph7_expr_node **apNode;` |
|   5590015 | 13114 | `			int hasSpread = 0;` |
|   5590015 | 13115 | `			int hasNamed = 0;` |
|   5590015 | 13116 | `			int bAnySpread = 0;` |
|   5590015 | 13117 | `			sxu32 byRefMask = 0;` |
|         - | 13118 | `			sxi32 nArgs;` |
|         - | 13119 | `			sxi32 n;` |
|         - | 13120 | `			/* Recurse and generate bytecodes for function arguments */` |
|   5590015 | 13121 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   5590015 | 13122 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|         - | 13123 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|         - | 13124 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|         - | 13125 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|   5590015 | 13126 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|        81 | 13127 | `				bFcc = 1;` |
|        81 | 13128 | `				nArgs = 0;` |
|        40 | 13129 | `			}` |
|         - | 13130 | `			/* Validate argument order like php: no positional argument after a` |
|         - | 13131 | ``			 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|         - | 13132 | `			{` |
|   5590015 | 13133 | `				int seenNamed = 0;` |
|   5590015 | 13134 | `				int seenSpread = 0;` |
|  11414639 | 13135 | `				for( n = 0; n < nArgs; ++n ){` |
|   5824631 | 13136 | `					if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      4127 | 13137 | `						bAnySpread = 1;` |
|      4127 | 13138 | `						seenSpread = 1;` |
|      4127 | 13139 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       ! 0 | 13140 | `							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13141 | `								"syntax error, unexpected token \"...\"");` |
|       ! 0 | 13142 | `							return SXERR_SYNTAX;` |
|         5 | 13143 | `						}` |
|   5822570 | 13144 | `					}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       289 | 13145 | `						seenNamed = 1;` |
|       289 | 13146 | `						hasNamed = 1;` |
|   5820367 | 13147 | `					}else if( seenNamed ){` |
|         3 | 13148 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13149 | `							"Cannot use positional argument after named argument");` |
|         3 | 13150 | `						return SXERR_SYNTAX;` |
|   5820223 | 13151 | `					}else if( seenSpread ){` |
|       ! 0 | 13152 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 13153 | `							"Cannot use positional argument after argument unpacking");` |
|       ! 0 | 13154 | `						return SXERR_SYNTAX;` |
|         - | 13155 | `					}` |
|   2912317 | 13156 | `				}` |
|         - | 13157 | `			}` |
|         - | 13158 | `			/* Read-only load */` |
|   5590013 | 13159 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|         - | 13160 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|         - | 13161 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|         - | 13162 | `			 * objects dispatch to the right method (offsetExists for both;` |
|         - | 13163 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|   5590013 | 13164 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|   5590013 | 13165 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|   5590008 | 13166 | `				if( pCallName->nByte == 5` |
|   3142883 | 13167 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|    283919 | 13168 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|   5448056 | 13169 | `				}else if( pCallName->nByte == 5` |
|   2858969 | 13170 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|       107 | 13171 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|        51 | 13172 | `				}` |
|         - | 13173 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|         - | 13174 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|         - | 13175 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|         - | 13176 | `				 * write back through. Skipped when spread/named args are present:` |
|         - | 13177 | `				 * the compile-time positional index no longer maps to the` |
|         - | 13178 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|   5590013 | 13179 | `				if( !bAnySpread && !hasNamed ){` |
|         - | 13180 | `					SyString sBuiltin;` |
|   5585761 | 13181 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|   5585761 | 13182 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|   2792878 | 13183 | `				}` |
|   2795004 | 13184 | `			}` |
|  11414635 | 13185 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|   5824627 | 13186 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   5824627 | 13187 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 13188 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|         - | 13189 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|         - | 13190 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|         - | 13191 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|         - | 13192 | `				 * builtin to write back through. A plain $var target is unaffected` |
|         - | 13193 | `				 * (iP1=0 either way). */` |
|   5824627 | 13194 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|     27639 | 13195 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|     27639 | 13196 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|     13817 | 13197 | `				}` |
|   5824627 | 13198 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|   5824627 | 13199 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 13200 | `					return rc;` |
|         - | 13201 | `				}` |
|         - | 13202 | `				/* Each argument is an independent nullsafe scope. */` |
|   5824627 | 13203 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|   5824627 | 13204 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|         - | 13205 | `					/* Emit spread opcode to unpack this array argument */` |
|      4127 | 13206 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      4127 | 13207 | `					hasSpread = 1;` |
|      2061 | 13208 | `				}` |
|   2912316 | 13209 | `			}` |
|         - | 13210 | `			/* Total number of given arguments */` |
|   5590013 | 13211 | `			iP1 = nArgs;` |
|   5590013 | 13212 | `			iP2 = hasSpread;` |
|         - | 13213 | `			/* Build VmCallArgMap if named arguments are present.` |
|         - | 13214 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|   5590013 | 13215 | `			if( hasNamed ){` |
|       178 | 13216 | `				sxu32 nStrBytes = 0;` |
|         - | 13217 | `				char *zBuf;` |
|       534 | 13218 | `				for( n = 0; n < nArgs; ++n ){` |
|       360 | 13219 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       286 | 13220 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|       141 | 13221 | `					}` |
|       182 | 13222 | `				}` |
|         - | 13223 | `				{` |
|       178 | 13224 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|       178 | 13225 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       174 | 13226 | `					&pGen->pVm->sAllocator, mapSize);` |
|       178 | 13227 | `				if( pMap ){` |
|       178 | 13228 | `					SyZero(pMap, mapSize);` |
|       178 | 13229 | `					pMap->bHasNamed = 1;` |
|       178 | 13230 | `					pMap->nTotal = (sxu32)nArgs;` |
|       178 | 13231 | `					pMap->aNames = (SyString *)&pMap[1];` |
|       178 | 13232 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|       534 | 13233 | `					for( n = 0; n < nArgs; ++n ){` |
|       360 | 13234 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       286 | 13235 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|       286 | 13236 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|       286 | 13237 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|       286 | 13238 | `							zBuf += nb;` |
|       141 | 13239 | `						}` |
|         - | 13240 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|       182 | 13241 | `					}` |
|       178 | 13242 | `					p3 = (void *)pMap;` |
|        87 | 13243 | `				}` |
|         - | 13244 | `				}` |
|        87 | 13245 | `			}` |
|         - | 13246 | `			/* Remove stale flags now */` |
|   5590013 | 13247 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|   2795004 | 13248 | `		}` |
|         - | 13249 | `		{` |
|         - | 13250 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|         - | 13251 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|         - | 13252 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|         - | 13253 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|         - | 13254 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|         - | 13255 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|         - | 13256 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|         - | 13257 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
|  23462315 | 13258 | `			sxi32 iLeftFlags = iFlags;` |
|  23462310 | 13259 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  19300636 | 13260 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|   7569507 | 13261 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   6421475 | 13262 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   2473775 | 13263 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   1236885 | 13264 | `			}` |
|         - | 13265 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|         - | 13266 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|         - | 13267 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|         - | 13268 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|         - | 13269 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|         - | 13270 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|         - | 13271 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
|  23462310 | 13272 | `			if( pNode->pOp` |
|  32859132 | 13273 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
|  21128024 | 13274 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  18793686 | 13275 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|   5039735 | 13276 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   2519865 | 13277 | `			}` |
|         - | 13278 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|         - | 13279 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|         - | 13280 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|         - | 13281 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|         - | 13282 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|         - | 13283 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
|  23462310 | 13284 | `			if( pNode->pOp` |
|  23462315 | 13285 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|    150135 | 13286 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|     75065 | 13287 | `			}` |
|  23462315 | 13288 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|         - | 13289 | `		}` |
|  23462315 | 13290 | `		if( rc != SXRET_OK ){` |
|        34 | 13291 | `			return rc;` |
|         - | 13292 | `		}` |
|  23462285 | 13293 | `		if( !bIsChainOp ){` |
|         - | 13294 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|         - | 13295 | `			 * target the end of that LHS chain, which is right here. */` |
|  10242171 | 13296 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|   5121083 | 13297 | `		}` |
|  23462285 | 13298 | `		if( iVmOp == PH7_OP_CALL ){` |
|   5590013 | 13299 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   5590013 | 13300 | `			if( pInstr ){` |
|   5590013 | 13301 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|   3939319 | 13302 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|         - | 13303 | `					sxu32 nQual;` |
|   3939319 | 13304 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 13305 | `					/* Prevent constant expansion but preserve the absolute flag` |
|         - | 13306 | `					 * so the later NEW handler (if any) can see it. */` |
|   3939319 | 13307 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|         - | 13308 | `					/* Namespace-qualify the function name for CALL, unless the` |
|         - | 13309 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|         - | 13310 | `					 * imports — class imports must NOT affect function` |
|         - | 13311 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|         - | 13312 | `					 * before NEW; we store the original literal index in the` |
|         - | 13313 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|         - | 13314 | `					 * the unqualified name and re-qualify with class imports. */` |
|   3939319 | 13315 | `					if( bAbsolute ){` |
|      3971 | 13316 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      1988 | 13317 | `					}else{` |
|   3935353 | 13318 | `						int fromImport = 0;` |
|   3935353 | 13319 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|   3935353 | 13320 | `						pInstr->iP2 = (sxi32)nQual;` |
|   3935353 | 13321 | `						if( nQual != nOrig ){` |
|         - | 13322 | `							/* Record the original literal index in the arg map` |
|         - | 13323 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|         - | 13324 | `							 * flag) so the NEW handler can recover the` |
|         - | 13325 | `							 * unqualified name and re-qualify with CLASS` |
|         - | 13326 | `							 * imports. */` |
|        77 | 13327 | `							if( p3 == 0 ){` |
|        77 | 13328 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|        72 | 13329 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|        77 | 13330 | `								if( pMap ){` |
|        77 | 13331 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|        77 | 13332 | `									p3 = (void *)pMap;` |
|        36 | 13333 | `								}` |
|        36 | 13334 | `							}` |
|        77 | 13335 | `							if( p3 ){` |
|        77 | 13336 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|        77 | 13337 | `								if( !fromImport ){` |
|         - | 13338 | `									/* Mark as namespace-qualified */` |
|        67 | 13339 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|        31 | 13340 | `								}` |
|        36 | 13341 | `							}` |
|        36 | 13342 | `						}` |
|         5 | 13343 | `					}` |
|   3620356 | 13344 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|         - | 13345 | `					/* Method call,flag that */` |
|   1630397 | 13346 | `					pInstr->iP2 = 1;` |
|    815196 | 13347 | `				}` |
|   2795009 | 13348 | `			}` |
|  20667281 | 13349 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|         - | 13350 | `			ph7_expr_node **apNode;` |
|         - | 13351 | `			sxi32 n;` |
|   2590381 | 13352 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|         - | 13353 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|         - | 13354 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 13355 | `			/* Recurse and generate bytecodes for array index */` |
|   2590381 | 13356 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   5019083 | 13357 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   2428707 | 13358 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   2428707 | 13359 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   2428707 | 13360 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 13361 | `					return rc;` |
|         - | 13362 | `				}` |
|         - | 13363 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   2428707 | 13364 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   1214356 | 13365 | `			}` |
|   2590381 | 13366 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   2428707 | 13367 | `				iP1 = 1; /* Node have an index associated with it */` |
|   1214351 | 13368 | `			}` |
|   2590381 | 13369 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|         - | 13370 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|    323185 | 13371 | `				iP2 = 4;` |
|   2428791 | 13372 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|         - | 13373 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|         - | 13374 | `				 * so the trailing unset() builtin can drop the slot. */` |
|     23709 | 13375 | `				iP2 = 5;` |
|   2255349 | 13376 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|         - | 13377 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|         - | 13378 | `				 * short-circuit on missing keys without invoking offsetGet` |
|         - | 13379 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|        29 | 13380 | `				iP2 = 6;` |
|   2243485 | 13381 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|         - | 13382 | `				/* Create an empty entry when the desired index is not found */` |
|    378787 | 13383 | `				iP2 = 1;` |
|    189396 | 13384 | `			}` |
|  16577089 | 13385 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         - | 13386 | `			/* POP the left node */` |
|        32 | 13387 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        15 | 13388 | `		}` |
|  11731140 | 13389 | `	}` |
|  23485915 | 13390 | `	rc = SXRET_OK;` |
|  23485915 | 13391 | `	nJmpIdx = 0;` |
|         - | 13392 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|         - | 13393 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|         - | 13394 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  23485915 | 13395 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|    394689 | 13396 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    394689 | 13397 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    394689 | 13398 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    394689 | 13399 | `			int isSpecial = 0;` |
|    394689 | 13400 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|    347401 | 13401 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|    347401 | 13402 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|    347396 | 13403 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    315832 | 13404 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    171701 | 13405 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|    106475 | 13406 | `					isSpecial = 1;` |
|     53235 | 13407 | `				}` |
|    185520 | 13408 | `			}` |
|    418333 | 13409 | `			pInstr->iP1 = 0;` |
|    418333 | 13410 | `			if( !isSpecial ){` |
|    264575 | 13411 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    132285 | 13412 | `			}` |
|         - | 13413 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|         - | 13414 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|    371045 | 13415 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    264575 | 13416 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    264575 | 13417 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|        60 | 13418 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|        62 | 13419 | `					return SXRET_OK;` |
|         - | 13420 | `				}` |
|    132256 | 13421 | `			}` |
|    185491 | 13422 | `		}` |
|    232758 | 13423 | `	}` |
|         - | 13424 | `	/* Generate code for the right tree */` |
|  23462227 | 13425 | `	if( pNode->pRight ){` |
|  13484713 | 13426 | `		if( iVmOp == PH7_OP_LAND ){` |
|         - | 13427 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    323451 | 13428 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  13322990 | 13429 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|         - | 13430 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    220711 | 13431 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  13050914 | 13432 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|         - | 13433 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     51349 | 13434 | `			iVmOp = 0; /* No binary operator to emit */` |
|     51349 | 13435 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  12914941 | 13436 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         - | 13437 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|         - | 13438 | `			 * the entire containing postfix chain to null. The jump target is` |
|         - | 13439 | `			 * patched later by the innermost non-chain ancestor (or by` |
|         - | 13440 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|         - | 13441 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|         - | 13442 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|       108 | 13443 | `			sxu32 nNsJmp = 0;` |
|       108 | 13444 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|       108 | 13445 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  12889165 | 13446 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|         - | 13447 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|         - | 13448 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|         - | 13449 | `			 * auto-created — PHP auto-vivifies on write. */` |
|   4095383 | 13450 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|   2047689 | 13451 | `		}` |
|  13484713 | 13452 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  13484713 | 13453 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  13484713 | 13454 | `		if( !bIsChainOp ){` |
|         - | 13455 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|         - | 13456 | `			 * operator instruction is emitted. */` |
|   8445041 | 13457 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|   4222518 | 13458 | `		}` |
|  13484713 | 13459 | `		if( iVmOp == PH7_OP_STORE ){` |
|   3669749 | 13460 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|   3669714 | 13461 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|         - | 13462 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|         - | 13463 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|         - | 13464 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|         - | 13465 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|         - | 13466 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|         - | 13467 | `				 */` |
|        89 | 13468 | `				iVmOp = 0;` |
|   3669707 | 13469 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|   3669665 | 13470 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 13471 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|    792249 | 13472 | `					iP2 = 1;` |
|    396127 | 13473 | `				}else{` |
|   2877421 | 13474 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 13475 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|    359001 | 13476 | `						iVmOp = PH7_OP_STORE_IDX;` |
|    359001 | 13477 | `						iP1 = pInstr->iP1;` |
|    179503 | 13478 | `					}else{` |
|   2518425 | 13479 | `						p3 = pInstr->p3;` |
|         - | 13480 | `					}` |
|         - | 13481 | `					/* POP the last dynamic load instruction */` |
|   2877421 | 13482 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - | 13483 | `				}` |
|   1834835 | 13484 | `			}` |
|  11649841 | 13485 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|        62 | 13486 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|        62 | 13487 | `			if( pInstr ){` |
|        62 | 13488 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 13489 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|         - | 13490 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|         - | 13491 | `					 */` |
|        19 | 13492 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|        19 | 13493 | `					iP1 = pInstr->iP1;` |
|        19 | 13494 | `					iP2 = pInstr->iP2;` |
|        19 | 13495 | `					p3  = pInstr->p3;` |
|        10 | 13496 | `				}else{` |
|        44 | 13497 | `					p3 = pInstr->p3;` |
|         - | 13498 | `				}` |
|        30 | 13499 | `			}` |
|        30 | 13500 | `		}` |
|   6742354 | 13501 | `	}` |
|  23462222 | 13502 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|    367843 | 13503 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|         - | 13504 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|         - | 13505 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|        32 | 13506 | `		iVmOp = 0;` |
|        14 | 13507 | `	}` |
|  23462227 | 13508 | `	if( iVmOp > 0 ){` |
|  23410741 | 13509 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    150135 | 13510 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|         - | 13511 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|     11851 | 13512 | `				iP1 = 1;` |
|      5928 | 13513 | `			}` |
|  23335676 | 13514 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|         - | 13515 | `			/* Namespace-qualify the class name for NEW */ {` |
|    735329 | 13516 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|    735329 | 13517 | `				VmInstr *pCallInstr = 0;` |
|    735329 | 13518 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|    735033 | 13519 | `					pCallInstr = pPeek;` |
|    735033 | 13520 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    367514 | 13521 | `				}` |
|    735329 | 13522 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|    719573 | 13523 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 13524 | `					sxu32 nLitForClass;` |
|    719573 | 13525 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|         - | 13526 | `					/* If the CALL handler qualified the name with FUNCTION` |
|         - | 13527 | `					 * imports, recover the original literal (recorded in the` |
|         - | 13528 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|         - | 13529 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|         - | 13530 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|         - | 13531 | `					 * with class imports. */` |
|    719573 | 13532 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|        37 | 13533 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|        21 | 13534 | `					}else{` |
|    719541 | 13535 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|         - | 13536 | `					}` |
|    719573 | 13537 | `					pPeek->iP1 = 0;` |
|    719573 | 13538 | `					if( !bAbsolute ){` |
|    715611 | 13539 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    357808 | 13540 | `					}else{` |
|      3967 | 13541 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|         - | 13542 | `					}` |
|    359784 | 13543 | `				}` |
|         - | 13544 | `			}` |
|    735329 | 13545 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    735329 | 13546 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|         - | 13547 | `				VmInstr *pPrev;` |
|    735033 | 13548 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|    735033 | 13549 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|         - | 13550 | `					/* Pop the call instruction, preserve named-arg map and` |
|         - | 13551 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|         - | 13552 | `					 * accumulator exactly like OP_CALL would have). */` |
|    735033 | 13553 | `					iP1 = pInstr->iP1;` |
|    735033 | 13554 | `					iP2 = pInstr->iP2;` |
|    735033 | 13555 | `					if( pInstr->p3 ){` |
|        47 | 13556 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|        21 | 13557 | `					}` |
|    735033 | 13558 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    367514 | 13559 | `				}` |
|    367519 | 13560 | `			}` |
|  22892949 | 13561 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|         - | 13562 | `			/* instanceof: right operand is a class name, not a constant.` |
|         - | 13563 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     71141 | 13564 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     71141 | 13565 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     71141 | 13566 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     71141 | 13567 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     71141 | 13568 | `				int isSpecialIs = 0;` |
|     71141 | 13569 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     71141 | 13570 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     71141 | 13571 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     71136 | 13572 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     71139 | 13573 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     35568 | 13574 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        12 | 13575 | `						isSpecialIs = 1;` |
|         5 | 13576 | `					}` |
|     35568 | 13577 | `				}` |
|     71141 | 13578 | `				pInstr->iP1 = 0;` |
|     71141 | 13579 | `				if( !isSpecialIs && !bAbsolute ){` |
|     71121 | 13580 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     35558 | 13581 | `				}` |
|     35573 | 13582 | `			}` |
|  22489719 | 13583 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|         - | 13584 | `			/* Prevent constant expansion for member/property names.` |
|         - | 13585 | `			 * The right child (member name) was just compiled — its LOADC` |
|         - | 13586 | `			 * should not trigger constant lookup. */` |
|   5039677 | 13587 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   5039677 | 13588 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   4799381 | 13589 | `				pInstr->iP1 = 0;` |
|   2399688 | 13590 | `			}` |
|   5039677 | 13591 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|         - | 13592 | `				/* Static member access,remember that */` |
|    371001 | 13593 | `				iP1 = 1;` |
|    371001 | 13594 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    371001 | 13595 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|    236347 | 13596 | `					p3 = pInstr->p3;` |
|    236347 | 13597 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    118171 | 13598 | `				}` |
|    185498 | 13599 | `			}` |
|         - | 13600 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|         - | 13601 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|         - | 13602 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|         - | 13603 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|   5039677 | 13604 | `			if( iP2 == PH7_MEMBER_READ ){` |
|   5039677 | 13605 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        42 | 13606 | `					iP2 = PH7_MEMBER_UNSET;` |
|   5039657 | 13607 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|     63111 | 13608 | `					iP2 = PH7_MEMBER_ISSET;` |
|   5008084 | 13609 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        17 | 13610 | `					iP2 = PH7_MEMBER_EMPTY;` |
|   4976523 | 13611 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|         - | 13612 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|    965705 | 13613 | `					iP2 = PH7_MEMBER_WRITE;` |
|    482850 | 13614 | `				}` |
|   2519836 | 13615 | `			}` |
|   2519836 | 13616 | `		}` |
|         - | 13617 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|         - | 13618 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|         - | 13619 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|         - | 13620 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|         - | 13621 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
|  23410741 | 13622 | `		if( bFcc ){` |
|        81 | 13623 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|        81 | 13624 | `			iP2 = 0;` |
|        81 | 13625 | `			p3 = 0;` |
|        81 | 13626 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|        81 | 13627 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 13628 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|         - | 13629 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|         - | 13630 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|         - | 13631 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|        37 | 13632 | `				void *pMemberName = pInstr->p3;` |
|        37 | 13633 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|        37 | 13634 | `				if( pMemberName ){` |
|         3 | 13635 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|         1 | 13636 | `				}` |
|        37 | 13637 | `				iP1 = 2;` |
|        19 | 13638 | `			}else{` |
|        45 | 13639 | `				iP1 = 1;` |
|         - | 13640 | `			}` |
|        40 | 13641 | `		}` |
|         - | 13642 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|         - | 13643 | `		 * This is the primary emit path for user-visible calls. */` |
|  23410741 | 13644 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|   6325257 | 13645 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|   3162626 | 13646 | `		}` |
|         - | 13647 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  23410741 | 13648 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  11705368 | 13649 | `	}` |
|  23462227 | 13650 | `	if( nJmpIdx > 0 ){` |
|         - | 13651 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|    595501 | 13652 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|    595501 | 13653 | `		if( pInstr ){` |
|    595501 | 13654 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    297748 | 13655 | `		}` |
|    297748 | 13656 | `	}` |
|  23462227 | 13657 | `	return rc;` |
|  29683539 | 13658 | `}` |
|         - | 13659 | `/*` |
|         - | 13660 | ` * Compile a PHP expression.` |
|         - | 13661 | ` * According to the PHP language reference manual:` |
|         - | 13662 | ` *  Expressions are the most important building stones of PHP.` |
|         - | 13663 | ` *  In PHP, almost anything you write is an expression.` |
|         - | 13664 | ` *  The simplest yet most accurate way to define an expression` |
|         - | 13665 | ` *  is "anything that has a value".` |
|         - | 13666 | ` * If something goes wrong while compiling the expression,this` |
|         - | 13667 | ` * function takes care of generating the appropriate error` |
|         - | 13668 | ` * message.` |
|         - | 13669 | ` */` |
|  13050286 | 13670 | `static sxi32 PH7_CompileExpr(` |
|         - | 13671 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 13672 | `	sxi32 iFlags,        /* Control flags */` |
|         - | 13673 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|         - | 13674 | `	)` |
|         5 | 13675 | `{` |
|         - | 13676 | `	ph7_expr_node *pRoot;` |
|         - | 13677 | `	SySet sExprNode;` |
|         - | 13678 | `	SyToken *pEnd;` |
|         - | 13679 | `	sxi32 nExpr;` |
|         - | 13680 | `	sxi32 iNest;` |
|         - | 13681 | `	sxi32 rc;` |
|         - | 13682 | `	sxu32 nNullsafeBase;` |
|         - | 13683 | `	/* Initialize worker variables */` |
|  13050291 | 13684 | `	nExpr = 0;` |
|  13050291 | 13685 | `	pRoot = 0;` |
|         - | 13686 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|         - | 13687 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  13050291 | 13688 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  13050291 | 13689 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  13050291 | 13690 | `	SySetAlloc(&sExprNode,0x10);` |
|  13050291 | 13691 | `	rc = SXRET_OK;` |
|         - | 13692 | `	/* Delimit the expression */` |
|  13050291 | 13693 | `	pEnd = pGen->pIn;` |
|  13050291 | 13694 | `	iNest = 0;` |
| 103639093 | 13695 | `	while( pEnd < pGen->pEnd ){` |
|  98859829 | 13696 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 13697 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      4659 | 13698 | `			iNest++;` |
|  98857502 | 13699 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      4667 | 13700 | `			iNest--;` |
|  98852844 | 13701 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|   8271645 | 13702 | `			if( iNest <= 0 ){` |
|   8271027 | 13703 | `				break;` |
|         - | 13704 | `			}` |
|       309 | 13705 | `		}` |
|  90588807 | 13706 | `		pEnd++;` |
|         5 | 13707 | `	}` |
|  13050291 | 13708 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|    658655 | 13709 | `		SyToken *pEnd2 = pGen->pIn;` |
|    658655 | 13710 | `		iNest = 0;` |
|         - | 13711 | `		/* Stop at the first comma */` |
|   1436173 | 13712 | `		while( pEnd2 < pEnd ){` |
|    777525 | 13713 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|     43427 | 13714 | `				iNest++;` |
|    755814 | 13715 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|     43427 | 13716 | `				iNest--;` |
|    712392 | 13717 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|        63 | 13718 | `				if( iNest <= 0 ){` |
|         3 | 13719 | `					break;` |
|         - | 13720 | `				}` |
|        28 | 13721 | `			}` |
|    777523 | 13722 | `			pEnd2++;` |
|         5 | 13723 | `		}` |
|    658655 | 13724 | `		if( pEnd2 <pEnd ){` |
|         3 | 13725 | `			pEnd = pEnd2;` |
|         1 | 13726 | `		}` |
|    329325 | 13727 | `	}` |
|  13050291 | 13728 | `	if( pEnd > pGen->pIn ){` |
|  13026653 | 13729 | `		SyToken *pTmp = pGen->pEnd;` |
|         - | 13730 | `		/* Swap delimiter */` |
|  13026653 | 13731 | `		pGen->pEnd = pEnd;` |
|         - | 13732 | `		/* Try to get an expression tree */` |
|  13026653 | 13733 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  13026653 | 13734 | `		if( rc == SXRET_OK && pRoot ){` |
|  13026471 | 13735 | `			rc = SXRET_OK;` |
|  13026471 | 13736 | `			if( xTreeValidator ){` |
|         - | 13737 | `				/* Call the upper layer validator callback */` |
|    855549 | 13738 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    427772 | 13739 | `			}` |
|  13026471 | 13740 | `			if( rc != SXERR_ABORT ){` |
|         - | 13741 | `				/* Generate code for the given tree */` |
|  13026471 | 13742 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|         - | 13743 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|         - | 13744 | `				 * expression so they short-circuit to its end. */` |
|  13026471 | 13745 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|   6513233 | 13746 | `			}` |
|  13026471 | 13747 | `			nExpr = 1;` |
|   6513233 | 13748 | `		}` |
|         - | 13749 | `		/* Release the whole tree */` |
|  13026653 | 13750 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|         - | 13751 | `		/* Synchronize token stream */` |
|  13026653 | 13752 | `		pGen->pEnd = pTmp;` |
|  13026653 | 13753 | `		pGen->pIn  = pEnd;` |
|  13026653 | 13754 | `		if( rc == SXERR_ABORT ){` |
|        13 | 13755 | `			SySetRelease(&sExprNode);` |
|        13 | 13756 | `			return SXERR_ABORT;` |
|         - | 13757 | `		}` |
|   6513319 | 13758 | `	}` |
|  13050281 | 13759 | `	SySetRelease(&sExprNode);` |
|  13050281 | 13760 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|   6525148 | 13761 | `}` |
|         - | 13762 | `/*` |
|         - | 13763 | ` * Return a pointer to the node construct handler associated` |
|         - | 13764 | ` * with a given node type [i.e: string,integer,float,...].` |
|         - | 13765 | ` */` |
|   7322044 | 13766 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|         5 | 13767 | `{` |
|   7322049 | 13768 | `	if( nNodeType & PH7_TK_NUM ){` |
|         - | 13769 | `		/* Numeric literal: Either real or integer */` |
|   2796241 | 13770 | `		return PH7_CompileNumLiteral;` |
|   4525813 | 13771 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|         - | 13772 | `		/* Double quoted string */` |
|     82139 | 13773 | `		return PH7_CompileString;` |
|   4443679 | 13774 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|         - | 13775 | `		/* Single quoted string */` |
|   4443559 | 13776 | `		return PH7_CompileSimpleString;` |
|       124 | 13777 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|         - | 13778 | `		/* Heredoc */` |
|        70 | 13779 | `		return PH7_CompileHereDoc;` |
|        57 | 13780 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|         - | 13781 | `		/* Nowdoc */` |
|        51 | 13782 | `		return PH7_CompileNowDoc;` |
|         8 | 13783 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|         - | 13784 | `		/* Backtick quoted string */` |
|         6 | 13785 | `		return PH7_CompileBacktic;` |
|         - | 13786 | `	}` |
|         3 | 13787 | `	return 0;` |
|   3661027 | 13788 | `}` |
|         - | 13789 | `/*` |
|         - | 13790 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|         - | 13791 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|         - | 13792 | ` * in write context" parse error.` |
|         - | 13793 | ` */` |
|     30860 | 13794 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|         5 | 13795 | `{` |
|         - | 13796 | `	sxi32 rc;` |
|     30865 | 13797 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|     30863 | 13798 | `		return SXRET_OK;` |
|         - | 13799 | `	}` |
|         5 | 13800 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|         2 | 13801 | `		pNode ? pNode->pStart->nLine : 1,` |
|         - | 13802 | `		"Can't use nullsafe operator in write context");` |
|         3 | 13803 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     15435 | 13804 | `}` |
|         - | 13805 | `/*` |
|         - | 13806 | ` * Compile an unset() statement.` |
|         - | 13807 | ` * unset($var, $arr[$key], ...);` |
|         - | 13808 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|         - | 13809 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|         - | 13810 | ` * parent array before extracting the element to unset.` |
|         - | 13811 | ` */` |
|     26622 | 13812 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|         5 | 13813 | `{` |
|     26627 | 13814 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     26627 | 13815 | `	sxu32 nIdx = 0;` |
|         - | 13816 | `	SyString sName;` |
|         - | 13817 | `	sxi32 rc;` |
|         - | 13818 | `	/* Jump the 'unset' keyword */` |
|     26627 | 13819 | `	pGen->pIn++;` |
|         - | 13820 | `	/* Save delimiter */` |
|     26627 | 13821 | `	pTmp = pGen->pEnd;` |
|         - | 13822 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     26627 | 13823 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     26627 | 13824 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 13825 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|         - | 13826 | `		SyToken *pClose;` |
|     26627 | 13827 | `		pGen->pIn++;   /* Skip '(' */` |
|     26627 | 13828 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     26627 | 13829 | `		pEnd = pClose; /* Stop at ')' */` |
|     13311 | 13830 | `	}` |
|     26627 | 13831 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|         - | 13832 | `	/* Resolve the 'unset' builtin name once */` |
|     26627 | 13833 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      3943 | 13834 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3943 | 13835 | `		if( pObj == 0 ){` |
|       ! 0 | 13836 | `			return SXERR_ABORT;` |
|         - | 13837 | `		}` |
|      3943 | 13838 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      3943 | 13839 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1969 | 13840 | `	}` |
|         - | 13841 | `	/* Compile each comma-separated argument */` |
|     57489 | 13842 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     30867 | 13843 | `		if( pGen->pIn < pNext ){` |
|     30867 | 13844 | `			pGen->pEnd = pNext;` |
|     30867 | 13845 | `			rc = PH7_CompileExpr(&(*pGen),` |
|         - | 13846 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|         - | 13847 | `				GenStateUnsetValidator);` |
|     30867 | 13848 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 13849 | `				return SXERR_ABORT;` |
|         - | 13850 | `			}` |
|     30867 | 13851 | `			if( rc != SXERR_EMPTY ){` |
|         - | 13852 | `				/* Emit call for this single argument */` |
|     30865 | 13853 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     30865 | 13854 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|     30865 | 13855 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     15430 | 13856 | `			}` |
|     15431 | 13857 | `		}` |
|         - | 13858 | `		/* Jump trailing commas */` |
|     35109 | 13859 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|      4247 | 13860 | `			pNext++;` |
|         5 | 13861 | `		}` |
|     30867 | 13862 | `		pGen->pIn = pNext;` |
|         5 | 13863 | `	}` |
|         - | 13864 | `	/* Skip past the closing ')' if present */` |
|     26627 | 13865 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     26627 | 13866 | `		pGen->pIn++;` |
|     13311 | 13867 | `	}` |
|         - | 13868 | `	/* Restore token stream */` |
|     26627 | 13869 | `	pGen->pEnd = pTmp;` |
|     26627 | 13870 | `	return SXRET_OK;` |
|     13316 | 13871 | `}` |
|         - | 13872 | `/*` |
|         - | 13873 | ` * PHP Language construct table.` |
|         - | 13874 | ` */` |
|         - | 13875 | `static const LangConstruct aLangConstruct[] = {` |
|         - | 13876 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|         - | 13877 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|         - | 13878 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|         - | 13879 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|         - | 13880 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|         - | 13881 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|         - | 13882 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|         - | 13883 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|         - | 13884 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|         - | 13885 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|         - | 13886 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|         - | 13887 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|         - | 13888 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|         - | 13889 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|         - | 13890 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|         - | 13891 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|         - | 13892 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|         - | 13893 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|         - | 13894 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|         - | 13895 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|         - | 13896 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|         - | 13897 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|         - | 13898 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|         - | 13899 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|         - | 13900 | `};` |
|         - | 13901 | `/*` |
|         - | 13902 | ` * Return a pointer to the statement handler routine associated` |
|         - | 13903 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|         - | 13904 | ` */` |
|   6479814 | 13905 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|         - | 13906 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|         - | 13907 | `	SyToken *pLookahed  /* Look-ahead token */` |
|         - | 13908 | `	)` |
|         5 | 13909 | `{` |
|   6479819 | 13910 | `	sxu32 n = 0;` |
|  26763322 | 13911 | `	for(;;){` |
|  53526649 | 13912 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|    443367 | 13913 | `			break;` |
|         - | 13914 | `		}` |
|  53083287 | 13915 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|   6036457 | 13916 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|       ! 0 | 13917 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|       ! 0 | 13918 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|         - | 13919 | `					/* 'static' (class context),return null */` |
|       ! 0 | 13920 | `					return 0;` |
|         - | 13921 | `				}` |
|       ! 0 | 13922 | `			}` |
|   6036452 | 13923 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|        14 | 13924 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|        14 | 13925 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|         - | 13926 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|         3 | 13927 | `				return 0;` |
|         - | 13928 | `			}` |
|         - | 13929 | `			/* Return a pointer to the handler.` |
|         - | 13930 | `			*/` |
|   6036455 | 13931 | `			return aLangConstruct[n].xConstruct;` |
|         - | 13932 | `		}` |
|  47046835 | 13933 | `		n++;` |
|         5 | 13934 | `	}` |
|    443367 | 13935 | `	if( pLookahed ){` |
|    443367 | 13936 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|     70993 | 13937 | `			return PH7_CompileClassInterface;` |
|    372379 | 13938 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|    324559 | 13939 | `			return PH7_CompileClass;` |
|     47825 | 13940 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      7959 | 13941 | `			return PH7_CompileTrait;` |
|         - | 13942 | `		}` |
|         - | 13943 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|         - | 13944 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|         - | 13945 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|         - | 13946 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     19933 | 13947 | `	}` |
|         - | 13948 | `	/* Not a language construct */` |
|     39871 | 13949 | `	return 0;` |
|   3239912 | 13950 | `}` |
|         - | 13951 | `/*` |
|         - | 13952 | ` * Check if the given keyword is in fact a PHP language construct.` |
|         - | 13953 | ` * Return TRUE on success. FALSE otheriwse.` |
|         - | 13954 | ` */` |
|     39868 | 13955 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|         5 | 13956 | `{` |
|         - | 13957 | `	int rc;` |
|     39873 | 13958 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     39873 | 13959 | `	if( rc == FALSE ){` |
|     39754 | 13960 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     16118 | 13961 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|         - | 13962 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|         - | 13963 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|         - | 13964 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|         - | 13965 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|         - | 13966 | `			*/` |
|         - | 13967 | `			){` |
|     39751 | 13968 | `				rc = TRUE;` |
|     19873 | 13969 | `		}` |
|     19877 | 13970 | `	}` |
|     39873 | 13971 | `	return rc;` |
|         5 | 13972 | `}` |
|         - | 13973 | `/*` |
|         - | 13974 | ` * Compile a PHP chunk.` |
|         - | 13975 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 13976 | ` * takes care of generating the appropriate error message.` |
|         - | 13977 | ` */` |
|         - | 13978 | `/*` |
|         - | 13979 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|         - | 13980 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|         - | 13981 | ` * the chunk token set it becomes the pending docblock. An existing` |
|         - | 13982 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|         - | 13983 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|         - | 13984 | ` * intervening non-declaration statements.` |
|         - | 13985 | ` */` |
|  14356512 | 13986 | `static void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|         5 | 13987 | `{` |
|  14356517 | 13988 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  14356517 | 13989 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  14356517 | 13990 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 13991 | `	sxu32 nIdx, n;` |
|  14356512 | 13992 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   1558419 | 13993 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|         - | 13994 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|         - | 13995 | `		 * indexes do not map to the sidecar */` |
|  12798105 | 13996 | `		return;` |
|         - | 13997 | `	}` |
|   1558417 | 13998 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|         - | 13999 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|         - | 14000 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|   1558417 | 14001 | `	SySetReset(&pGen->aPendingAttrs);` |
|   4676735 | 14002 | `	for( n = 0 ; n < nT ; n++ ){` |
|   3118323 | 14003 | `		if( aT[n].nTokIdx != nIdx ){` |
|   3110283 | 14004 | `			continue;` |
|         - | 14005 | `		}` |
|      8045 | 14006 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|        29 | 14007 | `			pGen->sPendingDoc = aT[n].sText;` |
|      8033 | 14008 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|      8021 | 14009 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|      4008 | 14010 | `		}` |
|      4025 | 14011 | `	}` |
|   7178261 | 14012 | `}` |
|         - | 14013 | `/*` |
|         - | 14014 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|         - | 14015 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|         - | 14016 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|         - | 14017 | ` */` |
|   3987992 | 14018 | `static void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|         5 | 14019 | `{` |
|         - | 14020 | `	char *zDup;` |
|   3987997 | 14021 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|   3987977 | 14022 | `		return;` |
|         - | 14023 | `	}` |
|        35 | 14024 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 | 14025 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|        25 | 14026 | `	if( zDup ){` |
|        25 | 14027 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|        10 | 14028 | `	}` |
|        25 | 14029 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|   1994001 | 14030 | `}` |
|         - | 14031 | `/*` |
|         - | 14032 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|         - | 14033 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|         - | 14034 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|         - | 14035 | ` * names may point into the token text, which must outlive the raw script` |
|         - | 14036 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|         - | 14037 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|         - | 14038 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|         - | 14039 | ` */` |
|      8028 | 14040 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|         5 | 14041 | `{` |
|         - | 14042 | `	SySet *pToken;` |
|         - | 14043 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|         - | 14044 | `	char *zSpan;` |
|      8033 | 14045 | `	sxi32 rc = SXRET_OK;` |
|      8033 | 14046 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|       ! 0 | 14047 | `		return SXRET_OK;` |
|         - | 14048 | `	}` |
|     12047 | 14049 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      4014 | 14050 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|      8033 | 14051 | `	if( zSpan == 0 ){` |
|       ! 0 | 14052 | `		return SXRET_OK;` |
|         - | 14053 | `	}` |
|         - | 14054 | `	/* The token set must outlive compilation too: interned operands may` |
|         - | 14055 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|         - | 14056 | `	 * the number of attribute declarations in the program. */` |
|      8033 | 14057 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      8033 | 14058 | `	if( pToken == 0 ){` |
|       ! 0 | 14059 | `		return SXRET_OK;` |
|         - | 14060 | `	}` |
|      8033 | 14061 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      8033 | 14062 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|      8033 | 14063 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|      8033 | 14064 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|      8033 | 14065 | `	pSavedIn = pGen->pIn;` |
|      8033 | 14066 | `	pSavedEnd = pGen->pEnd;` |
|      8037 | 14067 | `	while( pIn < pEnd ){` |
|         - | 14068 | `		ph7_attribute sAttr;` |
|         - | 14069 | `		SyBlob sFQN;` |
|      8037 | 14070 | `		int bAbsolute = 0;` |
|      8037 | 14071 | `		SyZero(&sAttr,sizeof(sAttr));` |
|      8037 | 14072 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|      8037 | 14073 | `		sAttr.nLine = pIn->nLine;` |
|      8037 | 14074 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|        75 | 14075 | `			bAbsolute = 1;` |
|        75 | 14076 | `			pIn++;` |
|        35 | 14077 | `		}` |
|      8037 | 14078 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      8037 | 14079 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      8037 | 14080 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|      8037 | 14081 | `			pIn++;` |
|      8037 | 14082 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       ! 0 | 14083 | `				SyBlobAppend(&sFQN,"\\",1);` |
|       ! 0 | 14084 | `				pIn++;` |
|       ! 0 | 14085 | `				continue;` |
|         - | 14086 | `			}` |
|      8037 | 14087 | `			break;` |
|       ! 0 | 14088 | `		}` |
|      8037 | 14089 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|         - | 14090 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|         - | 14091 | `			 * this feature; never turn it into a new fatal) */` |
|       ! 0 | 14092 | `			SyBlobRelease(&sFQN);` |
|       ! 0 | 14093 | `			break;` |
|         - | 14094 | `		}` |
|         - | 14095 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|         - | 14096 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|         - | 14097 | `		{` |
|      8037 | 14098 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|      8037 | 14099 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|      8037 | 14100 | `			char *zDup = 0;` |
|      8037 | 14101 | `			if( !bAbsolute ){` |
|      7967 | 14102 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|      7967 | 14103 | `				if( pImp ){` |
|       ! 0 | 14104 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|       ! 0 | 14105 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|       ! 0 | 14106 | `					if( zDup ){` |
|       ! 0 | 14107 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|       ! 0 | 14108 | `					}` |
|      7967 | 14109 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - | 14110 | `					SyBlob sTmp;` |
|       ! 0 | 14111 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|       ! 0 | 14112 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       ! 0 | 14113 | `					SyBlobAppend(&sTmp,"\\",1);` |
|       ! 0 | 14114 | `					SyBlobAppend(&sTmp,zName,nName);` |
|       ! 0 | 14115 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       ! 0 | 14116 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|       ! 0 | 14117 | `					if( zDup ){` |
|       ! 0 | 14118 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|       ! 0 | 14119 | `					}` |
|       ! 0 | 14120 | `					SyBlobRelease(&sTmp);` |
|       ! 0 | 14121 | `				}` |
|      3981 | 14122 | `			}` |
|      8037 | 14123 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|      8037 | 14124 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|      8037 | 14125 | `				if( zDup ){` |
|      8037 | 14126 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|      4016 | 14127 | `				}` |
|      4016 | 14128 | `			}` |
|         - | 14129 | `		}` |
|      8037 | 14130 | `		SyBlobRelease(&sFQN);` |
|      8037 | 14131 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 14132 | `			SyToken *pArgsEnd;` |
|      7935 | 14133 | `			pIn++;` |
|      7935 | 14134 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|     15879 | 14135 | `			while( pIn < pArgsEnd ){` |
|      7949 | 14136 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|      7949 | 14137 | `				sxi32 iDepth = 0;` |
|         - | 14138 | `				ph7_attr_arg sArgRec;` |
|     79005 | 14139 | `				while( pArgStop < pArgsEnd ){` |
|     71077 | 14140 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        11 | 14141 | `						iDepth++;` |
|     71072 | 14142 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        11 | 14143 | `						iDepth--;` |
|     71062 | 14144 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|        17 | 14145 | `						break;` |
|         - | 14146 | `					}` |
|     71061 | 14147 | `					pArgStop++;` |
|         5 | 14148 | `				}` |
|      7949 | 14149 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|      7949 | 14150 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      7944 | 14151 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      7928 | 14152 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        28 | 14153 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|         9 | 14154 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|        19 | 14155 | `					if( zN ){` |
|        19 | 14156 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|         9 | 14157 | `					}` |
|        19 | 14158 | `					pArgStart += 2;` |
|         9 | 14159 | `				}` |
|      7949 | 14160 | `				if( pArgStart < pArgStop ){` |
|         - | 14161 | `					SySet *pInstrContainer;` |
|      7949 | 14162 | `					pGen->pIn = pArgStart;` |
|      7949 | 14163 | `					pGen->pEnd = pArgStop;` |
|      7949 | 14164 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7949 | 14165 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|      7949 | 14166 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7949 | 14167 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7949 | 14168 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7949 | 14169 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 14170 | `						pGen->pIn = pSavedIn;` |
|       ! 0 | 14171 | `						pGen->pEnd = pSavedEnd;` |
|       ! 0 | 14172 | `						return SXERR_ABORT;` |
|         - | 14173 | `					}` |
|      7949 | 14174 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|      3972 | 14175 | `				}` |
|      7949 | 14176 | `				pIn = pArgStop;` |
|      7949 | 14177 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 | 14178 | `					pIn++;` |
|         8 | 14179 | `				}` |
|         5 | 14180 | `			}` |
|      7935 | 14181 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|      3965 | 14182 | `		}` |
|      8037 | 14183 | `		SySetPut(pOut,(const void *)&sAttr);` |
|      8037 | 14184 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|         5 | 14185 | `			pIn++;` |
|         5 | 14186 | `			continue;` |
|         - | 14187 | `		}` |
|      8033 | 14188 | `		break;` |
|       ! 0 | 14189 | `	}` |
|      8033 | 14190 | `	pGen->pIn = pSavedIn;` |
|      8033 | 14191 | `	pGen->pEnd = pSavedEnd;` |
|      8033 | 14192 | `	return SXRET_OK;` |
|      4019 | 14193 | `}` |
|         - | 14194 | `/*` |
|         - | 14195 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|         - | 14196 | ` * every recorded group into pOut and clear the pending list.` |
|         - | 14197 | ` */` |
|   3987996 | 14198 | `static sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|         5 | 14199 | `{` |
|   3988001 | 14200 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|         - | 14201 | `	sxu32 n;` |
|         - | 14202 | `	sxi32 rc;` |
|   3996017 | 14203 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|      8021 | 14204 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|      8021 | 14205 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 14206 | `			return SXERR_ABORT;` |
|         - | 14207 | `		}` |
|      4013 | 14208 | `	}` |
|   3988001 | 14209 | `	SySetReset(&pGen->aPendingAttrs);` |
|   3988001 | 14210 | `	return SXRET_OK;` |
|   1994003 | 14211 | `}` |
|         - | 14212 | `/*` |
|         - | 14213 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|         - | 14214 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|         - | 14215 | ` * the main token stream, so the sidecar indexes map directly.` |
|         - | 14216 | ` */` |
|   1708790 | 14217 | `static sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|         5 | 14218 | `{` |
|   1708795 | 14219 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   1708795 | 14220 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   1708795 | 14221 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 14222 | `	sxu32 nIdx, n;` |
|         - | 14223 | `	sxi32 rc;` |
|   1708790 | 14224 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|    197235 | 14225 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   1511565 | 14226 | `		return SXRET_OK;` |
|         - | 14227 | `	}` |
|    197235 | 14228 | `	nIdx = (sxu32)(pTok - pBase);` |
|    591693 | 14229 | `	for( n = 0 ; n < nT ; n++ ){` |
|    394463 | 14230 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|        13 | 14231 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|        13 | 14232 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14233 | `				return SXERR_ABORT;` |
|         - | 14234 | `			}` |
|         6 | 14235 | `		}` |
|    197234 | 14236 | `	}` |
|    197235 | 14237 | `	return SXRET_OK;` |
|    854400 | 14238 | `}` |
|  10396828 | 14239 | `static sxi32 GenStateCompileChunk(` |
|         - | 14240 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 14241 | `	sxi32 iFlags         /* Compile flags */` |
|         - | 14242 | `	)` |
|         5 | 14243 | `{` |
|         - | 14244 | `	ProcLangConstruct xCons;` |
|         - | 14245 | `	sxi32 rc;` |
|  10396833 | 14246 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|   5988038 | 14247 | `	for(;;){` |
|  11186457 | 14248 | `		int bStmtIsDeclare = 0;` |
|  11186457 | 14249 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 14250 | `			/* No more input to process */` |
|     69809 | 14251 | `			break;` |
|         - | 14252 | `		}` |
|         - | 14253 | `		/* Bind a directly-preceding docblock to this statement */` |
|  11116653 | 14254 | `		GenStateSetPendingDoc(&(*pGen));` |
|  11116653 | 14255 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - | 14256 | `			/* php: a statement-position attribute group must be followed by a` |
|         - | 14257 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|         - | 14258 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|         - | 14259 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|         - | 14260 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|      7939 | 14261 | `			int bAttrTarget = 0;` |
|      7934 | 14262 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|      4001 | 14263 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|      7881 | 14264 | `				bAttrTarget = 1;` |
|      3997 | 14265 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        59 | 14266 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        58 | 14267 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|        15 | 14268 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|         4 | 14269 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|         4 | 14270 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|         1 | 14271 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|        59 | 14272 | `					bAttrTarget = 1;` |
|        29 | 14273 | `				}` |
|        29 | 14274 | `			}` |
|      7939 | 14275 | `			if( !bAttrTarget ){` |
|       ! 0 | 14276 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 14277 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|       ! 0 | 14278 | `					&pGen->pIn->sData);` |
|       ! 0 | 14279 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 14280 | `					break;` |
|         - | 14281 | `				}` |
|       ! 0 | 14282 | `				SySetReset(&pGen->aPendingAttrs);` |
|       ! 0 | 14283 | `			}` |
|      3967 | 14284 | `		}` |
|         - | 14285 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|         - | 14286 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  11116653 | 14287 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   6515311 | 14288 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   6515311 | 14289 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|        47 | 14290 | `				bStmtIsDeclare = 1;` |
|        21 | 14291 | `			}` |
|   3257653 | 14292 | `		}` |
|  11116653 | 14293 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|         - | 14294 | `			/* Any non-declare top-level statement locks the strict_types` |
|         - | 14295 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|    789597 | 14296 | `			pGen->bStrictTypesLocked = 1;` |
|    394796 | 14297 | `		}` |
|  11116653 | 14298 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 14299 | `			/* Compile block */` |
|      3961 | 14300 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      3961 | 14301 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 14302 | `				break;` |
|         - | 14303 | `			}` |
|      1983 | 14304 | `		}else{` |
|  11112697 | 14305 | `			xCons = 0;` |
|  11112697 | 14306 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|         - | 14307 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|         - | 14308 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|         - | 14309 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|     35523 | 14310 | `				xCons = PH7_CompileClassModifiers;` |
|  11094938 | 14311 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|         - | 14312 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|         - | 14313 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|      3971 | 14314 | `				xCons = PH7_CompileEnum;` |
|  11075196 | 14315 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   6479819 | 14316 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - | 14317 | `				/* Try to extract a language construct handler */` |
|   6479819 | 14318 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|   6479819 | 14319 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|        13 | 14320 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 14321 | `						"Syntax error: Unexpected keyword '%z'",` |
|         8 | 14322 | `						&pGen->pIn->sData);` |
|         9 | 14323 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 14324 | `						break;` |
|         - | 14325 | `					}` |
|         - | 14326 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|         - | 14327 | `					 * this erroneous statement.` |
|         - | 14328 | `					 */` |
|         9 | 14329 | `					xCons = PH7_ErrorRecover;` |
|         4 | 14330 | `				}` |
|   7833306 | 14331 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    370893 | 14332 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|         - | 14333 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|       117 | 14334 | `				xCons = PH7_CompileLabel;` |
|        56 | 14335 | `			}` |
|  11112697 | 14336 | `			if( xCons == 0 ){` |
|         - | 14337 | `				/* Assume an expression an try to compile it */` |
|   4633147 | 14338 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   4633147 | 14339 | `				if(  rc != SXERR_EMPTY ){` |
|         - | 14340 | `					/* Pop l-value */` |
|   4632997 | 14341 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   2316496 | 14342 | `				}` |
|   2316576 | 14343 | `			}else{` |
|         - | 14344 | `				/* Go compile the sucker */` |
|   6479555 | 14345 | `				rc = xCons(&(*pGen));` |
|         - | 14346 | `			}` |
|  11112697 | 14347 | `			if( rc == SXERR_ABORT ){` |
|         - | 14348 | `				/* Request to abort compilation */` |
|        13 | 14349 | `				break;` |
|         - | 14350 | `			}` |
|         - | 14351 | `		}` |
|         - | 14352 | `		/* Ignore trailing semi-colons ';' */` |
|  19148887 | 14353 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|   8032249 | 14354 | `			pGen->pIn++;` |
|         5 | 14355 | `		}` |
|  11116643 | 14356 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|         - | 14357 | `			/* Compile a single statement and return */` |
|  10327019 | 14358 | `			break;` |
|         - | 14359 | `		}` |
|         - | 14360 | `		/* LOOP ONE */` |
|         - | 14361 | `		/* LOOP TWO */` |
|         - | 14362 | `		/* LOOP THREE */` |
|         - | 14363 | `		/* LOOP FOUR */` |
|         5 | 14364 | `	}` |
|         - | 14365 | `	/* Return compilation status */` |
|  10396833 | 14366 | `	return rc;` |
|         5 | 14367 | `}` |
|         - | 14368 | `/*` |
|         - | 14369 | ` * Compile a Raw PHP chunk.` |
|         - | 14370 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 14371 | ` * takes care of generating the appropriate error message.` |
|         - | 14372 | ` */` |
|     69816 | 14373 | `static sxi32 PH7_CompilePHP(` |
|         - | 14374 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 14375 | `	SySet *pTokenSet,     /* Token set */` |
|         - | 14376 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|         - | 14377 | `	)` |
|         5 | 14378 | `{` |
|     69821 | 14379 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|         - | 14380 | `	sxi32 rc;` |
|         - | 14381 | `	/* Reset the token set (and its trivia sidecar) */` |
|     69821 | 14382 | `	SySetReset(&(*pTokenSet));` |
|     69821 | 14383 | `	SySetReset(&pGen->aTrivia);` |
|         - | 14384 | `	/* Mark as the default token set */` |
|     69821 | 14385 | `	pGen->pTokenSet = &(*pTokenSet);` |
|         - | 14386 | `	/* Advance the stream cursor */` |
|     69821 | 14387 | `	pGen->pRawIn++;` |
|         - | 14388 | `	/* Tokenize the PHP chunk first */` |
|     69821 | 14389 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|         - | 14390 | `	/* Point to the head and tail of the token stream. */` |
|     69821 | 14391 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     69821 | 14392 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|     69821 | 14393 | `	if( is_expr ){` |
|       ! 0 | 14394 | `		rc = SXERR_EMPTY;` |
|       ! 0 | 14395 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - | 14396 | `			/* A simple expression,compile it */` |
|       ! 0 | 14397 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 | 14398 | `		}` |
|         - | 14399 | `		/* Emit the DONE instruction */` |
|       ! 0 | 14400 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       ! 0 | 14401 | `		return SXRET_OK;` |
|         - | 14402 | `	}` |
|     69821 | 14403 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - | 14404 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - | 14405 | `		/*` |
|         - | 14406 | `		 * Shortcut syntax for the 'echo' language construct.` |
|         - | 14407 | `		 * According to the PHP reference manual:` |
|         - | 14408 | `		 *  echo() also has a shortcut syntax, where you can` |
|         - | 14409 | `		 *  immediately follow` |
|         - | 14410 | `		 *  the opening tag with an equals sign as follows:` |
|         - | 14411 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|         - | 14412 | `		 * Symisc extension:` |
|         - | 14413 | `		 *   This short syntax works with all PHP opening` |
|         - | 14414 | `		 *   tags unlike the default PHP engine that handle` |
|         - | 14415 | `		 *   only short tag.` |
|         - | 14416 | `		 */` |
|         - | 14417 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|         3 | 14418 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|         3 | 14419 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|         3 | 14420 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|         3 | 14421 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|         3 | 14422 | `		if( rc != SXERR_EMPTY ){` |
|         3 | 14423 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 | 14424 | `		}` |
|         3 | 14425 | `		return SXRET_OK;` |
|         - | 14426 | `	}` |
|         - | 14427 | `	/* Compile the PHP chunk */` |
|     69819 | 14428 | `	rc = GenStateCompileChunk(pGen,0);` |
|         - | 14429 | `	/* Fix exceptions jumps */` |
|     69819 | 14430 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - | 14431 | `	/* Fix gotos now, the jump destination is resolved */` |
|     69819 | 14432 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|         3 | 14433 | `		rc = SXERR_ABORT;` |
|         1 | 14434 | `	}` |
|         - | 14435 | `	/* Reset container */` |
|     69819 | 14436 | `	SySetReset(&pGen->aGoto);` |
|     69819 | 14437 | `	SySetReset(&pGen->aLabel);` |
|     69819 | 14438 | `	SySetReset(&pGen->aNullsafeJmp);` |
|         - | 14439 | `	/* Compilation result */` |
|     69819 | 14440 | `	return rc;` |
|     34913 | 14441 | `}` |
|         - | 14442 | `/*` |
|         - | 14443 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|         - | 14444 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|         - | 14445 | ` * This is the only compile interface exported from this file.` |
|         - | 14446 | ` */` |
|     72882 | 14447 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|         - | 14448 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|         - | 14449 | `	SyString *pScript,  /* Script to compile */` |
|         - | 14450 | `	sxi32 iFlags        /* Compile flags */` |
|         - | 14451 | `	)` |
|         5 | 14452 | `{` |
|         - | 14453 | `	SySet aPhpToken,aRawToken;` |
|         - | 14454 | `	ph7_gen_state *pCodeGen;` |
|         - | 14455 | `	ph7_value *pRawObj;` |
|         - | 14456 | `	sxu32 nObjIdx;` |
|         - | 14457 | `	sxi32 nRawObj;` |
|         - | 14458 | `	int is_expr;` |
|         - | 14459 | `	sxi8 bSavedStrict;` |
|         - | 14460 | `	sxi8 bSavedStrictLocked;` |
|         - | 14461 | `	sxi32 rc;` |
|     72887 | 14462 | `	if( pScript->nByte < 1 ){` |
|         - | 14463 | `		/* Nothing to compile */` |
|       ! 0 | 14464 | `		return PH7_OK;` |
|         - | 14465 | `	}` |
|         - | 14466 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|         - | 14467 | `	 * file's flags so include/require restore them on return. */` |
|     72887 | 14468 | `	pCodeGen = &pVm->sCodeGen;` |
|     72887 | 14469 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|     72887 | 14470 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|     72887 | 14471 | `	pCodeGen->bStrictTypes = 0;` |
|     72887 | 14472 | `	pCodeGen->bStrictTypesLocked = 0;` |
|         - | 14473 | `	/* Initialize the tokens containers */` |
|     72887 | 14474 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|     72887 | 14475 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|     72887 | 14476 | `	SySetAlloc(&aPhpToken,0xc0);` |
|     72887 | 14477 | `	is_expr = 0;` |
|     72887 | 14478 | `	if( iFlags & PH7_PHP_ONLY ){` |
|         - | 14479 | `		SyToken sTmp;` |
|         - | 14480 | `		/* PHP only: -*/` |
|     59187 | 14481 | `		sTmp.nLine = 1;` |
|     59187 | 14482 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|     59187 | 14483 | `		sTmp.pUserData = 0;` |
|     59187 | 14484 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|     59187 | 14485 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|     59187 | 14486 | `		if( iFlags & PH7_PHP_EXPR ){` |
|         - | 14487 | `			/* A simple PHP expression */` |
|       ! 0 | 14488 | `			is_expr = 1;` |
|       ! 0 | 14489 | `		}` |
|     29596 | 14490 | `	}else{` |
|         - | 14491 | `		/* Tokenize raw text */` |
|     13705 | 14492 | `		SySetAlloc(&aRawToken,32);` |
|     13705 | 14493 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|         - | 14494 | `	}` |
|         - | 14495 | `	/* Process high-level tokens */` |
|     72887 | 14496 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|     72887 | 14497 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|     72887 | 14498 | `	rc = PH7_OK;` |
|     72887 | 14499 | `	if( is_expr ){` |
|         - | 14500 | `		/* Compile the expression */` |
|       ! 0 | 14501 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|       ! 0 | 14502 | `		goto cleanup;` |
|         - | 14503 | `	}` |
|     72887 | 14504 | `	nObjIdx = 0;` |
|         - | 14505 | `	/* Each compilation unit starts in the global namespace.` |
|         - | 14506 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|         - | 14507 | `	 * preventing namespace bleeding across include()d files. */` |
|     72887 | 14508 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|         - | 14509 | `	/* Start the compilation process */` |
|     43296 | 14510 | `	for(;;){` |
|    156401 | 14511 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|     72875 | 14512 | `			break; /* No more tokens to process */` |
|         - | 14513 | `		}` |
|     83531 | 14514 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|         - | 14515 | `			/* Compile the PHP chunk */` |
|     69821 | 14516 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|     69821 | 14517 | `			if( rc == SXERR_ABORT ){` |
|        16 | 14518 | `				break;` |
|         - | 14519 | `			}` |
|     69809 | 14520 | `			continue;` |
|         - | 14521 | `		}` |
|         - | 14522 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|     13715 | 14523 | `		nRawObj = 0;` |
|     27425 | 14524 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|         - | 14525 | `			/* Consume the raw chunk without any processing */` |
|     13715 | 14526 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|     13715 | 14527 | `			if( pRawObj == 0 ){` |
|       ! 0 | 14528 | `				rc = SXERR_MEM;` |
|       ! 0 | 14529 | `				break;` |
|         - | 14530 | `			}` |
|         - | 14531 | `			/* Mark as constant and emit the load constant instruction */` |
|     13715 | 14532 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|     13715 | 14533 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     13715 | 14534 | `			++nRawObj;` |
|     13715 | 14535 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|         5 | 14536 | `		}` |
|     13715 | 14537 | `		if( nRawObj > 0 ){` |
|         - | 14538 | `			/* Emit the consume instruction */` |
|     13715 | 14539 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      6855 | 14540 | `		}` |
|     36446 | 14541 | `	}` |
|     36441 | 14542 | `cleanup:` |
|     72887 | 14543 | `	SySetRelease(&aRawToken);` |
|     72887 | 14544 | `	SySetRelease(&aPhpToken);` |
|         - | 14545 | `	/* Restore outer file's strict_types scope */` |
|     72887 | 14546 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|     72887 | 14547 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|     72887 | 14548 | `	return rc;` |
|     36446 | 14549 | `}` |
|         - | 14550 | `/*` |
|         - | 14551 | ` * Utility routines.Initialize the code generator.` |
|         - | 14552 | ` */` |
|      3938 | 14553 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|         - | 14554 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 14555 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 14556 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 14557 | `	)` |
|         5 | 14558 | `{` |
|      3943 | 14559 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 14560 | `	/* Zero the structure */` |
|      3943 | 14561 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|         - | 14562 | `	/* Initial state */` |
|      3943 | 14563 | `	pGen->pVm  = &(*pVm);` |
|      3943 | 14564 | `	pGen->xErr = xErr;` |
|      3943 | 14565 | `	pGen->pErrData = pErrData;` |
|      3943 | 14566 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|      3943 | 14567 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|      3943 | 14568 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|      3943 | 14569 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3943 | 14570 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3943 | 14571 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|      3943 | 14572 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|         - | 14573 | `	/* Error log buffer */` |
|      3943 | 14574 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         - | 14575 | `	/* General purpose working buffer */` |
|      3943 | 14576 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         - | 14577 | `	/* Namespace state */` |
|      3943 | 14578 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|      3943 | 14579 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|      3943 | 14580 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|      3943 | 14581 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 14582 | `	/* Create the global scope */` |
|      3943 | 14583 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|         - | 14584 | `	/* Point to the global scope */` |
|      3943 | 14585 | `	pGen->pCurrent = &pGen->sGlobal;` |
|      3943 | 14586 | `	return SXRET_OK;` |
|         5 | 14587 | `}` |
|         - | 14588 | `/*` |
|         - | 14589 | ` * Utility routines. Reset the code generator to it's initial state.` |
|         - | 14590 | ` */` |
|     76432 | 14591 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|         - | 14592 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 14593 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 14594 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 14595 | `	)` |
|         5 | 14596 | `{` |
|     76437 | 14597 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 14598 | `	GenBlock *pBlock,*pParent;` |
|         - | 14599 | `	/* Reset state */` |
|     76437 | 14600 | `	SySetReset(&pGen->aLabel);` |
|     76437 | 14601 | `	SySetReset(&pGen->aGoto);` |
|     76437 | 14602 | `	SySetReset(&pGen->aNullsafeJmp);` |
|     76437 | 14603 | `	SySetReset(&pGen->aTrivia);` |
|     76437 | 14604 | `	SySetReset(&pGen->aPendingAttrs);` |
|     76437 | 14605 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|     76437 | 14606 | `	SyBlobRelease(&pGen->sErrBuf);` |
|     76437 | 14607 | `	SyBlobRelease(&pGen->sWorker);` |
|     76437 | 14608 | `	SyBlobRelease(&pGen->sNamespace);` |
|     76437 | 14609 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     76437 | 14610 | `	SyHashRelease(&pGen->hUseImports);` |
|     76437 | 14611 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|     76437 | 14612 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     76437 | 14613 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|     76437 | 14614 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     76437 | 14615 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 14616 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|         - | 14617 | `	 * They intern variable names and literal strings that are referenced by` |
|         - | 14618 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|         - | 14619 | `	 * Releasing them would either leak the interned strings or require freeing` |
|         - | 14620 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|         - | 14621 | `	 * number of unique names, which is acceptable. */` |
|         - | 14622 | `	/* Point to the global scope */` |
|     76437 | 14623 | `	pBlock = pGen->pCurrent;` |
|     76437 | 14624 | `	while( pBlock->pParent != 0 ){` |
|       ! 0 | 14625 | `		pParent = pBlock->pParent;` |
|       ! 0 | 14626 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 14627 | `		pBlock = pParent;` |
|       ! 0 | 14628 | `	}` |
|     76437 | 14629 | `	pGen->xErr = xErr;` |
|     76437 | 14630 | `	pGen->pErrData = pErrData;` |
|     76437 | 14631 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     76437 | 14632 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|     76437 | 14633 | `	pGen->pIn = pGen->pEnd = 0;` |
|     76437 | 14634 | `	pGen->nErr = 0;` |
|     76437 | 14635 | `	return SXRET_OK;` |
|         5 | 14636 | `}` |
|         - | 14637 | `/*` |
|         - | 14638 | ` * Generate a compile-time error message.` |
|         - | 14639 | ` * If the error count limit is reached (usually 15 error message)` |
|         - | 14640 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|         - | 14641 | ` * abort compilation immediately.` |
|         - | 14642 | ` */` |
|     16448 | 14643 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|         5 | 14644 | `{` |
|     16453 | 14645 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     16453 | 14646 | `	const char *zErr = "Error";` |
|         - | 14647 | `	SyString *pFile;` |
|         - | 14648 | `	va_list ap;` |
|         - | 14649 | `	sxi32 rc;` |
|         - | 14650 | `	/* Reset the working buffer */` |
|     16453 | 14651 | `	SyBlobReset(pWorker);` |
|         - | 14652 | `	/* Peek the processed file path if available */` |
|     16453 | 14653 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     16453 | 14654 | `	if( nErrType == E_ERROR ){` |
|         - | 14655 | `		/* Increment the error counter */` |
|       551 | 14656 | `		pGen->nErr++;` |
|       551 | 14657 | `		if( pGen->nErr > 15 ){` |
|         - | 14658 | `			/* Error count limit reached */` |
|         6 | 14659 | `			if( pGen->xErr ){` |
|         6 | 14660 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|         6 | 14661 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|         6 | 14662 | `				if( pFile ){` |
|         6 | 14663 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|         2 | 14664 | `				}` |
|         6 | 14665 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|         6 | 14666 | `				if( SyBlobLength(pWorker) > 0 ){` |
|         6 | 14667 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|         2 | 14668 | `				}` |
|         2 | 14669 | `			}` |
|         - | 14670 | `			/* Abort immediately */` |
|         6 | 14671 | `			return SXERR_ABORT;` |
|         - | 14672 | `		}` |
|       271 | 14673 | `	}` |
|     16449 | 14674 | `	if( pGen->xErr == 0 ){` |
|         - | 14675 | `		/* No available error consumer,return immediately */` |
|     15759 | 14676 | `		return SXRET_OK;` |
|         - | 14677 | `	}` |
|       694 | 14678 | `	switch(nErrType){` |
|       544 | 14679 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|        32 | 14680 | `	case E_WARNING: zErr = "Warning";     break;` |
|       112 | 14681 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|        11 | 14682 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|       ! 0 | 14683 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|       ! 0 | 14684 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|       ! 0 | 14685 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|         7 | 14686 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|       ! 0 | 14687 | `	default:` |
|       ! 0 | 14688 | `		break;` |
|         - | 14689 | `	}` |
|       694 | 14690 | `	rc = SXRET_OK;` |
|         - | 14691 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|       694 | 14692 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|       694 | 14693 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|       694 | 14694 | `	va_start(ap,zFormat);` |
|       694 | 14695 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|       694 | 14696 | `	va_end(ap);` |
|       694 | 14697 | `	if( pFile ){` |
|       694 | 14698 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       345 | 14699 | `	}` |
|         - | 14700 | `	/* Append a new line */` |
|       694 | 14701 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       694 | 14702 | `	if( SyBlobLength(pWorker) > 0 ){` |
|         - | 14703 | `		/* Consume the generated error message */` |
|       694 | 14704 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       345 | 14705 | `	}` |
|       694 | 14706 | `	return rc;` |
|      8229 | 14707 | `}` |
|         - | 14708 |  |
