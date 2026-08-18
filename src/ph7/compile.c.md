# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 6619/8186 lines (80.86%)

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
|    58550 |   137 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|        5 |   138 | `{` |
|    58555 |   139 | `	GenBlock *pBlock = pCurrent;` |
|   136214 |   140 | `	for(;;){` |
|   272433 |   141 | `		if( pBlock->iFlags & iBlockType ){` |
|    58447 |   142 | `			iCount--; /* Decrement nesting level */` |
|    58447 |   143 | `			if( iCount < 1 ){` |
|        - |   144 | `				/* Block meet with the desired criteria */` |
|    58421 |   145 | `				return pBlock;` |
|        - |   146 | `			}` |
|       13 |   147 | `		}` |
|        - |   148 | `		/* Point to the upper block */` |
|   214017 |   149 | `		pBlock = pBlock->pParent;` |
|   214017 |   150 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|        - |   151 | `			/* Forbidden */` |
|       72 |   152 | `			break;` |
|        - |   153 | `		}` |
|        5 |   154 | `	}` |
|        - |   155 | `	/* No such block */` |
|      139 |   156 | `	return 0;` |
|    29280 |   157 | `}` |
|        - |   158 | `/*` |
|        - |   159 | ` * Initialize a freshly allocated block instance.` |
|        - |   160 | ` */` |
|  5845774 |   161 | `static void GenStateInitBlock(` |
|        - |   162 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |   163 | `	GenBlock *pBlock,    /* Target block */` |
|        - |   164 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|        - |   165 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|        - |   166 | `	void *pUserData      /* Upper layer private data */` |
|        - |   167 | `	)` |
|        5 |   168 | `{` |
|        - |   169 | `	/* Initialize block fields */` |
|  5845779 |   170 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  5845779 |   171 | `	pBlock->pUserData   = pUserData;` |
|  5845779 |   172 | `	pBlock->pGen        = pGen;` |
|  5845779 |   173 | `	pBlock->iFlags      = iType;` |
|  5845779 |   174 | `	pBlock->pParent     = 0;` |
|  5845779 |   175 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  5845779 |   176 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  5845779 |   177 | `}` |
|        - |   178 | `/*` |
|        - |   179 | ` * Allocate a new block instance.` |
|        - |   180 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|        - |   181 | ` * on success.Otherwise generate a compile-time error and abort` |
|        - |   182 | ` * processing on failure.` |
|        - |   183 | ` */` |
|  5841890 |   184 | `static sxi32 GenStateEnterBlock(` |
|        - |   185 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - |   186 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|        - |   187 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|        - |   188 | `	void *pUserData,      /* Upper layer private data */` |
|        - |   189 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|        - |   190 | `	)` |
|        5 |   191 | `{` |
|        - |   192 | `	GenBlock *pBlock;` |
|        - |   193 | `	/* Allocate a new block instance */` |
|  5841895 |   194 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  5841895 |   195 | `	if( pBlock == 0 ){` |
|        - |   196 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   197 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   198 | `		 */` |
|      ! 0 |   199 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|        - |   200 | `		/* Abort processing immediately */` |
|      ! 0 |   201 | `		return SXERR_ABORT;` |
|        - |   202 | `	}` |
|        - |   203 | `	/* Zero the structure */` |
|  5841895 |   204 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  5841895 |   205 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|        - |   206 | `	/* Link to the parent block */` |
|  5841895 |   207 | `	pBlock->pParent = pGen->pCurrent;` |
|        - |   208 | `	/* Mark as the current block */` |
|  5841895 |   209 | `	pGen->pCurrent = pBlock;` |
|  5841895 |   210 | `	if( ppBlock ){` |
|        - |   211 | `		/* Write a pointer to the new instance */` |
|  2830281 |   212 | `		*ppBlock = pBlock;` |
|  1415138 |   213 | `	}` |
|  5841895 |   214 | `	return SXRET_OK;` |
|  2920950 |   215 | `}` |
|        - |   216 | `/*` |
|        - |   217 | ` * Release block fields without freeing the whole instance.` |
|        - |   218 | ` */` |
|  5841882 |   219 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|        5 |   220 | `{` |
|  5841887 |   221 | `	SySetRelease(&pBlock->aPostContFix);` |
|  5841887 |   222 | `	SySetRelease(&pBlock->aJumpFix);` |
|  5841887 |   223 | `}` |
|        - |   224 | `/*` |
|        - |   225 | ` * Release a block.` |
|        - |   226 | ` */` |
|  5841882 |   227 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|        5 |   228 | `{` |
|  5841887 |   229 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  5841887 |   230 | `	GenStateReleaseBlock(&(*pBlock));` |
|        - |   231 | `	/* Free the instance */` |
|  5841887 |   232 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  5841887 |   233 | `}` |
|        - |   234 | `/*` |
|        - |   235 | ` * POP and release a block from the stack of compiled blocks.` |
|        - |   236 | ` */` |
|  5841882 |   237 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|        5 |   238 | `{` |
|  5841887 |   239 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  5841887 |   240 | `	if( pBlock == 0 ){` |
|        - |   241 | `		/* No more block to pop */` |
|      ! 0 |   242 | `		return SXERR_EMPTY;` |
|        - |   243 | `	}` |
|        - |   244 | `	/* Point to the upper block */` |
|  5841887 |   245 | `	pGen->pCurrent = pBlock->pParent;` |
|  5841887 |   246 | `	if( ppBlock ){` |
|        - |   247 | `		/* Write a pointer to the popped block */` |
|      ! 0 |   248 | `		*ppBlock = pBlock;` |
|      ! 0 |   249 | `	}else{` |
|        - |   250 | `		/* Safely release the block */` |
|  5841887 |   251 | `		GenStateFreeBlock(&(*pBlock));` |
|        - |   252 | `	}` |
|  5841887 |   253 | `	return SXRET_OK;` |
|  2920946 |   254 | `}` |
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
|  2212786 |   265 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|        5 |   266 | `{` |
|        - |   267 | `	JumpFixup sJumpFix;` |
|        - |   268 | `	sxi32 rc;` |
|        - |   269 | `	/* Init the JumpFixup structure */` |
|  2212791 |   270 | `	sJumpFix.nJumpType = nJumpType;` |
|  2212791 |   271 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|        - |   272 | `	/* Insert in the jump fixup table */` |
|  2212791 |   273 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  2212791 |   274 | `	return rc;` |
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
|  4160302 |   287 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|        5 |   288 | `{` |
|        - |   289 | `	JumpFixup *aFix;` |
|        - |   290 | `	VmInstr *pInstr;` |
|        - |   291 | `	sxu32 nFixed;` |
|        - |   292 | `	sxu32 n;` |
|        - |   293 | `	/* Point to the jump fixup table */` |
|  4160307 |   294 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|        - |   295 | `	/* Fix the desired jumps */` |
|  8098901 |   296 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  3938599 |   297 | `		if( aFix[n].nJumpType < 0 ){` |
|        - |   298 | `			/* Already fixed */` |
|  1414241 |   299 | `			continue;` |
|        - |   300 | `		}` |
|  2524363 |   301 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|        - |   302 | `			/* Not of our interest */` |
|   311579 |   303 | `			continue;` |
|        - |   304 | `		}` |
|        - |   305 | `		/* Point to the instruction to fix */` |
|  2212789 |   306 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  2212789 |   307 | `		if( pInstr ){` |
|  2212789 |   308 | `			pInstr->iP2 = nJumpDest;` |
|  2212789 |   309 | `			nFixed++;` |
|        - |   310 | `			/* Mark as fixed */` |
|  2212789 |   311 | `			aFix[n].nJumpType = -1;` |
|  1106392 |   312 | `		}` |
|  1106397 |   313 | `	}` |
|        - |   314 | `	/* Total number of fixed jumps */` |
|  4160307 |   315 | `	return nFixed;` |
|        5 |   316 | `}` |
|        - |   317 | `/*` |
|        - |   318 | ` * Fix a 'goto' now the jump destination is resolved.` |
|        - |   319 | ` * The goto statement can be used to jump to another section` |
|        - |   320 | ` * in the program.` |
|        - |   321 | ` * Refer to the routine responsible of compiling the goto` |
|        - |   322 | ` * statement for more information.` |
|        - |   323 | ` */` |
|  1458828 |   324 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|        5 |   325 | `{` |
|        - |   326 | `	JumpFixup *pJump,*aJumps;` |
|        - |   327 | `	Label *pLabel,*aLabel;` |
|        - |   328 | `	VmInstr *pInstr;` |
|        - |   329 | `	sxi32 rc;` |
|        - |   330 | `	sxu32 n;` |
|        - |   331 | `	/* Point to the goto table */` |
|  1458833 |   332 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|        - |   333 | `	/* Fix */` |
|  1458979 |   334 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  1458831 |   359 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  1458963 |   360 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|      137 |   361 | `		if( aLabel[n].bRef == FALSE ){` |
|        - |   362 | `			/* Emit a warning */` |
|       40 |   363 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|       24 |   364 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|       12 |   365 | `		}` |
|       71 |   366 | `	}` |
|  1458831 |   367 | `	return SXRET_OK;` |
|   729419 |   368 | `}` |
|        - |   369 | `/*` |
|        - |   370 | ` * Check if a given token value is installed in the literal table.` |
|        - |   371 | ` */` |
|  7326542 |   372 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|        5 |   373 | `{` |
|        - |   374 | `	SyHashEntry *pEntry;` |
|  7326547 |   375 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  7326547 |   376 | `	if( pEntry == 0 ){` |
|  1930563 |   377 | `		return SXERR_NOTFOUND;` |
|        - |   378 | `	}` |
|  5395989 |   379 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  5395989 |   380 | `	return SXRET_OK;` |
|  3663276 |   381 | `}` |
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
|  1930558 |   392 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|        5 |   393 | `{` |
|  1930563 |   394 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  1930563 |   395 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   965279 |   396 | `	}` |
|  1930563 |   397 | `	return SXRET_OK;` |
|        5 |   398 | `}` |
|        - |   399 | `/*` |
|        - |   400 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|        - |   401 | ` * in the constant table.` |
|        - |   402 | ` */` |
|  1295818 |   403 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|        5 |   404 | `{` |
|        - |   405 | `	ph7_value *pObj;` |
|  1295823 |   406 | `	sxu32 nIdx = 0; /* cc warning */` |
|        - |   407 | `	/* Reserve a new constant */` |
|  1295823 |   408 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  1295823 |   409 | `	if( pObj == 0 ){` |
|      ! 0 |   410 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |   411 | `		return 0;` |
|        - |   412 | `	}` |
|  1295823 |   413 | `	*pIdx = nIdx;` |
|        - |   414 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|        - |   415 | `	 * the constant string iterals table [optimization purposes].` |
|        - |   416 | `	 */` |
|  1295823 |   417 | `	return pObj;` |
|   647914 |   418 | `}` |
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
|  3691258 |   433 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|        5 |   434 | `{` |
|        - |   435 | `	VmCallArgMap *pMap;` |
|  3691263 |   436 | `	if( !pGen->bStrictTypes ) return p3;` |
|       39 |   437 | `	if( p3 == 0 ){` |
|       35 |   438 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|       35 |   439 | `		if( pMap == 0 ) return 0;` |
|       35 |   440 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|       35 |   441 | `		p3 = (void *)pMap;` |
|       16 |   442 | `	}` |
|       39 |   443 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|       39 |   444 | `	return p3;` |
|  1845634 |   445 | `}` |
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
|  1296806 |   509 | `static int GenStateFindBadNumericSeparator(` |
|        - |   510 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|        5 |   511 | `{` |
|  1296811 |   512 | `	const char *z = pRaw->zString;` |
|  1296811 |   513 | `	sxu32 n = pRaw->nByte;` |
|  1296811 |   514 | `	int base = 10;` |
|        - |   515 | `	sxu32 i, start;` |
|  1296811 |   516 | `	if( n < 2 ) return 0;` |
|   404221 |   517 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|       80 |   518 | `		base = 16;` |
|   404182 |   519 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|      284 |   520 | `		base = 2;` |
|      141 |   521 | `	}` |
|  1306843 |   522 | `	for( i = 0; i < n; ++i ){` |
|   902641 |   523 | `		if( z[i] != '_' ) continue;` |
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
|   404207 |   540 | `	return 0;` |
|   648408 |   541 | `}` |
|        - |   542 | `/*` |
|        - |   543 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|        - |   544 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|        - |   545 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|        - |   546 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|        - |   547 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|        - |   548 | ` * so callers can bail from the current construct).` |
|        - |   549 | ` */` |
|  1296806 |   550 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|        5 |   551 | `{` |
|  1296811 |   552 | `	const char *zBad = 0;` |
|  1296811 |   553 | `	sxu32 nBad = 0;` |
|        - |   554 | `	SyString sBad;` |
|        - |   555 | `	sxi32 rc;` |
|  1296811 |   556 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  1296797 |   557 | `		return SXRET_OK;` |
|        - |   558 | `	}` |
|       18 |   559 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|       18 |   560 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|        - |   561 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|       18 |   562 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |   563 | `		return SXERR_ABORT;` |
|        - |   564 | `	}` |
|       18 |   565 | `	return SXERR_SYNTAX;` |
|   648408 |   566 | `}` |
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
|  1296792 |   583 | `static sxi32 GenStateStripNumericSeparators(` |
|        - |   584 | `	SyMemBackend *pAlloc,` |
|        - |   585 | `	const SyString *pToken,` |
|        - |   586 | `	char *zScratch, sxu32 nScratch,` |
|        - |   587 | `	SyString *pOut, char **pzAlloc)` |
|        5 |   588 | `{` |
|        - |   589 | `	sxu32 i, j;` |
|  1296797 |   590 | `	int hasUnderscore = 0;` |
|        - |   591 | `	char *zBuf;` |
|  1296797 |   592 | `	*pzAlloc = 0;` |
|  3089943 |   593 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  1793403 |   594 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   896578 |   595 | `	}` |
|  1296797 |   596 | `	if( !hasUnderscore ){` |
|  1296545 |   597 | `		SyStringDupPtr(pOut, pToken);` |
|  1296545 |   598 | `		return SXRET_OK;` |
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
|   648401 |   615 | `}` |
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
|  1295852 |   651 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|        5 |   652 | `{` |
|  1295857 |   653 | `	const char *z = pNum->zString;` |
|  1295857 |   654 | `	const char *zEnd = z + pNum->nByte;` |
|        - |   655 | `	const char *p, *q;` |
|        - |   656 | `	int n;` |
|  1295857 |   657 | `	*pbDecimal = FALSE;` |
|  1295857 |   658 | `	if( z >= zEnd ){` |
|      ! 0 |   659 | `		return FALSE;` |
|        - |   660 | `	}` |
|  1295857 |   661 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
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
|  1295781 |   676 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
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
|  1295501 |   691 | `	}else if( z[0] == '0' ){` |
|        - |   692 | `		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the` |
|        - |   693 | `		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1` |
|        - |   694 | `		 * "0o" marker ends the run and leaves it to the int path (as today). */` |
|   359469 |   695 | `		p = z;` |
|   718935 |   696 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|   359697 |   697 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|   359469 |   698 | `		if( n <= 21 ){` |
|   359467 |   699 | `			return FALSE;` |
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
|   936037 |   712 | `	p = z;` |
|   936037 |   713 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
|  2363255 |   714 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|   936037 |   715 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|       25 |   716 | `		*pbDecimal = TRUE;` |
|       25 |   717 | `		return TRUE;` |
|        - |   718 | `	}` |
|   936013 |   719 | `	return FALSE;` |
|   647931 |   720 | `}` |
|  1296778 |   721 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |   722 | `{` |
|  1296783 |   723 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  1296783 |   724 | `	sxu32 nIdx = 0;` |
|        - |   725 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  1296783 |   726 | `	char *zAlloc = 0;` |
|        - |   727 | `	SyString sNum;` |
|        - |   728 | `	sxi32 rc;` |
|   648389 |   729 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  1296783 |   730 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  1296783 |   731 | `	if( rc != SXRET_OK ){` |
|       14 |   732 | `		return rc;` |
|        - |   733 | `	}` |
|  1945157 |   734 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   648384 |   735 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  1296773 |   736 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   737 | `		return SXERR_ABORT;` |
|        - |   738 | `	}` |
|  1296773 |   739 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|        - |   740 | `		ph7_value *pObj;` |
|        - |   741 | `		sxi64 iValue;` |
|  1295857 |   742 | `		ph7_real rOverflow = 0;` |
|  1295857 |   743 | `		int bDecimalOverflow = 0;` |
|  1295857 |   744 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
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
|  1295823 |   761 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|  1295823 |   762 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  1295823 |   763 | `			if( pObj == 0 ){` |
|      ! 0 |   764 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |   765 | `				return SXERR_ABORT;` |
|        - |   766 | `			}` |
|  1295823 |   767 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|        - |   768 | `		}` |
|   647931 |   769 | `	}else{` |
|        - |   770 | `		/* Real number */` |
|        - |   771 | `		ph7_value *pObj;` |
|        - |   772 | `		/* Reserve a new constant */` |
|      920 |   773 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      920 |   774 | `		if( pObj == 0 ){` |
|      ! 0 |   775 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |   776 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |   777 | `			return SXERR_ABORT;` |
|        - |   778 | `		}` |
|      920 |   779 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|      920 |   780 | `		PH7_MemObjToReal(pObj);` |
|        - |   781 | `	}` |
|  1296773 |   782 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        - |   783 | `	/* Emit the load constant instruction */` |
|  1296773 |   784 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - |   785 | `	/* Node successfully compiled */` |
|  1296773 |   786 | `	return SXRET_OK;` |
|   648394 |   787 | `}` |
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
|  2979866 |   799 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |   800 | `{` |
|  2979871 |   801 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|        - |   802 | `	const char *zIn,*zCur,*zEnd;` |
|        - |   803 | `	ph7_value *pObj;` |
|        - |   804 | `	sxu32 nIdx;` |
|  2979871 |   805 | `	nIdx = 0; /* Prevent compiler warning */` |
|        - |   806 | `	/* Delimit the string */` |
|  2979871 |   807 | `	zIn  = pStr->zString;` |
|  2979871 |   808 | `	zEnd = &zIn[pStr->nByte];` |
|  2979871 |   809 | `	if( zIn >= zEnd ){` |
|        - |   810 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|        - |   811 | `		 * rather than reserving a new object each time. */` |
|   136133 |   812 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|   136133 |   813 | `		return SXRET_OK;` |
|        - |   814 | `	}` |
|  2843743 |   815 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|        - |   816 | `		/* Already processed,emit the load constant instruction` |
|        - |   817 | `		 * and return.` |
|        - |   818 | `		 */` |
|  1825755 |   819 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  1825755 |   820 | `		return SXRET_OK;` |
|        - |   821 | `	}` |
|        - |   822 | `	/* Reserve a new constant */` |
|  1017993 |   823 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  1017993 |   824 | `	if( pObj == 0 ){` |
|      ! 0 |   825 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |   826 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |   827 | `		return SXERR_ABORT;` |
|        - |   828 | `	}` |
|  1017993 |   829 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|        - |   830 | `	/* Compile the node */` |
|  1018047 |   831 | `	for(;;){` |
|  2036099 |   832 | `		if( zIn >= zEnd ){` |
|        - |   833 | `			/* End of input */` |
|  1017993 |   834 | `			break;` |
|        - |   835 | `		}` |
|  1018111 |   836 | `		zCur = zIn;` |
| 19839595 |   837 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
| 18821489 |   838 | `			zIn++;` |
|        5 |   839 | `		}` |
|  1018111 |   840 | `		if( zIn > zCur ){` |
|        - |   841 | `			/* Append raw contents*/` |
|   987013 |   842 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   493504 |   843 | `		}` |
|  1018111 |   844 | `		zIn++;` |
|  1018111 |   845 | `		if( zIn < zEnd ){` |
|    31217 |   846 | `			if( zIn[0] == '\\' ){` |
|        - |   847 | `				/* A literal backslash */` |
|    31105 |   848 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|    15664 |   849 | `			}else if( zIn[0] == '\'' ){` |
|        - |   850 | `				/* A single quote */` |
|       11 |   851 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|        6 |   852 | `			}else{` |
|        - |   853 | `				/* verbatim copy */` |
|      104 |   854 | `				zIn--;` |
|      104 |   855 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|      104 |   856 | `				zIn++;` |
|        - |   857 | `			}` |
|    15606 |   858 | `		}` |
|        - |   859 | `		/* Advance the stream cursor */` |
|  1018111 |   860 | `		zIn++;` |
|        5 |   861 | `	}` |
|        - |   862 | `	/* Emit the load constant instruction */` |
|  1017993 |   863 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  1017993 |   864 | `	if( pStr->nByte < 1024 ){` |
|        - |   865 | `		/* Install in the literal table */` |
|  1017993 |   866 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   508994 |   867 | `	}` |
|        - |   868 | `	/* Node successfully compiled */` |
|  1017993 |   869 | `	return SXRET_OK;` |
|  1489938 |   870 | `}` |
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
|     2468 |  1036 | `static sxi32 GenStateProcessStringExpression(` |
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
|     2473 |  1047 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        - |  1048 | `	/* Preallocate some slots */` |
|     2473 |  1049 | `	SySetAlloc(&sToken,0x08);` |
|        - |  1050 | `	/* Tokenize the text */` |
|     2473 |  1051 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken,0);` |
|        - |  1052 | `	/* Swap delimiter */` |
|     2473 |  1053 | `	pTmpIn  = pGen->pIn;` |
|     2473 |  1054 | `	pTmpEnd = pGen->pEnd;` |
|     2473 |  1055 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|     2473 |  1056 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|        - |  1057 | `	/* Compile the expression */` |
|     2473 |  1058 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  1059 | `	/* Restore token stream */` |
|     2473 |  1060 | `	pGen->pIn  = pTmpIn;` |
|     2473 |  1061 | `	pGen->pEnd = pTmpEnd;` |
|        - |  1062 | `	/* Release the token set */` |
|     2473 |  1063 | `	SySetRelease(&sToken);` |
|        - |  1064 | `	/* Compilation result */` |
|     2473 |  1065 | `	return rc;` |
|        5 |  1066 | `}` |
|        - |  1067 | `/*` |
|        - |  1068 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|        - |  1069 | ` */` |
|    38488 |  1070 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|        5 |  1071 | `{` |
|        - |  1072 | `	ph7_value *pConstObj;` |
|    38493 |  1073 | `	sxu32 nIdx = 0;` |
|        - |  1074 | `	/* Reserve a new constant */` |
|    38493 |  1075 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    38493 |  1076 | `	if( pConstObj == 0 ){` |
|      ! 0 |  1077 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  1078 | `		return 0;` |
|        - |  1079 | `	}` |
|    38493 |  1080 | `	(*pCount)++;` |
|    38493 |  1081 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|        - |  1082 | `	/* Emit the load constant instruction */` |
|    38493 |  1083 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    38493 |  1084 | `	return pConstObj;` |
|    19249 |  1085 | `}` |
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
|    36974 |  1148 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|        5 |  1149 | `{` |
|    36979 |  1150 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|        - |  1151 | `	const char *zIn,*zCur,*zEnd;` |
|    36979 |  1152 | `	ph7_value *pObj = 0;` |
|        - |  1153 | `	sxi32 iCons;` |
|        - |  1154 | `	sxi32 rc;` |
|        - |  1155 | `	/* Delimit the string */` |
|    36979 |  1156 | `	zIn  = pStr->zString;` |
|    36979 |  1157 | `	zEnd = &zIn[pStr->nByte];` |
|    36979 |  1158 | `	if( zIn >= zEnd ){` |
|        - |  1159 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|        - |  1160 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|        - |  1161 | `		 * literal table from growing when many "" literals appear in the source.` |
|        - |  1162 | `		 */` |
|      377 |  1163 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|      377 |  1164 | `		return SXRET_OK;` |
|        - |  1165 | `	}` |
|    36607 |  1166 | `	zCur = 0;` |
|        - |  1167 | `	/* Compile the node */` |
|    36607 |  1168 | `	iCons = 0;` |
|    19535 |  1169 | `	for(;;){` |
|    62919 |  1170 | `		zCur = zIn;` |
|   215239 |  1171 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|   154793 |  1172 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|       72 |  1173 | `				break;` |
|   154659 |  1174 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|     2338 |  1175 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     1170 |  1176 | `					break;` |
|        - |  1177 | `			}` |
|   152325 |  1178 | `			zIn++;` |
|        5 |  1179 | `		}` |
|    62919 |  1180 | `		if( zIn > zCur ){` |
|    20435 |  1181 | `			if( pObj == 0 ){` |
|    19905 |  1182 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    19905 |  1183 | `				if( pObj == 0 ){` |
|      ! 0 |  1184 | `					return SXERR_ABORT;` |
|        - |  1185 | `				}` |
|     9950 |  1186 | `			}` |
|    20435 |  1187 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    10215 |  1188 | `		}` |
|    62919 |  1189 | `		if( zIn >= zEnd ){` |
|    36605 |  1190 | `			break;` |
|        - |  1191 | `		}` |
|    26319 |  1192 | `		if( zIn[0] == '\\' ){` |
|    23851 |  1193 | `			const char *zPtr = 0;` |
|        - |  1194 | `			sxu32 n;` |
|    23851 |  1195 | `			zIn++;` |
|    23851 |  1196 | `			if( pObj == 0 ){` |
|    18593 |  1197 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    18593 |  1198 | `				if( pObj == 0 ){` |
|      ! 0 |  1199 | `					return SXERR_ABORT;` |
|        - |  1200 | `				}` |
|     9294 |  1201 | `			}` |
|    23851 |  1202 | `			if( zIn >= zEnd ){` |
|        - |  1203 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|        3 |  1204 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|        3 |  1205 | `				break;` |
|        - |  1206 | `			}` |
|    23849 |  1207 | `			n = sizeof(char); /* size of conversion */` |
|    23849 |  1208 | `			switch( zIn[0] ){` |
|       11 |  1209 | `			case '$':` |
|        - |  1210 | `				/* Dollar sign */` |
|       25 |  1211 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|       25 |  1212 | `				break;` |
|       57 |  1213 | `			case '\\':` |
|        - |  1214 | `				/* A literal backslash */` |
|      119 |  1215 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      119 |  1216 | `				break;` |
|        1 |  1217 | `			case 'e':` |
|        - |  1218 | `				/* Escape (ESC) ASCII code 27 */` |
|        3 |  1219 | `				PH7_MemObjStringAppend(pObj,"\x1b",sizeof(char));` |
|        3 |  1220 | `				break;` |
|        4 |  1221 | `			case 'f':` |
|        - |  1222 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|        9 |  1223 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|        9 |  1224 | `				break;` |
|    11370 |  1225 | `			case 'n':` |
|        - |  1226 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    22745 |  1227 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    22745 |  1228 | `				break;` |
|       19 |  1229 | `			case 'r':` |
|        - |  1230 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|       43 |  1231 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|       43 |  1232 | `				break;` |
|       27 |  1233 | `			case 't':` |
|        - |  1234 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|       59 |  1235 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|       59 |  1236 | `				break;` |
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
|       12 |  1343 | `			default:` |
|        - |  1344 | `				/* Unrecognized escape: keep the backslash, as php does.` |
|        - |  1345 | `				 * zIn[-1] is the backslash itself, so both bytes are contiguous` |
|        - |  1346 | `				 * in the source buffer — one batched append. */` |
|       25 |  1347 | `				PH7_MemObjStringAppend(pObj,&zIn[-1],sizeof(char)*2);` |
|       24 |  1348 | `				break;` |
|        - |  1349 | `			}` |
|        - |  1350 | `			/* Advance the stream cursor */` |
|    23849 |  1351 | `			zIn += n;` |
|    23849 |  1352 | `			continue;` |
|        - |  1353 | `		}` |
|     2473 |  1354 | `		if( zIn[0] == '{' ){` |
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
|     2335 |  1388 | `			const char *zExpr = zIn;` |
|        - |  1389 | `			/* Assemble variable name */` |
|     1190 |  1390 | `			for(;;){` |
|        - |  1391 | `				/* Jump leading dollars */` |
|     4715 |  1392 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|     2335 |  1393 | `					zIn++;` |
|        5 |  1394 | `				}` |
|     1190 |  1395 | `				for(;;){` |
|    12495 |  1396 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|     8925 |  1397 | `						zIn++;` |
|        5 |  1398 | `					}` |
|     2385 |  1399 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|        - |  1400 | `						/* UTF-8 stream */` |
|      ! 0 |  1401 | `						zIn++;` |
|      ! 0 |  1402 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  1403 | `							zIn++;` |
|      ! 0 |  1404 | `						}` |
|      ! 0 |  1405 | `						continue;` |
|        - |  1406 | `					}` |
|     2385 |  1407 | `					break;` |
|      ! 0 |  1408 | `				}` |
|     2385 |  1409 | `				if( zIn >= zEnd ){` |
|      252 |  1410 | `					break;` |
|        - |  1411 | `				}` |
|     2137 |  1412 | `				if( zIn[0] == '[' ){` |
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
|     2127 |  1430 | `				}else if(zIn[0] == '{' ){` |
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
|     2123 |  1448 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|        - |  1449 | `					/* Member access operator '->' */` |
|       53 |  1450 | `					zIn += 2;` |
|     2098 |  1451 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|        - |  1452 | `					/* Static member access operator '::' */` |
|      ! 0 |  1453 | `					zIn += 2;` |
|      ! 0 |  1454 | `				}else{` |
|     1039 |  1455 | `					break;` |
|        - |  1456 | `				}` |
|        3 |  1457 | `			}` |
|        - |  1458 | `			/* Process the expression */` |
|     2335 |  1459 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     2335 |  1460 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  1461 | `				return SXERR_ABORT;` |
|        - |  1462 | `			}` |
|     2335 |  1463 | `			if( rc != SXERR_EMPTY ){` |
|     2333 |  1464 | `				++iCons;` |
|     1164 |  1465 | `			}` |
|        - |  1466 | `		}` |
|        - |  1467 | `		/* Invalidate the previously used constant */` |
|     2473 |  1468 | `		pObj = 0;` |
|        5 |  1469 | `	}/*for(;;)*/` |
|    36607 |  1470 | `	if( iCons > 1 ){` |
|        - |  1471 | `		/* Concatenate all compiled constants */` |
|     1807 |  1472 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|      901 |  1473 | `	}` |
|        - |  1474 | `	/* Node successfully compiled */` |
|    36607 |  1475 | `	return SXRET_OK;` |
|    18492 |  1476 | `}` |
|        - |  1477 | `/*` |
|        - |  1478 | ` * Compile a double quoted string.` |
|        - |  1479 | ` *  See the block-comment above for more information.` |
|        - |  1480 | ` */` |
|    36912 |  1481 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  1482 | `{` |
|        - |  1483 | `	sxi32 rc;` |
|    36917 |  1484 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|    18456 |  1485 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  1486 | `	/* Compilation result */` |
|    36917 |  1487 | `	return rc;` |
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
|   529884 |  1531 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   529889 |  1542 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|        - |  1543 | `	/* Compile the expression*/` |
|   529889 |  1544 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|        - |  1545 | `	/* Restore token stream */` |
|   529889 |  1546 | `	RE_SWAP_DELIMITER(pGen);` |
|   529889 |  1547 | `	return rc;` |
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
|   567472 |  1588 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|        5 |  1589 | `{` |
|   567477 |  1590 | `	SyToken *pCur = pStart;` |
|   567477 |  1591 | `	sxi32 iNest = 0;` |
|  1720681 |  1592 | `	while( pCur < pEnd ){` |
|  1357333 |  1593 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|   204125 |  1594 | `			return pCur;` |
|        - |  1595 | `		}` |
|        - |  1596 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|        - |  1597 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|        - |  1598 | `		 * not an entry separator. Skip past the signature.` |
|        - |  1599 | `		 */` |
|  1153213 |  1600 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|    19527 |  1601 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|    19527 |  1602 | `			SyToken *pFn = pCur;` |
|    19522 |  1603 | `			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd` |
|      ! 0 |  1604 | `				&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|        5 |  1605 | `				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|      ! 0 |  1606 | `				pFn = &pCur[1];` |
|      ! 0 |  1607 | `				nKw = PH7_TKWRD_FN;` |
|      ! 0 |  1608 | `			}` |
|    19527 |  1609 | `			if( nKw == PH7_TKWRD_FN ){` |
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
|    19523 |  1640 | `			if( nKw == PH7_TKWRD_MATCH ){` |
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
|     9758 |  1660 | `		}` |
|  1153207 |  1661 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|    50943 |  1662 | `			iNest++;` |
|  1127738 |  1663 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|        - |  1664 | `			/* Don't worry about mismatched brackets here, the expression` |
|        - |  1665 | `			 * parser will shortly detect any syntax error. */` |
|    50943 |  1666 | `			iNest--;` |
|    25469 |  1667 | `		}` |
|  1153207 |  1668 | `		pCur++;` |
|        5 |  1669 | `	}` |
|   363353 |  1670 | `	return pEnd;` |
|   283741 |  1671 | `}` |
|        - |  1672 | `/*` |
|        - |  1673 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|        - |  1674 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|        - |  1675 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|        - |  1676 | ` */` |
|   290986 |  1677 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|        5 |  1678 | `{` |
|        - |  1679 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|        - |  1680 | `	SyToken *pKey,*pCur;` |
|   290991 |  1681 | `	sxi32 iEmitRef = 0;` |
|   290991 |  1682 | `	sxi32 iSpread = 0;` |
|   290991 |  1683 | `	sxi32 nPair = 0;` |
|        - |  1684 | `	sxi32 rc;` |
|   290991 |  1685 | `	xValidator = 0;` |
|   341404 |  1686 | `	for(;;){` |
|        - |  1687 | `		/* Jump leading commas */` |
|   974525 |  1688 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|   291717 |  1689 | `			pGen->pIn++;` |
|        5 |  1690 | `		}` |
|   682813 |  1691 | `		pCur = pGen->pIn;` |
|   682813 |  1692 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|        - |  1693 | `			/* No more entry to process */` |
|   290975 |  1694 | `			break;` |
|        - |  1695 | `		}` |
|   391843 |  1696 | `		if( pCur >= pGen->pIn ){` |
|      ! 0 |  1697 | `			continue;` |
|        - |  1698 | `		}` |
|        - |  1699 | `		/* Compile the key if available */` |
|   391843 |  1700 | `		pKey = pCur;` |
|   391843 |  1701 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   391843 |  1702 | `		rc = SXERR_EMPTY;` |
|   391843 |  1703 | `		if( pCur < pGen->pIn ){` |
|   137795 |  1704 | `			if( &pCur[1] >= pGen->pIn ){` |
|        - |  1705 | `				/* Missing value */` |
|       13 |  1706 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|       13 |  1707 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  1708 | `					return SXERR_ABORT;` |
|        - |  1709 | `				}` |
|       13 |  1710 | `				return SXRET_OK;` |
|        - |  1711 | `			}` |
|        - |  1712 | `			/* Compile the expression holding the key */` |
|   137785 |  1713 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|        - |  1714 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|   137785 |  1715 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  1716 | `				return SXERR_ABORT;` |
|        - |  1717 | `			}` |
|   137785 |  1718 | `			pCur++; /* Jump the '=>' operator */` |
|   322943 |  1719 | `		}else if( pKey == pCur ){` |
|        - |  1720 | `			/* Key is omitted,emit a warning */` |
|      ! 0 |  1721 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|      ! 0 |  1722 | `			pCur++; /* Jump the '=>' operator */` |
|      ! 0 |  1723 | `		}else{` |
|        - |  1724 | `			/* Reset back the cursor and point to the entry value */` |
|   254053 |  1725 | `			pCur = pKey;` |
|        - |  1726 | `		}` |
|   391833 |  1727 | `		if( rc == SXERR_EMPTY ){` |
|        - |  1728 | `			/* No available key,load NULL */` |
|   254055 |  1729 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|   127025 |  1730 | `		}` |
|   391833 |  1731 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|   391831 |  1750 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   391831 |  1751 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
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
|   391827 |  1764 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   391827 |  1765 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  1766 | `			return SXERR_ABORT;` |
|        - |  1767 | `		}` |
|   391827 |  1768 | `		if( iSpread ){` |
|        - |  1769 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|       69 |  1770 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   391794 |  1771 | `		}else if( iEmitRef ){` |
|        - |  1772 | `			/* Emit the load reference instruction */` |
|       41 |  1773 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|       18 |  1774 | `		}` |
|   391827 |  1775 | `		xValidator = 0;` |
|   391827 |  1776 | `		iEmitRef = 0;` |
|   391827 |  1777 | `		iSpread = 0;` |
|   391827 |  1778 | `		nPair++;` |
|        5 |  1779 | `	}` |
|        - |  1780 | `	/* Emit the load map instruction */` |
|   290975 |  1781 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|        - |  1782 | `	/* Node successfully compiled */` |
|   290975 |  1783 | `	return SXRET_OK;` |
|   145498 |  1784 | `}` |
|        - |  1785 | `/*` |
|        - |  1786 | ` * Compile the 'array' language construct.` |
|        - |  1787 | ` *	 According to the PHP language reference manual` |
|        - |  1788 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|        - |  1789 | ` *   values to keys. This type is optimized for several different uses; it can` |
|        - |  1790 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|        - |  1791 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|        - |  1792 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|        - |  1793 | ` */` |
|   289264 |  1794 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  1795 | `{` |
|        - |  1796 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   289269 |  1797 | `	pGen->pIn += 2;` |
|   289269 |  1798 | `	pGen->pEnd--;` |
|   144632 |  1799 | `	SXUNUSED(iCompileFlag);` |
|   289269 |  1800 | `	return GenStateCompileArrayBody(pGen);` |
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
|     1722 |  1899 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  1900 | `{` |
|        - |  1901 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     1727 |  1902 | `	pGen->pIn++;` |
|     1727 |  1903 | `	pGen->pEnd--;` |
|      861 |  1904 | `	SXUNUSED(iCompileFlag);` |
|     1727 |  1905 | `	return GenStateCompileArrayBody(pGen);` |
|        5 |  1906 | `}` |
|        - |  1907 | `/*` |
|        - |  1908 | ` * Expression tree validator callback for the 'list' language construct.` |
|        - |  1909 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|        - |  1910 | ` * an invalid expression tree and this function will generate the appropriate` |
|        - |  1911 | ` * error message.` |
|        - |  1912 | ` * See the routine responible of compiling the list language construct` |
|        - |  1913 | ` * for more inforation.` |
|        - |  1914 | ` */` |
|      202 |  1915 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 |  1916 | `{` |
|      207 |  1917 | `	sxi32 rc = SXRET_OK;` |
|      207 |  1918 | `	if( pRoot->pOp ){` |
|        4 |  1919 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|        2 |  1920 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|        - |  1921 | `				/* Unexpected expression */` |
|      ! 0 |  1922 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - |  1923 | `					"list(): Expecting a variable not an expression");` |
|      ! 0 |  1924 | `				if( rc != SXERR_ABORT ){` |
|      ! 0 |  1925 | `					rc = SXERR_INVALID;` |
|      ! 0 |  1926 | `				}` |
|        1 |  1927 | `		}` |
|      205 |  1928 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|        - |  1929 | `		/* Unexpected expression */` |
|        6 |  1930 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - |  1931 | `			"list(): Expecting a variable not an expression");` |
|        6 |  1932 | `		if( rc != SXERR_ABORT ){` |
|        6 |  1933 | `			rc = SXERR_INVALID;` |
|        2 |  1934 | `		}` |
|        2 |  1935 | `	}` |
|      207 |  1936 | `	return rc;` |
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
|      122 |  2059 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|        5 |  2060 | `{` |
|        - |  2061 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|        - |  2062 | `	SyToken *pNext;` |
|        - |  2063 | `	SyToken *pClassifyIn;` |
|      127 |  2064 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|        - |  2065 | `	sxi32 nExpr;` |
|        - |  2066 | `	sxi32 rc;` |
|        - |  2067 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|        - |  2068 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|        - |  2069 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|        - |  2070 | `	 * list. */` |
|      127 |  2071 | `	pClassifyIn = pGen->pIn;` |
|      359 |  2072 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|      237 |  2073 | `		if( pGen->pIn >= pNext ){` |
|       13 |  2074 | `			nEmpty++;` |
|      231 |  2075 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|       38 |  2076 | `			nKeyed++;` |
|       20 |  2077 | `		}else{` |
|      189 |  2078 | `			nPositional++;` |
|        - |  2079 | `		}` |
|      237 |  2080 | `		pGen->pIn = &pNext[1];` |
|        5 |  2081 | `	}` |
|      127 |  2082 | `	pGen->pIn = pClassifyIn;` |
|      127 |  2083 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|      ! 0 |  2084 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  2085 | `			"Cannot use empty array entries in keyed array assignment");` |
|      ! 0 |  2086 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - |  2087 | `	}` |
|      127 |  2088 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|      ! 0 |  2089 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  2090 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|      ! 0 |  2091 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - |  2092 | `	}` |
|      127 |  2093 | `	if( nKeyed > 0 ){` |
|       30 |  2094 | `		return GenStateCompileKeyedListBody(pGen);` |
|        - |  2095 | `	}` |
|       99 |  2096 | `	nExpr = 0;` |
|       99 |  2097 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|      295 |  2098 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|      201 |  2099 | `		if( pGen->pIn < pNext ){` |
|        - |  2100 | `			/* Check for nested list() */` |
|      189 |  2101 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
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
|      188 |  2118 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
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
|      175 |  2134 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|      175 |  2135 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  2136 | `					SySetRelease(&sNested);` |
|      ! 0 |  2137 | `					return SXRET_OK;` |
|        - |  2138 | `				}` |
|        - |  2139 | `			}` |
|       97 |  2140 | `		}else{` |
|        - |  2141 | `			/* Empty entry,load NULL */` |
|       13 |  2142 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|        - |  2143 | `		}` |
|      201 |  2144 | `		nExpr++;` |
|        - |  2145 | `		/* Advance the stream cursor */` |
|      201 |  2146 | `		pGen->pIn = &pNext[1];` |
|        5 |  2147 | `	}` |
|        - |  2148 | `	/* Emit the LOAD_LIST instruction */` |
|       99 |  2149 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|        - |  2150 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|        - |  2151 | `	 * For each nested entry, emit code to extract the sub-array` |
|        - |  2152 | `	 * at the corresponding index and recursively destructure it.` |
|        - |  2153 | `	 */` |
|       99 |  2154 | `	if( SySetUsed(&sNested) > 0 ){` |
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
|       99 |  2196 | `	SySetRelease(&sNested);` |
|        - |  2197 | `	/* Node successfully compiled */` |
|       99 |  2198 | `	return SXRET_OK;` |
|       66 |  2199 | `}` |
|       38 |  2200 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  2201 | `{` |
|        - |  2202 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|       43 |  2203 | `	pGen->pIn += 2;` |
|       43 |  2204 | `	pGen->pEnd--;` |
|       19 |  2205 | `	SXUNUSED(iCompileFlag);` |
|       43 |  2206 | `	return GenStateCompileListBody(pGen);` |
|        5 |  2207 | `}` |
|       84 |  2208 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        4 |  2209 | `{` |
|        - |  2210 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|       88 |  2211 | `	pGen->pIn++;` |
|       88 |  2212 | `	pGen->pEnd--;` |
|       42 |  2213 | `	SXUNUSED(iCompileFlag);` |
|       88 |  2214 | `	return GenStateCompileListBody(pGen);` |
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
|      448 |  2243 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  2244 | `{` |
|      453 |  2245 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|        - |  2246 | `	char zName[512];         /* Unique lambda name */` |
|        - |  2247 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|        - |  2248 | `							  * one thread is allowed to compile the script.` |
|        - |  2249 | `						      */` |
|        - |  2250 | `	SyString sName;` |
|      453 |  2251 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `$f = #[A] function…` trivia`` |
|        - |  2252 | `	                              * is keyed to this ['static'] 'function' token */` |
|        - |  2253 | `	sxu32 nKwLine;` |
|      453 |  2254 | `	sxi32 iFlags = 0;` |
|        - |  2255 | `	sxu32 nLen;` |
|        - |  2256 | `	sxi32 rc;` |
|      224 |  2257 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  2258 |  |
|      453 |  2259 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|      448 |  2260 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      453 |  2261 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - |  2262 | `		/* Static closure: no $this auto-capture, bind refused */` |
|        9 |  2263 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|        9 |  2264 | `		pGen->pIn++; /* Jump the 'static' keyword */` |
|        4 |  2265 | `	}` |
|      453 |  2266 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|      453 |  2267 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      ! 0 |  2268 | `		pGen->pIn++;` |
|      ! 0 |  2269 | `	}` |
|        - |  2270 | `	/* Generate a unique name */` |
|      453 |  2271 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|        - |  2272 | `	/* Make sure the generated name is unique */` |
|      453 |  2273 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 |  2274 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      ! 0 |  2275 | `	}` |
|      453 |  2276 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|        - |  2277 | `	/* Compile the lambda body */` |
|      453 |  2278 | `	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,&pAnnonFunc);` |
|      453 |  2279 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2280 | `		return SXERR_ABORT;` |
|        - |  2281 | `	}` |
|      453 |  2282 | `	if( pAnnonFunc ){` |
|      453 |  2283 | `		pAnnonFunc->nLine = nKwLine;` |
|        - |  2284 | ``		/* Expression-position attributes (`$f = #[A] function () {}`): the trivia`` |
|        - |  2285 | `		 * sidecar keys them to the closure's first keyword token. */` |
|      453 |  2286 | `		if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnnonFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  2287 | `			return SXERR_ABORT;` |
|        - |  2288 | `		}` |
|      224 |  2289 | `	}` |
|        - |  2290 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|        - |  2291 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|        - |  2292 | `	 * the handler wraps either in a Closure instance. */` |
|      453 |  2293 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|        - |  2294 | `	/* Node successfully compiled */` |
|      453 |  2295 | `	return SXRET_OK;` |
|      229 |  2296 | `}` |
|        - |  2297 | `/*` |
|        - |  2298 | ` * Add a free variable to the arrow function's closure environment, unless` |
|        - |  2299 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|        - |  2300 | ` * enclosing arrow level, or has already been captured.` |
|        - |  2301 | ` */` |
|      196 |  2302 | `static sxi32 GenStateArrowAddCapture(` |
|        - |  2303 | `	ph7_gen_state *pGen,` |
|        - |  2304 | `	ph7_vm_func *pFunc,` |
|        - |  2305 | `	const char *zName,` |
|        - |  2306 | `	sxu32 nByte,` |
|        - |  2307 | `	SyString *aShadow,` |
|        - |  2308 | `	sxu32 nShadow)` |
|        3 |  2309 | `{` |
|        - |  2310 | `	ph7_vm_func_closure_env sEnv;` |
|        - |  2311 | `	ph7_vm_func_closure_env *aEnv;` |
|        - |  2312 | `	sxu32 n, nEnv;` |
|        - |  2313 | `	char *zDup;` |
|      199 |  2314 | `	if( nByte == 0 ){` |
|      ! 0 |  2315 | `		return SXRET_OK;` |
|        - |  2316 | `	}` |
|      196 |  2317 | `	if( nByte == sizeof("this")-1` |
|      107 |  2318 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|        3 |  2319 | `		return SXRET_OK;` |
|        - |  2320 | `	}` |
|      247 |  2321 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|      182 |  2322 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|      176 |  2323 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|      135 |  2324 | `			return SXRET_OK;` |
|        - |  2325 | `		}` |
|       27 |  2326 | `	}` |
|       63 |  2327 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       63 |  2328 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|       91 |  2329 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|       30 |  2330 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|       29 |  2331 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|        3 |  2332 | `			return SXRET_OK;` |
|        - |  2333 | `		}` |
|       15 |  2334 | `	}` |
|       61 |  2335 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|       61 |  2336 | `	if( zDup == 0 ){` |
|      ! 0 |  2337 | `		return SXERR_ABORT;` |
|        - |  2338 | `	}` |
|       61 |  2339 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       61 |  2340 | `	sEnv.iFlags = 0;` |
|       61 |  2341 | `	sEnv.nIdx = SXU32_HIGH;` |
|       61 |  2342 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       61 |  2343 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|       61 |  2344 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       61 |  2345 | `	return SXRET_OK;` |
|      101 |  2346 | `}` |
|        - |  2347 | `/*` |
|        - |  2348 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|        - |  2349 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|        - |  2350 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|        - |  2351 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|        - |  2352 | ` */` |
|       56 |  2353 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|        - |  2354 | `	ph7_gen_state *pGen,` |
|        - |  2355 | `	ph7_vm_func *pFunc,` |
|        - |  2356 | `	const char *zIn,` |
|        - |  2357 | `	const char *zEnd,` |
|        - |  2358 | `	SyString *aShadow,` |
|        - |  2359 | `	sxu32 nShadow)` |
|        2 |  2360 | `{` |
|        - |  2361 | `	sxi32 rc;` |
|      370 |  2362 | `	while( zIn < zEnd ){` |
|      314 |  2363 | `		if( zIn[0] == '\\' ){` |
|        5 |  2364 | `			zIn++;` |
|        5 |  2365 | `			if( zIn < zEnd ){` |
|        5 |  2366 | `				zIn++;` |
|        2 |  2367 | `			}` |
|        5 |  2368 | `			continue;` |
|        - |  2369 | `		}` |
|      308 |  2370 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|       26 |  2371 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|       24 |  2372 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|        - |  2373 | `			const char *zName;` |
|       26 |  2374 | `			zIn++; /* skip '$' */` |
|       26 |  2375 | `			zName = zIn;` |
|       82 |  2376 | `			while( zIn < zEnd ){` |
|       76 |  2377 | `				unsigned char c = (unsigned char)zIn[0];` |
|       76 |  2378 | `				if( c >= 0xc0 ){` |
|      ! 0 |  2379 | `					zIn++;` |
|      ! 0 |  2380 | `					while( zIn < zEnd` |
|      ! 0 |  2381 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  2382 | `						zIn++;` |
|      ! 0 |  2383 | `					}` |
|      ! 0 |  2384 | `					continue;` |
|        - |  2385 | `				}` |
|       76 |  2386 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       20 |  2387 | `					break;` |
|        - |  2388 | `				}` |
|       58 |  2389 | `				zIn++;` |
|        2 |  2390 | `			}` |
|       26 |  2391 | `			if( zIn > zName ){` |
|       38 |  2392 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|       24 |  2393 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|       26 |  2394 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  2395 | `					return SXERR_ABORT;` |
|        - |  2396 | `				}` |
|       12 |  2397 | `			}` |
|       26 |  2398 | `			continue;` |
|        - |  2399 | `		}` |
|      286 |  2400 | `		zIn++;` |
|        2 |  2401 | `	}` |
|       58 |  2402 | `	return SXRET_OK;` |
|       30 |  2403 | `}` |
|        - |  2404 | `/*` |
|        - |  2405 | ` * Scan the body token range of an arrow function for free-variable` |
|        - |  2406 | ` * references and record them in pFunc's closure environment. Handles:` |
|        - |  2407 | ` *   - plain $<id> pairs` |
|        - |  2408 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|        - |  2409 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|        - |  2410 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|        - |  2411 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|        - |  2412 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|        - |  2413 | ` *     are never mistakenly captured.` |
|        - |  2414 | ` */` |
|      296 |  2415 | `static sxi32 GenStateArrowCaptureScan(` |
|        - |  2416 | `	ph7_gen_state *pGen,` |
|        - |  2417 | `	ph7_vm_func *pFunc,` |
|        - |  2418 | `	SyToken *pStart,` |
|        - |  2419 | `	SyToken *pEnd,` |
|        - |  2420 | `	SyString *aShadow,` |
|        - |  2421 | `	sxu32 nShadow)` |
|        4 |  2422 | `{` |
|      300 |  2423 | `	SyToken *pScan = pStart;` |
|        - |  2424 | `	sxi32 rc;` |
|     1708 |  2425 | `	while( pScan < pEnd ){` |
|     1412 |  2426 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|       86 |  2427 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|       28 |  2428 | `				pScan->sData.zString,` |
|       56 |  2429 | `				pScan->sData.zString + pScan->sData.nByte,` |
|       28 |  2430 | `				aShadow,nShadow);` |
|       58 |  2431 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2432 | `				return SXERR_ABORT;` |
|        - |  2433 | `			}` |
|       58 |  2434 | `			pScan++;` |
|       58 |  2435 | `			continue;` |
|        - |  2436 | `		}` |
|     1356 |  2437 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|       30 |  2438 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|       30 |  2439 | `			SyToken *pFnKw = pScan;` |
|       28 |  2440 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|      ! 0 |  2441 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|        2 |  2442 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|      ! 0 |  2443 | `				pFnKw = &pScan[1];` |
|      ! 0 |  2444 | `				nKw = PH7_TKWRD_FN;` |
|      ! 0 |  2445 | `			}` |
|       30 |  2446 | `			if( nKw == PH7_TKWRD_FN ){` |
|        - |  2447 | `				SyToken *pInnerSigStart;` |
|        - |  2448 | `				SyToken *pInnerSigEnd;` |
|        - |  2449 | `				SyToken *pInnerBodyEnd;` |
|        - |  2450 | `				SyString *aInnerShadow;` |
|        - |  2451 | `				sxu32 nInnerShadow;` |
|        - |  2452 | `				sxu32 nInnerParamMax;` |
|        - |  2453 | `				SyToken *p;` |
|        - |  2454 | `				int iNestInner;` |
|       19 |  2455 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|       19 |  2456 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  2457 | `					pScan++;` |
|      ! 0 |  2458 | `				}` |
|       19 |  2459 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 |  2460 | `					pScan++;` |
|      ! 0 |  2461 | `					continue;` |
|        - |  2462 | `				}` |
|       19 |  2463 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|       19 |  2464 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|        - |  2465 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|       19 |  2466 | `				if( pInnerSigEnd >= pEnd ){` |
|      ! 0 |  2467 | `					pScan = pEnd;` |
|      ! 0 |  2468 | `					continue;` |
|        - |  2469 | `				}` |
|        - |  2470 | `				/* Build an augmented shadow list: inherited + inner params */` |
|       19 |  2471 | `				nInnerParamMax = 0;` |
|       57 |  2472 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|       39 |  2473 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|       13 |  2474 | `						nInnerParamMax++;` |
|        6 |  2475 | `					}` |
|       20 |  2476 | `				}` |
|       19 |  2477 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|       18 |  2478 | `					&pGen->pVm->sAllocator,` |
|       18 |  2479 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|       19 |  2480 | `				if( aInnerShadow == 0 ){` |
|      ! 0 |  2481 | `					return SXERR_ABORT;` |
|        - |  2482 | `				}` |
|       19 |  2483 | `				nInnerShadow = 0;` |
|       25 |  2484 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|        7 |  2485 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|        4 |  2486 | `				}` |
|       57 |  2487 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|       39 |  2488 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|       27 |  2489 | `						continue;` |
|        - |  2490 | `					}` |
|       13 |  2491 | `					if( &p[1] >= pInnerSigEnd ){` |
|      ! 0 |  2492 | `						break;` |
|        - |  2493 | `					}` |
|       13 |  2494 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  2495 | `						continue;` |
|        - |  2496 | `					}` |
|       13 |  2497 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|        7 |  2498 | `				}` |
|       19 |  2499 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|       19 |  2500 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|      ! 0 |  2501 | `					pScan++;` |
|      ! 0 |  2502 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|      ! 0 |  2503 | `						&& pScan->sData.nByte == 1` |
|      ! 0 |  2504 | `						&& pScan->sData.zString[0] == '?' ){` |
|      ! 0 |  2505 | `						pScan++;` |
|      ! 0 |  2506 | `					}` |
|      ! 0 |  2507 | `					if( pScan < pEnd` |
|      ! 0 |  2508 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|      ! 0 |  2509 | `						pScan++;` |
|      ! 0 |  2510 | `					}` |
|      ! 0 |  2511 | `				}` |
|       19 |  2512 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|       19 |  2513 | `					pScan++; /* past '=>' */` |
|        9 |  2514 | `				}` |
|       19 |  2515 | `				pInnerBodyEnd = pScan;` |
|       19 |  2516 | `				iNestInner = 0;` |
|      131 |  2517 | `				while( pInnerBodyEnd < pEnd ){` |
|      113 |  2518 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|        - |  2519 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|        - |  2520 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|      ! 0 |  2521 | `						break;` |
|        - |  2522 | `					}` |
|      113 |  2523 | `					if( pInnerBodyEnd->nType &` |
|        - |  2524 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        3 |  2525 | `						iNestInner++;` |
|      112 |  2526 | `					}else if( pInnerBodyEnd->nType &` |
|        - |  2527 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        3 |  2528 | `						iNestInner--;` |
|        1 |  2529 | `					}` |
|      113 |  2530 | `					pInnerBodyEnd++;` |
|        1 |  2531 | `				}` |
|        - |  2532 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|        - |  2533 | `				 * the outer's body: a default value is evaluated at call time` |
|        - |  2534 | `				 * in the outer frame, so any free variable it references is` |
|        - |  2535 | `				 * an outer capture. We must NOT scan the parameter-name` |
|        - |  2536 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|        - |  2537 | `				 * or those names leak into the outer's closure environment.` |
|        - |  2538 | `				 *` |
|        - |  2539 | `				 * Walk the signature argument-by-argument, splitting on` |
|        - |  2540 | `				 * top-level commas, and for each argument scan only the token` |
|        - |  2541 | `				 * range after the '=' sign. */` |
|        - |  2542 | `				{` |
|       19 |  2543 | `					SyToken *pArgStart = pInnerSigStart;` |
|       31 |  2544 | `					while( pArgStart < pInnerSigEnd ){` |
|       13 |  2545 | `						SyToken *pArgEnd = pArgStart;` |
|       13 |  2546 | `						SyToken *pEq = 0;` |
|       13 |  2547 | `						int iNestArg = 0;` |
|       49 |  2548 | `						while( pArgEnd < pInnerSigEnd ){` |
|       38 |  2549 | `							if( iNestArg == 0` |
|       39 |  2550 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|        3 |  2551 | `								break;` |
|        - |  2552 | `							}` |
|       37 |  2553 | `							if( pArgEnd->nType &` |
|        - |  2554 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      ! 0 |  2555 | `								iNestArg++;` |
|       37 |  2556 | `							}else if( pArgEnd->nType &` |
|        - |  2557 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      ! 0 |  2558 | `								iNestArg--;` |
|      ! 0 |  2559 | `							}` |
|       36 |  2560 | `							if( pEq == 0 && iNestArg == 0` |
|       31 |  2561 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|        7 |  2562 | `								pEq = pArgEnd;` |
|        3 |  2563 | `							}` |
|       37 |  2564 | `							pArgEnd++;` |
|        1 |  2565 | `						}` |
|       13 |  2566 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|       10 |  2567 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|        3 |  2568 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|        7 |  2569 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 |  2570 | `								return SXERR_ABORT;` |
|        - |  2571 | `							}` |
|        3 |  2572 | `						}` |
|       13 |  2573 | `						pArgStart = pArgEnd;` |
|       12 |  2574 | `						if( pArgStart < pInnerSigEnd` |
|        8 |  2575 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|        3 |  2576 | `							pArgStart++;` |
|        1 |  2577 | `						}` |
|        1 |  2578 | `					}` |
|        - |  2579 | `				}` |
|       28 |  2580 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|        9 |  2581 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|       19 |  2582 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  2583 | `					return SXERR_ABORT;` |
|        - |  2584 | `				}` |
|       19 |  2585 | `				pScan = pInnerBodyEnd;` |
|       19 |  2586 | `				continue;` |
|        - |  2587 | `			}` |
|        5 |  2588 | `		}` |
|     1338 |  2589 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     1166 |  2590 | `			pScan++;` |
|     1166 |  2591 | `			continue;` |
|        - |  2592 | `		}` |
|        - |  2593 | `		{` |
|        - |  2594 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|      175 |  2595 | `			SyToken *pDollar = pScan;` |
|      258 |  2596 | `			while( &pDollar[1] < pEnd` |
|      175 |  2597 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|      ! 0 |  2598 | `				pDollar++;` |
|      ! 0 |  2599 | `			}` |
|      175 |  2600 | `			if( &pDollar[1] >= pEnd ){` |
|      ! 0 |  2601 | `				break;` |
|        - |  2602 | `			}` |
|      175 |  2603 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  2604 | `				pScan = pDollar + 1;` |
|      ! 0 |  2605 | `				continue;` |
|        - |  2606 | `			}` |
|      261 |  2607 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|      172 |  2608 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|       86 |  2609 | `				aShadow,nShadow);` |
|      175 |  2610 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2611 | `				return SXERR_ABORT;` |
|        - |  2612 | `			}` |
|      175 |  2613 | `			pScan = pDollar + 2;` |
|        - |  2614 | `		}` |
|        3 |  2615 | `	}` |
|      300 |  2616 | `	return SXRET_OK;` |
|      152 |  2617 | `}` |
|        - |  2618 | `/*` |
|        - |  2619 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|        - |  2620 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|        - |  2621 | ` * variables by value. The body is a single expression that acts as an` |
|        - |  2622 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|        - |  2623 | ` * $this is also made available.` |
|        - |  2624 | ` */` |
|      278 |  2625 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  2626 | `{` |
|        - |  2627 | `	ph7_vm_func *pFunc;` |
|        - |  2628 | `	ph7_vm_func_closure_env sEnv;` |
|        - |  2629 | `	GenBlock *pBlock;` |
|        - |  2630 | `	SySet *pInstrContainer;` |
|        - |  2631 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|        - |  2632 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|        - |  2633 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|        - |  2634 | `	SyToken *pSavedEnd;` |
|        - |  2635 | `	ph7_vm_func_arg *aArgs;` |
|        - |  2636 | `	char zName[512];` |
|        - |  2637 | `	static int iCnt = 1;` |
|        - |  2638 | `	char *zDup;` |
|        - |  2639 | `	SyToken *pTokKw;` |
|        - |  2640 | `	sxu32 nLen;` |
|        - |  2641 | `	sxu32 nLine;` |
|      283 |  2642 | `	sxi32 iFlags = 0;` |
|      283 |  2643 | `	int bStatic = 0;` |
|        - |  2644 | `	sxi32 rc;` |
|        - |  2645 | `	sxu32 n;` |
|      139 |  2646 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  2647 |  |
|      283 |  2648 | `	nLine = pGen->pIn->nLine;` |
|        - |  2649 | ``	/* Attribute-sidecar key: `#[A] [static] fn` trivia is keyed to this token */`` |
|      283 |  2650 | `	pTokKw = pGen->pIn;` |
|        - |  2651 | `	/* Optional 'static' prefix */` |
|      278 |  2652 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      283 |  2653 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        7 |  2654 | `		bStatic = 1;` |
|        7 |  2655 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|        7 |  2656 | `		pGen->pIn++;` |
|        3 |  2657 | `	}` |
|        - |  2658 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|      278 |  2659 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      283 |  2660 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|      ! 0 |  2661 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2662 | `			"Arrow function: expected 'fn' keyword");` |
|      ! 0 |  2663 | `		return SXERR_SYNTAX;` |
|        - |  2664 | `	}` |
|      283 |  2665 | `	pGen->pIn++; /* Jump 'fn' */` |
|        - |  2666 | `	/* Optional '&' — return by reference */` |
|      283 |  2667 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  2668 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|      ! 0 |  2669 | `		pGen->pIn++;` |
|      ! 0 |  2670 | `	}` |
|        - |  2671 | `	/* Expect '(' */` |
|      283 |  2672 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        3 |  2673 | `		if( pGen->pIn < pGen->pEnd ){` |
|        4 |  2674 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - |  2675 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|        2 |  2676 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        2 |  2677 | `		}else{` |
|      ! 0 |  2678 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  2679 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|        - |  2680 | `		}` |
|        3 |  2681 | `		return SXERR_SYNTAX;` |
|        - |  2682 | `	}` |
|      281 |  2683 | `	pGen->pIn++; /* Jump '(' */` |
|        - |  2684 | `	/* Delimit the parameter list */` |
|      281 |  2685 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|      281 |  2686 | `	if( pSigEnd >= pGen->pEnd ){` |
|        3 |  2687 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  2688 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|        3 |  2689 | `		return SXERR_SYNTAX;` |
|        - |  2690 | `	}` |
|        - |  2691 | `	/* Allocate the function state */` |
|      279 |  2692 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|      279 |  2693 | `	if( pFunc == 0 ){` |
|      ! 0 |  2694 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2695 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2696 | `		return SXERR_ABORT;` |
|        - |  2697 | `	}` |
|        - |  2698 | `	/* Generate a unique lambda name */` |
|      279 |  2699 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      279 |  2700 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 |  2701 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      ! 0 |  2702 | `	}` |
|      279 |  2703 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|      279 |  2704 | `	if( zDup == 0 ){` |
|      ! 0 |  2705 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2706 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2707 | `		return SXERR_ABORT;` |
|        - |  2708 | `	}` |
|      279 |  2709 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|        - |  2710 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|      279 |  2711 | `	pFunc->nLine = nLine;` |
|        - |  2712 | ``	/* Expression-position attributes (`$f = #[A] fn () => …`) */`` |
|      279 |  2713 | `	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  2714 | `		return SXERR_ABORT;` |
|        - |  2715 | `	}` |
|        - |  2716 | `	/* Collect function arguments */` |
|      279 |  2717 | `	if( pGen->pIn < pSigEnd ){` |
|      110 |  2718 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|      110 |  2719 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  2720 | `			return SXERR_ABORT;` |
|        - |  2721 | `		}` |
|       53 |  2722 | `	}` |
|        - |  2723 | `	/* Point past ')' and parse optional return type */` |
|      279 |  2724 | `	pGen->pIn = &pSigEnd[1];` |
|      279 |  2725 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|      279 |  2726 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2727 | `		return SXERR_ABORT;` |
|      279 |  2728 | `	}else if( rc == SXERR_SYNTAX ){` |
|      ! 0 |  2729 | `		return SXERR_SYNTAX;` |
|        - |  2730 | `	}` |
|        - |  2731 | `	/* Expect '=>' */` |
|      279 |  2732 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|        3 |  2733 | `		if( pGen->pIn < pGen->pEnd ){` |
|        4 |  2734 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - |  2735 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|        2 |  2736 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        2 |  2737 | `		}else{` |
|      ! 0 |  2738 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  2739 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|        - |  2740 | `		}` |
|        3 |  2741 | `		return SXERR_SYNTAX;` |
|        - |  2742 | `	}` |
|      276 |  2743 | `	pGen->pIn++; /* Jump '=>' */` |
|      276 |  2744 | `	pBodyStart = pGen->pIn;` |
|      276 |  2745 | `	pBodyEnd = pGen->pEnd;` |
|        - |  2746 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|        - |  2747 | `	 * recursively collect free-variable references from the body. The scan` |
|        - |  2748 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|        - |  2749 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|      276 |  2750 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|        - |  2751 | `	{` |
|      276 |  2752 | `		SyString *aShadow = 0;` |
|      276 |  2753 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|      276 |  2754 | `		if( nShadow > 0 ){` |
|      107 |  2755 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      104 |  2756 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|      107 |  2757 | `			if( aShadow == 0 ){` |
|      ! 0 |  2758 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2759 | `					"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2760 | `				return SXERR_ABORT;` |
|        - |  2761 | `			}` |
|      239 |  2762 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|      135 |  2763 | `				aShadow[n] = aArgs[n].sName;` |
|       69 |  2764 | `			}` |
|       52 |  2765 | `		}` |
|      412 |  2766 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      136 |  2767 | `			aShadow,nShadow);` |
|      276 |  2768 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  2769 | `			return SXERR_ABORT;` |
|        - |  2770 | `		}` |
|        - |  2771 | `	}` |
|        - |  2772 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|        - |  2773 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|        - |  2774 | `	 * captured value is silently dropped when the enclosing scope has no` |
|        - |  2775 | `	 * $this. */` |
|      276 |  2776 | `	if( !bStatic ){` |
|        - |  2777 | `		char *zThisDup;` |
|      270 |  2778 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|      270 |  2779 | `		if( zThisDup == 0 ){` |
|      ! 0 |  2780 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2781 | `				"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2782 | `			return SXERR_ABORT;` |
|        - |  2783 | `		}` |
|      270 |  2784 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      270 |  2785 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|      270 |  2786 | `		sEnv.nIdx = SXU32_HIGH;` |
|      270 |  2787 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      270 |  2788 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|      270 |  2789 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      133 |  2790 | `	}` |
|        - |  2791 | `	/* Arrow functions are always closures */` |
|      276 |  2792 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|        - |  2793 | `	/* Compile the body expression as an implicit return */` |
|      412 |  2794 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      136 |  2795 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|      276 |  2796 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  2797 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2798 | `			"PH7 engine is running out-of-memory");` |
|      ! 0 |  2799 | `		return SXERR_ABORT;` |
|        - |  2800 | `	}` |
|      276 |  2801 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      276 |  2802 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|      276 |  2803 | `	pSavedEnd = pGen->pEnd;` |
|      276 |  2804 | `	pGen->pIn = pBodyStart;` |
|      276 |  2805 | `	pGen->pEnd = pBodyEnd;` |
|      276 |  2806 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      276 |  2807 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2808 | `		return SXERR_ABORT;` |
|        - |  2809 | `	}` |
|        - |  2810 | `	/* The cursor stopped just past the body expression */` |
|      276 |  2811 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|        - |  2812 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|        - |  2813 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|        - |  2814 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|        - |  2815 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|      276 |  2816 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      276 |  2817 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      276 |  2818 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      276 |  2819 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      276 |  2820 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - |  2821 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|      276 |  2822 | `	pGen->pIn = pBodyEnd;` |
|      276 |  2823 | `	pGen->pEnd = pSavedEnd;` |
|        - |  2824 | `	/* Emit the load-closure instruction */` |
|      276 |  2825 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|      276 |  2826 | `	return SXRET_OK;` |
|      144 |  2827 | `}` |
|        - |  2828 | `/*` |
|        - |  2829 | ` * Compile a single arm's expression range into a freshly-allocated` |
|        - |  2830 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|        - |  2831 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|        - |  2832 | ` * expression's value.` |
|        - |  2833 | ` */` |
|      354 |  2834 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|        - |  2835 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|        3 |  2836 | `{` |
|        - |  2837 | `	SySet *pInstrContainer;` |
|        - |  2838 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |  2839 | `	GenBlock *pArmBlock;` |
|        - |  2840 | `	sxi32 rc;` |
|      357 |  2841 | `	pTmpIn  = pGen->pIn;` |
|      357 |  2842 | `	pTmpEnd = pGen->pEnd;` |
|      357 |  2843 | `	pGen->pIn  = pStart;` |
|      357 |  2844 | `	pGen->pEnd = pStop;` |
|      357 |  2845 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      357 |  2846 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|        - |  2847 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|        - |  2848 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|        - |  2849 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|        - |  2850 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|        - |  2851 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|      534 |  2852 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      177 |  2853 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|      357 |  2854 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  2855 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  2856 | `		pGen->pIn  = pTmpIn;` |
|      ! 0 |  2857 | `		pGen->pEnd = pTmpEnd;` |
|      ! 0 |  2858 | `		return SXERR_ABORT;` |
|        - |  2859 | `	}` |
|      357 |  2860 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      357 |  2861 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      357 |  2862 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      357 |  2863 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      357 |  2864 | `	GenStateLeaveBlock(&(*pGen),0);` |
|      357 |  2865 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      357 |  2866 | `	pGen->pIn  = pTmpIn;` |
|      357 |  2867 | `	pGen->pEnd = pTmpEnd;` |
|      357 |  2868 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2869 | `		return SXERR_ABORT;` |
|        - |  2870 | `	}` |
|      357 |  2871 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 |  2872 | `		return SXERR_EMPTY;` |
|        - |  2873 | `	}` |
|      357 |  2874 | `	return SXRET_OK;` |
|      180 |  2875 | `}` |
|        - |  2876 | `/*` |
|        - |  2877 | ` * Compile a PHP 8.0 match expression:` |
|        - |  2878 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|        - |  2879 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|        - |  2880 | ` * Strict comparison (===) is used between the subject and each condition.` |
|        - |  2881 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|        - |  2882 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|        - |  2883 | ` */` |
|        - |  2884 | `/*` |
|        - |  2885 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|        - |  2886 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|        - |  2887 | ` * caller can bail out of the current expression.` |
|        - |  2888 | ` */` |
|        2 |  2889 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|        1 |  2890 | `{` |
|        - |  2891 | `	va_list ap;` |
|        - |  2892 | `	sxi32 rc;` |
|        - |  2893 | `	SyBlob sMsg;` |
|        3 |  2894 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|        3 |  2895 | `	va_start(ap,zFmt);` |
|        3 |  2896 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|        3 |  2897 | `	va_end(ap);` |
|        3 |  2898 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|        3 |  2899 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|        3 |  2900 | `	SyBlobRelease(&sMsg);` |
|        3 |  2901 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2902 | `		return SXERR_ABORT;` |
|        - |  2903 | `	}` |
|        3 |  2904 | `	return SXERR_SYNTAX;` |
|        2 |  2905 | `}` |
|        - |  2906 | `/*` |
|        - |  2907 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|        - |  2908 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|        - |  2909 | ` * Returns the stop token pointer (or pEnd if none found).` |
|        - |  2910 | ` */` |
|      356 |  2911 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|        4 |  2912 | `{` |
|      360 |  2913 | `	SyToken *pCur = pStart;` |
|      360 |  2914 | `	int iNest = 0;` |
|      838 |  2915 | `	while( pCur < pEnd ){` |
|      802 |  2916 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       13 |  2917 | `			iNest++;` |
|      796 |  2918 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       13 |  2919 | `			iNest--;` |
|      784 |  2920 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|      323 |  2921 | `			return pCur;` |
|        - |  2922 | `		}` |
|      482 |  2923 | `		pCur++;` |
|        4 |  2924 | `	}` |
|       39 |  2925 | `	return pEnd;` |
|      182 |  2926 | `}` |
|       72 |  2927 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  2928 | `{` |
|        - |  2929 | `	ph7_match *pMatch;` |
|        - |  2930 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|       77 |  2931 | `	int bHasDefault = 0;` |
|        - |  2932 | `	sxu32 nLine;` |
|        - |  2933 | `	sxi32 rc;` |
|       36 |  2934 | `	SXUNUSED(iCompileFlag);` |
|       77 |  2935 | `	nLine = pGen->pIn->nLine;` |
|       77 |  2936 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|        - |  2937 | `	/* Expect '(' */` |
|       77 |  2938 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 |  2939 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  2940 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|      ! 0 |  2941 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|        - |  2942 | `	}` |
|       77 |  2943 | `	pGen->pIn++; /* Jump '(' */` |
|       77 |  2944 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|       77 |  2945 | `	if( pSubjEnd >= pGen->pEnd ){` |
|      ! 0 |  2946 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  2947 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|        - |  2948 | `	}` |
|       77 |  2949 | `	if( pGen->pIn >= pSubjEnd ){` |
|      ! 0 |  2950 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  2951 | `			"syntax error, unexpected \")\", expecting match subject");` |
|        - |  2952 | `	}` |
|        - |  2953 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|       77 |  2954 | `	pSavedEnd = pGen->pEnd;` |
|       77 |  2955 | `	pGen->pEnd = pSubjEnd;` |
|       77 |  2956 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       77 |  2957 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2958 | `		return SXERR_ABORT;` |
|        - |  2959 | `	}` |
|       77 |  2960 | `	pGen->pEnd = pSavedEnd;` |
|       77 |  2961 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|        - |  2962 | `	/* Expect '{' */` |
|       77 |  2963 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|      ! 0 |  2964 | `		return GenStateMatchError(pGen,` |
|      ! 0 |  2965 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|        - |  2966 | `			"syntax error, expecting \"{\" after match subject");` |
|        - |  2967 | `	}` |
|       77 |  2968 | `	pGen->pIn++; /* Jump '{' */` |
|       77 |  2969 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|       77 |  2970 | `	if( pBodyEnd >= pGen->pEnd ){` |
|      ! 0 |  2971 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  2972 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|        - |  2973 | `	}` |
|        - |  2974 | `	/* Allocate ph7_match container */` |
|       77 |  2975 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|       77 |  2976 | `	if( pMatch == 0 ){` |
|      ! 0 |  2977 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2978 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2979 | `		return SXERR_ABORT;` |
|        - |  2980 | `	}` |
|       77 |  2981 | `	SyZero(pMatch,sizeof(ph7_match));` |
|       77 |  2982 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|        - |  2983 | `	/* Iterate arms */` |
|      259 |  2984 | `	while( pGen->pIn < pBodyEnd ){` |
|        - |  2985 | `		ph7_match_arm sArm;` |
|        - |  2986 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|      190 |  2987 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|      190 |  2988 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|      190 |  2989 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|      190 |  2990 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - |  2991 | `		/* 'default' arm? */` |
|      186 |  2992 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      107 |  2993 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|       22 |  2994 | `			if( bHasDefault ){` |
|        3 |  2995 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|        - |  2996 | `					"Match expressions may only contain one default arm");` |
|        4 |  2997 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  2998 | `			}` |
|       20 |  2999 | `			sArm.bDefault = 1;` |
|       20 |  3000 | `			bHasDefault = 1;` |
|       20 |  3001 | `			pGen->pIn++;` |
|       20 |  3002 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|      ! 0 |  3003 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  3004 | `					"syntax error, expecting \"=>\" after 'default'");` |
|        - |  3005 | `			}` |
|       20 |  3006 | `			pGen->pIn++; /* Jump '=>' */` |
|       11 |  3007 | `		}else{` |
|        - |  3008 | `			/* Condition list: cond (',' cond)* '=>' */` |
|      170 |  3009 | `			pCondStart = pGen->pIn;` |
|      170 |  3010 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|        - |  3011 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|      178 |  3012 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|        - |  3013 | `				SySet sCondBc;` |
|        9 |  3014 | `				if( pCondStart >= pArrow ){` |
|      ! 0 |  3015 | `					return GenStateMatchError(pGen,nArmLine,` |
|        - |  3016 | `						"syntax error, empty match condition expression");` |
|        - |  3017 | `				}` |
|        9 |  3018 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        9 |  3019 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|        9 |  3020 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  3021 | `					return SXERR_ABORT;` |
|        - |  3022 | `				}` |
|        9 |  3023 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|        9 |  3024 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|        9 |  3025 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|        - |  3026 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|        1 |  3027 | `			}` |
|      170 |  3028 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|        3 |  3029 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  3030 | `					"syntax error, expecting \"=>\" in match arm");` |
|        - |  3031 | `			}` |
|      167 |  3032 | `			if( pCondStart >= pArrow ){` |
|      ! 0 |  3033 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  3034 | `					"syntax error, empty match condition expression");` |
|        - |  3035 | `			}` |
|        - |  3036 | `			{` |
|        - |  3037 | `				SySet sCondBc;` |
|      167 |  3038 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      167 |  3039 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|      167 |  3040 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  3041 | `					return SXERR_ABORT;` |
|        - |  3042 | `				}` |
|      167 |  3043 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|        - |  3044 | `			}` |
|      167 |  3045 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|        - |  3046 | `		}` |
|        - |  3047 | `		/* Compile result expression: up to top-level ',' or body end */` |
|      185 |  3048 | `		pResStart = pGen->pIn;` |
|      185 |  3049 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|      185 |  3050 | `		if( pResStart >= pResEnd ){` |
|      ! 0 |  3051 | `			return GenStateMatchError(pGen,nArmLine,` |
|        - |  3052 | `				"syntax error, expected expression after \"=>\"");` |
|        - |  3053 | `		}` |
|      185 |  3054 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|      185 |  3055 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3056 | `			return SXERR_ABORT;` |
|        - |  3057 | `		}` |
|      185 |  3058 | `		pGen->pIn = pResEnd;` |
|      185 |  3059 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      151 |  3060 | `			pGen->pIn++; /* Skip trailing ',' */` |
|       74 |  3061 | `		}` |
|      185 |  3062 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|        3 |  3063 | `	}` |
|       71 |  3064 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|       71 |  3065 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|       71 |  3066 | `	return SXRET_OK;` |
|       41 |  3067 | `}` |
|        - |  3068 | `/*` |
|        - |  3069 | ` * Compile a backtick quoted string.` |
|        - |  3070 | ` */` |
|        4 |  3071 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        2 |  3072 | `{` |
|        - |  3073 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|        - |  3074 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|        - |  3075 | `	 */` |
|        8 |  3076 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|        - |  3077 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|        2 |  3078 | `		ph7_lib_version()` |
|        - |  3079 | `		);` |
|        - |  3080 | `	/* Load NULL */` |
|        6 |  3081 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        2 |  3082 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  3083 | `	/* Node successfully compiled */` |
|        6 |  3084 | `	return SXRET_OK;` |
|        2 |  3085 | `}` |
|        - |  3086 | `/*` |
|        - |  3087 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|        - |  3088 | ` * construct.` |
|        - |  3089 | ` */` |
|       82 |  3090 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  3091 | `{` |
|        - |  3092 | `	SyString *pName;` |
|        - |  3093 | `	sxu32 nKeyID;` |
|        - |  3094 | `	sxi32 rc;` |
|        - |  3095 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|       87 |  3096 | `	pName = &pGen->pIn->sData;` |
|       87 |  3097 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       87 |  3098 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|       87 |  3099 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|        9 |  3100 | `		SyToken *pTmp,*pNext = 0;` |
|        - |  3101 | `		/* Compile arguments one after one */` |
|        9 |  3102 | `		pTmp = pGen->pEnd;` |
|        - |  3103 | `		/* Symisc eXtension to the PHP programming language:` |
|        - |  3104 | `		 * 'echo' can be used in the context of a function which` |
|        - |  3105 | `		 *  mean that the following expression is valid:` |
|        - |  3106 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|        - |  3107 | `		 */` |
|        9 |  3108 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|       17 |  3109 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|        9 |  3110 | `			if( pGen->pIn < pNext ){` |
|        9 |  3111 | `				pGen->pEnd = pNext;` |
|        9 |  3112 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|        9 |  3113 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  3114 | `					return SXERR_ABORT;` |
|        - |  3115 | `				}` |
|        9 |  3116 | `				if( rc != SXERR_EMPTY ){` |
|        - |  3117 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|        - |  3118 | `					 * without the overhead of a function call.` |
|        - |  3119 | `					 * This is a very powerful optimization that improve` |
|        - |  3120 | `					 * performance greatly.` |
|        - |  3121 | `					 */` |
|        9 |  3122 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|        4 |  3123 | `				}` |
|        4 |  3124 | `			}` |
|        - |  3125 | `			/* Jump trailing commas */` |
|        9 |  3126 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|      ! 0 |  3127 | `				pNext++;` |
|      ! 0 |  3128 | `			}` |
|        9 |  3129 | `			pGen->pIn = pNext;` |
|        1 |  3130 | `		}` |
|        - |  3131 | `		/* Restore token stream */` |
|        9 |  3132 | `		pGen->pEnd = pTmp;` |
|        5 |  3133 | `	}else{` |
|       79 |  3134 | `		sxi32 nArg = 0;` |
|       79 |  3135 | `		sxu32 nIdx = 0;` |
|       79 |  3136 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|       79 |  3137 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3138 | `			return SXERR_ABORT;` |
|       79 |  3139 | `		}else if(rc != SXERR_EMPTY ){` |
|       79 |  3140 | `			nArg = 1;` |
|       37 |  3141 | `		}` |
|       79 |  3142 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|        - |  3143 | `			ph7_value *pObj;` |
|        - |  3144 | `			/* Emit the call instruction */` |
|       31 |  3145 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       31 |  3146 | `			if( pObj == 0 ){` |
|      ! 0 |  3147 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3148 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  3149 | `				return SXERR_ABORT;` |
|        - |  3150 | `			}` |
|       31 |  3151 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|        - |  3152 | `			/* Install in the literal table */` |
|       31 |  3153 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       13 |  3154 | `		}` |
|        - |  3155 | `		/* Emit the call instruction */` |
|       79 |  3156 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       79 |  3157 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        - |  3158 | `	}` |
|        - |  3159 | `	/* Node successfully compiled */` |
|       87 |  3160 | `	return SXRET_OK;` |
|       46 |  3161 | `}` |
|        - |  3162 | `/*` |
|        - |  3163 | ` * Compile a node holding a variable declaration.` |
|        - |  3164 | ` * According to the PHP language reference` |
|        - |  3165 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|        - |  3166 | ` *  The variable name is case-sensitive.` |
|        - |  3167 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|        - |  3168 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|        - |  3169 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|        - |  3170 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|        - |  3171 | ` *  Note: $this is a special variable that can't be assigned.` |
|        - |  3172 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|        - |  3173 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|        - |  3174 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|        - |  3175 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|        - |  3176 | ` *  the chapter on Expressions.` |
|        - |  3177 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|        - |  3178 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|        - |  3179 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|        - |  3180 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|        - |  3181 | ` *  is being assigned (the source variable).` |
|        - |  3182 | ` */` |
|  8815024 |  3183 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  3184 | `{` |
|  8815029 |  3185 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |  3186 | `	sxi32 iVv;` |
|        - |  3187 | `	sxi32 iP1;` |
|        - |  3188 | `	void *p3;` |
|        - |  3189 | `	sxi32 rc;` |
|  8815029 |  3190 | `	iVv = -1; /* Variable variable counter */` |
| 17630065 |  3191 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  8815041 |  3192 | `		pGen->pIn++;` |
|  8815041 |  3193 | `		iVv++;` |
|        5 |  3194 | `	}` |
|  8815029 |  3195 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|        - |  3196 | `		/* Invalid variable name */` |
|      ! 0 |  3197 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|      ! 0 |  3198 | `		if( rc == SXERR_ABORT ){` |
|        - |  3199 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3200 | `			return SXERR_ABORT;` |
|        - |  3201 | `		}` |
|      ! 0 |  3202 | `		return SXRET_OK;` |
|        - |  3203 | `	}` |
|  8815029 |  3204 | `	p3  = 0;` |
|  8815029 |  3205 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|        - |  3206 | `		/* Dynamic variable creation */` |
|       21 |  3207 | `		pGen->pIn++;  /* Jump the open curly */` |
|       21 |  3208 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|       21 |  3209 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - |  3210 | `			/* Empty expression */` |
|        3 |  3211 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|        3 |  3212 | `			return SXRET_OK;` |
|        - |  3213 | `		}` |
|        - |  3214 | `		/* Compile the expression holding the variable name */` |
|       18 |  3215 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       18 |  3216 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3217 | `			return SXERR_ABORT;` |
|       18 |  3218 | `		}else if( rc == SXERR_EMPTY ){` |
|        3 |  3219 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|        3 |  3220 | `			return SXRET_OK;` |
|        - |  3221 | `		}` |
|        8 |  3222 | `	}else{` |
|        - |  3223 | `		SyHashEntry *pEntry;` |
|        - |  3224 | `		SyString *pName;` |
|  8815011 |  3225 | `		char *zName = 0;` |
|        - |  3226 | `		/* Extract variable name */` |
|  8815011 |  3227 | `		pName = &pGen->pIn->sData;` |
|        - |  3228 | `		/* Advance the stream cursor */` |
|  8815011 |  3229 | `		pGen->pIn++;` |
|  8815011 |  3230 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  8815011 |  3231 | `		if( pEntry == 0 ){` |
|        - |  3232 | `			/* Duplicate name */` |
|   562841 |  3233 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   562841 |  3234 | `			if( zName == 0 ){` |
|      ! 0 |  3235 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3236 | `				return SXERR_ABORT;` |
|        - |  3237 | `			}` |
|        - |  3238 | `			/* Install in the hashtable */` |
|   562841 |  3239 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   281423 |  3240 | `		}else{` |
|        - |  3241 | `			/* Name already available */` |
|  8252175 |  3242 | `			zName = (char *)pEntry->pUserData;` |
|        - |  3243 | `		}` |
|  8815011 |  3244 | `		p3 = (void *)zName;` |
|        - |  3245 | `	}` |
|  8815025 |  3246 | `	iP1 = 0;` |
|  8815025 |  3247 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  2667631 |  3248 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|        - |  3249 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  2667613 |  3250 | `			iP1 = 1;` |
|  1333804 |  3251 | `		}` |
|  1333813 |  3252 | `	}` |
|        - |  3253 | `	/* Emit the load instruction */` |
|  8815025 |  3254 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  8815037 |  3255 | `	while( iVv > 0 ){` |
|       13 |  3256 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|       13 |  3257 | `		iVv--;` |
|        1 |  3258 | `	}` |
|        - |  3259 | `	/* Node successfully compiled */` |
|  8815025 |  3260 | `	return SXRET_OK;` |
|  4407517 |  3261 | `}` |
|        - |  3262 | `/*` |
|        - |  3263 | ` * Load a literal.` |
|        - |  3264 | ` */` |
|  5596868 |  3265 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|        5 |  3266 | `{` |
|  5596873 |  3267 | `	SyToken *pToken = pGen->pIn;` |
|        - |  3268 | `	ph7_value *pObj;` |
|        - |  3269 | `	SyString *pStr;` |
|        - |  3270 | `	sxu32 nIdx;` |
|        - |  3271 | `	/* Extract token value */` |
|  5596873 |  3272 | `	pStr = &pToken->sData;` |
|        - |  3273 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  5596873 |  3274 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  1362969 |  3275 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|        - |  3276 | `			/* NULL constant are always indexed at 0 */` |
|   560195 |  3277 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   560195 |  3278 | `			return SXRET_OK;` |
|   802779 |  3279 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|        - |  3280 | `			/* TRUE constant are always indexed at 1 */` |
|   148577 |  3281 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|   148577 |  3282 | `			return SXRET_OK;` |
|        5 |  3283 | `		}` |
|  5034801 |  3284 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   947582 |  3285 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|        - |  3286 | `			/* FALSE constant are always indexed at 2 */` |
|   400695 |  3287 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   400695 |  3288 | `			return SXRET_OK;` |
|  4115611 |  3289 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   564784 |  3290 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|        - |  3291 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    11663 |  3292 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    11663 |  3293 | `			if( pObj == 0 ){` |
|      ! 0 |  3294 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3295 | `				return SXERR_ABORT;` |
|        - |  3296 | `			}` |
|    11663 |  3297 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|        - |  3298 | `			/* Emit the load constant instruction */` |
|    11663 |  3299 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    11663 |  3300 | `			return SXRET_OK;` |
|  3850989 |  3301 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|    58856 |  3302 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|        - |  3303 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|        8 |  3304 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        8 |  3305 | `			if( pObj == 0 ){` |
|      ! 0 |  3306 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3307 | `				return SXERR_ABORT;` |
|        - |  3308 | `			}` |
|        8 |  3309 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        - |  3310 | `				SyString sNs;` |
|        8 |  3311 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        8 |  3312 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|        5 |  3313 | `			}else{` |
|      ! 0 |  3314 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|        - |  3315 | `			}` |
|        8 |  3316 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        8 |  3317 | `			return SXRET_OK;` |
|  3843437 |  3318 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   151992 |  3319 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  3929756 |  3320 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   216426 |  3321 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|       11 |  3322 | `			GenBlock *pBlock = pGen->pCurrent;` |
|        - |  3323 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|       21 |  3324 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|        - |  3325 | `				/* Point to the upper block */` |
|       11 |  3326 | `				pBlock = pBlock->pParent;` |
|        1 |  3327 | `			}` |
|       11 |  3328 | `			if( pBlock == 0 ){` |
|        - |  3329 | `				/* Called in the global scope,load NULL */` |
|        5 |  3330 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        3 |  3331 | `			}else{` |
|        - |  3332 | `				/* Extract the target function/method */` |
|        7 |  3333 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        7 |  3334 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|        - |  3335 | `					/* Not a class method,Load null */` |
|        3 |  3336 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        2 |  3337 | `				}else{` |
|        5 |  3338 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        5 |  3339 | `					if( pObj == 0 ){` |
|      ! 0 |  3340 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3341 | `						return SXERR_ABORT;` |
|        - |  3342 | `					}` |
|        5 |  3343 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|        - |  3344 | `					/* Emit the load constant instruction */` |
|        5 |  3345 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - |  3346 | `				}` |
|        - |  3347 | `			}` |
|       11 |  3348 | `			return SXRET_OK;` |
|        - |  3349 | `	}` |
|        - |  3350 | `	/* Query literal table */` |
|  4475747 |  3351 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|        - |  3352 | `		ph7_value *pLitObj;` |
|        - |  3353 | `		/* Unknown literal,install it in the literal table */` |
|   908197 |  3354 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   908197 |  3355 | `		if( pLitObj == 0 ){` |
|      ! 0 |  3356 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  3357 | `			return SXERR_ABORT;` |
|        - |  3358 | `		}` |
|   908197 |  3359 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|   908197 |  3360 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|   454096 |  3361 | `	}` |
|        - |  3362 | `	/* Emit the load constant instruction */` |
|  4475747 |  3363 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  4475747 |  3364 | `	return SXRET_OK;` |
|  2798439 |  3365 | `}` |
|        - |  3366 | `/*` |
|        - |  3367 | ` * Resolve a namespace path or simply load a literal.` |
|        - |  3368 | ` * If the token stream contains namespace separators (backslashes),` |
|        - |  3369 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|        - |  3370 | ` * Otherwise, load the simple literal directly.` |
|        - |  3371 | ` */` |
|  5600800 |  3372 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|        5 |  3373 | `{` |
|        - |  3374 | `	sxi32 rc;` |
|  5600805 |  3375 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  3376 | `		return SXRET_OK;` |
|        - |  3377 | `	}` |
|        - |  3378 | `	/* Check if this is a multi-token namespace path */` |
|  5600805 |  3379 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|        - |  3380 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|     3937 |  3381 | `		SyBlob *pWorker = &pGen->sWorker;` |
|     3937 |  3382 | `		int isAbsolute = 0;` |
|     3937 |  3383 | `		SyBlobReset(pWorker);` |
|        - |  3384 | `		/* Check for leading backslash (absolute path) */` |
|     3937 |  3385 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|     3935 |  3386 | `			isAbsolute = 1;` |
|     3935 |  3387 | `			pGen->pIn++; /* Skip leading backslash */` |
|     1965 |  3388 | `		}` |
|        - |  3389 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|     3937 |  3390 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        3 |  3391 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        3 |  3392 | `			SyBlobAppend(pWorker,"\\",1);` |
|        1 |  3393 | `		}` |
|        - |  3394 | `		/* Collect all path components */` |
|     4045 |  3395 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     4045 |  3396 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       58 |  3397 | `				SyBlobAppend(pWorker,"\\",1);` |
|       31 |  3398 | `			}else{` |
|     3991 |  3399 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - |  3400 | `			}` |
|     4045 |  3401 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|     3937 |  3402 | `				pGen->pIn++;` |
|     3937 |  3403 | `				break;` |
|        - |  3404 | `			}` |
|      112 |  3405 | `			pGen->pIn++;` |
|        4 |  3406 | `		}` |
|     3937 |  3407 | `		if( SyBlobLength(pWorker) > 0 ){` |
|        - |  3408 | `			ph7_value *pObj;` |
|        - |  3409 | `			SyString sPath;` |
|        - |  3410 | `			sxu32 nIdx;` |
|     3937 |  3411 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|        - |  3412 | `			/* Install in the literal table */` |
|     3937 |  3413 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|     3909 |  3414 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     3909 |  3415 | `				if( pObj == 0 ){` |
|      ! 0 |  3416 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  3417 | `					return SXERR_ABORT;` |
|        - |  3418 | `				}` |
|     3909 |  3419 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|     3909 |  3420 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     1952 |  3421 | `			}` |
|        - |  3422 | `			/* Emit the load constant instruction.` |
|        - |  3423 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|        - |  3424 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|     5903 |  3425 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|     1966 |  3426 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|     1966 |  3427 | `				nIdx,0,0);` |
|     3937 |  3428 | `			return SXRET_OK;` |
|        - |  3429 | `		}` |
|      ! 0 |  3430 | `	}` |
|        - |  3431 | `	/* Single-token literal: load directly */` |
|  5596873 |  3432 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  5596873 |  3433 | `	return rc;` |
|  2800405 |  3434 | `}` |
|        - |  3435 | `/*` |
|        - |  3436 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|        - |  3437 | ` */` |
|        - |  3438 | `/*` |
|        - |  3439 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|        - |  3440 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|        - |  3441 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|        - |  3442 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|        - |  3443 | ` */` |
|      ! 0 |  3444 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|      ! 0 |  3445 | `{` |
|      ! 0 |  3446 | `	SXUNUSED(iCompileFlag);` |
|      ! 0 |  3447 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|        - |  3448 | `		"Cannot use the first-class callable syntax '...' here");` |
|      ! 0 |  3449 | `	return SXERR_SYNTAX;` |
|      ! 0 |  3450 | `}` |
|  5600800 |  3451 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  3452 | `{` |
|        - |  3453 | `	sxi32 rc;` |
|  5600805 |  3454 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  5600805 |  3455 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  3456 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  3457 | `		return rc;` |
|        - |  3458 | `	}` |
|        - |  3459 | `	/* Node successfully compiled */` |
|  5600805 |  3460 | `	return SXRET_OK;` |
|  2800405 |  3461 | `}` |
|        - |  3462 | `/*` |
|        - |  3463 | ` * Recover from a compile-time error. In other words synchronize` |
|        - |  3464 | ` * the token stream cursor with the first semi-colon seen.` |
|        - |  3465 | ` */` |
|        8 |  3466 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|        1 |  3467 | `{` |
|        - |  3468 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|       17 |  3469 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|        9 |  3470 | `		pGen->pIn++;` |
|        1 |  3471 | `	}` |
|        9 |  3472 | `	return SXRET_OK;` |
|        1 |  3473 | `}` |
|        - |  3474 | `/*` |
|        - |  3475 | ` * Check if the given identifier name is reserved or not.` |
|        - |  3476 | ` * Return TRUE if reserved.FALSE otherwise.` |
|        - |  3477 | ` */` |
|   143928 |  3478 | `static int GenStateIsReservedConstant(SyString *pName)` |
|        5 |  3479 | `{` |
|   143933 |  3480 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|       48 |  3481 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|        3 |  3482 | `			return TRUE;` |
|       46 |  3483 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|        6 |  3484 | `			return TRUE;` |
|        3 |  3485 | `		}` |
|   143908 |  3486 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       22 |  3487 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|        3 |  3488 | `			return TRUE;` |
|        - |  3489 | `		}` |
|        9 |  3490 | `	}` |
|        - |  3491 | `	/* Not a reserved constant */` |
|   143925 |  3492 | `	return FALSE;` |
|    71969 |  3493 | `}` |
|        - |  3494 | `/*` |
|        - |  3495 | ` * Compile the 'const' statement.` |
|        - |  3496 | ` * According to the PHP language reference` |
|        - |  3497 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|        - |  3498 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|        - |  3499 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|        - |  3500 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|        - |  3501 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|        - |  3502 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|        - |  3503 | ` *  Syntax` |
|        - |  3504 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|        - |  3505 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|        - |  3506 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|        - |  3507 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|        - |  3508 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|        - |  3509 | ` *  to get a list of all defined constants.` |
|        - |  3510 | ` *` |
|        - |  3511 | ` * Symisc eXtension.` |
|        - |  3512 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|        - |  3513 | ` *  would allow only simple scalar value.` |
|        - |  3514 | ` *  Example` |
|        - |  3515 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|        - |  3516 | ` *    Refer to the official documentation for more information on this feature.` |
|        - |  3517 | ` */` |
|       48 |  3518 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|        5 |  3519 | `{` |
|        - |  3520 | `	SySet *pConsCode,*pInstrContainer;` |
|       53 |  3521 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |  3522 | `	SyString *pName;` |
|        - |  3523 | `	sxi32 rc;` |
|       53 |  3524 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       53 |  3525 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  3526 | `		/* Invalid constant name */` |
|        8 |  3527 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|        8 |  3528 | `		if( rc == SXERR_ABORT ){` |
|        - |  3529 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3530 | `			return SXERR_ABORT;` |
|        - |  3531 | `		}` |
|        8 |  3532 | `		goto Synchronize;` |
|        - |  3533 | `	}` |
|        - |  3534 | `	/* Peek constant name */` |
|       47 |  3535 | `	pName = &pGen->pIn->sData;` |
|        - |  3536 | `	/* Make sure the constant name isn't reserved */` |
|       47 |  3537 | `	if( GenStateIsReservedConstant(pName) ){` |
|        - |  3538 | `		/* Reserved constant */` |
|       10 |  3539 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       10 |  3540 | `		if( rc == SXERR_ABORT ){` |
|        - |  3541 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3542 | `			return SXERR_ABORT;` |
|        - |  3543 | `		}` |
|       10 |  3544 | `		goto Synchronize;` |
|        - |  3545 | `	}` |
|       38 |  3546 | `	pGen->pIn++;` |
|       38 |  3547 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|        - |  3548 | `		/* Invalid statement*/` |
|        6 |  3549 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|        6 |  3550 | `		if( rc == SXERR_ABORT ){` |
|        - |  3551 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3552 | `			return SXERR_ABORT;` |
|        - |  3553 | `		}` |
|        6 |  3554 | `		goto Synchronize;` |
|        - |  3555 | `	}` |
|       32 |  3556 | `	pGen->pIn++; /*Jump the equal sign */` |
|        - |  3557 | `	/* Allocate a new constant value container */` |
|       32 |  3558 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|       32 |  3559 | `	if( pConsCode == 0 ){` |
|      ! 0 |  3560 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3561 | `		return SXERR_ABORT;` |
|        - |  3562 | `	}` |
|       32 |  3563 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - |  3564 | `	/* Swap bytecode container */` |
|       32 |  3565 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       32 |  3566 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|        - |  3567 | `	/* Compile constant value */` |
|       32 |  3568 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  3569 | `	/* Emit the done instruction */` |
|       32 |  3570 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       32 |  3571 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       32 |  3572 | `	if( rc == SXERR_ABORT ){` |
|        - |  3573 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  3574 | `		return SXERR_ABORT;` |
|        - |  3575 | `	}` |
|       32 |  3576 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|        - |  3577 | `	/* Register the constant with namespace-qualified name */` |
|        - |  3578 | `	{` |
|        - |  3579 | `		SyBlob sFQN;` |
|        - |  3580 | `		SyString sFQNStr;` |
|       32 |  3581 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       32 |  3582 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|       32 |  3583 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       47 |  3584 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|       30 |  3585 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|       32 |  3586 | `		if( rc == SXRET_OK && SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|        - |  3587 | ``			/* php 8.5: attributes on `const` statements — attach the pending`` |
|        - |  3588 | `			 * groups to the registered constant record for Reflection. */` |
|        7 |  3589 | `			SyHashEntry *pCEntry = SyHashGet(&pGen->pVm->hConstant,` |
|        4 |  3590 | `				SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        5 |  3591 | `			if( pCEntry ){` |
|        5 |  3592 | `				ph7_constant *pRegCons = (ph7_constant *)pCEntry->pUserData;` |
|        5 |  3593 | `				if( GenStateConsumeAttrs(&(*pGen),&pRegCons->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  3594 | `					SyBlobRelease(&sFQN);` |
|      ! 0 |  3595 | `					return SXERR_ABORT;` |
|        - |  3596 | `				}` |
|        2 |  3597 | `			}` |
|        2 |  3598 | `		}` |
|       32 |  3599 | `		SyBlobRelease(&sFQN);` |
|        - |  3600 | `	}` |
|       32 |  3601 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  3602 | `		SySetRelease(pConsCode);` |
|      ! 0 |  3603 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|      ! 0 |  3604 | `	}` |
|       32 |  3605 | `	return SXRET_OK;` |
|        9 |  3606 | `Synchronize:` |
|        - |  3607 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|       60 |  3608 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       42 |  3609 | `		pGen->pIn++;` |
|        4 |  3610 | `	}` |
|       22 |  3611 | `	return SXRET_OK;` |
|       29 |  3612 | `}` |
|        - |  3613 | `/*` |
|        - |  3614 | ` * Compile the 'continue' statement.` |
|        - |  3615 | ` * According to the PHP language reference` |
|        - |  3616 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|        - |  3617 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|        - |  3618 | ` *  iteration.` |
|        - |  3619 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|        - |  3620 | ` *  the purposes of continue.` |
|        - |  3621 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|        - |  3622 | ` *  of enclosing loops it should skip to the end of.` |
|        - |  3623 | ` *  Note:` |
|        - |  3624 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|        - |  3625 | ` */` |
|        - |  3626 | `/*` |
|        - |  3627 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|        - |  3628 | ` * block and the target loop block. This ensures finally blocks run when` |
|        - |  3629 | ` * break/continue crosses a try boundary.` |
|        - |  3630 | ` *` |
|        - |  3631 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|        - |  3632 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|        - |  3633 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|        - |  3634 | ` */` |
|    58412 |  3635 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|        5 |  3636 | `{` |
|    58417 |  3637 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    58417 |  3638 | `	int nInlineTry = 0;` |
|   272279 |  3639 | `	while( pBlock && pBlock != pTarget ){` |
|   213867 |  3640 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|        6 |  3641 | `			if( pBlock->pUserData ){` |
|        - |  3642 | `				/* A try block with an exception context. In a generator its catch/finally` |
|        - |  3643 | `				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that` |
|        - |  3644 | `				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.` |
|        - |  3645 | `				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */` |
|        6 |  3646 | `				if( pGen->bInGenerator ){` |
|        3 |  3647 | `					nInlineTry++;` |
|        2 |  3648 | `				}else{` |
|        3 |  3649 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|        - |  3650 | `				}` |
|        4 |  3651 | `			}else{` |
|        - |  3652 | `				/* A catch/finally block compiled into a separate bytecode container` |
|        - |  3653 | `				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */` |
|      ! 0 |  3654 | `				break;` |
|        - |  3655 | `			}` |
|        2 |  3656 | `		}` |
|   213867 |  3657 | `		pBlock = pBlock->pParent;` |
|        5 |  3658 | `	}` |
|    58417 |  3659 | `	return nInlineTry;` |
|        5 |  3660 | `}` |
|    27238 |  3661 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|        5 |  3662 | `{` |
|        - |  3663 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  3664 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  3665 | `	sxu32 nLineLocal;` |
|        - |  3666 | `	sxi32 rc;` |
|    27243 |  3667 | `	nLineLocal = pGen->pIn->nLine;` |
|    27243 |  3668 | `	iLevel = 0;` |
|        - |  3669 | `	/* Jump the 'continue' keyword */` |
|    27243 |  3670 | `	pGen->pIn++;` |
|    27243 |  3671 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|        - |  3672 | `		/* optional numeric argument which tells us how many levels` |
|        - |  3673 | `		 * of enclosing loops we should skip to the end of.` |
|        - |  3674 | `		 */` |
|        - |  3675 | `		char zScratch[GEN_NUM_SCRATCH];` |
|       17 |  3676 | `		char *zAlloc = 0;` |
|        - |  3677 | `		SyString sNum;` |
|       17 |  3678 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|       17 |  3679 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3680 | `			return SXERR_ABORT;` |
|        - |  3681 | `		}` |
|       17 |  3682 | `		if( rc == SXRET_OK ){` |
|       20 |  3683 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       12 |  3684 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|       14 |  3685 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3686 | `				return SXERR_ABORT;` |
|        - |  3687 | `			}` |
|       14 |  3688 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       14 |  3689 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        6 |  3690 | `		}` |
|       17 |  3691 | `		if( iLevel < 2 ){` |
|        3 |  3692 | `			iLevel = 0;` |
|        1 |  3693 | `		}` |
|       17 |  3694 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|        7 |  3695 | `	}` |
|        - |  3696 | `	/* Point to the target loop */` |
|    27243 |  3697 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    27243 |  3698 | `	if( pLoop == 0 ){` |
|        - |  3699 | `		/* Illegal continue */` |
|       12 |  3700 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|       12 |  3701 | `		if( rc == SXERR_ABORT ){` |
|        - |  3702 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3703 | `			return SXERR_ABORT;` |
|        - |  3704 | `		}` |
|        7 |  3705 | `	}else{` |
|    27233 |  3706 | `		sxu32 nInstrIdx = 0;` |
|        - |  3707 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    27233 |  3708 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  3709 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|        - |  3710 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|    27233 |  3711 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|    27233 |  3712 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|        - |  3713 | `			/* According to the PHP language reference manual` |
|        - |  3714 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|        - |  3715 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|        - |  3716 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|        - |  3717 | `			 */` |
|        5 |  3718 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|        5 |  3719 | `			if( rc == SXRET_OK ){` |
|        5 |  3720 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|        2 |  3721 | `			}` |
|        3 |  3722 | `		}else{` |
|        - |  3723 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    27229 |  3724 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    27229 |  3725 | `			if( pLoop->bPostContinue == TRUE ){` |
|        - |  3726 | `				JumpFixup sJumpFix;` |
|        - |  3727 | `				/* Post-continue */` |
|       14 |  3728 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|       14 |  3729 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|       14 |  3730 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|        6 |  3731 | `			}` |
|        - |  3732 | `		}` |
|        - |  3733 | `	}` |
|    27243 |  3734 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  3735 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  3736 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|      ! 0 |  3737 | `	}` |
|        - |  3738 | `	/* Statement successfully compiled */` |
|    27243 |  3739 | `	return SXRET_OK;` |
|    13624 |  3740 | `}` |
|        - |  3741 | `/*` |
|        - |  3742 | ` * Compile the 'break' statement.` |
|        - |  3743 | ` * According to the PHP language reference` |
|        - |  3744 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|        - |  3745 | ` *  structure.` |
|        - |  3746 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|        - |  3747 | ` *  enclosing structures are to be broken out of.` |
|        - |  3748 | ` */` |
|    31200 |  3749 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|        5 |  3750 | `{` |
|        - |  3751 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  3752 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  3753 | `	sxi32 rc;` |
|    31205 |  3754 | `	iLevel = 0;` |
|        - |  3755 | `	/* Jump the 'break' keyword */` |
|    31205 |  3756 | `	pGen->pIn++;` |
|    31205 |  3757 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|        - |  3758 | `		/* optional numeric argument which tells us how many levels` |
|        - |  3759 | `		 * of enclosing loops we should skip to the end of.` |
|        - |  3760 | `		 */` |
|        - |  3761 | `		char zScratch[GEN_NUM_SCRATCH];` |
|       18 |  3762 | `		char *zAlloc = 0;` |
|        - |  3763 | `		SyString sNum;` |
|       18 |  3764 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|       18 |  3765 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3766 | `			return SXERR_ABORT;` |
|        - |  3767 | `		}` |
|       18 |  3768 | `		if( rc == SXRET_OK ){` |
|       21 |  3769 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       12 |  3770 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|       15 |  3771 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3772 | `				return SXERR_ABORT;` |
|        - |  3773 | `			}` |
|       15 |  3774 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       15 |  3775 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        6 |  3776 | `		}` |
|       18 |  3777 | `		if( iLevel < 2 ){` |
|        3 |  3778 | `			iLevel = 0;` |
|        1 |  3779 | `		}` |
|       18 |  3780 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|        7 |  3781 | `	}` |
|        - |  3782 | `	/* Extract the target loop */` |
|    31205 |  3783 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    31205 |  3784 | `	if( pLoop == 0 ){` |
|        - |  3785 | `		/* Illegal break */` |
|       19 |  3786 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|       19 |  3787 | `		if( rc == SXERR_ABORT ){` |
|        - |  3788 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3789 | `			return SXERR_ABORT;` |
|        - |  3790 | `		}` |
|       11 |  3791 | `	}else{` |
|        - |  3792 | `		sxu32 nInstrIdx;` |
|        - |  3793 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    31189 |  3794 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  3795 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|    31189 |  3796 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|    31189 |  3797 | `		if( rc == SXRET_OK ){` |
|        - |  3798 | `			/* Fix the jump later when the jump destination is resolved */` |
|    31189 |  3799 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|    15592 |  3800 | `		}` |
|        - |  3801 | `	}` |
|    31205 |  3802 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  3803 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  3804 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|      ! 0 |  3805 | `	}` |
|        - |  3806 | `	/* Statement successfully compiled */` |
|    31205 |  3807 | `	return SXRET_OK;` |
|    15605 |  3808 | `}` |
|        - |  3809 | `/*` |
|        - |  3810 | ` * Compile or record a label.` |
|        - |  3811 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|        - |  3812 | ` * Example` |
|        - |  3813 | ` *  goto LABEL;` |
|        - |  3814 | ` *   echo 'Foo';` |
|        - |  3815 | ` *  LABEL:` |
|        - |  3816 | ` *   echo 'Bar';` |
|        - |  3817 | ` */` |
|      112 |  3818 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|        5 |  3819 | `{` |
|        - |  3820 | `	GenBlock *pBlock;` |
|        - |  3821 | `	Label sLabel;` |
|        - |  3822 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|      117 |  3823 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|      117 |  3824 | `	if( pBlock ){` |
|        - |  3825 | `		sxi32 rc;` |
|        8 |  3826 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        4 |  3827 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|        6 |  3828 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3829 | `			return SXERR_ABORT;` |
|        - |  3830 | `		}` |
|        4 |  3831 | `	}else{` |
|      113 |  3832 | `		SyString *pTarget = &pGen->pIn->sData;` |
|        - |  3833 | `		char *zDup;` |
|        - |  3834 | `		/* Initialize label fields */` |
|      113 |  3835 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|        - |  3836 | `		/* Duplicate label name */` |
|      113 |  3837 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      113 |  3838 | `		if( zDup == 0 ){` |
|      ! 0 |  3839 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  3840 | `			return SXERR_ABORT;` |
|        - |  3841 | `		}` |
|      113 |  3842 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|      113 |  3843 | `		sLabel.bRef  = FALSE;` |
|      113 |  3844 | `		sLabel.nLine = pGen->pIn->nLine;` |
|      113 |  3845 | `		pBlock = pGen->pCurrent;` |
|      221 |  3846 | `		while( pBlock ){` |
|      133 |  3847 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|       24 |  3848 | `				break;` |
|        - |  3849 | `			}` |
|        - |  3850 | `			/* Point to the upper block */` |
|      113 |  3851 | `			pBlock = pBlock->pParent;` |
|        5 |  3852 | `		}` |
|      113 |  3853 | `		if( pBlock ){` |
|       24 |  3854 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       14 |  3855 | `		}else{` |
|       93 |  3856 | `			sLabel.pFunc = 0;` |
|        - |  3857 | `		}` |
|        - |  3858 | `		/* Insert in label set */` |
|      113 |  3859 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|        - |  3860 | `	}` |
|      117 |  3861 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|      117 |  3862 | `	return SXRET_OK;` |
|       61 |  3863 | `}` |
|        - |  3864 | `/*` |
|        - |  3865 | ` * Compile the so hated 'goto' statement.` |
|        - |  3866 | ` * You've probably been taught that gotos are bad, but this sort` |
|        - |  3867 | ` * of rewriting  happens all the time, in fact every time you run` |
|        - |  3868 | ` * a compiler it has to do this.` |
|        - |  3869 | ` * According to the PHP language reference manual` |
|        - |  3870 | ` *   The goto operator can be used to jump to another section in the program.` |
|        - |  3871 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|        - |  3872 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|        - |  3873 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|        - |  3874 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|        - |  3875 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|        - |  3876 | ` *   of a multi-level break` |
|        - |  3877 | ` */` |
|      152 |  3878 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|        5 |  3879 | `{` |
|        - |  3880 | `	JumpFixup sJump;` |
|        - |  3881 | `	sxi32 rc;` |
|      157 |  3882 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|      157 |  3883 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  3884 | `		/* Missing label */` |
|      ! 0 |  3885 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|      ! 0 |  3886 | `		if( rc == SXERR_ABORT ){` |
|        - |  3887 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3888 | `			return SXERR_ABORT;` |
|        - |  3889 | `		}` |
|      ! 0 |  3890 | `		return SXRET_OK;` |
|        - |  3891 | `	}` |
|      157 |  3892 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        6 |  3893 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|        6 |  3894 | `		if( rc == SXERR_ABORT ){` |
|        - |  3895 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3896 | `			return SXERR_ABORT;` |
|        - |  3897 | `		}` |
|        4 |  3898 | `	}else{` |
|      153 |  3899 | `		SyString *pTarget = &pGen->pIn->sData;` |
|        - |  3900 | `		GenBlock *pBlock;` |
|        - |  3901 | `		char *zDup;` |
|        - |  3902 | `		/* Prepare the jump destination */` |
|      153 |  3903 | `		sJump.nJumpType = PH7_OP_JMP;` |
|      153 |  3904 | `		sJump.nLine = pGen->pIn->nLine;` |
|        - |  3905 | `		/* Duplicate label name */` |
|      153 |  3906 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      153 |  3907 | `		if( zDup == 0 ){` |
|      ! 0 |  3908 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  3909 | `			return SXERR_ABORT;` |
|        - |  3910 | `		}` |
|      153 |  3911 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|      153 |  3912 | `		pBlock = pGen->pCurrent;` |
|      315 |  3913 | `		while( pBlock ){` |
|      199 |  3914 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|       37 |  3915 | `				break;` |
|        - |  3916 | `			}` |
|        - |  3917 | `			/* Point to the upper block */` |
|      167 |  3918 | `			pBlock = pBlock->pParent;` |
|        5 |  3919 | `		}` |
|      153 |  3920 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|        9 |  3921 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|        9 |  3922 | `			if( rc == SXERR_ABORT ){` |
|        - |  3923 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  3924 | `				return SXERR_ABORT;` |
|        - |  3925 | `			}` |
|        3 |  3926 | `		}` |
|      153 |  3927 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|       30 |  3928 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       17 |  3929 | `		}else{` |
|      127 |  3930 | `			sJump.pFunc = 0;` |
|        - |  3931 | `		}` |
|        - |  3932 | `		/* Emit the unconditional jump */` |
|      153 |  3933 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|      153 |  3934 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|       74 |  3935 | `		}` |
|        - |  3936 | `	}` |
|      157 |  3937 | `	pGen->pIn++; /* Jump the label name */` |
|      157 |  3938 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        3 |  3939 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|        1 |  3940 | `	}` |
|        - |  3941 | `	/* Statement successfully compiled */` |
|      157 |  3942 | `	return SXRET_OK;` |
|       81 |  3943 | `}` |
|        - |  3944 | `/*` |
|        - |  3945 | ` * Point to the next PHP chunk that will be processed shortly.` |
|        - |  3946 | ` * Return SXRET_OK on success. Any other return value indicates` |
|        - |  3947 | ` * failure.` |
|        - |  3948 | ` */` |
|       20 |  3949 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|        2 |  3950 | `{` |
|        - |  3951 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|        - |  3952 | `	sxu32 nRawObj;` |
|       10 |  3953 | `	sxu32 nObjIdx;` |
|        - |  3954 | `	/* Consume raw chunks verbatim without any processing until we get` |
|        - |  3955 | `	 * a PHP block.` |
|        - |  3956 | `	 */` |
|       10 |  3957 | `Consume:` |
|       22 |  3958 | `	nRawObj = nObjIdx = 0;` |
|       22 |  3959 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|      ! 0 |  3960 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|      ! 0 |  3961 | `		if( pRawObj == 0 ){` |
|      ! 0 |  3962 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3963 | `			return SXERR_ABORT;` |
|        - |  3964 | `		}` |
|        - |  3965 | `		/* Mark as constant and emit the load constant instruction */` |
|      ! 0 |  3966 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|      ! 0 |  3967 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|      ! 0 |  3968 | `		++nRawObj;` |
|      ! 0 |  3969 | `		pGen->pRawIn++; /* Next chunk */` |
|      ! 0 |  3970 | `	}` |
|       22 |  3971 | `	if( nRawObj > 0 ){` |
|        - |  3972 | `		/* Emit the consume instruction */` |
|      ! 0 |  3973 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      ! 0 |  3974 | `	}` |
|       22 |  3975 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|      ! 0 |  3976 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|        - |  3977 | `		/* Reset the token set (and its trivia sidecar) */` |
|      ! 0 |  3978 | `		SySetReset(pTokenSet);` |
|      ! 0 |  3979 | `		SySetReset(&pGen->aTrivia);` |
|        - |  3980 | `		/* Tokenize input */` |
|      ! 0 |  3981 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|      ! 0 |  3982 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|        - |  3983 | `		/* Point to the fresh token stream */` |
|      ! 0 |  3984 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|      ! 0 |  3985 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|        - |  3986 | `		/* Advance the stream cursor */` |
|      ! 0 |  3987 | `		pGen->pRawIn++;` |
|        - |  3988 | `		/* TICKET 1433-011 */` |
|      ! 0 |  3989 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|        - |  3990 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|        - |  3991 | `			sxi32 rc;` |
|        - |  3992 | `			/* Refer to TICKET 1433-009  */` |
|      ! 0 |  3993 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|      ! 0 |  3994 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|      ! 0 |  3995 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|      ! 0 |  3996 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|      ! 0 |  3997 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  3998 | `				return SXERR_ABORT;` |
|      ! 0 |  3999 | `			}else if( rc != SXERR_EMPTY ){` |
|      ! 0 |  4000 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 |  4001 | `			}` |
|      ! 0 |  4002 | `			goto Consume;` |
|        - |  4003 | `		}` |
|      ! 0 |  4004 | `	}else{` |
|        - |  4005 | `		/* No more chunks to process */` |
|       22 |  4006 | `		pGen->pIn = pGen->pEnd;` |
|       22 |  4007 | `		return SXERR_EOF;` |
|        - |  4008 | `	}` |
|      ! 0 |  4009 | `	return SXRET_OK;` |
|       12 |  4010 | `}` |
|        - |  4011 | `/*` |
|        - |  4012 | ` * Compile a PHP block.` |
|        - |  4013 | ` * A block is simply one or more PHP statements and expressions to compile` |
|        - |  4014 | ` * optionally delimited by braces {}.` |
|        - |  4015 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|        - |  4016 | ` * and this function takes care of generating the appropriate error` |
|        - |  4017 | ` * message.` |
|        - |  4018 | ` */` |
|  3013072 |  4019 | `static sxi32 PH7_CompileBlock(` |
|        - |  4020 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  4021 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|        - |  4022 | `	)` |
|        5 |  4023 | `{` |
|        - |  4024 | `	sxi32 rc;` |
|        - |  4025 | `	sxu32 nLine;` |
|  3013077 |  4026 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  3011619 |  4027 | `		nLine = pGen->pIn->nLine;` |
|  3011619 |  4028 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  3011619 |  4029 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  4030 | `			return SXERR_ABORT;` |
|        - |  4031 | `		}` |
|  3011619 |  4032 | `		pGen->pIn++;` |
|        - |  4033 | `		/* Compile until we hit the closing braces '}' */` |
|  4408883 |  4034 | `		for(;;){` |
|  8817771 |  4035 | `			if( pGen->pIn >= pGen->pEnd ){` |
|       22 |  4036 | `				rc = GenStateNextChunk(&(*pGen));` |
|       22 |  4037 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  4038 | `			 	   return SXERR_ABORT;` |
|        - |  4039 | `				}` |
|       22 |  4040 | `				if( rc == SXERR_EOF ){` |
|        - |  4041 | `					/* No more token to process. Missing closing braces */` |
|       22 |  4042 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|       22 |  4043 | `					break;` |
|        - |  4044 | `				}` |
|      ! 0 |  4045 | `			}` |
|  8817751 |  4046 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|        - |  4047 | `				/* Closing braces found,break immediately*/` |
|  3011599 |  4048 | `				pGen->pIn++;` |
|  3011599 |  4049 | `				break;` |
|        - |  4050 | `			}` |
|        - |  4051 | `			/* Compile a single statement */` |
|  5806157 |  4052 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  5806157 |  4053 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4054 | `				return SXERR_ABORT;` |
|        - |  4055 | `			}` |
|        5 |  4056 | `		}` |
|  3011619 |  4057 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  1507270 |  4058 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|      ! 0 |  4059 | `		pGen->pIn++;` |
|      ! 0 |  4060 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|      ! 0 |  4061 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  4062 | `			return SXERR_ABORT;` |
|        - |  4063 | `		}` |
|        - |  4064 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|      ! 0 |  4065 | `		for(;;){` |
|      ! 0 |  4066 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  4067 | `				rc = GenStateNextChunk(&(*pGen));` |
|      ! 0 |  4068 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  4069 | `			 	   return SXERR_ABORT;` |
|        - |  4070 | `				}` |
|      ! 0 |  4071 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|        - |  4072 | `					/* No more token to process */` |
|      ! 0 |  4073 | `					if( rc == SXERR_EOF ){` |
|      ! 0 |  4074 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|        - |  4075 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|      ! 0 |  4076 | `					}` |
|      ! 0 |  4077 | `					break;` |
|        - |  4078 | `				}` |
|      ! 0 |  4079 | `			}` |
|      ! 0 |  4080 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        - |  4081 | `				sxi32 nKwrd;` |
|        - |  4082 | `				/* Keyword found */` |
|      ! 0 |  4083 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      ! 0 |  4084 | `				if( nKwrd == nKeywordEnd \|\|` |
|      ! 0 |  4085 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|        - |  4086 | `						/* Delimiter keyword found,break */` |
|      ! 0 |  4087 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|      ! 0 |  4088 | `							pGen->pIn++; /*  endif;endswitch... */` |
|      ! 0 |  4089 | `						}` |
|      ! 0 |  4090 | `						break;` |
|        - |  4091 | `				}` |
|      ! 0 |  4092 | `			}` |
|        - |  4093 | `			/* Compile a single statement */` |
|      ! 0 |  4094 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|      ! 0 |  4095 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4096 | `				return SXERR_ABORT;` |
|        - |  4097 | `			}` |
|      ! 0 |  4098 | `		}` |
|      ! 0 |  4099 | `		GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  4100 | `	}else{` |
|        - |  4101 | `		/* Compile a single statement */` |
|     1463 |  4102 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     1463 |  4103 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4104 | `			return SXERR_ABORT;` |
|        - |  4105 | `		}` |
|        - |  4106 | `	}` |
|        - |  4107 | `	/* Jump trailing semi-colons ';' */` |
|  3013077 |  4108 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      ! 0 |  4109 | `		pGen->pIn++;` |
|      ! 0 |  4110 | `	}` |
|  3013077 |  4111 | `	return SXRET_OK;` |
|  1506541 |  4112 | `}` |
|        - |  4113 | `/*` |
|        - |  4114 | ` * Compile the gentle 'while' statement.` |
|        - |  4115 | ` * According to the PHP language reference` |
|        - |  4116 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|        - |  4117 | ` *  The basic form of a while statement is:` |
|        - |  4118 | ` *  while (expr)` |
|        - |  4119 | ` *   statement` |
|        - |  4120 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|        - |  4121 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|        - |  4122 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|        - |  4123 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|        - |  4124 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|        - |  4125 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|        - |  4126 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|        - |  4127 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|        - |  4128 | ` *  while (expr):` |
|        - |  4129 | ` *    statement` |
|        - |  4130 | ` *   endwhile;` |
|        - |  4131 | ` */` |
|    15672 |  4132 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|        5 |  4133 | `{` |
|    15677 |  4134 | `	GenBlock *pWhileBlock = 0;` |
|    15677 |  4135 | `	SyToken *pTmp,*pEnd = 0;` |
|        - |  4136 | `	sxu32 nFalseJump;` |
|        - |  4137 | `	sxu32 nLine;` |
|        - |  4138 | `	sxi32 rc;` |
|    15677 |  4139 | `	nLine = pGen->pIn->nLine;` |
|        - |  4140 | `	/* Jump the 'while' keyword */` |
|    15677 |  4141 | `	pGen->pIn++;` |
|    15677 |  4142 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4143 | `		/* Syntax error */` |
|      ! 0 |  4144 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  4145 | `		if( rc == SXERR_ABORT ){` |
|        - |  4146 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4147 | `			return SXERR_ABORT;` |
|        - |  4148 | `		}` |
|      ! 0 |  4149 | `		goto Synchronize;` |
|        - |  4150 | `	}` |
|        - |  4151 | `	/* Jump the left parenthesis '(' */` |
|    15677 |  4152 | `	pGen->pIn++;` |
|        - |  4153 | `	/* Create the loop block */` |
|    15677 |  4154 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|    15677 |  4155 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4156 | `		return SXERR_ABORT;` |
|        - |  4157 | `	}` |
|        - |  4158 | `	/* Delimit the condition */` |
|    15677 |  4159 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    15677 |  4160 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  4161 | `		/* Empty expression */` |
|        3 |  4162 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|        3 |  4163 | `		if( rc == SXERR_ABORT ){` |
|        - |  4164 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4165 | `			return SXERR_ABORT;` |
|        - |  4166 | `		}` |
|        1 |  4167 | `	}` |
|        - |  4168 | `	/* Swap token streams */` |
|    15677 |  4169 | `	pTmp = pGen->pEnd;` |
|    15677 |  4170 | `	pGen->pEnd = pEnd;` |
|        - |  4171 | `	/* Compile the expression */` |
|    15677 |  4172 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    15677 |  4173 | `	if( rc == SXERR_ABORT ){` |
|        - |  4174 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4175 | `		return SXERR_ABORT;` |
|        - |  4176 | `	}` |
|        - |  4177 | `	/* Update token stream */` |
|    15677 |  4178 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  4179 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4180 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4181 | `			return SXERR_ABORT;` |
|        - |  4182 | `		}` |
|      ! 0 |  4183 | `		pGen->pIn++;` |
|      ! 0 |  4184 | `	}` |
|        - |  4185 | `	/* Synchronize pointers */` |
|    15677 |  4186 | `	pGen->pIn  = &pEnd[1];` |
|    15677 |  4187 | `	pGen->pEnd = pTmp;` |
|        - |  4188 | `	/* Emit the false jump */` |
|    15677 |  4189 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - |  4190 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    15677 |  4191 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|        - |  4192 | `	/* Compile the loop body */` |
|    15677 |  4193 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|    15677 |  4194 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  4195 | `		return SXERR_ABORT;` |
|        - |  4196 | `	}` |
|        - |  4197 | `	/* Emit the unconditional jump to the start of the loop */` |
|    15677 |  4198 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|        - |  4199 | `	/* Fix all jumps now the destination is resolved */` |
|    15677 |  4200 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4201 | `	/* Release the loop block */` |
|    15677 |  4202 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4203 | `	/* Statement successfully compiled */` |
|    15677 |  4204 | `	return SXRET_OK;` |
|      ! 0 |  4205 | `Synchronize:` |
|        - |  4206 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  4207 | `	 * compiling this erroneous block.` |
|        - |  4208 | `	 */` |
|      ! 0 |  4209 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  4210 | `		pGen->pIn++;` |
|      ! 0 |  4211 | `	}` |
|      ! 0 |  4212 | `	return SXRET_OK;` |
|     7841 |  4213 | `}` |
|        - |  4214 | `/*` |
|        - |  4215 | ` * Compile the ugly do..while() statement.` |
|        - |  4216 | ` * According to the PHP language reference` |
|        - |  4217 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|        - |  4218 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|        - |  4219 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|        - |  4220 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|        - |  4221 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|        - |  4222 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|        - |  4223 | ` *  would end immediately).` |
|        - |  4224 | ` *  There is just one syntax for do-while loops:` |
|        - |  4225 | ` *  <?php` |
|        - |  4226 | ` *  $i = 0;` |
|        - |  4227 | ` *  do {` |
|        - |  4228 | ` *   echo $i;` |
|        - |  4229 | ` *  } while ($i > 0);` |
|        - |  4230 | ` * ?>` |
|        - |  4231 | ` */` |
|        2 |  4232 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|        1 |  4233 | `{` |
|        3 |  4234 | `	SyToken *pTmp,*pEnd = 0;` |
|        3 |  4235 | `	GenBlock *pDoBlock = 0;` |
|        - |  4236 | `	sxu32 nLine;` |
|        - |  4237 | `	sxi32 rc;` |
|        3 |  4238 | `	nLine = pGen->pIn->nLine;` |
|        - |  4239 | `	/* Jump the 'do' keyword */` |
|        3 |  4240 | `	pGen->pIn++;` |
|        - |  4241 | `	/* Create the loop block */` |
|        3 |  4242 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|        3 |  4243 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4244 | `		return SXERR_ABORT;` |
|        - |  4245 | `	}` |
|        - |  4246 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|        3 |  4247 | `	pDoBlock->bPostContinue = TRUE;` |
|        3 |  4248 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|        3 |  4249 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  4250 | `		return SXERR_ABORT;` |
|        - |  4251 | `	}` |
|        3 |  4252 | `	if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  4253 | `		nLine = pGen->pIn->nLine;` |
|      ! 0 |  4254 | `	}` |
|        3 |  4255 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|      ! 0 |  4256 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|        - |  4257 | `			/* Missing 'while' statement */` |
|        3 |  4258 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|        3 |  4259 | `			if( rc == SXERR_ABORT ){` |
|        - |  4260 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4261 | `				return SXERR_ABORT;` |
|        - |  4262 | `			}` |
|        3 |  4263 | `			goto Synchronize;` |
|        - |  4264 | `	}` |
|        - |  4265 | `	/* Jump the 'while' keyword */` |
|      ! 0 |  4266 | `	pGen->pIn++;` |
|      ! 0 |  4267 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4268 | `		/* Syntax error */` |
|      ! 0 |  4269 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  4270 | `		if( rc == SXERR_ABORT ){` |
|        - |  4271 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4272 | `			return SXERR_ABORT;` |
|        - |  4273 | `		}` |
|      ! 0 |  4274 | `		goto Synchronize;` |
|        - |  4275 | `	}` |
|        - |  4276 | `	/* Jump the left parenthesis '(' */` |
|      ! 0 |  4277 | `	pGen->pIn++;` |
|        - |  4278 | `	/* Delimit the condition */` |
|      ! 0 |  4279 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      ! 0 |  4280 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  4281 | `		/* Empty expression */` |
|      ! 0 |  4282 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|      ! 0 |  4283 | `		if( rc == SXERR_ABORT ){` |
|        - |  4284 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4285 | `			return SXERR_ABORT;` |
|        - |  4286 | `		}` |
|      ! 0 |  4287 | `		goto Synchronize;` |
|        - |  4288 | `	}` |
|        - |  4289 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|      ! 0 |  4290 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|        - |  4291 | `		JumpFixup *aPost;` |
|        - |  4292 | `		VmInstr *pInstr;` |
|        - |  4293 | `		sxu32 nJumpDest;` |
|        - |  4294 | `		sxu32 n;` |
|      ! 0 |  4295 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|      ! 0 |  4296 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      ! 0 |  4297 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|      ! 0 |  4298 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      ! 0 |  4299 | `			if( pInstr ){` |
|        - |  4300 | `				/* Fix */` |
|      ! 0 |  4301 | `				pInstr->iP2 = nJumpDest;` |
|      ! 0 |  4302 | `			}` |
|      ! 0 |  4303 | `		}` |
|      ! 0 |  4304 | `	}` |
|        - |  4305 | `	/* Swap token streams */` |
|      ! 0 |  4306 | `	pTmp = pGen->pEnd;` |
|      ! 0 |  4307 | `	pGen->pEnd = pEnd;` |
|        - |  4308 | `	/* Compile the expression */` |
|      ! 0 |  4309 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      ! 0 |  4310 | `	if( rc == SXERR_ABORT ){` |
|        - |  4311 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4312 | `		return SXERR_ABORT;` |
|        - |  4313 | `	}` |
|        - |  4314 | `	/* Update token stream */` |
|      ! 0 |  4315 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  4316 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4317 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4318 | `			return SXERR_ABORT;` |
|        - |  4319 | `		}` |
|      ! 0 |  4320 | `		pGen->pIn++;` |
|      ! 0 |  4321 | `	}` |
|      ! 0 |  4322 | `	pGen->pIn  = &pEnd[1];` |
|      ! 0 |  4323 | `	pGen->pEnd = pTmp;` |
|        - |  4324 | `	/* Emit the true jump to the beginning of the loop */` |
|      ! 0 |  4325 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|        - |  4326 | `	/* Fix all jumps now the destination is resolved */` |
|      ! 0 |  4327 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4328 | `	/* Release the loop block */` |
|      ! 0 |  4329 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4330 | `	/* Statement successfully compiled */` |
|      ! 0 |  4331 | `	return SXRET_OK;` |
|        1 |  4332 | `Synchronize:` |
|        - |  4333 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  4334 | `	 * compiling this erroneous block.` |
|        - |  4335 | `	 */` |
|        3 |  4336 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  4337 | `		pGen->pIn++;` |
|      ! 0 |  4338 | `	}` |
|        3 |  4339 | `	return SXRET_OK;` |
|        2 |  4340 | `}` |
|        - |  4341 | `/*` |
|        - |  4342 | ` * Compile the complex and powerful 'for' statement.` |
|        - |  4343 | ` * According to the PHP language reference` |
|        - |  4344 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|        - |  4345 | ` *  The syntax of a for loop is:` |
|        - |  4346 | ` *  for (expr1; expr2; expr3)` |
|        - |  4347 | ` *   statement` |
|        - |  4348 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|        - |  4349 | ` *  the beginning of the loop.` |
|        - |  4350 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|        - |  4351 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|        - |  4352 | ` *  to FALSE, the execution of the loop ends.` |
|        - |  4353 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|        - |  4354 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|        - |  4355 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|        - |  4356 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|        - |  4357 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|        - |  4358 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|        - |  4359 | ` *  of using the for truth expression.` |
|        - |  4360 | ` */` |
|    38980 |  4361 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|        5 |  4362 | `{` |
|    38985 |  4363 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|    38985 |  4364 | `	GenBlock *pForBlock = 0;` |
|        - |  4365 | `	sxu32 nFalseJump;` |
|        - |  4366 | `	sxu32 nLine;` |
|        - |  4367 | `	sxi32 rc;` |
|    38985 |  4368 | `	nLine = pGen->pIn->nLine;` |
|        - |  4369 | `	/* Jump the 'for' keyword */` |
|    38985 |  4370 | `	pGen->pIn++;` |
|    38985 |  4371 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4372 | `		/* Syntax error */` |
|      ! 0 |  4373 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|      ! 0 |  4374 | `		if( rc == SXERR_ABORT ){` |
|        - |  4375 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4376 | `			return SXERR_ABORT;` |
|        - |  4377 | `		}` |
|      ! 0 |  4378 | `		return SXRET_OK;` |
|        - |  4379 | `	}` |
|        - |  4380 | `	/* Jump the left parenthesis '(' */` |
|    38985 |  4381 | `	pGen->pIn++;` |
|        - |  4382 | `	/* Delimit the init-expr;condition;post-expr */` |
|    38985 |  4383 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    38985 |  4384 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  4385 | `		/* Empty expression */` |
|      ! 0 |  4386 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|      ! 0 |  4387 | `		if( rc == SXERR_ABORT ){` |
|        - |  4388 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4389 | `			return SXERR_ABORT;` |
|        - |  4390 | `		}` |
|        - |  4391 | `		/* Synchronize */` |
|      ! 0 |  4392 | `		pGen->pIn = pEnd;` |
|      ! 0 |  4393 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  4394 | `			pGen->pIn++;` |
|      ! 0 |  4395 | `		}` |
|      ! 0 |  4396 | `		return SXRET_OK;` |
|        - |  4397 | `	}` |
|        - |  4398 | `	/* Swap token streams */` |
|    38985 |  4399 | `	pTmp = pGen->pEnd;` |
|    38985 |  4400 | `	pGen->pEnd = pEnd;` |
|        - |  4401 | `	/* Compile initialization expressions if available */` |
|    38985 |  4402 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  4403 | `	/* Pop operand lvalues */` |
|    38985 |  4404 | `	if( rc == SXERR_ABORT ){` |
|        - |  4405 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4406 | `		return SXERR_ABORT;` |
|    38985 |  4407 | `	}else if( rc != SXERR_EMPTY ){` |
|    38983 |  4408 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    19489 |  4409 | `	}` |
|    38985 |  4410 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  4411 | `		/* Syntax error */` |
|      ! 0 |  4412 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  4413 | `			"for: Expected ';' after initialization expressions");` |
|      ! 0 |  4414 | `		if( rc == SXERR_ABORT ){` |
|        - |  4415 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4416 | `			return SXERR_ABORT;` |
|        - |  4417 | `		}` |
|      ! 0 |  4418 | `		return SXRET_OK;` |
|        - |  4419 | `	}` |
|        - |  4420 | `	/* Jump the trailing ';' */` |
|    38985 |  4421 | `	pGen->pIn++;` |
|        - |  4422 | `	/* Create the loop block */` |
|    38985 |  4423 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|    38985 |  4424 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4425 | `		return SXERR_ABORT;` |
|        - |  4426 | `	}` |
|        - |  4427 | `	/* Deffer continue jumps */` |
|    38985 |  4428 | `	pForBlock->bPostContinue = TRUE;` |
|        - |  4429 | `	/* Compile the condition */` |
|    38985 |  4430 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    38985 |  4431 | `	if( rc == SXERR_ABORT ){` |
|        - |  4432 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4433 | `		return SXERR_ABORT;` |
|    38985 |  4434 | `	}else if( rc != SXERR_EMPTY ){` |
|        - |  4435 | `		/* Emit the false jump */` |
|    38983 |  4436 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - |  4437 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    38983 |  4438 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    19489 |  4439 | `	}` |
|    38985 |  4440 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  4441 | `		/* Syntax error */` |
|        6 |  4442 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  4443 | `			"for: Expected ';' after conditionals expressions");` |
|        6 |  4444 | `		if( rc == SXERR_ABORT ){` |
|        - |  4445 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4446 | `			return SXERR_ABORT;` |
|        - |  4447 | `		}` |
|        6 |  4448 | `		return SXRET_OK;` |
|        - |  4449 | `	}` |
|        - |  4450 | `	/* Jump the trailing ';' */` |
|    38981 |  4451 | `	pGen->pIn++;` |
|        - |  4452 | `	/* Save the post condition stream */` |
|    38981 |  4453 | `	pPostStart = pGen->pIn;` |
|        - |  4454 | `	/* Compile the loop body */` |
|    38981 |  4455 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|    38981 |  4456 | `	pGen->pEnd = pTmp;` |
|    38981 |  4457 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|    38981 |  4458 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  4459 | `		return SXERR_ABORT;` |
|        - |  4460 | `	}` |
|        - |  4461 | `	/* Fix post-continue jumps */` |
|    38981 |  4462 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|        - |  4463 | `		JumpFixup *aPost;` |
|        - |  4464 | `		VmInstr *pInstr;` |
|        - |  4465 | `		sxu32 nJumpDest;` |
|        - |  4466 | `		sxu32 n;` |
|       14 |  4467 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|       14 |  4468 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       26 |  4469 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|       14 |  4470 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|       14 |  4471 | `			if( pInstr ){` |
|        - |  4472 | `				/* Fix jump */` |
|       14 |  4473 | `				pInstr->iP2 = nJumpDest;` |
|        6 |  4474 | `			}` |
|        8 |  4475 | `		}` |
|        6 |  4476 | `	}` |
|        - |  4477 | `	/* compile the post-expressions if available */` |
|    38981 |  4478 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|      ! 0 |  4479 | `		pPostStart++;` |
|      ! 0 |  4480 | `	}` |
|    38981 |  4481 | `	if( pPostStart < pEnd ){` |
|        - |  4482 | `		SyToken *pTmpIn,*pTmpEnd;` |
|    38981 |  4483 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|    38981 |  4484 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    38981 |  4485 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - |  4486 | `			/* Syntax error */` |
|      ! 0 |  4487 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|      ! 0 |  4488 | `			if( rc == SXERR_ABORT ){` |
|        - |  4489 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4490 | `				return SXERR_ABORT;` |
|        - |  4491 | `			}` |
|      ! 0 |  4492 | `			return SXRET_OK;` |
|        - |  4493 | `		}` |
|    38981 |  4494 | `		RE_SWAP_DELIMITER(pGen);` |
|    38981 |  4495 | `		if( rc == SXERR_ABORT ){` |
|        - |  4496 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4497 | `			return SXERR_ABORT;` |
|    38981 |  4498 | `		}else if( rc != SXERR_EMPTY){` |
|        - |  4499 | `			/* Pop operand lvalue */` |
|    38981 |  4500 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    19488 |  4501 | `		}` |
|    19488 |  4502 | `	}` |
|        - |  4503 | `	/* Emit the unconditional jump to the start of the loop */` |
|    38981 |  4504 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|        - |  4505 | `	/* Fix all jumps now the destination is resolved */` |
|    38981 |  4506 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4507 | `	/* Release the loop block */` |
|    38981 |  4508 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4509 | `	/* Statement successfully compiled */` |
|    38981 |  4510 | `	return SXRET_OK;` |
|    19495 |  4511 | `}` |
|        - |  4512 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|        - |  4513 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|        - |  4514 | ` * are allowed.` |
|        - |  4515 | ` */` |
|   241616 |  4516 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 |  4517 | `{` |
|   241621 |  4518 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|   241621 |  4519 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|        - |  4520 | `		/* Unexpected expression */` |
|      ! 0 |  4521 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - |  4522 | `			"foreach: Expecting a variable name");` |
|      ! 0 |  4523 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 |  4524 | `			rc = SXERR_INVALID;` |
|      ! 0 |  4525 | `		}` |
|      ! 0 |  4526 | `	}` |
|   241621 |  4527 | `	return rc;` |
|        5 |  4528 | `}` |
|        - |  4529 | `/*` |
|        - |  4530 | ` * Compile the 'foreach' statement.` |
|        - |  4531 | ` * According to the PHP language reference` |
|        - |  4532 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|        - |  4533 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|        - |  4534 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|        - |  4535 | ` *  is a minor but useful extension of the first:` |
|        - |  4536 | ` *  foreach (array_expression as $value)` |
|        - |  4537 | ` *    statement` |
|        - |  4538 | ` *  foreach (array_expression as $key => $value)` |
|        - |  4539 | ` *   statement` |
|        - |  4540 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|        - |  4541 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|        - |  4542 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|        - |  4543 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|        - |  4544 | ` *  to the variable $key on each loop.` |
|        - |  4545 | ` *  Note:` |
|        - |  4546 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|        - |  4547 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|        - |  4548 | ` *  Note:` |
|        - |  4549 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|        - |  4550 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|        - |  4551 | ` *  or after the foreach without resetting it.` |
|        - |  4552 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|        - |  4553 | ` *  of copying the value.` |
|        - |  4554 | ` */` |
|   175378 |  4555 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|        5 |  4556 | `{` |
|   175383 |  4557 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|   175383 |  4558 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|   175383 |  4559 | `	GenBlock *pForeachBlock = 0;` |
|        - |  4560 | `	ph7_foreach_info *pInfo;` |
|        - |  4561 | `	sxu32 nFalseJump;` |
|        - |  4562 | `	VmInstr *pInstr;` |
|        - |  4563 | `	sxu32 nLine;` |
|        - |  4564 | `	sxi32 rc;` |
|   175383 |  4565 | `	nLine = pGen->pIn->nLine;` |
|        - |  4566 | `	/* Jump the 'foreach' keyword */` |
|   175383 |  4567 | `	pGen->pIn++;` |
|   175383 |  4568 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4569 | `		/* Syntax error */` |
|      ! 0 |  4570 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|      ! 0 |  4571 | `		if( rc == SXERR_ABORT ){` |
|        - |  4572 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4573 | `			return SXERR_ABORT;` |
|        - |  4574 | `		}` |
|      ! 0 |  4575 | `		goto Synchronize;` |
|        - |  4576 | `	}` |
|        - |  4577 | `	/* Jump the left parenthesis '(' */` |
|   175383 |  4578 | `	pGen->pIn++;` |
|        - |  4579 | `	/* Create the loop block */` |
|   175383 |  4580 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|   175383 |  4581 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4582 | `		return SXERR_ABORT;` |
|        - |  4583 | `	}` |
|        - |  4584 | `	/* Delimit the expression */` |
|   175383 |  4585 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   175383 |  4586 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  4587 | `		/* Empty expression */` |
|      ! 0 |  4588 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|      ! 0 |  4589 | `		if( rc == SXERR_ABORT ){` |
|        - |  4590 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4591 | `			return SXERR_ABORT;` |
|        - |  4592 | `		}` |
|        - |  4593 | `		/* Synchronize */` |
|      ! 0 |  4594 | `		pGen->pIn = pEnd;` |
|      ! 0 |  4595 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  4596 | `			pGen->pIn++;` |
|      ! 0 |  4597 | `		}` |
|      ! 0 |  4598 | `		return SXRET_OK;` |
|        - |  4599 | `	}` |
|        - |  4600 | `	/* Compile the array expression */` |
|   175383 |  4601 | `	pCur = pGen->pIn;` |
|  1024999 |  4602 | `	while( pCur < pEnd ){` |
|  1024999 |  4603 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|   179281 |  4604 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|   179281 |  4605 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|        - |  4606 | `				/* Break with the first 'as' found */` |
|   175383 |  4607 | `				break;` |
|        - |  4608 | `			}` |
|     1949 |  4609 | `		}` |
|        - |  4610 | `		/* Advance the stream cursor */` |
|   849621 |  4611 | `		pCur++;` |
|        5 |  4612 | `	}` |
|   175383 |  4613 | `	if( pCur <= pGen->pIn ){` |
|      ! 0 |  4614 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  4615 | `			"foreach: Missing array/object expression");` |
|      ! 0 |  4616 | `		if( rc == SXERR_ABORT ){` |
|        - |  4617 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4618 | `			return SXERR_ABORT;` |
|        - |  4619 | `		}` |
|      ! 0 |  4620 | `		goto Synchronize;` |
|        - |  4621 | `	}` |
|        - |  4622 | `	/* Swap token streams */` |
|   175383 |  4623 | `	pTmp = pGen->pEnd;` |
|   175383 |  4624 | `	pGen->pEnd = pCur;` |
|   175383 |  4625 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   175383 |  4626 | `	if( rc == SXERR_ABORT ){` |
|        - |  4627 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4628 | `		return SXERR_ABORT;` |
|        - |  4629 | `	}` |
|        - |  4630 | `	/* Update token stream */` |
|   175383 |  4631 | `	while(pGen->pIn < pCur ){` |
|      ! 0 |  4632 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4633 | `		if( rc == SXERR_ABORT ){` |
|        - |  4634 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4635 | `			return SXERR_ABORT;` |
|        - |  4636 | `		}` |
|      ! 0 |  4637 | `		pGen->pIn++;` |
|      ! 0 |  4638 | `	}` |
|   175383 |  4639 | `	pCur++; /* Jump the 'as' keyword */` |
|   175383 |  4640 | `	pGen->pIn = pCur;` |
|   175383 |  4641 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 |  4642 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|      ! 0 |  4643 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4644 | `			return SXERR_ABORT;` |
|        - |  4645 | `		}` |
|      ! 0 |  4646 | `	}` |
|        - |  4647 | `	/* Create the foreach context */` |
|   175383 |  4648 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|   175383 |  4649 | `	if( pInfo == 0 ){` |
|      ! 0 |  4650 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 |  4651 | `		return SXERR_ABORT;` |
|        - |  4652 | `	}` |
|        - |  4653 | `	/* Zero the structure */` |
|   175383 |  4654 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|        - |  4655 | `	/* Initialize structure fields */` |
|   175383 |  4656 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|        - |  4657 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|        - |  4658 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|        - |  4659 | `	 * '=>'. */` |
|   175383 |  4660 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|   175383 |  4661 | `	if( pCur < pEnd ){` |
|        - |  4662 | `		/* Compile the expression holding the key name */` |
|    66263 |  4663 | `		if( pGen->pIn >= pCur ){` |
|      ! 0 |  4664 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|      ! 0 |  4665 | `			if( rc == SXERR_ABORT ){` |
|        - |  4666 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4667 | `				return SXERR_ABORT;` |
|        - |  4668 | `			}` |
|      ! 0 |  4669 | `		}else{` |
|    66263 |  4670 | `			pGen->pEnd = pCur;` |
|    66263 |  4671 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    66263 |  4672 | `			if( rc == SXERR_ABORT ){` |
|        - |  4673 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4674 | `				return SXERR_ABORT;` |
|        - |  4675 | `			}` |
|    66263 |  4676 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    66263 |  4677 | `			if( pInstr->p3 ){` |
|        - |  4678 | `				/* Record key name */` |
|    66263 |  4679 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    33129 |  4680 | `			}` |
|    66263 |  4681 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|        - |  4682 | `		}` |
|    66263 |  4683 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    33129 |  4684 | `	}` |
|   175383 |  4685 | `	pGen->pEnd = pEnd;` |
|   175383 |  4686 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 |  4687 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|      ! 0 |  4688 | `		if( rc == SXERR_ABORT ){` |
|        - |  4689 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4690 | `			return SXERR_ABORT;` |
|        - |  4691 | `		}` |
|      ! 0 |  4692 | `		goto Synchronize;` |
|        - |  4693 | `	}` |
|   175383 |  4694 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|       31 |  4695 | `		pGen->pIn++;` |
|        - |  4696 | `		/* Pass by reference  */` |
|       31 |  4697 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       14 |  4698 | `	}` |
|        - |  4699 | `	/* Check if the value target is list() */` |
|   175383 |  4700 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        8 |  4701 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|        - |  4702 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|        - |  4703 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|        - |  4704 | `		 */` |
|        - |  4705 | `		static int iForeachListCnt = 0;` |
|        - |  4706 | `		char zTmp[128];` |
|        - |  4707 | `		sxu32 nLen;` |
|        - |  4708 | `		char *zDup;` |
|       10 |  4709 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|       10 |  4710 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       10 |  4711 | `		if( zDup == 0 ){` |
|      ! 0 |  4712 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  4713 | `			return SXERR_ABORT;` |
|        - |  4714 | `		}` |
|       10 |  4715 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - |  4716 | `		/* Save list() token boundaries */` |
|       10 |  4717 | `		pListStart = pGen->pIn;` |
|        - |  4718 | `		/* Advance past list(...) — validate parentheses */` |
|       10 |  4719 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|       10 |  4720 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        3 |  4721 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|        - |  4722 | `				"foreach: Expected '(' after 'list'");` |
|        3 |  4723 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4724 | `				return SXERR_ABORT;` |
|        - |  4725 | `			}` |
|        3 |  4726 | `			goto Synchronize;` |
|        - |  4727 | `		}` |
|        7 |  4728 | `		pGen->pIn++; /* Jump '(' */` |
|        7 |  4729 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|        7 |  4730 | `		if( pListEnd >= pEnd ){` |
|      ! 0 |  4731 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  4732 | `				"foreach: Missing closing ')' after list");` |
|      ! 0 |  4733 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4734 | `				return SXERR_ABORT;` |
|        - |  4735 | `			}` |
|      ! 0 |  4736 | `			goto Synchronize;` |
|        - |  4737 | `		}` |
|        7 |  4738 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|        7 |  4739 | `		pListEnd = pGen->pIn;` |
|        7 |  4740 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|   175378 |  4741 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|        - |  4742 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|        - |  4743 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|        - |  4744 | `		 */` |
|        - |  4745 | `		static int iForeachShortListCnt = 0;` |
|        - |  4746 | `		char zTmp[128];` |
|        - |  4747 | `		sxu32 nLen;` |
|        - |  4748 | `		char *zDup;` |
|       13 |  4749 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       13 |  4750 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       13 |  4751 | `		if( zDup == 0 ){` |
|      ! 0 |  4752 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  4753 | `			return SXERR_ABORT;` |
|        - |  4754 | `		}` |
|       13 |  4755 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - |  4756 | `		/* Save [...] token boundaries */` |
|       13 |  4757 | `		pListStart = pGen->pIn;` |
|        - |  4758 | `		/* Advance past [...] */` |
|       13 |  4759 | `		pGen->pIn++; /* Jump '[' */` |
|       13 |  4760 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       13 |  4761 | `		if( pListEnd >= pEnd ){` |
|      ! 0 |  4762 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  4763 | `				"foreach: Missing closing ']' after short list");` |
|      ! 0 |  4764 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4765 | `				return SXERR_ABORT;` |
|        - |  4766 | `			}` |
|      ! 0 |  4767 | `			goto Synchronize;` |
|        - |  4768 | `		}` |
|       13 |  4769 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       13 |  4770 | `		pListEnd = pGen->pIn;` |
|       13 |  4771 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|        7 |  4772 | `	}else{` |
|        - |  4773 | `		/* Compile the expression holding the value name */` |
|   175363 |  4774 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   175363 |  4775 | `		if( rc == SXERR_ABORT ){` |
|        - |  4776 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4777 | `			return SXERR_ABORT;` |
|        - |  4778 | `		}` |
|   175363 |  4779 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   175363 |  4780 | `		if( pInstr->p3 ){` |
|        - |  4781 | `			/* Record value name */` |
|   175363 |  4782 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    87679 |  4783 | `		}` |
|        - |  4784 | `	}` |
|        - |  4785 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|   175381 |  4786 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|        - |  4787 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   175381 |  4788 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|        - |  4789 | `	/* Record the first instruction to execute */` |
|   175381 |  4790 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|        - |  4791 | `	/* Emit the FOREACH_STEP instruction */` |
|   175381 |  4792 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|        - |  4793 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   175381 |  4794 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|        - |  4795 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|   175381 |  4796 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|        - |  4797 | `		SyToken *pSavedIn,*pSavedEnd;` |
|        - |  4798 | `		/* Load the temporary variable holding the current value onto the stack.` |
|        - |  4799 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|        - |  4800 | `		 */` |
|       19 |  4801 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|        - |  4802 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|        - |  4803 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|        - |  4804 | `		 * picks up the delimiter and the variable names inside.` |
|        - |  4805 | `		 */` |
|       19 |  4806 | `		pSavedIn = pGen->pIn;` |
|       19 |  4807 | `		pSavedEnd = pGen->pEnd;` |
|       19 |  4808 | `		pGen->pIn = pListStart;` |
|       19 |  4809 | `		pGen->pEnd = pListEnd;` |
|       19 |  4810 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       13 |  4811 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|        7 |  4812 | `		}else{` |
|        7 |  4813 | `			rc = PH7_CompileList(&(*pGen),0);` |
|        - |  4814 | `		}` |
|       19 |  4815 | `		pGen->pIn = pSavedIn;` |
|       19 |  4816 | `		pGen->pEnd = pSavedEnd;` |
|       19 |  4817 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4818 | `			return SXERR_ABORT;` |
|        - |  4819 | `		}` |
|        - |  4820 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       19 |  4821 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        9 |  4822 | `	}` |
|        - |  4823 | `	/* Compile the loop body */` |
|   175381 |  4824 | `	pGen->pIn = &pEnd[1];` |
|   175381 |  4825 | `	pGen->pEnd = pTmp;` |
|   175381 |  4826 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|   175381 |  4827 | `	if( rc == SXERR_ABORT ){` |
|        - |  4828 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4829 | `		return SXERR_ABORT;` |
|        - |  4830 | `	}` |
|        - |  4831 | `	/* Emit the unconditional jump to the start of the loop */` |
|   175381 |  4832 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|        - |  4833 | `	/* Fix all jumps now the destination is resolved */` |
|   175381 |  4834 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4835 | `	/* Release the loop block */` |
|   175381 |  4836 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4837 | `	/* Statement successfully compiled */` |
|   175381 |  4838 | `	return SXRET_OK;` |
|        1 |  4839 | `Synchronize:` |
|        - |  4840 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  4841 | `	 * compiling this erroneous block.` |
|        - |  4842 | `	 */` |
|        3 |  4843 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  4844 | `		pGen->pIn++;` |
|      ! 0 |  4845 | `	}` |
|        3 |  4846 | `	return SXRET_OK;` |
|    87694 |  4847 | `}` |
|        - |  4848 | `/*` |
|        - |  4849 | ` * Compile the infamous if/elseif/else if/else statements.` |
|        - |  4850 | ` * According to the PHP language reference` |
|        - |  4851 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|        - |  4852 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|        - |  4853 | ` *  that is similar to that of C:` |
|        - |  4854 | ` *  if (expr)` |
|        - |  4855 | ` *   statement` |
|        - |  4856 | ` *  else construct:` |
|        - |  4857 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|        - |  4858 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|        - |  4859 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|        - |  4860 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|        - |  4861 | ` *   $b, and a is NOT greater than b otherwise.` |
|        - |  4862 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|        - |  4863 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|        - |  4864 | ` *  elseif` |
|        - |  4865 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|        - |  4866 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|        - |  4867 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|        - |  4868 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|        - |  4869 | ` *   than b, a equal to b or a is smaller than b:` |
|        - |  4870 | ` *   <?php` |
|        - |  4871 | ` *    if ($a > $b) {` |
|        - |  4872 | ` *     echo "a is bigger than b";` |
|        - |  4873 | ` *    } elseif ($a == $b) {` |
|        - |  4874 | ` *     echo "a is equal to b";` |
|        - |  4875 | ` *    } else {` |
|        - |  4876 | ` *     echo "a is smaller than b";` |
|        - |  4877 | ` *    }` |
|        - |  4878 | ` *    ?>` |
|        - |  4879 | ` */` |
|  1183342 |  4880 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|        5 |  4881 | `{` |
|  1183347 |  4882 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  1183347 |  4883 | `	GenBlock *pCondBlock = 0;` |
|        - |  4884 | `	sxu32 nJumpIdx;` |
|        - |  4885 | `	sxu32 nKeyID;` |
|        - |  4886 | `	sxi32 rc;` |
|        - |  4887 | `	/* Jump the 'if' keyword */` |
|  1183347 |  4888 | `	pGen->pIn++;` |
|  1183347 |  4889 | `	pToken = pGen->pIn;` |
|        - |  4890 | `	/* Create the conditional block */` |
|  1183347 |  4891 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  1183347 |  4892 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4893 | `		return SXERR_ABORT;` |
|        - |  4894 | `	}` |
|        - |  4895 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   638338 |  4896 | `	for(;;){` |
|  1276681 |  4897 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4898 | `			/* Syntax error */` |
|      ! 0 |  4899 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 |  4900 | `				pToken--;` |
|      ! 0 |  4901 | `			}` |
|      ! 0 |  4902 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|      ! 0 |  4903 | `			if( rc == SXERR_ABORT ){` |
|        - |  4904 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4905 | `				return SXERR_ABORT;` |
|        - |  4906 | `			}` |
|      ! 0 |  4907 | `			goto Synchronize;` |
|        - |  4908 | `		}` |
|        - |  4909 | `		/* Jump the left parenthesis '(' */` |
|  1276681 |  4910 | `		pToken++;` |
|        - |  4911 | `		/* Delimit the condition */` |
|  1276681 |  4912 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  1276681 |  4913 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|        - |  4914 | `			/* Syntax error */` |
|      ! 0 |  4915 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 |  4916 | `				pToken--;` |
|      ! 0 |  4917 | `			}` |
|      ! 0 |  4918 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|      ! 0 |  4919 | `			if( rc == SXERR_ABORT ){` |
|        - |  4920 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4921 | `				return SXERR_ABORT;` |
|        - |  4922 | `			}` |
|      ! 0 |  4923 | `			goto Synchronize;` |
|        - |  4924 | `		}` |
|        - |  4925 | `		/* Swap token streams */` |
|  1276681 |  4926 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|        - |  4927 | `		/* Compile the condition */` |
|  1276681 |  4928 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  4929 | `		/* Update token stream */` |
|  1276681 |  4930 | `		while(pGen->pIn < pEnd ){` |
|      ! 0 |  4931 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4932 | `			pGen->pIn++;` |
|      ! 0 |  4933 | `		}` |
|  1276681 |  4934 | `		pGen->pIn  = &pEnd[1];` |
|  1276681 |  4935 | `		pGen->pEnd = pTmp;` |
|  1276681 |  4936 | `		if( rc == SXERR_ABORT ){` |
|        - |  4937 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4938 | `			return SXERR_ABORT;` |
|        - |  4939 | `		}` |
|        - |  4940 | `		/* Emit the false jump */` |
|  1276681 |  4941 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|        - |  4942 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  1276681 |  4943 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|        - |  4944 | `		/* Compile the body */` |
|  1276681 |  4945 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  1276681 |  4946 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4947 | `			return SXERR_ABORT;` |
|        - |  4948 | `		}` |
|  1276681 |  4949 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   239764 |  4950 | `			break;` |
|        - |  4951 | `		}` |
|        - |  4952 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   797163 |  4953 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   797163 |  4954 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   617909 |  4955 | `			break;` |
|        - |  4956 | `		}` |
|        - |  4957 | `		/* Emit the unconditional jump */` |
|   179259 |  4958 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|        - |  4959 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   179259 |  4960 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   179259 |  4961 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   171373 |  4962 | `			pToken = &pGen->pIn[1];` |
|   171373 |  4963 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    85486 |  4964 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    42965 |  4965 | `					break;` |
|        - |  4966 | `			}` |
|    85453 |  4967 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    42724 |  4968 | `		}` |
|    93339 |  4969 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|        - |  4970 | `		/* Synchronize cursors */` |
|    93339 |  4971 | `		pToken = pGen->pIn;` |
|        - |  4972 | `		/* Fix the false jump */` |
|    93339 |  4973 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|        5 |  4974 | `	} /* For(;;) */` |
|        - |  4975 | `	/* Fix the false jump */` |
|  1183347 |  4976 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  1183347 |  4977 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   703824 |  4978 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|        - |  4979 | `			/* Compile the else block */` |
|    85925 |  4980 | `			pGen->pIn++;` |
|    85925 |  4981 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    85925 |  4982 | `			if( rc == SXERR_ABORT ){` |
|        - |  4983 |  |
|      ! 0 |  4984 | `				return SXERR_ABORT;` |
|        - |  4985 | `			}` |
|    42960 |  4986 | `	}` |
|  1183347 |  4987 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|        - |  4988 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  1183347 |  4989 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|        - |  4990 | `	/* Release the conditional block */` |
|  1183347 |  4991 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4992 | `	/* Statement successfully compiled */` |
|  1183347 |  4993 | `	return SXRET_OK;` |
|      ! 0 |  4994 | `Synchronize:` |
|        - |  4995 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|        - |  4996 | `	 */` |
|      ! 0 |  4997 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  4998 | `		pGen->pIn++;` |
|      ! 0 |  4999 | `	}` |
|      ! 0 |  5000 | `	return SXRET_OK;` |
|   591676 |  5001 | `}` |
|        - |  5002 | `/*` |
|        - |  5003 | ` * Compile the global construct.` |
|        - |  5004 | ` * According to the PHP language reference` |
|        - |  5005 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|        - |  5006 | ` *  to be used in that function.` |
|        - |  5007 | ` *  Example #1 Using global` |
|        - |  5008 | ` *  <?php` |
|        - |  5009 | ` *   $a = 1;` |
|        - |  5010 | ` *   $b = 2;` |
|        - |  5011 | ` *   function Sum()` |
|        - |  5012 | ` *   {` |
|        - |  5013 | ` *    global $a, $b;` |
|        - |  5014 | ` *    $b = $a + $b;` |
|        - |  5015 | ` *   }` |
|        - |  5016 | ` *   Sum();` |
|        - |  5017 | ` *   echo $b;` |
|        - |  5018 | ` *  ?>` |
|        - |  5019 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|        - |  5020 | ` *  all references to either variable will refer to the global version. There is no limit` |
|        - |  5021 | ` *  to the number of global variables that can be manipulated by a function.` |
|        - |  5022 | ` */` |
|       36 |  5023 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|        5 |  5024 | `{` |
|       41 |  5025 | `	SyToken *pTmp,*pNext = 0;` |
|        - |  5026 | `	sxi32 nExpr;` |
|        - |  5027 | `	sxi32 rc;` |
|        - |  5028 | `	/* Jump the 'global' keyword */` |
|       41 |  5029 | `	pGen->pIn++;` |
|       41 |  5030 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        - |  5031 | `		/* Nothing to process */` |
|      ! 0 |  5032 | `		return SXRET_OK;` |
|        - |  5033 | `	}` |
|       41 |  5034 | `	pTmp = pGen->pEnd;` |
|       41 |  5035 | `	nExpr = 0;` |
|       87 |  5036 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       51 |  5037 | `		if( pGen->pIn < pNext ){` |
|       51 |  5038 | `			pGen->pEnd = pNext;` |
|       51 |  5039 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  5040 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|      ! 0 |  5041 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  5042 | `					return SXERR_ABORT;` |
|        - |  5043 | `				}` |
|      ! 0 |  5044 | `			}else{` |
|       51 |  5045 | `				pGen->pIn++;` |
|       51 |  5046 | `				if( pGen->pIn >= pGen->pEnd ){` |
|        - |  5047 | `					/* Emit a warning */` |
|      ! 0 |  5048 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|      ! 0 |  5049 | `				}else{` |
|       51 |  5050 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       51 |  5051 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  5052 | `						return SXERR_ABORT;` |
|       51 |  5053 | `					}else if(rc != SXERR_EMPTY ){` |
|       51 |  5054 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|       51 |  5055 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|        - |  5056 | `							/* Variable name, not a constant */` |
|       51 |  5057 | `							pLast->iP1 = 0;` |
|       23 |  5058 | `						}` |
|       51 |  5059 | `						nExpr++;` |
|       23 |  5060 | `					}` |
|        - |  5061 | `				}` |
|        - |  5062 | `			}` |
|       23 |  5063 | `		}` |
|        - |  5064 | `		/* Next expression in the stream */` |
|       51 |  5065 | `		pGen->pIn = pNext;` |
|        - |  5066 | `		/* Jump trailing commas */` |
|       61 |  5067 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       15 |  5068 | `			pGen->pIn++;` |
|        5 |  5069 | `		}` |
|        5 |  5070 | `	}` |
|        - |  5071 | `	/* Restore token stream */` |
|       41 |  5072 | `	pGen->pEnd = pTmp;` |
|       41 |  5073 | `	if( nExpr > 0 ){` |
|        - |  5074 | `		/* Emit the uplink instruction */` |
|       41 |  5075 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|       18 |  5076 | `	}` |
|       41 |  5077 | `	return SXRET_OK;` |
|       23 |  5078 | `}` |
|        - |  5079 | `/*` |
|        - |  5080 | ` * Compile the return statement.` |
|        - |  5081 | ` * According to the PHP language reference` |
|        - |  5082 | ` *  If called from within a function, the return() statement immediately ends execution` |
|        - |  5083 | ` *  of the current function, and returns its argument as the value of the function call.` |
|        - |  5084 | ` *  return() will also end the execution of an eval() statement or script file.` |
|        - |  5085 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|        - |  5086 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|        - |  5087 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|        - |  5088 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|        - |  5089 | ` *  from within the main script file, then script execution end.` |
|        - |  5090 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|        - |  5091 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|        - |  5092 | ` *  should do so as PHP has less work to do in this case.` |
|        - |  5093 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|        - |  5094 | ` */` |
|  1625486 |  5095 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|        5 |  5096 | `{` |
|  1625491 |  5097 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|        - |  5098 | `	sxi32 rc;` |
|  1625491 |  5099 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  1625491 |  5100 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|        - |  5101 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|        - |  5102 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|        - |  5103 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|        - |  5104 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|        - |  5105 | `	 * normally below so token processing stays consistent. */` |
|  4238311 |  5106 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  2612825 |  5107 | `		pFuncBlock = pFuncBlock->pParent;` |
|        5 |  5108 | `	}` |
|  1625486 |  5109 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|  1625459 |  5110 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|        3 |  5111 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  5112 | `			"A never-returning function must not return");` |
|        3 |  5113 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5114 | `			return SXERR_ABORT;` |
|        - |  5115 | `		}` |
|        1 |  5116 | `	}` |
|        - |  5117 | `	/* Jump the 'return' keyword */` |
|  1625491 |  5118 | `	pGen->pIn++;` |
|  1625491 |  5119 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  5120 | `		/* Compile the expression */` |
|  1609925 |  5121 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  1609925 |  5122 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5123 | `			return SXERR_ABORT;` |
|  1609925 |  5124 | `		}else if(rc != SXERR_EMPTY ){` |
|  1609925 |  5125 | `			nRet = 1;` |
|   804960 |  5126 | `		}` |
|   804960 |  5127 | `	}` |
|        - |  5128 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|        - |  5129 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|        - |  5130 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|        - |  5131 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|  1625491 |  5132 | `	if( pGen->bInGenerator ){` |
|       32 |  5133 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|       32 |  5134 | `		return SXRET_OK;` |
|        - |  5135 | `	}` |
|        - |  5136 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|        - |  5137 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|        - |  5138 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|        - |  5139 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|        - |  5140 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  1625463 |  5141 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  1625463 |  5142 | `	return SXRET_OK;` |
|   812748 |  5143 | `}` |
|        - |  5144 | `/*` |
|        - |  5145 | ` * Compile a yield expression.` |
|        - |  5146 | ` * Called from the expression code generator when a yield node is encountered.` |
|        - |  5147 | ` * Handles: yield, yield $value, yield $key => $value` |
|        - |  5148 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|        - |  5149 | ` */` |
|      384 |  5150 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        5 |  5151 | `{` |
|        - |  5152 | `	SyToken *pTmp, *pSplit;` |
|      389 |  5153 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      389 |  5154 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|        - |  5155 | `	sxi32 rc;` |
|      192 |  5156 | `	(void)iCompileFlag;` |
|        - |  5157 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      389 |  5158 | `	pGen->pIn++;` |
|        - |  5159 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|        - |  5160 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|        - |  5161 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|        - |  5162 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|        - |  5163 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|      384 |  5164 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|      227 |  5165 | `		&& pGen->pIn->sData.nByte == 4` |
|       72 |  5166 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|       67 |  5167 | `		pGen->pIn++; /* Skip 'from' */` |
|       67 |  5168 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       67 |  5169 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5170 | `			return SXERR_ABORT;` |
|        - |  5171 | `		}` |
|       67 |  5172 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 |  5173 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|      ! 0 |  5174 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|        - |  5175 | `				"Missing expression after 'yield from'");` |
|      ! 0 |  5176 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5177 | `				return SXERR_ABORT;` |
|        - |  5178 | `			}` |
|      ! 0 |  5179 | `		}` |
|       67 |  5180 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|       67 |  5181 | `		return SXRET_OK;` |
|        - |  5182 | `	}` |
|      327 |  5183 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  5184 | `		/* Bare yield — no value */` |
|        3 |  5185 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|        3 |  5186 | `		return SXRET_OK;` |
|        - |  5187 | `	}` |
|        - |  5188 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|      325 |  5189 | `	pSplit = 0;` |
|        - |  5190 | `	{` |
|      325 |  5191 | `		SyToken *pCur = pGen->pIn;` |
|      325 |  5192 | `		sxi32 nNest = 0;` |
|      781 |  5193 | `		while( pCur < pGen->pEnd ){` |
|      475 |  5194 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       18 |  5195 | `				nNest++;` |
|      467 |  5196 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       18 |  5197 | `				nNest--;` |
|      451 |  5198 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|       16 |  5199 | `				pSplit = pCur;` |
|       16 |  5200 | `				break;` |
|        - |  5201 | `			}` |
|      461 |  5202 | `			pCur++;` |
|        5 |  5203 | `		}` |
|        - |  5204 | `	}` |
|      325 |  5205 | `	pTmp = pGen->pEnd;` |
|      325 |  5206 | `	if( pSplit ){` |
|        - |  5207 | `		/* yield $key => $value */` |
|       16 |  5208 | `		pGen->pEnd = pSplit;` |
|       16 |  5209 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       16 |  5210 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       16 |  5211 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|       16 |  5212 | `		pGen->pEnd = pTmp;` |
|       16 |  5213 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       16 |  5214 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       16 |  5215 | `		iP1 = 1;` |
|       16 |  5216 | `		iP2 = 1;` |
|        9 |  5217 | `	}else{` |
|        - |  5218 | `		/* yield $value */` |
|      311 |  5219 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      311 |  5220 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      311 |  5221 | `		if( rc != SXERR_EMPTY ){` |
|      311 |  5222 | `			iP1 = 1;` |
|      153 |  5223 | `		}` |
|        - |  5224 | `	}` |
|      325 |  5225 | `	pGen->pEnd = pTmp;` |
|      325 |  5226 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      325 |  5227 | `	return SXRET_OK;` |
|      197 |  5228 | `}` |
|        - |  5229 | `/*` |
|        - |  5230 | ` * Compile the die/exit language construct.` |
|        - |  5231 | ` * The role of these constructs is to terminate execution of the script.` |
|        - |  5232 | ` * Shutdown functions will always be executed even if exit() is called.` |
|        - |  5233 | ` */` |
|      122 |  5234 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|        5 |  5235 | `{` |
|      127 |  5236 | `	sxi32 nExpr = 0;` |
|        - |  5237 | `	sxi32 rc;` |
|        - |  5238 | `	/* Jump the die/exit keyword */` |
|      127 |  5239 | `	pGen->pIn++;` |
|      127 |  5240 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  5241 | `		/* Compile the expression */` |
|      127 |  5242 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      127 |  5243 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5244 | `			return SXERR_ABORT;` |
|      127 |  5245 | `		}else if(rc != SXERR_EMPTY ){` |
|      127 |  5246 | `			nExpr = 1;` |
|       61 |  5247 | `		}` |
|       61 |  5248 | `	}` |
|        - |  5249 | `	/* Emit the HALT instruction */` |
|      127 |  5250 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      127 |  5251 | `	return SXRET_OK;` |
|       66 |  5252 | `}` |
|        - |  5253 | `/*` |
|        - |  5254 | ` * Compile the 'echo' language construct.` |
|        - |  5255 | ` */` |
|    17070 |  5256 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|        5 |  5257 | `{` |
|    17075 |  5258 | `	SyToken *pTmp,*pNext = 0;` |
|        - |  5259 | `	sxi32 rc;` |
|        - |  5260 | `	/* Jump the 'echo' keyword */` |
|    17075 |  5261 | `	pGen->pIn++;` |
|        - |  5262 | `	/* Compile arguments one after one */` |
|    17075 |  5263 | `	pTmp = pGen->pEnd;` |
|    41693 |  5264 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|    24623 |  5265 | `		if( pGen->pIn < pNext ){` |
|    24623 |  5266 | `			pGen->pEnd = pNext;` |
|    24623 |  5267 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|    24623 |  5268 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5269 | `				return SXERR_ABORT;` |
|    24623 |  5270 | `			}else if( rc != SXERR_EMPTY ){` |
|        - |  5271 | `				/* Emit the consume instruction */` |
|    24599 |  5272 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    12297 |  5273 | `			}` |
|    12309 |  5274 | `		}` |
|        - |  5275 | `		/* Jump trailing commas */` |
|    32171 |  5276 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     7553 |  5277 | `			pNext++;` |
|        5 |  5278 | `		}` |
|    24623 |  5279 | `		pGen->pIn = pNext;` |
|        5 |  5280 | `	}` |
|        - |  5281 | `	/* Restore token stream */` |
|    17075 |  5282 | `	pGen->pEnd = pTmp;` |
|    17075 |  5283 | `	return SXRET_OK;` |
|     8540 |  5284 | `}` |
|        - |  5285 | `/*` |
|        - |  5286 | ` * Compile the static statement.` |
|        - |  5287 | ` * According to the PHP language reference` |
|        - |  5288 | ` *  Another important feature of variable scoping is the static variable.` |
|        - |  5289 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|        - |  5290 | ` *  when program execution leaves this scope.` |
|        - |  5291 | ` *  Static variables also provide one way to deal with recursive functions.` |
|        - |  5292 | ` * Symisc eXtension.` |
|        - |  5293 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|        - |  5294 | ` *  the zend engine would allow only simple scalar value.` |
|        - |  5295 | ` *  Example` |
|        - |  5296 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|        - |  5297 | ` *    Refer to the official documentation for more information on this feature.` |
|        - |  5298 | ` */` |
|       12 |  5299 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|        3 |  5300 | `{` |
|        - |  5301 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|        - |  5302 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|        - |  5303 | `	GenBlock *pBlock;` |
|        - |  5304 | `	SyString *pName;` |
|        - |  5305 | `	char *zDup;` |
|        - |  5306 | `	sxu32 nLine;` |
|        - |  5307 | `	sxi32 rc;` |
|        - |  5308 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|        - |  5309 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|        - |  5310 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|       12 |  5311 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|       10 |  5312 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|        1 |  5313 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|        3 |  5314 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        3 |  5315 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5316 | `			return SXERR_ABORT;` |
|        3 |  5317 | `		}else if( rc != SXERR_EMPTY ){` |
|        3 |  5318 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        1 |  5319 | `		}` |
|        3 |  5320 | `		return SXRET_OK;` |
|        - |  5321 | `	}` |
|        - |  5322 | `	/* Jump the static keyword */` |
|       13 |  5323 | `	nLine = pGen->pIn->nLine;` |
|       13 |  5324 | `	pGen->pIn++;` |
|        - |  5325 | `	/* Extract the enclosing function if any */` |
|       13 |  5326 | `	pBlock = pGen->pCurrent;` |
|       23 |  5327 | `	while( pBlock ){` |
|       23 |  5328 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       13 |  5329 | `			break;` |
|        - |  5330 | `		}` |
|        - |  5331 | `		/* Point to the upper block */` |
|       13 |  5332 | `		pBlock = pBlock->pParent;` |
|        3 |  5333 | `	}` |
|       13 |  5334 | `	if( pBlock == 0 ){` |
|        - |  5335 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|      ! 0 |  5336 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  5337 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|      ! 0 |  5338 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5339 | `				return SXERR_ABORT;` |
|        - |  5340 | `			}` |
|      ! 0 |  5341 | `			goto Synchronize;` |
|        - |  5342 | `		}` |
|        - |  5343 | `		/* Compile the expression holding the variable */` |
|      ! 0 |  5344 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      ! 0 |  5345 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5346 | `			return SXERR_ABORT;` |
|      ! 0 |  5347 | `		}else if( rc != SXERR_EMPTY ){` |
|        - |  5348 | `			/* Emit the POP instruction */` |
|      ! 0 |  5349 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 |  5350 | `		}` |
|      ! 0 |  5351 | `		return SXRET_OK;` |
|        - |  5352 | `	}` |
|       13 |  5353 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        - |  5354 | `	/* Make sure we are dealing with a valid statement */` |
|       13 |  5355 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|        8 |  5356 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        3 |  5357 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|        3 |  5358 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5359 | `				return SXERR_ABORT;` |
|        - |  5360 | `			}` |
|        3 |  5361 | `			goto Synchronize;` |
|        - |  5362 | `	}` |
|       10 |  5363 | `	pGen->pIn++;` |
|        - |  5364 | `	/* Extract variable name */` |
|       10 |  5365 | `	pName = &pGen->pIn->sData;` |
|       10 |  5366 | `	pGen->pIn++; /* Jump the var name */` |
|       10 |  5367 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|      ! 0 |  5368 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  5369 | `		goto Synchronize;` |
|        - |  5370 | `	}` |
|        - |  5371 | `	/* Initialize the structure describing the static variable */` |
|       10 |  5372 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       10 |  5373 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|        - |  5374 | `	/* Duplicate variable name */` |
|       10 |  5375 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       10 |  5376 | `	if( zDup == 0 ){` |
|      ! 0 |  5377 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  5378 | `		return SXERR_ABORT;` |
|        - |  5379 | `	}` |
|       10 |  5380 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|        - |  5381 | `	/* Check if we have an expression to compile */` |
|       10 |  5382 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|        - |  5383 | `		SySet *pInstrContainer;` |
|        - |  5384 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|        - |  5385 | `		 * Static variable can take any complex expression including function` |
|        - |  5386 | `		 * call as their initialization value.` |
|        - |  5387 | `		 * Example:` |
|        - |  5388 | `		 *		static $var = foo(1,4+5,bar());` |
|        - |  5389 | `		 */` |
|       10 |  5390 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|        - |  5391 | `		/* Swap bytecode container */` |
|       10 |  5392 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       10 |  5393 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|        - |  5394 | `		/* Compile the expression */` |
|       10 |  5395 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  5396 | `		/* Emit the done instruction */` |
|       10 |  5397 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        - |  5398 | `		/* Restore default bytecode container */` |
|       10 |  5399 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        4 |  5400 | `	}` |
|        - |  5401 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       10 |  5402 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       10 |  5403 | `	return SXRET_OK;` |
|        1 |  5404 | `Synchronize:` |
|        - |  5405 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|        - |  5406 | `	 * statement.` |
|        - |  5407 | `	 */` |
|        5 |  5408 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|        3 |  5409 | `		pGen->pIn++;` |
|        1 |  5410 | `	}` |
|        3 |  5411 | `	return SXRET_OK;` |
|        9 |  5412 | `}` |
|        - |  5413 | `/*` |
|        - |  5414 | ` * Compile the var statement.` |
|        - |  5415 | ` * Symisc Extension:` |
|        - |  5416 | ` *      var statement can be used outside of a class definition.` |
|        - |  5417 | ` */` |
|        4 |  5418 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|        1 |  5419 | `{` |
|        - |  5420 | `	sxu32 nLine;` |
|        - |  5421 | `	sxi32 rc;` |
|        5 |  5422 | `	nLine = pGen->pIn->nLine;` |
|        - |  5423 | `	/* Jump the 'var' keyword */` |
|        5 |  5424 | `	pGen->pIn++;` |
|        5 |  5425 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|      ! 0 |  5426 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|        - |  5427 | `		/* Synchronize with the first semi-colon */` |
|      ! 0 |  5428 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|      ! 0 |  5429 | `			pGen->pIn++;` |
|      ! 0 |  5430 | `		}` |
|      ! 0 |  5431 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5432 | `			return SXERR_ABORT;` |
|        - |  5433 | `		}` |
|      ! 0 |  5434 | `	}else{` |
|        - |  5435 | `		/* Compile the expression */` |
|        5 |  5436 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        5 |  5437 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5438 | `			return SXERR_ABORT;` |
|        5 |  5439 | `		}else if( rc != SXERR_EMPTY ){` |
|        5 |  5440 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        2 |  5441 | `		}` |
|        - |  5442 | `	}` |
|        5 |  5443 | `	return SXRET_OK;` |
|        3 |  5444 | `}` |
|        - |  5445 | `/*` |
|        - |  5446 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|        - |  5447 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|        - |  5448 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|        - |  5449 | ` */` |
|        - |  5450 | `/*` |
|        - |  5451 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|        - |  5452 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|        - |  5453 | ` * hash and any shared references), this creates a new literal entry with the` |
|        - |  5454 | ` * qualified name and updates the instruction's operand index.` |
|        - |  5455 | ` *` |
|        - |  5456 | ` * Resolution order:` |
|        - |  5457 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|        - |  5458 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|        - |  5459 | ` *   3. Otherwise return the original literal index unchanged.` |
|        - |  5460 | ` *` |
|        - |  5461 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|        - |  5462 | ` * came from an import (step 1) and 0 otherwise.` |
|        - |  5463 | ` * Returns the (possibly new) literal index.` |
|        - |  5464 | ` */` |
|  2884978 |  5465 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|        5 |  5466 | `{` |
|        - |  5467 | `	ph7_value *pLit;` |
|        - |  5468 | `	const char *zLit;` |
|        - |  5469 | `	SyString sQualified;` |
|        - |  5470 | `	sxu32 nLit;` |
|        - |  5471 | `	sxu32 k;` |
|        - |  5472 | `	sxu32 nNewIdx;` |
|        - |  5473 | `	int hasNsSep;` |
|        - |  5474 | `	SyHashEntry *pImport;` |
|        - |  5475 | `	ph7_value *pNew;` |
|  2884983 |  5476 | `	if( pFromImport ){` |
|  2353743 |  5477 | `		*pFromImport = 0;` |
|  1176869 |  5478 | `	}` |
|  2884983 |  5479 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  2884983 |  5480 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|      ! 0 |  5481 | `		return nOrigIdx;` |
|        - |  5482 | `	}` |
|  2884983 |  5483 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  2884983 |  5484 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|        - |  5485 | `	/* Skip if already qualified (contains backslash) */` |
|  2884983 |  5486 | `	hasNsSep = 0;` |
| 37236555 |  5487 | `	for( k = 0; k < nLit; k++ ){` |
| 34351585 |  5488 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 17175791 |  5489 | `	}` |
|  2884983 |  5490 | `	if( hasNsSep ){` |
|       10 |  5491 | `		return nOrigIdx;` |
|        - |  5492 | `	}` |
|        - |  5493 | `	/* Check use imports first (works even outside namespaces) */` |
|  2884975 |  5494 | `	SyBlobReset(&pGen->sWorker);` |
|  2884975 |  5495 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  2884975 |  5496 | `	if( pImport ){` |
|       41 |  5497 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       41 |  5498 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|       41 |  5499 | `		if( pFromImport ){` |
|       18 |  5500 | `			*pFromImport = 1;` |
|        8 |  5501 | `		}` |
|       23 |  5502 | `	}else{` |
|  2884939 |  5503 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  2884849 |  5504 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|        - |  5505 | `		}` |
|        - |  5506 | `		/* Prepend current namespace */` |
|       95 |  5507 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       95 |  5508 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|       95 |  5509 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|        - |  5510 | `	}` |
|        - |  5511 | `	/* Look up or create a new literal for the qualified name */` |
|      131 |  5512 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|      131 |  5513 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|       57 |  5514 | `		return nNewIdx; /* Already interned */` |
|        - |  5515 | `	}` |
|       79 |  5516 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|       79 |  5517 | `	if( pNew == 0 ){` |
|      ! 0 |  5518 | `		return nOrigIdx; /* OOM, fall back to original */` |
|        - |  5519 | `	}` |
|       79 |  5520 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|       79 |  5521 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|       79 |  5522 | `	return nNewIdx;` |
|  1442494 |  5523 | `}` |
|        - |  5524 | `/*` |
|        - |  5525 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|        - |  5526 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|        - |  5527 | ` */` |
|   187742 |  5528 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 |  5529 | `{` |
|        - |  5530 | `	SyHashEntry *pImport;` |
|        - |  5531 | `	/* Check use imports first */` |
|   187747 |  5532 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   187747 |  5533 | `	if( pImport ){` |
|       19 |  5534 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       19 |  5535 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|       19 |  5536 | `		return;` |
|        - |  5537 | `	}` |
|        - |  5538 | `	/* Prepend current namespace if active */` |
|   187731 |  5539 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        8 |  5540 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        8 |  5541 | `		SyBlobAppend(pOut,"\\",1);` |
|        3 |  5542 | `	}` |
|   187731 |  5543 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    93876 |  5544 | `}` |
|        - |  5545 | `/*` |
|        - |  5546 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|        - |  5547 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|        - |  5548 | ` * The caller must release pOut when done.` |
|        - |  5549 | ` */` |
|   262024 |  5550 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 |  5551 | `{` |
|   262029 |  5552 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     3947 |  5553 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     3947 |  5554 | `		SyBlobAppend(pOut,"\\",1);` |
|     1971 |  5555 | `	}` |
|   262029 |  5556 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   262029 |  5557 | `}` |
|        - |  5558 | `/*` |
|        - |  5559 | ` * Compile a namespace statement` |
|        - |  5560 | ` * According to the PHP language reference manual` |
|        - |  5561 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|        - |  5562 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|        - |  5563 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|        - |  5564 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|        - |  5565 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|        - |  5566 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|        - |  5567 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|        - |  5568 | ` *  programming world.` |
|        - |  5569 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|        - |  5570 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|        - |  5571 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|        - |  5572 | ` *  classes/functions/constants.` |
|        - |  5573 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|        - |  5574 | ` *  readability of source code.` |
|        - |  5575 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|        - |  5576 | ` *  Here is an example of namespace syntax in PHP:` |
|        - |  5577 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|        - |  5578 | ` *       class MyClass {}` |
|        - |  5579 | ` *       function myfunction() {}` |
|        - |  5580 | ` *       const MYCONST = 1;` |
|        - |  5581 | ` *       $a = new MyClass;` |
|        - |  5582 | ` *       $c = new \my\name\MyClass;` |
|        - |  5583 | ` *       $a = strlen('hi');` |
|        - |  5584 | ` *       $d = namespace\MYCONST;` |
|        - |  5585 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|        - |  5586 | ` *       echo constant($d);` |
|        - |  5587 | ` * NOTE` |
|        - |  5588 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - |  5589 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - |  5590 | ` */` |
|        - |  5591 | `/*` |
|        - |  5592 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|        - |  5593 | ` */` |
|       14 |  5594 | `static const char * TokenTypeName(sxu32 nType)` |
|        3 |  5595 | `{` |
|       17 |  5596 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       11 |  5597 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       11 |  5598 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       11 |  5599 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       11 |  5600 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       11 |  5601 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|        3 |  5602 | `	return "token";` |
|       10 |  5603 | `}` |
|     3990 |  5604 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|        5 |  5605 | `{` |
|        - |  5606 | `	sxu32 nLine;` |
|        - |  5607 | `	sxi32 rc;` |
|     3995 |  5608 | `	nLine = pGen->pIn->nLine;` |
|     3995 |  5609 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|        - |  5610 | `	/* Reset namespace and clear previous use imports */` |
|     3995 |  5611 | `	SyBlobReset(&pGen->sNamespace);` |
|     3995 |  5612 | `	SyHashRelease(&pGen->hUseImports);` |
|     3995 |  5613 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     3995 |  5614 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     3995 |  5615 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     3995 |  5616 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     3995 |  5617 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     3995 |  5618 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  5619 | `		/* Global namespace (bare "namespace;") */` |
|      ! 0 |  5620 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 |  5621 | `		return SXRET_OK;` |
|        - |  5622 | `	}` |
|     3995 |  5623 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        - |  5624 | `		/* namespace; — switch to global namespace */` |
|      ! 0 |  5625 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 |  5626 | `		return SXRET_OK;` |
|        - |  5627 | `	}` |
|     3995 |  5628 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|        - |  5629 | `		/* namespace { } — global namespace block */` |
|      ! 0 |  5630 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 |  5631 | `		return SXRET_OK;` |
|        - |  5632 | `	}` |
|        - |  5633 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     8027 |  5634 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     4037 |  5635 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        - |  5636 | `			/* Append backslash separator */` |
|       26 |  5637 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       26 |  5638 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|       11 |  5639 | `			}` |
|       15 |  5640 | `		}else{` |
|        - |  5641 | `			/* Append identifier */` |
|     4015 |  5642 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - |  5643 | `		}` |
|     4037 |  5644 | `		pGen->pIn++;` |
|        5 |  5645 | `	}` |
|        - |  5646 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|        - |  5647 | `	 * at the correct program counter, not just the last one compiled. */` |
|        - |  5648 | `	{` |
|     3995 |  5649 | `		char *zNsDup = 0;` |
|     3995 |  5650 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     5987 |  5651 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     3988 |  5652 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     1994 |  5653 | `		}` |
|     3995 |  5654 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|        - |  5655 | `	}` |
|     3995 |  5656 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        8 |  5657 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  5658 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|        4 |  5659 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        6 |  5660 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5661 | `			return SXERR_ABORT;` |
|        - |  5662 | `		}` |
|        2 |  5663 | `	}` |
|     3995 |  5664 | `	return SXRET_OK;` |
|     2000 |  5665 | `}` |
|        - |  5666 | `/*` |
|        - |  5667 | ` * Compile the 'use' statement` |
|        - |  5668 | ` * According to the PHP language reference manual` |
|        - |  5669 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|        - |  5670 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|        - |  5671 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|        - |  5672 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|        - |  5673 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|        - |  5674 | ` *  a function or constant is not supported.` |
|        - |  5675 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|        - |  5676 | ` * NOTE` |
|        - |  5677 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - |  5678 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - |  5679 | ` */` |
|       72 |  5680 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|        5 |  5681 | `{` |
|        - |  5682 | `	sxu32 nLine;` |
|        - |  5683 | `	sxi32 rc;` |
|        - |  5684 | `	SyBlob sPath;` |
|        - |  5685 | `	SyString sAlias;` |
|        - |  5686 | `	SyToken *pLast;` |
|        - |  5687 | `	char *zDup;` |
|        - |  5688 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|        - |  5689 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|        - |  5690 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|       77 |  5691 | `	nLine = pGen->pIn->nLine;` |
|       77 |  5692 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|        - |  5693 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|       77 |  5694 | `	iUseType = 0;` |
|       77 |  5695 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       30 |  5696 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|       30 |  5697 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|       16 |  5698 | `			iUseType = 1;` |
|       16 |  5699 | `			pGen->pIn++;` |
|       23 |  5700 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|       16 |  5701 | `			iUseType = 2;` |
|       16 |  5702 | `			pGen->pIn++;` |
|        7 |  5703 | `		}` |
|       14 |  5704 | `	}` |
|        - |  5705 | `	/* Select target hash tables based on import type */` |
|       77 |  5706 | `	switch( iUseType ){` |
|        7 |  5707 | `		case 1:` |
|       16 |  5708 | `			pGenHash = &pGen->hUseFuncImports;` |
|       16 |  5709 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|       16 |  5710 | `			break;` |
|        7 |  5711 | `		case 2:` |
|       16 |  5712 | `			pGenHash = &pGen->hUseConstImports;` |
|       16 |  5713 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|       16 |  5714 | `			break;` |
|       22 |  5715 | `		default:` |
|       49 |  5716 | `			pGenHash = &pGen->hUseImports;` |
|       49 |  5717 | `			pVmHash = &pGen->pVm->hUseImports;` |
|       44 |  5718 | `			break;` |
|        - |  5719 | `	}` |
|       77 |  5720 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|        - |  5721 | `	/* Process one or more use declarations separated by commas */` |
|       37 |  5722 | `	for(;;){` |
|       79 |  5723 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  5724 | `			break;` |
|        - |  5725 | `		}` |
|       79 |  5726 | `		SyBlobReset(&sPath);` |
|       79 |  5727 | `		pLast = 0;` |
|        - |  5728 | `		/* Collect the full namespace path */` |
|      269 |  5729 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|      195 |  5730 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|      135 |  5731 | `				pLast = pGen->pIn;` |
|      135 |  5732 | `				if( SyBlobLength(&sPath) > 0 ){` |
|       65 |  5733 | `					SyBlobAppend(&sPath,"\\",1);` |
|       30 |  5734 | `				}` |
|      135 |  5735 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       65 |  5736 | `			}` |
|      195 |  5737 | `			pGen->pIn++;` |
|        5 |  5738 | `		}` |
|       79 |  5739 | `		if( pLast == 0 ){` |
|        - |  5740 | `			/* Empty path */` |
|        6 |  5741 | `			break;` |
|        - |  5742 | `		}` |
|        - |  5743 | `		/* Default alias is the last component of the path */` |
|       75 |  5744 | `		sAlias = pLast->sData;` |
|        - |  5745 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|       70 |  5746 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       50 |  5747 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|       24 |  5748 | `			pGen->pIn++; /* Jump 'as' */` |
|       24 |  5749 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|       24 |  5750 | `				sAlias = pGen->pIn->sData;` |
|       24 |  5751 | `				pGen->pIn++;` |
|       10 |  5752 | `			}` |
|       10 |  5753 | `		}` |
|        - |  5754 | `		/* Check for duplicate import alias (per-type) */` |
|       75 |  5755 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|        8 |  5756 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  5757 | `				"Cannot use %.*s as %z because the name is already in use",` |
|        4 |  5758 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|        6 |  5759 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5760 | `				SyBlobRelease(&sPath);` |
|      ! 0 |  5761 | `				return SXERR_ABORT;` |
|        - |  5762 | `			}` |
|        2 |  5763 | `		}` |
|        - |  5764 | `		/* Register the import: alias -> FQN.` |
|        - |  5765 | `		 * Strings are allocated from the VM pool allocator and freed` |
|        - |  5766 | `		 * when the entire VM is released. SyHashRelease does not free` |
|        - |  5767 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      110 |  5768 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       70 |  5769 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|       75 |  5770 | `		if( zDup ){` |
|       75 |  5771 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|       75 |  5772 | `			if( pVmHash ){` |
|        - |  5773 | `				/* Class imports: populate VM table directly (class resolution` |
|        - |  5774 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|       47 |  5775 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       47 |  5776 | `				if( zAliasDup ){` |
|       47 |  5777 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|       21 |  5778 | `				}` |
|       21 |  5779 | `			}` |
|       75 |  5780 | `			if( iUseType == 2 ){` |
|        - |  5781 | `				/* Const imports: emit a runtime instruction so imports are` |
|        - |  5782 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|       16 |  5783 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       16 |  5784 | `				if( zAliasDup ){` |
|        - |  5785 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|        - |  5786 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|        - |  5787 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|       16 |  5788 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|       16 |  5789 | `					if( azPair ){` |
|       16 |  5790 | `						azPair[0] = zAliasDup;` |
|       16 |  5791 | `						azPair[1] = zDup;` |
|       16 |  5792 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|        7 |  5793 | `					}` |
|        7 |  5794 | `				}` |
|        7 |  5795 | `			}` |
|       35 |  5796 | `		}` |
|        - |  5797 | `		/* Check for comma (multiple use declarations) */` |
|       75 |  5798 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        3 |  5799 | `			pGen->pIn++;` |
|        2 |  5800 | `		}else{` |
|       39 |  5801 | `			break;` |
|        - |  5802 | `		}` |
|        1 |  5803 | `	}` |
|       77 |  5804 | `	SyBlobRelease(&sPath);` |
|       77 |  5805 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        4 |  5806 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|        2 |  5807 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        3 |  5808 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5809 | `			return SXERR_ABORT;` |
|        - |  5810 | `		}` |
|        1 |  5811 | `	}` |
|       77 |  5812 | `	return SXRET_OK;` |
|       41 |  5813 | `}` |
|        - |  5814 | `/*` |
|        - |  5815 | ` * Compile the stupid 'declare' language construct.` |
|        - |  5816 | ` *` |
|        - |  5817 | ` * According to the PHP language reference manual.` |
|        - |  5818 | ` *  The declare construct is used to set execution directives for a block of code.` |
|        - |  5819 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|        - |  5820 | ` *  declare (directive)` |
|        - |  5821 | ` *   statement` |
|        - |  5822 | ` * The directive section allows the behavior of the declare block to be set.` |
|        - |  5823 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|        - |  5824 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|        - |  5825 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|        - |  5826 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|        - |  5827 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|        - |  5828 | ` * <?php` |
|        - |  5829 | ` * // these are the same:` |
|        - |  5830 | ` * // you can use this:` |
|        - |  5831 | ` * declare(ticks=1) {` |
|        - |  5832 | ` *   // entire script here` |
|        - |  5833 | ` * }` |
|        - |  5834 | ` * // or you can use this:` |
|        - |  5835 | ` * declare(ticks=1);` |
|        - |  5836 | ` * // entire script here` |
|        - |  5837 | ` * ?>` |
|        - |  5838 | ` *` |
|        - |  5839 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|        - |  5840 | ` */` |
|        - |  5841 | `/*` |
|        - |  5842 | ` * Match a directive name against a known literal (case-insensitive).` |
|        - |  5843 | ` */` |
|       72 |  5844 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|        5 |  5845 | `{` |
|      109 |  5846 | `	return SyStringLength(pName) == nWant` |
|       72 |  5847 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|        5 |  5848 | `}` |
|        - |  5849 |  |
|       42 |  5850 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|        5 |  5851 | `{` |
|       47 |  5852 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       47 |  5853 | `	SyToken *pBodyEnd = 0;` |
|        - |  5854 | `	SyToken *pBodyStart;` |
|        - |  5855 | `	SyToken *pCursor;` |
|        - |  5856 | `	int bHasStrictTypes;` |
|        - |  5857 | `	int bBlockForm;` |
|        - |  5858 | `	int bPlacementOk;` |
|        - |  5859 | `	sxi32 rc;` |
|       47 |  5860 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       47 |  5861 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|        6 |  5862 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|        6 |  5863 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5864 | `			return SXERR_ABORT;` |
|        - |  5865 | `		}` |
|        6 |  5866 | `		goto Synchro;` |
|        - |  5867 | `	}` |
|       43 |  5868 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       43 |  5869 | `	pBodyStart = pGen->pIn;` |
|        - |  5870 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|       43 |  5871 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|       43 |  5872 | `	if( pBodyEnd >= pGen->pEnd ){` |
|      ! 0 |  5873 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|      ! 0 |  5874 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5875 | `			return SXERR_ABORT;` |
|        - |  5876 | `		}` |
|      ! 0 |  5877 | `		return SXRET_OK;` |
|        - |  5878 | `	}` |
|        - |  5879 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|        - |  5880 | `	 * now delimits the comma-separated directive list. */` |
|       43 |  5881 | `	pGen->pIn = &pBodyEnd[1];` |
|       43 |  5882 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      ! 0 |  5883 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|      ! 0 |  5884 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5885 | `			return SXERR_ABORT;` |
|        - |  5886 | `		}` |
|      ! 0 |  5887 | `	}` |
|       43 |  5888 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|       43 |  5889 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|       43 |  5890 | `	bHasStrictTypes = 0;` |
|        - |  5891 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|        - |  5892 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|        - |  5893 | `	 * directive appears anywhere in the list, before validating values. */` |
|       43 |  5894 | `	pCursor = pBodyStart;` |
|       55 |  5895 | `	while( pCursor < pBodyEnd ){` |
|       51 |  5896 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       43 |  5897 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|       39 |  5898 | `				bHasStrictTypes = 1;` |
|       39 |  5899 | `				break;` |
|        - |  5900 | `			}` |
|        2 |  5901 | `		}` |
|       14 |  5902 | `		pCursor++;` |
|        2 |  5903 | `	}` |
|       43 |  5904 | `	if( bHasStrictTypes && bBlockForm ){` |
|        3 |  5905 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5906 | `			"strict_types declaration must not use block mode");` |
|        3 |  5907 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 |  5908 | `		return SXRET_OK;` |
|        - |  5909 | `	}` |
|       41 |  5910 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|        6 |  5911 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5912 | `			"strict_types declaration must be the very first statement in the script");` |
|        6 |  5913 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        6 |  5914 | `		return SXRET_OK;` |
|        - |  5915 | `	}` |
|        - |  5916 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|       37 |  5917 | `	pCursor = pBodyStart;` |
|       69 |  5918 | `	while( pCursor < pBodyEnd ){` |
|        - |  5919 | `		SyToken *pNameTok;` |
|        - |  5920 | `		SyToken *pEqTok;` |
|        - |  5921 | `		SyToken *pValTok;` |
|        - |  5922 | `		SyString *pDirName;` |
|        - |  5923 | `		int bIsStrict;` |
|        - |  5924 | `		int iStrictValue;` |
|       39 |  5925 | `		pNameTok = pCursor;` |
|       39 |  5926 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  5927 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5928 | `				"declare: Expecting a directive name");` |
|      ! 0 |  5929 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5930 | `			return SXRET_OK;` |
|        - |  5931 | `		}` |
|       39 |  5932 | `		pEqTok = pNameTok + 1;` |
|       39 |  5933 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|      ! 0 |  5934 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5935 | `				"declare: Expecting '=' after directive name");` |
|      ! 0 |  5936 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5937 | `			return SXRET_OK;` |
|        - |  5938 | `		}` |
|       39 |  5939 | `		pValTok = pEqTok + 1;` |
|       39 |  5940 | `		if( pValTok >= pBodyEnd ){` |
|      ! 0 |  5941 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5942 | `				"declare: Expecting value after '='");` |
|      ! 0 |  5943 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5944 | `			return SXRET_OK;` |
|        - |  5945 | `		}` |
|       39 |  5946 | `		pDirName = &pNameTok->sData;` |
|       39 |  5947 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|       39 |  5948 | `		if( bIsStrict ){` |
|        - |  5949 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|        - |  5950 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|       35 |  5951 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      ! 0 |  5952 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5953 | `					"declare(strict_types) value must be a literal");` |
|      ! 0 |  5954 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5955 | `				return SXRET_OK;` |
|        - |  5956 | `			}` |
|       35 |  5957 | `			iStrictValue = -1;` |
|       35 |  5958 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|       35 |  5959 | `				const char *zv = SyStringData(&pValTok->sData);` |
|       35 |  5960 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|       35 |  5961 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|       33 |  5962 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|       15 |  5963 | `			}` |
|       35 |  5964 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|        3 |  5965 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5966 | `					"strict_types declaration must have 0 or 1 as its value");` |
|        3 |  5967 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 |  5968 | `				return SXRET_OK;` |
|        - |  5969 | `			}` |
|       32 |  5970 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|       18 |  5971 | `		}else{` |
|        - |  5972 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|        - |  5973 | `			 * preserve the legacy notice so callers relying on the old` |
|        - |  5974 | `			 * behavior don't regress. */` |
|        8 |  5975 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|        - |  5976 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|        2 |  5977 | `				ph7_lib_version()` |
|        - |  5978 | `				);` |
|        - |  5979 | `		}` |
|       36 |  5980 | `		pCursor = pValTok + 1;` |
|        - |  5981 | `		/* Consume separating comma (or end). */` |
|       36 |  5982 | `		if( pCursor < pBodyEnd ){` |
|        3 |  5983 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 |  5984 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5985 | `					"declare: Expecting ',' or ')' after directive value");` |
|      ! 0 |  5986 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5987 | `				return SXRET_OK;` |
|        - |  5988 | `			}` |
|        3 |  5989 | `			pCursor++;` |
|        1 |  5990 | `		}` |
|        4 |  5991 | `	}` |
|        - |  5992 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|        - |  5993 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|        - |  5994 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|       34 |  5995 | `	return SXRET_OK;` |
|        2 |  5996 | `Synchro:` |
|        - |  5997 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|       16 |  5998 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       12 |  5999 | `		pGen->pIn++;` |
|        2 |  6000 | `	}` |
|        6 |  6001 | `	return SXRET_OK;` |
|       26 |  6002 | `}` |
|        - |  6003 | `/*` |
|        - |  6004 | ` * Process default argument values. That is,a function may define C++-style default value` |
|        - |  6005 | ` * as follows:` |
|        - |  6006 | ` * function makecoffee($type = "cappuccino")` |
|        - |  6007 | ` * {` |
|        - |  6008 | ` *   return "Making a cup of $type.\n";` |
|        - |  6009 | ` * }` |
|        - |  6010 | ` * Symisc eXtension.` |
|        - |  6011 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|        - |  6012 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|        - |  6013 | ` *      Example: Work only with PH7,generate error under zend` |
|        - |  6014 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|        - |  6015 | ` *      {` |
|        - |  6016 | ` *       var_dump($a);` |
|        - |  6017 | ` *      }` |
|        - |  6018 | ` *     //call test without args` |
|        - |  6019 | ` *      test();` |
|        - |  6020 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|        - |  6021 | ` *      Example:` |
|        - |  6022 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|        - |  6023 | ` * 3 -) Function overloading!!` |
|        - |  6024 | ` *      Example:` |
|        - |  6025 | ` *      function foo($a) {` |
|        - |  6026 | ` *   	  return $a.PHP_EOL;` |
|        - |  6027 | ` *	    }` |
|        - |  6028 | ` *	    function foo($a, $b) {` |
|        - |  6029 | ` *   	  return $a + $b;` |
|        - |  6030 | ` *	    }` |
|        - |  6031 | ` *	    echo foo(5); // Prints "5"` |
|        - |  6032 | ` *	    echo foo(5, 2); // Prints "7"` |
|        - |  6033 | ` *      // Same arg` |
|        - |  6034 | ` *	   function foo(string $a)` |
|        - |  6035 | ` *	   {` |
|        - |  6036 | ` *	     echo "a is a string\n";` |
|        - |  6037 | ` *	     var_dump($a);` |
|        - |  6038 | ` *	   }` |
|        - |  6039 | ` *	  function foo(int $a)` |
|        - |  6040 | ` *	  {` |
|        - |  6041 | ` *	    echo "a is integer\n";` |
|        - |  6042 | ` *	    var_dump($a);` |
|        - |  6043 | ` *	  }` |
|        - |  6044 | ` *	  function foo(array $a)` |
|        - |  6045 | ` *	  {` |
|        - |  6046 | ` * 	    echo "a is an array\n";` |
|        - |  6047 | ` * 	    var_dump($a);` |
|        - |  6048 | ` *	  }` |
|        - |  6049 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|        - |  6050 | ` *	  foo(52); // a is integer [second foo]` |
|        - |  6051 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|        - |  6052 | ` * Please refer to the official documentation for more information on the powerful extension` |
|        - |  6053 | ` * introduced by the PH7 engine.` |
|        - |  6054 | ` */` |
|   240962 |  6055 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|        5 |  6056 | `{` |
|        - |  6057 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |  6058 | `	SySet *pInstrContainer;` |
|        - |  6059 | `	sxi32 rc;` |
|        - |  6060 | `	/* Swap token stream */` |
|   240967 |  6061 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   240967 |  6062 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   240967 |  6063 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|        - |  6064 | `	/* Compile the expression holding the argument value */` |
|   240967 |  6065 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  6066 | `	/* Emit the done instruction */` |
|   240967 |  6067 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   240967 |  6068 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   240967 |  6069 | `	RE_SWAP_DELIMITER(pGen);` |
|   240967 |  6070 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  6071 | `		return SXERR_ABORT;` |
|        - |  6072 | `	}` |
|   240967 |  6073 | `	return SXRET_OK;` |
|   120486 |  6074 | `}` |
|        - |  6075 | `/*` |
|        - |  6076 | ` * Collect function arguments one after one.` |
|        - |  6077 | ` * According to the PHP language reference manual.` |
|        - |  6078 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|        - |  6079 | ` * list of expressions.` |
|        - |  6080 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|        - |  6081 | ` * and default argument values. Variable-length argument lists are also supported,` |
|        - |  6082 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|        - |  6083 | ` * for more information.` |
|        - |  6084 | ` * Example #1 Passing arrays to functions` |
|        - |  6085 | ` * <?php` |
|        - |  6086 | ` * function takes_array($input)` |
|        - |  6087 | ` * {` |
|        - |  6088 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|        - |  6089 | ` * }` |
|        - |  6090 | ` * ?>` |
|        - |  6091 | ` * Making arguments be passed by reference` |
|        - |  6092 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|        - |  6093 | ` * within the function is changed, it does not get changed outside of the function).` |
|        - |  6094 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|        - |  6095 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|        - |  6096 | ` * to the argument name in the function definition:` |
|        - |  6097 | ` * Example #2 Passing function parameters by reference` |
|        - |  6098 | ` * <?php` |
|        - |  6099 | ` * function add_some_extra(&$string)` |
|        - |  6100 | ` * {` |
|        - |  6101 | ` *   $string .= 'and something extra.';` |
|        - |  6102 | ` * }` |
|        - |  6103 | ` * $str = 'This is a string, ';` |
|        - |  6104 | ` * add_some_extra($str);` |
|        - |  6105 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|        - |  6106 | ` * ?>` |
|        - |  6107 | ` *` |
|        - |  6108 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|        - |  6109 | ` * complex agrument values.Please refer to the official documentation for more information` |
|        - |  6110 | ` * on these extension.` |
|        - |  6111 | ` */` |
|   491222 |  6112 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|        5 |  6113 | `{` |
|        - |  6114 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|        - |  6115 | `	SyToken *pIn;  /* Token stream */` |
|        - |  6116 | `	SyBlob sSig;         /* Function signature */` |
|        - |  6117 | `	char *zDup;          /* Copy of argument name */` |
|        - |  6118 | `	sxi32 rc;` |
|        - |  6119 |  |
|   491227 |  6120 | `	pIn = pGen->pIn;` |
|   491227 |  6121 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|        - |  6122 | `	/* Process arguments one after one */` |
|   604316 |  6123 | `	for(;;){` |
|  1208637 |  6124 | `		if( pIn >= pEnd ){` |
|        - |  6125 | `			/* No more arguments to process */` |
|   491211 |  6126 | `			break;` |
|        - |  6127 | `		}` |
|   717431 |  6128 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   717431 |  6129 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   717431 |  6130 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   717431 |  6131 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|   717431 |  6132 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|        - |  6133 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|        - |  6134 | `		 * first token inside the main token stream */` |
|   717431 |  6135 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  6136 | `			return SXERR_ABORT;` |
|        - |  6137 | `		}` |
|        - |  6138 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|        - |  6139 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|        - |  6140 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|        - |  6141 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|        - |  6142 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|        - |  6143 | `		{` |
|   717431 |  6144 | `			int bReadonly = 0, bVisSeen = 0;` |
|   717431 |  6145 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|   717431 |  6146 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|        3 |  6147 | `				bReadonly = 1;` |
|        3 |  6148 | `				pIn++;` |
|        1 |  6149 | `			}` |
|   717431 |  6150 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|    81943 |  6151 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|    81943 |  6152 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|       87 |  6153 | `					bVisSeen = 1;` |
|       87 |  6154 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|      117 |  6155 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|       38 |  6156 | `						: PH7_CLASS_PROT_PUBLIC;` |
|       87 |  6157 | `					pIn++;` |
|       87 |  6158 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       18 |  6159 | `						bReadonly = 1;` |
|       18 |  6160 | `						pIn++;` |
|        7 |  6161 | `					}` |
|       41 |  6162 | `				}` |
|    40969 |  6163 | `			}` |
|   717431 |  6164 | `			if( bVisSeen \|\| bReadonly ){` |
|       89 |  6165 | `				if( !bCtorCtx ){` |
|        6 |  6166 | `					if( bAbstractCtx ){` |
|        3 |  6167 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|        - |  6168 | `							"Cannot declare promoted property in an abstract constructor");` |
|        2 |  6169 | `					}else{` |
|        3 |  6170 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|        - |  6171 | `							"Cannot declare promoted property outside a constructor");` |
|        - |  6172 | `					}` |
|        6 |  6173 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  6174 | `						return SXERR_ABORT;` |
|        - |  6175 | `					}` |
|        6 |  6176 | `					return SXERR_SYNTAX;` |
|        - |  6177 | `				}` |
|       85 |  6178 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|       85 |  6179 | `				sArg.iPromoteVis = iVis;` |
|       85 |  6180 | `				if( bReadonly ){` |
|       20 |  6181 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|        8 |  6182 | `				}` |
|       40 |  6183 | `			}` |
|        - |  6184 | `		}` |
|        - |  6185 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|   717422 |  6186 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   419215 |  6187 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   119055 |  6188 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|    97595 |  6189 | `			sxu32 nLineLocal = pIn->nLine;` |
|    97595 |  6190 | `			sxi32 iTFlags = 0;` |
|    97595 |  6191 | `			pGen->pIn = pIn;` |
|    97595 |  6192 | `			rc = GenStateParseUnionTypeDecl(` |
|    48795 |  6193 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|    48795 |  6194 | `				&iTFlags, &sArg.sTypeName,` |
|        - |  6195 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|        - |  6196 | `				/* bAllowVoid */ 0,` |
|    48795 |  6197 | `						nLineLocal);` |
|    97595 |  6198 | `			pIn = pGen->pIn;` |
|    97595 |  6199 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  6200 | `				return SXERR_ABORT;` |
|    97595 |  6201 | `			}else if( rc == SXERR_CORRUPT ){` |
|        - |  6202 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|        3 |  6203 | `				return SXERR_SYNTAX;` |
|    97593 |  6204 | `			}else if( rc == SXERR_SYNTAX ){` |
|       12 |  6205 | `				if( pIn < pEnd ){` |
|       16 |  6206 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|        - |  6207 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|        4 |  6208 | `						&pIn->sData);` |
|        8 |  6209 | `				}else{` |
|      ! 0 |  6210 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|        - |  6211 | `						"syntax error, unexpected end of file");` |
|        - |  6212 | `				}` |
|       12 |  6213 | `				return SXERR_SYNTAX;` |
|        - |  6214 | `			}` |
|    97585 |  6215 | `			sArg.iFlags \|= iTFlags;` |
|    48790 |  6216 | `		}` |
|   717417 |  6217 | `		if( pIn >= pEnd ){` |
|      ! 0 |  6218 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|      ! 0 |  6219 | `			return rc;` |
|        - |  6220 | `		}` |
|   717417 |  6221 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|        - |  6222 | `			/* Pass by reference,record that */` |
|     3929 |  6223 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|     3929 |  6224 | `			pIn++;` |
|     1962 |  6225 | `		}` |
|   717417 |  6226 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|        - |  6227 | `			/* Variadic parameter: ...$args */` |
|    19529 |  6228 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|    19529 |  6229 | `			pIn++;` |
|     9762 |  6230 | `		}` |
|   717417 |  6231 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  6232 | `			/* Invalid argument */` |
|      ! 0 |  6233 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|      ! 0 |  6234 | `			return rc;` |
|        - |  6235 | `		}` |
|   717417 |  6236 | `		pIn++; /* Jump the dollar sign */` |
|        - |  6237 | `		/* Copy argument name */` |
|   717417 |  6238 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   717417 |  6239 | `		if( zDup == 0 ){` |
|      ! 0 |  6240 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  6241 | `			return SXERR_ABORT;` |
|        - |  6242 | `		}` |
|   717417 |  6243 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   717417 |  6244 | `		pIn++;` |
|   717417 |  6245 | `		if( pIn < pEnd ){` |
|   373913 |  6246 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|        - |  6247 | `				SyToken *pDefend;` |
|   240969 |  6248 | `				sxi32 iNest = 0;` |
|   240969 |  6249 | `				pIn++; /* Jump the equal sign */` |
|   240969 |  6250 | `				pDefend = pIn;` |
|        - |  6251 | `				/* Process the default value associated with this argument */` |
|   513031 |  6252 | `				while( pDefend < pEnd ){` |
|   365333 |  6253 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|    93271 |  6254 | `						break;` |
|        - |  6255 | `					}` |
|   272067 |  6256 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|        - |  6257 | `						/* Increment nesting level */` |
|    15549 |  6258 | `						iNest++;` |
|   264295 |  6259 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|        - |  6260 | `						/* Decrement nesting level */` |
|    15549 |  6261 | `						iNest--;` |
|     7772 |  6262 | `					}` |
|   272067 |  6263 | `					pDefend++;` |
|        5 |  6264 | `				}` |
|   240969 |  6265 | `				if( pIn >= pDefend ){` |
|        3 |  6266 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|        3 |  6267 | `					return rc;` |
|        - |  6268 | `				}` |
|        - |  6269 | `				/* Process default value */` |
|   240967 |  6270 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   240967 |  6271 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  6272 | `					return rc;` |
|        - |  6273 | `				}` |
|        - |  6274 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|        - |  6275 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|        - |  6276 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|        - |  6277 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|        - |  6278 | `				 * arg-type check lets null through. */` |
|   240962 |  6279 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|   145748 |  6280 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|   145745 |  6281 | `					&& &pIn[1] == pDefend` |
|    46643 |  6282 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|    34976 |  6283 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|    21373 |  6284 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|    15547 |  6285 | `					sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|     7771 |  6286 | `				}` |
|        - |  6287 | `				/* Point beyond the default value */` |
|   240967 |  6288 | `				pIn = pDefend;` |
|   120481 |  6289 | `			}` |
|   373911 |  6290 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 |  6291 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|      ! 0 |  6292 | `				return rc;` |
|        - |  6293 | `			}` |
|   373911 |  6294 | `			pIn++; /* Jump the trailing comma */` |
|   186953 |  6295 | `		}` |
|        - |  6296 | `		/* Append argument signature */` |
|   717415 |  6297 | `		if( sArg.nType > 0 ){` |
|    97523 |  6298 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|        - |  6299 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    15621 |  6300 | `				int marker = 'o';` |
|    15621 |  6301 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    15621 |  6302 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|     7813 |  6303 | `			}else{` |
|        - |  6304 | `				int c;` |
|    81907 |  6305 | `				c = 'n'; /* cc warning */` |
|        - |  6306 | `				/* Type leading character */` |
|    81907 |  6307 | `				switch(sArg.nType){` |
|     5832 |  6308 | `				case MEMOBJ_HASHMAP:` |
|        - |  6309 | `					/* Hashmap aka 'array' */` |
|    11669 |  6310 | `					c = 'h';` |
|    11669 |  6311 | `					break;` |
|     9822 |  6312 | `				case MEMOBJ_INT:` |
|        - |  6313 | `					/* Integer */` |
|    19649 |  6314 | `					c = 'i';` |
|    19649 |  6315 | `					break;` |
|        2 |  6316 | `				case MEMOBJ_BOOL:` |
|        - |  6317 | `					/* Bool */` |
|        5 |  6318 | `					c = 'b';` |
|        5 |  6319 | `					break;` |
|        5 |  6320 | `				case MEMOBJ_REAL:` |
|        - |  6321 | `					/* Float */` |
|       12 |  6322 | `					c = 'f';` |
|       12 |  6323 | `					break;` |
|    25282 |  6324 | `				case MEMOBJ_STRING:` |
|        - |  6325 | `					/* String */` |
|    50569 |  6326 | `					c = 's';` |
|    50569 |  6327 | `					break;` |
|        7 |  6328 | `				case MEMOBJ_OBJ:` |
|        - |  6329 | `					/* Object */` |
|       16 |  6330 | `					c = 'o';` |
|       14 |  6331 | `					break;` |
|        1 |  6332 | `				default:` |
|        2 |  6333 | `					break;` |
|        - |  6334 | `				}` |
|    81907 |  6335 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|        - |  6336 | `			}` |
|    48764 |  6337 | `		}else{` |
|        - |  6338 | `			/* No type is associated with this parameter which mean` |
|        - |  6339 | `			 * that this function is not condidate for overloading.` |
|        - |  6340 | `			 */` |
|   619897 |  6341 | `			SyBlobRelease(&sSig);` |
|        - |  6342 | `		}` |
|        - |  6343 | `		/* Save in the argument set */` |
|   717415 |  6344 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|        5 |  6345 | `	}` |
|   491211 |  6346 | `	if( SyBlobLength(&sSig) > 0 ){` |
|        - |  6347 | `		/* Save function signature */` |
|    66379 |  6348 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|    33187 |  6349 | `	}` |
|   491211 |  6350 | `	return SXRET_OK;` |
|   245616 |  6351 | `}` |
|        - |  6352 | `/*` |
|        - |  6353 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|        - |  6354 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|        - |  6355 | ` * the enclosing function. Returns the token just past the nested construct.` |
|        - |  6356 | ` */` |
|    34998 |  6357 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|        5 |  6358 | `{` |
|    35003 |  6359 | `	sxi32 iParen = 0;` |
|    35003 |  6360 | `	pIn++; /* past 'function'/'fn' */` |
|        - |  6361 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|        - |  6362 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|        - |  6363 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|   155593 |  6364 | `	while( pIn < pEnd ){` |
|   155593 |  6365 | `		sxu32 t = pIn->nType;` |
|   155593 |  6366 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|   151655 |  6367 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|   104993 |  6368 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|    85531 |  6369 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|   120595 |  6370 | `		pIn++;` |
|        5 |  6371 | `	}` |
|    19467 |  6372 | `	if( pIn >= pEnd ){ return pIn; }` |
|        - |  6373 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|        - |  6374 | `	{` |
|    19467 |  6375 | `		sxi32 d = 0;` |
|   773341 |  6376 | `		while( pIn < pEnd ){` |
|   773341 |  6377 | `			sxu32 t = pIn->nType;` |
|   773341 |  6378 | `			if( t & PH7_TK_OCB ){ d++; }` |
|   742223 |  6379 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|   753879 |  6380 | `			pIn++;` |
|        5 |  6381 | `		}` |
|        - |  6382 | `	}` |
|    19467 |  6383 | `	return pIn;` |
|    17504 |  6384 | `}` |
|        - |  6385 | `/*` |
|        - |  6386 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|        - |  6387 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|        - |  6388 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|        - |  6389 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|        - |  6390 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|        - |  6391 | ` * detached-mini-program path untouched.` |
|        - |  6392 | ` */` |
|        - |  6393 | `/*` |
|        - |  6394 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|        - |  6395 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|        - |  6396 | ` * mixed, object.` |
|        - |  6397 | ` */` |
|       28 |  6398 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|        3 |  6399 | `{` |
|        - |  6400 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|        - |  6401 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|        - |  6402 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|        - |  6403 | `	};` |
|        - |  6404 | `	sxu32 i;` |
|       31 |  6405 | `	if( nName > 0 && zName[0] == '\\' ){` |
|      ! 0 |  6406 | `		zName++;` |
|      ! 0 |  6407 | `		nName--;` |
|      ! 0 |  6408 | `	}` |
|       63 |  6409 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|       59 |  6410 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|       27 |  6411 | `			return 1;` |
|        - |  6412 | `		}` |
|       17 |  6413 | `	}` |
|        5 |  6414 | `	return 0;` |
|       17 |  6415 | `}` |
|        - |  6416 | `/*` |
|        - |  6417 | ` * One atom of a generator's declared return type: is it a supertype of` |
|        - |  6418 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|        - |  6419 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|        - |  6420 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|        - |  6421 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|        - |  6422 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|        - |  6423 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|        - |  6424 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|        - |  6425 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|        - |  6426 | ` * both rather than fatal on valid code (a recorded divergence).` |
|        - |  6427 | ` */` |
|       26 |  6428 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|        4 |  6429 | `{` |
|       30 |  6430 | `	if( nType == MEMOBJ_OBJ ){` |
|      ! 0 |  6431 | ``		return 1; /* bare `object` */`` |
|        - |  6432 | `	}` |
|       30 |  6433 | `	if( nType != SXU32_HIGH ){` |
|        3 |  6434 | `		return 0; /* scalar/array/void/never/null/... */` |
|        - |  6435 | `	}` |
|       27 |  6436 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|       23 |  6437 | `		return 1;` |
|        - |  6438 | `	}` |
|        - |  6439 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|        - |  6440 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|        - |  6441 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|        - |  6442 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|        - |  6443 | `	{` |
|        - |  6444 | `		SyBlob sFQN;` |
|        - |  6445 | `		int bOk;` |
|        5 |  6446 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        5 |  6447 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|        5 |  6448 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|        5 |  6449 | `		SyBlobRelease(&sFQN);` |
|        5 |  6450 | `		return bOk;` |
|        - |  6451 | `	}` |
|       17 |  6452 | `}` |
|        - |  6453 | `/*` |
|        - |  6454 | ` * php 8: a generator function may only declare a return type that is a` |
|        - |  6455 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|        - |  6456 | ` * group qualifies only if every member does. Anything else is php's exact` |
|        - |  6457 | ` * compile-time fatal "Generator return type must be a supertype of` |
|        - |  6458 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|        - |  6459 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|        - |  6460 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|        - |  6461 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|        - |  6462 | ` */` |
|      264 |  6463 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|        5 |  6464 | `{` |
|      269 |  6465 | `	int bOk = 0;` |
|        - |  6466 | `	sxu32 nLine;` |
|        - |  6467 | `	sxi32 rc;` |
|      269 |  6468 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|      243 |  6469 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|        - |  6470 | `	}` |
|       30 |  6471 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|      ! 0 |  6472 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  6473 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|        - |  6474 | `		sxu32 i,j;` |
|      ! 0 |  6475 | `		for( i = 0; i < n && !bOk; i++ ){` |
|        - |  6476 | `			int bGroupOk;` |
|      ! 0 |  6477 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|      ! 0 |  6478 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|        - |  6479 | `			}` |
|      ! 0 |  6480 | `			bGroupOk = 1;` |
|      ! 0 |  6481 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|      ! 0 |  6482 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|      ! 0 |  6483 | `					bGroupOk = 0;` |
|      ! 0 |  6484 | `					break;` |
|        - |  6485 | `				}` |
|      ! 0 |  6486 | `			}` |
|      ! 0 |  6487 | `			bOk = bGroupOk;` |
|      ! 0 |  6488 | `		}` |
|      ! 0 |  6489 | `	}else{` |
|       30 |  6490 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|        - |  6491 | `	}` |
|       30 |  6492 | `	if( bOk ){` |
|       27 |  6493 | `		return SXRET_OK;` |
|        - |  6494 | `	}` |
|        - |  6495 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|        - |  6496 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|        - |  6497 | `	 * token of this stream — its line is the function's closing brace. php` |
|        - |  6498 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|        - |  6499 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|        3 |  6500 | `	nLine = pGen->pIn[-1].nLine;` |
|        - |  6501 | `	{` |
|        3 |  6502 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|        3 |  6503 | `		if( sGiven.nByte < 1 ){` |
|      ! 0 |  6504 | `			sGiven = pFunc->sReturnClass;` |
|      ! 0 |  6505 | `		}` |
|        3 |  6506 | `		if( sGiven.nByte < 1 ){` |
|        - |  6507 | ``			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the`` |
|        - |  6508 | `			 * rendered type text, so sReturnTypeName arrives empty for them —` |
|        - |  6509 | `			 * name them here (the root fix belongs to that renderer, §3.7). */` |
|      ! 0 |  6510 | `			const char *zScalar =` |
|      ! 0 |  6511 | `				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :` |
|      ! 0 |  6512 | `				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";` |
|      ! 0 |  6513 | `			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));` |
|      ! 0 |  6514 | `		}` |
|        3 |  6515 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  6516 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|        - |  6517 | `	}` |
|        3 |  6518 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|      137 |  6519 | `}` |
|  1405470 |  6520 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|        5 |  6521 | `{` |
|  1405475 |  6522 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|  1405475 |  6523 | `	SyToken *pEnd = pGen->pEnd;` |
|  1405475 |  6524 | `	sxi32 iDepth = 0;` |
|  1405475 |  6525 | `	int bStarted = 0;` |
| 63312169 |  6526 | `	while( pIn < pEnd ){` |
| 63312169 |  6527 | `		sxu32 t = pIn->nType;` |
| 63312169 |  6528 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 60340491 |  6529 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 57369191 |  6530 | `		if( t & PH7_TK_KEYWORD ){` |
|  4650381 |  6531 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|  4650381 |  6532 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|  4650117 |  6533 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|        - |  6534 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|  2307557 |  6535 | `		}` |
| 57333929 |  6536 | `		pIn++;` |
|        5 |  6537 | `	}` |
|  1405211 |  6538 | `	return FALSE;` |
|   702740 |  6539 | `}` |
|        - |  6540 | `/*` |
|        - |  6541 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|        - |  6542 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|        - |  6543 | ` * and this routine takes care of generating the appropriate error message.` |
|        - |  6544 | ` */` |
|  1405470 |  6545 | `static sxi32 GenStateCompileFuncBody(` |
|        - |  6546 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - |  6547 | `	ph7_vm_func *pFunc    /* Function state */` |
|        - |  6548 | `	)` |
|        5 |  6549 | `{` |
|        - |  6550 | `	SySet *pInstrContainer; /* Instruction container */` |
|        - |  6551 | `	GenBlock *pBlock;` |
|        - |  6552 | `	sxu32 nGotoOfft;` |
|        - |  6553 | `	sxi32 rc;` |
|        - |  6554 | `	/* Attach the new function */` |
|  1405475 |  6555 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  1405475 |  6556 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  6557 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|        - |  6558 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  6559 | `		return SXERR_ABORT;` |
|        - |  6560 | `	}` |
|  1405475 |  6561 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|        - |  6562 | `	/* Swap bytecode containers */` |
|  1405475 |  6563 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  1405475 |  6564 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|        - |  6565 | `	/* Emit constructor property promotion prologue:` |
|        - |  6566 | `	 *   $this->NAME = $NAME;` |
|        - |  6567 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|        - |  6568 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|        - |  6569 | `	{` |
|  1405475 |  6570 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|        - |  6571 | `		sxu32 i;` |
|  2091657 |  6572 | `		for( i = 0; i < nArg; i++ ){` |
|   686187 |  6573 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|        - |  6574 | `			char *zSrc;` |
|        - |  6575 | `			sxu32 nSrc,nName;` |
|        - |  6576 | `			SySet sToken;` |
|        - |  6577 | `			SyToken *pTmpIn,*pTmpEnd;` |
|        - |  6578 | `			sxi32 rcPromote;` |
|   686187 |  6579 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   686117 |  6580 | `				continue;` |
|        - |  6581 | `			}` |
|        - |  6582 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|        - |  6583 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|        - |  6584 | `			 * copied), so it must outlive the function — never free it. The` |
|        - |  6585 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|        - |  6586 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|       75 |  6587 | `			nName = SyStringLength(&pArg->sName);` |
|       75 |  6588 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|       75 |  6589 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|       75 |  6590 | `			if( zSrc == 0 ){` |
|      ! 0 |  6591 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  6592 | `				GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  6593 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  6594 | `				return SXERR_ABORT;` |
|        - |  6595 | `			}` |
|        - |  6596 | `			{` |
|       75 |  6597 | `				char *z = zSrc;` |
|       75 |  6598 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|       75 |  6599 | `				z += sizeof("$this->")-1;` |
|       75 |  6600 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|       75 |  6601 | `				z += nName;` |
|       75 |  6602 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|       75 |  6603 | `				z += sizeof(" = $")-1;` |
|       75 |  6604 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|       75 |  6605 | `				z += nName;` |
|       75 |  6606 | `				*z = 0;` |
|        - |  6607 | `			}` |
|       75 |  6608 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       75 |  6609 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);` |
|       75 |  6610 | `			pTmpIn = pGen->pIn;` |
|       75 |  6611 | `			pTmpEnd = pGen->pEnd;` |
|       75 |  6612 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|       75 |  6613 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       75 |  6614 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|       75 |  6615 | `			pGen->pIn = pTmpIn;` |
|       75 |  6616 | `			pGen->pEnd = pTmpEnd;` |
|       75 |  6617 | `			SySetRelease(&sToken);` |
|       75 |  6618 | `			if( rcPromote == SXERR_ABORT ){` |
|      ! 0 |  6619 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  6620 | `				GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  6621 | `				return SXERR_ABORT;` |
|        - |  6622 | `			}` |
|        - |  6623 | `			/* Discard the assignment result — this is a statement expression. */` |
|       75 |  6624 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       40 |  6625 | `		}` |
|        - |  6626 | `	}` |
|        - |  6627 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|        - |  6628 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|        - |  6629 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|        - |  6630 | `	 * generator — and vice versa — is classified independently. */` |
|        - |  6631 | `	{` |
|  1405475 |  6632 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|  1405475 |  6633 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|        - |  6634 | `		/* Compile the body */` |
|  1405475 |  6635 | `		PH7_CompileBlock(&(*pGen),0);` |
|  1405475 |  6636 | `		pGen->bInGenerator = bSavedGen;` |
|        - |  6637 | `	}` |
|        - |  6638 | `	/* Fix exception jumps now the destination is resolved */` |
|  1405475 |  6639 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        - |  6640 | `	/* Emit the final return if not yet done */` |
|  1405475 |  6641 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - |  6642 | `	/* Fix gotos jumps now the destination is resolved */` |
|  1405475 |  6643 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|      ! 0 |  6644 | `		rc = SXERR_ABORT;` |
|      ! 0 |  6645 | `	}` |
|  1405475 |  6646 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|        - |  6647 | `	/* Restore the default container */` |
|  1405475 |  6648 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - |  6649 | `	/* Leave function block */` |
|  1405475 |  6650 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  1405475 |  6651 | `	if( rc == SXERR_ABORT ){` |
|        - |  6652 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  6653 | `		return SXERR_ABORT;` |
|        - |  6654 | `	}` |
|        - |  6655 | `	/* Scan for yield opcodes to detect generator functions */` |
|        - |  6656 | `	{` |
|  1405475 |  6657 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|        - |  6658 | `		sxu32 i;` |
| 38464813 |  6659 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 37059607 |  6660 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|      269 |  6661 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      269 |  6662 | `				break;` |
|        - |  6663 | `			}` |
| 18529674 |  6664 | `		}` |
|        - |  6665 | `	}` |
|  1405475 |  6666 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  6667 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|      269 |  6668 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|      ! 0 |  6669 | `			return SXERR_ABORT;` |
|        - |  6670 | `		}` |
|      132 |  6671 | `	}` |
|        - |  6672 | `	/* All done, function body compiled */` |
|  1405475 |  6673 | `	return SXRET_OK;` |
|   702740 |  6674 | `}` |
|        - |  6675 | `/*` |
|        - |  6676 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|        - |  6677 | ` * According to the PHP language reference manual.` |
|        - |  6678 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|        - |  6679 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|        - |  6680 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|        - |  6681 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|        - |  6682 | ` *  Functions need not be defined before they are referenced.` |
|        - |  6683 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|        - |  6684 | ` *  a function even if they were defined inside and vice versa.` |
|        - |  6685 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|        - |  6686 | ` *  calls with over 32-64 recursion levels.` |
|        - |  6687 | ` *` |
|        - |  6688 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|        - |  6689 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|        - |  6690 | ` * on these extension.` |
|        - |  6691 | ` */` |
|        - |  6692 | `/*` |
|        - |  6693 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|        - |  6694 | ` */` |
|      570 |  6695 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|        5 |  6696 | `{` |
|        - |  6697 | `	sxu32 i;` |
|     1611 |  6698 | `	for( i = 0; i < n; i++ ){` |
|     1381 |  6699 | `		int a = zA[i], b = zB[i];` |
|     1381 |  6700 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|     1381 |  6701 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|     1381 |  6702 | `		if( a != b ) return a - b;` |
|      523 |  6703 | `	}` |
|      235 |  6704 | `	return 0;` |
|      290 |  6705 | `}` |
|        - |  6706 | `/*` |
|        - |  6707 | ` * Internal type-atom kinds used during union type parsing.` |
|        - |  6708 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|        - |  6709 | ` * (which are positive bit values stored in sxu32).` |
|        - |  6710 | ` */` |
|        - |  6711 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|        - |  6712 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|        - |  6713 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|        - |  6714 |  |
|        - |  6715 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|        - |  6716 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|        - |  6717 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|        - |  6718 |  |
|        - |  6719 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|        - |  6720 | `struct PhlTypeAtom {` |
|        - |  6721 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|        - |  6722 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|        - |  6723 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|        - |  6724 | `	sxu32 nCanon;` |
|        - |  6725 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|        - |  6726 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|        - |  6727 | `};` |
|        - |  6728 |  |
|        - |  6729 | `/*` |
|        - |  6730 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|        - |  6731 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|        - |  6732 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|        - |  6733 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|        - |  6734 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|        - |  6735 | ` * already be consumed by the caller.` |
|        - |  6736 | ` */` |
|    98640 |  6737 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|        5 |  6738 | `{` |
|    98645 |  6739 | `	SyToken *pIn = pGen->pIn;` |
|    98645 |  6740 | `	SyZero(pOut, sizeof(*pOut));` |
|    98645 |  6741 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|    98645 |  6742 | `	if( pIn >= pGen->pEnd ){` |
|      ! 0 |  6743 | `		return SXERR_SYNTAX;` |
|        - |  6744 | `	}` |
|        - |  6745 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|    98645 |  6746 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|        8 |  6747 | `		pIn++;` |
|        8 |  6748 | `		if( pIn >= pGen->pEnd ){` |
|      ! 0 |  6749 | `			return SXERR_SYNTAX;` |
|        - |  6750 | `		}` |
|        3 |  6751 | `	}` |
|    98645 |  6752 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  6753 | `		return SXERR_SYNTAX;` |
|        - |  6754 | `	}` |
|    98645 |  6755 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|    82569 |  6756 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|    82569 |  6757 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|    11693 |  6758 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|    76725 |  6759 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|       81 |  6760 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|    70843 |  6761 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|    19949 |  6762 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|    60833 |  6763 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|    50779 |  6764 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|    25474 |  6765 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|       41 |  6766 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|       68 |  6767 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|       27 |  6768 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|       37 |  6769 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       14 |  6770 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|       23 |  6771 | `			pOut->nType = SXU32_HIGH;` |
|       23 |  6772 | `			pOut->sClass = pIn->sData;` |
|       13 |  6773 | `		}else{` |
|        3 |  6774 | `			return SXERR_SYNTAX;` |
|        - |  6775 | `		}` |
|    82567 |  6776 | `		pIn++;` |
|    41286 |  6777 | `	}else{` |
|        - |  6778 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|        - |  6779 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|    16081 |  6780 | `		SyString *pT = &pIn->sData;` |
|    16081 |  6781 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|       34 |  6782 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|       34 |  6783 | `			pIn++;` |
|    16066 |  6784 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|      177 |  6785 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|      177 |  6786 | `			pIn++;` |
|    15965 |  6787 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       26 |  6788 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       26 |  6789 | `			pIn++;` |
|       15 |  6790 | `		}else{` |
|        - |  6791 | `			/* Class / interface name; consume namespace path a\b\c */` |
|    15857 |  6792 | `			SyToken *pFirst = pIn;` |
|    15857 |  6793 | `			SyToken *pLast = pIn;` |
|    15857 |  6794 | `			pOut->nType = SXU32_HIGH;` |
|    15857 |  6795 | `			pOut->sClass = pIn->sData;` |
|    15857 |  6796 | `			pIn++;` |
|    23781 |  6797 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|    15860 |  6798 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|        3 |  6799 | `				pLast = &pIn[1];` |
|        3 |  6800 | `				pIn += 2;` |
|        1 |  6801 | `			}` |
|    15857 |  6802 | `			if( pLast != pFirst ){` |
|        3 |  6803 | `				const char *zFirst = pFirst->sData.zString;` |
|        3 |  6804 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|        3 |  6805 | `				pOut->sClass.zString = zFirst;` |
|        3 |  6806 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|        1 |  6807 | `			}` |
|        - |  6808 | `		}` |
|        - |  6809 | `	}` |
|    98643 |  6810 | `	pGen->pIn = pIn;` |
|    98643 |  6811 | `	return SXRET_OK;` |
|    49325 |  6812 | `}` |
|        - |  6813 |  |
|        - |  6814 | `/*` |
|        - |  6815 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|        - |  6816 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|        - |  6817 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|        - |  6818 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|        - |  6819 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|        - |  6820 | ` */` |
|    98462 |  6821 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|        5 |  6822 | `{` |
|        - |  6823 | `	int i;` |
|    98467 |  6824 | `	int nNonNull = 0;` |
|    98467 |  6825 | `	int bAnyIntersection = 0;` |
|        - |  6826 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|    98467 |  6827 | `	sxu32 nMaxGroup = 0;` |
|  3249251 |  6828 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|   197081 |  6829 | `	for( i = 0; i < nAtoms; i++ ){` |
|    98619 |  6830 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    98589 |  6831 | `			nNonNull++;` |
|    98589 |  6832 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|    98589 |  6833 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|    98589 |  6834 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|    49292 |  6835 | `			}` |
|    49292 |  6836 | `		}` |
|    49312 |  6837 | `	}` |
|   197029 |  6838 | `	for( i = 0; i < nAtoms; i++ ){` |
|    98591 |  6839 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|       29 |  6840 | `			bAnyIntersection = 1;` |
|       29 |  6841 | `			break;` |
|        - |  6842 | `		}` |
|    49286 |  6843 | `	}` |
|    98467 |  6844 | `	if( bAnyIntersection ){` |
|        - |  6845 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|        - |  6846 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|        - |  6847 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|       29 |  6848 | `		sxu32 g, nGroups = 0;` |
|       29 |  6849 | `		int bFirstGroup = 1;` |
|       59 |  6850 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|       59 |  6851 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|       35 |  6852 | `			int bFirstMember = 1;` |
|        - |  6853 | `			int bWrap;` |
|       35 |  6854 | `			if( aGroupCount[g] == 0 ) continue;` |
|        - |  6855 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|        - |  6856 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|        - |  6857 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|        - |  6858 | `			 * parens, matching PHP's canonical text. */` |
|       47 |  6859 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|       35 |  6860 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|       35 |  6861 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|      107 |  6862 | `			for( i = 0; i < nAtoms; i++ ){` |
|       77 |  6863 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|       59 |  6864 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|       59 |  6865 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       55 |  6866 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       30 |  6867 | `				}else{` |
|        6 |  6868 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|        - |  6869 | `				}` |
|       59 |  6870 | `				bFirstMember = 0;` |
|       32 |  6871 | `			}` |
|       35 |  6872 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|       35 |  6873 | `			bFirstGroup = 0;` |
|       20 |  6874 | `		}` |
|       29 |  6875 | `		if( bNullable ){` |
|      ! 0 |  6876 | `			SyBlobAppend(pBlob, "\|", 1);` |
|      ! 0 |  6877 | `			SyBlobAppend(pBlob, "null", 4);` |
|      ! 0 |  6878 | `		}` |
|       78 |  6879 | `		return;` |
|        - |  6880 | `	}` |
|    98443 |  6881 | `	if( nNonNull == 1 && bNullable ){` |
|        - |  6882 | `		/* Shorthand: ?T */` |
|      102 |  6883 | `		for( i = 0; i < nAtoms; i++ ){` |
|      102 |  6884 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      102 |  6885 | `			SyBlobAppend(pBlob, "?", 1);` |
|      102 |  6886 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|       24 |  6887 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       14 |  6888 | `			}else{` |
|       82 |  6889 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|        - |  6890 | `			}` |
|      102 |  6891 | `			return;` |
|      ! 0 |  6892 | `		}` |
|      ! 0 |  6893 | `	}` |
|        - |  6894 | `	{` |
|    98345 |  6895 | `		int bFirst = 1;` |
|        - |  6896 | `		/* 1) Classes in declaration order */` |
|   196793 |  6897 | `		for( i = 0; i < nAtoms; i++ ){` |
|    98453 |  6898 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|    15807 |  6899 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    15807 |  6900 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|    15807 |  6901 | `				bFirst = 0;` |
|     7901 |  6902 | `			}` |
|    49229 |  6903 | `		}` |
|        - |  6904 | `		/* 2) Built-ins in canonical order */` |
|        - |  6905 | `		{` |
|        - |  6906 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|        - |  6907 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|        - |  6908 | `			int k;` |
|   688385 |  6909 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  1098177 |  6910 | `				for( i = 0; i < nAtoms; i++ ){` |
|   590581 |  6911 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|    82449 |  6912 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    82449 |  6913 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|    82449 |  6914 | `						bFirst = 0;` |
|    82449 |  6915 | `						break;` |
|        - |  6916 | `					}` |
|   254071 |  6917 | `				}` |
|   295025 |  6918 | `			}` |
|        - |  6919 | `		}` |
|        - |  6920 | `		/* 3) null suffix */` |
|    98345 |  6921 | `		if( bNullable ){` |
|       19 |  6922 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|       19 |  6923 | `			SyBlobAppend(pBlob, "null", 4);` |
|        8 |  6924 | `		}` |
|        - |  6925 | `	}` |
|    49236 |  6926 | `}` |
|        - |  6927 |  |
|        - |  6928 | `/*` |
|        - |  6929 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|        - |  6930 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|        - |  6931 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|        - |  6932 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|        - |  6933 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|        - |  6934 | ` * whether it was parenthesized.` |
|        - |  6935 | ` *` |
|        - |  6936 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|        - |  6937 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|        - |  6938 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|        - |  6939 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|        - |  6940 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|        - |  6941 | ` */` |
|    98614 |  6942 | `static sxi32 GenStateParsePart(` |
|        - |  6943 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|        - |  6944 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|        5 |  6945 | `{` |
|        - |  6946 | `	sxi32 rc;` |
|    98619 |  6947 | `	int nMembers = 0;` |
|    98619 |  6948 | `	int bParen = 0;` |
|    98619 |  6949 | `	*pnMembers = 0;` |
|    98619 |  6950 | `	*pbParen = 0;` |
|    98619 |  6951 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        9 |  6952 | `		bParen = 1;` |
|        9 |  6953 | `		pGen->pIn++; /* skip '(' */` |
|        3 |  6954 | `	}` |
|    49307 |  6955 | `	for(;;){` |
|    98645 |  6956 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|      ! 0 |  6957 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  6958 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|      ! 0 |  6959 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  6960 | `		}` |
|    98645 |  6961 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|    98645 |  6962 | `		if( rc != SXRET_OK ){` |
|        3 |  6963 | `			return rc;` |
|        - |  6964 | `		}` |
|    98643 |  6965 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|    98643 |  6966 | `		(*pnAtoms)++;` |
|    98643 |  6967 | `		nMembers++;` |
|        - |  6968 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|    98643 |  6969 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       39 |  6970 | `			SyToken *pNext = &pGen->pIn[1];` |
|       34 |  6971 | `			if( pNext < pGen->pEnd` |
|       39 |  6972 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       31 |  6973 | `				pGen->pIn++; /* skip '&' */` |
|       31 |  6974 | `				continue;` |
|        - |  6975 | `			}` |
|        4 |  6976 | `		}` |
|    98617 |  6977 | `		break;` |
|      ! 0 |  6978 | `	}` |
|    98617 |  6979 | `	if( bParen ){` |
|        9 |  6980 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 |  6981 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  6982 | `				"Malformed DNF type: expecting ')'");` |
|      ! 0 |  6983 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  6984 | `		}` |
|        9 |  6985 | `		pGen->pIn++; /* skip ')' */` |
|        9 |  6986 | `		if( nMembers < 2 ){` |
|      ! 0 |  6987 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  6988 | `				"Parenthesized type must be an intersection of at least two types");` |
|      ! 0 |  6989 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  6990 | `		}` |
|        3 |  6991 | `	}` |
|    98617 |  6992 | `	*pnMembers = nMembers;` |
|    98617 |  6993 | `	*pbParen = bParen;` |
|    98617 |  6994 | `	return SXRET_OK;` |
|    49312 |  6995 | `}` |
|        - |  6996 |  |
|        - |  6997 | `/*` |
|        - |  6998 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|        - |  6999 | ` *` |
|        - |  7000 | ` * Outputs:` |
|        - |  7001 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|        - |  7002 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|        - |  7003 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|        - |  7004 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|        - |  7005 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|        - |  7006 | ` *     already be initialized by the caller (allocator set, etc).` |
|        - |  7007 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|        - |  7008 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|        - |  7009 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|        - |  7010 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|        - |  7011 | ` *` |
|        - |  7012 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|        - |  7013 | ` * SXERR_ABORT on fatal compile errors.` |
|        - |  7014 | ` */` |
|    98478 |  7015 | `static sxi32 GenStateParseUnionTypeDecl(` |
|        - |  7016 | `	ph7_gen_state *pGen,` |
|        - |  7017 | `	sxu32 *pnType,` |
|        - |  7018 | `	SyString *pClass,` |
|        - |  7019 | `	SySet *pAlts,` |
|        - |  7020 | `	sxi32 *piTypeFlags,` |
|        - |  7021 | `	SyString *pTypeText,` |
|        - |  7022 | `	int iNullableFlag,` |
|        - |  7023 | `	int iUnionFlag,` |
|        - |  7024 | `	int bAllowVoid,` |
|        - |  7025 | `	sxu32 nLine` |
|        5 |  7026 | `){` |
|        - |  7027 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|    98483 |  7028 | `	int nAtoms = 0;` |
|    98483 |  7029 | `	int bShortNullable = 0;` |
|    98483 |  7030 | `	int bExplicitNull = 0;` |
|        - |  7031 | `	sxi32 rc;` |
|    98483 |  7032 | `	*pnType = 0;` |
|    98483 |  7033 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|    98483 |  7034 | `	*piTypeFlags = 0;` |
|    98483 |  7035 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|        - |  7036 |  |
|    98483 |  7037 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  7038 | `		return SXRET_OK;` |
|        - |  7039 | `	}` |
|        - |  7040 | ``	/* Optional `?` shorthand prefix */`` |
|    98478 |  7041 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|       91 |  7042 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|       90 |  7043 | `		bShortNullable = 1;` |
|       90 |  7044 | `		pGen->pIn++;` |
|       90 |  7045 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  7046 | `			return SXERR_SYNTAX;` |
|        - |  7047 | `		}` |
|       43 |  7048 | `	}` |
|        - |  7049 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|        - |  7050 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|        - |  7051 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|        - |  7052 | `	{` |
|        - |  7053 | `		int nMembers, bParen;` |
|    98483 |  7054 | `		sxu32 iGroup = 0;` |
|    98483 |  7055 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|    98483 |  7056 | `		if( rc != SXRET_OK ){` |
|        4 |  7057 | `			return rc;` |
|        - |  7058 | `		}` |
|        - |  7059 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|        - |  7060 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|        - |  7061 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|        - |  7062 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|        - |  7063 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|   147923 |  7064 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|    98690 |  7065 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      143 |  7066 | `			if( bShortNullable ){` |
|        - |  7067 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|        - |  7068 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|        - |  7069 | `				 * already reported" so callers skip their own error emission. */` |
|        3 |  7070 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|        - |  7071 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|        3 |  7072 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|        - |  7073 | `			}` |
|      141 |  7074 | `			if( nMembers >= 2 && !bParen ){` |
|      ! 0 |  7075 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|        - |  7076 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|      ! 0 |  7077 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  7078 | `			}` |
|      141 |  7079 | ``			pGen->pIn++; /* skip `\|` */`` |
|      141 |  7080 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|      141 |  7081 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  7082 | `				return rc;` |
|        - |  7083 | `			}` |
|        5 |  7084 | `		}` |
|    98479 |  7085 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|      ! 0 |  7086 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7087 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|      ! 0 |  7088 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  7089 | `		}` |
|        - |  7090 | `	}` |
|        - |  7091 | `	/* Validation pass.` |
|        - |  7092 | `	 *` |
|        - |  7093 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|        - |  7094 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|        - |  7095 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|        - |  7096 | `	 */` |
|        - |  7097 | `	{` |
|        - |  7098 | `		int i, j;` |
|    98479 |  7099 | `		int bHasNonNull = 0;` |
|    98479 |  7100 | `		int bAnyIntersection = 0;` |
|        - |  7101 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|        - |  7102 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|        - |  7103 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
|  3249647 |  7104 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|   197115 |  7105 | `		for( i = 0; i < nAtoms; i++ ){` |
|    98641 |  7106 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|    49323 |  7107 | `		}` |
|   197059 |  7108 | `		for( i = 0; i < nAtoms; i++ ){` |
|    98611 |  7109 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|    49295 |  7110 | `		}` |
|        - |  7111 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|        - |  7112 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|    98479 |  7113 | `		if( bShortNullable && bAnyIntersection ){` |
|      ! 0 |  7114 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7115 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|      ! 0 |  7116 | `			return SXERR_SYNTAX;` |
|        - |  7117 | `		}` |
|   197101 |  7118 | `		for( i = 0; i < nAtoms; i++ ){` |
|        - |  7119 | `			/* Intersection members must be class/interface types (PHP rejects` |
|        - |  7120 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|        - |  7121 | ``			 * `true`/`false` in an intersection). */`` |
|    98639 |  7122 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|       55 |  7123 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|       55 |  7124 | `				if( bClassLike ){` |
|       53 |  7125 | `					SyString *pC = &aAtoms[i].sClass;` |
|       48 |  7126 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|       48 |  7127 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|       48 |  7128 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|       53 |  7129 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|      ! 0 |  7130 | `						bClassLike = 0;` |
|      ! 0 |  7131 | `					}` |
|       24 |  7132 | `				}` |
|       55 |  7133 | `				if( !bClassLike ){` |
|        - |  7134 | `					const char *zName; sxu32 nName;` |
|        3 |  7135 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      ! 0 |  7136 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|      ! 0 |  7137 | `					}else{` |
|        3 |  7138 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|        - |  7139 | `					}` |
|        4 |  7140 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7141 | `						"Type %.*s cannot be part of an intersection type",` |
|        1 |  7142 | `						(int)nName, zName);` |
|        3 |  7143 | `					return SXERR_SYNTAX;` |
|        - |  7144 | `				}` |
|       24 |  7145 | `			}` |
|    98637 |  7146 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|      177 |  7147 | `				if( nAtoms > 1 ){` |
|        3 |  7148 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7149 | `						"Void can only be used as a standalone type");` |
|        3 |  7150 | `					return SXERR_SYNTAX;` |
|        - |  7151 | `				}` |
|      175 |  7152 | `				if( !bAllowVoid ){` |
|      ! 0 |  7153 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7154 | `						"void cannot be used here");` |
|      ! 0 |  7155 | `					return SXERR_SYNTAX;` |
|        - |  7156 | `				}` |
|      175 |  7157 | `				if( bShortNullable ){` |
|      ! 0 |  7158 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7159 | `						"Void type cannot be nullable");` |
|      ! 0 |  7160 | `					return SXERR_SYNTAX;` |
|        - |  7161 | `				}` |
|       85 |  7162 | `			}` |
|    98635 |  7163 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|        - |  7164 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|        - |  7165 | `				 * type (never = the function does not return). Mirrors the void` |
|        - |  7166 | `				 * validation above; accepted here and enforced at compile time` |
|        - |  7167 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|       26 |  7168 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|        - |  7169 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|        - |  7170 | `					 * same as any other non-standalone use. */` |
|        5 |  7171 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7172 | `						"never can only be used as a standalone type");` |
|        5 |  7173 | `					return SXERR_SYNTAX;` |
|        - |  7174 | `				}` |
|       21 |  7175 | `				if( !bAllowVoid ){` |
|        - |  7176 | `					/* Return-only: params call with bAllowVoid=0. */` |
|        3 |  7177 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7178 | `						"never cannot be used as a parameter type");` |
|        3 |  7179 | `					return SXERR_SYNTAX;` |
|        - |  7180 | `				}` |
|        8 |  7181 | `			}` |
|    98629 |  7182 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|       34 |  7183 | `				bExplicitNull = 1;` |
|       19 |  7184 | `			}else{` |
|    98599 |  7185 | `				bHasNonNull = 1;` |
|        - |  7186 | `			}` |
|        - |  7187 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|        - |  7188 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|        - |  7189 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|        - |  7190 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|        - |  7191 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|    98829 |  7192 | `			for( j = 0; j < i; j++ ){` |
|      207 |  7193 | `				int bDup = 0;` |
|      207 |  7194 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|      395 |  7195 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|      202 |  7196 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|      207 |  7197 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|      195 |  7198 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|       51 |  7199 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       44 |  7200 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|       44 |  7201 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|       17 |  7202 | `								aAtoms[j].sClass.zString,` |
|       34 |  7203 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|      ! 0 |  7204 | `							bDup = 1;` |
|      ! 0 |  7205 | `						}` |
|       27 |  7206 | `					}else{` |
|        3 |  7207 | `						bDup = 1;` |
|        - |  7208 | `					}` |
|       23 |  7209 | `				}` |
|      195 |  7210 | `				if( bDup ){` |
|        - |  7211 | `					const char *zName;` |
|        - |  7212 | `					sxu32 nName;` |
|        3 |  7213 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      ! 0 |  7214 | `						zName = aAtoms[i].sClass.zString;` |
|      ! 0 |  7215 | `						nName = aAtoms[i].sClass.nByte;` |
|      ! 0 |  7216 | `					}else{` |
|        3 |  7217 | `						zName = aAtoms[i].zCanon;` |
|        3 |  7218 | `						nName = aAtoms[i].nCanon;` |
|        - |  7219 | `					}` |
|        4 |  7220 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        1 |  7221 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|        3 |  7222 | `					return SXERR_SYNTAX;` |
|        - |  7223 | `				}` |
|       99 |  7224 | `			}` |
|    49316 |  7225 | `		}` |
|    98467 |  7226 | `		if( !bHasNonNull && bExplicitNull ){` |
|        7 |  7227 | `			if( bShortNullable ){` |
|        - |  7228 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|      ! 0 |  7229 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7230 | `					"Null can not be used as a standalone type");` |
|      ! 0 |  7231 | `				return SXERR_SYNTAX;` |
|        - |  7232 | `			}` |
|        - |  7233 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|        - |  7234 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|        - |  7235 | `			 * path below leaves *pnType untouched when there is no non-null` |
|        - |  7236 | `			 * atom, so set it here. */` |
|        7 |  7237 | `			*pnType = MEMOBJ_NULL;` |
|        3 |  7238 | `		}` |
|        - |  7239 | `	}` |
|        - |  7240 | `	/* Compute nullability flag */` |
|    98467 |  7241 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      118 |  7242 | `		*piTypeFlags \|= iNullableFlag;` |
|       57 |  7243 | `	}` |
|        - |  7244 | `	/* Build canonical type text */` |
|    98467 |  7245 | `	if( pTypeText ){` |
|        - |  7246 | `		SyBlob sBlob;` |
|    98467 |  7247 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|   147656 |  7248 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|    49231 |  7249 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|    98467 |  7250 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|   147419 |  7251 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    98276 |  7252 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|    98281 |  7253 | `			if( zDup ){` |
|    98281 |  7254 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|    49138 |  7255 | `			}` |
|    49138 |  7256 | `		}` |
|    98467 |  7257 | `		SyBlobRelease(&sBlob);` |
|    49231 |  7258 | `	}` |
|        - |  7259 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|        - |  7260 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|        - |  7261 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|        - |  7262 | `	{` |
|    98467 |  7263 | `		int nNonNull = 0;` |
|    98467 |  7264 | `		int iNonNullIdx = -1;` |
|        - |  7265 | `		int i;` |
|   197081 |  7266 | `		for( i = 0; i < nAtoms; i++ ){` |
|    98619 |  7267 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    98589 |  7268 | `				nNonNull++;` |
|    98589 |  7269 | `				iNonNullIdx = i;` |
|    49292 |  7270 | `			}` |
|    49312 |  7271 | `		}` |
|    98467 |  7272 | `		if( nNonNull <= 1 ){` |
|        - |  7273 | `			/* Fast path: store as single type. */` |
|    98361 |  7274 | `			if( iNonNullIdx >= 0 ){` |
|    98355 |  7275 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|    98355 |  7276 | `				if( pA->nType == SXU32_HIGH ){` |
|    23672 |  7277 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     7889 |  7278 | `						pA->sClass.zString, pA->sClass.nByte);` |
|    15783 |  7279 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|    15783 |  7280 | `					*pnType = SXU32_HIGH;` |
|    15783 |  7281 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|    90466 |  7282 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|      175 |  7283 | `					*pnType = MEMOBJ_VOID;` |
|    82492 |  7284 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|       18 |  7285 | `					*pnType = MEMOBJ_NEVER;` |
|       10 |  7286 | `				}else{` |
|    82391 |  7287 | `					*pnType = pA->nType;` |
|        - |  7288 | `				}` |
|    49175 |  7289 | `			}` |
|    49183 |  7290 | `		}else{` |
|        - |  7291 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      111 |  7292 | `			*piTypeFlags \|= iUnionFlag;` |
|      355 |  7293 | `			for( i = 0; i < nAtoms; i++ ){` |
|        - |  7294 | `				ph7_type_alt sAlt;` |
|      249 |  7295 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      239 |  7296 | `				SyZero(&sAlt, sizeof(sAlt));` |
|      239 |  7297 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|      239 |  7298 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      146 |  7299 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       47 |  7300 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       99 |  7301 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|       99 |  7302 | `					sAlt.nType = SXU32_HIGH;` |
|       99 |  7303 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|       52 |  7304 | `				}else{` |
|      145 |  7305 | `					sAlt.nType = aAtoms[i].nType;` |
|      145 |  7306 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|        - |  7307 | `				}` |
|      239 |  7308 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|      122 |  7309 | `			}` |
|        - |  7310 | `		}` |
|        - |  7311 | `	}` |
|    98467 |  7312 | `	return SXRET_OK;` |
|    49244 |  7313 | `}` |
|        - |  7314 |  |
|        - |  7315 | `/*` |
|        - |  7316 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|        - |  7317 | `` * pGen->pIn should point to the token after `)`.`` |
|        - |  7318 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|        - |  7319 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|        - |  7320 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|        - |  7321 | `` *          and union types `: T\|U`.`` |
|        - |  7322 | ` */` |
|  1506812 |  7323 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|        5 |  7324 | `{` |
|  1506817 |  7325 | `	sxi32 iFlags = 0;` |
|        - |  7326 | `	sxi32 rc;` |
|        - |  7327 | `	sxu32 nLine;` |
|  1506817 |  7328 | `	pFunc->nReturnType = 0;` |
|  1506817 |  7329 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  1506817 |  7330 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|        - |  7331 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|        - |  7332 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|        - |  7333 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|        - |  7334 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|        - |  7335 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|  1506817 |  7336 | `	SySetReset(&pFunc->aReturnUnion);` |
|  1506817 |  7337 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|  1506817 |  7338 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  1506165 |  7339 | `		return SXRET_OK;` |
|        - |  7340 | `	}` |
|      657 |  7341 | `	pGen->pIn++; /* Skip ':' */` |
|      657 |  7342 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  7343 | `		return SXRET_OK;` |
|        - |  7344 | `	}` |
|      657 |  7345 | `	nLine = pGen->pIn->nLine;` |
|      657 |  7346 | `	rc = GenStateParseUnionTypeDecl(` |
|      326 |  7347 | `		pGen,` |
|      326 |  7348 | `		&pFunc->nReturnType,` |
|      326 |  7349 | `		&pFunc->sReturnClass,` |
|      326 |  7350 | `		&pFunc->aReturnUnion,` |
|        - |  7351 | `		&iFlags,` |
|      326 |  7352 | `		&pFunc->sReturnTypeName,` |
|        - |  7353 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|        - |  7354 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|        - |  7355 | `		/* iUnionFlag */ 0,` |
|        - |  7356 | `		/* bAllowVoid */ 1,` |
|      326 |  7357 | `		nLine);` |
|      657 |  7358 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  7359 | `		return SXERR_ABORT;` |
|        - |  7360 | `	}` |
|      657 |  7361 | `	if( rc == SXERR_CORRUPT ){` |
|        - |  7362 | `		/* Error already reported */` |
|      ! 0 |  7363 | `		return SXERR_SYNTAX;` |
|        - |  7364 | `	}` |
|      657 |  7365 | `	if( rc == SXERR_SYNTAX ){` |
|        8 |  7366 | `		if( pGen->pIn < pGen->pEnd ){` |
|       11 |  7367 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|        - |  7368 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|        6 |  7369 | `				&pGen->pIn->sData);` |
|        5 |  7370 | `		}else{` |
|      ! 0 |  7371 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|        - |  7372 | `				"syntax error, unexpected end of file in return type declaration");` |
|        - |  7373 | `		}` |
|        8 |  7374 | `		return SXERR_SYNTAX;` |
|        - |  7375 | `	}` |
|      651 |  7376 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|      651 |  7377 | `	return SXRET_OK;` |
|   753411 |  7378 | `}` |
|        - |  7379 |  |
|   118436 |  7380 | `static sxi32 GenStateCompileFunc(` |
|        - |  7381 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  7382 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|        - |  7383 | `	sxi32 iFlags,        /* Control flags */` |
|        - |  7384 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|        - |  7385 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|        - |  7386 | `	)` |
|        5 |  7387 | `{` |
|        - |  7388 | `	ph7_vm_func *pFunc;` |
|        - |  7389 | `	SyToken *pEnd;` |
|        - |  7390 | `	sxu32 nLine;` |
|        - |  7391 | `	char *zName;` |
|        - |  7392 | `	sxi32 rc;` |
|        - |  7393 | `	/* Extract line number */` |
|   118441 |  7394 | `	nLine = pGen->pIn->nLine;` |
|        - |  7395 | `	/* Jump the left parenthesis '(' */` |
|   118441 |  7396 | `	pGen->pIn++;` |
|        - |  7397 | `	/* Delimit the function signature */` |
|   118441 |  7398 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   118441 |  7399 | `	if( pEnd >= pGen->pEnd ){` |
|        - |  7400 | `		/* Syntax error */` |
|        8 |  7401 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|        8 |  7402 | `		if( rc == SXERR_ABORT ){` |
|        - |  7403 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7404 | `			return SXERR_ABORT;` |
|        - |  7405 | `		}` |
|        8 |  7406 | `		pGen->pIn = pGen->pEnd;` |
|        8 |  7407 | `		return SXRET_OK;` |
|        - |  7408 | `	}` |
|        - |  7409 | `	/* Create the function state */` |
|   118435 |  7410 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   118435 |  7411 | `	if( pFunc == 0 ){` |
|      ! 0 |  7412 | `		goto OutOfMem;` |
|        - |  7413 | `	}` |
|        - |  7414 | `	/* Build the function name, prepending namespace if active */` |
|   118442 |  7415 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|        - |  7416 | `		SyBlob sFQN;` |
|        - |  7417 | `		sxu32 nLen;` |
|       16 |  7418 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       16 |  7419 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       16 |  7420 | `		SyBlobAppend(&sFQN,"\\",1);` |
|       16 |  7421 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|       16 |  7422 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|       16 |  7423 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|       16 |  7424 | `		SyBlobRelease(&sFQN);` |
|       16 |  7425 | `		if( zName == 0 ){` |
|      ! 0 |  7426 | `			goto OutOfMem;` |
|        - |  7427 | `		}` |
|       16 |  7428 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|        9 |  7429 | `	}else{` |
|   118421 |  7430 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   118421 |  7431 | `		if( zName == 0 ){` |
|      ! 0 |  7432 | `			goto OutOfMem;` |
|        - |  7433 | `		}` |
|   118421 |  7434 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|        - |  7435 | `	}` |
|        - |  7436 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|        - |  7437 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|   118435 |  7438 | `	pFunc->nLine = nLine;` |
|   118435 |  7439 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|   118435 |  7440 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  7441 | `		return SXERR_ABORT;` |
|        - |  7442 | `	}` |
|   118435 |  7443 | `	if( pGen->pIn < pEnd ){` |
|        - |  7444 | `		/* Collect function arguments */` |
|   102077 |  7445 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   102077 |  7446 | `		if( rc == SXERR_ABORT ){` |
|        - |  7447 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  7448 | `			return SXERR_ABORT;` |
|        - |  7449 | `		}` |
|    51036 |  7450 | `	}` |
|        - |  7451 | `	/* Point past ')' and parse optional return type ': type' */` |
|   118435 |  7452 | `	pGen->pIn = &pEnd[1];` |
|        - |  7453 | `	{` |
|   118435 |  7454 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   118435 |  7455 | `		if( rcRt == SXERR_ABORT ){` |
|      ! 0 |  7456 | `			return SXERR_ABORT;` |
|   118435 |  7457 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|        8 |  7458 | `			return SXERR_SYNTAX;` |
|        - |  7459 | `		}` |
|        - |  7460 | `	}` |
|   118429 |  7461 | `	if( bHandleClosure ){` |
|        - |  7462 | `		ph7_vm_func_closure_env sEnv;` |
|      453 |  7463 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|      448 |  7464 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      270 |  7465 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|       87 |  7466 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |  7467 | `				/* Closure,record environment variable */` |
|       87 |  7468 | `				pGen->pIn++;` |
|       87 |  7469 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 |  7470 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|      ! 0 |  7471 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  7472 | `						return SXERR_ABORT;` |
|        - |  7473 | `					}` |
|      ! 0 |  7474 | `				}` |
|       87 |  7475 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|        - |  7476 | `				/* Compile until we hit the first closing parenthesis */` |
|      179 |  7477 | `				while( pGen->pIn < pGen->pEnd ){` |
|      179 |  7478 | `					int iFlagsLocal = 0;` |
|      179 |  7479 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|       87 |  7480 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|       87 |  7481 | `						break;` |
|        - |  7482 | `					}` |
|       97 |  7483 | `					nLineLocal = pGen->pIn->nLine;` |
|       97 |  7484 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|        - |  7485 | `						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry` |
|        - |  7486 | `						 * to the variable's memory slot instead of copying its value. */` |
|       53 |  7487 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|       53 |  7488 | `						pGen->pIn++;` |
|       26 |  7489 | `					}` |
|       92 |  7490 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|       97 |  7491 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  7492 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        - |  7493 | `								"Closure: Unexpected token. Expecting a variable name");` |
|      ! 0 |  7494 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 |  7495 | `								return SXERR_ABORT;` |
|        - |  7496 | `							}` |
|        - |  7497 | `							/* Find the closing parenthesis */` |
|      ! 0 |  7498 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 |  7499 | `								pGen->pIn++;` |
|      ! 0 |  7500 | `							}` |
|      ! 0 |  7501 | `							if(pGen->pIn < pGen->pEnd){` |
|      ! 0 |  7502 | `								pGen->pIn++;` |
|      ! 0 |  7503 | `							}` |
|      ! 0 |  7504 | `							break;` |
|        - |  7505 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|      ! 0 |  7506 | `					}else{` |
|        - |  7507 | `						SyString *pNameLocal;` |
|        - |  7508 | `						char *zDup;` |
|        - |  7509 | `						/* Duplicate variable name */` |
|       97 |  7510 | `						pNameLocal = &pGen->pIn[1].sData;` |
|       97 |  7511 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|       97 |  7512 | `						if( zDup ){` |
|        - |  7513 | `							/* Zero the structure */` |
|       97 |  7514 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       97 |  7515 | `							sEnv.iFlags = iFlagsLocal;` |
|       97 |  7516 | `							sEnv.nIdx = SXU32_HIGH;` |
|       97 |  7517 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       97 |  7518 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      112 |  7519 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|       30 |  7520 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|      ! 0 |  7521 | `									got_this = 1;` |
|      ! 0 |  7522 | `							}` |
|        - |  7523 | `							/* Save imported variable */` |
|       97 |  7524 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       51 |  7525 | `						}else{` |
|      ! 0 |  7526 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  7527 | `							 return SXERR_ABORT;` |
|        - |  7528 | `						}` |
|        - |  7529 | `					}` |
|       97 |  7530 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      109 |  7531 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  7532 | `						/* Ignore trailing commas */` |
|       13 |  7533 | `						pGen->pIn++;` |
|        1 |  7534 | `					}` |
|        5 |  7535 | `				}` |
|        - |  7536 | `				/* php 7.1+: the return type follows the use clause —` |
|        - |  7537 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|        - |  7538 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|        - |  7539 | `				 * so an unconditional call would wipe a type parsed at the` |
|        - |  7540 | `				 * legacy pre-use position. */` |
|       87 |  7541 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|        7 |  7542 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|        7 |  7543 | `					if( rcRt2 == SXERR_ABORT ){` |
|      ! 0 |  7544 | `						return SXERR_ABORT;` |
|        7 |  7545 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|      ! 0 |  7546 | `						return SXERR_SYNTAX;` |
|        - |  7547 | `					}` |
|        3 |  7548 | `				}` |
|       41 |  7549 | `		}` |
|      453 |  7550 | `		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){` |
|        - |  7551 | `			/* Make the $this variable [Current processed Object (class instance)]` |
|        - |  7552 | `			 * available to the closure environment — for EVERY non-static` |
|        - |  7553 | `			 * anonymous function, use list or not (php binds $this to any` |
|        - |  7554 | ``			 * closure declared in a method; pre-fix only `use (...)` closures`` |
|        - |  7555 | `			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of` |
|        - |  7556 | `			 * a global-scope closure is silently dropped at install. A static` |
|        - |  7557 | `			 * closure never binds $this (php). */` |
|      445 |  7558 | `			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      445 |  7559 | `			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      445 |  7560 | `			sEnv.nIdx = SXU32_HIGH;` |
|      445 |  7561 | `			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      445 |  7562 | `			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      445 |  7563 | `			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      220 |  7564 | `		}` |
|      453 |  7565 | `		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|        - |  7566 | `			/* Mark as closure */` |
|      447 |  7567 | `			pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|      221 |  7568 | `		}` |
|      224 |  7569 | `	}` |
|        - |  7570 | `	/* Compile the body */` |
|   118429 |  7571 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   118429 |  7572 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  7573 | `		return SXERR_ABORT;` |
|        - |  7574 | `	}` |
|        - |  7575 | `	/* The cursor sits just past the body's closing brace */` |
|   118429 |  7576 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|   118429 |  7577 | `	if( ppFunc ){` |
|   118429 |  7578 | `		*ppFunc = pFunc;` |
|    59212 |  7579 | `	}` |
|   118429 |  7580 | `	rc = SXRET_OK;` |
|   118429 |  7581 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|        - |  7582 | `		/* Finally register the function */` |
|   117987 |  7583 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|    58991 |  7584 | `	}` |
|   118429 |  7585 | `	if( rc == SXRET_OK ){` |
|   118429 |  7586 | `		return SXRET_OK;` |
|        - |  7587 | `	}` |
|        - |  7588 | `	/* Fall through if something goes wrong */` |
|      ! 0 |  7589 | `OutOfMem:` |
|        - |  7590 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  7591 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  7592 | `	 */` |
|      ! 0 |  7593 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 |  7594 | `	return SXERR_ABORT;` |
|    59223 |  7595 | `}` |
|        - |  7596 | `/*` |
|        - |  7597 | ` * Compile a standard PHP function.` |
|        - |  7598 | ` *  Refer to the block-comment above for more information.` |
|        - |  7599 | ` */` |
|   117996 |  7600 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|        5 |  7601 | `{` |
|        - |  7602 | `	SyString *pName;` |
|        - |  7603 | `	sxi32 iFlags;` |
|        - |  7604 | `	sxu32 nKwLine;` |
|        - |  7605 | `	sxu32 nLine;` |
|        - |  7606 | `	sxi32 rc;` |
|        - |  7607 |  |
|   118001 |  7608 | `	nLine = pGen->pIn->nLine;` |
|   118001 |  7609 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|   118001 |  7610 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   118001 |  7611 | `	iFlags = 0;` |
|   118001 |  7612 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        - |  7613 | `		/* Return by reference,remember that */` |
|       12 |  7614 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|        - |  7615 | `		/* Jump the '&' token */` |
|       12 |  7616 | `		pGen->pIn++;` |
|        5 |  7617 | `	}` |
|   118001 |  7618 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  7619 | `		/* Invalid function name */` |
|        8 |  7620 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|        8 |  7621 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  7622 | `			return SXERR_ABORT;` |
|        - |  7623 | `		}` |
|        - |  7624 | `		/* Sychronize with the next semi-colon or braces*/` |
|       22 |  7625 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       16 |  7626 | `			pGen->pIn++;` |
|        2 |  7627 | `		}` |
|        8 |  7628 | `		return SXRET_OK;` |
|        - |  7629 | `	}` |
|   117995 |  7630 | `	pName = &pGen->pIn->sData;` |
|   117995 |  7631 | `	nLine = pGen->pIn->nLine;` |
|        - |  7632 | `	/* Jump the function name */` |
|   117995 |  7633 | `	pGen->pIn++;` |
|   117995 |  7634 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  7635 | `		/* Syntax error */` |
|        3 |  7636 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|        3 |  7637 | `		if( rc == SXERR_ABORT ){` |
|        - |  7638 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7639 | `			return SXERR_ABORT;` |
|        - |  7640 | `		}` |
|        - |  7641 | `		/* Sychronize with the next semi-colon or '{' */` |
|        3 |  7642 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  7643 | `			pGen->pIn++;` |
|      ! 0 |  7644 | `		}` |
|        3 |  7645 | `		return SXRET_OK;` |
|        - |  7646 | `	}` |
|        - |  7647 | `	/* Compile function body */` |
|        - |  7648 | `	{` |
|   117993 |  7649 | `		ph7_vm_func *pFuncState = 0;` |
|   117993 |  7650 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);` |
|   117993 |  7651 | `		if( pFuncState ){` |
|        - |  7652 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|   117981 |  7653 | `			pFuncState->nLine = nKwLine;` |
|    58988 |  7654 | `		}` |
|        - |  7655 | `	}` |
|   117993 |  7656 | `	return rc;` |
|    59003 |  7657 | `}` |
|        - |  7658 | `/*` |
|        - |  7659 | ` * Extract the visibility level associated with a given keyword.` |
|        - |  7660 | ` * According to the PHP language reference manual` |
|        - |  7661 | ` *  Visibility:` |
|        - |  7662 | ` *  The visibility of a property or method can be defined by prefixing` |
|        - |  7663 | ` *  the declaration with the keywords public, protected or private.` |
|        - |  7664 | ` *  Class members declared public can be accessed everywhere.` |
|        - |  7665 | ` *  Members declared protected can be accessed only within the class` |
|        - |  7666 | ` *  itself and by inherited and parent classes. Members declared as private` |
|        - |  7667 | ` *  may only be accessed by the class that defines the member.` |
|        - |  7668 | ` */` |
|  1742554 |  7669 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|        5 |  7670 | `{` |
|  1742559 |  7671 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    23467 |  7672 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  1719097 |  7673 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   182629 |  7674 | `		return PH7_CLASS_PROT_PROTECTED;` |
|        - |  7675 | `	}` |
|        - |  7676 | `	/* Assume public by default */` |
|  1536473 |  7677 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   871282 |  7678 | `}` |
|        - |  7679 | `/*` |
|        - |  7680 | ` * Compile a class constant.` |
|        - |  7681 | ` * According to the PHP language reference manual` |
|        - |  7682 | ` *  Class Constants` |
|        - |  7683 | ` *   It is possible to define constant values on a per-class basis remaining` |
|        - |  7684 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|        - |  7685 | ` *   you don't use the $ symbol to declare or use them.` |
|        - |  7686 | ` *   The value must be a constant expression, not (for example) a variable,` |
|        - |  7687 | ` *   a property, a result of a mathematical operation, or a function call.` |
|        - |  7688 | ` *   It's also possible for interfaces to have constants.` |
|        - |  7689 | ` * Symisc eXtension.` |
|        - |  7690 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|        - |  7691 | ` *  the zend engine would allow only simple scalar value.` |
|        - |  7692 | ` *  Example:` |
|        - |  7693 | ` *   class Test{` |
|        - |  7694 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - |  7695 | ` *   };` |
|        - |  7696 | ` *   var_dump(TEST::MyConst);` |
|        - |  7697 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - |  7698 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - |  7699 | ` */` |
|        - |  7700 | `/*` |
|        - |  7701 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|        - |  7702 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|        - |  7703 | ` * token immediately followed by '='. Anything else with a leading type token` |
|        - |  7704 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|        - |  7705 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|        - |  7706 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|        - |  7707 | ` */` |
|   143884 |  7708 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|        5 |  7709 | `{` |
|        - |  7710 | `	SyToken *p0, *p1;` |
|   143889 |  7711 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  7712 | `		return 0;` |
|        - |  7713 | `	}` |
|   143889 |  7714 | `	p0 = pGen->pIn;` |
|        - |  7715 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|   143889 |  7716 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|      ! 0 |  7717 | `		return 1;` |
|        - |  7718 | `	}` |
|   143889 |  7719 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|        5 |  7720 | `		return 1;` |
|        - |  7721 | `	}` |
|        - |  7722 | `	/* A name-like first token begins a type only when followed by another` |
|        - |  7723 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|        - |  7724 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|   143885 |  7725 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|   143885 |  7726 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|   143885 |  7727 | `		if( p1 ){` |
|   143885 |  7728 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|       34 |  7729 | `				return 1;` |
|        - |  7730 | `			}` |
|   143855 |  7731 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|        5 |  7732 | `				return 1;` |
|        - |  7733 | `			}` |
|    71923 |  7734 | `		}` |
|    71923 |  7735 | `	}` |
|   143851 |  7736 | `	return 0;` |
|    71947 |  7737 | `}` |
|        - |  7738 | `/*` |
|        - |  7739 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|        - |  7740 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|        - |  7741 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|        - |  7742 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|        - |  7743 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|        - |  7744 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|        - |  7745 | ` * Peek only; never consumes tokens.` |
|        - |  7746 | ` */` |
|       24 |  7747 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|        4 |  7748 | `{` |
|       28 |  7749 | `	SyToken *p = pGen->pIn;` |
|       39 |  7750 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|       20 |  7751 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|        3 |  7752 | `		p++; /* skip leading unary sign(s) */` |
|        1 |  7753 | `	}` |
|       28 |  7754 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|       23 |  7755 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|        - |  7756 | `	}` |
|        6 |  7757 | `	p++;` |
|        - |  7758 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|        6 |  7759 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|       16 |  7760 | `}` |
|        - |  7761 | `/*` |
|        - |  7762 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|        - |  7763 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|        - |  7764 | `` * `$o->new`), not a `new` expression.`` |
|        - |  7765 | ` */` |
|        6 |  7766 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|        3 |  7767 | `{` |
|        - |  7768 | `	sxi32 iOp;` |
|        9 |  7769 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|      ! 0 |  7770 | `		return 0;` |
|        - |  7771 | `	}` |
|        9 |  7772 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|        9 |  7773 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|        6 |  7774 | `}` |
|        - |  7775 | `/*` |
|        - |  7776 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|        - |  7777 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|        - |  7778 | ` * interface-constant and (instance/static) property-default initializers` |
|        - |  7779 | ` * ("New expressions are not supported in this context") while still allowing it` |
|        - |  7780 | ` * in global constants, parameter defaults and static-local initializers (which` |
|        - |  7781 | ` * are compiled by different functions and left untouched). The scan is` |
|        - |  7782 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|        - |  7783 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|        - |  7784 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|        - |  7785 | ` *` |
|        - |  7786 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|        - |  7787 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|        - |  7788 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|        - |  7789 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|        - |  7790 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|        - |  7791 | ` */` |
|   229920 |  7792 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|        5 |  7793 | `{` |
|   229925 |  7794 | `	SyToken *p = pGen->pIn;` |
|   229925 |  7795 | `	int iDepth = 0;` |
|   561815 |  7796 | `	while( p < pGen->pEnd ){` |
|   561815 |  7797 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|   229917 |  7798 | `			break; /* end of this initializer */` |
|        - |  7799 | `		}` |
|   331898 |  7800 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|   169854 |  7801 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|     7800 |  7802 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|        - |  7803 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|        - |  7804 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|        - |  7805 | `			 * expression. */` |
|        3 |  7806 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|        3 |  7807 | `			p++;` |
|        3 |  7808 | `			if( bArrow ){` |
|        - |  7809 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|        - |  7810 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|        3 |  7811 | `				int iBase = iDepth;` |
|       17 |  7812 | `				while( p < pGen->pEnd ){` |
|       17 |  7813 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        5 |  7814 | `						iDepth++;` |
|       15 |  7815 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        5 |  7816 | `						if( iDepth <= iBase ){` |
|      ! 0 |  7817 | `							break; /* closes an enclosing group, not the fn's own */` |
|        - |  7818 | `						}` |
|        5 |  7819 | `						iDepth--;` |
|       11 |  7820 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|        3 |  7821 | `						break;` |
|        - |  7822 | `					}` |
|       15 |  7823 | `					p++;` |
|        1 |  7824 | `				}` |
|        2 |  7825 | `			}else{` |
|        - |  7826 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|        - |  7827 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|        - |  7828 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|        - |  7829 | `				 * then skip the balanced brace block. */` |
|      ! 0 |  7830 | `				int iLocal = 0;` |
|      ! 0 |  7831 | `				while( p < pGen->pEnd ){` |
|      ! 0 |  7832 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|      ! 0 |  7833 | `						break; /* body brace */` |
|        - |  7834 | `					}` |
|      ! 0 |  7835 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      ! 0 |  7836 | `						iLocal++;` |
|      ! 0 |  7837 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      ! 0 |  7838 | `						if( iLocal > 0 ){` |
|      ! 0 |  7839 | `							iLocal--;` |
|      ! 0 |  7840 | `						}` |
|      ! 0 |  7841 | `					}` |
|      ! 0 |  7842 | `					p++;` |
|      ! 0 |  7843 | `				}` |
|      ! 0 |  7844 | `				if( p < pGen->pEnd ){` |
|      ! 0 |  7845 | `					int iBrace = 0; /* p is on the body '{' */` |
|      ! 0 |  7846 | `					while( p < pGen->pEnd ){` |
|      ! 0 |  7847 | `						if( p->nType & PH7_TK_OCB ){` |
|      ! 0 |  7848 | `							iBrace++;` |
|      ! 0 |  7849 | `						}else if( p->nType & PH7_TK_CCB ){` |
|      ! 0 |  7850 | `							iBrace--;` |
|      ! 0 |  7851 | `							if( iBrace == 0 ){` |
|      ! 0 |  7852 | `								p++;` |
|      ! 0 |  7853 | `								break;` |
|        - |  7854 | `							}` |
|      ! 0 |  7855 | `						}` |
|      ! 0 |  7856 | `						p++;` |
|      ! 0 |  7857 | `					}` |
|      ! 0 |  7858 | `				}` |
|        - |  7859 | `			}` |
|        3 |  7860 | `			continue;` |
|        - |  7861 | `		}` |
|   331901 |  7862 | `		if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     7845 |  7863 | `			iDepth++;` |
|   327981 |  7864 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     7843 |  7865 | `			if( iDepth > 0 ){` |
|     7843 |  7866 | `				iDepth--;` |
|     3919 |  7867 | `			}` |
|   320142 |  7868 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|    86143 |  7869 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|        - |  7870 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|        - |  7871 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|        - |  7872 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|       11 |  7873 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|       11 |  7874 | `				return 1;` |
|        - |  7875 | `			}` |
|      ! 0 |  7876 | `		}` |
|   331893 |  7877 | `		p++;` |
|        5 |  7878 | `	}` |
|   229917 |  7879 | `	return 0;` |
|   114965 |  7880 | `}` |
|        - |  7881 | `/*` |
|        - |  7882 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|        - |  7883 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|        - |  7884 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|        - |  7885 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|        - |  7886 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|        - |  7887 | ` * share the same backing.` |
|        - |  7888 | ` */` |
|      226 |  7889 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|        - |  7890 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|        5 |  7891 | `{` |
|      231 |  7892 | `	pAttr->nType = nType;` |
|      231 |  7893 | `	pAttr->sClass = *pClass;` |
|      231 |  7894 | `	pAttr->sTypeName = *pTypeName;` |
|      231 |  7895 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|        - |  7896 | `		sxu32 i;` |
|       73 |  7897 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|       51 |  7898 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|       51 |  7899 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|       28 |  7900 | `		}` |
|       11 |  7901 | `	}` |
|      231 |  7902 | `}` |
|   143884 |  7903 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 |  7904 | `{` |
|   143889 |  7905 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  7906 | `	SySet *pInstrContainer;` |
|        - |  7907 | `	ph7_class_attr *pCons;` |
|        - |  7908 | `	SyString *pName;` |
|        - |  7909 | `	sxi32 rc;` |
|   143889 |  7910 | `	sxu32 nType = 0;` |
|        - |  7911 | `	SyString sTypeClass;` |
|        - |  7912 | `	SyString sTypeText;` |
|        - |  7913 | `	SySet aUnionAlts;` |
|   143889 |  7914 | `	sxi32 iTypeFlags = 0;` |
|   143889 |  7915 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   143889 |  7916 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   143889 |  7917 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  7918 | `	/* Extract visibility level */` |
|   143889 |  7919 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - |  7920 | `	/* Mark as constant */` |
|   143889 |  7921 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|   143889 |  7922 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        - |  7923 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|        - |  7924 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|   143908 |  7925 | `	if( GenStateClassConstHasType(pGen) ){` |
|       61 |  7926 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|       38 |  7927 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|        - |  7928 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|        - |  7929 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|        - |  7930 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|        - |  7931 | `		 * and success paths release. */` |
|       42 |  7932 | `		if( rc == SXERR_CORRUPT ){` |
|        - |  7933 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|      ! 0 |  7934 | `			goto Synchronize;` |
|       42 |  7935 | `		}else if( rc == SXERR_ABORT ){` |
|      ! 0 |  7936 | `			return SXERR_ABORT;` |
|       42 |  7937 | `		}else if( rc != SXRET_OK ){` |
|      ! 0 |  7938 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  7939 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|      ! 0 |  7940 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  7941 | `				return SXERR_ABORT;` |
|        - |  7942 | `			}` |
|      ! 0 |  7943 | `			goto Synchronize;` |
|        - |  7944 | `		}` |
|       42 |  7945 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|       19 |  7946 | `	}` |
|    71942 |  7947 | `loop:` |
|   143891 |  7948 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|        - |  7949 | `		/* Invalid constant name */` |
|      ! 0 |  7950 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|      ! 0 |  7951 | `		if( rc == SXERR_ABORT ){` |
|        - |  7952 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7953 | `			return SXERR_ABORT;` |
|        - |  7954 | `		}` |
|      ! 0 |  7955 | `		goto Synchronize;` |
|        - |  7956 | `	}` |
|        - |  7957 | `	/* Peek constant name */` |
|   143891 |  7958 | `	pName = &pGen->pIn->sData;` |
|        - |  7959 | `	/* Make sure the constant name isn't reserved */` |
|   143891 |  7960 | `	if( GenStateIsReservedConstant(pName) ){` |
|        - |  7961 | `		/* Reserved constant name */` |
|      ! 0 |  7962 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|      ! 0 |  7963 | `		if( rc == SXERR_ABORT ){` |
|        - |  7964 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7965 | `			return SXERR_ABORT;` |
|        - |  7966 | `		}` |
|      ! 0 |  7967 | `		goto Synchronize;` |
|        - |  7968 | `	}` |
|        - |  7969 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|   143891 |  7970 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       61 |  7971 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|       38 |  7972 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       19 |  7973 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|       42 |  7974 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  7975 | `			return SXERR_ABORT;` |
|       42 |  7976 | `		}else if( rc != SXRET_OK ){` |
|        3 |  7977 | `			goto Synchronize;` |
|        - |  7978 | `		}` |
|       18 |  7979 | `	}` |
|        - |  7980 | `	/* Advance the stream cursor */` |
|   143889 |  7981 | `	pGen->pIn++;` |
|   143889 |  7982 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|        - |  7983 | `		/* Invalid declaration */` |
|      ! 0 |  7984 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|      ! 0 |  7985 | `		if( rc == SXERR_ABORT ){` |
|        - |  7986 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7987 | `			return SXERR_ABORT;` |
|        - |  7988 | `		}` |
|      ! 0 |  7989 | `		goto Synchronize;` |
|        - |  7990 | `	}` |
|   143889 |  7991 | `	pGen->pIn++; /* Jump the equal sign */` |
|        - |  7992 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|        - |  7993 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|        - |  7994 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|        - |  7995 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|   143884 |  7996 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|       39 |  7997 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|        8 |  7998 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  7999 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|        2 |  8000 | `			&pClass->sName,pName,&sTypeText);` |
|        6 |  8001 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8002 | `			return SXERR_ABORT;` |
|        - |  8003 | `		}` |
|        6 |  8004 | `		goto Synchronize;` |
|        - |  8005 | `	}` |
|        - |  8006 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|        - |  8007 | `	 * constant initializer ("New expressions are not supported in this context").` |
|        - |  8008 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|   143885 |  8009 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|        5 |  8010 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8011 | `			"New expressions are not supported in this context");` |
|        5 |  8012 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8013 | `			return SXERR_ABORT;` |
|        - |  8014 | `		}` |
|        5 |  8015 | `		goto Synchronize;` |
|        - |  8016 | `	}` |
|        - |  8017 | `	/* Allocate a new class attribute */` |
|   143881 |  8018 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   143881 |  8019 | `	if( pCons ){` |
|   143881 |  8020 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|   143881 |  8021 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  8022 | `			return SXERR_ABORT;` |
|        - |  8023 | `		}` |
|    71938 |  8024 | `	}` |
|   143881 |  8025 | `	if( pCons == 0 ){` |
|      ! 0 |  8026 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8027 | `		return SXERR_ABORT;` |
|        - |  8028 | `	}` |
|   143881 |  8029 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       35 |  8030 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       16 |  8031 | `	}` |
|        - |  8032 | `	/* Swap bytecode container */` |
|   143881 |  8033 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   143881 |  8034 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|        - |  8035 | `	/* Compile constant value.` |
|        - |  8036 | `	 */` |
|   143881 |  8037 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   143881 |  8038 | `	if( rc == SXERR_EMPTY ){` |
|        3 |  8039 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|        3 |  8040 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8041 | `			return SXERR_ABORT;` |
|        - |  8042 | `		}` |
|        1 |  8043 | `	}` |
|        - |  8044 | `	/* Emit the done instruction */` |
|   143881 |  8045 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   143881 |  8046 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   143881 |  8047 | `	if( rc == SXERR_ABORT ){` |
|        - |  8048 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  8049 | `		return SXERR_ABORT;` |
|        - |  8050 | `	}` |
|        - |  8051 | `	/* All done,install the constant */` |
|   143881 |  8052 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|   143881 |  8053 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8054 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8055 | `		return SXERR_ABORT;` |
|        - |  8056 | `	}` |
|   143881 |  8057 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  8058 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|        3 |  8059 | `		pGen->pIn++; /* Jump the comma */` |
|        3 |  8060 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 |  8061 | `			SyToken *pTok = pGen->pIn;` |
|      ! 0 |  8062 | `			if( pTok >= pGen->pEnd ){` |
|      ! 0 |  8063 | `				pTok--;` |
|      ! 0 |  8064 | `			}` |
|      ! 0 |  8065 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  8066 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|      ! 0 |  8067 | `				&pTok->sData,&pClass->sName);` |
|      ! 0 |  8068 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8069 | `				return SXERR_ABORT;` |
|        - |  8070 | `			}` |
|      ! 0 |  8071 | `		}else{` |
|        3 |  8072 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|        3 |  8073 | `				goto loop;` |
|        - |  8074 | `			}` |
|        - |  8075 | `		}` |
|      ! 0 |  8076 | `	}` |
|   143879 |  8077 | `	SySetRelease(&aUnionAlts);` |
|   143879 |  8078 | `	return SXRET_OK;` |
|        5 |  8079 | `Synchronize:` |
|       13 |  8080 | `	SySetRelease(&aUnionAlts);` |
|        - |  8081 | `	/* Synchronize with the first semi-colon */` |
|       45 |  8082 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       35 |  8083 | `		pGen->pIn++;` |
|        3 |  8084 | `	}` |
|       13 |  8085 | `	return SXERR_CORRUPT;` |
|    71947 |  8086 | `}` |
|        - |  8087 | `/*` |
|        - |  8088 | ` * complie a class attribute or Properties in the PHP jargon.` |
|        - |  8089 | ` * According to the PHP language reference manual` |
|        - |  8090 | ` *  Properties` |
|        - |  8091 | ` *  Class member variables are called "properties". You may also see them referred` |
|        - |  8092 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|        - |  8093 | ` *  of this reference we will use "properties". They are defined by using one` |
|        - |  8094 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|        - |  8095 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|        - |  8096 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|        - |  8097 | ` *  and must not depend on run-time information in order to be evaluated.` |
|        - |  8098 | ` * Symisc eXtension.` |
|        - |  8099 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|        - |  8100 | ` *  the zend engine would allow only simple scalar value.` |
|        - |  8101 | ` *  Example:` |
|        - |  8102 | ` *   class Test{` |
|        - |  8103 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - |  8104 | ` *   };` |
|        - |  8105 | ` *   var_dump(TEST::myVar);` |
|        - |  8106 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - |  8107 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - |  8108 | ` */` |
|        - |  8109 | `/*` |
|        - |  8110 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|        - |  8111 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|        - |  8112 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|        - |  8113 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|        - |  8114 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|        - |  8115 | ` */` |
|  1310368 |  8116 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|        5 |  8117 | `{` |
|  1310373 |  8118 | `	SyToken *p = pStart;` |
|  1310373 |  8119 | `	int bFirst = 1;` |
|  1310373 |  8120 | `	if( p >= pEnd ) return 0;` |
|        - |  8121 | ``	/* Optional nullable `?` shorthand. */`` |
|  1310373 |  8122 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|       25 |  8123 | `		p++;` |
|       25 |  8124 | `		if( p >= pEnd ) return 0;` |
|       11 |  8125 | `	}` |
|        - |  8126 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|        - |  8127 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|        - |  8128 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|        - |  8129 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   655184 |  8130 | `	for(;;){` |
|  1310393 |  8131 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|        - |  8132 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|        3 |  8133 | `			p++;` |
|        9 |  8134 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|        3 |  8135 | `			if( p >= pEnd ) return 0;` |
|        3 |  8136 | `			p++; /* skip ')' */` |
|        2 |  8137 | `		}else{` |
|        - |  8138 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|        - |  8139 | ``			 * then any `&`-joined intersection members. */`` |
|  1310391 |  8140 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|  1310391 |  8141 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  8142 | `				return 0;` |
|        - |  8143 | `			}` |
|        - |  8144 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|        - |  8145 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|        - |  8146 | `			 * may still appear at the initial dispatch site). */` |
|  1310391 |  8147 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|  1310343 |  8148 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  1310338 |  8149 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    23590 |  8150 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  1310175 |  8151 | `					return 0;` |
|        - |  8152 | `				}` |
|       84 |  8153 | `			}` |
|      221 |  8154 | `			p++;` |
|      223 |  8155 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  8156 | `				p += 2;` |
|        1 |  8157 | `			}` |
|      327 |  8158 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|      224 |  8159 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  8160 | `				p++; /* skip '&' */` |
|        3 |  8161 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|        3 |  8162 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|        3 |  8163 | `				p++;` |
|        3 |  8164 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      ! 0 |  8165 | `					p += 2;` |
|      ! 0 |  8166 | `				}` |
|        1 |  8167 | `			}` |
|        - |  8168 | `		}` |
|      223 |  8169 | `		bFirst = 0;` |
|      218 |  8170 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|       25 |  8171 | `			&& p->sData.zString[0] == '\|' ){` |
|       25 |  8172 | ``			p++; /* next `\|`-separated part */`` |
|       25 |  8173 | `			continue;` |
|        - |  8174 | `		}` |
|      203 |  8175 | `		break;` |
|      ! 0 |  8176 | `	}` |
|      203 |  8177 | `	if( p >= pEnd ) return 0;` |
|      203 |  8178 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   655189 |  8179 | `}` |
|        - |  8180 |  |
|        - |  8181 | `/*` |
|        - |  8182 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|        - |  8183 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|        - |  8184 | ` * if not). Recognized forms:` |
|        - |  8185 | ` *   ?Type, array, bool, int, float, string, object,` |
|        - |  8186 | ` *   self, parent, \Ns\ClassName, ClassName` |
|        - |  8187 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|        - |  8188 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|        - |  8189 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|        - |  8190 | ` * on unrecoverable error.` |
|        - |  8191 | ` *` |
|        - |  8192 | ` * When a type is parsed:` |
|        - |  8193 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|        - |  8194 | ` *   *pClass is set to the class name (for class types)` |
|        - |  8195 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|        - |  8196 | ` *   *pTypeText is set to the original text span of the type` |
|        - |  8197 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|        - |  8198 | ` */` |
|      198 |  8199 | `static sxi32 GenStateParsePropertyType(` |
|        - |  8200 | `	ph7_gen_state *pGen,` |
|        - |  8201 | `	sxu32 *pnType,` |
|        - |  8202 | `	SyString *pClass,` |
|        - |  8203 | `	sxi32 *piTypeFlags,` |
|        - |  8204 | `	SyString *pTypeText,` |
|        - |  8205 | `	SySet *pAlts` |
|        5 |  8206 | `){` |
|      203 |  8207 | `	sxi32 iFlags = 0;` |
|        - |  8208 | `	sxi32 rc;` |
|      203 |  8209 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  8210 | `		return SXRET_OK;` |
|        - |  8211 | `	}` |
|        - |  8212 | `	/* If the first token is '$', there's no type */` |
|      203 |  8213 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      ! 0 |  8214 | `		return SXRET_OK;` |
|        - |  8215 | `	}` |
|      203 |  8216 | `	rc = GenStateParseUnionTypeDecl(` |
|       99 |  8217 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|        - |  8218 | `		PH7_CLASS_ATTR_NULLABLE,` |
|        - |  8219 | `		PH7_CLASS_ATTR_UNION,` |
|        - |  8220 | `		/* bAllowVoid */ 0,` |
|      198 |  8221 | `		pGen->pIn->nLine);` |
|      203 |  8222 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8223 | `		return rc;` |
|        - |  8224 | `	}` |
|        - |  8225 | `	/* Verify next token is '$' (start of property name) */` |
|      203 |  8226 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  8227 | `		return SXERR_SYNTAX;` |
|        - |  8228 | `	}` |
|      203 |  8229 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|      203 |  8230 | `	return SXRET_OK;` |
|      104 |  8231 | `}` |
|        - |  8232 |  |
|        - |  8233 | `/*` |
|        - |  8234 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|        - |  8235 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|        - |  8236 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|        - |  8237 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|        - |  8238 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|        - |  8239 | ` * by the type parser itself before reaching here.` |
|        - |  8240 | ` *` |
|        - |  8241 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|        - |  8242 | ` * use in the error message.` |
|        - |  8243 | ` */` |
|      370 |  8244 | `static int GenStateIsDisallowedPropertyAtom(` |
|        - |  8245 | `	sxu32 nType,` |
|        - |  8246 | `	const SyString *pClass,` |
|        - |  8247 | `	const char **pzName,` |
|        - |  8248 | `	sxu32 *pnName)` |
|        5 |  8249 | `{` |
|        - |  8250 | `	const char *z;` |
|        - |  8251 | `	sxu32 n;` |
|      375 |  8252 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|      321 |  8253 | `		return 0;` |
|        - |  8254 | `	}` |
|       59 |  8255 | `	z = pClass->zString;` |
|       59 |  8256 | `	n = pClass->nByte;` |
|       59 |  8257 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|        8 |  8258 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|        - |  8259 | `	}` |
|        - |  8260 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|        - |  8261 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|        - |  8262 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|       52 |  8263 | `	return 0;` |
|      190 |  8264 | `}` |
|        - |  8265 |  |
|        - |  8266 | `/*` |
|        - |  8267 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|        - |  8268 | ` * constant) — the main atom plus any union alternatives — against the` |
|        - |  8269 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|        - |  8270 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|        - |  8271 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|        - |  8272 | ` * type T" vs "Class constant C::X cannot have type T").` |
|        - |  8273 | ` *` |
|        - |  8274 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|        - |  8275 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|        - |  8276 | ` */` |
|      308 |  8277 | `static sxi32 GenStateValidateMemberType(` |
|        - |  8278 | `	ph7_gen_state *pGen,` |
|        - |  8279 | `	ph7_class *pClass,` |
|        - |  8280 | `	const SyString *pMemberName,` |
|        - |  8281 | `	sxu32 nType,` |
|        - |  8282 | `	const SyString *pTypeClass,` |
|        - |  8283 | `	const SyString *pTypeText,` |
|        - |  8284 | `	SySet *pUnionAlts,` |
|        - |  8285 | `	const char *zErrFmt,` |
|        - |  8286 | `	sxu32 nLine)` |
|        5 |  8287 | `{` |
|      313 |  8288 | `	const char *zBad = 0;` |
|      313 |  8289 | `	sxu32 nBad = 0;` |
|        - |  8290 | `	SyString sFallback;` |
|        - |  8291 | `	const SyString *pBad;` |
|        - |  8292 | `	sxi32 rc;` |
|      313 |  8293 | `	int bDisallowed = 0;` |
|      313 |  8294 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|        5 |  8295 | `		bDisallowed = 1;` |
|      311 |  8296 | `	}else if( pUnionAlts ){` |
|        - |  8297 | `		sxu32 i;` |
|       95 |  8298 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|       67 |  8299 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|       67 |  8300 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|        3 |  8301 | `				bDisallowed = 1;` |
|        3 |  8302 | `				break;` |
|        - |  8303 | `			}` |
|       35 |  8304 | `		}` |
|       15 |  8305 | `	}` |
|      313 |  8306 | `	if( !bDisallowed ){` |
|      307 |  8307 | `		return SXRET_OK;` |
|        - |  8308 | `	}` |
|        - |  8309 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|        - |  8310 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|        - |  8311 | `	 * canonical spelling if the type text is unavailable. */` |
|        8 |  8312 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|        8 |  8313 | `		pBad = pTypeText;` |
|        5 |  8314 | `	}else{` |
|      ! 0 |  8315 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|      ! 0 |  8316 | `		pBad = &sFallback;` |
|        - |  8317 | `	}` |
|       11 |  8318 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        3 |  8319 | `		zErrFmt,` |
|        3 |  8320 | `		&pClass->sName,pMemberName,pBad);` |
|        8 |  8321 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  8322 | `		return SXERR_ABORT;` |
|        - |  8323 | `	}` |
|        8 |  8324 | `	return SXERR_SYNTAX;` |
|      159 |  8325 | `}` |
|        - |  8326 | `/*` |
|        - |  8327 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|        - |  8328 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|        - |  8329 | ` * matched as a plain identifier in the class-member modifier position rather` |
|        - |  8330 | ` * than promoted to a lexer keyword.` |
|        - |  8331 | ` */` |
| 10134428 |  8332 | `static int GenStateIsReadonly(SyToken *pTok)` |
|        5 |  8333 | `{` |
| 10175570 |  8334 | `	return (pTok->nType & PH7_TK_ID)` |
|  5108351 |  8335 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 10175565 |  8336 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|        5 |  8337 | `}` |
|   210566 |  8338 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 |  8339 | `{` |
|   210571 |  8340 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  8341 | `	ph7_class_attr *pAttr;` |
|        - |  8342 | `	SyString *pName;` |
|        - |  8343 | `	sxi32 rc;` |
|   210571 |  8344 | `	sxu32 nType = 0;` |
|        - |  8345 | `	SyString sTypeClass;` |
|        - |  8346 | `	SyString sTypeText;` |
|        - |  8347 | `	SySet aUnionAlts;` |
|   210571 |  8348 | `	sxi32 iTypeFlags = 0;` |
|   210571 |  8349 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   210571 |  8350 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   210571 |  8351 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  8352 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|        - |  8353 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|        - |  8354 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   210571 |  8355 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|       21 |  8356 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|        9 |  8357 | `	}` |
|        - |  8358 | `	/* Extract visibility level */` |
|   210571 |  8359 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - |  8360 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   210670 |  8361 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      203 |  8362 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|      203 |  8363 | `		if( rc == SXERR_CORRUPT ){` |
|        - |  8364 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|      ! 0 |  8365 | `			goto Synchronize;` |
|      203 |  8366 | `		}else if( rc == SXERR_SYNTAX ){` |
|      ! 0 |  8367 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8368 | `				"Invalid property type or declaration near '%z'",` |
|      ! 0 |  8369 | `				&pGen->pIn->sData);` |
|      ! 0 |  8370 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8371 | `				return SXERR_ABORT;` |
|        - |  8372 | `			}` |
|      ! 0 |  8373 | `			goto Synchronize;` |
|      203 |  8374 | `		}else if( rc == SXERR_ABORT ){` |
|      ! 0 |  8375 | `			return SXERR_ABORT;` |
|        - |  8376 | `		}` |
|       99 |  8377 | `	}` |
|      ! 0 |  8378 | `loop:` |
|   210575 |  8379 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  8380 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|      ! 0 |  8381 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8382 | `			return SXERR_ABORT;` |
|        - |  8383 | `		}` |
|      ! 0 |  8384 | `		goto Synchronize;` |
|        - |  8385 | `	}` |
|   210575 |  8386 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   210575 |  8387 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        - |  8388 | `		/* Invalid attribute name */` |
|      ! 0 |  8389 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|      ! 0 |  8390 | `		if( rc == SXERR_ABORT ){` |
|        - |  8391 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8392 | `			return SXERR_ABORT;` |
|        - |  8393 | `		}` |
|      ! 0 |  8394 | `		goto Synchronize;` |
|        - |  8395 | `	}` |
|        - |  8396 | `	/* Peek attribute name */` |
|   210575 |  8397 | `	pName = &pGen->pIn->sData;` |
|        - |  8398 | `	/* Advance the stream cursor */` |
|   210575 |  8399 | `	pGen->pIn++;` |
|   210575 |  8400 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|        - |  8401 | `		/* Invalid declaration */` |
|        3 |  8402 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|        3 |  8403 | `		if( rc == SXERR_ABORT ){` |
|        - |  8404 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8405 | `			return SXERR_ABORT;` |
|        - |  8406 | `		}` |
|        3 |  8407 | `		goto Synchronize;` |
|        - |  8408 | `	}` |
|        - |  8409 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|        - |  8410 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   210573 |  8411 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|       41 |  8412 | `		const char *zRoErr = 0;` |
|       41 |  8413 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        3 |  8414 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|       40 |  8415 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        6 |  8416 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|       37 |  8417 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|        6 |  8418 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|        2 |  8419 | `		}` |
|       41 |  8420 | `		if( zRoErr ){` |
|       13 |  8421 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|       13 |  8422 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8423 | `				return SXERR_ABORT;` |
|        - |  8424 | `			}` |
|       13 |  8425 | `			goto Synchronize;` |
|        - |  8426 | `		}` |
|       13 |  8427 | `	}` |
|        - |  8428 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|        - |  8429 | `	 * type atom or any union alternative. void/never are already rejected` |
|        - |  8430 | `	 * by the type parser. */` |
|   210563 |  8431 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      299 |  8432 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|        - |  8433 | `			&sTypeText,` |
|      196 |  8434 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       98 |  8435 | `			"Property %z::$%z cannot have type %z",nLine);` |
|      201 |  8436 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8437 | `			return SXERR_ABORT;` |
|      201 |  8438 | `		}else if( rc != SXRET_OK ){` |
|      ! 0 |  8439 | `			goto Synchronize;` |
|        - |  8440 | `		}` |
|       98 |  8441 | `	}` |
|        - |  8442 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   210563 |  8443 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|        4 |  8444 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  8445 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|        3 |  8446 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8447 | `			return SXERR_ABORT;` |
|        - |  8448 | `		}` |
|        3 |  8449 | `		goto Synchronize;` |
|        - |  8450 | `	}` |
|        - |  8451 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|        - |  8452 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|        - |  8453 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|        - |  8454 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|        - |  8455 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|        - |  8456 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|   210561 |  8457 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|        6 |  8458 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8459 | `			"New expressions are not supported in this context");` |
|        6 |  8460 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8461 | `			return SXERR_ABORT;` |
|        - |  8462 | `		}` |
|        6 |  8463 | `		goto Synchronize;` |
|        - |  8464 | `	}` |
|        - |  8465 | `	/* Allocate a new class attribute */` |
|   210557 |  8466 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   210557 |  8467 | `	if( pAttr ){` |
|   210557 |  8468 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|   210557 |  8469 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  8470 | `			return SXERR_ABORT;` |
|        - |  8471 | `		}` |
|   105276 |  8472 | `	}` |
|   210557 |  8473 | `	if( pAttr == 0 ){` |
|      ! 0 |  8474 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  8475 | `		return SXERR_ABORT;` |
|        - |  8476 | `	}` |
|   210557 |  8477 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      199 |  8478 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       97 |  8479 | `	}` |
|   210557 |  8480 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|        - |  8481 | `		SySet *pInstrContainer;` |
|    86041 |  8482 | `		pGen->pIn++; /*Jump the equal sign */` |
|        - |  8483 | `		/* Swap bytecode container */` |
|    86041 |  8484 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    86041 |  8485 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|        - |  8486 | `		/* Compile attribute value.` |
|        - |  8487 | `		 */` |
|    86041 |  8488 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    86041 |  8489 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 |  8490 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|      ! 0 |  8491 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8492 | `				return SXERR_ABORT;` |
|        - |  8493 | `			}` |
|      ! 0 |  8494 | `		}` |
|        - |  8495 | `		/* Emit the done instruction */` |
|    86041 |  8496 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    86041 |  8497 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    43018 |  8498 | `	}` |
|        - |  8499 | `	/* All done,install the attribute */` |
|   210557 |  8500 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   210557 |  8501 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8502 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8503 | `		return SXERR_ABORT;` |
|        - |  8504 | `	}` |
|   210557 |  8505 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  8506 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|        5 |  8507 | `		pGen->pIn++; /* Jump the comma */` |
|        5 |  8508 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|      ! 0 |  8509 | `			SyToken *pTok = pGen->pIn;` |
|      ! 0 |  8510 | `			if( pTok >= pGen->pEnd ){` |
|      ! 0 |  8511 | `				pTok--;` |
|      ! 0 |  8512 | `			}` |
|      ! 0 |  8513 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  8514 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|      ! 0 |  8515 | `				&pTok->sData,&pClass->sName);` |
|      ! 0 |  8516 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8517 | `				return SXERR_ABORT;` |
|        - |  8518 | `			}` |
|      ! 0 |  8519 | `		}else{` |
|        5 |  8520 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        5 |  8521 | `				goto loop;` |
|        - |  8522 | `			}` |
|        - |  8523 | `		}` |
|      ! 0 |  8524 | `	}` |
|   210553 |  8525 | `	SySetRelease(&aUnionAlts);` |
|   210553 |  8526 | `	return SXRET_OK;` |
|        9 |  8527 | `Synchronize:` |
|        - |  8528 | `	/* Synchronize with the first semi-colon */` |
|       56 |  8529 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       37 |  8530 | `		pGen->pIn++;` |
|        3 |  8531 | `	}` |
|       22 |  8532 | `	SySetRelease(&aUnionAlts);` |
|       22 |  8533 | `	return SXERR_CORRUPT;` |
|   105288 |  8534 | `}` |
|        - |  8535 | `/*` |
|        - |  8536 | ` * Compile a class method.` |
|        - |  8537 | ` *` |
|        - |  8538 | ` * Refer to the official documentation for more information` |
|        - |  8539 | ` * on the powerful extension introduced by the PH7 engine` |
|        - |  8540 | ` * to the OO subsystem such as full type hinting,method` |
|        - |  8541 | ` * overloading and many more.` |
|        - |  8542 | ` */` |
|  1388104 |  8543 | `static sxi32 GenStateCompileClassMethod(` |
|        - |  8544 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  8545 | `	sxi32 iProtection,   /* Visibility level */` |
|        - |  8546 | `	sxi32 iFlags,        /* Configuration flags */` |
|        - |  8547 | `	int doBody,          /* TRUE to process method body */` |
|        - |  8548 | `	ph7_class *pClass    /* Class this method belongs */` |
|        - |  8549 | `	)` |
|        5 |  8550 | `{` |
|  1388109 |  8551 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  1388109 |  8552 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|        - |  8553 | `	ph7_class_method *pMeth;` |
|        - |  8554 | `	sxi32 iFuncFlags;` |
|        - |  8555 | `	SyString *pName;` |
|        - |  8556 | `	SyToken *pEnd;` |
|        - |  8557 | `	sxi32 rc;` |
|        - |  8558 | `	/* Extract visibility level */` |
|  1388109 |  8559 | `	iProtection = GetProtectionLevel(iProtection);` |
|  1388109 |  8560 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  1388109 |  8561 | `	iFuncFlags = 0;` |
|  1388109 |  8562 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  8563 | `		/* Invalid method name */` |
|      ! 0 |  8564 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|      ! 0 |  8565 | `		if( rc == SXERR_ABORT ){` |
|        - |  8566 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8567 | `			return SXERR_ABORT;` |
|        - |  8568 | `		}` |
|      ! 0 |  8569 | `		goto Synchronize;` |
|        - |  8570 | `	}` |
|  1388109 |  8571 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        - |  8572 | `		/* Return by reference,remember that */` |
|      ! 0 |  8573 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|        - |  8574 | `		/* Jump the '&' token */` |
|      ! 0 |  8575 | `		pGen->pIn++;` |
|      ! 0 |  8576 | `	}` |
|  1388109 |  8577 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  8578 | `		/* Invalid method name */` |
|      ! 0 |  8579 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|      ! 0 |  8580 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8581 | `			return SXERR_ABORT;` |
|        - |  8582 | `		}` |
|      ! 0 |  8583 | `		goto Synchronize;` |
|        - |  8584 | `	}` |
|        - |  8585 | `	/* Peek method name */` |
|  1388109 |  8586 | `	pName = &pGen->pIn->sData;` |
|  1388109 |  8587 | `	nLine = pGen->pIn->nLine;` |
|        - |  8588 | `	/* Jump the method name */` |
|  1388109 |  8589 | `	pGen->pIn++;` |
|  1388109 |  8590 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - |  8591 | `		/* Abstract method */` |
|   101051 |  8592 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|      ! 0 |  8593 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8594 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|      ! 0 |  8595 | `				&pClass->sName,pName);` |
|      ! 0 |  8596 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8597 | `				return SXERR_ABORT;` |
|        - |  8598 | `			}` |
|      ! 0 |  8599 | `		}` |
|        - |  8600 | `		/* Assemble method signature only */` |
|   101051 |  8601 | `		doBody = FALSE;` |
|    50523 |  8602 | `	}` |
|  1388109 |  8603 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  8604 | `		/* Syntax error */` |
|      ! 0 |  8605 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|      ! 0 |  8606 | `		if( rc == SXERR_ABORT ){` |
|        - |  8607 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8608 | `			return SXERR_ABORT;` |
|        - |  8609 | `		}` |
|      ! 0 |  8610 | `		goto Synchronize;` |
|        - |  8611 | `	}` |
|        - |  8612 | `	/* Allocate a new class_method instance */` |
|  1388109 |  8613 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  1388109 |  8614 | `	if( pMeth == 0 ){` |
|      ! 0 |  8615 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8616 | `		return SXERR_ABORT;` |
|        - |  8617 | `	}` |
|  1388109 |  8618 | `	pMeth->sFunc.nLine = nKwLine;` |
|  1388109 |  8619 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|  1388109 |  8620 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  8621 | `		return SXERR_ABORT;` |
|        - |  8622 | `	}` |
|        - |  8623 | `	/* Jump the left parenthesis '(' */` |
|  1388109 |  8624 | `	pGen->pIn++;` |
|  1388109 |  8625 | `	pEnd = 0; /* cc warning */` |
|        - |  8626 | `	/* Delimit the method signature */` |
|  1388109 |  8627 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  1388109 |  8628 | `	if( pEnd >= pGen->pEnd ){` |
|        - |  8629 | `		/* Syntax error */` |
|        3 |  8630 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|        3 |  8631 | `		if( rc == SXERR_ABORT ){` |
|        - |  8632 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8633 | `			return SXERR_ABORT;` |
|        - |  8634 | `		}` |
|        3 |  8635 | `		goto Synchronize;` |
|        - |  8636 | `	}` |
|        - |  8637 | `	{` |
|  1388107 |  8638 | `		int bIsCtor = 0;` |
|  1388107 |  8639 | `		int bAbstractCtor = 0;` |
|  1388102 |  8640 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|   810720 |  8641 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  1335572 |  8642 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   105075 |  8643 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        3 |  8644 | `				bAbstractCtor = 1;` |
|        2 |  8645 | `			}else{` |
|   105073 |  8646 | `				bIsCtor = 1;` |
|        - |  8647 | `			}` |
|    52535 |  8648 | `		}` |
|  1388107 |  8649 | `		if( pGen->pIn < pEnd ){` |
|        - |  8650 | `			/* Collect method arguments */` |
|   389049 |  8651 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   389049 |  8652 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8653 | `				return SXERR_ABORT;` |
|        - |  8654 | `			}` |
|   194522 |  8655 | `		}` |
|        - |  8656 | `	}` |
|        - |  8657 | `	/* Point past ')' and parse optional return type ': type' */` |
|  1388107 |  8658 | `	pGen->pIn = &pEnd[1];` |
|        - |  8659 | `	{` |
|  1388107 |  8660 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  1388107 |  8661 | `		if( rcRt == SXERR_ABORT ){` |
|      ! 0 |  8662 | `			return SXERR_ABORT;` |
|  1388107 |  8663 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|      ! 0 |  8664 | `			goto Synchronize;` |
|        - |  8665 | `		}` |
|        - |  8666 | `	}` |
|        - |  8667 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|        - |  8668 | `	 * property init/typecheck is handled by the generic typed-property path` |
|        - |  8669 | `	 * since we mint real ph7_class_attr entries. */` |
|        - |  8670 | `	{` |
|  1388107 |  8671 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|        - |  8672 | `		sxu32 i;` |
|  1971515 |  8673 | `		for( i = 0; i < nArg; i++ ){` |
|   583423 |  8674 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|        - |  8675 | `			ph7_class_attr *pAttr;` |
|   583423 |  8676 | `			sxi32 iAttrFlags = 0;` |
|        - |  8677 | `			int bArgTyped;` |
|   583423 |  8678 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   583343 |  8679 | `				continue;` |
|        - |  8680 | `			}` |
|        - |  8681 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|        - |  8682 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|        - |  8683 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|       57 |  8684 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|       86 |  8685 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|       85 |  8686 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        3 |  8687 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8688 | `					"Cannot declare variadic promoted property");` |
|        3 |  8689 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8690 | `					return SXERR_ABORT;` |
|        - |  8691 | `				}` |
|        3 |  8692 | `				goto Synchronize;` |
|        - |  8693 | `			}` |
|        - |  8694 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|        - |  8695 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|        - |  8696 | `			 * appear as an alternative of a union type. */` |
|       83 |  8697 | `			if( bArgTyped ){` |
|      116 |  8698 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|       74 |  8699 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|       74 |  8700 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|       37 |  8701 | `					"Property %z::$%z cannot have type %z",nLine);` |
|       79 |  8702 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8703 | `					return SXERR_ABORT;` |
|       79 |  8704 | `				}else if( rc != SXRET_OK ){` |
|        6 |  8705 | `					goto Synchronize;` |
|        - |  8706 | `				}` |
|       35 |  8707 | `			}` |
|        - |  8708 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|       79 |  8709 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|        4 |  8710 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  8711 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|        3 |  8712 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8713 | `					return SXERR_ABORT;` |
|        - |  8714 | `				}` |
|        3 |  8715 | `				goto Synchronize;` |
|        - |  8716 | `			}` |
|       77 |  8717 | `			if( bArgTyped ){` |
|       73 |  8718 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|       34 |  8719 | `			}` |
|       77 |  8720 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|        3 |  8721 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|        1 |  8722 | `			}` |
|       77 |  8723 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|        8 |  8724 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|        3 |  8725 | `			}` |
|       77 |  8726 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|        - |  8727 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|        - |  8728 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|       26 |  8729 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        4 |  8730 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  8731 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|        3 |  8732 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  8733 | `						return SXERR_ABORT;` |
|        - |  8734 | `					}` |
|        3 |  8735 | `					goto Synchronize;` |
|        - |  8736 | `				}` |
|       24 |  8737 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       10 |  8738 | `			}` |
|       75 |  8739 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|       75 |  8740 | `			if( pAttr == 0 ){` |
|      ! 0 |  8741 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8742 | `				return SXERR_ABORT;` |
|        - |  8743 | `			}` |
|       75 |  8744 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|       73 |  8745 | `				pAttr->nType = pArg->nType;` |
|       73 |  8746 | `				pAttr->sClass = pArg->sClass;` |
|       73 |  8747 | `				pAttr->sTypeName = pArg->sTypeName;` |
|       73 |  8748 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|        - |  8749 | `					sxu32 k;` |
|       20 |  8750 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|       14 |  8751 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|       14 |  8752 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|        8 |  8753 | `					}` |
|        3 |  8754 | `				}` |
|       34 |  8755 | `			}` |
|       75 |  8756 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|       75 |  8757 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  8758 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8759 | `				return SXERR_ABORT;` |
|        - |  8760 | `			}` |
|       40 |  8761 | `		}` |
|        - |  8762 | `	}` |
|  1388097 |  8763 | `	if( doBody ){` |
|        - |  8764 | `		/* Compile method body */` |
|  1287051 |  8765 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  1287051 |  8766 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8767 | `			return SXERR_ABORT;` |
|        - |  8768 | `		}` |
|        - |  8769 | `		/* The cursor sits just past the body's closing brace */` |
|  1287051 |  8770 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|   643528 |  8771 | `	}else{` |
|        - |  8772 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|   101051 |  8773 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|   101051 |  8774 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|    50523 |  8775 | `		}` |
|        - |  8776 | `		/* Only method signature is allowed */` |
|   101051 |  8777 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|      ! 0 |  8778 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  8779 | `				"Expected ';' after method signature '%z'",pName);` |
|      ! 0 |  8780 | `				if( rc == SXERR_ABORT ){` |
|        - |  8781 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 |  8782 | `					return SXERR_ABORT;` |
|        - |  8783 | `				}` |
|      ! 0 |  8784 | `				return SXERR_CORRUPT;` |
|        - |  8785 | `			}` |
|        - |  8786 | `	}` |
|        - |  8787 | `	/* All done,install the method */` |
|  1388097 |  8788 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  1388097 |  8789 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8790 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8791 | `		return SXERR_ABORT;` |
|        - |  8792 | `	}` |
|  1388097 |  8793 | `	return SXRET_OK;` |
|        6 |  8794 | `Synchronize:` |
|        - |  8795 | `	/* Synchronize with the first semi-colon */` |
|       40 |  8796 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       28 |  8797 | `		pGen->pIn++;` |
|        4 |  8798 | `	}` |
|       16 |  8799 | `	return SXERR_CORRUPT;` |
|   694057 |  8800 | `}` |
|        - |  8801 | `/*` |
|        - |  8802 | ` * Compile an object interface.` |
|        - |  8803 | ` *  According to the PHP language reference manual` |
|        - |  8804 | ` *   Object Interfaces:` |
|        - |  8805 | ` *   Object interfaces allow you to create code which specifies which methods` |
|        - |  8806 | ` *   a class must implement, without having to define how these methods are handled.` |
|        - |  8807 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|        - |  8808 | ` *   class, but without any of the methods having their contents defined.` |
|        - |  8809 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|        - |  8810 | ` */` |
|    46708 |  8811 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|        5 |  8812 | `{` |
|    46713 |  8813 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  8814 | `	ph7_class *pClass,*pBase;` |
|        - |  8815 | `	SyToken *pEnd,*pTmp;` |
|        - |  8816 | `	SyString *pName;` |
|        - |  8817 | `	sxi32 nKwrd;` |
|        - |  8818 | `	sxi32 rc;` |
|        - |  8819 | `	/* Jump the 'interface' keyword */` |
|    46713 |  8820 | `	pGen->pIn++;` |
|        - |  8821 | `	/* Extract interface name */` |
|    46713 |  8822 | `	pName = &pGen->pIn->sData;` |
|        - |  8823 | `	/* Advance the stream cursor */` |
|    46713 |  8824 | `	pGen->pIn++;` |
|        - |  8825 | `	/* Build FQN and obtain a raw class */ {` |
|        - |  8826 | `		SyBlob sFQN;` |
|        - |  8827 | `		SyString sFQNStr;` |
|    46713 |  8828 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    46713 |  8829 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    46713 |  8830 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    46713 |  8831 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    46713 |  8832 | `		SyBlobRelease(&sFQN);` |
|        - |  8833 | `	}` |
|    46713 |  8834 | `	if( pClass == 0 ){` |
|      ! 0 |  8835 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8836 | `		return SXERR_ABORT;` |
|        - |  8837 | `	}` |
|    46713 |  8838 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    46713 |  8839 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  8840 | `		return SXERR_ABORT;` |
|        - |  8841 | `	}` |
|        - |  8842 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|    46713 |  8843 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|        - |  8844 | `	/* Assume no base class is given */` |
|    46713 |  8845 | `	pBase = 0;` |
|    46713 |  8846 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    15551 |  8847 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    15551 |  8848 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|        - |  8849 | `			SyBlob sResolved;` |
|        - |  8850 | `			SyString sBaseName;` |
|        - |  8851 | `			sxu32 nRefLine;` |
|        - |  8852 | `			/* Extract base interface */` |
|    15551 |  8853 | `			pGen->pIn++;` |
|    15551 |  8854 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    15551 |  8855 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    15551 |  8856 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 |  8857 | `				SyBlobRelease(&sResolved);` |
|      ! 0 |  8858 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8859 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|      ! 0 |  8860 | `					pName);` |
|      ! 0 |  8861 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  8862 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8863 | `					return SXERR_ABORT;` |
|        - |  8864 | `				}` |
|      ! 0 |  8865 | `				return SXRET_OK;` |
|        - |  8866 | `			}` |
|    23324 |  8867 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    15546 |  8868 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    15551 |  8869 | `			SyStringInitFromBuf(&sBaseName,` |
|        - |  8870 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - |  8871 | `			/* Only interfaces is allowed */` |
|    15551 |  8872 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 |  8873 | `				pBase = pBase->pNextName;` |
|      ! 0 |  8874 | `			}` |
|    15551 |  8875 | `			if( pBase == 0 ){` |
|      ! 0 |  8876 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - |  8877 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|      ! 0 |  8878 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8879 | `					SyBlobRelease(&sResolved);` |
|      ! 0 |  8880 | `					return SXERR_ABORT;` |
|        - |  8881 | `				}` |
|      ! 0 |  8882 | `			}` |
|    15551 |  8883 | `			SyBlobRelease(&sResolved);` |
|     7773 |  8884 | `		}` |
|     7773 |  8885 | `	}` |
|    46713 |  8886 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - |  8887 | `		/* Syntax error */` |
|      ! 0 |  8888 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|      ! 0 |  8889 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  8890 | `		if( rc == SXERR_ABORT ){` |
|        - |  8891 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8892 | `			return SXERR_ABORT;` |
|        - |  8893 | `		}` |
|      ! 0 |  8894 | `		return SXRET_OK;` |
|        - |  8895 | `	}` |
|    46713 |  8896 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    46713 |  8897 | `	pEnd = 0; /* cc warning */` |
|        - |  8898 | `	/* Delimit the interface body */` |
|    46713 |  8899 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    46713 |  8900 | `	if( pEnd >= pGen->pEnd ){` |
|        - |  8901 | `		/* Syntax error */` |
|      ! 0 |  8902 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|      ! 0 |  8903 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  8904 | `		if( rc == SXERR_ABORT ){` |
|        - |  8905 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8906 | `			return SXERR_ABORT;` |
|        - |  8907 | `		}` |
|      ! 0 |  8908 | `		return SXRET_OK;` |
|        - |  8909 | `	}` |
|        - |  8910 | `	/* The delimiter token is the interface body's closing brace */` |
|    46713 |  8911 | `	pClass->nEndLine = pEnd->nLine;` |
|        - |  8912 | `	/* Swap token stream */` |
|    46713 |  8913 | `	pTmp = pGen->pEnd;` |
|    46713 |  8914 | `	pGen->pEnd = pEnd;` |
|        - |  8915 | `	/* Start the parse process` |
|        - |  8916 | `	 * Note (According to the PHP reference manual):` |
|        - |  8917 | `	 *  Only constants and function signatures(without body) are allowed.` |
|        - |  8918 | `	 *  Only 'public' visibility is allowed.` |
|        - |  8919 | `	 */` |
|    73875 |  8920 | `	for(;;){` |
|        - |  8921 | `		/* Jump leading/trailing semi-colons */` |
|   248797 |  8922 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   101047 |  8923 | `			pGen->pIn++;` |
|        5 |  8924 | `		}` |
|   147755 |  8925 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - |  8926 | `			/* End of interface body */` |
|    46709 |  8927 | `			break;` |
|        - |  8928 | `		}` |
|        - |  8929 | `		/* Bind a directly-preceding docblock to this member */` |
|   101051 |  8930 | `		GenStateSetPendingDoc(&(*pGen));` |
|   101051 |  8931 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 |  8932 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  8933 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|      ! 0 |  8934 | `				&pGen->pIn->sData,pName);` |
|      ! 0 |  8935 | `			if( rc == SXERR_ABORT ){` |
|        - |  8936 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  8937 | `				return SXERR_ABORT;` |
|        - |  8938 | `			}` |
|      ! 0 |  8939 | `			goto done;` |
|        - |  8940 | `		}` |
|        - |  8941 | `		/* Extract the current keyword */` |
|   101051 |  8942 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   101051 |  8943 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        - |  8944 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|        - |  8945 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|        3 |  8946 | `			const char *zKind = "member";` |
|        3 |  8947 | `			SyString *pMemberName = 0;` |
|        3 |  8948 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|        3 |  8949 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|        3 |  8950 | `				if( nNext == PH7_TKWRD_CONST ){` |
|        3 |  8951 | `					zKind = "constant";` |
|        3 |  8952 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|        3 |  8953 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|        2 |  8954 | `					}` |
|        1 |  8955 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|      ! 0 |  8956 | `					zKind = "method";` |
|      ! 0 |  8957 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|      ! 0 |  8958 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|      ! 0 |  8959 | `					}` |
|      ! 0 |  8960 | `				}` |
|        1 |  8961 | `			}` |
|        3 |  8962 | `			if( pMemberName ){` |
|        4 |  8963 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        1 |  8964 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|        2 |  8965 | `			}else{` |
|      ! 0 |  8966 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  8967 | `					"Access type for interface %s must be public",zKind);` |
|        - |  8968 | `			}` |
|        3 |  8969 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8970 | `				return SXERR_ABORT;` |
|        - |  8971 | `			}` |
|        3 |  8972 | `			goto done;` |
|        - |  8973 | `		}` |
|   101049 |  8974 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|      ! 0 |  8975 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  8976 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|      ! 0 |  8977 | `			if( rc == SXERR_ABORT ){` |
|        - |  8978 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  8979 | `				return SXERR_ABORT;` |
|        - |  8980 | `			}` |
|      ! 0 |  8981 | `			goto done;` |
|        - |  8982 | `		}` |
|   101049 |  8983 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|        - |  8984 | `			/* Advance the stream cursor */` |
|   101031 |  8985 | `			pGen->pIn++;` |
|   101031 |  8986 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 |  8987 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  8988 | `					"Expecting method signature inside interface '%z'",pName);` |
|      ! 0 |  8989 | `				if( rc == SXERR_ABORT ){` |
|        - |  8990 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 |  8991 | `					return SXERR_ABORT;` |
|        - |  8992 | `				}` |
|      ! 0 |  8993 | `				goto done;` |
|        - |  8994 | `			}` |
|   101031 |  8995 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   101031 |  8996 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|      ! 0 |  8997 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  8998 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|      ! 0 |  8999 | `				if( rc == SXERR_ABORT ){` |
|        - |  9000 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 |  9001 | `					return SXERR_ABORT;` |
|        - |  9002 | `				}` |
|      ! 0 |  9003 | `				goto done;` |
|        - |  9004 | `			}` |
|    50513 |  9005 | `		}` |
|   101049 |  9006 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|        - |  9007 | `			/* Parse constant */` |
|       16 |  9008 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       16 |  9009 | `			if( rc != SXRET_OK ){` |
|        3 |  9010 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9011 | `					return SXERR_ABORT;` |
|        - |  9012 | `				}` |
|        3 |  9013 | `				goto done;` |
|        - |  9014 | `			}` |
|        7 |  9015 | `		}else{` |
|   101035 |  9016 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   101035 |  9017 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|        - |  9018 | `				/* Static method,record that */` |
|    11657 |  9019 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|        - |  9020 | `				/* Advance the stream cursor */` |
|    11657 |  9021 | `				pGen->pIn++;` |
|    11652 |  9022 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    11657 |  9023 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 |  9024 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9025 | `							"Expecting method signature inside interface '%z'",pName);` |
|      ! 0 |  9026 | `						if( rc == SXERR_ABORT ){` |
|        - |  9027 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 |  9028 | `							return SXERR_ABORT;` |
|        - |  9029 | `						}` |
|      ! 0 |  9030 | `						goto done;` |
|        - |  9031 | `				}` |
|     5826 |  9032 | `			}` |
|        - |  9033 | `			/* Process method signature (no body for interface methods) */` |
|   101035 |  9034 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   101035 |  9035 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9036 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9037 | `					return SXERR_ABORT;` |
|        - |  9038 | `				}` |
|      ! 0 |  9039 | `				goto done;` |
|        - |  9040 | `			}` |
|        - |  9041 | `		}` |
|        5 |  9042 | `	}` |
|        - |  9043 | `	/* Install the interface */` |
|    46709 |  9044 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    46709 |  9045 | `	if( rc == SXRET_OK && pBase ){` |
|        - |  9046 | `		/* Inherit from the base interface */` |
|    15551 |  9047 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|     7773 |  9048 | `	}` |
|    46709 |  9049 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  9050 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9051 | `		return SXERR_ABORT;` |
|        - |  9052 | `	}` |
|    23352 |  9053 | `done:` |
|        - |  9054 | `	/* Point beyond the interface body */` |
|    46713 |  9055 | `	pGen->pIn  = &pEnd[1];` |
|    46713 |  9056 | `	pGen->pEnd = pTmp;` |
|    46713 |  9057 | `	return PH7_OK;` |
|    23359 |  9058 | `}` |
|        - |  9059 | `/*` |
|        - |  9060 | ` * Compile a user-defined class.` |
|        - |  9061 | ` * According to the PHP language reference manual` |
|        - |  9062 | ` *  class` |
|        - |  9063 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|        - |  9064 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|        - |  9065 | ` *  of the properties and methods belonging to the class.` |
|        - |  9066 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|        - |  9067 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|        - |  9068 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|        - |  9069 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|        - |  9070 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|        - |  9071 | ` *  (called "methods").` |
|        - |  9072 | ` */` |
|        - |  9073 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|        - |  9074 | `typedef struct TraitUseEntry TraitUseEntry;` |
|        - |  9075 | `struct TraitUseEntry {` |
|        - |  9076 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|        - |  9077 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|        - |  9078 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|        - |  9079 | `};` |
|        - |  9080 | `/*` |
|        - |  9081 | ` * Validate that methods implementing interface contracts have compatible` |
|        - |  9082 | ` * signatures: public visibility and at least as many parameters as declared.` |
|        - |  9083 | ` */` |
|   215198 |  9084 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 |  9085 | `{` |
|        - |  9086 | `	ph7_class **apIface;` |
|        - |  9087 | `	sxu32 nIface,i;` |
|        - |  9088 | `	sxi32 rc;` |
|   215203 |  9089 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      ! 0 |  9090 | `		return SXRET_OK;` |
|        - |  9091 | `	}` |
|   215203 |  9092 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   215203 |  9093 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   429149 |  9094 | `	for(i = 0; i < nIface; i++){` |
|   213951 |  9095 | `		ph7_class *pIface = apIface[i];` |
|        - |  9096 | `		SyHashEntry *pEntry;` |
|   213951 |  9097 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   498055 |  9098 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   284109 |  9099 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |  9100 | `			ph7_class_method *pImplMeth;` |
|   284109 |  9101 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|        - |  9102 | `			/* Find the implementing method in the class */` |
|   284109 |  9103 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   284109 |  9104 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       18 |  9105 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|        - |  9106 | `			}` |
|        - |  9107 | `			/* Check visibility: interface methods must be implemented as public */` |
|   284095 |  9108 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        4 |  9109 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|        - |  9110 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|        1 |  9111 | `					&pClass->sName,pMName,&pIface->sName);` |
|        3 |  9112 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9113 | `					return SXERR_ABORT;` |
|        - |  9114 | `				}` |
|        1 |  9115 | `			}` |
|        - |  9116 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|        - |  9117 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|        - |  9118 | `			 */` |
|        - |  9119 | `			{` |
|   284095 |  9120 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   284095 |  9121 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   284095 |  9122 | `				int sigError = 0;` |
|   284095 |  9123 | `				if( nImplArgs < nIfaceArgs ){` |
|        3 |  9124 | `					sigError = 1;` |
|   284094 |  9125 | `				}else if( nImplArgs > nIfaceArgs ){` |
|        - |  9126 | `					/* Extra parameters must all have default values */` |
|        6 |  9127 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|        - |  9128 | `					sxu32 k;` |
|        8 |  9129 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|        6 |  9130 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|        3 |  9131 | `							sigError = 1;` |
|        3 |  9132 | `							break;` |
|        - |  9133 | `						}` |
|        2 |  9134 | `					}` |
|        2 |  9135 | `				}` |
|   284095 |  9136 | `				if( sigError ){` |
|        - |  9137 | `					SyBlob sImplSig, sIfaceSig;` |
|        - |  9138 | `					ph7_vm_func_arg *aArgs;` |
|        - |  9139 | `					sxu32 j;` |
|        6 |  9140 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|        6 |  9141 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|        - |  9142 | `					/* Build implementing method signature */` |
|        6 |  9143 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       12 |  9144 | `					for(j = 0; j < nImplArgs; j++){` |
|        8 |  9145 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|        8 |  9146 | `						SyBlobAppend(&sImplSig,"$",1);` |
|        8 |  9147 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|        5 |  9148 | `					}` |
|        - |  9149 | `					/* Build interface method signature */` |
|        6 |  9150 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|       12 |  9151 | `					for(j = 0; j < nIfaceArgs; j++){` |
|        8 |  9152 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|        8 |  9153 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|        8 |  9154 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|        5 |  9155 | `					}` |
|        8 |  9156 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|        - |  9157 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|        2 |  9158 | `						&pClass->sName,pMName,` |
|        4 |  9159 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|        2 |  9160 | `						&pIface->sName,pMName,` |
|        4 |  9161 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|        6 |  9162 | `					SyBlobRelease(&sImplSig);` |
|        6 |  9163 | `					SyBlobRelease(&sIfaceSig);` |
|        6 |  9164 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9165 | `						return SXERR_ABORT;` |
|        - |  9166 | `					}` |
|        2 |  9167 | `				}` |
|        - |  9168 | `			}` |
|        5 |  9169 | `		}` |
|   106978 |  9170 | `	}` |
|   215203 |  9171 | `	return SXRET_OK;` |
|   107604 |  9172 | `}` |
|        - |  9173 | `/*` |
|        - |  9174 | ` * Check that a concrete class has no remaining abstract methods.` |
|        - |  9175 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|        - |  9176 | ` */` |
|   215198 |  9177 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 |  9178 | `{` |
|        - |  9179 | `	ph7_class_method *pMeth;` |
|        - |  9180 | `	SyHashEntry *pEntry;` |
|        - |  9181 | `	sxu32 nAbstract;` |
|        - |  9182 | `	SyBlob sMsg;` |
|        - |  9183 | `	sxi32 rc;` |
|        - |  9184 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   215203 |  9185 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     7811 |  9186 | `		return SXRET_OK;` |
|        - |  9187 | `	}` |
|        - |  9188 | `	/* Count abstract methods */` |
|   207397 |  9189 | `	nAbstract = 0;` |
|   207397 |  9190 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  3068153 |  9191 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  2860761 |  9192 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  2860761 |  9193 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       20 |  9194 | `			nAbstract++;` |
|        8 |  9195 | `		}` |
|        5 |  9196 | `	}` |
|   207397 |  9197 | `	if( nAbstract == 0 ){` |
|   207383 |  9198 | `		return SXRET_OK;` |
|        - |  9199 | `	}` |
|        - |  9200 | `	/* Build the error message listing all abstract methods with origins */` |
|       18 |  9201 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       18 |  9202 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|        - |  9203 | `		"be declared abstract or implement the remaining method%s (",` |
|        7 |  9204 | `		&pClass->sName,nAbstract,` |
|        7 |  9205 | `		(nAbstract > 1 ? "s" : ""),` |
|        7 |  9206 | `		(nAbstract > 1 ? "s" : ""));` |
|        - |  9207 | `	/* Second pass: list methods with origins */` |
|        - |  9208 | `	{` |
|       18 |  9209 | `		sxu32 nListed = 0;` |
|       18 |  9210 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|       36 |  9211 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|       22 |  9212 | `			ph7_class *pOrigin = 0;` |
|        - |  9213 | `			SyString *pMName;` |
|       22 |  9214 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|       22 |  9215 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|        3 |  9216 | `				continue;` |
|        - |  9217 | `			}` |
|       20 |  9218 | `			pMName = &pMeth->sFunc.sName;` |
|       20 |  9219 | `			if( nListed > 0 ){` |
|        3 |  9220 | `				SyBlobAppend(&sMsg,", ",2);` |
|        1 |  9221 | `			}` |
|        - |  9222 | `			/* Find the origin of this abstract method.` |
|        - |  9223 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|        - |  9224 | `			 * inheritance chains) take precedence for interface-declared` |
|        - |  9225 | `			 * methods. Abstract class methods only win when the class` |
|        - |  9226 | `			 * itself declared the abstract method (not inherited from` |
|        - |  9227 | `			 * an interface). Trait methods are adopted into the using` |
|        - |  9228 | `			 * class's namespace.` |
|        - |  9229 | `			 */` |
|        - |  9230 | `			{` |
|        - |  9231 | `				ph7_class **apIface;` |
|        - |  9232 | `				ph7_class **apTrait;` |
|        - |  9233 | `				ph7_class *pWalk;` |
|        - |  9234 | `				sxu32 i;` |
|        - |  9235 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|        - |  9236 | `				 * (one that was written in the class body, not inherited from an` |
|        - |  9237 | `				 * interface). PHP attributes origin to the declaring class.` |
|        - |  9238 | `				 */` |
|       20 |  9239 | `				if( pClass->pBase ){` |
|       11 |  9240 | `					pWalk = pClass->pBase;` |
|       19 |  9241 | `					while( pWalk ){` |
|        - |  9242 | `						ph7_class_method *pParentMeth;` |
|       13 |  9243 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|       13 |  9244 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        - |  9245 | `							/* Exclude methods that came from an interface anywhere` |
|        - |  9246 | `							 * in this class's ancestor chain.` |
|        - |  9247 | `							 */` |
|       13 |  9248 | `							int fromIface = 0;` |
|       13 |  9249 | `							ph7_class *pAnc = pWalk;` |
|       17 |  9250 | `							while( pAnc ){` |
|        - |  9251 | `								ph7_class **apPI;` |
|        - |  9252 | `								sxu32 j;` |
|       15 |  9253 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|       15 |  9254 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       10 |  9255 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       10 |  9256 | `										fromIface = 1;` |
|       10 |  9257 | `										break;` |
|        - |  9258 | `									}` |
|      ! 0 |  9259 | `								}` |
|       15 |  9260 | `								if( fromIface ) break;` |
|        6 |  9261 | `								pAnc = pAnc->pBase;` |
|        2 |  9262 | `							}` |
|       13 |  9263 | `							if( !fromIface ){` |
|        3 |  9264 | `								pOrigin = pWalk;` |
|        3 |  9265 | `								break;` |
|        - |  9266 | `							}` |
|        4 |  9267 | `						}` |
|       10 |  9268 | `						pWalk = pWalk->pBase;` |
|        2 |  9269 | `					}` |
|        4 |  9270 | `				}` |
|        - |  9271 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|        - |  9272 | `				 * each interface's own parent chain for the deepest origin.` |
|        - |  9273 | `				 */` |
|       20 |  9274 | `				if( !pOrigin ){` |
|       18 |  9275 | `					pWalk = pClass;` |
|       40 |  9276 | `					while( pWalk && !pOrigin ){` |
|       26 |  9277 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|       26 |  9278 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|       16 |  9279 | `							ph7_class *pIface = apIface[i];` |
|       16 |  9280 | `							ph7_class *pDeepest = 0;` |
|       28 |  9281 | `							while( pIface ){` |
|       16 |  9282 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|       16 |  9283 | `									pDeepest = pIface;` |
|        6 |  9284 | `								}` |
|       16 |  9285 | `								pIface = pIface->pBase;` |
|        4 |  9286 | `							}` |
|       16 |  9287 | `							if( pDeepest ){` |
|       16 |  9288 | `								pOrigin = pDeepest;` |
|       16 |  9289 | `								break;` |
|        - |  9290 | `							}` |
|      ! 0 |  9291 | `						}` |
|       26 |  9292 | `						pWalk = pWalk->pBase;` |
|        4 |  9293 | `					}` |
|        7 |  9294 | `				}` |
|        - |  9295 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|       20 |  9296 | `				if( !pOrigin ){` |
|        3 |  9297 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|        3 |  9298 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|        3 |  9299 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|        3 |  9300 | `							pOrigin = pClass;` |
|        3 |  9301 | `							break;` |
|        - |  9302 | `						}` |
|      ! 0 |  9303 | `					}` |
|        1 |  9304 | `				}` |
|        - |  9305 | `			}` |
|       20 |  9306 | `			if( pOrigin ){` |
|       20 |  9307 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|       12 |  9308 | `			}else{` |
|        - |  9309 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|      ! 0 |  9310 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|        - |  9311 | `			}` |
|       20 |  9312 | `			nListed++;` |
|        4 |  9313 | `		}` |
|        - |  9314 | `	}` |
|       18 |  9315 | `	SyBlobAppend(&sMsg,")",1);` |
|       25 |  9316 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|       14 |  9317 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|       18 |  9318 | `	SyBlobRelease(&sMsg);` |
|       18 |  9319 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  9320 | `		return SXERR_ABORT;` |
|        - |  9321 | `	}` |
|       18 |  9322 | `	return SXRET_OK;` |
|   107604 |  9323 | `}` |
|        - |  9324 | `/*` |
|        - |  9325 | ` * Parse a class/interface name reference from the current token stream.` |
|        - |  9326 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|        - |  9327 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|        - |  9328 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|        - |  9329 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|        - |  9330 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|        - |  9331 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|        - |  9332 | ` */` |
|   192140 |  9333 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|        5 |  9334 | `{` |
|   192145 |  9335 | `	int isAbsolute = 0;` |
|   192145 |  9336 | `	SyToken *pStart = pGen->pIn;` |
|        - |  9337 | `	SyBlob sName;` |
|   192145 |  9338 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|     4473 |  9339 | `		isAbsolute = 1;` |
|     4473 |  9340 | `		pGen->pIn++;` |
|     2234 |  9341 | `	}` |
|   192145 |  9342 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        8 |  9343 | `		pGen->pIn = pStart;` |
|        8 |  9344 | `		return SXERR_INVALID;` |
|        - |  9345 | `	}` |
|   192139 |  9346 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   192139 |  9347 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   192139 |  9348 | `	pGen->pIn++;` |
|   288222 |  9349 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|    96093 |  9350 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       16 |  9351 | `		SyBlobAppend(&sName,"\\",1);` |
|       16 |  9352 | `		pGen->pIn++;` |
|       16 |  9353 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       16 |  9354 | `		pGen->pIn++;` |
|        2 |  9355 | `	}` |
|   192139 |  9356 | `	if( isAbsolute ){` |
|     4471 |  9357 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|     2238 |  9358 | `	}else{` |
|        - |  9359 | `		SyString sRaw;` |
|   187673 |  9360 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   187673 |  9361 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|        - |  9362 | `	}` |
|   192139 |  9363 | `	SyBlobRelease(&sName);` |
|   192139 |  9364 | `	return SXRET_OK;` |
|    96075 |  9365 | `}` |
|        - |  9366 | `/*` |
|        - |  9367 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|        - |  9368 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|        - |  9369 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|        - |  9370 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|        - |  9371 | ` * either direction cannot run unbounded.` |
|        - |  9372 | ` */` |
|        - |  9373 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    46804 |  9374 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|        5 |  9375 | `{` |
|        - |  9376 | `	ph7_class **apParent;` |
|        - |  9377 | `	sxu32 n;` |
|   120839 |  9378 | `	while( pInterface ){` |
|    81813 |  9379 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|      ! 0 |  9380 | `			return FALSE;` |
|        - |  9381 | `		}` |
|   101252 |  9382 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    38878 |  9383 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|     7783 |  9384 | `			return TRUE;` |
|        - |  9385 | `		}` |
|    74035 |  9386 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    74035 |  9387 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|      ! 0 |  9388 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|      ! 0 |  9389 | `				return TRUE;` |
|        - |  9390 | `			}` |
|      ! 0 |  9391 | `		}` |
|    74035 |  9392 | `		pInterface = pInterface->pBase;` |
|    74035 |  9393 | `		iDepth++;` |
|        5 |  9394 | `	}` |
|    39031 |  9395 | `	return FALSE;` |
|    23407 |  9396 | `}` |
|    46804 |  9397 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|        5 |  9398 | `{` |
|    46809 |  9399 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|        5 |  9400 | `}` |
|        - |  9401 | `/*` |
|        - |  9402 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|        - |  9403 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|        - |  9404 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|        - |  9405 | ` */` |
|     7778 |  9406 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|        5 |  9407 | `{` |
|     7787 |  9408 | `	while( pBase ){` |
|       10 |  9409 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|        2 |  9410 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|        3 |  9411 | `			return TRUE;` |
|        - |  9412 | `		}` |
|       10 |  9413 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|        6 |  9414 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|        3 |  9415 | `			return TRUE;` |
|        - |  9416 | `		}` |
|        5 |  9417 | `		pBase = pBase->pBase;` |
|        1 |  9418 | `	}` |
|     7779 |  9419 | `	return FALSE;` |
|     3894 |  9420 | `}` |
|        - |  9421 | `/*` |
|        - |  9422 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|        - |  9423 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|        - |  9424 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|        - |  9425 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|        - |  9426 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|        - |  9427 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|        - |  9428 | ` * pClass->aEnumCases for cases().` |
|        - |  9429 | ` */` |
|       42 |  9430 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 |  9431 | `{` |
|       47 |  9432 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  9433 | `	SySet *pInstrContainer;` |
|        - |  9434 | `	ph7_class_attr *pCase;` |
|        - |  9435 | `	SyString *pName;` |
|        - |  9436 | `	sxi32 rc;` |
|       47 |  9437 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|       47 |  9438 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  9439 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  9440 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|      ! 0 |  9441 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  9442 | `			return SXERR_ABORT;` |
|        - |  9443 | `		}` |
|      ! 0 |  9444 | `		goto Synchronize;` |
|        - |  9445 | `	}` |
|       47 |  9446 | `	pName = &pGen->pIn->sData;` |
|        - |  9447 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|       47 |  9448 | `	if( SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte) != 0 ){` |
|      ! 0 |  9449 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9450 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|      ! 0 |  9451 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  9452 | `			return SXERR_ABORT;` |
|        - |  9453 | `		}` |
|      ! 0 |  9454 | `		goto Synchronize;` |
|        - |  9455 | `	}` |
|       47 |  9456 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - |  9457 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|       47 |  9458 | `	if( pCase == 0 ){` |
|      ! 0 |  9459 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9460 | `		return SXERR_ABORT;` |
|        - |  9461 | `	}` |
|       47 |  9462 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|       47 |  9463 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  9464 | `		return SXERR_ABORT;` |
|        - |  9465 | `	}` |
|       47 |  9466 | `	pGen->pIn++; /* Jump the case name */` |
|       47 |  9467 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|       31 |  9468 | `		if( pClass->nEnumBacking == 0 ){` |
|        8 |  9469 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        2 |  9470 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|        6 |  9471 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9472 | `				return SXERR_ABORT;` |
|        - |  9473 | `			}` |
|        6 |  9474 | `			goto Synchronize;` |
|        - |  9475 | `		}` |
|       25 |  9476 | `		pGen->pIn++; /* Jump the equal sign */` |
|        - |  9477 | `		/* Compile the backing value expression into the case's own container` |
|        - |  9478 | `		 * (same technique as class constants). */` |
|       25 |  9479 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       25 |  9480 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|       25 |  9481 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|       25 |  9482 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 |  9483 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  9484 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|      ! 0 |  9485 | `		}` |
|       25 |  9486 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|       25 |  9487 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       25 |  9488 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  9489 | `			return SXERR_ABORT;` |
|        - |  9490 | `		}` |
|       13 |  9491 | `	}else{` |
|       17 |  9492 | `		if( pClass->nEnumBacking != 0 ){` |
|      ! 0 |  9493 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  9494 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|      ! 0 |  9495 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9496 | `				return SXERR_ABORT;` |
|        - |  9497 | `			}` |
|      ! 0 |  9498 | `			goto Synchronize;` |
|        - |  9499 | `		}` |
|        - |  9500 | `	}` |
|       41 |  9501 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|       41 |  9502 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  9503 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9504 | `		return SXERR_ABORT;` |
|        - |  9505 | `	}` |
|       41 |  9506 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|       41 |  9507 | `	return SXRET_OK;` |
|        2 |  9508 | `Synchronize:` |
|        - |  9509 | `	/* Synchronize with the first semi-colon */` |
|       14 |  9510 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|       10 |  9511 | `		pGen->pIn++;` |
|        2 |  9512 | `	}` |
|        6 |  9513 | `	return SXERR_CORRUPT;` |
|       26 |  9514 | `}` |
|        - |  9515 | `/*` |
|        - |  9516 | ` * Synthesize the enum interface methods (PHP 8.1): cases() for every enum,` |
|        - |  9517 | ` * plus from()/tryFrom() for backed enums. Each is an ordinary public static` |
|        - |  9518 | ` * method whose body forwards to a __phl_enum_* engine thunk (vm.c) with the` |
|        - |  9519 | ` * enum's FQN embedded as a literal — the same forwarder pattern the` |
|        - |  9520 | ` * Generator/Fiber/Reflection builtins use. The source buffer is owned by the` |
|        - |  9521 | ` * VM allocator and never freed: tokens (method and parameter names) keep` |
|        - |  9522 | ` * pointers into it (see the constructor-promotion precedent above).` |
|        - |  9523 | ` */` |
|       24 |  9524 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|        3 |  9525 | `{` |
|        - |  9526 | `	SyToken *pSaveIn,*pSaveEnd;` |
|        - |  9527 | `	const char *zBack;` |
|        - |  9528 | `	SySet sToken;` |
|        - |  9529 | `	char *zSrc;` |
|        - |  9530 | `	sxu32 nSrc,nMax;` |
|       27 |  9531 | `	sxi32 rc = SXRET_OK;` |
|       27 |  9532 | `	nMax = 3*(sxu32)sizeof("function tryFrom(string $value){return __phl_enum_tryfrom('',$value);}")` |
|       24 |  9533 | `		+ 3*SyStringLength(&pClass->sName) + 64;` |
|       27 |  9534 | `	zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nMax);` |
|       27 |  9535 | `	if( zSrc == 0 ){` |
|      ! 0 |  9536 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9537 | `		return SXERR_ABORT;` |
|        - |  9538 | `	}` |
|       27 |  9539 | `	zBack = (pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string";` |
|       27 |  9540 | `	if( pClass->nEnumBacking != 0 ){` |
|       19 |  9541 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|        - |  9542 | `			"function cases(){return __phl_enum_cases('%z');}"` |
|        - |  9543 | `			"function from(%s $value){return __phl_enum_from('%z',$value);}"` |
|        - |  9544 | `			"function tryFrom(%s $value){return __phl_enum_tryfrom('%z',$value);}",` |
|        6 |  9545 | `			&pClass->sName,zBack,&pClass->sName,zBack,&pClass->sName);` |
|        7 |  9546 | `	}else{` |
|       21 |  9547 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|        6 |  9548 | `			"function cases(){return __phl_enum_cases('%z');}",&pClass->sName);` |
|        - |  9549 | `	}` |
|       27 |  9550 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       27 |  9551 | `	PH7_TokenizePHP(zSrc,nSrc,pClass->nLine,&sToken,0);` |
|       27 |  9552 | `	pSaveIn = pGen->pIn;` |
|       27 |  9553 | `	pSaveEnd = pGen->pEnd;` |
|       27 |  9554 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|       27 |  9555 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       75 |  9556 | `	while( pGen->pIn < pGen->pEnd && rc != SXERR_ABORT ){` |
|       51 |  9557 | `		rc = GenStateCompileClassMethod(&(*pGen),PH7_TKWRD_PUBLIC,PH7_CLASS_ATTR_STATIC,TRUE,pClass);` |
|        3 |  9558 | `	}` |
|       27 |  9559 | `	pGen->pIn = pSaveIn;` |
|       27 |  9560 | `	pGen->pEnd = pSaveEnd;` |
|       27 |  9561 | `	SySetRelease(&sToken);` |
|       27 |  9562 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXRET_OK;` |
|       15 |  9563 | `}` |
|        - |  9564 | `/*` |
|        - |  9565 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|        - |  9566 | ` * __call/__callStatic/__invoke stay allowed).` |
|        - |  9567 | ` */` |
|        - |  9568 | `static const char *azEnumBannedMagic[] = {` |
|        - |  9569 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|        - |  9570 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|        - |  9571 | `};` |
|        - |  9572 | `/*` |
|        - |  9573 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|        - |  9574 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|        - |  9575 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|        - |  9576 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|        - |  9577 | ` * and before the class is installed.` |
|        - |  9578 | ` */` |
|       24 |  9579 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|        3 |  9580 | `{` |
|        - |  9581 | `	SyHashEntry *pEntry;` |
|        - |  9582 | `	sxi32 rc;` |
|        - |  9583 | `	sxu32 n;` |
|        - |  9584 | `	/* php: "Enum %s cannot include properties" */` |
|       27 |  9585 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|       69 |  9586 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|       47 |  9587 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       47 |  9588 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        3 |  9589 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|        1 |  9590 | `				"Enum %z cannot include properties",&pClass->sName);` |
|        3 |  9591 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9592 | `				return SXERR_ABORT;` |
|        - |  9593 | `			}` |
|        3 |  9594 | `			break;` |
|        - |  9595 | `		}` |
|        2 |  9596 | `	}` |
|        - |  9597 | `	/* php: "Enum %s cannot include magic method %s" */` |
|      339 |  9598 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|      468 |  9599 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|      315 |  9600 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|      ! 0 |  9601 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  9602 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|      ! 0 |  9603 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9604 | `				return SXERR_ABORT;` |
|        - |  9605 | `			}` |
|      ! 0 |  9606 | `		}` |
|      159 |  9607 | `	}` |
|        - |  9608 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|        - |  9609 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|        - |  9610 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|        - |  9611 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|        - |  9612 | `	{` |
|        - |  9613 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|        - |  9614 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|        - |  9615 | `		ph7_class_attr *pAttr;` |
|       27 |  9616 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - |  9617 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|       27 |  9618 | `		if( pAttr == 0 ){` |
|      ! 0 |  9619 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9620 | `			return SXERR_ABORT;` |
|        - |  9621 | `		}` |
|       27 |  9622 | `		pAttr->nType = MEMOBJ_STRING;` |
|       27 |  9623 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|       27 |  9624 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|       27 |  9625 | `		if( pClass->nEnumBacking != 0 ){` |
|       13 |  9626 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - |  9627 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|       13 |  9628 | `			if( pAttr == 0 ){` |
|      ! 0 |  9629 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9630 | `				return SXERR_ABORT;` |
|        - |  9631 | `			}` |
|       13 |  9632 | `			pAttr->nType = pClass->nEnumBacking;` |
|       13 |  9633 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|        7 |  9634 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|        4 |  9635 | `			}else{` |
|        7 |  9636 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|        - |  9637 | `			}` |
|       13 |  9638 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|        6 |  9639 | `		}` |
|        - |  9640 | `	}` |
|       27 |  9641 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|       15 |  9642 | `}` |
|        - |  9643 | `/*` |
|        - |  9644 | ` * Compile a class declaration, named or anonymous.` |
|        - |  9645 | ` *` |
|        - |  9646 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|        - |  9647 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|        - |  9648 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|        - |  9649 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|        - |  9650 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|        - |  9651 | ` * implements, body, install) is shared by both paths.` |
|        - |  9652 | ` */` |
|   215242 |  9653 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|        - |  9654 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|        5 |  9655 | `{` |
|   215247 |  9656 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  9657 | `	ph7_class *pClass,*pBase;` |
|        - |  9658 | `	SyToken *pEnd,*pTmp;` |
|        - |  9659 | `	sxi32 iProtection;` |
|        - |  9660 | `	SySet aInterfaces;` |
|        - |  9661 | `	SySet aUseEntries;` |
|        - |  9662 | `	sxi32 iAttrflags;` |
|        - |  9663 | `	SyString *pName;` |
|        - |  9664 | `	sxi32 nKwrd;` |
|        - |  9665 | `	sxi32 rc;` |
|        - |  9666 | `	/* Jump the 'class' keyword */` |
|   215247 |  9667 | `	pGen->pIn++;` |
|   215247 |  9668 | `	if( pAnonName ){` |
|        - |  9669 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|        - |  9670 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|        - |  9671 | `		 * then use the synthesized name. */` |
|       32 |  9672 | `		*ppArgStart = *ppArgEnd = 0;` |
|       32 |  9673 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        7 |  9674 | `			pGen->pIn++; /* Jump '(' */` |
|        7 |  9675 | `			*ppArgStart = pGen->pIn;` |
|       10 |  9676 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|        3 |  9677 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|        7 |  9678 | `			pGen->pIn = *ppArgEnd;` |
|        7 |  9679 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|        3 |  9680 | `		}` |
|       32 |  9681 | `		pName = pAnonName;` |
|       32 |  9682 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|       18 |  9683 | `	}else{` |
|   215219 |  9684 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|        - |  9685 | `			/* Syntax error */` |
|      ! 0 |  9686 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|      ! 0 |  9687 | `			if( rc == SXERR_ABORT ){` |
|        - |  9688 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  9689 | `				return SXERR_ABORT;` |
|        - |  9690 | `			}` |
|        - |  9691 | `			/* Synchronize with the first semi-colon or curly braces */` |
|      ! 0 |  9692 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|      ! 0 |  9693 | `				pGen->pIn++;` |
|      ! 0 |  9694 | `			}` |
|      ! 0 |  9695 | `			return SXRET_OK;` |
|        - |  9696 | `		}` |
|        - |  9697 | `		/* Extract class name */` |
|   215219 |  9698 | `		pName = &pGen->pIn->sData;` |
|        - |  9699 | `		/* Advance the stream cursor */` |
|   215219 |  9700 | `		pGen->pIn++;` |
|        - |  9701 | `		/* Build FQN and obtain a raw class */ {` |
|        - |  9702 | `			SyBlob sFQN;` |
|        - |  9703 | `			SyString sFQNStr;` |
|   215219 |  9704 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   215219 |  9705 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|   215219 |  9706 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   215219 |  9707 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   215219 |  9708 | `			SyBlobRelease(&sFQN);` |
|        - |  9709 | `		}` |
|        - |  9710 | `	}` |
|   215247 |  9711 | `	if( pClass == 0 ){` |
|      ! 0 |  9712 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9713 | `		return SXERR_ABORT;` |
|        - |  9714 | `	}` |
|   215242 |  9715 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|       33 |  9716 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|        - |  9717 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|       16 |  9718 | `		pGen->pIn++; /* Jump ':' */` |
|       14 |  9719 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       16 |  9720 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|        7 |  9721 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|        7 |  9722 | `			pGen->pIn++;` |
|       12 |  9723 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       10 |  9724 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|        7 |  9725 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|        7 |  9726 | `			pGen->pIn++;` |
|        4 |  9727 | `		}else{` |
|        3 |  9728 | `			SyToken *pTok = pGen->pIn;` |
|        3 |  9729 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|        4 |  9730 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|        1 |  9731 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|        3 |  9732 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9733 | `				return SXERR_ABORT;` |
|        - |  9734 | `			}` |
|        3 |  9735 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|        3 |  9736 | `				pGen->pIn++; /* Skip the bogus type token */` |
|        1 |  9737 | `			}` |
|        - |  9738 | `		}` |
|        7 |  9739 | `	}` |
|   215247 |  9740 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|   215247 |  9741 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  9742 | `		return SXERR_ABORT;` |
|        - |  9743 | `	}` |
|        - |  9744 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   215247 |  9745 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   215247 |  9746 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|        - |  9747 | `	/* Assume a standalone class */` |
|   215247 |  9748 | `	pBase = 0;` |
|   215247 |  9749 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   171293 |  9750 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   171293 |  9751 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|        - |  9752 | `			SyBlob sResolved;` |
|        - |  9753 | `			SyString sBaseName;` |
|        - |  9754 | `			sxu32 nRefLine;` |
|   124513 |  9755 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|        - |  9756 | `				/* php parse-fatals here (enums have no inheritance) */` |
|      ! 0 |  9757 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9758 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|      ! 0 |  9759 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9760 | `					return SXERR_ABORT;` |
|        - |  9761 | `				}` |
|      ! 0 |  9762 | `			}` |
|   124513 |  9763 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   124513 |  9764 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   124513 |  9765 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   124513 |  9766 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        3 |  9767 | `				SyBlobRelease(&sResolved);` |
|        4 |  9768 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  9769 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|        1 |  9770 | `					pName);` |
|        3 |  9771 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|        3 |  9772 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9773 | `					return SXERR_ABORT;` |
|        - |  9774 | `				}` |
|        3 |  9775 | `				return SXRET_OK;` |
|        - |  9776 | `			}` |
|   186764 |  9777 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   124506 |  9778 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   124511 |  9779 | `			SyStringInitFromBuf(&sBaseName,` |
|        - |  9780 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - |  9781 | `			/* Interfaces are not allowed */` |
|   124511 |  9782 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|      ! 0 |  9783 | `				pBase = pBase->pNextName;` |
|      ! 0 |  9784 | `			}` |
|   124511 |  9785 | `			if( pBase == 0 ){` |
|      ! 0 |  9786 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - |  9787 | `					"Nonexistent base class '%z'",&sBaseName);` |
|      ! 0 |  9788 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9789 | `					SyBlobRelease(&sResolved);` |
|      ! 0 |  9790 | `					return SXERR_ABORT;` |
|        - |  9791 | `				}` |
|      ! 0 |  9792 | `			}else{` |
|   124511 |  9793 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|        4 |  9794 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  9795 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|        3 |  9796 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9797 | `						SyBlobRelease(&sResolved);` |
|      ! 0 |  9798 | `						return SXERR_ABORT;` |
|        - |  9799 | `					}` |
|        3 |  9800 | `					pBase = 0; /* Never inherit from an enum */` |
|   124510 |  9801 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|      ! 0 |  9802 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  9803 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|      ! 0 |  9804 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9805 | `						SyBlobRelease(&sResolved);` |
|      ! 0 |  9806 | `						return SXERR_ABORT;` |
|        - |  9807 | `					}` |
|      ! 0 |  9808 | `				}` |
|        - |  9809 | `			}` |
|   124511 |  9810 | `			SyBlobRelease(&sResolved);` |
|   124511 |  9811 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|      ! 0 |  9812 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|      ! 0 |  9813 | `			}` |
|    62253 |  9814 | `		}` |
|   171291 |  9815 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|        - |  9816 | `			ph7_class *pInterface;` |
|        - |  9817 | `			/* Interface implementation */` |
|    46797 |  9818 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    23408 |  9819 | `			for(;;){` |
|        - |  9820 | `				SyBlob sResolved;` |
|        - |  9821 | `				SyString sIntName;` |
|        - |  9822 | `				sxu32 nRefLine;` |
|    46809 |  9823 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    46809 |  9824 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    46809 |  9825 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 |  9826 | `					SyBlobRelease(&sResolved);` |
|      ! 0 |  9827 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  9828 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|      ! 0 |  9829 | `						pName);` |
|      ! 0 |  9830 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9831 | `						return SXERR_ABORT;` |
|        - |  9832 | `					}` |
|      ! 0 |  9833 | `					break;` |
|        - |  9834 | `				}` |
|    93613 |  9835 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    46804 |  9836 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    46809 |  9837 | `				SyStringInitFromBuf(&sIntName,` |
|        - |  9838 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - |  9839 | `				/* Only interfaces are allowed */` |
|    46809 |  9840 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 |  9841 | `					pInterface = pInterface->pNextName;` |
|      ! 0 |  9842 | `				}` |
|    46809 |  9843 | `				if( pInterface == 0 ){` |
|      ! 0 |  9844 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - |  9845 | `						"Nonexistent base interface '%z'",&sIntName);` |
|      ! 0 |  9846 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9847 | `						SyBlobRelease(&sResolved);` |
|      ! 0 |  9848 | `						return SXERR_ABORT;` |
|        - |  9849 | `					}` |
|      ! 0 |  9850 | `				}else{` |
|        - |  9851 | `					/* Reject user classes that try to implement Throwable` |
|        - |  9852 | `					 * directly (or via an interface that extends Throwable)` |
|        - |  9853 | `					 * unless they already extend Exception or Error.` |
|        - |  9854 | `					 * Exception and Error themselves are compiled from the` |
|        - |  9855 | `					 * built-in library and are exempt by FQN — a namespaced` |
|        - |  9856 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    46809 |  9857 | `					SyString *pFqn = &pClass->sName;` |
|    46809 |  9858 | `					int bIsExceptionOrError =` |
|    27290 |  9859 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|    72152 |  9860 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    44869 |  9861 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|     3898 |  9862 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|    50693 |  9863 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|    11670 |  9864 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|     3887 |  9865 | `						!bIsExceptionOrError ){` |
|       12 |  9866 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  9867 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|        3 |  9868 | `							&pClass->sName);` |
|        9 |  9869 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  9870 | `							SyBlobRelease(&sResolved);` |
|      ! 0 |  9871 | `							return SXERR_ABORT;` |
|        - |  9872 | `						}` |
|        - |  9873 | `						/* Skip registration so the follow-up abstract-method` |
|        - |  9874 | `						 * check does not produce a duplicate fatal. */` |
|        6 |  9875 | `					}else{` |
|    46803 |  9876 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|        - |  9877 | `					}` |
|        - |  9878 | `				}` |
|    46809 |  9879 | `				SyBlobRelease(&sResolved);` |
|    46809 |  9880 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    23401 |  9881 | `					break;` |
|        - |  9882 | `				}` |
|       16 |  9883 | `				pGen->pIn++;/* Jump the comma */` |
|        4 |  9884 | `			}` |
|    23396 |  9885 | `		}` |
|    85643 |  9886 | `	}` |
|   215245 |  9887 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - |  9888 | `		/* Syntax error */` |
|      ! 0 |  9889 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|      ! 0 |  9890 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  9891 | `		if( rc == SXERR_ABORT ){` |
|        - |  9892 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  9893 | `			return SXERR_ABORT;` |
|        - |  9894 | `		}` |
|      ! 0 |  9895 | `		return SXRET_OK;` |
|        - |  9896 | `	}` |
|   215245 |  9897 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   215245 |  9898 | `	pEnd = 0; /* cc warning */` |
|        - |  9899 | `	/* Delimit the class body */` |
|   215245 |  9900 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   215245 |  9901 | `	if( pEnd >= pGen->pEnd ){` |
|        - |  9902 | `		/* Syntax error */` |
|      ! 0 |  9903 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|      ! 0 |  9904 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  9905 | `		if( rc == SXERR_ABORT ){` |
|        - |  9906 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  9907 | `			return SXERR_ABORT;` |
|        - |  9908 | `		}` |
|      ! 0 |  9909 | `		return SXRET_OK;` |
|        - |  9910 | `	}` |
|        - |  9911 | `	/* The delimiter token is the class body's closing brace */` |
|   215245 |  9912 | `	pClass->nEndLine = pEnd->nLine;` |
|        - |  9913 | `	/* Swap token stream */` |
|   215245 |  9914 | `	pTmp = pGen->pEnd;` |
|   215245 |  9915 | `	pGen->pEnd = pEnd;` |
|        - |  9916 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|   215245 |  9917 | `	pClass->iFlags \|= iFlags;` |
|        - |  9918 | `	/* Start the parse process */` |
|   823020 |  9919 | `	for(;;){` |
|        - |  9920 | `		/* Jump leading/trailing semi-colons */` |
|  2211171 |  9921 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   354497 |  9922 | `			pGen->pIn++;` |
|        5 |  9923 | `		}` |
|  1856679 |  9924 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - |  9925 | `			/* End of class body */` |
|   215203 |  9926 | `			break;` |
|        - |  9927 | `		}` |
|        - |  9928 | `		/* Bind a directly-preceding docblock to this member */` |
|  1641481 |  9929 | `		GenStateSetPendingDoc(&(*pGen));` |
|  1641476 |  9930 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|   820743 |  9931 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|      ! 0 |  9932 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  9933 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|      ! 0 |  9934 | `				&pGen->pIn->sData,pName);` |
|      ! 0 |  9935 | `			if( rc == SXERR_ABORT ){` |
|        - |  9936 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  9937 | `				return SXERR_ABORT;` |
|        - |  9938 | `			}` |
|      ! 0 |  9939 | `			goto done;` |
|        - |  9940 | `		}` |
|        - |  9941 | `		/* Assume public visibility */` |
|  1641481 |  9942 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  1641481 |  9943 | `		iAttrflags = 0;` |
|        - |  9944 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|        - |  9945 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|        - |  9946 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|        - |  9947 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  1641481 |  9948 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      ! 0 |  9949 | `			int bMod = 0;` |
|      ! 0 |  9950 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      ! 0 |  9951 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        - |  9952 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|        - |  9953 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|        - |  9954 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|        - |  9955 | `			 * that the generic keyword dispatch would misread as a method. */` |
|      ! 0 |  9956 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      ! 0 |  9957 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      ! 0 |  9958 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|      ! 0 |  9959 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|      ! 0 |  9960 | `			}` |
|      ! 0 |  9961 | `			if( !bMod ){` |
|      ! 0 |  9962 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 |  9963 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  9964 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9965 | `						return SXERR_ABORT;` |
|        - |  9966 | `					}` |
|      ! 0 |  9967 | `					goto done;` |
|        - |  9968 | `				}` |
|      ! 0 |  9969 | `				continue;` |
|        - |  9970 | `			}` |
|      ! 0 |  9971 | `		}` |
|  1641481 |  9972 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - |  9973 | `			/* Extract the current keyword */` |
|  1641481 |  9974 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1641481 |  9975 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|        - |  9976 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|       47 |  9977 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|       47 |  9978 | `				if( rc != SXRET_OK ){` |
|        6 |  9979 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9980 | `						return SXERR_ABORT;` |
|        - |  9981 | `					}` |
|        6 |  9982 | `					goto done;` |
|        - |  9983 | `				}` |
|       41 |  9984 | `				continue;` |
|        - |  9985 | `			}` |
|  1641439 |  9986 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - |  9987 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|        - |  9988 | `				TraitUseEntry sUse;` |
|       63 |  9989 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|       63 |  9990 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|       63 |  9991 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|       37 |  9992 | `				for(;;){` |
|        - |  9993 | `					ph7_class *pTrait;` |
|        - |  9994 | `					SyString *pTraitName;` |
|       71 |  9995 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 |  9996 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9997 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|      ! 0 |  9998 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  9999 | `							return SXERR_ABORT;` |
|        - | 10000 | `						}` |
|      ! 0 | 10001 | `						break;` |
|        - | 10002 | `					}` |
|       71 | 10003 | `					pTraitName = &pGen->pIn->sData;` |
|        - | 10004 | `					/* Resolve trait name through namespace/imports */ {` |
|        - | 10005 | `						SyBlob sResolved;` |
|       71 | 10006 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       71 | 10007 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|      137 | 10008 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|       66 | 10009 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       71 | 10010 | `						SyBlobRelease(&sResolved);` |
|        - | 10011 | `					}` |
|        - | 10012 | `					/* Only traits are allowed */` |
|       71 | 10013 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 | 10014 | `						pTrait = pTrait->pNextName;` |
|      ! 0 | 10015 | `					}` |
|       71 | 10016 | `					if( pTrait == 0 ){` |
|      ! 0 | 10017 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 10018 | `							"'%z' is not a trait",pTraitName);` |
|      ! 0 | 10019 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10020 | `							return SXERR_ABORT;` |
|        - | 10021 | `						}` |
|      ! 0 | 10022 | `					}else{` |
|       71 | 10023 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|        - | 10024 | `					}` |
|       71 | 10025 | `					pGen->pIn++; /* Advance past trait name */` |
|       71 | 10026 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       34 | 10027 | `						break;` |
|        - | 10028 | `					}` |
|       10 | 10029 | `					pGen->pIn++; /* Jump the comma */` |
|        2 | 10030 | `				}` |
|        - | 10031 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|       63 | 10032 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|        - | 10033 | `					SyToken *pBlock;` |
|       13 | 10034 | `					pGen->pIn++; /* Jump '{' */` |
|       13 | 10035 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       13 | 10036 | `					sUse.pResolvStart = pGen->pIn;` |
|       13 | 10037 | `					sUse.pResolvEnd = pBlock;` |
|       13 | 10038 | `					if( pBlock < pGen->pEnd ){` |
|       13 | 10039 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|        8 | 10040 | `					}else{` |
|      ! 0 | 10041 | `						pGen->pIn = pGen->pEnd;` |
|        - | 10042 | `					}` |
|        5 | 10043 | `				}` |
|       63 | 10044 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|        - | 10045 | `				/* The semicolon will be consumed by the outer loop */` |
|       63 | 10046 | `				continue;` |
|        - | 10047 | `			}` |
|  1641381 | 10048 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  1497221 | 10049 | `				iProtection = nKwrd;` |
|  1497221 | 10050 | `				pGen->pIn++; /* Jump the visibility token */` |
|        - | 10051 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`. */`` |
|  1497221 | 10052 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       22 | 10053 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       22 | 10054 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        9 | 10055 | `				}` |
|  1497216 | 10056 | `				if( pGen->pIn >= pGen->pEnd` |
|  1497221 | 10057 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 10058 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10059 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|      ! 0 | 10060 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 10061 | `					if( rc == SXERR_ABORT ){` |
|        - | 10062 | `						/* Error count limit reached,abort immediately */` |
|      ! 0 | 10063 | `						return SXERR_ABORT;` |
|        - | 10064 | `					}` |
|      ! 0 | 10065 | `					goto done;` |
|        - | 10066 | `				}` |
|  1497221 | 10067 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        - | 10068 | `					/* Attribute declaration (untyped) */` |
|   210329 | 10069 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   210329 | 10070 | `					if( rc != SXRET_OK ){` |
|       11 | 10071 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10072 | `							return SXERR_ABORT;` |
|        - | 10073 | `						}` |
|       11 | 10074 | `						goto done;` |
|        - | 10075 | `					}` |
|   210321 | 10076 | `					continue;` |
|        - | 10077 | `				}` |
|  1286897 | 10078 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        - | 10079 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|      187 | 10080 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      187 | 10081 | `					if( rc != SXRET_OK ){` |
|        8 | 10082 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10083 | `							return SXERR_ABORT;` |
|        - | 10084 | `						}` |
|        8 | 10085 | `						goto done;` |
|        - | 10086 | `					}` |
|      181 | 10087 | `					continue;` |
|        - | 10088 | `				}` |
|        - | 10089 | `				/* Extract the keyword */` |
|  1286715 | 10090 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   643355 | 10091 | `			}` |
|  1430875 | 10092 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|        - | 10093 | `				/* Process constant declaration */` |
|   143863 | 10094 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|   143863 | 10095 | `				if( rc != SXRET_OK ){` |
|       11 | 10096 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10097 | `						return SXERR_ABORT;` |
|        - | 10098 | `					}` |
|       11 | 10099 | `					goto done;` |
|        - | 10100 | `				}` |
|    71930 | 10101 | `			}else{` |
|  1287017 | 10102 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|        - | 10103 | `					/* Static method or attribute,record that */` |
|    23441 | 10104 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    23441 | 10105 | `					pGen->pIn++; /* Jump the static keyword */` |
|    23441 | 10106 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 10107 | `						/* Extract the keyword */` |
|    23413 | 10108 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    23413 | 10109 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      ! 0 | 10110 | `							iProtection = nKwrd;` |
|      ! 0 | 10111 | `							pGen->pIn++; /* Jump the visibility token */` |
|      ! 0 | 10112 | `						}` |
|    11704 | 10113 | `					}` |
|        - | 10114 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|        - | 10115 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|        - | 10116 | `					 * than a generic "expecting method" parse error. */` |
|    23441 | 10117 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      ! 0 | 10118 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      ! 0 | 10119 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|      ! 0 | 10120 | `					}` |
|    23436 | 10121 | `					if( pGen->pIn >= pGen->pEnd` |
|    23441 | 10122 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 10123 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10124 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|      ! 0 | 10125 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 10126 | `						if( rc == SXERR_ABORT ){` |
|        - | 10127 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 10128 | `							return SXERR_ABORT;` |
|        - | 10129 | `						}` |
|      ! 0 | 10130 | `						goto done;` |
|        - | 10131 | `					}` |
|    23441 | 10132 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        - | 10133 | `						/* Attribute declaration */` |
|       29 | 10134 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       29 | 10135 | `						if( rc != SXRET_OK ){` |
|        3 | 10136 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 10137 | `								return SXERR_ABORT;` |
|        - | 10138 | `							}` |
|        3 | 10139 | `							goto done;` |
|        - | 10140 | `						}` |
|       26 | 10141 | `						continue;` |
|        - | 10142 | `					}` |
|    23415 | 10143 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        - | 10144 | `						/* Typed static attribute declaration */` |
|       15 | 10145 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       15 | 10146 | `						if( rc != SXRET_OK ){` |
|        3 | 10147 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 10148 | `								return SXERR_ABORT;` |
|        - | 10149 | `							}` |
|        3 | 10150 | `							goto done;` |
|        - | 10151 | `						}` |
|       13 | 10152 | `						continue;` |
|        - | 10153 | `					}` |
|        - | 10154 | `					/* Extract the keyword */` |
|    23403 | 10155 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1275280 | 10156 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|        - | 10157 | `					/* Abstract method,record that */` |
|       15 | 10158 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|        - | 10159 | `					/* Mark the whole class as abstract */` |
|       15 | 10160 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|        - | 10161 | `					/* Advance the stream cursor */` |
|       15 | 10162 | `					pGen->pIn++;` |
|       15 | 10163 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       15 | 10164 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       15 | 10165 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       13 | 10166 | `							iProtection = nKwrd;` |
|       13 | 10167 | `							pGen->pIn++; /* Jump the visibility token */` |
|        5 | 10168 | `						}` |
|        6 | 10169 | `					}` |
|       15 | 10170 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       12 | 10171 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - | 10172 | `							/* Static method */` |
|      ! 0 | 10173 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 | 10174 | `							pGen->pIn++; /* Jump the static keyword */` |
|      ! 0 | 10175 | `					}` |
|       15 | 10176 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       12 | 10177 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 10178 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10179 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|      ! 0 | 10180 | `								&pGen->pIn->sData,pName);` |
|      ! 0 | 10181 | `							if( rc == SXERR_ABORT ){` |
|        - | 10182 | `								/* Error count limit reached,abort immediately */` |
|      ! 0 | 10183 | `								return SXERR_ABORT;` |
|        - | 10184 | `							}` |
|      ! 0 | 10185 | `							goto done;` |
|        - | 10186 | `					}` |
|       15 | 10187 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  1263575 | 10188 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|        - | 10189 | `					/* final method ,record that */` |
|       21 | 10190 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       21 | 10191 | `					pGen->pIn++; /* Jump the final keyword */` |
|       21 | 10192 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 10193 | `						/* Extract the keyword */` |
|       21 | 10194 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       21 | 10195 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       11 | 10196 | `							iProtection = nKwrd;` |
|       11 | 10197 | `							pGen->pIn++; /* Jump the visibility token */` |
|        4 | 10198 | `						}` |
|        9 | 10199 | `					}` |
|       21 | 10200 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       18 | 10201 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|        - | 10202 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|        - | 10203 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|        - | 10204 | `							 * child class is compiled (PH7_ClassInherit). */` |
|       14 | 10205 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|       14 | 10206 | `							if( rc != SXRET_OK ){` |
|      ! 0 | 10207 | `								if( rc == SXERR_ABORT ){` |
|      ! 0 | 10208 | `									return SXERR_ABORT;` |
|        - | 10209 | `								}` |
|      ! 0 | 10210 | `								goto done;` |
|        - | 10211 | `							}` |
|       14 | 10212 | `							continue;` |
|        - | 10213 | `					}` |
|        9 | 10214 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        6 | 10215 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - | 10216 | `							/* Static method */` |
|      ! 0 | 10217 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 | 10218 | `							pGen->pIn++; /* Jump the static keyword */` |
|      ! 0 | 10219 | `					}` |
|        9 | 10220 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|        6 | 10221 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 10222 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10223 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|      ! 0 | 10224 | `								&pGen->pIn->sData,pName);` |
|      ! 0 | 10225 | `							if( rc == SXERR_ABORT ){` |
|        - | 10226 | `								/* Error count limit reached,abort immediately */` |
|      ! 0 | 10227 | `								return SXERR_ABORT;` |
|        - | 10228 | `							}` |
|      ! 0 | 10229 | `							goto done;` |
|        - | 10230 | `					}` |
|        9 | 10231 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|        3 | 10232 | `				}` |
|  1286967 | 10233 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|      ! 0 | 10234 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10235 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|      ! 0 | 10236 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 10237 | `						if( rc == SXERR_ABORT ){` |
|        - | 10238 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 10239 | `							return SXERR_ABORT;` |
|        - | 10240 | `						}` |
|      ! 0 | 10241 | `						goto done;` |
|        - | 10242 | `				}` |
|  1286967 | 10243 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|        7 | 10244 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|        7 | 10245 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|      ! 0 | 10246 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10247 | `							"Expecting attribute declaration after 'var' keyword");` |
|      ! 0 | 10248 | `						if( rc == SXERR_ABORT ){` |
|        - | 10249 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 10250 | `							return SXERR_ABORT;` |
|        - | 10251 | `						}` |
|      ! 0 | 10252 | `						goto done;` |
|        - | 10253 | `					}` |
|        - | 10254 | `					/* Attribute declaration */` |
|        7 | 10255 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        4 | 10256 | `				}else{` |
|        - | 10257 | `					/* Process method declaration */` |
|  1286961 | 10258 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|        - | 10259 | `				}` |
|  1286967 | 10260 | `				if( rc != SXRET_OK ){` |
|       16 | 10261 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10262 | `						return SXERR_ABORT;` |
|        - | 10263 | `					}` |
|       16 | 10264 | `					goto done;` |
|        - | 10265 | `				}` |
|        - | 10266 | `			}` |
|   715405 | 10267 | `		}else{` |
|        - | 10268 | `			/* Attribute declaration */` |
|      ! 0 | 10269 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 10270 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 10271 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10272 | `					return SXERR_ABORT;` |
|        - | 10273 | `				}` |
|      ! 0 | 10274 | `				goto done;` |
|        - | 10275 | `			}` |
|        - | 10276 | `		}` |
|        5 | 10277 | `	}` |
|        - | 10278 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|        - | 10279 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|        - | 10280 | `	 */` |
|        - | 10281 | `	{` |
|        - | 10282 | `		TraitUseEntry *apUse;` |
|        - | 10283 | `		sxu32 nU;` |
|   215203 | 10284 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   215261 | 10285 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|       63 | 10286 | `			TraitUseEntry *pUse = &apUse[nU];` |
|       63 | 10287 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|       63 | 10288 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|       63 | 10289 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|        - | 10290 | `			sxu32 nT;` |
|       63 | 10291 | `			if( !hasResolution ){` |
|        - | 10292 | `				/* No conflict resolution block: use standard trait application */` |
|      107 | 10293 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       59 | 10294 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|       59 | 10295 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 10296 | `						break;` |
|        - | 10297 | `					}` |
|       32 | 10298 | `				}` |
|       29 | 10299 | `			}else{` |
|        - | 10300 | `				/* With resolution block: copy attributes, record traits,` |
|        - | 10301 | `				 * then use the block to resolve method conflicts.` |
|        - | 10302 | `				 */` |
|        - | 10303 | `				SyToken *pR;` |
|       25 | 10304 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       15 | 10305 | `					ph7_class *pTR = apTrait[nT];` |
|        - | 10306 | `					ph7_class_attr *pAR;` |
|        - | 10307 | `					SyHashEntry *pER;` |
|        - | 10308 | `					SyString *pNR;` |
|       15 | 10309 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|       21 | 10310 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|      ! 0 | 10311 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|      ! 0 | 10312 | `						pNR = &pAR->sName;` |
|      ! 0 | 10313 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      ! 0 | 10314 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|      ! 0 | 10315 | `						}` |
|      ! 0 | 10316 | `					}` |
|       15 | 10317 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|        9 | 10318 | `				}` |
|        - | 10319 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       13 | 10320 | `				pR = pUse->pResolvStart;` |
|       27 | 10321 | `				while( pR < pUse->pResolvEnd ){` |
|        - | 10322 | `					SyString sTrait,sMethod;` |
|        - | 10323 | `					ph7_class *pSrcTrait;` |
|        - | 10324 | `					ph7_class_method *pMeth;` |
|        - | 10325 | `					sxi32 nRKwrd;` |
|       41 | 10326 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|       27 | 10327 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       17 | 10328 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       17 | 10329 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       17 | 10330 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       17 | 10331 | `					sMethod = pR->sData;` |
|       17 | 10332 | `					pR++;` |
|       17 | 10333 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        7 | 10334 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        7 | 10335 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        7 | 10336 | `							sTrait = sMethod;` |
|        7 | 10337 | `							pR++;` |
|        7 | 10338 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        7 | 10339 | `							sMethod = pR->sData;` |
|        7 | 10340 | `							pR++;` |
|        3 | 10341 | `						}` |
|        3 | 10342 | `					}` |
|       17 | 10343 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 10344 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 10345 | `						continue;` |
|        - | 10346 | `					}` |
|       17 | 10347 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       17 | 10348 | `					pR++;` |
|       17 | 10349 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|        5 | 10350 | `						pSrcTrait = 0;` |
|        7 | 10351 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        7 | 10352 | `							SyString *pTN = &apTrait[nT]->sName;` |
|       10 | 10353 | `							if( pTN->nByte >= sTrait.nByte &&` |
|        6 | 10354 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|        5 | 10355 | `								pSrcTrait = apTrait[nT];` |
|        5 | 10356 | `								break;` |
|        - | 10357 | `							}` |
|        2 | 10358 | `						}` |
|        5 | 10359 | `						if( pSrcTrait ){` |
|        5 | 10360 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        5 | 10361 | `							if( pMeth ){` |
|        5 | 10362 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|        5 | 10363 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|        5 | 10364 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|        2 | 10365 | `								}` |
|        2 | 10366 | `							}` |
|        2 | 10367 | `						}` |
|        2 | 10368 | `					}` |
|       35 | 10369 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        3 | 10370 | `				}` |
|        - | 10371 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|       25 | 10372 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        - | 10373 | `					ph7_class_method *pMR;` |
|        - | 10374 | `					SyHashEntry *pER;` |
|        - | 10375 | `					SyString *pNR;` |
|       15 | 10376 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|       41 | 10377 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|       23 | 10378 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|       23 | 10379 | `						pNR = &pMR->sFunc.sName;` |
|       23 | 10380 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       14 | 10381 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|        6 | 10382 | `						}` |
|        3 | 10383 | `					}` |
|        9 | 10384 | `				}` |
|        - | 10385 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       13 | 10386 | `				pR = pUse->pResolvStart;` |
|       27 | 10387 | `				while( pR < pUse->pResolvEnd ){` |
|        - | 10388 | `					SyString sTrait,sMethod,sAlias;` |
|        - | 10389 | `					ph7_class *pSrcTrait;` |
|        - | 10390 | `					ph7_class_method *pMeth;` |
|       27 | 10391 | `					int hasQual = 0;` |
|        - | 10392 | `					sxi32 nRKwrd;` |
|       41 | 10393 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|       27 | 10394 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       17 | 10395 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       17 | 10396 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       17 | 10397 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|       17 | 10398 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       17 | 10399 | `					sMethod = pR->sData;` |
|       17 | 10400 | `					pR++;` |
|       17 | 10401 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        7 | 10402 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        7 | 10403 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        7 | 10404 | `							sTrait = sMethod;` |
|        7 | 10405 | `							hasQual = 1;` |
|        7 | 10406 | `							pR++;` |
|        7 | 10407 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        7 | 10408 | `							sMethod = pR->sData;` |
|        7 | 10409 | `							pR++;` |
|        3 | 10410 | `						}` |
|        3 | 10411 | `					}` |
|       17 | 10412 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 10413 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 10414 | `						continue;` |
|        - | 10415 | `					}` |
|       17 | 10416 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       17 | 10417 | `					pR++;` |
|       17 | 10418 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       13 | 10419 | `						sxi32 iNewVis = -1;` |
|       13 | 10420 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|        7 | 10421 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|        7 | 10422 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|        7 | 10423 | `								iNewVis = nAK;` |
|        7 | 10424 | `								pR++;` |
|        3 | 10425 | `							}` |
|        3 | 10426 | `						}` |
|       13 | 10427 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       11 | 10428 | `							sAlias = pR->sData;` |
|       11 | 10429 | `							pR++;` |
|        4 | 10430 | `						}` |
|       13 | 10431 | `						pMeth = 0;` |
|       13 | 10432 | `						if( hasQual ){` |
|        3 | 10433 | `							pSrcTrait = 0;` |
|        5 | 10434 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        5 | 10435 | `								SyString *pTN = &apTrait[nT]->sName;` |
|        7 | 10436 | `								if( pTN->nByte >= sTrait.nByte &&` |
|        4 | 10437 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|        3 | 10438 | `									pSrcTrait = apTrait[nT];` |
|        3 | 10439 | `									break;` |
|        - | 10440 | `								}` |
|        2 | 10441 | `							}` |
|        3 | 10442 | `							if( pSrcTrait ){` |
|        3 | 10443 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        1 | 10444 | `							}` |
|        2 | 10445 | `						}else{` |
|       10 | 10446 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|        - | 10447 | `						}` |
|       13 | 10448 | `						if( pMeth ){` |
|       13 | 10449 | `							if( sAlias.nByte > 0 ){` |
|        - | 10450 | `								/* Create a shallow copy of the method struct for the alias` |
|        - | 10451 | `								 * so it can carry its own visibility without affecting the original.` |
|        - | 10452 | `								 */` |
|        - | 10453 | `								ph7_class_method *pAlias;` |
|        - | 10454 | `								char *zAliasDup;` |
|       11 | 10455 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       11 | 10456 | `								if( pAlias ){` |
|       11 | 10457 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       11 | 10458 | `									if( iNewVis >= 0 ){` |
|        5 | 10459 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|        3 | 10460 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|      ! 0 | 10461 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|        2 | 10462 | `									}` |
|       11 | 10463 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       11 | 10464 | `									if( zAliasDup ){` |
|       11 | 10465 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|        4 | 10466 | `									}` |
|        7 | 10467 | `								}` |
|        7 | 10468 | `							}else if( iNewVis >= 0 ){` |
|        - | 10469 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|        - | 10470 | `								ph7_class_method *pCopy;` |
|        3 | 10471 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        3 | 10472 | `								if( pCopy ){` |
|        3 | 10473 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|        3 | 10474 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|        3 | 10475 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|        3 | 10476 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|      ! 0 | 10477 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|        - | 10478 | `									/* Replace the method in the class hash */` |
|        3 | 10479 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|        3 | 10480 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|        1 | 10481 | `								}` |
|        1 | 10482 | `							}` |
|        5 | 10483 | `						}` |
|        5 | 10484 | `						SXUNUSED(hasQual);` |
|        5 | 10485 | `					}` |
|       21 | 10486 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        3 | 10487 | `				}` |
|        - | 10488 | `			}` |
|       63 | 10489 | `			SySetRelease(&pUse->aTraits);` |
|       34 | 10490 | `		}` |
|        - | 10491 | `	}` |
|   215203 | 10492 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|        - | 10493 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|        - | 10494 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|       27 | 10495 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|       27 | 10496 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10497 | `			SySetRelease(&aUseEntries);` |
|      ! 0 | 10498 | `			SySetRelease(&aInterfaces);` |
|      ! 0 | 10499 | `			return SXERR_ABORT;` |
|        - | 10500 | `		}` |
|       12 | 10501 | `	}` |
|        - | 10502 | `	/* Install the class */` |
|   215203 | 10503 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   215203 | 10504 | `	if( rc == SXRET_OK ){` |
|        - | 10505 | `		ph7_class **apInterface;` |
|        - | 10506 | `		sxu32 n;` |
|   215203 | 10507 | `		if( pBase ){` |
|        - | 10508 | `			/* Inherit from base class and mark as a subclass */` |
|   124509 | 10509 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|    62252 | 10510 | `		}` |
|   215203 | 10511 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   262001 | 10512 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|        - | 10513 | `			/* Implements one or more interface */` |
|    46803 | 10514 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    46803 | 10515 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 10516 | `				break;` |
|        - | 10517 | `			}` |
|    23404 | 10518 | `		}` |
|        - | 10519 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|        - | 10520 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|   215203 | 10521 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|       27 | 10522 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|       27 | 10523 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 10524 | `				pIntf = pIntf->pNextName;` |
|      ! 0 | 10525 | `			}` |
|       27 | 10526 | `			if( pIntf ){` |
|       27 | 10527 | `				PH7_ClassImplement(pClass,pIntf);` |
|       12 | 10528 | `			}` |
|       27 | 10529 | `			if( pClass->nEnumBacking != 0 ){` |
|       13 | 10530 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|       13 | 10531 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 10532 | `					pIntf = pIntf->pNextName;` |
|      ! 0 | 10533 | `				}` |
|       13 | 10534 | `				if( pIntf ){` |
|       13 | 10535 | `					PH7_ClassImplement(pClass,pIntf);` |
|        6 | 10536 | `				}` |
|        6 | 10537 | `			}` |
|       12 | 10538 | `		}` |
|        - | 10539 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|        - | 10540 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|   215198 | 10541 | `		if( rc == SXRET_OK` |
|   215198 | 10542 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|   215203 | 10543 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   171003 | 10544 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|        - | 10545 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   171003 | 10546 | `			if( pStringable ){` |
|   171003 | 10547 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   171003 | 10548 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|        - | 10549 | `				sxu32 i;` |
|   171003 | 10550 | `				int bAlready = 0;` |
|   209847 | 10551 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    42735 | 10552 | `					if( apImpl[i] == pStringable ){` |
|     3891 | 10553 | `						bAlready = 1;` |
|     3891 | 10554 | `						break;` |
|        - | 10555 | `					}` |
|    19427 | 10556 | `				}` |
|   171003 | 10557 | `				if( !bAlready ){` |
|   167117 | 10558 | `					PH7_ClassImplement(pClass,pStringable);` |
|    83556 | 10559 | `				}` |
|    85499 | 10560 | `			}` |
|    85499 | 10561 | `		}` |
|        - | 10562 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   215203 | 10563 | `		if( rc == SXRET_OK ){` |
|   215203 | 10564 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   215203 | 10565 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 10566 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 10567 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 10568 | `				return SXERR_ABORT;` |
|        - | 10569 | `			}` |
|   107599 | 10570 | `		}` |
|        - | 10571 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   215203 | 10572 | `		if( rc == SXRET_OK ){` |
|   215203 | 10573 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   215203 | 10574 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 10575 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 10576 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 10577 | `				return SXERR_ABORT;` |
|        - | 10578 | `			}` |
|   107599 | 10579 | `		}` |
|   107599 | 10580 | `	}` |
|   215203 | 10581 | `	SySetRelease(&aUseEntries);` |
|   215203 | 10582 | `	SySetRelease(&aInterfaces);` |
|   215203 | 10583 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10584 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 10585 | `		return SXERR_ABORT;` |
|        - | 10586 | `	}` |
|   107599 | 10587 | `done:` |
|        - | 10588 | `	/* Point beyond the class body */` |
|   215245 | 10589 | `	pGen->pIn = &pEnd[1];` |
|   215245 | 10590 | `	pGen->pEnd = pTmp;` |
|   215245 | 10591 | `	return PH7_OK;` |
|   107626 | 10592 | `}` |
|        - | 10593 | `/* Compile a named class declaration (the common case). */` |
|   215214 | 10594 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|        5 | 10595 | `{` |
|   215219 | 10596 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|        5 | 10597 | `}` |
|        - | 10598 | `/*` |
|        - | 10599 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|        - | 10600 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|        - | 10601 | ` * compile + install the class body once (at compile time, like every other` |
|        - | 10602 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|        - | 10603 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|        - | 10604 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|        - | 10605 | ` */` |
|       28 | 10606 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        4 | 10607 | `{` |
|        - | 10608 | `	char zName[128];         /* Synthesized class name */` |
|        - | 10609 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|        - | 10610 | `	SyString sName;` |
|        - | 10611 | `	SyToken *pArgStart,*pArgEnd;` |
|       32 | 10612 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|        - | 10613 | `	                              * is keyed to this 'class' token */` |
|        - | 10614 | `	ph7_value *pObj;` |
|       32 | 10615 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 10616 | `	sxu32 nIdx,nLen;` |
|        - | 10617 | `	sxi32 nArg,rc;` |
|       14 | 10618 | `	SXUNUSED(iCompileFlag);` |
|        - | 10619 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|       32 | 10620 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|       32 | 10621 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 | 10622 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      ! 0 | 10623 | `	}` |
|       32 | 10624 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|        - | 10625 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|        - | 10626 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|        - | 10627 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|       32 | 10628 | `	pArgStart = pArgEnd = 0;` |
|       32 | 10629 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|       32 | 10630 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10631 | `		return rc;` |
|        - | 10632 | `	}` |
|        - | 10633 | `	{` |
|        - | 10634 | ``		/* Expression-position attributes (`new #[A] class {…}`) */`` |
|       32 | 10635 | `		ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0);` |
|       28 | 10636 | `		if( pAnonClass` |
|       32 | 10637 | `		 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 10638 | `			return SXERR_ABORT;` |
|        - | 10639 | `		}` |
|        - | 10640 | `	}` |
|        - | 10641 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|        - | 10642 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|       32 | 10643 | `	nArg = 0;` |
|       32 | 10644 | `	if( pArgStart < pArgEnd ){` |
|        7 | 10645 | `		SyToken *pSavedIn = pGen->pIn;` |
|        7 | 10646 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|        - | 10647 | `		SyToken *pArgNext;` |
|        7 | 10648 | `		pGen->pIn = pArgStart;` |
|        7 | 10649 | `		pGen->pEnd = pArgEnd;` |
|       13 | 10650 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|        7 | 10651 | `			if( pGen->pIn < pArgNext ){` |
|        7 | 10652 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|        7 | 10653 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10654 | `					pGen->pIn = pSavedIn;` |
|      ! 0 | 10655 | `					pGen->pEnd = pSavedEnd;` |
|      ! 0 | 10656 | `					return SXERR_ABORT;` |
|        - | 10657 | `				}` |
|        7 | 10658 | `				nArg++;` |
|        3 | 10659 | `			}` |
|        7 | 10660 | `			pGen->pIn = &pArgNext[1];` |
|        1 | 10661 | `		}` |
|        7 | 10662 | `		pGen->pIn = pSavedIn;` |
|        7 | 10663 | `		pGen->pEnd = pSavedEnd;` |
|        3 | 10664 | `	}` |
|        - | 10665 | `	/* Load the synthesized class name */` |
|       32 | 10666 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       32 | 10667 | `	if( pObj == 0 ){` |
|      ! 0 | 10668 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 10669 | `		return SXERR_ABORT;` |
|        - | 10670 | `	}` |
|       32 | 10671 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       32 | 10672 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - | 10673 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|       32 | 10674 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       32 | 10675 | `	return SXRET_OK;` |
|       18 | 10676 | `}` |
|        - | 10677 | `/*` |
|        - | 10678 | ` * Compile a user-defined abstract class.` |
|        - | 10679 | ` *  According to the PHP language reference manual` |
|        - | 10680 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|        - | 10681 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|        - | 10682 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|        - | 10683 | ` *   the method's signature - they cannot define the implementation.` |
|        - | 10684 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|        - | 10685 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|        - | 10686 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|        - | 10687 | ` *   method is defined as protected, the function implementation must be defined as either` |
|        - | 10688 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|        - | 10689 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|        - | 10690 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|        - | 10691 | ` *   could differ.` |
|        - | 10692 | ` */` |
|        - | 10693 | `/*` |
|        - | 10694 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|        - | 10695 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|        - | 10696 | ` * receives the corresponding PH7_CLASS_* bit.` |
|        - | 10697 | ` */` |
|  6317036 | 10698 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|        5 | 10699 | `{` |
|  6317041 | 10700 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  3931557 | 10701 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  3931557 | 10702 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  3884923 | 10703 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  1934654 | 10704 | `	}` |
|  6254797 | 10705 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  6254737 | 10706 | `	return FALSE;` |
|  3158523 | 10707 | `}` |
|        - | 10708 | `/*` |
|        - | 10709 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|        - | 10710 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|        - | 10711 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|        - | 10712 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|        - | 10713 | ` */` |
|  6254732 | 10714 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|        5 | 10715 | `{` |
|  6254737 | 10716 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  6254737 | 10717 | `	sxi32 iFlags = 0,iFlag;` |
|  6317041 | 10718 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|    62309 | 10719 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|        5 | 10720 | `			pDup = pIn;` |
|        2 | 10721 | `		}` |
|    62309 | 10722 | `		iFlags \|= iFlag;` |
|    62309 | 10723 | `		pIn++;` |
|        5 | 10724 | `	}` |
|  6254737 | 10725 | `	*ppIn = pIn;` |
|  6254737 | 10726 | `	if( ppDup ){ *ppDup = pDup; }` |
|  6254737 | 10727 | `	return iFlags;` |
|        5 | 10728 | `}` |
|        - | 10729 | `/*` |
|        - | 10730 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|        - | 10731 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|        - | 10732 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|        - | 10733 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|        - | 10734 | `` * `readonly`) to their existing handlers.`` |
|        - | 10735 | ` */` |
|  6227474 | 10736 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|        5 | 10737 | `{` |
|  6227479 | 10738 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  3148770 | 10739 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  6244989 | 10740 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|        5 | 10741 | `}` |
|        - | 10742 | `/*` |
|        - | 10743 | ` * Compile a class declaration carrying one or more leading modifiers` |
|        - | 10744 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|        - | 10745 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|        - | 10746 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|        - | 10747 | `` * `abstract`+`final` pair, like PHP.`` |
|        - | 10748 | ` */` |
|    27258 | 10749 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|        5 | 10750 | `{` |
|        - | 10751 | `	SyToken *pDup;` |
|    27263 | 10752 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|        - | 10753 | `	sxi32 rc;` |
|    27263 | 10754 | `	if( pDup ){` |
|        4 | 10755 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|        2 | 10756 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|        3 | 10757 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10758 | `			return SXERR_ABORT;` |
|        - | 10759 | `		}` |
|        1 | 10760 | `	}` |
|    27258 | 10761 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|    13634 | 10762 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|        3 | 10763 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10764 | `			"Cannot use the final modifier on an abstract class");` |
|        3 | 10765 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10766 | `			return SXERR_ABORT;` |
|        - | 10767 | `		}` |
|        1 | 10768 | `	}` |
|    27263 | 10769 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|    13634 | 10770 | `}` |
|        - | 10771 | `/*` |
|        - | 10772 | ` * Compile a user-defined trait.` |
|        - | 10773 | ` *  Traits are similar to classes, but only intended to group functionality` |
|        - | 10774 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|        - | 10775 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|        - | 10776 | ` */` |
|       72 | 10777 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|        5 | 10778 | `{` |
|       77 | 10779 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 10780 | `	ph7_class *pClass;` |
|        - | 10781 | `	SyToken *pEnd,*pTmp;` |
|        - | 10782 | `	sxi32 iProtection;` |
|        - | 10783 | `	sxi32 iAttrflags;` |
|        - | 10784 | `	SyString *pName;` |
|        - | 10785 | `	sxi32 nKwrd;` |
|        - | 10786 | `	sxi32 rc;` |
|        - | 10787 | `	/* Jump the 'trait' keyword */` |
|       77 | 10788 | `	pGen->pIn++;` |
|       77 | 10789 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 10790 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|      ! 0 | 10791 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10792 | `			return SXERR_ABORT;` |
|        - | 10793 | `		}` |
|      ! 0 | 10794 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|      ! 0 | 10795 | `			pGen->pIn++;` |
|      ! 0 | 10796 | `		}` |
|      ! 0 | 10797 | `		return SXRET_OK;` |
|        - | 10798 | `	}` |
|        - | 10799 | `	/* Extract trait name */` |
|       77 | 10800 | `	pName = &pGen->pIn->sData;` |
|       77 | 10801 | `	pGen->pIn++;` |
|        - | 10802 | `	/* Build FQN and obtain a raw class */ {` |
|        - | 10803 | `		SyBlob sFQN;` |
|        - | 10804 | `		SyString sFQNStr;` |
|       77 | 10805 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       77 | 10806 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|       77 | 10807 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       77 | 10808 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|       77 | 10809 | `		SyBlobRelease(&sFQN);` |
|        - | 10810 | `	}` |
|       77 | 10811 | `	if( pClass == 0 ){` |
|      ! 0 | 10812 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 10813 | `		return SXERR_ABORT;` |
|        - | 10814 | `	}` |
|       77 | 10815 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|       77 | 10816 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 10817 | `		return SXERR_ABORT;` |
|        - | 10818 | `	}` |
|        - | 10819 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|       77 | 10820 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|      ! 0 | 10821 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|      ! 0 | 10822 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 10823 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10824 | `			return SXERR_ABORT;` |
|        - | 10825 | `		}` |
|      ! 0 | 10826 | `		return SXRET_OK;` |
|        - | 10827 | `	}` |
|       77 | 10828 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|       77 | 10829 | `	pEnd = 0;` |
|       77 | 10830 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|       77 | 10831 | `	if( pEnd >= pGen->pEnd ){` |
|      ! 0 | 10832 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|      ! 0 | 10833 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 10834 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10835 | `			return SXERR_ABORT;` |
|        - | 10836 | `		}` |
|      ! 0 | 10837 | `		return SXRET_OK;` |
|        - | 10838 | `	}` |
|        - | 10839 | `	/* The delimiter token is the trait body's closing brace */` |
|       77 | 10840 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 10841 | `	/* Swap token stream */` |
|       77 | 10842 | `	pTmp = pGen->pEnd;` |
|       77 | 10843 | `	pGen->pEnd = pEnd;` |
|        - | 10844 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|       77 | 10845 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|        - | 10846 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|       71 | 10847 | `	for(;;){` |
|      191 | 10848 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       28 | 10849 | `			pGen->pIn++;` |
|        4 | 10850 | `		}` |
|      167 | 10851 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       77 | 10852 | `			break;` |
|        - | 10853 | `		}` |
|        - | 10854 | `		/* Bind a directly-preceding docblock to this member */` |
|       95 | 10855 | `		GenStateSetPendingDoc(&(*pGen));` |
|       95 | 10856 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|      ! 0 | 10857 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10858 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|      ! 0 | 10859 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 10860 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 10861 | `				return SXERR_ABORT;` |
|        - | 10862 | `			}` |
|      ! 0 | 10863 | `			goto done;` |
|        - | 10864 | `		}` |
|       95 | 10865 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|       95 | 10866 | `		iAttrflags = 0;` |
|       95 | 10867 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       95 | 10868 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       95 | 10869 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - | 10870 | `				/* Trait uses another trait: use OtherTrait; */` |
|        5 | 10871 | `				pGen->pIn++; /* Jump 'use' */` |
|        2 | 10872 | `				for(;;){` |
|        - | 10873 | `					ph7_class *pUsedTrait;` |
|        - | 10874 | `					SyString *pUsedName;` |
|        5 | 10875 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 10876 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 10877 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|      ! 0 | 10878 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10879 | `							return SXERR_ABORT;` |
|        - | 10880 | `						}` |
|      ! 0 | 10881 | `						break;` |
|        - | 10882 | `					}` |
|        5 | 10883 | `					pUsedName = &pGen->pIn->sData;` |
|        - | 10884 | `					{` |
|        - | 10885 | `						SyBlob sResolved;` |
|        5 | 10886 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|        5 | 10887 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|        7 | 10888 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|        4 | 10889 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|        5 | 10890 | `						SyBlobRelease(&sResolved);` |
|        - | 10891 | `					}` |
|        5 | 10892 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 | 10893 | `						pUsedTrait = pUsedTrait->pNextName;` |
|      ! 0 | 10894 | `					}` |
|        5 | 10895 | `					if( pUsedTrait == 0 ){` |
|        4 | 10896 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        1 | 10897 | `							"'%z' is not a trait",pUsedName);` |
|        3 | 10898 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10899 | `							return SXERR_ABORT;` |
|        - | 10900 | `						}` |
|        2 | 10901 | `					}else{` |
|        3 | 10902 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|        - | 10903 | `					}` |
|        5 | 10904 | `					pGen->pIn++;` |
|        5 | 10905 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|        3 | 10906 | `						break;` |
|        - | 10907 | `					}` |
|      ! 0 | 10908 | `					pGen->pIn++;` |
|      ! 0 | 10909 | `				}` |
|        5 | 10910 | `				continue;` |
|        - | 10911 | `			}` |
|       91 | 10912 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       77 | 10913 | `				iProtection = nKwrd;` |
|       77 | 10914 | `				pGen->pIn++;` |
|       72 | 10915 | `				if( pGen->pIn >= pGen->pEnd` |
|       77 | 10916 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 10917 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10918 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|      ! 0 | 10919 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 10920 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10921 | `						return SXERR_ABORT;` |
|        - | 10922 | `					}` |
|      ! 0 | 10923 | `					goto done;` |
|        - | 10924 | `				}` |
|       77 | 10925 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       12 | 10926 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       12 | 10927 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 10928 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10929 | `							return SXERR_ABORT;` |
|        - | 10930 | `						}` |
|      ! 0 | 10931 | `						goto done;` |
|        - | 10932 | `					}` |
|       12 | 10933 | `					continue;` |
|        - | 10934 | `				}` |
|       67 | 10935 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        5 | 10936 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        5 | 10937 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 10938 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10939 | `							return SXERR_ABORT;` |
|        - | 10940 | `						}` |
|      ! 0 | 10941 | `						goto done;` |
|        - | 10942 | `					}` |
|        5 | 10943 | `					continue;` |
|        - | 10944 | `				}` |
|       63 | 10945 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       29 | 10946 | `			}` |
|       77 | 10947 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|      ! 0 | 10948 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10949 | `					"Traits cannot have constants");` |
|      ! 0 | 10950 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10951 | `					return SXERR_ABORT;` |
|        - | 10952 | `				}` |
|      ! 0 | 10953 | `				goto done;` |
|      ! 0 | 10954 | `			}else{` |
|       77 | 10955 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|        8 | 10956 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|        8 | 10957 | `					pGen->pIn++;` |
|        8 | 10958 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        6 | 10959 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        6 | 10960 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      ! 0 | 10961 | `							iProtection = nKwrd;` |
|      ! 0 | 10962 | `							pGen->pIn++;` |
|      ! 0 | 10963 | `						}` |
|        2 | 10964 | `					}` |
|        6 | 10965 | `					if( pGen->pIn >= pGen->pEnd` |
|        8 | 10966 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 10967 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10968 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|      ! 0 | 10969 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 10970 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10971 | `							return SXERR_ABORT;` |
|        - | 10972 | `						}` |
|      ! 0 | 10973 | `						goto done;` |
|        - | 10974 | `					}` |
|        8 | 10975 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        3 | 10976 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        3 | 10977 | `						if( rc != SXRET_OK ){` |
|      ! 0 | 10978 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 10979 | `								return SXERR_ABORT;` |
|        - | 10980 | `							}` |
|      ! 0 | 10981 | `							goto done;` |
|        - | 10982 | `						}` |
|        3 | 10983 | `						continue;` |
|        - | 10984 | `					}` |
|        6 | 10985 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|      ! 0 | 10986 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 10987 | `						if( rc != SXRET_OK ){` |
|      ! 0 | 10988 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 10989 | `								return SXERR_ABORT;` |
|        - | 10990 | `							}` |
|      ! 0 | 10991 | `							goto done;` |
|        - | 10992 | `						}` |
|      ! 0 | 10993 | `						continue;` |
|        - | 10994 | `					}` |
|        6 | 10995 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       73 | 10996 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|        6 | 10997 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|        6 | 10998 | `					pGen->pIn++;` |
|        6 | 10999 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        6 | 11000 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        6 | 11001 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        6 | 11002 | `							iProtection = nKwrd;` |
|        6 | 11003 | `							pGen->pIn++;` |
|        2 | 11004 | `						}` |
|        2 | 11005 | `					}` |
|        6 | 11006 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|        4 | 11007 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 11008 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11009 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|      ! 0 | 11010 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 11011 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11012 | `							return SXERR_ABORT;` |
|        - | 11013 | `						}` |
|      ! 0 | 11014 | `						goto done;` |
|        - | 11015 | `					}` |
|        6 | 11016 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|        2 | 11017 | `				}` |
|       75 | 11018 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|      ! 0 | 11019 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11020 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|      ! 0 | 11021 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 11022 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 11023 | `						return SXERR_ABORT;` |
|        - | 11024 | `					}` |
|      ! 0 | 11025 | `					goto done;` |
|        - | 11026 | `				}` |
|       75 | 11027 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|      ! 0 | 11028 | `					pGen->pIn++;` |
|      ! 0 | 11029 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 11030 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11031 | `							"Expecting attribute declaration after 'var' keyword");` |
|      ! 0 | 11032 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11033 | `							return SXERR_ABORT;` |
|        - | 11034 | `						}` |
|      ! 0 | 11035 | `						goto done;` |
|        - | 11036 | `					}` |
|      ! 0 | 11037 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 11038 | `				}else{` |
|       75 | 11039 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|        - | 11040 | `				}` |
|       75 | 11041 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 11042 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 11043 | `						return SXERR_ABORT;` |
|        - | 11044 | `					}` |
|      ! 0 | 11045 | `					goto done;` |
|        - | 11046 | `				}` |
|        - | 11047 | `			}` |
|       40 | 11048 | `		}else{` |
|      ! 0 | 11049 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 11050 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 11051 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 11052 | `					return SXERR_ABORT;` |
|        - | 11053 | `				}` |
|      ! 0 | 11054 | `				goto done;` |
|        - | 11055 | `			}` |
|        - | 11056 | `		}` |
|        5 | 11057 | `	}` |
|        - | 11058 | `	/* Install the trait */` |
|       77 | 11059 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|       77 | 11060 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11061 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 11062 | `		return SXERR_ABORT;` |
|        - | 11063 | `	}` |
|       36 | 11064 | `done:` |
|        - | 11065 | `	/* Point beyond the trait body */` |
|       77 | 11066 | `	pGen->pIn = &pEnd[1];` |
|       77 | 11067 | `	pGen->pEnd = pTmp;` |
|       77 | 11068 | `	return PH7_OK;` |
|       41 | 11069 | `}` |
|        - | 11070 | `/*` |
|        - | 11071 | ` * Compile a user-defined class.` |
|        - | 11072 | ` *  According to the PHP language reference manual` |
|        - | 11073 | ` *   Basic class definitions begin with the keyword class, followed` |
|        - | 11074 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|        - | 11075 | ` *   the definitions of the properties and methods belonging to the class.` |
|        - | 11076 | ` *   A class may contain its own constants, variables (called "properties")` |
|        - | 11077 | ` *   and functions (called "methods").` |
|        - | 11078 | ` */` |
|   187928 | 11079 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|        5 | 11080 | `{` |
|        - | 11081 | `	sxi32 rc;` |
|   187933 | 11082 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   187933 | 11083 | `	return rc;` |
|        5 | 11084 | `}` |
|        - | 11085 | `/*` |
|        - | 11086 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|        - | 11087 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|        - | 11088 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|        - | 11089 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|        - | 11090 | `` * meaning; `enum Name` can never start a valid expression.`` |
|        - | 11091 | ` */` |
|  6192448 | 11092 | `static int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|        5 | 11093 | `{` |
|  6225721 | 11094 | `	return (pIn->nType & PH7_TK_ID)` |
|  3129492 | 11095 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|    37273 | 11096 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
|  6225716 | 11097 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|        5 | 11098 | `}` |
|        - | 11099 | `/*` |
|        - | 11100 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|        - | 11101 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|        - | 11102 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|        - | 11103 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|        - | 11104 | ` */` |
|       28 | 11105 | `static sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|        5 | 11106 | `{` |
|       33 | 11107 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|        5 | 11108 | `}` |
|        - | 11109 | `/*` |
|        - | 11110 | ` * Exception handling.` |
|        - | 11111 | ` *  According to the PHP language reference manual` |
|        - | 11112 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|        - | 11113 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|        - | 11114 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|        - | 11115 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|        - | 11116 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|        - | 11117 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|        - | 11118 | ` *    (or re-thrown) within a catch block.` |
|        - | 11119 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|        - | 11120 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|        - | 11121 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|        - | 11122 | ` *    been defined with set_exception_handler().` |
|        - | 11123 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|        - | 11124 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|        - | 11125 | ` */` |
|        - | 11126 | `/*` |
|        - | 11127 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|        - | 11128 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|        - | 11129 | ` * indicates failure.` |
|        - | 11130 | ` */` |
|   315008 | 11131 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 11132 | `{` |
|   315013 | 11133 | `	sxi32 rc = SXRET_OK;` |
|   315013 | 11134 | `	if( pRoot->pOp ){` |
|   315001 | 11135 | `		switch( pRoot->pOp->iOp ){` |
|   157498 | 11136 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|        - | 11137 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|        - | 11138 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|        - | 11139 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|        - | 11140 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|        - | 11141 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|   315001 | 11142 | `			break;` |
|      ! 0 | 11143 | `		default:` |
|        - | 11144 | `			/* Runtime will still reject non-Throwable values; the set above` |
|        - | 11145 | `			 * covers the common shapes and gives a friendlier compile error` |
|        - | 11146 | ``			 * for obvious mistakes like `throw 5`. */`` |
|      ! 0 | 11147 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 11148 | `				"throw: Expecting an exception class instance");` |
|      ! 0 | 11149 | `			if( rc != SXERR_ABORT ){` |
|      ! 0 | 11150 | `				rc = SXERR_INVALID;` |
|      ! 0 | 11151 | `			}` |
|      ! 0 | 11152 | `			break;` |
|        - | 11153 | `		}` |
|   157515 | 11154 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 11155 | `		/* Unexpected expression */` |
|      ! 0 | 11156 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 11157 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 11158 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 | 11159 | `			rc = SXERR_INVALID;` |
|      ! 0 | 11160 | `		}` |
|      ! 0 | 11161 | `	}` |
|   315013 | 11162 | `	return rc;` |
|        5 | 11163 | `}` |
|        - | 11164 | `/*` |
|        - | 11165 | ` * Compile a 'throw' statement.` |
|        - | 11166 | ` * throw: This is how you trigger an exception.` |
|        - | 11167 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|        - | 11168 | ` */` |
|   314972 | 11169 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|        5 | 11170 | `{` |
|   314977 | 11171 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11172 | `	GenBlock *pBlock;` |
|        - | 11173 | `	sxu32 nIdx;` |
|        - | 11174 | `	sxi32 rc;` |
|   314977 | 11175 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|        - | 11176 | `	/* Compile the expression */` |
|   314977 | 11177 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   314977 | 11178 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 11179 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|      ! 0 | 11180 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11181 | `			return SXERR_ABORT;` |
|        - | 11182 | `		}` |
|      ! 0 | 11183 | `		return SXRET_OK;` |
|        - | 11184 | `	}` |
|   314977 | 11185 | `	pBlock = pGen->pCurrent;` |
|        - | 11186 | `	/* Point to the top most function or try block and emit the forward jump */` |
|  1228101 | 11187 | `	while(pBlock->pParent){` |
|  1228097 | 11188 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   314973 | 11189 | `			break;` |
|        - | 11190 | `		}` |
|        - | 11191 | `		/* Point to the parent block */` |
|   913129 | 11192 | `		pBlock = pBlock->pParent;` |
|        5 | 11193 | `	}` |
|        - | 11194 | `	/* Emit the throw instruction */` |
|   314977 | 11195 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        - | 11196 | `	/* Emit the jump */` |
|   314977 | 11197 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   314977 | 11198 | `	return SXRET_OK;` |
|   157491 | 11199 | `}` |
|        - | 11200 | `/*` |
|        - | 11201 | ` * Compile a PHP 8.0 'throw' expression.` |
|        - | 11202 | ` * Called from the expression code generator when a 'throw' keyword is` |
|        - | 11203 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|        - | 11204 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|        - | 11205 | ` * the validator guarantees the operand is a valid exception target.` |
|        - | 11206 | ` */` |
|       36 | 11207 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        2 | 11208 | `{` |
|       38 | 11209 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11210 | `	GenBlock *pBlock;` |
|        - | 11211 | `	sxu32 nIdx;` |
|        - | 11212 | `	sxi32 rc;` |
|       18 | 11213 | `	(void)iCompileFlag;` |
|       38 | 11214 | `	pGen->pIn++; /* Skip 'throw' */` |
|       38 | 11215 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 11216 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 11217 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 11218 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11219 | `			return SXERR_ABORT;` |
|        - | 11220 | `		}` |
|      ! 0 | 11221 | `		return SXRET_OK;` |
|        - | 11222 | `	}` |
|       38 | 11223 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|       38 | 11224 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 11225 | `		return SXERR_ABORT;` |
|        - | 11226 | `	}` |
|       38 | 11227 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 11228 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 11229 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 11230 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11231 | `			return SXERR_ABORT;` |
|        - | 11232 | `		}` |
|      ! 0 | 11233 | `		return SXRET_OK;` |
|        - | 11234 | `	}` |
|        - | 11235 | `	/* Walk up to nearest exception/function block for the jump target */` |
|       38 | 11236 | `	pBlock = pGen->pCurrent;` |
|       60 | 11237 | `	while( pBlock->pParent ){` |
|       49 | 11238 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|       27 | 11239 | `			break;` |
|        - | 11240 | `		}` |
|       23 | 11241 | `		pBlock = pBlock->pParent;` |
|        1 | 11242 | `	}` |
|       38 | 11243 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       38 | 11244 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|       38 | 11245 | `	return SXRET_OK;` |
|       20 | 11246 | `}` |
|        - | 11247 | `/*` |
|        - | 11248 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|        - | 11249 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|        - | 11250 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|        - | 11251 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|        - | 11252 | ` * compile error propagated from the parser.` |
|        - | 11253 | ` */` |
|       54 | 11254 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|        5 | 11255 | `{` |
|        - | 11256 | `	SyString sClassName;` |
|        - | 11257 | `	SyToken *pToken;` |
|        - | 11258 | `	SyString *pName;` |
|        - | 11259 | `	char *zDup;` |
|        - | 11260 | `	sxi32 rc;` |
|       59 | 11261 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       59 | 11262 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|       59 | 11263 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|       59 | 11264 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       59 | 11265 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 | 11266 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 11267 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11268 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11269 | `		return SXERR_INVALID;` |
|        - | 11270 | `	}` |
|       59 | 11271 | `	pGen->pIn++; /* '(' */` |
|       27 | 11272 | `	for(;;){` |
|        - | 11273 | `		SyBlob sResolved;` |
|       59 | 11274 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       59 | 11275 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 11276 | `			SyBlobRelease(&sResolved);` |
|      ! 0 | 11277 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 11278 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11279 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11280 | `			return SXERR_INVALID;` |
|        - | 11281 | `		}` |
|       86 | 11282 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       54 | 11283 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       59 | 11284 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|       59 | 11285 | `		SyBlobRelease(&sResolved);` |
|       59 | 11286 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|       59 | 11287 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|       59 | 11288 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       54 | 11289 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|        5 | 11290 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      ! 0 | 11291 | `			pGen->pIn++; continue;` |
|        - | 11292 | `		}` |
|       59 | 11293 | `		break;` |
|      ! 0 | 11294 | `	}` |
|       54 | 11295 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|       59 | 11296 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 11297 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 11298 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11299 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11300 | `		return SXERR_INVALID;` |
|        - | 11301 | `	}` |
|       59 | 11302 | `	pGen->pIn++; /* '$' */` |
|       59 | 11303 | `	pName = &pGen->pIn->sData;` |
|       59 | 11304 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       59 | 11305 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|       59 | 11306 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|       59 | 11307 | `	pGen->pIn++;` |
|       59 | 11308 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 11309 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 11310 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11311 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11312 | `		return SXERR_INVALID;` |
|        - | 11313 | `	}` |
|       59 | 11314 | `	pGen->pIn++; /* ')' */` |
|       59 | 11315 | `	return SXRET_OK;` |
|       32 | 11316 | `}` |
|        - | 11317 | `/*` |
|        - | 11318 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|        - | 11319 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|        - | 11320 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|        - | 11321 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|        - | 11322 | ` * VmThrowException):` |
|        - | 11323 | ` *` |
|        - | 11324 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|        - | 11325 | ` *    <try body>` |
|        - | 11326 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|        - | 11327 | ` *    JMP  -> finally\|end` |
|        - | 11328 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|        - | 11329 | ` *    <catch body>` |
|        - | 11330 | ` *    JMP  -> finally\|end` |
|        - | 11331 | ` *    ... more catches ...` |
|        - | 11332 | ` *  Lfin: <finally body>` |
|        - | 11333 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|        - | 11334 | ` *  Lend:` |
|        - | 11335 | ` */` |
|       98 | 11336 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|        5 | 11337 | `{` |
|      103 | 11338 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11339 | `	GenBlock *pTry;` |
|        - | 11340 | `	VmInstr *pInstr;` |
|      103 | 11341 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|        - | 11342 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|        - | 11343 | `	sxi32 rc;` |
|      103 | 11344 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|        - | 11345 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|      103 | 11346 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      103 | 11347 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      103 | 11348 | `	pTry->pUserData = pException;` |
|      103 | 11349 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|      103 | 11350 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|      103 | 11351 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      103 | 11352 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      103 | 11353 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|      103 | 11354 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|        - | 11355 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|      103 | 11356 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|      103 | 11357 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|      103 | 11358 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      103 | 11359 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 11360 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|      103 | 11361 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|        - | 11362 | `	/* Catch clauses (inline) */` |
|      103 | 11363 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       98 | 11364 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       59 | 11365 | `		sxu32 k = 0;` |
|       81 | 11366 | `		for(;;){` |
|        - | 11367 | `			ph7_exception_block sCatch;` |
|        - | 11368 | `			GenBlock *pCatchBlk;` |
|      113 | 11369 | `			sxu32 idxJmp = 0;` |
|      108 | 11370 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      104 | 11371 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|       32 | 11372 | `				break;` |
|        - | 11373 | `			}` |
|       59 | 11374 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|       59 | 11375 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       59 | 11376 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|       59 | 11377 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|       59 | 11378 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|       59 | 11379 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|       59 | 11380 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        - | 11381 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|        - | 11382 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|        - | 11383 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|       59 | 11384 | `			pCatchBlk->pUserData = pException;` |
|       59 | 11385 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|       59 | 11386 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       59 | 11387 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       59 | 11388 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        - | 11389 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|        - | 11390 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|       59 | 11391 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       59 | 11392 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|       59 | 11393 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|       59 | 11394 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|       59 | 11395 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       59 | 11396 | `			k++;` |
|        5 | 11397 | `		}` |
|       27 | 11398 | `	}` |
|        - | 11399 | `	/* Finally (inline) */` |
|      103 | 11400 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       80 | 11401 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 11402 | `		GenBlock *pFinBlk;` |
|       52 | 11403 | `		pGen->pIn++; /* Jump 'finally' */` |
|       52 | 11404 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|       52 | 11405 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|       52 | 11406 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       52 | 11407 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       52 | 11408 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       52 | 11409 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       52 | 11410 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       52 | 11411 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|       52 | 11412 | `		pException->iHasFinally = 1;` |
|       24 | 11413 | `	}` |
|      103 | 11414 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|      103 | 11415 | `	pException->iInlined = 1;` |
|        - | 11416 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|        - | 11417 | `	{` |
|      103 | 11418 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|        - | 11419 | `		sxu32 *aJ; sxu32 n;` |
|      103 | 11420 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|      103 | 11421 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      103 | 11422 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|      157 | 11423 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|       59 | 11424 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|       59 | 11425 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       32 | 11426 | `		}` |
|        - | 11427 | `	}` |
|      103 | 11428 | `	SySetRelease(&aCatchJmp);` |
|      103 | 11429 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|      ! 0 | 11430 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|      ! 0 | 11431 | `	}` |
|      103 | 11432 | `	return SXRET_OK;` |
|       54 | 11433 | `}` |
|        - | 11434 | `/*` |
|        - | 11435 | ` * Compile a 'catch' block.` |
|        - | 11436 | ` * Catch: A "catch" block retrieves an exception and creates` |
|        - | 11437 | ` * an object containing the exception information.` |
|        - | 11438 | ` */` |
|     5200 | 11439 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|        5 | 11440 | `{` |
|     5205 | 11441 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11442 | `	ph7_exception_block sCatch;` |
|        - | 11443 | `	SySet *pInstrContainer;` |
|        - | 11444 | `	SyString sClassName;` |
|        - | 11445 | `	GenBlock *pCatch;` |
|        - | 11446 | `	SyToken *pToken;` |
|        - | 11447 | `	SyString *pName;` |
|        - | 11448 | `	char *zDup;` |
|        - | 11449 | `	sxi32 rc;` |
|     5205 | 11450 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        - | 11451 | `	/* Zero the structure */` |
|     5205 | 11452 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|        - | 11453 | `	/* Initialize fields */` |
|     5205 | 11454 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     5205 | 11455 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     5205 | 11456 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|        - | 11457 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 11458 | `			pToken = pGen->pIn;` |
|      ! 0 | 11459 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 11460 | `				pToken--;` |
|      ! 0 | 11461 | `			}` |
|      ! 0 | 11462 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 11463 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11464 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11465 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11466 | `				return SXERR_ABORT;` |
|        - | 11467 | `			}` |
|      ! 0 | 11468 | `			return SXERR_INVALID;` |
|        - | 11469 | `	}` |
|        - | 11470 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     5205 | 11471 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     2614 | 11472 | `	for(;;){` |
|        - | 11473 | `		SyBlob sResolved;` |
|     5233 | 11474 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     5233 | 11475 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        6 | 11476 | `			SyBlobRelease(&sResolved);` |
|        6 | 11477 | `			pToken = pGen->pIn;` |
|        6 | 11478 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 11479 | `				pToken--;` |
|      ! 0 | 11480 | `			}` |
|        8 | 11481 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 11482 | `				"syntax error, unexpected %s \"%z\"",` |
|        2 | 11483 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|        6 | 11484 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11485 | `				return SXERR_ABORT;` |
|        - | 11486 | `			}` |
|        6 | 11487 | `			return SXERR_INVALID;` |
|        - | 11488 | `		}` |
|        - | 11489 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|        - | 11490 | `		 * transient SyBlob allocation. */` |
|     7841 | 11491 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     5224 | 11492 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     5229 | 11493 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     5229 | 11494 | `		SyBlobRelease(&sResolved);` |
|     5229 | 11495 | `		if( zDup == 0 ){` |
|      ! 0 | 11496 | `			goto Mem;` |
|        - | 11497 | `		}` |
|     5229 | 11498 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     5229 | 11499 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 11500 | `			goto Mem;` |
|        - | 11501 | `		}` |
|        - | 11502 | `		/* Check for '\|' (multi-catch separator) */` |
|     5224 | 11503 | `		if( pGen->pIn < pGen->pEnd &&` |
|     5224 | 11504 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|       33 | 11505 | `			pGen->pIn->sData.nByte == 1 &&` |
|       28 | 11506 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|       30 | 11507 | `			pGen->pIn++; /* Consume the '\|' */` |
|       30 | 11508 | `			continue;` |
|        - | 11509 | `		}` |
|     5201 | 11510 | `		break;` |
|      ! 0 | 11511 | `	}` |
|     5196 | 11512 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     5201 | 11513 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 11514 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 11515 | `			pToken = pGen->pIn;` |
|      ! 0 | 11516 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 11517 | `				pToken--;` |
|      ! 0 | 11518 | `			}` |
|      ! 0 | 11519 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 11520 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11521 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11522 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11523 | `				return SXERR_ABORT;` |
|        - | 11524 | `			}` |
|      ! 0 | 11525 | `			return SXERR_INVALID;` |
|        - | 11526 | `	}` |
|     5201 | 11527 | `	pGen->pIn++; /* Jump the dollar sign */` |
|        - | 11528 | `	/* Duplicate instance name */` |
|     5201 | 11529 | `	pName = &pGen->pIn->sData;` |
|     5201 | 11530 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     5201 | 11531 | `	if( zDup == 0 ){` |
|      ! 0 | 11532 | `		goto Mem;` |
|        - | 11533 | `	}` |
|     5201 | 11534 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     5201 | 11535 | `	pGen->pIn++;` |
|     5201 | 11536 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|        - | 11537 | `		/* Unexpected token,break immediately */` |
|      ! 0 | 11538 | `		pToken = pGen->pIn;` |
|      ! 0 | 11539 | `		if( pToken >= pGen->pEnd ){` |
|      ! 0 | 11540 | `			pToken--;` |
|      ! 0 | 11541 | `		}` |
|      ! 0 | 11542 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 11543 | `			"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11544 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11545 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11546 | `			return SXERR_ABORT;` |
|        - | 11547 | `		}` |
|      ! 0 | 11548 | `		return SXERR_INVALID;` |
|        - | 11549 | `	}` |
|        - | 11550 | `	/* Compile the block */` |
|     5201 | 11551 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|        - | 11552 | `	/* Create the catch block */` |
|     5201 | 11553 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     5201 | 11554 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11555 | `		return SXERR_ABORT;` |
|        - | 11556 | `	}` |
|        - | 11557 | `	/* Swap bytecode container */` |
|     5201 | 11558 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     5201 | 11559 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|        - | 11560 | `	/* Compile the block */` |
|     5201 | 11561 | `	PH7_CompileBlock(&(*pGen),0);` |
|        - | 11562 | `	/* Fix forward jumps now the destination is resolved  */` |
|     5201 | 11563 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 11564 | `	/* Emit the DONE instruction */` |
|     5201 | 11565 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 11566 | `	/* Leave the block */` |
|     5201 | 11567 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 11568 | `	/* Restore the default container */` |
|     5201 | 11569 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 11570 | `	/* Install the catch block */` |
|     5201 | 11571 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     5201 | 11572 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11573 | `		goto Mem;` |
|        - | 11574 | `	}` |
|     5201 | 11575 | `	return SXRET_OK;` |
|      ! 0 | 11576 | `Mem:` |
|      ! 0 | 11577 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 11578 | `	return SXERR_ABORT;` |
|     2605 | 11579 | `}` |
|        - | 11580 | `/*` |
|        - | 11581 | ` * Compile a 'try' block.` |
|        - | 11582 | ` * A function using an exception should be in a "try" block.` |
|        - | 11583 | ` * If the exception does not trigger, the code will continue` |
|        - | 11584 | ` * as normal. However if the exception triggers, an exception` |
|        - | 11585 | ` * is "thrown".` |
|        - | 11586 | ` */` |
|     5356 | 11587 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|        5 | 11588 | `{` |
|        - | 11589 | `	ph7_exception *pException;` |
|     5361 | 11590 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11591 | `	GenBlock *pTry;` |
|        - | 11592 | `	sxu32 nJmpIdx;` |
|        - | 11593 | `	sxi32 rc;` |
|        - | 11594 | `	/* Create the exception container */` |
|     5361 | 11595 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     5361 | 11596 | `	if( pException == 0 ){` |
|      ! 0 | 11597 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 11598 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 11599 | `		return SXERR_ABORT;` |
|        - | 11600 | `	}` |
|        - | 11601 | `	/* Zero the structure */` |
|     5361 | 11602 | `	SyZero(pException,sizeof(ph7_exception));` |
|        - | 11603 | `	/* Initialize fields */` |
|     5361 | 11604 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     5361 | 11605 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     5361 | 11606 | `	pException->iHasFinally = 0;` |
|     5361 | 11607 | `	pException->iFinallyDone = 0;` |
|     5361 | 11608 | `	pException->pVm = pGen->pVm;` |
|        - | 11609 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|        - | 11610 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|        - | 11611 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|        - | 11612 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|        - | 11613 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|        - | 11614 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|     5361 | 11615 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|      103 | 11616 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|        - | 11617 | `	}` |
|        - | 11618 | `	/* Create the try block */` |
|     5263 | 11619 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     5263 | 11620 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11621 | `		return SXERR_ABORT;` |
|        - | 11622 | `	}` |
|        - | 11623 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     5263 | 11624 | `	pTry->pUserData = pException;` |
|        - | 11625 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     5263 | 11626 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|        - | 11627 | `	/* Fix the jump later when the destination is resolved */` |
|     5263 | 11628 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     5263 | 11629 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|        - | 11630 | `	/* Compile the block */` |
|     5263 | 11631 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     5263 | 11632 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 11633 | `		return SXERR_ABORT;` |
|        - | 11634 | `	}` |
|        - | 11635 | `	/* Fix forward jumps now the destination is resolved */` |
|     5263 | 11636 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 11637 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     5263 | 11638 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        - | 11639 | `	/* Leave the block */` |
|     5263 | 11640 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 11641 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     5263 | 11642 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     5256 | 11643 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        - | 11644 | `		/* Compile one or more catch blocks */` |
|     5196 | 11645 | `		for(;;){` |
|    10392 | 11646 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     7830 | 11647 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     2601 | 11648 | `					break;` |
|        - | 11649 | `			}` |
|     5205 | 11650 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     5205 | 11651 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11652 | `				return SXERR_ABORT;` |
|        - | 11653 | `			}` |
|        5 | 11654 | `		}` |
|     2596 | 11655 | `	}` |
|        - | 11656 | `	/* Compile optional finally block */` |
|     5263 | 11657 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      644 | 11658 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 11659 | `		SySet *pInstrContainer;` |
|        - | 11660 | `		GenBlock *pFinBlock;` |
|      129 | 11661 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|        - | 11662 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      129 | 11663 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      129 | 11664 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 11665 | `			return SXERR_ABORT;` |
|        - | 11666 | `		}` |
|        - | 11667 | `		/* Swap bytecode container */` |
|      129 | 11668 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      129 | 11669 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|        - | 11670 | `		/* Compile the finally body */` |
|      129 | 11671 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      129 | 11672 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11673 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 | 11674 | `			return SXERR_ABORT;` |
|        - | 11675 | `		}` |
|        - | 11676 | `		/* Fix forward jumps now the destination is resolved */` |
|      129 | 11677 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 11678 | `		/* Emit DONE to terminate the finally block */` |
|      129 | 11679 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 11680 | `		/* Leave the block */` |
|      129 | 11681 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        - | 11682 | `		/* Restore the default container */` |
|      129 | 11683 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      129 | 11684 | `		pException->iHasFinally = 1;` |
|       62 | 11685 | `	}` |
|        - | 11686 | `	/* Must have at least one catch or finally */` |
|     5263 | 11687 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|        8 | 11688 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 11689 | `			"Cannot use try without catch or finally");` |
|        8 | 11690 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11691 | `			return SXERR_ABORT;` |
|        - | 11692 | `		}` |
|        3 | 11693 | `	}` |
|     5263 | 11694 | `	return SXRET_OK;` |
|     2683 | 11695 | `}` |
|        - | 11696 | `/*` |
|        - | 11697 | ` * Compile a switch block.` |
|        - | 11698 | ` *  (See block-comment below for more information)` |
|        - | 11699 | ` */` |
|      112 | 11700 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|        5 | 11701 | `{` |
|      117 | 11702 | `	sxi32 rc = SXRET_OK;` |
|      117 | 11703 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|        - | 11704 | `		/* Unexpected token */` |
|      ! 0 | 11705 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 11706 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11707 | `			return SXERR_ABORT;` |
|        - | 11708 | `		}` |
|      ! 0 | 11709 | `		pGen->pIn++;` |
|      ! 0 | 11710 | `	}` |
|      117 | 11711 | `	pGen->pIn++;` |
|        - | 11712 | `	/* First instruction to execute in this block. */` |
|      117 | 11713 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|        - | 11714 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|        - | 11715 | `	 * or the '}' token */` |
|      206 | 11716 | `	for(;;){` |
|      417 | 11717 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 11718 | `			/* No more input to process */` |
|      ! 0 | 11719 | `			break;` |
|        - | 11720 | `		}` |
|      417 | 11721 | `		rc = SXRET_OK;` |
|      417 | 11722 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       85 | 11723 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|       31 | 11724 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|        - | 11725 | `					/* Unexpected token */` |
|      ! 0 | 11726 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 11727 | `						&pGen->pIn->sData);` |
|      ! 0 | 11728 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 11729 | `						return SXERR_ABORT;` |
|        - | 11730 | `					}` |
|        - | 11731 | `					/* FALL THROUGH */` |
|      ! 0 | 11732 | `				}` |
|       31 | 11733 | `				rc = SXERR_EOF;` |
|       31 | 11734 | `				break;` |
|        - | 11735 | `			}` |
|       32 | 11736 | `		}else{` |
|        - | 11737 | `			sxi32 nKwrd;` |
|        - | 11738 | `			/* Extract the keyword */` |
|      337 | 11739 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      337 | 11740 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|       47 | 11741 | `				break;` |
|        - | 11742 | `			}` |
|      253 | 11743 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 11744 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|        - | 11745 | `					/* Unexpected token */` |
|      ! 0 | 11746 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 11747 | `						&pGen->pIn->sData);` |
|      ! 0 | 11748 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 11749 | `						return SXERR_ABORT;` |
|        - | 11750 | `					}` |
|        - | 11751 | `					/* FALL THROUGH */` |
|      ! 0 | 11752 | `				}` |
|        - | 11753 | `				/* Block compiled */` |
|        3 | 11754 | `				break;` |
|        - | 11755 | `			}` |
|        - | 11756 | `		}` |
|        - | 11757 | `		/* Compile block */` |
|      305 | 11758 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      305 | 11759 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11760 | `			return SXERR_ABORT;` |
|        - | 11761 | `		}` |
|        5 | 11762 | `	}` |
|      117 | 11763 | `	return rc;` |
|       61 | 11764 | `}` |
|        - | 11765 | `/*` |
|        - | 11766 | ` * Compile a case eXpression.` |
|        - | 11767 | ` *  (See block-comment below for more information)` |
|        - | 11768 | ` */` |
|       92 | 11769 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|        5 | 11770 | `{` |
|        - | 11771 | `	SySet *pInstrContainer;` |
|        - | 11772 | `	SyToken *pEnd,*pTmp;` |
|       97 | 11773 | `	sxi32 iNest = 0;` |
|        - | 11774 | `	sxi32 rc;` |
|        - | 11775 | `	/* Delimit the expression */` |
|       97 | 11776 | `	pEnd = pGen->pIn;` |
|      197 | 11777 | `	while( pEnd < pGen->pEnd ){` |
|      197 | 11778 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|        - | 11779 | `			/* Increment nesting level */` |
|        3 | 11780 | `			iNest++;` |
|      196 | 11781 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|        - | 11782 | `			/* Decrement nesting level */` |
|        3 | 11783 | `			iNest--;` |
|      194 | 11784 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|       97 | 11785 | `			break;` |
|        - | 11786 | `		}` |
|      105 | 11787 | `		pEnd++;` |
|        5 | 11788 | `	}` |
|       97 | 11789 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 11790 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|      ! 0 | 11791 | `		if( rc == SXERR_ABORT ){` |
|        - | 11792 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 11793 | `			return SXERR_ABORT;` |
|        - | 11794 | `		}` |
|      ! 0 | 11795 | `	}` |
|        - | 11796 | `	/* Swap token stream */` |
|       97 | 11797 | `	pTmp = pGen->pEnd;` |
|       97 | 11798 | `	pGen->pEnd = pEnd;` |
|       97 | 11799 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       97 | 11800 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|       97 | 11801 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 11802 | `	/* Emit the done instruction */` |
|       97 | 11803 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       97 | 11804 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 11805 | `	/* Update token stream */` |
|       97 | 11806 | `	pGen->pIn  = pEnd;` |
|       97 | 11807 | `	pGen->pEnd = pTmp;` |
|       97 | 11808 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 11809 | `		return SXERR_ABORT;` |
|        - | 11810 | `	}` |
|       97 | 11811 | `	return SXRET_OK;` |
|       51 | 11812 | `}` |
|        - | 11813 | `/*` |
|        - | 11814 | ` * Compile the smart switch statement.` |
|        - | 11815 | ` * According to the PHP language reference manual` |
|        - | 11816 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|        - | 11817 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|        - | 11818 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|        - | 11819 | ` *  This is exactly what the switch statement is for.` |
|        - | 11820 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|        - | 11821 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|        - | 11822 | ` *  of the outer loop, use continue 2.` |
|        - | 11823 | ` *  Note that switch/case does loose comparision.` |
|        - | 11824 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|        - | 11825 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|        - | 11826 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|        - | 11827 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|        - | 11828 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|        - | 11829 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|        - | 11830 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|        - | 11831 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|        - | 11832 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|        - | 11833 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|        - | 11834 | ` *  list for the next case.` |
|        - | 11835 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|        - | 11836 | ` *  or floating-point numbers and strings.` |
|        - | 11837 | ` */` |
|       28 | 11838 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|        5 | 11839 | `{` |
|        - | 11840 | `	GenBlock *pSwitchBlock;` |
|        - | 11841 | `	SyToken *pTmp,*pEnd;` |
|        - | 11842 | `	ph7_switch *pSwitch;` |
|        - | 11843 | `	sxu32 nToken;` |
|        - | 11844 | `	sxu32 nLine;` |
|        - | 11845 | `	sxi32 rc;` |
|       33 | 11846 | `	nLine = pGen->pIn->nLine;` |
|        - | 11847 | `	/* Jump the 'switch' keyword */` |
|       33 | 11848 | `	pGen->pIn++;` |
|       33 | 11849 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 11850 | `		/* Syntax error */` |
|      ! 0 | 11851 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|      ! 0 | 11852 | `		if( rc == SXERR_ABORT ){` |
|        - | 11853 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 11854 | `			return SXERR_ABORT;` |
|        - | 11855 | `		}` |
|      ! 0 | 11856 | `		goto Synchronize;` |
|        - | 11857 | `	}` |
|        - | 11858 | `	/* Jump the left parenthesis '(' */` |
|       33 | 11859 | `	pGen->pIn++;` |
|       33 | 11860 | `	pEnd = 0; /* cc warning */` |
|        - | 11861 | `	/* Create the loop block */` |
|       47 | 11862 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|       14 | 11863 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|       33 | 11864 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11865 | `		return SXERR_ABORT;` |
|        - | 11866 | `	}` |
|        - | 11867 | `	/* Delimit the condition */` |
|       33 | 11868 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|       33 | 11869 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 11870 | `		/* Empty expression */` |
|      ! 0 | 11871 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|      ! 0 | 11872 | `		if( rc == SXERR_ABORT ){` |
|        - | 11873 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 11874 | `			return SXERR_ABORT;` |
|        - | 11875 | `		}` |
|      ! 0 | 11876 | `	}` |
|        - | 11877 | `	/* Swap token streams */` |
|       33 | 11878 | `	pTmp = pGen->pEnd;` |
|       33 | 11879 | `	pGen->pEnd = pEnd;` |
|        - | 11880 | `	/* Compile the expression */` |
|       33 | 11881 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       33 | 11882 | `	if( rc == SXERR_ABORT ){` |
|        - | 11883 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 11884 | `		return SXERR_ABORT;` |
|        - | 11885 | `	}` |
|        - | 11886 | `	/* Update token stream */` |
|       33 | 11887 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 | 11888 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 11889 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 11890 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11891 | `			return SXERR_ABORT;` |
|        - | 11892 | `		}` |
|      ! 0 | 11893 | `		pGen->pIn++;` |
|      ! 0 | 11894 | `	}` |
|       33 | 11895 | `	pGen->pIn  = &pEnd[1];` |
|       33 | 11896 | `	pGen->pEnd = pTmp;` |
|       33 | 11897 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       28 | 11898 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|      ! 0 | 11899 | `			pTmp = pGen->pIn;` |
|      ! 0 | 11900 | `			if( pTmp >= pGen->pEnd ){` |
|      ! 0 | 11901 | `				pTmp--;` |
|      ! 0 | 11902 | `			}` |
|        - | 11903 | `			/* Unexpected token */` |
|      ! 0 | 11904 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|      ! 0 | 11905 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11906 | `				return SXERR_ABORT;` |
|        - | 11907 | `			}` |
|      ! 0 | 11908 | `			goto Synchronize;` |
|        - | 11909 | `	}` |
|        - | 11910 | `	/* Set the delimiter token */` |
|       33 | 11911 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|        3 | 11912 | `		nToken = PH7_TK_KEYWORD;` |
|        - | 11913 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|        2 | 11914 | `	}else{` |
|       31 | 11915 | `		nToken = PH7_TK_CCB; /* '}' */` |
|        - | 11916 | `	}` |
|       33 | 11917 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|        - | 11918 | `	/* Create the switch blocks container */` |
|       33 | 11919 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|       33 | 11920 | `	if( pSwitch == 0 ){` |
|        - | 11921 | `		/* Abort compilation */` |
|      ! 0 | 11922 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 11923 | `		return SXERR_ABORT;` |
|        - | 11924 | `	}` |
|        - | 11925 | `	/* Zero the structure */` |
|       33 | 11926 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|        - | 11927 | `	/* Initialize fields */` |
|       33 | 11928 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|        - | 11929 | `	/* Emit the switch instruction */` |
|       33 | 11930 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|        - | 11931 | `	/* Compile case blocks */` |
|      100 | 11932 | `	for(;;){` |
|        - | 11933 | `		sxu32 nKwrd;` |
|      119 | 11934 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 11935 | `			/* No more input to process */` |
|      ! 0 | 11936 | `			break;` |
|        - | 11937 | `		}` |
|      119 | 11938 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 11939 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|        - | 11940 | `				/* Unexpected token */` |
|      ! 0 | 11941 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 11942 | `					&pGen->pIn->sData);` |
|      ! 0 | 11943 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 11944 | `					return SXERR_ABORT;` |
|        - | 11945 | `				}` |
|        - | 11946 | `				/* FALL THROUGH */` |
|      ! 0 | 11947 | `			}` |
|        - | 11948 | `			/* Block compiled */` |
|      ! 0 | 11949 | `			break;` |
|        - | 11950 | `		}` |
|        - | 11951 | `		/* Extract the keyword */` |
|      119 | 11952 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      119 | 11953 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 11954 | `			if( nToken != PH7_TK_KEYWORD ){` |
|        - | 11955 | `				/* Unexpected token */` |
|      ! 0 | 11956 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 11957 | `					&pGen->pIn->sData);` |
|      ! 0 | 11958 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 11959 | `					return SXERR_ABORT;` |
|        - | 11960 | `				}` |
|        - | 11961 | `				/* FALL THROUGH */` |
|      ! 0 | 11962 | `			}` |
|        - | 11963 | `			/* Block compiled */` |
|        3 | 11964 | `			break;` |
|        - | 11965 | `		}` |
|      117 | 11966 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|        - | 11967 | `			/*` |
|        - | 11968 | `			 * Accroding to the PHP language reference manual` |
|        - | 11969 | `			 *  A special case is the default case. This case matches anything` |
|        - | 11970 | `			 *  that wasn't matched by the other cases.` |
|        - | 11971 | `			 */` |
|       25 | 11972 | `			if( pSwitch->nDefault > 0 ){` |
|        - | 11973 | `				/* Default case already compiled */` |
|      ! 0 | 11974 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|      ! 0 | 11975 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 11976 | `					return SXERR_ABORT;` |
|        - | 11977 | `				}` |
|      ! 0 | 11978 | `			}` |
|       25 | 11979 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|        - | 11980 | `			/* Compile the default block */` |
|       25 | 11981 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|       25 | 11982 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 11983 | `				return SXERR_ABORT;` |
|       25 | 11984 | `			}else if( rc == SXERR_EOF ){` |
|       23 | 11985 | `				break;` |
|        1 | 11986 | `			}` |
|       98 | 11987 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|        - | 11988 | `			ph7_case_expr sCase;` |
|        - | 11989 | `			/* Standard case block */` |
|       97 | 11990 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|        - | 11991 | `			/* initialize the structure */` |
|       97 | 11992 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - | 11993 | `			/* Compile the case expression */` |
|       97 | 11994 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|       97 | 11995 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11996 | `				return SXERR_ABORT;` |
|        - | 11997 | `			}` |
|        - | 11998 | `			/* Compile the case block */` |
|       97 | 11999 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|        - | 12000 | `			/* Insert in the switch container */` |
|       97 | 12001 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|       97 | 12002 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 12003 | `				return SXERR_ABORT;` |
|       97 | 12004 | `			}else if( rc == SXERR_EOF ){` |
|        9 | 12005 | `				break;` |
|        - | 12006 | `			}` |
|       47 | 12007 | `		}else{` |
|        - | 12008 | `			/* Unexpected token */` |
|      ! 0 | 12009 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 12010 | `				&pGen->pIn->sData);` |
|      ! 0 | 12011 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 12012 | `				return SXERR_ABORT;` |
|        - | 12013 | `			}` |
|      ! 0 | 12014 | `			break;` |
|        - | 12015 | `		}` |
|        5 | 12016 | `	}` |
|        - | 12017 | `	/* Fix all jumps now the destination is resolved */` |
|       33 | 12018 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|       33 | 12019 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 12020 | `	/* Release the loop block */` |
|       33 | 12021 | `	GenStateLeaveBlock(pGen,0);` |
|       33 | 12022 | `	if( pGen->pIn < pGen->pEnd ){` |
|        - | 12023 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|       33 | 12024 | `		pGen->pIn++;` |
|       14 | 12025 | `	}` |
|        - | 12026 | `	/* Statement successfully compiled */` |
|       33 | 12027 | `	return SXRET_OK;` |
|      ! 0 | 12028 | `Synchronize:` |
|        - | 12029 | `	/* Synchronize with the first semi-colon */` |
|      ! 0 | 12030 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      ! 0 | 12031 | `		pGen->pIn++;` |
|      ! 0 | 12032 | `	}` |
|      ! 0 | 12033 | `	return SXRET_OK;` |
|       19 | 12034 | `}` |
|        - | 12035 | `/*` |
|        - | 12036 | ` * Chain operators participate in a postfix member-access chain.` |
|        - | 12037 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|        - | 12038 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|        - | 12039 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|        - | 12040 | ` */` |
|        - | 12041 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|        - | 12042 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|        - | 12043 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|        - | 12044 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|        - | 12045 |  |
|        - | 12046 | `/*` |
|        - | 12047 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|        - | 12048 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|        - | 12049 | ` * patched entries from the pending set.` |
|        - | 12050 | ` */` |
| 22843480 | 12051 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|        5 | 12052 | `{` |
| 22843485 | 12053 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|        - | 12054 | `	sxu32 nTarget;` |
|        - | 12055 | `	sxu32 *aIdx;` |
|        - | 12056 | `	sxu32 i;` |
| 22843485 | 12057 | `	if( nCur <= nBaseline ){` |
| 22843389 | 12058 | `		return;` |
|        - | 12059 | `	}` |
|      100 | 12060 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      100 | 12061 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|      204 | 12062 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|      108 | 12063 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|      108 | 12064 | `		if( pInstr ){` |
|      108 | 12065 | `			pInstr->iP2 = (sxi32)nTarget;` |
|       52 | 12066 | `		}` |
|       56 | 12067 | `	}` |
|      100 | 12068 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 11421745 | 12069 | `}` |
|        - | 12070 |  |
|        - | 12071 | `/*` |
|        - | 12072 | ` * By-reference out-parameters of builtin functions.` |
|        - | 12073 | ` *` |
|        - | 12074 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|        - | 12075 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|        - | 12076 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|        - | 12077 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|        - | 12078 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|        - | 12079 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|        - | 12080 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|        - | 12081 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|        - | 12082 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|        - | 12083 | ` * creates it" behaviour).` |
|        - | 12084 | ` *` |
|        - | 12085 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|        - | 12086 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|        - | 12087 | ` */` |
|  3196242 | 12088 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|        5 | 12089 | `{` |
|        - | 12090 | `	static const struct {` |
|        - | 12091 | `		const char *zName;` |
|        - | 12092 | `		sxu32 nByte;` |
|        - | 12093 | `		sxu32 mask;` |
|        - | 12094 | `	} aByRef[] = {` |
|        - | 12095 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|        - | 12096 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|        - | 12097 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|        - | 12098 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|        - | 12099 | `		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */` |
|        - | 12100 | `	};` |
|        - | 12101 | `	sxu32 i;` |
|  3196247 | 12102 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|   838873 | 12103 | `		return 0;` |
|        - | 12104 | `	}` |
| 14143855 | 12105 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 11786598 | 12106 | `		if( pName->nByte == aByRef[i].nByte` |
|  6038712 | 12107 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|      127 | 12108 | `			return aByRef[i].mask;` |
|        - | 12109 | `		}` |
|  5893243 | 12110 | `	}` |
|  2357257 | 12111 | `	return 0;` |
|  1598126 | 12112 | `}` |
|        - | 12113 | `/*` |
|        - | 12114 | ` * Recover the bare global-builtin name from a call's callee node.` |
|        - | 12115 | ` *` |
|        - | 12116 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|        - | 12117 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|        - | 12118 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|        - | 12119 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|        - | 12120 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|        - | 12121 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|        - | 12122 | ` */` |
|  3196242 | 12123 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|        5 | 12124 | `{` |
|        - | 12125 | `	SyToken *p, *pEnd;` |
|  3196247 | 12126 | `	pOut->zString = 0;` |
|  3196247 | 12127 | `	pOut->nByte = 0;` |
|  3196247 | 12128 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|      ! 0 | 12129 | `		return;` |
|        - | 12130 | `	}` |
|  3196247 | 12131 | `	p = pLeft->pStart;` |
|  3196247 | 12132 | `	pEnd = pLeft->pEnd;` |
|        - | 12133 | `	/* Optional single leading namespace separator (absolute path). */` |
|  3196247 | 12134 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|     3917 | 12135 | `		p++;` |
|     1956 | 12136 | `	}` |
|  3196247 | 12137 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|   838837 | 12138 | `		return;` |
|        - | 12139 | `	}` |
|        - | 12140 | `	/* Must be a single component: nothing follows the name token. */` |
|  2357415 | 12141 | `	if( p + 1 != pEnd ){` |
|       40 | 12142 | `		return;` |
|        - | 12143 | `	}` |
|  2357379 | 12144 | `	*pOut = p->sData;` |
|  1598126 | 12145 | `}` |
|        - | 12146 | `/*` |
|        - | 12147 | ` * Generate bytecode for a given expression tree.` |
|        - | 12148 | ` * If something goes wrong while generating bytecode` |
|        - | 12149 | ` * for the expression tree (A very unlikely scenario)` |
|        - | 12150 | ` * this function takes care of generating the appropriate` |
|        - | 12151 | ` * error message.` |
|        - | 12152 | ` */` |
| 31658778 | 12153 | `static sxi32 GenStateEmitExprCode(` |
|        - | 12154 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - | 12155 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|        - | 12156 | `	sxi32 iFlags /* Control flags */` |
|        - | 12157 | `	)` |
|        5 | 12158 | `{` |
|        - | 12159 | `	VmInstr *pInstr;` |
|        - | 12160 | `	sxu32 nJmpIdx;` |
| 31658783 | 12161 | `	sxi32 iP1 = 0;` |
| 31658783 | 12162 | `	sxu32 iP2 = 0;` |
| 31658783 | 12163 | `	void *p3  = 0;` |
|        - | 12164 | `	sxi32 iVmOp;` |
|        - | 12165 | `	sxi32 rc;` |
| 31658783 | 12166 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 31658783 | 12167 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
| 31658783 | 12168 | `	sxu32 nRhsNsBase = 0;` |
| 31658783 | 12169 | `	if( pNode->xCode ){` |
|        - | 12170 | `		SyToken *pTmpIn,*pTmpEnd;` |
|        - | 12171 | `		/* Compile node */` |
| 19021925 | 12172 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 19021925 | 12173 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 19021925 | 12174 | `		RE_SWAP_DELIMITER(pGen);` |
| 19021925 | 12175 | `		return rc;` |
|        - | 12176 | `	}` |
| 12636863 | 12177 | `	if( pNode->pOp == 0 ){` |
|      ! 0 | 12178 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 12179 | `			"Invalid expression node,PH7 is aborting compilation");` |
|      ! 0 | 12180 | `		return SXERR_ABORT;` |
|        - | 12181 | `	}` |
| 12636863 | 12182 | `	iVmOp = pNode->pOp->iVmOp;` |
| 12636863 | 12183 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|        - | 12184 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|        - | 12185 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|        - | 12186 | `		 * and later errors are still reported. */` |
|        3 | 12187 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 12188 | `			"The (unset) cast is no longer supported");` |
|        3 | 12189 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12190 | `			return SXERR_ABORT;` |
|        - | 12191 | `		}` |
|        1 | 12192 | `	}` |
| 12636863 | 12193 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|       65 | 12194 | `		sxu32 nJmp = 0;` |
|        - | 12195 | `		sxu32 nNcNsBase;` |
|        - | 12196 | `		VmInstr *pInstrFix;` |
|        - | 12197 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|        - | 12198 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|        - | 12199 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|        - | 12200 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|        - | 12201 | `		 * stack slot carries a writable nIdx. */` |
|       65 | 12202 | `		if( pNode->pRight ){` |
|       65 | 12203 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       65 | 12204 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|       65 | 12205 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 12206 | `				return rc;` |
|        - | 12207 | `			}` |
|       65 | 12208 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        - | 12209 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|        - | 12210 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|        - | 12211 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|        - | 12212 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|        - | 12213 | `			 * the store, so the parent array does not need to be copied at` |
|        - | 12214 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|        - | 12215 | `			 * cascade for the actual write path stays correct. */` |
|       65 | 12216 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|       65 | 12217 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|       31 | 12218 | `				pInstrFix->iP2 = 3;` |
|       14 | 12219 | `			}` |
|       31 | 12220 | `		}` |
|        - | 12221 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|       65 | 12222 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|        - | 12223 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|       65 | 12224 | `		if( pNode->pLeft ){` |
|       65 | 12225 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       65 | 12226 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|       65 | 12227 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 12228 | `				return rc;` |
|        - | 12229 | `			}` |
|       65 | 12230 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       31 | 12231 | `		}` |
|        - | 12232 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|       65 | 12233 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|        - | 12234 | `		/* Patch the short-circuit jump to land after the store. */` |
|       65 | 12235 | `		if( nJmp > 0 ){` |
|       65 | 12236 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|       65 | 12237 | `			if( pInstrFix ){` |
|       65 | 12238 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|       31 | 12239 | `			}` |
|       31 | 12240 | `		}` |
|       65 | 12241 | `		return SXRET_OK;` |
|        - | 12242 | `	}` |
| 12636801 | 12243 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|        - | 12244 | `		sxu32 nJz,nJmp;` |
|        - | 12245 | `		sxu32 nTernaryNsBase;` |
|        - | 12246 | `		/* Ternary operator require special handling */` |
|        - | 12247 | `		/* Phase#1: Compile the condition */` |
|   205151 | 12248 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   205151 | 12249 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|   205151 | 12250 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12251 | `			return rc;` |
|        - | 12252 | `		}` |
|        - | 12253 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|        - | 12254 | `		 * compiling the condition must short-circuit to the end of the` |
|        - | 12255 | `		 * condition expression, not leak past the ternary. */` |
|   205151 | 12256 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|   205151 | 12257 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|   205151 | 12258 | `		if( pNode->pLeft ){` |
|        - | 12259 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|        - | 12260 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|   205083 | 12261 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|        - | 12262 | `			/* Phase#3: Compile the 'then' expression  */` |
|   205083 | 12263 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   205083 | 12264 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|   205083 | 12265 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 12266 | `				return rc;` |
|        - | 12267 | `			}` |
|   205083 | 12268 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|   102544 | 12269 | `		}else{` |
|        - | 12270 | `			/* Elvis operator: (expr) ?: (else)` |
|        - | 12271 | `			 * Duplicate condition so original value is the 'then' result.` |
|        - | 12272 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|       70 | 12273 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       70 | 12274 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|        - | 12275 | `		}` |
|        - | 12276 | `		/* Phase#4: Emit the unconditional jump */` |
|   205151 | 12277 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|        - | 12278 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|   205151 | 12279 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|   205151 | 12280 | `		if( pInstr ){` |
|   205151 | 12281 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|   102573 | 12282 | `		}` |
|   205151 | 12283 | `		if( !pNode->pLeft ){` |
|        - | 12284 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|       70 | 12285 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       34 | 12286 | `		}` |
|        - | 12287 | `		/* Phase#6: Compile the 'else' expression */` |
|   205151 | 12288 | `		if( pNode->pRight ){` |
|   205151 | 12289 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   205151 | 12290 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|   205151 | 12291 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 12292 | `				return rc;` |
|        - | 12293 | `			}` |
|   205151 | 12294 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|   102573 | 12295 | `		}` |
|   205151 | 12296 | `		if( nJmp > 0 ){` |
|        - | 12297 | `			/* Phase#7: Fix the unconditional jump */` |
|   205151 | 12298 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|   205151 | 12299 | `			if( pInstr ){` |
|   205151 | 12300 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|   102573 | 12301 | `			}` |
|   102573 | 12302 | `		}` |
|        - | 12303 | `		/* All done */` |
|   205151 | 12304 | `		return SXRET_OK;` |
|        - | 12305 | `	}` |
| 12431655 | 12306 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|        - | 12307 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|        - | 12308 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|        - | 12309 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|        - | 12310 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|        - | 12311 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|        - | 12312 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|        - | 12313 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|        - | 12314 | `		sxu32 nPipeNsBase;` |
|       27 | 12315 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|       27 | 12316 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|      ! 0 | 12317 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 12318 | `				"'\|>': Missing operand");` |
|      ! 0 | 12319 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 12320 | `		}` |
|        - | 12321 | `		/* Argument: the LHS value. */` |
|       27 | 12322 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       27 | 12323 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|       27 | 12324 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12325 | `			return rc;` |
|        - | 12326 | `		}` |
|       27 | 12327 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|        - | 12328 | `		/* Callable: the RHS. */` |
|       27 | 12329 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       27 | 12330 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|       27 | 12331 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12332 | `			return rc;` |
|        - | 12333 | `		}` |
|       27 | 12334 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|        - | 12335 | `		/* Invoke the callable with the single piped argument. */` |
|       27 | 12336 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       27 | 12337 | `		return SXRET_OK;` |
|        - | 12338 | `	}` |
| 12431629 | 12339 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|        - | 12340 | `	/* Generate code for the left tree */` |
| 12431629 | 12341 | `	if( pNode->pLeft ){` |
| 12419971 | 12342 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 12419971 | 12343 | `		if( iVmOp == PH7_OP_CALL ){` |
|        - | 12344 | `			ph7_expr_node **apNode;` |
|  3200447 | 12345 | `			int hasSpread = 0;` |
|  3200447 | 12346 | `			int hasNamed = 0;` |
|  3200447 | 12347 | `			int bAnySpread = 0;` |
|  3200447 | 12348 | `			sxu32 byRefMask = 0;` |
|        - | 12349 | `			sxi32 nArgs;` |
|        - | 12350 | `			sxi32 n;` |
|        - | 12351 | `			/* Recurse and generate bytecodes for function arguments */` |
|  3200447 | 12352 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  3200447 | 12353 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|        - | 12354 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|        - | 12355 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|        - | 12356 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|  3200447 | 12357 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|       81 | 12358 | `				bFcc = 1;` |
|       81 | 12359 | `				nArgs = 0;` |
|       40 | 12360 | `			}` |
|        - | 12361 | `			/* Validate argument order like php: no positional argument after a` |
|        - | 12362 | ``			 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|        - | 12363 | `			{` |
|  3200447 | 12364 | `				int seenNamed = 0;` |
|  3200447 | 12365 | `				int seenSpread = 0;` |
|  6368099 | 12366 | `				for( n = 0; n < nArgs; ++n ){` |
|  3167659 | 12367 | `					if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|     4073 | 12368 | `						bAnySpread = 1;` |
|     4073 | 12369 | `						seenSpread = 1;` |
|     4073 | 12370 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      ! 0 | 12371 | `							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 12372 | `								"syntax error, unexpected token \"...\"");` |
|      ! 0 | 12373 | `							return SXERR_SYNTAX;` |
|        5 | 12374 | `						}` |
|  3165625 | 12375 | `					}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      289 | 12376 | `						seenNamed = 1;` |
|      289 | 12377 | `						hasNamed = 1;` |
|  3163449 | 12378 | `					}else if( seenNamed ){` |
|        3 | 12379 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 12380 | `							"Cannot use positional argument after named argument");` |
|        3 | 12381 | `						return SXERR_SYNTAX;` |
|  3163305 | 12382 | `					}else if( seenSpread ){` |
|      ! 0 | 12383 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 12384 | `							"Cannot use positional argument after argument unpacking");` |
|      ! 0 | 12385 | `						return SXERR_SYNTAX;` |
|        - | 12386 | `					}` |
|  1583831 | 12387 | `				}` |
|        - | 12388 | `			}` |
|        - | 12389 | `			/* Read-only load */` |
|  3200445 | 12390 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|        - | 12391 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|        - | 12392 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|        - | 12393 | `			 * objects dispatch to the right method (offsetExists for both;` |
|        - | 12394 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|  3200445 | 12395 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|  3200445 | 12396 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  3200440 | 12397 | `				if( pCallName->nByte == 5` |
|  1758772 | 12398 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|   155717 | 12399 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|  3122589 | 12400 | `				}else if( pCallName->nByte == 5` |
|  1603060 | 12401 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|      101 | 12402 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|       48 | 12403 | `				}` |
|        - | 12404 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|        - | 12405 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|        - | 12406 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|        - | 12407 | `				 * write back through. Skipped when spread/named args are present:` |
|        - | 12408 | `				 * the compile-time positional index no longer maps to the` |
|        - | 12409 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|  3200445 | 12410 | `				if( !bAnySpread && !hasNamed ){` |
|        - | 12411 | `					SyString sBuiltin;` |
|  3196247 | 12412 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|  3196247 | 12413 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|  1598121 | 12414 | `				}` |
|  1600220 | 12415 | `			}` |
|  6368095 | 12416 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  3167655 | 12417 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  3167655 | 12418 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|        - | 12419 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|        - | 12420 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|        - | 12421 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|        - | 12422 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|        - | 12423 | `				 * builtin to write back through. A plain $var target is unaffected` |
|        - | 12424 | `				 * (iP1=0 either way). */` |
|  3167655 | 12425 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|       61 | 12426 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|       61 | 12427 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|       28 | 12428 | `				}` |
|  3167655 | 12429 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  3167655 | 12430 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 12431 | `					return rc;` |
|        - | 12432 | `				}` |
|        - | 12433 | `				/* Each argument is an independent nullsafe scope. */` |
|  3167655 | 12434 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  3167655 | 12435 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|        - | 12436 | `					/* Emit spread opcode to unpack this array argument */` |
|     4073 | 12437 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|     4073 | 12438 | `					hasSpread = 1;` |
|     2034 | 12439 | `				}` |
|  1583830 | 12440 | `			}` |
|        - | 12441 | `			/* Total number of given arguments */` |
|  3200445 | 12442 | `			iP1 = nArgs;` |
|  3200445 | 12443 | `			iP2 = hasSpread;` |
|        - | 12444 | `			/* Build VmCallArgMap if named arguments are present.` |
|        - | 12445 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  3200445 | 12446 | `			if( hasNamed ){` |
|      178 | 12447 | `				sxu32 nStrBytes = 0;` |
|        - | 12448 | `				char *zBuf;` |
|      534 | 12449 | `				for( n = 0; n < nArgs; ++n ){` |
|      360 | 12450 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      286 | 12451 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|      141 | 12452 | `					}` |
|      182 | 12453 | `				}` |
|        - | 12454 | `				{` |
|      178 | 12455 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|      178 | 12456 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      174 | 12457 | `					&pGen->pVm->sAllocator, mapSize);` |
|      178 | 12458 | `				if( pMap ){` |
|      178 | 12459 | `					SyZero(pMap, mapSize);` |
|      178 | 12460 | `					pMap->bHasNamed = 1;` |
|      178 | 12461 | `					pMap->nTotal = (sxu32)nArgs;` |
|      178 | 12462 | `					pMap->aNames = (SyString *)&pMap[1];` |
|      178 | 12463 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|      534 | 12464 | `					for( n = 0; n < nArgs; ++n ){` |
|      360 | 12465 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      286 | 12466 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|      286 | 12467 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|      286 | 12468 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|      286 | 12469 | `							zBuf += nb;` |
|      141 | 12470 | `						}` |
|        - | 12471 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|      182 | 12472 | `					}` |
|      178 | 12473 | `					p3 = (void *)pMap;` |
|       87 | 12474 | `				}` |
|        - | 12475 | `				}` |
|       87 | 12476 | `			}` |
|        - | 12477 | `			/* Remove stale flags now */` |
|  3200445 | 12478 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  1600220 | 12479 | `		}` |
|        - | 12480 | `		{` |
|        - | 12481 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|        - | 12482 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|        - | 12483 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|        - | 12484 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|        - | 12485 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|        - | 12486 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|        - | 12487 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|        - | 12488 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
| 12419969 | 12489 | `			sxi32 iLeftFlags = iFlags;` |
| 12419964 | 12490 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
| 10364945 | 12491 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|  4154989 | 12492 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  3691083 | 12493 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   943745 | 12494 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   471870 | 12495 | `			}` |
|        - | 12496 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|        - | 12497 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|        - | 12498 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|        - | 12499 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|        - | 12500 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|        - | 12501 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|        - | 12502 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
| 12419964 | 12503 | `			if( pNode->pOp` |
| 17642881 | 12504 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
| 11432946 | 12505 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
| 10445876 | 12506 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|  2005899 | 12507 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|  1002947 | 12508 | `			}` |
| 12419969 | 12509 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|        - | 12510 | `		}` |
| 12419969 | 12511 | `		if( rc != SXRET_OK ){` |
|       34 | 12512 | `			return rc;` |
|        - | 12513 | `		}` |
| 12419939 | 12514 | `		if( !bIsChainOp ){` |
|        - | 12515 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|        - | 12516 | `			 * target the end of that LHS chain, which is right here. */` |
|  5622165 | 12517 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  2811080 | 12518 | `		}` |
| 12419939 | 12519 | `		if( iVmOp == PH7_OP_CALL ){` |
|  3200445 | 12520 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  3200445 | 12521 | `			if( pInstr ){` |
|  3200445 | 12522 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  2357655 | 12523 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|        - | 12524 | `					sxu32 nQual;` |
|  2357655 | 12525 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|        - | 12526 | `					/* Prevent constant expansion but preserve the absolute flag` |
|        - | 12527 | `					 * so the later NEW handler (if any) can see it. */` |
|  2357655 | 12528 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|        - | 12529 | `					/* Namespace-qualify the function name for CALL, unless the` |
|        - | 12530 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|        - | 12531 | `					 * imports — class imports must NOT affect function` |
|        - | 12532 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|        - | 12533 | `					 * before NEW; we store the original literal index in the` |
|        - | 12534 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|        - | 12535 | `					 * the unqualified name and re-qualify with class imports. */` |
|  2357655 | 12536 | `					if( bAbsolute ){` |
|     3917 | 12537 | `						pInstr->iP2 = (sxi32)nOrig;` |
|     1961 | 12538 | `					}else{` |
|  2353743 | 12539 | `						int fromImport = 0;` |
|  2353743 | 12540 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  2353743 | 12541 | `						pInstr->iP2 = (sxi32)nQual;` |
|  2353743 | 12542 | `						if( nQual != nOrig ){` |
|        - | 12543 | `							/* Record the original literal index in the arg map` |
|        - | 12544 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|        - | 12545 | `							 * flag) so the NEW handler can recover the` |
|        - | 12546 | `							 * unqualified name and re-qualify with CLASS` |
|        - | 12547 | `							 * imports. */` |
|       77 | 12548 | `							if( p3 == 0 ){` |
|       77 | 12549 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       72 | 12550 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|       77 | 12551 | `								if( pMap ){` |
|       77 | 12552 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|       77 | 12553 | `									p3 = (void *)pMap;` |
|       36 | 12554 | `								}` |
|       36 | 12555 | `							}` |
|       77 | 12556 | `							if( p3 ){` |
|       77 | 12557 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|       77 | 12558 | `								if( !fromImport ){` |
|        - | 12559 | `									/* Mark as namespace-qualified */` |
|       67 | 12560 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|       31 | 12561 | `								}` |
|       36 | 12562 | `							}` |
|       36 | 12563 | `						}` |
|        5 | 12564 | `					}` |
|  2021620 | 12565 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|        - | 12566 | `					/* Method call,flag that */` |
|   838305 | 12567 | `					pInstr->iP2 = 1;` |
|   419150 | 12568 | `				}` |
|  1600225 | 12569 | `			}` |
| 10819719 | 12570 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|        - | 12571 | `			ph7_expr_node **apNode;` |
|        - | 12572 | `			sxi32 n;` |
|  1591445 | 12573 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|        - | 12574 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|        - | 12575 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|        - | 12576 | `			/* Recurse and generate bytecodes for array index */` |
|  1591445 | 12577 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  3054583 | 12578 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|  1463143 | 12579 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  1463143 | 12580 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|  1463143 | 12581 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 12582 | `					return rc;` |
|        - | 12583 | `				}` |
|        - | 12584 | `				/* Each subscript index is an independent nullsafe scope. */` |
|  1463143 | 12585 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   731574 | 12586 | `			}` |
|  1591445 | 12587 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|  1463143 | 12588 | `				iP1 = 1; /* Node have an index associated with it */` |
|   731569 | 12589 | `			}` |
|  1591445 | 12590 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|        - | 12591 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|   194445 | 12592 | `				iP2 = 4;` |
|  1494225 | 12593 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        - | 12594 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|        - | 12595 | `				 * so the trailing unset() builtin can drop the slot. */` |
|       72 | 12596 | `				iP2 = 5;` |
|  1396971 | 12597 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        - | 12598 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|        - | 12599 | `				 * short-circuit on missing keys without invoking offsetGet` |
|        - | 12600 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|       29 | 12601 | `				iP2 = 6;` |
|  1396925 | 12602 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|        - | 12603 | `				/* Create an empty entry when the desired index is not found */` |
|   190899 | 12604 | `				iP2 = 1;` |
|    95452 | 12605 | `			}` |
|  8423779 | 12606 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|        - | 12607 | `			/* POP the left node */` |
|       32 | 12608 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       15 | 12609 | `		}` |
|  6209967 | 12610 | `	}` |
| 12431597 | 12611 | `	rc = SXRET_OK;` |
| 12431597 | 12612 | `	nJmpIdx = 0;` |
|        - | 12613 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|        - | 12614 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|        - | 12615 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 12431597 | 12616 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|    43417 | 12617 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    43417 | 12618 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    43417 | 12619 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    43417 | 12620 | `			int isSpecial = 0;` |
|    43417 | 12621 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|    20073 | 12622 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|    20073 | 12623 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|    20068 | 12624 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    31674 | 12625 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    15839 | 12626 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|    11787 | 12627 | `					isSpecial = 1;` |
|     5891 | 12628 | `				}` |
|    15870 | 12629 | `			}` |
|    55089 | 12630 | `			pInstr->iP1 = 0;` |
|    55089 | 12631 | `			if( !isSpecial ){` |
|    19963 | 12632 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     9979 | 12633 | `			}` |
|        - | 12634 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|        - | 12635 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|    31745 | 12636 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    19963 | 12637 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    19963 | 12638 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|       60 | 12639 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|       62 | 12640 | `					return SXRET_OK;` |
|        - | 12641 | `				}` |
|     9950 | 12642 | `			}` |
|    15841 | 12643 | `		}` |
|    39164 | 12644 | `	}` |
|        - | 12645 | `	/* Generate code for the right tree */` |
| 12419881 | 12646 | `	if( pNode->pRight ){` |
|  6788253 | 12647 | `		if( iVmOp == PH7_OP_LAND ){` |
|        - | 12648 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|   136471 | 12649 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  6720020 | 12650 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|        - | 12651 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    93399 | 12652 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  6605090 | 12653 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|        - | 12654 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|      141 | 12655 | `			iVmOp = 0; /* No binary operator to emit */` |
|      141 | 12656 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  6558377 | 12657 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|        - | 12658 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|        - | 12659 | `			 * the entire containing postfix chain to null. The jump target is` |
|        - | 12660 | `			 * patched later by the innermost non-chain ancestor (or by` |
|        - | 12661 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|        - | 12662 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|        - | 12663 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|      108 | 12664 | `			sxu32 nNsJmp = 0;` |
|      108 | 12665 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|      108 | 12666 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  6558205 | 12667 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|        - | 12668 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|        - | 12669 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|        - | 12670 | `			 * auto-created — PHP auto-vivifies on write. */` |
|  2314113 | 12671 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|  1157054 | 12672 | `		}` |
|  6788253 | 12673 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  6788253 | 12674 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  6788253 | 12675 | `		if( !bIsChainOp ){` |
|        - | 12676 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|        - | 12677 | `			 * operator instruction is emitted. */` |
|  4782417 | 12678 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  2391206 | 12679 | `		}` |
|  6788253 | 12680 | `		if( iVmOp == PH7_OP_STORE ){` |
|  2026383 | 12681 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  2026348 | 12682 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|        - | 12683 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|        - | 12684 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|        - | 12685 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|        - | 12686 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|        - | 12687 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|        - | 12688 | `				 */` |
|       91 | 12689 | `				iVmOp = 0;` |
|  2026340 | 12690 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  2026297 | 12691 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|        - | 12692 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   249147 | 12693 | `					iP2 = 1;` |
|   124576 | 12694 | `				}else{` |
|  1777155 | 12695 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        - | 12696 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   190817 | 12697 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   190817 | 12698 | `						iP1 = pInstr->iP1;` |
|    95411 | 12699 | `					}else{` |
|  1586343 | 12700 | `						p3 = pInstr->p3;` |
|        - | 12701 | `					}` |
|        - | 12702 | `					/* POP the last dynamic load instruction */` |
|  1777155 | 12703 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        - | 12704 | `				}` |
|  1013151 | 12705 | `			}` |
|  5775064 | 12706 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|       64 | 12707 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|       64 | 12708 | `			if( pInstr ){` |
|       64 | 12709 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        - | 12710 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|        - | 12711 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|        - | 12712 | `					 */` |
|       19 | 12713 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|       19 | 12714 | `					iP1 = pInstr->iP1;` |
|       19 | 12715 | `					iP2 = pInstr->iP2;` |
|       19 | 12716 | `					p3  = pInstr->p3;` |
|       10 | 12717 | `				}else{` |
|       46 | 12718 | `					p3 = pInstr->p3;` |
|        - | 12719 | `				}` |
|       30 | 12720 | `			}` |
|       30 | 12721 | `		}` |
|  3394124 | 12722 | `	}` |
| 12419876 | 12723 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|   242116 | 12724 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|        - | 12725 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|        - | 12726 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|       32 | 12727 | `		iVmOp = 0;` |
|       14 | 12728 | `	}` |
| 12419881 | 12729 | `	if( iVmOp > 0 ){` |
| 12419601 | 12730 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    70369 | 12731 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|        - | 12732 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    11685 | 12733 | `				iP1 = 1;` |
|     5845 | 12734 | `			}` |
| 12384419 | 12735 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|        - | 12736 | `			/* Namespace-qualify the class name for NEW */ {` |
|   483923 | 12737 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   483923 | 12738 | `				VmInstr *pCallInstr = 0;` |
|   483923 | 12739 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   483675 | 12740 | `					pCallInstr = pPeek;` |
|   483675 | 12741 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|   241835 | 12742 | `				}` |
|   483923 | 12743 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   483919 | 12744 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|        - | 12745 | `					sxu32 nLitForClass;` |
|   483919 | 12746 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|        - | 12747 | `					/* If the CALL handler qualified the name with FUNCTION` |
|        - | 12748 | `					 * imports, recover the original literal (recorded in the` |
|        - | 12749 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|        - | 12750 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|        - | 12751 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|        - | 12752 | `					 * with class imports. */` |
|   483919 | 12753 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|       37 | 12754 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|       21 | 12755 | `					}else{` |
|   483887 | 12756 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|        - | 12757 | `					}` |
|   483919 | 12758 | `					pPeek->iP1 = 0;` |
|   483919 | 12759 | `					if( !bAbsolute ){` |
|   480011 | 12760 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|   240008 | 12761 | `					}else{` |
|     3913 | 12762 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|        - | 12763 | `					}` |
|   241957 | 12764 | `				}` |
|        - | 12765 | `			}` |
|   483923 | 12766 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   483923 | 12767 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|        - | 12768 | `				VmInstr *pPrev;` |
|   483675 | 12769 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   483675 | 12770 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|        - | 12771 | `					/* Pop the call instruction, preserve named-arg map and` |
|        - | 12772 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|        - | 12773 | `					 * accumulator exactly like OP_CALL would have). */` |
|   483675 | 12774 | `					iP1 = pInstr->iP1;` |
|   483675 | 12775 | `					iP2 = pInstr->iP2;` |
|   483675 | 12776 | `					if( pInstr->p3 ){` |
|       47 | 12777 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|       21 | 12778 | `					}` |
|   483675 | 12779 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|   241835 | 12780 | `				}` |
|   241840 | 12781 | `			}` |
| 12107278 | 12782 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|        - | 12783 | `			/* instanceof: right operand is a class name, not a constant.` |
|        - | 12784 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|    31301 | 12785 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    31301 | 12786 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    31301 | 12787 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    31301 | 12788 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|    31301 | 12789 | `				int isSpecialIs = 0;` |
|    31301 | 12790 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    31301 | 12791 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    31301 | 12792 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    31296 | 12793 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    31299 | 12794 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    15648 | 12795 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|       12 | 12796 | `						isSpecialIs = 1;` |
|        5 | 12797 | `					}` |
|    15648 | 12798 | `				}` |
|    31301 | 12799 | `				pInstr->iP1 = 0;` |
|    31301 | 12800 | `				if( !isSpecialIs && !bAbsolute ){` |
|    31281 | 12801 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    15638 | 12802 | `				}` |
|    15653 | 12803 | `			}` |
| 11849671 | 12804 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|        - | 12805 | `			/* Prevent constant expansion for member/property names.` |
|        - | 12806 | `			 * The right child (member name) was just compiled — its LOADC` |
|        - | 12807 | `			 * should not trigger constant lookup. */` |
|  2005841 | 12808 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  2005841 | 12809 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  2005777 | 12810 | `				pInstr->iP1 = 0;` |
|  1002886 | 12811 | `			}` |
|  2005841 | 12812 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|        - | 12813 | `				/* Static member access,remember that */` |
|    31701 | 12814 | `				iP1 = 1;` |
|    31701 | 12815 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    31701 | 12816 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|       54 | 12817 | `					p3 = pInstr->p3;` |
|       54 | 12818 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       25 | 12819 | `				}` |
|    15848 | 12820 | `			}` |
|        - | 12821 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|        - | 12822 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|        - | 12823 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|        - | 12824 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|  2005841 | 12825 | `			if( iP2 == PH7_MEMBER_READ ){` |
|  2005841 | 12826 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       36 | 12827 | `					iP2 = PH7_MEMBER_UNSET;` |
|  2005824 | 12828 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       91 | 12829 | `					iP2 = PH7_MEMBER_ISSET;` |
|  2005764 | 12830 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       15 | 12831 | `					iP2 = PH7_MEMBER_EMPTY;` |
|  2005714 | 12832 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|        - | 12833 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|   249227 | 12834 | `					iP2 = PH7_MEMBER_WRITE;` |
|   124611 | 12835 | `				}` |
|  1002918 | 12836 | `			}` |
|  1002918 | 12837 | `		}` |
|        - | 12838 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|        - | 12839 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|        - | 12840 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|        - | 12841 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|        - | 12842 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
| 12419601 | 12843 | `		if( bFcc ){` |
|       81 | 12844 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|       81 | 12845 | `			iP2 = 0;` |
|       81 | 12846 | `			p3 = 0;` |
|       81 | 12847 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|       81 | 12848 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|        - | 12849 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|        - | 12850 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|        - | 12851 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|        - | 12852 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|       37 | 12853 | `				void *pMemberName = pInstr->p3;` |
|       37 | 12854 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|       37 | 12855 | `				if( pMemberName ){` |
|        3 | 12856 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|        1 | 12857 | `				}` |
|       37 | 12858 | `				iP1 = 2;` |
|       19 | 12859 | `			}else{` |
|       45 | 12860 | `				iP1 = 1;` |
|        - | 12861 | `			}` |
|       40 | 12862 | `		}` |
|        - | 12863 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|        - | 12864 | `		 * This is the primary emit path for user-visible calls. */` |
| 12419601 | 12865 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  3684283 | 12866 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  1842139 | 12867 | `		}` |
|        - | 12868 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 12419601 | 12869 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  6209798 | 12870 | `	}` |
| 12419881 | 12871 | `	if( nJmpIdx > 0 ){` |
|        - | 12872 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   230001 | 12873 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   230001 | 12874 | `		if( pInstr ){` |
|   230001 | 12875 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|   114998 | 12876 | `		}` |
|   114998 | 12877 | `	}` |
| 12419881 | 12878 | `	return rc;` |
| 15823565 | 12879 | `}` |
|        - | 12880 | `/*` |
|        - | 12881 | ` * Compile a PHP expression.` |
|        - | 12882 | ` * According to the PHP language reference manual:` |
|        - | 12883 | ` *  Expressions are the most important building stones of PHP.` |
|        - | 12884 | ` *  In PHP, almost anything you write is an expression.` |
|        - | 12885 | ` *  The simplest yet most accurate way to define an expression` |
|        - | 12886 | ` *  is "anything that has a value".` |
|        - | 12887 | ` * If something goes wrong while compiling the expression,this` |
|        - | 12888 | ` * function takes care of generating the appropriate error` |
|        - | 12889 | ` * message.` |
|        - | 12890 | ` */` |
|  7192766 | 12891 | `static sxi32 PH7_CompileExpr(` |
|        - | 12892 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 12893 | `	sxi32 iFlags,        /* Control flags */` |
|        - | 12894 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|        - | 12895 | `	)` |
|        5 | 12896 | `{` |
|        - | 12897 | `	ph7_expr_node *pRoot;` |
|        - | 12898 | `	SySet sExprNode;` |
|        - | 12899 | `	SyToken *pEnd;` |
|        - | 12900 | `	sxi32 nExpr;` |
|        - | 12901 | `	sxi32 iNest;` |
|        - | 12902 | `	sxi32 rc;` |
|        - | 12903 | `	sxu32 nNullsafeBase;` |
|        - | 12904 | `	/* Initialize worker variables */` |
|  7192771 | 12905 | `	nExpr = 0;` |
|  7192771 | 12906 | `	pRoot = 0;` |
|        - | 12907 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|        - | 12908 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  7192771 | 12909 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  7192771 | 12910 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  7192771 | 12911 | `	SySetAlloc(&sExprNode,0x10);` |
|  7192771 | 12912 | `	rc = SXRET_OK;` |
|        - | 12913 | `	/* Delimit the expression */` |
|  7192771 | 12914 | `	pEnd = pGen->pIn;` |
|  7192771 | 12915 | `	iNest = 0;` |
| 55877707 | 12916 | `	while( pEnd < pGen->pEnd ){` |
| 53315285 | 12917 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|        - | 12918 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      701 | 12919 | `			iNest++;` |
| 53314937 | 12920 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      709 | 12921 | `			iNest--;` |
| 53314237 | 12922 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  4630935 | 12923 | `			if( iNest <= 0 ){` |
|  4630349 | 12924 | `				break;` |
|        - | 12925 | `			}` |
|      293 | 12926 | `		}` |
| 48684941 | 12927 | `		pEnd++;` |
|        5 | 12928 | `	}` |
|  7192771 | 12929 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   237777 | 12930 | `		SyToken *pEnd2 = pGen->pIn;` |
|   237777 | 12931 | `		iNest = 0;` |
|        - | 12932 | `		/* Stop at the first comma */` |
|   553719 | 12933 | `		while( pEnd2 < pEnd ){` |
|   315953 | 12934 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|     7857 | 12935 | `				iNest++;` |
|   312027 | 12936 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|     7857 | 12937 | `				iNest--;` |
|   304175 | 12938 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|       65 | 12939 | `				if( iNest <= 0 ){` |
|        7 | 12940 | `					break;` |
|        - | 12941 | `				}` |
|       27 | 12942 | `			}` |
|   315947 | 12943 | `			pEnd2++;` |
|        5 | 12944 | `		}` |
|   237777 | 12945 | `		if( pEnd2 <pEnd ){` |
|        7 | 12946 | `			pEnd = pEnd2;` |
|        3 | 12947 | `		}` |
|   118886 | 12948 | `	}` |
|  7192771 | 12949 | `	if( pEnd > pGen->pIn ){` |
|  7192761 | 12950 | `		SyToken *pTmp = pGen->pEnd;` |
|        - | 12951 | `		/* Swap delimiter */` |
|  7192761 | 12952 | `		pGen->pEnd = pEnd;` |
|        - | 12953 | `		/* Try to get an expression tree */` |
|  7192761 | 12954 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  7192761 | 12955 | `		if( rc == SXRET_OK && pRoot ){` |
|  7192579 | 12956 | `			rc = SXRET_OK;` |
|  7192579 | 12957 | `			if( xTreeValidator ){` |
|        - | 12958 | `				/* Call the upper layer validator callback */` |
|   563719 | 12959 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   281857 | 12960 | `			}` |
|  7192579 | 12961 | `			if( rc != SXERR_ABORT ){` |
|        - | 12962 | `				/* Generate code for the given tree */` |
|  7192579 | 12963 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|        - | 12964 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|        - | 12965 | `				 * expression so they short-circuit to its end. */` |
|  7192579 | 12966 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  3596287 | 12967 | `			}` |
|  7192579 | 12968 | `			nExpr = 1;` |
|  3596287 | 12969 | `		}` |
|        - | 12970 | `		/* Release the whole tree */` |
|  7192761 | 12971 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|        - | 12972 | `		/* Synchronize token stream */` |
|  7192761 | 12973 | `		pGen->pEnd = pTmp;` |
|  7192761 | 12974 | `		pGen->pIn  = pEnd;` |
|  7192761 | 12975 | `		if( rc == SXERR_ABORT ){` |
|       13 | 12976 | `			SySetRelease(&sExprNode);` |
|       13 | 12977 | `			return SXERR_ABORT;` |
|        - | 12978 | `		}` |
|  3596373 | 12979 | `	}` |
|  7192761 | 12980 | `	SySetRelease(&sExprNode);` |
|  7192761 | 12981 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  3596388 | 12982 | `}` |
|        - | 12983 | `/*` |
|        - | 12984 | ` * Return a pointer to the node construct handler associated` |
|        - | 12985 | ` * with a given node type [i.e: string,integer,float,...].` |
|        - | 12986 | ` */` |
|  4313772 | 12987 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|        5 | 12988 | `{` |
|  4313777 | 12989 | `	if( nNodeType & PH7_TK_NUM ){` |
|        - | 12990 | `		/* Numeric literal: Either real or integer */` |
|  1296873 | 12991 | `		return PH7_CompileNumLiteral;` |
|  3016909 | 12992 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|        - | 12993 | `		/* Double quoted string */` |
|    36923 | 12994 | `		return PH7_CompileString;` |
|  2979991 | 12995 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|        - | 12996 | `		/* Single quoted string */` |
|  2979871 | 12997 | `		return PH7_CompileSimpleString;` |
|      125 | 12998 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|        - | 12999 | `		/* Heredoc */` |
|       71 | 13000 | `		return PH7_CompileHereDoc;` |
|       58 | 13001 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|        - | 13002 | `		/* Nowdoc */` |
|       51 | 13003 | `		return PH7_CompileNowDoc;` |
|        9 | 13004 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|        - | 13005 | `		/* Backtick quoted string */` |
|        6 | 13006 | `		return PH7_CompileBacktic;` |
|        - | 13007 | `	}` |
|        3 | 13008 | `	return 0;` |
|  2156891 | 13009 | `}` |
|        - | 13010 | `/*` |
|        - | 13011 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|        - | 13012 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|        - | 13013 | ` * in write context" parse error.` |
|        - | 13014 | ` */` |
|     6852 | 13015 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|        5 | 13016 | `{` |
|        - | 13017 | `	sxi32 rc;` |
|     6857 | 13018 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|     6855 | 13019 | `		return SXRET_OK;` |
|        - | 13020 | `	}` |
|        5 | 13021 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|        2 | 13022 | `		pNode ? pNode->pStart->nLine : 1,` |
|        - | 13023 | `		"Can't use nullsafe operator in write context");` |
|        3 | 13024 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     3431 | 13025 | `}` |
|        - | 13026 | `/*` |
|        - | 13027 | ` * Compile an unset() statement.` |
|        - | 13028 | ` * unset($var, $arr[$key], ...);` |
|        - | 13029 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|        - | 13030 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|        - | 13031 | ` * parent array before extracting the element to unset.` |
|        - | 13032 | ` */` |
|     2930 | 13033 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|        5 | 13034 | `{` |
|     2935 | 13035 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     2935 | 13036 | `	sxu32 nIdx = 0;` |
|        - | 13037 | `	SyString sName;` |
|        - | 13038 | `	sxi32 rc;` |
|        - | 13039 | `	/* Jump the 'unset' keyword */` |
|     2935 | 13040 | `	pGen->pIn++;` |
|        - | 13041 | `	/* Save delimiter */` |
|     2935 | 13042 | `	pTmp = pGen->pEnd;` |
|        - | 13043 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     2935 | 13044 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     2935 | 13045 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        - | 13046 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|        - | 13047 | `		SyToken *pClose;` |
|     2935 | 13048 | `		pGen->pIn++;   /* Skip '(' */` |
|     2935 | 13049 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     2935 | 13050 | `		pEnd = pClose; /* Stop at ')' */` |
|     1465 | 13051 | `	}` |
|     2935 | 13052 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|        - | 13053 | `	/* Resolve the 'unset' builtin name once */` |
|     2935 | 13054 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      379 | 13055 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      379 | 13056 | `		if( pObj == 0 ){` |
|      ! 0 | 13057 | `			return SXERR_ABORT;` |
|        - | 13058 | `		}` |
|      379 | 13059 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      379 | 13060 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      187 | 13061 | `	}` |
|        - | 13062 | `	/* Compile each comma-separated argument */` |
|     9789 | 13063 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     6859 | 13064 | `		if( pGen->pIn < pNext ){` |
|     6859 | 13065 | `			pGen->pEnd = pNext;` |
|     6859 | 13066 | `			rc = PH7_CompileExpr(&(*pGen),` |
|        - | 13067 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|        - | 13068 | `				GenStateUnsetValidator);` |
|     6859 | 13069 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 13070 | `				return SXERR_ABORT;` |
|        - | 13071 | `			}` |
|     6859 | 13072 | `			if( rc != SXERR_EMPTY ){` |
|        - | 13073 | `				/* Emit call for this single argument */` |
|     6857 | 13074 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     6857 | 13075 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|     6857 | 13076 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     3426 | 13077 | `			}` |
|     3427 | 13078 | `		}` |
|        - | 13079 | `		/* Jump trailing commas */` |
|    10785 | 13080 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|     3931 | 13081 | `			pNext++;` |
|        5 | 13082 | `		}` |
|     6859 | 13083 | `		pGen->pIn = pNext;` |
|        5 | 13084 | `	}` |
|        - | 13085 | `	/* Skip past the closing ')' if present */` |
|     2935 | 13086 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     2935 | 13087 | `		pGen->pIn++;` |
|     1465 | 13088 | `	}` |
|        - | 13089 | `	/* Restore token stream */` |
|     2935 | 13090 | `	pGen->pEnd = pTmp;` |
|     2935 | 13091 | `	return SXRET_OK;` |
|     1470 | 13092 | `}` |
|        - | 13093 | `/*` |
|        - | 13094 | ` * PHP Language construct table.` |
|        - | 13095 | ` */` |
|        - | 13096 | `static const LangConstruct aLangConstruct[] = {` |
|        - | 13097 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|        - | 13098 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|        - | 13099 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|        - | 13100 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|        - | 13101 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|        - | 13102 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|        - | 13103 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|        - | 13104 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|        - | 13105 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|        - | 13106 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|        - | 13107 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|        - | 13108 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|        - | 13109 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|        - | 13110 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|        - | 13111 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|        - | 13112 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|        - | 13113 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|        - | 13114 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|        - | 13115 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|        - | 13116 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|        - | 13117 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|        - | 13118 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|        - | 13119 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|        - | 13120 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|        - | 13121 | `};` |
|        - | 13122 | `/*` |
|        - | 13123 | ` * Return a pointer to the statement handler routine associated` |
|        - | 13124 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|        - | 13125 | ` */` |
|  3806970 | 13126 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|        - | 13127 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|        - | 13128 | `	SyToken *pLookahed  /* Look-ahead token */` |
|        - | 13129 | `	)` |
|        5 | 13130 | `{` |
|  3806975 | 13131 | `	sxu32 n = 0;` |
| 15496100 | 13132 | `	for(;;){` |
| 30992205 | 13133 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   246845 | 13134 | `			break;` |
|        - | 13135 | `		}` |
| 30745365 | 13136 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  3560135 | 13137 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|      ! 0 | 13138 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|      ! 0 | 13139 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|        - | 13140 | `					/* 'static' (class context),return null */` |
|      ! 0 | 13141 | `					return 0;` |
|        - | 13142 | `				}` |
|      ! 0 | 13143 | `			}` |
|  3560130 | 13144 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       14 | 13145 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       14 | 13146 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|        - | 13147 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|        3 | 13148 | `				return 0;` |
|        - | 13149 | `			}` |
|        - | 13150 | `			/* Return a pointer to the handler.` |
|        - | 13151 | `			*/` |
|  3560133 | 13152 | `			return aLangConstruct[n].xConstruct;` |
|        - | 13153 | `		}` |
| 27185235 | 13154 | `		n++;` |
|        5 | 13155 | `	}` |
|   246845 | 13156 | `	if( pLookahed ){` |
|   246845 | 13157 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    46713 | 13158 | `			return PH7_CompileClassInterface;` |
|   200137 | 13159 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   187933 | 13160 | `			return PH7_CompileClass;` |
|    12209 | 13161 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|       77 | 13162 | `			return PH7_CompileTrait;` |
|        - | 13163 | `		}` |
|        - | 13164 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|        - | 13165 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|        - | 13166 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|        - | 13167 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     6066 | 13168 | `	}` |
|        - | 13169 | `	/* Not a language construct */` |
|    12137 | 13170 | `	return 0;` |
|  1903490 | 13171 | `}` |
|        - | 13172 | `/*` |
|        - | 13173 | ` * Check if the given keyword is in fact a PHP language construct.` |
|        - | 13174 | ` * Return TRUE on success. FALSE otheriwse.` |
|        - | 13175 | ` */` |
|    12134 | 13176 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|        5 | 13177 | `{` |
|        - | 13178 | `	int rc;` |
|    12139 | 13179 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|    12139 | 13180 | `	if( rc == FALSE ){` |
|    12020 | 13181 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      366 | 13182 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|        - | 13183 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|        - | 13184 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|        - | 13185 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|        - | 13186 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|        - | 13187 | `			*/` |
|        - | 13188 | `			){` |
|    12017 | 13189 | `				rc = TRUE;` |
|     6006 | 13190 | `		}` |
|     6010 | 13191 | `	}` |
|    12139 | 13192 | `	return rc;` |
|        5 | 13193 | `}` |
|        - | 13194 | `/*` |
|        - | 13195 | ` * Compile a PHP chunk.` |
|        - | 13196 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|        - | 13197 | ` * takes care of generating the appropriate error message.` |
|        - | 13198 | ` */` |
|        - | 13199 | `/*` |
|        - | 13200 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|        - | 13201 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|        - | 13202 | ` * the chunk token set it becomes the pending docblock. An existing` |
|        - | 13203 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|        - | 13204 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|        - | 13205 | ` * intervening non-declaration statements.` |
|        - | 13206 | ` */` |
|  7966162 | 13207 | `static void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|        5 | 13208 | `{` |
|  7966167 | 13209 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  7966167 | 13210 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  7966167 | 13211 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|        - | 13212 | `	sxu32 nIdx, n;` |
|  7966162 | 13213 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|  1537013 | 13214 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|        - | 13215 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|        - | 13216 | `		 * indexes do not map to the sidecar */` |
|  6429161 | 13217 | `		return;` |
|        - | 13218 | `	}` |
|  1537011 | 13219 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|        - | 13220 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|        - | 13221 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|  1537011 | 13222 | `	SySetReset(&pGen->aPendingAttrs);` |
|  4612517 | 13223 | `	for( n = 0 ; n < nT ; n++ ){` |
|  3075511 | 13224 | `		if( aT[n].nTokIdx != nIdx ){` |
|  3067579 | 13225 | `			continue;` |
|        - | 13226 | `		}` |
|     7937 | 13227 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|       29 | 13228 | `			pGen->sPendingDoc = aT[n].sText;` |
|     7925 | 13229 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|     7913 | 13230 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|     3954 | 13231 | `		}` |
|     3971 | 13232 | `	}` |
|  3983086 | 13233 | `}` |
|        - | 13234 | `/*` |
|        - | 13235 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|        - | 13236 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|        - | 13237 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|        - | 13238 | ` */` |
|  2123026 | 13239 | `static void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|        5 | 13240 | `{` |
|        - | 13241 | `	char *zDup;` |
|  2123031 | 13242 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|  2123011 | 13243 | `		return;` |
|        - | 13244 | `	}` |
|       35 | 13245 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       10 | 13246 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|       25 | 13247 | `	if( zDup ){` |
|       25 | 13248 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|       10 | 13249 | `	}` |
|       25 | 13250 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|  1061518 | 13251 | `}` |
|        - | 13252 | `/*` |
|        - | 13253 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|        - | 13254 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|        - | 13255 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|        - | 13256 | ` * names may point into the token text, which must outlive the raw script` |
|        - | 13257 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|        - | 13258 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|        - | 13259 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|        - | 13260 | ` */` |
|     7920 | 13261 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|        5 | 13262 | `{` |
|        - | 13263 | `	SySet *pToken;` |
|        - | 13264 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|        - | 13265 | `	char *zSpan;` |
|     7925 | 13266 | `	sxi32 rc = SXRET_OK;` |
|     7925 | 13267 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|      ! 0 | 13268 | `		return SXRET_OK;` |
|        - | 13269 | `	}` |
|    11885 | 13270 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     3960 | 13271 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|     7925 | 13272 | `	if( zSpan == 0 ){` |
|      ! 0 | 13273 | `		return SXRET_OK;` |
|        - | 13274 | `	}` |
|        - | 13275 | `	/* The token set must outlive compilation too: interned operands may` |
|        - | 13276 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|        - | 13277 | `	 * the number of attribute declarations in the program. */` |
|     7925 | 13278 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|     7925 | 13279 | `	if( pToken == 0 ){` |
|      ! 0 | 13280 | `		return SXRET_OK;` |
|        - | 13281 | `	}` |
|     7925 | 13282 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|     7925 | 13283 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|     7925 | 13284 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|     7925 | 13285 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|     7925 | 13286 | `	pSavedIn = pGen->pIn;` |
|     7925 | 13287 | `	pSavedEnd = pGen->pEnd;` |
|     7929 | 13288 | `	while( pIn < pEnd ){` |
|        - | 13289 | `		ph7_attribute sAttr;` |
|        - | 13290 | `		SyBlob sFQN;` |
|     7929 | 13291 | `		int bAbsolute = 0;` |
|     7929 | 13292 | `		SyZero(&sAttr,sizeof(sAttr));` |
|     7929 | 13293 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|     7929 | 13294 | `		sAttr.nLine = pIn->nLine;` |
|     7929 | 13295 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|       75 | 13296 | `			bAbsolute = 1;` |
|       75 | 13297 | `			pIn++;` |
|       35 | 13298 | `		}` |
|     7929 | 13299 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     7929 | 13300 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     7929 | 13301 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|     7929 | 13302 | `			pIn++;` |
|     7929 | 13303 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|      ! 0 | 13304 | `				SyBlobAppend(&sFQN,"\\",1);` |
|      ! 0 | 13305 | `				pIn++;` |
|      ! 0 | 13306 | `				continue;` |
|        - | 13307 | `			}` |
|     7929 | 13308 | `			break;` |
|      ! 0 | 13309 | `		}` |
|     7929 | 13310 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|        - | 13311 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|        - | 13312 | `			 * this feature; never turn it into a new fatal) */` |
|      ! 0 | 13313 | `			SyBlobRelease(&sFQN);` |
|      ! 0 | 13314 | `			break;` |
|        - | 13315 | `		}` |
|        - | 13316 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|        - | 13317 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|        - | 13318 | `		{` |
|     7929 | 13319 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|     7929 | 13320 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|     7929 | 13321 | `			char *zDup = 0;` |
|     7929 | 13322 | `			if( !bAbsolute ){` |
|     7859 | 13323 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|     7859 | 13324 | `				if( pImp ){` |
|      ! 0 | 13325 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|      ! 0 | 13326 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|      ! 0 | 13327 | `					if( zDup ){` |
|      ! 0 | 13328 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|      ! 0 | 13329 | `					}` |
|     7859 | 13330 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        - | 13331 | `					SyBlob sTmp;` |
|      ! 0 | 13332 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|      ! 0 | 13333 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      ! 0 | 13334 | `					SyBlobAppend(&sTmp,"\\",1);` |
|      ! 0 | 13335 | `					SyBlobAppend(&sTmp,zName,nName);` |
|      ! 0 | 13336 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      ! 0 | 13337 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|      ! 0 | 13338 | `					if( zDup ){` |
|      ! 0 | 13339 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|      ! 0 | 13340 | `					}` |
|      ! 0 | 13341 | `					SyBlobRelease(&sTmp);` |
|      ! 0 | 13342 | `				}` |
|     3927 | 13343 | `			}` |
|     7929 | 13344 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|     7929 | 13345 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|     7929 | 13346 | `				if( zDup ){` |
|     7929 | 13347 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|     3962 | 13348 | `				}` |
|     3962 | 13349 | `			}` |
|        - | 13350 | `		}` |
|     7929 | 13351 | `		SyBlobRelease(&sFQN);` |
|     7929 | 13352 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        - | 13353 | `			SyToken *pArgsEnd;` |
|     7827 | 13354 | `			pIn++;` |
|     7827 | 13355 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|    15663 | 13356 | `			while( pIn < pArgsEnd ){` |
|     7841 | 13357 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|     7841 | 13358 | `				sxi32 iDepth = 0;` |
|        - | 13359 | `				ph7_attr_arg sArgRec;` |
|    77925 | 13360 | `				while( pArgStop < pArgsEnd ){` |
|    70105 | 13361 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       11 | 13362 | `						iDepth++;` |
|    70100 | 13363 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       11 | 13364 | `						iDepth--;` |
|    70090 | 13365 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|       17 | 13366 | `						break;` |
|        - | 13367 | `					}` |
|    70089 | 13368 | `					pArgStop++;` |
|        5 | 13369 | `				}` |
|     7841 | 13370 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|     7841 | 13371 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     7836 | 13372 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|     7820 | 13373 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|       28 | 13374 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        9 | 13375 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|       19 | 13376 | `					if( zN ){` |
|       19 | 13377 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|        9 | 13378 | `					}` |
|       19 | 13379 | `					pArgStart += 2;` |
|        9 | 13380 | `				}` |
|     7841 | 13381 | `				if( pArgStart < pArgStop ){` |
|        - | 13382 | `					SySet *pInstrContainer;` |
|     7841 | 13383 | `					pGen->pIn = pArgStart;` |
|     7841 | 13384 | `					pGen->pEnd = pArgStop;` |
|     7841 | 13385 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     7841 | 13386 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|     7841 | 13387 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|     7841 | 13388 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|     7841 | 13389 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     7841 | 13390 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 13391 | `						pGen->pIn = pSavedIn;` |
|      ! 0 | 13392 | `						pGen->pEnd = pSavedEnd;` |
|      ! 0 | 13393 | `						return SXERR_ABORT;` |
|        - | 13394 | `					}` |
|     7841 | 13395 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|     3918 | 13396 | `				}` |
|     7841 | 13397 | `				pIn = pArgStop;` |
|     7841 | 13398 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|       17 | 13399 | `					pIn++;` |
|        8 | 13400 | `				}` |
|        5 | 13401 | `			}` |
|     7827 | 13402 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|     3911 | 13403 | `		}` |
|     7929 | 13404 | `		SySetPut(pOut,(const void *)&sAttr);` |
|     7929 | 13405 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        5 | 13406 | `			pIn++;` |
|        5 | 13407 | `			continue;` |
|        - | 13408 | `		}` |
|     7925 | 13409 | `		break;` |
|      ! 0 | 13410 | `	}` |
|     7925 | 13411 | `	pGen->pIn = pSavedIn;` |
|     7925 | 13412 | `	pGen->pEnd = pSavedEnd;` |
|     7925 | 13413 | `	return SXRET_OK;` |
|     3965 | 13414 | `}` |
|        - | 13415 | `/*` |
|        - | 13416 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|        - | 13417 | ` * every recorded group into pOut and clear the pending list.` |
|        - | 13418 | ` */` |
|  2123030 | 13419 | `static sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|        5 | 13420 | `{` |
|  2123035 | 13421 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|        - | 13422 | `	sxu32 n;` |
|        - | 13423 | `	sxi32 rc;` |
|  2130943 | 13424 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|     7913 | 13425 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|     7913 | 13426 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 13427 | `			return SXERR_ABORT;` |
|        - | 13428 | `		}` |
|     3959 | 13429 | `	}` |
|  2123035 | 13430 | `	SySetReset(&pGen->aPendingAttrs);` |
|  2123035 | 13431 | `	return SXRET_OK;` |
|  1061520 | 13432 | `}` |
|        - | 13433 | `/*` |
|        - | 13434 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|        - | 13435 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|        - | 13436 | ` * the main token stream, so the sidecar indexes map directly.` |
|        - | 13437 | ` */` |
|   718176 | 13438 | `static sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|        5 | 13439 | `{` |
|   718181 | 13440 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   718181 | 13441 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   718181 | 13442 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|        - | 13443 | `	sxu32 nIdx, n;` |
|        - | 13444 | `	sxi32 rc;` |
|   718176 | 13445 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   194535 | 13446 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   523651 | 13447 | `		return SXRET_OK;` |
|        - | 13448 | `	}` |
|   194535 | 13449 | `	nIdx = (sxu32)(pTok - pBase);` |
|   583593 | 13450 | `	for( n = 0 ; n < nT ; n++ ){` |
|   389063 | 13451 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|       13 | 13452 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|       13 | 13453 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 13454 | `				return SXERR_ABORT;` |
|        - | 13455 | `			}` |
|        6 | 13456 | `		}` |
|   194534 | 13457 | `	}` |
|   194535 | 13458 | `	return SXRET_OK;` |
|   359093 | 13459 | `}` |
|  5860968 | 13460 | `static sxi32 GenStateCompileChunk(` |
|        - | 13461 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 13462 | `	sxi32 iFlags         /* Compile flags */` |
|        - | 13463 | `	)` |
|        5 | 13464 | `{` |
|        - | 13465 | `	ProcLangConstruct xCons;` |
|        - | 13466 | `	sxi32 rc;` |
|  5860973 | 13467 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  3346414 | 13468 | `	for(;;){` |
|  6276903 | 13469 | `		int bStmtIsDeclare = 0;` |
|  6276903 | 13470 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 13471 | `			/* No more input to process */` |
|    53353 | 13472 | `			break;` |
|        - | 13473 | `		}` |
|        - | 13474 | `		/* Bind a directly-preceding docblock to this statement */` |
|  6223555 | 13475 | `		GenStateSetPendingDoc(&(*pGen));` |
|  6223555 | 13476 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|        - | 13477 | `			/* php: a statement-position attribute group must be followed by a` |
|        - | 13478 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|        - | 13479 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|        - | 13480 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|        - | 13481 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|     7831 | 13482 | `			int bAttrTarget = 0;` |
|     7826 | 13483 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|     3947 | 13484 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|     7773 | 13485 | `				bAttrTarget = 1;` |
|     3943 | 13486 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       59 | 13487 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       58 | 13488 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|       15 | 13489 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|        4 | 13490 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|        4 | 13491 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|        1 | 13492 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|       59 | 13493 | `					bAttrTarget = 1;` |
|       29 | 13494 | `				}` |
|       29 | 13495 | `			}` |
|     7831 | 13496 | `			if( !bAttrTarget ){` |
|      ! 0 | 13497 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 13498 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|      ! 0 | 13499 | `					&pGen->pIn->sData);` |
|      ! 0 | 13500 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 13501 | `					break;` |
|        - | 13502 | `				}` |
|      ! 0 | 13503 | `				SySetReset(&pGen->aPendingAttrs);` |
|      ! 0 | 13504 | `			}` |
|     3913 | 13505 | `		}` |
|        - | 13506 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|        - | 13507 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  6223555 | 13508 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  3834207 | 13509 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  3834207 | 13510 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|       47 | 13511 | `				bStmtIsDeclare = 1;` |
|       21 | 13512 | `			}` |
|  1917101 | 13513 | `		}` |
|  6223555 | 13514 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|        - | 13515 | `			/* Any non-declare top-level statement locks the strict_types` |
|        - | 13516 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|   415903 | 13517 | `			pGen->bStrictTypesLocked = 1;` |
|   207949 | 13518 | `		}` |
|  6223555 | 13519 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|        - | 13520 | `			/* Compile block */` |
|     3907 | 13521 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|     3907 | 13522 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 13523 | `				break;` |
|        - | 13524 | `			}` |
|     1956 | 13525 | `		}else{` |
|  6219653 | 13526 | `			xCons = 0;` |
|  6219653 | 13527 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|        - | 13528 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|        - | 13529 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|        - | 13530 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|    27263 | 13531 | `				xCons = PH7_CompileClassModifiers;` |
|  6206024 | 13532 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|        - | 13533 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|        - | 13534 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|       33 | 13535 | `				xCons = PH7_CompileEnum;` |
|  6192381 | 13536 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  3806975 | 13537 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        - | 13538 | `				/* Try to extract a language construct handler */` |
|  3806975 | 13539 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  3806975 | 13540 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|       13 | 13541 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 13542 | `						"Syntax error: Unexpected keyword '%z'",` |
|        8 | 13543 | `						&pGen->pIn->sData);` |
|        9 | 13544 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 13545 | `						break;` |
|        - | 13546 | `					}` |
|        - | 13547 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|        - | 13548 | `					 * this erroneous statement.` |
|        - | 13549 | `					 */` |
|        9 | 13550 | `					xCons = PH7_ErrorRecover;` |
|        4 | 13551 | `				}` |
|  4288882 | 13552 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    66513 | 13553 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|        - | 13554 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|      117 | 13555 | `				xCons = PH7_CompileLabel;` |
|       56 | 13556 | `			}` |
|  6219653 | 13557 | `			if( xCons == 0 ){` |
|        - | 13558 | `				/* Assume an expression an try to compile it */` |
|  2397411 | 13559 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  2397411 | 13560 | `				if(  rc != SXERR_EMPTY ){` |
|        - | 13561 | `					/* Pop l-value */` |
|  2397261 | 13562 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  1198628 | 13563 | `				}` |
|  1198708 | 13564 | `			}else{` |
|        - | 13565 | `				/* Go compile the sucker */` |
|  3822247 | 13566 | `				rc = xCons(&(*pGen));` |
|        - | 13567 | `			}` |
|  6219653 | 13568 | `			if( rc == SXERR_ABORT ){` |
|        - | 13569 | `				/* Request to abort compilation */` |
|       13 | 13570 | `				break;` |
|        - | 13571 | `			}` |
|        - | 13572 | `		}` |
|        - | 13573 | `		/* Ignore trailing semi-colons ';' */` |
| 10640399 | 13574 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  4416859 | 13575 | `			pGen->pIn++;` |
|        5 | 13576 | `		}` |
|  6223545 | 13577 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|        - | 13578 | `			/* Compile a single statement and return */` |
|  5807615 | 13579 | `			break;` |
|        - | 13580 | `		}` |
|        - | 13581 | `		/* LOOP ONE */` |
|        - | 13582 | `		/* LOOP TWO */` |
|        - | 13583 | `		/* LOOP THREE */` |
|        - | 13584 | `		/* LOOP FOUR */` |
|        5 | 13585 | `	}` |
|        - | 13586 | `	/* Return compilation status */` |
|  5860973 | 13587 | `	return rc;` |
|        5 | 13588 | `}` |
|        - | 13589 | `/*` |
|        - | 13590 | ` * Compile a Raw PHP chunk.` |
|        - | 13591 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|        - | 13592 | ` * takes care of generating the appropriate error message.` |
|        - | 13593 | ` */` |
|    53360 | 13594 | `static sxi32 PH7_CompilePHP(` |
|        - | 13595 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - | 13596 | `	SySet *pTokenSet,     /* Token set */` |
|        - | 13597 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|        - | 13598 | `	)` |
|        5 | 13599 | `{` |
|    53365 | 13600 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|        - | 13601 | `	sxi32 rc;` |
|        - | 13602 | `	/* Reset the token set (and its trivia sidecar) */` |
|    53365 | 13603 | `	SySetReset(&(*pTokenSet));` |
|    53365 | 13604 | `	SySetReset(&pGen->aTrivia);` |
|        - | 13605 | `	/* Mark as the default token set */` |
|    53365 | 13606 | `	pGen->pTokenSet = &(*pTokenSet);` |
|        - | 13607 | `	/* Advance the stream cursor */` |
|    53365 | 13608 | `	pGen->pRawIn++;` |
|        - | 13609 | `	/* Tokenize the PHP chunk first */` |
|    53365 | 13610 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|        - | 13611 | `	/* Point to the head and tail of the token stream. */` |
|    53365 | 13612 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|    53365 | 13613 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|    53365 | 13614 | `	if( is_expr ){` |
|      ! 0 | 13615 | `		rc = SXERR_EMPTY;` |
|      ! 0 | 13616 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - | 13617 | `			/* A simple expression,compile it */` |
|      ! 0 | 13618 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|      ! 0 | 13619 | `		}` |
|        - | 13620 | `		/* Emit the DONE instruction */` |
|      ! 0 | 13621 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      ! 0 | 13622 | `		return SXRET_OK;` |
|        - | 13623 | `	}` |
|    53365 | 13624 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|        - | 13625 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|        - | 13626 | `		/*` |
|        - | 13627 | `		 * Shortcut syntax for the 'echo' language construct.` |
|        - | 13628 | `		 * According to the PHP reference manual:` |
|        - | 13629 | `		 *  echo() also has a shortcut syntax, where you can` |
|        - | 13630 | `		 *  immediately follow` |
|        - | 13631 | `		 *  the opening tag with an equals sign as follows:` |
|        - | 13632 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|        - | 13633 | `		 * Symisc extension:` |
|        - | 13634 | `		 *   This short syntax works with all PHP opening` |
|        - | 13635 | `		 *   tags unlike the default PHP engine that handle` |
|        - | 13636 | `		 *   only short tag.` |
|        - | 13637 | `		 */` |
|        - | 13638 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|        3 | 13639 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|        3 | 13640 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|        3 | 13641 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|        3 | 13642 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|        3 | 13643 | `		if( rc != SXERR_EMPTY ){` |
|        3 | 13644 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        1 | 13645 | `		}` |
|        3 | 13646 | `		return SXRET_OK;` |
|        - | 13647 | `	}` |
|        - | 13648 | `	/* Compile the PHP chunk */` |
|    53363 | 13649 | `	rc = GenStateCompileChunk(pGen,0);` |
|        - | 13650 | `	/* Fix exceptions jumps */` |
|    53363 | 13651 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        - | 13652 | `	/* Fix gotos now, the jump destination is resolved */` |
|    53363 | 13653 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|        3 | 13654 | `		rc = SXERR_ABORT;` |
|        1 | 13655 | `	}` |
|        - | 13656 | `	/* Reset container */` |
|    53363 | 13657 | `	SySetReset(&pGen->aGoto);` |
|    53363 | 13658 | `	SySetReset(&pGen->aLabel);` |
|    53363 | 13659 | `	SySetReset(&pGen->aNullsafeJmp);` |
|        - | 13660 | `	/* Compilation result */` |
|    53363 | 13661 | `	return rc;` |
|    26685 | 13662 | `}` |
|        - | 13663 | `/*` |
|        - | 13664 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|        - | 13665 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|        - | 13666 | ` * This is the only compile interface exported from this file.` |
|        - | 13667 | ` */` |
|    56420 | 13668 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|        - | 13669 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|        - | 13670 | `	SyString *pScript,  /* Script to compile */` |
|        - | 13671 | `	sxi32 iFlags        /* Compile flags */` |
|        - | 13672 | `	)` |
|        5 | 13673 | `{` |
|        - | 13674 | `	SySet aPhpToken,aRawToken;` |
|        - | 13675 | `	ph7_gen_state *pCodeGen;` |
|        - | 13676 | `	ph7_value *pRawObj;` |
|        - | 13677 | `	sxu32 nObjIdx;` |
|        - | 13678 | `	sxi32 nRawObj;` |
|        - | 13679 | `	int is_expr;` |
|        - | 13680 | `	sxi8 bSavedStrict;` |
|        - | 13681 | `	sxi8 bSavedStrictLocked;` |
|        - | 13682 | `	sxi32 rc;` |
|    56425 | 13683 | `	if( pScript->nByte < 1 ){` |
|        - | 13684 | `		/* Nothing to compile */` |
|      ! 0 | 13685 | `		return PH7_OK;` |
|        - | 13686 | `	}` |
|        - | 13687 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|        - | 13688 | `	 * file's flags so include/require restore them on return. */` |
|    56425 | 13689 | `	pCodeGen = &pVm->sCodeGen;` |
|    56425 | 13690 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|    56425 | 13691 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|    56425 | 13692 | `	pCodeGen->bStrictTypes = 0;` |
|    56425 | 13693 | `	pCodeGen->bStrictTypesLocked = 0;` |
|        - | 13694 | `	/* Initialize the tokens containers */` |
|    56425 | 13695 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|    56425 | 13696 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|    56425 | 13697 | `	SySetAlloc(&aPhpToken,0xc0);` |
|    56425 | 13698 | `	is_expr = 0;` |
|    56425 | 13699 | `	if( iFlags & PH7_PHP_ONLY ){` |
|        - | 13700 | `		SyToken sTmp;` |
|        - | 13701 | `		/* PHP only: -*/` |
|    42827 | 13702 | `		sTmp.nLine = 1;` |
|    42827 | 13703 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    42827 | 13704 | `		sTmp.pUserData = 0;` |
|    42827 | 13705 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    42827 | 13706 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    42827 | 13707 | `		if( iFlags & PH7_PHP_EXPR ){` |
|        - | 13708 | `			/* A simple PHP expression */` |
|      ! 0 | 13709 | `			is_expr = 1;` |
|      ! 0 | 13710 | `		}` |
|    21416 | 13711 | `	}else{` |
|        - | 13712 | `		/* Tokenize raw text */` |
|    13603 | 13713 | `		SySetAlloc(&aRawToken,32);` |
|    13603 | 13714 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|        - | 13715 | `	}` |
|        - | 13716 | `	/* Process high-level tokens */` |
|    56425 | 13717 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|    56425 | 13718 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|    56425 | 13719 | `	rc = PH7_OK;` |
|    56425 | 13720 | `	if( is_expr ){` |
|        - | 13721 | `		/* Compile the expression */` |
|      ! 0 | 13722 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|      ! 0 | 13723 | `		goto cleanup;` |
|        - | 13724 | `	}` |
|    56425 | 13725 | `	nObjIdx = 0;` |
|        - | 13726 | `	/* Each compilation unit starts in the global namespace.` |
|        - | 13727 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|        - | 13728 | `	 * preventing namespace bleeding across include()d files. */` |
|    56425 | 13729 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|        - | 13730 | `	/* Start the compilation process */` |
|    35015 | 13731 | `	for(;;){` |
|   123383 | 13732 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|    56413 | 13733 | `			break; /* No more tokens to process */` |
|        - | 13734 | `		}` |
|    66975 | 13735 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|        - | 13736 | `			/* Compile the PHP chunk */` |
|    53365 | 13737 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|    53365 | 13738 | `			if( rc == SXERR_ABORT ){` |
|       15 | 13739 | `				break;` |
|        - | 13740 | `			}` |
|    53353 | 13741 | `			continue;` |
|        - | 13742 | `		}` |
|        - | 13743 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|    13615 | 13744 | `		nRawObj = 0;` |
|    27267 | 13745 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|        - | 13746 | `			/* Consume the raw chunk without any processing */` |
|    13657 | 13747 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|    13657 | 13748 | `			if( pRawObj == 0 ){` |
|      ! 0 | 13749 | `				rc = SXERR_MEM;` |
|      ! 0 | 13750 | `				break;` |
|        - | 13751 | `			}` |
|        - | 13752 | `			/* Mark as constant and emit the load constant instruction */` |
|    13657 | 13753 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|    13657 | 13754 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|    13657 | 13755 | `			++nRawObj;` |
|    13657 | 13756 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|        5 | 13757 | `		}` |
|    13615 | 13758 | `		if( nRawObj > 0 ){` |
|        - | 13759 | `			/* Emit the consume instruction */` |
|    13615 | 13760 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     6805 | 13761 | `		}` |
|    28215 | 13762 | `	}` |
|    28210 | 13763 | `cleanup:` |
|    56425 | 13764 | `	SySetRelease(&aRawToken);` |
|    56425 | 13765 | `	SySetRelease(&aPhpToken);` |
|        - | 13766 | `	/* Restore outer file's strict_types scope */` |
|    56425 | 13767 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|    56425 | 13768 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|    56425 | 13769 | `	return rc;` |
|    28215 | 13770 | `}` |
|        - | 13771 | `/*` |
|        - | 13772 | ` * Utility routines.Initialize the code generator.` |
|        - | 13773 | ` */` |
|     3884 | 13774 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|        - | 13775 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 13776 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|        - | 13777 | `	void *pErrData     /* Last argument to xErr() */` |
|        - | 13778 | `	)` |
|        5 | 13779 | `{` |
|     3889 | 13780 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 13781 | `	/* Zero the structure */` |
|     3889 | 13782 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|        - | 13783 | `	/* Initial state */` |
|     3889 | 13784 | `	pGen->pVm  = &(*pVm);` |
|     3889 | 13785 | `	pGen->xErr = xErr;` |
|     3889 | 13786 | `	pGen->pErrData = pErrData;` |
|     3889 | 13787 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|     3889 | 13788 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|     3889 | 13789 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|     3889 | 13790 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|     3889 | 13791 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|     3889 | 13792 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|     3889 | 13793 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|        - | 13794 | `	/* Error log buffer */` |
|     3889 | 13795 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|        - | 13796 | `	/* General purpose working buffer */` |
|     3889 | 13797 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|        - | 13798 | `	/* Namespace state */` |
|     3889 | 13799 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     3889 | 13800 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|     3889 | 13801 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|     3889 | 13802 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|        - | 13803 | `	/* Create the global scope */` |
|     3889 | 13804 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|        - | 13805 | `	/* Point to the global scope */` |
|     3889 | 13806 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     3889 | 13807 | `	return SXRET_OK;` |
|        5 | 13808 | `}` |
|        - | 13809 | `/*` |
|        - | 13810 | ` * Utility routines. Reset the code generator to it's initial state.` |
|        - | 13811 | ` */` |
|    59924 | 13812 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|        - | 13813 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 13814 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|        - | 13815 | `	void *pErrData     /* Last argument to xErr() */` |
|        - | 13816 | `	)` |
|        5 | 13817 | `{` |
|    59929 | 13818 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 13819 | `	GenBlock *pBlock,*pParent;` |
|        - | 13820 | `	/* Reset state */` |
|    59929 | 13821 | `	SySetReset(&pGen->aLabel);` |
|    59929 | 13822 | `	SySetReset(&pGen->aGoto);` |
|    59929 | 13823 | `	SySetReset(&pGen->aNullsafeJmp);` |
|    59929 | 13824 | `	SySetReset(&pGen->aTrivia);` |
|    59929 | 13825 | `	SySetReset(&pGen->aPendingAttrs);` |
|    59929 | 13826 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|    59929 | 13827 | `	SyBlobRelease(&pGen->sErrBuf);` |
|    59929 | 13828 | `	SyBlobRelease(&pGen->sWorker);` |
|    59929 | 13829 | `	SyBlobRelease(&pGen->sNamespace);` |
|    59929 | 13830 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    59929 | 13831 | `	SyHashRelease(&pGen->hUseImports);` |
|    59929 | 13832 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    59929 | 13833 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|    59929 | 13834 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    59929 | 13835 | `	SyHashRelease(&pGen->hUseConstImports);` |
|    59929 | 13836 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|        - | 13837 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|        - | 13838 | `	 * They intern variable names and literal strings that are referenced by` |
|        - | 13839 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|        - | 13840 | `	 * Releasing them would either leak the interned strings or require freeing` |
|        - | 13841 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|        - | 13842 | `	 * number of unique names, which is acceptable. */` |
|        - | 13843 | `	/* Point to the global scope */` |
|    59929 | 13844 | `	pBlock = pGen->pCurrent;` |
|    59929 | 13845 | `	while( pBlock->pParent != 0 ){` |
|      ! 0 | 13846 | `		pParent = pBlock->pParent;` |
|      ! 0 | 13847 | `		GenStateFreeBlock(pBlock);` |
|      ! 0 | 13848 | `		pBlock = pParent;` |
|      ! 0 | 13849 | `	}` |
|    59929 | 13850 | `	pGen->xErr = xErr;` |
|    59929 | 13851 | `	pGen->pErrData = pErrData;` |
|    59929 | 13852 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    59929 | 13853 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|    59929 | 13854 | `	pGen->pIn = pGen->pEnd = 0;` |
|    59929 | 13855 | `	pGen->nErr = 0;` |
|    59929 | 13856 | `	return SXRET_OK;` |
|        5 | 13857 | `}` |
|        - | 13858 | `/*` |
|        - | 13859 | ` * Generate a compile-time error message.` |
|        - | 13860 | ` * If the error count limit is reached (usually 15 error message)` |
|        - | 13861 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|        - | 13862 | ` * abort compilation immediately.` |
|        - | 13863 | ` */` |
|      652 | 13864 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|        5 | 13865 | `{` |
|      657 | 13866 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|      657 | 13867 | `	const char *zErr = "Error";` |
|        - | 13868 | `	SyString *pFile;` |
|        - | 13869 | `	va_list ap;` |
|        - | 13870 | `	sxi32 rc;` |
|        - | 13871 | `	/* Reset the working buffer */` |
|      657 | 13872 | `	SyBlobReset(pWorker);` |
|        - | 13873 | `	/* Peek the processed file path if available */` |
|      657 | 13874 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|      657 | 13875 | `	if( nErrType == E_ERROR ){` |
|        - | 13876 | `		/* Increment the error counter */` |
|      543 | 13877 | `		pGen->nErr++;` |
|      543 | 13878 | `		if( pGen->nErr > 15 ){` |
|        - | 13879 | `			/* Error count limit reached */` |
|        6 | 13880 | `			if( pGen->xErr ){` |
|        6 | 13881 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|        6 | 13882 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|        6 | 13883 | `				if( pFile ){` |
|        6 | 13884 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|        2 | 13885 | `				}` |
|        6 | 13886 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|        6 | 13887 | `				if( SyBlobLength(pWorker) > 0 ){` |
|        6 | 13888 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|        2 | 13889 | `				}` |
|        2 | 13890 | `			}` |
|        - | 13891 | `			/* Abort immediately */` |
|        6 | 13892 | `			return SXERR_ABORT;` |
|        - | 13893 | `		}` |
|      267 | 13894 | `	}` |
|      653 | 13895 | `	if( pGen->xErr == 0 ){` |
|        - | 13896 | `		/* No available error consumer,return immediately */` |
|        3 | 13897 | `		return SXRET_OK;` |
|        - | 13898 | `	}` |
|      650 | 13899 | `	switch(nErrType){` |
|      536 | 13900 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|       32 | 13901 | `	case E_WARNING: zErr = "Warning";     break;` |
|       82 | 13902 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       12 | 13903 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|      ! 0 | 13904 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|      ! 0 | 13905 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|      ! 0 | 13906 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|      ! 0 | 13907 | `	default:` |
|      ! 0 | 13908 | `		break;` |
|        - | 13909 | `	}` |
|      650 | 13910 | `	rc = SXRET_OK;` |
|        - | 13911 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|      650 | 13912 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|      650 | 13913 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|      650 | 13914 | `	va_start(ap,zFormat);` |
|      650 | 13915 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|      650 | 13916 | `	va_end(ap);` |
|      650 | 13917 | `	if( pFile ){` |
|      650 | 13918 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|      323 | 13919 | `	}` |
|        - | 13920 | `	/* Append a new line */` |
|      650 | 13921 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|      650 | 13922 | `	if( SyBlobLength(pWorker) > 0 ){` |
|        - | 13923 | `		/* Consume the generated error message */` |
|      650 | 13924 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|      323 | 13925 | `	}` |
|      650 | 13926 | `	return rc;` |
|      331 | 13927 | `}` |
|        - | 13928 |  |
