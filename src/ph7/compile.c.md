# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 6790/8410 lines (80.74%)

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
|  5861492 |   161 | `static void GenStateInitBlock(` |
|        - |   162 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |   163 | `	GenBlock *pBlock,    /* Target block */` |
|        - |   164 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|        - |   165 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|        - |   166 | `	void *pUserData      /* Upper layer private data */` |
|        - |   167 | `	)` |
|        5 |   168 | `{` |
|        - |   169 | `	/* Initialize block fields */` |
|  5861497 |   170 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  5861497 |   171 | `	pBlock->pUserData   = pUserData;` |
|  5861497 |   172 | `	pBlock->pGen        = pGen;` |
|  5861497 |   173 | `	pBlock->iFlags      = iType;` |
|  5861497 |   174 | `	pBlock->pParent     = 0;` |
|  5861497 |   175 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  5861497 |   176 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  5861497 |   177 | `}` |
|        - |   178 | `/*` |
|        - |   179 | ` * Allocate a new block instance.` |
|        - |   180 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|        - |   181 | ` * on success.Otherwise generate a compile-time error and abort` |
|        - |   182 | ` * processing on failure.` |
|        - |   183 | ` */` |
|  5857608 |   184 | `static sxi32 GenStateEnterBlock(` |
|        - |   185 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - |   186 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|        - |   187 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|        - |   188 | `	void *pUserData,      /* Upper layer private data */` |
|        - |   189 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|        - |   190 | `	)` |
|        5 |   191 | `{` |
|        - |   192 | `	GenBlock *pBlock;` |
|        - |   193 | `	/* Allocate a new block instance */` |
|  5857613 |   194 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  5857613 |   195 | `	if( pBlock == 0 ){` |
|        - |   196 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   197 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   198 | `		 */` |
|      ! 0 |   199 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|        - |   200 | `		/* Abort processing immediately */` |
|      ! 0 |   201 | `		return SXERR_ABORT;` |
|        - |   202 | `	}` |
|        - |   203 | `	/* Zero the structure */` |
|  5857613 |   204 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  5857613 |   205 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|        - |   206 | `	/* Link to the parent block */` |
|  5857613 |   207 | `	pBlock->pParent = pGen->pCurrent;` |
|        - |   208 | `	/* Mark as the current block */` |
|  5857613 |   209 | `	pGen->pCurrent = pBlock;` |
|  5857613 |   210 | `	if( ppBlock ){` |
|        - |   211 | `		/* Write a pointer to the new instance */` |
|  2838147 |   212 | `		*ppBlock = pBlock;` |
|  1419071 |   213 | `	}` |
|  5857613 |   214 | `	return SXRET_OK;` |
|  2928809 |   215 | `}` |
|        - |   216 | `/*` |
|        - |   217 | ` * Release block fields without freeing the whole instance.` |
|        - |   218 | ` */` |
|  5857600 |   219 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|        5 |   220 | `{` |
|  5857605 |   221 | `	SySetRelease(&pBlock->aPostContFix);` |
|  5857605 |   222 | `	SySetRelease(&pBlock->aJumpFix);` |
|  5857605 |   223 | `}` |
|        - |   224 | `/*` |
|        - |   225 | ` * Release a block.` |
|        - |   226 | ` */` |
|  5857600 |   227 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|        5 |   228 | `{` |
|  5857605 |   229 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  5857605 |   230 | `	GenStateReleaseBlock(&(*pBlock));` |
|        - |   231 | `	/* Free the instance */` |
|  5857605 |   232 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  5857605 |   233 | `}` |
|        - |   234 | `/*` |
|        - |   235 | ` * POP and release a block from the stack of compiled blocks.` |
|        - |   236 | ` */` |
|  5857600 |   237 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|        5 |   238 | `{` |
|  5857605 |   239 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  5857605 |   240 | `	if( pBlock == 0 ){` |
|        - |   241 | `		/* No more block to pop */` |
|      ! 0 |   242 | `		return SXERR_EMPTY;` |
|        - |   243 | `	}` |
|        - |   244 | `	/* Point to the upper block */` |
|  5857605 |   245 | `	pGen->pCurrent = pBlock->pParent;` |
|  5857605 |   246 | `	if( ppBlock ){` |
|        - |   247 | `		/* Write a pointer to the popped block */` |
|      ! 0 |   248 | `		*ppBlock = pBlock;` |
|      ! 0 |   249 | `	}else{` |
|        - |   250 | `		/* Safely release the block */` |
|  5857605 |   251 | `		GenStateFreeBlock(&(*pBlock));` |
|        - |   252 | `	}` |
|  5857605 |   253 | `	return SXRET_OK;` |
|  2928805 |   254 | `}` |
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
|  2212814 |   265 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|        5 |   266 | `{` |
|        - |   267 | `	JumpFixup sJumpFix;` |
|        - |   268 | `	sxi32 rc;` |
|        - |   269 | `	/* Init the JumpFixup structure */` |
|  2212819 |   270 | `	sJumpFix.nJumpType = nJumpType;` |
|  2212819 |   271 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|        - |   272 | `	/* Insert in the jump fixup table */` |
|  2212819 |   273 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  2212819 |   274 | `	return rc;` |
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
|  4168174 |   287 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|        5 |   288 | `{` |
|        - |   289 | `	JumpFixup *aFix;` |
|        - |   290 | `	VmInstr *pInstr;` |
|        - |   291 | `	sxu32 nFixed;` |
|        - |   292 | `	sxu32 n;` |
|        - |   293 | `	/* Point to the jump fixup table */` |
|  4168179 |   294 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|        - |   295 | `	/* Fix the desired jumps */` |
|  8106811 |   296 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  3938637 |   297 | `		if( aFix[n].nJumpType < 0 ){` |
|        - |   298 | `			/* Already fixed */` |
|  1414251 |   299 | `			continue;` |
|        - |   300 | `		}` |
|  2524391 |   301 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|        - |   302 | `			/* Not of our interest */` |
|   311579 |   303 | `			continue;` |
|        - |   304 | `		}` |
|        - |   305 | `		/* Point to the instruction to fix */` |
|  2212817 |   306 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  2212817 |   307 | `		if( pInstr ){` |
|  2212817 |   308 | `			pInstr->iP2 = nJumpDest;` |
|  2212817 |   309 | `			nFixed++;` |
|        - |   310 | `			/* Mark as fixed */` |
|  2212817 |   311 | `			aFix[n].nJumpType = -1;` |
|  1106406 |   312 | `		}` |
|  1106411 |   313 | `	}` |
|        - |   314 | `	/* Total number of fixed jumps */` |
|  4168179 |   315 | `	return nFixed;` |
|        5 |   316 | `}` |
|        - |   317 | `/*` |
|        - |   318 | ` * Fix a 'goto' now the jump destination is resolved.` |
|        - |   319 | ` * The goto statement can be used to jump to another section` |
|        - |   320 | ` * in the program.` |
|        - |   321 | ` * Refer to the routine responsible of compiling the goto` |
|        - |   322 | ` * statement for more information.` |
|        - |   323 | ` */` |
|  1466638 |   324 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|        5 |   325 | `{` |
|        - |   326 | `	JumpFixup *pJump,*aJumps;` |
|        - |   327 | `	Label *pLabel,*aLabel;` |
|        - |   328 | `	VmInstr *pInstr;` |
|        - |   329 | `	sxi32 rc;` |
|        - |   330 | `	sxu32 n;` |
|        - |   331 | `	/* Point to the goto table */` |
|  1466643 |   332 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|        - |   333 | `	/* Fix */` |
|  1466789 |   334 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  1466641 |   359 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  1466773 |   360 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|      137 |   361 | `		if( aLabel[n].bRef == FALSE ){` |
|        - |   362 | `			/* Emit a warning */` |
|       40 |   363 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|       24 |   364 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|       12 |   365 | `		}` |
|       71 |   366 | `	}` |
|  1466641 |   367 | `	return SXRET_OK;` |
|   733324 |   368 | `}` |
|        - |   369 | `/*` |
|        - |   370 | ` * Check if a given token value is installed in the literal table.` |
|        - |   371 | ` */` |
|  7357874 |   372 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|        5 |   373 | `{` |
|        - |   374 | `	SyHashEntry *pEntry;` |
|  7357879 |   375 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  7357879 |   376 | `	if( pEntry == 0 ){` |
|  1938389 |   377 | `		return SXERR_NOTFOUND;` |
|        - |   378 | `	}` |
|  5419495 |   379 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  5419495 |   380 | `	return SXRET_OK;` |
|  3678942 |   381 | `}` |
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
|  1938384 |   392 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|        5 |   393 | `{` |
|  1938389 |   394 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  1938389 |   395 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   969192 |   396 | `	}` |
|  1938389 |   397 | `	return SXRET_OK;` |
|        5 |   398 | `}` |
|        - |   399 | `/*` |
|        - |   400 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|        - |   401 | ` * in the constant table.` |
|        - |   402 | ` */` |
|  1295884 |   403 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|        5 |   404 | `{` |
|        - |   405 | `	ph7_value *pObj;` |
|  1295889 |   406 | `	sxu32 nIdx = 0; /* cc warning */` |
|        - |   407 | `	/* Reserve a new constant */` |
|  1295889 |   408 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  1295889 |   409 | `	if( pObj == 0 ){` |
|      ! 0 |   410 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |   411 | `		return 0;` |
|        - |   412 | `	}` |
|  1295889 |   413 | `	*pIdx = nIdx;` |
|        - |   414 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|        - |   415 | `	 * the constant string iterals table [optimization purposes].` |
|        - |   416 | `	 */` |
|  1295889 |   417 | `	return pObj;` |
|   647947 |   418 | `}` |
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
|  3706960 |   433 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|        5 |   434 | `{` |
|        - |   435 | `	VmCallArgMap *pMap;` |
|  3706965 |   436 | `	if( !pGen->bStrictTypes ) return p3;` |
|       39 |   437 | `	if( p3 == 0 ){` |
|       35 |   438 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|       35 |   439 | `		if( pMap == 0 ) return 0;` |
|       35 |   440 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|       35 |   441 | `		p3 = (void *)pMap;` |
|       16 |   442 | `	}` |
|       39 |   443 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|       39 |   444 | `	return p3;` |
|  1853485 |   445 | `}` |
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
|  1296872 |   509 | `static int GenStateFindBadNumericSeparator(` |
|        - |   510 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|        5 |   511 | `{` |
|  1296877 |   512 | `	const char *z = pRaw->zString;` |
|  1296877 |   513 | `	sxu32 n = pRaw->nByte;` |
|  1296877 |   514 | `	int base = 10;` |
|        - |   515 | `	sxu32 i, start;` |
|  1296877 |   516 | `	if( n < 2 ) return 0;` |
|   404227 |   517 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|       80 |   518 | `		base = 16;` |
|   404188 |   519 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|      284 |   520 | `		base = 2;` |
|      141 |   521 | `	}` |
|  1306861 |   522 | `	for( i = 0; i < n; ++i ){` |
|   902653 |   523 | `		if( z[i] != '_' ) continue;` |
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
|   404213 |   540 | `	return 0;` |
|   648441 |   541 | `}` |
|        - |   542 | `/*` |
|        - |   543 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|        - |   544 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|        - |   545 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|        - |   546 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|        - |   547 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|        - |   548 | ` * so callers can bail from the current construct).` |
|        - |   549 | ` */` |
|  1296872 |   550 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|        5 |   551 | `{` |
|  1296877 |   552 | `	const char *zBad = 0;` |
|  1296877 |   553 | `	sxu32 nBad = 0;` |
|        - |   554 | `	SyString sBad;` |
|        - |   555 | `	sxi32 rc;` |
|  1296877 |   556 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  1296863 |   557 | `		return SXRET_OK;` |
|        - |   558 | `	}` |
|       18 |   559 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|       18 |   560 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|        - |   561 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|       18 |   562 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |   563 | `		return SXERR_ABORT;` |
|        - |   564 | `	}` |
|       18 |   565 | `	return SXERR_SYNTAX;` |
|   648441 |   566 | `}` |
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
|  1296858 |   583 | `static sxi32 GenStateStripNumericSeparators(` |
|        - |   584 | `	SyMemBackend *pAlloc,` |
|        - |   585 | `	const SyString *pToken,` |
|        - |   586 | `	char *zScratch, sxu32 nScratch,` |
|        - |   587 | `	SyString *pOut, char **pzAlloc)` |
|        5 |   588 | `{` |
|        - |   589 | `	sxu32 i, j;` |
|  1296863 |   590 | `	int hasUnderscore = 0;` |
|        - |   591 | `	char *zBuf;` |
|  1296863 |   592 | `	*pzAlloc = 0;` |
|  3090081 |   593 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  1793475 |   594 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   896614 |   595 | `	}` |
|  1296863 |   596 | `	if( !hasUnderscore ){` |
|  1296611 |   597 | `		SyStringDupPtr(pOut, pToken);` |
|  1296611 |   598 | `		return SXRET_OK;` |
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
|   648434 |   615 | `}` |
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
|  1295918 |   651 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|        5 |   652 | `{` |
|  1295923 |   653 | `	const char *z = pNum->zString;` |
|  1295923 |   654 | `	const char *zEnd = z + pNum->nByte;` |
|        - |   655 | `	const char *p, *q;` |
|        - |   656 | `	int n;` |
|  1295923 |   657 | `	*pbDecimal = FALSE;` |
|  1295923 |   658 | `	if( z >= zEnd ){` |
|      ! 0 |   659 | `		return FALSE;` |
|        - |   660 | `	}` |
|  1295923 |   661 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
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
|  1295847 |   676 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
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
|  1295567 |   691 | `	}else if( z[0] == '0' ){` |
|        - |   692 | `		/* Octal: INT64_MAX == 0o777...7 (21 significant octal digits). Skip the` |
|        - |   693 | `		 * leading zeros (incl. the base '0'); a non-octal char such as the 8.1` |
|        - |   694 | `		 * "0o" marker ends the run and leaves it to the int path (as today). */` |
|   359473 |   695 | `		p = z;` |
|   718943 |   696 | `		while( p < zEnd && p[0] == '0' ){ p++; }` |
|   359701 |   697 | `		for( q = p, n = 0; q < zEnd && q[0] >= '0' && q[0] <= '7'; q++ ){ n++; }` |
|   359473 |   698 | `		if( n <= 21 ){` |
|   359471 |   699 | `			return FALSE;` |
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
|   936099 |   712 | `	p = z;` |
|   936099 |   713 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
|  2363385 |   714 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|   936099 |   715 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|       25 |   716 | `		*pbDecimal = TRUE;` |
|       25 |   717 | `		return TRUE;` |
|        - |   718 | `	}` |
|   936075 |   719 | `	return FALSE;` |
|   647964 |   720 | `}` |
|  1296844 |   721 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |   722 | `{` |
|  1296849 |   723 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  1296849 |   724 | `	sxu32 nIdx = 0;` |
|        - |   725 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  1296849 |   726 | `	char *zAlloc = 0;` |
|        - |   727 | `	SyString sNum;` |
|        - |   728 | `	sxi32 rc;` |
|   648422 |   729 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  1296849 |   730 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  1296849 |   731 | `	if( rc != SXRET_OK ){` |
|       14 |   732 | `		return rc;` |
|        - |   733 | `	}` |
|  1945256 |   734 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   648417 |   735 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  1296839 |   736 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   737 | `		return SXERR_ABORT;` |
|        - |   738 | `	}` |
|  1296839 |   739 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|        - |   740 | `		ph7_value *pObj;` |
|        - |   741 | `		sxi64 iValue;` |
|  1295923 |   742 | `		ph7_real rOverflow = 0;` |
|  1295923 |   743 | `		int bDecimalOverflow = 0;` |
|  1295923 |   744 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
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
|  1295889 |   761 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|  1295889 |   762 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  1295889 |   763 | `			if( pObj == 0 ){` |
|      ! 0 |   764 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |   765 | `				return SXERR_ABORT;` |
|        - |   766 | `			}` |
|  1295889 |   767 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|        - |   768 | `		}` |
|   647964 |   769 | `	}else{` |
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
|  1296839 |   782 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        - |   783 | `	/* Emit the load constant instruction */` |
|  1296839 |   784 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - |   785 | `	/* Node successfully compiled */` |
|  1296839 |   786 | `	return SXRET_OK;` |
|   648427 |   787 | `}` |
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
|  2995402 |   799 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |   800 | `{` |
|  2995407 |   801 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|        - |   802 | `	const char *zIn,*zCur,*zEnd;` |
|        - |   803 | `	ph7_value *pObj;` |
|        - |   804 | `	sxu32 nIdx;` |
|  2995407 |   805 | `	nIdx = 0; /* Prevent compiler warning */` |
|        - |   806 | `	/* Delimit the string */` |
|  2995407 |   807 | `	zIn  = pStr->zString;` |
|  2995407 |   808 | `	zEnd = &zIn[pStr->nByte];` |
|  2995407 |   809 | `	if( zIn >= zEnd ){` |
|        - |   810 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|        - |   811 | `		 * rather than reserving a new object each time. */` |
|   136133 |   812 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|   136133 |   813 | `		return SXRET_OK;` |
|        - |   814 | `	}` |
|  2859279 |   815 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|        - |   816 | `		/* Already processed,emit the load constant instruction` |
|        - |   817 | `		 * and return.` |
|        - |   818 | `		 */` |
|  1833531 |   819 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  1833531 |   820 | `		return SXRET_OK;` |
|        - |   821 | `	}` |
|        - |   822 | `	/* Reserve a new constant */` |
|  1025753 |   823 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  1025753 |   824 | `	if( pObj == 0 ){` |
|      ! 0 |   825 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |   826 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |   827 | `		return SXERR_ABORT;` |
|        - |   828 | `	}` |
|  1025753 |   829 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|        - |   830 | `	/* Compile the node */` |
|  1025807 |   831 | `	for(;;){` |
|  2051619 |   832 | `		if( zIn >= zEnd ){` |
|        - |   833 | `			/* End of input */` |
|  1025753 |   834 | `			break;` |
|        - |   835 | `		}` |
|  1025871 |   836 | `		zCur = zIn;` |
| 19901719 |   837 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
| 18875853 |   838 | `			zIn++;` |
|        5 |   839 | `		}` |
|  1025871 |   840 | `		if( zIn > zCur ){` |
|        - |   841 | `			/* Append raw contents*/` |
|   994773 |   842 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   497384 |   843 | `		}` |
|  1025871 |   844 | `		zIn++;` |
|  1025871 |   845 | `		if( zIn < zEnd ){` |
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
|  1025871 |   860 | `		zIn++;` |
|        5 |   861 | `	}` |
|        - |   862 | `	/* Emit the load constant instruction */` |
|  1025753 |   863 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|  1025753 |   864 | `	if( pStr->nByte < 1024 ){` |
|        - |   865 | `		/* Install in the literal table */` |
|  1025753 |   866 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   512874 |   867 | `	}` |
|        - |   868 | `	/* Node successfully compiled */` |
|  1025753 |   869 | `	return SXRET_OK;` |
|  1497706 |   870 | `}` |
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
|    38612 |  1070 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|        5 |  1071 | `{` |
|        - |  1072 | `	ph7_value *pConstObj;` |
|    38617 |  1073 | `	sxu32 nIdx = 0;` |
|        - |  1074 | `	/* Reserve a new constant */` |
|    38617 |  1075 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    38617 |  1076 | `	if( pConstObj == 0 ){` |
|      ! 0 |  1077 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  1078 | `		return 0;` |
|        - |  1079 | `	}` |
|    38617 |  1080 | `	(*pCount)++;` |
|    38617 |  1081 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|        - |  1082 | `	/* Emit the load constant instruction */` |
|    38617 |  1083 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    38617 |  1084 | `	return pConstObj;` |
|    19311 |  1085 | `}` |
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
|    37098 |  1148 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|        5 |  1149 | `{` |
|    37103 |  1150 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|        - |  1151 | `	const char *zIn,*zCur,*zEnd;` |
|    37103 |  1152 | `	ph7_value *pObj = 0;` |
|        - |  1153 | `	sxi32 iCons;` |
|        - |  1154 | `	sxi32 rc;` |
|        - |  1155 | `	/* Delimit the string */` |
|    37103 |  1156 | `	zIn  = pStr->zString;` |
|    37103 |  1157 | `	zEnd = &zIn[pStr->nByte];` |
|    37103 |  1158 | `	if( zIn >= zEnd ){` |
|        - |  1159 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|        - |  1160 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|        - |  1161 | `		 * literal table from growing when many "" literals appear in the source.` |
|        - |  1162 | `		 */` |
|      377 |  1163 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|      377 |  1164 | `		return SXRET_OK;` |
|        - |  1165 | `	}` |
|    36731 |  1166 | `	zCur = 0;` |
|        - |  1167 | `	/* Compile the node */` |
|    36731 |  1168 | `	iCons = 0;` |
|    19597 |  1169 | `	for(;;){` |
|    63107 |  1170 | `		zCur = zIn;` |
|   215563 |  1171 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|   154929 |  1172 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|       72 |  1173 | `				break;` |
|   154795 |  1174 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|     2338 |  1175 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     1170 |  1176 | `					break;` |
|        - |  1177 | `			}` |
|   152461 |  1178 | `			zIn++;` |
|        5 |  1179 | `		}` |
|    63107 |  1180 | `		if( zIn > zCur ){` |
|    20495 |  1181 | `			if( pObj == 0 ){` |
|    19965 |  1182 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    19965 |  1183 | `				if( pObj == 0 ){` |
|      ! 0 |  1184 | `					return SXERR_ABORT;` |
|        - |  1185 | `				}` |
|     9980 |  1186 | `			}` |
|    20495 |  1187 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    10245 |  1188 | `		}` |
|    63107 |  1189 | `		if( zIn >= zEnd ){` |
|    36729 |  1190 | `			break;` |
|        - |  1191 | `		}` |
|    26383 |  1192 | `		if( zIn[0] == '\\' ){` |
|    23915 |  1193 | `			const char *zPtr = 0;` |
|        - |  1194 | `			sxu32 n;` |
|    23915 |  1195 | `			zIn++;` |
|    23915 |  1196 | `			if( pObj == 0 ){` |
|    18657 |  1197 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    18657 |  1198 | `				if( pObj == 0 ){` |
|      ! 0 |  1199 | `					return SXERR_ABORT;` |
|        - |  1200 | `				}` |
|     9326 |  1201 | `			}` |
|    23915 |  1202 | `			if( zIn >= zEnd ){` |
|        - |  1203 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|        3 |  1204 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|        3 |  1205 | `				break;` |
|        - |  1206 | `			}` |
|    23913 |  1207 | `			n = sizeof(char); /* size of conversion */` |
|    23913 |  1208 | `			switch( zIn[0] ){` |
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
|    11402 |  1225 | `			case 'n':` |
|        - |  1226 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    22809 |  1227 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    22809 |  1228 | `				break;` |
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
|    23913 |  1351 | `			zIn += n;` |
|    23913 |  1352 | `			continue;` |
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
|    36731 |  1470 | `	if( iCons > 1 ){` |
|        - |  1471 | `		/* Concatenate all compiled constants */` |
|     1807 |  1472 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|      901 |  1473 | `	}` |
|        - |  1474 | `	/* Node successfully compiled */` |
|    36731 |  1475 | `	return SXRET_OK;` |
|    18554 |  1476 | `}` |
|        - |  1477 | `/*` |
|        - |  1478 | ` * Compile a double quoted string.` |
|        - |  1479 | ` *  See the block-comment above for more information.` |
|        - |  1480 | ` */` |
|    37036 |  1481 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  1482 | `{` |
|        - |  1483 | `	sxi32 rc;` |
|    37041 |  1484 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|    18518 |  1485 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  1486 | `	/* Compilation result */` |
|    37041 |  1487 | `	return rc;` |
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
|   290988 |  1677 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|        5 |  1678 | `{` |
|        - |  1679 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|        - |  1680 | `	SyToken *pKey,*pCur;` |
|   290993 |  1681 | `	sxi32 iEmitRef = 0;` |
|   290993 |  1682 | `	sxi32 iSpread = 0;` |
|   290993 |  1683 | `	sxi32 nPair = 0;` |
|        - |  1684 | `	sxi32 rc;` |
|   290993 |  1685 | `	xValidator = 0;` |
|   341405 |  1686 | `	for(;;){` |
|        - |  1687 | `		/* Jump leading commas */` |
|   974527 |  1688 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|   291717 |  1689 | `			pGen->pIn++;` |
|        5 |  1690 | `		}` |
|   682815 |  1691 | `		pCur = pGen->pIn;` |
|   682815 |  1692 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|        - |  1693 | `			/* No more entry to process */` |
|   290977 |  1694 | `			break;` |
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
|   290977 |  1781 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|        - |  1782 | `	/* Node successfully compiled */` |
|   290977 |  1783 | `	return SXRET_OK;` |
|   145499 |  1784 | `}` |
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
|     1724 |  1899 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  1900 | `{` |
|        - |  1901 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     1729 |  1902 | `	pGen->pIn++;` |
|     1729 |  1903 | `	pGen->pEnd--;` |
|      862 |  1904 | `	SXUNUSED(iCompileFlag);` |
|     1729 |  1905 | `	return GenStateCompileArrayBody(pGen);` |
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
|      448 |  2246 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  2247 | `{` |
|      453 |  2248 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|        - |  2249 | `	char zName[512];         /* Unique lambda name */` |
|        - |  2250 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|        - |  2251 | `							  * one thread is allowed to compile the script.` |
|        - |  2252 | `						      */` |
|        - |  2253 | `	SyString sName;` |
|      453 |  2254 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `$f = #[A] function…` trivia`` |
|        - |  2255 | `	                              * is keyed to this ['static'] 'function' token */` |
|        - |  2256 | `	sxu32 nKwLine;` |
|      453 |  2257 | `	sxi32 iFlags = 0;` |
|        - |  2258 | `	sxu32 nLen;` |
|        - |  2259 | `	sxi32 rc;` |
|      224 |  2260 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  2261 |  |
|      453 |  2262 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|      448 |  2263 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      453 |  2264 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - |  2265 | `		/* Static closure: no $this auto-capture, bind refused */` |
|        9 |  2266 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|        9 |  2267 | `		pGen->pIn++; /* Jump the 'static' keyword */` |
|        4 |  2268 | `	}` |
|      453 |  2269 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|      453 |  2270 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      ! 0 |  2271 | `		pGen->pIn++;` |
|      ! 0 |  2272 | `	}` |
|        - |  2273 | `	/* Generate a unique name */` |
|      453 |  2274 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|        - |  2275 | `	/* Make sure the generated name is unique */` |
|      453 |  2276 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 |  2277 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      ! 0 |  2278 | `	}` |
|      453 |  2279 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|        - |  2280 | `	/* Compile the lambda body */` |
|      453 |  2281 | `	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,&pAnnonFunc);` |
|      453 |  2282 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2283 | `		return SXERR_ABORT;` |
|        - |  2284 | `	}` |
|      453 |  2285 | `	if( pAnnonFunc ){` |
|      453 |  2286 | `		pAnnonFunc->nLine = nKwLine;` |
|        - |  2287 | ``		/* Expression-position attributes (`$f = #[A] function () {}`): the trivia`` |
|        - |  2288 | `		 * sidecar keys them to the closure's first keyword token. */` |
|      453 |  2289 | `		if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnnonFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  2290 | `			return SXERR_ABORT;` |
|        - |  2291 | `		}` |
|      224 |  2292 | `	}` |
|        - |  2293 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|        - |  2294 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|        - |  2295 | `	 * the handler wraps either in a Closure instance. */` |
|      453 |  2296 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|        - |  2297 | `	/* Node successfully compiled */` |
|      453 |  2298 | `	return SXRET_OK;` |
|      229 |  2299 | `}` |
|        - |  2300 | `/*` |
|        - |  2301 | ` * Add a free variable to the arrow function's closure environment, unless` |
|        - |  2302 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|        - |  2303 | ` * enclosing arrow level, or has already been captured.` |
|        - |  2304 | ` */` |
|      196 |  2305 | `static sxi32 GenStateArrowAddCapture(` |
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
|      199 |  2317 | `	if( nByte == 0 ){` |
|      ! 0 |  2318 | `		return SXRET_OK;` |
|        - |  2319 | `	}` |
|      196 |  2320 | `	if( nByte == sizeof("this")-1` |
|      107 |  2321 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|        3 |  2322 | `		return SXRET_OK;` |
|        - |  2323 | `	}` |
|      247 |  2324 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|      182 |  2325 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|      176 |  2326 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|      135 |  2327 | `			return SXRET_OK;` |
|        - |  2328 | `		}` |
|       27 |  2329 | `	}` |
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
|      101 |  2349 | `}` |
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
|      296 |  2418 | `static sxi32 GenStateArrowCaptureScan(` |
|        - |  2419 | `	ph7_gen_state *pGen,` |
|        - |  2420 | `	ph7_vm_func *pFunc,` |
|        - |  2421 | `	SyToken *pStart,` |
|        - |  2422 | `	SyToken *pEnd,` |
|        - |  2423 | `	SyString *aShadow,` |
|        - |  2424 | `	sxu32 nShadow)` |
|        4 |  2425 | `{` |
|      300 |  2426 | `	SyToken *pScan = pStart;` |
|        - |  2427 | `	sxi32 rc;` |
|     1708 |  2428 | `	while( pScan < pEnd ){` |
|     1412 |  2429 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
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
|     1356 |  2440 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
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
|     1338 |  2592 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     1166 |  2593 | `			pScan++;` |
|     1166 |  2594 | `			continue;` |
|        - |  2595 | `		}` |
|        - |  2596 | `		{` |
|        - |  2597 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|      175 |  2598 | `			SyToken *pDollar = pScan;` |
|      258 |  2599 | `			while( &pDollar[1] < pEnd` |
|      175 |  2600 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|      ! 0 |  2601 | `				pDollar++;` |
|      ! 0 |  2602 | `			}` |
|      175 |  2603 | `			if( &pDollar[1] >= pEnd ){` |
|      ! 0 |  2604 | `				break;` |
|        - |  2605 | `			}` |
|      175 |  2606 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  2607 | `				pScan = pDollar + 1;` |
|      ! 0 |  2608 | `				continue;` |
|        - |  2609 | `			}` |
|      261 |  2610 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|      172 |  2611 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|       86 |  2612 | `				aShadow,nShadow);` |
|      175 |  2613 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2614 | `				return SXERR_ABORT;` |
|        - |  2615 | `			}` |
|      175 |  2616 | `			pScan = pDollar + 2;` |
|        - |  2617 | `		}` |
|        3 |  2618 | `	}` |
|      300 |  2619 | `	return SXRET_OK;` |
|      152 |  2620 | `}` |
|        - |  2621 | `/*` |
|        - |  2622 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|        - |  2623 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|        - |  2624 | ` * variables by value. The body is a single expression that acts as an` |
|        - |  2625 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|        - |  2626 | ` * $this is also made available.` |
|        - |  2627 | ` */` |
|      278 |  2628 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
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
|      283 |  2645 | `	sxi32 iFlags = 0;` |
|      283 |  2646 | `	int bStatic = 0;` |
|        - |  2647 | `	sxi32 rc;` |
|        - |  2648 | `	sxu32 n;` |
|      139 |  2649 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  2650 |  |
|      283 |  2651 | `	nLine = pGen->pIn->nLine;` |
|        - |  2652 | ``	/* Attribute-sidecar key: `#[A] [static] fn` trivia is keyed to this token */`` |
|      283 |  2653 | `	pTokKw = pGen->pIn;` |
|        - |  2654 | `	/* Optional 'static' prefix */` |
|      278 |  2655 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      283 |  2656 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        7 |  2657 | `		bStatic = 1;` |
|        7 |  2658 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|        7 |  2659 | `		pGen->pIn++;` |
|        3 |  2660 | `	}` |
|        - |  2661 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|      278 |  2662 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      283 |  2663 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|      ! 0 |  2664 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2665 | `			"Arrow function: expected 'fn' keyword");` |
|      ! 0 |  2666 | `		return SXERR_SYNTAX;` |
|        - |  2667 | `	}` |
|      283 |  2668 | `	pGen->pIn++; /* Jump 'fn' */` |
|        - |  2669 | `	/* Optional '&' — return by reference */` |
|      283 |  2670 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  2671 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|      ! 0 |  2672 | `		pGen->pIn++;` |
|      ! 0 |  2673 | `	}` |
|        - |  2674 | `	/* Expect '(' */` |
|      283 |  2675 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
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
|      281 |  2686 | `	pGen->pIn++; /* Jump '(' */` |
|        - |  2687 | `	/* Delimit the parameter list */` |
|      281 |  2688 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|      281 |  2689 | `	if( pSigEnd >= pGen->pEnd ){` |
|        3 |  2690 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  2691 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|        3 |  2692 | `		return SXERR_SYNTAX;` |
|        - |  2693 | `	}` |
|        - |  2694 | `	/* Allocate the function state */` |
|      279 |  2695 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|      279 |  2696 | `	if( pFunc == 0 ){` |
|      ! 0 |  2697 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2698 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2699 | `		return SXERR_ABORT;` |
|        - |  2700 | `	}` |
|        - |  2701 | `	/* Generate a unique lambda name */` |
|      279 |  2702 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      279 |  2703 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 |  2704 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      ! 0 |  2705 | `	}` |
|      279 |  2706 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|      279 |  2707 | `	if( zDup == 0 ){` |
|      ! 0 |  2708 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2709 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2710 | `		return SXERR_ABORT;` |
|        - |  2711 | `	}` |
|      279 |  2712 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|        - |  2713 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|      279 |  2714 | `	pFunc->nLine = nLine;` |
|        - |  2715 | ``	/* Expression-position attributes (`$f = #[A] fn () => …`) */`` |
|      279 |  2716 | `	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  2717 | `		return SXERR_ABORT;` |
|        - |  2718 | `	}` |
|        - |  2719 | `	/* Collect function arguments */` |
|      279 |  2720 | `	if( pGen->pIn < pSigEnd ){` |
|      110 |  2721 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|      110 |  2722 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  2723 | `			return SXERR_ABORT;` |
|        - |  2724 | `		}` |
|       53 |  2725 | `	}` |
|        - |  2726 | `	/* Point past ')' and parse optional return type */` |
|      279 |  2727 | `	pGen->pIn = &pSigEnd[1];` |
|      279 |  2728 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|      279 |  2729 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2730 | `		return SXERR_ABORT;` |
|      279 |  2731 | `	}else if( rc == SXERR_SYNTAX ){` |
|      ! 0 |  2732 | `		return SXERR_SYNTAX;` |
|        - |  2733 | `	}` |
|        - |  2734 | `	/* Expect '=>' */` |
|      279 |  2735 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
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
|      276 |  2746 | `	pGen->pIn++; /* Jump '=>' */` |
|      276 |  2747 | `	pBodyStart = pGen->pIn;` |
|      276 |  2748 | `	pBodyEnd = pGen->pEnd;` |
|        - |  2749 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|        - |  2750 | `	 * recursively collect free-variable references from the body. The scan` |
|        - |  2751 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|        - |  2752 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|      276 |  2753 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|        - |  2754 | `	{` |
|      276 |  2755 | `		SyString *aShadow = 0;` |
|      276 |  2756 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|      276 |  2757 | `		if( nShadow > 0 ){` |
|      107 |  2758 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      104 |  2759 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|      107 |  2760 | `			if( aShadow == 0 ){` |
|      ! 0 |  2761 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2762 | `					"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2763 | `				return SXERR_ABORT;` |
|        - |  2764 | `			}` |
|      239 |  2765 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|      135 |  2766 | `				aShadow[n] = aArgs[n].sName;` |
|       69 |  2767 | `			}` |
|       52 |  2768 | `		}` |
|      412 |  2769 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      136 |  2770 | `			aShadow,nShadow);` |
|      276 |  2771 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  2772 | `			return SXERR_ABORT;` |
|        - |  2773 | `		}` |
|        - |  2774 | `	}` |
|        - |  2775 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|        - |  2776 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|        - |  2777 | `	 * captured value is silently dropped when the enclosing scope has no` |
|        - |  2778 | `	 * $this. */` |
|      276 |  2779 | `	if( !bStatic ){` |
|        - |  2780 | `		char *zThisDup;` |
|      270 |  2781 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|      270 |  2782 | `		if( zThisDup == 0 ){` |
|      ! 0 |  2783 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2784 | `				"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2785 | `			return SXERR_ABORT;` |
|        - |  2786 | `		}` |
|      270 |  2787 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      270 |  2788 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|      270 |  2789 | `		sEnv.nIdx = SXU32_HIGH;` |
|      270 |  2790 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      270 |  2791 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|      270 |  2792 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      133 |  2793 | `	}` |
|        - |  2794 | `	/* Arrow functions are always closures */` |
|      276 |  2795 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|        - |  2796 | `	/* Compile the body expression as an implicit return */` |
|      412 |  2797 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      136 |  2798 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|      276 |  2799 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  2800 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2801 | `			"PH7 engine is running out-of-memory");` |
|      ! 0 |  2802 | `		return SXERR_ABORT;` |
|        - |  2803 | `	}` |
|      276 |  2804 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      276 |  2805 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|      276 |  2806 | `	pSavedEnd = pGen->pEnd;` |
|      276 |  2807 | `	pGen->pIn = pBodyStart;` |
|      276 |  2808 | `	pGen->pEnd = pBodyEnd;` |
|      276 |  2809 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      276 |  2810 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2811 | `		return SXERR_ABORT;` |
|        - |  2812 | `	}` |
|        - |  2813 | `	/* The cursor stopped just past the body expression */` |
|      276 |  2814 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|        - |  2815 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|        - |  2816 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|        - |  2817 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|        - |  2818 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|      276 |  2819 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      276 |  2820 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      276 |  2821 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      276 |  2822 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      276 |  2823 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - |  2824 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|      276 |  2825 | `	pGen->pIn = pBodyEnd;` |
|      276 |  2826 | `	pGen->pEnd = pSavedEnd;` |
|        - |  2827 | `	/* Emit the load-closure instruction */` |
|      276 |  2828 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|      276 |  2829 | `	return SXRET_OK;` |
|      144 |  2830 | `}` |
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
|  8846356 |  3186 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  3187 | `{` |
|  8846361 |  3188 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |  3189 | `	sxi32 iVv;` |
|        - |  3190 | `	sxi32 iP1;` |
|        - |  3191 | `	void *p3;` |
|        - |  3192 | `	sxi32 rc;` |
|  8846361 |  3193 | `	iVv = -1; /* Variable variable counter */` |
| 17692729 |  3194 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  8846373 |  3195 | `		pGen->pIn++;` |
|  8846373 |  3196 | `		iVv++;` |
|        5 |  3197 | `	}` |
|  8846361 |  3198 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|        - |  3199 | `		/* Invalid variable name */` |
|      ! 0 |  3200 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|      ! 0 |  3201 | `		if( rc == SXERR_ABORT ){` |
|        - |  3202 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3203 | `			return SXERR_ABORT;` |
|        - |  3204 | `		}` |
|      ! 0 |  3205 | `		return SXRET_OK;` |
|        - |  3206 | `	}` |
|  8846361 |  3207 | `	p3  = 0;` |
|  8846361 |  3208 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
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
|  8846343 |  3228 | `		char *zName = 0;` |
|        - |  3229 | `		/* Extract variable name */` |
|  8846343 |  3230 | `		pName = &pGen->pIn->sData;` |
|        - |  3231 | `		/* Advance the stream cursor */` |
|  8846343 |  3232 | `		pGen->pIn++;` |
|  8846343 |  3233 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  8846343 |  3234 | `		if( pEntry == 0 ){` |
|        - |  3235 | `			/* Duplicate name */` |
|   562883 |  3236 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   562883 |  3237 | `			if( zName == 0 ){` |
|      ! 0 |  3238 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3239 | `				return SXERR_ABORT;` |
|        - |  3240 | `			}` |
|        - |  3241 | `			/* Install in the hashtable */` |
|   562883 |  3242 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   281444 |  3243 | `		}else{` |
|        - |  3244 | `			/* Name already available */` |
|  8283465 |  3245 | `			zName = (char *)pEntry->pUserData;` |
|        - |  3246 | `		}` |
|  8846343 |  3247 | `		p3 = (void *)zName;` |
|        - |  3248 | `	}` |
|  8846357 |  3249 | `	iP1 = 0;` |
|  8846357 |  3250 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  2675467 |  3251 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|        - |  3252 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  2675449 |  3253 | `			iP1 = 1;` |
|  1337722 |  3254 | `		}` |
|  1337731 |  3255 | `	}` |
|        - |  3256 | `	/* Emit the load instruction */` |
|  8846357 |  3257 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  8846369 |  3258 | `	while( iVv > 0 ){` |
|       13 |  3259 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|       13 |  3260 | `		iVv--;` |
|        1 |  3261 | `	}` |
|        - |  3262 | `	/* Node successfully compiled */` |
|  8846357 |  3263 | `	return SXRET_OK;` |
|  4423183 |  3264 | `}` |
|        - |  3265 | `/*` |
|        - |  3266 | ` * Load a literal.` |
|        - |  3267 | ` */` |
|  5620432 |  3268 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|        5 |  3269 | `{` |
|  5620437 |  3270 | `	SyToken *pToken = pGen->pIn;` |
|        - |  3271 | `	ph7_value *pObj;` |
|        - |  3272 | `	SyString *pStr;` |
|        - |  3273 | `	sxu32 nIdx;` |
|        - |  3274 | `	/* Extract token value */` |
|  5620437 |  3275 | `	pStr = &pToken->sData;` |
|        - |  3276 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  5620437 |  3277 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  1363029 |  3278 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|        - |  3279 | `			/* NULL constant are always indexed at 0 */` |
|   560195 |  3280 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   560195 |  3281 | `			return SXRET_OK;` |
|   802839 |  3282 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|        - |  3283 | `			/* TRUE constant are always indexed at 1 */` |
|   148577 |  3284 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|   148577 |  3285 | `			return SXRET_OK;` |
|        5 |  3286 | `		}` |
|  5066107 |  3287 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   963126 |  3288 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|        - |  3289 | `			/* FALSE constant are always indexed at 2 */` |
|   408463 |  3290 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   408463 |  3291 | `			return SXRET_OK;` |
|  4135231 |  3292 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   572552 |  3293 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|        - |  3294 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    11663 |  3295 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    11663 |  3296 | `			if( pObj == 0 ){` |
|      ! 0 |  3297 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3298 | `				return SXERR_ABORT;` |
|        - |  3299 | `			}` |
|    11663 |  3300 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|        - |  3301 | `			/* Emit the load constant instruction */` |
|    11663 |  3302 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    11663 |  3303 | `			return SXRET_OK;` |
|  3866725 |  3304 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|    58856 |  3305 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
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
|  3859176 |  3321 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   152010 |  3322 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  3945504 |  3323 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   216450 |  3324 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
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
|  4491543 |  3354 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|        - |  3355 | `		ph7_value *pLitObj;` |
|        - |  3356 | `		/* Unknown literal,install it in the literal table */` |
|   908263 |  3357 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   908263 |  3358 | `		if( pLitObj == 0 ){` |
|      ! 0 |  3359 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  3360 | `			return SXERR_ABORT;` |
|        - |  3361 | `		}` |
|   908263 |  3362 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|   908263 |  3363 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|   454129 |  3364 | `	}` |
|        - |  3365 | `	/* Emit the load constant instruction */` |
|  4491543 |  3366 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  4491543 |  3367 | `	return SXRET_OK;` |
|  2810221 |  3368 | `}` |
|        - |  3369 | `/*` |
|        - |  3370 | ` * Resolve a namespace path or simply load a literal.` |
|        - |  3371 | ` * If the token stream contains namespace separators (backslashes),` |
|        - |  3372 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|        - |  3373 | ` * Otherwise, load the simple literal directly.` |
|        - |  3374 | ` */` |
|  5624364 |  3375 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|        5 |  3376 | `{` |
|        - |  3377 | `	sxi32 rc;` |
|  5624369 |  3378 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  3379 | `		return SXRET_OK;` |
|        - |  3380 | `	}` |
|        - |  3381 | `	/* Check if this is a multi-token namespace path */` |
|  5624369 |  3382 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|        - |  3383 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|     3937 |  3384 | `		SyBlob *pWorker = &pGen->sWorker;` |
|     3937 |  3385 | `		int isAbsolute = 0;` |
|     3937 |  3386 | `		SyBlobReset(pWorker);` |
|        - |  3387 | `		/* Check for leading backslash (absolute path) */` |
|     3937 |  3388 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|     3935 |  3389 | `			isAbsolute = 1;` |
|     3935 |  3390 | `			pGen->pIn++; /* Skip leading backslash */` |
|     1965 |  3391 | `		}` |
|        - |  3392 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|     3937 |  3393 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        3 |  3394 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        3 |  3395 | `			SyBlobAppend(pWorker,"\\",1);` |
|        1 |  3396 | `		}` |
|        - |  3397 | `		/* Collect all path components */` |
|     4045 |  3398 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     4045 |  3399 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       58 |  3400 | `				SyBlobAppend(pWorker,"\\",1);` |
|       31 |  3401 | `			}else{` |
|     3991 |  3402 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - |  3403 | `			}` |
|     4045 |  3404 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|     3937 |  3405 | `				pGen->pIn++;` |
|     3937 |  3406 | `				break;` |
|        - |  3407 | `			}` |
|      112 |  3408 | `			pGen->pIn++;` |
|        4 |  3409 | `		}` |
|     3937 |  3410 | `		if( SyBlobLength(pWorker) > 0 ){` |
|        - |  3411 | `			ph7_value *pObj;` |
|        - |  3412 | `			SyString sPath;` |
|        - |  3413 | `			sxu32 nIdx;` |
|     3937 |  3414 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|        - |  3415 | `			/* Install in the literal table */` |
|     3937 |  3416 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|     3909 |  3417 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     3909 |  3418 | `				if( pObj == 0 ){` |
|      ! 0 |  3419 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  3420 | `					return SXERR_ABORT;` |
|        - |  3421 | `				}` |
|     3909 |  3422 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|     3909 |  3423 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     1952 |  3424 | `			}` |
|        - |  3425 | `			/* Emit the load constant instruction.` |
|        - |  3426 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|        - |  3427 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|     5903 |  3428 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|     1966 |  3429 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|     1966 |  3430 | `				nIdx,0,0);` |
|     3937 |  3431 | `			return SXRET_OK;` |
|        - |  3432 | `		}` |
|      ! 0 |  3433 | `	}` |
|        - |  3434 | `	/* Single-token literal: load directly */` |
|  5620437 |  3435 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  5620437 |  3436 | `	return rc;` |
|  2812187 |  3437 | `}` |
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
|  5624364 |  3454 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  3455 | `{` |
|        - |  3456 | `	sxi32 rc;` |
|  5624369 |  3457 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  5624369 |  3458 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  3459 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  3460 | `		return rc;` |
|        - |  3461 | `	}` |
|        - |  3462 | `	/* Node successfully compiled */` |
|  5624369 |  3463 | `	return SXRET_OK;` |
|  2812187 |  3464 | `}` |
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
|   143928 |  3481 | `static int GenStateIsReservedConstant(SyString *pName)` |
|        5 |  3482 | `{` |
|   143933 |  3483 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|       48 |  3484 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|        3 |  3485 | `			return TRUE;` |
|       46 |  3486 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|        6 |  3487 | `			return TRUE;` |
|        3 |  3488 | `		}` |
|   143908 |  3489 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       22 |  3490 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|        3 |  3491 | `			return TRUE;` |
|        - |  3492 | `		}` |
|        9 |  3493 | `	}` |
|        - |  3494 | `	/* Not a reserved constant */` |
|   143925 |  3495 | `	return FALSE;` |
|    71969 |  3496 | `}` |
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
|       47 |  3538 | `	pName = &pGen->pIn->sData;` |
|        - |  3539 | `	/* Make sure the constant name isn't reserved */` |
|       47 |  3540 | `	if( GenStateIsReservedConstant(pName) ){` |
|        - |  3541 | `		/* Reserved constant */` |
|       10 |  3542 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       10 |  3543 | `		if( rc == SXERR_ABORT ){` |
|        - |  3544 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3545 | `			return SXERR_ABORT;` |
|        - |  3546 | `		}` |
|       10 |  3547 | `		goto Synchronize;` |
|        - |  3548 | `	}` |
|       38 |  3549 | `	pGen->pIn++;` |
|       38 |  3550 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
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
|       42 |  3612 | `		pGen->pIn++;` |
|        4 |  3613 | `	}` |
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
|    58412 |  3638 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|        5 |  3639 | `{` |
|    58417 |  3640 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    58417 |  3641 | `	int nInlineTry = 0;` |
|   272279 |  3642 | `	while( pBlock && pBlock != pTarget ){` |
|   213867 |  3643 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
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
|   213867 |  3660 | `		pBlock = pBlock->pParent;` |
|        5 |  3661 | `	}` |
|    58417 |  3662 | `	return nInlineTry;` |
|        5 |  3663 | `}` |
|    27238 |  3664 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|        5 |  3665 | `{` |
|        - |  3666 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  3667 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  3668 | `	sxu32 nLineLocal;` |
|        - |  3669 | `	sxi32 rc;` |
|    27243 |  3670 | `	nLineLocal = pGen->pIn->nLine;` |
|    27243 |  3671 | `	iLevel = 0;` |
|        - |  3672 | `	/* Jump the 'continue' keyword */` |
|    27243 |  3673 | `	pGen->pIn++;` |
|    27243 |  3674 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
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
|    27243 |  3700 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    27243 |  3701 | `	if( pLoop == 0 ){` |
|        - |  3702 | `		/* Illegal continue */` |
|       12 |  3703 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|       12 |  3704 | `		if( rc == SXERR_ABORT ){` |
|        - |  3705 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3706 | `			return SXERR_ABORT;` |
|        - |  3707 | `		}` |
|        7 |  3708 | `	}else{` |
|    27233 |  3709 | `		sxu32 nInstrIdx = 0;` |
|        - |  3710 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    27233 |  3711 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  3712 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|        - |  3713 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|    27233 |  3714 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|    27233 |  3715 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
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
|    27229 |  3727 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    27229 |  3728 | `			if( pLoop->bPostContinue == TRUE ){` |
|        - |  3729 | `				JumpFixup sJumpFix;` |
|        - |  3730 | `				/* Post-continue */` |
|       14 |  3731 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|       14 |  3732 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|       14 |  3733 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|        6 |  3734 | `			}` |
|        - |  3735 | `		}` |
|        - |  3736 | `	}` |
|    27243 |  3737 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  3738 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  3739 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|      ! 0 |  3740 | `	}` |
|        - |  3741 | `	/* Statement successfully compiled */` |
|    27243 |  3742 | `	return SXRET_OK;` |
|    13624 |  3743 | `}` |
|        - |  3744 | `/*` |
|        - |  3745 | ` * Compile the 'break' statement.` |
|        - |  3746 | ` * According to the PHP language reference` |
|        - |  3747 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|        - |  3748 | ` *  structure.` |
|        - |  3749 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|        - |  3750 | ` *  enclosing structures are to be broken out of.` |
|        - |  3751 | ` */` |
|    31200 |  3752 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|        5 |  3753 | `{` |
|        - |  3754 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  3755 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  3756 | `	sxi32 rc;` |
|    31205 |  3757 | `	iLevel = 0;` |
|        - |  3758 | `	/* Jump the 'break' keyword */` |
|    31205 |  3759 | `	pGen->pIn++;` |
|    31205 |  3760 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
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
|    31205 |  3786 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    31205 |  3787 | `	if( pLoop == 0 ){` |
|        - |  3788 | `		/* Illegal break */` |
|       19 |  3789 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|       19 |  3790 | `		if( rc == SXERR_ABORT ){` |
|        - |  3791 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3792 | `			return SXERR_ABORT;` |
|        - |  3793 | `		}` |
|       11 |  3794 | `	}else{` |
|        - |  3795 | `		sxu32 nInstrIdx;` |
|        - |  3796 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    31189 |  3797 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  3798 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|    31189 |  3799 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|    31189 |  3800 | `		if( rc == SXRET_OK ){` |
|        - |  3801 | `			/* Fix the jump later when the jump destination is resolved */` |
|    31189 |  3802 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|    15592 |  3803 | `		}` |
|        - |  3804 | `	}` |
|    31205 |  3805 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  3806 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  3807 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|      ! 0 |  3808 | `	}` |
|        - |  3809 | `	/* Statement successfully compiled */` |
|    31205 |  3810 | `	return SXRET_OK;` |
|    15605 |  3811 | `}` |
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
|        6 |  3896 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|        6 |  3897 | `		if( rc == SXERR_ABORT ){` |
|        - |  3898 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3899 | `			return SXERR_ABORT;` |
|        - |  3900 | `		}` |
|        4 |  3901 | `	}else{` |
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
|       37 |  3918 | `				break;` |
|        - |  3919 | `			}` |
|        - |  3920 | `			/* Point to the upper block */` |
|      167 |  3921 | `			pBlock = pBlock->pParent;` |
|        5 |  3922 | `		}` |
|      153 |  3923 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|        9 |  3924 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|        9 |  3925 | `			if( rc == SXERR_ABORT ){` |
|        - |  3926 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  3927 | `				return SXERR_ABORT;` |
|        - |  3928 | `			}` |
|        3 |  3929 | `		}` |
|      153 |  3930 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|       30 |  3931 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       17 |  3932 | `		}else{` |
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
|  3020924 |  4022 | `static sxi32 PH7_CompileBlock(` |
|        - |  4023 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  4024 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|        - |  4025 | `	)` |
|        5 |  4026 | `{` |
|        - |  4027 | `	sxi32 rc;` |
|        - |  4028 | `	sxu32 nLine;` |
|  3020929 |  4029 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  3019471 |  4030 | `		nLine = pGen->pIn->nLine;` |
|  3019471 |  4031 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  3019471 |  4032 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  4033 | `			return SXERR_ABORT;` |
|        - |  4034 | `		}` |
|  3019471 |  4035 | `		pGen->pIn++;` |
|        - |  4036 | `		/* Compile until we hit the closing braces '}' */` |
|  4420618 |  4037 | `		for(;;){` |
|  8841241 |  4038 | `			if( pGen->pIn >= pGen->pEnd ){` |
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
|  8841221 |  4049 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|        - |  4050 | `				/* Closing braces found,break immediately*/` |
|  3019451 |  4051 | `				pGen->pIn++;` |
|  3019451 |  4052 | `				break;` |
|        - |  4053 | `			}` |
|        - |  4054 | `			/* Compile a single statement */` |
|  5821775 |  4055 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  5821775 |  4056 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4057 | `				return SXERR_ABORT;` |
|        - |  4058 | `			}` |
|        5 |  4059 | `		}` |
|  3019471 |  4060 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  1511196 |  4061 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
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
|     1463 |  4105 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     1463 |  4106 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4107 | `			return SXERR_ABORT;` |
|        - |  4108 | `		}` |
|        - |  4109 | `	}` |
|        - |  4110 | `	/* Jump trailing semi-colons ';' */` |
|  3020929 |  4111 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      ! 0 |  4112 | `		pGen->pIn++;` |
|      ! 0 |  4113 | `	}` |
|  3020929 |  4114 | `	return SXRET_OK;` |
|  1510467 |  4115 | `}` |
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
|    15672 |  4135 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|        5 |  4136 | `{` |
|    15677 |  4137 | `	GenBlock *pWhileBlock = 0;` |
|    15677 |  4138 | `	SyToken *pTmp,*pEnd = 0;` |
|        - |  4139 | `	sxu32 nFalseJump;` |
|        - |  4140 | `	sxu32 nLine;` |
|        - |  4141 | `	sxi32 rc;` |
|    15677 |  4142 | `	nLine = pGen->pIn->nLine;` |
|        - |  4143 | `	/* Jump the 'while' keyword */` |
|    15677 |  4144 | `	pGen->pIn++;` |
|    15677 |  4145 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4146 | `		/* Syntax error */` |
|      ! 0 |  4147 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  4148 | `		if( rc == SXERR_ABORT ){` |
|        - |  4149 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4150 | `			return SXERR_ABORT;` |
|        - |  4151 | `		}` |
|      ! 0 |  4152 | `		goto Synchronize;` |
|        - |  4153 | `	}` |
|        - |  4154 | `	/* Jump the left parenthesis '(' */` |
|    15677 |  4155 | `	pGen->pIn++;` |
|        - |  4156 | `	/* Create the loop block */` |
|    15677 |  4157 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|    15677 |  4158 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4159 | `		return SXERR_ABORT;` |
|        - |  4160 | `	}` |
|        - |  4161 | `	/* Delimit the condition */` |
|    15677 |  4162 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    15677 |  4163 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  4164 | `		/* Empty expression */` |
|        3 |  4165 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|        3 |  4166 | `		if( rc == SXERR_ABORT ){` |
|        - |  4167 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4168 | `			return SXERR_ABORT;` |
|        - |  4169 | `		}` |
|        1 |  4170 | `	}` |
|        - |  4171 | `	/* Swap token streams */` |
|    15677 |  4172 | `	pTmp = pGen->pEnd;` |
|    15677 |  4173 | `	pGen->pEnd = pEnd;` |
|        - |  4174 | `	/* Compile the expression */` |
|    15677 |  4175 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    15677 |  4176 | `	if( rc == SXERR_ABORT ){` |
|        - |  4177 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4178 | `		return SXERR_ABORT;` |
|        - |  4179 | `	}` |
|        - |  4180 | `	/* Update token stream */` |
|    15677 |  4181 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  4182 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4183 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4184 | `			return SXERR_ABORT;` |
|        - |  4185 | `		}` |
|      ! 0 |  4186 | `		pGen->pIn++;` |
|      ! 0 |  4187 | `	}` |
|        - |  4188 | `	/* Synchronize pointers */` |
|    15677 |  4189 | `	pGen->pIn  = &pEnd[1];` |
|    15677 |  4190 | `	pGen->pEnd = pTmp;` |
|        - |  4191 | `	/* Emit the false jump */` |
|    15677 |  4192 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - |  4193 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    15677 |  4194 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|        - |  4195 | `	/* Compile the loop body */` |
|    15677 |  4196 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|    15677 |  4197 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  4198 | `		return SXERR_ABORT;` |
|        - |  4199 | `	}` |
|        - |  4200 | `	/* Emit the unconditional jump to the start of the loop */` |
|    15677 |  4201 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|        - |  4202 | `	/* Fix all jumps now the destination is resolved */` |
|    15677 |  4203 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4204 | `	/* Release the loop block */` |
|    15677 |  4205 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4206 | `	/* Statement successfully compiled */` |
|    15677 |  4207 | `	return SXRET_OK;` |
|      ! 0 |  4208 | `Synchronize:` |
|        - |  4209 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  4210 | `	 * compiling this erroneous block.` |
|        - |  4211 | `	 */` |
|      ! 0 |  4212 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  4213 | `		pGen->pIn++;` |
|      ! 0 |  4214 | `	}` |
|      ! 0 |  4215 | `	return SXRET_OK;` |
|     7841 |  4216 | `}` |
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
|    38980 |  4364 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|        5 |  4365 | `{` |
|    38985 |  4366 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|    38985 |  4367 | `	GenBlock *pForBlock = 0;` |
|        - |  4368 | `	sxu32 nFalseJump;` |
|        - |  4369 | `	sxu32 nLine;` |
|        - |  4370 | `	sxi32 rc;` |
|    38985 |  4371 | `	nLine = pGen->pIn->nLine;` |
|        - |  4372 | `	/* Jump the 'for' keyword */` |
|    38985 |  4373 | `	pGen->pIn++;` |
|    38985 |  4374 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4375 | `		/* Syntax error */` |
|      ! 0 |  4376 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|      ! 0 |  4377 | `		if( rc == SXERR_ABORT ){` |
|        - |  4378 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4379 | `			return SXERR_ABORT;` |
|        - |  4380 | `		}` |
|      ! 0 |  4381 | `		return SXRET_OK;` |
|        - |  4382 | `	}` |
|        - |  4383 | `	/* Jump the left parenthesis '(' */` |
|    38985 |  4384 | `	pGen->pIn++;` |
|        - |  4385 | `	/* Delimit the init-expr;condition;post-expr */` |
|    38985 |  4386 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    38985 |  4387 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
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
|    38985 |  4402 | `	pTmp = pGen->pEnd;` |
|    38985 |  4403 | `	pGen->pEnd = pEnd;` |
|        - |  4404 | `	/* Compile initialization expressions if available */` |
|    38985 |  4405 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  4406 | `	/* Pop operand lvalues */` |
|    38985 |  4407 | `	if( rc == SXERR_ABORT ){` |
|        - |  4408 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4409 | `		return SXERR_ABORT;` |
|    38985 |  4410 | `	}else if( rc != SXERR_EMPTY ){` |
|    38983 |  4411 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    19489 |  4412 | `	}` |
|    38985 |  4413 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
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
|    38985 |  4424 | `	pGen->pIn++;` |
|        - |  4425 | `	/* Create the loop block */` |
|    38985 |  4426 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|    38985 |  4427 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4428 | `		return SXERR_ABORT;` |
|        - |  4429 | `	}` |
|        - |  4430 | `	/* Deffer continue jumps */` |
|    38985 |  4431 | `	pForBlock->bPostContinue = TRUE;` |
|        - |  4432 | `	/* Compile the condition */` |
|    38985 |  4433 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    38985 |  4434 | `	if( rc == SXERR_ABORT ){` |
|        - |  4435 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4436 | `		return SXERR_ABORT;` |
|    38985 |  4437 | `	}else if( rc != SXERR_EMPTY ){` |
|        - |  4438 | `		/* Emit the false jump */` |
|    38983 |  4439 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - |  4440 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    38983 |  4441 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    19489 |  4442 | `	}` |
|    38985 |  4443 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
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
|    38981 |  4454 | `	pGen->pIn++;` |
|        - |  4455 | `	/* Save the post condition stream */` |
|    38981 |  4456 | `	pPostStart = pGen->pIn;` |
|        - |  4457 | `	/* Compile the loop body */` |
|    38981 |  4458 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|    38981 |  4459 | `	pGen->pEnd = pTmp;` |
|    38981 |  4460 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|    38981 |  4461 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  4462 | `		return SXERR_ABORT;` |
|        - |  4463 | `	}` |
|        - |  4464 | `	/* Fix post-continue jumps */` |
|    38981 |  4465 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|        - |  4466 | `		JumpFixup *aPost;` |
|        - |  4467 | `		VmInstr *pInstr;` |
|        - |  4468 | `		sxu32 nJumpDest;` |
|        - |  4469 | `		sxu32 n;` |
|       14 |  4470 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|       14 |  4471 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       26 |  4472 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|       14 |  4473 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|       14 |  4474 | `			if( pInstr ){` |
|        - |  4475 | `				/* Fix jump */` |
|       14 |  4476 | `				pInstr->iP2 = nJumpDest;` |
|        6 |  4477 | `			}` |
|        8 |  4478 | `		}` |
|        6 |  4479 | `	}` |
|        - |  4480 | `	/* compile the post-expressions if available */` |
|    38981 |  4481 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|      ! 0 |  4482 | `		pPostStart++;` |
|      ! 0 |  4483 | `	}` |
|    38981 |  4484 | `	if( pPostStart < pEnd ){` |
|        - |  4485 | `		SyToken *pTmpIn,*pTmpEnd;` |
|    38981 |  4486 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|    38981 |  4487 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    38981 |  4488 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - |  4489 | `			/* Syntax error */` |
|      ! 0 |  4490 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|      ! 0 |  4491 | `			if( rc == SXERR_ABORT ){` |
|        - |  4492 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4493 | `				return SXERR_ABORT;` |
|        - |  4494 | `			}` |
|      ! 0 |  4495 | `			return SXRET_OK;` |
|        - |  4496 | `		}` |
|    38981 |  4497 | `		RE_SWAP_DELIMITER(pGen);` |
|    38981 |  4498 | `		if( rc == SXERR_ABORT ){` |
|        - |  4499 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4500 | `			return SXERR_ABORT;` |
|    38981 |  4501 | `		}else if( rc != SXERR_EMPTY){` |
|        - |  4502 | `			/* Pop operand lvalue */` |
|    38981 |  4503 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    19488 |  4504 | `		}` |
|    19488 |  4505 | `	}` |
|        - |  4506 | `	/* Emit the unconditional jump to the start of the loop */` |
|    38981 |  4507 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|        - |  4508 | `	/* Fix all jumps now the destination is resolved */` |
|    38981 |  4509 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4510 | `	/* Release the loop block */` |
|    38981 |  4511 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4512 | `	/* Statement successfully compiled */` |
|    38981 |  4513 | `	return SXRET_OK;` |
|    19495 |  4514 | `}` |
|        - |  4515 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|        - |  4516 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|        - |  4517 | ` * are allowed.` |
|        - |  4518 | ` */` |
|   241616 |  4519 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 |  4520 | `{` |
|   241621 |  4521 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|   241621 |  4522 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|        - |  4523 | `		/* Unexpected expression */` |
|      ! 0 |  4524 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - |  4525 | `			"foreach: Expecting a variable name");` |
|      ! 0 |  4526 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 |  4527 | `			rc = SXERR_INVALID;` |
|      ! 0 |  4528 | `		}` |
|      ! 0 |  4529 | `	}` |
|   241621 |  4530 | `	return rc;` |
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
|   175378 |  4558 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|        5 |  4559 | `{` |
|   175383 |  4560 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|   175383 |  4561 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|   175383 |  4562 | `	GenBlock *pForeachBlock = 0;` |
|        - |  4563 | `	ph7_foreach_info *pInfo;` |
|        - |  4564 | `	sxu32 nFalseJump;` |
|        - |  4565 | `	VmInstr *pInstr;` |
|        - |  4566 | `	sxu32 nLine;` |
|        - |  4567 | `	sxi32 rc;` |
|   175383 |  4568 | `	nLine = pGen->pIn->nLine;` |
|        - |  4569 | `	/* Jump the 'foreach' keyword */` |
|   175383 |  4570 | `	pGen->pIn++;` |
|   175383 |  4571 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4572 | `		/* Syntax error */` |
|      ! 0 |  4573 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|      ! 0 |  4574 | `		if( rc == SXERR_ABORT ){` |
|        - |  4575 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4576 | `			return SXERR_ABORT;` |
|        - |  4577 | `		}` |
|      ! 0 |  4578 | `		goto Synchronize;` |
|        - |  4579 | `	}` |
|        - |  4580 | `	/* Jump the left parenthesis '(' */` |
|   175383 |  4581 | `	pGen->pIn++;` |
|        - |  4582 | `	/* Create the loop block */` |
|   175383 |  4583 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|   175383 |  4584 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4585 | `		return SXERR_ABORT;` |
|        - |  4586 | `	}` |
|        - |  4587 | `	/* Delimit the expression */` |
|   175383 |  4588 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   175383 |  4589 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
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
|   175383 |  4604 | `	pCur = pGen->pIn;` |
|  1024999 |  4605 | `	while( pCur < pEnd ){` |
|  1024999 |  4606 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|   179281 |  4607 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|   179281 |  4608 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|        - |  4609 | `				/* Break with the first 'as' found */` |
|   175383 |  4610 | `				break;` |
|        - |  4611 | `			}` |
|     1949 |  4612 | `		}` |
|        - |  4613 | `		/* Advance the stream cursor */` |
|   849621 |  4614 | `		pCur++;` |
|        5 |  4615 | `	}` |
|   175383 |  4616 | `	if( pCur <= pGen->pIn ){` |
|      ! 0 |  4617 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  4618 | `			"foreach: Missing array/object expression");` |
|      ! 0 |  4619 | `		if( rc == SXERR_ABORT ){` |
|        - |  4620 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4621 | `			return SXERR_ABORT;` |
|        - |  4622 | `		}` |
|      ! 0 |  4623 | `		goto Synchronize;` |
|        - |  4624 | `	}` |
|        - |  4625 | `	/* Swap token streams */` |
|   175383 |  4626 | `	pTmp = pGen->pEnd;` |
|   175383 |  4627 | `	pGen->pEnd = pCur;` |
|   175383 |  4628 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   175383 |  4629 | `	if( rc == SXERR_ABORT ){` |
|        - |  4630 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4631 | `		return SXERR_ABORT;` |
|        - |  4632 | `	}` |
|        - |  4633 | `	/* Update token stream */` |
|   175383 |  4634 | `	while(pGen->pIn < pCur ){` |
|      ! 0 |  4635 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4636 | `		if( rc == SXERR_ABORT ){` |
|        - |  4637 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4638 | `			return SXERR_ABORT;` |
|        - |  4639 | `		}` |
|      ! 0 |  4640 | `		pGen->pIn++;` |
|      ! 0 |  4641 | `	}` |
|   175383 |  4642 | `	pCur++; /* Jump the 'as' keyword */` |
|   175383 |  4643 | `	pGen->pIn = pCur;` |
|   175383 |  4644 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 |  4645 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|      ! 0 |  4646 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4647 | `			return SXERR_ABORT;` |
|        - |  4648 | `		}` |
|      ! 0 |  4649 | `	}` |
|        - |  4650 | `	/* Create the foreach context */` |
|   175383 |  4651 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|   175383 |  4652 | `	if( pInfo == 0 ){` |
|      ! 0 |  4653 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 |  4654 | `		return SXERR_ABORT;` |
|        - |  4655 | `	}` |
|        - |  4656 | `	/* Zero the structure */` |
|   175383 |  4657 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|        - |  4658 | `	/* Initialize structure fields */` |
|   175383 |  4659 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|        - |  4660 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|        - |  4661 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|        - |  4662 | `	 * '=>'. */` |
|   175383 |  4663 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|   175383 |  4664 | `	if( pCur < pEnd ){` |
|        - |  4665 | `		/* Compile the expression holding the key name */` |
|    66263 |  4666 | `		if( pGen->pIn >= pCur ){` |
|      ! 0 |  4667 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|      ! 0 |  4668 | `			if( rc == SXERR_ABORT ){` |
|        - |  4669 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4670 | `				return SXERR_ABORT;` |
|        - |  4671 | `			}` |
|      ! 0 |  4672 | `		}else{` |
|    66263 |  4673 | `			pGen->pEnd = pCur;` |
|    66263 |  4674 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    66263 |  4675 | `			if( rc == SXERR_ABORT ){` |
|        - |  4676 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4677 | `				return SXERR_ABORT;` |
|        - |  4678 | `			}` |
|    66263 |  4679 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    66263 |  4680 | `			if( pInstr->p3 ){` |
|        - |  4681 | `				/* Record key name */` |
|    66263 |  4682 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    33129 |  4683 | `			}` |
|    66263 |  4684 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|        - |  4685 | `		}` |
|    66263 |  4686 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    33129 |  4687 | `	}` |
|   175383 |  4688 | `	pGen->pEnd = pEnd;` |
|   175383 |  4689 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 |  4690 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|      ! 0 |  4691 | `		if( rc == SXERR_ABORT ){` |
|        - |  4692 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4693 | `			return SXERR_ABORT;` |
|        - |  4694 | `		}` |
|      ! 0 |  4695 | `		goto Synchronize;` |
|        - |  4696 | `	}` |
|   175383 |  4697 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|       31 |  4698 | `		pGen->pIn++;` |
|        - |  4699 | `		/* Pass by reference  */` |
|       31 |  4700 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       14 |  4701 | `	}` |
|        - |  4702 | `	/* Check if the value target is list() */` |
|   175383 |  4703 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
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
|   175378 |  4744 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
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
|   175363 |  4777 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   175363 |  4778 | `		if( rc == SXERR_ABORT ){` |
|        - |  4779 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4780 | `			return SXERR_ABORT;` |
|        - |  4781 | `		}` |
|   175363 |  4782 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   175363 |  4783 | `		if( pInstr->p3 ){` |
|        - |  4784 | `			/* Record value name */` |
|   175363 |  4785 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    87679 |  4786 | `		}` |
|        - |  4787 | `	}` |
|        - |  4788 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|   175381 |  4789 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|        - |  4790 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   175381 |  4791 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|        - |  4792 | `	/* Record the first instruction to execute */` |
|   175381 |  4793 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|        - |  4794 | `	/* Emit the FOREACH_STEP instruction */` |
|   175381 |  4795 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|        - |  4796 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   175381 |  4797 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|        - |  4798 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|   175381 |  4799 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
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
|   175381 |  4827 | `	pGen->pIn = &pEnd[1];` |
|   175381 |  4828 | `	pGen->pEnd = pTmp;` |
|   175381 |  4829 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|   175381 |  4830 | `	if( rc == SXERR_ABORT ){` |
|        - |  4831 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4832 | `		return SXERR_ABORT;` |
|        - |  4833 | `	}` |
|        - |  4834 | `	/* Emit the unconditional jump to the start of the loop */` |
|   175381 |  4835 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|        - |  4836 | `	/* Fix all jumps now the destination is resolved */` |
|   175381 |  4837 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4838 | `	/* Release the loop block */` |
|   175381 |  4839 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4840 | `	/* Statement successfully compiled */` |
|   175381 |  4841 | `	return SXRET_OK;` |
|        1 |  4842 | `Synchronize:` |
|        - |  4843 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  4844 | `	 * compiling this erroneous block.` |
|        - |  4845 | `	 */` |
|        3 |  4846 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  4847 | `		pGen->pIn++;` |
|      ! 0 |  4848 | `	}` |
|        3 |  4849 | `	return SXRET_OK;` |
|    87694 |  4850 | `}` |
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
|  1183344 |  4883 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|        5 |  4884 | `{` |
|  1183349 |  4885 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  1183349 |  4886 | `	GenBlock *pCondBlock = 0;` |
|        - |  4887 | `	sxu32 nJumpIdx;` |
|        - |  4888 | `	sxu32 nKeyID;` |
|        - |  4889 | `	sxi32 rc;` |
|        - |  4890 | `	/* Jump the 'if' keyword */` |
|  1183349 |  4891 | `	pGen->pIn++;` |
|  1183349 |  4892 | `	pToken = pGen->pIn;` |
|        - |  4893 | `	/* Create the conditional block */` |
|  1183349 |  4894 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  1183349 |  4895 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4896 | `		return SXERR_ABORT;` |
|        - |  4897 | `	}` |
|        - |  4898 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   638339 |  4899 | `	for(;;){` |
|  1276683 |  4900 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
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
|  1276683 |  4913 | `		pToken++;` |
|        - |  4914 | `		/* Delimit the condition */` |
|  1276683 |  4915 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  1276683 |  4916 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|        - |  4917 | `			/* Syntax error */` |
|      ! 0 |  4918 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 |  4919 | `				pToken--;` |
|      ! 0 |  4920 | `			}` |
|      ! 0 |  4921 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|      ! 0 |  4922 | `			if( rc == SXERR_ABORT ){` |
|        - |  4923 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4924 | `				return SXERR_ABORT;` |
|        - |  4925 | `			}` |
|      ! 0 |  4926 | `			goto Synchronize;` |
|        - |  4927 | `		}` |
|        - |  4928 | `		/* Swap token streams */` |
|  1276683 |  4929 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|        - |  4930 | `		/* Compile the condition */` |
|  1276683 |  4931 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  4932 | `		/* Update token stream */` |
|  1276683 |  4933 | `		while(pGen->pIn < pEnd ){` |
|      ! 0 |  4934 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4935 | `			pGen->pIn++;` |
|      ! 0 |  4936 | `		}` |
|  1276683 |  4937 | `		pGen->pIn  = &pEnd[1];` |
|  1276683 |  4938 | `		pGen->pEnd = pTmp;` |
|  1276683 |  4939 | `		if( rc == SXERR_ABORT ){` |
|        - |  4940 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4941 | `			return SXERR_ABORT;` |
|        - |  4942 | `		}` |
|        - |  4943 | `		/* Emit the false jump */` |
|  1276683 |  4944 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|        - |  4945 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  1276683 |  4946 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|        - |  4947 | `		/* Compile the body */` |
|  1276683 |  4948 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  1276683 |  4949 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4950 | `			return SXERR_ABORT;` |
|        - |  4951 | `		}` |
|  1276683 |  4952 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   239765 |  4953 | `			break;` |
|        - |  4954 | `		}` |
|        - |  4955 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   797163 |  4956 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   797163 |  4957 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   617909 |  4958 | `			break;` |
|        - |  4959 | `		}` |
|        - |  4960 | `		/* Emit the unconditional jump */` |
|   179259 |  4961 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|        - |  4962 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   179259 |  4963 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   179259 |  4964 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   171373 |  4965 | `			pToken = &pGen->pIn[1];` |
|   171373 |  4966 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    85486 |  4967 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    42965 |  4968 | `					break;` |
|        - |  4969 | `			}` |
|    85453 |  4970 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    42724 |  4971 | `		}` |
|    93339 |  4972 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|        - |  4973 | `		/* Synchronize cursors */` |
|    93339 |  4974 | `		pToken = pGen->pIn;` |
|        - |  4975 | `		/* Fix the false jump */` |
|    93339 |  4976 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|        5 |  4977 | `	} /* For(;;) */` |
|        - |  4978 | `	/* Fix the false jump */` |
|  1183349 |  4979 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  1183349 |  4980 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   703824 |  4981 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|        - |  4982 | `			/* Compile the else block */` |
|    85925 |  4983 | `			pGen->pIn++;` |
|    85925 |  4984 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    85925 |  4985 | `			if( rc == SXERR_ABORT ){` |
|        - |  4986 |  |
|      ! 0 |  4987 | `				return SXERR_ABORT;` |
|        - |  4988 | `			}` |
|    42960 |  4989 | `	}` |
|  1183349 |  4990 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|        - |  4991 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  1183349 |  4992 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|        - |  4993 | `	/* Release the conditional block */` |
|  1183349 |  4994 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4995 | `	/* Statement successfully compiled */` |
|  1183349 |  4996 | `	return SXRET_OK;` |
|      ! 0 |  4997 | `Synchronize:` |
|        - |  4998 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|        - |  4999 | `	 */` |
|      ! 0 |  5000 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  5001 | `		pGen->pIn++;` |
|      ! 0 |  5002 | `	}` |
|      ! 0 |  5003 | `	return SXRET_OK;` |
|   591677 |  5004 | `}` |
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
|       36 |  5026 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|        5 |  5027 | `{` |
|       41 |  5028 | `	SyToken *pTmp,*pNext = 0;` |
|        - |  5029 | `	sxi32 nExpr;` |
|        - |  5030 | `	sxi32 rc;` |
|        - |  5031 | `	/* Jump the 'global' keyword */` |
|       41 |  5032 | `	pGen->pIn++;` |
|       41 |  5033 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        - |  5034 | `		/* Nothing to process */` |
|      ! 0 |  5035 | `		return SXRET_OK;` |
|        - |  5036 | `	}` |
|       41 |  5037 | `	pTmp = pGen->pEnd;` |
|       41 |  5038 | `	nExpr = 0;` |
|       87 |  5039 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       51 |  5040 | `		if( pGen->pIn < pNext ){` |
|       51 |  5041 | `			pGen->pEnd = pNext;` |
|       51 |  5042 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  5043 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|      ! 0 |  5044 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  5045 | `					return SXERR_ABORT;` |
|        - |  5046 | `				}` |
|      ! 0 |  5047 | `			}else{` |
|       51 |  5048 | `				pGen->pIn++;` |
|       51 |  5049 | `				if( pGen->pIn >= pGen->pEnd ){` |
|        - |  5050 | `					/* Emit a warning */` |
|      ! 0 |  5051 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|      ! 0 |  5052 | `				}else{` |
|       51 |  5053 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       51 |  5054 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  5055 | `						return SXERR_ABORT;` |
|       51 |  5056 | `					}else if(rc != SXERR_EMPTY ){` |
|       51 |  5057 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|       51 |  5058 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|        - |  5059 | `							/* Variable name, not a constant */` |
|       51 |  5060 | `							pLast->iP1 = 0;` |
|       23 |  5061 | `						}` |
|       51 |  5062 | `						nExpr++;` |
|       23 |  5063 | `					}` |
|        - |  5064 | `				}` |
|        - |  5065 | `			}` |
|       23 |  5066 | `		}` |
|        - |  5067 | `		/* Next expression in the stream */` |
|       51 |  5068 | `		pGen->pIn = pNext;` |
|        - |  5069 | `		/* Jump trailing commas */` |
|       61 |  5070 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       15 |  5071 | `			pGen->pIn++;` |
|        5 |  5072 | `		}` |
|        5 |  5073 | `	}` |
|        - |  5074 | `	/* Restore token stream */` |
|       41 |  5075 | `	pGen->pEnd = pTmp;` |
|       41 |  5076 | `	if( nExpr > 0 ){` |
|        - |  5077 | `		/* Emit the uplink instruction */` |
|       41 |  5078 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|       18 |  5079 | `	}` |
|       41 |  5080 | `	return SXRET_OK;` |
|       23 |  5081 | `}` |
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
|  1633256 |  5098 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|        5 |  5099 | `{` |
|  1633261 |  5100 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|        - |  5101 | `	sxi32 rc;` |
|  1633261 |  5102 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  1633261 |  5103 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|        - |  5104 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|        - |  5105 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|        - |  5106 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|        - |  5107 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|        - |  5108 | `	 * normally below so token processing stays consistent. */` |
|  4253851 |  5109 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  2620595 |  5110 | `		pFuncBlock = pFuncBlock->pParent;` |
|        5 |  5111 | `	}` |
|  1633256 |  5112 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|  1633229 |  5113 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|        3 |  5114 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  5115 | `			"A never-returning function must not return");` |
|        3 |  5116 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5117 | `			return SXERR_ABORT;` |
|        - |  5118 | `		}` |
|        1 |  5119 | `	}` |
|        - |  5120 | `	/* Jump the 'return' keyword */` |
|  1633261 |  5121 | `	pGen->pIn++;` |
|  1633261 |  5122 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  5123 | `		/* Compile the expression */` |
|  1617695 |  5124 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  1617695 |  5125 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5126 | `			return SXERR_ABORT;` |
|  1617695 |  5127 | `		}else if(rc != SXERR_EMPTY ){` |
|  1617695 |  5128 | `			nRet = 1;` |
|   808845 |  5129 | `		}` |
|   808845 |  5130 | `	}` |
|        - |  5131 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|        - |  5132 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|        - |  5133 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|        - |  5134 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|  1633261 |  5135 | `	if( pGen->bInGenerator ){` |
|       32 |  5136 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|       32 |  5137 | `		return SXRET_OK;` |
|        - |  5138 | `	}` |
|        - |  5139 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|        - |  5140 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|        - |  5141 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|        - |  5142 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|        - |  5143 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  1633233 |  5144 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  1633233 |  5145 | `	return SXRET_OK;` |
|   816633 |  5146 | `}` |
|        - |  5147 | `/*` |
|        - |  5148 | ` * Compile a yield expression.` |
|        - |  5149 | ` * Called from the expression code generator when a yield node is encountered.` |
|        - |  5150 | ` * Handles: yield, yield $value, yield $key => $value` |
|        - |  5151 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|        - |  5152 | ` */` |
|      384 |  5153 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        5 |  5154 | `{` |
|        - |  5155 | `	SyToken *pTmp, *pSplit;` |
|      389 |  5156 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      389 |  5157 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|        - |  5158 | `	sxi32 rc;` |
|      192 |  5159 | `	(void)iCompileFlag;` |
|        - |  5160 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      389 |  5161 | `	pGen->pIn++;` |
|        - |  5162 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|        - |  5163 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|        - |  5164 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|        - |  5165 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|        - |  5166 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|      384 |  5167 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|      227 |  5168 | `		&& pGen->pIn->sData.nByte == 4` |
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
|      327 |  5186 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  5187 | `		/* Bare yield — no value */` |
|        3 |  5188 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|        3 |  5189 | `		return SXRET_OK;` |
|        - |  5190 | `	}` |
|        - |  5191 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|      325 |  5192 | `	pSplit = 0;` |
|        - |  5193 | `	{` |
|      325 |  5194 | `		SyToken *pCur = pGen->pIn;` |
|      325 |  5195 | `		sxi32 nNest = 0;` |
|      781 |  5196 | `		while( pCur < pGen->pEnd ){` |
|      475 |  5197 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       18 |  5198 | `				nNest++;` |
|      467 |  5199 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       18 |  5200 | `				nNest--;` |
|      451 |  5201 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|       16 |  5202 | `				pSplit = pCur;` |
|       16 |  5203 | `				break;` |
|        - |  5204 | `			}` |
|      461 |  5205 | `			pCur++;` |
|        5 |  5206 | `		}` |
|        - |  5207 | `	}` |
|      325 |  5208 | `	pTmp = pGen->pEnd;` |
|      325 |  5209 | `	if( pSplit ){` |
|        - |  5210 | `		/* yield $key => $value */` |
|       16 |  5211 | `		pGen->pEnd = pSplit;` |
|       16 |  5212 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       16 |  5213 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       16 |  5214 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|       16 |  5215 | `		pGen->pEnd = pTmp;` |
|       16 |  5216 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       16 |  5217 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       16 |  5218 | `		iP1 = 1;` |
|       16 |  5219 | `		iP2 = 1;` |
|        9 |  5220 | `	}else{` |
|        - |  5221 | `		/* yield $value */` |
|      311 |  5222 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      311 |  5223 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      311 |  5224 | `		if( rc != SXERR_EMPTY ){` |
|      311 |  5225 | `			iP1 = 1;` |
|      153 |  5226 | `		}` |
|        - |  5227 | `	}` |
|      325 |  5228 | `	pGen->pEnd = pTmp;` |
|      325 |  5229 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      325 |  5230 | `	return SXRET_OK;` |
|      197 |  5231 | `}` |
|        - |  5232 | `/*` |
|        - |  5233 | ` * Compile the die/exit language construct.` |
|        - |  5234 | ` * The role of these constructs is to terminate execution of the script.` |
|        - |  5235 | ` * Shutdown functions will always be executed even if exit() is called.` |
|        - |  5236 | ` */` |
|      122 |  5237 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|        5 |  5238 | `{` |
|      127 |  5239 | `	sxi32 nExpr = 0;` |
|        - |  5240 | `	sxi32 rc;` |
|        - |  5241 | `	/* Jump the die/exit keyword */` |
|      127 |  5242 | `	pGen->pIn++;` |
|      127 |  5243 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  5244 | `		/* Compile the expression */` |
|      127 |  5245 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      127 |  5246 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5247 | `			return SXERR_ABORT;` |
|      127 |  5248 | `		}else if(rc != SXERR_EMPTY ){` |
|      127 |  5249 | `			nExpr = 1;` |
|       61 |  5250 | `		}` |
|       61 |  5251 | `	}` |
|        - |  5252 | `	/* Emit the HALT instruction */` |
|      127 |  5253 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      127 |  5254 | `	return SXRET_OK;` |
|       66 |  5255 | `}` |
|        - |  5256 | `/*` |
|        - |  5257 | ` * Compile the 'echo' language construct.` |
|        - |  5258 | ` */` |
|    17146 |  5259 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|        5 |  5260 | `{` |
|    17151 |  5261 | `	SyToken *pTmp,*pNext = 0;` |
|        - |  5262 | `	sxi32 rc;` |
|        - |  5263 | `	/* Jump the 'echo' keyword */` |
|    17151 |  5264 | `	pGen->pIn++;` |
|        - |  5265 | `	/* Compile arguments one after one */` |
|    17151 |  5266 | `	pTmp = pGen->pEnd;` |
|    41945 |  5267 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|    24799 |  5268 | `		if( pGen->pIn < pNext ){` |
|    24799 |  5269 | `			pGen->pEnd = pNext;` |
|    24799 |  5270 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|    24799 |  5271 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5272 | `				return SXERR_ABORT;` |
|    24799 |  5273 | `			}else if( rc != SXERR_EMPTY ){` |
|        - |  5274 | `				/* Emit the consume instruction */` |
|    24775 |  5275 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    12385 |  5276 | `			}` |
|    12397 |  5277 | `		}` |
|        - |  5278 | `		/* Jump trailing commas */` |
|    32447 |  5279 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     7653 |  5280 | `			pNext++;` |
|        5 |  5281 | `		}` |
|    24799 |  5282 | `		pGen->pIn = pNext;` |
|        5 |  5283 | `	}` |
|        - |  5284 | `	/* Restore token stream */` |
|    17151 |  5285 | `	pGen->pEnd = pTmp;` |
|    17151 |  5286 | `	return SXRET_OK;` |
|     8578 |  5287 | `}` |
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
|  2892872 |  5468 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
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
|  2892877 |  5479 | `	if( pFromImport ){` |
|  2361579 |  5480 | `		*pFromImport = 0;` |
|  1180787 |  5481 | `	}` |
|  2892877 |  5482 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  2892877 |  5483 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|      ! 0 |  5484 | `		return nOrigIdx;` |
|        - |  5485 | `	}` |
|  2892877 |  5486 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  2892877 |  5487 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|        - |  5488 | `	/* Skip if already qualified (contains backslash) */` |
|  2892877 |  5489 | `	hasNsSep = 0;` |
| 37284141 |  5490 | `	for( k = 0; k < nLit; k++ ){` |
| 34391277 |  5491 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 17195637 |  5492 | `	}` |
|  2892877 |  5493 | `	if( hasNsSep ){` |
|       10 |  5494 | `		return nOrigIdx;` |
|        - |  5495 | `	}` |
|        - |  5496 | `	/* Check use imports first (works even outside namespaces) */` |
|  2892869 |  5497 | `	SyBlobReset(&pGen->sWorker);` |
|  2892869 |  5498 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  2892869 |  5499 | `	if( pImport ){` |
|       41 |  5500 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       41 |  5501 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|       41 |  5502 | `		if( pFromImport ){` |
|       18 |  5503 | `			*pFromImport = 1;` |
|        8 |  5504 | `		}` |
|       23 |  5505 | `	}else{` |
|  2892833 |  5506 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  2892743 |  5507 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
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
|  1446441 |  5526 | `}` |
|        - |  5527 | `/*` |
|        - |  5528 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|        - |  5529 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|        - |  5530 | ` */` |
|   187772 |  5531 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 |  5532 | `{` |
|        - |  5533 | `	SyHashEntry *pImport;` |
|        - |  5534 | `	/* Check use imports first */` |
|   187777 |  5535 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   187777 |  5536 | `	if( pImport ){` |
|       19 |  5537 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       19 |  5538 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|       19 |  5539 | `		return;` |
|        - |  5540 | `	}` |
|        - |  5541 | `	/* Prepend current namespace if active */` |
|   187761 |  5542 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        8 |  5543 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        8 |  5544 | `		SyBlobAppend(pOut,"\\",1);` |
|        3 |  5545 | `	}` |
|   187761 |  5546 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    93891 |  5547 | `}` |
|        - |  5548 | `/*` |
|        - |  5549 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|        - |  5550 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|        - |  5551 | ` * The caller must release pOut when done.` |
|        - |  5552 | ` */` |
|   262068 |  5553 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 |  5554 | `{` |
|   262073 |  5555 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     3947 |  5556 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     3947 |  5557 | `		SyBlobAppend(pOut,"\\",1);` |
|     1971 |  5558 | `	}` |
|   262073 |  5559 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   262073 |  5560 | `}` |
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
|        3 |  5598 | `{` |
|       17 |  5599 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       11 |  5600 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       11 |  5601 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       11 |  5602 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       11 |  5603 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       11 |  5604 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|        3 |  5605 | `	return "token";` |
|       10 |  5606 | `}` |
|     3990 |  5607 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|        5 |  5608 | `{` |
|        - |  5609 | `	sxu32 nLine;` |
|        - |  5610 | `	sxi32 rc;` |
|     3995 |  5611 | `	nLine = pGen->pIn->nLine;` |
|     3995 |  5612 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|        - |  5613 | `	/* Reset namespace and clear previous use imports */` |
|     3995 |  5614 | `	SyBlobReset(&pGen->sNamespace);` |
|     3995 |  5615 | `	SyHashRelease(&pGen->hUseImports);` |
|     3995 |  5616 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     3995 |  5617 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     3995 |  5618 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     3995 |  5619 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     3995 |  5620 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     3995 |  5621 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  5622 | `		/* Global namespace (bare "namespace;") */` |
|      ! 0 |  5623 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 |  5624 | `		return SXRET_OK;` |
|        - |  5625 | `	}` |
|     3995 |  5626 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        - |  5627 | `		/* namespace; — switch to global namespace */` |
|      ! 0 |  5628 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 |  5629 | `		return SXRET_OK;` |
|        - |  5630 | `	}` |
|     3995 |  5631 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|        - |  5632 | `		/* namespace { } — global namespace block */` |
|      ! 0 |  5633 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 |  5634 | `		return SXRET_OK;` |
|        - |  5635 | `	}` |
|        - |  5636 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     8027 |  5637 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     4037 |  5638 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        - |  5639 | `			/* Append backslash separator */` |
|       26 |  5640 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       26 |  5641 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|       11 |  5642 | `			}` |
|       15 |  5643 | `		}else{` |
|        - |  5644 | `			/* Append identifier */` |
|     4015 |  5645 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - |  5646 | `		}` |
|     4037 |  5647 | `		pGen->pIn++;` |
|        5 |  5648 | `	}` |
|        - |  5649 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|        - |  5650 | `	 * at the correct program counter, not just the last one compiled. */` |
|        - |  5651 | `	{` |
|     3995 |  5652 | `		char *zNsDup = 0;` |
|     3995 |  5653 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     5987 |  5654 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     3988 |  5655 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     1994 |  5656 | `		}` |
|     3995 |  5657 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|        - |  5658 | `	}` |
|     3995 |  5659 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        8 |  5660 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  5661 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|        4 |  5662 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        6 |  5663 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5664 | `			return SXERR_ABORT;` |
|        - |  5665 | `		}` |
|        2 |  5666 | `	}` |
|     3995 |  5667 | `	return SXRET_OK;` |
|     2000 |  5668 | `}` |
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
|       24 |  5751 | `			pGen->pIn++; /* Jump 'as' */` |
|       24 |  5752 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|       24 |  5753 | `				sAlias = pGen->pIn->sData;` |
|       24 |  5754 | `				pGen->pIn++;` |
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
|   240966 |  6058 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|        5 |  6059 | `{` |
|        - |  6060 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |  6061 | `	SySet *pInstrContainer;` |
|        - |  6062 | `	sxi32 rc;` |
|        - |  6063 | `	/* Swap token stream */` |
|   240971 |  6064 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   240971 |  6065 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   240971 |  6066 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|        - |  6067 | `	/* Compile the expression holding the argument value */` |
|   240971 |  6068 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  6069 | `	/* Emit the done instruction */` |
|   240971 |  6070 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   240971 |  6071 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   240971 |  6072 | `	RE_SWAP_DELIMITER(pGen);` |
|   240971 |  6073 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  6074 | `		return SXERR_ABORT;` |
|        - |  6075 | `	}` |
|   240971 |  6076 | `	return SXRET_OK;` |
|   120488 |  6077 | `}` |
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
|   491248 |  6115 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|        5 |  6116 | `{` |
|        - |  6117 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|        - |  6118 | `	SyToken *pIn;  /* Token stream */` |
|        - |  6119 | `	SyBlob sSig;         /* Function signature */` |
|        - |  6120 | `	char *zDup;          /* Copy of argument name */` |
|        - |  6121 | `	sxi32 rc;` |
|        - |  6122 |  |
|   491253 |  6123 | `	pIn = pGen->pIn;` |
|   491253 |  6124 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|        - |  6125 | `	/* Process arguments one after one */` |
|   604342 |  6126 | `	for(;;){` |
|  1208689 |  6127 | `		if( pIn >= pEnd ){` |
|        - |  6128 | `			/* No more arguments to process */` |
|   491237 |  6129 | `			break;` |
|        - |  6130 | `		}` |
|   717457 |  6131 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   717457 |  6132 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   717457 |  6133 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   717457 |  6134 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|   717457 |  6135 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|        - |  6136 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|        - |  6137 | `		 * first token inside the main token stream */` |
|   717457 |  6138 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  6139 | `			return SXERR_ABORT;` |
|        - |  6140 | `		}` |
|        - |  6141 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|        - |  6142 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|        - |  6143 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|        - |  6144 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|        - |  6145 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|        - |  6146 | `		{` |
|   717457 |  6147 | `			int bReadonly = 0, bVisSeen = 0;` |
|   717457 |  6148 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|   717457 |  6149 | `			sxi32 iSetVisFlag = 0;` |
|        - |  6150 | `			int nSetTok;` |
|        - |  6151 | `			sxi32 nSetVis;` |
|   717457 |  6152 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|        3 |  6153 | `				bReadonly = 1;` |
|        3 |  6154 | `				pIn++;` |
|        1 |  6155 | `			}` |
|   717457 |  6156 | `			nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|   717457 |  6157 | `			if( nSetVis ){` |
|        - |  6158 | ``				/* Leading `private(set)` etc: promoted with a public read side */`` |
|        3 |  6159 | `				iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|        3 |  6160 | `				bVisSeen = 1;` |
|        3 |  6161 | `				pIn += nSetTok;` |
|        3 |  6162 | `				if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|      ! 0 |  6163 | `					bReadonly = 1;` |
|      ! 0 |  6164 | `					pIn++;` |
|        1 |  6165 | `				}` |
|   717456 |  6166 | `			}else if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|    81959 |  6167 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|    81959 |  6168 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
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
|    40977 |  6185 | `			}` |
|   717457 |  6186 | `			if( iSetVisFlag == PH7_CLASS_ATTR_PRIVATE_SET ){` |
|        5 |  6187 | `				sArg.iFlags \|= VM_FUNC_ARG_PRIV_SET;` |
|   717455 |  6188 | `			}else if( iSetVisFlag == PH7_CLASS_ATTR_PROTECTED_SET ){` |
|      ! 0 |  6189 | `				sArg.iFlags \|= VM_FUNC_ARG_PROT_SET;` |
|      ! 0 |  6190 | `			}` |
|   717457 |  6191 | `			if( bVisSeen \|\| bReadonly ){` |
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
|   717448 |  6213 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   419237 |  6214 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   119073 |  6215 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|    97613 |  6216 | `			sxu32 nLineLocal = pIn->nLine;` |
|    97613 |  6217 | `			sxi32 iTFlags = 0;` |
|    97613 |  6218 | `			pGen->pIn = pIn;` |
|    97613 |  6219 | `			rc = GenStateParseUnionTypeDecl(` |
|    48804 |  6220 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|    48804 |  6221 | `				&iTFlags, &sArg.sTypeName,` |
|        - |  6222 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|        - |  6223 | `				/* bAllowVoid */ 0,` |
|    48804 |  6224 | `						nLineLocal);` |
|    97613 |  6225 | `			pIn = pGen->pIn;` |
|    97613 |  6226 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  6227 | `				return SXERR_ABORT;` |
|    97613 |  6228 | `			}else if( rc == SXERR_CORRUPT ){` |
|        - |  6229 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|        3 |  6230 | `				return SXERR_SYNTAX;` |
|    97611 |  6231 | `			}else if( rc == SXERR_SYNTAX ){` |
|       12 |  6232 | `				if( pIn < pEnd ){` |
|       16 |  6233 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|        - |  6234 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|        4 |  6235 | `						&pIn->sData);` |
|        8 |  6236 | `				}else{` |
|      ! 0 |  6237 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|        - |  6238 | `						"syntax error, unexpected end of file");` |
|        - |  6239 | `				}` |
|       12 |  6240 | `				return SXERR_SYNTAX;` |
|        - |  6241 | `			}` |
|    97603 |  6242 | `			sArg.iFlags \|= iTFlags;` |
|    48799 |  6243 | `		}` |
|   717443 |  6244 | `		if( pIn >= pEnd ){` |
|      ! 0 |  6245 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|      ! 0 |  6246 | `			return rc;` |
|        - |  6247 | `		}` |
|   717443 |  6248 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|        - |  6249 | `			/* Pass by reference,record that */` |
|     3929 |  6250 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|     3929 |  6251 | `			pIn++;` |
|     1962 |  6252 | `		}` |
|   717443 |  6253 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|        - |  6254 | `			/* Variadic parameter: ...$args */` |
|    19529 |  6255 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|    19529 |  6256 | `			pIn++;` |
|     9762 |  6257 | `		}` |
|   717443 |  6258 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  6259 | `			/* Invalid argument */` |
|      ! 0 |  6260 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|      ! 0 |  6261 | `			return rc;` |
|        - |  6262 | `		}` |
|   717443 |  6263 | `		pIn++; /* Jump the dollar sign */` |
|        - |  6264 | `		/* Copy argument name */` |
|   717443 |  6265 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   717443 |  6266 | `		if( zDup == 0 ){` |
|      ! 0 |  6267 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  6268 | `			return SXERR_ABORT;` |
|        - |  6269 | `		}` |
|   717443 |  6270 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   717443 |  6271 | `		pIn++;` |
|   717443 |  6272 | `		if( pIn < pEnd ){` |
|   373917 |  6273 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|        - |  6274 | `				SyToken *pDefend;` |
|   240973 |  6275 | `				sxi32 iNest = 0;` |
|   240973 |  6276 | `				pIn++; /* Jump the equal sign */` |
|   240973 |  6277 | `				pDefend = pIn;` |
|        - |  6278 | `				/* Process the default value associated with this argument */` |
|   513039 |  6279 | `				while( pDefend < pEnd ){` |
|   365337 |  6280 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|    93271 |  6281 | `						break;` |
|        - |  6282 | `					}` |
|   272071 |  6283 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|        - |  6284 | `						/* Increment nesting level */` |
|    15549 |  6285 | `						iNest++;` |
|   264299 |  6286 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|        - |  6287 | `						/* Decrement nesting level */` |
|    15549 |  6288 | `						iNest--;` |
|     7772 |  6289 | `					}` |
|   272071 |  6290 | `					pDefend++;` |
|        5 |  6291 | `				}` |
|   240973 |  6292 | `				if( pIn >= pDefend ){` |
|        3 |  6293 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|        3 |  6294 | `					return rc;` |
|        - |  6295 | `				}` |
|        - |  6296 | `				/* Process default value */` |
|   240971 |  6297 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   240971 |  6298 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  6299 | `					return rc;` |
|        - |  6300 | `				}` |
|        - |  6301 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|        - |  6302 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|        - |  6303 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|        - |  6304 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|        - |  6305 | `				 * arg-type check lets null through. */` |
|   240966 |  6306 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|   145752 |  6307 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|   145749 |  6308 | `					&& &pIn[1] == pDefend` |
|    46647 |  6309 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|    34978 |  6310 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|    21373 |  6311 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|    15547 |  6312 | `					sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|     7771 |  6313 | `				}` |
|        - |  6314 | `				/* Point beyond the default value */` |
|   240971 |  6315 | `				pIn = pDefend;` |
|   120483 |  6316 | `			}` |
|   373915 |  6317 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 |  6318 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|      ! 0 |  6319 | `				return rc;` |
|        - |  6320 | `			}` |
|   373915 |  6321 | `			pIn++; /* Jump the trailing comma */` |
|   186955 |  6322 | `		}` |
|        - |  6323 | `		/* Append argument signature */` |
|   717441 |  6324 | `		if( sArg.nType > 0 ){` |
|    97541 |  6325 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|        - |  6326 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    15621 |  6327 | `				int marker = 'o';` |
|    15621 |  6328 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    15621 |  6329 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|     7813 |  6330 | `			}else{` |
|        - |  6331 | `				int c;` |
|    81925 |  6332 | `				c = 'n'; /* cc warning */` |
|        - |  6333 | `				/* Type leading character */` |
|    81925 |  6334 | `				switch(sArg.nType){` |
|     5832 |  6335 | `				case MEMOBJ_HASHMAP:` |
|        - |  6336 | `					/* Hashmap aka 'array' */` |
|    11669 |  6337 | `					c = 'h';` |
|    11669 |  6338 | `					break;` |
|     9828 |  6339 | `				case MEMOBJ_INT:` |
|        - |  6340 | `					/* Integer */` |
|    19661 |  6341 | `					c = 'i';` |
|    19661 |  6342 | `					break;` |
|        2 |  6343 | `				case MEMOBJ_BOOL:` |
|        - |  6344 | `					/* Bool */` |
|        5 |  6345 | `					c = 'b';` |
|        5 |  6346 | `					break;` |
|        5 |  6347 | `				case MEMOBJ_REAL:` |
|        - |  6348 | `					/* Float */` |
|       12 |  6349 | `					c = 'f';` |
|       12 |  6350 | `					break;` |
|    25285 |  6351 | `				case MEMOBJ_STRING:` |
|        - |  6352 | `					/* String */` |
|    50575 |  6353 | `					c = 's';` |
|    50575 |  6354 | `					break;` |
|        7 |  6355 | `				case MEMOBJ_OBJ:` |
|        - |  6356 | `					/* Object */` |
|       16 |  6357 | `					c = 'o';` |
|       14 |  6358 | `					break;` |
|        1 |  6359 | `				default:` |
|        2 |  6360 | `					break;` |
|        - |  6361 | `				}` |
|    81925 |  6362 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|        - |  6363 | `			}` |
|    48773 |  6364 | `		}else{` |
|        - |  6365 | `			/* No type is associated with this parameter which mean` |
|        - |  6366 | `			 * that this function is not condidate for overloading.` |
|        - |  6367 | `			 */` |
|   619905 |  6368 | `			SyBlobRelease(&sSig);` |
|        - |  6369 | `		}` |
|        - |  6370 | `		/* Save in the argument set */` |
|   717441 |  6371 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|        5 |  6372 | `	}` |
|   491237 |  6373 | `	if( SyBlobLength(&sSig) > 0 ){` |
|        - |  6374 | `		/* Save function signature */` |
|    66397 |  6375 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|    33196 |  6376 | `	}` |
|   491237 |  6377 | `	return SXRET_OK;` |
|   245629 |  6378 | `}` |
|        - |  6379 | `/*` |
|        - |  6380 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|        - |  6381 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|        - |  6382 | ` * the enclosing function. Returns the token just past the nested construct.` |
|        - |  6383 | ` */` |
|    34998 |  6384 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|        5 |  6385 | `{` |
|    35003 |  6386 | `	sxi32 iParen = 0;` |
|    35003 |  6387 | `	pIn++; /* past 'function'/'fn' */` |
|        - |  6388 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|        - |  6389 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|        - |  6390 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|   155593 |  6391 | `	while( pIn < pEnd ){` |
|   155593 |  6392 | `		sxu32 t = pIn->nType;` |
|   155593 |  6393 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|   151655 |  6394 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|   104993 |  6395 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|    85531 |  6396 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|   120595 |  6397 | `		pIn++;` |
|        5 |  6398 | `	}` |
|    19467 |  6399 | `	if( pIn >= pEnd ){ return pIn; }` |
|        - |  6400 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|        - |  6401 | `	{` |
|    19467 |  6402 | `		sxi32 d = 0;` |
|   773341 |  6403 | `		while( pIn < pEnd ){` |
|   773341 |  6404 | `			sxu32 t = pIn->nType;` |
|   773341 |  6405 | `			if( t & PH7_TK_OCB ){ d++; }` |
|   742223 |  6406 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|   753879 |  6407 | `			pIn++;` |
|        5 |  6408 | `		}` |
|        - |  6409 | `	}` |
|    19467 |  6410 | `	return pIn;` |
|    17504 |  6411 | `}` |
|        - |  6412 | `/*` |
|        - |  6413 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|        - |  6414 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|        - |  6415 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|        - |  6416 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|        - |  6417 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|        - |  6418 | ` * detached-mini-program path untouched.` |
|        - |  6419 | ` */` |
|        - |  6420 | `/*` |
|        - |  6421 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|        - |  6422 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|        - |  6423 | ` * mixed, object.` |
|        - |  6424 | ` */` |
|       28 |  6425 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|        3 |  6426 | `{` |
|        - |  6427 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|        - |  6428 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|        - |  6429 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|        - |  6430 | `	};` |
|        - |  6431 | `	sxu32 i;` |
|       31 |  6432 | `	if( nName > 0 && zName[0] == '\\' ){` |
|      ! 0 |  6433 | `		zName++;` |
|      ! 0 |  6434 | `		nName--;` |
|      ! 0 |  6435 | `	}` |
|       63 |  6436 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|       59 |  6437 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|       27 |  6438 | `			return 1;` |
|        - |  6439 | `		}` |
|       17 |  6440 | `	}` |
|        5 |  6441 | `	return 0;` |
|       17 |  6442 | `}` |
|        - |  6443 | `/*` |
|        - |  6444 | ` * One atom of a generator's declared return type: is it a supertype of` |
|        - |  6445 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|        - |  6446 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|        - |  6447 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|        - |  6448 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|        - |  6449 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|        - |  6450 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|        - |  6451 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|        - |  6452 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|        - |  6453 | ` * both rather than fatal on valid code (a recorded divergence).` |
|        - |  6454 | ` */` |
|       26 |  6455 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|        4 |  6456 | `{` |
|       30 |  6457 | `	if( nType == MEMOBJ_OBJ ){` |
|      ! 0 |  6458 | ``		return 1; /* bare `object` */`` |
|        - |  6459 | `	}` |
|       30 |  6460 | `	if( nType != SXU32_HIGH ){` |
|        3 |  6461 | `		return 0; /* scalar/array/void/never/null/... */` |
|        - |  6462 | `	}` |
|       27 |  6463 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|       23 |  6464 | `		return 1;` |
|        - |  6465 | `	}` |
|        - |  6466 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|        - |  6467 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|        - |  6468 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|        - |  6469 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|        - |  6470 | `	{` |
|        - |  6471 | `		SyBlob sFQN;` |
|        - |  6472 | `		int bOk;` |
|        5 |  6473 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        5 |  6474 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|        5 |  6475 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|        5 |  6476 | `		SyBlobRelease(&sFQN);` |
|        5 |  6477 | `		return bOk;` |
|        - |  6478 | `	}` |
|       17 |  6479 | `}` |
|        - |  6480 | `/*` |
|        - |  6481 | ` * php 8: a generator function may only declare a return type that is a` |
|        - |  6482 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|        - |  6483 | ` * group qualifies only if every member does. Anything else is php's exact` |
|        - |  6484 | ` * compile-time fatal "Generator return type must be a supertype of` |
|        - |  6485 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|        - |  6486 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|        - |  6487 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|        - |  6488 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|        - |  6489 | ` */` |
|      264 |  6490 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|        5 |  6491 | `{` |
|      269 |  6492 | `	int bOk = 0;` |
|        - |  6493 | `	sxu32 nLine;` |
|        - |  6494 | `	sxi32 rc;` |
|      269 |  6495 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|      243 |  6496 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|        - |  6497 | `	}` |
|       30 |  6498 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|      ! 0 |  6499 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  6500 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|        - |  6501 | `		sxu32 i,j;` |
|      ! 0 |  6502 | `		for( i = 0; i < n && !bOk; i++ ){` |
|        - |  6503 | `			int bGroupOk;` |
|      ! 0 |  6504 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|      ! 0 |  6505 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|        - |  6506 | `			}` |
|      ! 0 |  6507 | `			bGroupOk = 1;` |
|      ! 0 |  6508 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|      ! 0 |  6509 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|      ! 0 |  6510 | `					bGroupOk = 0;` |
|      ! 0 |  6511 | `					break;` |
|        - |  6512 | `				}` |
|      ! 0 |  6513 | `			}` |
|      ! 0 |  6514 | `			bOk = bGroupOk;` |
|      ! 0 |  6515 | `		}` |
|      ! 0 |  6516 | `	}else{` |
|       30 |  6517 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|        - |  6518 | `	}` |
|       30 |  6519 | `	if( bOk ){` |
|       27 |  6520 | `		return SXRET_OK;` |
|        - |  6521 | `	}` |
|        - |  6522 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|        - |  6523 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|        - |  6524 | `	 * token of this stream — its line is the function's closing brace. php` |
|        - |  6525 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|        - |  6526 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|        3 |  6527 | `	nLine = pGen->pIn[-1].nLine;` |
|        - |  6528 | `	{` |
|        3 |  6529 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|        3 |  6530 | `		if( sGiven.nByte < 1 ){` |
|      ! 0 |  6531 | `			sGiven = pFunc->sReturnClass;` |
|      ! 0 |  6532 | `		}` |
|        3 |  6533 | `		if( sGiven.nByte < 1 ){` |
|        - |  6534 | ``			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the`` |
|        - |  6535 | `			 * rendered type text, so sReturnTypeName arrives empty for them —` |
|        - |  6536 | `			 * name them here (the root fix belongs to that renderer, §3.7). */` |
|      ! 0 |  6537 | `			const char *zScalar =` |
|      ! 0 |  6538 | `				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :` |
|      ! 0 |  6539 | `				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";` |
|      ! 0 |  6540 | `			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));` |
|      ! 0 |  6541 | `		}` |
|        3 |  6542 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  6543 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|        - |  6544 | `	}` |
|        3 |  6545 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|      137 |  6546 | `}` |
|  1413276 |  6547 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|        5 |  6548 | `{` |
|  1413281 |  6549 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|  1413281 |  6550 | `	SyToken *pEnd = pGen->pEnd;` |
|  1413281 |  6551 | `	sxi32 iDepth = 0;` |
|  1413281 |  6552 | `	int bStarted = 0;` |
| 63545597 |  6553 | `	while( pIn < pEnd ){` |
| 63545597 |  6554 | `		sxu32 t = pIn->nType;` |
| 63545597 |  6555 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 60566111 |  6556 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 57587003 |  6557 | `		if( t & PH7_TK_KEYWORD ){` |
|  4665927 |  6558 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|  4665927 |  6559 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|  4665663 |  6560 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|        - |  6561 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|  2315330 |  6562 | `		}` |
| 57551741 |  6563 | `		pIn++;` |
|        5 |  6564 | `	}` |
|  1413017 |  6565 | `	return FALSE;` |
|   706643 |  6566 | `}` |
|        - |  6567 | `/*` |
|        - |  6568 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|        - |  6569 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|        - |  6570 | ` * and this routine takes care of generating the appropriate error message.` |
|        - |  6571 | ` */` |
|  1413276 |  6572 | `static sxi32 GenStateCompileFuncBody(` |
|        - |  6573 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - |  6574 | `	ph7_vm_func *pFunc    /* Function state */` |
|        - |  6575 | `	)` |
|        5 |  6576 | `{` |
|        - |  6577 | `	SySet *pInstrContainer; /* Instruction container */` |
|        - |  6578 | `	GenBlock *pBlock;` |
|        - |  6579 | `	sxu32 nGotoOfft;` |
|        - |  6580 | `	sxi32 rc;` |
|        - |  6581 | `	/* Attach the new function */` |
|  1413281 |  6582 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  1413281 |  6583 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  6584 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|        - |  6585 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  6586 | `		return SXERR_ABORT;` |
|        - |  6587 | `	}` |
|  1413281 |  6588 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|        - |  6589 | `	/* Swap bytecode containers */` |
|  1413281 |  6590 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  1413281 |  6591 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|        - |  6592 | `	/* Emit constructor property promotion prologue:` |
|        - |  6593 | `	 *   $this->NAME = $NAME;` |
|        - |  6594 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|        - |  6595 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|        - |  6596 | `	{` |
|  1413281 |  6597 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|        - |  6598 | `		sxu32 i;` |
|  2099489 |  6599 | `		for( i = 0; i < nArg; i++ ){` |
|   686213 |  6600 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|        - |  6601 | `			char *zSrc;` |
|        - |  6602 | `			sxu32 nSrc,nName;` |
|        - |  6603 | `			SySet sToken;` |
|        - |  6604 | `			SyToken *pTmpIn,*pTmpEnd;` |
|        - |  6605 | `			sxi32 rcPromote;` |
|   686213 |  6606 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   686139 |  6607 | `				continue;` |
|        - |  6608 | `			}` |
|        - |  6609 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|        - |  6610 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|        - |  6611 | `			 * copied), so it must outlive the function — never free it. The` |
|        - |  6612 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|        - |  6613 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|       79 |  6614 | `			nName = SyStringLength(&pArg->sName);` |
|       79 |  6615 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|       79 |  6616 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|       79 |  6617 | `			if( zSrc == 0 ){` |
|      ! 0 |  6618 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  6619 | `				GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  6620 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  6621 | `				return SXERR_ABORT;` |
|        - |  6622 | `			}` |
|        - |  6623 | `			{` |
|       79 |  6624 | `				char *z = zSrc;` |
|       79 |  6625 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|       79 |  6626 | `				z += sizeof("$this->")-1;` |
|       79 |  6627 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|       79 |  6628 | `				z += nName;` |
|       79 |  6629 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|       79 |  6630 | `				z += sizeof(" = $")-1;` |
|       79 |  6631 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|       79 |  6632 | `				z += nName;` |
|       79 |  6633 | `				*z = 0;` |
|        - |  6634 | `			}` |
|       79 |  6635 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       79 |  6636 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);` |
|       79 |  6637 | `			pTmpIn = pGen->pIn;` |
|       79 |  6638 | `			pTmpEnd = pGen->pEnd;` |
|       79 |  6639 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|       79 |  6640 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       79 |  6641 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|       79 |  6642 | `			pGen->pIn = pTmpIn;` |
|       79 |  6643 | `			pGen->pEnd = pTmpEnd;` |
|       79 |  6644 | `			SySetRelease(&sToken);` |
|       79 |  6645 | `			if( rcPromote == SXERR_ABORT ){` |
|      ! 0 |  6646 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  6647 | `				GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  6648 | `				return SXERR_ABORT;` |
|        - |  6649 | `			}` |
|        - |  6650 | `			/* Discard the assignment result — this is a statement expression. */` |
|       79 |  6651 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       42 |  6652 | `		}` |
|        - |  6653 | `	}` |
|        - |  6654 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|        - |  6655 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|        - |  6656 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|        - |  6657 | `	 * generator — and vice versa — is classified independently. */` |
|        - |  6658 | `	{` |
|  1413281 |  6659 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|  1413281 |  6660 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|        - |  6661 | `		/* Compile the body */` |
|  1413281 |  6662 | `		PH7_CompileBlock(&(*pGen),0);` |
|  1413281 |  6663 | `		pGen->bInGenerator = bSavedGen;` |
|        - |  6664 | `	}` |
|        - |  6665 | `	/* Fix exception jumps now the destination is resolved */` |
|  1413281 |  6666 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        - |  6667 | `	/* Emit the final return if not yet done */` |
|  1413281 |  6668 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - |  6669 | `	/* Fix gotos jumps now the destination is resolved */` |
|  1413281 |  6670 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|      ! 0 |  6671 | `		rc = SXERR_ABORT;` |
|      ! 0 |  6672 | `	}` |
|  1413281 |  6673 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|        - |  6674 | `	/* Restore the default container */` |
|  1413281 |  6675 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - |  6676 | `	/* Leave function block */` |
|  1413281 |  6677 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  1413281 |  6678 | `	if( rc == SXERR_ABORT ){` |
|        - |  6679 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  6680 | `		return SXERR_ABORT;` |
|        - |  6681 | `	}` |
|        - |  6682 | `	/* Scan for yield opcodes to detect generator functions */` |
|        - |  6683 | `	{` |
|  1413281 |  6684 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|        - |  6685 | `		sxu32 i;` |
| 38620497 |  6686 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 37207485 |  6687 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|      269 |  6688 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      269 |  6689 | `				break;` |
|        - |  6690 | `			}` |
| 18603613 |  6691 | `		}` |
|        - |  6692 | `	}` |
|  1413281 |  6693 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  6694 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|      269 |  6695 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|      ! 0 |  6696 | `			return SXERR_ABORT;` |
|        - |  6697 | `		}` |
|      132 |  6698 | `	}` |
|        - |  6699 | `	/* All done, function body compiled */` |
|  1413281 |  6700 | `	return SXRET_OK;` |
|   706643 |  6701 | `}` |
|        - |  6702 | `/*` |
|        - |  6703 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|        - |  6704 | ` * According to the PHP language reference manual.` |
|        - |  6705 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|        - |  6706 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|        - |  6707 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|        - |  6708 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|        - |  6709 | ` *  Functions need not be defined before they are referenced.` |
|        - |  6710 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|        - |  6711 | ` *  a function even if they were defined inside and vice versa.` |
|        - |  6712 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|        - |  6713 | ` *  calls with over 32-64 recursion levels.` |
|        - |  6714 | ` *` |
|        - |  6715 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|        - |  6716 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|        - |  6717 | ` * on these extension.` |
|        - |  6718 | ` */` |
|        - |  6719 | `/*` |
|        - |  6720 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|        - |  6721 | ` */` |
|      570 |  6722 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|        5 |  6723 | `{` |
|        - |  6724 | `	sxu32 i;` |
|     1611 |  6725 | `	for( i = 0; i < n; i++ ){` |
|     1381 |  6726 | `		int a = zA[i], b = zB[i];` |
|     1381 |  6727 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|     1381 |  6728 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|     1381 |  6729 | `		if( a != b ) return a - b;` |
|      523 |  6730 | `	}` |
|      235 |  6731 | `	return 0;` |
|      290 |  6732 | `}` |
|        - |  6733 | `/*` |
|        - |  6734 | ` * Internal type-atom kinds used during union type parsing.` |
|        - |  6735 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|        - |  6736 | ` * (which are positive bit values stored in sxu32).` |
|        - |  6737 | ` */` |
|        - |  6738 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|        - |  6739 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|        - |  6740 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|        - |  6741 |  |
|        - |  6742 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|        - |  6743 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|        - |  6744 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|        - |  6745 |  |
|        - |  6746 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|        - |  6747 | `struct PhlTypeAtom {` |
|        - |  6748 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|        - |  6749 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|        - |  6750 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|        - |  6751 | `	sxu32 nCanon;` |
|        - |  6752 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|        - |  6753 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|        - |  6754 | `};` |
|        - |  6755 |  |
|        - |  6756 | `/*` |
|        - |  6757 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|        - |  6758 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|        - |  6759 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|        - |  6760 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|        - |  6761 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|        - |  6762 | ` * already be consumed by the caller.` |
|        - |  6763 | ` */` |
|    98698 |  6764 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|        5 |  6765 | `{` |
|    98703 |  6766 | `	SyToken *pIn = pGen->pIn;` |
|    98703 |  6767 | `	SyZero(pOut, sizeof(*pOut));` |
|    98703 |  6768 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|    98703 |  6769 | `	if( pIn >= pGen->pEnd ){` |
|      ! 0 |  6770 | `		return SXERR_SYNTAX;` |
|        - |  6771 | `	}` |
|        - |  6772 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|    98703 |  6773 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|        8 |  6774 | `		pIn++;` |
|        8 |  6775 | `		if( pIn >= pGen->pEnd ){` |
|      ! 0 |  6776 | `			return SXERR_SYNTAX;` |
|        - |  6777 | `		}` |
|        3 |  6778 | `	}` |
|    98703 |  6779 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  6780 | `		return SXERR_SYNTAX;` |
|        - |  6781 | `	}` |
|    98703 |  6782 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|    82627 |  6783 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|    82627 |  6784 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|    11695 |  6785 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|    76782 |  6786 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|       81 |  6787 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|    70899 |  6788 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|    19989 |  6789 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|    60869 |  6790 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|    50795 |  6791 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|    25482 |  6792 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|       41 |  6793 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|       68 |  6794 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|       27 |  6795 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|       37 |  6796 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       14 |  6797 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|       23 |  6798 | `			pOut->nType = SXU32_HIGH;` |
|       23 |  6799 | `			pOut->sClass = pIn->sData;` |
|       13 |  6800 | `		}else{` |
|        3 |  6801 | `			return SXERR_SYNTAX;` |
|        - |  6802 | `		}` |
|    82625 |  6803 | `		pIn++;` |
|    41315 |  6804 | `	}else{` |
|        - |  6805 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|        - |  6806 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|    16081 |  6807 | `		SyString *pT = &pIn->sData;` |
|    16081 |  6808 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|       34 |  6809 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|       34 |  6810 | `			pIn++;` |
|    16066 |  6811 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|      177 |  6812 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|      177 |  6813 | `			pIn++;` |
|    15965 |  6814 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       26 |  6815 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       26 |  6816 | `			pIn++;` |
|       15 |  6817 | `		}else{` |
|        - |  6818 | `			/* Class / interface name; consume namespace path a\b\c */` |
|    15857 |  6819 | `			SyToken *pFirst = pIn;` |
|    15857 |  6820 | `			SyToken *pLast = pIn;` |
|    15857 |  6821 | `			pOut->nType = SXU32_HIGH;` |
|    15857 |  6822 | `			pOut->sClass = pIn->sData;` |
|    15857 |  6823 | `			pIn++;` |
|    23781 |  6824 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|    15860 |  6825 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|        3 |  6826 | `				pLast = &pIn[1];` |
|        3 |  6827 | `				pIn += 2;` |
|        1 |  6828 | `			}` |
|    15857 |  6829 | `			if( pLast != pFirst ){` |
|        3 |  6830 | `				const char *zFirst = pFirst->sData.zString;` |
|        3 |  6831 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|        3 |  6832 | `				pOut->sClass.zString = zFirst;` |
|        3 |  6833 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|        1 |  6834 | `			}` |
|        - |  6835 | `		}` |
|        - |  6836 | `	}` |
|    98701 |  6837 | `	pGen->pIn = pIn;` |
|    98701 |  6838 | `	return SXRET_OK;` |
|    49354 |  6839 | `}` |
|        - |  6840 |  |
|        - |  6841 | `/*` |
|        - |  6842 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|        - |  6843 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|        - |  6844 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|        - |  6845 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|        - |  6846 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|        - |  6847 | ` */` |
|    98520 |  6848 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|        5 |  6849 | `{` |
|        - |  6850 | `	int i;` |
|    98525 |  6851 | `	int nNonNull = 0;` |
|    98525 |  6852 | `	int bAnyIntersection = 0;` |
|        - |  6853 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|    98525 |  6854 | `	sxu32 nMaxGroup = 0;` |
|  3251165 |  6855 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|   197197 |  6856 | `	for( i = 0; i < nAtoms; i++ ){` |
|    98677 |  6857 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    98647 |  6858 | `			nNonNull++;` |
|    98647 |  6859 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|    98647 |  6860 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|    98647 |  6861 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|    49321 |  6862 | `			}` |
|    49321 |  6863 | `		}` |
|    49341 |  6864 | `	}` |
|   197145 |  6865 | `	for( i = 0; i < nAtoms; i++ ){` |
|    98649 |  6866 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|       29 |  6867 | `			bAnyIntersection = 1;` |
|       29 |  6868 | `			break;` |
|        - |  6869 | `		}` |
|    49315 |  6870 | `	}` |
|    98525 |  6871 | `	if( bAnyIntersection ){` |
|        - |  6872 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|        - |  6873 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|        - |  6874 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|       29 |  6875 | `		sxu32 g, nGroups = 0;` |
|       29 |  6876 | `		int bFirstGroup = 1;` |
|       59 |  6877 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|       59 |  6878 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|       35 |  6879 | `			int bFirstMember = 1;` |
|        - |  6880 | `			int bWrap;` |
|       35 |  6881 | `			if( aGroupCount[g] == 0 ) continue;` |
|        - |  6882 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|        - |  6883 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|        - |  6884 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|        - |  6885 | `			 * parens, matching PHP's canonical text. */` |
|       47 |  6886 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|       35 |  6887 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|       35 |  6888 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|      107 |  6889 | `			for( i = 0; i < nAtoms; i++ ){` |
|       77 |  6890 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|       59 |  6891 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|       59 |  6892 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       55 |  6893 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       30 |  6894 | `				}else{` |
|        6 |  6895 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|        - |  6896 | `				}` |
|       59 |  6897 | `				bFirstMember = 0;` |
|       32 |  6898 | `			}` |
|       35 |  6899 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|       35 |  6900 | `			bFirstGroup = 0;` |
|       20 |  6901 | `		}` |
|       29 |  6902 | `		if( bNullable ){` |
|      ! 0 |  6903 | `			SyBlobAppend(pBlob, "\|", 1);` |
|      ! 0 |  6904 | `			SyBlobAppend(pBlob, "null", 4);` |
|      ! 0 |  6905 | `		}` |
|       78 |  6906 | `		return;` |
|        - |  6907 | `	}` |
|    98501 |  6908 | `	if( nNonNull == 1 && bNullable ){` |
|        - |  6909 | `		/* Shorthand: ?T */` |
|      102 |  6910 | `		for( i = 0; i < nAtoms; i++ ){` |
|      102 |  6911 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      102 |  6912 | `			SyBlobAppend(pBlob, "?", 1);` |
|      102 |  6913 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|       24 |  6914 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       14 |  6915 | `			}else{` |
|       82 |  6916 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|        - |  6917 | `			}` |
|      102 |  6918 | `			return;` |
|      ! 0 |  6919 | `		}` |
|      ! 0 |  6920 | `	}` |
|        - |  6921 | `	{` |
|    98403 |  6922 | `		int bFirst = 1;` |
|        - |  6923 | `		/* 1) Classes in declaration order */` |
|   196909 |  6924 | `		for( i = 0; i < nAtoms; i++ ){` |
|    98511 |  6925 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|    15807 |  6926 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    15807 |  6927 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|    15807 |  6928 | `				bFirst = 0;` |
|     7901 |  6929 | `			}` |
|    49258 |  6930 | `		}` |
|        - |  6931 | `		/* 2) Built-ins in canonical order */` |
|        - |  6932 | `		{` |
|        - |  6933 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|        - |  6934 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|        - |  6935 | `			int k;` |
|   688791 |  6936 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  1098815 |  6937 | `				for( i = 0; i < nAtoms; i++ ){` |
|   590929 |  6938 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|    82507 |  6939 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    82507 |  6940 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|    82507 |  6941 | `						bFirst = 0;` |
|    82507 |  6942 | `						break;` |
|        - |  6943 | `					}` |
|   254216 |  6944 | `				}` |
|   295199 |  6945 | `			}` |
|        - |  6946 | `		}` |
|        - |  6947 | `		/* 3) null suffix */` |
|    98403 |  6948 | `		if( bNullable ){` |
|       19 |  6949 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|       19 |  6950 | `			SyBlobAppend(pBlob, "null", 4);` |
|        8 |  6951 | `		}` |
|        - |  6952 | `	}` |
|    49265 |  6953 | `}` |
|        - |  6954 |  |
|        - |  6955 | `/*` |
|        - |  6956 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|        - |  6957 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|        - |  6958 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|        - |  6959 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|        - |  6960 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|        - |  6961 | ` * whether it was parenthesized.` |
|        - |  6962 | ` *` |
|        - |  6963 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|        - |  6964 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|        - |  6965 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|        - |  6966 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|        - |  6967 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|        - |  6968 | ` */` |
|    98672 |  6969 | `static sxi32 GenStateParsePart(` |
|        - |  6970 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|        - |  6971 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|        5 |  6972 | `{` |
|        - |  6973 | `	sxi32 rc;` |
|    98677 |  6974 | `	int nMembers = 0;` |
|    98677 |  6975 | `	int bParen = 0;` |
|    98677 |  6976 | `	*pnMembers = 0;` |
|    98677 |  6977 | `	*pbParen = 0;` |
|    98677 |  6978 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        9 |  6979 | `		bParen = 1;` |
|        9 |  6980 | `		pGen->pIn++; /* skip '(' */` |
|        3 |  6981 | `	}` |
|    49336 |  6982 | `	for(;;){` |
|    98703 |  6983 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|      ! 0 |  6984 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  6985 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|      ! 0 |  6986 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  6987 | `		}` |
|    98703 |  6988 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|    98703 |  6989 | `		if( rc != SXRET_OK ){` |
|        3 |  6990 | `			return rc;` |
|        - |  6991 | `		}` |
|    98701 |  6992 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|    98701 |  6993 | `		(*pnAtoms)++;` |
|    98701 |  6994 | `		nMembers++;` |
|        - |  6995 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|    98701 |  6996 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       39 |  6997 | `			SyToken *pNext = &pGen->pIn[1];` |
|       34 |  6998 | `			if( pNext < pGen->pEnd` |
|       39 |  6999 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       31 |  7000 | `				pGen->pIn++; /* skip '&' */` |
|       31 |  7001 | `				continue;` |
|        - |  7002 | `			}` |
|        4 |  7003 | `		}` |
|    98675 |  7004 | `		break;` |
|      ! 0 |  7005 | `	}` |
|    98675 |  7006 | `	if( bParen ){` |
|        9 |  7007 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 |  7008 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7009 | `				"Malformed DNF type: expecting ')'");` |
|      ! 0 |  7010 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  7011 | `		}` |
|        9 |  7012 | `		pGen->pIn++; /* skip ')' */` |
|        9 |  7013 | `		if( nMembers < 2 ){` |
|      ! 0 |  7014 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7015 | `				"Parenthesized type must be an intersection of at least two types");` |
|      ! 0 |  7016 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  7017 | `		}` |
|        3 |  7018 | `	}` |
|    98675 |  7019 | `	*pnMembers = nMembers;` |
|    98675 |  7020 | `	*pbParen = bParen;` |
|    98675 |  7021 | `	return SXRET_OK;` |
|    49341 |  7022 | `}` |
|        - |  7023 |  |
|        - |  7024 | `/*` |
|        - |  7025 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|        - |  7026 | ` *` |
|        - |  7027 | ` * Outputs:` |
|        - |  7028 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|        - |  7029 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|        - |  7030 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|        - |  7031 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|        - |  7032 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|        - |  7033 | ` *     already be initialized by the caller (allocator set, etc).` |
|        - |  7034 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|        - |  7035 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|        - |  7036 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|        - |  7037 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|        - |  7038 | ` *` |
|        - |  7039 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|        - |  7040 | ` * SXERR_ABORT on fatal compile errors.` |
|        - |  7041 | ` */` |
|    98536 |  7042 | `static sxi32 GenStateParseUnionTypeDecl(` |
|        - |  7043 | `	ph7_gen_state *pGen,` |
|        - |  7044 | `	sxu32 *pnType,` |
|        - |  7045 | `	SyString *pClass,` |
|        - |  7046 | `	SySet *pAlts,` |
|        - |  7047 | `	sxi32 *piTypeFlags,` |
|        - |  7048 | `	SyString *pTypeText,` |
|        - |  7049 | `	int iNullableFlag,` |
|        - |  7050 | `	int iUnionFlag,` |
|        - |  7051 | `	int bAllowVoid,` |
|        - |  7052 | `	sxu32 nLine` |
|        5 |  7053 | `){` |
|        - |  7054 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|    98541 |  7055 | `	int nAtoms = 0;` |
|    98541 |  7056 | `	int bShortNullable = 0;` |
|    98541 |  7057 | `	int bExplicitNull = 0;` |
|        - |  7058 | `	sxi32 rc;` |
|    98541 |  7059 | `	*pnType = 0;` |
|    98541 |  7060 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|    98541 |  7061 | `	*piTypeFlags = 0;` |
|    98541 |  7062 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|        - |  7063 |  |
|    98541 |  7064 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  7065 | `		return SXRET_OK;` |
|        - |  7066 | `	}` |
|        - |  7067 | ``	/* Optional `?` shorthand prefix */`` |
|    98536 |  7068 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|       91 |  7069 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|       90 |  7070 | `		bShortNullable = 1;` |
|       90 |  7071 | `		pGen->pIn++;` |
|       90 |  7072 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  7073 | `			return SXERR_SYNTAX;` |
|        - |  7074 | `		}` |
|       43 |  7075 | `	}` |
|        - |  7076 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|        - |  7077 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|        - |  7078 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|        - |  7079 | `	{` |
|        - |  7080 | `		int nMembers, bParen;` |
|    98541 |  7081 | `		sxu32 iGroup = 0;` |
|    98541 |  7082 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|    98541 |  7083 | `		if( rc != SXRET_OK ){` |
|        4 |  7084 | `			return rc;` |
|        - |  7085 | `		}` |
|        - |  7086 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|        - |  7087 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|        - |  7088 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|        - |  7089 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|        - |  7090 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|   148010 |  7091 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|    98748 |  7092 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      143 |  7093 | `			if( bShortNullable ){` |
|        - |  7094 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|        - |  7095 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|        - |  7096 | `				 * already reported" so callers skip their own error emission. */` |
|        3 |  7097 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|        - |  7098 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|        3 |  7099 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|        - |  7100 | `			}` |
|      141 |  7101 | `			if( nMembers >= 2 && !bParen ){` |
|      ! 0 |  7102 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|        - |  7103 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|      ! 0 |  7104 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  7105 | `			}` |
|      141 |  7106 | ``			pGen->pIn++; /* skip `\|` */`` |
|      141 |  7107 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|      141 |  7108 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  7109 | `				return rc;` |
|        - |  7110 | `			}` |
|        5 |  7111 | `		}` |
|    98537 |  7112 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|      ! 0 |  7113 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7114 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|      ! 0 |  7115 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  7116 | `		}` |
|        - |  7117 | `	}` |
|        - |  7118 | `	/* Validation pass.` |
|        - |  7119 | `	 *` |
|        - |  7120 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|        - |  7121 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|        - |  7122 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|        - |  7123 | `	 */` |
|        - |  7124 | `	{` |
|        - |  7125 | `		int i, j;` |
|    98537 |  7126 | `		int bHasNonNull = 0;` |
|    98537 |  7127 | `		int bAnyIntersection = 0;` |
|        - |  7128 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|        - |  7129 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|        - |  7130 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
|  3251561 |  7131 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|   197231 |  7132 | `		for( i = 0; i < nAtoms; i++ ){` |
|    98699 |  7133 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|    49352 |  7134 | `		}` |
|   197175 |  7135 | `		for( i = 0; i < nAtoms; i++ ){` |
|    98669 |  7136 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|    49324 |  7137 | `		}` |
|        - |  7138 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|        - |  7139 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|    98537 |  7140 | `		if( bShortNullable && bAnyIntersection ){` |
|      ! 0 |  7141 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7142 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|      ! 0 |  7143 | `			return SXERR_SYNTAX;` |
|        - |  7144 | `		}` |
|   197217 |  7145 | `		for( i = 0; i < nAtoms; i++ ){` |
|        - |  7146 | `			/* Intersection members must be class/interface types (PHP rejects` |
|        - |  7147 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|        - |  7148 | ``			 * `true`/`false` in an intersection). */`` |
|    98697 |  7149 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|       55 |  7150 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|       55 |  7151 | `				if( bClassLike ){` |
|       53 |  7152 | `					SyString *pC = &aAtoms[i].sClass;` |
|       48 |  7153 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|       48 |  7154 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|       48 |  7155 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|       53 |  7156 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|      ! 0 |  7157 | `						bClassLike = 0;` |
|      ! 0 |  7158 | `					}` |
|       24 |  7159 | `				}` |
|       55 |  7160 | `				if( !bClassLike ){` |
|        - |  7161 | `					const char *zName; sxu32 nName;` |
|        3 |  7162 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      ! 0 |  7163 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|      ! 0 |  7164 | `					}else{` |
|        3 |  7165 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|        - |  7166 | `					}` |
|        4 |  7167 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7168 | `						"Type %.*s cannot be part of an intersection type",` |
|        1 |  7169 | `						(int)nName, zName);` |
|        3 |  7170 | `					return SXERR_SYNTAX;` |
|        - |  7171 | `				}` |
|       24 |  7172 | `			}` |
|    98695 |  7173 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|      177 |  7174 | `				if( nAtoms > 1 ){` |
|        3 |  7175 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7176 | `						"Void can only be used as a standalone type");` |
|        3 |  7177 | `					return SXERR_SYNTAX;` |
|        - |  7178 | `				}` |
|      175 |  7179 | `				if( !bAllowVoid ){` |
|      ! 0 |  7180 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7181 | `						"void cannot be used here");` |
|      ! 0 |  7182 | `					return SXERR_SYNTAX;` |
|        - |  7183 | `				}` |
|      175 |  7184 | `				if( bShortNullable ){` |
|      ! 0 |  7185 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7186 | `						"Void type cannot be nullable");` |
|      ! 0 |  7187 | `					return SXERR_SYNTAX;` |
|        - |  7188 | `				}` |
|       85 |  7189 | `			}` |
|    98693 |  7190 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|        - |  7191 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|        - |  7192 | `				 * type (never = the function does not return). Mirrors the void` |
|        - |  7193 | `				 * validation above; accepted here and enforced at compile time` |
|        - |  7194 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|       26 |  7195 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|        - |  7196 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|        - |  7197 | `					 * same as any other non-standalone use. */` |
|        5 |  7198 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7199 | `						"never can only be used as a standalone type");` |
|        5 |  7200 | `					return SXERR_SYNTAX;` |
|        - |  7201 | `				}` |
|       21 |  7202 | `				if( !bAllowVoid ){` |
|        - |  7203 | `					/* Return-only: params call with bAllowVoid=0. */` |
|        3 |  7204 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7205 | `						"never cannot be used as a parameter type");` |
|        3 |  7206 | `					return SXERR_SYNTAX;` |
|        - |  7207 | `				}` |
|        8 |  7208 | `			}` |
|    98687 |  7209 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|       34 |  7210 | `				bExplicitNull = 1;` |
|       19 |  7211 | `			}else{` |
|    98657 |  7212 | `				bHasNonNull = 1;` |
|        - |  7213 | `			}` |
|        - |  7214 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|        - |  7215 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|        - |  7216 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|        - |  7217 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|        - |  7218 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|    98887 |  7219 | `			for( j = 0; j < i; j++ ){` |
|      207 |  7220 | `				int bDup = 0;` |
|      207 |  7221 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|      395 |  7222 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|      202 |  7223 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|      207 |  7224 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|      195 |  7225 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|       51 |  7226 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       44 |  7227 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|       44 |  7228 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|       17 |  7229 | `								aAtoms[j].sClass.zString,` |
|       34 |  7230 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|      ! 0 |  7231 | `							bDup = 1;` |
|      ! 0 |  7232 | `						}` |
|       27 |  7233 | `					}else{` |
|        3 |  7234 | `						bDup = 1;` |
|        - |  7235 | `					}` |
|       23 |  7236 | `				}` |
|      195 |  7237 | `				if( bDup ){` |
|        - |  7238 | `					const char *zName;` |
|        - |  7239 | `					sxu32 nName;` |
|        3 |  7240 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      ! 0 |  7241 | `						zName = aAtoms[i].sClass.zString;` |
|      ! 0 |  7242 | `						nName = aAtoms[i].sClass.nByte;` |
|      ! 0 |  7243 | `					}else{` |
|        3 |  7244 | `						zName = aAtoms[i].zCanon;` |
|        3 |  7245 | `						nName = aAtoms[i].nCanon;` |
|        - |  7246 | `					}` |
|        4 |  7247 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        1 |  7248 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|        3 |  7249 | `					return SXERR_SYNTAX;` |
|        - |  7250 | `				}` |
|       99 |  7251 | `			}` |
|    49345 |  7252 | `		}` |
|    98525 |  7253 | `		if( !bHasNonNull && bExplicitNull ){` |
|        7 |  7254 | `			if( bShortNullable ){` |
|        - |  7255 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|      ! 0 |  7256 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7257 | `					"Null can not be used as a standalone type");` |
|      ! 0 |  7258 | `				return SXERR_SYNTAX;` |
|        - |  7259 | `			}` |
|        - |  7260 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|        - |  7261 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|        - |  7262 | `			 * path below leaves *pnType untouched when there is no non-null` |
|        - |  7263 | `			 * atom, so set it here. */` |
|        7 |  7264 | `			*pnType = MEMOBJ_NULL;` |
|        3 |  7265 | `		}` |
|        - |  7266 | `	}` |
|        - |  7267 | `	/* Compute nullability flag */` |
|    98525 |  7268 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      118 |  7269 | `		*piTypeFlags \|= iNullableFlag;` |
|       57 |  7270 | `	}` |
|        - |  7271 | `	/* Build canonical type text */` |
|    98525 |  7272 | `	if( pTypeText ){` |
|        - |  7273 | `		SyBlob sBlob;` |
|    98525 |  7274 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|   147743 |  7275 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|    49260 |  7276 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|    98525 |  7277 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|   147506 |  7278 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    98334 |  7279 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|    98339 |  7280 | `			if( zDup ){` |
|    98339 |  7281 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|    49167 |  7282 | `			}` |
|    49167 |  7283 | `		}` |
|    98525 |  7284 | `		SyBlobRelease(&sBlob);` |
|    49260 |  7285 | `	}` |
|        - |  7286 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|        - |  7287 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|        - |  7288 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|        - |  7289 | `	{` |
|    98525 |  7290 | `		int nNonNull = 0;` |
|    98525 |  7291 | `		int iNonNullIdx = -1;` |
|        - |  7292 | `		int i;` |
|   197197 |  7293 | `		for( i = 0; i < nAtoms; i++ ){` |
|    98677 |  7294 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    98647 |  7295 | `				nNonNull++;` |
|    98647 |  7296 | `				iNonNullIdx = i;` |
|    49321 |  7297 | `			}` |
|    49341 |  7298 | `		}` |
|    98525 |  7299 | `		if( nNonNull <= 1 ){` |
|        - |  7300 | `			/* Fast path: store as single type. */` |
|    98419 |  7301 | `			if( iNonNullIdx >= 0 ){` |
|    98413 |  7302 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|    98413 |  7303 | `				if( pA->nType == SXU32_HIGH ){` |
|    23672 |  7304 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     7889 |  7305 | `						pA->sClass.zString, pA->sClass.nByte);` |
|    15783 |  7306 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|    15783 |  7307 | `					*pnType = SXU32_HIGH;` |
|    15783 |  7308 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|    90524 |  7309 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|      175 |  7310 | `					*pnType = MEMOBJ_VOID;` |
|    82550 |  7311 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|       18 |  7312 | `					*pnType = MEMOBJ_NEVER;` |
|       10 |  7313 | `				}else{` |
|    82449 |  7314 | `					*pnType = pA->nType;` |
|        - |  7315 | `				}` |
|    49204 |  7316 | `			}` |
|    49212 |  7317 | `		}else{` |
|        - |  7318 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      111 |  7319 | `			*piTypeFlags \|= iUnionFlag;` |
|      355 |  7320 | `			for( i = 0; i < nAtoms; i++ ){` |
|        - |  7321 | `				ph7_type_alt sAlt;` |
|      249 |  7322 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      239 |  7323 | `				SyZero(&sAlt, sizeof(sAlt));` |
|      239 |  7324 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|      239 |  7325 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      146 |  7326 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       47 |  7327 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       99 |  7328 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|       99 |  7329 | `					sAlt.nType = SXU32_HIGH;` |
|       99 |  7330 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|       52 |  7331 | `				}else{` |
|      145 |  7332 | `					sAlt.nType = aAtoms[i].nType;` |
|      145 |  7333 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|        - |  7334 | `				}` |
|      239 |  7335 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|      122 |  7336 | `			}` |
|        - |  7337 | `		}` |
|        - |  7338 | `	}` |
|    98525 |  7339 | `	return SXRET_OK;` |
|    49273 |  7340 | `}` |
|        - |  7341 |  |
|        - |  7342 | `/*` |
|        - |  7343 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|        - |  7344 | `` * pGen->pIn should point to the token after `)`.`` |
|        - |  7345 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|        - |  7346 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|        - |  7347 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|        - |  7348 | `` *          and union types `: T\|U`.`` |
|        - |  7349 | ` */` |
|  1514600 |  7350 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|        5 |  7351 | `{` |
|  1514605 |  7352 | `	sxi32 iFlags = 0;` |
|        - |  7353 | `	sxi32 rc;` |
|        - |  7354 | `	sxu32 nLine;` |
|  1514605 |  7355 | `	pFunc->nReturnType = 0;` |
|  1514605 |  7356 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  1514605 |  7357 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|        - |  7358 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|        - |  7359 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|        - |  7360 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|        - |  7361 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|        - |  7362 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|  1514605 |  7363 | `	SySetReset(&pFunc->aReturnUnion);` |
|  1514605 |  7364 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|  1514605 |  7365 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  1513953 |  7366 | `		return SXRET_OK;` |
|        - |  7367 | `	}` |
|      657 |  7368 | `	pGen->pIn++; /* Skip ':' */` |
|      657 |  7369 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  7370 | `		return SXRET_OK;` |
|        - |  7371 | `	}` |
|      657 |  7372 | `	nLine = pGen->pIn->nLine;` |
|      657 |  7373 | `	rc = GenStateParseUnionTypeDecl(` |
|      326 |  7374 | `		pGen,` |
|      326 |  7375 | `		&pFunc->nReturnType,` |
|      326 |  7376 | `		&pFunc->sReturnClass,` |
|      326 |  7377 | `		&pFunc->aReturnUnion,` |
|        - |  7378 | `		&iFlags,` |
|      326 |  7379 | `		&pFunc->sReturnTypeName,` |
|        - |  7380 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|        - |  7381 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|        - |  7382 | `		/* iUnionFlag */ 0,` |
|        - |  7383 | `		/* bAllowVoid */ 1,` |
|      326 |  7384 | `		nLine);` |
|      657 |  7385 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  7386 | `		return SXERR_ABORT;` |
|        - |  7387 | `	}` |
|      657 |  7388 | `	if( rc == SXERR_CORRUPT ){` |
|        - |  7389 | `		/* Error already reported */` |
|      ! 0 |  7390 | `		return SXERR_SYNTAX;` |
|        - |  7391 | `	}` |
|      657 |  7392 | `	if( rc == SXERR_SYNTAX ){` |
|        8 |  7393 | `		if( pGen->pIn < pGen->pEnd ){` |
|       11 |  7394 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|        - |  7395 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|        6 |  7396 | `				&pGen->pIn->sData);` |
|        5 |  7397 | `		}else{` |
|      ! 0 |  7398 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|        - |  7399 | `				"syntax error, unexpected end of file in return type declaration");` |
|        - |  7400 | `		}` |
|        8 |  7401 | `		return SXERR_SYNTAX;` |
|        - |  7402 | `	}` |
|      651 |  7403 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|      651 |  7404 | `	return SXRET_OK;` |
|   757305 |  7405 | `}` |
|        - |  7406 |  |
|   118436 |  7407 | `static sxi32 GenStateCompileFunc(` |
|        - |  7408 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  7409 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|        - |  7410 | `	sxi32 iFlags,        /* Control flags */` |
|        - |  7411 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|        - |  7412 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|        - |  7413 | `	)` |
|        5 |  7414 | `{` |
|        - |  7415 | `	ph7_vm_func *pFunc;` |
|        - |  7416 | `	SyToken *pEnd;` |
|        - |  7417 | `	sxu32 nLine;` |
|        - |  7418 | `	char *zName;` |
|        - |  7419 | `	sxi32 rc;` |
|        - |  7420 | `	/* Extract line number */` |
|   118441 |  7421 | `	nLine = pGen->pIn->nLine;` |
|        - |  7422 | `	/* Jump the left parenthesis '(' */` |
|   118441 |  7423 | `	pGen->pIn++;` |
|        - |  7424 | `	/* Delimit the function signature */` |
|   118441 |  7425 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   118441 |  7426 | `	if( pEnd >= pGen->pEnd ){` |
|        - |  7427 | `		/* Syntax error */` |
|        8 |  7428 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|        8 |  7429 | `		if( rc == SXERR_ABORT ){` |
|        - |  7430 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7431 | `			return SXERR_ABORT;` |
|        - |  7432 | `		}` |
|        8 |  7433 | `		pGen->pIn = pGen->pEnd;` |
|        8 |  7434 | `		return SXRET_OK;` |
|        - |  7435 | `	}` |
|        - |  7436 | `	/* Create the function state */` |
|   118435 |  7437 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   118435 |  7438 | `	if( pFunc == 0 ){` |
|      ! 0 |  7439 | `		goto OutOfMem;` |
|        - |  7440 | `	}` |
|        - |  7441 | `	/* Build the function name, prepending namespace if active */` |
|   118442 |  7442 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|        - |  7443 | `		SyBlob sFQN;` |
|        - |  7444 | `		sxu32 nLen;` |
|       16 |  7445 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       16 |  7446 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       16 |  7447 | `		SyBlobAppend(&sFQN,"\\",1);` |
|       16 |  7448 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|       16 |  7449 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|       16 |  7450 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|       16 |  7451 | `		SyBlobRelease(&sFQN);` |
|       16 |  7452 | `		if( zName == 0 ){` |
|      ! 0 |  7453 | `			goto OutOfMem;` |
|        - |  7454 | `		}` |
|       16 |  7455 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|        9 |  7456 | `	}else{` |
|   118421 |  7457 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   118421 |  7458 | `		if( zName == 0 ){` |
|      ! 0 |  7459 | `			goto OutOfMem;` |
|        - |  7460 | `		}` |
|   118421 |  7461 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|        - |  7462 | `	}` |
|        - |  7463 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|        - |  7464 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|   118435 |  7465 | `	pFunc->nLine = nLine;` |
|   118435 |  7466 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|   118435 |  7467 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  7468 | `		return SXERR_ABORT;` |
|        - |  7469 | `	}` |
|   118435 |  7470 | `	if( pGen->pIn < pEnd ){` |
|        - |  7471 | `		/* Collect function arguments */` |
|   102077 |  7472 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   102077 |  7473 | `		if( rc == SXERR_ABORT ){` |
|        - |  7474 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  7475 | `			return SXERR_ABORT;` |
|        - |  7476 | `		}` |
|    51036 |  7477 | `	}` |
|        - |  7478 | `	/* Point past ')' and parse optional return type ': type' */` |
|   118435 |  7479 | `	pGen->pIn = &pEnd[1];` |
|        - |  7480 | `	{` |
|   118435 |  7481 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   118435 |  7482 | `		if( rcRt == SXERR_ABORT ){` |
|      ! 0 |  7483 | `			return SXERR_ABORT;` |
|   118435 |  7484 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|        8 |  7485 | `			return SXERR_SYNTAX;` |
|        - |  7486 | `		}` |
|        - |  7487 | `	}` |
|   118429 |  7488 | `	if( bHandleClosure ){` |
|        - |  7489 | `		ph7_vm_func_closure_env sEnv;` |
|      453 |  7490 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|      448 |  7491 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      270 |  7492 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|       87 |  7493 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |  7494 | `				/* Closure,record environment variable */` |
|       87 |  7495 | `				pGen->pIn++;` |
|       87 |  7496 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 |  7497 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|      ! 0 |  7498 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  7499 | `						return SXERR_ABORT;` |
|        - |  7500 | `					}` |
|      ! 0 |  7501 | `				}` |
|       87 |  7502 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|        - |  7503 | `				/* Compile until we hit the first closing parenthesis */` |
|      179 |  7504 | `				while( pGen->pIn < pGen->pEnd ){` |
|      179 |  7505 | `					int iFlagsLocal = 0;` |
|      179 |  7506 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|       87 |  7507 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|       87 |  7508 | `						break;` |
|        - |  7509 | `					}` |
|       97 |  7510 | `					nLineLocal = pGen->pIn->nLine;` |
|       97 |  7511 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|        - |  7512 | `						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry` |
|        - |  7513 | `						 * to the variable's memory slot instead of copying its value. */` |
|       53 |  7514 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|       53 |  7515 | `						pGen->pIn++;` |
|       26 |  7516 | `					}` |
|       92 |  7517 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|       97 |  7518 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  7519 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        - |  7520 | `								"Closure: Unexpected token. Expecting a variable name");` |
|      ! 0 |  7521 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 |  7522 | `								return SXERR_ABORT;` |
|        - |  7523 | `							}` |
|        - |  7524 | `							/* Find the closing parenthesis */` |
|      ! 0 |  7525 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 |  7526 | `								pGen->pIn++;` |
|      ! 0 |  7527 | `							}` |
|      ! 0 |  7528 | `							if(pGen->pIn < pGen->pEnd){` |
|      ! 0 |  7529 | `								pGen->pIn++;` |
|      ! 0 |  7530 | `							}` |
|      ! 0 |  7531 | `							break;` |
|        - |  7532 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|      ! 0 |  7533 | `					}else{` |
|        - |  7534 | `						SyString *pNameLocal;` |
|        - |  7535 | `						char *zDup;` |
|        - |  7536 | `						/* Duplicate variable name */` |
|       97 |  7537 | `						pNameLocal = &pGen->pIn[1].sData;` |
|       97 |  7538 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|       97 |  7539 | `						if( zDup ){` |
|        - |  7540 | `							/* Zero the structure */` |
|       97 |  7541 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       97 |  7542 | `							sEnv.iFlags = iFlagsLocal;` |
|       97 |  7543 | `							sEnv.nIdx = SXU32_HIGH;` |
|       97 |  7544 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       97 |  7545 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      112 |  7546 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|       30 |  7547 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|      ! 0 |  7548 | `									got_this = 1;` |
|      ! 0 |  7549 | `							}` |
|        - |  7550 | `							/* Save imported variable */` |
|       97 |  7551 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       51 |  7552 | `						}else{` |
|      ! 0 |  7553 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  7554 | `							 return SXERR_ABORT;` |
|        - |  7555 | `						}` |
|        - |  7556 | `					}` |
|       97 |  7557 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      109 |  7558 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  7559 | `						/* Ignore trailing commas */` |
|       13 |  7560 | `						pGen->pIn++;` |
|        1 |  7561 | `					}` |
|        5 |  7562 | `				}` |
|        - |  7563 | `				/* php 7.1+: the return type follows the use clause —` |
|        - |  7564 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|        - |  7565 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|        - |  7566 | `				 * so an unconditional call would wipe a type parsed at the` |
|        - |  7567 | `				 * legacy pre-use position. */` |
|       87 |  7568 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|        7 |  7569 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|        7 |  7570 | `					if( rcRt2 == SXERR_ABORT ){` |
|      ! 0 |  7571 | `						return SXERR_ABORT;` |
|        7 |  7572 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|      ! 0 |  7573 | `						return SXERR_SYNTAX;` |
|        - |  7574 | `					}` |
|        3 |  7575 | `				}` |
|       41 |  7576 | `		}` |
|      453 |  7577 | `		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){` |
|        - |  7578 | `			/* Make the $this variable [Current processed Object (class instance)]` |
|        - |  7579 | `			 * available to the closure environment — for EVERY non-static` |
|        - |  7580 | `			 * anonymous function, use list or not (php binds $this to any` |
|        - |  7581 | ``			 * closure declared in a method; pre-fix only `use (...)` closures`` |
|        - |  7582 | `			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of` |
|        - |  7583 | `			 * a global-scope closure is silently dropped at install. A static` |
|        - |  7584 | `			 * closure never binds $this (php). */` |
|      445 |  7585 | `			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      445 |  7586 | `			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      445 |  7587 | `			sEnv.nIdx = SXU32_HIGH;` |
|      445 |  7588 | `			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      445 |  7589 | `			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      445 |  7590 | `			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      220 |  7591 | `		}` |
|      453 |  7592 | `		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|        - |  7593 | `			/* Mark as closure */` |
|      447 |  7594 | `			pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|      221 |  7595 | `		}` |
|      224 |  7596 | `	}` |
|        - |  7597 | `	/* Compile the body */` |
|   118429 |  7598 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   118429 |  7599 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  7600 | `		return SXERR_ABORT;` |
|        - |  7601 | `	}` |
|        - |  7602 | `	/* The cursor sits just past the body's closing brace */` |
|   118429 |  7603 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|   118429 |  7604 | `	if( ppFunc ){` |
|   118429 |  7605 | `		*ppFunc = pFunc;` |
|    59212 |  7606 | `	}` |
|   118429 |  7607 | `	rc = SXRET_OK;` |
|   118429 |  7608 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|        - |  7609 | `		/* Finally register the function */` |
|   117987 |  7610 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|    58991 |  7611 | `	}` |
|   118429 |  7612 | `	if( rc == SXRET_OK ){` |
|   118429 |  7613 | `		return SXRET_OK;` |
|        - |  7614 | `	}` |
|        - |  7615 | `	/* Fall through if something goes wrong */` |
|      ! 0 |  7616 | `OutOfMem:` |
|        - |  7617 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  7618 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  7619 | `	 */` |
|      ! 0 |  7620 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 |  7621 | `	return SXERR_ABORT;` |
|    59223 |  7622 | `}` |
|        - |  7623 | `/*` |
|        - |  7624 | ` * Compile a standard PHP function.` |
|        - |  7625 | ` *  Refer to the block-comment above for more information.` |
|        - |  7626 | ` */` |
|   117996 |  7627 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|        5 |  7628 | `{` |
|        - |  7629 | `	SyString *pName;` |
|        - |  7630 | `	sxi32 iFlags;` |
|        - |  7631 | `	sxu32 nKwLine;` |
|        - |  7632 | `	sxu32 nLine;` |
|        - |  7633 | `	sxi32 rc;` |
|        - |  7634 |  |
|   118001 |  7635 | `	nLine = pGen->pIn->nLine;` |
|   118001 |  7636 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|   118001 |  7637 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   118001 |  7638 | `	iFlags = 0;` |
|   118001 |  7639 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        - |  7640 | `		/* Return by reference,remember that */` |
|       12 |  7641 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|        - |  7642 | `		/* Jump the '&' token */` |
|       12 |  7643 | `		pGen->pIn++;` |
|        5 |  7644 | `	}` |
|   118001 |  7645 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  7646 | `		/* Invalid function name */` |
|        8 |  7647 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|        8 |  7648 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  7649 | `			return SXERR_ABORT;` |
|        - |  7650 | `		}` |
|        - |  7651 | `		/* Sychronize with the next semi-colon or braces*/` |
|       22 |  7652 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       16 |  7653 | `			pGen->pIn++;` |
|        2 |  7654 | `		}` |
|        8 |  7655 | `		return SXRET_OK;` |
|        - |  7656 | `	}` |
|   117995 |  7657 | `	pName = &pGen->pIn->sData;` |
|   117995 |  7658 | `	nLine = pGen->pIn->nLine;` |
|        - |  7659 | `	/* Jump the function name */` |
|   117995 |  7660 | `	pGen->pIn++;` |
|   117995 |  7661 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  7662 | `		/* Syntax error */` |
|        3 |  7663 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|        3 |  7664 | `		if( rc == SXERR_ABORT ){` |
|        - |  7665 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7666 | `			return SXERR_ABORT;` |
|        - |  7667 | `		}` |
|        - |  7668 | `		/* Sychronize with the next semi-colon or '{' */` |
|        3 |  7669 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  7670 | `			pGen->pIn++;` |
|      ! 0 |  7671 | `		}` |
|        3 |  7672 | `		return SXRET_OK;` |
|        - |  7673 | `	}` |
|        - |  7674 | `	/* Compile function body */` |
|        - |  7675 | `	{` |
|   117993 |  7676 | `		ph7_vm_func *pFuncState = 0;` |
|   117993 |  7677 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);` |
|   117993 |  7678 | `		if( pFuncState ){` |
|        - |  7679 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|   117981 |  7680 | `			pFuncState->nLine = nKwLine;` |
|    58988 |  7681 | `		}` |
|        - |  7682 | `	}` |
|   117993 |  7683 | `	return rc;` |
|    59003 |  7684 | `}` |
|        - |  7685 | `/*` |
|        - |  7686 | ` * Extract the visibility level associated with a given keyword.` |
|        - |  7687 | ` * According to the PHP language reference manual` |
|        - |  7688 | ` *  Visibility:` |
|        - |  7689 | ` *  The visibility of a property or method can be defined by prefixing` |
|        - |  7690 | ` *  the declaration with the keywords public, protected or private.` |
|        - |  7691 | ` *  Class members declared public can be accessed everywhere.` |
|        - |  7692 | ` *  Members declared protected can be accessed only within the class` |
|        - |  7693 | ` *  itself and by inherited and parent classes. Members declared as private` |
|        - |  7694 | ` *  may only be accessed by the class that defines the member.` |
|        - |  7695 | ` */` |
|  1750382 |  7696 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|        5 |  7697 | `{` |
|  1750387 |  7698 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    23469 |  7699 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  1726923 |  7700 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   182629 |  7701 | `		return PH7_CLASS_PROT_PROTECTED;` |
|        - |  7702 | `	}` |
|        - |  7703 | `	/* Assume public by default */` |
|  1544299 |  7704 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   875196 |  7705 | `}` |
|        - |  7706 | `/*` |
|        - |  7707 | ` * Compile a class constant.` |
|        - |  7708 | ` * According to the PHP language reference manual` |
|        - |  7709 | ` *  Class Constants` |
|        - |  7710 | ` *   It is possible to define constant values on a per-class basis remaining` |
|        - |  7711 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|        - |  7712 | ` *   you don't use the $ symbol to declare or use them.` |
|        - |  7713 | ` *   The value must be a constant expression, not (for example) a variable,` |
|        - |  7714 | ` *   a property, a result of a mathematical operation, or a function call.` |
|        - |  7715 | ` *   It's also possible for interfaces to have constants.` |
|        - |  7716 | ` * Symisc eXtension.` |
|        - |  7717 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|        - |  7718 | ` *  the zend engine would allow only simple scalar value.` |
|        - |  7719 | ` *  Example:` |
|        - |  7720 | ` *   class Test{` |
|        - |  7721 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - |  7722 | ` *   };` |
|        - |  7723 | ` *   var_dump(TEST::MyConst);` |
|        - |  7724 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - |  7725 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - |  7726 | ` */` |
|        - |  7727 | `/*` |
|        - |  7728 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|        - |  7729 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|        - |  7730 | ` * token immediately followed by '='. Anything else with a leading type token` |
|        - |  7731 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|        - |  7732 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|        - |  7733 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|        - |  7734 | ` */` |
|   143884 |  7735 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|        5 |  7736 | `{` |
|        - |  7737 | `	SyToken *p0, *p1;` |
|   143889 |  7738 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  7739 | `		return 0;` |
|        - |  7740 | `	}` |
|   143889 |  7741 | `	p0 = pGen->pIn;` |
|        - |  7742 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|   143889 |  7743 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|      ! 0 |  7744 | `		return 1;` |
|        - |  7745 | `	}` |
|   143889 |  7746 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|        5 |  7747 | `		return 1;` |
|        - |  7748 | `	}` |
|        - |  7749 | `	/* A name-like first token begins a type only when followed by another` |
|        - |  7750 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|        - |  7751 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|   143885 |  7752 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|   143885 |  7753 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|   143885 |  7754 | `		if( p1 ){` |
|   143885 |  7755 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|       34 |  7756 | `				return 1;` |
|        - |  7757 | `			}` |
|   143855 |  7758 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|        5 |  7759 | `				return 1;` |
|        - |  7760 | `			}` |
|    71923 |  7761 | `		}` |
|    71923 |  7762 | `	}` |
|   143851 |  7763 | `	return 0;` |
|    71947 |  7764 | `}` |
|        - |  7765 | `/*` |
|        - |  7766 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|        - |  7767 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|        - |  7768 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|        - |  7769 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|        - |  7770 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|        - |  7771 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|        - |  7772 | ` * Peek only; never consumes tokens.` |
|        - |  7773 | ` */` |
|       24 |  7774 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|        4 |  7775 | `{` |
|       28 |  7776 | `	SyToken *p = pGen->pIn;` |
|       39 |  7777 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|       20 |  7778 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|        3 |  7779 | `		p++; /* skip leading unary sign(s) */` |
|        1 |  7780 | `	}` |
|       28 |  7781 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|       23 |  7782 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|        - |  7783 | `	}` |
|        6 |  7784 | `	p++;` |
|        - |  7785 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|        6 |  7786 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|       16 |  7787 | `}` |
|        - |  7788 | `/*` |
|        - |  7789 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|        - |  7790 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|        - |  7791 | `` * `$o->new`), not a `new` expression.`` |
|        - |  7792 | ` */` |
|        6 |  7793 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|        3 |  7794 | `{` |
|        - |  7795 | `	sxi32 iOp;` |
|        9 |  7796 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|      ! 0 |  7797 | `		return 0;` |
|        - |  7798 | `	}` |
|        9 |  7799 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|        9 |  7800 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|        6 |  7801 | `}` |
|        - |  7802 | `/*` |
|        - |  7803 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|        - |  7804 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|        - |  7805 | ` * interface-constant and (instance/static) property-default initializers` |
|        - |  7806 | ` * ("New expressions are not supported in this context") while still allowing it` |
|        - |  7807 | ` * in global constants, parameter defaults and static-local initializers (which` |
|        - |  7808 | ` * are compiled by different functions and left untouched). The scan is` |
|        - |  7809 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|        - |  7810 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|        - |  7811 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|        - |  7812 | ` *` |
|        - |  7813 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|        - |  7814 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|        - |  7815 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|        - |  7816 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|        - |  7817 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|        - |  7818 | ` */` |
|   229938 |  7819 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|        5 |  7820 | `{` |
|   229943 |  7821 | `	SyToken *p = pGen->pIn;` |
|   229943 |  7822 | `	int iDepth = 0;` |
|   561929 |  7823 | `	while( p < pGen->pEnd ){` |
|   561927 |  7824 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|   229933 |  7825 | `			break; /* end of this initializer */` |
|        - |  7826 | `		}` |
|   331994 |  7827 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|   169903 |  7828 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|     7802 |  7829 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|        - |  7830 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|        - |  7831 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|        - |  7832 | `			 * expression. */` |
|        3 |  7833 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|        3 |  7834 | `			p++;` |
|        3 |  7835 | `			if( bArrow ){` |
|        - |  7836 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|        - |  7837 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|        3 |  7838 | `				int iBase = iDepth;` |
|       17 |  7839 | `				while( p < pGen->pEnd ){` |
|       17 |  7840 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        5 |  7841 | `						iDepth++;` |
|       15 |  7842 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        5 |  7843 | `						if( iDepth <= iBase ){` |
|      ! 0 |  7844 | `							break; /* closes an enclosing group, not the fn's own */` |
|        - |  7845 | `						}` |
|        5 |  7846 | `						iDepth--;` |
|       11 |  7847 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|        3 |  7848 | `						break;` |
|        - |  7849 | `					}` |
|       15 |  7850 | `					p++;` |
|        1 |  7851 | `				}` |
|        2 |  7852 | `			}else{` |
|        - |  7853 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|        - |  7854 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|        - |  7855 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|        - |  7856 | `				 * then skip the balanced brace block. */` |
|      ! 0 |  7857 | `				int iLocal = 0;` |
|      ! 0 |  7858 | `				while( p < pGen->pEnd ){` |
|      ! 0 |  7859 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|      ! 0 |  7860 | `						break; /* body brace */` |
|        - |  7861 | `					}` |
|      ! 0 |  7862 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      ! 0 |  7863 | `						iLocal++;` |
|      ! 0 |  7864 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      ! 0 |  7865 | `						if( iLocal > 0 ){` |
|      ! 0 |  7866 | `							iLocal--;` |
|      ! 0 |  7867 | `						}` |
|      ! 0 |  7868 | `					}` |
|      ! 0 |  7869 | `					p++;` |
|      ! 0 |  7870 | `				}` |
|      ! 0 |  7871 | `				if( p < pGen->pEnd ){` |
|      ! 0 |  7872 | `					int iBrace = 0; /* p is on the body '{' */` |
|      ! 0 |  7873 | `					while( p < pGen->pEnd ){` |
|      ! 0 |  7874 | `						if( p->nType & PH7_TK_OCB ){` |
|      ! 0 |  7875 | `							iBrace++;` |
|      ! 0 |  7876 | `						}else if( p->nType & PH7_TK_CCB ){` |
|      ! 0 |  7877 | `							iBrace--;` |
|      ! 0 |  7878 | `							if( iBrace == 0 ){` |
|      ! 0 |  7879 | `								p++;` |
|      ! 0 |  7880 | `								break;` |
|        - |  7881 | `							}` |
|      ! 0 |  7882 | `						}` |
|      ! 0 |  7883 | `						p++;` |
|      ! 0 |  7884 | `					}` |
|      ! 0 |  7885 | `				}` |
|        - |  7886 | `			}` |
|        3 |  7887 | `			continue;` |
|        - |  7888 | `		}` |
|   331997 |  7889 | `		if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     7853 |  7890 | `			iDepth++;` |
|   328073 |  7891 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     7851 |  7892 | `			if( iDepth > 0 ){` |
|     7851 |  7893 | `				iDepth--;` |
|     3923 |  7894 | `			}` |
|   320226 |  7895 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|    86171 |  7896 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|        - |  7897 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|        - |  7898 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|        - |  7899 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|       11 |  7900 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|       11 |  7901 | `				return 1;` |
|        - |  7902 | `			}` |
|      ! 0 |  7903 | `		}` |
|   331989 |  7904 | `		p++;` |
|        5 |  7905 | `	}` |
|   229935 |  7906 | `	return 0;` |
|   114974 |  7907 | `}` |
|        - |  7908 | `/*` |
|        - |  7909 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|        - |  7910 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|        - |  7911 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|        - |  7912 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|        - |  7913 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|        - |  7914 | ` * share the same backing.` |
|        - |  7915 | ` */` |
|      266 |  7916 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|        - |  7917 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|        5 |  7918 | `{` |
|      271 |  7919 | `	pAttr->nType = nType;` |
|      271 |  7920 | `	pAttr->sClass = *pClass;` |
|      271 |  7921 | `	pAttr->sTypeName = *pTypeName;` |
|      271 |  7922 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|        - |  7923 | `		sxu32 i;` |
|       73 |  7924 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|       51 |  7925 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|       51 |  7926 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|       28 |  7927 | `		}` |
|       11 |  7928 | `	}` |
|      271 |  7929 | `}` |
|   143884 |  7930 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 |  7931 | `{` |
|   143889 |  7932 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  7933 | `	SySet *pInstrContainer;` |
|        - |  7934 | `	ph7_class_attr *pCons;` |
|        - |  7935 | `	SyString *pName;` |
|        - |  7936 | `	sxi32 rc;` |
|   143889 |  7937 | `	sxu32 nType = 0;` |
|        - |  7938 | `	SyString sTypeClass;` |
|        - |  7939 | `	SyString sTypeText;` |
|        - |  7940 | `	SySet aUnionAlts;` |
|   143889 |  7941 | `	sxi32 iTypeFlags = 0;` |
|   143889 |  7942 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   143889 |  7943 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   143889 |  7944 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  7945 | `	/* Extract visibility level */` |
|   143889 |  7946 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - |  7947 | `	/* Mark as constant */` |
|   143889 |  7948 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|   143889 |  7949 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        - |  7950 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|        - |  7951 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|   143908 |  7952 | `	if( GenStateClassConstHasType(pGen) ){` |
|       61 |  7953 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|       38 |  7954 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|        - |  7955 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|        - |  7956 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|        - |  7957 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|        - |  7958 | `		 * and success paths release. */` |
|       42 |  7959 | `		if( rc == SXERR_CORRUPT ){` |
|        - |  7960 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|      ! 0 |  7961 | `			goto Synchronize;` |
|       42 |  7962 | `		}else if( rc == SXERR_ABORT ){` |
|      ! 0 |  7963 | `			return SXERR_ABORT;` |
|       42 |  7964 | `		}else if( rc != SXRET_OK ){` |
|      ! 0 |  7965 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  7966 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|      ! 0 |  7967 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  7968 | `				return SXERR_ABORT;` |
|        - |  7969 | `			}` |
|      ! 0 |  7970 | `			goto Synchronize;` |
|        - |  7971 | `		}` |
|       42 |  7972 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|       19 |  7973 | `	}` |
|    71942 |  7974 | `loop:` |
|   143891 |  7975 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|        - |  7976 | `		/* Invalid constant name */` |
|      ! 0 |  7977 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|      ! 0 |  7978 | `		if( rc == SXERR_ABORT ){` |
|        - |  7979 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7980 | `			return SXERR_ABORT;` |
|        - |  7981 | `		}` |
|      ! 0 |  7982 | `		goto Synchronize;` |
|        - |  7983 | `	}` |
|        - |  7984 | `	/* Peek constant name */` |
|   143891 |  7985 | `	pName = &pGen->pIn->sData;` |
|        - |  7986 | `	/* Make sure the constant name isn't reserved */` |
|   143891 |  7987 | `	if( GenStateIsReservedConstant(pName) ){` |
|        - |  7988 | `		/* Reserved constant name */` |
|      ! 0 |  7989 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|      ! 0 |  7990 | `		if( rc == SXERR_ABORT ){` |
|        - |  7991 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7992 | `			return SXERR_ABORT;` |
|        - |  7993 | `		}` |
|      ! 0 |  7994 | `		goto Synchronize;` |
|        - |  7995 | `	}` |
|        - |  7996 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|   143891 |  7997 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       61 |  7998 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|       38 |  7999 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       19 |  8000 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|       42 |  8001 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8002 | `			return SXERR_ABORT;` |
|       42 |  8003 | `		}else if( rc != SXRET_OK ){` |
|        3 |  8004 | `			goto Synchronize;` |
|        - |  8005 | `		}` |
|       18 |  8006 | `	}` |
|        - |  8007 | `	/* Advance the stream cursor */` |
|   143889 |  8008 | `	pGen->pIn++;` |
|   143889 |  8009 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|        - |  8010 | `		/* Invalid declaration */` |
|      ! 0 |  8011 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|      ! 0 |  8012 | `		if( rc == SXERR_ABORT ){` |
|        - |  8013 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8014 | `			return SXERR_ABORT;` |
|        - |  8015 | `		}` |
|      ! 0 |  8016 | `		goto Synchronize;` |
|        - |  8017 | `	}` |
|   143889 |  8018 | `	pGen->pIn++; /* Jump the equal sign */` |
|        - |  8019 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|        - |  8020 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|        - |  8021 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|        - |  8022 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|   143884 |  8023 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|       39 |  8024 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|        8 |  8025 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8026 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|        2 |  8027 | `			&pClass->sName,pName,&sTypeText);` |
|        6 |  8028 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8029 | `			return SXERR_ABORT;` |
|        - |  8030 | `		}` |
|        6 |  8031 | `		goto Synchronize;` |
|        - |  8032 | `	}` |
|        - |  8033 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|        - |  8034 | `	 * constant initializer ("New expressions are not supported in this context").` |
|        - |  8035 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|   143885 |  8036 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|        5 |  8037 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8038 | `			"New expressions are not supported in this context");` |
|        5 |  8039 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8040 | `			return SXERR_ABORT;` |
|        - |  8041 | `		}` |
|        5 |  8042 | `		goto Synchronize;` |
|        - |  8043 | `	}` |
|        - |  8044 | `	/* Allocate a new class attribute */` |
|   143881 |  8045 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   143881 |  8046 | `	if( pCons ){` |
|   143881 |  8047 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|   143881 |  8048 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  8049 | `			return SXERR_ABORT;` |
|        - |  8050 | `		}` |
|    71938 |  8051 | `	}` |
|   143881 |  8052 | `	if( pCons == 0 ){` |
|      ! 0 |  8053 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8054 | `		return SXERR_ABORT;` |
|        - |  8055 | `	}` |
|   143881 |  8056 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       35 |  8057 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       16 |  8058 | `	}` |
|        - |  8059 | `	/* Swap bytecode container */` |
|   143881 |  8060 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   143881 |  8061 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|        - |  8062 | `	/* Compile constant value.` |
|        - |  8063 | `	 */` |
|   143881 |  8064 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   143881 |  8065 | `	if( rc == SXERR_EMPTY ){` |
|        3 |  8066 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|        3 |  8067 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8068 | `			return SXERR_ABORT;` |
|        - |  8069 | `		}` |
|        1 |  8070 | `	}` |
|        - |  8071 | `	/* Emit the done instruction */` |
|   143881 |  8072 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   143881 |  8073 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   143881 |  8074 | `	if( rc == SXERR_ABORT ){` |
|        - |  8075 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  8076 | `		return SXERR_ABORT;` |
|        - |  8077 | `	}` |
|        - |  8078 | `	/* All done,install the constant */` |
|   143881 |  8079 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|   143881 |  8080 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8081 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8082 | `		return SXERR_ABORT;` |
|        - |  8083 | `	}` |
|   143881 |  8084 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  8085 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|        3 |  8086 | `		pGen->pIn++; /* Jump the comma */` |
|        3 |  8087 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 |  8088 | `			SyToken *pTok = pGen->pIn;` |
|      ! 0 |  8089 | `			if( pTok >= pGen->pEnd ){` |
|      ! 0 |  8090 | `				pTok--;` |
|      ! 0 |  8091 | `			}` |
|      ! 0 |  8092 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  8093 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|      ! 0 |  8094 | `				&pTok->sData,&pClass->sName);` |
|      ! 0 |  8095 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8096 | `				return SXERR_ABORT;` |
|        - |  8097 | `			}` |
|      ! 0 |  8098 | `		}else{` |
|        3 |  8099 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|        3 |  8100 | `				goto loop;` |
|        - |  8101 | `			}` |
|        - |  8102 | `		}` |
|      ! 0 |  8103 | `	}` |
|   143879 |  8104 | `	SySetRelease(&aUnionAlts);` |
|   143879 |  8105 | `	return SXRET_OK;` |
|        5 |  8106 | `Synchronize:` |
|       13 |  8107 | `	SySetRelease(&aUnionAlts);` |
|        - |  8108 | `	/* Synchronize with the first semi-colon */` |
|       45 |  8109 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       35 |  8110 | `		pGen->pIn++;` |
|        3 |  8111 | `	}` |
|       13 |  8112 | `	return SXERR_CORRUPT;` |
|    71947 |  8113 | `}` |
|        - |  8114 | `/*` |
|        - |  8115 | ` * complie a class attribute or Properties in the PHP jargon.` |
|        - |  8116 | ` * According to the PHP language reference manual` |
|        - |  8117 | ` *  Properties` |
|        - |  8118 | ` *  Class member variables are called "properties". You may also see them referred` |
|        - |  8119 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|        - |  8120 | ` *  of this reference we will use "properties". They are defined by using one` |
|        - |  8121 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|        - |  8122 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|        - |  8123 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|        - |  8124 | ` *  and must not depend on run-time information in order to be evaluated.` |
|        - |  8125 | ` * Symisc eXtension.` |
|        - |  8126 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|        - |  8127 | ` *  the zend engine would allow only simple scalar value.` |
|        - |  8128 | ` *  Example:` |
|        - |  8129 | ` *   class Test{` |
|        - |  8130 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - |  8131 | ` *   };` |
|        - |  8132 | ` *   var_dump(TEST::myVar);` |
|        - |  8133 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - |  8134 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - |  8135 | ` */` |
|        - |  8136 | `/*` |
|        - |  8137 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|        - |  8138 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|        - |  8139 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|        - |  8140 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|        - |  8141 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|        - |  8142 | ` */` |
|  1318180 |  8143 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|        5 |  8144 | `{` |
|  1318185 |  8145 | `	SyToken *p = pStart;` |
|  1318185 |  8146 | `	int bFirst = 1;` |
|  1318185 |  8147 | `	if( p >= pEnd ) return 0;` |
|        - |  8148 | ``	/* Optional nullable `?` shorthand. */`` |
|  1318185 |  8149 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|       25 |  8150 | `		p++;` |
|       25 |  8151 | `		if( p >= pEnd ) return 0;` |
|       11 |  8152 | `	}` |
|        - |  8153 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|        - |  8154 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|        - |  8155 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|        - |  8156 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   659090 |  8157 | `	for(;;){` |
|  1318205 |  8158 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|        - |  8159 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|        3 |  8160 | `			p++;` |
|        9 |  8161 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|        3 |  8162 | `			if( p >= pEnd ) return 0;` |
|        3 |  8163 | `			p++; /* skip ')' */` |
|        2 |  8164 | `		}else{` |
|        - |  8165 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|        - |  8166 | ``			 * then any `&`-joined intersection members. */`` |
|  1318203 |  8167 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|  1318203 |  8168 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  8169 | `				return 0;` |
|        - |  8170 | `			}` |
|        - |  8171 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|        - |  8172 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|        - |  8173 | `			 * may still appear at the initial dispatch site). */` |
|  1318203 |  8174 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|  1318155 |  8175 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  1318150 |  8176 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    23632 |  8177 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  1317947 |  8178 | `					return 0;` |
|        - |  8179 | `				}` |
|      104 |  8180 | `			}` |
|      261 |  8181 | `			p++;` |
|      263 |  8182 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  8183 | `				p += 2;` |
|        1 |  8184 | `			}` |
|      387 |  8185 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|      264 |  8186 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  8187 | `				p++; /* skip '&' */` |
|        3 |  8188 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|        3 |  8189 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|        3 |  8190 | `				p++;` |
|        3 |  8191 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      ! 0 |  8192 | `					p += 2;` |
|      ! 0 |  8193 | `				}` |
|        1 |  8194 | `			}` |
|        - |  8195 | `		}` |
|      263 |  8196 | `		bFirst = 0;` |
|      258 |  8197 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|       25 |  8198 | `			&& p->sData.zString[0] == '\|' ){` |
|       25 |  8199 | ``			p++; /* next `\|`-separated part */`` |
|       25 |  8200 | `			continue;` |
|        - |  8201 | `		}` |
|      243 |  8202 | `		break;` |
|      ! 0 |  8203 | `	}` |
|      243 |  8204 | `	if( p >= pEnd ) return 0;` |
|      243 |  8205 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   659095 |  8206 | `}` |
|        - |  8207 |  |
|        - |  8208 | `/*` |
|        - |  8209 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|        - |  8210 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|        - |  8211 | ` * if not). Recognized forms:` |
|        - |  8212 | ` *   ?Type, array, bool, int, float, string, object,` |
|        - |  8213 | ` *   self, parent, \Ns\ClassName, ClassName` |
|        - |  8214 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|        - |  8215 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|        - |  8216 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|        - |  8217 | ` * on unrecoverable error.` |
|        - |  8218 | ` *` |
|        - |  8219 | ` * When a type is parsed:` |
|        - |  8220 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|        - |  8221 | ` *   *pClass is set to the class name (for class types)` |
|        - |  8222 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|        - |  8223 | ` *   *pTypeText is set to the original text span of the type` |
|        - |  8224 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|        - |  8225 | ` */` |
|      238 |  8226 | `static sxi32 GenStateParsePropertyType(` |
|        - |  8227 | `	ph7_gen_state *pGen,` |
|        - |  8228 | `	sxu32 *pnType,` |
|        - |  8229 | `	SyString *pClass,` |
|        - |  8230 | `	sxi32 *piTypeFlags,` |
|        - |  8231 | `	SyString *pTypeText,` |
|        - |  8232 | `	SySet *pAlts` |
|        5 |  8233 | `){` |
|      243 |  8234 | `	sxi32 iFlags = 0;` |
|        - |  8235 | `	sxi32 rc;` |
|      243 |  8236 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  8237 | `		return SXRET_OK;` |
|        - |  8238 | `	}` |
|        - |  8239 | `	/* If the first token is '$', there's no type */` |
|      243 |  8240 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      ! 0 |  8241 | `		return SXRET_OK;` |
|        - |  8242 | `	}` |
|      243 |  8243 | `	rc = GenStateParseUnionTypeDecl(` |
|      119 |  8244 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|        - |  8245 | `		PH7_CLASS_ATTR_NULLABLE,` |
|        - |  8246 | `		PH7_CLASS_ATTR_UNION,` |
|        - |  8247 | `		/* bAllowVoid */ 0,` |
|      238 |  8248 | `		pGen->pIn->nLine);` |
|      243 |  8249 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8250 | `		return rc;` |
|        - |  8251 | `	}` |
|        - |  8252 | `	/* Verify next token is '$' (start of property name) */` |
|      243 |  8253 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  8254 | `		return SXERR_SYNTAX;` |
|        - |  8255 | `	}` |
|      243 |  8256 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|      243 |  8257 | `	return SXRET_OK;` |
|      124 |  8258 | `}` |
|        - |  8259 |  |
|        - |  8260 | `/*` |
|        - |  8261 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|        - |  8262 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|        - |  8263 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|        - |  8264 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|        - |  8265 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|        - |  8266 | ` * by the type parser itself before reaching here.` |
|        - |  8267 | ` *` |
|        - |  8268 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|        - |  8269 | ` * use in the error message.` |
|        - |  8270 | ` */` |
|      414 |  8271 | `static int GenStateIsDisallowedPropertyAtom(` |
|        - |  8272 | `	sxu32 nType,` |
|        - |  8273 | `	const SyString *pClass,` |
|        - |  8274 | `	const char **pzName,` |
|        - |  8275 | `	sxu32 *pnName)` |
|        5 |  8276 | `{` |
|        - |  8277 | `	const char *z;` |
|        - |  8278 | `	sxu32 n;` |
|      419 |  8279 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|      365 |  8280 | `		return 0;` |
|        - |  8281 | `	}` |
|       59 |  8282 | `	z = pClass->zString;` |
|       59 |  8283 | `	n = pClass->nByte;` |
|       59 |  8284 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|        8 |  8285 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|        - |  8286 | `	}` |
|        - |  8287 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|        - |  8288 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|        - |  8289 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|       52 |  8290 | `	return 0;` |
|      212 |  8291 | `}` |
|        - |  8292 |  |
|        - |  8293 | `/*` |
|        - |  8294 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|        - |  8295 | ` * constant) — the main atom plus any union alternatives — against the` |
|        - |  8296 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|        - |  8297 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|        - |  8298 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|        - |  8299 | ` * type T" vs "Class constant C::X cannot have type T").` |
|        - |  8300 | ` *` |
|        - |  8301 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|        - |  8302 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|        - |  8303 | ` */` |
|      352 |  8304 | `static sxi32 GenStateValidateMemberType(` |
|        - |  8305 | `	ph7_gen_state *pGen,` |
|        - |  8306 | `	ph7_class *pClass,` |
|        - |  8307 | `	const SyString *pMemberName,` |
|        - |  8308 | `	sxu32 nType,` |
|        - |  8309 | `	const SyString *pTypeClass,` |
|        - |  8310 | `	const SyString *pTypeText,` |
|        - |  8311 | `	SySet *pUnionAlts,` |
|        - |  8312 | `	const char *zErrFmt,` |
|        - |  8313 | `	sxu32 nLine)` |
|        5 |  8314 | `{` |
|      357 |  8315 | `	const char *zBad = 0;` |
|      357 |  8316 | `	sxu32 nBad = 0;` |
|        - |  8317 | `	SyString sFallback;` |
|        - |  8318 | `	const SyString *pBad;` |
|        - |  8319 | `	sxi32 rc;` |
|      357 |  8320 | `	int bDisallowed = 0;` |
|      357 |  8321 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|        5 |  8322 | `		bDisallowed = 1;` |
|      355 |  8323 | `	}else if( pUnionAlts ){` |
|        - |  8324 | `		sxu32 i;` |
|       95 |  8325 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|       67 |  8326 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|       67 |  8327 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|        3 |  8328 | `				bDisallowed = 1;` |
|        3 |  8329 | `				break;` |
|        - |  8330 | `			}` |
|       35 |  8331 | `		}` |
|       15 |  8332 | `	}` |
|      357 |  8333 | `	if( !bDisallowed ){` |
|      351 |  8334 | `		return SXRET_OK;` |
|        - |  8335 | `	}` |
|        - |  8336 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|        - |  8337 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|        - |  8338 | `	 * canonical spelling if the type text is unavailable. */` |
|        8 |  8339 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|        8 |  8340 | `		pBad = pTypeText;` |
|        5 |  8341 | `	}else{` |
|      ! 0 |  8342 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|      ! 0 |  8343 | `		pBad = &sFallback;` |
|        - |  8344 | `	}` |
|       11 |  8345 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        3 |  8346 | `		zErrFmt,` |
|        3 |  8347 | `		&pClass->sName,pMemberName,pBad);` |
|        8 |  8348 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  8349 | `		return SXERR_ABORT;` |
|        - |  8350 | `	}` |
|        8 |  8351 | `	return SXERR_SYNTAX;` |
|      181 |  8352 | `}` |
|        - |  8353 | `/*` |
|        - |  8354 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|        - |  8355 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|        - |  8356 | ` * matched as a plain identifier in the class-member modifier position rather` |
|        - |  8357 | ` * than promoted to a lexer keyword.` |
|        - |  8358 | ` */` |
| 10165906 |  8359 | `static int GenStateIsReadonly(SyToken *pTok)` |
|        5 |  8360 | `{` |
| 10207051 |  8361 | `	return (pTok->nType & PH7_TK_ID)` |
|  5124093 |  8362 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 10207046 |  8363 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|        5 |  8364 | `}` |
|        - |  8365 | `/*` |
|        - |  8366 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|        - |  8367 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|        - |  8368 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|        - |  8369 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|        - |  8370 | ` */` |
|  3750994 |  8371 | `static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|        5 |  8372 | `{` |
|  3750999 |  8373 | `	*pnTok = 0;` |
|  3750994 |  8374 | `	if( &pTok[3] < pEnd` |
|  3579264 |  8375 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|  3138722 |  8376 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|  1434963 |  8377 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|       16 |  8378 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|       16 |  8379 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|       21 |  8380 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|       17 |  8381 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|       17 |  8382 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|       17 |  8383 | `			*pnTok = 4;` |
|       17 |  8384 | `			return nKw;` |
|        - |  8385 | `		}` |
|      ! 0 |  8386 | `	}` |
|  3750983 |  8387 | `	return 0;` |
|  1875502 |  8388 | `}` |
|        - |  8389 | `/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */` |
|       16 |  8390 | `static sxi32 GenStateSetVisFlag(sxi32 nKw)` |
|        1 |  8391 | `{` |
|       17 |  8392 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|       13 |  8393 | `		return PH7_CLASS_ATTR_PRIVATE_SET;` |
|        - |  8394 | `	}` |
|        5 |  8395 | `	if( nKw == PH7_TKWRD_PROTECTED ){` |
|        3 |  8396 | `		return PH7_CLASS_ATTR_PROTECTED_SET;` |
|        - |  8397 | `	}` |
|        3 |  8398 | `	return PH7_CLASS_ATTR_PUBLIC_SET;` |
|        9 |  8399 | `}` |
|   210606 |  8400 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 |  8401 | `{` |
|   210611 |  8402 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  8403 | `	ph7_class_attr *pAttr;` |
|        - |  8404 | `	SyString *pName;` |
|        - |  8405 | `	sxi32 rc;` |
|   210611 |  8406 | `	sxu32 nType = 0;` |
|        - |  8407 | `	SyString sTypeClass;` |
|        - |  8408 | `	SyString sTypeText;` |
|        - |  8409 | `	SySet aUnionAlts;` |
|   210611 |  8410 | `	sxi32 iTypeFlags = 0;` |
|   210611 |  8411 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   210611 |  8412 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   210611 |  8413 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  8414 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|        - |  8415 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|        - |  8416 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   210611 |  8417 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|       21 |  8418 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|        9 |  8419 | `	}` |
|        - |  8420 | `	/* Extract visibility level */` |
|   210611 |  8421 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - |  8422 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   210730 |  8423 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      243 |  8424 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|      243 |  8425 | `		if( rc == SXERR_CORRUPT ){` |
|        - |  8426 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|      ! 0 |  8427 | `			goto Synchronize;` |
|      243 |  8428 | `		}else if( rc == SXERR_SYNTAX ){` |
|      ! 0 |  8429 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8430 | `				"Invalid property type or declaration near '%z'",` |
|      ! 0 |  8431 | `				&pGen->pIn->sData);` |
|      ! 0 |  8432 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8433 | `				return SXERR_ABORT;` |
|        - |  8434 | `			}` |
|      ! 0 |  8435 | `			goto Synchronize;` |
|      243 |  8436 | `		}else if( rc == SXERR_ABORT ){` |
|      ! 0 |  8437 | `			return SXERR_ABORT;` |
|        - |  8438 | `		}` |
|      119 |  8439 | `	}` |
|      ! 0 |  8440 | `loop:` |
|   210615 |  8441 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  8442 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|      ! 0 |  8443 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8444 | `			return SXERR_ABORT;` |
|        - |  8445 | `		}` |
|      ! 0 |  8446 | `		goto Synchronize;` |
|        - |  8447 | `	}` |
|   210615 |  8448 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   210615 |  8449 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        - |  8450 | `		/* Invalid attribute name */` |
|      ! 0 |  8451 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|      ! 0 |  8452 | `		if( rc == SXERR_ABORT ){` |
|        - |  8453 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8454 | `			return SXERR_ABORT;` |
|        - |  8455 | `		}` |
|      ! 0 |  8456 | `		goto Synchronize;` |
|        - |  8457 | `	}` |
|        - |  8458 | `	/* Peek attribute name */` |
|   210615 |  8459 | `	pName = &pGen->pIn->sData;` |
|        - |  8460 | `	/* Advance the stream cursor */` |
|   210615 |  8461 | `	pGen->pIn++;` |
|   210615 |  8462 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/\|PH7_TK_OCB/*'{' hooks*/)) == 0 ){` |
|        - |  8463 | `		/* Invalid declaration */` |
|        3 |  8464 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|        3 |  8465 | `		if( rc == SXERR_ABORT ){` |
|        - |  8466 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8467 | `			return SXERR_ABORT;` |
|        - |  8468 | `		}` |
|        3 |  8469 | `		goto Synchronize;` |
|        - |  8470 | `	}` |
|        - |  8471 | `	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and` |
|        - |  8472 | `	 * the read visibility must not be narrower than the set visibility. */` |
|   210613 |  8473 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
|       13 |  8474 | `		const char *zAvErr = 0;` |
|       19 |  8475 | `		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE` |
|       10 |  8476 | `			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED` |
|        2 |  8477 | `			: PH7_CLASS_PROT_PUBLIC;` |
|       13 |  8478 | `		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  8479 | `			zAvErr = "Property with asymmetric visibility %z::$%z must have type";` |
|       13 |  8480 | `		}else if( iProtection > iSetLevel ){` |
|      ! 0 |  8481 | `			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";` |
|      ! 0 |  8482 | `		}` |
|       13 |  8483 | `		if( zAvErr ){` |
|      ! 0 |  8484 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sName,pName);` |
|      ! 0 |  8485 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8486 | `				return SXERR_ABORT;` |
|        - |  8487 | `			}` |
|      ! 0 |  8488 | `			goto Synchronize;` |
|        - |  8489 | `		}` |
|        6 |  8490 | `	}` |
|        - |  8491 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|        - |  8492 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   210613 |  8493 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|       43 |  8494 | `		const char *zRoErr = 0;` |
|       43 |  8495 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        3 |  8496 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|       42 |  8497 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        6 |  8498 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|       39 |  8499 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|        6 |  8500 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|        2 |  8501 | `		}` |
|       43 |  8502 | `		if( zRoErr ){` |
|       13 |  8503 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|       13 |  8504 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8505 | `				return SXERR_ABORT;` |
|        - |  8506 | `			}` |
|       13 |  8507 | `			goto Synchronize;` |
|        - |  8508 | `		}` |
|       14 |  8509 | `	}` |
|        - |  8510 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|        - |  8511 | `	 * type atom or any union alternative. void/never are already rejected` |
|        - |  8512 | `	 * by the type parser. */` |
|   210603 |  8513 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      359 |  8514 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|        - |  8515 | `			&sTypeText,` |
|      236 |  8516 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      118 |  8517 | `			"Property %z::$%z cannot have type %z",nLine);` |
|      241 |  8518 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8519 | `			return SXERR_ABORT;` |
|      241 |  8520 | `		}else if( rc != SXRET_OK ){` |
|      ! 0 |  8521 | `			goto Synchronize;` |
|        - |  8522 | `		}` |
|      118 |  8523 | `	}` |
|        - |  8524 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   210603 |  8525 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|        4 |  8526 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  8527 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|        3 |  8528 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8529 | `			return SXERR_ABORT;` |
|        - |  8530 | `		}` |
|        3 |  8531 | `		goto Synchronize;` |
|        - |  8532 | `	}` |
|        - |  8533 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|        - |  8534 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|        - |  8535 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|        - |  8536 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|        - |  8537 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|        - |  8538 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|   210601 |  8539 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|        6 |  8540 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8541 | `			"New expressions are not supported in this context");` |
|        6 |  8542 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8543 | `			return SXERR_ABORT;` |
|        - |  8544 | `		}` |
|        6 |  8545 | `		goto Synchronize;` |
|        - |  8546 | `	}` |
|        - |  8547 | `	/* Allocate a new class attribute */` |
|   210597 |  8548 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   210597 |  8549 | `	if( pAttr ){` |
|   210597 |  8550 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|   210597 |  8551 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  8552 | `			return SXERR_ABORT;` |
|        - |  8553 | `		}` |
|   105296 |  8554 | `	}` |
|   210597 |  8555 | `	if( pAttr == 0 ){` |
|      ! 0 |  8556 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  8557 | `		return SXERR_ABORT;` |
|        - |  8558 | `	}` |
|   210597 |  8559 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      239 |  8560 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      117 |  8561 | `	}` |
|   210597 |  8562 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|        - |  8563 | `		SySet *pInstrContainer;` |
|    86059 |  8564 | `		SyToken *pSavedDefEnd = pGen->pEnd;` |
|    86059 |  8565 | `		pGen->pIn++; /*Jump the equal sign */` |
|        - |  8566 | `		{` |
|        - |  8567 | `			/* Delimit the default expression: it ends at the declaration's` |
|        - |  8568 | `			 * ';'/',' or at a top-level '{' opening a PHP 8.4 hook list` |
|        - |  8569 | ``			 * (`public string $w = "init" { get => …; }`) — the expression`` |
|        - |  8570 | `			 * compiler would otherwise run into the hook tokens. */` |
|    86059 |  8571 | `			SyToken *pScan = pGen->pIn;` |
|    86059 |  8572 | `			sxi32 iNest = 0;` |
|   187983 |  8573 | `			while( pScan < pGen->pEnd ){` |
|   187983 |  8574 | `				if( pScan->nType & (PH7_TK_LPAREN\|PH7_TK_OSB) ){` |
|     7845 |  8575 | `					iNest++;` |
|   184063 |  8576 | `				}else if( pScan->nType & (PH7_TK_RPAREN\|PH7_TK_CSB) ){` |
|     7845 |  8577 | `					iNest--;` |
|   176223 |  8578 | `				}else if( iNest <= 0 && (pScan->nType & (PH7_TK_SEMI\|PH7_TK_COMMA\|PH7_TK_OCB)) ){` |
|    86059 |  8579 | `					break;` |
|        - |  8580 | `				}` |
|   101929 |  8581 | `				pScan++;` |
|        5 |  8582 | `			}` |
|    86059 |  8583 | `			pGen->pEnd = pScan;` |
|        - |  8584 | `		}` |
|        - |  8585 | `		/* Swap bytecode container */` |
|    86059 |  8586 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    86059 |  8587 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|        - |  8588 | `		/* Compile attribute value.` |
|        - |  8589 | `		 */` |
|    86059 |  8590 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    86059 |  8591 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 |  8592 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|      ! 0 |  8593 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8594 | `				return SXERR_ABORT;` |
|        - |  8595 | `			}` |
|      ! 0 |  8596 | `		}` |
|        - |  8597 | `		/* Emit the done instruction */` |
|    86059 |  8598 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    86059 |  8599 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    86059 |  8600 | `		pGen->pIn = pGen->pEnd;   /* land exactly on the delimiter */` |
|    86059 |  8601 | `		pGen->pEnd = pSavedDefEnd;` |
|    43027 |  8602 | `	}` |
|        - |  8603 | `	/* All done,install the attribute */` |
|   210597 |  8604 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   210597 |  8605 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8606 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8607 | `		return SXERR_ABORT;` |
|        - |  8608 | `	}` |
|   210597 |  8609 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) ){` |
|        - |  8610 | ``		/* PHP 8.4 property hooks: `public [T] $x [= default] { get ...; set ...; }`.`` |
|        - |  8611 | `		 * The list ends the declaration at '}' — no trailing ';', no comma list. */` |
|       23 |  8612 | `		rc = GenStateCompilePropertyHooks(&(*pGen),pClass,pAttr);` |
|       23 |  8613 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8614 | `			return SXERR_ABORT;` |
|        - |  8615 | `		}` |
|       23 |  8616 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  8617 | `			goto Synchronize;` |
|        - |  8618 | `		}` |
|       23 |  8619 | `		SySetRelease(&aUnionAlts);` |
|       23 |  8620 | `		return SXRET_OK;` |
|        - |  8621 | `	}` |
|   210575 |  8622 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  8623 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|        5 |  8624 | `		pGen->pIn++; /* Jump the comma */` |
|        5 |  8625 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|      ! 0 |  8626 | `			SyToken *pTok = pGen->pIn;` |
|      ! 0 |  8627 | `			if( pTok >= pGen->pEnd ){` |
|      ! 0 |  8628 | `				pTok--;` |
|      ! 0 |  8629 | `			}` |
|      ! 0 |  8630 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  8631 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|      ! 0 |  8632 | `				&pTok->sData,&pClass->sName);` |
|      ! 0 |  8633 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8634 | `				return SXERR_ABORT;` |
|        - |  8635 | `			}` |
|      ! 0 |  8636 | `		}else{` |
|        5 |  8637 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        5 |  8638 | `				goto loop;` |
|        - |  8639 | `			}` |
|        - |  8640 | `		}` |
|      ! 0 |  8641 | `	}` |
|   210571 |  8642 | `	SySetRelease(&aUnionAlts);` |
|   210571 |  8643 | `	return SXRET_OK;` |
|        9 |  8644 | `Synchronize:` |
|        - |  8645 | `	/* Synchronize with the first semi-colon */` |
|       56 |  8646 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       37 |  8647 | `		pGen->pIn++;` |
|        3 |  8648 | `	}` |
|       22 |  8649 | `	SySetRelease(&aUnionAlts);` |
|       22 |  8650 | `	return SXERR_CORRUPT;` |
|   105308 |  8651 | `}` |
|        - |  8652 | `/*` |
|        - |  8653 | ` * Compile a class method.` |
|        - |  8654 | ` *` |
|        - |  8655 | ` * Refer to the official documentation for more information` |
|        - |  8656 | ` * on the powerful extension introduced by the PH7 engine` |
|        - |  8657 | ` * to the OO subsystem such as full type hinting,method` |
|        - |  8658 | ` * overloading and many more.` |
|        - |  8659 | ` */` |
|  1395892 |  8660 | `static sxi32 GenStateCompileClassMethod(` |
|        - |  8661 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  8662 | `	sxi32 iProtection,   /* Visibility level */` |
|        - |  8663 | `	sxi32 iFlags,        /* Configuration flags */` |
|        - |  8664 | `	int doBody,          /* TRUE to process method body */` |
|        - |  8665 | `	ph7_class *pClass    /* Class this method belongs */` |
|        - |  8666 | `	)` |
|        5 |  8667 | `{` |
|  1395897 |  8668 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  1395897 |  8669 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|        - |  8670 | `	ph7_class_method *pMeth;` |
|        - |  8671 | `	sxi32 iFuncFlags;` |
|        - |  8672 | `	SyString *pName;` |
|        - |  8673 | `	SyToken *pEnd;` |
|        - |  8674 | `	sxi32 rc;` |
|        - |  8675 | `	/* Extract visibility level */` |
|  1395897 |  8676 | `	iProtection = GetProtectionLevel(iProtection);` |
|  1395897 |  8677 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  1395897 |  8678 | `	iFuncFlags = 0;` |
|  1395897 |  8679 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  8680 | `		/* Invalid method name */` |
|      ! 0 |  8681 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|      ! 0 |  8682 | `		if( rc == SXERR_ABORT ){` |
|        - |  8683 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8684 | `			return SXERR_ABORT;` |
|        - |  8685 | `		}` |
|      ! 0 |  8686 | `		goto Synchronize;` |
|        - |  8687 | `	}` |
|  1395897 |  8688 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        - |  8689 | `		/* Return by reference,remember that */` |
|      ! 0 |  8690 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|        - |  8691 | `		/* Jump the '&' token */` |
|      ! 0 |  8692 | `		pGen->pIn++;` |
|      ! 0 |  8693 | `	}` |
|  1395897 |  8694 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  8695 | `		/* Invalid method name */` |
|      ! 0 |  8696 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|      ! 0 |  8697 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8698 | `			return SXERR_ABORT;` |
|        - |  8699 | `		}` |
|      ! 0 |  8700 | `		goto Synchronize;` |
|        - |  8701 | `	}` |
|        - |  8702 | `	/* Peek method name */` |
|  1395897 |  8703 | `	pName = &pGen->pIn->sData;` |
|  1395897 |  8704 | `	nLine = pGen->pIn->nLine;` |
|        - |  8705 | `	/* Jump the method name */` |
|  1395897 |  8706 | `	pGen->pIn++;` |
|  1395897 |  8707 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - |  8708 | `		/* Abstract method */` |
|   101051 |  8709 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|      ! 0 |  8710 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8711 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|      ! 0 |  8712 | `				&pClass->sName,pName);` |
|      ! 0 |  8713 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8714 | `				return SXERR_ABORT;` |
|        - |  8715 | `			}` |
|      ! 0 |  8716 | `		}` |
|        - |  8717 | `		/* Assemble method signature only */` |
|   101051 |  8718 | `		doBody = FALSE;` |
|    50523 |  8719 | `	}` |
|  1395897 |  8720 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  8721 | `		/* Syntax error */` |
|      ! 0 |  8722 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|      ! 0 |  8723 | `		if( rc == SXERR_ABORT ){` |
|        - |  8724 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8725 | `			return SXERR_ABORT;` |
|        - |  8726 | `		}` |
|      ! 0 |  8727 | `		goto Synchronize;` |
|        - |  8728 | `	}` |
|        - |  8729 | `	/* Allocate a new class_method instance */` |
|  1395897 |  8730 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  1395897 |  8731 | `	if( pMeth == 0 ){` |
|      ! 0 |  8732 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8733 | `		return SXERR_ABORT;` |
|        - |  8734 | `	}` |
|  1395897 |  8735 | `	pMeth->sFunc.nLine = nKwLine;` |
|  1395897 |  8736 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|  1395897 |  8737 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  8738 | `		return SXERR_ABORT;` |
|        - |  8739 | `	}` |
|        - |  8740 | `	/* Jump the left parenthesis '(' */` |
|  1395897 |  8741 | `	pGen->pIn++;` |
|  1395897 |  8742 | `	pEnd = 0; /* cc warning */` |
|        - |  8743 | `	/* Delimit the method signature */` |
|  1395897 |  8744 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  1395897 |  8745 | `	if( pEnd >= pGen->pEnd ){` |
|        - |  8746 | `		/* Syntax error */` |
|        3 |  8747 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|        3 |  8748 | `		if( rc == SXERR_ABORT ){` |
|        - |  8749 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8750 | `			return SXERR_ABORT;` |
|        - |  8751 | `		}` |
|        3 |  8752 | `		goto Synchronize;` |
|        - |  8753 | `	}` |
|        - |  8754 | `	{` |
|  1395895 |  8755 | `		int bIsCtor = 0;` |
|  1395895 |  8756 | `		int bAbstractCtor = 0;` |
|  1395890 |  8757 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|   814617 |  8758 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  1343357 |  8759 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   105081 |  8760 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        3 |  8761 | `				bAbstractCtor = 1;` |
|        2 |  8762 | `			}else{` |
|   105079 |  8763 | `				bIsCtor = 1;` |
|        - |  8764 | `			}` |
|    52538 |  8765 | `		}` |
|  1395895 |  8766 | `		if( pGen->pIn < pEnd ){` |
|        - |  8767 | `			/* Collect method arguments */` |
|   389061 |  8768 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   389061 |  8769 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8770 | `				return SXERR_ABORT;` |
|        - |  8771 | `			}` |
|   194528 |  8772 | `		}` |
|        - |  8773 | `	}` |
|        - |  8774 | `	/* Point past ')' and parse optional return type ': type' */` |
|  1395895 |  8775 | `	pGen->pIn = &pEnd[1];` |
|        - |  8776 | `	{` |
|  1395895 |  8777 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  1395895 |  8778 | `		if( rcRt == SXERR_ABORT ){` |
|      ! 0 |  8779 | `			return SXERR_ABORT;` |
|  1395895 |  8780 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|      ! 0 |  8781 | `			goto Synchronize;` |
|        - |  8782 | `		}` |
|        - |  8783 | `	}` |
|        - |  8784 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|        - |  8785 | `	 * property init/typecheck is handled by the generic typed-property path` |
|        - |  8786 | `	 * since we mint real ph7_class_attr entries. */` |
|        - |  8787 | `	{` |
|  1395895 |  8788 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|        - |  8789 | `		sxu32 i;` |
|  1979315 |  8790 | `		for( i = 0; i < nArg; i++ ){` |
|   583435 |  8791 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|        - |  8792 | `			ph7_class_attr *pAttr;` |
|   583435 |  8793 | `			sxi32 iAttrFlags = 0;` |
|        - |  8794 | `			int bArgTyped;` |
|   583435 |  8795 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   583351 |  8796 | `				continue;` |
|        - |  8797 | `			}` |
|        - |  8798 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|        - |  8799 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|        - |  8800 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|       59 |  8801 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|       90 |  8802 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|       89 |  8803 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        3 |  8804 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8805 | `					"Cannot declare variadic promoted property");` |
|        3 |  8806 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8807 | `					return SXERR_ABORT;` |
|        - |  8808 | `				}` |
|        3 |  8809 | `				goto Synchronize;` |
|        - |  8810 | `			}` |
|        - |  8811 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|        - |  8812 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|        - |  8813 | `			 * appear as an alternative of a union type. */` |
|       87 |  8814 | `			if( bArgTyped ){` |
|      122 |  8815 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|       78 |  8816 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|       78 |  8817 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|       39 |  8818 | `					"Property %z::$%z cannot have type %z",nLine);` |
|       83 |  8819 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8820 | `					return SXERR_ABORT;` |
|       83 |  8821 | `				}else if( rc != SXRET_OK ){` |
|        6 |  8822 | `					goto Synchronize;` |
|        - |  8823 | `				}` |
|       37 |  8824 | `			}` |
|        - |  8825 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|       83 |  8826 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|        4 |  8827 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  8828 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|        3 |  8829 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8830 | `					return SXERR_ABORT;` |
|        - |  8831 | `				}` |
|        3 |  8832 | `				goto Synchronize;` |
|        - |  8833 | `			}` |
|       81 |  8834 | `			if( bArgTyped ){` |
|       77 |  8835 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|       36 |  8836 | `			}` |
|       81 |  8837 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|        3 |  8838 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|        1 |  8839 | `			}` |
|       81 |  8840 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|        8 |  8841 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|        3 |  8842 | `			}` |
|       81 |  8843 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|        - |  8844 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|        - |  8845 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|       26 |  8846 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        4 |  8847 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  8848 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|        3 |  8849 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  8850 | `						return SXERR_ABORT;` |
|        - |  8851 | `					}` |
|        3 |  8852 | `					goto Synchronize;` |
|        - |  8853 | `				}` |
|       24 |  8854 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       10 |  8855 | `			}` |
|       79 |  8856 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|        - |  8857 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|        5 |  8858 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  8859 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8860 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|      ! 0 |  8861 | `						&pClass->sName,&pArg->sName);` |
|      ! 0 |  8862 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  8863 | `						return SXERR_ABORT;` |
|        - |  8864 | `					}` |
|      ! 0 |  8865 | `					goto Synchronize;` |
|        - |  8866 | `				}` |
|        5 |  8867 | `				iAttrFlags \|= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)` |
|        2 |  8868 | `					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;` |
|        2 |  8869 | `			}` |
|       79 |  8870 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|       79 |  8871 | `			if( pAttr == 0 ){` |
|      ! 0 |  8872 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8873 | `				return SXERR_ABORT;` |
|        - |  8874 | `			}` |
|       79 |  8875 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|       77 |  8876 | `				pAttr->nType = pArg->nType;` |
|       77 |  8877 | `				pAttr->sClass = pArg->sClass;` |
|       77 |  8878 | `				pAttr->sTypeName = pArg->sTypeName;` |
|       77 |  8879 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|        - |  8880 | `					sxu32 k;` |
|       20 |  8881 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|       14 |  8882 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|       14 |  8883 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|        8 |  8884 | `					}` |
|        3 |  8885 | `				}` |
|       36 |  8886 | `			}` |
|       79 |  8887 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|       79 |  8888 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  8889 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8890 | `				return SXERR_ABORT;` |
|        - |  8891 | `			}` |
|       42 |  8892 | `		}` |
|        - |  8893 | `	}` |
|  1395885 |  8894 | `	if( doBody ){` |
|        - |  8895 | `		/* Compile method body */` |
|  1294839 |  8896 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  1294839 |  8897 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8898 | `			return SXERR_ABORT;` |
|        - |  8899 | `		}` |
|        - |  8900 | `		/* The cursor sits just past the body's closing brace */` |
|  1294839 |  8901 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|   647422 |  8902 | `	}else{` |
|        - |  8903 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|   101051 |  8904 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|   101051 |  8905 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|    50523 |  8906 | `		}` |
|        - |  8907 | `		/* Only method signature is allowed */` |
|   101051 |  8908 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|      ! 0 |  8909 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  8910 | `				"Expected ';' after method signature '%z'",pName);` |
|      ! 0 |  8911 | `				if( rc == SXERR_ABORT ){` |
|        - |  8912 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 |  8913 | `					return SXERR_ABORT;` |
|        - |  8914 | `				}` |
|      ! 0 |  8915 | `				return SXERR_CORRUPT;` |
|        - |  8916 | `			}` |
|        - |  8917 | `	}` |
|        - |  8918 | `	/* All done,install the method */` |
|  1395885 |  8919 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  1395885 |  8920 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8921 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8922 | `		return SXERR_ABORT;` |
|        - |  8923 | `	}` |
|  1395885 |  8924 | `	return SXRET_OK;` |
|        6 |  8925 | `Synchronize:` |
|        - |  8926 | `	/* Synchronize with the first semi-colon */` |
|       40 |  8927 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       28 |  8928 | `		pGen->pIn++;` |
|        4 |  8929 | `	}` |
|       16 |  8930 | `	return SXERR_CORRUPT;` |
|   697951 |  8931 | `}` |
|        - |  8932 | `/*` |
|        - |  8933 | `` * Compile a PHP 8.4 property-hook list `{ get ...; set ...; }` following a`` |
|        - |  8934 | ` * property declaration. Each hook body is synthesized into a hidden public` |
|        - |  8935 | ` * class method (__phl_hook_get_NAME / __phl_hook_set_NAME) so inheritance,` |
|        - |  8936 | ` * $this binding, and dispatch ride the ordinary method machinery; OP_MEMBER /` |
|        - |  8937 | ` * OP_STORE route reads and plain writes through them (a per-instance guard` |
|        - |  8938 | ` * makes $this->NAME inside a hook body address the raw backing slot — php's` |
|        - |  8939 | `` * rule that hooks see the backing store). `get => expr;` compiles as an`` |
|        - |  8940 | `` * implicit return (the arrow-fn pattern); `set => expr;` compiles the same`` |
|        - |  8941 | ` * and is flagged VM_FUNC_HOOK_SET_EXPR — the dispatcher assigns its return` |
|        - |  8942 | `` * value to the backing slot. A `set` without a parameter list receives the`` |
|        - |  8943 | `` * implicit `$value` formal.`` |
|        - |  8944 | ` * On entry pGen->pIn sits on '{'; on success it sits just past '}'.` |
|        - |  8945 | ` */` |
|       22 |  8946 | `static sxi32 GenStateCompilePropertyHooks(ph7_gen_state *pGen,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 |  8947 | `{` |
|       23 |  8948 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  8949 | `	sxi32 rc;` |
|       23 |  8950 | `	pGen->pIn++; /* Jump '{' */` |
|       55 |  8951 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|        - |  8952 | `		char zHook[384];` |
|        - |  8953 | `		SyString sHookName;` |
|        - |  8954 | `		ph7_class_method *pMeth;` |
|        - |  8955 | `		int bGet;` |
|       33 |  8956 | `		sxu32 nHLine = pGen->pIn->nLine;` |
|       33 |  8957 | `		if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|      ! 0 |  8958 | `			pGen->pIn++; /* stray ';' between hooks */` |
|      ! 0 |  8959 | `			continue;` |
|        - |  8960 | `		}` |
|       33 |  8961 | `		if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|        - |  8962 | `			/* by-reference get hook: not modeled (loud, recorded) */` |
|      ! 0 |  8963 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nHLine,` |
|        - |  8964 | `				"By-reference property hooks are not supported for %z::$%z",` |
|      ! 0 |  8965 | `				&pClass->sName,&pAttr->sName);` |
|      ! 0 |  8966 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8967 | `				return SXERR_ABORT;` |
|        - |  8968 | `			}` |
|      ! 0 |  8969 | `			return SXERR_CORRUPT;` |
|        - |  8970 | `		}` |
|       33 |  8971 | `		if( (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  8972 | `			goto HookSyntax;` |
|        - |  8973 | `		}` |
|       32 |  8974 | `		if( pGen->pIn->sData.nByte == 3` |
|       33 |  8975 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"get",3) == 0 ){` |
|       17 |  8976 | `			bGet = 1;` |
|       25 |  8977 | `		}else if( pGen->pIn->sData.nByte == 3` |
|       17 |  8978 | `		 && SyStrnicmp(pGen->pIn->sData.zString,"set",3) == 0 ){` |
|       17 |  8979 | `			bGet = 0;` |
|        9 |  8980 | `		}else{` |
|      ! 0 |  8981 | `			goto HookSyntax;` |
|        - |  8982 | `		}` |
|       33 |  8983 | `		pGen->pIn++; /* Jump 'get'/'set' */` |
|       33 |  8984 | `		sHookName.zString = zHook;` |
|       49 |  8985 | `		sHookName.nByte = SyBufferFormat(zHook,sizeof(zHook),"__phl_hook_%s_%z",` |
|       16 |  8986 | `			bGet ? "get" : "set",&pAttr->sName);` |
|       33 |  8987 | `		pMeth = PH7_NewClassMethod(pGen->pVm,pClass,&sHookName,nHLine,` |
|        - |  8988 | `			PH7_CLASS_PROT_PUBLIC,0,0);` |
|       33 |  8989 | `		if( pMeth == 0 ){` |
|      ! 0 |  8990 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8991 | `			return SXERR_ABORT;` |
|        - |  8992 | `		}` |
|       33 |  8993 | `		pMeth->sFunc.nLine = nHLine;` |
|       33 |  8994 | `		if( !bGet ){` |
|        - |  8995 | ``			/* Parameter list: explicit `set(Type $v)` or the implicit `$value` */`` |
|       17 |  8996 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       15 |  8997 | `				SyToken *pRp = 0;` |
|       15 |  8998 | `				pGen->pIn++;` |
|       15 |  8999 | `				PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pRp);` |
|       15 |  9000 | `				if( pRp >= pGen->pEnd ){` |
|      ! 0 |  9001 | `					goto HookSyntax;` |
|        - |  9002 | `				}` |
|       15 |  9003 | `				if( pGen->pIn < pRp ){` |
|       15 |  9004 | `					rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pRp,0,0);` |
|       15 |  9005 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9006 | `						return SXERR_ABORT;` |
|        - |  9007 | `					}` |
|        7 |  9008 | `				}` |
|       15 |  9009 | `				pGen->pIn = &pRp[1];` |
|        7 |  9010 | `			}` |
|       17 |  9011 | `			if( SySetUsed(&pMeth->sFunc.aArgs) < 1 ){` |
|        - |  9012 | `				/* Implicit $value formal */` |
|        - |  9013 | `				ph7_vm_func_arg sVArg;` |
|        3 |  9014 | `				char *zVName = SyMemBackendStrDup(&pGen->pVm->sAllocator,"value",sizeof("value")-1);` |
|        3 |  9015 | `				if( zVName == 0 ){` |
|      ! 0 |  9016 | `					PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9017 | `					return SXERR_ABORT;` |
|        - |  9018 | `				}` |
|        3 |  9019 | `				SyZero(&sVArg,sizeof(ph7_vm_func_arg));` |
|        3 |  9020 | `				SyStringInitFromBuf(&sVArg.sName,zVName,sizeof("value")-1);` |
|        3 |  9021 | `				SySetInit(&sVArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        3 |  9022 | `				SySetInit(&sVArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        3 |  9023 | `				SySetInit(&sVArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|        3 |  9024 | `				SyStringInitFromBuf(&sVArg.sTypeName,0,0);` |
|        3 |  9025 | `				SySetPut(&pMeth->sFunc.aArgs,(const void *)&sVArg);` |
|        1 |  9026 | `			}` |
|        8 |  9027 | `		}` |
|       33 |  9028 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|        - |  9029 | `			/* Block body */` |
|       19 |  9030 | `			rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|       19 |  9031 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9032 | `				return SXERR_ABORT;` |
|        - |  9033 | `			}` |
|       19 |  9034 | `			pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|       31 |  9035 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ARRAY_OP) ){` |
|        - |  9036 | ``			/* `=> expr;` — implicit-return body (the arrow-fn pattern) */`` |
|        - |  9037 | `			GenBlock *pBlock;` |
|        - |  9038 | `			SySet *pInstrContainer;` |
|       15 |  9039 | `			pGen->pIn++; /* Jump '=>' */` |
|       22 |  9040 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|       14 |  9041 | `				PH7_VmInstrLength(pGen->pVm),&pMeth->sFunc,&pBlock);` |
|       15 |  9042 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9043 | `				PH7_GenCompileError(pGen,E_ERROR,nHLine,"PH7 engine is running out-of-memory");` |
|      ! 0 |  9044 | `				return SXERR_ABORT;` |
|        - |  9045 | `			}` |
|       15 |  9046 | `			pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       15 |  9047 | `			PH7_VmSetByteCodeContainer(pGen->pVm,&pMeth->sFunc.aByteCode);` |
|       15 |  9048 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|       15 |  9049 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       15 |  9050 | `			GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       15 |  9051 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       15 |  9052 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       15 |  9053 | `			GenStateLeaveBlock(&(*pGen),0);` |
|       15 |  9054 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9055 | `				return SXERR_ABORT;` |
|        - |  9056 | `			}` |
|       15 |  9057 | `			pMeth->sFunc.nEndLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nHLine;` |
|       15 |  9058 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       15 |  9059 | `				pGen->pIn++; /* Jump ';' */` |
|        7 |  9060 | `			}` |
|       15 |  9061 | `			if( !bGet ){` |
|        - |  9062 | ``				/* `set => expr` assigns the expression to the backing store:`` |
|        - |  9063 | `				 * the dispatcher consumes the implicit return value. */` |
|        3 |  9064 | `				pMeth->sFunc.iFlags \|= VM_FUNC_HOOK_SET_EXPR;` |
|        1 |  9065 | `			}` |
|        8 |  9066 | `		}else{` |
|      ! 0 |  9067 | `			goto HookSyntax;` |
|        - |  9068 | `		}` |
|       33 |  9069 | `		rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|       33 |  9070 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  9071 | `			PH7_GenCompileError(pGen,E_ERROR,nHLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9072 | `			return SXERR_ABORT;` |
|        - |  9073 | `		}` |
|       33 |  9074 | `		pAttr->iFlags \|= bGet ? PH7_CLASS_ATTR_HOOK_GET : PH7_CLASS_ATTR_HOOK_SET;` |
|        1 |  9075 | `	}` |
|       23 |  9076 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_CCB) == 0 ){` |
|      ! 0 |  9077 | `		goto HookSyntax;` |
|        - |  9078 | `	}` |
|       23 |  9079 | `	pGen->pIn++; /* Jump '}' */` |
|       23 |  9080 | `	return SXRET_OK;` |
|      ! 0 |  9081 | `HookSyntax:` |
|      ! 0 |  9082 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  9083 | `		"Invalid property hook declaration for %z::$%z: expecting 'get' or 'set'",` |
|      ! 0 |  9084 | `		&pClass->sName,&pAttr->sName);` |
|      ! 0 |  9085 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  9086 | `		return SXERR_ABORT;` |
|        - |  9087 | `	}` |
|      ! 0 |  9088 | `	return SXERR_CORRUPT;` |
|       12 |  9089 | `}` |
|        - |  9090 | `/*` |
|        - |  9091 | ` * Compile an object interface.` |
|        - |  9092 | ` *  According to the PHP language reference manual` |
|        - |  9093 | ` *   Object Interfaces:` |
|        - |  9094 | ` *   Object interfaces allow you to create code which specifies which methods` |
|        - |  9095 | ` *   a class must implement, without having to define how these methods are handled.` |
|        - |  9096 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|        - |  9097 | ` *   class, but without any of the methods having their contents defined.` |
|        - |  9098 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|        - |  9099 | ` */` |
|    46708 |  9100 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|        5 |  9101 | `{` |
|    46713 |  9102 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  9103 | `	ph7_class *pClass,*pBase;` |
|        - |  9104 | `	SyToken *pEnd,*pTmp;` |
|        - |  9105 | `	SyString *pName;` |
|        - |  9106 | `	sxi32 nKwrd;` |
|        - |  9107 | `	sxi32 rc;` |
|        - |  9108 | `	/* Jump the 'interface' keyword */` |
|    46713 |  9109 | `	pGen->pIn++;` |
|        - |  9110 | `	/* Extract interface name */` |
|    46713 |  9111 | `	pName = &pGen->pIn->sData;` |
|        - |  9112 | `	/* Advance the stream cursor */` |
|    46713 |  9113 | `	pGen->pIn++;` |
|        - |  9114 | `	/* Build FQN and obtain a raw class */ {` |
|        - |  9115 | `		SyBlob sFQN;` |
|        - |  9116 | `		SyString sFQNStr;` |
|    46713 |  9117 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    46713 |  9118 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    46713 |  9119 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    46713 |  9120 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    46713 |  9121 | `		SyBlobRelease(&sFQN);` |
|        - |  9122 | `	}` |
|    46713 |  9123 | `	if( pClass == 0 ){` |
|      ! 0 |  9124 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9125 | `		return SXERR_ABORT;` |
|        - |  9126 | `	}` |
|    46713 |  9127 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    46713 |  9128 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  9129 | `		return SXERR_ABORT;` |
|        - |  9130 | `	}` |
|        - |  9131 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|    46713 |  9132 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|        - |  9133 | `	/* Assume no base class is given */` |
|    46713 |  9134 | `	pBase = 0;` |
|    46713 |  9135 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    15551 |  9136 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    15551 |  9137 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|        - |  9138 | `			SyBlob sResolved;` |
|        - |  9139 | `			SyString sBaseName;` |
|        - |  9140 | `			sxu32 nRefLine;` |
|        - |  9141 | `			/* Extract base interface */` |
|    15551 |  9142 | `			pGen->pIn++;` |
|    15551 |  9143 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    15551 |  9144 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    15551 |  9145 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 |  9146 | `				SyBlobRelease(&sResolved);` |
|      ! 0 |  9147 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  9148 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|      ! 0 |  9149 | `					pName);` |
|      ! 0 |  9150 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  9151 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9152 | `					return SXERR_ABORT;` |
|        - |  9153 | `				}` |
|      ! 0 |  9154 | `				return SXRET_OK;` |
|        - |  9155 | `			}` |
|    23324 |  9156 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    15546 |  9157 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    15551 |  9158 | `			SyStringInitFromBuf(&sBaseName,` |
|        - |  9159 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - |  9160 | `			/* Only interfaces is allowed */` |
|    15551 |  9161 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 |  9162 | `				pBase = pBase->pNextName;` |
|      ! 0 |  9163 | `			}` |
|    15551 |  9164 | `			if( pBase == 0 ){` |
|      ! 0 |  9165 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - |  9166 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|      ! 0 |  9167 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9168 | `					SyBlobRelease(&sResolved);` |
|      ! 0 |  9169 | `					return SXERR_ABORT;` |
|        - |  9170 | `				}` |
|      ! 0 |  9171 | `			}` |
|    15551 |  9172 | `			SyBlobRelease(&sResolved);` |
|     7773 |  9173 | `		}` |
|     7773 |  9174 | `	}` |
|    46713 |  9175 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - |  9176 | `		/* Syntax error */` |
|      ! 0 |  9177 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|      ! 0 |  9178 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  9179 | `		if( rc == SXERR_ABORT ){` |
|        - |  9180 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  9181 | `			return SXERR_ABORT;` |
|        - |  9182 | `		}` |
|      ! 0 |  9183 | `		return SXRET_OK;` |
|        - |  9184 | `	}` |
|    46713 |  9185 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    46713 |  9186 | `	pEnd = 0; /* cc warning */` |
|        - |  9187 | `	/* Delimit the interface body */` |
|    46713 |  9188 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    46713 |  9189 | `	if( pEnd >= pGen->pEnd ){` |
|        - |  9190 | `		/* Syntax error */` |
|      ! 0 |  9191 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|      ! 0 |  9192 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  9193 | `		if( rc == SXERR_ABORT ){` |
|        - |  9194 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  9195 | `			return SXERR_ABORT;` |
|        - |  9196 | `		}` |
|      ! 0 |  9197 | `		return SXRET_OK;` |
|        - |  9198 | `	}` |
|        - |  9199 | `	/* The delimiter token is the interface body's closing brace */` |
|    46713 |  9200 | `	pClass->nEndLine = pEnd->nLine;` |
|        - |  9201 | `	/* Swap token stream */` |
|    46713 |  9202 | `	pTmp = pGen->pEnd;` |
|    46713 |  9203 | `	pGen->pEnd = pEnd;` |
|        - |  9204 | `	/* Start the parse process` |
|        - |  9205 | `	 * Note (According to the PHP reference manual):` |
|        - |  9206 | `	 *  Only constants and function signatures(without body) are allowed.` |
|        - |  9207 | `	 *  Only 'public' visibility is allowed.` |
|        - |  9208 | `	 */` |
|    73875 |  9209 | `	for(;;){` |
|        - |  9210 | `		/* Jump leading/trailing semi-colons */` |
|   248797 |  9211 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   101047 |  9212 | `			pGen->pIn++;` |
|        5 |  9213 | `		}` |
|   147755 |  9214 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - |  9215 | `			/* End of interface body */` |
|    46709 |  9216 | `			break;` |
|        - |  9217 | `		}` |
|        - |  9218 | `		/* Bind a directly-preceding docblock to this member */` |
|   101051 |  9219 | `		GenStateSetPendingDoc(&(*pGen));` |
|   101051 |  9220 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 |  9221 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  9222 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|      ! 0 |  9223 | `				&pGen->pIn->sData,pName);` |
|      ! 0 |  9224 | `			if( rc == SXERR_ABORT ){` |
|        - |  9225 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  9226 | `				return SXERR_ABORT;` |
|        - |  9227 | `			}` |
|      ! 0 |  9228 | `			goto done;` |
|        - |  9229 | `		}` |
|        - |  9230 | `		/* Extract the current keyword */` |
|   101051 |  9231 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   101051 |  9232 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        - |  9233 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|        - |  9234 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|        3 |  9235 | `			const char *zKind = "member";` |
|        3 |  9236 | `			SyString *pMemberName = 0;` |
|        3 |  9237 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|        3 |  9238 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|        3 |  9239 | `				if( nNext == PH7_TKWRD_CONST ){` |
|        3 |  9240 | `					zKind = "constant";` |
|        3 |  9241 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|        3 |  9242 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|        2 |  9243 | `					}` |
|        1 |  9244 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|      ! 0 |  9245 | `					zKind = "method";` |
|      ! 0 |  9246 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|      ! 0 |  9247 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|      ! 0 |  9248 | `					}` |
|      ! 0 |  9249 | `				}` |
|        1 |  9250 | `			}` |
|        3 |  9251 | `			if( pMemberName ){` |
|        4 |  9252 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        1 |  9253 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|        2 |  9254 | `			}else{` |
|      ! 0 |  9255 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9256 | `					"Access type for interface %s must be public",zKind);` |
|        - |  9257 | `			}` |
|        3 |  9258 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9259 | `				return SXERR_ABORT;` |
|        - |  9260 | `			}` |
|        3 |  9261 | `			goto done;` |
|        - |  9262 | `		}` |
|   101049 |  9263 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|      ! 0 |  9264 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9265 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|      ! 0 |  9266 | `			if( rc == SXERR_ABORT ){` |
|        - |  9267 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  9268 | `				return SXERR_ABORT;` |
|        - |  9269 | `			}` |
|      ! 0 |  9270 | `			goto done;` |
|        - |  9271 | `		}` |
|   101049 |  9272 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|        - |  9273 | `			/* Advance the stream cursor */` |
|   101031 |  9274 | `			pGen->pIn++;` |
|   101031 |  9275 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 |  9276 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9277 | `					"Expecting method signature inside interface '%z'",pName);` |
|      ! 0 |  9278 | `				if( rc == SXERR_ABORT ){` |
|        - |  9279 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 |  9280 | `					return SXERR_ABORT;` |
|        - |  9281 | `				}` |
|      ! 0 |  9282 | `				goto done;` |
|        - |  9283 | `			}` |
|   101031 |  9284 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   101031 |  9285 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|      ! 0 |  9286 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9287 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|      ! 0 |  9288 | `				if( rc == SXERR_ABORT ){` |
|        - |  9289 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 |  9290 | `					return SXERR_ABORT;` |
|        - |  9291 | `				}` |
|      ! 0 |  9292 | `				goto done;` |
|        - |  9293 | `			}` |
|    50513 |  9294 | `		}` |
|   101049 |  9295 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|        - |  9296 | `			/* Parse constant */` |
|       16 |  9297 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       16 |  9298 | `			if( rc != SXRET_OK ){` |
|        3 |  9299 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9300 | `					return SXERR_ABORT;` |
|        - |  9301 | `				}` |
|        3 |  9302 | `				goto done;` |
|        - |  9303 | `			}` |
|        7 |  9304 | `		}else{` |
|   101035 |  9305 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   101035 |  9306 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|        - |  9307 | `				/* Static method,record that */` |
|    11657 |  9308 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|        - |  9309 | `				/* Advance the stream cursor */` |
|    11657 |  9310 | `				pGen->pIn++;` |
|    11652 |  9311 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    11657 |  9312 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 |  9313 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9314 | `							"Expecting method signature inside interface '%z'",pName);` |
|      ! 0 |  9315 | `						if( rc == SXERR_ABORT ){` |
|        - |  9316 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 |  9317 | `							return SXERR_ABORT;` |
|        - |  9318 | `						}` |
|      ! 0 |  9319 | `						goto done;` |
|        - |  9320 | `				}` |
|     5826 |  9321 | `			}` |
|        - |  9322 | `			/* Process method signature (no body for interface methods) */` |
|   101035 |  9323 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   101035 |  9324 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9325 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9326 | `					return SXERR_ABORT;` |
|        - |  9327 | `				}` |
|      ! 0 |  9328 | `				goto done;` |
|        - |  9329 | `			}` |
|        - |  9330 | `		}` |
|        5 |  9331 | `	}` |
|        - |  9332 | `	/* Install the interface */` |
|    46709 |  9333 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    46709 |  9334 | `	if( rc == SXRET_OK && pBase ){` |
|        - |  9335 | `		/* Inherit from the base interface */` |
|    15551 |  9336 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|     7773 |  9337 | `	}` |
|    46709 |  9338 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  9339 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9340 | `		return SXERR_ABORT;` |
|        - |  9341 | `	}` |
|    23352 |  9342 | `done:` |
|        - |  9343 | `	/* Point beyond the interface body */` |
|    46713 |  9344 | `	pGen->pIn  = &pEnd[1];` |
|    46713 |  9345 | `	pGen->pEnd = pTmp;` |
|    46713 |  9346 | `	return PH7_OK;` |
|    23359 |  9347 | `}` |
|        - |  9348 | `/*` |
|        - |  9349 | ` * Compile a user-defined class.` |
|        - |  9350 | ` * According to the PHP language reference manual` |
|        - |  9351 | ` *  class` |
|        - |  9352 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|        - |  9353 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|        - |  9354 | ` *  of the properties and methods belonging to the class.` |
|        - |  9355 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|        - |  9356 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|        - |  9357 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|        - |  9358 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|        - |  9359 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|        - |  9360 | ` *  (called "methods").` |
|        - |  9361 | ` */` |
|        - |  9362 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|        - |  9363 | `typedef struct TraitUseEntry TraitUseEntry;` |
|        - |  9364 | `struct TraitUseEntry {` |
|        - |  9365 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|        - |  9366 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|        - |  9367 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|        - |  9368 | `};` |
|        - |  9369 | `/*` |
|        - |  9370 | ` * Validate that methods implementing interface contracts have compatible` |
|        - |  9371 | ` * signatures: public visibility and at least as many parameters as declared.` |
|        - |  9372 | ` */` |
|   215242 |  9373 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 |  9374 | `{` |
|        - |  9375 | `	ph7_class **apIface;` |
|        - |  9376 | `	sxu32 nIface,i;` |
|        - |  9377 | `	sxi32 rc;` |
|   215247 |  9378 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      ! 0 |  9379 | `		return SXRET_OK;` |
|        - |  9380 | `	}` |
|   215247 |  9381 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   215247 |  9382 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   429193 |  9383 | `	for(i = 0; i < nIface; i++){` |
|   213951 |  9384 | `		ph7_class *pIface = apIface[i];` |
|        - |  9385 | `		SyHashEntry *pEntry;` |
|   213951 |  9386 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   498055 |  9387 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   284109 |  9388 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |  9389 | `			ph7_class_method *pImplMeth;` |
|   284109 |  9390 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|        - |  9391 | `			/* Find the implementing method in the class */` |
|   284109 |  9392 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   284109 |  9393 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       18 |  9394 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|        - |  9395 | `			}` |
|        - |  9396 | `			/* Check visibility: interface methods must be implemented as public */` |
|   284095 |  9397 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        4 |  9398 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|        - |  9399 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|        1 |  9400 | `					&pClass->sName,pMName,&pIface->sName);` |
|        3 |  9401 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9402 | `					return SXERR_ABORT;` |
|        - |  9403 | `				}` |
|        1 |  9404 | `			}` |
|        - |  9405 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|        - |  9406 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|        - |  9407 | `			 */` |
|        - |  9408 | `			{` |
|   284095 |  9409 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   284095 |  9410 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   284095 |  9411 | `				int sigError = 0;` |
|   284095 |  9412 | `				if( nImplArgs < nIfaceArgs ){` |
|        3 |  9413 | `					sigError = 1;` |
|   284094 |  9414 | `				}else if( nImplArgs > nIfaceArgs ){` |
|        - |  9415 | `					/* Extra parameters must all have default values */` |
|        6 |  9416 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|        - |  9417 | `					sxu32 k;` |
|        8 |  9418 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|        6 |  9419 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|        3 |  9420 | `							sigError = 1;` |
|        3 |  9421 | `							break;` |
|        - |  9422 | `						}` |
|        2 |  9423 | `					}` |
|        2 |  9424 | `				}` |
|   284095 |  9425 | `				if( sigError ){` |
|        - |  9426 | `					SyBlob sImplSig, sIfaceSig;` |
|        - |  9427 | `					ph7_vm_func_arg *aArgs;` |
|        - |  9428 | `					sxu32 j;` |
|        6 |  9429 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|        6 |  9430 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|        - |  9431 | `					/* Build implementing method signature */` |
|        6 |  9432 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       12 |  9433 | `					for(j = 0; j < nImplArgs; j++){` |
|        8 |  9434 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|        8 |  9435 | `						SyBlobAppend(&sImplSig,"$",1);` |
|        8 |  9436 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|        5 |  9437 | `					}` |
|        - |  9438 | `					/* Build interface method signature */` |
|        6 |  9439 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|       12 |  9440 | `					for(j = 0; j < nIfaceArgs; j++){` |
|        8 |  9441 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|        8 |  9442 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|        8 |  9443 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|        5 |  9444 | `					}` |
|        8 |  9445 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|        - |  9446 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|        2 |  9447 | `						&pClass->sName,pMName,` |
|        4 |  9448 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|        2 |  9449 | `						&pIface->sName,pMName,` |
|        4 |  9450 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|        6 |  9451 | `					SyBlobRelease(&sImplSig);` |
|        6 |  9452 | `					SyBlobRelease(&sIfaceSig);` |
|        6 |  9453 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9454 | `						return SXERR_ABORT;` |
|        - |  9455 | `					}` |
|        2 |  9456 | `				}` |
|        - |  9457 | `			}` |
|        5 |  9458 | `		}` |
|   106978 |  9459 | `	}` |
|   215247 |  9460 | `	return SXRET_OK;` |
|   107626 |  9461 | `}` |
|        - |  9462 | `/*` |
|        - |  9463 | ` * Check that a concrete class has no remaining abstract methods.` |
|        - |  9464 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|        - |  9465 | ` */` |
|   215242 |  9466 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 |  9467 | `{` |
|        - |  9468 | `	ph7_class_method *pMeth;` |
|        - |  9469 | `	SyHashEntry *pEntry;` |
|        - |  9470 | `	sxu32 nAbstract;` |
|        - |  9471 | `	SyBlob sMsg;` |
|        - |  9472 | `	sxi32 rc;` |
|        - |  9473 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   215247 |  9474 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     7811 |  9475 | `		return SXRET_OK;` |
|        - |  9476 | `	}` |
|        - |  9477 | `	/* Count abstract methods */` |
|   207441 |  9478 | `	nAbstract = 0;` |
|   207441 |  9479 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  3076025 |  9480 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  2868589 |  9481 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  2868589 |  9482 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       20 |  9483 | `			nAbstract++;` |
|        8 |  9484 | `		}` |
|        5 |  9485 | `	}` |
|   207441 |  9486 | `	if( nAbstract == 0 ){` |
|   207427 |  9487 | `		return SXRET_OK;` |
|        - |  9488 | `	}` |
|        - |  9489 | `	/* Build the error message listing all abstract methods with origins */` |
|       18 |  9490 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       18 |  9491 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|        - |  9492 | `		"be declared abstract or implement the remaining method%s (",` |
|        7 |  9493 | `		&pClass->sName,nAbstract,` |
|        7 |  9494 | `		(nAbstract > 1 ? "s" : ""),` |
|        7 |  9495 | `		(nAbstract > 1 ? "s" : ""));` |
|        - |  9496 | `	/* Second pass: list methods with origins */` |
|        - |  9497 | `	{` |
|       18 |  9498 | `		sxu32 nListed = 0;` |
|       18 |  9499 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|       36 |  9500 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|       22 |  9501 | `			ph7_class *pOrigin = 0;` |
|        - |  9502 | `			SyString *pMName;` |
|       22 |  9503 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|       22 |  9504 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|        3 |  9505 | `				continue;` |
|        - |  9506 | `			}` |
|       20 |  9507 | `			pMName = &pMeth->sFunc.sName;` |
|       20 |  9508 | `			if( nListed > 0 ){` |
|        3 |  9509 | `				SyBlobAppend(&sMsg,", ",2);` |
|        1 |  9510 | `			}` |
|        - |  9511 | `			/* Find the origin of this abstract method.` |
|        - |  9512 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|        - |  9513 | `			 * inheritance chains) take precedence for interface-declared` |
|        - |  9514 | `			 * methods. Abstract class methods only win when the class` |
|        - |  9515 | `			 * itself declared the abstract method (not inherited from` |
|        - |  9516 | `			 * an interface). Trait methods are adopted into the using` |
|        - |  9517 | `			 * class's namespace.` |
|        - |  9518 | `			 */` |
|        - |  9519 | `			{` |
|        - |  9520 | `				ph7_class **apIface;` |
|        - |  9521 | `				ph7_class **apTrait;` |
|        - |  9522 | `				ph7_class *pWalk;` |
|        - |  9523 | `				sxu32 i;` |
|        - |  9524 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|        - |  9525 | `				 * (one that was written in the class body, not inherited from an` |
|        - |  9526 | `				 * interface). PHP attributes origin to the declaring class.` |
|        - |  9527 | `				 */` |
|       20 |  9528 | `				if( pClass->pBase ){` |
|       11 |  9529 | `					pWalk = pClass->pBase;` |
|       19 |  9530 | `					while( pWalk ){` |
|        - |  9531 | `						ph7_class_method *pParentMeth;` |
|       13 |  9532 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|       13 |  9533 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        - |  9534 | `							/* Exclude methods that came from an interface anywhere` |
|        - |  9535 | `							 * in this class's ancestor chain.` |
|        - |  9536 | `							 */` |
|       13 |  9537 | `							int fromIface = 0;` |
|       13 |  9538 | `							ph7_class *pAnc = pWalk;` |
|       17 |  9539 | `							while( pAnc ){` |
|        - |  9540 | `								ph7_class **apPI;` |
|        - |  9541 | `								sxu32 j;` |
|       15 |  9542 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|       15 |  9543 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       10 |  9544 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       10 |  9545 | `										fromIface = 1;` |
|       10 |  9546 | `										break;` |
|        - |  9547 | `									}` |
|      ! 0 |  9548 | `								}` |
|       15 |  9549 | `								if( fromIface ) break;` |
|        6 |  9550 | `								pAnc = pAnc->pBase;` |
|        2 |  9551 | `							}` |
|       13 |  9552 | `							if( !fromIface ){` |
|        3 |  9553 | `								pOrigin = pWalk;` |
|        3 |  9554 | `								break;` |
|        - |  9555 | `							}` |
|        4 |  9556 | `						}` |
|       10 |  9557 | `						pWalk = pWalk->pBase;` |
|        2 |  9558 | `					}` |
|        4 |  9559 | `				}` |
|        - |  9560 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|        - |  9561 | `				 * each interface's own parent chain for the deepest origin.` |
|        - |  9562 | `				 */` |
|       20 |  9563 | `				if( !pOrigin ){` |
|       18 |  9564 | `					pWalk = pClass;` |
|       40 |  9565 | `					while( pWalk && !pOrigin ){` |
|       26 |  9566 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|       26 |  9567 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|       16 |  9568 | `							ph7_class *pIface = apIface[i];` |
|       16 |  9569 | `							ph7_class *pDeepest = 0;` |
|       28 |  9570 | `							while( pIface ){` |
|       16 |  9571 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|       16 |  9572 | `									pDeepest = pIface;` |
|        6 |  9573 | `								}` |
|       16 |  9574 | `								pIface = pIface->pBase;` |
|        4 |  9575 | `							}` |
|       16 |  9576 | `							if( pDeepest ){` |
|       16 |  9577 | `								pOrigin = pDeepest;` |
|       16 |  9578 | `								break;` |
|        - |  9579 | `							}` |
|      ! 0 |  9580 | `						}` |
|       26 |  9581 | `						pWalk = pWalk->pBase;` |
|        4 |  9582 | `					}` |
|        7 |  9583 | `				}` |
|        - |  9584 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|       20 |  9585 | `				if( !pOrigin ){` |
|        3 |  9586 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|        3 |  9587 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|        3 |  9588 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|        3 |  9589 | `							pOrigin = pClass;` |
|        3 |  9590 | `							break;` |
|        - |  9591 | `						}` |
|      ! 0 |  9592 | `					}` |
|        1 |  9593 | `				}` |
|        - |  9594 | `			}` |
|       20 |  9595 | `			if( pOrigin ){` |
|       20 |  9596 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|       12 |  9597 | `			}else{` |
|        - |  9598 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|      ! 0 |  9599 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|        - |  9600 | `			}` |
|       20 |  9601 | `			nListed++;` |
|        4 |  9602 | `		}` |
|        - |  9603 | `	}` |
|       18 |  9604 | `	SyBlobAppend(&sMsg,")",1);` |
|       25 |  9605 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|       14 |  9606 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|       18 |  9607 | `	SyBlobRelease(&sMsg);` |
|       18 |  9608 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  9609 | `		return SXERR_ABORT;` |
|        - |  9610 | `	}` |
|       18 |  9611 | `	return SXRET_OK;` |
|   107626 |  9612 | `}` |
|        - |  9613 | `/*` |
|        - |  9614 | ` * Parse a class/interface name reference from the current token stream.` |
|        - |  9615 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|        - |  9616 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|        - |  9617 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|        - |  9618 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|        - |  9619 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|        - |  9620 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|        - |  9621 | ` */` |
|   192170 |  9622 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|        5 |  9623 | `{` |
|   192175 |  9624 | `	int isAbsolute = 0;` |
|   192175 |  9625 | `	SyToken *pStart = pGen->pIn;` |
|        - |  9626 | `	SyBlob sName;` |
|   192175 |  9627 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|     4473 |  9628 | `		isAbsolute = 1;` |
|     4473 |  9629 | `		pGen->pIn++;` |
|     2234 |  9630 | `	}` |
|   192175 |  9631 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        8 |  9632 | `		pGen->pIn = pStart;` |
|        8 |  9633 | `		return SXERR_INVALID;` |
|        - |  9634 | `	}` |
|   192169 |  9635 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   192169 |  9636 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   192169 |  9637 | `	pGen->pIn++;` |
|   288267 |  9638 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|    96108 |  9639 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       16 |  9640 | `		SyBlobAppend(&sName,"\\",1);` |
|       16 |  9641 | `		pGen->pIn++;` |
|       16 |  9642 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       16 |  9643 | `		pGen->pIn++;` |
|        2 |  9644 | `	}` |
|   192169 |  9645 | `	if( isAbsolute ){` |
|     4471 |  9646 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|     2238 |  9647 | `	}else{` |
|        - |  9648 | `		SyString sRaw;` |
|   187703 |  9649 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   187703 |  9650 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|        - |  9651 | `	}` |
|   192169 |  9652 | `	SyBlobRelease(&sName);` |
|   192169 |  9653 | `	return SXRET_OK;` |
|    96090 |  9654 | `}` |
|        - |  9655 | `/*` |
|        - |  9656 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|        - |  9657 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|        - |  9658 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|        - |  9659 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|        - |  9660 | ` * either direction cannot run unbounded.` |
|        - |  9661 | ` */` |
|        - |  9662 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    46804 |  9663 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|        5 |  9664 | `{` |
|        - |  9665 | `	ph7_class **apParent;` |
|        - |  9666 | `	sxu32 n;` |
|   120839 |  9667 | `	while( pInterface ){` |
|    81813 |  9668 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|      ! 0 |  9669 | `			return FALSE;` |
|        - |  9670 | `		}` |
|   101252 |  9671 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    38878 |  9672 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|     7783 |  9673 | `			return TRUE;` |
|        - |  9674 | `		}` |
|    74035 |  9675 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    74035 |  9676 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|      ! 0 |  9677 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|      ! 0 |  9678 | `				return TRUE;` |
|        - |  9679 | `			}` |
|      ! 0 |  9680 | `		}` |
|    74035 |  9681 | `		pInterface = pInterface->pBase;` |
|    74035 |  9682 | `		iDepth++;` |
|        5 |  9683 | `	}` |
|    39031 |  9684 | `	return FALSE;` |
|    23407 |  9685 | `}` |
|    46804 |  9686 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|        5 |  9687 | `{` |
|    46809 |  9688 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|        5 |  9689 | `}` |
|        - |  9690 | `/*` |
|        - |  9691 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|        - |  9692 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|        - |  9693 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|        - |  9694 | ` */` |
|     7778 |  9695 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|        5 |  9696 | `{` |
|     7787 |  9697 | `	while( pBase ){` |
|       10 |  9698 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|        2 |  9699 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|        3 |  9700 | `			return TRUE;` |
|        - |  9701 | `		}` |
|       10 |  9702 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|        6 |  9703 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|        3 |  9704 | `			return TRUE;` |
|        - |  9705 | `		}` |
|        5 |  9706 | `		pBase = pBase->pBase;` |
|        1 |  9707 | `	}` |
|     7779 |  9708 | `	return FALSE;` |
|     3894 |  9709 | `}` |
|        - |  9710 | `/*` |
|        - |  9711 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|        - |  9712 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|        - |  9713 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|        - |  9714 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|        - |  9715 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|        - |  9716 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|        - |  9717 | ` * pClass->aEnumCases for cases().` |
|        - |  9718 | ` */` |
|       42 |  9719 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 |  9720 | `{` |
|       47 |  9721 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  9722 | `	SySet *pInstrContainer;` |
|        - |  9723 | `	ph7_class_attr *pCase;` |
|        - |  9724 | `	SyString *pName;` |
|        - |  9725 | `	sxi32 rc;` |
|       47 |  9726 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|       47 |  9727 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  9728 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  9729 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|      ! 0 |  9730 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  9731 | `			return SXERR_ABORT;` |
|        - |  9732 | `		}` |
|      ! 0 |  9733 | `		goto Synchronize;` |
|        - |  9734 | `	}` |
|       47 |  9735 | `	pName = &pGen->pIn->sData;` |
|        - |  9736 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|       47 |  9737 | `	if( SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte) != 0 ){` |
|      ! 0 |  9738 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9739 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|      ! 0 |  9740 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  9741 | `			return SXERR_ABORT;` |
|        - |  9742 | `		}` |
|      ! 0 |  9743 | `		goto Synchronize;` |
|        - |  9744 | `	}` |
|       47 |  9745 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - |  9746 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|       47 |  9747 | `	if( pCase == 0 ){` |
|      ! 0 |  9748 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9749 | `		return SXERR_ABORT;` |
|        - |  9750 | `	}` |
|       47 |  9751 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|       47 |  9752 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  9753 | `		return SXERR_ABORT;` |
|        - |  9754 | `	}` |
|       47 |  9755 | `	pGen->pIn++; /* Jump the case name */` |
|       47 |  9756 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|       31 |  9757 | `		if( pClass->nEnumBacking == 0 ){` |
|        8 |  9758 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        2 |  9759 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|        6 |  9760 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9761 | `				return SXERR_ABORT;` |
|        - |  9762 | `			}` |
|        6 |  9763 | `			goto Synchronize;` |
|        - |  9764 | `		}` |
|       25 |  9765 | `		pGen->pIn++; /* Jump the equal sign */` |
|        - |  9766 | `		/* Compile the backing value expression into the case's own container` |
|        - |  9767 | `		 * (same technique as class constants). */` |
|       25 |  9768 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       25 |  9769 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|       25 |  9770 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|       25 |  9771 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 |  9772 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  9773 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|      ! 0 |  9774 | `		}` |
|       25 |  9775 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|       25 |  9776 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       25 |  9777 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  9778 | `			return SXERR_ABORT;` |
|        - |  9779 | `		}` |
|       13 |  9780 | `	}else{` |
|       17 |  9781 | `		if( pClass->nEnumBacking != 0 ){` |
|      ! 0 |  9782 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  9783 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|      ! 0 |  9784 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9785 | `				return SXERR_ABORT;` |
|        - |  9786 | `			}` |
|      ! 0 |  9787 | `			goto Synchronize;` |
|        - |  9788 | `		}` |
|        - |  9789 | `	}` |
|       41 |  9790 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|       41 |  9791 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  9792 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9793 | `		return SXERR_ABORT;` |
|        - |  9794 | `	}` |
|       41 |  9795 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|       41 |  9796 | `	return SXRET_OK;` |
|        2 |  9797 | `Synchronize:` |
|        - |  9798 | `	/* Synchronize with the first semi-colon */` |
|       14 |  9799 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|       10 |  9800 | `		pGen->pIn++;` |
|        2 |  9801 | `	}` |
|        6 |  9802 | `	return SXERR_CORRUPT;` |
|       26 |  9803 | `}` |
|        - |  9804 | `/*` |
|        - |  9805 | ` * Synthesize the enum interface methods (PHP 8.1): cases() for every enum,` |
|        - |  9806 | ` * plus from()/tryFrom() for backed enums. Each is an ordinary public static` |
|        - |  9807 | ` * method whose body forwards to a __phl_enum_* engine thunk (vm.c) with the` |
|        - |  9808 | ` * enum's FQN embedded as a literal — the same forwarder pattern the` |
|        - |  9809 | ` * Generator/Fiber/Reflection builtins use. The source buffer is owned by the` |
|        - |  9810 | ` * VM allocator and never freed: tokens (method and parameter names) keep` |
|        - |  9811 | ` * pointers into it (see the constructor-promotion precedent above).` |
|        - |  9812 | ` */` |
|       24 |  9813 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|        3 |  9814 | `{` |
|        - |  9815 | `	SyToken *pSaveIn,*pSaveEnd;` |
|        - |  9816 | `	const char *zBack;` |
|        - |  9817 | `	SySet sToken;` |
|        - |  9818 | `	char *zSrc;` |
|        - |  9819 | `	sxu32 nSrc,nMax;` |
|       27 |  9820 | `	sxi32 rc = SXRET_OK;` |
|       27 |  9821 | `	nMax = 3*(sxu32)sizeof("function tryFrom(string $value){return __phl_enum_tryfrom('',$value);}")` |
|       24 |  9822 | `		+ 3*SyStringLength(&pClass->sName) + 64;` |
|       27 |  9823 | `	zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nMax);` |
|       27 |  9824 | `	if( zSrc == 0 ){` |
|      ! 0 |  9825 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9826 | `		return SXERR_ABORT;` |
|        - |  9827 | `	}` |
|       27 |  9828 | `	zBack = (pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string";` |
|       27 |  9829 | `	if( pClass->nEnumBacking != 0 ){` |
|       19 |  9830 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|        - |  9831 | `			"function cases(){return __phl_enum_cases('%z');}"` |
|        - |  9832 | `			"function from(%s $value){return __phl_enum_from('%z',$value);}"` |
|        - |  9833 | `			"function tryFrom(%s $value){return __phl_enum_tryfrom('%z',$value);}",` |
|        6 |  9834 | `			&pClass->sName,zBack,&pClass->sName,zBack,&pClass->sName);` |
|        7 |  9835 | `	}else{` |
|       21 |  9836 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|        6 |  9837 | `			"function cases(){return __phl_enum_cases('%z');}",&pClass->sName);` |
|        - |  9838 | `	}` |
|       27 |  9839 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       27 |  9840 | `	PH7_TokenizePHP(zSrc,nSrc,pClass->nLine,&sToken,0);` |
|       27 |  9841 | `	pSaveIn = pGen->pIn;` |
|       27 |  9842 | `	pSaveEnd = pGen->pEnd;` |
|       27 |  9843 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|       27 |  9844 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       75 |  9845 | `	while( pGen->pIn < pGen->pEnd && rc != SXERR_ABORT ){` |
|       51 |  9846 | `		rc = GenStateCompileClassMethod(&(*pGen),PH7_TKWRD_PUBLIC,PH7_CLASS_ATTR_STATIC,TRUE,pClass);` |
|        3 |  9847 | `	}` |
|       27 |  9848 | `	pGen->pIn = pSaveIn;` |
|       27 |  9849 | `	pGen->pEnd = pSaveEnd;` |
|       27 |  9850 | `	SySetRelease(&sToken);` |
|       27 |  9851 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXRET_OK;` |
|       15 |  9852 | `}` |
|        - |  9853 | `/*` |
|        - |  9854 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|        - |  9855 | ` * __call/__callStatic/__invoke stay allowed).` |
|        - |  9856 | ` */` |
|        - |  9857 | `static const char *azEnumBannedMagic[] = {` |
|        - |  9858 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|        - |  9859 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|        - |  9860 | `};` |
|        - |  9861 | `/*` |
|        - |  9862 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|        - |  9863 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|        - |  9864 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|        - |  9865 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|        - |  9866 | ` * and before the class is installed.` |
|        - |  9867 | ` */` |
|       24 |  9868 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|        3 |  9869 | `{` |
|        - |  9870 | `	SyHashEntry *pEntry;` |
|        - |  9871 | `	sxi32 rc;` |
|        - |  9872 | `	sxu32 n;` |
|        - |  9873 | `	/* php: "Enum %s cannot include properties" */` |
|       27 |  9874 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|       69 |  9875 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|       47 |  9876 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       47 |  9877 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        3 |  9878 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|        1 |  9879 | `				"Enum %z cannot include properties",&pClass->sName);` |
|        3 |  9880 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9881 | `				return SXERR_ABORT;` |
|        - |  9882 | `			}` |
|        3 |  9883 | `			break;` |
|        - |  9884 | `		}` |
|        2 |  9885 | `	}` |
|        - |  9886 | `	/* php: "Enum %s cannot include magic method %s" */` |
|      339 |  9887 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|      468 |  9888 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|      315 |  9889 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|      ! 0 |  9890 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  9891 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|      ! 0 |  9892 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9893 | `				return SXERR_ABORT;` |
|        - |  9894 | `			}` |
|      ! 0 |  9895 | `		}` |
|      159 |  9896 | `	}` |
|        - |  9897 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|        - |  9898 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|        - |  9899 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|        - |  9900 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|        - |  9901 | `	{` |
|        - |  9902 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|        - |  9903 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|        - |  9904 | `		ph7_class_attr *pAttr;` |
|       27 |  9905 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - |  9906 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|       27 |  9907 | `		if( pAttr == 0 ){` |
|      ! 0 |  9908 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9909 | `			return SXERR_ABORT;` |
|        - |  9910 | `		}` |
|       27 |  9911 | `		pAttr->nType = MEMOBJ_STRING;` |
|       27 |  9912 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|       27 |  9913 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|       27 |  9914 | `		if( pClass->nEnumBacking != 0 ){` |
|       13 |  9915 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - |  9916 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|       13 |  9917 | `			if( pAttr == 0 ){` |
|      ! 0 |  9918 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9919 | `				return SXERR_ABORT;` |
|        - |  9920 | `			}` |
|       13 |  9921 | `			pAttr->nType = pClass->nEnumBacking;` |
|       13 |  9922 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|        7 |  9923 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|        4 |  9924 | `			}else{` |
|        7 |  9925 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|        - |  9926 | `			}` |
|       13 |  9927 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|        6 |  9928 | `		}` |
|        - |  9929 | `	}` |
|       27 |  9930 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|       15 |  9931 | `}` |
|        - |  9932 | `/*` |
|        - |  9933 | ` * Compile a class declaration, named or anonymous.` |
|        - |  9934 | ` *` |
|        - |  9935 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|        - |  9936 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|        - |  9937 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|        - |  9938 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|        - |  9939 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|        - |  9940 | ` * implements, body, install) is shared by both paths.` |
|        - |  9941 | ` */` |
|   215286 |  9942 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|        - |  9943 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|        5 |  9944 | `{` |
|   215291 |  9945 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  9946 | `	ph7_class *pClass,*pBase;` |
|        - |  9947 | `	SyToken *pEnd,*pTmp;` |
|        - |  9948 | `	sxi32 iProtection;` |
|        - |  9949 | `	SySet aInterfaces;` |
|        - |  9950 | `	SySet aUseEntries;` |
|        - |  9951 | `	sxi32 iAttrflags;` |
|        - |  9952 | `	SyString *pName;` |
|        - |  9953 | `	sxi32 nKwrd;` |
|        - |  9954 | `	sxi32 rc;` |
|        - |  9955 | `	/* Jump the 'class' keyword */` |
|   215291 |  9956 | `	pGen->pIn++;` |
|   215291 |  9957 | `	if( pAnonName ){` |
|        - |  9958 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|        - |  9959 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|        - |  9960 | `		 * then use the synthesized name. */` |
|       32 |  9961 | `		*ppArgStart = *ppArgEnd = 0;` |
|       32 |  9962 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        7 |  9963 | `			pGen->pIn++; /* Jump '(' */` |
|        7 |  9964 | `			*ppArgStart = pGen->pIn;` |
|       10 |  9965 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|        3 |  9966 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|        7 |  9967 | `			pGen->pIn = *ppArgEnd;` |
|        7 |  9968 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|        3 |  9969 | `		}` |
|       32 |  9970 | `		pName = pAnonName;` |
|       32 |  9971 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|       18 |  9972 | `	}else{` |
|   215263 |  9973 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|        - |  9974 | `			/* Syntax error */` |
|      ! 0 |  9975 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|      ! 0 |  9976 | `			if( rc == SXERR_ABORT ){` |
|        - |  9977 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  9978 | `				return SXERR_ABORT;` |
|        - |  9979 | `			}` |
|        - |  9980 | `			/* Synchronize with the first semi-colon or curly braces */` |
|      ! 0 |  9981 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|      ! 0 |  9982 | `				pGen->pIn++;` |
|      ! 0 |  9983 | `			}` |
|      ! 0 |  9984 | `			return SXRET_OK;` |
|        - |  9985 | `		}` |
|        - |  9986 | `		/* Extract class name */` |
|   215263 |  9987 | `		pName = &pGen->pIn->sData;` |
|        - |  9988 | `		/* Advance the stream cursor */` |
|   215263 |  9989 | `		pGen->pIn++;` |
|        - |  9990 | `		/* Build FQN and obtain a raw class */ {` |
|        - |  9991 | `			SyBlob sFQN;` |
|        - |  9992 | `			SyString sFQNStr;` |
|   215263 |  9993 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   215263 |  9994 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|   215263 |  9995 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   215263 |  9996 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   215263 |  9997 | `			SyBlobRelease(&sFQN);` |
|        - |  9998 | `		}` |
|        - |  9999 | `	}` |
|   215291 | 10000 | `	if( pClass == 0 ){` |
|      ! 0 | 10001 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 10002 | `		return SXERR_ABORT;` |
|        - | 10003 | `	}` |
|   215286 | 10004 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|       33 | 10005 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|        - | 10006 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|       16 | 10007 | `		pGen->pIn++; /* Jump ':' */` |
|       14 | 10008 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       16 | 10009 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|        7 | 10010 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|        7 | 10011 | `			pGen->pIn++;` |
|       12 | 10012 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       10 | 10013 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|        7 | 10014 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|        7 | 10015 | `			pGen->pIn++;` |
|        4 | 10016 | `		}else{` |
|        3 | 10017 | `			SyToken *pTok = pGen->pIn;` |
|        3 | 10018 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|        4 | 10019 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|        1 | 10020 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|        3 | 10021 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 10022 | `				return SXERR_ABORT;` |
|        - | 10023 | `			}` |
|        3 | 10024 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|        3 | 10025 | `				pGen->pIn++; /* Skip the bogus type token */` |
|        1 | 10026 | `			}` |
|        - | 10027 | `		}` |
|        7 | 10028 | `	}` |
|   215291 | 10029 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|   215291 | 10030 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 10031 | `		return SXERR_ABORT;` |
|        - | 10032 | `	}` |
|        - | 10033 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   215291 | 10034 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   215291 | 10035 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|        - | 10036 | `	/* Assume a standalone class */` |
|   215291 | 10037 | `	pBase = 0;` |
|   215291 | 10038 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   171301 | 10039 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   171301 | 10040 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|        - | 10041 | `			SyBlob sResolved;` |
|        - | 10042 | `			SyString sBaseName;` |
|        - | 10043 | `			sxu32 nRefLine;` |
|   124521 | 10044 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|        - | 10045 | `				/* php parse-fatals here (enums have no inheritance) */` |
|      ! 0 | 10046 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 10047 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|      ! 0 | 10048 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10049 | `					return SXERR_ABORT;` |
|        - | 10050 | `				}` |
|      ! 0 | 10051 | `			}` |
|   124521 | 10052 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   124521 | 10053 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   124521 | 10054 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   124521 | 10055 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        3 | 10056 | `				SyBlobRelease(&sResolved);` |
|        4 | 10057 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 10058 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|        1 | 10059 | `					pName);` |
|        3 | 10060 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|        3 | 10061 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10062 | `					return SXERR_ABORT;` |
|        - | 10063 | `				}` |
|        3 | 10064 | `				return SXRET_OK;` |
|        - | 10065 | `			}` |
|   186776 | 10066 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   124514 | 10067 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   124519 | 10068 | `			SyStringInitFromBuf(&sBaseName,` |
|        - | 10069 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 10070 | `			/* Interfaces are not allowed */` |
|   124519 | 10071 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|      ! 0 | 10072 | `				pBase = pBase->pNextName;` |
|      ! 0 | 10073 | `			}` |
|   124519 | 10074 | `			if( pBase == 0 ){` |
|      ! 0 | 10075 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - | 10076 | `					"Nonexistent base class '%z'",&sBaseName);` |
|      ! 0 | 10077 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10078 | `					SyBlobRelease(&sResolved);` |
|      ! 0 | 10079 | `					return SXERR_ABORT;` |
|        - | 10080 | `				}` |
|      ! 0 | 10081 | `			}else{` |
|   124519 | 10082 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|        4 | 10083 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 | 10084 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|        3 | 10085 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10086 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 10087 | `						return SXERR_ABORT;` |
|        - | 10088 | `					}` |
|        3 | 10089 | `					pBase = 0; /* Never inherit from an enum */` |
|   124518 | 10090 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|      ! 0 | 10091 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 | 10092 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|      ! 0 | 10093 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10094 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 10095 | `						return SXERR_ABORT;` |
|        - | 10096 | `					}` |
|      ! 0 | 10097 | `				}` |
|        - | 10098 | `			}` |
|   124519 | 10099 | `			SyBlobRelease(&sResolved);` |
|   124519 | 10100 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|      ! 0 | 10101 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|      ! 0 | 10102 | `			}` |
|    62257 | 10103 | `		}` |
|   171299 | 10104 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|        - | 10105 | `			ph7_class *pInterface;` |
|        - | 10106 | `			/* Interface implementation */` |
|    46797 | 10107 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    23408 | 10108 | `			for(;;){` |
|        - | 10109 | `				SyBlob sResolved;` |
|        - | 10110 | `				SyString sIntName;` |
|        - | 10111 | `				sxu32 nRefLine;` |
|    46809 | 10112 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    46809 | 10113 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    46809 | 10114 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 10115 | `					SyBlobRelease(&sResolved);` |
|      ! 0 | 10116 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 10117 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|      ! 0 | 10118 | `						pName);` |
|      ! 0 | 10119 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10120 | `						return SXERR_ABORT;` |
|        - | 10121 | `					}` |
|      ! 0 | 10122 | `					break;` |
|        - | 10123 | `				}` |
|    93613 | 10124 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    46804 | 10125 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    46809 | 10126 | `				SyStringInitFromBuf(&sIntName,` |
|        - | 10127 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - | 10128 | `				/* Only interfaces are allowed */` |
|    46809 | 10129 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 10130 | `					pInterface = pInterface->pNextName;` |
|      ! 0 | 10131 | `				}` |
|    46809 | 10132 | `				if( pInterface == 0 ){` |
|      ! 0 | 10133 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - | 10134 | `						"Nonexistent base interface '%z'",&sIntName);` |
|      ! 0 | 10135 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10136 | `						SyBlobRelease(&sResolved);` |
|      ! 0 | 10137 | `						return SXERR_ABORT;` |
|        - | 10138 | `					}` |
|      ! 0 | 10139 | `				}else{` |
|        - | 10140 | `					/* Reject user classes that try to implement Throwable` |
|        - | 10141 | `					 * directly (or via an interface that extends Throwable)` |
|        - | 10142 | `					 * unless they already extend Exception or Error.` |
|        - | 10143 | `					 * Exception and Error themselves are compiled from the` |
|        - | 10144 | `					 * built-in library and are exempt by FQN — a namespaced` |
|        - | 10145 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    46809 | 10146 | `					SyString *pFqn = &pClass->sName;` |
|    46809 | 10147 | `					int bIsExceptionOrError =` |
|    27290 | 10148 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|    72152 | 10149 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    44869 | 10150 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|     3898 | 10151 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|    50693 | 10152 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|    11670 | 10153 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|     3887 | 10154 | `						!bIsExceptionOrError ){` |
|       12 | 10155 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10156 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|        3 | 10157 | `							&pClass->sName);` |
|        9 | 10158 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10159 | `							SyBlobRelease(&sResolved);` |
|      ! 0 | 10160 | `							return SXERR_ABORT;` |
|        - | 10161 | `						}` |
|        - | 10162 | `						/* Skip registration so the follow-up abstract-method` |
|        - | 10163 | `						 * check does not produce a duplicate fatal. */` |
|        6 | 10164 | `					}else{` |
|    46803 | 10165 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|        - | 10166 | `					}` |
|        - | 10167 | `				}` |
|    46809 | 10168 | `				SyBlobRelease(&sResolved);` |
|    46809 | 10169 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    23401 | 10170 | `					break;` |
|        - | 10171 | `				}` |
|       16 | 10172 | `				pGen->pIn++;/* Jump the comma */` |
|        4 | 10173 | `			}` |
|    23396 | 10174 | `		}` |
|    85647 | 10175 | `	}` |
|   215289 | 10176 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - | 10177 | `		/* Syntax error */` |
|      ! 0 | 10178 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|      ! 0 | 10179 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 10180 | `		if( rc == SXERR_ABORT ){` |
|        - | 10181 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 10182 | `			return SXERR_ABORT;` |
|        - | 10183 | `		}` |
|      ! 0 | 10184 | `		return SXRET_OK;` |
|        - | 10185 | `	}` |
|   215289 | 10186 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   215289 | 10187 | `	pEnd = 0; /* cc warning */` |
|        - | 10188 | `	/* Delimit the class body */` |
|   215289 | 10189 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   215289 | 10190 | `	if( pEnd >= pGen->pEnd ){` |
|        - | 10191 | `		/* Syntax error */` |
|      ! 0 | 10192 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|      ! 0 | 10193 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 10194 | `		if( rc == SXERR_ABORT ){` |
|        - | 10195 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 10196 | `			return SXERR_ABORT;` |
|        - | 10197 | `		}` |
|      ! 0 | 10198 | `		return SXRET_OK;` |
|        - | 10199 | `	}` |
|        - | 10200 | `	/* The delimiter token is the class body's closing brace */` |
|   215289 | 10201 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 10202 | `	/* Swap token stream */` |
|   215289 | 10203 | `	pTmp = pGen->pEnd;` |
|   215289 | 10204 | `	pGen->pEnd = pEnd;` |
|        - | 10205 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|   215289 | 10206 | `	pClass->iFlags \|= iFlags;` |
|        - | 10207 | `	/* Start the parse process */` |
|   826936 | 10208 | `	for(;;){` |
|        - | 10209 | `		/* Jump leading/trailing semi-colons */` |
|  2219061 | 10210 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   354515 | 10211 | `			pGen->pIn++;` |
|        5 | 10212 | `		}` |
|  1864551 | 10213 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 10214 | `			/* End of class body */` |
|   215247 | 10215 | `			break;` |
|        - | 10216 | `		}` |
|        - | 10217 | `		/* Bind a directly-preceding docblock to this member */` |
|  1649309 | 10218 | `		GenStateSetPendingDoc(&(*pGen));` |
|  1649304 | 10219 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|   824657 | 10220 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|      ! 0 | 10221 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10222 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|      ! 0 | 10223 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 10224 | `			if( rc == SXERR_ABORT ){` |
|        - | 10225 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 10226 | `				return SXERR_ABORT;` |
|        - | 10227 | `			}` |
|      ! 0 | 10228 | `			goto done;` |
|        - | 10229 | `		}` |
|        - | 10230 | `		/* Assume public visibility */` |
|  1649309 | 10231 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  1649309 | 10232 | `		iAttrflags = 0;` |
|        - | 10233 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|        - | 10234 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|        - | 10235 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|        - | 10236 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  1649309 | 10237 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      ! 0 | 10238 | `			int bMod = 0;` |
|      ! 0 | 10239 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      ! 0 | 10240 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        - | 10241 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|        - | 10242 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|        - | 10243 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|        - | 10244 | `			 * that the generic keyword dispatch would misread as a method. */` |
|      ! 0 | 10245 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      ! 0 | 10246 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      ! 0 | 10247 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|      ! 0 | 10248 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|      ! 0 | 10249 | `			}` |
|      ! 0 | 10250 | `			if( !bMod ){` |
|      ! 0 | 10251 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 10252 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 10253 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10254 | `						return SXERR_ABORT;` |
|        - | 10255 | `					}` |
|      ! 0 | 10256 | `					goto done;` |
|        - | 10257 | `				}` |
|      ! 0 | 10258 | `				continue;` |
|        - | 10259 | `			}` |
|      ! 0 | 10260 | `		}` |
|  1649309 | 10261 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 10262 | `			/* Extract the current keyword */` |
|  1649309 | 10263 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1649309 | 10264 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|        - | 10265 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|       47 | 10266 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|       47 | 10267 | `				if( rc != SXRET_OK ){` |
|        6 | 10268 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10269 | `						return SXERR_ABORT;` |
|        - | 10270 | `					}` |
|        6 | 10271 | `					goto done;` |
|        - | 10272 | `				}` |
|       41 | 10273 | `				continue;` |
|        - | 10274 | `			}` |
|  1649267 | 10275 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - | 10276 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|        - | 10277 | `				TraitUseEntry sUse;` |
|       63 | 10278 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|       63 | 10279 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|       63 | 10280 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|       37 | 10281 | `				for(;;){` |
|        - | 10282 | `					ph7_class *pTrait;` |
|        - | 10283 | `					SyString *pTraitName;` |
|       71 | 10284 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 10285 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 10286 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|      ! 0 | 10287 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10288 | `							return SXERR_ABORT;` |
|        - | 10289 | `						}` |
|      ! 0 | 10290 | `						break;` |
|        - | 10291 | `					}` |
|       71 | 10292 | `					pTraitName = &pGen->pIn->sData;` |
|        - | 10293 | `					/* Resolve trait name through namespace/imports */ {` |
|        - | 10294 | `						SyBlob sResolved;` |
|       71 | 10295 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       71 | 10296 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|      137 | 10297 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|       66 | 10298 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       71 | 10299 | `						SyBlobRelease(&sResolved);` |
|        - | 10300 | `					}` |
|        - | 10301 | `					/* Only traits are allowed */` |
|       71 | 10302 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 | 10303 | `						pTrait = pTrait->pNextName;` |
|      ! 0 | 10304 | `					}` |
|       71 | 10305 | `					if( pTrait == 0 ){` |
|      ! 0 | 10306 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 10307 | `							"'%z' is not a trait",pTraitName);` |
|      ! 0 | 10308 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10309 | `							return SXERR_ABORT;` |
|        - | 10310 | `						}` |
|      ! 0 | 10311 | `					}else{` |
|       71 | 10312 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|        - | 10313 | `					}` |
|       71 | 10314 | `					pGen->pIn++; /* Advance past trait name */` |
|       71 | 10315 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       34 | 10316 | `						break;` |
|        - | 10317 | `					}` |
|       10 | 10318 | `					pGen->pIn++; /* Jump the comma */` |
|        2 | 10319 | `				}` |
|        - | 10320 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|       63 | 10321 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|        - | 10322 | `					SyToken *pBlock;` |
|       13 | 10323 | `					pGen->pIn++; /* Jump '{' */` |
|       13 | 10324 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       13 | 10325 | `					sUse.pResolvStart = pGen->pIn;` |
|       13 | 10326 | `					sUse.pResolvEnd = pBlock;` |
|       13 | 10327 | `					if( pBlock < pGen->pEnd ){` |
|       13 | 10328 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|        8 | 10329 | `					}else{` |
|      ! 0 | 10330 | `						pGen->pIn = pGen->pEnd;` |
|        - | 10331 | `					}` |
|        5 | 10332 | `				}` |
|       63 | 10333 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|        - | 10334 | `				/* The semicolon will be consumed by the outer loop */` |
|       63 | 10335 | `				continue;` |
|        - | 10336 | `			}` |
|  1649209 | 10337 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        - | 10338 | `				int nSetTok;` |
|  1505029 | 10339 | `				sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|  1505029 | 10340 | `				if( nSetVis ){` |
|        - | 10341 | ``					/* Leading `private(set)`/`protected(set)` with no read`` |
|        - | 10342 | `					 * visibility: the read side defaults to public (php 8.4). */` |
|        3 | 10343 | `					iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|        3 | 10344 | `					pGen->pIn += nSetTok;` |
|        2 | 10345 | `				}else{` |
|  1505027 | 10346 | `					iProtection = nKwrd;` |
|  1505027 | 10347 | `					pGen->pIn++; /* Jump the visibility token */` |
|        - | 10348 | `					/* Optional asymmetric set-visibility after the read` |
|        - | 10349 | ``					 * visibility: `public private(set) int $x`. */`` |
|  1505027 | 10350 | `					nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|  1505027 | 10351 | `					if( nSetVis ){` |
|        9 | 10352 | `						iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|        9 | 10353 | `						pGen->pIn += nSetTok;` |
|        4 | 10354 | `					}` |
|        - | 10355 | `				}` |
|        - | 10356 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`,`` |
|        - | 10357 | ``				 * `public private(set) readonly int $x`. */`` |
|  1505029 | 10358 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       24 | 10359 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       24 | 10360 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       10 | 10361 | `				}` |
|  1505024 | 10362 | `				if( pGen->pIn >= pGen->pEnd` |
|  1505029 | 10363 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 10364 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10365 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|      ! 0 | 10366 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 10367 | `					if( rc == SXERR_ABORT ){` |
|        - | 10368 | `						/* Error count limit reached,abort immediately */` |
|      ! 0 | 10369 | `						return SXERR_ABORT;` |
|        - | 10370 | `					}` |
|      ! 0 | 10371 | `					goto done;` |
|        - | 10372 | `				}` |
|  1505029 | 10373 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        - | 10374 | `					/* Attribute declaration (untyped) */` |
|   210329 | 10375 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   210329 | 10376 | `					if( rc != SXRET_OK ){` |
|       11 | 10377 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10378 | `							return SXERR_ABORT;` |
|        - | 10379 | `						}` |
|       11 | 10380 | `						goto done;` |
|        - | 10381 | `					}` |
|   210428 | 10382 | `					continue;` |
|        - | 10383 | `				}` |
|  1294705 | 10384 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        - | 10385 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|      225 | 10386 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      225 | 10387 | `					if( rc != SXRET_OK ){` |
|        8 | 10388 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10389 | `							return SXERR_ABORT;` |
|        - | 10390 | `						}` |
|        8 | 10391 | `						goto done;` |
|        - | 10392 | `					}` |
|      219 | 10393 | `					continue;` |
|        - | 10394 | `				}` |
|        - | 10395 | `				/* Extract the keyword */` |
|  1294485 | 10396 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   647240 | 10397 | `			}` |
|  1438665 | 10398 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|        - | 10399 | `				/* Process constant declaration */` |
|   143863 | 10400 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|   143863 | 10401 | `				if( rc != SXRET_OK ){` |
|       11 | 10402 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10403 | `						return SXERR_ABORT;` |
|        - | 10404 | `					}` |
|       11 | 10405 | `					goto done;` |
|        - | 10406 | `				}` |
|    71930 | 10407 | `			}else{` |
|  1294807 | 10408 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|        - | 10409 | `					/* Static method or attribute,record that */` |
|    23445 | 10410 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    23445 | 10411 | `					pGen->pIn++; /* Jump the static keyword */` |
|    23445 | 10412 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 10413 | `						int nSetTok;` |
|    23417 | 10414 | `						sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|    23417 | 10415 | `						if( nSetVis ){` |
|        - | 10416 | ``							/* `static private(set) int $x` — read side stays public */`` |
|        3 | 10417 | `							iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|        3 | 10418 | `							pGen->pIn += nSetTok;` |
|        2 | 10419 | `						}else{` |
|        - | 10420 | `							/* Extract the keyword */` |
|    23415 | 10421 | `							nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    23415 | 10422 | `							if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      ! 0 | 10423 | `								iProtection = nKwrd;` |
|      ! 0 | 10424 | `								pGen->pIn++; /* Jump the visibility token */` |
|      ! 0 | 10425 | `								nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|      ! 0 | 10426 | `								if( nSetVis ){` |
|      ! 0 | 10427 | `									iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|      ! 0 | 10428 | `									pGen->pIn += nSetTok;` |
|      ! 0 | 10429 | `								}` |
|      ! 0 | 10430 | `							}` |
|        - | 10431 | `						}` |
|    11706 | 10432 | `					}` |
|        - | 10433 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|        - | 10434 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|        - | 10435 | `					 * than a generic "expecting method" parse error. */` |
|    23445 | 10436 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      ! 0 | 10437 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      ! 0 | 10438 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|      ! 0 | 10439 | `					}` |
|    23440 | 10440 | `					if( pGen->pIn >= pGen->pEnd` |
|    23445 | 10441 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 10442 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10443 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|      ! 0 | 10444 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 10445 | `						if( rc == SXERR_ABORT ){` |
|        - | 10446 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 10447 | `							return SXERR_ABORT;` |
|        - | 10448 | `						}` |
|      ! 0 | 10449 | `						goto done;` |
|        - | 10450 | `					}` |
|    23445 | 10451 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        - | 10452 | `						/* Attribute declaration */` |
|       29 | 10453 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       29 | 10454 | `						if( rc != SXRET_OK ){` |
|        3 | 10455 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 10456 | `								return SXERR_ABORT;` |
|        - | 10457 | `							}` |
|        3 | 10458 | `							goto done;` |
|        - | 10459 | `						}` |
|       26 | 10460 | `						continue;` |
|        - | 10461 | `					}` |
|    23419 | 10462 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        - | 10463 | `						/* Typed static attribute declaration */` |
|       17 | 10464 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       17 | 10465 | `						if( rc != SXRET_OK ){` |
|        3 | 10466 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 10467 | `								return SXERR_ABORT;` |
|        - | 10468 | `							}` |
|        3 | 10469 | `							goto done;` |
|        - | 10470 | `						}` |
|       15 | 10471 | `						continue;` |
|        - | 10472 | `					}` |
|        - | 10473 | `					/* Extract the keyword */` |
|    23405 | 10474 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1283067 | 10475 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|        - | 10476 | `					/* Abstract method,record that */` |
|       15 | 10477 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|        - | 10478 | `					/* Mark the whole class as abstract */` |
|       15 | 10479 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|        - | 10480 | `					/* Advance the stream cursor */` |
|       15 | 10481 | `					pGen->pIn++;` |
|       15 | 10482 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       15 | 10483 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       15 | 10484 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       13 | 10485 | `							iProtection = nKwrd;` |
|       13 | 10486 | `							pGen->pIn++; /* Jump the visibility token */` |
|        5 | 10487 | `						}` |
|        6 | 10488 | `					}` |
|       15 | 10489 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       12 | 10490 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - | 10491 | `							/* Static method */` |
|      ! 0 | 10492 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 | 10493 | `							pGen->pIn++; /* Jump the static keyword */` |
|      ! 0 | 10494 | `					}` |
|       15 | 10495 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       12 | 10496 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 10497 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10498 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|      ! 0 | 10499 | `								&pGen->pIn->sData,pName);` |
|      ! 0 | 10500 | `							if( rc == SXERR_ABORT ){` |
|        - | 10501 | `								/* Error count limit reached,abort immediately */` |
|      ! 0 | 10502 | `								return SXERR_ABORT;` |
|        - | 10503 | `							}` |
|      ! 0 | 10504 | `							goto done;` |
|        - | 10505 | `					}` |
|       15 | 10506 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  1271361 | 10507 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|        - | 10508 | `					/* final method ,record that */` |
|       21 | 10509 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       21 | 10510 | `					pGen->pIn++; /* Jump the final keyword */` |
|       21 | 10511 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 10512 | `						/* Extract the keyword */` |
|       21 | 10513 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       21 | 10514 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       11 | 10515 | `							iProtection = nKwrd;` |
|       11 | 10516 | `							pGen->pIn++; /* Jump the visibility token */` |
|        4 | 10517 | `						}` |
|        9 | 10518 | `					}` |
|       21 | 10519 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       18 | 10520 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|        - | 10521 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|        - | 10522 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|        - | 10523 | `							 * child class is compiled (PH7_ClassInherit). */` |
|       14 | 10524 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|       14 | 10525 | `							if( rc != SXRET_OK ){` |
|      ! 0 | 10526 | `								if( rc == SXERR_ABORT ){` |
|      ! 0 | 10527 | `									return SXERR_ABORT;` |
|        - | 10528 | `								}` |
|      ! 0 | 10529 | `								goto done;` |
|        - | 10530 | `							}` |
|       14 | 10531 | `							continue;` |
|        - | 10532 | `					}` |
|        9 | 10533 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        6 | 10534 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - | 10535 | `							/* Static method */` |
|      ! 0 | 10536 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 | 10537 | `							pGen->pIn++; /* Jump the static keyword */` |
|      ! 0 | 10538 | `					}` |
|        9 | 10539 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|        6 | 10540 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 10541 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10542 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|      ! 0 | 10543 | `								&pGen->pIn->sData,pName);` |
|      ! 0 | 10544 | `							if( rc == SXERR_ABORT ){` |
|        - | 10545 | `								/* Error count limit reached,abort immediately */` |
|      ! 0 | 10546 | `								return SXERR_ABORT;` |
|        - | 10547 | `							}` |
|      ! 0 | 10548 | `							goto done;` |
|        - | 10549 | `					}` |
|        9 | 10550 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|        3 | 10551 | `				}` |
|  1294755 | 10552 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|      ! 0 | 10553 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10554 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|      ! 0 | 10555 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 10556 | `						if( rc == SXERR_ABORT ){` |
|        - | 10557 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 10558 | `							return SXERR_ABORT;` |
|        - | 10559 | `						}` |
|      ! 0 | 10560 | `						goto done;` |
|        - | 10561 | `				}` |
|  1294755 | 10562 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|        7 | 10563 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|        7 | 10564 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|      ! 0 | 10565 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10566 | `							"Expecting attribute declaration after 'var' keyword");` |
|      ! 0 | 10567 | `						if( rc == SXERR_ABORT ){` |
|        - | 10568 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 10569 | `							return SXERR_ABORT;` |
|        - | 10570 | `						}` |
|      ! 0 | 10571 | `						goto done;` |
|        - | 10572 | `					}` |
|        - | 10573 | `					/* Attribute declaration */` |
|        7 | 10574 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        4 | 10575 | `				}else{` |
|        - | 10576 | `					/* Process method declaration */` |
|  1294749 | 10577 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|        - | 10578 | `				}` |
|  1294755 | 10579 | `				if( rc != SXRET_OK ){` |
|       16 | 10580 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10581 | `						return SXERR_ABORT;` |
|        - | 10582 | `					}` |
|       16 | 10583 | `					goto done;` |
|        - | 10584 | `				}` |
|        - | 10585 | `			}` |
|   719299 | 10586 | `		}else{` |
|        - | 10587 | `			/* Attribute declaration */` |
|      ! 0 | 10588 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 10589 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 10590 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10591 | `					return SXERR_ABORT;` |
|        - | 10592 | `				}` |
|      ! 0 | 10593 | `				goto done;` |
|        - | 10594 | `			}` |
|        - | 10595 | `		}` |
|        5 | 10596 | `	}` |
|        - | 10597 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|        - | 10598 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|        - | 10599 | `	 */` |
|        - | 10600 | `	{` |
|        - | 10601 | `		TraitUseEntry *apUse;` |
|        - | 10602 | `		sxu32 nU;` |
|   215247 | 10603 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   215305 | 10604 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|       63 | 10605 | `			TraitUseEntry *pUse = &apUse[nU];` |
|       63 | 10606 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|       63 | 10607 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|       63 | 10608 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|        - | 10609 | `			sxu32 nT;` |
|       63 | 10610 | `			if( !hasResolution ){` |
|        - | 10611 | `				/* No conflict resolution block: use standard trait application */` |
|      107 | 10612 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       59 | 10613 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|       59 | 10614 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 10615 | `						break;` |
|        - | 10616 | `					}` |
|       32 | 10617 | `				}` |
|       29 | 10618 | `			}else{` |
|        - | 10619 | `				/* With resolution block: copy attributes, record traits,` |
|        - | 10620 | `				 * then use the block to resolve method conflicts.` |
|        - | 10621 | `				 */` |
|        - | 10622 | `				SyToken *pR;` |
|       25 | 10623 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       15 | 10624 | `					ph7_class *pTR = apTrait[nT];` |
|        - | 10625 | `					ph7_class_attr *pAR;` |
|        - | 10626 | `					SyHashEntry *pER;` |
|        - | 10627 | `					SyString *pNR;` |
|       15 | 10628 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|       21 | 10629 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|      ! 0 | 10630 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|      ! 0 | 10631 | `						pNR = &pAR->sName;` |
|      ! 0 | 10632 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      ! 0 | 10633 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|      ! 0 | 10634 | `						}` |
|      ! 0 | 10635 | `					}` |
|       15 | 10636 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|        9 | 10637 | `				}` |
|        - | 10638 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       13 | 10639 | `				pR = pUse->pResolvStart;` |
|       27 | 10640 | `				while( pR < pUse->pResolvEnd ){` |
|        - | 10641 | `					SyString sTrait,sMethod;` |
|        - | 10642 | `					ph7_class *pSrcTrait;` |
|        - | 10643 | `					ph7_class_method *pMeth;` |
|        - | 10644 | `					sxi32 nRKwrd;` |
|       41 | 10645 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|       27 | 10646 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       17 | 10647 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       17 | 10648 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       17 | 10649 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       17 | 10650 | `					sMethod = pR->sData;` |
|       17 | 10651 | `					pR++;` |
|       17 | 10652 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        7 | 10653 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        7 | 10654 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        7 | 10655 | `							sTrait = sMethod;` |
|        7 | 10656 | `							pR++;` |
|        7 | 10657 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        7 | 10658 | `							sMethod = pR->sData;` |
|        7 | 10659 | `							pR++;` |
|        3 | 10660 | `						}` |
|        3 | 10661 | `					}` |
|       17 | 10662 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 10663 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 10664 | `						continue;` |
|        - | 10665 | `					}` |
|       17 | 10666 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       17 | 10667 | `					pR++;` |
|       17 | 10668 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|        5 | 10669 | `						pSrcTrait = 0;` |
|        7 | 10670 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        7 | 10671 | `							SyString *pTN = &apTrait[nT]->sName;` |
|       10 | 10672 | `							if( pTN->nByte >= sTrait.nByte &&` |
|        6 | 10673 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|        5 | 10674 | `								pSrcTrait = apTrait[nT];` |
|        5 | 10675 | `								break;` |
|        - | 10676 | `							}` |
|        2 | 10677 | `						}` |
|        5 | 10678 | `						if( pSrcTrait ){` |
|        5 | 10679 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        5 | 10680 | `							if( pMeth ){` |
|        5 | 10681 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|        5 | 10682 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|        5 | 10683 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|        2 | 10684 | `								}` |
|        2 | 10685 | `							}` |
|        2 | 10686 | `						}` |
|        2 | 10687 | `					}` |
|       35 | 10688 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        3 | 10689 | `				}` |
|        - | 10690 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|       25 | 10691 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        - | 10692 | `					ph7_class_method *pMR;` |
|        - | 10693 | `					SyHashEntry *pER;` |
|        - | 10694 | `					SyString *pNR;` |
|       15 | 10695 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|       41 | 10696 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|       23 | 10697 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|       23 | 10698 | `						pNR = &pMR->sFunc.sName;` |
|       23 | 10699 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       14 | 10700 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|        6 | 10701 | `						}` |
|        3 | 10702 | `					}` |
|        9 | 10703 | `				}` |
|        - | 10704 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       13 | 10705 | `				pR = pUse->pResolvStart;` |
|       27 | 10706 | `				while( pR < pUse->pResolvEnd ){` |
|        - | 10707 | `					SyString sTrait,sMethod,sAlias;` |
|        - | 10708 | `					ph7_class *pSrcTrait;` |
|        - | 10709 | `					ph7_class_method *pMeth;` |
|       27 | 10710 | `					int hasQual = 0;` |
|        - | 10711 | `					sxi32 nRKwrd;` |
|       41 | 10712 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|       27 | 10713 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       17 | 10714 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       17 | 10715 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       17 | 10716 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|       17 | 10717 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       17 | 10718 | `					sMethod = pR->sData;` |
|       17 | 10719 | `					pR++;` |
|       17 | 10720 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        7 | 10721 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        7 | 10722 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        7 | 10723 | `							sTrait = sMethod;` |
|        7 | 10724 | `							hasQual = 1;` |
|        7 | 10725 | `							pR++;` |
|        7 | 10726 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        7 | 10727 | `							sMethod = pR->sData;` |
|        7 | 10728 | `							pR++;` |
|        3 | 10729 | `						}` |
|        3 | 10730 | `					}` |
|       17 | 10731 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 10732 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 10733 | `						continue;` |
|        - | 10734 | `					}` |
|       17 | 10735 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       17 | 10736 | `					pR++;` |
|       17 | 10737 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       13 | 10738 | `						sxi32 iNewVis = -1;` |
|       13 | 10739 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|        7 | 10740 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|        7 | 10741 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|        7 | 10742 | `								iNewVis = nAK;` |
|        7 | 10743 | `								pR++;` |
|        3 | 10744 | `							}` |
|        3 | 10745 | `						}` |
|       13 | 10746 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       11 | 10747 | `							sAlias = pR->sData;` |
|       11 | 10748 | `							pR++;` |
|        4 | 10749 | `						}` |
|       13 | 10750 | `						pMeth = 0;` |
|       13 | 10751 | `						if( hasQual ){` |
|        3 | 10752 | `							pSrcTrait = 0;` |
|        5 | 10753 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        5 | 10754 | `								SyString *pTN = &apTrait[nT]->sName;` |
|        7 | 10755 | `								if( pTN->nByte >= sTrait.nByte &&` |
|        4 | 10756 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|        3 | 10757 | `									pSrcTrait = apTrait[nT];` |
|        3 | 10758 | `									break;` |
|        - | 10759 | `								}` |
|        2 | 10760 | `							}` |
|        3 | 10761 | `							if( pSrcTrait ){` |
|        3 | 10762 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        1 | 10763 | `							}` |
|        2 | 10764 | `						}else{` |
|       10 | 10765 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|        - | 10766 | `						}` |
|       13 | 10767 | `						if( pMeth ){` |
|       13 | 10768 | `							if( sAlias.nByte > 0 ){` |
|        - | 10769 | `								/* Create a shallow copy of the method struct for the alias` |
|        - | 10770 | `								 * so it can carry its own visibility without affecting the original.` |
|        - | 10771 | `								 */` |
|        - | 10772 | `								ph7_class_method *pAlias;` |
|        - | 10773 | `								char *zAliasDup;` |
|       11 | 10774 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       11 | 10775 | `								if( pAlias ){` |
|       11 | 10776 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       11 | 10777 | `									if( iNewVis >= 0 ){` |
|        5 | 10778 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|        3 | 10779 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|      ! 0 | 10780 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|        2 | 10781 | `									}` |
|       11 | 10782 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       11 | 10783 | `									if( zAliasDup ){` |
|       11 | 10784 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|        4 | 10785 | `									}` |
|        7 | 10786 | `								}` |
|        7 | 10787 | `							}else if( iNewVis >= 0 ){` |
|        - | 10788 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|        - | 10789 | `								ph7_class_method *pCopy;` |
|        3 | 10790 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        3 | 10791 | `								if( pCopy ){` |
|        3 | 10792 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|        3 | 10793 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|        3 | 10794 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|        3 | 10795 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|      ! 0 | 10796 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|        - | 10797 | `									/* Replace the method in the class hash */` |
|        3 | 10798 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|        3 | 10799 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|        1 | 10800 | `								}` |
|        1 | 10801 | `							}` |
|        5 | 10802 | `						}` |
|        5 | 10803 | `						SXUNUSED(hasQual);` |
|        5 | 10804 | `					}` |
|       21 | 10805 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        3 | 10806 | `				}` |
|        - | 10807 | `			}` |
|       63 | 10808 | `			SySetRelease(&pUse->aTraits);` |
|       34 | 10809 | `		}` |
|        - | 10810 | `	}` |
|   215247 | 10811 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|        - | 10812 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|        - | 10813 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|       27 | 10814 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|       27 | 10815 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10816 | `			SySetRelease(&aUseEntries);` |
|      ! 0 | 10817 | `			SySetRelease(&aInterfaces);` |
|      ! 0 | 10818 | `			return SXERR_ABORT;` |
|        - | 10819 | `		}` |
|       12 | 10820 | `	}` |
|        - | 10821 | `	/* Install the class */` |
|   215247 | 10822 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   215247 | 10823 | `	if( rc == SXRET_OK ){` |
|        - | 10824 | `		ph7_class **apInterface;` |
|        - | 10825 | `		sxu32 n;` |
|   215247 | 10826 | `		if( pBase ){` |
|        - | 10827 | `			/* Inherit from base class and mark as a subclass */` |
|   124517 | 10828 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|    62256 | 10829 | `		}` |
|   215247 | 10830 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   262045 | 10831 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|        - | 10832 | `			/* Implements one or more interface */` |
|    46803 | 10833 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    46803 | 10834 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 10835 | `				break;` |
|        - | 10836 | `			}` |
|    23404 | 10837 | `		}` |
|        - | 10838 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|        - | 10839 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|   215247 | 10840 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|       27 | 10841 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|       27 | 10842 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 10843 | `				pIntf = pIntf->pNextName;` |
|      ! 0 | 10844 | `			}` |
|       27 | 10845 | `			if( pIntf ){` |
|       27 | 10846 | `				PH7_ClassImplement(pClass,pIntf);` |
|       12 | 10847 | `			}` |
|       27 | 10848 | `			if( pClass->nEnumBacking != 0 ){` |
|       13 | 10849 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|       13 | 10850 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 10851 | `					pIntf = pIntf->pNextName;` |
|      ! 0 | 10852 | `				}` |
|       13 | 10853 | `				if( pIntf ){` |
|       13 | 10854 | `					PH7_ClassImplement(pClass,pIntf);` |
|        6 | 10855 | `				}` |
|        6 | 10856 | `			}` |
|       12 | 10857 | `		}` |
|        - | 10858 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|        - | 10859 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|   215242 | 10860 | `		if( rc == SXRET_OK` |
|   215242 | 10861 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|   215247 | 10862 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   171003 | 10863 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|        - | 10864 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   171003 | 10865 | `			if( pStringable ){` |
|   171003 | 10866 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   171003 | 10867 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|        - | 10868 | `				sxu32 i;` |
|   171003 | 10869 | `				int bAlready = 0;` |
|   209847 | 10870 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    42735 | 10871 | `					if( apImpl[i] == pStringable ){` |
|     3891 | 10872 | `						bAlready = 1;` |
|     3891 | 10873 | `						break;` |
|        - | 10874 | `					}` |
|    19427 | 10875 | `				}` |
|   171003 | 10876 | `				if( !bAlready ){` |
|   167117 | 10877 | `					PH7_ClassImplement(pClass,pStringable);` |
|    83556 | 10878 | `				}` |
|    85499 | 10879 | `			}` |
|    85499 | 10880 | `		}` |
|        - | 10881 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   215247 | 10882 | `		if( rc == SXRET_OK ){` |
|   215247 | 10883 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   215247 | 10884 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 10885 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 10886 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 10887 | `				return SXERR_ABORT;` |
|        - | 10888 | `			}` |
|   107621 | 10889 | `		}` |
|        - | 10890 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   215247 | 10891 | `		if( rc == SXRET_OK ){` |
|   215247 | 10892 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   215247 | 10893 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 10894 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 10895 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 10896 | `				return SXERR_ABORT;` |
|        - | 10897 | `			}` |
|   107621 | 10898 | `		}` |
|   107621 | 10899 | `	}` |
|   215247 | 10900 | `	SySetRelease(&aUseEntries);` |
|   215247 | 10901 | `	SySetRelease(&aInterfaces);` |
|   215247 | 10902 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10903 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 10904 | `		return SXERR_ABORT;` |
|        - | 10905 | `	}` |
|   107621 | 10906 | `done:` |
|        - | 10907 | `	/* Point beyond the class body */` |
|   215289 | 10908 | `	pGen->pIn = &pEnd[1];` |
|   215289 | 10909 | `	pGen->pEnd = pTmp;` |
|   215289 | 10910 | `	return PH7_OK;` |
|   107648 | 10911 | `}` |
|        - | 10912 | `/* Compile a named class declaration (the common case). */` |
|   215258 | 10913 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|        5 | 10914 | `{` |
|   215263 | 10915 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|        5 | 10916 | `}` |
|        - | 10917 | `/*` |
|        - | 10918 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|        - | 10919 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|        - | 10920 | ` * compile + install the class body once (at compile time, like every other` |
|        - | 10921 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|        - | 10922 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|        - | 10923 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|        - | 10924 | ` */` |
|       28 | 10925 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        4 | 10926 | `{` |
|        - | 10927 | `	char zName[128];         /* Synthesized class name */` |
|        - | 10928 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|        - | 10929 | `	SyString sName;` |
|        - | 10930 | `	SyToken *pArgStart,*pArgEnd;` |
|       32 | 10931 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|        - | 10932 | `	                              * is keyed to this 'class' token */` |
|        - | 10933 | `	ph7_value *pObj;` |
|       32 | 10934 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 10935 | `	sxu32 nIdx,nLen;` |
|        - | 10936 | `	sxi32 nArg,rc;` |
|       14 | 10937 | `	SXUNUSED(iCompileFlag);` |
|        - | 10938 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|       32 | 10939 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|       32 | 10940 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 | 10941 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      ! 0 | 10942 | `	}` |
|       32 | 10943 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|        - | 10944 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|        - | 10945 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|        - | 10946 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|       32 | 10947 | `	pArgStart = pArgEnd = 0;` |
|       32 | 10948 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|       32 | 10949 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10950 | `		return rc;` |
|        - | 10951 | `	}` |
|        - | 10952 | `	{` |
|        - | 10953 | ``		/* Expression-position attributes (`new #[A] class {…}`) */`` |
|       32 | 10954 | `		ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0);` |
|       28 | 10955 | `		if( pAnonClass` |
|       32 | 10956 | `		 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 10957 | `			return SXERR_ABORT;` |
|        - | 10958 | `		}` |
|        - | 10959 | `	}` |
|        - | 10960 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|        - | 10961 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|       32 | 10962 | `	nArg = 0;` |
|       32 | 10963 | `	if( pArgStart < pArgEnd ){` |
|        7 | 10964 | `		SyToken *pSavedIn = pGen->pIn;` |
|        7 | 10965 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|        - | 10966 | `		SyToken *pArgNext;` |
|        7 | 10967 | `		pGen->pIn = pArgStart;` |
|        7 | 10968 | `		pGen->pEnd = pArgEnd;` |
|       13 | 10969 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|        7 | 10970 | `			if( pGen->pIn < pArgNext ){` |
|        7 | 10971 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|        7 | 10972 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10973 | `					pGen->pIn = pSavedIn;` |
|      ! 0 | 10974 | `					pGen->pEnd = pSavedEnd;` |
|      ! 0 | 10975 | `					return SXERR_ABORT;` |
|        - | 10976 | `				}` |
|        7 | 10977 | `				nArg++;` |
|        3 | 10978 | `			}` |
|        7 | 10979 | `			pGen->pIn = &pArgNext[1];` |
|        1 | 10980 | `		}` |
|        7 | 10981 | `		pGen->pIn = pSavedIn;` |
|        7 | 10982 | `		pGen->pEnd = pSavedEnd;` |
|        3 | 10983 | `	}` |
|        - | 10984 | `	/* Load the synthesized class name */` |
|       32 | 10985 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       32 | 10986 | `	if( pObj == 0 ){` |
|      ! 0 | 10987 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 10988 | `		return SXERR_ABORT;` |
|        - | 10989 | `	}` |
|       32 | 10990 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       32 | 10991 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - | 10992 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|       32 | 10993 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       32 | 10994 | `	return SXRET_OK;` |
|       18 | 10995 | `}` |
|        - | 10996 | `/*` |
|        - | 10997 | ` * Compile a user-defined abstract class.` |
|        - | 10998 | ` *  According to the PHP language reference manual` |
|        - | 10999 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|        - | 11000 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|        - | 11001 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|        - | 11002 | ` *   the method's signature - they cannot define the implementation.` |
|        - | 11003 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|        - | 11004 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|        - | 11005 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|        - | 11006 | ` *   method is defined as protected, the function implementation must be defined as either` |
|        - | 11007 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|        - | 11008 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|        - | 11009 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|        - | 11010 | ` *   could differ.` |
|        - | 11011 | ` */` |
|        - | 11012 | `/*` |
|        - | 11013 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|        - | 11014 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|        - | 11015 | ` * receives the corresponding PH7_CLASS_* bit.` |
|        - | 11016 | ` */` |
|  6332844 | 11017 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|        5 | 11018 | `{` |
|  6332849 | 11019 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  3939477 | 11020 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  3939477 | 11021 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  3892843 | 11022 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  1938614 | 11023 | `	}` |
|  6270605 | 11024 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  6270545 | 11025 | `	return FALSE;` |
|  3166427 | 11026 | `}` |
|        - | 11027 | `/*` |
|        - | 11028 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|        - | 11029 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|        - | 11030 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|        - | 11031 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|        - | 11032 | ` */` |
|  6270540 | 11033 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|        5 | 11034 | `{` |
|  6270545 | 11035 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  6270545 | 11036 | `	sxi32 iFlags = 0,iFlag;` |
|  6332849 | 11037 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|    62309 | 11038 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|        5 | 11039 | `			pDup = pIn;` |
|        2 | 11040 | `		}` |
|    62309 | 11041 | `		iFlags \|= iFlag;` |
|    62309 | 11042 | `		pIn++;` |
|        5 | 11043 | `	}` |
|  6270545 | 11044 | `	*ppIn = pIn;` |
|  6270545 | 11045 | `	if( ppDup ){ *ppDup = pDup; }` |
|  6270545 | 11046 | `	return iFlags;` |
|        5 | 11047 | `}` |
|        - | 11048 | `/*` |
|        - | 11049 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|        - | 11050 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|        - | 11051 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|        - | 11052 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|        - | 11053 | `` * `readonly`) to their existing handlers.`` |
|        - | 11054 | ` */` |
|  6243282 | 11055 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|        5 | 11056 | `{` |
|  6243287 | 11057 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  3156674 | 11058 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  6260797 | 11059 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|        5 | 11060 | `}` |
|        - | 11061 | `/*` |
|        - | 11062 | ` * Compile a class declaration carrying one or more leading modifiers` |
|        - | 11063 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|        - | 11064 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|        - | 11065 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|        - | 11066 | `` * `abstract`+`final` pair, like PHP.`` |
|        - | 11067 | ` */` |
|    27258 | 11068 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|        5 | 11069 | `{` |
|        - | 11070 | `	SyToken *pDup;` |
|    27263 | 11071 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|        - | 11072 | `	sxi32 rc;` |
|    27263 | 11073 | `	if( pDup ){` |
|        4 | 11074 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|        2 | 11075 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|        3 | 11076 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11077 | `			return SXERR_ABORT;` |
|        - | 11078 | `		}` |
|        1 | 11079 | `	}` |
|    27258 | 11080 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|    13634 | 11081 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|        3 | 11082 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11083 | `			"Cannot use the final modifier on an abstract class");` |
|        3 | 11084 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11085 | `			return SXERR_ABORT;` |
|        - | 11086 | `		}` |
|        1 | 11087 | `	}` |
|    27263 | 11088 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|    13634 | 11089 | `}` |
|        - | 11090 | `/*` |
|        - | 11091 | ` * Compile a user-defined trait.` |
|        - | 11092 | ` *  Traits are similar to classes, but only intended to group functionality` |
|        - | 11093 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|        - | 11094 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|        - | 11095 | ` */` |
|       72 | 11096 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|        5 | 11097 | `{` |
|       77 | 11098 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11099 | `	ph7_class *pClass;` |
|        - | 11100 | `	SyToken *pEnd,*pTmp;` |
|        - | 11101 | `	sxi32 iProtection;` |
|        - | 11102 | `	sxi32 iAttrflags;` |
|        - | 11103 | `	SyString *pName;` |
|        - | 11104 | `	sxi32 nKwrd;` |
|        - | 11105 | `	sxi32 rc;` |
|        - | 11106 | `	/* Jump the 'trait' keyword */` |
|       77 | 11107 | `	pGen->pIn++;` |
|       77 | 11108 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 11109 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|      ! 0 | 11110 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11111 | `			return SXERR_ABORT;` |
|        - | 11112 | `		}` |
|      ! 0 | 11113 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|      ! 0 | 11114 | `			pGen->pIn++;` |
|      ! 0 | 11115 | `		}` |
|      ! 0 | 11116 | `		return SXRET_OK;` |
|        - | 11117 | `	}` |
|        - | 11118 | `	/* Extract trait name */` |
|       77 | 11119 | `	pName = &pGen->pIn->sData;` |
|       77 | 11120 | `	pGen->pIn++;` |
|        - | 11121 | `	/* Build FQN and obtain a raw class */ {` |
|        - | 11122 | `		SyBlob sFQN;` |
|        - | 11123 | `		SyString sFQNStr;` |
|       77 | 11124 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       77 | 11125 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|       77 | 11126 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       77 | 11127 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|       77 | 11128 | `		SyBlobRelease(&sFQN);` |
|        - | 11129 | `	}` |
|       77 | 11130 | `	if( pClass == 0 ){` |
|      ! 0 | 11131 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 11132 | `		return SXERR_ABORT;` |
|        - | 11133 | `	}` |
|       77 | 11134 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|       77 | 11135 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 11136 | `		return SXERR_ABORT;` |
|        - | 11137 | `	}` |
|        - | 11138 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|       77 | 11139 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|      ! 0 | 11140 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|      ! 0 | 11141 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 11142 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11143 | `			return SXERR_ABORT;` |
|        - | 11144 | `		}` |
|      ! 0 | 11145 | `		return SXRET_OK;` |
|        - | 11146 | `	}` |
|       77 | 11147 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|       77 | 11148 | `	pEnd = 0;` |
|       77 | 11149 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|       77 | 11150 | `	if( pEnd >= pGen->pEnd ){` |
|      ! 0 | 11151 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|      ! 0 | 11152 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 11153 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11154 | `			return SXERR_ABORT;` |
|        - | 11155 | `		}` |
|      ! 0 | 11156 | `		return SXRET_OK;` |
|        - | 11157 | `	}` |
|        - | 11158 | `	/* The delimiter token is the trait body's closing brace */` |
|       77 | 11159 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 11160 | `	/* Swap token stream */` |
|       77 | 11161 | `	pTmp = pGen->pEnd;` |
|       77 | 11162 | `	pGen->pEnd = pEnd;` |
|        - | 11163 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|       77 | 11164 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|        - | 11165 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|       71 | 11166 | `	for(;;){` |
|      191 | 11167 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       28 | 11168 | `			pGen->pIn++;` |
|        4 | 11169 | `		}` |
|      167 | 11170 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       77 | 11171 | `			break;` |
|        - | 11172 | `		}` |
|        - | 11173 | `		/* Bind a directly-preceding docblock to this member */` |
|       95 | 11174 | `		GenStateSetPendingDoc(&(*pGen));` |
|       95 | 11175 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|      ! 0 | 11176 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11177 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|      ! 0 | 11178 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 11179 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11180 | `				return SXERR_ABORT;` |
|        - | 11181 | `			}` |
|      ! 0 | 11182 | `			goto done;` |
|        - | 11183 | `		}` |
|       95 | 11184 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|       95 | 11185 | `		iAttrflags = 0;` |
|       95 | 11186 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       95 | 11187 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       95 | 11188 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - | 11189 | `				/* Trait uses another trait: use OtherTrait; */` |
|        5 | 11190 | `				pGen->pIn++; /* Jump 'use' */` |
|        2 | 11191 | `				for(;;){` |
|        - | 11192 | `					ph7_class *pUsedTrait;` |
|        - | 11193 | `					SyString *pUsedName;` |
|        5 | 11194 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 11195 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 11196 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|      ! 0 | 11197 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11198 | `							return SXERR_ABORT;` |
|        - | 11199 | `						}` |
|      ! 0 | 11200 | `						break;` |
|        - | 11201 | `					}` |
|        5 | 11202 | `					pUsedName = &pGen->pIn->sData;` |
|        - | 11203 | `					{` |
|        - | 11204 | `						SyBlob sResolved;` |
|        5 | 11205 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|        5 | 11206 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|        7 | 11207 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|        4 | 11208 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|        5 | 11209 | `						SyBlobRelease(&sResolved);` |
|        - | 11210 | `					}` |
|        5 | 11211 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 | 11212 | `						pUsedTrait = pUsedTrait->pNextName;` |
|      ! 0 | 11213 | `					}` |
|        5 | 11214 | `					if( pUsedTrait == 0 ){` |
|        4 | 11215 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        1 | 11216 | `							"'%z' is not a trait",pUsedName);` |
|        3 | 11217 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11218 | `							return SXERR_ABORT;` |
|        - | 11219 | `						}` |
|        2 | 11220 | `					}else{` |
|        3 | 11221 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|        - | 11222 | `					}` |
|        5 | 11223 | `					pGen->pIn++;` |
|        5 | 11224 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|        3 | 11225 | `						break;` |
|        - | 11226 | `					}` |
|      ! 0 | 11227 | `					pGen->pIn++;` |
|      ! 0 | 11228 | `				}` |
|        5 | 11229 | `				continue;` |
|        - | 11230 | `			}` |
|       91 | 11231 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       77 | 11232 | `				iProtection = nKwrd;` |
|       77 | 11233 | `				pGen->pIn++;` |
|       72 | 11234 | `				if( pGen->pIn >= pGen->pEnd` |
|       77 | 11235 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 11236 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11237 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|      ! 0 | 11238 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 11239 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 11240 | `						return SXERR_ABORT;` |
|        - | 11241 | `					}` |
|      ! 0 | 11242 | `					goto done;` |
|        - | 11243 | `				}` |
|       77 | 11244 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       12 | 11245 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       12 | 11246 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 11247 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11248 | `							return SXERR_ABORT;` |
|        - | 11249 | `						}` |
|      ! 0 | 11250 | `						goto done;` |
|        - | 11251 | `					}` |
|       12 | 11252 | `					continue;` |
|        - | 11253 | `				}` |
|       67 | 11254 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        5 | 11255 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        5 | 11256 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 11257 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11258 | `							return SXERR_ABORT;` |
|        - | 11259 | `						}` |
|      ! 0 | 11260 | `						goto done;` |
|        - | 11261 | `					}` |
|        5 | 11262 | `					continue;` |
|        - | 11263 | `				}` |
|       63 | 11264 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       29 | 11265 | `			}` |
|       77 | 11266 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|      ! 0 | 11267 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11268 | `					"Traits cannot have constants");` |
|      ! 0 | 11269 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 11270 | `					return SXERR_ABORT;` |
|        - | 11271 | `				}` |
|      ! 0 | 11272 | `				goto done;` |
|      ! 0 | 11273 | `			}else{` |
|       77 | 11274 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|        8 | 11275 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|        8 | 11276 | `					pGen->pIn++;` |
|        8 | 11277 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        6 | 11278 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        6 | 11279 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      ! 0 | 11280 | `							iProtection = nKwrd;` |
|      ! 0 | 11281 | `							pGen->pIn++;` |
|      ! 0 | 11282 | `						}` |
|        2 | 11283 | `					}` |
|        6 | 11284 | `					if( pGen->pIn >= pGen->pEnd` |
|        8 | 11285 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 11286 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11287 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|      ! 0 | 11288 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 11289 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11290 | `							return SXERR_ABORT;` |
|        - | 11291 | `						}` |
|      ! 0 | 11292 | `						goto done;` |
|        - | 11293 | `					}` |
|        8 | 11294 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        3 | 11295 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        3 | 11296 | `						if( rc != SXRET_OK ){` |
|      ! 0 | 11297 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 11298 | `								return SXERR_ABORT;` |
|        - | 11299 | `							}` |
|      ! 0 | 11300 | `							goto done;` |
|        - | 11301 | `						}` |
|        3 | 11302 | `						continue;` |
|        - | 11303 | `					}` |
|        6 | 11304 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|      ! 0 | 11305 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 11306 | `						if( rc != SXRET_OK ){` |
|      ! 0 | 11307 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 11308 | `								return SXERR_ABORT;` |
|        - | 11309 | `							}` |
|      ! 0 | 11310 | `							goto done;` |
|        - | 11311 | `						}` |
|      ! 0 | 11312 | `						continue;` |
|        - | 11313 | `					}` |
|        6 | 11314 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       73 | 11315 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|        6 | 11316 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|        6 | 11317 | `					pGen->pIn++;` |
|        6 | 11318 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        6 | 11319 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        6 | 11320 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        6 | 11321 | `							iProtection = nKwrd;` |
|        6 | 11322 | `							pGen->pIn++;` |
|        2 | 11323 | `						}` |
|        2 | 11324 | `					}` |
|        6 | 11325 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|        4 | 11326 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 11327 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11328 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|      ! 0 | 11329 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 11330 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11331 | `							return SXERR_ABORT;` |
|        - | 11332 | `						}` |
|      ! 0 | 11333 | `						goto done;` |
|        - | 11334 | `					}` |
|        6 | 11335 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|        2 | 11336 | `				}` |
|       75 | 11337 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|      ! 0 | 11338 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11339 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|      ! 0 | 11340 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 11341 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 11342 | `						return SXERR_ABORT;` |
|        - | 11343 | `					}` |
|      ! 0 | 11344 | `					goto done;` |
|        - | 11345 | `				}` |
|       75 | 11346 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|      ! 0 | 11347 | `					pGen->pIn++;` |
|      ! 0 | 11348 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 11349 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11350 | `							"Expecting attribute declaration after 'var' keyword");` |
|      ! 0 | 11351 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11352 | `							return SXERR_ABORT;` |
|        - | 11353 | `						}` |
|      ! 0 | 11354 | `						goto done;` |
|        - | 11355 | `					}` |
|      ! 0 | 11356 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 11357 | `				}else{` |
|       75 | 11358 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|        - | 11359 | `				}` |
|       75 | 11360 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 11361 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 11362 | `						return SXERR_ABORT;` |
|        - | 11363 | `					}` |
|      ! 0 | 11364 | `					goto done;` |
|        - | 11365 | `				}` |
|        - | 11366 | `			}` |
|       40 | 11367 | `		}else{` |
|      ! 0 | 11368 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 11369 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 11370 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 11371 | `					return SXERR_ABORT;` |
|        - | 11372 | `				}` |
|      ! 0 | 11373 | `				goto done;` |
|        - | 11374 | `			}` |
|        - | 11375 | `		}` |
|        5 | 11376 | `	}` |
|        - | 11377 | `	/* Install the trait */` |
|       77 | 11378 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|       77 | 11379 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11380 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 11381 | `		return SXERR_ABORT;` |
|        - | 11382 | `	}` |
|       36 | 11383 | `done:` |
|        - | 11384 | `	/* Point beyond the trait body */` |
|       77 | 11385 | `	pGen->pIn = &pEnd[1];` |
|       77 | 11386 | `	pGen->pEnd = pTmp;` |
|       77 | 11387 | `	return PH7_OK;` |
|       41 | 11388 | `}` |
|        - | 11389 | `/*` |
|        - | 11390 | ` * Compile a user-defined class.` |
|        - | 11391 | ` *  According to the PHP language reference manual` |
|        - | 11392 | ` *   Basic class definitions begin with the keyword class, followed` |
|        - | 11393 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|        - | 11394 | ` *   the definitions of the properties and methods belonging to the class.` |
|        - | 11395 | ` *   A class may contain its own constants, variables (called "properties")` |
|        - | 11396 | ` *   and functions (called "methods").` |
|        - | 11397 | ` */` |
|   187972 | 11398 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|        5 | 11399 | `{` |
|        - | 11400 | `	sxi32 rc;` |
|   187977 | 11401 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   187977 | 11402 | `	return rc;` |
|        5 | 11403 | `}` |
|        - | 11404 | `/*` |
|        - | 11405 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|        - | 11406 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|        - | 11407 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|        - | 11408 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|        - | 11409 | `` * meaning; `enum Name` can never start a valid expression.`` |
|        - | 11410 | ` */` |
|  6208256 | 11411 | `static int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|        5 | 11412 | `{` |
|  6241531 | 11413 | `	return (pIn->nType & PH7_TK_ID)` |
|  3137398 | 11414 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|    37277 | 11415 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
|  6241526 | 11416 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|        5 | 11417 | `}` |
|        - | 11418 | `/*` |
|        - | 11419 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|        - | 11420 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|        - | 11421 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|        - | 11422 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|        - | 11423 | ` */` |
|       28 | 11424 | `static sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|        5 | 11425 | `{` |
|       33 | 11426 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|        5 | 11427 | `}` |
|        - | 11428 | `/*` |
|        - | 11429 | ` * Exception handling.` |
|        - | 11430 | ` *  According to the PHP language reference manual` |
|        - | 11431 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|        - | 11432 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|        - | 11433 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|        - | 11434 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|        - | 11435 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|        - | 11436 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|        - | 11437 | ` *    (or re-thrown) within a catch block.` |
|        - | 11438 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|        - | 11439 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|        - | 11440 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|        - | 11441 | ` *    been defined with set_exception_handler().` |
|        - | 11442 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|        - | 11443 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|        - | 11444 | ` */` |
|        - | 11445 | `/*` |
|        - | 11446 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|        - | 11447 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|        - | 11448 | ` * indicates failure.` |
|        - | 11449 | ` */` |
|   315012 | 11450 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 11451 | `{` |
|   315017 | 11452 | `	sxi32 rc = SXRET_OK;` |
|   315017 | 11453 | `	if( pRoot->pOp ){` |
|   315005 | 11454 | `		switch( pRoot->pOp->iOp ){` |
|   157500 | 11455 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|        - | 11456 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|        - | 11457 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|        - | 11458 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|        - | 11459 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|        - | 11460 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|   315005 | 11461 | `			break;` |
|      ! 0 | 11462 | `		default:` |
|        - | 11463 | `			/* Runtime will still reject non-Throwable values; the set above` |
|        - | 11464 | `			 * covers the common shapes and gives a friendlier compile error` |
|        - | 11465 | ``			 * for obvious mistakes like `throw 5`. */`` |
|      ! 0 | 11466 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 11467 | `				"throw: Expecting an exception class instance");` |
|      ! 0 | 11468 | `			if( rc != SXERR_ABORT ){` |
|      ! 0 | 11469 | `				rc = SXERR_INVALID;` |
|      ! 0 | 11470 | `			}` |
|      ! 0 | 11471 | `			break;` |
|        - | 11472 | `		}` |
|   157517 | 11473 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 11474 | `		/* Unexpected expression */` |
|      ! 0 | 11475 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 11476 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 11477 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 | 11478 | `			rc = SXERR_INVALID;` |
|      ! 0 | 11479 | `		}` |
|      ! 0 | 11480 | `	}` |
|   315017 | 11481 | `	return rc;` |
|        5 | 11482 | `}` |
|        - | 11483 | `/*` |
|        - | 11484 | ` * Compile a 'throw' statement.` |
|        - | 11485 | ` * throw: This is how you trigger an exception.` |
|        - | 11486 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|        - | 11487 | ` */` |
|   314976 | 11488 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|        5 | 11489 | `{` |
|   314981 | 11490 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11491 | `	GenBlock *pBlock;` |
|        - | 11492 | `	sxu32 nIdx;` |
|        - | 11493 | `	sxi32 rc;` |
|   314981 | 11494 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|        - | 11495 | `	/* Compile the expression */` |
|   314981 | 11496 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   314981 | 11497 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 11498 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|      ! 0 | 11499 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11500 | `			return SXERR_ABORT;` |
|        - | 11501 | `		}` |
|      ! 0 | 11502 | `		return SXRET_OK;` |
|        - | 11503 | `	}` |
|   314981 | 11504 | `	pBlock = pGen->pCurrent;` |
|        - | 11505 | `	/* Point to the top most function or try block and emit the forward jump */` |
|  1228113 | 11506 | `	while(pBlock->pParent){` |
|  1228109 | 11507 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   314977 | 11508 | `			break;` |
|        - | 11509 | `		}` |
|        - | 11510 | `		/* Point to the parent block */` |
|   913137 | 11511 | `		pBlock = pBlock->pParent;` |
|        5 | 11512 | `	}` |
|        - | 11513 | `	/* Emit the throw instruction */` |
|   314981 | 11514 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        - | 11515 | `	/* Emit the jump */` |
|   314981 | 11516 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   314981 | 11517 | `	return SXRET_OK;` |
|   157493 | 11518 | `}` |
|        - | 11519 | `/*` |
|        - | 11520 | ` * Compile a PHP 8.0 'throw' expression.` |
|        - | 11521 | ` * Called from the expression code generator when a 'throw' keyword is` |
|        - | 11522 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|        - | 11523 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|        - | 11524 | ` * the validator guarantees the operand is a valid exception target.` |
|        - | 11525 | ` */` |
|       36 | 11526 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        2 | 11527 | `{` |
|       38 | 11528 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11529 | `	GenBlock *pBlock;` |
|        - | 11530 | `	sxu32 nIdx;` |
|        - | 11531 | `	sxi32 rc;` |
|       18 | 11532 | `	(void)iCompileFlag;` |
|       38 | 11533 | `	pGen->pIn++; /* Skip 'throw' */` |
|       38 | 11534 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 11535 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 11536 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 11537 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11538 | `			return SXERR_ABORT;` |
|        - | 11539 | `		}` |
|      ! 0 | 11540 | `		return SXRET_OK;` |
|        - | 11541 | `	}` |
|       38 | 11542 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|       38 | 11543 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 11544 | `		return SXERR_ABORT;` |
|        - | 11545 | `	}` |
|       38 | 11546 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 11547 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 11548 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 11549 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11550 | `			return SXERR_ABORT;` |
|        - | 11551 | `		}` |
|      ! 0 | 11552 | `		return SXRET_OK;` |
|        - | 11553 | `	}` |
|        - | 11554 | `	/* Walk up to nearest exception/function block for the jump target */` |
|       38 | 11555 | `	pBlock = pGen->pCurrent;` |
|       60 | 11556 | `	while( pBlock->pParent ){` |
|       49 | 11557 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|       27 | 11558 | `			break;` |
|        - | 11559 | `		}` |
|       23 | 11560 | `		pBlock = pBlock->pParent;` |
|        1 | 11561 | `	}` |
|       38 | 11562 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       38 | 11563 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|       38 | 11564 | `	return SXRET_OK;` |
|       20 | 11565 | `}` |
|        - | 11566 | `/*` |
|        - | 11567 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|        - | 11568 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|        - | 11569 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|        - | 11570 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|        - | 11571 | ` * compile error propagated from the parser.` |
|        - | 11572 | ` */` |
|       54 | 11573 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|        5 | 11574 | `{` |
|        - | 11575 | `	SyString sClassName;` |
|        - | 11576 | `	SyToken *pToken;` |
|        - | 11577 | `	SyString *pName;` |
|        - | 11578 | `	char *zDup;` |
|        - | 11579 | `	sxi32 rc;` |
|       59 | 11580 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       59 | 11581 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|       59 | 11582 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|       59 | 11583 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       59 | 11584 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 | 11585 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 11586 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11587 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11588 | `		return SXERR_INVALID;` |
|        - | 11589 | `	}` |
|       59 | 11590 | `	pGen->pIn++; /* '(' */` |
|       27 | 11591 | `	for(;;){` |
|        - | 11592 | `		SyBlob sResolved;` |
|       59 | 11593 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       59 | 11594 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 11595 | `			SyBlobRelease(&sResolved);` |
|      ! 0 | 11596 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 11597 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11598 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11599 | `			return SXERR_INVALID;` |
|        - | 11600 | `		}` |
|       86 | 11601 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       54 | 11602 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       59 | 11603 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|       59 | 11604 | `		SyBlobRelease(&sResolved);` |
|       59 | 11605 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|       59 | 11606 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|       59 | 11607 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       54 | 11608 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|        5 | 11609 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      ! 0 | 11610 | `			pGen->pIn++; continue;` |
|        - | 11611 | `		}` |
|       59 | 11612 | `		break;` |
|      ! 0 | 11613 | `	}` |
|       54 | 11614 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|       59 | 11615 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 11616 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 11617 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11618 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11619 | `		return SXERR_INVALID;` |
|        - | 11620 | `	}` |
|       59 | 11621 | `	pGen->pIn++; /* '$' */` |
|       59 | 11622 | `	pName = &pGen->pIn->sData;` |
|       59 | 11623 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       59 | 11624 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|       59 | 11625 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|       59 | 11626 | `	pGen->pIn++;` |
|       59 | 11627 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 11628 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 11629 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11630 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11631 | `		return SXERR_INVALID;` |
|        - | 11632 | `	}` |
|       59 | 11633 | `	pGen->pIn++; /* ')' */` |
|       59 | 11634 | `	return SXRET_OK;` |
|       32 | 11635 | `}` |
|        - | 11636 | `/*` |
|        - | 11637 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|        - | 11638 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|        - | 11639 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|        - | 11640 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|        - | 11641 | ` * VmThrowException):` |
|        - | 11642 | ` *` |
|        - | 11643 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|        - | 11644 | ` *    <try body>` |
|        - | 11645 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|        - | 11646 | ` *    JMP  -> finally\|end` |
|        - | 11647 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|        - | 11648 | ` *    <catch body>` |
|        - | 11649 | ` *    JMP  -> finally\|end` |
|        - | 11650 | ` *    ... more catches ...` |
|        - | 11651 | ` *  Lfin: <finally body>` |
|        - | 11652 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|        - | 11653 | ` *  Lend:` |
|        - | 11654 | ` */` |
|       98 | 11655 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|        5 | 11656 | `{` |
|      103 | 11657 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11658 | `	GenBlock *pTry;` |
|        - | 11659 | `	VmInstr *pInstr;` |
|      103 | 11660 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|        - | 11661 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|        - | 11662 | `	sxi32 rc;` |
|      103 | 11663 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|        - | 11664 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|      103 | 11665 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      103 | 11666 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      103 | 11667 | `	pTry->pUserData = pException;` |
|      103 | 11668 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|      103 | 11669 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|      103 | 11670 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      103 | 11671 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      103 | 11672 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|      103 | 11673 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|        - | 11674 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|      103 | 11675 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|      103 | 11676 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|      103 | 11677 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      103 | 11678 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 11679 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|      103 | 11680 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|        - | 11681 | `	/* Catch clauses (inline) */` |
|      103 | 11682 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       98 | 11683 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       59 | 11684 | `		sxu32 k = 0;` |
|       81 | 11685 | `		for(;;){` |
|        - | 11686 | `			ph7_exception_block sCatch;` |
|        - | 11687 | `			GenBlock *pCatchBlk;` |
|      113 | 11688 | `			sxu32 idxJmp = 0;` |
|      108 | 11689 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      104 | 11690 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|       32 | 11691 | `				break;` |
|        - | 11692 | `			}` |
|       59 | 11693 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|       59 | 11694 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       59 | 11695 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|       59 | 11696 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|       59 | 11697 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|       59 | 11698 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|       59 | 11699 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        - | 11700 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|        - | 11701 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|        - | 11702 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|       59 | 11703 | `			pCatchBlk->pUserData = pException;` |
|       59 | 11704 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|       59 | 11705 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       59 | 11706 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       59 | 11707 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        - | 11708 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|        - | 11709 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|       59 | 11710 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       59 | 11711 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|       59 | 11712 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|       59 | 11713 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|       59 | 11714 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       59 | 11715 | `			k++;` |
|        5 | 11716 | `		}` |
|       27 | 11717 | `	}` |
|        - | 11718 | `	/* Finally (inline) */` |
|      103 | 11719 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       80 | 11720 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 11721 | `		GenBlock *pFinBlk;` |
|       52 | 11722 | `		pGen->pIn++; /* Jump 'finally' */` |
|       52 | 11723 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|       52 | 11724 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|       52 | 11725 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       52 | 11726 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       52 | 11727 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       52 | 11728 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       52 | 11729 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       52 | 11730 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|       52 | 11731 | `		pException->iHasFinally = 1;` |
|       24 | 11732 | `	}` |
|      103 | 11733 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|      103 | 11734 | `	pException->iInlined = 1;` |
|        - | 11735 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|        - | 11736 | `	{` |
|      103 | 11737 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|        - | 11738 | `		sxu32 *aJ; sxu32 n;` |
|      103 | 11739 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|      103 | 11740 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      103 | 11741 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|      157 | 11742 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|       59 | 11743 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|       59 | 11744 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       32 | 11745 | `		}` |
|        - | 11746 | `	}` |
|      103 | 11747 | `	SySetRelease(&aCatchJmp);` |
|      103 | 11748 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|      ! 0 | 11749 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|      ! 0 | 11750 | `	}` |
|      103 | 11751 | `	return SXRET_OK;` |
|       54 | 11752 | `}` |
|        - | 11753 | `/*` |
|        - | 11754 | ` * Compile a 'catch' block.` |
|        - | 11755 | ` * Catch: A "catch" block retrieves an exception and creates` |
|        - | 11756 | ` * an object containing the exception information.` |
|        - | 11757 | ` */` |
|     5222 | 11758 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|        5 | 11759 | `{` |
|     5227 | 11760 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11761 | `	ph7_exception_block sCatch;` |
|        - | 11762 | `	SySet *pInstrContainer;` |
|        - | 11763 | `	SyString sClassName;` |
|        - | 11764 | `	GenBlock *pCatch;` |
|        - | 11765 | `	SyToken *pToken;` |
|        - | 11766 | `	SyString *pName;` |
|        - | 11767 | `	char *zDup;` |
|        - | 11768 | `	sxi32 rc;` |
|     5227 | 11769 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        - | 11770 | `	/* Zero the structure */` |
|     5227 | 11771 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|        - | 11772 | `	/* Initialize fields */` |
|     5227 | 11773 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     5227 | 11774 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     5227 | 11775 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|        - | 11776 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 11777 | `			pToken = pGen->pIn;` |
|      ! 0 | 11778 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 11779 | `				pToken--;` |
|      ! 0 | 11780 | `			}` |
|      ! 0 | 11781 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 11782 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11783 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11784 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11785 | `				return SXERR_ABORT;` |
|        - | 11786 | `			}` |
|      ! 0 | 11787 | `			return SXERR_INVALID;` |
|        - | 11788 | `	}` |
|        - | 11789 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     5227 | 11790 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     2625 | 11791 | `	for(;;){` |
|        - | 11792 | `		SyBlob sResolved;` |
|     5255 | 11793 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     5255 | 11794 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        6 | 11795 | `			SyBlobRelease(&sResolved);` |
|        6 | 11796 | `			pToken = pGen->pIn;` |
|        6 | 11797 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 11798 | `				pToken--;` |
|      ! 0 | 11799 | `			}` |
|        8 | 11800 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 11801 | `				"syntax error, unexpected %s \"%z\"",` |
|        2 | 11802 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|        6 | 11803 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11804 | `				return SXERR_ABORT;` |
|        - | 11805 | `			}` |
|        6 | 11806 | `			return SXERR_INVALID;` |
|        - | 11807 | `		}` |
|        - | 11808 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|        - | 11809 | `		 * transient SyBlob allocation. */` |
|     7874 | 11810 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     5246 | 11811 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     5251 | 11812 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     5251 | 11813 | `		SyBlobRelease(&sResolved);` |
|     5251 | 11814 | `		if( zDup == 0 ){` |
|      ! 0 | 11815 | `			goto Mem;` |
|        - | 11816 | `		}` |
|     5251 | 11817 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     5251 | 11818 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 11819 | `			goto Mem;` |
|        - | 11820 | `		}` |
|        - | 11821 | `		/* Check for '\|' (multi-catch separator) */` |
|     5246 | 11822 | `		if( pGen->pIn < pGen->pEnd &&` |
|     5246 | 11823 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|       33 | 11824 | `			pGen->pIn->sData.nByte == 1 &&` |
|       28 | 11825 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|       30 | 11826 | `			pGen->pIn++; /* Consume the '\|' */` |
|       30 | 11827 | `			continue;` |
|        - | 11828 | `		}` |
|     5223 | 11829 | `		break;` |
|      ! 0 | 11830 | `	}` |
|     5218 | 11831 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     5223 | 11832 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 11833 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 11834 | `			pToken = pGen->pIn;` |
|      ! 0 | 11835 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 11836 | `				pToken--;` |
|      ! 0 | 11837 | `			}` |
|      ! 0 | 11838 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 11839 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11840 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11841 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11842 | `				return SXERR_ABORT;` |
|        - | 11843 | `			}` |
|      ! 0 | 11844 | `			return SXERR_INVALID;` |
|        - | 11845 | `	}` |
|     5223 | 11846 | `	pGen->pIn++; /* Jump the dollar sign */` |
|        - | 11847 | `	/* Duplicate instance name */` |
|     5223 | 11848 | `	pName = &pGen->pIn->sData;` |
|     5223 | 11849 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     5223 | 11850 | `	if( zDup == 0 ){` |
|      ! 0 | 11851 | `		goto Mem;` |
|        - | 11852 | `	}` |
|     5223 | 11853 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     5223 | 11854 | `	pGen->pIn++;` |
|     5223 | 11855 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|        - | 11856 | `		/* Unexpected token,break immediately */` |
|      ! 0 | 11857 | `		pToken = pGen->pIn;` |
|      ! 0 | 11858 | `		if( pToken >= pGen->pEnd ){` |
|      ! 0 | 11859 | `			pToken--;` |
|      ! 0 | 11860 | `		}` |
|      ! 0 | 11861 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 11862 | `			"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11863 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11864 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11865 | `			return SXERR_ABORT;` |
|        - | 11866 | `		}` |
|      ! 0 | 11867 | `		return SXERR_INVALID;` |
|        - | 11868 | `	}` |
|        - | 11869 | `	/* Compile the block */` |
|     5223 | 11870 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|        - | 11871 | `	/* Create the catch block */` |
|     5223 | 11872 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     5223 | 11873 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11874 | `		return SXERR_ABORT;` |
|        - | 11875 | `	}` |
|        - | 11876 | `	/* Swap bytecode container */` |
|     5223 | 11877 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     5223 | 11878 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|        - | 11879 | `	/* Compile the block */` |
|     5223 | 11880 | `	PH7_CompileBlock(&(*pGen),0);` |
|        - | 11881 | `	/* Fix forward jumps now the destination is resolved  */` |
|     5223 | 11882 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 11883 | `	/* Emit the DONE instruction */` |
|     5223 | 11884 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 11885 | `	/* Leave the block */` |
|     5223 | 11886 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 11887 | `	/* Restore the default container */` |
|     5223 | 11888 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 11889 | `	/* Install the catch block */` |
|     5223 | 11890 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     5223 | 11891 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11892 | `		goto Mem;` |
|        - | 11893 | `	}` |
|     5223 | 11894 | `	return SXRET_OK;` |
|      ! 0 | 11895 | `Mem:` |
|      ! 0 | 11896 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 11897 | `	return SXERR_ABORT;` |
|     2616 | 11898 | `}` |
|        - | 11899 | `/*` |
|        - | 11900 | ` * Compile a 'try' block.` |
|        - | 11901 | ` * A function using an exception should be in a "try" block.` |
|        - | 11902 | ` * If the exception does not trigger, the code will continue` |
|        - | 11903 | ` * as normal. However if the exception triggers, an exception` |
|        - | 11904 | ` * is "thrown".` |
|        - | 11905 | ` */` |
|     5378 | 11906 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|        5 | 11907 | `{` |
|        - | 11908 | `	ph7_exception *pException;` |
|     5383 | 11909 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11910 | `	GenBlock *pTry;` |
|        - | 11911 | `	sxu32 nJmpIdx;` |
|        - | 11912 | `	sxi32 rc;` |
|        - | 11913 | `	/* Create the exception container */` |
|     5383 | 11914 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     5383 | 11915 | `	if( pException == 0 ){` |
|      ! 0 | 11916 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 11917 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 11918 | `		return SXERR_ABORT;` |
|        - | 11919 | `	}` |
|        - | 11920 | `	/* Zero the structure */` |
|     5383 | 11921 | `	SyZero(pException,sizeof(ph7_exception));` |
|        - | 11922 | `	/* Initialize fields */` |
|     5383 | 11923 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     5383 | 11924 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     5383 | 11925 | `	pException->iHasFinally = 0;` |
|     5383 | 11926 | `	pException->iFinallyDone = 0;` |
|     5383 | 11927 | `	pException->pVm = pGen->pVm;` |
|        - | 11928 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|        - | 11929 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|        - | 11930 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|        - | 11931 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|        - | 11932 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|        - | 11933 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|     5383 | 11934 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|      103 | 11935 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|        - | 11936 | `	}` |
|        - | 11937 | `	/* Create the try block */` |
|     5285 | 11938 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     5285 | 11939 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11940 | `		return SXERR_ABORT;` |
|        - | 11941 | `	}` |
|        - | 11942 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     5285 | 11943 | `	pTry->pUserData = pException;` |
|        - | 11944 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     5285 | 11945 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|        - | 11946 | `	/* Fix the jump later when the destination is resolved */` |
|     5285 | 11947 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     5285 | 11948 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|        - | 11949 | `	/* Compile the block */` |
|     5285 | 11950 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     5285 | 11951 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 11952 | `		return SXERR_ABORT;` |
|        - | 11953 | `	}` |
|        - | 11954 | `	/* Fix forward jumps now the destination is resolved */` |
|     5285 | 11955 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 11956 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     5285 | 11957 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        - | 11958 | `	/* Leave the block */` |
|     5285 | 11959 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 11960 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     5285 | 11961 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     5278 | 11962 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        - | 11963 | `		/* Compile one or more catch blocks */` |
|     5218 | 11964 | `		for(;;){` |
|    10436 | 11965 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     7873 | 11966 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     2612 | 11967 | `					break;` |
|        - | 11968 | `			}` |
|     5227 | 11969 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     5227 | 11970 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11971 | `				return SXERR_ABORT;` |
|        - | 11972 | `			}` |
|        5 | 11973 | `		}` |
|     2607 | 11974 | `	}` |
|        - | 11975 | `	/* Compile optional finally block */` |
|     5285 | 11976 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      664 | 11977 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 11978 | `		SySet *pInstrContainer;` |
|        - | 11979 | `		GenBlock *pFinBlock;` |
|      129 | 11980 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|        - | 11981 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      129 | 11982 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      129 | 11983 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 11984 | `			return SXERR_ABORT;` |
|        - | 11985 | `		}` |
|        - | 11986 | `		/* Swap bytecode container */` |
|      129 | 11987 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      129 | 11988 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|        - | 11989 | `		/* Compile the finally body */` |
|      129 | 11990 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      129 | 11991 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11992 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 | 11993 | `			return SXERR_ABORT;` |
|        - | 11994 | `		}` |
|        - | 11995 | `		/* Fix forward jumps now the destination is resolved */` |
|      129 | 11996 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 11997 | `		/* Emit DONE to terminate the finally block */` |
|      129 | 11998 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 11999 | `		/* Leave the block */` |
|      129 | 12000 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        - | 12001 | `		/* Restore the default container */` |
|      129 | 12002 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      129 | 12003 | `		pException->iHasFinally = 1;` |
|       62 | 12004 | `	}` |
|        - | 12005 | `	/* Must have at least one catch or finally */` |
|     5285 | 12006 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|        8 | 12007 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 12008 | `			"Cannot use try without catch or finally");` |
|        8 | 12009 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12010 | `			return SXERR_ABORT;` |
|        - | 12011 | `		}` |
|        3 | 12012 | `	}` |
|     5285 | 12013 | `	return SXRET_OK;` |
|     2694 | 12014 | `}` |
|        - | 12015 | `/*` |
|        - | 12016 | ` * Compile a switch block.` |
|        - | 12017 | ` *  (See block-comment below for more information)` |
|        - | 12018 | ` */` |
|      112 | 12019 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|        5 | 12020 | `{` |
|      117 | 12021 | `	sxi32 rc = SXRET_OK;` |
|      117 | 12022 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|        - | 12023 | `		/* Unexpected token */` |
|      ! 0 | 12024 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 12025 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12026 | `			return SXERR_ABORT;` |
|        - | 12027 | `		}` |
|      ! 0 | 12028 | `		pGen->pIn++;` |
|      ! 0 | 12029 | `	}` |
|      117 | 12030 | `	pGen->pIn++;` |
|        - | 12031 | `	/* First instruction to execute in this block. */` |
|      117 | 12032 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|        - | 12033 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|        - | 12034 | `	 * or the '}' token */` |
|      206 | 12035 | `	for(;;){` |
|      417 | 12036 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 12037 | `			/* No more input to process */` |
|      ! 0 | 12038 | `			break;` |
|        - | 12039 | `		}` |
|      417 | 12040 | `		rc = SXRET_OK;` |
|      417 | 12041 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       85 | 12042 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|       31 | 12043 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|        - | 12044 | `					/* Unexpected token */` |
|      ! 0 | 12045 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 12046 | `						&pGen->pIn->sData);` |
|      ! 0 | 12047 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 12048 | `						return SXERR_ABORT;` |
|        - | 12049 | `					}` |
|        - | 12050 | `					/* FALL THROUGH */` |
|      ! 0 | 12051 | `				}` |
|       31 | 12052 | `				rc = SXERR_EOF;` |
|       31 | 12053 | `				break;` |
|        - | 12054 | `			}` |
|       32 | 12055 | `		}else{` |
|        - | 12056 | `			sxi32 nKwrd;` |
|        - | 12057 | `			/* Extract the keyword */` |
|      337 | 12058 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      337 | 12059 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|       47 | 12060 | `				break;` |
|        - | 12061 | `			}` |
|      253 | 12062 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 12063 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|        - | 12064 | `					/* Unexpected token */` |
|      ! 0 | 12065 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 12066 | `						&pGen->pIn->sData);` |
|      ! 0 | 12067 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 12068 | `						return SXERR_ABORT;` |
|        - | 12069 | `					}` |
|        - | 12070 | `					/* FALL THROUGH */` |
|      ! 0 | 12071 | `				}` |
|        - | 12072 | `				/* Block compiled */` |
|        3 | 12073 | `				break;` |
|        - | 12074 | `			}` |
|        - | 12075 | `		}` |
|        - | 12076 | `		/* Compile block */` |
|      305 | 12077 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      305 | 12078 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12079 | `			return SXERR_ABORT;` |
|        - | 12080 | `		}` |
|        5 | 12081 | `	}` |
|      117 | 12082 | `	return rc;` |
|       61 | 12083 | `}` |
|        - | 12084 | `/*` |
|        - | 12085 | ` * Compile a case eXpression.` |
|        - | 12086 | ` *  (See block-comment below for more information)` |
|        - | 12087 | ` */` |
|       92 | 12088 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|        5 | 12089 | `{` |
|        - | 12090 | `	SySet *pInstrContainer;` |
|        - | 12091 | `	SyToken *pEnd,*pTmp;` |
|       97 | 12092 | `	sxi32 iNest = 0;` |
|        - | 12093 | `	sxi32 rc;` |
|        - | 12094 | `	/* Delimit the expression */` |
|       97 | 12095 | `	pEnd = pGen->pIn;` |
|      197 | 12096 | `	while( pEnd < pGen->pEnd ){` |
|      197 | 12097 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|        - | 12098 | `			/* Increment nesting level */` |
|        3 | 12099 | `			iNest++;` |
|      196 | 12100 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|        - | 12101 | `			/* Decrement nesting level */` |
|        3 | 12102 | `			iNest--;` |
|      194 | 12103 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|       97 | 12104 | `			break;` |
|        - | 12105 | `		}` |
|      105 | 12106 | `		pEnd++;` |
|        5 | 12107 | `	}` |
|       97 | 12108 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 12109 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|      ! 0 | 12110 | `		if( rc == SXERR_ABORT ){` |
|        - | 12111 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 12112 | `			return SXERR_ABORT;` |
|        - | 12113 | `		}` |
|      ! 0 | 12114 | `	}` |
|        - | 12115 | `	/* Swap token stream */` |
|       97 | 12116 | `	pTmp = pGen->pEnd;` |
|       97 | 12117 | `	pGen->pEnd = pEnd;` |
|       97 | 12118 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       97 | 12119 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|       97 | 12120 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 12121 | `	/* Emit the done instruction */` |
|       97 | 12122 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       97 | 12123 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 12124 | `	/* Update token stream */` |
|       97 | 12125 | `	pGen->pIn  = pEnd;` |
|       97 | 12126 | `	pGen->pEnd = pTmp;` |
|       97 | 12127 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 12128 | `		return SXERR_ABORT;` |
|        - | 12129 | `	}` |
|       97 | 12130 | `	return SXRET_OK;` |
|       51 | 12131 | `}` |
|        - | 12132 | `/*` |
|        - | 12133 | ` * Compile the smart switch statement.` |
|        - | 12134 | ` * According to the PHP language reference manual` |
|        - | 12135 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|        - | 12136 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|        - | 12137 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|        - | 12138 | ` *  This is exactly what the switch statement is for.` |
|        - | 12139 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|        - | 12140 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|        - | 12141 | ` *  of the outer loop, use continue 2.` |
|        - | 12142 | ` *  Note that switch/case does loose comparision.` |
|        - | 12143 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|        - | 12144 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|        - | 12145 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|        - | 12146 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|        - | 12147 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|        - | 12148 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|        - | 12149 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|        - | 12150 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|        - | 12151 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|        - | 12152 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|        - | 12153 | ` *  list for the next case.` |
|        - | 12154 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|        - | 12155 | ` *  or floating-point numbers and strings.` |
|        - | 12156 | ` */` |
|       28 | 12157 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|        5 | 12158 | `{` |
|        - | 12159 | `	GenBlock *pSwitchBlock;` |
|        - | 12160 | `	SyToken *pTmp,*pEnd;` |
|        - | 12161 | `	ph7_switch *pSwitch;` |
|        - | 12162 | `	sxu32 nToken;` |
|        - | 12163 | `	sxu32 nLine;` |
|        - | 12164 | `	sxi32 rc;` |
|       33 | 12165 | `	nLine = pGen->pIn->nLine;` |
|        - | 12166 | `	/* Jump the 'switch' keyword */` |
|       33 | 12167 | `	pGen->pIn++;` |
|       33 | 12168 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 12169 | `		/* Syntax error */` |
|      ! 0 | 12170 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|      ! 0 | 12171 | `		if( rc == SXERR_ABORT ){` |
|        - | 12172 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 12173 | `			return SXERR_ABORT;` |
|        - | 12174 | `		}` |
|      ! 0 | 12175 | `		goto Synchronize;` |
|        - | 12176 | `	}` |
|        - | 12177 | `	/* Jump the left parenthesis '(' */` |
|       33 | 12178 | `	pGen->pIn++;` |
|       33 | 12179 | `	pEnd = 0; /* cc warning */` |
|        - | 12180 | `	/* Create the loop block */` |
|       47 | 12181 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|       14 | 12182 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|       33 | 12183 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 12184 | `		return SXERR_ABORT;` |
|        - | 12185 | `	}` |
|        - | 12186 | `	/* Delimit the condition */` |
|       33 | 12187 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|       33 | 12188 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 12189 | `		/* Empty expression */` |
|      ! 0 | 12190 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|      ! 0 | 12191 | `		if( rc == SXERR_ABORT ){` |
|        - | 12192 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 12193 | `			return SXERR_ABORT;` |
|        - | 12194 | `		}` |
|      ! 0 | 12195 | `	}` |
|        - | 12196 | `	/* Swap token streams */` |
|       33 | 12197 | `	pTmp = pGen->pEnd;` |
|       33 | 12198 | `	pGen->pEnd = pEnd;` |
|        - | 12199 | `	/* Compile the expression */` |
|       33 | 12200 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       33 | 12201 | `	if( rc == SXERR_ABORT ){` |
|        - | 12202 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 12203 | `		return SXERR_ABORT;` |
|        - | 12204 | `	}` |
|        - | 12205 | `	/* Update token stream */` |
|       33 | 12206 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 | 12207 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 12208 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 12209 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12210 | `			return SXERR_ABORT;` |
|        - | 12211 | `		}` |
|      ! 0 | 12212 | `		pGen->pIn++;` |
|      ! 0 | 12213 | `	}` |
|       33 | 12214 | `	pGen->pIn  = &pEnd[1];` |
|       33 | 12215 | `	pGen->pEnd = pTmp;` |
|       33 | 12216 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       28 | 12217 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|      ! 0 | 12218 | `			pTmp = pGen->pIn;` |
|      ! 0 | 12219 | `			if( pTmp >= pGen->pEnd ){` |
|      ! 0 | 12220 | `				pTmp--;` |
|      ! 0 | 12221 | `			}` |
|        - | 12222 | `			/* Unexpected token */` |
|      ! 0 | 12223 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|      ! 0 | 12224 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 12225 | `				return SXERR_ABORT;` |
|        - | 12226 | `			}` |
|      ! 0 | 12227 | `			goto Synchronize;` |
|        - | 12228 | `	}` |
|        - | 12229 | `	/* Set the delimiter token */` |
|       33 | 12230 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|        3 | 12231 | `		nToken = PH7_TK_KEYWORD;` |
|        - | 12232 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|        2 | 12233 | `	}else{` |
|       31 | 12234 | `		nToken = PH7_TK_CCB; /* '}' */` |
|        - | 12235 | `	}` |
|       33 | 12236 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|        - | 12237 | `	/* Create the switch blocks container */` |
|       33 | 12238 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|       33 | 12239 | `	if( pSwitch == 0 ){` |
|        - | 12240 | `		/* Abort compilation */` |
|      ! 0 | 12241 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 12242 | `		return SXERR_ABORT;` |
|        - | 12243 | `	}` |
|        - | 12244 | `	/* Zero the structure */` |
|       33 | 12245 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|        - | 12246 | `	/* Initialize fields */` |
|       33 | 12247 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|        - | 12248 | `	/* Emit the switch instruction */` |
|       33 | 12249 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|        - | 12250 | `	/* Compile case blocks */` |
|      100 | 12251 | `	for(;;){` |
|        - | 12252 | `		sxu32 nKwrd;` |
|      119 | 12253 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 12254 | `			/* No more input to process */` |
|      ! 0 | 12255 | `			break;` |
|        - | 12256 | `		}` |
|      119 | 12257 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 12258 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|        - | 12259 | `				/* Unexpected token */` |
|      ! 0 | 12260 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 12261 | `					&pGen->pIn->sData);` |
|      ! 0 | 12262 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 12263 | `					return SXERR_ABORT;` |
|        - | 12264 | `				}` |
|        - | 12265 | `				/* FALL THROUGH */` |
|      ! 0 | 12266 | `			}` |
|        - | 12267 | `			/* Block compiled */` |
|      ! 0 | 12268 | `			break;` |
|        - | 12269 | `		}` |
|        - | 12270 | `		/* Extract the keyword */` |
|      119 | 12271 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      119 | 12272 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 12273 | `			if( nToken != PH7_TK_KEYWORD ){` |
|        - | 12274 | `				/* Unexpected token */` |
|      ! 0 | 12275 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 12276 | `					&pGen->pIn->sData);` |
|      ! 0 | 12277 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 12278 | `					return SXERR_ABORT;` |
|        - | 12279 | `				}` |
|        - | 12280 | `				/* FALL THROUGH */` |
|      ! 0 | 12281 | `			}` |
|        - | 12282 | `			/* Block compiled */` |
|        3 | 12283 | `			break;` |
|        - | 12284 | `		}` |
|      117 | 12285 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|        - | 12286 | `			/*` |
|        - | 12287 | `			 * Accroding to the PHP language reference manual` |
|        - | 12288 | `			 *  A special case is the default case. This case matches anything` |
|        - | 12289 | `			 *  that wasn't matched by the other cases.` |
|        - | 12290 | `			 */` |
|       25 | 12291 | `			if( pSwitch->nDefault > 0 ){` |
|        - | 12292 | `				/* Default case already compiled */` |
|      ! 0 | 12293 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|      ! 0 | 12294 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 12295 | `					return SXERR_ABORT;` |
|        - | 12296 | `				}` |
|      ! 0 | 12297 | `			}` |
|       25 | 12298 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|        - | 12299 | `			/* Compile the default block */` |
|       25 | 12300 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|       25 | 12301 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 12302 | `				return SXERR_ABORT;` |
|       25 | 12303 | `			}else if( rc == SXERR_EOF ){` |
|       23 | 12304 | `				break;` |
|        1 | 12305 | `			}` |
|       98 | 12306 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|        - | 12307 | `			ph7_case_expr sCase;` |
|        - | 12308 | `			/* Standard case block */` |
|       97 | 12309 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|        - | 12310 | `			/* initialize the structure */` |
|       97 | 12311 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - | 12312 | `			/* Compile the case expression */` |
|       97 | 12313 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|       97 | 12314 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 12315 | `				return SXERR_ABORT;` |
|        - | 12316 | `			}` |
|        - | 12317 | `			/* Compile the case block */` |
|       97 | 12318 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|        - | 12319 | `			/* Insert in the switch container */` |
|       97 | 12320 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|       97 | 12321 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 12322 | `				return SXERR_ABORT;` |
|       97 | 12323 | `			}else if( rc == SXERR_EOF ){` |
|        9 | 12324 | `				break;` |
|        - | 12325 | `			}` |
|       47 | 12326 | `		}else{` |
|        - | 12327 | `			/* Unexpected token */` |
|      ! 0 | 12328 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 12329 | `				&pGen->pIn->sData);` |
|      ! 0 | 12330 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 12331 | `				return SXERR_ABORT;` |
|        - | 12332 | `			}` |
|      ! 0 | 12333 | `			break;` |
|        - | 12334 | `		}` |
|        5 | 12335 | `	}` |
|        - | 12336 | `	/* Fix all jumps now the destination is resolved */` |
|       33 | 12337 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|       33 | 12338 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 12339 | `	/* Release the loop block */` |
|       33 | 12340 | `	GenStateLeaveBlock(pGen,0);` |
|       33 | 12341 | `	if( pGen->pIn < pGen->pEnd ){` |
|        - | 12342 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|       33 | 12343 | `		pGen->pIn++;` |
|       14 | 12344 | `	}` |
|        - | 12345 | `	/* Statement successfully compiled */` |
|       33 | 12346 | `	return SXRET_OK;` |
|      ! 0 | 12347 | `Synchronize:` |
|        - | 12348 | `	/* Synchronize with the first semi-colon */` |
|      ! 0 | 12349 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      ! 0 | 12350 | `		pGen->pIn++;` |
|      ! 0 | 12351 | `	}` |
|      ! 0 | 12352 | `	return SXRET_OK;` |
|       19 | 12353 | `}` |
|        - | 12354 | `/*` |
|        - | 12355 | ` * Chain operators participate in a postfix member-access chain.` |
|        - | 12356 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|        - | 12357 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|        - | 12358 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|        - | 12359 | ` */` |
|        - | 12360 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|        - | 12361 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|        - | 12362 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|        - | 12363 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|        - | 12364 |  |
|        - | 12365 | `/*` |
|        - | 12366 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|        - | 12367 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|        - | 12368 | ` * patched entries from the pending set.` |
|        - | 12369 | ` */` |
| 22921878 | 12370 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|        5 | 12371 | `{` |
| 22921883 | 12372 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|        - | 12373 | `	sxu32 nTarget;` |
|        - | 12374 | `	sxu32 *aIdx;` |
|        - | 12375 | `	sxu32 i;` |
| 22921883 | 12376 | `	if( nCur <= nBaseline ){` |
| 22921787 | 12377 | `		return;` |
|        - | 12378 | `	}` |
|      100 | 12379 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      100 | 12380 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|      204 | 12381 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|      108 | 12382 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|      108 | 12383 | `		if( pInstr ){` |
|      108 | 12384 | `			pInstr->iP2 = (sxi32)nTarget;` |
|       52 | 12385 | `		}` |
|       56 | 12386 | `	}` |
|      100 | 12387 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 11460944 | 12388 | `}` |
|        - | 12389 |  |
|        - | 12390 | `/*` |
|        - | 12391 | ` * By-reference out-parameters of builtin functions.` |
|        - | 12392 | ` *` |
|        - | 12393 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|        - | 12394 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|        - | 12395 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|        - | 12396 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|        - | 12397 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|        - | 12398 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|        - | 12399 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|        - | 12400 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|        - | 12401 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|        - | 12402 | ` * creates it" behaviour).` |
|        - | 12403 | ` *` |
|        - | 12404 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|        - | 12405 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|        - | 12406 | ` */` |
|  3211894 | 12407 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|        5 | 12408 | `{` |
|        - | 12409 | `	static const struct {` |
|        - | 12410 | `		const char *zName;` |
|        - | 12411 | `		sxu32 nByte;` |
|        - | 12412 | `		sxu32 mask;` |
|        - | 12413 | `	} aByRef[] = {` |
|        - | 12414 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|        - | 12415 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|        - | 12416 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|        - | 12417 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|        - | 12418 | `		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */` |
|        - | 12419 | `	};` |
|        - | 12420 | `	sxu32 i;` |
|  3211899 | 12421 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|   846689 | 12422 | `		return 0;` |
|        - | 12423 | `	}` |
| 14190871 | 12424 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 11825778 | 12425 | `		if( pName->nByte == aByRef[i].nByte` |
|  6058303 | 12426 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|      127 | 12427 | `			return aByRef[i].mask;` |
|        - | 12428 | `		}` |
|  5912833 | 12429 | `	}` |
|  2365093 | 12430 | `	return 0;` |
|  1605952 | 12431 | `}` |
|        - | 12432 | `/*` |
|        - | 12433 | ` * Recover the bare global-builtin name from a call's callee node.` |
|        - | 12434 | ` *` |
|        - | 12435 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|        - | 12436 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|        - | 12437 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|        - | 12438 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|        - | 12439 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|        - | 12440 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|        - | 12441 | ` */` |
|  3211894 | 12442 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|        5 | 12443 | `{` |
|        - | 12444 | `	SyToken *p, *pEnd;` |
|  3211899 | 12445 | `	pOut->zString = 0;` |
|  3211899 | 12446 | `	pOut->nByte = 0;` |
|  3211899 | 12447 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|      ! 0 | 12448 | `		return;` |
|        - | 12449 | `	}` |
|  3211899 | 12450 | `	p = pLeft->pStart;` |
|  3211899 | 12451 | `	pEnd = pLeft->pEnd;` |
|        - | 12452 | `	/* Optional single leading namespace separator (absolute path). */` |
|  3211899 | 12453 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|     3917 | 12454 | `		p++;` |
|     1956 | 12455 | `	}` |
|  3211899 | 12456 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|   846653 | 12457 | `		return;` |
|        - | 12458 | `	}` |
|        - | 12459 | `	/* Must be a single component: nothing follows the name token. */` |
|  2365251 | 12460 | `	if( p + 1 != pEnd ){` |
|       40 | 12461 | `		return;` |
|        - | 12462 | `	}` |
|  2365215 | 12463 | `	*pOut = p->sData;` |
|  1605952 | 12464 | `}` |
|        - | 12465 | `/*` |
|        - | 12466 | ` * Generate bytecode for a given expression tree.` |
|        - | 12467 | ` * If something goes wrong while generating bytecode` |
|        - | 12468 | ` * for the expression tree (A very unlikely scenario)` |
|        - | 12469 | ` * this function takes care of generating the appropriate` |
|        - | 12470 | ` * error message.` |
|        - | 12471 | ` */` |
| 31784272 | 12472 | `static sxi32 GenStateEmitExprCode(` |
|        - | 12473 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - | 12474 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|        - | 12475 | `	sxi32 iFlags /* Control flags */` |
|        - | 12476 | `	)` |
|        5 | 12477 | `{` |
|        - | 12478 | `	VmInstr *pInstr;` |
|        - | 12479 | `	sxu32 nJmpIdx;` |
| 31784277 | 12480 | `	sxi32 iP1 = 0;` |
| 31784277 | 12481 | `	sxu32 iP2 = 0;` |
| 31784277 | 12482 | `	void *p3  = 0;` |
|        - | 12483 | `	sxi32 iVmOp;` |
|        - | 12484 | `	sxi32 rc;` |
| 31784277 | 12485 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 31784277 | 12486 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
| 31784277 | 12487 | `	sxu32 nRhsNsBase = 0;` |
| 31784277 | 12488 | `	if( pNode->xCode ){` |
|        - | 12489 | `		SyToken *pTmpIn,*pTmpEnd;` |
|        - | 12490 | `		/* Compile node */` |
| 19092549 | 12491 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 19092549 | 12492 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 19092549 | 12493 | `		RE_SWAP_DELIMITER(pGen);` |
| 19092549 | 12494 | `		return rc;` |
|        - | 12495 | `	}` |
| 12691733 | 12496 | `	if( pNode->pOp == 0 ){` |
|      ! 0 | 12497 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 12498 | `			"Invalid expression node,PH7 is aborting compilation");` |
|      ! 0 | 12499 | `		return SXERR_ABORT;` |
|        - | 12500 | `	}` |
| 12691733 | 12501 | `	iVmOp = pNode->pOp->iVmOp;` |
| 12691733 | 12502 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|        - | 12503 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|        - | 12504 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|        - | 12505 | `		 * and later errors are still reported. */` |
|        3 | 12506 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 12507 | `			"The (unset) cast is no longer supported");` |
|        3 | 12508 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12509 | `			return SXERR_ABORT;` |
|        - | 12510 | `		}` |
|        1 | 12511 | `	}` |
| 12691733 | 12512 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|       65 | 12513 | `		sxu32 nJmp = 0;` |
|        - | 12514 | `		sxu32 nNcNsBase;` |
|        - | 12515 | `		VmInstr *pInstrFix;` |
|        - | 12516 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|        - | 12517 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|        - | 12518 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|        - | 12519 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|        - | 12520 | `		 * stack slot carries a writable nIdx. */` |
|       65 | 12521 | `		if( pNode->pRight ){` |
|       65 | 12522 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       65 | 12523 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|       65 | 12524 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 12525 | `				return rc;` |
|        - | 12526 | `			}` |
|       65 | 12527 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        - | 12528 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|        - | 12529 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|        - | 12530 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|        - | 12531 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|        - | 12532 | `			 * the store, so the parent array does not need to be copied at` |
|        - | 12533 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|        - | 12534 | `			 * cascade for the actual write path stays correct. */` |
|       65 | 12535 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|       65 | 12536 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|       31 | 12537 | `				pInstrFix->iP2 = 3;` |
|       14 | 12538 | `			}` |
|       31 | 12539 | `		}` |
|        - | 12540 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|       65 | 12541 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|        - | 12542 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|       65 | 12543 | `		if( pNode->pLeft ){` |
|       65 | 12544 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       65 | 12545 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|       65 | 12546 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 12547 | `				return rc;` |
|        - | 12548 | `			}` |
|       65 | 12549 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       31 | 12550 | `		}` |
|        - | 12551 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|       65 | 12552 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|        - | 12553 | `		/* Patch the short-circuit jump to land after the store. */` |
|       65 | 12554 | `		if( nJmp > 0 ){` |
|       65 | 12555 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|       65 | 12556 | `			if( pInstrFix ){` |
|       65 | 12557 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|       31 | 12558 | `			}` |
|       31 | 12559 | `		}` |
|       65 | 12560 | `		return SXRET_OK;` |
|        - | 12561 | `	}` |
| 12691671 | 12562 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|        - | 12563 | `		sxu32 nJz,nJmp;` |
|        - | 12564 | `		sxu32 nTernaryNsBase;` |
|        - | 12565 | `		/* Ternary operator require special handling */` |
|        - | 12566 | `		/* Phase#1: Compile the condition */` |
|   212923 | 12567 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   212923 | 12568 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|   212923 | 12569 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12570 | `			return rc;` |
|        - | 12571 | `		}` |
|        - | 12572 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|        - | 12573 | `		 * compiling the condition must short-circuit to the end of the` |
|        - | 12574 | `		 * condition expression, not leak past the ternary. */` |
|   212923 | 12575 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|   212923 | 12576 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|   212923 | 12577 | `		if( pNode->pLeft ){` |
|        - | 12578 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|        - | 12579 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|   212855 | 12580 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|        - | 12581 | `			/* Phase#3: Compile the 'then' expression  */` |
|   212855 | 12582 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   212855 | 12583 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|   212855 | 12584 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 12585 | `				return rc;` |
|        - | 12586 | `			}` |
|   212855 | 12587 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|   106430 | 12588 | `		}else{` |
|        - | 12589 | `			/* Elvis operator: (expr) ?: (else)` |
|        - | 12590 | `			 * Duplicate condition so original value is the 'then' result.` |
|        - | 12591 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|       70 | 12592 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       70 | 12593 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|        - | 12594 | `		}` |
|        - | 12595 | `		/* Phase#4: Emit the unconditional jump */` |
|   212923 | 12596 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|        - | 12597 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|   212923 | 12598 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|   212923 | 12599 | `		if( pInstr ){` |
|   212923 | 12600 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|   106459 | 12601 | `		}` |
|   212923 | 12602 | `		if( !pNode->pLeft ){` |
|        - | 12603 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|       70 | 12604 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       34 | 12605 | `		}` |
|        - | 12606 | `		/* Phase#6: Compile the 'else' expression */` |
|   212923 | 12607 | `		if( pNode->pRight ){` |
|   212923 | 12608 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   212923 | 12609 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|   212923 | 12610 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 12611 | `				return rc;` |
|        - | 12612 | `			}` |
|   212923 | 12613 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|   106459 | 12614 | `		}` |
|   212923 | 12615 | `		if( nJmp > 0 ){` |
|        - | 12616 | `			/* Phase#7: Fix the unconditional jump */` |
|   212923 | 12617 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|   212923 | 12618 | `			if( pInstr ){` |
|   212923 | 12619 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|   106459 | 12620 | `			}` |
|   106459 | 12621 | `		}` |
|        - | 12622 | `		/* All done */` |
|   212923 | 12623 | `		return SXRET_OK;` |
|        - | 12624 | `	}` |
| 12478753 | 12625 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|        - | 12626 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|        - | 12627 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|        - | 12628 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|        - | 12629 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|        - | 12630 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|        - | 12631 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|        - | 12632 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|        - | 12633 | `		sxu32 nPipeNsBase;` |
|       27 | 12634 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|       27 | 12635 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|      ! 0 | 12636 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 12637 | `				"'\|>': Missing operand");` |
|      ! 0 | 12638 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 12639 | `		}` |
|        - | 12640 | `		/* Argument: the LHS value. */` |
|       27 | 12641 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       27 | 12642 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|       27 | 12643 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12644 | `			return rc;` |
|        - | 12645 | `		}` |
|       27 | 12646 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|        - | 12647 | `		/* Callable: the RHS. */` |
|       27 | 12648 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       27 | 12649 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|       27 | 12650 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12651 | `			return rc;` |
|        - | 12652 | `		}` |
|       27 | 12653 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|        - | 12654 | `		/* Invoke the callable with the single piped argument. */` |
|       27 | 12655 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       27 | 12656 | `		return SXRET_OK;` |
|        - | 12657 | `	}` |
| 12478727 | 12658 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|        - | 12659 | `	/* Generate code for the left tree */` |
| 12478727 | 12660 | `	if( pNode->pLeft ){` |
| 12467077 | 12661 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 12467077 | 12662 | `		if( iVmOp == PH7_OP_CALL ){` |
|        - | 12663 | `			ph7_expr_node **apNode;` |
|  3216099 | 12664 | `			int hasSpread = 0;` |
|  3216099 | 12665 | `			int hasNamed = 0;` |
|  3216099 | 12666 | `			int bAnySpread = 0;` |
|  3216099 | 12667 | `			sxu32 byRefMask = 0;` |
|        - | 12668 | `			sxi32 nArgs;` |
|        - | 12669 | `			sxi32 n;` |
|        - | 12670 | `			/* Recurse and generate bytecodes for function arguments */` |
|  3216099 | 12671 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  3216099 | 12672 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|        - | 12673 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|        - | 12674 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|        - | 12675 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|  3216099 | 12676 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|       81 | 12677 | `				bFcc = 1;` |
|       81 | 12678 | `				nArgs = 0;` |
|       40 | 12679 | `			}` |
|        - | 12680 | `			/* Validate argument order like php: no positional argument after a` |
|        - | 12681 | ``			 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|        - | 12682 | `			{` |
|  3216099 | 12683 | `				int seenNamed = 0;` |
|  3216099 | 12684 | `				int seenSpread = 0;` |
|  6391565 | 12685 | `				for( n = 0; n < nArgs; ++n ){` |
|  3175473 | 12686 | `					if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|     4073 | 12687 | `						bAnySpread = 1;` |
|     4073 | 12688 | `						seenSpread = 1;` |
|     4073 | 12689 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      ! 0 | 12690 | `							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 12691 | `								"syntax error, unexpected token \"...\"");` |
|      ! 0 | 12692 | `							return SXERR_SYNTAX;` |
|        5 | 12693 | `						}` |
|  3173439 | 12694 | `					}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      289 | 12695 | `						seenNamed = 1;` |
|      289 | 12696 | `						hasNamed = 1;` |
|  3171263 | 12697 | `					}else if( seenNamed ){` |
|        3 | 12698 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 12699 | `							"Cannot use positional argument after named argument");` |
|        3 | 12700 | `						return SXERR_SYNTAX;` |
|  3171119 | 12701 | `					}else if( seenSpread ){` |
|      ! 0 | 12702 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 12703 | `							"Cannot use positional argument after argument unpacking");` |
|      ! 0 | 12704 | `						return SXERR_SYNTAX;` |
|        - | 12705 | `					}` |
|  1587738 | 12706 | `				}` |
|        - | 12707 | `			}` |
|        - | 12708 | `			/* Read-only load */` |
|  3216097 | 12709 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|        - | 12710 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|        - | 12711 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|        - | 12712 | `			 * objects dispatch to the right method (offsetExists for both;` |
|        - | 12713 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|  3216097 | 12714 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|  3216097 | 12715 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  3216092 | 12716 | `				if( pCallName->nByte == 5` |
|  1770484 | 12717 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|   163487 | 12718 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|  3134356 | 12719 | `				}else if( pCallName->nByte == 5` |
|  1607002 | 12720 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|      103 | 12721 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|       49 | 12722 | `				}` |
|        - | 12723 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|        - | 12724 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|        - | 12725 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|        - | 12726 | `				 * write back through. Skipped when spread/named args are present:` |
|        - | 12727 | `				 * the compile-time positional index no longer maps to the` |
|        - | 12728 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|  3216097 | 12729 | `				if( !bAnySpread && !hasNamed ){` |
|        - | 12730 | `					SyString sBuiltin;` |
|  3211899 | 12731 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|  3211899 | 12732 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|  1605947 | 12733 | `				}` |
|  1608046 | 12734 | `			}` |
|  6391561 | 12735 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  3175469 | 12736 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  3175469 | 12737 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|        - | 12738 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|        - | 12739 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|        - | 12740 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|        - | 12741 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|        - | 12742 | `				 * builtin to write back through. A plain $var target is unaffected` |
|        - | 12743 | `				 * (iP1=0 either way). */` |
|  3175469 | 12744 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|       61 | 12745 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|       61 | 12746 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|       28 | 12747 | `				}` |
|  3175469 | 12748 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  3175469 | 12749 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 12750 | `					return rc;` |
|        - | 12751 | `				}` |
|        - | 12752 | `				/* Each argument is an independent nullsafe scope. */` |
|  3175469 | 12753 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  3175469 | 12754 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|        - | 12755 | `					/* Emit spread opcode to unpack this array argument */` |
|     4073 | 12756 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|     4073 | 12757 | `					hasSpread = 1;` |
|     2034 | 12758 | `				}` |
|  1587737 | 12759 | `			}` |
|        - | 12760 | `			/* Total number of given arguments */` |
|  3216097 | 12761 | `			iP1 = nArgs;` |
|  3216097 | 12762 | `			iP2 = hasSpread;` |
|        - | 12763 | `			/* Build VmCallArgMap if named arguments are present.` |
|        - | 12764 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  3216097 | 12765 | `			if( hasNamed ){` |
|      178 | 12766 | `				sxu32 nStrBytes = 0;` |
|        - | 12767 | `				char *zBuf;` |
|      534 | 12768 | `				for( n = 0; n < nArgs; ++n ){` |
|      360 | 12769 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      286 | 12770 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|      141 | 12771 | `					}` |
|      182 | 12772 | `				}` |
|        - | 12773 | `				{` |
|      178 | 12774 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|      178 | 12775 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      174 | 12776 | `					&pGen->pVm->sAllocator, mapSize);` |
|      178 | 12777 | `				if( pMap ){` |
|      178 | 12778 | `					SyZero(pMap, mapSize);` |
|      178 | 12779 | `					pMap->bHasNamed = 1;` |
|      178 | 12780 | `					pMap->nTotal = (sxu32)nArgs;` |
|      178 | 12781 | `					pMap->aNames = (SyString *)&pMap[1];` |
|      178 | 12782 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|      534 | 12783 | `					for( n = 0; n < nArgs; ++n ){` |
|      360 | 12784 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      286 | 12785 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|      286 | 12786 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|      286 | 12787 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|      286 | 12788 | `							zBuf += nb;` |
|      141 | 12789 | `						}` |
|        - | 12790 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|      182 | 12791 | `					}` |
|      178 | 12792 | `					p3 = (void *)pMap;` |
|       87 | 12793 | `				}` |
|        - | 12794 | `				}` |
|       87 | 12795 | `			}` |
|        - | 12796 | `			/* Remove stale flags now */` |
|  3216097 | 12797 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  1608046 | 12798 | `		}` |
|        - | 12799 | `		{` |
|        - | 12800 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|        - | 12801 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|        - | 12802 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|        - | 12803 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|        - | 12804 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|        - | 12805 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|        - | 12806 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|        - | 12807 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
| 12467075 | 12808 | `			sxi32 iLeftFlags = iFlags;` |
| 12467070 | 12809 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
| 10400298 | 12810 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|  4166789 | 12811 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  3698971 | 12812 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   951571 | 12813 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   475783 | 12814 | `			}` |
|        - | 12815 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|        - | 12816 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|        - | 12817 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|        - | 12818 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|        - | 12819 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|        - | 12820 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|        - | 12821 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
| 12467070 | 12822 | `			if( pNode->pOp` |
| 17709566 | 12823 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
| 11476078 | 12824 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
| 10485034 | 12825 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|  2013857 | 12826 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|  1006926 | 12827 | `			}` |
| 12467075 | 12828 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|        - | 12829 | `		}` |
| 12467075 | 12830 | `		if( rc != SXRET_OK ){` |
|       34 | 12831 | `			return rc;` |
|        - | 12832 | `		}` |
| 12467045 | 12833 | `		if( !bIsChainOp ){` |
|        - | 12834 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|        - | 12835 | `			 * target the end of that LHS chain, which is right here. */` |
|  5630125 | 12836 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  2815060 | 12837 | `		}` |
| 12467045 | 12838 | `		if( iVmOp == PH7_OP_CALL ){` |
|  3216097 | 12839 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  3216097 | 12840 | `			if( pInstr ){` |
|  3216097 | 12841 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  2365491 | 12842 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|        - | 12843 | `					sxu32 nQual;` |
|  2365491 | 12844 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|        - | 12845 | `					/* Prevent constant expansion but preserve the absolute flag` |
|        - | 12846 | `					 * so the later NEW handler (if any) can see it. */` |
|  2365491 | 12847 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|        - | 12848 | `					/* Namespace-qualify the function name for CALL, unless the` |
|        - | 12849 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|        - | 12850 | `					 * imports — class imports must NOT affect function` |
|        - | 12851 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|        - | 12852 | `					 * before NEW; we store the original literal index in the` |
|        - | 12853 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|        - | 12854 | `					 * the unqualified name and re-qualify with class imports. */` |
|  2365491 | 12855 | `					if( bAbsolute ){` |
|     3917 | 12856 | `						pInstr->iP2 = (sxi32)nOrig;` |
|     1961 | 12857 | `					}else{` |
|  2361579 | 12858 | `						int fromImport = 0;` |
|  2361579 | 12859 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  2361579 | 12860 | `						pInstr->iP2 = (sxi32)nQual;` |
|  2361579 | 12861 | `						if( nQual != nOrig ){` |
|        - | 12862 | `							/* Record the original literal index in the arg map` |
|        - | 12863 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|        - | 12864 | `							 * flag) so the NEW handler can recover the` |
|        - | 12865 | `							 * unqualified name and re-qualify with CLASS` |
|        - | 12866 | `							 * imports. */` |
|       77 | 12867 | `							if( p3 == 0 ){` |
|       77 | 12868 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       72 | 12869 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|       77 | 12870 | `								if( pMap ){` |
|       77 | 12871 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|       77 | 12872 | `									p3 = (void *)pMap;` |
|       36 | 12873 | `								}` |
|       36 | 12874 | `							}` |
|       77 | 12875 | `							if( p3 ){` |
|       77 | 12876 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|       77 | 12877 | `								if( !fromImport ){` |
|        - | 12878 | `									/* Mark as namespace-qualified */` |
|       67 | 12879 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|       31 | 12880 | `								}` |
|       36 | 12881 | `							}` |
|       36 | 12882 | `						}` |
|        5 | 12883 | `					}` |
|  2033354 | 12884 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|        - | 12885 | `					/* Method call,flag that */` |
|   846121 | 12886 | `					pInstr->iP2 = 1;` |
|   423058 | 12887 | `				}` |
|  1608051 | 12888 | `			}` |
| 10858999 | 12889 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|        - | 12890 | `			ph7_expr_node **apNode;` |
|        - | 12891 | `			sxi32 n;` |
|  1606981 | 12892 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|        - | 12893 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|        - | 12894 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|        - | 12895 | `			/* Recurse and generate bytecodes for array index */` |
|  1606981 | 12896 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  3085655 | 12897 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|  1478679 | 12898 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  1478679 | 12899 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|  1478679 | 12900 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 12901 | `					return rc;` |
|        - | 12902 | `				}` |
|        - | 12903 | `				/* Each subscript index is an independent nullsafe scope. */` |
|  1478679 | 12904 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   739342 | 12905 | `			}` |
|  1606981 | 12906 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|  1478679 | 12907 | `				iP1 = 1; /* Node have an index associated with it */` |
|   739337 | 12908 | `			}` |
|  1606981 | 12909 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|        - | 12910 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|   202213 | 12911 | `				iP2 = 4;` |
|  1505877 | 12912 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        - | 12913 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|        - | 12914 | `				 * so the trailing unset() builtin can drop the slot. */` |
|       72 | 12915 | `				iP2 = 5;` |
|  1404739 | 12916 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        - | 12917 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|        - | 12918 | `				 * short-circuit on missing keys without invoking offsetGet` |
|        - | 12919 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|       29 | 12920 | `				iP2 = 6;` |
|  1404693 | 12921 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|        - | 12922 | `				/* Create an empty entry when the desired index is not found */` |
|   190899 | 12923 | `				iP2 = 1;` |
|    95452 | 12924 | `			}` |
|  8447465 | 12925 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|        - | 12926 | `			/* POP the left node */` |
|       32 | 12927 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       15 | 12928 | `		}` |
|  6233520 | 12929 | `	}` |
| 12478695 | 12930 | `	rc = SXRET_OK;` |
| 12478695 | 12931 | `	nJmpIdx = 0;` |
|        - | 12932 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|        - | 12933 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|        - | 12934 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 12478695 | 12935 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|    43419 | 12936 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    43419 | 12937 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    43419 | 12938 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    43419 | 12939 | `			int isSpecial = 0;` |
|    43419 | 12940 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|    20091 | 12941 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|    20091 | 12942 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|    20086 | 12943 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    31682 | 12944 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    15843 | 12945 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|    11789 | 12946 | `					isSpecial = 1;` |
|     5892 | 12947 | `				}` |
|    15875 | 12948 | `			}` |
|    55083 | 12949 | `			pInstr->iP1 = 0;` |
|    55083 | 12950 | `			if( !isSpecial ){` |
|    19971 | 12951 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     9983 | 12952 | `			}` |
|        - | 12953 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|        - | 12954 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|    31755 | 12955 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    19971 | 12956 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    19971 | 12957 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|       60 | 12958 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|       62 | 12959 | `					return SXRET_OK;` |
|        - | 12960 | `				}` |
|     9954 | 12961 | `			}` |
|    15846 | 12962 | `		}` |
|    39153 | 12963 | `	}` |
|        - | 12964 | `	/* Generate code for the right tree */` |
| 12466987 | 12965 | `	if( pNode->pRight ){` |
|  6804101 | 12966 | `		if( iVmOp == PH7_OP_LAND ){` |
|        - | 12967 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|   136471 | 12968 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  6735868 | 12969 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|        - | 12970 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    93399 | 12971 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  6620938 | 12972 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|        - | 12973 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|      141 | 12974 | `			iVmOp = 0; /* No binary operator to emit */` |
|      141 | 12975 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  6574225 | 12976 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|        - | 12977 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|        - | 12978 | `			 * the entire containing postfix chain to null. The jump target is` |
|        - | 12979 | `			 * patched later by the innermost non-chain ancestor (or by` |
|        - | 12980 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|        - | 12981 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|        - | 12982 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|      108 | 12983 | `			sxu32 nNsJmp = 0;` |
|      108 | 12984 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|      108 | 12985 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  6574053 | 12986 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|        - | 12987 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|        - | 12988 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|        - | 12989 | `			 * auto-created — PHP auto-vivifies on write. */` |
|  2321987 | 12990 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|  1160991 | 12991 | `		}` |
|  6804101 | 12992 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  6804101 | 12993 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  6804101 | 12994 | `		if( !bIsChainOp ){` |
|        - | 12995 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|        - | 12996 | `			 * operator instruction is emitted. */` |
|  4790307 | 12997 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  2395151 | 12998 | `		}` |
|  6804101 | 12999 | `		if( iVmOp == PH7_OP_STORE ){` |
|  2034257 | 13000 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  2034222 | 13001 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|        - | 13002 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|        - | 13003 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|        - | 13004 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|        - | 13005 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|        - | 13006 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|        - | 13007 | `				 */` |
|       91 | 13008 | `				iVmOp = 0;` |
|  2034214 | 13009 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  2034171 | 13010 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|        - | 13011 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   249211 | 13012 | `					iP2 = 1;` |
|   124608 | 13013 | `				}else{` |
|  1784965 | 13014 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        - | 13015 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   190817 | 13016 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   190817 | 13017 | `						iP1 = pInstr->iP1;` |
|    95411 | 13018 | `					}else{` |
|  1594153 | 13019 | `						p3 = pInstr->p3;` |
|        - | 13020 | `					}` |
|        - | 13021 | `					/* POP the last dynamic load instruction */` |
|  1784965 | 13022 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        - | 13023 | `				}` |
|  1017088 | 13024 | `			}` |
|  5786975 | 13025 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|       64 | 13026 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|       64 | 13027 | `			if( pInstr ){` |
|       64 | 13028 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        - | 13029 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|        - | 13030 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|        - | 13031 | `					 */` |
|       19 | 13032 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|       19 | 13033 | `					iP1 = pInstr->iP1;` |
|       19 | 13034 | `					iP2 = pInstr->iP2;` |
|       19 | 13035 | `					p3  = pInstr->p3;` |
|       10 | 13036 | `				}else{` |
|       46 | 13037 | `					p3 = pInstr->p3;` |
|        - | 13038 | `				}` |
|       30 | 13039 | `			}` |
|       30 | 13040 | `		}` |
|  3402048 | 13041 | `	}` |
| 12466982 | 13042 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|   242141 | 13043 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|        - | 13044 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|        - | 13045 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|       32 | 13046 | `		iVmOp = 0;` |
|       14 | 13047 | `	}` |
| 12466987 | 13048 | `	if( iVmOp > 0 ){` |
| 12466707 | 13049 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    70375 | 13050 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|        - | 13051 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    11685 | 13052 | `				iP1 = 1;` |
|     5845 | 13053 | `			}` |
| 12431522 | 13054 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|        - | 13055 | `			/* Namespace-qualify the class name for NEW */ {` |
|   483973 | 13056 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   483973 | 13057 | `				VmInstr *pCallInstr = 0;` |
|   483973 | 13058 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   483725 | 13059 | `					pCallInstr = pPeek;` |
|   483725 | 13060 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|   241860 | 13061 | `				}` |
|   483973 | 13062 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   483969 | 13063 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|        - | 13064 | `					sxu32 nLitForClass;` |
|   483969 | 13065 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|        - | 13066 | `					/* If the CALL handler qualified the name with FUNCTION` |
|        - | 13067 | `					 * imports, recover the original literal (recorded in the` |
|        - | 13068 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|        - | 13069 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|        - | 13070 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|        - | 13071 | `					 * with class imports. */` |
|   483969 | 13072 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|       37 | 13073 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|       21 | 13074 | `					}else{` |
|   483937 | 13075 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|        - | 13076 | `					}` |
|   483969 | 13077 | `					pPeek->iP1 = 0;` |
|   483969 | 13078 | `					if( !bAbsolute ){` |
|   480061 | 13079 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|   240033 | 13080 | `					}else{` |
|     3913 | 13081 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|        - | 13082 | `					}` |
|   241982 | 13083 | `				}` |
|        - | 13084 | `			}` |
|   483973 | 13085 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   483973 | 13086 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|        - | 13087 | `				VmInstr *pPrev;` |
|   483725 | 13088 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   483725 | 13089 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|        - | 13090 | `					/* Pop the call instruction, preserve named-arg map and` |
|        - | 13091 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|        - | 13092 | `					 * accumulator exactly like OP_CALL would have). */` |
|   483725 | 13093 | `					iP1 = pInstr->iP1;` |
|   483725 | 13094 | `					iP2 = pInstr->iP2;` |
|   483725 | 13095 | `					if( pInstr->p3 ){` |
|       47 | 13096 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|       21 | 13097 | `					}` |
|   483725 | 13098 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|   241860 | 13099 | `				}` |
|   241865 | 13100 | `			}` |
| 12154353 | 13101 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|        - | 13102 | `			/* instanceof: right operand is a class name, not a constant.` |
|        - | 13103 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|    31301 | 13104 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    31301 | 13105 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    31301 | 13106 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    31301 | 13107 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|    31301 | 13108 | `				int isSpecialIs = 0;` |
|    31301 | 13109 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    31301 | 13110 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    31301 | 13111 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    31296 | 13112 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    31299 | 13113 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    15648 | 13114 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|       12 | 13115 | `						isSpecialIs = 1;` |
|        5 | 13116 | `					}` |
|    15648 | 13117 | `				}` |
|    31301 | 13118 | `				pInstr->iP1 = 0;` |
|    31301 | 13119 | `				if( !isSpecialIs && !bAbsolute ){` |
|    31281 | 13120 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    15638 | 13121 | `				}` |
|    15653 | 13122 | `			}` |
| 11896721 | 13123 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|        - | 13124 | `			/* Prevent constant expansion for member/property names.` |
|        - | 13125 | `			 * The right child (member name) was just compiled — its LOADC` |
|        - | 13126 | `			 * should not trigger constant lookup. */` |
|  2013799 | 13127 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  2013799 | 13128 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  2013727 | 13129 | `				pInstr->iP1 = 0;` |
|  1006861 | 13130 | `			}` |
|  2013799 | 13131 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|        - | 13132 | `				/* Static member access,remember that */` |
|    31711 | 13133 | `				iP1 = 1;` |
|    31711 | 13134 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    31711 | 13135 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|       62 | 13136 | `					p3 = pInstr->p3;` |
|       62 | 13137 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       29 | 13138 | `				}` |
|    15853 | 13139 | `			}` |
|        - | 13140 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|        - | 13141 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|        - | 13142 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|        - | 13143 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|  2013799 | 13144 | `			if( iP2 == PH7_MEMBER_READ ){` |
|  2013799 | 13145 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       36 | 13146 | `					iP2 = PH7_MEMBER_UNSET;` |
|  2013782 | 13147 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       93 | 13148 | `					iP2 = PH7_MEMBER_ISSET;` |
|  2013721 | 13149 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       17 | 13150 | `					iP2 = PH7_MEMBER_EMPTY;` |
|  2013669 | 13151 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|        - | 13152 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|   249291 | 13153 | `					iP2 = PH7_MEMBER_WRITE;` |
|   124643 | 13154 | `				}` |
|  1006897 | 13155 | `			}` |
|  1006897 | 13156 | `		}` |
|        - | 13157 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|        - | 13158 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|        - | 13159 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|        - | 13160 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|        - | 13161 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
| 12466707 | 13162 | `		if( bFcc ){` |
|       81 | 13163 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|       81 | 13164 | `			iP2 = 0;` |
|       81 | 13165 | `			p3 = 0;` |
|       81 | 13166 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|       81 | 13167 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|        - | 13168 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|        - | 13169 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|        - | 13170 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|        - | 13171 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|       37 | 13172 | `				void *pMemberName = pInstr->p3;` |
|       37 | 13173 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|       37 | 13174 | `				if( pMemberName ){` |
|        3 | 13175 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|        1 | 13176 | `				}` |
|       37 | 13177 | `				iP1 = 2;` |
|       19 | 13178 | `			}else{` |
|       45 | 13179 | `				iP1 = 1;` |
|        - | 13180 | `			}` |
|       40 | 13181 | `		}` |
|        - | 13182 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|        - | 13183 | `		 * This is the primary emit path for user-visible calls. */` |
| 12466707 | 13184 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  3699985 | 13185 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  1849990 | 13186 | `		}` |
|        - | 13187 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 12466707 | 13188 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  6233351 | 13189 | `	}` |
| 12466987 | 13190 | `	if( nJmpIdx > 0 ){` |
|        - | 13191 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   230001 | 13192 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   230001 | 13193 | `		if( pInstr ){` |
|   230001 | 13194 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|   114998 | 13195 | `		}` |
|   114998 | 13196 | `	}` |
| 12466987 | 13197 | `	return rc;` |
| 15886316 | 13198 | `}` |
|        - | 13199 | `/*` |
|        - | 13200 | ` * Compile a PHP expression.` |
|        - | 13201 | ` * According to the PHP language reference manual:` |
|        - | 13202 | ` *  Expressions are the most important building stones of PHP.` |
|        - | 13203 | ` *  In PHP, almost anything you write is an expression.` |
|        - | 13204 | ` *  The simplest yet most accurate way to define an expression` |
|        - | 13205 | ` *  is "anything that has a value".` |
|        - | 13206 | ` * If something goes wrong while compiling the expression,this` |
|        - | 13207 | ` * function takes care of generating the appropriate error` |
|        - | 13208 | ` * message.` |
|        - | 13209 | ` */` |
|  7208648 | 13210 | `static sxi32 PH7_CompileExpr(` |
|        - | 13211 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 13212 | `	sxi32 iFlags,        /* Control flags */` |
|        - | 13213 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|        - | 13214 | `	)` |
|        5 | 13215 | `{` |
|        - | 13216 | `	ph7_expr_node *pRoot;` |
|        - | 13217 | `	SySet sExprNode;` |
|        - | 13218 | `	SyToken *pEnd;` |
|        - | 13219 | `	sxi32 nExpr;` |
|        - | 13220 | `	sxi32 iNest;` |
|        - | 13221 | `	sxi32 rc;` |
|        - | 13222 | `	sxu32 nNullsafeBase;` |
|        - | 13223 | `	/* Initialize worker variables */` |
|  7208653 | 13224 | `	nExpr = 0;` |
|  7208653 | 13225 | `	pRoot = 0;` |
|        - | 13226 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|        - | 13227 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  7208653 | 13228 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  7208653 | 13229 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  7208653 | 13230 | `	SySetAlloc(&sExprNode,0x10);` |
|  7208653 | 13231 | `	rc = SXRET_OK;` |
|        - | 13232 | `	/* Delimit the expression */` |
|  7208653 | 13233 | `	pEnd = pGen->pIn;` |
|  7208653 | 13234 | `	iNest = 0;` |
| 56089373 | 13235 | `	while( pEnd < pGen->pEnd ){` |
| 53440711 | 13236 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|        - | 13237 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      701 | 13238 | `			iNest++;` |
| 53440363 | 13239 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      709 | 13240 | `			iNest--;` |
| 53439663 | 13241 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  4560577 | 13242 | `			if( iNest <= 0 ){` |
|  4559991 | 13243 | `				break;` |
|        - | 13244 | `			}` |
|      293 | 13245 | `		}` |
| 48880725 | 13246 | `		pEnd++;` |
|        5 | 13247 | `	}` |
|  7208653 | 13248 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   237809 | 13249 | `		SyToken *pEnd2 = pGen->pIn;` |
|   237809 | 13250 | `		iNest = 0;` |
|        - | 13251 | `		/* Stop at the first comma */` |
|   553851 | 13252 | `		while( pEnd2 < pEnd ){` |
|   316049 | 13253 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|     7861 | 13254 | `				iNest++;` |
|   312121 | 13255 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|     7861 | 13256 | `				iNest--;` |
|   304265 | 13257 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|       63 | 13258 | `				if( iNest <= 0 ){` |
|        3 | 13259 | `					break;` |
|        - | 13260 | `				}` |
|       28 | 13261 | `			}` |
|   316047 | 13262 | `			pEnd2++;` |
|        5 | 13263 | `		}` |
|   237809 | 13264 | `		if( pEnd2 <pEnd ){` |
|        3 | 13265 | `			pEnd = pEnd2;` |
|        1 | 13266 | `		}` |
|   118902 | 13267 | `	}` |
|  7208653 | 13268 | `	if( pEnd > pGen->pIn ){` |
|  7208643 | 13269 | `		SyToken *pTmp = pGen->pEnd;` |
|        - | 13270 | `		/* Swap delimiter */` |
|  7208643 | 13271 | `		pGen->pEnd = pEnd;` |
|        - | 13272 | `		/* Try to get an expression tree */` |
|  7208643 | 13273 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  7208643 | 13274 | `		if( rc == SXRET_OK && pRoot ){` |
|  7208461 | 13275 | `			rc = SXRET_OK;` |
|  7208461 | 13276 | `			if( xTreeValidator ){` |
|        - | 13277 | `				/* Call the upper layer validator callback */` |
|   563723 | 13278 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   281859 | 13279 | `			}` |
|  7208461 | 13280 | `			if( rc != SXERR_ABORT ){` |
|        - | 13281 | `				/* Generate code for the given tree */` |
|  7208461 | 13282 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|        - | 13283 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|        - | 13284 | `				 * expression so they short-circuit to its end. */` |
|  7208461 | 13285 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  3604228 | 13286 | `			}` |
|  7208461 | 13287 | `			nExpr = 1;` |
|  3604228 | 13288 | `		}` |
|        - | 13289 | `		/* Release the whole tree */` |
|  7208643 | 13290 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|        - | 13291 | `		/* Synchronize token stream */` |
|  7208643 | 13292 | `		pGen->pEnd = pTmp;` |
|  7208643 | 13293 | `		pGen->pIn  = pEnd;` |
|  7208643 | 13294 | `		if( rc == SXERR_ABORT ){` |
|       13 | 13295 | `			SySetRelease(&sExprNode);` |
|       13 | 13296 | `			return SXERR_ABORT;` |
|        - | 13297 | `		}` |
|  3604314 | 13298 | `	}` |
|  7208643 | 13299 | `	SySetRelease(&sExprNode);` |
|  7208643 | 13300 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  3604329 | 13301 | `}` |
|        - | 13302 | `/*` |
|        - | 13303 | ` * Return a pointer to the node construct handler associated` |
|        - | 13304 | ` * with a given node type [i.e: string,integer,float,...].` |
|        - | 13305 | ` */` |
|  4329498 | 13306 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|        5 | 13307 | `{` |
|  4329503 | 13308 | `	if( nNodeType & PH7_TK_NUM ){` |
|        - | 13309 | `		/* Numeric literal: Either real or integer */` |
|  1296939 | 13310 | `		return PH7_CompileNumLiteral;` |
|  3032569 | 13311 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|        - | 13312 | `		/* Double quoted string */` |
|    37047 | 13313 | `		return PH7_CompileString;` |
|  2995527 | 13314 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|        - | 13315 | `		/* Single quoted string */` |
|  2995407 | 13316 | `		return PH7_CompileSimpleString;` |
|      125 | 13317 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|        - | 13318 | `		/* Heredoc */` |
|       71 | 13319 | `		return PH7_CompileHereDoc;` |
|       58 | 13320 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|        - | 13321 | `		/* Nowdoc */` |
|       51 | 13322 | `		return PH7_CompileNowDoc;` |
|        9 | 13323 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|        - | 13324 | `		/* Backtick quoted string */` |
|        6 | 13325 | `		return PH7_CompileBacktic;` |
|        - | 13326 | `	}` |
|        3 | 13327 | `	return 0;` |
|  2164754 | 13328 | `}` |
|        - | 13329 | `/*` |
|        - | 13330 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|        - | 13331 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|        - | 13332 | ` * in write context" parse error.` |
|        - | 13333 | ` */` |
|     6852 | 13334 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|        5 | 13335 | `{` |
|        - | 13336 | `	sxi32 rc;` |
|     6857 | 13337 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|     6855 | 13338 | `		return SXRET_OK;` |
|        - | 13339 | `	}` |
|        5 | 13340 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|        2 | 13341 | `		pNode ? pNode->pStart->nLine : 1,` |
|        - | 13342 | `		"Can't use nullsafe operator in write context");` |
|        3 | 13343 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     3431 | 13344 | `}` |
|        - | 13345 | `/*` |
|        - | 13346 | ` * Compile an unset() statement.` |
|        - | 13347 | ` * unset($var, $arr[$key], ...);` |
|        - | 13348 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|        - | 13349 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|        - | 13350 | ` * parent array before extracting the element to unset.` |
|        - | 13351 | ` */` |
|     2930 | 13352 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|        5 | 13353 | `{` |
|     2935 | 13354 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     2935 | 13355 | `	sxu32 nIdx = 0;` |
|        - | 13356 | `	SyString sName;` |
|        - | 13357 | `	sxi32 rc;` |
|        - | 13358 | `	/* Jump the 'unset' keyword */` |
|     2935 | 13359 | `	pGen->pIn++;` |
|        - | 13360 | `	/* Save delimiter */` |
|     2935 | 13361 | `	pTmp = pGen->pEnd;` |
|        - | 13362 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     2935 | 13363 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     2935 | 13364 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        - | 13365 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|        - | 13366 | `		SyToken *pClose;` |
|     2935 | 13367 | `		pGen->pIn++;   /* Skip '(' */` |
|     2935 | 13368 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     2935 | 13369 | `		pEnd = pClose; /* Stop at ')' */` |
|     1465 | 13370 | `	}` |
|     2935 | 13371 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|        - | 13372 | `	/* Resolve the 'unset' builtin name once */` |
|     2935 | 13373 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      379 | 13374 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      379 | 13375 | `		if( pObj == 0 ){` |
|      ! 0 | 13376 | `			return SXERR_ABORT;` |
|        - | 13377 | `		}` |
|      379 | 13378 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      379 | 13379 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      187 | 13380 | `	}` |
|        - | 13381 | `	/* Compile each comma-separated argument */` |
|     9789 | 13382 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     6859 | 13383 | `		if( pGen->pIn < pNext ){` |
|     6859 | 13384 | `			pGen->pEnd = pNext;` |
|     6859 | 13385 | `			rc = PH7_CompileExpr(&(*pGen),` |
|        - | 13386 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|        - | 13387 | `				GenStateUnsetValidator);` |
|     6859 | 13388 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 13389 | `				return SXERR_ABORT;` |
|        - | 13390 | `			}` |
|     6859 | 13391 | `			if( rc != SXERR_EMPTY ){` |
|        - | 13392 | `				/* Emit call for this single argument */` |
|     6857 | 13393 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     6857 | 13394 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|     6857 | 13395 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     3426 | 13396 | `			}` |
|     3427 | 13397 | `		}` |
|        - | 13398 | `		/* Jump trailing commas */` |
|    10785 | 13399 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|     3931 | 13400 | `			pNext++;` |
|        5 | 13401 | `		}` |
|     6859 | 13402 | `		pGen->pIn = pNext;` |
|        5 | 13403 | `	}` |
|        - | 13404 | `	/* Skip past the closing ')' if present */` |
|     2935 | 13405 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     2935 | 13406 | `		pGen->pIn++;` |
|     1465 | 13407 | `	}` |
|        - | 13408 | `	/* Restore token stream */` |
|     2935 | 13409 | `	pGen->pEnd = pTmp;` |
|     2935 | 13410 | `	return SXRET_OK;` |
|     1470 | 13411 | `}` |
|        - | 13412 | `/*` |
|        - | 13413 | ` * PHP Language construct table.` |
|        - | 13414 | ` */` |
|        - | 13415 | `static const LangConstruct aLangConstruct[] = {` |
|        - | 13416 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|        - | 13417 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|        - | 13418 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|        - | 13419 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|        - | 13420 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|        - | 13421 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|        - | 13422 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|        - | 13423 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|        - | 13424 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|        - | 13425 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|        - | 13426 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|        - | 13427 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|        - | 13428 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|        - | 13429 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|        - | 13430 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|        - | 13431 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|        - | 13432 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|        - | 13433 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|        - | 13434 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|        - | 13435 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|        - | 13436 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|        - | 13437 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|        - | 13438 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|        - | 13439 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|        - | 13440 | `};` |
|        - | 13441 | `/*` |
|        - | 13442 | ` * Return a pointer to the statement handler routine associated` |
|        - | 13443 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|        - | 13444 | ` */` |
|  3814890 | 13445 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|        - | 13446 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|        - | 13447 | `	SyToken *pLookahed  /* Look-ahead token */` |
|        - | 13448 | `	)` |
|        5 | 13449 | `{` |
|  3814895 | 13450 | `	sxu32 n = 0;` |
| 15531890 | 13451 | `	for(;;){` |
| 31063785 | 13452 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   246891 | 13453 | `			break;` |
|        - | 13454 | `		}` |
| 30816899 | 13455 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  3568009 | 13456 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|      ! 0 | 13457 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|      ! 0 | 13458 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|        - | 13459 | `					/* 'static' (class context),return null */` |
|      ! 0 | 13460 | `					return 0;` |
|        - | 13461 | `				}` |
|      ! 0 | 13462 | `			}` |
|  3568004 | 13463 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       14 | 13464 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       14 | 13465 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|        - | 13466 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|        3 | 13467 | `				return 0;` |
|        - | 13468 | `			}` |
|        - | 13469 | `			/* Return a pointer to the handler.` |
|        - | 13470 | `			*/` |
|  3568007 | 13471 | `			return aLangConstruct[n].xConstruct;` |
|        - | 13472 | `		}` |
| 27248895 | 13473 | `		n++;` |
|        5 | 13474 | `	}` |
|   246891 | 13475 | `	if( pLookahed ){` |
|   246891 | 13476 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    46713 | 13477 | `			return PH7_CompileClassInterface;` |
|   200183 | 13478 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   187977 | 13479 | `			return PH7_CompileClass;` |
|    12211 | 13480 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|       77 | 13481 | `			return PH7_CompileTrait;` |
|        - | 13482 | `		}` |
|        - | 13483 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|        - | 13484 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|        - | 13485 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|        - | 13486 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     6067 | 13487 | `	}` |
|        - | 13488 | `	/* Not a language construct */` |
|    12139 | 13489 | `	return 0;` |
|  1907450 | 13490 | `}` |
|        - | 13491 | `/*` |
|        - | 13492 | ` * Check if the given keyword is in fact a PHP language construct.` |
|        - | 13493 | ` * Return TRUE on success. FALSE otheriwse.` |
|        - | 13494 | ` */` |
|    12136 | 13495 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|        5 | 13496 | `{` |
|        - | 13497 | `	int rc;` |
|    12141 | 13498 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|    12141 | 13499 | `	if( rc == FALSE ){` |
|    12022 | 13500 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      366 | 13501 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|        - | 13502 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|        - | 13503 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|        - | 13504 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|        - | 13505 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|        - | 13506 | `			*/` |
|        - | 13507 | `			){` |
|    12019 | 13508 | `				rc = TRUE;` |
|     6007 | 13509 | `		}` |
|     6011 | 13510 | `	}` |
|    12141 | 13511 | `	return rc;` |
|        5 | 13512 | `}` |
|        - | 13513 | `/*` |
|        - | 13514 | ` * Compile a PHP chunk.` |
|        - | 13515 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|        - | 13516 | ` * takes care of generating the appropriate error message.` |
|        - | 13517 | ` */` |
|        - | 13518 | `/*` |
|        - | 13519 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|        - | 13520 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|        - | 13521 | ` * the chunk token set it becomes the pending docblock. An existing` |
|        - | 13522 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|        - | 13523 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|        - | 13524 | ` * intervening non-declaration statements.` |
|        - | 13525 | ` */` |
|  7989798 | 13526 | `static void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|        5 | 13527 | `{` |
|  7989803 | 13528 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  7989803 | 13529 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  7989803 | 13530 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|        - | 13531 | `	sxu32 nIdx, n;` |
|  7989798 | 13532 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|  1537013 | 13533 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|        - | 13534 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|        - | 13535 | `		 * indexes do not map to the sidecar */` |
|  6452797 | 13536 | `		return;` |
|        - | 13537 | `	}` |
|  1537011 | 13538 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|        - | 13539 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|        - | 13540 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|  1537011 | 13541 | `	SySetReset(&pGen->aPendingAttrs);` |
|  4612517 | 13542 | `	for( n = 0 ; n < nT ; n++ ){` |
|  3075511 | 13543 | `		if( aT[n].nTokIdx != nIdx ){` |
|  3067579 | 13544 | `			continue;` |
|        - | 13545 | `		}` |
|     7937 | 13546 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|       29 | 13547 | `			pGen->sPendingDoc = aT[n].sText;` |
|     7925 | 13548 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|     7913 | 13549 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|     3954 | 13550 | `		}` |
|     3971 | 13551 | `	}` |
|  3994904 | 13552 | `}` |
|        - | 13553 | `/*` |
|        - | 13554 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|        - | 13555 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|        - | 13556 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|        - | 13557 | ` */` |
|  2130898 | 13558 | `static void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|        5 | 13559 | `{` |
|        - | 13560 | `	char *zDup;` |
|  2130903 | 13561 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|  2130883 | 13562 | `		return;` |
|        - | 13563 | `	}` |
|       35 | 13564 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       10 | 13565 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|       25 | 13566 | `	if( zDup ){` |
|       25 | 13567 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|       10 | 13568 | `	}` |
|       25 | 13569 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|  1065454 | 13570 | `}` |
|        - | 13571 | `/*` |
|        - | 13572 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|        - | 13573 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|        - | 13574 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|        - | 13575 | ` * names may point into the token text, which must outlive the raw script` |
|        - | 13576 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|        - | 13577 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|        - | 13578 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|        - | 13579 | ` */` |
|     7920 | 13580 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|        5 | 13581 | `{` |
|        - | 13582 | `	SySet *pToken;` |
|        - | 13583 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|        - | 13584 | `	char *zSpan;` |
|     7925 | 13585 | `	sxi32 rc = SXRET_OK;` |
|     7925 | 13586 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|      ! 0 | 13587 | `		return SXRET_OK;` |
|        - | 13588 | `	}` |
|    11885 | 13589 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     3960 | 13590 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|     7925 | 13591 | `	if( zSpan == 0 ){` |
|      ! 0 | 13592 | `		return SXRET_OK;` |
|        - | 13593 | `	}` |
|        - | 13594 | `	/* The token set must outlive compilation too: interned operands may` |
|        - | 13595 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|        - | 13596 | `	 * the number of attribute declarations in the program. */` |
|     7925 | 13597 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|     7925 | 13598 | `	if( pToken == 0 ){` |
|      ! 0 | 13599 | `		return SXRET_OK;` |
|        - | 13600 | `	}` |
|     7925 | 13601 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|     7925 | 13602 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|     7925 | 13603 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|     7925 | 13604 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|     7925 | 13605 | `	pSavedIn = pGen->pIn;` |
|     7925 | 13606 | `	pSavedEnd = pGen->pEnd;` |
|     7929 | 13607 | `	while( pIn < pEnd ){` |
|        - | 13608 | `		ph7_attribute sAttr;` |
|        - | 13609 | `		SyBlob sFQN;` |
|     7929 | 13610 | `		int bAbsolute = 0;` |
|     7929 | 13611 | `		SyZero(&sAttr,sizeof(sAttr));` |
|     7929 | 13612 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|     7929 | 13613 | `		sAttr.nLine = pIn->nLine;` |
|     7929 | 13614 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|       75 | 13615 | `			bAbsolute = 1;` |
|       75 | 13616 | `			pIn++;` |
|       35 | 13617 | `		}` |
|     7929 | 13618 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     7929 | 13619 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     7929 | 13620 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|     7929 | 13621 | `			pIn++;` |
|     7929 | 13622 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|      ! 0 | 13623 | `				SyBlobAppend(&sFQN,"\\",1);` |
|      ! 0 | 13624 | `				pIn++;` |
|      ! 0 | 13625 | `				continue;` |
|        - | 13626 | `			}` |
|     7929 | 13627 | `			break;` |
|      ! 0 | 13628 | `		}` |
|     7929 | 13629 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|        - | 13630 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|        - | 13631 | `			 * this feature; never turn it into a new fatal) */` |
|      ! 0 | 13632 | `			SyBlobRelease(&sFQN);` |
|      ! 0 | 13633 | `			break;` |
|        - | 13634 | `		}` |
|        - | 13635 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|        - | 13636 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|        - | 13637 | `		{` |
|     7929 | 13638 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|     7929 | 13639 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|     7929 | 13640 | `			char *zDup = 0;` |
|     7929 | 13641 | `			if( !bAbsolute ){` |
|     7859 | 13642 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|     7859 | 13643 | `				if( pImp ){` |
|      ! 0 | 13644 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|      ! 0 | 13645 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|      ! 0 | 13646 | `					if( zDup ){` |
|      ! 0 | 13647 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|      ! 0 | 13648 | `					}` |
|     7859 | 13649 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        - | 13650 | `					SyBlob sTmp;` |
|      ! 0 | 13651 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|      ! 0 | 13652 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      ! 0 | 13653 | `					SyBlobAppend(&sTmp,"\\",1);` |
|      ! 0 | 13654 | `					SyBlobAppend(&sTmp,zName,nName);` |
|      ! 0 | 13655 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      ! 0 | 13656 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|      ! 0 | 13657 | `					if( zDup ){` |
|      ! 0 | 13658 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|      ! 0 | 13659 | `					}` |
|      ! 0 | 13660 | `					SyBlobRelease(&sTmp);` |
|      ! 0 | 13661 | `				}` |
|     3927 | 13662 | `			}` |
|     7929 | 13663 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|     7929 | 13664 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|     7929 | 13665 | `				if( zDup ){` |
|     7929 | 13666 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|     3962 | 13667 | `				}` |
|     3962 | 13668 | `			}` |
|        - | 13669 | `		}` |
|     7929 | 13670 | `		SyBlobRelease(&sFQN);` |
|     7929 | 13671 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        - | 13672 | `			SyToken *pArgsEnd;` |
|     7827 | 13673 | `			pIn++;` |
|     7827 | 13674 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|    15663 | 13675 | `			while( pIn < pArgsEnd ){` |
|     7841 | 13676 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|     7841 | 13677 | `				sxi32 iDepth = 0;` |
|        - | 13678 | `				ph7_attr_arg sArgRec;` |
|    77925 | 13679 | `				while( pArgStop < pArgsEnd ){` |
|    70105 | 13680 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       11 | 13681 | `						iDepth++;` |
|    70100 | 13682 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       11 | 13683 | `						iDepth--;` |
|    70090 | 13684 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|       17 | 13685 | `						break;` |
|        - | 13686 | `					}` |
|    70089 | 13687 | `					pArgStop++;` |
|        5 | 13688 | `				}` |
|     7841 | 13689 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|     7841 | 13690 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     7836 | 13691 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|     7820 | 13692 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|       28 | 13693 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        9 | 13694 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|       19 | 13695 | `					if( zN ){` |
|       19 | 13696 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|        9 | 13697 | `					}` |
|       19 | 13698 | `					pArgStart += 2;` |
|        9 | 13699 | `				}` |
|     7841 | 13700 | `				if( pArgStart < pArgStop ){` |
|        - | 13701 | `					SySet *pInstrContainer;` |
|     7841 | 13702 | `					pGen->pIn = pArgStart;` |
|     7841 | 13703 | `					pGen->pEnd = pArgStop;` |
|     7841 | 13704 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     7841 | 13705 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|     7841 | 13706 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|     7841 | 13707 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|     7841 | 13708 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     7841 | 13709 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 13710 | `						pGen->pIn = pSavedIn;` |
|      ! 0 | 13711 | `						pGen->pEnd = pSavedEnd;` |
|      ! 0 | 13712 | `						return SXERR_ABORT;` |
|        - | 13713 | `					}` |
|     7841 | 13714 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|     3918 | 13715 | `				}` |
|     7841 | 13716 | `				pIn = pArgStop;` |
|     7841 | 13717 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|       17 | 13718 | `					pIn++;` |
|        8 | 13719 | `				}` |
|        5 | 13720 | `			}` |
|     7827 | 13721 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|     3911 | 13722 | `		}` |
|     7929 | 13723 | `		SySetPut(pOut,(const void *)&sAttr);` |
|     7929 | 13724 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        5 | 13725 | `			pIn++;` |
|        5 | 13726 | `			continue;` |
|        - | 13727 | `		}` |
|     7925 | 13728 | `		break;` |
|      ! 0 | 13729 | `	}` |
|     7925 | 13730 | `	pGen->pIn = pSavedIn;` |
|     7925 | 13731 | `	pGen->pEnd = pSavedEnd;` |
|     7925 | 13732 | `	return SXRET_OK;` |
|     3965 | 13733 | `}` |
|        - | 13734 | `/*` |
|        - | 13735 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|        - | 13736 | ` * every recorded group into pOut and clear the pending list.` |
|        - | 13737 | ` */` |
|  2130902 | 13738 | `static sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|        5 | 13739 | `{` |
|  2130907 | 13740 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|        - | 13741 | `	sxu32 n;` |
|        - | 13742 | `	sxi32 rc;` |
|  2138815 | 13743 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|     7913 | 13744 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|     7913 | 13745 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 13746 | `			return SXERR_ABORT;` |
|        - | 13747 | `		}` |
|     3959 | 13748 | `	}` |
|  2130907 | 13749 | `	SySetReset(&pGen->aPendingAttrs);` |
|  2130907 | 13750 | `	return SXRET_OK;` |
|  1065456 | 13751 | `}` |
|        - | 13752 | `/*` |
|        - | 13753 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|        - | 13754 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|        - | 13755 | ` * the main token stream, so the sidecar indexes map directly.` |
|        - | 13756 | ` */` |
|   718202 | 13757 | `static sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|        5 | 13758 | `{` |
|   718207 | 13759 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   718207 | 13760 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   718207 | 13761 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|        - | 13762 | `	sxu32 nIdx, n;` |
|        - | 13763 | `	sxi32 rc;` |
|   718202 | 13764 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   194535 | 13765 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   523677 | 13766 | `		return SXRET_OK;` |
|        - | 13767 | `	}` |
|   194535 | 13768 | `	nIdx = (sxu32)(pTok - pBase);` |
|   583593 | 13769 | `	for( n = 0 ; n < nT ; n++ ){` |
|   389063 | 13770 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|       13 | 13771 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|       13 | 13772 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 13773 | `				return SXERR_ABORT;` |
|        - | 13774 | `			}` |
|        6 | 13775 | `		}` |
|   194534 | 13776 | `	}` |
|   194535 | 13777 | `	return SXRET_OK;` |
|   359106 | 13778 | `}` |
|  5876590 | 13779 | `static sxi32 GenStateCompileChunk(` |
|        - | 13780 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 13781 | `	sxi32 iFlags         /* Compile flags */` |
|        - | 13782 | `	)` |
|        5 | 13783 | `{` |
|        - | 13784 | `	ProcLangConstruct xCons;` |
|        - | 13785 | `	sxi32 rc;` |
|  5876595 | 13786 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  3354415 | 13787 | `	for(;;){` |
|  6292715 | 13788 | `		int bStmtIsDeclare = 0;` |
|  6292715 | 13789 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 13790 | `			/* No more input to process */` |
|    53357 | 13791 | `			break;` |
|        - | 13792 | `		}` |
|        - | 13793 | `		/* Bind a directly-preceding docblock to this statement */` |
|  6239363 | 13794 | `		GenStateSetPendingDoc(&(*pGen));` |
|  6239363 | 13795 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|        - | 13796 | `			/* php: a statement-position attribute group must be followed by a` |
|        - | 13797 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|        - | 13798 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|        - | 13799 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|        - | 13800 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|     7831 | 13801 | `			int bAttrTarget = 0;` |
|     7826 | 13802 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|     3947 | 13803 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|     7773 | 13804 | `				bAttrTarget = 1;` |
|     3943 | 13805 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       59 | 13806 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       58 | 13807 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|       15 | 13808 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|        4 | 13809 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|        4 | 13810 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|        1 | 13811 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|       59 | 13812 | `					bAttrTarget = 1;` |
|       29 | 13813 | `				}` |
|       29 | 13814 | `			}` |
|     7831 | 13815 | `			if( !bAttrTarget ){` |
|      ! 0 | 13816 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 13817 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|      ! 0 | 13818 | `					&pGen->pIn->sData);` |
|      ! 0 | 13819 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 13820 | `					break;` |
|        - | 13821 | `				}` |
|      ! 0 | 13822 | `				SySetReset(&pGen->aPendingAttrs);` |
|      ! 0 | 13823 | `			}` |
|     3913 | 13824 | `		}` |
|        - | 13825 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|        - | 13826 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  6239363 | 13827 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  3842127 | 13828 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  3842127 | 13829 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|       47 | 13830 | `				bStmtIsDeclare = 1;` |
|       21 | 13831 | `			}` |
|  1921061 | 13832 | `		}` |
|  6239363 | 13833 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|        - | 13834 | `			/* Any non-declare top-level statement locks the strict_types` |
|        - | 13835 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|   416093 | 13836 | `			pGen->bStrictTypesLocked = 1;` |
|   208044 | 13837 | `		}` |
|  6239363 | 13838 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|        - | 13839 | `			/* Compile block */` |
|     3907 | 13840 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|     3907 | 13841 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 13842 | `				break;` |
|        - | 13843 | `			}` |
|     1956 | 13844 | `		}else{` |
|  6235461 | 13845 | `			xCons = 0;` |
|  6235461 | 13846 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|        - | 13847 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|        - | 13848 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|        - | 13849 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|    27263 | 13850 | `				xCons = PH7_CompileClassModifiers;` |
|  6221832 | 13851 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|        - | 13852 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|        - | 13853 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|       33 | 13854 | `				xCons = PH7_CompileEnum;` |
|  6208189 | 13855 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  3814895 | 13856 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        - | 13857 | `				/* Try to extract a language construct handler */` |
|  3814895 | 13858 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  3814895 | 13859 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|       13 | 13860 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 13861 | `						"Syntax error: Unexpected keyword '%z'",` |
|        8 | 13862 | `						&pGen->pIn->sData);` |
|        9 | 13863 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 13864 | `						break;` |
|        - | 13865 | `					}` |
|        - | 13866 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|        - | 13867 | `					 * this erroneous statement.` |
|        - | 13868 | `					 */` |
|        9 | 13869 | `					xCons = PH7_ErrorRecover;` |
|        4 | 13870 | `				}` |
|  4300730 | 13871 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    66517 | 13872 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|        - | 13873 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|      117 | 13874 | `				xCons = PH7_CompileLabel;` |
|       56 | 13875 | `			}` |
|  6235461 | 13876 | `			if( xCons == 0 ){` |
|        - | 13877 | `				/* Assume an expression an try to compile it */` |
|  2405301 | 13878 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  2405301 | 13879 | `				if(  rc != SXERR_EMPTY ){` |
|        - | 13880 | `					/* Pop l-value */` |
|  2405151 | 13881 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  1202573 | 13882 | `				}` |
|  1202653 | 13883 | `			}else{` |
|        - | 13884 | `				/* Go compile the sucker */` |
|  3830165 | 13885 | `				rc = xCons(&(*pGen));` |
|        - | 13886 | `			}` |
|  6235461 | 13887 | `			if( rc == SXERR_ABORT ){` |
|        - | 13888 | `				/* Request to abort compilation */` |
|       13 | 13889 | `				break;` |
|        - | 13890 | `			}` |
|        - | 13891 | `		}` |
|        - | 13892 | `		/* Ignore trailing semi-colons ';' */` |
| 10671947 | 13893 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  4432599 | 13894 | `			pGen->pIn++;` |
|        5 | 13895 | `		}` |
|  6239353 | 13896 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|        - | 13897 | `			/* Compile a single statement and return */` |
|  5823233 | 13898 | `			break;` |
|        - | 13899 | `		}` |
|        - | 13900 | `		/* LOOP ONE */` |
|        - | 13901 | `		/* LOOP TWO */` |
|        - | 13902 | `		/* LOOP THREE */` |
|        - | 13903 | `		/* LOOP FOUR */` |
|        5 | 13904 | `	}` |
|        - | 13905 | `	/* Return compilation status */` |
|  5876595 | 13906 | `	return rc;` |
|        5 | 13907 | `}` |
|        - | 13908 | `/*` |
|        - | 13909 | ` * Compile a Raw PHP chunk.` |
|        - | 13910 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|        - | 13911 | ` * takes care of generating the appropriate error message.` |
|        - | 13912 | ` */` |
|    53364 | 13913 | `static sxi32 PH7_CompilePHP(` |
|        - | 13914 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - | 13915 | `	SySet *pTokenSet,     /* Token set */` |
|        - | 13916 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|        - | 13917 | `	)` |
|        5 | 13918 | `{` |
|    53369 | 13919 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|        - | 13920 | `	sxi32 rc;` |
|        - | 13921 | `	/* Reset the token set (and its trivia sidecar) */` |
|    53369 | 13922 | `	SySetReset(&(*pTokenSet));` |
|    53369 | 13923 | `	SySetReset(&pGen->aTrivia);` |
|        - | 13924 | `	/* Mark as the default token set */` |
|    53369 | 13925 | `	pGen->pTokenSet = &(*pTokenSet);` |
|        - | 13926 | `	/* Advance the stream cursor */` |
|    53369 | 13927 | `	pGen->pRawIn++;` |
|        - | 13928 | `	/* Tokenize the PHP chunk first */` |
|    53369 | 13929 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|        - | 13930 | `	/* Point to the head and tail of the token stream. */` |
|    53369 | 13931 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|    53369 | 13932 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|    53369 | 13933 | `	if( is_expr ){` |
|      ! 0 | 13934 | `		rc = SXERR_EMPTY;` |
|      ! 0 | 13935 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - | 13936 | `			/* A simple expression,compile it */` |
|      ! 0 | 13937 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|      ! 0 | 13938 | `		}` |
|        - | 13939 | `		/* Emit the DONE instruction */` |
|      ! 0 | 13940 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      ! 0 | 13941 | `		return SXRET_OK;` |
|        - | 13942 | `	}` |
|    53369 | 13943 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|        - | 13944 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|        - | 13945 | `		/*` |
|        - | 13946 | `		 * Shortcut syntax for the 'echo' language construct.` |
|        - | 13947 | `		 * According to the PHP reference manual:` |
|        - | 13948 | `		 *  echo() also has a shortcut syntax, where you can` |
|        - | 13949 | `		 *  immediately follow` |
|        - | 13950 | `		 *  the opening tag with an equals sign as follows:` |
|        - | 13951 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|        - | 13952 | `		 * Symisc extension:` |
|        - | 13953 | `		 *   This short syntax works with all PHP opening` |
|        - | 13954 | `		 *   tags unlike the default PHP engine that handle` |
|        - | 13955 | `		 *   only short tag.` |
|        - | 13956 | `		 */` |
|        - | 13957 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|        3 | 13958 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|        3 | 13959 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|        3 | 13960 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|        3 | 13961 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|        3 | 13962 | `		if( rc != SXERR_EMPTY ){` |
|        3 | 13963 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        1 | 13964 | `		}` |
|        3 | 13965 | `		return SXRET_OK;` |
|        - | 13966 | `	}` |
|        - | 13967 | `	/* Compile the PHP chunk */` |
|    53367 | 13968 | `	rc = GenStateCompileChunk(pGen,0);` |
|        - | 13969 | `	/* Fix exceptions jumps */` |
|    53367 | 13970 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        - | 13971 | `	/* Fix gotos now, the jump destination is resolved */` |
|    53367 | 13972 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|        3 | 13973 | `		rc = SXERR_ABORT;` |
|        1 | 13974 | `	}` |
|        - | 13975 | `	/* Reset container */` |
|    53367 | 13976 | `	SySetReset(&pGen->aGoto);` |
|    53367 | 13977 | `	SySetReset(&pGen->aLabel);` |
|    53367 | 13978 | `	SySetReset(&pGen->aNullsafeJmp);` |
|        - | 13979 | `	/* Compilation result */` |
|    53367 | 13980 | `	return rc;` |
|    26687 | 13981 | `}` |
|        - | 13982 | `/*` |
|        - | 13983 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|        - | 13984 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|        - | 13985 | ` * This is the only compile interface exported from this file.` |
|        - | 13986 | ` */` |
|    56424 | 13987 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|        - | 13988 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|        - | 13989 | `	SyString *pScript,  /* Script to compile */` |
|        - | 13990 | `	sxi32 iFlags        /* Compile flags */` |
|        - | 13991 | `	)` |
|        5 | 13992 | `{` |
|        - | 13993 | `	SySet aPhpToken,aRawToken;` |
|        - | 13994 | `	ph7_gen_state *pCodeGen;` |
|        - | 13995 | `	ph7_value *pRawObj;` |
|        - | 13996 | `	sxu32 nObjIdx;` |
|        - | 13997 | `	sxi32 nRawObj;` |
|        - | 13998 | `	int is_expr;` |
|        - | 13999 | `	sxi8 bSavedStrict;` |
|        - | 14000 | `	sxi8 bSavedStrictLocked;` |
|        - | 14001 | `	sxi32 rc;` |
|    56429 | 14002 | `	if( pScript->nByte < 1 ){` |
|        - | 14003 | `		/* Nothing to compile */` |
|      ! 0 | 14004 | `		return PH7_OK;` |
|        - | 14005 | `	}` |
|        - | 14006 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|        - | 14007 | `	 * file's flags so include/require restore them on return. */` |
|    56429 | 14008 | `	pCodeGen = &pVm->sCodeGen;` |
|    56429 | 14009 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|    56429 | 14010 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|    56429 | 14011 | `	pCodeGen->bStrictTypes = 0;` |
|    56429 | 14012 | `	pCodeGen->bStrictTypesLocked = 0;` |
|        - | 14013 | `	/* Initialize the tokens containers */` |
|    56429 | 14014 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|    56429 | 14015 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|    56429 | 14016 | `	SySetAlloc(&aPhpToken,0xc0);` |
|    56429 | 14017 | `	is_expr = 0;` |
|    56429 | 14018 | `	if( iFlags & PH7_PHP_ONLY ){` |
|        - | 14019 | `		SyToken sTmp;` |
|        - | 14020 | `		/* PHP only: -*/` |
|    42827 | 14021 | `		sTmp.nLine = 1;` |
|    42827 | 14022 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    42827 | 14023 | `		sTmp.pUserData = 0;` |
|    42827 | 14024 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    42827 | 14025 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    42827 | 14026 | `		if( iFlags & PH7_PHP_EXPR ){` |
|        - | 14027 | `			/* A simple PHP expression */` |
|      ! 0 | 14028 | `			is_expr = 1;` |
|      ! 0 | 14029 | `		}` |
|    21416 | 14030 | `	}else{` |
|        - | 14031 | `		/* Tokenize raw text */` |
|    13607 | 14032 | `		SySetAlloc(&aRawToken,32);` |
|    13607 | 14033 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|        - | 14034 | `	}` |
|        - | 14035 | `	/* Process high-level tokens */` |
|    56429 | 14036 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|    56429 | 14037 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|    56429 | 14038 | `	rc = PH7_OK;` |
|    56429 | 14039 | `	if( is_expr ){` |
|        - | 14040 | `		/* Compile the expression */` |
|      ! 0 | 14041 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|      ! 0 | 14042 | `		goto cleanup;` |
|        - | 14043 | `	}` |
|    56429 | 14044 | `	nObjIdx = 0;` |
|        - | 14045 | `	/* Each compilation unit starts in the global namespace.` |
|        - | 14046 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|        - | 14047 | `	 * preventing namespace bleeding across include()d files. */` |
|    56429 | 14048 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|        - | 14049 | `	/* Start the compilation process */` |
|    35019 | 14050 | `	for(;;){` |
|   123395 | 14051 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|    56417 | 14052 | `			break; /* No more tokens to process */` |
|        - | 14053 | `		}` |
|    66983 | 14054 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|        - | 14055 | `			/* Compile the PHP chunk */` |
|    53369 | 14056 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|    53369 | 14057 | `			if( rc == SXERR_ABORT ){` |
|       15 | 14058 | `				break;` |
|        - | 14059 | `			}` |
|    53357 | 14060 | `			continue;` |
|        - | 14061 | `		}` |
|        - | 14062 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|    13619 | 14063 | `		nRawObj = 0;` |
|    27275 | 14064 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|        - | 14065 | `			/* Consume the raw chunk without any processing */` |
|    13661 | 14066 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|    13661 | 14067 | `			if( pRawObj == 0 ){` |
|      ! 0 | 14068 | `				rc = SXERR_MEM;` |
|      ! 0 | 14069 | `				break;` |
|        - | 14070 | `			}` |
|        - | 14071 | `			/* Mark as constant and emit the load constant instruction */` |
|    13661 | 14072 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|    13661 | 14073 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|    13661 | 14074 | `			++nRawObj;` |
|    13661 | 14075 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|        5 | 14076 | `		}` |
|    13619 | 14077 | `		if( nRawObj > 0 ){` |
|        - | 14078 | `			/* Emit the consume instruction */` |
|    13619 | 14079 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     6807 | 14080 | `		}` |
|    28217 | 14081 | `	}` |
|    28212 | 14082 | `cleanup:` |
|    56429 | 14083 | `	SySetRelease(&aRawToken);` |
|    56429 | 14084 | `	SySetRelease(&aPhpToken);` |
|        - | 14085 | `	/* Restore outer file's strict_types scope */` |
|    56429 | 14086 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|    56429 | 14087 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|    56429 | 14088 | `	return rc;` |
|    28217 | 14089 | `}` |
|        - | 14090 | `/*` |
|        - | 14091 | ` * Utility routines.Initialize the code generator.` |
|        - | 14092 | ` */` |
|     3884 | 14093 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|        - | 14094 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 14095 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|        - | 14096 | `	void *pErrData     /* Last argument to xErr() */` |
|        - | 14097 | `	)` |
|        5 | 14098 | `{` |
|     3889 | 14099 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 14100 | `	/* Zero the structure */` |
|     3889 | 14101 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|        - | 14102 | `	/* Initial state */` |
|     3889 | 14103 | `	pGen->pVm  = &(*pVm);` |
|     3889 | 14104 | `	pGen->xErr = xErr;` |
|     3889 | 14105 | `	pGen->pErrData = pErrData;` |
|     3889 | 14106 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|     3889 | 14107 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|     3889 | 14108 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|     3889 | 14109 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|     3889 | 14110 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|     3889 | 14111 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|     3889 | 14112 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|        - | 14113 | `	/* Error log buffer */` |
|     3889 | 14114 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|        - | 14115 | `	/* General purpose working buffer */` |
|     3889 | 14116 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|        - | 14117 | `	/* Namespace state */` |
|     3889 | 14118 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     3889 | 14119 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|     3889 | 14120 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|     3889 | 14121 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|        - | 14122 | `	/* Create the global scope */` |
|     3889 | 14123 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|        - | 14124 | `	/* Point to the global scope */` |
|     3889 | 14125 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     3889 | 14126 | `	return SXRET_OK;` |
|        5 | 14127 | `}` |
|        - | 14128 | `/*` |
|        - | 14129 | ` * Utility routines. Reset the code generator to it's initial state.` |
|        - | 14130 | ` */` |
|    59928 | 14131 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|        - | 14132 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 14133 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|        - | 14134 | `	void *pErrData     /* Last argument to xErr() */` |
|        - | 14135 | `	)` |
|        5 | 14136 | `{` |
|    59933 | 14137 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 14138 | `	GenBlock *pBlock,*pParent;` |
|        - | 14139 | `	/* Reset state */` |
|    59933 | 14140 | `	SySetReset(&pGen->aLabel);` |
|    59933 | 14141 | `	SySetReset(&pGen->aGoto);` |
|    59933 | 14142 | `	SySetReset(&pGen->aNullsafeJmp);` |
|    59933 | 14143 | `	SySetReset(&pGen->aTrivia);` |
|    59933 | 14144 | `	SySetReset(&pGen->aPendingAttrs);` |
|    59933 | 14145 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|    59933 | 14146 | `	SyBlobRelease(&pGen->sErrBuf);` |
|    59933 | 14147 | `	SyBlobRelease(&pGen->sWorker);` |
|    59933 | 14148 | `	SyBlobRelease(&pGen->sNamespace);` |
|    59933 | 14149 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    59933 | 14150 | `	SyHashRelease(&pGen->hUseImports);` |
|    59933 | 14151 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    59933 | 14152 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|    59933 | 14153 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    59933 | 14154 | `	SyHashRelease(&pGen->hUseConstImports);` |
|    59933 | 14155 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|        - | 14156 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|        - | 14157 | `	 * They intern variable names and literal strings that are referenced by` |
|        - | 14158 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|        - | 14159 | `	 * Releasing them would either leak the interned strings or require freeing` |
|        - | 14160 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|        - | 14161 | `	 * number of unique names, which is acceptable. */` |
|        - | 14162 | `	/* Point to the global scope */` |
|    59933 | 14163 | `	pBlock = pGen->pCurrent;` |
|    59933 | 14164 | `	while( pBlock->pParent != 0 ){` |
|      ! 0 | 14165 | `		pParent = pBlock->pParent;` |
|      ! 0 | 14166 | `		GenStateFreeBlock(pBlock);` |
|      ! 0 | 14167 | `		pBlock = pParent;` |
|      ! 0 | 14168 | `	}` |
|    59933 | 14169 | `	pGen->xErr = xErr;` |
|    59933 | 14170 | `	pGen->pErrData = pErrData;` |
|    59933 | 14171 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    59933 | 14172 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|    59933 | 14173 | `	pGen->pIn = pGen->pEnd = 0;` |
|    59933 | 14174 | `	pGen->nErr = 0;` |
|    59933 | 14175 | `	return SXRET_OK;` |
|        5 | 14176 | `}` |
|        - | 14177 | `/*` |
|        - | 14178 | ` * Generate a compile-time error message.` |
|        - | 14179 | ` * If the error count limit is reached (usually 15 error message)` |
|        - | 14180 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|        - | 14181 | ` * abort compilation immediately.` |
|        - | 14182 | ` */` |
|      652 | 14183 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|        5 | 14184 | `{` |
|      657 | 14185 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|      657 | 14186 | `	const char *zErr = "Error";` |
|        - | 14187 | `	SyString *pFile;` |
|        - | 14188 | `	va_list ap;` |
|        - | 14189 | `	sxi32 rc;` |
|        - | 14190 | `	/* Reset the working buffer */` |
|      657 | 14191 | `	SyBlobReset(pWorker);` |
|        - | 14192 | `	/* Peek the processed file path if available */` |
|      657 | 14193 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|      657 | 14194 | `	if( nErrType == E_ERROR ){` |
|        - | 14195 | `		/* Increment the error counter */` |
|      543 | 14196 | `		pGen->nErr++;` |
|      543 | 14197 | `		if( pGen->nErr > 15 ){` |
|        - | 14198 | `			/* Error count limit reached */` |
|        6 | 14199 | `			if( pGen->xErr ){` |
|        6 | 14200 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|        6 | 14201 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|        6 | 14202 | `				if( pFile ){` |
|        6 | 14203 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|        2 | 14204 | `				}` |
|        6 | 14205 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|        6 | 14206 | `				if( SyBlobLength(pWorker) > 0 ){` |
|        6 | 14207 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|        2 | 14208 | `				}` |
|        2 | 14209 | `			}` |
|        - | 14210 | `			/* Abort immediately */` |
|        6 | 14211 | `			return SXERR_ABORT;` |
|        - | 14212 | `		}` |
|      267 | 14213 | `	}` |
|      653 | 14214 | `	if( pGen->xErr == 0 ){` |
|        - | 14215 | `		/* No available error consumer,return immediately */` |
|        3 | 14216 | `		return SXRET_OK;` |
|        - | 14217 | `	}` |
|      650 | 14218 | `	switch(nErrType){` |
|      536 | 14219 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|       32 | 14220 | `	case E_WARNING: zErr = "Warning";     break;` |
|       82 | 14221 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       12 | 14222 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|      ! 0 | 14223 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|      ! 0 | 14224 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|      ! 0 | 14225 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|      ! 0 | 14226 | `	default:` |
|      ! 0 | 14227 | `		break;` |
|        - | 14228 | `	}` |
|      650 | 14229 | `	rc = SXRET_OK;` |
|        - | 14230 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|      650 | 14231 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|      650 | 14232 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|      650 | 14233 | `	va_start(ap,zFormat);` |
|      650 | 14234 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|      650 | 14235 | `	va_end(ap);` |
|      650 | 14236 | `	if( pFile ){` |
|      650 | 14237 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|      323 | 14238 | `	}` |
|        - | 14239 | `	/* Append a new line */` |
|      650 | 14240 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|      650 | 14241 | `	if( SyBlobLength(pWorker) > 0 ){` |
|        - | 14242 | `		/* Consume the generated error message */` |
|      650 | 14243 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|      323 | 14244 | `	}` |
|      650 | 14245 | `	return rc;` |
|      331 | 14246 | `}` |
|        - | 14247 |  |
