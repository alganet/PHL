# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 7013/8687 lines (80.73%)

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
|      276 |   120 | `		if( SyStringCmp(&aLabel[n].sName,pName,SyMemcmp) == 0 ){` |
|        - |   121 | `			/* Jump destination found */` |
|       96 |   122 | `			aLabel[n].bRef = TRUE;` |
|       96 |   123 | `			if( ppOut ){` |
|       96 |   124 | `				*ppOut = &aLabel[n];` |
|       46 |   125 | `			}` |
|       96 |   126 | `			return SXRET_OK;` |
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
|    70060 |   137 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|        5 |   138 | `{` |
|    70065 |   139 | `	GenBlock *pBlock = pCurrent;` |
|   159193 |   140 | `	for(;;){` |
|   318391 |   141 | `		if( pBlock->iFlags & iBlockType ){` |
|    69957 |   142 | `			iCount--; /* Decrement nesting level */` |
|    69957 |   143 | `			if( iCount < 1 ){` |
|        - |   144 | `				/* Block meet with the desired criteria */` |
|    69931 |   145 | `				return pBlock;` |
|        - |   146 | `			}` |
|       13 |   147 | `		}` |
|        - |   148 | `		/* Point to the upper block */` |
|   248465 |   149 | `		pBlock = pBlock->pParent;` |
|   248465 |   150 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|        - |   151 | `			/* Forbidden */` |
|       72 |   152 | `			break;` |
|        - |   153 | `		}` |
|        5 |   154 | `	}` |
|        - |   155 | `	/* No such block */` |
|      139 |   156 | `	return 0;` |
|    35035 |   157 | `}` |
|        - |   158 | `/*` |
|        - |   159 | ` * Initialize a freshly allocated block instance.` |
|        - |   160 | ` */` |
|  7768794 |   161 | `static void GenStateInitBlock(` |
|        - |   162 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |   163 | `	GenBlock *pBlock,    /* Target block */` |
|        - |   164 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|        - |   165 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|        - |   166 | `	void *pUserData      /* Upper layer private data */` |
|        - |   167 | `	)` |
|        5 |   168 | `{` |
|        - |   169 | `	/* Initialize block fields */` |
|  7768799 |   170 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  7768799 |   171 | `	pBlock->pUserData   = pUserData;` |
|  7768799 |   172 | `	pBlock->pGen        = pGen;` |
|  7768799 |   173 | `	pBlock->iFlags      = iType;` |
|  7768799 |   174 | `	pBlock->pParent     = 0;` |
|  7768799 |   175 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  7768799 |   176 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  7768799 |   177 | `}` |
|        - |   178 | `/*` |
|        - |   179 | ` * Allocate a new block instance.` |
|        - |   180 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|        - |   181 | ` * on success.Otherwise generate a compile-time error and abort` |
|        - |   182 | ` * processing on failure.` |
|        - |   183 | ` */` |
|  7764918 |   184 | `static sxi32 GenStateEnterBlock(` |
|        - |   185 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - |   186 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|        - |   187 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|        - |   188 | `	void *pUserData,      /* Upper layer private data */` |
|        - |   189 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|        - |   190 | `	)` |
|        5 |   191 | `{` |
|        - |   192 | `	GenBlock *pBlock;` |
|        - |   193 | `	/* Allocate a new block instance */` |
|  7764923 |   194 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  7764923 |   195 | `	if( pBlock == 0 ){` |
|        - |   196 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   197 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   198 | `		 */` |
|      ! 0 |   199 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|        - |   200 | `		/* Abort processing immediately */` |
|      ! 0 |   201 | `		return SXERR_ABORT;` |
|        - |   202 | `	}` |
|        - |   203 | `	/* Zero the structure */` |
|  7764923 |   204 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  7764923 |   205 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|        - |   206 | `	/* Link to the parent block */` |
|  7764923 |   207 | `	pBlock->pParent = pGen->pCurrent;` |
|        - |   208 | `	/* Mark as the current block */` |
|  7764923 |   209 | `	pGen->pCurrent = pBlock;` |
|  7764923 |   210 | `	if( ppBlock ){` |
|        - |   211 | `		/* Write a pointer to the new instance */` |
|  3716437 |   212 | `		*ppBlock = pBlock;` |
|  1858216 |   213 | `	}` |
|  7764923 |   214 | `	return SXRET_OK;` |
|  3882464 |   215 | `}` |
|        - |   216 | `/*` |
|        - |   217 | ` * Release block fields without freeing the whole instance.` |
|        - |   218 | ` */` |
|  7764902 |   219 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|        5 |   220 | `{` |
|  7764907 |   221 | `	SySetRelease(&pBlock->aPostContFix);` |
|  7764907 |   222 | `	SySetRelease(&pBlock->aJumpFix);` |
|  7764907 |   223 | `}` |
|        - |   224 | `/*` |
|        - |   225 | ` * Release a block.` |
|        - |   226 | ` */` |
|  7764902 |   227 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|        5 |   228 | `{` |
|  7764907 |   229 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  7764907 |   230 | `	GenStateReleaseBlock(&(*pBlock));` |
|        - |   231 | `	/* Free the instance */` |
|  7764907 |   232 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  7764907 |   233 | `}` |
|        - |   234 | `/*` |
|        - |   235 | ` * POP and release a block from the stack of compiled blocks.` |
|        - |   236 | ` */` |
|  7764902 |   237 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|        5 |   238 | `{` |
|  7764907 |   239 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  7764907 |   240 | `	if( pBlock == 0 ){` |
|        - |   241 | `		/* No more block to pop */` |
|      ! 0 |   242 | `		return SXERR_EMPTY;` |
|        - |   243 | `	}` |
|        - |   244 | `	/* Point to the upper block */` |
|  7764907 |   245 | `	pGen->pCurrent = pBlock->pParent;` |
|  7764907 |   246 | `	if( ppBlock ){` |
|        - |   247 | `		/* Write a pointer to the popped block */` |
|      ! 0 |   248 | `		*ppBlock = pBlock;` |
|      ! 0 |   249 | `	}else{` |
|        - |   250 | `		/* Safely release the block */` |
|  7764907 |   251 | `		GenStateFreeBlock(&(*pBlock));` |
|        - |   252 | `	}` |
|  7764907 |   253 | `	return SXRET_OK;` |
|  3882456 |   254 | `}` |
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
|  2836366 |   265 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|        5 |   266 | `{` |
|        - |   267 | `	JumpFixup sJumpFix;` |
|        - |   268 | `	sxi32 rc;` |
|        - |   269 | `	/* Init the JumpFixup structure */` |
|  2836371 |   270 | `	sJumpFix.nJumpType = nJumpType;` |
|  2836371 |   271 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|        - |   272 | `	/* Insert in the jump fixup table */` |
|  2836371 |   273 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  2836371 |   274 | `	return rc;` |
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
|  5388704 |   287 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|        5 |   288 | `{` |
|        - |   289 | `	JumpFixup *aFix;` |
|        - |   290 | `	VmInstr *pInstr;` |
|        - |   291 | `	sxu32 nFixed;` |
|        - |   292 | `	sxu32 n;` |
|        - |   293 | `	/* Point to the jump fixup table */` |
|  5388709 |   294 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|        - |   295 | `	/* Fix the desired jumps */` |
| 11893107 |   296 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  6504403 |   297 | `		if( aFix[n].nJumpType < 0 ){` |
|        - |   298 | `			/* Already fixed */` |
|  2477253 |   299 | `			continue;` |
|        - |   300 | `		}` |
|  4027155 |   301 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|        - |   302 | `			/* Not of our interest */` |
|  1190791 |   303 | `			continue;` |
|        - |   304 | `		}` |
|        - |   305 | `		/* Point to the instruction to fix */` |
|  2836369 |   306 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  2836369 |   307 | `		if( pInstr ){` |
|  2836369 |   308 | `			pInstr->iP2 = nJumpDest;` |
|  2836369 |   309 | `			nFixed++;` |
|        - |   310 | `			/* Mark as fixed */` |
|  2836369 |   311 | `			aFix[n].nJumpType = -1;` |
|  1418182 |   312 | `		}` |
|  1418187 |   313 | `	}` |
|        - |   314 | `	/* Total number of fixed jumps */` |
|  5388709 |   315 | `	return nFixed;` |
|        5 |   316 | `}` |
|        - |   317 | `/*` |
|        - |   318 | ` * Fix a 'goto' now the jump destination is resolved.` |
|        - |   319 | ` * The goto statement can be used to jump to another section` |
|        - |   320 | ` * in the program.` |
|        - |   321 | ` * Refer to the routine responsible of compiling the goto` |
|        - |   322 | ` * statement for more information.` |
|        - |   323 | ` */` |
|  2056828 |   324 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|        5 |   325 | `{` |
|        - |   326 | `	JumpFixup *pJump,*aJumps;` |
|        - |   327 | `	Label *pLabel,*aLabel;` |
|        - |   328 | `	VmInstr *pInstr;` |
|        - |   329 | `	sxi32 rc;` |
|        - |   330 | `	sxu32 n;` |
|        - |   331 | `	/* Point to the goto table */` |
|  2056833 |   332 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|        - |   333 | `	/* Fix */` |
|  2056979 |   334 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|       96 |   347 | `		if( pLabel->pFunc != pJump->pFunc ){` |
|       11 |   348 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' is unreachable",&pJump->sLabel);` |
|       11 |   349 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |   350 | `				return SXERR_ABORT;` |
|        - |   351 | `			}` |
|        4 |   352 | `		}` |
|        - |   353 | `		/* Fix the jump now the destination is resolved */` |
|       96 |   354 | `		pInstr = PH7_VmGetInstr(pGen->pVm,pJump->nInstrIdx);` |
|       96 |   355 | `		if( pInstr ){` |
|       96 |   356 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|       46 |   357 | `		}` |
|       50 |   358 | `	}` |
|  2056831 |   359 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  2056963 |   360 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|      137 |   361 | `		if( aLabel[n].bRef == FALSE ){` |
|        - |   362 | `			/* Emit a warning */` |
|       40 |   363 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|       24 |   364 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|       12 |   365 | `		}` |
|       71 |   366 | `	}` |
|  2056831 |   367 | `	return SXRET_OK;` |
|  1028419 |   368 | `}` |
|        - |   369 | `/*` |
|        - |   370 | ` * Check if a given token value is installed in the literal table.` |
|        - |   371 | ` */` |
| 10341300 |   372 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|        5 |   373 | `{` |
|        - |   374 | `	SyHashEntry *pEntry;` |
| 10341305 |   375 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
| 10341305 |   376 | `	if( pEntry == 0 ){` |
|  2763437 |   377 | `		return SXERR_NOTFOUND;` |
|        - |   378 | `	}` |
|  7577873 |   379 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  7577873 |   380 | `	return SXRET_OK;` |
|  5170655 |   381 | `}` |
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
|  2763432 |   392 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|        5 |   393 | `{` |
|  2763437 |   394 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  2763437 |   395 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  1381716 |   396 | `	}` |
|  2763437 |   397 | `	return SXRET_OK;` |
|        5 |   398 | `}` |
|        - |   399 | `/*` |
|        - |   400 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|        - |   401 | ` * in the constant table.` |
|        - |   402 | ` */` |
|  1999172 |   403 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|        5 |   404 | `{` |
|        - |   405 | `	ph7_value *pObj;` |
|  1999177 |   406 | `	sxu32 nIdx = 0; /* cc warning */` |
|        - |   407 | `	/* Reserve a new constant */` |
|  1999177 |   408 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  1999177 |   409 | `	if( pObj == 0 ){` |
|      ! 0 |   410 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |   411 | `		return 0;` |
|        - |   412 | `	}` |
|  1999177 |   413 | `	*pIdx = nIdx;` |
|        - |   414 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|        - |   415 | `	 * the constant string iterals table [optimization purposes].` |
|        - |   416 | `	 */` |
|  1999177 |   417 | `	return pObj;` |
|   999591 |   418 | `}` |
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
|  4902710 |   433 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|        5 |   434 | `{` |
|        - |   435 | `	VmCallArgMap *pMap;` |
|  4902715 |   436 | `	if( !pGen->bStrictTypes ) return p3;` |
|       39 |   437 | `	if( p3 == 0 ){` |
|       35 |   438 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|       35 |   439 | `		if( pMap == 0 ) return 0;` |
|       35 |   440 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|       35 |   441 | `		p3 = (void *)pMap;` |
|       16 |   442 | `	}` |
|       39 |   443 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|       39 |   444 | `	return p3;` |
|  2451360 |   445 | `}` |
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
|  2000166 |   509 | `static int GenStateFindBadNumericSeparator(` |
|        - |   510 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|        5 |   511 | `{` |
|  2000171 |   512 | `	const char *z = pRaw->zString;` |
|  2000171 |   513 | `	sxu32 n = pRaw->nByte;` |
|  2000171 |   514 | `	int base = 10;` |
|        - |   515 | `	sxu32 i, start;` |
|  2000171 |   516 | `	if( n < 2 ) return 0;` |
|   434485 |   517 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|       80 |   518 | `		base = 16;` |
|   434446 |   519 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|      284 |   520 | `		base = 2;` |
|      141 |   521 | `	}` |
|  1459595 |   522 | `	for( i = 0; i < n; ++i ){` |
|  1025129 |   523 | `		if( z[i] != '_' ) continue;` |
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
|   434471 |   540 | `	return 0;` |
|  1000088 |   541 | `}` |
|        - |   542 | `/*` |
|        - |   543 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|        - |   544 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|        - |   545 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|        - |   546 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|        - |   547 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|        - |   548 | ` * so callers can bail from the current construct).` |
|        - |   549 | ` */` |
|  2000166 |   550 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|        5 |   551 | `{` |
|  2000171 |   552 | `	const char *zBad = 0;` |
|  2000171 |   553 | `	sxu32 nBad = 0;` |
|        - |   554 | `	SyString sBad;` |
|        - |   555 | `	sxi32 rc;` |
|  2000171 |   556 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  2000157 |   557 | `		return SXRET_OK;` |
|        - |   558 | `	}` |
|       18 |   559 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|       18 |   560 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|        - |   561 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|       18 |   562 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |   563 | `		return SXERR_ABORT;` |
|        - |   564 | `	}` |
|       18 |   565 | `	return SXERR_SYNTAX;` |
|  1000088 |   566 | `}` |
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
|  2000152 |   583 | `static sxi32 GenStateStripNumericSeparators(` |
|        - |   584 | `	SyMemBackend *pAlloc,` |
|        - |   585 | `	const SyString *pToken,` |
|        - |   586 | `	char *zScratch, sxu32 nScratch,` |
|        - |   587 | `	SyString *pOut, char **pzAlloc)` |
|        5 |   588 | `{` |
|        - |   589 | `	sxu32 i, j;` |
|  2000157 |   590 | `	int hasUnderscore = 0;` |
|        - |   591 | `	char *zBuf;` |
|  2000157 |   592 | `	*pzAlloc = 0;` |
|  4588887 |   593 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  2588987 |   594 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|  1294370 |   595 | `	}` |
|  2000157 |   596 | `	if( !hasUnderscore ){` |
|  1999905 |   597 | `		SyStringDupPtr(pOut, pToken);` |
|  1999905 |   598 | `		return SXRET_OK;` |
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
|  1000081 |   615 | `}` |
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
|        - |   649 | ` * residual; matching php exactly would need a port of those functions.` |
|        - |   650 | ` */` |
|  1999206 |   651 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|        5 |   652 | `{` |
|  1999211 |   653 | `	const char *z = pNum->zString;` |
|  1999211 |   654 | `	const char *zEnd = z + pNum->nByte;` |
|        - |   655 | `	const char *p, *q;` |
|        - |   656 | `	int n;` |
|  1999211 |   657 | `	*pbDecimal = FALSE;` |
|  1999211 |   658 | `	if( z >= zEnd ){` |
|      ! 0 |   659 | `		return FALSE;` |
|        - |   660 | `	}` |
|  1999211 |   661 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
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
|  1999135 |   676 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
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
|  1998855 |   691 | `	}else if( z[0] == '0' ){` |
|        - |   692 | `		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the` |
|        - |   693 | `		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1` |
|        - |   694 | `		 * "0o" marker ends the run and leaves it to the int path (as today). */` |
|   688287 |   695 | `		p = z;` |
|  1376571 |   696 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|   688515 |   697 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|   688287 |   698 | `		if( n <= 21 ){` |
|   688285 |   699 | `			return FALSE;` |
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
|  1310573 |   712 | `	p = z;` |
|  1310573 |   713 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
|  3204539 |   714 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|  1310573 |   715 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|       25 |   716 | `		*pbDecimal = TRUE;` |
|       25 |   717 | `		return TRUE;` |
|        - |   718 | `	}` |
|  1310549 |   719 | `	return FALSE;` |
|   999608 |   720 | `}` |
|  2000138 |   721 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |   722 | `{` |
|  2000143 |   723 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  2000143 |   724 | `	sxu32 nIdx = 0;` |
|        - |   725 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  2000143 |   726 | `	char *zAlloc = 0;` |
|        - |   727 | `	SyString sNum;` |
|        - |   728 | `	sxi32 rc;` |
|  1000069 |   729 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  2000143 |   730 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  2000143 |   731 | `	if( rc != SXRET_OK ){` |
|       14 |   732 | `		return rc;` |
|        - |   733 | `	}` |
|  3000197 |   734 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|  1000064 |   735 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  2000133 |   736 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   737 | `		return SXERR_ABORT;` |
|        - |   738 | `	}` |
|  2000133 |   739 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|        - |   740 | `		ph7_value *pObj;` |
|        - |   741 | `		sxi64 iValue;` |
|  1999211 |   742 | `		ph7_real rOverflow = 0;` |
|  1999211 |   743 | `		int bDecimalOverflow = 0;` |
|  1999211 |   744 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
|        - |   745 | `			/* Literal exceeds the signed 64-bit range: PHP represents it as a` |
|        - |   746 | `			 * float instead of wrapping/dropping digits. */` |
|       35 |   747 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       35 |   748 | `			if( pObj == 0 ){` |
|      ! 0 |   749 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |   750 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |   751 | `				return SXERR_ABORT;` |
|        - |   752 | `			}` |
|       35 |   753 | `			if( bDecimalOverflow ){` |
|        - |   754 | `				/* strtod on the decimal token yields php-exact rounding. */` |
|       25 |   755 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|       25 |   756 | `				PH7_MemObjToReal(pObj);` |
|       13 |   757 | `			}else{` |
|       11 |   758 | `				PH7_MemObjInitFromReal(pGen->pVm,pObj,rOverflow);` |
|        - |   759 | `			}` |
|       18 |   760 | `		}else{` |
|  1999177 |   761 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|  1999177 |   762 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  1999177 |   763 | `			if( pObj == 0 ){` |
|      ! 0 |   764 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |   765 | `				return SXERR_ABORT;` |
|        - |   766 | `			}` |
|  1999177 |   767 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|        - |   768 | `		}` |
|   999608 |   769 | `	}else{` |
|        - |   770 | `		/* Real number */` |
|        - |   771 | `		ph7_value *pObj;` |
|        - |   772 | `		/* Reserve a new constant */` |
|      925 |   773 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      925 |   774 | `		if( pObj == 0 ){` |
|      ! 0 |   775 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |   776 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |   777 | `			return SXERR_ABORT;` |
|        - |   778 | `		}` |
|      925 |   779 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|      925 |   780 | `		PH7_MemObjToReal(pObj);` |
|        - |   781 | `	}` |
|  2000133 |   782 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        - |   783 | `	/* Emit the load constant instruction */` |
|  2000133 |   784 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - |   785 | `	/* Node successfully compiled */` |
|  2000133 |   786 | `	return SXRET_OK;` |
|  1000074 |   787 | `}` |
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
|  3749592 |   799 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |   800 | `{` |
|  3749597 |   801 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|        - |   802 | `	const char *zIn,*zCur,*zEnd;` |
|        - |   803 | `	ph7_value *pObj;` |
|        - |   804 | `	sxu32 nIdx;` |
|  3749597 |   805 | `	nIdx = 0; /* Prevent compiler warning */` |
|        - |   806 | `	/* Delimit the string */` |
|  3749597 |   807 | `	zIn  = pStr->zString;` |
|  3749597 |   808 | `	zEnd = &zIn[pStr->nByte];` |
|  3749597 |   809 | `	if( zIn >= zEnd ){` |
|        - |   810 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|        - |   811 | `		 * rather than reserving a new object each time. */` |
|   151365 |   812 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|   151365 |   813 | `		return SXRET_OK;` |
|        - |   814 | `	}` |
|  3598237 |   815 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|        - |   816 | `		/* Already processed,emit the load constant instruction` |
|        - |   817 | `		 * and return.` |
|        - |   818 | `		 */` |
|  2214065 |   819 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  2214065 |   820 | `		return SXRET_OK;` |
|        - |   821 | `	}` |
|        - |   822 | `	/* Reserve a new constant */` |
|  1384177 |   823 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  1384177 |   824 | `	if( pObj == 0 ){` |
|      ! 0 |   825 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |   826 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |   827 | `		return SXERR_ABORT;` |
|        - |   828 | `	}` |
|  1384177 |   829 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|        - |   830 | `	/* Compile the node */` |
|  1426868 |   831 | `	for(;;){` |
|  2853741 |   832 | `		if( zIn >= zEnd ){` |
|        - |   833 | `			/* End of input */` |
|  1384177 |   834 | `			break;` |
|        - |   835 | `		}` |
|  1469569 |   836 | `		zCur = zIn;` |
| 26727181 |   837 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
| 25257617 |   838 | `			zIn++;` |
|        5 |   839 | `		}` |
|  1469569 |   840 | `		if( zIn > zCur ){` |
|        - |   841 | `			/* Append raw contents*/` |
|  1430783 |   842 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   715389 |   843 | `		}` |
|  1469569 |   844 | `		zIn++;` |
|  1469569 |   845 | `		if( zIn < zEnd ){` |
|   120303 |   846 | `			if( zIn[0] == '\\' ){` |
|        - |   847 | `				/* A literal backslash */` |
|    31043 |   848 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|   104784 |   849 | `			}else if( zIn[0] == '\'' ){` |
|        - |   850 | `				/* A single quote */` |
|       11 |   851 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|        6 |   852 | `			}else{` |
|        - |   853 | `				/* verbatim copy */` |
|    89255 |   854 | `				zIn--;` |
|    89255 |   855 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|    89255 |   856 | `				zIn++;` |
|        - |   857 | `			}` |
|    60149 |   858 | `		}` |
|        - |   859 | `		/* Advance the stream cursor */` |
|  1469569 |   860 | `		zIn++;` |
|        5 |   861 | `	}` |
|        - |   862 | `	/* Emit the load constant instruction */` |
|  1384177 |   863 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  1384177 |   864 | `	if( pStr->nByte < 1024 ){` |
|        - |   865 | `		/* Install in the literal table */` |
|  1384177 |   866 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   692086 |   867 | `	}` |
|        - |   868 | `	/* Node successfully compiled */` |
|  1384177 |   869 | `	return SXRET_OK;` |
|  1874801 |   870 | `}` |
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
|        4 |   890 | `{` |
|      118 |   891 | `	SyString *pIn = &pGen->pIn->sData;` |
|      118 |   892 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        - |   893 | `	const char *zPrefix;` |
|        - |   894 | `	const char *z, *zEnd;` |
|        - |   895 | `	char *zBuf, *zDst;` |
|      118 |   896 | `	if( nIndent == 0 ){` |
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
|       47 |   907 | `	zPrefix = pIn->zString + pIn->nByte;` |
|       47 |   908 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|      ! 0 |   909 | `		zPrefix += 2;` |
|      ! 0 |   910 | `	}else{` |
|       47 |   911 | `		zPrefix += 1;` |
|        - |   912 | `	}` |
|        - |   913 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|       47 |   914 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|       47 |   915 | `	if( zBuf == 0 ){` |
|      ! 0 |   916 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |   917 | `		return SXERR_ABORT;` |
|        - |   918 | `	}` |
|       47 |   919 | `	zDst = zBuf;` |
|       47 |   920 | `	z = pIn->zString;` |
|       47 |   921 | `	zEnd = z + pIn->nByte;` |
|      129 |   922 | `	while( z < zEnd ){` |
|       71 |   923 | `		const char *zLine = z;` |
|        - |   924 | `		sxu32 nLine;` |
|        - |   925 | `		int bEmpty;` |
|      799 |   926 | `		while( z < zEnd && z[0] != '\n' ){` |
|      731 |   927 | `			z++;` |
|        3 |   928 | `		}` |
|       71 |   929 | `		nLine = (sxu32)(z - zLine);` |
|       71 |   930 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|       71 |   931 | `		if( !bEmpty ){` |
|        - |   932 | `			sxu32 i;` |
|       67 |   933 | `			if( nLine < nIndent ){` |
|      ! 0 |   934 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |   935 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|      ! 0 |   936 | `					nIndent);` |
|      ! 0 |   937 | `				return SXERR_ABORT;` |
|        - |   938 | `			}` |
|      269 |   939 | `			for( i = 0; i < nIndent; i++ ){` |
|      213 |   940 | `				if( zLine[i] != zPrefix[i] ){` |
|       10 |   941 | `					unsigned char c = (unsigned char)zLine[i];` |
|       10 |   942 | `					if( c == ' ' \|\| c == '\t' ){` |
|        5 |   943 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |   944 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|        3 |   945 | `					}else{` |
|        7 |   946 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |   947 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|        2 |   948 | `							nIndent);` |
|        - |   949 | `					}` |
|       10 |   950 | `					return SXERR_ABORT;` |
|        - |   951 | `				}` |
|      103 |   952 | `			}` |
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
|       61 |   967 | `}` |
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
|     2514 |  1036 | `static sxi32 GenStateProcessStringExpression(` |
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
|     2519 |  1047 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        - |  1048 | `	/* Preallocate some slots */` |
|     2519 |  1049 | `	SySetAlloc(&sToken,0x08);` |
|        - |  1050 | `	/* Tokenize the text */` |
|     2519 |  1051 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken,0);` |
|        - |  1052 | `	/* Swap delimiter */` |
|     2519 |  1053 | `	pTmpIn  = pGen->pIn;` |
|     2519 |  1054 | `	pTmpEnd = pGen->pEnd;` |
|     2519 |  1055 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|     2519 |  1056 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|        - |  1057 | `	/* Compile the expression */` |
|     2519 |  1058 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  1059 | `	/* Restore token stream */` |
|     2519 |  1060 | `	pGen->pIn  = pTmpIn;` |
|     2519 |  1061 | `	pGen->pEnd = pTmpEnd;` |
|        - |  1062 | `	/* Release the token set */` |
|     2519 |  1063 | `	SySetRelease(&sToken);` |
|        - |  1064 | `	/* Compilation result */` |
|     2519 |  1065 | `	return rc;` |
|        5 |  1066 | `}` |
|        - |  1067 | `/*` |
|        - |  1068 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|        - |  1069 | ` */` |
|    47216 |  1070 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|        5 |  1071 | `{` |
|        - |  1072 | `	ph7_value *pConstObj;` |
|    47221 |  1073 | `	sxu32 nIdx = 0;` |
|        - |  1074 | `	/* Reserve a new constant */` |
|    47221 |  1075 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    47221 |  1076 | `	if( pConstObj == 0 ){` |
|      ! 0 |  1077 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  1078 | `		return 0;` |
|        - |  1079 | `	}` |
|    47221 |  1080 | `	(*pCount)++;` |
|    47221 |  1081 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|        - |  1082 | `	/* Emit the load constant instruction */` |
|    47221 |  1083 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    47221 |  1084 | `	return pConstObj;` |
|    23613 |  1085 | `}` |
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
|    45674 |  1148 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|        5 |  1149 | `{` |
|    45679 |  1150 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|        - |  1151 | `	const char *zIn,*zCur,*zEnd;` |
|    45679 |  1152 | `	ph7_value *pObj = 0;` |
|        - |  1153 | `	sxi32 iCons;` |
|        - |  1154 | `	sxi32 rc;` |
|        - |  1155 | `	/* Delimit the string */` |
|    45679 |  1156 | `	zIn  = pStr->zString;` |
|    45679 |  1157 | `	zEnd = &zIn[pStr->nByte];` |
|    45679 |  1158 | `	if( zIn >= zEnd ){` |
|        - |  1159 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|        - |  1160 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|        - |  1161 | `		 * literal table from growing when many "" literals appear in the source.` |
|        - |  1162 | `		 */` |
|      377 |  1163 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|      377 |  1164 | `		return SXRET_OK;` |
|        - |  1165 | `	}` |
|    45307 |  1166 | `	zCur = 0;` |
|        - |  1167 | `	/* Compile the node */` |
|    45307 |  1168 | `	iCons = 0;` |
|    23908 |  1169 | `	for(;;){` |
|    79979 |  1170 | `		zCur = zIn;` |
|   233801 |  1171 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|   156341 |  1172 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|       72 |  1173 | `				break;` |
|   156207 |  1174 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|     2384 |  1175 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     1193 |  1176 | `					break;` |
|        - |  1177 | `			}` |
|   153827 |  1178 | `			zIn++;` |
|        5 |  1179 | `		}` |
|    79979 |  1180 | `		if( zIn > zCur ){` |
|    20861 |  1181 | `			if( pObj == 0 ){` |
|    20329 |  1182 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    20329 |  1183 | `				if( pObj == 0 ){` |
|      ! 0 |  1184 | `					return SXERR_ABORT;` |
|        - |  1185 | `				}` |
|    10162 |  1186 | `			}` |
|    20861 |  1187 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    10428 |  1188 | `		}` |
|    79979 |  1189 | `		if( zIn >= zEnd ){` |
|    45305 |  1190 | `			break;` |
|        - |  1191 | `		}` |
|    34679 |  1192 | `		if( zIn[0] == '\\' ){` |
|    32165 |  1193 | `			const char *zPtr = 0;` |
|        - |  1194 | `			sxu32 n;` |
|    32165 |  1195 | `			zIn++;` |
|    32165 |  1196 | `			if( pObj == 0 ){` |
|    26897 |  1197 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    26897 |  1198 | `				if( pObj == 0 ){` |
|      ! 0 |  1199 | `					return SXERR_ABORT;` |
|        - |  1200 | `				}` |
|    13446 |  1201 | `			}` |
|    32165 |  1202 | `			if( zIn >= zEnd ){` |
|        - |  1203 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|        3 |  1204 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|        3 |  1205 | `				break;` |
|        - |  1206 | `			}` |
|    32163 |  1207 | `			n = sizeof(char); /* size of conversion */` |
|    32163 |  1208 | `			switch( zIn[0] ){` |
|       11 |  1209 | `			case '$':` |
|        - |  1210 | `				/* Dollar sign */` |
|       25 |  1211 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|       25 |  1212 | `				break;` |
|       52 |  1213 | `			case '\\':` |
|        - |  1214 | `				/* A literal backslash */` |
|      109 |  1215 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      109 |  1216 | `				break;` |
|        1 |  1217 | `			case 'e':` |
|        - |  1218 | `				/* Escape (ESC) ASCII code 27 */` |
|        3 |  1219 | `				PH7_MemObjStringAppend(pObj,"\x1b",sizeof(char));` |
|        3 |  1220 | `				break;` |
|        4 |  1221 | `			case 'f':` |
|        - |  1222 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|        9 |  1223 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|        9 |  1224 | `				break;` |
|    13590 |  1225 | `			case 'n':` |
|        - |  1226 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    27185 |  1227 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    27185 |  1228 | `				break;` |
|       21 |  1229 | `			case 'r':` |
|        - |  1230 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|       47 |  1231 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|       47 |  1232 | `				break;` |
|     1969 |  1233 | `			case 't':` |
|        - |  1234 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|     3943 |  1235 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|     3943 |  1236 | `				break;` |
|        3 |  1237 | `			case 'v':` |
|        - |  1238 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|        7 |  1239 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|        7 |  1240 | `				break;` |
|      107 |  1241 | `			case '"':` |
|      219 |  1242 | `				if( bHeredoc ){` |
|        - |  1243 | `					/* No active quote char in a heredoc: php keeps \" verbatim */` |
|        5 |  1244 | `					PH7_MemObjStringAppend(pObj,"\\\"",sizeof(char)*2);` |
|        3 |  1245 | `				}else{` |
|        - |  1246 | `					/* Double quote */` |
|      215 |  1247 | `					PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|        - |  1248 | `				}` |
|      219 |  1249 | `				break;` |
|       25 |  1250 | `			case '0': case '1': case '2': case '3':` |
|        - |  1251 | `			case '4': case '5': case '6': case '7': {` |
|        - |  1252 | `				/* \[0-7]{1,3}: a character in octal notation. A value above \377` |
|        - |  1253 | `				 * warns and wraps to the low byte, matching php 8. */` |
|       52 |  1254 | `				int c = 0;` |
|        - |  1255 | `				char cOut;` |
|      148 |  1256 | `				for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|      126 |  1257 | `					if( zPtr >= zEnd \|\| zPtr[0] < '0' \|\| zPtr[0] > '7' ){` |
|       15 |  1258 | `						break;` |
|        - |  1259 | `					}` |
|       98 |  1260 | `					c = c * 8 + (zPtr[0] - '0');` |
|       50 |  1261 | `				}` |
|       52 |  1262 | `				if( c > 0xFF ){` |
|        - |  1263 | `					SyString sSeq;` |
|        3 |  1264 | `					SyStringInitFromBuf(&sSeq,zIn,(sxu32)(zPtr-zIn));` |
|        3 |  1265 | `					PH7_GenCompileError(&(*pGen),E_WARNING,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|        - |  1266 | `						"Octal escape sequence overflow \\%z is greater than \\377",&sSeq);` |
|        3 |  1267 | `					c &= 0xFF;` |
|        1 |  1268 | `				}` |
|       52 |  1269 | `				cOut = (char)c; /* value byte, independent of host endianness */` |
|       52 |  1270 | `				PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|       52 |  1271 | `				n = (sxu32)(zPtr-zIn);` |
|       52 |  1272 | `				break;` |
|        - |  1273 | `			}` |
|      271 |  1274 | `			case 'x':` |
|      812 |  1275 | `				if( &zIn[1] < zEnd && SyisHex((unsigned char)zIn[1]) ){` |
|        - |  1276 | `					/* \x[0-9A-Fa-f]{1,2}: a character in hexadecimal notation */` |
|      539 |  1277 | `					int c = SyHexToint(zIn[1]);` |
|        - |  1278 | `					char cOut;` |
|      539 |  1279 | `					n += sizeof(char);` |
|      539 |  1280 | `					if( &zIn[2] < zEnd && SyisHex((unsigned char)zIn[2]) ){` |
|      535 |  1281 | `						c = (c << 4) + SyHexToint(zIn[2]);` |
|      535 |  1282 | `						n += sizeof(char);` |
|      267 |  1283 | `					}` |
|      539 |  1284 | `					cOut = (char)c; /* value byte, independent of host endianness */` |
|      539 |  1285 | `					PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|      270 |  1286 | `				}else{` |
|        - |  1287 | `					/* Not an escape: keep the backslash, as php does */` |
|        5 |  1288 | `					PH7_MemObjStringAppend(pObj,"\\x",sizeof(char)*2);` |
|        - |  1289 | `				}` |
|      543 |  1290 | `				break;` |
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
|       16 |  1343 | `			default:` |
|        - |  1344 | `				/* Unrecognized escape: keep the backslash, as php does.` |
|        - |  1345 | `				 * zIn[-1] is the backslash itself, so both bytes are contiguous` |
|        - |  1346 | `				 * in the source buffer — one batched append. */` |
|       33 |  1347 | `				PH7_MemObjStringAppend(pObj,&zIn[-1],sizeof(char)*2);` |
|       32 |  1348 | `				break;` |
|        - |  1349 | `			}` |
|        - |  1350 | `			/* Advance the stream cursor */` |
|    32163 |  1351 | `			zIn += n;` |
|    32163 |  1352 | `			continue;` |
|        - |  1353 | `		}` |
|     2519 |  1354 | `		if( zIn[0] == '{' ){` |
|        - |  1355 | `			/* Curly syntax */` |
|        - |  1356 | `			const char *zExpr;` |
|      141 |  1357 | `			sxi32 iNest = 1;` |
|      141 |  1358 | `			zIn++;` |
|      141 |  1359 | `			zExpr = zIn;` |
|        - |  1360 | `			/* Synchronize with the next closing curly braces */` |
|     1419 |  1361 | `			while( zIn < zEnd ){` |
|     1419 |  1362 | `				if( zIn[0] == '{' ){` |
|        - |  1363 | `					/* Increment nesting level */` |
|        9 |  1364 | `					iNest++;` |
|     1415 |  1365 | `				}else if(zIn[0] == '}' ){` |
|        - |  1366 | `					/* Decrement nesting level */` |
|      149 |  1367 | `					iNest--;` |
|      149 |  1368 | `					if( iNest <= 0 ){` |
|      141 |  1369 | `						break;` |
|        - |  1370 | `					}` |
|        4 |  1371 | `				}` |
|     1281 |  1372 | `				zIn++;` |
|        3 |  1373 | `			}` |
|        - |  1374 | `			/* Process the expression */` |
|      141 |  1375 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|      141 |  1376 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  1377 | `				return SXERR_ABORT;` |
|        - |  1378 | `			}` |
|      141 |  1379 | `			if( rc != SXERR_EMPTY ){` |
|      141 |  1380 | `				++iCons;` |
|       69 |  1381 | `			}` |
|      141 |  1382 | `			if( zIn < zEnd ){` |
|        - |  1383 | `				/* Jump the trailing curly */` |
|      141 |  1384 | `				zIn++;` |
|       69 |  1385 | `			}` |
|       72 |  1386 | `		}else{` |
|        - |  1387 | `			/* Simple syntax */` |
|     2381 |  1388 | `			const char *zExpr = zIn;` |
|        - |  1389 | `			/* Assemble variable name */` |
|     1213 |  1390 | `			for(;;){` |
|        - |  1391 | `				/* Jump leading dollars */` |
|     4807 |  1392 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|     2381 |  1393 | `					zIn++;` |
|        5 |  1394 | `				}` |
|     1213 |  1395 | `				for(;;){` |
|    12692 |  1396 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|     9053 |  1397 | `						zIn++;` |
|        5 |  1398 | `					}` |
|     2431 |  1399 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|        - |  1400 | `						/* UTF-8 stream */` |
|      ! 0 |  1401 | `						zIn++;` |
|      ! 0 |  1402 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  1403 | `							zIn++;` |
|      ! 0 |  1404 | `						}` |
|      ! 0 |  1405 | `						continue;` |
|        - |  1406 | `					}` |
|     2431 |  1407 | `					break;` |
|      ! 0 |  1408 | `				}` |
|     2431 |  1409 | `				if( zIn >= zEnd ){` |
|      257 |  1410 | `					break;` |
|        - |  1411 | `				}` |
|     2179 |  1412 | `				if( zIn[0] == '[' ){` |
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
|     2169 |  1430 | `				}else if(zIn[0] == '{' ){` |
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
|     2165 |  1448 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|        - |  1449 | `					/* Member access operator '->' */` |
|       53 |  1450 | `					zIn += 2;` |
|     2140 |  1451 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|        - |  1452 | `					/* Static member access operator '::' */` |
|      ! 0 |  1453 | `					zIn += 2;` |
|      ! 0 |  1454 | `				}else{` |
|     1060 |  1455 | `					break;` |
|        - |  1456 | `				}` |
|        3 |  1457 | `			}` |
|        - |  1458 | `			/* Process the expression */` |
|     2381 |  1459 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     2381 |  1460 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  1461 | `				return SXERR_ABORT;` |
|        - |  1462 | `			}` |
|     2381 |  1463 | `			if( rc != SXERR_EMPTY ){` |
|     2379 |  1464 | `				++iCons;` |
|     1187 |  1465 | `			}` |
|        - |  1466 | `		}` |
|        - |  1467 | `		/* Invalidate the previously used constant */` |
|     2519 |  1468 | `		pObj = 0;` |
|        5 |  1469 | `	}/*for(;;)*/` |
|    45307 |  1470 | `	if( iCons > 1 ){` |
|        - |  1471 | `		/* Concatenate all compiled constants */` |
|     1829 |  1472 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|      912 |  1473 | `	}` |
|        - |  1474 | `	/* Node successfully compiled */` |
|    45307 |  1475 | `	return SXRET_OK;` |
|    22842 |  1476 | `}` |
|        - |  1477 | `/*` |
|        - |  1478 | ` * Compile a double quoted string.` |
|        - |  1479 | ` *  See the block-comment above for more information.` |
|        - |  1480 | ` */` |
|    45612 |  1481 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  1482 | `{` |
|        - |  1483 | `	sxi32 rc;` |
|    45617 |  1484 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|    22806 |  1485 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  1486 | `	/* Compilation result */` |
|    45617 |  1487 | `	return rc;` |
|        5 |  1488 | `}` |
|        - |  1489 | `/*` |
|        - |  1490 | ` * Compile a Heredoc string.` |
|        - |  1491 | ` *  See the block-comment above for more information.` |
|        - |  1492 | ` */` |
|       66 |  1493 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        4 |  1494 | `{` |
|        - |  1495 | `	SyString sOrig, sStripped;` |
|        - |  1496 | `	sxi32 rc;` |
|       70 |  1497 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|       70 |  1498 | `	if( rc != SXRET_OK ){` |
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
|       37 |  1511 | `}` |
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
|   591074 |  1531 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   591079 |  1542 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|        - |  1543 | `	/* Compile the expression*/` |
|   591079 |  1544 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|        - |  1545 | `	/* Restore token stream */` |
|   591079 |  1546 | `	RE_SWAP_DELIMITER(pGen);` |
|   591079 |  1547 | `	return rc;` |
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
|       16 |  1563 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|        - |  1564 | `			/* Unexpected expression */` |
|       13 |  1565 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - |  1566 | `				"array(): Expecting a variable/array member/function call after reference operator '&'");` |
|       13 |  1567 | `			if( rc != SXERR_ABORT ){` |
|       13 |  1568 | `				rc = SXERR_INVALID;` |
|        5 |  1569 | `			}` |
|        9 |  1570 | `		}` |
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
|   597588 |  1588 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|        5 |  1589 | `{` |
|   597593 |  1590 | `	SyToken *pCur = pStart;` |
|   597593 |  1591 | `	sxi32 iNest = 0;` |
|  1779791 |  1592 | `	while( pCur < pEnd ){` |
|  1416985 |  1593 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|   234783 |  1594 | `			return pCur;` |
|        - |  1595 | `		}` |
|        - |  1596 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|        - |  1597 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|        - |  1598 | `		 * not an entry separator. Skip past the signature.` |
|        - |  1599 | `		 */` |
|  1182207 |  1600 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|    19487 |  1601 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    19487 |  1602 | `			SyToken *pFn = pCur;` |
|    19482 |  1603 | `			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd` |
|      ! 0 |  1604 | `				&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|        5 |  1605 | `				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|      ! 0 |  1606 | `				pFn = &pCur[1];` |
|      ! 0 |  1607 | `				nKw = PH7_TKWRD_FN;` |
|      ! 0 |  1608 | `			}` |
|    19487 |  1609 | `			if( nKw == PH7_TKWRD_FN ){` |
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
|    19483 |  1640 | `			if( nKw == PH7_TKWRD_MATCH ){` |
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
|     9738 |  1660 | `		}` |
|  1182201 |  1661 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|    50839 |  1662 | `			iNest++;` |
|  1156784 |  1663 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|        - |  1664 | `			/* Don't worry about mismatched brackets here, the expression` |
|        - |  1665 | `			 * parser will shortly detect any syntax error. */` |
|    50839 |  1666 | `			iNest--;` |
|    25417 |  1667 | `		}` |
|  1182201 |  1668 | `		pCur++;` |
|        5 |  1669 | `	}` |
|   362811 |  1670 | `	return pEnd;` |
|   298799 |  1671 | `}` |
|        - |  1672 | `/*` |
|        - |  1673 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|        - |  1674 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|        - |  1675 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|        - |  1676 | ` */` |
|   329274 |  1677 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|        5 |  1678 | `{` |
|        - |  1679 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|        - |  1680 | `	SyToken *pKey,*pCur;` |
|   329279 |  1681 | `	sxi32 iEmitRef = 0;` |
|   329279 |  1682 | `	sxi32 iSpread = 0;` |
|   329279 |  1683 | `	sxi32 nPair = 0;` |
|        - |  1684 | `	sxi32 rc;` |
|   329279 |  1685 | `	xValidator = 0;` |
|   375755 |  1686 | `	for(;;){` |
|        - |  1687 | `		/* Jump leading commas */` |
|  1066013 |  1688 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|   314503 |  1689 | `			pGen->pIn++;` |
|        5 |  1690 | `		}` |
|   751515 |  1691 | `		pCur = pGen->pIn;` |
|   751515 |  1692 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|        - |  1693 | `			/* No more entry to process */` |
|   329263 |  1694 | `			break;` |
|        - |  1695 | `		}` |
|   422257 |  1696 | `		if( pCur >= pGen->pIn ){` |
|      ! 0 |  1697 | `			continue;` |
|        - |  1698 | `		}` |
|        - |  1699 | `		/* Compile the key if available */` |
|   422257 |  1700 | `		pKey = pCur;` |
|   422257 |  1701 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   422257 |  1702 | `		rc = SXERR_EMPTY;` |
|   422257 |  1703 | `		if( pCur < pGen->pIn ){` |
|   168573 |  1704 | `			if( &pCur[1] >= pGen->pIn ){` |
|        - |  1705 | `				/* Missing value */` |
|       13 |  1706 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|       13 |  1707 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  1708 | `					return SXERR_ABORT;` |
|        - |  1709 | `				}` |
|       13 |  1710 | `				return SXRET_OK;` |
|        - |  1711 | `			}` |
|        - |  1712 | `			/* Compile the expression holding the key */` |
|   168563 |  1713 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|        - |  1714 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|   168563 |  1715 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  1716 | `				return SXERR_ABORT;` |
|        - |  1717 | `			}` |
|   168563 |  1718 | `			pCur++; /* Jump the '=>' operator */` |
|   337968 |  1719 | `		}else if( pKey == pCur ){` |
|        - |  1720 | `			/* Key is omitted,emit a warning */` |
|      ! 0 |  1721 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|      ! 0 |  1722 | `			pCur++; /* Jump the '=>' operator */` |
|      ! 0 |  1723 | `		}else{` |
|        - |  1724 | `			/* Reset back the cursor and point to the entry value */` |
|   253689 |  1725 | `			pCur = pKey;` |
|        - |  1726 | `		}` |
|   422247 |  1727 | `		if( rc == SXERR_EMPTY ){` |
|        - |  1728 | `			/* No available key,load NULL */` |
|   253691 |  1729 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|   126843 |  1730 | `		}` |
|   422247 |  1731 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|   422245 |  1750 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   422245 |  1751 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
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
|   422241 |  1764 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   422241 |  1765 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  1766 | `			return SXERR_ABORT;` |
|        - |  1767 | `		}` |
|   422241 |  1768 | `		if( iSpread ){` |
|        - |  1769 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|       69 |  1770 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   422208 |  1771 | `		}else if( iEmitRef ){` |
|        - |  1772 | `			/* Emit the load reference instruction */` |
|       41 |  1773 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|       18 |  1774 | `		}` |
|   422241 |  1775 | `		xValidator = 0;` |
|   422241 |  1776 | `		iEmitRef = 0;` |
|   422241 |  1777 | `		iSpread = 0;` |
|   422241 |  1778 | `		nPair++;` |
|        5 |  1779 | `	}` |
|        - |  1780 | `	/* Emit the load map instruction */` |
|   329263 |  1781 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|        - |  1782 | `	/* Node successfully compiled */` |
|   329263 |  1783 | `	return SXRET_OK;` |
|   164642 |  1784 | `}` |
|        - |  1785 | `/*` |
|        - |  1786 | ` * Compile the 'array' language construct.` |
|        - |  1787 | ` *	 According to the PHP language reference manual` |
|        - |  1788 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|        - |  1789 | ` *   values to keys. This type is optimized for several different uses; it can` |
|        - |  1790 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|        - |  1791 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|        - |  1792 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|        - |  1793 | ` */` |
|   288680 |  1794 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  1795 | `{` |
|        - |  1796 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   288685 |  1797 | `	pGen->pIn += 2;` |
|   288685 |  1798 | `	pGen->pEnd--;` |
|   144340 |  1799 | `	SXUNUSED(iCompileFlag);` |
|   288685 |  1800 | `	return GenStateCompileArrayBody(pGen);` |
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
|    40594 |  1899 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  1900 | `{` |
|        - |  1901 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|    40599 |  1902 | `	pGen->pIn++;` |
|    40599 |  1903 | `	pGen->pEnd--;` |
|    20297 |  1904 | `	SXUNUSED(iCompileFlag);` |
|    40599 |  1905 | `	return GenStateCompileArrayBody(pGen);` |
|        5 |  1906 | `}` |
|        - |  1907 | `/*` |
|        - |  1908 | ` * Expression tree validator callback for the 'list' language construct.` |
|        - |  1909 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|        - |  1910 | ` * an invalid expression tree and this function will generate the appropriate` |
|        - |  1911 | ` * error message.` |
|        - |  1912 | ` * See the routine responible of compiling the list language construct` |
|        - |  1913 | ` * for more inforation.` |
|        - |  1914 | ` */` |
|      206 |  1915 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 |  1916 | `{` |
|      211 |  1917 | `	sxi32 rc = SXRET_OK;` |
|      211 |  1918 | `	if( pRoot->pOp ){` |
|        4 |  1919 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|        2 |  1920 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|        - |  1921 | `				/* Unexpected expression */` |
|      ! 0 |  1922 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - |  1923 | `					"list(): Expecting a variable not an expression");` |
|      ! 0 |  1924 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 |  1925 | `					rc = SXERR_INVALID;` |
|      ! 0 |  1926 | `				}` |
|        1 |  1927 | `		}` |
|      209 |  1928 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|        - |  1929 | `		/* Unexpected expression */` |
|        6 |  1930 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - |  1931 | `			"list(): Expecting a variable not an expression");` |
|        6 |  1932 | `		if( rc != SXERR_ABORT ){` |
|        6 |  1933 | `			rc = SXERR_INVALID;` |
|        2 |  1934 | `		}` |
|        2 |  1935 | `	}` |
|      211 |  1936 | `	return rc;` |
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
|       22 |  1970 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|        1 |  1971 | `{` |
|        - |  1972 | `	SyToken *pNext;` |
|        - |  1973 | `	sxi32 rc;` |
|       53 |  1974 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|        - |  1975 | `		SyToken *pArrow,*pTarget;` |
|        - |  1976 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|       31 |  1977 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|       31 |  1978 | `		pTarget = &pArrow[1];` |
|       31 |  1979 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|        - |  1980 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|        - |  1981 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|      ! 0 |  1982 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  1983 | `				"Cannot use empty array entries in keyed array assignment");` |
|      ! 0 |  1984 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - |  1985 | `		}` |
|        - |  1986 | `		/* DUP the source array (it is on the stack top) */` |
|       31 |  1987 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|        - |  1988 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|       31 |  1989 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|       31 |  1990 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  1991 | `			return SXERR_ABORT;` |
|        - |  1992 | `		}` |
|        - |  1993 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|        - |  1994 | `		 * iP2=7 is the keyed-destructuring read context: an array source reads like` |
|        - |  1995 | ``		 * iP2=0 (missing key loads NULL silently, matching a normal `$arr[$k]` read;`` |
|        - |  1996 | `		 * PHP also emits an "Undefined array key" warning here, PHL omits it — §3.7),` |
|        - |  1997 | `		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"` |
|        - |  1998 | `		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */` |
|       31 |  1999 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);` |
|       31 |  2000 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|       28 |  2001 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|       15 |  2002 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
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
|       27 |  2025 | `			sxi32 iVmOp = PH7_OP_STORE;` |
|       27 |  2026 | `			sxi32 iP1 = 0, iP2 = 0;` |
|       27 |  2027 | `			void *p3 = 0;` |
|       27 |  2028 | `			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,` |
|        - |  2029 | `				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|       27 |  2030 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  2031 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - |  2032 | `			}` |
|       27 |  2033 | `			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|       27 |  2034 | `				if( pInstr->iOp == PH7_OP_MEMBER ){` |
|        3 |  2035 | `					iP2 = 1; /* member store: keep MEMBER, store value below it */` |
|       26 |  2036 | `				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        3 |  2037 | `					iVmOp = PH7_OP_STORE_IDX;` |
|        3 |  2038 | `					iP1 = pInstr->iP1;` |
|        3 |  2039 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        2 |  2040 | `				}else{` |
|       23 |  2041 | `					p3 = pInstr->p3; /* named store: $v = value */` |
|       23 |  2042 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        - |  2043 | `				}` |
|       13 |  2044 | `			}` |
|       27 |  2045 | `			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|        - |  2046 | `			/* STORE leaves the assigned value on the stack top; drop it so the` |
|        - |  2047 | `			 * source array is back on top for the next entry. */` |
|       27 |  2048 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        - |  2049 | `		}` |
|       31 |  2050 | `		pGen->pIn = &pNext[1];` |
|        1 |  2051 | `	}` |
|       23 |  2052 | `	return SXRET_OK;` |
|       12 |  2053 | `}` |
|        - |  2054 | `/*` |
|        - |  2055 | ` * Shared body for list() and short list [...] compilation.` |
|        - |  2056 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|        - |  2057 | ` * the opening delimiter and before the closing delimiter.` |
|        - |  2058 | ` */` |
|      120 |  2059 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|        5 |  2060 | `{` |
|        - |  2061 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|        - |  2062 | `	SyToken *pNext;` |
|        - |  2063 | `	SyToken *pClassifyIn;` |
|      125 |  2064 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|        - |  2065 | `	sxi32 nExpr;` |
|        - |  2066 | `	sxi32 rc;` |
|        - |  2067 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|        - |  2068 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|        - |  2069 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|        - |  2070 | `	 * list. */` |
|      125 |  2071 | `	pClassifyIn = pGen->pIn;` |
|      361 |  2072 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|      241 |  2073 | `		if( pGen->pIn >= pNext ){` |
|       13 |  2074 | `			nEmpty++;` |
|      235 |  2075 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|       31 |  2076 | `			nKeyed++;` |
|       16 |  2077 | `		}else{` |
|      199 |  2078 | `			nPositional++;` |
|        - |  2079 | `		}` |
|      241 |  2080 | `		pGen->pIn = &pNext[1];` |
|        5 |  2081 | `	}` |
|      125 |  2082 | `	pGen->pIn = pClassifyIn;` |
|      125 |  2083 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|      ! 0 |  2084 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  2085 | `			"Cannot use empty array entries in keyed array assignment");` |
|      ! 0 |  2086 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - |  2087 | `	}` |
|      125 |  2088 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|      ! 0 |  2089 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  2090 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|      ! 0 |  2091 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - |  2092 | `	}` |
|      125 |  2093 | `	if( nKeyed > 0 ){` |
|       23 |  2094 | `		return GenStateCompileKeyedListBody(pGen);` |
|        - |  2095 | `	}` |
|      103 |  2096 | `	nExpr = 0;` |
|      103 |  2097 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|      309 |  2098 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|      211 |  2099 | `		if( pGen->pIn < pNext ){` |
|        - |  2100 | `			/* Check for nested list() */` |
|      199 |  2101 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
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
|      198 |  2118 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
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
|      185 |  2134 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|      185 |  2135 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  2136 | `					SySetRelease(&sNested);` |
|      ! 0 |  2137 | `					return SXRET_OK;` |
|        - |  2138 | `				}` |
|        - |  2139 | `			}` |
|      102 |  2140 | `		}else{` |
|        - |  2141 | `			/* Empty entry,load NULL */` |
|       13 |  2142 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|        - |  2143 | `		}` |
|      211 |  2144 | `		nExpr++;` |
|        - |  2145 | `		/* Advance the stream cursor */` |
|      211 |  2146 | `		pGen->pIn = &pNext[1];` |
|        5 |  2147 | `	}` |
|        - |  2148 | `	/* Emit the LOAD_LIST instruction */` |
|      103 |  2149 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|        - |  2150 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|        - |  2151 | `	 * For each nested entry, emit code to extract the sub-array` |
|        - |  2152 | `	 * at the corresponding index and recursively destructure it.` |
|        - |  2153 | `	 */` |
|      103 |  2154 | `	if( SySetUsed(&sNested) > 0 ){` |
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
|      103 |  2196 | `	SySetRelease(&sNested);` |
|        - |  2197 | `	/* Node successfully compiled */` |
|      103 |  2198 | `	return SXRET_OK;` |
|       65 |  2199 | `}` |
|       38 |  2200 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  2201 | `{` |
|        - |  2202 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|       43 |  2203 | `	pGen->pIn += 2;` |
|       43 |  2204 | `	pGen->pEnd--;` |
|       19 |  2205 | `	SXUNUSED(iCompileFlag);` |
|       43 |  2206 | `	return GenStateCompileListBody(pGen);` |
|        5 |  2207 | `}` |
|       82 |  2208 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        3 |  2209 | `{` |
|        - |  2210 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|       85 |  2211 | `	pGen->pIn++;` |
|       85 |  2212 | `	pGen->pEnd--;` |
|       41 |  2213 | `	SXUNUSED(iCompileFlag);` |
|       85 |  2214 | `	return GenStateCompileListBody(pGen);` |
|        3 |  2215 | `}` |
|        - |  2216 | `/* Forward declarations */` |
|        - |  2217 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|        - |  2218 | `static int GenStateIsReservedConstant(SyString *pName);` |
|        - |  2219 | `static int GenStateIsReadonly(SyToken *pTok);` |
|        - |  2220 | `static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok);` |
|        - |  2221 | `static sxi32 GenStateSetVisFlag(sxi32 nKw);` |
|        - |  2222 | `static sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr);` |
|        - |  2223 | `static sxi32 GenStateValidateMemberType(ph7_gen_state *pGen,ph7_class *pClass,const SyString *pMemberName,` |
|        - |  2224 | `	sxu32 nType,const SyString *pTypeClass,const SyString *pTypeText,SySet *pUnionAlts,const char *zErrFmt,sxu32 nLine);` |
|        - |  2225 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|        - |  2226 | `/*` |
|        - |  2227 | ` * Compile an annoynmous function or a closure.` |
|        - |  2228 | ` * According to the PHP language reference` |
|        - |  2229 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|        - |  2230 | ` *  which have no specified name. They are most useful as the value of callback` |
|        - |  2231 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|        - |  2232 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|        - |  2233 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|        - |  2234 | ` *  Example Anonymous function variable assignment example` |
|        - |  2235 | ` * <?php` |
|        - |  2236 | ` * $greet = function($name)` |
|        - |  2237 | ` * {` |
|        - |  2238 | ` *    printf("Hello %s\r\n", $name);` |
|        - |  2239 | ` * };` |
|        - |  2240 | ` * $greet('World');` |
|        - |  2241 | ` * $greet('PHP');` |
|        - |  2242 | ` * ?>` |
|        - |  2243 | ` * Note that the implementation of annoynmous function and closure under` |
|        - |  2244 | ` * PH7 is completely different from the one used by the zend engine.` |
|        - |  2245 | ` */` |
|      466 |  2246 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  2247 | `{` |
|      471 |  2248 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|        - |  2249 | `	char zName[512];         /* Unique lambda name */` |
|        - |  2250 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|        - |  2251 | `							  * one thread is allowed to compile the script.` |
|        - |  2252 | `						      */` |
|        - |  2253 | `	SyString sName;` |
|      471 |  2254 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `$f = #[A] function…` trivia`` |
|        - |  2255 | `	                              * is keyed to this ['static'] 'function' token */` |
|        - |  2256 | `	sxu32 nKwLine;` |
|      471 |  2257 | `	sxi32 iFlags = 0;` |
|        - |  2258 | `	sxu32 nLen;` |
|        - |  2259 | `	sxi32 rc;` |
|      233 |  2260 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  2261 |  |
|      471 |  2262 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|      466 |  2263 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      471 |  2264 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - |  2265 | `		/* Static closure: no $this auto-capture, bind refused */` |
|        9 |  2266 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|        9 |  2267 | `		pGen->pIn++; /* Jump the 'static' keyword */` |
|        4 |  2268 | `	}` |
|      471 |  2269 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|      471 |  2270 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      ! 0 |  2271 | `		pGen->pIn++;` |
|      ! 0 |  2272 | `	}` |
|        - |  2273 | `	/* Generate a unique name */` |
|      471 |  2274 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|        - |  2275 | `	/* Make sure the generated name is unique */` |
|      471 |  2276 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 |  2277 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      ! 0 |  2278 | `	}` |
|      471 |  2279 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|        - |  2280 | `	/* Compile the lambda body */` |
|      471 |  2281 | `	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,&pAnnonFunc);` |
|      471 |  2282 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2283 | `		return SXERR_ABORT;` |
|        - |  2284 | `	}` |
|      471 |  2285 | `	if( pAnnonFunc ){` |
|      471 |  2286 | `		pAnnonFunc->nLine = nKwLine;` |
|        - |  2287 | ``		/* Expression-position attributes (`$f = #[A] function () {}`): the trivia`` |
|        - |  2288 | `		 * sidecar keys them to the closure's first keyword token. */` |
|      471 |  2289 | `		if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnnonFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  2290 | `			return SXERR_ABORT;` |
|        - |  2291 | `		}` |
|      233 |  2292 | `	}` |
|        - |  2293 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|        - |  2294 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|        - |  2295 | `	 * the handler wraps either in a Closure instance. */` |
|      471 |  2296 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|        - |  2297 | `	/* Node successfully compiled */` |
|      471 |  2298 | `	return SXRET_OK;` |
|      238 |  2299 | `}` |
|        - |  2300 | `/*` |
|        - |  2301 | ` * Add a free variable to the arrow function's closure environment, unless` |
|        - |  2302 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|        - |  2303 | ` * enclosing arrow level, or has already been captured.` |
|        - |  2304 | ` */` |
|      204 |  2305 | `static sxi32 GenStateArrowAddCapture(` |
|        - |  2306 | `	ph7_gen_state *pGen,` |
|        - |  2307 | `	ph7_vm_func *pFunc,` |
|        - |  2308 | `	const char *zName,` |
|        - |  2309 | `	sxu32 nByte,` |
|        - |  2310 | `	SyString *aShadow,` |
|        - |  2311 | `	sxu32 nShadow)` |
|        3 |  2312 | `{` |
|        - |  2313 | `	ph7_vm_func_closure_env sEnv;` |
|        - |  2314 | `	ph7_vm_func_closure_env *aEnv;` |
|        - |  2315 | `	sxu32 n, nEnv;` |
|        - |  2316 | `	char *zDup;` |
|      207 |  2317 | `	if( nByte == 0 ){` |
|      ! 0 |  2318 | `		return SXRET_OK;` |
|        - |  2319 | `	}` |
|      204 |  2320 | `	if( nByte == sizeof("this")-1` |
|      111 |  2321 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|        3 |  2322 | `		return SXRET_OK;` |
|        - |  2323 | `	}` |
|      257 |  2324 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|      192 |  2325 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|      186 |  2326 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|      143 |  2327 | `			return SXRET_OK;` |
|        - |  2328 | `		}` |
|       28 |  2329 | `	}` |
|       63 |  2330 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       63 |  2331 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|       91 |  2332 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|       30 |  2333 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|       29 |  2334 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|        3 |  2335 | `			return SXRET_OK;` |
|        - |  2336 | `		}` |
|       15 |  2337 | `	}` |
|       61 |  2338 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|       61 |  2339 | `	if( zDup == 0 ){` |
|      ! 0 |  2340 | `		return SXERR_ABORT;` |
|        - |  2341 | `	}` |
|       61 |  2342 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       61 |  2343 | `	sEnv.iFlags = 0;` |
|       61 |  2344 | `	sEnv.nIdx = SXU32_HIGH;` |
|       61 |  2345 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       61 |  2346 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|       61 |  2347 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       61 |  2348 | `	return SXRET_OK;` |
|      105 |  2349 | `}` |
|        - |  2350 | `/*` |
|        - |  2351 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|        - |  2352 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|        - |  2353 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|        - |  2354 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|        - |  2355 | ` */` |
|       56 |  2356 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|        - |  2357 | `	ph7_gen_state *pGen,` |
|        - |  2358 | `	ph7_vm_func *pFunc,` |
|        - |  2359 | `	const char *zIn,` |
|        - |  2360 | `	const char *zEnd,` |
|        - |  2361 | `	SyString *aShadow,` |
|        - |  2362 | `	sxu32 nShadow)` |
|        2 |  2363 | `{` |
|        - |  2364 | `	sxi32 rc;` |
|      370 |  2365 | `	while( zIn < zEnd ){` |
|      314 |  2366 | `		if( zIn[0] == '\\' ){` |
|        5 |  2367 | `			zIn++;` |
|        5 |  2368 | `			if( zIn < zEnd ){` |
|        5 |  2369 | `				zIn++;` |
|        2 |  2370 | `			}` |
|        5 |  2371 | `			continue;` |
|        - |  2372 | `		}` |
|      308 |  2373 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|       26 |  2374 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|       24 |  2375 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|        - |  2376 | `			const char *zName;` |
|       26 |  2377 | `			zIn++; /* skip '$' */` |
|       26 |  2378 | `			zName = zIn;` |
|       82 |  2379 | `			while( zIn < zEnd ){` |
|       76 |  2380 | `				unsigned char c = (unsigned char)zIn[0];` |
|       76 |  2381 | `				if( c >= 0xc0 ){` |
|      ! 0 |  2382 | `					zIn++;` |
|      ! 0 |  2383 | `					while( zIn < zEnd` |
|      ! 0 |  2384 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  2385 | `						zIn++;` |
|      ! 0 |  2386 | `					}` |
|      ! 0 |  2387 | `					continue;` |
|        - |  2388 | `				}` |
|       76 |  2389 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       20 |  2390 | `					break;` |
|        - |  2391 | `				}` |
|       58 |  2392 | `				zIn++;` |
|        2 |  2393 | `			}` |
|       26 |  2394 | `			if( zIn > zName ){` |
|       38 |  2395 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|       24 |  2396 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|       26 |  2397 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  2398 | `					return SXERR_ABORT;` |
|        - |  2399 | `				}` |
|       12 |  2400 | `			}` |
|       26 |  2401 | `			continue;` |
|        - |  2402 | `		}` |
|      286 |  2403 | `		zIn++;` |
|        2 |  2404 | `	}` |
|       58 |  2405 | `	return SXRET_OK;` |
|       30 |  2406 | `}` |
|        - |  2407 | `/*` |
|        - |  2408 | ` * Scan the body token range of an arrow function for free-variable` |
|        - |  2409 | ` * references and record them in pFunc's closure environment. Handles:` |
|        - |  2410 | ` *   - plain $<id> pairs` |
|        - |  2411 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|        - |  2412 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|        - |  2413 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|        - |  2414 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|        - |  2415 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|        - |  2416 | ` *     are never mistakenly captured.` |
|        - |  2417 | ` */` |
|      304 |  2418 | `static sxi32 GenStateArrowCaptureScan(` |
|        - |  2419 | `	ph7_gen_state *pGen,` |
|        - |  2420 | `	ph7_vm_func *pFunc,` |
|        - |  2421 | `	SyToken *pStart,` |
|        - |  2422 | `	SyToken *pEnd,` |
|        - |  2423 | `	SyString *aShadow,` |
|        - |  2424 | `	sxu32 nShadow)` |
|        4 |  2425 | `{` |
|      308 |  2426 | `	SyToken *pScan = pStart;` |
|        - |  2427 | `	sxi32 rc;` |
|     1740 |  2428 | `	while( pScan < pEnd ){` |
|     1436 |  2429 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|       86 |  2430 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|       28 |  2431 | `				pScan->sData.zString,` |
|       56 |  2432 | `				pScan->sData.zString + pScan->sData.nByte,` |
|       28 |  2433 | `				aShadow,nShadow);` |
|       58 |  2434 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2435 | `				return SXERR_ABORT;` |
|        - |  2436 | `			}` |
|       58 |  2437 | `			pScan++;` |
|       58 |  2438 | `			continue;` |
|        - |  2439 | `		}` |
|     1380 |  2440 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|       30 |  2441 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|       30 |  2442 | `			SyToken *pFnKw = pScan;` |
|       28 |  2443 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|      ! 0 |  2444 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|        2 |  2445 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|      ! 0 |  2446 | `				pFnKw = &pScan[1];` |
|      ! 0 |  2447 | `				nKw = PH7_TKWRD_FN;` |
|      ! 0 |  2448 | `			}` |
|       30 |  2449 | `			if( nKw == PH7_TKWRD_FN ){` |
|        - |  2450 | `				SyToken *pInnerSigStart;` |
|        - |  2451 | `				SyToken *pInnerSigEnd;` |
|        - |  2452 | `				SyToken *pInnerBodyEnd;` |
|        - |  2453 | `				SyString *aInnerShadow;` |
|        - |  2454 | `				sxu32 nInnerShadow;` |
|        - |  2455 | `				sxu32 nInnerParamMax;` |
|        - |  2456 | `				SyToken *p;` |
|        - |  2457 | `				int iNestInner;` |
|       19 |  2458 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|       19 |  2459 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  2460 | `					pScan++;` |
|      ! 0 |  2461 | `				}` |
|       19 |  2462 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 |  2463 | `					pScan++;` |
|      ! 0 |  2464 | `					continue;` |
|        - |  2465 | `				}` |
|       19 |  2466 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|       19 |  2467 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|        - |  2468 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|       19 |  2469 | `				if( pInnerSigEnd >= pEnd ){` |
|      ! 0 |  2470 | `					pScan = pEnd;` |
|      ! 0 |  2471 | `					continue;` |
|        - |  2472 | `				}` |
|        - |  2473 | `				/* Build an augmented shadow list: inherited + inner params */` |
|       19 |  2474 | `				nInnerParamMax = 0;` |
|       57 |  2475 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|       39 |  2476 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|       13 |  2477 | `						nInnerParamMax++;` |
|        6 |  2478 | `					}` |
|       20 |  2479 | `				}` |
|       19 |  2480 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|       18 |  2481 | `					&pGen->pVm->sAllocator,` |
|       18 |  2482 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|       19 |  2483 | `				if( aInnerShadow == 0 ){` |
|      ! 0 |  2484 | `					return SXERR_ABORT;` |
|        - |  2485 | `				}` |
|       19 |  2486 | `				nInnerShadow = 0;` |
|       25 |  2487 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|        7 |  2488 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|        4 |  2489 | `				}` |
|       57 |  2490 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|       39 |  2491 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|       27 |  2492 | `						continue;` |
|        - |  2493 | `					}` |
|       13 |  2494 | `					if( &p[1] >= pInnerSigEnd ){` |
|      ! 0 |  2495 | `						break;` |
|        - |  2496 | `					}` |
|       13 |  2497 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  2498 | `						continue;` |
|        - |  2499 | `					}` |
|       13 |  2500 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|        7 |  2501 | `				}` |
|       19 |  2502 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|       19 |  2503 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|      ! 0 |  2504 | `					pScan++;` |
|      ! 0 |  2505 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|      ! 0 |  2506 | `						&& pScan->sData.nByte == 1` |
|      ! 0 |  2507 | `						&& pScan->sData.zString[0] == '?' ){` |
|      ! 0 |  2508 | `						pScan++;` |
|      ! 0 |  2509 | `					}` |
|      ! 0 |  2510 | `					if( pScan < pEnd` |
|      ! 0 |  2511 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|      ! 0 |  2512 | `						pScan++;` |
|      ! 0 |  2513 | `					}` |
|      ! 0 |  2514 | `				}` |
|       19 |  2515 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|       19 |  2516 | `					pScan++; /* past '=>' */` |
|        9 |  2517 | `				}` |
|       19 |  2518 | `				pInnerBodyEnd = pScan;` |
|       19 |  2519 | `				iNestInner = 0;` |
|      131 |  2520 | `				while( pInnerBodyEnd < pEnd ){` |
|      113 |  2521 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|        - |  2522 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|        - |  2523 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|      ! 0 |  2524 | `						break;` |
|        - |  2525 | `					}` |
|      113 |  2526 | `					if( pInnerBodyEnd->nType &` |
|        - |  2527 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        3 |  2528 | `						iNestInner++;` |
|      112 |  2529 | `					}else if( pInnerBodyEnd->nType &` |
|        - |  2530 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        3 |  2531 | `						iNestInner--;` |
|        1 |  2532 | `					}` |
|      113 |  2533 | `					pInnerBodyEnd++;` |
|        1 |  2534 | `				}` |
|        - |  2535 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|        - |  2536 | `				 * the outer's body: a default value is evaluated at call time` |
|        - |  2537 | `				 * in the outer frame, so any free variable it references is` |
|        - |  2538 | `				 * an outer capture. We must NOT scan the parameter-name` |
|        - |  2539 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|        - |  2540 | `				 * or those names leak into the outer's closure environment.` |
|        - |  2541 | `				 *` |
|        - |  2542 | `				 * Walk the signature argument-by-argument, splitting on` |
|        - |  2543 | `				 * top-level commas, and for each argument scan only the token` |
|        - |  2544 | `				 * range after the '=' sign. */` |
|        - |  2545 | `				{` |
|       19 |  2546 | `					SyToken *pArgStart = pInnerSigStart;` |
|       31 |  2547 | `					while( pArgStart < pInnerSigEnd ){` |
|       13 |  2548 | `						SyToken *pArgEnd = pArgStart;` |
|       13 |  2549 | `						SyToken *pEq = 0;` |
|       13 |  2550 | `						int iNestArg = 0;` |
|       49 |  2551 | `						while( pArgEnd < pInnerSigEnd ){` |
|       38 |  2552 | `							if( iNestArg == 0` |
|       39 |  2553 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|        3 |  2554 | `								break;` |
|        - |  2555 | `							}` |
|       37 |  2556 | `							if( pArgEnd->nType &` |
|        - |  2557 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      ! 0 |  2558 | `								iNestArg++;` |
|       37 |  2559 | `							}else if( pArgEnd->nType &` |
|        - |  2560 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      ! 0 |  2561 | `								iNestArg--;` |
|      ! 0 |  2562 | `							}` |
|       36 |  2563 | `							if( pEq == 0 && iNestArg == 0` |
|       31 |  2564 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|        7 |  2565 | `								pEq = pArgEnd;` |
|        3 |  2566 | `							}` |
|       37 |  2567 | `							pArgEnd++;` |
|        1 |  2568 | `						}` |
|       13 |  2569 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|       10 |  2570 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|        3 |  2571 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|        7 |  2572 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 |  2573 | `								return SXERR_ABORT;` |
|        - |  2574 | `							}` |
|        3 |  2575 | `						}` |
|       13 |  2576 | `						pArgStart = pArgEnd;` |
|       12 |  2577 | `						if( pArgStart < pInnerSigEnd` |
|        8 |  2578 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|        3 |  2579 | `							pArgStart++;` |
|        1 |  2580 | `						}` |
|        1 |  2581 | `					}` |
|        - |  2582 | `				}` |
|       28 |  2583 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|        9 |  2584 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|       19 |  2585 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  2586 | `					return SXERR_ABORT;` |
|        - |  2587 | `				}` |
|       19 |  2588 | `				pScan = pInnerBodyEnd;` |
|       19 |  2589 | `				continue;` |
|        - |  2590 | `			}` |
|        5 |  2591 | `		}` |
|     1362 |  2592 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     1182 |  2593 | `			pScan++;` |
|     1182 |  2594 | `			continue;` |
|        - |  2595 | `		}` |
|        - |  2596 | `		{` |
|        - |  2597 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|      183 |  2598 | `			SyToken *pDollar = pScan;` |
|      270 |  2599 | `			while( &pDollar[1] < pEnd` |
|      183 |  2600 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|      ! 0 |  2601 | `				pDollar++;` |
|      ! 0 |  2602 | `			}` |
|      183 |  2603 | `			if( &pDollar[1] >= pEnd ){` |
|      ! 0 |  2604 | `				break;` |
|        - |  2605 | `			}` |
|      183 |  2606 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  2607 | `				pScan = pDollar + 1;` |
|      ! 0 |  2608 | `				continue;` |
|        - |  2609 | `			}` |
|      273 |  2610 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|      180 |  2611 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|       90 |  2612 | `				aShadow,nShadow);` |
|      183 |  2613 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2614 | `				return SXERR_ABORT;` |
|        - |  2615 | `			}` |
|      183 |  2616 | `			pScan = pDollar + 2;` |
|        - |  2617 | `		}` |
|        3 |  2618 | `	}` |
|      308 |  2619 | `	return SXRET_OK;` |
|      156 |  2620 | `}` |
|        - |  2621 | `/*` |
|        - |  2622 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|        - |  2623 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|        - |  2624 | ` * variables by value. The body is a single expression that acts as an` |
|        - |  2625 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|        - |  2626 | ` * $this is also made available.` |
|        - |  2627 | ` */` |
|      286 |  2628 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  2629 | `{` |
|        - |  2630 | `	ph7_vm_func *pFunc;` |
|        - |  2631 | `	ph7_vm_func_closure_env sEnv;` |
|        - |  2632 | `	GenBlock *pBlock;` |
|        - |  2633 | `	SySet *pInstrContainer;` |
|        - |  2634 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|        - |  2635 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|        - |  2636 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|        - |  2637 | `	SyToken *pSavedEnd;` |
|        - |  2638 | `	ph7_vm_func_arg *aArgs;` |
|        - |  2639 | `	char zName[512];` |
|        - |  2640 | `	static int iCnt = 1;` |
|        - |  2641 | `	char *zDup;` |
|        - |  2642 | `	SyToken *pTokKw;` |
|        - |  2643 | `	sxu32 nLen;` |
|        - |  2644 | `	sxu32 nLine;` |
|      291 |  2645 | `	sxi32 iFlags = 0;` |
|      291 |  2646 | `	int bStatic = 0;` |
|        - |  2647 | `	sxi32 rc;` |
|        - |  2648 | `	sxu32 n;` |
|      143 |  2649 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  2650 |  |
|      291 |  2651 | `	nLine = pGen->pIn->nLine;` |
|        - |  2652 | ``	/* Attribute-sidecar key: `#[A] [static] fn` trivia is keyed to this token */`` |
|      291 |  2653 | `	pTokKw = pGen->pIn;` |
|        - |  2654 | `	/* Optional 'static' prefix */` |
|      286 |  2655 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      291 |  2656 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        7 |  2657 | `		bStatic = 1;` |
|        7 |  2658 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|        7 |  2659 | `		pGen->pIn++;` |
|        3 |  2660 | `	}` |
|        - |  2661 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|      286 |  2662 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      291 |  2663 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|      ! 0 |  2664 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2665 | `			"Arrow function: expected 'fn' keyword");` |
|      ! 0 |  2666 | `		return SXERR_SYNTAX;` |
|        - |  2667 | `	}` |
|      291 |  2668 | `	pGen->pIn++; /* Jump 'fn' */` |
|        - |  2669 | `	/* Optional '&' — return by reference */` |
|      291 |  2670 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  2671 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|      ! 0 |  2672 | `		pGen->pIn++;` |
|      ! 0 |  2673 | `	}` |
|        - |  2674 | `	/* Expect '(' */` |
|      291 |  2675 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        3 |  2676 | `		if( pGen->pIn < pGen->pEnd ){` |
|        4 |  2677 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - |  2678 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|        2 |  2679 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        2 |  2680 | `		}else{` |
|      ! 0 |  2681 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  2682 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|        - |  2683 | `		}` |
|        3 |  2684 | `		return SXERR_SYNTAX;` |
|        - |  2685 | `	}` |
|      289 |  2686 | `	pGen->pIn++; /* Jump '(' */` |
|        - |  2687 | `	/* Delimit the parameter list */` |
|      289 |  2688 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|      289 |  2689 | `	if( pSigEnd >= pGen->pEnd ){` |
|        3 |  2690 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  2691 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|        3 |  2692 | `		return SXERR_SYNTAX;` |
|        - |  2693 | `	}` |
|        - |  2694 | `	/* Allocate the function state */` |
|      287 |  2695 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|      287 |  2696 | `	if( pFunc == 0 ){` |
|      ! 0 |  2697 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2698 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2699 | `		return SXERR_ABORT;` |
|        - |  2700 | `	}` |
|        - |  2701 | `	/* Generate a unique lambda name */` |
|      287 |  2702 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      287 |  2703 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 |  2704 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      ! 0 |  2705 | `	}` |
|      287 |  2706 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|      287 |  2707 | `	if( zDup == 0 ){` |
|      ! 0 |  2708 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2709 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2710 | `		return SXERR_ABORT;` |
|        - |  2711 | `	}` |
|      287 |  2712 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|        - |  2713 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|      287 |  2714 | `	pFunc->nLine = nLine;` |
|        - |  2715 | ``	/* Expression-position attributes (`$f = #[A] fn () => …`) */`` |
|      287 |  2716 | `	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  2717 | `		return SXERR_ABORT;` |
|        - |  2718 | `	}` |
|        - |  2719 | `	/* Collect function arguments */` |
|      287 |  2720 | `	if( pGen->pIn < pSigEnd ){` |
|      116 |  2721 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|      116 |  2722 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  2723 | `			return SXERR_ABORT;` |
|        - |  2724 | `		}` |
|       56 |  2725 | `	}` |
|        - |  2726 | `	/* Point past ')' and parse optional return type */` |
|      287 |  2727 | `	pGen->pIn = &pSigEnd[1];` |
|      287 |  2728 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|      287 |  2729 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2730 | `		return SXERR_ABORT;` |
|      287 |  2731 | `	}else if( rc == SXERR_SYNTAX ){` |
|      ! 0 |  2732 | `		return SXERR_SYNTAX;` |
|        - |  2733 | `	}` |
|        - |  2734 | `	/* Expect '=>' */` |
|      287 |  2735 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|        3 |  2736 | `		if( pGen->pIn < pGen->pEnd ){` |
|        4 |  2737 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - |  2738 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|        2 |  2739 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        2 |  2740 | `		}else{` |
|      ! 0 |  2741 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  2742 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|        - |  2743 | `		}` |
|        3 |  2744 | `		return SXERR_SYNTAX;` |
|        - |  2745 | `	}` |
|      284 |  2746 | `	pGen->pIn++; /* Jump '=>' */` |
|      284 |  2747 | `	pBodyStart = pGen->pIn;` |
|      284 |  2748 | `	pBodyEnd = pGen->pEnd;` |
|        - |  2749 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|        - |  2750 | `	 * recursively collect free-variable references from the body. The scan` |
|        - |  2751 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|        - |  2752 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|      284 |  2753 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|        - |  2754 | `	{` |
|      284 |  2755 | `		SyString *aShadow = 0;` |
|      284 |  2756 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|      284 |  2757 | `		if( nShadow > 0 ){` |
|      113 |  2758 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      110 |  2759 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|      113 |  2760 | `			if( aShadow == 0 ){` |
|      ! 0 |  2761 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2762 | `					"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2763 | `				return SXERR_ABORT;` |
|        - |  2764 | `			}` |
|      257 |  2765 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|      147 |  2766 | `				aShadow[n] = aArgs[n].sName;` |
|       75 |  2767 | `			}` |
|       55 |  2768 | `		}` |
|      424 |  2769 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      140 |  2770 | `			aShadow,nShadow);` |
|      284 |  2771 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  2772 | `			return SXERR_ABORT;` |
|        - |  2773 | `		}` |
|        - |  2774 | `	}` |
|        - |  2775 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|        - |  2776 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|        - |  2777 | `	 * captured value is silently dropped when the enclosing scope has no` |
|        - |  2778 | `	 * $this. */` |
|      284 |  2779 | `	if( !bStatic ){` |
|        - |  2780 | `		char *zThisDup;` |
|      278 |  2781 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|      278 |  2782 | `		if( zThisDup == 0 ){` |
|      ! 0 |  2783 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2784 | `				"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2785 | `			return SXERR_ABORT;` |
|        - |  2786 | `		}` |
|      278 |  2787 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      278 |  2788 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|      278 |  2789 | `		sEnv.nIdx = SXU32_HIGH;` |
|      278 |  2790 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      278 |  2791 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|      278 |  2792 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      137 |  2793 | `	}` |
|        - |  2794 | `	/* Arrow functions are always closures */` |
|      284 |  2795 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|        - |  2796 | `	/* Compile the body expression as an implicit return */` |
|      424 |  2797 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      140 |  2798 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|      284 |  2799 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  2800 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2801 | `			"PH7 engine is running out-of-memory");` |
|      ! 0 |  2802 | `		return SXERR_ABORT;` |
|        - |  2803 | `	}` |
|      284 |  2804 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      284 |  2805 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|      284 |  2806 | `	pSavedEnd = pGen->pEnd;` |
|      284 |  2807 | `	pGen->pIn = pBodyStart;` |
|      284 |  2808 | `	pGen->pEnd = pBodyEnd;` |
|      284 |  2809 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      284 |  2810 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2811 | `		return SXERR_ABORT;` |
|        - |  2812 | `	}` |
|        - |  2813 | `	/* The cursor stopped just past the body expression */` |
|      284 |  2814 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|        - |  2815 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|        - |  2816 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|        - |  2817 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|        - |  2818 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|      284 |  2819 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      284 |  2820 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      284 |  2821 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      284 |  2822 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      284 |  2823 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - |  2824 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|      284 |  2825 | `	pGen->pIn = pBodyEnd;` |
|      284 |  2826 | `	pGen->pEnd = pSavedEnd;` |
|        - |  2827 | `	/* Emit the load-closure instruction */` |
|      284 |  2828 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|      284 |  2829 | `	return SXRET_OK;` |
|      148 |  2830 | `}` |
|        - |  2831 | `/*` |
|        - |  2832 | ` * Compile a single arm's expression range into a freshly-allocated` |
|        - |  2833 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|        - |  2834 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|        - |  2835 | ` * expression's value.` |
|        - |  2836 | ` */` |
|      354 |  2837 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|        - |  2838 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|        3 |  2839 | `{` |
|        - |  2840 | `	SySet *pInstrContainer;` |
|        - |  2841 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |  2842 | `	GenBlock *pArmBlock;` |
|        - |  2843 | `	sxi32 rc;` |
|      357 |  2844 | `	pTmpIn  = pGen->pIn;` |
|      357 |  2845 | `	pTmpEnd = pGen->pEnd;` |
|      357 |  2846 | `	pGen->pIn  = pStart;` |
|      357 |  2847 | `	pGen->pEnd = pStop;` |
|      357 |  2848 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      357 |  2849 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|        - |  2850 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|        - |  2851 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|        - |  2852 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|        - |  2853 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|        - |  2854 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|      534 |  2855 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      177 |  2856 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|      357 |  2857 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  2858 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  2859 | `		pGen->pIn  = pTmpIn;` |
|      ! 0 |  2860 | `		pGen->pEnd = pTmpEnd;` |
|      ! 0 |  2861 | `		return SXERR_ABORT;` |
|        - |  2862 | `	}` |
|      357 |  2863 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      357 |  2864 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      357 |  2865 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      357 |  2866 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      357 |  2867 | `	GenStateLeaveBlock(&(*pGen),0);` |
|      357 |  2868 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      357 |  2869 | `	pGen->pIn  = pTmpIn;` |
|      357 |  2870 | `	pGen->pEnd = pTmpEnd;` |
|      357 |  2871 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2872 | `		return SXERR_ABORT;` |
|        - |  2873 | `	}` |
|      357 |  2874 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 |  2875 | `		return SXERR_EMPTY;` |
|        - |  2876 | `	}` |
|      357 |  2877 | `	return SXRET_OK;` |
|      180 |  2878 | `}` |
|        - |  2879 | `/*` |
|        - |  2880 | ` * Compile a PHP 8.0 match expression:` |
|        - |  2881 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|        - |  2882 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|        - |  2883 | ` * Strict comparison (===) is used between the subject and each condition.` |
|        - |  2884 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|        - |  2885 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|        - |  2886 | ` */` |
|        - |  2887 | `/*` |
|        - |  2888 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|        - |  2889 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|        - |  2890 | ` * caller can bail out of the current expression.` |
|        - |  2891 | ` */` |
|        2 |  2892 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|        1 |  2893 | `{` |
|        - |  2894 | `	va_list ap;` |
|        - |  2895 | `	sxi32 rc;` |
|        - |  2896 | `	SyBlob sMsg;` |
|        3 |  2897 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|        3 |  2898 | `	va_start(ap,zFmt);` |
|        3 |  2899 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|        3 |  2900 | `	va_end(ap);` |
|        3 |  2901 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|        3 |  2902 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|        3 |  2903 | `	SyBlobRelease(&sMsg);` |
|        3 |  2904 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2905 | `		return SXERR_ABORT;` |
|        - |  2906 | `	}` |
|        3 |  2907 | `	return SXERR_SYNTAX;` |
|        2 |  2908 | `}` |
|        - |  2909 | `/*` |
|        - |  2910 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|        - |  2911 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|        - |  2912 | ` * Returns the stop token pointer (or pEnd if none found).` |
|        - |  2913 | ` */` |
|      356 |  2914 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|        4 |  2915 | `{` |
|      360 |  2916 | `	SyToken *pCur = pStart;` |
|      360 |  2917 | `	int iNest = 0;` |
|      838 |  2918 | `	while( pCur < pEnd ){` |
|      802 |  2919 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       13 |  2920 | `			iNest++;` |
|      796 |  2921 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       13 |  2922 | `			iNest--;` |
|      784 |  2923 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|      323 |  2924 | `			return pCur;` |
|        - |  2925 | `		}` |
|      482 |  2926 | `		pCur++;` |
|        4 |  2927 | `	}` |
|       39 |  2928 | `	return pEnd;` |
|      182 |  2929 | `}` |
|       72 |  2930 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  2931 | `{` |
|        - |  2932 | `	ph7_match *pMatch;` |
|        - |  2933 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|       77 |  2934 | `	int bHasDefault = 0;` |
|        - |  2935 | `	sxu32 nLine;` |
|        - |  2936 | `	sxi32 rc;` |
|       36 |  2937 | `	SXUNUSED(iCompileFlag);` |
|       77 |  2938 | `	nLine = pGen->pIn->nLine;` |
|       77 |  2939 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|        - |  2940 | `	/* Expect '(' */` |
|       77 |  2941 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 |  2942 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  2943 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|      ! 0 |  2944 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|        - |  2945 | `	}` |
|       77 |  2946 | `	pGen->pIn++; /* Jump '(' */` |
|       77 |  2947 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|       77 |  2948 | `	if( pSubjEnd >= pGen->pEnd ){` |
|      ! 0 |  2949 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  2950 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|        - |  2951 | `	}` |
|       77 |  2952 | `	if( pGen->pIn >= pSubjEnd ){` |
|      ! 0 |  2953 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  2954 | `			"syntax error, unexpected \")\", expecting match subject");` |
|        - |  2955 | `	}` |
|        - |  2956 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|       77 |  2957 | `	pSavedEnd = pGen->pEnd;` |
|       77 |  2958 | `	pGen->pEnd = pSubjEnd;` |
|       77 |  2959 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       77 |  2960 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2961 | `		return SXERR_ABORT;` |
|        - |  2962 | `	}` |
|       77 |  2963 | `	pGen->pEnd = pSavedEnd;` |
|       77 |  2964 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|        - |  2965 | `	/* Expect '{' */` |
|       77 |  2966 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|      ! 0 |  2967 | `		return GenStateMatchError(pGen,` |
|      ! 0 |  2968 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|        - |  2969 | `			"syntax error, expecting \"{\" after match subject");` |
|        - |  2970 | `	}` |
|       77 |  2971 | `	pGen->pIn++; /* Jump '{' */` |
|       77 |  2972 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|       77 |  2973 | `	if( pBodyEnd >= pGen->pEnd ){` |
|      ! 0 |  2974 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  2975 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|        - |  2976 | `	}` |
|        - |  2977 | `	/* Allocate ph7_match container */` |
|       77 |  2978 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|       77 |  2979 | `	if( pMatch == 0 ){` |
|      ! 0 |  2980 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2981 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2982 | `		return SXERR_ABORT;` |
|        - |  2983 | `	}` |
|       77 |  2984 | `	SyZero(pMatch,sizeof(ph7_match));` |
|       77 |  2985 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|        - |  2986 | `	/* Iterate arms */` |
|      259 |  2987 | `	while( pGen->pIn < pBodyEnd ){` |
|        - |  2988 | `		ph7_match_arm sArm;` |
|        - |  2989 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|      190 |  2990 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|      190 |  2991 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|      190 |  2992 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|      190 |  2993 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - |  2994 | `		/* 'default' arm? */` |
|      186 |  2995 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      107 |  2996 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|       22 |  2997 | `			if( bHasDefault ){` |
|        3 |  2998 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|        - |  2999 | `					"Match expressions may only contain one default arm");` |
|        4 |  3000 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  3001 | `			}` |
|       20 |  3002 | `			sArm.bDefault = 1;` |
|       20 |  3003 | `			bHasDefault = 1;` |
|       20 |  3004 | `			pGen->pIn++;` |
|       20 |  3005 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|      ! 0 |  3006 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  3007 | `					"syntax error, expecting \"=>\" after 'default'");` |
|        - |  3008 | `			}` |
|       20 |  3009 | `			pGen->pIn++; /* Jump '=>' */` |
|       11 |  3010 | `		}else{` |
|        - |  3011 | `			/* Condition list: cond (',' cond)* '=>' */` |
|      170 |  3012 | `			pCondStart = pGen->pIn;` |
|      170 |  3013 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|        - |  3014 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|      178 |  3015 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|        - |  3016 | `				SySet sCondBc;` |
|        9 |  3017 | `				if( pCondStart >= pArrow ){` |
|      ! 0 |  3018 | `					return GenStateMatchError(pGen,nArmLine,` |
|        - |  3019 | `						"syntax error, empty match condition expression");` |
|        - |  3020 | `				}` |
|        9 |  3021 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        9 |  3022 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|        9 |  3023 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  3024 | `					return SXERR_ABORT;` |
|        - |  3025 | `				}` |
|        9 |  3026 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|        9 |  3027 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|        9 |  3028 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|        - |  3029 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|        1 |  3030 | `			}` |
|      170 |  3031 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|        3 |  3032 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  3033 | `					"syntax error, expecting \"=>\" in match arm");` |
|        - |  3034 | `			}` |
|      167 |  3035 | `			if( pCondStart >= pArrow ){` |
|      ! 0 |  3036 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  3037 | `					"syntax error, empty match condition expression");` |
|        - |  3038 | `			}` |
|        - |  3039 | `			{` |
|        - |  3040 | `				SySet sCondBc;` |
|      167 |  3041 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      167 |  3042 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|      167 |  3043 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  3044 | `					return SXERR_ABORT;` |
|        - |  3045 | `				}` |
|      167 |  3046 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|        - |  3047 | `			}` |
|      167 |  3048 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|        - |  3049 | `		}` |
|        - |  3050 | `		/* Compile result expression: up to top-level ',' or body end */` |
|      185 |  3051 | `		pResStart = pGen->pIn;` |
|      185 |  3052 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|      185 |  3053 | `		if( pResStart >= pResEnd ){` |
|      ! 0 |  3054 | `			return GenStateMatchError(pGen,nArmLine,` |
|        - |  3055 | `				"syntax error, expected expression after \"=>\"");` |
|        - |  3056 | `		}` |
|      185 |  3057 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|      185 |  3058 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3059 | `			return SXERR_ABORT;` |
|        - |  3060 | `		}` |
|      185 |  3061 | `		pGen->pIn = pResEnd;` |
|      185 |  3062 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      151 |  3063 | `			pGen->pIn++; /* Skip trailing ',' */` |
|       74 |  3064 | `		}` |
|      185 |  3065 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|        3 |  3066 | `	}` |
|       71 |  3067 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|       71 |  3068 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|       71 |  3069 | `	return SXRET_OK;` |
|       41 |  3070 | `}` |
|        - |  3071 | `/*` |
|        - |  3072 | ` * Compile a backtick quoted string.` |
|        - |  3073 | ` */` |
|        4 |  3074 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        2 |  3075 | `{` |
|        - |  3076 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|        - |  3077 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|        - |  3078 | `	 */` |
|        8 |  3079 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|        - |  3080 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|        2 |  3081 | `		ph7_lib_version()` |
|        - |  3082 | `		);` |
|        - |  3083 | `	/* Load NULL */` |
|        6 |  3084 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        2 |  3085 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  3086 | `	/* Node successfully compiled */` |
|        6 |  3087 | `	return SXRET_OK;` |
|        2 |  3088 | `}` |
|        - |  3089 | `/*` |
|        - |  3090 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|        - |  3091 | ` * construct.` |
|        - |  3092 | ` */` |
|       82 |  3093 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  3094 | `{` |
|        - |  3095 | `	SyString *pName;` |
|        - |  3096 | `	sxu32 nKeyID;` |
|        - |  3097 | `	sxi32 rc;` |
|        - |  3098 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|       87 |  3099 | `	pName = &pGen->pIn->sData;` |
|       87 |  3100 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       87 |  3101 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|       87 |  3102 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|        9 |  3103 | `		SyToken *pTmp,*pNext = 0;` |
|        - |  3104 | `		/* Compile arguments one after one */` |
|        9 |  3105 | `		pTmp = pGen->pEnd;` |
|        - |  3106 | `		/* Symisc eXtension to the PHP programming language:` |
|        - |  3107 | `		 * 'echo' can be used in the context of a function which` |
|        - |  3108 | `		 *  mean that the following expression is valid:` |
|        - |  3109 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|        - |  3110 | `		 */` |
|        9 |  3111 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|       17 |  3112 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|        9 |  3113 | `			if( pGen->pIn < pNext ){` |
|        9 |  3114 | `				pGen->pEnd = pNext;` |
|        9 |  3115 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|        9 |  3116 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  3117 | `					return SXERR_ABORT;` |
|        - |  3118 | `				}` |
|        9 |  3119 | `				if( rc != SXERR_EMPTY ){` |
|        - |  3120 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|        - |  3121 | `					 * without the overhead of a function call.` |
|        - |  3122 | `					 * This is a very powerful optimization that improve` |
|        - |  3123 | `					 * performance greatly.` |
|        - |  3124 | `					 */` |
|        9 |  3125 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|        4 |  3126 | `				}` |
|        4 |  3127 | `			}` |
|        - |  3128 | `			/* Jump trailing commas */` |
|        9 |  3129 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|      ! 0 |  3130 | `				pNext++;` |
|      ! 0 |  3131 | `			}` |
|        9 |  3132 | `			pGen->pIn = pNext;` |
|        1 |  3133 | `		}` |
|        - |  3134 | `		/* Restore token stream */` |
|        9 |  3135 | `		pGen->pEnd = pTmp;` |
|        5 |  3136 | `	}else{` |
|       79 |  3137 | `		sxi32 nArg = 0;` |
|       79 |  3138 | `		sxu32 nIdx = 0;` |
|       79 |  3139 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|       79 |  3140 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3141 | `			return SXERR_ABORT;` |
|       79 |  3142 | `		}else if(rc != SXERR_EMPTY ){` |
|       79 |  3143 | `			nArg = 1;` |
|       37 |  3144 | `		}` |
|       79 |  3145 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|        - |  3146 | `			ph7_value *pObj;` |
|        - |  3147 | `			/* Emit the call instruction */` |
|       31 |  3148 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       31 |  3149 | `			if( pObj == 0 ){` |
|      ! 0 |  3150 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3151 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  3152 | `				return SXERR_ABORT;` |
|        - |  3153 | `			}` |
|       31 |  3154 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|        - |  3155 | `			/* Install in the literal table */` |
|       31 |  3156 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       13 |  3157 | `		}` |
|        - |  3158 | `		/* Emit the call instruction */` |
|       79 |  3159 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       79 |  3160 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        - |  3161 | `	}` |
|        - |  3162 | `	/* Node successfully compiled */` |
|       87 |  3163 | `	return SXRET_OK;` |
|       46 |  3164 | `}` |
|        - |  3165 | `/*` |
|        - |  3166 | ` * Compile a node holding a variable declaration.` |
|        - |  3167 | ` * According to the PHP language reference` |
|        - |  3168 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|        - |  3169 | ` *  The variable name is case-sensitive.` |
|        - |  3170 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|        - |  3171 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|        - |  3172 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|        - |  3173 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|        - |  3174 | ` *  Note: $this is a special variable that can't be assigned.` |
|        - |  3175 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|        - |  3176 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|        - |  3177 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|        - |  3178 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|        - |  3179 | ` *  the chapter on Expressions.` |
|        - |  3180 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|        - |  3181 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|        - |  3182 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|        - |  3183 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|        - |  3184 | ` *  is being assigned (the source variable).` |
|        - |  3185 | ` */` |
| 12577930 |  3186 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  3187 | `{` |
| 12577935 |  3188 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |  3189 | `	sxi32 iVv;` |
|        - |  3190 | `	sxi32 iP1;` |
|        - |  3191 | `	void *p3;` |
|        - |  3192 | `	sxi32 rc;` |
| 12577935 |  3193 | `	iVv = -1; /* Variable variable counter */` |
| 25155877 |  3194 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
| 12577947 |  3195 | `		pGen->pIn++;` |
| 12577947 |  3196 | `		iVv++;` |
|        5 |  3197 | `	}` |
| 12577935 |  3198 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|        - |  3199 | `		/* Invalid variable name */` |
|      ! 0 |  3200 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|      ! 0 |  3201 | `		if( rc == SXERR_ABORT ){` |
|        - |  3202 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3203 | `			return SXERR_ABORT;` |
|        - |  3204 | `		}` |
|      ! 0 |  3205 | `		return SXRET_OK;` |
|        - |  3206 | `	}` |
| 12577935 |  3207 | `	p3  = 0;` |
| 12577935 |  3208 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|        - |  3209 | `		/* Dynamic variable creation */` |
|       21 |  3210 | `		pGen->pIn++;  /* Jump the open curly */` |
|       21 |  3211 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|       21 |  3212 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - |  3213 | `			/* Empty expression */` |
|        3 |  3214 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|        3 |  3215 | `			return SXRET_OK;` |
|        - |  3216 | `		}` |
|        - |  3217 | `		/* Compile the expression holding the variable name */` |
|       18 |  3218 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       18 |  3219 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3220 | `			return SXERR_ABORT;` |
|       18 |  3221 | `		}else if( rc == SXERR_EMPTY ){` |
|        3 |  3222 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|        3 |  3223 | `			return SXRET_OK;` |
|        - |  3224 | `		}` |
|        8 |  3225 | `	}else{` |
|        - |  3226 | `		SyHashEntry *pEntry;` |
|        - |  3227 | `		SyString *pName;` |
| 12577917 |  3228 | `		char *zName = 0;` |
|        - |  3229 | `		/* Extract variable name */` |
| 12577917 |  3230 | `		pName = &pGen->pIn->sData;` |
|        - |  3231 | `		/* Advance the stream cursor */` |
| 12577917 |  3232 | `		pGen->pIn++;` |
| 12577917 |  3233 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
| 12577917 |  3234 | `		if( pEntry == 0 ){` |
|        - |  3235 | `			/* Duplicate name */` |
|   774965 |  3236 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   774965 |  3237 | `			if( zName == 0 ){` |
|      ! 0 |  3238 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3239 | `				return SXERR_ABORT;` |
|        - |  3240 | `			}` |
|        - |  3241 | `			/* Install in the hashtable */` |
|   774965 |  3242 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   387485 |  3243 | `		}else{` |
|        - |  3244 | `			/* Name already available */` |
| 11802957 |  3245 | `			zName = (char *)pEntry->pUserData;` |
|        - |  3246 | `		}` |
| 12577917 |  3247 | `		p3 = (void *)zName;` |
|        - |  3248 | `	}` |
| 12577931 |  3249 | `	iP1 = 0;` |
| 12577931 |  3250 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  3697679 |  3251 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|        - |  3252 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  3697649 |  3253 | `			iP1 = 1;` |
|  1848822 |  3254 | `		}` |
|  1848837 |  3255 | `	}` |
|        - |  3256 | `	/* Emit the load instruction */` |
| 12577931 |  3257 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
| 12577943 |  3258 | `	while( iVv > 0 ){` |
|       13 |  3259 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|       13 |  3260 | `		iVv--;` |
|        1 |  3261 | `	}` |
|        - |  3262 | `	/* Node successfully compiled */` |
| 12577931 |  3263 | `	return SXRET_OK;` |
|  6288970 |  3264 | `}` |
|        - |  3265 | `/*` |
|        - |  3266 | ` * Load a literal.` |
|        - |  3267 | ` */` |
|  8122328 |  3268 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|        5 |  3269 | `{` |
|  8122333 |  3270 | `	SyToken *pToken = pGen->pIn;` |
|        - |  3271 | `	ph7_value *pObj;` |
|        - |  3272 | `	SyString *pStr;` |
|        - |  3273 | `	sxu32 nIdx;` |
|        - |  3274 | `	/* Extract token value */` |
|  8122333 |  3275 | `	pStr = &pToken->sData;` |
|        - |  3276 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  8122333 |  3277 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  1755723 |  3278 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|        - |  3279 | `			/* NULL constant are always indexed at 0 */` |
|   721869 |  3280 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   721869 |  3281 | `			return SXRET_OK;` |
|  1033859 |  3282 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|        - |  3283 | `			/* TRUE constant are always indexed at 1 */` |
|   187063 |  3284 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|   187063 |  3285 | `			return SXRET_OK;` |
|        5 |  3286 | `		}` |
|  7495431 |  3287 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  1410836 |  3288 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|        - |  3289 | `			/* FALSE constant are always indexed at 2 */` |
|   473521 |  3290 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   473521 |  3291 | `			return SXRET_OK;` |
|  6275759 |  3292 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   765320 |  3293 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|        - |  3294 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    11639 |  3295 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    11639 |  3296 | `			if( pObj == 0 ){` |
|      ! 0 |  3297 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3298 | `				return SXERR_ABORT;` |
|        - |  3299 | `			}` |
|    11639 |  3300 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|        - |  3301 | `			/* Emit the load constant instruction */` |
|    11639 |  3302 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    11639 |  3303 | `			return SXRET_OK;` |
|  5914789 |  3304 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|    66648 |  3305 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|        - |  3306 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|        8 |  3307 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        8 |  3308 | `			if( pObj == 0 ){` |
|      ! 0 |  3309 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3310 | `				return SXERR_ABORT;` |
|        - |  3311 | `			}` |
|        8 |  3312 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        - |  3313 | `				SyString sNs;` |
|        8 |  3314 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        8 |  3315 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|        5 |  3316 | `			}else{` |
|      ! 0 |  3317 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|        - |  3318 | `			}` |
|        8 |  3319 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        8 |  3320 | `			return SXRET_OK;` |
|  5936314 |  3321 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   258499 |  3322 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  6030221 |  3323 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   297548 |  3324 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|       11 |  3325 | `			GenBlock *pBlock = pGen->pCurrent;` |
|        - |  3326 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|       21 |  3327 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|        - |  3328 | `				/* Point to the upper block */` |
|       11 |  3329 | `				pBlock = pBlock->pParent;` |
|        1 |  3330 | `			}` |
|       11 |  3331 | `			if( pBlock == 0 ){` |
|        - |  3332 | `				/* Called in the global scope,load NULL */` |
|        5 |  3333 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        3 |  3334 | `			}else{` |
|        - |  3335 | `				/* Extract the target function/method */` |
|        7 |  3336 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        7 |  3337 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|        - |  3338 | `					/* Not a class method,Load null */` |
|        3 |  3339 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        2 |  3340 | `				}else{` |
|        5 |  3341 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        5 |  3342 | `					if( pObj == 0 ){` |
|      ! 0 |  3343 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3344 | `						return SXERR_ABORT;` |
|        - |  3345 | `					}` |
|        5 |  3346 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|        - |  3347 | `					/* Emit the load constant instruction */` |
|        5 |  3348 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - |  3349 | `				}` |
|        - |  3350 | `			}` |
|       11 |  3351 | `			return SXRET_OK;` |
|        - |  3352 | `	}` |
|        - |  3353 | `	/* Query literal table */` |
|  6728245 |  3354 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|        - |  3355 | `		ph7_value *pLitObj;` |
|        - |  3356 | `		/* Unknown literal,install it in the literal table */` |
|  1371393 |  3357 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  1371393 |  3358 | `		if( pLitObj == 0 ){` |
|      ! 0 |  3359 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  3360 | `			return SXERR_ABORT;` |
|        - |  3361 | `		}` |
|  1371393 |  3362 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  1371393 |  3363 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|   685694 |  3364 | `	}` |
|        - |  3365 | `	/* Emit the load constant instruction */` |
|  6728245 |  3366 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  6728245 |  3367 | `	return SXRET_OK;` |
|  4061169 |  3368 | `}` |
|        - |  3369 | `/*` |
|        - |  3370 | ` * Resolve a namespace path or simply load a literal.` |
|        - |  3371 | ` * If the token stream contains namespace separators (backslashes),` |
|        - |  3372 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|        - |  3373 | ` * Otherwise, load the simple literal directly.` |
|        - |  3374 | ` */` |
|  8126252 |  3375 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|        5 |  3376 | `{` |
|        - |  3377 | `	sxi32 rc;` |
|  8126257 |  3378 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  3379 | `		return SXRET_OK;` |
|        - |  3380 | `	}` |
|        - |  3381 | `	/* Check if this is a multi-token namespace path */` |
|  8126257 |  3382 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|        - |  3383 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|     3929 |  3384 | `		SyBlob *pWorker = &pGen->sWorker;` |
|     3929 |  3385 | `		int isAbsolute = 0;` |
|     3929 |  3386 | `		SyBlobReset(pWorker);` |
|        - |  3387 | `		/* Check for leading backslash (absolute path) */` |
|     3929 |  3388 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|     3927 |  3389 | `			isAbsolute = 1;` |
|     3927 |  3390 | `			pGen->pIn++; /* Skip leading backslash */` |
|     1961 |  3391 | `		}` |
|        - |  3392 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|     3929 |  3393 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        3 |  3394 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        3 |  3395 | `			SyBlobAppend(pWorker,"\\",1);` |
|        1 |  3396 | `		}` |
|        - |  3397 | `		/* Collect all path components */` |
|     4037 |  3398 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     4037 |  3399 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       59 |  3400 | `				SyBlobAppend(pWorker,"\\",1);` |
|       32 |  3401 | `			}else{` |
|     3983 |  3402 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - |  3403 | `			}` |
|     4037 |  3404 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|     3929 |  3405 | `				pGen->pIn++;` |
|     3929 |  3406 | `				break;` |
|        - |  3407 | `			}` |
|      113 |  3408 | `			pGen->pIn++;` |
|        5 |  3409 | `		}` |
|     3929 |  3410 | `		if( SyBlobLength(pWorker) > 0 ){` |
|        - |  3411 | `			ph7_value *pObj;` |
|        - |  3412 | `			SyString sPath;` |
|        - |  3413 | `			sxu32 nIdx;` |
|     3929 |  3414 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|        - |  3415 | `			/* Install in the literal table */` |
|     3929 |  3416 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|     3901 |  3417 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     3901 |  3418 | `				if( pObj == 0 ){` |
|      ! 0 |  3419 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  3420 | `					return SXERR_ABORT;` |
|        - |  3421 | `				}` |
|     3901 |  3422 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|     3901 |  3423 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     1948 |  3424 | `			}` |
|        - |  3425 | `			/* Emit the load constant instruction.` |
|        - |  3426 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|        - |  3427 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|     5891 |  3428 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|     1962 |  3429 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|     1962 |  3430 | `				nIdx,0,0);` |
|     3929 |  3431 | `			return SXRET_OK;` |
|        - |  3432 | `		}` |
|      ! 0 |  3433 | `	}` |
|        - |  3434 | `	/* Single-token literal: load directly */` |
|  8122333 |  3435 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  8122333 |  3436 | `	return rc;` |
|  4063131 |  3437 | `}` |
|        - |  3438 | `/*` |
|        - |  3439 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|        - |  3440 | ` */` |
|        - |  3441 | `/*` |
|        - |  3442 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|        - |  3443 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|        - |  3444 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|        - |  3445 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|        - |  3446 | ` */` |
|      ! 0 |  3447 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|      ! 0 |  3448 | `{` |
|      ! 0 |  3449 | `	SXUNUSED(iCompileFlag);` |
|      ! 0 |  3450 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|        - |  3451 | `		"Cannot use the first-class callable syntax '...' here");` |
|      ! 0 |  3452 | `	return SXERR_SYNTAX;` |
|      ! 0 |  3453 | `}` |
|  8126252 |  3454 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  3455 | `{` |
|        - |  3456 | `	sxi32 rc;` |
|  8126257 |  3457 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  8126257 |  3458 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  3459 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  3460 | `		return rc;` |
|        - |  3461 | `	}` |
|        - |  3462 | `	/* Node successfully compiled */` |
|  8126257 |  3463 | `	return SXRET_OK;` |
|  4063131 |  3464 | `}` |
|        - |  3465 | `/*` |
|        - |  3466 | ` * Recover from a compile-time error. In other words synchronize` |
|        - |  3467 | ` * the token stream cursor with the first semi-colon seen.` |
|        - |  3468 | ` */` |
|        8 |  3469 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|        1 |  3470 | `{` |
|        - |  3471 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|       17 |  3472 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|        9 |  3473 | `		pGen->pIn++;` |
|        1 |  3474 | `	}` |
|        9 |  3475 | `	return SXRET_OK;` |
|        1 |  3476 | `}` |
|        - |  3477 | `/*` |
|        - |  3478 | ` * Check if the given identifier name is reserved or not.` |
|        - |  3479 | ` * Return TRUE if reserved.FALSE otherwise.` |
|        - |  3480 | ` */` |
|   248284 |  3481 | `static int GenStateIsReservedConstant(SyString *pName)` |
|        5 |  3482 | `{` |
|   248289 |  3483 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|     3925 |  3484 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|        3 |  3485 | `			return TRUE;` |
|     3923 |  3486 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|        6 |  3487 | `			return TRUE;` |
|        5 |  3488 | `		}` |
|   246326 |  3489 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|     7777 |  3490 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|        3 |  3491 | `			return TRUE;` |
|        - |  3492 | `		}` |
|     3885 |  3493 | `	}` |
|        - |  3494 | `	/* Not a reserved constant */` |
|   248281 |  3495 | `	return FALSE;` |
|   124147 |  3496 | `}` |
|        - |  3497 | `/*` |
|        - |  3498 | ` * Compile the 'const' statement.` |
|        - |  3499 | ` * According to the PHP language reference` |
|        - |  3500 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|        - |  3501 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|        - |  3502 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|        - |  3503 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|        - |  3504 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|        - |  3505 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|        - |  3506 | ` *  Syntax` |
|        - |  3507 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|        - |  3508 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|        - |  3509 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|        - |  3510 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|        - |  3511 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|        - |  3512 | ` *  to get a list of all defined constants.` |
|        - |  3513 | ` *` |
|        - |  3514 | ` * Symisc eXtension.` |
|        - |  3515 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|        - |  3516 | ` *  would allow only simple scalar value.` |
|        - |  3517 | ` *  Example` |
|        - |  3518 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|        - |  3519 | ` *    Refer to the official documentation for more information on this feature.` |
|        - |  3520 | ` */` |
|       48 |  3521 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|        5 |  3522 | `{` |
|        - |  3523 | `	SySet *pConsCode,*pInstrContainer;` |
|       53 |  3524 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |  3525 | `	SyString *pName;` |
|        - |  3526 | `	sxi32 rc;` |
|       53 |  3527 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       53 |  3528 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  3529 | `		/* Invalid constant name */` |
|        8 |  3530 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|        8 |  3531 | `		if( rc == SXERR_ABORT ){` |
|        - |  3532 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3533 | `			return SXERR_ABORT;` |
|        - |  3534 | `		}` |
|        8 |  3535 | `		goto Synchronize;` |
|        - |  3536 | `	}` |
|        - |  3537 | `	/* Peek constant name */` |
|       46 |  3538 | `	pName = &pGen->pIn->sData;` |
|        - |  3539 | `	/* Make sure the constant name isn't reserved */` |
|       46 |  3540 | `	if( GenStateIsReservedConstant(pName) ){` |
|        - |  3541 | `		/* Reserved constant */` |
|       10 |  3542 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       10 |  3543 | `		if( rc == SXERR_ABORT ){` |
|        - |  3544 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3545 | `			return SXERR_ABORT;` |
|        - |  3546 | `		}` |
|       10 |  3547 | `		goto Synchronize;` |
|        - |  3548 | `	}` |
|       37 |  3549 | `	pGen->pIn++;` |
|       37 |  3550 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|        - |  3551 | `		/* Invalid statement*/` |
|        6 |  3552 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|        6 |  3553 | `		if( rc == SXERR_ABORT ){` |
|        - |  3554 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3555 | `			return SXERR_ABORT;` |
|        - |  3556 | `		}` |
|        6 |  3557 | `		goto Synchronize;` |
|        - |  3558 | `	}` |
|       32 |  3559 | `	pGen->pIn++; /*Jump the equal sign */` |
|        - |  3560 | `	/* Allocate a new constant value container */` |
|       32 |  3561 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|       32 |  3562 | `	if( pConsCode == 0 ){` |
|      ! 0 |  3563 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3564 | `		return SXERR_ABORT;` |
|        - |  3565 | `	}` |
|       32 |  3566 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - |  3567 | `	/* Swap bytecode container */` |
|       32 |  3568 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       32 |  3569 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|        - |  3570 | `	/* Compile constant value */` |
|       32 |  3571 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  3572 | `	/* Emit the done instruction */` |
|       32 |  3573 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       32 |  3574 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       32 |  3575 | `	if( rc == SXERR_ABORT ){` |
|        - |  3576 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  3577 | `		return SXERR_ABORT;` |
|        - |  3578 | `	}` |
|       32 |  3579 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|        - |  3580 | `	/* Register the constant with namespace-qualified name */` |
|        - |  3581 | `	{` |
|        - |  3582 | `		SyBlob sFQN;` |
|        - |  3583 | `		SyString sFQNStr;` |
|       32 |  3584 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       32 |  3585 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|       32 |  3586 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       47 |  3587 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|       30 |  3588 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|       32 |  3589 | `		if( rc == SXRET_OK && SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|        - |  3590 | ``			/* php 8.5: attributes on `const` statements — attach the pending`` |
|        - |  3591 | `			 * groups to the registered constant record for Reflection. */` |
|        7 |  3592 | `			SyHashEntry *pCEntry = SyHashGet(&pGen->pVm->hConstant,` |
|        4 |  3593 | `				SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        5 |  3594 | `			if( pCEntry ){` |
|        5 |  3595 | `				ph7_constant *pRegCons = (ph7_constant *)pCEntry->pUserData;` |
|        5 |  3596 | `				if( GenStateConsumeAttrs(&(*pGen),&pRegCons->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  3597 | `					SyBlobRelease(&sFQN);` |
|      ! 0 |  3598 | `					return SXERR_ABORT;` |
|        - |  3599 | `				}` |
|        2 |  3600 | `			}` |
|        2 |  3601 | `		}` |
|       32 |  3602 | `		SyBlobRelease(&sFQN);` |
|        - |  3603 | `	}` |
|       32 |  3604 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  3605 | `		SySetRelease(pConsCode);` |
|      ! 0 |  3606 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|      ! 0 |  3607 | `	}` |
|       32 |  3608 | `	return SXRET_OK;` |
|        9 |  3609 | `Synchronize:` |
|        - |  3610 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|       60 |  3611 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       41 |  3612 | `		pGen->pIn++;` |
|        3 |  3613 | `	}` |
|       22 |  3614 | `	return SXRET_OK;` |
|       29 |  3615 | `}` |
|        - |  3616 | `/*` |
|        - |  3617 | ` * Compile the 'continue' statement.` |
|        - |  3618 | ` * According to the PHP language reference` |
|        - |  3619 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|        - |  3620 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|        - |  3621 | ` *  iteration.` |
|        - |  3622 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|        - |  3623 | ` *  the purposes of continue.` |
|        - |  3624 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|        - |  3625 | ` *  of enclosing loops it should skip to the end of.` |
|        - |  3626 | ` *  Note:` |
|        - |  3627 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|        - |  3628 | ` */` |
|        - |  3629 | `/*` |
|        - |  3630 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|        - |  3631 | ` * block and the target loop block. This ensures finally blocks run when` |
|        - |  3632 | ` * break/continue crosses a try boundary.` |
|        - |  3633 | ` *` |
|        - |  3634 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|        - |  3635 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|        - |  3636 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|        - |  3637 | ` */` |
|    69922 |  3638 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|        5 |  3639 | `{` |
|    69927 |  3640 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    69927 |  3641 | `	int nInlineTry = 0;` |
|   318237 |  3642 | `	while( pBlock && pBlock != pTarget ){` |
|   248315 |  3643 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|        6 |  3644 | `			if( pBlock->pUserData ){` |
|        - |  3645 | `				/* A try block with an exception context. In a generator its catch/finally` |
|        - |  3646 | `				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that` |
|        - |  3647 | `				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.` |
|        - |  3648 | `				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */` |
|        6 |  3649 | `				if( pGen->bInGenerator ){` |
|        3 |  3650 | `					nInlineTry++;` |
|        2 |  3651 | `				}else{` |
|        3 |  3652 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|        - |  3653 | `				}` |
|        4 |  3654 | `			}else{` |
|        - |  3655 | `				/* A catch/finally block compiled into a separate bytecode container` |
|        - |  3656 | `				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */` |
|      ! 0 |  3657 | `				break;` |
|        - |  3658 | `			}` |
|        2 |  3659 | `		}` |
|   248315 |  3660 | `		pBlock = pBlock->pParent;` |
|        5 |  3661 | `	}` |
|    69927 |  3662 | `	return nInlineTry;` |
|        5 |  3663 | `}` |
|    31058 |  3664 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|        5 |  3665 | `{` |
|        - |  3666 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  3667 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  3668 | `	sxu32 nLineLocal;` |
|        - |  3669 | `	sxi32 rc;` |
|    31063 |  3670 | `	nLineLocal = pGen->pIn->nLine;` |
|    31063 |  3671 | `	iLevel = 0;` |
|        - |  3672 | `	/* Jump the 'continue' keyword */` |
|    31063 |  3673 | `	pGen->pIn++;` |
|    31063 |  3674 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|        - |  3675 | `		/* optional numeric argument which tells us how many levels` |
|        - |  3676 | `		 * of enclosing loops we should skip to the end of.` |
|        - |  3677 | `		 */` |
|        - |  3678 | `		char zScratch[GEN_NUM_SCRATCH];` |
|       17 |  3679 | `		char *zAlloc = 0;` |
|        - |  3680 | `		SyString sNum;` |
|       17 |  3681 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|       17 |  3682 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3683 | `			return SXERR_ABORT;` |
|        - |  3684 | `		}` |
|       17 |  3685 | `		if( rc == SXRET_OK ){` |
|       20 |  3686 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       12 |  3687 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|       14 |  3688 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3689 | `				return SXERR_ABORT;` |
|        - |  3690 | `			}` |
|       14 |  3691 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       14 |  3692 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        6 |  3693 | `		}` |
|       17 |  3694 | `		if( iLevel < 2 ){` |
|        3 |  3695 | `			iLevel = 0;` |
|        1 |  3696 | `		}` |
|       17 |  3697 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|        7 |  3698 | `	}` |
|        - |  3699 | `	/* Point to the target loop */` |
|    31063 |  3700 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    31063 |  3701 | `	if( pLoop == 0 ){` |
|        - |  3702 | `		/* Illegal continue */` |
|       13 |  3703 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|       13 |  3704 | `		if( rc == SXERR_ABORT ){` |
|        - |  3705 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3706 | `			return SXERR_ABORT;` |
|        - |  3707 | `		}` |
|        8 |  3708 | `	}else{` |
|    31053 |  3709 | `		sxu32 nInstrIdx = 0;` |
|        - |  3710 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    31053 |  3711 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  3712 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|        - |  3713 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|    31053 |  3714 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|    31053 |  3715 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|        - |  3716 | `			/* According to the PHP language reference manual` |
|        - |  3717 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|        - |  3718 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|        - |  3719 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|        - |  3720 | `			 */` |
|        5 |  3721 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|        5 |  3722 | `			if( rc == SXRET_OK ){` |
|        5 |  3723 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|        2 |  3724 | `			}` |
|        3 |  3725 | `		}else{` |
|        - |  3726 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    31049 |  3727 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    31049 |  3728 | `			if( pLoop->bPostContinue == TRUE ){` |
|        - |  3729 | `				JumpFixup sJumpFix;` |
|        - |  3730 | `				/* Post-continue */` |
|     3893 |  3731 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|     3893 |  3732 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|     3893 |  3733 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|     1944 |  3734 | `			}` |
|        - |  3735 | `		}` |
|        - |  3736 | `	}` |
|    31063 |  3737 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  3738 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  3739 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|      ! 0 |  3740 | `	}` |
|        - |  3741 | `	/* Statement successfully compiled */` |
|    31063 |  3742 | `	return SXRET_OK;` |
|    15534 |  3743 | `}` |
|        - |  3744 | `/*` |
|        - |  3745 | ` * Compile the 'break' statement.` |
|        - |  3746 | ` * According to the PHP language reference` |
|        - |  3747 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|        - |  3748 | ` *  structure.` |
|        - |  3749 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|        - |  3750 | ` *  enclosing structures are to be broken out of.` |
|        - |  3751 | ` */` |
|    38890 |  3752 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|        5 |  3753 | `{` |
|        - |  3754 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  3755 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  3756 | `	sxi32 rc;` |
|    38895 |  3757 | `	iLevel = 0;` |
|        - |  3758 | `	/* Jump the 'break' keyword */` |
|    38895 |  3759 | `	pGen->pIn++;` |
|    38895 |  3760 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|        - |  3761 | `		/* optional numeric argument which tells us how many levels` |
|        - |  3762 | `		 * of enclosing loops we should skip to the end of.` |
|        - |  3763 | `		 */` |
|        - |  3764 | `		char zScratch[GEN_NUM_SCRATCH];` |
|       18 |  3765 | `		char *zAlloc = 0;` |
|        - |  3766 | `		SyString sNum;` |
|       18 |  3767 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|       18 |  3768 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3769 | `			return SXERR_ABORT;` |
|        - |  3770 | `		}` |
|       18 |  3771 | `		if( rc == SXRET_OK ){` |
|       21 |  3772 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       12 |  3773 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|       15 |  3774 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3775 | `				return SXERR_ABORT;` |
|        - |  3776 | `			}` |
|       15 |  3777 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       15 |  3778 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        6 |  3779 | `		}` |
|       18 |  3780 | `		if( iLevel < 2 ){` |
|        3 |  3781 | `			iLevel = 0;` |
|        1 |  3782 | `		}` |
|       18 |  3783 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|        7 |  3784 | `	}` |
|        - |  3785 | `	/* Extract the target loop */` |
|    38895 |  3786 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    38895 |  3787 | `	if( pLoop == 0 ){` |
|        - |  3788 | `		/* Illegal break */` |
|       18 |  3789 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|       18 |  3790 | `		if( rc == SXERR_ABORT ){` |
|        - |  3791 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3792 | `			return SXERR_ABORT;` |
|        - |  3793 | `		}` |
|       10 |  3794 | `	}else{` |
|        - |  3795 | `		sxu32 nInstrIdx;` |
|        - |  3796 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    38879 |  3797 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  3798 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|    38879 |  3799 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|    38879 |  3800 | `		if( rc == SXRET_OK ){` |
|        - |  3801 | `			/* Fix the jump later when the jump destination is resolved */` |
|    38879 |  3802 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|    19437 |  3803 | `		}` |
|        - |  3804 | `	}` |
|    38895 |  3805 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  3806 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  3807 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|      ! 0 |  3808 | `	}` |
|        - |  3809 | `	/* Statement successfully compiled */` |
|    38895 |  3810 | `	return SXRET_OK;` |
|    19450 |  3811 | `}` |
|        - |  3812 | `/*` |
|        - |  3813 | ` * Compile or record a label.` |
|        - |  3814 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|        - |  3815 | ` * Example` |
|        - |  3816 | ` *  goto LABEL;` |
|        - |  3817 | ` *   echo 'Foo';` |
|        - |  3818 | ` *  LABEL:` |
|        - |  3819 | ` *   echo 'Bar';` |
|        - |  3820 | ` */` |
|      112 |  3821 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|        5 |  3822 | `{` |
|        - |  3823 | `	GenBlock *pBlock;` |
|        - |  3824 | `	Label sLabel;` |
|        - |  3825 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|      117 |  3826 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|      117 |  3827 | `	if( pBlock ){` |
|        - |  3828 | `		sxi32 rc;` |
|        8 |  3829 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        4 |  3830 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|        6 |  3831 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3832 | `			return SXERR_ABORT;` |
|        - |  3833 | `		}` |
|        4 |  3834 | `	}else{` |
|      113 |  3835 | `		SyString *pTarget = &pGen->pIn->sData;` |
|        - |  3836 | `		char *zDup;` |
|        - |  3837 | `		/* Initialize label fields */` |
|      113 |  3838 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|        - |  3839 | `		/* Duplicate label name */` |
|      113 |  3840 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      113 |  3841 | `		if( zDup == 0 ){` |
|      ! 0 |  3842 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  3843 | `			return SXERR_ABORT;` |
|        - |  3844 | `		}` |
|      113 |  3845 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|      113 |  3846 | `		sLabel.bRef  = FALSE;` |
|      113 |  3847 | `		sLabel.nLine = pGen->pIn->nLine;` |
|      113 |  3848 | `		pBlock = pGen->pCurrent;` |
|      221 |  3849 | `		while( pBlock ){` |
|      133 |  3850 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|       24 |  3851 | `				break;` |
|        - |  3852 | `			}` |
|        - |  3853 | `			/* Point to the upper block */` |
|      113 |  3854 | `			pBlock = pBlock->pParent;` |
|        5 |  3855 | `		}` |
|      113 |  3856 | `		if( pBlock ){` |
|       24 |  3857 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       14 |  3858 | `		}else{` |
|       93 |  3859 | `			sLabel.pFunc = 0;` |
|        - |  3860 | `		}` |
|        - |  3861 | `		/* Insert in label set */` |
|      113 |  3862 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|        - |  3863 | `	}` |
|      117 |  3864 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|      117 |  3865 | `	return SXRET_OK;` |
|       61 |  3866 | `}` |
|        - |  3867 | `/*` |
|        - |  3868 | ` * Compile the so hated 'goto' statement.` |
|        - |  3869 | ` * You've probably been taught that gotos are bad, but this sort` |
|        - |  3870 | ` * of rewriting  happens all the time, in fact every time you run` |
|        - |  3871 | ` * a compiler it has to do this.` |
|        - |  3872 | ` * According to the PHP language reference manual` |
|        - |  3873 | ` *   The goto operator can be used to jump to another section in the program.` |
|        - |  3874 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|        - |  3875 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|        - |  3876 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|        - |  3877 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|        - |  3878 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|        - |  3879 | ` *   of a multi-level break` |
|        - |  3880 | ` */` |
|      152 |  3881 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|        5 |  3882 | `{` |
|        - |  3883 | `	JumpFixup sJump;` |
|        - |  3884 | `	sxi32 rc;` |
|      157 |  3885 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|      157 |  3886 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  3887 | `		/* Missing label */` |
|      ! 0 |  3888 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|      ! 0 |  3889 | `		if( rc == SXERR_ABORT ){` |
|        - |  3890 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3891 | `			return SXERR_ABORT;` |
|        - |  3892 | `		}` |
|      ! 0 |  3893 | `		return SXRET_OK;` |
|        - |  3894 | `	}` |
|      157 |  3895 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        5 |  3896 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|        5 |  3897 | `		if( rc == SXERR_ABORT ){` |
|        - |  3898 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3899 | `			return SXERR_ABORT;` |
|        - |  3900 | `		}` |
|        3 |  3901 | `	}else{` |
|      153 |  3902 | `		SyString *pTarget = &pGen->pIn->sData;` |
|        - |  3903 | `		GenBlock *pBlock;` |
|        - |  3904 | `		char *zDup;` |
|        - |  3905 | `		/* Prepare the jump destination */` |
|      153 |  3906 | `		sJump.nJumpType = PH7_OP_JMP;` |
|      153 |  3907 | `		sJump.nLine = pGen->pIn->nLine;` |
|        - |  3908 | `		/* Duplicate label name */` |
|      153 |  3909 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      153 |  3910 | `		if( zDup == 0 ){` |
|      ! 0 |  3911 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  3912 | `			return SXERR_ABORT;` |
|        - |  3913 | `		}` |
|      153 |  3914 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|      153 |  3915 | `		pBlock = pGen->pCurrent;` |
|      315 |  3916 | `		while( pBlock ){` |
|      199 |  3917 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|       36 |  3918 | `				break;` |
|        - |  3919 | `			}` |
|        - |  3920 | `			/* Point to the upper block */` |
|      167 |  3921 | `			pBlock = pBlock->pParent;` |
|        5 |  3922 | `		}` |
|      153 |  3923 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|        8 |  3924 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|        8 |  3925 | `			if( rc == SXERR_ABORT ){` |
|        - |  3926 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  3927 | `				return SXERR_ABORT;` |
|        - |  3928 | `			}` |
|        3 |  3929 | `		}` |
|      153 |  3930 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|       29 |  3931 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       16 |  3932 | `		}else{` |
|      127 |  3933 | `			sJump.pFunc = 0;` |
|        - |  3934 | `		}` |
|        - |  3935 | `		/* Emit the unconditional jump */` |
|      153 |  3936 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|      153 |  3937 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|       74 |  3938 | `		}` |
|        - |  3939 | `	}` |
|      157 |  3940 | `	pGen->pIn++; /* Jump the label name */` |
|      157 |  3941 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        3 |  3942 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|        1 |  3943 | `	}` |
|        - |  3944 | `	/* Statement successfully compiled */` |
|      157 |  3945 | `	return SXRET_OK;` |
|       81 |  3946 | `}` |
|        - |  3947 | `/*` |
|        - |  3948 | ` * Point to the next PHP chunk that will be processed shortly.` |
|        - |  3949 | ` * Return SXRET_OK on success. Any other return value indicates` |
|        - |  3950 | ` * failure.` |
|        - |  3951 | ` */` |
|       20 |  3952 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|        2 |  3953 | `{` |
|        - |  3954 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|        - |  3955 | `	sxu32 nRawObj;` |
|       10 |  3956 | `	sxu32 nObjIdx;` |
|        - |  3957 | `	/* Consume raw chunks verbatim without any processing until we get` |
|        - |  3958 | `	 * a PHP block.` |
|        - |  3959 | `	 */` |
|       10 |  3960 | `Consume:` |
|       22 |  3961 | `	nRawObj = nObjIdx = 0;` |
|       22 |  3962 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|      ! 0 |  3963 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|      ! 0 |  3964 | `		if( pRawObj == 0 ){` |
|      ! 0 |  3965 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3966 | `			return SXERR_ABORT;` |
|        - |  3967 | `		}` |
|        - |  3968 | `		/* Mark as constant and emit the load constant instruction */` |
|      ! 0 |  3969 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|      ! 0 |  3970 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|      ! 0 |  3971 | `		++nRawObj;` |
|      ! 0 |  3972 | `		pGen->pRawIn++; /* Next chunk */` |
|      ! 0 |  3973 | `	}` |
|       22 |  3974 | `	if( nRawObj > 0 ){` |
|        - |  3975 | `		/* Emit the consume instruction */` |
|      ! 0 |  3976 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      ! 0 |  3977 | `	}` |
|       22 |  3978 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|      ! 0 |  3979 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|        - |  3980 | `		/* Reset the token set (and its trivia sidecar) */` |
|      ! 0 |  3981 | `		SySetReset(pTokenSet);` |
|      ! 0 |  3982 | `		SySetReset(&pGen->aTrivia);` |
|        - |  3983 | `		/* Tokenize input */` |
|      ! 0 |  3984 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|      ! 0 |  3985 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|        - |  3986 | `		/* Point to the fresh token stream */` |
|      ! 0 |  3987 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|      ! 0 |  3988 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|        - |  3989 | `		/* Advance the stream cursor */` |
|      ! 0 |  3990 | `		pGen->pRawIn++;` |
|        - |  3991 | `		/* TICKET 1433-011 */` |
|      ! 0 |  3992 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|        - |  3993 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|        - |  3994 | `			sxi32 rc;` |
|        - |  3995 | `			/* Refer to TICKET 1433-009  */` |
|      ! 0 |  3996 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|      ! 0 |  3997 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|      ! 0 |  3998 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|      ! 0 |  3999 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|      ! 0 |  4000 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4001 | `				return SXERR_ABORT;` |
|      ! 0 |  4002 | `			}else if( rc != SXERR_EMPTY ){` |
|      ! 0 |  4003 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 |  4004 | `			}` |
|      ! 0 |  4005 | `			goto Consume;` |
|        - |  4006 | `		}` |
|      ! 0 |  4007 | `	}else{` |
|        - |  4008 | `		/* No more chunks to process */` |
|       22 |  4009 | `		pGen->pIn = pGen->pEnd;` |
|       22 |  4010 | `		return SXERR_EOF;` |
|        - |  4011 | `	}` |
|      ! 0 |  4012 | `	return SXRET_OK;` |
|       12 |  4013 | `}` |
|        - |  4014 | `/*` |
|        - |  4015 | ` * Compile a PHP block.` |
|        - |  4016 | ` * A block is simply one or more PHP statements and expressions to compile` |
|        - |  4017 | ` * optionally delimited by braces {}.` |
|        - |  4018 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|        - |  4019 | ` * and this function takes care of generating the appropriate error` |
|        - |  4020 | ` * message.` |
|        - |  4021 | ` */` |
|  4049938 |  4022 | `static sxi32 PH7_CompileBlock(` |
|        - |  4023 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  4024 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|        - |  4025 | `	)` |
|        5 |  4026 | `{` |
|        - |  4027 | `	sxi32 rc;` |
|        - |  4028 | `	sxu32 nLine;` |
|  4049943 |  4029 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  4048491 |  4030 | `		nLine = pGen->pIn->nLine;` |
|  4048491 |  4031 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  4048491 |  4032 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  4033 | `			return SXERR_ABORT;` |
|        - |  4034 | `		}` |
|  4048491 |  4035 | `		pGen->pIn++;` |
|        - |  4036 | `		/* Compile until we hit the closing braces '}' */` |
|  5915804 |  4037 | `		for(;;){` |
| 11831613 |  4038 | `			if( pGen->pIn >= pGen->pEnd ){` |
|       22 |  4039 | `				rc = GenStateNextChunk(&(*pGen));` |
|       22 |  4040 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  4041 | `			 	   return SXERR_ABORT;` |
|        - |  4042 | `				}` |
|       22 |  4043 | `				if( rc == SXERR_EOF ){` |
|        - |  4044 | `					/* No more token to process. Missing closing braces */` |
|       22 |  4045 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|       22 |  4046 | `					break;` |
|        - |  4047 | `				}` |
|      ! 0 |  4048 | `			}` |
| 11831593 |  4049 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|        - |  4050 | `				/* Closing braces found,break immediately*/` |
|  4048471 |  4051 | `				pGen->pIn++;` |
|  4048471 |  4052 | `				break;` |
|        - |  4053 | `			}` |
|        - |  4054 | `			/* Compile a single statement */` |
|  7783127 |  4055 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  7783127 |  4056 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4057 | `				return SXERR_ABORT;` |
|        - |  4058 | `			}` |
|        5 |  4059 | `		}` |
|  4048491 |  4060 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  2025700 |  4061 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|      ! 0 |  4062 | `		pGen->pIn++;` |
|      ! 0 |  4063 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|      ! 0 |  4064 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  4065 | `			return SXERR_ABORT;` |
|        - |  4066 | `		}` |
|        - |  4067 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|      ! 0 |  4068 | `		for(;;){` |
|      ! 0 |  4069 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  4070 | `				rc = GenStateNextChunk(&(*pGen));` |
|      ! 0 |  4071 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  4072 | `			 	   return SXERR_ABORT;` |
|        - |  4073 | `				}` |
|      ! 0 |  4074 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|        - |  4075 | `					/* No more token to process */` |
|      ! 0 |  4076 | `					if( rc == SXERR_EOF ){` |
|      ! 0 |  4077 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|        - |  4078 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|      ! 0 |  4079 | `					}` |
|      ! 0 |  4080 | `					break;` |
|        - |  4081 | `				}` |
|      ! 0 |  4082 | `			}` |
|      ! 0 |  4083 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        - |  4084 | `				sxi32 nKwrd;` |
|        - |  4085 | `				/* Keyword found */` |
|      ! 0 |  4086 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      ! 0 |  4087 | `				if( nKwrd == nKeywordEnd \|\|` |
|      ! 0 |  4088 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|        - |  4089 | `						/* Delimiter keyword found,break */` |
|      ! 0 |  4090 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|      ! 0 |  4091 | `							pGen->pIn++; /*  endif;endswitch... */` |
|      ! 0 |  4092 | `						}` |
|      ! 0 |  4093 | `						break;` |
|        - |  4094 | `				}` |
|      ! 0 |  4095 | `			}` |
|        - |  4096 | `			/* Compile a single statement */` |
|      ! 0 |  4097 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|      ! 0 |  4098 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4099 | `				return SXERR_ABORT;` |
|        - |  4100 | `			}` |
|      ! 0 |  4101 | `		}` |
|      ! 0 |  4102 | `		GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  4103 | `	}else{` |
|        - |  4104 | `		/* Compile a single statement */` |
|     1457 |  4105 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     1457 |  4106 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4107 | `			return SXERR_ABORT;` |
|        - |  4108 | `		}` |
|        - |  4109 | `	}` |
|        - |  4110 | `	/* Jump trailing semi-colons ';' */` |
|  4049943 |  4111 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      ! 0 |  4112 | `		pGen->pIn++;` |
|      ! 0 |  4113 | `	}` |
|  4049943 |  4114 | `	return SXRET_OK;` |
|  2024974 |  4115 | `}` |
|        - |  4116 | `/*` |
|        - |  4117 | ` * Compile the gentle 'while' statement.` |
|        - |  4118 | ` * According to the PHP language reference` |
|        - |  4119 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|        - |  4120 | ` *  The basic form of a while statement is:` |
|        - |  4121 | ` *  while (expr)` |
|        - |  4122 | ` *   statement` |
|        - |  4123 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|        - |  4124 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|        - |  4125 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|        - |  4126 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|        - |  4127 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|        - |  4128 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|        - |  4129 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|        - |  4130 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|        - |  4131 | ` *  while (expr):` |
|        - |  4132 | ` *    statement` |
|        - |  4133 | ` *   endwhile;` |
|        - |  4134 | ` */` |
|    35020 |  4135 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|        5 |  4136 | `{` |
|    35025 |  4137 | `	GenBlock *pWhileBlock = 0;` |
|    35025 |  4138 | `	SyToken *pTmp,*pEnd = 0;` |
|        - |  4139 | `	sxu32 nFalseJump;` |
|        - |  4140 | `	sxu32 nLine;` |
|        - |  4141 | `	sxi32 rc;` |
|    35025 |  4142 | `	nLine = pGen->pIn->nLine;` |
|        - |  4143 | `	/* Jump the 'while' keyword */` |
|    35025 |  4144 | `	pGen->pIn++;` |
|    35025 |  4145 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4146 | `		/* Syntax error */` |
|      ! 0 |  4147 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  4148 | `		if( rc == SXERR_ABORT ){` |
|        - |  4149 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4150 | `			return SXERR_ABORT;` |
|        - |  4151 | `		}` |
|      ! 0 |  4152 | `		goto Synchronize;` |
|        - |  4153 | `	}` |
|        - |  4154 | `	/* Jump the left parenthesis '(' */` |
|    35025 |  4155 | `	pGen->pIn++;` |
|        - |  4156 | `	/* Create the loop block */` |
|    35025 |  4157 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|    35025 |  4158 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4159 | `		return SXERR_ABORT;` |
|        - |  4160 | `	}` |
|        - |  4161 | `	/* Delimit the condition */` |
|    35025 |  4162 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    35025 |  4163 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  4164 | `		/* Empty expression */` |
|        3 |  4165 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|        3 |  4166 | `		if( rc == SXERR_ABORT ){` |
|        - |  4167 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4168 | `			return SXERR_ABORT;` |
|        - |  4169 | `		}` |
|        1 |  4170 | `	}` |
|        - |  4171 | `	/* Swap token streams */` |
|    35025 |  4172 | `	pTmp = pGen->pEnd;` |
|    35025 |  4173 | `	pGen->pEnd = pEnd;` |
|        - |  4174 | `	/* Compile the expression */` |
|    35025 |  4175 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    35025 |  4176 | `	if( rc == SXERR_ABORT ){` |
|        - |  4177 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4178 | `		return SXERR_ABORT;` |
|        - |  4179 | `	}` |
|        - |  4180 | `	/* Update token stream */` |
|    35025 |  4181 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  4182 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4183 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4184 | `			return SXERR_ABORT;` |
|        - |  4185 | `		}` |
|      ! 0 |  4186 | `		pGen->pIn++;` |
|      ! 0 |  4187 | `	}` |
|        - |  4188 | `	/* Synchronize pointers */` |
|    35025 |  4189 | `	pGen->pIn  = &pEnd[1];` |
|    35025 |  4190 | `	pGen->pEnd = pTmp;` |
|        - |  4191 | `	/* Emit the false jump */` |
|    35025 |  4192 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - |  4193 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    35025 |  4194 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|        - |  4195 | `	/* Compile the loop body */` |
|    35025 |  4196 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|    35025 |  4197 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  4198 | `		return SXERR_ABORT;` |
|        - |  4199 | `	}` |
|        - |  4200 | `	/* Emit the unconditional jump to the start of the loop */` |
|    35025 |  4201 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|        - |  4202 | `	/* Fix all jumps now the destination is resolved */` |
|    35025 |  4203 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4204 | `	/* Release the loop block */` |
|    35025 |  4205 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4206 | `	/* Statement successfully compiled */` |
|    35025 |  4207 | `	return SXRET_OK;` |
|      ! 0 |  4208 | `Synchronize:` |
|        - |  4209 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  4210 | `	 * compiling this erroneous block.` |
|        - |  4211 | `	 */` |
|      ! 0 |  4212 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  4213 | `		pGen->pIn++;` |
|      ! 0 |  4214 | `	}` |
|      ! 0 |  4215 | `	return SXRET_OK;` |
|    17515 |  4216 | `}` |
|        - |  4217 | `/*` |
|        - |  4218 | ` * Compile the ugly do..while() statement.` |
|        - |  4219 | ` * According to the PHP language reference` |
|        - |  4220 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|        - |  4221 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|        - |  4222 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|        - |  4223 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|        - |  4224 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|        - |  4225 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|        - |  4226 | ` *  would end immediately).` |
|        - |  4227 | ` *  There is just one syntax for do-while loops:` |
|        - |  4228 | ` *  <?php` |
|        - |  4229 | ` *  $i = 0;` |
|        - |  4230 | ` *  do {` |
|        - |  4231 | ` *   echo $i;` |
|        - |  4232 | ` *  } while ($i > 0);` |
|        - |  4233 | ` * ?>` |
|        - |  4234 | ` */` |
|        2 |  4235 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|        1 |  4236 | `{` |
|        3 |  4237 | `	SyToken *pTmp,*pEnd = 0;` |
|        3 |  4238 | `	GenBlock *pDoBlock = 0;` |
|        - |  4239 | `	sxu32 nLine;` |
|        - |  4240 | `	sxi32 rc;` |
|        3 |  4241 | `	nLine = pGen->pIn->nLine;` |
|        - |  4242 | `	/* Jump the 'do' keyword */` |
|        3 |  4243 | `	pGen->pIn++;` |
|        - |  4244 | `	/* Create the loop block */` |
|        3 |  4245 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|        3 |  4246 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4247 | `		return SXERR_ABORT;` |
|        - |  4248 | `	}` |
|        - |  4249 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|        3 |  4250 | `	pDoBlock->bPostContinue = TRUE;` |
|        3 |  4251 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|        3 |  4252 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  4253 | `		return SXERR_ABORT;` |
|        - |  4254 | `	}` |
|        3 |  4255 | `	if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  4256 | `		nLine = pGen->pIn->nLine;` |
|      ! 0 |  4257 | `	}` |
|        3 |  4258 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|      ! 0 |  4259 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|        - |  4260 | `			/* Missing 'while' statement */` |
|        3 |  4261 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|        3 |  4262 | `			if( rc == SXERR_ABORT ){` |
|        - |  4263 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4264 | `				return SXERR_ABORT;` |
|        - |  4265 | `			}` |
|        3 |  4266 | `			goto Synchronize;` |
|        - |  4267 | `	}` |
|        - |  4268 | `	/* Jump the 'while' keyword */` |
|      ! 0 |  4269 | `	pGen->pIn++;` |
|      ! 0 |  4270 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4271 | `		/* Syntax error */` |
|      ! 0 |  4272 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  4273 | `		if( rc == SXERR_ABORT ){` |
|        - |  4274 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4275 | `			return SXERR_ABORT;` |
|        - |  4276 | `		}` |
|      ! 0 |  4277 | `		goto Synchronize;` |
|        - |  4278 | `	}` |
|        - |  4279 | `	/* Jump the left parenthesis '(' */` |
|      ! 0 |  4280 | `	pGen->pIn++;` |
|        - |  4281 | `	/* Delimit the condition */` |
|      ! 0 |  4282 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      ! 0 |  4283 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  4284 | `		/* Empty expression */` |
|      ! 0 |  4285 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|      ! 0 |  4286 | `		if( rc == SXERR_ABORT ){` |
|        - |  4287 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4288 | `			return SXERR_ABORT;` |
|        - |  4289 | `		}` |
|      ! 0 |  4290 | `		goto Synchronize;` |
|        - |  4291 | `	}` |
|        - |  4292 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|      ! 0 |  4293 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|        - |  4294 | `		JumpFixup *aPost;` |
|        - |  4295 | `		VmInstr *pInstr;` |
|        - |  4296 | `		sxu32 nJumpDest;` |
|        - |  4297 | `		sxu32 n;` |
|      ! 0 |  4298 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|      ! 0 |  4299 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      ! 0 |  4300 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|      ! 0 |  4301 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      ! 0 |  4302 | `			if( pInstr ){` |
|        - |  4303 | `				/* Fix */` |
|      ! 0 |  4304 | `				pInstr->iP2 = nJumpDest;` |
|      ! 0 |  4305 | `			}` |
|      ! 0 |  4306 | `		}` |
|      ! 0 |  4307 | `	}` |
|        - |  4308 | `	/* Swap token streams */` |
|      ! 0 |  4309 | `	pTmp = pGen->pEnd;` |
|      ! 0 |  4310 | `	pGen->pEnd = pEnd;` |
|        - |  4311 | `	/* Compile the expression */` |
|      ! 0 |  4312 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      ! 0 |  4313 | `	if( rc == SXERR_ABORT ){` |
|        - |  4314 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4315 | `		return SXERR_ABORT;` |
|        - |  4316 | `	}` |
|        - |  4317 | `	/* Update token stream */` |
|      ! 0 |  4318 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  4319 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4320 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4321 | `			return SXERR_ABORT;` |
|        - |  4322 | `		}` |
|      ! 0 |  4323 | `		pGen->pIn++;` |
|      ! 0 |  4324 | `	}` |
|      ! 0 |  4325 | `	pGen->pIn  = &pEnd[1];` |
|      ! 0 |  4326 | `	pGen->pEnd = pTmp;` |
|        - |  4327 | `	/* Emit the true jump to the beginning of the loop */` |
|      ! 0 |  4328 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|        - |  4329 | `	/* Fix all jumps now the destination is resolved */` |
|      ! 0 |  4330 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4331 | `	/* Release the loop block */` |
|      ! 0 |  4332 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4333 | `	/* Statement successfully compiled */` |
|      ! 0 |  4334 | `	return SXRET_OK;` |
|        1 |  4335 | `Synchronize:` |
|        - |  4336 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  4337 | `	 * compiling this erroneous block.` |
|        - |  4338 | `	 */` |
|        3 |  4339 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  4340 | `		pGen->pIn++;` |
|      ! 0 |  4341 | `	}` |
|        3 |  4342 | `	return SXRET_OK;` |
|        2 |  4343 | `}` |
|        - |  4344 | `/*` |
|        - |  4345 | ` * Compile the complex and powerful 'for' statement.` |
|        - |  4346 | ` * According to the PHP language reference` |
|        - |  4347 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|        - |  4348 | ` *  The syntax of a for loop is:` |
|        - |  4349 | ` *  for (expr1; expr2; expr3)` |
|        - |  4350 | ` *   statement` |
|        - |  4351 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|        - |  4352 | ` *  the beginning of the loop.` |
|        - |  4353 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|        - |  4354 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|        - |  4355 | ` *  to FALSE, the execution of the loop ends.` |
|        - |  4356 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|        - |  4357 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|        - |  4358 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|        - |  4359 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|        - |  4360 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|        - |  4361 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|        - |  4362 | ` *  of using the for truth expression.` |
|        - |  4363 | ` */` |
|    66032 |  4364 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|        5 |  4365 | `{` |
|    66037 |  4366 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|    66037 |  4367 | `	GenBlock *pForBlock = 0;` |
|        - |  4368 | `	sxu32 nFalseJump;` |
|        - |  4369 | `	sxu32 nLine;` |
|        - |  4370 | `	sxi32 rc;` |
|    66037 |  4371 | `	nLine = pGen->pIn->nLine;` |
|        - |  4372 | `	/* Jump the 'for' keyword */` |
|    66037 |  4373 | `	pGen->pIn++;` |
|    66037 |  4374 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4375 | `		/* Syntax error */` |
|      ! 0 |  4376 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|      ! 0 |  4377 | `		if( rc == SXERR_ABORT ){` |
|        - |  4378 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4379 | `			return SXERR_ABORT;` |
|        - |  4380 | `		}` |
|      ! 0 |  4381 | `		return SXRET_OK;` |
|        - |  4382 | `	}` |
|        - |  4383 | `	/* Jump the left parenthesis '(' */` |
|    66037 |  4384 | `	pGen->pIn++;` |
|        - |  4385 | `	/* Delimit the init-expr;condition;post-expr */` |
|    66037 |  4386 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    66037 |  4387 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  4388 | `		/* Empty expression */` |
|      ! 0 |  4389 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|      ! 0 |  4390 | `		if( rc == SXERR_ABORT ){` |
|        - |  4391 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4392 | `			return SXERR_ABORT;` |
|        - |  4393 | `		}` |
|        - |  4394 | `		/* Synchronize */` |
|      ! 0 |  4395 | `		pGen->pIn = pEnd;` |
|      ! 0 |  4396 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  4397 | `			pGen->pIn++;` |
|      ! 0 |  4398 | `		}` |
|      ! 0 |  4399 | `		return SXRET_OK;` |
|        - |  4400 | `	}` |
|        - |  4401 | `	/* Swap token streams */` |
|    66037 |  4402 | `	pTmp = pGen->pEnd;` |
|    66037 |  4403 | `	pGen->pEnd = pEnd;` |
|        - |  4404 | `	/* Compile initialization expressions if available */` |
|    66037 |  4405 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  4406 | `	/* Pop operand lvalues */` |
|    66037 |  4407 | `	if( rc == SXERR_ABORT ){` |
|        - |  4408 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4409 | `		return SXERR_ABORT;` |
|    66037 |  4410 | `	}else if( rc != SXERR_EMPTY ){` |
|    66035 |  4411 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    33015 |  4412 | `	}` |
|    66037 |  4413 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  4414 | `		/* Syntax error */` |
|      ! 0 |  4415 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  4416 | `			"for: Expected ';' after initialization expressions");` |
|      ! 0 |  4417 | `		if( rc == SXERR_ABORT ){` |
|        - |  4418 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4419 | `			return SXERR_ABORT;` |
|        - |  4420 | `		}` |
|      ! 0 |  4421 | `		return SXRET_OK;` |
|        - |  4422 | `	}` |
|        - |  4423 | `	/* Jump the trailing ';' */` |
|    66037 |  4424 | `	pGen->pIn++;` |
|        - |  4425 | `	/* Create the loop block */` |
|    66037 |  4426 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|    66037 |  4427 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4428 | `		return SXERR_ABORT;` |
|        - |  4429 | `	}` |
|        - |  4430 | `	/* Deffer continue jumps */` |
|    66037 |  4431 | `	pForBlock->bPostContinue = TRUE;` |
|        - |  4432 | `	/* Compile the condition */` |
|    66037 |  4433 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    66037 |  4434 | `	if( rc == SXERR_ABORT ){` |
|        - |  4435 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4436 | `		return SXERR_ABORT;` |
|    66037 |  4437 | `	}else if( rc != SXERR_EMPTY ){` |
|        - |  4438 | `		/* Emit the false jump */` |
|    66035 |  4439 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - |  4440 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    66035 |  4441 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    33015 |  4442 | `	}` |
|    66037 |  4443 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  4444 | `		/* Syntax error */` |
|        6 |  4445 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  4446 | `			"for: Expected ';' after conditionals expressions");` |
|        6 |  4447 | `		if( rc == SXERR_ABORT ){` |
|        - |  4448 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4449 | `			return SXERR_ABORT;` |
|        - |  4450 | `		}` |
|        6 |  4451 | `		return SXRET_OK;` |
|        - |  4452 | `	}` |
|        - |  4453 | `	/* Jump the trailing ';' */` |
|    66033 |  4454 | `	pGen->pIn++;` |
|        - |  4455 | `	/* Save the post condition stream */` |
|    66033 |  4456 | `	pPostStart = pGen->pIn;` |
|        - |  4457 | `	/* Compile the loop body */` |
|    66033 |  4458 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|    66033 |  4459 | `	pGen->pEnd = pTmp;` |
|    66033 |  4460 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|    66033 |  4461 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  4462 | `		return SXERR_ABORT;` |
|        - |  4463 | `	}` |
|        - |  4464 | `	/* Fix post-continue jumps */` |
|    66033 |  4465 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|        - |  4466 | `		JumpFixup *aPost;` |
|        - |  4467 | `		VmInstr *pInstr;` |
|        - |  4468 | `		sxu32 nJumpDest;` |
|        - |  4469 | `		sxu32 n;` |
|     3893 |  4470 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|     3893 |  4471 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     7781 |  4472 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|     3893 |  4473 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     3893 |  4474 | `			if( pInstr ){` |
|        - |  4475 | `				/* Fix jump */` |
|     3893 |  4476 | `				pInstr->iP2 = nJumpDest;` |
|     1944 |  4477 | `			}` |
|     1949 |  4478 | `		}` |
|     1944 |  4479 | `	}` |
|        - |  4480 | `	/* compile the post-expressions if available */` |
|    66033 |  4481 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|      ! 0 |  4482 | `		pPostStart++;` |
|      ! 0 |  4483 | `	}` |
|    66033 |  4484 | `	if( pPostStart < pEnd ){` |
|        - |  4485 | `		SyToken *pTmpIn,*pTmpEnd;` |
|    66033 |  4486 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|    66033 |  4487 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    66033 |  4488 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - |  4489 | `			/* Syntax error */` |
|      ! 0 |  4490 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|      ! 0 |  4491 | `			if( rc == SXERR_ABORT ){` |
|        - |  4492 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4493 | `				return SXERR_ABORT;` |
|        - |  4494 | `			}` |
|      ! 0 |  4495 | `			return SXRET_OK;` |
|        - |  4496 | `		}` |
|    66033 |  4497 | `		RE_SWAP_DELIMITER(pGen);` |
|    66033 |  4498 | `		if( rc == SXERR_ABORT ){` |
|        - |  4499 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4500 | `			return SXERR_ABORT;` |
|    66033 |  4501 | `		}else if( rc != SXERR_EMPTY){` |
|        - |  4502 | `			/* Pop operand lvalue */` |
|    66033 |  4503 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    33014 |  4504 | `		}` |
|    33014 |  4505 | `	}` |
|        - |  4506 | `	/* Emit the unconditional jump to the start of the loop */` |
|    66033 |  4507 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|        - |  4508 | `	/* Fix all jumps now the destination is resolved */` |
|    66033 |  4509 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4510 | `	/* Release the loop block */` |
|    66033 |  4511 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4512 | `	/* Statement successfully compiled */` |
|    66033 |  4513 | `	return SXRET_OK;` |
|    33021 |  4514 | `}` |
|        - |  4515 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|        - |  4516 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|        - |  4517 | ` * are allowed.` |
|        - |  4518 | ` */` |
|   241212 |  4519 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 |  4520 | `{` |
|   241217 |  4521 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|   241217 |  4522 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|        - |  4523 | `		/* Unexpected expression */` |
|      ! 0 |  4524 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - |  4525 | `			"foreach: Expecting a variable name");` |
|      ! 0 |  4526 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 |  4527 | `			rc = SXERR_INVALID;` |
|      ! 0 |  4528 | `		}` |
|      ! 0 |  4529 | `	}` |
|   241217 |  4530 | `	return rc;` |
|        5 |  4531 | `}` |
|        - |  4532 | `/*` |
|        - |  4533 | ` * Compile the 'foreach' statement.` |
|        - |  4534 | ` * According to the PHP language reference` |
|        - |  4535 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|        - |  4536 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|        - |  4537 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|        - |  4538 | ` *  is a minor but useful extension of the first:` |
|        - |  4539 | ` *  foreach (array_expression as $value)` |
|        - |  4540 | ` *    statement` |
|        - |  4541 | ` *  foreach (array_expression as $key => $value)` |
|        - |  4542 | ` *   statement` |
|        - |  4543 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|        - |  4544 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|        - |  4545 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|        - |  4546 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|        - |  4547 | ` *  to the variable $key on each loop.` |
|        - |  4548 | ` *  Note:` |
|        - |  4549 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|        - |  4550 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|        - |  4551 | ` *  Note:` |
|        - |  4552 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|        - |  4553 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|        - |  4554 | ` *  or after the foreach without resetting it.` |
|        - |  4555 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|        - |  4556 | ` *  of copying the value.` |
|        - |  4557 | ` */` |
|   175082 |  4558 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|        5 |  4559 | `{` |
|   175087 |  4560 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|   175087 |  4561 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|   175087 |  4562 | `	GenBlock *pForeachBlock = 0;` |
|        - |  4563 | `	ph7_foreach_info *pInfo;` |
|        - |  4564 | `	sxu32 nFalseJump;` |
|        - |  4565 | `	VmInstr *pInstr;` |
|        - |  4566 | `	sxu32 nLine;` |
|        - |  4567 | `	sxi32 rc;` |
|   175087 |  4568 | `	nLine = pGen->pIn->nLine;` |
|        - |  4569 | `	/* Jump the 'foreach' keyword */` |
|   175087 |  4570 | `	pGen->pIn++;` |
|   175087 |  4571 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4572 | `		/* Syntax error */` |
|      ! 0 |  4573 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|      ! 0 |  4574 | `		if( rc == SXERR_ABORT ){` |
|        - |  4575 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4576 | `			return SXERR_ABORT;` |
|        - |  4577 | `		}` |
|      ! 0 |  4578 | `		goto Synchronize;` |
|        - |  4579 | `	}` |
|        - |  4580 | `	/* Jump the left parenthesis '(' */` |
|   175087 |  4581 | `	pGen->pIn++;` |
|        - |  4582 | `	/* Create the loop block */` |
|   175087 |  4583 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|   175087 |  4584 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4585 | `		return SXERR_ABORT;` |
|        - |  4586 | `	}` |
|        - |  4587 | `	/* Delimit the expression */` |
|   175087 |  4588 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   175087 |  4589 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  4590 | `		/* Empty expression */` |
|      ! 0 |  4591 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|      ! 0 |  4592 | `		if( rc == SXERR_ABORT ){` |
|        - |  4593 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4594 | `			return SXERR_ABORT;` |
|        - |  4595 | `		}` |
|        - |  4596 | `		/* Synchronize */` |
|      ! 0 |  4597 | `		pGen->pIn = pEnd;` |
|      ! 0 |  4598 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  4599 | `			pGen->pIn++;` |
|      ! 0 |  4600 | `		}` |
|      ! 0 |  4601 | `		return SXRET_OK;` |
|        - |  4602 | `	}` |
|        - |  4603 | `	/* Compile the array expression */` |
|   175087 |  4604 | `	pCur = pGen->pIn;` |
|  1023145 |  4605 | `	while( pCur < pEnd ){` |
|  1023145 |  4606 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|   178977 |  4607 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|   178977 |  4608 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|        - |  4609 | `				/* Break with the first 'as' found */` |
|   175087 |  4610 | `				break;` |
|        - |  4611 | `			}` |
|     1945 |  4612 | `		}` |
|        - |  4613 | `		/* Advance the stream cursor */` |
|   848063 |  4614 | `		pCur++;` |
|        5 |  4615 | `	}` |
|   175087 |  4616 | `	if( pCur <= pGen->pIn ){` |
|      ! 0 |  4617 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  4618 | `			"foreach: Missing array/object expression");` |
|      ! 0 |  4619 | `		if( rc == SXERR_ABORT ){` |
|        - |  4620 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4621 | `			return SXERR_ABORT;` |
|        - |  4622 | `		}` |
|      ! 0 |  4623 | `		goto Synchronize;` |
|        - |  4624 | `	}` |
|        - |  4625 | `	/* Swap token streams */` |
|   175087 |  4626 | `	pTmp = pGen->pEnd;` |
|   175087 |  4627 | `	pGen->pEnd = pCur;` |
|   175087 |  4628 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   175087 |  4629 | `	if( rc == SXERR_ABORT ){` |
|        - |  4630 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4631 | `		return SXERR_ABORT;` |
|        - |  4632 | `	}` |
|        - |  4633 | `	/* Update token stream */` |
|   175087 |  4634 | `	while(pGen->pIn < pCur ){` |
|      ! 0 |  4635 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4636 | `		if( rc == SXERR_ABORT ){` |
|        - |  4637 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4638 | `			return SXERR_ABORT;` |
|        - |  4639 | `		}` |
|      ! 0 |  4640 | `		pGen->pIn++;` |
|      ! 0 |  4641 | `	}` |
|   175087 |  4642 | `	pCur++; /* Jump the 'as' keyword */` |
|   175087 |  4643 | `	pGen->pIn = pCur;` |
|   175087 |  4644 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 |  4645 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|      ! 0 |  4646 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4647 | `			return SXERR_ABORT;` |
|        - |  4648 | `		}` |
|      ! 0 |  4649 | `	}` |
|        - |  4650 | `	/* Create the foreach context */` |
|   175087 |  4651 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|   175087 |  4652 | `	if( pInfo == 0 ){` |
|      ! 0 |  4653 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 |  4654 | `		return SXERR_ABORT;` |
|        - |  4655 | `	}` |
|        - |  4656 | `	/* Zero the structure */` |
|   175087 |  4657 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|        - |  4658 | `	/* Initialize structure fields */` |
|   175087 |  4659 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|        - |  4660 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|        - |  4661 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|        - |  4662 | `	 * '=>'. */` |
|   175087 |  4663 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|   175087 |  4664 | `	if( pCur < pEnd ){` |
|        - |  4665 | `		/* Compile the expression holding the key name */` |
|    66155 |  4666 | `		if( pGen->pIn >= pCur ){` |
|      ! 0 |  4667 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|      ! 0 |  4668 | `			if( rc == SXERR_ABORT ){` |
|        - |  4669 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4670 | `				return SXERR_ABORT;` |
|        - |  4671 | `			}` |
|      ! 0 |  4672 | `		}else{` |
|    66155 |  4673 | `			pGen->pEnd = pCur;` |
|    66155 |  4674 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    66155 |  4675 | `			if( rc == SXERR_ABORT ){` |
|        - |  4676 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4677 | `				return SXERR_ABORT;` |
|        - |  4678 | `			}` |
|    66155 |  4679 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    66155 |  4680 | `			if( pInstr->p3 ){` |
|        - |  4681 | `				/* Record key name */` |
|    66155 |  4682 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    33075 |  4683 | `			}` |
|    66155 |  4684 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|        - |  4685 | `		}` |
|    66155 |  4686 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    33075 |  4687 | `	}` |
|   175087 |  4688 | `	pGen->pEnd = pEnd;` |
|   175087 |  4689 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 |  4690 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|      ! 0 |  4691 | `		if( rc == SXERR_ABORT ){` |
|        - |  4692 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4693 | `			return SXERR_ABORT;` |
|        - |  4694 | `		}` |
|      ! 0 |  4695 | `		goto Synchronize;` |
|        - |  4696 | `	}` |
|   175087 |  4697 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|       33 |  4698 | `		pGen->pIn++;` |
|        - |  4699 | `		/* Pass by reference  */` |
|       33 |  4700 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       15 |  4701 | `	}` |
|        - |  4702 | `	/* Check if the value target is list() */` |
|   175087 |  4703 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        8 |  4704 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|        - |  4705 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|        - |  4706 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|        - |  4707 | `		 */` |
|        - |  4708 | `		static int iForeachListCnt = 0;` |
|        - |  4709 | `		char zTmp[128];` |
|        - |  4710 | `		sxu32 nLen;` |
|        - |  4711 | `		char *zDup;` |
|       10 |  4712 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|       10 |  4713 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       10 |  4714 | `		if( zDup == 0 ){` |
|      ! 0 |  4715 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  4716 | `			return SXERR_ABORT;` |
|        - |  4717 | `		}` |
|       10 |  4718 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - |  4719 | `		/* Save list() token boundaries */` |
|       10 |  4720 | `		pListStart = pGen->pIn;` |
|        - |  4721 | `		/* Advance past list(...) — validate parentheses */` |
|       10 |  4722 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|       10 |  4723 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        3 |  4724 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|        - |  4725 | `				"foreach: Expected '(' after 'list'");` |
|        3 |  4726 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4727 | `				return SXERR_ABORT;` |
|        - |  4728 | `			}` |
|        3 |  4729 | `			goto Synchronize;` |
|        - |  4730 | `		}` |
|        7 |  4731 | `		pGen->pIn++; /* Jump '(' */` |
|        7 |  4732 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|        7 |  4733 | `		if( pListEnd >= pEnd ){` |
|      ! 0 |  4734 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  4735 | `				"foreach: Missing closing ')' after list");` |
|      ! 0 |  4736 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4737 | `				return SXERR_ABORT;` |
|        - |  4738 | `			}` |
|      ! 0 |  4739 | `			goto Synchronize;` |
|        - |  4740 | `		}` |
|        7 |  4741 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|        7 |  4742 | `		pListEnd = pGen->pIn;` |
|        7 |  4743 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|   175082 |  4744 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|        - |  4745 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|        - |  4746 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|        - |  4747 | `		 */` |
|        - |  4748 | `		static int iForeachShortListCnt = 0;` |
|        - |  4749 | `		char zTmp[128];` |
|        - |  4750 | `		sxu32 nLen;` |
|        - |  4751 | `		char *zDup;` |
|       13 |  4752 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       13 |  4753 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       13 |  4754 | `		if( zDup == 0 ){` |
|      ! 0 |  4755 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  4756 | `			return SXERR_ABORT;` |
|        - |  4757 | `		}` |
|       13 |  4758 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - |  4759 | `		/* Save [...] token boundaries */` |
|       13 |  4760 | `		pListStart = pGen->pIn;` |
|        - |  4761 | `		/* Advance past [...] */` |
|       13 |  4762 | `		pGen->pIn++; /* Jump '[' */` |
|       13 |  4763 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       13 |  4764 | `		if( pListEnd >= pEnd ){` |
|      ! 0 |  4765 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  4766 | `				"foreach: Missing closing ']' after short list");` |
|      ! 0 |  4767 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4768 | `				return SXERR_ABORT;` |
|        - |  4769 | `			}` |
|      ! 0 |  4770 | `			goto Synchronize;` |
|        - |  4771 | `		}` |
|       13 |  4772 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       13 |  4773 | `		pListEnd = pGen->pIn;` |
|       13 |  4774 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|        7 |  4775 | `	}else{` |
|        - |  4776 | `		/* Compile the expression holding the value name */` |
|   175067 |  4777 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   175067 |  4778 | `		if( rc == SXERR_ABORT ){` |
|        - |  4779 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4780 | `			return SXERR_ABORT;` |
|        - |  4781 | `		}` |
|   175067 |  4782 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   175067 |  4783 | `		if( pInstr->p3 ){` |
|        - |  4784 | `			/* Record value name */` |
|   175067 |  4785 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    87531 |  4786 | `		}` |
|        - |  4787 | `	}` |
|        - |  4788 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|   175085 |  4789 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|        - |  4790 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   175085 |  4791 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|        - |  4792 | `	/* Record the first instruction to execute */` |
|   175085 |  4793 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|        - |  4794 | `	/* Emit the FOREACH_STEP instruction */` |
|   175085 |  4795 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|        - |  4796 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   175085 |  4797 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|        - |  4798 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|   175085 |  4799 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|        - |  4800 | `		SyToken *pSavedIn,*pSavedEnd;` |
|        - |  4801 | `		/* Load the temporary variable holding the current value onto the stack.` |
|        - |  4802 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|        - |  4803 | `		 */` |
|       19 |  4804 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|        - |  4805 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|        - |  4806 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|        - |  4807 | `		 * picks up the delimiter and the variable names inside.` |
|        - |  4808 | `		 */` |
|       19 |  4809 | `		pSavedIn = pGen->pIn;` |
|       19 |  4810 | `		pSavedEnd = pGen->pEnd;` |
|       19 |  4811 | `		pGen->pIn = pListStart;` |
|       19 |  4812 | `		pGen->pEnd = pListEnd;` |
|       19 |  4813 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       13 |  4814 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|        7 |  4815 | `		}else{` |
|        7 |  4816 | `			rc = PH7_CompileList(&(*pGen),0);` |
|        - |  4817 | `		}` |
|       19 |  4818 | `		pGen->pIn = pSavedIn;` |
|       19 |  4819 | `		pGen->pEnd = pSavedEnd;` |
|       19 |  4820 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4821 | `			return SXERR_ABORT;` |
|        - |  4822 | `		}` |
|        - |  4823 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       19 |  4824 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        9 |  4825 | `	}` |
|        - |  4826 | `	/* Compile the loop body */` |
|   175085 |  4827 | `	pGen->pIn = &pEnd[1];` |
|   175085 |  4828 | `	pGen->pEnd = pTmp;` |
|   175085 |  4829 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|   175085 |  4830 | `	if( rc == SXERR_ABORT ){` |
|        - |  4831 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4832 | `		return SXERR_ABORT;` |
|        - |  4833 | `	}` |
|        - |  4834 | `	/* Emit the unconditional jump to the start of the loop */` |
|   175085 |  4835 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|        - |  4836 | `	/* Fix all jumps now the destination is resolved */` |
|   175085 |  4837 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4838 | `	/* Release the loop block */` |
|   175085 |  4839 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4840 | `	/* Statement successfully compiled */` |
|   175085 |  4841 | `	return SXRET_OK;` |
|        1 |  4842 | `Synchronize:` |
|        - |  4843 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  4844 | `	 * compiling this erroneous block.` |
|        - |  4845 | `	 */` |
|        3 |  4846 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  4847 | `		pGen->pIn++;` |
|      ! 0 |  4848 | `	}` |
|        3 |  4849 | `	return SXRET_OK;` |
|    87546 |  4850 | `}` |
|        - |  4851 | `/*` |
|        - |  4852 | ` * Compile the infamous if/elseif/else if/else statements.` |
|        - |  4853 | ` * According to the PHP language reference` |
|        - |  4854 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|        - |  4855 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|        - |  4856 | ` *  that is similar to that of C:` |
|        - |  4857 | ` *  if (expr)` |
|        - |  4858 | ` *   statement` |
|        - |  4859 | ` *  else construct:` |
|        - |  4860 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|        - |  4861 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|        - |  4862 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|        - |  4863 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|        - |  4864 | ` *   $b, and a is NOT greater than b otherwise.` |
|        - |  4865 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|        - |  4866 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|        - |  4867 | ` *  elseif` |
|        - |  4868 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|        - |  4869 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|        - |  4870 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|        - |  4871 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|        - |  4872 | ` *   than b, a equal to b or a is smaller than b:` |
|        - |  4873 | ` *   <?php` |
|        - |  4874 | ` *    if ($a > $b) {` |
|        - |  4875 | ` *     echo "a is bigger than b";` |
|        - |  4876 | ` *    } elseif ($a == $b) {` |
|        - |  4877 | ` *     echo "a is equal to b";` |
|        - |  4878 | ` *    } else {` |
|        - |  4879 | ` *     echo "a is smaller than b";` |
|        - |  4880 | ` *    }` |
|        - |  4881 | ` *    ?>` |
|        - |  4882 | ` */` |
|  1401826 |  4883 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|        5 |  4884 | `{` |
|  1401831 |  4885 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  1401831 |  4886 | `	GenBlock *pCondBlock = 0;` |
|        - |  4887 | `	sxu32 nJumpIdx;` |
|        - |  4888 | `	sxu32 nKeyID;` |
|        - |  4889 | `	sxi32 rc;` |
|        - |  4890 | `	/* Jump the 'if' keyword */` |
|  1401831 |  4891 | `	pGen->pIn++;` |
|  1401831 |  4892 | `	pToken = pGen->pIn;` |
|        - |  4893 | `	/* Create the conditional block */` |
|  1401831 |  4894 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  1401831 |  4895 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4896 | `		return SXERR_ABORT;` |
|        - |  4897 | `	}` |
|        - |  4898 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   805624 |  4899 | `	for(;;){` |
|  1611253 |  4900 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4901 | `			/* Syntax error */` |
|      ! 0 |  4902 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 |  4903 | `				pToken--;` |
|      ! 0 |  4904 | `			}` |
|      ! 0 |  4905 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|      ! 0 |  4906 | `			if( rc == SXERR_ABORT ){` |
|        - |  4907 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4908 | `				return SXERR_ABORT;` |
|        - |  4909 | `			}` |
|      ! 0 |  4910 | `			goto Synchronize;` |
|        - |  4911 | `		}` |
|        - |  4912 | `		/* Jump the left parenthesis '(' */` |
|  1611253 |  4913 | `		pToken++;` |
|        - |  4914 | `		/* Delimit the condition */` |
|  1611253 |  4915 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  1611253 |  4916 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|        - |  4917 | `			/* Syntax error */` |
|       10 |  4918 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 |  4919 | `				pToken--;` |
|      ! 0 |  4920 | `			}` |
|       10 |  4921 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|       10 |  4922 | `			if( rc == SXERR_ABORT ){` |
|        - |  4923 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4924 | `				return SXERR_ABORT;` |
|        - |  4925 | `			}` |
|       10 |  4926 | `			goto Synchronize;` |
|        - |  4927 | `		}` |
|        - |  4928 | `		/* Swap token streams */` |
|  1611245 |  4929 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|        - |  4930 | `		/* Compile the condition */` |
|  1611245 |  4931 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  4932 | `		/* Update token stream */` |
|  1611245 |  4933 | `		while(pGen->pIn < pEnd ){` |
|      ! 0 |  4934 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4935 | `			pGen->pIn++;` |
|      ! 0 |  4936 | `		}` |
|  1611245 |  4937 | `		pGen->pIn  = &pEnd[1];` |
|  1611245 |  4938 | `		pGen->pEnd = pTmp;` |
|  1611245 |  4939 | `		if( rc == SXERR_ABORT ){` |
|        - |  4940 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4941 | `			return SXERR_ABORT;` |
|        - |  4942 | `		}` |
|        - |  4943 | `		/* Emit the false jump */` |
|  1611245 |  4944 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|        - |  4945 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  1611245 |  4946 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|        - |  4947 | `		/* Compile the body */` |
|  1611245 |  4948 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  1611245 |  4949 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4950 | `			return SXERR_ABORT;` |
|        - |  4951 | `		}` |
|  1611245 |  4952 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   291583 |  4953 | `			break;` |
|        - |  4954 | `		}` |
|        - |  4955 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|  1028089 |  4956 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1028089 |  4957 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   698039 |  4958 | `			break;` |
|        - |  4959 | `		}` |
|        - |  4960 | `		/* Emit the unconditional jump */` |
|   330055 |  4961 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|        - |  4962 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   330055 |  4963 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   330055 |  4964 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   205905 |  4965 | `			pToken = &pGen->pIn[1];` |
|   205905 |  4966 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    85310 |  4967 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    60319 |  4968 | `					break;` |
|        - |  4969 | `			}` |
|    85277 |  4970 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    42636 |  4971 | `		}` |
|   209427 |  4972 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|        - |  4973 | `		/* Synchronize cursors */` |
|   209427 |  4974 | `		pToken = pGen->pIn;` |
|        - |  4975 | `		/* Fix the false jump */` |
|   209427 |  4976 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|        5 |  4977 | `	} /* For(;;) */` |
|        - |  4978 | `	/* Fix the false jump */` |
|  1401823 |  4979 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  1401823 |  4980 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   818662 |  4981 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|        - |  4982 | `			/* Compile the else block */` |
|   120633 |  4983 | `			pGen->pIn++;` |
|   120633 |  4984 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   120633 |  4985 | `			if( rc == SXERR_ABORT ){` |
|        - |  4986 |  |
|      ! 0 |  4987 | `				return SXERR_ABORT;` |
|        - |  4988 | `			}` |
|    60314 |  4989 | `	}` |
|  1401823 |  4990 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|        - |  4991 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  1401823 |  4992 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|        - |  4993 | `	/* Release the conditional block */` |
|  1401823 |  4994 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4995 | `	/* Statement successfully compiled */` |
|  1401823 |  4996 | `	return SXRET_OK;` |
|        4 |  4997 | `Synchronize:` |
|        - |  4998 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|        - |  4999 | `	 */` |
|       66 |  5000 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       58 |  5001 | `		pGen->pIn++;` |
|        2 |  5002 | `	}` |
|       10 |  5003 | `	return SXRET_OK;` |
|   700918 |  5004 | `}` |
|        - |  5005 | `/*` |
|        - |  5006 | ` * Compile the global construct.` |
|        - |  5007 | ` * According to the PHP language reference` |
|        - |  5008 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|        - |  5009 | ` *  to be used in that function.` |
|        - |  5010 | ` *  Example #1 Using global` |
|        - |  5011 | ` *  <?php` |
|        - |  5012 | ` *   $a = 1;` |
|        - |  5013 | ` *   $b = 2;` |
|        - |  5014 | ` *   function Sum()` |
|        - |  5015 | ` *   {` |
|        - |  5016 | ` *    global $a, $b;` |
|        - |  5017 | ` *    $b = $a + $b;` |
|        - |  5018 | ` *   }` |
|        - |  5019 | ` *   Sum();` |
|        - |  5020 | ` *   echo $b;` |
|        - |  5021 | ` *  ?>` |
|        - |  5022 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|        - |  5023 | ` *  all references to either variable will refer to the global version. There is no limit` |
|        - |  5024 | ` *  to the number of global variables that can be manipulated by a function.` |
|        - |  5025 | ` */` |
|       38 |  5026 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|        5 |  5027 | `{` |
|       43 |  5028 | `	SyToken *pTmp,*pNext = 0;` |
|        - |  5029 | `	sxi32 nExpr;` |
|        - |  5030 | `	sxi32 rc;` |
|        - |  5031 | `	/* Jump the 'global' keyword */` |
|       43 |  5032 | `	pGen->pIn++;` |
|       43 |  5033 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        - |  5034 | `		/* Nothing to process */` |
|      ! 0 |  5035 | `		return SXRET_OK;` |
|        - |  5036 | `	}` |
|       43 |  5037 | `	pTmp = pGen->pEnd;` |
|       43 |  5038 | `	nExpr = 0;` |
|       91 |  5039 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       53 |  5040 | `		if( pGen->pIn < pNext ){` |
|       53 |  5041 | `			pGen->pEnd = pNext;` |
|       53 |  5042 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  5043 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|      ! 0 |  5044 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  5045 | `					return SXERR_ABORT;` |
|        - |  5046 | `				}` |
|      ! 0 |  5047 | `			}else{` |
|       53 |  5048 | `				pGen->pIn++;` |
|       53 |  5049 | `				if( pGen->pIn >= pGen->pEnd ){` |
|        - |  5050 | `					/* Emit a warning */` |
|      ! 0 |  5051 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|      ! 0 |  5052 | `				}else{` |
|       53 |  5053 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       53 |  5054 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  5055 | `						return SXERR_ABORT;` |
|       53 |  5056 | `					}else if(rc != SXERR_EMPTY ){` |
|       53 |  5057 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|       53 |  5058 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|        - |  5059 | `							/* Variable name, not a constant */` |
|       53 |  5060 | `							pLast->iP1 = 0;` |
|       24 |  5061 | `						}` |
|       53 |  5062 | `						nExpr++;` |
|       24 |  5063 | `					}` |
|        - |  5064 | `				}` |
|        - |  5065 | `			}` |
|       24 |  5066 | `		}` |
|        - |  5067 | `		/* Next expression in the stream */` |
|       53 |  5068 | `		pGen->pIn = pNext;` |
|        - |  5069 | `		/* Jump trailing commas */` |
|       63 |  5070 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       15 |  5071 | `			pGen->pIn++;` |
|        5 |  5072 | `		}` |
|        5 |  5073 | `	}` |
|        - |  5074 | `	/* Restore token stream */` |
|       43 |  5075 | `	pGen->pEnd = pTmp;` |
|       43 |  5076 | `	if( nExpr > 0 ){` |
|        - |  5077 | `		/* Emit the uplink instruction */` |
|       43 |  5078 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|       19 |  5079 | `	}` |
|       43 |  5080 | `	return SXRET_OK;` |
|       24 |  5081 | `}` |
|        - |  5082 | `/*` |
|        - |  5083 | ` * Compile the return statement.` |
|        - |  5084 | ` * According to the PHP language reference` |
|        - |  5085 | ` *  If called from within a function, the return() statement immediately ends execution` |
|        - |  5086 | ` *  of the current function, and returns its argument as the value of the function call.` |
|        - |  5087 | ` *  return() will also end the execution of an eval() statement or script file.` |
|        - |  5088 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|        - |  5089 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|        - |  5090 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|        - |  5091 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|        - |  5092 | ` *  from within the main script file, then script execution end.` |
|        - |  5093 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|        - |  5094 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|        - |  5095 | ` *  should do so as PHP has less work to do in this case.` |
|        - |  5096 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|        - |  5097 | ` */` |
|  2114474 |  5098 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|        5 |  5099 | `{` |
|  2114479 |  5100 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|        - |  5101 | `	sxi32 rc;` |
|  2114479 |  5102 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  2114479 |  5103 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|        - |  5104 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|        - |  5105 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|        - |  5106 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|        - |  5107 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|        - |  5108 | `	 * normally below so token processing stays consistent. */` |
|  5392551 |  5109 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  3278077 |  5110 | `		pFuncBlock = pFuncBlock->pParent;` |
|        5 |  5111 | `	}` |
|  2114474 |  5112 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|  2114447 |  5113 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|        3 |  5114 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  5115 | `			"A never-returning function must not return");` |
|        3 |  5116 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5117 | `			return SXERR_ABORT;` |
|        - |  5118 | `		}` |
|        1 |  5119 | `	}` |
|        - |  5120 | `	/* Jump the 'return' keyword */` |
|  2114479 |  5121 | `	pGen->pIn++;` |
|  2114479 |  5122 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  5123 | `		/* Compile the expression */` |
|  2075689 |  5124 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  2075689 |  5125 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5126 | `			return SXERR_ABORT;` |
|  2075689 |  5127 | `		}else if(rc != SXERR_EMPTY ){` |
|  2075689 |  5128 | `			nRet = 1;` |
|  1037842 |  5129 | `		}` |
|  1037842 |  5130 | `	}` |
|        - |  5131 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|        - |  5132 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|        - |  5133 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|        - |  5134 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|  2114479 |  5135 | `	if( pGen->bInGenerator ){` |
|     3909 |  5136 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|     3909 |  5137 | `		return SXRET_OK;` |
|        - |  5138 | `	}` |
|        - |  5139 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|        - |  5140 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|        - |  5141 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|        - |  5142 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|        - |  5143 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  2110575 |  5144 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  2110575 |  5145 | `	return SXRET_OK;` |
|  1057242 |  5146 | `}` |
|        - |  5147 | `/*` |
|        - |  5148 | ` * Compile a yield expression.` |
|        - |  5149 | ` * Called from the expression code generator when a yield node is encountered.` |
|        - |  5150 | ` * Handles: yield, yield $value, yield $key => $value` |
|        - |  5151 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|        - |  5152 | ` */` |
|     8136 |  5153 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        5 |  5154 | `{` |
|        - |  5155 | `	SyToken *pTmp, *pSplit;` |
|     8141 |  5156 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     8141 |  5157 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|        - |  5158 | `	sxi32 rc;` |
|     4068 |  5159 | `	(void)iCompileFlag;` |
|        - |  5160 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     8141 |  5161 | `	pGen->pIn++;` |
|        - |  5162 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|        - |  5163 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|        - |  5164 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|        - |  5165 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|        - |  5166 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     8136 |  5167 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|     4103 |  5168 | `		&& pGen->pIn->sData.nByte == 4` |
|       72 |  5169 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|       67 |  5170 | `		pGen->pIn++; /* Skip 'from' */` |
|       67 |  5171 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       67 |  5172 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5173 | `			return SXERR_ABORT;` |
|        - |  5174 | `		}` |
|       67 |  5175 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 |  5176 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|      ! 0 |  5177 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|        - |  5178 | `				"Missing expression after 'yield from'");` |
|      ! 0 |  5179 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5180 | `				return SXERR_ABORT;` |
|        - |  5181 | `			}` |
|      ! 0 |  5182 | `		}` |
|       67 |  5183 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|       67 |  5184 | `		return SXRET_OK;` |
|        - |  5185 | `	}` |
|     8079 |  5186 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  5187 | `		/* Bare yield — no value */` |
|        3 |  5188 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|        3 |  5189 | `		return SXRET_OK;` |
|        - |  5190 | `	}` |
|        - |  5191 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     8077 |  5192 | `	pSplit = 0;` |
|        - |  5193 | `	{` |
|     8077 |  5194 | `		SyToken *pCur = pGen->pIn;` |
|     8077 |  5195 | `		sxi32 nNest = 0;` |
|    24037 |  5196 | `		while( pCur < pGen->pEnd ){` |
|    23731 |  5197 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       18 |  5198 | `				nNest++;` |
|    23723 |  5199 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       18 |  5200 | `				nNest--;` |
|    23707 |  5201 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|     7771 |  5202 | `				pSplit = pCur;` |
|     7771 |  5203 | `				break;` |
|        - |  5204 | `			}` |
|    15965 |  5205 | `			pCur++;` |
|        5 |  5206 | `		}` |
|        - |  5207 | `	}` |
|     8077 |  5208 | `	pTmp = pGen->pEnd;` |
|     8077 |  5209 | `	if( pSplit ){` |
|        - |  5210 | `		/* yield $key => $value */` |
|     7771 |  5211 | `		pGen->pEnd = pSplit;` |
|     7771 |  5212 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     7771 |  5213 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     7771 |  5214 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|     7771 |  5215 | `		pGen->pEnd = pTmp;` |
|     7771 |  5216 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     7771 |  5217 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     7771 |  5218 | `		iP1 = 1;` |
|     7771 |  5219 | `		iP2 = 1;` |
|     3888 |  5220 | `	}else{` |
|        - |  5221 | `		/* yield $value */` |
|      311 |  5222 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      311 |  5223 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      311 |  5224 | `		if( rc != SXERR_EMPTY ){` |
|      311 |  5225 | `			iP1 = 1;` |
|      153 |  5226 | `		}` |
|        - |  5227 | `	}` |
|     8077 |  5228 | `	pGen->pEnd = pTmp;` |
|     8077 |  5229 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     8077 |  5230 | `	return SXRET_OK;` |
|     4073 |  5231 | `}` |
|        - |  5232 | `/*` |
|        - |  5233 | ` * Compile the die/exit language construct.` |
|        - |  5234 | ` * The role of these constructs is to terminate execution of the script.` |
|        - |  5235 | ` * Shutdown functions will always be executed even if exit() is called.` |
|        - |  5236 | ` */` |
|      128 |  5237 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|        5 |  5238 | `{` |
|      133 |  5239 | `	sxi32 nExpr = 0;` |
|        - |  5240 | `	sxi32 rc;` |
|        - |  5241 | `	/* Jump the die/exit keyword */` |
|      133 |  5242 | `	pGen->pIn++;` |
|      133 |  5243 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  5244 | `		/* Compile the expression */` |
|      133 |  5245 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      133 |  5246 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5247 | `			return SXERR_ABORT;` |
|      133 |  5248 | `		}else if(rc != SXERR_EMPTY ){` |
|      133 |  5249 | `			nExpr = 1;` |
|       64 |  5250 | `		}` |
|       64 |  5251 | `	}` |
|        - |  5252 | `	/* Emit the HALT instruction */` |
|      133 |  5253 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      133 |  5254 | `	return SXRET_OK;` |
|       69 |  5255 | `}` |
|        - |  5256 | `/*` |
|        - |  5257 | ` * Compile the 'echo' language construct.` |
|        - |  5258 | ` */` |
|    17712 |  5259 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|        5 |  5260 | `{` |
|    17717 |  5261 | `	SyToken *pTmp,*pNext = 0;` |
|        - |  5262 | `	sxi32 rc;` |
|        - |  5263 | `	/* Jump the 'echo' keyword */` |
|    17717 |  5264 | `	pGen->pIn++;` |
|        - |  5265 | `	/* Compile arguments one after one */` |
|    17717 |  5266 | `	pTmp = pGen->pEnd;` |
|    43851 |  5267 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|    26139 |  5268 | `		if( pGen->pIn < pNext ){` |
|    26139 |  5269 | `			pGen->pEnd = pNext;` |
|    26139 |  5270 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|    26139 |  5271 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5272 | `				return SXERR_ABORT;` |
|    26139 |  5273 | `			}else if( rc != SXERR_EMPTY ){` |
|        - |  5274 | `				/* Emit the consume instruction */` |
|    26115 |  5275 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    13055 |  5276 | `			}` |
|    13067 |  5277 | `		}` |
|        - |  5278 | `		/* Jump trailing commas */` |
|    34561 |  5279 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     8427 |  5280 | `			pNext++;` |
|        5 |  5281 | `		}` |
|    26139 |  5282 | `		pGen->pIn = pNext;` |
|        5 |  5283 | `	}` |
|        - |  5284 | `	/* Restore token stream */` |
|    17717 |  5285 | `	pGen->pEnd = pTmp;` |
|    17717 |  5286 | `	return SXRET_OK;` |
|     8861 |  5287 | `}` |
|        - |  5288 | `/*` |
|        - |  5289 | ` * Compile the static statement.` |
|        - |  5290 | ` * According to the PHP language reference` |
|        - |  5291 | ` *  Another important feature of variable scoping is the static variable.` |
|        - |  5292 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|        - |  5293 | ` *  when program execution leaves this scope.` |
|        - |  5294 | ` *  Static variables also provide one way to deal with recursive functions.` |
|        - |  5295 | ` * Symisc eXtension.` |
|        - |  5296 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|        - |  5297 | ` *  the zend engine would allow only simple scalar value.` |
|        - |  5298 | ` *  Example` |
|        - |  5299 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|        - |  5300 | ` *    Refer to the official documentation for more information on this feature.` |
|        - |  5301 | ` */` |
|       12 |  5302 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|        3 |  5303 | `{` |
|        - |  5304 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|        - |  5305 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|        - |  5306 | `	GenBlock *pBlock;` |
|        - |  5307 | `	SyString *pName;` |
|        - |  5308 | `	char *zDup;` |
|        - |  5309 | `	sxu32 nLine;` |
|        - |  5310 | `	sxi32 rc;` |
|        - |  5311 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|        - |  5312 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|        - |  5313 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|       12 |  5314 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|       10 |  5315 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|        1 |  5316 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|        3 |  5317 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        3 |  5318 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5319 | `			return SXERR_ABORT;` |
|        3 |  5320 | `		}else if( rc != SXERR_EMPTY ){` |
|        3 |  5321 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        1 |  5322 | `		}` |
|        3 |  5323 | `		return SXRET_OK;` |
|        - |  5324 | `	}` |
|        - |  5325 | `	/* Jump the static keyword */` |
|       13 |  5326 | `	nLine = pGen->pIn->nLine;` |
|       13 |  5327 | `	pGen->pIn++;` |
|        - |  5328 | `	/* Extract the enclosing function if any */` |
|       13 |  5329 | `	pBlock = pGen->pCurrent;` |
|       23 |  5330 | `	while( pBlock ){` |
|       23 |  5331 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       13 |  5332 | `			break;` |
|        - |  5333 | `		}` |
|        - |  5334 | `		/* Point to the upper block */` |
|       13 |  5335 | `		pBlock = pBlock->pParent;` |
|        3 |  5336 | `	}` |
|       13 |  5337 | `	if( pBlock == 0 ){` |
|        - |  5338 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|      ! 0 |  5339 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  5340 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|      ! 0 |  5341 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5342 | `				return SXERR_ABORT;` |
|        - |  5343 | `			}` |
|      ! 0 |  5344 | `			goto Synchronize;` |
|        - |  5345 | `		}` |
|        - |  5346 | `		/* Compile the expression holding the variable */` |
|      ! 0 |  5347 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      ! 0 |  5348 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5349 | `			return SXERR_ABORT;` |
|      ! 0 |  5350 | `		}else if( rc != SXERR_EMPTY ){` |
|        - |  5351 | `			/* Emit the POP instruction */` |
|      ! 0 |  5352 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 |  5353 | `		}` |
|      ! 0 |  5354 | `		return SXRET_OK;` |
|        - |  5355 | `	}` |
|       13 |  5356 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        - |  5357 | `	/* Make sure we are dealing with a valid statement */` |
|       13 |  5358 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|        8 |  5359 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        3 |  5360 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|        3 |  5361 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5362 | `				return SXERR_ABORT;` |
|        - |  5363 | `			}` |
|        3 |  5364 | `			goto Synchronize;` |
|        - |  5365 | `	}` |
|       10 |  5366 | `	pGen->pIn++;` |
|        - |  5367 | `	/* Extract variable name */` |
|       10 |  5368 | `	pName = &pGen->pIn->sData;` |
|       10 |  5369 | `	pGen->pIn++; /* Jump the var name */` |
|       10 |  5370 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|      ! 0 |  5371 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  5372 | `		goto Synchronize;` |
|        - |  5373 | `	}` |
|        - |  5374 | `	/* Initialize the structure describing the static variable */` |
|       10 |  5375 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       10 |  5376 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|        - |  5377 | `	/* Duplicate variable name */` |
|       10 |  5378 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       10 |  5379 | `	if( zDup == 0 ){` |
|      ! 0 |  5380 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  5381 | `		return SXERR_ABORT;` |
|        - |  5382 | `	}` |
|       10 |  5383 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|        - |  5384 | `	/* Check if we have an expression to compile */` |
|       10 |  5385 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|        - |  5386 | `		SySet *pInstrContainer;` |
|        - |  5387 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|        - |  5388 | `		 * Static variable can take any complex expression including function` |
|        - |  5389 | `		 * call as their initialization value.` |
|        - |  5390 | `		 * Example:` |
|        - |  5391 | `		 *		static $var = foo(1,4+5,bar());` |
|        - |  5392 | `		 */` |
|       10 |  5393 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|        - |  5394 | `		/* Swap bytecode container */` |
|       10 |  5395 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       10 |  5396 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|        - |  5397 | `		/* Compile the expression */` |
|       10 |  5398 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  5399 | `		/* Emit the done instruction */` |
|       10 |  5400 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        - |  5401 | `		/* Restore default bytecode container */` |
|       10 |  5402 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        4 |  5403 | `	}` |
|        - |  5404 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       10 |  5405 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       10 |  5406 | `	return SXRET_OK;` |
|        1 |  5407 | `Synchronize:` |
|        - |  5408 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|        - |  5409 | `	 * statement.` |
|        - |  5410 | `	 */` |
|        5 |  5411 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|        3 |  5412 | `		pGen->pIn++;` |
|        1 |  5413 | `	}` |
|        3 |  5414 | `	return SXRET_OK;` |
|        9 |  5415 | `}` |
|        - |  5416 | `/*` |
|        - |  5417 | ` * Compile the var statement.` |
|        - |  5418 | ` * Symisc Extension:` |
|        - |  5419 | ` *      var statement can be used outside of a class definition.` |
|        - |  5420 | ` */` |
|        4 |  5421 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|        1 |  5422 | `{` |
|        - |  5423 | `	sxu32 nLine;` |
|        - |  5424 | `	sxi32 rc;` |
|        5 |  5425 | `	nLine = pGen->pIn->nLine;` |
|        - |  5426 | `	/* Jump the 'var' keyword */` |
|        5 |  5427 | `	pGen->pIn++;` |
|        5 |  5428 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|      ! 0 |  5429 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|        - |  5430 | `		/* Synchronize with the first semi-colon */` |
|      ! 0 |  5431 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|      ! 0 |  5432 | `			pGen->pIn++;` |
|      ! 0 |  5433 | `		}` |
|      ! 0 |  5434 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5435 | `			return SXERR_ABORT;` |
|        - |  5436 | `		}` |
|      ! 0 |  5437 | `	}else{` |
|        - |  5438 | `		/* Compile the expression */` |
|        5 |  5439 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        5 |  5440 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5441 | `			return SXERR_ABORT;` |
|        5 |  5442 | `		}else if( rc != SXERR_EMPTY ){` |
|        5 |  5443 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        2 |  5444 | `		}` |
|        - |  5445 | `	}` |
|        5 |  5446 | `	return SXRET_OK;` |
|        3 |  5447 | `}` |
|        - |  5448 | `/*` |
|        - |  5449 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|        - |  5450 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|        - |  5451 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|        - |  5452 | ` */` |
|        - |  5453 | `/*` |
|        - |  5454 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|        - |  5455 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|        - |  5456 | ` * hash and any shared references), this creates a new literal entry with the` |
|        - |  5457 | ` * qualified name and updates the instruction's operand index.` |
|        - |  5458 | ` *` |
|        - |  5459 | ` * Resolution order:` |
|        - |  5460 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|        - |  5461 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|        - |  5462 | ` *   3. Otherwise return the original literal index unchanged.` |
|        - |  5463 | ` *` |
|        - |  5464 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|        - |  5465 | ` * came from an import (step 1) and 0 otherwise.` |
|        - |  5466 | ` * Returns the (possibly new) literal index.` |
|        - |  5467 | ` */` |
|  3605240 |  5468 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|        5 |  5469 | `{` |
|        - |  5470 | `	ph7_value *pLit;` |
|        - |  5471 | `	const char *zLit;` |
|        - |  5472 | `	SyString sQualified;` |
|        - |  5473 | `	sxu32 nLit;` |
|        - |  5474 | `	sxu32 k;` |
|        - |  5475 | `	sxu32 nNewIdx;` |
|        - |  5476 | `	int hasNsSep;` |
|        - |  5477 | `	SyHashEntry *pImport;` |
|        - |  5478 | `	ph7_value *pNew;` |
|  3605245 |  5479 | `	if( pFromImport ){` |
|  2915581 |  5480 | `		*pFromImport = 0;` |
|  1457788 |  5481 | `	}` |
|  3605245 |  5482 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  3605245 |  5483 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|      ! 0 |  5484 | `		return nOrigIdx;` |
|        - |  5485 | `	}` |
|  3605245 |  5486 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  3605245 |  5487 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|        - |  5488 | `	/* Skip if already qualified (contains backslash) */` |
|  3605245 |  5489 | `	hasNsSep = 0;` |
| 46341493 |  5490 | `	for( k = 0; k < nLit; k++ ){` |
| 42736261 |  5491 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 21368129 |  5492 | `	}` |
|  3605245 |  5493 | `	if( hasNsSep ){` |
|       11 |  5494 | `		return nOrigIdx;` |
|        - |  5495 | `	}` |
|        - |  5496 | `	/* Check use imports first (works even outside namespaces) */` |
|  3605237 |  5497 | `	SyBlobReset(&pGen->sWorker);` |
|  3605237 |  5498 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  3605237 |  5499 | `	if( pImport ){` |
|       41 |  5500 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       41 |  5501 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|       41 |  5502 | `		if( pFromImport ){` |
|       18 |  5503 | `			*pFromImport = 1;` |
|        8 |  5504 | `		}` |
|       23 |  5505 | `	}else{` |
|  3605201 |  5506 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  3605111 |  5507 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|        - |  5508 | `		}` |
|        - |  5509 | `		/* Prepend current namespace */` |
|       95 |  5510 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       95 |  5511 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|       95 |  5512 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|        - |  5513 | `	}` |
|        - |  5514 | `	/* Look up or create a new literal for the qualified name */` |
|      131 |  5515 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|      131 |  5516 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|       57 |  5517 | `		return nNewIdx; /* Already interned */` |
|        - |  5518 | `	}` |
|       79 |  5519 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|       79 |  5520 | `	if( pNew == 0 ){` |
|      ! 0 |  5521 | `		return nOrigIdx; /* OOM, fall back to original */` |
|        - |  5522 | `	}` |
|       79 |  5523 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|       79 |  5524 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|       79 |  5525 | `	return nNewIdx;` |
|  1802625 |  5526 | `}` |
|        - |  5527 | `/*` |
|        - |  5528 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|        - |  5529 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|        - |  5530 | ` */` |
|   315406 |  5531 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 |  5532 | `{` |
|        - |  5533 | `	SyHashEntry *pImport;` |
|        - |  5534 | `	/* Check use imports first */` |
|   315411 |  5535 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   315411 |  5536 | `	if( pImport ){` |
|       20 |  5537 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       20 |  5538 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|       20 |  5539 | `		return;` |
|        - |  5540 | `	}` |
|        - |  5541 | `	/* Prepend current namespace if active */` |
|   315395 |  5542 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        8 |  5543 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        8 |  5544 | `		SyBlobAppend(pOut,"\\",1);` |
|        3 |  5545 | `	}` |
|   315395 |  5546 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   157708 |  5547 | `}` |
|        - |  5548 | `/*` |
|        - |  5549 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|        - |  5550 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|        - |  5551 | ` * The caller must release pOut when done.` |
|        - |  5552 | ` */` |
|   366294 |  5553 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 |  5554 | `{` |
|   366299 |  5555 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     3939 |  5556 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     3939 |  5557 | `		SyBlobAppend(pOut,"\\",1);` |
|     1967 |  5558 | `	}` |
|   366299 |  5559 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   366299 |  5560 | `}` |
|        - |  5561 | `/*` |
|        - |  5562 | ` * Compile a namespace statement` |
|        - |  5563 | ` * According to the PHP language reference manual` |
|        - |  5564 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|        - |  5565 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|        - |  5566 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|        - |  5567 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|        - |  5568 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|        - |  5569 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|        - |  5570 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|        - |  5571 | ` *  programming world.` |
|        - |  5572 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|        - |  5573 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|        - |  5574 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|        - |  5575 | ` *  classes/functions/constants.` |
|        - |  5576 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|        - |  5577 | ` *  readability of source code.` |
|        - |  5578 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|        - |  5579 | ` *  Here is an example of namespace syntax in PHP:` |
|        - |  5580 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|        - |  5581 | ` *       class MyClass {}` |
|        - |  5582 | ` *       function myfunction() {}` |
|        - |  5583 | ` *       const MYCONST = 1;` |
|        - |  5584 | ` *       $a = new MyClass;` |
|        - |  5585 | ` *       $c = new \my\name\MyClass;` |
|        - |  5586 | ` *       $a = strlen('hi');` |
|        - |  5587 | ` *       $d = namespace\MYCONST;` |
|        - |  5588 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|        - |  5589 | ` *       echo constant($d);` |
|        - |  5590 | ` * NOTE` |
|        - |  5591 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - |  5592 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - |  5593 | ` */` |
|        - |  5594 | `/*` |
|        - |  5595 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|        - |  5596 | ` */` |
|       14 |  5597 | `static const char * TokenTypeName(sxu32 nType)` |
|        4 |  5598 | `{` |
|       18 |  5599 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       11 |  5600 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       11 |  5601 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       11 |  5602 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       11 |  5603 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       11 |  5604 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|        3 |  5605 | `	return "token";` |
|       11 |  5606 | `}` |
|     3982 |  5607 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|        5 |  5608 | `{` |
|        - |  5609 | `	sxu32 nLine;` |
|        - |  5610 | `	sxi32 rc;` |
|     3987 |  5611 | `	nLine = pGen->pIn->nLine;` |
|     3987 |  5612 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|        - |  5613 | `	/* Reset namespace and clear previous use imports */` |
|     3987 |  5614 | `	SyBlobReset(&pGen->sNamespace);` |
|     3987 |  5615 | `	SyHashRelease(&pGen->hUseImports);` |
|     3987 |  5616 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     3987 |  5617 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     3987 |  5618 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     3987 |  5619 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     3987 |  5620 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     3987 |  5621 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  5622 | `		/* Global namespace (bare "namespace;") */` |
|      ! 0 |  5623 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 |  5624 | `		return SXRET_OK;` |
|        - |  5625 | `	}` |
|     3987 |  5626 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        - |  5627 | `		/* namespace; — switch to global namespace */` |
|      ! 0 |  5628 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 |  5629 | `		return SXRET_OK;` |
|        - |  5630 | `	}` |
|     3987 |  5631 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|        - |  5632 | `		/* namespace { } — global namespace block */` |
|      ! 0 |  5633 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 |  5634 | `		return SXRET_OK;` |
|        - |  5635 | `	}` |
|        - |  5636 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     8011 |  5637 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     4029 |  5638 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        - |  5639 | `			/* Append backslash separator */` |
|       26 |  5640 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       26 |  5641 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|       11 |  5642 | `			}` |
|       15 |  5643 | `		}else{` |
|        - |  5644 | `			/* Append identifier */` |
|     4007 |  5645 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - |  5646 | `		}` |
|     4029 |  5647 | `		pGen->pIn++;` |
|        5 |  5648 | `	}` |
|        - |  5649 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|        - |  5650 | `	 * at the correct program counter, not just the last one compiled. */` |
|        - |  5651 | `	{` |
|     3987 |  5652 | `		char *zNsDup = 0;` |
|     3987 |  5653 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     5975 |  5654 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     3980 |  5655 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     1990 |  5656 | `		}` |
|     3987 |  5657 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|        - |  5658 | `	}` |
|     3987 |  5659 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        8 |  5660 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  5661 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|        4 |  5662 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        6 |  5663 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5664 | `			return SXERR_ABORT;` |
|        - |  5665 | `		}` |
|        2 |  5666 | `	}` |
|     3987 |  5667 | `	return SXRET_OK;` |
|     1996 |  5668 | `}` |
|        - |  5669 | `/*` |
|        - |  5670 | ` * Compile the 'use' statement` |
|        - |  5671 | ` * According to the PHP language reference manual` |
|        - |  5672 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|        - |  5673 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|        - |  5674 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|        - |  5675 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|        - |  5676 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|        - |  5677 | ` *  a function or constant is not supported.` |
|        - |  5678 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|        - |  5679 | ` * NOTE` |
|        - |  5680 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - |  5681 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - |  5682 | ` */` |
|       72 |  5683 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|        5 |  5684 | `{` |
|        - |  5685 | `	sxu32 nLine;` |
|        - |  5686 | `	sxi32 rc;` |
|        - |  5687 | `	SyBlob sPath;` |
|        - |  5688 | `	SyString sAlias;` |
|        - |  5689 | `	SyToken *pLast;` |
|        - |  5690 | `	char *zDup;` |
|        - |  5691 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|        - |  5692 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|        - |  5693 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|       77 |  5694 | `	nLine = pGen->pIn->nLine;` |
|       77 |  5695 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|        - |  5696 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|       77 |  5697 | `	iUseType = 0;` |
|       77 |  5698 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       30 |  5699 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|       30 |  5700 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|       16 |  5701 | `			iUseType = 1;` |
|       16 |  5702 | `			pGen->pIn++;` |
|       23 |  5703 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|       16 |  5704 | `			iUseType = 2;` |
|       16 |  5705 | `			pGen->pIn++;` |
|        7 |  5706 | `		}` |
|       14 |  5707 | `	}` |
|        - |  5708 | `	/* Select target hash tables based on import type */` |
|       77 |  5709 | `	switch( iUseType ){` |
|        7 |  5710 | `		case 1:` |
|       16 |  5711 | `			pGenHash = &pGen->hUseFuncImports;` |
|       16 |  5712 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|       16 |  5713 | `			break;` |
|        7 |  5714 | `		case 2:` |
|       16 |  5715 | `			pGenHash = &pGen->hUseConstImports;` |
|       16 |  5716 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|       16 |  5717 | `			break;` |
|       22 |  5718 | `		default:` |
|       49 |  5719 | `			pGenHash = &pGen->hUseImports;` |
|       49 |  5720 | `			pVmHash = &pGen->pVm->hUseImports;` |
|       44 |  5721 | `			break;` |
|        - |  5722 | `	}` |
|       77 |  5723 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|        - |  5724 | `	/* Process one or more use declarations separated by commas */` |
|       37 |  5725 | `	for(;;){` |
|       79 |  5726 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  5727 | `			break;` |
|        - |  5728 | `		}` |
|       79 |  5729 | `		SyBlobReset(&sPath);` |
|       79 |  5730 | `		pLast = 0;` |
|        - |  5731 | `		/* Collect the full namespace path */` |
|      269 |  5732 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|      195 |  5733 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|      135 |  5734 | `				pLast = pGen->pIn;` |
|      135 |  5735 | `				if( SyBlobLength(&sPath) > 0 ){` |
|       65 |  5736 | `					SyBlobAppend(&sPath,"\\",1);` |
|       30 |  5737 | `				}` |
|      135 |  5738 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       65 |  5739 | `			}` |
|      195 |  5740 | `			pGen->pIn++;` |
|        5 |  5741 | `		}` |
|       79 |  5742 | `		if( pLast == 0 ){` |
|        - |  5743 | `			/* Empty path */` |
|        6 |  5744 | `			break;` |
|        - |  5745 | `		}` |
|        - |  5746 | `		/* Default alias is the last component of the path */` |
|       75 |  5747 | `		sAlias = pLast->sData;` |
|        - |  5748 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|       70 |  5749 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       50 |  5750 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|       22 |  5751 | `			pGen->pIn++; /* Jump 'as' */` |
|       22 |  5752 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|       22 |  5753 | `				sAlias = pGen->pIn->sData;` |
|       22 |  5754 | `				pGen->pIn++;` |
|       10 |  5755 | `			}` |
|       10 |  5756 | `		}` |
|        - |  5757 | `		/* Check for duplicate import alias (per-type) */` |
|       75 |  5758 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|        8 |  5759 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  5760 | `				"Cannot use %.*s as %z because the name is already in use",` |
|        4 |  5761 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|        6 |  5762 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5763 | `				SyBlobRelease(&sPath);` |
|      ! 0 |  5764 | `				return SXERR_ABORT;` |
|        - |  5765 | `			}` |
|        2 |  5766 | `		}` |
|        - |  5767 | `		/* Register the import: alias -> FQN.` |
|        - |  5768 | `		 * Strings are allocated from the VM pool allocator and freed` |
|        - |  5769 | `		 * when the entire VM is released. SyHashRelease does not free` |
|        - |  5770 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      110 |  5771 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       70 |  5772 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|       75 |  5773 | `		if( zDup ){` |
|       75 |  5774 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|       75 |  5775 | `			if( pVmHash ){` |
|        - |  5776 | `				/* Class imports: populate VM table directly (class resolution` |
|        - |  5777 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|       47 |  5778 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       47 |  5779 | `				if( zAliasDup ){` |
|       47 |  5780 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|       21 |  5781 | `				}` |
|       21 |  5782 | `			}` |
|       75 |  5783 | `			if( iUseType == 2 ){` |
|        - |  5784 | `				/* Const imports: emit a runtime instruction so imports are` |
|        - |  5785 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|       16 |  5786 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       16 |  5787 | `				if( zAliasDup ){` |
|        - |  5788 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|        - |  5789 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|        - |  5790 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|       16 |  5791 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|       16 |  5792 | `					if( azPair ){` |
|       16 |  5793 | `						azPair[0] = zAliasDup;` |
|       16 |  5794 | `						azPair[1] = zDup;` |
|       16 |  5795 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|        7 |  5796 | `					}` |
|        7 |  5797 | `				}` |
|        7 |  5798 | `			}` |
|       35 |  5799 | `		}` |
|        - |  5800 | `		/* Check for comma (multiple use declarations) */` |
|       75 |  5801 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        3 |  5802 | `			pGen->pIn++;` |
|        2 |  5803 | `		}else{` |
|       39 |  5804 | `			break;` |
|        - |  5805 | `		}` |
|        1 |  5806 | `	}` |
|       77 |  5807 | `	SyBlobRelease(&sPath);` |
|       77 |  5808 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        4 |  5809 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|        2 |  5810 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        3 |  5811 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5812 | `			return SXERR_ABORT;` |
|        - |  5813 | `		}` |
|        1 |  5814 | `	}` |
|       77 |  5815 | `	return SXRET_OK;` |
|       41 |  5816 | `}` |
|        - |  5817 | `/*` |
|        - |  5818 | ` * Compile the stupid 'declare' language construct.` |
|        - |  5819 | ` *` |
|        - |  5820 | ` * According to the PHP language reference manual.` |
|        - |  5821 | ` *  The declare construct is used to set execution directives for a block of code.` |
|        - |  5822 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|        - |  5823 | ` *  declare (directive)` |
|        - |  5824 | ` *   statement` |
|        - |  5825 | ` * The directive section allows the behavior of the declare block to be set.` |
|        - |  5826 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|        - |  5827 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|        - |  5828 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|        - |  5829 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|        - |  5830 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|        - |  5831 | ` * <?php` |
|        - |  5832 | ` * // these are the same:` |
|        - |  5833 | ` * // you can use this:` |
|        - |  5834 | ` * declare(ticks=1) {` |
|        - |  5835 | ` *   // entire script here` |
|        - |  5836 | ` * }` |
|        - |  5837 | ` * // or you can use this:` |
|        - |  5838 | ` * declare(ticks=1);` |
|        - |  5839 | ` * // entire script here` |
|        - |  5840 | ` * ?>` |
|        - |  5841 | ` *` |
|        - |  5842 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|        - |  5843 | ` */` |
|        - |  5844 | `/*` |
|        - |  5845 | ` * Match a directive name against a known literal (case-insensitive).` |
|        - |  5846 | ` */` |
|       72 |  5847 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|        5 |  5848 | `{` |
|      109 |  5849 | `	return SyStringLength(pName) == nWant` |
|       72 |  5850 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|        5 |  5851 | `}` |
|        - |  5852 |  |
|       42 |  5853 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|        5 |  5854 | `{` |
|       47 |  5855 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       47 |  5856 | `	SyToken *pBodyEnd = 0;` |
|        - |  5857 | `	SyToken *pBodyStart;` |
|        - |  5858 | `	SyToken *pCursor;` |
|        - |  5859 | `	int bHasStrictTypes;` |
|        - |  5860 | `	int bBlockForm;` |
|        - |  5861 | `	int bPlacementOk;` |
|        - |  5862 | `	sxi32 rc;` |
|       47 |  5863 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       47 |  5864 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|        6 |  5865 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|        6 |  5866 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5867 | `			return SXERR_ABORT;` |
|        - |  5868 | `		}` |
|        6 |  5869 | `		goto Synchro;` |
|        - |  5870 | `	}` |
|       43 |  5871 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       43 |  5872 | `	pBodyStart = pGen->pIn;` |
|        - |  5873 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|       43 |  5874 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|       43 |  5875 | `	if( pBodyEnd >= pGen->pEnd ){` |
|      ! 0 |  5876 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|      ! 0 |  5877 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5878 | `			return SXERR_ABORT;` |
|        - |  5879 | `		}` |
|      ! 0 |  5880 | `		return SXRET_OK;` |
|        - |  5881 | `	}` |
|        - |  5882 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|        - |  5883 | `	 * now delimits the comma-separated directive list. */` |
|       43 |  5884 | `	pGen->pIn = &pBodyEnd[1];` |
|       43 |  5885 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      ! 0 |  5886 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|      ! 0 |  5887 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5888 | `			return SXERR_ABORT;` |
|        - |  5889 | `		}` |
|      ! 0 |  5890 | `	}` |
|       43 |  5891 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|       43 |  5892 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|       43 |  5893 | `	bHasStrictTypes = 0;` |
|        - |  5894 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|        - |  5895 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|        - |  5896 | `	 * directive appears anywhere in the list, before validating values. */` |
|       43 |  5897 | `	pCursor = pBodyStart;` |
|       55 |  5898 | `	while( pCursor < pBodyEnd ){` |
|       51 |  5899 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       43 |  5900 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|       39 |  5901 | `				bHasStrictTypes = 1;` |
|       39 |  5902 | `				break;` |
|        - |  5903 | `			}` |
|        2 |  5904 | `		}` |
|       14 |  5905 | `		pCursor++;` |
|        2 |  5906 | `	}` |
|       43 |  5907 | `	if( bHasStrictTypes && bBlockForm ){` |
|        3 |  5908 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5909 | `			"strict_types declaration must not use block mode");` |
|        3 |  5910 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 |  5911 | `		return SXRET_OK;` |
|        - |  5912 | `	}` |
|       41 |  5913 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|        6 |  5914 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5915 | `			"strict_types declaration must be the very first statement in the script");` |
|        6 |  5916 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        6 |  5917 | `		return SXRET_OK;` |
|        - |  5918 | `	}` |
|        - |  5919 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|       37 |  5920 | `	pCursor = pBodyStart;` |
|       69 |  5921 | `	while( pCursor < pBodyEnd ){` |
|        - |  5922 | `		SyToken *pNameTok;` |
|        - |  5923 | `		SyToken *pEqTok;` |
|        - |  5924 | `		SyToken *pValTok;` |
|        - |  5925 | `		SyString *pDirName;` |
|        - |  5926 | `		int bIsStrict;` |
|        - |  5927 | `		int iStrictValue;` |
|       39 |  5928 | `		pNameTok = pCursor;` |
|       39 |  5929 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  5930 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5931 | `				"declare: Expecting a directive name");` |
|      ! 0 |  5932 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5933 | `			return SXRET_OK;` |
|        - |  5934 | `		}` |
|       39 |  5935 | `		pEqTok = pNameTok + 1;` |
|       39 |  5936 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|      ! 0 |  5937 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5938 | `				"declare: Expecting '=' after directive name");` |
|      ! 0 |  5939 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5940 | `			return SXRET_OK;` |
|        - |  5941 | `		}` |
|       39 |  5942 | `		pValTok = pEqTok + 1;` |
|       39 |  5943 | `		if( pValTok >= pBodyEnd ){` |
|      ! 0 |  5944 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5945 | `				"declare: Expecting value after '='");` |
|      ! 0 |  5946 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5947 | `			return SXRET_OK;` |
|        - |  5948 | `		}` |
|       39 |  5949 | `		pDirName = &pNameTok->sData;` |
|       39 |  5950 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|       39 |  5951 | `		if( bIsStrict ){` |
|        - |  5952 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|        - |  5953 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|       35 |  5954 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      ! 0 |  5955 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5956 | `					"declare(strict_types) value must be a literal");` |
|      ! 0 |  5957 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5958 | `				return SXRET_OK;` |
|        - |  5959 | `			}` |
|       35 |  5960 | `			iStrictValue = -1;` |
|       35 |  5961 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|       35 |  5962 | `				const char *zv = SyStringData(&pValTok->sData);` |
|       35 |  5963 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|       35 |  5964 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|       33 |  5965 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|       15 |  5966 | `			}` |
|       35 |  5967 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|        3 |  5968 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5969 | `					"strict_types declaration must have 0 or 1 as its value");` |
|        3 |  5970 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 |  5971 | `				return SXRET_OK;` |
|        - |  5972 | `			}` |
|       32 |  5973 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|       18 |  5974 | `		}else{` |
|        - |  5975 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|        - |  5976 | `			 * preserve the legacy notice so callers relying on the old` |
|        - |  5977 | `			 * behavior don't regress. */` |
|        8 |  5978 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|        - |  5979 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|        2 |  5980 | `				ph7_lib_version()` |
|        - |  5981 | `				);` |
|        - |  5982 | `		}` |
|       36 |  5983 | `		pCursor = pValTok + 1;` |
|        - |  5984 | `		/* Consume separating comma (or end). */` |
|       36 |  5985 | `		if( pCursor < pBodyEnd ){` |
|        3 |  5986 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 |  5987 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5988 | `					"declare: Expecting ',' or ')' after directive value");` |
|      ! 0 |  5989 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5990 | `				return SXRET_OK;` |
|        - |  5991 | `			}` |
|        3 |  5992 | `			pCursor++;` |
|        1 |  5993 | `		}` |
|        4 |  5994 | `	}` |
|        - |  5995 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|        - |  5996 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|        - |  5997 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|       34 |  5998 | `	return SXRET_OK;` |
|        2 |  5999 | `Synchro:` |
|        - |  6000 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|       16 |  6001 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       12 |  6002 | `		pGen->pIn++;` |
|        2 |  6003 | `	}` |
|        6 |  6004 | `	return SXRET_OK;` |
|       26 |  6005 | `}` |
|        - |  6006 | `/*` |
|        - |  6007 | ` * Process default argument values. That is,a function may define C++-style default value` |
|        - |  6008 | ` * as follows:` |
|        - |  6009 | ` * function makecoffee($type = "cappuccino")` |
|        - |  6010 | ` * {` |
|        - |  6011 | ` *   return "Making a cup of $type.\n";` |
|        - |  6012 | ` * }` |
|        - |  6013 | ` * Symisc eXtension.` |
|        - |  6014 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|        - |  6015 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|        - |  6016 | ` *      Example: Work only with PH7,generate error under zend` |
|        - |  6017 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|        - |  6018 | ` *      {` |
|        - |  6019 | ` *       var_dump($a);` |
|        - |  6020 | ` *      }` |
|        - |  6021 | ` *     //call test without args` |
|        - |  6022 | ` *      test();` |
|        - |  6023 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|        - |  6024 | ` *      Example:` |
|        - |  6025 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|        - |  6026 | ` * 3 -) Function overloading!!` |
|        - |  6027 | ` *      Example:` |
|        - |  6028 | ` *      function foo($a) {` |
|        - |  6029 | ` *   	  return $a.PHP_EOL;` |
|        - |  6030 | ` *	    }` |
|        - |  6031 | ` *	    function foo($a, $b) {` |
|        - |  6032 | ` *   	  return $a + $b;` |
|        - |  6033 | ` *	    }` |
|        - |  6034 | ` *	    echo foo(5); // Prints "5"` |
|        - |  6035 | ` *	    echo foo(5, 2); // Prints "7"` |
|        - |  6036 | ` *      // Same arg` |
|        - |  6037 | ` *	   function foo(string $a)` |
|        - |  6038 | ` *	   {` |
|        - |  6039 | ` *	     echo "a is a string\n";` |
|        - |  6040 | ` *	     var_dump($a);` |
|        - |  6041 | ` *	   }` |
|        - |  6042 | ` *	  function foo(int $a)` |
|        - |  6043 | ` *	  {` |
|        - |  6044 | ` *	    echo "a is integer\n";` |
|        - |  6045 | ` *	    var_dump($a);` |
|        - |  6046 | ` *	  }` |
|        - |  6047 | ` *	  function foo(array $a)` |
|        - |  6048 | ` *	  {` |
|        - |  6049 | ` * 	    echo "a is an array\n";` |
|        - |  6050 | ` * 	    var_dump($a);` |
|        - |  6051 | ` *	  }` |
|        - |  6052 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|        - |  6053 | ` *	  foo(52); // a is integer [second foo]` |
|        - |  6054 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|        - |  6055 | ` * Please refer to the official documentation for more information on the powerful extension` |
|        - |  6056 | ` * introduced by the PH7 engine.` |
|        - |  6057 | ` */` |
|   399386 |  6058 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|        5 |  6059 | `{` |
|        - |  6060 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |  6061 | `	SySet *pInstrContainer;` |
|        - |  6062 | `	sxi32 rc;` |
|        - |  6063 | `	/* Swap token stream */` |
|   399391 |  6064 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   399391 |  6065 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   399391 |  6066 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|        - |  6067 | `	/* Compile the expression holding the argument value */` |
|   399391 |  6068 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  6069 | `	/* Emit the done instruction */` |
|   399391 |  6070 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   399391 |  6071 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   399391 |  6072 | `	RE_SWAP_DELIMITER(pGen);` |
|   399391 |  6073 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  6074 | `		return SXERR_ABORT;` |
|        - |  6075 | `	}` |
|   399391 |  6076 | `	return SXRET_OK;` |
|   199698 |  6077 | `}` |
|        - |  6078 | `/*` |
|        - |  6079 | ` * Collect function arguments one after one.` |
|        - |  6080 | ` * According to the PHP language reference manual.` |
|        - |  6081 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|        - |  6082 | ` * list of expressions.` |
|        - |  6083 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|        - |  6084 | ` * and default argument values. Variable-length argument lists are also supported,` |
|        - |  6085 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|        - |  6086 | ` * for more information.` |
|        - |  6087 | ` * Example #1 Passing arrays to functions` |
|        - |  6088 | ` * <?php` |
|        - |  6089 | ` * function takes_array($input)` |
|        - |  6090 | ` * {` |
|        - |  6091 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|        - |  6092 | ` * }` |
|        - |  6093 | ` * ?>` |
|        - |  6094 | ` * Making arguments be passed by reference` |
|        - |  6095 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|        - |  6096 | ` * within the function is changed, it does not get changed outside of the function).` |
|        - |  6097 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|        - |  6098 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|        - |  6099 | ` * to the argument name in the function definition:` |
|        - |  6100 | ` * Example #2 Passing function parameters by reference` |
|        - |  6101 | ` * <?php` |
|        - |  6102 | ` * function add_some_extra(&$string)` |
|        - |  6103 | ` * {` |
|        - |  6104 | ` *   $string .= 'and something extra.';` |
|        - |  6105 | ` * }` |
|        - |  6106 | ` * $str = 'This is a string, ';` |
|        - |  6107 | ` * add_some_extra($str);` |
|        - |  6108 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|        - |  6109 | ` * ?>` |
|        - |  6110 | ` *` |
|        - |  6111 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|        - |  6112 | ` * complex agrument values.Please refer to the official documentation for more information` |
|        - |  6113 | ` * on these extension.` |
|        - |  6114 | ` */` |
|   846860 |  6115 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|        5 |  6116 | `{` |
|        - |  6117 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|        - |  6118 | `	SyToken *pIn;  /* Token stream */` |
|        - |  6119 | `	SyBlob sSig;         /* Function signature */` |
|        - |  6120 | `	char *zDup;          /* Copy of argument name */` |
|        - |  6121 | `	sxi32 rc;` |
|        - |  6122 |  |
|   846865 |  6123 | `	pIn = pGen->pIn;` |
|   846865 |  6124 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|        - |  6125 | `	/* Process arguments one after one */` |
|  1091513 |  6126 | `	for(;;){` |
|  2183031 |  6127 | `		if( pIn >= pEnd ){` |
|        - |  6128 | `			/* No more arguments to process */` |
|   846849 |  6129 | `			break;` |
|        - |  6130 | `		}` |
|  1336187 |  6131 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|  1336187 |  6132 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|  1336187 |  6133 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|  1336187 |  6134 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|  1336187 |  6135 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|        - |  6136 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|        - |  6137 | `		 * first token inside the main token stream */` |
|  1336187 |  6138 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  6139 | `			return SXERR_ABORT;` |
|        - |  6140 | `		}` |
|        - |  6141 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|        - |  6142 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|        - |  6143 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|        - |  6144 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|        - |  6145 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|        - |  6146 | `		{` |
|  1336187 |  6147 | `			int bReadonly = 0, bVisSeen = 0;` |
|  1336187 |  6148 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|  1336187 |  6149 | `			sxi32 iSetVisFlag = 0;` |
|        - |  6150 | `			int nSetTok;` |
|        - |  6151 | `			sxi32 nSetVis;` |
|  1336187 |  6152 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|        3 |  6153 | `				bReadonly = 1;` |
|        3 |  6154 | `				pIn++;` |
|        1 |  6155 | `			}` |
|  1336187 |  6156 | `			nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|  1336187 |  6157 | `			if( nSetVis ){` |
|        - |  6158 | ``				/* Leading `private(set)` etc: promoted with a public read side */`` |
|        3 |  6159 | `				iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|        3 |  6160 | `				bVisSeen = 1;` |
|        3 |  6161 | `				pIn += nSetTok;` |
|        3 |  6162 | `				if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|      ! 0 |  6163 | `					bReadonly = 1;` |
|      ! 0 |  6164 | `					pIn++;` |
|        1 |  6165 | `				}` |
|  1336186 |  6166 | `			}else if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|    89547 |  6167 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|    89547 |  6168 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|       89 |  6169 | `					bVisSeen = 1;` |
|       89 |  6170 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|      120 |  6171 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|       39 |  6172 | `						: PH7_CLASS_PROT_PUBLIC;` |
|       89 |  6173 | `					pIn++;` |
|       89 |  6174 | `					nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|       89 |  6175 | `					if( nSetVis ){` |
|        - |  6176 | ``						/* `public private(set) T $x` promoted form */`` |
|        3 |  6177 | `						iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|        3 |  6178 | `						pIn += nSetTok;` |
|        1 |  6179 | `					}` |
|       89 |  6180 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       18 |  6181 | `						bReadonly = 1;` |
|       18 |  6182 | `						pIn++;` |
|        7 |  6183 | `					}` |
|       42 |  6184 | `				}` |
|    44771 |  6185 | `			}` |
|  1336187 |  6186 | `			if( iSetVisFlag == PH7_CLASS_ATTR_PRIVATE_SET ){` |
|        5 |  6187 | `				sArg.iFlags \|= VM_FUNC_ARG_PRIV_SET;` |
|  1336185 |  6188 | `			}else if( iSetVisFlag == PH7_CLASS_ATTR_PROTECTED_SET ){` |
|      ! 0 |  6189 | `				sArg.iFlags \|= VM_FUNC_ARG_PROT_SET;` |
|      ! 0 |  6190 | `			}` |
|  1336187 |  6191 | `			if( bVisSeen \|\| bReadonly ){` |
|       93 |  6192 | `				if( !bCtorCtx ){` |
|        6 |  6193 | `					if( bAbstractCtx ){` |
|        3 |  6194 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|        - |  6195 | `							"Cannot declare promoted property in an abstract constructor");` |
|        2 |  6196 | `					}else{` |
|        3 |  6197 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|        - |  6198 | `							"Cannot declare promoted property outside a constructor");` |
|        - |  6199 | `					}` |
|        6 |  6200 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  6201 | `						return SXERR_ABORT;` |
|        - |  6202 | `					}` |
|        6 |  6203 | `					return SXERR_SYNTAX;` |
|        - |  6204 | `				}` |
|       89 |  6205 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|       89 |  6206 | `				sArg.iPromoteVis = iVis;` |
|       89 |  6207 | `				if( bReadonly ){` |
|       20 |  6208 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|        8 |  6209 | `				}` |
|       42 |  6210 | `			}` |
|        - |  6211 | `		}` |
|        - |  6212 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|  1336178 |  6213 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   736232 |  6214 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   130461 |  6215 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   105169 |  6216 | `			sxu32 nLineLocal = pIn->nLine;` |
|   105169 |  6217 | `			sxi32 iTFlags = 0;` |
|   105169 |  6218 | `			pGen->pIn = pIn;` |
|   105169 |  6219 | `			rc = GenStateParseUnionTypeDecl(` |
|    52582 |  6220 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|    52582 |  6221 | `				&iTFlags, &sArg.sTypeName,` |
|        - |  6222 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|        - |  6223 | `				/* bAllowVoid */ 0,` |
|    52582 |  6224 | `						nLineLocal);` |
|   105169 |  6225 | `			pIn = pGen->pIn;` |
|   105169 |  6226 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  6227 | `				return SXERR_ABORT;` |
|   105169 |  6228 | `			}else if( rc == SXERR_CORRUPT ){` |
|        - |  6229 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|        3 |  6230 | `				return SXERR_SYNTAX;` |
|   105167 |  6231 | `			}else if( rc == SXERR_SYNTAX ){` |
|       11 |  6232 | `				if( pIn < pEnd ){` |
|       15 |  6233 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|        - |  6234 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|        4 |  6235 | `						&pIn->sData);` |
|        7 |  6236 | `				}else{` |
|      ! 0 |  6237 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|        - |  6238 | `						"syntax error, unexpected end of file");` |
|        - |  6239 | `				}` |
|       11 |  6240 | `				return SXERR_SYNTAX;` |
|        - |  6241 | `			}` |
|   105159 |  6242 | `			sArg.iFlags \|= iTFlags;` |
|    52577 |  6243 | `		}` |
|  1336173 |  6244 | `		if( pIn >= pEnd ){` |
|      ! 0 |  6245 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|      ! 0 |  6246 | `			return rc;` |
|        - |  6247 | `		}` |
|  1336173 |  6248 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|        - |  6249 | `			/* Pass by reference,record that */` |
|    11673 |  6250 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    11673 |  6251 | `			pIn++;` |
|     5834 |  6252 | `		}` |
|  1336173 |  6253 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|        - |  6254 | `			/* Variadic parameter: ...$args */` |
|    19489 |  6255 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|    19489 |  6256 | `			pIn++;` |
|     9742 |  6257 | `		}` |
|  1336173 |  6258 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  6259 | `			/* Invalid argument */` |
|      ! 0 |  6260 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|      ! 0 |  6261 | `			return rc;` |
|        - |  6262 | `		}` |
|  1336173 |  6263 | `		pIn++; /* Jump the dollar sign */` |
|        - |  6264 | `		/* Copy argument name */` |
|  1336173 |  6265 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|  1336173 |  6266 | `		if( zDup == 0 ){` |
|      ! 0 |  6267 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  6268 | `			return SXERR_ABORT;` |
|        - |  6269 | `		}` |
|  1336173 |  6270 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|  1336173 |  6271 | `		pIn++;` |
|  1336173 |  6272 | `		if( pIn < pEnd ){` |
|   737507 |  6273 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|        - |  6274 | `				SyToken *pDefend;` |
|   399393 |  6275 | `				sxi32 iNest = 0;` |
|   399393 |  6276 | `				pIn++; /* Jump the equal sign */` |
|   399393 |  6277 | `				pDefend = pIn;` |
|        - |  6278 | `				/* Process the default value associated with this argument */` |
|   841443 |  6279 | `				while( pDefend < pEnd ){` |
|   593269 |  6280 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   151219 |  6281 | `						break;` |
|        - |  6282 | `					}` |
|   442055 |  6283 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|        - |  6284 | `						/* Increment nesting level */` |
|    23269 |  6285 | `						iNest++;` |
|   430423 |  6286 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|        - |  6287 | `						/* Decrement nesting level */` |
|    23269 |  6288 | `						iNest--;` |
|    11632 |  6289 | `					}` |
|   442055 |  6290 | `					pDefend++;` |
|        5 |  6291 | `				}` |
|   399393 |  6292 | `				if( pIn >= pDefend ){` |
|        3 |  6293 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|        3 |  6294 | `					return rc;` |
|        - |  6295 | `				}` |
|        - |  6296 | `				/* Process default value */` |
|   399391 |  6297 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   399391 |  6298 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  6299 | `					return rc;` |
|        - |  6300 | `				}` |
|        - |  6301 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|        - |  6302 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|        - |  6303 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|        - |  6304 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|        - |  6305 | `				 * arg-type check lets null through. */` |
|   399386 |  6306 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|   224910 |  6307 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|   224907 |  6308 | `					&& &pIn[1] == pDefend` |
|    46551 |  6309 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|    34906 |  6310 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|    21329 |  6311 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|    15515 |  6312 | `					sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|        - |  6313 | `					/* php 8.4: the implicit form is deprecated at COMPILE time —` |
|        - |  6314 | `` 					 * `f(): Implicitly marking parameter $x as nullable …` `` |
|        - |  6315 | `					 * (methods carry the Class:: prefix when the class link is` |
|        - |  6316 | `					 * already up at this point). */` |
|        - |  6317 | `					{` |
|    15515 |  6318 | `						const char *zSep = "";` |
|    15515 |  6319 | `						SyString sCls = { "", 0 };` |
|    15515 |  6320 | `						if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|    15509 |  6321 | `							sCls = ((ph7_class *)pFunc->pUserData)->sName;` |
|    15509 |  6322 | `							zSep = "::";` |
|     7752 |  6323 | `						}` |
|    23270 |  6324 | `						PH7_GenCompileError(&(*pGen),8192 /* E_DEPRECATED */,pIn->nLine,` |
|        - |  6325 | `							"%z%s%z(): Implicitly marking parameter $%z as nullable is deprecated, the explicit nullable type must be used instead",` |
|     7755 |  6326 | `							&sCls,zSep,&pFunc->sName,&sArg.sName);` |
|        - |  6327 | `					}` |
|     7755 |  6328 | `				}` |
|        - |  6329 | `				/* Point beyond the default value */` |
|   399391 |  6330 | `				pIn = pDefend;` |
|   199693 |  6331 | `			}` |
|   737505 |  6332 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 |  6333 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|      ! 0 |  6334 | `				return rc;` |
|        - |  6335 | `			}` |
|   737505 |  6336 | `			pIn++; /* Jump the trailing comma */` |
|   368750 |  6337 | `		}` |
|        - |  6338 | `		/* Append argument signature */` |
|  1336171 |  6339 | `		if( sArg.nType > 0 ){` |
|   105097 |  6340 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|        - |  6341 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    15589 |  6342 | `				int marker = 'o';` |
|    15589 |  6343 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    15589 |  6344 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|     7797 |  6345 | `			}else{` |
|        - |  6346 | `				int c;` |
|    89513 |  6347 | `				c = 'n'; /* cc warning */` |
|        - |  6348 | `				/* Type leading character */` |
|    89513 |  6349 | `				switch(sArg.nType){` |
|     5820 |  6350 | `				case MEMOBJ_HASHMAP:` |
|        - |  6351 | `					/* Hashmap aka 'array' */` |
|    11645 |  6352 | `					c = 'h';` |
|    11645 |  6353 | `					break;` |
|     9810 |  6354 | `				case MEMOBJ_INT:` |
|        - |  6355 | `					/* Integer */` |
|    19625 |  6356 | `					c = 'i';` |
|    19625 |  6357 | `					break;` |
|        2 |  6358 | `				case MEMOBJ_BOOL:` |
|        - |  6359 | `					/* Bool */` |
|        5 |  6360 | `					c = 'b';` |
|        5 |  6361 | `					break;` |
|        5 |  6362 | `				case MEMOBJ_REAL:` |
|        - |  6363 | `					/* Float */` |
|       12 |  6364 | `					c = 'f';` |
|       12 |  6365 | `					break;` |
|    29109 |  6366 | `				case MEMOBJ_STRING:` |
|        - |  6367 | `					/* String */` |
|    58223 |  6368 | `					c = 's';` |
|    58223 |  6369 | `					break;` |
|        7 |  6370 | `				case MEMOBJ_OBJ:` |
|        - |  6371 | `					/* Object */` |
|       16 |  6372 | `					c = 'o';` |
|       14 |  6373 | `					break;` |
|        1 |  6374 | `				default:` |
|        2 |  6375 | `					break;` |
|        - |  6376 | `				}` |
|    89513 |  6377 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|        - |  6378 | `			}` |
|    52551 |  6379 | `		}else{` |
|        - |  6380 | `			/* No type is associated with this parameter which mean` |
|        - |  6381 | `			 * that this function is not condidate for overloading.` |
|        - |  6382 | `			 */` |
|  1231079 |  6383 | `			SyBlobRelease(&sSig);` |
|        - |  6384 | `		}` |
|        - |  6385 | `		/* Save in the argument set */` |
|  1336171 |  6386 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|        5 |  6387 | `	}` |
|   846849 |  6388 | `	if( SyBlobLength(&sSig) > 0 ){` |
|        - |  6389 | `		/* Save function signature */` |
|    74017 |  6390 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|    37006 |  6391 | `	}` |
|   846849 |  6392 | `	return SXRET_OK;` |
|   423435 |  6393 | `}` |
|        - |  6394 | `/*` |
|        - |  6395 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|        - |  6396 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|        - |  6397 | ` * the enclosing function. Returns the token just past the nested construct.` |
|        - |  6398 | ` */` |
|    34926 |  6399 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|        5 |  6400 | `{` |
|    34931 |  6401 | `	sxi32 iParen = 0;` |
|    34931 |  6402 | `	pIn++; /* past 'function'/'fn' */` |
|        - |  6403 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|        - |  6404 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|        - |  6405 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|   155273 |  6406 | `	while( pIn < pEnd ){` |
|   155273 |  6407 | `		sxu32 t = pIn->nType;` |
|   155273 |  6408 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|   151343 |  6409 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|   104777 |  6410 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|    85355 |  6411 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|   120347 |  6412 | `		pIn++;` |
|        5 |  6413 | `	}` |
|    19427 |  6414 | `	if( pIn >= pEnd ){ return pIn; }` |
|        - |  6415 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|        - |  6416 | `	{` |
|    19427 |  6417 | `		sxi32 d = 0;` |
|   771749 |  6418 | `		while( pIn < pEnd ){` |
|   771749 |  6419 | `			sxu32 t = pIn->nType;` |
|   771749 |  6420 | `			if( t & PH7_TK_OCB ){ d++; }` |
|   740695 |  6421 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|   752327 |  6422 | `			pIn++;` |
|        5 |  6423 | `		}` |
|        - |  6424 | `	}` |
|    19427 |  6425 | `	return pIn;` |
|    17468 |  6426 | `}` |
|        - |  6427 | `/*` |
|        - |  6428 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|        - |  6429 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|        - |  6430 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|        - |  6431 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|        - |  6432 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|        - |  6433 | ` * detached-mini-program path untouched.` |
|        - |  6434 | ` */` |
|        - |  6435 | `/*` |
|        - |  6436 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|        - |  6437 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|        - |  6438 | ` * mixed, object.` |
|        - |  6439 | ` */` |
|     3904 |  6440 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|        5 |  6441 | `{` |
|        - |  6442 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|        - |  6443 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|        - |  6444 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|        - |  6445 | `	};` |
|        - |  6446 | `	sxu32 i;` |
|     3909 |  6447 | `	if( nName > 0 && zName[0] == '\\' ){` |
|      ! 0 |  6448 | `		zName++;` |
|      ! 0 |  6449 | `		nName--;` |
|      ! 0 |  6450 | `	}` |
|     3941 |  6451 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|     3937 |  6452 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|     3905 |  6453 | `			return 1;` |
|        - |  6454 | `		}` |
|       17 |  6455 | `	}` |
|        5 |  6456 | `	return 0;` |
|     1957 |  6457 | `}` |
|        - |  6458 | `/*` |
|        - |  6459 | ` * One atom of a generator's declared return type: is it a supertype of` |
|        - |  6460 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|        - |  6461 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|        - |  6462 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|        - |  6463 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|        - |  6464 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|        - |  6465 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|        - |  6466 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|        - |  6467 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|        - |  6468 | ` * both rather than fatal on valid code (a recorded divergence).` |
|        - |  6469 | ` */` |
|     3902 |  6470 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|        5 |  6471 | `{` |
|     3907 |  6472 | `	if( nType == MEMOBJ_OBJ ){` |
|      ! 0 |  6473 | ``		return 1; /* bare `object` */`` |
|        - |  6474 | `	}` |
|     3907 |  6475 | `	if( nType != SXU32_HIGH ){` |
|        3 |  6476 | `		return 0; /* scalar/array/void/never/null/... */` |
|        - |  6477 | `	}` |
|     3905 |  6478 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|     3901 |  6479 | `		return 1;` |
|        - |  6480 | `	}` |
|        - |  6481 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|        - |  6482 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|        - |  6483 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|        - |  6484 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|        - |  6485 | `	{` |
|        - |  6486 | `		SyBlob sFQN;` |
|        - |  6487 | `		int bOk;` |
|        5 |  6488 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        5 |  6489 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|        5 |  6490 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|        5 |  6491 | `		SyBlobRelease(&sFQN);` |
|        5 |  6492 | `		return bOk;` |
|        - |  6493 | `	}` |
|     1956 |  6494 | `}` |
|        - |  6495 | `/*` |
|        - |  6496 | ` * php 8: a generator function may only declare a return type that is a` |
|        - |  6497 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|        - |  6498 | ` * group qualifies only if every member does. Anything else is php's exact` |
|        - |  6499 | ` * compile-time fatal "Generator return type must be a supertype of` |
|        - |  6500 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|        - |  6501 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|        - |  6502 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|        - |  6503 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|        - |  6504 | ` */` |
|     4140 |  6505 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|        5 |  6506 | `{` |
|     4145 |  6507 | `	int bOk = 0;` |
|        - |  6508 | `	sxu32 nLine;` |
|        - |  6509 | `	sxi32 rc;` |
|     4145 |  6510 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|      243 |  6511 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|        - |  6512 | `	}` |
|     3907 |  6513 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|      ! 0 |  6514 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  6515 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|        - |  6516 | `		sxu32 i,j;` |
|      ! 0 |  6517 | `		for( i = 0; i < n && !bOk; i++ ){` |
|        - |  6518 | `			int bGroupOk;` |
|      ! 0 |  6519 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|      ! 0 |  6520 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|        - |  6521 | `			}` |
|      ! 0 |  6522 | `			bGroupOk = 1;` |
|      ! 0 |  6523 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|      ! 0 |  6524 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|      ! 0 |  6525 | `					bGroupOk = 0;` |
|      ! 0 |  6526 | `					break;` |
|        - |  6527 | `				}` |
|      ! 0 |  6528 | `			}` |
|      ! 0 |  6529 | `			bOk = bGroupOk;` |
|      ! 0 |  6530 | `		}` |
|      ! 0 |  6531 | `	}else{` |
|     3907 |  6532 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|        - |  6533 | `	}` |
|     3907 |  6534 | `	if( bOk ){` |
|     3905 |  6535 | `		return SXRET_OK;` |
|        - |  6536 | `	}` |
|        - |  6537 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|        - |  6538 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|        - |  6539 | `	 * token of this stream — its line is the function's closing brace. php` |
|        - |  6540 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|        - |  6541 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|        3 |  6542 | `	nLine = pGen->pIn[-1].nLine;` |
|        - |  6543 | `	{` |
|        3 |  6544 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|        3 |  6545 | `		if( sGiven.nByte < 1 ){` |
|      ! 0 |  6546 | `			sGiven = pFunc->sReturnClass;` |
|      ! 0 |  6547 | `		}` |
|        3 |  6548 | `		if( sGiven.nByte < 1 ){` |
|        - |  6549 | ``			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the`` |
|        - |  6550 | `			 * rendered type text, so sReturnTypeName arrives empty for them —` |
|        - |  6551 | `			 * name them here (the root fix belongs to that renderer, §3.7). */` |
|      ! 0 |  6552 | `			const char *zScalar =` |
|      ! 0 |  6553 | `				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :` |
|      ! 0 |  6554 | `				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";` |
|      ! 0 |  6555 | `			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));` |
|      ! 0 |  6556 | `		}` |
|        3 |  6557 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  6558 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|        - |  6559 | `	}` |
|        3 |  6560 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|     2075 |  6561 | `}` |
|  1995780 |  6562 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|        5 |  6563 | `{` |
|  1995785 |  6564 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|  1995785 |  6565 | `	SyToken *pEnd = pGen->pEnd;` |
|  1995785 |  6566 | `	sxi32 iDepth = 0;` |
|  1995785 |  6567 | `	int bStarted = 0;` |
| 87459089 |  6568 | `	while( pIn < pEnd ){` |
| 87459089 |  6569 | `		sxu32 t = pIn->nType;` |
| 87459089 |  6570 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 83454581 |  6571 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 79465955 |  6572 | `		if( t & PH7_TK_KEYWORD ){` |
|  5927777 |  6573 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|  5927777 |  6574 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|  5923637 |  6575 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|        - |  6576 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|  2944353 |  6577 | `		}` |
| 79426889 |  6578 | `		pIn++;` |
|        5 |  6579 | `	}` |
|  1991645 |  6580 | `	return FALSE;` |
|   997895 |  6581 | `}` |
|        - |  6582 | `/*` |
|        - |  6583 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|        - |  6584 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|        - |  6585 | ` * and this routine takes care of generating the appropriate error message.` |
|        - |  6586 | ` */` |
|  1995780 |  6587 | `static sxi32 GenStateCompileFuncBody(` |
|        - |  6588 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - |  6589 | `	ph7_vm_func *pFunc    /* Function state */` |
|        - |  6590 | `	)` |
|        5 |  6591 | `{` |
|        - |  6592 | `	SySet *pInstrContainer; /* Instruction container */` |
|        - |  6593 | `	GenBlock *pBlock;` |
|        - |  6594 | `	sxu32 nGotoOfft;` |
|        - |  6595 | `	sxi32 rc;` |
|        - |  6596 | `	/* Attach the new function */` |
|  1995785 |  6597 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  1995785 |  6598 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  6599 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|        - |  6600 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  6601 | `		return SXERR_ABORT;` |
|        - |  6602 | `	}` |
|  1995785 |  6603 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|        - |  6604 | `	/* Swap bytecode containers */` |
|  1995785 |  6605 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  1995785 |  6606 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|        - |  6607 | `	/* Emit constructor property promotion prologue:` |
|        - |  6608 | `	 *   $this->NAME = $NAME;` |
|        - |  6609 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|        - |  6610 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|        - |  6611 | `	{` |
|  1995785 |  6612 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|        - |  6613 | `		sxu32 i;` |
|  3296941 |  6614 | `		for( i = 0; i < nArg; i++ ){` |
|  1301161 |  6615 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|        - |  6616 | `			char *zSrc;` |
|        - |  6617 | `			sxu32 nSrc,nName;` |
|        - |  6618 | `			SySet sToken;` |
|        - |  6619 | `			SyToken *pTmpIn,*pTmpEnd;` |
|        - |  6620 | `			sxi32 rcPromote;` |
|  1301161 |  6621 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  1301087 |  6622 | `				continue;` |
|        - |  6623 | `			}` |
|        - |  6624 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|        - |  6625 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|        - |  6626 | `			 * copied), so it must outlive the function — never free it. The` |
|        - |  6627 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|        - |  6628 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|       79 |  6629 | `			nName = SyStringLength(&pArg->sName);` |
|       79 |  6630 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|       79 |  6631 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|       79 |  6632 | `			if( zSrc == 0 ){` |
|      ! 0 |  6633 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  6634 | `				GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  6635 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  6636 | `				return SXERR_ABORT;` |
|        - |  6637 | `			}` |
|        - |  6638 | `			{` |
|       79 |  6639 | `				char *z = zSrc;` |
|       79 |  6640 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|       79 |  6641 | `				z += sizeof("$this->")-1;` |
|       79 |  6642 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|       79 |  6643 | `				z += nName;` |
|       79 |  6644 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|       79 |  6645 | `				z += sizeof(" = $")-1;` |
|       79 |  6646 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|       79 |  6647 | `				z += nName;` |
|       79 |  6648 | `				*z = 0;` |
|        - |  6649 | `			}` |
|       79 |  6650 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       79 |  6651 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);` |
|       79 |  6652 | `			pTmpIn = pGen->pIn;` |
|       79 |  6653 | `			pTmpEnd = pGen->pEnd;` |
|       79 |  6654 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|       79 |  6655 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       79 |  6656 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|       79 |  6657 | `			pGen->pIn = pTmpIn;` |
|       79 |  6658 | `			pGen->pEnd = pTmpEnd;` |
|       79 |  6659 | `			SySetRelease(&sToken);` |
|       79 |  6660 | `			if( rcPromote == SXERR_ABORT ){` |
|      ! 0 |  6661 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  6662 | `				GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  6663 | `				return SXERR_ABORT;` |
|        - |  6664 | `			}` |
|        - |  6665 | `			/* Discard the assignment result — this is a statement expression. */` |
|       79 |  6666 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       42 |  6667 | `		}` |
|        - |  6668 | `	}` |
|        - |  6669 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|        - |  6670 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|        - |  6671 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|        - |  6672 | `	 * generator — and vice versa — is classified independently. */` |
|        - |  6673 | `	{` |
|  1995785 |  6674 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|  1995785 |  6675 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|        - |  6676 | `		/* Compile the body */` |
|  1995785 |  6677 | `		PH7_CompileBlock(&(*pGen),0);` |
|  1995785 |  6678 | `		pGen->bInGenerator = bSavedGen;` |
|        - |  6679 | `	}` |
|        - |  6680 | `	/* Fix exception jumps now the destination is resolved */` |
|  1995785 |  6681 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        - |  6682 | `	/* Emit the final return if not yet done */` |
|  1995785 |  6683 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - |  6684 | `	/* Fix gotos jumps now the destination is resolved */` |
|  1995785 |  6685 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|      ! 0 |  6686 | `		rc = SXERR_ABORT;` |
|      ! 0 |  6687 | `	}` |
|  1995785 |  6688 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|        - |  6689 | `	/* Restore the default container */` |
|  1995785 |  6690 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - |  6691 | `	/* Leave function block */` |
|  1995785 |  6692 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  1995785 |  6693 | `	if( rc == SXERR_ABORT ){` |
|        - |  6694 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  6695 | `		return SXERR_ABORT;` |
|        - |  6696 | `	}` |
|        - |  6697 | `	/* Scan for yield opcodes to detect generator functions */` |
|        - |  6698 | `	{` |
|  1995785 |  6699 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|        - |  6700 | `		sxu32 i;` |
| 54131409 |  6701 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 52139769 |  6702 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     4145 |  6703 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     4145 |  6704 | `				break;` |
|        - |  6705 | `			}` |
| 26067817 |  6706 | `		}` |
|        - |  6707 | `	}` |
|  1995785 |  6708 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  6709 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|     4145 |  6710 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|      ! 0 |  6711 | `			return SXERR_ABORT;` |
|        - |  6712 | `		}` |
|     2070 |  6713 | `	}` |
|        - |  6714 | `	/* All done, function body compiled */` |
|  1995785 |  6715 | `	return SXRET_OK;` |
|   997895 |  6716 | `}` |
|        - |  6717 | `/*` |
|        - |  6718 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|        - |  6719 | ` * According to the PHP language reference manual.` |
|        - |  6720 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|        - |  6721 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|        - |  6722 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|        - |  6723 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|        - |  6724 | ` *  Functions need not be defined before they are referenced.` |
|        - |  6725 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|        - |  6726 | ` *  a function even if they were defined inside and vice versa.` |
|        - |  6727 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|        - |  6728 | ` *  calls with over 32-64 recursion levels.` |
|        - |  6729 | ` *` |
|        - |  6730 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|        - |  6731 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|        - |  6732 | ` * on these extension.` |
|        - |  6733 | ` */` |
|        - |  6734 | `/*` |
|        - |  6735 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|        - |  6736 | ` */` |
|      570 |  6737 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|        5 |  6738 | `{` |
|        - |  6739 | `	sxu32 i;` |
|     1611 |  6740 | `	for( i = 0; i < n; i++ ){` |
|     1381 |  6741 | `		int a = zA[i], b = zB[i];` |
|     1381 |  6742 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|     1381 |  6743 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|     1381 |  6744 | `		if( a != b ) return a - b;` |
|      523 |  6745 | `	}` |
|      235 |  6746 | `	return 0;` |
|      290 |  6747 | `}` |
|        - |  6748 | `/*` |
|        - |  6749 | ` * Internal type-atom kinds used during union type parsing.` |
|        - |  6750 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|        - |  6751 | ` * (which are positive bit values stored in sxu32).` |
|        - |  6752 | ` */` |
|        - |  6753 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|        - |  6754 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|        - |  6755 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|        - |  6756 |  |
|        - |  6757 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|        - |  6758 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|        - |  6759 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|        - |  6760 |  |
|        - |  6761 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|        - |  6762 | `struct PhlTypeAtom {` |
|        - |  6763 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|        - |  6764 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|        - |  6765 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|        - |  6766 | `	sxu32 nCanon;` |
|        - |  6767 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|        - |  6768 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|        - |  6769 | `};` |
|        - |  6770 |  |
|        - |  6771 | `/*` |
|        - |  6772 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|        - |  6773 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|        - |  6774 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|        - |  6775 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|        - |  6776 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|        - |  6777 | ` * already be consumed by the caller.` |
|        - |  6778 | ` */` |
|   110218 |  6779 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|        5 |  6780 | `{` |
|   110223 |  6781 | `	SyToken *pIn = pGen->pIn;` |
|   110223 |  6782 | `	SyZero(pOut, sizeof(*pOut));` |
|   110223 |  6783 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   110223 |  6784 | `	if( pIn >= pGen->pEnd ){` |
|      ! 0 |  6785 | `		return SXERR_SYNTAX;` |
|        - |  6786 | `	}` |
|        - |  6787 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   110223 |  6788 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|        8 |  6789 | `		pIn++;` |
|        8 |  6790 | `		if( pIn >= pGen->pEnd ){` |
|      ! 0 |  6791 | `			return SXERR_SYNTAX;` |
|        - |  6792 | `		}` |
|        3 |  6793 | `	}` |
|   110223 |  6794 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  6795 | `		return SXERR_SYNTAX;` |
|        - |  6796 | `	}` |
|   110223 |  6797 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|    90303 |  6798 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|    90303 |  6799 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|    11677 |  6800 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|    84467 |  6801 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|       81 |  6802 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|    78593 |  6803 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|    20029 |  6804 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|    68543 |  6805 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|    58449 |  6806 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|    29309 |  6807 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|       41 |  6808 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|       67 |  6809 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|       27 |  6810 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|       37 |  6811 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       13 |  6812 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|       23 |  6813 | `			pOut->nType = SXU32_HIGH;` |
|       23 |  6814 | `			pOut->sClass = pIn->sData;` |
|       13 |  6815 | `		}else{` |
|        3 |  6816 | `			return SXERR_SYNTAX;` |
|        - |  6817 | `		}` |
|    90301 |  6818 | `		pIn++;` |
|    45153 |  6819 | `	}else{` |
|        - |  6820 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|        - |  6821 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|    19925 |  6822 | `		SyString *pT = &pIn->sData;` |
|    19925 |  6823 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|       34 |  6824 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|       34 |  6825 | `			pIn++;` |
|    19910 |  6826 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|      177 |  6827 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|      177 |  6828 | `			pIn++;` |
|    19809 |  6829 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       26 |  6830 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       26 |  6831 | `			pIn++;` |
|       15 |  6832 | `		}else{` |
|        - |  6833 | `			/* Class / interface name; consume namespace path a\b\c */` |
|    19701 |  6834 | `			SyToken *pFirst = pIn;` |
|    19701 |  6835 | `			SyToken *pLast = pIn;` |
|    19701 |  6836 | `			pOut->nType = SXU32_HIGH;` |
|    19701 |  6837 | `			pOut->sClass = pIn->sData;` |
|    19701 |  6838 | `			pIn++;` |
|    29547 |  6839 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|    19704 |  6840 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|        3 |  6841 | `				pLast = &pIn[1];` |
|        3 |  6842 | `				pIn += 2;` |
|        1 |  6843 | `			}` |
|    19701 |  6844 | `			if( pLast != pFirst ){` |
|        3 |  6845 | `				const char *zFirst = pFirst->sData.zString;` |
|        3 |  6846 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|        3 |  6847 | `				pOut->sClass.zString = zFirst;` |
|        3 |  6848 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|        1 |  6849 | `			}` |
|        - |  6850 | `		}` |
|        - |  6851 | `	}` |
|   110221 |  6852 | `	pGen->pIn = pIn;` |
|   110221 |  6853 | `	return SXRET_OK;` |
|    55114 |  6854 | `}` |
|        - |  6855 |  |
|        - |  6856 | `/*` |
|        - |  6857 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|        - |  6858 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|        - |  6859 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|        - |  6860 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|        - |  6861 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|        - |  6862 | ` */` |
|   110040 |  6863 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|        5 |  6864 | `{` |
|        - |  6865 | `	int i;` |
|   110045 |  6866 | `	int nNonNull = 0;` |
|   110045 |  6867 | `	int bAnyIntersection = 0;` |
|        - |  6868 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|   110045 |  6869 | `	sxu32 nMaxGroup = 0;` |
|  3631325 |  6870 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|   220237 |  6871 | `	for( i = 0; i < nAtoms; i++ ){` |
|   110197 |  6872 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   110167 |  6873 | `			nNonNull++;` |
|   110167 |  6874 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|   110167 |  6875 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|   110167 |  6876 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|    55081 |  6877 | `			}` |
|    55081 |  6878 | `		}` |
|    55101 |  6879 | `	}` |
|   220185 |  6880 | `	for( i = 0; i < nAtoms; i++ ){` |
|   110169 |  6881 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|       29 |  6882 | `			bAnyIntersection = 1;` |
|       29 |  6883 | `			break;` |
|        - |  6884 | `		}` |
|    55075 |  6885 | `	}` |
|   110045 |  6886 | `	if( bAnyIntersection ){` |
|        - |  6887 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|        - |  6888 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|        - |  6889 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|       29 |  6890 | `		sxu32 g, nGroups = 0;` |
|       29 |  6891 | `		int bFirstGroup = 1;` |
|       59 |  6892 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|       59 |  6893 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|       35 |  6894 | `			int bFirstMember = 1;` |
|        - |  6895 | `			int bWrap;` |
|       35 |  6896 | `			if( aGroupCount[g] == 0 ) continue;` |
|        - |  6897 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|        - |  6898 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|        - |  6899 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|        - |  6900 | `			 * parens, matching PHP's canonical text. */` |
|       47 |  6901 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|       35 |  6902 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|       35 |  6903 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|      107 |  6904 | `			for( i = 0; i < nAtoms; i++ ){` |
|       77 |  6905 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|       59 |  6906 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|       59 |  6907 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       55 |  6908 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       30 |  6909 | `				}else{` |
|        6 |  6910 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|        - |  6911 | `				}` |
|       59 |  6912 | `				bFirstMember = 0;` |
|       32 |  6913 | `			}` |
|       35 |  6914 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|       35 |  6915 | `			bFirstGroup = 0;` |
|       20 |  6916 | `		}` |
|       29 |  6917 | `		if( bNullable ){` |
|      ! 0 |  6918 | `			SyBlobAppend(pBlob, "\|", 1);` |
|      ! 0 |  6919 | `			SyBlobAppend(pBlob, "null", 4);` |
|      ! 0 |  6920 | `		}` |
|       83 |  6921 | `		return;` |
|        - |  6922 | `	}` |
|   110021 |  6923 | `	if( nNonNull == 1 && bNullable ){` |
|        - |  6924 | `		/* Shorthand: ?T */` |
|      112 |  6925 | `		for( i = 0; i < nAtoms; i++ ){` |
|      112 |  6926 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      112 |  6927 | `			SyBlobAppend(pBlob, "?", 1);` |
|      112 |  6928 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|       23 |  6929 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       13 |  6930 | `			}else{` |
|       92 |  6931 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|        - |  6932 | `			}` |
|      112 |  6933 | `			return;` |
|      ! 0 |  6934 | `		}` |
|      ! 0 |  6935 | `	}` |
|        - |  6936 | `	{` |
|   109913 |  6937 | `		int bFirst = 1;` |
|        - |  6938 | `		/* 1) Classes in declaration order */` |
|   219929 |  6939 | `		for( i = 0; i < nAtoms; i++ ){` |
|   110021 |  6940 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|    19651 |  6941 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    19651 |  6942 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|    19651 |  6943 | `				bFirst = 0;` |
|     9823 |  6944 | `			}` |
|    55013 |  6945 | `		}` |
|        - |  6946 | `		/* 2) Built-ins in canonical order */` |
|        - |  6947 | `		{` |
|        - |  6948 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|        - |  6949 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|        - |  6950 | `			int k;` |
|   769361 |  6951 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  1229269 |  6952 | `				for( i = 0; i < nAtoms; i++ ){` |
|   659989 |  6953 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|    90173 |  6954 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    90173 |  6955 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|    90173 |  6956 | `						bFirst = 0;` |
|    90173 |  6957 | `						break;` |
|        - |  6958 | `					}` |
|   284913 |  6959 | `				}` |
|   329729 |  6960 | `			}` |
|        - |  6961 | `		}` |
|        - |  6962 | `		/* 3) null suffix */` |
|   109913 |  6963 | `		if( bNullable ){` |
|       19 |  6964 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|       19 |  6965 | `			SyBlobAppend(pBlob, "null", 4);` |
|        8 |  6966 | `		}` |
|        - |  6967 | `	}` |
|    55025 |  6968 | `}` |
|        - |  6969 |  |
|        - |  6970 | `/*` |
|        - |  6971 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|        - |  6972 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|        - |  6973 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|        - |  6974 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|        - |  6975 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|        - |  6976 | ` * whether it was parenthesized.` |
|        - |  6977 | ` *` |
|        - |  6978 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|        - |  6979 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|        - |  6980 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|        - |  6981 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|        - |  6982 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|        - |  6983 | ` */` |
|   110192 |  6984 | `static sxi32 GenStateParsePart(` |
|        - |  6985 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|        - |  6986 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|        5 |  6987 | `{` |
|        - |  6988 | `	sxi32 rc;` |
|   110197 |  6989 | `	int nMembers = 0;` |
|   110197 |  6990 | `	int bParen = 0;` |
|   110197 |  6991 | `	*pnMembers = 0;` |
|   110197 |  6992 | `	*pbParen = 0;` |
|   110197 |  6993 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        9 |  6994 | `		bParen = 1;` |
|        9 |  6995 | `		pGen->pIn++; /* skip '(' */` |
|        3 |  6996 | `	}` |
|    55096 |  6997 | `	for(;;){` |
|   110223 |  6998 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|      ! 0 |  6999 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7000 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|      ! 0 |  7001 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  7002 | `		}` |
|   110223 |  7003 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|   110223 |  7004 | `		if( rc != SXRET_OK ){` |
|        3 |  7005 | `			return rc;` |
|        - |  7006 | `		}` |
|   110221 |  7007 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|   110221 |  7008 | `		(*pnAtoms)++;` |
|   110221 |  7009 | `		nMembers++;` |
|        - |  7010 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|   110221 |  7011 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       39 |  7012 | `			SyToken *pNext = &pGen->pIn[1];` |
|       34 |  7013 | `			if( pNext < pGen->pEnd` |
|       39 |  7014 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       31 |  7015 | `				pGen->pIn++; /* skip '&' */` |
|       31 |  7016 | `				continue;` |
|        - |  7017 | `			}` |
|        4 |  7018 | `		}` |
|   110195 |  7019 | `		break;` |
|      ! 0 |  7020 | `	}` |
|   110195 |  7021 | `	if( bParen ){` |
|        9 |  7022 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 |  7023 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7024 | `				"Malformed DNF type: expecting ')'");` |
|      ! 0 |  7025 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  7026 | `		}` |
|        9 |  7027 | `		pGen->pIn++; /* skip ')' */` |
|        9 |  7028 | `		if( nMembers < 2 ){` |
|      ! 0 |  7029 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7030 | `				"Parenthesized type must be an intersection of at least two types");` |
|      ! 0 |  7031 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  7032 | `		}` |
|        3 |  7033 | `	}` |
|   110195 |  7034 | `	*pnMembers = nMembers;` |
|   110195 |  7035 | `	*pbParen = bParen;` |
|   110195 |  7036 | `	return SXRET_OK;` |
|    55101 |  7037 | `}` |
|        - |  7038 |  |
|        - |  7039 | `/*` |
|        - |  7040 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|        - |  7041 | ` *` |
|        - |  7042 | ` * Outputs:` |
|        - |  7043 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|        - |  7044 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|        - |  7045 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|        - |  7046 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|        - |  7047 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|        - |  7048 | ` *     already be initialized by the caller (allocator set, etc).` |
|        - |  7049 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|        - |  7050 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|        - |  7051 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|        - |  7052 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|        - |  7053 | ` *` |
|        - |  7054 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|        - |  7055 | ` * SXERR_ABORT on fatal compile errors.` |
|        - |  7056 | ` */` |
|   110056 |  7057 | `static sxi32 GenStateParseUnionTypeDecl(` |
|        - |  7058 | `	ph7_gen_state *pGen,` |
|        - |  7059 | `	sxu32 *pnType,` |
|        - |  7060 | `	SyString *pClass,` |
|        - |  7061 | `	SySet *pAlts,` |
|        - |  7062 | `	sxi32 *piTypeFlags,` |
|        - |  7063 | `	SyString *pTypeText,` |
|        - |  7064 | `	int iNullableFlag,` |
|        - |  7065 | `	int iUnionFlag,` |
|        - |  7066 | `	int bAllowVoid,` |
|        - |  7067 | `	sxu32 nLine` |
|        5 |  7068 | `){` |
|        - |  7069 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   110061 |  7070 | `	int nAtoms = 0;` |
|   110061 |  7071 | `	int bShortNullable = 0;` |
|   110061 |  7072 | `	int bExplicitNull = 0;` |
|        - |  7073 | `	sxi32 rc;` |
|   110061 |  7074 | `	*pnType = 0;` |
|   110061 |  7075 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   110061 |  7076 | `	*piTypeFlags = 0;` |
|   110061 |  7077 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|        - |  7078 |  |
|   110061 |  7079 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  7080 | `		return SXRET_OK;` |
|        - |  7081 | `	}` |
|        - |  7082 | ``	/* Optional `?` shorthand prefix */`` |
|   110056 |  7083 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      101 |  7084 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      100 |  7085 | `		bShortNullable = 1;` |
|      100 |  7086 | `		pGen->pIn++;` |
|      100 |  7087 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  7088 | `			return SXERR_SYNTAX;` |
|        - |  7089 | `		}` |
|       48 |  7090 | `	}` |
|        - |  7091 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|        - |  7092 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|        - |  7093 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|        - |  7094 | `	{` |
|        - |  7095 | `		int nMembers, bParen;` |
|   110061 |  7096 | `		sxu32 iGroup = 0;` |
|   110061 |  7097 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|   110061 |  7098 | `		if( rc != SXRET_OK ){` |
|        4 |  7099 | `			return rc;` |
|        - |  7100 | `		}` |
|        - |  7101 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|        - |  7102 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|        - |  7103 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|        - |  7104 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|        - |  7105 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|   165290 |  7106 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   110268 |  7107 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      143 |  7108 | `			if( bShortNullable ){` |
|        - |  7109 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|        - |  7110 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|        - |  7111 | `				 * already reported" so callers skip their own error emission. */` |
|        3 |  7112 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|        - |  7113 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|        3 |  7114 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|        - |  7115 | `			}` |
|      141 |  7116 | `			if( nMembers >= 2 && !bParen ){` |
|      ! 0 |  7117 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|        - |  7118 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|      ! 0 |  7119 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  7120 | `			}` |
|      141 |  7121 | ``			pGen->pIn++; /* skip `\|` */`` |
|      141 |  7122 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|      141 |  7123 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  7124 | `				return rc;` |
|        - |  7125 | `			}` |
|        5 |  7126 | `		}` |
|   110057 |  7127 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|      ! 0 |  7128 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7129 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|      ! 0 |  7130 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  7131 | `		}` |
|        - |  7132 | `	}` |
|        - |  7133 | `	/* Validation pass.` |
|        - |  7134 | `	 *` |
|        - |  7135 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|        - |  7136 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|        - |  7137 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|        - |  7138 | `	 */` |
|        - |  7139 | `	{` |
|        - |  7140 | `		int i, j;` |
|   110057 |  7141 | `		int bHasNonNull = 0;` |
|   110057 |  7142 | `		int bAnyIntersection = 0;` |
|        - |  7143 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|        - |  7144 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|        - |  7145 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
|  3631721 |  7146 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|   220271 |  7147 | `		for( i = 0; i < nAtoms; i++ ){` |
|   110219 |  7148 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|    55112 |  7149 | `		}` |
|   220215 |  7150 | `		for( i = 0; i < nAtoms; i++ ){` |
|   110189 |  7151 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|    55084 |  7152 | `		}` |
|        - |  7153 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|        - |  7154 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|   110057 |  7155 | `		if( bShortNullable && bAnyIntersection ){` |
|      ! 0 |  7156 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7157 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|      ! 0 |  7158 | `			return SXERR_SYNTAX;` |
|        - |  7159 | `		}` |
|   220257 |  7160 | `		for( i = 0; i < nAtoms; i++ ){` |
|        - |  7161 | `			/* Intersection members must be class/interface types (PHP rejects` |
|        - |  7162 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|        - |  7163 | ``			 * `true`/`false` in an intersection). */`` |
|   110217 |  7164 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|       55 |  7165 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|       55 |  7166 | `				if( bClassLike ){` |
|       53 |  7167 | `					SyString *pC = &aAtoms[i].sClass;` |
|       48 |  7168 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|       48 |  7169 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|       48 |  7170 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|       53 |  7171 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|      ! 0 |  7172 | `						bClassLike = 0;` |
|      ! 0 |  7173 | `					}` |
|       24 |  7174 | `				}` |
|       55 |  7175 | `				if( !bClassLike ){` |
|        - |  7176 | `					const char *zName; sxu32 nName;` |
|        3 |  7177 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      ! 0 |  7178 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|      ! 0 |  7179 | `					}else{` |
|        3 |  7180 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|        - |  7181 | `					}` |
|        4 |  7182 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7183 | `						"Type %.*s cannot be part of an intersection type",` |
|        1 |  7184 | `						(int)nName, zName);` |
|        3 |  7185 | `					return SXERR_SYNTAX;` |
|        - |  7186 | `				}` |
|       24 |  7187 | `			}` |
|   110215 |  7188 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|      177 |  7189 | `				if( nAtoms > 1 ){` |
|        3 |  7190 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7191 | `						"Void can only be used as a standalone type");` |
|        3 |  7192 | `					return SXERR_SYNTAX;` |
|        - |  7193 | `				}` |
|      175 |  7194 | `				if( !bAllowVoid ){` |
|      ! 0 |  7195 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7196 | `						"void cannot be used here");` |
|      ! 0 |  7197 | `					return SXERR_SYNTAX;` |
|        - |  7198 | `				}` |
|      175 |  7199 | `				if( bShortNullable ){` |
|      ! 0 |  7200 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7201 | `						"Void type cannot be nullable");` |
|      ! 0 |  7202 | `					return SXERR_SYNTAX;` |
|        - |  7203 | `				}` |
|       85 |  7204 | `			}` |
|   110213 |  7205 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|        - |  7206 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|        - |  7207 | `				 * type (never = the function does not return). Mirrors the void` |
|        - |  7208 | `				 * validation above; accepted here and enforced at compile time` |
|        - |  7209 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|       26 |  7210 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|        - |  7211 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|        - |  7212 | `					 * same as any other non-standalone use. */` |
|        5 |  7213 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7214 | `						"never can only be used as a standalone type");` |
|        5 |  7215 | `					return SXERR_SYNTAX;` |
|        - |  7216 | `				}` |
|       21 |  7217 | `				if( !bAllowVoid ){` |
|        - |  7218 | `					/* Return-only: params call with bAllowVoid=0. */` |
|        3 |  7219 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7220 | `						"never cannot be used as a parameter type");` |
|        3 |  7221 | `					return SXERR_SYNTAX;` |
|        - |  7222 | `				}` |
|        8 |  7223 | `			}` |
|   110207 |  7224 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|       34 |  7225 | `				bExplicitNull = 1;` |
|       19 |  7226 | `			}else{` |
|   110177 |  7227 | `				bHasNonNull = 1;` |
|        - |  7228 | `			}` |
|        - |  7229 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|        - |  7230 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|        - |  7231 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|        - |  7232 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|        - |  7233 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|   110407 |  7234 | `			for( j = 0; j < i; j++ ){` |
|      207 |  7235 | `				int bDup = 0;` |
|      207 |  7236 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|      395 |  7237 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|      202 |  7238 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|      207 |  7239 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|      195 |  7240 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|       51 |  7241 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       44 |  7242 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|       44 |  7243 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|       17 |  7244 | `								aAtoms[j].sClass.zString,` |
|       34 |  7245 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|      ! 0 |  7246 | `							bDup = 1;` |
|      ! 0 |  7247 | `						}` |
|       27 |  7248 | `					}else{` |
|        3 |  7249 | `						bDup = 1;` |
|        - |  7250 | `					}` |
|       23 |  7251 | `				}` |
|      195 |  7252 | `				if( bDup ){` |
|        - |  7253 | `					const char *zName;` |
|        - |  7254 | `					sxu32 nName;` |
|        3 |  7255 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      ! 0 |  7256 | `						zName = aAtoms[i].sClass.zString;` |
|      ! 0 |  7257 | `						nName = aAtoms[i].sClass.nByte;` |
|      ! 0 |  7258 | `					}else{` |
|        3 |  7259 | `						zName = aAtoms[i].zCanon;` |
|        3 |  7260 | `						nName = aAtoms[i].nCanon;` |
|        - |  7261 | `					}` |
|        4 |  7262 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        1 |  7263 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|        3 |  7264 | `					return SXERR_SYNTAX;` |
|        - |  7265 | `				}` |
|       99 |  7266 | `			}` |
|    55105 |  7267 | `		}` |
|   110045 |  7268 | `		if( !bHasNonNull && bExplicitNull ){` |
|        7 |  7269 | `			if( bShortNullable ){` |
|        - |  7270 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|      ! 0 |  7271 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7272 | `					"Null can not be used as a standalone type");` |
|      ! 0 |  7273 | `				return SXERR_SYNTAX;` |
|        - |  7274 | `			}` |
|        - |  7275 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|        - |  7276 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|        - |  7277 | `			 * path below leaves *pnType untouched when there is no non-null` |
|        - |  7278 | `			 * atom, so set it here. */` |
|        7 |  7279 | `			*pnType = MEMOBJ_NULL;` |
|        3 |  7280 | `		}` |
|        - |  7281 | `	}` |
|        - |  7282 | `	/* Compute nullability flag */` |
|   110045 |  7283 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      128 |  7284 | `		*piTypeFlags \|= iNullableFlag;` |
|       62 |  7285 | `	}` |
|        - |  7286 | `	/* Build canonical type text */` |
|   110045 |  7287 | `	if( pTypeText ){` |
|        - |  7288 | `		SyBlob sBlob;` |
|   110045 |  7289 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|   165018 |  7290 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|    55020 |  7291 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   110045 |  7292 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|   164786 |  7293 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   109854 |  7294 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   109859 |  7295 | `			if( zDup ){` |
|   109859 |  7296 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|    54927 |  7297 | `			}` |
|    54927 |  7298 | `		}` |
|   110045 |  7299 | `		SyBlobRelease(&sBlob);` |
|    55020 |  7300 | `	}` |
|        - |  7301 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|        - |  7302 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|        - |  7303 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|        - |  7304 | `	{` |
|   110045 |  7305 | `		int nNonNull = 0;` |
|   110045 |  7306 | `		int iNonNullIdx = -1;` |
|        - |  7307 | `		int i;` |
|   220237 |  7308 | `		for( i = 0; i < nAtoms; i++ ){` |
|   110197 |  7309 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   110167 |  7310 | `				nNonNull++;` |
|   110167 |  7311 | `				iNonNullIdx = i;` |
|    55081 |  7312 | `			}` |
|    55101 |  7313 | `		}` |
|   110045 |  7314 | `		if( nNonNull <= 1 ){` |
|        - |  7315 | `			/* Fast path: store as single type. */` |
|   109939 |  7316 | `			if( iNonNullIdx >= 0 ){` |
|   109933 |  7317 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   109933 |  7318 | `				if( pA->nType == SXU32_HIGH ){` |
|    29438 |  7319 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     9811 |  7320 | `						pA->sClass.zString, pA->sClass.nByte);` |
|    19627 |  7321 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|    19627 |  7322 | `					*pnType = SXU32_HIGH;` |
|    19627 |  7323 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   100122 |  7324 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|      175 |  7325 | `					*pnType = MEMOBJ_VOID;` |
|    90226 |  7326 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|       18 |  7327 | `					*pnType = MEMOBJ_NEVER;` |
|       10 |  7328 | `				}else{` |
|    90125 |  7329 | `					*pnType = pA->nType;` |
|        - |  7330 | `				}` |
|    54964 |  7331 | `			}` |
|    54972 |  7332 | `		}else{` |
|        - |  7333 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      111 |  7334 | `			*piTypeFlags \|= iUnionFlag;` |
|      355 |  7335 | `			for( i = 0; i < nAtoms; i++ ){` |
|        - |  7336 | `				ph7_type_alt sAlt;` |
|      249 |  7337 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      239 |  7338 | `				SyZero(&sAlt, sizeof(sAlt));` |
|      239 |  7339 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|      239 |  7340 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      146 |  7341 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       47 |  7342 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       99 |  7343 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|       99 |  7344 | `					sAlt.nType = SXU32_HIGH;` |
|       99 |  7345 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|       52 |  7346 | `				}else{` |
|      145 |  7347 | `					sAlt.nType = aAtoms[i].nType;` |
|      145 |  7348 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|        - |  7349 | `				}` |
|      239 |  7350 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|      122 |  7351 | `			}` |
|        - |  7352 | `		}` |
|        - |  7353 | `	}` |
|   110045 |  7354 | `	return SXRET_OK;` |
|    55033 |  7355 | `}` |
|        - |  7356 |  |
|        - |  7357 | `/*` |
|        - |  7358 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|        - |  7359 | `` * pGen->pIn should point to the token after `)`.`` |
|        - |  7360 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|        - |  7361 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|        - |  7362 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|        - |  7363 | `` *          and union types `: T\|U`.`` |
|        - |  7364 | ` */` |
|  2108482 |  7365 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|        5 |  7366 | `{` |
|  2108487 |  7367 | `	sxi32 iFlags = 0;` |
|        - |  7368 | `	sxi32 rc;` |
|        - |  7369 | `	sxu32 nLine;` |
|  2108487 |  7370 | `	pFunc->nReturnType = 0;` |
|  2108487 |  7371 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  2108487 |  7372 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|        - |  7373 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|        - |  7374 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|        - |  7375 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|        - |  7376 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|        - |  7377 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|  2108487 |  7378 | `	SySetReset(&pFunc->aReturnUnion);` |
|  2108487 |  7379 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|  2108487 |  7380 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  2103955 |  7381 | `		return SXRET_OK;` |
|        - |  7382 | `	}` |
|     4537 |  7383 | `	pGen->pIn++; /* Skip ':' */` |
|     4537 |  7384 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  7385 | `		return SXRET_OK;` |
|        - |  7386 | `	}` |
|     4537 |  7387 | `	nLine = pGen->pIn->nLine;` |
|     4537 |  7388 | `	rc = GenStateParseUnionTypeDecl(` |
|     2266 |  7389 | `		pGen,` |
|     2266 |  7390 | `		&pFunc->nReturnType,` |
|     2266 |  7391 | `		&pFunc->sReturnClass,` |
|     2266 |  7392 | `		&pFunc->aReturnUnion,` |
|        - |  7393 | `		&iFlags,` |
|     2266 |  7394 | `		&pFunc->sReturnTypeName,` |
|        - |  7395 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|        - |  7396 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|        - |  7397 | `		/* iUnionFlag */ 0,` |
|        - |  7398 | `		/* bAllowVoid */ 1,` |
|     2266 |  7399 | `		nLine);` |
|     4537 |  7400 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  7401 | `		return SXERR_ABORT;` |
|        - |  7402 | `	}` |
|     4537 |  7403 | `	if( rc == SXERR_CORRUPT ){` |
|        - |  7404 | `		/* Error already reported */` |
|      ! 0 |  7405 | `		return SXERR_SYNTAX;` |
|        - |  7406 | `	}` |
|     4537 |  7407 | `	if( rc == SXERR_SYNTAX ){` |
|        8 |  7408 | `		if( pGen->pIn < pGen->pEnd ){` |
|       11 |  7409 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|        - |  7410 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|        6 |  7411 | `				&pGen->pIn->sData);` |
|        5 |  7412 | `		}else{` |
|      ! 0 |  7413 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|        - |  7414 | `				"syntax error, unexpected end of file in return type declaration");` |
|        - |  7415 | `		}` |
|        8 |  7416 | `		return SXERR_SYNTAX;` |
|        - |  7417 | `	}` |
|     4531 |  7418 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     4531 |  7419 | `	return SXRET_OK;` |
|  1054246 |  7420 | `}` |
|        - |  7421 |  |
|   207370 |  7422 | `static sxi32 GenStateCompileFunc(` |
|        - |  7423 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  7424 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|        - |  7425 | `	sxi32 iFlags,        /* Control flags */` |
|        - |  7426 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|        - |  7427 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|        - |  7428 | `	)` |
|        5 |  7429 | `{` |
|        - |  7430 | `	ph7_vm_func *pFunc;` |
|        - |  7431 | `	SyToken *pEnd;` |
|        - |  7432 | `	sxu32 nLine;` |
|        - |  7433 | `	char *zName;` |
|        - |  7434 | `	sxi32 rc;` |
|        - |  7435 | `	/* Extract line number */` |
|   207375 |  7436 | `	nLine = pGen->pIn->nLine;` |
|        - |  7437 | `	/* Jump the left parenthesis '(' */` |
|   207375 |  7438 | `	pGen->pIn++;` |
|        - |  7439 | `	/* Delimit the function signature */` |
|   207375 |  7440 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   207375 |  7441 | `	if( pEnd >= pGen->pEnd ){` |
|        - |  7442 | `		/* Syntax error */` |
|        8 |  7443 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|        8 |  7444 | `		if( rc == SXERR_ABORT ){` |
|        - |  7445 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7446 | `			return SXERR_ABORT;` |
|        - |  7447 | `		}` |
|        8 |  7448 | `		pGen->pIn = pGen->pEnd;` |
|        8 |  7449 | `		return SXRET_OK;` |
|        - |  7450 | `	}` |
|        - |  7451 | `	/* Create the function state */` |
|   207369 |  7452 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   207369 |  7453 | `	if( pFunc == 0 ){` |
|      ! 0 |  7454 | `		goto OutOfMem;` |
|        - |  7455 | `	}` |
|        - |  7456 | `	/* Build the function name, prepending namespace if active */` |
|   207376 |  7457 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|        - |  7458 | `		SyBlob sFQN;` |
|        - |  7459 | `		sxu32 nLen;` |
|       16 |  7460 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       16 |  7461 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       16 |  7462 | `		SyBlobAppend(&sFQN,"\\",1);` |
|       16 |  7463 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|       16 |  7464 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|       16 |  7465 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|       16 |  7466 | `		SyBlobRelease(&sFQN);` |
|       16 |  7467 | `		if( zName == 0 ){` |
|      ! 0 |  7468 | `			goto OutOfMem;` |
|        - |  7469 | `		}` |
|       16 |  7470 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|        9 |  7471 | `	}else{` |
|   207355 |  7472 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   207355 |  7473 | `		if( zName == 0 ){` |
|      ! 0 |  7474 | `			goto OutOfMem;` |
|        - |  7475 | `		}` |
|   207355 |  7476 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|        - |  7477 | `	}` |
|        - |  7478 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|        - |  7479 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|   207369 |  7480 | `	pFunc->nLine = nLine;` |
|   207369 |  7481 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|   207369 |  7482 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  7483 | `		return SXERR_ABORT;` |
|        - |  7484 | `	}` |
|   207369 |  7485 | `	if( pGen->pIn < pEnd ){` |
|        - |  7486 | `		/* Collect function arguments */` |
|   187149 |  7487 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   187149 |  7488 | `		if( rc == SXERR_ABORT ){` |
|        - |  7489 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  7490 | `			return SXERR_ABORT;` |
|        - |  7491 | `		}` |
|    93572 |  7492 | `	}` |
|        - |  7493 | `	/* Point past ')' and parse optional return type ': type' */` |
|   207369 |  7494 | `	pGen->pIn = &pEnd[1];` |
|        - |  7495 | `	{` |
|   207369 |  7496 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   207369 |  7497 | `		if( rcRt == SXERR_ABORT ){` |
|      ! 0 |  7498 | `			return SXERR_ABORT;` |
|   207369 |  7499 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|        8 |  7500 | `			return SXERR_SYNTAX;` |
|        - |  7501 | `		}` |
|        - |  7502 | `	}` |
|   207363 |  7503 | `	if( bHandleClosure ){` |
|        - |  7504 | `		ph7_vm_func_closure_env sEnv;` |
|      471 |  7505 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|      466 |  7506 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      281 |  7507 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|       91 |  7508 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |  7509 | `				/* Closure,record environment variable */` |
|       91 |  7510 | `				pGen->pIn++;` |
|       91 |  7511 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 |  7512 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|      ! 0 |  7513 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  7514 | `						return SXERR_ABORT;` |
|        - |  7515 | `					}` |
|      ! 0 |  7516 | `				}` |
|       91 |  7517 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|        - |  7518 | `				/* Compile until we hit the first closing parenthesis */` |
|      187 |  7519 | `				while( pGen->pIn < pGen->pEnd ){` |
|      187 |  7520 | `					int iFlagsLocal = 0;` |
|      187 |  7521 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|       91 |  7522 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|       91 |  7523 | `						break;` |
|        - |  7524 | `					}` |
|      101 |  7525 | `					nLineLocal = pGen->pIn->nLine;` |
|      101 |  7526 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|        - |  7527 | `						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry` |
|        - |  7528 | `						 * to the variable's memory slot instead of copying its value. */` |
|       55 |  7529 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|       55 |  7530 | `						pGen->pIn++;` |
|       27 |  7531 | `					}` |
|       96 |  7532 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      101 |  7533 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  7534 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        - |  7535 | `								"Closure: Unexpected token. Expecting a variable name");` |
|      ! 0 |  7536 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 |  7537 | `								return SXERR_ABORT;` |
|        - |  7538 | `							}` |
|        - |  7539 | `							/* Find the closing parenthesis */` |
|      ! 0 |  7540 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 |  7541 | `								pGen->pIn++;` |
|      ! 0 |  7542 | `							}` |
|      ! 0 |  7543 | `							if(pGen->pIn < pGen->pEnd){` |
|      ! 0 |  7544 | `								pGen->pIn++;` |
|      ! 0 |  7545 | `							}` |
|      ! 0 |  7546 | `							break;` |
|        - |  7547 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|      ! 0 |  7548 | `					}else{` |
|        - |  7549 | `						SyString *pNameLocal;` |
|        - |  7550 | `						char *zDup;` |
|        - |  7551 | `						/* Duplicate variable name */` |
|      101 |  7552 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      101 |  7553 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      101 |  7554 | `						if( zDup ){` |
|        - |  7555 | `							/* Zero the structure */` |
|      101 |  7556 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      101 |  7557 | `							sEnv.iFlags = iFlagsLocal;` |
|      101 |  7558 | `							sEnv.nIdx = SXU32_HIGH;` |
|      101 |  7559 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      101 |  7560 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      116 |  7561 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|       30 |  7562 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|      ! 0 |  7563 | `									got_this = 1;` |
|      ! 0 |  7564 | `							}` |
|        - |  7565 | `							/* Save imported variable */` |
|      101 |  7566 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       53 |  7567 | `						}else{` |
|      ! 0 |  7568 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  7569 | `							 return SXERR_ABORT;` |
|        - |  7570 | `						}` |
|        - |  7571 | `					}` |
|      101 |  7572 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      113 |  7573 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  7574 | `						/* Ignore trailing commas */` |
|       13 |  7575 | `						pGen->pIn++;` |
|        1 |  7576 | `					}` |
|        5 |  7577 | `				}` |
|        - |  7578 | `				/* php 7.1+: the return type follows the use clause —` |
|        - |  7579 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|        - |  7580 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|        - |  7581 | `				 * so an unconditional call would wipe a type parsed at the` |
|        - |  7582 | `				 * legacy pre-use position. */` |
|       91 |  7583 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|        7 |  7584 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|        7 |  7585 | `					if( rcRt2 == SXERR_ABORT ){` |
|      ! 0 |  7586 | `						return SXERR_ABORT;` |
|        7 |  7587 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|      ! 0 |  7588 | `						return SXERR_SYNTAX;` |
|        - |  7589 | `					}` |
|        3 |  7590 | `				}` |
|       43 |  7591 | `		}` |
|      471 |  7592 | `		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){` |
|        - |  7593 | `			/* Make the $this variable [Current processed Object (class instance)]` |
|        - |  7594 | `			 * available to the closure environment — for EVERY non-static` |
|        - |  7595 | `			 * anonymous function, use list or not (php binds $this to any` |
|        - |  7596 | ``			 * closure declared in a method; pre-fix only `use (...)` closures`` |
|        - |  7597 | `			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of` |
|        - |  7598 | `			 * a global-scope closure is silently dropped at install. A static` |
|        - |  7599 | `			 * closure never binds $this (php). */` |
|      463 |  7600 | `			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      463 |  7601 | `			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      463 |  7602 | `			sEnv.nIdx = SXU32_HIGH;` |
|      463 |  7603 | `			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      463 |  7604 | `			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      463 |  7605 | `			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      229 |  7606 | `		}` |
|      471 |  7607 | `		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|        - |  7608 | `			/* Mark as closure */` |
|      465 |  7609 | `			pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|      230 |  7610 | `		}` |
|      233 |  7611 | `	}` |
|        - |  7612 | `	/* Compile the body */` |
|   207363 |  7613 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   207363 |  7614 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  7615 | `		return SXERR_ABORT;` |
|        - |  7616 | `	}` |
|        - |  7617 | `	/* The cursor sits just past the body's closing brace */` |
|   207363 |  7618 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|   207363 |  7619 | `	if( ppFunc ){` |
|   207363 |  7620 | `		*ppFunc = pFunc;` |
|   103679 |  7621 | `	}` |
|   207363 |  7622 | `	rc = SXRET_OK;` |
|   207363 |  7623 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|        - |  7624 | `		/* Finally register the function */` |
|   206903 |  7625 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   103449 |  7626 | `	}` |
|   207363 |  7627 | `	if( rc == SXRET_OK ){` |
|   207363 |  7628 | `		return SXRET_OK;` |
|        - |  7629 | `	}` |
|        - |  7630 | `	/* Fall through if something goes wrong */` |
|      ! 0 |  7631 | `OutOfMem:` |
|        - |  7632 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  7633 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  7634 | `	 */` |
|      ! 0 |  7635 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 |  7636 | `	return SXERR_ABORT;` |
|   103690 |  7637 | `}` |
|        - |  7638 | `/*` |
|        - |  7639 | ` * Compile a standard PHP function.` |
|        - |  7640 | ` *  Refer to the block-comment above for more information.` |
|        - |  7641 | ` */` |
|   206912 |  7642 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|        5 |  7643 | `{` |
|        - |  7644 | `	SyString *pName;` |
|        - |  7645 | `	sxi32 iFlags;` |
|        - |  7646 | `	sxu32 nKwLine;` |
|        - |  7647 | `	sxu32 nLine;` |
|        - |  7648 | `	sxi32 rc;` |
|        - |  7649 |  |
|   206917 |  7650 | `	nLine = pGen->pIn->nLine;` |
|   206917 |  7651 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|   206917 |  7652 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   206917 |  7653 | `	iFlags = 0;` |
|   206917 |  7654 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        - |  7655 | `		/* Return by reference,remember that */` |
|       12 |  7656 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|        - |  7657 | `		/* Jump the '&' token */` |
|       12 |  7658 | `		pGen->pIn++;` |
|        5 |  7659 | `	}` |
|   206917 |  7660 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  7661 | `		/* Invalid function name */` |
|        8 |  7662 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|        8 |  7663 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  7664 | `			return SXERR_ABORT;` |
|        - |  7665 | `		}` |
|        - |  7666 | `		/* Sychronize with the next semi-colon or braces*/` |
|       22 |  7667 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       16 |  7668 | `			pGen->pIn++;` |
|        2 |  7669 | `		}` |
|        8 |  7670 | `		return SXRET_OK;` |
|        - |  7671 | `	}` |
|   206911 |  7672 | `	pName = &pGen->pIn->sData;` |
|   206911 |  7673 | `	nLine = pGen->pIn->nLine;` |
|        - |  7674 | `	/* Jump the function name */` |
|   206911 |  7675 | `	pGen->pIn++;` |
|   206911 |  7676 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  7677 | `		/* Syntax error */` |
|        3 |  7678 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|        3 |  7679 | `		if( rc == SXERR_ABORT ){` |
|        - |  7680 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7681 | `			return SXERR_ABORT;` |
|        - |  7682 | `		}` |
|        - |  7683 | `		/* Sychronize with the next semi-colon or '{' */` |
|        3 |  7684 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  7685 | `			pGen->pIn++;` |
|      ! 0 |  7686 | `		}` |
|        3 |  7687 | `		return SXRET_OK;` |
|        - |  7688 | `	}` |
|        - |  7689 | `	/* Compile function body */` |
|        - |  7690 | `	{` |
|   206909 |  7691 | `		ph7_vm_func *pFuncState = 0;` |
|   206909 |  7692 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);` |
|   206909 |  7693 | `		if( pFuncState ){` |
|        - |  7694 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|   206897 |  7695 | `			pFuncState->nLine = nKwLine;` |
|   103446 |  7696 | `		}` |
|        - |  7697 | `	}` |
|   206909 |  7698 | `	return rc;` |
|   103461 |  7699 | `}` |
|        - |  7700 | `/*` |
|        - |  7701 | ` * Extract the visibility level associated with a given keyword.` |
|        - |  7702 | ` * According to the PHP language reference manual` |
|        - |  7703 | ` *  Visibility:` |
|        - |  7704 | ` *  The visibility of a property or method can be defined by prefixing` |
|        - |  7705 | ` *  the declaration with the keywords public, protected or private.` |
|        - |  7706 | ` *  Class members declared public can be accessed everywhere.` |
|        - |  7707 | ` *  Members declared protected can be accessed only within the class` |
|        - |  7708 | ` *  itself and by inherited and parent classes. Members declared as private` |
|        - |  7709 | ` *  may only be accessed by the class that defines the member.` |
|        - |  7710 | ` */` |
|  2514384 |  7711 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|        5 |  7712 | `{` |
|  2514389 |  7713 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|   135849 |  7714 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  2378545 |  7715 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   182257 |  7716 | `		return PH7_CLASS_PROT_PROTECTED;` |
|        - |  7717 | `	}` |
|        - |  7718 | `	/* Assume public by default */` |
|  2196293 |  7719 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  1257197 |  7720 | `}` |
|        - |  7721 | `/*` |
|        - |  7722 | ` * Compile a class constant.` |
|        - |  7723 | ` * According to the PHP language reference manual` |
|        - |  7724 | ` *  Class Constants` |
|        - |  7725 | ` *   It is possible to define constant values on a per-class basis remaining` |
|        - |  7726 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|        - |  7727 | ` *   you don't use the $ symbol to declare or use them.` |
|        - |  7728 | ` *   The value must be a constant expression, not (for example) a variable,` |
|        - |  7729 | ` *   a property, a result of a mathematical operation, or a function call.` |
|        - |  7730 | ` *   It's also possible for interfaces to have constants.` |
|        - |  7731 | ` * Symisc eXtension.` |
|        - |  7732 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|        - |  7733 | ` *  the zend engine would allow only simple scalar value.` |
|        - |  7734 | ` *  Example:` |
|        - |  7735 | ` *   class Test{` |
|        - |  7736 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - |  7737 | ` *   };` |
|        - |  7738 | ` *   var_dump(TEST::MyConst);` |
|        - |  7739 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - |  7740 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - |  7741 | ` */` |
|        - |  7742 | `/*` |
|        - |  7743 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|        - |  7744 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|        - |  7745 | ` * token immediately followed by '='. Anything else with a leading type token` |
|        - |  7746 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|        - |  7747 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|        - |  7748 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|        - |  7749 | ` */` |
|   248240 |  7750 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|        5 |  7751 | `{` |
|        - |  7752 | `	SyToken *p0, *p1;` |
|   248245 |  7753 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  7754 | `		return 0;` |
|        - |  7755 | `	}` |
|   248245 |  7756 | `	p0 = pGen->pIn;` |
|        - |  7757 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|   248245 |  7758 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|      ! 0 |  7759 | `		return 1;` |
|        - |  7760 | `	}` |
|   248245 |  7761 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|        5 |  7762 | `		return 1;` |
|        - |  7763 | `	}` |
|        - |  7764 | `	/* A name-like first token begins a type only when followed by another` |
|        - |  7765 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|        - |  7766 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|   248241 |  7767 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|   248241 |  7768 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|   248241 |  7769 | `		if( p1 ){` |
|   248241 |  7770 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|       34 |  7771 | `				return 1;` |
|        - |  7772 | `			}` |
|   248211 |  7773 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|        5 |  7774 | `				return 1;` |
|        - |  7775 | `			}` |
|   124101 |  7776 | `		}` |
|   124101 |  7777 | `	}` |
|   248207 |  7778 | `	return 0;` |
|   124125 |  7779 | `}` |
|        - |  7780 | `/*` |
|        - |  7781 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|        - |  7782 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|        - |  7783 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|        - |  7784 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|        - |  7785 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|        - |  7786 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|        - |  7787 | ` * Peek only; never consumes tokens.` |
|        - |  7788 | ` */` |
|       24 |  7789 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|        4 |  7790 | `{` |
|       28 |  7791 | `	SyToken *p = pGen->pIn;` |
|       39 |  7792 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|       20 |  7793 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|        3 |  7794 | `		p++; /* skip leading unary sign(s) */` |
|        1 |  7795 | `	}` |
|       28 |  7796 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|       23 |  7797 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|        - |  7798 | `	}` |
|        6 |  7799 | `	p++;` |
|        - |  7800 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|        6 |  7801 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|       16 |  7802 | `}` |
|        - |  7803 | `/*` |
|        - |  7804 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|        - |  7805 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|        - |  7806 | `` * `$o->new`), not a `new` expression.`` |
|        - |  7807 | ` */` |
|      110 |  7808 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|        4 |  7809 | `{` |
|        - |  7810 | `	sxi32 iOp;` |
|      114 |  7811 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|       11 |  7812 | `		return 0;` |
|        - |  7813 | `	}` |
|      104 |  7814 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|      104 |  7815 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|       59 |  7816 | `}` |
|        - |  7817 | `/*` |
|        - |  7818 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|        - |  7819 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|        - |  7820 | ` * interface-constant and (instance/static) property-default initializers` |
|        - |  7821 | ` * ("New expressions are not supported in this context") while still allowing it` |
|        - |  7822 | ` * in global constants, parameter defaults and static-local initializers (which` |
|        - |  7823 | ` * are compiled by different functions and left untouched). The scan is` |
|        - |  7824 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|        - |  7825 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|        - |  7826 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|        - |  7827 | ` *` |
|        - |  7828 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|        - |  7829 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|        - |  7830 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|        - |  7831 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|        - |  7832 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|        - |  7833 | ` */` |
|   489226 |  7834 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|        5 |  7835 | `{` |
|   489231 |  7836 | `	SyToken *p = pGen->pIn;` |
|   489231 |  7837 | `	int iDepth = 0;` |
|  1246985 |  7838 | `	while( p < pGen->pEnd ){` |
|  1246985 |  7839 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|   489179 |  7840 | `			break; /* end of this initializer */` |
|        - |  7841 | `		}` |
|   757806 |  7842 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|   382800 |  7843 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|     7784 |  7844 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|        - |  7845 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|        - |  7846 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|        - |  7847 | `			 * expression. */` |
|        3 |  7848 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|        3 |  7849 | `			p++;` |
|        3 |  7850 | `			if( bArrow ){` |
|        - |  7851 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|        - |  7852 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|        3 |  7853 | `				int iBase = iDepth;` |
|       17 |  7854 | `				while( p < pGen->pEnd ){` |
|       17 |  7855 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        5 |  7856 | `						iDepth++;` |
|       15 |  7857 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        5 |  7858 | `						if( iDepth <= iBase ){` |
|      ! 0 |  7859 | `							break; /* closes an enclosing group, not the fn's own */` |
|        - |  7860 | `						}` |
|        5 |  7861 | `						iDepth--;` |
|       11 |  7862 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|        3 |  7863 | `						break;` |
|        - |  7864 | `					}` |
|       15 |  7865 | `					p++;` |
|        1 |  7866 | `				}` |
|        2 |  7867 | `			}else{` |
|        - |  7868 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|        - |  7869 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|        - |  7870 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|        - |  7871 | `				 * then skip the balanced brace block. */` |
|      ! 0 |  7872 | `				int iLocal = 0;` |
|      ! 0 |  7873 | `				while( p < pGen->pEnd ){` |
|      ! 0 |  7874 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|      ! 0 |  7875 | `						break; /* body brace */` |
|        - |  7876 | `					}` |
|      ! 0 |  7877 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      ! 0 |  7878 | `						iLocal++;` |
|      ! 0 |  7879 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      ! 0 |  7880 | `						if( iLocal > 0 ){` |
|      ! 0 |  7881 | `							iLocal--;` |
|      ! 0 |  7882 | `						}` |
|      ! 0 |  7883 | `					}` |
|      ! 0 |  7884 | `					p++;` |
|      ! 0 |  7885 | `				}` |
|      ! 0 |  7886 | `				if( p < pGen->pEnd ){` |
|      ! 0 |  7887 | `					int iBrace = 0; /* p is on the body '{' */` |
|      ! 0 |  7888 | `					while( p < pGen->pEnd ){` |
|      ! 0 |  7889 | `						if( p->nType & PH7_TK_OCB ){` |
|      ! 0 |  7890 | `							iBrace++;` |
|      ! 0 |  7891 | `						}else if( p->nType & PH7_TK_CCB ){` |
|      ! 0 |  7892 | `							iBrace--;` |
|      ! 0 |  7893 | `							if( iBrace == 0 ){` |
|      ! 0 |  7894 | `								p++;` |
|      ! 0 |  7895 | `								break;` |
|        - |  7896 | `							}` |
|      ! 0 |  7897 | `						}` |
|      ! 0 |  7898 | `						p++;` |
|      ! 0 |  7899 | `					}` |
|      ! 0 |  7900 | `				}` |
|        - |  7901 | `			}` |
|        3 |  7902 | `			continue;` |
|        - |  7903 | `		}` |
|   757809 |  7904 | `		if( p->nType & PH7_TK_OCB ){` |
|       45 |  7905 | `			if( iDepth == 0 ){` |
|        - |  7906 | `				/* A depth-0 '{' can only open a PHP 8.4 property-hook list` |
|        - |  7907 | ``				 * (`public T $x = default { get …; }`): the default expression`` |
|        - |  7908 | ``				 * ends here. A `new` inside a hook BODY runs at access time and`` |
|        - |  7909 | `				 * is legal — don't scan into it. */` |
|       45 |  7910 | `				break;` |
|        - |  7911 | `			}` |
|      ! 0 |  7912 | `			iDepth++;` |
|   757765 |  7913 | `		}else if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|    15591 |  7914 | `			iDepth++;` |
|   749972 |  7915 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|    15589 |  7916 | `			if( iDepth > 0 ){` |
|    15589 |  7917 | `				iDepth--;` |
|     7792 |  7918 | `			}` |
|   734387 |  7919 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|   244969 |  7920 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|        - |  7921 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|        - |  7922 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|        - |  7923 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|       11 |  7924 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|       11 |  7925 | `				return 1;` |
|        - |  7926 | `			}` |
|      ! 0 |  7927 | `		}` |
|   757757 |  7928 | `		p++;` |
|        5 |  7929 | `	}` |
|   489223 |  7930 | `	return 0;` |
|   244618 |  7931 | `}` |
|        - |  7932 | `/*` |
|        - |  7933 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|        - |  7934 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|        - |  7935 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|        - |  7936 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|        - |  7937 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|        - |  7938 | ` * share the same backing.` |
|        - |  7939 | ` */` |
|      350 |  7940 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|        - |  7941 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|        5 |  7942 | `{` |
|      355 |  7943 | `	pAttr->nType = nType;` |
|      355 |  7944 | `	pAttr->sClass = *pClass;` |
|      355 |  7945 | `	pAttr->sTypeName = *pTypeName;` |
|      355 |  7946 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|        - |  7947 | `		sxu32 i;` |
|       73 |  7948 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|       51 |  7949 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|       51 |  7950 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|       28 |  7951 | `		}` |
|       11 |  7952 | `	}` |
|      355 |  7953 | `}` |
|   248240 |  7954 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 |  7955 | `{` |
|   248245 |  7956 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  7957 | `	SySet *pInstrContainer;` |
|        - |  7958 | `	ph7_class_attr *pCons;` |
|        - |  7959 | `	SyString *pName;` |
|        - |  7960 | `	sxi32 rc;` |
|   248245 |  7961 | `	sxu32 nType = 0;` |
|        - |  7962 | `	SyString sTypeClass;` |
|        - |  7963 | `	SyString sTypeText;` |
|        - |  7964 | `	SySet aUnionAlts;` |
|   248245 |  7965 | `	sxi32 iTypeFlags = 0;` |
|   248245 |  7966 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   248245 |  7967 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   248245 |  7968 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  7969 | `	/* Extract visibility level */` |
|   248245 |  7970 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - |  7971 | `	/* Mark as constant */` |
|   248245 |  7972 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|   248245 |  7973 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        - |  7974 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|        - |  7975 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|   248264 |  7976 | `	if( GenStateClassConstHasType(pGen) ){` |
|       61 |  7977 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|       38 |  7978 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|        - |  7979 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|        - |  7980 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|        - |  7981 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|        - |  7982 | `		 * and success paths release. */` |
|       42 |  7983 | `		if( rc == SXERR_CORRUPT ){` |
|        - |  7984 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|      ! 0 |  7985 | `			goto Synchronize;` |
|       42 |  7986 | `		}else if( rc == SXERR_ABORT ){` |
|      ! 0 |  7987 | `			return SXERR_ABORT;` |
|       42 |  7988 | `		}else if( rc != SXRET_OK ){` |
|      ! 0 |  7989 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  7990 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|      ! 0 |  7991 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  7992 | `				return SXERR_ABORT;` |
|        - |  7993 | `			}` |
|      ! 0 |  7994 | `			goto Synchronize;` |
|        - |  7995 | `		}` |
|       42 |  7996 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|       19 |  7997 | `	}` |
|   124120 |  7998 | `loop:` |
|   248247 |  7999 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|        - |  8000 | `		/* Invalid constant name */` |
|      ! 0 |  8001 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|      ! 0 |  8002 | `		if( rc == SXERR_ABORT ){` |
|        - |  8003 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8004 | `			return SXERR_ABORT;` |
|        - |  8005 | `		}` |
|      ! 0 |  8006 | `		goto Synchronize;` |
|        - |  8007 | `	}` |
|        - |  8008 | `	/* Peek constant name */` |
|   248247 |  8009 | `	pName = &pGen->pIn->sData;` |
|        - |  8010 | `	/* Make sure the constant name isn't reserved */` |
|   248247 |  8011 | `	if( GenStateIsReservedConstant(pName) ){` |
|        - |  8012 | `		/* Reserved constant name */` |
|      ! 0 |  8013 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|      ! 0 |  8014 | `		if( rc == SXERR_ABORT ){` |
|        - |  8015 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8016 | `			return SXERR_ABORT;` |
|        - |  8017 | `		}` |
|      ! 0 |  8018 | `		goto Synchronize;` |
|        - |  8019 | `	}` |
|        - |  8020 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|   248247 |  8021 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       61 |  8022 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|       38 |  8023 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       19 |  8024 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|       42 |  8025 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8026 | `			return SXERR_ABORT;` |
|       42 |  8027 | `		}else if( rc != SXRET_OK ){` |
|        3 |  8028 | `			goto Synchronize;` |
|        - |  8029 | `		}` |
|       18 |  8030 | `	}` |
|        - |  8031 | `	/* Advance the stream cursor */` |
|   248245 |  8032 | `	pGen->pIn++;` |
|   248245 |  8033 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|        - |  8034 | `		/* Invalid declaration */` |
|      ! 0 |  8035 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|      ! 0 |  8036 | `		if( rc == SXERR_ABORT ){` |
|        - |  8037 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8038 | `			return SXERR_ABORT;` |
|        - |  8039 | `		}` |
|      ! 0 |  8040 | `		goto Synchronize;` |
|        - |  8041 | `	}` |
|   248245 |  8042 | `	pGen->pIn++; /* Jump the equal sign */` |
|        - |  8043 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|        - |  8044 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|        - |  8045 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|        - |  8046 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|   248240 |  8047 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|       39 |  8048 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|        8 |  8049 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8050 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|        2 |  8051 | `			&pClass->sName,pName,&sTypeText);` |
|        6 |  8052 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8053 | `			return SXERR_ABORT;` |
|        - |  8054 | `		}` |
|        6 |  8055 | `		goto Synchronize;` |
|        - |  8056 | `	}` |
|        - |  8057 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|        - |  8058 | `	 * constant initializer ("New expressions are not supported in this context").` |
|        - |  8059 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|   248241 |  8060 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|        5 |  8061 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8062 | `			"New expressions are not supported in this context");` |
|        5 |  8063 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8064 | `			return SXERR_ABORT;` |
|        - |  8065 | `		}` |
|        5 |  8066 | `		goto Synchronize;` |
|        - |  8067 | `	}` |
|        - |  8068 | `	/* Allocate a new class attribute */` |
|   248237 |  8069 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   248237 |  8070 | `	if( pCons ){` |
|   248237 |  8071 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|   248237 |  8072 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  8073 | `			return SXERR_ABORT;` |
|        - |  8074 | `		}` |
|   124116 |  8075 | `	}` |
|   248237 |  8076 | `	if( pCons == 0 ){` |
|      ! 0 |  8077 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8078 | `		return SXERR_ABORT;` |
|        - |  8079 | `	}` |
|   248237 |  8080 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       35 |  8081 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       16 |  8082 | `	}` |
|        - |  8083 | `	/* Swap bytecode container */` |
|   248237 |  8084 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   248237 |  8085 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|        - |  8086 | `	/* Compile constant value.` |
|        - |  8087 | `	 */` |
|   248237 |  8088 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   248237 |  8089 | `	if( rc == SXERR_EMPTY ){` |
|        3 |  8090 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|        3 |  8091 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8092 | `			return SXERR_ABORT;` |
|        - |  8093 | `		}` |
|        1 |  8094 | `	}` |
|        - |  8095 | `	/* Emit the done instruction */` |
|   248237 |  8096 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   248237 |  8097 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   248237 |  8098 | `	if( rc == SXERR_ABORT ){` |
|        - |  8099 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  8100 | `		return SXERR_ABORT;` |
|        - |  8101 | `	}` |
|        - |  8102 | `	/* All done,install the constant */` |
|   248237 |  8103 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|   248237 |  8104 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8105 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8106 | `		return SXERR_ABORT;` |
|        - |  8107 | `	}` |
|   248237 |  8108 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  8109 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|        3 |  8110 | `		pGen->pIn++; /* Jump the comma */` |
|        3 |  8111 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 |  8112 | `			SyToken *pTok = pGen->pIn;` |
|      ! 0 |  8113 | `			if( pTok >= pGen->pEnd ){` |
|      ! 0 |  8114 | `				pTok--;` |
|      ! 0 |  8115 | `			}` |
|      ! 0 |  8116 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  8117 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|      ! 0 |  8118 | `				&pTok->sData,&pClass->sName);` |
|      ! 0 |  8119 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8120 | `				return SXERR_ABORT;` |
|        - |  8121 | `			}` |
|      ! 0 |  8122 | `		}else{` |
|        3 |  8123 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|        3 |  8124 | `				goto loop;` |
|        - |  8125 | `			}` |
|        - |  8126 | `		}` |
|      ! 0 |  8127 | `	}` |
|   248235 |  8128 | `	SySetRelease(&aUnionAlts);` |
|   248235 |  8129 | `	return SXRET_OK;` |
|        5 |  8130 | `Synchronize:` |
|       13 |  8131 | `	SySetRelease(&aUnionAlts);` |
|        - |  8132 | `	/* Synchronize with the first semi-colon */` |
|       45 |  8133 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       35 |  8134 | `		pGen->pIn++;` |
|        3 |  8135 | `	}` |
|       13 |  8136 | `	return SXERR_CORRUPT;` |
|   124125 |  8137 | `}` |
|        - |  8138 | `/*` |
|        - |  8139 | ` * complie a class attribute or Properties in the PHP jargon.` |
|        - |  8140 | ` * According to the PHP language reference manual` |
|        - |  8141 | ` *  Properties` |
|        - |  8142 | ` *  Class member variables are called "properties". You may also see them referred` |
|        - |  8143 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|        - |  8144 | ` *  of this reference we will use "properties". They are defined by using one` |
|        - |  8145 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|        - |  8146 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|        - |  8147 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|        - |  8148 | ` *  and must not depend on run-time information in order to be evaluated.` |
|        - |  8149 | ` * Symisc eXtension.` |
|        - |  8150 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|        - |  8151 | ` *  the zend engine would allow only simple scalar value.` |
|        - |  8152 | ` *  Example:` |
|        - |  8153 | ` *   class Test{` |
|        - |  8154 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - |  8155 | ` *   };` |
|        - |  8156 | ` *   var_dump(TEST::myVar);` |
|        - |  8157 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - |  8158 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - |  8159 | ` */` |
|        - |  8160 | `/*` |
|        - |  8161 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|        - |  8162 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|        - |  8163 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|        - |  8164 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|        - |  8165 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|        - |  8166 | ` */` |
|  1850476 |  8167 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|        5 |  8168 | `{` |
|  1850481 |  8169 | `	SyToken *p = pStart;` |
|  1850481 |  8170 | `	int bFirst = 1;` |
|  1850481 |  8171 | `	if( p >= pEnd ) return 0;` |
|        - |  8172 | ``	/* Optional nullable `?` shorthand. */`` |
|  1850481 |  8173 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|       35 |  8174 | `		p++;` |
|       35 |  8175 | `		if( p >= pEnd ) return 0;` |
|       16 |  8176 | `	}` |
|        - |  8177 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|        - |  8178 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|        - |  8179 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|        - |  8180 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   925238 |  8181 | `	for(;;){` |
|  1850501 |  8182 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|        - |  8183 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|        3 |  8184 | `			p++;` |
|        9 |  8185 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|        3 |  8186 | `			if( p >= pEnd ) return 0;` |
|        3 |  8187 | `			p++; /* skip ')' */` |
|        2 |  8188 | `		}else{` |
|        - |  8189 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|        - |  8190 | ``			 * then any `&`-joined intersection members. */`` |
|  1850499 |  8191 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|  1850499 |  8192 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  8193 | `				return 0;` |
|        - |  8194 | `			}` |
|        - |  8195 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|        - |  8196 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|        - |  8197 | `			 * may still appear at the initial dispatch site). */` |
|  1850499 |  8198 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|  1850451 |  8199 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  1850446 |  8200 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    74052 |  8201 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  1850169 |  8202 | `					return 0;` |
|        - |  8203 | `				}` |
|      141 |  8204 | `			}` |
|      335 |  8205 | `			p++;` |
|      337 |  8206 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  8207 | `				p += 2;` |
|        1 |  8208 | `			}` |
|      498 |  8209 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|      338 |  8210 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  8211 | `				p++; /* skip '&' */` |
|        3 |  8212 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|        3 |  8213 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|        3 |  8214 | `				p++;` |
|        3 |  8215 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      ! 0 |  8216 | `					p += 2;` |
|      ! 0 |  8217 | `				}` |
|        1 |  8218 | `			}` |
|        - |  8219 | `		}` |
|      337 |  8220 | `		bFirst = 0;` |
|      332 |  8221 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|       25 |  8222 | `			&& p->sData.zString[0] == '\|' ){` |
|       25 |  8223 | ``			p++; /* next `\|`-separated part */`` |
|       25 |  8224 | `			continue;` |
|        - |  8225 | `		}` |
|      317 |  8226 | `		break;` |
|      ! 0 |  8227 | `	}` |
|      317 |  8228 | `	if( p >= pEnd ) return 0;` |
|      317 |  8229 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   925243 |  8230 | `}` |
|        - |  8231 |  |
|        - |  8232 | `/*` |
|        - |  8233 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|        - |  8234 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|        - |  8235 | ` * if not). Recognized forms:` |
|        - |  8236 | ` *   ?Type, array, bool, int, float, string, object,` |
|        - |  8237 | ` *   self, parent, \Ns\ClassName, ClassName` |
|        - |  8238 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|        - |  8239 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|        - |  8240 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|        - |  8241 | ` * on unrecoverable error.` |
|        - |  8242 | ` *` |
|        - |  8243 | ` * When a type is parsed:` |
|        - |  8244 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|        - |  8245 | ` *   *pClass is set to the class name (for class types)` |
|        - |  8246 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|        - |  8247 | ` *   *pTypeText is set to the original text span of the type` |
|        - |  8248 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|        - |  8249 | ` */` |
|      322 |  8250 | `static sxi32 GenStateParsePropertyType(` |
|        - |  8251 | `	ph7_gen_state *pGen,` |
|        - |  8252 | `	sxu32 *pnType,` |
|        - |  8253 | `	SyString *pClass,` |
|        - |  8254 | `	sxi32 *piTypeFlags,` |
|        - |  8255 | `	SyString *pTypeText,` |
|        - |  8256 | `	SySet *pAlts` |
|        5 |  8257 | `){` |
|      327 |  8258 | `	sxi32 iFlags = 0;` |
|        - |  8259 | `	sxi32 rc;` |
|      327 |  8260 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  8261 | `		return SXRET_OK;` |
|        - |  8262 | `	}` |
|        - |  8263 | `	/* If the first token is '$', there's no type */` |
|      327 |  8264 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      ! 0 |  8265 | `		return SXRET_OK;` |
|        - |  8266 | `	}` |
|      327 |  8267 | `	rc = GenStateParseUnionTypeDecl(` |
|      161 |  8268 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|        - |  8269 | `		PH7_CLASS_ATTR_NULLABLE,` |
|        - |  8270 | `		PH7_CLASS_ATTR_UNION,` |
|        - |  8271 | `		/* bAllowVoid */ 0,` |
|      322 |  8272 | `		pGen->pIn->nLine);` |
|      327 |  8273 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8274 | `		return rc;` |
|        - |  8275 | `	}` |
|        - |  8276 | `	/* Verify next token is '$' (start of property name) */` |
|      327 |  8277 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  8278 | `		return SXERR_SYNTAX;` |
|        - |  8279 | `	}` |
|      327 |  8280 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|      327 |  8281 | `	return SXRET_OK;` |
|      166 |  8282 | `}` |
|        - |  8283 |  |
|        - |  8284 | `/*` |
|        - |  8285 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|        - |  8286 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|        - |  8287 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|        - |  8288 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|        - |  8289 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|        - |  8290 | ` * by the type parser itself before reaching here.` |
|        - |  8291 | ` *` |
|        - |  8292 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|        - |  8293 | ` * use in the error message.` |
|        - |  8294 | ` */` |
|      498 |  8295 | `static int GenStateIsDisallowedPropertyAtom(` |
|        - |  8296 | `	sxu32 nType,` |
|        - |  8297 | `	const SyString *pClass,` |
|        - |  8298 | `	const char **pzName,` |
|        - |  8299 | `	sxu32 *pnName)` |
|        5 |  8300 | `{` |
|        - |  8301 | `	const char *z;` |
|        - |  8302 | `	sxu32 n;` |
|      503 |  8303 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|      449 |  8304 | `		return 0;` |
|        - |  8305 | `	}` |
|       59 |  8306 | `	z = pClass->zString;` |
|       59 |  8307 | `	n = pClass->nByte;` |
|       59 |  8308 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|        8 |  8309 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|        - |  8310 | `	}` |
|        - |  8311 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|        - |  8312 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|        - |  8313 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|       52 |  8314 | `	return 0;` |
|      254 |  8315 | `}` |
|        - |  8316 |  |
|        - |  8317 | `/*` |
|        - |  8318 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|        - |  8319 | ` * constant) — the main atom plus any union alternatives — against the` |
|        - |  8320 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|        - |  8321 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|        - |  8322 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|        - |  8323 | ` * type T" vs "Class constant C::X cannot have type T").` |
|        - |  8324 | ` *` |
|        - |  8325 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|        - |  8326 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|        - |  8327 | ` */` |
|      436 |  8328 | `static sxi32 GenStateValidateMemberType(` |
|        - |  8329 | `	ph7_gen_state *pGen,` |
|        - |  8330 | `	ph7_class *pClass,` |
|        - |  8331 | `	const SyString *pMemberName,` |
|        - |  8332 | `	sxu32 nType,` |
|        - |  8333 | `	const SyString *pTypeClass,` |
|        - |  8334 | `	const SyString *pTypeText,` |
|        - |  8335 | `	SySet *pUnionAlts,` |
|        - |  8336 | `	const char *zErrFmt,` |
|        - |  8337 | `	sxu32 nLine)` |
|        5 |  8338 | `{` |
|      441 |  8339 | `	const char *zBad = 0;` |
|      441 |  8340 | `	sxu32 nBad = 0;` |
|        - |  8341 | `	SyString sFallback;` |
|        - |  8342 | `	const SyString *pBad;` |
|        - |  8343 | `	sxi32 rc;` |
|      441 |  8344 | `	int bDisallowed = 0;` |
|      441 |  8345 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|        5 |  8346 | `		bDisallowed = 1;` |
|      439 |  8347 | `	}else if( pUnionAlts ){` |
|        - |  8348 | `		sxu32 i;` |
|       95 |  8349 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|       67 |  8350 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|       67 |  8351 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|        3 |  8352 | `				bDisallowed = 1;` |
|        3 |  8353 | `				break;` |
|        - |  8354 | `			}` |
|       35 |  8355 | `		}` |
|       15 |  8356 | `	}` |
|      441 |  8357 | `	if( !bDisallowed ){` |
|      435 |  8358 | `		return SXRET_OK;` |
|        - |  8359 | `	}` |
|        - |  8360 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|        - |  8361 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|        - |  8362 | `	 * canonical spelling if the type text is unavailable. */` |
|        8 |  8363 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|        8 |  8364 | `		pBad = pTypeText;` |
|        5 |  8365 | `	}else{` |
|      ! 0 |  8366 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|      ! 0 |  8367 | `		pBad = &sFallback;` |
|        - |  8368 | `	}` |
|       11 |  8369 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        3 |  8370 | `		zErrFmt,` |
|        3 |  8371 | `		&pClass->sName,pMemberName,pBad);` |
|        8 |  8372 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  8373 | `		return SXERR_ABORT;` |
|        - |  8374 | `	}` |
|        8 |  8375 | `	return SXERR_SYNTAX;` |
|      223 |  8376 | `}` |
|        - |  8377 | `/*` |
|        - |  8378 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|        - |  8379 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|        - |  8380 | ` * matched as a plain identifier in the class-member modifier position rather` |
|        - |  8381 | ` * than promoted to a lexer keyword.` |
|        - |  8382 | ` */` |
| 14096534 |  8383 | `static int GenStateIsReadonly(SyToken *pTok)` |
|        5 |  8384 | `{` |
| 14168662 |  8385 | `	return (pTok->nType & PH7_TK_ID)` |
|  7120390 |  8386 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 14168657 |  8387 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|        5 |  8388 | `}` |
|        - |  8389 | `/*` |
|        - |  8390 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|        - |  8391 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|        - |  8392 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|        - |  8393 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|        - |  8394 | ` */` |
|  5441250 |  8395 | `static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|        5 |  8396 | `{` |
|  5441255 |  8397 | `	*pnTok = 0;` |
|  5441250 |  8398 | `	if( &pTok[3] < pEnd` |
|  5141950 |  8399 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|  4326308 |  8400 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|  1904991 |  8401 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|       16 |  8402 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|       16 |  8403 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|       21 |  8404 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|       17 |  8405 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|       17 |  8406 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|       17 |  8407 | `			*pnTok = 4;` |
|       17 |  8408 | `			return nKw;` |
|        - |  8409 | `		}` |
|      ! 0 |  8410 | `	}` |
|  5441239 |  8411 | `	return 0;` |
|  2720630 |  8412 | `}` |
|        - |  8413 | `/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */` |
|       16 |  8414 | `static sxi32 GenStateSetVisFlag(sxi32 nKw)` |
|        1 |  8415 | `{` |
|       17 |  8416 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|       13 |  8417 | `		return PH7_CLASS_ATTR_PRIVATE_SET;` |
|        - |  8418 | `	}` |
|        5 |  8419 | `	if( nKw == PH7_TKWRD_PROTECTED ){` |
|        3 |  8420 | `		return PH7_CLASS_ATTR_PROTECTED_SET;` |
|        - |  8421 | `	}` |
|        3 |  8422 | `	return PH7_CLASS_ATTR_PUBLIC_SET;` |
|        9 |  8423 | `}` |
|   365312 |  8424 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 |  8425 | `{` |
|   365317 |  8426 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  8427 | `	ph7_class_attr *pAttr;` |
|        - |  8428 | `	SyString *pName;` |
|        - |  8429 | `	sxi32 rc;` |
|   365317 |  8430 | `	sxu32 nType = 0;` |
|        - |  8431 | `	SyString sTypeClass;` |
|        - |  8432 | `	SyString sTypeText;` |
|        - |  8433 | `	SySet aUnionAlts;` |
|   365317 |  8434 | `	sxi32 iTypeFlags = 0;` |
|   365317 |  8435 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   365317 |  8436 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   365317 |  8437 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  8438 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|        - |  8439 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|        - |  8440 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   365317 |  8441 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|       21 |  8442 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|        9 |  8443 | `	}` |
|        - |  8444 | `	/* Extract visibility level */` |
|   365317 |  8445 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - |  8446 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   365478 |  8447 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      327 |  8448 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|      327 |  8449 | `		if( rc == SXERR_CORRUPT ){` |
|        - |  8450 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|      ! 0 |  8451 | `			goto Synchronize;` |
|      327 |  8452 | `		}else if( rc == SXERR_SYNTAX ){` |
|      ! 0 |  8453 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8454 | `				"Invalid property type or declaration near '%z'",` |
|      ! 0 |  8455 | `				&pGen->pIn->sData);` |
|      ! 0 |  8456 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8457 | `				return SXERR_ABORT;` |
|        - |  8458 | `			}` |
|      ! 0 |  8459 | `			goto Synchronize;` |
|      327 |  8460 | `		}else if( rc == SXERR_ABORT ){` |
|      ! 0 |  8461 | `			return SXERR_ABORT;` |
|        - |  8462 | `		}` |
|      161 |  8463 | `	}` |
|      ! 0 |  8464 | `loop:` |
|   365321 |  8465 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  8466 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|      ! 0 |  8467 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8468 | `			return SXERR_ABORT;` |
|        - |  8469 | `		}` |
|      ! 0 |  8470 | `		goto Synchronize;` |
|        - |  8471 | `	}` |
|   365321 |  8472 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   365321 |  8473 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        - |  8474 | `		/* Invalid attribute name */` |
|      ! 0 |  8475 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|      ! 0 |  8476 | `		if( rc == SXERR_ABORT ){` |
|        - |  8477 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8478 | `			return SXERR_ABORT;` |
|        - |  8479 | `		}` |
|      ! 0 |  8480 | `		goto Synchronize;` |
|        - |  8481 | `	}` |
|        - |  8482 | `	/* Peek attribute name */` |
|   365321 |  8483 | `	pName = &pGen->pIn->sData;` |
|        - |  8484 | `	/* Advance the stream cursor */` |
|   365321 |  8485 | `	pGen->pIn++;` |
|   365321 |  8486 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/\|PH7_TK_OCB/*'{' hooks*/)) == 0 ){` |
|        - |  8487 | `		/* Invalid declaration */` |
|        3 |  8488 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|        3 |  8489 | `		if( rc == SXERR_ABORT ){` |
|        - |  8490 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8491 | `			return SXERR_ABORT;` |
|        - |  8492 | `		}` |
|        3 |  8493 | `		goto Synchronize;` |
|        - |  8494 | `	}` |
|        - |  8495 | `	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and` |
|        - |  8496 | `	 * the read visibility must not be narrower than the set visibility. */` |
|   365319 |  8497 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
|       13 |  8498 | `		const char *zAvErr = 0;` |
|       19 |  8499 | `		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE` |
|       10 |  8500 | `			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED` |
|        2 |  8501 | `			: PH7_CLASS_PROT_PUBLIC;` |
|       13 |  8502 | `		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  8503 | `			zAvErr = "Property with asymmetric visibility %z::$%z must have type";` |
|       13 |  8504 | `		}else if( iProtection > iSetLevel ){` |
|      ! 0 |  8505 | `			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";` |
|      ! 0 |  8506 | `		}` |
|       13 |  8507 | `		if( zAvErr ){` |
|      ! 0 |  8508 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sName,pName);` |
|      ! 0 |  8509 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8510 | `				return SXERR_ABORT;` |
|        - |  8511 | `			}` |
|      ! 0 |  8512 | `			goto Synchronize;` |
|        - |  8513 | `		}` |
|        6 |  8514 | `	}` |
|        - |  8515 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|        - |  8516 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   365319 |  8517 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|       43 |  8518 | `		const char *zRoErr = 0;` |
|       43 |  8519 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        3 |  8520 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|       42 |  8521 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        6 |  8522 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|       39 |  8523 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|        6 |  8524 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|        2 |  8525 | `		}` |
|       43 |  8526 | `		if( zRoErr ){` |
|       13 |  8527 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|       13 |  8528 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8529 | `				return SXERR_ABORT;` |
|        - |  8530 | `			}` |
|       13 |  8531 | `			goto Synchronize;` |
|        - |  8532 | `		}` |
|       14 |  8533 | `	}` |
|        - |  8534 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|        - |  8535 | `	 * type atom or any union alternative. void/never are already rejected` |
|        - |  8536 | `	 * by the type parser. */` |
|   365309 |  8537 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      485 |  8538 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|        - |  8539 | `			&sTypeText,` |
|      320 |  8540 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      160 |  8541 | `			"Property %z::$%z cannot have type %z",nLine);` |
|      325 |  8542 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8543 | `			return SXERR_ABORT;` |
|      325 |  8544 | `		}else if( rc != SXRET_OK ){` |
|      ! 0 |  8545 | `			goto Synchronize;` |
|        - |  8546 | `		}` |
|      160 |  8547 | `	}` |
|        - |  8548 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   365309 |  8549 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|        4 |  8550 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  8551 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|        3 |  8552 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8553 | `			return SXERR_ABORT;` |
|        - |  8554 | `		}` |
|        3 |  8555 | `		goto Synchronize;` |
|        - |  8556 | `	}` |
|        - |  8557 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|        - |  8558 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|        - |  8559 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|        - |  8560 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|        - |  8561 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|        - |  8562 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|   365307 |  8563 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|        6 |  8564 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8565 | `			"New expressions are not supported in this context");` |
|        6 |  8566 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8567 | `			return SXERR_ABORT;` |
|        - |  8568 | `		}` |
|        6 |  8569 | `		goto Synchronize;` |
|        - |  8570 | `	}` |
|        - |  8571 | `	/* Allocate a new class attribute */` |
|   365303 |  8572 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   365303 |  8573 | `	if( pAttr ){` |
|   365303 |  8574 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|   365303 |  8575 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  8576 | `			return SXERR_ABORT;` |
|        - |  8577 | `		}` |
|   182649 |  8578 | `	}` |
|   365303 |  8579 | `	if( pAttr == 0 ){` |
|      ! 0 |  8580 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  8581 | `		return SXERR_ABORT;` |
|        - |  8582 | `	}` |
|   365303 |  8583 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      323 |  8584 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      159 |  8585 | `	}` |
|   365303 |  8586 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|        - |  8587 | `		SySet *pInstrContainer;` |
|   240991 |  8588 | `		SyToken *pSavedDefEnd = pGen->pEnd;` |
|   240991 |  8589 | `		pGen->pIn++; /*Jump the equal sign */` |
|        - |  8590 | `		{` |
|        - |  8591 | `			/* Delimit the default expression: it ends at the declaration's` |
|        - |  8592 | `			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list` |
|        - |  8593 | ``			 * (`public string $w = "init" { get => …; }`) — the expression`` |
|        - |  8594 | `			 * compiler would otherwise run into the hook tokens. */` |
|   240991 |  8595 | `			SyToken *pScan = pGen->pIn;` |
|   240991 |  8596 | `			sxi32 iNest = 0;` |
|   509453 |  8597 | `			while( pScan < pGen->pEnd ){` |
|   509453 |  8598 | `				if( pScan->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|    15589 |  8599 | `					iNest++;` |
|   501661 |  8600 | `				}else if( pScan->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|    15589 |  8601 | `					iNest--;` |
|   486077 |  8602 | `				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI\|PH7_TK_COMMA\|PH7_TK_OCB)) ){` |
|   240991 |  8603 | `					break;` |
|        - |  8604 | `				}` |
|   268467 |  8605 | `				pScan++;` |
|        5 |  8606 | `			}` |
|   240991 |  8607 | `			pGen->pEnd = pScan;` |
|        - |  8608 | `		}` |
|        - |  8609 | `		/* Swap bytecode container */` |
|   240991 |  8610 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   240991 |  8611 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|        - |  8612 | `		/* Compile attribute value.` |
|        - |  8613 | `		 */` |
|   240991 |  8614 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   240991 |  8615 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 |  8616 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|      ! 0 |  8617 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8618 | `				return SXERR_ABORT;` |
|        - |  8619 | `			}` |
|      ! 0 |  8620 | `		}` |
|        - |  8621 | `		/* Emit the done instruction */` |
|   240991 |  8622 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   240991 |  8623 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   240991 |  8624 | `		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */` |
|   240991 |  8625 | `		pGen->pEnd = pSavedDefEnd;` |
|   120493 |  8626 | `	}` |
|        - |  8627 | `	/* All done,install the attribute */` |
|   365303 |  8628 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   365303 |  8629 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8630 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8631 | `		return SXERR_ABORT;` |
|        - |  8632 | `	}` |
|   365303 |  8633 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){` |
|        - |  8634 | ``		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.`` |
|        - |  8635 | `		 * The list ends the declaration at '}' — no trailing ';', no comma list. */` |
|       95 |  8636 | `		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);` |
|       95 |  8637 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8638 | `			return SXERR_ABORT;` |
|        - |  8639 | `		}` |
|       95 |  8640 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  8641 | `			goto Synchronize;` |
|        - |  8642 | `		}` |
|       95 |  8643 | `		SySetRelease(&aUnionAlts);` |
|       95 |  8644 | `		return SXRET_OK;` |
|        - |  8645 | `	}` |
|   365209 |  8646 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - |  8647 | ``		/* php 8.4: `abstract` on a property requires a hook list (php's exact`` |
|        - |  8648 | `		 * wording differs per declaration site) */` |
|      ! 0 |  8649 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  8650 | `			(pClass->iFlags & PH7_CLASS_INTERFACE)` |
|        - |  8651 | `				? "Interfaces may only include hooked properties"` |
|        - |  8652 | `				: "Only hooked properties may be declared abstract");` |
|      ! 0 |  8653 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8654 | `			return SXERR_ABORT;` |
|        - |  8655 | `		}` |
|      ! 0 |  8656 | `		goto Synchronize;` |
|        - |  8657 | `	}` |
|   365209 |  8658 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  8659 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|        5 |  8660 | `		pGen->pIn++; /* Jump the comma */` |
|        5 |  8661 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|      ! 0 |  8662 | `			SyToken *pTok = pGen->pIn;` |
|      ! 0 |  8663 | `			if( pTok >= pGen->pEnd ){` |
|      ! 0 |  8664 | `				pTok--;` |
|      ! 0 |  8665 | `			}` |
|      ! 0 |  8666 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  8667 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|      ! 0 |  8668 | `				&pTok->sData,&pClass->sName);` |
|      ! 0 |  8669 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8670 | `				return SXERR_ABORT;` |
|        - |  8671 | `			}` |
|      ! 0 |  8672 | `		}else{` |
|        5 |  8673 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        5 |  8674 | `				goto loop;` |
|        - |  8675 | `			}` |
|        - |  8676 | `		}` |
|      ! 0 |  8677 | `	}` |
|   365205 |  8678 | `	SySetRelease(&aUnionAlts);` |
|   365205 |  8679 | `	return SXRET_OK;` |
|        9 |  8680 | `Synchronize:` |
|        - |  8681 | `	/* Synchronize with the first semi-colon */` |
|       56 |  8682 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       37 |  8683 | `		pGen->pIn++;` |
|        3 |  8684 | `	}` |
|       22 |  8685 | `	SySetRelease(&aUnionAlts);` |
|       22 |  8686 | `	return SXERR_CORRUPT;` |
|   182661 |  8687 | `}` |
|        - |  8688 | `/*` |
|        - |  8689 | ` * Compile a class method.` |
|        - |  8690 | ` *` |
|        - |  8691 | ` * Refer to the official documentation for more information` |
|        - |  8692 | ` * on the powerful extension introduced by the PH7 engine` |
|        - |  8693 | ` * to the OO subsystem such as full type hinting,method` |
|        - |  8694 | ` * overloading and many more.` |
|        - |  8695 | ` */` |
|  1900832 |  8696 | `static sxi32 GenStateCompileClassMethod(` |
|        - |  8697 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  8698 | `	sxi32 iProtection,   /* Visibility level */` |
|        - |  8699 | `	sxi32 iFlags,        /* Configuration flags */` |
|        - |  8700 | `	int doBody,          /* TRUE to process method body */` |
|        - |  8701 | `	ph7_class *pClass    /* Class this method belongs */` |
|        - |  8702 | `	)` |
|        5 |  8703 | `{` |
|  1900837 |  8704 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  1900837 |  8705 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|        - |  8706 | `	ph7_class_method *pMeth;` |
|        - |  8707 | `	sxi32 iFuncFlags;` |
|        - |  8708 | `	SyString *pName;` |
|        - |  8709 | `	SyToken *pEnd;` |
|        - |  8710 | `	sxi32 rc;` |
|        - |  8711 | `	/* Extract visibility level */` |
|  1900837 |  8712 | `	iProtection = GetProtectionLevel(iProtection);` |
|  1900837 |  8713 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  1900837 |  8714 | `	iFuncFlags = 0;` |
|  1900837 |  8715 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  8716 | `		/* Invalid method name */` |
|      ! 0 |  8717 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|      ! 0 |  8718 | `		if( rc == SXERR_ABORT ){` |
|        - |  8719 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8720 | `			return SXERR_ABORT;` |
|        - |  8721 | `		}` |
|      ! 0 |  8722 | `		goto Synchronize;` |
|        - |  8723 | `	}` |
|  1900837 |  8724 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        - |  8725 | `		/* Return by reference,remember that */` |
|      ! 0 |  8726 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|        - |  8727 | `		/* Jump the '&' token */` |
|      ! 0 |  8728 | `		pGen->pIn++;` |
|      ! 0 |  8729 | `	}` |
|  1900837 |  8730 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  8731 | `		/* Invalid method name */` |
|      ! 0 |  8732 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|      ! 0 |  8733 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8734 | `			return SXERR_ABORT;` |
|        - |  8735 | `		}` |
|      ! 0 |  8736 | `		goto Synchronize;` |
|        - |  8737 | `	}` |
|        - |  8738 | `	/* Peek method name */` |
|  1900837 |  8739 | `	pName = &pGen->pIn->sData;` |
|  1900837 |  8740 | `	nLine = pGen->pIn->nLine;` |
|        - |  8741 | `	/* Jump the method name */` |
|  1900837 |  8742 | `	pGen->pIn++;` |
|  1900837 |  8743 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - |  8744 | `		/* Abstract method */` |
|   112471 |  8745 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|      ! 0 |  8746 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8747 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|      ! 0 |  8748 | `				&pClass->sName,pName);` |
|      ! 0 |  8749 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8750 | `				return SXERR_ABORT;` |
|        - |  8751 | `			}` |
|      ! 0 |  8752 | `		}` |
|        - |  8753 | `		/* Assemble method signature only */` |
|   112471 |  8754 | `		doBody = FALSE;` |
|    56233 |  8755 | `	}` |
|  1900837 |  8756 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  8757 | `		/* Syntax error */` |
|      ! 0 |  8758 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|      ! 0 |  8759 | `		if( rc == SXERR_ABORT ){` |
|        - |  8760 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8761 | `			return SXERR_ABORT;` |
|        - |  8762 | `		}` |
|      ! 0 |  8763 | `		goto Synchronize;` |
|        - |  8764 | `	}` |
|        - |  8765 | `	/* Allocate a new class_method instance */` |
|  1900837 |  8766 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  1900837 |  8767 | `	if( pMeth == 0 ){` |
|      ! 0 |  8768 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8769 | `		return SXERR_ABORT;` |
|        - |  8770 | `	}` |
|  1900837 |  8771 | `	pMeth->sFunc.nLine = nKwLine;` |
|  1900837 |  8772 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|  1900837 |  8773 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  8774 | `		return SXERR_ABORT;` |
|        - |  8775 | `	}` |
|        - |  8776 | `	/* Jump the left parenthesis '(' */` |
|  1900837 |  8777 | `	pGen->pIn++;` |
|  1900837 |  8778 | `	pEnd = 0; /* cc warning */` |
|        - |  8779 | `	/* Delimit the method signature */` |
|  1900837 |  8780 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  1900837 |  8781 | `	if( pEnd >= pGen->pEnd ){` |
|        - |  8782 | `		/* Syntax error */` |
|        3 |  8783 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|        3 |  8784 | `		if( rc == SXERR_ABORT ){` |
|        - |  8785 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8786 | `			return SXERR_ABORT;` |
|        - |  8787 | `		}` |
|        3 |  8788 | `		goto Synchronize;` |
|        - |  8789 | `	}` |
|        - |  8790 | `	{` |
|  1900835 |  8791 | `		int bIsCtor = 0;` |
|  1900835 |  8792 | `		int bAbstractCtor = 0;` |
|  1900830 |  8793 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  1109486 |  8794 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  1825149 |  8795 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   151377 |  8796 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        3 |  8797 | `				bAbstractCtor = 1;` |
|        2 |  8798 | `			}else{` |
|   151375 |  8799 | `				bIsCtor = 1;` |
|        - |  8800 | `			}` |
|    75686 |  8801 | `		}` |
|  1900835 |  8802 | `		if( pGen->pIn < pEnd ){` |
|        - |  8803 | `			/* Collect method arguments */` |
|   659593 |  8804 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   659593 |  8805 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8806 | `				return SXERR_ABORT;` |
|        - |  8807 | `			}` |
|   329794 |  8808 | `		}` |
|        - |  8809 | `	}` |
|        - |  8810 | `	/* Point past ')' and parse optional return type ': type' */` |
|  1900835 |  8811 | `	pGen->pIn = &pEnd[1];` |
|        - |  8812 | `	{` |
|  1900835 |  8813 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  1900835 |  8814 | `		if( rcRt == SXERR_ABORT ){` |
|      ! 0 |  8815 | `			return SXERR_ABORT;` |
|  1900835 |  8816 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|      ! 0 |  8817 | `			goto Synchronize;` |
|        - |  8818 | `		}` |
|        - |  8819 | `	}` |
|        - |  8820 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|        - |  8821 | `	 * property init/typecheck is handled by the generic typed-property path` |
|        - |  8822 | `	 * since we mint real ph7_class_attr entries. */` |
|        - |  8823 | `	{` |
|  1900835 |  8824 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|        - |  8825 | `		sxu32 i;` |
|  2932687 |  8826 | `		for( i = 0; i < nArg; i++ ){` |
|  1031867 |  8827 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|        - |  8828 | `			ph7_class_attr *pAttr;` |
|  1031867 |  8829 | `			sxi32 iAttrFlags = 0;` |
|        - |  8830 | `			int bArgTyped;` |
|  1031867 |  8831 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  1031783 |  8832 | `				continue;` |
|        - |  8833 | `			}` |
|        - |  8834 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|        - |  8835 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|        - |  8836 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|       59 |  8837 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|       90 |  8838 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|       89 |  8839 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        3 |  8840 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8841 | `					"Cannot declare variadic promoted property");` |
|        3 |  8842 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8843 | `					return SXERR_ABORT;` |
|        - |  8844 | `				}` |
|        3 |  8845 | `				goto Synchronize;` |
|        - |  8846 | `			}` |
|        - |  8847 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|        - |  8848 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|        - |  8849 | `			 * appear as an alternative of a union type. */` |
|       87 |  8850 | `			if( bArgTyped ){` |
|      122 |  8851 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|       78 |  8852 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|       78 |  8853 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|       39 |  8854 | `					"Property %z::$%z cannot have type %z",nLine);` |
|       83 |  8855 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8856 | `					return SXERR_ABORT;` |
|       83 |  8857 | `				}else if( rc != SXRET_OK ){` |
|        6 |  8858 | `					goto Synchronize;` |
|        - |  8859 | `				}` |
|       37 |  8860 | `			}` |
|        - |  8861 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|       83 |  8862 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|        4 |  8863 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  8864 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|        3 |  8865 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8866 | `					return SXERR_ABORT;` |
|        - |  8867 | `				}` |
|        3 |  8868 | `				goto Synchronize;` |
|        - |  8869 | `			}` |
|       81 |  8870 | `			if( bArgTyped ){` |
|       77 |  8871 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|       36 |  8872 | `			}` |
|       81 |  8873 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|        3 |  8874 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|        1 |  8875 | `			}` |
|       81 |  8876 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|        8 |  8877 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|        3 |  8878 | `			}` |
|       81 |  8879 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|        - |  8880 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|        - |  8881 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|       26 |  8882 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        4 |  8883 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  8884 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|        3 |  8885 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  8886 | `						return SXERR_ABORT;` |
|        - |  8887 | `					}` |
|        3 |  8888 | `					goto Synchronize;` |
|        - |  8889 | `				}` |
|       24 |  8890 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       10 |  8891 | `			}` |
|       79 |  8892 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|        - |  8893 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|        5 |  8894 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  8895 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8896 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|      ! 0 |  8897 | `						&pClass->sName,&pArg->sName);` |
|      ! 0 |  8898 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  8899 | `						return SXERR_ABORT;` |
|        - |  8900 | `					}` |
|      ! 0 |  8901 | `					goto Synchronize;` |
|        - |  8902 | `				}` |
|        5 |  8903 | `				iAttrFlags \|= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)` |
|        2 |  8904 | `					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;` |
|        2 |  8905 | `			}` |
|       79 |  8906 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|       79 |  8907 | `			if( pAttr == 0 ){` |
|      ! 0 |  8908 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8909 | `				return SXERR_ABORT;` |
|        - |  8910 | `			}` |
|       79 |  8911 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|       77 |  8912 | `				pAttr->nType = pArg->nType;` |
|       77 |  8913 | `				pAttr->sClass = pArg->sClass;` |
|       77 |  8914 | `				pAttr->sTypeName = pArg->sTypeName;` |
|       77 |  8915 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|        - |  8916 | `					sxu32 k;` |
|       20 |  8917 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|       14 |  8918 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|       14 |  8919 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|        8 |  8920 | `					}` |
|        3 |  8921 | `				}` |
|       36 |  8922 | `			}` |
|       79 |  8923 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|       79 |  8924 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  8925 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8926 | `				return SXERR_ABORT;` |
|        - |  8927 | `			}` |
|       42 |  8928 | `		}` |
|        - |  8929 | `	}` |
|  1900825 |  8930 | `	if( doBody ){` |
|        - |  8931 | `		/* Compile method body */` |
|  1788359 |  8932 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  1788359 |  8933 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8934 | `			return SXERR_ABORT;` |
|        - |  8935 | `		}` |
|        - |  8936 | `		/* The cursor sits just past the body's closing brace */` |
|  1788359 |  8937 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|   894182 |  8938 | `	}else{` |
|        - |  8939 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|   112471 |  8940 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|   112471 |  8941 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|    56233 |  8942 | `		}` |
|        - |  8943 | `		/* Only method signature is allowed */` |
|   112471 |  8944 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|      ! 0 |  8945 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  8946 | `				"Expected ';' after method signature '%z'",pName);` |
|      ! 0 |  8947 | `				if( rc == SXERR_ABORT ){` |
|        - |  8948 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 |  8949 | `					return SXERR_ABORT;` |
|        - |  8950 | `				}` |
|      ! 0 |  8951 | `				return SXERR_CORRUPT;` |
|        - |  8952 | `			}` |
|        - |  8953 | `	}` |
|        - |  8954 | `	/* All done,install the method */` |
|  1900825 |  8955 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  1900825 |  8956 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8957 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8958 | `		return SXERR_ABORT;` |
|        - |  8959 | `	}` |
|  1900825 |  8960 | `	return SXRET_OK;` |
|        6 |  8961 | `Synchronize:` |
|        - |  8962 | `	/* Synchronize with the first semi-colon */` |
|       40 |  8963 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       28 |  8964 | `		pGen->pIn++;` |
|        4 |  8965 | `	}` |
|       16 |  8966 | `	return SXERR_CORRUPT;` |
|   950421 |  8967 | `}` |
|        - |  8968 | `/*` |
|        - |  8969 | `` * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a`` |
|        - |  8970 | ` * property declaration. Each hook body is synthesized into a hidden public` |
|        - |  8971 | ` * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,` |
|        - |  8972 | ` * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /` |
|        - |  8973 | ` * OP_STORE route reads and plain writes through them (a per-instance guard` |
|        - |  8974 | ` * makes $this->NAME inside a hook body address the raw backing slot — php's` |
|        - |  8975 | `` * rule that hooks see the backing store). `get => expr;` compiles as an`` |
|        - |  8976 | `` * implicit return (the arrow-fn pattern); `set => expr;` compiles the same`` |
|        - |  8977 | ` * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return` |
|        - |  8978 | `` * value to the backing slot. A `set` without a parameter list receives the`` |
|        - |  8979 | `` * implicit `$value` formal.`` |
|        - |  8980 | ` * On entry pGen->pIn sits on '{'; on success it sits just past '}'.` |
|        - |  8981 | ` */` |
|        - |  8982 | `/*` |
|        - |  8983 | `` * Whether any token in [pStart, pEnd) spells `$this->NAME` (this property's own`` |
|        - |  8984 | `` * name; `?->` and `::` member ops count too). php 8.4's virtual-vs-backed rule:`` |
|        - |  8985 | ` * a hooked property is BACKED iff any of its OWN hook bodies references it by` |
|        - |  8986 | ` * name through $this — otherwise it is VIRTUAL: no backing store, no default` |
|        - |  8987 | ` * allowed, excluded from the raw object surfaces.` |
|        - |  8988 | ` */` |
|       94 |  8989 | `static int GenStateHookBodyRefsProp(SyToken *pStart,SyToken *pEnd,const SyString *pName)` |
|        1 |  8990 | `{` |
|        - |  8991 | `	SyToken *p;` |
|      345 |  8992 | `	for( p = pStart ; p + 1 < pEnd ; p++ ){` |
|      303 |  8993 | `		if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      223 |  8994 | `			continue;` |
|        - |  8995 | `		}` |
|        - |  8996 | ``		/* `$this->NAME` (also `?->`/`::`) */`` |
|       80 |  8997 | `		if( p + 3 < pEnd` |
|       80 |  8998 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       80 |  8999 | `		 && p[1].sData.nByte == sizeof("this")-1` |
|       73 |  9000 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0` |
|       66 |  9001 | `		 && GenStateTokenIsMemberOp(&p[2])` |
|       66 |  9002 | `		 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|       66 |  9003 | `		 && p[3].sData.nByte == pName->nByte` |
|       60 |  9004 | `		 && SyMemcmp((const void *)p[3].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|       51 |  9005 | `			return 1;` |
|        - |  9006 | `		}` |
|        - |  9007 | ``		/* `parent::$NAME` (the parent::$x::get() hook-call form): the parent`` |
|        - |  9008 | `		 * hook operates on the shared per-instance backing store, so the` |
|        - |  9009 | `		 * property is backed (php compiles a default alongside it). */` |
|       30 |  9010 | `		if( p > pStart` |
|       26 |  9011 | `		 && GenStateTokenIsMemberOp(&p[-1])` |
|       12 |  9012 | `		 && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        2 |  9013 | `		 && p[1].sData.nByte == pName->nByte` |
|        3 |  9014 | `		 && SyMemcmp((const void *)p[1].sData.zString,(const void *)pName->zString,pName->nByte) == 0 ){` |
|        3 |  9015 | `			return 1;` |
|        - |  9016 | `		}` |
|       15 |  9017 | `	}` |
|       43 |  9018 | `	return 0;` |
|       48 |  9019 | `}` |
|        - |  9020 | `/*` |
|        - |  9021 | ` * True when p opens php 8.4's parent-hook call form` |
|        - |  9022 | `` * `parent :: $ NAME :: get\|set (` (7 tokens through the '(').`` |
|        - |  9023 | ` */` |
|      990 |  9024 | `static int GenStateIsParentHookCallAt(SyToken *p,SyToken *pEnd)` |
|        1 |  9025 | `{` |
|     1167 |  9026 | `	return p + 6 < pEnd` |
|      671 |  9027 | `	 && (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|      250 |  9028 | `	 && p->sData.nByte == sizeof("parent")-1` |
|       81 |  9029 | `	 && SyMemcmp((const void *)p->sData.zString,(const void *)"parent",sizeof("parent")-1) == 0` |
|       11 |  9030 | `	 && GenStateTokenIsMemberOp(&p[1])` |
|        8 |  9031 | `	 && (p[2].nType & PH7_TK_DOLLAR) != 0` |
|        8 |  9032 | `	 && (p[3].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        8 |  9033 | `	 && GenStateTokenIsMemberOp(&p[4])` |
|        8 |  9034 | `	 && (p[5].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|        8 |  9035 | `	 && p[5].sData.nByte == 3` |
|        8 |  9036 | `	 && (SyMemcmp((const void *)p[5].sData.zString,(const void *)"get",3) == 0` |
|        6 |  9037 | `	  \|\| SyMemcmp((const void *)p[5].sData.zString,(const void *)"set",3) == 0)` |
|     1166 |  9038 | `	 && (p[6].nType & PH7_TK_LPAREN) != 0;` |
|        1 |  9039 | `}` |
|        - |  9040 | `/*` |
|        - |  9041 | `` * Rewrite php 8.4 `parent::$x::get(...)` / `parent::$x::set(...)` calls in a`` |
|        - |  9042 | ` * hook body into calls of the parent class's synthesized hook method` |
|        - |  9043 | `` * (`parent::__phl_hook_get_x(...)`). Builds a token COPY into pCopy (only`` |
|        - |  9044 | ` * called when GenStateIsParentHookCallAt matched somewhere in the range);` |
|        - |  9045 | ` * copied tokens keep pointing at source-owned lexeme storage, and the` |
|        - |  9046 | ` * synthesized method-name lexemes are VM-allocator owned. Returns SXRET_OK` |
|        - |  9047 | ` * or SXERR_MEM.` |
|        - |  9048 | ` */` |
|        4 |  9049 | `static sxi32 GenStateRewriteParentHookCalls(ph7_gen_state *pGen,SySet *pCopy,` |
|        - |  9050 | `	SyToken *pStart,SyToken *pEnd)` |
|        1 |  9051 | `{` |
|        5 |  9052 | `	SyToken *p = pStart;` |
|       35 |  9053 | `	while( p < pEnd ){` |
|       31 |  9054 | `		if( GenStateIsParentHookCallAt(p,pEnd) ){` |
|        - |  9055 | `			SyToken sTok;` |
|        - |  9056 | `			char zName[384];` |
|        - |  9057 | `			sxu32 nName;` |
|        - |  9058 | `			char *zDup;` |
|        - |  9059 | ``			/* `parent` `::` */`` |
|        5 |  9060 | `			SySetPut(pCopy,(const void *)&p[0]);` |
|        5 |  9061 | `			SySetPut(pCopy,(const void *)&p[1]);` |
|        7 |  9062 | `			nName = SyBufferFormat(zName,sizeof(zName),"__phl_hook_%.3s_%.*s",` |
|        4 |  9063 | `				p[5].sData.zString,(int)p[3].sData.nByte,p[3].sData.zString);` |
|        5 |  9064 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|        5 |  9065 | `			if( zDup == 0 ){` |
|      ! 0 |  9066 | `				return SXERR_MEM;` |
|        - |  9067 | `			}` |
|        5 |  9068 | `			sTok = p[3]; /* keep the line info of the property name */` |
|        5 |  9069 | `			sTok.nType = PH7_TK_ID;` |
|        5 |  9070 | `			SyStringInitFromBuf(&sTok.sData,zDup,nName);` |
|        5 |  9071 | `			sTok.pUserData = 0;` |
|        5 |  9072 | `			SySetPut(pCopy,(const void *)&sTok);` |
|        5 |  9073 | `			p += 6; /* continue at the '(' — arguments copy through unchanged */` |
|        5 |  9074 | `			continue;` |
|        - |  9075 | `		}` |
|       27 |  9076 | `		SySetPut(pCopy,(const void *)p);` |
|       27 |  9077 | `		p++;` |
|        1 |  9078 | `	}` |
|        5 |  9079 | `	return SXRET_OK;` |
|        3 |  9080 | `}` |
|       94 |  9081 | `static sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 |  9082 | `{` |
|       95 |  9083 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  9084 | `	sxi32 rc;` |
|       95 |  9085 | `	int bRefsSelf = 0;` |
|       95 |  9086 | `	pGen->pIn++; /* Jump '{' */` |
|      253 |  9087 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|        - |  9088 | `		char zHook[384];` |
|        - |  9089 | `		SyString sHookName;` |
|        - |  9090 | `		ph7_class_method *pMeth;` |
|        - |  9091 | `		int bGet;` |
|      159 |  9092 | `		sxu32 nHLine = pGen->pIn->nLine;` |
|      159 |  9093 | `		if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       15 |  9094 | `			pGen->pIn++; /* stray ';' between hooks */` |
|       22 |  9095 | `			continue;` |
|        - |  9096 | `		}` |
|      145 |  9097 | `		if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|        - |  9098 | `			/* by-reference get hook: not modeled (loud, recorded) */` |
|      ! 0 |  9099 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - |  9100 | `				"By-reference property hooks are not supported for %z::$%z",` |
|      ! 0 |  9101 | `				&pClass->sName,&pAttr->sName);` |
|      ! 0 |  9102 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9103 | `				return SXERR_ABORT;` |
|        - |  9104 | `			}` |
|      ! 0 |  9105 | `			return SXERR_CORRUPT;` |
|        - |  9106 | `		}` |
|      145 |  9107 | `		if( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  9108 | `			goto HookSyntax;` |
|        - |  9109 | `		}` |
|      144 |  9110 | `		if( pGen->pIn->sData.nByte == 3` |
|      145 |  9111 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"get",3) == 0 ){` |
|       79 |  9112 | `			bGet = 1;` |
|      106 |  9113 | `		}else if( pGen->pIn->sData.nByte == 3` |
|       67 |  9114 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"set",3) == 0 ){` |
|       67 |  9115 | `			bGet = 0;` |
|       34 |  9116 | `		}else{` |
|      ! 0 |  9117 | `			goto HookSyntax;` |
|        - |  9118 | `		}` |
|      145 |  9119 | `		pGen->pIn++; /* Jump 'get'/'set' */` |
|      145 |  9120 | `		sHookName.zString = zHook;` |
|      217 |  9121 | `		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",` |
|       72 |  9122 | `			bGet ? "get" : "set",&pAttr->sName);` |
|      145 |  9123 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_CCB)) ){` |
|        - |  9124 | ``			/* Bare `get;` / `set;` — an ABSTRACT hook declaration (php 8.4):`` |
|        - |  9125 | ``			 * legal only on an `abstract` property or inside an interface. The`` |
|        - |  9126 | `			 * synthesized method carries PH7_CLASS_ATTR_ABSTRACT and rides the` |
|        - |  9127 | `			 * existing must-implement machinery; a concrete hook override (or a` |
|        - |  9128 | `			 * plain property, see GenStateCheckAbstractMethods) satisfies it. */` |
|       14 |  9129 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|        8 |  9130 | `			 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 |  9131 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - |  9132 | `					"Non-abstract property hook must have a body");` |
|      ! 0 |  9133 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9134 | `					return SXERR_ABORT;` |
|        - |  9135 | `				}` |
|      ! 0 |  9136 | `				return SXERR_CORRUPT;` |
|        - |  9137 | `			}` |
|       15 |  9138 | `			pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|        - |  9139 | `				PH7_CLASS_PROT_PUBLIC,PH7_CLASS_ATTR_ABSTRACT,0);` |
|       15 |  9140 | `			if( pMeth == 0 ){` |
|      ! 0 |  9141 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9142 | `				return SXERR_ABORT;` |
|        - |  9143 | `			}` |
|       15 |  9144 | `			pMeth->sFunc.nLine = nHLine;` |
|       15 |  9145 | `			if( !bGet ){` |
|        - |  9146 | ``				/* The implicit `$value` formal keeps the stub's signature`` |
|        - |  9147 | `				 * compatible with concrete set-hook implementations (which` |
|        - |  9148 | `				 * always carry one parameter). It takes the PROPERTY's declared` |
|        - |  9149 | `				 * type (php: the abstract set's parameter type IS the property` |
|        - |  9150 | `				 * type), so the override contravariance check accepts a typed` |
|        - |  9151 | ``				 * `set(int $v)` implementation on an `int $x` requirement. */`` |
|        - |  9152 | `				ph7_vm_func_arg sVArg;` |
|        7 |  9153 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|        7 |  9154 | `				if( zVName == 0 ){` |
|      ! 0 |  9155 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9156 | `					return SXERR_ABORT;` |
|        - |  9157 | `				}` |
|        7 |  9158 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|        7 |  9159 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|        7 |  9160 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        7 |  9161 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        7 |  9162 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|        7 |  9163 | `				sVArg.nType = pAttr->nType;` |
|        7 |  9164 | `				sVArg.sClass = pAttr->sClass;` |
|        7 |  9165 | `				sVArg.sTypeName = pAttr->sTypeName;` |
|        7 |  9166 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|      ! 0 |  9167 | `					sVArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|      ! 0 |  9168 | `				}` |
|        7 |  9169 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|        3 |  9170 | `			}` |
|       15 |  9171 | `			rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|       15 |  9172 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9173 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9174 | `				return SXERR_ABORT;` |
|        - |  9175 | `			}` |
|       15 |  9176 | `			pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|       15 |  9177 | `			continue; /* the loop consumes the ';' as a stray separator */` |
|        - |  9178 | `		}` |
|      130 |  9179 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0` |
|      131 |  9180 | `		 \|\| (pClass->iFlags & PH7_CLASS_INTERFACE) != 0 ){` |
|        - |  9181 | `			/* php: an abstract/interface property hook cannot carry a body */` |
|      ! 0 |  9182 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - |  9183 | `				"Abstract property hook cannot have body");` |
|      ! 0 |  9184 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9185 | `				return SXERR_ABORT;` |
|        - |  9186 | `			}` |
|      ! 0 |  9187 | `			return SXERR_CORRUPT;` |
|        - |  9188 | `		}` |
|      131 |  9189 | `		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|        - |  9190 | `			PH7_CLASS_PROT_PUBLIC,0,0);` |
|      131 |  9191 | `		if( pMeth == 0 ){` |
|      ! 0 |  9192 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9193 | `			return SXERR_ABORT;` |
|        - |  9194 | `		}` |
|      131 |  9195 | `		pMeth->sFunc.nLine = nHLine;` |
|      131 |  9196 | `		if( !bGet ){` |
|        - |  9197 | ``			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */`` |
|       61 |  9198 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       17 |  9199 | `				SyToken *pRp = 0;` |
|       17 |  9200 | `				pGen->pIn++;` |
|       17 |  9201 | `				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|       17 |  9202 | `				if( pRp >= pGen->pEnd ){` |
|      ! 0 |  9203 | `					goto HookSyntax;` |
|        - |  9204 | `				}` |
|       17 |  9205 | `				if( pGen->pIn < pRp ){` |
|       17 |  9206 | `					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);` |
|       17 |  9207 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9208 | `						return SXERR_ABORT;` |
|        - |  9209 | `					}` |
|        8 |  9210 | `				}` |
|       17 |  9211 | `				pGen->pIn = &pRp[1];` |
|        8 |  9212 | `			}` |
|       61 |  9213 | `			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){` |
|        - |  9214 | `				/* Implicit $value formal */` |
|        - |  9215 | `				ph7_vm_func_arg sVArg;` |
|       45 |  9216 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|       45 |  9217 | `				if( zVName == 0 ){` |
|      ! 0 |  9218 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9219 | `					return SXERR_ABORT;` |
|        - |  9220 | `				}` |
|       45 |  9221 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|       45 |  9222 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|       45 |  9223 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       45 |  9224 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       45 |  9225 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|       45 |  9226 | `				SyStringInitFromBuf(&sVArg.sTypeName,0,0);` |
|       45 |  9227 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|       22 |  9228 | `			}` |
|       30 |  9229 | `		}` |
|      165 |  9230 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|        - |  9231 | `			/* Block body */` |
|       69 |  9232 | `			SyToken *pBodyStart = pGen->pIn;` |
|       69 |  9233 | `			SyToken *pCloser = 0;` |
|       69 |  9234 | `			int bParentCall = 0;` |
|       69 |  9235 | `			PH7_DelimitNestedTokens(&pBodyStart[1],pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pCloser);` |
|       69 |  9236 | `			if( pCloser < pGen->pEnd ){` |
|        - |  9237 | `				SyToken *pScan;` |
|      753 |  9238 | `				for( pScan = &pBodyStart[1] ; pScan < pCloser ; pScan++ ){` |
|      687 |  9239 | `					if( GenStateIsParentHookCallAt(pScan,pCloser) ){` |
|        3 |  9240 | `						bParentCall = 1;` |
|        3 |  9241 | `						break;` |
|        - |  9242 | `					}` |
|      343 |  9243 | `				}` |
|       34 |  9244 | `			}` |
|       69 |  9245 | `			if( bParentCall ){` |
|        - |  9246 | ``				/* `parent::$x::get()` inside the body: compile a REWRITTEN copy`` |
|        - |  9247 | `				 * of the body tokens (the call becomes the parent's synthesized` |
|        - |  9248 | `				 * hook method), then continue past the original body. */` |
|        - |  9249 | `				SySet sBody;` |
|        3 |  9250 | `				SyToken *pSavedEnd = pGen->pEnd;` |
|        3 |  9251 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        3 |  9252 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,&pCloser[1]);` |
|        3 |  9253 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  9254 | `					SySetRelease(&sBody);` |
|      ! 0 |  9255 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9256 | `					return SXERR_ABORT;` |
|        - |  9257 | `				}` |
|        3 |  9258 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|        3 |  9259 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|        3 |  9260 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|        3 |  9261 | `				pGen->pIn = &pCloser[1];` |
|        3 |  9262 | `				pGen->pEnd = pSavedEnd;` |
|        3 |  9263 | `				SySetRelease(&sBody);` |
|        3 |  9264 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9265 | `					return SXERR_ABORT;` |
|        - |  9266 | `				}` |
|        3 |  9267 | `				pMeth->sFunc.nEndLine = pCloser->nLine;` |
|        2 |  9268 | `			}else{` |
|       67 |  9269 | `				rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|       67 |  9270 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9271 | `					return SXERR_ABORT;` |
|        - |  9272 | `				}` |
|       67 |  9273 | `				pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|        - |  9274 | `			}` |
|       69 |  9275 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|       17 |  9276 | `				bRefsSelf = 1;` |
|        9 |  9277 | `			}` |
|      128 |  9278 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){` |
|        - |  9279 | ``			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */`` |
|        - |  9280 | `			GenBlock *pBlock;` |
|        - |  9281 | `			SySet *pInstrContainer;` |
|        - |  9282 | `			SyToken *pBodyStart;` |
|        - |  9283 | `			SyToken *pExprEnd;` |
|       63 |  9284 | `			SyToken *pSavedEnd = 0;` |
|        - |  9285 | `			SySet sBody;` |
|       63 |  9286 | `			int bParentCall = 0;` |
|       63 |  9287 | `			pGen->pIn++; /* Jump '=>' */` |
|       63 |  9288 | `			pBodyStart = pGen->pIn;` |
|        - |  9289 | `			/* Delimit the expression (first top-level ';', or a closer that` |
|        - |  9290 | `			 * would end the enclosing hook list) and rewrite any` |
|        - |  9291 | ``			 * `parent::$x::get()` calls into the parent's synthesized hook`` |
|        - |  9292 | `			 * method on a token copy. */` |
|        - |  9293 | `			{` |
|       63 |  9294 | `				sxi32 iNest = 0;` |
|       63 |  9295 | `				pExprEnd = pBodyStart;` |
|      355 |  9296 | `				while( pExprEnd < pGen->pEnd ){` |
|      355 |  9297 | `					if( pExprEnd->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        9 |  9298 | `						iNest++;` |
|      351 |  9299 | `					}else if( pExprEnd->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        9 |  9300 | `						if( iNest <= 0 ){` |
|      ! 0 |  9301 | `							break;` |
|        - |  9302 | `						}` |
|        9 |  9303 | `						iNest--;` |
|      343 |  9304 | `					}else if( iNest <= 0 && (pExprEnd->nType & PH7_TK_SEMI) ){` |
|       63 |  9305 | `						break;` |
|        - |  9306 | `					}` |
|      293 |  9307 | `					pExprEnd++;` |
|        1 |  9308 | `				}` |
|        - |  9309 | `			}` |
|        - |  9310 | `			{` |
|        - |  9311 | `				SyToken *pScan;` |
|      335 |  9312 | `				for( pScan = pBodyStart ; pScan < pExprEnd ; pScan++ ){` |
|      275 |  9313 | `					if( GenStateIsParentHookCallAt(pScan,pExprEnd) ){` |
|        3 |  9314 | `						bParentCall = 1;` |
|        3 |  9315 | `						break;` |
|        - |  9316 | `					}` |
|      137 |  9317 | `				}` |
|        - |  9318 | `			}` |
|       63 |  9319 | `			if( bParentCall ){` |
|        3 |  9320 | `				SySetInit(&sBody,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        3 |  9321 | `				rc = GenStateRewriteParentHookCalls(&(*pGen),&sBody,pBodyStart,pExprEnd);` |
|        3 |  9322 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  9323 | `					SySetRelease(&sBody);` |
|      ! 0 |  9324 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9325 | `					return SXERR_ABORT;` |
|        - |  9326 | `				}` |
|        3 |  9327 | `				pSavedEnd = pGen->pEnd;` |
|        3 |  9328 | `				pGen->pIn = (SyToken *)SySetBasePtr(&sBody);` |
|        3 |  9329 | `				pGen->pEnd = &pGen->pIn[SySetUsed(&sBody)];` |
|        1 |  9330 | `			}` |
|       94 |  9331 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       62 |  9332 | `				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);` |
|       63 |  9333 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9334 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");` |
|      ! 0 |  9335 | `				return SXERR_ABORT;` |
|        - |  9336 | `			}` |
|       63 |  9337 | `			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       63 |  9338 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|       63 |  9339 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|       63 |  9340 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       63 |  9341 | `			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       63 |  9342 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       63 |  9343 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       63 |  9344 | `			GenStateLeaveBlock(&(*pGen),0);` |
|       63 |  9345 | `			if( bParentCall ){` |
|        3 |  9346 | `				pGen->pIn = pExprEnd; /* land on the original ';' */` |
|        3 |  9347 | `				pGen->pEnd = pSavedEnd;` |
|        3 |  9348 | `				SySetRelease(&sBody);` |
|        1 |  9349 | `			}` |
|       63 |  9350 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9351 | `				return SXERR_ABORT;` |
|        - |  9352 | `			}` |
|       63 |  9353 | `			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;` |
|       63 |  9354 | `			if( !bRefsSelf && GenStateHookBodyRefsProp(pBodyStart,pGen->pIn,&pAttr->sName) ){` |
|       37 |  9355 | `				bRefsSelf = 1;` |
|       18 |  9356 | `			}` |
|       63 |  9357 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       63 |  9358 | `				pGen->pIn++; /* Jump ';' */` |
|       31 |  9359 | `			}` |
|       63 |  9360 | `			if( !bGet ){` |
|        - |  9361 | ``				/* `set => expr` assigns the expression to the backing store:`` |
|        - |  9362 | `				 * the dispatcher consumes the implicit return value — which` |
|        - |  9363 | `				 * also makes the property BACKED (php: the shorthand is sugar` |
|        - |  9364 | ``				 * for `$this->NAME = expr`). */`` |
|        3 |  9365 | `				pMeth->sFunc.iFlags \|= VM_FUNC_HOOK_SET_EXPR;` |
|        3 |  9366 | `				bRefsSelf = 1;` |
|        1 |  9367 | `			}` |
|       32 |  9368 | `		}else{` |
|      ! 0 |  9369 | `			goto HookSyntax;` |
|        - |  9370 | `		}` |
|      131 |  9371 | `		rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|      131 |  9372 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  9373 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9374 | `			return SXERR_ABORT;` |
|        - |  9375 | `		}` |
|      131 |  9376 | `		pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|        1 |  9377 | `	}` |
|       95 |  9378 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|      ! 0 |  9379 | `		goto HookSyntax;` |
|        - |  9380 | `	}` |
|       95 |  9381 | `	pGen->pIn++; /* Jump '}' */` |
|       95 |  9382 | `	if( !bRefsSelf ){` |
|        - |  9383 | ``		/* php 8.4 virtual-vs-backed: no hook body referenced `$this->NAME`, so`` |
|        - |  9384 | `		 * this property is VIRTUAL — php gives it no backing store and forbids` |
|        - |  9385 | `		 * a default value (compile fatal, php's exact wording). */` |
|       41 |  9386 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_HOOK_VIRTUAL;` |
|       41 |  9387 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|      ! 0 |  9388 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  9389 | `				"Cannot specify default value for virtual hooked property %z::$%z",` |
|      ! 0 |  9390 | `				&pClass->sName,&pAttr->sName);` |
|      ! 0 |  9391 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9392 | `				return SXERR_ABORT;` |
|        - |  9393 | `			}` |
|      ! 0 |  9394 | `			return SXERR_CORRUPT;` |
|        - |  9395 | `		}` |
|       20 |  9396 | `	}` |
|       95 |  9397 | `	return SXRET_OK;` |
|      ! 0 |  9398 | `HookSyntax:` |
|      ! 0 |  9399 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  9400 | `		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",` |
|      ! 0 |  9401 | `		&pClass->sName,&pAttr->sName);` |
|      ! 0 |  9402 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  9403 | `		return SXERR_ABORT;` |
|        - |  9404 | `	}` |
|      ! 0 |  9405 | `	return SXERR_CORRUPT;` |
|       48 |  9406 | `}` |
|        - |  9407 | `/*` |
|        - |  9408 | ` * Compile an object interface.` |
|        - |  9409 | ` *  According to the PHP language reference manual` |
|        - |  9410 | ` *   Object Interfaces:` |
|        - |  9411 | ` *   Object interfaces allow you to create code which specifies which methods` |
|        - |  9412 | ` *   a class must implement, without having to define how these methods are handled.` |
|        - |  9413 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|        - |  9414 | ` *   class, but without any of the methods having their contents defined.` |
|        - |  9415 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|        - |  9416 | ` */` |
|    58244 |  9417 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|        5 |  9418 | `{` |
|    58249 |  9419 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  9420 | `	ph7_class *pClass,*pBase;` |
|        - |  9421 | `	SyToken *pEnd,*pTmp;` |
|        - |  9422 | `	SyString *pName;` |
|        - |  9423 | `	sxi32 nKwrd;` |
|        - |  9424 | `	sxi32 rc;` |
|        - |  9425 | `	/* Jump the 'interface' keyword */` |
|    58249 |  9426 | `	pGen->pIn++;` |
|        - |  9427 | `	/* Extract interface name */` |
|    58249 |  9428 | `	pName = &pGen->pIn->sData;` |
|        - |  9429 | `	/* Advance the stream cursor */` |
|    58249 |  9430 | `	pGen->pIn++;` |
|        - |  9431 | `	/* Build FQN and obtain a raw class */ {` |
|        - |  9432 | `		SyBlob sFQN;` |
|        - |  9433 | `		SyString sFQNStr;` |
|    58249 |  9434 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    58249 |  9435 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    58249 |  9436 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    58249 |  9437 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    58249 |  9438 | `		SyBlobRelease(&sFQN);` |
|        - |  9439 | `	}` |
|    58249 |  9440 | `	if( pClass == 0 ){` |
|      ! 0 |  9441 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9442 | `		return SXERR_ABORT;` |
|        - |  9443 | `	}` |
|    58249 |  9444 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    58249 |  9445 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  9446 | `		return SXERR_ABORT;` |
|        - |  9447 | `	}` |
|        - |  9448 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|    58249 |  9449 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|        - |  9450 | `	/* Assume no base class is given */` |
|    58249 |  9451 | `	pBase = 0;` |
|    58249 |  9452 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    23271 |  9453 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    23271 |  9454 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|        - |  9455 | `			SyBlob sResolved;` |
|        - |  9456 | `			SyString sBaseName;` |
|        - |  9457 | `			sxu32 nRefLine;` |
|        - |  9458 | `			/* Extract base interface */` |
|    23271 |  9459 | `			pGen->pIn++;` |
|    23271 |  9460 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    23271 |  9461 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    23271 |  9462 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 |  9463 | `				SyBlobRelease(&sResolved);` |
|      ! 0 |  9464 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  9465 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|      ! 0 |  9466 | `					pName);` |
|      ! 0 |  9467 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  9468 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9469 | `					return SXERR_ABORT;` |
|        - |  9470 | `				}` |
|      ! 0 |  9471 | `				return SXRET_OK;` |
|        - |  9472 | `			}` |
|    34904 |  9473 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    23266 |  9474 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    23271 |  9475 | `			SyStringInitFromBuf(&sBaseName,` |
|        - |  9476 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - |  9477 | `			/* Only interfaces is allowed */` |
|    23271 |  9478 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 |  9479 | `				pBase = pBase->pNextName;` |
|      ! 0 |  9480 | `			}` |
|    23271 |  9481 | `			if( pBase == 0 ){` |
|      ! 0 |  9482 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - |  9483 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|      ! 0 |  9484 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9485 | `					SyBlobRelease(&sResolved);` |
|      ! 0 |  9486 | `					return SXERR_ABORT;` |
|        - |  9487 | `				}` |
|      ! 0 |  9488 | `			}` |
|    23271 |  9489 | `			SyBlobRelease(&sResolved);` |
|    11633 |  9490 | `		}` |
|    11633 |  9491 | `	}` |
|    58249 |  9492 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - |  9493 | `		/* Syntax error */` |
|      ! 0 |  9494 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|      ! 0 |  9495 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  9496 | `		if( rc == SXERR_ABORT ){` |
|        - |  9497 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  9498 | `			return SXERR_ABORT;` |
|        - |  9499 | `		}` |
|      ! 0 |  9500 | `		return SXRET_OK;` |
|        - |  9501 | `	}` |
|    58249 |  9502 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    58249 |  9503 | `	pEnd = 0; /* cc warning */` |
|        - |  9504 | `	/* Delimit the interface body */` |
|    58249 |  9505 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    58249 |  9506 | `	if( pEnd >= pGen->pEnd ){` |
|        - |  9507 | `		/* Syntax error */` |
|      ! 0 |  9508 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|      ! 0 |  9509 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  9510 | `		if( rc == SXERR_ABORT ){` |
|        - |  9511 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  9512 | `			return SXERR_ABORT;` |
|        - |  9513 | `		}` |
|      ! 0 |  9514 | `		return SXRET_OK;` |
|        - |  9515 | `	}` |
|        - |  9516 | `	/* The delimiter token is the interface body's closing brace */` |
|    58249 |  9517 | `	pClass->nEndLine = pEnd->nLine;` |
|        - |  9518 | `	/* Swap token stream */` |
|    58249 |  9519 | `	pTmp = pGen->pEnd;` |
|    58249 |  9520 | `	pGen->pEnd = pEnd;` |
|        - |  9521 | `	/* Start the parse process` |
|        - |  9522 | `	 * Note (According to the PHP reference manual):` |
|        - |  9523 | `	 *  Only constants and function signatures(without body) are allowed.` |
|        - |  9524 | `	 *  Only 'public' visibility is allowed.` |
|        - |  9525 | `	 */` |
|   110547 |  9526 | `	for(;;){` |
|        - |  9527 | `		/* Jump leading/trailing semi-colons */` |
|   383953 |  9528 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   162855 |  9529 | `			pGen->pIn++;` |
|        5 |  9530 | `		}` |
|   221103 |  9531 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - |  9532 | `			/* End of interface body */` |
|    58245 |  9533 | `			break;` |
|        - |  9534 | `		}` |
|        - |  9535 | `		/* Bind a directly-preceding docblock to this member */` |
|   162863 |  9536 | `		GenStateSetPendingDoc(&(*pGen));` |
|   162863 |  9537 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 |  9538 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  9539 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|      ! 0 |  9540 | `				&pGen->pIn->sData,pName);` |
|      ! 0 |  9541 | `			if( rc == SXERR_ABORT ){` |
|        - |  9542 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  9543 | `				return SXERR_ABORT;` |
|        - |  9544 | `			}` |
|      ! 0 |  9545 | `			goto done;` |
|        - |  9546 | `		}` |
|        - |  9547 | `		/* Extract the current keyword */` |
|   162863 |  9548 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   162863 |  9549 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        - |  9550 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|        - |  9551 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|        3 |  9552 | `			const char *zKind = "member";` |
|        3 |  9553 | `			SyString *pMemberName = 0;` |
|        3 |  9554 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|        3 |  9555 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|        3 |  9556 | `				if( nNext == PH7_TKWRD_CONST ){` |
|        3 |  9557 | `					zKind = "constant";` |
|        3 |  9558 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|        3 |  9559 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|        2 |  9560 | `					}` |
|        1 |  9561 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|      ! 0 |  9562 | `					zKind = "method";` |
|      ! 0 |  9563 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|      ! 0 |  9564 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|      ! 0 |  9565 | `					}` |
|      ! 0 |  9566 | `				}` |
|        1 |  9567 | `			}` |
|        3 |  9568 | `			if( pMemberName ){` |
|        4 |  9569 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        1 |  9570 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|        2 |  9571 | `			}else{` |
|      ! 0 |  9572 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9573 | `					"Access type for interface %s must be public",zKind);` |
|        - |  9574 | `			}` |
|        3 |  9575 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9576 | `				return SXERR_ABORT;` |
|        - |  9577 | `			}` |
|        3 |  9578 | `			goto done;` |
|        - |  9579 | `		}` |
|   162861 |  9580 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|      ! 0 |  9581 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9582 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|      ! 0 |  9583 | `			if( rc == SXERR_ABORT ){` |
|        - |  9584 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  9585 | `				return SXERR_ABORT;` |
|        - |  9586 | `			}` |
|      ! 0 |  9587 | `			goto done;` |
|        - |  9588 | `		}` |
|   162861 |  9589 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|        - |  9590 | `			/* Advance the stream cursor */` |
|   108579 |  9591 | `			pGen->pIn++;` |
|   108574 |  9592 | `			if( pGen->pIn < pGen->pEnd` |
|   108579 |  9593 | `			 && ((pGen->pIn->nType & PH7_TK_DOLLAR) != 0` |
|   108574 |  9594 | `			  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|        - |  9595 | ``				/* PHP 8.4: `public [?T] $x { get; set; }` — a hooked-property`` |
|        - |  9596 | `				 * requirement. The attribute compiler + hook parser handle it` |
|        - |  9597 | `				 * (bare hooks are implicitly abstract inside an interface; a` |
|        - |  9598 | `				 * property without hooks is ITS "Interfaces may only include` |
|        - |  9599 | `				 * hooked properties" error). */` |
|      ! 0 |  9600 | `				rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|      ! 0 |  9601 | `					PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|      ! 0 |  9602 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  9603 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9604 | `						return SXERR_ABORT;` |
|        - |  9605 | `					}` |
|      ! 0 |  9606 | `					goto done;` |
|        - |  9607 | `				}` |
|      ! 0 |  9608 | `				continue;` |
|        - |  9609 | `			}` |
|   108579 |  9610 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|        - |  9611 | `				/* A type NAME (a plain identifier, e.g. a class type) followed by` |
|        - |  9612 | `				 * '$' also opens a hooked-property requirement. */` |
|      ! 0 |  9613 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) != 0` |
|      ! 0 |  9614 | `				 && (pGen->pIn + 1) < pGen->pEnd` |
|      ! 0 |  9615 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|      ! 0 |  9616 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|      ! 0 |  9617 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|      ! 0 |  9618 | `					if( rc != SXRET_OK ){` |
|      ! 0 |  9619 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  9620 | `							return SXERR_ABORT;` |
|        - |  9621 | `						}` |
|      ! 0 |  9622 | `						goto done;` |
|        - |  9623 | `					}` |
|      ! 0 |  9624 | `					continue;` |
|        - |  9625 | `				}` |
|      ! 0 |  9626 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9627 | `					"Expecting method signature inside interface '%z'",pName);` |
|      ! 0 |  9628 | `				if( rc == SXERR_ABORT ){` |
|        - |  9629 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 |  9630 | `					return SXERR_ABORT;` |
|        - |  9631 | `				}` |
|      ! 0 |  9632 | `				goto done;` |
|        - |  9633 | `			}` |
|   108579 |  9634 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   108579 |  9635 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|        - |  9636 | `				/* A type KEYWORD (int/string/bool/…) followed by '$' opens a` |
|        - |  9637 | `				 * hooked-property requirement (PHP 8.4). */` |
|        4 |  9638 | `				if( (pGen->pIn + 1) < pGen->pEnd` |
|        5 |  9639 | `				 && ((pGen->pIn + 1)->nType & PH7_TK_DOLLAR) != 0 ){` |
|        7 |  9640 | `					rc = GenStateCompileClassAttr(&(*pGen),PH7_CLASS_PROT_PUBLIC,` |
|        2 |  9641 | `						PH7_CLASS_ATTR_ABSTRACT,pClass);` |
|        5 |  9642 | `					if( rc != SXRET_OK ){` |
|      ! 0 |  9643 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  9644 | `							return SXERR_ABORT;` |
|        - |  9645 | `						}` |
|      ! 0 |  9646 | `						goto done;` |
|        - |  9647 | `					}` |
|        5 |  9648 | `					continue;` |
|        - |  9649 | `				}` |
|      ! 0 |  9650 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9651 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|      ! 0 |  9652 | `				if( rc == SXERR_ABORT ){` |
|        - |  9653 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 |  9654 | `					return SXERR_ABORT;` |
|        - |  9655 | `				}` |
|      ! 0 |  9656 | `				goto done;` |
|        - |  9657 | `			}` |
|    54285 |  9658 | `		}` |
|   162857 |  9659 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|        - |  9660 | `			/* Parse constant */` |
|    54283 |  9661 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|    54283 |  9662 | `			if( rc != SXRET_OK ){` |
|        3 |  9663 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9664 | `					return SXERR_ABORT;` |
|        - |  9665 | `				}` |
|        3 |  9666 | `				goto done;` |
|        - |  9667 | `			}` |
|    27143 |  9668 | `		}else{` |
|   108579 |  9669 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   108579 |  9670 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|        - |  9671 | `				/* Static method,record that */` |
|    11633 |  9672 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|        - |  9673 | `				/* Advance the stream cursor */` |
|    11633 |  9674 | `				pGen->pIn++;` |
|    11628 |  9675 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    11633 |  9676 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 |  9677 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9678 | `							"Expecting method signature inside interface '%z'",pName);` |
|      ! 0 |  9679 | `						if( rc == SXERR_ABORT ){` |
|        - |  9680 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 |  9681 | `							return SXERR_ABORT;` |
|        - |  9682 | `						}` |
|      ! 0 |  9683 | `						goto done;` |
|        - |  9684 | `				}` |
|     5814 |  9685 | `			}` |
|        - |  9686 | `			/* Process method signature (no body for interface methods) */` |
|   108579 |  9687 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   108579 |  9688 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9689 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9690 | `					return SXERR_ABORT;` |
|        - |  9691 | `				}` |
|      ! 0 |  9692 | `				goto done;` |
|        - |  9693 | `			}` |
|        - |  9694 | `		}` |
|        5 |  9695 | `	}` |
|        - |  9696 | `	/* Install the interface */` |
|    58245 |  9697 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    58245 |  9698 | `	if( rc == SXRET_OK && pBase ){` |
|        - |  9699 | `		/* Inherit from the base interface */` |
|    23271 |  9700 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|    11633 |  9701 | `	}` |
|    58245 |  9702 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  9703 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9704 | `		return SXERR_ABORT;` |
|        - |  9705 | `	}` |
|    29120 |  9706 | `done:` |
|        - |  9707 | `	/* Point beyond the interface body */` |
|    58249 |  9708 | `	pGen->pIn  = &pEnd[1];` |
|    58249 |  9709 | `	pGen->pEnd = pTmp;` |
|    58249 |  9710 | `	return PH7_OK;` |
|    29127 |  9711 | `}` |
|        - |  9712 | `/*` |
|        - |  9713 | ` * Compile a user-defined class.` |
|        - |  9714 | ` * According to the PHP language reference manual` |
|        - |  9715 | ` *  class` |
|        - |  9716 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|        - |  9717 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|        - |  9718 | ` *  of the properties and methods belonging to the class.` |
|        - |  9719 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|        - |  9720 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|        - |  9721 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|        - |  9722 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|        - |  9723 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|        - |  9724 | ` *  (called "methods").` |
|        - |  9725 | ` */` |
|        - |  9726 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|        - |  9727 | `typedef struct TraitUseEntry TraitUseEntry;` |
|        - |  9728 | `struct TraitUseEntry {` |
|        - |  9729 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|        - |  9730 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|        - |  9731 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|        - |  9732 | `};` |
|        - |  9733 | `/*` |
|        - |  9734 | ` * Validate that methods implementing interface contracts have compatible` |
|        - |  9735 | ` * signatures: public visibility and at least as many parameters as declared.` |
|        - |  9736 | ` */` |
|   300174 |  9737 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 |  9738 | `{` |
|        - |  9739 | `	ph7_class **apIface;` |
|        - |  9740 | `	sxu32 nIface,i;` |
|        - |  9741 | `	sxi32 rc;` |
|   300179 |  9742 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      ! 0 |  9743 | `		return SXRET_OK;` |
|        - |  9744 | `	}` |
|   300179 |  9745 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   300179 |  9746 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   587335 |  9747 | `	for(i = 0; i < nIface; i++){` |
|   287161 |  9748 | `		ph7_class *pIface = apIface[i];` |
|        - |  9749 | `		SyHashEntry *pEntry;` |
|   287161 |  9750 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   741233 |  9751 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   454077 |  9752 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |  9753 | `			ph7_class_method *pImplMeth;` |
|   454077 |  9754 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|        - |  9755 | `			/* Find the implementing method in the class */` |
|   454077 |  9756 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   454077 |  9757 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       23 |  9758 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|        - |  9759 | `			}` |
|        - |  9760 | `			/* Check visibility: interface methods must be implemented as public */` |
|   454059 |  9761 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        4 |  9762 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|        - |  9763 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|        1 |  9764 | `					&pClass->sName,pMName,&pIface->sName);` |
|        3 |  9765 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9766 | `					return SXERR_ABORT;` |
|        - |  9767 | `				}` |
|        1 |  9768 | `			}` |
|        - |  9769 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|        - |  9770 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|        - |  9771 | `			 */` |
|        - |  9772 | `			{` |
|   454059 |  9773 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   454059 |  9774 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   454059 |  9775 | `				int sigError = 0;` |
|   454059 |  9776 | `				if( nImplArgs < nIfaceArgs ){` |
|        3 |  9777 | `					sigError = 1;` |
|   454058 |  9778 | `				}else if( nImplArgs > nIfaceArgs ){` |
|        - |  9779 | `					/* Extra parameters must all have default values */` |
|        6 |  9780 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|        - |  9781 | `					sxu32 k;` |
|        8 |  9782 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|        6 |  9783 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|        3 |  9784 | `							sigError = 1;` |
|        3 |  9785 | `							break;` |
|        - |  9786 | `						}` |
|        2 |  9787 | `					}` |
|        2 |  9788 | `				}` |
|   454059 |  9789 | `				if( sigError ){` |
|        - |  9790 | `					SyBlob sImplSig, sIfaceSig;` |
|        - |  9791 | `					ph7_vm_func_arg *aArgs;` |
|        - |  9792 | `					sxu32 j;` |
|        6 |  9793 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|        6 |  9794 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|        - |  9795 | `					/* Build implementing method signature */` |
|        6 |  9796 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       12 |  9797 | `					for(j = 0; j < nImplArgs; j++){` |
|        8 |  9798 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|        8 |  9799 | `						SyBlobAppend(&sImplSig,"$",1);` |
|        8 |  9800 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|        5 |  9801 | `					}` |
|        - |  9802 | `					/* Build interface method signature */` |
|        6 |  9803 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|       12 |  9804 | `					for(j = 0; j < nIfaceArgs; j++){` |
|        8 |  9805 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|        8 |  9806 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|        8 |  9807 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|        5 |  9808 | `					}` |
|        8 |  9809 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|        - |  9810 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|        2 |  9811 | `						&pClass->sName,pMName,` |
|        4 |  9812 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|        2 |  9813 | `						&pIface->sName,pMName,` |
|        4 |  9814 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|        6 |  9815 | `					SyBlobRelease(&sImplSig);` |
|        6 |  9816 | `					SyBlobRelease(&sIfaceSig);` |
|        6 |  9817 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9818 | `						return SXERR_ABORT;` |
|        - |  9819 | `					}` |
|        2 |  9820 | `				}` |
|        - |  9821 | `			}` |
|        5 |  9822 | `		}` |
|   143583 |  9823 | `	}` |
|   300179 |  9824 | `	return SXRET_OK;` |
|   150092 |  9825 | `}` |
|        - |  9826 | `/*` |
|        - |  9827 | ` * An abstract property-hook stub (__phl_hook_{get,set}_NAME) is satisfied by` |
|        - |  9828 | ` * the class declaring a PLAIN (non-abstract, non-hooked) property NAME: php` |
|        - |  9829 | `` * lets a plain property implement `{ get; set; }` requirements — its raw`` |
|        - |  9830 | ` * read/write IS the default get/set. A concrete hook override replaced the` |
|        - |  9831 | ` * stub in hMethod already, so a surviving stub next to a HOOKED property` |
|        - |  9832 | ` * means that specific hook is still missing.` |
|        - |  9833 | ` */` |
|       38 |  9834 | `static int GenStateAbstractHookSatisfied(ph7_class *pClass,const SyString *pMName)` |
|        5 |  9835 | `{` |
|        - |  9836 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|        - |  9837 | `	ph7_class_attr *pProp;` |
|       38 |  9838 | `	if( pMName->nByte <= nPfx` |
|       27 |  9839 | `	 \|\| (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) != 0` |
|        4 |  9840 | `	  && SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) != 0) ){` |
|       36 |  9841 | `		return 0; /* not a hook stub */` |
|        - |  9842 | `	}` |
|        7 |  9843 | `	pProp = PH7_ClassExtractAttribute(pClass,&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|        7 |  9844 | `	return pProp != 0` |
|        6 |  9845 | `		&& (pProp->iFlags & (PH7_CLASS_ATTR_ABSTRACT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|        3 |  9846 | `			\|PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) == 0;` |
|       24 |  9847 | `}` |
|        - |  9848 | `/*` |
|        - |  9849 | ` * Append an abstract member's display name to the message blob, translating a` |
|        - |  9850 | `` * property-hook stub (__phl_hook_get_x) to php's `$x::get` form.`` |
|        - |  9851 | ` */` |
|       16 |  9852 | `static void GenStateAppendAbstractMemberName(SyBlob *pMsg,const SyString *pMName)` |
|        4 |  9853 | `{` |
|        - |  9854 | `	static const sxu32 nPfx = sizeof("__phl_hook_get_")-1;` |
|       16 |  9855 | `	if( pMName->nByte > nPfx` |
|       12 |  9856 | `	 && (SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_get_",nPfx) == 0` |
|      ! 0 |  9857 | `	  \|\| SyMemcmp((const void *)pMName->zString,(const void *)"__phl_hook_set_",nPfx) == 0) ){` |
|      ! 0 |  9858 | `		SyBlobAppend(pMsg,"$",1);` |
|      ! 0 |  9859 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[nPfx],pMName->nByte - nPfx);` |
|      ! 0 |  9860 | `		SyBlobAppend(pMsg,"::",2);` |
|      ! 0 |  9861 | `		SyBlobAppend(pMsg,(const void *)&pMName->zString[sizeof("__phl_hook_")-1],3);` |
|      ! 0 |  9862 | `		return;` |
|        - |  9863 | `	}` |
|       20 |  9864 | `	SyBlobAppend(pMsg,(const void *)pMName->zString,pMName->nByte);` |
|       12 |  9865 | `}` |
|        - |  9866 | `/*` |
|        - |  9867 | ` * Check that a concrete class has no remaining abstract methods.` |
|        - |  9868 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|        - |  9869 | ` */` |
|   300174 |  9870 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 |  9871 | `{` |
|        - |  9872 | `	ph7_class_method *pMeth;` |
|        - |  9873 | `	SyHashEntry *pEntry;` |
|        - |  9874 | `	sxu32 nAbstract;` |
|        - |  9875 | `	SyBlob sMsg;` |
|        - |  9876 | `	sxi32 rc;` |
|        - |  9877 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   300179 |  9878 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|    11677 |  9879 | `		return SXRET_OK;` |
|        - |  9880 | `	}` |
|        - |  9881 | `	/* Count abstract methods */` |
|   288507 |  9882 | `	nAbstract = 0;` |
|   288507 |  9883 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  4163900 |  9884 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  3731147 |  9885 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  3731147 |  9886 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       27 |  9887 | `			if( GenStateAbstractHookSatisfied(pClass,&pMeth->sFunc.sName) ){` |
|        7 |  9888 | `				continue; /* hook requirement met by a plain property (php) */` |
|        - |  9889 | `			}` |
|       20 |  9890 | `			nAbstract++;` |
|        8 |  9891 | `		}` |
|        5 |  9892 | `	}` |
|   288507 |  9893 | `	if( nAbstract == 0 ){` |
|   288493 |  9894 | `		return SXRET_OK;` |
|        - |  9895 | `	}` |
|        - |  9896 | `	/* Build the error message listing all abstract methods with origins */` |
|       18 |  9897 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       18 |  9898 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|        - |  9899 | `		"be declared abstract or implement the remaining method%s (",` |
|        7 |  9900 | `		&pClass->sName,nAbstract,` |
|        7 |  9901 | `		(nAbstract > 1 ? "s" : ""),` |
|        7 |  9902 | `		(nAbstract > 1 ? "s" : ""));` |
|        - |  9903 | `	/* Second pass: list methods with origins */` |
|        - |  9904 | `	{` |
|       18 |  9905 | `		sxu32 nListed = 0;` |
|       18 |  9906 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|       36 |  9907 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|       22 |  9908 | `			ph7_class *pOrigin = 0;` |
|        - |  9909 | `			SyString *pMName;` |
|       22 |  9910 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|       22 |  9911 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|        3 |  9912 | `				continue;` |
|        - |  9913 | `			}` |
|       20 |  9914 | `			pMName = &pMeth->sFunc.sName;` |
|       20 |  9915 | `			if( GenStateAbstractHookSatisfied(pClass,pMName) ){` |
|      ! 0 |  9916 | `				continue; /* hook requirement met by a plain property (php) */` |
|        - |  9917 | `			}` |
|       20 |  9918 | `			if( nListed > 0 ){` |
|        3 |  9919 | `				SyBlobAppend(&sMsg,", ",2);` |
|        1 |  9920 | `			}` |
|        - |  9921 | `			/* Find the origin of this abstract method.` |
|        - |  9922 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|        - |  9923 | `			 * inheritance chains) take precedence for interface-declared` |
|        - |  9924 | `			 * methods. Abstract class methods only win when the class` |
|        - |  9925 | `			 * itself declared the abstract method (not inherited from` |
|        - |  9926 | `			 * an interface). Trait methods are adopted into the using` |
|        - |  9927 | `			 * class's namespace.` |
|        - |  9928 | `			 */` |
|        - |  9929 | `			{` |
|        - |  9930 | `				ph7_class **apIface;` |
|        - |  9931 | `				ph7_class **apTrait;` |
|        - |  9932 | `				ph7_class *pWalk;` |
|        - |  9933 | `				sxu32 i;` |
|        - |  9934 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|        - |  9935 | `				 * (one that was written in the class body, not inherited from an` |
|        - |  9936 | `				 * interface). PHP attributes origin to the declaring class.` |
|        - |  9937 | `				 */` |
|       20 |  9938 | `				if( pClass->pBase ){` |
|       11 |  9939 | `					pWalk = pClass->pBase;` |
|       19 |  9940 | `					while( pWalk ){` |
|        - |  9941 | `						ph7_class_method *pParentMeth;` |
|       13 |  9942 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|       13 |  9943 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        - |  9944 | `							/* Exclude methods that came from an interface anywhere` |
|        - |  9945 | `							 * in this class's ancestor chain.` |
|        - |  9946 | `							 */` |
|       13 |  9947 | `							int fromIface = 0;` |
|       13 |  9948 | `							ph7_class *pAnc = pWalk;` |
|       17 |  9949 | `							while( pAnc ){` |
|        - |  9950 | `								ph7_class **apPI;` |
|        - |  9951 | `								sxu32 j;` |
|       15 |  9952 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|       15 |  9953 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       10 |  9954 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       10 |  9955 | `										fromIface = 1;` |
|       10 |  9956 | `										break;` |
|        - |  9957 | `									}` |
|      ! 0 |  9958 | `								}` |
|       15 |  9959 | `								if( fromIface ) break;` |
|        6 |  9960 | `								pAnc = pAnc->pBase;` |
|        2 |  9961 | `							}` |
|       13 |  9962 | `							if( !fromIface ){` |
|        3 |  9963 | `								pOrigin = pWalk;` |
|        3 |  9964 | `								break;` |
|        - |  9965 | `							}` |
|        4 |  9966 | `						}` |
|       10 |  9967 | `						pWalk = pWalk->pBase;` |
|        2 |  9968 | `					}` |
|        4 |  9969 | `				}` |
|        - |  9970 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|        - |  9971 | `				 * each interface's own parent chain for the deepest origin.` |
|        - |  9972 | `				 */` |
|       20 |  9973 | `				if( !pOrigin ){` |
|       18 |  9974 | `					pWalk = pClass;` |
|       40 |  9975 | `					while( pWalk && !pOrigin ){` |
|       26 |  9976 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|       26 |  9977 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|       16 |  9978 | `							ph7_class *pIface = apIface[i];` |
|       16 |  9979 | `							ph7_class *pDeepest = 0;` |
|       28 |  9980 | `							while( pIface ){` |
|       16 |  9981 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|       16 |  9982 | `									pDeepest = pIface;` |
|        6 |  9983 | `								}` |
|       16 |  9984 | `								pIface = pIface->pBase;` |
|        4 |  9985 | `							}` |
|       16 |  9986 | `							if( pDeepest ){` |
|       16 |  9987 | `								pOrigin = pDeepest;` |
|       16 |  9988 | `								break;` |
|        - |  9989 | `							}` |
|      ! 0 |  9990 | `						}` |
|       26 |  9991 | `						pWalk = pWalk->pBase;` |
|        4 |  9992 | `					}` |
|        7 |  9993 | `				}` |
|        - |  9994 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|       20 |  9995 | `				if( !pOrigin ){` |
|        3 |  9996 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|        3 |  9997 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|        3 |  9998 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|        3 |  9999 | `							pOrigin = pClass;` |
|        3 | 10000 | `							break;` |
|        - | 10001 | `						}` |
|      ! 0 | 10002 | `					}` |
|        1 | 10003 | `				}` |
|        - | 10004 | `			}` |
|       20 | 10005 | `			if( pOrigin ){` |
|       20 | 10006 | `				SyBlobFormat(&sMsg,"%z::",&pOrigin->sName);` |
|       12 | 10007 | `			}else{` |
|        - | 10008 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|      ! 0 | 10009 | `				SyBlobFormat(&sMsg,"%z::",&pClass->sName);` |
|        - | 10010 | `			}` |
|       20 | 10011 | `			GenStateAppendAbstractMemberName(&sMsg,pMName);` |
|       20 | 10012 | `			nListed++;` |
|        4 | 10013 | `		}` |
|        - | 10014 | `	}` |
|       18 | 10015 | `	SyBlobAppend(&sMsg,")",1);` |
|       25 | 10016 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|       14 | 10017 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|       18 | 10018 | `	SyBlobRelease(&sMsg);` |
|       18 | 10019 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 10020 | `		return SXERR_ABORT;` |
|        - | 10021 | `	}` |
|       18 | 10022 | `	return SXRET_OK;` |
|   150092 | 10023 | `}` |
|        - | 10024 | `/*` |
|        - | 10025 | ` * Parse a class/interface name reference from the current token stream.` |
|        - | 10026 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|        - | 10027 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|        - | 10028 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|        - | 10029 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|        - | 10030 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|        - | 10031 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|        - | 10032 | ` */` |
|   304286 | 10033 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|        5 | 10034 | `{` |
|   304291 | 10035 | `	int isAbsolute = 0;` |
|   304291 | 10036 | `	SyToken *pStart = pGen->pIn;` |
|        - | 10037 | `	SyBlob sName;` |
|   304291 | 10038 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|     4465 | 10039 | `		isAbsolute = 1;` |
|     4465 | 10040 | `		pGen->pIn++;` |
|     2230 | 10041 | `	}` |
|   304291 | 10042 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        8 | 10043 | `		pGen->pIn = pStart;` |
|        8 | 10044 | `		return SXERR_INVALID;` |
|        - | 10045 | `	}` |
|   304285 | 10046 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   304285 | 10047 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   304285 | 10048 | `	pGen->pIn++;` |
|   456441 | 10049 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   152166 | 10050 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       16 | 10051 | `		SyBlobAppend(&sName,"\\",1);` |
|       16 | 10052 | `		pGen->pIn++;` |
|       16 | 10053 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       16 | 10054 | `		pGen->pIn++;` |
|        2 | 10055 | `	}` |
|   304285 | 10056 | `	if( isAbsolute ){` |
|     4463 | 10057 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|     2234 | 10058 | `	}else{` |
|        - | 10059 | `		SyString sRaw;` |
|   299827 | 10060 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   299827 | 10061 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|        - | 10062 | `	}` |
|   304285 | 10063 | `	SyBlobRelease(&sName);` |
|   304285 | 10064 | `	return SXRET_OK;` |
|   152148 | 10065 | `}` |
|        - | 10066 | `/*` |
|        - | 10067 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|        - | 10068 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|        - | 10069 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|        - | 10070 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|        - | 10071 | ` * either direction cannot run unbounded.` |
|        - | 10072 | ` */` |
|        - | 10073 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    93224 | 10074 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|        5 | 10075 | `{` |
|        - | 10076 | `	ph7_class **apParent;` |
|        - | 10077 | `	sxu32 n;` |
|   248507 | 10078 | `	while( pInterface ){` |
|   163045 | 10079 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|      ! 0 | 10080 | `			return FALSE;` |
|        - | 10081 | `		}` |
|   186320 | 10082 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    46550 | 10083 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|     7767 | 10084 | `			return TRUE;` |
|        - | 10085 | `		}` |
|   155283 | 10086 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|   155283 | 10087 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|      ! 0 | 10088 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|      ! 0 | 10089 | `				return TRUE;` |
|        - | 10090 | `			}` |
|      ! 0 | 10091 | `		}` |
|   155283 | 10092 | `		pInterface = pInterface->pBase;` |
|   155283 | 10093 | `		iDepth++;` |
|        5 | 10094 | `	}` |
|    85467 | 10095 | `	return FALSE;` |
|    46617 | 10096 | `}` |
|    93224 | 10097 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|        5 | 10098 | `{` |
|    93229 | 10099 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|        5 | 10100 | `}` |
|        - | 10101 | `/*` |
|        - | 10102 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|        - | 10103 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|        - | 10104 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|        - | 10105 | ` */` |
|     7762 | 10106 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|        5 | 10107 | `{` |
|     7771 | 10108 | `	while( pBase ){` |
|       10 | 10109 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|        2 | 10110 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|        3 | 10111 | `			return TRUE;` |
|        - | 10112 | `		}` |
|       10 | 10113 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|        6 | 10114 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|        3 | 10115 | `			return TRUE;` |
|        - | 10116 | `		}` |
|        5 | 10117 | `		pBase = pBase->pBase;` |
|        1 | 10118 | `	}` |
|     7763 | 10119 | `	return FALSE;` |
|     3886 | 10120 | `}` |
|        - | 10121 | `/*` |
|        - | 10122 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|        - | 10123 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|        - | 10124 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|        - | 10125 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|        - | 10126 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|        - | 10127 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|        - | 10128 | ` * pClass->aEnumCases for cases().` |
|        - | 10129 | ` */` |
|     7794 | 10130 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 10131 | `{` |
|     7799 | 10132 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 10133 | `	SySet *pInstrContainer;` |
|        - | 10134 | `	ph7_class_attr *pCase;` |
|        - | 10135 | `	SyString *pName;` |
|        - | 10136 | `	sxi32 rc;` |
|     7799 | 10137 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|     7799 | 10138 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 10139 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 10140 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|      ! 0 | 10141 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10142 | `			return SXERR_ABORT;` |
|        - | 10143 | `		}` |
|      ! 0 | 10144 | `		goto Synchronize;` |
|        - | 10145 | `	}` |
|     7799 | 10146 | `	pName = &pGen->pIn->sData;` |
|        - | 10147 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|     7799 | 10148 | `	if( SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte) != 0 ){` |
|      ! 0 | 10149 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 10150 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|      ! 0 | 10151 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10152 | `			return SXERR_ABORT;` |
|        - | 10153 | `		}` |
|      ! 0 | 10154 | `		goto Synchronize;` |
|        - | 10155 | `	}` |
|     7799 | 10156 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - | 10157 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|     7799 | 10158 | `	if( pCase == 0 ){` |
|      ! 0 | 10159 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 10160 | `		return SXERR_ABORT;` |
|        - | 10161 | `	}` |
|     7799 | 10162 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|     7799 | 10163 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 10164 | `		return SXERR_ABORT;` |
|        - | 10165 | `	}` |
|     7799 | 10166 | `	pGen->pIn++; /* Jump the case name */` |
|     7799 | 10167 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|     7785 | 10168 | `		if( pClass->nEnumBacking == 0 ){` |
|        8 | 10169 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        2 | 10170 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|        6 | 10171 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 10172 | `				return SXERR_ABORT;` |
|        - | 10173 | `			}` |
|        6 | 10174 | `			goto Synchronize;` |
|        - | 10175 | `		}` |
|     7781 | 10176 | `		pGen->pIn++; /* Jump the equal sign */` |
|        - | 10177 | `		/* Compile the backing value expression into the case's own container` |
|        - | 10178 | `		 * (same technique as class constants). */` |
|     7781 | 10179 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     7781 | 10180 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|     7781 | 10181 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|     7781 | 10182 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 | 10183 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 10184 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|      ! 0 | 10185 | `		}` |
|     7781 | 10186 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|     7781 | 10187 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     7781 | 10188 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10189 | `			return SXERR_ABORT;` |
|        - | 10190 | `		}` |
|     3893 | 10191 | `	}else{` |
|       17 | 10192 | `		if( pClass->nEnumBacking != 0 ){` |
|      ! 0 | 10193 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 10194 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|      ! 0 | 10195 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 10196 | `				return SXERR_ABORT;` |
|        - | 10197 | `			}` |
|      ! 0 | 10198 | `			goto Synchronize;` |
|        - | 10199 | `		}` |
|        - | 10200 | `	}` |
|     7795 | 10201 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|     7795 | 10202 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10203 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 10204 | `		return SXERR_ABORT;` |
|        - | 10205 | `	}` |
|     7795 | 10206 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|     7795 | 10207 | `	return SXRET_OK;` |
|        2 | 10208 | `Synchronize:` |
|        - | 10209 | `	/* Synchronize with the first semi-colon */` |
|       14 | 10210 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|       10 | 10211 | `		pGen->pIn++;` |
|        2 | 10212 | `	}` |
|        6 | 10213 | `	return SXERR_CORRUPT;` |
|     3902 | 10214 | `}` |
|        - | 10215 | `/*` |
|        - | 10216 | ` * Synthesize the enum interface methods (PHP 8.1): cases() for every enum,` |
|        - | 10217 | ` * plus from()/tryFrom() for backed enums. Each is an ordinary public static` |
|        - | 10218 | ` * method whose body forwards to a __phl_enum_* engine thunk (vm.c) with the` |
|        - | 10219 | ` * enum's FQN embedded as a literal — the same forwarder pattern the` |
|        - | 10220 | ` * Generator/Fiber/Reflection builtins use. The source buffer is owned by the` |
|        - | 10221 | ` * VM allocator and never freed: tokens (method and parameter names) keep` |
|        - | 10222 | ` * pointers into it (see the constructor-promotion precedent above).` |
|        - | 10223 | ` */` |
|     3900 | 10224 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 | 10225 | `{` |
|        - | 10226 | `	SyToken *pSaveIn,*pSaveEnd;` |
|        - | 10227 | `	const char *zBack;` |
|        - | 10228 | `	SySet sToken;` |
|        - | 10229 | `	char *zSrc;` |
|        - | 10230 | `	sxu32 nSrc,nMax;` |
|     3905 | 10231 | `	sxi32 rc = SXRET_OK;` |
|     3905 | 10232 | `	nMax = 3*(sxu32)sizeof("function tryFrom(string $value){return __phl_enum_tryfrom('',$value);}")` |
|     3900 | 10233 | `		+ 3*SyStringLength(&pClass->sName) + 64;` |
|     3905 | 10234 | `	zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nMax);` |
|     3905 | 10235 | `	if( zSrc == 0 ){` |
|      ! 0 | 10236 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 10237 | `		return SXERR_ABORT;` |
|        - | 10238 | `	}` |
|     3905 | 10239 | `	zBack = (pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string";` |
|     3905 | 10240 | `	if( pClass->nEnumBacking != 0 ){` |
|     5837 | 10241 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|        - | 10242 | `			"function cases(){return __phl_enum_cases('%z');}"` |
|        - | 10243 | `			"function from(%s $value){return __phl_enum_from('%z',$value);}"` |
|        - | 10244 | `			"function tryFrom(%s $value){return __phl_enum_tryfrom('%z',$value);}",` |
|     1944 | 10245 | `			&pClass->sName,zBack,&pClass->sName,zBack,&pClass->sName);` |
|     1949 | 10246 | `	}else{` |
|       21 | 10247 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|        6 | 10248 | `			"function cases(){return __phl_enum_cases('%z');}",&pClass->sName);` |
|        - | 10249 | `	}` |
|     3905 | 10250 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|     3905 | 10251 | `	PH7_TokenizePHP(zSrc,nSrc,pClass->nLine,&sToken,0);` |
|     3905 | 10252 | `	pSaveIn = pGen->pIn;` |
|     3905 | 10253 | `	pSaveEnd = pGen->pEnd;` |
|     3905 | 10254 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|     3905 | 10255 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|    15581 | 10256 | `	while( pGen->pIn < pGen->pEnd && rc != SXERR_ABORT ){` |
|    11681 | 10257 | `		rc = GenStateCompileClassMethod(&(*pGen),PH7_TKWRD_PUBLIC,PH7_CLASS_ATTR_STATIC,TRUE,pClass);` |
|        5 | 10258 | `	}` |
|     3905 | 10259 | `	pGen->pIn = pSaveIn;` |
|     3905 | 10260 | `	pGen->pEnd = pSaveEnd;` |
|     3905 | 10261 | `	SySetRelease(&sToken);` |
|     3905 | 10262 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXRET_OK;` |
|     1955 | 10263 | `}` |
|        - | 10264 | `/*` |
|        - | 10265 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|        - | 10266 | ` * __call/__callStatic/__invoke stay allowed).` |
|        - | 10267 | ` */` |
|        - | 10268 | `static const char *azEnumBannedMagic[] = {` |
|        - | 10269 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|        - | 10270 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|        - | 10271 | `};` |
|        - | 10272 | `/*` |
|        - | 10273 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|        - | 10274 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|        - | 10275 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|        - | 10276 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|        - | 10277 | ` * and before the class is installed.` |
|        - | 10278 | ` */` |
|     3900 | 10279 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|        5 | 10280 | `{` |
|        - | 10281 | `	SyHashEntry *pEntry;` |
|        - | 10282 | `	sxi32 rc;` |
|        - | 10283 | `	sxu32 n;` |
|        - | 10284 | `	/* php: "Enum %s cannot include properties" */` |
|     3905 | 10285 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    11699 | 10286 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     7801 | 10287 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     7801 | 10288 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        3 | 10289 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|        1 | 10290 | `				"Enum %z cannot include properties",&pClass->sName);` |
|        3 | 10291 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 10292 | `				return SXERR_ABORT;` |
|        - | 10293 | `			}` |
|        3 | 10294 | `			break;` |
|        - | 10295 | `		}` |
|        5 | 10296 | `	}` |
|        - | 10297 | `	/* php: "Enum %s cannot include magic method %s" */` |
|    54605 | 10298 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|    76050 | 10299 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|    50705 | 10300 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|      ! 0 | 10301 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 10302 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|      ! 0 | 10303 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 10304 | `				return SXERR_ABORT;` |
|        - | 10305 | `			}` |
|      ! 0 | 10306 | `		}` |
|    25355 | 10307 | `	}` |
|        - | 10308 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|        - | 10309 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|        - | 10310 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|        - | 10311 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|        - | 10312 | `	{` |
|        - | 10313 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|        - | 10314 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|        - | 10315 | `		ph7_class_attr *pAttr;` |
|     3905 | 10316 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - | 10317 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|     3905 | 10318 | `		if( pAttr == 0 ){` |
|      ! 0 | 10319 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 10320 | `			return SXERR_ABORT;` |
|        - | 10321 | `		}` |
|     3905 | 10322 | `		pAttr->nType = MEMOBJ_STRING;` |
|     3905 | 10323 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|     3905 | 10324 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|     3905 | 10325 | `		if( pClass->nEnumBacking != 0 ){` |
|     3893 | 10326 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - | 10327 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|     3893 | 10328 | `			if( pAttr == 0 ){` |
|      ! 0 | 10329 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 10330 | `				return SXERR_ABORT;` |
|        - | 10331 | `			}` |
|     3893 | 10332 | `			pAttr->nType = pClass->nEnumBacking;` |
|     3893 | 10333 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|        7 | 10334 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|        4 | 10335 | `			}else{` |
|     3887 | 10336 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|        - | 10337 | `			}` |
|     3893 | 10338 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|     1944 | 10339 | `		}` |
|        - | 10340 | `	}` |
|     3905 | 10341 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|     1955 | 10342 | `}` |
|        - | 10343 | `/*` |
|        - | 10344 | ` * Compile a class declaration, named or anonymous.` |
|        - | 10345 | ` *` |
|        - | 10346 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|        - | 10347 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|        - | 10348 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|        - | 10349 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|        - | 10350 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|        - | 10351 | ` * implements, body, install) is shared by both paths.` |
|        - | 10352 | ` */` |
|   300218 | 10353 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|        - | 10354 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|        5 | 10355 | `{` |
|   300223 | 10356 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 10357 | `	ph7_class *pClass,*pBase;` |
|        - | 10358 | `	SyToken *pEnd,*pTmp;` |
|        - | 10359 | `	sxi32 iProtection;` |
|        - | 10360 | `	SySet aInterfaces;` |
|        - | 10361 | `	SySet aUseEntries;` |
|        - | 10362 | `	sxi32 iAttrflags;` |
|        - | 10363 | `	SyString *pName;` |
|        - | 10364 | `	sxi32 nKwrd;` |
|        - | 10365 | `	sxi32 rc;` |
|        - | 10366 | `	/* Jump the 'class' keyword */` |
|   300223 | 10367 | `	pGen->pIn++;` |
|   300223 | 10368 | `	if( pAnonName ){` |
|        - | 10369 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|        - | 10370 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|        - | 10371 | `		 * then use the synthesized name. */` |
|       32 | 10372 | `		*ppArgStart = *ppArgEnd = 0;` |
|       32 | 10373 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        7 | 10374 | `			pGen->pIn++; /* Jump '(' */` |
|        7 | 10375 | `			*ppArgStart = pGen->pIn;` |
|       10 | 10376 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|        3 | 10377 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|        7 | 10378 | `			pGen->pIn = *ppArgEnd;` |
|        7 | 10379 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|        3 | 10380 | `		}` |
|       32 | 10381 | `		pName = pAnonName;` |
|       32 | 10382 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|       18 | 10383 | `	}else{` |
|   300195 | 10384 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|        - | 10385 | `			/* Syntax error */` |
|      ! 0 | 10386 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|      ! 0 | 10387 | `			if( rc == SXERR_ABORT ){` |
|        - | 10388 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 10389 | `				return SXERR_ABORT;` |
|        - | 10390 | `			}` |
|        - | 10391 | `			/* Synchronize with the first semi-colon or curly braces */` |
|      ! 0 | 10392 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|      ! 0 | 10393 | `				pGen->pIn++;` |
|      ! 0 | 10394 | `			}` |
|      ! 0 | 10395 | `			return SXRET_OK;` |
|        - | 10396 | `		}` |
|        - | 10397 | `		/* Extract class name */` |
|   300195 | 10398 | `		pName = &pGen->pIn->sData;` |
|        - | 10399 | `		/* Advance the stream cursor */` |
|   300195 | 10400 | `		pGen->pIn++;` |
|        - | 10401 | `		/* Build FQN and obtain a raw class */ {` |
|        - | 10402 | `			SyBlob sFQN;` |
|        - | 10403 | `			SyString sFQNStr;` |
|   300195 | 10404 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   300195 | 10405 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|   300195 | 10406 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   300195 | 10407 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   300195 | 10408 | `			SyBlobRelease(&sFQN);` |
|        - | 10409 | `		}` |
|        - | 10410 | `	}` |
|   300223 | 10411 | `	if( pClass == 0 ){` |
|      ! 0 | 10412 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 10413 | `		return SXERR_ABORT;` |
|        - | 10414 | `	}` |
|   300218 | 10415 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|     3909 | 10416 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|        - | 10417 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|     3895 | 10418 | `		pGen->pIn++; /* Jump ':' */` |
|     3890 | 10419 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     3895 | 10420 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|        7 | 10421 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|        7 | 10422 | `			pGen->pIn++;` |
|     3888 | 10423 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     3889 | 10424 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|     3887 | 10425 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|     3887 | 10426 | `			pGen->pIn++;` |
|     1946 | 10427 | `		}else{` |
|        3 | 10428 | `			SyToken *pTok = pGen->pIn;` |
|        3 | 10429 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|        4 | 10430 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|        1 | 10431 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|        3 | 10432 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 10433 | `				return SXERR_ABORT;` |
|        - | 10434 | `			}` |
|        3 | 10435 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|        3 | 10436 | `				pGen->pIn++; /* Skip the bogus type token */` |
|        1 | 10437 | `			}` |
|        - | 10438 | `		}` |
|     1945 | 10439 | `	}` |
|   300223 | 10440 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|   300223 | 10441 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 10442 | `		return SXERR_ABORT;` |
|        - | 10443 | `	}` |
|        - | 10444 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   300223 | 10445 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   300223 | 10446 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|        - | 10447 | `	/* Assume a standalone class */` |
|   300223 | 10448 | `	pBase = 0;` |
|   300223 | 10449 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   244621 | 10450 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   244621 | 10451 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|        - | 10452 | `			SyBlob sResolved;` |
|        - | 10453 | `			SyString sBaseName;` |
|        - | 10454 | `			sxu32 nRefLine;` |
|   166925 | 10455 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|        - | 10456 | `				/* php parse-fatals here (enums have no inheritance) */` |
|      ! 0 | 10457 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 10458 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|      ! 0 | 10459 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10460 | `					return SXERR_ABORT;` |
|        - | 10461 | `				}` |
|      ! 0 | 10462 | `			}` |
|   166925 | 10463 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   166925 | 10464 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   166925 | 10465 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   166925 | 10466 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        3 | 10467 | `				SyBlobRelease(&sResolved);` |
|        4 | 10468 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 10469 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|        1 | 10470 | `					pName);` |
|        3 | 10471 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|        3 | 10472 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10473 | `					return SXERR_ABORT;` |
|        - | 10474 | `				}` |
|        3 | 10475 | `				return SXRET_OK;` |
|        - | 10476 | `			}` |
|   250382 | 10477 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   166918 | 10478 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   166923 | 10479 | `			SyStringInitFromBuf(&sBaseName,` |
|        - | 10480 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 10481 | `			/* Interfaces are not allowed */` |
|   166923 | 10482 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|      ! 0 | 10483 | `				pBase = pBase->pNextName;` |
|      ! 0 | 10484 | `			}` |
|   166923 | 10485 | `			if( pBase == 0 ){` |
|      ! 0 | 10486 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - | 10487 | `					"Nonexistent base class '%z'",&sBaseName);` |
|      ! 0 | 10488 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10489 | `					SyBlobRelease(&sResolved);` |
|      ! 0 | 10490 | `					return SXERR_ABORT;` |
|        - | 10491 | `				}` |
|      ! 0 | 10492 | `			}else{` |
|   166923 | 10493 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|        4 | 10494 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 10495 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|        3 | 10496 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10497 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 10498 | `						return SXERR_ABORT;` |
|        - | 10499 | `					}` |
|        3 | 10500 | `					pBase = 0; /* Never inherit from an enum */` |
|   166922 | 10501 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|      ! 0 | 10502 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 10503 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|      ! 0 | 10504 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10505 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 10506 | `						return SXERR_ABORT;` |
|        - | 10507 | `					}` |
|      ! 0 | 10508 | `				}` |
|        - | 10509 | `			}` |
|   166923 | 10510 | `			SyBlobRelease(&sResolved);` |
|   166923 | 10511 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|      ! 0 | 10512 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|      ! 0 | 10513 | `			}` |
|    83459 | 10514 | `		}` |
|   244619 | 10515 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|        - | 10516 | `			ph7_class *pInterface;` |
|        - | 10517 | `			/* Interface implementation */` |
|    77713 | 10518 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    54370 | 10519 | `			for(;;){` |
|        - | 10520 | `				SyBlob sResolved;` |
|        - | 10521 | `				SyString sIntName;` |
|        - | 10522 | `				sxu32 nRefLine;` |
|    93229 | 10523 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    93229 | 10524 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    93229 | 10525 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 10526 | `					SyBlobRelease(&sResolved);` |
|      ! 0 | 10527 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 10528 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|      ! 0 | 10529 | `						pName);` |
|      ! 0 | 10530 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10531 | `						return SXERR_ABORT;` |
|        - | 10532 | `					}` |
|      ! 0 | 10533 | `					break;` |
|        - | 10534 | `				}` |
|   186453 | 10535 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    93224 | 10536 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    93229 | 10537 | `				SyStringInitFromBuf(&sIntName,` |
|        - | 10538 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 10539 | `				/* Only interfaces are allowed */` |
|    93229 | 10540 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 10541 | `					pInterface = pInterface->pNextName;` |
|      ! 0 | 10542 | `				}` |
|    93229 | 10543 | `				if( pInterface == 0 ){` |
|      ! 0 | 10544 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - | 10545 | `						"Nonexistent base interface '%z'",&sIntName);` |
|      ! 0 | 10546 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10547 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 10548 | `						return SXERR_ABORT;` |
|        - | 10549 | `					}` |
|      ! 0 | 10550 | `				}else{` |
|        - | 10551 | `					/* Reject user classes that try to implement Throwable` |
|        - | 10552 | `					 * directly (or via an interface that extends Throwable)` |
|        - | 10553 | `					 * unless they already extend Exception or Error.` |
|        - | 10554 | `					 * Exception and Error themselves are compiled from the` |
|        - | 10555 | `					 * built-in library and are exempt by FQN — a namespaced` |
|        - | 10556 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    93229 | 10557 | `					SyString *pFqn = &pClass->sName;` |
|    93229 | 10558 | `					int bIsExceptionOrError =` |
|    50492 | 10559 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   141778 | 10560 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    91293 | 10561 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|     3890 | 10562 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|    97105 | 10563 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|    11646 | 10564 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|     3879 | 10565 | `						!bIsExceptionOrError ){` |
|       12 | 10566 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10567 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|        3 | 10568 | `							&pClass->sName);` |
|        9 | 10569 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10570 | `							SyBlobRelease(&sResolved);` |
|      ! 0 | 10571 | `							return SXERR_ABORT;` |
|        - | 10572 | `						}` |
|        - | 10573 | `						/* Skip registration so the follow-up abstract-method` |
|        - | 10574 | `						 * check does not produce a duplicate fatal. */` |
|        6 | 10575 | `					}else{` |
|    93223 | 10576 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|        - | 10577 | `					}` |
|        - | 10578 | `				}` |
|    93229 | 10579 | `				SyBlobRelease(&sResolved);` |
|    93229 | 10580 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    38859 | 10581 | `					break;` |
|        - | 10582 | `				}` |
|    15521 | 10583 | `				pGen->pIn++;/* Jump the comma */` |
|        5 | 10584 | `			}` |
|    38854 | 10585 | `		}` |
|   122307 | 10586 | `	}` |
|   300221 | 10587 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - | 10588 | `		/* Syntax error */` |
|      ! 0 | 10589 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|      ! 0 | 10590 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 10591 | `		if( rc == SXERR_ABORT ){` |
|        - | 10592 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 10593 | `			return SXERR_ABORT;` |
|        - | 10594 | `		}` |
|      ! 0 | 10595 | `		return SXRET_OK;` |
|        - | 10596 | `	}` |
|   300221 | 10597 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   300221 | 10598 | `	pEnd = 0; /* cc warning */` |
|        - | 10599 | `	/* Delimit the class body */` |
|   300221 | 10600 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   300221 | 10601 | `	if( pEnd >= pGen->pEnd ){` |
|        - | 10602 | `		/* Syntax error */` |
|      ! 0 | 10603 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|      ! 0 | 10604 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 10605 | `		if( rc == SXERR_ABORT ){` |
|        - | 10606 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 10607 | `			return SXERR_ABORT;` |
|        - | 10608 | `		}` |
|      ! 0 | 10609 | `		return SXRET_OK;` |
|        - | 10610 | `	}` |
|        - | 10611 | `	/* The delimiter token is the class body's closing brace */` |
|   300221 | 10612 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 10613 | `	/* Swap token stream */` |
|   300221 | 10614 | `	pTmp = pGen->pEnd;` |
|   300221 | 10615 | `	pGen->pEnd = pEnd;` |
|        - | 10616 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|   300221 | 10617 | `	pClass->iFlags \|= iFlags;` |
|        - | 10618 | `	/* Start the parse process */` |
|  1086939 | 10619 | `	for(;;){` |
|        - | 10620 | `		/* Jump leading/trailing semi-colons */` |
|  3110131 | 10621 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   566997 | 10622 | `			pGen->pIn++;` |
|        5 | 10623 | `		}` |
|  2543139 | 10624 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 10625 | `			/* End of class body */` |
|   300179 | 10626 | `			break;` |
|        - | 10627 | `		}` |
|        - | 10628 | `		/* Bind a directly-preceding docblock to this member */` |
|  2242965 | 10629 | `		GenStateSetPendingDoc(&(*pGen));` |
|  2242960 | 10630 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|  1121485 | 10631 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|      ! 0 | 10632 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10633 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|      ! 0 | 10634 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 10635 | `			if( rc == SXERR_ABORT ){` |
|        - | 10636 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 10637 | `				return SXERR_ABORT;` |
|        - | 10638 | `			}` |
|      ! 0 | 10639 | `			goto done;` |
|        - | 10640 | `		}` |
|        - | 10641 | `		/* Assume public visibility */` |
|  2242965 | 10642 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  2242965 | 10643 | `		iAttrflags = 0;` |
|        - | 10644 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|        - | 10645 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|        - | 10646 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|        - | 10647 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  2242965 | 10648 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      ! 0 | 10649 | `			int bMod = 0;` |
|      ! 0 | 10650 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      ! 0 | 10651 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        - | 10652 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|        - | 10653 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|        - | 10654 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|        - | 10655 | `			 * that the generic keyword dispatch would misread as a method. */` |
|      ! 0 | 10656 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      ! 0 | 10657 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      ! 0 | 10658 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|      ! 0 | 10659 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|      ! 0 | 10660 | `			}` |
|      ! 0 | 10661 | `			if( !bMod ){` |
|      ! 0 | 10662 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 10663 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 10664 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10665 | `						return SXERR_ABORT;` |
|        - | 10666 | `					}` |
|      ! 0 | 10667 | `					goto done;` |
|        - | 10668 | `				}` |
|      ! 0 | 10669 | `				continue;` |
|        - | 10670 | `			}` |
|      ! 0 | 10671 | `		}` |
|  2242965 | 10672 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 10673 | `			/* Extract the current keyword */` |
|  2242965 | 10674 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  2242965 | 10675 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|        - | 10676 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|     7799 | 10677 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|     7799 | 10678 | `				if( rc != SXRET_OK ){` |
|        6 | 10679 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10680 | `						return SXERR_ABORT;` |
|        - | 10681 | `					}` |
|        6 | 10682 | `					goto done;` |
|        - | 10683 | `				}` |
|     7795 | 10684 | `				continue;` |
|        - | 10685 | `			}` |
|  2235171 | 10686 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - | 10687 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|        - | 10688 | `				TraitUseEntry sUse;` |
|    15573 | 10689 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|    15573 | 10690 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|    15573 | 10691 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|     7792 | 10692 | `				for(;;){` |
|        - | 10693 | `					ph7_class *pTrait;` |
|        - | 10694 | `					SyString *pTraitName;` |
|    15581 | 10695 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 10696 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 10697 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|      ! 0 | 10698 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10699 | `							return SXERR_ABORT;` |
|        - | 10700 | `						}` |
|      ! 0 | 10701 | `						break;` |
|        - | 10702 | `					}` |
|    15581 | 10703 | `					pTraitName = &pGen->pIn->sData;` |
|        - | 10704 | `					/* Resolve trait name through namespace/imports */ {` |
|        - | 10705 | `						SyBlob sResolved;` |
|    15581 | 10706 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    15581 | 10707 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|    31157 | 10708 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|    15576 | 10709 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    15581 | 10710 | `						SyBlobRelease(&sResolved);` |
|        - | 10711 | `					}` |
|        - | 10712 | `					/* Only traits are allowed */` |
|    15581 | 10713 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 | 10714 | `						pTrait = pTrait->pNextName;` |
|      ! 0 | 10715 | `					}` |
|    15581 | 10716 | `					if( pTrait == 0 ){` |
|      ! 0 | 10717 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 10718 | `							"'%z' is not a trait",pTraitName);` |
|      ! 0 | 10719 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10720 | `							return SXERR_ABORT;` |
|        - | 10721 | `						}` |
|      ! 0 | 10722 | `					}else{` |
|    15581 | 10723 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|        - | 10724 | `					}` |
|    15581 | 10725 | `					pGen->pIn++; /* Advance past trait name */` |
|    15581 | 10726 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     7789 | 10727 | `						break;` |
|        - | 10728 | `					}` |
|       10 | 10729 | `					pGen->pIn++; /* Jump the comma */` |
|        2 | 10730 | `				}` |
|        - | 10731 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|    15573 | 10732 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|        - | 10733 | `					SyToken *pBlock;` |
|       13 | 10734 | `					pGen->pIn++; /* Jump '{' */` |
|       13 | 10735 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       13 | 10736 | `					sUse.pResolvStart = pGen->pIn;` |
|       13 | 10737 | `					sUse.pResolvEnd = pBlock;` |
|       13 | 10738 | `					if( pBlock < pGen->pEnd ){` |
|       13 | 10739 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|        8 | 10740 | `					}else{` |
|      ! 0 | 10741 | `						pGen->pIn = pGen->pEnd;` |
|        - | 10742 | `					}` |
|        5 | 10743 | `				}` |
|    15573 | 10744 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|        - | 10745 | `				/* The semicolon will be consumed by the outer loop */` |
|    15573 | 10746 | `				continue;` |
|        - | 10747 | `			}` |
|  2219603 | 10748 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        - | 10749 | `				int nSetTok;` |
|  2021435 | 10750 | `				sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|  2021435 | 10751 | `				if( nSetVis ){` |
|        - | 10752 | ``					/* Leading `private(set)`/`protected(set)` with no read`` |
|        - | 10753 | `					 * visibility: the read side defaults to public (php 8.4). */` |
|        3 | 10754 | `					iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|        3 | 10755 | `					pGen->pIn += nSetTok;` |
|        2 | 10756 | `				}else{` |
|  2021433 | 10757 | `					iProtection = nKwrd;` |
|  2021433 | 10758 | `					pGen->pIn++; /* Jump the visibility token */` |
|        - | 10759 | `					/* Optional asymmetric set-visibility after the read` |
|        - | 10760 | ``					 * visibility: `public private(set) int $x`. */`` |
|  2021433 | 10761 | `					nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|  2021433 | 10762 | `					if( nSetVis ){` |
|        9 | 10763 | `						iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|        9 | 10764 | `						pGen->pIn += nSetTok;` |
|        4 | 10765 | `					}` |
|        - | 10766 | `				}` |
|        - | 10767 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`,`` |
|        - | 10768 | ``				 * `public private(set) readonly int $x`. */`` |
|  2021435 | 10769 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       24 | 10770 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       24 | 10771 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       10 | 10772 | `				}` |
|  2021430 | 10773 | `				if( pGen->pIn >= pGen->pEnd` |
|  2021435 | 10774 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 10775 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10776 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|      ! 0 | 10777 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 10778 | `					if( rc == SXERR_ABORT ){` |
|        - | 10779 | `						/* Error count limit reached,abort immediately */` |
|      ! 0 | 10780 | `						return SXERR_ABORT;` |
|        - | 10781 | `					}` |
|      ! 0 | 10782 | `					goto done;` |
|        - | 10783 | `				}` |
|  2021435 | 10784 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        - | 10785 | `					/* Attribute declaration (untyped) */` |
|   341693 | 10786 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   341693 | 10787 | `					if( rc != SXRET_OK ){` |
|       11 | 10788 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10789 | `							return SXERR_ABORT;` |
|        - | 10790 | `						}` |
|       11 | 10791 | `						goto done;` |
|        - | 10792 | `					}` |
|   341829 | 10793 | `					continue;` |
|        - | 10794 | `				}` |
|  1679747 | 10795 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        - | 10796 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|      299 | 10797 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      299 | 10798 | `					if( rc != SXRET_OK ){` |
|        8 | 10799 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10800 | `							return SXERR_ABORT;` |
|        - | 10801 | `						}` |
|        8 | 10802 | `						goto done;` |
|        - | 10803 | `					}` |
|      293 | 10804 | `					continue;` |
|        - | 10805 | `				}` |
|        - | 10806 | `				/* Extract the keyword */` |
|  1679453 | 10807 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   839724 | 10808 | `			}` |
|  1877621 | 10809 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|        - | 10810 | `				/* Process constant declaration */` |
|   193955 | 10811 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|   193955 | 10812 | `				if( rc != SXRET_OK ){` |
|       11 | 10813 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10814 | `						return SXERR_ABORT;` |
|        - | 10815 | `					}` |
|       11 | 10816 | `					goto done;` |
|        - | 10817 | `				}` |
|    96976 | 10818 | `			}else{` |
|  1683671 | 10819 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|        - | 10820 | `					/* Static method or attribute,record that */` |
|    66035 | 10821 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    66035 | 10822 | `					pGen->pIn++; /* Jump the static keyword */` |
|    66035 | 10823 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 10824 | `						int nSetTok;` |
|    62131 | 10825 | `						sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|    62131 | 10826 | `						if( nSetVis ){` |
|        - | 10827 | ``							/* `static private(set) int $x` — read side stays public */`` |
|        3 | 10828 | `							iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|        3 | 10829 | `							pGen->pIn += nSetTok;` |
|        2 | 10830 | `						}else{` |
|        - | 10831 | `							/* Extract the keyword */` |
|    62129 | 10832 | `							nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    62129 | 10833 | `							if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      ! 0 | 10834 | `								iProtection = nKwrd;` |
|      ! 0 | 10835 | `								pGen->pIn++; /* Jump the visibility token */` |
|      ! 0 | 10836 | `								nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|      ! 0 | 10837 | `								if( nSetVis ){` |
|      ! 0 | 10838 | `									iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|      ! 0 | 10839 | `									pGen->pIn += nSetTok;` |
|      ! 0 | 10840 | `								}` |
|      ! 0 | 10841 | `							}` |
|        - | 10842 | `						}` |
|    31063 | 10843 | `					}` |
|        - | 10844 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|        - | 10845 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|        - | 10846 | `					 * than a generic "expecting method" parse error. */` |
|    66035 | 10847 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      ! 0 | 10848 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      ! 0 | 10849 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|      ! 0 | 10850 | `					}` |
|    66030 | 10851 | `					if( pGen->pIn >= pGen->pEnd` |
|    66035 | 10852 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 10853 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10854 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|      ! 0 | 10855 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 10856 | `						if( rc == SXERR_ABORT ){` |
|        - | 10857 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 10858 | `							return SXERR_ABORT;` |
|        - | 10859 | `						}` |
|      ! 0 | 10860 | `						goto done;` |
|        - | 10861 | `					}` |
|    66035 | 10862 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        - | 10863 | `						/* Attribute declaration */` |
|     3907 | 10864 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     3907 | 10865 | `						if( rc != SXRET_OK ){` |
|        3 | 10866 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 10867 | `								return SXERR_ABORT;` |
|        - | 10868 | `							}` |
|        3 | 10869 | `							goto done;` |
|        - | 10870 | `						}` |
|     3905 | 10871 | `						continue;` |
|        - | 10872 | `					}` |
|    62133 | 10873 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        - | 10874 | `						/* Typed static attribute declaration */` |
|       17 | 10875 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       17 | 10876 | `						if( rc != SXRET_OK ){` |
|        3 | 10877 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 10878 | `								return SXERR_ABORT;` |
|        - | 10879 | `							}` |
|        3 | 10880 | `							goto done;` |
|        - | 10881 | `						}` |
|       15 | 10882 | `						continue;` |
|        - | 10883 | `					}` |
|        - | 10884 | `					/* Extract the keyword */` |
|    62119 | 10885 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1648698 | 10886 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|        - | 10887 | `					/* Abstract method,record that */` |
|     3899 | 10888 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|        - | 10889 | `					/* Mark the whole class as abstract */` |
|     3899 | 10890 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|        - | 10891 | `					/* Advance the stream cursor */` |
|     3899 | 10892 | `					pGen->pIn++;` |
|     3899 | 10893 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     3899 | 10894 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     3899 | 10895 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     3897 | 10896 | `							iProtection = nKwrd;` |
|     3897 | 10897 | `							pGen->pIn++; /* Jump the visibility token */` |
|     1946 | 10898 | `						}` |
|     1947 | 10899 | `					}` |
|     3899 | 10900 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     3894 | 10901 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - | 10902 | `							/* Static method */` |
|      ! 0 | 10903 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 | 10904 | `							pGen->pIn++; /* Jump the static keyword */` |
|      ! 0 | 10905 | `					}` |
|     3899 | 10906 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|     3894 | 10907 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|        - | 10908 | ``							/* PHP 8.4: `abstract public [T] $x { get; set; }` — an abstract`` |
|        - | 10909 | `							 * HOOKED property declaration. Route anything that is not a` |
|        - | 10910 | `							 * method through the attribute compiler with the ABSTRACT flag;` |
|        - | 10911 | ``							 * the hook parser accepts the bare `get;`/`set;` forms there`` |
|        - | 10912 | `							 * (and a non-hooked abstract property is ITS error to raise). */` |
|        6 | 10913 | `							if( pGen->pIn < pGen->pEnd` |
|        7 | 10914 | `							 && ((pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID\|PH7_TK_DOLLAR)) != 0` |
|        3 | 10915 | `							  \|\| (pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '?')) ){` |
|        7 | 10916 | `								rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        7 | 10917 | `								if( rc != SXRET_OK ){` |
|      ! 0 | 10918 | `									if( rc == SXERR_ABORT ){` |
|      ! 0 | 10919 | `										return SXERR_ABORT;` |
|        - | 10920 | `									}` |
|      ! 0 | 10921 | `									goto done;` |
|        - | 10922 | `								}` |
|        7 | 10923 | `								continue;` |
|        - | 10924 | `							}` |
|      ! 0 | 10925 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10926 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|      ! 0 | 10927 | `								&pGen->pIn->sData,pName);` |
|      ! 0 | 10928 | `							if( rc == SXERR_ABORT ){` |
|        - | 10929 | `								/* Error count limit reached,abort immediately */` |
|      ! 0 | 10930 | `								return SXERR_ABORT;` |
|        - | 10931 | `							}` |
|      ! 0 | 10932 | `							goto done;` |
|        - | 10933 | `					}` |
|     3893 | 10934 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  1615691 | 10935 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|        - | 10936 | `					/* final method ,record that */` |
|       20 | 10937 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       20 | 10938 | `					pGen->pIn++; /* Jump the final keyword */` |
|       20 | 10939 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 10940 | `						/* Extract the keyword */` |
|       20 | 10941 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       20 | 10942 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       10 | 10943 | `							iProtection = nKwrd;` |
|       10 | 10944 | `							pGen->pIn++; /* Jump the visibility token */` |
|        4 | 10945 | `						}` |
|        9 | 10946 | `					}` |
|       20 | 10947 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       18 | 10948 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|        - | 10949 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|        - | 10950 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|        - | 10951 | `							 * child class is compiled (PH7_ClassInherit). */` |
|       14 | 10952 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|       14 | 10953 | `							if( rc != SXRET_OK ){` |
|      ! 0 | 10954 | `								if( rc == SXERR_ABORT ){` |
|      ! 0 | 10955 | `									return SXERR_ABORT;` |
|        - | 10956 | `								}` |
|      ! 0 | 10957 | `								goto done;` |
|        - | 10958 | `							}` |
|       14 | 10959 | `							continue;` |
|        - | 10960 | `					}` |
|        8 | 10961 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        6 | 10962 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - | 10963 | `							/* Static method */` |
|      ! 0 | 10964 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 | 10965 | `							pGen->pIn++; /* Jump the static keyword */` |
|      ! 0 | 10966 | `					}` |
|        8 | 10967 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|        6 | 10968 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 10969 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10970 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|      ! 0 | 10971 | `								&pGen->pIn->sData,pName);` |
|      ! 0 | 10972 | `							if( rc == SXERR_ABORT ){` |
|        - | 10973 | `								/* Error count limit reached,abort immediately */` |
|      ! 0 | 10974 | `								return SXERR_ABORT;` |
|        - | 10975 | `							}` |
|      ! 0 | 10976 | `							goto done;` |
|        - | 10977 | `					}` |
|        8 | 10978 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|        3 | 10979 | `				}` |
|  1679737 | 10980 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|      ! 0 | 10981 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10982 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|      ! 0 | 10983 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 10984 | `						if( rc == SXERR_ABORT ){` |
|        - | 10985 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 10986 | `							return SXERR_ABORT;` |
|        - | 10987 | `						}` |
|      ! 0 | 10988 | `						goto done;` |
|        - | 10989 | `				}` |
|  1679737 | 10990 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|        7 | 10991 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|        7 | 10992 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|      ! 0 | 10993 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10994 | `							"Expecting attribute declaration after 'var' keyword");` |
|      ! 0 | 10995 | `						if( rc == SXERR_ABORT ){` |
|        - | 10996 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 10997 | `							return SXERR_ABORT;` |
|        - | 10998 | `						}` |
|      ! 0 | 10999 | `						goto done;` |
|        - | 11000 | `					}` |
|        - | 11001 | `					/* Attribute declaration */` |
|        7 | 11002 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        4 | 11003 | `				}else{` |
|        - | 11004 | `					/* Process method declaration */` |
|  1679731 | 11005 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|        - | 11006 | `				}` |
|  1679737 | 11007 | `				if( rc != SXRET_OK ){` |
|       16 | 11008 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 11009 | `						return SXERR_ABORT;` |
|        - | 11010 | `					}` |
|       16 | 11011 | `					goto done;` |
|        - | 11012 | `				}` |
|        - | 11013 | `			}` |
|   936836 | 11014 | `		}else{` |
|        - | 11015 | `			/* Attribute declaration */` |
|      ! 0 | 11016 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 11017 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 11018 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 11019 | `					return SXERR_ABORT;` |
|        - | 11020 | `				}` |
|      ! 0 | 11021 | `				goto done;` |
|        - | 11022 | `			}` |
|        - | 11023 | `		}` |
|        5 | 11024 | `	}` |
|        - | 11025 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|        - | 11026 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|        - | 11027 | `	 */` |
|        - | 11028 | `	{` |
|        - | 11029 | `		TraitUseEntry *apUse;` |
|        - | 11030 | `		sxu32 nU;` |
|   300179 | 11031 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   315747 | 11032 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|    15573 | 11033 | `			TraitUseEntry *pUse = &apUse[nU];` |
|    15573 | 11034 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|    15573 | 11035 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|    15573 | 11036 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|        - | 11037 | `			sxu32 nT;` |
|    15573 | 11038 | `			if( !hasResolution ){` |
|        - | 11039 | `				/* No conflict resolution block: use standard trait application */` |
|    31127 | 11040 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|    15569 | 11041 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|    15569 | 11042 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 11043 | `						break;` |
|        - | 11044 | `					}` |
|     7787 | 11045 | `				}` |
|     7784 | 11046 | `			}else{` |
|        - | 11047 | `				/* With resolution block: copy attributes, record traits,` |
|        - | 11048 | `				 * then use the block to resolve method conflicts.` |
|        - | 11049 | `				 */` |
|        - | 11050 | `				SyToken *pR;` |
|       25 | 11051 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       15 | 11052 | `					ph7_class *pTR = apTrait[nT];` |
|        - | 11053 | `					ph7_class_attr *pAR;` |
|        - | 11054 | `					SyHashEntry *pER;` |
|        - | 11055 | `					SyString *pNR;` |
|       15 | 11056 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|       21 | 11057 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|      ! 0 | 11058 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|      ! 0 | 11059 | `						pNR = &pAR->sName;` |
|      ! 0 | 11060 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      ! 0 | 11061 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|      ! 0 | 11062 | `						}` |
|      ! 0 | 11063 | `					}` |
|       15 | 11064 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|        9 | 11065 | `				}` |
|        - | 11066 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       13 | 11067 | `				pR = pUse->pResolvStart;` |
|       27 | 11068 | `				while( pR < pUse->pResolvEnd ){` |
|        - | 11069 | `					SyString sTrait,sMethod;` |
|        - | 11070 | `					ph7_class *pSrcTrait;` |
|        - | 11071 | `					ph7_class_method *pMeth;` |
|        - | 11072 | `					sxi32 nRKwrd;` |
|       41 | 11073 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|       27 | 11074 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       17 | 11075 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       17 | 11076 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       17 | 11077 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       17 | 11078 | `					sMethod = pR->sData;` |
|       17 | 11079 | `					pR++;` |
|       17 | 11080 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        7 | 11081 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        7 | 11082 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        7 | 11083 | `							sTrait = sMethod;` |
|        7 | 11084 | `							pR++;` |
|        7 | 11085 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        7 | 11086 | `							sMethod = pR->sData;` |
|        7 | 11087 | `							pR++;` |
|        3 | 11088 | `						}` |
|        3 | 11089 | `					}` |
|       17 | 11090 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 11091 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 11092 | `						continue;` |
|        - | 11093 | `					}` |
|       17 | 11094 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       17 | 11095 | `					pR++;` |
|       17 | 11096 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|        5 | 11097 | `						pSrcTrait = 0;` |
|        7 | 11098 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        7 | 11099 | `							SyString *pTN = &apTrait[nT]->sName;` |
|       10 | 11100 | `							if( pTN->nByte >= sTrait.nByte &&` |
|        6 | 11101 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|        5 | 11102 | `								pSrcTrait = apTrait[nT];` |
|        5 | 11103 | `								break;` |
|        - | 11104 | `							}` |
|        2 | 11105 | `						}` |
|        5 | 11106 | `						if( pSrcTrait ){` |
|        5 | 11107 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        5 | 11108 | `							if( pMeth ){` |
|        5 | 11109 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|        5 | 11110 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|        5 | 11111 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|        2 | 11112 | `								}` |
|        2 | 11113 | `							}` |
|        2 | 11114 | `						}` |
|        2 | 11115 | `					}` |
|       35 | 11116 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        3 | 11117 | `				}` |
|        - | 11118 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|       25 | 11119 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        - | 11120 | `					ph7_class_method *pMR;` |
|        - | 11121 | `					SyHashEntry *pER;` |
|        - | 11122 | `					SyString *pNR;` |
|       15 | 11123 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|       41 | 11124 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|       23 | 11125 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|       23 | 11126 | `						pNR = &pMR->sFunc.sName;` |
|       23 | 11127 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       14 | 11128 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|        6 | 11129 | `						}` |
|        3 | 11130 | `					}` |
|        9 | 11131 | `				}` |
|        - | 11132 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       13 | 11133 | `				pR = pUse->pResolvStart;` |
|       27 | 11134 | `				while( pR < pUse->pResolvEnd ){` |
|        - | 11135 | `					SyString sTrait,sMethod,sAlias;` |
|        - | 11136 | `					ph7_class *pSrcTrait;` |
|        - | 11137 | `					ph7_class_method *pMeth;` |
|       27 | 11138 | `					int hasQual = 0;` |
|        - | 11139 | `					sxi32 nRKwrd;` |
|       41 | 11140 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|       27 | 11141 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       17 | 11142 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       17 | 11143 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       17 | 11144 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|       17 | 11145 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       17 | 11146 | `					sMethod = pR->sData;` |
|       17 | 11147 | `					pR++;` |
|       17 | 11148 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        7 | 11149 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        7 | 11150 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        7 | 11151 | `							sTrait = sMethod;` |
|        7 | 11152 | `							hasQual = 1;` |
|        7 | 11153 | `							pR++;` |
|        7 | 11154 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        7 | 11155 | `							sMethod = pR->sData;` |
|        7 | 11156 | `							pR++;` |
|        3 | 11157 | `						}` |
|        3 | 11158 | `					}` |
|       17 | 11159 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 11160 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 11161 | `						continue;` |
|        - | 11162 | `					}` |
|       17 | 11163 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       17 | 11164 | `					pR++;` |
|       17 | 11165 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       13 | 11166 | `						sxi32 iNewVis = -1;` |
|       13 | 11167 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|        7 | 11168 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|        7 | 11169 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|        7 | 11170 | `								iNewVis = nAK;` |
|        7 | 11171 | `								pR++;` |
|        3 | 11172 | `							}` |
|        3 | 11173 | `						}` |
|       13 | 11174 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       11 | 11175 | `							sAlias = pR->sData;` |
|       11 | 11176 | `							pR++;` |
|        4 | 11177 | `						}` |
|       13 | 11178 | `						pMeth = 0;` |
|       13 | 11179 | `						if( hasQual ){` |
|        3 | 11180 | `							pSrcTrait = 0;` |
|        5 | 11181 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        5 | 11182 | `								SyString *pTN = &apTrait[nT]->sName;` |
|        7 | 11183 | `								if( pTN->nByte >= sTrait.nByte &&` |
|        4 | 11184 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|        3 | 11185 | `									pSrcTrait = apTrait[nT];` |
|        3 | 11186 | `									break;` |
|        - | 11187 | `								}` |
|        2 | 11188 | `							}` |
|        3 | 11189 | `							if( pSrcTrait ){` |
|        3 | 11190 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        1 | 11191 | `							}` |
|        2 | 11192 | `						}else{` |
|       10 | 11193 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|        - | 11194 | `						}` |
|       13 | 11195 | `						if( pMeth ){` |
|       13 | 11196 | `							if( sAlias.nByte > 0 ){` |
|        - | 11197 | `								/* Create a shallow copy of the method struct for the alias` |
|        - | 11198 | `								 * so it can carry its own visibility without affecting the original.` |
|        - | 11199 | `								 */` |
|        - | 11200 | `								ph7_class_method *pAlias;` |
|        - | 11201 | `								char *zAliasDup;` |
|       11 | 11202 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       11 | 11203 | `								if( pAlias ){` |
|       11 | 11204 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       11 | 11205 | `									if( iNewVis >= 0 ){` |
|        5 | 11206 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|        3 | 11207 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|      ! 0 | 11208 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|        2 | 11209 | `									}` |
|       11 | 11210 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       11 | 11211 | `									if( zAliasDup ){` |
|       11 | 11212 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|        4 | 11213 | `									}` |
|        7 | 11214 | `								}` |
|        7 | 11215 | `							}else if( iNewVis >= 0 ){` |
|        - | 11216 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|        - | 11217 | `								ph7_class_method *pCopy;` |
|        3 | 11218 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        3 | 11219 | `								if( pCopy ){` |
|        3 | 11220 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|        3 | 11221 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|        3 | 11222 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|        3 | 11223 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|      ! 0 | 11224 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|        - | 11225 | `									/* Replace the method in the class hash */` |
|        3 | 11226 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|        3 | 11227 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|        1 | 11228 | `								}` |
|        1 | 11229 | `							}` |
|        5 | 11230 | `						}` |
|        5 | 11231 | `						SXUNUSED(hasQual);` |
|        5 | 11232 | `					}` |
|       21 | 11233 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        3 | 11234 | `				}` |
|        - | 11235 | `			}` |
|    15573 | 11236 | `			SySetRelease(&pUse->aTraits);` |
|     7789 | 11237 | `		}` |
|        - | 11238 | `	}` |
|   300179 | 11239 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|        - | 11240 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|        - | 11241 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|     3905 | 11242 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|     3905 | 11243 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11244 | `			SySetRelease(&aUseEntries);` |
|      ! 0 | 11245 | `			SySetRelease(&aInterfaces);` |
|      ! 0 | 11246 | `			return SXERR_ABORT;` |
|        - | 11247 | `		}` |
|     1950 | 11248 | `	}` |
|        - | 11249 | `	/* Install the class */` |
|   300179 | 11250 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   300179 | 11251 | `	if( rc == SXRET_OK ){` |
|        - | 11252 | `		ph7_class **apInterface;` |
|        - | 11253 | `		sxu32 n;` |
|   300179 | 11254 | `		if( pBase ){` |
|        - | 11255 | `			/* Inherit from base class and mark as a subclass */` |
|   166921 | 11256 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|    83458 | 11257 | `		}` |
|   300179 | 11258 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   393397 | 11259 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|        - | 11260 | `			/* Implements one or more interface */` |
|    93223 | 11261 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    93223 | 11262 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 11263 | `				break;` |
|        - | 11264 | `			}` |
|    46614 | 11265 | `		}` |
|        - | 11266 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|        - | 11267 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|   300179 | 11268 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|     3905 | 11269 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|     3905 | 11270 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 11271 | `				pIntf = pIntf->pNextName;` |
|      ! 0 | 11272 | `			}` |
|     3905 | 11273 | `			if( pIntf ){` |
|     3905 | 11274 | `				PH7_ClassImplement(pClass,pIntf);` |
|     1950 | 11275 | `			}` |
|     3905 | 11276 | `			if( pClass->nEnumBacking != 0 ){` |
|     3893 | 11277 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|     3893 | 11278 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 11279 | `					pIntf = pIntf->pNextName;` |
|      ! 0 | 11280 | `				}` |
|     3893 | 11281 | `				if( pIntf ){` |
|     3893 | 11282 | `					PH7_ClassImplement(pClass,pIntf);` |
|     1944 | 11283 | `				}` |
|     1944 | 11284 | `			}` |
|     1950 | 11285 | `		}` |
|        - | 11286 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|        - | 11287 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|   300174 | 11288 | `		if( rc == SXRET_OK` |
|   300174 | 11289 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|   300179 | 11290 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   190033 | 11291 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|        - | 11292 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   190033 | 11293 | `			if( pStringable ){` |
|   190033 | 11294 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   190033 | 11295 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|        - | 11296 | `				sxu32 i;` |
|   190033 | 11297 | `				int bAlready = 0;` |
|   228797 | 11298 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    42647 | 11299 | `					if( apImpl[i] == pStringable ){` |
|     3883 | 11300 | `						bAlready = 1;` |
|     3883 | 11301 | `						break;` |
|        - | 11302 | `					}` |
|    19387 | 11303 | `				}` |
|   190033 | 11304 | `				if( !bAlready ){` |
|   186155 | 11305 | `					PH7_ClassImplement(pClass,pStringable);` |
|    93075 | 11306 | `				}` |
|    95014 | 11307 | `			}` |
|    95014 | 11308 | `		}` |
|        - | 11309 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   300179 | 11310 | `		if( rc == SXRET_OK ){` |
|   300179 | 11311 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   300179 | 11312 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 11313 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 11314 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 11315 | `				return SXERR_ABORT;` |
|        - | 11316 | `			}` |
|   150087 | 11317 | `		}` |
|        - | 11318 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   300179 | 11319 | `		if( rc == SXRET_OK ){` |
|   300179 | 11320 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   300179 | 11321 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 11322 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 11323 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 11324 | `				return SXERR_ABORT;` |
|        - | 11325 | `			}` |
|   150087 | 11326 | `		}` |
|   150087 | 11327 | `	}` |
|   300179 | 11328 | `	SySetRelease(&aUseEntries);` |
|   300179 | 11329 | `	SySetRelease(&aInterfaces);` |
|   300179 | 11330 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11331 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 11332 | `		return SXERR_ABORT;` |
|        - | 11333 | `	}` |
|   150087 | 11334 | `done:` |
|        - | 11335 | `	/* Point beyond the class body */` |
|   300221 | 11336 | `	pGen->pIn = &pEnd[1];` |
|   300221 | 11337 | `	pGen->pEnd = pTmp;` |
|   300221 | 11338 | `	return PH7_OK;` |
|   150114 | 11339 | `}` |
|        - | 11340 | `/* Compile a named class declaration (the common case). */` |
|   300190 | 11341 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|        5 | 11342 | `{` |
|   300195 | 11343 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|        5 | 11344 | `}` |
|        - | 11345 | `/*` |
|        - | 11346 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|        - | 11347 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|        - | 11348 | ` * compile + install the class body once (at compile time, like every other` |
|        - | 11349 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|        - | 11350 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|        - | 11351 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|        - | 11352 | ` */` |
|       28 | 11353 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        4 | 11354 | `{` |
|        - | 11355 | `	char zName[128];         /* Synthesized class name */` |
|        - | 11356 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|        - | 11357 | `	SyString sName;` |
|        - | 11358 | `	SyToken *pArgStart,*pArgEnd;` |
|       32 | 11359 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|        - | 11360 | `	                              * is keyed to this 'class' token */` |
|        - | 11361 | `	ph7_value *pObj;` |
|       32 | 11362 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11363 | `	sxu32 nIdx,nLen;` |
|        - | 11364 | `	sxi32 nArg,rc;` |
|       14 | 11365 | `	SXUNUSED(iCompileFlag);` |
|        - | 11366 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|       32 | 11367 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|       32 | 11368 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 | 11369 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      ! 0 | 11370 | `	}` |
|       32 | 11371 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|        - | 11372 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|        - | 11373 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|        - | 11374 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|       32 | 11375 | `	pArgStart = pArgEnd = 0;` |
|       32 | 11376 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|       32 | 11377 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11378 | `		return rc;` |
|        - | 11379 | `	}` |
|        - | 11380 | `	{` |
|        - | 11381 | ``		/* Expression-position attributes (`new #[A] class {…}`) */`` |
|       32 | 11382 | `		ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0);` |
|       28 | 11383 | `		if( pAnonClass` |
|       32 | 11384 | `		 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 11385 | `			return SXERR_ABORT;` |
|        - | 11386 | `		}` |
|        - | 11387 | `	}` |
|        - | 11388 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|        - | 11389 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|       32 | 11390 | `	nArg = 0;` |
|       32 | 11391 | `	if( pArgStart < pArgEnd ){` |
|        7 | 11392 | `		SyToken *pSavedIn = pGen->pIn;` |
|        7 | 11393 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|        - | 11394 | `		SyToken *pArgNext;` |
|        7 | 11395 | `		pGen->pIn = pArgStart;` |
|        7 | 11396 | `		pGen->pEnd = pArgEnd;` |
|       13 | 11397 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|        7 | 11398 | `			if( pGen->pIn < pArgNext ){` |
|        7 | 11399 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|        7 | 11400 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 11401 | `					pGen->pIn = pSavedIn;` |
|      ! 0 | 11402 | `					pGen->pEnd = pSavedEnd;` |
|      ! 0 | 11403 | `					return SXERR_ABORT;` |
|        - | 11404 | `				}` |
|        7 | 11405 | `				nArg++;` |
|        3 | 11406 | `			}` |
|        7 | 11407 | `			pGen->pIn = &pArgNext[1];` |
|        1 | 11408 | `		}` |
|        7 | 11409 | `		pGen->pIn = pSavedIn;` |
|        7 | 11410 | `		pGen->pEnd = pSavedEnd;` |
|        3 | 11411 | `	}` |
|        - | 11412 | `	/* Load the synthesized class name */` |
|       32 | 11413 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       32 | 11414 | `	if( pObj == 0 ){` |
|      ! 0 | 11415 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 11416 | `		return SXERR_ABORT;` |
|        - | 11417 | `	}` |
|       32 | 11418 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       32 | 11419 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - | 11420 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|       32 | 11421 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       32 | 11422 | `	return SXRET_OK;` |
|       18 | 11423 | `}` |
|        - | 11424 | `/*` |
|        - | 11425 | ` * Compile a user-defined abstract class.` |
|        - | 11426 | ` *  According to the PHP language reference manual` |
|        - | 11427 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|        - | 11428 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|        - | 11429 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|        - | 11430 | ` *   the method's signature - they cannot define the implementation.` |
|        - | 11431 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|        - | 11432 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|        - | 11433 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|        - | 11434 | ` *   method is defined as protected, the function implementation must be defined as either` |
|        - | 11435 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|        - | 11436 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|        - | 11437 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|        - | 11438 | ` *   could differ.` |
|        - | 11439 | ` */` |
|        - | 11440 | `/*` |
|        - | 11441 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|        - | 11442 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|        - | 11443 | ` * receives the corresponding PH7_CLASS_* bit.` |
|        - | 11444 | ` */` |
|  8499726 | 11445 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|        5 | 11446 | `{` |
|  8499731 | 11447 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  5009897 | 11448 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  5009897 | 11449 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  4963359 | 11450 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  2470006 | 11451 | `	}` |
|  8429851 | 11452 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  8429791 | 11453 | `	return FALSE;` |
|  4249868 | 11454 | `}` |
|        - | 11455 | `/*` |
|        - | 11456 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|        - | 11457 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|        - | 11458 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|        - | 11459 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|        - | 11460 | ` */` |
|  8429786 | 11461 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|        5 | 11462 | `{` |
|  8429791 | 11463 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  8429791 | 11464 | `	sxi32 iFlags = 0,iFlag;` |
|  8499731 | 11465 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|    69945 | 11466 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|        5 | 11467 | `			pDup = pIn;` |
|        2 | 11468 | `		}` |
|    69945 | 11469 | `		iFlags \|= iFlag;` |
|    69945 | 11470 | `		pIn++;` |
|        5 | 11471 | `	}` |
|  8429791 | 11472 | `	*ppIn = pIn;` |
|  8429791 | 11473 | `	if( ppDup ){ *ppDup = pDup; }` |
|  8429791 | 11474 | `	return iFlags;` |
|        5 | 11475 | `}` |
|        - | 11476 | `/*` |
|        - | 11477 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|        - | 11478 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|        - | 11479 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|        - | 11480 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|        - | 11481 | `` * `readonly`) to their existing handlers.`` |
|        - | 11482 | ` */` |
|  8398702 | 11483 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|        5 | 11484 | `{` |
|  8398707 | 11485 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  4238194 | 11486 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  8418122 | 11487 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|        5 | 11488 | `}` |
|        - | 11489 | `/*` |
|        - | 11490 | ` * Compile a class declaration carrying one or more leading modifiers` |
|        - | 11491 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|        - | 11492 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|        - | 11493 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|        - | 11494 | `` * `abstract`+`final` pair, like PHP.`` |
|        - | 11495 | ` */` |
|    31084 | 11496 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|        5 | 11497 | `{` |
|        - | 11498 | `	SyToken *pDup;` |
|    31089 | 11499 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|        - | 11500 | `	sxi32 rc;` |
|    31089 | 11501 | `	if( pDup ){` |
|        4 | 11502 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|        2 | 11503 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|        3 | 11504 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11505 | `			return SXERR_ABORT;` |
|        - | 11506 | `		}` |
|        1 | 11507 | `	}` |
|    31084 | 11508 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|    15547 | 11509 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|        3 | 11510 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11511 | `			"Cannot use the final modifier on an abstract class");` |
|        3 | 11512 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11513 | `			return SXERR_ABORT;` |
|        - | 11514 | `		}` |
|        1 | 11515 | `	}` |
|    31089 | 11516 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|    15547 | 11517 | `}` |
|        - | 11518 | `/*` |
|        - | 11519 | ` * Compile a user-defined trait.` |
|        - | 11520 | ` *  Traits are similar to classes, but only intended to group functionality` |
|        - | 11521 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|        - | 11522 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|        - | 11523 | ` */` |
|     7830 | 11524 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|        5 | 11525 | `{` |
|     7835 | 11526 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11527 | `	ph7_class *pClass;` |
|        - | 11528 | `	SyToken *pEnd,*pTmp;` |
|        - | 11529 | `	sxi32 iProtection;` |
|        - | 11530 | `	sxi32 iAttrflags;` |
|        - | 11531 | `	SyString *pName;` |
|        - | 11532 | `	sxi32 nKwrd;` |
|        - | 11533 | `	sxi32 rc;` |
|        - | 11534 | `	/* Jump the 'trait' keyword */` |
|     7835 | 11535 | `	pGen->pIn++;` |
|     7835 | 11536 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 11537 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|      ! 0 | 11538 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11539 | `			return SXERR_ABORT;` |
|        - | 11540 | `		}` |
|      ! 0 | 11541 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|      ! 0 | 11542 | `			pGen->pIn++;` |
|      ! 0 | 11543 | `		}` |
|      ! 0 | 11544 | `		return SXRET_OK;` |
|        - | 11545 | `	}` |
|        - | 11546 | `	/* Extract trait name */` |
|     7835 | 11547 | `	pName = &pGen->pIn->sData;` |
|     7835 | 11548 | `	pGen->pIn++;` |
|        - | 11549 | `	/* Build FQN and obtain a raw class */ {` |
|        - | 11550 | `		SyBlob sFQN;` |
|        - | 11551 | `		SyString sFQNStr;` |
|     7835 | 11552 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     7835 | 11553 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|     7835 | 11554 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|     7835 | 11555 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|     7835 | 11556 | `		SyBlobRelease(&sFQN);` |
|        - | 11557 | `	}` |
|     7835 | 11558 | `	if( pClass == 0 ){` |
|      ! 0 | 11559 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 11560 | `		return SXERR_ABORT;` |
|        - | 11561 | `	}` |
|     7835 | 11562 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|     7835 | 11563 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 11564 | `		return SXERR_ABORT;` |
|        - | 11565 | `	}` |
|        - | 11566 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|     7835 | 11567 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|      ! 0 | 11568 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|      ! 0 | 11569 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 11570 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11571 | `			return SXERR_ABORT;` |
|        - | 11572 | `		}` |
|      ! 0 | 11573 | `		return SXRET_OK;` |
|        - | 11574 | `	}` |
|     7835 | 11575 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|     7835 | 11576 | `	pEnd = 0;` |
|     7835 | 11577 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|     7835 | 11578 | `	if( pEnd >= pGen->pEnd ){` |
|      ! 0 | 11579 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|      ! 0 | 11580 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 11581 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11582 | `			return SXERR_ABORT;` |
|        - | 11583 | `		}` |
|      ! 0 | 11584 | `		return SXRET_OK;` |
|        - | 11585 | `	}` |
|        - | 11586 | `	/* The delimiter token is the trait body's closing brace */` |
|     7835 | 11587 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 11588 | `	/* Swap token stream */` |
|     7835 | 11589 | `	pTmp = pGen->pEnd;` |
|     7835 | 11590 | `	pGen->pEnd = pEnd;` |
|        - | 11591 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|     7835 | 11592 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|        - | 11593 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|    54343 | 11594 | `	for(;;){` |
|   147499 | 11595 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|    19411 | 11596 | `			pGen->pIn++;` |
|        5 | 11597 | `		}` |
|   128093 | 11598 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     7835 | 11599 | `			break;` |
|        - | 11600 | `		}` |
|        - | 11601 | `		/* Bind a directly-preceding docblock to this member */` |
|   120263 | 11602 | `		GenStateSetPendingDoc(&(*pGen));` |
|   120263 | 11603 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|      ! 0 | 11604 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11605 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|      ! 0 | 11606 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 11607 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11608 | `				return SXERR_ABORT;` |
|        - | 11609 | `			}` |
|      ! 0 | 11610 | `			goto done;` |
|        - | 11611 | `		}` |
|   120263 | 11612 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|   120263 | 11613 | `		iAttrflags = 0;` |
|   120263 | 11614 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   120263 | 11615 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   120263 | 11616 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - | 11617 | `				/* Trait uses another trait: use OtherTrait; */` |
|        5 | 11618 | `				pGen->pIn++; /* Jump 'use' */` |
|        2 | 11619 | `				for(;;){` |
|        - | 11620 | `					ph7_class *pUsedTrait;` |
|        - | 11621 | `					SyString *pUsedName;` |
|        5 | 11622 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 11623 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 11624 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|      ! 0 | 11625 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11626 | `							return SXERR_ABORT;` |
|        - | 11627 | `						}` |
|      ! 0 | 11628 | `						break;` |
|        - | 11629 | `					}` |
|        5 | 11630 | `					pUsedName = &pGen->pIn->sData;` |
|        - | 11631 | `					{` |
|        - | 11632 | `						SyBlob sResolved;` |
|        5 | 11633 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|        5 | 11634 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|        7 | 11635 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|        4 | 11636 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|        5 | 11637 | `						SyBlobRelease(&sResolved);` |
|        - | 11638 | `					}` |
|        5 | 11639 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 | 11640 | `						pUsedTrait = pUsedTrait->pNextName;` |
|      ! 0 | 11641 | `					}` |
|        5 | 11642 | `					if( pUsedTrait == 0 ){` |
|        4 | 11643 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        1 | 11644 | `							"'%z' is not a trait",pUsedName);` |
|        3 | 11645 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11646 | `							return SXERR_ABORT;` |
|        - | 11647 | `						}` |
|        2 | 11648 | `					}else{` |
|        3 | 11649 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|        - | 11650 | `					}` |
|        5 | 11651 | `					pGen->pIn++;` |
|        5 | 11652 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|        3 | 11653 | `						break;` |
|        - | 11654 | `					}` |
|      ! 0 | 11655 | `					pGen->pIn++;` |
|      ! 0 | 11656 | `				}` |
|        5 | 11657 | `				continue;` |
|        - | 11658 | `			}` |
|   120259 | 11659 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|   120243 | 11660 | `				iProtection = nKwrd;` |
|   120243 | 11661 | `				pGen->pIn++;` |
|   120238 | 11662 | `				if( pGen->pIn >= pGen->pEnd` |
|   120243 | 11663 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 11664 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11665 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|      ! 0 | 11666 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 11667 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 11668 | `						return SXERR_ABORT;` |
|        - | 11669 | `					}` |
|      ! 0 | 11670 | `					goto done;` |
|        - | 11671 | `				}` |
|   120243 | 11672 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|    19397 | 11673 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|    19397 | 11674 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 11675 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11676 | `							return SXERR_ABORT;` |
|        - | 11677 | `						}` |
|      ! 0 | 11678 | `						goto done;` |
|        - | 11679 | `					}` |
|    19397 | 11680 | `					continue;` |
|        - | 11681 | `				}` |
|   100851 | 11682 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        5 | 11683 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        5 | 11684 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 11685 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11686 | `							return SXERR_ABORT;` |
|        - | 11687 | `						}` |
|      ! 0 | 11688 | `						goto done;` |
|        - | 11689 | `					}` |
|        5 | 11690 | `					continue;` |
|        - | 11691 | `				}` |
|   100847 | 11692 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    50421 | 11693 | `			}` |
|   100863 | 11694 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|      ! 0 | 11695 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11696 | `					"Traits cannot have constants");` |
|      ! 0 | 11697 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 11698 | `					return SXERR_ABORT;` |
|        - | 11699 | `				}` |
|      ! 0 | 11700 | `				goto done;` |
|      ! 0 | 11701 | `			}else{` |
|   100863 | 11702 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|     7767 | 11703 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     7767 | 11704 | `					pGen->pIn++;` |
|     7767 | 11705 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     7765 | 11706 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     7765 | 11707 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      ! 0 | 11708 | `							iProtection = nKwrd;` |
|      ! 0 | 11709 | `							pGen->pIn++;` |
|      ! 0 | 11710 | `						}` |
|     3880 | 11711 | `					}` |
|     7762 | 11712 | `					if( pGen->pIn >= pGen->pEnd` |
|     7767 | 11713 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 11714 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11715 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|      ! 0 | 11716 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 11717 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11718 | `							return SXERR_ABORT;` |
|        - | 11719 | `						}` |
|      ! 0 | 11720 | `						goto done;` |
|        - | 11721 | `					}` |
|     7767 | 11722 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        3 | 11723 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        3 | 11724 | `						if( rc != SXRET_OK ){` |
|      ! 0 | 11725 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 11726 | `								return SXERR_ABORT;` |
|        - | 11727 | `							}` |
|      ! 0 | 11728 | `							goto done;` |
|        - | 11729 | `						}` |
|        3 | 11730 | `						continue;` |
|        - | 11731 | `					}` |
|     7765 | 11732 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|      ! 0 | 11733 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 11734 | `						if( rc != SXRET_OK ){` |
|      ! 0 | 11735 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 11736 | `								return SXERR_ABORT;` |
|        - | 11737 | `							}` |
|      ! 0 | 11738 | `							goto done;` |
|        - | 11739 | `						}` |
|      ! 0 | 11740 | `						continue;` |
|        - | 11741 | `					}` |
|     7765 | 11742 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    96981 | 11743 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|        6 | 11744 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|        6 | 11745 | `					pGen->pIn++;` |
|        6 | 11746 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        6 | 11747 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        6 | 11748 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        6 | 11749 | `							iProtection = nKwrd;` |
|        6 | 11750 | `							pGen->pIn++;` |
|        2 | 11751 | `						}` |
|        2 | 11752 | `					}` |
|        6 | 11753 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|        4 | 11754 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 11755 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11756 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|      ! 0 | 11757 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 11758 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11759 | `							return SXERR_ABORT;` |
|        - | 11760 | `						}` |
|      ! 0 | 11761 | `						goto done;` |
|        - | 11762 | `					}` |
|        6 | 11763 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|        2 | 11764 | `				}` |
|   100861 | 11765 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|      ! 0 | 11766 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11767 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|      ! 0 | 11768 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 11769 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 11770 | `						return SXERR_ABORT;` |
|        - | 11771 | `					}` |
|      ! 0 | 11772 | `					goto done;` |
|        - | 11773 | `				}` |
|   100861 | 11774 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|      ! 0 | 11775 | `					pGen->pIn++;` |
|      ! 0 | 11776 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 11777 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11778 | `							"Expecting attribute declaration after 'var' keyword");` |
|      ! 0 | 11779 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11780 | `							return SXERR_ABORT;` |
|        - | 11781 | `						}` |
|      ! 0 | 11782 | `						goto done;` |
|        - | 11783 | `					}` |
|      ! 0 | 11784 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 11785 | `				}else{` |
|   100861 | 11786 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|        - | 11787 | `				}` |
|   100861 | 11788 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 11789 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 11790 | `						return SXERR_ABORT;` |
|        - | 11791 | `					}` |
|      ! 0 | 11792 | `					goto done;` |
|        - | 11793 | `				}` |
|        - | 11794 | `			}` |
|    50433 | 11795 | `		}else{` |
|      ! 0 | 11796 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 11797 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 11798 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 11799 | `					return SXERR_ABORT;` |
|        - | 11800 | `				}` |
|      ! 0 | 11801 | `				goto done;` |
|        - | 11802 | `			}` |
|        - | 11803 | `		}` |
|        5 | 11804 | `	}` |
|        - | 11805 | `	/* Install the trait */` |
|     7835 | 11806 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|     7835 | 11807 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11808 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 11809 | `		return SXERR_ABORT;` |
|        - | 11810 | `	}` |
|     3915 | 11811 | `done:` |
|        - | 11812 | `	/* Point beyond the trait body */` |
|     7835 | 11813 | `	pGen->pIn = &pEnd[1];` |
|     7835 | 11814 | `	pGen->pEnd = pTmp;` |
|     7835 | 11815 | `	return PH7_OK;` |
|     3920 | 11816 | `}` |
|        - | 11817 | `/*` |
|        - | 11818 | ` * Compile a user-defined class.` |
|        - | 11819 | ` *  According to the PHP language reference manual` |
|        - | 11820 | ` *   Basic class definitions begin with the keyword class, followed` |
|        - | 11821 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|        - | 11822 | ` *   the definitions of the properties and methods belonging to the class.` |
|        - | 11823 | ` *   A class may contain its own constants, variables (called "properties")` |
|        - | 11824 | ` *   and functions (called "methods").` |
|        - | 11825 | ` */` |
|   265202 | 11826 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|        5 | 11827 | `{` |
|        - | 11828 | `	sxi32 rc;` |
|   265207 | 11829 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   265207 | 11830 | `	return rc;` |
|        5 | 11831 | `}` |
|        - | 11832 | `/*` |
|        - | 11833 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|        - | 11834 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|        - | 11835 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|        - | 11836 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|        - | 11837 | `` * meaning; `enum Name` can never start a valid expression.`` |
|        - | 11838 | ` */` |
|  8359866 | 11839 | `static int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|        5 | 11840 | `{` |
|  8424140 | 11841 | `	return (pIn->nType & PH7_TK_ID)` |
|  4244202 | 11842 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|    74082 | 11843 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
|  8424135 | 11844 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|        5 | 11845 | `}` |
|        - | 11846 | `/*` |
|        - | 11847 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|        - | 11848 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|        - | 11849 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|        - | 11850 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|        - | 11851 | ` */` |
|     3904 | 11852 | `static sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|        5 | 11853 | `{` |
|     3909 | 11854 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|        5 | 11855 | `}` |
|        - | 11856 | `/*` |
|        - | 11857 | ` * Exception handling.` |
|        - | 11858 | ` *  According to the PHP language reference manual` |
|        - | 11859 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|        - | 11860 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|        - | 11861 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|        - | 11862 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|        - | 11863 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|        - | 11864 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|        - | 11865 | ` *    (or re-thrown) within a catch block.` |
|        - | 11866 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|        - | 11867 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|        - | 11868 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|        - | 11869 | ` *    been defined with set_exception_handler().` |
|        - | 11870 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|        - | 11871 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|        - | 11872 | ` */` |
|        - | 11873 | `/*` |
|        - | 11874 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|        - | 11875 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|        - | 11876 | ` * indicates failure.` |
|        - | 11877 | ` */` |
|   384136 | 11878 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 11879 | `{` |
|   384141 | 11880 | `	sxi32 rc = SXRET_OK;` |
|   384141 | 11881 | `	if( pRoot->pOp ){` |
|   384129 | 11882 | `		switch( pRoot->pOp->iOp ){` |
|   192062 | 11883 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|        - | 11884 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|        - | 11885 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|        - | 11886 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|        - | 11887 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|        - | 11888 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|   384129 | 11889 | `			break;` |
|      ! 0 | 11890 | `		default:` |
|        - | 11891 | `			/* Runtime will still reject non-Throwable values; the set above` |
|        - | 11892 | `			 * covers the common shapes and gives a friendlier compile error` |
|        - | 11893 | ``			 * for obvious mistakes like `throw 5`. */`` |
|      ! 0 | 11894 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 11895 | `				"throw: Expecting an exception class instance");` |
|      ! 0 | 11896 | `			if( rc != SXERR_ABORT ){` |
|      ! 0 | 11897 | `				rc = SXERR_INVALID;` |
|      ! 0 | 11898 | `			}` |
|      ! 0 | 11899 | `			break;` |
|        - | 11900 | `		}` |
|   192079 | 11901 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 11902 | `		/* Unexpected expression */` |
|      ! 0 | 11903 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 11904 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 11905 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 | 11906 | `			rc = SXERR_INVALID;` |
|      ! 0 | 11907 | `		}` |
|      ! 0 | 11908 | `	}` |
|   384141 | 11909 | `	return rc;` |
|        5 | 11910 | `}` |
|        - | 11911 | `/*` |
|        - | 11912 | ` * Compile a 'throw' statement.` |
|        - | 11913 | ` * throw: This is how you trigger an exception.` |
|        - | 11914 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|        - | 11915 | ` */` |
|   384100 | 11916 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|        5 | 11917 | `{` |
|   384105 | 11918 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11919 | `	GenBlock *pBlock;` |
|        - | 11920 | `	sxu32 nIdx;` |
|        - | 11921 | `	sxi32 rc;` |
|   384105 | 11922 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|        - | 11923 | `	/* Compile the expression */` |
|   384105 | 11924 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   384105 | 11925 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 11926 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|      ! 0 | 11927 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11928 | `			return SXERR_ABORT;` |
|        - | 11929 | `		}` |
|      ! 0 | 11930 | `		return SXRET_OK;` |
|        - | 11931 | `	}` |
|   384105 | 11932 | `	pBlock = pGen->pCurrent;` |
|        - | 11933 | `	/* Point to the top most function or try block and emit the forward jump */` |
|  1496913 | 11934 | `	while(pBlock->pParent){` |
|  1496909 | 11935 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   384101 | 11936 | `			break;` |
|        - | 11937 | `		}` |
|        - | 11938 | `		/* Point to the parent block */` |
|  1112813 | 11939 | `		pBlock = pBlock->pParent;` |
|        5 | 11940 | `	}` |
|        - | 11941 | `	/* Emit the throw instruction */` |
|   384105 | 11942 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        - | 11943 | `	/* Emit the jump */` |
|   384105 | 11944 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   384105 | 11945 | `	return SXRET_OK;` |
|   192055 | 11946 | `}` |
|        - | 11947 | `/*` |
|        - | 11948 | ` * Compile a PHP 8.0 'throw' expression.` |
|        - | 11949 | ` * Called from the expression code generator when a 'throw' keyword is` |
|        - | 11950 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|        - | 11951 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|        - | 11952 | ` * the validator guarantees the operand is a valid exception target.` |
|        - | 11953 | ` */` |
|       36 | 11954 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        2 | 11955 | `{` |
|       38 | 11956 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11957 | `	GenBlock *pBlock;` |
|        - | 11958 | `	sxu32 nIdx;` |
|        - | 11959 | `	sxi32 rc;` |
|       18 | 11960 | `	(void)iCompileFlag;` |
|       38 | 11961 | `	pGen->pIn++; /* Skip 'throw' */` |
|       38 | 11962 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 11963 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 11964 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 11965 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11966 | `			return SXERR_ABORT;` |
|        - | 11967 | `		}` |
|      ! 0 | 11968 | `		return SXRET_OK;` |
|        - | 11969 | `	}` |
|       38 | 11970 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|       38 | 11971 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 11972 | `		return SXERR_ABORT;` |
|        - | 11973 | `	}` |
|       38 | 11974 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 11975 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 11976 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 11977 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11978 | `			return SXERR_ABORT;` |
|        - | 11979 | `		}` |
|      ! 0 | 11980 | `		return SXRET_OK;` |
|        - | 11981 | `	}` |
|        - | 11982 | `	/* Walk up to nearest exception/function block for the jump target */` |
|       38 | 11983 | `	pBlock = pGen->pCurrent;` |
|       60 | 11984 | `	while( pBlock->pParent ){` |
|       49 | 11985 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|       27 | 11986 | `			break;` |
|        - | 11987 | `		}` |
|       23 | 11988 | `		pBlock = pBlock->pParent;` |
|        1 | 11989 | `	}` |
|       38 | 11990 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       38 | 11991 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|       38 | 11992 | `	return SXRET_OK;` |
|       20 | 11993 | `}` |
|        - | 11994 | `/*` |
|        - | 11995 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|        - | 11996 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|        - | 11997 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|        - | 11998 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|        - | 11999 | ` * compile error propagated from the parser.` |
|        - | 12000 | ` */` |
|       54 | 12001 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|        5 | 12002 | `{` |
|        - | 12003 | `	SyString sClassName;` |
|        - | 12004 | `	SyToken *pToken;` |
|        - | 12005 | `	SyString *pName;` |
|        - | 12006 | `	char *zDup;` |
|        - | 12007 | `	sxi32 rc;` |
|       59 | 12008 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       59 | 12009 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|       59 | 12010 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|       59 | 12011 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       59 | 12012 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 | 12013 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 12014 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 12015 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 12016 | `		return SXERR_INVALID;` |
|        - | 12017 | `	}` |
|       59 | 12018 | `	pGen->pIn++; /* '(' */` |
|       27 | 12019 | `	for(;;){` |
|        - | 12020 | `		SyBlob sResolved;` |
|       59 | 12021 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       59 | 12022 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 12023 | `			SyBlobRelease(&sResolved);` |
|      ! 0 | 12024 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 12025 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 12026 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 12027 | `			return SXERR_INVALID;` |
|        - | 12028 | `		}` |
|       86 | 12029 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       54 | 12030 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       59 | 12031 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|       59 | 12032 | `		SyBlobRelease(&sResolved);` |
|       59 | 12033 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|       59 | 12034 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|       59 | 12035 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       54 | 12036 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|        5 | 12037 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      ! 0 | 12038 | `			pGen->pIn++; continue;` |
|        - | 12039 | `		}` |
|       59 | 12040 | `		break;` |
|      ! 0 | 12041 | `	}` |
|       54 | 12042 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|       59 | 12043 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 12044 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 12045 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 12046 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 12047 | `		return SXERR_INVALID;` |
|        - | 12048 | `	}` |
|       59 | 12049 | `	pGen->pIn++; /* '$' */` |
|       59 | 12050 | `	pName = &pGen->pIn->sData;` |
|       59 | 12051 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       59 | 12052 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|       59 | 12053 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|       59 | 12054 | `	pGen->pIn++;` |
|       59 | 12055 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 12056 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 12057 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 12058 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 12059 | `		return SXERR_INVALID;` |
|        - | 12060 | `	}` |
|       59 | 12061 | `	pGen->pIn++; /* ')' */` |
|       59 | 12062 | `	return SXRET_OK;` |
|       32 | 12063 | `}` |
|        - | 12064 | `/*` |
|        - | 12065 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|        - | 12066 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|        - | 12067 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|        - | 12068 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|        - | 12069 | ` * VmThrowException):` |
|        - | 12070 | ` *` |
|        - | 12071 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|        - | 12072 | ` *    <try body>` |
|        - | 12073 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|        - | 12074 | ` *    JMP  -> finally\|end` |
|        - | 12075 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|        - | 12076 | ` *    <catch body>` |
|        - | 12077 | ` *    JMP  -> finally\|end` |
|        - | 12078 | ` *    ... more catches ...` |
|        - | 12079 | ` *  Lfin: <finally body>` |
|        - | 12080 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|        - | 12081 | ` *  Lend:` |
|        - | 12082 | ` */` |
|       98 | 12083 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|        5 | 12084 | `{` |
|      103 | 12085 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 12086 | `	GenBlock *pTry;` |
|        - | 12087 | `	VmInstr *pInstr;` |
|      103 | 12088 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|        - | 12089 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|        - | 12090 | `	sxi32 rc;` |
|      103 | 12091 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|        - | 12092 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|      103 | 12093 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      103 | 12094 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      103 | 12095 | `	pTry->pUserData = pException;` |
|      103 | 12096 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|      103 | 12097 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|      103 | 12098 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      103 | 12099 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      103 | 12100 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|      103 | 12101 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|        - | 12102 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|      103 | 12103 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|      103 | 12104 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|      103 | 12105 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      103 | 12106 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 12107 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|      103 | 12108 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|        - | 12109 | `	/* Catch clauses (inline) */` |
|      103 | 12110 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       98 | 12111 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       59 | 12112 | `		sxu32 k = 0;` |
|       81 | 12113 | `		for(;;){` |
|        - | 12114 | `			ph7_exception_block sCatch;` |
|        - | 12115 | `			GenBlock *pCatchBlk;` |
|      113 | 12116 | `			sxu32 idxJmp = 0;` |
|      108 | 12117 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      104 | 12118 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|       32 | 12119 | `				break;` |
|        - | 12120 | `			}` |
|       59 | 12121 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|       59 | 12122 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       59 | 12123 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|       59 | 12124 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|       59 | 12125 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|       59 | 12126 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|       59 | 12127 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        - | 12128 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|        - | 12129 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|        - | 12130 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|       59 | 12131 | `			pCatchBlk->pUserData = pException;` |
|       59 | 12132 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|       59 | 12133 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       59 | 12134 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       59 | 12135 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        - | 12136 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|        - | 12137 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|       59 | 12138 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       59 | 12139 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|       59 | 12140 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|       59 | 12141 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|       59 | 12142 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       59 | 12143 | `			k++;` |
|        5 | 12144 | `		}` |
|       27 | 12145 | `	}` |
|        - | 12146 | `	/* Finally (inline) */` |
|      103 | 12147 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       80 | 12148 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 12149 | `		GenBlock *pFinBlk;` |
|       52 | 12150 | `		pGen->pIn++; /* Jump 'finally' */` |
|       52 | 12151 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|       52 | 12152 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|       52 | 12153 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       52 | 12154 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       52 | 12155 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       52 | 12156 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       52 | 12157 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       52 | 12158 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|       52 | 12159 | `		pException->iHasFinally = 1;` |
|       24 | 12160 | `	}` |
|      103 | 12161 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|      103 | 12162 | `	pException->iInlined = 1;` |
|        - | 12163 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|        - | 12164 | `	{` |
|      103 | 12165 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|        - | 12166 | `		sxu32 *aJ; sxu32 n;` |
|      103 | 12167 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|      103 | 12168 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      103 | 12169 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|      157 | 12170 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|       59 | 12171 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|       59 | 12172 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       32 | 12173 | `		}` |
|        - | 12174 | `	}` |
|      103 | 12175 | `	SySetRelease(&aCatchJmp);` |
|      103 | 12176 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|      ! 0 | 12177 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|      ! 0 | 12178 | `	}` |
|      103 | 12179 | `	return SXRET_OK;` |
|       54 | 12180 | `}` |
|        - | 12181 | `/*` |
|        - | 12182 | ` * Compile a 'catch' block.` |
|        - | 12183 | ` * Catch: A "catch" block retrieves an exception and creates` |
|        - | 12184 | ` * an object containing the exception information.` |
|        - | 12185 | ` */` |
|    20794 | 12186 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|        5 | 12187 | `{` |
|    20799 | 12188 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 12189 | `	ph7_exception_block sCatch;` |
|        - | 12190 | `	SySet *pInstrContainer;` |
|        - | 12191 | `	SyString sClassName;` |
|        - | 12192 | `	GenBlock *pCatch;` |
|        - | 12193 | `	SyToken *pToken;` |
|        - | 12194 | `	SyString *pName;` |
|        - | 12195 | `	char *zDup;` |
|        - | 12196 | `	sxi32 rc;` |
|    20799 | 12197 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        - | 12198 | `	/* Zero the structure */` |
|    20799 | 12199 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|        - | 12200 | `	/* Initialize fields */` |
|    20799 | 12201 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|    20799 | 12202 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|    20799 | 12203 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|        - | 12204 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 12205 | `			pToken = pGen->pIn;` |
|      ! 0 | 12206 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 12207 | `				pToken--;` |
|      ! 0 | 12208 | `			}` |
|      ! 0 | 12209 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 12210 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 12211 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 12212 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 12213 | `				return SXERR_ABORT;` |
|        - | 12214 | `			}` |
|      ! 0 | 12215 | `			return SXERR_INVALID;` |
|        - | 12216 | `	}` |
|        - | 12217 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|    20799 | 12218 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|    10411 | 12219 | `	for(;;){` |
|        - | 12220 | `		SyBlob sResolved;` |
|    20827 | 12221 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    20827 | 12222 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        6 | 12223 | `			SyBlobRelease(&sResolved);` |
|        6 | 12224 | `			pToken = pGen->pIn;` |
|        6 | 12225 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 12226 | `				pToken--;` |
|      ! 0 | 12227 | `			}` |
|        8 | 12228 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 12229 | `				"syntax error, unexpected %s \"%z\"",` |
|        2 | 12230 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|        6 | 12231 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 12232 | `				return SXERR_ABORT;` |
|        - | 12233 | `			}` |
|        6 | 12234 | `			return SXERR_INVALID;` |
|        - | 12235 | `		}` |
|        - | 12236 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|        - | 12237 | `		 * transient SyBlob allocation. */` |
|    31232 | 12238 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    20818 | 12239 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|    20823 | 12240 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|    20823 | 12241 | `		SyBlobRelease(&sResolved);` |
|    20823 | 12242 | `		if( zDup == 0 ){` |
|      ! 0 | 12243 | `			goto Mem;` |
|        - | 12244 | `		}` |
|    20823 | 12245 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|    20823 | 12246 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12247 | `			goto Mem;` |
|        - | 12248 | `		}` |
|        - | 12249 | `		/* Check for '\|' (multi-catch separator) */` |
|    20818 | 12250 | `		if( pGen->pIn < pGen->pEnd &&` |
|    20818 | 12251 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|       33 | 12252 | `			pGen->pIn->sData.nByte == 1 &&` |
|       28 | 12253 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|       30 | 12254 | `			pGen->pIn++; /* Consume the '\|' */` |
|       30 | 12255 | `			continue;` |
|        - | 12256 | `		}` |
|    20795 | 12257 | `		break;` |
|      ! 0 | 12258 | `	}` |
|    20790 | 12259 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|    20795 | 12260 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 12261 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 12262 | `			pToken = pGen->pIn;` |
|      ! 0 | 12263 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 12264 | `				pToken--;` |
|      ! 0 | 12265 | `			}` |
|      ! 0 | 12266 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 12267 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 12268 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 12269 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 12270 | `				return SXERR_ABORT;` |
|        - | 12271 | `			}` |
|      ! 0 | 12272 | `			return SXERR_INVALID;` |
|        - | 12273 | `	}` |
|    20795 | 12274 | `	pGen->pIn++; /* Jump the dollar sign */` |
|        - | 12275 | `	/* Duplicate instance name */` |
|    20795 | 12276 | `	pName = &pGen->pIn->sData;` |
|    20795 | 12277 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    20795 | 12278 | `	if( zDup == 0 ){` |
|      ! 0 | 12279 | `		goto Mem;` |
|        - | 12280 | `	}` |
|    20795 | 12281 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|    20795 | 12282 | `	pGen->pIn++;` |
|    20795 | 12283 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|        - | 12284 | `		/* Unexpected token,break immediately */` |
|      ! 0 | 12285 | `		pToken = pGen->pIn;` |
|      ! 0 | 12286 | `		if( pToken >= pGen->pEnd ){` |
|      ! 0 | 12287 | `			pToken--;` |
|      ! 0 | 12288 | `		}` |
|      ! 0 | 12289 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 12290 | `			"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 12291 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 12292 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12293 | `			return SXERR_ABORT;` |
|        - | 12294 | `		}` |
|      ! 0 | 12295 | `		return SXERR_INVALID;` |
|        - | 12296 | `	}` |
|        - | 12297 | `	/* Compile the block */` |
|    20795 | 12298 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|        - | 12299 | `	/* Create the catch block */` |
|    20795 | 12300 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|    20795 | 12301 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 12302 | `		return SXERR_ABORT;` |
|        - | 12303 | `	}` |
|        - | 12304 | `	/* Swap bytecode container */` |
|    20795 | 12305 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    20795 | 12306 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|        - | 12307 | `	/* Compile the block */` |
|    20795 | 12308 | `	PH7_CompileBlock(&(*pGen),0);` |
|        - | 12309 | `	/* Fix forward jumps now the destination is resolved  */` |
|    20795 | 12310 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 12311 | `	/* Emit the DONE instruction */` |
|    20795 | 12312 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 12313 | `	/* Leave the block */` |
|    20795 | 12314 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 12315 | `	/* Restore the default container */` |
|    20795 | 12316 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 12317 | `	/* Install the catch block */` |
|    20795 | 12318 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|    20795 | 12319 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 12320 | `		goto Mem;` |
|        - | 12321 | `	}` |
|    20795 | 12322 | `	return SXRET_OK;` |
|      ! 0 | 12323 | `Mem:` |
|      ! 0 | 12324 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 12325 | `	return SXERR_ABORT;` |
|    10402 | 12326 | `}` |
|        - | 12327 | `/*` |
|        - | 12328 | ` * Compile a 'try' block.` |
|        - | 12329 | ` * A function using an exception should be in a "try" block.` |
|        - | 12330 | ` * If the exception does not trigger, the code will continue` |
|        - | 12331 | ` * as normal. However if the exception triggers, an exception` |
|        - | 12332 | ` * is "thrown".` |
|        - | 12333 | ` */` |
|    20950 | 12334 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|        5 | 12335 | `{` |
|        - | 12336 | `	ph7_exception *pException;` |
|    20955 | 12337 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 12338 | `	GenBlock *pTry;` |
|        - | 12339 | `	sxu32 nJmpIdx;` |
|        - | 12340 | `	sxi32 rc;` |
|        - | 12341 | `	/* Create the exception container */` |
|    20955 | 12342 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|    20955 | 12343 | `	if( pException == 0 ){` |
|      ! 0 | 12344 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 12345 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 12346 | `		return SXERR_ABORT;` |
|        - | 12347 | `	}` |
|        - | 12348 | `	/* Zero the structure */` |
|    20955 | 12349 | `	SyZero(pException,sizeof(ph7_exception));` |
|        - | 12350 | `	/* Initialize fields */` |
|    20955 | 12351 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|    20955 | 12352 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|    20955 | 12353 | `	pException->iHasFinally = 0;` |
|    20955 | 12354 | `	pException->iFinallyDone = 0;` |
|    20955 | 12355 | `	pException->pVm = pGen->pVm;` |
|        - | 12356 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|        - | 12357 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|        - | 12358 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|        - | 12359 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|        - | 12360 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|        - | 12361 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|    20955 | 12362 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|      103 | 12363 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|        - | 12364 | `	}` |
|        - | 12365 | `	/* Create the try block */` |
|    20857 | 12366 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|    20857 | 12367 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 12368 | `		return SXERR_ABORT;` |
|        - | 12369 | `	}` |
|        - | 12370 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|    20857 | 12371 | `	pTry->pUserData = pException;` |
|        - | 12372 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|    20857 | 12373 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|        - | 12374 | `	/* Fix the jump later when the destination is resolved */` |
|    20857 | 12375 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|    20857 | 12376 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|        - | 12377 | `	/* Compile the block */` |
|    20857 | 12378 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|    20857 | 12379 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 12380 | `		return SXERR_ABORT;` |
|        - | 12381 | `	}` |
|        - | 12382 | `	/* Fix forward jumps now the destination is resolved */` |
|    20857 | 12383 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 12384 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|    20857 | 12385 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        - | 12386 | `	/* Leave the block */` |
|    20857 | 12387 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 12388 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|    20857 | 12389 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|    20850 | 12390 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        - | 12391 | `		/* Compile one or more catch blocks */` |
|    20790 | 12392 | `		for(;;){` |
|    41580 | 12393 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    31248 | 12394 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|    10398 | 12395 | `					break;` |
|        - | 12396 | `			}` |
|    20799 | 12397 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|    20799 | 12398 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 12399 | `				return SXERR_ABORT;` |
|        - | 12400 | `			}` |
|        5 | 12401 | `		}` |
|    10393 | 12402 | `	}` |
|        - | 12403 | `	/* Compile optional finally block */` |
|    20857 | 12404 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      708 | 12405 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 12406 | `		SySet *pInstrContainer;` |
|        - | 12407 | `		GenBlock *pFinBlock;` |
|      129 | 12408 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|        - | 12409 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      129 | 12410 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      129 | 12411 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12412 | `			return SXERR_ABORT;` |
|        - | 12413 | `		}` |
|        - | 12414 | `		/* Swap bytecode container */` |
|      129 | 12415 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      129 | 12416 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|        - | 12417 | `		/* Compile the finally body */` |
|      129 | 12418 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      129 | 12419 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12420 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 | 12421 | `			return SXERR_ABORT;` |
|        - | 12422 | `		}` |
|        - | 12423 | `		/* Fix forward jumps now the destination is resolved */` |
|      129 | 12424 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 12425 | `		/* Emit DONE to terminate the finally block */` |
|      129 | 12426 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 12427 | `		/* Leave the block */` |
|      129 | 12428 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        - | 12429 | `		/* Restore the default container */` |
|      129 | 12430 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      129 | 12431 | `		pException->iHasFinally = 1;` |
|       62 | 12432 | `	}` |
|        - | 12433 | `	/* Must have at least one catch or finally */` |
|    20857 | 12434 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|        9 | 12435 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 12436 | `			"Cannot use try without catch or finally");` |
|        9 | 12437 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12438 | `			return SXERR_ABORT;` |
|        - | 12439 | `		}` |
|        3 | 12440 | `	}` |
|    20857 | 12441 | `	return SXRET_OK;` |
|    10480 | 12442 | `}` |
|        - | 12443 | `/*` |
|        - | 12444 | ` * Compile a switch block.` |
|        - | 12445 | ` *  (See block-comment below for more information)` |
|        - | 12446 | ` */` |
|      112 | 12447 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|        5 | 12448 | `{` |
|      117 | 12449 | `	sxi32 rc = SXRET_OK;` |
|      117 | 12450 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|        - | 12451 | `		/* Unexpected token */` |
|      ! 0 | 12452 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 12453 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12454 | `			return SXERR_ABORT;` |
|        - | 12455 | `		}` |
|      ! 0 | 12456 | `		pGen->pIn++;` |
|      ! 0 | 12457 | `	}` |
|      117 | 12458 | `	pGen->pIn++;` |
|        - | 12459 | `	/* First instruction to execute in this block. */` |
|      117 | 12460 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|        - | 12461 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|        - | 12462 | `	 * or the '}' token */` |
|      206 | 12463 | `	for(;;){` |
|      417 | 12464 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 12465 | `			/* No more input to process */` |
|      ! 0 | 12466 | `			break;` |
|        - | 12467 | `		}` |
|      417 | 12468 | `		rc = SXRET_OK;` |
|      417 | 12469 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       85 | 12470 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|       31 | 12471 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|        - | 12472 | `					/* Unexpected token */` |
|      ! 0 | 12473 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 12474 | `						&pGen->pIn->sData);` |
|      ! 0 | 12475 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 12476 | `						return SXERR_ABORT;` |
|        - | 12477 | `					}` |
|        - | 12478 | `					/* FALL THROUGH */` |
|      ! 0 | 12479 | `				}` |
|       31 | 12480 | `				rc = SXERR_EOF;` |
|       31 | 12481 | `				break;` |
|        - | 12482 | `			}` |
|       32 | 12483 | `		}else{` |
|        - | 12484 | `			sxi32 nKwrd;` |
|        - | 12485 | `			/* Extract the keyword */` |
|      337 | 12486 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      337 | 12487 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|       47 | 12488 | `				break;` |
|        - | 12489 | `			}` |
|      253 | 12490 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 12491 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|        - | 12492 | `					/* Unexpected token */` |
|      ! 0 | 12493 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 12494 | `						&pGen->pIn->sData);` |
|      ! 0 | 12495 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 12496 | `						return SXERR_ABORT;` |
|        - | 12497 | `					}` |
|        - | 12498 | `					/* FALL THROUGH */` |
|      ! 0 | 12499 | `				}` |
|        - | 12500 | `				/* Block compiled */` |
|        3 | 12501 | `				break;` |
|        - | 12502 | `			}` |
|        - | 12503 | `		}` |
|        - | 12504 | `		/* Compile block */` |
|      305 | 12505 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      305 | 12506 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12507 | `			return SXERR_ABORT;` |
|        - | 12508 | `		}` |
|        5 | 12509 | `	}` |
|      117 | 12510 | `	return rc;` |
|       61 | 12511 | `}` |
|        - | 12512 | `/*` |
|        - | 12513 | ` * Compile a case eXpression.` |
|        - | 12514 | ` *  (See block-comment below for more information)` |
|        - | 12515 | ` */` |
|       92 | 12516 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|        5 | 12517 | `{` |
|        - | 12518 | `	SySet *pInstrContainer;` |
|        - | 12519 | `	SyToken *pEnd,*pTmp;` |
|       97 | 12520 | `	sxi32 iNest = 0;` |
|        - | 12521 | `	sxi32 rc;` |
|        - | 12522 | `	/* Delimit the expression */` |
|       97 | 12523 | `	pEnd = pGen->pIn;` |
|      197 | 12524 | `	while( pEnd < pGen->pEnd ){` |
|      197 | 12525 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|        - | 12526 | `			/* Increment nesting level */` |
|        3 | 12527 | `			iNest++;` |
|      196 | 12528 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|        - | 12529 | `			/* Decrement nesting level */` |
|        3 | 12530 | `			iNest--;` |
|      194 | 12531 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|       97 | 12532 | `			break;` |
|        - | 12533 | `		}` |
|      105 | 12534 | `		pEnd++;` |
|        5 | 12535 | `	}` |
|       97 | 12536 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 12537 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|      ! 0 | 12538 | `		if( rc == SXERR_ABORT ){` |
|        - | 12539 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 12540 | `			return SXERR_ABORT;` |
|        - | 12541 | `		}` |
|      ! 0 | 12542 | `	}` |
|        - | 12543 | `	/* Swap token stream */` |
|       97 | 12544 | `	pTmp = pGen->pEnd;` |
|       97 | 12545 | `	pGen->pEnd = pEnd;` |
|       97 | 12546 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       97 | 12547 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|       97 | 12548 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 12549 | `	/* Emit the done instruction */` |
|       97 | 12550 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       97 | 12551 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 12552 | `	/* Update token stream */` |
|       97 | 12553 | `	pGen->pIn  = pEnd;` |
|       97 | 12554 | `	pGen->pEnd = pTmp;` |
|       97 | 12555 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 12556 | `		return SXERR_ABORT;` |
|        - | 12557 | `	}` |
|       97 | 12558 | `	return SXRET_OK;` |
|       51 | 12559 | `}` |
|        - | 12560 | `/*` |
|        - | 12561 | ` * Compile the smart switch statement.` |
|        - | 12562 | ` * According to the PHP language reference manual` |
|        - | 12563 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|        - | 12564 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|        - | 12565 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|        - | 12566 | ` *  This is exactly what the switch statement is for.` |
|        - | 12567 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|        - | 12568 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|        - | 12569 | ` *  of the outer loop, use continue 2.` |
|        - | 12570 | ` *  Note that switch/case does loose comparision.` |
|        - | 12571 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|        - | 12572 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|        - | 12573 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|        - | 12574 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|        - | 12575 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|        - | 12576 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|        - | 12577 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|        - | 12578 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|        - | 12579 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|        - | 12580 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|        - | 12581 | ` *  list for the next case.` |
|        - | 12582 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|        - | 12583 | ` *  or floating-point numbers and strings.` |
|        - | 12584 | ` */` |
|       28 | 12585 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|        5 | 12586 | `{` |
|        - | 12587 | `	GenBlock *pSwitchBlock;` |
|        - | 12588 | `	SyToken *pTmp,*pEnd;` |
|        - | 12589 | `	ph7_switch *pSwitch;` |
|        - | 12590 | `	sxu32 nToken;` |
|        - | 12591 | `	sxu32 nLine;` |
|        - | 12592 | `	sxi32 rc;` |
|       33 | 12593 | `	nLine = pGen->pIn->nLine;` |
|        - | 12594 | `	/* Jump the 'switch' keyword */` |
|       33 | 12595 | `	pGen->pIn++;` |
|       33 | 12596 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 12597 | `		/* Syntax error */` |
|      ! 0 | 12598 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|      ! 0 | 12599 | `		if( rc == SXERR_ABORT ){` |
|        - | 12600 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 12601 | `			return SXERR_ABORT;` |
|        - | 12602 | `		}` |
|      ! 0 | 12603 | `		goto Synchronize;` |
|        - | 12604 | `	}` |
|        - | 12605 | `	/* Jump the left parenthesis '(' */` |
|       33 | 12606 | `	pGen->pIn++;` |
|       33 | 12607 | `	pEnd = 0; /* cc warning */` |
|        - | 12608 | `	/* Create the loop block */` |
|       47 | 12609 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|       14 | 12610 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|       33 | 12611 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 12612 | `		return SXERR_ABORT;` |
|        - | 12613 | `	}` |
|        - | 12614 | `	/* Delimit the condition */` |
|       33 | 12615 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|       33 | 12616 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 12617 | `		/* Empty expression */` |
|      ! 0 | 12618 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|      ! 0 | 12619 | `		if( rc == SXERR_ABORT ){` |
|        - | 12620 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 12621 | `			return SXERR_ABORT;` |
|        - | 12622 | `		}` |
|      ! 0 | 12623 | `	}` |
|        - | 12624 | `	/* Swap token streams */` |
|       33 | 12625 | `	pTmp = pGen->pEnd;` |
|       33 | 12626 | `	pGen->pEnd = pEnd;` |
|        - | 12627 | `	/* Compile the expression */` |
|       33 | 12628 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       33 | 12629 | `	if( rc == SXERR_ABORT ){` |
|        - | 12630 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 12631 | `		return SXERR_ABORT;` |
|        - | 12632 | `	}` |
|        - | 12633 | `	/* Update token stream */` |
|       33 | 12634 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 | 12635 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 12636 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 12637 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12638 | `			return SXERR_ABORT;` |
|        - | 12639 | `		}` |
|      ! 0 | 12640 | `		pGen->pIn++;` |
|      ! 0 | 12641 | `	}` |
|       33 | 12642 | `	pGen->pIn  = &pEnd[1];` |
|       33 | 12643 | `	pGen->pEnd = pTmp;` |
|       33 | 12644 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       28 | 12645 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|      ! 0 | 12646 | `			pTmp = pGen->pIn;` |
|      ! 0 | 12647 | `			if( pTmp >= pGen->pEnd ){` |
|      ! 0 | 12648 | `				pTmp--;` |
|      ! 0 | 12649 | `			}` |
|        - | 12650 | `			/* Unexpected token */` |
|      ! 0 | 12651 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|      ! 0 | 12652 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 12653 | `				return SXERR_ABORT;` |
|        - | 12654 | `			}` |
|      ! 0 | 12655 | `			goto Synchronize;` |
|        - | 12656 | `	}` |
|        - | 12657 | `	/* Set the delimiter token */` |
|       33 | 12658 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|        3 | 12659 | `		nToken = PH7_TK_KEYWORD;` |
|        - | 12660 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|        2 | 12661 | `	}else{` |
|       31 | 12662 | `		nToken = PH7_TK_CCB; /* '}' */` |
|        - | 12663 | `	}` |
|       33 | 12664 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|        - | 12665 | `	/* Create the switch blocks container */` |
|       33 | 12666 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|       33 | 12667 | `	if( pSwitch == 0 ){` |
|        - | 12668 | `		/* Abort compilation */` |
|      ! 0 | 12669 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 12670 | `		return SXERR_ABORT;` |
|        - | 12671 | `	}` |
|        - | 12672 | `	/* Zero the structure */` |
|       33 | 12673 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|        - | 12674 | `	/* Initialize fields */` |
|       33 | 12675 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|        - | 12676 | `	/* Emit the switch instruction */` |
|       33 | 12677 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|        - | 12678 | `	/* Compile case blocks */` |
|      100 | 12679 | `	for(;;){` |
|        - | 12680 | `		sxu32 nKwrd;` |
|      119 | 12681 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 12682 | `			/* No more input to process */` |
|      ! 0 | 12683 | `			break;` |
|        - | 12684 | `		}` |
|      119 | 12685 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 12686 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|        - | 12687 | `				/* Unexpected token */` |
|      ! 0 | 12688 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 12689 | `					&pGen->pIn->sData);` |
|      ! 0 | 12690 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 12691 | `					return SXERR_ABORT;` |
|        - | 12692 | `				}` |
|        - | 12693 | `				/* FALL THROUGH */` |
|      ! 0 | 12694 | `			}` |
|        - | 12695 | `			/* Block compiled */` |
|      ! 0 | 12696 | `			break;` |
|        - | 12697 | `		}` |
|        - | 12698 | `		/* Extract the keyword */` |
|      119 | 12699 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      119 | 12700 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 12701 | `			if( nToken != PH7_TK_KEYWORD ){` |
|        - | 12702 | `				/* Unexpected token */` |
|      ! 0 | 12703 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 12704 | `					&pGen->pIn->sData);` |
|      ! 0 | 12705 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 12706 | `					return SXERR_ABORT;` |
|        - | 12707 | `				}` |
|        - | 12708 | `				/* FALL THROUGH */` |
|      ! 0 | 12709 | `			}` |
|        - | 12710 | `			/* Block compiled */` |
|        3 | 12711 | `			break;` |
|        - | 12712 | `		}` |
|      117 | 12713 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|        - | 12714 | `			/*` |
|        - | 12715 | `			 * Accroding to the PHP language reference manual` |
|        - | 12716 | `			 *  A special case is the default case. This case matches anything` |
|        - | 12717 | `			 *  that wasn't matched by the other cases.` |
|        - | 12718 | `			 */` |
|       25 | 12719 | `			if( pSwitch->nDefault > 0 ){` |
|        - | 12720 | `				/* Default case already compiled */` |
|      ! 0 | 12721 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|      ! 0 | 12722 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 12723 | `					return SXERR_ABORT;` |
|        - | 12724 | `				}` |
|      ! 0 | 12725 | `			}` |
|       25 | 12726 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|        - | 12727 | `			/* Compile the default block */` |
|       25 | 12728 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|       25 | 12729 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 12730 | `				return SXERR_ABORT;` |
|       25 | 12731 | `			}else if( rc == SXERR_EOF ){` |
|       23 | 12732 | `				break;` |
|        1 | 12733 | `			}` |
|       98 | 12734 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|        - | 12735 | `			ph7_case_expr sCase;` |
|        - | 12736 | `			/* Standard case block */` |
|       97 | 12737 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|        - | 12738 | `			/* initialize the structure */` |
|       97 | 12739 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - | 12740 | `			/* Compile the case expression */` |
|       97 | 12741 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|       97 | 12742 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 12743 | `				return SXERR_ABORT;` |
|        - | 12744 | `			}` |
|        - | 12745 | `			/* Compile the case block */` |
|       97 | 12746 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|        - | 12747 | `			/* Insert in the switch container */` |
|       97 | 12748 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|       97 | 12749 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 12750 | `				return SXERR_ABORT;` |
|       97 | 12751 | `			}else if( rc == SXERR_EOF ){` |
|        9 | 12752 | `				break;` |
|        - | 12753 | `			}` |
|       47 | 12754 | `		}else{` |
|        - | 12755 | `			/* Unexpected token */` |
|      ! 0 | 12756 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 12757 | `				&pGen->pIn->sData);` |
|      ! 0 | 12758 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 12759 | `				return SXERR_ABORT;` |
|        - | 12760 | `			}` |
|      ! 0 | 12761 | `			break;` |
|        - | 12762 | `		}` |
|        5 | 12763 | `	}` |
|        - | 12764 | `	/* Fix all jumps now the destination is resolved */` |
|       33 | 12765 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|       33 | 12766 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 12767 | `	/* Release the loop block */` |
|       33 | 12768 | `	GenStateLeaveBlock(pGen,0);` |
|       33 | 12769 | `	if( pGen->pIn < pGen->pEnd ){` |
|        - | 12770 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|       33 | 12771 | `		pGen->pIn++;` |
|       14 | 12772 | `	}` |
|        - | 12773 | `	/* Statement successfully compiled */` |
|       33 | 12774 | `	return SXRET_OK;` |
|      ! 0 | 12775 | `Synchronize:` |
|        - | 12776 | `	/* Synchronize with the first semi-colon */` |
|      ! 0 | 12777 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      ! 0 | 12778 | `		pGen->pIn++;` |
|      ! 0 | 12779 | `	}` |
|      ! 0 | 12780 | `	return SXRET_OK;` |
|       19 | 12781 | `}` |
|        - | 12782 | `/*` |
|        - | 12783 | ` * Chain operators participate in a postfix member-access chain.` |
|        - | 12784 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|        - | 12785 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|        - | 12786 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|        - | 12787 | ` */` |
|        - | 12788 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|        - | 12789 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|        - | 12790 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|        - | 12791 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|        - | 12792 |  |
|        - | 12793 | `/*` |
|        - | 12794 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|        - | 12795 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|        - | 12796 | ` * patched entries from the pending set.` |
|        - | 12797 | ` */` |
| 31473750 | 12798 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|        5 | 12799 | `{` |
| 31473755 | 12800 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|        - | 12801 | `	sxu32 nTarget;` |
|        - | 12802 | `	sxu32 *aIdx;` |
|        - | 12803 | `	sxu32 i;` |
| 31473755 | 12804 | `	if( nCur <= nBaseline ){` |
| 31473659 | 12805 | `		return;` |
|        - | 12806 | `	}` |
|      100 | 12807 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      100 | 12808 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|      204 | 12809 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|      108 | 12810 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|      108 | 12811 | `		if( pInstr ){` |
|      108 | 12812 | `			pInstr->iP2 = (sxi32)nTarget;` |
|       52 | 12813 | `		}` |
|       56 | 12814 | `	}` |
|      100 | 12815 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 15736880 | 12816 | `}` |
|        - | 12817 |  |
|        - | 12818 | `/*` |
|        - | 12819 | ` * By-reference out-parameters of builtin functions.` |
|        - | 12820 | ` *` |
|        - | 12821 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|        - | 12822 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|        - | 12823 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|        - | 12824 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|        - | 12825 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|        - | 12826 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|        - | 12827 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|        - | 12828 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|        - | 12829 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|        - | 12830 | ` * creates it" behaviour).` |
|        - | 12831 | ` *` |
|        - | 12832 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|        - | 12833 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|        - | 12834 | ` */` |
|  4272414 | 12835 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|        5 | 12836 | `{` |
|        - | 12837 | `	static const struct {` |
|        - | 12838 | `		const char *zName;` |
|        - | 12839 | `		sxu32 nByte;` |
|        - | 12840 | `		sxu32 mask;` |
|        - | 12841 | `	} aByRef[] = {` |
|        - | 12842 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|        - | 12843 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|        - | 12844 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|        - | 12845 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|        - | 12846 | `		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */` |
|        - | 12847 | `	};` |
|        - | 12848 | `	sxu32 i;` |
|  4272419 | 12849 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|  1353215 | 12850 | `		return 0;` |
|        - | 12851 | `	}` |
| 17371423 | 12852 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 14483344 | 12853 | `		if( pName->nByte == aByRef[i].nByte` |
|  7462501 | 12854 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|    31135 | 12855 | `			return aByRef[i].mask;` |
|        - | 12856 | `		}` |
|  7226112 | 12857 | `	}` |
|  2888079 | 12858 | `	return 0;` |
|  2136212 | 12859 | `}` |
|        - | 12860 | `/*` |
|        - | 12861 | ` * Recover the bare global-builtin name from a call's callee node.` |
|        - | 12862 | ` *` |
|        - | 12863 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|        - | 12864 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|        - | 12865 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|        - | 12866 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|        - | 12867 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|        - | 12868 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|        - | 12869 | ` */` |
|  4272414 | 12870 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|        5 | 12871 | `{` |
|        - | 12872 | `	SyToken *p, *pEnd;` |
|  4272419 | 12873 | `	pOut->zString = 0;` |
|  4272419 | 12874 | `	pOut->nByte = 0;` |
|  4272419 | 12875 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|      ! 0 | 12876 | `		return;` |
|        - | 12877 | `	}` |
|  4272419 | 12878 | `	p = pLeft->pStart;` |
|  4272419 | 12879 | `	pEnd = pLeft->pEnd;` |
|        - | 12880 | `	/* Optional single leading namespace separator (absolute path). */` |
|  4272419 | 12881 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|     3909 | 12882 | `		p++;` |
|     1952 | 12883 | `	}` |
|  4272419 | 12884 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|  1353179 | 12885 | `		return;` |
|        - | 12886 | `	}` |
|        - | 12887 | `	/* Must be a single component: nothing follows the name token. */` |
|  2919245 | 12888 | `	if( p + 1 != pEnd ){` |
|       41 | 12889 | `		return;` |
|        - | 12890 | `	}` |
|  2919209 | 12891 | `	*pOut = p->sData;` |
|  2136212 | 12892 | `}` |
|        - | 12893 | `/*` |
|        - | 12894 | ` * Generate bytecode for a given expression tree.` |
|        - | 12895 | ` * If something goes wrong while generating bytecode` |
|        - | 12896 | ` * for the expression tree (A very unlikely scenario)` |
|        - | 12897 | ` * this function takes care of generating the appropriate` |
|        - | 12898 | ` * error message.` |
|        - | 12899 | ` */` |
| 44956626 | 12900 | `static sxi32 GenStateEmitExprCode(` |
|        - | 12901 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - | 12902 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|        - | 12903 | `	sxi32 iFlags /* Control flags */` |
|        - | 12904 | `	)` |
|        5 | 12905 | `{` |
|        - | 12906 | `	VmInstr *pInstr;` |
|        - | 12907 | `	sxu32 nJmpIdx;` |
| 44956631 | 12908 | `	sxi32 iP1 = 0;` |
| 44956631 | 12909 | `	sxu32 iP2 = 0;` |
| 44956631 | 12910 | `	void *p3  = 0;` |
|        - | 12911 | `	sxi32 iVmOp;` |
|        - | 12912 | `	sxi32 rc;` |
| 44956631 | 12913 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 44956631 | 12914 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
| 44956631 | 12915 | `	sxu32 nRhsNsBase = 0;` |
| 44956631 | 12916 | `	if( pNode->xCode ){` |
|        - | 12917 | `		SyToken *pTmpIn,*pTmpEnd;` |
|        - | 12918 | `		/* Compile node */` |
| 26838133 | 12919 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 26838133 | 12920 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 26838133 | 12921 | `		RE_SWAP_DELIMITER(pGen);` |
| 26838133 | 12922 | `		return rc;` |
|        - | 12923 | `	}` |
| 18118503 | 12924 | `	if( pNode->pOp == 0 ){` |
|      ! 0 | 12925 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 12926 | `			"Invalid expression node,PH7 is aborting compilation");` |
|      ! 0 | 12927 | `		return SXERR_ABORT;` |
|        - | 12928 | `	}` |
| 18118503 | 12929 | `	iVmOp = pNode->pOp->iVmOp;` |
| 18118503 | 12930 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|        - | 12931 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|        - | 12932 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|        - | 12933 | `		 * and later errors are still reported. */` |
|        3 | 12934 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 12935 | `			"The (unset) cast is no longer supported");` |
|        3 | 12936 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12937 | `			return SXERR_ABORT;` |
|        - | 12938 | `		}` |
|        1 | 12939 | `	}` |
| 18118503 | 12940 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|       91 | 12941 | `		sxu32 nJmp = 0;` |
|        - | 12942 | `		sxu32 nNcNsBase;` |
|        - | 12943 | `		VmInstr *pInstrFix;` |
|        - | 12944 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|        - | 12945 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|        - | 12946 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|        - | 12947 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|        - | 12948 | `		 * stack slot carries a writable nIdx. */` |
|       91 | 12949 | `		if( pNode->pRight ){` |
|       91 | 12950 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       91 | 12951 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|       91 | 12952 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 12953 | `				return rc;` |
|        - | 12954 | `			}` |
|       91 | 12955 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        - | 12956 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|        - | 12957 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|        - | 12958 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|        - | 12959 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|        - | 12960 | `			 * the store, so the parent array does not need to be copied at` |
|        - | 12961 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|        - | 12962 | `			 * cascade for the actual write path stays correct. */` |
|       91 | 12963 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|       91 | 12964 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|       33 | 12965 | `				pInstrFix->iP2 = 3;` |
|       15 | 12966 | `			}` |
|       44 | 12967 | `		}` |
|        - | 12968 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|       91 | 12969 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|        - | 12970 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|       91 | 12971 | `		if( pNode->pLeft ){` |
|       91 | 12972 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       91 | 12973 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|       91 | 12974 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 12975 | `				return rc;` |
|        - | 12976 | `			}` |
|       91 | 12977 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       44 | 12978 | `		}` |
|        - | 12979 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|       91 | 12980 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|        - | 12981 | `		/* Patch the short-circuit jump to land after the store. */` |
|       91 | 12982 | `		if( nJmp > 0 ){` |
|       91 | 12983 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|       91 | 12984 | `			if( pInstrFix ){` |
|       91 | 12985 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|       44 | 12986 | `			}` |
|       44 | 12987 | `		}` |
|       91 | 12988 | `		return SXRET_OK;` |
|        - | 12989 | `	}` |
| 18118415 | 12990 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|        - | 12991 | `		sxu32 nJz,nJmp;` |
|        - | 12992 | `		sxu32 nTernaryNsBase;` |
|        - | 12993 | `		/* Ternary operator require special handling */` |
|        - | 12994 | `		/* Phase#1: Compile the condition */` |
|   286199 | 12995 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   286199 | 12996 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|   286199 | 12997 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12998 | `			return rc;` |
|        - | 12999 | `		}` |
|        - | 13000 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|        - | 13001 | `		 * compiling the condition must short-circuit to the end of the` |
|        - | 13002 | `		 * condition expression, not leak past the ternary. */` |
|   286199 | 13003 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|   286199 | 13004 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|   286199 | 13005 | `		if( pNode->pLeft ){` |
|        - | 13006 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|        - | 13007 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|   286131 | 13008 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|        - | 13009 | `			/* Phase#3: Compile the 'then' expression  */` |
|   286131 | 13010 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   286131 | 13011 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|   286131 | 13012 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 13013 | `				return rc;` |
|        - | 13014 | `			}` |
|   286131 | 13015 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|   143068 | 13016 | `		}else{` |
|        - | 13017 | `			/* Elvis operator: (expr) ?: (else)` |
|        - | 13018 | `			 * Duplicate condition so original value is the 'then' result.` |
|        - | 13019 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|       70 | 13020 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       70 | 13021 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|        - | 13022 | `		}` |
|        - | 13023 | `		/* Phase#4: Emit the unconditional jump */` |
|   286199 | 13024 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|        - | 13025 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|   286199 | 13026 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|   286199 | 13027 | `		if( pInstr ){` |
|   286199 | 13028 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|   143097 | 13029 | `		}` |
|   286199 | 13030 | `		if( !pNode->pLeft ){` |
|        - | 13031 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|       70 | 13032 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       34 | 13033 | `		}` |
|        - | 13034 | `		/* Phase#6: Compile the 'else' expression */` |
|   286199 | 13035 | `		if( pNode->pRight ){` |
|   286199 | 13036 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   286199 | 13037 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|   286199 | 13038 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 13039 | `				return rc;` |
|        - | 13040 | `			}` |
|   286199 | 13041 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|   143097 | 13042 | `		}` |
|   286199 | 13043 | `		if( nJmp > 0 ){` |
|        - | 13044 | `			/* Phase#7: Fix the unconditional jump */` |
|   286199 | 13045 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|   286199 | 13046 | `			if( pInstr ){` |
|   286199 | 13047 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|   143097 | 13048 | `			}` |
|   143097 | 13049 | `		}` |
|        - | 13050 | `		/* All done */` |
|   286199 | 13051 | `		return SXRET_OK;` |
|        - | 13052 | `	}` |
| 17832221 | 13053 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|        - | 13054 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|        - | 13055 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|        - | 13056 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|        - | 13057 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|        - | 13058 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|        - | 13059 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|        - | 13060 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|        - | 13061 | `		sxu32 nPipeNsBase;` |
|       27 | 13062 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|       27 | 13063 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|      ! 0 | 13064 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 13065 | `				"'\|>': Missing operand");` |
|      ! 0 | 13066 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 13067 | `		}` |
|        - | 13068 | `		/* Argument: the LHS value. */` |
|       27 | 13069 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       27 | 13070 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|       27 | 13071 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 13072 | `			return rc;` |
|        - | 13073 | `		}` |
|       27 | 13074 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|        - | 13075 | `		/* Callable: the RHS. */` |
|       27 | 13076 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       27 | 13077 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|       27 | 13078 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 13079 | `			return rc;` |
|        - | 13080 | `		}` |
|       27 | 13081 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|        - | 13082 | `		/* Invoke the callable with the single piped argument. */` |
|       27 | 13083 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       27 | 13084 | `		return SXRET_OK;` |
|        - | 13085 | `	}` |
| 17832195 | 13086 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|        - | 13087 | `	/* Generate code for the left tree */` |
| 17832195 | 13088 | `	if( pNode->pLeft ){` |
| 17808937 | 13089 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 17808937 | 13090 | `		if( iVmOp == PH7_OP_CALL ){` |
|        - | 13091 | `			ph7_expr_node **apNode;` |
|  4276611 | 13092 | `			int hasSpread = 0;` |
|  4276611 | 13093 | `			int hasNamed = 0;` |
|  4276611 | 13094 | `			int bAnySpread = 0;` |
|  4276611 | 13095 | `			sxu32 byRefMask = 0;` |
|        - | 13096 | `			sxi32 nArgs;` |
|        - | 13097 | `			sxi32 n;` |
|        - | 13098 | `			/* Recurse and generate bytecodes for function arguments */` |
|  4276611 | 13099 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  4276611 | 13100 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|        - | 13101 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|        - | 13102 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|        - | 13103 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|  4276611 | 13104 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|       81 | 13105 | `				bFcc = 1;` |
|       81 | 13106 | `				nArgs = 0;` |
|       40 | 13107 | `			}` |
|        - | 13108 | `			/* Validate argument order like php: no positional argument after a` |
|        - | 13109 | ``			 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|        - | 13110 | `			{` |
|  4276611 | 13111 | `				int seenNamed = 0;` |
|  4276611 | 13112 | `				int seenSpread = 0;` |
|  8760801 | 13113 | `				for( n = 0; n < nArgs; ++n ){` |
|  4484197 | 13114 | `					if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|     4065 | 13115 | `						bAnySpread = 1;` |
|     4065 | 13116 | `						seenSpread = 1;` |
|     4065 | 13117 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      ! 0 | 13118 | `							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 13119 | `								"syntax error, unexpected token \"...\"");` |
|      ! 0 | 13120 | `							return SXERR_SYNTAX;` |
|        5 | 13121 | `						}` |
|  4482167 | 13122 | `					}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      289 | 13123 | `						seenNamed = 1;` |
|      289 | 13124 | `						hasNamed = 1;` |
|  4479995 | 13125 | `					}else if( seenNamed ){` |
|        3 | 13126 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 13127 | `							"Cannot use positional argument after named argument");` |
|        3 | 13128 | `						return SXERR_SYNTAX;` |
|  4479851 | 13129 | `					}else if( seenSpread ){` |
|      ! 0 | 13130 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 13131 | `							"Cannot use positional argument after argument unpacking");` |
|      ! 0 | 13132 | `						return SXERR_SYNTAX;` |
|        - | 13133 | `					}` |
|  2242100 | 13134 | `				}` |
|        - | 13135 | `			}` |
|        - | 13136 | `			/* Read-only load */` |
|  4276609 | 13137 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|        - | 13138 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|        - | 13139 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|        - | 13140 | `			 * objects dispatch to the right method (offsetExists for both;` |
|        - | 13141 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|  4276609 | 13142 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|  4276609 | 13143 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  4276604 | 13144 | `				if( pCallName->nByte == 5` |
|  2354687 | 13145 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|   221299 | 13146 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|  4165962 | 13147 | `				}else if( pCallName->nByte == 5` |
|  2133393 | 13148 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|      107 | 13149 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|       51 | 13150 | `				}` |
|        - | 13151 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|        - | 13152 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|        - | 13153 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|        - | 13154 | `				 * write back through. Skipped when spread/named args are present:` |
|        - | 13155 | `				 * the compile-time positional index no longer maps to the` |
|        - | 13156 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|  4276609 | 13157 | `				if( !bAnySpread && !hasNamed ){` |
|        - | 13158 | `					SyString sBuiltin;` |
|  4272419 | 13159 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|  4272419 | 13160 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|  2136207 | 13161 | `				}` |
|  2138302 | 13162 | `			}` |
|  8760797 | 13163 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  4484193 | 13164 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  4484193 | 13165 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|        - | 13166 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|        - | 13167 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|        - | 13168 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|        - | 13169 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|        - | 13170 | `				 * builtin to write back through. A plain $var target is unaffected` |
|        - | 13171 | `				 * (iP1=0 either way). */` |
|  4484193 | 13172 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|    27193 | 13173 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|    27193 | 13174 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|    13594 | 13175 | `				}` |
|  4484193 | 13176 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  4484193 | 13177 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 13178 | `					return rc;` |
|        - | 13179 | `				}` |
|        - | 13180 | `				/* Each argument is an independent nullsafe scope. */` |
|  4484193 | 13181 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  4484193 | 13182 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|        - | 13183 | `					/* Emit spread opcode to unpack this array argument */` |
|     4065 | 13184 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|     4065 | 13185 | `					hasSpread = 1;` |
|     2030 | 13186 | `				}` |
|  2242099 | 13187 | `			}` |
|        - | 13188 | `			/* Total number of given arguments */` |
|  4276609 | 13189 | `			iP1 = nArgs;` |
|  4276609 | 13190 | `			iP2 = hasSpread;` |
|        - | 13191 | `			/* Build VmCallArgMap if named arguments are present.` |
|        - | 13192 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  4276609 | 13193 | `			if( hasNamed ){` |
|      178 | 13194 | `				sxu32 nStrBytes = 0;` |
|        - | 13195 | `				char *zBuf;` |
|      534 | 13196 | `				for( n = 0; n < nArgs; ++n ){` |
|      360 | 13197 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      286 | 13198 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|      141 | 13199 | `					}` |
|      182 | 13200 | `				}` |
|        - | 13201 | `				{` |
|      178 | 13202 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|      178 | 13203 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      174 | 13204 | `					&pGen->pVm->sAllocator, mapSize);` |
|      178 | 13205 | `				if( pMap ){` |
|      178 | 13206 | `					SyZero(pMap, mapSize);` |
|      178 | 13207 | `					pMap->bHasNamed = 1;` |
|      178 | 13208 | `					pMap->nTotal = (sxu32)nArgs;` |
|      178 | 13209 | `					pMap->aNames = (SyString *)&pMap[1];` |
|      178 | 13210 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|      534 | 13211 | `					for( n = 0; n < nArgs; ++n ){` |
|      360 | 13212 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      286 | 13213 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|      286 | 13214 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|      286 | 13215 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|      286 | 13216 | `							zBuf += nb;` |
|      141 | 13217 | `						}` |
|        - | 13218 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|      182 | 13219 | `					}` |
|      178 | 13220 | `					p3 = (void *)pMap;` |
|       87 | 13221 | `				}` |
|        - | 13222 | `				}` |
|       87 | 13223 | `			}` |
|        - | 13224 | `			/* Remove stale flags now */` |
|  4276609 | 13225 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  2138302 | 13226 | `		}` |
|        - | 13227 | `		{` |
|        - | 13228 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|        - | 13229 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|        - | 13230 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|        - | 13231 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|        - | 13232 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|        - | 13233 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|        - | 13234 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|        - | 13235 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
| 17808935 | 13236 | `			sxi32 iLeftFlags = iFlags;` |
| 17808930 | 13237 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
| 14763421 | 13238 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|  5858982 | 13239 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  5044985 | 13240 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|  1686613 | 13241 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   843304 | 13242 | `			}` |
|        - | 13243 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|        - | 13244 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|        - | 13245 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|        - | 13246 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|        - | 13247 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|        - | 13248 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|        - | 13249 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
| 17808930 | 13250 | `			if( pNode->pOp` |
| 24970149 | 13251 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
| 16065731 | 13252 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
| 14322480 | 13253 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|  3611359 | 13254 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|  1805677 | 13255 | `			}` |
|        - | 13256 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|        - | 13257 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|        - | 13258 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|        - | 13259 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|        - | 13260 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|        - | 13261 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
| 17808930 | 13262 | `			if( pNode->pOp` |
| 17808935 | 13263 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|   116771 | 13264 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|    58383 | 13265 | `			}` |
| 17808935 | 13266 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|        - | 13267 | `		}` |
| 17808935 | 13268 | `		if( rc != SXRET_OK ){` |
|       34 | 13269 | `			return rc;` |
|        - | 13270 | `		}` |
| 17808905 | 13271 | `		if( !bIsChainOp ){` |
|        - | 13272 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|        - | 13273 | `			 * target the end of that LHS chain, which is right here. */` |
|  7960613 | 13274 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  3980304 | 13275 | `		}` |
| 17808905 | 13276 | `		if( iVmOp == PH7_OP_CALL ){` |
|  4276609 | 13277 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  4276609 | 13278 | `			if( pInstr ){` |
|  4276609 | 13279 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  2919485 | 13280 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|        - | 13281 | `					sxu32 nQual;` |
|  2919485 | 13282 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|        - | 13283 | `					/* Prevent constant expansion but preserve the absolute flag` |
|        - | 13284 | `					 * so the later NEW handler (if any) can see it. */` |
|  2919485 | 13285 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|        - | 13286 | `					/* Namespace-qualify the function name for CALL, unless the` |
|        - | 13287 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|        - | 13288 | `					 * imports — class imports must NOT affect function` |
|        - | 13289 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|        - | 13290 | `					 * before NEW; we store the original literal index in the` |
|        - | 13291 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|        - | 13292 | `					 * the unqualified name and re-qualify with class imports. */` |
|  2919485 | 13293 | `					if( bAbsolute ){` |
|     3909 | 13294 | `						pInstr->iP2 = (sxi32)nOrig;` |
|     1957 | 13295 | `					}else{` |
|  2915581 | 13296 | `						int fromImport = 0;` |
|  2915581 | 13297 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  2915581 | 13298 | `						pInstr->iP2 = (sxi32)nQual;` |
|  2915581 | 13299 | `						if( nQual != nOrig ){` |
|        - | 13300 | `							/* Record the original literal index in the arg map` |
|        - | 13301 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|        - | 13302 | `							 * flag) so the NEW handler can recover the` |
|        - | 13303 | `							 * unqualified name and re-qualify with CLASS` |
|        - | 13304 | `							 * imports. */` |
|       77 | 13305 | `							if( p3 == 0 ){` |
|       77 | 13306 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       72 | 13307 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|       77 | 13308 | `								if( pMap ){` |
|       77 | 13309 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|       77 | 13310 | `									p3 = (void *)pMap;` |
|       36 | 13311 | `								}` |
|       36 | 13312 | `							}` |
|       77 | 13313 | `							if( p3 ){` |
|       77 | 13314 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|       77 | 13315 | `								if( !fromImport ){` |
|        - | 13316 | `									/* Mark as namespace-qualified */` |
|       67 | 13317 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|       31 | 13318 | `								}` |
|       36 | 13319 | `							}` |
|       36 | 13320 | `						}` |
|        5 | 13321 | `					}` |
|  2816869 | 13322 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|        - | 13323 | `					/* Method call,flag that */` |
|  1341013 | 13324 | `					pInstr->iP2 = 1;` |
|   670504 | 13325 | `				}` |
|  2138307 | 13326 | `			}` |
| 15670603 | 13327 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|        - | 13328 | `			ph7_expr_node **apNode;` |
|        - | 13329 | `			sxi32 n;` |
|  1960339 | 13330 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|        - | 13331 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|        - | 13332 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|        - | 13333 | `			/* Recurse and generate bytecodes for array index */` |
|  1960339 | 13334 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  3780991 | 13335 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|  1820657 | 13336 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  1820657 | 13337 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|  1820657 | 13338 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 13339 | `					return rc;` |
|        - | 13340 | `				}` |
|        - | 13341 | `				/* Each subscript index is an independent nullsafe scope. */` |
|  1820657 | 13342 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   910331 | 13343 | `			}` |
|  1960339 | 13344 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|  1820657 | 13345 | `				iP1 = 1; /* Node have an index associated with it */` |
|   910326 | 13346 | `			}` |
|  1960339 | 13347 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|        - | 13348 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|   259945 | 13349 | `				iP2 = 4;` |
|  1830369 | 13350 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        - | 13351 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|        - | 13352 | `				 * so the trailing unset() builtin can drop the slot. */` |
|     7829 | 13353 | `				iP2 = 5;` |
|  1696487 | 13354 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        - | 13355 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|        - | 13356 | `				 * short-circuit on missing keys without invoking offsetGet` |
|        - | 13357 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|       29 | 13358 | `				iP2 = 6;` |
|  1692563 | 13359 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|        - | 13360 | `				/* Create an empty entry when the desired index is not found */` |
|   225437 | 13361 | `				iP2 = 1;` |
|   112721 | 13362 | `			}` |
| 12552134 | 13363 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|        - | 13364 | `			/* POP the left node */` |
|       32 | 13365 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       15 | 13366 | `		}` |
|  8904450 | 13367 | `	}` |
| 17832163 | 13368 | `	rc = SXRET_OK;` |
| 17832163 | 13369 | `	nJmpIdx = 0;` |
|        - | 13370 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|        - | 13371 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|        - | 13372 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 17832163 | 13373 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|   148115 | 13374 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   148115 | 13375 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   148115 | 13376 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|   148115 | 13377 | `			int isSpecial = 0;` |
|   148115 | 13378 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|   101571 | 13379 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|   101571 | 13380 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|   101566 | 13381 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    70502 | 13382 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    35253 | 13383 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|    77665 | 13384 | `					isSpecial = 1;` |
|    38830 | 13385 | `				}` |
|    62419 | 13386 | `			}` |
|   171387 | 13387 | `			pInstr->iP1 = 0;` |
|   171387 | 13388 | `			if( !isSpecial ){` |
|    47183 | 13389 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    23589 | 13390 | `			}` |
|        - | 13391 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|        - | 13392 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|   124843 | 13393 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    47183 | 13394 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    47183 | 13395 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|       60 | 13396 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|       62 | 13397 | `					return SXRET_OK;` |
|        - | 13398 | `				}` |
|    23560 | 13399 | `			}` |
|    62390 | 13400 | `		}` |
|   108913 | 13401 | `	}` |
|        - | 13402 | `	/* Generate code for the right tree */` |
| 17808847 | 13403 | `	if( pNode->pRight ){` |
| 10167469 | 13404 | `		if( iVmOp == PH7_OP_LAND ){` |
|        - | 13405 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|   198205 | 13406 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
| 10068369 | 13407 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|        - | 13408 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|   162975 | 13409 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  9887784 | 13410 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|        - | 13411 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|    31159 | 13412 | `			iVmOp = 0; /* No binary operator to emit */` |
|    31159 | 13413 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  9790774 | 13414 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|        - | 13415 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|        - | 13416 | `			 * the entire containing postfix chain to null. The jump target is` |
|        - | 13417 | `			 * patched later by the innermost non-chain ancestor (or by` |
|        - | 13418 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|        - | 13419 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|        - | 13420 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|      108 | 13421 | `			sxu32 nNsJmp = 0;` |
|      108 | 13422 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|      108 | 13423 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  9775093 | 13424 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|        - | 13425 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|        - | 13426 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|        - | 13427 | `			 * auto-created — PHP auto-vivifies on write. */` |
|  3274951 | 13428 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|  1637473 | 13429 | `		}` |
| 10167469 | 13430 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 10167469 | 13431 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
| 10167469 | 13432 | `		if( !bIsChainOp ){` |
|        - | 13433 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|        - | 13434 | `			 * operator instruction is emitted. */` |
|  6556173 | 13435 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  3278084 | 13436 | `		}` |
| 10167469 | 13437 | `		if( iVmOp == PH7_OP_STORE ){` |
|  2871527 | 13438 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  2871492 | 13439 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|        - | 13440 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|        - | 13441 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|        - | 13442 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|        - | 13443 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|        - | 13444 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|        - | 13445 | `				 */` |
|       89 | 13446 | `				iVmOp = 0;` |
|  2871485 | 13447 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  2871443 | 13448 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|        - | 13449 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   620857 | 13450 | `					iP2 = 1;` |
|   310431 | 13451 | `				}else{` |
|  2250591 | 13452 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        - | 13453 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   225341 | 13454 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   225341 | 13455 | `						iP1 = pInstr->iP1;` |
|   112673 | 13456 | `					}else{` |
|  2025255 | 13457 | `						p3 = pInstr->p3;` |
|        - | 13458 | `					}` |
|        - | 13459 | `					/* POP the last dynamic load instruction */` |
|  2250591 | 13460 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        - | 13461 | `				}` |
|  1435724 | 13462 | `			}` |
|  8731708 | 13463 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|       62 | 13464 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|       62 | 13465 | `			if( pInstr ){` |
|       62 | 13466 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        - | 13467 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|        - | 13468 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|        - | 13469 | `					 */` |
|       19 | 13470 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|       19 | 13471 | `					iP1 = pInstr->iP1;` |
|       19 | 13472 | `					iP2 = pInstr->iP2;` |
|       19 | 13473 | `					p3  = pInstr->p3;` |
|       10 | 13474 | `				}else{` |
|       44 | 13475 | `					p3 = pInstr->p3;` |
|        - | 13476 | `				}` |
|       30 | 13477 | `			}` |
|       30 | 13478 | `		}` |
|  5083732 | 13479 | `	}` |
| 17808842 | 13480 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|   305810 | 13481 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|        - | 13482 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|        - | 13483 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|       32 | 13484 | `		iVmOp = 0;` |
|       14 | 13485 | `	}` |
| 17808847 | 13486 | `	if( iVmOp > 0 ){` |
| 17777551 | 13487 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   116771 | 13488 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|        - | 13489 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    11665 | 13490 | `				iP1 = 1;` |
|     5835 | 13491 | `			}` |
| 17719168 | 13492 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|        - | 13493 | `			/* Namespace-qualify the class name for NEW */ {` |
|   611291 | 13494 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   611291 | 13495 | `				VmInstr *pCallInstr = 0;` |
|   611291 | 13496 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   611023 | 13497 | `					pCallInstr = pPeek;` |
|   611023 | 13498 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|   305509 | 13499 | `				}` |
|   611291 | 13500 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   599659 | 13501 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|        - | 13502 | `					sxu32 nLitForClass;` |
|   599659 | 13503 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|        - | 13504 | `					/* If the CALL handler qualified the name with FUNCTION` |
|        - | 13505 | `					 * imports, recover the original literal (recorded in the` |
|        - | 13506 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|        - | 13507 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|        - | 13508 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|        - | 13509 | `					 * with class imports. */` |
|   599659 | 13510 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|       37 | 13511 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|       21 | 13512 | `					}else{` |
|   599627 | 13513 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|        - | 13514 | `					}` |
|   599659 | 13515 | `					pPeek->iP1 = 0;` |
|   599659 | 13516 | `					if( !bAbsolute ){` |
|   595759 | 13517 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|   297882 | 13518 | `					}else{` |
|     3905 | 13519 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|        - | 13520 | `					}` |
|   299827 | 13521 | `				}` |
|        - | 13522 | `			}` |
|   611291 | 13523 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   611291 | 13524 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|        - | 13525 | `				VmInstr *pPrev;` |
|   611023 | 13526 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   611023 | 13527 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|        - | 13528 | `					/* Pop the call instruction, preserve named-arg map and` |
|        - | 13529 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|        - | 13530 | `					 * accumulator exactly like OP_CALL would have). */` |
|   611023 | 13531 | `					iP1 = pInstr->iP1;` |
|   611023 | 13532 | `					iP2 = pInstr->iP2;` |
|   611023 | 13533 | `					if( pInstr->p3 ){` |
|       47 | 13534 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|       21 | 13535 | `					}` |
|   611023 | 13536 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|   305509 | 13537 | `				}` |
|   305514 | 13538 | `			}` |
| 17355142 | 13539 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|        - | 13540 | `			/* instanceof: right operand is a class name, not a constant.` |
|        - | 13541 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|    46757 | 13542 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    46757 | 13543 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    46757 | 13544 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    46757 | 13545 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|    46757 | 13546 | `				int isSpecialIs = 0;` |
|    46757 | 13547 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    46757 | 13548 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    46757 | 13549 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    46752 | 13550 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    46755 | 13551 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    23376 | 13552 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|       12 | 13553 | `						isSpecialIs = 1;` |
|        5 | 13554 | `					}` |
|    23376 | 13555 | `				}` |
|    46757 | 13556 | `				pInstr->iP1 = 0;` |
|    46757 | 13557 | `				if( !isSpecialIs && !bAbsolute ){` |
|    46737 | 13558 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    23366 | 13559 | `				}` |
|    23381 | 13560 | `			}` |
| 17026123 | 13561 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|        - | 13562 | `			/* Prevent constant expansion for member/property names.` |
|        - | 13563 | `			 * The right child (member name) was just compiled — its LOADC` |
|        - | 13564 | `			 * should not trigger constant lookup. */` |
|  3611301 | 13565 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  3611301 | 13566 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  3587973 | 13567 | `				pInstr->iP1 = 0;` |
|  1793984 | 13568 | `			}` |
|  3611301 | 13569 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|        - | 13570 | `				/* Static member access,remember that */` |
|   124799 | 13571 | `				iP1 = 1;` |
|   124799 | 13572 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   124799 | 13573 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|    19443 | 13574 | `					p3 = pInstr->p3;` |
|    19443 | 13575 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|     9719 | 13576 | `				}` |
|    62397 | 13577 | `			}` |
|        - | 13578 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|        - | 13579 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|        - | 13580 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|        - | 13581 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|  3611301 | 13582 | `			if( iP2 == PH7_MEMBER_READ ){` |
|  3611301 | 13583 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       42 | 13584 | `					iP2 = PH7_MEMBER_UNSET;` |
|  3611281 | 13585 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|    19483 | 13586 | `					iP2 = PH7_MEMBER_ISSET;` |
|  3601522 | 13587 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       17 | 13588 | `					iP2 = PH7_MEMBER_EMPTY;` |
|  3591775 | 13589 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|        - | 13590 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|   679175 | 13591 | `					iP2 = PH7_MEMBER_WRITE;` |
|   339585 | 13592 | `				}` |
|  1805648 | 13593 | `			}` |
|  1805648 | 13594 | `		}` |
|        - | 13595 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|        - | 13596 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|        - | 13597 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|        - | 13598 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|        - | 13599 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
| 17777551 | 13600 | `		if( bFcc ){` |
|       81 | 13601 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|       81 | 13602 | `			iP2 = 0;` |
|       81 | 13603 | `			p3 = 0;` |
|       81 | 13604 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|       81 | 13605 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|        - | 13606 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|        - | 13607 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|        - | 13608 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|        - | 13609 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|       37 | 13610 | `				void *pMemberName = pInstr->p3;` |
|       37 | 13611 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|       37 | 13612 | `				if( pMemberName ){` |
|        3 | 13613 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|        1 | 13614 | `				}` |
|       37 | 13615 | `				iP1 = 2;` |
|       19 | 13616 | `			}else{` |
|       45 | 13617 | `				iP1 = 1;` |
|        - | 13618 | `			}` |
|       40 | 13619 | `		}` |
|        - | 13620 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|        - | 13621 | `		 * This is the primary emit path for user-visible calls. */` |
| 17777551 | 13622 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  4887815 | 13623 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  2443905 | 13624 | `		}` |
|        - | 13625 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 17777551 | 13626 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  8888773 | 13627 | `	}` |
| 17808847 | 13628 | `	if( nJmpIdx > 0 ){` |
|        - | 13629 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   392329 | 13630 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   392329 | 13631 | `		if( pInstr ){` |
|   392329 | 13632 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|   196162 | 13633 | `		}` |
|   196162 | 13634 | `	}` |
| 17808847 | 13635 | `	return rc;` |
| 22466689 | 13636 | `}` |
|        - | 13637 | `/*` |
|        - | 13638 | ` * Compile a PHP expression.` |
|        - | 13639 | ` * According to the PHP language reference manual:` |
|        - | 13640 | ` *  Expressions are the most important building stones of PHP.` |
|        - | 13641 | ` *  In PHP, almost anything you write is an expression.` |
|        - | 13642 | ` *  The simplest yet most accurate way to define an expression` |
|        - | 13643 | ` *  is "anything that has a value".` |
|        - | 13644 | ` * If something goes wrong while compiling the expression,this` |
|        - | 13645 | ` * function takes care of generating the appropriate error` |
|        - | 13646 | ` * message.` |
|        - | 13647 | ` */` |
|  9793584 | 13648 | `static sxi32 PH7_CompileExpr(` |
|        - | 13649 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 13650 | `	sxi32 iFlags,        /* Control flags */` |
|        - | 13651 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|        - | 13652 | `	)` |
|        5 | 13653 | `{` |
|        - | 13654 | `	ph7_expr_node *pRoot;` |
|        - | 13655 | `	SySet sExprNode;` |
|        - | 13656 | `	SyToken *pEnd;` |
|        - | 13657 | `	sxi32 nExpr;` |
|        - | 13658 | `	sxi32 iNest;` |
|        - | 13659 | `	sxi32 rc;` |
|        - | 13660 | `	sxu32 nNullsafeBase;` |
|        - | 13661 | `	/* Initialize worker variables */` |
|  9793589 | 13662 | `	nExpr = 0;` |
|  9793589 | 13663 | `	pRoot = 0;` |
|        - | 13664 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|        - | 13665 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  9793589 | 13666 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  9793589 | 13667 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  9793589 | 13668 | `	SySetAlloc(&sExprNode,0x10);` |
|  9793589 | 13669 | `	rc = SXRET_OK;` |
|        - | 13670 | `	/* Delimit the expression */` |
|  9793589 | 13671 | `	pEnd = pGen->pIn;` |
|  9793589 | 13672 | `	iNest = 0;` |
| 78041003 | 13673 | `	while( pEnd < pGen->pEnd ){` |
| 74612731 | 13674 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|        - | 13675 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     4597 | 13676 | `			iNest++;` |
| 74610435 | 13677 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     4605 | 13678 | `			iNest--;` |
| 74605839 | 13679 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  6365935 | 13680 | `			if( iNest <= 0 ){` |
|  6365317 | 13681 | `				break;` |
|        - | 13682 | `			}` |
|      309 | 13683 | `		}` |
| 68247419 | 13684 | `		pEnd++;` |
|        5 | 13685 | `	}` |
|  9793589 | 13686 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   504881 | 13687 | `		SyToken *pEnd2 = pGen->pIn;` |
|   504881 | 13688 | `		iNest = 0;` |
|        - | 13689 | `		/* Stop at the first comma */` |
|  1099631 | 13690 | `		while( pEnd2 < pEnd ){` |
|   594757 | 13691 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|    15611 | 13692 | `				iNest++;` |
|   586954 | 13693 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|    15611 | 13694 | `				iNest--;` |
|   571348 | 13695 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|       63 | 13696 | `				if( iNest <= 0 ){` |
|        3 | 13697 | `					break;` |
|        - | 13698 | `				}` |
|       28 | 13699 | `			}` |
|   594755 | 13700 | `			pEnd2++;` |
|        5 | 13701 | `		}` |
|   504881 | 13702 | `		if( pEnd2 <pEnd ){` |
|        3 | 13703 | `			pEnd = pEnd2;` |
|        1 | 13704 | `		}` |
|   252438 | 13705 | `	}` |
|  9793589 | 13706 | `	if( pEnd > pGen->pIn ){` |
|  9793579 | 13707 | `		SyToken *pTmp = pGen->pEnd;` |
|        - | 13708 | `		/* Swap delimiter */` |
|  9793579 | 13709 | `		pGen->pEnd = pEnd;` |
|        - | 13710 | `		/* Try to get an expression tree */` |
|  9793579 | 13711 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  9793579 | 13712 | `		if( rc == SXRET_OK && pRoot ){` |
|  9793397 | 13713 | `			rc = SXRET_OK;` |
|  9793397 | 13714 | `			if( xTreeValidator ){` |
|        - | 13715 | `				/* Call the upper layer validator callback */` |
|   640367 | 13716 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   320181 | 13717 | `			}` |
|  9793397 | 13718 | `			if( rc != SXERR_ABORT ){` |
|        - | 13719 | `				/* Generate code for the given tree */` |
|  9793397 | 13720 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|        - | 13721 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|        - | 13722 | `				 * expression so they short-circuit to its end. */` |
|  9793397 | 13723 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  4896696 | 13724 | `			}` |
|  9793397 | 13725 | `			nExpr = 1;` |
|  4896696 | 13726 | `		}` |
|        - | 13727 | `		/* Release the whole tree */` |
|  9793579 | 13728 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|        - | 13729 | `		/* Synchronize token stream */` |
|  9793579 | 13730 | `		pGen->pEnd = pTmp;` |
|  9793579 | 13731 | `		pGen->pIn  = pEnd;` |
|  9793579 | 13732 | `		if( rc == SXERR_ABORT ){` |
|       13 | 13733 | `			SySetRelease(&sExprNode);` |
|       13 | 13734 | `			return SXERR_ABORT;` |
|        - | 13735 | `		}` |
|  4896782 | 13736 | `	}` |
|  9793579 | 13737 | `	SySetRelease(&sExprNode);` |
|  9793579 | 13738 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  4896797 | 13739 | `}` |
|        - | 13740 | `/*` |
|        - | 13741 | ` * Return a pointer to the node construct handler associated` |
|        - | 13742 | ` * with a given node type [i.e: string,integer,float,...].` |
|        - | 13743 | ` */` |
|  5795558 | 13744 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|        5 | 13745 | `{` |
|  5795563 | 13746 | `	if( nNodeType & PH7_TK_NUM ){` |
|        - | 13747 | `		/* Numeric literal: Either real or integer */` |
|  2000233 | 13748 | `		return PH7_CompileNumLiteral;` |
|  3795335 | 13749 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|        - | 13750 | `		/* Double quoted string */` |
|    45623 | 13751 | `		return PH7_CompileString;` |
|  3749717 | 13752 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|        - | 13753 | `		/* Single quoted string */` |
|  3749597 | 13754 | `		return PH7_CompileSimpleString;` |
|      125 | 13755 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|        - | 13756 | `		/* Heredoc */` |
|       70 | 13757 | `		return PH7_CompileHereDoc;` |
|       58 | 13758 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|        - | 13759 | `		/* Nowdoc */` |
|       51 | 13760 | `		return PH7_CompileNowDoc;` |
|        8 | 13761 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|        - | 13762 | `		/* Backtick quoted string */` |
|        6 | 13763 | `		return PH7_CompileBacktic;` |
|        - | 13764 | `	}` |
|        3 | 13765 | `	return 0;` |
|  2897784 | 13766 | `}` |
|        - | 13767 | `/*` |
|        - | 13768 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|        - | 13769 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|        - | 13770 | ` * in write context" parse error.` |
|        - | 13771 | ` */` |
|    14772 | 13772 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|        5 | 13773 | `{` |
|        - | 13774 | `	sxi32 rc;` |
|    14777 | 13775 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|    14775 | 13776 | `		return SXRET_OK;` |
|        - | 13777 | `	}` |
|        5 | 13778 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|        2 | 13779 | `		pNode ? pNode->pStart->nLine : 1,` |
|        - | 13780 | `		"Can't use nullsafe operator in write context");` |
|        3 | 13781 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     7391 | 13782 | `}` |
|        - | 13783 | `/*` |
|        - | 13784 | ` * Compile an unset() statement.` |
|        - | 13785 | ` * unset($var, $arr[$key], ...);` |
|        - | 13786 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|        - | 13787 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|        - | 13788 | ` * parent array before extracting the element to unset.` |
|        - | 13789 | ` */` |
|    10704 | 13790 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|        5 | 13791 | `{` |
|    10709 | 13792 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    10709 | 13793 | `	sxu32 nIdx = 0;` |
|        - | 13794 | `	SyString sName;` |
|        - | 13795 | `	sxi32 rc;` |
|        - | 13796 | `	/* Jump the 'unset' keyword */` |
|    10709 | 13797 | `	pGen->pIn++;` |
|        - | 13798 | `	/* Save delimiter */` |
|    10709 | 13799 | `	pTmp = pGen->pEnd;` |
|        - | 13800 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    10709 | 13801 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    10709 | 13802 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        - | 13803 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|        - | 13804 | `		SyToken *pClose;` |
|    10709 | 13805 | `		pGen->pIn++;   /* Skip '(' */` |
|    10709 | 13806 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    10709 | 13807 | `		pEnd = pClose; /* Stop at ')' */` |
|     5352 | 13808 | `	}` |
|    10709 | 13809 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|        - | 13810 | `	/* Resolve the 'unset' builtin name once */` |
|    10709 | 13811 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     3881 | 13812 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     3881 | 13813 | `		if( pObj == 0 ){` |
|      ! 0 | 13814 | `			return SXERR_ABORT;` |
|        - | 13815 | `		}` |
|     3881 | 13816 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     3881 | 13817 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     1938 | 13818 | `	}` |
|        - | 13819 | `	/* Compile each comma-separated argument */` |
|    25483 | 13820 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    14779 | 13821 | `		if( pGen->pIn < pNext ){` |
|    14779 | 13822 | `			pGen->pEnd = pNext;` |
|    14779 | 13823 | `			rc = PH7_CompileExpr(&(*pGen),` |
|        - | 13824 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|        - | 13825 | `				GenStateUnsetValidator);` |
|    14779 | 13826 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 13827 | `				return SXERR_ABORT;` |
|        - | 13828 | `			}` |
|    14779 | 13829 | `			if( rc != SXERR_EMPTY ){` |
|        - | 13830 | `				/* Emit call for this single argument */` |
|    14777 | 13831 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    14777 | 13832 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|    14777 | 13833 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     7386 | 13834 | `			}` |
|     7387 | 13835 | `		}` |
|        - | 13836 | `		/* Jump trailing commas */` |
|    18851 | 13837 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|     4077 | 13838 | `			pNext++;` |
|        5 | 13839 | `		}` |
|    14779 | 13840 | `		pGen->pIn = pNext;` |
|        5 | 13841 | `	}` |
|        - | 13842 | `	/* Skip past the closing ')' if present */` |
|    10709 | 13843 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    10709 | 13844 | `		pGen->pIn++;` |
|     5352 | 13845 | `	}` |
|        - | 13846 | `	/* Restore token stream */` |
|    10709 | 13847 | `	pGen->pEnd = pTmp;` |
|    10709 | 13848 | `	return SXRET_OK;` |
|     5357 | 13849 | `}` |
|        - | 13850 | `/*` |
|        - | 13851 | ` * PHP Language construct table.` |
|        - | 13852 | ` */` |
|        - | 13853 | `static const LangConstruct aLangConstruct[] = {` |
|        - | 13854 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|        - | 13855 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|        - | 13856 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|        - | 13857 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|        - | 13858 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|        - | 13859 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|        - | 13860 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|        - | 13861 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|        - | 13862 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|        - | 13863 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|        - | 13864 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|        - | 13865 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|        - | 13866 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|        - | 13867 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|        - | 13868 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|        - | 13869 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|        - | 13870 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|        - | 13871 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|        - | 13872 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|        - | 13873 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|        - | 13874 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|        - | 13875 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|        - | 13876 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|        - | 13877 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|        - | 13878 | `};` |
|        - | 13879 | `/*` |
|        - | 13880 | ` * Return a pointer to the statement handler routine associated` |
|        - | 13881 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|        - | 13882 | ` */` |
|  4870038 | 13883 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|        - | 13884 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|        - | 13885 | `	SyToken *pLookahed  /* Look-ahead token */` |
|        - | 13886 | `	)` |
|        5 | 13887 | `{` |
|  4870043 | 13888 | `	sxu32 n = 0;` |
| 20559464 | 13889 | `	for(;;){` |
| 41118933 | 13890 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   362773 | 13891 | `			break;` |
|        - | 13892 | `		}` |
| 40756165 | 13893 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  4507275 | 13894 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|      ! 0 | 13895 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|      ! 0 | 13896 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|        - | 13897 | `					/* 'static' (class context),return null */` |
|      ! 0 | 13898 | `					return 0;` |
|        - | 13899 | `				}` |
|      ! 0 | 13900 | `			}` |
|  4507270 | 13901 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       14 | 13902 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       14 | 13903 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|        - | 13904 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|        3 | 13905 | `				return 0;` |
|        - | 13906 | `			}` |
|        - | 13907 | `			/* Return a pointer to the handler.` |
|        - | 13908 | `			*/` |
|  4507273 | 13909 | `			return aLangConstruct[n].xConstruct;` |
|        - | 13910 | `		}` |
| 36248895 | 13911 | `		n++;` |
|        5 | 13912 | `	}` |
|   362773 | 13913 | `	if( pLookahed ){` |
|   362773 | 13914 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    58249 | 13915 | `			return PH7_CompileClassInterface;` |
|   304529 | 13916 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   265207 | 13917 | `			return PH7_CompileClass;` |
|    39327 | 13918 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|     7835 | 13919 | `			return PH7_CompileTrait;` |
|        - | 13920 | `		}` |
|        - | 13921 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|        - | 13922 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|        - | 13923 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|        - | 13924 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|    15746 | 13925 | `	}` |
|        - | 13926 | `	/* Not a language construct */` |
|    31497 | 13927 | `	return 0;` |
|  2435024 | 13928 | `}` |
|        - | 13929 | `/*` |
|        - | 13930 | ` * Check if the given keyword is in fact a PHP language construct.` |
|        - | 13931 | ` * Return TRUE on success. FALSE otheriwse.` |
|        - | 13932 | ` */` |
|    31494 | 13933 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|        5 | 13934 | `{` |
|        - | 13935 | `	int rc;` |
|    31499 | 13936 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|    31499 | 13937 | `	if( rc == FALSE ){` |
|    31380 | 13938 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     8118 | 13939 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|        - | 13940 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|        - | 13941 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|        - | 13942 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|        - | 13943 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|        - | 13944 | `			*/` |
|        - | 13945 | `			){` |
|    31377 | 13946 | `				rc = TRUE;` |
|    15686 | 13947 | `		}` |
|    15690 | 13948 | `	}` |
|    31499 | 13949 | `	return rc;` |
|        5 | 13950 | `}` |
|        - | 13951 | `/*` |
|        - | 13952 | ` * Compile a PHP chunk.` |
|        - | 13953 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|        - | 13954 | ` * takes care of generating the appropriate error message.` |
|        - | 13955 | ` */` |
|        - | 13956 | `/*` |
|        - | 13957 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|        - | 13958 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|        - | 13959 | ` * the chunk token set it becomes the pending docblock. An existing` |
|        - | 13960 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|        - | 13961 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|        - | 13962 | ` * intervening non-declaration statements.` |
|        - | 13963 | ` */` |
| 10920862 | 13964 | `static void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|        5 | 13965 | `{` |
| 10920867 | 13966 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
| 10920867 | 13967 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
| 10920867 | 13968 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|        - | 13969 | `	sxu32 nIdx, n;` |
| 10920862 | 13970 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|  1533929 | 13971 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|        - | 13972 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|        - | 13973 | `		 * indexes do not map to the sidecar */` |
|  9386945 | 13974 | `		return;` |
|        - | 13975 | `	}` |
|  1533927 | 13976 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|        - | 13977 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|        - | 13978 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|  1533927 | 13979 | `	SySetReset(&pGen->aPendingAttrs);` |
|  4603265 | 13980 | `	for( n = 0 ; n < nT ; n++ ){` |
|  3069343 | 13981 | `		if( aT[n].nTokIdx != nIdx ){` |
|  3061427 | 13982 | `			continue;` |
|        - | 13983 | `		}` |
|     7921 | 13984 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|       29 | 13985 | `			pGen->sPendingDoc = aT[n].sText;` |
|     7909 | 13986 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|     7897 | 13987 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|     3946 | 13988 | `		}` |
|     3963 | 13989 | `	}` |
|  5460436 | 13990 | `}` |
|        - | 13991 | `/*` |
|        - | 13992 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|        - | 13993 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|        - | 13994 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|        - | 13995 | ` */` |
|  3095812 | 13996 | `static void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|        5 | 13997 | `{` |
|        - | 13998 | `	char *zDup;` |
|  3095817 | 13999 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|  3095797 | 14000 | `		return;` |
|        - | 14001 | `	}` |
|       35 | 14002 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       10 | 14003 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|       25 | 14004 | `	if( zDup ){` |
|       25 | 14005 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|       10 | 14006 | `	}` |
|       25 | 14007 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|  1547911 | 14008 | `}` |
|        - | 14009 | `/*` |
|        - | 14010 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|        - | 14011 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|        - | 14012 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|        - | 14013 | ` * names may point into the token text, which must outlive the raw script` |
|        - | 14014 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|        - | 14015 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|        - | 14016 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|        - | 14017 | ` */` |
|     7904 | 14018 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|        5 | 14019 | `{` |
|        - | 14020 | `	SySet *pToken;` |
|        - | 14021 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|        - | 14022 | `	char *zSpan;` |
|     7909 | 14023 | `	sxi32 rc = SXRET_OK;` |
|     7909 | 14024 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|      ! 0 | 14025 | `		return SXRET_OK;` |
|        - | 14026 | `	}` |
|    11861 | 14027 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     3952 | 14028 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|     7909 | 14029 | `	if( zSpan == 0 ){` |
|      ! 0 | 14030 | `		return SXRET_OK;` |
|        - | 14031 | `	}` |
|        - | 14032 | `	/* The token set must outlive compilation too: interned operands may` |
|        - | 14033 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|        - | 14034 | `	 * the number of attribute declarations in the program. */` |
|     7909 | 14035 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|     7909 | 14036 | `	if( pToken == 0 ){` |
|      ! 0 | 14037 | `		return SXRET_OK;` |
|        - | 14038 | `	}` |
|     7909 | 14039 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|     7909 | 14040 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|     7909 | 14041 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|     7909 | 14042 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|     7909 | 14043 | `	pSavedIn = pGen->pIn;` |
|     7909 | 14044 | `	pSavedEnd = pGen->pEnd;` |
|     7913 | 14045 | `	while( pIn < pEnd ){` |
|        - | 14046 | `		ph7_attribute sAttr;` |
|        - | 14047 | `		SyBlob sFQN;` |
|     7913 | 14048 | `		int bAbsolute = 0;` |
|     7913 | 14049 | `		SyZero(&sAttr,sizeof(sAttr));` |
|     7913 | 14050 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|     7913 | 14051 | `		sAttr.nLine = pIn->nLine;` |
|     7913 | 14052 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|       75 | 14053 | `			bAbsolute = 1;` |
|       75 | 14054 | `			pIn++;` |
|       35 | 14055 | `		}` |
|     7913 | 14056 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     7913 | 14057 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     7913 | 14058 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|     7913 | 14059 | `			pIn++;` |
|     7913 | 14060 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|      ! 0 | 14061 | `				SyBlobAppend(&sFQN,"\\",1);` |
|      ! 0 | 14062 | `				pIn++;` |
|      ! 0 | 14063 | `				continue;` |
|        - | 14064 | `			}` |
|     7913 | 14065 | `			break;` |
|      ! 0 | 14066 | `		}` |
|     7913 | 14067 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|        - | 14068 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|        - | 14069 | `			 * this feature; never turn it into a new fatal) */` |
|      ! 0 | 14070 | `			SyBlobRelease(&sFQN);` |
|      ! 0 | 14071 | `			break;` |
|        - | 14072 | `		}` |
|        - | 14073 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|        - | 14074 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|        - | 14075 | `		{` |
|     7913 | 14076 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|     7913 | 14077 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|     7913 | 14078 | `			char *zDup = 0;` |
|     7913 | 14079 | `			if( !bAbsolute ){` |
|     7843 | 14080 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|     7843 | 14081 | `				if( pImp ){` |
|      ! 0 | 14082 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|      ! 0 | 14083 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|      ! 0 | 14084 | `					if( zDup ){` |
|      ! 0 | 14085 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|      ! 0 | 14086 | `					}` |
|     7843 | 14087 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        - | 14088 | `					SyBlob sTmp;` |
|      ! 0 | 14089 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|      ! 0 | 14090 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      ! 0 | 14091 | `					SyBlobAppend(&sTmp,"\\",1);` |
|      ! 0 | 14092 | `					SyBlobAppend(&sTmp,zName,nName);` |
|      ! 0 | 14093 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      ! 0 | 14094 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|      ! 0 | 14095 | `					if( zDup ){` |
|      ! 0 | 14096 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|      ! 0 | 14097 | `					}` |
|      ! 0 | 14098 | `					SyBlobRelease(&sTmp);` |
|      ! 0 | 14099 | `				}` |
|     3919 | 14100 | `			}` |
|     7913 | 14101 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|     7913 | 14102 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|     7913 | 14103 | `				if( zDup ){` |
|     7913 | 14104 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|     3954 | 14105 | `				}` |
|     3954 | 14106 | `			}` |
|        - | 14107 | `		}` |
|     7913 | 14108 | `		SyBlobRelease(&sFQN);` |
|     7913 | 14109 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        - | 14110 | `			SyToken *pArgsEnd;` |
|     7811 | 14111 | `			pIn++;` |
|     7811 | 14112 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|    15631 | 14113 | `			while( pIn < pArgsEnd ){` |
|     7825 | 14114 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|     7825 | 14115 | `				sxi32 iDepth = 0;` |
|        - | 14116 | `				ph7_attr_arg sArgRec;` |
|    77765 | 14117 | `				while( pArgStop < pArgsEnd ){` |
|    69961 | 14118 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       11 | 14119 | `						iDepth++;` |
|    69956 | 14120 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       11 | 14121 | `						iDepth--;` |
|    69946 | 14122 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|       17 | 14123 | `						break;` |
|        - | 14124 | `					}` |
|    69945 | 14125 | `					pArgStop++;` |
|        5 | 14126 | `				}` |
|     7825 | 14127 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|     7825 | 14128 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     7820 | 14129 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|     7804 | 14130 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|       28 | 14131 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        9 | 14132 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|       19 | 14133 | `					if( zN ){` |
|       19 | 14134 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|        9 | 14135 | `					}` |
|       19 | 14136 | `					pArgStart += 2;` |
|        9 | 14137 | `				}` |
|     7825 | 14138 | `				if( pArgStart < pArgStop ){` |
|        - | 14139 | `					SySet *pInstrContainer;` |
|     7825 | 14140 | `					pGen->pIn = pArgStart;` |
|     7825 | 14141 | `					pGen->pEnd = pArgStop;` |
|     7825 | 14142 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     7825 | 14143 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|     7825 | 14144 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|     7825 | 14145 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|     7825 | 14146 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     7825 | 14147 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 14148 | `						pGen->pIn = pSavedIn;` |
|      ! 0 | 14149 | `						pGen->pEnd = pSavedEnd;` |
|      ! 0 | 14150 | `						return SXERR_ABORT;` |
|        - | 14151 | `					}` |
|     7825 | 14152 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|     3910 | 14153 | `				}` |
|     7825 | 14154 | `				pIn = pArgStop;` |
|     7825 | 14155 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|       17 | 14156 | `					pIn++;` |
|        8 | 14157 | `				}` |
|        5 | 14158 | `			}` |
|     7811 | 14159 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|     3903 | 14160 | `		}` |
|     7913 | 14161 | `		SySetPut(pOut,(const void *)&sAttr);` |
|     7913 | 14162 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        5 | 14163 | `			pIn++;` |
|        5 | 14164 | `			continue;` |
|        - | 14165 | `		}` |
|     7909 | 14166 | `		break;` |
|      ! 0 | 14167 | `	}` |
|     7909 | 14168 | `	pGen->pIn = pSavedIn;` |
|     7909 | 14169 | `	pGen->pEnd = pSavedEnd;` |
|     7909 | 14170 | `	return SXRET_OK;` |
|     3957 | 14171 | `}` |
|        - | 14172 | `/*` |
|        - | 14173 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|        - | 14174 | ` * every recorded group into pOut and clear the pending list.` |
|        - | 14175 | ` */` |
|  3095816 | 14176 | `static sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|        5 | 14177 | `{` |
|  3095821 | 14178 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|        - | 14179 | `	sxu32 n;` |
|        - | 14180 | `	sxi32 rc;` |
|  3103713 | 14181 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|     7897 | 14182 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|     7897 | 14183 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 14184 | `			return SXERR_ABORT;` |
|        - | 14185 | `		}` |
|     3951 | 14186 | `	}` |
|  3095821 | 14187 | `	SySetReset(&pGen->aPendingAttrs);` |
|  3095821 | 14188 | `	return SXRET_OK;` |
|  1547913 | 14189 | `}` |
|        - | 14190 | `/*` |
|        - | 14191 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|        - | 14192 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|        - | 14193 | ` * the main token stream, so the sidecar indexes map directly.` |
|        - | 14194 | ` */` |
|  1336958 | 14195 | `static sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|        5 | 14196 | `{` |
|  1336963 | 14197 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  1336963 | 14198 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  1336963 | 14199 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|        - | 14200 | `	sxu32 nIdx, n;` |
|        - | 14201 | `	sxi32 rc;` |
|  1336958 | 14202 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   194135 | 14203 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|  1142833 | 14204 | `		return SXRET_OK;` |
|        - | 14205 | `	}` |
|   194135 | 14206 | `	nIdx = (sxu32)(pTok - pBase);` |
|   582393 | 14207 | `	for( n = 0 ; n < nT ; n++ ){` |
|   388263 | 14208 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|       13 | 14209 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|       13 | 14210 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 14211 | `				return SXERR_ABORT;` |
|        - | 14212 | `			}` |
|        6 | 14213 | `		}` |
|   194134 | 14214 | `	}` |
|   194135 | 14215 | `	return SXRET_OK;` |
|   668484 | 14216 | `}` |
|  7845622 | 14217 | `static sxi32 GenStateCompileChunk(` |
|        - | 14218 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 14219 | `	sxi32 iFlags         /* Compile flags */` |
|        - | 14220 | `	)` |
|        5 | 14221 | `{` |
|        - | 14222 | `	ProcLangConstruct xCons;` |
|        - | 14223 | `	sxi32 rc;` |
|  7845627 | 14224 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  4533013 | 14225 | `	for(;;){` |
|  8455829 | 14226 | `		int bStmtIsDeclare = 0;` |
|  8455829 | 14227 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 14228 | `			/* No more input to process */` |
|    61043 | 14229 | `			break;` |
|        - | 14230 | `		}` |
|        - | 14231 | `		/* Bind a directly-preceding docblock to this statement */` |
|  8394791 | 14232 | `		GenStateSetPendingDoc(&(*pGen));` |
|  8394791 | 14233 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|        - | 14234 | `			/* php: a statement-position attribute group must be followed by a` |
|        - | 14235 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|        - | 14236 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|        - | 14237 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|        - | 14238 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|     7815 | 14239 | `			int bAttrTarget = 0;` |
|     7810 | 14240 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|     3939 | 14241 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|     7757 | 14242 | `				bAttrTarget = 1;` |
|     3935 | 14243 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       59 | 14244 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       58 | 14245 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|       15 | 14246 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|        4 | 14247 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|        4 | 14248 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|        1 | 14249 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|       59 | 14250 | `					bAttrTarget = 1;` |
|       29 | 14251 | `				}` |
|       29 | 14252 | `			}` |
|     7815 | 14253 | `			if( !bAttrTarget ){` |
|      ! 0 | 14254 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 14255 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|      ! 0 | 14256 | `					&pGen->pIn->sData);` |
|      ! 0 | 14257 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 14258 | `					break;` |
|        - | 14259 | `				}` |
|      ! 0 | 14260 | `				SySetReset(&pGen->aPendingAttrs);` |
|      ! 0 | 14261 | `			}` |
|     3905 | 14262 | `		}` |
|        - | 14263 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|        - | 14264 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  8394791 | 14265 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  4901101 | 14266 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  4901101 | 14267 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|       47 | 14268 | `				bStmtIsDeclare = 1;` |
|       21 | 14269 | `			}` |
|  2450548 | 14270 | `		}` |
|  8394791 | 14271 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|        - | 14272 | `			/* Any non-declare top-level statement locks the strict_types` |
|        - | 14273 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|   610175 | 14274 | `			pGen->bStrictTypesLocked = 1;` |
|   305085 | 14275 | `		}` |
|  8394791 | 14276 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|        - | 14277 | `			/* Compile block */` |
|     3899 | 14278 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|     3899 | 14279 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 14280 | `				break;` |
|        - | 14281 | `			}` |
|     1952 | 14282 | `		}else{` |
|  8390897 | 14283 | `			xCons = 0;` |
|  8390897 | 14284 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|        - | 14285 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|        - | 14286 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|        - | 14287 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|    31089 | 14288 | `				xCons = PH7_CompileClassModifiers;` |
|  8375355 | 14289 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|        - | 14290 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|        - | 14291 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|     3909 | 14292 | `				xCons = PH7_CompileEnum;` |
|  8357861 | 14293 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  4870043 | 14294 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        - | 14295 | `				/* Try to extract a language construct handler */` |
|  4870043 | 14296 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  4870043 | 14297 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|       13 | 14298 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 14299 | `						"Syntax error: Unexpected keyword '%z'",` |
|        8 | 14300 | `						&pGen->pIn->sData);` |
|        9 | 14301 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 14302 | `						break;` |
|        - | 14303 | `					}` |
|        - | 14304 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|        - | 14305 | `					 * this erroneous statement.` |
|        - | 14306 | `					 */` |
|        9 | 14307 | `					xCons = PH7_ErrorRecover;` |
|        4 | 14308 | `				}` |
|  5920890 | 14309 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   124639 | 14310 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|        - | 14311 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|      117 | 14312 | `				xCons = PH7_CompileLabel;` |
|       56 | 14313 | `			}` |
|  8390897 | 14314 | `			if( xCons == 0 ){` |
|        - | 14315 | `				/* Assume an expression an try to compile it */` |
|  3517245 | 14316 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  3517245 | 14317 | `				if(  rc != SXERR_EMPTY ){` |
|        - | 14318 | `					/* Pop l-value */` |
|  3517095 | 14319 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  1758545 | 14320 | `				}` |
|  1758625 | 14321 | `			}else{` |
|        - | 14322 | `				/* Go compile the sucker */` |
|  4873657 | 14323 | `				rc = xCons(&(*pGen));` |
|        - | 14324 | `			}` |
|  8390897 | 14325 | `			if( rc == SXERR_ABORT ){` |
|        - | 14326 | `				/* Request to abort compilation */` |
|       13 | 14327 | `				break;` |
|        - | 14328 | `			}` |
|        - | 14329 | `		}` |
|        - | 14330 | `		/* Ignore trailing semi-colons ';' */` |
| 14509527 | 14331 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  6114751 | 14332 | `			pGen->pIn++;` |
|        5 | 14333 | `		}` |
|  8394781 | 14334 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|        - | 14335 | `			/* Compile a single statement and return */` |
|  7784579 | 14336 | `			break;` |
|        - | 14337 | `		}` |
|        - | 14338 | `		/* LOOP ONE */` |
|        - | 14339 | `		/* LOOP TWO */` |
|        - | 14340 | `		/* LOOP THREE */` |
|        - | 14341 | `		/* LOOP FOUR */` |
|        5 | 14342 | `	}` |
|        - | 14343 | `	/* Return compilation status */` |
|  7845627 | 14344 | `	return rc;` |
|        5 | 14345 | `}` |
|        - | 14346 | `/*` |
|        - | 14347 | ` * Compile a Raw PHP chunk.` |
|        - | 14348 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|        - | 14349 | ` * takes care of generating the appropriate error message.` |
|        - | 14350 | ` */` |
|    61050 | 14351 | `static sxi32 PH7_CompilePHP(` |
|        - | 14352 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - | 14353 | `	SySet *pTokenSet,     /* Token set */` |
|        - | 14354 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|        - | 14355 | `	)` |
|        5 | 14356 | `{` |
|    61055 | 14357 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|        - | 14358 | `	sxi32 rc;` |
|        - | 14359 | `	/* Reset the token set (and its trivia sidecar) */` |
|    61055 | 14360 | `	SySetReset(&(*pTokenSet));` |
|    61055 | 14361 | `	SySetReset(&pGen->aTrivia);` |
|        - | 14362 | `	/* Mark as the default token set */` |
|    61055 | 14363 | `	pGen->pTokenSet = &(*pTokenSet);` |
|        - | 14364 | `	/* Advance the stream cursor */` |
|    61055 | 14365 | `	pGen->pRawIn++;` |
|        - | 14366 | `	/* Tokenize the PHP chunk first */` |
|    61055 | 14367 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|        - | 14368 | `	/* Point to the head and tail of the token stream. */` |
|    61055 | 14369 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|    61055 | 14370 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|    61055 | 14371 | `	if( is_expr ){` |
|      ! 0 | 14372 | `		rc = SXERR_EMPTY;` |
|      ! 0 | 14373 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - | 14374 | `			/* A simple expression,compile it */` |
|      ! 0 | 14375 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|      ! 0 | 14376 | `		}` |
|        - | 14377 | `		/* Emit the DONE instruction */` |
|      ! 0 | 14378 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      ! 0 | 14379 | `		return SXRET_OK;` |
|        - | 14380 | `	}` |
|    61055 | 14381 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|        - | 14382 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|        - | 14383 | `		/*` |
|        - | 14384 | `		 * Shortcut syntax for the 'echo' language construct.` |
|        - | 14385 | `		 * According to the PHP reference manual:` |
|        - | 14386 | `		 *  echo() also has a shortcut syntax, where you can` |
|        - | 14387 | `		 *  immediately follow` |
|        - | 14388 | `		 *  the opening tag with an equals sign as follows:` |
|        - | 14389 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|        - | 14390 | `		 * Symisc extension:` |
|        - | 14391 | `		 *   This short syntax works with all PHP opening` |
|        - | 14392 | `		 *   tags unlike the default PHP engine that handle` |
|        - | 14393 | `		 *   only short tag.` |
|        - | 14394 | `		 */` |
|        - | 14395 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|        3 | 14396 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|        3 | 14397 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|        3 | 14398 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|        3 | 14399 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|        3 | 14400 | `		if( rc != SXERR_EMPTY ){` |
|        3 | 14401 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        1 | 14402 | `		}` |
|        3 | 14403 | `		return SXRET_OK;` |
|        - | 14404 | `	}` |
|        - | 14405 | `	/* Compile the PHP chunk */` |
|    61053 | 14406 | `	rc = GenStateCompileChunk(pGen,0);` |
|        - | 14407 | `	/* Fix exceptions jumps */` |
|    61053 | 14408 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        - | 14409 | `	/* Fix gotos now, the jump destination is resolved */` |
|    61053 | 14410 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|        3 | 14411 | `		rc = SXERR_ABORT;` |
|        1 | 14412 | `	}` |
|        - | 14413 | `	/* Reset container */` |
|    61053 | 14414 | `	SySetReset(&pGen->aGoto);` |
|    61053 | 14415 | `	SySetReset(&pGen->aLabel);` |
|    61053 | 14416 | `	SySetReset(&pGen->aNullsafeJmp);` |
|        - | 14417 | `	/* Compilation result */` |
|    61053 | 14418 | `	return rc;` |
|    30530 | 14419 | `}` |
|        - | 14420 | `/*` |
|        - | 14421 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|        - | 14422 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|        - | 14423 | ` * This is the only compile interface exported from this file.` |
|        - | 14424 | ` */` |
|    64110 | 14425 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|        - | 14426 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|        - | 14427 | `	SyString *pScript,  /* Script to compile */` |
|        - | 14428 | `	sxi32 iFlags        /* Compile flags */` |
|        - | 14429 | `	)` |
|        5 | 14430 | `{` |
|        - | 14431 | `	SySet aPhpToken,aRawToken;` |
|        - | 14432 | `	ph7_gen_state *pCodeGen;` |
|        - | 14433 | `	ph7_value *pRawObj;` |
|        - | 14434 | `	sxu32 nObjIdx;` |
|        - | 14435 | `	sxi32 nRawObj;` |
|        - | 14436 | `	int is_expr;` |
|        - | 14437 | `	sxi8 bSavedStrict;` |
|        - | 14438 | `	sxi8 bSavedStrictLocked;` |
|        - | 14439 | `	sxi32 rc;` |
|    64115 | 14440 | `	if( pScript->nByte < 1 ){` |
|        - | 14441 | `		/* Nothing to compile */` |
|      ! 0 | 14442 | `		return PH7_OK;` |
|        - | 14443 | `	}` |
|        - | 14444 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|        - | 14445 | `	 * file's flags so include/require restore them on return. */` |
|    64115 | 14446 | `	pCodeGen = &pVm->sCodeGen;` |
|    64115 | 14447 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|    64115 | 14448 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|    64115 | 14449 | `	pCodeGen->bStrictTypes = 0;` |
|    64115 | 14450 | `	pCodeGen->bStrictTypesLocked = 0;` |
|        - | 14451 | `	/* Initialize the tokens containers */` |
|    64115 | 14452 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|    64115 | 14453 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|    64115 | 14454 | `	SySetAlloc(&aPhpToken,0xc0);` |
|    64115 | 14455 | `	is_expr = 0;` |
|    64115 | 14456 | `	if( iFlags & PH7_PHP_ONLY ){` |
|        - | 14457 | `		SyToken sTmp;` |
|        - | 14458 | `		/* PHP only: -*/` |
|    50495 | 14459 | `		sTmp.nLine = 1;` |
|    50495 | 14460 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    50495 | 14461 | `		sTmp.pUserData = 0;` |
|    50495 | 14462 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    50495 | 14463 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    50495 | 14464 | `		if( iFlags & PH7_PHP_EXPR ){` |
|        - | 14465 | `			/* A simple PHP expression */` |
|      ! 0 | 14466 | `			is_expr = 1;` |
|      ! 0 | 14467 | `		}` |
|    25250 | 14468 | `	}else{` |
|        - | 14469 | `		/* Tokenize raw text */` |
|    13625 | 14470 | `		SySetAlloc(&aRawToken,32);` |
|    13625 | 14471 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|        - | 14472 | `	}` |
|        - | 14473 | `	/* Process high-level tokens */` |
|    64115 | 14474 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|    64115 | 14475 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|    64115 | 14476 | `	rc = PH7_OK;` |
|    64115 | 14477 | `	if( is_expr ){` |
|        - | 14478 | `		/* Compile the expression */` |
|      ! 0 | 14479 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|      ! 0 | 14480 | `		goto cleanup;` |
|        - | 14481 | `	}` |
|    64115 | 14482 | `	nObjIdx = 0;` |
|        - | 14483 | `	/* Each compilation unit starts in the global namespace.` |
|        - | 14484 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|        - | 14485 | `	 * preventing namespace bleeding across include()d files. */` |
|    64115 | 14486 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|        - | 14487 | `	/* Start the compilation process */` |
|    38870 | 14488 | `	for(;;){` |
|   138783 | 14489 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|    64103 | 14490 | `			break; /* No more tokens to process */` |
|        - | 14491 | `		}` |
|    74685 | 14492 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|        - | 14493 | `			/* Compile the PHP chunk */` |
|    61055 | 14494 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|    61055 | 14495 | `			if( rc == SXERR_ABORT ){` |
|       16 | 14496 | `				break;` |
|        - | 14497 | `			}` |
|    61043 | 14498 | `			continue;` |
|        - | 14499 | `		}` |
|        - | 14500 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|    13635 | 14501 | `		nRawObj = 0;` |
|    27265 | 14502 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|        - | 14503 | `			/* Consume the raw chunk without any processing */` |
|    13635 | 14504 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|    13635 | 14505 | `			if( pRawObj == 0 ){` |
|      ! 0 | 14506 | `				rc = SXERR_MEM;` |
|      ! 0 | 14507 | `				break;` |
|        - | 14508 | `			}` |
|        - | 14509 | `			/* Mark as constant and emit the load constant instruction */` |
|    13635 | 14510 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|    13635 | 14511 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|    13635 | 14512 | `			++nRawObj;` |
|    13635 | 14513 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|        5 | 14514 | `		}` |
|    13635 | 14515 | `		if( nRawObj > 0 ){` |
|        - | 14516 | `			/* Emit the consume instruction */` |
|    13635 | 14517 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     6815 | 14518 | `		}` |
|    32060 | 14519 | `	}` |
|    32055 | 14520 | `cleanup:` |
|    64115 | 14521 | `	SySetRelease(&aRawToken);` |
|    64115 | 14522 | `	SySetRelease(&aPhpToken);` |
|        - | 14523 | `	/* Restore outer file's strict_types scope */` |
|    64115 | 14524 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|    64115 | 14525 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|    64115 | 14526 | `	return rc;` |
|    32060 | 14527 | `}` |
|        - | 14528 | `/*` |
|        - | 14529 | ` * Utility routines.Initialize the code generator.` |
|        - | 14530 | ` */` |
|     3876 | 14531 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|        - | 14532 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 14533 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|        - | 14534 | `	void *pErrData     /* Last argument to xErr() */` |
|        - | 14535 | `	)` |
|        5 | 14536 | `{` |
|     3881 | 14537 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 14538 | `	/* Zero the structure */` |
|     3881 | 14539 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|        - | 14540 | `	/* Initial state */` |
|     3881 | 14541 | `	pGen->pVm  = &(*pVm);` |
|     3881 | 14542 | `	pGen->xErr = xErr;` |
|     3881 | 14543 | `	pGen->pErrData = pErrData;` |
|     3881 | 14544 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|     3881 | 14545 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|     3881 | 14546 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|     3881 | 14547 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|     3881 | 14548 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|     3881 | 14549 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|     3881 | 14550 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|        - | 14551 | `	/* Error log buffer */` |
|     3881 | 14552 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|        - | 14553 | `	/* General purpose working buffer */` |
|     3881 | 14554 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|        - | 14555 | `	/* Namespace state */` |
|     3881 | 14556 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     3881 | 14557 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|     3881 | 14558 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|     3881 | 14559 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|        - | 14560 | `	/* Create the global scope */` |
|     3881 | 14561 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|        - | 14562 | `	/* Point to the global scope */` |
|     3881 | 14563 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     3881 | 14564 | `	return SXRET_OK;` |
|        5 | 14565 | `}` |
|        - | 14566 | `/*` |
|        - | 14567 | ` * Utility routines. Reset the code generator to it's initial state.` |
|        - | 14568 | ` */` |
|    67598 | 14569 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|        - | 14570 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 14571 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|        - | 14572 | `	void *pErrData     /* Last argument to xErr() */` |
|        - | 14573 | `	)` |
|        5 | 14574 | `{` |
|    67603 | 14575 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 14576 | `	GenBlock *pBlock,*pParent;` |
|        - | 14577 | `	/* Reset state */` |
|    67603 | 14578 | `	SySetReset(&pGen->aLabel);` |
|    67603 | 14579 | `	SySetReset(&pGen->aGoto);` |
|    67603 | 14580 | `	SySetReset(&pGen->aNullsafeJmp);` |
|    67603 | 14581 | `	SySetReset(&pGen->aTrivia);` |
|    67603 | 14582 | `	SySetReset(&pGen->aPendingAttrs);` |
|    67603 | 14583 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|    67603 | 14584 | `	SyBlobRelease(&pGen->sErrBuf);` |
|    67603 | 14585 | `	SyBlobRelease(&pGen->sWorker);` |
|    67603 | 14586 | `	SyBlobRelease(&pGen->sNamespace);` |
|    67603 | 14587 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    67603 | 14588 | `	SyHashRelease(&pGen->hUseImports);` |
|    67603 | 14589 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    67603 | 14590 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|    67603 | 14591 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    67603 | 14592 | `	SyHashRelease(&pGen->hUseConstImports);` |
|    67603 | 14593 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|        - | 14594 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|        - | 14595 | `	 * They intern variable names and literal strings that are referenced by` |
|        - | 14596 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|        - | 14597 | `	 * Releasing them would either leak the interned strings or require freeing` |
|        - | 14598 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|        - | 14599 | `	 * number of unique names, which is acceptable. */` |
|        - | 14600 | `	/* Point to the global scope */` |
|    67603 | 14601 | `	pBlock = pGen->pCurrent;` |
|    67603 | 14602 | `	while( pBlock->pParent != 0 ){` |
|      ! 0 | 14603 | `		pParent = pBlock->pParent;` |
|      ! 0 | 14604 | `		GenStateFreeBlock(pBlock);` |
|      ! 0 | 14605 | `		pBlock = pParent;` |
|      ! 0 | 14606 | `	}` |
|    67603 | 14607 | `	pGen->xErr = xErr;` |
|    67603 | 14608 | `	pGen->pErrData = pErrData;` |
|    67603 | 14609 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    67603 | 14610 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|    67603 | 14611 | `	pGen->pIn = pGen->pEnd = 0;` |
|    67603 | 14612 | `	pGen->nErr = 0;` |
|    67603 | 14613 | `	return SXRET_OK;` |
|        5 | 14614 | `}` |
|        - | 14615 | `/*` |
|        - | 14616 | ` * Generate a compile-time error message.` |
|        - | 14617 | ` * If the error count limit is reached (usually 15 error message)` |
|        - | 14618 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|        - | 14619 | ` * abort compilation immediately.` |
|        - | 14620 | ` */` |
|    16170 | 14621 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|        5 | 14622 | `{` |
|    16175 | 14623 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|    16175 | 14624 | `	const char *zErr = "Error";` |
|        - | 14625 | `	SyString *pFile;` |
|        - | 14626 | `	va_list ap;` |
|        - | 14627 | `	sxi32 rc;` |
|        - | 14628 | `	/* Reset the working buffer */` |
|    16175 | 14629 | `	SyBlobReset(pWorker);` |
|        - | 14630 | `	/* Peek the processed file path if available */` |
|    16175 | 14631 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|    16175 | 14632 | `	if( nErrType == E_ERROR ){` |
|        - | 14633 | `		/* Increment the error counter */` |
|      551 | 14634 | `		pGen->nErr++;` |
|      551 | 14635 | `		if( pGen->nErr > 15 ){` |
|        - | 14636 | `			/* Error count limit reached */` |
|        6 | 14637 | `			if( pGen->xErr ){` |
|        6 | 14638 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|        6 | 14639 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|        6 | 14640 | `				if( pFile ){` |
|        6 | 14641 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|        2 | 14642 | `				}` |
|        6 | 14643 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|        6 | 14644 | `				if( SyBlobLength(pWorker) > 0 ){` |
|        6 | 14645 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|        2 | 14646 | `				}` |
|        2 | 14647 | `			}` |
|        - | 14648 | `			/* Abort immediately */` |
|        6 | 14649 | `			return SXERR_ABORT;` |
|        - | 14650 | `		}` |
|      271 | 14651 | `	}` |
|    16171 | 14652 | `	if( pGen->xErr == 0 ){` |
|        - | 14653 | `		/* No available error consumer,return immediately */` |
|    15511 | 14654 | `		return SXRET_OK;` |
|        - | 14655 | `	}` |
|      664 | 14656 | `	switch(nErrType){` |
|      544 | 14657 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|       32 | 14658 | `	case E_WARNING: zErr = "Warning";     break;` |
|       82 | 14659 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       11 | 14660 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|      ! 0 | 14661 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|      ! 0 | 14662 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|      ! 0 | 14663 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|        7 | 14664 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|      ! 0 | 14665 | `	default:` |
|      ! 0 | 14666 | `		break;` |
|        - | 14667 | `	}` |
|      664 | 14668 | `	rc = SXRET_OK;` |
|        - | 14669 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|      664 | 14670 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|      664 | 14671 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|      664 | 14672 | `	va_start(ap,zFormat);` |
|      664 | 14673 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|      664 | 14674 | `	va_end(ap);` |
|      664 | 14675 | `	if( pFile ){` |
|      664 | 14676 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|      330 | 14677 | `	}` |
|        - | 14678 | `	/* Append a new line */` |
|      664 | 14679 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|      664 | 14680 | `	if( SyBlobLength(pWorker) > 0 ){` |
|        - | 14681 | `		/* Consume the generated error message */` |
|      664 | 14682 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|      330 | 14683 | `	}` |
|      664 | 14684 | `	return rc;` |
|     8090 | 14685 | `}` |
|        - | 14686 |  |
