# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 6392/7893 lines (80.98%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits |  Line | Source |
| -------: | ----: | :--- |
|        - |     1 | `/**` |
|        - |     2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |     3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |     4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |     5 | ` */` |
|        - |     6 | `#include "ph7int.h"` |
|        - |     7 | `/*` |
|        - |     8 | ` * This file implement a thread-safe and full-reentrant compiler for the PH7 engine.` |
|        - |     9 | ` * That is, routines defined in this file takes a stream of tokens and output` |
|        - |    10 | ` * PH7 bytecode instructions.` |
|        - |    11 | ` */` |
|        - |    12 | `/* Forward declaration */` |
|        - |    13 | `typedef struct LangConstruct LangConstruct;` |
|        - |    14 | `typedef struct JumpFixup     JumpFixup;` |
|        - |    15 | `typedef struct Label         Label;` |
|        - |    16 | `/* Block [i.e: set of statements] control flags */` |
|        - |    17 | `#define GEN_BLOCK_LOOP        0x001    /* Loop block [i.e: for,while,...] */` |
|        - |    18 | `#define GEN_BLOCK_PROTECTED   0x002    /* Protected block */` |
|        - |    19 | `#define GEN_BLOCK_COND        0x004    /* Conditional block [i.e: if(condition){} ]*/` |
|        - |    20 | `#define GEN_BLOCK_FUNC        0x008    /* Function body */` |
|        - |    21 | `#define GEN_BLOCK_GLOBAL      0x010    /* Global block (always set)*/` |
|        - |    22 | `#define GEN_BLOC_NESTED_FUNC  0x020    /* Nested function body */` |
|        - |    23 | `#define GEN_BLOCK_EXPR        0x040    /* Expression */` |
|        - |    24 | `#define GEN_BLOCK_STD         0x080    /* Standard block */` |
|        - |    25 | `#define GEN_BLOCK_EXCEPTION   0x100    /* Exception block [i.e: try{ } }*/` |
|        - |    26 | `#define GEN_BLOCK_SWITCH      0x200    /* Switch statement */` |
|        - |    27 | `/*` |
|        - |    28 | ` * Each label seen in the input is recorded in an instance` |
|        - |    29 | ` * of the following structure.` |
|        - |    30 | ` * A label is a target point [i.e: a jump destination] that is specified` |
|        - |    31 | ` * by an identifier followed by a colon.` |
|        - |    32 | ` * Example` |
|        - |    33 | ` *  LABEL:` |
|        - |    34 | ` *		echo "hello\n";` |
|        - |    35 | ` */` |
|        - |    36 | `struct Label` |
|        - |    37 | `{` |
|        - |    38 | `	ph7_vm_func *pFunc;  /* Compiled function where the label was declared.NULL otherwise */` |
|        - |    39 | `	sxu32 nJumpDest;     /* Jump destination */` |
|        - |    40 | `	SyString sName;      /* Label name */` |
|        - |    41 | `	sxu32 nLine;         /* Line number this label occurs */` |
|        - |    42 | `	sxu8 bRef;           /* True if the label was referenced */` |
|        - |    43 | `};` |
|        - |    44 | `/*` |
|        - |    45 | ` * Compilation of some PHP constructs such as if, for, while, the logical or` |
|        - |    46 | ` * (\|\|) and logical and (&&) operators in expressions requires the` |
|        - |    47 | ` * generation of forward jumps.` |
|        - |    48 | ` * Since the destination PC target of these jumps isn't known when the jumps` |
|        - |    49 | ` * are emitted, we record each forward jump in an instance of the following` |
|        - |    50 | ` * structure. Those jumps are fixed later when the jump destination is resolved.` |
|        - |    51 | ` */` |
|        - |    52 | `struct JumpFixup` |
|        - |    53 | `{` |
|        - |    54 | `	sxi32 nJumpType;     /* Jump type. Either TRUE jump, FALSE jump or Unconditional jump */` |
|        - |    55 | `	sxu32 nInstrIdx;     /* Instruction index to fix later when the jump destination is resolved. */` |
|        - |    56 | `	/* The following fields are only used by the goto statement */` |
|        - |    57 | `	SyString sLabel;    /* Label name */` |
|        - |    58 | `	ph7_vm_func *pFunc; /* Compiled function inside which the goto was emitted. NULL otherwise */` |
|        - |    59 | `	sxu32 nLine;        /* Track line number */` |
|        - |    60 | `};` |
|        - |    61 | `/*` |
|        - |    62 | ` * Each language construct is represented by an instance` |
|        - |    63 | ` * of the following structure.` |
|        - |    64 | ` */` |
|        - |    65 | `struct LangConstruct` |
|        - |    66 | `{` |
|        - |    67 | `	sxu32 nID;                     /* Language construct ID [i.e: PH7_TKWRD_WHILE,PH7_TKWRD_FOR,PH7_TKWRD_IF...] */` |
|        - |    68 | `	ProcLangConstruct xConstruct;  /* C function implementing the language construct */` |
|        - |    69 | `};` |
|        - |    70 | `/* Compilation flags */` |
|        - |    71 | `#define PH7_COMPILE_SINGLE_STMT 0x001 /* Compile a single statement */` |
|        - |    72 | `/* Token stream synchronization macros */` |
|        - |    73 | `#define SWAP_TOKEN_STREAM(GEN,START,END)\` |
|        - |    74 | `	pTmp  = GEN->pEnd;\` |
|        - |    75 | `	pGen->pIn  = START;\` |
|        - |    76 | `	pGen->pEnd = END` |
|        - |    77 | `#define UPDATE_TOKEN_STREAM(GEN)\` |
|        - |    78 | `	if( GEN->pIn < pTmp ){\` |
|        - |    79 | `	    GEN->pIn++;\` |
|        - |    80 | `	}\` |
|        - |    81 | `	GEN->pEnd = pTmp` |
|        - |    82 | `#define SWAP_DELIMITER(GEN,START,END)\` |
|        - |    83 | `	pTmpIn  = GEN->pIn;\` |
|        - |    84 | `	pTmpEnd = GEN->pEnd;\` |
|        - |    85 | `	GEN->pIn = START;\` |
|        - |    86 | `	GEN->pEnd = END` |
|        - |    87 | `#define RE_SWAP_DELIMITER(GEN)\` |
|        - |    88 | `	GEN->pIn  = pTmpIn;\` |
|        - |    89 | `	GEN->pEnd = pTmpEnd` |
|        - |    90 | `/* Flags related to expression compilation */` |
|        - |    91 | `#define EXPR_FLAG_LOAD_IDX_STORE    0x001 /* Set the iP2 flag when dealing with the LOAD_IDX instruction */` |
|        - |    92 | `#define EXPR_FLAG_RDONLY_LOAD       0x002 /* Read-only load, refer to the 'PH7_OP_LOAD' VM instruction for more information */` |
|        - |    93 | `#define EXPR_FLAG_COMMA_STATEMENT   0x004 /* Treat comma expression as a single statement (used by class attributes) */` |
|        - |    94 | `#define EXPR_FLAG_LOAD_IDX_ISSET    0x008 /* LOAD_IDX argument is the LHS of isset() — emit iP2=4 (offsetExists) */` |
|        - |    95 | `#define EXPR_FLAG_LOAD_IDX_UNSET    0x010 /* LOAD_IDX argument is the LHS of unset() — emit iP2=5 (offsetUnset) */` |
|        - |    96 | `#define EXPR_FLAG_LOAD_IDX_EMPTY    0x020 /* LOAD_IDX argument is the LHS of empty() — emit iP2=6 (offsetExists+offsetGet) */` |
|        - |    97 | `#define EXPR_FLAG_MEMBER_WRITE      0x040 /* Sub-tree is the write lvalue of an assignment: tag a target` |
|        - |    98 | `                                           * OP_MEMBER iP2=PH7_MEMBER_WRITE so the VM auto-creates a missing` |
|        - |    99 | ``                                           * property (e.g. `$o->arr[$k] = v`, `$o->p ??= v`). Propagated`` |
|        - |   100 | `                                           * from the precedence-18 lvalue through SUBSCRIPT to the base` |
|        - |   101 | ``                                            * member; stripped when descending into an intermediate `->` `` |
|        - |   102 | `                                           * container (the container is read, not the write target). */` |
|        - |   103 | `/* Forward declaration */` |
|        - |   104 | `static sxi32 PH7_CompileExpr(ph7_gen_state *pGen,sxi32 iFlags,sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *));` |
|        - |   105 | `/*` |
|        - |   106 | ` * Local utility routines used in the code generation phase.` |
|        - |   107 | ` */` |
|        - |   108 | `/*` |
|        - |   109 | ` * Check if the given name refer to a valid label.` |
|        - |   110 | ` * Return SXRET_OK and write a pointer to that label on success.` |
|        - |   111 | ` * Any other return value indicates no such label.` |
|        - |   112 | ` */` |
|      148 |   113 | `static sxi32 GenStateGetLabel(ph7_gen_state *pGen,SyString *pName,Label **ppOut)` |
|        5 |   114 | `{` |
|        - |   115 | `	Label *aLabel;` |
|        - |   116 | `	sxu32 n;` |
|        - |   117 | `	/* Perform a linear scan on the label table */` |
|      153 |   118 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|      333 |   119 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|      277 |   120 | `		if( SyStringCmp(&aLabel[n].sName,pName,SyMemcmp) == 0 ){` |
|        - |   121 | `			/* Jump destination found */` |
|       97 |   122 | `			aLabel[n].bRef = TRUE;` |
|       97 |   123 | `			if( ppOut ){` |
|       97 |   124 | `				*ppOut = &aLabel[n];` |
|       46 |   125 | `			}` |
|       97 |   126 | `			return SXRET_OK;` |
|        - |   127 | `		}` |
|       93 |   128 | `	}` |
|        - |   129 | `	/* No such destination */` |
|       60 |   130 | `	return SXERR_NOTFOUND;` |
|       79 |   131 | `}` |
|        - |   132 | `/*` |
|        - |   133 | ` * Fetch a block that correspond to the given criteria from the stack of` |
|        - |   134 | ` * compiled blocks.` |
|        - |   135 | ` * Return a pointer to that block on success. NULL otherwise.` |
|        - |   136 | ` */` |
|    57946 |   137 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|        5 |   138 | `{` |
|    57951 |   139 | `	GenBlock *pBlock = pCurrent;` |
|   134808 |   140 | `	for(;;){` |
|   269621 |   141 | `		if( pBlock->iFlags & iBlockType ){` |
|    57843 |   142 | `			iCount--; /* Decrement nesting level */` |
|    57843 |   143 | `			if( iCount < 1 ){` |
|        - |   144 | `				/* Block meet with the desired criteria */` |
|    57817 |   145 | `				return pBlock;` |
|        - |   146 | `			}` |
|       13 |   147 | `		}` |
|        - |   148 | `		/* Point to the upper block */` |
|   211809 |   149 | `		pBlock = pBlock->pParent;` |
|   211809 |   150 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|        - |   151 | `			/* Forbidden */` |
|       72 |   152 | `			break;` |
|        - |   153 | `		}` |
|        5 |   154 | `	}` |
|        - |   155 | `	/* No such block */` |
|      139 |   156 | `	return 0;` |
|    28978 |   157 | `}` |
|        - |   158 | `/*` |
|        - |   159 | ` * Initialize a freshly allocated block instance.` |
|        - |   160 | ` */` |
|  5722410 |   161 | `static void GenStateInitBlock(` |
|        - |   162 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |   163 | `	GenBlock *pBlock,    /* Target block */` |
|        - |   164 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|        - |   165 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|        - |   166 | `	void *pUserData      /* Upper layer private data */` |
|        - |   167 | `	)` |
|        5 |   168 | `{` |
|        - |   169 | `	/* Initialize block fields */` |
|  5722415 |   170 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  5722415 |   171 | `	pBlock->pUserData   = pUserData;` |
|  5722415 |   172 | `	pBlock->pGen        = pGen;` |
|  5722415 |   173 | `	pBlock->iFlags      = iType;` |
|  5722415 |   174 | `	pBlock->pParent     = 0;` |
|  5722415 |   175 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  5722415 |   176 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  5722415 |   177 | `}` |
|        - |   178 | `/*` |
|        - |   179 | ` * Allocate a new block instance.` |
|        - |   180 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|        - |   181 | ` * on success.Otherwise generate a compile-time error and abort` |
|        - |   182 | ` * processing on failure.` |
|        - |   183 | ` */` |
|  5718566 |   184 | `static sxi32 GenStateEnterBlock(` |
|        - |   185 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - |   186 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|        - |   187 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|        - |   188 | `	void *pUserData,      /* Upper layer private data */` |
|        - |   189 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|        - |   190 | `	)` |
|        5 |   191 | `{` |
|        - |   192 | `	GenBlock *pBlock;` |
|        - |   193 | `	/* Allocate a new block instance */` |
|  5718571 |   194 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  5718571 |   195 | `	if( pBlock == 0 ){` |
|        - |   196 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   197 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   198 | `		 */` |
|      ! 0 |   199 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|        - |   200 | `		/* Abort processing immediately */` |
|      ! 0 |   201 | `		return SXERR_ABORT;` |
|        - |   202 | `	}` |
|        - |   203 | `	/* Zero the structure */` |
|  5718571 |   204 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  5718571 |   205 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|        - |   206 | `	/* Link to the parent block */` |
|  5718571 |   207 | `	pBlock->pParent = pGen->pCurrent;` |
|        - |   208 | `	/* Mark as the current block */` |
|  5718571 |   209 | `	pGen->pCurrent = pBlock;` |
|  5718571 |   210 | `	if( ppBlock ){` |
|        - |   211 | `		/* Write a pointer to the new instance */` |
|  2769523 |   212 | `		*ppBlock = pBlock;` |
|  1384759 |   213 | `	}` |
|  5718571 |   214 | `	return SXRET_OK;` |
|  2859288 |   215 | `}` |
|        - |   216 | `/*` |
|        - |   217 | ` * Release block fields without freeing the whole instance.` |
|        - |   218 | ` */` |
|  5718558 |   219 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|        5 |   220 | `{` |
|  5718563 |   221 | `	SySetRelease(&pBlock->aPostContFix);` |
|  5718563 |   222 | `	SySetRelease(&pBlock->aJumpFix);` |
|  5718563 |   223 | `}` |
|        - |   224 | `/*` |
|        - |   225 | ` * Release a block.` |
|        - |   226 | ` */` |
|  5718558 |   227 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|        5 |   228 | `{` |
|  5718563 |   229 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  5718563 |   230 | `	GenStateReleaseBlock(&(*pBlock));` |
|        - |   231 | `	/* Free the instance */` |
|  5718563 |   232 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  5718563 |   233 | `}` |
|        - |   234 | `/*` |
|        - |   235 | ` * POP and release a block from the stack of compiled blocks.` |
|        - |   236 | ` */` |
|  5718558 |   237 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|        5 |   238 | `{` |
|  5718563 |   239 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  5718563 |   240 | `	if( pBlock == 0 ){` |
|        - |   241 | `		/* No more block to pop */` |
|      ! 0 |   242 | `		return SXERR_EMPTY;` |
|        - |   243 | `	}` |
|        - |   244 | `	/* Point to the upper block */` |
|  5718563 |   245 | `	pGen->pCurrent = pBlock->pParent;` |
|  5718563 |   246 | `	if( ppBlock ){` |
|        - |   247 | `		/* Write a pointer to the popped block */` |
|      ! 0 |   248 | `		*ppBlock = pBlock;` |
|      ! 0 |   249 | `	}else{` |
|        - |   250 | `		/* Safely release the block */` |
|  5718563 |   251 | `		GenStateFreeBlock(&(*pBlock));` |
|        - |   252 | `	}` |
|  5718563 |   253 | `	return SXRET_OK;` |
|  2859284 |   254 | `}` |
|        - |   255 | `/*` |
|        - |   256 | ` * Emit a forward jump.` |
|        - |   257 | ` * Notes on forward jumps` |
|        - |   258 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|        - |   259 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|        - |   260 | ` *  generation of forward jumps.` |
|        - |   261 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|        - |   262 | ` *  are emitted, we record each forward jump in an instance of the following` |
|        - |   263 | ` *  structure. Those jumps are fixed later when the jump destination is resolved.` |
|        - |   264 | ` */` |
|  2151228 |   265 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|        5 |   266 | `{` |
|        - |   267 | `	JumpFixup sJumpFix;` |
|        - |   268 | `	sxi32 rc;` |
|        - |   269 | `	/* Init the JumpFixup structure */` |
|  2151233 |   270 | `	sJumpFix.nJumpType = nJumpType;` |
|  2151233 |   271 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|        - |   272 | `	/* Insert in the jump fixup table */` |
|  2151233 |   273 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  2151233 |   274 | `	return rc;` |
|        5 |   275 | `}` |
|        - |   276 | `/*` |
|        - |   277 | ` * Fix a forward jump now the jump destination is resolved.` |
|        - |   278 | ` * Return the total number of fixed jumps.` |
|        - |   279 | ` * Notes on forward jumps:` |
|        - |   280 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|        - |   281 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|        - |   282 | ` *  generation of forward jumps.` |
|        - |   283 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|        - |   284 | ` *  are emitted, we record each forward jump in an instance of the following` |
|        - |   285 | ` *  structure.Those jumps are fixed later when the jump destination is resolved.` |
|        - |   286 | ` */` |
|  4058938 |   287 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|        5 |   288 | `{` |
|        - |   289 | `	JumpFixup *aFix;` |
|        - |   290 | `	VmInstr *pInstr;` |
|        - |   291 | `	sxu32 nFixed;` |
|        - |   292 | `	sxu32 n;` |
|        - |   293 | `	/* Point to the jump fixup table */` |
|  4058943 |   294 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|        - |   295 | `	/* Fix the desired jumps */` |
|  7891277 |   296 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  3832339 |   297 | `		if( aFix[n].nJumpType < 0 ){` |
|        - |   298 | `			/* Already fixed */` |
|  1372737 |   299 | `			continue;` |
|        - |   300 | `		}` |
|  2459607 |   301 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|        - |   302 | `			/* Not of our interest */` |
|   308381 |   303 | `			continue;` |
|        - |   304 | `		}` |
|        - |   305 | `		/* Point to the instruction to fix */` |
|  2151231 |   306 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  2151231 |   307 | `		if( pInstr ){` |
|  2151231 |   308 | `			pInstr->iP2 = nJumpDest;` |
|  2151231 |   309 | `			nFixed++;` |
|        - |   310 | `			/* Mark as fixed */` |
|  2151231 |   311 | `			aFix[n].nJumpType = -1;` |
|  1075613 |   312 | `		}` |
|  1075618 |   313 | `	}` |
|        - |   314 | `	/* Total number of fixed jumps */` |
|  4058943 |   315 | `	return nFixed;` |
|        5 |   316 | `}` |
|        - |   317 | `/*` |
|        - |   318 | ` * Fix a 'goto' now the jump destination is resolved.` |
|        - |   319 | ` * The goto statement can be used to jump to another section` |
|        - |   320 | ` * in the program.` |
|        - |   321 | ` * Refer to the routine responsible of compiling the goto` |
|        - |   322 | ` * statement for more information.` |
|        - |   323 | ` */` |
|  1443370 |   324 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|        5 |   325 | `{` |
|        - |   326 | `	JumpFixup *pJump,*aJumps;` |
|        - |   327 | `	Label *pLabel,*aLabel;` |
|        - |   328 | `	VmInstr *pInstr;` |
|        - |   329 | `	sxi32 rc;` |
|        - |   330 | `	sxu32 n;` |
|        - |   331 | `	/* Point to the goto table */` |
|  1443375 |   332 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|        - |   333 | `	/* Fix */` |
|  1443521 |   334 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
|      153 |   335 | `		pJump = &aJumps[n];` |
|        - |   336 | `		/* Extract the target label */` |
|      153 |   337 | `		rc = GenStateGetLabel(&(*pGen),&pJump->sLabel,&pLabel);` |
|      153 |   338 | `		if( rc != SXRET_OK ){` |
|        - |   339 | `			/* No such label */` |
|       60 |   340 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' was referenced but not defined",&pJump->sLabel);` |
|       60 |   341 | `			if( rc == SXERR_ABORT ){` |
|        3 |   342 | `				return SXERR_ABORT;` |
|        - |   343 | `			}` |
|       58 |   344 | `			continue;` |
|        - |   345 | `		}` |
|        - |   346 | `		/* Make sure the target label is reachable */` |
|       97 |   347 | `		if( pLabel->pFunc != pJump->pFunc ){` |
|       12 |   348 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' is unreachable",&pJump->sLabel);` |
|       12 |   349 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |   350 | `				return SXERR_ABORT;` |
|        - |   351 | `			}` |
|        4 |   352 | `		}` |
|        - |   353 | `		/* Fix the jump now the destination is resolved */` |
|       97 |   354 | `		pInstr = PH7_VmGetInstr(pGen->pVm,pJump->nInstrIdx);` |
|       97 |   355 | `		if( pInstr ){` |
|       97 |   356 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|       46 |   357 | `		}` |
|       51 |   358 | `	}` |
|  1443373 |   359 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  1443505 |   360 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|      137 |   361 | `		if( aLabel[n].bRef == FALSE ){` |
|        - |   362 | `			/* Emit a warning */` |
|       40 |   363 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|       24 |   364 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|       12 |   365 | `		}` |
|       71 |   366 | `	}` |
|  1443373 |   367 | `	return SXRET_OK;` |
|   721690 |   368 | `}` |
|        - |   369 | `/*` |
|        - |   370 | ` * Check if a given token value is installed in the literal table.` |
|        - |   371 | ` */` |
|  7045162 |   372 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|        5 |   373 | `{` |
|        - |   374 | `	SyHashEntry *pEntry;` |
|  7045167 |   375 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  7045167 |   376 | `	if( pEntry == 0 ){` |
|  1864241 |   377 | `		return SXERR_NOTFOUND;` |
|        - |   378 | `	}` |
|  5180931 |   379 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  5180931 |   380 | `	return SXRET_OK;` |
|  3522586 |   381 | `}` |
|        - |   382 | `/*` |
|        - |   383 | ` * Install a given constant index in the literal table.` |
|        - |   384 | ` * In order to be installed, the ph7_value must be of type string.` |
|        - |   385 | ` *` |
|        - |   386 | ` * NOTE: empty strings are deliberately omitted here.  The VM reserves a` |
|        - |   387 | ` * single shared constant for "" during initialization (pVm->nEmptyStringIdx)` |
|        - |   388 | ` * and the compiler emits a LOADC referencing that slot whenever an empty` |
|        - |   389 | ` * literal is encountered.  This keeps the literal hash from growing when` |
|        - |   390 | ` * many "" literals appear in user code.` |
|        - |   391 | ` */` |
|  1864236 |   392 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|        5 |   393 | `{` |
|  1864241 |   394 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  1864241 |   395 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   932118 |   396 | `	}` |
|  1864241 |   397 | `	return SXRET_OK;` |
|        5 |   398 | `}` |
|        - |   399 | `/*` |
|        - |   400 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|        - |   401 | ` * in the constant table.` |
|        - |   402 | ` */` |
|  1273540 |   403 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|        5 |   404 | `{` |
|        - |   405 | `	ph7_value *pObj;` |
|  1273545 |   406 | `	sxu32 nIdx = 0; /* cc warning */` |
|        - |   407 | `	/* Reserve a new constant */` |
|  1273545 |   408 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  1273545 |   409 | `	if( pObj == 0 ){` |
|      ! 0 |   410 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |   411 | `		return 0;` |
|        - |   412 | `	}` |
|  1273545 |   413 | `	*pIdx = nIdx;` |
|        - |   414 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|        - |   415 | `	 * the constant string iterals table [optimization purposes].` |
|        - |   416 | `	 */` |
|  1273545 |   417 | `	return pObj;` |
|   636775 |   418 | `}` |
|        - |   419 | `/*` |
|        - |   420 | ` * Implementation of the PHP language constructs.` |
|        - |   421 | ` */` |
|        - |   422 | `/*` |
|        - |   423 | ` * Ensure the about-to-be-emitted CALL/NEW opcode carries a VmCallArgMap` |
|        - |   424 | ` * that reflects the caller file's strict_types mode. Returns the (possibly` |
|        - |   425 | ` * newly allocated and zero-initialized) map pointer. In weak-mode files` |
|        - |   426 | ` * this is a no-op and the caller's p3 is returned unchanged.` |
|        - |   427 | ` *` |
|        - |   428 | ` * NOTE: on allocation failure the call reverts to weak semantics rather` |
|        - |   429 | ` * than aborting compilation — out-of-memory during a map allocation is` |
|        - |   430 | ` * vanishingly unlikely and silently dropping to weak mode matches the` |
|        - |   431 | ` * surrounding callsites' zero-check fallback pattern.` |
|        - |   432 | ` */` |
|  3535868 |   433 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|        5 |   434 | `{` |
|        - |   435 | `	VmCallArgMap *pMap;` |
|  3535873 |   436 | `	if( !pGen->bStrictTypes ) return p3;` |
|       39 |   437 | `	if( p3 == 0 ){` |
|       35 |   438 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|       35 |   439 | `		if( pMap == 0 ) return 0;` |
|       35 |   440 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|       35 |   441 | `		p3 = (void *)pMap;` |
|       16 |   442 | `	}` |
|       39 |   443 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|       39 |   444 | `	return p3;` |
|  1767939 |   445 | `}` |
|        - |   446 | `/* Forward declaration */` |
|        - |   447 | `static sxi32 GenStateCompileChunk(ph7_gen_state *pGen,sxi32 iFlags);` |
|        - |   448 | `static void GenStateSetPendingDoc(ph7_gen_state *pGen);` |
|        - |   449 | `static void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut);` |
|        - |   450 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut);` |
|        - |   451 | `static sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut);` |
|        - |   452 | `static sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut);` |
|        - |   453 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx);` |
|        - |   454 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn);` |
|        - |   455 | `/* Forward decl: union type parser is defined later in this file. */` |
|        - |   456 | `static sxi32 GenStateParseUnionTypeDecl(` |
|        - |   457 | `	ph7_gen_state *pGen,` |
|        - |   458 | `	sxu32 *pnType,` |
|        - |   459 | `	SyString *pClass,` |
|        - |   460 | `	SySet *pAlts,` |
|        - |   461 | `	sxi32 *piTypeFlags,` |
|        - |   462 | `	SyString *pTypeText,` |
|        - |   463 | `	int iNullableFlag,` |
|        - |   464 | `	int iUnionFlag,` |
|        - |   465 | `	int bAllowVoid,` |
|        - |   466 | `	sxu32 nLine` |
|        - |   467 | `);` |
|        - |   468 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc);` |
|        - |   469 | `static const char * TokenTypeName(sxu32 nType);` |
|        - |   470 | `/*` |
|        - |   471 | ` * Stack-scratch size for stripping PHP 7.4 numeric separators. A typical` |
|        - |   472 | ` * literal (INT64_MAX decimal is 19 digits, binary 64-bit with per-nibble` |
|        - |   473 | ` * separators is ~80 chars) fits comfortably, so the fast path never touches` |
|        - |   474 | ` * the heap. The language itself imposes no upper bound on the length of a` |
|        - |   475 | ` * well-formed literal — the stripper falls back to a VM-allocator buffer` |
|        - |   476 | ` * for anything larger, so correctness is preserved even for pathological` |
|        - |   477 | ` * inputs like a thousand-digit number.` |
|        - |   478 | ` */` |
|        - |   479 | `#define GEN_NUM_SCRATCH 128` |
|        - |   480 | `/*` |
|        - |   481 | ` * Return TRUE if c is a valid digit for the given numeric base.` |
|        - |   482 | ` *   base 16 => SyisHex (0-9, a-f, A-F)` |
|        - |   483 | ` *   base  2 => 0 or 1` |
|        - |   484 | ` *   base 10 => SyisDigit (0-9, also used for octal literals which share the` |
|        - |   485 | ` *              decimal scan in the lexer)` |
|        - |   486 | ` */` |
|     1076 |   487 | `static int GenStateIsBaseDigit(int c, int base)` |
|        5 |   488 | `{` |
|     1081 |   489 | `	if( base == 16 ){ return SyisHex(c); }` |
|      982 |   490 | `	if( base == 2 ){ return c == '0' \|\| c == '1'; }` |
|      703 |   491 | `	return SyisDigit(c);` |
|      543 |   492 | `}` |
|        - |   493 | `/*` |
|        - |   494 | ` * Given the raw text of a numeric literal token, locate a misplaced PHP 7.4` |
|        - |   495 | ` * underscore separator so the caller can report the malformed portion with` |
|        - |   496 | ` * the exact wording PHP uses:` |
|        - |   497 | ` *` |
|        - |   498 | ` *   syntax error, unexpected identifier "X"` |
|        - |   499 | ` *` |
|        - |   500 | ` * The lexer guarantees that every underscore it consumed as a separator is` |
|        - |   501 | ` * surrounded by valid base digits; anything else sits in the trailing run` |
|        - |   502 | ` * absorbed by the lexer specifically to let this validator see and report` |
|        - |   503 | ` * it. That invariant means the malformed span is exactly [bad .. nByte) —` |
|        - |   504 | ` * no forward rescan needed.` |
|        - |   505 | ` *` |
|        - |   506 | ` * Returns 1 and fills pBadStart / pBadLen when the literal is malformed;` |
|        - |   507 | ` * returns 0 when it is well-formed.` |
|        - |   508 | ` */` |
|  1274510 |   509 | `static int GenStateFindBadNumericSeparator(` |
|        - |   510 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|        5 |   511 | `{` |
|  1274515 |   512 | `	const char *z = pRaw->zString;` |
|  1274515 |   513 | `	sxu32 n = pRaw->nByte;` |
|  1274515 |   514 | `	int base = 10;` |
|        - |   515 | `	sxu32 i, start;` |
|  1274515 |   516 | `	if( n < 2 ) return 0;` |
|   395959 |   517 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|       80 |   518 | `		base = 16;` |
|   395920 |   519 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|      284 |   520 | `		base = 2;` |
|      141 |   521 | `	}` |
|  1280805 |   522 | `	for( i = 0; i < n; ++i ){` |
|   884865 |   523 | `		if( z[i] != '_' ) continue;` |
|      546 |   524 | `		if( i > 0 && i + 1 < n` |
|      543 |   525 | `			&& GenStateIsBaseDigit((unsigned char)z[i-1], base)` |
|      543 |   526 | `			&& GenStateIsBaseDigit((unsigned char)z[i+1], base) ){` |
|      533 |   527 | `			continue; /* well-placed separator */` |
|        - |   528 | `		}` |
|        - |   529 | `		/* First misplaced underscore — the lexer already absorbed the full` |
|        - |   530 | `		 * malformed tail, so it runs from here to the end of the token. */` |
|       18 |   531 | `		start = i;` |
|       23 |   532 | `		if( start > 0 && (z[start-1] == 'x' \|\| z[start-1] == 'X'` |
|       12 |   533 | `			\|\| z[start-1] == 'b' \|\| z[start-1] == 'B') ){` |
|        6 |   534 | `			start--; /* include the base letter for 0x_... / 0b_... */` |
|        2 |   535 | `		}` |
|       18 |   536 | `		*pBadStart = &z[start];` |
|       18 |   537 | `		*pBadLen = n - start;` |
|       18 |   538 | `		return 1;` |
|      ! 0 |   539 | `	}` |
|   395945 |   540 | `	return 0;` |
|   637260 |   541 | `}` |
|        - |   542 | `/*` |
|        - |   543 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|        - |   544 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|        - |   545 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|        - |   546 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|        - |   547 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|        - |   548 | ` * so callers can bail from the current construct).` |
|        - |   549 | ` */` |
|  1274510 |   550 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|        5 |   551 | `{` |
|  1274515 |   552 | `	const char *zBad = 0;` |
|  1274515 |   553 | `	sxu32 nBad = 0;` |
|        - |   554 | `	SyString sBad;` |
|        - |   555 | `	sxi32 rc;` |
|  1274515 |   556 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  1274501 |   557 | `		return SXRET_OK;` |
|        - |   558 | `	}` |
|       18 |   559 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|       18 |   560 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|        - |   561 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|       18 |   562 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |   563 | `		return SXERR_ABORT;` |
|        - |   564 | `	}` |
|       18 |   565 | `	return SXERR_SYNTAX;` |
|   637260 |   566 | `}` |
|        - |   567 | `/*` |
|        - |   568 | ` * Strip PHP 7.4 numeric literal separators (underscores between digits) from` |
|        - |   569 | ` * a numeric token's text and yield a SyString suitable for the low-level` |
|        - |   570 | ` * converters (SyStrToInt64 / SyStrToReal / etc.).` |
|        - |   571 | ` *` |
|        - |   572 | ` * Fast path: if the token contains no '_', *pOut aliases pToken with no copy` |
|        - |   573 | ` * and *pzAlloc is set to NULL.` |
|        - |   574 | ` * Stack path: if the cleaned bytes fit in zScratch, they are written there` |
|        - |   575 | ` * and *pzAlloc is set to NULL.` |
|        - |   576 | ` * Heap path: for literals larger than the scratch buffer, a fresh buffer is` |
|        - |   577 | ` * allocated from pAlloc, returned via *pzAlloc, and must be released by the` |
|        - |   578 | ` * caller with SyMemBackendFree once the converter is done.` |
|        - |   579 | ` *` |
|        - |   580 | ` * Returns SXRET_OK on success, SXERR_ABORT on allocator failure (in which` |
|        - |   581 | ` * case *pOut is left untouched and the caller must not read it).` |
|        - |   582 | ` */` |
|  1274496 |   583 | `static sxi32 GenStateStripNumericSeparators(` |
|        - |   584 | `	SyMemBackend *pAlloc,` |
|        - |   585 | `	const SyString *pToken,` |
|        - |   586 | `	char *zScratch, sxu32 nScratch,` |
|        - |   587 | `	SyString *pOut, char **pzAlloc)` |
|        5 |   588 | `{` |
|        - |   589 | `	sxu32 i, j;` |
|  1274501 |   590 | `	int hasUnderscore = 0;` |
|        - |   591 | `	char *zBuf;` |
|  1274501 |   592 | `	*pzAlloc = 0;` |
|  3035837 |   593 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  1761593 |   594 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   880673 |   595 | `	}` |
|  1274501 |   596 | `	if( !hasUnderscore ){` |
|  1274249 |   597 | `		SyStringDupPtr(pOut, pToken);` |
|  1274249 |   598 | `		return SXRET_OK;` |
|        - |   599 | `	}` |
|      253 |   600 | `	if( pToken->nByte <= nScratch ){` |
|      251 |   601 | `		zBuf = zScratch;` |
|      126 |   602 | `	}else{` |
|        3 |   603 | `		zBuf = (char *)SyMemBackendAlloc(pAlloc, pToken->nByte);` |
|        3 |   604 | `		if( zBuf == 0 ){` |
|      ! 0 |   605 | `			return SXERR_ABORT;` |
|        - |   606 | `		}` |
|        3 |   607 | `		*pzAlloc = zBuf;` |
|        - |   608 | `	}` |
|      253 |   609 | `	j = 0;` |
|     2895 |   610 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|     2643 |   611 | `		if( pToken->zString[i] != '_' ){ zBuf[j++] = pToken->zString[i]; }` |
|     1322 |   612 | `	}` |
|      253 |   613 | `	SyStringInitFromBuf(pOut, zBuf, j);` |
|      253 |   614 | `	return SXRET_OK;` |
|   637253 |   615 | `}` |
|        - |   616 | `/*` |
|        - |   617 | ` * Compile a numeric [i.e: integer or real] literal.` |
|        - |   618 | ` * Notes on the integer type.` |
|        - |   619 | ` *  According to the PHP language reference manual` |
|        - |   620 | ` *  Integers can be specified in decimal (base 10), hexadecimal (base 16), octal (base 8)` |
|        - |   621 | ` *  or binary (base 2) notation, optionally preceded by a sign (- or +).` |
|        - |   622 | ` *  To use octal notation, precede the number with a 0 (zero). To use hexadecimal` |
|        - |   623 | ` *  notation precede the number with 0x. To use binary notation precede the number with 0b.` |
|        - |   624 | ` * Symisc eXtension to the integer type.` |
|        - |   625 | ` *  PH7 introduced platform-independant 64-bit integer unlike the standard PHP engine` |
|        - |   626 | ` *  where the size of an integer is platform-dependent.That is,the size of an integer` |
|        - |   627 | ` *  is 8 bytes and the maximum integer size is 0x7FFFFFFFFFFFFFFF for all platforms` |
|        - |   628 | ` *  [i.e: either 32bit or 64bit].` |
|        - |   629 | ` *  For more information on this powerfull extension please refer to the official` |
|        - |   630 | ` *  documentation.` |
|        - |   631 | ` */` |
|        - |   632 | `/*` |
|        - |   633 | ` * Determine whether an integer literal token exceeds the signed 64-bit range.` |
|        - |   634 | ` * PHP promotes such a literal to a float (e.g. 9223372036854775808 ->` |
|        - |   635 | ` * float(9.22...E+18), 0xFFFFFFFFFFFFFFFF -> float) rather than wrapping or` |
|        - |   636 | ` * dropping digits. pNum is the separator-stripped token (unsigned; the sign of` |
|        - |   637 | ` * a "-1" is a separate unary operator). Base detection mirrors` |
|        - |   638 | ` * PH7_TokenValueToInt64. Returns TRUE on overflow: for a non-decimal base the` |
|        - |   639 | ` * float value is accumulated into *pReal (dv = dv*base + digit); for decimal` |
|        - |   640 | ` * *pbDecimal is set so the caller reuses strtod on the token for a` |
|        - |   641 | ` * correctly-rounded value. Returns FALSE (value fits) for anything it cannot` |
|        - |   642 | ` * confidently classify, so the int path stays in charge.` |
|        - |   643 | ` *` |
|        - |   644 | ` * The int/float CLASSIFICATION is php-exact for every base. VALUES are byte-exact` |
|        - |   645 | ` * for decimal (strtod) and hex (php's zend_hex_strtod uses the same dv*16+digit` |
|        - |   646 | ` * doubling). Octal/binary overflow values can differ from php by the low bit(s):` |
|        - |   647 | ` * php's zend_{oct,bin}_strtod rounds differently than this doubling — e.g. php's` |
|        - |   648 | ` * binary 2**63 is 2**63-1024 whereas this returns the exact 2**63. Recorded as a` |
|        - |   649 | ` * residual in PLAN.md; matching php exactly would need a port of those functions.` |
|        - |   650 | ` */` |
|  1273562 |   651 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|        5 |   652 | `{` |
|  1273567 |   653 | `	const char *z = pNum->zString;` |
|  1273567 |   654 | `	const char *zEnd = z + pNum->nByte;` |
|        - |   655 | `	const char *p, *q;` |
|        - |   656 | `	int n;` |
|  1273567 |   657 | `	*pbDecimal = FALSE;` |
|  1273567 |   658 | `	if( z >= zEnd ){` |
|      ! 0 |   659 | `		return FALSE;` |
|        - |   660 | `	}` |
|  1273567 |   661 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|        - |   662 | `		/* Hexadecimal: INT64_MAX == 0x7FFF...F (16 digits, leading nibble 7). */` |
|       77 |   663 | `		p = z + 2;` |
|       85 |   664 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|      493 |   665 | `		for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){ n++; }` |
|       77 |   666 | `		if( n < 16 \|\| (n == 16 && SyHexToint(p[0]) < 8) ){` |
|       71 |   667 | `			return FALSE;` |
|        - |   668 | `		}` |
|        7 |   669 | `		{ ph7_real dv = 0;` |
|      103 |   670 | `		  for( q = p; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisHex(q[0]); q++ ){` |
|       97 |   671 | `			dv = dv * 16 + (ph7_real)SyHexToint(q[0]);` |
|       49 |   672 | `		  }` |
|        7 |   673 | `		  *pReal = dv;` |
|        - |   674 | `		}` |
|        7 |   675 | `		return TRUE;` |
|  1273491 |   676 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|        - |   677 | `		/* Binary: INT64_MAX needs 63 significant bits. */` |
|      281 |   678 | `		p = z + 2;` |
|      329 |   679 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|     2149 |   680 | `		for( q = p, n = 0; q < zEnd && (q[0] == '0' \|\| q[0] == '1'); q++ ){ n++; }` |
|      281 |   681 | `		if( n <= 63 ){` |
|      279 |   682 | `			return FALSE;` |
|        - |   683 | `		}` |
|        3 |   684 | `		{ ph7_real dv = 0;` |
|      195 |   685 | `		  for( q = p; q < zEnd && (q[0] == '0' \|\| q[0] == '1'); q++ ){` |
|      129 |   686 | `			dv = dv * 2 + (ph7_real)(q[0] - '0');` |
|       65 |   687 | `		  }` |
|        3 |   688 | `		  *pReal = dv;` |
|        - |   689 | `		}` |
|        3 |   690 | `		return TRUE;` |
|  1273211 |   691 | `	}else if( z[0] == '0' ){` |
|        - |   692 | `		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the` |
|        - |   693 | `		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1` |
|        - |   694 | `		 * "0o" marker ends the run and leaves it to the int path (as today). */` |
|   351799 |   695 | `		p = z;` |
|   703595 |   696 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|   352027 |   697 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|   351799 |   698 | `		if( n <= 21 ){` |
|   351797 |   699 | `			return FALSE;` |
|        - |   700 | `		}` |
|        3 |   701 | `		{ ph7_real dv = 0;` |
|       47 |   702 | `		  for( q = p; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){` |
|       45 |   703 | `			dv = dv * 8 + (ph7_real)(q[0] - '0');` |
|       23 |   704 | `		  }` |
|        3 |   705 | `		  *pReal = dv;` |
|        - |   706 | `		}` |
|        3 |   707 | `		return TRUE;` |
|        - |   708 | `	}` |
|        - |   709 | `	/* Decimal: overflow iff more than 19 significant digits, or exactly 19 that` |
|        - |   710 | `	 * compare greater than INT64_MAX. Defer the value to strtod (via the caller)` |
|        - |   711 | `	 * for php-exact rounding. */` |
|   921417 |   712 | `	p = z;` |
|   921417 |   713 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
|  2324515 |   714 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|   921417 |   715 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|       13 |   716 | `		*pbDecimal = TRUE;` |
|       13 |   717 | `		return TRUE;` |
|        - |   718 | `	}` |
|   921405 |   719 | `	return FALSE;` |
|   636786 |   720 | `}` |
|  1274482 |   721 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |   722 | `{` |
|  1274487 |   723 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  1274487 |   724 | `	sxu32 nIdx = 0;` |
|        - |   725 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  1274487 |   726 | `	char *zAlloc = 0;` |
|        - |   727 | `	SyString sNum;` |
|        - |   728 | `	sxi32 rc;` |
|   637241 |   729 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  1274487 |   730 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  1274487 |   731 | `	if( rc != SXRET_OK ){` |
|       14 |   732 | `		return rc;` |
|        - |   733 | `	}` |
|  1911713 |   734 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   637236 |   735 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  1274477 |   736 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   737 | `		return SXERR_ABORT;` |
|        - |   738 | `	}` |
|  1274477 |   739 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|        - |   740 | `		ph7_value *pObj;` |
|        - |   741 | `		sxi64 iValue;` |
|  1273567 |   742 | `		ph7_real rOverflow = 0;` |
|  1273567 |   743 | `		int bDecimalOverflow = 0;` |
|  1273567 |   744 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
|        - |   745 | `			/* Literal exceeds the signed 64-bit range: PHP represents it as a` |
|        - |   746 | `			 * float instead of wrapping/dropping digits. */` |
|       23 |   747 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       23 |   748 | `			if( pObj == 0 ){` |
|      ! 0 |   749 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |   750 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |   751 | `				return SXERR_ABORT;` |
|        - |   752 | `			}` |
|       23 |   753 | `			if( bDecimalOverflow ){` |
|        - |   754 | `				/* strtod on the decimal token yields php-exact rounding. */` |
|       13 |   755 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|       13 |   756 | `				PH7_MemObjToReal(pObj);` |
|        7 |   757 | `			}else{` |
|       11 |   758 | `				PH7_MemObjInitFromReal(pGen->pVm,pObj,rOverflow);` |
|        - |   759 | `			}` |
|       12 |   760 | `		}else{` |
|  1273545 |   761 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|  1273545 |   762 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  1273545 |   763 | `			if( pObj == 0 ){` |
|      ! 0 |   764 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |   765 | `				return SXERR_ABORT;` |
|        - |   766 | `			}` |
|  1273545 |   767 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|        - |   768 | `		}` |
|   636786 |   769 | `	}else{` |
|        - |   770 | `		/* Real number */` |
|        - |   771 | `		ph7_value *pObj;` |
|        - |   772 | `		/* Reserve a new constant */` |
|      915 |   773 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      915 |   774 | `		if( pObj == 0 ){` |
|      ! 0 |   775 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |   776 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |   777 | `			return SXERR_ABORT;` |
|        - |   778 | `		}` |
|      915 |   779 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|      915 |   780 | `		PH7_MemObjToReal(pObj);` |
|        - |   781 | `	}` |
|  1274477 |   782 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        - |   783 | `	/* Emit the load constant instruction */` |
|  1274477 |   784 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - |   785 | `	/* Node successfully compiled */` |
|  1274477 |   786 | `	return SXRET_OK;` |
|   637246 |   787 | `}` |
|        - |   788 | `/*` |
|        - |   789 | ` * Compile a single quoted string.` |
|        - |   790 | ` * According to the PHP language reference manual:` |
|        - |   791 | ` *` |
|        - |   792 | ` *   The simplest way to specify a string is to enclose it in single quotes (the character ' ).` |
|        - |   793 | ` *   To specify a literal single quote, escape it with a backslash (\). To specify a literal` |
|        - |   794 | ` *   backslash, double it (\\). All other instances of backslash will be treated as a literal` |
|        - |   795 | ` *   backslash: this means that the other escape sequences you might be used to, such as \r` |
|        - |   796 | ` *   or \n, will be output literally as specified rather than having any special meaning.` |
|        - |   797 | ` *` |
|        - |   798 | ` */` |
|  2872374 |   799 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |   800 | `{` |
|  2872379 |   801 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|        - |   802 | `	const char *zIn,*zCur,*zEnd;` |
|        - |   803 | `	ph7_value *pObj;` |
|        - |   804 | `	sxu32 nIdx;` |
|  2872379 |   805 | `	nIdx = 0; /* Prevent compiler warning */` |
|        - |   806 | `	/* Delimit the string */` |
|  2872379 |   807 | `	zIn  = pStr->zString;` |
|  2872379 |   808 | `	zEnd = &zIn[pStr->nByte];` |
|  2872379 |   809 | `	if( zIn >= zEnd ){` |
|        - |   810 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|        - |   811 | `		 * rather than reserving a new object each time. */` |
|   127045 |   812 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|   127045 |   813 | `		return SXRET_OK;` |
|        - |   814 | `	}` |
|  2745339 |   815 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|        - |   816 | `		/* Already processed,emit the load constant instruction` |
|        - |   817 | `		 * and return.` |
|        - |   818 | `		 */` |
|  1764719 |   819 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  1764719 |   820 | `		return SXRET_OK;` |
|        - |   821 | `	}` |
|        - |   822 | `	/* Reserve a new constant */` |
|   980625 |   823 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   980625 |   824 | `	if( pObj == 0 ){` |
|      ! 0 |   825 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |   826 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |   827 | `		return SXERR_ABORT;` |
|        - |   828 | `	}` |
|   980625 |   829 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|        - |   830 | `	/* Compile the node */` |
|   980679 |   831 | `	for(;;){` |
|  1961363 |   832 | `		if( zIn >= zEnd ){` |
|        - |   833 | `			/* End of input */` |
|   980625 |   834 | `			break;` |
|        - |   835 | `		}` |
|   980743 |   836 | `		zCur = zIn;` |
| 19401107 |   837 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
| 18420369 |   838 | `			zIn++;` |
|        5 |   839 | `		}` |
|   980743 |   840 | `		if( zIn > zCur ){` |
|        - |   841 | `			/* Append raw contents*/` |
|   949967 |   842 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   474981 |   843 | `		}` |
|   980743 |   844 | `		zIn++;` |
|   980743 |   845 | `		if( zIn < zEnd ){` |
|    30895 |   846 | `			if( zIn[0] == '\\' ){` |
|        - |   847 | `				/* A literal backslash */` |
|    30783 |   848 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|    15503 |   849 | `			}else if( zIn[0] == '\'' ){` |
|        - |   850 | `				/* A single quote */` |
|       11 |   851 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|        6 |   852 | `			}else{` |
|        - |   853 | `				/* verbatim copy */` |
|      104 |   854 | `				zIn--;` |
|      104 |   855 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|      104 |   856 | `				zIn++;` |
|        - |   857 | `			}` |
|    15445 |   858 | `		}` |
|        - |   859 | `		/* Advance the stream cursor */` |
|   980743 |   860 | `		zIn++;` |
|        5 |   861 | `	}` |
|        - |   862 | `	/* Emit the load constant instruction */` |
|   980625 |   863 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   980625 |   864 | `	if( pStr->nByte < 1024 ){` |
|        - |   865 | `		/* Install in the literal table */` |
|   980625 |   866 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   490310 |   867 | `	}` |
|        - |   868 | `	/* Node successfully compiled */` |
|   980625 |   869 | `	return SXRET_OK;` |
|  1436192 |   870 | `}` |
|        - |   871 | `/*` |
|        - |   872 | ` * PHP 7.3 flexible heredoc/nowdoc closing-marker indent stripping.` |
|        - |   873 | ` *` |
|        - |   874 | ` * When the lexer matched the closing marker with leading whitespace on its` |
|        - |   875 | ` * own line, it stored the indent count in pGen->pIn->pUserData. The marker's` |
|        - |   876 | ` * indent prefix bytes sit immediately after the stripped body (at` |
|        - |   877 | ` * pIn->sData.zString + pIn->sData.nByte + 1 for LF, +2 for CRLF) in the` |
|        - |   878 | ` * original source buffer — the buffer is stable through compilation.` |
|        - |   879 | ` *` |
|        - |   880 | `` * For each body line, we remove exactly `nIndent` leading bytes that must`` |
|        - |   881 | ` * byte-for-byte match the marker's prefix. Empty lines (0 bytes or bare \r)` |
|        - |   882 | ` * bypass validation. Mismatches raise the exact PHP 7.3+ parse errors:` |
|        - |   883 | ` *   - "Invalid body indentation level (expecting an indentation level of` |
|        - |   884 | ` *     at least N)" — line too short, or first differing byte is not` |
|        - |   885 | ` *     whitespace.` |
|        - |   886 | ` *   - "Invalid indentation - tabs and spaces cannot be mixed" — first` |
|        - |   887 | ` *     differing byte is whitespace but differs from the marker prefix.` |
|        - |   888 | ` */` |
|      114 |   889 | `static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)` |
|        5 |   890 | `{` |
|      119 |   891 | `	SyString *pIn = &pGen->pIn->sData;` |
|      119 |   892 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        - |   893 | `	const char *zPrefix;` |
|        - |   894 | `	const char *z, *zEnd;` |
|        - |   895 | `	char *zBuf, *zDst;` |
|      119 |   896 | `	if( nIndent == 0 ){` |
|        - |   897 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|       73 |   898 | `		*pOut = *pIn;` |
|       73 |   899 | `		return SXRET_OK;` |
|        - |   900 | `	}` |
|        - |   901 | `	/* Recover the marker indent prefix from the original source buffer.` |
|        - |   902 | `	 * Skip the terminator the lexer stripped: one '\n' plus an optional` |
|        - |   903 | `	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the` |
|        - |   904 | `	 * lexer stripped nothing, so this offset is one byte past the true` |
|        - |   905 | `	 * marker-indent start. That is harmless — the strip loop below never` |
|        - |   906 | `	 * runs (z == zEnd), and zPrefix is never dereferenced. */` |
|       48 |   907 | `	zPrefix = pIn->zString + pIn->nByte;` |
|       48 |   908 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|      ! 0 |   909 | `		zPrefix += 2;` |
|      ! 0 |   910 | `	}else{` |
|       48 |   911 | `		zPrefix += 1;` |
|        - |   912 | `	}` |
|        - |   913 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|       48 |   914 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|       48 |   915 | `	if( zBuf == 0 ){` |
|      ! 0 |   916 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |   917 | `		return SXERR_ABORT;` |
|        - |   918 | `	}` |
|       48 |   919 | `	zDst = zBuf;` |
|       48 |   920 | `	z = pIn->zString;` |
|       48 |   921 | `	zEnd = z + pIn->nByte;` |
|      130 |   922 | `	while( z < zEnd ){` |
|       72 |   923 | `		const char *zLine = z;` |
|        - |   924 | `		sxu32 nLine;` |
|        - |   925 | `		int bEmpty;` |
|      800 |   926 | `		while( z < zEnd && z[0] != '\n' ){` |
|      732 |   927 | `			z++;` |
|        4 |   928 | `		}` |
|       72 |   929 | `		nLine = (sxu32)(z - zLine);` |
|       72 |   930 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|       72 |   931 | `		if( !bEmpty ){` |
|        - |   932 | `			sxu32 i;` |
|       68 |   933 | `			if( nLine < nIndent ){` |
|      ! 0 |   934 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |   935 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|      ! 0 |   936 | `					nIndent);` |
|      ! 0 |   937 | `				return SXERR_ABORT;` |
|        - |   938 | `			}` |
|      270 |   939 | `			for( i = 0; i < nIndent; i++ ){` |
|      214 |   940 | `				if( zLine[i] != zPrefix[i] ){` |
|       11 |   941 | `					unsigned char c = (unsigned char)zLine[i];` |
|       11 |   942 | `					if( c == ' ' \|\| c == '\t' ){` |
|        6 |   943 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |   944 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|        4 |   945 | `					}else{` |
|        8 |   946 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |   947 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|        2 |   948 | `							nIndent);` |
|        - |   949 | `					}` |
|       11 |   950 | `					return SXERR_ABORT;` |
|        - |   951 | `				}` |
|      104 |   952 | `			}` |
|       57 |   953 | `			SyMemcpy((const void *)(zLine + nIndent), (void *)zDst, nLine - nIndent);` |
|       57 |   954 | `			zDst += nLine - nIndent;` |
|       33 |   955 | `		}else if( nLine == 1 ){` |
|        - |   956 | `			/* Preserve the stray '\r' on an otherwise empty line */` |
|      ! 0 |   957 | `			*zDst++ = '\r';` |
|      ! 0 |   958 | `		}` |
|       61 |   959 | `		if( z < zEnd ){` |
|       25 |   960 | `			*zDst++ = '\n';` |
|       25 |   961 | `			z++;` |
|       12 |   962 | `		}` |
|        1 |   963 | `	}` |
|       37 |   964 | `	pOut->zString = zBuf;` |
|       37 |   965 | `	pOut->nByte = (sxu32)(zDst - zBuf);` |
|       37 |   966 | `	return SXRET_OK;` |
|       62 |   967 | `}` |
|        - |   968 | `/*` |
|        - |   969 | ` * Compile a nowdoc string.` |
|        - |   970 | ` * According to the PHP language reference manual:` |
|        - |   971 | ` *` |
|        - |   972 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|        - |   973 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|        - |   974 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the` |
|        - |   975 | ` *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]>` |
|        - |   976 | ` *  construct, in that it declares a block of text which is not for parsing.` |
|        - |   977 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier` |
|        - |   978 | ` *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc` |
|        - |   979 | ` *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance` |
|        - |   980 | ` *  of the closing identifier.` |
|        - |   981 | ` */` |
|       48 |   982 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        3 |   983 | `{` |
|        - |   984 | `	SyString sStripped;` |
|        - |   985 | `	SyString *pStr;` |
|        - |   986 | `	ph7_value *pObj;` |
|        - |   987 | `	sxu32 nIdx;` |
|        - |   988 | `	sxi32 rc;` |
|       51 |   989 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|       51 |   990 | `	if( rc != SXRET_OK ){` |
|        6 |   991 | `		return rc;` |
|        - |   992 | `	}` |
|       46 |   993 | `	pStr = &sStripped;` |
|       46 |   994 | `	nIdx = 0; /* Prevent compiler warning */` |
|       46 |   995 | `	if( pStr->nByte <= 0 ){` |
|        - |   996 | `		/* Empty string,load NULL */` |
|        7 |   997 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        7 |   998 | `		return SXRET_OK;` |
|        - |   999 | `	}` |
|        - |  1000 | `	/* Reserve a new constant */` |
|       40 |  1001 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       40 |  1002 | `	if( pObj == 0 ){` |
|      ! 0 |  1003 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  1004 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  1005 | `		return SXERR_ABORT;` |
|        - |  1006 | `	}` |
|        - |  1007 | `	/* No processing is done here, simply a memcpy() operation */` |
|       40 |  1008 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|        - |  1009 | `	/* Emit the load constant instruction */` |
|       40 |  1010 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - |  1011 | `	/* Node successfully compiled */` |
|       40 |  1012 | `	return SXRET_OK;` |
|       27 |  1013 | `}` |
|        - |  1014 | `/*` |
|        - |  1015 | ` * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.` |
|        - |  1016 | ` * According to the PHP language reference manual` |
|        - |  1017 | ` *   When a string is specified in double quotes or with heredoc,variables are parsed within it.` |
|        - |  1018 | ` *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most` |
|        - |  1019 | ` *  common and convenient. It provides a way to embed a variable, an array value, or an object` |
|        - |  1020 | ` *  property in a string with a minimum of effort.` |
|        - |  1021 | ` *  Simple syntax` |
|        - |  1022 | ` *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible` |
|        - |  1023 | ` *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify` |
|        - |  1024 | ` *   the end of the name.` |
|        - |  1025 | ` *   Similarly, an array index or an object property can be parsed. With array indices, the closing` |
|        - |  1026 | ` *   square bracket (]) marks the end of the index. The same rules apply to object properties` |
|        - |  1027 | ` *   as to simple variables.` |
|        - |  1028 | ` *  Complex (curly) syntax` |
|        - |  1029 | ` *   This isn't called complex because the syntax is complex, but because it allows for the use` |
|        - |  1030 | ` *   of complex expressions.` |
|        - |  1031 | ` *   Any scalar variable, array element or object property with a string representation can be` |
|        - |  1032 | ` *   included via this syntax. Simply write the expression the same way as it would appear outside` |
|        - |  1033 | ` *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only` |
|        - |  1034 | ` *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$` |
|        - |  1035 | ` */` |
|     2382 |  1036 | `static sxi32 GenStateProcessStringExpression(` |
|        - |  1037 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  1038 | `	sxu32 nLine,         /* Line number */` |
|        - |  1039 | `	const char *zIn,     /* Raw expression */` |
|        - |  1040 | `	const char *zEnd     /* End of the expression */` |
|        - |  1041 | `	)` |
|        5 |  1042 | `{` |
|        - |  1043 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |  1044 | `	SySet sToken;` |
|        - |  1045 | `	sxi32 rc;` |
|        - |  1046 | `	/* Initialize the token set */` |
|     2387 |  1047 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        - |  1048 | `	/* Preallocate some slots */` |
|     2387 |  1049 | `	SySetAlloc(&sToken,0x08);` |
|        - |  1050 | `	/* Tokenize the text */` |
|     2387 |  1051 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken,0);` |
|        - |  1052 | `	/* Swap delimiter */` |
|     2387 |  1053 | `	pTmpIn  = pGen->pIn;` |
|     2387 |  1054 | `	pTmpEnd = pGen->pEnd;` |
|     2387 |  1055 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|     2387 |  1056 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|        - |  1057 | `	/* Compile the expression */` |
|     2387 |  1058 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  1059 | `	/* Restore token stream */` |
|     2387 |  1060 | `	pGen->pIn  = pTmpIn;` |
|     2387 |  1061 | `	pGen->pEnd = pTmpEnd;` |
|        - |  1062 | `	/* Release the token set */` |
|     2387 |  1063 | `	SySetRelease(&sToken);` |
|        - |  1064 | `	/* Compilation result */` |
|     2387 |  1065 | `	return rc;` |
|        5 |  1066 | `}` |
|        - |  1067 | `/*` |
|        - |  1068 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|        - |  1069 | ` */` |
|    36050 |  1070 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|        5 |  1071 | `{` |
|        - |  1072 | `	ph7_value *pConstObj;` |
|    36055 |  1073 | `	sxu32 nIdx = 0;` |
|        - |  1074 | `	/* Reserve a new constant */` |
|    36055 |  1075 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    36055 |  1076 | `	if( pConstObj == 0 ){` |
|      ! 0 |  1077 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  1078 | `		return 0;` |
|        - |  1079 | `	}` |
|    36055 |  1080 | `	(*pCount)++;` |
|    36055 |  1081 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|        - |  1082 | `	/* Emit the load constant instruction */` |
|    36055 |  1083 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    36055 |  1084 | `	return pConstObj;` |
|    18030 |  1085 | `}` |
|        - |  1086 | `/*` |
|        - |  1087 | ` * Compile a double quoted/heredoc string.` |
|        - |  1088 | ` * According to the PHP language reference manual` |
|        - |  1089 | ` * Heredoc` |
|        - |  1090 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|        - |  1091 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|        - |  1092 | ` *  to close the quotation.` |
|        - |  1093 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|        - |  1094 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|        - |  1095 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|        - |  1096 | ` *  Warning` |
|        - |  1097 | ` *  It is very important to note that the line with the closing identifier must contain` |
|        - |  1098 | ` *  no other characters, except possibly a semicolon (;). That means especially that the identifier` |
|        - |  1099 | ` *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.` |
|        - |  1100 | ` *  It's also important to realize that the first character before the closing identifier must` |
|        - |  1101 | ` *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.` |
|        - |  1102 | ` *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.` |
|        - |  1103 | ` *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing` |
|        - |  1104 | ` *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before` |
|        - |  1105 | ` *  the end of the current file, a parse error will result at the last line.` |
|        - |  1106 | ` *  Heredocs can not be used for initializing class properties.` |
|        - |  1107 | ` * Double quoted` |
|        - |  1108 | ` *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:` |
|        - |  1109 | ` *  Escaped characters Sequence 	Meaning` |
|        - |  1110 | ` *  \n linefeed (LF or 0x0A (10) in ASCII)` |
|        - |  1111 | ` *  \r carriage return (CR or 0x0D (13) in ASCII)` |
|        - |  1112 | ` *  \t horizontal tab (HT or 0x09 (9) in ASCII)` |
|        - |  1113 | ` *  \v vertical tab (VT or 0x0B (11) in ASCII)` |
|        - |  1114 | ` *  \e escape (ESC or 0x1B (27) in ASCII)` |
|        - |  1115 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|        - |  1116 | ` *  \\ backslash` |
|        - |  1117 | ` *  \$ dollar sign` |
|        - |  1118 | ` *  \" double-quote` |
|        - |  1119 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation,` |
|        - |  1120 | ` *      which silently overflows to fit in a byte (e.g. "\400" === "\000")` |
|        - |  1121 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|        - |  1122 | ` *  \u{[0-9A-Fa-f]+} 	the sequence of characters matching the regular expression is a Unicode codepoint,` |
|        - |  1123 | ` *      which will be output to the string as that codepoint's UTF-8 representation` |
|        - |  1124 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|        - |  1125 | ` * (The PH7-ism "\oNNN" octal form is gone: a literal "\o" now round-trips like php 8.)` |
|        - |  1126 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|        - |  1127 | ` * See string parsing for details.` |
|        - |  1128 | ` */` |
|        - |  1129 | `/*` |
|        - |  1130 | ` * Line number of an escape sequence inside the string body being compiled:` |
|        - |  1131 | ` * the token's line plus every newline before the escape (php reports the` |
|        - |  1132 | ` * escape's own line, not the string's opening line). A heredoc body starts` |
|        - |  1133 | ` * on the line after the '<<<' marker, hence the +1.` |
|        - |  1134 | ` */` |
|        6 |  1135 | `static sxu32 GenStateStringEscLine(ph7_gen_state *pGen,const char *zPos,int bHeredoc)` |
|        3 |  1136 | `{` |
|        9 |  1137 | `	const char *z = pGen->pIn->sData.zString;` |
|        9 |  1138 | `	sxu32 nLine = pGen->pIn->nLine + (bHeredoc ? 1 : 0);` |
|       15 |  1139 | `	for( ; z < zPos ; z++ ){` |
|        9 |  1140 | `		if( z[0] == '\n' ){` |
|      ! 0 |  1141 | `			nLine++;` |
|      ! 0 |  1142 | `		}` |
|        6 |  1143 | `	}` |
|        9 |  1144 | `	return nLine;` |
|        3 |  1145 | `}` |
|        - |  1146 | `/* bHeredoc: php strips the backslash from '\"' only when '"' is the active` |
|        - |  1147 | ` * quote character; a heredoc has none, so '\"' stays verbatim there. */` |
|    34490 |  1148 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|        5 |  1149 | `{` |
|    34495 |  1150 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|        - |  1151 | `	const char *zIn,*zCur,*zEnd;` |
|    34495 |  1152 | `	ph7_value *pObj = 0;` |
|        - |  1153 | `	sxi32 iCons;` |
|        - |  1154 | `	sxi32 rc;` |
|        - |  1155 | `	/* Delimit the string */` |
|    34495 |  1156 | `	zIn  = pStr->zString;` |
|    34495 |  1157 | `	zEnd = &zIn[pStr->nByte];` |
|    34495 |  1158 | `	if( zIn >= zEnd ){` |
|        - |  1159 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|        - |  1160 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|        - |  1161 | `		 * literal table from growing when many "" literals appear in the source.` |
|        - |  1162 | `		 */` |
|      317 |  1163 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|      317 |  1164 | `		return SXRET_OK;` |
|        - |  1165 | `	}` |
|    34183 |  1166 | `	zCur = 0;` |
|        - |  1167 | `	/* Compile the node */` |
|    34183 |  1168 | `	iCons = 0;` |
|    18280 |  1169 | `	for(;;){` |
|    59259 |  1170 | `		zCur = zIn;` |
|   206157 |  1171 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|   149285 |  1172 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|       69 |  1173 | `				break;` |
|   149157 |  1174 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|     2258 |  1175 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     1130 |  1176 | `					break;` |
|        - |  1177 | `			}` |
|   146903 |  1178 | `			zIn++;` |
|        5 |  1179 | `		}` |
|    59259 |  1180 | `		if( zIn > zCur ){` |
|    18999 |  1181 | `			if( pObj == 0 ){` |
|    18475 |  1182 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    18475 |  1183 | `				if( pObj == 0 ){` |
|      ! 0 |  1184 | `					return SXERR_ABORT;` |
|        - |  1185 | `				}` |
|     9235 |  1186 | `			}` |
|    18999 |  1187 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|     9497 |  1188 | `		}` |
|    59259 |  1189 | `		if( zIn >= zEnd ){` |
|    34181 |  1190 | `			break;` |
|        - |  1191 | `		}` |
|    25083 |  1192 | `		if( zIn[0] == '\\' ){` |
|    22701 |  1193 | `			const char *zPtr = 0;` |
|        - |  1194 | `			sxu32 n;` |
|    22701 |  1195 | `			zIn++;` |
|    22701 |  1196 | `			if( pObj == 0 ){` |
|    17585 |  1197 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    17585 |  1198 | `				if( pObj == 0 ){` |
|      ! 0 |  1199 | `					return SXERR_ABORT;` |
|        - |  1200 | `				}` |
|     8790 |  1201 | `			}` |
|    22701 |  1202 | `			if( zIn >= zEnd ){` |
|        - |  1203 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|        3 |  1204 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|        3 |  1205 | `				break;` |
|        - |  1206 | `			}` |
|    22699 |  1207 | `			n = sizeof(char); /* size of conversion */` |
|    22699 |  1208 | `			switch( zIn[0] ){` |
|       11 |  1209 | `			case '$':` |
|        - |  1210 | `				/* Dollar sign */` |
|       25 |  1211 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|       25 |  1212 | `				break;` |
|       56 |  1213 | `			case '\\':` |
|        - |  1214 | `				/* A literal backslash */` |
|      117 |  1215 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      117 |  1216 | `				break;` |
|        1 |  1217 | `			case 'e':` |
|        - |  1218 | `				/* Escape (ESC) ASCII code 27 */` |
|        3 |  1219 | `				PH7_MemObjStringAppend(pObj,"\x1b",sizeof(char));` |
|        3 |  1220 | `				break;` |
|        4 |  1221 | `			case 'f':` |
|        - |  1222 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|        9 |  1223 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|        9 |  1224 | `				break;` |
|    10799 |  1225 | `			case 'n':` |
|        - |  1226 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    21603 |  1227 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    21603 |  1228 | `				break;` |
|       19 |  1229 | `			case 'r':` |
|        - |  1230 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|       43 |  1231 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|       43 |  1232 | `				break;` |
|       26 |  1233 | `			case 't':` |
|        - |  1234 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|       57 |  1235 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|       57 |  1236 | `				break;` |
|        3 |  1237 | `			case 'v':` |
|        - |  1238 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|        7 |  1239 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|        7 |  1240 | `				break;` |
|      113 |  1241 | `			case '"':` |
|      231 |  1242 | `				if( bHeredoc ){` |
|        - |  1243 | `					/* No active quote char in a heredoc: php keeps \" verbatim */` |
|        5 |  1244 | `					PH7_MemObjStringAppend(pObj,"\\\"",sizeof(char)*2);` |
|        3 |  1245 | `				}else{` |
|        - |  1246 | `					/* Double quote */` |
|      227 |  1247 | `					PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|        - |  1248 | `				}` |
|      231 |  1249 | `				break;` |
|       24 |  1250 | `			case '0': case '1': case '2': case '3':` |
|        - |  1251 | `			case '4': case '5': case '6': case '7': {` |
|        - |  1252 | `				/* \[0-7]{1,3}: a character in octal notation. A value above \377` |
|        - |  1253 | `				 * warns and wraps to the low byte, matching php 8. */` |
|       50 |  1254 | `				int c = 0;` |
|        - |  1255 | `				char cOut;` |
|      144 |  1256 | `				for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|      122 |  1257 | `					if( zPtr >= zEnd \|\| zPtr[0] < '0' \|\| zPtr[0] > '7' ){` |
|       14 |  1258 | `						break;` |
|        - |  1259 | `					}` |
|       96 |  1260 | `					c = c * 8 + (zPtr[0] - '0');` |
|       49 |  1261 | `				}` |
|       50 |  1262 | `				if( c > 0xFF ){` |
|        - |  1263 | `					SyString sSeq;` |
|        3 |  1264 | `					SyStringInitFromBuf(&sSeq,zIn,(sxu32)(zPtr-zIn));` |
|        3 |  1265 | `					PH7_GenCompileError(&(*pGen),E_WARNING,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|        - |  1266 | `						"Octal escape sequence overflow \\%z is greater than \\377",&sSeq);` |
|        3 |  1267 | `					c &= 0xFF;` |
|        1 |  1268 | `				}` |
|       50 |  1269 | `				cOut = (char)c; /* value byte, independent of host endianness */` |
|       50 |  1270 | `				PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|       50 |  1271 | `				n = (sxu32)(zPtr-zIn);` |
|       50 |  1272 | `				break;` |
|        - |  1273 | `			}` |
|      270 |  1274 | `			case 'x':` |
|      809 |  1275 | `				if( &zIn[1] < zEnd && SyisHex((unsigned char)zIn[1]) ){` |
|        - |  1276 | `					/* \x[0-9A-Fa-f]{1,2}: a character in hexadecimal notation */` |
|      537 |  1277 | `					int c = SyHexToint(zIn[1]);` |
|        - |  1278 | `					char cOut;` |
|      537 |  1279 | `					n += sizeof(char);` |
|      537 |  1280 | `					if( &zIn[2] < zEnd && SyisHex((unsigned char)zIn[2]) ){` |
|      533 |  1281 | `						c = (c << 4) + SyHexToint(zIn[2]);` |
|      533 |  1282 | `						n += sizeof(char);` |
|      266 |  1283 | `					}` |
|      537 |  1284 | `					cOut = (char)c; /* value byte, independent of host endianness */` |
|      537 |  1285 | `					PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|      269 |  1286 | `				}else{` |
|        - |  1287 | `					/* Not an escape: keep the backslash, as php does */` |
|        5 |  1288 | `					PH7_MemObjStringAppend(pObj,"\\x",sizeof(char)*2);` |
|        - |  1289 | `				}` |
|      541 |  1290 | `				break;` |
|        9 |  1291 | `			case 'u':` |
|       18 |  1292 | `				if( &zIn[1] < zEnd && zIn[1] == '{'` |
|       22 |  1293 | `				 && !(&zIn[2] < zEnd && zIn[2] == '$') ){` |
|        - |  1294 | `					/* \u{codepoint}: UTF-8 encoding of the given codepoint (php 7+).` |
|        - |  1295 | `					 * php encodes surrogates verbatim, so the only invalid value` |
|        - |  1296 | `					 * is > U+10FFFF; malformed/empty braces are a compile error.` |
|        - |  1297 | `					 * "\u{$..." is excluded above: php treats it as a literal \u` |
|        - |  1298 | `					 * followed by {$...} curly interpolation. */` |
|       15 |  1299 | `					sxu32 nCp = 0;` |
|       15 |  1300 | `					zPtr = &zIn[2];` |
|       59 |  1301 | `					while( zPtr < zEnd && SyisHex((unsigned char)zPtr[0]) ){` |
|       46 |  1302 | `						if( nCp <= 0x10FFFF ){` |
|        - |  1303 | `							/* stop accumulating once out of range: keeps a long` |
|        - |  1304 | `							 * digit run from wrapping sxu32 */` |
|       46 |  1305 | `							nCp = nCp * 16 + (sxu32)SyHexToint(zPtr[0]);` |
|       22 |  1306 | `						}` |
|       46 |  1307 | `						zPtr++;` |
|        2 |  1308 | `					}` |
|       15 |  1309 | `					if( zPtr == &zIn[2] \|\| zPtr >= zEnd \|\| zPtr[0] != '}' ){` |
|        - |  1310 | `						/* Error recorded (nErr>0 fails the whole compile); consume the` |
|        - |  1311 | `						 * malformed sequence so later errors are still reported. */` |
|        3 |  1312 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|        - |  1313 | `							"Invalid UTF-8 codepoint escape sequence");` |
|        3 |  1314 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  1315 | `							return SXERR_ABORT;` |
|        - |  1316 | `						}` |
|        3 |  1317 | `						n = (sxu32)(zPtr-zIn);` |
|        3 |  1318 | `						if( zPtr < zEnd && zPtr[0] == '}' ){` |
|        3 |  1319 | `							n += sizeof(char);` |
|        1 |  1320 | `						}` |
|        3 |  1321 | `						break;` |
|        - |  1322 | `					}` |
|       12 |  1323 | `					n = (sxu32)(&zPtr[1]-zIn); /* 'u{...}' incl. closing brace */` |
|       12 |  1324 | `					if( nCp > 0x10FFFF ){` |
|        3 |  1325 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|        - |  1326 | `							"Invalid UTF-8 codepoint escape sequence: Codepoint too large");` |
|        3 |  1327 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  1328 | `							return SXERR_ABORT;` |
|        - |  1329 | `						}` |
|        3 |  1330 | `						break;` |
|        - |  1331 | `					}` |
|        - |  1332 | `					{` |
|        - |  1333 | `						char zUtf[4];` |
|        9 |  1334 | `						sxu8 *zOut = (sxu8 *)zUtf;` |
|        9 |  1335 | `						SX_WRITE_UTF8(zOut,nCp);` |
|        9 |  1336 | `						PH7_MemObjStringAppend(pObj,zUtf,(sxu32)(zOut-(sxu8 *)zUtf));` |
|        - |  1337 | `					}` |
|        5 |  1338 | `				}else{` |
|        - |  1339 | `					/* Not an escape: keep the backslash, as php does */` |
|        7 |  1340 | `					PH7_MemObjStringAppend(pObj,"\\u",sizeof(char)*2);` |
|        - |  1341 | `				}` |
|       15 |  1342 | `				break;` |
|       12 |  1343 | `			default:` |
|        - |  1344 | `				/* Unrecognized escape: keep the backslash, as php does.` |
|        - |  1345 | `				 * zIn[-1] is the backslash itself, so both bytes are contiguous` |
|        - |  1346 | `				 * in the source buffer — one batched append. */` |
|       25 |  1347 | `				PH7_MemObjStringAppend(pObj,&zIn[-1],sizeof(char)*2);` |
|       24 |  1348 | `				break;` |
|        - |  1349 | `			}` |
|        - |  1350 | `			/* Advance the stream cursor */` |
|    22699 |  1351 | `			zIn += n;` |
|    22699 |  1352 | `			continue;` |
|        - |  1353 | `		}` |
|     2387 |  1354 | `		if( zIn[0] == '{' ){` |
|        - |  1355 | `			/* Curly syntax */` |
|        - |  1356 | `			const char *zExpr;` |
|      135 |  1357 | `			sxi32 iNest = 1;` |
|      135 |  1358 | `			zIn++;` |
|      135 |  1359 | `			zExpr = zIn;` |
|        - |  1360 | `			/* Synchronize with the next closing curly braces */` |
|     1383 |  1361 | `			while( zIn < zEnd ){` |
|     1383 |  1362 | `				if( zIn[0] == '{' ){` |
|        - |  1363 | `					/* Increment nesting level */` |
|        9 |  1364 | `					iNest++;` |
|     1379 |  1365 | `				}else if(zIn[0] == '}' ){` |
|        - |  1366 | `					/* Decrement nesting level */` |
|      143 |  1367 | `					iNest--;` |
|      143 |  1368 | `					if( iNest <= 0 ){` |
|      135 |  1369 | `						break;` |
|        - |  1370 | `					}` |
|        4 |  1371 | `				}` |
|     1251 |  1372 | `				zIn++;` |
|        3 |  1373 | `			}` |
|        - |  1374 | `			/* Process the expression */` |
|      135 |  1375 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|      135 |  1376 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  1377 | `				return SXERR_ABORT;` |
|        - |  1378 | `			}` |
|      135 |  1379 | `			if( rc != SXERR_EMPTY ){` |
|      135 |  1380 | `				++iCons;` |
|       66 |  1381 | `			}` |
|      135 |  1382 | `			if( zIn < zEnd ){` |
|        - |  1383 | `				/* Jump the trailing curly */` |
|      135 |  1384 | `				zIn++;` |
|       66 |  1385 | `			}` |
|       69 |  1386 | `		}else{` |
|        - |  1387 | `			/* Simple syntax */` |
|     2255 |  1388 | `			const char *zExpr = zIn;` |
|        - |  1389 | `			/* Assemble variable name */` |
|     1150 |  1390 | `			for(;;){` |
|        - |  1391 | `				/* Jump leading dollars */` |
|     4555 |  1392 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|     2255 |  1393 | `					zIn++;` |
|        5 |  1394 | `				}` |
|     1150 |  1395 | `				for(;;){` |
|    12275 |  1396 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|     8825 |  1397 | `						zIn++;` |
|        5 |  1398 | `					}` |
|     2305 |  1399 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|        - |  1400 | `						/* UTF-8 stream */` |
|      ! 0 |  1401 | `						zIn++;` |
|      ! 0 |  1402 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  1403 | `							zIn++;` |
|      ! 0 |  1404 | `						}` |
|      ! 0 |  1405 | `						continue;` |
|        - |  1406 | `					}` |
|     2305 |  1407 | `					break;` |
|      ! 0 |  1408 | `				}` |
|     2305 |  1409 | `				if( zIn >= zEnd ){` |
|      226 |  1410 | `					break;` |
|        - |  1411 | `				}` |
|     2083 |  1412 | `				if( zIn[0] == '[' ){` |
|       12 |  1413 | `					sxi32 iSquare = 1;` |
|       12 |  1414 | `					zIn++;` |
|       28 |  1415 | `					while( zIn < zEnd ){` |
|       28 |  1416 | `						if( zIn[0] == '[' ){` |
|      ! 0 |  1417 | `							iSquare++;` |
|       28 |  1418 | `						}else if (zIn[0] == ']' ){` |
|       12 |  1419 | `							iSquare--;` |
|       12 |  1420 | `							if( iSquare <= 0 ){` |
|       12 |  1421 | `								break;` |
|        - |  1422 | `							}` |
|      ! 0 |  1423 | `						}` |
|       18 |  1424 | `						zIn++;` |
|        2 |  1425 | `					}` |
|       12 |  1426 | `					if( zIn < zEnd ){` |
|       12 |  1427 | `						zIn++;` |
|        5 |  1428 | `					}` |
|       12 |  1429 | `					break;` |
|     2073 |  1430 | `				}else if(zIn[0] == '{' ){` |
|        6 |  1431 | `					sxi32 iCurly = 1;` |
|        6 |  1432 | `					zIn++;` |
|       18 |  1433 | `					while( zIn < zEnd ){` |
|       16 |  1434 | `						if( zIn[0] == '{' ){` |
|      ! 0 |  1435 | `							iCurly++;` |
|       16 |  1436 | `						}else if (zIn[0] == '}' ){` |
|        3 |  1437 | `							iCurly--;` |
|        3 |  1438 | `							if( iCurly <= 0 ){` |
|        3 |  1439 | `								break;` |
|        - |  1440 | `							}` |
|      ! 0 |  1441 | `						}` |
|       14 |  1442 | `						zIn++;` |
|        2 |  1443 | `					}` |
|        6 |  1444 | `					if( zIn < zEnd ){` |
|        3 |  1445 | `						zIn++;` |
|        1 |  1446 | `					}` |
|        6 |  1447 | `					break;` |
|     2069 |  1448 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|        - |  1449 | `					/* Member access operator '->' */` |
|       53 |  1450 | `					zIn += 2;` |
|     2044 |  1451 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|        - |  1452 | `					/* Static member access operator '::' */` |
|      ! 0 |  1453 | `					zIn += 2;` |
|      ! 0 |  1454 | `				}else{` |
|     1012 |  1455 | `					break;` |
|        - |  1456 | `				}` |
|        3 |  1457 | `			}` |
|        - |  1458 | `			/* Process the expression */` |
|     2255 |  1459 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     2255 |  1460 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  1461 | `				return SXERR_ABORT;` |
|        - |  1462 | `			}` |
|     2255 |  1463 | `			if( rc != SXERR_EMPTY ){` |
|     2253 |  1464 | `				++iCons;` |
|     1124 |  1465 | `			}` |
|        - |  1466 | `		}` |
|        - |  1467 | `		/* Invalidate the previously used constant */` |
|     2387 |  1468 | `		pObj = 0;` |
|        5 |  1469 | `	}/*for(;;)*/` |
|    34183 |  1470 | `	if( iCons > 1 ){` |
|        - |  1471 | `		/* Concatenate all compiled constants */` |
|     1759 |  1472 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|      877 |  1473 | `	}` |
|        - |  1474 | `	/* Node successfully compiled */` |
|    34183 |  1475 | `	return SXRET_OK;` |
|    17250 |  1476 | `}` |
|        - |  1477 | `/*` |
|        - |  1478 | ` * Compile a double quoted string.` |
|        - |  1479 | ` *  See the block-comment above for more information.` |
|        - |  1480 | ` */` |
|    34428 |  1481 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  1482 | `{` |
|        - |  1483 | `	sxi32 rc;` |
|    34433 |  1484 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|    17214 |  1485 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  1486 | `	/* Compilation result */` |
|    34433 |  1487 | `	return rc;` |
|        5 |  1488 | `}` |
|        - |  1489 | `/*` |
|        - |  1490 | ` * Compile a Heredoc string.` |
|        - |  1491 | ` *  See the block-comment above for more information.` |
|        - |  1492 | ` */` |
|       66 |  1493 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  1494 | `{` |
|        - |  1495 | `	SyString sOrig, sStripped;` |
|        - |  1496 | `	sxi32 rc;` |
|       71 |  1497 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|       71 |  1498 | `	if( rc != SXRET_OK ){` |
|        6 |  1499 | `		return rc;` |
|        - |  1500 | `	}` |
|        - |  1501 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|        - |  1502 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|        - |  1503 | `	 * Restore before returning so downstream code that references pIn is` |
|        - |  1504 | `	 * unaffected, including on the error path. */` |
|       65 |  1505 | `	sOrig = pGen->pIn->sData;` |
|       65 |  1506 | `	pGen->pIn->sData = sStripped;` |
|       65 |  1507 | `	rc = GenStateCompileString(&(*pGen),1/*bHeredoc*/);` |
|       65 |  1508 | `	pGen->pIn->sData = sOrig;` |
|       31 |  1509 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       65 |  1510 | `	return rc;` |
|       38 |  1511 | `}` |
|        - |  1512 | `/*` |
|        - |  1513 | ` * Compile an array entry whether it is a key or a value.` |
|        - |  1514 | ` *  Notes on array entries.` |
|        - |  1515 | ` *  According to the PHP language reference manual` |
|        - |  1516 | ` *  An array can be created by the array() language construct.` |
|        - |  1517 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|        - |  1518 | ` *  array(  key =>  value` |
|        - |  1519 | ` *    , ...` |
|        - |  1520 | ` *    )` |
|        - |  1521 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|        - |  1522 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|        - |  1523 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|        - |  1524 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|        - |  1525 | ` *  contain integer and string indices.` |
|        - |  1526 | ` *  A value can be any PHP type.` |
|        - |  1527 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|        - |  1528 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|        - |  1529 | ` *  is specified, that value will be overwritten.` |
|        - |  1530 | ` */` |
|   508348 |  1531 | `static sxi32 GenStateCompileArrayEntry(` |
|        - |  1532 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  1533 | `	SyToken *pIn,        /* Token stream */` |
|        - |  1534 | `	SyToken *pEnd,       /* End of the token stream */` |
|        - |  1535 | `	sxi32 iFlags,        /* Compilation flags */` |
|        - |  1536 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|        - |  1537 | `	)` |
|        5 |  1538 | `{` |
|        - |  1539 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |  1540 | `	sxi32 rc;` |
|        - |  1541 | `	/* Swap token stream */` |
|   508353 |  1542 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|        - |  1543 | `	/* Compile the expression*/` |
|   508353 |  1544 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|        - |  1545 | `	/* Restore token stream */` |
|   508353 |  1546 | `	RE_SWAP_DELIMITER(pGen);` |
|   508353 |  1547 | `	return rc;` |
|        5 |  1548 | `}` |
|        - |  1549 | `/*` |
|        - |  1550 | ` * Expression tree validator callback for the 'array' language construct.` |
|        - |  1551 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|        - |  1552 | ` * an invalid expression tree and this function will generate the appropriate` |
|        - |  1553 | ` * error message.` |
|        - |  1554 | ` * See the routine responible of compiling the array language construct` |
|        - |  1555 | ` * for more inforation.` |
|        - |  1556 | ` */` |
|       36 |  1557 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 |  1558 | `{` |
|       41 |  1559 | `	sxi32 rc = SXRET_OK;` |
|       41 |  1560 | `	if( pRoot->pOp ){` |
|       14 |  1561 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|       12 |  1562 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|       17 |  1563 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|        - |  1564 | `			/* Unexpected expression */` |
|       14 |  1565 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - |  1566 | `				"array(): Expecting a variable/array member/function call after reference operator '&'");` |
|       14 |  1567 | `			if( rc != SXERR_ABORT ){` |
|       14 |  1568 | `				rc = SXERR_INVALID;` |
|        5 |  1569 | `			}` |
|       10 |  1570 | `		}` |
|       31 |  1571 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|        - |  1572 | `		/* Unexpected expression */` |
|        3 |  1573 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - |  1574 | `			"array(): Expecting a variable after reference operator '&'");` |
|        3 |  1575 | `		if( rc != SXERR_ABORT ){` |
|        3 |  1576 | `			rc = SXERR_INVALID;` |
|        1 |  1577 | `		}` |
|        1 |  1578 | `	}` |
|       41 |  1579 | `	return rc;` |
|        5 |  1580 | `}` |
|        - |  1581 | `/*` |
|        - |  1582 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|        - |  1583 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|        - |  1584 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|        - |  1585 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|        - |  1586 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|        - |  1587 | ` */` |
|   541680 |  1588 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|        5 |  1589 | `{` |
|   541685 |  1590 | `	SyToken *pCur = pStart;` |
|   541685 |  1591 | `	sxi32 iNest = 0;` |
|  1647479 |  1592 | `	while( pCur < pEnd ){` |
|  1307755 |  1593 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|   201957 |  1594 | `			return pCur;` |
|        - |  1595 | `		}` |
|        - |  1596 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|        - |  1597 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|        - |  1598 | `		 * not an entry separator. Skip past the signature.` |
|        - |  1599 | `		 */` |
|  1105803 |  1600 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|    19317 |  1601 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    19317 |  1602 | `			SyToken *pFn = pCur;` |
|    19312 |  1603 | `			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd` |
|      ! 0 |  1604 | `				&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|        5 |  1605 | `				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|      ! 0 |  1606 | `				pFn = &pCur[1];` |
|      ! 0 |  1607 | `				nKw = PH7_TKWRD_FN;` |
|      ! 0 |  1608 | `			}` |
|    19317 |  1609 | `			if( nKw == PH7_TKWRD_FN ){` |
|        5 |  1610 | `				pCur = pFn + 1; /* past 'fn' */` |
|        5 |  1611 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  1612 | `					pCur++;` |
|      ! 0 |  1613 | `				}` |
|        5 |  1614 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|        5 |  1615 | `					pCur++;` |
|        5 |  1616 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|        - |  1617 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|        5 |  1618 | `					if( pCur < pEnd ){` |
|        5 |  1619 | `						pCur++;` |
|        2 |  1620 | `					}` |
|        2 |  1621 | `				}` |
|        5 |  1622 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_COLON) ){` |
|      ! 0 |  1623 | `					pCur++;` |
|      ! 0 |  1624 | `					if( pCur < pEnd && (pCur->nType & PH7_TK_OP)` |
|      ! 0 |  1625 | `						&& pCur->sData.nByte == 1` |
|      ! 0 |  1626 | `						&& pCur->sData.zString[0] == '?' ){` |
|      ! 0 |  1627 | `						pCur++;` |
|      ! 0 |  1628 | `					}` |
|      ! 0 |  1629 | `					if( pCur < pEnd` |
|      ! 0 |  1630 | `						&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|      ! 0 |  1631 | `						pCur++;` |
|      ! 0 |  1632 | `					}` |
|      ! 0 |  1633 | `				}` |
|        - |  1634 | `				/* The rest of the entry is the arrow-function body — no outer` |
|        - |  1635 | `				 * key to extract. */` |
|        5 |  1636 | `				return pEnd;` |
|        - |  1637 | `			}` |
|        - |  1638 | `			/* Match expression (PHP 8.0): the '=>' inside match arms is not an` |
|        - |  1639 | `			 * entry separator. Skip past the full match span. */` |
|    19313 |  1640 | `			if( nKw == PH7_TKWRD_MATCH ){` |
|        3 |  1641 | `				pCur++; /* past 'match' */` |
|        3 |  1642 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|        3 |  1643 | `					pCur++;` |
|        3 |  1644 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|        - |  1645 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|        3 |  1646 | `					if( pCur < pEnd ){` |
|        3 |  1647 | `						pCur++;` |
|        1 |  1648 | `					}` |
|        1 |  1649 | `				}` |
|        3 |  1650 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_OCB) ){` |
|        3 |  1651 | `					pCur++;` |
|        3 |  1652 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|        - |  1653 | `						PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|        3 |  1654 | `					if( pCur < pEnd ){` |
|        3 |  1655 | `						pCur++;` |
|        1 |  1656 | `					}` |
|        1 |  1657 | `				}` |
|        3 |  1658 | `				continue;` |
|        - |  1659 | `			}` |
|     9653 |  1660 | `		}` |
|  1105797 |  1661 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|    50401 |  1662 | `			iNest++;` |
|  1080599 |  1663 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|        - |  1664 | `			/* Don't worry about mismatched brackets here, the expression` |
|        - |  1665 | `			 * parser will shortly detect any syntax error. */` |
|    50401 |  1666 | `			iNest--;` |
|    25198 |  1667 | `		}` |
|  1105797 |  1668 | `		pCur++;` |
|        5 |  1669 | `	}` |
|   339729 |  1670 | `	return pEnd;` |
|   270845 |  1671 | `}` |
|        - |  1672 | `/*` |
|        - |  1673 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|        - |  1674 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|        - |  1675 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|        - |  1676 | ` */` |
|   283852 |  1677 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|        5 |  1678 | `{` |
|        - |  1679 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|        - |  1680 | `	SyToken *pKey,*pCur;` |
|   283857 |  1681 | `	sxi32 iEmitRef = 0;` |
|   283857 |  1682 | `	sxi32 iSpread = 0;` |
|   283857 |  1683 | `	sxi32 nPair = 0;` |
|        - |  1684 | `	sxi32 rc;` |
|   283857 |  1685 | `	xValidator = 0;` |
|   327814 |  1686 | `	for(;;){` |
|        - |  1687 | `		/* Jump leading commas */` |
|   932423 |  1688 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|   276795 |  1689 | `			pGen->pIn++;` |
|        5 |  1690 | `		}` |
|   655633 |  1691 | `		pCur = pGen->pIn;` |
|   655633 |  1692 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|        - |  1693 | `			/* No more entry to process */` |
|   283841 |  1694 | `			break;` |
|        - |  1695 | `		}` |
|   371797 |  1696 | `		if( pCur >= pGen->pIn ){` |
|      ! 0 |  1697 | `			continue;` |
|        - |  1698 | `		}` |
|        - |  1699 | `		/* Compile the key if available */` |
|   371797 |  1700 | `		pKey = pCur;` |
|   371797 |  1701 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   371797 |  1702 | `		rc = SXERR_EMPTY;` |
|   371797 |  1703 | `		if( pCur < pGen->pIn ){` |
|   136317 |  1704 | `			if( &pCur[1] >= pGen->pIn ){` |
|        - |  1705 | `				/* Missing value */` |
|       13 |  1706 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|       13 |  1707 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  1708 | `					return SXERR_ABORT;` |
|        - |  1709 | `				}` |
|       13 |  1710 | `				return SXRET_OK;` |
|        - |  1711 | `			}` |
|        - |  1712 | `			/* Compile the expression holding the key */` |
|   136307 |  1713 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|        - |  1714 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|   136307 |  1715 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  1716 | `				return SXERR_ABORT;` |
|        - |  1717 | `			}` |
|   136307 |  1718 | `			pCur++; /* Jump the '=>' operator */` |
|   303636 |  1719 | `		}else if( pKey == pCur ){` |
|        - |  1720 | `			/* Key is omitted,emit a warning */` |
|      ! 0 |  1721 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|      ! 0 |  1722 | `			pCur++; /* Jump the '=>' operator */` |
|      ! 0 |  1723 | `		}else{` |
|        - |  1724 | `			/* Reset back the cursor and point to the entry value */` |
|   235485 |  1725 | `			pCur = pKey;` |
|        - |  1726 | `		}` |
|   371787 |  1727 | `		if( rc == SXERR_EMPTY ){` |
|        - |  1728 | `			/* No available key,load NULL */` |
|   235487 |  1729 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|   117741 |  1730 | `		}` |
|   371787 |  1731 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|        - |  1732 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|       45 |  1733 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|       45 |  1734 | `			iEmitRef = 1;` |
|       45 |  1735 | `			pCur++; /* Jump the '&' token */` |
|       45 |  1736 | `			if( pCur >= pGen->pIn ){` |
|        - |  1737 | `				/* Missing value */` |
|        3 |  1738 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|        3 |  1739 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  1740 | `					return SXERR_ABORT;` |
|        - |  1741 | `				}` |
|        3 |  1742 | `				return SXRET_OK;` |
|        - |  1743 | `			}` |
|       19 |  1744 | `		}` |
|        - |  1745 | `		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with` |
|        - |  1746 | `		 * string-key support since PHP 8.1). The parser strips the '...' inside` |
|        - |  1747 | `		 * ExprExtractNode; we only need to know it's there so we can emit` |
|        - |  1748 | `		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the` |
|        - |  1749 | `		 * resulting hashmap rather than insert it as a scalar entry. */` |
|   371785 |  1750 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   371785 |  1751 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
|        - |  1752 | `			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the` |
|        - |  1753 | `			 * '...' token cannot follow either '=>' or '&' inside an array` |
|        - |  1754 | `			 * literal. Emit the same Parse-error wording PHP uses so the` |
|        - |  1755 | `			 * output is engine-portable. */` |
|        6 |  1756 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|        - |  1757 | `				"syntax error, unexpected token \"...\"");` |
|        6 |  1758 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  1759 | `				return SXERR_ABORT;` |
|        - |  1760 | `			}` |
|        6 |  1761 | `			return SXRET_OK;` |
|        - |  1762 | `		}` |
|        - |  1763 | `		/* Compile indice value */` |
|   371781 |  1764 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   371781 |  1765 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  1766 | `			return SXERR_ABORT;` |
|        - |  1767 | `		}` |
|   371781 |  1768 | `		if( iSpread ){` |
|        - |  1769 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|       64 |  1770 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   371750 |  1771 | `		}else if( iEmitRef ){` |
|        - |  1772 | `			/* Emit the load reference instruction */` |
|       41 |  1773 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|       18 |  1774 | `		}` |
|   371781 |  1775 | `		xValidator = 0;` |
|   371781 |  1776 | `		iEmitRef = 0;` |
|   371781 |  1777 | `		iSpread = 0;` |
|   371781 |  1778 | `		nPair++;` |
|        5 |  1779 | `	}` |
|        - |  1780 | `	/* Emit the load map instruction */` |
|   283841 |  1781 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|        - |  1782 | `	/* Node successfully compiled */` |
|   283841 |  1783 | `	return SXRET_OK;` |
|   141931 |  1784 | `}` |
|        - |  1785 | `/*` |
|        - |  1786 | ` * Compile the 'array' language construct.` |
|        - |  1787 | ` *	 According to the PHP language reference manual` |
|        - |  1788 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|        - |  1789 | ` *   values to keys. This type is optimized for several different uses; it can` |
|        - |  1790 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|        - |  1791 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|        - |  1792 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|        - |  1793 | ` */` |
|   282460 |  1794 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  1795 | `{` |
|        - |  1796 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   282465 |  1797 | `	pGen->pIn += 2;` |
|   282465 |  1798 | `	pGen->pEnd--;` |
|   141230 |  1799 | `	SXUNUSED(iCompileFlag);` |
|   282465 |  1800 | `	return GenStateCompileArrayBody(pGen);` |
|        5 |  1801 | `}` |
|        - |  1802 | `/*` |
|        - |  1803 | ` * Compile the PHP 8.5 clone(...) call form:` |
|        - |  1804 | `` *   clone($object)                          -> identical to the `clone $object` operator`` |
|        - |  1805 | ` *   clone($object, ['prop' => value, ...])  -> clone, run __clone(), then apply the` |
|        - |  1806 | ` *                                              property updates as scope-aware writes` |
|        - |  1807 | ` *   clone(object: $o, withProperties: [..]) -> the named-argument spelling` |
|        - |  1808 | ` * Codegen: compile the object argument and emit OP_CLONE (which clones and runs` |
|        - |  1809 | ` * __clone()); if a withProperties argument is present, compile it and emit` |
|        - |  1810 | ` * OP_CLONE_APPLY, which applies each update to the fresh clone AFTER __clone(),` |
|        - |  1811 | ` * honouring visibility / readonly-set-scope / typed-property enforcement in the` |
|        - |  1812 | ` * calling scope. The parser (ExprExtractNode) delimited this node's tokens as` |
|        - |  1813 | `` * `clone ( ... )`; pGen->pIn/pEnd point at the first/one-past-last of that range.`` |
|        - |  1814 | ` */` |
|       22 |  1815 | `PH7_PRIVATE sxi32 PH7_CompileCloneCall(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        2 |  1816 | `{` |
|        - |  1817 | `	SyToken *pIn,*pEnd,*pNext;` |
|       24 |  1818 | `	SyToken *pObjStart = 0,*pObjEnd = 0;` |
|       24 |  1819 | `	SyToken *pUpdStart = 0,*pUpdEnd = 0;` |
|       24 |  1820 | `	int nArg = 0;` |
|        - |  1821 | `	sxi32 rc;` |
|       11 |  1822 | `	SXUNUSED(iCompileFlag);` |
|        - |  1823 | `	/* pGen->pIn -> 'clone', pGen->pIn[1] -> '(', pGen->pEnd -> one past ')'. */` |
|       24 |  1824 | `	pIn  = pGen->pIn + 2;   /* skip 'clone' and the opening '(' */` |
|       24 |  1825 | `	pEnd = pGen->pEnd - 1;  /* exclude the closing ')' */` |
|        - |  1826 | `	/* clone(...) first-class-callable form: a lone ellipsis is the whole list. */` |
|       24 |  1827 | `	if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|      ! 0 |  1828 | `		return PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|        - |  1829 | `			"clone(...) first-class callable form is not yet supported");` |
|        - |  1830 | `	}` |
|        - |  1831 | `	/* Split the (at most two) comma-separated arguments, tolerating named labels. */` |
|       62 |  1832 | `	while( pIn < pEnd ){` |
|       40 |  1833 | `		SyToken *pArgStart,*pArgEnd,*pName = 0;` |
|       40 |  1834 | `		if( PH7_GetNextExpr(pIn,pEnd,&pNext) != SXRET_OK ){` |
|      ! 0 |  1835 | `			break;` |
|        - |  1836 | `		}` |
|       40 |  1837 | `		pArgStart = pIn;` |
|       40 |  1838 | `		pArgEnd   = pNext;` |
|        - |  1839 | `		/* Named-argument label: <ID\|keyword> ':' expr. A single ':' is PH7_TK_COLON;` |
|        - |  1840 | ``		 * '::' is a distinct operator token, so this never mis-fires on `A::B`. */`` |
|       38 |  1841 | `		if( (pArgEnd - pArgStart) >= 2` |
|       37 |  1842 | `			&& (pArgStart[0].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|       23 |  1843 | `			&& (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        5 |  1844 | `			pName = pArgStart;` |
|        5 |  1845 | `			pArgStart += 2;` |
|        2 |  1846 | `		}` |
|       40 |  1847 | `		if( pName ){` |
|        - |  1848 | `` 			/* PHP named parameters are case-SENSITIVE, so `Object:`/`WITHPROPERTIES:` `` |
|        - |  1849 | `			 * must be rejected as unknown (SyMemcmp, not SyStrnicmp). */` |
|        4 |  1850 | `			if( pName->sData.nByte == sizeof("object")-1` |
|        4 |  1851 | `				&& SyMemcmp(pName->sData.zString,"object",sizeof("object")-1) == 0 ){` |
|        3 |  1852 | `				pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|        4 |  1853 | `			}else if( pName->sData.nByte == sizeof("withProperties")-1` |
|        3 |  1854 | `				&& SyMemcmp(pName->sData.zString,"withProperties",sizeof("withProperties")-1) == 0 ){` |
|        3 |  1855 | `				pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|        2 |  1856 | `			}else{` |
|      ! 0 |  1857 | `				return PH7_GenCompileError(pGen,E_ERROR,pName->nLine,` |
|      ! 0 |  1858 | `					"Unknown named parameter $%z",&pName->sData);` |
|        1 |  1859 | `			}` |
|       38 |  1860 | `		}else if( nArg == 0 ){` |
|       22 |  1861 | `			pObjStart = pArgStart; pObjEnd = pArgEnd;` |
|       25 |  1862 | `		}else if( nArg == 1 ){` |
|       15 |  1863 | `			pUpdStart = pArgStart; pUpdEnd = pArgEnd;` |
|        8 |  1864 | `		}else{` |
|      ! 0 |  1865 | `			return PH7_GenCompileError(pGen,E_ERROR,pArgStart->nLine,` |
|        - |  1866 | `				"clone() expects at most 2 arguments");` |
|        - |  1867 | `		}` |
|       40 |  1868 | `		nArg++;` |
|       40 |  1869 | `		pIn = pNext;` |
|       40 |  1870 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|       17 |  1871 | `			pIn++; /* step over the argument separator */` |
|        8 |  1872 | `		}` |
|        2 |  1873 | `	}` |
|       24 |  1874 | `	if( pObjStart == 0 \|\| pObjStart >= pObjEnd ){` |
|      ! 0 |  1875 | `		return PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  1876 | `			"clone() expects at least 1 argument, 0 given");` |
|        - |  1877 | `	}` |
|        - |  1878 | `	/* Object argument -> clone (+ __clone()). */` |
|       24 |  1879 | `	rc = GenStateCompileArrayEntry(pGen,pObjStart,pObjEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|       24 |  1880 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  1881 | `		return SXERR_ABORT;` |
|        - |  1882 | `	}` |
|       24 |  1883 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE,0,0,0,0);` |
|        - |  1884 | `	/* Property updates (evaluated after __clone runs). */` |
|       24 |  1885 | `	if( pUpdStart && pUpdStart < pUpdEnd ){` |
|       17 |  1886 | `		rc = GenStateCompileArrayEntry(pGen,pUpdStart,pUpdEnd,EXPR_FLAG_RDONLY_LOAD,0);` |
|       17 |  1887 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  1888 | `			return SXERR_ABORT;` |
|        - |  1889 | `		}` |
|       17 |  1890 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CLONE_APPLY,0,0,0,0);` |
|        8 |  1891 | `	}` |
|       24 |  1892 | `	return SXRET_OK;` |
|       13 |  1893 | `}` |
|        - |  1894 | `/*` |
|        - |  1895 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|        - |  1896 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|        - |  1897 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|        - |  1898 | ` */` |
|     1392 |  1899 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  1900 | `{` |
|        - |  1901 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     1397 |  1902 | `	pGen->pIn++;` |
|     1397 |  1903 | `	pGen->pEnd--;` |
|      696 |  1904 | `	SXUNUSED(iCompileFlag);` |
|     1397 |  1905 | `	return GenStateCompileArrayBody(pGen);` |
|        5 |  1906 | `}` |
|        - |  1907 | `/*` |
|        - |  1908 | ` * Expression tree validator callback for the 'list' language construct.` |
|        - |  1909 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|        - |  1910 | ` * an invalid expression tree and this function will generate the appropriate` |
|        - |  1911 | ` * error message.` |
|        - |  1912 | ` * See the routine responible of compiling the list language construct` |
|        - |  1913 | ` * for more inforation.` |
|        - |  1914 | ` */` |
|      190 |  1915 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 |  1916 | `{` |
|      195 |  1917 | `	sxi32 rc = SXRET_OK;` |
|      195 |  1918 | `	if( pRoot->pOp ){` |
|        4 |  1919 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|        2 |  1920 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|        - |  1921 | `				/* Unexpected expression */` |
|      ! 0 |  1922 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - |  1923 | `					"list(): Expecting a variable not an expression");` |
|      ! 0 |  1924 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 |  1925 | `					rc = SXERR_INVALID;` |
|      ! 0 |  1926 | `				}` |
|        1 |  1927 | `		}` |
|      193 |  1928 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|        - |  1929 | `		/* Unexpected expression */` |
|        6 |  1930 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - |  1931 | `			"list(): Expecting a variable not an expression");` |
|        6 |  1932 | `		if( rc != SXERR_ABORT ){` |
|        6 |  1933 | `			rc = SXERR_INVALID;` |
|        2 |  1934 | `		}` |
|        2 |  1935 | `	}` |
|      195 |  1936 | `	return rc;` |
|        5 |  1937 | `}` |
|        - |  1938 | `/*` |
|        - |  1939 | ` * Compile the 'list' language construct.` |
|        - |  1940 | ` *  According to the PHP language reference` |
|        - |  1941 | ` *  list(): Assign variables as if they were an array.` |
|        - |  1942 | ` *  list() is used to assign a list of variables in one operation.` |
|        - |  1943 | ` *  Description` |
|        - |  1944 | ` *   array list (mixed $varname [, mixed $... ] )` |
|        - |  1945 | ` *   Like array(), this is not really a function, but a language construct.` |
|        - |  1946 | ` *   list() is used to assign a list of variables in one operation.` |
|        - |  1947 | ` *  Parameters` |
|        - |  1948 | ` *   $varname: A variable.` |
|        - |  1949 | ` *  Return Values` |
|        - |  1950 | ` *   The assigned array.` |
|        - |  1951 | ` */` |
|        - |  1952 | `/* Nested list entry recorded during first pass of list body compilation */` |
|        - |  1953 | `struct NestedListEntry {` |
|        - |  1954 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|        - |  1955 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|        - |  1956 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|        - |  1957 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|        - |  1958 | `};` |
|        - |  1959 | `/*` |
|        - |  1960 | ` * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where` |
|        - |  1961 | `` * every entry has the form `keyExpr => target`. The source array is on the stack`` |
|        - |  1962 | ` * top on entry and remains there on exit, mirroring the positional LOAD_LIST` |
|        - |  1963 | ` * path so the caller's teardown is unchanged. For each entry: DUP the source,` |
|        - |  1964 | ` * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,` |
|        - |  1965 | ` * like a normal subscript read), then assign the fetched value to the target — a` |
|        - |  1966 | ` * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a` |
|        - |  1967 | ` * normal assignment (the value sits below the lvalue-load, exactly as in` |
|        - |  1968 | ` * GenStateEmitExprCode where the assignment RHS precedes the LHS load).` |
|        - |  1969 | ` */` |
|       28 |  1970 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|        2 |  1971 | `{` |
|        - |  1972 | `	SyToken *pNext;` |
|        - |  1973 | `	sxi32 rc;` |
|       66 |  1974 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|        - |  1975 | `		SyToken *pArrow,*pTarget;` |
|        - |  1976 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|       38 |  1977 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|       38 |  1978 | `		pTarget = &pArrow[1];` |
|       38 |  1979 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|        - |  1980 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|        - |  1981 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|      ! 0 |  1982 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  1983 | `				"Cannot use empty array entries in keyed array assignment");` |
|      ! 0 |  1984 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - |  1985 | `		}` |
|        - |  1986 | `		/* DUP the source array (it is on the stack top) */` |
|       38 |  1987 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|        - |  1988 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|       38 |  1989 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|       38 |  1990 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  1991 | `			return SXERR_ABORT;` |
|        - |  1992 | `		}` |
|        - |  1993 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|        - |  1994 | `		 * iP2=7 is the keyed-destructuring read context: an array source reads like` |
|        - |  1995 | ``		 * iP2=0 (missing key loads NULL silently, matching a normal `$arr[$k]` read;`` |
|        - |  1996 | `		 * PHP also emits an "Undefined array key" warning here, PHL omits it — §3.7),` |
|        - |  1997 | `		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"` |
|        - |  1998 | `		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */` |
|       38 |  1999 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);` |
|       38 |  2000 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|       34 |  2001 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|       18 |  2002 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|        - |  2003 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|        - |  2004 | `			 * Treat source[key] as the inner body's source, then drop the` |
|        - |  2005 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|        5 |  2006 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|        5 |  2007 | `			SyToken *pSavedIn = pGen->pIn;` |
|        5 |  2008 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|        5 |  2009 | `			pGen->pIn = pTarget;` |
|        5 |  2010 | `			pGen->pEnd = pNext;` |
|        5 |  2011 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|        2 |  2012 | `			             : PH7_CompileList(&(*pGen),0);` |
|        5 |  2013 | `			pGen->pIn = pSavedIn;` |
|        5 |  2014 | `			pGen->pEnd = pSavedEnd;` |
|        5 |  2015 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2016 | `				return SXERR_ABORT;` |
|        - |  2017 | `			}` |
|        5 |  2018 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        3 |  2019 | `		}else{` |
|        - |  2020 | `			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]` |
|        - |  2021 | `			 * is already on the stack as the value; compiling the target appends` |
|        - |  2022 | `			 * its lvalue-load, which we fold into a STORE just as a normal` |
|        - |  2023 | `			 * assignment does. */` |
|        - |  2024 | `			VmInstr *pInstr;` |
|       34 |  2025 | `			sxi32 iVmOp = PH7_OP_STORE;` |
|       34 |  2026 | `			sxi32 iP1 = 0, iP2 = 0;` |
|       34 |  2027 | `			void *p3 = 0;` |
|       34 |  2028 | `			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,` |
|        - |  2029 | `				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|       34 |  2030 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  2031 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - |  2032 | `			}` |
|       34 |  2033 | `			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|       34 |  2034 | `				if( pInstr->iOp == PH7_OP_MEMBER ){` |
|        3 |  2035 | `					iP2 = 1; /* member store: keep MEMBER, store value below it */` |
|       33 |  2036 | `				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        3 |  2037 | `					iVmOp = PH7_OP_STORE_IDX;` |
|        3 |  2038 | `					iP1 = pInstr->iP1;` |
|        3 |  2039 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        2 |  2040 | `				}else{` |
|       30 |  2041 | `					p3 = pInstr->p3; /* named store: $v = value */` |
|       30 |  2042 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        - |  2043 | `				}` |
|       16 |  2044 | `			}` |
|       34 |  2045 | `			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|        - |  2046 | `			/* STORE leaves the assigned value on the stack top; drop it so the` |
|        - |  2047 | `			 * source array is back on top for the next entry. */` |
|       34 |  2048 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        - |  2049 | `		}` |
|       38 |  2050 | `		pGen->pIn = &pNext[1];` |
|        2 |  2051 | `	}` |
|       30 |  2052 | `	return SXRET_OK;` |
|       16 |  2053 | `}` |
|        - |  2054 | `/*` |
|        - |  2055 | ` * Shared body for list() and short list [...] compilation.` |
|        - |  2056 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|        - |  2057 | ` * the opening delimiter and before the closing delimiter.` |
|        - |  2058 | ` */` |
|      116 |  2059 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|        5 |  2060 | `{` |
|        - |  2061 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|        - |  2062 | `	SyToken *pNext;` |
|        - |  2063 | `	SyToken *pClassifyIn;` |
|      121 |  2064 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|        - |  2065 | `	sxi32 nExpr;` |
|        - |  2066 | `	sxi32 rc;` |
|        - |  2067 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|        - |  2068 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|        - |  2069 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|        - |  2070 | `	 * list. */` |
|      121 |  2071 | `	pClassifyIn = pGen->pIn;` |
|      341 |  2072 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|      225 |  2073 | `		if( pGen->pIn >= pNext ){` |
|       13 |  2074 | `			nEmpty++;` |
|      219 |  2075 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|       38 |  2076 | `			nKeyed++;` |
|       20 |  2077 | `		}else{` |
|      177 |  2078 | `			nPositional++;` |
|        - |  2079 | `		}` |
|      225 |  2080 | `		pGen->pIn = &pNext[1];` |
|        5 |  2081 | `	}` |
|      121 |  2082 | `	pGen->pIn = pClassifyIn;` |
|      121 |  2083 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|      ! 0 |  2084 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  2085 | `			"Cannot use empty array entries in keyed array assignment");` |
|      ! 0 |  2086 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - |  2087 | `	}` |
|      121 |  2088 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|      ! 0 |  2089 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  2090 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|      ! 0 |  2091 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - |  2092 | `	}` |
|      121 |  2093 | `	if( nKeyed > 0 ){` |
|       30 |  2094 | `		return GenStateCompileKeyedListBody(pGen);` |
|        - |  2095 | `	}` |
|       93 |  2096 | `	nExpr = 0;` |
|       93 |  2097 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|      277 |  2098 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|      189 |  2099 | `		if( pGen->pIn < pNext ){` |
|        - |  2100 | `			/* Check for nested list() */` |
|      177 |  2101 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        3 |  2102 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|        - |  2103 | `				/* Record this nested list for post-processing */` |
|        3 |  2104 | `				SyToken *pListEnd = 0;` |
|        3 |  2105 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|        3 |  2106 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|        1 |  2107 | `				}` |
|        3 |  2108 | `				if( pListEnd ){` |
|        - |  2109 | `					struct NestedListEntry sEntry;` |
|        3 |  2110 | `					sEntry.nIndex = nExpr;` |
|        3 |  2111 | `					sEntry.pStart = pGen->pIn;` |
|        3 |  2112 | `					sEntry.pEnd = pListEnd + 1;` |
|        3 |  2113 | `					sEntry.isShort = 0;` |
|        3 |  2114 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|        1 |  2115 | `				}` |
|        - |  2116 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|        3 |  2117 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|      176 |  2118 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|        - |  2119 | `				/* Nested short destructuring [...] */` |
|       13 |  2120 | `				SyToken *pBracketEnd = 0;` |
|       13 |  2121 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|       13 |  2122 | `				if( pBracketEnd ){` |
|        - |  2123 | `					struct NestedListEntry sEntry;` |
|       13 |  2124 | `					sEntry.nIndex = nExpr;` |
|       13 |  2125 | `					sEntry.pStart = pGen->pIn;` |
|       13 |  2126 | `					sEntry.pEnd = pBracketEnd + 1;` |
|       13 |  2127 | `					sEntry.isShort = 1;` |
|       13 |  2128 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|        6 |  2129 | `				}` |
|        - |  2130 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       13 |  2131 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        7 |  2132 | `			}else{` |
|        - |  2133 | `				/* Compile the expression holding the variable */` |
|      163 |  2134 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|      163 |  2135 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  2136 | `					SySetRelease(&sNested);` |
|      ! 0 |  2137 | `					return SXRET_OK;` |
|        - |  2138 | `				}` |
|        - |  2139 | `			}` |
|       91 |  2140 | `		}else{` |
|        - |  2141 | `			/* Empty entry,load NULL */` |
|       13 |  2142 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|        - |  2143 | `		}` |
|      189 |  2144 | `		nExpr++;` |
|        - |  2145 | `		/* Advance the stream cursor */` |
|      189 |  2146 | `		pGen->pIn = &pNext[1];` |
|        5 |  2147 | `	}` |
|        - |  2148 | `	/* Emit the LOAD_LIST instruction */` |
|       93 |  2149 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|        - |  2150 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|        - |  2151 | `	 * For each nested entry, emit code to extract the sub-array` |
|        - |  2152 | `	 * at the corresponding index and recursively destructure it.` |
|        - |  2153 | `	 */` |
|       93 |  2154 | `	if( SySetUsed(&sNested) > 0 ){` |
|       13 |  2155 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|        - |  2156 | `		sxu32 i;` |
|       27 |  2157 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|       15 |  2158 | `			SyToken *pSavedIn = pGen->pIn;` |
|       15 |  2159 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|        - |  2160 | `			ph7_value *pIdx;` |
|        - |  2161 | `			sxu32 nConstIdx;` |
|        - |  2162 | `			/* DUP the source array (it's on stack top) */` |
|       15 |  2163 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|        - |  2164 | `			/* Push the integer index for this nested entry */` |
|       15 |  2165 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|       15 |  2166 | `			if( pIdx == 0 ){` |
|      ! 0 |  2167 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2168 | `				SySetRelease(&sNested);` |
|      ! 0 |  2169 | `				return SXERR_ABORT;` |
|        - |  2170 | `			}` |
|       15 |  2171 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|       15 |  2172 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|        - |  2173 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|        - |  2174 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|        - |  2175 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|        - |  2176 | `			 */` |
|       15 |  2177 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|        - |  2178 | `			/* Recursively compile the inner list */` |
|       15 |  2179 | `			pGen->pIn = apNested[i].pStart;` |
|       15 |  2180 | `			pGen->pEnd = apNested[i].pEnd;` |
|       15 |  2181 | `			if( apNested[i].isShort ){` |
|       13 |  2182 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|        7 |  2183 | `			}else{` |
|        3 |  2184 | `				rc = PH7_CompileList(&(*pGen),0);` |
|        - |  2185 | `			}` |
|       15 |  2186 | `			pGen->pIn = pSavedIn;` |
|       15 |  2187 | `			pGen->pEnd = pSavedEnd;` |
|       15 |  2188 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2189 | `				SySetRelease(&sNested);` |
|      ! 0 |  2190 | `				return SXERR_ABORT;` |
|        - |  2191 | `			}` |
|        - |  2192 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|       15 |  2193 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        8 |  2194 | `		}` |
|        6 |  2195 | `	}` |
|       93 |  2196 | `	SySetRelease(&sNested);` |
|        - |  2197 | `	/* Node successfully compiled */` |
|       93 |  2198 | `	return SXRET_OK;` |
|       63 |  2199 | `}` |
|       38 |  2200 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  2201 | `{` |
|        - |  2202 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|       43 |  2203 | `	pGen->pIn += 2;` |
|       43 |  2204 | `	pGen->pEnd--;` |
|       19 |  2205 | `	SXUNUSED(iCompileFlag);` |
|       43 |  2206 | `	return GenStateCompileListBody(pGen);` |
|        5 |  2207 | `}` |
|       78 |  2208 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        4 |  2209 | `{` |
|        - |  2210 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|       82 |  2211 | `	pGen->pIn++;` |
|       82 |  2212 | `	pGen->pEnd--;` |
|       39 |  2213 | `	SXUNUSED(iCompileFlag);` |
|       82 |  2214 | `	return GenStateCompileListBody(pGen);` |
|        4 |  2215 | `}` |
|        - |  2216 | `/* Forward declarations */` |
|        - |  2217 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|        - |  2218 | `static int GenStateIsReservedConstant(SyString *pName);` |
|        - |  2219 | `static int GenStateIsReadonly(SyToken *pTok);` |
|        - |  2220 | `static sxi32 GenStateValidateMemberType(ph7_gen_state *pGen,ph7_class *pClass,const SyString *pMemberName,` |
|        - |  2221 | `	sxu32 nType,const SyString *pTypeClass,const SyString *pTypeText,SySet *pUnionAlts,const char *zErrFmt,sxu32 nLine);` |
|        - |  2222 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|        - |  2223 | `/*` |
|        - |  2224 | ` * Compile an annoynmous function or a closure.` |
|        - |  2225 | ` * According to the PHP language reference` |
|        - |  2226 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|        - |  2227 | ` *  which have no specified name. They are most useful as the value of callback` |
|        - |  2228 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|        - |  2229 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|        - |  2230 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|        - |  2231 | ` *  Example Anonymous function variable assignment example` |
|        - |  2232 | ` * <?php` |
|        - |  2233 | ` * $greet = function($name)` |
|        - |  2234 | ` * {` |
|        - |  2235 | ` *    printf("Hello %s\r\n", $name);` |
|        - |  2236 | ` * };` |
|        - |  2237 | ` * $greet('World');` |
|        - |  2238 | ` * $greet('PHP');` |
|        - |  2239 | ` * ?>` |
|        - |  2240 | ` * Note that the implementation of annoynmous function and closure under` |
|        - |  2241 | ` * PH7 is completely different from the one used by the zend engine.` |
|        - |  2242 | ` */` |
|      332 |  2243 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  2244 | `{` |
|      337 |  2245 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|        - |  2246 | `	char zName[512];         /* Unique lambda name */` |
|        - |  2247 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|        - |  2248 | `							  * one thread is allowed to compile the script.` |
|        - |  2249 | `						      */` |
|        - |  2250 | `	SyString sName;` |
|        - |  2251 | `	sxu32 nKwLine;` |
|        - |  2252 | `	sxu32 nLen;` |
|        - |  2253 | `	sxi32 rc;` |
|      166 |  2254 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  2255 |  |
|      337 |  2256 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|      337 |  2257 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|      337 |  2258 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      ! 0 |  2259 | `		pGen->pIn++;` |
|      ! 0 |  2260 | `	}` |
|        - |  2261 | `	/* Generate a unique name */` |
|      337 |  2262 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|        - |  2263 | `	/* Make sure the generated name is unique */` |
|      337 |  2264 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 |  2265 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      ! 0 |  2266 | `	}` |
|      337 |  2267 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|        - |  2268 | `	/* Compile the lambda body */` |
|      337 |  2269 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|      337 |  2270 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2271 | `		return SXERR_ABORT;` |
|        - |  2272 | `	}` |
|      337 |  2273 | `	if( pAnnonFunc ){` |
|      337 |  2274 | `		pAnnonFunc->nLine = nKwLine;` |
|      166 |  2275 | `	}` |
|        - |  2276 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|        - |  2277 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|        - |  2278 | `	 * the handler wraps either in a Closure instance. */` |
|      337 |  2279 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|        - |  2280 | `	/* Node successfully compiled */` |
|      337 |  2281 | `	return SXRET_OK;` |
|      171 |  2282 | `}` |
|        - |  2283 | `/*` |
|        - |  2284 | ` * Add a free variable to the arrow function's closure environment, unless` |
|        - |  2285 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|        - |  2286 | ` * enclosing arrow level, or has already been captured.` |
|        - |  2287 | ` */` |
|      186 |  2288 | `static sxi32 GenStateArrowAddCapture(` |
|        - |  2289 | `	ph7_gen_state *pGen,` |
|        - |  2290 | `	ph7_vm_func *pFunc,` |
|        - |  2291 | `	const char *zName,` |
|        - |  2292 | `	sxu32 nByte,` |
|        - |  2293 | `	SyString *aShadow,` |
|        - |  2294 | `	sxu32 nShadow)` |
|        3 |  2295 | `{` |
|        - |  2296 | `	ph7_vm_func_closure_env sEnv;` |
|        - |  2297 | `	ph7_vm_func_closure_env *aEnv;` |
|        - |  2298 | `	sxu32 n, nEnv;` |
|        - |  2299 | `	char *zDup;` |
|      189 |  2300 | `	if( nByte == 0 ){` |
|      ! 0 |  2301 | `		return SXRET_OK;` |
|        - |  2302 | `	}` |
|      186 |  2303 | `	if( nByte == sizeof("this")-1` |
|      102 |  2304 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|        3 |  2305 | `		return SXRET_OK;` |
|        - |  2306 | `	}` |
|      235 |  2307 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|      174 |  2308 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|      168 |  2309 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|      129 |  2310 | `			return SXRET_OK;` |
|        - |  2311 | `		}` |
|       26 |  2312 | `	}` |
|       59 |  2313 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       59 |  2314 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|       87 |  2315 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|       28 |  2316 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|       27 |  2317 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|      ! 0 |  2318 | `			return SXRET_OK;` |
|        - |  2319 | `		}` |
|       15 |  2320 | `	}` |
|       59 |  2321 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|       59 |  2322 | `	if( zDup == 0 ){` |
|      ! 0 |  2323 | `		return SXERR_ABORT;` |
|        - |  2324 | `	}` |
|       59 |  2325 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       59 |  2326 | `	sEnv.iFlags = 0;` |
|       59 |  2327 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       59 |  2328 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|       59 |  2329 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       59 |  2330 | `	return SXRET_OK;` |
|       96 |  2331 | `}` |
|        - |  2332 | `/*` |
|        - |  2333 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|        - |  2334 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|        - |  2335 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|        - |  2336 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|        - |  2337 | ` */` |
|       46 |  2338 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|        - |  2339 | `	ph7_gen_state *pGen,` |
|        - |  2340 | `	ph7_vm_func *pFunc,` |
|        - |  2341 | `	const char *zIn,` |
|        - |  2342 | `	const char *zEnd,` |
|        - |  2343 | `	SyString *aShadow,` |
|        - |  2344 | `	sxu32 nShadow)` |
|        2 |  2345 | `{` |
|        - |  2346 | `	sxi32 rc;` |
|      342 |  2347 | `	while( zIn < zEnd ){` |
|      296 |  2348 | `		if( zIn[0] == '\\' ){` |
|        5 |  2349 | `			zIn++;` |
|        5 |  2350 | `			if( zIn < zEnd ){` |
|        5 |  2351 | `				zIn++;` |
|        2 |  2352 | `			}` |
|        5 |  2353 | `			continue;` |
|        - |  2354 | `		}` |
|      290 |  2355 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|       22 |  2356 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|       20 |  2357 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|        - |  2358 | `			const char *zName;` |
|       22 |  2359 | `			zIn++; /* skip '$' */` |
|       22 |  2360 | `			zName = zIn;` |
|       74 |  2361 | `			while( zIn < zEnd ){` |
|       70 |  2362 | `				unsigned char c = (unsigned char)zIn[0];` |
|       70 |  2363 | `				if( c >= 0xc0 ){` |
|      ! 0 |  2364 | `					zIn++;` |
|      ! 0 |  2365 | `					while( zIn < zEnd` |
|      ! 0 |  2366 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  2367 | `						zIn++;` |
|      ! 0 |  2368 | `					}` |
|      ! 0 |  2369 | `					continue;` |
|        - |  2370 | `				}` |
|       70 |  2371 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       18 |  2372 | `					break;` |
|        - |  2373 | `				}` |
|       54 |  2374 | `				zIn++;` |
|        2 |  2375 | `			}` |
|       22 |  2376 | `			if( zIn > zName ){` |
|       32 |  2377 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|       20 |  2378 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|       22 |  2379 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  2380 | `					return SXERR_ABORT;` |
|        - |  2381 | `				}` |
|       10 |  2382 | `			}` |
|       22 |  2383 | `			continue;` |
|        - |  2384 | `		}` |
|      272 |  2385 | `		zIn++;` |
|        2 |  2386 | `	}` |
|       48 |  2387 | `	return SXRET_OK;` |
|       25 |  2388 | `}` |
|        - |  2389 | `/*` |
|        - |  2390 | ` * Scan the body token range of an arrow function for free-variable` |
|        - |  2391 | ` * references and record them in pFunc's closure environment. Handles:` |
|        - |  2392 | ` *   - plain $<id> pairs` |
|        - |  2393 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|        - |  2394 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|        - |  2395 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|        - |  2396 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|        - |  2397 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|        - |  2398 | ` *     are never mistakenly captured.` |
|        - |  2399 | ` */` |
|      250 |  2400 | `static sxi32 GenStateArrowCaptureScan(` |
|        - |  2401 | `	ph7_gen_state *pGen,` |
|        - |  2402 | `	ph7_vm_func *pFunc,` |
|        - |  2403 | `	SyToken *pStart,` |
|        - |  2404 | `	SyToken *pEnd,` |
|        - |  2405 | `	SyString *aShadow,` |
|        - |  2406 | `	sxu32 nShadow)` |
|        4 |  2407 | `{` |
|      254 |  2408 | `	SyToken *pScan = pStart;` |
|        - |  2409 | `	sxi32 rc;` |
|     1274 |  2410 | `	while( pScan < pEnd ){` |
|     1024 |  2411 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|       71 |  2412 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|       23 |  2413 | `				pScan->sData.zString,` |
|       46 |  2414 | `				pScan->sData.zString + pScan->sData.nByte,` |
|       23 |  2415 | `				aShadow,nShadow);` |
|       48 |  2416 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2417 | `				return SXERR_ABORT;` |
|        - |  2418 | `			}` |
|       48 |  2419 | `			pScan++;` |
|       48 |  2420 | `			continue;` |
|        - |  2421 | `		}` |
|      978 |  2422 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|       24 |  2423 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|       24 |  2424 | `			SyToken *pFnKw = pScan;` |
|       22 |  2425 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|      ! 0 |  2426 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|        2 |  2427 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|      ! 0 |  2428 | `				pFnKw = &pScan[1];` |
|      ! 0 |  2429 | `				nKw = PH7_TKWRD_FN;` |
|      ! 0 |  2430 | `			}` |
|       24 |  2431 | `			if( nKw == PH7_TKWRD_FN ){` |
|        - |  2432 | `				SyToken *pInnerSigStart;` |
|        - |  2433 | `				SyToken *pInnerSigEnd;` |
|        - |  2434 | `				SyToken *pInnerBodyEnd;` |
|        - |  2435 | `				SyString *aInnerShadow;` |
|        - |  2436 | `				sxu32 nInnerShadow;` |
|        - |  2437 | `				sxu32 nInnerParamMax;` |
|        - |  2438 | `				SyToken *p;` |
|        - |  2439 | `				int iNestInner;` |
|       19 |  2440 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|       19 |  2441 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  2442 | `					pScan++;` |
|      ! 0 |  2443 | `				}` |
|       19 |  2444 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 |  2445 | `					pScan++;` |
|      ! 0 |  2446 | `					continue;` |
|        - |  2447 | `				}` |
|       19 |  2448 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|       19 |  2449 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|        - |  2450 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|       19 |  2451 | `				if( pInnerSigEnd >= pEnd ){` |
|      ! 0 |  2452 | `					pScan = pEnd;` |
|      ! 0 |  2453 | `					continue;` |
|        - |  2454 | `				}` |
|        - |  2455 | `				/* Build an augmented shadow list: inherited + inner params */` |
|       19 |  2456 | `				nInnerParamMax = 0;` |
|       57 |  2457 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|       39 |  2458 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|       13 |  2459 | `						nInnerParamMax++;` |
|        6 |  2460 | `					}` |
|       20 |  2461 | `				}` |
|       19 |  2462 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|       18 |  2463 | `					&pGen->pVm->sAllocator,` |
|       18 |  2464 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|       19 |  2465 | `				if( aInnerShadow == 0 ){` |
|      ! 0 |  2466 | `					return SXERR_ABORT;` |
|        - |  2467 | `				}` |
|       19 |  2468 | `				nInnerShadow = 0;` |
|       25 |  2469 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|        7 |  2470 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|        4 |  2471 | `				}` |
|       57 |  2472 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|       39 |  2473 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|       27 |  2474 | `						continue;` |
|        - |  2475 | `					}` |
|       13 |  2476 | `					if( &p[1] >= pInnerSigEnd ){` |
|      ! 0 |  2477 | `						break;` |
|        - |  2478 | `					}` |
|       13 |  2479 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  2480 | `						continue;` |
|        - |  2481 | `					}` |
|       13 |  2482 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|        7 |  2483 | `				}` |
|       19 |  2484 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|       19 |  2485 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|      ! 0 |  2486 | `					pScan++;` |
|      ! 0 |  2487 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|      ! 0 |  2488 | `						&& pScan->sData.nByte == 1` |
|      ! 0 |  2489 | `						&& pScan->sData.zString[0] == '?' ){` |
|      ! 0 |  2490 | `						pScan++;` |
|      ! 0 |  2491 | `					}` |
|      ! 0 |  2492 | `					if( pScan < pEnd` |
|      ! 0 |  2493 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|      ! 0 |  2494 | `						pScan++;` |
|      ! 0 |  2495 | `					}` |
|      ! 0 |  2496 | `				}` |
|       19 |  2497 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|       19 |  2498 | `					pScan++; /* past '=>' */` |
|        9 |  2499 | `				}` |
|       19 |  2500 | `				pInnerBodyEnd = pScan;` |
|       19 |  2501 | `				iNestInner = 0;` |
|      131 |  2502 | `				while( pInnerBodyEnd < pEnd ){` |
|      113 |  2503 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|        - |  2504 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|        - |  2505 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|      ! 0 |  2506 | `						break;` |
|        - |  2507 | `					}` |
|      113 |  2508 | `					if( pInnerBodyEnd->nType &` |
|        - |  2509 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        3 |  2510 | `						iNestInner++;` |
|      112 |  2511 | `					}else if( pInnerBodyEnd->nType &` |
|        - |  2512 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        3 |  2513 | `						iNestInner--;` |
|        1 |  2514 | `					}` |
|      113 |  2515 | `					pInnerBodyEnd++;` |
|        1 |  2516 | `				}` |
|        - |  2517 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|        - |  2518 | `				 * the outer's body: a default value is evaluated at call time` |
|        - |  2519 | `				 * in the outer frame, so any free variable it references is` |
|        - |  2520 | `				 * an outer capture. We must NOT scan the parameter-name` |
|        - |  2521 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|        - |  2522 | `				 * or those names leak into the outer's closure environment.` |
|        - |  2523 | `				 *` |
|        - |  2524 | `				 * Walk the signature argument-by-argument, splitting on` |
|        - |  2525 | `				 * top-level commas, and for each argument scan only the token` |
|        - |  2526 | `				 * range after the '=' sign. */` |
|        - |  2527 | `				{` |
|       19 |  2528 | `					SyToken *pArgStart = pInnerSigStart;` |
|       31 |  2529 | `					while( pArgStart < pInnerSigEnd ){` |
|       13 |  2530 | `						SyToken *pArgEnd = pArgStart;` |
|       13 |  2531 | `						SyToken *pEq = 0;` |
|       13 |  2532 | `						int iNestArg = 0;` |
|       49 |  2533 | `						while( pArgEnd < pInnerSigEnd ){` |
|       38 |  2534 | `							if( iNestArg == 0` |
|       39 |  2535 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|        3 |  2536 | `								break;` |
|        - |  2537 | `							}` |
|       37 |  2538 | `							if( pArgEnd->nType &` |
|        - |  2539 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      ! 0 |  2540 | `								iNestArg++;` |
|       37 |  2541 | `							}else if( pArgEnd->nType &` |
|        - |  2542 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      ! 0 |  2543 | `								iNestArg--;` |
|      ! 0 |  2544 | `							}` |
|       36 |  2545 | `							if( pEq == 0 && iNestArg == 0` |
|       31 |  2546 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|        7 |  2547 | `								pEq = pArgEnd;` |
|        3 |  2548 | `							}` |
|       37 |  2549 | `							pArgEnd++;` |
|        1 |  2550 | `						}` |
|       13 |  2551 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|       10 |  2552 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|        3 |  2553 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|        7 |  2554 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 |  2555 | `								return SXERR_ABORT;` |
|        - |  2556 | `							}` |
|        3 |  2557 | `						}` |
|       13 |  2558 | `						pArgStart = pArgEnd;` |
|       12 |  2559 | `						if( pArgStart < pInnerSigEnd` |
|        8 |  2560 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|        3 |  2561 | `							pArgStart++;` |
|        1 |  2562 | `						}` |
|        1 |  2563 | `					}` |
|        - |  2564 | `				}` |
|       28 |  2565 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|        9 |  2566 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|       19 |  2567 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  2568 | `					return SXERR_ABORT;` |
|        - |  2569 | `				}` |
|       19 |  2570 | `				pScan = pInnerBodyEnd;` |
|       19 |  2571 | `				continue;` |
|        - |  2572 | `			}` |
|        2 |  2573 | `		}` |
|      960 |  2574 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|      794 |  2575 | `			pScan++;` |
|      794 |  2576 | `			continue;` |
|        - |  2577 | `		}` |
|        - |  2578 | `		{` |
|        - |  2579 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|      169 |  2580 | `			SyToken *pDollar = pScan;` |
|      249 |  2581 | `			while( &pDollar[1] < pEnd` |
|      169 |  2582 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|      ! 0 |  2583 | `				pDollar++;` |
|      ! 0 |  2584 | `			}` |
|      169 |  2585 | `			if( &pDollar[1] >= pEnd ){` |
|      ! 0 |  2586 | `				break;` |
|        - |  2587 | `			}` |
|      169 |  2588 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  2589 | `				pScan = pDollar + 1;` |
|      ! 0 |  2590 | `				continue;` |
|        - |  2591 | `			}` |
|      252 |  2592 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|      166 |  2593 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|       83 |  2594 | `				aShadow,nShadow);` |
|      169 |  2595 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2596 | `				return SXERR_ABORT;` |
|        - |  2597 | `			}` |
|      169 |  2598 | `			pScan = pDollar + 2;` |
|        - |  2599 | `		}` |
|        3 |  2600 | `	}` |
|      254 |  2601 | `	return SXRET_OK;` |
|      129 |  2602 | `}` |
|        - |  2603 | `/*` |
|        - |  2604 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|        - |  2605 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|        - |  2606 | ` * variables by value. The body is a single expression that acts as an` |
|        - |  2607 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|        - |  2608 | ` * $this is also made available.` |
|        - |  2609 | ` */` |
|      232 |  2610 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  2611 | `{` |
|        - |  2612 | `	ph7_vm_func *pFunc;` |
|        - |  2613 | `	ph7_vm_func_closure_env sEnv;` |
|        - |  2614 | `	GenBlock *pBlock;` |
|        - |  2615 | `	SySet *pInstrContainer;` |
|        - |  2616 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|        - |  2617 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|        - |  2618 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|        - |  2619 | `	SyToken *pSavedEnd;` |
|        - |  2620 | `	ph7_vm_func_arg *aArgs;` |
|        - |  2621 | `	char zName[512];` |
|        - |  2622 | `	static int iCnt = 1;` |
|        - |  2623 | `	char *zDup;` |
|        - |  2624 | `	sxu32 nLen;` |
|        - |  2625 | `	sxu32 nLine;` |
|      237 |  2626 | `	sxi32 iFlags = 0;` |
|      237 |  2627 | `	int bStatic = 0;` |
|        - |  2628 | `	sxi32 rc;` |
|        - |  2629 | `	sxu32 n;` |
|      116 |  2630 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  2631 |  |
|      237 |  2632 | `	nLine = pGen->pIn->nLine;` |
|        - |  2633 | `	/* Optional 'static' prefix */` |
|      232 |  2634 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      237 |  2635 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        3 |  2636 | `		bStatic = 1;` |
|        3 |  2637 | `		pGen->pIn++;` |
|        1 |  2638 | `	}` |
|        - |  2639 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|      232 |  2640 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      237 |  2641 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|      ! 0 |  2642 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2643 | `			"Arrow function: expected 'fn' keyword");` |
|      ! 0 |  2644 | `		return SXERR_SYNTAX;` |
|        - |  2645 | `	}` |
|      237 |  2646 | `	pGen->pIn++; /* Jump 'fn' */` |
|        - |  2647 | `	/* Optional '&' — return by reference */` |
|      237 |  2648 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  2649 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|      ! 0 |  2650 | `		pGen->pIn++;` |
|      ! 0 |  2651 | `	}` |
|        - |  2652 | `	/* Expect '(' */` |
|      237 |  2653 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        3 |  2654 | `		if( pGen->pIn < pGen->pEnd ){` |
|        4 |  2655 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - |  2656 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|        2 |  2657 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        2 |  2658 | `		}else{` |
|      ! 0 |  2659 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  2660 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|        - |  2661 | `		}` |
|        3 |  2662 | `		return SXERR_SYNTAX;` |
|        - |  2663 | `	}` |
|      235 |  2664 | `	pGen->pIn++; /* Jump '(' */` |
|        - |  2665 | `	/* Delimit the parameter list */` |
|      235 |  2666 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|      235 |  2667 | `	if( pSigEnd >= pGen->pEnd ){` |
|        3 |  2668 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  2669 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|        3 |  2670 | `		return SXERR_SYNTAX;` |
|        - |  2671 | `	}` |
|        - |  2672 | `	/* Allocate the function state */` |
|      233 |  2673 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|      233 |  2674 | `	if( pFunc == 0 ){` |
|      ! 0 |  2675 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2676 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2677 | `		return SXERR_ABORT;` |
|        - |  2678 | `	}` |
|        - |  2679 | `	/* Generate a unique lambda name */` |
|      233 |  2680 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      289 |  2681 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|       58 |  2682 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|        2 |  2683 | `	}` |
|      233 |  2684 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|      233 |  2685 | `	if( zDup == 0 ){` |
|      ! 0 |  2686 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2687 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2688 | `		return SXERR_ABORT;` |
|        - |  2689 | `	}` |
|      233 |  2690 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|        - |  2691 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|      233 |  2692 | `	pFunc->nLine = nLine;` |
|        - |  2693 | `	/* Collect function arguments */` |
|      233 |  2694 | `	if( pGen->pIn < pSigEnd ){` |
|      106 |  2695 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|      106 |  2696 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  2697 | `			return SXERR_ABORT;` |
|        - |  2698 | `		}` |
|       51 |  2699 | `	}` |
|        - |  2700 | `	/* Point past ')' and parse optional return type */` |
|      233 |  2701 | `	pGen->pIn = &pSigEnd[1];` |
|      233 |  2702 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|      233 |  2703 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2704 | `		return SXERR_ABORT;` |
|      233 |  2705 | `	}else if( rc == SXERR_SYNTAX ){` |
|      ! 0 |  2706 | `		return SXERR_SYNTAX;` |
|        - |  2707 | `	}` |
|        - |  2708 | `	/* Expect '=>' */` |
|      233 |  2709 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|        3 |  2710 | `		if( pGen->pIn < pGen->pEnd ){` |
|        4 |  2711 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - |  2712 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|        2 |  2713 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        2 |  2714 | `		}else{` |
|      ! 0 |  2715 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  2716 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|        - |  2717 | `		}` |
|        3 |  2718 | `		return SXERR_SYNTAX;` |
|        - |  2719 | `	}` |
|      230 |  2720 | `	pGen->pIn++; /* Jump '=>' */` |
|      230 |  2721 | `	pBodyStart = pGen->pIn;` |
|      230 |  2722 | `	pBodyEnd = pGen->pEnd;` |
|        - |  2723 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|        - |  2724 | `	 * recursively collect free-variable references from the body. The scan` |
|        - |  2725 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|        - |  2726 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|      230 |  2727 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|        - |  2728 | `	{` |
|      230 |  2729 | `		SyString *aShadow = 0;` |
|      230 |  2730 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|      230 |  2731 | `		if( nShadow > 0 ){` |
|      103 |  2732 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      100 |  2733 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|      103 |  2734 | `			if( aShadow == 0 ){` |
|      ! 0 |  2735 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2736 | `					"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2737 | `				return SXERR_ABORT;` |
|        - |  2738 | `			}` |
|      229 |  2739 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|      129 |  2740 | `				aShadow[n] = aArgs[n].sName;` |
|       66 |  2741 | `			}` |
|       50 |  2742 | `		}` |
|      343 |  2743 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      113 |  2744 | `			aShadow,nShadow);` |
|      230 |  2745 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  2746 | `			return SXERR_ABORT;` |
|        - |  2747 | `		}` |
|        - |  2748 | `	}` |
|        - |  2749 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|        - |  2750 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|        - |  2751 | `	 * captured value is silently dropped when the enclosing scope has no` |
|        - |  2752 | `	 * $this. */` |
|      230 |  2753 | `	if( !bStatic ){` |
|        - |  2754 | `		char *zThisDup;` |
|      228 |  2755 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|      228 |  2756 | `		if( zThisDup == 0 ){` |
|      ! 0 |  2757 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2758 | `				"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2759 | `			return SXERR_ABORT;` |
|        - |  2760 | `		}` |
|      228 |  2761 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      228 |  2762 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|      228 |  2763 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      228 |  2764 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|      228 |  2765 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      112 |  2766 | `	}` |
|        - |  2767 | `	/* Arrow functions are always closures */` |
|      230 |  2768 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|        - |  2769 | `	/* Compile the body expression as an implicit return */` |
|      343 |  2770 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      113 |  2771 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|      230 |  2772 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  2773 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2774 | `			"PH7 engine is running out-of-memory");` |
|      ! 0 |  2775 | `		return SXERR_ABORT;` |
|        - |  2776 | `	}` |
|      230 |  2777 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      230 |  2778 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|      230 |  2779 | `	pSavedEnd = pGen->pEnd;` |
|      230 |  2780 | `	pGen->pIn = pBodyStart;` |
|      230 |  2781 | `	pGen->pEnd = pBodyEnd;` |
|      230 |  2782 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      230 |  2783 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2784 | `		return SXERR_ABORT;` |
|        - |  2785 | `	}` |
|        - |  2786 | `	/* The cursor stopped just past the body expression */` |
|      230 |  2787 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|        - |  2788 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|        - |  2789 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|        - |  2790 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|        - |  2791 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|      230 |  2792 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      230 |  2793 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      230 |  2794 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      230 |  2795 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      230 |  2796 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - |  2797 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|      230 |  2798 | `	pGen->pIn = pBodyEnd;` |
|      230 |  2799 | `	pGen->pEnd = pSavedEnd;` |
|        - |  2800 | `	/* Emit the load-closure instruction */` |
|      230 |  2801 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|      230 |  2802 | `	return SXRET_OK;` |
|      121 |  2803 | `}` |
|        - |  2804 | `/*` |
|        - |  2805 | ` * Compile a single arm's expression range into a freshly-allocated` |
|        - |  2806 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|        - |  2807 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|        - |  2808 | ` * expression's value.` |
|        - |  2809 | ` */` |
|      346 |  2810 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|        - |  2811 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|        3 |  2812 | `{` |
|        - |  2813 | `	SySet *pInstrContainer;` |
|        - |  2814 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |  2815 | `	GenBlock *pArmBlock;` |
|        - |  2816 | `	sxi32 rc;` |
|      349 |  2817 | `	pTmpIn  = pGen->pIn;` |
|      349 |  2818 | `	pTmpEnd = pGen->pEnd;` |
|      349 |  2819 | `	pGen->pIn  = pStart;` |
|      349 |  2820 | `	pGen->pEnd = pStop;` |
|      349 |  2821 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      349 |  2822 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|        - |  2823 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|        - |  2824 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|        - |  2825 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|        - |  2826 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|        - |  2827 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|      522 |  2828 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      173 |  2829 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|      349 |  2830 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  2831 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  2832 | `		pGen->pIn  = pTmpIn;` |
|      ! 0 |  2833 | `		pGen->pEnd = pTmpEnd;` |
|      ! 0 |  2834 | `		return SXERR_ABORT;` |
|        - |  2835 | `	}` |
|      349 |  2836 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      349 |  2837 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      349 |  2838 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      349 |  2839 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      349 |  2840 | `	GenStateLeaveBlock(&(*pGen),0);` |
|      349 |  2841 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      349 |  2842 | `	pGen->pIn  = pTmpIn;` |
|      349 |  2843 | `	pGen->pEnd = pTmpEnd;` |
|      349 |  2844 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2845 | `		return SXERR_ABORT;` |
|        - |  2846 | `	}` |
|      349 |  2847 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 |  2848 | `		return SXERR_EMPTY;` |
|        - |  2849 | `	}` |
|      349 |  2850 | `	return SXRET_OK;` |
|      176 |  2851 | `}` |
|        - |  2852 | `/*` |
|        - |  2853 | ` * Compile a PHP 8.0 match expression:` |
|        - |  2854 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|        - |  2855 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|        - |  2856 | ` * Strict comparison (===) is used between the subject and each condition.` |
|        - |  2857 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|        - |  2858 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|        - |  2859 | ` */` |
|        - |  2860 | `/*` |
|        - |  2861 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|        - |  2862 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|        - |  2863 | ` * caller can bail out of the current expression.` |
|        - |  2864 | ` */` |
|        2 |  2865 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|        1 |  2866 | `{` |
|        - |  2867 | `	va_list ap;` |
|        - |  2868 | `	sxi32 rc;` |
|        - |  2869 | `	SyBlob sMsg;` |
|        3 |  2870 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|        3 |  2871 | `	va_start(ap,zFmt);` |
|        3 |  2872 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|        3 |  2873 | `	va_end(ap);` |
|        3 |  2874 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|        3 |  2875 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|        3 |  2876 | `	SyBlobRelease(&sMsg);` |
|        3 |  2877 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2878 | `		return SXERR_ABORT;` |
|        - |  2879 | `	}` |
|        3 |  2880 | `	return SXERR_SYNTAX;` |
|        2 |  2881 | `}` |
|        - |  2882 | `/*` |
|        - |  2883 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|        - |  2884 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|        - |  2885 | ` * Returns the stop token pointer (or pEnd if none found).` |
|        - |  2886 | ` */` |
|      348 |  2887 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|        4 |  2888 | `{` |
|      352 |  2889 | `	SyToken *pCur = pStart;` |
|      352 |  2890 | `	int iNest = 0;` |
|      814 |  2891 | `	while( pCur < pEnd ){` |
|      780 |  2892 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       13 |  2893 | `			iNest++;` |
|      774 |  2894 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       13 |  2895 | `			iNest--;` |
|      762 |  2896 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|      317 |  2897 | `			return pCur;` |
|        - |  2898 | `		}` |
|      466 |  2899 | `		pCur++;` |
|        4 |  2900 | `	}` |
|       37 |  2901 | `	return pEnd;` |
|      178 |  2902 | `}` |
|       70 |  2903 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  2904 | `{` |
|        - |  2905 | `	ph7_match *pMatch;` |
|        - |  2906 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|       75 |  2907 | `	int bHasDefault = 0;` |
|        - |  2908 | `	sxu32 nLine;` |
|        - |  2909 | `	sxi32 rc;` |
|       35 |  2910 | `	SXUNUSED(iCompileFlag);` |
|       75 |  2911 | `	nLine = pGen->pIn->nLine;` |
|       75 |  2912 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|        - |  2913 | `	/* Expect '(' */` |
|       75 |  2914 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 |  2915 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  2916 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|      ! 0 |  2917 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|        - |  2918 | `	}` |
|       75 |  2919 | `	pGen->pIn++; /* Jump '(' */` |
|       75 |  2920 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|       75 |  2921 | `	if( pSubjEnd >= pGen->pEnd ){` |
|      ! 0 |  2922 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  2923 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|        - |  2924 | `	}` |
|       75 |  2925 | `	if( pGen->pIn >= pSubjEnd ){` |
|      ! 0 |  2926 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  2927 | `			"syntax error, unexpected \")\", expecting match subject");` |
|        - |  2928 | `	}` |
|        - |  2929 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|       75 |  2930 | `	pSavedEnd = pGen->pEnd;` |
|       75 |  2931 | `	pGen->pEnd = pSubjEnd;` |
|       75 |  2932 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       75 |  2933 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2934 | `		return SXERR_ABORT;` |
|        - |  2935 | `	}` |
|       75 |  2936 | `	pGen->pEnd = pSavedEnd;` |
|       75 |  2937 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|        - |  2938 | `	/* Expect '{' */` |
|       75 |  2939 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|      ! 0 |  2940 | `		return GenStateMatchError(pGen,` |
|      ! 0 |  2941 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|        - |  2942 | `			"syntax error, expecting \"{\" after match subject");` |
|        - |  2943 | `	}` |
|       75 |  2944 | `	pGen->pIn++; /* Jump '{' */` |
|       75 |  2945 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|       75 |  2946 | `	if( pBodyEnd >= pGen->pEnd ){` |
|      ! 0 |  2947 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  2948 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|        - |  2949 | `	}` |
|        - |  2950 | `	/* Allocate ph7_match container */` |
|       75 |  2951 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|       75 |  2952 | `	if( pMatch == 0 ){` |
|      ! 0 |  2953 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2954 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2955 | `		return SXERR_ABORT;` |
|        - |  2956 | `	}` |
|       75 |  2957 | `	SyZero(pMatch,sizeof(ph7_match));` |
|       75 |  2958 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|        - |  2959 | `	/* Iterate arms */` |
|      253 |  2960 | `	while( pGen->pIn < pBodyEnd ){` |
|        - |  2961 | `		ph7_match_arm sArm;` |
|        - |  2962 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|      186 |  2963 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|      186 |  2964 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|      186 |  2965 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|      186 |  2966 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - |  2967 | `		/* 'default' arm? */` |
|      182 |  2968 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      105 |  2969 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|       22 |  2970 | `			if( bHasDefault ){` |
|        3 |  2971 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|        - |  2972 | `					"Match expressions may only contain one default arm");` |
|        4 |  2973 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  2974 | `			}` |
|       20 |  2975 | `			sArm.bDefault = 1;` |
|       20 |  2976 | `			bHasDefault = 1;` |
|       20 |  2977 | `			pGen->pIn++;` |
|       20 |  2978 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|      ! 0 |  2979 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  2980 | `					"syntax error, expecting \"=>\" after 'default'");` |
|        - |  2981 | `			}` |
|       20 |  2982 | `			pGen->pIn++; /* Jump '=>' */` |
|       11 |  2983 | `		}else{` |
|        - |  2984 | `			/* Condition list: cond (',' cond)* '=>' */` |
|      166 |  2985 | `			pCondStart = pGen->pIn;` |
|      166 |  2986 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|        - |  2987 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|      174 |  2988 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|        - |  2989 | `				SySet sCondBc;` |
|        9 |  2990 | `				if( pCondStart >= pArrow ){` |
|      ! 0 |  2991 | `					return GenStateMatchError(pGen,nArmLine,` |
|        - |  2992 | `						"syntax error, empty match condition expression");` |
|        - |  2993 | `				}` |
|        9 |  2994 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        9 |  2995 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|        9 |  2996 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  2997 | `					return SXERR_ABORT;` |
|        - |  2998 | `				}` |
|        9 |  2999 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|        9 |  3000 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|        9 |  3001 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|        - |  3002 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|        1 |  3003 | `			}` |
|      166 |  3004 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|        3 |  3005 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  3006 | `					"syntax error, expecting \"=>\" in match arm");` |
|        - |  3007 | `			}` |
|      163 |  3008 | `			if( pCondStart >= pArrow ){` |
|      ! 0 |  3009 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  3010 | `					"syntax error, empty match condition expression");` |
|        - |  3011 | `			}` |
|        - |  3012 | `			{` |
|        - |  3013 | `				SySet sCondBc;` |
|      163 |  3014 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      163 |  3015 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|      163 |  3016 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  3017 | `					return SXERR_ABORT;` |
|        - |  3018 | `				}` |
|      163 |  3019 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|        - |  3020 | `			}` |
|      163 |  3021 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|        - |  3022 | `		}` |
|        - |  3023 | `		/* Compile result expression: up to top-level ',' or body end */` |
|      181 |  3024 | `		pResStart = pGen->pIn;` |
|      181 |  3025 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|      181 |  3026 | `		if( pResStart >= pResEnd ){` |
|      ! 0 |  3027 | `			return GenStateMatchError(pGen,nArmLine,` |
|        - |  3028 | `				"syntax error, expected expression after \"=>\"");` |
|        - |  3029 | `		}` |
|      181 |  3030 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|      181 |  3031 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3032 | `			return SXERR_ABORT;` |
|        - |  3033 | `		}` |
|      181 |  3034 | `		pGen->pIn = pResEnd;` |
|      181 |  3035 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      149 |  3036 | `			pGen->pIn++; /* Skip trailing ',' */` |
|       73 |  3037 | `		}` |
|      181 |  3038 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|        3 |  3039 | `	}` |
|       69 |  3040 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|       69 |  3041 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|       69 |  3042 | `	return SXRET_OK;` |
|       40 |  3043 | `}` |
|        - |  3044 | `/*` |
|        - |  3045 | ` * Compile a backtick quoted string.` |
|        - |  3046 | ` */` |
|        4 |  3047 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        2 |  3048 | `{` |
|        - |  3049 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|        - |  3050 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|        - |  3051 | `	 */` |
|        8 |  3052 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|        - |  3053 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|        2 |  3054 | `		ph7_lib_version()` |
|        - |  3055 | `		);` |
|        - |  3056 | `	/* Load NULL */` |
|        6 |  3057 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        2 |  3058 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  3059 | `	/* Node successfully compiled */` |
|        6 |  3060 | `	return SXRET_OK;` |
|        2 |  3061 | `}` |
|        - |  3062 | `/*` |
|        - |  3063 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|        - |  3064 | ` * construct.` |
|        - |  3065 | ` */` |
|       82 |  3066 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  3067 | `{` |
|        - |  3068 | `	SyString *pName;` |
|        - |  3069 | `	sxu32 nKeyID;` |
|        - |  3070 | `	sxi32 rc;` |
|        - |  3071 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|       87 |  3072 | `	pName = &pGen->pIn->sData;` |
|       87 |  3073 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       87 |  3074 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|       87 |  3075 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|        9 |  3076 | `		SyToken *pTmp,*pNext = 0;` |
|        - |  3077 | `		/* Compile arguments one after one */` |
|        9 |  3078 | `		pTmp = pGen->pEnd;` |
|        - |  3079 | `		/* Symisc eXtension to the PHP programming language:` |
|        - |  3080 | `		 * 'echo' can be used in the context of a function which` |
|        - |  3081 | `		 *  mean that the following expression is valid:` |
|        - |  3082 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|        - |  3083 | `		 */` |
|        9 |  3084 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|       17 |  3085 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|        9 |  3086 | `			if( pGen->pIn < pNext ){` |
|        9 |  3087 | `				pGen->pEnd = pNext;` |
|        9 |  3088 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|        9 |  3089 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  3090 | `					return SXERR_ABORT;` |
|        - |  3091 | `				}` |
|        9 |  3092 | `				if( rc != SXERR_EMPTY ){` |
|        - |  3093 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|        - |  3094 | `					 * without the overhead of a function call.` |
|        - |  3095 | `					 * This is a very powerful optimization that improve` |
|        - |  3096 | `					 * performance greatly.` |
|        - |  3097 | `					 */` |
|        9 |  3098 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|        4 |  3099 | `				}` |
|        4 |  3100 | `			}` |
|        - |  3101 | `			/* Jump trailing commas */` |
|        9 |  3102 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|      ! 0 |  3103 | `				pNext++;` |
|      ! 0 |  3104 | `			}` |
|        9 |  3105 | `			pGen->pIn = pNext;` |
|        1 |  3106 | `		}` |
|        - |  3107 | `		/* Restore token stream */` |
|        9 |  3108 | `		pGen->pEnd = pTmp;` |
|        5 |  3109 | `	}else{` |
|       79 |  3110 | `		sxi32 nArg = 0;` |
|       79 |  3111 | `		sxu32 nIdx = 0;` |
|       79 |  3112 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|       79 |  3113 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3114 | `			return SXERR_ABORT;` |
|       79 |  3115 | `		}else if(rc != SXERR_EMPTY ){` |
|       79 |  3116 | `			nArg = 1;` |
|       37 |  3117 | `		}` |
|       79 |  3118 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|        - |  3119 | `			ph7_value *pObj;` |
|        - |  3120 | `			/* Emit the call instruction */` |
|       31 |  3121 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       31 |  3122 | `			if( pObj == 0 ){` |
|      ! 0 |  3123 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3124 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  3125 | `				return SXERR_ABORT;` |
|        - |  3126 | `			}` |
|       31 |  3127 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|        - |  3128 | `			/* Install in the literal table */` |
|       31 |  3129 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       13 |  3130 | `		}` |
|        - |  3131 | `		/* Emit the call instruction */` |
|       79 |  3132 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       79 |  3133 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        - |  3134 | `	}` |
|        - |  3135 | `	/* Node successfully compiled */` |
|       87 |  3136 | `	return SXRET_OK;` |
|       46 |  3137 | `}` |
|        - |  3138 | `/*` |
|        - |  3139 | ` * Compile a node holding a variable declaration.` |
|        - |  3140 | ` * According to the PHP language reference` |
|        - |  3141 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|        - |  3142 | ` *  The variable name is case-sensitive.` |
|        - |  3143 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|        - |  3144 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|        - |  3145 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|        - |  3146 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|        - |  3147 | ` *  Note: $this is a special variable that can't be assigned.` |
|        - |  3148 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|        - |  3149 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|        - |  3150 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|        - |  3151 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|        - |  3152 | ` *  the chapter on Expressions.` |
|        - |  3153 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|        - |  3154 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|        - |  3155 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|        - |  3156 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|        - |  3157 | ` *  is being assigned (the source variable).` |
|        - |  3158 | ` */` |
|  8492048 |  3159 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  3160 | `{` |
|  8492053 |  3161 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |  3162 | `	sxi32 iVv;` |
|        - |  3163 | `	sxi32 iP1;` |
|        - |  3164 | `	void *p3;` |
|        - |  3165 | `	sxi32 rc;` |
|  8492053 |  3166 | `	iVv = -1; /* Variable variable counter */` |
| 16984113 |  3167 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  8492065 |  3168 | `		pGen->pIn++;` |
|  8492065 |  3169 | `		iVv++;` |
|        5 |  3170 | `	}` |
|  8492053 |  3171 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|        - |  3172 | `		/* Invalid variable name */` |
|      ! 0 |  3173 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|      ! 0 |  3174 | `		if( rc == SXERR_ABORT ){` |
|        - |  3175 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3176 | `			return SXERR_ABORT;` |
|        - |  3177 | `		}` |
|      ! 0 |  3178 | `		return SXRET_OK;` |
|        - |  3179 | `	}` |
|  8492053 |  3180 | `	p3  = 0;` |
|  8492053 |  3181 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|        - |  3182 | `		/* Dynamic variable creation */` |
|       21 |  3183 | `		pGen->pIn++;  /* Jump the open curly */` |
|       21 |  3184 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|       21 |  3185 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - |  3186 | `			/* Empty expression */` |
|        3 |  3187 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|        3 |  3188 | `			return SXRET_OK;` |
|        - |  3189 | `		}` |
|        - |  3190 | `		/* Compile the expression holding the variable name */` |
|       18 |  3191 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       18 |  3192 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3193 | `			return SXERR_ABORT;` |
|       18 |  3194 | `		}else if( rc == SXERR_EMPTY ){` |
|        3 |  3195 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|        3 |  3196 | `			return SXRET_OK;` |
|        - |  3197 | `		}` |
|        8 |  3198 | `	}else{` |
|        - |  3199 | `		SyHashEntry *pEntry;` |
|        - |  3200 | `		SyString *pName;` |
|  8492035 |  3201 | `		char *zName = 0;` |
|        - |  3202 | `		/* Extract variable name */` |
|  8492035 |  3203 | `		pName = &pGen->pIn->sData;` |
|        - |  3204 | `		/* Advance the stream cursor */` |
|  8492035 |  3205 | `		pGen->pIn++;` |
|  8492035 |  3206 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  8492035 |  3207 | `		if( pEntry == 0 ){` |
|        - |  3208 | `			/* Duplicate name */` |
|   556831 |  3209 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   556831 |  3210 | `			if( zName == 0 ){` |
|      ! 0 |  3211 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3212 | `				return SXERR_ABORT;` |
|        - |  3213 | `			}` |
|        - |  3214 | `			/* Install in the hashtable */` |
|   556831 |  3215 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   278418 |  3216 | `		}else{` |
|        - |  3217 | `			/* Name already available */` |
|  7935209 |  3218 | `			zName = (char *)pEntry->pUserData;` |
|        - |  3219 | `		}` |
|  8492035 |  3220 | `		p3 = (void *)zName;` |
|        - |  3221 | `	}` |
|  8492049 |  3222 | `	iP1 = 0;` |
|  8492049 |  3223 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  2551311 |  3224 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|        - |  3225 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  2551293 |  3226 | `			iP1 = 1;` |
|  1275644 |  3227 | `		}` |
|  1275653 |  3228 | `	}` |
|        - |  3229 | `	/* Emit the load instruction */` |
|  8492049 |  3230 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  8492061 |  3231 | `	while( iVv > 0 ){` |
|       13 |  3232 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|       13 |  3233 | `		iVv--;` |
|        1 |  3234 | `	}` |
|        - |  3235 | `	/* Node successfully compiled */` |
|  8492049 |  3236 | `	return SXRET_OK;` |
|  4246029 |  3237 | `}` |
|        - |  3238 | `/*` |
|        - |  3239 | ` * Load a literal.` |
|        - |  3240 | ` */` |
|  5417680 |  3241 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|        5 |  3242 | `{` |
|  5417685 |  3243 | `	SyToken *pToken = pGen->pIn;` |
|        - |  3244 | `	ph7_value *pObj;` |
|        - |  3245 | `	SyString *pStr;` |
|        - |  3246 | `	sxu32 nIdx;` |
|        - |  3247 | `	/* Extract token value */` |
|  5417685 |  3248 | `	pStr = &pToken->sData;` |
|        - |  3249 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  5417685 |  3250 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  1317843 |  3251 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|        - |  3252 | `			/* NULL constant are always indexed at 0 */` |
|   554327 |  3253 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   554327 |  3254 | `			return SXRET_OK;` |
|   763521 |  3255 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|        - |  3256 | `			/* TRUE constant are always indexed at 1 */` |
|   143169 |  3257 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|   143169 |  3258 | `			return SXRET_OK;` |
|        5 |  3259 | `		}` |
|  4882650 |  3260 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   945254 |  3261 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|        - |  3262 | `			/* FALSE constant are always indexed at 2 */` |
|   415785 |  3263 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   415785 |  3264 | `			return SXRET_OK;` |
|  3950034 |  3265 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   531934 |  3266 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|        - |  3267 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    11543 |  3268 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    11543 |  3269 | `			if( pObj == 0 ){` |
|      ! 0 |  3270 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3271 | `				return SXERR_ABORT;` |
|        - |  3272 | `			}` |
|    11543 |  3273 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|        - |  3274 | `			/* Emit the load constant instruction */` |
|    11543 |  3275 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    11543 |  3276 | `			return SXRET_OK;` |
|  3701645 |  3277 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|    58232 |  3278 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|        - |  3279 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|        8 |  3280 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        8 |  3281 | `			if( pObj == 0 ){` |
|      ! 0 |  3282 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3283 | `				return SXERR_ABORT;` |
|        - |  3284 | `			}` |
|        8 |  3285 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        - |  3286 | `				SyString sNs;` |
|        8 |  3287 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        8 |  3288 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|        5 |  3289 | `			}else{` |
|      ! 0 |  3290 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|        - |  3291 | `			}` |
|        8 |  3292 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        8 |  3293 | `			return SXRET_OK;` |
|  3694161 |  3294 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   150259 |  3295 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  3779479 |  3296 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   213936 |  3297 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|       11 |  3298 | `			GenBlock *pBlock = pGen->pCurrent;` |
|        - |  3299 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|       21 |  3300 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|        - |  3301 | `				/* Point to the upper block */` |
|       11 |  3302 | `				pBlock = pBlock->pParent;` |
|        1 |  3303 | `			}` |
|       11 |  3304 | `			if( pBlock == 0 ){` |
|        - |  3305 | `				/* Called in the global scope,load NULL */` |
|        5 |  3306 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        3 |  3307 | `			}else{` |
|        - |  3308 | `				/* Extract the target function/method */` |
|        7 |  3309 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        7 |  3310 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|        - |  3311 | `					/* Not a class method,Load null */` |
|        3 |  3312 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        2 |  3313 | `				}else{` |
|        5 |  3314 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        5 |  3315 | `					if( pObj == 0 ){` |
|      ! 0 |  3316 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3317 | `						return SXERR_ABORT;` |
|        - |  3318 | `					}` |
|        5 |  3319 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|        - |  3320 | `					/* Emit the load constant instruction */` |
|        5 |  3321 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - |  3322 | `				}` |
|        - |  3323 | `			}` |
|       11 |  3324 | `			return SXRET_OK;` |
|        - |  3325 | `	}` |
|        - |  3326 | `	/* Query literal table */` |
|  4292865 |  3327 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|        - |  3328 | `		ph7_value *pLitObj;` |
|        - |  3329 | `		/* Unknown literal,install it in the literal table */` |
|   879293 |  3330 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   879293 |  3331 | `		if( pLitObj == 0 ){` |
|      ! 0 |  3332 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  3333 | `			return SXERR_ABORT;` |
|        - |  3334 | `		}` |
|   879293 |  3335 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|   879293 |  3336 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|   439644 |  3337 | `	}` |
|        - |  3338 | `	/* Emit the load constant instruction */` |
|  4292865 |  3339 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  4292865 |  3340 | `	return SXRET_OK;` |
|  2708845 |  3341 | `}` |
|        - |  3342 | `/*` |
|        - |  3343 | ` * Resolve a namespace path or simply load a literal.` |
|        - |  3344 | ` * If the token stream contains namespace separators (backslashes),` |
|        - |  3345 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|        - |  3346 | ` * Otherwise, load the simple literal directly.` |
|        - |  3347 | ` */` |
|  5421572 |  3348 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|        5 |  3349 | `{` |
|        - |  3350 | `	sxi32 rc;` |
|  5421577 |  3351 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  3352 | `		return SXRET_OK;` |
|        - |  3353 | `	}` |
|        - |  3354 | `	/* Check if this is a multi-token namespace path */` |
|  5421577 |  3355 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|        - |  3356 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|     3897 |  3357 | `		SyBlob *pWorker = &pGen->sWorker;` |
|     3897 |  3358 | `		int isAbsolute = 0;` |
|     3897 |  3359 | `		SyBlobReset(pWorker);` |
|        - |  3360 | `		/* Check for leading backslash (absolute path) */` |
|     3897 |  3361 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|     3895 |  3362 | `			isAbsolute = 1;` |
|     3895 |  3363 | `			pGen->pIn++; /* Skip leading backslash */` |
|     1945 |  3364 | `		}` |
|        - |  3365 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|     3897 |  3366 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        3 |  3367 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        3 |  3368 | `			SyBlobAppend(pWorker,"\\",1);` |
|        1 |  3369 | `		}` |
|        - |  3370 | `		/* Collect all path components */` |
|     4005 |  3371 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     4005 |  3372 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       59 |  3373 | `				SyBlobAppend(pWorker,"\\",1);` |
|       32 |  3374 | `			}else{` |
|     3951 |  3375 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - |  3376 | `			}` |
|     4005 |  3377 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|     3897 |  3378 | `				pGen->pIn++;` |
|     3897 |  3379 | `				break;` |
|        - |  3380 | `			}` |
|      113 |  3381 | `			pGen->pIn++;` |
|        5 |  3382 | `		}` |
|     3897 |  3383 | `		if( SyBlobLength(pWorker) > 0 ){` |
|        - |  3384 | `			ph7_value *pObj;` |
|        - |  3385 | `			SyString sPath;` |
|        - |  3386 | `			sxu32 nIdx;` |
|     3897 |  3387 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|        - |  3388 | `			/* Install in the literal table */` |
|     3897 |  3389 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|     3869 |  3390 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     3869 |  3391 | `				if( pObj == 0 ){` |
|      ! 0 |  3392 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  3393 | `					return SXERR_ABORT;` |
|        - |  3394 | `				}` |
|     3869 |  3395 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|     3869 |  3396 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     1932 |  3397 | `			}` |
|        - |  3398 | `			/* Emit the load constant instruction.` |
|        - |  3399 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|        - |  3400 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|     5843 |  3401 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|     1946 |  3402 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|     1946 |  3403 | `				nIdx,0,0);` |
|     3897 |  3404 | `			return SXRET_OK;` |
|        - |  3405 | `		}` |
|      ! 0 |  3406 | `	}` |
|        - |  3407 | `	/* Single-token literal: load directly */` |
|  5417685 |  3408 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  5417685 |  3409 | `	return rc;` |
|  2710791 |  3410 | `}` |
|        - |  3411 | `/*` |
|        - |  3412 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|        - |  3413 | ` */` |
|        - |  3414 | `/*` |
|        - |  3415 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|        - |  3416 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|        - |  3417 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|        - |  3418 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|        - |  3419 | ` */` |
|      ! 0 |  3420 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|      ! 0 |  3421 | `{` |
|      ! 0 |  3422 | `	SXUNUSED(iCompileFlag);` |
|      ! 0 |  3423 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|        - |  3424 | `		"Cannot use the first-class callable syntax '...' here");` |
|      ! 0 |  3425 | `	return SXERR_SYNTAX;` |
|      ! 0 |  3426 | `}` |
|  5421572 |  3427 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  3428 | `{` |
|        - |  3429 | `	sxi32 rc;` |
|  5421577 |  3430 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  5421577 |  3431 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  3432 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  3433 | `		return rc;` |
|        - |  3434 | `	}` |
|        - |  3435 | `	/* Node successfully compiled */` |
|  5421577 |  3436 | `	return SXRET_OK;` |
|  2710791 |  3437 | `}` |
|        - |  3438 | `/*` |
|        - |  3439 | ` * Recover from a compile-time error. In other words synchronize` |
|        - |  3440 | ` * the token stream cursor with the first semi-colon seen.` |
|        - |  3441 | ` */` |
|        8 |  3442 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|        1 |  3443 | `{` |
|        - |  3444 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|       17 |  3445 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|        9 |  3446 | `		pGen->pIn++;` |
|        1 |  3447 | `	}` |
|        9 |  3448 | `	return SXRET_OK;` |
|        1 |  3449 | `}` |
|        - |  3450 | `/*` |
|        - |  3451 | ` * Check if the given identifier name is reserved or not.` |
|        - |  3452 | ` * Return TRUE if reserved.FALSE otherwise.` |
|        - |  3453 | ` */` |
|   142396 |  3454 | `static int GenStateIsReservedConstant(SyString *pName)` |
|        5 |  3455 | `{` |
|   142401 |  3456 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|       40 |  3457 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|        3 |  3458 | `			return TRUE;` |
|       38 |  3459 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|        6 |  3460 | `			return TRUE;` |
|        3 |  3461 | `		}` |
|   142380 |  3462 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       16 |  3463 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|        3 |  3464 | `			return TRUE;` |
|        - |  3465 | `		}` |
|        6 |  3466 | `	}` |
|        - |  3467 | `	/* Not a reserved constant */` |
|   142393 |  3468 | `	return FALSE;` |
|    71203 |  3469 | `}` |
|        - |  3470 | `/*` |
|        - |  3471 | ` * Compile the 'const' statement.` |
|        - |  3472 | ` * According to the PHP language reference` |
|        - |  3473 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|        - |  3474 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|        - |  3475 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|        - |  3476 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|        - |  3477 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|        - |  3478 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|        - |  3479 | ` *  Syntax` |
|        - |  3480 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|        - |  3481 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|        - |  3482 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|        - |  3483 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|        - |  3484 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|        - |  3485 | ` *  to get a list of all defined constants.` |
|        - |  3486 | ` *` |
|        - |  3487 | ` * Symisc eXtension.` |
|        - |  3488 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|        - |  3489 | ` *  would allow only simple scalar value.` |
|        - |  3490 | ` *  Example` |
|        - |  3491 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|        - |  3492 | ` *    Refer to the official documentation for more information on this feature.` |
|        - |  3493 | ` */` |
|       44 |  3494 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|        5 |  3495 | `{` |
|        - |  3496 | `	SySet *pConsCode,*pInstrContainer;` |
|       49 |  3497 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |  3498 | `	SyString *pName;` |
|        - |  3499 | `	sxi32 rc;` |
|       49 |  3500 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       49 |  3501 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  3502 | `		/* Invalid constant name */` |
|        9 |  3503 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|        9 |  3504 | `		if( rc == SXERR_ABORT ){` |
|        - |  3505 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3506 | `			return SXERR_ABORT;` |
|        - |  3507 | `		}` |
|        9 |  3508 | `		goto Synchronize;` |
|        - |  3509 | `	}` |
|        - |  3510 | `	/* Peek constant name */` |
|       43 |  3511 | `	pName = &pGen->pIn->sData;` |
|        - |  3512 | `	/* Make sure the constant name isn't reserved */` |
|       43 |  3513 | `	if( GenStateIsReservedConstant(pName) ){` |
|        - |  3514 | `		/* Reserved constant */` |
|       10 |  3515 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       10 |  3516 | `		if( rc == SXERR_ABORT ){` |
|        - |  3517 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3518 | `			return SXERR_ABORT;` |
|        - |  3519 | `		}` |
|       10 |  3520 | `		goto Synchronize;` |
|        - |  3521 | `	}` |
|       34 |  3522 | `	pGen->pIn++;` |
|       34 |  3523 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|        - |  3524 | `		/* Invalid statement*/` |
|        6 |  3525 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|        6 |  3526 | `		if( rc == SXERR_ABORT ){` |
|        - |  3527 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3528 | `			return SXERR_ABORT;` |
|        - |  3529 | `		}` |
|        6 |  3530 | `		goto Synchronize;` |
|        - |  3531 | `	}` |
|       28 |  3532 | `	pGen->pIn++; /*Jump the equal sign */` |
|        - |  3533 | `	/* Allocate a new constant value container */` |
|       28 |  3534 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|       28 |  3535 | `	if( pConsCode == 0 ){` |
|      ! 0 |  3536 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3537 | `		return SXERR_ABORT;` |
|        - |  3538 | `	}` |
|       28 |  3539 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - |  3540 | `	/* Swap bytecode container */` |
|       28 |  3541 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       28 |  3542 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|        - |  3543 | `	/* Compile constant value */` |
|       28 |  3544 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  3545 | `	/* Emit the done instruction */` |
|       28 |  3546 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       28 |  3547 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       28 |  3548 | `	if( rc == SXERR_ABORT ){` |
|        - |  3549 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  3550 | `		return SXERR_ABORT;` |
|        - |  3551 | `	}` |
|       28 |  3552 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|        - |  3553 | `	/* Register the constant with namespace-qualified name */` |
|        - |  3554 | `	{` |
|        - |  3555 | `		SyBlob sFQN;` |
|        - |  3556 | `		SyString sFQNStr;` |
|       28 |  3557 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       28 |  3558 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|       28 |  3559 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       41 |  3560 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|       26 |  3561 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|       28 |  3562 | `		SyBlobRelease(&sFQN);` |
|        - |  3563 | `	}` |
|       28 |  3564 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  3565 | `		SySetRelease(pConsCode);` |
|      ! 0 |  3566 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|      ! 0 |  3567 | `	}` |
|       28 |  3568 | `	return SXRET_OK;` |
|        9 |  3569 | `Synchronize:` |
|        - |  3570 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|       60 |  3571 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       42 |  3572 | `		pGen->pIn++;` |
|        4 |  3573 | `	}` |
|       22 |  3574 | `	return SXRET_OK;` |
|       27 |  3575 | `}` |
|        - |  3576 | `/*` |
|        - |  3577 | ` * Compile the 'continue' statement.` |
|        - |  3578 | ` * According to the PHP language reference` |
|        - |  3579 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|        - |  3580 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|        - |  3581 | ` *  iteration.` |
|        - |  3582 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|        - |  3583 | ` *  the purposes of continue.` |
|        - |  3584 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|        - |  3585 | ` *  of enclosing loops it should skip to the end of.` |
|        - |  3586 | ` *  Note:` |
|        - |  3587 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|        - |  3588 | ` */` |
|        - |  3589 | `/*` |
|        - |  3590 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|        - |  3591 | ` * block and the target loop block. This ensures finally blocks run when` |
|        - |  3592 | ` * break/continue crosses a try boundary.` |
|        - |  3593 | ` *` |
|        - |  3594 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|        - |  3595 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|        - |  3596 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|        - |  3597 | ` */` |
|    57808 |  3598 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|        5 |  3599 | `{` |
|    57813 |  3600 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    57813 |  3601 | `	int nInlineTry = 0;` |
|   269467 |  3602 | `	while( pBlock && pBlock != pTarget ){` |
|   211659 |  3603 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|        6 |  3604 | `			if( pBlock->pUserData ){` |
|        - |  3605 | `				/* A try block with an exception context. In a generator its catch/finally` |
|        - |  3606 | `				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that` |
|        - |  3607 | `				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.` |
|        - |  3608 | `				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */` |
|        6 |  3609 | `				if( pGen->bInGenerator ){` |
|        3 |  3610 | `					nInlineTry++;` |
|        2 |  3611 | `				}else{` |
|        3 |  3612 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|        - |  3613 | `				}` |
|        4 |  3614 | `			}else{` |
|        - |  3615 | `				/* A catch/finally block compiled into a separate bytecode container` |
|        - |  3616 | `				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */` |
|      ! 0 |  3617 | `				break;` |
|        - |  3618 | `			}` |
|        2 |  3619 | `		}` |
|   211659 |  3620 | `		pBlock = pBlock->pParent;` |
|        5 |  3621 | `	}` |
|    57813 |  3622 | `	return nInlineTry;` |
|        5 |  3623 | `}` |
|    26958 |  3624 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|        5 |  3625 | `{` |
|        - |  3626 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  3627 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  3628 | `	sxu32 nLineLocal;` |
|        - |  3629 | `	sxi32 rc;` |
|    26963 |  3630 | `	nLineLocal = pGen->pIn->nLine;` |
|    26963 |  3631 | `	iLevel = 0;` |
|        - |  3632 | `	/* Jump the 'continue' keyword */` |
|    26963 |  3633 | `	pGen->pIn++;` |
|    26963 |  3634 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|        - |  3635 | `		/* optional numeric argument which tells us how many levels` |
|        - |  3636 | `		 * of enclosing loops we should skip to the end of.` |
|        - |  3637 | `		 */` |
|        - |  3638 | `		char zScratch[GEN_NUM_SCRATCH];` |
|       17 |  3639 | `		char *zAlloc = 0;` |
|        - |  3640 | `		SyString sNum;` |
|       17 |  3641 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|       17 |  3642 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3643 | `			return SXERR_ABORT;` |
|        - |  3644 | `		}` |
|       17 |  3645 | `		if( rc == SXRET_OK ){` |
|       20 |  3646 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       12 |  3647 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|       14 |  3648 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3649 | `				return SXERR_ABORT;` |
|        - |  3650 | `			}` |
|       14 |  3651 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       14 |  3652 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        6 |  3653 | `		}` |
|       17 |  3654 | `		if( iLevel < 2 ){` |
|        3 |  3655 | `			iLevel = 0;` |
|        1 |  3656 | `		}` |
|       17 |  3657 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|        7 |  3658 | `	}` |
|        - |  3659 | `	/* Point to the target loop */` |
|    26963 |  3660 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    26963 |  3661 | `	if( pLoop == 0 ){` |
|        - |  3662 | `		/* Illegal continue */` |
|       12 |  3663 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|       12 |  3664 | `		if( rc == SXERR_ABORT ){` |
|        - |  3665 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3666 | `			return SXERR_ABORT;` |
|        - |  3667 | `		}` |
|        7 |  3668 | `	}else{` |
|    26953 |  3669 | `		sxu32 nInstrIdx = 0;` |
|        - |  3670 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    26953 |  3671 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  3672 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|        - |  3673 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|    26953 |  3674 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|    26953 |  3675 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|        - |  3676 | `			/* According to the PHP language reference manual` |
|        - |  3677 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|        - |  3678 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|        - |  3679 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|        - |  3680 | `			 */` |
|        5 |  3681 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|        5 |  3682 | `			if( rc == SXRET_OK ){` |
|        5 |  3683 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|        2 |  3684 | `			}` |
|        3 |  3685 | `		}else{` |
|        - |  3686 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    26949 |  3687 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    26949 |  3688 | `			if( pLoop->bPostContinue == TRUE ){` |
|        - |  3689 | `				JumpFixup sJumpFix;` |
|        - |  3690 | `				/* Post-continue */` |
|       14 |  3691 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|       14 |  3692 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|       14 |  3693 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|        6 |  3694 | `			}` |
|        - |  3695 | `		}` |
|        - |  3696 | `	}` |
|    26963 |  3697 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  3698 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  3699 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|      ! 0 |  3700 | `	}` |
|        - |  3701 | `	/* Statement successfully compiled */` |
|    26963 |  3702 | `	return SXRET_OK;` |
|    13484 |  3703 | `}` |
|        - |  3704 | `/*` |
|        - |  3705 | ` * Compile the 'break' statement.` |
|        - |  3706 | ` * According to the PHP language reference` |
|        - |  3707 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|        - |  3708 | ` *  structure.` |
|        - |  3709 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|        - |  3710 | ` *  enclosing structures are to be broken out of.` |
|        - |  3711 | ` */` |
|    30876 |  3712 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|        5 |  3713 | `{` |
|        - |  3714 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  3715 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  3716 | `	sxi32 rc;` |
|    30881 |  3717 | `	iLevel = 0;` |
|        - |  3718 | `	/* Jump the 'break' keyword */` |
|    30881 |  3719 | `	pGen->pIn++;` |
|    30881 |  3720 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|        - |  3721 | `		/* optional numeric argument which tells us how many levels` |
|        - |  3722 | `		 * of enclosing loops we should skip to the end of.` |
|        - |  3723 | `		 */` |
|        - |  3724 | `		char zScratch[GEN_NUM_SCRATCH];` |
|       18 |  3725 | `		char *zAlloc = 0;` |
|        - |  3726 | `		SyString sNum;` |
|       18 |  3727 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|       18 |  3728 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3729 | `			return SXERR_ABORT;` |
|        - |  3730 | `		}` |
|       18 |  3731 | `		if( rc == SXRET_OK ){` |
|       21 |  3732 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       12 |  3733 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|       15 |  3734 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3735 | `				return SXERR_ABORT;` |
|        - |  3736 | `			}` |
|       15 |  3737 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       15 |  3738 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        6 |  3739 | `		}` |
|       18 |  3740 | `		if( iLevel < 2 ){` |
|        3 |  3741 | `			iLevel = 0;` |
|        1 |  3742 | `		}` |
|       18 |  3743 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|        7 |  3744 | `	}` |
|        - |  3745 | `	/* Extract the target loop */` |
|    30881 |  3746 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    30881 |  3747 | `	if( pLoop == 0 ){` |
|        - |  3748 | `		/* Illegal break */` |
|       19 |  3749 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|       19 |  3750 | `		if( rc == SXERR_ABORT ){` |
|        - |  3751 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3752 | `			return SXERR_ABORT;` |
|        - |  3753 | `		}` |
|       11 |  3754 | `	}else{` |
|        - |  3755 | `		sxu32 nInstrIdx;` |
|        - |  3756 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    30865 |  3757 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  3758 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|    30865 |  3759 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|    30865 |  3760 | `		if( rc == SXRET_OK ){` |
|        - |  3761 | `			/* Fix the jump later when the jump destination is resolved */` |
|    30865 |  3762 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|    15430 |  3763 | `		}` |
|        - |  3764 | `	}` |
|    30881 |  3765 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  3766 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  3767 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|      ! 0 |  3768 | `	}` |
|        - |  3769 | `	/* Statement successfully compiled */` |
|    30881 |  3770 | `	return SXRET_OK;` |
|    15443 |  3771 | `}` |
|        - |  3772 | `/*` |
|        - |  3773 | ` * Compile or record a label.` |
|        - |  3774 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|        - |  3775 | ` * Example` |
|        - |  3776 | ` *  goto LABEL;` |
|        - |  3777 | ` *   echo 'Foo';` |
|        - |  3778 | ` *  LABEL:` |
|        - |  3779 | ` *   echo 'Bar';` |
|        - |  3780 | ` */` |
|      112 |  3781 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|        5 |  3782 | `{` |
|        - |  3783 | `	GenBlock *pBlock;` |
|        - |  3784 | `	Label sLabel;` |
|        - |  3785 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|      117 |  3786 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|      117 |  3787 | `	if( pBlock ){` |
|        - |  3788 | `		sxi32 rc;` |
|        8 |  3789 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        4 |  3790 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|        6 |  3791 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3792 | `			return SXERR_ABORT;` |
|        - |  3793 | `		}` |
|        4 |  3794 | `	}else{` |
|      113 |  3795 | `		SyString *pTarget = &pGen->pIn->sData;` |
|        - |  3796 | `		char *zDup;` |
|        - |  3797 | `		/* Initialize label fields */` |
|      113 |  3798 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|        - |  3799 | `		/* Duplicate label name */` |
|      113 |  3800 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      113 |  3801 | `		if( zDup == 0 ){` |
|      ! 0 |  3802 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  3803 | `			return SXERR_ABORT;` |
|        - |  3804 | `		}` |
|      113 |  3805 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|      113 |  3806 | `		sLabel.bRef  = FALSE;` |
|      113 |  3807 | `		sLabel.nLine = pGen->pIn->nLine;` |
|      113 |  3808 | `		pBlock = pGen->pCurrent;` |
|      221 |  3809 | `		while( pBlock ){` |
|      133 |  3810 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|       24 |  3811 | `				break;` |
|        - |  3812 | `			}` |
|        - |  3813 | `			/* Point to the upper block */` |
|      113 |  3814 | `			pBlock = pBlock->pParent;` |
|        5 |  3815 | `		}` |
|      113 |  3816 | `		if( pBlock ){` |
|       24 |  3817 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       14 |  3818 | `		}else{` |
|       93 |  3819 | `			sLabel.pFunc = 0;` |
|        - |  3820 | `		}` |
|        - |  3821 | `		/* Insert in label set */` |
|      113 |  3822 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|        - |  3823 | `	}` |
|      117 |  3824 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|      117 |  3825 | `	return SXRET_OK;` |
|       61 |  3826 | `}` |
|        - |  3827 | `/*` |
|        - |  3828 | ` * Compile the so hated 'goto' statement.` |
|        - |  3829 | ` * You've probably been taught that gotos are bad, but this sort` |
|        - |  3830 | ` * of rewriting  happens all the time, in fact every time you run` |
|        - |  3831 | ` * a compiler it has to do this.` |
|        - |  3832 | ` * According to the PHP language reference manual` |
|        - |  3833 | ` *   The goto operator can be used to jump to another section in the program.` |
|        - |  3834 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|        - |  3835 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|        - |  3836 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|        - |  3837 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|        - |  3838 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|        - |  3839 | ` *   of a multi-level break` |
|        - |  3840 | ` */` |
|      152 |  3841 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|        5 |  3842 | `{` |
|        - |  3843 | `	JumpFixup sJump;` |
|        - |  3844 | `	sxi32 rc;` |
|      157 |  3845 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|      157 |  3846 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  3847 | `		/* Missing label */` |
|      ! 0 |  3848 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|      ! 0 |  3849 | `		if( rc == SXERR_ABORT ){` |
|        - |  3850 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3851 | `			return SXERR_ABORT;` |
|        - |  3852 | `		}` |
|      ! 0 |  3853 | `		return SXRET_OK;` |
|        - |  3854 | `	}` |
|      157 |  3855 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        6 |  3856 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|        6 |  3857 | `		if( rc == SXERR_ABORT ){` |
|        - |  3858 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3859 | `			return SXERR_ABORT;` |
|        - |  3860 | `		}` |
|        4 |  3861 | `	}else{` |
|      153 |  3862 | `		SyString *pTarget = &pGen->pIn->sData;` |
|        - |  3863 | `		GenBlock *pBlock;` |
|        - |  3864 | `		char *zDup;` |
|        - |  3865 | `		/* Prepare the jump destination */` |
|      153 |  3866 | `		sJump.nJumpType = PH7_OP_JMP;` |
|      153 |  3867 | `		sJump.nLine = pGen->pIn->nLine;` |
|        - |  3868 | `		/* Duplicate label name */` |
|      153 |  3869 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      153 |  3870 | `		if( zDup == 0 ){` |
|      ! 0 |  3871 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  3872 | `			return SXERR_ABORT;` |
|        - |  3873 | `		}` |
|      153 |  3874 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|      153 |  3875 | `		pBlock = pGen->pCurrent;` |
|      315 |  3876 | `		while( pBlock ){` |
|      199 |  3877 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|       36 |  3878 | `				break;` |
|        - |  3879 | `			}` |
|        - |  3880 | `			/* Point to the upper block */` |
|      167 |  3881 | `			pBlock = pBlock->pParent;` |
|        5 |  3882 | `		}` |
|      153 |  3883 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|        8 |  3884 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|        8 |  3885 | `			if( rc == SXERR_ABORT ){` |
|        - |  3886 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  3887 | `				return SXERR_ABORT;` |
|        - |  3888 | `			}` |
|        3 |  3889 | `		}` |
|      153 |  3890 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|       29 |  3891 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       16 |  3892 | `		}else{` |
|      127 |  3893 | `			sJump.pFunc = 0;` |
|        - |  3894 | `		}` |
|        - |  3895 | `		/* Emit the unconditional jump */` |
|      153 |  3896 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|      153 |  3897 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|       74 |  3898 | `		}` |
|        - |  3899 | `	}` |
|      157 |  3900 | `	pGen->pIn++; /* Jump the label name */` |
|      157 |  3901 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        3 |  3902 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|        1 |  3903 | `	}` |
|        - |  3904 | `	/* Statement successfully compiled */` |
|      157 |  3905 | `	return SXRET_OK;` |
|       81 |  3906 | `}` |
|        - |  3907 | `/*` |
|        - |  3908 | ` * Point to the next PHP chunk that will be processed shortly.` |
|        - |  3909 | ` * Return SXRET_OK on success. Any other return value indicates` |
|        - |  3910 | ` * failure.` |
|        - |  3911 | ` */` |
|       20 |  3912 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|        2 |  3913 | `{` |
|        - |  3914 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|        - |  3915 | `	sxu32 nRawObj;` |
|       10 |  3916 | `	sxu32 nObjIdx;` |
|        - |  3917 | `	/* Consume raw chunks verbatim without any processing until we get` |
|        - |  3918 | `	 * a PHP block.` |
|        - |  3919 | `	 */` |
|       10 |  3920 | `Consume:` |
|       22 |  3921 | `	nRawObj = nObjIdx = 0;` |
|       22 |  3922 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|      ! 0 |  3923 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|      ! 0 |  3924 | `		if( pRawObj == 0 ){` |
|      ! 0 |  3925 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3926 | `			return SXERR_ABORT;` |
|        - |  3927 | `		}` |
|        - |  3928 | `		/* Mark as constant and emit the load constant instruction */` |
|      ! 0 |  3929 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|      ! 0 |  3930 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|      ! 0 |  3931 | `		++nRawObj;` |
|      ! 0 |  3932 | `		pGen->pRawIn++; /* Next chunk */` |
|      ! 0 |  3933 | `	}` |
|       22 |  3934 | `	if( nRawObj > 0 ){` |
|        - |  3935 | `		/* Emit the consume instruction */` |
|      ! 0 |  3936 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      ! 0 |  3937 | `	}` |
|       22 |  3938 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|      ! 0 |  3939 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|        - |  3940 | `		/* Reset the token set (and its trivia sidecar) */` |
|      ! 0 |  3941 | `		SySetReset(pTokenSet);` |
|      ! 0 |  3942 | `		SySetReset(&pGen->aTrivia);` |
|        - |  3943 | `		/* Tokenize input */` |
|      ! 0 |  3944 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|      ! 0 |  3945 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|        - |  3946 | `		/* Point to the fresh token stream */` |
|      ! 0 |  3947 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|      ! 0 |  3948 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|        - |  3949 | `		/* Advance the stream cursor */` |
|      ! 0 |  3950 | `		pGen->pRawIn++;` |
|        - |  3951 | `		/* TICKET 1433-011 */` |
|      ! 0 |  3952 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|        - |  3953 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|        - |  3954 | `			sxi32 rc;` |
|        - |  3955 | `			/* Refer to TICKET 1433-009  */` |
|      ! 0 |  3956 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|      ! 0 |  3957 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|      ! 0 |  3958 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|      ! 0 |  3959 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|      ! 0 |  3960 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  3961 | `				return SXERR_ABORT;` |
|      ! 0 |  3962 | `			}else if( rc != SXERR_EMPTY ){` |
|      ! 0 |  3963 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 |  3964 | `			}` |
|      ! 0 |  3965 | `			goto Consume;` |
|        - |  3966 | `		}` |
|      ! 0 |  3967 | `	}else{` |
|        - |  3968 | `		/* No more chunks to process */` |
|       22 |  3969 | `		pGen->pIn = pGen->pEnd;` |
|       22 |  3970 | `		return SXERR_EOF;` |
|        - |  3971 | `	}` |
|      ! 0 |  3972 | `	return SXRET_OK;` |
|       12 |  3973 | `}` |
|        - |  3974 | `/*` |
|        - |  3975 | ` * Compile a PHP block.` |
|        - |  3976 | ` * A block is simply one or more PHP statements and expressions to compile` |
|        - |  3977 | ` * optionally delimited by braces {}.` |
|        - |  3978 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|        - |  3979 | ` * and this function takes care of generating the appropriate error` |
|        - |  3980 | ` * message.` |
|        - |  3981 | ` */` |
|  2950490 |  3982 | `static sxi32 PH7_CompileBlock(` |
|        - |  3983 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  3984 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|        - |  3985 | `	)` |
|        5 |  3986 | `{` |
|        - |  3987 | `	sxi32 rc;` |
|        - |  3988 | `	sxu32 nLine;` |
|  2950495 |  3989 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  2949053 |  3990 | `		nLine = pGen->pIn->nLine;` |
|  2949053 |  3991 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  2949053 |  3992 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  3993 | `			return SXERR_ABORT;` |
|        - |  3994 | `		}` |
|  2949053 |  3995 | `		pGen->pIn++;` |
|        - |  3996 | `		/* Compile until we hit the closing braces '}' */` |
|  4297282 |  3997 | `		for(;;){` |
|  8594569 |  3998 | `			if( pGen->pIn >= pGen->pEnd ){` |
|       22 |  3999 | `				rc = GenStateNextChunk(&(*pGen));` |
|       22 |  4000 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  4001 | `			 	   return SXERR_ABORT;` |
|        - |  4002 | `				}` |
|       22 |  4003 | `				if( rc == SXERR_EOF ){` |
|        - |  4004 | `					/* No more token to process. Missing closing braces */` |
|       22 |  4005 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|       22 |  4006 | `					break;` |
|        - |  4007 | `				}` |
|      ! 0 |  4008 | `			}` |
|  8594549 |  4009 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|        - |  4010 | `				/* Closing braces found,break immediately*/` |
|  2949033 |  4011 | `				pGen->pIn++;` |
|  2949033 |  4012 | `				break;` |
|        - |  4013 | `			}` |
|        - |  4014 | `			/* Compile a single statement */` |
|  5645521 |  4015 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  5645521 |  4016 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4017 | `				return SXERR_ABORT;` |
|        - |  4018 | `			}` |
|        5 |  4019 | `		}` |
|  2949053 |  4020 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  1475971 |  4021 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|      ! 0 |  4022 | `		pGen->pIn++;` |
|      ! 0 |  4023 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|      ! 0 |  4024 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  4025 | `			return SXERR_ABORT;` |
|        - |  4026 | `		}` |
|        - |  4027 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|      ! 0 |  4028 | `		for(;;){` |
|      ! 0 |  4029 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  4030 | `				rc = GenStateNextChunk(&(*pGen));` |
|      ! 0 |  4031 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  4032 | `			 	   return SXERR_ABORT;` |
|        - |  4033 | `				}` |
|      ! 0 |  4034 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|        - |  4035 | `					/* No more token to process */` |
|      ! 0 |  4036 | `					if( rc == SXERR_EOF ){` |
|      ! 0 |  4037 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|        - |  4038 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|      ! 0 |  4039 | `					}` |
|      ! 0 |  4040 | `					break;` |
|        - |  4041 | `				}` |
|      ! 0 |  4042 | `			}` |
|      ! 0 |  4043 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        - |  4044 | `				sxi32 nKwrd;` |
|        - |  4045 | `				/* Keyword found */` |
|      ! 0 |  4046 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      ! 0 |  4047 | `				if( nKwrd == nKeywordEnd \|\|` |
|      ! 0 |  4048 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|        - |  4049 | `						/* Delimiter keyword found,break */` |
|      ! 0 |  4050 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|      ! 0 |  4051 | `							pGen->pIn++; /*  endif;endswitch... */` |
|      ! 0 |  4052 | `						}` |
|      ! 0 |  4053 | `						break;` |
|        - |  4054 | `				}` |
|      ! 0 |  4055 | `			}` |
|        - |  4056 | `			/* Compile a single statement */` |
|      ! 0 |  4057 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|      ! 0 |  4058 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4059 | `				return SXERR_ABORT;` |
|        - |  4060 | `			}` |
|      ! 0 |  4061 | `		}` |
|      ! 0 |  4062 | `		GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  4063 | `	}else{` |
|        - |  4064 | `		/* Compile a single statement */` |
|     1447 |  4065 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     1447 |  4066 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4067 | `			return SXERR_ABORT;` |
|        - |  4068 | `		}` |
|        - |  4069 | `	}` |
|        - |  4070 | `	/* Jump trailing semi-colons ';' */` |
|  2950495 |  4071 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      ! 0 |  4072 | `		pGen->pIn++;` |
|      ! 0 |  4073 | `	}` |
|  2950495 |  4074 | `	return SXRET_OK;` |
|  1475250 |  4075 | `}` |
|        - |  4076 | `/*` |
|        - |  4077 | ` * Compile the gentle 'while' statement.` |
|        - |  4078 | ` * According to the PHP language reference` |
|        - |  4079 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|        - |  4080 | ` *  The basic form of a while statement is:` |
|        - |  4081 | ` *  while (expr)` |
|        - |  4082 | ` *   statement` |
|        - |  4083 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|        - |  4084 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|        - |  4085 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|        - |  4086 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|        - |  4087 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|        - |  4088 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|        - |  4089 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|        - |  4090 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|        - |  4091 | ` *  while (expr):` |
|        - |  4092 | ` *    statement` |
|        - |  4093 | ` *   endwhile;` |
|        - |  4094 | ` */` |
|    15508 |  4095 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|        5 |  4096 | `{` |
|    15513 |  4097 | `	GenBlock *pWhileBlock = 0;` |
|    15513 |  4098 | `	SyToken *pTmp,*pEnd = 0;` |
|        - |  4099 | `	sxu32 nFalseJump;` |
|        - |  4100 | `	sxu32 nLine;` |
|        - |  4101 | `	sxi32 rc;` |
|    15513 |  4102 | `	nLine = pGen->pIn->nLine;` |
|        - |  4103 | `	/* Jump the 'while' keyword */` |
|    15513 |  4104 | `	pGen->pIn++;` |
|    15513 |  4105 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4106 | `		/* Syntax error */` |
|      ! 0 |  4107 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  4108 | `		if( rc == SXERR_ABORT ){` |
|        - |  4109 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4110 | `			return SXERR_ABORT;` |
|        - |  4111 | `		}` |
|      ! 0 |  4112 | `		goto Synchronize;` |
|        - |  4113 | `	}` |
|        - |  4114 | `	/* Jump the left parenthesis '(' */` |
|    15513 |  4115 | `	pGen->pIn++;` |
|        - |  4116 | `	/* Create the loop block */` |
|    15513 |  4117 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|    15513 |  4118 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4119 | `		return SXERR_ABORT;` |
|        - |  4120 | `	}` |
|        - |  4121 | `	/* Delimit the condition */` |
|    15513 |  4122 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    15513 |  4123 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  4124 | `		/* Empty expression */` |
|        3 |  4125 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|        3 |  4126 | `		if( rc == SXERR_ABORT ){` |
|        - |  4127 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4128 | `			return SXERR_ABORT;` |
|        - |  4129 | `		}` |
|        1 |  4130 | `	}` |
|        - |  4131 | `	/* Swap token streams */` |
|    15513 |  4132 | `	pTmp = pGen->pEnd;` |
|    15513 |  4133 | `	pGen->pEnd = pEnd;` |
|        - |  4134 | `	/* Compile the expression */` |
|    15513 |  4135 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    15513 |  4136 | `	if( rc == SXERR_ABORT ){` |
|        - |  4137 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4138 | `		return SXERR_ABORT;` |
|        - |  4139 | `	}` |
|        - |  4140 | `	/* Update token stream */` |
|    15513 |  4141 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  4142 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4143 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4144 | `			return SXERR_ABORT;` |
|        - |  4145 | `		}` |
|      ! 0 |  4146 | `		pGen->pIn++;` |
|      ! 0 |  4147 | `	}` |
|        - |  4148 | `	/* Synchronize pointers */` |
|    15513 |  4149 | `	pGen->pIn  = &pEnd[1];` |
|    15513 |  4150 | `	pGen->pEnd = pTmp;` |
|        - |  4151 | `	/* Emit the false jump */` |
|    15513 |  4152 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - |  4153 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    15513 |  4154 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|        - |  4155 | `	/* Compile the loop body */` |
|    15513 |  4156 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|    15513 |  4157 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  4158 | `		return SXERR_ABORT;` |
|        - |  4159 | `	}` |
|        - |  4160 | `	/* Emit the unconditional jump to the start of the loop */` |
|    15513 |  4161 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|        - |  4162 | `	/* Fix all jumps now the destination is resolved */` |
|    15513 |  4163 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4164 | `	/* Release the loop block */` |
|    15513 |  4165 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4166 | `	/* Statement successfully compiled */` |
|    15513 |  4167 | `	return SXRET_OK;` |
|      ! 0 |  4168 | `Synchronize:` |
|        - |  4169 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  4170 | `	 * compiling this erroneous block.` |
|        - |  4171 | `	 */` |
|      ! 0 |  4172 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  4173 | `		pGen->pIn++;` |
|      ! 0 |  4174 | `	}` |
|      ! 0 |  4175 | `	return SXRET_OK;` |
|     7759 |  4176 | `}` |
|        - |  4177 | `/*` |
|        - |  4178 | ` * Compile the ugly do..while() statement.` |
|        - |  4179 | ` * According to the PHP language reference` |
|        - |  4180 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|        - |  4181 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|        - |  4182 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|        - |  4183 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|        - |  4184 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|        - |  4185 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|        - |  4186 | ` *  would end immediately).` |
|        - |  4187 | ` *  There is just one syntax for do-while loops:` |
|        - |  4188 | ` *  <?php` |
|        - |  4189 | ` *  $i = 0;` |
|        - |  4190 | ` *  do {` |
|        - |  4191 | ` *   echo $i;` |
|        - |  4192 | ` *  } while ($i > 0);` |
|        - |  4193 | ` * ?>` |
|        - |  4194 | ` */` |
|        2 |  4195 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|        1 |  4196 | `{` |
|        3 |  4197 | `	SyToken *pTmp,*pEnd = 0;` |
|        3 |  4198 | `	GenBlock *pDoBlock = 0;` |
|        - |  4199 | `	sxu32 nLine;` |
|        - |  4200 | `	sxi32 rc;` |
|        3 |  4201 | `	nLine = pGen->pIn->nLine;` |
|        - |  4202 | `	/* Jump the 'do' keyword */` |
|        3 |  4203 | `	pGen->pIn++;` |
|        - |  4204 | `	/* Create the loop block */` |
|        3 |  4205 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|        3 |  4206 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4207 | `		return SXERR_ABORT;` |
|        - |  4208 | `	}` |
|        - |  4209 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|        3 |  4210 | `	pDoBlock->bPostContinue = TRUE;` |
|        3 |  4211 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|        3 |  4212 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  4213 | `		return SXERR_ABORT;` |
|        - |  4214 | `	}` |
|        3 |  4215 | `	if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  4216 | `		nLine = pGen->pIn->nLine;` |
|      ! 0 |  4217 | `	}` |
|        3 |  4218 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|      ! 0 |  4219 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|        - |  4220 | `			/* Missing 'while' statement */` |
|        3 |  4221 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|        3 |  4222 | `			if( rc == SXERR_ABORT ){` |
|        - |  4223 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4224 | `				return SXERR_ABORT;` |
|        - |  4225 | `			}` |
|        3 |  4226 | `			goto Synchronize;` |
|        - |  4227 | `	}` |
|        - |  4228 | `	/* Jump the 'while' keyword */` |
|      ! 0 |  4229 | `	pGen->pIn++;` |
|      ! 0 |  4230 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4231 | `		/* Syntax error */` |
|      ! 0 |  4232 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  4233 | `		if( rc == SXERR_ABORT ){` |
|        - |  4234 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4235 | `			return SXERR_ABORT;` |
|        - |  4236 | `		}` |
|      ! 0 |  4237 | `		goto Synchronize;` |
|        - |  4238 | `	}` |
|        - |  4239 | `	/* Jump the left parenthesis '(' */` |
|      ! 0 |  4240 | `	pGen->pIn++;` |
|        - |  4241 | `	/* Delimit the condition */` |
|      ! 0 |  4242 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      ! 0 |  4243 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  4244 | `		/* Empty expression */` |
|      ! 0 |  4245 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|      ! 0 |  4246 | `		if( rc == SXERR_ABORT ){` |
|        - |  4247 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4248 | `			return SXERR_ABORT;` |
|        - |  4249 | `		}` |
|      ! 0 |  4250 | `		goto Synchronize;` |
|        - |  4251 | `	}` |
|        - |  4252 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|      ! 0 |  4253 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|        - |  4254 | `		JumpFixup *aPost;` |
|        - |  4255 | `		VmInstr *pInstr;` |
|        - |  4256 | `		sxu32 nJumpDest;` |
|        - |  4257 | `		sxu32 n;` |
|      ! 0 |  4258 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|      ! 0 |  4259 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      ! 0 |  4260 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|      ! 0 |  4261 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      ! 0 |  4262 | `			if( pInstr ){` |
|        - |  4263 | `				/* Fix */` |
|      ! 0 |  4264 | `				pInstr->iP2 = nJumpDest;` |
|      ! 0 |  4265 | `			}` |
|      ! 0 |  4266 | `		}` |
|      ! 0 |  4267 | `	}` |
|        - |  4268 | `	/* Swap token streams */` |
|      ! 0 |  4269 | `	pTmp = pGen->pEnd;` |
|      ! 0 |  4270 | `	pGen->pEnd = pEnd;` |
|        - |  4271 | `	/* Compile the expression */` |
|      ! 0 |  4272 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      ! 0 |  4273 | `	if( rc == SXERR_ABORT ){` |
|        - |  4274 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4275 | `		return SXERR_ABORT;` |
|        - |  4276 | `	}` |
|        - |  4277 | `	/* Update token stream */` |
|      ! 0 |  4278 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  4279 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4280 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4281 | `			return SXERR_ABORT;` |
|        - |  4282 | `		}` |
|      ! 0 |  4283 | `		pGen->pIn++;` |
|      ! 0 |  4284 | `	}` |
|      ! 0 |  4285 | `	pGen->pIn  = &pEnd[1];` |
|      ! 0 |  4286 | `	pGen->pEnd = pTmp;` |
|        - |  4287 | `	/* Emit the true jump to the beginning of the loop */` |
|      ! 0 |  4288 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|        - |  4289 | `	/* Fix all jumps now the destination is resolved */` |
|      ! 0 |  4290 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4291 | `	/* Release the loop block */` |
|      ! 0 |  4292 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4293 | `	/* Statement successfully compiled */` |
|      ! 0 |  4294 | `	return SXRET_OK;` |
|        1 |  4295 | `Synchronize:` |
|        - |  4296 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  4297 | `	 * compiling this erroneous block.` |
|        - |  4298 | `	 */` |
|        3 |  4299 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  4300 | `		pGen->pIn++;` |
|      ! 0 |  4301 | `	}` |
|        3 |  4302 | `	return SXRET_OK;` |
|        2 |  4303 | `}` |
|        - |  4304 | `/*` |
|        - |  4305 | ` * Compile the complex and powerful 'for' statement.` |
|        - |  4306 | ` * According to the PHP language reference` |
|        - |  4307 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|        - |  4308 | ` *  The syntax of a for loop is:` |
|        - |  4309 | ` *  for (expr1; expr2; expr3)` |
|        - |  4310 | ` *   statement` |
|        - |  4311 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|        - |  4312 | ` *  the beginning of the loop.` |
|        - |  4313 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|        - |  4314 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|        - |  4315 | ` *  to FALSE, the execution of the loop ends.` |
|        - |  4316 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|        - |  4317 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|        - |  4318 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|        - |  4319 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|        - |  4320 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|        - |  4321 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|        - |  4322 | ` *  of using the for truth expression.` |
|        - |  4323 | ` */` |
|    38574 |  4324 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|        5 |  4325 | `{` |
|    38579 |  4326 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|    38579 |  4327 | `	GenBlock *pForBlock = 0;` |
|        - |  4328 | `	sxu32 nFalseJump;` |
|        - |  4329 | `	sxu32 nLine;` |
|        - |  4330 | `	sxi32 rc;` |
|    38579 |  4331 | `	nLine = pGen->pIn->nLine;` |
|        - |  4332 | `	/* Jump the 'for' keyword */` |
|    38579 |  4333 | `	pGen->pIn++;` |
|    38579 |  4334 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4335 | `		/* Syntax error */` |
|      ! 0 |  4336 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|      ! 0 |  4337 | `		if( rc == SXERR_ABORT ){` |
|        - |  4338 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4339 | `			return SXERR_ABORT;` |
|        - |  4340 | `		}` |
|      ! 0 |  4341 | `		return SXRET_OK;` |
|        - |  4342 | `	}` |
|        - |  4343 | `	/* Jump the left parenthesis '(' */` |
|    38579 |  4344 | `	pGen->pIn++;` |
|        - |  4345 | `	/* Delimit the init-expr;condition;post-expr */` |
|    38579 |  4346 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    38579 |  4347 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  4348 | `		/* Empty expression */` |
|      ! 0 |  4349 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|      ! 0 |  4350 | `		if( rc == SXERR_ABORT ){` |
|        - |  4351 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4352 | `			return SXERR_ABORT;` |
|        - |  4353 | `		}` |
|        - |  4354 | `		/* Synchronize */` |
|      ! 0 |  4355 | `		pGen->pIn = pEnd;` |
|      ! 0 |  4356 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  4357 | `			pGen->pIn++;` |
|      ! 0 |  4358 | `		}` |
|      ! 0 |  4359 | `		return SXRET_OK;` |
|        - |  4360 | `	}` |
|        - |  4361 | `	/* Swap token streams */` |
|    38579 |  4362 | `	pTmp = pGen->pEnd;` |
|    38579 |  4363 | `	pGen->pEnd = pEnd;` |
|        - |  4364 | `	/* Compile initialization expressions if available */` |
|    38579 |  4365 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  4366 | `	/* Pop operand lvalues */` |
|    38579 |  4367 | `	if( rc == SXERR_ABORT ){` |
|        - |  4368 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4369 | `		return SXERR_ABORT;` |
|    38579 |  4370 | `	}else if( rc != SXERR_EMPTY ){` |
|    38577 |  4371 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    19286 |  4372 | `	}` |
|    38579 |  4373 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  4374 | `		/* Syntax error */` |
|      ! 0 |  4375 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  4376 | `			"for: Expected ';' after initialization expressions");` |
|      ! 0 |  4377 | `		if( rc == SXERR_ABORT ){` |
|        - |  4378 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4379 | `			return SXERR_ABORT;` |
|        - |  4380 | `		}` |
|      ! 0 |  4381 | `		return SXRET_OK;` |
|        - |  4382 | `	}` |
|        - |  4383 | `	/* Jump the trailing ';' */` |
|    38579 |  4384 | `	pGen->pIn++;` |
|        - |  4385 | `	/* Create the loop block */` |
|    38579 |  4386 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|    38579 |  4387 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4388 | `		return SXERR_ABORT;` |
|        - |  4389 | `	}` |
|        - |  4390 | `	/* Deffer continue jumps */` |
|    38579 |  4391 | `	pForBlock->bPostContinue = TRUE;` |
|        - |  4392 | `	/* Compile the condition */` |
|    38579 |  4393 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    38579 |  4394 | `	if( rc == SXERR_ABORT ){` |
|        - |  4395 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4396 | `		return SXERR_ABORT;` |
|    38579 |  4397 | `	}else if( rc != SXERR_EMPTY ){` |
|        - |  4398 | `		/* Emit the false jump */` |
|    38577 |  4399 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - |  4400 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    38577 |  4401 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    19286 |  4402 | `	}` |
|    38579 |  4403 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  4404 | `		/* Syntax error */` |
|        6 |  4405 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  4406 | `			"for: Expected ';' after conditionals expressions");` |
|        6 |  4407 | `		if( rc == SXERR_ABORT ){` |
|        - |  4408 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4409 | `			return SXERR_ABORT;` |
|        - |  4410 | `		}` |
|        6 |  4411 | `		return SXRET_OK;` |
|        - |  4412 | `	}` |
|        - |  4413 | `	/* Jump the trailing ';' */` |
|    38575 |  4414 | `	pGen->pIn++;` |
|        - |  4415 | `	/* Save the post condition stream */` |
|    38575 |  4416 | `	pPostStart = pGen->pIn;` |
|        - |  4417 | `	/* Compile the loop body */` |
|    38575 |  4418 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|    38575 |  4419 | `	pGen->pEnd = pTmp;` |
|    38575 |  4420 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|    38575 |  4421 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  4422 | `		return SXERR_ABORT;` |
|        - |  4423 | `	}` |
|        - |  4424 | `	/* Fix post-continue jumps */` |
|    38575 |  4425 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|        - |  4426 | `		JumpFixup *aPost;` |
|        - |  4427 | `		VmInstr *pInstr;` |
|        - |  4428 | `		sxu32 nJumpDest;` |
|        - |  4429 | `		sxu32 n;` |
|       14 |  4430 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|       14 |  4431 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       26 |  4432 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|       14 |  4433 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|       14 |  4434 | `			if( pInstr ){` |
|        - |  4435 | `				/* Fix jump */` |
|       14 |  4436 | `				pInstr->iP2 = nJumpDest;` |
|        6 |  4437 | `			}` |
|        8 |  4438 | `		}` |
|        6 |  4439 | `	}` |
|        - |  4440 | `	/* compile the post-expressions if available */` |
|    38575 |  4441 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|      ! 0 |  4442 | `		pPostStart++;` |
|      ! 0 |  4443 | `	}` |
|    38575 |  4444 | `	if( pPostStart < pEnd ){` |
|        - |  4445 | `		SyToken *pTmpIn,*pTmpEnd;` |
|    38575 |  4446 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|    38575 |  4447 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    38575 |  4448 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - |  4449 | `			/* Syntax error */` |
|      ! 0 |  4450 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|      ! 0 |  4451 | `			if( rc == SXERR_ABORT ){` |
|        - |  4452 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4453 | `				return SXERR_ABORT;` |
|        - |  4454 | `			}` |
|      ! 0 |  4455 | `			return SXRET_OK;` |
|        - |  4456 | `		}` |
|    38575 |  4457 | `		RE_SWAP_DELIMITER(pGen);` |
|    38575 |  4458 | `		if( rc == SXERR_ABORT ){` |
|        - |  4459 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4460 | `			return SXERR_ABORT;` |
|    38575 |  4461 | `		}else if( rc != SXERR_EMPTY){` |
|        - |  4462 | `			/* Pop operand lvalue */` |
|    38575 |  4463 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    19285 |  4464 | `		}` |
|    19285 |  4465 | `	}` |
|        - |  4466 | `	/* Emit the unconditional jump to the start of the loop */` |
|    38575 |  4467 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|        - |  4468 | `	/* Fix all jumps now the destination is resolved */` |
|    38575 |  4469 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4470 | `	/* Release the loop block */` |
|    38575 |  4471 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4472 | `	/* Statement successfully compiled */` |
|    38575 |  4473 | `	return SXRET_OK;` |
|    19292 |  4474 | `}` |
|        - |  4475 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|        - |  4476 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|        - |  4477 | ` * are allowed.` |
|        - |  4478 | ` */` |
|   235192 |  4479 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 |  4480 | `{` |
|   235197 |  4481 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|   235197 |  4482 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|        - |  4483 | `		/* Unexpected expression */` |
|      ! 0 |  4484 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - |  4485 | `			"foreach: Expecting a variable name");` |
|      ! 0 |  4486 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 |  4487 | `			rc = SXERR_INVALID;` |
|      ! 0 |  4488 | `		}` |
|      ! 0 |  4489 | `	}` |
|   235197 |  4490 | `	return rc;` |
|        5 |  4491 | `}` |
|        - |  4492 | `/*` |
|        - |  4493 | ` * Compile the 'foreach' statement.` |
|        - |  4494 | ` * According to the PHP language reference` |
|        - |  4495 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|        - |  4496 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|        - |  4497 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|        - |  4498 | ` *  is a minor but useful extension of the first:` |
|        - |  4499 | ` *  foreach (array_expression as $value)` |
|        - |  4500 | ` *    statement` |
|        - |  4501 | ` *  foreach (array_expression as $key => $value)` |
|        - |  4502 | ` *   statement` |
|        - |  4503 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|        - |  4504 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|        - |  4505 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|        - |  4506 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|        - |  4507 | ` *  to the variable $key on each loop.` |
|        - |  4508 | ` *  Note:` |
|        - |  4509 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|        - |  4510 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|        - |  4511 | ` *  Note:` |
|        - |  4512 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|        - |  4513 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|        - |  4514 | ` *  or after the foreach without resetting it.` |
|        - |  4515 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|        - |  4516 | ` *  of copying the value.` |
|        - |  4517 | ` */` |
|   169644 |  4518 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|        5 |  4519 | `{` |
|   169649 |  4520 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|   169649 |  4521 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|   169649 |  4522 | `	GenBlock *pForeachBlock = 0;` |
|        - |  4523 | `	ph7_foreach_info *pInfo;` |
|        - |  4524 | `	sxu32 nFalseJump;` |
|        - |  4525 | `	VmInstr *pInstr;` |
|        - |  4526 | `	sxu32 nLine;` |
|        - |  4527 | `	sxi32 rc;` |
|   169649 |  4528 | `	nLine = pGen->pIn->nLine;` |
|        - |  4529 | `	/* Jump the 'foreach' keyword */` |
|   169649 |  4530 | `	pGen->pIn++;` |
|   169649 |  4531 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4532 | `		/* Syntax error */` |
|      ! 0 |  4533 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|      ! 0 |  4534 | `		if( rc == SXERR_ABORT ){` |
|        - |  4535 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4536 | `			return SXERR_ABORT;` |
|        - |  4537 | `		}` |
|      ! 0 |  4538 | `		goto Synchronize;` |
|        - |  4539 | `	}` |
|        - |  4540 | `	/* Jump the left parenthesis '(' */` |
|   169649 |  4541 | `	pGen->pIn++;` |
|        - |  4542 | `	/* Create the loop block */` |
|   169649 |  4543 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|   169649 |  4544 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4545 | `		return SXERR_ABORT;` |
|        - |  4546 | `	}` |
|        - |  4547 | `	/* Delimit the expression */` |
|   169649 |  4548 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   169649 |  4549 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  4550 | `		/* Empty expression */` |
|      ! 0 |  4551 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|      ! 0 |  4552 | `		if( rc == SXERR_ABORT ){` |
|        - |  4553 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4554 | `			return SXERR_ABORT;` |
|        - |  4555 | `		}` |
|        - |  4556 | `		/* Synchronize */` |
|      ! 0 |  4557 | `		pGen->pIn = pEnd;` |
|      ! 0 |  4558 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  4559 | `			pGen->pIn++;` |
|      ! 0 |  4560 | `		}` |
|      ! 0 |  4561 | `		return SXRET_OK;` |
|        - |  4562 | `	}` |
|        - |  4563 | `	/* Compile the array expression */` |
|   169649 |  4564 | `	pCur = pGen->pIn;` |
|   991027 |  4565 | `	while( pCur < pEnd ){` |
|   991027 |  4566 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|   173507 |  4567 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|   173507 |  4568 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|        - |  4569 | `				/* Break with the first 'as' found */` |
|   169649 |  4570 | `				break;` |
|        - |  4571 | `			}` |
|     1929 |  4572 | `		}` |
|        - |  4573 | `		/* Advance the stream cursor */` |
|   821383 |  4574 | `		pCur++;` |
|        5 |  4575 | `	}` |
|   169649 |  4576 | `	if( pCur <= pGen->pIn ){` |
|      ! 0 |  4577 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  4578 | `			"foreach: Missing array/object expression");` |
|      ! 0 |  4579 | `		if( rc == SXERR_ABORT ){` |
|        - |  4580 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4581 | `			return SXERR_ABORT;` |
|        - |  4582 | `		}` |
|      ! 0 |  4583 | `		goto Synchronize;` |
|        - |  4584 | `	}` |
|        - |  4585 | `	/* Swap token streams */` |
|   169649 |  4586 | `	pTmp = pGen->pEnd;` |
|   169649 |  4587 | `	pGen->pEnd = pCur;` |
|   169649 |  4588 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   169649 |  4589 | `	if( rc == SXERR_ABORT ){` |
|        - |  4590 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4591 | `		return SXERR_ABORT;` |
|        - |  4592 | `	}` |
|        - |  4593 | `	/* Update token stream */` |
|   169649 |  4594 | `	while(pGen->pIn < pCur ){` |
|      ! 0 |  4595 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4596 | `		if( rc == SXERR_ABORT ){` |
|        - |  4597 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4598 | `			return SXERR_ABORT;` |
|        - |  4599 | `		}` |
|      ! 0 |  4600 | `		pGen->pIn++;` |
|      ! 0 |  4601 | `	}` |
|   169649 |  4602 | `	pCur++; /* Jump the 'as' keyword */` |
|   169649 |  4603 | `	pGen->pIn = pCur;` |
|   169649 |  4604 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 |  4605 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|      ! 0 |  4606 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4607 | `			return SXERR_ABORT;` |
|        - |  4608 | `		}` |
|      ! 0 |  4609 | `	}` |
|        - |  4610 | `	/* Create the foreach context */` |
|   169649 |  4611 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|   169649 |  4612 | `	if( pInfo == 0 ){` |
|      ! 0 |  4613 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 |  4614 | `		return SXERR_ABORT;` |
|        - |  4615 | `	}` |
|        - |  4616 | `	/* Zero the structure */` |
|   169649 |  4617 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|        - |  4618 | `	/* Initialize structure fields */` |
|   169649 |  4619 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|        - |  4620 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|        - |  4621 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|        - |  4622 | `	 * '=>'. */` |
|   169649 |  4623 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|   169649 |  4624 | `	if( pCur < pEnd ){` |
|        - |  4625 | `		/* Compile the expression holding the key name */` |
|    65573 |  4626 | `		if( pGen->pIn >= pCur ){` |
|      ! 0 |  4627 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|      ! 0 |  4628 | `			if( rc == SXERR_ABORT ){` |
|        - |  4629 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4630 | `				return SXERR_ABORT;` |
|        - |  4631 | `			}` |
|      ! 0 |  4632 | `		}else{` |
|    65573 |  4633 | `			pGen->pEnd = pCur;` |
|    65573 |  4634 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    65573 |  4635 | `			if( rc == SXERR_ABORT ){` |
|        - |  4636 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4637 | `				return SXERR_ABORT;` |
|        - |  4638 | `			}` |
|    65573 |  4639 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    65573 |  4640 | `			if( pInstr->p3 ){` |
|        - |  4641 | `				/* Record key name */` |
|    65573 |  4642 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    32784 |  4643 | `			}` |
|    65573 |  4644 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|        - |  4645 | `		}` |
|    65573 |  4646 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    32784 |  4647 | `	}` |
|   169649 |  4648 | `	pGen->pEnd = pEnd;` |
|   169649 |  4649 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 |  4650 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|      ! 0 |  4651 | `		if( rc == SXERR_ABORT ){` |
|        - |  4652 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4653 | `			return SXERR_ABORT;` |
|        - |  4654 | `		}` |
|      ! 0 |  4655 | `		goto Synchronize;` |
|        - |  4656 | `	}` |
|   169649 |  4657 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|       11 |  4658 | `		pGen->pIn++;` |
|        - |  4659 | `		/* Pass by reference  */` |
|       11 |  4660 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|        5 |  4661 | `	}` |
|        - |  4662 | `	/* Check if the value target is list() */` |
|   169649 |  4663 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        8 |  4664 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|        - |  4665 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|        - |  4666 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|        - |  4667 | `		 */` |
|        - |  4668 | `		static int iForeachListCnt = 0;` |
|        - |  4669 | `		char zTmp[128];` |
|        - |  4670 | `		sxu32 nLen;` |
|        - |  4671 | `		char *zDup;` |
|       10 |  4672 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|       10 |  4673 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       10 |  4674 | `		if( zDup == 0 ){` |
|      ! 0 |  4675 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  4676 | `			return SXERR_ABORT;` |
|        - |  4677 | `		}` |
|       10 |  4678 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - |  4679 | `		/* Save list() token boundaries */` |
|       10 |  4680 | `		pListStart = pGen->pIn;` |
|        - |  4681 | `		/* Advance past list(...) — validate parentheses */` |
|       10 |  4682 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|       10 |  4683 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        3 |  4684 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|        - |  4685 | `				"foreach: Expected '(' after 'list'");` |
|        3 |  4686 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4687 | `				return SXERR_ABORT;` |
|        - |  4688 | `			}` |
|        3 |  4689 | `			goto Synchronize;` |
|        - |  4690 | `		}` |
|        7 |  4691 | `		pGen->pIn++; /* Jump '(' */` |
|        7 |  4692 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|        7 |  4693 | `		if( pListEnd >= pEnd ){` |
|      ! 0 |  4694 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  4695 | `				"foreach: Missing closing ')' after list");` |
|      ! 0 |  4696 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4697 | `				return SXERR_ABORT;` |
|        - |  4698 | `			}` |
|      ! 0 |  4699 | `			goto Synchronize;` |
|        - |  4700 | `		}` |
|        7 |  4701 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|        7 |  4702 | `		pListEnd = pGen->pIn;` |
|        7 |  4703 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|   169644 |  4704 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|        - |  4705 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|        - |  4706 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|        - |  4707 | `		 */` |
|        - |  4708 | `		static int iForeachShortListCnt = 0;` |
|        - |  4709 | `		char zTmp[128];` |
|        - |  4710 | `		sxu32 nLen;` |
|        - |  4711 | `		char *zDup;` |
|       13 |  4712 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       13 |  4713 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       13 |  4714 | `		if( zDup == 0 ){` |
|      ! 0 |  4715 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  4716 | `			return SXERR_ABORT;` |
|        - |  4717 | `		}` |
|       13 |  4718 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - |  4719 | `		/* Save [...] token boundaries */` |
|       13 |  4720 | `		pListStart = pGen->pIn;` |
|        - |  4721 | `		/* Advance past [...] */` |
|       13 |  4722 | `		pGen->pIn++; /* Jump '[' */` |
|       13 |  4723 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       13 |  4724 | `		if( pListEnd >= pEnd ){` |
|      ! 0 |  4725 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  4726 | `				"foreach: Missing closing ']' after short list");` |
|      ! 0 |  4727 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4728 | `				return SXERR_ABORT;` |
|        - |  4729 | `			}` |
|      ! 0 |  4730 | `			goto Synchronize;` |
|        - |  4731 | `		}` |
|       13 |  4732 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       13 |  4733 | `		pListEnd = pGen->pIn;` |
|       13 |  4734 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|        7 |  4735 | `	}else{` |
|        - |  4736 | `		/* Compile the expression holding the value name */` |
|   169629 |  4737 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   169629 |  4738 | `		if( rc == SXERR_ABORT ){` |
|        - |  4739 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4740 | `			return SXERR_ABORT;` |
|        - |  4741 | `		}` |
|   169629 |  4742 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   169629 |  4743 | `		if( pInstr->p3 ){` |
|        - |  4744 | `			/* Record value name */` |
|   169629 |  4745 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    84812 |  4746 | `		}` |
|        - |  4747 | `	}` |
|        - |  4748 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|   169647 |  4749 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|        - |  4750 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   169647 |  4751 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|        - |  4752 | `	/* Record the first instruction to execute */` |
|   169647 |  4753 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|        - |  4754 | `	/* Emit the FOREACH_STEP instruction */` |
|   169647 |  4755 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|        - |  4756 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   169647 |  4757 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|        - |  4758 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|   169647 |  4759 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|        - |  4760 | `		SyToken *pSavedIn,*pSavedEnd;` |
|        - |  4761 | `		/* Load the temporary variable holding the current value onto the stack.` |
|        - |  4762 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|        - |  4763 | `		 */` |
|       19 |  4764 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|        - |  4765 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|        - |  4766 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|        - |  4767 | `		 * picks up the delimiter and the variable names inside.` |
|        - |  4768 | `		 */` |
|       19 |  4769 | `		pSavedIn = pGen->pIn;` |
|       19 |  4770 | `		pSavedEnd = pGen->pEnd;` |
|       19 |  4771 | `		pGen->pIn = pListStart;` |
|       19 |  4772 | `		pGen->pEnd = pListEnd;` |
|       19 |  4773 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       13 |  4774 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|        7 |  4775 | `		}else{` |
|        7 |  4776 | `			rc = PH7_CompileList(&(*pGen),0);` |
|        - |  4777 | `		}` |
|       19 |  4778 | `		pGen->pIn = pSavedIn;` |
|       19 |  4779 | `		pGen->pEnd = pSavedEnd;` |
|       19 |  4780 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4781 | `			return SXERR_ABORT;` |
|        - |  4782 | `		}` |
|        - |  4783 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       19 |  4784 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        9 |  4785 | `	}` |
|        - |  4786 | `	/* Compile the loop body */` |
|   169647 |  4787 | `	pGen->pIn = &pEnd[1];` |
|   169647 |  4788 | `	pGen->pEnd = pTmp;` |
|   169647 |  4789 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|   169647 |  4790 | `	if( rc == SXERR_ABORT ){` |
|        - |  4791 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4792 | `		return SXERR_ABORT;` |
|        - |  4793 | `	}` |
|        - |  4794 | `	/* Emit the unconditional jump to the start of the loop */` |
|   169647 |  4795 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|        - |  4796 | `	/* Fix all jumps now the destination is resolved */` |
|   169647 |  4797 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4798 | `	/* Release the loop block */` |
|   169647 |  4799 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4800 | `	/* Statement successfully compiled */` |
|   169647 |  4801 | `	return SXRET_OK;` |
|        1 |  4802 | `Synchronize:` |
|        - |  4803 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  4804 | `	 * compiling this erroneous block.` |
|        - |  4805 | `	 */` |
|        3 |  4806 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  4807 | `		pGen->pIn++;` |
|      ! 0 |  4808 | `	}` |
|        3 |  4809 | `	return SXRET_OK;` |
|    84827 |  4810 | `}` |
|        - |  4811 | `/*` |
|        - |  4812 | ` * Compile the infamous if/elseif/else if/else statements.` |
|        - |  4813 | ` * According to the PHP language reference` |
|        - |  4814 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|        - |  4815 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|        - |  4816 | ` *  that is similar to that of C:` |
|        - |  4817 | ` *  if (expr)` |
|        - |  4818 | ` *   statement` |
|        - |  4819 | ` *  else construct:` |
|        - |  4820 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|        - |  4821 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|        - |  4822 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|        - |  4823 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|        - |  4824 | ` *   $b, and a is NOT greater than b otherwise.` |
|        - |  4825 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|        - |  4826 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|        - |  4827 | ` *  elseif` |
|        - |  4828 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|        - |  4829 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|        - |  4830 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|        - |  4831 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|        - |  4832 | ` *   than b, a equal to b or a is smaller than b:` |
|        - |  4833 | ` *   <?php` |
|        - |  4834 | ` *    if ($a > $b) {` |
|        - |  4835 | ` *     echo "a is bigger than b";` |
|        - |  4836 | ` *    } elseif ($a == $b) {` |
|        - |  4837 | ` *     echo "a is equal to b";` |
|        - |  4838 | ` *    } else {` |
|        - |  4839 | ` *     echo "a is smaller than b";` |
|        - |  4840 | ` *    }` |
|        - |  4841 | ` *    ?>` |
|        - |  4842 | ` */` |
|  1144254 |  4843 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|        5 |  4844 | `{` |
|  1144259 |  4845 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  1144259 |  4846 | `	GenBlock *pCondBlock = 0;` |
|        - |  4847 | `	sxu32 nJumpIdx;` |
|        - |  4848 | `	sxu32 nKeyID;` |
|        - |  4849 | `	sxi32 rc;` |
|        - |  4850 | `	/* Jump the 'if' keyword */` |
|  1144259 |  4851 | `	pGen->pIn++;` |
|  1144259 |  4852 | `	pToken = pGen->pIn;` |
|        - |  4853 | `	/* Create the conditional block */` |
|  1144259 |  4854 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  1144259 |  4855 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4856 | `		return SXERR_ABORT;` |
|        - |  4857 | `	}` |
|        - |  4858 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   618314 |  4859 | `	for(;;){` |
|  1236633 |  4860 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4861 | `			/* Syntax error */` |
|      ! 0 |  4862 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 |  4863 | `				pToken--;` |
|      ! 0 |  4864 | `			}` |
|      ! 0 |  4865 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|      ! 0 |  4866 | `			if( rc == SXERR_ABORT ){` |
|        - |  4867 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4868 | `				return SXERR_ABORT;` |
|        - |  4869 | `			}` |
|      ! 0 |  4870 | `			goto Synchronize;` |
|        - |  4871 | `		}` |
|        - |  4872 | `		/* Jump the left parenthesis '(' */` |
|  1236633 |  4873 | `		pToken++;` |
|        - |  4874 | `		/* Delimit the condition */` |
|  1236633 |  4875 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  1236633 |  4876 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|        - |  4877 | `			/* Syntax error */` |
|      ! 0 |  4878 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 |  4879 | `				pToken--;` |
|      ! 0 |  4880 | `			}` |
|      ! 0 |  4881 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|      ! 0 |  4882 | `			if( rc == SXERR_ABORT ){` |
|        - |  4883 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4884 | `				return SXERR_ABORT;` |
|        - |  4885 | `			}` |
|      ! 0 |  4886 | `			goto Synchronize;` |
|        - |  4887 | `		}` |
|        - |  4888 | `		/* Swap token streams */` |
|  1236633 |  4889 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|        - |  4890 | `		/* Compile the condition */` |
|  1236633 |  4891 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  4892 | `		/* Update token stream */` |
|  1236633 |  4893 | `		while(pGen->pIn < pEnd ){` |
|      ! 0 |  4894 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4895 | `			pGen->pIn++;` |
|      ! 0 |  4896 | `		}` |
|  1236633 |  4897 | `		pGen->pIn  = &pEnd[1];` |
|  1236633 |  4898 | `		pGen->pEnd = pTmp;` |
|  1236633 |  4899 | `		if( rc == SXERR_ABORT ){` |
|        - |  4900 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4901 | `			return SXERR_ABORT;` |
|        - |  4902 | `		}` |
|        - |  4903 | `		/* Emit the false jump */` |
|  1236633 |  4904 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|        - |  4905 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  1236633 |  4906 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|        - |  4907 | `		/* Compile the body */` |
|  1236633 |  4908 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  1236633 |  4909 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4910 | `			return SXERR_ABORT;` |
|        - |  4911 | `		}` |
|  1236633 |  4912 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   233449 |  4913 | `			break;` |
|        - |  4914 | `		}` |
|        - |  4915 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   769745 |  4916 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   769745 |  4917 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   592329 |  4918 | `			break;` |
|        - |  4919 | `		}` |
|        - |  4920 | `		/* Emit the unconditional jump */` |
|   177421 |  4921 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|        - |  4922 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   177421 |  4923 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   177421 |  4924 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   169615 |  4925 | `			pToken = &pGen->pIn[1];` |
|   169615 |  4926 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    84606 |  4927 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    42526 |  4928 | `					break;` |
|        - |  4929 | `			}` |
|    84573 |  4930 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    42284 |  4931 | `		}` |
|    92379 |  4932 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|        - |  4933 | `		/* Synchronize cursors */` |
|    92379 |  4934 | `		pToken = pGen->pIn;` |
|        - |  4935 | `		/* Fix the false jump */` |
|    92379 |  4936 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|        5 |  4937 | `	} /* For(;;) */` |
|        - |  4938 | `	/* Fix the false jump */` |
|  1144259 |  4939 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  1144259 |  4940 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   677366 |  4941 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|        - |  4942 | `			/* Compile the else block */` |
|    85047 |  4943 | `			pGen->pIn++;` |
|    85047 |  4944 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    85047 |  4945 | `			if( rc == SXERR_ABORT ){` |
|        - |  4946 |  |
|      ! 0 |  4947 | `				return SXERR_ABORT;` |
|        - |  4948 | `			}` |
|    42521 |  4949 | `	}` |
|  1144259 |  4950 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|        - |  4951 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  1144259 |  4952 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|        - |  4953 | `	/* Release the conditional block */` |
|  1144259 |  4954 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4955 | `	/* Statement successfully compiled */` |
|  1144259 |  4956 | `	return SXRET_OK;` |
|      ! 0 |  4957 | `Synchronize:` |
|        - |  4958 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|        - |  4959 | `	 */` |
|      ! 0 |  4960 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  4961 | `		pGen->pIn++;` |
|      ! 0 |  4962 | `	}` |
|      ! 0 |  4963 | `	return SXRET_OK;` |
|   572132 |  4964 | `}` |
|        - |  4965 | `/*` |
|        - |  4966 | ` * Compile the global construct.` |
|        - |  4967 | ` * According to the PHP language reference` |
|        - |  4968 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|        - |  4969 | ` *  to be used in that function.` |
|        - |  4970 | ` *  Example #1 Using global` |
|        - |  4971 | ` *  <?php` |
|        - |  4972 | ` *   $a = 1;` |
|        - |  4973 | ` *   $b = 2;` |
|        - |  4974 | ` *   function Sum()` |
|        - |  4975 | ` *   {` |
|        - |  4976 | ` *    global $a, $b;` |
|        - |  4977 | ` *    $b = $a + $b;` |
|        - |  4978 | ` *   }` |
|        - |  4979 | ` *   Sum();` |
|        - |  4980 | ` *   echo $b;` |
|        - |  4981 | ` *  ?>` |
|        - |  4982 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|        - |  4983 | ` *  all references to either variable will refer to the global version. There is no limit` |
|        - |  4984 | ` *  to the number of global variables that can be manipulated by a function.` |
|        - |  4985 | ` */` |
|       36 |  4986 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|        5 |  4987 | `{` |
|       41 |  4988 | `	SyToken *pTmp,*pNext = 0;` |
|        - |  4989 | `	sxi32 nExpr;` |
|        - |  4990 | `	sxi32 rc;` |
|        - |  4991 | `	/* Jump the 'global' keyword */` |
|       41 |  4992 | `	pGen->pIn++;` |
|       41 |  4993 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        - |  4994 | `		/* Nothing to process */` |
|      ! 0 |  4995 | `		return SXRET_OK;` |
|        - |  4996 | `	}` |
|       41 |  4997 | `	pTmp = pGen->pEnd;` |
|       41 |  4998 | `	nExpr = 0;` |
|       87 |  4999 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       51 |  5000 | `		if( pGen->pIn < pNext ){` |
|       51 |  5001 | `			pGen->pEnd = pNext;` |
|       51 |  5002 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  5003 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|      ! 0 |  5004 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  5005 | `					return SXERR_ABORT;` |
|        - |  5006 | `				}` |
|      ! 0 |  5007 | `			}else{` |
|       51 |  5008 | `				pGen->pIn++;` |
|       51 |  5009 | `				if( pGen->pIn >= pGen->pEnd ){` |
|        - |  5010 | `					/* Emit a warning */` |
|      ! 0 |  5011 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|      ! 0 |  5012 | `				}else{` |
|       51 |  5013 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       51 |  5014 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  5015 | `						return SXERR_ABORT;` |
|       51 |  5016 | `					}else if(rc != SXERR_EMPTY ){` |
|       51 |  5017 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|       51 |  5018 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|        - |  5019 | `							/* Variable name, not a constant */` |
|       51 |  5020 | `							pLast->iP1 = 0;` |
|       23 |  5021 | `						}` |
|       51 |  5022 | `						nExpr++;` |
|       23 |  5023 | `					}` |
|        - |  5024 | `				}` |
|        - |  5025 | `			}` |
|       23 |  5026 | `		}` |
|        - |  5027 | `		/* Next expression in the stream */` |
|       51 |  5028 | `		pGen->pIn = pNext;` |
|        - |  5029 | `		/* Jump trailing commas */` |
|       61 |  5030 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       15 |  5031 | `			pGen->pIn++;` |
|        5 |  5032 | `		}` |
|        5 |  5033 | `	}` |
|        - |  5034 | `	/* Restore token stream */` |
|       41 |  5035 | `	pGen->pEnd = pTmp;` |
|       41 |  5036 | `	if( nExpr > 0 ){` |
|        - |  5037 | `		/* Emit the uplink instruction */` |
|       41 |  5038 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|       18 |  5039 | `	}` |
|       41 |  5040 | `	return SXRET_OK;` |
|       23 |  5041 | `}` |
|        - |  5042 | `/*` |
|        - |  5043 | ` * Compile the return statement.` |
|        - |  5044 | ` * According to the PHP language reference` |
|        - |  5045 | ` *  If called from within a function, the return() statement immediately ends execution` |
|        - |  5046 | ` *  of the current function, and returns its argument as the value of the function call.` |
|        - |  5047 | ` *  return() will also end the execution of an eval() statement or script file.` |
|        - |  5048 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|        - |  5049 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|        - |  5050 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|        - |  5051 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|        - |  5052 | ` *  from within the main script file, then script execution end.` |
|        - |  5053 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|        - |  5054 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|        - |  5055 | ` *  should do so as PHP has less work to do in this case.` |
|        - |  5056 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|        - |  5057 | ` */` |
|  1593092 |  5058 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|        5 |  5059 | `{` |
|  1593097 |  5060 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|        - |  5061 | `	sxi32 rc;` |
|  1593097 |  5062 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  1593097 |  5063 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|        - |  5064 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|        - |  5065 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|        - |  5066 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|        - |  5067 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|        - |  5068 | `	 * normally below so token processing stays consistent. */` |
|  4140299 |  5069 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  2547207 |  5070 | `		pFuncBlock = pFuncBlock->pParent;` |
|        5 |  5071 | `	}` |
|  1593092 |  5072 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|  1593065 |  5073 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|        3 |  5074 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  5075 | `			"A never-returning function must not return");` |
|        3 |  5076 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5077 | `			return SXERR_ABORT;` |
|        - |  5078 | `		}` |
|        1 |  5079 | `	}` |
|        - |  5080 | `	/* Jump the 'return' keyword */` |
|  1593097 |  5081 | `	pGen->pIn++;` |
|  1593097 |  5082 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  5083 | `		/* Compile the expression */` |
|  1577691 |  5084 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  1577691 |  5085 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5086 | `			return SXERR_ABORT;` |
|  1577691 |  5087 | `		}else if(rc != SXERR_EMPTY ){` |
|  1577691 |  5088 | `			nRet = 1;` |
|   788843 |  5089 | `		}` |
|   788843 |  5090 | `	}` |
|        - |  5091 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|        - |  5092 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|        - |  5093 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|        - |  5094 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|  1593097 |  5095 | `	if( pGen->bInGenerator ){` |
|       32 |  5096 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|       32 |  5097 | `		return SXRET_OK;` |
|        - |  5098 | `	}` |
|        - |  5099 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|        - |  5100 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|        - |  5101 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|        - |  5102 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|        - |  5103 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  1593069 |  5104 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  1593069 |  5105 | `	return SXRET_OK;` |
|   796551 |  5106 | `}` |
|        - |  5107 | `/*` |
|        - |  5108 | ` * Compile a yield expression.` |
|        - |  5109 | ` * Called from the expression code generator when a yield node is encountered.` |
|        - |  5110 | ` * Handles: yield, yield $value, yield $key => $value` |
|        - |  5111 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|        - |  5112 | ` */` |
|      338 |  5113 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        5 |  5114 | `{` |
|        - |  5115 | `	SyToken *pTmp, *pSplit;` |
|      343 |  5116 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      343 |  5117 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|        - |  5118 | `	sxi32 rc;` |
|      169 |  5119 | `	(void)iCompileFlag;` |
|        - |  5120 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      343 |  5121 | `	pGen->pIn++;` |
|        - |  5122 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|        - |  5123 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|        - |  5124 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|        - |  5125 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|        - |  5126 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|      338 |  5127 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|      200 |  5128 | `		&& pGen->pIn->sData.nByte == 4` |
|       68 |  5129 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|       67 |  5130 | `		pGen->pIn++; /* Skip 'from' */` |
|       67 |  5131 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       67 |  5132 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5133 | `			return SXERR_ABORT;` |
|        - |  5134 | `		}` |
|       67 |  5135 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 |  5136 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|      ! 0 |  5137 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|        - |  5138 | `				"Missing expression after 'yield from'");` |
|      ! 0 |  5139 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5140 | `				return SXERR_ABORT;` |
|        - |  5141 | `			}` |
|      ! 0 |  5142 | `		}` |
|       67 |  5143 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|       67 |  5144 | `		return SXRET_OK;` |
|        - |  5145 | `	}` |
|      281 |  5146 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  5147 | `		/* Bare yield — no value */` |
|        3 |  5148 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|        3 |  5149 | `		return SXRET_OK;` |
|        - |  5150 | `	}` |
|        - |  5151 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|      279 |  5152 | `	pSplit = 0;` |
|        - |  5153 | `	{` |
|      279 |  5154 | `		SyToken *pCur = pGen->pIn;` |
|      279 |  5155 | `		sxi32 nNest = 0;` |
|      585 |  5156 | `		while( pCur < pGen->pEnd ){` |
|      325 |  5157 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        3 |  5158 | `				nNest++;` |
|      324 |  5159 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        3 |  5160 | `				nNest--;` |
|      322 |  5161 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|       16 |  5162 | `				pSplit = pCur;` |
|       16 |  5163 | `				break;` |
|        - |  5164 | `			}` |
|      311 |  5165 | `			pCur++;` |
|        5 |  5166 | `		}` |
|        - |  5167 | `	}` |
|      279 |  5168 | `	pTmp = pGen->pEnd;` |
|      279 |  5169 | `	if( pSplit ){` |
|        - |  5170 | `		/* yield $key => $value */` |
|       16 |  5171 | `		pGen->pEnd = pSplit;` |
|       16 |  5172 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       16 |  5173 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       16 |  5174 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|       16 |  5175 | `		pGen->pEnd = pTmp;` |
|       16 |  5176 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       16 |  5177 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       16 |  5178 | `		iP1 = 1;` |
|       16 |  5179 | `		iP2 = 1;` |
|        9 |  5180 | `	}else{` |
|        - |  5181 | `		/* yield $value */` |
|      265 |  5182 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      265 |  5183 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      265 |  5184 | `		if( rc != SXERR_EMPTY ){` |
|      265 |  5185 | `			iP1 = 1;` |
|      130 |  5186 | `		}` |
|        - |  5187 | `	}` |
|      279 |  5188 | `	pGen->pEnd = pTmp;` |
|      279 |  5189 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      279 |  5190 | `	return SXRET_OK;` |
|      174 |  5191 | `}` |
|        - |  5192 | `/*` |
|        - |  5193 | ` * Compile the die/exit language construct.` |
|        - |  5194 | ` * The role of these constructs is to terminate execution of the script.` |
|        - |  5195 | ` * Shutdown functions will always be executed even if exit() is called.` |
|        - |  5196 | ` */` |
|      122 |  5197 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|        5 |  5198 | `{` |
|      127 |  5199 | `	sxi32 nExpr = 0;` |
|        - |  5200 | `	sxi32 rc;` |
|        - |  5201 | `	/* Jump the die/exit keyword */` |
|      127 |  5202 | `	pGen->pIn++;` |
|      127 |  5203 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  5204 | `		/* Compile the expression */` |
|      127 |  5205 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      127 |  5206 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5207 | `			return SXERR_ABORT;` |
|      127 |  5208 | `		}else if(rc != SXERR_EMPTY ){` |
|      127 |  5209 | `			nExpr = 1;` |
|       61 |  5210 | `		}` |
|       61 |  5211 | `	}` |
|        - |  5212 | `	/* Emit the HALT instruction */` |
|      127 |  5213 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      127 |  5214 | `	return SXRET_OK;` |
|       66 |  5215 | `}` |
|        - |  5216 | `/*` |
|        - |  5217 | ` * Compile the 'echo' language construct.` |
|        - |  5218 | ` */` |
|    15950 |  5219 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|        5 |  5220 | `{` |
|    15955 |  5221 | `	SyToken *pTmp,*pNext = 0;` |
|        - |  5222 | `	sxi32 rc;` |
|        - |  5223 | `	/* Jump the 'echo' keyword */` |
|    15955 |  5224 | `	pGen->pIn++;` |
|        - |  5225 | `	/* Compile arguments one after one */` |
|    15955 |  5226 | `	pTmp = pGen->pEnd;` |
|    38151 |  5227 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|    22201 |  5228 | `		if( pGen->pIn < pNext ){` |
|    22201 |  5229 | `			pGen->pEnd = pNext;` |
|    22201 |  5230 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|    22201 |  5231 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5232 | `				return SXERR_ABORT;` |
|    22201 |  5233 | `			}else if( rc != SXERR_EMPTY ){` |
|        - |  5234 | `				/* Emit the consume instruction */` |
|    22177 |  5235 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    11086 |  5236 | `			}` |
|    11098 |  5237 | `		}` |
|        - |  5238 | `		/* Jump trailing commas */` |
|    28447 |  5239 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     6251 |  5240 | `			pNext++;` |
|        5 |  5241 | `		}` |
|    22201 |  5242 | `		pGen->pIn = pNext;` |
|        5 |  5243 | `	}` |
|        - |  5244 | `	/* Restore token stream */` |
|    15955 |  5245 | `	pGen->pEnd = pTmp;` |
|    15955 |  5246 | `	return SXRET_OK;` |
|     7980 |  5247 | `}` |
|        - |  5248 | `/*` |
|        - |  5249 | ` * Compile the static statement.` |
|        - |  5250 | ` * According to the PHP language reference` |
|        - |  5251 | ` *  Another important feature of variable scoping is the static variable.` |
|        - |  5252 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|        - |  5253 | ` *  when program execution leaves this scope.` |
|        - |  5254 | ` *  Static variables also provide one way to deal with recursive functions.` |
|        - |  5255 | ` * Symisc eXtension.` |
|        - |  5256 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|        - |  5257 | ` *  the zend engine would allow only simple scalar value.` |
|        - |  5258 | ` *  Example` |
|        - |  5259 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|        - |  5260 | ` *    Refer to the official documentation for more information on this feature.` |
|        - |  5261 | ` */` |
|       10 |  5262 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|        3 |  5263 | `{` |
|        - |  5264 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|        - |  5265 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|        - |  5266 | `	GenBlock *pBlock;` |
|        - |  5267 | `	SyString *pName;` |
|        - |  5268 | `	char *zDup;` |
|        - |  5269 | `	sxu32 nLine;` |
|        - |  5270 | `	sxi32 rc;` |
|        - |  5271 | `	/* Jump the static keyword */` |
|       13 |  5272 | `	nLine = pGen->pIn->nLine;` |
|       13 |  5273 | `	pGen->pIn++;` |
|        - |  5274 | `	/* Extract the enclosing function if any */` |
|       13 |  5275 | `	pBlock = pGen->pCurrent;` |
|       23 |  5276 | `	while( pBlock ){` |
|       23 |  5277 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       13 |  5278 | `			break;` |
|        - |  5279 | `		}` |
|        - |  5280 | `		/* Point to the upper block */` |
|       13 |  5281 | `		pBlock = pBlock->pParent;` |
|        3 |  5282 | `	}` |
|       13 |  5283 | `	if( pBlock == 0 ){` |
|        - |  5284 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|      ! 0 |  5285 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  5286 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|      ! 0 |  5287 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5288 | `				return SXERR_ABORT;` |
|        - |  5289 | `			}` |
|      ! 0 |  5290 | `			goto Synchronize;` |
|        - |  5291 | `		}` |
|        - |  5292 | `		/* Compile the expression holding the variable */` |
|      ! 0 |  5293 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      ! 0 |  5294 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5295 | `			return SXERR_ABORT;` |
|      ! 0 |  5296 | `		}else if( rc != SXERR_EMPTY ){` |
|        - |  5297 | `			/* Emit the POP instruction */` |
|      ! 0 |  5298 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 |  5299 | `		}` |
|      ! 0 |  5300 | `		return SXRET_OK;` |
|        - |  5301 | `	}` |
|       13 |  5302 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        - |  5303 | `	/* Make sure we are dealing with a valid statement */` |
|       13 |  5304 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|        8 |  5305 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        3 |  5306 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|        3 |  5307 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5308 | `				return SXERR_ABORT;` |
|        - |  5309 | `			}` |
|        3 |  5310 | `			goto Synchronize;` |
|        - |  5311 | `	}` |
|       10 |  5312 | `	pGen->pIn++;` |
|        - |  5313 | `	/* Extract variable name */` |
|       10 |  5314 | `	pName = &pGen->pIn->sData;` |
|       10 |  5315 | `	pGen->pIn++; /* Jump the var name */` |
|       10 |  5316 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|      ! 0 |  5317 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  5318 | `		goto Synchronize;` |
|        - |  5319 | `	}` |
|        - |  5320 | `	/* Initialize the structure describing the static variable */` |
|       10 |  5321 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       10 |  5322 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|        - |  5323 | `	/* Duplicate variable name */` |
|       10 |  5324 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       10 |  5325 | `	if( zDup == 0 ){` |
|      ! 0 |  5326 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  5327 | `		return SXERR_ABORT;` |
|        - |  5328 | `	}` |
|       10 |  5329 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|        - |  5330 | `	/* Check if we have an expression to compile */` |
|       10 |  5331 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|        - |  5332 | `		SySet *pInstrContainer;` |
|        - |  5333 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|        - |  5334 | `		 * Static variable can take any complex expression including function` |
|        - |  5335 | `		 * call as their initialization value.` |
|        - |  5336 | `		 * Example:` |
|        - |  5337 | `		 *		static $var = foo(1,4+5,bar());` |
|        - |  5338 | `		 */` |
|       10 |  5339 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|        - |  5340 | `		/* Swap bytecode container */` |
|       10 |  5341 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       10 |  5342 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|        - |  5343 | `		/* Compile the expression */` |
|       10 |  5344 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  5345 | `		/* Emit the done instruction */` |
|       10 |  5346 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        - |  5347 | `		/* Restore default bytecode container */` |
|       10 |  5348 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        4 |  5349 | `	}` |
|        - |  5350 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       10 |  5351 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       10 |  5352 | `	return SXRET_OK;` |
|        1 |  5353 | `Synchronize:` |
|        - |  5354 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|        - |  5355 | `	 * statement.` |
|        - |  5356 | `	 */` |
|        5 |  5357 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|        3 |  5358 | `		pGen->pIn++;` |
|        1 |  5359 | `	}` |
|        3 |  5360 | `	return SXRET_OK;` |
|        8 |  5361 | `}` |
|        - |  5362 | `/*` |
|        - |  5363 | ` * Compile the var statement.` |
|        - |  5364 | ` * Symisc Extension:` |
|        - |  5365 | ` *      var statement can be used outside of a class definition.` |
|        - |  5366 | ` */` |
|        4 |  5367 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|        1 |  5368 | `{` |
|        - |  5369 | `	sxu32 nLine;` |
|        - |  5370 | `	sxi32 rc;` |
|        5 |  5371 | `	nLine = pGen->pIn->nLine;` |
|        - |  5372 | `	/* Jump the 'var' keyword */` |
|        5 |  5373 | `	pGen->pIn++;` |
|        5 |  5374 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|      ! 0 |  5375 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|        - |  5376 | `		/* Synchronize with the first semi-colon */` |
|      ! 0 |  5377 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|      ! 0 |  5378 | `			pGen->pIn++;` |
|      ! 0 |  5379 | `		}` |
|      ! 0 |  5380 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5381 | `			return SXERR_ABORT;` |
|        - |  5382 | `		}` |
|      ! 0 |  5383 | `	}else{` |
|        - |  5384 | `		/* Compile the expression */` |
|        5 |  5385 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        5 |  5386 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5387 | `			return SXERR_ABORT;` |
|        5 |  5388 | `		}else if( rc != SXERR_EMPTY ){` |
|        5 |  5389 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        2 |  5390 | `		}` |
|        - |  5391 | `	}` |
|        5 |  5392 | `	return SXRET_OK;` |
|        3 |  5393 | `}` |
|        - |  5394 | `/*` |
|        - |  5395 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|        - |  5396 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|        - |  5397 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|        - |  5398 | ` */` |
|        - |  5399 | `/*` |
|        - |  5400 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|        - |  5401 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|        - |  5402 | ` * hash and any shared references), this creates a new literal entry with the` |
|        - |  5403 | ` * qualified name and updates the instruction's operand index.` |
|        - |  5404 | ` *` |
|        - |  5405 | ` * Resolution order:` |
|        - |  5406 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|        - |  5407 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|        - |  5408 | ` *   3. Otherwise return the original literal index unchanged.` |
|        - |  5409 | ` *` |
|        - |  5410 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|        - |  5411 | ` * came from an import (step 1) and 0 otherwise.` |
|        - |  5412 | ` * Returns the (possibly new) literal index.` |
|        - |  5413 | ` */` |
|  2788200 |  5414 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|        5 |  5415 | `{` |
|        - |  5416 | `	ph7_value *pLit;` |
|        - |  5417 | `	const char *zLit;` |
|        - |  5418 | `	SyString sQualified;` |
|        - |  5419 | `	sxu32 nLit;` |
|        - |  5420 | `	sxu32 k;` |
|        - |  5421 | `	sxu32 nNewIdx;` |
|        - |  5422 | `	int hasNsSep;` |
|        - |  5423 | `	SyHashEntry *pImport;` |
|        - |  5424 | `	ph7_value *pNew;` |
|  2788205 |  5425 | `	if( pFromImport ){` |
|  2285951 |  5426 | `		*pFromImport = 0;` |
|  1142973 |  5427 | `	}` |
|  2788205 |  5428 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  2788205 |  5429 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|      ! 0 |  5430 | `		return nOrigIdx;` |
|        - |  5431 | `	}` |
|  2788205 |  5432 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  2788205 |  5433 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|        - |  5434 | `	/* Skip if already qualified (contains backslash) */` |
|  2788205 |  5435 | `	hasNsSep = 0;` |
| 35471567 |  5436 | `	for( k = 0; k < nLit; k++ ){` |
| 32683375 |  5437 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 16341686 |  5438 | `	}` |
|  2788205 |  5439 | `	if( hasNsSep ){` |
|       10 |  5440 | `		return nOrigIdx;` |
|        - |  5441 | `	}` |
|        - |  5442 | `	/* Check use imports first (works even outside namespaces) */` |
|  2788197 |  5443 | `	SyBlobReset(&pGen->sWorker);` |
|  2788197 |  5444 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  2788197 |  5445 | `	if( pImport ){` |
|       41 |  5446 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       41 |  5447 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|       41 |  5448 | `		if( pFromImport ){` |
|       18 |  5449 | `			*pFromImport = 1;` |
|        8 |  5450 | `		}` |
|       23 |  5451 | `	}else{` |
|  2788161 |  5452 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  2788071 |  5453 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|        - |  5454 | `		}` |
|        - |  5455 | `		/* Prepend current namespace */` |
|       95 |  5456 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       95 |  5457 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|       95 |  5458 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|        - |  5459 | `	}` |
|        - |  5460 | `	/* Look up or create a new literal for the qualified name */` |
|      131 |  5461 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|      131 |  5462 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|       57 |  5463 | `		return nNewIdx; /* Already interned */` |
|        - |  5464 | `	}` |
|       79 |  5465 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|       79 |  5466 | `	if( pNew == 0 ){` |
|      ! 0 |  5467 | `		return nOrigIdx; /* OOM, fall back to original */` |
|        - |  5468 | `	}` |
|       79 |  5469 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|       79 |  5470 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|       79 |  5471 | `	return nNewIdx;` |
|  1394105 |  5472 | `}` |
|        - |  5473 | `/*` |
|        - |  5474 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|        - |  5475 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|        - |  5476 | ` */` |
|   185686 |  5477 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 |  5478 | `{` |
|        - |  5479 | `	SyHashEntry *pImport;` |
|        - |  5480 | `	/* Check use imports first */` |
|   185691 |  5481 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   185691 |  5482 | `	if( pImport ){` |
|       19 |  5483 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       19 |  5484 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|       19 |  5485 | `		return;` |
|        - |  5486 | `	}` |
|        - |  5487 | `	/* Prepend current namespace if active */` |
|   185675 |  5488 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        8 |  5489 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        8 |  5490 | `		SyBlobAppend(pOut,"\\",1);` |
|        3 |  5491 | `	}` |
|   185675 |  5492 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    92848 |  5493 | `}` |
|        - |  5494 | `/*` |
|        - |  5495 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|        - |  5496 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|        - |  5497 | ` * The caller must release pOut when done.` |
|        - |  5498 | ` */` |
|   259128 |  5499 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 |  5500 | `{` |
|   259133 |  5501 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     3907 |  5502 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     3907 |  5503 | `		SyBlobAppend(pOut,"\\",1);` |
|     1951 |  5504 | `	}` |
|   259133 |  5505 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   259133 |  5506 | `}` |
|        - |  5507 | `/*` |
|        - |  5508 | ` * Compile a namespace statement` |
|        - |  5509 | ` * According to the PHP language reference manual` |
|        - |  5510 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|        - |  5511 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|        - |  5512 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|        - |  5513 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|        - |  5514 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|        - |  5515 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|        - |  5516 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|        - |  5517 | ` *  programming world.` |
|        - |  5518 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|        - |  5519 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|        - |  5520 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|        - |  5521 | ` *  classes/functions/constants.` |
|        - |  5522 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|        - |  5523 | ` *  readability of source code.` |
|        - |  5524 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|        - |  5525 | ` *  Here is an example of namespace syntax in PHP:` |
|        - |  5526 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|        - |  5527 | ` *       class MyClass {}` |
|        - |  5528 | ` *       function myfunction() {}` |
|        - |  5529 | ` *       const MYCONST = 1;` |
|        - |  5530 | ` *       $a = new MyClass;` |
|        - |  5531 | ` *       $c = new \my\name\MyClass;` |
|        - |  5532 | ` *       $a = strlen('hi');` |
|        - |  5533 | ` *       $d = namespace\MYCONST;` |
|        - |  5534 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|        - |  5535 | ` *       echo constant($d);` |
|        - |  5536 | ` * NOTE` |
|        - |  5537 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - |  5538 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - |  5539 | ` */` |
|        - |  5540 | `/*` |
|        - |  5541 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|        - |  5542 | ` */` |
|       14 |  5543 | `static const char * TokenTypeName(sxu32 nType)` |
|        3 |  5544 | `{` |
|       17 |  5545 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       11 |  5546 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       11 |  5547 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       11 |  5548 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       11 |  5549 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       11 |  5550 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|        3 |  5551 | `	return "token";` |
|       10 |  5552 | `}` |
|     3950 |  5553 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|        5 |  5554 | `{` |
|        - |  5555 | `	sxu32 nLine;` |
|        - |  5556 | `	sxi32 rc;` |
|     3955 |  5557 | `	nLine = pGen->pIn->nLine;` |
|     3955 |  5558 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|        - |  5559 | `	/* Reset namespace and clear previous use imports */` |
|     3955 |  5560 | `	SyBlobReset(&pGen->sNamespace);` |
|     3955 |  5561 | `	SyHashRelease(&pGen->hUseImports);` |
|     3955 |  5562 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     3955 |  5563 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     3955 |  5564 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     3955 |  5565 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     3955 |  5566 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     3955 |  5567 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  5568 | `		/* Global namespace (bare "namespace;") */` |
|      ! 0 |  5569 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 |  5570 | `		return SXRET_OK;` |
|        - |  5571 | `	}` |
|     3955 |  5572 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        - |  5573 | `		/* namespace; — switch to global namespace */` |
|      ! 0 |  5574 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 |  5575 | `		return SXRET_OK;` |
|        - |  5576 | `	}` |
|     3955 |  5577 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|        - |  5578 | `		/* namespace { } — global namespace block */` |
|      ! 0 |  5579 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 |  5580 | `		return SXRET_OK;` |
|        - |  5581 | `	}` |
|        - |  5582 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     7947 |  5583 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     3997 |  5584 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        - |  5585 | `			/* Append backslash separator */` |
|       26 |  5586 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       26 |  5587 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|       11 |  5588 | `			}` |
|       15 |  5589 | `		}else{` |
|        - |  5590 | `			/* Append identifier */` |
|     3975 |  5591 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - |  5592 | `		}` |
|     3997 |  5593 | `		pGen->pIn++;` |
|        5 |  5594 | `	}` |
|        - |  5595 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|        - |  5596 | `	 * at the correct program counter, not just the last one compiled. */` |
|        - |  5597 | `	{` |
|     3955 |  5598 | `		char *zNsDup = 0;` |
|     3955 |  5599 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     5927 |  5600 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     3948 |  5601 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     1974 |  5602 | `		}` |
|     3955 |  5603 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|        - |  5604 | `	}` |
|     3955 |  5605 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        8 |  5606 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  5607 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|        4 |  5608 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        6 |  5609 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5610 | `			return SXERR_ABORT;` |
|        - |  5611 | `		}` |
|        2 |  5612 | `	}` |
|     3955 |  5613 | `	return SXRET_OK;` |
|     1980 |  5614 | `}` |
|        - |  5615 | `/*` |
|        - |  5616 | ` * Compile the 'use' statement` |
|        - |  5617 | ` * According to the PHP language reference manual` |
|        - |  5618 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|        - |  5619 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|        - |  5620 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|        - |  5621 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|        - |  5622 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|        - |  5623 | ` *  a function or constant is not supported.` |
|        - |  5624 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|        - |  5625 | ` * NOTE` |
|        - |  5626 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - |  5627 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - |  5628 | ` */` |
|       72 |  5629 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|        5 |  5630 | `{` |
|        - |  5631 | `	sxu32 nLine;` |
|        - |  5632 | `	sxi32 rc;` |
|        - |  5633 | `	SyBlob sPath;` |
|        - |  5634 | `	SyString sAlias;` |
|        - |  5635 | `	SyToken *pLast;` |
|        - |  5636 | `	char *zDup;` |
|        - |  5637 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|        - |  5638 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|        - |  5639 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|       77 |  5640 | `	nLine = pGen->pIn->nLine;` |
|       77 |  5641 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|        - |  5642 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|       77 |  5643 | `	iUseType = 0;` |
|       77 |  5644 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       30 |  5645 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|       30 |  5646 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|       16 |  5647 | `			iUseType = 1;` |
|       16 |  5648 | `			pGen->pIn++;` |
|       23 |  5649 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|       16 |  5650 | `			iUseType = 2;` |
|       16 |  5651 | `			pGen->pIn++;` |
|        7 |  5652 | `		}` |
|       14 |  5653 | `	}` |
|        - |  5654 | `	/* Select target hash tables based on import type */` |
|       77 |  5655 | `	switch( iUseType ){` |
|        7 |  5656 | `		case 1:` |
|       16 |  5657 | `			pGenHash = &pGen->hUseFuncImports;` |
|       16 |  5658 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|       16 |  5659 | `			break;` |
|        7 |  5660 | `		case 2:` |
|       16 |  5661 | `			pGenHash = &pGen->hUseConstImports;` |
|       16 |  5662 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|       16 |  5663 | `			break;` |
|       22 |  5664 | `		default:` |
|       49 |  5665 | `			pGenHash = &pGen->hUseImports;` |
|       49 |  5666 | `			pVmHash = &pGen->pVm->hUseImports;` |
|       44 |  5667 | `			break;` |
|        - |  5668 | `	}` |
|       77 |  5669 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|        - |  5670 | `	/* Process one or more use declarations separated by commas */` |
|       37 |  5671 | `	for(;;){` |
|       79 |  5672 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  5673 | `			break;` |
|        - |  5674 | `		}` |
|       79 |  5675 | `		SyBlobReset(&sPath);` |
|       79 |  5676 | `		pLast = 0;` |
|        - |  5677 | `		/* Collect the full namespace path */` |
|      269 |  5678 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|      195 |  5679 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|      135 |  5680 | `				pLast = pGen->pIn;` |
|      135 |  5681 | `				if( SyBlobLength(&sPath) > 0 ){` |
|       65 |  5682 | `					SyBlobAppend(&sPath,"\\",1);` |
|       30 |  5683 | `				}` |
|      135 |  5684 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       65 |  5685 | `			}` |
|      195 |  5686 | `			pGen->pIn++;` |
|        5 |  5687 | `		}` |
|       79 |  5688 | `		if( pLast == 0 ){` |
|        - |  5689 | `			/* Empty path */` |
|        6 |  5690 | `			break;` |
|        - |  5691 | `		}` |
|        - |  5692 | `		/* Default alias is the last component of the path */` |
|       75 |  5693 | `		sAlias = pLast->sData;` |
|        - |  5694 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|       70 |  5695 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       50 |  5696 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|       24 |  5697 | `			pGen->pIn++; /* Jump 'as' */` |
|       24 |  5698 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|       24 |  5699 | `				sAlias = pGen->pIn->sData;` |
|       24 |  5700 | `				pGen->pIn++;` |
|       10 |  5701 | `			}` |
|       10 |  5702 | `		}` |
|        - |  5703 | `		/* Check for duplicate import alias (per-type) */` |
|       75 |  5704 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|        8 |  5705 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  5706 | `				"Cannot use %.*s as %z because the name is already in use",` |
|        4 |  5707 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|        6 |  5708 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5709 | `				SyBlobRelease(&sPath);` |
|      ! 0 |  5710 | `				return SXERR_ABORT;` |
|        - |  5711 | `			}` |
|        2 |  5712 | `		}` |
|        - |  5713 | `		/* Register the import: alias -> FQN.` |
|        - |  5714 | `		 * Strings are allocated from the VM pool allocator and freed` |
|        - |  5715 | `		 * when the entire VM is released. SyHashRelease does not free` |
|        - |  5716 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      110 |  5717 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       70 |  5718 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|       75 |  5719 | `		if( zDup ){` |
|       75 |  5720 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|       75 |  5721 | `			if( pVmHash ){` |
|        - |  5722 | `				/* Class imports: populate VM table directly (class resolution` |
|        - |  5723 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|       47 |  5724 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       47 |  5725 | `				if( zAliasDup ){` |
|       47 |  5726 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|       21 |  5727 | `				}` |
|       21 |  5728 | `			}` |
|       75 |  5729 | `			if( iUseType == 2 ){` |
|        - |  5730 | `				/* Const imports: emit a runtime instruction so imports are` |
|        - |  5731 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|       16 |  5732 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       16 |  5733 | `				if( zAliasDup ){` |
|        - |  5734 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|        - |  5735 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|        - |  5736 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|       16 |  5737 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|       16 |  5738 | `					if( azPair ){` |
|       16 |  5739 | `						azPair[0] = zAliasDup;` |
|       16 |  5740 | `						azPair[1] = zDup;` |
|       16 |  5741 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|        7 |  5742 | `					}` |
|        7 |  5743 | `				}` |
|        7 |  5744 | `			}` |
|       35 |  5745 | `		}` |
|        - |  5746 | `		/* Check for comma (multiple use declarations) */` |
|       75 |  5747 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        3 |  5748 | `			pGen->pIn++;` |
|        2 |  5749 | `		}else{` |
|       39 |  5750 | `			break;` |
|        - |  5751 | `		}` |
|        1 |  5752 | `	}` |
|       77 |  5753 | `	SyBlobRelease(&sPath);` |
|       77 |  5754 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        4 |  5755 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|        2 |  5756 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        3 |  5757 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5758 | `			return SXERR_ABORT;` |
|        - |  5759 | `		}` |
|        1 |  5760 | `	}` |
|       77 |  5761 | `	return SXRET_OK;` |
|       41 |  5762 | `}` |
|        - |  5763 | `/*` |
|        - |  5764 | ` * Compile the stupid 'declare' language construct.` |
|        - |  5765 | ` *` |
|        - |  5766 | ` * According to the PHP language reference manual.` |
|        - |  5767 | ` *  The declare construct is used to set execution directives for a block of code.` |
|        - |  5768 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|        - |  5769 | ` *  declare (directive)` |
|        - |  5770 | ` *   statement` |
|        - |  5771 | ` * The directive section allows the behavior of the declare block to be set.` |
|        - |  5772 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|        - |  5773 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|        - |  5774 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|        - |  5775 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|        - |  5776 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|        - |  5777 | ` * <?php` |
|        - |  5778 | ` * // these are the same:` |
|        - |  5779 | ` * // you can use this:` |
|        - |  5780 | ` * declare(ticks=1) {` |
|        - |  5781 | ` *   // entire script here` |
|        - |  5782 | ` * }` |
|        - |  5783 | ` * // or you can use this:` |
|        - |  5784 | ` * declare(ticks=1);` |
|        - |  5785 | ` * // entire script here` |
|        - |  5786 | ` * ?>` |
|        - |  5787 | ` *` |
|        - |  5788 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|        - |  5789 | ` */` |
|        - |  5790 | `/*` |
|        - |  5791 | ` * Match a directive name against a known literal (case-insensitive).` |
|        - |  5792 | ` */` |
|       72 |  5793 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|        5 |  5794 | `{` |
|      109 |  5795 | `	return SyStringLength(pName) == nWant` |
|       72 |  5796 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|        5 |  5797 | `}` |
|        - |  5798 |  |
|       42 |  5799 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|        5 |  5800 | `{` |
|       47 |  5801 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       47 |  5802 | `	SyToken *pBodyEnd = 0;` |
|        - |  5803 | `	SyToken *pBodyStart;` |
|        - |  5804 | `	SyToken *pCursor;` |
|        - |  5805 | `	int bHasStrictTypes;` |
|        - |  5806 | `	int bBlockForm;` |
|        - |  5807 | `	int bPlacementOk;` |
|        - |  5808 | `	sxi32 rc;` |
|       47 |  5809 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       47 |  5810 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|        6 |  5811 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|        6 |  5812 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5813 | `			return SXERR_ABORT;` |
|        - |  5814 | `		}` |
|        6 |  5815 | `		goto Synchro;` |
|        - |  5816 | `	}` |
|       43 |  5817 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       43 |  5818 | `	pBodyStart = pGen->pIn;` |
|        - |  5819 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|       43 |  5820 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|       43 |  5821 | `	if( pBodyEnd >= pGen->pEnd ){` |
|      ! 0 |  5822 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|      ! 0 |  5823 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5824 | `			return SXERR_ABORT;` |
|        - |  5825 | `		}` |
|      ! 0 |  5826 | `		return SXRET_OK;` |
|        - |  5827 | `	}` |
|        - |  5828 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|        - |  5829 | `	 * now delimits the comma-separated directive list. */` |
|       43 |  5830 | `	pGen->pIn = &pBodyEnd[1];` |
|       43 |  5831 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      ! 0 |  5832 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|      ! 0 |  5833 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5834 | `			return SXERR_ABORT;` |
|        - |  5835 | `		}` |
|      ! 0 |  5836 | `	}` |
|       43 |  5837 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|       43 |  5838 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|       43 |  5839 | `	bHasStrictTypes = 0;` |
|        - |  5840 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|        - |  5841 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|        - |  5842 | `	 * directive appears anywhere in the list, before validating values. */` |
|       43 |  5843 | `	pCursor = pBodyStart;` |
|       55 |  5844 | `	while( pCursor < pBodyEnd ){` |
|       51 |  5845 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       43 |  5846 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|       39 |  5847 | `				bHasStrictTypes = 1;` |
|       39 |  5848 | `				break;` |
|        - |  5849 | `			}` |
|        2 |  5850 | `		}` |
|       14 |  5851 | `		pCursor++;` |
|        2 |  5852 | `	}` |
|       43 |  5853 | `	if( bHasStrictTypes && bBlockForm ){` |
|        3 |  5854 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5855 | `			"strict_types declaration must not use block mode");` |
|        3 |  5856 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 |  5857 | `		return SXRET_OK;` |
|        - |  5858 | `	}` |
|       41 |  5859 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|        6 |  5860 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5861 | `			"strict_types declaration must be the very first statement in the script");` |
|        6 |  5862 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        6 |  5863 | `		return SXRET_OK;` |
|        - |  5864 | `	}` |
|        - |  5865 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|       37 |  5866 | `	pCursor = pBodyStart;` |
|       69 |  5867 | `	while( pCursor < pBodyEnd ){` |
|        - |  5868 | `		SyToken *pNameTok;` |
|        - |  5869 | `		SyToken *pEqTok;` |
|        - |  5870 | `		SyToken *pValTok;` |
|        - |  5871 | `		SyString *pDirName;` |
|        - |  5872 | `		int bIsStrict;` |
|        - |  5873 | `		int iStrictValue;` |
|       39 |  5874 | `		pNameTok = pCursor;` |
|       39 |  5875 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  5876 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5877 | `				"declare: Expecting a directive name");` |
|      ! 0 |  5878 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5879 | `			return SXRET_OK;` |
|        - |  5880 | `		}` |
|       39 |  5881 | `		pEqTok = pNameTok + 1;` |
|       39 |  5882 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|      ! 0 |  5883 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5884 | `				"declare: Expecting '=' after directive name");` |
|      ! 0 |  5885 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5886 | `			return SXRET_OK;` |
|        - |  5887 | `		}` |
|       39 |  5888 | `		pValTok = pEqTok + 1;` |
|       39 |  5889 | `		if( pValTok >= pBodyEnd ){` |
|      ! 0 |  5890 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5891 | `				"declare: Expecting value after '='");` |
|      ! 0 |  5892 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5893 | `			return SXRET_OK;` |
|        - |  5894 | `		}` |
|       39 |  5895 | `		pDirName = &pNameTok->sData;` |
|       39 |  5896 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|       39 |  5897 | `		if( bIsStrict ){` |
|        - |  5898 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|        - |  5899 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|       35 |  5900 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      ! 0 |  5901 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5902 | `					"declare(strict_types) value must be a literal");` |
|      ! 0 |  5903 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5904 | `				return SXRET_OK;` |
|        - |  5905 | `			}` |
|       35 |  5906 | `			iStrictValue = -1;` |
|       35 |  5907 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|       35 |  5908 | `				const char *zv = SyStringData(&pValTok->sData);` |
|       35 |  5909 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|       35 |  5910 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|       33 |  5911 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|       15 |  5912 | `			}` |
|       35 |  5913 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|        3 |  5914 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5915 | `					"strict_types declaration must have 0 or 1 as its value");` |
|        3 |  5916 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 |  5917 | `				return SXRET_OK;` |
|        - |  5918 | `			}` |
|       32 |  5919 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|       18 |  5920 | `		}else{` |
|        - |  5921 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|        - |  5922 | `			 * preserve the legacy notice so callers relying on the old` |
|        - |  5923 | `			 * behavior don't regress. */` |
|        8 |  5924 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|        - |  5925 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|        2 |  5926 | `				ph7_lib_version()` |
|        - |  5927 | `				);` |
|        - |  5928 | `		}` |
|       36 |  5929 | `		pCursor = pValTok + 1;` |
|        - |  5930 | `		/* Consume separating comma (or end). */` |
|       36 |  5931 | `		if( pCursor < pBodyEnd ){` |
|        3 |  5932 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 |  5933 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5934 | `					"declare: Expecting ',' or ')' after directive value");` |
|      ! 0 |  5935 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5936 | `				return SXRET_OK;` |
|        - |  5937 | `			}` |
|        3 |  5938 | `			pCursor++;` |
|        1 |  5939 | `		}` |
|        4 |  5940 | `	}` |
|        - |  5941 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|        - |  5942 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|        - |  5943 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|       34 |  5944 | `	return SXRET_OK;` |
|        2 |  5945 | `Synchro:` |
|        - |  5946 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|       16 |  5947 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       12 |  5948 | `		pGen->pIn++;` |
|        2 |  5949 | `	}` |
|        6 |  5950 | `	return SXRET_OK;` |
|       26 |  5951 | `}` |
|        - |  5952 | `/*` |
|        - |  5953 | ` * Process default argument values. That is,a function may define C++-style default value` |
|        - |  5954 | ` * as follows:` |
|        - |  5955 | ` * function makecoffee($type = "cappuccino")` |
|        - |  5956 | ` * {` |
|        - |  5957 | ` *   return "Making a cup of $type.\n";` |
|        - |  5958 | ` * }` |
|        - |  5959 | ` * Symisc eXtension.` |
|        - |  5960 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|        - |  5961 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|        - |  5962 | ` *      Example: Work only with PH7,generate error under zend` |
|        - |  5963 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|        - |  5964 | ` *      {` |
|        - |  5965 | ` *       var_dump($a);` |
|        - |  5966 | ` *      }` |
|        - |  5967 | ` *     //call test without args` |
|        - |  5968 | ` *      test();` |
|        - |  5969 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|        - |  5970 | ` *      Example:` |
|        - |  5971 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|        - |  5972 | ` * 3 -) Function overloading!!` |
|        - |  5973 | ` *      Example:` |
|        - |  5974 | ` *      function foo($a) {` |
|        - |  5975 | ` *   	  return $a.PHP_EOL;` |
|        - |  5976 | ` *	    }` |
|        - |  5977 | ` *	    function foo($a, $b) {` |
|        - |  5978 | ` *   	  return $a + $b;` |
|        - |  5979 | ` *	    }` |
|        - |  5980 | ` *	    echo foo(5); // Prints "5"` |
|        - |  5981 | ` *	    echo foo(5, 2); // Prints "7"` |
|        - |  5982 | ` *      // Same arg` |
|        - |  5983 | ` *	   function foo(string $a)` |
|        - |  5984 | ` *	   {` |
|        - |  5985 | ` *	     echo "a is a string\n";` |
|        - |  5986 | ` *	     var_dump($a);` |
|        - |  5987 | ` *	   }` |
|        - |  5988 | ` *	  function foo(int $a)` |
|        - |  5989 | ` *	  {` |
|        - |  5990 | ` *	    echo "a is integer\n";` |
|        - |  5991 | ` *	    var_dump($a);` |
|        - |  5992 | ` *	  }` |
|        - |  5993 | ` *	  function foo(array $a)` |
|        - |  5994 | ` *	  {` |
|        - |  5995 | ` * 	    echo "a is an array\n";` |
|        - |  5996 | ` * 	    var_dump($a);` |
|        - |  5997 | ` *	  }` |
|        - |  5998 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|        - |  5999 | ` *	  foo(52); // a is integer [second foo]` |
|        - |  6000 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|        - |  6001 | ` * Please refer to the official documentation for more information on the powerful extension` |
|        - |  6002 | ` * introduced by the PH7 engine.` |
|        - |  6003 | ` */` |
|   238422 |  6004 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|        5 |  6005 | `{` |
|        - |  6006 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |  6007 | `	SySet *pInstrContainer;` |
|        - |  6008 | `	sxi32 rc;` |
|        - |  6009 | `	/* Swap token stream */` |
|   238427 |  6010 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   238427 |  6011 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   238427 |  6012 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|        - |  6013 | `	/* Compile the expression holding the argument value */` |
|   238427 |  6014 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  6015 | `	/* Emit the done instruction */` |
|   238427 |  6016 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   238427 |  6017 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   238427 |  6018 | `	RE_SWAP_DELIMITER(pGen);` |
|   238427 |  6019 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  6020 | `		return SXERR_ABORT;` |
|        - |  6021 | `	}` |
|   238427 |  6022 | `	return SXRET_OK;` |
|   119216 |  6023 | `}` |
|        - |  6024 | `/*` |
|        - |  6025 | ` * Collect function arguments one after one.` |
|        - |  6026 | ` * According to the PHP language reference manual.` |
|        - |  6027 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|        - |  6028 | ` * list of expressions.` |
|        - |  6029 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|        - |  6030 | ` * and default argument values. Variable-length argument lists are also supported,` |
|        - |  6031 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|        - |  6032 | ` * for more information.` |
|        - |  6033 | ` * Example #1 Passing arrays to functions` |
|        - |  6034 | ` * <?php` |
|        - |  6035 | ` * function takes_array($input)` |
|        - |  6036 | ` * {` |
|        - |  6037 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|        - |  6038 | ` * }` |
|        - |  6039 | ` * ?>` |
|        - |  6040 | ` * Making arguments be passed by reference` |
|        - |  6041 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|        - |  6042 | ` * within the function is changed, it does not get changed outside of the function).` |
|        - |  6043 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|        - |  6044 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|        - |  6045 | ` * to the argument name in the function definition:` |
|        - |  6046 | ` * Example #2 Passing function parameters by reference` |
|        - |  6047 | ` * <?php` |
|        - |  6048 | ` * function add_some_extra(&$string)` |
|        - |  6049 | ` * {` |
|        - |  6050 | ` *   $string .= 'and something extra.';` |
|        - |  6051 | ` * }` |
|        - |  6052 | ` * $str = 'This is a string, ';` |
|        - |  6053 | ` * add_some_extra($str);` |
|        - |  6054 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|        - |  6055 | ` * ?>` |
|        - |  6056 | ` *` |
|        - |  6057 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|        - |  6058 | ` * complex agrument values.Please refer to the official documentation for more information` |
|        - |  6059 | ` * on these extension.` |
|        - |  6060 | ` */` |
|   485920 |  6061 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|        5 |  6062 | `{` |
|        - |  6063 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|        - |  6064 | `	SyToken *pIn;  /* Token stream */` |
|        - |  6065 | `	SyBlob sSig;         /* Function signature */` |
|        - |  6066 | `	char *zDup;          /* Copy of argument name */` |
|        - |  6067 | `	sxi32 rc;` |
|        - |  6068 |  |
|   485925 |  6069 | `	pIn = pGen->pIn;` |
|   485925 |  6070 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|        - |  6071 | `	/* Process arguments one after one */` |
|   597806 |  6072 | `	for(;;){` |
|  1195617 |  6073 | `		if( pIn >= pEnd ){` |
|        - |  6074 | `			/* No more arguments to process */` |
|   485909 |  6075 | `			break;` |
|        - |  6076 | `		}` |
|   709713 |  6077 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   709713 |  6078 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   709713 |  6079 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   709713 |  6080 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|   709713 |  6081 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|        - |  6082 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|        - |  6083 | `		 * first token inside the main token stream */` |
|   709713 |  6084 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  6085 | `			return SXERR_ABORT;` |
|        - |  6086 | `		}` |
|        - |  6087 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|        - |  6088 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|        - |  6089 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|        - |  6090 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|        - |  6091 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|        - |  6092 | `		{` |
|   709713 |  6093 | `			int bReadonly = 0, bVisSeen = 0;` |
|   709713 |  6094 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|   709713 |  6095 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|        3 |  6096 | `				bReadonly = 1;` |
|        3 |  6097 | `				pIn++;` |
|        1 |  6098 | `			}` |
|   709713 |  6099 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|    81053 |  6100 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|    81053 |  6101 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|       83 |  6102 | `					bVisSeen = 1;` |
|       83 |  6103 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|      111 |  6104 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|       36 |  6105 | `						: PH7_CLASS_PROT_PUBLIC;` |
|       83 |  6106 | `					pIn++;` |
|       83 |  6107 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       18 |  6108 | `						bReadonly = 1;` |
|       18 |  6109 | `						pIn++;` |
|        7 |  6110 | `					}` |
|       39 |  6111 | `				}` |
|    40524 |  6112 | `			}` |
|   709713 |  6113 | `			if( bVisSeen \|\| bReadonly ){` |
|       85 |  6114 | `				if( !bCtorCtx ){` |
|        6 |  6115 | `					if( bAbstractCtx ){` |
|        3 |  6116 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|        - |  6117 | `							"Cannot declare promoted property in an abstract constructor");` |
|        2 |  6118 | `					}else{` |
|        3 |  6119 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|        - |  6120 | `							"Cannot declare promoted property outside a constructor");` |
|        - |  6121 | `					}` |
|        6 |  6122 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  6123 | `						return SXERR_ABORT;` |
|        - |  6124 | `					}` |
|        6 |  6125 | `					return SXERR_SYNTAX;` |
|        - |  6126 | `				}` |
|       81 |  6127 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|       81 |  6128 | `				sArg.iPromoteVis = iVis;` |
|       81 |  6129 | `				if( bReadonly ){` |
|       20 |  6130 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|        8 |  6131 | `				}` |
|       38 |  6132 | `			}` |
|        - |  6133 | `		}` |
|        - |  6134 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|   709704 |  6135 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   414683 |  6136 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   117730 |  6137 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|    96535 |  6138 | `			sxu32 nLineLocal = pIn->nLine;` |
|    96535 |  6139 | `			sxi32 iTFlags = 0;` |
|    96535 |  6140 | `			pGen->pIn = pIn;` |
|    96535 |  6141 | `			rc = GenStateParseUnionTypeDecl(` |
|    48265 |  6142 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|    48265 |  6143 | `				&iTFlags, &sArg.sTypeName,` |
|        - |  6144 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|        - |  6145 | `				/* bAllowVoid */ 0,` |
|    48265 |  6146 | `						nLineLocal);` |
|    96535 |  6147 | `			pIn = pGen->pIn;` |
|    96535 |  6148 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  6149 | `				return SXERR_ABORT;` |
|    96535 |  6150 | `			}else if( rc == SXERR_CORRUPT ){` |
|        - |  6151 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|        3 |  6152 | `				return SXERR_SYNTAX;` |
|    96533 |  6153 | `			}else if( rc == SXERR_SYNTAX ){` |
|       11 |  6154 | `				if( pIn < pEnd ){` |
|       15 |  6155 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|        - |  6156 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|        4 |  6157 | `						&pIn->sData);` |
|        7 |  6158 | `				}else{` |
|      ! 0 |  6159 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|        - |  6160 | `						"syntax error, unexpected end of file");` |
|        - |  6161 | `				}` |
|       11 |  6162 | `				return SXERR_SYNTAX;` |
|        - |  6163 | `			}` |
|    96525 |  6164 | `			sArg.iFlags \|= iTFlags;` |
|    48260 |  6165 | `		}` |
|   709699 |  6166 | `		if( pIn >= pEnd ){` |
|      ! 0 |  6167 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|      ! 0 |  6168 | `			return rc;` |
|        - |  6169 | `		}` |
|   709699 |  6170 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|        - |  6171 | `			/* Pass by reference,record that */` |
|     3887 |  6172 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|     3887 |  6173 | `			pIn++;` |
|     1941 |  6174 | `		}` |
|   709699 |  6175 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|        - |  6176 | `			/* Variadic parameter: ...$args */` |
|    19283 |  6177 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|    19283 |  6178 | `			pIn++;` |
|     9639 |  6179 | `		}` |
|   709699 |  6180 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  6181 | `			/* Invalid argument */` |
|      ! 0 |  6182 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|      ! 0 |  6183 | `			return rc;` |
|        - |  6184 | `		}` |
|   709699 |  6185 | `		pIn++; /* Jump the dollar sign */` |
|        - |  6186 | `		/* Copy argument name */` |
|   709699 |  6187 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   709699 |  6188 | `		if( zDup == 0 ){` |
|      ! 0 |  6189 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  6190 | `			return SXERR_ABORT;` |
|        - |  6191 | `		}` |
|   709699 |  6192 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   709699 |  6193 | `		pIn++;` |
|   709699 |  6194 | `		if( pIn < pEnd ){` |
|   369941 |  6195 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|        - |  6196 | `				SyToken *pDefend;` |
|   238429 |  6197 | `				sxi32 iNest = 0;` |
|   238429 |  6198 | `				pIn++; /* Jump the equal sign */` |
|   238429 |  6199 | `				pDefend = pIn;` |
|        - |  6200 | `				/* Process the default value associated with this argument */` |
|   507627 |  6201 | `				while( pDefend < pEnd ){` |
|   361485 |  6202 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|    92287 |  6203 | `						break;` |
|        - |  6204 | `					}` |
|   269203 |  6205 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|        - |  6206 | `						/* Increment nesting level */` |
|    15389 |  6207 | `						iNest++;` |
|   261511 |  6208 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|        - |  6209 | `						/* Decrement nesting level */` |
|    15389 |  6210 | `						iNest--;` |
|     7692 |  6211 | `					}` |
|   269203 |  6212 | `					pDefend++;` |
|        5 |  6213 | `				}` |
|   238429 |  6214 | `				if( pIn >= pDefend ){` |
|        3 |  6215 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|        3 |  6216 | `					return rc;` |
|        - |  6217 | `				}` |
|        - |  6218 | `				/* Process default value */` |
|   238427 |  6219 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   238427 |  6220 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  6221 | `					return rc;` |
|        - |  6222 | `				}` |
|        - |  6223 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|        - |  6224 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|        - |  6225 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|        - |  6226 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|        - |  6227 | `				 * arg-type check lets null through. */` |
|   238422 |  6228 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|   144213 |  6229 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|   144211 |  6230 | `					&& &pIn[1] == pDefend` |
|    46156 |  6231 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|    34613 |  6232 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|    21153 |  6233 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|    15387 |  6234 | `					sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|     7691 |  6235 | `				}` |
|        - |  6236 | `				/* Point beyond the default value */` |
|   238427 |  6237 | `				pIn = pDefend;` |
|   119211 |  6238 | `			}` |
|   369939 |  6239 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 |  6240 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|      ! 0 |  6241 | `				return rc;` |
|        - |  6242 | `			}` |
|   369939 |  6243 | `			pIn++; /* Jump the trailing comma */` |
|   184967 |  6244 | `		}` |
|        - |  6245 | `		/* Append argument signature */` |
|   709697 |  6246 | `		if( sArg.nType > 0 ){` |
|    96463 |  6247 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|        - |  6248 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    15453 |  6249 | `				int marker = 'o';` |
|    15453 |  6250 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    15453 |  6251 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|     7729 |  6252 | `			}else{` |
|        - |  6253 | `				int c;` |
|    81015 |  6254 | `				c = 'n'; /* cc warning */` |
|        - |  6255 | `				/* Type leading character */` |
|    81015 |  6256 | `				switch(sArg.nType){` |
|     5771 |  6257 | `				case MEMOBJ_HASHMAP:` |
|        - |  6258 | `					/* Hashmap aka 'array' */` |
|    11547 |  6259 | `					c = 'h';` |
|    11547 |  6260 | `					break;` |
|     9705 |  6261 | `				case MEMOBJ_INT:` |
|        - |  6262 | `					/* Integer */` |
|    19415 |  6263 | `					c = 'i';` |
|    19415 |  6264 | `					break;` |
|        2 |  6265 | `				case MEMOBJ_BOOL:` |
|        - |  6266 | `					/* Bool */` |
|        5 |  6267 | `					c = 'b';` |
|        5 |  6268 | `					break;` |
|        5 |  6269 | `				case MEMOBJ_REAL:` |
|        - |  6270 | `					/* Float */` |
|       12 |  6271 | `					c = 'f';` |
|       12 |  6272 | `					break;` |
|    25014 |  6273 | `				case MEMOBJ_STRING:` |
|        - |  6274 | `					/* String */` |
|    50033 |  6275 | `					c = 's';` |
|    50033 |  6276 | `					break;` |
|        7 |  6277 | `				case MEMOBJ_OBJ:` |
|        - |  6278 | `					/* Object */` |
|       16 |  6279 | `					c = 'o';` |
|       14 |  6280 | `					break;` |
|        1 |  6281 | `				default:` |
|        2 |  6282 | `					break;` |
|        - |  6283 | `				}` |
|    81015 |  6284 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|        - |  6285 | `			}` |
|    48234 |  6286 | `		}else{` |
|        - |  6287 | `			/* No type is associated with this parameter which mean` |
|        - |  6288 | `			 * that this function is not condidate for overloading.` |
|        - |  6289 | `			 */` |
|   613239 |  6290 | `			SyBlobRelease(&sSig);` |
|        - |  6291 | `		}` |
|        - |  6292 | `		/* Save in the argument set */` |
|   709697 |  6293 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|        5 |  6294 | `	}` |
|   485909 |  6295 | `	if( SyBlobLength(&sSig) > 0 ){` |
|        - |  6296 | `		/* Save function signature */` |
|    65643 |  6297 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|    32819 |  6298 | `	}` |
|   485909 |  6299 | `	return SXRET_OK;` |
|   242965 |  6300 | `}` |
|        - |  6301 | `/*` |
|        - |  6302 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|        - |  6303 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|        - |  6304 | ` * the enclosing function. Returns the token just past the nested construct.` |
|        - |  6305 | ` */` |
|    34616 |  6306 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|        5 |  6307 | `{` |
|    34621 |  6308 | `	sxi32 iParen = 0;` |
|    34621 |  6309 | `	pIn++; /* past 'function'/'fn' */` |
|        - |  6310 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|        - |  6311 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|        - |  6312 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|   153845 |  6313 | `	while( pIn < pEnd ){` |
|   153845 |  6314 | `		sxu32 t = pIn->nType;` |
|   153845 |  6315 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|   149981 |  6316 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|   103833 |  6317 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|    84593 |  6318 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|   119229 |  6319 | `		pIn++;` |
|        5 |  6320 | `	}` |
|    19245 |  6321 | `	if( pIn >= pEnd ){ return pIn; }` |
|        - |  6322 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|        - |  6323 | `	{` |
|    19245 |  6324 | `		sxi32 d = 0;` |
|   765169 |  6325 | `		while( pIn < pEnd ){` |
|   765169 |  6326 | `			sxu32 t = pIn->nType;` |
|   765169 |  6327 | `			if( t & PH7_TK_OCB ){ d++; }` |
|   734393 |  6328 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|   745929 |  6329 | `			pIn++;` |
|        5 |  6330 | `		}` |
|        - |  6331 | `	}` |
|    19245 |  6332 | `	return pIn;` |
|    17313 |  6333 | `}` |
|        - |  6334 | `/*` |
|        - |  6335 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|        - |  6336 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|        - |  6337 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|        - |  6338 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|        - |  6339 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|        - |  6340 | ` * detached-mini-program path untouched.` |
|        - |  6341 | ` */` |
|        - |  6342 | `/*` |
|        - |  6343 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|        - |  6344 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|        - |  6345 | ` * mixed, object.` |
|        - |  6346 | ` */` |
|       28 |  6347 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|        3 |  6348 | `{` |
|        - |  6349 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|        - |  6350 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|        - |  6351 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|        - |  6352 | `	};` |
|        - |  6353 | `	sxu32 i;` |
|       31 |  6354 | `	if( nName > 0 && zName[0] == '\\' ){` |
|      ! 0 |  6355 | `		zName++;` |
|      ! 0 |  6356 | `		nName--;` |
|      ! 0 |  6357 | `	}` |
|       63 |  6358 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|       59 |  6359 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|       27 |  6360 | `			return 1;` |
|        - |  6361 | `		}` |
|       17 |  6362 | `	}` |
|        5 |  6363 | `	return 0;` |
|       17 |  6364 | `}` |
|        - |  6365 | `/*` |
|        - |  6366 | ` * One atom of a generator's declared return type: is it a supertype of` |
|        - |  6367 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|        - |  6368 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|        - |  6369 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|        - |  6370 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|        - |  6371 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|        - |  6372 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|        - |  6373 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|        - |  6374 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|        - |  6375 | ` * both rather than fatal on valid code (divergence recorded in PLAN.md).` |
|        - |  6376 | ` */` |
|       26 |  6377 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|        4 |  6378 | `{` |
|       30 |  6379 | `	if( nType == MEMOBJ_OBJ ){` |
|      ! 0 |  6380 | ``		return 1; /* bare `object` */`` |
|        - |  6381 | `	}` |
|       30 |  6382 | `	if( nType != SXU32_HIGH ){` |
|        3 |  6383 | `		return 0; /* scalar/array/void/never/null/... */` |
|        - |  6384 | `	}` |
|       27 |  6385 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|       23 |  6386 | `		return 1;` |
|        - |  6387 | `	}` |
|        - |  6388 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|        - |  6389 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|        - |  6390 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|        - |  6391 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|        - |  6392 | `	{` |
|        - |  6393 | `		SyBlob sFQN;` |
|        - |  6394 | `		int bOk;` |
|        5 |  6395 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        5 |  6396 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|        5 |  6397 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|        5 |  6398 | `		SyBlobRelease(&sFQN);` |
|        5 |  6399 | `		return bOk;` |
|        - |  6400 | `	}` |
|       17 |  6401 | `}` |
|        - |  6402 | `/*` |
|        - |  6403 | ` * php 8: a generator function may only declare a return type that is a` |
|        - |  6404 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|        - |  6405 | ` * group qualifies only if every member does. Anything else is php's exact` |
|        - |  6406 | ` * compile-time fatal "Generator return type must be a supertype of` |
|        - |  6407 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|        - |  6408 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|        - |  6409 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|        - |  6410 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|        - |  6411 | ` */` |
|      220 |  6412 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|        5 |  6413 | `{` |
|      225 |  6414 | `	int bOk = 0;` |
|        - |  6415 | `	sxu32 nLine;` |
|        - |  6416 | `	sxi32 rc;` |
|      225 |  6417 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|      199 |  6418 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|        - |  6419 | `	}` |
|       30 |  6420 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|      ! 0 |  6421 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  6422 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|        - |  6423 | `		sxu32 i,j;` |
|      ! 0 |  6424 | `		for( i = 0; i < n && !bOk; i++ ){` |
|        - |  6425 | `			int bGroupOk;` |
|      ! 0 |  6426 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|      ! 0 |  6427 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|        - |  6428 | `			}` |
|      ! 0 |  6429 | `			bGroupOk = 1;` |
|      ! 0 |  6430 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|      ! 0 |  6431 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|      ! 0 |  6432 | `					bGroupOk = 0;` |
|      ! 0 |  6433 | `					break;` |
|        - |  6434 | `				}` |
|      ! 0 |  6435 | `			}` |
|      ! 0 |  6436 | `			bOk = bGroupOk;` |
|      ! 0 |  6437 | `		}` |
|      ! 0 |  6438 | `	}else{` |
|       30 |  6439 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|        - |  6440 | `	}` |
|       30 |  6441 | `	if( bOk ){` |
|       27 |  6442 | `		return SXRET_OK;` |
|        - |  6443 | `	}` |
|        - |  6444 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|        - |  6445 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|        - |  6446 | `	 * token of this stream — its line is the function's closing brace. php` |
|        - |  6447 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|        - |  6448 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|        3 |  6449 | `	nLine = pGen->pIn[-1].nLine;` |
|        - |  6450 | `	{` |
|        3 |  6451 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|        3 |  6452 | `		if( sGiven.nByte < 1 ){` |
|      ! 0 |  6453 | `			sGiven = pFunc->sReturnClass;` |
|      ! 0 |  6454 | `		}` |
|        3 |  6455 | `		if( sGiven.nByte < 1 ){` |
|        - |  6456 | ``			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the`` |
|        - |  6457 | `			 * rendered type text, so sReturnTypeName arrives empty for them —` |
|        - |  6458 | `			 * name them here (the root fix belongs to that renderer, §3.7). */` |
|      ! 0 |  6459 | `			const char *zScalar =` |
|      ! 0 |  6460 | `				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :` |
|      ! 0 |  6461 | `				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";` |
|      ! 0 |  6462 | `			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));` |
|      ! 0 |  6463 | `		}` |
|        3 |  6464 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  6465 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|        - |  6466 | `	}` |
|        3 |  6467 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|      115 |  6468 | `}` |
|  1390570 |  6469 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|        5 |  6470 | `{` |
|  1390575 |  6471 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|  1390575 |  6472 | `	SyToken *pEnd = pGen->pEnd;` |
|  1390575 |  6473 | `	sxi32 iDepth = 0;` |
|  1390575 |  6474 | `	int bStarted = 0;` |
| 61172457 |  6475 | `	while( pIn < pEnd ){` |
| 61172457 |  6476 | `		sxu32 t = pIn->nType;` |
| 61172457 |  6477 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 58262583 |  6478 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 55353033 |  6479 | `		if( t & PH7_TK_KEYWORD ){` |
|  4528971 |  6480 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|  4528971 |  6481 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|  4528751 |  6482 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|        - |  6483 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|  2247065 |  6484 | `		}` |
| 55318197 |  6485 | `		pIn++;` |
|        5 |  6486 | `	}` |
|  1390355 |  6487 | `	return FALSE;` |
|   695290 |  6488 | `}` |
|        - |  6489 | `/*` |
|        - |  6490 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|        - |  6491 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|        - |  6492 | ` * and this routine takes care of generating the appropriate error message.` |
|        - |  6493 | ` */` |
|  1390570 |  6494 | `static sxi32 GenStateCompileFuncBody(` |
|        - |  6495 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - |  6496 | `	ph7_vm_func *pFunc    /* Function state */` |
|        - |  6497 | `	)` |
|        5 |  6498 | `{` |
|        - |  6499 | `	SySet *pInstrContainer; /* Instruction container */` |
|        - |  6500 | `	GenBlock *pBlock;` |
|        - |  6501 | `	sxu32 nGotoOfft;` |
|        - |  6502 | `	sxi32 rc;` |
|        - |  6503 | `	/* Attach the new function */` |
|  1390575 |  6504 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  1390575 |  6505 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  6506 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|        - |  6507 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  6508 | `		return SXERR_ABORT;` |
|        - |  6509 | `	}` |
|  1390575 |  6510 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|        - |  6511 | `	/* Swap bytecode containers */` |
|  1390575 |  6512 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  1390575 |  6513 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|        - |  6514 | `	/* Emit constructor property promotion prologue:` |
|        - |  6515 | `	 *   $this->NAME = $NAME;` |
|        - |  6516 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|        - |  6517 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|        - |  6518 | `	{` |
|  1390575 |  6519 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|        - |  6520 | `		sxu32 i;` |
|  2069365 |  6521 | `		for( i = 0; i < nArg; i++ ){` |
|   678795 |  6522 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|        - |  6523 | `			char *zSrc;` |
|        - |  6524 | `			sxu32 nSrc,nName;` |
|        - |  6525 | `			SySet sToken;` |
|        - |  6526 | `			SyToken *pTmpIn,*pTmpEnd;` |
|        - |  6527 | `			sxi32 rcPromote;` |
|   678795 |  6528 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   678729 |  6529 | `				continue;` |
|        - |  6530 | `			}` |
|        - |  6531 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|        - |  6532 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|        - |  6533 | `			 * copied), so it must outlive the function — never free it. The` |
|        - |  6534 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|        - |  6535 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|       71 |  6536 | `			nName = SyStringLength(&pArg->sName);` |
|       71 |  6537 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|       71 |  6538 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|       71 |  6539 | `			if( zSrc == 0 ){` |
|      ! 0 |  6540 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  6541 | `				GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  6542 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  6543 | `				return SXERR_ABORT;` |
|        - |  6544 | `			}` |
|        - |  6545 | `			{` |
|       71 |  6546 | `				char *z = zSrc;` |
|       71 |  6547 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|       71 |  6548 | `				z += sizeof("$this->")-1;` |
|       71 |  6549 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|       71 |  6550 | `				z += nName;` |
|       71 |  6551 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|       71 |  6552 | `				z += sizeof(" = $")-1;` |
|       71 |  6553 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|       71 |  6554 | `				z += nName;` |
|       71 |  6555 | `				*z = 0;` |
|        - |  6556 | `			}` |
|       71 |  6557 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       71 |  6558 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);` |
|       71 |  6559 | `			pTmpIn = pGen->pIn;` |
|       71 |  6560 | `			pTmpEnd = pGen->pEnd;` |
|       71 |  6561 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|       71 |  6562 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       71 |  6563 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|       71 |  6564 | `			pGen->pIn = pTmpIn;` |
|       71 |  6565 | `			pGen->pEnd = pTmpEnd;` |
|       71 |  6566 | `			SySetRelease(&sToken);` |
|       71 |  6567 | `			if( rcPromote == SXERR_ABORT ){` |
|      ! 0 |  6568 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  6569 | `				GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  6570 | `				return SXERR_ABORT;` |
|        - |  6571 | `			}` |
|        - |  6572 | `			/* Discard the assignment result — this is a statement expression. */` |
|       71 |  6573 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       38 |  6574 | `		}` |
|        - |  6575 | `	}` |
|        - |  6576 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|        - |  6577 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|        - |  6578 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|        - |  6579 | `	 * generator — and vice versa — is classified independently. */` |
|        - |  6580 | `	{` |
|  1390575 |  6581 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|  1390575 |  6582 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|        - |  6583 | `		/* Compile the body */` |
|  1390575 |  6584 | `		PH7_CompileBlock(&(*pGen),0);` |
|  1390575 |  6585 | `		pGen->bInGenerator = bSavedGen;` |
|        - |  6586 | `	}` |
|        - |  6587 | `	/* Fix exception jumps now the destination is resolved */` |
|  1390575 |  6588 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        - |  6589 | `	/* Emit the final return if not yet done */` |
|  1390575 |  6590 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - |  6591 | `	/* Fix gotos jumps now the destination is resolved */` |
|  1390575 |  6592 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|      ! 0 |  6593 | `		rc = SXERR_ABORT;` |
|      ! 0 |  6594 | `	}` |
|  1390575 |  6595 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|        - |  6596 | `	/* Restore the default container */` |
|  1390575 |  6597 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - |  6598 | `	/* Leave function block */` |
|  1390575 |  6599 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  1390575 |  6600 | `	if( rc == SXERR_ABORT ){` |
|        - |  6601 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  6602 | `		return SXERR_ABORT;` |
|        - |  6603 | `	}` |
|        - |  6604 | `	/* Scan for yield opcodes to detect generator functions */` |
|        - |  6605 | `	{` |
|  1390575 |  6606 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|        - |  6607 | `		sxu32 i;` |
| 37208553 |  6608 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 35818203 |  6609 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|      225 |  6610 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      225 |  6611 | `				break;` |
|        - |  6612 | `			}` |
| 17908994 |  6613 | `		}` |
|        - |  6614 | `	}` |
|  1390575 |  6615 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  6616 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|      225 |  6617 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|      ! 0 |  6618 | `			return SXERR_ABORT;` |
|        - |  6619 | `		}` |
|      110 |  6620 | `	}` |
|        - |  6621 | `	/* All done, function body compiled */` |
|  1390575 |  6622 | `	return SXRET_OK;` |
|   695290 |  6623 | `}` |
|        - |  6624 | `/*` |
|        - |  6625 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|        - |  6626 | ` * According to the PHP language reference manual.` |
|        - |  6627 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|        - |  6628 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|        - |  6629 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|        - |  6630 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|        - |  6631 | ` *  Functions need not be defined before they are referenced.` |
|        - |  6632 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|        - |  6633 | ` *  a function even if they were defined inside and vice versa.` |
|        - |  6634 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|        - |  6635 | ` *  calls with over 32-64 recursion levels.` |
|        - |  6636 | ` *` |
|        - |  6637 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|        - |  6638 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|        - |  6639 | ` * on these extension.` |
|        - |  6640 | ` */` |
|        - |  6641 | `/*` |
|        - |  6642 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|        - |  6643 | ` */` |
|      554 |  6644 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|        5 |  6645 | `{` |
|        - |  6646 | `	sxu32 i;` |
|     1571 |  6647 | `	for( i = 0; i < n; i++ ){` |
|     1347 |  6648 | `		int a = zA[i], b = zB[i];` |
|     1347 |  6649 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|     1347 |  6650 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|     1347 |  6651 | `		if( a != b ) return a - b;` |
|      511 |  6652 | `	}` |
|      229 |  6653 | `	return 0;` |
|      282 |  6654 | `}` |
|        - |  6655 | `/*` |
|        - |  6656 | ` * Internal type-atom kinds used during union type parsing.` |
|        - |  6657 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|        - |  6658 | ` * (which are positive bit values stored in sxu32).` |
|        - |  6659 | ` */` |
|        - |  6660 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|        - |  6661 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|        - |  6662 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|        - |  6663 |  |
|        - |  6664 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|        - |  6665 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|        - |  6666 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|        - |  6667 |  |
|        - |  6668 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|        - |  6669 | `struct PhlTypeAtom {` |
|        - |  6670 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|        - |  6671 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|        - |  6672 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|        - |  6673 | `	sxu32 nCanon;` |
|        - |  6674 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|        - |  6675 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|        - |  6676 | `};` |
|        - |  6677 |  |
|        - |  6678 | `/*` |
|        - |  6679 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|        - |  6680 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|        - |  6681 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|        - |  6682 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|        - |  6683 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|        - |  6684 | ` * already be consumed by the caller.` |
|        - |  6685 | ` */` |
|    97542 |  6686 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|        5 |  6687 | `{` |
|    97547 |  6688 | `	SyToken *pIn = pGen->pIn;` |
|    97547 |  6689 | `	SyZero(pOut, sizeof(*pOut));` |
|    97547 |  6690 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|    97547 |  6691 | `	if( pIn >= pGen->pEnd ){` |
|      ! 0 |  6692 | `		return SXERR_SYNTAX;` |
|        - |  6693 | `	}` |
|        - |  6694 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|    97547 |  6695 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|        8 |  6696 | `		pIn++;` |
|        8 |  6697 | `		if( pIn >= pGen->pEnd ){` |
|      ! 0 |  6698 | `			return SXERR_SYNTAX;` |
|        - |  6699 | `		}` |
|        3 |  6700 | `	}` |
|    97547 |  6701 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  6702 | `		return SXERR_SYNTAX;` |
|        - |  6703 | `	}` |
|    97547 |  6704 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|    81651 |  6705 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|    81651 |  6706 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|    11571 |  6707 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|    75868 |  6708 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|       79 |  6709 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|    70048 |  6710 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|    19713 |  6711 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|    60157 |  6712 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|    50221 |  6713 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|    25195 |  6714 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|       41 |  6715 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|       68 |  6716 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|       27 |  6717 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|       37 |  6718 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       14 |  6719 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|       23 |  6720 | `			pOut->nType = SXU32_HIGH;` |
|       23 |  6721 | `			pOut->sClass = pIn->sData;` |
|       13 |  6722 | `		}else{` |
|        3 |  6723 | `			return SXERR_SYNTAX;` |
|        - |  6724 | `		}` |
|    81649 |  6725 | `		pIn++;` |
|    40827 |  6726 | `	}else{` |
|        - |  6727 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|        - |  6728 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|    15901 |  6729 | `		SyString *pT = &pIn->sData;` |
|    15901 |  6730 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|       34 |  6731 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|       34 |  6732 | `			pIn++;` |
|    15886 |  6733 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|      171 |  6734 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|      171 |  6735 | `			pIn++;` |
|    15788 |  6736 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       26 |  6737 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       26 |  6738 | `			pIn++;` |
|       15 |  6739 | `		}else{` |
|        - |  6740 | `			/* Class / interface name; consume namespace path a\b\c */` |
|    15683 |  6741 | `			SyToken *pFirst = pIn;` |
|    15683 |  6742 | `			SyToken *pLast = pIn;` |
|    15683 |  6743 | `			pOut->nType = SXU32_HIGH;` |
|    15683 |  6744 | `			pOut->sClass = pIn->sData;` |
|    15683 |  6745 | `			pIn++;` |
|    23520 |  6746 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|    15686 |  6747 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|        3 |  6748 | `				pLast = &pIn[1];` |
|        3 |  6749 | `				pIn += 2;` |
|        1 |  6750 | `			}` |
|    15683 |  6751 | `			if( pLast != pFirst ){` |
|        3 |  6752 | `				const char *zFirst = pFirst->sData.zString;` |
|        3 |  6753 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|        3 |  6754 | `				pOut->sClass.zString = zFirst;` |
|        3 |  6755 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|        1 |  6756 | `			}` |
|        - |  6757 | `		}` |
|        - |  6758 | `	}` |
|    97545 |  6759 | `	pGen->pIn = pIn;` |
|    97545 |  6760 | `	return SXRET_OK;` |
|    48776 |  6761 | `}` |
|        - |  6762 |  |
|        - |  6763 | `/*` |
|        - |  6764 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|        - |  6765 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|        - |  6766 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|        - |  6767 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|        - |  6768 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|        - |  6769 | ` */` |
|    97364 |  6770 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|        5 |  6771 | `{` |
|        - |  6772 | `	int i;` |
|    97369 |  6773 | `	int nNonNull = 0;` |
|    97369 |  6774 | `	int bAnyIntersection = 0;` |
|        - |  6775 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|    97369 |  6776 | `	sxu32 nMaxGroup = 0;` |
|  3213017 |  6777 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|   194885 |  6778 | `	for( i = 0; i < nAtoms; i++ ){` |
|    97521 |  6779 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    97491 |  6780 | `			nNonNull++;` |
|    97491 |  6781 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|    97491 |  6782 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|    97491 |  6783 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|    48743 |  6784 | `			}` |
|    48743 |  6785 | `		}` |
|    48763 |  6786 | `	}` |
|   194833 |  6787 | `	for( i = 0; i < nAtoms; i++ ){` |
|    97493 |  6788 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|       29 |  6789 | `			bAnyIntersection = 1;` |
|       29 |  6790 | `			break;` |
|        - |  6791 | `		}` |
|    48737 |  6792 | `	}` |
|    97369 |  6793 | `	if( bAnyIntersection ){` |
|        - |  6794 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|        - |  6795 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|        - |  6796 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|       29 |  6797 | `		sxu32 g, nGroups = 0;` |
|       29 |  6798 | `		int bFirstGroup = 1;` |
|       59 |  6799 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|       59 |  6800 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|       35 |  6801 | `			int bFirstMember = 1;` |
|        - |  6802 | `			int bWrap;` |
|       35 |  6803 | `			if( aGroupCount[g] == 0 ) continue;` |
|        - |  6804 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|        - |  6805 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|        - |  6806 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|        - |  6807 | `			 * parens, matching PHP's canonical text. */` |
|       47 |  6808 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|       35 |  6809 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|       35 |  6810 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|      107 |  6811 | `			for( i = 0; i < nAtoms; i++ ){` |
|       77 |  6812 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|       59 |  6813 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|       59 |  6814 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       55 |  6815 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       30 |  6816 | `				}else{` |
|        6 |  6817 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|        - |  6818 | `				}` |
|       59 |  6819 | `				bFirstMember = 0;` |
|       32 |  6820 | `			}` |
|       35 |  6821 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|       35 |  6822 | `			bFirstGroup = 0;` |
|       20 |  6823 | `		}` |
|       29 |  6824 | `		if( bNullable ){` |
|      ! 0 |  6825 | `			SyBlobAppend(pBlob, "\|", 1);` |
|      ! 0 |  6826 | `			SyBlobAppend(pBlob, "null", 4);` |
|      ! 0 |  6827 | `		}` |
|       77 |  6828 | `		return;` |
|        - |  6829 | `	}` |
|    97345 |  6830 | `	if( nNonNull == 1 && bNullable ){` |
|        - |  6831 | `		/* Shorthand: ?T */` |
|      100 |  6832 | `		for( i = 0; i < nAtoms; i++ ){` |
|      100 |  6833 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      100 |  6834 | `			SyBlobAppend(pBlob, "?", 1);` |
|      100 |  6835 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|       24 |  6836 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       14 |  6837 | `			}else{` |
|       80 |  6838 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|        - |  6839 | `			}` |
|      100 |  6840 | `			return;` |
|      ! 0 |  6841 | `		}` |
|      ! 0 |  6842 | `	}` |
|        - |  6843 | `	{` |
|    97249 |  6844 | `		int bFirst = 1;` |
|        - |  6845 | `		/* 1) Classes in declaration order */` |
|   194601 |  6846 | `		for( i = 0; i < nAtoms; i++ ){` |
|    97357 |  6847 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|    15633 |  6848 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    15633 |  6849 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|    15633 |  6850 | `				bFirst = 0;` |
|     7814 |  6851 | `			}` |
|    48681 |  6852 | `		}` |
|        - |  6853 | `		/* 2) Built-ins in canonical order */` |
|        - |  6854 | `		{` |
|        - |  6855 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|        - |  6856 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|        - |  6857 | `			int k;` |
|   680713 |  6858 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  1085941 |  6859 | `				for( i = 0; i < nAtoms; i++ ){` |
|   584005 |  6860 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|    81533 |  6861 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    81533 |  6862 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|    81533 |  6863 | `						bFirst = 0;` |
|    81533 |  6864 | `						break;` |
|        - |  6865 | `					}` |
|   251241 |  6866 | `				}` |
|   291737 |  6867 | `			}` |
|        - |  6868 | `		}` |
|        - |  6869 | `		/* 3) null suffix */` |
|    97249 |  6870 | `		if( bNullable ){` |
|       19 |  6871 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|       19 |  6872 | `			SyBlobAppend(pBlob, "null", 4);` |
|        8 |  6873 | `		}` |
|        - |  6874 | `	}` |
|    48687 |  6875 | `}` |
|        - |  6876 |  |
|        - |  6877 | `/*` |
|        - |  6878 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|        - |  6879 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|        - |  6880 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|        - |  6881 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|        - |  6882 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|        - |  6883 | ` * whether it was parenthesized.` |
|        - |  6884 | ` *` |
|        - |  6885 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|        - |  6886 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|        - |  6887 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|        - |  6888 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|        - |  6889 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|        - |  6890 | ` */` |
|    97516 |  6891 | `static sxi32 GenStateParsePart(` |
|        - |  6892 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|        - |  6893 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|        5 |  6894 | `{` |
|        - |  6895 | `	sxi32 rc;` |
|    97521 |  6896 | `	int nMembers = 0;` |
|    97521 |  6897 | `	int bParen = 0;` |
|    97521 |  6898 | `	*pnMembers = 0;` |
|    97521 |  6899 | `	*pbParen = 0;` |
|    97521 |  6900 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        9 |  6901 | `		bParen = 1;` |
|        9 |  6902 | `		pGen->pIn++; /* skip '(' */` |
|        3 |  6903 | `	}` |
|    48758 |  6904 | `	for(;;){` |
|    97547 |  6905 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|      ! 0 |  6906 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  6907 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|      ! 0 |  6908 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  6909 | `		}` |
|    97547 |  6910 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|    97547 |  6911 | `		if( rc != SXRET_OK ){` |
|        3 |  6912 | `			return rc;` |
|        - |  6913 | `		}` |
|    97545 |  6914 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|    97545 |  6915 | `		(*pnAtoms)++;` |
|    97545 |  6916 | `		nMembers++;` |
|        - |  6917 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|    97545 |  6918 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       39 |  6919 | `			SyToken *pNext = &pGen->pIn[1];` |
|       34 |  6920 | `			if( pNext < pGen->pEnd` |
|       39 |  6921 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       31 |  6922 | `				pGen->pIn++; /* skip '&' */` |
|       31 |  6923 | `				continue;` |
|        - |  6924 | `			}` |
|        4 |  6925 | `		}` |
|    97519 |  6926 | `		break;` |
|      ! 0 |  6927 | `	}` |
|    97519 |  6928 | `	if( bParen ){` |
|        9 |  6929 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 |  6930 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  6931 | `				"Malformed DNF type: expecting ')'");` |
|      ! 0 |  6932 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  6933 | `		}` |
|        9 |  6934 | `		pGen->pIn++; /* skip ')' */` |
|        9 |  6935 | `		if( nMembers < 2 ){` |
|      ! 0 |  6936 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  6937 | `				"Parenthesized type must be an intersection of at least two types");` |
|      ! 0 |  6938 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  6939 | `		}` |
|        3 |  6940 | `	}` |
|    97519 |  6941 | `	*pnMembers = nMembers;` |
|    97519 |  6942 | `	*pbParen = bParen;` |
|    97519 |  6943 | `	return SXRET_OK;` |
|    48763 |  6944 | `}` |
|        - |  6945 |  |
|        - |  6946 | `/*` |
|        - |  6947 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|        - |  6948 | ` *` |
|        - |  6949 | ` * Outputs:` |
|        - |  6950 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|        - |  6951 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|        - |  6952 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|        - |  6953 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|        - |  6954 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|        - |  6955 | ` *     already be initialized by the caller (allocator set, etc).` |
|        - |  6956 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|        - |  6957 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|        - |  6958 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|        - |  6959 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|        - |  6960 | ` *` |
|        - |  6961 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|        - |  6962 | ` * SXERR_ABORT on fatal compile errors.` |
|        - |  6963 | ` */` |
|    97380 |  6964 | `static sxi32 GenStateParseUnionTypeDecl(` |
|        - |  6965 | `	ph7_gen_state *pGen,` |
|        - |  6966 | `	sxu32 *pnType,` |
|        - |  6967 | `	SyString *pClass,` |
|        - |  6968 | `	SySet *pAlts,` |
|        - |  6969 | `	sxi32 *piTypeFlags,` |
|        - |  6970 | `	SyString *pTypeText,` |
|        - |  6971 | `	int iNullableFlag,` |
|        - |  6972 | `	int iUnionFlag,` |
|        - |  6973 | `	int bAllowVoid,` |
|        - |  6974 | `	sxu32 nLine` |
|        5 |  6975 | `){` |
|        - |  6976 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|    97385 |  6977 | `	int nAtoms = 0;` |
|    97385 |  6978 | `	int bShortNullable = 0;` |
|    97385 |  6979 | `	int bExplicitNull = 0;` |
|        - |  6980 | `	sxi32 rc;` |
|    97385 |  6981 | `	*pnType = 0;` |
|    97385 |  6982 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|    97385 |  6983 | `	*piTypeFlags = 0;` |
|    97385 |  6984 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|        - |  6985 |  |
|    97385 |  6986 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  6987 | `		return SXRET_OK;` |
|        - |  6988 | `	}` |
|        - |  6989 | ``	/* Optional `?` shorthand prefix */`` |
|    97380 |  6990 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|       89 |  6991 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|       88 |  6992 | `		bShortNullable = 1;` |
|       88 |  6993 | `		pGen->pIn++;` |
|       88 |  6994 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  6995 | `			return SXERR_SYNTAX;` |
|        - |  6996 | `		}` |
|       42 |  6997 | `	}` |
|        - |  6998 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|        - |  6999 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|        - |  7000 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|        - |  7001 | `	{` |
|        - |  7002 | `		int nMembers, bParen;` |
|    97385 |  7003 | `		sxu32 iGroup = 0;` |
|    97385 |  7004 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|    97385 |  7005 | `		if( rc != SXRET_OK ){` |
|        4 |  7006 | `			return rc;` |
|        - |  7007 | `		}` |
|        - |  7008 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|        - |  7009 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|        - |  7010 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|        - |  7011 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|        - |  7012 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|   146276 |  7013 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|    97592 |  7014 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      143 |  7015 | `			if( bShortNullable ){` |
|        - |  7016 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|        - |  7017 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|        - |  7018 | `				 * already reported" so callers skip their own error emission. */` |
|        3 |  7019 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|        - |  7020 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|        3 |  7021 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|        - |  7022 | `			}` |
|      141 |  7023 | `			if( nMembers >= 2 && !bParen ){` |
|      ! 0 |  7024 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|        - |  7025 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|      ! 0 |  7026 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  7027 | `			}` |
|      141 |  7028 | ``			pGen->pIn++; /* skip `\|` */`` |
|      141 |  7029 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|      141 |  7030 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  7031 | `				return rc;` |
|        - |  7032 | `			}` |
|        5 |  7033 | `		}` |
|    97381 |  7034 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|      ! 0 |  7035 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7036 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|      ! 0 |  7037 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  7038 | `		}` |
|        - |  7039 | `	}` |
|        - |  7040 | `	/* Validation pass.` |
|        - |  7041 | `	 *` |
|        - |  7042 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|        - |  7043 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|        - |  7044 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|        - |  7045 | `	 */` |
|        - |  7046 | `	{` |
|        - |  7047 | `		int i, j;` |
|    97381 |  7048 | `		int bHasNonNull = 0;` |
|    97381 |  7049 | `		int bAnyIntersection = 0;` |
|        - |  7050 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|        - |  7051 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|        - |  7052 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
|  3213413 |  7053 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|   194919 |  7054 | `		for( i = 0; i < nAtoms; i++ ){` |
|    97543 |  7055 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|    48774 |  7056 | `		}` |
|   194863 |  7057 | `		for( i = 0; i < nAtoms; i++ ){` |
|    97513 |  7058 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|    48746 |  7059 | `		}` |
|        - |  7060 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|        - |  7061 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|    97381 |  7062 | `		if( bShortNullable && bAnyIntersection ){` |
|      ! 0 |  7063 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7064 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|      ! 0 |  7065 | `			return SXERR_SYNTAX;` |
|        - |  7066 | `		}` |
|   194905 |  7067 | `		for( i = 0; i < nAtoms; i++ ){` |
|        - |  7068 | `			/* Intersection members must be class/interface types (PHP rejects` |
|        - |  7069 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|        - |  7070 | ``			 * `true`/`false` in an intersection). */`` |
|    97541 |  7071 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|       55 |  7072 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|       55 |  7073 | `				if( bClassLike ){` |
|       53 |  7074 | `					SyString *pC = &aAtoms[i].sClass;` |
|       48 |  7075 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|       48 |  7076 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|       48 |  7077 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|       53 |  7078 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|      ! 0 |  7079 | `						bClassLike = 0;` |
|      ! 0 |  7080 | `					}` |
|       24 |  7081 | `				}` |
|       55 |  7082 | `				if( !bClassLike ){` |
|        - |  7083 | `					const char *zName; sxu32 nName;` |
|        3 |  7084 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      ! 0 |  7085 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|      ! 0 |  7086 | `					}else{` |
|        3 |  7087 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|        - |  7088 | `					}` |
|        4 |  7089 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7090 | `						"Type %.*s cannot be part of an intersection type",` |
|        1 |  7091 | `						(int)nName, zName);` |
|        3 |  7092 | `					return SXERR_SYNTAX;` |
|        - |  7093 | `				}` |
|       24 |  7094 | `			}` |
|    97539 |  7095 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|      171 |  7096 | `				if( nAtoms > 1 ){` |
|        3 |  7097 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7098 | `						"Void can only be used as a standalone type");` |
|        3 |  7099 | `					return SXERR_SYNTAX;` |
|        - |  7100 | `				}` |
|      169 |  7101 | `				if( !bAllowVoid ){` |
|      ! 0 |  7102 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7103 | `						"void cannot be used here");` |
|      ! 0 |  7104 | `					return SXERR_SYNTAX;` |
|        - |  7105 | `				}` |
|      169 |  7106 | `				if( bShortNullable ){` |
|      ! 0 |  7107 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7108 | `						"Void type cannot be nullable");` |
|      ! 0 |  7109 | `					return SXERR_SYNTAX;` |
|        - |  7110 | `				}` |
|       82 |  7111 | `			}` |
|    97537 |  7112 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|        - |  7113 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|        - |  7114 | `				 * type (never = the function does not return). Mirrors the void` |
|        - |  7115 | `				 * validation above; accepted here and enforced at compile time` |
|        - |  7116 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|       26 |  7117 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|        - |  7118 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|        - |  7119 | `					 * same as any other non-standalone use. */` |
|        5 |  7120 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7121 | `						"never can only be used as a standalone type");` |
|        5 |  7122 | `					return SXERR_SYNTAX;` |
|        - |  7123 | `				}` |
|       21 |  7124 | `				if( !bAllowVoid ){` |
|        - |  7125 | `					/* Return-only: params call with bAllowVoid=0. */` |
|        3 |  7126 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7127 | `						"never cannot be used as a parameter type");` |
|        3 |  7128 | `					return SXERR_SYNTAX;` |
|        - |  7129 | `				}` |
|        8 |  7130 | `			}` |
|    97531 |  7131 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|       34 |  7132 | `				bExplicitNull = 1;` |
|       19 |  7133 | `			}else{` |
|    97501 |  7134 | `				bHasNonNull = 1;` |
|        - |  7135 | `			}` |
|        - |  7136 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|        - |  7137 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|        - |  7138 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|        - |  7139 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|        - |  7140 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|    97731 |  7141 | `			for( j = 0; j < i; j++ ){` |
|      207 |  7142 | `				int bDup = 0;` |
|      207 |  7143 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|      395 |  7144 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|      202 |  7145 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|      207 |  7146 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|      195 |  7147 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|       51 |  7148 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       44 |  7149 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|       44 |  7150 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|       17 |  7151 | `								aAtoms[j].sClass.zString,` |
|       34 |  7152 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|      ! 0 |  7153 | `							bDup = 1;` |
|      ! 0 |  7154 | `						}` |
|       27 |  7155 | `					}else{` |
|        3 |  7156 | `						bDup = 1;` |
|        - |  7157 | `					}` |
|       23 |  7158 | `				}` |
|      195 |  7159 | `				if( bDup ){` |
|        - |  7160 | `					const char *zName;` |
|        - |  7161 | `					sxu32 nName;` |
|        3 |  7162 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      ! 0 |  7163 | `						zName = aAtoms[i].sClass.zString;` |
|      ! 0 |  7164 | `						nName = aAtoms[i].sClass.nByte;` |
|      ! 0 |  7165 | `					}else{` |
|        3 |  7166 | `						zName = aAtoms[i].zCanon;` |
|        3 |  7167 | `						nName = aAtoms[i].nCanon;` |
|        - |  7168 | `					}` |
|        4 |  7169 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        1 |  7170 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|        3 |  7171 | `					return SXERR_SYNTAX;` |
|        - |  7172 | `				}` |
|       99 |  7173 | `			}` |
|    48767 |  7174 | `		}` |
|    97369 |  7175 | `		if( !bHasNonNull && bExplicitNull ){` |
|        7 |  7176 | `			if( bShortNullable ){` |
|        - |  7177 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|      ! 0 |  7178 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7179 | `					"Null can not be used as a standalone type");` |
|      ! 0 |  7180 | `				return SXERR_SYNTAX;` |
|        - |  7181 | `			}` |
|        - |  7182 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|        - |  7183 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|        - |  7184 | `			 * path below leaves *pnType untouched when there is no non-null` |
|        - |  7185 | `			 * atom, so set it here. */` |
|        7 |  7186 | `			*pnType = MEMOBJ_NULL;` |
|        3 |  7187 | `		}` |
|        - |  7188 | `	}` |
|        - |  7189 | `	/* Compute nullability flag */` |
|    97369 |  7190 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      116 |  7191 | `		*piTypeFlags \|= iNullableFlag;` |
|       56 |  7192 | `	}` |
|        - |  7193 | `	/* Build canonical type text */` |
|    97369 |  7194 | `	if( pTypeText ){` |
|        - |  7195 | `		SyBlob sBlob;` |
|    97369 |  7196 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|   146010 |  7197 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|    48682 |  7198 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|    97369 |  7199 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|   145781 |  7200 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    97184 |  7201 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|    97189 |  7202 | `			if( zDup ){` |
|    97189 |  7203 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|    48592 |  7204 | `			}` |
|    48592 |  7205 | `		}` |
|    97369 |  7206 | `		SyBlobRelease(&sBlob);` |
|    48682 |  7207 | `	}` |
|        - |  7208 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|        - |  7209 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|        - |  7210 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|        - |  7211 | `	{` |
|    97369 |  7212 | `		int nNonNull = 0;` |
|    97369 |  7213 | `		int iNonNullIdx = -1;` |
|        - |  7214 | `		int i;` |
|   194885 |  7215 | `		for( i = 0; i < nAtoms; i++ ){` |
|    97521 |  7216 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    97491 |  7217 | `				nNonNull++;` |
|    97491 |  7218 | `				iNonNullIdx = i;` |
|    48743 |  7219 | `			}` |
|    48763 |  7220 | `		}` |
|    97369 |  7221 | `		if( nNonNull <= 1 ){` |
|        - |  7222 | `			/* Fast path: store as single type. */` |
|    97263 |  7223 | `			if( iNonNullIdx >= 0 ){` |
|    97257 |  7224 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|    97257 |  7225 | `				if( pA->nType == SXU32_HIGH ){` |
|    23411 |  7226 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     7802 |  7227 | `						pA->sClass.zString, pA->sClass.nByte);` |
|    15609 |  7228 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|    15609 |  7229 | `					*pnType = SXU32_HIGH;` |
|    15609 |  7230 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|    89455 |  7231 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|      169 |  7232 | `					*pnType = MEMOBJ_VOID;` |
|    81571 |  7233 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|       18 |  7234 | `					*pnType = MEMOBJ_NEVER;` |
|       10 |  7235 | `				}else{` |
|    81473 |  7236 | `					*pnType = pA->nType;` |
|        - |  7237 | `				}` |
|    48626 |  7238 | `			}` |
|    48634 |  7239 | `		}else{` |
|        - |  7240 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      111 |  7241 | `			*piTypeFlags \|= iUnionFlag;` |
|      355 |  7242 | `			for( i = 0; i < nAtoms; i++ ){` |
|        - |  7243 | `				ph7_type_alt sAlt;` |
|      249 |  7244 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      239 |  7245 | `				SyZero(&sAlt, sizeof(sAlt));` |
|      239 |  7246 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|      239 |  7247 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      146 |  7248 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       47 |  7249 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       99 |  7250 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|       99 |  7251 | `					sAlt.nType = SXU32_HIGH;` |
|       99 |  7252 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|       52 |  7253 | `				}else{` |
|      145 |  7254 | `					sAlt.nType = aAtoms[i].nType;` |
|      145 |  7255 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|        - |  7256 | `				}` |
|      239 |  7257 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|      122 |  7258 | `			}` |
|        - |  7259 | `		}` |
|        - |  7260 | `	}` |
|    97369 |  7261 | `	return SXRET_OK;` |
|    48695 |  7262 | `}` |
|        - |  7263 |  |
|        - |  7264 | `/*` |
|        - |  7265 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|        - |  7266 | `` * pGen->pIn should point to the token after `)`.`` |
|        - |  7267 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|        - |  7268 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|        - |  7269 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|        - |  7270 | `` *          and union types `: T\|U`.`` |
|        - |  7271 | ` */` |
|  1490824 |  7272 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|        5 |  7273 | `{` |
|  1490829 |  7274 | `	sxi32 iFlags = 0;` |
|        - |  7275 | `	sxi32 rc;` |
|        - |  7276 | `	sxu32 nLine;` |
|  1490829 |  7277 | `	pFunc->nReturnType = 0;` |
|  1490829 |  7278 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  1490829 |  7279 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|        - |  7280 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|        - |  7281 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|        - |  7282 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|        - |  7283 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|        - |  7284 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|  1490829 |  7285 | `	SySetReset(&pFunc->aReturnUnion);` |
|  1490829 |  7286 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|  1490829 |  7287 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  1490215 |  7288 | `		return SXRET_OK;` |
|        - |  7289 | `	}` |
|      619 |  7290 | `	pGen->pIn++; /* Skip ':' */` |
|      619 |  7291 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  7292 | `		return SXRET_OK;` |
|        - |  7293 | `	}` |
|      619 |  7294 | `	nLine = pGen->pIn->nLine;` |
|      619 |  7295 | `	rc = GenStateParseUnionTypeDecl(` |
|      307 |  7296 | `		pGen,` |
|      307 |  7297 | `		&pFunc->nReturnType,` |
|      307 |  7298 | `		&pFunc->sReturnClass,` |
|      307 |  7299 | `		&pFunc->aReturnUnion,` |
|        - |  7300 | `		&iFlags,` |
|      307 |  7301 | `		&pFunc->sReturnTypeName,` |
|        - |  7302 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|        - |  7303 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|        - |  7304 | `		/* iUnionFlag */ 0,` |
|        - |  7305 | `		/* bAllowVoid */ 1,` |
|      307 |  7306 | `		nLine);` |
|      619 |  7307 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  7308 | `		return SXERR_ABORT;` |
|        - |  7309 | `	}` |
|      619 |  7310 | `	if( rc == SXERR_CORRUPT ){` |
|        - |  7311 | `		/* Error already reported */` |
|      ! 0 |  7312 | `		return SXERR_SYNTAX;` |
|        - |  7313 | `	}` |
|      619 |  7314 | `	if( rc == SXERR_SYNTAX ){` |
|        8 |  7315 | `		if( pGen->pIn < pGen->pEnd ){` |
|       11 |  7316 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|        - |  7317 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|        6 |  7318 | `				&pGen->pIn->sData);` |
|        5 |  7319 | `		}else{` |
|      ! 0 |  7320 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|        - |  7321 | `				"syntax error, unexpected end of file in return type declaration");` |
|        - |  7322 | `		}` |
|        8 |  7323 | `		return SXERR_SYNTAX;` |
|        - |  7324 | `	}` |
|      613 |  7325 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|      613 |  7326 | `	return SXRET_OK;` |
|   745417 |  7327 | `}` |
|        - |  7328 |  |
|   116996 |  7329 | `static sxi32 GenStateCompileFunc(` |
|        - |  7330 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  7331 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|        - |  7332 | `	sxi32 iFlags,        /* Control flags */` |
|        - |  7333 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|        - |  7334 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|        - |  7335 | `	)` |
|        5 |  7336 | `{` |
|        - |  7337 | `	ph7_vm_func *pFunc;` |
|        - |  7338 | `	SyToken *pEnd;` |
|        - |  7339 | `	sxu32 nLine;` |
|        - |  7340 | `	char *zName;` |
|        - |  7341 | `	sxi32 rc;` |
|        - |  7342 | `	/* Extract line number */` |
|   117001 |  7343 | `	nLine = pGen->pIn->nLine;` |
|        - |  7344 | `	/* Jump the left parenthesis '(' */` |
|   117001 |  7345 | `	pGen->pIn++;` |
|        - |  7346 | `	/* Delimit the function signature */` |
|   117001 |  7347 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   117001 |  7348 | `	if( pEnd >= pGen->pEnd ){` |
|        - |  7349 | `		/* Syntax error */` |
|        9 |  7350 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|        9 |  7351 | `		if( rc == SXERR_ABORT ){` |
|        - |  7352 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7353 | `			return SXERR_ABORT;` |
|        - |  7354 | `		}` |
|        9 |  7355 | `		pGen->pIn = pGen->pEnd;` |
|        9 |  7356 | `		return SXRET_OK;` |
|        - |  7357 | `	}` |
|        - |  7358 | `	/* Create the function state */` |
|   116995 |  7359 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   116995 |  7360 | `	if( pFunc == 0 ){` |
|      ! 0 |  7361 | `		goto OutOfMem;` |
|        - |  7362 | `	}` |
|        - |  7363 | `	/* Build the function name, prepending namespace if active */` |
|   117002 |  7364 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|        - |  7365 | `		SyBlob sFQN;` |
|        - |  7366 | `		sxu32 nLen;` |
|       16 |  7367 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       16 |  7368 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       16 |  7369 | `		SyBlobAppend(&sFQN,"\\",1);` |
|       16 |  7370 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|       16 |  7371 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|       16 |  7372 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|       16 |  7373 | `		SyBlobRelease(&sFQN);` |
|       16 |  7374 | `		if( zName == 0 ){` |
|      ! 0 |  7375 | `			goto OutOfMem;` |
|        - |  7376 | `		}` |
|       16 |  7377 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|        9 |  7378 | `	}else{` |
|   116981 |  7379 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   116981 |  7380 | `		if( zName == 0 ){` |
|      ! 0 |  7381 | `			goto OutOfMem;` |
|        - |  7382 | `		}` |
|   116981 |  7383 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|        - |  7384 | `	}` |
|        - |  7385 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|        - |  7386 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|   116995 |  7387 | `	pFunc->nLine = nLine;` |
|   116995 |  7388 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|   116995 |  7389 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  7390 | `		return SXERR_ABORT;` |
|        - |  7391 | `	}` |
|   116995 |  7392 | `	if( pGen->pIn < pEnd ){` |
|        - |  7393 | `		/* Collect function arguments */` |
|   100899 |  7394 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   100899 |  7395 | `		if( rc == SXERR_ABORT ){` |
|        - |  7396 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  7397 | `			return SXERR_ABORT;` |
|        - |  7398 | `		}` |
|    50447 |  7399 | `	}` |
|        - |  7400 | `	/* Point past ')' and parse optional return type ': type' */` |
|   116995 |  7401 | `	pGen->pIn = &pEnd[1];` |
|        - |  7402 | `	{` |
|   116995 |  7403 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   116995 |  7404 | `		if( rcRt == SXERR_ABORT ){` |
|      ! 0 |  7405 | `			return SXERR_ABORT;` |
|   116995 |  7406 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|        8 |  7407 | `			return SXERR_SYNTAX;` |
|        - |  7408 | `		}` |
|        - |  7409 | `	}` |
|   116989 |  7410 | `	if( bHandleClosure ){` |
|        - |  7411 | `		ph7_vm_func_closure_env sEnv;` |
|      337 |  7412 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|      332 |  7413 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      184 |  7414 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|       31 |  7415 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |  7416 | `				/* Closure,record environment variable */` |
|       31 |  7417 | `				pGen->pIn++;` |
|       31 |  7418 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 |  7419 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|      ! 0 |  7420 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  7421 | `						return SXERR_ABORT;` |
|        - |  7422 | `					}` |
|      ! 0 |  7423 | `				}` |
|       31 |  7424 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|        - |  7425 | `				/* Compile until we hit the first closing parenthesis */` |
|       61 |  7426 | `				while( pGen->pIn < pGen->pEnd ){` |
|       61 |  7427 | `					int iFlagsLocal = 0;` |
|       61 |  7428 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|       31 |  7429 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|       31 |  7430 | `						break;` |
|        - |  7431 | `					}` |
|       35 |  7432 | `					nLineLocal = pGen->pIn->nLine;` |
|       35 |  7433 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|        - |  7434 | `						/* Pass by reference,record that */` |
|      ! 0 |  7435 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|        - |  7436 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  7437 | `							);` |
|      ! 0 |  7438 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|      ! 0 |  7439 | `						pGen->pIn++;` |
|      ! 0 |  7440 | `					}` |
|       30 |  7441 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|       35 |  7442 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  7443 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        - |  7444 | `								"Closure: Unexpected token. Expecting a variable name");` |
|      ! 0 |  7445 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 |  7446 | `								return SXERR_ABORT;` |
|        - |  7447 | `							}` |
|        - |  7448 | `							/* Find the closing parenthesis */` |
|      ! 0 |  7449 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 |  7450 | `								pGen->pIn++;` |
|      ! 0 |  7451 | `							}` |
|      ! 0 |  7452 | `							if(pGen->pIn < pGen->pEnd){` |
|      ! 0 |  7453 | `								pGen->pIn++;` |
|      ! 0 |  7454 | `							}` |
|      ! 0 |  7455 | `							break;` |
|        - |  7456 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|      ! 0 |  7457 | `					}else{` |
|        - |  7458 | `						SyString *pNameLocal;` |
|        - |  7459 | `						char *zDup;` |
|        - |  7460 | `						/* Duplicate variable name */` |
|       35 |  7461 | `						pNameLocal = &pGen->pIn[1].sData;` |
|       35 |  7462 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|       35 |  7463 | `						if( zDup ){` |
|        - |  7464 | `							/* Zero the structure */` |
|       35 |  7465 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       35 |  7466 | `							sEnv.iFlags = iFlagsLocal;` |
|       35 |  7467 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       35 |  7468 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|       35 |  7469 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|      ! 0 |  7470 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|      ! 0 |  7471 | `									got_this = 1;` |
|      ! 0 |  7472 | `							}` |
|        - |  7473 | `							/* Save imported variable */` |
|       35 |  7474 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       20 |  7475 | `						}else{` |
|      ! 0 |  7476 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  7477 | `							 return SXERR_ABORT;` |
|        - |  7478 | `						}` |
|        - |  7479 | `					}` |
|       35 |  7480 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|       41 |  7481 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  7482 | `						/* Ignore trailing commas */` |
|        7 |  7483 | `						pGen->pIn++;` |
|        1 |  7484 | `					}` |
|        5 |  7485 | `				}` |
|       31 |  7486 | `				if( !got_this ){` |
|        - |  7487 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|        - |  7488 | `					 * available to the closure environment.` |
|        - |  7489 | `					 */` |
|       31 |  7490 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       31 |  7491 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|       31 |  7492 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       31 |  7493 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|       31 |  7494 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       13 |  7495 | `				}` |
|       31 |  7496 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|        - |  7497 | `					/* Mark as closure */` |
|       31 |  7498 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       13 |  7499 | `				}` |
|        - |  7500 | `				/* php 7.1+: the return type follows the use clause —` |
|        - |  7501 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|        - |  7502 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|        - |  7503 | `				 * so an unconditional call would wipe a type parsed at the` |
|        - |  7504 | `				 * legacy pre-use position. */` |
|       31 |  7505 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|        7 |  7506 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|        7 |  7507 | `					if( rcRt2 == SXERR_ABORT ){` |
|      ! 0 |  7508 | `						return SXERR_ABORT;` |
|        7 |  7509 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|      ! 0 |  7510 | `						return SXERR_SYNTAX;` |
|        - |  7511 | `					}` |
|        3 |  7512 | `				}` |
|       13 |  7513 | `		}` |
|      166 |  7514 | `	}` |
|        - |  7515 | `	/* Compile the body */` |
|   116989 |  7516 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   116989 |  7517 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  7518 | `		return SXERR_ABORT;` |
|        - |  7519 | `	}` |
|        - |  7520 | `	/* The cursor sits just past the body's closing brace */` |
|   116989 |  7521 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|   116989 |  7522 | `	if( ppFunc ){` |
|   116989 |  7523 | `		*ppFunc = pFunc;` |
|    58492 |  7524 | `	}` |
|   116989 |  7525 | `	rc = SXRET_OK;` |
|   116989 |  7526 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|        - |  7527 | `		/* Finally register the function */` |
|   116963 |  7528 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|    58479 |  7529 | `	}` |
|   116989 |  7530 | `	if( rc == SXRET_OK ){` |
|   116989 |  7531 | `		return SXRET_OK;` |
|        - |  7532 | `	}` |
|        - |  7533 | `	/* Fall through if something goes wrong */` |
|      ! 0 |  7534 | `OutOfMem:` |
|        - |  7535 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  7536 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  7537 | `	 */` |
|      ! 0 |  7538 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 |  7539 | `	return SXERR_ABORT;` |
|    58503 |  7540 | `}` |
|        - |  7541 | `/*` |
|        - |  7542 | ` * Compile a standard PHP function.` |
|        - |  7543 | ` *  Refer to the block-comment above for more information.` |
|        - |  7544 | ` */` |
|   116672 |  7545 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|        5 |  7546 | `{` |
|        - |  7547 | `	SyString *pName;` |
|        - |  7548 | `	sxi32 iFlags;` |
|        - |  7549 | `	sxu32 nKwLine;` |
|        - |  7550 | `	sxu32 nLine;` |
|        - |  7551 | `	sxi32 rc;` |
|        - |  7552 |  |
|   116677 |  7553 | `	nLine = pGen->pIn->nLine;` |
|   116677 |  7554 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|   116677 |  7555 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   116677 |  7556 | `	iFlags = 0;` |
|   116677 |  7557 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        - |  7558 | `		/* Return by reference,remember that */` |
|       12 |  7559 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|        - |  7560 | `		/* Jump the '&' token */` |
|       12 |  7561 | `		pGen->pIn++;` |
|        5 |  7562 | `	}` |
|   116677 |  7563 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  7564 | `		/* Invalid function name */` |
|        7 |  7565 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|        7 |  7566 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  7567 | `			return SXERR_ABORT;` |
|        - |  7568 | `		}` |
|        - |  7569 | `		/* Sychronize with the next semi-colon or braces*/` |
|       21 |  7570 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       15 |  7571 | `			pGen->pIn++;` |
|        1 |  7572 | `		}` |
|        7 |  7573 | `		return SXRET_OK;` |
|        - |  7574 | `	}` |
|   116671 |  7575 | `	pName = &pGen->pIn->sData;` |
|   116671 |  7576 | `	nLine = pGen->pIn->nLine;` |
|        - |  7577 | `	/* Jump the function name */` |
|   116671 |  7578 | `	pGen->pIn++;` |
|   116671 |  7579 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  7580 | `		/* Syntax error */` |
|        3 |  7581 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|        3 |  7582 | `		if( rc == SXERR_ABORT ){` |
|        - |  7583 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7584 | `			return SXERR_ABORT;` |
|        - |  7585 | `		}` |
|        - |  7586 | `		/* Sychronize with the next semi-colon or '{' */` |
|        3 |  7587 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  7588 | `			pGen->pIn++;` |
|      ! 0 |  7589 | `		}` |
|        3 |  7590 | `		return SXRET_OK;` |
|        - |  7591 | `	}` |
|        - |  7592 | `	/* Compile function body */` |
|        - |  7593 | `	{` |
|   116669 |  7594 | `		ph7_vm_func *pFuncState = 0;` |
|   116669 |  7595 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);` |
|   116669 |  7596 | `		if( pFuncState ){` |
|        - |  7597 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|   116657 |  7598 | `			pFuncState->nLine = nKwLine;` |
|    58326 |  7599 | `		}` |
|        - |  7600 | `	}` |
|   116669 |  7601 | `	return rc;` |
|    58341 |  7602 | `}` |
|        - |  7603 | `/*` |
|        - |  7604 | ` * Extract the visibility level associated with a given keyword.` |
|        - |  7605 | ` * According to the PHP language reference manual` |
|        - |  7606 | ` *  Visibility:` |
|        - |  7607 | ` *  The visibility of a property or method can be defined by prefixing` |
|        - |  7608 | ` *  the declaration with the keywords public, protected or private.` |
|        - |  7609 | ` *  Class members declared public can be accessed everywhere.` |
|        - |  7610 | ` *  Members declared protected can be accessed only within the class` |
|        - |  7611 | ` *  itself and by inherited and parent classes. Members declared as private` |
|        - |  7612 | ` *  may only be accessed by the class that defines the member.` |
|        - |  7613 | ` */` |
|  1724308 |  7614 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|        5 |  7615 | `{` |
|  1724313 |  7616 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    23203 |  7617 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  1701115 |  7618 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   180747 |  7619 | `		return PH7_CLASS_PROT_PROTECTED;` |
|        - |  7620 | `	}` |
|        - |  7621 | `	/* Assume public by default */` |
|  1520373 |  7622 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   862159 |  7623 | `}` |
|        - |  7624 | `/*` |
|        - |  7625 | ` * Compile a class constant.` |
|        - |  7626 | ` * According to the PHP language reference manual` |
|        - |  7627 | ` *  Class Constants` |
|        - |  7628 | ` *   It is possible to define constant values on a per-class basis remaining` |
|        - |  7629 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|        - |  7630 | ` *   you don't use the $ symbol to declare or use them.` |
|        - |  7631 | ` *   The value must be a constant expression, not (for example) a variable,` |
|        - |  7632 | ` *   a property, a result of a mathematical operation, or a function call.` |
|        - |  7633 | ` *   It's also possible for interfaces to have constants.` |
|        - |  7634 | ` * Symisc eXtension.` |
|        - |  7635 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|        - |  7636 | ` *  the zend engine would allow only simple scalar value.` |
|        - |  7637 | ` *  Example:` |
|        - |  7638 | ` *   class Test{` |
|        - |  7639 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - |  7640 | ` *   };` |
|        - |  7641 | ` *   var_dump(TEST::MyConst);` |
|        - |  7642 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - |  7643 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - |  7644 | ` */` |
|        - |  7645 | `/*` |
|        - |  7646 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|        - |  7647 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|        - |  7648 | ` * token immediately followed by '='. Anything else with a leading type token` |
|        - |  7649 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|        - |  7650 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|        - |  7651 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|        - |  7652 | ` */` |
|   142356 |  7653 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|        5 |  7654 | `{` |
|        - |  7655 | `	SyToken *p0, *p1;` |
|   142361 |  7656 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  7657 | `		return 0;` |
|        - |  7658 | `	}` |
|   142361 |  7659 | `	p0 = pGen->pIn;` |
|        - |  7660 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|   142361 |  7661 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|      ! 0 |  7662 | `		return 1;` |
|        - |  7663 | `	}` |
|   142361 |  7664 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|        5 |  7665 | `		return 1;` |
|        - |  7666 | `	}` |
|        - |  7667 | `	/* A name-like first token begins a type only when followed by another` |
|        - |  7668 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|        - |  7669 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|   142357 |  7670 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|   142357 |  7671 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|   142357 |  7672 | `		if( p1 ){` |
|   142357 |  7673 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|       34 |  7674 | `				return 1;` |
|        - |  7675 | `			}` |
|   142327 |  7676 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|        5 |  7677 | `				return 1;` |
|        - |  7678 | `			}` |
|    71159 |  7679 | `		}` |
|    71159 |  7680 | `	}` |
|   142323 |  7681 | `	return 0;` |
|    71183 |  7682 | `}` |
|        - |  7683 | `/*` |
|        - |  7684 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|        - |  7685 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|        - |  7686 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|        - |  7687 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|        - |  7688 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|        - |  7689 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|        - |  7690 | ` * Peek only; never consumes tokens.` |
|        - |  7691 | ` */` |
|       24 |  7692 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|        4 |  7693 | `{` |
|       28 |  7694 | `	SyToken *p = pGen->pIn;` |
|       39 |  7695 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|       20 |  7696 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|        3 |  7697 | `		p++; /* skip leading unary sign(s) */` |
|        1 |  7698 | `	}` |
|       28 |  7699 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|       23 |  7700 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|        - |  7701 | `	}` |
|        6 |  7702 | `	p++;` |
|        - |  7703 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|        6 |  7704 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|       16 |  7705 | `}` |
|        - |  7706 | `/*` |
|        - |  7707 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|        - |  7708 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|        - |  7709 | `` * `$o->new`), not a `new` expression.`` |
|        - |  7710 | ` */` |
|        6 |  7711 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|        3 |  7712 | `{` |
|        - |  7713 | `	sxi32 iOp;` |
|        9 |  7714 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|      ! 0 |  7715 | `		return 0;` |
|        - |  7716 | `	}` |
|        9 |  7717 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|        9 |  7718 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|        6 |  7719 | `}` |
|        - |  7720 | `/*` |
|        - |  7721 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|        - |  7722 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|        - |  7723 | ` * interface-constant and (instance/static) property-default initializers` |
|        - |  7724 | ` * ("New expressions are not supported in this context") while still allowing it` |
|        - |  7725 | ` * in global constants, parameter defaults and static-local initializers (which` |
|        - |  7726 | ` * are compiled by different functions and left untouched). The scan is` |
|        - |  7727 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|        - |  7728 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|        - |  7729 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|        - |  7730 | ` *` |
|        - |  7731 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|        - |  7732 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|        - |  7733 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|        - |  7734 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|        - |  7735 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|        - |  7736 | ` */` |
|   227474 |  7737 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|        5 |  7738 | `{` |
|   227479 |  7739 | `	SyToken *p = pGen->pIn;` |
|   227479 |  7740 | `	int iDepth = 0;` |
|   555781 |  7741 | `	while( p < pGen->pEnd ){` |
|   555781 |  7742 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|   227471 |  7743 | `			break; /* end of this initializer */` |
|        - |  7744 | `		}` |
|   328310 |  7745 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|   168013 |  7746 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|     7706 |  7747 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|        - |  7748 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|        - |  7749 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|        - |  7750 | `			 * expression. */` |
|        3 |  7751 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|        3 |  7752 | `			p++;` |
|        3 |  7753 | `			if( bArrow ){` |
|        - |  7754 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|        - |  7755 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|        3 |  7756 | `				int iBase = iDepth;` |
|       17 |  7757 | `				while( p < pGen->pEnd ){` |
|       17 |  7758 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        5 |  7759 | `						iDepth++;` |
|       15 |  7760 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        5 |  7761 | `						if( iDepth <= iBase ){` |
|      ! 0 |  7762 | `							break; /* closes an enclosing group, not the fn's own */` |
|        - |  7763 | `						}` |
|        5 |  7764 | `						iDepth--;` |
|       11 |  7765 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|        3 |  7766 | `						break;` |
|        - |  7767 | `					}` |
|       15 |  7768 | `					p++;` |
|        1 |  7769 | `				}` |
|        2 |  7770 | `			}else{` |
|        - |  7771 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|        - |  7772 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|        - |  7773 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|        - |  7774 | `				 * then skip the balanced brace block. */` |
|      ! 0 |  7775 | `				int iLocal = 0;` |
|      ! 0 |  7776 | `				while( p < pGen->pEnd ){` |
|      ! 0 |  7777 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|      ! 0 |  7778 | `						break; /* body brace */` |
|        - |  7779 | `					}` |
|      ! 0 |  7780 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      ! 0 |  7781 | `						iLocal++;` |
|      ! 0 |  7782 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      ! 0 |  7783 | `						if( iLocal > 0 ){` |
|      ! 0 |  7784 | `							iLocal--;` |
|      ! 0 |  7785 | `						}` |
|      ! 0 |  7786 | `					}` |
|      ! 0 |  7787 | `					p++;` |
|      ! 0 |  7788 | `				}` |
|      ! 0 |  7789 | `				if( p < pGen->pEnd ){` |
|      ! 0 |  7790 | `					int iBrace = 0; /* p is on the body '{' */` |
|      ! 0 |  7791 | `					while( p < pGen->pEnd ){` |
|      ! 0 |  7792 | `						if( p->nType & PH7_TK_OCB ){` |
|      ! 0 |  7793 | `							iBrace++;` |
|      ! 0 |  7794 | `						}else if( p->nType & PH7_TK_CCB ){` |
|      ! 0 |  7795 | `							iBrace--;` |
|      ! 0 |  7796 | `							if( iBrace == 0 ){` |
|      ! 0 |  7797 | `								p++;` |
|      ! 0 |  7798 | `								break;` |
|        - |  7799 | `							}` |
|      ! 0 |  7800 | `						}` |
|      ! 0 |  7801 | `						p++;` |
|      ! 0 |  7802 | `					}` |
|      ! 0 |  7803 | `				}` |
|        - |  7804 | `			}` |
|        3 |  7805 | `			continue;` |
|        - |  7806 | `		}` |
|   328313 |  7807 | `		if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     7765 |  7808 | `			iDepth++;` |
|   324433 |  7809 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     7763 |  7810 | `			if( iDepth > 0 ){` |
|     7763 |  7811 | `				iDepth--;` |
|     3879 |  7812 | `			}` |
|   316674 |  7813 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|    85193 |  7814 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|        - |  7815 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|        - |  7816 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|        - |  7817 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|       11 |  7818 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|       11 |  7819 | `				return 1;` |
|        - |  7820 | `			}` |
|      ! 0 |  7821 | `		}` |
|   328305 |  7822 | `		p++;` |
|        5 |  7823 | `	}` |
|   227471 |  7824 | `	return 0;` |
|   113742 |  7825 | `}` |
|        - |  7826 | `/*` |
|        - |  7827 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|        - |  7828 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|        - |  7829 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|        - |  7830 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|        - |  7831 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|        - |  7832 | ` * share the same backing.` |
|        - |  7833 | ` */` |
|      226 |  7834 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|        - |  7835 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|        5 |  7836 | `{` |
|      231 |  7837 | `	pAttr->nType = nType;` |
|      231 |  7838 | `	pAttr->sClass = *pClass;` |
|      231 |  7839 | `	pAttr->sTypeName = *pTypeName;` |
|      231 |  7840 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|        - |  7841 | `		sxu32 i;` |
|       73 |  7842 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|       51 |  7843 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|       51 |  7844 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|       28 |  7845 | `		}` |
|       11 |  7846 | `	}` |
|      231 |  7847 | `}` |
|   142356 |  7848 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 |  7849 | `{` |
|   142361 |  7850 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  7851 | `	SySet *pInstrContainer;` |
|        - |  7852 | `	ph7_class_attr *pCons;` |
|        - |  7853 | `	SyString *pName;` |
|        - |  7854 | `	sxi32 rc;` |
|   142361 |  7855 | `	sxu32 nType = 0;` |
|        - |  7856 | `	SyString sTypeClass;` |
|        - |  7857 | `	SyString sTypeText;` |
|        - |  7858 | `	SySet aUnionAlts;` |
|   142361 |  7859 | `	sxi32 iTypeFlags = 0;` |
|   142361 |  7860 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   142361 |  7861 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   142361 |  7862 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  7863 | `	/* Extract visibility level */` |
|   142361 |  7864 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - |  7865 | `	/* Mark as constant */` |
|   142361 |  7866 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|   142361 |  7867 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        - |  7868 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|        - |  7869 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|   142380 |  7870 | `	if( GenStateClassConstHasType(pGen) ){` |
|       61 |  7871 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|       38 |  7872 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|        - |  7873 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|        - |  7874 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|        - |  7875 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|        - |  7876 | `		 * and success paths release. */` |
|       42 |  7877 | `		if( rc == SXERR_CORRUPT ){` |
|        - |  7878 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|      ! 0 |  7879 | `			goto Synchronize;` |
|       42 |  7880 | `		}else if( rc == SXERR_ABORT ){` |
|      ! 0 |  7881 | `			return SXERR_ABORT;` |
|       42 |  7882 | `		}else if( rc != SXRET_OK ){` |
|      ! 0 |  7883 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  7884 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|      ! 0 |  7885 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  7886 | `				return SXERR_ABORT;` |
|        - |  7887 | `			}` |
|      ! 0 |  7888 | `			goto Synchronize;` |
|        - |  7889 | `		}` |
|       42 |  7890 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|       19 |  7891 | `	}` |
|    71178 |  7892 | `loop:` |
|   142363 |  7893 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|        - |  7894 | `		/* Invalid constant name */` |
|      ! 0 |  7895 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|      ! 0 |  7896 | `		if( rc == SXERR_ABORT ){` |
|        - |  7897 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7898 | `			return SXERR_ABORT;` |
|        - |  7899 | `		}` |
|      ! 0 |  7900 | `		goto Synchronize;` |
|        - |  7901 | `	}` |
|        - |  7902 | `	/* Peek constant name */` |
|   142363 |  7903 | `	pName = &pGen->pIn->sData;` |
|        - |  7904 | `	/* Make sure the constant name isn't reserved */` |
|   142363 |  7905 | `	if( GenStateIsReservedConstant(pName) ){` |
|        - |  7906 | `		/* Reserved constant name */` |
|      ! 0 |  7907 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|      ! 0 |  7908 | `		if( rc == SXERR_ABORT ){` |
|        - |  7909 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7910 | `			return SXERR_ABORT;` |
|        - |  7911 | `		}` |
|      ! 0 |  7912 | `		goto Synchronize;` |
|        - |  7913 | `	}` |
|        - |  7914 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|   142363 |  7915 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       61 |  7916 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|       38 |  7917 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       19 |  7918 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|       42 |  7919 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  7920 | `			return SXERR_ABORT;` |
|       42 |  7921 | `		}else if( rc != SXRET_OK ){` |
|        3 |  7922 | `			goto Synchronize;` |
|        - |  7923 | `		}` |
|       18 |  7924 | `	}` |
|        - |  7925 | `	/* Advance the stream cursor */` |
|   142361 |  7926 | `	pGen->pIn++;` |
|   142361 |  7927 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|        - |  7928 | `		/* Invalid declaration */` |
|      ! 0 |  7929 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|      ! 0 |  7930 | `		if( rc == SXERR_ABORT ){` |
|        - |  7931 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7932 | `			return SXERR_ABORT;` |
|        - |  7933 | `		}` |
|      ! 0 |  7934 | `		goto Synchronize;` |
|        - |  7935 | `	}` |
|   142361 |  7936 | `	pGen->pIn++; /* Jump the equal sign */` |
|        - |  7937 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|        - |  7938 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|        - |  7939 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|        - |  7940 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|   142356 |  7941 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|       39 |  7942 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|        8 |  7943 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  7944 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|        2 |  7945 | `			&pClass->sName,pName,&sTypeText);` |
|        6 |  7946 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  7947 | `			return SXERR_ABORT;` |
|        - |  7948 | `		}` |
|        6 |  7949 | `		goto Synchronize;` |
|        - |  7950 | `	}` |
|        - |  7951 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|        - |  7952 | `	 * constant initializer ("New expressions are not supported in this context").` |
|        - |  7953 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|   142357 |  7954 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|        5 |  7955 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  7956 | `			"New expressions are not supported in this context");` |
|        5 |  7957 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  7958 | `			return SXERR_ABORT;` |
|        - |  7959 | `		}` |
|        5 |  7960 | `		goto Synchronize;` |
|        - |  7961 | `	}` |
|        - |  7962 | `	/* Allocate a new class attribute */` |
|   142353 |  7963 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   142353 |  7964 | `	if( pCons ){` |
|   142353 |  7965 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|   142353 |  7966 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  7967 | `			return SXERR_ABORT;` |
|        - |  7968 | `		}` |
|    71174 |  7969 | `	}` |
|   142353 |  7970 | `	if( pCons == 0 ){` |
|      ! 0 |  7971 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  7972 | `		return SXERR_ABORT;` |
|        - |  7973 | `	}` |
|   142353 |  7974 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       35 |  7975 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       16 |  7976 | `	}` |
|        - |  7977 | `	/* Swap bytecode container */` |
|   142353 |  7978 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   142353 |  7979 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|        - |  7980 | `	/* Compile constant value.` |
|        - |  7981 | `	 */` |
|   142353 |  7982 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   142353 |  7983 | `	if( rc == SXERR_EMPTY ){` |
|        3 |  7984 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|        3 |  7985 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  7986 | `			return SXERR_ABORT;` |
|        - |  7987 | `		}` |
|        1 |  7988 | `	}` |
|        - |  7989 | `	/* Emit the done instruction */` |
|   142353 |  7990 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   142353 |  7991 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   142353 |  7992 | `	if( rc == SXERR_ABORT ){` |
|        - |  7993 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  7994 | `		return SXERR_ABORT;` |
|        - |  7995 | `	}` |
|        - |  7996 | `	/* All done,install the constant */` |
|   142353 |  7997 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|   142353 |  7998 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7999 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8000 | `		return SXERR_ABORT;` |
|        - |  8001 | `	}` |
|   142353 |  8002 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  8003 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|        3 |  8004 | `		pGen->pIn++; /* Jump the comma */` |
|        3 |  8005 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 |  8006 | `			SyToken *pTok = pGen->pIn;` |
|      ! 0 |  8007 | `			if( pTok >= pGen->pEnd ){` |
|      ! 0 |  8008 | `				pTok--;` |
|      ! 0 |  8009 | `			}` |
|      ! 0 |  8010 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  8011 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|      ! 0 |  8012 | `				&pTok->sData,&pClass->sName);` |
|      ! 0 |  8013 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8014 | `				return SXERR_ABORT;` |
|        - |  8015 | `			}` |
|      ! 0 |  8016 | `		}else{` |
|        3 |  8017 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|        3 |  8018 | `				goto loop;` |
|        - |  8019 | `			}` |
|        - |  8020 | `		}` |
|      ! 0 |  8021 | `	}` |
|   142351 |  8022 | `	SySetRelease(&aUnionAlts);` |
|   142351 |  8023 | `	return SXRET_OK;` |
|        5 |  8024 | `Synchronize:` |
|       13 |  8025 | `	SySetRelease(&aUnionAlts);` |
|        - |  8026 | `	/* Synchronize with the first semi-colon */` |
|       45 |  8027 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       35 |  8028 | `		pGen->pIn++;` |
|        3 |  8029 | `	}` |
|       13 |  8030 | `	return SXERR_CORRUPT;` |
|    71183 |  8031 | `}` |
|        - |  8032 | `/*` |
|        - |  8033 | ` * complie a class attribute or Properties in the PHP jargon.` |
|        - |  8034 | ` * According to the PHP language reference manual` |
|        - |  8035 | ` *  Properties` |
|        - |  8036 | ` *  Class member variables are called "properties". You may also see them referred` |
|        - |  8037 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|        - |  8038 | ` *  of this reference we will use "properties". They are defined by using one` |
|        - |  8039 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|        - |  8040 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|        - |  8041 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|        - |  8042 | ` *  and must not depend on run-time information in order to be evaluated.` |
|        - |  8043 | ` * Symisc eXtension.` |
|        - |  8044 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|        - |  8045 | ` *  the zend engine would allow only simple scalar value.` |
|        - |  8046 | ` *  Example:` |
|        - |  8047 | ` *   class Test{` |
|        - |  8048 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - |  8049 | ` *   };` |
|        - |  8050 | ` *   var_dump(TEST::myVar);` |
|        - |  8051 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - |  8052 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - |  8053 | ` */` |
|        - |  8054 | `/*` |
|        - |  8055 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|        - |  8056 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|        - |  8057 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|        - |  8058 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|        - |  8059 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|        - |  8060 | ` */` |
|  1296720 |  8061 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|        5 |  8062 | `{` |
|  1296725 |  8063 | `	SyToken *p = pStart;` |
|  1296725 |  8064 | `	int bFirst = 1;` |
|  1296725 |  8065 | `	if( p >= pEnd ) return 0;` |
|        - |  8066 | ``	/* Optional nullable `?` shorthand. */`` |
|  1296725 |  8067 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|       25 |  8068 | `		p++;` |
|       25 |  8069 | `		if( p >= pEnd ) return 0;` |
|       11 |  8070 | `	}` |
|        - |  8071 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|        - |  8072 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|        - |  8073 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|        - |  8074 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   648360 |  8075 | `	for(;;){` |
|  1296745 |  8076 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|        - |  8077 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|        3 |  8078 | `			p++;` |
|        9 |  8079 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|        3 |  8080 | `			if( p >= pEnd ) return 0;` |
|        3 |  8081 | `			p++; /* skip ')' */` |
|        2 |  8082 | `		}else{` |
|        - |  8083 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|        - |  8084 | ``			 * then any `&`-joined intersection members. */`` |
|  1296743 |  8085 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|  1296743 |  8086 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  8087 | `				return 0;` |
|        - |  8088 | `			}` |
|        - |  8089 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|        - |  8090 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|        - |  8091 | `			 * may still appear at the initial dispatch site). */` |
|  1296743 |  8092 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|  1296695 |  8093 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  1296690 |  8094 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    23329 |  8095 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  1296527 |  8096 | `					return 0;` |
|        - |  8097 | `				}` |
|       84 |  8098 | `			}` |
|      221 |  8099 | `			p++;` |
|      223 |  8100 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  8101 | `				p += 2;` |
|        1 |  8102 | `			}` |
|      327 |  8103 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|      224 |  8104 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  8105 | `				p++; /* skip '&' */` |
|        3 |  8106 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|        3 |  8107 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|        3 |  8108 | `				p++;` |
|        3 |  8109 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      ! 0 |  8110 | `					p += 2;` |
|      ! 0 |  8111 | `				}` |
|        1 |  8112 | `			}` |
|        - |  8113 | `		}` |
|      223 |  8114 | `		bFirst = 0;` |
|      218 |  8115 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|       25 |  8116 | `			&& p->sData.zString[0] == '\|' ){` |
|       25 |  8117 | ``			p++; /* next `\|`-separated part */`` |
|       25 |  8118 | `			continue;` |
|        - |  8119 | `		}` |
|      203 |  8120 | `		break;` |
|      ! 0 |  8121 | `	}` |
|      203 |  8122 | `	if( p >= pEnd ) return 0;` |
|      203 |  8123 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   648365 |  8124 | `}` |
|        - |  8125 |  |
|        - |  8126 | `/*` |
|        - |  8127 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|        - |  8128 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|        - |  8129 | ` * if not). Recognized forms:` |
|        - |  8130 | ` *   ?Type, array, bool, int, float, string, object,` |
|        - |  8131 | ` *   self, parent, \Ns\ClassName, ClassName` |
|        - |  8132 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|        - |  8133 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|        - |  8134 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|        - |  8135 | ` * on unrecoverable error.` |
|        - |  8136 | ` *` |
|        - |  8137 | ` * When a type is parsed:` |
|        - |  8138 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|        - |  8139 | ` *   *pClass is set to the class name (for class types)` |
|        - |  8140 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|        - |  8141 | ` *   *pTypeText is set to the original text span of the type` |
|        - |  8142 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|        - |  8143 | ` */` |
|      198 |  8144 | `static sxi32 GenStateParsePropertyType(` |
|        - |  8145 | `	ph7_gen_state *pGen,` |
|        - |  8146 | `	sxu32 *pnType,` |
|        - |  8147 | `	SyString *pClass,` |
|        - |  8148 | `	sxi32 *piTypeFlags,` |
|        - |  8149 | `	SyString *pTypeText,` |
|        - |  8150 | `	SySet *pAlts` |
|        5 |  8151 | `){` |
|      203 |  8152 | `	sxi32 iFlags = 0;` |
|        - |  8153 | `	sxi32 rc;` |
|      203 |  8154 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  8155 | `		return SXRET_OK;` |
|        - |  8156 | `	}` |
|        - |  8157 | `	/* If the first token is '$', there's no type */` |
|      203 |  8158 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      ! 0 |  8159 | `		return SXRET_OK;` |
|        - |  8160 | `	}` |
|      203 |  8161 | `	rc = GenStateParseUnionTypeDecl(` |
|       99 |  8162 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|        - |  8163 | `		PH7_CLASS_ATTR_NULLABLE,` |
|        - |  8164 | `		PH7_CLASS_ATTR_UNION,` |
|        - |  8165 | `		/* bAllowVoid */ 0,` |
|      198 |  8166 | `		pGen->pIn->nLine);` |
|      203 |  8167 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8168 | `		return rc;` |
|        - |  8169 | `	}` |
|        - |  8170 | `	/* Verify next token is '$' (start of property name) */` |
|      203 |  8171 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  8172 | `		return SXERR_SYNTAX;` |
|        - |  8173 | `	}` |
|      203 |  8174 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|      203 |  8175 | `	return SXRET_OK;` |
|      104 |  8176 | `}` |
|        - |  8177 |  |
|        - |  8178 | `/*` |
|        - |  8179 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|        - |  8180 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|        - |  8181 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|        - |  8182 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|        - |  8183 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|        - |  8184 | ` * by the type parser itself before reaching here.` |
|        - |  8185 | ` *` |
|        - |  8186 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|        - |  8187 | ` * use in the error message.` |
|        - |  8188 | ` */` |
|      366 |  8189 | `static int GenStateIsDisallowedPropertyAtom(` |
|        - |  8190 | `	sxu32 nType,` |
|        - |  8191 | `	const SyString *pClass,` |
|        - |  8192 | `	const char **pzName,` |
|        - |  8193 | `	sxu32 *pnName)` |
|        5 |  8194 | `{` |
|        - |  8195 | `	const char *z;` |
|        - |  8196 | `	sxu32 n;` |
|      371 |  8197 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|      317 |  8198 | `		return 0;` |
|        - |  8199 | `	}` |
|       59 |  8200 | `	z = pClass->zString;` |
|       59 |  8201 | `	n = pClass->nByte;` |
|       59 |  8202 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|        8 |  8203 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|        - |  8204 | `	}` |
|        - |  8205 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|        - |  8206 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|        - |  8207 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|       52 |  8208 | `	return 0;` |
|      188 |  8209 | `}` |
|        - |  8210 |  |
|        - |  8211 | `/*` |
|        - |  8212 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|        - |  8213 | ` * constant) — the main atom plus any union alternatives — against the` |
|        - |  8214 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|        - |  8215 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|        - |  8216 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|        - |  8217 | ` * type T" vs "Class constant C::X cannot have type T").` |
|        - |  8218 | ` *` |
|        - |  8219 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|        - |  8220 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|        - |  8221 | ` */` |
|      304 |  8222 | `static sxi32 GenStateValidateMemberType(` |
|        - |  8223 | `	ph7_gen_state *pGen,` |
|        - |  8224 | `	ph7_class *pClass,` |
|        - |  8225 | `	const SyString *pMemberName,` |
|        - |  8226 | `	sxu32 nType,` |
|        - |  8227 | `	const SyString *pTypeClass,` |
|        - |  8228 | `	const SyString *pTypeText,` |
|        - |  8229 | `	SySet *pUnionAlts,` |
|        - |  8230 | `	const char *zErrFmt,` |
|        - |  8231 | `	sxu32 nLine)` |
|        5 |  8232 | `{` |
|      309 |  8233 | `	const char *zBad = 0;` |
|      309 |  8234 | `	sxu32 nBad = 0;` |
|        - |  8235 | `	SyString sFallback;` |
|        - |  8236 | `	const SyString *pBad;` |
|        - |  8237 | `	sxi32 rc;` |
|      309 |  8238 | `	int bDisallowed = 0;` |
|      309 |  8239 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|        5 |  8240 | `		bDisallowed = 1;` |
|      307 |  8241 | `	}else if( pUnionAlts ){` |
|        - |  8242 | `		sxu32 i;` |
|       95 |  8243 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|       67 |  8244 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|       67 |  8245 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|        3 |  8246 | `				bDisallowed = 1;` |
|        3 |  8247 | `				break;` |
|        - |  8248 | `			}` |
|       35 |  8249 | `		}` |
|       15 |  8250 | `	}` |
|      309 |  8251 | `	if( !bDisallowed ){` |
|      303 |  8252 | `		return SXRET_OK;` |
|        - |  8253 | `	}` |
|        - |  8254 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|        - |  8255 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|        - |  8256 | `	 * canonical spelling if the type text is unavailable. */` |
|        8 |  8257 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|        8 |  8258 | `		pBad = pTypeText;` |
|        5 |  8259 | `	}else{` |
|      ! 0 |  8260 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|      ! 0 |  8261 | `		pBad = &sFallback;` |
|        - |  8262 | `	}` |
|       11 |  8263 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        3 |  8264 | `		zErrFmt,` |
|        3 |  8265 | `		&pClass->sName,pMemberName,pBad);` |
|        8 |  8266 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  8267 | `		return SXERR_ABORT;` |
|        - |  8268 | `	}` |
|        8 |  8269 | `	return SXERR_SYNTAX;` |
|      157 |  8270 | `}` |
|        - |  8271 | `/*` |
|        - |  8272 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|        - |  8273 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|        - |  8274 | ` * matched as a plain identifier in the class-member modifier position rather` |
|        - |  8275 | ` * than promoted to a lexer keyword.` |
|        - |  8276 | ` */` |
|  9918972 |  8277 | `static int GenStateIsReadonly(SyToken *pTok)` |
|        5 |  8278 | `{` |
|  9959528 |  8279 | `	return (pTok->nType & PH7_TK_ID)` |
|  5000037 |  8280 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
|  9959523 |  8281 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|        5 |  8282 | `}` |
|   208350 |  8283 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 |  8284 | `{` |
|   208355 |  8285 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  8286 | `	ph7_class_attr *pAttr;` |
|        - |  8287 | `	SyString *pName;` |
|        - |  8288 | `	sxi32 rc;` |
|   208355 |  8289 | `	sxu32 nType = 0;` |
|        - |  8290 | `	SyString sTypeClass;` |
|        - |  8291 | `	SyString sTypeText;` |
|        - |  8292 | `	SySet aUnionAlts;` |
|   208355 |  8293 | `	sxi32 iTypeFlags = 0;` |
|   208355 |  8294 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   208355 |  8295 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   208355 |  8296 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  8297 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|        - |  8298 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|        - |  8299 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   208355 |  8300 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|       21 |  8301 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|        9 |  8302 | `	}` |
|        - |  8303 | `	/* Extract visibility level */` |
|   208355 |  8304 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - |  8305 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   208454 |  8306 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      203 |  8307 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|      203 |  8308 | `		if( rc == SXERR_CORRUPT ){` |
|        - |  8309 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|      ! 0 |  8310 | `			goto Synchronize;` |
|      203 |  8311 | `		}else if( rc == SXERR_SYNTAX ){` |
|      ! 0 |  8312 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8313 | `				"Invalid property type or declaration near '%z'",` |
|      ! 0 |  8314 | `				&pGen->pIn->sData);` |
|      ! 0 |  8315 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8316 | `				return SXERR_ABORT;` |
|        - |  8317 | `			}` |
|      ! 0 |  8318 | `			goto Synchronize;` |
|      203 |  8319 | `		}else if( rc == SXERR_ABORT ){` |
|      ! 0 |  8320 | `			return SXERR_ABORT;` |
|        - |  8321 | `		}` |
|       99 |  8322 | `	}` |
|      ! 0 |  8323 | `loop:` |
|   208359 |  8324 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  8325 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|      ! 0 |  8326 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8327 | `			return SXERR_ABORT;` |
|        - |  8328 | `		}` |
|      ! 0 |  8329 | `		goto Synchronize;` |
|        - |  8330 | `	}` |
|   208359 |  8331 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   208359 |  8332 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        - |  8333 | `		/* Invalid attribute name */` |
|      ! 0 |  8334 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|      ! 0 |  8335 | `		if( rc == SXERR_ABORT ){` |
|        - |  8336 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8337 | `			return SXERR_ABORT;` |
|        - |  8338 | `		}` |
|      ! 0 |  8339 | `		goto Synchronize;` |
|        - |  8340 | `	}` |
|        - |  8341 | `	/* Peek attribute name */` |
|   208359 |  8342 | `	pName = &pGen->pIn->sData;` |
|        - |  8343 | `	/* Advance the stream cursor */` |
|   208359 |  8344 | `	pGen->pIn++;` |
|   208359 |  8345 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|        - |  8346 | `		/* Invalid declaration */` |
|        3 |  8347 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|        3 |  8348 | `		if( rc == SXERR_ABORT ){` |
|        - |  8349 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8350 | `			return SXERR_ABORT;` |
|        - |  8351 | `		}` |
|        3 |  8352 | `		goto Synchronize;` |
|        - |  8353 | `	}` |
|        - |  8354 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|        - |  8355 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   208357 |  8356 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|       41 |  8357 | `		const char *zRoErr = 0;` |
|       41 |  8358 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        3 |  8359 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|       40 |  8360 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        6 |  8361 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|       37 |  8362 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|        6 |  8363 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|        2 |  8364 | `		}` |
|       41 |  8365 | `		if( zRoErr ){` |
|       13 |  8366 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|       13 |  8367 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8368 | `				return SXERR_ABORT;` |
|        - |  8369 | `			}` |
|       13 |  8370 | `			goto Synchronize;` |
|        - |  8371 | `		}` |
|       13 |  8372 | `	}` |
|        - |  8373 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|        - |  8374 | `	 * type atom or any union alternative. void/never are already rejected` |
|        - |  8375 | `	 * by the type parser. */` |
|   208347 |  8376 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      299 |  8377 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|        - |  8378 | `			&sTypeText,` |
|      196 |  8379 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       98 |  8380 | `			"Property %z::$%z cannot have type %z",nLine);` |
|      201 |  8381 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8382 | `			return SXERR_ABORT;` |
|      201 |  8383 | `		}else if( rc != SXRET_OK ){` |
|      ! 0 |  8384 | `			goto Synchronize;` |
|        - |  8385 | `		}` |
|       98 |  8386 | `	}` |
|        - |  8387 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   208347 |  8388 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|        4 |  8389 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  8390 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|        3 |  8391 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8392 | `			return SXERR_ABORT;` |
|        - |  8393 | `		}` |
|        3 |  8394 | `		goto Synchronize;` |
|        - |  8395 | `	}` |
|        - |  8396 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|        - |  8397 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|        - |  8398 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|        - |  8399 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|        - |  8400 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|        - |  8401 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|   208345 |  8402 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|        6 |  8403 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8404 | `			"New expressions are not supported in this context");` |
|        6 |  8405 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8406 | `			return SXERR_ABORT;` |
|        - |  8407 | `		}` |
|        6 |  8408 | `		goto Synchronize;` |
|        - |  8409 | `	}` |
|        - |  8410 | `	/* Allocate a new class attribute */` |
|   208341 |  8411 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   208341 |  8412 | `	if( pAttr ){` |
|   208341 |  8413 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|   208341 |  8414 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  8415 | `			return SXERR_ABORT;` |
|        - |  8416 | `		}` |
|   104168 |  8417 | `	}` |
|   208341 |  8418 | `	if( pAttr == 0 ){` |
|      ! 0 |  8419 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  8420 | `		return SXERR_ABORT;` |
|        - |  8421 | `	}` |
|   208341 |  8422 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      199 |  8423 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       97 |  8424 | `	}` |
|   208341 |  8425 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|        - |  8426 | `		SySet *pInstrContainer;` |
|    85123 |  8427 | `		pGen->pIn++; /*Jump the equal sign */` |
|        - |  8428 | `		/* Swap bytecode container */` |
|    85123 |  8429 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    85123 |  8430 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|        - |  8431 | `		/* Compile attribute value.` |
|        - |  8432 | `		 */` |
|    85123 |  8433 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    85123 |  8434 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 |  8435 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|      ! 0 |  8436 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8437 | `				return SXERR_ABORT;` |
|        - |  8438 | `			}` |
|      ! 0 |  8439 | `		}` |
|        - |  8440 | `		/* Emit the done instruction */` |
|    85123 |  8441 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    85123 |  8442 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    42559 |  8443 | `	}` |
|        - |  8444 | `	/* All done,install the attribute */` |
|   208341 |  8445 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   208341 |  8446 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8447 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8448 | `		return SXERR_ABORT;` |
|        - |  8449 | `	}` |
|   208341 |  8450 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  8451 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|        5 |  8452 | `		pGen->pIn++; /* Jump the comma */` |
|        5 |  8453 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|      ! 0 |  8454 | `			SyToken *pTok = pGen->pIn;` |
|      ! 0 |  8455 | `			if( pTok >= pGen->pEnd ){` |
|      ! 0 |  8456 | `				pTok--;` |
|      ! 0 |  8457 | `			}` |
|      ! 0 |  8458 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  8459 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|      ! 0 |  8460 | `				&pTok->sData,&pClass->sName);` |
|      ! 0 |  8461 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8462 | `				return SXERR_ABORT;` |
|        - |  8463 | `			}` |
|      ! 0 |  8464 | `		}else{` |
|        5 |  8465 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        5 |  8466 | `				goto loop;` |
|        - |  8467 | `			}` |
|        - |  8468 | `		}` |
|      ! 0 |  8469 | `	}` |
|   208337 |  8470 | `	SySetRelease(&aUnionAlts);` |
|   208337 |  8471 | `	return SXRET_OK;` |
|        9 |  8472 | `Synchronize:` |
|        - |  8473 | `	/* Synchronize with the first semi-colon */` |
|       56 |  8474 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       37 |  8475 | `		pGen->pIn++;` |
|        3 |  8476 | `	}` |
|       22 |  8477 | `	SySetRelease(&aUnionAlts);` |
|       22 |  8478 | `	return SXERR_CORRUPT;` |
|   104180 |  8479 | `}` |
|        - |  8480 | `/*` |
|        - |  8481 | ` * Compile a class method.` |
|        - |  8482 | ` *` |
|        - |  8483 | ` * Refer to the official documentation for more information` |
|        - |  8484 | ` * on the powerful extension introduced by the PH7 engine` |
|        - |  8485 | ` * to the OO subsystem such as full type hinting,method` |
|        - |  8486 | ` * overloading and many more.` |
|        - |  8487 | ` */` |
|  1373602 |  8488 | `static sxi32 GenStateCompileClassMethod(` |
|        - |  8489 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  8490 | `	sxi32 iProtection,   /* Visibility level */` |
|        - |  8491 | `	sxi32 iFlags,        /* Configuration flags */` |
|        - |  8492 | `	int doBody,          /* TRUE to process method body */` |
|        - |  8493 | `	ph7_class *pClass    /* Class this method belongs */` |
|        - |  8494 | `	)` |
|        5 |  8495 | `{` |
|  1373607 |  8496 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  1373607 |  8497 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|        - |  8498 | `	ph7_class_method *pMeth;` |
|        - |  8499 | `	sxi32 iFuncFlags;` |
|        - |  8500 | `	SyString *pName;` |
|        - |  8501 | `	SyToken *pEnd;` |
|        - |  8502 | `	sxi32 rc;` |
|        - |  8503 | `	/* Extract visibility level */` |
|  1373607 |  8504 | `	iProtection = GetProtectionLevel(iProtection);` |
|  1373607 |  8505 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  1373607 |  8506 | `	iFuncFlags = 0;` |
|  1373607 |  8507 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  8508 | `		/* Invalid method name */` |
|      ! 0 |  8509 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|      ! 0 |  8510 | `		if( rc == SXERR_ABORT ){` |
|        - |  8511 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8512 | `			return SXERR_ABORT;` |
|        - |  8513 | `		}` |
|      ! 0 |  8514 | `		goto Synchronize;` |
|        - |  8515 | `	}` |
|  1373607 |  8516 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        - |  8517 | `		/* Return by reference,remember that */` |
|      ! 0 |  8518 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|        - |  8519 | `		/* Jump the '&' token */` |
|      ! 0 |  8520 | `		pGen->pIn++;` |
|      ! 0 |  8521 | `	}` |
|  1373607 |  8522 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  8523 | `		/* Invalid method name */` |
|      ! 0 |  8524 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|      ! 0 |  8525 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8526 | `			return SXERR_ABORT;` |
|        - |  8527 | `		}` |
|      ! 0 |  8528 | `		goto Synchronize;` |
|        - |  8529 | `	}` |
|        - |  8530 | `	/* Peek method name */` |
|  1373607 |  8531 | `	pName = &pGen->pIn->sData;` |
|  1373607 |  8532 | `	nLine = pGen->pIn->nLine;` |
|        - |  8533 | `	/* Jump the method name */` |
|  1373607 |  8534 | `	pGen->pIn++;` |
|  1373607 |  8535 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - |  8536 | `		/* Abstract method */` |
|   100009 |  8537 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|      ! 0 |  8538 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8539 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|      ! 0 |  8540 | `				&pClass->sName,pName);` |
|      ! 0 |  8541 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8542 | `				return SXERR_ABORT;` |
|        - |  8543 | `			}` |
|      ! 0 |  8544 | `		}` |
|        - |  8545 | `		/* Assemble method signature only */` |
|   100009 |  8546 | `		doBody = FALSE;` |
|    50002 |  8547 | `	}` |
|  1373607 |  8548 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  8549 | `		/* Syntax error */` |
|      ! 0 |  8550 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|      ! 0 |  8551 | `		if( rc == SXERR_ABORT ){` |
|        - |  8552 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8553 | `			return SXERR_ABORT;` |
|        - |  8554 | `		}` |
|      ! 0 |  8555 | `		goto Synchronize;` |
|        - |  8556 | `	}` |
|        - |  8557 | `	/* Allocate a new class_method instance */` |
|  1373607 |  8558 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  1373607 |  8559 | `	if( pMeth == 0 ){` |
|      ! 0 |  8560 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8561 | `		return SXERR_ABORT;` |
|        - |  8562 | `	}` |
|  1373607 |  8563 | `	pMeth->sFunc.nLine = nKwLine;` |
|  1373607 |  8564 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|  1373607 |  8565 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  8566 | `		return SXERR_ABORT;` |
|        - |  8567 | `	}` |
|        - |  8568 | `	/* Jump the left parenthesis '(' */` |
|  1373607 |  8569 | `	pGen->pIn++;` |
|  1373607 |  8570 | `	pEnd = 0; /* cc warning */` |
|        - |  8571 | `	/* Delimit the method signature */` |
|  1373607 |  8572 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  1373607 |  8573 | `	if( pEnd >= pGen->pEnd ){` |
|        - |  8574 | `		/* Syntax error */` |
|        3 |  8575 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|        3 |  8576 | `		if( rc == SXERR_ABORT ){` |
|        - |  8577 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8578 | `			return SXERR_ABORT;` |
|        - |  8579 | `		}` |
|        3 |  8580 | `		goto Synchronize;` |
|        - |  8581 | `	}` |
|        - |  8582 | `	{` |
|  1373605 |  8583 | `		int bIsCtor = 0;` |
|  1373605 |  8584 | `		int bAbstractCtor = 0;` |
|  1373600 |  8585 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|   802258 |  8586 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  1321620 |  8587 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   103975 |  8588 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        3 |  8589 | `				bAbstractCtor = 1;` |
|        2 |  8590 | `			}else{` |
|   103973 |  8591 | `				bIsCtor = 1;` |
|        - |  8592 | `			}` |
|    51985 |  8593 | `		}` |
|  1373605 |  8594 | `		if( pGen->pIn < pEnd ){` |
|        - |  8595 | `			/* Collect method arguments */` |
|   384929 |  8596 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   384929 |  8597 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8598 | `				return SXERR_ABORT;` |
|        - |  8599 | `			}` |
|   192462 |  8600 | `		}` |
|        - |  8601 | `	}` |
|        - |  8602 | `	/* Point past ')' and parse optional return type ': type' */` |
|  1373605 |  8603 | `	pGen->pIn = &pEnd[1];` |
|        - |  8604 | `	{` |
|  1373605 |  8605 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  1373605 |  8606 | `		if( rcRt == SXERR_ABORT ){` |
|      ! 0 |  8607 | `			return SXERR_ABORT;` |
|  1373605 |  8608 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|      ! 0 |  8609 | `			goto Synchronize;` |
|        - |  8610 | `		}` |
|        - |  8611 | `	}` |
|        - |  8612 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|        - |  8613 | `	 * property init/typecheck is handled by the generic typed-property path` |
|        - |  8614 | `	 * since we mint real ph7_class_attr entries. */` |
|        - |  8615 | `	{` |
|  1373605 |  8616 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|        - |  8617 | `		sxu32 i;` |
|  1950859 |  8618 | `		for( i = 0; i < nArg; i++ ){` |
|   577269 |  8619 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|        - |  8620 | `			ph7_class_attr *pAttr;` |
|   577269 |  8621 | `			sxi32 iAttrFlags = 0;` |
|        - |  8622 | `			int bArgTyped;` |
|   577269 |  8623 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   577193 |  8624 | `				continue;` |
|        - |  8625 | `			}` |
|        - |  8626 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|        - |  8627 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|        - |  8628 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|       55 |  8629 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|       82 |  8630 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|       81 |  8631 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        3 |  8632 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8633 | `					"Cannot declare variadic promoted property");` |
|        3 |  8634 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8635 | `					return SXERR_ABORT;` |
|        - |  8636 | `				}` |
|        3 |  8637 | `				goto Synchronize;` |
|        - |  8638 | `			}` |
|        - |  8639 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|        - |  8640 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|        - |  8641 | `			 * appear as an alternative of a union type. */` |
|       79 |  8642 | `			if( bArgTyped ){` |
|      110 |  8643 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|       70 |  8644 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|       70 |  8645 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|       35 |  8646 | `					"Property %z::$%z cannot have type %z",nLine);` |
|       75 |  8647 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8648 | `					return SXERR_ABORT;` |
|       75 |  8649 | `				}else if( rc != SXRET_OK ){` |
|        6 |  8650 | `					goto Synchronize;` |
|        - |  8651 | `				}` |
|       33 |  8652 | `			}` |
|        - |  8653 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|       75 |  8654 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|        4 |  8655 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  8656 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|        3 |  8657 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8658 | `					return SXERR_ABORT;` |
|        - |  8659 | `				}` |
|        3 |  8660 | `				goto Synchronize;` |
|        - |  8661 | `			}` |
|       73 |  8662 | `			if( bArgTyped ){` |
|       69 |  8663 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|       32 |  8664 | `			}` |
|       73 |  8665 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|        3 |  8666 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|        1 |  8667 | `			}` |
|       73 |  8668 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|        8 |  8669 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|        3 |  8670 | `			}` |
|       73 |  8671 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|        - |  8672 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|        - |  8673 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|       26 |  8674 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        4 |  8675 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  8676 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|        3 |  8677 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  8678 | `						return SXERR_ABORT;` |
|        - |  8679 | `					}` |
|        3 |  8680 | `					goto Synchronize;` |
|        - |  8681 | `				}` |
|       24 |  8682 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       10 |  8683 | `			}` |
|       71 |  8684 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|       71 |  8685 | `			if( pAttr == 0 ){` |
|      ! 0 |  8686 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8687 | `				return SXERR_ABORT;` |
|        - |  8688 | `			}` |
|       71 |  8689 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|       69 |  8690 | `				pAttr->nType = pArg->nType;` |
|       69 |  8691 | `				pAttr->sClass = pArg->sClass;` |
|       69 |  8692 | `				pAttr->sTypeName = pArg->sTypeName;` |
|       69 |  8693 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|        - |  8694 | `					sxu32 k;` |
|       20 |  8695 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|       14 |  8696 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|       14 |  8697 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|        8 |  8698 | `					}` |
|        3 |  8699 | `				}` |
|       32 |  8700 | `			}` |
|       71 |  8701 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|       71 |  8702 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  8703 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8704 | `				return SXERR_ABORT;` |
|        - |  8705 | `			}` |
|       38 |  8706 | `		}` |
|        - |  8707 | `	}` |
|  1373595 |  8708 | `	if( doBody ){` |
|        - |  8709 | `		/* Compile method body */` |
|  1273591 |  8710 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  1273591 |  8711 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8712 | `			return SXERR_ABORT;` |
|        - |  8713 | `		}` |
|        - |  8714 | `		/* The cursor sits just past the body's closing brace */` |
|  1273591 |  8715 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|   636798 |  8716 | `	}else{` |
|        - |  8717 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|   100009 |  8718 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|   100009 |  8719 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|    50002 |  8720 | `		}` |
|        - |  8721 | `		/* Only method signature is allowed */` |
|   100009 |  8722 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|      ! 0 |  8723 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  8724 | `				"Expected ';' after method signature '%z'",pName);` |
|      ! 0 |  8725 | `				if( rc == SXERR_ABORT ){` |
|        - |  8726 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 |  8727 | `					return SXERR_ABORT;` |
|        - |  8728 | `				}` |
|      ! 0 |  8729 | `				return SXERR_CORRUPT;` |
|        - |  8730 | `			}` |
|        - |  8731 | `	}` |
|        - |  8732 | `	/* All done,install the method */` |
|  1373595 |  8733 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  1373595 |  8734 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8735 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8736 | `		return SXERR_ABORT;` |
|        - |  8737 | `	}` |
|  1373595 |  8738 | `	return SXRET_OK;` |
|        6 |  8739 | `Synchronize:` |
|        - |  8740 | `	/* Synchronize with the first semi-colon */` |
|       40 |  8741 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       28 |  8742 | `		pGen->pIn++;` |
|        4 |  8743 | `	}` |
|       16 |  8744 | `	return SXERR_CORRUPT;` |
|   686806 |  8745 | `}` |
|        - |  8746 | `/*` |
|        - |  8747 | ` * Compile an object interface.` |
|        - |  8748 | ` *  According to the PHP language reference manual` |
|        - |  8749 | ` *   Object Interfaces:` |
|        - |  8750 | ` *   Object interfaces allow you to create code which specifies which methods` |
|        - |  8751 | ` *   a class must implement, without having to define how these methods are handled.` |
|        - |  8752 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|        - |  8753 | ` *   class, but without any of the methods having their contents defined.` |
|        - |  8754 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|        - |  8755 | ` */` |
|    46220 |  8756 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|        5 |  8757 | `{` |
|    46225 |  8758 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  8759 | `	ph7_class *pClass,*pBase;` |
|        - |  8760 | `	SyToken *pEnd,*pTmp;` |
|        - |  8761 | `	SyString *pName;` |
|        - |  8762 | `	sxi32 nKwrd;` |
|        - |  8763 | `	sxi32 rc;` |
|        - |  8764 | `	/* Jump the 'interface' keyword */` |
|    46225 |  8765 | `	pGen->pIn++;` |
|        - |  8766 | `	/* Extract interface name */` |
|    46225 |  8767 | `	pName = &pGen->pIn->sData;` |
|        - |  8768 | `	/* Advance the stream cursor */` |
|    46225 |  8769 | `	pGen->pIn++;` |
|        - |  8770 | `	/* Build FQN and obtain a raw class */ {` |
|        - |  8771 | `		SyBlob sFQN;` |
|        - |  8772 | `		SyString sFQNStr;` |
|    46225 |  8773 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    46225 |  8774 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    46225 |  8775 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    46225 |  8776 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    46225 |  8777 | `		SyBlobRelease(&sFQN);` |
|        - |  8778 | `	}` |
|    46225 |  8779 | `	if( pClass == 0 ){` |
|      ! 0 |  8780 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8781 | `		return SXERR_ABORT;` |
|        - |  8782 | `	}` |
|    46225 |  8783 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    46225 |  8784 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  8785 | `		return SXERR_ABORT;` |
|        - |  8786 | `	}` |
|        - |  8787 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|    46225 |  8788 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|        - |  8789 | `	/* Assume no base class is given */` |
|    46225 |  8790 | `	pBase = 0;` |
|    46225 |  8791 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    15391 |  8792 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    15391 |  8793 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|        - |  8794 | `			SyBlob sResolved;` |
|        - |  8795 | `			SyString sBaseName;` |
|        - |  8796 | `			sxu32 nRefLine;` |
|        - |  8797 | `			/* Extract base interface */` |
|    15391 |  8798 | `			pGen->pIn++;` |
|    15391 |  8799 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    15391 |  8800 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    15391 |  8801 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 |  8802 | `				SyBlobRelease(&sResolved);` |
|      ! 0 |  8803 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8804 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|      ! 0 |  8805 | `					pName);` |
|      ! 0 |  8806 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  8807 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8808 | `					return SXERR_ABORT;` |
|        - |  8809 | `				}` |
|      ! 0 |  8810 | `				return SXRET_OK;` |
|        - |  8811 | `			}` |
|    23084 |  8812 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    15386 |  8813 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    15391 |  8814 | `			SyStringInitFromBuf(&sBaseName,` |
|        - |  8815 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - |  8816 | `			/* Only interfaces is allowed */` |
|    15391 |  8817 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 |  8818 | `				pBase = pBase->pNextName;` |
|      ! 0 |  8819 | `			}` |
|    15391 |  8820 | `			if( pBase == 0 ){` |
|      ! 0 |  8821 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - |  8822 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|      ! 0 |  8823 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8824 | `					SyBlobRelease(&sResolved);` |
|      ! 0 |  8825 | `					return SXERR_ABORT;` |
|        - |  8826 | `				}` |
|      ! 0 |  8827 | `			}` |
|    15391 |  8828 | `			SyBlobRelease(&sResolved);` |
|     7693 |  8829 | `		}` |
|     7693 |  8830 | `	}` |
|    46225 |  8831 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - |  8832 | `		/* Syntax error */` |
|      ! 0 |  8833 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|      ! 0 |  8834 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  8835 | `		if( rc == SXERR_ABORT ){` |
|        - |  8836 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8837 | `			return SXERR_ABORT;` |
|        - |  8838 | `		}` |
|      ! 0 |  8839 | `		return SXRET_OK;` |
|        - |  8840 | `	}` |
|    46225 |  8841 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    46225 |  8842 | `	pEnd = 0; /* cc warning */` |
|        - |  8843 | `	/* Delimit the interface body */` |
|    46225 |  8844 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    46225 |  8845 | `	if( pEnd >= pGen->pEnd ){` |
|        - |  8846 | `		/* Syntax error */` |
|      ! 0 |  8847 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|      ! 0 |  8848 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  8849 | `		if( rc == SXERR_ABORT ){` |
|        - |  8850 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8851 | `			return SXERR_ABORT;` |
|        - |  8852 | `		}` |
|      ! 0 |  8853 | `		return SXRET_OK;` |
|        - |  8854 | `	}` |
|        - |  8855 | `	/* The delimiter token is the interface body's closing brace */` |
|    46225 |  8856 | `	pClass->nEndLine = pEnd->nLine;` |
|        - |  8857 | `	/* Swap token stream */` |
|    46225 |  8858 | `	pTmp = pGen->pEnd;` |
|    46225 |  8859 | `	pGen->pEnd = pEnd;` |
|        - |  8860 | `	/* Start the parse process` |
|        - |  8861 | `	 * Note (According to the PHP reference manual):` |
|        - |  8862 | `	 *  Only constants and function signatures(without body) are allowed.` |
|        - |  8863 | `	 *  Only 'public' visibility is allowed.` |
|        - |  8864 | `	 */` |
|    73107 |  8865 | `	for(;;){` |
|        - |  8866 | `		/* Jump leading/trailing semi-colons */` |
|   246213 |  8867 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|    99999 |  8868 | `			pGen->pIn++;` |
|        5 |  8869 | `		}` |
|   146219 |  8870 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - |  8871 | `			/* End of interface body */` |
|    46221 |  8872 | `			break;` |
|        - |  8873 | `		}` |
|        - |  8874 | `		/* Bind a directly-preceding docblock to this member */` |
|   100003 |  8875 | `		GenStateSetPendingDoc(&(*pGen));` |
|   100003 |  8876 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 |  8877 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  8878 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|      ! 0 |  8879 | `				&pGen->pIn->sData,pName);` |
|      ! 0 |  8880 | `			if( rc == SXERR_ABORT ){` |
|        - |  8881 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  8882 | `				return SXERR_ABORT;` |
|        - |  8883 | `			}` |
|      ! 0 |  8884 | `			goto done;` |
|        - |  8885 | `		}` |
|        - |  8886 | `		/* Extract the current keyword */` |
|   100003 |  8887 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   100003 |  8888 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        - |  8889 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|        - |  8890 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|        3 |  8891 | `			const char *zKind = "member";` |
|        3 |  8892 | `			SyString *pMemberName = 0;` |
|        3 |  8893 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|        3 |  8894 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|        3 |  8895 | `				if( nNext == PH7_TKWRD_CONST ){` |
|        3 |  8896 | `					zKind = "constant";` |
|        3 |  8897 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|        3 |  8898 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|        2 |  8899 | `					}` |
|        1 |  8900 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|      ! 0 |  8901 | `					zKind = "method";` |
|      ! 0 |  8902 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|      ! 0 |  8903 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|      ! 0 |  8904 | `					}` |
|      ! 0 |  8905 | `				}` |
|        1 |  8906 | `			}` |
|        3 |  8907 | `			if( pMemberName ){` |
|        4 |  8908 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        1 |  8909 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|        2 |  8910 | `			}else{` |
|      ! 0 |  8911 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  8912 | `					"Access type for interface %s must be public",zKind);` |
|        - |  8913 | `			}` |
|        3 |  8914 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8915 | `				return SXERR_ABORT;` |
|        - |  8916 | `			}` |
|        3 |  8917 | `			goto done;` |
|        - |  8918 | `		}` |
|   100001 |  8919 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|      ! 0 |  8920 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  8921 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|      ! 0 |  8922 | `			if( rc == SXERR_ABORT ){` |
|        - |  8923 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  8924 | `				return SXERR_ABORT;` |
|        - |  8925 | `			}` |
|      ! 0 |  8926 | `			goto done;` |
|        - |  8927 | `		}` |
|   100001 |  8928 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|        - |  8929 | `			/* Advance the stream cursor */` |
|    99989 |  8930 | `			pGen->pIn++;` |
|    99989 |  8931 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 |  8932 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  8933 | `					"Expecting method signature inside interface '%z'",pName);` |
|      ! 0 |  8934 | `				if( rc == SXERR_ABORT ){` |
|        - |  8935 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 |  8936 | `					return SXERR_ABORT;` |
|        - |  8937 | `				}` |
|      ! 0 |  8938 | `				goto done;` |
|        - |  8939 | `			}` |
|    99989 |  8940 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    99989 |  8941 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|      ! 0 |  8942 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  8943 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|      ! 0 |  8944 | `				if( rc == SXERR_ABORT ){` |
|        - |  8945 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 |  8946 | `					return SXERR_ABORT;` |
|        - |  8947 | `				}` |
|      ! 0 |  8948 | `				goto done;` |
|        - |  8949 | `			}` |
|    49992 |  8950 | `		}` |
|   100001 |  8951 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|        - |  8952 | `			/* Parse constant */` |
|       10 |  8953 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       10 |  8954 | `			if( rc != SXRET_OK ){` |
|        3 |  8955 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8956 | `					return SXERR_ABORT;` |
|        - |  8957 | `				}` |
|        3 |  8958 | `				goto done;` |
|        - |  8959 | `			}` |
|        4 |  8960 | `		}else{` |
|    99993 |  8961 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|    99993 |  8962 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|        - |  8963 | `				/* Static method,record that */` |
|    11537 |  8964 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|        - |  8965 | `				/* Advance the stream cursor */` |
|    11537 |  8966 | `				pGen->pIn++;` |
|    11532 |  8967 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    11537 |  8968 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 |  8969 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  8970 | `							"Expecting method signature inside interface '%z'",pName);` |
|      ! 0 |  8971 | `						if( rc == SXERR_ABORT ){` |
|        - |  8972 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 |  8973 | `							return SXERR_ABORT;` |
|        - |  8974 | `						}` |
|      ! 0 |  8975 | `						goto done;` |
|        - |  8976 | `				}` |
|     5766 |  8977 | `			}` |
|        - |  8978 | `			/* Process method signature (no body for interface methods) */` |
|    99993 |  8979 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|    99993 |  8980 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  8981 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8982 | `					return SXERR_ABORT;` |
|        - |  8983 | `				}` |
|      ! 0 |  8984 | `				goto done;` |
|        - |  8985 | `			}` |
|        - |  8986 | `		}` |
|        5 |  8987 | `	}` |
|        - |  8988 | `	/* Install the interface */` |
|    46221 |  8989 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    46221 |  8990 | `	if( rc == SXRET_OK && pBase ){` |
|        - |  8991 | `		/* Inherit from the base interface */` |
|    15391 |  8992 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|     7693 |  8993 | `	}` |
|    46221 |  8994 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8995 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8996 | `		return SXERR_ABORT;` |
|        - |  8997 | `	}` |
|    23108 |  8998 | `done:` |
|        - |  8999 | `	/* Point beyond the interface body */` |
|    46225 |  9000 | `	pGen->pIn  = &pEnd[1];` |
|    46225 |  9001 | `	pGen->pEnd = pTmp;` |
|    46225 |  9002 | `	return PH7_OK;` |
|    23115 |  9003 | `}` |
|        - |  9004 | `/*` |
|        - |  9005 | ` * Compile a user-defined class.` |
|        - |  9006 | ` * According to the PHP language reference manual` |
|        - |  9007 | ` *  class` |
|        - |  9008 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|        - |  9009 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|        - |  9010 | ` *  of the properties and methods belonging to the class.` |
|        - |  9011 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|        - |  9012 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|        - |  9013 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|        - |  9014 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|        - |  9015 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|        - |  9016 | ` *  (called "methods").` |
|        - |  9017 | ` */` |
|        - |  9018 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|        - |  9019 | `typedef struct TraitUseEntry TraitUseEntry;` |
|        - |  9020 | `struct TraitUseEntry {` |
|        - |  9021 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|        - |  9022 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|        - |  9023 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|        - |  9024 | `};` |
|        - |  9025 | `/*` |
|        - |  9026 | ` * Validate that methods implementing interface contracts have compatible` |
|        - |  9027 | ` * signatures: public visibility and at least as many parameters as declared.` |
|        - |  9028 | ` */` |
|   212800 |  9029 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 |  9030 | `{` |
|        - |  9031 | `	ph7_class **apIface;` |
|        - |  9032 | `	sxu32 nIface,i;` |
|        - |  9033 | `	sxi32 rc;` |
|   212805 |  9034 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      ! 0 |  9035 | `		return SXRET_OK;` |
|        - |  9036 | `	}` |
|   212805 |  9037 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   212805 |  9038 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   424495 |  9039 | `	for(i = 0; i < nIface; i++){` |
|   211695 |  9040 | `		ph7_class *pIface = apIface[i];` |
|        - |  9041 | `		SyHashEntry *pEntry;` |
|   211695 |  9042 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   492795 |  9043 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   281105 |  9044 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |  9045 | `			ph7_class_method *pImplMeth;` |
|   281105 |  9046 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|        - |  9047 | `			/* Find the implementing method in the class */` |
|   281105 |  9048 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   281105 |  9049 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       18 |  9050 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|        - |  9051 | `			}` |
|        - |  9052 | `			/* Check visibility: interface methods must be implemented as public */` |
|   281091 |  9053 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        4 |  9054 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|        - |  9055 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|        1 |  9056 | `					&pClass->sName,pMName,&pIface->sName);` |
|        3 |  9057 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9058 | `					return SXERR_ABORT;` |
|        - |  9059 | `				}` |
|        1 |  9060 | `			}` |
|        - |  9061 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|        - |  9062 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|        - |  9063 | `			 */` |
|        - |  9064 | `			{` |
|   281091 |  9065 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   281091 |  9066 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   281091 |  9067 | `				int sigError = 0;` |
|   281091 |  9068 | `				if( nImplArgs < nIfaceArgs ){` |
|        3 |  9069 | `					sigError = 1;` |
|   281090 |  9070 | `				}else if( nImplArgs > nIfaceArgs ){` |
|        - |  9071 | `					/* Extra parameters must all have default values */` |
|        6 |  9072 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|        - |  9073 | `					sxu32 k;` |
|        8 |  9074 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|        6 |  9075 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|        3 |  9076 | `							sigError = 1;` |
|        3 |  9077 | `							break;` |
|        - |  9078 | `						}` |
|        2 |  9079 | `					}` |
|        2 |  9080 | `				}` |
|   281091 |  9081 | `				if( sigError ){` |
|        - |  9082 | `					SyBlob sImplSig, sIfaceSig;` |
|        - |  9083 | `					ph7_vm_func_arg *aArgs;` |
|        - |  9084 | `					sxu32 j;` |
|        6 |  9085 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|        6 |  9086 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|        - |  9087 | `					/* Build implementing method signature */` |
|        6 |  9088 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       12 |  9089 | `					for(j = 0; j < nImplArgs; j++){` |
|        8 |  9090 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|        8 |  9091 | `						SyBlobAppend(&sImplSig,"$",1);` |
|        8 |  9092 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|        5 |  9093 | `					}` |
|        - |  9094 | `					/* Build interface method signature */` |
|        6 |  9095 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|       12 |  9096 | `					for(j = 0; j < nIfaceArgs; j++){` |
|        8 |  9097 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|        8 |  9098 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|        8 |  9099 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|        5 |  9100 | `					}` |
|        8 |  9101 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|        - |  9102 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|        2 |  9103 | `						&pClass->sName,pMName,` |
|        4 |  9104 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|        2 |  9105 | `						&pIface->sName,pMName,` |
|        4 |  9106 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|        6 |  9107 | `					SyBlobRelease(&sImplSig);` |
|        6 |  9108 | `					SyBlobRelease(&sIfaceSig);` |
|        6 |  9109 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9110 | `						return SXERR_ABORT;` |
|        - |  9111 | `					}` |
|        2 |  9112 | `				}` |
|        - |  9113 | `			}` |
|        5 |  9114 | `		}` |
|   105850 |  9115 | `	}` |
|   212805 |  9116 | `	return SXRET_OK;` |
|   106405 |  9117 | `}` |
|        - |  9118 | `/*` |
|        - |  9119 | ` * Check that a concrete class has no remaining abstract methods.` |
|        - |  9120 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|        - |  9121 | ` */` |
|   212800 |  9122 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 |  9123 | `{` |
|        - |  9124 | `	ph7_class_method *pMeth;` |
|        - |  9125 | `	SyHashEntry *pEntry;` |
|        - |  9126 | `	sxu32 nAbstract;` |
|        - |  9127 | `	SyBlob sMsg;` |
|        - |  9128 | `	sxi32 rc;` |
|        - |  9129 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   212805 |  9130 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     7729 |  9131 | `		return SXRET_OK;` |
|        - |  9132 | `	}` |
|        - |  9133 | `	/* Count abstract methods */` |
|   205081 |  9134 | `	nAbstract = 0;` |
|   205081 |  9135 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  3036153 |  9136 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  2831077 |  9137 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  2831077 |  9138 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       20 |  9139 | `			nAbstract++;` |
|        8 |  9140 | `		}` |
|        5 |  9141 | `	}` |
|   205081 |  9142 | `	if( nAbstract == 0 ){` |
|   205067 |  9143 | `		return SXRET_OK;` |
|        - |  9144 | `	}` |
|        - |  9145 | `	/* Build the error message listing all abstract methods with origins */` |
|       18 |  9146 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       18 |  9147 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|        - |  9148 | `		"be declared abstract or implement the remaining method%s (",` |
|        7 |  9149 | `		&pClass->sName,nAbstract,` |
|        7 |  9150 | `		(nAbstract > 1 ? "s" : ""),` |
|        7 |  9151 | `		(nAbstract > 1 ? "s" : ""));` |
|        - |  9152 | `	/* Second pass: list methods with origins */` |
|        - |  9153 | `	{` |
|       18 |  9154 | `		sxu32 nListed = 0;` |
|       18 |  9155 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|       36 |  9156 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|       22 |  9157 | `			ph7_class *pOrigin = 0;` |
|        - |  9158 | `			SyString *pMName;` |
|       22 |  9159 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|       22 |  9160 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|        3 |  9161 | `				continue;` |
|        - |  9162 | `			}` |
|       20 |  9163 | `			pMName = &pMeth->sFunc.sName;` |
|       20 |  9164 | `			if( nListed > 0 ){` |
|        3 |  9165 | `				SyBlobAppend(&sMsg,", ",2);` |
|        1 |  9166 | `			}` |
|        - |  9167 | `			/* Find the origin of this abstract method.` |
|        - |  9168 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|        - |  9169 | `			 * inheritance chains) take precedence for interface-declared` |
|        - |  9170 | `			 * methods. Abstract class methods only win when the class` |
|        - |  9171 | `			 * itself declared the abstract method (not inherited from` |
|        - |  9172 | `			 * an interface). Trait methods are adopted into the using` |
|        - |  9173 | `			 * class's namespace.` |
|        - |  9174 | `			 */` |
|        - |  9175 | `			{` |
|        - |  9176 | `				ph7_class **apIface;` |
|        - |  9177 | `				ph7_class **apTrait;` |
|        - |  9178 | `				ph7_class *pWalk;` |
|        - |  9179 | `				sxu32 i;` |
|        - |  9180 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|        - |  9181 | `				 * (one that was written in the class body, not inherited from an` |
|        - |  9182 | `				 * interface). PHP attributes origin to the declaring class.` |
|        - |  9183 | `				 */` |
|       20 |  9184 | `				if( pClass->pBase ){` |
|       11 |  9185 | `					pWalk = pClass->pBase;` |
|       19 |  9186 | `					while( pWalk ){` |
|        - |  9187 | `						ph7_class_method *pParentMeth;` |
|       13 |  9188 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|       13 |  9189 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        - |  9190 | `							/* Exclude methods that came from an interface anywhere` |
|        - |  9191 | `							 * in this class's ancestor chain.` |
|        - |  9192 | `							 */` |
|       13 |  9193 | `							int fromIface = 0;` |
|       13 |  9194 | `							ph7_class *pAnc = pWalk;` |
|       17 |  9195 | `							while( pAnc ){` |
|        - |  9196 | `								ph7_class **apPI;` |
|        - |  9197 | `								sxu32 j;` |
|       15 |  9198 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|       15 |  9199 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       10 |  9200 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       10 |  9201 | `										fromIface = 1;` |
|       10 |  9202 | `										break;` |
|        - |  9203 | `									}` |
|      ! 0 |  9204 | `								}` |
|       15 |  9205 | `								if( fromIface ) break;` |
|        6 |  9206 | `								pAnc = pAnc->pBase;` |
|        2 |  9207 | `							}` |
|       13 |  9208 | `							if( !fromIface ){` |
|        3 |  9209 | `								pOrigin = pWalk;` |
|        3 |  9210 | `								break;` |
|        - |  9211 | `							}` |
|        4 |  9212 | `						}` |
|       10 |  9213 | `						pWalk = pWalk->pBase;` |
|        2 |  9214 | `					}` |
|        4 |  9215 | `				}` |
|        - |  9216 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|        - |  9217 | `				 * each interface's own parent chain for the deepest origin.` |
|        - |  9218 | `				 */` |
|       20 |  9219 | `				if( !pOrigin ){` |
|       18 |  9220 | `					pWalk = pClass;` |
|       40 |  9221 | `					while( pWalk && !pOrigin ){` |
|       26 |  9222 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|       26 |  9223 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|       16 |  9224 | `							ph7_class *pIface = apIface[i];` |
|       16 |  9225 | `							ph7_class *pDeepest = 0;` |
|       28 |  9226 | `							while( pIface ){` |
|       16 |  9227 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|       16 |  9228 | `									pDeepest = pIface;` |
|        6 |  9229 | `								}` |
|       16 |  9230 | `								pIface = pIface->pBase;` |
|        4 |  9231 | `							}` |
|       16 |  9232 | `							if( pDeepest ){` |
|       16 |  9233 | `								pOrigin = pDeepest;` |
|       16 |  9234 | `								break;` |
|        - |  9235 | `							}` |
|      ! 0 |  9236 | `						}` |
|       26 |  9237 | `						pWalk = pWalk->pBase;` |
|        4 |  9238 | `					}` |
|        7 |  9239 | `				}` |
|        - |  9240 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|       20 |  9241 | `				if( !pOrigin ){` |
|        3 |  9242 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|        3 |  9243 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|        3 |  9244 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|        3 |  9245 | `							pOrigin = pClass;` |
|        3 |  9246 | `							break;` |
|        - |  9247 | `						}` |
|      ! 0 |  9248 | `					}` |
|        1 |  9249 | `				}` |
|        - |  9250 | `			}` |
|       20 |  9251 | `			if( pOrigin ){` |
|       20 |  9252 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|       12 |  9253 | `			}else{` |
|        - |  9254 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|      ! 0 |  9255 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|        - |  9256 | `			}` |
|       20 |  9257 | `			nListed++;` |
|        4 |  9258 | `		}` |
|        - |  9259 | `	}` |
|       18 |  9260 | `	SyBlobAppend(&sMsg,")",1);` |
|       25 |  9261 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|       14 |  9262 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|       18 |  9263 | `	SyBlobRelease(&sMsg);` |
|       18 |  9264 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  9265 | `		return SXERR_ABORT;` |
|        - |  9266 | `	}` |
|       18 |  9267 | `	return SXRET_OK;` |
|   106405 |  9268 | `}` |
|        - |  9269 | `/*` |
|        - |  9270 | ` * Parse a class/interface name reference from the current token stream.` |
|        - |  9271 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|        - |  9272 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|        - |  9273 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|        - |  9274 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|        - |  9275 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|        - |  9276 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|        - |  9277 | ` */` |
|   189988 |  9278 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|        5 |  9279 | `{` |
|   189993 |  9280 | `	int isAbsolute = 0;` |
|   189993 |  9281 | `	SyToken *pStart = pGen->pIn;` |
|        - |  9282 | `	SyBlob sName;` |
|   189993 |  9283 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|     4373 |  9284 | `		isAbsolute = 1;` |
|     4373 |  9285 | `		pGen->pIn++;` |
|     2184 |  9286 | `	}` |
|   189993 |  9287 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        8 |  9288 | `		pGen->pIn = pStart;` |
|        8 |  9289 | `		return SXERR_INVALID;` |
|        - |  9290 | `	}` |
|   189987 |  9291 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   189987 |  9292 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   189987 |  9293 | `	pGen->pIn++;` |
|   284994 |  9294 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|    95017 |  9295 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       16 |  9296 | `		SyBlobAppend(&sName,"\\",1);` |
|       16 |  9297 | `		pGen->pIn++;` |
|       16 |  9298 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       16 |  9299 | `		pGen->pIn++;` |
|        2 |  9300 | `	}` |
|   189987 |  9301 | `	if( isAbsolute ){` |
|     4371 |  9302 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|     2188 |  9303 | `	}else{` |
|        - |  9304 | `		SyString sRaw;` |
|   185621 |  9305 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   185621 |  9306 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|        - |  9307 | `	}` |
|   189987 |  9308 | `	SyBlobRelease(&sName);` |
|   189987 |  9309 | `	return SXRET_OK;` |
|    94999 |  9310 | `}` |
|        - |  9311 | `/*` |
|        - |  9312 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|        - |  9313 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|        - |  9314 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|        - |  9315 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|        - |  9316 | ` * either direction cannot run unbounded.` |
|        - |  9317 | ` */` |
|        - |  9318 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    46316 |  9319 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|        5 |  9320 | `{` |
|        - |  9321 | `	ph7_class **apParent;` |
|        - |  9322 | `	sxu32 n;` |
|   119583 |  9323 | `	while( pInterface ){` |
|    80965 |  9324 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|      ! 0 |  9325 | `			return FALSE;` |
|        - |  9326 | `		}` |
|   100203 |  9327 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    38476 |  9328 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|     7703 |  9329 | `			return TRUE;` |
|        - |  9330 | `		}` |
|    73267 |  9331 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    73267 |  9332 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|      ! 0 |  9333 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|      ! 0 |  9334 | `				return TRUE;` |
|        - |  9335 | `			}` |
|      ! 0 |  9336 | `		}` |
|    73267 |  9337 | `		pInterface = pInterface->pBase;` |
|    73267 |  9338 | `		iDepth++;` |
|        5 |  9339 | `	}` |
|    38623 |  9340 | `	return FALSE;` |
|    23163 |  9341 | `}` |
|    46316 |  9342 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|        5 |  9343 | `{` |
|    46321 |  9344 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|        5 |  9345 | `}` |
|        - |  9346 | `/*` |
|        - |  9347 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|        - |  9348 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|        - |  9349 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|        - |  9350 | ` */` |
|     7698 |  9351 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|        5 |  9352 | `{` |
|     7707 |  9353 | `	while( pBase ){` |
|       10 |  9354 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|        2 |  9355 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|        3 |  9356 | `			return TRUE;` |
|        - |  9357 | `		}` |
|       10 |  9358 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|        6 |  9359 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|        3 |  9360 | `			return TRUE;` |
|        - |  9361 | `		}` |
|        5 |  9362 | `		pBase = pBase->pBase;` |
|        1 |  9363 | `	}` |
|     7699 |  9364 | `	return FALSE;` |
|     3854 |  9365 | `}` |
|        - |  9366 | `/*` |
|        - |  9367 | ` * Compile a class declaration, named or anonymous.` |
|        - |  9368 | ` *` |
|        - |  9369 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|        - |  9370 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|        - |  9371 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|        - |  9372 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|        - |  9373 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|        - |  9374 | ` * implements, body, install) is shared by both paths.` |
|        - |  9375 | ` */` |
|   212840 |  9376 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|        - |  9377 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|        5 |  9378 | `{` |
|   212845 |  9379 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  9380 | `	ph7_class *pClass,*pBase;` |
|        - |  9381 | `	SyToken *pEnd,*pTmp;` |
|        - |  9382 | `	sxi32 iProtection;` |
|        - |  9383 | `	SySet aInterfaces;` |
|        - |  9384 | `	SySet aUseEntries;` |
|        - |  9385 | `	sxi32 iAttrflags;` |
|        - |  9386 | `	SyString *pName;` |
|        - |  9387 | `	sxi32 nKwrd;` |
|        - |  9388 | `	sxi32 rc;` |
|        - |  9389 | `	/* Jump the 'class' keyword */` |
|   212845 |  9390 | `	pGen->pIn++;` |
|   212845 |  9391 | `	if( pAnonName ){` |
|        - |  9392 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|        - |  9393 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|        - |  9394 | `		 * then use the synthesized name. */` |
|       30 |  9395 | `		*ppArgStart = *ppArgEnd = 0;` |
|       30 |  9396 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        7 |  9397 | `			pGen->pIn++; /* Jump '(' */` |
|        7 |  9398 | `			*ppArgStart = pGen->pIn;` |
|       10 |  9399 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|        3 |  9400 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|        7 |  9401 | `			pGen->pIn = *ppArgEnd;` |
|        7 |  9402 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|        3 |  9403 | `		}` |
|       30 |  9404 | `		pName = pAnonName;` |
|       30 |  9405 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|       17 |  9406 | `	}else{` |
|   212819 |  9407 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|        - |  9408 | `			/* Syntax error */` |
|      ! 0 |  9409 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|      ! 0 |  9410 | `			if( rc == SXERR_ABORT ){` |
|        - |  9411 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  9412 | `				return SXERR_ABORT;` |
|        - |  9413 | `			}` |
|        - |  9414 | `			/* Synchronize with the first semi-colon or curly braces */` |
|      ! 0 |  9415 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|      ! 0 |  9416 | `				pGen->pIn++;` |
|      ! 0 |  9417 | `			}` |
|      ! 0 |  9418 | `			return SXRET_OK;` |
|        - |  9419 | `		}` |
|        - |  9420 | `		/* Extract class name */` |
|   212819 |  9421 | `		pName = &pGen->pIn->sData;` |
|        - |  9422 | `		/* Advance the stream cursor */` |
|   212819 |  9423 | `		pGen->pIn++;` |
|        - |  9424 | `		/* Build FQN and obtain a raw class */ {` |
|        - |  9425 | `			SyBlob sFQN;` |
|        - |  9426 | `			SyString sFQNStr;` |
|   212819 |  9427 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   212819 |  9428 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|   212819 |  9429 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   212819 |  9430 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   212819 |  9431 | `			SyBlobRelease(&sFQN);` |
|        - |  9432 | `		}` |
|        - |  9433 | `	}` |
|   212845 |  9434 | `	if( pClass == 0 ){` |
|      ! 0 |  9435 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9436 | `		return SXERR_ABORT;` |
|        - |  9437 | `	}` |
|   212845 |  9438 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|   212845 |  9439 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  9440 | `		return SXERR_ABORT;` |
|        - |  9441 | `	}` |
|        - |  9442 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   212845 |  9443 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   212845 |  9444 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|        - |  9445 | `	/* Assume a standalone class */` |
|   212845 |  9446 | `	pBase = 0;` |
|   212845 |  9447 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   169507 |  9448 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   169507 |  9449 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|        - |  9450 | `			SyBlob sResolved;` |
|        - |  9451 | `			SyString sBaseName;` |
|        - |  9452 | `			sxu32 nRefLine;` |
|   123215 |  9453 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   123215 |  9454 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   123215 |  9455 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   123215 |  9456 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        3 |  9457 | `				SyBlobRelease(&sResolved);` |
|        4 |  9458 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  9459 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|        1 |  9460 | `					pName);` |
|        3 |  9461 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|        3 |  9462 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9463 | `					return SXERR_ABORT;` |
|        - |  9464 | `				}` |
|        3 |  9465 | `				return SXRET_OK;` |
|        - |  9466 | `			}` |
|   184817 |  9467 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   123208 |  9468 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   123213 |  9469 | `			SyStringInitFromBuf(&sBaseName,` |
|        - |  9470 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - |  9471 | `			/* Interfaces are not allowed */` |
|   123213 |  9472 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|      ! 0 |  9473 | `				pBase = pBase->pNextName;` |
|      ! 0 |  9474 | `			}` |
|   123213 |  9475 | `			if( pBase == 0 ){` |
|      ! 0 |  9476 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - |  9477 | `					"Nonexistent base class '%z'",&sBaseName);` |
|      ! 0 |  9478 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9479 | `					SyBlobRelease(&sResolved);` |
|      ! 0 |  9480 | `					return SXERR_ABORT;` |
|        - |  9481 | `				}` |
|      ! 0 |  9482 | `			}else{` |
|   123213 |  9483 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|      ! 0 |  9484 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  9485 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|      ! 0 |  9486 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9487 | `						SyBlobRelease(&sResolved);` |
|      ! 0 |  9488 | `						return SXERR_ABORT;` |
|        - |  9489 | `					}` |
|      ! 0 |  9490 | `				}` |
|        - |  9491 | `			}` |
|   123213 |  9492 | `			SyBlobRelease(&sResolved);` |
|    61604 |  9493 | `		}` |
|   169505 |  9494 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|        - |  9495 | `			ph7_class *pInterface;` |
|        - |  9496 | `			/* Interface implementation */` |
|    46309 |  9497 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    23164 |  9498 | `			for(;;){` |
|        - |  9499 | `				SyBlob sResolved;` |
|        - |  9500 | `				SyString sIntName;` |
|        - |  9501 | `				sxu32 nRefLine;` |
|    46321 |  9502 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    46321 |  9503 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    46321 |  9504 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 |  9505 | `					SyBlobRelease(&sResolved);` |
|      ! 0 |  9506 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  9507 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|      ! 0 |  9508 | `						pName);` |
|      ! 0 |  9509 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9510 | `						return SXERR_ABORT;` |
|        - |  9511 | `					}` |
|      ! 0 |  9512 | `					break;` |
|        - |  9513 | `				}` |
|    92637 |  9514 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    46316 |  9515 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    46321 |  9516 | `				SyStringInitFromBuf(&sIntName,` |
|        - |  9517 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - |  9518 | `				/* Only interfaces are allowed */` |
|    46321 |  9519 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 |  9520 | `					pInterface = pInterface->pNextName;` |
|      ! 0 |  9521 | `				}` |
|    46321 |  9522 | `				if( pInterface == 0 ){` |
|      ! 0 |  9523 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - |  9524 | `						"Nonexistent base interface '%z'",&sIntName);` |
|      ! 0 |  9525 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9526 | `						SyBlobRelease(&sResolved);` |
|      ! 0 |  9527 | `						return SXERR_ABORT;` |
|        - |  9528 | `					}` |
|      ! 0 |  9529 | `				}else{` |
|        - |  9530 | `					/* Reject user classes that try to implement Throwable` |
|        - |  9531 | `					 * directly (or via an interface that extends Throwable)` |
|        - |  9532 | `					 * unless they already extend Exception or Error.` |
|        - |  9533 | `					 * Exception and Error themselves are compiled from the` |
|        - |  9534 | `					 * built-in library and are exempt by FQN — a namespaced` |
|        - |  9535 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    46321 |  9536 | `					SyString *pFqn = &pClass->sName;` |
|    46321 |  9537 | `					int bIsExceptionOrError =` |
|    27006 |  9538 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|    71400 |  9539 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    44401 |  9540 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|     3858 |  9541 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|    50165 |  9542 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|    11550 |  9543 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|     3847 |  9544 | `						!bIsExceptionOrError ){` |
|       12 |  9545 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  9546 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|        3 |  9547 | `							&pClass->sName);` |
|        9 |  9548 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  9549 | `							SyBlobRelease(&sResolved);` |
|      ! 0 |  9550 | `							return SXERR_ABORT;` |
|        - |  9551 | `						}` |
|        - |  9552 | `						/* Skip registration so the follow-up abstract-method` |
|        - |  9553 | `						 * check does not produce a duplicate fatal. */` |
|        6 |  9554 | `					}else{` |
|    46315 |  9555 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|        - |  9556 | `					}` |
|        - |  9557 | `				}` |
|    46321 |  9558 | `				SyBlobRelease(&sResolved);` |
|    46321 |  9559 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    23157 |  9560 | `					break;` |
|        - |  9561 | `				}` |
|       16 |  9562 | `				pGen->pIn++;/* Jump the comma */` |
|        4 |  9563 | `			}` |
|    23152 |  9564 | `		}` |
|    84750 |  9565 | `	}` |
|   212843 |  9566 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - |  9567 | `		/* Syntax error */` |
|      ! 0 |  9568 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|      ! 0 |  9569 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  9570 | `		if( rc == SXERR_ABORT ){` |
|        - |  9571 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  9572 | `			return SXERR_ABORT;` |
|        - |  9573 | `		}` |
|      ! 0 |  9574 | `		return SXRET_OK;` |
|        - |  9575 | `	}` |
|   212843 |  9576 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   212843 |  9577 | `	pEnd = 0; /* cc warning */` |
|        - |  9578 | `	/* Delimit the class body */` |
|   212843 |  9579 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   212843 |  9580 | `	if( pEnd >= pGen->pEnd ){` |
|        - |  9581 | `		/* Syntax error */` |
|      ! 0 |  9582 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|      ! 0 |  9583 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  9584 | `		if( rc == SXERR_ABORT ){` |
|        - |  9585 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  9586 | `			return SXERR_ABORT;` |
|        - |  9587 | `		}` |
|      ! 0 |  9588 | `		return SXRET_OK;` |
|        - |  9589 | `	}` |
|        - |  9590 | `	/* The delimiter token is the class body's closing brace */` |
|   212843 |  9591 | `	pClass->nEndLine = pEnd->nLine;` |
|        - |  9592 | `	/* Swap token stream */` |
|   212843 |  9593 | `	pTmp = pGen->pEnd;` |
|   212843 |  9594 | `	pGen->pEnd = pEnd;` |
|        - |  9595 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|   212843 |  9596 | `	pClass->iFlags \|= iFlags;` |
|        - |  9597 | `	/* Start the parse process */` |
|   814354 |  9598 | `	for(;;){` |
|        - |  9599 | `		/* Jump leading/trailing semi-colons */` |
|  2187801 |  9600 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   350717 |  9601 | `			pGen->pIn++;` |
|        5 |  9602 | `		}` |
|  1837089 |  9603 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - |  9604 | `			/* End of class body */` |
|   212805 |  9605 | `			break;` |
|        - |  9606 | `		}` |
|        - |  9607 | `		/* Bind a directly-preceding docblock to this member */` |
|  1624289 |  9608 | `		GenStateSetPendingDoc(&(*pGen));` |
|  1624284 |  9609 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|   812147 |  9610 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|      ! 0 |  9611 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  9612 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|      ! 0 |  9613 | `				&pGen->pIn->sData,pName);` |
|      ! 0 |  9614 | `			if( rc == SXERR_ABORT ){` |
|        - |  9615 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  9616 | `				return SXERR_ABORT;` |
|        - |  9617 | `			}` |
|      ! 0 |  9618 | `			goto done;` |
|        - |  9619 | `		}` |
|        - |  9620 | `		/* Assume public visibility */` |
|  1624289 |  9621 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  1624289 |  9622 | `		iAttrflags = 0;` |
|        - |  9623 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|        - |  9624 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|        - |  9625 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|        - |  9626 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  1624289 |  9627 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      ! 0 |  9628 | `			int bMod = 0;` |
|      ! 0 |  9629 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      ! 0 |  9630 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        - |  9631 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|        - |  9632 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|        - |  9633 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|        - |  9634 | `			 * that the generic keyword dispatch would misread as a method. */` |
|      ! 0 |  9635 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      ! 0 |  9636 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      ! 0 |  9637 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|      ! 0 |  9638 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|      ! 0 |  9639 | `			}` |
|      ! 0 |  9640 | `			if( !bMod ){` |
|      ! 0 |  9641 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 |  9642 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  9643 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9644 | `						return SXERR_ABORT;` |
|        - |  9645 | `					}` |
|      ! 0 |  9646 | `					goto done;` |
|        - |  9647 | `				}` |
|      ! 0 |  9648 | `				continue;` |
|        - |  9649 | `			}` |
|      ! 0 |  9650 | `		}` |
|  1624289 |  9651 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - |  9652 | `			/* Extract the current keyword */` |
|  1624289 |  9653 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1624289 |  9654 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - |  9655 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|        - |  9656 | `				TraitUseEntry sUse;` |
|       59 |  9657 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|       59 |  9658 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|       59 |  9659 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|       35 |  9660 | `				for(;;){` |
|        - |  9661 | `					ph7_class *pTrait;` |
|        - |  9662 | `					SyString *pTraitName;` |
|       67 |  9663 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 |  9664 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9665 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|      ! 0 |  9666 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  9667 | `							return SXERR_ABORT;` |
|        - |  9668 | `						}` |
|      ! 0 |  9669 | `						break;` |
|        - |  9670 | `					}` |
|       67 |  9671 | `					pTraitName = &pGen->pIn->sData;` |
|        - |  9672 | `					/* Resolve trait name through namespace/imports */ {` |
|        - |  9673 | `						SyBlob sResolved;` |
|       67 |  9674 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       67 |  9675 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|      129 |  9676 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|       62 |  9677 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       67 |  9678 | `						SyBlobRelease(&sResolved);` |
|        - |  9679 | `					}` |
|        - |  9680 | `					/* Only traits are allowed */` |
|       67 |  9681 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 |  9682 | `						pTrait = pTrait->pNextName;` |
|      ! 0 |  9683 | `					}` |
|       67 |  9684 | `					if( pTrait == 0 ){` |
|      ! 0 |  9685 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9686 | `							"'%z' is not a trait",pTraitName);` |
|      ! 0 |  9687 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  9688 | `							return SXERR_ABORT;` |
|        - |  9689 | `						}` |
|      ! 0 |  9690 | `					}else{` |
|       67 |  9691 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|        - |  9692 | `					}` |
|       67 |  9693 | `					pGen->pIn++; /* Advance past trait name */` |
|       67 |  9694 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       32 |  9695 | `						break;` |
|        - |  9696 | `					}` |
|       10 |  9697 | `					pGen->pIn++; /* Jump the comma */` |
|        2 |  9698 | `				}` |
|        - |  9699 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|       59 |  9700 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|        - |  9701 | `					SyToken *pBlock;` |
|       13 |  9702 | `					pGen->pIn++; /* Jump '{' */` |
|       13 |  9703 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       13 |  9704 | `					sUse.pResolvStart = pGen->pIn;` |
|       13 |  9705 | `					sUse.pResolvEnd = pBlock;` |
|       13 |  9706 | `					if( pBlock < pGen->pEnd ){` |
|       13 |  9707 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|        8 |  9708 | `					}else{` |
|      ! 0 |  9709 | `						pGen->pIn = pGen->pEnd;` |
|        - |  9710 | `					}` |
|        5 |  9711 | `				}` |
|       59 |  9712 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|        - |  9713 | `				/* The semicolon will be consumed by the outer loop */` |
|       59 |  9714 | `				continue;` |
|        - |  9715 | `			}` |
|  1624235 |  9716 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  1481641 |  9717 | `				iProtection = nKwrd;` |
|  1481641 |  9718 | `				pGen->pIn++; /* Jump the visibility token */` |
|        - |  9719 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`. */`` |
|  1481641 |  9720 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       22 |  9721 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       22 |  9722 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        9 |  9723 | `				}` |
|  1481636 |  9724 | `				if( pGen->pIn >= pGen->pEnd` |
|  1481641 |  9725 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 |  9726 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  9727 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|      ! 0 |  9728 | `						&pGen->pIn->sData,pName);` |
|      ! 0 |  9729 | `					if( rc == SXERR_ABORT ){` |
|        - |  9730 | `						/* Error count limit reached,abort immediately */` |
|      ! 0 |  9731 | `						return SXERR_ABORT;` |
|        - |  9732 | `					}` |
|      ! 0 |  9733 | `					goto done;` |
|        - |  9734 | `				}` |
|  1481641 |  9735 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        - |  9736 | `					/* Attribute declaration (untyped) */` |
|   208117 |  9737 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   208117 |  9738 | `					if( rc != SXRET_OK ){` |
|       11 |  9739 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  9740 | `							return SXERR_ABORT;` |
|        - |  9741 | `						}` |
|       11 |  9742 | `						goto done;` |
|        - |  9743 | `					}` |
|   208109 |  9744 | `					continue;` |
|        - |  9745 | `				}` |
|  1273529 |  9746 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        - |  9747 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|      187 |  9748 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      187 |  9749 | `					if( rc != SXRET_OK ){` |
|        8 |  9750 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  9751 | `							return SXERR_ABORT;` |
|        - |  9752 | `						}` |
|        8 |  9753 | `						goto done;` |
|        - |  9754 | `					}` |
|      181 |  9755 | `					continue;` |
|        - |  9756 | `				}` |
|        - |  9757 | `				/* Extract the keyword */` |
|  1273347 |  9758 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   636671 |  9759 | `			}` |
|  1415941 |  9760 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|        - |  9761 | `				/* Process constant declaration */` |
|   142341 |  9762 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|   142341 |  9763 | `				if( rc != SXRET_OK ){` |
|       11 |  9764 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9765 | `						return SXERR_ABORT;` |
|        - |  9766 | `					}` |
|       11 |  9767 | `					goto done;` |
|        - |  9768 | `				}` |
|    71169 |  9769 | `			}else{` |
|  1273605 |  9770 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|        - |  9771 | `					/* Static method or attribute,record that */` |
|    23161 |  9772 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    23161 |  9773 | `					pGen->pIn++; /* Jump the static keyword */` |
|    23161 |  9774 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - |  9775 | `						/* Extract the keyword */` |
|    23137 |  9776 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    23137 |  9777 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      ! 0 |  9778 | `							iProtection = nKwrd;` |
|      ! 0 |  9779 | `							pGen->pIn++; /* Jump the visibility token */` |
|      ! 0 |  9780 | `						}` |
|    11566 |  9781 | `					}` |
|        - |  9782 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|        - |  9783 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|        - |  9784 | `					 * than a generic "expecting method" parse error. */` |
|    23161 |  9785 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      ! 0 |  9786 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      ! 0 |  9787 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|      ! 0 |  9788 | `					}` |
|    23156 |  9789 | `					if( pGen->pIn >= pGen->pEnd` |
|    23161 |  9790 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 |  9791 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  9792 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|      ! 0 |  9793 | `							&pGen->pIn->sData,pName);` |
|      ! 0 |  9794 | `						if( rc == SXERR_ABORT ){` |
|        - |  9795 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 |  9796 | `							return SXERR_ABORT;` |
|        - |  9797 | `						}` |
|      ! 0 |  9798 | `						goto done;` |
|        - |  9799 | `					}` |
|    23161 |  9800 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        - |  9801 | `						/* Attribute declaration */` |
|       25 |  9802 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       25 |  9803 | `						if( rc != SXRET_OK ){` |
|        3 |  9804 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 |  9805 | `								return SXERR_ABORT;` |
|        - |  9806 | `							}` |
|        3 |  9807 | `							goto done;` |
|        - |  9808 | `						}` |
|       22 |  9809 | `						continue;` |
|        - |  9810 | `					}` |
|    23139 |  9811 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        - |  9812 | `						/* Typed static attribute declaration */` |
|       15 |  9813 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       15 |  9814 | `						if( rc != SXRET_OK ){` |
|        3 |  9815 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 |  9816 | `								return SXERR_ABORT;` |
|        - |  9817 | `							}` |
|        3 |  9818 | `							goto done;` |
|        - |  9819 | `						}` |
|       13 |  9820 | `						continue;` |
|        - |  9821 | `					}` |
|        - |  9822 | `					/* Extract the keyword */` |
|    23127 |  9823 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1262010 |  9824 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|        - |  9825 | `					/* Abstract method,record that */` |
|       15 |  9826 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|        - |  9827 | `					/* Mark the whole class as abstract */` |
|       15 |  9828 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|        - |  9829 | `					/* Advance the stream cursor */` |
|       15 |  9830 | `					pGen->pIn++;` |
|       15 |  9831 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       15 |  9832 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       15 |  9833 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       13 |  9834 | `							iProtection = nKwrd;` |
|       13 |  9835 | `							pGen->pIn++; /* Jump the visibility token */` |
|        5 |  9836 | `						}` |
|        6 |  9837 | `					}` |
|       15 |  9838 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       12 |  9839 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - |  9840 | `							/* Static method */` |
|      ! 0 |  9841 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 |  9842 | `							pGen->pIn++; /* Jump the static keyword */` |
|      ! 0 |  9843 | `					}` |
|       15 |  9844 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       12 |  9845 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 |  9846 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  9847 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|      ! 0 |  9848 | `								&pGen->pIn->sData,pName);` |
|      ! 0 |  9849 | `							if( rc == SXERR_ABORT ){` |
|        - |  9850 | `								/* Error count limit reached,abort immediately */` |
|      ! 0 |  9851 | `								return SXERR_ABORT;` |
|        - |  9852 | `							}` |
|      ! 0 |  9853 | `							goto done;` |
|        - |  9854 | `					}` |
|       15 |  9855 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  1250443 |  9856 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|        - |  9857 | `					/* final method ,record that */` |
|       21 |  9858 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       21 |  9859 | `					pGen->pIn++; /* Jump the final keyword */` |
|       21 |  9860 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - |  9861 | `						/* Extract the keyword */` |
|       21 |  9862 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       21 |  9863 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       11 |  9864 | `							iProtection = nKwrd;` |
|       11 |  9865 | `							pGen->pIn++; /* Jump the visibility token */` |
|        4 |  9866 | `						}` |
|        9 |  9867 | `					}` |
|       21 |  9868 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       18 |  9869 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|        - |  9870 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|        - |  9871 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|        - |  9872 | `							 * child class is compiled (PH7_ClassInherit). */` |
|       14 |  9873 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|       14 |  9874 | `							if( rc != SXRET_OK ){` |
|      ! 0 |  9875 | `								if( rc == SXERR_ABORT ){` |
|      ! 0 |  9876 | `									return SXERR_ABORT;` |
|        - |  9877 | `								}` |
|      ! 0 |  9878 | `								goto done;` |
|        - |  9879 | `							}` |
|       14 |  9880 | `							continue;` |
|        - |  9881 | `					}` |
|        9 |  9882 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        6 |  9883 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - |  9884 | `							/* Static method */` |
|      ! 0 |  9885 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 |  9886 | `							pGen->pIn++; /* Jump the static keyword */` |
|      ! 0 |  9887 | `					}` |
|        9 |  9888 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|        6 |  9889 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 |  9890 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  9891 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|      ! 0 |  9892 | `								&pGen->pIn->sData,pName);` |
|      ! 0 |  9893 | `							if( rc == SXERR_ABORT ){` |
|        - |  9894 | `								/* Error count limit reached,abort immediately */` |
|      ! 0 |  9895 | `								return SXERR_ABORT;` |
|        - |  9896 | `							}` |
|      ! 0 |  9897 | `							goto done;` |
|        - |  9898 | `					}` |
|        9 |  9899 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|        3 |  9900 | `				}` |
|  1273559 |  9901 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|      ! 0 |  9902 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  9903 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|      ! 0 |  9904 | `							&pGen->pIn->sData,pName);` |
|      ! 0 |  9905 | `						if( rc == SXERR_ABORT ){` |
|        - |  9906 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 |  9907 | `							return SXERR_ABORT;` |
|        - |  9908 | `						}` |
|      ! 0 |  9909 | `						goto done;` |
|        - |  9910 | `				}` |
|  1273559 |  9911 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|        7 |  9912 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|        7 |  9913 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|      ! 0 |  9914 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  9915 | `							"Expecting attribute declaration after 'var' keyword");` |
|      ! 0 |  9916 | `						if( rc == SXERR_ABORT ){` |
|        - |  9917 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 |  9918 | `							return SXERR_ABORT;` |
|        - |  9919 | `						}` |
|      ! 0 |  9920 | `						goto done;` |
|        - |  9921 | `					}` |
|        - |  9922 | `					/* Attribute declaration */` |
|        7 |  9923 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        4 |  9924 | `				}else{` |
|        - |  9925 | `					/* Process method declaration */` |
|  1273553 |  9926 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|        - |  9927 | `				}` |
|  1273559 |  9928 | `				if( rc != SXRET_OK ){` |
|       16 |  9929 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9930 | `						return SXERR_ABORT;` |
|        - |  9931 | `					}` |
|       16 |  9932 | `					goto done;` |
|        - |  9933 | `				}` |
|        - |  9934 | `			}` |
|   707940 |  9935 | `		}else{` |
|        - |  9936 | `			/* Attribute declaration */` |
|      ! 0 |  9937 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 |  9938 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9939 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9940 | `					return SXERR_ABORT;` |
|        - |  9941 | `				}` |
|      ! 0 |  9942 | `				goto done;` |
|        - |  9943 | `			}` |
|        - |  9944 | `		}` |
|        5 |  9945 | `	}` |
|        - |  9946 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|        - |  9947 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|        - |  9948 | `	 */` |
|        - |  9949 | `	{` |
|        - |  9950 | `		TraitUseEntry *apUse;` |
|        - |  9951 | `		sxu32 nU;` |
|   212805 |  9952 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   212859 |  9953 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|       59 |  9954 | `			TraitUseEntry *pUse = &apUse[nU];` |
|       59 |  9955 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|       59 |  9956 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|       59 |  9957 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|        - |  9958 | `			sxu32 nT;` |
|       59 |  9959 | `			if( !hasResolution ){` |
|        - |  9960 | `				/* No conflict resolution block: use standard trait application */` |
|       99 |  9961 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       55 |  9962 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|       55 |  9963 | `					if( rc != SXRET_OK ){` |
|      ! 0 |  9964 | `						break;` |
|        - |  9965 | `					}` |
|       30 |  9966 | `				}` |
|       27 |  9967 | `			}else{` |
|        - |  9968 | `				/* With resolution block: copy attributes, record traits,` |
|        - |  9969 | `				 * then use the block to resolve method conflicts.` |
|        - |  9970 | `				 */` |
|        - |  9971 | `				SyToken *pR;` |
|       25 |  9972 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       15 |  9973 | `					ph7_class *pTR = apTrait[nT];` |
|        - |  9974 | `					ph7_class_attr *pAR;` |
|        - |  9975 | `					SyHashEntry *pER;` |
|        - |  9976 | `					SyString *pNR;` |
|       15 |  9977 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|       21 |  9978 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|      ! 0 |  9979 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|      ! 0 |  9980 | `						pNR = &pAR->sName;` |
|      ! 0 |  9981 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      ! 0 |  9982 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|      ! 0 |  9983 | `						}` |
|      ! 0 |  9984 | `					}` |
|       15 |  9985 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|        9 |  9986 | `				}` |
|        - |  9987 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       13 |  9988 | `				pR = pUse->pResolvStart;` |
|       27 |  9989 | `				while( pR < pUse->pResolvEnd ){` |
|        - |  9990 | `					SyString sTrait,sMethod;` |
|        - |  9991 | `					ph7_class *pSrcTrait;` |
|        - |  9992 | `					ph7_class_method *pMeth;` |
|        - |  9993 | `					sxi32 nRKwrd;` |
|       41 |  9994 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|       27 |  9995 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       17 |  9996 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       17 |  9997 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       17 |  9998 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       17 |  9999 | `					sMethod = pR->sData;` |
|       17 | 10000 | `					pR++;` |
|       17 | 10001 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        7 | 10002 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        7 | 10003 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        7 | 10004 | `							sTrait = sMethod;` |
|        7 | 10005 | `							pR++;` |
|        7 | 10006 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        7 | 10007 | `							sMethod = pR->sData;` |
|        7 | 10008 | `							pR++;` |
|        3 | 10009 | `						}` |
|        3 | 10010 | `					}` |
|       17 | 10011 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 10012 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 10013 | `						continue;` |
|        - | 10014 | `					}` |
|       17 | 10015 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       17 | 10016 | `					pR++;` |
|       17 | 10017 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|        5 | 10018 | `						pSrcTrait = 0;` |
|        7 | 10019 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        7 | 10020 | `							SyString *pTN = &apTrait[nT]->sName;` |
|       10 | 10021 | `							if( pTN->nByte >= sTrait.nByte &&` |
|        6 | 10022 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|        5 | 10023 | `								pSrcTrait = apTrait[nT];` |
|        5 | 10024 | `								break;` |
|        - | 10025 | `							}` |
|        2 | 10026 | `						}` |
|        5 | 10027 | `						if( pSrcTrait ){` |
|        5 | 10028 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        5 | 10029 | `							if( pMeth ){` |
|        5 | 10030 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|        5 | 10031 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|        5 | 10032 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|        2 | 10033 | `								}` |
|        2 | 10034 | `							}` |
|        2 | 10035 | `						}` |
|        2 | 10036 | `					}` |
|       35 | 10037 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        3 | 10038 | `				}` |
|        - | 10039 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|       25 | 10040 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        - | 10041 | `					ph7_class_method *pMR;` |
|        - | 10042 | `					SyHashEntry *pER;` |
|        - | 10043 | `					SyString *pNR;` |
|       15 | 10044 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|       41 | 10045 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|       23 | 10046 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|       23 | 10047 | `						pNR = &pMR->sFunc.sName;` |
|       23 | 10048 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       14 | 10049 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|        6 | 10050 | `						}` |
|        3 | 10051 | `					}` |
|        9 | 10052 | `				}` |
|        - | 10053 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       13 | 10054 | `				pR = pUse->pResolvStart;` |
|       27 | 10055 | `				while( pR < pUse->pResolvEnd ){` |
|        - | 10056 | `					SyString sTrait,sMethod,sAlias;` |
|        - | 10057 | `					ph7_class *pSrcTrait;` |
|        - | 10058 | `					ph7_class_method *pMeth;` |
|       27 | 10059 | `					int hasQual = 0;` |
|        - | 10060 | `					sxi32 nRKwrd;` |
|       41 | 10061 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|       27 | 10062 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       17 | 10063 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       17 | 10064 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       17 | 10065 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|       17 | 10066 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       17 | 10067 | `					sMethod = pR->sData;` |
|       17 | 10068 | `					pR++;` |
|       17 | 10069 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        7 | 10070 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        7 | 10071 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        7 | 10072 | `							sTrait = sMethod;` |
|        7 | 10073 | `							hasQual = 1;` |
|        7 | 10074 | `							pR++;` |
|        7 | 10075 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        7 | 10076 | `							sMethod = pR->sData;` |
|        7 | 10077 | `							pR++;` |
|        3 | 10078 | `						}` |
|        3 | 10079 | `					}` |
|       17 | 10080 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 10081 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 10082 | `						continue;` |
|        - | 10083 | `					}` |
|       17 | 10084 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       17 | 10085 | `					pR++;` |
|       17 | 10086 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       13 | 10087 | `						sxi32 iNewVis = -1;` |
|       13 | 10088 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|        7 | 10089 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|        7 | 10090 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|        7 | 10091 | `								iNewVis = nAK;` |
|        7 | 10092 | `								pR++;` |
|        3 | 10093 | `							}` |
|        3 | 10094 | `						}` |
|       13 | 10095 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       11 | 10096 | `							sAlias = pR->sData;` |
|       11 | 10097 | `							pR++;` |
|        4 | 10098 | `						}` |
|       13 | 10099 | `						pMeth = 0;` |
|       13 | 10100 | `						if( hasQual ){` |
|        3 | 10101 | `							pSrcTrait = 0;` |
|        5 | 10102 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        5 | 10103 | `								SyString *pTN = &apTrait[nT]->sName;` |
|        7 | 10104 | `								if( pTN->nByte >= sTrait.nByte &&` |
|        4 | 10105 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|        3 | 10106 | `									pSrcTrait = apTrait[nT];` |
|        3 | 10107 | `									break;` |
|        - | 10108 | `								}` |
|        2 | 10109 | `							}` |
|        3 | 10110 | `							if( pSrcTrait ){` |
|        3 | 10111 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        1 | 10112 | `							}` |
|        2 | 10113 | `						}else{` |
|       10 | 10114 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|        - | 10115 | `						}` |
|       13 | 10116 | `						if( pMeth ){` |
|       13 | 10117 | `							if( sAlias.nByte > 0 ){` |
|        - | 10118 | `								/* Create a shallow copy of the method struct for the alias` |
|        - | 10119 | `								 * so it can carry its own visibility without affecting the original.` |
|        - | 10120 | `								 */` |
|        - | 10121 | `								ph7_class_method *pAlias;` |
|        - | 10122 | `								char *zAliasDup;` |
|       11 | 10123 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       11 | 10124 | `								if( pAlias ){` |
|       11 | 10125 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       11 | 10126 | `									if( iNewVis >= 0 ){` |
|        5 | 10127 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|        3 | 10128 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|      ! 0 | 10129 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|        2 | 10130 | `									}` |
|       11 | 10131 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       11 | 10132 | `									if( zAliasDup ){` |
|       11 | 10133 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|        4 | 10134 | `									}` |
|        7 | 10135 | `								}` |
|        7 | 10136 | `							}else if( iNewVis >= 0 ){` |
|        - | 10137 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|        - | 10138 | `								ph7_class_method *pCopy;` |
|        3 | 10139 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        3 | 10140 | `								if( pCopy ){` |
|        3 | 10141 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|        3 | 10142 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|        3 | 10143 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|        3 | 10144 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|      ! 0 | 10145 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|        - | 10146 | `									/* Replace the method in the class hash */` |
|        3 | 10147 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|        3 | 10148 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|        1 | 10149 | `								}` |
|        1 | 10150 | `							}` |
|        5 | 10151 | `						}` |
|        5 | 10152 | `						SXUNUSED(hasQual);` |
|        5 | 10153 | `					}` |
|       21 | 10154 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        3 | 10155 | `				}` |
|        - | 10156 | `			}` |
|       59 | 10157 | `			SySetRelease(&pUse->aTraits);` |
|       32 | 10158 | `		}` |
|        - | 10159 | `	}` |
|        - | 10160 | `	/* Install the class */` |
|   212805 | 10161 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   212805 | 10162 | `	if( rc == SXRET_OK ){` |
|        - | 10163 | `		ph7_class **apInterface;` |
|        - | 10164 | `		sxu32 n;` |
|   212805 | 10165 | `		if( pBase ){` |
|        - | 10166 | `			/* Inherit from base class and mark as a subclass */` |
|   123213 | 10167 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|    61604 | 10168 | `		}` |
|   212805 | 10169 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   259115 | 10170 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|        - | 10171 | `			/* Implements one or more interface */` |
|    46315 | 10172 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    46315 | 10173 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 10174 | `				break;` |
|        - | 10175 | `			}` |
|    23160 | 10176 | `		}` |
|        - | 10177 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|        - | 10178 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|   212800 | 10179 | `		if( rc == SXRET_OK` |
|   212800 | 10180 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|   212805 | 10181 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   169231 | 10182 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|        - | 10183 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   169231 | 10184 | `			if( pStringable ){` |
|   169231 | 10185 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   169231 | 10186 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|        - | 10187 | `				sxu32 i;` |
|   169231 | 10188 | `				int bAlready = 0;` |
|   207675 | 10189 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    42295 | 10190 | `					if( apImpl[i] == pStringable ){` |
|     3851 | 10191 | `						bAlready = 1;` |
|     3851 | 10192 | `						break;` |
|        - | 10193 | `					}` |
|    19227 | 10194 | `				}` |
|   169231 | 10195 | `				if( !bAlready ){` |
|   165385 | 10196 | `					PH7_ClassImplement(pClass,pStringable);` |
|    82690 | 10197 | `				}` |
|    84613 | 10198 | `			}` |
|    84613 | 10199 | `		}` |
|        - | 10200 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   212805 | 10201 | `		if( rc == SXRET_OK ){` |
|   212805 | 10202 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   212805 | 10203 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 10204 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 10205 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 10206 | `				return SXERR_ABORT;` |
|        - | 10207 | `			}` |
|   106400 | 10208 | `		}` |
|        - | 10209 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   212805 | 10210 | `		if( rc == SXRET_OK ){` |
|   212805 | 10211 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   212805 | 10212 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 10213 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 10214 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 10215 | `				return SXERR_ABORT;` |
|        - | 10216 | `			}` |
|   106400 | 10217 | `		}` |
|   106400 | 10218 | `	}` |
|   212805 | 10219 | `	SySetRelease(&aUseEntries);` |
|   212805 | 10220 | `	SySetRelease(&aInterfaces);` |
|   212805 | 10221 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10222 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 10223 | `		return SXERR_ABORT;` |
|        - | 10224 | `	}` |
|   106400 | 10225 | `done:` |
|        - | 10226 | `	/* Point beyond the class body */` |
|   212843 | 10227 | `	pGen->pIn = &pEnd[1];` |
|   212843 | 10228 | `	pGen->pEnd = pTmp;` |
|   212843 | 10229 | `	return PH7_OK;` |
|   106425 | 10230 | `}` |
|        - | 10231 | `/* Compile a named class declaration (the common case). */` |
|   212814 | 10232 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|        5 | 10233 | `{` |
|   212819 | 10234 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|        5 | 10235 | `}` |
|        - | 10236 | `/*` |
|        - | 10237 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|        - | 10238 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|        - | 10239 | ` * compile + install the class body once (at compile time, like every other` |
|        - | 10240 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|        - | 10241 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|        - | 10242 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|        - | 10243 | ` */` |
|       26 | 10244 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        4 | 10245 | `{` |
|        - | 10246 | `	char zName[128];         /* Synthesized class name */` |
|        - | 10247 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|        - | 10248 | `	SyString sName;` |
|        - | 10249 | `	SyToken *pArgStart,*pArgEnd;` |
|        - | 10250 | `	ph7_value *pObj;` |
|       30 | 10251 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 10252 | `	sxu32 nIdx,nLen;` |
|        - | 10253 | `	sxi32 nArg,rc;` |
|       13 | 10254 | `	SXUNUSED(iCompileFlag);` |
|        - | 10255 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|       30 | 10256 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|       30 | 10257 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 | 10258 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      ! 0 | 10259 | `	}` |
|       30 | 10260 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|        - | 10261 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|        - | 10262 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|        - | 10263 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|       30 | 10264 | `	pArgStart = pArgEnd = 0;` |
|       30 | 10265 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|       30 | 10266 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10267 | `		return rc;` |
|        - | 10268 | `	}` |
|        - | 10269 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|        - | 10270 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|       30 | 10271 | `	nArg = 0;` |
|       30 | 10272 | `	if( pArgStart < pArgEnd ){` |
|        7 | 10273 | `		SyToken *pSavedIn = pGen->pIn;` |
|        7 | 10274 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|        - | 10275 | `		SyToken *pArgNext;` |
|        7 | 10276 | `		pGen->pIn = pArgStart;` |
|        7 | 10277 | `		pGen->pEnd = pArgEnd;` |
|       13 | 10278 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|        7 | 10279 | `			if( pGen->pIn < pArgNext ){` |
|        7 | 10280 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|        7 | 10281 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10282 | `					pGen->pIn = pSavedIn;` |
|      ! 0 | 10283 | `					pGen->pEnd = pSavedEnd;` |
|      ! 0 | 10284 | `					return SXERR_ABORT;` |
|        - | 10285 | `				}` |
|        7 | 10286 | `				nArg++;` |
|        3 | 10287 | `			}` |
|        7 | 10288 | `			pGen->pIn = &pArgNext[1];` |
|        1 | 10289 | `		}` |
|        7 | 10290 | `		pGen->pIn = pSavedIn;` |
|        7 | 10291 | `		pGen->pEnd = pSavedEnd;` |
|        3 | 10292 | `	}` |
|        - | 10293 | `	/* Load the synthesized class name */` |
|       30 | 10294 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       30 | 10295 | `	if( pObj == 0 ){` |
|      ! 0 | 10296 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 10297 | `		return SXERR_ABORT;` |
|        - | 10298 | `	}` |
|       30 | 10299 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       30 | 10300 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - | 10301 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|       30 | 10302 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       30 | 10303 | `	return SXRET_OK;` |
|       17 | 10304 | `}` |
|        - | 10305 | `/*` |
|        - | 10306 | ` * Compile a user-defined abstract class.` |
|        - | 10307 | ` *  According to the PHP language reference manual` |
|        - | 10308 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|        - | 10309 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|        - | 10310 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|        - | 10311 | ` *   the method's signature - they cannot define the implementation.` |
|        - | 10312 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|        - | 10313 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|        - | 10314 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|        - | 10315 | ` *   method is defined as protected, the function implementation must be defined as either` |
|        - | 10316 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|        - | 10317 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|        - | 10318 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|        - | 10319 | ` *   could differ.` |
|        - | 10320 | ` */` |
|        - | 10321 | `/*` |
|        - | 10322 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|        - | 10323 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|        - | 10324 | ` * receives the corresponding PH7_CLASS_* bit.` |
|        - | 10325 | ` */` |
|  6134022 | 10326 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|        5 | 10327 | `{` |
|  6134027 | 10328 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  3819989 | 10329 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  3819989 | 10330 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  3781523 | 10331 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  1883036 | 10332 | `	}` |
|  6080115 | 10333 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  6080055 | 10334 | `	return FALSE;` |
|  3067016 | 10335 | `}` |
|        - | 10336 | `/*` |
|        - | 10337 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|        - | 10338 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|        - | 10339 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|        - | 10340 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|        - | 10341 | ` */` |
|  6080050 | 10342 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|        5 | 10343 | `{` |
|  6080055 | 10344 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  6080055 | 10345 | `	sxi32 iFlags = 0,iFlag;` |
|  6134027 | 10346 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|    53977 | 10347 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|        5 | 10348 | `			pDup = pIn;` |
|        2 | 10349 | `		}` |
|    53977 | 10350 | `		iFlags \|= iFlag;` |
|    53977 | 10351 | `		pIn++;` |
|        5 | 10352 | `	}` |
|  6080055 | 10353 | `	*ppIn = pIn;` |
|  6080055 | 10354 | `	if( ppDup ){ *ppDup = pDup; }` |
|  6080055 | 10355 | `	return iFlags;` |
|        5 | 10356 | `}` |
|        - | 10357 | `/*` |
|        - | 10358 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|        - | 10359 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|        - | 10360 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|        - | 10361 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|        - | 10362 | `` * `readonly`) to their existing handlers.`` |
|        - | 10363 | ` */` |
|  6053074 | 10364 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|        5 | 10365 | `{` |
|  6053079 | 10366 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  3053520 | 10367 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  6066564 | 10368 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|        5 | 10369 | `}` |
|        - | 10370 | `/*` |
|        - | 10371 | ` * Compile a class declaration carrying one or more leading modifiers` |
|        - | 10372 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|        - | 10373 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|        - | 10374 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|        - | 10375 | `` * `abstract`+`final` pair, like PHP.`` |
|        - | 10376 | ` */` |
|    26976 | 10377 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|        5 | 10378 | `{` |
|        - | 10379 | `	SyToken *pDup;` |
|    26981 | 10380 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|        - | 10381 | `	sxi32 rc;` |
|    26981 | 10382 | `	if( pDup ){` |
|        4 | 10383 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|        2 | 10384 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|        3 | 10385 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10386 | `			return SXERR_ABORT;` |
|        - | 10387 | `		}` |
|        1 | 10388 | `	}` |
|    26976 | 10389 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|    13493 | 10390 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|        3 | 10391 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10392 | `			"Cannot use the final modifier on an abstract class");` |
|        3 | 10393 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10394 | `			return SXERR_ABORT;` |
|        - | 10395 | `		}` |
|        1 | 10396 | `	}` |
|    26981 | 10397 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|    13493 | 10398 | `}` |
|        - | 10399 | `/*` |
|        - | 10400 | ` * Compile a user-defined trait.` |
|        - | 10401 | ` *  Traits are similar to classes, but only intended to group functionality` |
|        - | 10402 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|        - | 10403 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|        - | 10404 | ` */` |
|       68 | 10405 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|        5 | 10406 | `{` |
|       73 | 10407 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 10408 | `	ph7_class *pClass;` |
|        - | 10409 | `	SyToken *pEnd,*pTmp;` |
|        - | 10410 | `	sxi32 iProtection;` |
|        - | 10411 | `	sxi32 iAttrflags;` |
|        - | 10412 | `	SyString *pName;` |
|        - | 10413 | `	sxi32 nKwrd;` |
|        - | 10414 | `	sxi32 rc;` |
|        - | 10415 | `	/* Jump the 'trait' keyword */` |
|       73 | 10416 | `	pGen->pIn++;` |
|       73 | 10417 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 10418 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|      ! 0 | 10419 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10420 | `			return SXERR_ABORT;` |
|        - | 10421 | `		}` |
|      ! 0 | 10422 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|      ! 0 | 10423 | `			pGen->pIn++;` |
|      ! 0 | 10424 | `		}` |
|      ! 0 | 10425 | `		return SXRET_OK;` |
|        - | 10426 | `	}` |
|        - | 10427 | `	/* Extract trait name */` |
|       73 | 10428 | `	pName = &pGen->pIn->sData;` |
|       73 | 10429 | `	pGen->pIn++;` |
|        - | 10430 | `	/* Build FQN and obtain a raw class */ {` |
|        - | 10431 | `		SyBlob sFQN;` |
|        - | 10432 | `		SyString sFQNStr;` |
|       73 | 10433 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       73 | 10434 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|       73 | 10435 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       73 | 10436 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|       73 | 10437 | `		SyBlobRelease(&sFQN);` |
|        - | 10438 | `	}` |
|       73 | 10439 | `	if( pClass == 0 ){` |
|      ! 0 | 10440 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 10441 | `		return SXERR_ABORT;` |
|        - | 10442 | `	}` |
|       73 | 10443 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|       73 | 10444 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 10445 | `		return SXERR_ABORT;` |
|        - | 10446 | `	}` |
|        - | 10447 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|       73 | 10448 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|      ! 0 | 10449 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|      ! 0 | 10450 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 10451 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10452 | `			return SXERR_ABORT;` |
|        - | 10453 | `		}` |
|      ! 0 | 10454 | `		return SXRET_OK;` |
|        - | 10455 | `	}` |
|       73 | 10456 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|       73 | 10457 | `	pEnd = 0;` |
|       73 | 10458 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|       73 | 10459 | `	if( pEnd >= pGen->pEnd ){` |
|      ! 0 | 10460 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|      ! 0 | 10461 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 10462 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10463 | `			return SXERR_ABORT;` |
|        - | 10464 | `		}` |
|      ! 0 | 10465 | `		return SXRET_OK;` |
|        - | 10466 | `	}` |
|        - | 10467 | `	/* The delimiter token is the trait body's closing brace */` |
|       73 | 10468 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 10469 | `	/* Swap token stream */` |
|       73 | 10470 | `	pTmp = pGen->pEnd;` |
|       73 | 10471 | `	pGen->pEnd = pEnd;` |
|        - | 10472 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|       73 | 10473 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|        - | 10474 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|       67 | 10475 | `	for(;;){` |
|      183 | 10476 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       28 | 10477 | `			pGen->pIn++;` |
|        4 | 10478 | `		}` |
|      159 | 10479 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       73 | 10480 | `			break;` |
|        - | 10481 | `		}` |
|        - | 10482 | `		/* Bind a directly-preceding docblock to this member */` |
|       91 | 10483 | `		GenStateSetPendingDoc(&(*pGen));` |
|       91 | 10484 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|      ! 0 | 10485 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10486 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|      ! 0 | 10487 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 10488 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 10489 | `				return SXERR_ABORT;` |
|        - | 10490 | `			}` |
|      ! 0 | 10491 | `			goto done;` |
|        - | 10492 | `		}` |
|       91 | 10493 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|       91 | 10494 | `		iAttrflags = 0;` |
|       91 | 10495 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       91 | 10496 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       91 | 10497 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - | 10498 | `				/* Trait uses another trait: use OtherTrait; */` |
|        5 | 10499 | `				pGen->pIn++; /* Jump 'use' */` |
|        2 | 10500 | `				for(;;){` |
|        - | 10501 | `					ph7_class *pUsedTrait;` |
|        - | 10502 | `					SyString *pUsedName;` |
|        5 | 10503 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 10504 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 10505 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|      ! 0 | 10506 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10507 | `							return SXERR_ABORT;` |
|        - | 10508 | `						}` |
|      ! 0 | 10509 | `						break;` |
|        - | 10510 | `					}` |
|        5 | 10511 | `					pUsedName = &pGen->pIn->sData;` |
|        - | 10512 | `					{` |
|        - | 10513 | `						SyBlob sResolved;` |
|        5 | 10514 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|        5 | 10515 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|        7 | 10516 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|        4 | 10517 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|        5 | 10518 | `						SyBlobRelease(&sResolved);` |
|        - | 10519 | `					}` |
|        5 | 10520 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 | 10521 | `						pUsedTrait = pUsedTrait->pNextName;` |
|      ! 0 | 10522 | `					}` |
|        5 | 10523 | `					if( pUsedTrait == 0 ){` |
|        4 | 10524 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        1 | 10525 | `							"'%z' is not a trait",pUsedName);` |
|        3 | 10526 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10527 | `							return SXERR_ABORT;` |
|        - | 10528 | `						}` |
|        2 | 10529 | `					}else{` |
|        3 | 10530 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|        - | 10531 | `					}` |
|        5 | 10532 | `					pGen->pIn++;` |
|        5 | 10533 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|        3 | 10534 | `						break;` |
|        - | 10535 | `					}` |
|      ! 0 | 10536 | `					pGen->pIn++;` |
|      ! 0 | 10537 | `				}` |
|        5 | 10538 | `				continue;` |
|        - | 10539 | `			}` |
|       87 | 10540 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       75 | 10541 | `				iProtection = nKwrd;` |
|       75 | 10542 | `				pGen->pIn++;` |
|       70 | 10543 | `				if( pGen->pIn >= pGen->pEnd` |
|       75 | 10544 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 10545 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10546 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|      ! 0 | 10547 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 10548 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10549 | `						return SXERR_ABORT;` |
|        - | 10550 | `					}` |
|      ! 0 | 10551 | `					goto done;` |
|        - | 10552 | `				}` |
|       75 | 10553 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       12 | 10554 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       12 | 10555 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 10556 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10557 | `							return SXERR_ABORT;` |
|        - | 10558 | `						}` |
|      ! 0 | 10559 | `						goto done;` |
|        - | 10560 | `					}` |
|       12 | 10561 | `					continue;` |
|        - | 10562 | `				}` |
|       65 | 10563 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        5 | 10564 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        5 | 10565 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 10566 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10567 | `							return SXERR_ABORT;` |
|        - | 10568 | `						}` |
|      ! 0 | 10569 | `						goto done;` |
|        - | 10570 | `					}` |
|        5 | 10571 | `					continue;` |
|        - | 10572 | `				}` |
|       61 | 10573 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       28 | 10574 | `			}` |
|       73 | 10575 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|      ! 0 | 10576 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10577 | `					"Traits cannot have constants");` |
|      ! 0 | 10578 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10579 | `					return SXERR_ABORT;` |
|        - | 10580 | `				}` |
|      ! 0 | 10581 | `				goto done;` |
|      ! 0 | 10582 | `			}else{` |
|       73 | 10583 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|        5 | 10584 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|        5 | 10585 | `					pGen->pIn++;` |
|        5 | 10586 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        3 | 10587 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        3 | 10588 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      ! 0 | 10589 | `							iProtection = nKwrd;` |
|      ! 0 | 10590 | `							pGen->pIn++;` |
|      ! 0 | 10591 | `						}` |
|        1 | 10592 | `					}` |
|        4 | 10593 | `					if( pGen->pIn >= pGen->pEnd` |
|        5 | 10594 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 10595 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10596 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|      ! 0 | 10597 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 10598 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10599 | `							return SXERR_ABORT;` |
|        - | 10600 | `						}` |
|      ! 0 | 10601 | `						goto done;` |
|        - | 10602 | `					}` |
|        5 | 10603 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        3 | 10604 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        3 | 10605 | `						if( rc != SXRET_OK ){` |
|      ! 0 | 10606 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 10607 | `								return SXERR_ABORT;` |
|        - | 10608 | `							}` |
|      ! 0 | 10609 | `							goto done;` |
|        - | 10610 | `						}` |
|        3 | 10611 | `						continue;` |
|        - | 10612 | `					}` |
|        3 | 10613 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|      ! 0 | 10614 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 10615 | `						if( rc != SXRET_OK ){` |
|      ! 0 | 10616 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 10617 | `								return SXERR_ABORT;` |
|        - | 10618 | `							}` |
|      ! 0 | 10619 | `							goto done;` |
|        - | 10620 | `						}` |
|      ! 0 | 10621 | `						continue;` |
|        - | 10622 | `					}` |
|        3 | 10623 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       70 | 10624 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|        6 | 10625 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|        6 | 10626 | `					pGen->pIn++;` |
|        6 | 10627 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        6 | 10628 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        6 | 10629 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        6 | 10630 | `							iProtection = nKwrd;` |
|        6 | 10631 | `							pGen->pIn++;` |
|        2 | 10632 | `						}` |
|        2 | 10633 | `					}` |
|        6 | 10634 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|        4 | 10635 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 10636 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10637 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|      ! 0 | 10638 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 10639 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10640 | `							return SXERR_ABORT;` |
|        - | 10641 | `						}` |
|      ! 0 | 10642 | `						goto done;` |
|        - | 10643 | `					}` |
|        6 | 10644 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|        2 | 10645 | `				}` |
|       71 | 10646 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|      ! 0 | 10647 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10648 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|      ! 0 | 10649 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 10650 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10651 | `						return SXERR_ABORT;` |
|        - | 10652 | `					}` |
|      ! 0 | 10653 | `					goto done;` |
|        - | 10654 | `				}` |
|       71 | 10655 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|      ! 0 | 10656 | `					pGen->pIn++;` |
|      ! 0 | 10657 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 10658 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10659 | `							"Expecting attribute declaration after 'var' keyword");` |
|      ! 0 | 10660 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10661 | `							return SXERR_ABORT;` |
|        - | 10662 | `						}` |
|      ! 0 | 10663 | `						goto done;` |
|        - | 10664 | `					}` |
|      ! 0 | 10665 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 10666 | `				}else{` |
|       71 | 10667 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|        - | 10668 | `				}` |
|       71 | 10669 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 10670 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10671 | `						return SXERR_ABORT;` |
|        - | 10672 | `					}` |
|      ! 0 | 10673 | `					goto done;` |
|        - | 10674 | `				}` |
|        - | 10675 | `			}` |
|       38 | 10676 | `		}else{` |
|      ! 0 | 10677 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 10678 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 10679 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10680 | `					return SXERR_ABORT;` |
|        - | 10681 | `				}` |
|      ! 0 | 10682 | `				goto done;` |
|        - | 10683 | `			}` |
|        - | 10684 | `		}` |
|        5 | 10685 | `	}` |
|        - | 10686 | `	/* Install the trait */` |
|       73 | 10687 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|       73 | 10688 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10689 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 10690 | `		return SXERR_ABORT;` |
|        - | 10691 | `	}` |
|       34 | 10692 | `done:` |
|        - | 10693 | `	/* Point beyond the trait body */` |
|       73 | 10694 | `	pGen->pIn = &pEnd[1];` |
|       73 | 10695 | `	pGen->pEnd = pTmp;` |
|       73 | 10696 | `	return PH7_OK;` |
|       39 | 10697 | `}` |
|        - | 10698 | `/*` |
|        - | 10699 | ` * Compile a user-defined class.` |
|        - | 10700 | ` *  According to the PHP language reference manual` |
|        - | 10701 | ` *   Basic class definitions begin with the keyword class, followed` |
|        - | 10702 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|        - | 10703 | ` *   the definitions of the properties and methods belonging to the class.` |
|        - | 10704 | ` *   A class may contain its own constants, variables (called "properties")` |
|        - | 10705 | ` *   and functions (called "methods").` |
|        - | 10706 | ` */` |
|   185838 | 10707 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|        5 | 10708 | `{` |
|        - | 10709 | `	sxi32 rc;` |
|   185843 | 10710 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   185843 | 10711 | `	return rc;` |
|        5 | 10712 | `}` |
|        - | 10713 | `/*` |
|        - | 10714 | ` * Exception handling.` |
|        - | 10715 | ` *  According to the PHP language reference manual` |
|        - | 10716 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|        - | 10717 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|        - | 10718 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|        - | 10719 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|        - | 10720 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|        - | 10721 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|        - | 10722 | ` *    (or re-thrown) within a catch block.` |
|        - | 10723 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|        - | 10724 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|        - | 10725 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|        - | 10726 | ` *    been defined with set_exception_handler().` |
|        - | 10727 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|        - | 10728 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|        - | 10729 | ` */` |
|        - | 10730 | `/*` |
|        - | 10731 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|        - | 10732 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|        - | 10733 | ` * indicates failure.` |
|        - | 10734 | ` */` |
|   307896 | 10735 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 10736 | `{` |
|   307901 | 10737 | `	sxi32 rc = SXRET_OK;` |
|   307901 | 10738 | `	if( pRoot->pOp ){` |
|   307889 | 10739 | `		switch( pRoot->pOp->iOp ){` |
|   153942 | 10740 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|        - | 10741 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|        - | 10742 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|        - | 10743 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|        - | 10744 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|        - | 10745 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|   307889 | 10746 | `			break;` |
|      ! 0 | 10747 | `		default:` |
|        - | 10748 | `			/* Runtime will still reject non-Throwable values; the set above` |
|        - | 10749 | `			 * covers the common shapes and gives a friendlier compile error` |
|        - | 10750 | ``			 * for obvious mistakes like `throw 5`. */`` |
|      ! 0 | 10751 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 10752 | `				"throw: Expecting an exception class instance");` |
|      ! 0 | 10753 | `			if( rc != SXERR_ABORT ){` |
|      ! 0 | 10754 | `				rc = SXERR_INVALID;` |
|      ! 0 | 10755 | `			}` |
|      ! 0 | 10756 | `			break;` |
|        - | 10757 | `		}` |
|   153959 | 10758 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 10759 | `		/* Unexpected expression */` |
|      ! 0 | 10760 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 10761 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 10762 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 | 10763 | `			rc = SXERR_INVALID;` |
|      ! 0 | 10764 | `		}` |
|      ! 0 | 10765 | `	}` |
|   307901 | 10766 | `	return rc;` |
|        5 | 10767 | `}` |
|        - | 10768 | `/*` |
|        - | 10769 | ` * Compile a 'throw' statement.` |
|        - | 10770 | ` * throw: This is how you trigger an exception.` |
|        - | 10771 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|        - | 10772 | ` */` |
|   307860 | 10773 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|        5 | 10774 | `{` |
|   307865 | 10775 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 10776 | `	GenBlock *pBlock;` |
|        - | 10777 | `	sxu32 nIdx;` |
|        - | 10778 | `	sxi32 rc;` |
|   307865 | 10779 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|        - | 10780 | `	/* Compile the expression */` |
|   307865 | 10781 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   307865 | 10782 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 10783 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|      ! 0 | 10784 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10785 | `			return SXERR_ABORT;` |
|        - | 10786 | `		}` |
|      ! 0 | 10787 | `		return SXRET_OK;` |
|        - | 10788 | `	}` |
|   307865 | 10789 | `	pBlock = pGen->pCurrent;` |
|        - | 10790 | `	/* Point to the top most function or try block and emit the forward jump */` |
|  1176965 | 10791 | `	while(pBlock->pParent){` |
|  1176961 | 10792 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   307861 | 10793 | `			break;` |
|        - | 10794 | `		}` |
|        - | 10795 | `		/* Point to the parent block */` |
|   869105 | 10796 | `		pBlock = pBlock->pParent;` |
|        5 | 10797 | `	}` |
|        - | 10798 | `	/* Emit the throw instruction */` |
|   307865 | 10799 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        - | 10800 | `	/* Emit the jump */` |
|   307865 | 10801 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   307865 | 10802 | `	return SXRET_OK;` |
|   153935 | 10803 | `}` |
|        - | 10804 | `/*` |
|        - | 10805 | ` * Compile a PHP 8.0 'throw' expression.` |
|        - | 10806 | ` * Called from the expression code generator when a 'throw' keyword is` |
|        - | 10807 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|        - | 10808 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|        - | 10809 | ` * the validator guarantees the operand is a valid exception target.` |
|        - | 10810 | ` */` |
|       36 | 10811 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        2 | 10812 | `{` |
|       38 | 10813 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 10814 | `	GenBlock *pBlock;` |
|        - | 10815 | `	sxu32 nIdx;` |
|        - | 10816 | `	sxi32 rc;` |
|       18 | 10817 | `	(void)iCompileFlag;` |
|       38 | 10818 | `	pGen->pIn++; /* Skip 'throw' */` |
|       38 | 10819 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 10820 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 10821 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 10822 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10823 | `			return SXERR_ABORT;` |
|        - | 10824 | `		}` |
|      ! 0 | 10825 | `		return SXRET_OK;` |
|        - | 10826 | `	}` |
|       38 | 10827 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|       38 | 10828 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 10829 | `		return SXERR_ABORT;` |
|        - | 10830 | `	}` |
|       38 | 10831 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 10832 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 10833 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 10834 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10835 | `			return SXERR_ABORT;` |
|        - | 10836 | `		}` |
|      ! 0 | 10837 | `		return SXRET_OK;` |
|        - | 10838 | `	}` |
|        - | 10839 | `	/* Walk up to nearest exception/function block for the jump target */` |
|       38 | 10840 | `	pBlock = pGen->pCurrent;` |
|       60 | 10841 | `	while( pBlock->pParent ){` |
|       49 | 10842 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|       27 | 10843 | `			break;` |
|        - | 10844 | `		}` |
|       23 | 10845 | `		pBlock = pBlock->pParent;` |
|        1 | 10846 | `	}` |
|       38 | 10847 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       38 | 10848 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|       38 | 10849 | `	return SXRET_OK;` |
|       20 | 10850 | `}` |
|        - | 10851 | `/*` |
|        - | 10852 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|        - | 10853 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|        - | 10854 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|        - | 10855 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|        - | 10856 | ` * compile error propagated from the parser.` |
|        - | 10857 | ` */` |
|       46 | 10858 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|        4 | 10859 | `{` |
|        - | 10860 | `	SyString sClassName;` |
|        - | 10861 | `	SyToken *pToken;` |
|        - | 10862 | `	SyString *pName;` |
|        - | 10863 | `	char *zDup;` |
|        - | 10864 | `	sxi32 rc;` |
|       50 | 10865 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       50 | 10866 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|       50 | 10867 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|       50 | 10868 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       50 | 10869 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 | 10870 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 10871 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 10872 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 10873 | `		return SXERR_INVALID;` |
|        - | 10874 | `	}` |
|       50 | 10875 | `	pGen->pIn++; /* '(' */` |
|       23 | 10876 | `	for(;;){` |
|        - | 10877 | `		SyBlob sResolved;` |
|       50 | 10878 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       50 | 10879 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 10880 | `			SyBlobRelease(&sResolved);` |
|      ! 0 | 10881 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 10882 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 10883 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 10884 | `			return SXERR_INVALID;` |
|        - | 10885 | `		}` |
|       73 | 10886 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       46 | 10887 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       50 | 10888 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|       50 | 10889 | `		SyBlobRelease(&sResolved);` |
|       50 | 10890 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|       50 | 10891 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|       50 | 10892 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       46 | 10893 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|        4 | 10894 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      ! 0 | 10895 | `			pGen->pIn++; continue;` |
|        - | 10896 | `		}` |
|       50 | 10897 | `		break;` |
|      ! 0 | 10898 | `	}` |
|       46 | 10899 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|       50 | 10900 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 10901 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 10902 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 10903 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 10904 | `		return SXERR_INVALID;` |
|        - | 10905 | `	}` |
|       50 | 10906 | `	pGen->pIn++; /* '$' */` |
|       50 | 10907 | `	pName = &pGen->pIn->sData;` |
|       50 | 10908 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       50 | 10909 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|       50 | 10910 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|       50 | 10911 | `	pGen->pIn++;` |
|       50 | 10912 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 10913 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 10914 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 10915 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 10916 | `		return SXERR_INVALID;` |
|        - | 10917 | `	}` |
|       50 | 10918 | `	pGen->pIn++; /* ')' */` |
|       50 | 10919 | `	return SXRET_OK;` |
|       27 | 10920 | `}` |
|        - | 10921 | `/*` |
|        - | 10922 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|        - | 10923 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|        - | 10924 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|        - | 10925 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|        - | 10926 | ` * VmThrowException):` |
|        - | 10927 | ` *` |
|        - | 10928 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|        - | 10929 | ` *    <try body>` |
|        - | 10930 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|        - | 10931 | ` *    JMP  -> finally\|end` |
|        - | 10932 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|        - | 10933 | ` *    <catch body>` |
|        - | 10934 | ` *    JMP  -> finally\|end` |
|        - | 10935 | ` *    ... more catches ...` |
|        - | 10936 | ` *  Lfin: <finally body>` |
|        - | 10937 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|        - | 10938 | ` *  Lend:` |
|        - | 10939 | ` */` |
|       90 | 10940 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|        4 | 10941 | `{` |
|       94 | 10942 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 10943 | `	GenBlock *pTry;` |
|        - | 10944 | `	VmInstr *pInstr;` |
|       94 | 10945 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|        - | 10946 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|        - | 10947 | `	sxi32 rc;` |
|       94 | 10948 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|        - | 10949 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|       94 | 10950 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|       94 | 10951 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       94 | 10952 | `	pTry->pUserData = pException;` |
|       94 | 10953 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|       94 | 10954 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       94 | 10955 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       94 | 10956 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       94 | 10957 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       94 | 10958 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|        - | 10959 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|       94 | 10960 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|       94 | 10961 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|       94 | 10962 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       94 | 10963 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 10964 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|       94 | 10965 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|        - | 10966 | `	/* Catch clauses (inline) */` |
|       94 | 10967 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       90 | 10968 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       50 | 10969 | `		sxu32 k = 0;` |
|       69 | 10970 | `		for(;;){` |
|        - | 10971 | `			ph7_exception_block sCatch;` |
|        - | 10972 | `			GenBlock *pCatchBlk;` |
|       96 | 10973 | `			sxu32 idxJmp = 0;` |
|       92 | 10974 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|       88 | 10975 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|       27 | 10976 | `				break;` |
|        - | 10977 | `			}` |
|       50 | 10978 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|       50 | 10979 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       50 | 10980 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|       50 | 10981 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|       50 | 10982 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|       50 | 10983 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|       50 | 10984 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        - | 10985 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|        - | 10986 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|        - | 10987 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|       50 | 10988 | `			pCatchBlk->pUserData = pException;` |
|       50 | 10989 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|       50 | 10990 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       50 | 10991 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       50 | 10992 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        - | 10993 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|        - | 10994 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|       50 | 10995 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       50 | 10996 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|       50 | 10997 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|       50 | 10998 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|       50 | 10999 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       50 | 11000 | `			k++;` |
|        4 | 11001 | `		}` |
|       23 | 11002 | `	}` |
|        - | 11003 | `	/* Finally (inline) */` |
|       94 | 11004 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       74 | 11005 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 11006 | `		GenBlock *pFinBlk;` |
|       52 | 11007 | `		pGen->pIn++; /* Jump 'finally' */` |
|       52 | 11008 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|       52 | 11009 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|       52 | 11010 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       52 | 11011 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       52 | 11012 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       52 | 11013 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       52 | 11014 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       52 | 11015 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|       52 | 11016 | `		pException->iHasFinally = 1;` |
|       24 | 11017 | `	}` |
|       94 | 11018 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|       94 | 11019 | `	pException->iInlined = 1;` |
|        - | 11020 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|        - | 11021 | `	{` |
|       94 | 11022 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|        - | 11023 | `		sxu32 *aJ; sxu32 n;` |
|       94 | 11024 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|       94 | 11025 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       94 | 11026 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|      140 | 11027 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|       50 | 11028 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|       50 | 11029 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       27 | 11030 | `		}` |
|        - | 11031 | `	}` |
|       94 | 11032 | `	SySetRelease(&aCatchJmp);` |
|       94 | 11033 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|      ! 0 | 11034 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|      ! 0 | 11035 | `	}` |
|       94 | 11036 | `	return SXRET_OK;` |
|       49 | 11037 | `}` |
|        - | 11038 | `/*` |
|        - | 11039 | ` * Compile a 'catch' block.` |
|        - | 11040 | ` * Catch: A "catch" block retrieves an exception and creates` |
|        - | 11041 | ` * an object containing the exception information.` |
|        - | 11042 | ` */` |
|     5002 | 11043 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|        5 | 11044 | `{` |
|     5007 | 11045 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11046 | `	ph7_exception_block sCatch;` |
|        - | 11047 | `	SySet *pInstrContainer;` |
|        - | 11048 | `	SyString sClassName;` |
|        - | 11049 | `	GenBlock *pCatch;` |
|        - | 11050 | `	SyToken *pToken;` |
|        - | 11051 | `	SyString *pName;` |
|        - | 11052 | `	char *zDup;` |
|        - | 11053 | `	sxi32 rc;` |
|     5007 | 11054 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        - | 11055 | `	/* Zero the structure */` |
|     5007 | 11056 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|        - | 11057 | `	/* Initialize fields */` |
|     5007 | 11058 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     5007 | 11059 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     5007 | 11060 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|        - | 11061 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 11062 | `			pToken = pGen->pIn;` |
|      ! 0 | 11063 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 11064 | `				pToken--;` |
|      ! 0 | 11065 | `			}` |
|      ! 0 | 11066 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 11067 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11068 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11069 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11070 | `				return SXERR_ABORT;` |
|        - | 11071 | `			}` |
|      ! 0 | 11072 | `			return SXERR_INVALID;` |
|        - | 11073 | `	}` |
|        - | 11074 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     5007 | 11075 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     2515 | 11076 | `	for(;;){` |
|        - | 11077 | `		SyBlob sResolved;` |
|     5035 | 11078 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     5035 | 11079 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        6 | 11080 | `			SyBlobRelease(&sResolved);` |
|        6 | 11081 | `			pToken = pGen->pIn;` |
|        6 | 11082 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 11083 | `				pToken--;` |
|      ! 0 | 11084 | `			}` |
|        8 | 11085 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 11086 | `				"syntax error, unexpected %s \"%z\"",` |
|        2 | 11087 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|        6 | 11088 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11089 | `				return SXERR_ABORT;` |
|        - | 11090 | `			}` |
|        6 | 11091 | `			return SXERR_INVALID;` |
|        - | 11092 | `		}` |
|        - | 11093 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|        - | 11094 | `		 * transient SyBlob allocation. */` |
|     7544 | 11095 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     5026 | 11096 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     5031 | 11097 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     5031 | 11098 | `		SyBlobRelease(&sResolved);` |
|     5031 | 11099 | `		if( zDup == 0 ){` |
|      ! 0 | 11100 | `			goto Mem;` |
|        - | 11101 | `		}` |
|     5031 | 11102 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     5031 | 11103 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 11104 | `			goto Mem;` |
|        - | 11105 | `		}` |
|        - | 11106 | `		/* Check for '\|' (multi-catch separator) */` |
|     5026 | 11107 | `		if( pGen->pIn < pGen->pEnd &&` |
|     5026 | 11108 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|       33 | 11109 | `			pGen->pIn->sData.nByte == 1 &&` |
|       28 | 11110 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|       30 | 11111 | `			pGen->pIn++; /* Consume the '\|' */` |
|       30 | 11112 | `			continue;` |
|        - | 11113 | `		}` |
|     5003 | 11114 | `		break;` |
|      ! 0 | 11115 | `	}` |
|     4998 | 11116 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     5003 | 11117 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 11118 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 11119 | `			pToken = pGen->pIn;` |
|      ! 0 | 11120 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 11121 | `				pToken--;` |
|      ! 0 | 11122 | `			}` |
|      ! 0 | 11123 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 11124 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11125 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11126 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11127 | `				return SXERR_ABORT;` |
|        - | 11128 | `			}` |
|      ! 0 | 11129 | `			return SXERR_INVALID;` |
|        - | 11130 | `	}` |
|     5003 | 11131 | `	pGen->pIn++; /* Jump the dollar sign */` |
|        - | 11132 | `	/* Duplicate instance name */` |
|     5003 | 11133 | `	pName = &pGen->pIn->sData;` |
|     5003 | 11134 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     5003 | 11135 | `	if( zDup == 0 ){` |
|      ! 0 | 11136 | `		goto Mem;` |
|        - | 11137 | `	}` |
|     5003 | 11138 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     5003 | 11139 | `	pGen->pIn++;` |
|     5003 | 11140 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|        - | 11141 | `		/* Unexpected token,break immediately */` |
|      ! 0 | 11142 | `		pToken = pGen->pIn;` |
|      ! 0 | 11143 | `		if( pToken >= pGen->pEnd ){` |
|      ! 0 | 11144 | `			pToken--;` |
|      ! 0 | 11145 | `		}` |
|      ! 0 | 11146 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 11147 | `			"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11148 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11149 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11150 | `			return SXERR_ABORT;` |
|        - | 11151 | `		}` |
|      ! 0 | 11152 | `		return SXERR_INVALID;` |
|        - | 11153 | `	}` |
|        - | 11154 | `	/* Compile the block */` |
|     5003 | 11155 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|        - | 11156 | `	/* Create the catch block */` |
|     5003 | 11157 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     5003 | 11158 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11159 | `		return SXERR_ABORT;` |
|        - | 11160 | `	}` |
|        - | 11161 | `	/* Swap bytecode container */` |
|     5003 | 11162 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     5003 | 11163 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|        - | 11164 | `	/* Compile the block */` |
|     5003 | 11165 | `	PH7_CompileBlock(&(*pGen),0);` |
|        - | 11166 | `	/* Fix forward jumps now the destination is resolved  */` |
|     5003 | 11167 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 11168 | `	/* Emit the DONE instruction */` |
|     5003 | 11169 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 11170 | `	/* Leave the block */` |
|     5003 | 11171 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 11172 | `	/* Restore the default container */` |
|     5003 | 11173 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 11174 | `	/* Install the catch block */` |
|     5003 | 11175 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     5003 | 11176 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11177 | `		goto Mem;` |
|        - | 11178 | `	}` |
|     5003 | 11179 | `	return SXRET_OK;` |
|      ! 0 | 11180 | `Mem:` |
|      ! 0 | 11181 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 11182 | `	return SXERR_ABORT;` |
|     2506 | 11183 | `}` |
|        - | 11184 | `/*` |
|        - | 11185 | ` * Compile a 'try' block.` |
|        - | 11186 | ` * A function using an exception should be in a "try" block.` |
|        - | 11187 | ` * If the exception does not trigger, the code will continue` |
|        - | 11188 | ` * as normal. However if the exception triggers, an exception` |
|        - | 11189 | ` * is "thrown".` |
|        - | 11190 | ` */` |
|     5150 | 11191 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|        5 | 11192 | `{` |
|        - | 11193 | `	ph7_exception *pException;` |
|     5155 | 11194 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11195 | `	GenBlock *pTry;` |
|        - | 11196 | `	sxu32 nJmpIdx;` |
|        - | 11197 | `	sxi32 rc;` |
|        - | 11198 | `	/* Create the exception container */` |
|     5155 | 11199 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     5155 | 11200 | `	if( pException == 0 ){` |
|      ! 0 | 11201 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 11202 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 11203 | `		return SXERR_ABORT;` |
|        - | 11204 | `	}` |
|        - | 11205 | `	/* Zero the structure */` |
|     5155 | 11206 | `	SyZero(pException,sizeof(ph7_exception));` |
|        - | 11207 | `	/* Initialize fields */` |
|     5155 | 11208 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     5155 | 11209 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     5155 | 11210 | `	pException->iHasFinally = 0;` |
|     5155 | 11211 | `	pException->iFinallyDone = 0;` |
|     5155 | 11212 | `	pException->pVm = pGen->pVm;` |
|        - | 11213 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|        - | 11214 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|        - | 11215 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|        - | 11216 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|        - | 11217 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|        - | 11218 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|     5155 | 11219 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|       94 | 11220 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|        - | 11221 | `	}` |
|        - | 11222 | `	/* Create the try block */` |
|     5065 | 11223 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     5065 | 11224 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11225 | `		return SXERR_ABORT;` |
|        - | 11226 | `	}` |
|        - | 11227 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     5065 | 11228 | `	pTry->pUserData = pException;` |
|        - | 11229 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     5065 | 11230 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|        - | 11231 | `	/* Fix the jump later when the destination is resolved */` |
|     5065 | 11232 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     5065 | 11233 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|        - | 11234 | `	/* Compile the block */` |
|     5065 | 11235 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     5065 | 11236 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 11237 | `		return SXERR_ABORT;` |
|        - | 11238 | `	}` |
|        - | 11239 | `	/* Fix forward jumps now the destination is resolved */` |
|     5065 | 11240 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 11241 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     5065 | 11242 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        - | 11243 | `	/* Leave the block */` |
|     5065 | 11244 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 11245 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     5065 | 11246 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     5058 | 11247 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        - | 11248 | `		/* Compile one or more catch blocks */` |
|     4998 | 11249 | `		for(;;){` |
|     9996 | 11250 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     7471 | 11251 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     2502 | 11252 | `					break;` |
|        - | 11253 | `			}` |
|     5007 | 11254 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     5007 | 11255 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11256 | `				return SXERR_ABORT;` |
|        - | 11257 | `			}` |
|        5 | 11258 | `		}` |
|     2497 | 11259 | `	}` |
|        - | 11260 | `	/* Compile optional finally block */` |
|     5065 | 11261 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      510 | 11262 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 11263 | `		SySet *pInstrContainer;` |
|        - | 11264 | `		GenBlock *pFinBlock;` |
|      129 | 11265 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|        - | 11266 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      129 | 11267 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      129 | 11268 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 11269 | `			return SXERR_ABORT;` |
|        - | 11270 | `		}` |
|        - | 11271 | `		/* Swap bytecode container */` |
|      129 | 11272 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      129 | 11273 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|        - | 11274 | `		/* Compile the finally body */` |
|      129 | 11275 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      129 | 11276 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11277 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 | 11278 | `			return SXERR_ABORT;` |
|        - | 11279 | `		}` |
|        - | 11280 | `		/* Fix forward jumps now the destination is resolved */` |
|      129 | 11281 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 11282 | `		/* Emit DONE to terminate the finally block */` |
|      129 | 11283 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 11284 | `		/* Leave the block */` |
|      129 | 11285 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        - | 11286 | `		/* Restore the default container */` |
|      129 | 11287 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      129 | 11288 | `		pException->iHasFinally = 1;` |
|       62 | 11289 | `	}` |
|        - | 11290 | `	/* Must have at least one catch or finally */` |
|     5065 | 11291 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|        9 | 11292 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 11293 | `			"Cannot use try without catch or finally");` |
|        9 | 11294 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11295 | `			return SXERR_ABORT;` |
|        - | 11296 | `		}` |
|        3 | 11297 | `	}` |
|     5065 | 11298 | `	return SXRET_OK;` |
|     2580 | 11299 | `}` |
|        - | 11300 | `/*` |
|        - | 11301 | ` * Compile a switch block.` |
|        - | 11302 | ` *  (See block-comment below for more information)` |
|        - | 11303 | ` */` |
|      112 | 11304 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|        5 | 11305 | `{` |
|      117 | 11306 | `	sxi32 rc = SXRET_OK;` |
|      117 | 11307 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|        - | 11308 | `		/* Unexpected token */` |
|      ! 0 | 11309 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 11310 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11311 | `			return SXERR_ABORT;` |
|        - | 11312 | `		}` |
|      ! 0 | 11313 | `		pGen->pIn++;` |
|      ! 0 | 11314 | `	}` |
|      117 | 11315 | `	pGen->pIn++;` |
|        - | 11316 | `	/* First instruction to execute in this block. */` |
|      117 | 11317 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|        - | 11318 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|        - | 11319 | `	 * or the '}' token */` |
|      206 | 11320 | `	for(;;){` |
|      417 | 11321 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 11322 | `			/* No more input to process */` |
|      ! 0 | 11323 | `			break;` |
|        - | 11324 | `		}` |
|      417 | 11325 | `		rc = SXRET_OK;` |
|      417 | 11326 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       85 | 11327 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|       31 | 11328 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|        - | 11329 | `					/* Unexpected token */` |
|      ! 0 | 11330 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 11331 | `						&pGen->pIn->sData);` |
|      ! 0 | 11332 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 11333 | `						return SXERR_ABORT;` |
|        - | 11334 | `					}` |
|        - | 11335 | `					/* FALL THROUGH */` |
|      ! 0 | 11336 | `				}` |
|       31 | 11337 | `				rc = SXERR_EOF;` |
|       31 | 11338 | `				break;` |
|        - | 11339 | `			}` |
|       32 | 11340 | `		}else{` |
|        - | 11341 | `			sxi32 nKwrd;` |
|        - | 11342 | `			/* Extract the keyword */` |
|      337 | 11343 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      337 | 11344 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|       47 | 11345 | `				break;` |
|        - | 11346 | `			}` |
|      253 | 11347 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 11348 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|        - | 11349 | `					/* Unexpected token */` |
|      ! 0 | 11350 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 11351 | `						&pGen->pIn->sData);` |
|      ! 0 | 11352 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 11353 | `						return SXERR_ABORT;` |
|        - | 11354 | `					}` |
|        - | 11355 | `					/* FALL THROUGH */` |
|      ! 0 | 11356 | `				}` |
|        - | 11357 | `				/* Block compiled */` |
|        3 | 11358 | `				break;` |
|        - | 11359 | `			}` |
|        - | 11360 | `		}` |
|        - | 11361 | `		/* Compile block */` |
|      305 | 11362 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      305 | 11363 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11364 | `			return SXERR_ABORT;` |
|        - | 11365 | `		}` |
|        5 | 11366 | `	}` |
|      117 | 11367 | `	return rc;` |
|       61 | 11368 | `}` |
|        - | 11369 | `/*` |
|        - | 11370 | ` * Compile a case eXpression.` |
|        - | 11371 | ` *  (See block-comment below for more information)` |
|        - | 11372 | ` */` |
|       92 | 11373 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|        5 | 11374 | `{` |
|        - | 11375 | `	SySet *pInstrContainer;` |
|        - | 11376 | `	SyToken *pEnd,*pTmp;` |
|       97 | 11377 | `	sxi32 iNest = 0;` |
|        - | 11378 | `	sxi32 rc;` |
|        - | 11379 | `	/* Delimit the expression */` |
|       97 | 11380 | `	pEnd = pGen->pIn;` |
|      197 | 11381 | `	while( pEnd < pGen->pEnd ){` |
|      197 | 11382 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|        - | 11383 | `			/* Increment nesting level */` |
|        3 | 11384 | `			iNest++;` |
|      196 | 11385 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|        - | 11386 | `			/* Decrement nesting level */` |
|        3 | 11387 | `			iNest--;` |
|      194 | 11388 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|       97 | 11389 | `			break;` |
|        - | 11390 | `		}` |
|      105 | 11391 | `		pEnd++;` |
|        5 | 11392 | `	}` |
|       97 | 11393 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 11394 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|      ! 0 | 11395 | `		if( rc == SXERR_ABORT ){` |
|        - | 11396 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 11397 | `			return SXERR_ABORT;` |
|        - | 11398 | `		}` |
|      ! 0 | 11399 | `	}` |
|        - | 11400 | `	/* Swap token stream */` |
|       97 | 11401 | `	pTmp = pGen->pEnd;` |
|       97 | 11402 | `	pGen->pEnd = pEnd;` |
|       97 | 11403 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       97 | 11404 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|       97 | 11405 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 11406 | `	/* Emit the done instruction */` |
|       97 | 11407 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       97 | 11408 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 11409 | `	/* Update token stream */` |
|       97 | 11410 | `	pGen->pIn  = pEnd;` |
|       97 | 11411 | `	pGen->pEnd = pTmp;` |
|       97 | 11412 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 11413 | `		return SXERR_ABORT;` |
|        - | 11414 | `	}` |
|       97 | 11415 | `	return SXRET_OK;` |
|       51 | 11416 | `}` |
|        - | 11417 | `/*` |
|        - | 11418 | ` * Compile the smart switch statement.` |
|        - | 11419 | ` * According to the PHP language reference manual` |
|        - | 11420 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|        - | 11421 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|        - | 11422 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|        - | 11423 | ` *  This is exactly what the switch statement is for.` |
|        - | 11424 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|        - | 11425 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|        - | 11426 | ` *  of the outer loop, use continue 2.` |
|        - | 11427 | ` *  Note that switch/case does loose comparision.` |
|        - | 11428 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|        - | 11429 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|        - | 11430 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|        - | 11431 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|        - | 11432 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|        - | 11433 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|        - | 11434 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|        - | 11435 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|        - | 11436 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|        - | 11437 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|        - | 11438 | ` *  list for the next case.` |
|        - | 11439 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|        - | 11440 | ` *  or floating-point numbers and strings.` |
|        - | 11441 | ` */` |
|       28 | 11442 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|        5 | 11443 | `{` |
|        - | 11444 | `	GenBlock *pSwitchBlock;` |
|        - | 11445 | `	SyToken *pTmp,*pEnd;` |
|        - | 11446 | `	ph7_switch *pSwitch;` |
|        - | 11447 | `	sxu32 nToken;` |
|        - | 11448 | `	sxu32 nLine;` |
|        - | 11449 | `	sxi32 rc;` |
|       33 | 11450 | `	nLine = pGen->pIn->nLine;` |
|        - | 11451 | `	/* Jump the 'switch' keyword */` |
|       33 | 11452 | `	pGen->pIn++;` |
|       33 | 11453 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 11454 | `		/* Syntax error */` |
|      ! 0 | 11455 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|      ! 0 | 11456 | `		if( rc == SXERR_ABORT ){` |
|        - | 11457 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 11458 | `			return SXERR_ABORT;` |
|        - | 11459 | `		}` |
|      ! 0 | 11460 | `		goto Synchronize;` |
|        - | 11461 | `	}` |
|        - | 11462 | `	/* Jump the left parenthesis '(' */` |
|       33 | 11463 | `	pGen->pIn++;` |
|       33 | 11464 | `	pEnd = 0; /* cc warning */` |
|        - | 11465 | `	/* Create the loop block */` |
|       47 | 11466 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|       14 | 11467 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|       33 | 11468 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11469 | `		return SXERR_ABORT;` |
|        - | 11470 | `	}` |
|        - | 11471 | `	/* Delimit the condition */` |
|       33 | 11472 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|       33 | 11473 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 11474 | `		/* Empty expression */` |
|      ! 0 | 11475 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|      ! 0 | 11476 | `		if( rc == SXERR_ABORT ){` |
|        - | 11477 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 11478 | `			return SXERR_ABORT;` |
|        - | 11479 | `		}` |
|      ! 0 | 11480 | `	}` |
|        - | 11481 | `	/* Swap token streams */` |
|       33 | 11482 | `	pTmp = pGen->pEnd;` |
|       33 | 11483 | `	pGen->pEnd = pEnd;` |
|        - | 11484 | `	/* Compile the expression */` |
|       33 | 11485 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       33 | 11486 | `	if( rc == SXERR_ABORT ){` |
|        - | 11487 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 11488 | `		return SXERR_ABORT;` |
|        - | 11489 | `	}` |
|        - | 11490 | `	/* Update token stream */` |
|       33 | 11491 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 | 11492 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 11493 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 11494 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11495 | `			return SXERR_ABORT;` |
|        - | 11496 | `		}` |
|      ! 0 | 11497 | `		pGen->pIn++;` |
|      ! 0 | 11498 | `	}` |
|       33 | 11499 | `	pGen->pIn  = &pEnd[1];` |
|       33 | 11500 | `	pGen->pEnd = pTmp;` |
|       33 | 11501 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       28 | 11502 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|      ! 0 | 11503 | `			pTmp = pGen->pIn;` |
|      ! 0 | 11504 | `			if( pTmp >= pGen->pEnd ){` |
|      ! 0 | 11505 | `				pTmp--;` |
|      ! 0 | 11506 | `			}` |
|        - | 11507 | `			/* Unexpected token */` |
|      ! 0 | 11508 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|      ! 0 | 11509 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11510 | `				return SXERR_ABORT;` |
|        - | 11511 | `			}` |
|      ! 0 | 11512 | `			goto Synchronize;` |
|        - | 11513 | `	}` |
|        - | 11514 | `	/* Set the delimiter token */` |
|       33 | 11515 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|        3 | 11516 | `		nToken = PH7_TK_KEYWORD;` |
|        - | 11517 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|        2 | 11518 | `	}else{` |
|       31 | 11519 | `		nToken = PH7_TK_CCB; /* '}' */` |
|        - | 11520 | `	}` |
|       33 | 11521 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|        - | 11522 | `	/* Create the switch blocks container */` |
|       33 | 11523 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|       33 | 11524 | `	if( pSwitch == 0 ){` |
|        - | 11525 | `		/* Abort compilation */` |
|      ! 0 | 11526 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 11527 | `		return SXERR_ABORT;` |
|        - | 11528 | `	}` |
|        - | 11529 | `	/* Zero the structure */` |
|       33 | 11530 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|        - | 11531 | `	/* Initialize fields */` |
|       33 | 11532 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|        - | 11533 | `	/* Emit the switch instruction */` |
|       33 | 11534 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|        - | 11535 | `	/* Compile case blocks */` |
|      100 | 11536 | `	for(;;){` |
|        - | 11537 | `		sxu32 nKwrd;` |
|      119 | 11538 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 11539 | `			/* No more input to process */` |
|      ! 0 | 11540 | `			break;` |
|        - | 11541 | `		}` |
|      119 | 11542 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 11543 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|        - | 11544 | `				/* Unexpected token */` |
|      ! 0 | 11545 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 11546 | `					&pGen->pIn->sData);` |
|      ! 0 | 11547 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 11548 | `					return SXERR_ABORT;` |
|        - | 11549 | `				}` |
|        - | 11550 | `				/* FALL THROUGH */` |
|      ! 0 | 11551 | `			}` |
|        - | 11552 | `			/* Block compiled */` |
|      ! 0 | 11553 | `			break;` |
|        - | 11554 | `		}` |
|        - | 11555 | `		/* Extract the keyword */` |
|      119 | 11556 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      119 | 11557 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 11558 | `			if( nToken != PH7_TK_KEYWORD ){` |
|        - | 11559 | `				/* Unexpected token */` |
|      ! 0 | 11560 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 11561 | `					&pGen->pIn->sData);` |
|      ! 0 | 11562 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 11563 | `					return SXERR_ABORT;` |
|        - | 11564 | `				}` |
|        - | 11565 | `				/* FALL THROUGH */` |
|      ! 0 | 11566 | `			}` |
|        - | 11567 | `			/* Block compiled */` |
|        3 | 11568 | `			break;` |
|        - | 11569 | `		}` |
|      117 | 11570 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|        - | 11571 | `			/*` |
|        - | 11572 | `			 * Accroding to the PHP language reference manual` |
|        - | 11573 | `			 *  A special case is the default case. This case matches anything` |
|        - | 11574 | `			 *  that wasn't matched by the other cases.` |
|        - | 11575 | `			 */` |
|       25 | 11576 | `			if( pSwitch->nDefault > 0 ){` |
|        - | 11577 | `				/* Default case already compiled */` |
|      ! 0 | 11578 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|      ! 0 | 11579 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 11580 | `					return SXERR_ABORT;` |
|        - | 11581 | `				}` |
|      ! 0 | 11582 | `			}` |
|       25 | 11583 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|        - | 11584 | `			/* Compile the default block */` |
|       25 | 11585 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|       25 | 11586 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 11587 | `				return SXERR_ABORT;` |
|       25 | 11588 | `			}else if( rc == SXERR_EOF ){` |
|       23 | 11589 | `				break;` |
|        1 | 11590 | `			}` |
|       98 | 11591 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|        - | 11592 | `			ph7_case_expr sCase;` |
|        - | 11593 | `			/* Standard case block */` |
|       97 | 11594 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|        - | 11595 | `			/* initialize the structure */` |
|       97 | 11596 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - | 11597 | `			/* Compile the case expression */` |
|       97 | 11598 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|       97 | 11599 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11600 | `				return SXERR_ABORT;` |
|        - | 11601 | `			}` |
|        - | 11602 | `			/* Compile the case block */` |
|       97 | 11603 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|        - | 11604 | `			/* Insert in the switch container */` |
|       97 | 11605 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|       97 | 11606 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 11607 | `				return SXERR_ABORT;` |
|       97 | 11608 | `			}else if( rc == SXERR_EOF ){` |
|        9 | 11609 | `				break;` |
|        - | 11610 | `			}` |
|       47 | 11611 | `		}else{` |
|        - | 11612 | `			/* Unexpected token */` |
|      ! 0 | 11613 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 11614 | `				&pGen->pIn->sData);` |
|      ! 0 | 11615 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11616 | `				return SXERR_ABORT;` |
|        - | 11617 | `			}` |
|      ! 0 | 11618 | `			break;` |
|        - | 11619 | `		}` |
|        5 | 11620 | `	}` |
|        - | 11621 | `	/* Fix all jumps now the destination is resolved */` |
|       33 | 11622 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|       33 | 11623 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 11624 | `	/* Release the loop block */` |
|       33 | 11625 | `	GenStateLeaveBlock(pGen,0);` |
|       33 | 11626 | `	if( pGen->pIn < pGen->pEnd ){` |
|        - | 11627 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|       33 | 11628 | `		pGen->pIn++;` |
|       14 | 11629 | `	}` |
|        - | 11630 | `	/* Statement successfully compiled */` |
|       33 | 11631 | `	return SXRET_OK;` |
|      ! 0 | 11632 | `Synchronize:` |
|        - | 11633 | `	/* Synchronize with the first semi-colon */` |
|      ! 0 | 11634 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      ! 0 | 11635 | `		pGen->pIn++;` |
|      ! 0 | 11636 | `	}` |
|      ! 0 | 11637 | `	return SXRET_OK;` |
|       19 | 11638 | `}` |
|        - | 11639 | `/*` |
|        - | 11640 | ` * Chain operators participate in a postfix member-access chain.` |
|        - | 11641 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|        - | 11642 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|        - | 11643 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|        - | 11644 | ` */` |
|        - | 11645 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|        - | 11646 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|        - | 11647 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|        - | 11648 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|        - | 11649 |  |
|        - | 11650 | `/*` |
|        - | 11651 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|        - | 11652 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|        - | 11653 | ` * patched entries from the pending set.` |
|        - | 11654 | ` */` |
| 22123590 | 11655 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|        5 | 11656 | `{` |
| 22123595 | 11657 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|        - | 11658 | `	sxu32 nTarget;` |
|        - | 11659 | `	sxu32 *aIdx;` |
|        - | 11660 | `	sxu32 i;` |
| 22123595 | 11661 | `	if( nCur <= nBaseline ){` |
| 22123499 | 11662 | `		return;` |
|        - | 11663 | `	}` |
|      100 | 11664 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      100 | 11665 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|      204 | 11666 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|      108 | 11667 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|      108 | 11668 | `		if( pInstr ){` |
|      108 | 11669 | `			pInstr->iP2 = (sxi32)nTarget;` |
|       52 | 11670 | `		}` |
|       56 | 11671 | `	}` |
|      100 | 11672 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 11061800 | 11673 | `}` |
|        - | 11674 |  |
|        - | 11675 | `/*` |
|        - | 11676 | ` * By-reference out-parameters of builtin functions.` |
|        - | 11677 | ` *` |
|        - | 11678 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|        - | 11679 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|        - | 11680 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|        - | 11681 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|        - | 11682 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|        - | 11683 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|        - | 11684 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|        - | 11685 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|        - | 11686 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|        - | 11687 | ` * creates it" behaviour).` |
|        - | 11688 | ` *` |
|        - | 11689 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|        - | 11690 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|        - | 11691 | ` */` |
|  3069448 | 11692 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|        5 | 11693 | `{` |
|        - | 11694 | `	static const struct {` |
|        - | 11695 | `		const char *zName;` |
|        - | 11696 | `		sxu32 nByte;` |
|        - | 11697 | `		sxu32 mask;` |
|        - | 11698 | `	} aByRef[] = {` |
|        - | 11699 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|        - | 11700 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|        - | 11701 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|        - | 11702 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|        - | 11703 | `	};` |
|        - | 11704 | `	sxu32 i;` |
|  3069453 | 11705 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|   779789 | 11706 | `		return 0;` |
|        - | 11707 | `	}` |
| 11448053 | 11708 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
|  9158478 | 11709 | `		if( pName->nByte == aByRef[i].nByte` |
|  4714938 | 11710 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|       99 | 11711 | `			return aByRef[i].mask;` |
|        - | 11712 | `		}` |
|  4579197 | 11713 | `	}` |
|  2289575 | 11714 | `	return 0;` |
|  1534729 | 11715 | `}` |
|        - | 11716 | `/*` |
|        - | 11717 | ` * Recover the bare global-builtin name from a call's callee node.` |
|        - | 11718 | ` *` |
|        - | 11719 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|        - | 11720 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|        - | 11721 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|        - | 11722 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|        - | 11723 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|        - | 11724 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|        - | 11725 | ` */` |
|  3069448 | 11726 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|        5 | 11727 | `{` |
|        - | 11728 | `	SyToken *p, *pEnd;` |
|  3069453 | 11729 | `	pOut->zString = 0;` |
|  3069453 | 11730 | `	pOut->nByte = 0;` |
|  3069453 | 11731 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|      ! 0 | 11732 | `		return;` |
|        - | 11733 | `	}` |
|  3069453 | 11734 | `	p = pLeft->pStart;` |
|  3069453 | 11735 | `	pEnd = pLeft->pEnd;` |
|        - | 11736 | `	/* Optional single leading namespace separator (absolute path). */` |
|  3069453 | 11737 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|     3877 | 11738 | `		p++;` |
|     1936 | 11739 | `	}` |
|  3069453 | 11740 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|   779753 | 11741 | `		return;` |
|        - | 11742 | `	}` |
|        - | 11743 | `	/* Must be a single component: nothing follows the name token. */` |
|  2289705 | 11744 | `	if( p + 1 != pEnd ){` |
|       41 | 11745 | `		return;` |
|        - | 11746 | `	}` |
|  2289669 | 11747 | `	*pOut = p->sData;` |
|  1534729 | 11748 | `}` |
|        - | 11749 | `/*` |
|        - | 11750 | ` * Generate bytecode for a given expression tree.` |
|        - | 11751 | ` * If something goes wrong while generating bytecode` |
|        - | 11752 | ` * for the expression tree (A very unlikely scenario)` |
|        - | 11753 | ` * this function takes care of generating the appropriate` |
|        - | 11754 | ` * error message.` |
|        - | 11755 | ` */` |
| 30518246 | 11756 | `static sxi32 GenStateEmitExprCode(` |
|        - | 11757 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - | 11758 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|        - | 11759 | `	sxi32 iFlags /* Control flags */` |
|        - | 11760 | `	)` |
|        5 | 11761 | `{` |
|        - | 11762 | `	VmInstr *pInstr;` |
|        - | 11763 | `	sxu32 nJmpIdx;` |
| 30518251 | 11764 | `	sxi32 iP1 = 0;` |
| 30518251 | 11765 | `	sxu32 iP2 = 0;` |
| 30518251 | 11766 | `	void *p3  = 0;` |
|        - | 11767 | `	sxi32 iVmOp;` |
|        - | 11768 | `	sxi32 rc;` |
| 30518251 | 11769 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 30518251 | 11770 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
| 30518251 | 11771 | `	sxu32 nRhsNsBase = 0;` |
| 30518251 | 11772 | `	if( pNode->xCode ){` |
|        - | 11773 | `		SyToken *pTmpIn,*pTmpEnd;` |
|        - | 11774 | `		/* Compile node */` |
| 18380097 | 11775 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 18380097 | 11776 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 18380097 | 11777 | `		RE_SWAP_DELIMITER(pGen);` |
| 18380097 | 11778 | `		return rc;` |
|        - | 11779 | `	}` |
| 12138159 | 11780 | `	if( pNode->pOp == 0 ){` |
|      ! 0 | 11781 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 11782 | `			"Invalid expression node,PH7 is aborting compilation");` |
|      ! 0 | 11783 | `		return SXERR_ABORT;` |
|        - | 11784 | `	}` |
| 12138159 | 11785 | `	iVmOp = pNode->pOp->iVmOp;` |
| 12138159 | 11786 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|        - | 11787 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|        - | 11788 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|        - | 11789 | `		 * and later errors are still reported. */` |
|        3 | 11790 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 11791 | `			"The (unset) cast is no longer supported");` |
|        3 | 11792 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11793 | `			return SXERR_ABORT;` |
|        - | 11794 | `		}` |
|        1 | 11795 | `	}` |
| 12138159 | 11796 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|       65 | 11797 | `		sxu32 nJmp = 0;` |
|        - | 11798 | `		sxu32 nNcNsBase;` |
|        - | 11799 | `		VmInstr *pInstrFix;` |
|        - | 11800 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|        - | 11801 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|        - | 11802 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|        - | 11803 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|        - | 11804 | `		 * stack slot carries a writable nIdx. */` |
|       65 | 11805 | `		if( pNode->pRight ){` |
|       65 | 11806 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       65 | 11807 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|       65 | 11808 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 11809 | `				return rc;` |
|        - | 11810 | `			}` |
|       65 | 11811 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        - | 11812 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|        - | 11813 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|        - | 11814 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|        - | 11815 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|        - | 11816 | `			 * the store, so the parent array does not need to be copied at` |
|        - | 11817 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|        - | 11818 | `			 * cascade for the actual write path stays correct. */` |
|       65 | 11819 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|       65 | 11820 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|       31 | 11821 | `				pInstrFix->iP2 = 3;` |
|       14 | 11822 | `			}` |
|       31 | 11823 | `		}` |
|        - | 11824 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|       65 | 11825 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|        - | 11826 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|       65 | 11827 | `		if( pNode->pLeft ){` |
|       65 | 11828 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       65 | 11829 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|       65 | 11830 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 11831 | `				return rc;` |
|        - | 11832 | `			}` |
|       65 | 11833 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       31 | 11834 | `		}` |
|        - | 11835 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|       65 | 11836 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|        - | 11837 | `		/* Patch the short-circuit jump to land after the store. */` |
|       65 | 11838 | `		if( nJmp > 0 ){` |
|       65 | 11839 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|       65 | 11840 | `			if( pInstrFix ){` |
|       65 | 11841 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|       31 | 11842 | `			}` |
|       31 | 11843 | `		}` |
|       65 | 11844 | `		return SXRET_OK;` |
|        - | 11845 | `	}` |
| 12138097 | 11846 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|        - | 11847 | `		sxu32 nJz,nJmp;` |
|        - | 11848 | `		sxu32 nTernaryNsBase;` |
|        - | 11849 | `		/* Ternary operator require special handling */` |
|        - | 11850 | `		/* Phase#1: Compile the condition */` |
|   199101 | 11851 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   199101 | 11852 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|   199101 | 11853 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 11854 | `			return rc;` |
|        - | 11855 | `		}` |
|        - | 11856 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|        - | 11857 | `		 * compiling the condition must short-circuit to the end of the` |
|        - | 11858 | `		 * condition expression, not leak past the ternary. */` |
|   199101 | 11859 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|   199101 | 11860 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|   199101 | 11861 | `		if( pNode->pLeft ){` |
|        - | 11862 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|        - | 11863 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|   199033 | 11864 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|        - | 11865 | `			/* Phase#3: Compile the 'then' expression  */` |
|   199033 | 11866 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   199033 | 11867 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|   199033 | 11868 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 11869 | `				return rc;` |
|        - | 11870 | `			}` |
|   199033 | 11871 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    99519 | 11872 | `		}else{` |
|        - | 11873 | `			/* Elvis operator: (expr) ?: (else)` |
|        - | 11874 | `			 * Duplicate condition so original value is the 'then' result.` |
|        - | 11875 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|       70 | 11876 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       70 | 11877 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|        - | 11878 | `		}` |
|        - | 11879 | `		/* Phase#4: Emit the unconditional jump */` |
|   199101 | 11880 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|        - | 11881 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|   199101 | 11882 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|   199101 | 11883 | `		if( pInstr ){` |
|   199101 | 11884 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    99548 | 11885 | `		}` |
|   199101 | 11886 | `		if( !pNode->pLeft ){` |
|        - | 11887 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|       70 | 11888 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       34 | 11889 | `		}` |
|        - | 11890 | `		/* Phase#6: Compile the 'else' expression */` |
|   199101 | 11891 | `		if( pNode->pRight ){` |
|   199101 | 11892 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   199101 | 11893 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|   199101 | 11894 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 11895 | `				return rc;` |
|        - | 11896 | `			}` |
|   199101 | 11897 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    99548 | 11898 | `		}` |
|   199101 | 11899 | `		if( nJmp > 0 ){` |
|        - | 11900 | `			/* Phase#7: Fix the unconditional jump */` |
|   199101 | 11901 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|   199101 | 11902 | `			if( pInstr ){` |
|   199101 | 11903 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    99548 | 11904 | `			}` |
|    99548 | 11905 | `		}` |
|        - | 11906 | `		/* All done */` |
|   199101 | 11907 | `		return SXRET_OK;` |
|        - | 11908 | `	}` |
| 11939001 | 11909 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|        - | 11910 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|        - | 11911 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|        - | 11912 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|        - | 11913 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|        - | 11914 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|        - | 11915 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|        - | 11916 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|        - | 11917 | `		sxu32 nPipeNsBase;` |
|       27 | 11918 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|       27 | 11919 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|      ! 0 | 11920 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 11921 | `				"'\|>': Missing operand");` |
|      ! 0 | 11922 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 11923 | `		}` |
|        - | 11924 | `		/* Argument: the LHS value. */` |
|       27 | 11925 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       27 | 11926 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|       27 | 11927 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 11928 | `			return rc;` |
|        - | 11929 | `		}` |
|       27 | 11930 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|        - | 11931 | `		/* Callable: the RHS. */` |
|       27 | 11932 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       27 | 11933 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|       27 | 11934 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 11935 | `			return rc;` |
|        - | 11936 | `		}` |
|       27 | 11937 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|        - | 11938 | `		/* Invoke the callable with the single piped argument. */` |
|       27 | 11939 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       27 | 11940 | `		return SXRET_OK;` |
|        - | 11941 | `	}` |
| 11938975 | 11942 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|        - | 11943 | `	/* Generate code for the left tree */` |
| 11938975 | 11944 | `	if( pNode->pLeft ){` |
| 11931255 | 11945 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 11931255 | 11946 | `		if( iVmOp == PH7_OP_CALL ){` |
|        - | 11947 | `			ph7_expr_node **apNode;` |
|  3073465 | 11948 | `			int hasSpread = 0;` |
|  3073465 | 11949 | `			int hasNamed = 0;` |
|  3073465 | 11950 | `			int bAnySpread = 0;` |
|  3073465 | 11951 | `			sxu32 byRefMask = 0;` |
|        - | 11952 | `			sxi32 nArgs;` |
|        - | 11953 | `			sxi32 n;` |
|        - | 11954 | `			/* Recurse and generate bytecodes for function arguments */` |
|  3073465 | 11955 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  3073465 | 11956 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|        - | 11957 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|        - | 11958 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|        - | 11959 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|  3073465 | 11960 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|       77 | 11961 | `				bFcc = 1;` |
|       77 | 11962 | `				nArgs = 0;` |
|       38 | 11963 | `			}` |
|        - | 11964 | `			/* Validate: no positional arguments after named arguments */` |
|        - | 11965 | `			{` |
|  3073465 | 11966 | `				int seenNamed = 0;` |
|  6118285 | 11967 | `				for( n = 0; n < nArgs; ++n ){` |
|  3044827 | 11968 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      253 | 11969 | `						seenNamed = 1;` |
|      253 | 11970 | `						hasNamed = 1;` |
|  3044703 | 11971 | `					}else if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|     3877 | 11972 | `						bAnySpread = 1;` |
|  3042643 | 11973 | `					}else if( seenNamed ){` |
|        3 | 11974 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 11975 | `							"Cannot use positional argument after named argument");` |
|        3 | 11976 | `						return SXERR_SYNTAX;` |
|        - | 11977 | `					}` |
|  1522415 | 11978 | `				}` |
|        - | 11979 | `			}` |
|        - | 11980 | `			/* Read-only load */` |
|  3073463 | 11981 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|        - | 11982 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|        - | 11983 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|        - | 11984 | `			 * objects dispatch to the right method (offsetExists for both;` |
|        - | 11985 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|  3073463 | 11986 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|  3073463 | 11987 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  3073458 | 11988 | `				if( pCallName->nByte == 5` |
|  1693551 | 11989 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|   154099 | 11990 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|  2996416 | 11991 | `				}else if( pCallName->nByte == 5` |
|  1539457 | 11992 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|       99 | 11993 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|       47 | 11994 | `				}` |
|        - | 11995 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|        - | 11996 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|        - | 11997 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|        - | 11998 | `				 * write back through. Skipped when spread/named args are present:` |
|        - | 11999 | `				 * the compile-time positional index no longer maps to the` |
|        - | 12000 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|  3073463 | 12001 | `				if( !bAnySpread && !hasNamed ){` |
|        - | 12002 | `					SyString sBuiltin;` |
|  3069453 | 12003 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|  3069453 | 12004 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|  1534724 | 12005 | `				}` |
|  1536729 | 12006 | `			}` |
|  6118281 | 12007 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  3044823 | 12008 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  3044823 | 12009 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|        - | 12010 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|        - | 12011 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|        - | 12012 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|        - | 12013 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|        - | 12014 | `				 * builtin to write back through. A plain $var target is unaffected` |
|        - | 12015 | `				 * (iP1=0 either way). See PLAN.md §2 for the full rationale. */` |
|  3044823 | 12016 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|       55 | 12017 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|       55 | 12018 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|       25 | 12019 | `				}` |
|  3044823 | 12020 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  3044823 | 12021 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 12022 | `					return rc;` |
|        - | 12023 | `				}` |
|        - | 12024 | `				/* Each argument is an independent nullsafe scope. */` |
|  3044823 | 12025 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  3044823 | 12026 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|        - | 12027 | `					/* Emit spread opcode to unpack this array argument */` |
|     3877 | 12028 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|     3877 | 12029 | `					hasSpread = 1;` |
|     1936 | 12030 | `				}` |
|  1522414 | 12031 | `			}` |
|        - | 12032 | `			/* Total number of given arguments */` |
|  3073463 | 12033 | `			iP1 = nArgs;` |
|  3073463 | 12034 | `			iP2 = hasSpread;` |
|        - | 12035 | `			/* Build VmCallArgMap if named arguments are present.` |
|        - | 12036 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  3073463 | 12037 | `			if( hasNamed ){` |
|      142 | 12038 | `				sxu32 nStrBytes = 0;` |
|        - | 12039 | `				char *zBuf;` |
|      424 | 12040 | `				for( n = 0; n < nArgs; ++n ){` |
|      286 | 12041 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      250 | 12042 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|      123 | 12043 | `					}` |
|      145 | 12044 | `				}` |
|        - | 12045 | `				{` |
|      142 | 12046 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|      142 | 12047 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      138 | 12048 | `					&pGen->pVm->sAllocator, mapSize);` |
|      142 | 12049 | `				if( pMap ){` |
|      142 | 12050 | `					SyZero(pMap, mapSize);` |
|      142 | 12051 | `					pMap->bHasNamed = 1;` |
|      142 | 12052 | `					pMap->nTotal = (sxu32)nArgs;` |
|      142 | 12053 | `					pMap->aNames = (SyString *)&pMap[1];` |
|      142 | 12054 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|      424 | 12055 | `					for( n = 0; n < nArgs; ++n ){` |
|      286 | 12056 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      250 | 12057 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|      250 | 12058 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|      250 | 12059 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|      250 | 12060 | `							zBuf += nb;` |
|      123 | 12061 | `						}` |
|        - | 12062 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|      145 | 12063 | `					}` |
|      142 | 12064 | `					p3 = (void *)pMap;` |
|       69 | 12065 | `				}` |
|        - | 12066 | `				}` |
|       69 | 12067 | `			}` |
|        - | 12068 | `			/* Remove stale flags now */` |
|  3073463 | 12069 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  1536729 | 12070 | `		}` |
|        - | 12071 | `		{` |
|        - | 12072 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|        - | 12073 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|        - | 12074 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|        - | 12075 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|        - | 12076 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|        - | 12077 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|        - | 12078 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|        - | 12079 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
| 11931253 | 12080 | `			sxi32 iLeftFlags = iFlags;` |
| 11931248 | 12081 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  9934849 | 12082 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|  3969251 | 12083 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  3533335 | 12084 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   883615 | 12085 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   441805 | 12086 | `			}` |
|        - | 12087 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|        - | 12088 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|        - | 12089 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|        - | 12090 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|        - | 12091 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|        - | 12092 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|        - | 12093 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
| 11931248 | 12094 | `			if( pNode->pOp` |
| 16962476 | 12095 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
| 10996899 | 12096 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
| 10062498 | 12097 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|  1896159 | 12098 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   948077 | 12099 | `			}` |
| 11931253 | 12100 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|        - | 12101 | `		}` |
| 11931253 | 12102 | `		if( rc != SXRET_OK ){` |
|       34 | 12103 | `			return rc;` |
|        - | 12104 | `		}` |
| 11931223 | 12105 | `		if( !bIsChainOp ){` |
|        - | 12106 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|        - | 12107 | `			 * target the end of that LHS chain, which is right here. */` |
|  5440425 | 12108 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  2720210 | 12109 | `		}` |
| 11931223 | 12110 | `		if( iVmOp == PH7_OP_CALL ){` |
|  3073463 | 12111 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  3073463 | 12112 | `			if( pInstr ){` |
|  3073463 | 12113 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  2289823 | 12114 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|        - | 12115 | `					sxu32 nQual;` |
|  2289823 | 12116 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|        - | 12117 | `					/* Prevent constant expansion but preserve the absolute flag` |
|        - | 12118 | `					 * so the later NEW handler (if any) can see it. */` |
|  2289823 | 12119 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|        - | 12120 | `					/* Namespace-qualify the function name for CALL, unless the` |
|        - | 12121 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|        - | 12122 | `					 * imports — class imports must NOT affect function` |
|        - | 12123 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|        - | 12124 | `					 * before NEW; we store the original literal index in the` |
|        - | 12125 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|        - | 12126 | `					 * the unqualified name and re-qualify with class imports. */` |
|  2289823 | 12127 | `					if( bAbsolute ){` |
|     3877 | 12128 | `						pInstr->iP2 = (sxi32)nOrig;` |
|     1941 | 12129 | `					}else{` |
|  2285951 | 12130 | `						int fromImport = 0;` |
|  2285951 | 12131 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  2285951 | 12132 | `						pInstr->iP2 = (sxi32)nQual;` |
|  2285951 | 12133 | `						if( nQual != nOrig ){` |
|        - | 12134 | `							/* Store original literal index in CALL's iP2 so the` |
|        - | 12135 | `							 * NEW handler can recover the unqualified name. */` |
|       77 | 12136 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|       77 | 12137 | `							if( !fromImport ){` |
|        - | 12138 | `								/* Mark as namespace-qualified via VmCallArgMap */` |
|       67 | 12139 | `								if( p3 == 0 ){` |
|       67 | 12140 | `									VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       62 | 12141 | `										&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|       67 | 12142 | `									if( pMap ){` |
|       67 | 12143 | `										SyZero(pMap, sizeof(VmCallArgMap));` |
|       67 | 12144 | `										p3 = (void *)pMap;` |
|       31 | 12145 | `									}` |
|       31 | 12146 | `								}` |
|       67 | 12147 | `								if( p3 ){` |
|       67 | 12148 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|       31 | 12149 | `								}` |
|       31 | 12150 | `							}` |
|       36 | 12151 | `						}` |
|        5 | 12152 | `					}` |
|  1928554 | 12153 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|        - | 12154 | `					/* Method call,flag that */` |
|   779329 | 12155 | `					pInstr->iP2 = 1;` |
|   389662 | 12156 | `				}` |
|  1536734 | 12157 | `			}` |
| 10394494 | 12158 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|        - | 12159 | `			ph7_expr_node **apNode;` |
|        - | 12160 | `			sxi32 n;` |
|  1521191 | 12161 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|        - | 12162 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|        - | 12163 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|        - | 12164 | `			/* Recurse and generate bytecodes for array index */` |
|  1521191 | 12165 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  2919257 | 12166 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|  1398071 | 12167 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  1398071 | 12168 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|  1398071 | 12169 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 12170 | `					return rc;` |
|        - | 12171 | `				}` |
|        - | 12172 | `				/* Each subscript index is an independent nullsafe scope. */` |
|  1398071 | 12173 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   699038 | 12174 | `			}` |
|  1521191 | 12175 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|  1398071 | 12176 | `				iP1 = 1; /* Node have an index associated with it */` |
|   699033 | 12177 | `			}` |
|  1521191 | 12178 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|        - | 12179 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|   192445 | 12180 | `				iP2 = 4;` |
|  1424971 | 12181 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        - | 12182 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|        - | 12183 | `				 * so the trailing unset() builtin can drop the slot. */` |
|       64 | 12184 | `				iP2 = 5;` |
|  1328721 | 12185 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        - | 12186 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|        - | 12187 | `				 * short-circuit on missing keys without invoking offsetGet` |
|        - | 12188 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|       29 | 12189 | `				iP2 = 6;` |
|  1328679 | 12190 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|        - | 12191 | `				/* Create an empty entry when the desired index is not found */` |
|   185073 | 12192 | `				iP2 = 1;` |
|    92539 | 12193 | `			}` |
|  8097172 | 12194 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|        - | 12195 | `			/* POP the left node */` |
|       32 | 12196 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       15 | 12197 | `		}` |
|  5965609 | 12198 | `	}` |
| 11938943 | 12199 | `	rc = SXRET_OK;` |
| 11938943 | 12200 | `	nJmpIdx = 0;` |
|        - | 12201 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|        - | 12202 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|        - | 12203 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 11938943 | 12204 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|    35077 | 12205 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    35077 | 12206 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    35077 | 12207 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    35077 | 12208 | `			int isSpecial = 0;` |
|    35077 | 12209 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|    19609 | 12210 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|    19609 | 12211 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|    19604 | 12212 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    27278 | 12213 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    13633 | 12214 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|     7799 | 12215 | `					isSpecial = 1;` |
|     3897 | 12216 | `				}` |
|    13669 | 12217 | `			}` |
|    42811 | 12218 | `			pInstr->iP1 = 0;` |
|    42811 | 12219 | `			if( !isSpecial ){` |
|    19549 | 12220 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     9772 | 12221 | `			}` |
|        - | 12222 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|        - | 12223 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|    27343 | 12224 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    19549 | 12225 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    19549 | 12226 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|       48 | 12227 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|       50 | 12228 | `					return SXRET_OK;` |
|        - | 12229 | `				}` |
|     9749 | 12230 | `			}` |
|    13646 | 12231 | `		}` |
|    29093 | 12232 | `	}` |
|        - | 12233 | `	/* Generate code for the right tree */` |
| 11931177 | 12234 | `	if( pNode->pRight ){` |
|  6544195 | 12235 | `		if( iVmOp == PH7_OP_LAND ){` |
|        - | 12236 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|   135073 | 12237 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  6476661 | 12238 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|        - | 12239 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    88599 | 12240 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  6364830 | 12241 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|        - | 12242 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|      135 | 12243 | `			iVmOp = 0; /* No binary operator to emit */` |
|      135 | 12244 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  6320520 | 12245 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|        - | 12246 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|        - | 12247 | `			 * the entire containing postfix chain to null. The jump target is` |
|        - | 12248 | `			 * patched later by the innermost non-chain ancestor (or by` |
|        - | 12249 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|        - | 12250 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|        - | 12251 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|      108 | 12252 | `			sxu32 nNsJmp = 0;` |
|      108 | 12253 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|      108 | 12254 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  6320351 | 12255 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|        - | 12256 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|        - | 12257 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|        - | 12258 | `			 * auto-created — PHP auto-vivifies on write. */` |
|  2243773 | 12259 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|  1121884 | 12260 | `		}` |
|  6544195 | 12261 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  6544195 | 12262 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  6544195 | 12263 | `		if( !bIsChainOp ){` |
|        - | 12264 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|        - | 12265 | `			 * operator instruction is emitted. */` |
|  4648087 | 12266 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  2324041 | 12267 | `		}` |
|  6544195 | 12268 | `		if( iVmOp == PH7_OP_STORE ){` |
|  1959035 | 12269 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  1959000 | 12270 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|        - | 12271 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|        - | 12272 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|        - | 12273 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|        - | 12274 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|        - | 12275 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|        - | 12276 | `				 */` |
|       85 | 12277 | `				iVmOp = 0;` |
|  1958995 | 12278 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  1958955 | 12279 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|        - | 12280 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   246545 | 12281 | `					iP2 = 1;` |
|   123275 | 12282 | `				}else{` |
|  1712415 | 12283 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        - | 12284 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   184991 | 12285 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   184991 | 12286 | `						iP1 = pInstr->iP1;` |
|    92498 | 12287 | `					}else{` |
|  1527429 | 12288 | `						p3 = pInstr->p3;` |
|        - | 12289 | `					}` |
|        - | 12290 | `					/* POP the last dynamic load instruction */` |
|  1712415 | 12291 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        - | 12292 | `				}` |
|   979480 | 12293 | `			}` |
|  5564680 | 12294 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|       63 | 12295 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|       63 | 12296 | `			if( pInstr ){` |
|       63 | 12297 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        - | 12298 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|        - | 12299 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|        - | 12300 | `					 */` |
|       19 | 12301 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|       19 | 12302 | `					iP1 = pInstr->iP1;` |
|       19 | 12303 | `					iP2 = pInstr->iP2;` |
|       19 | 12304 | `					p3  = pInstr->p3;` |
|       10 | 12305 | `				}else{` |
|       45 | 12306 | `					p3 = pInstr->p3;` |
|        - | 12307 | `				}` |
|       30 | 12308 | `			}` |
|       30 | 12309 | `		}` |
|  3272095 | 12310 | `	}` |
| 11931172 | 12311 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|   227959 | 12312 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|        - | 12313 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|        - | 12314 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|       30 | 12315 | `		iVmOp = 0;` |
|       13 | 12316 | `	}` |
| 11931177 | 12317 | `	if( iVmOp > 0 ){` |
| 11930911 | 12318 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    69613 | 12319 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|        - | 12320 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    11557 | 12321 | `				iP1 = 1;` |
|     5781 | 12322 | `			}` |
| 11896107 | 12323 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|        - | 12324 | `			/* Namespace-qualify the class name for NEW */ {` |
|   455645 | 12325 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   455645 | 12326 | `				VmInstr *pCallInstr = 0;` |
|   455645 | 12327 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   455429 | 12328 | `					pCallInstr = pPeek;` |
|   455429 | 12329 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|   227712 | 12330 | `				}` |
|   455645 | 12331 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   455641 | 12332 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|        - | 12333 | `					sxu32 nLitForClass;` |
|        - | 12334 | `					/* If the CALL handler already qualified the name using` |
|        - | 12335 | `					 * function imports, recover the original unqualified` |
|        - | 12336 | `					 * literal so we can re-qualify with class imports. */` |
|   455641 | 12337 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|       37 | 12338 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|       21 | 12339 | `					}else{` |
|   455609 | 12340 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|        - | 12341 | `					}` |
|   455641 | 12342 | `					pPeek->iP1 = 0;` |
|   455641 | 12343 | `					if( !bAbsolute ){` |
|   451773 | 12344 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|   225889 | 12345 | `					}else{` |
|     3873 | 12346 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|        - | 12347 | `					}` |
|   227818 | 12348 | `				}` |
|        - | 12349 | `			}` |
|   455645 | 12350 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   455645 | 12351 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|        - | 12352 | `				VmInstr *pPrev;` |
|   455429 | 12353 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   455429 | 12354 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|        - | 12355 | `					/* Pop the call instruction, preserve named-arg map */` |
|   455429 | 12356 | `					iP1 = pInstr->iP1;` |
|   455429 | 12357 | `					if( pInstr->p3 ){` |
|       43 | 12358 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|       19 | 12359 | `					}` |
|   455429 | 12360 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|   227712 | 12361 | `				}` |
|   227717 | 12362 | `			}` |
| 11633483 | 12363 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|        - | 12364 | `			/* instanceof: right operand is a class name, not a constant.` |
|        - | 12365 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|    30967 | 12366 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    30967 | 12367 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    30967 | 12368 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    30967 | 12369 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|    30967 | 12370 | `				int isSpecialIs = 0;` |
|    30967 | 12371 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    30967 | 12372 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    30967 | 12373 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    30962 | 12374 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    30965 | 12375 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    15481 | 12376 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|       12 | 12377 | `						isSpecialIs = 1;` |
|        5 | 12378 | `					}` |
|    15481 | 12379 | `				}` |
|    30967 | 12380 | `				pInstr->iP1 = 0;` |
|    30967 | 12381 | `				if( !isSpecialIs && !bAbsolute ){` |
|    30947 | 12382 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    15471 | 12383 | `				}` |
|    15486 | 12384 | `			}` |
| 11390182 | 12385 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|        - | 12386 | `			/* Prevent constant expansion for member/property names.` |
|        - | 12387 | `			 * The right child (member name) was just compiled — its LOADC` |
|        - | 12388 | `			 * should not trigger constant lookup. */` |
|  1896113 | 12389 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  1896113 | 12390 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  1896059 | 12391 | `				pInstr->iP1 = 0;` |
|   948027 | 12392 | `			}` |
|  1896113 | 12393 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|        - | 12394 | `				/* Static member access,remember that */` |
|    27311 | 12395 | `				iP1 = 1;` |
|    27311 | 12396 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    27311 | 12397 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|       48 | 12398 | `					p3 = pInstr->p3;` |
|       48 | 12399 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       22 | 12400 | `				}` |
|    13653 | 12401 | `			}` |
|        - | 12402 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|        - | 12403 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|        - | 12404 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|        - | 12405 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|  1896113 | 12406 | `			if( iP2 == PH7_MEMBER_READ ){` |
|  1896113 | 12407 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       30 | 12408 | `					iP2 = PH7_MEMBER_UNSET;` |
|  1896099 | 12409 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       77 | 12410 | `					iP2 = PH7_MEMBER_ISSET;` |
|  1896049 | 12411 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       13 | 12412 | `					iP2 = PH7_MEMBER_EMPTY;` |
|  1896007 | 12413 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|        - | 12414 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|   246625 | 12415 | `					iP2 = PH7_MEMBER_WRITE;` |
|   123310 | 12416 | `				}` |
|   948054 | 12417 | `			}` |
|   948054 | 12418 | `		}` |
|        - | 12419 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|        - | 12420 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|        - | 12421 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|        - | 12422 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|        - | 12423 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
| 11930911 | 12424 | `		if( bFcc ){` |
|       77 | 12425 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|       77 | 12426 | `			iP2 = 0;` |
|       77 | 12427 | `			p3 = 0;` |
|       77 | 12428 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|       77 | 12429 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|        - | 12430 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|        - | 12431 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|        - | 12432 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|        - | 12433 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|       35 | 12434 | `				void *pMemberName = pInstr->p3;` |
|       35 | 12435 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|       35 | 12436 | `				if( pMemberName ){` |
|        3 | 12437 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|        1 | 12438 | `				}` |
|       35 | 12439 | `				iP1 = 2;` |
|       18 | 12440 | `			}else{` |
|       43 | 12441 | `				iP1 = 1;` |
|        - | 12442 | `			}` |
|       38 | 12443 | `		}` |
|        - | 12444 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|        - | 12445 | `		 * This is the primary emit path for user-visible calls. */` |
| 11930911 | 12446 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  3529027 | 12447 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  1764511 | 12448 | `		}` |
|        - | 12449 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 11930911 | 12450 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  5965453 | 12451 | `	}` |
| 11931177 | 12452 | `	if( nJmpIdx > 0 ){` |
|        - | 12453 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   223797 | 12454 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   223797 | 12455 | `		if( pInstr ){` |
|   223797 | 12456 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|   111896 | 12457 | `		}` |
|   111896 | 12458 | `	}` |
| 11931177 | 12459 | `	return rc;` |
| 15255268 | 12460 | `}` |
|        - | 12461 | `/*` |
|        - | 12462 | ` * Compile a PHP expression.` |
|        - | 12463 | ` * According to the PHP language reference manual:` |
|        - | 12464 | ` *  Expressions are the most important building stones of PHP.` |
|        - | 12465 | ` *  In PHP, almost anything you write is an expression.` |
|        - | 12466 | ` *  The simplest yet most accurate way to define an expression` |
|        - | 12467 | ` *  is "anything that has a value".` |
|        - | 12468 | ` * If something goes wrong while compiling the expression,this` |
|        - | 12469 | ` * function takes care of generating the appropriate error` |
|        - | 12470 | ` * message.` |
|        - | 12471 | ` */` |
|  6995000 | 12472 | `static sxi32 PH7_CompileExpr(` |
|        - | 12473 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 12474 | `	sxi32 iFlags,        /* Control flags */` |
|        - | 12475 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|        - | 12476 | `	)` |
|        5 | 12477 | `{` |
|        - | 12478 | `	ph7_expr_node *pRoot;` |
|        - | 12479 | `	SySet sExprNode;` |
|        - | 12480 | `	SyToken *pEnd;` |
|        - | 12481 | `	sxi32 nExpr;` |
|        - | 12482 | `	sxi32 iNest;` |
|        - | 12483 | `	sxi32 rc;` |
|        - | 12484 | `	sxu32 nNullsafeBase;` |
|        - | 12485 | `	/* Initialize worker variables */` |
|  6995005 | 12486 | `	nExpr = 0;` |
|  6995005 | 12487 | `	pRoot = 0;` |
|        - | 12488 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|        - | 12489 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  6995005 | 12490 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  6995005 | 12491 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  6995005 | 12492 | `	SySetAlloc(&sExprNode,0x10);` |
|  6995005 | 12493 | `	rc = SXRET_OK;` |
|        - | 12494 | `	/* Delimit the expression */` |
|  6995005 | 12495 | `	pEnd = pGen->pIn;` |
|  6995005 | 12496 | `	iNest = 0;` |
| 53894217 | 12497 | `	while( pEnd < pGen->pEnd ){` |
| 51411497 | 12498 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|        - | 12499 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      571 | 12500 | `			iNest++;` |
| 51411214 | 12501 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      579 | 12502 | `			iNest--;` |
| 51410644 | 12503 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  4512729 | 12504 | `			if( iNest <= 0 ){` |
|  4512285 | 12505 | `				break;` |
|        - | 12506 | `			}` |
|      222 | 12507 | `		}` |
| 46899217 | 12508 | `		pEnd++;` |
|        5 | 12509 | `	}` |
|  6995005 | 12510 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   235203 | 12511 | `		SyToken *pEnd2 = pGen->pIn;` |
|   235203 | 12512 | `		iNest = 0;` |
|        - | 12513 | `		/* Stop at the first comma */` |
|   547699 | 12514 | `		while( pEnd2 < pEnd ){` |
|   312507 | 12515 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|     7777 | 12516 | `				iNest++;` |
|   308621 | 12517 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|     7777 | 12518 | `				iNest--;` |
|   300849 | 12519 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|       65 | 12520 | `				if( iNest <= 0 ){` |
|        7 | 12521 | `					break;` |
|        - | 12522 | `				}` |
|       27 | 12523 | `			}` |
|   312501 | 12524 | `			pEnd2++;` |
|        5 | 12525 | `		}` |
|   235203 | 12526 | `		if( pEnd2 <pEnd ){` |
|        7 | 12527 | `			pEnd = pEnd2;` |
|        3 | 12528 | `		}` |
|   117599 | 12529 | `	}` |
|  6995005 | 12530 | `	if( pEnd > pGen->pIn ){` |
|  6994995 | 12531 | `		SyToken *pTmp = pGen->pEnd;` |
|        - | 12532 | `		/* Swap delimiter */` |
|  6994995 | 12533 | `		pGen->pEnd = pEnd;` |
|        - | 12534 | `		/* Try to get an expression tree */` |
|  6994995 | 12535 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  6994995 | 12536 | `		if( rc == SXRET_OK && pRoot ){` |
|  6994813 | 12537 | `			rc = SXRET_OK;` |
|  6994813 | 12538 | `			if( xTreeValidator ){` |
|        - | 12539 | `				/* Call the upper layer validator callback */` |
|   550039 | 12540 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   275017 | 12541 | `			}` |
|  6994813 | 12542 | `			if( rc != SXERR_ABORT ){` |
|        - | 12543 | `				/* Generate code for the given tree */` |
|  6994813 | 12544 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|        - | 12545 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|        - | 12546 | `				 * expression so they short-circuit to its end. */` |
|  6994813 | 12547 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  3497404 | 12548 | `			}` |
|  6994813 | 12549 | `			nExpr = 1;` |
|  3497404 | 12550 | `		}` |
|        - | 12551 | `		/* Release the whole tree */` |
|  6994995 | 12552 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|        - | 12553 | `		/* Synchronize token stream */` |
|  6994995 | 12554 | `		pGen->pEnd = pTmp;` |
|  6994995 | 12555 | `		pGen->pIn  = pEnd;` |
|  6994995 | 12556 | `		if( rc == SXERR_ABORT ){` |
|       13 | 12557 | `			SySetRelease(&sExprNode);` |
|       13 | 12558 | `			return SXERR_ABORT;` |
|        - | 12559 | `		}` |
|  3497490 | 12560 | `	}` |
|  6994995 | 12561 | `	SySetRelease(&sExprNode);` |
|  6994995 | 12562 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  3497505 | 12563 | `}` |
|        - | 12564 | `/*` |
|        - | 12565 | ` * Return a pointer to the node construct handler associated` |
|        - | 12566 | ` * with a given node type [i.e: string,integer,float,...].` |
|        - | 12567 | ` */` |
|  4181500 | 12568 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|        5 | 12569 | `{` |
|  4181505 | 12570 | `	if( nNodeType & PH7_TK_NUM ){` |
|        - | 12571 | `		/* Numeric literal: Either real or integer */` |
|  1274577 | 12572 | `		return PH7_CompileNumLiteral;` |
|  2906933 | 12573 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|        - | 12574 | `		/* Double quoted string */` |
|    34439 | 12575 | `		return PH7_CompileString;` |
|  2872499 | 12576 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|        - | 12577 | `		/* Single quoted string */` |
|  2872379 | 12578 | `		return PH7_CompileSimpleString;` |
|      125 | 12579 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|        - | 12580 | `		/* Heredoc */` |
|       71 | 12581 | `		return PH7_CompileHereDoc;` |
|       58 | 12582 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|        - | 12583 | `		/* Nowdoc */` |
|       51 | 12584 | `		return PH7_CompileNowDoc;` |
|        9 | 12585 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|        - | 12586 | `		/* Backtick quoted string */` |
|        6 | 12587 | `		return PH7_CompileBacktic;` |
|        - | 12588 | `	}` |
|        3 | 12589 | `	return 0;` |
|  2090755 | 12590 | `}` |
|        - | 12591 | `/*` |
|        - | 12592 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|        - | 12593 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|        - | 12594 | ` * in write context" parse error.` |
|        - | 12595 | ` */` |
|     6720 | 12596 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|        5 | 12597 | `{` |
|        - | 12598 | `	sxi32 rc;` |
|     6725 | 12599 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|     6723 | 12600 | `		return SXRET_OK;` |
|        - | 12601 | `	}` |
|        5 | 12602 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|        2 | 12603 | `		pNode ? pNode->pStart->nLine : 1,` |
|        - | 12604 | `		"Can't use nullsafe operator in write context");` |
|        3 | 12605 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     3365 | 12606 | `}` |
|        - | 12607 | `/*` |
|        - | 12608 | ` * Compile an unset() statement.` |
|        - | 12609 | ` * unset($var, $arr[$key], ...);` |
|        - | 12610 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|        - | 12611 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|        - | 12612 | ` * parent array before extracting the element to unset.` |
|        - | 12613 | ` */` |
|     2876 | 12614 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|        5 | 12615 | `{` |
|     2881 | 12616 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     2881 | 12617 | `	sxu32 nIdx = 0;` |
|        - | 12618 | `	SyString sName;` |
|        - | 12619 | `	sxi32 rc;` |
|        - | 12620 | `	/* Jump the 'unset' keyword */` |
|     2881 | 12621 | `	pGen->pIn++;` |
|        - | 12622 | `	/* Save delimiter */` |
|     2881 | 12623 | `	pTmp = pGen->pEnd;` |
|        - | 12624 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     2881 | 12625 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     2881 | 12626 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        - | 12627 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|        - | 12628 | `		SyToken *pClose;` |
|     2881 | 12629 | `		pGen->pIn++;   /* Skip '(' */` |
|     2881 | 12630 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     2881 | 12631 | `		pEnd = pClose; /* Stop at ')' */` |
|     1438 | 12632 | `	}` |
|     2881 | 12633 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|        - | 12634 | `	/* Resolve the 'unset' builtin name once */` |
|     2881 | 12635 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      369 | 12636 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      369 | 12637 | `		if( pObj == 0 ){` |
|      ! 0 | 12638 | `			return SXERR_ABORT;` |
|        - | 12639 | `		}` |
|      369 | 12640 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      369 | 12641 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      182 | 12642 | `	}` |
|        - | 12643 | `	/* Compile each comma-separated argument */` |
|     9603 | 12644 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     6727 | 12645 | `		if( pGen->pIn < pNext ){` |
|     6727 | 12646 | `			pGen->pEnd = pNext;` |
|     6727 | 12647 | `			rc = PH7_CompileExpr(&(*pGen),` |
|        - | 12648 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|        - | 12649 | `				GenStateUnsetValidator);` |
|     6727 | 12650 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 12651 | `				return SXERR_ABORT;` |
|        - | 12652 | `			}` |
|     6727 | 12653 | `			if( rc != SXERR_EMPTY ){` |
|        - | 12654 | `				/* Emit call for this single argument */` |
|     6725 | 12655 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     6725 | 12656 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|     6725 | 12657 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     3360 | 12658 | `			}` |
|     3361 | 12659 | `		}` |
|        - | 12660 | `		/* Jump trailing commas */` |
|    10575 | 12661 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|     3853 | 12662 | `			pNext++;` |
|        5 | 12663 | `		}` |
|     6727 | 12664 | `		pGen->pIn = pNext;` |
|        5 | 12665 | `	}` |
|        - | 12666 | `	/* Skip past the closing ')' if present */` |
|     2881 | 12667 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     2881 | 12668 | `		pGen->pIn++;` |
|     1438 | 12669 | `	}` |
|        - | 12670 | `	/* Restore token stream */` |
|     2881 | 12671 | `	pGen->pEnd = pTmp;` |
|     2881 | 12672 | `	return SXRET_OK;` |
|     1443 | 12673 | `}` |
|        - | 12674 | `/*` |
|        - | 12675 | ` * PHP Language construct table.` |
|        - | 12676 | ` */` |
|        - | 12677 | `static const LangConstruct aLangConstruct[] = {` |
|        - | 12678 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|        - | 12679 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|        - | 12680 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|        - | 12681 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|        - | 12682 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|        - | 12683 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|        - | 12684 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|        - | 12685 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|        - | 12686 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|        - | 12687 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|        - | 12688 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|        - | 12689 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|        - | 12690 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|        - | 12691 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|        - | 12692 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|        - | 12693 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|        - | 12694 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|        - | 12695 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|        - | 12696 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|        - | 12697 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|        - | 12698 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|        - | 12699 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|        - | 12700 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|        - | 12701 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|        - | 12702 | `};` |
|        - | 12703 | `/*` |
|        - | 12704 | ` * Return a pointer to the statement handler routine associated` |
|        - | 12705 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|        - | 12706 | ` */` |
|  3712124 | 12707 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|        - | 12708 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|        - | 12709 | `	SyToken *pLookahed  /* Look-ahead token */` |
|        - | 12710 | `	)` |
|        5 | 12711 | `{` |
|  3712129 | 12712 | `	sxu32 n = 0;` |
| 15143527 | 12713 | `	for(;;){` |
| 30287059 | 12714 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   240253 | 12715 | `			break;` |
|        - | 12716 | `		}` |
| 30046811 | 12717 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  3471881 | 12718 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|      ! 0 | 12719 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|      ! 0 | 12720 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|        - | 12721 | `					/* 'static' (class context),return null */` |
|      ! 0 | 12722 | `					return 0;` |
|        - | 12723 | `				}` |
|      ! 0 | 12724 | `			}` |
|  3471876 | 12725 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       10 | 12726 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       10 | 12727 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|        - | 12728 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|      ! 0 | 12729 | `				return 0;` |
|        - | 12730 | `			}` |
|        - | 12731 | `			/* Return a pointer to the handler.` |
|        - | 12732 | `			*/` |
|  3471881 | 12733 | `			return aLangConstruct[n].xConstruct;` |
|        - | 12734 | `		}` |
| 26574935 | 12735 | `		n++;` |
|        5 | 12736 | `	}` |
|   240253 | 12737 | `	if( pLookahed ){` |
|   240253 | 12738 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    46225 | 12739 | `			return PH7_CompileClassInterface;` |
|   194033 | 12740 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   185843 | 12741 | `			return PH7_CompileClass;` |
|     8195 | 12742 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|       73 | 12743 | `			return PH7_CompileTrait;` |
|        - | 12744 | `		}` |
|        - | 12745 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|        - | 12746 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|        - | 12747 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|        - | 12748 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     4061 | 12749 | `	}` |
|        - | 12750 | `	/* Not a language construct */` |
|     8127 | 12751 | `	return 0;` |
|  1856067 | 12752 | `}` |
|        - | 12753 | `/*` |
|        - | 12754 | ` * Check if the given keyword is in fact a PHP language construct.` |
|        - | 12755 | ` * Return TRUE on success. FALSE otheriwse.` |
|        - | 12756 | ` */` |
|     8122 | 12757 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|        5 | 12758 | `{` |
|        - | 12759 | `	int rc;` |
|     8127 | 12760 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     8127 | 12761 | `	if( rc == FALSE ){` |
|     8008 | 12762 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      319 | 12763 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|        - | 12764 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|        - | 12765 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|        - | 12766 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|        - | 12767 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|        - | 12768 | `			*/` |
|        - | 12769 | `			){` |
|     8005 | 12770 | `				rc = TRUE;` |
|     4000 | 12771 | `		}` |
|     4004 | 12772 | `	}` |
|     8127 | 12773 | `	return rc;` |
|        5 | 12774 | `}` |
|        - | 12775 | `/*` |
|        - | 12776 | ` * Compile a PHP chunk.` |
|        - | 12777 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|        - | 12778 | ` * takes care of generating the appropriate error message.` |
|        - | 12779 | ` */` |
|        - | 12780 | `/*` |
|        - | 12781 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|        - | 12782 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|        - | 12783 | ` * the chunk token set it becomes the pending docblock. An existing` |
|        - | 12784 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|        - | 12785 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|        - | 12786 | ` * intervening non-declaration statements.` |
|        - | 12787 | ` */` |
|  7781304 | 12788 | `static void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|        5 | 12789 | `{` |
|  7781309 | 12790 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  7781309 | 12791 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  7781309 | 12792 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|        - | 12793 | `	sxu32 nIdx, n;` |
|  7781304 | 12794 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|  1520859 | 12795 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|        - | 12796 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|        - | 12797 | `		 * indexes do not map to the sidecar */` |
|  6260455 | 12798 | `		return;` |
|        - | 12799 | `	}` |
|  1520859 | 12800 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|        - | 12801 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|        - | 12802 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|  1520859 | 12803 | `	SySetReset(&pGen->aPendingAttrs);` |
|  4563045 | 12804 | `	for( n = 0 ; n < nT ; n++ ){` |
|  3042191 | 12805 | `		if( aT[n].nTokIdx != nIdx ){` |
|  3034375 | 12806 | `			continue;` |
|        - | 12807 | `		}` |
|     7821 | 12808 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|       29 | 12809 | `			pGen->sPendingDoc = aT[n].sText;` |
|     7809 | 12810 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|     7797 | 12811 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|     3896 | 12812 | `		}` |
|     3913 | 12813 | `	}` |
|  3890657 | 12814 | `}` |
|        - | 12815 | `/*` |
|        - | 12816 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|        - | 12817 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|        - | 12818 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|        - | 12819 | ` */` |
|  2100404 | 12820 | `static void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|        5 | 12821 | `{` |
|        - | 12822 | `	char *zDup;` |
|  2100409 | 12823 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|  2100389 | 12824 | `		return;` |
|        - | 12825 | `	}` |
|       35 | 12826 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       10 | 12827 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|       25 | 12828 | `	if( zDup ){` |
|       25 | 12829 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|       10 | 12830 | `	}` |
|       25 | 12831 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|  1050207 | 12832 | `}` |
|        - | 12833 | `/*` |
|        - | 12834 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|        - | 12835 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|        - | 12836 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|        - | 12837 | ` * names may point into the token text, which must outlive the raw script` |
|        - | 12838 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|        - | 12839 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|        - | 12840 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|        - | 12841 | ` */` |
|     7796 | 12842 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|        5 | 12843 | `{` |
|        - | 12844 | `	SySet *pToken;` |
|        - | 12845 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|        - | 12846 | `	char *zSpan;` |
|     7801 | 12847 | `	sxi32 rc = SXRET_OK;` |
|     7801 | 12848 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|      ! 0 | 12849 | `		return SXRET_OK;` |
|        - | 12850 | `	}` |
|    11699 | 12851 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     3898 | 12852 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|     7801 | 12853 | `	if( zSpan == 0 ){` |
|      ! 0 | 12854 | `		return SXRET_OK;` |
|        - | 12855 | `	}` |
|        - | 12856 | `	/* The token set must outlive compilation too: interned operands may` |
|        - | 12857 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|        - | 12858 | `	 * the number of attribute declarations in the program. */` |
|     7801 | 12859 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|     7801 | 12860 | `	if( pToken == 0 ){` |
|      ! 0 | 12861 | `		return SXRET_OK;` |
|        - | 12862 | `	}` |
|     7801 | 12863 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|     7801 | 12864 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|     7801 | 12865 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|     7801 | 12866 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|     7801 | 12867 | `	pSavedIn = pGen->pIn;` |
|     7801 | 12868 | `	pSavedEnd = pGen->pEnd;` |
|     7805 | 12869 | `	while( pIn < pEnd ){` |
|        - | 12870 | `		ph7_attribute sAttr;` |
|        - | 12871 | `		SyBlob sFQN;` |
|     7805 | 12872 | `		int bAbsolute = 0;` |
|     7805 | 12873 | `		SyZero(&sAttr,sizeof(sAttr));` |
|     7805 | 12874 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|     7805 | 12875 | `		sAttr.nLine = pIn->nLine;` |
|     7805 | 12876 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|       49 | 12877 | `			bAbsolute = 1;` |
|       49 | 12878 | `			pIn++;` |
|       22 | 12879 | `		}` |
|     7805 | 12880 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     7805 | 12881 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     7805 | 12882 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|     7805 | 12883 | `			pIn++;` |
|     7805 | 12884 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|      ! 0 | 12885 | `				SyBlobAppend(&sFQN,"\\",1);` |
|      ! 0 | 12886 | `				pIn++;` |
|      ! 0 | 12887 | `				continue;` |
|        - | 12888 | `			}` |
|     7805 | 12889 | `			break;` |
|      ! 0 | 12890 | `		}` |
|     7805 | 12891 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|        - | 12892 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|        - | 12893 | `			 * this feature; never turn it into a new fatal) */` |
|      ! 0 | 12894 | `			SyBlobRelease(&sFQN);` |
|      ! 0 | 12895 | `			break;` |
|        - | 12896 | `		}` |
|        - | 12897 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|        - | 12898 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|        - | 12899 | `		{` |
|     7805 | 12900 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|     7805 | 12901 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|     7805 | 12902 | `			char *zDup = 0;` |
|     7805 | 12903 | `			if( !bAbsolute ){` |
|     7761 | 12904 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|     7761 | 12905 | `				if( pImp ){` |
|      ! 0 | 12906 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|      ! 0 | 12907 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|      ! 0 | 12908 | `					if( zDup ){` |
|      ! 0 | 12909 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|      ! 0 | 12910 | `					}` |
|     7761 | 12911 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        - | 12912 | `					SyBlob sTmp;` |
|      ! 0 | 12913 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|      ! 0 | 12914 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      ! 0 | 12915 | `					SyBlobAppend(&sTmp,"\\",1);` |
|      ! 0 | 12916 | `					SyBlobAppend(&sTmp,zName,nName);` |
|      ! 0 | 12917 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      ! 0 | 12918 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|      ! 0 | 12919 | `					if( zDup ){` |
|      ! 0 | 12920 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|      ! 0 | 12921 | `					}` |
|      ! 0 | 12922 | `					SyBlobRelease(&sTmp);` |
|      ! 0 | 12923 | `				}` |
|     3878 | 12924 | `			}` |
|     7805 | 12925 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|     7805 | 12926 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|     7805 | 12927 | `				if( zDup ){` |
|     7805 | 12928 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|     3900 | 12929 | `				}` |
|     3900 | 12930 | `			}` |
|        - | 12931 | `		}` |
|     7805 | 12932 | `		SyBlobRelease(&sFQN);` |
|     7805 | 12933 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        - | 12934 | `			SyToken *pArgsEnd;` |
|     7729 | 12935 | `			pIn++;` |
|     7729 | 12936 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|    15461 | 12937 | `			while( pIn < pArgsEnd ){` |
|     7737 | 12938 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|     7737 | 12939 | `				sxi32 iDepth = 0;` |
|        - | 12940 | `				ph7_attr_arg sArgRec;` |
|    77049 | 12941 | `				while( pArgStop < pArgsEnd ){` |
|    69327 | 12942 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       11 | 12943 | `						iDepth++;` |
|    69322 | 12944 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       11 | 12945 | `						iDepth--;` |
|    69312 | 12946 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|       11 | 12947 | `						break;` |
|        - | 12948 | `					}` |
|    69317 | 12949 | `					pArgStop++;` |
|        5 | 12950 | `				}` |
|     7737 | 12951 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|     7737 | 12952 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     7732 | 12953 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|     7721 | 12954 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        7 | 12955 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        2 | 12956 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|        5 | 12957 | `					if( zN ){` |
|        5 | 12958 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|        2 | 12959 | `					}` |
|        5 | 12960 | `					pArgStart += 2;` |
|        2 | 12961 | `				}` |
|     7737 | 12962 | `				if( pArgStart < pArgStop ){` |
|        - | 12963 | `					SySet *pInstrContainer;` |
|     7737 | 12964 | `					pGen->pIn = pArgStart;` |
|     7737 | 12965 | `					pGen->pEnd = pArgStop;` |
|     7737 | 12966 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     7737 | 12967 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|     7737 | 12968 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|     7737 | 12969 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|     7737 | 12970 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     7737 | 12971 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 12972 | `						pGen->pIn = pSavedIn;` |
|      ! 0 | 12973 | `						pGen->pEnd = pSavedEnd;` |
|      ! 0 | 12974 | `						return SXERR_ABORT;` |
|        - | 12975 | `					}` |
|     7737 | 12976 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|     3866 | 12977 | `				}` |
|     7737 | 12978 | `				pIn = pArgStop;` |
|     7737 | 12979 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|       11 | 12980 | `					pIn++;` |
|        5 | 12981 | `				}` |
|        5 | 12982 | `			}` |
|     7729 | 12983 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|     3862 | 12984 | `		}` |
|     7805 | 12985 | `		SySetPut(pOut,(const void *)&sAttr);` |
|     7805 | 12986 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        5 | 12987 | `			pIn++;` |
|        5 | 12988 | `			continue;` |
|        - | 12989 | `		}` |
|     7801 | 12990 | `		break;` |
|      ! 0 | 12991 | `	}` |
|     7801 | 12992 | `	pGen->pIn = pSavedIn;` |
|     7801 | 12993 | `	pGen->pEnd = pSavedEnd;` |
|     7801 | 12994 | `	return SXRET_OK;` |
|     3903 | 12995 | `}` |
|        - | 12996 | `/*` |
|        - | 12997 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|        - | 12998 | ` * every recorded group into pOut and clear the pending list.` |
|        - | 12999 | ` */` |
|  2100404 | 13000 | `static sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|        5 | 13001 | `{` |
|  2100409 | 13002 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|        - | 13003 | `	sxu32 n;` |
|        - | 13004 | `	sxi32 rc;` |
|  2108201 | 13005 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|     7797 | 13006 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|     7797 | 13007 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 13008 | `			return SXERR_ABORT;` |
|        - | 13009 | `		}` |
|     3901 | 13010 | `	}` |
|  2100409 | 13011 | `	SySetReset(&pGen->aPendingAttrs);` |
|  2100409 | 13012 | `	return SXRET_OK;` |
|  1050207 | 13013 | `}` |
|        - | 13014 | `/*` |
|        - | 13015 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|        - | 13016 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|        - | 13017 | ` * the main token stream, so the sidecar indexes map directly.` |
|        - | 13018 | ` */` |
|   709708 | 13019 | `static sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|        5 | 13020 | `{` |
|   709713 | 13021 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   709713 | 13022 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   709713 | 13023 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|        - | 13024 | `	sxu32 nIdx, n;` |
|        - | 13025 | `	sxi32 rc;` |
|   709708 | 13026 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   192463 | 13027 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   517255 | 13028 | `		return SXRET_OK;` |
|        - | 13029 | `	}` |
|   192463 | 13030 | `	nIdx = (sxu32)(pTok - pBase);` |
|   577263 | 13031 | `	for( n = 0 ; n < nT ; n++ ){` |
|   384805 | 13032 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|        5 | 13033 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|        5 | 13034 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 13035 | `				return SXERR_ABORT;` |
|        - | 13036 | `			}` |
|        2 | 13037 | `		}` |
|   192405 | 13038 | `	}` |
|   192463 | 13039 | `	return SXRET_OK;` |
|   354859 | 13040 | `}` |
|  5699758 | 13041 | `static sxi32 GenStateCompileChunk(` |
|        - | 13042 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 13043 | `	sxi32 iFlags         /* Compile flags */` |
|        - | 13044 | `	)` |
|        5 | 13045 | `{` |
|        - | 13046 | `	ProcLangConstruct xCons;` |
|        - | 13047 | `	sxi32 rc;` |
|  5699763 | 13048 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  3259847 | 13049 | `	for(;;){` |
|  6109731 | 13050 | `		int bStmtIsDeclare = 0;` |
|  6109731 | 13051 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 13052 | `			/* No more input to process */` |
|    52795 | 13053 | `			break;` |
|        - | 13054 | `		}` |
|        - | 13055 | `		/* Bind a directly-preceding docblock to this statement */` |
|  6056941 | 13056 | `		GenStateSetPendingDoc(&(*pGen));` |
|        - | 13057 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|        - | 13058 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  6056941 | 13059 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  3739079 | 13060 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  3739079 | 13061 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|       47 | 13062 | `				bStmtIsDeclare = 1;` |
|       21 | 13063 | `			}` |
|  1869537 | 13064 | `		}` |
|  6056941 | 13065 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|        - | 13066 | `			/* Any non-declare top-level statement locks the strict_types` |
|        - | 13067 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|   409941 | 13068 | `			pGen->bStrictTypesLocked = 1;` |
|   204968 | 13069 | `		}` |
|  6056941 | 13070 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|        - | 13071 | `			/* Compile block */` |
|     3867 | 13072 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|     3867 | 13073 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 13074 | `				break;` |
|        - | 13075 | `			}` |
|     1936 | 13076 | `		}else{` |
|  6053079 | 13077 | `			xCons = 0;` |
|  6053079 | 13078 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|        - | 13079 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|        - | 13080 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|        - | 13081 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|    26981 | 13082 | `				xCons = PH7_CompileClassModifiers;` |
|  6039591 | 13083 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  3712129 | 13084 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        - | 13085 | `				/* Try to extract a language construct handler */` |
|  3712129 | 13086 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  3712129 | 13087 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|       13 | 13088 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 13089 | `						"Syntax error: Unexpected keyword '%z'",` |
|        8 | 13090 | `						&pGen->pIn->sData);` |
|        9 | 13091 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 13092 | `						break;` |
|        - | 13093 | `					}` |
|        - | 13094 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|        - | 13095 | `					 * this erroneous statement.` |
|        - | 13096 | `					 */` |
|        9 | 13097 | `					xCons = PH7_ErrorRecover;` |
|        4 | 13098 | `				}` |
|  4170041 | 13099 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    65537 | 13100 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|        - | 13101 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|      117 | 13102 | `				xCons = PH7_CompileLabel;` |
|       56 | 13103 | `			}` |
|  6053079 | 13104 | `			if( xCons == 0 ){` |
|        - | 13105 | `				/* Assume an expression an try to compile it */` |
|  2321981 | 13106 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  2321981 | 13107 | `				if(  rc != SXERR_EMPTY ){` |
|        - | 13108 | `					/* Pop l-value */` |
|  2321831 | 13109 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  1160913 | 13110 | `				}` |
|  1160993 | 13111 | `			}else{` |
|        - | 13112 | `				/* Go compile the sucker */` |
|  3731103 | 13113 | `				rc = xCons(&(*pGen));` |
|        - | 13114 | `			}` |
|  6053079 | 13115 | `			if( rc == SXERR_ABORT ){` |
|        - | 13116 | `				/* Request to abort compilation */` |
|       13 | 13117 | `				break;` |
|        - | 13118 | `			}` |
|        - | 13119 | `		}` |
|        - | 13120 | `		/* Ignore trailing semi-colons ';' */` |
| 10357065 | 13121 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  4300139 | 13122 | `			pGen->pIn++;` |
|        5 | 13123 | `		}` |
|  6056931 | 13124 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|        - | 13125 | `			/* Compile a single statement and return */` |
|  5646963 | 13126 | `			break;` |
|        - | 13127 | `		}` |
|        - | 13128 | `		/* LOOP ONE */` |
|        - | 13129 | `		/* LOOP TWO */` |
|        - | 13130 | `		/* LOOP THREE */` |
|        - | 13131 | `		/* LOOP FOUR */` |
|        5 | 13132 | `	}` |
|        - | 13133 | `	/* Return compilation status */` |
|  5699763 | 13134 | `	return rc;` |
|        5 | 13135 | `}` |
|        - | 13136 | `/*` |
|        - | 13137 | ` * Compile a Raw PHP chunk.` |
|        - | 13138 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|        - | 13139 | ` * takes care of generating the appropriate error message.` |
|        - | 13140 | ` */` |
|    52802 | 13141 | `static sxi32 PH7_CompilePHP(` |
|        - | 13142 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - | 13143 | `	SySet *pTokenSet,     /* Token set */` |
|        - | 13144 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|        - | 13145 | `	)` |
|        5 | 13146 | `{` |
|    52807 | 13147 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|        - | 13148 | `	sxi32 rc;` |
|        - | 13149 | `	/* Reset the token set (and its trivia sidecar) */` |
|    52807 | 13150 | `	SySetReset(&(*pTokenSet));` |
|    52807 | 13151 | `	SySetReset(&pGen->aTrivia);` |
|        - | 13152 | `	/* Mark as the default token set */` |
|    52807 | 13153 | `	pGen->pTokenSet = &(*pTokenSet);` |
|        - | 13154 | `	/* Advance the stream cursor */` |
|    52807 | 13155 | `	pGen->pRawIn++;` |
|        - | 13156 | `	/* Tokenize the PHP chunk first */` |
|    52807 | 13157 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|        - | 13158 | `	/* Point to the head and tail of the token stream. */` |
|    52807 | 13159 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|    52807 | 13160 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|    52807 | 13161 | `	if( is_expr ){` |
|      ! 0 | 13162 | `		rc = SXERR_EMPTY;` |
|      ! 0 | 13163 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - | 13164 | `			/* A simple expression,compile it */` |
|      ! 0 | 13165 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|      ! 0 | 13166 | `		}` |
|        - | 13167 | `		/* Emit the DONE instruction */` |
|      ! 0 | 13168 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      ! 0 | 13169 | `		return SXRET_OK;` |
|        - | 13170 | `	}` |
|    52807 | 13171 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|        - | 13172 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|        - | 13173 | `		/*` |
|        - | 13174 | `		 * Shortcut syntax for the 'echo' language construct.` |
|        - | 13175 | `		 * According to the PHP reference manual:` |
|        - | 13176 | `		 *  echo() also has a shortcut syntax, where you can` |
|        - | 13177 | `		 *  immediately follow` |
|        - | 13178 | `		 *  the opening tag with an equals sign as follows:` |
|        - | 13179 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|        - | 13180 | `		 * Symisc extension:` |
|        - | 13181 | `		 *   This short syntax works with all PHP opening` |
|        - | 13182 | `		 *   tags unlike the default PHP engine that handle` |
|        - | 13183 | `		 *   only short tag.` |
|        - | 13184 | `		 */` |
|        - | 13185 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|        3 | 13186 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|        3 | 13187 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|        3 | 13188 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|        3 | 13189 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|        3 | 13190 | `		if( rc != SXERR_EMPTY ){` |
|        3 | 13191 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        1 | 13192 | `		}` |
|        3 | 13193 | `		return SXRET_OK;` |
|        - | 13194 | `	}` |
|        - | 13195 | `	/* Compile the PHP chunk */` |
|    52805 | 13196 | `	rc = GenStateCompileChunk(pGen,0);` |
|        - | 13197 | `	/* Fix exceptions jumps */` |
|    52805 | 13198 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        - | 13199 | `	/* Fix gotos now, the jump destination is resolved */` |
|    52805 | 13200 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|        3 | 13201 | `		rc = SXERR_ABORT;` |
|        1 | 13202 | `	}` |
|        - | 13203 | `	/* Reset container */` |
|    52805 | 13204 | `	SySetReset(&pGen->aGoto);` |
|    52805 | 13205 | `	SySetReset(&pGen->aLabel);` |
|    52805 | 13206 | `	SySetReset(&pGen->aNullsafeJmp);` |
|        - | 13207 | `	/* Compilation result */` |
|    52805 | 13208 | `	return rc;` |
|    26406 | 13209 | `}` |
|        - | 13210 | `/*` |
|        - | 13211 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|        - | 13212 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|        - | 13213 | ` * This is the only compile interface exported from this file.` |
|        - | 13214 | ` */` |
|    55816 | 13215 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|        - | 13216 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|        - | 13217 | `	SyString *pScript,  /* Script to compile */` |
|        - | 13218 | `	sxi32 iFlags        /* Compile flags */` |
|        - | 13219 | `	)` |
|        5 | 13220 | `{` |
|        - | 13221 | `	SySet aPhpToken,aRawToken;` |
|        - | 13222 | `	ph7_gen_state *pCodeGen;` |
|        - | 13223 | `	ph7_value *pRawObj;` |
|        - | 13224 | `	sxu32 nObjIdx;` |
|        - | 13225 | `	sxi32 nRawObj;` |
|        - | 13226 | `	int is_expr;` |
|        - | 13227 | `	sxi8 bSavedStrict;` |
|        - | 13228 | `	sxi8 bSavedStrictLocked;` |
|        - | 13229 | `	sxi32 rc;` |
|    55821 | 13230 | `	if( pScript->nByte < 1 ){` |
|        - | 13231 | `		/* Nothing to compile */` |
|      ! 0 | 13232 | `		return PH7_OK;` |
|        - | 13233 | `	}` |
|        - | 13234 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|        - | 13235 | `	 * file's flags so include/require restore them on return. */` |
|    55821 | 13236 | `	pCodeGen = &pVm->sCodeGen;` |
|    55821 | 13237 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|    55821 | 13238 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|    55821 | 13239 | `	pCodeGen->bStrictTypes = 0;` |
|    55821 | 13240 | `	pCodeGen->bStrictTypesLocked = 0;` |
|        - | 13241 | `	/* Initialize the tokens containers */` |
|    55821 | 13242 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|    55821 | 13243 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|    55821 | 13244 | `	SySetAlloc(&aPhpToken,0xc0);` |
|    55821 | 13245 | `	is_expr = 0;` |
|    55821 | 13246 | `	if( iFlags & PH7_PHP_ONLY ){` |
|        - | 13247 | `		SyToken sTmp;` |
|        - | 13248 | `		/* PHP only: -*/` |
|    42387 | 13249 | `		sTmp.nLine = 1;` |
|    42387 | 13250 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    42387 | 13251 | `		sTmp.pUserData = 0;` |
|    42387 | 13252 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    42387 | 13253 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    42387 | 13254 | `		if( iFlags & PH7_PHP_EXPR ){` |
|        - | 13255 | `			/* A simple PHP expression */` |
|      ! 0 | 13256 | `			is_expr = 1;` |
|      ! 0 | 13257 | `		}` |
|    21196 | 13258 | `	}else{` |
|        - | 13259 | `		/* Tokenize raw text */` |
|    13439 | 13260 | `		SySetAlloc(&aRawToken,32);` |
|    13439 | 13261 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|        - | 13262 | `	}` |
|        - | 13263 | `	/* Process high-level tokens */` |
|    55821 | 13264 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|    55821 | 13265 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|    55821 | 13266 | `	rc = PH7_OK;` |
|    55821 | 13267 | `	if( is_expr ){` |
|        - | 13268 | `		/* Compile the expression */` |
|      ! 0 | 13269 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|      ! 0 | 13270 | `		goto cleanup;` |
|        - | 13271 | `	}` |
|    55821 | 13272 | `	nObjIdx = 0;` |
|        - | 13273 | `	/* Each compilation unit starts in the global namespace.` |
|        - | 13274 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|        - | 13275 | `	 * preventing namespace bleeding across include()d files. */` |
|    55821 | 13276 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|        - | 13277 | `	/* Start the compilation process */` |
|    34631 | 13278 | `	for(;;){` |
|   122057 | 13279 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|    55809 | 13280 | `			break; /* No more tokens to process */` |
|        - | 13281 | `		}` |
|    66253 | 13282 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|        - | 13283 | `			/* Compile the PHP chunk */` |
|    52807 | 13284 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|    52807 | 13285 | `			if( rc == SXERR_ABORT ){` |
|       15 | 13286 | `				break;` |
|        - | 13287 | `			}` |
|    52795 | 13288 | `			continue;` |
|        - | 13289 | `		}` |
|        - | 13290 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|    13451 | 13291 | `		nRawObj = 0;` |
|    26939 | 13292 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|        - | 13293 | `			/* Consume the raw chunk without any processing */` |
|    13493 | 13294 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|    13493 | 13295 | `			if( pRawObj == 0 ){` |
|      ! 0 | 13296 | `				rc = SXERR_MEM;` |
|      ! 0 | 13297 | `				break;` |
|        - | 13298 | `			}` |
|        - | 13299 | `			/* Mark as constant and emit the load constant instruction */` |
|    13493 | 13300 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|    13493 | 13301 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|    13493 | 13302 | `			++nRawObj;` |
|    13493 | 13303 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|        5 | 13304 | `		}` |
|    13451 | 13305 | `		if( nRawObj > 0 ){` |
|        - | 13306 | `			/* Emit the consume instruction */` |
|    13451 | 13307 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     6723 | 13308 | `		}` |
|    27913 | 13309 | `	}` |
|    27908 | 13310 | `cleanup:` |
|    55821 | 13311 | `	SySetRelease(&aRawToken);` |
|    55821 | 13312 | `	SySetRelease(&aPhpToken);` |
|        - | 13313 | `	/* Restore outer file's strict_types scope */` |
|    55821 | 13314 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|    55821 | 13315 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|    55821 | 13316 | `	return rc;` |
|    27913 | 13317 | `}` |
|        - | 13318 | `/*` |
|        - | 13319 | ` * Utility routines.Initialize the code generator.` |
|        - | 13320 | ` */` |
|     3844 | 13321 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|        - | 13322 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 13323 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|        - | 13324 | `	void *pErrData     /* Last argument to xErr() */` |
|        - | 13325 | `	)` |
|        5 | 13326 | `{` |
|     3849 | 13327 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 13328 | `	/* Zero the structure */` |
|     3849 | 13329 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|        - | 13330 | `	/* Initial state */` |
|     3849 | 13331 | `	pGen->pVm  = &(*pVm);` |
|     3849 | 13332 | `	pGen->xErr = xErr;` |
|     3849 | 13333 | `	pGen->pErrData = pErrData;` |
|     3849 | 13334 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|     3849 | 13335 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|     3849 | 13336 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|     3849 | 13337 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|     3849 | 13338 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|     3849 | 13339 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|     3849 | 13340 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|        - | 13341 | `	/* Error log buffer */` |
|     3849 | 13342 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|        - | 13343 | `	/* General purpose working buffer */` |
|     3849 | 13344 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|        - | 13345 | `	/* Namespace state */` |
|     3849 | 13346 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     3849 | 13347 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|     3849 | 13348 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|     3849 | 13349 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|        - | 13350 | `	/* Create the global scope */` |
|     3849 | 13351 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|        - | 13352 | `	/* Point to the global scope */` |
|     3849 | 13353 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     3849 | 13354 | `	return SXRET_OK;` |
|        5 | 13355 | `}` |
|        - | 13356 | `/*` |
|        - | 13357 | ` * Utility routines. Reset the code generator to it's initial state.` |
|        - | 13358 | ` */` |
|    59288 | 13359 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|        - | 13360 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 13361 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|        - | 13362 | `	void *pErrData     /* Last argument to xErr() */` |
|        - | 13363 | `	)` |
|        5 | 13364 | `{` |
|    59293 | 13365 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 13366 | `	GenBlock *pBlock,*pParent;` |
|        - | 13367 | `	/* Reset state */` |
|    59293 | 13368 | `	SySetReset(&pGen->aLabel);` |
|    59293 | 13369 | `	SySetReset(&pGen->aGoto);` |
|    59293 | 13370 | `	SySetReset(&pGen->aNullsafeJmp);` |
|    59293 | 13371 | `	SySetReset(&pGen->aTrivia);` |
|    59293 | 13372 | `	SySetReset(&pGen->aPendingAttrs);` |
|    59293 | 13373 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|    59293 | 13374 | `	SyBlobRelease(&pGen->sErrBuf);` |
|    59293 | 13375 | `	SyBlobRelease(&pGen->sWorker);` |
|    59293 | 13376 | `	SyBlobRelease(&pGen->sNamespace);` |
|    59293 | 13377 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    59293 | 13378 | `	SyHashRelease(&pGen->hUseImports);` |
|    59293 | 13379 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    59293 | 13380 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|    59293 | 13381 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    59293 | 13382 | `	SyHashRelease(&pGen->hUseConstImports);` |
|    59293 | 13383 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|        - | 13384 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|        - | 13385 | `	 * They intern variable names and literal strings that are referenced by` |
|        - | 13386 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|        - | 13387 | `	 * Releasing them would either leak the interned strings or require freeing` |
|        - | 13388 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|        - | 13389 | `	 * number of unique names, which is acceptable. */` |
|        - | 13390 | `	/* Point to the global scope */` |
|    59293 | 13391 | `	pBlock = pGen->pCurrent;` |
|    59293 | 13392 | `	while( pBlock->pParent != 0 ){` |
|      ! 0 | 13393 | `		pParent = pBlock->pParent;` |
|      ! 0 | 13394 | `		GenStateFreeBlock(pBlock);` |
|      ! 0 | 13395 | `		pBlock = pParent;` |
|      ! 0 | 13396 | `	}` |
|    59293 | 13397 | `	pGen->xErr = xErr;` |
|    59293 | 13398 | `	pGen->pErrData = pErrData;` |
|    59293 | 13399 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    59293 | 13400 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|    59293 | 13401 | `	pGen->pIn = pGen->pEnd = 0;` |
|    59293 | 13402 | `	pGen->nErr = 0;` |
|    59293 | 13403 | `	return SXRET_OK;` |
|        5 | 13404 | `}` |
|        - | 13405 | `/*` |
|        - | 13406 | ` * Generate a compile-time error message.` |
|        - | 13407 | ` * If the error count limit is reached (usually 15 error message)` |
|        - | 13408 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|        - | 13409 | ` * abort compilation immediately.` |
|        - | 13410 | ` */` |
|      642 | 13411 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|        5 | 13412 | `{` |
|      647 | 13413 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|      647 | 13414 | `	const char *zErr = "Error";` |
|        - | 13415 | `	SyString *pFile;` |
|        - | 13416 | `	va_list ap;` |
|        - | 13417 | `	sxi32 rc;` |
|        - | 13418 | `	/* Reset the working buffer */` |
|      647 | 13419 | `	SyBlobReset(pWorker);` |
|        - | 13420 | `	/* Peek the processed file path if available */` |
|      647 | 13421 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|      647 | 13422 | `	if( nErrType == E_ERROR ){` |
|        - | 13423 | `		/* Increment the error counter */` |
|      533 | 13424 | `		pGen->nErr++;` |
|      533 | 13425 | `		if( pGen->nErr > 15 ){` |
|        - | 13426 | `			/* Error count limit reached */` |
|        6 | 13427 | `			if( pGen->xErr ){` |
|        6 | 13428 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|        6 | 13429 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|        6 | 13430 | `				if( pFile ){` |
|        6 | 13431 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|        2 | 13432 | `				}` |
|        6 | 13433 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|        6 | 13434 | `				if( SyBlobLength(pWorker) > 0 ){` |
|        6 | 13435 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|        2 | 13436 | `				}` |
|        2 | 13437 | `			}` |
|        - | 13438 | `			/* Abort immediately */` |
|        6 | 13439 | `			return SXERR_ABORT;` |
|        - | 13440 | `		}` |
|      262 | 13441 | `	}` |
|      643 | 13442 | `	if( pGen->xErr == 0 ){` |
|        - | 13443 | `		/* No available error consumer,return immediately */` |
|        3 | 13444 | `		return SXRET_OK;` |
|        - | 13445 | `	}` |
|      640 | 13446 | `	switch(nErrType){` |
|      526 | 13447 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|       32 | 13448 | `	case E_WARNING: zErr = "Warning";     break;` |
|       82 | 13449 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       12 | 13450 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|      ! 0 | 13451 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|      ! 0 | 13452 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|      ! 0 | 13453 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|      ! 0 | 13454 | `	default:` |
|      ! 0 | 13455 | `		break;` |
|        - | 13456 | `	}` |
|      640 | 13457 | `	rc = SXRET_OK;` |
|        - | 13458 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|      640 | 13459 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|      640 | 13460 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|      640 | 13461 | `	va_start(ap,zFormat);` |
|      640 | 13462 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|      640 | 13463 | `	va_end(ap);` |
|      640 | 13464 | `	if( pFile ){` |
|      640 | 13465 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|      318 | 13466 | `	}` |
|        - | 13467 | `	/* Append a new line */` |
|      640 | 13468 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|      640 | 13469 | `	if( SyBlobLength(pWorker) > 0 ){` |
|        - | 13470 | `		/* Consume the generated error message */` |
|      640 | 13471 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|      318 | 13472 | `	}` |
|      640 | 13473 | `	return rc;` |
|      326 | 13474 | `}` |
|        - | 13475 |  |
