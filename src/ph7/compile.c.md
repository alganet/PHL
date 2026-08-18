# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 6688/8277 lines (80.80%)

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
|  5861414 |   161 | `static void GenStateInitBlock(` |
|        - |   162 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |   163 | `	GenBlock *pBlock,    /* Target block */` |
|        - |   164 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|        - |   165 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|        - |   166 | `	void *pUserData      /* Upper layer private data */` |
|        - |   167 | `	)` |
|        5 |   168 | `{` |
|        - |   169 | `	/* Initialize block fields */` |
|  5861419 |   170 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  5861419 |   171 | `	pBlock->pUserData   = pUserData;` |
|  5861419 |   172 | `	pBlock->pGen        = pGen;` |
|  5861419 |   173 | `	pBlock->iFlags      = iType;` |
|  5861419 |   174 | `	pBlock->pParent     = 0;` |
|  5861419 |   175 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  5861419 |   176 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  5861419 |   177 | `}` |
|        - |   178 | `/*` |
|        - |   179 | ` * Allocate a new block instance.` |
|        - |   180 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|        - |   181 | ` * on success.Otherwise generate a compile-time error and abort` |
|        - |   182 | ` * processing on failure.` |
|        - |   183 | ` */` |
|  5857530 |   184 | `static sxi32 GenStateEnterBlock(` |
|        - |   185 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - |   186 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|        - |   187 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|        - |   188 | `	void *pUserData,      /* Upper layer private data */` |
|        - |   189 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|        - |   190 | `	)` |
|        5 |   191 | `{` |
|        - |   192 | `	GenBlock *pBlock;` |
|        - |   193 | `	/* Allocate a new block instance */` |
|  5857535 |   194 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  5857535 |   195 | `	if( pBlock == 0 ){` |
|        - |   196 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   197 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   198 | `		 */` |
|      ! 0 |   199 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|        - |   200 | `		/* Abort processing immediately */` |
|      ! 0 |   201 | `		return SXERR_ABORT;` |
|        - |   202 | `	}` |
|        - |   203 | `	/* Zero the structure */` |
|  5857535 |   204 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  5857535 |   205 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|        - |   206 | `	/* Link to the parent block */` |
|  5857535 |   207 | `	pBlock->pParent = pGen->pCurrent;` |
|        - |   208 | `	/* Mark as the current block */` |
|  5857535 |   209 | `	pGen->pCurrent = pBlock;` |
|  5857535 |   210 | `	if( ppBlock ){` |
|        - |   211 | `		/* Write a pointer to the new instance */` |
|  2838101 |   212 | `		*ppBlock = pBlock;` |
|  1419048 |   213 | `	}` |
|  5857535 |   214 | `	return SXRET_OK;` |
|  2928770 |   215 | `}` |
|        - |   216 | `/*` |
|        - |   217 | ` * Release block fields without freeing the whole instance.` |
|        - |   218 | ` */` |
|  5857522 |   219 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|        5 |   220 | `{` |
|  5857527 |   221 | `	SySetRelease(&pBlock->aPostContFix);` |
|  5857527 |   222 | `	SySetRelease(&pBlock->aJumpFix);` |
|  5857527 |   223 | `}` |
|        - |   224 | `/*` |
|        - |   225 | ` * Release a block.` |
|        - |   226 | ` */` |
|  5857522 |   227 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|        5 |   228 | `{` |
|  5857527 |   229 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  5857527 |   230 | `	GenStateReleaseBlock(&(*pBlock));` |
|        - |   231 | `	/* Free the instance */` |
|  5857527 |   232 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  5857527 |   233 | `}` |
|        - |   234 | `/*` |
|        - |   235 | ` * POP and release a block from the stack of compiled blocks.` |
|        - |   236 | ` */` |
|  5857522 |   237 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|        5 |   238 | `{` |
|  5857527 |   239 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  5857527 |   240 | `	if( pBlock == 0 ){` |
|        - |   241 | `		/* No more block to pop */` |
|      ! 0 |   242 | `		return SXERR_EMPTY;` |
|        - |   243 | `	}` |
|        - |   244 | `	/* Point to the upper block */` |
|  5857527 |   245 | `	pGen->pCurrent = pBlock->pParent;` |
|  5857527 |   246 | `	if( ppBlock ){` |
|        - |   247 | `		/* Write a pointer to the popped block */` |
|      ! 0 |   248 | `		*ppBlock = pBlock;` |
|      ! 0 |   249 | `	}else{` |
|        - |   250 | `		/* Safely release the block */` |
|  5857527 |   251 | `		GenStateFreeBlock(&(*pBlock));` |
|        - |   252 | `	}` |
|  5857527 |   253 | `	return SXRET_OK;` |
|  2928766 |   254 | `}` |
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
|  2212802 |   265 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|        5 |   266 | `{` |
|        - |   267 | `	JumpFixup sJumpFix;` |
|        - |   268 | `	sxi32 rc;` |
|        - |   269 | `	/* Init the JumpFixup structure */` |
|  2212807 |   270 | `	sJumpFix.nJumpType = nJumpType;` |
|  2212807 |   271 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|        - |   272 | `	/* Insert in the jump fixup table */` |
|  2212807 |   273 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  2212807 |   274 | `	return rc;` |
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
|  4168124 |   287 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|        5 |   288 | `{` |
|        - |   289 | `	JumpFixup *aFix;` |
|        - |   290 | `	VmInstr *pInstr;` |
|        - |   291 | `	sxu32 nFixed;` |
|        - |   292 | `	sxu32 n;` |
|        - |   293 | `	/* Point to the jump fixup table */` |
|  4168129 |   294 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|        - |   295 | `	/* Fix the desired jumps */` |
|  8106739 |   296 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  3938615 |   297 | `		if( aFix[n].nJumpType < 0 ){` |
|        - |   298 | `			/* Already fixed */` |
|  1414241 |   299 | `			continue;` |
|        - |   300 | `		}` |
|  2524379 |   301 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|        - |   302 | `			/* Not of our interest */` |
|   311579 |   303 | `			continue;` |
|        - |   304 | `		}` |
|        - |   305 | `		/* Point to the instruction to fix */` |
|  2212805 |   306 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  2212805 |   307 | `		if( pInstr ){` |
|  2212805 |   308 | `			pInstr->iP2 = nJumpDest;` |
|  2212805 |   309 | `			nFixed++;` |
|        - |   310 | `			/* Mark as fixed */` |
|  2212805 |   311 | `			aFix[n].nJumpType = -1;` |
|  1106400 |   312 | `		}` |
|  1106405 |   313 | `	}` |
|        - |   314 | `	/* Total number of fixed jumps */` |
|  4168129 |   315 | `	return nFixed;` |
|        5 |   316 | `}` |
|        - |   317 | `/*` |
|        - |   318 | ` * Fix a 'goto' now the jump destination is resolved.` |
|        - |   319 | ` * The goto statement can be used to jump to another section` |
|        - |   320 | ` * in the program.` |
|        - |   321 | ` * Refer to the routine responsible of compiling the goto` |
|        - |   322 | ` * statement for more information.` |
|        - |   323 | ` */` |
|  1466618 |   324 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|        5 |   325 | `{` |
|        - |   326 | `	JumpFixup *pJump,*aJumps;` |
|        - |   327 | `	Label *pLabel,*aLabel;` |
|        - |   328 | `	VmInstr *pInstr;` |
|        - |   329 | `	sxi32 rc;` |
|        - |   330 | `	sxu32 n;` |
|        - |   331 | `	/* Point to the goto table */` |
|  1466623 |   332 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|        - |   333 | `	/* Fix */` |
|  1466769 |   334 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  1466621 |   359 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  1466753 |   360 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|      137 |   361 | `		if( aLabel[n].bRef == FALSE ){` |
|        - |   362 | `			/* Emit a warning */` |
|       40 |   363 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|       24 |   364 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|       12 |   365 | `		}` |
|       71 |   366 | `	}` |
|  1466621 |   367 | `	return SXRET_OK;` |
|   733314 |   368 | `}` |
|        - |   369 | `/*` |
|        - |   370 | ` * Check if a given token value is installed in the literal table.` |
|        - |   371 | ` */` |
|  7357742 |   372 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|        5 |   373 | `{` |
|        - |   374 | `	SyHashEntry *pEntry;` |
|  7357747 |   375 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  7357747 |   376 | `	if( pEntry == 0 ){` |
|  1938357 |   377 | `		return SXERR_NOTFOUND;` |
|        - |   378 | `	}` |
|  5419395 |   379 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  5419395 |   380 | `	return SXRET_OK;` |
|  3678876 |   381 | `}` |
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
|  1938352 |   392 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|        5 |   393 | `{` |
|  1938357 |   394 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  1938357 |   395 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   969176 |   396 | `	}` |
|  1938357 |   397 | `	return SXRET_OK;` |
|        5 |   398 | `}` |
|        - |   399 | `/*` |
|        - |   400 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|        - |   401 | ` * in the constant table.` |
|        - |   402 | ` */` |
|  1295858 |   403 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|        5 |   404 | `{` |
|        - |   405 | `	ph7_value *pObj;` |
|  1295863 |   406 | `	sxu32 nIdx = 0; /* cc warning */` |
|        - |   407 | `	/* Reserve a new constant */` |
|  1295863 |   408 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  1295863 |   409 | `	if( pObj == 0 ){` |
|      ! 0 |   410 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |   411 | `		return 0;` |
|        - |   412 | `	}` |
|  1295863 |   413 | `	*pIdx = nIdx;` |
|        - |   414 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|        - |   415 | `	 * the constant string iterals table [optimization purposes].` |
|        - |   416 | `	 */` |
|  1295863 |   417 | `	return pObj;` |
|   647934 |   418 | `}` |
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
|  3706882 |   433 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|        5 |   434 | `{` |
|        - |   435 | `	VmCallArgMap *pMap;` |
|  3706887 |   436 | `	if( !pGen->bStrictTypes ) return p3;` |
|       39 |   437 | `	if( p3 == 0 ){` |
|       35 |   438 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|       35 |   439 | `		if( pMap == 0 ) return 0;` |
|       35 |   440 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|       35 |   441 | `		p3 = (void *)pMap;` |
|       16 |   442 | `	}` |
|       39 |   443 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|       39 |   444 | `	return p3;` |
|  1853446 |   445 | `}` |
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
|  1296846 |   509 | `static int GenStateFindBadNumericSeparator(` |
|        - |   510 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|        5 |   511 | `{` |
|  1296851 |   512 | `	const char *z = pRaw->zString;` |
|  1296851 |   513 | `	sxu32 n = pRaw->nByte;` |
|  1296851 |   514 | `	int base = 10;` |
|        - |   515 | `	sxu32 i, start;` |
|  1296851 |   516 | `	if( n < 2 ) return 0;` |
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
|   648428 |   541 | `}` |
|        - |   542 | `/*` |
|        - |   543 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|        - |   544 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|        - |   545 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|        - |   546 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|        - |   547 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|        - |   548 | ` * so callers can bail from the current construct).` |
|        - |   549 | ` */` |
|  1296846 |   550 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|        5 |   551 | `{` |
|  1296851 |   552 | `	const char *zBad = 0;` |
|  1296851 |   553 | `	sxu32 nBad = 0;` |
|        - |   554 | `	SyString sBad;` |
|        - |   555 | `	sxi32 rc;` |
|  1296851 |   556 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  1296837 |   557 | `		return SXRET_OK;` |
|        - |   558 | `	}` |
|       18 |   559 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|       18 |   560 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|        - |   561 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|       18 |   562 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |   563 | `		return SXERR_ABORT;` |
|        - |   564 | `	}` |
|       18 |   565 | `	return SXERR_SYNTAX;` |
|   648428 |   566 | `}` |
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
|  1296832 |   583 | `static sxi32 GenStateStripNumericSeparators(` |
|        - |   584 | `	SyMemBackend *pAlloc,` |
|        - |   585 | `	const SyString *pToken,` |
|        - |   586 | `	char *zScratch, sxu32 nScratch,` |
|        - |   587 | `	SyString *pOut, char **pzAlloc)` |
|        5 |   588 | `{` |
|        - |   589 | `	sxu32 i, j;` |
|  1296837 |   590 | `	int hasUnderscore = 0;` |
|        - |   591 | `	char *zBuf;` |
|  1296837 |   592 | `	*pzAlloc = 0;` |
|  3090023 |   593 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  1793443 |   594 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   896598 |   595 | `	}` |
|  1296837 |   596 | `	if( !hasUnderscore ){` |
|  1296585 |   597 | `		SyStringDupPtr(pOut, pToken);` |
|  1296585 |   598 | `		return SXRET_OK;` |
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
|   648421 |   615 | `}` |
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
|  1295892 |   651 | `static int GenStateIntLiteralOverflows(const SyString *pNum, ph7_real *pReal, int *pbDecimal)` |
|        5 |   652 | `{` |
|  1295897 |   653 | `	const char *z = pNum->zString;` |
|  1295897 |   654 | `	const char *zEnd = z + pNum->nByte;` |
|        - |   655 | `	const char *p, *q;` |
|        - |   656 | `	int n;` |
|  1295897 |   657 | `	*pbDecimal = FALSE;` |
|  1295897 |   658 | `	if( z >= zEnd ){` |
|      ! 0 |   659 | `		return FALSE;` |
|        - |   660 | `	}` |
|  1295897 |   661 | `	if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'x' \|\| z[1] == 'X') ){` |
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
|  1295821 |   676 | `	}else if( z[0] == '0' && (z + 1) < zEnd && (z[1] == 'b' \|\| z[1] == 'B') ){` |
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
|  1295541 |   691 | `	}else if( z[0] == '0' ){` |
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
|   936077 |   712 | `	p = z;` |
|   936077 |   713 | `	while( p < zEnd && p[0] == '0' ){ p++; }` |
|  2363335 |   714 | `	for( q = p, n = 0; q < zEnd && (unsigned char)q[0] < 0xc0 && SyisDigit(q[0]); q++ ){ n++; }` |
|   936077 |   715 | `	if( n > 19 \|\| (n == 19 && SyMemcmp(p, "9223372036854775807", 19) > 0) ){` |
|       25 |   716 | `		*pbDecimal = TRUE;` |
|       25 |   717 | `		return TRUE;` |
|        - |   718 | `	}` |
|   936053 |   719 | `	return FALSE;` |
|   647951 |   720 | `}` |
|  1296818 |   721 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |   722 | `{` |
|  1296823 |   723 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  1296823 |   724 | `	sxu32 nIdx = 0;` |
|        - |   725 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  1296823 |   726 | `	char *zAlloc = 0;` |
|        - |   727 | `	SyString sNum;` |
|        - |   728 | `	sxi32 rc;` |
|   648409 |   729 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  1296823 |   730 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  1296823 |   731 | `	if( rc != SXRET_OK ){` |
|       14 |   732 | `		return rc;` |
|        - |   733 | `	}` |
|  1945217 |   734 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   648404 |   735 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  1296813 |   736 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   737 | `		return SXERR_ABORT;` |
|        - |   738 | `	}` |
|  1296813 |   739 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|        - |   740 | `		ph7_value *pObj;` |
|        - |   741 | `		sxi64 iValue;` |
|  1295897 |   742 | `		ph7_real rOverflow = 0;` |
|  1295897 |   743 | `		int bDecimalOverflow = 0;` |
|  1295897 |   744 | `		if( GenStateIntLiteralOverflows(&sNum,&rOverflow,&bDecimalOverflow) ){` |
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
|  1295863 |   761 | `			iValue = PH7_TokenValueToInt64(&sNum);` |
|  1295863 |   762 | `			pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  1295863 |   763 | `			if( pObj == 0 ){` |
|      ! 0 |   764 | `				if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|      ! 0 |   765 | `				return SXERR_ABORT;` |
|        - |   766 | `			}` |
|  1295863 |   767 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|        - |   768 | `		}` |
|   647951 |   769 | `	}else{` |
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
|  1296813 |   782 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        - |   783 | `	/* Emit the load constant instruction */` |
|  1296813 |   784 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - |   785 | `	/* Node successfully compiled */` |
|  1296813 |   786 | `	return SXRET_OK;` |
|   648414 |   787 | `}` |
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
|    38534 |  1070 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|        5 |  1071 | `{` |
|        - |  1072 | `	ph7_value *pConstObj;` |
|    38539 |  1073 | `	sxu32 nIdx = 0;` |
|        - |  1074 | `	/* Reserve a new constant */` |
|    38539 |  1075 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    38539 |  1076 | `	if( pConstObj == 0 ){` |
|      ! 0 |  1077 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  1078 | `		return 0;` |
|        - |  1079 | `	}` |
|    38539 |  1080 | `	(*pCount)++;` |
|    38539 |  1081 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|        - |  1082 | `	/* Emit the load constant instruction */` |
|    38539 |  1083 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    38539 |  1084 | `	return pConstObj;` |
|    19272 |  1085 | `}` |
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
|    37020 |  1148 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|        5 |  1149 | `{` |
|    37025 |  1150 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|        - |  1151 | `	const char *zIn,*zCur,*zEnd;` |
|    37025 |  1152 | `	ph7_value *pObj = 0;` |
|        - |  1153 | `	sxi32 iCons;` |
|        - |  1154 | `	sxi32 rc;` |
|        - |  1155 | `	/* Delimit the string */` |
|    37025 |  1156 | `	zIn  = pStr->zString;` |
|    37025 |  1157 | `	zEnd = &zIn[pStr->nByte];` |
|    37025 |  1158 | `	if( zIn >= zEnd ){` |
|        - |  1159 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|        - |  1160 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|        - |  1161 | `		 * literal table from growing when many "" literals appear in the source.` |
|        - |  1162 | `		 */` |
|      377 |  1163 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|      377 |  1164 | `		return SXRET_OK;` |
|        - |  1165 | `	}` |
|    36653 |  1166 | `	zCur = 0;` |
|        - |  1167 | `	/* Compile the node */` |
|    36653 |  1168 | `	iCons = 0;` |
|    19558 |  1169 | `	for(;;){` |
|    62997 |  1170 | `		zCur = zIn;` |
|   215349 |  1171 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|   154825 |  1172 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|       72 |  1173 | `				break;` |
|   154691 |  1174 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|     2338 |  1175 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     1170 |  1176 | `					break;` |
|        - |  1177 | `			}` |
|   152357 |  1178 | `			zIn++;` |
|        5 |  1179 | `		}` |
|    62997 |  1180 | `		if( zIn > zCur ){` |
|    20449 |  1181 | `			if( pObj == 0 ){` |
|    19919 |  1182 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    19919 |  1183 | `				if( pObj == 0 ){` |
|      ! 0 |  1184 | `					return SXERR_ABORT;` |
|        - |  1185 | `				}` |
|     9957 |  1186 | `			}` |
|    20449 |  1187 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    10222 |  1188 | `		}` |
|    62997 |  1189 | `		if( zIn >= zEnd ){` |
|    36651 |  1190 | `			break;` |
|        - |  1191 | `		}` |
|    26351 |  1192 | `		if( zIn[0] == '\\' ){` |
|    23883 |  1193 | `			const char *zPtr = 0;` |
|        - |  1194 | `			sxu32 n;` |
|    23883 |  1195 | `			zIn++;` |
|    23883 |  1196 | `			if( pObj == 0 ){` |
|    18625 |  1197 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    18625 |  1198 | `				if( pObj == 0 ){` |
|      ! 0 |  1199 | `					return SXERR_ABORT;` |
|        - |  1200 | `				}` |
|     9310 |  1201 | `			}` |
|    23883 |  1202 | `			if( zIn >= zEnd ){` |
|        - |  1203 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|        3 |  1204 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|        3 |  1205 | `				break;` |
|        - |  1206 | `			}` |
|    23881 |  1207 | `			n = sizeof(char); /* size of conversion */` |
|    23881 |  1208 | `			switch( zIn[0] ){` |
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
|    11386 |  1225 | `			case 'n':` |
|        - |  1226 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    22777 |  1227 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    22777 |  1228 | `				break;` |
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
|    23881 |  1351 | `			zIn += n;` |
|    23881 |  1352 | `			continue;` |
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
|    36653 |  1470 | `	if( iCons > 1 ){` |
|        - |  1471 | `		/* Concatenate all compiled constants */` |
|     1807 |  1472 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|      901 |  1473 | `	}` |
|        - |  1474 | `	/* Node successfully compiled */` |
|    36653 |  1475 | `	return SXRET_OK;` |
|    18515 |  1476 | `}` |
|        - |  1477 | `/*` |
|        - |  1478 | ` * Compile a double quoted string.` |
|        - |  1479 | ` *  See the block-comment above for more information.` |
|        - |  1480 | ` */` |
|    36958 |  1481 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  1482 | `{` |
|        - |  1483 | `	sxi32 rc;` |
|    36963 |  1484 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|    18479 |  1485 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  1486 | `	/* Compilation result */` |
|    36963 |  1487 | `	return rc;` |
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
|        - |  2220 | `static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok);` |
|        - |  2221 | `static sxi32 GenStateSetVisFlag(sxi32 nKw);` |
|        - |  2222 | `static sxi32 GenStateValidateMemberType(ph7_gen_state *pGen,ph7_class *pClass,const SyString *pMemberName,` |
|        - |  2223 | `	sxu32 nType,const SyString *pTypeClass,const SyString *pTypeText,SySet *pUnionAlts,const char *zErrFmt,sxu32 nLine);` |
|        - |  2224 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|        - |  2225 | `/*` |
|        - |  2226 | ` * Compile an annoynmous function or a closure.` |
|        - |  2227 | ` * According to the PHP language reference` |
|        - |  2228 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|        - |  2229 | ` *  which have no specified name. They are most useful as the value of callback` |
|        - |  2230 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|        - |  2231 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|        - |  2232 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|        - |  2233 | ` *  Example Anonymous function variable assignment example` |
|        - |  2234 | ` * <?php` |
|        - |  2235 | ` * $greet = function($name)` |
|        - |  2236 | ` * {` |
|        - |  2237 | ` *    printf("Hello %s\r\n", $name);` |
|        - |  2238 | ` * };` |
|        - |  2239 | ` * $greet('World');` |
|        - |  2240 | ` * $greet('PHP');` |
|        - |  2241 | ` * ?>` |
|        - |  2242 | ` * Note that the implementation of annoynmous function and closure under` |
|        - |  2243 | ` * PH7 is completely different from the one used by the zend engine.` |
|        - |  2244 | ` */` |
|      448 |  2245 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  2246 | `{` |
|      453 |  2247 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|        - |  2248 | `	char zName[512];         /* Unique lambda name */` |
|        - |  2249 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|        - |  2250 | `							  * one thread is allowed to compile the script.` |
|        - |  2251 | `						      */` |
|        - |  2252 | `	SyString sName;` |
|      453 |  2253 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `$f = #[A] function…` trivia`` |
|        - |  2254 | `	                              * is keyed to this ['static'] 'function' token */` |
|        - |  2255 | `	sxu32 nKwLine;` |
|      453 |  2256 | `	sxi32 iFlags = 0;` |
|        - |  2257 | `	sxu32 nLen;` |
|        - |  2258 | `	sxi32 rc;` |
|      224 |  2259 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  2260 |  |
|      453 |  2261 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|      448 |  2262 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      453 |  2263 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - |  2264 | `		/* Static closure: no $this auto-capture, bind refused */` |
|        9 |  2265 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|        9 |  2266 | `		pGen->pIn++; /* Jump the 'static' keyword */` |
|        4 |  2267 | `	}` |
|      453 |  2268 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|      453 |  2269 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      ! 0 |  2270 | `		pGen->pIn++;` |
|      ! 0 |  2271 | `	}` |
|        - |  2272 | `	/* Generate a unique name */` |
|      453 |  2273 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|        - |  2274 | `	/* Make sure the generated name is unique */` |
|      453 |  2275 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 |  2276 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      ! 0 |  2277 | `	}` |
|      453 |  2278 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|        - |  2279 | `	/* Compile the lambda body */` |
|      453 |  2280 | `	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,&pAnnonFunc);` |
|      453 |  2281 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2282 | `		return SXERR_ABORT;` |
|        - |  2283 | `	}` |
|      453 |  2284 | `	if( pAnnonFunc ){` |
|      453 |  2285 | `		pAnnonFunc->nLine = nKwLine;` |
|        - |  2286 | ``		/* Expression-position attributes (`$f = #[A] function () {}`): the trivia`` |
|        - |  2287 | `		 * sidecar keys them to the closure's first keyword token. */` |
|      453 |  2288 | `		if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnnonFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  2289 | `			return SXERR_ABORT;` |
|        - |  2290 | `		}` |
|      224 |  2291 | `	}` |
|        - |  2292 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|        - |  2293 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|        - |  2294 | `	 * the handler wraps either in a Closure instance. */` |
|      453 |  2295 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|        - |  2296 | `	/* Node successfully compiled */` |
|      453 |  2297 | `	return SXRET_OK;` |
|      229 |  2298 | `}` |
|        - |  2299 | `/*` |
|        - |  2300 | ` * Add a free variable to the arrow function's closure environment, unless` |
|        - |  2301 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|        - |  2302 | ` * enclosing arrow level, or has already been captured.` |
|        - |  2303 | ` */` |
|      196 |  2304 | `static sxi32 GenStateArrowAddCapture(` |
|        - |  2305 | `	ph7_gen_state *pGen,` |
|        - |  2306 | `	ph7_vm_func *pFunc,` |
|        - |  2307 | `	const char *zName,` |
|        - |  2308 | `	sxu32 nByte,` |
|        - |  2309 | `	SyString *aShadow,` |
|        - |  2310 | `	sxu32 nShadow)` |
|        3 |  2311 | `{` |
|        - |  2312 | `	ph7_vm_func_closure_env sEnv;` |
|        - |  2313 | `	ph7_vm_func_closure_env *aEnv;` |
|        - |  2314 | `	sxu32 n, nEnv;` |
|        - |  2315 | `	char *zDup;` |
|      199 |  2316 | `	if( nByte == 0 ){` |
|      ! 0 |  2317 | `		return SXRET_OK;` |
|        - |  2318 | `	}` |
|      196 |  2319 | `	if( nByte == sizeof("this")-1` |
|      107 |  2320 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|        3 |  2321 | `		return SXRET_OK;` |
|        - |  2322 | `	}` |
|      247 |  2323 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|      182 |  2324 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|      176 |  2325 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|      135 |  2326 | `			return SXRET_OK;` |
|        - |  2327 | `		}` |
|       27 |  2328 | `	}` |
|       63 |  2329 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       63 |  2330 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|       91 |  2331 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|       30 |  2332 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|       29 |  2333 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|        3 |  2334 | `			return SXRET_OK;` |
|        - |  2335 | `		}` |
|       15 |  2336 | `	}` |
|       61 |  2337 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|       61 |  2338 | `	if( zDup == 0 ){` |
|      ! 0 |  2339 | `		return SXERR_ABORT;` |
|        - |  2340 | `	}` |
|       61 |  2341 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       61 |  2342 | `	sEnv.iFlags = 0;` |
|       61 |  2343 | `	sEnv.nIdx = SXU32_HIGH;` |
|       61 |  2344 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       61 |  2345 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|       61 |  2346 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       61 |  2347 | `	return SXRET_OK;` |
|      101 |  2348 | `}` |
|        - |  2349 | `/*` |
|        - |  2350 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|        - |  2351 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|        - |  2352 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|        - |  2353 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|        - |  2354 | ` */` |
|       56 |  2355 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|        - |  2356 | `	ph7_gen_state *pGen,` |
|        - |  2357 | `	ph7_vm_func *pFunc,` |
|        - |  2358 | `	const char *zIn,` |
|        - |  2359 | `	const char *zEnd,` |
|        - |  2360 | `	SyString *aShadow,` |
|        - |  2361 | `	sxu32 nShadow)` |
|        2 |  2362 | `{` |
|        - |  2363 | `	sxi32 rc;` |
|      370 |  2364 | `	while( zIn < zEnd ){` |
|      314 |  2365 | `		if( zIn[0] == '\\' ){` |
|        5 |  2366 | `			zIn++;` |
|        5 |  2367 | `			if( zIn < zEnd ){` |
|        5 |  2368 | `				zIn++;` |
|        2 |  2369 | `			}` |
|        5 |  2370 | `			continue;` |
|        - |  2371 | `		}` |
|      308 |  2372 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|       26 |  2373 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|       24 |  2374 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|        - |  2375 | `			const char *zName;` |
|       26 |  2376 | `			zIn++; /* skip '$' */` |
|       26 |  2377 | `			zName = zIn;` |
|       82 |  2378 | `			while( zIn < zEnd ){` |
|       76 |  2379 | `				unsigned char c = (unsigned char)zIn[0];` |
|       76 |  2380 | `				if( c >= 0xc0 ){` |
|      ! 0 |  2381 | `					zIn++;` |
|      ! 0 |  2382 | `					while( zIn < zEnd` |
|      ! 0 |  2383 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  2384 | `						zIn++;` |
|      ! 0 |  2385 | `					}` |
|      ! 0 |  2386 | `					continue;` |
|        - |  2387 | `				}` |
|       76 |  2388 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       20 |  2389 | `					break;` |
|        - |  2390 | `				}` |
|       58 |  2391 | `				zIn++;` |
|        2 |  2392 | `			}` |
|       26 |  2393 | `			if( zIn > zName ){` |
|       38 |  2394 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|       24 |  2395 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|       26 |  2396 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  2397 | `					return SXERR_ABORT;` |
|        - |  2398 | `				}` |
|       12 |  2399 | `			}` |
|       26 |  2400 | `			continue;` |
|        - |  2401 | `		}` |
|      286 |  2402 | `		zIn++;` |
|        2 |  2403 | `	}` |
|       58 |  2404 | `	return SXRET_OK;` |
|       30 |  2405 | `}` |
|        - |  2406 | `/*` |
|        - |  2407 | ` * Scan the body token range of an arrow function for free-variable` |
|        - |  2408 | ` * references and record them in pFunc's closure environment. Handles:` |
|        - |  2409 | ` *   - plain $<id> pairs` |
|        - |  2410 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|        - |  2411 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|        - |  2412 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|        - |  2413 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|        - |  2414 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|        - |  2415 | ` *     are never mistakenly captured.` |
|        - |  2416 | ` */` |
|      296 |  2417 | `static sxi32 GenStateArrowCaptureScan(` |
|        - |  2418 | `	ph7_gen_state *pGen,` |
|        - |  2419 | `	ph7_vm_func *pFunc,` |
|        - |  2420 | `	SyToken *pStart,` |
|        - |  2421 | `	SyToken *pEnd,` |
|        - |  2422 | `	SyString *aShadow,` |
|        - |  2423 | `	sxu32 nShadow)` |
|        4 |  2424 | `{` |
|      300 |  2425 | `	SyToken *pScan = pStart;` |
|        - |  2426 | `	sxi32 rc;` |
|     1708 |  2427 | `	while( pScan < pEnd ){` |
|     1412 |  2428 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|       86 |  2429 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|       28 |  2430 | `				pScan->sData.zString,` |
|       56 |  2431 | `				pScan->sData.zString + pScan->sData.nByte,` |
|       28 |  2432 | `				aShadow,nShadow);` |
|       58 |  2433 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2434 | `				return SXERR_ABORT;` |
|        - |  2435 | `			}` |
|       58 |  2436 | `			pScan++;` |
|       58 |  2437 | `			continue;` |
|        - |  2438 | `		}` |
|     1356 |  2439 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|       30 |  2440 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|       30 |  2441 | `			SyToken *pFnKw = pScan;` |
|       28 |  2442 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|      ! 0 |  2443 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|        2 |  2444 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|      ! 0 |  2445 | `				pFnKw = &pScan[1];` |
|      ! 0 |  2446 | `				nKw = PH7_TKWRD_FN;` |
|      ! 0 |  2447 | `			}` |
|       30 |  2448 | `			if( nKw == PH7_TKWRD_FN ){` |
|        - |  2449 | `				SyToken *pInnerSigStart;` |
|        - |  2450 | `				SyToken *pInnerSigEnd;` |
|        - |  2451 | `				SyToken *pInnerBodyEnd;` |
|        - |  2452 | `				SyString *aInnerShadow;` |
|        - |  2453 | `				sxu32 nInnerShadow;` |
|        - |  2454 | `				sxu32 nInnerParamMax;` |
|        - |  2455 | `				SyToken *p;` |
|        - |  2456 | `				int iNestInner;` |
|       19 |  2457 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|       19 |  2458 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  2459 | `					pScan++;` |
|      ! 0 |  2460 | `				}` |
|       19 |  2461 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 |  2462 | `					pScan++;` |
|      ! 0 |  2463 | `					continue;` |
|        - |  2464 | `				}` |
|       19 |  2465 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|       19 |  2466 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|        - |  2467 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|       19 |  2468 | `				if( pInnerSigEnd >= pEnd ){` |
|      ! 0 |  2469 | `					pScan = pEnd;` |
|      ! 0 |  2470 | `					continue;` |
|        - |  2471 | `				}` |
|        - |  2472 | `				/* Build an augmented shadow list: inherited + inner params */` |
|       19 |  2473 | `				nInnerParamMax = 0;` |
|       57 |  2474 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|       39 |  2475 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|       13 |  2476 | `						nInnerParamMax++;` |
|        6 |  2477 | `					}` |
|       20 |  2478 | `				}` |
|       19 |  2479 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|       18 |  2480 | `					&pGen->pVm->sAllocator,` |
|       18 |  2481 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|       19 |  2482 | `				if( aInnerShadow == 0 ){` |
|      ! 0 |  2483 | `					return SXERR_ABORT;` |
|        - |  2484 | `				}` |
|       19 |  2485 | `				nInnerShadow = 0;` |
|       25 |  2486 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|        7 |  2487 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|        4 |  2488 | `				}` |
|       57 |  2489 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|       39 |  2490 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|       27 |  2491 | `						continue;` |
|        - |  2492 | `					}` |
|       13 |  2493 | `					if( &p[1] >= pInnerSigEnd ){` |
|      ! 0 |  2494 | `						break;` |
|        - |  2495 | `					}` |
|       13 |  2496 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  2497 | `						continue;` |
|        - |  2498 | `					}` |
|       13 |  2499 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|        7 |  2500 | `				}` |
|       19 |  2501 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|       19 |  2502 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|      ! 0 |  2503 | `					pScan++;` |
|      ! 0 |  2504 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|      ! 0 |  2505 | `						&& pScan->sData.nByte == 1` |
|      ! 0 |  2506 | `						&& pScan->sData.zString[0] == '?' ){` |
|      ! 0 |  2507 | `						pScan++;` |
|      ! 0 |  2508 | `					}` |
|      ! 0 |  2509 | `					if( pScan < pEnd` |
|      ! 0 |  2510 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|      ! 0 |  2511 | `						pScan++;` |
|      ! 0 |  2512 | `					}` |
|      ! 0 |  2513 | `				}` |
|       19 |  2514 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|       19 |  2515 | `					pScan++; /* past '=>' */` |
|        9 |  2516 | `				}` |
|       19 |  2517 | `				pInnerBodyEnd = pScan;` |
|       19 |  2518 | `				iNestInner = 0;` |
|      131 |  2519 | `				while( pInnerBodyEnd < pEnd ){` |
|      113 |  2520 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|        - |  2521 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|        - |  2522 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|      ! 0 |  2523 | `						break;` |
|        - |  2524 | `					}` |
|      113 |  2525 | `					if( pInnerBodyEnd->nType &` |
|        - |  2526 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        3 |  2527 | `						iNestInner++;` |
|      112 |  2528 | `					}else if( pInnerBodyEnd->nType &` |
|        - |  2529 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        3 |  2530 | `						iNestInner--;` |
|        1 |  2531 | `					}` |
|      113 |  2532 | `					pInnerBodyEnd++;` |
|        1 |  2533 | `				}` |
|        - |  2534 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|        - |  2535 | `				 * the outer's body: a default value is evaluated at call time` |
|        - |  2536 | `				 * in the outer frame, so any free variable it references is` |
|        - |  2537 | `				 * an outer capture. We must NOT scan the parameter-name` |
|        - |  2538 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|        - |  2539 | `				 * or those names leak into the outer's closure environment.` |
|        - |  2540 | `				 *` |
|        - |  2541 | `				 * Walk the signature argument-by-argument, splitting on` |
|        - |  2542 | `				 * top-level commas, and for each argument scan only the token` |
|        - |  2543 | `				 * range after the '=' sign. */` |
|        - |  2544 | `				{` |
|       19 |  2545 | `					SyToken *pArgStart = pInnerSigStart;` |
|       31 |  2546 | `					while( pArgStart < pInnerSigEnd ){` |
|       13 |  2547 | `						SyToken *pArgEnd = pArgStart;` |
|       13 |  2548 | `						SyToken *pEq = 0;` |
|       13 |  2549 | `						int iNestArg = 0;` |
|       49 |  2550 | `						while( pArgEnd < pInnerSigEnd ){` |
|       38 |  2551 | `							if( iNestArg == 0` |
|       39 |  2552 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|        3 |  2553 | `								break;` |
|        - |  2554 | `							}` |
|       37 |  2555 | `							if( pArgEnd->nType &` |
|        - |  2556 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      ! 0 |  2557 | `								iNestArg++;` |
|       37 |  2558 | `							}else if( pArgEnd->nType &` |
|        - |  2559 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      ! 0 |  2560 | `								iNestArg--;` |
|      ! 0 |  2561 | `							}` |
|       36 |  2562 | `							if( pEq == 0 && iNestArg == 0` |
|       31 |  2563 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|        7 |  2564 | `								pEq = pArgEnd;` |
|        3 |  2565 | `							}` |
|       37 |  2566 | `							pArgEnd++;` |
|        1 |  2567 | `						}` |
|       13 |  2568 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|       10 |  2569 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|        3 |  2570 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|        7 |  2571 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 |  2572 | `								return SXERR_ABORT;` |
|        - |  2573 | `							}` |
|        3 |  2574 | `						}` |
|       13 |  2575 | `						pArgStart = pArgEnd;` |
|       12 |  2576 | `						if( pArgStart < pInnerSigEnd` |
|        8 |  2577 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|        3 |  2578 | `							pArgStart++;` |
|        1 |  2579 | `						}` |
|        1 |  2580 | `					}` |
|        - |  2581 | `				}` |
|       28 |  2582 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|        9 |  2583 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|       19 |  2584 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  2585 | `					return SXERR_ABORT;` |
|        - |  2586 | `				}` |
|       19 |  2587 | `				pScan = pInnerBodyEnd;` |
|       19 |  2588 | `				continue;` |
|        - |  2589 | `			}` |
|        5 |  2590 | `		}` |
|     1338 |  2591 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     1166 |  2592 | `			pScan++;` |
|     1166 |  2593 | `			continue;` |
|        - |  2594 | `		}` |
|        - |  2595 | `		{` |
|        - |  2596 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|      175 |  2597 | `			SyToken *pDollar = pScan;` |
|      258 |  2598 | `			while( &pDollar[1] < pEnd` |
|      175 |  2599 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|      ! 0 |  2600 | `				pDollar++;` |
|      ! 0 |  2601 | `			}` |
|      175 |  2602 | `			if( &pDollar[1] >= pEnd ){` |
|      ! 0 |  2603 | `				break;` |
|        - |  2604 | `			}` |
|      175 |  2605 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  2606 | `				pScan = pDollar + 1;` |
|      ! 0 |  2607 | `				continue;` |
|        - |  2608 | `			}` |
|      261 |  2609 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|      172 |  2610 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|       86 |  2611 | `				aShadow,nShadow);` |
|      175 |  2612 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2613 | `				return SXERR_ABORT;` |
|        - |  2614 | `			}` |
|      175 |  2615 | `			pScan = pDollar + 2;` |
|        - |  2616 | `		}` |
|        3 |  2617 | `	}` |
|      300 |  2618 | `	return SXRET_OK;` |
|      152 |  2619 | `}` |
|        - |  2620 | `/*` |
|        - |  2621 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|        - |  2622 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|        - |  2623 | ` * variables by value. The body is a single expression that acts as an` |
|        - |  2624 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|        - |  2625 | ` * $this is also made available.` |
|        - |  2626 | ` */` |
|      278 |  2627 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  2628 | `{` |
|        - |  2629 | `	ph7_vm_func *pFunc;` |
|        - |  2630 | `	ph7_vm_func_closure_env sEnv;` |
|        - |  2631 | `	GenBlock *pBlock;` |
|        - |  2632 | `	SySet *pInstrContainer;` |
|        - |  2633 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|        - |  2634 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|        - |  2635 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|        - |  2636 | `	SyToken *pSavedEnd;` |
|        - |  2637 | `	ph7_vm_func_arg *aArgs;` |
|        - |  2638 | `	char zName[512];` |
|        - |  2639 | `	static int iCnt = 1;` |
|        - |  2640 | `	char *zDup;` |
|        - |  2641 | `	SyToken *pTokKw;` |
|        - |  2642 | `	sxu32 nLen;` |
|        - |  2643 | `	sxu32 nLine;` |
|      283 |  2644 | `	sxi32 iFlags = 0;` |
|      283 |  2645 | `	int bStatic = 0;` |
|        - |  2646 | `	sxi32 rc;` |
|        - |  2647 | `	sxu32 n;` |
|      139 |  2648 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  2649 |  |
|      283 |  2650 | `	nLine = pGen->pIn->nLine;` |
|        - |  2651 | ``	/* Attribute-sidecar key: `#[A] [static] fn` trivia is keyed to this token */`` |
|      283 |  2652 | `	pTokKw = pGen->pIn;` |
|        - |  2653 | `	/* Optional 'static' prefix */` |
|      278 |  2654 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      283 |  2655 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        7 |  2656 | `		bStatic = 1;` |
|        7 |  2657 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|        7 |  2658 | `		pGen->pIn++;` |
|        3 |  2659 | `	}` |
|        - |  2660 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|      278 |  2661 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      283 |  2662 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|      ! 0 |  2663 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2664 | `			"Arrow function: expected 'fn' keyword");` |
|      ! 0 |  2665 | `		return SXERR_SYNTAX;` |
|        - |  2666 | `	}` |
|      283 |  2667 | `	pGen->pIn++; /* Jump 'fn' */` |
|        - |  2668 | `	/* Optional '&' — return by reference */` |
|      283 |  2669 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  2670 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|      ! 0 |  2671 | `		pGen->pIn++;` |
|      ! 0 |  2672 | `	}` |
|        - |  2673 | `	/* Expect '(' */` |
|      283 |  2674 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        3 |  2675 | `		if( pGen->pIn < pGen->pEnd ){` |
|        4 |  2676 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - |  2677 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|        2 |  2678 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        2 |  2679 | `		}else{` |
|      ! 0 |  2680 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  2681 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|        - |  2682 | `		}` |
|        3 |  2683 | `		return SXERR_SYNTAX;` |
|        - |  2684 | `	}` |
|      281 |  2685 | `	pGen->pIn++; /* Jump '(' */` |
|        - |  2686 | `	/* Delimit the parameter list */` |
|      281 |  2687 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|      281 |  2688 | `	if( pSigEnd >= pGen->pEnd ){` |
|        3 |  2689 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  2690 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|        3 |  2691 | `		return SXERR_SYNTAX;` |
|        - |  2692 | `	}` |
|        - |  2693 | `	/* Allocate the function state */` |
|      279 |  2694 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|      279 |  2695 | `	if( pFunc == 0 ){` |
|      ! 0 |  2696 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2697 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2698 | `		return SXERR_ABORT;` |
|        - |  2699 | `	}` |
|        - |  2700 | `	/* Generate a unique lambda name */` |
|      279 |  2701 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      279 |  2702 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 |  2703 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      ! 0 |  2704 | `	}` |
|      279 |  2705 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|      279 |  2706 | `	if( zDup == 0 ){` |
|      ! 0 |  2707 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2708 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2709 | `		return SXERR_ABORT;` |
|        - |  2710 | `	}` |
|      279 |  2711 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|        - |  2712 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|      279 |  2713 | `	pFunc->nLine = nLine;` |
|        - |  2714 | ``	/* Expression-position attributes (`$f = #[A] fn () => …`) */`` |
|      279 |  2715 | `	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  2716 | `		return SXERR_ABORT;` |
|        - |  2717 | `	}` |
|        - |  2718 | `	/* Collect function arguments */` |
|      279 |  2719 | `	if( pGen->pIn < pSigEnd ){` |
|      110 |  2720 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|      110 |  2721 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  2722 | `			return SXERR_ABORT;` |
|        - |  2723 | `		}` |
|       53 |  2724 | `	}` |
|        - |  2725 | `	/* Point past ')' and parse optional return type */` |
|      279 |  2726 | `	pGen->pIn = &pSigEnd[1];` |
|      279 |  2727 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|      279 |  2728 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2729 | `		return SXERR_ABORT;` |
|      279 |  2730 | `	}else if( rc == SXERR_SYNTAX ){` |
|      ! 0 |  2731 | `		return SXERR_SYNTAX;` |
|        - |  2732 | `	}` |
|        - |  2733 | `	/* Expect '=>' */` |
|      279 |  2734 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|        3 |  2735 | `		if( pGen->pIn < pGen->pEnd ){` |
|        4 |  2736 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - |  2737 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|        2 |  2738 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        2 |  2739 | `		}else{` |
|      ! 0 |  2740 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  2741 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|        - |  2742 | `		}` |
|        3 |  2743 | `		return SXERR_SYNTAX;` |
|        - |  2744 | `	}` |
|      276 |  2745 | `	pGen->pIn++; /* Jump '=>' */` |
|      276 |  2746 | `	pBodyStart = pGen->pIn;` |
|      276 |  2747 | `	pBodyEnd = pGen->pEnd;` |
|        - |  2748 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|        - |  2749 | `	 * recursively collect free-variable references from the body. The scan` |
|        - |  2750 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|        - |  2751 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|      276 |  2752 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|        - |  2753 | `	{` |
|      276 |  2754 | `		SyString *aShadow = 0;` |
|      276 |  2755 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|      276 |  2756 | `		if( nShadow > 0 ){` |
|      107 |  2757 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      104 |  2758 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|      107 |  2759 | `			if( aShadow == 0 ){` |
|      ! 0 |  2760 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2761 | `					"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2762 | `				return SXERR_ABORT;` |
|        - |  2763 | `			}` |
|      239 |  2764 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|      135 |  2765 | `				aShadow[n] = aArgs[n].sName;` |
|       69 |  2766 | `			}` |
|       52 |  2767 | `		}` |
|      412 |  2768 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      136 |  2769 | `			aShadow,nShadow);` |
|      276 |  2770 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  2771 | `			return SXERR_ABORT;` |
|        - |  2772 | `		}` |
|        - |  2773 | `	}` |
|        - |  2774 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|        - |  2775 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|        - |  2776 | `	 * captured value is silently dropped when the enclosing scope has no` |
|        - |  2777 | `	 * $this. */` |
|      276 |  2778 | `	if( !bStatic ){` |
|        - |  2779 | `		char *zThisDup;` |
|      270 |  2780 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|      270 |  2781 | `		if( zThisDup == 0 ){` |
|      ! 0 |  2782 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2783 | `				"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2784 | `			return SXERR_ABORT;` |
|        - |  2785 | `		}` |
|      270 |  2786 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      270 |  2787 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|      270 |  2788 | `		sEnv.nIdx = SXU32_HIGH;` |
|      270 |  2789 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      270 |  2790 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|      270 |  2791 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      133 |  2792 | `	}` |
|        - |  2793 | `	/* Arrow functions are always closures */` |
|      276 |  2794 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|        - |  2795 | `	/* Compile the body expression as an implicit return */` |
|      412 |  2796 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      136 |  2797 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|      276 |  2798 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  2799 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2800 | `			"PH7 engine is running out-of-memory");` |
|      ! 0 |  2801 | `		return SXERR_ABORT;` |
|        - |  2802 | `	}` |
|      276 |  2803 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      276 |  2804 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|      276 |  2805 | `	pSavedEnd = pGen->pEnd;` |
|      276 |  2806 | `	pGen->pIn = pBodyStart;` |
|      276 |  2807 | `	pGen->pEnd = pBodyEnd;` |
|      276 |  2808 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      276 |  2809 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2810 | `		return SXERR_ABORT;` |
|        - |  2811 | `	}` |
|        - |  2812 | `	/* The cursor stopped just past the body expression */` |
|      276 |  2813 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|        - |  2814 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|        - |  2815 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|        - |  2816 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|        - |  2817 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|      276 |  2818 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      276 |  2819 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      276 |  2820 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      276 |  2821 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      276 |  2822 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - |  2823 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|      276 |  2824 | `	pGen->pIn = pBodyEnd;` |
|      276 |  2825 | `	pGen->pEnd = pSavedEnd;` |
|        - |  2826 | `	/* Emit the load-closure instruction */` |
|      276 |  2827 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|      276 |  2828 | `	return SXRET_OK;` |
|      144 |  2829 | `}` |
|        - |  2830 | `/*` |
|        - |  2831 | ` * Compile a single arm's expression range into a freshly-allocated` |
|        - |  2832 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|        - |  2833 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|        - |  2834 | ` * expression's value.` |
|        - |  2835 | ` */` |
|      354 |  2836 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|        - |  2837 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|        3 |  2838 | `{` |
|        - |  2839 | `	SySet *pInstrContainer;` |
|        - |  2840 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |  2841 | `	GenBlock *pArmBlock;` |
|        - |  2842 | `	sxi32 rc;` |
|      357 |  2843 | `	pTmpIn  = pGen->pIn;` |
|      357 |  2844 | `	pTmpEnd = pGen->pEnd;` |
|      357 |  2845 | `	pGen->pIn  = pStart;` |
|      357 |  2846 | `	pGen->pEnd = pStop;` |
|      357 |  2847 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      357 |  2848 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|        - |  2849 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|        - |  2850 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|        - |  2851 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|        - |  2852 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|        - |  2853 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|      534 |  2854 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      177 |  2855 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|      357 |  2856 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  2857 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  2858 | `		pGen->pIn  = pTmpIn;` |
|      ! 0 |  2859 | `		pGen->pEnd = pTmpEnd;` |
|      ! 0 |  2860 | `		return SXERR_ABORT;` |
|        - |  2861 | `	}` |
|      357 |  2862 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      357 |  2863 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      357 |  2864 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      357 |  2865 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      357 |  2866 | `	GenStateLeaveBlock(&(*pGen),0);` |
|      357 |  2867 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      357 |  2868 | `	pGen->pIn  = pTmpIn;` |
|      357 |  2869 | `	pGen->pEnd = pTmpEnd;` |
|      357 |  2870 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2871 | `		return SXERR_ABORT;` |
|        - |  2872 | `	}` |
|      357 |  2873 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 |  2874 | `		return SXERR_EMPTY;` |
|        - |  2875 | `	}` |
|      357 |  2876 | `	return SXRET_OK;` |
|      180 |  2877 | `}` |
|        - |  2878 | `/*` |
|        - |  2879 | ` * Compile a PHP 8.0 match expression:` |
|        - |  2880 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|        - |  2881 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|        - |  2882 | ` * Strict comparison (===) is used between the subject and each condition.` |
|        - |  2883 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|        - |  2884 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|        - |  2885 | ` */` |
|        - |  2886 | `/*` |
|        - |  2887 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|        - |  2888 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|        - |  2889 | ` * caller can bail out of the current expression.` |
|        - |  2890 | ` */` |
|        2 |  2891 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|        1 |  2892 | `{` |
|        - |  2893 | `	va_list ap;` |
|        - |  2894 | `	sxi32 rc;` |
|        - |  2895 | `	SyBlob sMsg;` |
|        3 |  2896 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|        3 |  2897 | `	va_start(ap,zFmt);` |
|        3 |  2898 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|        3 |  2899 | `	va_end(ap);` |
|        3 |  2900 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|        3 |  2901 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|        3 |  2902 | `	SyBlobRelease(&sMsg);` |
|        3 |  2903 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2904 | `		return SXERR_ABORT;` |
|        - |  2905 | `	}` |
|        3 |  2906 | `	return SXERR_SYNTAX;` |
|        2 |  2907 | `}` |
|        - |  2908 | `/*` |
|        - |  2909 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|        - |  2910 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|        - |  2911 | ` * Returns the stop token pointer (or pEnd if none found).` |
|        - |  2912 | ` */` |
|      356 |  2913 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|        4 |  2914 | `{` |
|      360 |  2915 | `	SyToken *pCur = pStart;` |
|      360 |  2916 | `	int iNest = 0;` |
|      838 |  2917 | `	while( pCur < pEnd ){` |
|      802 |  2918 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       13 |  2919 | `			iNest++;` |
|      796 |  2920 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       13 |  2921 | `			iNest--;` |
|      784 |  2922 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|      323 |  2923 | `			return pCur;` |
|        - |  2924 | `		}` |
|      482 |  2925 | `		pCur++;` |
|        4 |  2926 | `	}` |
|       39 |  2927 | `	return pEnd;` |
|      182 |  2928 | `}` |
|       72 |  2929 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  2930 | `{` |
|        - |  2931 | `	ph7_match *pMatch;` |
|        - |  2932 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|       77 |  2933 | `	int bHasDefault = 0;` |
|        - |  2934 | `	sxu32 nLine;` |
|        - |  2935 | `	sxi32 rc;` |
|       36 |  2936 | `	SXUNUSED(iCompileFlag);` |
|       77 |  2937 | `	nLine = pGen->pIn->nLine;` |
|       77 |  2938 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|        - |  2939 | `	/* Expect '(' */` |
|       77 |  2940 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 |  2941 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  2942 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|      ! 0 |  2943 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|        - |  2944 | `	}` |
|       77 |  2945 | `	pGen->pIn++; /* Jump '(' */` |
|       77 |  2946 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|       77 |  2947 | `	if( pSubjEnd >= pGen->pEnd ){` |
|      ! 0 |  2948 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  2949 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|        - |  2950 | `	}` |
|       77 |  2951 | `	if( pGen->pIn >= pSubjEnd ){` |
|      ! 0 |  2952 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  2953 | `			"syntax error, unexpected \")\", expecting match subject");` |
|        - |  2954 | `	}` |
|        - |  2955 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|       77 |  2956 | `	pSavedEnd = pGen->pEnd;` |
|       77 |  2957 | `	pGen->pEnd = pSubjEnd;` |
|       77 |  2958 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       77 |  2959 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2960 | `		return SXERR_ABORT;` |
|        - |  2961 | `	}` |
|       77 |  2962 | `	pGen->pEnd = pSavedEnd;` |
|       77 |  2963 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|        - |  2964 | `	/* Expect '{' */` |
|       77 |  2965 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|      ! 0 |  2966 | `		return GenStateMatchError(pGen,` |
|      ! 0 |  2967 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|        - |  2968 | `			"syntax error, expecting \"{\" after match subject");` |
|        - |  2969 | `	}` |
|       77 |  2970 | `	pGen->pIn++; /* Jump '{' */` |
|       77 |  2971 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|       77 |  2972 | `	if( pBodyEnd >= pGen->pEnd ){` |
|      ! 0 |  2973 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  2974 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|        - |  2975 | `	}` |
|        - |  2976 | `	/* Allocate ph7_match container */` |
|       77 |  2977 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|       77 |  2978 | `	if( pMatch == 0 ){` |
|      ! 0 |  2979 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  2980 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  2981 | `		return SXERR_ABORT;` |
|        - |  2982 | `	}` |
|       77 |  2983 | `	SyZero(pMatch,sizeof(ph7_match));` |
|       77 |  2984 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|        - |  2985 | `	/* Iterate arms */` |
|      259 |  2986 | `	while( pGen->pIn < pBodyEnd ){` |
|        - |  2987 | `		ph7_match_arm sArm;` |
|        - |  2988 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|      190 |  2989 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|      190 |  2990 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|      190 |  2991 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|      190 |  2992 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - |  2993 | `		/* 'default' arm? */` |
|      186 |  2994 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      107 |  2995 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|       22 |  2996 | `			if( bHasDefault ){` |
|        3 |  2997 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|        - |  2998 | `					"Match expressions may only contain one default arm");` |
|        4 |  2999 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  3000 | `			}` |
|       20 |  3001 | `			sArm.bDefault = 1;` |
|       20 |  3002 | `			bHasDefault = 1;` |
|       20 |  3003 | `			pGen->pIn++;` |
|       20 |  3004 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|      ! 0 |  3005 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  3006 | `					"syntax error, expecting \"=>\" after 'default'");` |
|        - |  3007 | `			}` |
|       20 |  3008 | `			pGen->pIn++; /* Jump '=>' */` |
|       11 |  3009 | `		}else{` |
|        - |  3010 | `			/* Condition list: cond (',' cond)* '=>' */` |
|      170 |  3011 | `			pCondStart = pGen->pIn;` |
|      170 |  3012 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|        - |  3013 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|      178 |  3014 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|        - |  3015 | `				SySet sCondBc;` |
|        9 |  3016 | `				if( pCondStart >= pArrow ){` |
|      ! 0 |  3017 | `					return GenStateMatchError(pGen,nArmLine,` |
|        - |  3018 | `						"syntax error, empty match condition expression");` |
|        - |  3019 | `				}` |
|        9 |  3020 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        9 |  3021 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|        9 |  3022 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  3023 | `					return SXERR_ABORT;` |
|        - |  3024 | `				}` |
|        9 |  3025 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|        9 |  3026 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|        9 |  3027 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|        - |  3028 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|        1 |  3029 | `			}` |
|      170 |  3030 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|        3 |  3031 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  3032 | `					"syntax error, expecting \"=>\" in match arm");` |
|        - |  3033 | `			}` |
|      167 |  3034 | `			if( pCondStart >= pArrow ){` |
|      ! 0 |  3035 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  3036 | `					"syntax error, empty match condition expression");` |
|        - |  3037 | `			}` |
|        - |  3038 | `			{` |
|        - |  3039 | `				SySet sCondBc;` |
|      167 |  3040 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      167 |  3041 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|      167 |  3042 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  3043 | `					return SXERR_ABORT;` |
|        - |  3044 | `				}` |
|      167 |  3045 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|        - |  3046 | `			}` |
|      167 |  3047 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|        - |  3048 | `		}` |
|        - |  3049 | `		/* Compile result expression: up to top-level ',' or body end */` |
|      185 |  3050 | `		pResStart = pGen->pIn;` |
|      185 |  3051 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|      185 |  3052 | `		if( pResStart >= pResEnd ){` |
|      ! 0 |  3053 | `			return GenStateMatchError(pGen,nArmLine,` |
|        - |  3054 | `				"syntax error, expected expression after \"=>\"");` |
|        - |  3055 | `		}` |
|      185 |  3056 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|      185 |  3057 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3058 | `			return SXERR_ABORT;` |
|        - |  3059 | `		}` |
|      185 |  3060 | `		pGen->pIn = pResEnd;` |
|      185 |  3061 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      151 |  3062 | `			pGen->pIn++; /* Skip trailing ',' */` |
|       74 |  3063 | `		}` |
|      185 |  3064 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|        3 |  3065 | `	}` |
|       71 |  3066 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|       71 |  3067 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|       71 |  3068 | `	return SXRET_OK;` |
|       41 |  3069 | `}` |
|        - |  3070 | `/*` |
|        - |  3071 | ` * Compile a backtick quoted string.` |
|        - |  3072 | ` */` |
|        4 |  3073 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        2 |  3074 | `{` |
|        - |  3075 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|        - |  3076 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|        - |  3077 | `	 */` |
|        8 |  3078 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|        - |  3079 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|        2 |  3080 | `		ph7_lib_version()` |
|        - |  3081 | `		);` |
|        - |  3082 | `	/* Load NULL */` |
|        6 |  3083 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        2 |  3084 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  3085 | `	/* Node successfully compiled */` |
|        6 |  3086 | `	return SXRET_OK;` |
|        2 |  3087 | `}` |
|        - |  3088 | `/*` |
|        - |  3089 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|        - |  3090 | ` * construct.` |
|        - |  3091 | ` */` |
|       82 |  3092 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  3093 | `{` |
|        - |  3094 | `	SyString *pName;` |
|        - |  3095 | `	sxu32 nKeyID;` |
|        - |  3096 | `	sxi32 rc;` |
|        - |  3097 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|       87 |  3098 | `	pName = &pGen->pIn->sData;` |
|       87 |  3099 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       87 |  3100 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|       87 |  3101 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|        9 |  3102 | `		SyToken *pTmp,*pNext = 0;` |
|        - |  3103 | `		/* Compile arguments one after one */` |
|        9 |  3104 | `		pTmp = pGen->pEnd;` |
|        - |  3105 | `		/* Symisc eXtension to the PHP programming language:` |
|        - |  3106 | `		 * 'echo' can be used in the context of a function which` |
|        - |  3107 | `		 *  mean that the following expression is valid:` |
|        - |  3108 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|        - |  3109 | `		 */` |
|        9 |  3110 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|       17 |  3111 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|        9 |  3112 | `			if( pGen->pIn < pNext ){` |
|        9 |  3113 | `				pGen->pEnd = pNext;` |
|        9 |  3114 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|        9 |  3115 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  3116 | `					return SXERR_ABORT;` |
|        - |  3117 | `				}` |
|        9 |  3118 | `				if( rc != SXERR_EMPTY ){` |
|        - |  3119 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|        - |  3120 | `					 * without the overhead of a function call.` |
|        - |  3121 | `					 * This is a very powerful optimization that improve` |
|        - |  3122 | `					 * performance greatly.` |
|        - |  3123 | `					 */` |
|        9 |  3124 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|        4 |  3125 | `				}` |
|        4 |  3126 | `			}` |
|        - |  3127 | `			/* Jump trailing commas */` |
|        9 |  3128 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|      ! 0 |  3129 | `				pNext++;` |
|      ! 0 |  3130 | `			}` |
|        9 |  3131 | `			pGen->pIn = pNext;` |
|        1 |  3132 | `		}` |
|        - |  3133 | `		/* Restore token stream */` |
|        9 |  3134 | `		pGen->pEnd = pTmp;` |
|        5 |  3135 | `	}else{` |
|       79 |  3136 | `		sxi32 nArg = 0;` |
|       79 |  3137 | `		sxu32 nIdx = 0;` |
|       79 |  3138 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|       79 |  3139 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3140 | `			return SXERR_ABORT;` |
|       79 |  3141 | `		}else if(rc != SXERR_EMPTY ){` |
|       79 |  3142 | `			nArg = 1;` |
|       37 |  3143 | `		}` |
|       79 |  3144 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|        - |  3145 | `			ph7_value *pObj;` |
|        - |  3146 | `			/* Emit the call instruction */` |
|       31 |  3147 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       31 |  3148 | `			if( pObj == 0 ){` |
|      ! 0 |  3149 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3150 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  3151 | `				return SXERR_ABORT;` |
|        - |  3152 | `			}` |
|       31 |  3153 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|        - |  3154 | `			/* Install in the literal table */` |
|       31 |  3155 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       13 |  3156 | `		}` |
|        - |  3157 | `		/* Emit the call instruction */` |
|       79 |  3158 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       79 |  3159 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        - |  3160 | `	}` |
|        - |  3161 | `	/* Node successfully compiled */` |
|       87 |  3162 | `	return SXRET_OK;` |
|       46 |  3163 | `}` |
|        - |  3164 | `/*` |
|        - |  3165 | ` * Compile a node holding a variable declaration.` |
|        - |  3166 | ` * According to the PHP language reference` |
|        - |  3167 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|        - |  3168 | ` *  The variable name is case-sensitive.` |
|        - |  3169 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|        - |  3170 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|        - |  3171 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|        - |  3172 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|        - |  3173 | ` *  Note: $this is a special variable that can't be assigned.` |
|        - |  3174 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|        - |  3175 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|        - |  3176 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|        - |  3177 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|        - |  3178 | ` *  the chapter on Expressions.` |
|        - |  3179 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|        - |  3180 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|        - |  3181 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|        - |  3182 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|        - |  3183 | ` *  is being assigned (the source variable).` |
|        - |  3184 | ` */` |
|  8846228 |  3185 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  3186 | `{` |
|  8846233 |  3187 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |  3188 | `	sxi32 iVv;` |
|        - |  3189 | `	sxi32 iP1;` |
|        - |  3190 | `	void *p3;` |
|        - |  3191 | `	sxi32 rc;` |
|  8846233 |  3192 | `	iVv = -1; /* Variable variable counter */` |
| 17692473 |  3193 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  8846245 |  3194 | `		pGen->pIn++;` |
|  8846245 |  3195 | `		iVv++;` |
|        5 |  3196 | `	}` |
|  8846233 |  3197 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|        - |  3198 | `		/* Invalid variable name */` |
|      ! 0 |  3199 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|      ! 0 |  3200 | `		if( rc == SXERR_ABORT ){` |
|        - |  3201 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3202 | `			return SXERR_ABORT;` |
|        - |  3203 | `		}` |
|      ! 0 |  3204 | `		return SXRET_OK;` |
|        - |  3205 | `	}` |
|  8846233 |  3206 | `	p3  = 0;` |
|  8846233 |  3207 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|        - |  3208 | `		/* Dynamic variable creation */` |
|       21 |  3209 | `		pGen->pIn++;  /* Jump the open curly */` |
|       21 |  3210 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|       21 |  3211 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - |  3212 | `			/* Empty expression */` |
|        3 |  3213 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|        3 |  3214 | `			return SXRET_OK;` |
|        - |  3215 | `		}` |
|        - |  3216 | `		/* Compile the expression holding the variable name */` |
|       18 |  3217 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       18 |  3218 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3219 | `			return SXERR_ABORT;` |
|       18 |  3220 | `		}else if( rc == SXERR_EMPTY ){` |
|        3 |  3221 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|        3 |  3222 | `			return SXRET_OK;` |
|        - |  3223 | `		}` |
|        8 |  3224 | `	}else{` |
|        - |  3225 | `		SyHashEntry *pEntry;` |
|        - |  3226 | `		SyString *pName;` |
|  8846215 |  3227 | `		char *zName = 0;` |
|        - |  3228 | `		/* Extract variable name */` |
|  8846215 |  3229 | `		pName = &pGen->pIn->sData;` |
|        - |  3230 | `		/* Advance the stream cursor */` |
|  8846215 |  3231 | `		pGen->pIn++;` |
|  8846215 |  3232 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  8846215 |  3233 | `		if( pEntry == 0 ){` |
|        - |  3234 | `			/* Duplicate name */` |
|   562861 |  3235 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   562861 |  3236 | `			if( zName == 0 ){` |
|      ! 0 |  3237 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3238 | `				return SXERR_ABORT;` |
|        - |  3239 | `			}` |
|        - |  3240 | `			/* Install in the hashtable */` |
|   562861 |  3241 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   281433 |  3242 | `		}else{` |
|        - |  3243 | `			/* Name already available */` |
|  8283359 |  3244 | `			zName = (char *)pEntry->pUserData;` |
|        - |  3245 | `		}` |
|  8846215 |  3246 | `		p3 = (void *)zName;` |
|        - |  3247 | `	}` |
|  8846229 |  3248 | `	iP1 = 0;` |
|  8846229 |  3249 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  2675423 |  3250 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|        - |  3251 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  2675405 |  3252 | `			iP1 = 1;` |
|  1337700 |  3253 | `		}` |
|  1337709 |  3254 | `	}` |
|        - |  3255 | `	/* Emit the load instruction */` |
|  8846229 |  3256 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  8846241 |  3257 | `	while( iVv > 0 ){` |
|       13 |  3258 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|       13 |  3259 | `		iVv--;` |
|        1 |  3260 | `	}` |
|        - |  3261 | `	/* Node successfully compiled */` |
|  8846229 |  3262 | `	return SXRET_OK;` |
|  4423119 |  3263 | `}` |
|        - |  3264 | `/*` |
|        - |  3265 | ` * Load a literal.` |
|        - |  3266 | ` */` |
|  5620300 |  3267 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|        5 |  3268 | `{` |
|  5620305 |  3269 | `	SyToken *pToken = pGen->pIn;` |
|        - |  3270 | `	ph7_value *pObj;` |
|        - |  3271 | `	SyString *pStr;` |
|        - |  3272 | `	sxu32 nIdx;` |
|        - |  3273 | `	/* Extract token value */` |
|  5620305 |  3274 | `	pStr = &pToken->sData;` |
|        - |  3275 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  5620305 |  3276 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  1362997 |  3277 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|        - |  3278 | `			/* NULL constant are always indexed at 0 */` |
|   560195 |  3279 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   560195 |  3280 | `			return SXRET_OK;` |
|   802807 |  3281 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|        - |  3282 | `			/* TRUE constant are always indexed at 1 */` |
|   148577 |  3283 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|   148577 |  3284 | `			return SXRET_OK;` |
|        5 |  3285 | `		}` |
|  5065987 |  3286 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   963118 |  3287 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|        - |  3288 | `			/* FALSE constant are always indexed at 2 */` |
|   408463 |  3289 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   408463 |  3290 | `			return SXRET_OK;` |
|  4135131 |  3291 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   572552 |  3292 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|        - |  3293 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    11663 |  3294 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    11663 |  3295 | `			if( pObj == 0 ){` |
|      ! 0 |  3296 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3297 | `				return SXERR_ABORT;` |
|        - |  3298 | `			}` |
|    11663 |  3299 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|        - |  3300 | `			/* Emit the load constant instruction */` |
|    11663 |  3301 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    11663 |  3302 | `			return SXRET_OK;` |
|  3866625 |  3303 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|    58856 |  3304 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|        - |  3305 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|        8 |  3306 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        8 |  3307 | `			if( pObj == 0 ){` |
|      ! 0 |  3308 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3309 | `				return SXERR_ABORT;` |
|        - |  3310 | `			}` |
|        8 |  3311 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        - |  3312 | `				SyString sNs;` |
|        8 |  3313 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        8 |  3314 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|        5 |  3315 | `			}else{` |
|      ! 0 |  3316 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|        - |  3317 | `			}` |
|        8 |  3318 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        8 |  3319 | `			return SXRET_OK;` |
|  3859076 |  3320 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   152006 |  3321 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  3945400 |  3322 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   216442 |  3323 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|       11 |  3324 | `			GenBlock *pBlock = pGen->pCurrent;` |
|        - |  3325 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|       21 |  3326 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|        - |  3327 | `				/* Point to the upper block */` |
|       11 |  3328 | `				pBlock = pBlock->pParent;` |
|        1 |  3329 | `			}` |
|       11 |  3330 | `			if( pBlock == 0 ){` |
|        - |  3331 | `				/* Called in the global scope,load NULL */` |
|        5 |  3332 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        3 |  3333 | `			}else{` |
|        - |  3334 | `				/* Extract the target function/method */` |
|        7 |  3335 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        7 |  3336 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|        - |  3337 | `					/* Not a class method,Load null */` |
|        3 |  3338 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        2 |  3339 | `				}else{` |
|        5 |  3340 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        5 |  3341 | `					if( pObj == 0 ){` |
|      ! 0 |  3342 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3343 | `						return SXERR_ABORT;` |
|        - |  3344 | `					}` |
|        5 |  3345 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|        - |  3346 | `					/* Emit the load constant instruction */` |
|        5 |  3347 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - |  3348 | `				}` |
|        - |  3349 | `			}` |
|       11 |  3350 | `			return SXRET_OK;` |
|        - |  3351 | `	}` |
|        - |  3352 | `	/* Query literal table */` |
|  4491411 |  3353 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|        - |  3354 | `		ph7_value *pLitObj;` |
|        - |  3355 | `		/* Unknown literal,install it in the literal table */` |
|   908231 |  3356 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   908231 |  3357 | `		if( pLitObj == 0 ){` |
|      ! 0 |  3358 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  3359 | `			return SXERR_ABORT;` |
|        - |  3360 | `		}` |
|   908231 |  3361 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|   908231 |  3362 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|   454113 |  3363 | `	}` |
|        - |  3364 | `	/* Emit the load constant instruction */` |
|  4491411 |  3365 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  4491411 |  3366 | `	return SXRET_OK;` |
|  2810155 |  3367 | `}` |
|        - |  3368 | `/*` |
|        - |  3369 | ` * Resolve a namespace path or simply load a literal.` |
|        - |  3370 | ` * If the token stream contains namespace separators (backslashes),` |
|        - |  3371 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|        - |  3372 | ` * Otherwise, load the simple literal directly.` |
|        - |  3373 | ` */` |
|  5624232 |  3374 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|        5 |  3375 | `{` |
|        - |  3376 | `	sxi32 rc;` |
|  5624237 |  3377 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  3378 | `		return SXRET_OK;` |
|        - |  3379 | `	}` |
|        - |  3380 | `	/* Check if this is a multi-token namespace path */` |
|  5624237 |  3381 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|        - |  3382 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|     3937 |  3383 | `		SyBlob *pWorker = &pGen->sWorker;` |
|     3937 |  3384 | `		int isAbsolute = 0;` |
|     3937 |  3385 | `		SyBlobReset(pWorker);` |
|        - |  3386 | `		/* Check for leading backslash (absolute path) */` |
|     3937 |  3387 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|     3935 |  3388 | `			isAbsolute = 1;` |
|     3935 |  3389 | `			pGen->pIn++; /* Skip leading backslash */` |
|     1965 |  3390 | `		}` |
|        - |  3391 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|     3937 |  3392 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        3 |  3393 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        3 |  3394 | `			SyBlobAppend(pWorker,"\\",1);` |
|        1 |  3395 | `		}` |
|        - |  3396 | `		/* Collect all path components */` |
|     4045 |  3397 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     4045 |  3398 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       58 |  3399 | `				SyBlobAppend(pWorker,"\\",1);` |
|       31 |  3400 | `			}else{` |
|     3991 |  3401 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - |  3402 | `			}` |
|     4045 |  3403 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|     3937 |  3404 | `				pGen->pIn++;` |
|     3937 |  3405 | `				break;` |
|        - |  3406 | `			}` |
|      112 |  3407 | `			pGen->pIn++;` |
|        4 |  3408 | `		}` |
|     3937 |  3409 | `		if( SyBlobLength(pWorker) > 0 ){` |
|        - |  3410 | `			ph7_value *pObj;` |
|        - |  3411 | `			SyString sPath;` |
|        - |  3412 | `			sxu32 nIdx;` |
|     3937 |  3413 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|        - |  3414 | `			/* Install in the literal table */` |
|     3937 |  3415 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|     3909 |  3416 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     3909 |  3417 | `				if( pObj == 0 ){` |
|      ! 0 |  3418 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  3419 | `					return SXERR_ABORT;` |
|        - |  3420 | `				}` |
|     3909 |  3421 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|     3909 |  3422 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     1952 |  3423 | `			}` |
|        - |  3424 | `			/* Emit the load constant instruction.` |
|        - |  3425 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|        - |  3426 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|     5903 |  3427 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|     1966 |  3428 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|     1966 |  3429 | `				nIdx,0,0);` |
|     3937 |  3430 | `			return SXRET_OK;` |
|        - |  3431 | `		}` |
|      ! 0 |  3432 | `	}` |
|        - |  3433 | `	/* Single-token literal: load directly */` |
|  5620305 |  3434 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  5620305 |  3435 | `	return rc;` |
|  2812121 |  3436 | `}` |
|        - |  3437 | `/*` |
|        - |  3438 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|        - |  3439 | ` */` |
|        - |  3440 | `/*` |
|        - |  3441 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|        - |  3442 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|        - |  3443 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|        - |  3444 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|        - |  3445 | ` */` |
|      ! 0 |  3446 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|      ! 0 |  3447 | `{` |
|      ! 0 |  3448 | `	SXUNUSED(iCompileFlag);` |
|      ! 0 |  3449 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|        - |  3450 | `		"Cannot use the first-class callable syntax '...' here");` |
|      ! 0 |  3451 | `	return SXERR_SYNTAX;` |
|      ! 0 |  3452 | `}` |
|  5624232 |  3453 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  3454 | `{` |
|        - |  3455 | `	sxi32 rc;` |
|  5624237 |  3456 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  5624237 |  3457 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  3458 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  3459 | `		return rc;` |
|        - |  3460 | `	}` |
|        - |  3461 | `	/* Node successfully compiled */` |
|  5624237 |  3462 | `	return SXRET_OK;` |
|  2812121 |  3463 | `}` |
|        - |  3464 | `/*` |
|        - |  3465 | ` * Recover from a compile-time error. In other words synchronize` |
|        - |  3466 | ` * the token stream cursor with the first semi-colon seen.` |
|        - |  3467 | ` */` |
|        8 |  3468 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|        1 |  3469 | `{` |
|        - |  3470 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|       17 |  3471 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|        9 |  3472 | `		pGen->pIn++;` |
|        1 |  3473 | `	}` |
|        9 |  3474 | `	return SXRET_OK;` |
|        1 |  3475 | `}` |
|        - |  3476 | `/*` |
|        - |  3477 | ` * Check if the given identifier name is reserved or not.` |
|        - |  3478 | ` * Return TRUE if reserved.FALSE otherwise.` |
|        - |  3479 | ` */` |
|   143928 |  3480 | `static int GenStateIsReservedConstant(SyString *pName)` |
|        5 |  3481 | `{` |
|   143933 |  3482 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|       48 |  3483 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|        3 |  3484 | `			return TRUE;` |
|       46 |  3485 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|        6 |  3486 | `			return TRUE;` |
|        3 |  3487 | `		}` |
|   143908 |  3488 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       22 |  3489 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|        3 |  3490 | `			return TRUE;` |
|        - |  3491 | `		}` |
|        9 |  3492 | `	}` |
|        - |  3493 | `	/* Not a reserved constant */` |
|   143925 |  3494 | `	return FALSE;` |
|    71969 |  3495 | `}` |
|        - |  3496 | `/*` |
|        - |  3497 | ` * Compile the 'const' statement.` |
|        - |  3498 | ` * According to the PHP language reference` |
|        - |  3499 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|        - |  3500 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|        - |  3501 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|        - |  3502 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|        - |  3503 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|        - |  3504 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|        - |  3505 | ` *  Syntax` |
|        - |  3506 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|        - |  3507 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|        - |  3508 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|        - |  3509 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|        - |  3510 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|        - |  3511 | ` *  to get a list of all defined constants.` |
|        - |  3512 | ` *` |
|        - |  3513 | ` * Symisc eXtension.` |
|        - |  3514 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|        - |  3515 | ` *  would allow only simple scalar value.` |
|        - |  3516 | ` *  Example` |
|        - |  3517 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|        - |  3518 | ` *    Refer to the official documentation for more information on this feature.` |
|        - |  3519 | ` */` |
|       48 |  3520 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|        5 |  3521 | `{` |
|        - |  3522 | `	SySet *pConsCode,*pInstrContainer;` |
|       53 |  3523 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |  3524 | `	SyString *pName;` |
|        - |  3525 | `	sxi32 rc;` |
|       53 |  3526 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       53 |  3527 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  3528 | `		/* Invalid constant name */` |
|        8 |  3529 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|        8 |  3530 | `		if( rc == SXERR_ABORT ){` |
|        - |  3531 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3532 | `			return SXERR_ABORT;` |
|        - |  3533 | `		}` |
|        8 |  3534 | `		goto Synchronize;` |
|        - |  3535 | `	}` |
|        - |  3536 | `	/* Peek constant name */` |
|       47 |  3537 | `	pName = &pGen->pIn->sData;` |
|        - |  3538 | `	/* Make sure the constant name isn't reserved */` |
|       47 |  3539 | `	if( GenStateIsReservedConstant(pName) ){` |
|        - |  3540 | `		/* Reserved constant */` |
|       10 |  3541 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       10 |  3542 | `		if( rc == SXERR_ABORT ){` |
|        - |  3543 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3544 | `			return SXERR_ABORT;` |
|        - |  3545 | `		}` |
|       10 |  3546 | `		goto Synchronize;` |
|        - |  3547 | `	}` |
|       38 |  3548 | `	pGen->pIn++;` |
|       38 |  3549 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|        - |  3550 | `		/* Invalid statement*/` |
|        6 |  3551 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|        6 |  3552 | `		if( rc == SXERR_ABORT ){` |
|        - |  3553 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3554 | `			return SXERR_ABORT;` |
|        - |  3555 | `		}` |
|        6 |  3556 | `		goto Synchronize;` |
|        - |  3557 | `	}` |
|       32 |  3558 | `	pGen->pIn++; /*Jump the equal sign */` |
|        - |  3559 | `	/* Allocate a new constant value container */` |
|       32 |  3560 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|       32 |  3561 | `	if( pConsCode == 0 ){` |
|      ! 0 |  3562 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3563 | `		return SXERR_ABORT;` |
|        - |  3564 | `	}` |
|       32 |  3565 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - |  3566 | `	/* Swap bytecode container */` |
|       32 |  3567 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       32 |  3568 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|        - |  3569 | `	/* Compile constant value */` |
|       32 |  3570 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  3571 | `	/* Emit the done instruction */` |
|       32 |  3572 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       32 |  3573 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       32 |  3574 | `	if( rc == SXERR_ABORT ){` |
|        - |  3575 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  3576 | `		return SXERR_ABORT;` |
|        - |  3577 | `	}` |
|       32 |  3578 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|        - |  3579 | `	/* Register the constant with namespace-qualified name */` |
|        - |  3580 | `	{` |
|        - |  3581 | `		SyBlob sFQN;` |
|        - |  3582 | `		SyString sFQNStr;` |
|       32 |  3583 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       32 |  3584 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|       32 |  3585 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       47 |  3586 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|       30 |  3587 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|       32 |  3588 | `		if( rc == SXRET_OK && SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|        - |  3589 | ``			/* php 8.5: attributes on `const` statements — attach the pending`` |
|        - |  3590 | `			 * groups to the registered constant record for Reflection. */` |
|        7 |  3591 | `			SyHashEntry *pCEntry = SyHashGet(&pGen->pVm->hConstant,` |
|        4 |  3592 | `				SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        5 |  3593 | `			if( pCEntry ){` |
|        5 |  3594 | `				ph7_constant *pRegCons = (ph7_constant *)pCEntry->pUserData;` |
|        5 |  3595 | `				if( GenStateConsumeAttrs(&(*pGen),&pRegCons->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  3596 | `					SyBlobRelease(&sFQN);` |
|      ! 0 |  3597 | `					return SXERR_ABORT;` |
|        - |  3598 | `				}` |
|        2 |  3599 | `			}` |
|        2 |  3600 | `		}` |
|       32 |  3601 | `		SyBlobRelease(&sFQN);` |
|        - |  3602 | `	}` |
|       32 |  3603 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  3604 | `		SySetRelease(pConsCode);` |
|      ! 0 |  3605 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|      ! 0 |  3606 | `	}` |
|       32 |  3607 | `	return SXRET_OK;` |
|        9 |  3608 | `Synchronize:` |
|        - |  3609 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|       60 |  3610 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       42 |  3611 | `		pGen->pIn++;` |
|        4 |  3612 | `	}` |
|       22 |  3613 | `	return SXRET_OK;` |
|       29 |  3614 | `}` |
|        - |  3615 | `/*` |
|        - |  3616 | ` * Compile the 'continue' statement.` |
|        - |  3617 | ` * According to the PHP language reference` |
|        - |  3618 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|        - |  3619 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|        - |  3620 | ` *  iteration.` |
|        - |  3621 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|        - |  3622 | ` *  the purposes of continue.` |
|        - |  3623 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|        - |  3624 | ` *  of enclosing loops it should skip to the end of.` |
|        - |  3625 | ` *  Note:` |
|        - |  3626 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|        - |  3627 | ` */` |
|        - |  3628 | `/*` |
|        - |  3629 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|        - |  3630 | ` * block and the target loop block. This ensures finally blocks run when` |
|        - |  3631 | ` * break/continue crosses a try boundary.` |
|        - |  3632 | ` *` |
|        - |  3633 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|        - |  3634 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|        - |  3635 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|        - |  3636 | ` */` |
|    58412 |  3637 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|        5 |  3638 | `{` |
|    58417 |  3639 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    58417 |  3640 | `	int nInlineTry = 0;` |
|   272279 |  3641 | `	while( pBlock && pBlock != pTarget ){` |
|   213867 |  3642 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|        6 |  3643 | `			if( pBlock->pUserData ){` |
|        - |  3644 | `				/* A try block with an exception context. In a generator its catch/finally` |
|        - |  3645 | `				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that` |
|        - |  3646 | `				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.` |
|        - |  3647 | `				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */` |
|        6 |  3648 | `				if( pGen->bInGenerator ){` |
|        3 |  3649 | `					nInlineTry++;` |
|        2 |  3650 | `				}else{` |
|        3 |  3651 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|        - |  3652 | `				}` |
|        4 |  3653 | `			}else{` |
|        - |  3654 | `				/* A catch/finally block compiled into a separate bytecode container` |
|        - |  3655 | `				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */` |
|      ! 0 |  3656 | `				break;` |
|        - |  3657 | `			}` |
|        2 |  3658 | `		}` |
|   213867 |  3659 | `		pBlock = pBlock->pParent;` |
|        5 |  3660 | `	}` |
|    58417 |  3661 | `	return nInlineTry;` |
|        5 |  3662 | `}` |
|    27238 |  3663 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|        5 |  3664 | `{` |
|        - |  3665 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  3666 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  3667 | `	sxu32 nLineLocal;` |
|        - |  3668 | `	sxi32 rc;` |
|    27243 |  3669 | `	nLineLocal = pGen->pIn->nLine;` |
|    27243 |  3670 | `	iLevel = 0;` |
|        - |  3671 | `	/* Jump the 'continue' keyword */` |
|    27243 |  3672 | `	pGen->pIn++;` |
|    27243 |  3673 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|        - |  3674 | `		/* optional numeric argument which tells us how many levels` |
|        - |  3675 | `		 * of enclosing loops we should skip to the end of.` |
|        - |  3676 | `		 */` |
|        - |  3677 | `		char zScratch[GEN_NUM_SCRATCH];` |
|       17 |  3678 | `		char *zAlloc = 0;` |
|        - |  3679 | `		SyString sNum;` |
|       17 |  3680 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|       17 |  3681 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3682 | `			return SXERR_ABORT;` |
|        - |  3683 | `		}` |
|       17 |  3684 | `		if( rc == SXRET_OK ){` |
|       20 |  3685 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       12 |  3686 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|       14 |  3687 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3688 | `				return SXERR_ABORT;` |
|        - |  3689 | `			}` |
|       14 |  3690 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       14 |  3691 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        6 |  3692 | `		}` |
|       17 |  3693 | `		if( iLevel < 2 ){` |
|        3 |  3694 | `			iLevel = 0;` |
|        1 |  3695 | `		}` |
|       17 |  3696 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|        7 |  3697 | `	}` |
|        - |  3698 | `	/* Point to the target loop */` |
|    27243 |  3699 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    27243 |  3700 | `	if( pLoop == 0 ){` |
|        - |  3701 | `		/* Illegal continue */` |
|       12 |  3702 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|       12 |  3703 | `		if( rc == SXERR_ABORT ){` |
|        - |  3704 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3705 | `			return SXERR_ABORT;` |
|        - |  3706 | `		}` |
|        7 |  3707 | `	}else{` |
|    27233 |  3708 | `		sxu32 nInstrIdx = 0;` |
|        - |  3709 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    27233 |  3710 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  3711 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|        - |  3712 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|    27233 |  3713 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|    27233 |  3714 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|        - |  3715 | `			/* According to the PHP language reference manual` |
|        - |  3716 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|        - |  3717 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|        - |  3718 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|        - |  3719 | `			 */` |
|        5 |  3720 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|        5 |  3721 | `			if( rc == SXRET_OK ){` |
|        5 |  3722 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|        2 |  3723 | `			}` |
|        3 |  3724 | `		}else{` |
|        - |  3725 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    27229 |  3726 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    27229 |  3727 | `			if( pLoop->bPostContinue == TRUE ){` |
|        - |  3728 | `				JumpFixup sJumpFix;` |
|        - |  3729 | `				/* Post-continue */` |
|       14 |  3730 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|       14 |  3731 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|       14 |  3732 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|        6 |  3733 | `			}` |
|        - |  3734 | `		}` |
|        - |  3735 | `	}` |
|    27243 |  3736 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  3737 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  3738 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|      ! 0 |  3739 | `	}` |
|        - |  3740 | `	/* Statement successfully compiled */` |
|    27243 |  3741 | `	return SXRET_OK;` |
|    13624 |  3742 | `}` |
|        - |  3743 | `/*` |
|        - |  3744 | ` * Compile the 'break' statement.` |
|        - |  3745 | ` * According to the PHP language reference` |
|        - |  3746 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|        - |  3747 | ` *  structure.` |
|        - |  3748 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|        - |  3749 | ` *  enclosing structures are to be broken out of.` |
|        - |  3750 | ` */` |
|    31200 |  3751 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|        5 |  3752 | `{` |
|        - |  3753 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  3754 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  3755 | `	sxi32 rc;` |
|    31205 |  3756 | `	iLevel = 0;` |
|        - |  3757 | `	/* Jump the 'break' keyword */` |
|    31205 |  3758 | `	pGen->pIn++;` |
|    31205 |  3759 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|        - |  3760 | `		/* optional numeric argument which tells us how many levels` |
|        - |  3761 | `		 * of enclosing loops we should skip to the end of.` |
|        - |  3762 | `		 */` |
|        - |  3763 | `		char zScratch[GEN_NUM_SCRATCH];` |
|       18 |  3764 | `		char *zAlloc = 0;` |
|        - |  3765 | `		SyString sNum;` |
|       18 |  3766 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|       18 |  3767 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3768 | `			return SXERR_ABORT;` |
|        - |  3769 | `		}` |
|       18 |  3770 | `		if( rc == SXRET_OK ){` |
|       21 |  3771 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       12 |  3772 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|       15 |  3773 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3774 | `				return SXERR_ABORT;` |
|        - |  3775 | `			}` |
|       15 |  3776 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       15 |  3777 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        6 |  3778 | `		}` |
|       18 |  3779 | `		if( iLevel < 2 ){` |
|        3 |  3780 | `			iLevel = 0;` |
|        1 |  3781 | `		}` |
|       18 |  3782 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|        7 |  3783 | `	}` |
|        - |  3784 | `	/* Extract the target loop */` |
|    31205 |  3785 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    31205 |  3786 | `	if( pLoop == 0 ){` |
|        - |  3787 | `		/* Illegal break */` |
|       19 |  3788 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|       19 |  3789 | `		if( rc == SXERR_ABORT ){` |
|        - |  3790 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3791 | `			return SXERR_ABORT;` |
|        - |  3792 | `		}` |
|       11 |  3793 | `	}else{` |
|        - |  3794 | `		sxu32 nInstrIdx;` |
|        - |  3795 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    31189 |  3796 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|        - |  3797 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|    31189 |  3798 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|    31189 |  3799 | `		if( rc == SXRET_OK ){` |
|        - |  3800 | `			/* Fix the jump later when the jump destination is resolved */` |
|    31189 |  3801 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|    15592 |  3802 | `		}` |
|        - |  3803 | `	}` |
|    31205 |  3804 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  3805 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  3806 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|      ! 0 |  3807 | `	}` |
|        - |  3808 | `	/* Statement successfully compiled */` |
|    31205 |  3809 | `	return SXRET_OK;` |
|    15605 |  3810 | `}` |
|        - |  3811 | `/*` |
|        - |  3812 | ` * Compile or record a label.` |
|        - |  3813 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|        - |  3814 | ` * Example` |
|        - |  3815 | ` *  goto LABEL;` |
|        - |  3816 | ` *   echo 'Foo';` |
|        - |  3817 | ` *  LABEL:` |
|        - |  3818 | ` *   echo 'Bar';` |
|        - |  3819 | ` */` |
|      112 |  3820 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|        5 |  3821 | `{` |
|        - |  3822 | `	GenBlock *pBlock;` |
|        - |  3823 | `	Label sLabel;` |
|        - |  3824 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|      117 |  3825 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|      117 |  3826 | `	if( pBlock ){` |
|        - |  3827 | `		sxi32 rc;` |
|        8 |  3828 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        4 |  3829 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|        6 |  3830 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  3831 | `			return SXERR_ABORT;` |
|        - |  3832 | `		}` |
|        4 |  3833 | `	}else{` |
|      113 |  3834 | `		SyString *pTarget = &pGen->pIn->sData;` |
|        - |  3835 | `		char *zDup;` |
|        - |  3836 | `		/* Initialize label fields */` |
|      113 |  3837 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|        - |  3838 | `		/* Duplicate label name */` |
|      113 |  3839 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      113 |  3840 | `		if( zDup == 0 ){` |
|      ! 0 |  3841 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  3842 | `			return SXERR_ABORT;` |
|        - |  3843 | `		}` |
|      113 |  3844 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|      113 |  3845 | `		sLabel.bRef  = FALSE;` |
|      113 |  3846 | `		sLabel.nLine = pGen->pIn->nLine;` |
|      113 |  3847 | `		pBlock = pGen->pCurrent;` |
|      221 |  3848 | `		while( pBlock ){` |
|      133 |  3849 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|       24 |  3850 | `				break;` |
|        - |  3851 | `			}` |
|        - |  3852 | `			/* Point to the upper block */` |
|      113 |  3853 | `			pBlock = pBlock->pParent;` |
|        5 |  3854 | `		}` |
|      113 |  3855 | `		if( pBlock ){` |
|       24 |  3856 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       14 |  3857 | `		}else{` |
|       93 |  3858 | `			sLabel.pFunc = 0;` |
|        - |  3859 | `		}` |
|        - |  3860 | `		/* Insert in label set */` |
|      113 |  3861 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|        - |  3862 | `	}` |
|      117 |  3863 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|      117 |  3864 | `	return SXRET_OK;` |
|       61 |  3865 | `}` |
|        - |  3866 | `/*` |
|        - |  3867 | ` * Compile the so hated 'goto' statement.` |
|        - |  3868 | ` * You've probably been taught that gotos are bad, but this sort` |
|        - |  3869 | ` * of rewriting  happens all the time, in fact every time you run` |
|        - |  3870 | ` * a compiler it has to do this.` |
|        - |  3871 | ` * According to the PHP language reference manual` |
|        - |  3872 | ` *   The goto operator can be used to jump to another section in the program.` |
|        - |  3873 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|        - |  3874 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|        - |  3875 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|        - |  3876 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|        - |  3877 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|        - |  3878 | ` *   of a multi-level break` |
|        - |  3879 | ` */` |
|      152 |  3880 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|        5 |  3881 | `{` |
|        - |  3882 | `	JumpFixup sJump;` |
|        - |  3883 | `	sxi32 rc;` |
|      157 |  3884 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|      157 |  3885 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  3886 | `		/* Missing label */` |
|      ! 0 |  3887 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|      ! 0 |  3888 | `		if( rc == SXERR_ABORT ){` |
|        - |  3889 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3890 | `			return SXERR_ABORT;` |
|        - |  3891 | `		}` |
|      ! 0 |  3892 | `		return SXRET_OK;` |
|        - |  3893 | `	}` |
|      157 |  3894 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        6 |  3895 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|        6 |  3896 | `		if( rc == SXERR_ABORT ){` |
|        - |  3897 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  3898 | `			return SXERR_ABORT;` |
|        - |  3899 | `		}` |
|        4 |  3900 | `	}else{` |
|      153 |  3901 | `		SyString *pTarget = &pGen->pIn->sData;` |
|        - |  3902 | `		GenBlock *pBlock;` |
|        - |  3903 | `		char *zDup;` |
|        - |  3904 | `		/* Prepare the jump destination */` |
|      153 |  3905 | `		sJump.nJumpType = PH7_OP_JMP;` |
|      153 |  3906 | `		sJump.nLine = pGen->pIn->nLine;` |
|        - |  3907 | `		/* Duplicate label name */` |
|      153 |  3908 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      153 |  3909 | `		if( zDup == 0 ){` |
|      ! 0 |  3910 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  3911 | `			return SXERR_ABORT;` |
|        - |  3912 | `		}` |
|      153 |  3913 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|      153 |  3914 | `		pBlock = pGen->pCurrent;` |
|      315 |  3915 | `		while( pBlock ){` |
|      199 |  3916 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|       37 |  3917 | `				break;` |
|        - |  3918 | `			}` |
|        - |  3919 | `			/* Point to the upper block */` |
|      167 |  3920 | `			pBlock = pBlock->pParent;` |
|        5 |  3921 | `		}` |
|      153 |  3922 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|        9 |  3923 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|        9 |  3924 | `			if( rc == SXERR_ABORT ){` |
|        - |  3925 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  3926 | `				return SXERR_ABORT;` |
|        - |  3927 | `			}` |
|        3 |  3928 | `		}` |
|      153 |  3929 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|       30 |  3930 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       17 |  3931 | `		}else{` |
|      127 |  3932 | `			sJump.pFunc = 0;` |
|        - |  3933 | `		}` |
|        - |  3934 | `		/* Emit the unconditional jump */` |
|      153 |  3935 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|      153 |  3936 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|       74 |  3937 | `		}` |
|        - |  3938 | `	}` |
|      157 |  3939 | `	pGen->pIn++; /* Jump the label name */` |
|      157 |  3940 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        3 |  3941 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|        1 |  3942 | `	}` |
|        - |  3943 | `	/* Statement successfully compiled */` |
|      157 |  3944 | `	return SXRET_OK;` |
|       81 |  3945 | `}` |
|        - |  3946 | `/*` |
|        - |  3947 | ` * Point to the next PHP chunk that will be processed shortly.` |
|        - |  3948 | ` * Return SXRET_OK on success. Any other return value indicates` |
|        - |  3949 | ` * failure.` |
|        - |  3950 | ` */` |
|       20 |  3951 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|        2 |  3952 | `{` |
|        - |  3953 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|        - |  3954 | `	sxu32 nRawObj;` |
|       10 |  3955 | `	sxu32 nObjIdx;` |
|        - |  3956 | `	/* Consume raw chunks verbatim without any processing until we get` |
|        - |  3957 | `	 * a PHP block.` |
|        - |  3958 | `	 */` |
|       10 |  3959 | `Consume:` |
|       22 |  3960 | `	nRawObj = nObjIdx = 0;` |
|       22 |  3961 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|      ! 0 |  3962 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|      ! 0 |  3963 | `		if( pRawObj == 0 ){` |
|      ! 0 |  3964 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  3965 | `			return SXERR_ABORT;` |
|        - |  3966 | `		}` |
|        - |  3967 | `		/* Mark as constant and emit the load constant instruction */` |
|      ! 0 |  3968 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|      ! 0 |  3969 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|      ! 0 |  3970 | `		++nRawObj;` |
|      ! 0 |  3971 | `		pGen->pRawIn++; /* Next chunk */` |
|      ! 0 |  3972 | `	}` |
|       22 |  3973 | `	if( nRawObj > 0 ){` |
|        - |  3974 | `		/* Emit the consume instruction */` |
|      ! 0 |  3975 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      ! 0 |  3976 | `	}` |
|       22 |  3977 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|      ! 0 |  3978 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|        - |  3979 | `		/* Reset the token set (and its trivia sidecar) */` |
|      ! 0 |  3980 | `		SySetReset(pTokenSet);` |
|      ! 0 |  3981 | `		SySetReset(&pGen->aTrivia);` |
|        - |  3982 | `		/* Tokenize input */` |
|      ! 0 |  3983 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|      ! 0 |  3984 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|        - |  3985 | `		/* Point to the fresh token stream */` |
|      ! 0 |  3986 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|      ! 0 |  3987 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|        - |  3988 | `		/* Advance the stream cursor */` |
|      ! 0 |  3989 | `		pGen->pRawIn++;` |
|        - |  3990 | `		/* TICKET 1433-011 */` |
|      ! 0 |  3991 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|        - |  3992 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|        - |  3993 | `			sxi32 rc;` |
|        - |  3994 | `			/* Refer to TICKET 1433-009  */` |
|      ! 0 |  3995 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|      ! 0 |  3996 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|      ! 0 |  3997 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|      ! 0 |  3998 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|      ! 0 |  3999 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4000 | `				return SXERR_ABORT;` |
|      ! 0 |  4001 | `			}else if( rc != SXERR_EMPTY ){` |
|      ! 0 |  4002 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 |  4003 | `			}` |
|      ! 0 |  4004 | `			goto Consume;` |
|        - |  4005 | `		}` |
|      ! 0 |  4006 | `	}else{` |
|        - |  4007 | `		/* No more chunks to process */` |
|       22 |  4008 | `		pGen->pIn = pGen->pEnd;` |
|       22 |  4009 | `		return SXERR_EOF;` |
|        - |  4010 | `	}` |
|      ! 0 |  4011 | `	return SXRET_OK;` |
|       12 |  4012 | `}` |
|        - |  4013 | `/*` |
|        - |  4014 | ` * Compile a PHP block.` |
|        - |  4015 | ` * A block is simply one or more PHP statements and expressions to compile` |
|        - |  4016 | ` * optionally delimited by braces {}.` |
|        - |  4017 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|        - |  4018 | ` * and this function takes care of generating the appropriate error` |
|        - |  4019 | ` * message.` |
|        - |  4020 | ` */` |
|  3020892 |  4021 | `static sxi32 PH7_CompileBlock(` |
|        - |  4022 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  4023 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|        - |  4024 | `	)` |
|        5 |  4025 | `{` |
|        - |  4026 | `	sxi32 rc;` |
|        - |  4027 | `	sxu32 nLine;` |
|  3020897 |  4028 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  3019439 |  4029 | `		nLine = pGen->pIn->nLine;` |
|  3019439 |  4030 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  3019439 |  4031 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  4032 | `			return SXERR_ABORT;` |
|        - |  4033 | `		}` |
|  3019439 |  4034 | `		pGen->pIn++;` |
|        - |  4035 | `		/* Compile until we hit the closing braces '}' */` |
|  4420585 |  4036 | `		for(;;){` |
|  8841175 |  4037 | `			if( pGen->pIn >= pGen->pEnd ){` |
|       22 |  4038 | `				rc = GenStateNextChunk(&(*pGen));` |
|       22 |  4039 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  4040 | `			 	   return SXERR_ABORT;` |
|        - |  4041 | `				}` |
|       22 |  4042 | `				if( rc == SXERR_EOF ){` |
|        - |  4043 | `					/* No more token to process. Missing closing braces */` |
|       22 |  4044 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|       22 |  4045 | `					break;` |
|        - |  4046 | `				}` |
|      ! 0 |  4047 | `			}` |
|  8841155 |  4048 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|        - |  4049 | `				/* Closing braces found,break immediately*/` |
|  3019419 |  4050 | `				pGen->pIn++;` |
|  3019419 |  4051 | `				break;` |
|        - |  4052 | `			}` |
|        - |  4053 | `			/* Compile a single statement */` |
|  5821741 |  4054 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  5821741 |  4055 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4056 | `				return SXERR_ABORT;` |
|        - |  4057 | `			}` |
|        5 |  4058 | `		}` |
|  3019439 |  4059 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  1511180 |  4060 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|      ! 0 |  4061 | `		pGen->pIn++;` |
|      ! 0 |  4062 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|      ! 0 |  4063 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  4064 | `			return SXERR_ABORT;` |
|        - |  4065 | `		}` |
|        - |  4066 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|      ! 0 |  4067 | `		for(;;){` |
|      ! 0 |  4068 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  4069 | `				rc = GenStateNextChunk(&(*pGen));` |
|      ! 0 |  4070 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  4071 | `			 	   return SXERR_ABORT;` |
|        - |  4072 | `				}` |
|      ! 0 |  4073 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|        - |  4074 | `					/* No more token to process */` |
|      ! 0 |  4075 | `					if( rc == SXERR_EOF ){` |
|      ! 0 |  4076 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|        - |  4077 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|      ! 0 |  4078 | `					}` |
|      ! 0 |  4079 | `					break;` |
|        - |  4080 | `				}` |
|      ! 0 |  4081 | `			}` |
|      ! 0 |  4082 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        - |  4083 | `				sxi32 nKwrd;` |
|        - |  4084 | `				/* Keyword found */` |
|      ! 0 |  4085 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      ! 0 |  4086 | `				if( nKwrd == nKeywordEnd \|\|` |
|      ! 0 |  4087 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|        - |  4088 | `						/* Delimiter keyword found,break */` |
|      ! 0 |  4089 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|      ! 0 |  4090 | `							pGen->pIn++; /*  endif;endswitch... */` |
|      ! 0 |  4091 | `						}` |
|      ! 0 |  4092 | `						break;` |
|        - |  4093 | `				}` |
|      ! 0 |  4094 | `			}` |
|        - |  4095 | `			/* Compile a single statement */` |
|      ! 0 |  4096 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|      ! 0 |  4097 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4098 | `				return SXERR_ABORT;` |
|        - |  4099 | `			}` |
|      ! 0 |  4100 | `		}` |
|      ! 0 |  4101 | `		GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  4102 | `	}else{` |
|        - |  4103 | `		/* Compile a single statement */` |
|     1463 |  4104 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     1463 |  4105 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4106 | `			return SXERR_ABORT;` |
|        - |  4107 | `		}` |
|        - |  4108 | `	}` |
|        - |  4109 | `	/* Jump trailing semi-colons ';' */` |
|  3020897 |  4110 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      ! 0 |  4111 | `		pGen->pIn++;` |
|      ! 0 |  4112 | `	}` |
|  3020897 |  4113 | `	return SXRET_OK;` |
|  1510451 |  4114 | `}` |
|        - |  4115 | `/*` |
|        - |  4116 | ` * Compile the gentle 'while' statement.` |
|        - |  4117 | ` * According to the PHP language reference` |
|        - |  4118 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|        - |  4119 | ` *  The basic form of a while statement is:` |
|        - |  4120 | ` *  while (expr)` |
|        - |  4121 | ` *   statement` |
|        - |  4122 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|        - |  4123 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|        - |  4124 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|        - |  4125 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|        - |  4126 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|        - |  4127 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|        - |  4128 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|        - |  4129 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|        - |  4130 | ` *  while (expr):` |
|        - |  4131 | ` *    statement` |
|        - |  4132 | ` *   endwhile;` |
|        - |  4133 | ` */` |
|    15672 |  4134 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|        5 |  4135 | `{` |
|    15677 |  4136 | `	GenBlock *pWhileBlock = 0;` |
|    15677 |  4137 | `	SyToken *pTmp,*pEnd = 0;` |
|        - |  4138 | `	sxu32 nFalseJump;` |
|        - |  4139 | `	sxu32 nLine;` |
|        - |  4140 | `	sxi32 rc;` |
|    15677 |  4141 | `	nLine = pGen->pIn->nLine;` |
|        - |  4142 | `	/* Jump the 'while' keyword */` |
|    15677 |  4143 | `	pGen->pIn++;` |
|    15677 |  4144 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4145 | `		/* Syntax error */` |
|      ! 0 |  4146 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  4147 | `		if( rc == SXERR_ABORT ){` |
|        - |  4148 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4149 | `			return SXERR_ABORT;` |
|        - |  4150 | `		}` |
|      ! 0 |  4151 | `		goto Synchronize;` |
|        - |  4152 | `	}` |
|        - |  4153 | `	/* Jump the left parenthesis '(' */` |
|    15677 |  4154 | `	pGen->pIn++;` |
|        - |  4155 | `	/* Create the loop block */` |
|    15677 |  4156 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|    15677 |  4157 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4158 | `		return SXERR_ABORT;` |
|        - |  4159 | `	}` |
|        - |  4160 | `	/* Delimit the condition */` |
|    15677 |  4161 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    15677 |  4162 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  4163 | `		/* Empty expression */` |
|        3 |  4164 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|        3 |  4165 | `		if( rc == SXERR_ABORT ){` |
|        - |  4166 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4167 | `			return SXERR_ABORT;` |
|        - |  4168 | `		}` |
|        1 |  4169 | `	}` |
|        - |  4170 | `	/* Swap token streams */` |
|    15677 |  4171 | `	pTmp = pGen->pEnd;` |
|    15677 |  4172 | `	pGen->pEnd = pEnd;` |
|        - |  4173 | `	/* Compile the expression */` |
|    15677 |  4174 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    15677 |  4175 | `	if( rc == SXERR_ABORT ){` |
|        - |  4176 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4177 | `		return SXERR_ABORT;` |
|        - |  4178 | `	}` |
|        - |  4179 | `	/* Update token stream */` |
|    15677 |  4180 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  4181 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4182 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4183 | `			return SXERR_ABORT;` |
|        - |  4184 | `		}` |
|      ! 0 |  4185 | `		pGen->pIn++;` |
|      ! 0 |  4186 | `	}` |
|        - |  4187 | `	/* Synchronize pointers */` |
|    15677 |  4188 | `	pGen->pIn  = &pEnd[1];` |
|    15677 |  4189 | `	pGen->pEnd = pTmp;` |
|        - |  4190 | `	/* Emit the false jump */` |
|    15677 |  4191 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - |  4192 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    15677 |  4193 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|        - |  4194 | `	/* Compile the loop body */` |
|    15677 |  4195 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|    15677 |  4196 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  4197 | `		return SXERR_ABORT;` |
|        - |  4198 | `	}` |
|        - |  4199 | `	/* Emit the unconditional jump to the start of the loop */` |
|    15677 |  4200 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|        - |  4201 | `	/* Fix all jumps now the destination is resolved */` |
|    15677 |  4202 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4203 | `	/* Release the loop block */` |
|    15677 |  4204 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4205 | `	/* Statement successfully compiled */` |
|    15677 |  4206 | `	return SXRET_OK;` |
|      ! 0 |  4207 | `Synchronize:` |
|        - |  4208 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  4209 | `	 * compiling this erroneous block.` |
|        - |  4210 | `	 */` |
|      ! 0 |  4211 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  4212 | `		pGen->pIn++;` |
|      ! 0 |  4213 | `	}` |
|      ! 0 |  4214 | `	return SXRET_OK;` |
|     7841 |  4215 | `}` |
|        - |  4216 | `/*` |
|        - |  4217 | ` * Compile the ugly do..while() statement.` |
|        - |  4218 | ` * According to the PHP language reference` |
|        - |  4219 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|        - |  4220 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|        - |  4221 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|        - |  4222 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|        - |  4223 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|        - |  4224 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|        - |  4225 | ` *  would end immediately).` |
|        - |  4226 | ` *  There is just one syntax for do-while loops:` |
|        - |  4227 | ` *  <?php` |
|        - |  4228 | ` *  $i = 0;` |
|        - |  4229 | ` *  do {` |
|        - |  4230 | ` *   echo $i;` |
|        - |  4231 | ` *  } while ($i > 0);` |
|        - |  4232 | ` * ?>` |
|        - |  4233 | ` */` |
|        2 |  4234 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|        1 |  4235 | `{` |
|        3 |  4236 | `	SyToken *pTmp,*pEnd = 0;` |
|        3 |  4237 | `	GenBlock *pDoBlock = 0;` |
|        - |  4238 | `	sxu32 nLine;` |
|        - |  4239 | `	sxi32 rc;` |
|        3 |  4240 | `	nLine = pGen->pIn->nLine;` |
|        - |  4241 | `	/* Jump the 'do' keyword */` |
|        3 |  4242 | `	pGen->pIn++;` |
|        - |  4243 | `	/* Create the loop block */` |
|        3 |  4244 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|        3 |  4245 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4246 | `		return SXERR_ABORT;` |
|        - |  4247 | `	}` |
|        - |  4248 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|        3 |  4249 | `	pDoBlock->bPostContinue = TRUE;` |
|        3 |  4250 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|        3 |  4251 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  4252 | `		return SXERR_ABORT;` |
|        - |  4253 | `	}` |
|        3 |  4254 | `	if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  4255 | `		nLine = pGen->pIn->nLine;` |
|      ! 0 |  4256 | `	}` |
|        3 |  4257 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|      ! 0 |  4258 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|        - |  4259 | `			/* Missing 'while' statement */` |
|        3 |  4260 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|        3 |  4261 | `			if( rc == SXERR_ABORT ){` |
|        - |  4262 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4263 | `				return SXERR_ABORT;` |
|        - |  4264 | `			}` |
|        3 |  4265 | `			goto Synchronize;` |
|        - |  4266 | `	}` |
|        - |  4267 | `	/* Jump the 'while' keyword */` |
|      ! 0 |  4268 | `	pGen->pIn++;` |
|      ! 0 |  4269 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4270 | `		/* Syntax error */` |
|      ! 0 |  4271 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  4272 | `		if( rc == SXERR_ABORT ){` |
|        - |  4273 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4274 | `			return SXERR_ABORT;` |
|        - |  4275 | `		}` |
|      ! 0 |  4276 | `		goto Synchronize;` |
|        - |  4277 | `	}` |
|        - |  4278 | `	/* Jump the left parenthesis '(' */` |
|      ! 0 |  4279 | `	pGen->pIn++;` |
|        - |  4280 | `	/* Delimit the condition */` |
|      ! 0 |  4281 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      ! 0 |  4282 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  4283 | `		/* Empty expression */` |
|      ! 0 |  4284 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|      ! 0 |  4285 | `		if( rc == SXERR_ABORT ){` |
|        - |  4286 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4287 | `			return SXERR_ABORT;` |
|        - |  4288 | `		}` |
|      ! 0 |  4289 | `		goto Synchronize;` |
|        - |  4290 | `	}` |
|        - |  4291 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|      ! 0 |  4292 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|        - |  4293 | `		JumpFixup *aPost;` |
|        - |  4294 | `		VmInstr *pInstr;` |
|        - |  4295 | `		sxu32 nJumpDest;` |
|        - |  4296 | `		sxu32 n;` |
|      ! 0 |  4297 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|      ! 0 |  4298 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      ! 0 |  4299 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|      ! 0 |  4300 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      ! 0 |  4301 | `			if( pInstr ){` |
|        - |  4302 | `				/* Fix */` |
|      ! 0 |  4303 | `				pInstr->iP2 = nJumpDest;` |
|      ! 0 |  4304 | `			}` |
|      ! 0 |  4305 | `		}` |
|      ! 0 |  4306 | `	}` |
|        - |  4307 | `	/* Swap token streams */` |
|      ! 0 |  4308 | `	pTmp = pGen->pEnd;` |
|      ! 0 |  4309 | `	pGen->pEnd = pEnd;` |
|        - |  4310 | `	/* Compile the expression */` |
|      ! 0 |  4311 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      ! 0 |  4312 | `	if( rc == SXERR_ABORT ){` |
|        - |  4313 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4314 | `		return SXERR_ABORT;` |
|        - |  4315 | `	}` |
|        - |  4316 | `	/* Update token stream */` |
|      ! 0 |  4317 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  4318 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4319 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4320 | `			return SXERR_ABORT;` |
|        - |  4321 | `		}` |
|      ! 0 |  4322 | `		pGen->pIn++;` |
|      ! 0 |  4323 | `	}` |
|      ! 0 |  4324 | `	pGen->pIn  = &pEnd[1];` |
|      ! 0 |  4325 | `	pGen->pEnd = pTmp;` |
|        - |  4326 | `	/* Emit the true jump to the beginning of the loop */` |
|      ! 0 |  4327 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|        - |  4328 | `	/* Fix all jumps now the destination is resolved */` |
|      ! 0 |  4329 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4330 | `	/* Release the loop block */` |
|      ! 0 |  4331 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4332 | `	/* Statement successfully compiled */` |
|      ! 0 |  4333 | `	return SXRET_OK;` |
|        1 |  4334 | `Synchronize:` |
|        - |  4335 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  4336 | `	 * compiling this erroneous block.` |
|        - |  4337 | `	 */` |
|        3 |  4338 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  4339 | `		pGen->pIn++;` |
|      ! 0 |  4340 | `	}` |
|        3 |  4341 | `	return SXRET_OK;` |
|        2 |  4342 | `}` |
|        - |  4343 | `/*` |
|        - |  4344 | ` * Compile the complex and powerful 'for' statement.` |
|        - |  4345 | ` * According to the PHP language reference` |
|        - |  4346 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|        - |  4347 | ` *  The syntax of a for loop is:` |
|        - |  4348 | ` *  for (expr1; expr2; expr3)` |
|        - |  4349 | ` *   statement` |
|        - |  4350 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|        - |  4351 | ` *  the beginning of the loop.` |
|        - |  4352 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|        - |  4353 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|        - |  4354 | ` *  to FALSE, the execution of the loop ends.` |
|        - |  4355 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|        - |  4356 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|        - |  4357 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|        - |  4358 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|        - |  4359 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|        - |  4360 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|        - |  4361 | ` *  of using the for truth expression.` |
|        - |  4362 | ` */` |
|    38980 |  4363 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|        5 |  4364 | `{` |
|    38985 |  4365 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|    38985 |  4366 | `	GenBlock *pForBlock = 0;` |
|        - |  4367 | `	sxu32 nFalseJump;` |
|        - |  4368 | `	sxu32 nLine;` |
|        - |  4369 | `	sxi32 rc;` |
|    38985 |  4370 | `	nLine = pGen->pIn->nLine;` |
|        - |  4371 | `	/* Jump the 'for' keyword */` |
|    38985 |  4372 | `	pGen->pIn++;` |
|    38985 |  4373 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4374 | `		/* Syntax error */` |
|      ! 0 |  4375 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|      ! 0 |  4376 | `		if( rc == SXERR_ABORT ){` |
|        - |  4377 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4378 | `			return SXERR_ABORT;` |
|        - |  4379 | `		}` |
|      ! 0 |  4380 | `		return SXRET_OK;` |
|        - |  4381 | `	}` |
|        - |  4382 | `	/* Jump the left parenthesis '(' */` |
|    38985 |  4383 | `	pGen->pIn++;` |
|        - |  4384 | `	/* Delimit the init-expr;condition;post-expr */` |
|    38985 |  4385 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    38985 |  4386 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  4387 | `		/* Empty expression */` |
|      ! 0 |  4388 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|      ! 0 |  4389 | `		if( rc == SXERR_ABORT ){` |
|        - |  4390 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4391 | `			return SXERR_ABORT;` |
|        - |  4392 | `		}` |
|        - |  4393 | `		/* Synchronize */` |
|      ! 0 |  4394 | `		pGen->pIn = pEnd;` |
|      ! 0 |  4395 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  4396 | `			pGen->pIn++;` |
|      ! 0 |  4397 | `		}` |
|      ! 0 |  4398 | `		return SXRET_OK;` |
|        - |  4399 | `	}` |
|        - |  4400 | `	/* Swap token streams */` |
|    38985 |  4401 | `	pTmp = pGen->pEnd;` |
|    38985 |  4402 | `	pGen->pEnd = pEnd;` |
|        - |  4403 | `	/* Compile initialization expressions if available */` |
|    38985 |  4404 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  4405 | `	/* Pop operand lvalues */` |
|    38985 |  4406 | `	if( rc == SXERR_ABORT ){` |
|        - |  4407 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4408 | `		return SXERR_ABORT;` |
|    38985 |  4409 | `	}else if( rc != SXERR_EMPTY ){` |
|    38983 |  4410 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    19489 |  4411 | `	}` |
|    38985 |  4412 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  4413 | `		/* Syntax error */` |
|      ! 0 |  4414 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  4415 | `			"for: Expected ';' after initialization expressions");` |
|      ! 0 |  4416 | `		if( rc == SXERR_ABORT ){` |
|        - |  4417 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4418 | `			return SXERR_ABORT;` |
|        - |  4419 | `		}` |
|      ! 0 |  4420 | `		return SXRET_OK;` |
|        - |  4421 | `	}` |
|        - |  4422 | `	/* Jump the trailing ';' */` |
|    38985 |  4423 | `	pGen->pIn++;` |
|        - |  4424 | `	/* Create the loop block */` |
|    38985 |  4425 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|    38985 |  4426 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4427 | `		return SXERR_ABORT;` |
|        - |  4428 | `	}` |
|        - |  4429 | `	/* Deffer continue jumps */` |
|    38985 |  4430 | `	pForBlock->bPostContinue = TRUE;` |
|        - |  4431 | `	/* Compile the condition */` |
|    38985 |  4432 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    38985 |  4433 | `	if( rc == SXERR_ABORT ){` |
|        - |  4434 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4435 | `		return SXERR_ABORT;` |
|    38985 |  4436 | `	}else if( rc != SXERR_EMPTY ){` |
|        - |  4437 | `		/* Emit the false jump */` |
|    38983 |  4438 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - |  4439 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    38983 |  4440 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    19489 |  4441 | `	}` |
|    38985 |  4442 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  4443 | `		/* Syntax error */` |
|        6 |  4444 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  4445 | `			"for: Expected ';' after conditionals expressions");` |
|        6 |  4446 | `		if( rc == SXERR_ABORT ){` |
|        - |  4447 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4448 | `			return SXERR_ABORT;` |
|        - |  4449 | `		}` |
|        6 |  4450 | `		return SXRET_OK;` |
|        - |  4451 | `	}` |
|        - |  4452 | `	/* Jump the trailing ';' */` |
|    38981 |  4453 | `	pGen->pIn++;` |
|        - |  4454 | `	/* Save the post condition stream */` |
|    38981 |  4455 | `	pPostStart = pGen->pIn;` |
|        - |  4456 | `	/* Compile the loop body */` |
|    38981 |  4457 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|    38981 |  4458 | `	pGen->pEnd = pTmp;` |
|    38981 |  4459 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|    38981 |  4460 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  4461 | `		return SXERR_ABORT;` |
|        - |  4462 | `	}` |
|        - |  4463 | `	/* Fix post-continue jumps */` |
|    38981 |  4464 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|        - |  4465 | `		JumpFixup *aPost;` |
|        - |  4466 | `		VmInstr *pInstr;` |
|        - |  4467 | `		sxu32 nJumpDest;` |
|        - |  4468 | `		sxu32 n;` |
|       14 |  4469 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|       14 |  4470 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       26 |  4471 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|       14 |  4472 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|       14 |  4473 | `			if( pInstr ){` |
|        - |  4474 | `				/* Fix jump */` |
|       14 |  4475 | `				pInstr->iP2 = nJumpDest;` |
|        6 |  4476 | `			}` |
|        8 |  4477 | `		}` |
|        6 |  4478 | `	}` |
|        - |  4479 | `	/* compile the post-expressions if available */` |
|    38981 |  4480 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|      ! 0 |  4481 | `		pPostStart++;` |
|      ! 0 |  4482 | `	}` |
|    38981 |  4483 | `	if( pPostStart < pEnd ){` |
|        - |  4484 | `		SyToken *pTmpIn,*pTmpEnd;` |
|    38981 |  4485 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|    38981 |  4486 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    38981 |  4487 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - |  4488 | `			/* Syntax error */` |
|      ! 0 |  4489 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|      ! 0 |  4490 | `			if( rc == SXERR_ABORT ){` |
|        - |  4491 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4492 | `				return SXERR_ABORT;` |
|        - |  4493 | `			}` |
|      ! 0 |  4494 | `			return SXRET_OK;` |
|        - |  4495 | `		}` |
|    38981 |  4496 | `		RE_SWAP_DELIMITER(pGen);` |
|    38981 |  4497 | `		if( rc == SXERR_ABORT ){` |
|        - |  4498 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4499 | `			return SXERR_ABORT;` |
|    38981 |  4500 | `		}else if( rc != SXERR_EMPTY){` |
|        - |  4501 | `			/* Pop operand lvalue */` |
|    38981 |  4502 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    19488 |  4503 | `		}` |
|    19488 |  4504 | `	}` |
|        - |  4505 | `	/* Emit the unconditional jump to the start of the loop */` |
|    38981 |  4506 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|        - |  4507 | `	/* Fix all jumps now the destination is resolved */` |
|    38981 |  4508 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4509 | `	/* Release the loop block */` |
|    38981 |  4510 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4511 | `	/* Statement successfully compiled */` |
|    38981 |  4512 | `	return SXRET_OK;` |
|    19495 |  4513 | `}` |
|        - |  4514 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|        - |  4515 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|        - |  4516 | ` * are allowed.` |
|        - |  4517 | ` */` |
|   241616 |  4518 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 |  4519 | `{` |
|   241621 |  4520 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|   241621 |  4521 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|        - |  4522 | `		/* Unexpected expression */` |
|      ! 0 |  4523 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - |  4524 | `			"foreach: Expecting a variable name");` |
|      ! 0 |  4525 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 |  4526 | `			rc = SXERR_INVALID;` |
|      ! 0 |  4527 | `		}` |
|      ! 0 |  4528 | `	}` |
|   241621 |  4529 | `	return rc;` |
|        5 |  4530 | `}` |
|        - |  4531 | `/*` |
|        - |  4532 | ` * Compile the 'foreach' statement.` |
|        - |  4533 | ` * According to the PHP language reference` |
|        - |  4534 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|        - |  4535 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|        - |  4536 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|        - |  4537 | ` *  is a minor but useful extension of the first:` |
|        - |  4538 | ` *  foreach (array_expression as $value)` |
|        - |  4539 | ` *    statement` |
|        - |  4540 | ` *  foreach (array_expression as $key => $value)` |
|        - |  4541 | ` *   statement` |
|        - |  4542 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|        - |  4543 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|        - |  4544 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|        - |  4545 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|        - |  4546 | ` *  to the variable $key on each loop.` |
|        - |  4547 | ` *  Note:` |
|        - |  4548 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|        - |  4549 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|        - |  4550 | ` *  Note:` |
|        - |  4551 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|        - |  4552 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|        - |  4553 | ` *  or after the foreach without resetting it.` |
|        - |  4554 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|        - |  4555 | ` *  of copying the value.` |
|        - |  4556 | ` */` |
|   175378 |  4557 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|        5 |  4558 | `{` |
|   175383 |  4559 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|   175383 |  4560 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|   175383 |  4561 | `	GenBlock *pForeachBlock = 0;` |
|        - |  4562 | `	ph7_foreach_info *pInfo;` |
|        - |  4563 | `	sxu32 nFalseJump;` |
|        - |  4564 | `	VmInstr *pInstr;` |
|        - |  4565 | `	sxu32 nLine;` |
|        - |  4566 | `	sxi32 rc;` |
|   175383 |  4567 | `	nLine = pGen->pIn->nLine;` |
|        - |  4568 | `	/* Jump the 'foreach' keyword */` |
|   175383 |  4569 | `	pGen->pIn++;` |
|   175383 |  4570 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4571 | `		/* Syntax error */` |
|      ! 0 |  4572 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|      ! 0 |  4573 | `		if( rc == SXERR_ABORT ){` |
|        - |  4574 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4575 | `			return SXERR_ABORT;` |
|        - |  4576 | `		}` |
|      ! 0 |  4577 | `		goto Synchronize;` |
|        - |  4578 | `	}` |
|        - |  4579 | `	/* Jump the left parenthesis '(' */` |
|   175383 |  4580 | `	pGen->pIn++;` |
|        - |  4581 | `	/* Create the loop block */` |
|   175383 |  4582 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|   175383 |  4583 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4584 | `		return SXERR_ABORT;` |
|        - |  4585 | `	}` |
|        - |  4586 | `	/* Delimit the expression */` |
|   175383 |  4587 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   175383 |  4588 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  4589 | `		/* Empty expression */` |
|      ! 0 |  4590 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|      ! 0 |  4591 | `		if( rc == SXERR_ABORT ){` |
|        - |  4592 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  4593 | `			return SXERR_ABORT;` |
|        - |  4594 | `		}` |
|        - |  4595 | `		/* Synchronize */` |
|      ! 0 |  4596 | `		pGen->pIn = pEnd;` |
|      ! 0 |  4597 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 |  4598 | `			pGen->pIn++;` |
|      ! 0 |  4599 | `		}` |
|      ! 0 |  4600 | `		return SXRET_OK;` |
|        - |  4601 | `	}` |
|        - |  4602 | `	/* Compile the array expression */` |
|   175383 |  4603 | `	pCur = pGen->pIn;` |
|  1024999 |  4604 | `	while( pCur < pEnd ){` |
|  1024999 |  4605 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|   179281 |  4606 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|   179281 |  4607 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|        - |  4608 | `				/* Break with the first 'as' found */` |
|   175383 |  4609 | `				break;` |
|        - |  4610 | `			}` |
|     1949 |  4611 | `		}` |
|        - |  4612 | `		/* Advance the stream cursor */` |
|   849621 |  4613 | `		pCur++;` |
|        5 |  4614 | `	}` |
|   175383 |  4615 | `	if( pCur <= pGen->pIn ){` |
|      ! 0 |  4616 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - |  4617 | `			"foreach: Missing array/object expression");` |
|      ! 0 |  4618 | `		if( rc == SXERR_ABORT ){` |
|        - |  4619 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4620 | `			return SXERR_ABORT;` |
|        - |  4621 | `		}` |
|      ! 0 |  4622 | `		goto Synchronize;` |
|        - |  4623 | `	}` |
|        - |  4624 | `	/* Swap token streams */` |
|   175383 |  4625 | `	pTmp = pGen->pEnd;` |
|   175383 |  4626 | `	pGen->pEnd = pCur;` |
|   175383 |  4627 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   175383 |  4628 | `	if( rc == SXERR_ABORT ){` |
|        - |  4629 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4630 | `		return SXERR_ABORT;` |
|        - |  4631 | `	}` |
|        - |  4632 | `	/* Update token stream */` |
|   175383 |  4633 | `	while(pGen->pIn < pCur ){` |
|      ! 0 |  4634 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4635 | `		if( rc == SXERR_ABORT ){` |
|        - |  4636 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4637 | `			return SXERR_ABORT;` |
|        - |  4638 | `		}` |
|      ! 0 |  4639 | `		pGen->pIn++;` |
|      ! 0 |  4640 | `	}` |
|   175383 |  4641 | `	pCur++; /* Jump the 'as' keyword */` |
|   175383 |  4642 | `	pGen->pIn = pCur;` |
|   175383 |  4643 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 |  4644 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|      ! 0 |  4645 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4646 | `			return SXERR_ABORT;` |
|        - |  4647 | `		}` |
|      ! 0 |  4648 | `	}` |
|        - |  4649 | `	/* Create the foreach context */` |
|   175383 |  4650 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|   175383 |  4651 | `	if( pInfo == 0 ){` |
|      ! 0 |  4652 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 |  4653 | `		return SXERR_ABORT;` |
|        - |  4654 | `	}` |
|        - |  4655 | `	/* Zero the structure */` |
|   175383 |  4656 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|        - |  4657 | `	/* Initialize structure fields */` |
|   175383 |  4658 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|        - |  4659 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|        - |  4660 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|        - |  4661 | `	 * '=>'. */` |
|   175383 |  4662 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|   175383 |  4663 | `	if( pCur < pEnd ){` |
|        - |  4664 | `		/* Compile the expression holding the key name */` |
|    66263 |  4665 | `		if( pGen->pIn >= pCur ){` |
|      ! 0 |  4666 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|      ! 0 |  4667 | `			if( rc == SXERR_ABORT ){` |
|        - |  4668 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4669 | `				return SXERR_ABORT;` |
|        - |  4670 | `			}` |
|      ! 0 |  4671 | `		}else{` |
|    66263 |  4672 | `			pGen->pEnd = pCur;` |
|    66263 |  4673 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    66263 |  4674 | `			if( rc == SXERR_ABORT ){` |
|        - |  4675 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4676 | `				return SXERR_ABORT;` |
|        - |  4677 | `			}` |
|    66263 |  4678 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    66263 |  4679 | `			if( pInstr->p3 ){` |
|        - |  4680 | `				/* Record key name */` |
|    66263 |  4681 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    33129 |  4682 | `			}` |
|    66263 |  4683 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|        - |  4684 | `		}` |
|    66263 |  4685 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    33129 |  4686 | `	}` |
|   175383 |  4687 | `	pGen->pEnd = pEnd;` |
|   175383 |  4688 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 |  4689 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|      ! 0 |  4690 | `		if( rc == SXERR_ABORT ){` |
|        - |  4691 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4692 | `			return SXERR_ABORT;` |
|        - |  4693 | `		}` |
|      ! 0 |  4694 | `		goto Synchronize;` |
|        - |  4695 | `	}` |
|   175383 |  4696 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|       31 |  4697 | `		pGen->pIn++;` |
|        - |  4698 | `		/* Pass by reference  */` |
|       31 |  4699 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       14 |  4700 | `	}` |
|        - |  4701 | `	/* Check if the value target is list() */` |
|   175383 |  4702 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        8 |  4703 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|        - |  4704 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|        - |  4705 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|        - |  4706 | `		 */` |
|        - |  4707 | `		static int iForeachListCnt = 0;` |
|        - |  4708 | `		char zTmp[128];` |
|        - |  4709 | `		sxu32 nLen;` |
|        - |  4710 | `		char *zDup;` |
|       10 |  4711 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|       10 |  4712 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       10 |  4713 | `		if( zDup == 0 ){` |
|      ! 0 |  4714 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  4715 | `			return SXERR_ABORT;` |
|        - |  4716 | `		}` |
|       10 |  4717 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - |  4718 | `		/* Save list() token boundaries */` |
|       10 |  4719 | `		pListStart = pGen->pIn;` |
|        - |  4720 | `		/* Advance past list(...) — validate parentheses */` |
|       10 |  4721 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|       10 |  4722 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        3 |  4723 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|        - |  4724 | `				"foreach: Expected '(' after 'list'");` |
|        3 |  4725 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4726 | `				return SXERR_ABORT;` |
|        - |  4727 | `			}` |
|        3 |  4728 | `			goto Synchronize;` |
|        - |  4729 | `		}` |
|        7 |  4730 | `		pGen->pIn++; /* Jump '(' */` |
|        7 |  4731 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|        7 |  4732 | `		if( pListEnd >= pEnd ){` |
|      ! 0 |  4733 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  4734 | `				"foreach: Missing closing ')' after list");` |
|      ! 0 |  4735 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4736 | `				return SXERR_ABORT;` |
|        - |  4737 | `			}` |
|      ! 0 |  4738 | `			goto Synchronize;` |
|        - |  4739 | `		}` |
|        7 |  4740 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|        7 |  4741 | `		pListEnd = pGen->pIn;` |
|        7 |  4742 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|   175378 |  4743 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|        - |  4744 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|        - |  4745 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|        - |  4746 | `		 */` |
|        - |  4747 | `		static int iForeachShortListCnt = 0;` |
|        - |  4748 | `		char zTmp[128];` |
|        - |  4749 | `		sxu32 nLen;` |
|        - |  4750 | `		char *zDup;` |
|       13 |  4751 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       13 |  4752 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       13 |  4753 | `		if( zDup == 0 ){` |
|      ! 0 |  4754 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  4755 | `			return SXERR_ABORT;` |
|        - |  4756 | `		}` |
|       13 |  4757 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - |  4758 | `		/* Save [...] token boundaries */` |
|       13 |  4759 | `		pListStart = pGen->pIn;` |
|        - |  4760 | `		/* Advance past [...] */` |
|       13 |  4761 | `		pGen->pIn++; /* Jump '[' */` |
|       13 |  4762 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       13 |  4763 | `		if( pListEnd >= pEnd ){` |
|      ! 0 |  4764 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  4765 | `				"foreach: Missing closing ']' after short list");` |
|      ! 0 |  4766 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4767 | `				return SXERR_ABORT;` |
|        - |  4768 | `			}` |
|      ! 0 |  4769 | `			goto Synchronize;` |
|        - |  4770 | `		}` |
|       13 |  4771 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       13 |  4772 | `		pListEnd = pGen->pIn;` |
|       13 |  4773 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|        7 |  4774 | `	}else{` |
|        - |  4775 | `		/* Compile the expression holding the value name */` |
|   175363 |  4776 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   175363 |  4777 | `		if( rc == SXERR_ABORT ){` |
|        - |  4778 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4779 | `			return SXERR_ABORT;` |
|        - |  4780 | `		}` |
|   175363 |  4781 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   175363 |  4782 | `		if( pInstr->p3 ){` |
|        - |  4783 | `			/* Record value name */` |
|   175363 |  4784 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    87679 |  4785 | `		}` |
|        - |  4786 | `	}` |
|        - |  4787 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|   175381 |  4788 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|        - |  4789 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   175381 |  4790 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|        - |  4791 | `	/* Record the first instruction to execute */` |
|   175381 |  4792 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|        - |  4793 | `	/* Emit the FOREACH_STEP instruction */` |
|   175381 |  4794 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|        - |  4795 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   175381 |  4796 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|        - |  4797 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|   175381 |  4798 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|        - |  4799 | `		SyToken *pSavedIn,*pSavedEnd;` |
|        - |  4800 | `		/* Load the temporary variable holding the current value onto the stack.` |
|        - |  4801 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|        - |  4802 | `		 */` |
|       19 |  4803 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|        - |  4804 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|        - |  4805 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|        - |  4806 | `		 * picks up the delimiter and the variable names inside.` |
|        - |  4807 | `		 */` |
|       19 |  4808 | `		pSavedIn = pGen->pIn;` |
|       19 |  4809 | `		pSavedEnd = pGen->pEnd;` |
|       19 |  4810 | `		pGen->pIn = pListStart;` |
|       19 |  4811 | `		pGen->pEnd = pListEnd;` |
|       19 |  4812 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       13 |  4813 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|        7 |  4814 | `		}else{` |
|        7 |  4815 | `			rc = PH7_CompileList(&(*pGen),0);` |
|        - |  4816 | `		}` |
|       19 |  4817 | `		pGen->pIn = pSavedIn;` |
|       19 |  4818 | `		pGen->pEnd = pSavedEnd;` |
|       19 |  4819 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4820 | `			return SXERR_ABORT;` |
|        - |  4821 | `		}` |
|        - |  4822 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       19 |  4823 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        9 |  4824 | `	}` |
|        - |  4825 | `	/* Compile the loop body */` |
|   175381 |  4826 | `	pGen->pIn = &pEnd[1];` |
|   175381 |  4827 | `	pGen->pEnd = pTmp;` |
|   175381 |  4828 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|   175381 |  4829 | `	if( rc == SXERR_ABORT ){` |
|        - |  4830 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  4831 | `		return SXERR_ABORT;` |
|        - |  4832 | `	}` |
|        - |  4833 | `	/* Emit the unconditional jump to the start of the loop */` |
|   175381 |  4834 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|        - |  4835 | `	/* Fix all jumps now the destination is resolved */` |
|   175381 |  4836 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  4837 | `	/* Release the loop block */` |
|   175381 |  4838 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4839 | `	/* Statement successfully compiled */` |
|   175381 |  4840 | `	return SXRET_OK;` |
|        1 |  4841 | `Synchronize:` |
|        - |  4842 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  4843 | `	 * compiling this erroneous block.` |
|        - |  4844 | `	 */` |
|        3 |  4845 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  4846 | `		pGen->pIn++;` |
|      ! 0 |  4847 | `	}` |
|        3 |  4848 | `	return SXRET_OK;` |
|    87694 |  4849 | `}` |
|        - |  4850 | `/*` |
|        - |  4851 | ` * Compile the infamous if/elseif/else if/else statements.` |
|        - |  4852 | ` * According to the PHP language reference` |
|        - |  4853 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|        - |  4854 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|        - |  4855 | ` *  that is similar to that of C:` |
|        - |  4856 | ` *  if (expr)` |
|        - |  4857 | ` *   statement` |
|        - |  4858 | ` *  else construct:` |
|        - |  4859 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|        - |  4860 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|        - |  4861 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|        - |  4862 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|        - |  4863 | ` *   $b, and a is NOT greater than b otherwise.` |
|        - |  4864 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|        - |  4865 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|        - |  4866 | ` *  elseif` |
|        - |  4867 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|        - |  4868 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|        - |  4869 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|        - |  4870 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|        - |  4871 | ` *   than b, a equal to b or a is smaller than b:` |
|        - |  4872 | ` *   <?php` |
|        - |  4873 | ` *    if ($a > $b) {` |
|        - |  4874 | ` *     echo "a is bigger than b";` |
|        - |  4875 | ` *    } elseif ($a == $b) {` |
|        - |  4876 | ` *     echo "a is equal to b";` |
|        - |  4877 | ` *    } else {` |
|        - |  4878 | ` *     echo "a is smaller than b";` |
|        - |  4879 | ` *    }` |
|        - |  4880 | ` *    ?>` |
|        - |  4881 | ` */` |
|  1183342 |  4882 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|        5 |  4883 | `{` |
|  1183347 |  4884 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  1183347 |  4885 | `	GenBlock *pCondBlock = 0;` |
|        - |  4886 | `	sxu32 nJumpIdx;` |
|        - |  4887 | `	sxu32 nKeyID;` |
|        - |  4888 | `	sxi32 rc;` |
|        - |  4889 | `	/* Jump the 'if' keyword */` |
|  1183347 |  4890 | `	pGen->pIn++;` |
|  1183347 |  4891 | `	pToken = pGen->pIn;` |
|        - |  4892 | `	/* Create the conditional block */` |
|  1183347 |  4893 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  1183347 |  4894 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4895 | `		return SXERR_ABORT;` |
|        - |  4896 | `	}` |
|        - |  4897 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   638338 |  4898 | `	for(;;){` |
|  1276681 |  4899 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  4900 | `			/* Syntax error */` |
|      ! 0 |  4901 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 |  4902 | `				pToken--;` |
|      ! 0 |  4903 | `			}` |
|      ! 0 |  4904 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|      ! 0 |  4905 | `			if( rc == SXERR_ABORT ){` |
|        - |  4906 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4907 | `				return SXERR_ABORT;` |
|        - |  4908 | `			}` |
|      ! 0 |  4909 | `			goto Synchronize;` |
|        - |  4910 | `		}` |
|        - |  4911 | `		/* Jump the left parenthesis '(' */` |
|  1276681 |  4912 | `		pToken++;` |
|        - |  4913 | `		/* Delimit the condition */` |
|  1276681 |  4914 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  1276681 |  4915 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|        - |  4916 | `			/* Syntax error */` |
|      ! 0 |  4917 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 |  4918 | `				pToken--;` |
|      ! 0 |  4919 | `			}` |
|      ! 0 |  4920 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|      ! 0 |  4921 | `			if( rc == SXERR_ABORT ){` |
|        - |  4922 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  4923 | `				return SXERR_ABORT;` |
|        - |  4924 | `			}` |
|      ! 0 |  4925 | `			goto Synchronize;` |
|        - |  4926 | `		}` |
|        - |  4927 | `		/* Swap token streams */` |
|  1276681 |  4928 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|        - |  4929 | `		/* Compile the condition */` |
|  1276681 |  4930 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  4931 | `		/* Update token stream */` |
|  1276681 |  4932 | `		while(pGen->pIn < pEnd ){` |
|      ! 0 |  4933 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  4934 | `			pGen->pIn++;` |
|      ! 0 |  4935 | `		}` |
|  1276681 |  4936 | `		pGen->pIn  = &pEnd[1];` |
|  1276681 |  4937 | `		pGen->pEnd = pTmp;` |
|  1276681 |  4938 | `		if( rc == SXERR_ABORT ){` |
|        - |  4939 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  4940 | `			return SXERR_ABORT;` |
|        - |  4941 | `		}` |
|        - |  4942 | `		/* Emit the false jump */` |
|  1276681 |  4943 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|        - |  4944 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  1276681 |  4945 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|        - |  4946 | `		/* Compile the body */` |
|  1276681 |  4947 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  1276681 |  4948 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  4949 | `			return SXERR_ABORT;` |
|        - |  4950 | `		}` |
|  1276681 |  4951 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   239764 |  4952 | `			break;` |
|        - |  4953 | `		}` |
|        - |  4954 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   797163 |  4955 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   797163 |  4956 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   617909 |  4957 | `			break;` |
|        - |  4958 | `		}` |
|        - |  4959 | `		/* Emit the unconditional jump */` |
|   179259 |  4960 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|        - |  4961 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   179259 |  4962 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   179259 |  4963 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   171373 |  4964 | `			pToken = &pGen->pIn[1];` |
|   171373 |  4965 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    85486 |  4966 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    42965 |  4967 | `					break;` |
|        - |  4968 | `			}` |
|    85453 |  4969 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    42724 |  4970 | `		}` |
|    93339 |  4971 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|        - |  4972 | `		/* Synchronize cursors */` |
|    93339 |  4973 | `		pToken = pGen->pIn;` |
|        - |  4974 | `		/* Fix the false jump */` |
|    93339 |  4975 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|        5 |  4976 | `	} /* For(;;) */` |
|        - |  4977 | `	/* Fix the false jump */` |
|  1183347 |  4978 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  1183347 |  4979 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   703824 |  4980 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|        - |  4981 | `			/* Compile the else block */` |
|    85925 |  4982 | `			pGen->pIn++;` |
|    85925 |  4983 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    85925 |  4984 | `			if( rc == SXERR_ABORT ){` |
|        - |  4985 |  |
|      ! 0 |  4986 | `				return SXERR_ABORT;` |
|        - |  4987 | `			}` |
|    42960 |  4988 | `	}` |
|  1183347 |  4989 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|        - |  4990 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  1183347 |  4991 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|        - |  4992 | `	/* Release the conditional block */` |
|  1183347 |  4993 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  4994 | `	/* Statement successfully compiled */` |
|  1183347 |  4995 | `	return SXRET_OK;` |
|      ! 0 |  4996 | `Synchronize:` |
|        - |  4997 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|        - |  4998 | `	 */` |
|      ! 0 |  4999 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  5000 | `		pGen->pIn++;` |
|      ! 0 |  5001 | `	}` |
|      ! 0 |  5002 | `	return SXRET_OK;` |
|   591676 |  5003 | `}` |
|        - |  5004 | `/*` |
|        - |  5005 | ` * Compile the global construct.` |
|        - |  5006 | ` * According to the PHP language reference` |
|        - |  5007 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|        - |  5008 | ` *  to be used in that function.` |
|        - |  5009 | ` *  Example #1 Using global` |
|        - |  5010 | ` *  <?php` |
|        - |  5011 | ` *   $a = 1;` |
|        - |  5012 | ` *   $b = 2;` |
|        - |  5013 | ` *   function Sum()` |
|        - |  5014 | ` *   {` |
|        - |  5015 | ` *    global $a, $b;` |
|        - |  5016 | ` *    $b = $a + $b;` |
|        - |  5017 | ` *   }` |
|        - |  5018 | ` *   Sum();` |
|        - |  5019 | ` *   echo $b;` |
|        - |  5020 | ` *  ?>` |
|        - |  5021 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|        - |  5022 | ` *  all references to either variable will refer to the global version. There is no limit` |
|        - |  5023 | ` *  to the number of global variables that can be manipulated by a function.` |
|        - |  5024 | ` */` |
|       36 |  5025 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|        5 |  5026 | `{` |
|       41 |  5027 | `	SyToken *pTmp,*pNext = 0;` |
|        - |  5028 | `	sxi32 nExpr;` |
|        - |  5029 | `	sxi32 rc;` |
|        - |  5030 | `	/* Jump the 'global' keyword */` |
|       41 |  5031 | `	pGen->pIn++;` |
|       41 |  5032 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        - |  5033 | `		/* Nothing to process */` |
|      ! 0 |  5034 | `		return SXRET_OK;` |
|        - |  5035 | `	}` |
|       41 |  5036 | `	pTmp = pGen->pEnd;` |
|       41 |  5037 | `	nExpr = 0;` |
|       87 |  5038 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       51 |  5039 | `		if( pGen->pIn < pNext ){` |
|       51 |  5040 | `			pGen->pEnd = pNext;` |
|       51 |  5041 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  5042 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|      ! 0 |  5043 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  5044 | `					return SXERR_ABORT;` |
|        - |  5045 | `				}` |
|      ! 0 |  5046 | `			}else{` |
|       51 |  5047 | `				pGen->pIn++;` |
|       51 |  5048 | `				if( pGen->pIn >= pGen->pEnd ){` |
|        - |  5049 | `					/* Emit a warning */` |
|      ! 0 |  5050 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|      ! 0 |  5051 | `				}else{` |
|       51 |  5052 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       51 |  5053 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  5054 | `						return SXERR_ABORT;` |
|       51 |  5055 | `					}else if(rc != SXERR_EMPTY ){` |
|       51 |  5056 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|       51 |  5057 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|        - |  5058 | `							/* Variable name, not a constant */` |
|       51 |  5059 | `							pLast->iP1 = 0;` |
|       23 |  5060 | `						}` |
|       51 |  5061 | `						nExpr++;` |
|       23 |  5062 | `					}` |
|        - |  5063 | `				}` |
|        - |  5064 | `			}` |
|       23 |  5065 | `		}` |
|        - |  5066 | `		/* Next expression in the stream */` |
|       51 |  5067 | `		pGen->pIn = pNext;` |
|        - |  5068 | `		/* Jump trailing commas */` |
|       61 |  5069 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       15 |  5070 | `			pGen->pIn++;` |
|        5 |  5071 | `		}` |
|        5 |  5072 | `	}` |
|        - |  5073 | `	/* Restore token stream */` |
|       41 |  5074 | `	pGen->pEnd = pTmp;` |
|       41 |  5075 | `	if( nExpr > 0 ){` |
|        - |  5076 | `		/* Emit the uplink instruction */` |
|       41 |  5077 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|       18 |  5078 | `	}` |
|       41 |  5079 | `	return SXRET_OK;` |
|       23 |  5080 | `}` |
|        - |  5081 | `/*` |
|        - |  5082 | ` * Compile the return statement.` |
|        - |  5083 | ` * According to the PHP language reference` |
|        - |  5084 | ` *  If called from within a function, the return() statement immediately ends execution` |
|        - |  5085 | ` *  of the current function, and returns its argument as the value of the function call.` |
|        - |  5086 | ` *  return() will also end the execution of an eval() statement or script file.` |
|        - |  5087 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|        - |  5088 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|        - |  5089 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|        - |  5090 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|        - |  5091 | ` *  from within the main script file, then script execution end.` |
|        - |  5092 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|        - |  5093 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|        - |  5094 | ` *  should do so as PHP has less work to do in this case.` |
|        - |  5095 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|        - |  5096 | ` */` |
|  1633254 |  5097 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|        5 |  5098 | `{` |
|  1633259 |  5099 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|        - |  5100 | `	sxi32 rc;` |
|  1633259 |  5101 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  1633259 |  5102 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|        - |  5103 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|        - |  5104 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|        - |  5105 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|        - |  5106 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|        - |  5107 | `	 * normally below so token processing stays consistent. */` |
|  4253847 |  5108 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  2620593 |  5109 | `		pFuncBlock = pFuncBlock->pParent;` |
|        5 |  5110 | `	}` |
|  1633254 |  5111 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|  1633227 |  5112 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|        3 |  5113 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  5114 | `			"A never-returning function must not return");` |
|        3 |  5115 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5116 | `			return SXERR_ABORT;` |
|        - |  5117 | `		}` |
|        1 |  5118 | `	}` |
|        - |  5119 | `	/* Jump the 'return' keyword */` |
|  1633259 |  5120 | `	pGen->pIn++;` |
|  1633259 |  5121 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  5122 | `		/* Compile the expression */` |
|  1617693 |  5123 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  1617693 |  5124 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5125 | `			return SXERR_ABORT;` |
|  1617693 |  5126 | `		}else if(rc != SXERR_EMPTY ){` |
|  1617693 |  5127 | `			nRet = 1;` |
|   808844 |  5128 | `		}` |
|   808844 |  5129 | `	}` |
|        - |  5130 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|        - |  5131 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|        - |  5132 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|        - |  5133 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|  1633259 |  5134 | `	if( pGen->bInGenerator ){` |
|       32 |  5135 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|       32 |  5136 | `		return SXRET_OK;` |
|        - |  5137 | `	}` |
|        - |  5138 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|        - |  5139 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|        - |  5140 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|        - |  5141 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|        - |  5142 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  1633231 |  5143 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  1633231 |  5144 | `	return SXRET_OK;` |
|   816632 |  5145 | `}` |
|        - |  5146 | `/*` |
|        - |  5147 | ` * Compile a yield expression.` |
|        - |  5148 | ` * Called from the expression code generator when a yield node is encountered.` |
|        - |  5149 | ` * Handles: yield, yield $value, yield $key => $value` |
|        - |  5150 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|        - |  5151 | ` */` |
|      384 |  5152 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        5 |  5153 | `{` |
|        - |  5154 | `	SyToken *pTmp, *pSplit;` |
|      389 |  5155 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      389 |  5156 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|        - |  5157 | `	sxi32 rc;` |
|      192 |  5158 | `	(void)iCompileFlag;` |
|        - |  5159 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      389 |  5160 | `	pGen->pIn++;` |
|        - |  5161 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|        - |  5162 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|        - |  5163 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|        - |  5164 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|        - |  5165 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|      384 |  5166 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|      227 |  5167 | `		&& pGen->pIn->sData.nByte == 4` |
|       72 |  5168 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|       67 |  5169 | `		pGen->pIn++; /* Skip 'from' */` |
|       67 |  5170 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       67 |  5171 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5172 | `			return SXERR_ABORT;` |
|        - |  5173 | `		}` |
|       67 |  5174 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 |  5175 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|      ! 0 |  5176 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|        - |  5177 | `				"Missing expression after 'yield from'");` |
|      ! 0 |  5178 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5179 | `				return SXERR_ABORT;` |
|        - |  5180 | `			}` |
|      ! 0 |  5181 | `		}` |
|       67 |  5182 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|       67 |  5183 | `		return SXRET_OK;` |
|        - |  5184 | `	}` |
|      327 |  5185 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  5186 | `		/* Bare yield — no value */` |
|        3 |  5187 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|        3 |  5188 | `		return SXRET_OK;` |
|        - |  5189 | `	}` |
|        - |  5190 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|      325 |  5191 | `	pSplit = 0;` |
|        - |  5192 | `	{` |
|      325 |  5193 | `		SyToken *pCur = pGen->pIn;` |
|      325 |  5194 | `		sxi32 nNest = 0;` |
|      781 |  5195 | `		while( pCur < pGen->pEnd ){` |
|      475 |  5196 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       18 |  5197 | `				nNest++;` |
|      467 |  5198 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       18 |  5199 | `				nNest--;` |
|      451 |  5200 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|       16 |  5201 | `				pSplit = pCur;` |
|       16 |  5202 | `				break;` |
|        - |  5203 | `			}` |
|      461 |  5204 | `			pCur++;` |
|        5 |  5205 | `		}` |
|        - |  5206 | `	}` |
|      325 |  5207 | `	pTmp = pGen->pEnd;` |
|      325 |  5208 | `	if( pSplit ){` |
|        - |  5209 | `		/* yield $key => $value */` |
|       16 |  5210 | `		pGen->pEnd = pSplit;` |
|       16 |  5211 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       16 |  5212 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       16 |  5213 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|       16 |  5214 | `		pGen->pEnd = pTmp;` |
|       16 |  5215 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       16 |  5216 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       16 |  5217 | `		iP1 = 1;` |
|       16 |  5218 | `		iP2 = 1;` |
|        9 |  5219 | `	}else{` |
|        - |  5220 | `		/* yield $value */` |
|      311 |  5221 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      311 |  5222 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      311 |  5223 | `		if( rc != SXERR_EMPTY ){` |
|      311 |  5224 | `			iP1 = 1;` |
|      153 |  5225 | `		}` |
|        - |  5226 | `	}` |
|      325 |  5227 | `	pGen->pEnd = pTmp;` |
|      325 |  5228 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      325 |  5229 | `	return SXRET_OK;` |
|      197 |  5230 | `}` |
|        - |  5231 | `/*` |
|        - |  5232 | ` * Compile the die/exit language construct.` |
|        - |  5233 | ` * The role of these constructs is to terminate execution of the script.` |
|        - |  5234 | ` * Shutdown functions will always be executed even if exit() is called.` |
|        - |  5235 | ` */` |
|      122 |  5236 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|        5 |  5237 | `{` |
|      127 |  5238 | `	sxi32 nExpr = 0;` |
|        - |  5239 | `	sxi32 rc;` |
|        - |  5240 | `	/* Jump the die/exit keyword */` |
|      127 |  5241 | `	pGen->pIn++;` |
|      127 |  5242 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  5243 | `		/* Compile the expression */` |
|      127 |  5244 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      127 |  5245 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5246 | `			return SXERR_ABORT;` |
|      127 |  5247 | `		}else if(rc != SXERR_EMPTY ){` |
|      127 |  5248 | `			nExpr = 1;` |
|       61 |  5249 | `		}` |
|       61 |  5250 | `	}` |
|        - |  5251 | `	/* Emit the HALT instruction */` |
|      127 |  5252 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      127 |  5253 | `	return SXRET_OK;` |
|       66 |  5254 | `}` |
|        - |  5255 | `/*` |
|        - |  5256 | ` * Compile the 'echo' language construct.` |
|        - |  5257 | ` */` |
|    17112 |  5258 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|        5 |  5259 | `{` |
|    17117 |  5260 | `	SyToken *pTmp,*pNext = 0;` |
|        - |  5261 | `	sxi32 rc;` |
|        - |  5262 | `	/* Jump the 'echo' keyword */` |
|    17117 |  5263 | `	pGen->pIn++;` |
|        - |  5264 | `	/* Compile arguments one after one */` |
|    17117 |  5265 | `	pTmp = pGen->pEnd;` |
|    41823 |  5266 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|    24711 |  5267 | `		if( pGen->pIn < pNext ){` |
|    24711 |  5268 | `			pGen->pEnd = pNext;` |
|    24711 |  5269 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|    24711 |  5270 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5271 | `				return SXERR_ABORT;` |
|    24711 |  5272 | `			}else if( rc != SXERR_EMPTY ){` |
|        - |  5273 | `				/* Emit the consume instruction */` |
|    24687 |  5274 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    12341 |  5275 | `			}` |
|    12353 |  5276 | `		}` |
|        - |  5277 | `		/* Jump trailing commas */` |
|    32305 |  5278 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     7599 |  5279 | `			pNext++;` |
|        5 |  5280 | `		}` |
|    24711 |  5281 | `		pGen->pIn = pNext;` |
|        5 |  5282 | `	}` |
|        - |  5283 | `	/* Restore token stream */` |
|    17117 |  5284 | `	pGen->pEnd = pTmp;` |
|    17117 |  5285 | `	return SXRET_OK;` |
|     8561 |  5286 | `}` |
|        - |  5287 | `/*` |
|        - |  5288 | ` * Compile the static statement.` |
|        - |  5289 | ` * According to the PHP language reference` |
|        - |  5290 | ` *  Another important feature of variable scoping is the static variable.` |
|        - |  5291 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|        - |  5292 | ` *  when program execution leaves this scope.` |
|        - |  5293 | ` *  Static variables also provide one way to deal with recursive functions.` |
|        - |  5294 | ` * Symisc eXtension.` |
|        - |  5295 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|        - |  5296 | ` *  the zend engine would allow only simple scalar value.` |
|        - |  5297 | ` *  Example` |
|        - |  5298 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|        - |  5299 | ` *    Refer to the official documentation for more information on this feature.` |
|        - |  5300 | ` */` |
|       12 |  5301 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|        3 |  5302 | `{` |
|        - |  5303 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|        - |  5304 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|        - |  5305 | `	GenBlock *pBlock;` |
|        - |  5306 | `	SyString *pName;` |
|        - |  5307 | `	char *zDup;` |
|        - |  5308 | `	sxu32 nLine;` |
|        - |  5309 | `	sxi32 rc;` |
|        - |  5310 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|        - |  5311 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|        - |  5312 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|       12 |  5313 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|       10 |  5314 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|        1 |  5315 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|        3 |  5316 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        3 |  5317 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5318 | `			return SXERR_ABORT;` |
|        3 |  5319 | `		}else if( rc != SXERR_EMPTY ){` |
|        3 |  5320 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        1 |  5321 | `		}` |
|        3 |  5322 | `		return SXRET_OK;` |
|        - |  5323 | `	}` |
|        - |  5324 | `	/* Jump the static keyword */` |
|       13 |  5325 | `	nLine = pGen->pIn->nLine;` |
|       13 |  5326 | `	pGen->pIn++;` |
|        - |  5327 | `	/* Extract the enclosing function if any */` |
|       13 |  5328 | `	pBlock = pGen->pCurrent;` |
|       23 |  5329 | `	while( pBlock ){` |
|       23 |  5330 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       13 |  5331 | `			break;` |
|        - |  5332 | `		}` |
|        - |  5333 | `		/* Point to the upper block */` |
|       13 |  5334 | `		pBlock = pBlock->pParent;` |
|        3 |  5335 | `	}` |
|       13 |  5336 | `	if( pBlock == 0 ){` |
|        - |  5337 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|      ! 0 |  5338 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  5339 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|      ! 0 |  5340 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5341 | `				return SXERR_ABORT;` |
|        - |  5342 | `			}` |
|      ! 0 |  5343 | `			goto Synchronize;` |
|        - |  5344 | `		}` |
|        - |  5345 | `		/* Compile the expression holding the variable */` |
|      ! 0 |  5346 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      ! 0 |  5347 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5348 | `			return SXERR_ABORT;` |
|      ! 0 |  5349 | `		}else if( rc != SXERR_EMPTY ){` |
|        - |  5350 | `			/* Emit the POP instruction */` |
|      ! 0 |  5351 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 |  5352 | `		}` |
|      ! 0 |  5353 | `		return SXRET_OK;` |
|        - |  5354 | `	}` |
|       13 |  5355 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        - |  5356 | `	/* Make sure we are dealing with a valid statement */` |
|       13 |  5357 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|        8 |  5358 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        3 |  5359 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|        3 |  5360 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5361 | `				return SXERR_ABORT;` |
|        - |  5362 | `			}` |
|        3 |  5363 | `			goto Synchronize;` |
|        - |  5364 | `	}` |
|       10 |  5365 | `	pGen->pIn++;` |
|        - |  5366 | `	/* Extract variable name */` |
|       10 |  5367 | `	pName = &pGen->pIn->sData;` |
|       10 |  5368 | `	pGen->pIn++; /* Jump the var name */` |
|       10 |  5369 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|      ! 0 |  5370 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 |  5371 | `		goto Synchronize;` |
|        - |  5372 | `	}` |
|        - |  5373 | `	/* Initialize the structure describing the static variable */` |
|       10 |  5374 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       10 |  5375 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|        - |  5376 | `	/* Duplicate variable name */` |
|       10 |  5377 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       10 |  5378 | `	if( zDup == 0 ){` |
|      ! 0 |  5379 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  5380 | `		return SXERR_ABORT;` |
|        - |  5381 | `	}` |
|       10 |  5382 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|        - |  5383 | `	/* Check if we have an expression to compile */` |
|       10 |  5384 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|        - |  5385 | `		SySet *pInstrContainer;` |
|        - |  5386 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|        - |  5387 | `		 * Static variable can take any complex expression including function` |
|        - |  5388 | `		 * call as their initialization value.` |
|        - |  5389 | `		 * Example:` |
|        - |  5390 | `		 *		static $var = foo(1,4+5,bar());` |
|        - |  5391 | `		 */` |
|       10 |  5392 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|        - |  5393 | `		/* Swap bytecode container */` |
|       10 |  5394 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       10 |  5395 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|        - |  5396 | `		/* Compile the expression */` |
|       10 |  5397 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  5398 | `		/* Emit the done instruction */` |
|       10 |  5399 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        - |  5400 | `		/* Restore default bytecode container */` |
|       10 |  5401 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        4 |  5402 | `	}` |
|        - |  5403 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       10 |  5404 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       10 |  5405 | `	return SXRET_OK;` |
|        1 |  5406 | `Synchronize:` |
|        - |  5407 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|        - |  5408 | `	 * statement.` |
|        - |  5409 | `	 */` |
|        5 |  5410 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|        3 |  5411 | `		pGen->pIn++;` |
|        1 |  5412 | `	}` |
|        3 |  5413 | `	return SXRET_OK;` |
|        9 |  5414 | `}` |
|        - |  5415 | `/*` |
|        - |  5416 | ` * Compile the var statement.` |
|        - |  5417 | ` * Symisc Extension:` |
|        - |  5418 | ` *      var statement can be used outside of a class definition.` |
|        - |  5419 | ` */` |
|        4 |  5420 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|        1 |  5421 | `{` |
|        - |  5422 | `	sxu32 nLine;` |
|        - |  5423 | `	sxi32 rc;` |
|        5 |  5424 | `	nLine = pGen->pIn->nLine;` |
|        - |  5425 | `	/* Jump the 'var' keyword */` |
|        5 |  5426 | `	pGen->pIn++;` |
|        5 |  5427 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|      ! 0 |  5428 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|        - |  5429 | `		/* Synchronize with the first semi-colon */` |
|      ! 0 |  5430 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|      ! 0 |  5431 | `			pGen->pIn++;` |
|      ! 0 |  5432 | `		}` |
|      ! 0 |  5433 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5434 | `			return SXERR_ABORT;` |
|        - |  5435 | `		}` |
|      ! 0 |  5436 | `	}else{` |
|        - |  5437 | `		/* Compile the expression */` |
|        5 |  5438 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        5 |  5439 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5440 | `			return SXERR_ABORT;` |
|        5 |  5441 | `		}else if( rc != SXERR_EMPTY ){` |
|        5 |  5442 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        2 |  5443 | `		}` |
|        - |  5444 | `	}` |
|        5 |  5445 | `	return SXRET_OK;` |
|        3 |  5446 | `}` |
|        - |  5447 | `/*` |
|        - |  5448 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|        - |  5449 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|        - |  5450 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|        - |  5451 | ` */` |
|        - |  5452 | `/*` |
|        - |  5453 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|        - |  5454 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|        - |  5455 | ` * hash and any shared references), this creates a new literal entry with the` |
|        - |  5456 | ` * qualified name and updates the instruction's operand index.` |
|        - |  5457 | ` *` |
|        - |  5458 | ` * Resolution order:` |
|        - |  5459 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|        - |  5460 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|        - |  5461 | ` *   3. Otherwise return the original literal index unchanged.` |
|        - |  5462 | ` *` |
|        - |  5463 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|        - |  5464 | ` * came from an import (step 1) and 0 otherwise.` |
|        - |  5465 | ` * Returns the (possibly new) literal index.` |
|        - |  5466 | ` */` |
|  2892800 |  5467 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|        5 |  5468 | `{` |
|        - |  5469 | `	ph7_value *pLit;` |
|        - |  5470 | `	const char *zLit;` |
|        - |  5471 | `	SyString sQualified;` |
|        - |  5472 | `	sxu32 nLit;` |
|        - |  5473 | `	sxu32 k;` |
|        - |  5474 | `	sxu32 nNewIdx;` |
|        - |  5475 | `	int hasNsSep;` |
|        - |  5476 | `	SyHashEntry *pImport;` |
|        - |  5477 | `	ph7_value *pNew;` |
|  2892805 |  5478 | `	if( pFromImport ){` |
|  2361535 |  5479 | `		*pFromImport = 0;` |
|  1180765 |  5480 | `	}` |
|  2892805 |  5481 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  2892805 |  5482 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|      ! 0 |  5483 | `		return nOrigIdx;` |
|        - |  5484 | `	}` |
|  2892805 |  5485 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  2892805 |  5486 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|        - |  5487 | `	/* Skip if already qualified (contains backslash) */` |
|  2892805 |  5488 | `	hasNsSep = 0;` |
| 37283611 |  5489 | `	for( k = 0; k < nLit; k++ ){` |
| 34390819 |  5490 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 17195408 |  5491 | `	}` |
|  2892805 |  5492 | `	if( hasNsSep ){` |
|       10 |  5493 | `		return nOrigIdx;` |
|        - |  5494 | `	}` |
|        - |  5495 | `	/* Check use imports first (works even outside namespaces) */` |
|  2892797 |  5496 | `	SyBlobReset(&pGen->sWorker);` |
|  2892797 |  5497 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  2892797 |  5498 | `	if( pImport ){` |
|       41 |  5499 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       41 |  5500 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|       41 |  5501 | `		if( pFromImport ){` |
|       18 |  5502 | `			*pFromImport = 1;` |
|        8 |  5503 | `		}` |
|       23 |  5504 | `	}else{` |
|  2892761 |  5505 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  2892671 |  5506 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|        - |  5507 | `		}` |
|        - |  5508 | `		/* Prepend current namespace */` |
|       95 |  5509 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       95 |  5510 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|       95 |  5511 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|        - |  5512 | `	}` |
|        - |  5513 | `	/* Look up or create a new literal for the qualified name */` |
|      131 |  5514 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|      131 |  5515 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|       57 |  5516 | `		return nNewIdx; /* Already interned */` |
|        - |  5517 | `	}` |
|       79 |  5518 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|       79 |  5519 | `	if( pNew == 0 ){` |
|      ! 0 |  5520 | `		return nOrigIdx; /* OOM, fall back to original */` |
|        - |  5521 | `	}` |
|       79 |  5522 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|       79 |  5523 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|       79 |  5524 | `	return nNewIdx;` |
|  1446405 |  5525 | `}` |
|        - |  5526 | `/*` |
|        - |  5527 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|        - |  5528 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|        - |  5529 | ` */` |
|   187762 |  5530 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 |  5531 | `{` |
|        - |  5532 | `	SyHashEntry *pImport;` |
|        - |  5533 | `	/* Check use imports first */` |
|   187767 |  5534 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   187767 |  5535 | `	if( pImport ){` |
|       19 |  5536 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       19 |  5537 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|       19 |  5538 | `		return;` |
|        - |  5539 | `	}` |
|        - |  5540 | `	/* Prepend current namespace if active */` |
|   187751 |  5541 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        8 |  5542 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        8 |  5543 | `		SyBlobAppend(pOut,"\\",1);` |
|        3 |  5544 | `	}` |
|   187751 |  5545 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    93886 |  5546 | `}` |
|        - |  5547 | `/*` |
|        - |  5548 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|        - |  5549 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|        - |  5550 | ` * The caller must release pOut when done.` |
|        - |  5551 | ` */` |
|   262044 |  5552 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 |  5553 | `{` |
|   262049 |  5554 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     3947 |  5555 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     3947 |  5556 | `		SyBlobAppend(pOut,"\\",1);` |
|     1971 |  5557 | `	}` |
|   262049 |  5558 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   262049 |  5559 | `}` |
|        - |  5560 | `/*` |
|        - |  5561 | ` * Compile a namespace statement` |
|        - |  5562 | ` * According to the PHP language reference manual` |
|        - |  5563 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|        - |  5564 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|        - |  5565 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|        - |  5566 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|        - |  5567 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|        - |  5568 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|        - |  5569 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|        - |  5570 | ` *  programming world.` |
|        - |  5571 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|        - |  5572 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|        - |  5573 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|        - |  5574 | ` *  classes/functions/constants.` |
|        - |  5575 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|        - |  5576 | ` *  readability of source code.` |
|        - |  5577 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|        - |  5578 | ` *  Here is an example of namespace syntax in PHP:` |
|        - |  5579 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|        - |  5580 | ` *       class MyClass {}` |
|        - |  5581 | ` *       function myfunction() {}` |
|        - |  5582 | ` *       const MYCONST = 1;` |
|        - |  5583 | ` *       $a = new MyClass;` |
|        - |  5584 | ` *       $c = new \my\name\MyClass;` |
|        - |  5585 | ` *       $a = strlen('hi');` |
|        - |  5586 | ` *       $d = namespace\MYCONST;` |
|        - |  5587 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|        - |  5588 | ` *       echo constant($d);` |
|        - |  5589 | ` * NOTE` |
|        - |  5590 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - |  5591 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - |  5592 | ` */` |
|        - |  5593 | `/*` |
|        - |  5594 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|        - |  5595 | ` */` |
|       14 |  5596 | `static const char * TokenTypeName(sxu32 nType)` |
|        3 |  5597 | `{` |
|       17 |  5598 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       11 |  5599 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       11 |  5600 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       11 |  5601 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       11 |  5602 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       11 |  5603 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|        3 |  5604 | `	return "token";` |
|       10 |  5605 | `}` |
|     3990 |  5606 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|        5 |  5607 | `{` |
|        - |  5608 | `	sxu32 nLine;` |
|        - |  5609 | `	sxi32 rc;` |
|     3995 |  5610 | `	nLine = pGen->pIn->nLine;` |
|     3995 |  5611 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|        - |  5612 | `	/* Reset namespace and clear previous use imports */` |
|     3995 |  5613 | `	SyBlobReset(&pGen->sNamespace);` |
|     3995 |  5614 | `	SyHashRelease(&pGen->hUseImports);` |
|     3995 |  5615 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     3995 |  5616 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     3995 |  5617 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     3995 |  5618 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     3995 |  5619 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     3995 |  5620 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  5621 | `		/* Global namespace (bare "namespace;") */` |
|      ! 0 |  5622 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 |  5623 | `		return SXRET_OK;` |
|        - |  5624 | `	}` |
|     3995 |  5625 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|        - |  5626 | `		/* namespace; — switch to global namespace */` |
|      ! 0 |  5627 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 |  5628 | `		return SXRET_OK;` |
|        - |  5629 | `	}` |
|     3995 |  5630 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|        - |  5631 | `		/* namespace { } — global namespace block */` |
|      ! 0 |  5632 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|      ! 0 |  5633 | `		return SXRET_OK;` |
|        - |  5634 | `	}` |
|        - |  5635 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     8027 |  5636 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     4037 |  5637 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        - |  5638 | `			/* Append backslash separator */` |
|       26 |  5639 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       26 |  5640 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|       11 |  5641 | `			}` |
|       15 |  5642 | `		}else{` |
|        - |  5643 | `			/* Append identifier */` |
|     4015 |  5644 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - |  5645 | `		}` |
|     4037 |  5646 | `		pGen->pIn++;` |
|        5 |  5647 | `	}` |
|        - |  5648 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|        - |  5649 | `	 * at the correct program counter, not just the last one compiled. */` |
|        - |  5650 | `	{` |
|     3995 |  5651 | `		char *zNsDup = 0;` |
|     3995 |  5652 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     5987 |  5653 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     3988 |  5654 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     1994 |  5655 | `		}` |
|     3995 |  5656 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|        - |  5657 | `	}` |
|     3995 |  5658 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        8 |  5659 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  5660 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|        4 |  5661 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        6 |  5662 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5663 | `			return SXERR_ABORT;` |
|        - |  5664 | `		}` |
|        2 |  5665 | `	}` |
|     3995 |  5666 | `	return SXRET_OK;` |
|     2000 |  5667 | `}` |
|        - |  5668 | `/*` |
|        - |  5669 | ` * Compile the 'use' statement` |
|        - |  5670 | ` * According to the PHP language reference manual` |
|        - |  5671 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|        - |  5672 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|        - |  5673 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|        - |  5674 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|        - |  5675 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|        - |  5676 | ` *  a function or constant is not supported.` |
|        - |  5677 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|        - |  5678 | ` * NOTE` |
|        - |  5679 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - |  5680 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - |  5681 | ` */` |
|       72 |  5682 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|        5 |  5683 | `{` |
|        - |  5684 | `	sxu32 nLine;` |
|        - |  5685 | `	sxi32 rc;` |
|        - |  5686 | `	SyBlob sPath;` |
|        - |  5687 | `	SyString sAlias;` |
|        - |  5688 | `	SyToken *pLast;` |
|        - |  5689 | `	char *zDup;` |
|        - |  5690 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|        - |  5691 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|        - |  5692 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|       77 |  5693 | `	nLine = pGen->pIn->nLine;` |
|       77 |  5694 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|        - |  5695 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|       77 |  5696 | `	iUseType = 0;` |
|       77 |  5697 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       30 |  5698 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|       30 |  5699 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|       16 |  5700 | `			iUseType = 1;` |
|       16 |  5701 | `			pGen->pIn++;` |
|       23 |  5702 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|       16 |  5703 | `			iUseType = 2;` |
|       16 |  5704 | `			pGen->pIn++;` |
|        7 |  5705 | `		}` |
|       14 |  5706 | `	}` |
|        - |  5707 | `	/* Select target hash tables based on import type */` |
|       77 |  5708 | `	switch( iUseType ){` |
|        7 |  5709 | `		case 1:` |
|       16 |  5710 | `			pGenHash = &pGen->hUseFuncImports;` |
|       16 |  5711 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|       16 |  5712 | `			break;` |
|        7 |  5713 | `		case 2:` |
|       16 |  5714 | `			pGenHash = &pGen->hUseConstImports;` |
|       16 |  5715 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|       16 |  5716 | `			break;` |
|       22 |  5717 | `		default:` |
|       49 |  5718 | `			pGenHash = &pGen->hUseImports;` |
|       49 |  5719 | `			pVmHash = &pGen->pVm->hUseImports;` |
|       44 |  5720 | `			break;` |
|        - |  5721 | `	}` |
|       77 |  5722 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|        - |  5723 | `	/* Process one or more use declarations separated by commas */` |
|       37 |  5724 | `	for(;;){` |
|       79 |  5725 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  5726 | `			break;` |
|        - |  5727 | `		}` |
|       79 |  5728 | `		SyBlobReset(&sPath);` |
|       79 |  5729 | `		pLast = 0;` |
|        - |  5730 | `		/* Collect the full namespace path */` |
|      269 |  5731 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|      195 |  5732 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|      135 |  5733 | `				pLast = pGen->pIn;` |
|      135 |  5734 | `				if( SyBlobLength(&sPath) > 0 ){` |
|       65 |  5735 | `					SyBlobAppend(&sPath,"\\",1);` |
|       30 |  5736 | `				}` |
|      135 |  5737 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       65 |  5738 | `			}` |
|      195 |  5739 | `			pGen->pIn++;` |
|        5 |  5740 | `		}` |
|       79 |  5741 | `		if( pLast == 0 ){` |
|        - |  5742 | `			/* Empty path */` |
|        6 |  5743 | `			break;` |
|        - |  5744 | `		}` |
|        - |  5745 | `		/* Default alias is the last component of the path */` |
|       75 |  5746 | `		sAlias = pLast->sData;` |
|        - |  5747 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|       70 |  5748 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       50 |  5749 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|       24 |  5750 | `			pGen->pIn++; /* Jump 'as' */` |
|       24 |  5751 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|       24 |  5752 | `				sAlias = pGen->pIn->sData;` |
|       24 |  5753 | `				pGen->pIn++;` |
|       10 |  5754 | `			}` |
|       10 |  5755 | `		}` |
|        - |  5756 | `		/* Check for duplicate import alias (per-type) */` |
|       75 |  5757 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|        8 |  5758 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  5759 | `				"Cannot use %.*s as %z because the name is already in use",` |
|        4 |  5760 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|        6 |  5761 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  5762 | `				SyBlobRelease(&sPath);` |
|      ! 0 |  5763 | `				return SXERR_ABORT;` |
|        - |  5764 | `			}` |
|        2 |  5765 | `		}` |
|        - |  5766 | `		/* Register the import: alias -> FQN.` |
|        - |  5767 | `		 * Strings are allocated from the VM pool allocator and freed` |
|        - |  5768 | `		 * when the entire VM is released. SyHashRelease does not free` |
|        - |  5769 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      110 |  5770 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       70 |  5771 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|       75 |  5772 | `		if( zDup ){` |
|       75 |  5773 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|       75 |  5774 | `			if( pVmHash ){` |
|        - |  5775 | `				/* Class imports: populate VM table directly (class resolution` |
|        - |  5776 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|       47 |  5777 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       47 |  5778 | `				if( zAliasDup ){` |
|       47 |  5779 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|       21 |  5780 | `				}` |
|       21 |  5781 | `			}` |
|       75 |  5782 | `			if( iUseType == 2 ){` |
|        - |  5783 | `				/* Const imports: emit a runtime instruction so imports are` |
|        - |  5784 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|       16 |  5785 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       16 |  5786 | `				if( zAliasDup ){` |
|        - |  5787 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|        - |  5788 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|        - |  5789 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|       16 |  5790 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|       16 |  5791 | `					if( azPair ){` |
|       16 |  5792 | `						azPair[0] = zAliasDup;` |
|       16 |  5793 | `						azPair[1] = zDup;` |
|       16 |  5794 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|        7 |  5795 | `					}` |
|        7 |  5796 | `				}` |
|        7 |  5797 | `			}` |
|       35 |  5798 | `		}` |
|        - |  5799 | `		/* Check for comma (multiple use declarations) */` |
|       75 |  5800 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        3 |  5801 | `			pGen->pIn++;` |
|        2 |  5802 | `		}else{` |
|       39 |  5803 | `			break;` |
|        - |  5804 | `		}` |
|        1 |  5805 | `	}` |
|       77 |  5806 | `	SyBlobRelease(&sPath);` |
|       77 |  5807 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        4 |  5808 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|        2 |  5809 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        3 |  5810 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5811 | `			return SXERR_ABORT;` |
|        - |  5812 | `		}` |
|        1 |  5813 | `	}` |
|       77 |  5814 | `	return SXRET_OK;` |
|       41 |  5815 | `}` |
|        - |  5816 | `/*` |
|        - |  5817 | ` * Compile the stupid 'declare' language construct.` |
|        - |  5818 | ` *` |
|        - |  5819 | ` * According to the PHP language reference manual.` |
|        - |  5820 | ` *  The declare construct is used to set execution directives for a block of code.` |
|        - |  5821 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|        - |  5822 | ` *  declare (directive)` |
|        - |  5823 | ` *   statement` |
|        - |  5824 | ` * The directive section allows the behavior of the declare block to be set.` |
|        - |  5825 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|        - |  5826 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|        - |  5827 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|        - |  5828 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|        - |  5829 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|        - |  5830 | ` * <?php` |
|        - |  5831 | ` * // these are the same:` |
|        - |  5832 | ` * // you can use this:` |
|        - |  5833 | ` * declare(ticks=1) {` |
|        - |  5834 | ` *   // entire script here` |
|        - |  5835 | ` * }` |
|        - |  5836 | ` * // or you can use this:` |
|        - |  5837 | ` * declare(ticks=1);` |
|        - |  5838 | ` * // entire script here` |
|        - |  5839 | ` * ?>` |
|        - |  5840 | ` *` |
|        - |  5841 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|        - |  5842 | ` */` |
|        - |  5843 | `/*` |
|        - |  5844 | ` * Match a directive name against a known literal (case-insensitive).` |
|        - |  5845 | ` */` |
|       72 |  5846 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|        5 |  5847 | `{` |
|      109 |  5848 | `	return SyStringLength(pName) == nWant` |
|       72 |  5849 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|        5 |  5850 | `}` |
|        - |  5851 |  |
|       42 |  5852 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|        5 |  5853 | `{` |
|       47 |  5854 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       47 |  5855 | `	SyToken *pBodyEnd = 0;` |
|        - |  5856 | `	SyToken *pBodyStart;` |
|        - |  5857 | `	SyToken *pCursor;` |
|        - |  5858 | `	int bHasStrictTypes;` |
|        - |  5859 | `	int bBlockForm;` |
|        - |  5860 | `	int bPlacementOk;` |
|        - |  5861 | `	sxi32 rc;` |
|       47 |  5862 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       47 |  5863 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|        6 |  5864 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|        6 |  5865 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5866 | `			return SXERR_ABORT;` |
|        - |  5867 | `		}` |
|        6 |  5868 | `		goto Synchro;` |
|        - |  5869 | `	}` |
|       43 |  5870 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       43 |  5871 | `	pBodyStart = pGen->pIn;` |
|        - |  5872 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|       43 |  5873 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|       43 |  5874 | `	if( pBodyEnd >= pGen->pEnd ){` |
|      ! 0 |  5875 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|      ! 0 |  5876 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5877 | `			return SXERR_ABORT;` |
|        - |  5878 | `		}` |
|      ! 0 |  5879 | `		return SXRET_OK;` |
|        - |  5880 | `	}` |
|        - |  5881 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|        - |  5882 | `	 * now delimits the comma-separated directive list. */` |
|       43 |  5883 | `	pGen->pIn = &pBodyEnd[1];` |
|       43 |  5884 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      ! 0 |  5885 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|      ! 0 |  5886 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  5887 | `			return SXERR_ABORT;` |
|        - |  5888 | `		}` |
|      ! 0 |  5889 | `	}` |
|       43 |  5890 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|       43 |  5891 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|       43 |  5892 | `	bHasStrictTypes = 0;` |
|        - |  5893 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|        - |  5894 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|        - |  5895 | `	 * directive appears anywhere in the list, before validating values. */` |
|       43 |  5896 | `	pCursor = pBodyStart;` |
|       55 |  5897 | `	while( pCursor < pBodyEnd ){` |
|       51 |  5898 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       43 |  5899 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|       39 |  5900 | `				bHasStrictTypes = 1;` |
|       39 |  5901 | `				break;` |
|        - |  5902 | `			}` |
|        2 |  5903 | `		}` |
|       14 |  5904 | `		pCursor++;` |
|        2 |  5905 | `	}` |
|       43 |  5906 | `	if( bHasStrictTypes && bBlockForm ){` |
|        3 |  5907 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5908 | `			"strict_types declaration must not use block mode");` |
|        3 |  5909 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 |  5910 | `		return SXRET_OK;` |
|        - |  5911 | `	}` |
|       41 |  5912 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|        6 |  5913 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5914 | `			"strict_types declaration must be the very first statement in the script");` |
|        6 |  5915 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        6 |  5916 | `		return SXRET_OK;` |
|        - |  5917 | `	}` |
|        - |  5918 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|       37 |  5919 | `	pCursor = pBodyStart;` |
|       69 |  5920 | `	while( pCursor < pBodyEnd ){` |
|        - |  5921 | `		SyToken *pNameTok;` |
|        - |  5922 | `		SyToken *pEqTok;` |
|        - |  5923 | `		SyToken *pValTok;` |
|        - |  5924 | `		SyString *pDirName;` |
|        - |  5925 | `		int bIsStrict;` |
|        - |  5926 | `		int iStrictValue;` |
|       39 |  5927 | `		pNameTok = pCursor;` |
|       39 |  5928 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  5929 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5930 | `				"declare: Expecting a directive name");` |
|      ! 0 |  5931 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5932 | `			return SXRET_OK;` |
|        - |  5933 | `		}` |
|       39 |  5934 | `		pEqTok = pNameTok + 1;` |
|       39 |  5935 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|      ! 0 |  5936 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5937 | `				"declare: Expecting '=' after directive name");` |
|      ! 0 |  5938 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5939 | `			return SXRET_OK;` |
|        - |  5940 | `		}` |
|       39 |  5941 | `		pValTok = pEqTok + 1;` |
|       39 |  5942 | `		if( pValTok >= pBodyEnd ){` |
|      ! 0 |  5943 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5944 | `				"declare: Expecting value after '='");` |
|      ! 0 |  5945 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5946 | `			return SXRET_OK;` |
|        - |  5947 | `		}` |
|       39 |  5948 | `		pDirName = &pNameTok->sData;` |
|       39 |  5949 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|       39 |  5950 | `		if( bIsStrict ){` |
|        - |  5951 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|        - |  5952 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|       35 |  5953 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      ! 0 |  5954 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5955 | `					"declare(strict_types) value must be a literal");` |
|      ! 0 |  5956 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5957 | `				return SXRET_OK;` |
|        - |  5958 | `			}` |
|       35 |  5959 | `			iStrictValue = -1;` |
|       35 |  5960 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|       35 |  5961 | `				const char *zv = SyStringData(&pValTok->sData);` |
|       35 |  5962 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|       35 |  5963 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|       33 |  5964 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|       15 |  5965 | `			}` |
|       35 |  5966 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|        3 |  5967 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5968 | `					"strict_types declaration must have 0 or 1 as its value");` |
|        3 |  5969 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 |  5970 | `				return SXRET_OK;` |
|        - |  5971 | `			}` |
|       32 |  5972 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|       18 |  5973 | `		}else{` |
|        - |  5974 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|        - |  5975 | `			 * preserve the legacy notice so callers relying on the old` |
|        - |  5976 | `			 * behavior don't regress. */` |
|        8 |  5977 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|        - |  5978 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|        2 |  5979 | `				ph7_lib_version()` |
|        - |  5980 | `				);` |
|        - |  5981 | `		}` |
|       36 |  5982 | `		pCursor = pValTok + 1;` |
|        - |  5983 | `		/* Consume separating comma (or end). */` |
|       36 |  5984 | `		if( pCursor < pBodyEnd ){` |
|        3 |  5985 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 |  5986 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  5987 | `					"declare: Expecting ',' or ')' after directive value");` |
|      ! 0 |  5988 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 |  5989 | `				return SXRET_OK;` |
|        - |  5990 | `			}` |
|        3 |  5991 | `			pCursor++;` |
|        1 |  5992 | `		}` |
|        4 |  5993 | `	}` |
|        - |  5994 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|        - |  5995 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|        - |  5996 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|       34 |  5997 | `	return SXRET_OK;` |
|        2 |  5998 | `Synchro:` |
|        - |  5999 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|       16 |  6000 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       12 |  6001 | `		pGen->pIn++;` |
|        2 |  6002 | `	}` |
|        6 |  6003 | `	return SXRET_OK;` |
|       26 |  6004 | `}` |
|        - |  6005 | `/*` |
|        - |  6006 | ` * Process default argument values. That is,a function may define C++-style default value` |
|        - |  6007 | ` * as follows:` |
|        - |  6008 | ` * function makecoffee($type = "cappuccino")` |
|        - |  6009 | ` * {` |
|        - |  6010 | ` *   return "Making a cup of $type.\n";` |
|        - |  6011 | ` * }` |
|        - |  6012 | ` * Symisc eXtension.` |
|        - |  6013 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|        - |  6014 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|        - |  6015 | ` *      Example: Work only with PH7,generate error under zend` |
|        - |  6016 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|        - |  6017 | ` *      {` |
|        - |  6018 | ` *       var_dump($a);` |
|        - |  6019 | ` *      }` |
|        - |  6020 | ` *     //call test without args` |
|        - |  6021 | ` *      test();` |
|        - |  6022 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|        - |  6023 | ` *      Example:` |
|        - |  6024 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|        - |  6025 | ` * 3 -) Function overloading!!` |
|        - |  6026 | ` *      Example:` |
|        - |  6027 | ` *      function foo($a) {` |
|        - |  6028 | ` *   	  return $a.PHP_EOL;` |
|        - |  6029 | ` *	    }` |
|        - |  6030 | ` *	    function foo($a, $b) {` |
|        - |  6031 | ` *   	  return $a + $b;` |
|        - |  6032 | ` *	    }` |
|        - |  6033 | ` *	    echo foo(5); // Prints "5"` |
|        - |  6034 | ` *	    echo foo(5, 2); // Prints "7"` |
|        - |  6035 | ` *      // Same arg` |
|        - |  6036 | ` *	   function foo(string $a)` |
|        - |  6037 | ` *	   {` |
|        - |  6038 | ` *	     echo "a is a string\n";` |
|        - |  6039 | ` *	     var_dump($a);` |
|        - |  6040 | ` *	   }` |
|        - |  6041 | ` *	  function foo(int $a)` |
|        - |  6042 | ` *	  {` |
|        - |  6043 | ` *	    echo "a is integer\n";` |
|        - |  6044 | ` *	    var_dump($a);` |
|        - |  6045 | ` *	  }` |
|        - |  6046 | ` *	  function foo(array $a)` |
|        - |  6047 | ` *	  {` |
|        - |  6048 | ` * 	    echo "a is an array\n";` |
|        - |  6049 | ` * 	    var_dump($a);` |
|        - |  6050 | ` *	  }` |
|        - |  6051 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|        - |  6052 | ` *	  foo(52); // a is integer [second foo]` |
|        - |  6053 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|        - |  6054 | ` * Please refer to the official documentation for more information on the powerful extension` |
|        - |  6055 | ` * introduced by the PH7 engine.` |
|        - |  6056 | ` */` |
|   240966 |  6057 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|        5 |  6058 | `{` |
|        - |  6059 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |  6060 | `	SySet *pInstrContainer;` |
|        - |  6061 | `	sxi32 rc;` |
|        - |  6062 | `	/* Swap token stream */` |
|   240971 |  6063 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   240971 |  6064 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   240971 |  6065 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|        - |  6066 | `	/* Compile the expression holding the argument value */` |
|   240971 |  6067 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - |  6068 | `	/* Emit the done instruction */` |
|   240971 |  6069 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   240971 |  6070 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   240971 |  6071 | `	RE_SWAP_DELIMITER(pGen);` |
|   240971 |  6072 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  6073 | `		return SXERR_ABORT;` |
|        - |  6074 | `	}` |
|   240971 |  6075 | `	return SXRET_OK;` |
|   120488 |  6076 | `}` |
|        - |  6077 | `/*` |
|        - |  6078 | ` * Collect function arguments one after one.` |
|        - |  6079 | ` * According to the PHP language reference manual.` |
|        - |  6080 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|        - |  6081 | ` * list of expressions.` |
|        - |  6082 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|        - |  6083 | ` * and default argument values. Variable-length argument lists are also supported,` |
|        - |  6084 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|        - |  6085 | ` * for more information.` |
|        - |  6086 | ` * Example #1 Passing arrays to functions` |
|        - |  6087 | ` * <?php` |
|        - |  6088 | ` * function takes_array($input)` |
|        - |  6089 | ` * {` |
|        - |  6090 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|        - |  6091 | ` * }` |
|        - |  6092 | ` * ?>` |
|        - |  6093 | ` * Making arguments be passed by reference` |
|        - |  6094 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|        - |  6095 | ` * within the function is changed, it does not get changed outside of the function).` |
|        - |  6096 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|        - |  6097 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|        - |  6098 | ` * to the argument name in the function definition:` |
|        - |  6099 | ` * Example #2 Passing function parameters by reference` |
|        - |  6100 | ` * <?php` |
|        - |  6101 | ` * function add_some_extra(&$string)` |
|        - |  6102 | ` * {` |
|        - |  6103 | ` *   $string .= 'and something extra.';` |
|        - |  6104 | ` * }` |
|        - |  6105 | ` * $str = 'This is a string, ';` |
|        - |  6106 | ` * add_some_extra($str);` |
|        - |  6107 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|        - |  6108 | ` * ?>` |
|        - |  6109 | ` *` |
|        - |  6110 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|        - |  6111 | ` * complex agrument values.Please refer to the official documentation for more information` |
|        - |  6112 | ` * on these extension.` |
|        - |  6113 | ` */` |
|   491234 |  6114 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|        5 |  6115 | `{` |
|        - |  6116 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|        - |  6117 | `	SyToken *pIn;  /* Token stream */` |
|        - |  6118 | `	SyBlob sSig;         /* Function signature */` |
|        - |  6119 | `	char *zDup;          /* Copy of argument name */` |
|        - |  6120 | `	sxi32 rc;` |
|        - |  6121 |  |
|   491239 |  6122 | `	pIn = pGen->pIn;` |
|   491239 |  6123 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|        - |  6124 | `	/* Process arguments one after one */` |
|   604328 |  6125 | `	for(;;){` |
|  1208661 |  6126 | `		if( pIn >= pEnd ){` |
|        - |  6127 | `			/* No more arguments to process */` |
|   491223 |  6128 | `			break;` |
|        - |  6129 | `		}` |
|   717443 |  6130 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   717443 |  6131 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   717443 |  6132 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   717443 |  6133 | `		SySetInit(&sArg.aAttrs,&pGen->pVm->sAllocator,sizeof(ph7_attribute));` |
|   717443 |  6134 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|        - |  6135 | `		/* Parameter #[...] attributes: the group precedes the parameter's` |
|        - |  6136 | `		 * first token inside the main token stream */` |
|   717443 |  6137 | `		if( GenStateCollectParamAttrs(&(*pGen),pIn,&sArg.aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  6138 | `			return SXERR_ABORT;` |
|        - |  6139 | `		}` |
|        - |  6140 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|        - |  6141 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|        - |  6142 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|        - |  6143 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|        - |  6144 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|        - |  6145 | `		{` |
|   717443 |  6146 | `			int bReadonly = 0, bVisSeen = 0;` |
|   717443 |  6147 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|   717443 |  6148 | `			sxi32 iSetVisFlag = 0;` |
|        - |  6149 | `			int nSetTok;` |
|        - |  6150 | `			sxi32 nSetVis;` |
|   717443 |  6151 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|        3 |  6152 | `				bReadonly = 1;` |
|        3 |  6153 | `				pIn++;` |
|        1 |  6154 | `			}` |
|   717443 |  6155 | `			nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|   717443 |  6156 | `			if( nSetVis ){` |
|        - |  6157 | ``				/* Leading `private(set)` etc: promoted with a public read side */`` |
|        3 |  6158 | `				iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|        3 |  6159 | `				bVisSeen = 1;` |
|        3 |  6160 | `				pIn += nSetTok;` |
|        3 |  6161 | `				if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|      ! 0 |  6162 | `					bReadonly = 1;` |
|      ! 0 |  6163 | `					pIn++;` |
|        1 |  6164 | `				}` |
|   717442 |  6165 | `			}else if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|    81945 |  6166 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|    81945 |  6167 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|       89 |  6168 | `					bVisSeen = 1;` |
|       89 |  6169 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|      120 |  6170 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|       39 |  6171 | `						: PH7_CLASS_PROT_PUBLIC;` |
|       89 |  6172 | `					pIn++;` |
|       89 |  6173 | `					nSetVis = GenStatePeekSetVisibility(pIn,pEnd,&nSetTok);` |
|       89 |  6174 | `					if( nSetVis ){` |
|        - |  6175 | ``						/* `public private(set) T $x` promoted form */`` |
|        3 |  6176 | `						iSetVisFlag = GenStateSetVisFlag(nSetVis);` |
|        3 |  6177 | `						pIn += nSetTok;` |
|        1 |  6178 | `					}` |
|       89 |  6179 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       18 |  6180 | `						bReadonly = 1;` |
|       18 |  6181 | `						pIn++;` |
|        7 |  6182 | `					}` |
|       42 |  6183 | `				}` |
|    40970 |  6184 | `			}` |
|   717443 |  6185 | `			if( iSetVisFlag == PH7_CLASS_ATTR_PRIVATE_SET ){` |
|        5 |  6186 | `				sArg.iFlags \|= VM_FUNC_ARG_PRIV_SET;` |
|   717441 |  6187 | `			}else if( iSetVisFlag == PH7_CLASS_ATTR_PROTECTED_SET ){` |
|      ! 0 |  6188 | `				sArg.iFlags \|= VM_FUNC_ARG_PROT_SET;` |
|      ! 0 |  6189 | `			}` |
|   717443 |  6190 | `			if( bVisSeen \|\| bReadonly ){` |
|       93 |  6191 | `				if( !bCtorCtx ){` |
|        6 |  6192 | `					if( bAbstractCtx ){` |
|        3 |  6193 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|        - |  6194 | `							"Cannot declare promoted property in an abstract constructor");` |
|        2 |  6195 | `					}else{` |
|        3 |  6196 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|        - |  6197 | `							"Cannot declare promoted property outside a constructor");` |
|        - |  6198 | `					}` |
|        6 |  6199 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  6200 | `						return SXERR_ABORT;` |
|        - |  6201 | `					}` |
|        6 |  6202 | `					return SXERR_SYNTAX;` |
|        - |  6203 | `				}` |
|       89 |  6204 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|       89 |  6205 | `				sArg.iPromoteVis = iVis;` |
|       89 |  6206 | `				if( bReadonly ){` |
|       20 |  6207 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|        8 |  6208 | `				}` |
|       42 |  6209 | `			}` |
|        - |  6210 | `		}` |
|        - |  6211 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|   717434 |  6212 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   419223 |  6213 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   119059 |  6214 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|    97599 |  6215 | `			sxu32 nLineLocal = pIn->nLine;` |
|    97599 |  6216 | `			sxi32 iTFlags = 0;` |
|    97599 |  6217 | `			pGen->pIn = pIn;` |
|    97599 |  6218 | `			rc = GenStateParseUnionTypeDecl(` |
|    48797 |  6219 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|    48797 |  6220 | `				&iTFlags, &sArg.sTypeName,` |
|        - |  6221 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|        - |  6222 | `				/* bAllowVoid */ 0,` |
|    48797 |  6223 | `						nLineLocal);` |
|    97599 |  6224 | `			pIn = pGen->pIn;` |
|    97599 |  6225 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  6226 | `				return SXERR_ABORT;` |
|    97599 |  6227 | `			}else if( rc == SXERR_CORRUPT ){` |
|        - |  6228 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|        3 |  6229 | `				return SXERR_SYNTAX;` |
|    97597 |  6230 | `			}else if( rc == SXERR_SYNTAX ){` |
|       12 |  6231 | `				if( pIn < pEnd ){` |
|       16 |  6232 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|        - |  6233 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|        4 |  6234 | `						&pIn->sData);` |
|        8 |  6235 | `				}else{` |
|      ! 0 |  6236 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|        - |  6237 | `						"syntax error, unexpected end of file");` |
|        - |  6238 | `				}` |
|       12 |  6239 | `				return SXERR_SYNTAX;` |
|        - |  6240 | `			}` |
|    97589 |  6241 | `			sArg.iFlags \|= iTFlags;` |
|    48792 |  6242 | `		}` |
|   717429 |  6243 | `		if( pIn >= pEnd ){` |
|      ! 0 |  6244 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|      ! 0 |  6245 | `			return rc;` |
|        - |  6246 | `		}` |
|   717429 |  6247 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|        - |  6248 | `			/* Pass by reference,record that */` |
|     3929 |  6249 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|     3929 |  6250 | `			pIn++;` |
|     1962 |  6251 | `		}` |
|   717429 |  6252 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|        - |  6253 | `			/* Variadic parameter: ...$args */` |
|    19529 |  6254 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|    19529 |  6255 | `			pIn++;` |
|     9762 |  6256 | `		}` |
|   717429 |  6257 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  6258 | `			/* Invalid argument */` |
|      ! 0 |  6259 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|      ! 0 |  6260 | `			return rc;` |
|        - |  6261 | `		}` |
|   717429 |  6262 | `		pIn++; /* Jump the dollar sign */` |
|        - |  6263 | `		/* Copy argument name */` |
|   717429 |  6264 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   717429 |  6265 | `		if( zDup == 0 ){` |
|      ! 0 |  6266 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|      ! 0 |  6267 | `			return SXERR_ABORT;` |
|        - |  6268 | `		}` |
|   717429 |  6269 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   717429 |  6270 | `		pIn++;` |
|   717429 |  6271 | `		if( pIn < pEnd ){` |
|   373917 |  6272 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|        - |  6273 | `				SyToken *pDefend;` |
|   240973 |  6274 | `				sxi32 iNest = 0;` |
|   240973 |  6275 | `				pIn++; /* Jump the equal sign */` |
|   240973 |  6276 | `				pDefend = pIn;` |
|        - |  6277 | `				/* Process the default value associated with this argument */` |
|   513039 |  6278 | `				while( pDefend < pEnd ){` |
|   365337 |  6279 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|    93271 |  6280 | `						break;` |
|        - |  6281 | `					}` |
|   272071 |  6282 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|        - |  6283 | `						/* Increment nesting level */` |
|    15549 |  6284 | `						iNest++;` |
|   264299 |  6285 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|        - |  6286 | `						/* Decrement nesting level */` |
|    15549 |  6287 | `						iNest--;` |
|     7772 |  6288 | `					}` |
|   272071 |  6289 | `					pDefend++;` |
|        5 |  6290 | `				}` |
|   240973 |  6291 | `				if( pIn >= pDefend ){` |
|        3 |  6292 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|        3 |  6293 | `					return rc;` |
|        - |  6294 | `				}` |
|        - |  6295 | `				/* Process default value */` |
|   240971 |  6296 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   240971 |  6297 | `				if( rc != SXRET_OK ){` |
|      ! 0 |  6298 | `					return rc;` |
|        - |  6299 | `				}` |
|        - |  6300 | `` 				/* PHP rule: a typed parameter whose default is the literal `null` `` |
|        - |  6301 | ``				 * (`C $c = null`, `int $x = null`, `A\|B $x = null`) is implicitly`` |
|        - |  6302 | `				 * nullable — an explicit null is accepted even though the type isn't` |
|        - |  6303 | ``				 * written `?T`. Detect the single-token `null` default here so the VM`` |
|        - |  6304 | `				 * arg-type check lets null through. */` |
|   240966 |  6305 | `				if( (sArg.nType > 0 \|\| (sArg.iFlags & VM_FUNC_ARG_UNION))` |
|   145752 |  6306 | `					&& (sArg.iFlags & VM_FUNC_ARG_NULLABLE) == 0` |
|   145749 |  6307 | `					&& &pIn[1] == pDefend` |
|    46647 |  6308 | `					&& pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)` |
|    34978 |  6309 | `					&& pIn->sData.nByte == sizeof("null")-1` |
|    21373 |  6310 | `					&& SyStrnicmp(SyStringData(&pIn->sData),"null",sizeof("null")-1) == 0 ){` |
|    15547 |  6311 | `					sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|     7771 |  6312 | `				}` |
|        - |  6313 | `				/* Point beyond the default value */` |
|   240971 |  6314 | `				pIn = pDefend;` |
|   120483 |  6315 | `			}` |
|   373915 |  6316 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 |  6317 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|      ! 0 |  6318 | `				return rc;` |
|        - |  6319 | `			}` |
|   373915 |  6320 | `			pIn++; /* Jump the trailing comma */` |
|   186955 |  6321 | `		}` |
|        - |  6322 | `		/* Append argument signature */` |
|   717427 |  6323 | `		if( sArg.nType > 0 ){` |
|    97527 |  6324 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|        - |  6325 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    15621 |  6326 | `				int marker = 'o';` |
|    15621 |  6327 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    15621 |  6328 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|     7813 |  6329 | `			}else{` |
|        - |  6330 | `				int c;` |
|    81911 |  6331 | `				c = 'n'; /* cc warning */` |
|        - |  6332 | `				/* Type leading character */` |
|    81911 |  6333 | `				switch(sArg.nType){` |
|     5832 |  6334 | `				case MEMOBJ_HASHMAP:` |
|        - |  6335 | `					/* Hashmap aka 'array' */` |
|    11669 |  6336 | `					c = 'h';` |
|    11669 |  6337 | `					break;` |
|     9824 |  6338 | `				case MEMOBJ_INT:` |
|        - |  6339 | `					/* Integer */` |
|    19653 |  6340 | `					c = 'i';` |
|    19653 |  6341 | `					break;` |
|        2 |  6342 | `				case MEMOBJ_BOOL:` |
|        - |  6343 | `					/* Bool */` |
|        5 |  6344 | `					c = 'b';` |
|        5 |  6345 | `					break;` |
|        5 |  6346 | `				case MEMOBJ_REAL:` |
|        - |  6347 | `					/* Float */` |
|       12 |  6348 | `					c = 'f';` |
|       12 |  6349 | `					break;` |
|    25282 |  6350 | `				case MEMOBJ_STRING:` |
|        - |  6351 | `					/* String */` |
|    50569 |  6352 | `					c = 's';` |
|    50569 |  6353 | `					break;` |
|        7 |  6354 | `				case MEMOBJ_OBJ:` |
|        - |  6355 | `					/* Object */` |
|       16 |  6356 | `					c = 'o';` |
|       14 |  6357 | `					break;` |
|        1 |  6358 | `				default:` |
|        2 |  6359 | `					break;` |
|        - |  6360 | `				}` |
|    81911 |  6361 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|        - |  6362 | `			}` |
|    48766 |  6363 | `		}else{` |
|        - |  6364 | `			/* No type is associated with this parameter which mean` |
|        - |  6365 | `			 * that this function is not condidate for overloading.` |
|        - |  6366 | `			 */` |
|   619905 |  6367 | `			SyBlobRelease(&sSig);` |
|        - |  6368 | `		}` |
|        - |  6369 | `		/* Save in the argument set */` |
|   717427 |  6370 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|        5 |  6371 | `	}` |
|   491223 |  6372 | `	if( SyBlobLength(&sSig) > 0 ){` |
|        - |  6373 | `		/* Save function signature */` |
|    66383 |  6374 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|    33189 |  6375 | `	}` |
|   491223 |  6376 | `	return SXRET_OK;` |
|   245622 |  6377 | `}` |
|        - |  6378 | `/*` |
|        - |  6379 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|        - |  6380 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|        - |  6381 | ` * the enclosing function. Returns the token just past the nested construct.` |
|        - |  6382 | ` */` |
|    34998 |  6383 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|        5 |  6384 | `{` |
|    35003 |  6385 | `	sxi32 iParen = 0;` |
|    35003 |  6386 | `	pIn++; /* past 'function'/'fn' */` |
|        - |  6387 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|        - |  6388 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|        - |  6389 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|   155593 |  6390 | `	while( pIn < pEnd ){` |
|   155593 |  6391 | `		sxu32 t = pIn->nType;` |
|   155593 |  6392 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|   151655 |  6393 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|   104993 |  6394 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|    85531 |  6395 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|   120595 |  6396 | `		pIn++;` |
|        5 |  6397 | `	}` |
|    19467 |  6398 | `	if( pIn >= pEnd ){ return pIn; }` |
|        - |  6399 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|        - |  6400 | `	{` |
|    19467 |  6401 | `		sxi32 d = 0;` |
|   773341 |  6402 | `		while( pIn < pEnd ){` |
|   773341 |  6403 | `			sxu32 t = pIn->nType;` |
|   773341 |  6404 | `			if( t & PH7_TK_OCB ){ d++; }` |
|   742223 |  6405 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|   753879 |  6406 | `			pIn++;` |
|        5 |  6407 | `		}` |
|        - |  6408 | `	}` |
|    19467 |  6409 | `	return pIn;` |
|    17504 |  6410 | `}` |
|        - |  6411 | `/*` |
|        - |  6412 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|        - |  6413 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|        - |  6414 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|        - |  6415 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|        - |  6416 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|        - |  6417 | ` * detached-mini-program path untouched.` |
|        - |  6418 | ` */` |
|        - |  6419 | `/*` |
|        - |  6420 | ` * Case-insensitive match of a (possibly '\'-prefixed) name against the` |
|        - |  6421 | ` * Generator-supertype whitelist: Generator, Iterator, Traversable, iterable,` |
|        - |  6422 | ` * mixed, object.` |
|        - |  6423 | ` */` |
|       28 |  6424 | `static int GenStateGenRetNameOk(const char *zName,sxu32 nName)` |
|        3 |  6425 | `{` |
|        - |  6426 | `	static const struct { const char *zName; sxu32 nLen; } aOk[] = {` |
|        - |  6427 | `		{"Generator",9},{"Iterator",8},{"Traversable",11},` |
|        - |  6428 | `		{"iterable",8},{"mixed",5},{"object",6}` |
|        - |  6429 | `	};` |
|        - |  6430 | `	sxu32 i;` |
|       31 |  6431 | `	if( nName > 0 && zName[0] == '\\' ){` |
|      ! 0 |  6432 | `		zName++;` |
|      ! 0 |  6433 | `		nName--;` |
|      ! 0 |  6434 | `	}` |
|       63 |  6435 | `	for( i = 0; i < SX_ARRAYSIZE(aOk); i++ ){` |
|       59 |  6436 | `		if( nName == aOk[i].nLen && SyStrnicmp(zName,aOk[i].zName,nName) == 0 ){` |
|       27 |  6437 | `			return 1;` |
|        - |  6438 | `		}` |
|       17 |  6439 | `	}` |
|        5 |  6440 | `	return 0;` |
|       17 |  6441 | `}` |
|        - |  6442 | `/*` |
|        - |  6443 | ` * One atom of a generator's declared return type: is it a supertype of` |
|        - |  6444 | ` * Generator? php 8 accepts Generator, Iterator, Traversable, iterable,` |
|        - |  6445 | ` * mixed and object (nullability is irrelevant — it only widens). A class` |
|        - |  6446 | ` * atom is accepted when its raw name matches OR its use-import/namespace` |
|        - |  6447 | `` * resolution (GenStateResolveName) matches — so `use Generator as Gen;`` |
|        - |  6448 | `` * function g(): Gen` compiles like php. Raw-first is deliberately LENIENT:`` |
|        - |  6449 | `` * the parser strips a leading `\`, so inside `namespace Foo;` a`` |
|        - |  6450 | ``  * fully-qualified `\Generator` (php: accept) and a bare `Generator` `` |
|        - |  6451 | ` * (php: reject as Foo\Generator) are indistinguishable here — we accept` |
|        - |  6452 | ` * both rather than fatal on valid code (a recorded divergence).` |
|        - |  6453 | ` */` |
|       26 |  6454 | `static int GenStateGenRetAtomOk(ph7_gen_state *pGen,sxu32 nType,const SyString *pName)` |
|        4 |  6455 | `{` |
|       30 |  6456 | `	if( nType == MEMOBJ_OBJ ){` |
|      ! 0 |  6457 | ``		return 1; /* bare `object` */`` |
|        - |  6458 | `	}` |
|       30 |  6459 | `	if( nType != SXU32_HIGH ){` |
|        3 |  6460 | `		return 0; /* scalar/array/void/never/null/... */` |
|        - |  6461 | `	}` |
|       27 |  6462 | `	if( GenStateGenRetNameOk(pName->zString,pName->nByte) ){` |
|       23 |  6463 | `		return 1;` |
|        - |  6464 | `	}` |
|        - |  6465 | `	/* Not a whitelist name as written — try the compile-time resolution` |
|        - |  6466 | ``	 * (use-import aliases; namespace prefix). `use Iterator as It;` must`` |
|        - |  6467 | ``	 * compile; a userland `MyIter` resolves to [Ns\]MyIter and still fails,`` |
|        - |  6468 | `	 * matching php (a subinterface is not a SUPERtype of Generator). */` |
|        - |  6469 | `	{` |
|        - |  6470 | `		SyBlob sFQN;` |
|        - |  6471 | `		int bOk;` |
|        5 |  6472 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        5 |  6473 | `		GenStateResolveName(pGen,pName,&sFQN);` |
|        5 |  6474 | `		bOk = GenStateGenRetNameOk((const char *)SyBlobData(&sFQN),(sxu32)SyBlobLength(&sFQN));` |
|        5 |  6475 | `		SyBlobRelease(&sFQN);` |
|        5 |  6476 | `		return bOk;` |
|        - |  6477 | `	}` |
|       17 |  6478 | `}` |
|        - |  6479 | `/*` |
|        - |  6480 | ` * php 8: a generator function may only declare a return type that is a` |
|        - |  6481 | ` * supertype of Generator, alone or as a union alternative; an intersection` |
|        - |  6482 | ` * group qualifies only if every member does. Anything else is php's exact` |
|        - |  6483 | ` * compile-time fatal "Generator return type must be a supertype of` |
|        - |  6484 | ` * Generator, %s given" (byte-matched vs php 8.5.7; the type text is the` |
|        - |  6485 | ` * canonical-order sReturnTypeName). Without this check the declared type` |
|        - |  6486 | ` * used to leak into the BODY's completion OP_DONE via the ctx resume paths` |
|        - |  6487 | ` * and threw a spurious runtime TypeError instead (see VmStartCtx/VmResumeCtx).` |
|        - |  6488 | ` */` |
|      264 |  6489 | `static sxi32 GenStateValidateGeneratorReturnType(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|        5 |  6490 | `{` |
|      269 |  6491 | `	int bOk = 0;` |
|        - |  6492 | `	sxu32 nLine;` |
|        - |  6493 | `	sxi32 rc;` |
|      269 |  6494 | `	if( pFunc->nReturnType < 1 && SySetUsed(&pFunc->aReturnUnion) < 1 ){` |
|      243 |  6495 | `		return SXRET_OK; /* untyped: nothing to validate */` |
|        - |  6496 | `	}` |
|       30 |  6497 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|      ! 0 |  6498 | `		ph7_type_alt *aAlt = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  6499 | `		sxu32 n = SySetUsed(&pFunc->aReturnUnion);` |
|        - |  6500 | `		sxu32 i,j;` |
|      ! 0 |  6501 | `		for( i = 0; i < n && !bOk; i++ ){` |
|        - |  6502 | `			int bGroupOk;` |
|      ! 0 |  6503 | `			if( i > 0 && aAlt[i].nGroup == aAlt[i-1].nGroup ){` |
|      ! 0 |  6504 | `				continue; /* group already judged at its first member (ids are contiguous) */` |
|        - |  6505 | `			}` |
|      ! 0 |  6506 | `			bGroupOk = 1;` |
|      ! 0 |  6507 | `			for( j = i; j < n && aAlt[j].nGroup == aAlt[i].nGroup; j++ ){` |
|      ! 0 |  6508 | `				if( !GenStateGenRetAtomOk(&(*pGen),aAlt[j].nType,&aAlt[j].sClass) ){` |
|      ! 0 |  6509 | `					bGroupOk = 0;` |
|      ! 0 |  6510 | `					break;` |
|        - |  6511 | `				}` |
|      ! 0 |  6512 | `			}` |
|      ! 0 |  6513 | `			bOk = bGroupOk;` |
|      ! 0 |  6514 | `		}` |
|      ! 0 |  6515 | `	}else{` |
|       30 |  6516 | `		bOk = GenStateGenRetAtomOk(&(*pGen),pFunc->nReturnType,&pFunc->sReturnClass);` |
|        - |  6517 | `	}` |
|       30 |  6518 | `	if( bOk ){` |
|       27 |  6519 | `		return SXRET_OK;` |
|        - |  6520 | `	}` |
|        - |  6521 | `	/* This validator runs at the end of GenStateCompileFuncBody, after the` |
|        - |  6522 | `	 * body's tokens (>= the '{...}') were consumed, so pIn[-1] is always a` |
|        - |  6523 | `	 * token of this stream — its line is the function's closing brace. php` |
|        - |  6524 | `	 * reports the SIGNATURE line instead; the drift is the §3.7 error-` |
|        - |  6525 | `	 * fidelity class (recorded), pending a decl-line field on ph7_vm_func. */` |
|        3 |  6526 | `	nLine = pGen->pIn[-1].nLine;` |
|        - |  6527 | `	{` |
|        3 |  6528 | `		SyString sGiven = pFunc->sReturnTypeName;` |
|        3 |  6529 | `		if( sGiven.nByte < 1 ){` |
|      ! 0 |  6530 | `			sGiven = pFunc->sReturnClass;` |
|      ! 0 |  6531 | `		}` |
|        3 |  6532 | `		if( sGiven.nByte < 1 ){` |
|        - |  6533 | ``			/* `void`/`never`: GenBuildUnionTypeText omits their atoms from the`` |
|        - |  6534 | `			 * rendered type text, so sReturnTypeName arrives empty for them —` |
|        - |  6535 | `			 * name them here (the root fix belongs to that renderer, §3.7). */` |
|      ! 0 |  6536 | `			const char *zScalar =` |
|      ! 0 |  6537 | `				pFunc->nReturnType == MEMOBJ_VOID  ? "void"  :` |
|      ! 0 |  6538 | `				pFunc->nReturnType == MEMOBJ_NEVER ? "never" : "?";` |
|      ! 0 |  6539 | `			SyStringInitFromBuf(&sGiven,zScalar,SyStrlen(zScalar));` |
|      ! 0 |  6540 | `		}` |
|        3 |  6541 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  6542 | `			"Generator return type must be a supertype of Generator, %z given",&sGiven);` |
|        - |  6543 | `	}` |
|        3 |  6544 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|      137 |  6545 | `}` |
|  1413258 |  6546 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|        5 |  6547 | `{` |
|  1413263 |  6548 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|  1413263 |  6549 | `	SyToken *pEnd = pGen->pEnd;` |
|  1413263 |  6550 | `	sxi32 iDepth = 0;` |
|  1413263 |  6551 | `	int bStarted = 0;` |
| 63545365 |  6552 | `	while( pIn < pEnd ){` |
| 63545365 |  6553 | `		sxu32 t = pIn->nType;` |
| 63545365 |  6554 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 60565899 |  6555 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 57586811 |  6556 | `		if( t & PH7_TK_KEYWORD ){` |
|  4665919 |  6557 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|  4665919 |  6558 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|  4665655 |  6559 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|        - |  6560 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|  2315326 |  6561 | `		}` |
| 57551549 |  6562 | `		pIn++;` |
|        5 |  6563 | `	}` |
|  1412999 |  6564 | `	return FALSE;` |
|   706634 |  6565 | `}` |
|        - |  6566 | `/*` |
|        - |  6567 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|        - |  6568 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|        - |  6569 | ` * and this routine takes care of generating the appropriate error message.` |
|        - |  6570 | ` */` |
|  1413258 |  6571 | `static sxi32 GenStateCompileFuncBody(` |
|        - |  6572 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - |  6573 | `	ph7_vm_func *pFunc    /* Function state */` |
|        - |  6574 | `	)` |
|        5 |  6575 | `{` |
|        - |  6576 | `	SySet *pInstrContainer; /* Instruction container */` |
|        - |  6577 | `	GenBlock *pBlock;` |
|        - |  6578 | `	sxu32 nGotoOfft;` |
|        - |  6579 | `	sxi32 rc;` |
|        - |  6580 | `	/* Attach the new function */` |
|  1413263 |  6581 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  1413263 |  6582 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  6583 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|        - |  6584 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  6585 | `		return SXERR_ABORT;` |
|        - |  6586 | `	}` |
|  1413263 |  6587 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|        - |  6588 | `	/* Swap bytecode containers */` |
|  1413263 |  6589 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  1413263 |  6590 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|        - |  6591 | `	/* Emit constructor property promotion prologue:` |
|        - |  6592 | `	 *   $this->NAME = $NAME;` |
|        - |  6593 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|        - |  6594 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|        - |  6595 | `	{` |
|  1413263 |  6596 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|        - |  6597 | `		sxu32 i;` |
|  2099457 |  6598 | `		for( i = 0; i < nArg; i++ ){` |
|   686199 |  6599 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|        - |  6600 | `			char *zSrc;` |
|        - |  6601 | `			sxu32 nSrc,nName;` |
|        - |  6602 | `			SySet sToken;` |
|        - |  6603 | `			SyToken *pTmpIn,*pTmpEnd;` |
|        - |  6604 | `			sxi32 rcPromote;` |
|   686199 |  6605 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   686125 |  6606 | `				continue;` |
|        - |  6607 | `			}` |
|        - |  6608 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|        - |  6609 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|        - |  6610 | `			 * copied), so it must outlive the function — never free it. The` |
|        - |  6611 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|        - |  6612 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|       79 |  6613 | `			nName = SyStringLength(&pArg->sName);` |
|       79 |  6614 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|       79 |  6615 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|       79 |  6616 | `			if( zSrc == 0 ){` |
|      ! 0 |  6617 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  6618 | `				GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  6619 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  6620 | `				return SXERR_ABORT;` |
|        - |  6621 | `			}` |
|        - |  6622 | `			{` |
|       79 |  6623 | `				char *z = zSrc;` |
|       79 |  6624 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|       79 |  6625 | `				z += sizeof("$this->")-1;` |
|       79 |  6626 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|       79 |  6627 | `				z += nName;` |
|       79 |  6628 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|       79 |  6629 | `				z += sizeof(" = $")-1;` |
|       79 |  6630 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|       79 |  6631 | `				z += nName;` |
|       79 |  6632 | `				*z = 0;` |
|        - |  6633 | `			}` |
|       79 |  6634 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       79 |  6635 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken,0);` |
|       79 |  6636 | `			pTmpIn = pGen->pIn;` |
|       79 |  6637 | `			pTmpEnd = pGen->pEnd;` |
|       79 |  6638 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|       79 |  6639 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       79 |  6640 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|       79 |  6641 | `			pGen->pIn = pTmpIn;` |
|       79 |  6642 | `			pGen->pEnd = pTmpEnd;` |
|       79 |  6643 | `			SySetRelease(&sToken);` |
|       79 |  6644 | `			if( rcPromote == SXERR_ABORT ){` |
|      ! 0 |  6645 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  6646 | `				GenStateLeaveBlock(&(*pGen),0);` |
|      ! 0 |  6647 | `				return SXERR_ABORT;` |
|        - |  6648 | `			}` |
|        - |  6649 | `			/* Discard the assignment result — this is a statement expression. */` |
|       79 |  6650 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       42 |  6651 | `		}` |
|        - |  6652 | `	}` |
|        - |  6653 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|        - |  6654 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|        - |  6655 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|        - |  6656 | `	 * generator — and vice versa — is classified independently. */` |
|        - |  6657 | `	{` |
|  1413263 |  6658 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|  1413263 |  6659 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|        - |  6660 | `		/* Compile the body */` |
|  1413263 |  6661 | `		PH7_CompileBlock(&(*pGen),0);` |
|  1413263 |  6662 | `		pGen->bInGenerator = bSavedGen;` |
|        - |  6663 | `	}` |
|        - |  6664 | `	/* Fix exception jumps now the destination is resolved */` |
|  1413263 |  6665 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        - |  6666 | `	/* Emit the final return if not yet done */` |
|  1413263 |  6667 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - |  6668 | `	/* Fix gotos jumps now the destination is resolved */` |
|  1413263 |  6669 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|      ! 0 |  6670 | `		rc = SXERR_ABORT;` |
|      ! 0 |  6671 | `	}` |
|  1413263 |  6672 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|        - |  6673 | `	/* Restore the default container */` |
|  1413263 |  6674 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - |  6675 | `	/* Leave function block */` |
|  1413263 |  6676 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  1413263 |  6677 | `	if( rc == SXERR_ABORT ){` |
|        - |  6678 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  6679 | `		return SXERR_ABORT;` |
|        - |  6680 | `	}` |
|        - |  6681 | `	/* Scan for yield opcodes to detect generator functions */` |
|        - |  6682 | `	{` |
|  1413263 |  6683 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|        - |  6684 | `		sxu32 i;` |
| 38620327 |  6685 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 37207333 |  6686 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|      269 |  6687 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      269 |  6688 | `				break;` |
|        - |  6689 | `			}` |
| 18603537 |  6690 | `		}` |
|        - |  6691 | `	}` |
|  1413263 |  6692 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  6693 | `		/* php-exact definition-time check; see the helper's block comment. */` |
|      269 |  6694 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|      ! 0 |  6695 | `			return SXERR_ABORT;` |
|        - |  6696 | `		}` |
|      132 |  6697 | `	}` |
|        - |  6698 | `	/* All done, function body compiled */` |
|  1413263 |  6699 | `	return SXRET_OK;` |
|   706634 |  6700 | `}` |
|        - |  6701 | `/*` |
|        - |  6702 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|        - |  6703 | ` * According to the PHP language reference manual.` |
|        - |  6704 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|        - |  6705 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|        - |  6706 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|        - |  6707 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|        - |  6708 | ` *  Functions need not be defined before they are referenced.` |
|        - |  6709 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|        - |  6710 | ` *  a function even if they were defined inside and vice versa.` |
|        - |  6711 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|        - |  6712 | ` *  calls with over 32-64 recursion levels.` |
|        - |  6713 | ` *` |
|        - |  6714 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|        - |  6715 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|        - |  6716 | ` * on these extension.` |
|        - |  6717 | ` */` |
|        - |  6718 | `/*` |
|        - |  6719 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|        - |  6720 | ` */` |
|      570 |  6721 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|        5 |  6722 | `{` |
|        - |  6723 | `	sxu32 i;` |
|     1611 |  6724 | `	for( i = 0; i < n; i++ ){` |
|     1381 |  6725 | `		int a = zA[i], b = zB[i];` |
|     1381 |  6726 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|     1381 |  6727 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|     1381 |  6728 | `		if( a != b ) return a - b;` |
|      523 |  6729 | `	}` |
|      235 |  6730 | `	return 0;` |
|      290 |  6731 | `}` |
|        - |  6732 | `/*` |
|        - |  6733 | ` * Internal type-atom kinds used during union type parsing.` |
|        - |  6734 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|        - |  6735 | ` * (which are positive bit values stored in sxu32).` |
|        - |  6736 | ` */` |
|        - |  6737 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|        - |  6738 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|        - |  6739 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|        - |  6740 |  |
|        - |  6741 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|        - |  6742 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|        - |  6743 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|        - |  6744 |  |
|        - |  6745 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|        - |  6746 | `struct PhlTypeAtom {` |
|        - |  6747 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|        - |  6748 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|        - |  6749 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|        - |  6750 | `	sxu32 nCanon;` |
|        - |  6751 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|        - |  6752 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|        - |  6753 | `};` |
|        - |  6754 |  |
|        - |  6755 | `/*` |
|        - |  6756 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|        - |  6757 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|        - |  6758 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|        - |  6759 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|        - |  6760 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|        - |  6761 | ` * already be consumed by the caller.` |
|        - |  6762 | ` */` |
|    98656 |  6763 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|        5 |  6764 | `{` |
|    98661 |  6765 | `	SyToken *pIn = pGen->pIn;` |
|    98661 |  6766 | `	SyZero(pOut, sizeof(*pOut));` |
|    98661 |  6767 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|    98661 |  6768 | `	if( pIn >= pGen->pEnd ){` |
|      ! 0 |  6769 | `		return SXERR_SYNTAX;` |
|        - |  6770 | `	}` |
|        - |  6771 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|    98661 |  6772 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|        8 |  6773 | `		pIn++;` |
|        8 |  6774 | `		if( pIn >= pGen->pEnd ){` |
|      ! 0 |  6775 | `			return SXERR_SYNTAX;` |
|        - |  6776 | `		}` |
|        3 |  6777 | `	}` |
|    98661 |  6778 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  6779 | `		return SXERR_SYNTAX;` |
|        - |  6780 | `	}` |
|    98661 |  6781 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|    82585 |  6782 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|    82585 |  6783 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|    11693 |  6784 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|    76741 |  6785 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|       81 |  6786 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|    70859 |  6787 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|    19965 |  6788 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|    60841 |  6789 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|    50779 |  6790 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|    25474 |  6791 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|       41 |  6792 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|       68 |  6793 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|       27 |  6794 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|       37 |  6795 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       14 |  6796 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|       23 |  6797 | `			pOut->nType = SXU32_HIGH;` |
|       23 |  6798 | `			pOut->sClass = pIn->sData;` |
|       13 |  6799 | `		}else{` |
|        3 |  6800 | `			return SXERR_SYNTAX;` |
|        - |  6801 | `		}` |
|    82583 |  6802 | `		pIn++;` |
|    41294 |  6803 | `	}else{` |
|        - |  6804 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|        - |  6805 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|    16081 |  6806 | `		SyString *pT = &pIn->sData;` |
|    16081 |  6807 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|       34 |  6808 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|       34 |  6809 | `			pIn++;` |
|    16066 |  6810 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|      177 |  6811 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|      177 |  6812 | `			pIn++;` |
|    15965 |  6813 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       26 |  6814 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       26 |  6815 | `			pIn++;` |
|       15 |  6816 | `		}else{` |
|        - |  6817 | `			/* Class / interface name; consume namespace path a\b\c */` |
|    15857 |  6818 | `			SyToken *pFirst = pIn;` |
|    15857 |  6819 | `			SyToken *pLast = pIn;` |
|    15857 |  6820 | `			pOut->nType = SXU32_HIGH;` |
|    15857 |  6821 | `			pOut->sClass = pIn->sData;` |
|    15857 |  6822 | `			pIn++;` |
|    23781 |  6823 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|    15860 |  6824 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|        3 |  6825 | `				pLast = &pIn[1];` |
|        3 |  6826 | `				pIn += 2;` |
|        1 |  6827 | `			}` |
|    15857 |  6828 | `			if( pLast != pFirst ){` |
|        3 |  6829 | `				const char *zFirst = pFirst->sData.zString;` |
|        3 |  6830 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|        3 |  6831 | `				pOut->sClass.zString = zFirst;` |
|        3 |  6832 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|        1 |  6833 | `			}` |
|        - |  6834 | `		}` |
|        - |  6835 | `	}` |
|    98659 |  6836 | `	pGen->pIn = pIn;` |
|    98659 |  6837 | `	return SXRET_OK;` |
|    49333 |  6838 | `}` |
|        - |  6839 |  |
|        - |  6840 | `/*` |
|        - |  6841 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|        - |  6842 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|        - |  6843 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|        - |  6844 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|        - |  6845 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|        - |  6846 | ` */` |
|    98478 |  6847 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|        5 |  6848 | `{` |
|        - |  6849 | `	int i;` |
|    98483 |  6850 | `	int nNonNull = 0;` |
|    98483 |  6851 | `	int bAnyIntersection = 0;` |
|        - |  6852 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|    98483 |  6853 | `	sxu32 nMaxGroup = 0;` |
|  3249779 |  6854 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|   197113 |  6855 | `	for( i = 0; i < nAtoms; i++ ){` |
|    98635 |  6856 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    98605 |  6857 | `			nNonNull++;` |
|    98605 |  6858 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|    98605 |  6859 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|    98605 |  6860 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|    49300 |  6861 | `			}` |
|    49300 |  6862 | `		}` |
|    49320 |  6863 | `	}` |
|   197061 |  6864 | `	for( i = 0; i < nAtoms; i++ ){` |
|    98607 |  6865 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|       29 |  6866 | `			bAnyIntersection = 1;` |
|       29 |  6867 | `			break;` |
|        - |  6868 | `		}` |
|    49294 |  6869 | `	}` |
|    98483 |  6870 | `	if( bAnyIntersection ){` |
|        - |  6871 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|        - |  6872 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|        - |  6873 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|       29 |  6874 | `		sxu32 g, nGroups = 0;` |
|       29 |  6875 | `		int bFirstGroup = 1;` |
|       59 |  6876 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|       59 |  6877 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|       35 |  6878 | `			int bFirstMember = 1;` |
|        - |  6879 | `			int bWrap;` |
|       35 |  6880 | `			if( aGroupCount[g] == 0 ) continue;` |
|        - |  6881 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|        - |  6882 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|        - |  6883 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|        - |  6884 | `			 * parens, matching PHP's canonical text. */` |
|       47 |  6885 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|       35 |  6886 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|       35 |  6887 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|      107 |  6888 | `			for( i = 0; i < nAtoms; i++ ){` |
|       77 |  6889 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|       59 |  6890 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|       59 |  6891 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|       55 |  6892 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       30 |  6893 | `				}else{` |
|        6 |  6894 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|        - |  6895 | `				}` |
|       59 |  6896 | `				bFirstMember = 0;` |
|       32 |  6897 | `			}` |
|       35 |  6898 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|       35 |  6899 | `			bFirstGroup = 0;` |
|       20 |  6900 | `		}` |
|       29 |  6901 | `		if( bNullable ){` |
|      ! 0 |  6902 | `			SyBlobAppend(pBlob, "\|", 1);` |
|      ! 0 |  6903 | `			SyBlobAppend(pBlob, "null", 4);` |
|      ! 0 |  6904 | `		}` |
|       78 |  6905 | `		return;` |
|        - |  6906 | `	}` |
|    98459 |  6907 | `	if( nNonNull == 1 && bNullable ){` |
|        - |  6908 | `		/* Shorthand: ?T */` |
|      102 |  6909 | `		for( i = 0; i < nAtoms; i++ ){` |
|      102 |  6910 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      102 |  6911 | `			SyBlobAppend(pBlob, "?", 1);` |
|      102 |  6912 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|       24 |  6913 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       14 |  6914 | `			}else{` |
|       82 |  6915 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|        - |  6916 | `			}` |
|      102 |  6917 | `			return;` |
|      ! 0 |  6918 | `		}` |
|      ! 0 |  6919 | `	}` |
|        - |  6920 | `	{` |
|    98361 |  6921 | `		int bFirst = 1;` |
|        - |  6922 | `		/* 1) Classes in declaration order */` |
|   196825 |  6923 | `		for( i = 0; i < nAtoms; i++ ){` |
|    98469 |  6924 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|    15807 |  6925 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    15807 |  6926 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|    15807 |  6927 | `				bFirst = 0;` |
|     7901 |  6928 | `			}` |
|    49237 |  6929 | `		}` |
|        - |  6930 | `		/* 2) Built-ins in canonical order */` |
|        - |  6931 | `		{` |
|        - |  6932 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|        - |  6933 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|        - |  6934 | `			int k;` |
|   688497 |  6935 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  1098353 |  6936 | `				for( i = 0; i < nAtoms; i++ ){` |
|   590677 |  6937 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|    82465 |  6938 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    82465 |  6939 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|    82465 |  6940 | `						bFirst = 0;` |
|    82465 |  6941 | `						break;` |
|        - |  6942 | `					}` |
|   254111 |  6943 | `				}` |
|   295073 |  6944 | `			}` |
|        - |  6945 | `		}` |
|        - |  6946 | `		/* 3) null suffix */` |
|    98361 |  6947 | `		if( bNullable ){` |
|       19 |  6948 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|       19 |  6949 | `			SyBlobAppend(pBlob, "null", 4);` |
|        8 |  6950 | `		}` |
|        - |  6951 | `	}` |
|    49244 |  6952 | `}` |
|        - |  6953 |  |
|        - |  6954 | `/*` |
|        - |  6955 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|        - |  6956 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|        - |  6957 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|        - |  6958 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|        - |  6959 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|        - |  6960 | ` * whether it was parenthesized.` |
|        - |  6961 | ` *` |
|        - |  6962 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|        - |  6963 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|        - |  6964 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|        - |  6965 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|        - |  6966 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|        - |  6967 | ` */` |
|    98630 |  6968 | `static sxi32 GenStateParsePart(` |
|        - |  6969 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|        - |  6970 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|        5 |  6971 | `{` |
|        - |  6972 | `	sxi32 rc;` |
|    98635 |  6973 | `	int nMembers = 0;` |
|    98635 |  6974 | `	int bParen = 0;` |
|    98635 |  6975 | `	*pnMembers = 0;` |
|    98635 |  6976 | `	*pbParen = 0;` |
|    98635 |  6977 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        9 |  6978 | `		bParen = 1;` |
|        9 |  6979 | `		pGen->pIn++; /* skip '(' */` |
|        3 |  6980 | `	}` |
|    49315 |  6981 | `	for(;;){` |
|    98661 |  6982 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|      ! 0 |  6983 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  6984 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|      ! 0 |  6985 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  6986 | `		}` |
|    98661 |  6987 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|    98661 |  6988 | `		if( rc != SXRET_OK ){` |
|        3 |  6989 | `			return rc;` |
|        - |  6990 | `		}` |
|    98659 |  6991 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|    98659 |  6992 | `		(*pnAtoms)++;` |
|    98659 |  6993 | `		nMembers++;` |
|        - |  6994 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|    98659 |  6995 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       39 |  6996 | `			SyToken *pNext = &pGen->pIn[1];` |
|       34 |  6997 | `			if( pNext < pGen->pEnd` |
|       39 |  6998 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       31 |  6999 | `				pGen->pIn++; /* skip '&' */` |
|       31 |  7000 | `				continue;` |
|        - |  7001 | `			}` |
|        4 |  7002 | `		}` |
|    98633 |  7003 | `		break;` |
|      ! 0 |  7004 | `	}` |
|    98633 |  7005 | `	if( bParen ){` |
|        9 |  7006 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 |  7007 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7008 | `				"Malformed DNF type: expecting ')'");` |
|      ! 0 |  7009 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  7010 | `		}` |
|        9 |  7011 | `		pGen->pIn++; /* skip ')' */` |
|        9 |  7012 | `		if( nMembers < 2 ){` |
|      ! 0 |  7013 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7014 | `				"Parenthesized type must be an intersection of at least two types");` |
|      ! 0 |  7015 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  7016 | `		}` |
|        3 |  7017 | `	}` |
|    98633 |  7018 | `	*pnMembers = nMembers;` |
|    98633 |  7019 | `	*pbParen = bParen;` |
|    98633 |  7020 | `	return SXRET_OK;` |
|    49320 |  7021 | `}` |
|        - |  7022 |  |
|        - |  7023 | `/*` |
|        - |  7024 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|        - |  7025 | ` *` |
|        - |  7026 | ` * Outputs:` |
|        - |  7027 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|        - |  7028 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|        - |  7029 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|        - |  7030 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|        - |  7031 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|        - |  7032 | ` *     already be initialized by the caller (allocator set, etc).` |
|        - |  7033 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|        - |  7034 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|        - |  7035 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|        - |  7036 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|        - |  7037 | ` *` |
|        - |  7038 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|        - |  7039 | ` * SXERR_ABORT on fatal compile errors.` |
|        - |  7040 | ` */` |
|    98494 |  7041 | `static sxi32 GenStateParseUnionTypeDecl(` |
|        - |  7042 | `	ph7_gen_state *pGen,` |
|        - |  7043 | `	sxu32 *pnType,` |
|        - |  7044 | `	SyString *pClass,` |
|        - |  7045 | `	SySet *pAlts,` |
|        - |  7046 | `	sxi32 *piTypeFlags,` |
|        - |  7047 | `	SyString *pTypeText,` |
|        - |  7048 | `	int iNullableFlag,` |
|        - |  7049 | `	int iUnionFlag,` |
|        - |  7050 | `	int bAllowVoid,` |
|        - |  7051 | `	sxu32 nLine` |
|        5 |  7052 | `){` |
|        - |  7053 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|    98499 |  7054 | `	int nAtoms = 0;` |
|    98499 |  7055 | `	int bShortNullable = 0;` |
|    98499 |  7056 | `	int bExplicitNull = 0;` |
|        - |  7057 | `	sxi32 rc;` |
|    98499 |  7058 | `	*pnType = 0;` |
|    98499 |  7059 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|    98499 |  7060 | `	*piTypeFlags = 0;` |
|    98499 |  7061 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|        - |  7062 |  |
|    98499 |  7063 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  7064 | `		return SXRET_OK;` |
|        - |  7065 | `	}` |
|        - |  7066 | ``	/* Optional `?` shorthand prefix */`` |
|    98494 |  7067 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|       91 |  7068 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|       90 |  7069 | `		bShortNullable = 1;` |
|       90 |  7070 | `		pGen->pIn++;` |
|       90 |  7071 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  7072 | `			return SXERR_SYNTAX;` |
|        - |  7073 | `		}` |
|       43 |  7074 | `	}` |
|        - |  7075 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|        - |  7076 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|        - |  7077 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|        - |  7078 | `	{` |
|        - |  7079 | `		int nMembers, bParen;` |
|    98499 |  7080 | `		sxu32 iGroup = 0;` |
|    98499 |  7081 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|    98499 |  7082 | `		if( rc != SXRET_OK ){` |
|        4 |  7083 | `			return rc;` |
|        - |  7084 | `		}` |
|        - |  7085 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|        - |  7086 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|        - |  7087 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|        - |  7088 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|        - |  7089 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|   147947 |  7090 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|    98706 |  7091 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      143 |  7092 | `			if( bShortNullable ){` |
|        - |  7093 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|        - |  7094 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|        - |  7095 | `				 * already reported" so callers skip their own error emission. */` |
|        3 |  7096 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|        - |  7097 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|        3 |  7098 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|        - |  7099 | `			}` |
|      141 |  7100 | `			if( nMembers >= 2 && !bParen ){` |
|      ! 0 |  7101 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|        - |  7102 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|      ! 0 |  7103 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  7104 | `			}` |
|      141 |  7105 | ``			pGen->pIn++; /* skip `\|` */`` |
|      141 |  7106 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|      141 |  7107 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  7108 | `				return rc;` |
|        - |  7109 | `			}` |
|        5 |  7110 | `		}` |
|    98495 |  7111 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|      ! 0 |  7112 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7113 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|      ! 0 |  7114 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  7115 | `		}` |
|        - |  7116 | `	}` |
|        - |  7117 | `	/* Validation pass.` |
|        - |  7118 | `	 *` |
|        - |  7119 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|        - |  7120 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|        - |  7121 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|        - |  7122 | `	 */` |
|        - |  7123 | `	{` |
|        - |  7124 | `		int i, j;` |
|    98495 |  7125 | `		int bHasNonNull = 0;` |
|    98495 |  7126 | `		int bAnyIntersection = 0;` |
|        - |  7127 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|        - |  7128 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|        - |  7129 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
|  3250175 |  7130 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|   197147 |  7131 | `		for( i = 0; i < nAtoms; i++ ){` |
|    98657 |  7132 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|    49331 |  7133 | `		}` |
|   197091 |  7134 | `		for( i = 0; i < nAtoms; i++ ){` |
|    98627 |  7135 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|    49303 |  7136 | `		}` |
|        - |  7137 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|        - |  7138 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|    98495 |  7139 | `		if( bShortNullable && bAnyIntersection ){` |
|      ! 0 |  7140 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7141 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|      ! 0 |  7142 | `			return SXERR_SYNTAX;` |
|        - |  7143 | `		}` |
|   197133 |  7144 | `		for( i = 0; i < nAtoms; i++ ){` |
|        - |  7145 | `			/* Intersection members must be class/interface types (PHP rejects` |
|        - |  7146 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|        - |  7147 | ``			 * `true`/`false` in an intersection). */`` |
|    98655 |  7148 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|       55 |  7149 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|       55 |  7150 | `				if( bClassLike ){` |
|       53 |  7151 | `					SyString *pC = &aAtoms[i].sClass;` |
|       48 |  7152 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|       48 |  7153 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|       48 |  7154 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|       53 |  7155 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|      ! 0 |  7156 | `						bClassLike = 0;` |
|      ! 0 |  7157 | `					}` |
|       24 |  7158 | `				}` |
|       55 |  7159 | `				if( !bClassLike ){` |
|        - |  7160 | `					const char *zName; sxu32 nName;` |
|        3 |  7161 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      ! 0 |  7162 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|      ! 0 |  7163 | `					}else{` |
|        3 |  7164 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|        - |  7165 | `					}` |
|        4 |  7166 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7167 | `						"Type %.*s cannot be part of an intersection type",` |
|        1 |  7168 | `						(int)nName, zName);` |
|        3 |  7169 | `					return SXERR_SYNTAX;` |
|        - |  7170 | `				}` |
|       24 |  7171 | `			}` |
|    98653 |  7172 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|      177 |  7173 | `				if( nAtoms > 1 ){` |
|        3 |  7174 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7175 | `						"Void can only be used as a standalone type");` |
|        3 |  7176 | `					return SXERR_SYNTAX;` |
|        - |  7177 | `				}` |
|      175 |  7178 | `				if( !bAllowVoid ){` |
|      ! 0 |  7179 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7180 | `						"void cannot be used here");` |
|      ! 0 |  7181 | `					return SXERR_SYNTAX;` |
|        - |  7182 | `				}` |
|      175 |  7183 | `				if( bShortNullable ){` |
|      ! 0 |  7184 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7185 | `						"Void type cannot be nullable");` |
|      ! 0 |  7186 | `					return SXERR_SYNTAX;` |
|        - |  7187 | `				}` |
|       85 |  7188 | `			}` |
|    98651 |  7189 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|        - |  7190 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|        - |  7191 | `				 * type (never = the function does not return). Mirrors the void` |
|        - |  7192 | `				 * validation above; accepted here and enforced at compile time` |
|        - |  7193 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|       26 |  7194 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|        - |  7195 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|        - |  7196 | `					 * same as any other non-standalone use. */` |
|        5 |  7197 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7198 | `						"never can only be used as a standalone type");` |
|        5 |  7199 | `					return SXERR_SYNTAX;` |
|        - |  7200 | `				}` |
|       21 |  7201 | `				if( !bAllowVoid ){` |
|        - |  7202 | `					/* Return-only: params call with bAllowVoid=0. */` |
|        3 |  7203 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7204 | `						"never cannot be used as a parameter type");` |
|        3 |  7205 | `					return SXERR_SYNTAX;` |
|        - |  7206 | `				}` |
|        8 |  7207 | `			}` |
|    98645 |  7208 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|       34 |  7209 | `				bExplicitNull = 1;` |
|       19 |  7210 | `			}else{` |
|    98615 |  7211 | `				bHasNonNull = 1;` |
|        - |  7212 | `			}` |
|        - |  7213 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|        - |  7214 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|        - |  7215 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|        - |  7216 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|        - |  7217 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|    98845 |  7218 | `			for( j = 0; j < i; j++ ){` |
|      207 |  7219 | `				int bDup = 0;` |
|      207 |  7220 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|      395 |  7221 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|      202 |  7222 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|      207 |  7223 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|      195 |  7224 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|       51 |  7225 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|       44 |  7226 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|       44 |  7227 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|       17 |  7228 | `								aAtoms[j].sClass.zString,` |
|       34 |  7229 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|      ! 0 |  7230 | `							bDup = 1;` |
|      ! 0 |  7231 | `						}` |
|       27 |  7232 | `					}else{` |
|        3 |  7233 | `						bDup = 1;` |
|        - |  7234 | `					}` |
|       23 |  7235 | `				}` |
|      195 |  7236 | `				if( bDup ){` |
|        - |  7237 | `					const char *zName;` |
|        - |  7238 | `					sxu32 nName;` |
|        3 |  7239 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      ! 0 |  7240 | `						zName = aAtoms[i].sClass.zString;` |
|      ! 0 |  7241 | `						nName = aAtoms[i].sClass.nByte;` |
|      ! 0 |  7242 | `					}else{` |
|        3 |  7243 | `						zName = aAtoms[i].zCanon;` |
|        3 |  7244 | `						nName = aAtoms[i].nCanon;` |
|        - |  7245 | `					}` |
|        4 |  7246 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        1 |  7247 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|        3 |  7248 | `					return SXERR_SYNTAX;` |
|        - |  7249 | `				}` |
|       99 |  7250 | `			}` |
|    49324 |  7251 | `		}` |
|    98483 |  7252 | `		if( !bHasNonNull && bExplicitNull ){` |
|        7 |  7253 | `			if( bShortNullable ){` |
|        - |  7254 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|      ! 0 |  7255 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - |  7256 | `					"Null can not be used as a standalone type");` |
|      ! 0 |  7257 | `				return SXERR_SYNTAX;` |
|        - |  7258 | `			}` |
|        - |  7259 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|        - |  7260 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|        - |  7261 | `			 * path below leaves *pnType untouched when there is no non-null` |
|        - |  7262 | `			 * atom, so set it here. */` |
|        7 |  7263 | `			*pnType = MEMOBJ_NULL;` |
|        3 |  7264 | `		}` |
|        - |  7265 | `	}` |
|        - |  7266 | `	/* Compute nullability flag */` |
|    98483 |  7267 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      118 |  7268 | `		*piTypeFlags \|= iNullableFlag;` |
|       57 |  7269 | `	}` |
|        - |  7270 | `	/* Build canonical type text */` |
|    98483 |  7271 | `	if( pTypeText ){` |
|        - |  7272 | `		SyBlob sBlob;` |
|    98483 |  7273 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|   147680 |  7274 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|    49239 |  7275 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|    98483 |  7276 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|   147443 |  7277 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    98292 |  7278 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|    98297 |  7279 | `			if( zDup ){` |
|    98297 |  7280 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|    49146 |  7281 | `			}` |
|    49146 |  7282 | `		}` |
|    98483 |  7283 | `		SyBlobRelease(&sBlob);` |
|    49239 |  7284 | `	}` |
|        - |  7285 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|        - |  7286 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|        - |  7287 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|        - |  7288 | `	{` |
|    98483 |  7289 | `		int nNonNull = 0;` |
|    98483 |  7290 | `		int iNonNullIdx = -1;` |
|        - |  7291 | `		int i;` |
|   197113 |  7292 | `		for( i = 0; i < nAtoms; i++ ){` |
|    98635 |  7293 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|    98605 |  7294 | `				nNonNull++;` |
|    98605 |  7295 | `				iNonNullIdx = i;` |
|    49300 |  7296 | `			}` |
|    49320 |  7297 | `		}` |
|    98483 |  7298 | `		if( nNonNull <= 1 ){` |
|        - |  7299 | `			/* Fast path: store as single type. */` |
|    98377 |  7300 | `			if( iNonNullIdx >= 0 ){` |
|    98371 |  7301 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|    98371 |  7302 | `				if( pA->nType == SXU32_HIGH ){` |
|    23672 |  7303 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     7889 |  7304 | `						pA->sClass.zString, pA->sClass.nByte);` |
|    15783 |  7305 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|    15783 |  7306 | `					*pnType = SXU32_HIGH;` |
|    15783 |  7307 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|    90482 |  7308 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|      175 |  7309 | `					*pnType = MEMOBJ_VOID;` |
|    82508 |  7310 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|       18 |  7311 | `					*pnType = MEMOBJ_NEVER;` |
|       10 |  7312 | `				}else{` |
|    82407 |  7313 | `					*pnType = pA->nType;` |
|        - |  7314 | `				}` |
|    49183 |  7315 | `			}` |
|    49191 |  7316 | `		}else{` |
|        - |  7317 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      111 |  7318 | `			*piTypeFlags \|= iUnionFlag;` |
|      355 |  7319 | `			for( i = 0; i < nAtoms; i++ ){` |
|        - |  7320 | `				ph7_type_alt sAlt;` |
|      249 |  7321 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      239 |  7322 | `				SyZero(&sAlt, sizeof(sAlt));` |
|      239 |  7323 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|      239 |  7324 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      146 |  7325 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       47 |  7326 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       99 |  7327 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|       99 |  7328 | `					sAlt.nType = SXU32_HIGH;` |
|       99 |  7329 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|       52 |  7330 | `				}else{` |
|      145 |  7331 | `					sAlt.nType = aAtoms[i].nType;` |
|      145 |  7332 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|        - |  7333 | `				}` |
|      239 |  7334 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|      122 |  7335 | `			}` |
|        - |  7336 | `		}` |
|        - |  7337 | `	}` |
|    98483 |  7338 | `	return SXRET_OK;` |
|    49252 |  7339 | `}` |
|        - |  7340 |  |
|        - |  7341 | `/*` |
|        - |  7342 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|        - |  7343 | `` * pGen->pIn should point to the token after `)`.`` |
|        - |  7344 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|        - |  7345 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|        - |  7346 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|        - |  7347 | `` *          and union types `: T\|U`.`` |
|        - |  7348 | ` */` |
|  1514600 |  7349 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|        5 |  7350 | `{` |
|  1514605 |  7351 | `	sxi32 iFlags = 0;` |
|        - |  7352 | `	sxi32 rc;` |
|        - |  7353 | `	sxu32 nLine;` |
|  1514605 |  7354 | `	pFunc->nReturnType = 0;` |
|  1514605 |  7355 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  1514605 |  7356 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|        - |  7357 | `	/* Reset ALL declared-return-type state, not just the scalar fields: this` |
|        - |  7358 | `	 * parser can legitimately run twice for one closure (legacy pre-use colon` |
|        - |  7359 | `	 * position + the php post-use position). Leaving stale union alternatives` |
|        - |  7360 | `	 * or the nullable flag behind merges two declarations — enforcement then` |
|        - |  7361 | ``	 * honored a wiped `: int\|string` over the real `: bool`. */`` |
|  1514605 |  7362 | `	SySetReset(&pFunc->aReturnUnion);` |
|  1514605 |  7363 | `	pFunc->iFlags &= ~VM_FUNC_RETURN_NULLABLE;` |
|  1514605 |  7364 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  1513953 |  7365 | `		return SXRET_OK;` |
|        - |  7366 | `	}` |
|      657 |  7367 | `	pGen->pIn++; /* Skip ':' */` |
|      657 |  7368 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  7369 | `		return SXRET_OK;` |
|        - |  7370 | `	}` |
|      657 |  7371 | `	nLine = pGen->pIn->nLine;` |
|      657 |  7372 | `	rc = GenStateParseUnionTypeDecl(` |
|      326 |  7373 | `		pGen,` |
|      326 |  7374 | `		&pFunc->nReturnType,` |
|      326 |  7375 | `		&pFunc->sReturnClass,` |
|      326 |  7376 | `		&pFunc->aReturnUnion,` |
|        - |  7377 | `		&iFlags,` |
|      326 |  7378 | `		&pFunc->sReturnTypeName,` |
|        - |  7379 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|        - |  7380 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|        - |  7381 | `		/* iUnionFlag */ 0,` |
|        - |  7382 | `		/* bAllowVoid */ 1,` |
|      326 |  7383 | `		nLine);` |
|      657 |  7384 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  7385 | `		return SXERR_ABORT;` |
|        - |  7386 | `	}` |
|      657 |  7387 | `	if( rc == SXERR_CORRUPT ){` |
|        - |  7388 | `		/* Error already reported */` |
|      ! 0 |  7389 | `		return SXERR_SYNTAX;` |
|        - |  7390 | `	}` |
|      657 |  7391 | `	if( rc == SXERR_SYNTAX ){` |
|        8 |  7392 | `		if( pGen->pIn < pGen->pEnd ){` |
|       11 |  7393 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|        - |  7394 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|        6 |  7395 | `				&pGen->pIn->sData);` |
|        5 |  7396 | `		}else{` |
|      ! 0 |  7397 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|        - |  7398 | `				"syntax error, unexpected end of file in return type declaration");` |
|        - |  7399 | `		}` |
|        8 |  7400 | `		return SXERR_SYNTAX;` |
|        - |  7401 | `	}` |
|      651 |  7402 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|      651 |  7403 | `	return SXRET_OK;` |
|   757305 |  7404 | `}` |
|        - |  7405 |  |
|   118436 |  7406 | `static sxi32 GenStateCompileFunc(` |
|        - |  7407 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  7408 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|        - |  7409 | `	sxi32 iFlags,        /* Control flags */` |
|        - |  7410 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|        - |  7411 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|        - |  7412 | `	)` |
|        5 |  7413 | `{` |
|        - |  7414 | `	ph7_vm_func *pFunc;` |
|        - |  7415 | `	SyToken *pEnd;` |
|        - |  7416 | `	sxu32 nLine;` |
|        - |  7417 | `	char *zName;` |
|        - |  7418 | `	sxi32 rc;` |
|        - |  7419 | `	/* Extract line number */` |
|   118441 |  7420 | `	nLine = pGen->pIn->nLine;` |
|        - |  7421 | `	/* Jump the left parenthesis '(' */` |
|   118441 |  7422 | `	pGen->pIn++;` |
|        - |  7423 | `	/* Delimit the function signature */` |
|   118441 |  7424 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   118441 |  7425 | `	if( pEnd >= pGen->pEnd ){` |
|        - |  7426 | `		/* Syntax error */` |
|        8 |  7427 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|        8 |  7428 | `		if( rc == SXERR_ABORT ){` |
|        - |  7429 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7430 | `			return SXERR_ABORT;` |
|        - |  7431 | `		}` |
|        8 |  7432 | `		pGen->pIn = pGen->pEnd;` |
|        8 |  7433 | `		return SXRET_OK;` |
|        - |  7434 | `	}` |
|        - |  7435 | `	/* Create the function state */` |
|   118435 |  7436 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   118435 |  7437 | `	if( pFunc == 0 ){` |
|      ! 0 |  7438 | `		goto OutOfMem;` |
|        - |  7439 | `	}` |
|        - |  7440 | `	/* Build the function name, prepending namespace if active */` |
|   118442 |  7441 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|        - |  7442 | `		SyBlob sFQN;` |
|        - |  7443 | `		sxu32 nLen;` |
|       16 |  7444 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       16 |  7445 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       16 |  7446 | `		SyBlobAppend(&sFQN,"\\",1);` |
|       16 |  7447 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|       16 |  7448 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|       16 |  7449 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|       16 |  7450 | `		SyBlobRelease(&sFQN);` |
|       16 |  7451 | `		if( zName == 0 ){` |
|      ! 0 |  7452 | `			goto OutOfMem;` |
|        - |  7453 | `		}` |
|       16 |  7454 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|        9 |  7455 | `	}else{` |
|   118421 |  7456 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   118421 |  7457 | `		if( zName == 0 ){` |
|      ! 0 |  7458 | `			goto OutOfMem;` |
|        - |  7459 | `		}` |
|   118421 |  7460 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|        - |  7461 | `	}` |
|        - |  7462 | `	/* Fallback start line (the '(' token); callers that know the line of the` |
|        - |  7463 | `	 * 'function'/'fn' keyword overwrite this with the exact PHP getStartLine. */` |
|   118435 |  7464 | `	pFunc->nLine = nLine;` |
|   118435 |  7465 | `	GenStateConsumeDoc(&(*pGen),&pFunc->sDoc);` |
|   118435 |  7466 | `	if( GenStateConsumeAttrs(&(*pGen),&pFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  7467 | `		return SXERR_ABORT;` |
|        - |  7468 | `	}` |
|   118435 |  7469 | `	if( pGen->pIn < pEnd ){` |
|        - |  7470 | `		/* Collect function arguments */` |
|   102077 |  7471 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   102077 |  7472 | `		if( rc == SXERR_ABORT ){` |
|        - |  7473 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  7474 | `			return SXERR_ABORT;` |
|        - |  7475 | `		}` |
|    51036 |  7476 | `	}` |
|        - |  7477 | `	/* Point past ')' and parse optional return type ': type' */` |
|   118435 |  7478 | `	pGen->pIn = &pEnd[1];` |
|        - |  7479 | `	{` |
|   118435 |  7480 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   118435 |  7481 | `		if( rcRt == SXERR_ABORT ){` |
|      ! 0 |  7482 | `			return SXERR_ABORT;` |
|   118435 |  7483 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|        8 |  7484 | `			return SXERR_SYNTAX;` |
|        - |  7485 | `		}` |
|        - |  7486 | `	}` |
|   118429 |  7487 | `	if( bHandleClosure ){` |
|        - |  7488 | `		ph7_vm_func_closure_env sEnv;` |
|      453 |  7489 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|      448 |  7490 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      270 |  7491 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|       87 |  7492 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |  7493 | `				/* Closure,record environment variable */` |
|       87 |  7494 | `				pGen->pIn++;` |
|       87 |  7495 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 |  7496 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|      ! 0 |  7497 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  7498 | `						return SXERR_ABORT;` |
|        - |  7499 | `					}` |
|      ! 0 |  7500 | `				}` |
|       87 |  7501 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|        - |  7502 | `				/* Compile until we hit the first closing parenthesis */` |
|      179 |  7503 | `				while( pGen->pIn < pGen->pEnd ){` |
|      179 |  7504 | `					int iFlagsLocal = 0;` |
|      179 |  7505 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|       87 |  7506 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|       87 |  7507 | `						break;` |
|        - |  7508 | `					}` |
|       97 |  7509 | `					nLineLocal = pGen->pIn->nLine;` |
|       97 |  7510 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|        - |  7511 | `						/* Capture by reference: OP_LOAD_CLOSURE binds the env entry` |
|        - |  7512 | `						 * to the variable's memory slot instead of copying its value. */` |
|       53 |  7513 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|       53 |  7514 | `						pGen->pIn++;` |
|       26 |  7515 | `					}` |
|       92 |  7516 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|       97 |  7517 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  7518 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        - |  7519 | `								"Closure: Unexpected token. Expecting a variable name");` |
|      ! 0 |  7520 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 |  7521 | `								return SXERR_ABORT;` |
|        - |  7522 | `							}` |
|        - |  7523 | `							/* Find the closing parenthesis */` |
|      ! 0 |  7524 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 |  7525 | `								pGen->pIn++;` |
|      ! 0 |  7526 | `							}` |
|      ! 0 |  7527 | `							if(pGen->pIn < pGen->pEnd){` |
|      ! 0 |  7528 | `								pGen->pIn++;` |
|      ! 0 |  7529 | `							}` |
|      ! 0 |  7530 | `							break;` |
|        - |  7531 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|      ! 0 |  7532 | `					}else{` |
|        - |  7533 | `						SyString *pNameLocal;` |
|        - |  7534 | `						char *zDup;` |
|        - |  7535 | `						/* Duplicate variable name */` |
|       97 |  7536 | `						pNameLocal = &pGen->pIn[1].sData;` |
|       97 |  7537 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|       97 |  7538 | `						if( zDup ){` |
|        - |  7539 | `							/* Zero the structure */` |
|       97 |  7540 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       97 |  7541 | `							sEnv.iFlags = iFlagsLocal;` |
|       97 |  7542 | `							sEnv.nIdx = SXU32_HIGH;` |
|       97 |  7543 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       97 |  7544 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      112 |  7545 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|       30 |  7546 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|      ! 0 |  7547 | `									got_this = 1;` |
|      ! 0 |  7548 | `							}` |
|        - |  7549 | `							/* Save imported variable */` |
|       97 |  7550 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       51 |  7551 | `						}else{` |
|      ! 0 |  7552 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  7553 | `							 return SXERR_ABORT;` |
|        - |  7554 | `						}` |
|        - |  7555 | `					}` |
|       97 |  7556 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      109 |  7557 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  7558 | `						/* Ignore trailing commas */` |
|       13 |  7559 | `						pGen->pIn++;` |
|        1 |  7560 | `					}` |
|        5 |  7561 | `				}` |
|        - |  7562 | `				/* php 7.1+: the return type follows the use clause —` |
|        - |  7563 | ``				 * `function (...) use (...) : int {`. Gated on the colon:`` |
|        - |  7564 | `				 * GenStateParseReturnType resets the type fields at entry,` |
|        - |  7565 | `				 * so an unconditional call would wipe a type parsed at the` |
|        - |  7566 | `				 * legacy pre-use position. */` |
|       87 |  7567 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COLON) ){` |
|        7 |  7568 | `					sxi32 rcRt2 = GenStateParseReturnType(&(*pGen),pFunc);` |
|        7 |  7569 | `					if( rcRt2 == SXERR_ABORT ){` |
|      ! 0 |  7570 | `						return SXERR_ABORT;` |
|        7 |  7571 | `					}else if( rcRt2 == SXERR_SYNTAX ){` |
|      ! 0 |  7572 | `						return SXERR_SYNTAX;` |
|        - |  7573 | `					}` |
|        3 |  7574 | `				}` |
|       41 |  7575 | `		}` |
|      453 |  7576 | `		if( !got_this && (iFlags & VM_FUNC_STATIC_CL) == 0 ){` |
|        - |  7577 | `			/* Make the $this variable [Current processed Object (class instance)]` |
|        - |  7578 | `			 * available to the closure environment — for EVERY non-static` |
|        - |  7579 | `			 * anonymous function, use list or not (php binds $this to any` |
|        - |  7580 | ``			 * closure declared in a method; pre-fix only `use (...)` closures`` |
|        - |  7581 | `			 * captured it). Flagged VM_FUNC_ARG_IGNORE so the null capture of` |
|        - |  7582 | `			 * a global-scope closure is silently dropped at install. A static` |
|        - |  7583 | `			 * closure never binds $this (php). */` |
|      445 |  7584 | `			SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      445 |  7585 | `			sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      445 |  7586 | `			sEnv.nIdx = SXU32_HIGH;` |
|      445 |  7587 | `			PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      445 |  7588 | `			SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      445 |  7589 | `			SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      220 |  7590 | `		}` |
|      453 |  7591 | `		if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|        - |  7592 | `			/* Mark as closure */` |
|      447 |  7593 | `			pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|      221 |  7594 | `		}` |
|      224 |  7595 | `	}` |
|        - |  7596 | `	/* Compile the body */` |
|   118429 |  7597 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   118429 |  7598 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  7599 | `		return SXERR_ABORT;` |
|        - |  7600 | `	}` |
|        - |  7601 | `	/* The cursor sits just past the body's closing brace */` |
|   118429 |  7602 | `	pFunc->nEndLine = pGen->pIn[-1].nLine;` |
|   118429 |  7603 | `	if( ppFunc ){` |
|   118429 |  7604 | `		*ppFunc = pFunc;` |
|    59212 |  7605 | `	}` |
|   118429 |  7606 | `	rc = SXRET_OK;` |
|   118429 |  7607 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|        - |  7608 | `		/* Finally register the function */` |
|   117987 |  7609 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|    58991 |  7610 | `	}` |
|   118429 |  7611 | `	if( rc == SXRET_OK ){` |
|   118429 |  7612 | `		return SXRET_OK;` |
|        - |  7613 | `	}` |
|        - |  7614 | `	/* Fall through if something goes wrong */` |
|      ! 0 |  7615 | `OutOfMem:` |
|        - |  7616 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  7617 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  7618 | `	 */` |
|      ! 0 |  7619 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 |  7620 | `	return SXERR_ABORT;` |
|    59223 |  7621 | `}` |
|        - |  7622 | `/*` |
|        - |  7623 | ` * Compile a standard PHP function.` |
|        - |  7624 | ` *  Refer to the block-comment above for more information.` |
|        - |  7625 | ` */` |
|   117996 |  7626 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|        5 |  7627 | `{` |
|        - |  7628 | `	SyString *pName;` |
|        - |  7629 | `	sxi32 iFlags;` |
|        - |  7630 | `	sxu32 nKwLine;` |
|        - |  7631 | `	sxu32 nLine;` |
|        - |  7632 | `	sxi32 rc;` |
|        - |  7633 |  |
|   118001 |  7634 | `	nLine = pGen->pIn->nLine;` |
|   118001 |  7635 | `	nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|   118001 |  7636 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   118001 |  7637 | `	iFlags = 0;` |
|   118001 |  7638 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        - |  7639 | `		/* Return by reference,remember that */` |
|       12 |  7640 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|        - |  7641 | `		/* Jump the '&' token */` |
|       12 |  7642 | `		pGen->pIn++;` |
|        5 |  7643 | `	}` |
|   118001 |  7644 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  7645 | `		/* Invalid function name */` |
|        8 |  7646 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|        8 |  7647 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  7648 | `			return SXERR_ABORT;` |
|        - |  7649 | `		}` |
|        - |  7650 | `		/* Sychronize with the next semi-colon or braces*/` |
|       22 |  7651 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       16 |  7652 | `			pGen->pIn++;` |
|        2 |  7653 | `		}` |
|        8 |  7654 | `		return SXRET_OK;` |
|        - |  7655 | `	}` |
|   117995 |  7656 | `	pName = &pGen->pIn->sData;` |
|   117995 |  7657 | `	nLine = pGen->pIn->nLine;` |
|        - |  7658 | `	/* Jump the function name */` |
|   117995 |  7659 | `	pGen->pIn++;` |
|   117995 |  7660 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  7661 | `		/* Syntax error */` |
|        3 |  7662 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|        3 |  7663 | `		if( rc == SXERR_ABORT ){` |
|        - |  7664 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7665 | `			return SXERR_ABORT;` |
|        - |  7666 | `		}` |
|        - |  7667 | `		/* Sychronize with the next semi-colon or '{' */` |
|        3 |  7668 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  7669 | `			pGen->pIn++;` |
|      ! 0 |  7670 | `		}` |
|        3 |  7671 | `		return SXRET_OK;` |
|        - |  7672 | `	}` |
|        - |  7673 | `	/* Compile function body */` |
|        - |  7674 | `	{` |
|   117993 |  7675 | `		ph7_vm_func *pFuncState = 0;` |
|   117993 |  7676 | `		rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,&pFuncState);` |
|   117993 |  7677 | `		if( pFuncState ){` |
|        - |  7678 | `			/* Reflection getStartLine(): line of the 'function' keyword */` |
|   117981 |  7679 | `			pFuncState->nLine = nKwLine;` |
|    58988 |  7680 | `		}` |
|        - |  7681 | `	}` |
|   117993 |  7682 | `	return rc;` |
|    59003 |  7683 | `}` |
|        - |  7684 | `/*` |
|        - |  7685 | ` * Extract the visibility level associated with a given keyword.` |
|        - |  7686 | ` * According to the PHP language reference manual` |
|        - |  7687 | ` *  Visibility:` |
|        - |  7688 | ` *  The visibility of a property or method can be defined by prefixing` |
|        - |  7689 | ` *  the declaration with the keywords public, protected or private.` |
|        - |  7690 | ` *  Class members declared public can be accessed everywhere.` |
|        - |  7691 | ` *  Members declared protected can be accessed only within the class` |
|        - |  7692 | ` *  itself and by inherited and parent classes. Members declared as private` |
|        - |  7693 | ` *  may only be accessed by the class that defines the member.` |
|        - |  7694 | ` */` |
|  1750354 |  7695 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|        5 |  7696 | `{` |
|  1750359 |  7697 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    23467 |  7698 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  1726897 |  7699 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   182629 |  7700 | `		return PH7_CLASS_PROT_PROTECTED;` |
|        - |  7701 | `	}` |
|        - |  7702 | `	/* Assume public by default */` |
|  1544273 |  7703 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   875182 |  7704 | `}` |
|        - |  7705 | `/*` |
|        - |  7706 | ` * Compile a class constant.` |
|        - |  7707 | ` * According to the PHP language reference manual` |
|        - |  7708 | ` *  Class Constants` |
|        - |  7709 | ` *   It is possible to define constant values on a per-class basis remaining` |
|        - |  7710 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|        - |  7711 | ` *   you don't use the $ symbol to declare or use them.` |
|        - |  7712 | ` *   The value must be a constant expression, not (for example) a variable,` |
|        - |  7713 | ` *   a property, a result of a mathematical operation, or a function call.` |
|        - |  7714 | ` *   It's also possible for interfaces to have constants.` |
|        - |  7715 | ` * Symisc eXtension.` |
|        - |  7716 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|        - |  7717 | ` *  the zend engine would allow only simple scalar value.` |
|        - |  7718 | ` *  Example:` |
|        - |  7719 | ` *   class Test{` |
|        - |  7720 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - |  7721 | ` *   };` |
|        - |  7722 | ` *   var_dump(TEST::MyConst);` |
|        - |  7723 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - |  7724 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - |  7725 | ` */` |
|        - |  7726 | `/*` |
|        - |  7727 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|        - |  7728 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|        - |  7729 | ` * token immediately followed by '='. Anything else with a leading type token` |
|        - |  7730 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|        - |  7731 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|        - |  7732 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|        - |  7733 | ` */` |
|   143884 |  7734 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|        5 |  7735 | `{` |
|        - |  7736 | `	SyToken *p0, *p1;` |
|   143889 |  7737 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  7738 | `		return 0;` |
|        - |  7739 | `	}` |
|   143889 |  7740 | `	p0 = pGen->pIn;` |
|        - |  7741 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|   143889 |  7742 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|      ! 0 |  7743 | `		return 1;` |
|        - |  7744 | `	}` |
|   143889 |  7745 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|        5 |  7746 | `		return 1;` |
|        - |  7747 | `	}` |
|        - |  7748 | `	/* A name-like first token begins a type only when followed by another` |
|        - |  7749 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|        - |  7750 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|   143885 |  7751 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|   143885 |  7752 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|   143885 |  7753 | `		if( p1 ){` |
|   143885 |  7754 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|       34 |  7755 | `				return 1;` |
|        - |  7756 | `			}` |
|   143855 |  7757 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|        5 |  7758 | `				return 1;` |
|        - |  7759 | `			}` |
|    71923 |  7760 | `		}` |
|    71923 |  7761 | `	}` |
|   143851 |  7762 | `	return 0;` |
|    71947 |  7763 | `}` |
|        - |  7764 | `/*` |
|        - |  7765 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|        - |  7766 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|        - |  7767 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|        - |  7768 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|        - |  7769 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|        - |  7770 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|        - |  7771 | ` * Peek only; never consumes tokens.` |
|        - |  7772 | ` */` |
|       24 |  7773 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|        4 |  7774 | `{` |
|       28 |  7775 | `	SyToken *p = pGen->pIn;` |
|       39 |  7776 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|       20 |  7777 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|        3 |  7778 | `		p++; /* skip leading unary sign(s) */` |
|        1 |  7779 | `	}` |
|       28 |  7780 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|       23 |  7781 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|        - |  7782 | `	}` |
|        6 |  7783 | `	p++;` |
|        - |  7784 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|        6 |  7785 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|       16 |  7786 | `}` |
|        - |  7787 | `/*` |
|        - |  7788 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|        - |  7789 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|        - |  7790 | `` * `$o->new`), not a `new` expression.`` |
|        - |  7791 | ` */` |
|        6 |  7792 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|        3 |  7793 | `{` |
|        - |  7794 | `	sxi32 iOp;` |
|        9 |  7795 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|      ! 0 |  7796 | `		return 0;` |
|        - |  7797 | `	}` |
|        9 |  7798 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|        9 |  7799 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|        6 |  7800 | `}` |
|        - |  7801 | `/*` |
|        - |  7802 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|        - |  7803 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|        - |  7804 | ` * interface-constant and (instance/static) property-default initializers` |
|        - |  7805 | ` * ("New expressions are not supported in this context") while still allowing it` |
|        - |  7806 | ` * in global constants, parameter defaults and static-local initializers (which` |
|        - |  7807 | ` * are compiled by different functions and left untouched). The scan is` |
|        - |  7808 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|        - |  7809 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|        - |  7810 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|        - |  7811 | ` *` |
|        - |  7812 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|        - |  7813 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|        - |  7814 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|        - |  7815 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|        - |  7816 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|        - |  7817 | ` */` |
|   229930 |  7818 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|        5 |  7819 | `{` |
|   229935 |  7820 | `	SyToken *p = pGen->pIn;` |
|   229935 |  7821 | `	int iDepth = 0;` |
|   561845 |  7822 | `	while( p < pGen->pEnd ){` |
|   561845 |  7823 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|   229927 |  7824 | `			break; /* end of this initializer */` |
|        - |  7825 | `		}` |
|   331918 |  7826 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|   169864 |  7827 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|     7800 |  7828 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|        - |  7829 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|        - |  7830 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|        - |  7831 | `			 * expression. */` |
|        3 |  7832 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|        3 |  7833 | `			p++;` |
|        3 |  7834 | `			if( bArrow ){` |
|        - |  7835 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|        - |  7836 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|        3 |  7837 | `				int iBase = iDepth;` |
|       17 |  7838 | `				while( p < pGen->pEnd ){` |
|       17 |  7839 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        5 |  7840 | `						iDepth++;` |
|       15 |  7841 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        5 |  7842 | `						if( iDepth <= iBase ){` |
|      ! 0 |  7843 | `							break; /* closes an enclosing group, not the fn's own */` |
|        - |  7844 | `						}` |
|        5 |  7845 | `						iDepth--;` |
|       11 |  7846 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|        3 |  7847 | `						break;` |
|        - |  7848 | `					}` |
|       15 |  7849 | `					p++;` |
|        1 |  7850 | `				}` |
|        2 |  7851 | `			}else{` |
|        - |  7852 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|        - |  7853 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|        - |  7854 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|        - |  7855 | `				 * then skip the balanced brace block. */` |
|      ! 0 |  7856 | `				int iLocal = 0;` |
|      ! 0 |  7857 | `				while( p < pGen->pEnd ){` |
|      ! 0 |  7858 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|      ! 0 |  7859 | `						break; /* body brace */` |
|        - |  7860 | `					}` |
|      ! 0 |  7861 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      ! 0 |  7862 | `						iLocal++;` |
|      ! 0 |  7863 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      ! 0 |  7864 | `						if( iLocal > 0 ){` |
|      ! 0 |  7865 | `							iLocal--;` |
|      ! 0 |  7866 | `						}` |
|      ! 0 |  7867 | `					}` |
|      ! 0 |  7868 | `					p++;` |
|      ! 0 |  7869 | `				}` |
|      ! 0 |  7870 | `				if( p < pGen->pEnd ){` |
|      ! 0 |  7871 | `					int iBrace = 0; /* p is on the body '{' */` |
|      ! 0 |  7872 | `					while( p < pGen->pEnd ){` |
|      ! 0 |  7873 | `						if( p->nType & PH7_TK_OCB ){` |
|      ! 0 |  7874 | `							iBrace++;` |
|      ! 0 |  7875 | `						}else if( p->nType & PH7_TK_CCB ){` |
|      ! 0 |  7876 | `							iBrace--;` |
|      ! 0 |  7877 | `							if( iBrace == 0 ){` |
|      ! 0 |  7878 | `								p++;` |
|      ! 0 |  7879 | `								break;` |
|        - |  7880 | `							}` |
|      ! 0 |  7881 | `						}` |
|      ! 0 |  7882 | `						p++;` |
|      ! 0 |  7883 | `					}` |
|      ! 0 |  7884 | `				}` |
|        - |  7885 | `			}` |
|        3 |  7886 | `			continue;` |
|        - |  7887 | `		}` |
|   331921 |  7888 | `		if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     7845 |  7889 | `			iDepth++;` |
|   328001 |  7890 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     7843 |  7891 | `			if( iDepth > 0 ){` |
|     7843 |  7892 | `				iDepth--;` |
|     3919 |  7893 | `			}` |
|   320162 |  7894 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|    86153 |  7895 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|        - |  7896 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|        - |  7897 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|        - |  7898 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|       11 |  7899 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|       11 |  7900 | `				return 1;` |
|        - |  7901 | `			}` |
|      ! 0 |  7902 | `		}` |
|   331913 |  7903 | `		p++;` |
|        5 |  7904 | `	}` |
|   229927 |  7905 | `	return 0;` |
|   114970 |  7906 | `}` |
|        - |  7907 | `/*` |
|        - |  7908 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|        - |  7909 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|        - |  7910 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|        - |  7911 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|        - |  7912 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|        - |  7913 | ` * share the same backing.` |
|        - |  7914 | ` */` |
|      238 |  7915 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|        - |  7916 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|        5 |  7917 | `{` |
|      243 |  7918 | `	pAttr->nType = nType;` |
|      243 |  7919 | `	pAttr->sClass = *pClass;` |
|      243 |  7920 | `	pAttr->sTypeName = *pTypeName;` |
|      243 |  7921 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|        - |  7922 | `		sxu32 i;` |
|       73 |  7923 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|       51 |  7924 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|       51 |  7925 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|       28 |  7926 | `		}` |
|       11 |  7927 | `	}` |
|      243 |  7928 | `}` |
|   143884 |  7929 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 |  7930 | `{` |
|   143889 |  7931 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  7932 | `	SySet *pInstrContainer;` |
|        - |  7933 | `	ph7_class_attr *pCons;` |
|        - |  7934 | `	SyString *pName;` |
|        - |  7935 | `	sxi32 rc;` |
|   143889 |  7936 | `	sxu32 nType = 0;` |
|        - |  7937 | `	SyString sTypeClass;` |
|        - |  7938 | `	SyString sTypeText;` |
|        - |  7939 | `	SySet aUnionAlts;` |
|   143889 |  7940 | `	sxi32 iTypeFlags = 0;` |
|   143889 |  7941 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   143889 |  7942 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   143889 |  7943 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  7944 | `	/* Extract visibility level */` |
|   143889 |  7945 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - |  7946 | `	/* Mark as constant */` |
|   143889 |  7947 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|   143889 |  7948 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        - |  7949 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|        - |  7950 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|   143908 |  7951 | `	if( GenStateClassConstHasType(pGen) ){` |
|       61 |  7952 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|       38 |  7953 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|        - |  7954 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|        - |  7955 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|        - |  7956 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|        - |  7957 | `		 * and success paths release. */` |
|       42 |  7958 | `		if( rc == SXERR_CORRUPT ){` |
|        - |  7959 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|      ! 0 |  7960 | `			goto Synchronize;` |
|       42 |  7961 | `		}else if( rc == SXERR_ABORT ){` |
|      ! 0 |  7962 | `			return SXERR_ABORT;` |
|       42 |  7963 | `		}else if( rc != SXRET_OK ){` |
|      ! 0 |  7964 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  7965 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|      ! 0 |  7966 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  7967 | `				return SXERR_ABORT;` |
|        - |  7968 | `			}` |
|      ! 0 |  7969 | `			goto Synchronize;` |
|        - |  7970 | `		}` |
|       42 |  7971 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|       19 |  7972 | `	}` |
|    71942 |  7973 | `loop:` |
|   143891 |  7974 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|        - |  7975 | `		/* Invalid constant name */` |
|      ! 0 |  7976 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|      ! 0 |  7977 | `		if( rc == SXERR_ABORT ){` |
|        - |  7978 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7979 | `			return SXERR_ABORT;` |
|        - |  7980 | `		}` |
|      ! 0 |  7981 | `		goto Synchronize;` |
|        - |  7982 | `	}` |
|        - |  7983 | `	/* Peek constant name */` |
|   143891 |  7984 | `	pName = &pGen->pIn->sData;` |
|        - |  7985 | `	/* Make sure the constant name isn't reserved */` |
|   143891 |  7986 | `	if( GenStateIsReservedConstant(pName) ){` |
|        - |  7987 | `		/* Reserved constant name */` |
|      ! 0 |  7988 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|      ! 0 |  7989 | `		if( rc == SXERR_ABORT ){` |
|        - |  7990 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  7991 | `			return SXERR_ABORT;` |
|        - |  7992 | `		}` |
|      ! 0 |  7993 | `		goto Synchronize;` |
|        - |  7994 | `	}` |
|        - |  7995 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|   143891 |  7996 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       61 |  7997 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|       38 |  7998 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|       19 |  7999 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|       42 |  8000 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8001 | `			return SXERR_ABORT;` |
|       42 |  8002 | `		}else if( rc != SXRET_OK ){` |
|        3 |  8003 | `			goto Synchronize;` |
|        - |  8004 | `		}` |
|       18 |  8005 | `	}` |
|        - |  8006 | `	/* Advance the stream cursor */` |
|   143889 |  8007 | `	pGen->pIn++;` |
|   143889 |  8008 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|        - |  8009 | `		/* Invalid declaration */` |
|      ! 0 |  8010 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|      ! 0 |  8011 | `		if( rc == SXERR_ABORT ){` |
|        - |  8012 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8013 | `			return SXERR_ABORT;` |
|        - |  8014 | `		}` |
|      ! 0 |  8015 | `		goto Synchronize;` |
|        - |  8016 | `	}` |
|   143889 |  8017 | `	pGen->pIn++; /* Jump the equal sign */` |
|        - |  8018 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|        - |  8019 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|        - |  8020 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|        - |  8021 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|   143884 |  8022 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|       39 |  8023 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|        8 |  8024 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8025 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|        2 |  8026 | `			&pClass->sName,pName,&sTypeText);` |
|        6 |  8027 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8028 | `			return SXERR_ABORT;` |
|        - |  8029 | `		}` |
|        6 |  8030 | `		goto Synchronize;` |
|        - |  8031 | `	}` |
|        - |  8032 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|        - |  8033 | `	 * constant initializer ("New expressions are not supported in this context").` |
|        - |  8034 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|   143885 |  8035 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|        5 |  8036 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8037 | `			"New expressions are not supported in this context");` |
|        5 |  8038 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8039 | `			return SXERR_ABORT;` |
|        - |  8040 | `		}` |
|        5 |  8041 | `		goto Synchronize;` |
|        - |  8042 | `	}` |
|        - |  8043 | `	/* Allocate a new class attribute */` |
|   143881 |  8044 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   143881 |  8045 | `	if( pCons ){` |
|   143881 |  8046 | `		GenStateConsumeDoc(&(*pGen),&pCons->sDoc);` |
|   143881 |  8047 | `		if( GenStateConsumeAttrs(&(*pGen),&pCons->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  8048 | `			return SXERR_ABORT;` |
|        - |  8049 | `		}` |
|    71938 |  8050 | `	}` |
|   143881 |  8051 | `	if( pCons == 0 ){` |
|      ! 0 |  8052 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8053 | `		return SXERR_ABORT;` |
|        - |  8054 | `	}` |
|   143881 |  8055 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|       35 |  8056 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|       16 |  8057 | `	}` |
|        - |  8058 | `	/* Swap bytecode container */` |
|   143881 |  8059 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   143881 |  8060 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|        - |  8061 | `	/* Compile constant value.` |
|        - |  8062 | `	 */` |
|   143881 |  8063 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   143881 |  8064 | `	if( rc == SXERR_EMPTY ){` |
|        3 |  8065 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|        3 |  8066 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8067 | `			return SXERR_ABORT;` |
|        - |  8068 | `		}` |
|        1 |  8069 | `	}` |
|        - |  8070 | `	/* Emit the done instruction */` |
|   143881 |  8071 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   143881 |  8072 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   143881 |  8073 | `	if( rc == SXERR_ABORT ){` |
|        - |  8074 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  8075 | `		return SXERR_ABORT;` |
|        - |  8076 | `	}` |
|        - |  8077 | `	/* All done,install the constant */` |
|   143881 |  8078 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|   143881 |  8079 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8080 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8081 | `		return SXERR_ABORT;` |
|        - |  8082 | `	}` |
|   143881 |  8083 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  8084 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|        3 |  8085 | `		pGen->pIn++; /* Jump the comma */` |
|        3 |  8086 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 |  8087 | `			SyToken *pTok = pGen->pIn;` |
|      ! 0 |  8088 | `			if( pTok >= pGen->pEnd ){` |
|      ! 0 |  8089 | `				pTok--;` |
|      ! 0 |  8090 | `			}` |
|      ! 0 |  8091 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  8092 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|      ! 0 |  8093 | `				&pTok->sData,&pClass->sName);` |
|      ! 0 |  8094 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8095 | `				return SXERR_ABORT;` |
|        - |  8096 | `			}` |
|      ! 0 |  8097 | `		}else{` |
|        3 |  8098 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|        3 |  8099 | `				goto loop;` |
|        - |  8100 | `			}` |
|        - |  8101 | `		}` |
|      ! 0 |  8102 | `	}` |
|   143879 |  8103 | `	SySetRelease(&aUnionAlts);` |
|   143879 |  8104 | `	return SXRET_OK;` |
|        5 |  8105 | `Synchronize:` |
|       13 |  8106 | `	SySetRelease(&aUnionAlts);` |
|        - |  8107 | `	/* Synchronize with the first semi-colon */` |
|       45 |  8108 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       35 |  8109 | `		pGen->pIn++;` |
|        3 |  8110 | `	}` |
|       13 |  8111 | `	return SXERR_CORRUPT;` |
|    71947 |  8112 | `}` |
|        - |  8113 | `/*` |
|        - |  8114 | ` * complie a class attribute or Properties in the PHP jargon.` |
|        - |  8115 | ` * According to the PHP language reference manual` |
|        - |  8116 | ` *  Properties` |
|        - |  8117 | ` *  Class member variables are called "properties". You may also see them referred` |
|        - |  8118 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|        - |  8119 | ` *  of this reference we will use "properties". They are defined by using one` |
|        - |  8120 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|        - |  8121 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|        - |  8122 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|        - |  8123 | ` *  and must not depend on run-time information in order to be evaluated.` |
|        - |  8124 | ` * Symisc eXtension.` |
|        - |  8125 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|        - |  8126 | ` *  the zend engine would allow only simple scalar value.` |
|        - |  8127 | ` *  Example:` |
|        - |  8128 | ` *   class Test{` |
|        - |  8129 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - |  8130 | ` *   };` |
|        - |  8131 | ` *   var_dump(TEST::myVar);` |
|        - |  8132 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - |  8133 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - |  8134 | ` */` |
|        - |  8135 | `/*` |
|        - |  8136 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|        - |  8137 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|        - |  8138 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|        - |  8139 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|        - |  8140 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|        - |  8141 | ` */` |
|  1318152 |  8142 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|        5 |  8143 | `{` |
|  1318157 |  8144 | `	SyToken *p = pStart;` |
|  1318157 |  8145 | `	int bFirst = 1;` |
|  1318157 |  8146 | `	if( p >= pEnd ) return 0;` |
|        - |  8147 | ``	/* Optional nullable `?` shorthand. */`` |
|  1318157 |  8148 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|       25 |  8149 | `		p++;` |
|       25 |  8150 | `		if( p >= pEnd ) return 0;` |
|       11 |  8151 | `	}` |
|        - |  8152 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|        - |  8153 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|        - |  8154 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|        - |  8155 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   659076 |  8156 | `	for(;;){` |
|  1318177 |  8157 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|        - |  8158 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|        3 |  8159 | `			p++;` |
|        9 |  8160 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|        3 |  8161 | `			if( p >= pEnd ) return 0;` |
|        3 |  8162 | `			p++; /* skip ')' */` |
|        2 |  8163 | `		}else{` |
|        - |  8164 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|        - |  8165 | ``			 * then any `&`-joined intersection members. */`` |
|  1318175 |  8166 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|  1318175 |  8167 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  8168 | `				return 0;` |
|        - |  8169 | `			}` |
|        - |  8170 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|        - |  8171 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|        - |  8172 | `			 * may still appear at the initial dispatch site). */` |
|  1318175 |  8173 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|  1318127 |  8174 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  1318122 |  8175 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    23604 |  8176 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  1317947 |  8177 | `					return 0;` |
|        - |  8178 | `				}` |
|       90 |  8179 | `			}` |
|      233 |  8180 | `			p++;` |
|      235 |  8181 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  8182 | `				p += 2;` |
|        1 |  8183 | `			}` |
|      345 |  8184 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|      236 |  8185 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        3 |  8186 | `				p++; /* skip '&' */` |
|        3 |  8187 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|        3 |  8188 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|        3 |  8189 | `				p++;` |
|        3 |  8190 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      ! 0 |  8191 | `					p += 2;` |
|      ! 0 |  8192 | `				}` |
|        1 |  8193 | `			}` |
|        - |  8194 | `		}` |
|      235 |  8195 | `		bFirst = 0;` |
|      230 |  8196 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|       25 |  8197 | `			&& p->sData.zString[0] == '\|' ){` |
|       25 |  8198 | ``			p++; /* next `\|`-separated part */`` |
|       25 |  8199 | `			continue;` |
|        - |  8200 | `		}` |
|      215 |  8201 | `		break;` |
|      ! 0 |  8202 | `	}` |
|      215 |  8203 | `	if( p >= pEnd ) return 0;` |
|      215 |  8204 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   659081 |  8205 | `}` |
|        - |  8206 |  |
|        - |  8207 | `/*` |
|        - |  8208 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|        - |  8209 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|        - |  8210 | ` * if not). Recognized forms:` |
|        - |  8211 | ` *   ?Type, array, bool, int, float, string, object,` |
|        - |  8212 | ` *   self, parent, \Ns\ClassName, ClassName` |
|        - |  8213 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|        - |  8214 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|        - |  8215 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|        - |  8216 | ` * on unrecoverable error.` |
|        - |  8217 | ` *` |
|        - |  8218 | ` * When a type is parsed:` |
|        - |  8219 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|        - |  8220 | ` *   *pClass is set to the class name (for class types)` |
|        - |  8221 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|        - |  8222 | ` *   *pTypeText is set to the original text span of the type` |
|        - |  8223 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|        - |  8224 | ` */` |
|      210 |  8225 | `static sxi32 GenStateParsePropertyType(` |
|        - |  8226 | `	ph7_gen_state *pGen,` |
|        - |  8227 | `	sxu32 *pnType,` |
|        - |  8228 | `	SyString *pClass,` |
|        - |  8229 | `	sxi32 *piTypeFlags,` |
|        - |  8230 | `	SyString *pTypeText,` |
|        - |  8231 | `	SySet *pAlts` |
|        5 |  8232 | `){` |
|      215 |  8233 | `	sxi32 iFlags = 0;` |
|        - |  8234 | `	sxi32 rc;` |
|      215 |  8235 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  8236 | `		return SXRET_OK;` |
|        - |  8237 | `	}` |
|        - |  8238 | `	/* If the first token is '$', there's no type */` |
|      215 |  8239 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      ! 0 |  8240 | `		return SXRET_OK;` |
|        - |  8241 | `	}` |
|      215 |  8242 | `	rc = GenStateParseUnionTypeDecl(` |
|      105 |  8243 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|        - |  8244 | `		PH7_CLASS_ATTR_NULLABLE,` |
|        - |  8245 | `		PH7_CLASS_ATTR_UNION,` |
|        - |  8246 | `		/* bAllowVoid */ 0,` |
|      210 |  8247 | `		pGen->pIn->nLine);` |
|      215 |  8248 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8249 | `		return rc;` |
|        - |  8250 | `	}` |
|        - |  8251 | `	/* Verify next token is '$' (start of property name) */` |
|      215 |  8252 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  8253 | `		return SXERR_SYNTAX;` |
|        - |  8254 | `	}` |
|      215 |  8255 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|      215 |  8256 | `	return SXRET_OK;` |
|      110 |  8257 | `}` |
|        - |  8258 |  |
|        - |  8259 | `/*` |
|        - |  8260 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|        - |  8261 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|        - |  8262 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|        - |  8263 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|        - |  8264 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|        - |  8265 | ` * by the type parser itself before reaching here.` |
|        - |  8266 | ` *` |
|        - |  8267 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|        - |  8268 | ` * use in the error message.` |
|        - |  8269 | ` */` |
|      386 |  8270 | `static int GenStateIsDisallowedPropertyAtom(` |
|        - |  8271 | `	sxu32 nType,` |
|        - |  8272 | `	const SyString *pClass,` |
|        - |  8273 | `	const char **pzName,` |
|        - |  8274 | `	sxu32 *pnName)` |
|        5 |  8275 | `{` |
|        - |  8276 | `	const char *z;` |
|        - |  8277 | `	sxu32 n;` |
|      391 |  8278 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|      337 |  8279 | `		return 0;` |
|        - |  8280 | `	}` |
|       59 |  8281 | `	z = pClass->zString;` |
|       59 |  8282 | `	n = pClass->nByte;` |
|       59 |  8283 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|        8 |  8284 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|        - |  8285 | `	}` |
|        - |  8286 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|        - |  8287 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|        - |  8288 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|       52 |  8289 | `	return 0;` |
|      198 |  8290 | `}` |
|        - |  8291 |  |
|        - |  8292 | `/*` |
|        - |  8293 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|        - |  8294 | ` * constant) — the main atom plus any union alternatives — against the` |
|        - |  8295 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|        - |  8296 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|        - |  8297 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|        - |  8298 | ` * type T" vs "Class constant C::X cannot have type T").` |
|        - |  8299 | ` *` |
|        - |  8300 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|        - |  8301 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|        - |  8302 | ` */` |
|      324 |  8303 | `static sxi32 GenStateValidateMemberType(` |
|        - |  8304 | `	ph7_gen_state *pGen,` |
|        - |  8305 | `	ph7_class *pClass,` |
|        - |  8306 | `	const SyString *pMemberName,` |
|        - |  8307 | `	sxu32 nType,` |
|        - |  8308 | `	const SyString *pTypeClass,` |
|        - |  8309 | `	const SyString *pTypeText,` |
|        - |  8310 | `	SySet *pUnionAlts,` |
|        - |  8311 | `	const char *zErrFmt,` |
|        - |  8312 | `	sxu32 nLine)` |
|        5 |  8313 | `{` |
|      329 |  8314 | `	const char *zBad = 0;` |
|      329 |  8315 | `	sxu32 nBad = 0;` |
|        - |  8316 | `	SyString sFallback;` |
|        - |  8317 | `	const SyString *pBad;` |
|        - |  8318 | `	sxi32 rc;` |
|      329 |  8319 | `	int bDisallowed = 0;` |
|      329 |  8320 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|        5 |  8321 | `		bDisallowed = 1;` |
|      327 |  8322 | `	}else if( pUnionAlts ){` |
|        - |  8323 | `		sxu32 i;` |
|       95 |  8324 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|       67 |  8325 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|       67 |  8326 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|        3 |  8327 | `				bDisallowed = 1;` |
|        3 |  8328 | `				break;` |
|        - |  8329 | `			}` |
|       35 |  8330 | `		}` |
|       15 |  8331 | `	}` |
|      329 |  8332 | `	if( !bDisallowed ){` |
|      323 |  8333 | `		return SXRET_OK;` |
|        - |  8334 | `	}` |
|        - |  8335 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|        - |  8336 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|        - |  8337 | `	 * canonical spelling if the type text is unavailable. */` |
|        8 |  8338 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|        8 |  8339 | `		pBad = pTypeText;` |
|        5 |  8340 | `	}else{` |
|      ! 0 |  8341 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|      ! 0 |  8342 | `		pBad = &sFallback;` |
|        - |  8343 | `	}` |
|       11 |  8344 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        3 |  8345 | `		zErrFmt,` |
|        3 |  8346 | `		&pClass->sName,pMemberName,pBad);` |
|        8 |  8347 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  8348 | `		return SXERR_ABORT;` |
|        - |  8349 | `	}` |
|        8 |  8350 | `	return SXERR_SYNTAX;` |
|      167 |  8351 | `}` |
|        - |  8352 | `/*` |
|        - |  8353 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|        - |  8354 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|        - |  8355 | ` * matched as a plain identifier in the class-member modifier position rather` |
|        - |  8356 | ` * than promoted to a lexer keyword.` |
|        - |  8357 | ` */` |
| 10165708 |  8358 | `static int GenStateIsReadonly(SyToken *pTok)` |
|        5 |  8359 | `{` |
| 10206853 |  8360 | `	return (pTok->nType & PH7_TK_ID)` |
|  5123994 |  8361 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 10206848 |  8362 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|        5 |  8363 | `}` |
|        - |  8364 | `/*` |
|        - |  8365 | ``  * Detect an asymmetric set-visibility modifier `public(set)` / `protected(set)` `` |
|        - |  8366 | `` * / `private(set)` (PHP 8.4) starting at pTok. Returns the visibility keyword id`` |
|        - |  8367 | ` * (PH7_TKWRD_*) and sets *pnTok to the 4 tokens consumed, or 0 when not present` |
|        - |  8368 | ` * (a bare visibility keyword is NOT a set-modifier; the '(' 'set' ')' run is).` |
|        - |  8369 | ` */` |
|  3750924 |  8370 | `static sxi32 GenStatePeekSetVisibility(SyToken *pTok,SyToken *pEnd,int *pnTok)` |
|        5 |  8371 | `{` |
|  3750929 |  8372 | `	*pnTok = 0;` |
|  3750924 |  8373 | `	if( &pTok[3] < pEnd` |
|  3579201 |  8374 | `	 && (pTok->nType & PH7_TK_KEYWORD)` |
|  3138666 |  8375 | `	 && (pTok[1].nType & PH7_TK_LPAREN)` |
|  1434935 |  8376 | `	 && (pTok[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|       16 |  8377 | `	 && pTok[2].sData.nByte == sizeof("set")-1` |
|       16 |  8378 | `	 && SyStrnicmp(pTok[2].sData.zString,"set",sizeof("set")-1) == 0` |
|       21 |  8379 | `	 && (pTok[3].nType & PH7_TK_RPAREN) ){` |
|       17 |  8380 | `		sxi32 nKw = SX_PTR_TO_INT(pTok->pUserData);` |
|       17 |  8381 | `		if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PRIVATE \|\| nKw == PH7_TKWRD_PROTECTED ){` |
|       17 |  8382 | `			*pnTok = 4;` |
|       17 |  8383 | `			return nKw;` |
|        - |  8384 | `		}` |
|      ! 0 |  8385 | `	}` |
|  3750913 |  8386 | `	return 0;` |
|  1875467 |  8387 | `}` |
|        - |  8388 | `/* Map a set-visibility keyword to its PH7_CLASS_ATTR_* flag. */` |
|       16 |  8389 | `static sxi32 GenStateSetVisFlag(sxi32 nKw)` |
|        1 |  8390 | `{` |
|       17 |  8391 | `	if( nKw == PH7_TKWRD_PRIVATE ){` |
|       13 |  8392 | `		return PH7_CLASS_ATTR_PRIVATE_SET;` |
|        - |  8393 | `	}` |
|        5 |  8394 | `	if( nKw == PH7_TKWRD_PROTECTED ){` |
|        3 |  8395 | `		return PH7_CLASS_ATTR_PROTECTED_SET;` |
|        - |  8396 | `	}` |
|        3 |  8397 | `	return PH7_CLASS_ATTR_PUBLIC_SET;` |
|        9 |  8398 | `}` |
|   210578 |  8399 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|        5 |  8400 | `{` |
|   210583 |  8401 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  8402 | `	ph7_class_attr *pAttr;` |
|        - |  8403 | `	SyString *pName;` |
|        - |  8404 | `	sxi32 rc;` |
|   210583 |  8405 | `	sxu32 nType = 0;` |
|        - |  8406 | `	SyString sTypeClass;` |
|        - |  8407 | `	SyString sTypeText;` |
|        - |  8408 | `	SySet aUnionAlts;` |
|   210583 |  8409 | `	sxi32 iTypeFlags = 0;` |
|   210583 |  8410 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   210583 |  8411 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   210583 |  8412 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|        - |  8413 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|        - |  8414 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|        - |  8415 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   210583 |  8416 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|       21 |  8417 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|        9 |  8418 | `	}` |
|        - |  8419 | `	/* Extract visibility level */` |
|   210583 |  8420 | `	iProtection = GetProtectionLevel(iProtection);` |
|        - |  8421 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   210688 |  8422 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      215 |  8423 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|      215 |  8424 | `		if( rc == SXERR_CORRUPT ){` |
|        - |  8425 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|      ! 0 |  8426 | `			goto Synchronize;` |
|      215 |  8427 | `		}else if( rc == SXERR_SYNTAX ){` |
|      ! 0 |  8428 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8429 | `				"Invalid property type or declaration near '%z'",` |
|      ! 0 |  8430 | `				&pGen->pIn->sData);` |
|      ! 0 |  8431 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8432 | `				return SXERR_ABORT;` |
|        - |  8433 | `			}` |
|      ! 0 |  8434 | `			goto Synchronize;` |
|      215 |  8435 | `		}else if( rc == SXERR_ABORT ){` |
|      ! 0 |  8436 | `			return SXERR_ABORT;` |
|        - |  8437 | `		}` |
|      105 |  8438 | `	}` |
|      ! 0 |  8439 | `loop:` |
|   210587 |  8440 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 |  8441 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|      ! 0 |  8442 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8443 | `			return SXERR_ABORT;` |
|        - |  8444 | `		}` |
|      ! 0 |  8445 | `		goto Synchronize;` |
|        - |  8446 | `	}` |
|   210587 |  8447 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   210587 |  8448 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        - |  8449 | `		/* Invalid attribute name */` |
|      ! 0 |  8450 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|      ! 0 |  8451 | `		if( rc == SXERR_ABORT ){` |
|        - |  8452 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8453 | `			return SXERR_ABORT;` |
|        - |  8454 | `		}` |
|      ! 0 |  8455 | `		goto Synchronize;` |
|        - |  8456 | `	}` |
|        - |  8457 | `	/* Peek attribute name */` |
|   210587 |  8458 | `	pName = &pGen->pIn->sData;` |
|        - |  8459 | `	/* Advance the stream cursor */` |
|   210587 |  8460 | `	pGen->pIn++;` |
|   210587 |  8461 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|        - |  8462 | `		/* Invalid declaration */` |
|        3 |  8463 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|        3 |  8464 | `		if( rc == SXERR_ABORT ){` |
|        - |  8465 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8466 | `			return SXERR_ABORT;` |
|        - |  8467 | `		}` |
|        3 |  8468 | `		goto Synchronize;` |
|        - |  8469 | `	}` |
|        - |  8470 | `	/* Asymmetric-visibility rules (PHP 8.4): the property must be typed, and` |
|        - |  8471 | `	 * the read visibility must not be narrower than the set visibility. */` |
|   210585 |  8472 | `	if( iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET\|PH7_CLASS_ATTR_PUBLIC_SET) ){` |
|       13 |  8473 | `		const char *zAvErr = 0;` |
|       19 |  8474 | `		sxi32 iSetLevel = (iFlags & PH7_CLASS_ATTR_PRIVATE_SET) ? PH7_CLASS_PROT_PRIVATE` |
|       10 |  8475 | `			: (iFlags & PH7_CLASS_ATTR_PROTECTED_SET) ? PH7_CLASS_PROT_PROTECTED` |
|        2 |  8476 | `			: PH7_CLASS_PROT_PUBLIC;` |
|       13 |  8477 | `		if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  8478 | `			zAvErr = "Property with asymmetric visibility %z::$%z must have type";` |
|       13 |  8479 | `		}else if( iProtection > iSetLevel ){` |
|      ! 0 |  8480 | `			zAvErr = "Visibility of property %z::$%z must not be weaker than set visibility";` |
|      ! 0 |  8481 | `		}` |
|       13 |  8482 | `		if( zAvErr ){` |
|      ! 0 |  8483 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zAvErr,&pClass->sName,pName);` |
|      ! 0 |  8484 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8485 | `				return SXERR_ABORT;` |
|        - |  8486 | `			}` |
|      ! 0 |  8487 | `			goto Synchronize;` |
|        - |  8488 | `		}` |
|        6 |  8489 | `	}` |
|        - |  8490 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|        - |  8491 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   210585 |  8492 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|       43 |  8493 | `		const char *zRoErr = 0;` |
|       43 |  8494 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        3 |  8495 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|       42 |  8496 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        6 |  8497 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|       39 |  8498 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|        6 |  8499 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|        2 |  8500 | `		}` |
|       43 |  8501 | `		if( zRoErr ){` |
|       13 |  8502 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|       13 |  8503 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8504 | `				return SXERR_ABORT;` |
|        - |  8505 | `			}` |
|       13 |  8506 | `			goto Synchronize;` |
|        - |  8507 | `		}` |
|       14 |  8508 | `	}` |
|        - |  8509 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|        - |  8510 | `	 * type atom or any union alternative. void/never are already rejected` |
|        - |  8511 | `	 * by the type parser. */` |
|   210575 |  8512 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      317 |  8513 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|        - |  8514 | `			&sTypeText,` |
|      208 |  8515 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      104 |  8516 | `			"Property %z::$%z cannot have type %z",nLine);` |
|      213 |  8517 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8518 | `			return SXERR_ABORT;` |
|      213 |  8519 | `		}else if( rc != SXRET_OK ){` |
|      ! 0 |  8520 | `			goto Synchronize;` |
|        - |  8521 | `		}` |
|      104 |  8522 | `	}` |
|        - |  8523 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   210575 |  8524 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|        4 |  8525 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  8526 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|        3 |  8527 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8528 | `			return SXERR_ABORT;` |
|        - |  8529 | `		}` |
|        3 |  8530 | `		goto Synchronize;` |
|        - |  8531 | `	}` |
|        - |  8532 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|        - |  8533 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|        - |  8534 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|        - |  8535 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|        - |  8536 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|        - |  8537 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|   210573 |  8538 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|        6 |  8539 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8540 | `			"New expressions are not supported in this context");` |
|        6 |  8541 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8542 | `			return SXERR_ABORT;` |
|        - |  8543 | `		}` |
|        6 |  8544 | `		goto Synchronize;` |
|        - |  8545 | `	}` |
|        - |  8546 | `	/* Allocate a new class attribute */` |
|   210569 |  8547 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   210569 |  8548 | `	if( pAttr ){` |
|   210569 |  8549 | `		GenStateConsumeDoc(&(*pGen),&pAttr->sDoc);` |
|   210569 |  8550 | `		if( GenStateConsumeAttrs(&(*pGen),&pAttr->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  8551 | `			return SXERR_ABORT;` |
|        - |  8552 | `		}` |
|   105282 |  8553 | `	}` |
|   210569 |  8554 | `	if( pAttr == 0 ){` |
|      ! 0 |  8555 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  8556 | `		return SXERR_ABORT;` |
|        - |  8557 | `	}` |
|   210569 |  8558 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      211 |  8559 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      103 |  8560 | `	}` |
|   210569 |  8561 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|        - |  8562 | `		SySet *pInstrContainer;` |
|    86051 |  8563 | `		pGen->pIn++; /*Jump the equal sign */` |
|        - |  8564 | `		/* Swap bytecode container */` |
|    86051 |  8565 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    86051 |  8566 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|        - |  8567 | `		/* Compile attribute value.` |
|        - |  8568 | `		 */` |
|    86051 |  8569 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    86051 |  8570 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 |  8571 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|      ! 0 |  8572 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8573 | `				return SXERR_ABORT;` |
|        - |  8574 | `			}` |
|      ! 0 |  8575 | `		}` |
|        - |  8576 | `		/* Emit the done instruction */` |
|    86051 |  8577 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    86051 |  8578 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    43023 |  8579 | `	}` |
|        - |  8580 | `	/* All done,install the attribute */` |
|   210569 |  8581 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   210569 |  8582 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8583 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8584 | `		return SXERR_ABORT;` |
|        - |  8585 | `	}` |
|   210569 |  8586 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|        - |  8587 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|        5 |  8588 | `		pGen->pIn++; /* Jump the comma */` |
|        5 |  8589 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|      ! 0 |  8590 | `			SyToken *pTok = pGen->pIn;` |
|      ! 0 |  8591 | `			if( pTok >= pGen->pEnd ){` |
|      ! 0 |  8592 | `				pTok--;` |
|      ! 0 |  8593 | `			}` |
|      ! 0 |  8594 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  8595 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|      ! 0 |  8596 | `				&pTok->sData,&pClass->sName);` |
|      ! 0 |  8597 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8598 | `				return SXERR_ABORT;` |
|        - |  8599 | `			}` |
|      ! 0 |  8600 | `		}else{` |
|        5 |  8601 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        5 |  8602 | `				goto loop;` |
|        - |  8603 | `			}` |
|        - |  8604 | `		}` |
|      ! 0 |  8605 | `	}` |
|   210565 |  8606 | `	SySetRelease(&aUnionAlts);` |
|   210565 |  8607 | `	return SXRET_OK;` |
|        9 |  8608 | `Synchronize:` |
|        - |  8609 | `	/* Synchronize with the first semi-colon */` |
|       56 |  8610 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       37 |  8611 | `		pGen->pIn++;` |
|        3 |  8612 | `	}` |
|       22 |  8613 | `	SySetRelease(&aUnionAlts);` |
|       22 |  8614 | `	return SXERR_CORRUPT;` |
|   105294 |  8615 | `}` |
|        - |  8616 | `/*` |
|        - |  8617 | ` * Compile a class method.` |
|        - |  8618 | ` *` |
|        - |  8619 | ` * Refer to the official documentation for more information` |
|        - |  8620 | ` * on the powerful extension introduced by the PH7 engine` |
|        - |  8621 | ` * to the OO subsystem such as full type hinting,method` |
|        - |  8622 | ` * overloading and many more.` |
|        - |  8623 | ` */` |
|  1395892 |  8624 | `static sxi32 GenStateCompileClassMethod(` |
|        - |  8625 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  8626 | `	sxi32 iProtection,   /* Visibility level */` |
|        - |  8627 | `	sxi32 iFlags,        /* Configuration flags */` |
|        - |  8628 | `	int doBody,          /* TRUE to process method body */` |
|        - |  8629 | `	ph7_class *pClass    /* Class this method belongs */` |
|        - |  8630 | `	)` |
|        5 |  8631 | `{` |
|  1395897 |  8632 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  1395897 |  8633 | `	sxu32 nKwLine = nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|        - |  8634 | `	ph7_class_method *pMeth;` |
|        - |  8635 | `	sxi32 iFuncFlags;` |
|        - |  8636 | `	SyString *pName;` |
|        - |  8637 | `	SyToken *pEnd;` |
|        - |  8638 | `	sxi32 rc;` |
|        - |  8639 | `	/* Extract visibility level */` |
|  1395897 |  8640 | `	iProtection = GetProtectionLevel(iProtection);` |
|  1395897 |  8641 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  1395897 |  8642 | `	iFuncFlags = 0;` |
|  1395897 |  8643 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  8644 | `		/* Invalid method name */` |
|      ! 0 |  8645 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|      ! 0 |  8646 | `		if( rc == SXERR_ABORT ){` |
|        - |  8647 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8648 | `			return SXERR_ABORT;` |
|        - |  8649 | `		}` |
|      ! 0 |  8650 | `		goto Synchronize;` |
|        - |  8651 | `	}` |
|  1395897 |  8652 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|        - |  8653 | `		/* Return by reference,remember that */` |
|      ! 0 |  8654 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|        - |  8655 | `		/* Jump the '&' token */` |
|      ! 0 |  8656 | `		pGen->pIn++;` |
|      ! 0 |  8657 | `	}` |
|  1395897 |  8658 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |  8659 | `		/* Invalid method name */` |
|      ! 0 |  8660 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|      ! 0 |  8661 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8662 | `			return SXERR_ABORT;` |
|        - |  8663 | `		}` |
|      ! 0 |  8664 | `		goto Synchronize;` |
|        - |  8665 | `	}` |
|        - |  8666 | `	/* Peek method name */` |
|  1395897 |  8667 | `	pName = &pGen->pIn->sData;` |
|  1395897 |  8668 | `	nLine = pGen->pIn->nLine;` |
|        - |  8669 | `	/* Jump the method name */` |
|  1395897 |  8670 | `	pGen->pIn++;` |
|  1395897 |  8671 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        - |  8672 | `		/* Abstract method */` |
|   101051 |  8673 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|      ! 0 |  8674 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8675 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|      ! 0 |  8676 | `				&pClass->sName,pName);` |
|      ! 0 |  8677 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8678 | `				return SXERR_ABORT;` |
|        - |  8679 | `			}` |
|      ! 0 |  8680 | `		}` |
|        - |  8681 | `		/* Assemble method signature only */` |
|   101051 |  8682 | `		doBody = FALSE;` |
|    50523 |  8683 | `	}` |
|  1395897 |  8684 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  8685 | `		/* Syntax error */` |
|      ! 0 |  8686 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|      ! 0 |  8687 | `		if( rc == SXERR_ABORT ){` |
|        - |  8688 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8689 | `			return SXERR_ABORT;` |
|        - |  8690 | `		}` |
|      ! 0 |  8691 | `		goto Synchronize;` |
|        - |  8692 | `	}` |
|        - |  8693 | `	/* Allocate a new class_method instance */` |
|  1395897 |  8694 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  1395897 |  8695 | `	if( pMeth == 0 ){` |
|      ! 0 |  8696 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8697 | `		return SXERR_ABORT;` |
|        - |  8698 | `	}` |
|  1395897 |  8699 | `	pMeth->sFunc.nLine = nKwLine;` |
|  1395897 |  8700 | `	GenStateConsumeDoc(&(*pGen),&pMeth->sFunc.sDoc);` |
|  1395897 |  8701 | `	if( GenStateConsumeAttrs(&(*pGen),&pMeth->sFunc.aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  8702 | `		return SXERR_ABORT;` |
|        - |  8703 | `	}` |
|        - |  8704 | `	/* Jump the left parenthesis '(' */` |
|  1395897 |  8705 | `	pGen->pIn++;` |
|  1395897 |  8706 | `	pEnd = 0; /* cc warning */` |
|        - |  8707 | `	/* Delimit the method signature */` |
|  1395897 |  8708 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  1395897 |  8709 | `	if( pEnd >= pGen->pEnd ){` |
|        - |  8710 | `		/* Syntax error */` |
|        3 |  8711 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|        3 |  8712 | `		if( rc == SXERR_ABORT ){` |
|        - |  8713 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8714 | `			return SXERR_ABORT;` |
|        - |  8715 | `		}` |
|        3 |  8716 | `		goto Synchronize;` |
|        - |  8717 | `	}` |
|        - |  8718 | `	{` |
|  1395895 |  8719 | `		int bIsCtor = 0;` |
|  1395895 |  8720 | `		int bAbstractCtor = 0;` |
|  1395890 |  8721 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|   814617 |  8722 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  1343357 |  8723 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   105081 |  8724 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|        3 |  8725 | `				bAbstractCtor = 1;` |
|        2 |  8726 | `			}else{` |
|   105079 |  8727 | `				bIsCtor = 1;` |
|        - |  8728 | `			}` |
|    52538 |  8729 | `		}` |
|  1395895 |  8730 | `		if( pGen->pIn < pEnd ){` |
|        - |  8731 | `			/* Collect method arguments */` |
|   389061 |  8732 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   389061 |  8733 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8734 | `				return SXERR_ABORT;` |
|        - |  8735 | `			}` |
|   194528 |  8736 | `		}` |
|        - |  8737 | `	}` |
|        - |  8738 | `	/* Point past ')' and parse optional return type ': type' */` |
|  1395895 |  8739 | `	pGen->pIn = &pEnd[1];` |
|        - |  8740 | `	{` |
|  1395895 |  8741 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  1395895 |  8742 | `		if( rcRt == SXERR_ABORT ){` |
|      ! 0 |  8743 | `			return SXERR_ABORT;` |
|  1395895 |  8744 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|      ! 0 |  8745 | `			goto Synchronize;` |
|        - |  8746 | `		}` |
|        - |  8747 | `	}` |
|        - |  8748 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|        - |  8749 | `	 * property init/typecheck is handled by the generic typed-property path` |
|        - |  8750 | `	 * since we mint real ph7_class_attr entries. */` |
|        - |  8751 | `	{` |
|  1395895 |  8752 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|        - |  8753 | `		sxu32 i;` |
|  1979315 |  8754 | `		for( i = 0; i < nArg; i++ ){` |
|   583435 |  8755 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|        - |  8756 | `			ph7_class_attr *pAttr;` |
|   583435 |  8757 | `			sxi32 iAttrFlags = 0;` |
|        - |  8758 | `			int bArgTyped;` |
|   583435 |  8759 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   583351 |  8760 | `				continue;` |
|        - |  8761 | `			}` |
|        - |  8762 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|        - |  8763 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|        - |  8764 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|       59 |  8765 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|       90 |  8766 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|       89 |  8767 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        3 |  8768 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8769 | `					"Cannot declare variadic promoted property");` |
|        3 |  8770 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8771 | `					return SXERR_ABORT;` |
|        - |  8772 | `				}` |
|        3 |  8773 | `				goto Synchronize;` |
|        - |  8774 | `			}` |
|        - |  8775 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|        - |  8776 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|        - |  8777 | `			 * appear as an alternative of a union type. */` |
|       87 |  8778 | `			if( bArgTyped ){` |
|      122 |  8779 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|       78 |  8780 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|       78 |  8781 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|       39 |  8782 | `					"Property %z::$%z cannot have type %z",nLine);` |
|       83 |  8783 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8784 | `					return SXERR_ABORT;` |
|       83 |  8785 | `				}else if( rc != SXRET_OK ){` |
|        6 |  8786 | `					goto Synchronize;` |
|        - |  8787 | `				}` |
|       37 |  8788 | `			}` |
|        - |  8789 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|       83 |  8790 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|        4 |  8791 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  8792 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|        3 |  8793 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8794 | `					return SXERR_ABORT;` |
|        - |  8795 | `				}` |
|        3 |  8796 | `				goto Synchronize;` |
|        - |  8797 | `			}` |
|       81 |  8798 | `			if( bArgTyped ){` |
|       77 |  8799 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|       36 |  8800 | `			}` |
|       81 |  8801 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|        3 |  8802 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|        1 |  8803 | `			}` |
|       81 |  8804 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|        8 |  8805 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|        3 |  8806 | `			}` |
|       81 |  8807 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|        - |  8808 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|        - |  8809 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|       26 |  8810 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|        4 |  8811 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  8812 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|        3 |  8813 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  8814 | `						return SXERR_ABORT;` |
|        - |  8815 | `					}` |
|        3 |  8816 | `					goto Synchronize;` |
|        - |  8817 | `				}` |
|       24 |  8818 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       10 |  8819 | `			}` |
|       79 |  8820 | `			if( pArg->iFlags & (VM_FUNC_ARG_PRIV_SET\|VM_FUNC_ARG_PROT_SET) ){` |
|        - |  8821 | `				/* Asymmetric set-visibility on a promoted property (PHP 8.4) */` |
|        5 |  8822 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  8823 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8824 | `						"Property with asymmetric visibility %z::$%z must have type",` |
|      ! 0 |  8825 | `						&pClass->sName,&pArg->sName);` |
|      ! 0 |  8826 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  8827 | `						return SXERR_ABORT;` |
|        - |  8828 | `					}` |
|      ! 0 |  8829 | `					goto Synchronize;` |
|        - |  8830 | `				}` |
|        5 |  8831 | `				iAttrFlags \|= (pArg->iFlags & VM_FUNC_ARG_PRIV_SET)` |
|        2 |  8832 | `					? PH7_CLASS_ATTR_PRIVATE_SET : PH7_CLASS_ATTR_PROTECTED_SET;` |
|        2 |  8833 | `			}` |
|       79 |  8834 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|       79 |  8835 | `			if( pAttr == 0 ){` |
|      ! 0 |  8836 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8837 | `				return SXERR_ABORT;` |
|        - |  8838 | `			}` |
|       79 |  8839 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|       77 |  8840 | `				pAttr->nType = pArg->nType;` |
|       77 |  8841 | `				pAttr->sClass = pArg->sClass;` |
|       77 |  8842 | `				pAttr->sTypeName = pArg->sTypeName;` |
|       77 |  8843 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|        - |  8844 | `					sxu32 k;` |
|       20 |  8845 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|       14 |  8846 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|       14 |  8847 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|        8 |  8848 | `					}` |
|        3 |  8849 | `				}` |
|       36 |  8850 | `			}` |
|       79 |  8851 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|       79 |  8852 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  8853 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8854 | `				return SXERR_ABORT;` |
|        - |  8855 | `			}` |
|       42 |  8856 | `		}` |
|        - |  8857 | `	}` |
|  1395885 |  8858 | `	if( doBody ){` |
|        - |  8859 | `		/* Compile method body */` |
|  1294839 |  8860 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  1294839 |  8861 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8862 | `			return SXERR_ABORT;` |
|        - |  8863 | `		}` |
|        - |  8864 | `		/* The cursor sits just past the body's closing brace */` |
|  1294839 |  8865 | `		pMeth->sFunc.nEndLine = pGen->pIn[-1].nLine;` |
|   647422 |  8866 | `	}else{` |
|        - |  8867 | `		/* Abstract/interface method: declaration ends at the ';' */` |
|   101051 |  8868 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) ){` |
|   101051 |  8869 | `			pMeth->sFunc.nEndLine = pGen->pIn->nLine;` |
|    50523 |  8870 | `		}` |
|        - |  8871 | `		/* Only method signature is allowed */` |
|   101051 |  8872 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|      ! 0 |  8873 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  8874 | `				"Expected ';' after method signature '%z'",pName);` |
|      ! 0 |  8875 | `				if( rc == SXERR_ABORT ){` |
|        - |  8876 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 |  8877 | `					return SXERR_ABORT;` |
|        - |  8878 | `				}` |
|      ! 0 |  8879 | `				return SXERR_CORRUPT;` |
|        - |  8880 | `			}` |
|        - |  8881 | `	}` |
|        - |  8882 | `	/* All done,install the method */` |
|  1395885 |  8883 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  1395885 |  8884 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8885 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8886 | `		return SXERR_ABORT;` |
|        - |  8887 | `	}` |
|  1395885 |  8888 | `	return SXRET_OK;` |
|        6 |  8889 | `Synchronize:` |
|        - |  8890 | `	/* Synchronize with the first semi-colon */` |
|       40 |  8891 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       28 |  8892 | `		pGen->pIn++;` |
|        4 |  8893 | `	}` |
|       16 |  8894 | `	return SXERR_CORRUPT;` |
|   697951 |  8895 | `}` |
|        - |  8896 | `/*` |
|        - |  8897 | ` * Compile an object interface.` |
|        - |  8898 | ` *  According to the PHP language reference manual` |
|        - |  8899 | ` *   Object Interfaces:` |
|        - |  8900 | ` *   Object interfaces allow you to create code which specifies which methods` |
|        - |  8901 | ` *   a class must implement, without having to define how these methods are handled.` |
|        - |  8902 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|        - |  8903 | ` *   class, but without any of the methods having their contents defined.` |
|        - |  8904 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|        - |  8905 | ` */` |
|    46708 |  8906 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|        5 |  8907 | `{` |
|    46713 |  8908 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  8909 | `	ph7_class *pClass,*pBase;` |
|        - |  8910 | `	SyToken *pEnd,*pTmp;` |
|        - |  8911 | `	SyString *pName;` |
|        - |  8912 | `	sxi32 nKwrd;` |
|        - |  8913 | `	sxi32 rc;` |
|        - |  8914 | `	/* Jump the 'interface' keyword */` |
|    46713 |  8915 | `	pGen->pIn++;` |
|        - |  8916 | `	/* Extract interface name */` |
|    46713 |  8917 | `	pName = &pGen->pIn->sData;` |
|        - |  8918 | `	/* Advance the stream cursor */` |
|    46713 |  8919 | `	pGen->pIn++;` |
|        - |  8920 | `	/* Build FQN and obtain a raw class */ {` |
|        - |  8921 | `		SyBlob sFQN;` |
|        - |  8922 | `		SyString sFQNStr;` |
|    46713 |  8923 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    46713 |  8924 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    46713 |  8925 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    46713 |  8926 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    46713 |  8927 | `		SyBlobRelease(&sFQN);` |
|        - |  8928 | `	}` |
|    46713 |  8929 | `	if( pClass == 0 ){` |
|      ! 0 |  8930 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  8931 | `		return SXERR_ABORT;` |
|        - |  8932 | `	}` |
|    46713 |  8933 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|    46713 |  8934 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  8935 | `		return SXERR_ABORT;` |
|        - |  8936 | `	}` |
|        - |  8937 | `	/* Mark as an interface (PH7_NewRawClass may have set INTERNAL) */` |
|    46713 |  8938 | `	pClass->iFlags \|= PH7_CLASS_INTERFACE;` |
|        - |  8939 | `	/* Assume no base class is given */` |
|    46713 |  8940 | `	pBase = 0;` |
|    46713 |  8941 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|    15551 |  8942 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    15551 |  8943 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|        - |  8944 | `			SyBlob sResolved;` |
|        - |  8945 | `			SyString sBaseName;` |
|        - |  8946 | `			sxu32 nRefLine;` |
|        - |  8947 | `			/* Extract base interface */` |
|    15551 |  8948 | `			pGen->pIn++;` |
|    15551 |  8949 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    15551 |  8950 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    15551 |  8951 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 |  8952 | `				SyBlobRelease(&sResolved);` |
|      ! 0 |  8953 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  8954 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|      ! 0 |  8955 | `					pName);` |
|      ! 0 |  8956 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  8957 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8958 | `					return SXERR_ABORT;` |
|        - |  8959 | `				}` |
|      ! 0 |  8960 | `				return SXRET_OK;` |
|        - |  8961 | `			}` |
|    23324 |  8962 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|    15546 |  8963 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    15551 |  8964 | `			SyStringInitFromBuf(&sBaseName,` |
|        - |  8965 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - |  8966 | `			/* Only interfaces is allowed */` |
|    15551 |  8967 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 |  8968 | `				pBase = pBase->pNextName;` |
|      ! 0 |  8969 | `			}` |
|    15551 |  8970 | `			if( pBase == 0 ){` |
|      ! 0 |  8971 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - |  8972 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|      ! 0 |  8973 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8974 | `					SyBlobRelease(&sResolved);` |
|      ! 0 |  8975 | `					return SXERR_ABORT;` |
|        - |  8976 | `				}` |
|      ! 0 |  8977 | `			}` |
|    15551 |  8978 | `			SyBlobRelease(&sResolved);` |
|     7773 |  8979 | `		}` |
|     7773 |  8980 | `	}` |
|    46713 |  8981 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - |  8982 | `		/* Syntax error */` |
|      ! 0 |  8983 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|      ! 0 |  8984 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  8985 | `		if( rc == SXERR_ABORT ){` |
|        - |  8986 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  8987 | `			return SXERR_ABORT;` |
|        - |  8988 | `		}` |
|      ! 0 |  8989 | `		return SXRET_OK;` |
|        - |  8990 | `	}` |
|    46713 |  8991 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    46713 |  8992 | `	pEnd = 0; /* cc warning */` |
|        - |  8993 | `	/* Delimit the interface body */` |
|    46713 |  8994 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    46713 |  8995 | `	if( pEnd >= pGen->pEnd ){` |
|        - |  8996 | `		/* Syntax error */` |
|      ! 0 |  8997 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|      ! 0 |  8998 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  8999 | `		if( rc == SXERR_ABORT ){` |
|        - |  9000 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  9001 | `			return SXERR_ABORT;` |
|        - |  9002 | `		}` |
|      ! 0 |  9003 | `		return SXRET_OK;` |
|        - |  9004 | `	}` |
|        - |  9005 | `	/* The delimiter token is the interface body's closing brace */` |
|    46713 |  9006 | `	pClass->nEndLine = pEnd->nLine;` |
|        - |  9007 | `	/* Swap token stream */` |
|    46713 |  9008 | `	pTmp = pGen->pEnd;` |
|    46713 |  9009 | `	pGen->pEnd = pEnd;` |
|        - |  9010 | `	/* Start the parse process` |
|        - |  9011 | `	 * Note (According to the PHP reference manual):` |
|        - |  9012 | `	 *  Only constants and function signatures(without body) are allowed.` |
|        - |  9013 | `	 *  Only 'public' visibility is allowed.` |
|        - |  9014 | `	 */` |
|    73875 |  9015 | `	for(;;){` |
|        - |  9016 | `		/* Jump leading/trailing semi-colons */` |
|   248797 |  9017 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   101047 |  9018 | `			pGen->pIn++;` |
|        5 |  9019 | `		}` |
|   147755 |  9020 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - |  9021 | `			/* End of interface body */` |
|    46709 |  9022 | `			break;` |
|        - |  9023 | `		}` |
|        - |  9024 | `		/* Bind a directly-preceding docblock to this member */` |
|   101051 |  9025 | `		GenStateSetPendingDoc(&(*pGen));` |
|   101051 |  9026 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 |  9027 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  9028 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|      ! 0 |  9029 | `				&pGen->pIn->sData,pName);` |
|      ! 0 |  9030 | `			if( rc == SXERR_ABORT ){` |
|        - |  9031 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  9032 | `				return SXERR_ABORT;` |
|        - |  9033 | `			}` |
|      ! 0 |  9034 | `			goto done;` |
|        - |  9035 | `		}` |
|        - |  9036 | `		/* Extract the current keyword */` |
|   101051 |  9037 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   101051 |  9038 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        - |  9039 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|        - |  9040 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|        3 |  9041 | `			const char *zKind = "member";` |
|        3 |  9042 | `			SyString *pMemberName = 0;` |
|        3 |  9043 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|        3 |  9044 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|        3 |  9045 | `				if( nNext == PH7_TKWRD_CONST ){` |
|        3 |  9046 | `					zKind = "constant";` |
|        3 |  9047 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|        3 |  9048 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|        2 |  9049 | `					}` |
|        1 |  9050 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|      ! 0 |  9051 | `					zKind = "method";` |
|      ! 0 |  9052 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|      ! 0 |  9053 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|      ! 0 |  9054 | `					}` |
|      ! 0 |  9055 | `				}` |
|        1 |  9056 | `			}` |
|        3 |  9057 | `			if( pMemberName ){` |
|        4 |  9058 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        1 |  9059 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|        2 |  9060 | `			}else{` |
|      ! 0 |  9061 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9062 | `					"Access type for interface %s must be public",zKind);` |
|        - |  9063 | `			}` |
|        3 |  9064 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9065 | `				return SXERR_ABORT;` |
|        - |  9066 | `			}` |
|        3 |  9067 | `			goto done;` |
|        - |  9068 | `		}` |
|   101049 |  9069 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|      ! 0 |  9070 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9071 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|      ! 0 |  9072 | `			if( rc == SXERR_ABORT ){` |
|        - |  9073 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  9074 | `				return SXERR_ABORT;` |
|        - |  9075 | `			}` |
|      ! 0 |  9076 | `			goto done;` |
|        - |  9077 | `		}` |
|   101049 |  9078 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|        - |  9079 | `			/* Advance the stream cursor */` |
|   101031 |  9080 | `			pGen->pIn++;` |
|   101031 |  9081 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 |  9082 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9083 | `					"Expecting method signature inside interface '%z'",pName);` |
|      ! 0 |  9084 | `				if( rc == SXERR_ABORT ){` |
|        - |  9085 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 |  9086 | `					return SXERR_ABORT;` |
|        - |  9087 | `				}` |
|      ! 0 |  9088 | `				goto done;` |
|        - |  9089 | `			}` |
|   101031 |  9090 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   101031 |  9091 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|      ! 0 |  9092 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9093 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|      ! 0 |  9094 | `				if( rc == SXERR_ABORT ){` |
|        - |  9095 | `					/* Error count limit reached,abort immediately */` |
|      ! 0 |  9096 | `					return SXERR_ABORT;` |
|        - |  9097 | `				}` |
|      ! 0 |  9098 | `				goto done;` |
|        - |  9099 | `			}` |
|    50513 |  9100 | `		}` |
|   101049 |  9101 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|        - |  9102 | `			/* Parse constant */` |
|       16 |  9103 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       16 |  9104 | `			if( rc != SXRET_OK ){` |
|        3 |  9105 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9106 | `					return SXERR_ABORT;` |
|        - |  9107 | `				}` |
|        3 |  9108 | `				goto done;` |
|        - |  9109 | `			}` |
|        7 |  9110 | `		}else{` |
|   101035 |  9111 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   101035 |  9112 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|        - |  9113 | `				/* Static method,record that */` |
|    11657 |  9114 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|        - |  9115 | `				/* Advance the stream cursor */` |
|    11657 |  9116 | `				pGen->pIn++;` |
|    11652 |  9117 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    11657 |  9118 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 |  9119 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9120 | `							"Expecting method signature inside interface '%z'",pName);` |
|      ! 0 |  9121 | `						if( rc == SXERR_ABORT ){` |
|        - |  9122 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 |  9123 | `							return SXERR_ABORT;` |
|        - |  9124 | `						}` |
|      ! 0 |  9125 | `						goto done;` |
|        - |  9126 | `				}` |
|     5826 |  9127 | `			}` |
|        - |  9128 | `			/* Process method signature (no body for interface methods) */` |
|   101035 |  9129 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   101035 |  9130 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9131 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9132 | `					return SXERR_ABORT;` |
|        - |  9133 | `				}` |
|      ! 0 |  9134 | `				goto done;` |
|        - |  9135 | `			}` |
|        - |  9136 | `		}` |
|        5 |  9137 | `	}` |
|        - |  9138 | `	/* Install the interface */` |
|    46709 |  9139 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    46709 |  9140 | `	if( rc == SXRET_OK && pBase ){` |
|        - |  9141 | `		/* Inherit from the base interface */` |
|    15551 |  9142 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|     7773 |  9143 | `	}` |
|    46709 |  9144 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  9145 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9146 | `		return SXERR_ABORT;` |
|        - |  9147 | `	}` |
|    23352 |  9148 | `done:` |
|        - |  9149 | `	/* Point beyond the interface body */` |
|    46713 |  9150 | `	pGen->pIn  = &pEnd[1];` |
|    46713 |  9151 | `	pGen->pEnd = pTmp;` |
|    46713 |  9152 | `	return PH7_OK;` |
|    23359 |  9153 | `}` |
|        - |  9154 | `/*` |
|        - |  9155 | ` * Compile a user-defined class.` |
|        - |  9156 | ` * According to the PHP language reference manual` |
|        - |  9157 | ` *  class` |
|        - |  9158 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|        - |  9159 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|        - |  9160 | ` *  of the properties and methods belonging to the class.` |
|        - |  9161 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|        - |  9162 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|        - |  9163 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|        - |  9164 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|        - |  9165 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|        - |  9166 | ` *  (called "methods").` |
|        - |  9167 | ` */` |
|        - |  9168 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|        - |  9169 | `typedef struct TraitUseEntry TraitUseEntry;` |
|        - |  9170 | `struct TraitUseEntry {` |
|        - |  9171 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|        - |  9172 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|        - |  9173 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|        - |  9174 | `};` |
|        - |  9175 | `/*` |
|        - |  9176 | ` * Validate that methods implementing interface contracts have compatible` |
|        - |  9177 | ` * signatures: public visibility and at least as many parameters as declared.` |
|        - |  9178 | ` */` |
|   215218 |  9179 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 |  9180 | `{` |
|        - |  9181 | `	ph7_class **apIface;` |
|        - |  9182 | `	sxu32 nIface,i;` |
|        - |  9183 | `	sxi32 rc;` |
|   215223 |  9184 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      ! 0 |  9185 | `		return SXRET_OK;` |
|        - |  9186 | `	}` |
|   215223 |  9187 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   215223 |  9188 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   429169 |  9189 | `	for(i = 0; i < nIface; i++){` |
|   213951 |  9190 | `		ph7_class *pIface = apIface[i];` |
|        - |  9191 | `		SyHashEntry *pEntry;` |
|   213951 |  9192 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   498055 |  9193 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   284109 |  9194 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |  9195 | `			ph7_class_method *pImplMeth;` |
|   284109 |  9196 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|        - |  9197 | `			/* Find the implementing method in the class */` |
|   284109 |  9198 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   284109 |  9199 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       18 |  9200 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|        - |  9201 | `			}` |
|        - |  9202 | `			/* Check visibility: interface methods must be implemented as public */` |
|   284095 |  9203 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        4 |  9204 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|        - |  9205 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|        1 |  9206 | `					&pClass->sName,pMName,&pIface->sName);` |
|        3 |  9207 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9208 | `					return SXERR_ABORT;` |
|        - |  9209 | `				}` |
|        1 |  9210 | `			}` |
|        - |  9211 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|        - |  9212 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|        - |  9213 | `			 */` |
|        - |  9214 | `			{` |
|   284095 |  9215 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   284095 |  9216 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   284095 |  9217 | `				int sigError = 0;` |
|   284095 |  9218 | `				if( nImplArgs < nIfaceArgs ){` |
|        3 |  9219 | `					sigError = 1;` |
|   284094 |  9220 | `				}else if( nImplArgs > nIfaceArgs ){` |
|        - |  9221 | `					/* Extra parameters must all have default values */` |
|        6 |  9222 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|        - |  9223 | `					sxu32 k;` |
|        8 |  9224 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|        6 |  9225 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|        3 |  9226 | `							sigError = 1;` |
|        3 |  9227 | `							break;` |
|        - |  9228 | `						}` |
|        2 |  9229 | `					}` |
|        2 |  9230 | `				}` |
|   284095 |  9231 | `				if( sigError ){` |
|        - |  9232 | `					SyBlob sImplSig, sIfaceSig;` |
|        - |  9233 | `					ph7_vm_func_arg *aArgs;` |
|        - |  9234 | `					sxu32 j;` |
|        6 |  9235 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|        6 |  9236 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|        - |  9237 | `					/* Build implementing method signature */` |
|        6 |  9238 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       12 |  9239 | `					for(j = 0; j < nImplArgs; j++){` |
|        8 |  9240 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|        8 |  9241 | `						SyBlobAppend(&sImplSig,"$",1);` |
|        8 |  9242 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|        5 |  9243 | `					}` |
|        - |  9244 | `					/* Build interface method signature */` |
|        6 |  9245 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|       12 |  9246 | `					for(j = 0; j < nIfaceArgs; j++){` |
|        8 |  9247 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|        8 |  9248 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|        8 |  9249 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|        5 |  9250 | `					}` |
|        8 |  9251 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|        - |  9252 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|        2 |  9253 | `						&pClass->sName,pMName,` |
|        4 |  9254 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|        2 |  9255 | `						&pIface->sName,pMName,` |
|        4 |  9256 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|        6 |  9257 | `					SyBlobRelease(&sImplSig);` |
|        6 |  9258 | `					SyBlobRelease(&sIfaceSig);` |
|        6 |  9259 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9260 | `						return SXERR_ABORT;` |
|        - |  9261 | `					}` |
|        2 |  9262 | `				}` |
|        - |  9263 | `			}` |
|        5 |  9264 | `		}` |
|   106978 |  9265 | `	}` |
|   215223 |  9266 | `	return SXRET_OK;` |
|   107614 |  9267 | `}` |
|        - |  9268 | `/*` |
|        - |  9269 | ` * Check that a concrete class has no remaining abstract methods.` |
|        - |  9270 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|        - |  9271 | ` */` |
|   215218 |  9272 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 |  9273 | `{` |
|        - |  9274 | `	ph7_class_method *pMeth;` |
|        - |  9275 | `	SyHashEntry *pEntry;` |
|        - |  9276 | `	sxu32 nAbstract;` |
|        - |  9277 | `	SyBlob sMsg;` |
|        - |  9278 | `	sxi32 rc;` |
|        - |  9279 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   215223 |  9280 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     7811 |  9281 | `		return SXRET_OK;` |
|        - |  9282 | `	}` |
|        - |  9283 | `	/* Count abstract methods */` |
|   207417 |  9284 | `	nAbstract = 0;` |
|   207417 |  9285 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  3075965 |  9286 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  2868553 |  9287 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  2868553 |  9288 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       20 |  9289 | `			nAbstract++;` |
|        8 |  9290 | `		}` |
|        5 |  9291 | `	}` |
|   207417 |  9292 | `	if( nAbstract == 0 ){` |
|   207403 |  9293 | `		return SXRET_OK;` |
|        - |  9294 | `	}` |
|        - |  9295 | `	/* Build the error message listing all abstract methods with origins */` |
|       18 |  9296 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       18 |  9297 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|        - |  9298 | `		"be declared abstract or implement the remaining method%s (",` |
|        7 |  9299 | `		&pClass->sName,nAbstract,` |
|        7 |  9300 | `		(nAbstract > 1 ? "s" : ""),` |
|        7 |  9301 | `		(nAbstract > 1 ? "s" : ""));` |
|        - |  9302 | `	/* Second pass: list methods with origins */` |
|        - |  9303 | `	{` |
|       18 |  9304 | `		sxu32 nListed = 0;` |
|       18 |  9305 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|       36 |  9306 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|       22 |  9307 | `			ph7_class *pOrigin = 0;` |
|        - |  9308 | `			SyString *pMName;` |
|       22 |  9309 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|       22 |  9310 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|        3 |  9311 | `				continue;` |
|        - |  9312 | `			}` |
|       20 |  9313 | `			pMName = &pMeth->sFunc.sName;` |
|       20 |  9314 | `			if( nListed > 0 ){` |
|        3 |  9315 | `				SyBlobAppend(&sMsg,", ",2);` |
|        1 |  9316 | `			}` |
|        - |  9317 | `			/* Find the origin of this abstract method.` |
|        - |  9318 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|        - |  9319 | `			 * inheritance chains) take precedence for interface-declared` |
|        - |  9320 | `			 * methods. Abstract class methods only win when the class` |
|        - |  9321 | `			 * itself declared the abstract method (not inherited from` |
|        - |  9322 | `			 * an interface). Trait methods are adopted into the using` |
|        - |  9323 | `			 * class's namespace.` |
|        - |  9324 | `			 */` |
|        - |  9325 | `			{` |
|        - |  9326 | `				ph7_class **apIface;` |
|        - |  9327 | `				ph7_class **apTrait;` |
|        - |  9328 | `				ph7_class *pWalk;` |
|        - |  9329 | `				sxu32 i;` |
|        - |  9330 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|        - |  9331 | `				 * (one that was written in the class body, not inherited from an` |
|        - |  9332 | `				 * interface). PHP attributes origin to the declaring class.` |
|        - |  9333 | `				 */` |
|       20 |  9334 | `				if( pClass->pBase ){` |
|       11 |  9335 | `					pWalk = pClass->pBase;` |
|       19 |  9336 | `					while( pWalk ){` |
|        - |  9337 | `						ph7_class_method *pParentMeth;` |
|       13 |  9338 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|       13 |  9339 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|        - |  9340 | `							/* Exclude methods that came from an interface anywhere` |
|        - |  9341 | `							 * in this class's ancestor chain.` |
|        - |  9342 | `							 */` |
|       13 |  9343 | `							int fromIface = 0;` |
|       13 |  9344 | `							ph7_class *pAnc = pWalk;` |
|       17 |  9345 | `							while( pAnc ){` |
|        - |  9346 | `								ph7_class **apPI;` |
|        - |  9347 | `								sxu32 j;` |
|       15 |  9348 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|       15 |  9349 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       10 |  9350 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       10 |  9351 | `										fromIface = 1;` |
|       10 |  9352 | `										break;` |
|        - |  9353 | `									}` |
|      ! 0 |  9354 | `								}` |
|       15 |  9355 | `								if( fromIface ) break;` |
|        6 |  9356 | `								pAnc = pAnc->pBase;` |
|        2 |  9357 | `							}` |
|       13 |  9358 | `							if( !fromIface ){` |
|        3 |  9359 | `								pOrigin = pWalk;` |
|        3 |  9360 | `								break;` |
|        - |  9361 | `							}` |
|        4 |  9362 | `						}` |
|       10 |  9363 | `						pWalk = pWalk->pBase;` |
|        2 |  9364 | `					}` |
|        4 |  9365 | `				}` |
|        - |  9366 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|        - |  9367 | `				 * each interface's own parent chain for the deepest origin.` |
|        - |  9368 | `				 */` |
|       20 |  9369 | `				if( !pOrigin ){` |
|       18 |  9370 | `					pWalk = pClass;` |
|       40 |  9371 | `					while( pWalk && !pOrigin ){` |
|       26 |  9372 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|       26 |  9373 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|       16 |  9374 | `							ph7_class *pIface = apIface[i];` |
|       16 |  9375 | `							ph7_class *pDeepest = 0;` |
|       28 |  9376 | `							while( pIface ){` |
|       16 |  9377 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|       16 |  9378 | `									pDeepest = pIface;` |
|        6 |  9379 | `								}` |
|       16 |  9380 | `								pIface = pIface->pBase;` |
|        4 |  9381 | `							}` |
|       16 |  9382 | `							if( pDeepest ){` |
|       16 |  9383 | `								pOrigin = pDeepest;` |
|       16 |  9384 | `								break;` |
|        - |  9385 | `							}` |
|      ! 0 |  9386 | `						}` |
|       26 |  9387 | `						pWalk = pWalk->pBase;` |
|        4 |  9388 | `					}` |
|        7 |  9389 | `				}` |
|        - |  9390 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|       20 |  9391 | `				if( !pOrigin ){` |
|        3 |  9392 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|        3 |  9393 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|        3 |  9394 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|        3 |  9395 | `							pOrigin = pClass;` |
|        3 |  9396 | `							break;` |
|        - |  9397 | `						}` |
|      ! 0 |  9398 | `					}` |
|        1 |  9399 | `				}` |
|        - |  9400 | `			}` |
|       20 |  9401 | `			if( pOrigin ){` |
|       20 |  9402 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|       12 |  9403 | `			}else{` |
|        - |  9404 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|      ! 0 |  9405 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|        - |  9406 | `			}` |
|       20 |  9407 | `			nListed++;` |
|        4 |  9408 | `		}` |
|        - |  9409 | `	}` |
|       18 |  9410 | `	SyBlobAppend(&sMsg,")",1);` |
|       25 |  9411 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|       14 |  9412 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|       18 |  9413 | `	SyBlobRelease(&sMsg);` |
|       18 |  9414 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  9415 | `		return SXERR_ABORT;` |
|        - |  9416 | `	}` |
|       18 |  9417 | `	return SXRET_OK;` |
|   107614 |  9418 | `}` |
|        - |  9419 | `/*` |
|        - |  9420 | ` * Parse a class/interface name reference from the current token stream.` |
|        - |  9421 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|        - |  9422 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|        - |  9423 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|        - |  9424 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|        - |  9425 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|        - |  9426 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|        - |  9427 | ` */` |
|   192160 |  9428 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|        5 |  9429 | `{` |
|   192165 |  9430 | `	int isAbsolute = 0;` |
|   192165 |  9431 | `	SyToken *pStart = pGen->pIn;` |
|        - |  9432 | `	SyBlob sName;` |
|   192165 |  9433 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|     4473 |  9434 | `		isAbsolute = 1;` |
|     4473 |  9435 | `		pGen->pIn++;` |
|     2234 |  9436 | `	}` |
|   192165 |  9437 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        8 |  9438 | `		pGen->pIn = pStart;` |
|        8 |  9439 | `		return SXERR_INVALID;` |
|        - |  9440 | `	}` |
|   192159 |  9441 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   192159 |  9442 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   192159 |  9443 | `	pGen->pIn++;` |
|   288252 |  9444 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|    96103 |  9445 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       16 |  9446 | `		SyBlobAppend(&sName,"\\",1);` |
|       16 |  9447 | `		pGen->pIn++;` |
|       16 |  9448 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       16 |  9449 | `		pGen->pIn++;` |
|        2 |  9450 | `	}` |
|   192159 |  9451 | `	if( isAbsolute ){` |
|     4471 |  9452 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|     2238 |  9453 | `	}else{` |
|        - |  9454 | `		SyString sRaw;` |
|   187693 |  9455 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   187693 |  9456 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|        - |  9457 | `	}` |
|   192159 |  9458 | `	SyBlobRelease(&sName);` |
|   192159 |  9459 | `	return SXRET_OK;` |
|    96085 |  9460 | `}` |
|        - |  9461 | `/*` |
|        - |  9462 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|        - |  9463 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|        - |  9464 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|        - |  9465 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|        - |  9466 | ` * either direction cannot run unbounded.` |
|        - |  9467 | ` */` |
|        - |  9468 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    46804 |  9469 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|        5 |  9470 | `{` |
|        - |  9471 | `	ph7_class **apParent;` |
|        - |  9472 | `	sxu32 n;` |
|   120839 |  9473 | `	while( pInterface ){` |
|    81813 |  9474 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|      ! 0 |  9475 | `			return FALSE;` |
|        - |  9476 | `		}` |
|   101252 |  9477 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    38878 |  9478 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|     7783 |  9479 | `			return TRUE;` |
|        - |  9480 | `		}` |
|    74035 |  9481 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    74035 |  9482 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|      ! 0 |  9483 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|      ! 0 |  9484 | `				return TRUE;` |
|        - |  9485 | `			}` |
|      ! 0 |  9486 | `		}` |
|    74035 |  9487 | `		pInterface = pInterface->pBase;` |
|    74035 |  9488 | `		iDepth++;` |
|        5 |  9489 | `	}` |
|    39031 |  9490 | `	return FALSE;` |
|    23407 |  9491 | `}` |
|    46804 |  9492 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|        5 |  9493 | `{` |
|    46809 |  9494 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|        5 |  9495 | `}` |
|        - |  9496 | `/*` |
|        - |  9497 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|        - |  9498 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|        - |  9499 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|        - |  9500 | ` */` |
|     7778 |  9501 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|        5 |  9502 | `{` |
|     7787 |  9503 | `	while( pBase ){` |
|       10 |  9504 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|        2 |  9505 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|        3 |  9506 | `			return TRUE;` |
|        - |  9507 | `		}` |
|       10 |  9508 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|        6 |  9509 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|        3 |  9510 | `			return TRUE;` |
|        - |  9511 | `		}` |
|        5 |  9512 | `		pBase = pBase->pBase;` |
|        1 |  9513 | `	}` |
|     7779 |  9514 | `	return FALSE;` |
|     3894 |  9515 | `}` |
|        - |  9516 | `/*` |
|        - |  9517 | `` * Compile a single `case NAME [= value];` member of an enum body (PHP 8.1).`` |
|        - |  9518 | ` * A case is stored as a class constant (PH7_CLASS_ATTR_CONSTANT\|ENUMCASE) whose` |
|        - |  9519 | ` * aByteCode holds the BACKING value expression for backed enums (empty for pure` |
|        - |  9520 | ` * enums). The case's runtime value — the singleton instance — is materialized` |
|        - |  9521 | ` * lazily on first access (VmEnumMaterialize, vm.c), matching PHP's lazy` |
|        - |  9522 | ` * backing-value type/duplicate checks. Declaration order is recorded in` |
|        - |  9523 | ` * pClass->aEnumCases for cases().` |
|        - |  9524 | ` */` |
|       42 |  9525 | `static sxi32 GenStateCompileEnumCase(ph7_gen_state *pGen,ph7_class *pClass)` |
|        5 |  9526 | `{` |
|       47 |  9527 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  9528 | `	SySet *pInstrContainer;` |
|        - |  9529 | `	ph7_class_attr *pCase;` |
|        - |  9530 | `	SyString *pName;` |
|        - |  9531 | `	sxi32 rc;` |
|       47 |  9532 | `	pGen->pIn++; /* Jump the 'case' keyword */` |
|       47 |  9533 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  9534 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  9535 | `			"Invalid enum case name inside enum '%z'",&pClass->sName);` |
|      ! 0 |  9536 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  9537 | `			return SXERR_ABORT;` |
|        - |  9538 | `		}` |
|      ! 0 |  9539 | `		goto Synchronize;` |
|        - |  9540 | `	}` |
|       47 |  9541 | `	pName = &pGen->pIn->sData;` |
|        - |  9542 | `	/* Cases share the class-constant namespace (php: "Cannot redefine class constant") */` |
|       47 |  9543 | `	if( SyHashGet(&pClass->hAttr,(const void *)pName->zString,pName->nByte) != 0 ){` |
|      ! 0 |  9544 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9545 | `			"Cannot redefine class constant %z::%z",&pClass->sName,pName);` |
|      ! 0 |  9546 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  9547 | `			return SXERR_ABORT;` |
|        - |  9548 | `		}` |
|      ! 0 |  9549 | `		goto Synchronize;` |
|        - |  9550 | `	}` |
|       47 |  9551 | `	pCase = PH7_NewClassAttr(pGen->pVm,pName,pGen->pIn->nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - |  9552 | `		PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|       47 |  9553 | `	if( pCase == 0 ){` |
|      ! 0 |  9554 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9555 | `		return SXERR_ABORT;` |
|        - |  9556 | `	}` |
|       47 |  9557 | `	GenStateConsumeDoc(&(*pGen),&pCase->sDoc);` |
|       47 |  9558 | `	if( GenStateConsumeAttrs(&(*pGen),&pCase->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  9559 | `		return SXERR_ABORT;` |
|        - |  9560 | `	}` |
|       47 |  9561 | `	pGen->pIn++; /* Jump the case name */` |
|       47 |  9562 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) ){` |
|       31 |  9563 | `		if( pClass->nEnumBacking == 0 ){` |
|        8 |  9564 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        2 |  9565 | `				"Case %z of non-backed enum %z must not have a value",pName,&pClass->sName);` |
|        6 |  9566 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9567 | `				return SXERR_ABORT;` |
|        - |  9568 | `			}` |
|        6 |  9569 | `			goto Synchronize;` |
|        - |  9570 | `		}` |
|       25 |  9571 | `		pGen->pIn++; /* Jump the equal sign */` |
|        - |  9572 | `		/* Compile the backing value expression into the case's own container` |
|        - |  9573 | `		 * (same technique as class constants). */` |
|       25 |  9574 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       25 |  9575 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pCase->aByteCode);` |
|       25 |  9576 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|       25 |  9577 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 |  9578 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  9579 | `				"Empty value for enum case %z::%z",&pClass->sName,pName);` |
|      ! 0 |  9580 | `		}` |
|       25 |  9581 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|       25 |  9582 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       25 |  9583 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  9584 | `			return SXERR_ABORT;` |
|        - |  9585 | `		}` |
|       13 |  9586 | `	}else{` |
|       17 |  9587 | `		if( pClass->nEnumBacking != 0 ){` |
|      ! 0 |  9588 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  9589 | `				"Case %z of backed enum %z must have a value",pName,&pClass->sName);` |
|      ! 0 |  9590 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9591 | `				return SXERR_ABORT;` |
|        - |  9592 | `			}` |
|      ! 0 |  9593 | `			goto Synchronize;` |
|        - |  9594 | `		}` |
|        - |  9595 | `	}` |
|       41 |  9596 | `	rc = PH7_ClassInstallAttr(pClass,pCase);` |
|       41 |  9597 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  9598 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9599 | `		return SXERR_ABORT;` |
|        - |  9600 | `	}` |
|       41 |  9601 | `	SySetPut(&pClass->aEnumCases,(const void *)&pCase);` |
|       41 |  9602 | `	return SXRET_OK;` |
|        2 |  9603 | `Synchronize:` |
|        - |  9604 | `	/* Synchronize with the first semi-colon */` |
|       14 |  9605 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|       10 |  9606 | `		pGen->pIn++;` |
|        2 |  9607 | `	}` |
|        6 |  9608 | `	return SXERR_CORRUPT;` |
|       26 |  9609 | `}` |
|        - |  9610 | `/*` |
|        - |  9611 | ` * Synthesize the enum interface methods (PHP 8.1): cases() for every enum,` |
|        - |  9612 | ` * plus from()/tryFrom() for backed enums. Each is an ordinary public static` |
|        - |  9613 | ` * method whose body forwards to a __phl_enum_* engine thunk (vm.c) with the` |
|        - |  9614 | ` * enum's FQN embedded as a literal — the same forwarder pattern the` |
|        - |  9615 | ` * Generator/Fiber/Reflection builtins use. The source buffer is owned by the` |
|        - |  9616 | ` * VM allocator and never freed: tokens (method and parameter names) keep` |
|        - |  9617 | ` * pointers into it (see the constructor-promotion precedent above).` |
|        - |  9618 | ` */` |
|       24 |  9619 | `static sxi32 GenStateCompileEnumMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|        3 |  9620 | `{` |
|        - |  9621 | `	SyToken *pSaveIn,*pSaveEnd;` |
|        - |  9622 | `	const char *zBack;` |
|        - |  9623 | `	SySet sToken;` |
|        - |  9624 | `	char *zSrc;` |
|        - |  9625 | `	sxu32 nSrc,nMax;` |
|       27 |  9626 | `	sxi32 rc = SXRET_OK;` |
|       27 |  9627 | `	nMax = 3*(sxu32)sizeof("function tryFrom(string $value){return __phl_enum_tryfrom('',$value);}")` |
|       24 |  9628 | `		+ 3*SyStringLength(&pClass->sName) + 64;` |
|       27 |  9629 | `	zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nMax);` |
|       27 |  9630 | `	if( zSrc == 0 ){` |
|      ! 0 |  9631 | `		PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9632 | `		return SXERR_ABORT;` |
|        - |  9633 | `	}` |
|       27 |  9634 | `	zBack = (pClass->nEnumBacking == MEMOBJ_INT) ? "int" : "string";` |
|       27 |  9635 | `	if( pClass->nEnumBacking != 0 ){` |
|       19 |  9636 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|        - |  9637 | `			"function cases(){return __phl_enum_cases('%z');}"` |
|        - |  9638 | `			"function from(%s $value){return __phl_enum_from('%z',$value);}"` |
|        - |  9639 | `			"function tryFrom(%s $value){return __phl_enum_tryfrom('%z',$value);}",` |
|        6 |  9640 | `			&pClass->sName,zBack,&pClass->sName,zBack,&pClass->sName);` |
|        7 |  9641 | `	}else{` |
|       21 |  9642 | `		nSrc = SyBufferFormat(zSrc,nMax,` |
|        6 |  9643 | `			"function cases(){return __phl_enum_cases('%z');}",&pClass->sName);` |
|        - |  9644 | `	}` |
|       27 |  9645 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       27 |  9646 | `	PH7_TokenizePHP(zSrc,nSrc,pClass->nLine,&sToken,0);` |
|       27 |  9647 | `	pSaveIn = pGen->pIn;` |
|       27 |  9648 | `	pSaveEnd = pGen->pEnd;` |
|       27 |  9649 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|       27 |  9650 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       75 |  9651 | `	while( pGen->pIn < pGen->pEnd && rc != SXERR_ABORT ){` |
|       51 |  9652 | `		rc = GenStateCompileClassMethod(&(*pGen),PH7_TKWRD_PUBLIC,PH7_CLASS_ATTR_STATIC,TRUE,pClass);` |
|        3 |  9653 | `	}` |
|       27 |  9654 | `	pGen->pIn = pSaveIn;` |
|       27 |  9655 | `	pGen->pEnd = pSaveEnd;` |
|       27 |  9656 | `	SySetRelease(&sToken);` |
|       27 |  9657 | `	return (rc == SXERR_ABORT) ? SXERR_ABORT : SXRET_OK;` |
|       15 |  9658 | `}` |
|        - |  9659 | `/*` |
|        - |  9660 | ` * Magic methods an enum may not declare (php 8.1, zend_enum.c list —` |
|        - |  9661 | ` * __call/__callStatic/__invoke stay allowed).` |
|        - |  9662 | ` */` |
|        - |  9663 | `static const char *azEnumBannedMagic[] = {` |
|        - |  9664 | `	"__construct","__destruct","__clone","__get","__set","__isset","__unset",` |
|        - |  9665 | `	"__toString","__sleep","__wakeup","__serialize","__unserialize","__set_state"` |
|        - |  9666 | `};` |
|        - |  9667 | `/*` |
|        - |  9668 | ` * Enum post-body validation + synthesis: reject declared properties (including` |
|        - |  9669 | ``  * trait-imported ones) and banned magic methods, install the readonly `name` `` |
|        - |  9670 | `` * (and, for backed enums, `value`) instance properties the case singletons`` |
|        - |  9671 | ` * carry, and synthesize cases()/from()/tryFrom(). Runs after trait application` |
|        - |  9672 | ` * and before the class is installed.` |
|        - |  9673 | ` */` |
|       24 |  9674 | `static sxi32 GenStateEnumFinalize(ph7_gen_state *pGen,ph7_class *pClass,sxu32 nLine)` |
|        3 |  9675 | `{` |
|        - |  9676 | `	SyHashEntry *pEntry;` |
|        - |  9677 | `	sxi32 rc;` |
|        - |  9678 | `	sxu32 n;` |
|        - |  9679 | `	/* php: "Enum %s cannot include properties" */` |
|       27 |  9680 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|       69 |  9681 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|       47 |  9682 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|       47 |  9683 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        3 |  9684 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pAttr->nLine ? pAttr->nLine : nLine,` |
|        1 |  9685 | `				"Enum %z cannot include properties",&pClass->sName);` |
|        3 |  9686 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9687 | `				return SXERR_ABORT;` |
|        - |  9688 | `			}` |
|        3 |  9689 | `			break;` |
|        - |  9690 | `		}` |
|        2 |  9691 | `	}` |
|        - |  9692 | `	/* php: "Enum %s cannot include magic method %s" */` |
|      339 |  9693 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEnumBannedMagic) ; n++ ){` |
|      468 |  9694 | `		if( SyHashGet(&pClass->hMethod,(const void *)azEnumBannedMagic[n],` |
|      315 |  9695 | `			SyStrlen(azEnumBannedMagic[n])) != 0 ){` |
|      ! 0 |  9696 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  9697 | `				"Enum %z cannot include magic method %s",&pClass->sName,azEnumBannedMagic[n]);` |
|      ! 0 |  9698 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9699 | `				return SXERR_ABORT;` |
|        - |  9700 | `			}` |
|      ! 0 |  9701 | `		}` |
|      159 |  9702 | `	}` |
|        - |  9703 | ``	/* Install the case-singleton instance properties: readonly `name` (every`` |
|        - |  9704 | ``	 * enum) and `value` (backed only). Materialization (vm.c) fills them and`` |
|        - |  9705 | `	 * clears the readonly write-once latch; user writes then raise php's` |
|        - |  9706 | `	 * "Cannot modify readonly property" through the normal store path. */` |
|        - |  9707 | `	{` |
|        - |  9708 | `		static const SyString sNameProp = { "name",sizeof("name")-1 };` |
|        - |  9709 | `		static const SyString sValueProp = { "value",sizeof("value")-1 };` |
|        - |  9710 | `		ph7_class_attr *pAttr;` |
|       27 |  9711 | `		pAttr = PH7_NewClassAttr(pGen->pVm,&sNameProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - |  9712 | `			PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|       27 |  9713 | `		if( pAttr == 0 ){` |
|      ! 0 |  9714 | `			PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9715 | `			return SXERR_ABORT;` |
|        - |  9716 | `		}` |
|       27 |  9717 | `		pAttr->nType = MEMOBJ_STRING;` |
|       27 |  9718 | `		SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|       27 |  9719 | `		PH7_ClassInstallAttr(pClass,pAttr);` |
|       27 |  9720 | `		if( pClass->nEnumBacking != 0 ){` |
|       13 |  9721 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&sValueProp,nLine,PH7_CLASS_PROT_PUBLIC,` |
|        - |  9722 | `				PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|       13 |  9723 | `			if( pAttr == 0 ){` |
|      ! 0 |  9724 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9725 | `				return SXERR_ABORT;` |
|        - |  9726 | `			}` |
|       13 |  9727 | `			pAttr->nType = pClass->nEnumBacking;` |
|       13 |  9728 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|        7 |  9729 | `				SyStringInitFromBuf(&pAttr->sTypeName,"int",sizeof("int")-1);` |
|        4 |  9730 | `			}else{` |
|        7 |  9731 | `				SyStringInitFromBuf(&pAttr->sTypeName,"string",sizeof("string")-1);` |
|        - |  9732 | `			}` |
|       13 |  9733 | `			PH7_ClassInstallAttr(pClass,pAttr);` |
|        6 |  9734 | `		}` |
|        - |  9735 | `	}` |
|       27 |  9736 | `	return GenStateCompileEnumMethods(&(*pGen),pClass);` |
|       15 |  9737 | `}` |
|        - |  9738 | `/*` |
|        - |  9739 | ` * Compile a class declaration, named or anonymous.` |
|        - |  9740 | ` *` |
|        - |  9741 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|        - |  9742 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|        - |  9743 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|        - |  9744 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|        - |  9745 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|        - |  9746 | ` * implements, body, install) is shared by both paths.` |
|        - |  9747 | ` */` |
|   215262 |  9748 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|        - |  9749 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|        5 |  9750 | `{` |
|   215267 |  9751 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - |  9752 | `	ph7_class *pClass,*pBase;` |
|        - |  9753 | `	SyToken *pEnd,*pTmp;` |
|        - |  9754 | `	sxi32 iProtection;` |
|        - |  9755 | `	SySet aInterfaces;` |
|        - |  9756 | `	SySet aUseEntries;` |
|        - |  9757 | `	sxi32 iAttrflags;` |
|        - |  9758 | `	SyString *pName;` |
|        - |  9759 | `	sxi32 nKwrd;` |
|        - |  9760 | `	sxi32 rc;` |
|        - |  9761 | `	/* Jump the 'class' keyword */` |
|   215267 |  9762 | `	pGen->pIn++;` |
|   215267 |  9763 | `	if( pAnonName ){` |
|        - |  9764 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|        - |  9765 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|        - |  9766 | `		 * then use the synthesized name. */` |
|       32 |  9767 | `		*ppArgStart = *ppArgEnd = 0;` |
|       32 |  9768 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        7 |  9769 | `			pGen->pIn++; /* Jump '(' */` |
|        7 |  9770 | `			*ppArgStart = pGen->pIn;` |
|       10 |  9771 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|        3 |  9772 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|        7 |  9773 | `			pGen->pIn = *ppArgEnd;` |
|        7 |  9774 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|        3 |  9775 | `		}` |
|       32 |  9776 | `		pName = pAnonName;` |
|       32 |  9777 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|       18 |  9778 | `	}else{` |
|   215239 |  9779 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|        - |  9780 | `			/* Syntax error */` |
|      ! 0 |  9781 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|      ! 0 |  9782 | `			if( rc == SXERR_ABORT ){` |
|        - |  9783 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  9784 | `				return SXERR_ABORT;` |
|        - |  9785 | `			}` |
|        - |  9786 | `			/* Synchronize with the first semi-colon or curly braces */` |
|      ! 0 |  9787 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|      ! 0 |  9788 | `				pGen->pIn++;` |
|      ! 0 |  9789 | `			}` |
|      ! 0 |  9790 | `			return SXRET_OK;` |
|        - |  9791 | `		}` |
|        - |  9792 | `		/* Extract class name */` |
|   215239 |  9793 | `		pName = &pGen->pIn->sData;` |
|        - |  9794 | `		/* Advance the stream cursor */` |
|   215239 |  9795 | `		pGen->pIn++;` |
|        - |  9796 | `		/* Build FQN and obtain a raw class */ {` |
|        - |  9797 | `			SyBlob sFQN;` |
|        - |  9798 | `			SyString sFQNStr;` |
|   215239 |  9799 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   215239 |  9800 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|   215239 |  9801 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   215239 |  9802 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   215239 |  9803 | `			SyBlobRelease(&sFQN);` |
|        - |  9804 | `		}` |
|        - |  9805 | `	}` |
|   215267 |  9806 | `	if( pClass == 0 ){` |
|      ! 0 |  9807 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  9808 | `		return SXERR_ABORT;` |
|        - |  9809 | `	}` |
|   215262 |  9810 | `	if( (iFlags & PH7_CLASS_ENUM) && pGen->pIn < pGen->pEnd` |
|       33 |  9811 | `		&& (pGen->pIn->nType & PH7_TK_COLON /* ':' */) ){` |
|        - |  9812 | ``		/* Backed enum: `enum Name: int\|string` (PHP 8.1) */`` |
|       16 |  9813 | `		pGen->pIn++; /* Jump ':' */` |
|       14 |  9814 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       16 |  9815 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_INT ){` |
|        7 |  9816 | `			pClass->nEnumBacking = MEMOBJ_INT;` |
|        7 |  9817 | `			pGen->pIn++;` |
|       12 |  9818 | `		}else if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|       10 |  9819 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STRING ){` |
|        7 |  9820 | `			pClass->nEnumBacking = MEMOBJ_STRING;` |
|        7 |  9821 | `			pGen->pIn++;` |
|        4 |  9822 | `		}else{` |
|        3 |  9823 | `			SyToken *pTok = pGen->pIn;` |
|        3 |  9824 | `			if( pTok >= pGen->pEnd ){ pTok--; }` |
|        4 |  9825 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pTok->nLine,` |
|        1 |  9826 | `				"Enum backing type must be int or string, %z given",&pTok->sData);` |
|        3 |  9827 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9828 | `				return SXERR_ABORT;` |
|        - |  9829 | `			}` |
|        3 |  9830 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|        3 |  9831 | `				pGen->pIn++; /* Skip the bogus type token */` |
|        1 |  9832 | `			}` |
|        - |  9833 | `		}` |
|        7 |  9834 | `	}` |
|   215267 |  9835 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|   215267 |  9836 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  9837 | `		return SXERR_ABORT;` |
|        - |  9838 | `	}` |
|        - |  9839 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   215267 |  9840 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   215267 |  9841 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|        - |  9842 | `	/* Assume a standalone class */` |
|   215267 |  9843 | `	pBase = 0;` |
|   215267 |  9844 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   171297 |  9845 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   171297 |  9846 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|        - |  9847 | `			SyBlob sResolved;` |
|        - |  9848 | `			SyString sBaseName;` |
|        - |  9849 | `			sxu32 nRefLine;` |
|   124517 |  9850 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|        - |  9851 | `				/* php parse-fatals here (enums have no inheritance) */` |
|      ! 0 |  9852 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 |  9853 | `					"Enum %z cannot extend a class",&pClass->sName);` |
|      ! 0 |  9854 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9855 | `					return SXERR_ABORT;` |
|        - |  9856 | `				}` |
|      ! 0 |  9857 | `			}` |
|   124517 |  9858 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   124517 |  9859 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   124517 |  9860 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   124517 |  9861 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        3 |  9862 | `				SyBlobRelease(&sResolved);` |
|        4 |  9863 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  9864 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|        1 |  9865 | `					pName);` |
|        3 |  9866 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|        3 |  9867 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9868 | `					return SXERR_ABORT;` |
|        - |  9869 | `				}` |
|        3 |  9870 | `				return SXRET_OK;` |
|        - |  9871 | `			}` |
|   186770 |  9872 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   124510 |  9873 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   124515 |  9874 | `			SyStringInitFromBuf(&sBaseName,` |
|        - |  9875 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - |  9876 | `			/* Interfaces are not allowed */` |
|   124515 |  9877 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|      ! 0 |  9878 | `				pBase = pBase->pNextName;` |
|      ! 0 |  9879 | `			}` |
|   124515 |  9880 | `			if( pBase == 0 ){` |
|      ! 0 |  9881 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - |  9882 | `					"Nonexistent base class '%z'",&sBaseName);` |
|      ! 0 |  9883 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9884 | `					SyBlobRelease(&sResolved);` |
|      ! 0 |  9885 | `					return SXERR_ABORT;` |
|        - |  9886 | `				}` |
|      ! 0 |  9887 | `			}else{` |
|   124515 |  9888 | `				if( pBase->iFlags & PH7_CLASS_ENUM ){` |
|        4 |  9889 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        1 |  9890 | `						"Class %z cannot extend enum %z",pName,&pBase->sName);` |
|        3 |  9891 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9892 | `						SyBlobRelease(&sResolved);` |
|      ! 0 |  9893 | `						return SXERR_ABORT;` |
|        - |  9894 | `					}` |
|        3 |  9895 | `					pBase = 0; /* Never inherit from an enum */` |
|   124514 |  9896 | `				}else if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|      ! 0 |  9897 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|      ! 0 |  9898 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|      ! 0 |  9899 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9900 | `						SyBlobRelease(&sResolved);` |
|      ! 0 |  9901 | `						return SXERR_ABORT;` |
|        - |  9902 | `					}` |
|      ! 0 |  9903 | `				}` |
|        - |  9904 | `			}` |
|   124515 |  9905 | `			SyBlobRelease(&sResolved);` |
|   124515 |  9906 | `			if( iFlags & PH7_CLASS_ENUM ){` |
|      ! 0 |  9907 | `				pBase = 0; /* Error already reported: enums have no base class */` |
|      ! 0 |  9908 | `			}` |
|    62255 |  9909 | `		}` |
|   171295 |  9910 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|        - |  9911 | `			ph7_class *pInterface;` |
|        - |  9912 | `			/* Interface implementation */` |
|    46797 |  9913 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    23408 |  9914 | `			for(;;){` |
|        - |  9915 | `				SyBlob sResolved;` |
|        - |  9916 | `				SyString sIntName;` |
|        - |  9917 | `				sxu32 nRefLine;` |
|    46809 |  9918 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    46809 |  9919 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    46809 |  9920 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 |  9921 | `					SyBlobRelease(&sResolved);` |
|      ! 0 |  9922 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - |  9923 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|      ! 0 |  9924 | `						pName);` |
|      ! 0 |  9925 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9926 | `						return SXERR_ABORT;` |
|        - |  9927 | `					}` |
|      ! 0 |  9928 | `					break;` |
|        - |  9929 | `				}` |
|    93613 |  9930 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    46804 |  9931 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    46809 |  9932 | `				SyStringInitFromBuf(&sIntName,` |
|        - |  9933 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|        - |  9934 | `				/* Only interfaces are allowed */` |
|    46809 |  9935 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 |  9936 | `					pInterface = pInterface->pNextName;` |
|      ! 0 |  9937 | `				}` |
|    46809 |  9938 | `				if( pInterface == 0 ){` |
|      ! 0 |  9939 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|        - |  9940 | `						"Nonexistent base interface '%z'",&sIntName);` |
|      ! 0 |  9941 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 |  9942 | `						SyBlobRelease(&sResolved);` |
|      ! 0 |  9943 | `						return SXERR_ABORT;` |
|        - |  9944 | `					}` |
|      ! 0 |  9945 | `				}else{` |
|        - |  9946 | `					/* Reject user classes that try to implement Throwable` |
|        - |  9947 | `					 * directly (or via an interface that extends Throwable)` |
|        - |  9948 | `					 * unless they already extend Exception or Error.` |
|        - |  9949 | `					 * Exception and Error themselves are compiled from the` |
|        - |  9950 | `					 * built-in library and are exempt by FQN — a namespaced` |
|        - |  9951 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    46809 |  9952 | `					SyString *pFqn = &pClass->sName;` |
|    46809 |  9953 | `					int bIsExceptionOrError =` |
|    27290 |  9954 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|    72152 |  9955 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    44869 |  9956 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|     3898 |  9957 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|    50693 |  9958 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|    11670 |  9959 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|     3887 |  9960 | `						!bIsExceptionOrError ){` |
|       12 |  9961 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  9962 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|        3 |  9963 | `							&pClass->sName);` |
|        9 |  9964 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 |  9965 | `							SyBlobRelease(&sResolved);` |
|      ! 0 |  9966 | `							return SXERR_ABORT;` |
|        - |  9967 | `						}` |
|        - |  9968 | `						/* Skip registration so the follow-up abstract-method` |
|        - |  9969 | `						 * check does not produce a duplicate fatal. */` |
|        6 |  9970 | `					}else{` |
|    46803 |  9971 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|        - |  9972 | `					}` |
|        - |  9973 | `				}` |
|    46809 |  9974 | `				SyBlobRelease(&sResolved);` |
|    46809 |  9975 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    23401 |  9976 | `					break;` |
|        - |  9977 | `				}` |
|       16 |  9978 | `				pGen->pIn++;/* Jump the comma */` |
|        4 |  9979 | `			}` |
|    23396 |  9980 | `		}` |
|    85645 |  9981 | `	}` |
|   215265 |  9982 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|        - |  9983 | `		/* Syntax error */` |
|      ! 0 |  9984 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|      ! 0 |  9985 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 |  9986 | `		if( rc == SXERR_ABORT ){` |
|        - |  9987 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  9988 | `			return SXERR_ABORT;` |
|        - |  9989 | `		}` |
|      ! 0 |  9990 | `		return SXRET_OK;` |
|        - |  9991 | `	}` |
|   215265 |  9992 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   215265 |  9993 | `	pEnd = 0; /* cc warning */` |
|        - |  9994 | `	/* Delimit the class body */` |
|   215265 |  9995 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   215265 |  9996 | `	if( pEnd >= pGen->pEnd ){` |
|        - |  9997 | `		/* Syntax error */` |
|      ! 0 |  9998 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|      ! 0 |  9999 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 10000 | `		if( rc == SXERR_ABORT ){` |
|        - | 10001 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 10002 | `			return SXERR_ABORT;` |
|        - | 10003 | `		}` |
|      ! 0 | 10004 | `		return SXRET_OK;` |
|        - | 10005 | `	}` |
|        - | 10006 | `	/* The delimiter token is the class body's closing brace */` |
|   215265 | 10007 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 10008 | `	/* Swap token stream */` |
|   215265 | 10009 | `	pTmp = pGen->pEnd;` |
|   215265 | 10010 | `	pGen->pEnd = pEnd;` |
|        - | 10011 | `	/* Merge the inherited flags (PH7_NewRawClass may have set INTERNAL) */` |
|   215265 | 10012 | `	pClass->iFlags \|= iFlags;` |
|        - | 10013 | `	/* Start the parse process */` |
|   826924 | 10014 | `	for(;;){` |
|        - | 10015 | `		/* Jump leading/trailing semi-colons */` |
|  2219003 | 10016 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   354509 | 10017 | `			pGen->pIn++;` |
|        5 | 10018 | `		}` |
|  1864499 | 10019 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 10020 | `			/* End of class body */` |
|   215223 | 10021 | `			break;` |
|        - | 10022 | `		}` |
|        - | 10023 | `		/* Bind a directly-preceding docblock to this member */` |
|  1649281 | 10024 | `		GenStateSetPendingDoc(&(*pGen));` |
|  1649276 | 10025 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|   824643 | 10026 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|      ! 0 | 10027 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10028 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|      ! 0 | 10029 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 10030 | `			if( rc == SXERR_ABORT ){` |
|        - | 10031 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 10032 | `				return SXERR_ABORT;` |
|        - | 10033 | `			}` |
|      ! 0 | 10034 | `			goto done;` |
|        - | 10035 | `		}` |
|        - | 10036 | `		/* Assume public visibility */` |
|  1649281 | 10037 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  1649281 | 10038 | `		iAttrflags = 0;` |
|        - | 10039 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|        - | 10040 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|        - | 10041 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|        - | 10042 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  1649281 | 10043 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      ! 0 | 10044 | `			int bMod = 0;` |
|      ! 0 | 10045 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      ! 0 | 10046 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|        - | 10047 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|        - | 10048 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|        - | 10049 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|        - | 10050 | `			 * that the generic keyword dispatch would misread as a method. */` |
|      ! 0 | 10051 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      ! 0 | 10052 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      ! 0 | 10053 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|      ! 0 | 10054 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|      ! 0 | 10055 | `			}` |
|      ! 0 | 10056 | `			if( !bMod ){` |
|      ! 0 | 10057 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 10058 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 10059 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10060 | `						return SXERR_ABORT;` |
|        - | 10061 | `					}` |
|      ! 0 | 10062 | `					goto done;` |
|        - | 10063 | `				}` |
|      ! 0 | 10064 | `				continue;` |
|        - | 10065 | `			}` |
|      ! 0 | 10066 | `		}` |
|  1649281 | 10067 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 10068 | `			/* Extract the current keyword */` |
|  1649281 | 10069 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1649281 | 10070 | `			if( nKwrd == PH7_TKWRD_CASE && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|        - | 10071 | ``				/* Enum case declaration: `case NAME [= value];` */`` |
|       47 | 10072 | `				rc = GenStateCompileEnumCase(&(*pGen),pClass);` |
|       47 | 10073 | `				if( rc != SXRET_OK ){` |
|        6 | 10074 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10075 | `						return SXERR_ABORT;` |
|        - | 10076 | `					}` |
|        6 | 10077 | `					goto done;` |
|        - | 10078 | `				}` |
|       41 | 10079 | `				continue;` |
|        - | 10080 | `			}` |
|  1649239 | 10081 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - | 10082 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|        - | 10083 | `				TraitUseEntry sUse;` |
|       63 | 10084 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|       63 | 10085 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|       63 | 10086 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|       37 | 10087 | `				for(;;){` |
|        - | 10088 | `					ph7_class *pTrait;` |
|        - | 10089 | `					SyString *pTraitName;` |
|       71 | 10090 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 10091 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 10092 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|      ! 0 | 10093 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10094 | `							return SXERR_ABORT;` |
|        - | 10095 | `						}` |
|      ! 0 | 10096 | `						break;` |
|        - | 10097 | `					}` |
|       71 | 10098 | `					pTraitName = &pGen->pIn->sData;` |
|        - | 10099 | `					/* Resolve trait name through namespace/imports */ {` |
|        - | 10100 | `						SyBlob sResolved;` |
|       71 | 10101 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       71 | 10102 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|      137 | 10103 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|       66 | 10104 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       71 | 10105 | `						SyBlobRelease(&sResolved);` |
|        - | 10106 | `					}` |
|        - | 10107 | `					/* Only traits are allowed */` |
|       71 | 10108 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 | 10109 | `						pTrait = pTrait->pNextName;` |
|      ! 0 | 10110 | `					}` |
|       71 | 10111 | `					if( pTrait == 0 ){` |
|      ! 0 | 10112 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 10113 | `							"'%z' is not a trait",pTraitName);` |
|      ! 0 | 10114 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10115 | `							return SXERR_ABORT;` |
|        - | 10116 | `						}` |
|      ! 0 | 10117 | `					}else{` |
|       71 | 10118 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|        - | 10119 | `					}` |
|       71 | 10120 | `					pGen->pIn++; /* Advance past trait name */` |
|       71 | 10121 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       34 | 10122 | `						break;` |
|        - | 10123 | `					}` |
|       10 | 10124 | `					pGen->pIn++; /* Jump the comma */` |
|        2 | 10125 | `				}` |
|        - | 10126 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|       63 | 10127 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|        - | 10128 | `					SyToken *pBlock;` |
|       13 | 10129 | `					pGen->pIn++; /* Jump '{' */` |
|       13 | 10130 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       13 | 10131 | `					sUse.pResolvStart = pGen->pIn;` |
|       13 | 10132 | `					sUse.pResolvEnd = pBlock;` |
|       13 | 10133 | `					if( pBlock < pGen->pEnd ){` |
|       13 | 10134 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|        8 | 10135 | `					}else{` |
|      ! 0 | 10136 | `						pGen->pIn = pGen->pEnd;` |
|        - | 10137 | `					}` |
|        5 | 10138 | `				}` |
|       63 | 10139 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|        - | 10140 | `				/* The semicolon will be consumed by the outer loop */` |
|       63 | 10141 | `				continue;` |
|        - | 10142 | `			}` |
|  1649181 | 10143 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        - | 10144 | `				int nSetTok;` |
|  1505001 | 10145 | `				sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|  1505001 | 10146 | `				if( nSetVis ){` |
|        - | 10147 | ``					/* Leading `private(set)`/`protected(set)` with no read`` |
|        - | 10148 | `					 * visibility: the read side defaults to public (php 8.4). */` |
|        3 | 10149 | `					iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|        3 | 10150 | `					pGen->pIn += nSetTok;` |
|        2 | 10151 | `				}else{` |
|  1504999 | 10152 | `					iProtection = nKwrd;` |
|  1504999 | 10153 | `					pGen->pIn++; /* Jump the visibility token */` |
|        - | 10154 | `					/* Optional asymmetric set-visibility after the read` |
|        - | 10155 | ``					 * visibility: `public private(set) int $x`. */`` |
|  1504999 | 10156 | `					nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|  1504999 | 10157 | `					if( nSetVis ){` |
|        9 | 10158 | `						iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|        9 | 10159 | `						pGen->pIn += nSetTok;` |
|        4 | 10160 | `					}` |
|        - | 10161 | `				}` |
|        - | 10162 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`,`` |
|        - | 10163 | ``				 * `public private(set) readonly int $x`. */`` |
|  1505001 | 10164 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|       24 | 10165 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|       24 | 10166 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       10 | 10167 | `				}` |
|  1504996 | 10168 | `				if( pGen->pIn >= pGen->pEnd` |
|  1505001 | 10169 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 10170 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10171 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|      ! 0 | 10172 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 10173 | `					if( rc == SXERR_ABORT ){` |
|        - | 10174 | `						/* Error count limit reached,abort immediately */` |
|      ! 0 | 10175 | `						return SXERR_ABORT;` |
|        - | 10176 | `					}` |
|      ! 0 | 10177 | `					goto done;` |
|        - | 10178 | `				}` |
|  1505001 | 10179 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        - | 10180 | `					/* Attribute declaration (untyped) */` |
|   210329 | 10181 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   210329 | 10182 | `					if( rc != SXRET_OK ){` |
|       11 | 10183 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10184 | `							return SXERR_ABORT;` |
|        - | 10185 | `						}` |
|       11 | 10186 | `						goto done;` |
|        - | 10187 | `					}` |
|   210414 | 10188 | `					continue;` |
|        - | 10189 | `				}` |
|  1294677 | 10190 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        - | 10191 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|      197 | 10192 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      197 | 10193 | `					if( rc != SXRET_OK ){` |
|        8 | 10194 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 10195 | `							return SXERR_ABORT;` |
|        - | 10196 | `						}` |
|        8 | 10197 | `						goto done;` |
|        - | 10198 | `					}` |
|      191 | 10199 | `					continue;` |
|        - | 10200 | `				}` |
|        - | 10201 | `				/* Extract the keyword */` |
|  1294485 | 10202 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   647240 | 10203 | `			}` |
|  1438665 | 10204 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|        - | 10205 | `				/* Process constant declaration */` |
|   143863 | 10206 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|   143863 | 10207 | `				if( rc != SXRET_OK ){` |
|       11 | 10208 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10209 | `						return SXERR_ABORT;` |
|        - | 10210 | `					}` |
|       11 | 10211 | `					goto done;` |
|        - | 10212 | `				}` |
|    71930 | 10213 | `			}else{` |
|  1294807 | 10214 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|        - | 10215 | `					/* Static method or attribute,record that */` |
|    23445 | 10216 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    23445 | 10217 | `					pGen->pIn++; /* Jump the static keyword */` |
|    23445 | 10218 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 10219 | `						int nSetTok;` |
|    23417 | 10220 | `						sxi32 nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|    23417 | 10221 | `						if( nSetVis ){` |
|        - | 10222 | ``							/* `static private(set) int $x` — read side stays public */`` |
|        3 | 10223 | `							iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|        3 | 10224 | `							pGen->pIn += nSetTok;` |
|        2 | 10225 | `						}else{` |
|        - | 10226 | `							/* Extract the keyword */` |
|    23415 | 10227 | `							nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    23415 | 10228 | `							if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      ! 0 | 10229 | `								iProtection = nKwrd;` |
|      ! 0 | 10230 | `								pGen->pIn++; /* Jump the visibility token */` |
|      ! 0 | 10231 | `								nSetVis = GenStatePeekSetVisibility(pGen->pIn,pGen->pEnd,&nSetTok);` |
|      ! 0 | 10232 | `								if( nSetVis ){` |
|      ! 0 | 10233 | `									iAttrflags \|= GenStateSetVisFlag(nSetVis);` |
|      ! 0 | 10234 | `									pGen->pIn += nSetTok;` |
|      ! 0 | 10235 | `								}` |
|      ! 0 | 10236 | `							}` |
|        - | 10237 | `						}` |
|    11706 | 10238 | `					}` |
|        - | 10239 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|        - | 10240 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|        - | 10241 | `					 * than a generic "expecting method" parse error. */` |
|    23445 | 10242 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      ! 0 | 10243 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      ! 0 | 10244 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|      ! 0 | 10245 | `					}` |
|    23440 | 10246 | `					if( pGen->pIn >= pGen->pEnd` |
|    23445 | 10247 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 10248 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10249 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|      ! 0 | 10250 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 10251 | `						if( rc == SXERR_ABORT ){` |
|        - | 10252 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 10253 | `							return SXERR_ABORT;` |
|        - | 10254 | `						}` |
|      ! 0 | 10255 | `						goto done;` |
|        - | 10256 | `					}` |
|    23445 | 10257 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        - | 10258 | `						/* Attribute declaration */` |
|       29 | 10259 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       29 | 10260 | `						if( rc != SXRET_OK ){` |
|        3 | 10261 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 10262 | `								return SXERR_ABORT;` |
|        - | 10263 | `							}` |
|        3 | 10264 | `							goto done;` |
|        - | 10265 | `						}` |
|       26 | 10266 | `						continue;` |
|        - | 10267 | `					}` |
|    23419 | 10268 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        - | 10269 | `						/* Typed static attribute declaration */` |
|       17 | 10270 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       17 | 10271 | `						if( rc != SXRET_OK ){` |
|        3 | 10272 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 10273 | `								return SXERR_ABORT;` |
|        - | 10274 | `							}` |
|        3 | 10275 | `							goto done;` |
|        - | 10276 | `						}` |
|       15 | 10277 | `						continue;` |
|        - | 10278 | `					}` |
|        - | 10279 | `					/* Extract the keyword */` |
|    23405 | 10280 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1283067 | 10281 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|        - | 10282 | `					/* Abstract method,record that */` |
|       15 | 10283 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|        - | 10284 | `					/* Mark the whole class as abstract */` |
|       15 | 10285 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|        - | 10286 | `					/* Advance the stream cursor */` |
|       15 | 10287 | `					pGen->pIn++;` |
|       15 | 10288 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       15 | 10289 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       15 | 10290 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       13 | 10291 | `							iProtection = nKwrd;` |
|       13 | 10292 | `							pGen->pIn++; /* Jump the visibility token */` |
|        5 | 10293 | `						}` |
|        6 | 10294 | `					}` |
|       15 | 10295 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       12 | 10296 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - | 10297 | `							/* Static method */` |
|      ! 0 | 10298 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 | 10299 | `							pGen->pIn++; /* Jump the static keyword */` |
|      ! 0 | 10300 | `					}` |
|       15 | 10301 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       12 | 10302 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 10303 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10304 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|      ! 0 | 10305 | `								&pGen->pIn->sData,pName);` |
|      ! 0 | 10306 | `							if( rc == SXERR_ABORT ){` |
|        - | 10307 | `								/* Error count limit reached,abort immediately */` |
|      ! 0 | 10308 | `								return SXERR_ABORT;` |
|        - | 10309 | `							}` |
|      ! 0 | 10310 | `							goto done;` |
|        - | 10311 | `					}` |
|       15 | 10312 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  1271361 | 10313 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|        - | 10314 | `					/* final method ,record that */` |
|       21 | 10315 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       21 | 10316 | `					pGen->pIn++; /* Jump the final keyword */` |
|       21 | 10317 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        - | 10318 | `						/* Extract the keyword */` |
|       21 | 10319 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       21 | 10320 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       11 | 10321 | `							iProtection = nKwrd;` |
|       11 | 10322 | `							pGen->pIn++; /* Jump the visibility token */` |
|        4 | 10323 | `						}` |
|        9 | 10324 | `					}` |
|       21 | 10325 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       18 | 10326 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|        - | 10327 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|        - | 10328 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|        - | 10329 | `							 * child class is compiled (PH7_ClassInherit). */` |
|       14 | 10330 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|       14 | 10331 | `							if( rc != SXRET_OK ){` |
|      ! 0 | 10332 | `								if( rc == SXERR_ABORT ){` |
|      ! 0 | 10333 | `									return SXERR_ABORT;` |
|        - | 10334 | `								}` |
|      ! 0 | 10335 | `								goto done;` |
|        - | 10336 | `							}` |
|       14 | 10337 | `							continue;` |
|        - | 10338 | `					}` |
|        9 | 10339 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        6 | 10340 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - | 10341 | `							/* Static method */` |
|      ! 0 | 10342 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 | 10343 | `							pGen->pIn++; /* Jump the static keyword */` |
|      ! 0 | 10344 | `					}` |
|        9 | 10345 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|        6 | 10346 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 10347 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10348 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|      ! 0 | 10349 | `								&pGen->pIn->sData,pName);` |
|      ! 0 | 10350 | `							if( rc == SXERR_ABORT ){` |
|        - | 10351 | `								/* Error count limit reached,abort immediately */` |
|      ! 0 | 10352 | `								return SXERR_ABORT;` |
|        - | 10353 | `							}` |
|      ! 0 | 10354 | `							goto done;` |
|        - | 10355 | `					}` |
|        9 | 10356 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|        3 | 10357 | `				}` |
|  1294755 | 10358 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|      ! 0 | 10359 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10360 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|      ! 0 | 10361 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 10362 | `						if( rc == SXERR_ABORT ){` |
|        - | 10363 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 10364 | `							return SXERR_ABORT;` |
|        - | 10365 | `						}` |
|      ! 0 | 10366 | `						goto done;` |
|        - | 10367 | `				}` |
|  1294755 | 10368 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|        7 | 10369 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|        7 | 10370 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|      ! 0 | 10371 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10372 | `							"Expecting attribute declaration after 'var' keyword");` |
|      ! 0 | 10373 | `						if( rc == SXERR_ABORT ){` |
|        - | 10374 | `							/* Error count limit reached,abort immediately */` |
|      ! 0 | 10375 | `							return SXERR_ABORT;` |
|        - | 10376 | `						}` |
|      ! 0 | 10377 | `						goto done;` |
|        - | 10378 | `					}` |
|        - | 10379 | `					/* Attribute declaration */` |
|        7 | 10380 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        4 | 10381 | `				}else{` |
|        - | 10382 | `					/* Process method declaration */` |
|  1294749 | 10383 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|        - | 10384 | `				}` |
|  1294755 | 10385 | `				if( rc != SXRET_OK ){` |
|       16 | 10386 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 10387 | `						return SXERR_ABORT;` |
|        - | 10388 | `					}` |
|       16 | 10389 | `					goto done;` |
|        - | 10390 | `				}` |
|        - | 10391 | `			}` |
|   719299 | 10392 | `		}else{` |
|        - | 10393 | `			/* Attribute declaration */` |
|      ! 0 | 10394 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 10395 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 10396 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10397 | `					return SXERR_ABORT;` |
|        - | 10398 | `				}` |
|      ! 0 | 10399 | `				goto done;` |
|        - | 10400 | `			}` |
|        - | 10401 | `		}` |
|        5 | 10402 | `	}` |
|        - | 10403 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|        - | 10404 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|        - | 10405 | `	 */` |
|        - | 10406 | `	{` |
|        - | 10407 | `		TraitUseEntry *apUse;` |
|        - | 10408 | `		sxu32 nU;` |
|   215223 | 10409 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   215281 | 10410 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|       63 | 10411 | `			TraitUseEntry *pUse = &apUse[nU];` |
|       63 | 10412 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|       63 | 10413 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|       63 | 10414 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|        - | 10415 | `			sxu32 nT;` |
|       63 | 10416 | `			if( !hasResolution ){` |
|        - | 10417 | `				/* No conflict resolution block: use standard trait application */` |
|      107 | 10418 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       59 | 10419 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|       59 | 10420 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 10421 | `						break;` |
|        - | 10422 | `					}` |
|       32 | 10423 | `				}` |
|       29 | 10424 | `			}else{` |
|        - | 10425 | `				/* With resolution block: copy attributes, record traits,` |
|        - | 10426 | `				 * then use the block to resolve method conflicts.` |
|        - | 10427 | `				 */` |
|        - | 10428 | `				SyToken *pR;` |
|       25 | 10429 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       15 | 10430 | `					ph7_class *pTR = apTrait[nT];` |
|        - | 10431 | `					ph7_class_attr *pAR;` |
|        - | 10432 | `					SyHashEntry *pER;` |
|        - | 10433 | `					SyString *pNR;` |
|       15 | 10434 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|       21 | 10435 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|      ! 0 | 10436 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|      ! 0 | 10437 | `						pNR = &pAR->sName;` |
|      ! 0 | 10438 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      ! 0 | 10439 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|      ! 0 | 10440 | `						}` |
|      ! 0 | 10441 | `					}` |
|       15 | 10442 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|        9 | 10443 | `				}` |
|        - | 10444 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       13 | 10445 | `				pR = pUse->pResolvStart;` |
|       27 | 10446 | `				while( pR < pUse->pResolvEnd ){` |
|        - | 10447 | `					SyString sTrait,sMethod;` |
|        - | 10448 | `					ph7_class *pSrcTrait;` |
|        - | 10449 | `					ph7_class_method *pMeth;` |
|        - | 10450 | `					sxi32 nRKwrd;` |
|       41 | 10451 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|       27 | 10452 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       17 | 10453 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       17 | 10454 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       17 | 10455 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       17 | 10456 | `					sMethod = pR->sData;` |
|       17 | 10457 | `					pR++;` |
|       17 | 10458 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        7 | 10459 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        7 | 10460 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        7 | 10461 | `							sTrait = sMethod;` |
|        7 | 10462 | `							pR++;` |
|        7 | 10463 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        7 | 10464 | `							sMethod = pR->sData;` |
|        7 | 10465 | `							pR++;` |
|        3 | 10466 | `						}` |
|        3 | 10467 | `					}` |
|       17 | 10468 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 10469 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 10470 | `						continue;` |
|        - | 10471 | `					}` |
|       17 | 10472 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       17 | 10473 | `					pR++;` |
|       17 | 10474 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|        5 | 10475 | `						pSrcTrait = 0;` |
|        7 | 10476 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        7 | 10477 | `							SyString *pTN = &apTrait[nT]->sName;` |
|       10 | 10478 | `							if( pTN->nByte >= sTrait.nByte &&` |
|        6 | 10479 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|        5 | 10480 | `								pSrcTrait = apTrait[nT];` |
|        5 | 10481 | `								break;` |
|        - | 10482 | `							}` |
|        2 | 10483 | `						}` |
|        5 | 10484 | `						if( pSrcTrait ){` |
|        5 | 10485 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        5 | 10486 | `							if( pMeth ){` |
|        5 | 10487 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|        5 | 10488 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|        5 | 10489 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|        2 | 10490 | `								}` |
|        2 | 10491 | `							}` |
|        2 | 10492 | `						}` |
|        2 | 10493 | `					}` |
|       35 | 10494 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        3 | 10495 | `				}` |
|        - | 10496 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|       25 | 10497 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        - | 10498 | `					ph7_class_method *pMR;` |
|        - | 10499 | `					SyHashEntry *pER;` |
|        - | 10500 | `					SyString *pNR;` |
|       15 | 10501 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|       41 | 10502 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|       23 | 10503 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|       23 | 10504 | `						pNR = &pMR->sFunc.sName;` |
|       23 | 10505 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|       14 | 10506 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|        6 | 10507 | `						}` |
|        3 | 10508 | `					}` |
|        9 | 10509 | `				}` |
|        - | 10510 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       13 | 10511 | `				pR = pUse->pResolvStart;` |
|       27 | 10512 | `				while( pR < pUse->pResolvEnd ){` |
|        - | 10513 | `					SyString sTrait,sMethod,sAlias;` |
|        - | 10514 | `					ph7_class *pSrcTrait;` |
|        - | 10515 | `					ph7_class_method *pMeth;` |
|       27 | 10516 | `					int hasQual = 0;` |
|        - | 10517 | `					sxi32 nRKwrd;` |
|       41 | 10518 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|       27 | 10519 | `					if( pR >= pUse->pResolvEnd ) break;` |
|       17 | 10520 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|       17 | 10521 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|       17 | 10522 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|       17 | 10523 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|       17 | 10524 | `					sMethod = pR->sData;` |
|       17 | 10525 | `					pR++;` |
|       17 | 10526 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|        7 | 10527 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|        7 | 10528 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|        7 | 10529 | `							sTrait = sMethod;` |
|        7 | 10530 | `							hasQual = 1;` |
|        7 | 10531 | `							pR++;` |
|        7 | 10532 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|        7 | 10533 | `							sMethod = pR->sData;` |
|        7 | 10534 | `							pR++;` |
|        3 | 10535 | `						}` |
|        3 | 10536 | `					}` |
|       17 | 10537 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 10538 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|      ! 0 | 10539 | `						continue;` |
|        - | 10540 | `					}` |
|       17 | 10541 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|       17 | 10542 | `					pR++;` |
|       17 | 10543 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       13 | 10544 | `						sxi32 iNewVis = -1;` |
|       13 | 10545 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|        7 | 10546 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|        7 | 10547 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|        7 | 10548 | `								iNewVis = nAK;` |
|        7 | 10549 | `								pR++;` |
|        3 | 10550 | `							}` |
|        3 | 10551 | `						}` |
|       13 | 10552 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       11 | 10553 | `							sAlias = pR->sData;` |
|       11 | 10554 | `							pR++;` |
|        4 | 10555 | `						}` |
|       13 | 10556 | `						pMeth = 0;` |
|       13 | 10557 | `						if( hasQual ){` |
|        3 | 10558 | `							pSrcTrait = 0;` |
|        5 | 10559 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|        5 | 10560 | `								SyString *pTN = &apTrait[nT]->sName;` |
|        7 | 10561 | `								if( pTN->nByte >= sTrait.nByte &&` |
|        4 | 10562 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|        3 | 10563 | `									pSrcTrait = apTrait[nT];` |
|        3 | 10564 | `									break;` |
|        - | 10565 | `								}` |
|        2 | 10566 | `							}` |
|        3 | 10567 | `							if( pSrcTrait ){` |
|        3 | 10568 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|        1 | 10569 | `							}` |
|        2 | 10570 | `						}else{` |
|       10 | 10571 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|        - | 10572 | `						}` |
|       13 | 10573 | `						if( pMeth ){` |
|       13 | 10574 | `							if( sAlias.nByte > 0 ){` |
|        - | 10575 | `								/* Create a shallow copy of the method struct for the alias` |
|        - | 10576 | `								 * so it can carry its own visibility without affecting the original.` |
|        - | 10577 | `								 */` |
|        - | 10578 | `								ph7_class_method *pAlias;` |
|        - | 10579 | `								char *zAliasDup;` |
|       11 | 10580 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       11 | 10581 | `								if( pAlias ){` |
|       11 | 10582 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       11 | 10583 | `									if( iNewVis >= 0 ){` |
|        5 | 10584 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|        3 | 10585 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|      ! 0 | 10586 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|        2 | 10587 | `									}` |
|       11 | 10588 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       11 | 10589 | `									if( zAliasDup ){` |
|       11 | 10590 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|        4 | 10591 | `									}` |
|        7 | 10592 | `								}` |
|        7 | 10593 | `							}else if( iNewVis >= 0 ){` |
|        - | 10594 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|        - | 10595 | `								ph7_class_method *pCopy;` |
|        3 | 10596 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|        3 | 10597 | `								if( pCopy ){` |
|        3 | 10598 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|        3 | 10599 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|        3 | 10600 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|        3 | 10601 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|      ! 0 | 10602 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|        - | 10603 | `									/* Replace the method in the class hash */` |
|        3 | 10604 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|        3 | 10605 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|        1 | 10606 | `								}` |
|        1 | 10607 | `							}` |
|        5 | 10608 | `						}` |
|        5 | 10609 | `						SXUNUSED(hasQual);` |
|        5 | 10610 | `					}` |
|       21 | 10611 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|        3 | 10612 | `				}` |
|        - | 10613 | `			}` |
|       63 | 10614 | `			SySetRelease(&pUse->aTraits);` |
|       34 | 10615 | `		}` |
|        - | 10616 | `	}` |
|   215223 | 10617 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|        - | 10618 | `		/* Enum validation + name/value props + cases()/from()/tryFrom() synthesis.` |
|        - | 10619 | `		 * Runs after trait application so trait-imported properties are caught. */` |
|       27 | 10620 | `		rc = GenStateEnumFinalize(&(*pGen),pClass,nLine);` |
|       27 | 10621 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10622 | `			SySetRelease(&aUseEntries);` |
|      ! 0 | 10623 | `			SySetRelease(&aInterfaces);` |
|      ! 0 | 10624 | `			return SXERR_ABORT;` |
|        - | 10625 | `		}` |
|       12 | 10626 | `	}` |
|        - | 10627 | `	/* Install the class */` |
|   215223 | 10628 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   215223 | 10629 | `	if( rc == SXRET_OK ){` |
|        - | 10630 | `		ph7_class **apInterface;` |
|        - | 10631 | `		sxu32 n;` |
|   215223 | 10632 | `		if( pBase ){` |
|        - | 10633 | `			/* Inherit from base class and mark as a subclass */` |
|   124513 | 10634 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|    62254 | 10635 | `		}` |
|   215223 | 10636 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   262021 | 10637 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|        - | 10638 | `			/* Implements one or more interface */` |
|    46803 | 10639 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    46803 | 10640 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 10641 | `				break;` |
|        - | 10642 | `			}` |
|    23404 | 10643 | `		}` |
|        - | 10644 | `		/* Auto-implement UnitEnum (and BackedEnum for backed enums) — php 8.1:` |
|        - | 10645 | ``		 * every enum satisfies `instanceof UnitEnum` implicitly. */`` |
|   215223 | 10646 | `		if( rc == SXRET_OK && (pClass->iFlags & PH7_CLASS_ENUM) ){` |
|       27 | 10647 | `			ph7_class *pIntf = PH7_VmExtractClass(pGen->pVm,"UnitEnum",sizeof("UnitEnum")-1,FALSE,0);` |
|       27 | 10648 | `			while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 10649 | `				pIntf = pIntf->pNextName;` |
|      ! 0 | 10650 | `			}` |
|       27 | 10651 | `			if( pIntf ){` |
|       27 | 10652 | `				PH7_ClassImplement(pClass,pIntf);` |
|       12 | 10653 | `			}` |
|       27 | 10654 | `			if( pClass->nEnumBacking != 0 ){` |
|       13 | 10655 | `				pIntf = PH7_VmExtractClass(pGen->pVm,"BackedEnum",sizeof("BackedEnum")-1,FALSE,0);` |
|       13 | 10656 | `				while( pIntf && (pIntf->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      ! 0 | 10657 | `					pIntf = pIntf->pNextName;` |
|      ! 0 | 10658 | `				}` |
|       13 | 10659 | `				if( pIntf ){` |
|       13 | 10660 | `					PH7_ClassImplement(pClass,pIntf);` |
|        6 | 10661 | `				}` |
|        6 | 10662 | `			}` |
|       12 | 10663 | `		}` |
|        - | 10664 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|        - | 10665 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|   215218 | 10666 | `		if( rc == SXRET_OK` |
|   215218 | 10667 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|   215223 | 10668 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   171003 | 10669 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|        - | 10670 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   171003 | 10671 | `			if( pStringable ){` |
|   171003 | 10672 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   171003 | 10673 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|        - | 10674 | `				sxu32 i;` |
|   171003 | 10675 | `				int bAlready = 0;` |
|   209847 | 10676 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    42735 | 10677 | `					if( apImpl[i] == pStringable ){` |
|     3891 | 10678 | `						bAlready = 1;` |
|     3891 | 10679 | `						break;` |
|        - | 10680 | `					}` |
|    19427 | 10681 | `				}` |
|   171003 | 10682 | `				if( !bAlready ){` |
|   167117 | 10683 | `					PH7_ClassImplement(pClass,pStringable);` |
|    83556 | 10684 | `				}` |
|    85499 | 10685 | `			}` |
|    85499 | 10686 | `		}` |
|        - | 10687 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   215223 | 10688 | `		if( rc == SXRET_OK ){` |
|   215223 | 10689 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   215223 | 10690 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 10691 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 10692 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 10693 | `				return SXERR_ABORT;` |
|        - | 10694 | `			}` |
|   107609 | 10695 | `		}` |
|        - | 10696 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   215223 | 10697 | `		if( rc == SXRET_OK ){` |
|   215223 | 10698 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   215223 | 10699 | `			if( rcCheck == SXERR_ABORT ){` |
|      ! 0 | 10700 | `				SySetRelease(&aUseEntries);` |
|      ! 0 | 10701 | `				SySetRelease(&aInterfaces);` |
|      ! 0 | 10702 | `				return SXERR_ABORT;` |
|        - | 10703 | `			}` |
|   107609 | 10704 | `		}` |
|   107609 | 10705 | `	}` |
|   215223 | 10706 | `	SySetRelease(&aUseEntries);` |
|   215223 | 10707 | `	SySetRelease(&aInterfaces);` |
|   215223 | 10708 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10709 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 10710 | `		return SXERR_ABORT;` |
|        - | 10711 | `	}` |
|   107609 | 10712 | `done:` |
|        - | 10713 | `	/* Point beyond the class body */` |
|   215265 | 10714 | `	pGen->pIn = &pEnd[1];` |
|   215265 | 10715 | `	pGen->pEnd = pTmp;` |
|   215265 | 10716 | `	return PH7_OK;` |
|   107636 | 10717 | `}` |
|        - | 10718 | `/* Compile a named class declaration (the common case). */` |
|   215234 | 10719 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|        5 | 10720 | `{` |
|   215239 | 10721 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|        5 | 10722 | `}` |
|        - | 10723 | `/*` |
|        - | 10724 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|        - | 10725 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|        - | 10726 | ` * compile + install the class body once (at compile time, like every other` |
|        - | 10727 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|        - | 10728 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|        - | 10729 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|        - | 10730 | ` */` |
|       28 | 10731 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        4 | 10732 | `{` |
|        - | 10733 | `	char zName[128];         /* Synthesized class name */` |
|        - | 10734 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|        - | 10735 | `	SyString sName;` |
|        - | 10736 | `	SyToken *pArgStart,*pArgEnd;` |
|       32 | 10737 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `new #[A] class` trivia`` |
|        - | 10738 | `	                              * is keyed to this 'class' token */` |
|        - | 10739 | `	ph7_value *pObj;` |
|       32 | 10740 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 10741 | `	sxu32 nIdx,nLen;` |
|        - | 10742 | `	sxi32 nArg,rc;` |
|       14 | 10743 | `	SXUNUSED(iCompileFlag);` |
|        - | 10744 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|       32 | 10745 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|       32 | 10746 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 | 10747 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      ! 0 | 10748 | `	}` |
|       32 | 10749 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|        - | 10750 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|        - | 10751 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|        - | 10752 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|       32 | 10753 | `	pArgStart = pArgEnd = 0;` |
|       32 | 10754 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|       32 | 10755 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10756 | `		return rc;` |
|        - | 10757 | `	}` |
|        - | 10758 | `	{` |
|        - | 10759 | ``		/* Expression-position attributes (`new #[A] class {…}`) */`` |
|       32 | 10760 | `		ph7_class *pAnonClass = PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0);` |
|       28 | 10761 | `		if( pAnonClass` |
|       32 | 10762 | `		 && GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnonClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 10763 | `			return SXERR_ABORT;` |
|        - | 10764 | `		}` |
|        - | 10765 | `	}` |
|        - | 10766 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|        - | 10767 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|       32 | 10768 | `	nArg = 0;` |
|       32 | 10769 | `	if( pArgStart < pArgEnd ){` |
|        7 | 10770 | `		SyToken *pSavedIn = pGen->pIn;` |
|        7 | 10771 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|        - | 10772 | `		SyToken *pArgNext;` |
|        7 | 10773 | `		pGen->pIn = pArgStart;` |
|        7 | 10774 | `		pGen->pEnd = pArgEnd;` |
|       13 | 10775 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|        7 | 10776 | `			if( pGen->pIn < pArgNext ){` |
|        7 | 10777 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|        7 | 10778 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10779 | `					pGen->pIn = pSavedIn;` |
|      ! 0 | 10780 | `					pGen->pEnd = pSavedEnd;` |
|      ! 0 | 10781 | `					return SXERR_ABORT;` |
|        - | 10782 | `				}` |
|        7 | 10783 | `				nArg++;` |
|        3 | 10784 | `			}` |
|        7 | 10785 | `			pGen->pIn = &pArgNext[1];` |
|        1 | 10786 | `		}` |
|        7 | 10787 | `		pGen->pIn = pSavedIn;` |
|        7 | 10788 | `		pGen->pEnd = pSavedEnd;` |
|        3 | 10789 | `	}` |
|        - | 10790 | `	/* Load the synthesized class name */` |
|       32 | 10791 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       32 | 10792 | `	if( pObj == 0 ){` |
|      ! 0 | 10793 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 10794 | `		return SXERR_ABORT;` |
|        - | 10795 | `	}` |
|       32 | 10796 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       32 | 10797 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - | 10798 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|       32 | 10799 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       32 | 10800 | `	return SXRET_OK;` |
|       18 | 10801 | `}` |
|        - | 10802 | `/*` |
|        - | 10803 | ` * Compile a user-defined abstract class.` |
|        - | 10804 | ` *  According to the PHP language reference manual` |
|        - | 10805 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|        - | 10806 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|        - | 10807 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|        - | 10808 | ` *   the method's signature - they cannot define the implementation.` |
|        - | 10809 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|        - | 10810 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|        - | 10811 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|        - | 10812 | ` *   method is defined as protected, the function implementation must be defined as either` |
|        - | 10813 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|        - | 10814 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|        - | 10815 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|        - | 10816 | ` *   could differ.` |
|        - | 10817 | ` */` |
|        - | 10818 | `/*` |
|        - | 10819 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|        - | 10820 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|        - | 10821 | ` * receives the corresponding PH7_CLASS_* bit.` |
|        - | 10822 | ` */` |
|  6332716 | 10823 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|        5 | 10824 | `{` |
|  6332721 | 10825 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  3939405 | 10826 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  3939405 | 10827 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  3892771 | 10828 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  1938578 | 10829 | `	}` |
|  6270477 | 10830 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  6270417 | 10831 | `	return FALSE;` |
|  3166363 | 10832 | `}` |
|        - | 10833 | `/*` |
|        - | 10834 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|        - | 10835 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|        - | 10836 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|        - | 10837 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|        - | 10838 | ` */` |
|  6270412 | 10839 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|        5 | 10840 | `{` |
|  6270417 | 10841 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  6270417 | 10842 | `	sxi32 iFlags = 0,iFlag;` |
|  6332721 | 10843 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|    62309 | 10844 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|        5 | 10845 | `			pDup = pIn;` |
|        2 | 10846 | `		}` |
|    62309 | 10847 | `		iFlags \|= iFlag;` |
|    62309 | 10848 | `		pIn++;` |
|        5 | 10849 | `	}` |
|  6270417 | 10850 | `	*ppIn = pIn;` |
|  6270417 | 10851 | `	if( ppDup ){ *ppDup = pDup; }` |
|  6270417 | 10852 | `	return iFlags;` |
|        5 | 10853 | `}` |
|        - | 10854 | `/*` |
|        - | 10855 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|        - | 10856 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|        - | 10857 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|        - | 10858 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|        - | 10859 | `` * `readonly`) to their existing handlers.`` |
|        - | 10860 | ` */` |
|  6243154 | 10861 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|        5 | 10862 | `{` |
|  6243159 | 10863 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  3156610 | 10864 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  6260669 | 10865 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|        5 | 10866 | `}` |
|        - | 10867 | `/*` |
|        - | 10868 | ` * Compile a class declaration carrying one or more leading modifiers` |
|        - | 10869 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|        - | 10870 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|        - | 10871 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|        - | 10872 | `` * `abstract`+`final` pair, like PHP.`` |
|        - | 10873 | ` */` |
|    27258 | 10874 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|        5 | 10875 | `{` |
|        - | 10876 | `	SyToken *pDup;` |
|    27263 | 10877 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|        - | 10878 | `	sxi32 rc;` |
|    27263 | 10879 | `	if( pDup ){` |
|        4 | 10880 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|        2 | 10881 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|        3 | 10882 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10883 | `			return SXERR_ABORT;` |
|        - | 10884 | `		}` |
|        1 | 10885 | `	}` |
|    27258 | 10886 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|    13634 | 10887 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|        3 | 10888 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10889 | `			"Cannot use the final modifier on an abstract class");` |
|        3 | 10890 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10891 | `			return SXERR_ABORT;` |
|        - | 10892 | `		}` |
|        1 | 10893 | `	}` |
|    27263 | 10894 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|    13634 | 10895 | `}` |
|        - | 10896 | `/*` |
|        - | 10897 | ` * Compile a user-defined trait.` |
|        - | 10898 | ` *  Traits are similar to classes, but only intended to group functionality` |
|        - | 10899 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|        - | 10900 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|        - | 10901 | ` */` |
|       72 | 10902 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|        5 | 10903 | `{` |
|       77 | 10904 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 10905 | `	ph7_class *pClass;` |
|        - | 10906 | `	SyToken *pEnd,*pTmp;` |
|        - | 10907 | `	sxi32 iProtection;` |
|        - | 10908 | `	sxi32 iAttrflags;` |
|        - | 10909 | `	SyString *pName;` |
|        - | 10910 | `	sxi32 nKwrd;` |
|        - | 10911 | `	sxi32 rc;` |
|        - | 10912 | `	/* Jump the 'trait' keyword */` |
|       77 | 10913 | `	pGen->pIn++;` |
|       77 | 10914 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 10915 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|      ! 0 | 10916 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10917 | `			return SXERR_ABORT;` |
|        - | 10918 | `		}` |
|      ! 0 | 10919 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|      ! 0 | 10920 | `			pGen->pIn++;` |
|      ! 0 | 10921 | `		}` |
|      ! 0 | 10922 | `		return SXRET_OK;` |
|        - | 10923 | `	}` |
|        - | 10924 | `	/* Extract trait name */` |
|       77 | 10925 | `	pName = &pGen->pIn->sData;` |
|       77 | 10926 | `	pGen->pIn++;` |
|        - | 10927 | `	/* Build FQN and obtain a raw class */ {` |
|        - | 10928 | `		SyBlob sFQN;` |
|        - | 10929 | `		SyString sFQNStr;` |
|       77 | 10930 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       77 | 10931 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|       77 | 10932 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|       77 | 10933 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|       77 | 10934 | `		SyBlobRelease(&sFQN);` |
|        - | 10935 | `	}` |
|       77 | 10936 | `	if( pClass == 0 ){` |
|      ! 0 | 10937 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 10938 | `		return SXERR_ABORT;` |
|        - | 10939 | `	}` |
|       77 | 10940 | `	GenStateConsumeDoc(&(*pGen),&pClass->sDoc);` |
|       77 | 10941 | `	if( GenStateConsumeAttrs(&(*pGen),&pClass->aAttrs) == SXERR_ABORT ){` |
|      ! 0 | 10942 | `		return SXERR_ABORT;` |
|        - | 10943 | `	}` |
|        - | 10944 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|       77 | 10945 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|      ! 0 | 10946 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|      ! 0 | 10947 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 10948 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10949 | `			return SXERR_ABORT;` |
|        - | 10950 | `		}` |
|      ! 0 | 10951 | `		return SXRET_OK;` |
|        - | 10952 | `	}` |
|       77 | 10953 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|       77 | 10954 | `	pEnd = 0;` |
|       77 | 10955 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|       77 | 10956 | `	if( pEnd >= pGen->pEnd ){` |
|      ! 0 | 10957 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|      ! 0 | 10958 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|      ! 0 | 10959 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10960 | `			return SXERR_ABORT;` |
|        - | 10961 | `		}` |
|      ! 0 | 10962 | `		return SXRET_OK;` |
|        - | 10963 | `	}` |
|        - | 10964 | `	/* The delimiter token is the trait body's closing brace */` |
|       77 | 10965 | `	pClass->nEndLine = pEnd->nLine;` |
|        - | 10966 | `	/* Swap token stream */` |
|       77 | 10967 | `	pTmp = pGen->pEnd;` |
|       77 | 10968 | `	pGen->pEnd = pEnd;` |
|        - | 10969 | `	/* Mark as trait (PH7_NewRawClass may have set INTERNAL) */` |
|       77 | 10970 | `	pClass->iFlags \|= PH7_CLASS_TRAIT;` |
|        - | 10971 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|       71 | 10972 | `	for(;;){` |
|      191 | 10973 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       28 | 10974 | `			pGen->pIn++;` |
|        4 | 10975 | `		}` |
|      167 | 10976 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       77 | 10977 | `			break;` |
|        - | 10978 | `		}` |
|        - | 10979 | `		/* Bind a directly-preceding docblock to this member */` |
|       95 | 10980 | `		GenStateSetPendingDoc(&(*pGen));` |
|       95 | 10981 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|      ! 0 | 10982 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 10983 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|      ! 0 | 10984 | `				&pGen->pIn->sData,pName);` |
|      ! 0 | 10985 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 10986 | `				return SXERR_ABORT;` |
|        - | 10987 | `			}` |
|      ! 0 | 10988 | `			goto done;` |
|        - | 10989 | `		}` |
|       95 | 10990 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|       95 | 10991 | `		iAttrflags = 0;` |
|       95 | 10992 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       95 | 10993 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       95 | 10994 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|        - | 10995 | `				/* Trait uses another trait: use OtherTrait; */` |
|        5 | 10996 | `				pGen->pIn++; /* Jump 'use' */` |
|        2 | 10997 | `				for(;;){` |
|        - | 10998 | `					ph7_class *pUsedTrait;` |
|        - | 10999 | `					SyString *pUsedName;` |
|        5 | 11000 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|      ! 0 | 11001 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 11002 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|      ! 0 | 11003 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11004 | `							return SXERR_ABORT;` |
|        - | 11005 | `						}` |
|      ! 0 | 11006 | `						break;` |
|        - | 11007 | `					}` |
|        5 | 11008 | `					pUsedName = &pGen->pIn->sData;` |
|        - | 11009 | `					{` |
|        - | 11010 | `						SyBlob sResolved;` |
|        5 | 11011 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|        5 | 11012 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|        7 | 11013 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|        4 | 11014 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|        5 | 11015 | `						SyBlobRelease(&sResolved);` |
|        - | 11016 | `					}` |
|        5 | 11017 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|      ! 0 | 11018 | `						pUsedTrait = pUsedTrait->pNextName;` |
|      ! 0 | 11019 | `					}` |
|        5 | 11020 | `					if( pUsedTrait == 0 ){` |
|        4 | 11021 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        1 | 11022 | `							"'%z' is not a trait",pUsedName);` |
|        3 | 11023 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11024 | `							return SXERR_ABORT;` |
|        - | 11025 | `						}` |
|        2 | 11026 | `					}else{` |
|        3 | 11027 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|        - | 11028 | `					}` |
|        5 | 11029 | `					pGen->pIn++;` |
|        5 | 11030 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|        3 | 11031 | `						break;` |
|        - | 11032 | `					}` |
|      ! 0 | 11033 | `					pGen->pIn++;` |
|      ! 0 | 11034 | `				}` |
|        5 | 11035 | `				continue;` |
|        - | 11036 | `			}` |
|       91 | 11037 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       77 | 11038 | `				iProtection = nKwrd;` |
|       77 | 11039 | `				pGen->pIn++;` |
|       72 | 11040 | `				if( pGen->pIn >= pGen->pEnd` |
|       77 | 11041 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 11042 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11043 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|      ! 0 | 11044 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 11045 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 11046 | `						return SXERR_ABORT;` |
|        - | 11047 | `					}` |
|      ! 0 | 11048 | `					goto done;` |
|        - | 11049 | `				}` |
|       77 | 11050 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       12 | 11051 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       12 | 11052 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 11053 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11054 | `							return SXERR_ABORT;` |
|        - | 11055 | `						}` |
|      ! 0 | 11056 | `						goto done;` |
|        - | 11057 | `					}` |
|       12 | 11058 | `					continue;` |
|        - | 11059 | `				}` |
|       67 | 11060 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|        5 | 11061 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        5 | 11062 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 11063 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11064 | `							return SXERR_ABORT;` |
|        - | 11065 | `						}` |
|      ! 0 | 11066 | `						goto done;` |
|        - | 11067 | `					}` |
|        5 | 11068 | `					continue;` |
|        - | 11069 | `				}` |
|       63 | 11070 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       29 | 11071 | `			}` |
|       77 | 11072 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|      ! 0 | 11073 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11074 | `					"Traits cannot have constants");` |
|      ! 0 | 11075 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 11076 | `					return SXERR_ABORT;` |
|        - | 11077 | `				}` |
|      ! 0 | 11078 | `				goto done;` |
|      ! 0 | 11079 | `			}else{` |
|       77 | 11080 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|        8 | 11081 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|        8 | 11082 | `					pGen->pIn++;` |
|        8 | 11083 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        6 | 11084 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        6 | 11085 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      ! 0 | 11086 | `							iProtection = nKwrd;` |
|      ! 0 | 11087 | `							pGen->pIn++;` |
|      ! 0 | 11088 | `						}` |
|        2 | 11089 | `					}` |
|        6 | 11090 | `					if( pGen->pIn >= pGen->pEnd` |
|        8 | 11091 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|      ! 0 | 11092 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11093 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|      ! 0 | 11094 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 11095 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11096 | `							return SXERR_ABORT;` |
|        - | 11097 | `						}` |
|      ! 0 | 11098 | `						goto done;` |
|        - | 11099 | `					}` |
|        8 | 11100 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|        3 | 11101 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|        3 | 11102 | `						if( rc != SXRET_OK ){` |
|      ! 0 | 11103 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 11104 | `								return SXERR_ABORT;` |
|        - | 11105 | `							}` |
|      ! 0 | 11106 | `							goto done;` |
|        - | 11107 | `						}` |
|        3 | 11108 | `						continue;` |
|        - | 11109 | `					}` |
|        6 | 11110 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|      ! 0 | 11111 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 11112 | `						if( rc != SXRET_OK ){` |
|      ! 0 | 11113 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 | 11114 | `								return SXERR_ABORT;` |
|        - | 11115 | `							}` |
|      ! 0 | 11116 | `							goto done;` |
|        - | 11117 | `						}` |
|      ! 0 | 11118 | `						continue;` |
|        - | 11119 | `					}` |
|        6 | 11120 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       73 | 11121 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|        6 | 11122 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|        6 | 11123 | `					pGen->pIn++;` |
|        6 | 11124 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        6 | 11125 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        6 | 11126 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|        6 | 11127 | `							iProtection = nKwrd;` |
|        6 | 11128 | `							pGen->pIn++;` |
|        2 | 11129 | `						}` |
|        2 | 11130 | `					}` |
|        6 | 11131 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|        4 | 11132 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|      ! 0 | 11133 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11134 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|      ! 0 | 11135 | `							&pGen->pIn->sData,pName);` |
|      ! 0 | 11136 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11137 | `							return SXERR_ABORT;` |
|        - | 11138 | `						}` |
|      ! 0 | 11139 | `						goto done;` |
|        - | 11140 | `					}` |
|        6 | 11141 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|        2 | 11142 | `				}` |
|       75 | 11143 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|      ! 0 | 11144 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11145 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|      ! 0 | 11146 | `						&pGen->pIn->sData,pName);` |
|      ! 0 | 11147 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 11148 | `						return SXERR_ABORT;` |
|        - | 11149 | `					}` |
|      ! 0 | 11150 | `					goto done;` |
|        - | 11151 | `				}` |
|       75 | 11152 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|      ! 0 | 11153 | `					pGen->pIn++;` |
|      ! 0 | 11154 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 11155 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 11156 | `							"Expecting attribute declaration after 'var' keyword");` |
|      ! 0 | 11157 | `						if( rc == SXERR_ABORT ){` |
|      ! 0 | 11158 | `							return SXERR_ABORT;` |
|        - | 11159 | `						}` |
|      ! 0 | 11160 | `						goto done;` |
|        - | 11161 | `					}` |
|      ! 0 | 11162 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 11163 | `				}else{` |
|       75 | 11164 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|        - | 11165 | `				}` |
|       75 | 11166 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 11167 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 11168 | `						return SXERR_ABORT;` |
|        - | 11169 | `					}` |
|      ! 0 | 11170 | `					goto done;` |
|        - | 11171 | `				}` |
|        - | 11172 | `			}` |
|       40 | 11173 | `		}else{` |
|      ! 0 | 11174 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      ! 0 | 11175 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 11176 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 11177 | `					return SXERR_ABORT;` |
|        - | 11178 | `				}` |
|      ! 0 | 11179 | `				goto done;` |
|        - | 11180 | `			}` |
|        - | 11181 | `		}` |
|        5 | 11182 | `	}` |
|        - | 11183 | `	/* Install the trait */` |
|       77 | 11184 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|       77 | 11185 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11186 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 11187 | `		return SXERR_ABORT;` |
|        - | 11188 | `	}` |
|       36 | 11189 | `done:` |
|        - | 11190 | `	/* Point beyond the trait body */` |
|       77 | 11191 | `	pGen->pIn = &pEnd[1];` |
|       77 | 11192 | `	pGen->pEnd = pTmp;` |
|       77 | 11193 | `	return PH7_OK;` |
|       41 | 11194 | `}` |
|        - | 11195 | `/*` |
|        - | 11196 | ` * Compile a user-defined class.` |
|        - | 11197 | ` *  According to the PHP language reference manual` |
|        - | 11198 | ` *   Basic class definitions begin with the keyword class, followed` |
|        - | 11199 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|        - | 11200 | ` *   the definitions of the properties and methods belonging to the class.` |
|        - | 11201 | ` *   A class may contain its own constants, variables (called "properties")` |
|        - | 11202 | ` *   and functions (called "methods").` |
|        - | 11203 | ` */` |
|   187948 | 11204 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|        5 | 11205 | `{` |
|        - | 11206 | `	sxi32 rc;` |
|   187953 | 11207 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   187953 | 11208 | `	return rc;` |
|        5 | 11209 | `}` |
|        - | 11210 | `/*` |
|        - | 11211 | ` * Return TRUE if the token stream starts an enum declaration (PHP 8.1):` |
|        - | 11212 | `` * the context-sensitive identifier `enum` (not a reserved word — it stays`` |
|        - | 11213 | `` * valid as a function/constant name, like `readonly`) directly followed by`` |
|        - | 11214 | `` * an identifier. `enum(...)`/`enum;`/`$enum` all keep their expression`` |
|        - | 11215 | `` * meaning; `enum Name` can never start a valid expression.`` |
|        - | 11216 | ` */` |
|  6208128 | 11217 | `static int GenStateStartsEnumDecl(SyToken *pIn,SyToken *pEnd)` |
|        5 | 11218 | `{` |
|  6241403 | 11219 | `	return (pIn->nType & PH7_TK_ID)` |
|  3137334 | 11220 | `		&& pIn->sData.nByte == sizeof("enum")-1` |
|    37277 | 11221 | `		&& SyStrnicmp(pIn->sData.zString,"enum",sizeof("enum")-1) == 0` |
|  6241398 | 11222 | `		&& &pIn[1] < pEnd && (pIn[1].nType & PH7_TK_ID);` |
|        5 | 11223 | `}` |
|        - | 11224 | `/*` |
|        - | 11225 | ` * Compile an enum declaration (PHP 8.1). An enum is a final class carrying` |
|        - | 11226 | `` * PH7_CLASS_ENUM: `case` members become lazily-materialized singleton`` |
|        - | 11227 | ` * constants, cases()/from()/tryFrom() are synthesized, and UnitEnum/BackedEnum` |
|        - | 11228 | ` * are implemented implicitly (GenStateCompileClassEx handles the specifics).` |
|        - | 11229 | ` */` |
|       28 | 11230 | `static sxi32 PH7_CompileEnum(ph7_gen_state *pGen)` |
|        5 | 11231 | `{` |
|       33 | 11232 | `	return GenStateCompileClass(&(*pGen),PH7_CLASS_ENUM\|PH7_CLASS_FINAL);` |
|        5 | 11233 | `}` |
|        - | 11234 | `/*` |
|        - | 11235 | ` * Exception handling.` |
|        - | 11236 | ` *  According to the PHP language reference manual` |
|        - | 11237 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|        - | 11238 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|        - | 11239 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|        - | 11240 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|        - | 11241 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|        - | 11242 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|        - | 11243 | ` *    (or re-thrown) within a catch block.` |
|        - | 11244 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|        - | 11245 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|        - | 11246 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|        - | 11247 | ` *    been defined with set_exception_handler().` |
|        - | 11248 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|        - | 11249 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|        - | 11250 | ` */` |
|        - | 11251 | `/*` |
|        - | 11252 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|        - | 11253 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|        - | 11254 | ` * indicates failure.` |
|        - | 11255 | ` */` |
|   315008 | 11256 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 11257 | `{` |
|   315013 | 11258 | `	sxi32 rc = SXRET_OK;` |
|   315013 | 11259 | `	if( pRoot->pOp ){` |
|   315001 | 11260 | `		switch( pRoot->pOp->iOp ){` |
|   157498 | 11261 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|        - | 11262 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|        - | 11263 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|        - | 11264 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|        - | 11265 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|        - | 11266 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|   315001 | 11267 | `			break;` |
|      ! 0 | 11268 | `		default:` |
|        - | 11269 | `			/* Runtime will still reject non-Throwable values; the set above` |
|        - | 11270 | `			 * covers the common shapes and gives a friendlier compile error` |
|        - | 11271 | ``			 * for obvious mistakes like `throw 5`. */`` |
|      ! 0 | 11272 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 11273 | `				"throw: Expecting an exception class instance");` |
|      ! 0 | 11274 | `			if( rc != SXERR_ABORT ){` |
|      ! 0 | 11275 | `				rc = SXERR_INVALID;` |
|      ! 0 | 11276 | `			}` |
|      ! 0 | 11277 | `			break;` |
|        - | 11278 | `		}` |
|   157515 | 11279 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 11280 | `		/* Unexpected expression */` |
|      ! 0 | 11281 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 11282 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 11283 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 | 11284 | `			rc = SXERR_INVALID;` |
|      ! 0 | 11285 | `		}` |
|      ! 0 | 11286 | `	}` |
|   315013 | 11287 | `	return rc;` |
|        5 | 11288 | `}` |
|        - | 11289 | `/*` |
|        - | 11290 | ` * Compile a 'throw' statement.` |
|        - | 11291 | ` * throw: This is how you trigger an exception.` |
|        - | 11292 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|        - | 11293 | ` */` |
|   314972 | 11294 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|        5 | 11295 | `{` |
|   314977 | 11296 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11297 | `	GenBlock *pBlock;` |
|        - | 11298 | `	sxu32 nIdx;` |
|        - | 11299 | `	sxi32 rc;` |
|   314977 | 11300 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|        - | 11301 | `	/* Compile the expression */` |
|   314977 | 11302 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   314977 | 11303 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 11304 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|      ! 0 | 11305 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11306 | `			return SXERR_ABORT;` |
|        - | 11307 | `		}` |
|      ! 0 | 11308 | `		return SXRET_OK;` |
|        - | 11309 | `	}` |
|   314977 | 11310 | `	pBlock = pGen->pCurrent;` |
|        - | 11311 | `	/* Point to the top most function or try block and emit the forward jump */` |
|  1228101 | 11312 | `	while(pBlock->pParent){` |
|  1228097 | 11313 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   314973 | 11314 | `			break;` |
|        - | 11315 | `		}` |
|        - | 11316 | `		/* Point to the parent block */` |
|   913129 | 11317 | `		pBlock = pBlock->pParent;` |
|        5 | 11318 | `	}` |
|        - | 11319 | `	/* Emit the throw instruction */` |
|   314977 | 11320 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        - | 11321 | `	/* Emit the jump */` |
|   314977 | 11322 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   314977 | 11323 | `	return SXRET_OK;` |
|   157491 | 11324 | `}` |
|        - | 11325 | `/*` |
|        - | 11326 | ` * Compile a PHP 8.0 'throw' expression.` |
|        - | 11327 | ` * Called from the expression code generator when a 'throw' keyword is` |
|        - | 11328 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|        - | 11329 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|        - | 11330 | ` * the validator guarantees the operand is a valid exception target.` |
|        - | 11331 | ` */` |
|       36 | 11332 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        2 | 11333 | `{` |
|       38 | 11334 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11335 | `	GenBlock *pBlock;` |
|        - | 11336 | `	sxu32 nIdx;` |
|        - | 11337 | `	sxi32 rc;` |
|       18 | 11338 | `	(void)iCompileFlag;` |
|       38 | 11339 | `	pGen->pIn++; /* Skip 'throw' */` |
|       38 | 11340 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 11341 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 11342 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 11343 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11344 | `			return SXERR_ABORT;` |
|        - | 11345 | `		}` |
|      ! 0 | 11346 | `		return SXRET_OK;` |
|        - | 11347 | `	}` |
|       38 | 11348 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|       38 | 11349 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 11350 | `		return SXERR_ABORT;` |
|        - | 11351 | `	}` |
|       38 | 11352 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 11353 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 11354 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 11355 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11356 | `			return SXERR_ABORT;` |
|        - | 11357 | `		}` |
|      ! 0 | 11358 | `		return SXRET_OK;` |
|        - | 11359 | `	}` |
|        - | 11360 | `	/* Walk up to nearest exception/function block for the jump target */` |
|       38 | 11361 | `	pBlock = pGen->pCurrent;` |
|       60 | 11362 | `	while( pBlock->pParent ){` |
|       49 | 11363 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|       27 | 11364 | `			break;` |
|        - | 11365 | `		}` |
|       23 | 11366 | `		pBlock = pBlock->pParent;` |
|        1 | 11367 | `	}` |
|       38 | 11368 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       38 | 11369 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|       38 | 11370 | `	return SXRET_OK;` |
|       20 | 11371 | `}` |
|        - | 11372 | `/*` |
|        - | 11373 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|        - | 11374 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|        - | 11375 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|        - | 11376 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|        - | 11377 | ` * compile error propagated from the parser.` |
|        - | 11378 | ` */` |
|       54 | 11379 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|        5 | 11380 | `{` |
|        - | 11381 | `	SyString sClassName;` |
|        - | 11382 | `	SyToken *pToken;` |
|        - | 11383 | `	SyString *pName;` |
|        - | 11384 | `	char *zDup;` |
|        - | 11385 | `	sxi32 rc;` |
|       59 | 11386 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       59 | 11387 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|       59 | 11388 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|       59 | 11389 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       59 | 11390 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 | 11391 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 11392 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11393 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11394 | `		return SXERR_INVALID;` |
|        - | 11395 | `	}` |
|       59 | 11396 | `	pGen->pIn++; /* '(' */` |
|       27 | 11397 | `	for(;;){` |
|        - | 11398 | `		SyBlob sResolved;` |
|       59 | 11399 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       59 | 11400 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 11401 | `			SyBlobRelease(&sResolved);` |
|      ! 0 | 11402 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 11403 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11404 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11405 | `			return SXERR_INVALID;` |
|        - | 11406 | `		}` |
|       86 | 11407 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       54 | 11408 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       59 | 11409 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|       59 | 11410 | `		SyBlobRelease(&sResolved);` |
|       59 | 11411 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|       59 | 11412 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|       59 | 11413 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       54 | 11414 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|        5 | 11415 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      ! 0 | 11416 | `			pGen->pIn++; continue;` |
|        - | 11417 | `		}` |
|       59 | 11418 | `		break;` |
|      ! 0 | 11419 | `	}` |
|       54 | 11420 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|       59 | 11421 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 11422 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 11423 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11424 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11425 | `		return SXERR_INVALID;` |
|        - | 11426 | `	}` |
|       59 | 11427 | `	pGen->pIn++; /* '$' */` |
|       59 | 11428 | `	pName = &pGen->pIn->sData;` |
|       59 | 11429 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       59 | 11430 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|       59 | 11431 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|       59 | 11432 | `	pGen->pIn++;` |
|       59 | 11433 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 11434 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 11435 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11436 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11437 | `		return SXERR_INVALID;` |
|        - | 11438 | `	}` |
|       59 | 11439 | `	pGen->pIn++; /* ')' */` |
|       59 | 11440 | `	return SXRET_OK;` |
|       32 | 11441 | `}` |
|        - | 11442 | `/*` |
|        - | 11443 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|        - | 11444 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|        - | 11445 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|        - | 11446 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|        - | 11447 | ` * VmThrowException):` |
|        - | 11448 | ` *` |
|        - | 11449 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|        - | 11450 | ` *    <try body>` |
|        - | 11451 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|        - | 11452 | ` *    JMP  -> finally\|end` |
|        - | 11453 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|        - | 11454 | ` *    <catch body>` |
|        - | 11455 | ` *    JMP  -> finally\|end` |
|        - | 11456 | ` *    ... more catches ...` |
|        - | 11457 | ` *  Lfin: <finally body>` |
|        - | 11458 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|        - | 11459 | ` *  Lend:` |
|        - | 11460 | ` */` |
|       98 | 11461 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|        5 | 11462 | `{` |
|      103 | 11463 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11464 | `	GenBlock *pTry;` |
|        - | 11465 | `	VmInstr *pInstr;` |
|      103 | 11466 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|        - | 11467 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|        - | 11468 | `	sxi32 rc;` |
|      103 | 11469 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|        - | 11470 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|      103 | 11471 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      103 | 11472 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      103 | 11473 | `	pTry->pUserData = pException;` |
|      103 | 11474 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|      103 | 11475 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|      103 | 11476 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      103 | 11477 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      103 | 11478 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|      103 | 11479 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|        - | 11480 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|      103 | 11481 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|      103 | 11482 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|      103 | 11483 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      103 | 11484 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 11485 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|      103 | 11486 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|        - | 11487 | `	/* Catch clauses (inline) */` |
|      103 | 11488 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       98 | 11489 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       59 | 11490 | `		sxu32 k = 0;` |
|       81 | 11491 | `		for(;;){` |
|        - | 11492 | `			ph7_exception_block sCatch;` |
|        - | 11493 | `			GenBlock *pCatchBlk;` |
|      113 | 11494 | `			sxu32 idxJmp = 0;` |
|      108 | 11495 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      104 | 11496 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|       32 | 11497 | `				break;` |
|        - | 11498 | `			}` |
|       59 | 11499 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|       59 | 11500 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       59 | 11501 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|       59 | 11502 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|       59 | 11503 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|       59 | 11504 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|       59 | 11505 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|        - | 11506 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|        - | 11507 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|        - | 11508 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|       59 | 11509 | `			pCatchBlk->pUserData = pException;` |
|       59 | 11510 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|       59 | 11511 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       59 | 11512 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       59 | 11513 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        - | 11514 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|        - | 11515 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|       59 | 11516 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       59 | 11517 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|       59 | 11518 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|       59 | 11519 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|       59 | 11520 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       59 | 11521 | `			k++;` |
|        5 | 11522 | `		}` |
|       27 | 11523 | `	}` |
|        - | 11524 | `	/* Finally (inline) */` |
|      103 | 11525 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       80 | 11526 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 11527 | `		GenBlock *pFinBlk;` |
|       52 | 11528 | `		pGen->pIn++; /* Jump 'finally' */` |
|       52 | 11529 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|       52 | 11530 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|       52 | 11531 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       52 | 11532 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       52 | 11533 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       52 | 11534 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       52 | 11535 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       52 | 11536 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|       52 | 11537 | `		pException->iHasFinally = 1;` |
|       24 | 11538 | `	}` |
|      103 | 11539 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|      103 | 11540 | `	pException->iInlined = 1;` |
|        - | 11541 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|        - | 11542 | `	{` |
|      103 | 11543 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|        - | 11544 | `		sxu32 *aJ; sxu32 n;` |
|      103 | 11545 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|      103 | 11546 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      103 | 11547 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|      157 | 11548 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|       59 | 11549 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|       59 | 11550 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       32 | 11551 | `		}` |
|        - | 11552 | `	}` |
|      103 | 11553 | `	SySetRelease(&aCatchJmp);` |
|      103 | 11554 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|      ! 0 | 11555 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|      ! 0 | 11556 | `	}` |
|      103 | 11557 | `	return SXRET_OK;` |
|       54 | 11558 | `}` |
|        - | 11559 | `/*` |
|        - | 11560 | ` * Compile a 'catch' block.` |
|        - | 11561 | ` * Catch: A "catch" block retrieves an exception and creates` |
|        - | 11562 | ` * an object containing the exception information.` |
|        - | 11563 | ` */` |
|     5216 | 11564 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|        5 | 11565 | `{` |
|     5221 | 11566 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11567 | `	ph7_exception_block sCatch;` |
|        - | 11568 | `	SySet *pInstrContainer;` |
|        - | 11569 | `	SyString sClassName;` |
|        - | 11570 | `	GenBlock *pCatch;` |
|        - | 11571 | `	SyToken *pToken;` |
|        - | 11572 | `	SyString *pName;` |
|        - | 11573 | `	char *zDup;` |
|        - | 11574 | `	sxi32 rc;` |
|     5221 | 11575 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        - | 11576 | `	/* Zero the structure */` |
|     5221 | 11577 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|        - | 11578 | `	/* Initialize fields */` |
|     5221 | 11579 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     5221 | 11580 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     5221 | 11581 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|        - | 11582 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 11583 | `			pToken = pGen->pIn;` |
|      ! 0 | 11584 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 11585 | `				pToken--;` |
|      ! 0 | 11586 | `			}` |
|      ! 0 | 11587 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 11588 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11589 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11590 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11591 | `				return SXERR_ABORT;` |
|        - | 11592 | `			}` |
|      ! 0 | 11593 | `			return SXERR_INVALID;` |
|        - | 11594 | `	}` |
|        - | 11595 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     5221 | 11596 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     2622 | 11597 | `	for(;;){` |
|        - | 11598 | `		SyBlob sResolved;` |
|     5249 | 11599 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     5249 | 11600 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        6 | 11601 | `			SyBlobRelease(&sResolved);` |
|        6 | 11602 | `			pToken = pGen->pIn;` |
|        6 | 11603 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 11604 | `				pToken--;` |
|      ! 0 | 11605 | `			}` |
|        8 | 11606 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 11607 | `				"syntax error, unexpected %s \"%z\"",` |
|        2 | 11608 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|        6 | 11609 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11610 | `				return SXERR_ABORT;` |
|        - | 11611 | `			}` |
|        6 | 11612 | `			return SXERR_INVALID;` |
|        - | 11613 | `		}` |
|        - | 11614 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|        - | 11615 | `		 * transient SyBlob allocation. */` |
|     7865 | 11616 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     5240 | 11617 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     5245 | 11618 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     5245 | 11619 | `		SyBlobRelease(&sResolved);` |
|     5245 | 11620 | `		if( zDup == 0 ){` |
|      ! 0 | 11621 | `			goto Mem;` |
|        - | 11622 | `		}` |
|     5245 | 11623 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     5245 | 11624 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 11625 | `			goto Mem;` |
|        - | 11626 | `		}` |
|        - | 11627 | `		/* Check for '\|' (multi-catch separator) */` |
|     5240 | 11628 | `		if( pGen->pIn < pGen->pEnd &&` |
|     5240 | 11629 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|       33 | 11630 | `			pGen->pIn->sData.nByte == 1 &&` |
|       28 | 11631 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|       30 | 11632 | `			pGen->pIn++; /* Consume the '\|' */` |
|       30 | 11633 | `			continue;` |
|        - | 11634 | `		}` |
|     5217 | 11635 | `		break;` |
|      ! 0 | 11636 | `	}` |
|     5212 | 11637 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     5217 | 11638 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 11639 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 11640 | `			pToken = pGen->pIn;` |
|      ! 0 | 11641 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 11642 | `				pToken--;` |
|      ! 0 | 11643 | `			}` |
|      ! 0 | 11644 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 11645 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11646 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11647 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11648 | `				return SXERR_ABORT;` |
|        - | 11649 | `			}` |
|      ! 0 | 11650 | `			return SXERR_INVALID;` |
|        - | 11651 | `	}` |
|     5217 | 11652 | `	pGen->pIn++; /* Jump the dollar sign */` |
|        - | 11653 | `	/* Duplicate instance name */` |
|     5217 | 11654 | `	pName = &pGen->pIn->sData;` |
|     5217 | 11655 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     5217 | 11656 | `	if( zDup == 0 ){` |
|      ! 0 | 11657 | `		goto Mem;` |
|        - | 11658 | `	}` |
|     5217 | 11659 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     5217 | 11660 | `	pGen->pIn++;` |
|     5217 | 11661 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|        - | 11662 | `		/* Unexpected token,break immediately */` |
|      ! 0 | 11663 | `		pToken = pGen->pIn;` |
|      ! 0 | 11664 | `		if( pToken >= pGen->pEnd ){` |
|      ! 0 | 11665 | `			pToken--;` |
|      ! 0 | 11666 | `		}` |
|      ! 0 | 11667 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 11668 | `			"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 11669 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 11670 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11671 | `			return SXERR_ABORT;` |
|        - | 11672 | `		}` |
|      ! 0 | 11673 | `		return SXERR_INVALID;` |
|        - | 11674 | `	}` |
|        - | 11675 | `	/* Compile the block */` |
|     5217 | 11676 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|        - | 11677 | `	/* Create the catch block */` |
|     5217 | 11678 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     5217 | 11679 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11680 | `		return SXERR_ABORT;` |
|        - | 11681 | `	}` |
|        - | 11682 | `	/* Swap bytecode container */` |
|     5217 | 11683 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     5217 | 11684 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|        - | 11685 | `	/* Compile the block */` |
|     5217 | 11686 | `	PH7_CompileBlock(&(*pGen),0);` |
|        - | 11687 | `	/* Fix forward jumps now the destination is resolved  */` |
|     5217 | 11688 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 11689 | `	/* Emit the DONE instruction */` |
|     5217 | 11690 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 11691 | `	/* Leave the block */` |
|     5217 | 11692 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 11693 | `	/* Restore the default container */` |
|     5217 | 11694 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 11695 | `	/* Install the catch block */` |
|     5217 | 11696 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     5217 | 11697 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11698 | `		goto Mem;` |
|        - | 11699 | `	}` |
|     5217 | 11700 | `	return SXRET_OK;` |
|      ! 0 | 11701 | `Mem:` |
|      ! 0 | 11702 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 11703 | `	return SXERR_ABORT;` |
|     2613 | 11704 | `}` |
|        - | 11705 | `/*` |
|        - | 11706 | ` * Compile a 'try' block.` |
|        - | 11707 | ` * A function using an exception should be in a "try" block.` |
|        - | 11708 | ` * If the exception does not trigger, the code will continue` |
|        - | 11709 | ` * as normal. However if the exception triggers, an exception` |
|        - | 11710 | ` * is "thrown".` |
|        - | 11711 | ` */` |
|     5372 | 11712 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|        5 | 11713 | `{` |
|        - | 11714 | `	ph7_exception *pException;` |
|     5377 | 11715 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 11716 | `	GenBlock *pTry;` |
|        - | 11717 | `	sxu32 nJmpIdx;` |
|        - | 11718 | `	sxi32 rc;` |
|        - | 11719 | `	/* Create the exception container */` |
|     5377 | 11720 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     5377 | 11721 | `	if( pException == 0 ){` |
|      ! 0 | 11722 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 11723 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 11724 | `		return SXERR_ABORT;` |
|        - | 11725 | `	}` |
|        - | 11726 | `	/* Zero the structure */` |
|     5377 | 11727 | `	SyZero(pException,sizeof(ph7_exception));` |
|        - | 11728 | `	/* Initialize fields */` |
|     5377 | 11729 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     5377 | 11730 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     5377 | 11731 | `	pException->iHasFinally = 0;` |
|     5377 | 11732 | `	pException->iFinallyDone = 0;` |
|     5377 | 11733 | `	pException->pVm = pGen->pVm;` |
|        - | 11734 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|        - | 11735 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|        - | 11736 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|        - | 11737 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|        - | 11738 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|        - | 11739 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|     5377 | 11740 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|      103 | 11741 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|        - | 11742 | `	}` |
|        - | 11743 | `	/* Create the try block */` |
|     5279 | 11744 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     5279 | 11745 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11746 | `		return SXERR_ABORT;` |
|        - | 11747 | `	}` |
|        - | 11748 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     5279 | 11749 | `	pTry->pUserData = pException;` |
|        - | 11750 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     5279 | 11751 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|        - | 11752 | `	/* Fix the jump later when the destination is resolved */` |
|     5279 | 11753 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     5279 | 11754 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|        - | 11755 | `	/* Compile the block */` |
|     5279 | 11756 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     5279 | 11757 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 11758 | `		return SXERR_ABORT;` |
|        - | 11759 | `	}` |
|        - | 11760 | `	/* Fix forward jumps now the destination is resolved */` |
|     5279 | 11761 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 11762 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     5279 | 11763 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        - | 11764 | `	/* Leave the block */` |
|     5279 | 11765 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 11766 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     5279 | 11767 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     5272 | 11768 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        - | 11769 | `		/* Compile one or more catch blocks */` |
|     5212 | 11770 | `		for(;;){` |
|    10424 | 11771 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     7861 | 11772 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     2609 | 11773 | `					break;` |
|        - | 11774 | `			}` |
|     5221 | 11775 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     5221 | 11776 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11777 | `				return SXERR_ABORT;` |
|        - | 11778 | `			}` |
|        5 | 11779 | `		}` |
|     2604 | 11780 | `	}` |
|        - | 11781 | `	/* Compile optional finally block */` |
|     5279 | 11782 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      658 | 11783 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 11784 | `		SySet *pInstrContainer;` |
|        - | 11785 | `		GenBlock *pFinBlock;` |
|      129 | 11786 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|        - | 11787 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      129 | 11788 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      129 | 11789 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 11790 | `			return SXERR_ABORT;` |
|        - | 11791 | `		}` |
|        - | 11792 | `		/* Swap bytecode container */` |
|      129 | 11793 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      129 | 11794 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|        - | 11795 | `		/* Compile the finally body */` |
|      129 | 11796 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      129 | 11797 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11798 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 | 11799 | `			return SXERR_ABORT;` |
|        - | 11800 | `		}` |
|        - | 11801 | `		/* Fix forward jumps now the destination is resolved */` |
|      129 | 11802 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 11803 | `		/* Emit DONE to terminate the finally block */` |
|      129 | 11804 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 11805 | `		/* Leave the block */` |
|      129 | 11806 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        - | 11807 | `		/* Restore the default container */` |
|      129 | 11808 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      129 | 11809 | `		pException->iHasFinally = 1;` |
|       62 | 11810 | `	}` |
|        - | 11811 | `	/* Must have at least one catch or finally */` |
|     5279 | 11812 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|        8 | 11813 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 11814 | `			"Cannot use try without catch or finally");` |
|        8 | 11815 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11816 | `			return SXERR_ABORT;` |
|        - | 11817 | `		}` |
|        3 | 11818 | `	}` |
|     5279 | 11819 | `	return SXRET_OK;` |
|     2691 | 11820 | `}` |
|        - | 11821 | `/*` |
|        - | 11822 | ` * Compile a switch block.` |
|        - | 11823 | ` *  (See block-comment below for more information)` |
|        - | 11824 | ` */` |
|      112 | 11825 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|        5 | 11826 | `{` |
|      117 | 11827 | `	sxi32 rc = SXRET_OK;` |
|      117 | 11828 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|        - | 11829 | `		/* Unexpected token */` |
|      ! 0 | 11830 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 11831 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11832 | `			return SXERR_ABORT;` |
|        - | 11833 | `		}` |
|      ! 0 | 11834 | `		pGen->pIn++;` |
|      ! 0 | 11835 | `	}` |
|      117 | 11836 | `	pGen->pIn++;` |
|        - | 11837 | `	/* First instruction to execute in this block. */` |
|      117 | 11838 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|        - | 11839 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|        - | 11840 | `	 * or the '}' token */` |
|      206 | 11841 | `	for(;;){` |
|      417 | 11842 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 11843 | `			/* No more input to process */` |
|      ! 0 | 11844 | `			break;` |
|        - | 11845 | `		}` |
|      417 | 11846 | `		rc = SXRET_OK;` |
|      417 | 11847 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|       85 | 11848 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|       31 | 11849 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|        - | 11850 | `					/* Unexpected token */` |
|      ! 0 | 11851 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 11852 | `						&pGen->pIn->sData);` |
|      ! 0 | 11853 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 11854 | `						return SXERR_ABORT;` |
|        - | 11855 | `					}` |
|        - | 11856 | `					/* FALL THROUGH */` |
|      ! 0 | 11857 | `				}` |
|       31 | 11858 | `				rc = SXERR_EOF;` |
|       31 | 11859 | `				break;` |
|        - | 11860 | `			}` |
|       32 | 11861 | `		}else{` |
|        - | 11862 | `			sxi32 nKwrd;` |
|        - | 11863 | `			/* Extract the keyword */` |
|      337 | 11864 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      337 | 11865 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|       47 | 11866 | `				break;` |
|        - | 11867 | `			}` |
|      253 | 11868 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 11869 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|        - | 11870 | `					/* Unexpected token */` |
|      ! 0 | 11871 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 11872 | `						&pGen->pIn->sData);` |
|      ! 0 | 11873 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 11874 | `						return SXERR_ABORT;` |
|        - | 11875 | `					}` |
|        - | 11876 | `					/* FALL THROUGH */` |
|      ! 0 | 11877 | `				}` |
|        - | 11878 | `				/* Block compiled */` |
|        3 | 11879 | `				break;` |
|        - | 11880 | `			}` |
|        - | 11881 | `		}` |
|        - | 11882 | `		/* Compile block */` |
|      305 | 11883 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      305 | 11884 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11885 | `			return SXERR_ABORT;` |
|        - | 11886 | `		}` |
|        5 | 11887 | `	}` |
|      117 | 11888 | `	return rc;` |
|       61 | 11889 | `}` |
|        - | 11890 | `/*` |
|        - | 11891 | ` * Compile a case eXpression.` |
|        - | 11892 | ` *  (See block-comment below for more information)` |
|        - | 11893 | ` */` |
|       92 | 11894 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|        5 | 11895 | `{` |
|        - | 11896 | `	SySet *pInstrContainer;` |
|        - | 11897 | `	SyToken *pEnd,*pTmp;` |
|       97 | 11898 | `	sxi32 iNest = 0;` |
|        - | 11899 | `	sxi32 rc;` |
|        - | 11900 | `	/* Delimit the expression */` |
|       97 | 11901 | `	pEnd = pGen->pIn;` |
|      197 | 11902 | `	while( pEnd < pGen->pEnd ){` |
|      197 | 11903 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|        - | 11904 | `			/* Increment nesting level */` |
|        3 | 11905 | `			iNest++;` |
|      196 | 11906 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|        - | 11907 | `			/* Decrement nesting level */` |
|        3 | 11908 | `			iNest--;` |
|      194 | 11909 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|       97 | 11910 | `			break;` |
|        - | 11911 | `		}` |
|      105 | 11912 | `		pEnd++;` |
|        5 | 11913 | `	}` |
|       97 | 11914 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 11915 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|      ! 0 | 11916 | `		if( rc == SXERR_ABORT ){` |
|        - | 11917 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 11918 | `			return SXERR_ABORT;` |
|        - | 11919 | `		}` |
|      ! 0 | 11920 | `	}` |
|        - | 11921 | `	/* Swap token stream */` |
|       97 | 11922 | `	pTmp = pGen->pEnd;` |
|       97 | 11923 | `	pGen->pEnd = pEnd;` |
|       97 | 11924 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       97 | 11925 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|       97 | 11926 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 11927 | `	/* Emit the done instruction */` |
|       97 | 11928 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       97 | 11929 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 11930 | `	/* Update token stream */` |
|       97 | 11931 | `	pGen->pIn  = pEnd;` |
|       97 | 11932 | `	pGen->pEnd = pTmp;` |
|       97 | 11933 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 11934 | `		return SXERR_ABORT;` |
|        - | 11935 | `	}` |
|       97 | 11936 | `	return SXRET_OK;` |
|       51 | 11937 | `}` |
|        - | 11938 | `/*` |
|        - | 11939 | ` * Compile the smart switch statement.` |
|        - | 11940 | ` * According to the PHP language reference manual` |
|        - | 11941 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|        - | 11942 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|        - | 11943 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|        - | 11944 | ` *  This is exactly what the switch statement is for.` |
|        - | 11945 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|        - | 11946 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|        - | 11947 | ` *  of the outer loop, use continue 2.` |
|        - | 11948 | ` *  Note that switch/case does loose comparision.` |
|        - | 11949 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|        - | 11950 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|        - | 11951 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|        - | 11952 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|        - | 11953 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|        - | 11954 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|        - | 11955 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|        - | 11956 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|        - | 11957 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|        - | 11958 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|        - | 11959 | ` *  list for the next case.` |
|        - | 11960 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|        - | 11961 | ` *  or floating-point numbers and strings.` |
|        - | 11962 | ` */` |
|       28 | 11963 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|        5 | 11964 | `{` |
|        - | 11965 | `	GenBlock *pSwitchBlock;` |
|        - | 11966 | `	SyToken *pTmp,*pEnd;` |
|        - | 11967 | `	ph7_switch *pSwitch;` |
|        - | 11968 | `	sxu32 nToken;` |
|        - | 11969 | `	sxu32 nLine;` |
|        - | 11970 | `	sxi32 rc;` |
|       33 | 11971 | `	nLine = pGen->pIn->nLine;` |
|        - | 11972 | `	/* Jump the 'switch' keyword */` |
|       33 | 11973 | `	pGen->pIn++;` |
|       33 | 11974 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 11975 | `		/* Syntax error */` |
|      ! 0 | 11976 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|      ! 0 | 11977 | `		if( rc == SXERR_ABORT ){` |
|        - | 11978 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 11979 | `			return SXERR_ABORT;` |
|        - | 11980 | `		}` |
|      ! 0 | 11981 | `		goto Synchronize;` |
|        - | 11982 | `	}` |
|        - | 11983 | `	/* Jump the left parenthesis '(' */` |
|       33 | 11984 | `	pGen->pIn++;` |
|       33 | 11985 | `	pEnd = 0; /* cc warning */` |
|        - | 11986 | `	/* Create the loop block */` |
|       47 | 11987 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|       14 | 11988 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|       33 | 11989 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11990 | `		return SXERR_ABORT;` |
|        - | 11991 | `	}` |
|        - | 11992 | `	/* Delimit the condition */` |
|       33 | 11993 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|       33 | 11994 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 11995 | `		/* Empty expression */` |
|      ! 0 | 11996 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|      ! 0 | 11997 | `		if( rc == SXERR_ABORT ){` |
|        - | 11998 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 11999 | `			return SXERR_ABORT;` |
|        - | 12000 | `		}` |
|      ! 0 | 12001 | `	}` |
|        - | 12002 | `	/* Swap token streams */` |
|       33 | 12003 | `	pTmp = pGen->pEnd;` |
|       33 | 12004 | `	pGen->pEnd = pEnd;` |
|        - | 12005 | `	/* Compile the expression */` |
|       33 | 12006 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       33 | 12007 | `	if( rc == SXERR_ABORT ){` |
|        - | 12008 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 12009 | `		return SXERR_ABORT;` |
|        - | 12010 | `	}` |
|        - | 12011 | `	/* Update token stream */` |
|       33 | 12012 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 | 12013 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 12014 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 12015 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12016 | `			return SXERR_ABORT;` |
|        - | 12017 | `		}` |
|      ! 0 | 12018 | `		pGen->pIn++;` |
|      ! 0 | 12019 | `	}` |
|       33 | 12020 | `	pGen->pIn  = &pEnd[1];` |
|       33 | 12021 | `	pGen->pEnd = pTmp;` |
|       33 | 12022 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       28 | 12023 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|      ! 0 | 12024 | `			pTmp = pGen->pIn;` |
|      ! 0 | 12025 | `			if( pTmp >= pGen->pEnd ){` |
|      ! 0 | 12026 | `				pTmp--;` |
|      ! 0 | 12027 | `			}` |
|        - | 12028 | `			/* Unexpected token */` |
|      ! 0 | 12029 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|      ! 0 | 12030 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 12031 | `				return SXERR_ABORT;` |
|        - | 12032 | `			}` |
|      ! 0 | 12033 | `			goto Synchronize;` |
|        - | 12034 | `	}` |
|        - | 12035 | `	/* Set the delimiter token */` |
|       33 | 12036 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|        3 | 12037 | `		nToken = PH7_TK_KEYWORD;` |
|        - | 12038 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|        2 | 12039 | `	}else{` |
|       31 | 12040 | `		nToken = PH7_TK_CCB; /* '}' */` |
|        - | 12041 | `	}` |
|       33 | 12042 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|        - | 12043 | `	/* Create the switch blocks container */` |
|       33 | 12044 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|       33 | 12045 | `	if( pSwitch == 0 ){` |
|        - | 12046 | `		/* Abort compilation */` |
|      ! 0 | 12047 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 12048 | `		return SXERR_ABORT;` |
|        - | 12049 | `	}` |
|        - | 12050 | `	/* Zero the structure */` |
|       33 | 12051 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|        - | 12052 | `	/* Initialize fields */` |
|       33 | 12053 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|        - | 12054 | `	/* Emit the switch instruction */` |
|       33 | 12055 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|        - | 12056 | `	/* Compile case blocks */` |
|      100 | 12057 | `	for(;;){` |
|        - | 12058 | `		sxu32 nKwrd;` |
|      119 | 12059 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 12060 | `			/* No more input to process */` |
|      ! 0 | 12061 | `			break;` |
|        - | 12062 | `		}` |
|      119 | 12063 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 12064 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|        - | 12065 | `				/* Unexpected token */` |
|      ! 0 | 12066 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 12067 | `					&pGen->pIn->sData);` |
|      ! 0 | 12068 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 12069 | `					return SXERR_ABORT;` |
|        - | 12070 | `				}` |
|        - | 12071 | `				/* FALL THROUGH */` |
|      ! 0 | 12072 | `			}` |
|        - | 12073 | `			/* Block compiled */` |
|      ! 0 | 12074 | `			break;` |
|        - | 12075 | `		}` |
|        - | 12076 | `		/* Extract the keyword */` |
|      119 | 12077 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      119 | 12078 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        3 | 12079 | `			if( nToken != PH7_TK_KEYWORD ){` |
|        - | 12080 | `				/* Unexpected token */` |
|      ! 0 | 12081 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 12082 | `					&pGen->pIn->sData);` |
|      ! 0 | 12083 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 12084 | `					return SXERR_ABORT;` |
|        - | 12085 | `				}` |
|        - | 12086 | `				/* FALL THROUGH */` |
|      ! 0 | 12087 | `			}` |
|        - | 12088 | `			/* Block compiled */` |
|        3 | 12089 | `			break;` |
|        - | 12090 | `		}` |
|      117 | 12091 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|        - | 12092 | `			/*` |
|        - | 12093 | `			 * Accroding to the PHP language reference manual` |
|        - | 12094 | `			 *  A special case is the default case. This case matches anything` |
|        - | 12095 | `			 *  that wasn't matched by the other cases.` |
|        - | 12096 | `			 */` |
|       25 | 12097 | `			if( pSwitch->nDefault > 0 ){` |
|        - | 12098 | `				/* Default case already compiled */` |
|      ! 0 | 12099 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|      ! 0 | 12100 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 12101 | `					return SXERR_ABORT;` |
|        - | 12102 | `				}` |
|      ! 0 | 12103 | `			}` |
|       25 | 12104 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|        - | 12105 | `			/* Compile the default block */` |
|       25 | 12106 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|       25 | 12107 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 12108 | `				return SXERR_ABORT;` |
|       25 | 12109 | `			}else if( rc == SXERR_EOF ){` |
|       23 | 12110 | `				break;` |
|        1 | 12111 | `			}` |
|       98 | 12112 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|        - | 12113 | `			ph7_case_expr sCase;` |
|        - | 12114 | `			/* Standard case block */` |
|       97 | 12115 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|        - | 12116 | `			/* initialize the structure */` |
|       97 | 12117 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - | 12118 | `			/* Compile the case expression */` |
|       97 | 12119 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|       97 | 12120 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 12121 | `				return SXERR_ABORT;` |
|        - | 12122 | `			}` |
|        - | 12123 | `			/* Compile the case block */` |
|       97 | 12124 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|        - | 12125 | `			/* Insert in the switch container */` |
|       97 | 12126 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|       97 | 12127 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 12128 | `				return SXERR_ABORT;` |
|       97 | 12129 | `			}else if( rc == SXERR_EOF ){` |
|        9 | 12130 | `				break;` |
|        - | 12131 | `			}` |
|       47 | 12132 | `		}else{` |
|        - | 12133 | `			/* Unexpected token */` |
|      ! 0 | 12134 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 12135 | `				&pGen->pIn->sData);` |
|      ! 0 | 12136 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 12137 | `				return SXERR_ABORT;` |
|        - | 12138 | `			}` |
|      ! 0 | 12139 | `			break;` |
|        - | 12140 | `		}` |
|        5 | 12141 | `	}` |
|        - | 12142 | `	/* Fix all jumps now the destination is resolved */` |
|       33 | 12143 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|       33 | 12144 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 12145 | `	/* Release the loop block */` |
|       33 | 12146 | `	GenStateLeaveBlock(pGen,0);` |
|       33 | 12147 | `	if( pGen->pIn < pGen->pEnd ){` |
|        - | 12148 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|       33 | 12149 | `		pGen->pIn++;` |
|       14 | 12150 | `	}` |
|        - | 12151 | `	/* Statement successfully compiled */` |
|       33 | 12152 | `	return SXRET_OK;` |
|      ! 0 | 12153 | `Synchronize:` |
|        - | 12154 | `	/* Synchronize with the first semi-colon */` |
|      ! 0 | 12155 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      ! 0 | 12156 | `		pGen->pIn++;` |
|      ! 0 | 12157 | `	}` |
|      ! 0 | 12158 | `	return SXRET_OK;` |
|       19 | 12159 | `}` |
|        - | 12160 | `/*` |
|        - | 12161 | ` * Chain operators participate in a postfix member-access chain.` |
|        - | 12162 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|        - | 12163 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|        - | 12164 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|        - | 12165 | ` */` |
|        - | 12166 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|        - | 12167 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|        - | 12168 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|        - | 12169 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|        - | 12170 |  |
|        - | 12171 | `/*` |
|        - | 12172 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|        - | 12173 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|        - | 12174 | ` * patched entries from the pending set.` |
|        - | 12175 | ` */` |
| 22921494 | 12176 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|        5 | 12177 | `{` |
| 22921499 | 12178 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|        - | 12179 | `	sxu32 nTarget;` |
|        - | 12180 | `	sxu32 *aIdx;` |
|        - | 12181 | `	sxu32 i;` |
| 22921499 | 12182 | `	if( nCur <= nBaseline ){` |
| 22921403 | 12183 | `		return;` |
|        - | 12184 | `	}` |
|      100 | 12185 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      100 | 12186 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|      204 | 12187 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|      108 | 12188 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|      108 | 12189 | `		if( pInstr ){` |
|      108 | 12190 | `			pInstr->iP2 = (sxi32)nTarget;` |
|       52 | 12191 | `		}` |
|       56 | 12192 | `	}` |
|      100 | 12193 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 11460752 | 12194 | `}` |
|        - | 12195 |  |
|        - | 12196 | `/*` |
|        - | 12197 | ` * By-reference out-parameters of builtin functions.` |
|        - | 12198 | ` *` |
|        - | 12199 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|        - | 12200 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|        - | 12201 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|        - | 12202 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|        - | 12203 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|        - | 12204 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|        - | 12205 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|        - | 12206 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|        - | 12207 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|        - | 12208 | ` * creates it" behaviour).` |
|        - | 12209 | ` *` |
|        - | 12210 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|        - | 12211 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|        - | 12212 | ` */` |
|  3211844 | 12213 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|        5 | 12214 | `{` |
|        - | 12215 | `	static const struct {` |
|        - | 12216 | `		const char *zName;` |
|        - | 12217 | `		sxu32 nByte;` |
|        - | 12218 | `		sxu32 mask;` |
|        - | 12219 | `	} aByRef[] = {` |
|        - | 12220 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|        - | 12221 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|        - | 12222 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|        - | 12223 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|        - | 12224 | `		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */` |
|        - | 12225 | `	};` |
|        - | 12226 | `	sxu32 i;` |
|  3211849 | 12227 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|   846683 | 12228 | `		return 0;` |
|        - | 12229 | `	}` |
| 14190607 | 12230 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 11825558 | 12231 | `		if( pName->nByte == aByRef[i].nByte` |
|  6058192 | 12232 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|      127 | 12233 | `			return aByRef[i].mask;` |
|        - | 12234 | `		}` |
|  5912723 | 12235 | `	}` |
|  2365049 | 12236 | `	return 0;` |
|  1605927 | 12237 | `}` |
|        - | 12238 | `/*` |
|        - | 12239 | ` * Recover the bare global-builtin name from a call's callee node.` |
|        - | 12240 | ` *` |
|        - | 12241 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|        - | 12242 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|        - | 12243 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|        - | 12244 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|        - | 12245 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|        - | 12246 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|        - | 12247 | ` */` |
|  3211844 | 12248 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|        5 | 12249 | `{` |
|        - | 12250 | `	SyToken *p, *pEnd;` |
|  3211849 | 12251 | `	pOut->zString = 0;` |
|  3211849 | 12252 | `	pOut->nByte = 0;` |
|  3211849 | 12253 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|      ! 0 | 12254 | `		return;` |
|        - | 12255 | `	}` |
|  3211849 | 12256 | `	p = pLeft->pStart;` |
|  3211849 | 12257 | `	pEnd = pLeft->pEnd;` |
|        - | 12258 | `	/* Optional single leading namespace separator (absolute path). */` |
|  3211849 | 12259 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|     3917 | 12260 | `		p++;` |
|     1956 | 12261 | `	}` |
|  3211849 | 12262 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|   846647 | 12263 | `		return;` |
|        - | 12264 | `	}` |
|        - | 12265 | `	/* Must be a single component: nothing follows the name token. */` |
|  2365207 | 12266 | `	if( p + 1 != pEnd ){` |
|       40 | 12267 | `		return;` |
|        - | 12268 | `	}` |
|  2365171 | 12269 | `	*pOut = p->sData;` |
|  1605927 | 12270 | `}` |
|        - | 12271 | `/*` |
|        - | 12272 | ` * Generate bytecode for a given expression tree.` |
|        - | 12273 | ` * If something goes wrong while generating bytecode` |
|        - | 12274 | ` * for the expression tree (A very unlikely scenario)` |
|        - | 12275 | ` * this function takes care of generating the appropriate` |
|        - | 12276 | ` * error message.` |
|        - | 12277 | ` */` |
| 31783662 | 12278 | `static sxi32 GenStateEmitExprCode(` |
|        - | 12279 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - | 12280 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|        - | 12281 | `	sxi32 iFlags /* Control flags */` |
|        - | 12282 | `	)` |
|        5 | 12283 | `{` |
|        - | 12284 | `	VmInstr *pInstr;` |
|        - | 12285 | `	sxu32 nJmpIdx;` |
| 31783667 | 12286 | `	sxi32 iP1 = 0;` |
| 31783667 | 12287 | `	sxu32 iP2 = 0;` |
| 31783667 | 12288 | `	void *p3  = 0;` |
|        - | 12289 | `	sxi32 iVmOp;` |
|        - | 12290 | `	sxi32 rc;` |
| 31783667 | 12291 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 31783667 | 12292 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
| 31783667 | 12293 | `	sxu32 nRhsNsBase = 0;` |
| 31783667 | 12294 | `	if( pNode->xCode ){` |
|        - | 12295 | `		SyToken *pTmpIn,*pTmpEnd;` |
|        - | 12296 | `		/* Compile node */` |
| 19092183 | 12297 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 19092183 | 12298 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 19092183 | 12299 | `		RE_SWAP_DELIMITER(pGen);` |
| 19092183 | 12300 | `		return rc;` |
|        - | 12301 | `	}` |
| 12691489 | 12302 | `	if( pNode->pOp == 0 ){` |
|      ! 0 | 12303 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 12304 | `			"Invalid expression node,PH7 is aborting compilation");` |
|      ! 0 | 12305 | `		return SXERR_ABORT;` |
|        - | 12306 | `	}` |
| 12691489 | 12307 | `	iVmOp = pNode->pOp->iVmOp;` |
| 12691489 | 12308 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|        - | 12309 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|        - | 12310 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|        - | 12311 | `		 * and later errors are still reported. */` |
|        3 | 12312 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 12313 | `			"The (unset) cast is no longer supported");` |
|        3 | 12314 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12315 | `			return SXERR_ABORT;` |
|        - | 12316 | `		}` |
|        1 | 12317 | `	}` |
| 12691489 | 12318 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|       65 | 12319 | `		sxu32 nJmp = 0;` |
|        - | 12320 | `		sxu32 nNcNsBase;` |
|        - | 12321 | `		VmInstr *pInstrFix;` |
|        - | 12322 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|        - | 12323 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|        - | 12324 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|        - | 12325 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|        - | 12326 | `		 * stack slot carries a writable nIdx. */` |
|       65 | 12327 | `		if( pNode->pRight ){` |
|       65 | 12328 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       65 | 12329 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|       65 | 12330 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 12331 | `				return rc;` |
|        - | 12332 | `			}` |
|       65 | 12333 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        - | 12334 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|        - | 12335 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|        - | 12336 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|        - | 12337 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|        - | 12338 | `			 * the store, so the parent array does not need to be copied at` |
|        - | 12339 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|        - | 12340 | `			 * cascade for the actual write path stays correct. */` |
|       65 | 12341 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|       65 | 12342 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|       31 | 12343 | `				pInstrFix->iP2 = 3;` |
|       14 | 12344 | `			}` |
|       31 | 12345 | `		}` |
|        - | 12346 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|       65 | 12347 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|        - | 12348 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|       65 | 12349 | `		if( pNode->pLeft ){` |
|       65 | 12350 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       65 | 12351 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|       65 | 12352 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 12353 | `				return rc;` |
|        - | 12354 | `			}` |
|       65 | 12355 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       31 | 12356 | `		}` |
|        - | 12357 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|       65 | 12358 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|        - | 12359 | `		/* Patch the short-circuit jump to land after the store. */` |
|       65 | 12360 | `		if( nJmp > 0 ){` |
|       65 | 12361 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|       65 | 12362 | `			if( pInstrFix ){` |
|       65 | 12363 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|       31 | 12364 | `			}` |
|       31 | 12365 | `		}` |
|       65 | 12366 | `		return SXRET_OK;` |
|        - | 12367 | `	}` |
| 12691427 | 12368 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|        - | 12369 | `		sxu32 nJz,nJmp;` |
|        - | 12370 | `		sxu32 nTernaryNsBase;` |
|        - | 12371 | `		/* Ternary operator require special handling */` |
|        - | 12372 | `		/* Phase#1: Compile the condition */` |
|   212919 | 12373 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   212919 | 12374 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|   212919 | 12375 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12376 | `			return rc;` |
|        - | 12377 | `		}` |
|        - | 12378 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|        - | 12379 | `		 * compiling the condition must short-circuit to the end of the` |
|        - | 12380 | `		 * condition expression, not leak past the ternary. */` |
|   212919 | 12381 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|   212919 | 12382 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|   212919 | 12383 | `		if( pNode->pLeft ){` |
|        - | 12384 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|        - | 12385 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|   212851 | 12386 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|        - | 12387 | `			/* Phase#3: Compile the 'then' expression  */` |
|   212851 | 12388 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   212851 | 12389 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|   212851 | 12390 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 12391 | `				return rc;` |
|        - | 12392 | `			}` |
|   212851 | 12393 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|   106428 | 12394 | `		}else{` |
|        - | 12395 | `			/* Elvis operator: (expr) ?: (else)` |
|        - | 12396 | `			 * Duplicate condition so original value is the 'then' result.` |
|        - | 12397 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|       70 | 12398 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       70 | 12399 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|        - | 12400 | `		}` |
|        - | 12401 | `		/* Phase#4: Emit the unconditional jump */` |
|   212919 | 12402 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|        - | 12403 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|   212919 | 12404 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|   212919 | 12405 | `		if( pInstr ){` |
|   212919 | 12406 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|   106457 | 12407 | `		}` |
|   212919 | 12408 | `		if( !pNode->pLeft ){` |
|        - | 12409 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|       70 | 12410 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       34 | 12411 | `		}` |
|        - | 12412 | `		/* Phase#6: Compile the 'else' expression */` |
|   212919 | 12413 | `		if( pNode->pRight ){` |
|   212919 | 12414 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   212919 | 12415 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|   212919 | 12416 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 12417 | `				return rc;` |
|        - | 12418 | `			}` |
|   212919 | 12419 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|   106457 | 12420 | `		}` |
|   212919 | 12421 | `		if( nJmp > 0 ){` |
|        - | 12422 | `			/* Phase#7: Fix the unconditional jump */` |
|   212919 | 12423 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|   212919 | 12424 | `			if( pInstr ){` |
|   212919 | 12425 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|   106457 | 12426 | `			}` |
|   106457 | 12427 | `		}` |
|        - | 12428 | `		/* All done */` |
|   212919 | 12429 | `		return SXRET_OK;` |
|        - | 12430 | `	}` |
| 12478513 | 12431 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|        - | 12432 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|        - | 12433 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|        - | 12434 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|        - | 12435 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|        - | 12436 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|        - | 12437 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|        - | 12438 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|        - | 12439 | `		sxu32 nPipeNsBase;` |
|       27 | 12440 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|       27 | 12441 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|      ! 0 | 12442 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 12443 | `				"'\|>': Missing operand");` |
|      ! 0 | 12444 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 12445 | `		}` |
|        - | 12446 | `		/* Argument: the LHS value. */` |
|       27 | 12447 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       27 | 12448 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|       27 | 12449 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12450 | `			return rc;` |
|        - | 12451 | `		}` |
|       27 | 12452 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|        - | 12453 | `		/* Callable: the RHS. */` |
|       27 | 12454 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       27 | 12455 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|       27 | 12456 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12457 | `			return rc;` |
|        - | 12458 | `		}` |
|       27 | 12459 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|        - | 12460 | `		/* Invoke the callable with the single piped argument. */` |
|       27 | 12461 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       27 | 12462 | `		return SXRET_OK;` |
|        - | 12463 | `	}` |
| 12478487 | 12464 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|        - | 12465 | `	/* Generate code for the left tree */` |
| 12478487 | 12466 | `	if( pNode->pLeft ){` |
| 12466837 | 12467 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 12466837 | 12468 | `		if( iVmOp == PH7_OP_CALL ){` |
|        - | 12469 | `			ph7_expr_node **apNode;` |
|  3216049 | 12470 | `			int hasSpread = 0;` |
|  3216049 | 12471 | `			int hasNamed = 0;` |
|  3216049 | 12472 | `			int bAnySpread = 0;` |
|  3216049 | 12473 | `			sxu32 byRefMask = 0;` |
|        - | 12474 | `			sxi32 nArgs;` |
|        - | 12475 | `			sxi32 n;` |
|        - | 12476 | `			/* Recurse and generate bytecodes for function arguments */` |
|  3216049 | 12477 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  3216049 | 12478 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|        - | 12479 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|        - | 12480 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|        - | 12481 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|  3216049 | 12482 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|       81 | 12483 | `				bFcc = 1;` |
|       81 | 12484 | `				nArgs = 0;` |
|       40 | 12485 | `			}` |
|        - | 12486 | `			/* Validate argument order like php: no positional argument after a` |
|        - | 12487 | ``			 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|        - | 12488 | `			{` |
|  3216049 | 12489 | `				int seenNamed = 0;` |
|  3216049 | 12490 | `				int seenSpread = 0;` |
|  6391491 | 12491 | `				for( n = 0; n < nArgs; ++n ){` |
|  3175449 | 12492 | `					if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|     4073 | 12493 | `						bAnySpread = 1;` |
|     4073 | 12494 | `						seenSpread = 1;` |
|     4073 | 12495 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      ! 0 | 12496 | `							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 12497 | `								"syntax error, unexpected token \"...\"");` |
|      ! 0 | 12498 | `							return SXERR_SYNTAX;` |
|        5 | 12499 | `						}` |
|  3173415 | 12500 | `					}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      289 | 12501 | `						seenNamed = 1;` |
|      289 | 12502 | `						hasNamed = 1;` |
|  3171239 | 12503 | `					}else if( seenNamed ){` |
|        3 | 12504 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 12505 | `							"Cannot use positional argument after named argument");` |
|        3 | 12506 | `						return SXERR_SYNTAX;` |
|  3171095 | 12507 | `					}else if( seenSpread ){` |
|      ! 0 | 12508 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 12509 | `							"Cannot use positional argument after argument unpacking");` |
|      ! 0 | 12510 | `						return SXERR_SYNTAX;` |
|        - | 12511 | `					}` |
|  1587726 | 12512 | `				}` |
|        - | 12513 | `			}` |
|        - | 12514 | `			/* Read-only load */` |
|  3216047 | 12515 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|        - | 12516 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|        - | 12517 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|        - | 12518 | `			 * objects dispatch to the right method (offsetExists for both;` |
|        - | 12519 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|  3216047 | 12520 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|  3216047 | 12521 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  3216042 | 12522 | `				if( pCallName->nByte == 5` |
|  1770457 | 12523 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|   163485 | 12524 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|  3134307 | 12525 | `				}else if( pCallName->nByte == 5` |
|  1606977 | 12526 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|      101 | 12527 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|       48 | 12528 | `				}` |
|        - | 12529 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|        - | 12530 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|        - | 12531 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|        - | 12532 | `				 * write back through. Skipped when spread/named args are present:` |
|        - | 12533 | `				 * the compile-time positional index no longer maps to the` |
|        - | 12534 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|  3216047 | 12535 | `				if( !bAnySpread && !hasNamed ){` |
|        - | 12536 | `					SyString sBuiltin;` |
|  3211849 | 12537 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|  3211849 | 12538 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|  1605922 | 12539 | `				}` |
|  1608021 | 12540 | `			}` |
|  6391487 | 12541 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  3175445 | 12542 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  3175445 | 12543 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|        - | 12544 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|        - | 12545 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|        - | 12546 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|        - | 12547 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|        - | 12548 | `				 * builtin to write back through. A plain $var target is unaffected` |
|        - | 12549 | `				 * (iP1=0 either way). */` |
|  3175445 | 12550 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|       61 | 12551 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|       61 | 12552 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|       28 | 12553 | `				}` |
|  3175445 | 12554 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  3175445 | 12555 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 12556 | `					return rc;` |
|        - | 12557 | `				}` |
|        - | 12558 | `				/* Each argument is an independent nullsafe scope. */` |
|  3175445 | 12559 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  3175445 | 12560 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|        - | 12561 | `					/* Emit spread opcode to unpack this array argument */` |
|     4073 | 12562 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|     4073 | 12563 | `					hasSpread = 1;` |
|     2034 | 12564 | `				}` |
|  1587725 | 12565 | `			}` |
|        - | 12566 | `			/* Total number of given arguments */` |
|  3216047 | 12567 | `			iP1 = nArgs;` |
|  3216047 | 12568 | `			iP2 = hasSpread;` |
|        - | 12569 | `			/* Build VmCallArgMap if named arguments are present.` |
|        - | 12570 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  3216047 | 12571 | `			if( hasNamed ){` |
|      178 | 12572 | `				sxu32 nStrBytes = 0;` |
|        - | 12573 | `				char *zBuf;` |
|      534 | 12574 | `				for( n = 0; n < nArgs; ++n ){` |
|      360 | 12575 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      286 | 12576 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|      141 | 12577 | `					}` |
|      182 | 12578 | `				}` |
|        - | 12579 | `				{` |
|      178 | 12580 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|      178 | 12581 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      174 | 12582 | `					&pGen->pVm->sAllocator, mapSize);` |
|      178 | 12583 | `				if( pMap ){` |
|      178 | 12584 | `					SyZero(pMap, mapSize);` |
|      178 | 12585 | `					pMap->bHasNamed = 1;` |
|      178 | 12586 | `					pMap->nTotal = (sxu32)nArgs;` |
|      178 | 12587 | `					pMap->aNames = (SyString *)&pMap[1];` |
|      178 | 12588 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|      534 | 12589 | `					for( n = 0; n < nArgs; ++n ){` |
|      360 | 12590 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      286 | 12591 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|      286 | 12592 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|      286 | 12593 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|      286 | 12594 | `							zBuf += nb;` |
|      141 | 12595 | `						}` |
|        - | 12596 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|      182 | 12597 | `					}` |
|      178 | 12598 | `					p3 = (void *)pMap;` |
|       87 | 12599 | `				}` |
|        - | 12600 | `				}` |
|       87 | 12601 | `			}` |
|        - | 12602 | `			/* Remove stale flags now */` |
|  3216047 | 12603 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  1608021 | 12604 | `		}` |
|        - | 12605 | `		{` |
|        - | 12606 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|        - | 12607 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|        - | 12608 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|        - | 12609 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|        - | 12610 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|        - | 12611 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|        - | 12612 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|        - | 12613 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
| 12466835 | 12614 | `			sxi32 iLeftFlags = iFlags;` |
| 12466830 | 12615 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
| 10400111 | 12616 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|  4166722 | 12617 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  3698909 | 12618 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   951561 | 12619 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   475778 | 12620 | `			}` |
|        - | 12621 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|        - | 12622 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|        - | 12623 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|        - | 12624 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|        - | 12625 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|        - | 12626 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|        - | 12627 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
| 12466830 | 12628 | `			if( pNode->pOp` |
| 17709250 | 12629 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
| 11475882 | 12630 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
| 10484882 | 12631 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|  2013769 | 12632 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|  1006882 | 12633 | `			}` |
| 12466835 | 12634 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|        - | 12635 | `		}` |
| 12466835 | 12636 | `		if( rc != SXRET_OK ){` |
|       34 | 12637 | `			return rc;` |
|        - | 12638 | `		}` |
| 12466805 | 12639 | `		if( !bIsChainOp ){` |
|        - | 12640 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|        - | 12641 | `			 * target the end of that LHS chain, which is right here. */` |
|  5630023 | 12642 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  2815009 | 12643 | `		}` |
| 12466805 | 12644 | `		if( iVmOp == PH7_OP_CALL ){` |
|  3216047 | 12645 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  3216047 | 12646 | `			if( pInstr ){` |
|  3216047 | 12647 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  2365447 | 12648 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|        - | 12649 | `					sxu32 nQual;` |
|  2365447 | 12650 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|        - | 12651 | `					/* Prevent constant expansion but preserve the absolute flag` |
|        - | 12652 | `					 * so the later NEW handler (if any) can see it. */` |
|  2365447 | 12653 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|        - | 12654 | `					/* Namespace-qualify the function name for CALL, unless the` |
|        - | 12655 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|        - | 12656 | `					 * imports — class imports must NOT affect function` |
|        - | 12657 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|        - | 12658 | `					 * before NEW; we store the original literal index in the` |
|        - | 12659 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|        - | 12660 | `					 * the unqualified name and re-qualify with class imports. */` |
|  2365447 | 12661 | `					if( bAbsolute ){` |
|     3917 | 12662 | `						pInstr->iP2 = (sxi32)nOrig;` |
|     1961 | 12663 | `					}else{` |
|  2361535 | 12664 | `						int fromImport = 0;` |
|  2361535 | 12665 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  2361535 | 12666 | `						pInstr->iP2 = (sxi32)nQual;` |
|  2361535 | 12667 | `						if( nQual != nOrig ){` |
|        - | 12668 | `							/* Record the original literal index in the arg map` |
|        - | 12669 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|        - | 12670 | `							 * flag) so the NEW handler can recover the` |
|        - | 12671 | `							 * unqualified name and re-qualify with CLASS` |
|        - | 12672 | `							 * imports. */` |
|       77 | 12673 | `							if( p3 == 0 ){` |
|       77 | 12674 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       72 | 12675 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|       77 | 12676 | `								if( pMap ){` |
|       77 | 12677 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|       77 | 12678 | `									p3 = (void *)pMap;` |
|       36 | 12679 | `								}` |
|       36 | 12680 | `							}` |
|       77 | 12681 | `							if( p3 ){` |
|       77 | 12682 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|       77 | 12683 | `								if( !fromImport ){` |
|        - | 12684 | `									/* Mark as namespace-qualified */` |
|       67 | 12685 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|       31 | 12686 | `								}` |
|       36 | 12687 | `							}` |
|       36 | 12688 | `						}` |
|        5 | 12689 | `					}` |
|  2033326 | 12690 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|        - | 12691 | `					/* Method call,flag that */` |
|   846115 | 12692 | `					pInstr->iP2 = 1;` |
|   423055 | 12693 | `				}` |
|  1608026 | 12694 | `			}` |
| 10858784 | 12695 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|        - | 12696 | `			ph7_expr_node **apNode;` |
|        - | 12697 | `			sxi32 n;` |
|  1606981 | 12698 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|        - | 12699 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|        - | 12700 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|        - | 12701 | `			/* Recurse and generate bytecodes for array index */` |
|  1606981 | 12702 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  3085655 | 12703 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|  1478679 | 12704 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  1478679 | 12705 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|  1478679 | 12706 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 12707 | `					return rc;` |
|        - | 12708 | `				}` |
|        - | 12709 | `				/* Each subscript index is an independent nullsafe scope. */` |
|  1478679 | 12710 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   739342 | 12711 | `			}` |
|  1606981 | 12712 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|  1478679 | 12713 | `				iP1 = 1; /* Node have an index associated with it */` |
|   739337 | 12714 | `			}` |
|  1606981 | 12715 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|        - | 12716 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|   202213 | 12717 | `				iP2 = 4;` |
|  1505877 | 12718 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        - | 12719 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|        - | 12720 | `				 * so the trailing unset() builtin can drop the slot. */` |
|       72 | 12721 | `				iP2 = 5;` |
|  1404739 | 12722 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        - | 12723 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|        - | 12724 | `				 * short-circuit on missing keys without invoking offsetGet` |
|        - | 12725 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|       29 | 12726 | `				iP2 = 6;` |
|  1404693 | 12727 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|        - | 12728 | `				/* Create an empty entry when the desired index is not found */` |
|   190899 | 12729 | `				iP2 = 1;` |
|    95452 | 12730 | `			}` |
|  8447275 | 12731 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|        - | 12732 | `			/* POP the left node */` |
|       32 | 12733 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       15 | 12734 | `		}` |
|  6233400 | 12735 | `	}` |
| 12478455 | 12736 | `	rc = SXRET_OK;` |
| 12478455 | 12737 | `	nJmpIdx = 0;` |
|        - | 12738 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|        - | 12739 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|        - | 12740 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 12478455 | 12741 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|    43419 | 12742 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    43419 | 12743 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    43419 | 12744 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    43419 | 12745 | `			int isSpecial = 0;` |
|    43419 | 12746 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|    20091 | 12747 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|    20091 | 12748 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|    20086 | 12749 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    31682 | 12750 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    15843 | 12751 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|    11789 | 12752 | `					isSpecial = 1;` |
|     5892 | 12753 | `				}` |
|    15875 | 12754 | `			}` |
|    55083 | 12755 | `			pInstr->iP1 = 0;` |
|    55083 | 12756 | `			if( !isSpecial ){` |
|    19971 | 12757 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     9983 | 12758 | `			}` |
|        - | 12759 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|        - | 12760 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|    31755 | 12761 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    19971 | 12762 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    19971 | 12763 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|       60 | 12764 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|       62 | 12765 | `					return SXRET_OK;` |
|        - | 12766 | `				}` |
|     9954 | 12767 | `			}` |
|    15846 | 12768 | `		}` |
|    39153 | 12769 | `	}` |
|        - | 12770 | `	/* Generate code for the right tree */` |
| 12466747 | 12771 | `	if( pNode->pRight ){` |
|  6803941 | 12772 | `		if( iVmOp == PH7_OP_LAND ){` |
|        - | 12773 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|   136471 | 12774 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  6735708 | 12775 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|        - | 12776 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    93399 | 12777 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  6620778 | 12778 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|        - | 12779 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|      141 | 12780 | `			iVmOp = 0; /* No binary operator to emit */` |
|      141 | 12781 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  6574065 | 12782 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|        - | 12783 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|        - | 12784 | `			 * the entire containing postfix chain to null. The jump target is` |
|        - | 12785 | `			 * patched later by the innermost non-chain ancestor (or by` |
|        - | 12786 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|        - | 12787 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|        - | 12788 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|      108 | 12789 | `			sxu32 nNsJmp = 0;` |
|      108 | 12790 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|      108 | 12791 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  6573893 | 12792 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|        - | 12793 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|        - | 12794 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|        - | 12795 | `			 * auto-created — PHP auto-vivifies on write. */` |
|  2321931 | 12796 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|  1160963 | 12797 | `		}` |
|  6803941 | 12798 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  6803941 | 12799 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  6803941 | 12800 | `		if( !bIsChainOp ){` |
|        - | 12801 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|        - | 12802 | `			 * operator instruction is emitted. */` |
|  4790235 | 12803 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  2395115 | 12804 | `		}` |
|  6803941 | 12805 | `		if( iVmOp == PH7_OP_STORE ){` |
|  2034201 | 12806 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  2034166 | 12807 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|        - | 12808 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|        - | 12809 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|        - | 12810 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|        - | 12811 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|        - | 12812 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|        - | 12813 | `				 */` |
|       91 | 12814 | `				iVmOp = 0;` |
|  2034158 | 12815 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  2034115 | 12816 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|        - | 12817 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   249177 | 12818 | `					iP2 = 1;` |
|   124591 | 12819 | `				}else{` |
|  1784943 | 12820 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        - | 12821 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   190817 | 12822 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   190817 | 12823 | `						iP1 = pInstr->iP1;` |
|    95411 | 12824 | `					}else{` |
|  1594131 | 12825 | `						p3 = pInstr->p3;` |
|        - | 12826 | `					}` |
|        - | 12827 | `					/* POP the last dynamic load instruction */` |
|  1784943 | 12828 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        - | 12829 | `				}` |
|  1017060 | 12830 | `			}` |
|  5786843 | 12831 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|       64 | 12832 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|       64 | 12833 | `			if( pInstr ){` |
|       64 | 12834 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        - | 12835 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|        - | 12836 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|        - | 12837 | `					 */` |
|       19 | 12838 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|       19 | 12839 | `					iP1 = pInstr->iP1;` |
|       19 | 12840 | `					iP2 = pInstr->iP2;` |
|       19 | 12841 | `					p3  = pInstr->p3;` |
|       10 | 12842 | `				}else{` |
|       46 | 12843 | `					p3 = pInstr->p3;` |
|        - | 12844 | `				}` |
|       30 | 12845 | `			}` |
|       30 | 12846 | `		}` |
|  3401968 | 12847 | `	}` |
| 12466742 | 12848 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|   242127 | 12849 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|        - | 12850 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|        - | 12851 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|       32 | 12852 | `		iVmOp = 0;` |
|       14 | 12853 | `	}` |
| 12466747 | 12854 | `	if( iVmOp > 0 ){` |
| 12466467 | 12855 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    70375 | 12856 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|        - | 12857 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    11685 | 12858 | `				iP1 = 1;` |
|     5845 | 12859 | `			}` |
| 12431282 | 12860 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|        - | 12861 | `			/* Namespace-qualify the class name for NEW */ {` |
|   483945 | 12862 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   483945 | 12863 | `				VmInstr *pCallInstr = 0;` |
|   483945 | 12864 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   483697 | 12865 | `					pCallInstr = pPeek;` |
|   483697 | 12866 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|   241846 | 12867 | `				}` |
|   483945 | 12868 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   483941 | 12869 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|        - | 12870 | `					sxu32 nLitForClass;` |
|   483941 | 12871 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|        - | 12872 | `					/* If the CALL handler qualified the name with FUNCTION` |
|        - | 12873 | `					 * imports, recover the original literal (recorded in the` |
|        - | 12874 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|        - | 12875 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|        - | 12876 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|        - | 12877 | `					 * with class imports. */` |
|   483941 | 12878 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|       37 | 12879 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|       21 | 12880 | `					}else{` |
|   483909 | 12881 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|        - | 12882 | `					}` |
|   483941 | 12883 | `					pPeek->iP1 = 0;` |
|   483941 | 12884 | `					if( !bAbsolute ){` |
|   480033 | 12885 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|   240019 | 12886 | `					}else{` |
|     3913 | 12887 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|        - | 12888 | `					}` |
|   241968 | 12889 | `				}` |
|        - | 12890 | `			}` |
|   483945 | 12891 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   483945 | 12892 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|        - | 12893 | `				VmInstr *pPrev;` |
|   483697 | 12894 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   483697 | 12895 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|        - | 12896 | `					/* Pop the call instruction, preserve named-arg map and` |
|        - | 12897 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|        - | 12898 | `					 * accumulator exactly like OP_CALL would have). */` |
|   483697 | 12899 | `					iP1 = pInstr->iP1;` |
|   483697 | 12900 | `					iP2 = pInstr->iP2;` |
|   483697 | 12901 | `					if( pInstr->p3 ){` |
|       47 | 12902 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|       21 | 12903 | `					}` |
|   483697 | 12904 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|   241846 | 12905 | `				}` |
|   241851 | 12906 | `			}` |
| 12154127 | 12907 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|        - | 12908 | `			/* instanceof: right operand is a class name, not a constant.` |
|        - | 12909 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|    31301 | 12910 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    31301 | 12911 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    31301 | 12912 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    31301 | 12913 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|    31301 | 12914 | `				int isSpecialIs = 0;` |
|    31301 | 12915 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    31301 | 12916 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    31301 | 12917 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    31296 | 12918 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    31299 | 12919 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    15648 | 12920 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|       12 | 12921 | `						isSpecialIs = 1;` |
|        5 | 12922 | `					}` |
|    15648 | 12923 | `				}` |
|    31301 | 12924 | `				pInstr->iP1 = 0;` |
|    31301 | 12925 | `				if( !isSpecialIs && !bAbsolute ){` |
|    31281 | 12926 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    15638 | 12927 | `				}` |
|    15653 | 12928 | `			}` |
| 11896509 | 12929 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|        - | 12930 | `			/* Prevent constant expansion for member/property names.` |
|        - | 12931 | `			 * The right child (member name) was just compiled — its LOADC` |
|        - | 12932 | `			 * should not trigger constant lookup. */` |
|  2013711 | 12933 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  2013711 | 12934 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  2013639 | 12935 | `				pInstr->iP1 = 0;` |
|  1006817 | 12936 | `			}` |
|  2013711 | 12937 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|        - | 12938 | `				/* Static member access,remember that */` |
|    31711 | 12939 | `				iP1 = 1;` |
|    31711 | 12940 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    31711 | 12941 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|       62 | 12942 | `					p3 = pInstr->p3;` |
|       62 | 12943 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       29 | 12944 | `				}` |
|    15853 | 12945 | `			}` |
|        - | 12946 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|        - | 12947 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|        - | 12948 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|        - | 12949 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|  2013711 | 12950 | `			if( iP2 == PH7_MEMBER_READ ){` |
|  2013711 | 12951 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       36 | 12952 | `					iP2 = PH7_MEMBER_UNSET;` |
|  2013694 | 12953 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       91 | 12954 | `					iP2 = PH7_MEMBER_ISSET;` |
|  2013634 | 12955 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       15 | 12956 | `					iP2 = PH7_MEMBER_EMPTY;` |
|  2013584 | 12957 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|        - | 12958 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|   249257 | 12959 | `					iP2 = PH7_MEMBER_WRITE;` |
|   124626 | 12960 | `				}` |
|  1006853 | 12961 | `			}` |
|  1006853 | 12962 | `		}` |
|        - | 12963 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|        - | 12964 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|        - | 12965 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|        - | 12966 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|        - | 12967 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
| 12466467 | 12968 | `		if( bFcc ){` |
|       81 | 12969 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|       81 | 12970 | `			iP2 = 0;` |
|       81 | 12971 | `			p3 = 0;` |
|       81 | 12972 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|       81 | 12973 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|        - | 12974 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|        - | 12975 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|        - | 12976 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|        - | 12977 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|       37 | 12978 | `				void *pMemberName = pInstr->p3;` |
|       37 | 12979 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|       37 | 12980 | `				if( pMemberName ){` |
|        3 | 12981 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|        1 | 12982 | `				}` |
|       37 | 12983 | `				iP1 = 2;` |
|       19 | 12984 | `			}else{` |
|       45 | 12985 | `				iP1 = 1;` |
|        - | 12986 | `			}` |
|       40 | 12987 | `		}` |
|        - | 12988 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|        - | 12989 | `		 * This is the primary emit path for user-visible calls. */` |
| 12466467 | 12990 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  3699907 | 12991 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  1849951 | 12992 | `		}` |
|        - | 12993 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 12466467 | 12994 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  6233231 | 12995 | `	}` |
| 12466747 | 12996 | `	if( nJmpIdx > 0 ){` |
|        - | 12997 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   230001 | 12998 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   230001 | 12999 | `		if( pInstr ){` |
|   230001 | 13000 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|   114998 | 13001 | `		}` |
|   114998 | 13002 | `	}` |
| 12466747 | 13003 | `	return rc;` |
| 15886011 | 13004 | `}` |
|        - | 13005 | `/*` |
|        - | 13006 | ` * Compile a PHP expression.` |
|        - | 13007 | ` * According to the PHP language reference manual:` |
|        - | 13008 | ` *  Expressions are the most important building stones of PHP.` |
|        - | 13009 | ` *  In PHP, almost anything you write is an expression.` |
|        - | 13010 | ` *  The simplest yet most accurate way to define an expression` |
|        - | 13011 | ` *  is "anything that has a value".` |
|        - | 13012 | ` * If something goes wrong while compiling the expression,this` |
|        - | 13013 | ` * function takes care of generating the appropriate error` |
|        - | 13014 | ` * message.` |
|        - | 13015 | ` */` |
|  7208474 | 13016 | `static sxi32 PH7_CompileExpr(` |
|        - | 13017 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 13018 | `	sxi32 iFlags,        /* Control flags */` |
|        - | 13019 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|        - | 13020 | `	)` |
|        5 | 13021 | `{` |
|        - | 13022 | `	ph7_expr_node *pRoot;` |
|        - | 13023 | `	SySet sExprNode;` |
|        - | 13024 | `	SyToken *pEnd;` |
|        - | 13025 | `	sxi32 nExpr;` |
|        - | 13026 | `	sxi32 iNest;` |
|        - | 13027 | `	sxi32 rc;` |
|        - | 13028 | `	sxu32 nNullsafeBase;` |
|        - | 13029 | `	/* Initialize worker variables */` |
|  7208479 | 13030 | `	nExpr = 0;` |
|  7208479 | 13031 | `	pRoot = 0;` |
|        - | 13032 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|        - | 13033 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  7208479 | 13034 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  7208479 | 13035 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  7208479 | 13036 | `	SySetAlloc(&sExprNode,0x10);` |
|  7208479 | 13037 | `	rc = SXRET_OK;` |
|        - | 13038 | `	/* Delimit the expression */` |
|  7208479 | 13039 | `	pEnd = pGen->pIn;` |
|  7208479 | 13040 | `	iNest = 0;` |
| 56088427 | 13041 | `	while( pEnd < pGen->pEnd ){` |
| 53525909 | 13042 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|        - | 13043 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      701 | 13044 | `			iNest++;` |
| 53525561 | 13045 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      709 | 13046 | `			iNest--;` |
| 53524861 | 13047 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  4646547 | 13048 | `			if( iNest <= 0 ){` |
|  4645961 | 13049 | `				break;` |
|        - | 13050 | `			}` |
|      293 | 13051 | `		}` |
| 48879953 | 13052 | `		pEnd++;` |
|        5 | 13053 | `	}` |
|  7208479 | 13054 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   237787 | 13055 | `		SyToken *pEnd2 = pGen->pIn;` |
|   237787 | 13056 | `		iNest = 0;` |
|        - | 13057 | `		/* Stop at the first comma */` |
|   553739 | 13058 | `		while( pEnd2 < pEnd ){` |
|   315963 | 13059 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|     7857 | 13060 | `				iNest++;` |
|   312037 | 13061 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|     7857 | 13062 | `				iNest--;` |
|   304185 | 13063 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|       65 | 13064 | `				if( iNest <= 0 ){` |
|        7 | 13065 | `					break;` |
|        - | 13066 | `				}` |
|       27 | 13067 | `			}` |
|   315957 | 13068 | `			pEnd2++;` |
|        5 | 13069 | `		}` |
|   237787 | 13070 | `		if( pEnd2 <pEnd ){` |
|        7 | 13071 | `			pEnd = pEnd2;` |
|        3 | 13072 | `		}` |
|   118891 | 13073 | `	}` |
|  7208479 | 13074 | `	if( pEnd > pGen->pIn ){` |
|  7208469 | 13075 | `		SyToken *pTmp = pGen->pEnd;` |
|        - | 13076 | `		/* Swap delimiter */` |
|  7208469 | 13077 | `		pGen->pEnd = pEnd;` |
|        - | 13078 | `		/* Try to get an expression tree */` |
|  7208469 | 13079 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  7208469 | 13080 | `		if( rc == SXRET_OK && pRoot ){` |
|  7208287 | 13081 | `			rc = SXRET_OK;` |
|  7208287 | 13082 | `			if( xTreeValidator ){` |
|        - | 13083 | `				/* Call the upper layer validator callback */` |
|   563719 | 13084 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   281857 | 13085 | `			}` |
|  7208287 | 13086 | `			if( rc != SXERR_ABORT ){` |
|        - | 13087 | `				/* Generate code for the given tree */` |
|  7208287 | 13088 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|        - | 13089 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|        - | 13090 | `				 * expression so they short-circuit to its end. */` |
|  7208287 | 13091 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  3604141 | 13092 | `			}` |
|  7208287 | 13093 | `			nExpr = 1;` |
|  3604141 | 13094 | `		}` |
|        - | 13095 | `		/* Release the whole tree */` |
|  7208469 | 13096 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|        - | 13097 | `		/* Synchronize token stream */` |
|  7208469 | 13098 | `		pGen->pEnd = pTmp;` |
|  7208469 | 13099 | `		pGen->pIn  = pEnd;` |
|  7208469 | 13100 | `		if( rc == SXERR_ABORT ){` |
|       13 | 13101 | `			SySetRelease(&sExprNode);` |
|       13 | 13102 | `			return SXERR_ABORT;` |
|        - | 13103 | `		}` |
|  3604227 | 13104 | `	}` |
|  7208469 | 13105 | `	SySetRelease(&sExprNode);` |
|  7208469 | 13106 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  3604242 | 13107 | `}` |
|        - | 13108 | `/*` |
|        - | 13109 | ` * Return a pointer to the node construct handler associated` |
|        - | 13110 | ` * with a given node type [i.e: string,integer,float,...].` |
|        - | 13111 | ` */` |
|  4329394 | 13112 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|        5 | 13113 | `{` |
|  4329399 | 13114 | `	if( nNodeType & PH7_TK_NUM ){` |
|        - | 13115 | `		/* Numeric literal: Either real or integer */` |
|  1296913 | 13116 | `		return PH7_CompileNumLiteral;` |
|  3032491 | 13117 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|        - | 13118 | `		/* Double quoted string */` |
|    36969 | 13119 | `		return PH7_CompileString;` |
|  2995527 | 13120 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|        - | 13121 | `		/* Single quoted string */` |
|  2995407 | 13122 | `		return PH7_CompileSimpleString;` |
|      125 | 13123 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|        - | 13124 | `		/* Heredoc */` |
|       71 | 13125 | `		return PH7_CompileHereDoc;` |
|       58 | 13126 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|        - | 13127 | `		/* Nowdoc */` |
|       51 | 13128 | `		return PH7_CompileNowDoc;` |
|        9 | 13129 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|        - | 13130 | `		/* Backtick quoted string */` |
|        6 | 13131 | `		return PH7_CompileBacktic;` |
|        - | 13132 | `	}` |
|        3 | 13133 | `	return 0;` |
|  2164702 | 13134 | `}` |
|        - | 13135 | `/*` |
|        - | 13136 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|        - | 13137 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|        - | 13138 | ` * in write context" parse error.` |
|        - | 13139 | ` */` |
|     6852 | 13140 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|        5 | 13141 | `{` |
|        - | 13142 | `	sxi32 rc;` |
|     6857 | 13143 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|     6855 | 13144 | `		return SXRET_OK;` |
|        - | 13145 | `	}` |
|        5 | 13146 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|        2 | 13147 | `		pNode ? pNode->pStart->nLine : 1,` |
|        - | 13148 | `		"Can't use nullsafe operator in write context");` |
|        3 | 13149 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     3431 | 13150 | `}` |
|        - | 13151 | `/*` |
|        - | 13152 | ` * Compile an unset() statement.` |
|        - | 13153 | ` * unset($var, $arr[$key], ...);` |
|        - | 13154 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|        - | 13155 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|        - | 13156 | ` * parent array before extracting the element to unset.` |
|        - | 13157 | ` */` |
|     2930 | 13158 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|        5 | 13159 | `{` |
|     2935 | 13160 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     2935 | 13161 | `	sxu32 nIdx = 0;` |
|        - | 13162 | `	SyString sName;` |
|        - | 13163 | `	sxi32 rc;` |
|        - | 13164 | `	/* Jump the 'unset' keyword */` |
|     2935 | 13165 | `	pGen->pIn++;` |
|        - | 13166 | `	/* Save delimiter */` |
|     2935 | 13167 | `	pTmp = pGen->pEnd;` |
|        - | 13168 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     2935 | 13169 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     2935 | 13170 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        - | 13171 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|        - | 13172 | `		SyToken *pClose;` |
|     2935 | 13173 | `		pGen->pIn++;   /* Skip '(' */` |
|     2935 | 13174 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     2935 | 13175 | `		pEnd = pClose; /* Stop at ')' */` |
|     1465 | 13176 | `	}` |
|     2935 | 13177 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|        - | 13178 | `	/* Resolve the 'unset' builtin name once */` |
|     2935 | 13179 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      379 | 13180 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      379 | 13181 | `		if( pObj == 0 ){` |
|      ! 0 | 13182 | `			return SXERR_ABORT;` |
|        - | 13183 | `		}` |
|      379 | 13184 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      379 | 13185 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      187 | 13186 | `	}` |
|        - | 13187 | `	/* Compile each comma-separated argument */` |
|     9789 | 13188 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     6859 | 13189 | `		if( pGen->pIn < pNext ){` |
|     6859 | 13190 | `			pGen->pEnd = pNext;` |
|     6859 | 13191 | `			rc = PH7_CompileExpr(&(*pGen),` |
|        - | 13192 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|        - | 13193 | `				GenStateUnsetValidator);` |
|     6859 | 13194 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 13195 | `				return SXERR_ABORT;` |
|        - | 13196 | `			}` |
|     6859 | 13197 | `			if( rc != SXERR_EMPTY ){` |
|        - | 13198 | `				/* Emit call for this single argument */` |
|     6857 | 13199 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     6857 | 13200 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|     6857 | 13201 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     3426 | 13202 | `			}` |
|     3427 | 13203 | `		}` |
|        - | 13204 | `		/* Jump trailing commas */` |
|    10785 | 13205 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|     3931 | 13206 | `			pNext++;` |
|        5 | 13207 | `		}` |
|     6859 | 13208 | `		pGen->pIn = pNext;` |
|        5 | 13209 | `	}` |
|        - | 13210 | `	/* Skip past the closing ')' if present */` |
|     2935 | 13211 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     2935 | 13212 | `		pGen->pIn++;` |
|     1465 | 13213 | `	}` |
|        - | 13214 | `	/* Restore token stream */` |
|     2935 | 13215 | `	pGen->pEnd = pTmp;` |
|     2935 | 13216 | `	return SXRET_OK;` |
|     1470 | 13217 | `}` |
|        - | 13218 | `/*` |
|        - | 13219 | ` * PHP Language construct table.` |
|        - | 13220 | ` */` |
|        - | 13221 | `static const LangConstruct aLangConstruct[] = {` |
|        - | 13222 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|        - | 13223 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|        - | 13224 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|        - | 13225 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|        - | 13226 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|        - | 13227 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|        - | 13228 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|        - | 13229 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|        - | 13230 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|        - | 13231 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|        - | 13232 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|        - | 13233 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|        - | 13234 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|        - | 13235 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|        - | 13236 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|        - | 13237 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|        - | 13238 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|        - | 13239 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|        - | 13240 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|        - | 13241 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|        - | 13242 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|        - | 13243 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|        - | 13244 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|        - | 13245 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|        - | 13246 | `};` |
|        - | 13247 | `/*` |
|        - | 13248 | ` * Return a pointer to the statement handler routine associated` |
|        - | 13249 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|        - | 13250 | ` */` |
|  3814818 | 13251 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|        - | 13252 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|        - | 13253 | `	SyToken *pLookahed  /* Look-ahead token */` |
|        - | 13254 | `	)` |
|        5 | 13255 | `{` |
|  3814823 | 13256 | `	sxu32 n = 0;` |
| 15531480 | 13257 | `	for(;;){` |
| 31062965 | 13258 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   246867 | 13259 | `			break;` |
|        - | 13260 | `		}` |
| 30816103 | 13261 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  3567961 | 13262 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|      ! 0 | 13263 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|      ! 0 | 13264 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|        - | 13265 | `					/* 'static' (class context),return null */` |
|      ! 0 | 13266 | `					return 0;` |
|        - | 13267 | `				}` |
|      ! 0 | 13268 | `			}` |
|  3567956 | 13269 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       14 | 13270 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       14 | 13271 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|        - | 13272 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|        3 | 13273 | `				return 0;` |
|        - | 13274 | `			}` |
|        - | 13275 | `			/* Return a pointer to the handler.` |
|        - | 13276 | `			*/` |
|  3567959 | 13277 | `			return aLangConstruct[n].xConstruct;` |
|        - | 13278 | `		}` |
| 27248147 | 13279 | `		n++;` |
|        5 | 13280 | `	}` |
|   246867 | 13281 | `	if( pLookahed ){` |
|   246867 | 13282 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    46713 | 13283 | `			return PH7_CompileClassInterface;` |
|   200159 | 13284 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   187953 | 13285 | `			return PH7_CompileClass;` |
|    12211 | 13286 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|       77 | 13287 | `			return PH7_CompileTrait;` |
|        - | 13288 | `		}` |
|        - | 13289 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|        - | 13290 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|        - | 13291 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|        - | 13292 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     6067 | 13293 | `	}` |
|        - | 13294 | `	/* Not a language construct */` |
|    12139 | 13295 | `	return 0;` |
|  1907414 | 13296 | `}` |
|        - | 13297 | `/*` |
|        - | 13298 | ` * Check if the given keyword is in fact a PHP language construct.` |
|        - | 13299 | ` * Return TRUE on success. FALSE otheriwse.` |
|        - | 13300 | ` */` |
|    12136 | 13301 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|        5 | 13302 | `{` |
|        - | 13303 | `	int rc;` |
|    12141 | 13304 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|    12141 | 13305 | `	if( rc == FALSE ){` |
|    12022 | 13306 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      366 | 13307 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|        - | 13308 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|        - | 13309 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|        - | 13310 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|        - | 13311 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|        - | 13312 | `			*/` |
|        - | 13313 | `			){` |
|    12019 | 13314 | `				rc = TRUE;` |
|     6007 | 13315 | `		}` |
|     6011 | 13316 | `	}` |
|    12141 | 13317 | `	return rc;` |
|        5 | 13318 | `}` |
|        - | 13319 | `/*` |
|        - | 13320 | ` * Compile a PHP chunk.` |
|        - | 13321 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|        - | 13322 | ` * takes care of generating the appropriate error message.` |
|        - | 13323 | ` */` |
|        - | 13324 | `/*` |
|        - | 13325 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|        - | 13326 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|        - | 13327 | ` * the chunk token set it becomes the pending docblock. An existing` |
|        - | 13328 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|        - | 13329 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|        - | 13330 | ` * intervening non-declaration statements.` |
|        - | 13331 | ` */` |
|  7989642 | 13332 | `static void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|        5 | 13333 | `{` |
|  7989647 | 13334 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  7989647 | 13335 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  7989647 | 13336 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|        - | 13337 | `	sxu32 nIdx, n;` |
|  7989642 | 13338 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|  1537013 | 13339 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|        - | 13340 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|        - | 13341 | `		 * indexes do not map to the sidecar */` |
|  6452641 | 13342 | `		return;` |
|        - | 13343 | `	}` |
|  1537011 | 13344 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|        - | 13345 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|        - | 13346 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|  1537011 | 13347 | `	SySetReset(&pGen->aPendingAttrs);` |
|  4612517 | 13348 | `	for( n = 0 ; n < nT ; n++ ){` |
|  3075511 | 13349 | `		if( aT[n].nTokIdx != nIdx ){` |
|  3067579 | 13350 | `			continue;` |
|        - | 13351 | `		}` |
|     7937 | 13352 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|       29 | 13353 | `			pGen->sPendingDoc = aT[n].sText;` |
|     7925 | 13354 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|     7913 | 13355 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|     3954 | 13356 | `		}` |
|     3971 | 13357 | `	}` |
|  3994826 | 13358 | `}` |
|        - | 13359 | `/*` |
|        - | 13360 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|        - | 13361 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|        - | 13362 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|        - | 13363 | ` */` |
|  2130846 | 13364 | `static void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|        5 | 13365 | `{` |
|        - | 13366 | `	char *zDup;` |
|  2130851 | 13367 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|  2130831 | 13368 | `		return;` |
|        - | 13369 | `	}` |
|       35 | 13370 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       10 | 13371 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|       25 | 13372 | `	if( zDup ){` |
|       25 | 13373 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|       10 | 13374 | `	}` |
|       25 | 13375 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|  1065428 | 13376 | `}` |
|        - | 13377 | `/*` |
|        - | 13378 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|        - | 13379 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|        - | 13380 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|        - | 13381 | ` * names may point into the token text, which must outlive the raw script` |
|        - | 13382 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|        - | 13383 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|        - | 13384 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|        - | 13385 | ` */` |
|     7920 | 13386 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|        5 | 13387 | `{` |
|        - | 13388 | `	SySet *pToken;` |
|        - | 13389 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|        - | 13390 | `	char *zSpan;` |
|     7925 | 13391 | `	sxi32 rc = SXRET_OK;` |
|     7925 | 13392 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|      ! 0 | 13393 | `		return SXRET_OK;` |
|        - | 13394 | `	}` |
|    11885 | 13395 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     3960 | 13396 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|     7925 | 13397 | `	if( zSpan == 0 ){` |
|      ! 0 | 13398 | `		return SXRET_OK;` |
|        - | 13399 | `	}` |
|        - | 13400 | `	/* The token set must outlive compilation too: interned operands may` |
|        - | 13401 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|        - | 13402 | `	 * the number of attribute declarations in the program. */` |
|     7925 | 13403 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|     7925 | 13404 | `	if( pToken == 0 ){` |
|      ! 0 | 13405 | `		return SXRET_OK;` |
|        - | 13406 | `	}` |
|     7925 | 13407 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|     7925 | 13408 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|     7925 | 13409 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|     7925 | 13410 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|     7925 | 13411 | `	pSavedIn = pGen->pIn;` |
|     7925 | 13412 | `	pSavedEnd = pGen->pEnd;` |
|     7929 | 13413 | `	while( pIn < pEnd ){` |
|        - | 13414 | `		ph7_attribute sAttr;` |
|        - | 13415 | `		SyBlob sFQN;` |
|     7929 | 13416 | `		int bAbsolute = 0;` |
|     7929 | 13417 | `		SyZero(&sAttr,sizeof(sAttr));` |
|     7929 | 13418 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|     7929 | 13419 | `		sAttr.nLine = pIn->nLine;` |
|     7929 | 13420 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|       75 | 13421 | `			bAbsolute = 1;` |
|       75 | 13422 | `			pIn++;` |
|       35 | 13423 | `		}` |
|     7929 | 13424 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|     7929 | 13425 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     7929 | 13426 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|     7929 | 13427 | `			pIn++;` |
|     7929 | 13428 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|      ! 0 | 13429 | `				SyBlobAppend(&sFQN,"\\",1);` |
|      ! 0 | 13430 | `				pIn++;` |
|      ! 0 | 13431 | `				continue;` |
|        - | 13432 | `			}` |
|     7929 | 13433 | `			break;` |
|      ! 0 | 13434 | `		}` |
|     7929 | 13435 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|        - | 13436 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|        - | 13437 | `			 * this feature; never turn it into a new fatal) */` |
|      ! 0 | 13438 | `			SyBlobRelease(&sFQN);` |
|      ! 0 | 13439 | `			break;` |
|        - | 13440 | `		}` |
|        - | 13441 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|        - | 13442 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|        - | 13443 | `		{` |
|     7929 | 13444 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|     7929 | 13445 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|     7929 | 13446 | `			char *zDup = 0;` |
|     7929 | 13447 | `			if( !bAbsolute ){` |
|     7859 | 13448 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|     7859 | 13449 | `				if( pImp ){` |
|      ! 0 | 13450 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|      ! 0 | 13451 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|      ! 0 | 13452 | `					if( zDup ){` |
|      ! 0 | 13453 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|      ! 0 | 13454 | `					}` |
|     7859 | 13455 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        - | 13456 | `					SyBlob sTmp;` |
|      ! 0 | 13457 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|      ! 0 | 13458 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      ! 0 | 13459 | `					SyBlobAppend(&sTmp,"\\",1);` |
|      ! 0 | 13460 | `					SyBlobAppend(&sTmp,zName,nName);` |
|      ! 0 | 13461 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      ! 0 | 13462 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|      ! 0 | 13463 | `					if( zDup ){` |
|      ! 0 | 13464 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|      ! 0 | 13465 | `					}` |
|      ! 0 | 13466 | `					SyBlobRelease(&sTmp);` |
|      ! 0 | 13467 | `				}` |
|     3927 | 13468 | `			}` |
|     7929 | 13469 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|     7929 | 13470 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|     7929 | 13471 | `				if( zDup ){` |
|     7929 | 13472 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|     3962 | 13473 | `				}` |
|     3962 | 13474 | `			}` |
|        - | 13475 | `		}` |
|     7929 | 13476 | `		SyBlobRelease(&sFQN);` |
|     7929 | 13477 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        - | 13478 | `			SyToken *pArgsEnd;` |
|     7827 | 13479 | `			pIn++;` |
|     7827 | 13480 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|    15663 | 13481 | `			while( pIn < pArgsEnd ){` |
|     7841 | 13482 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|     7841 | 13483 | `				sxi32 iDepth = 0;` |
|        - | 13484 | `				ph7_attr_arg sArgRec;` |
|    77925 | 13485 | `				while( pArgStop < pArgsEnd ){` |
|    70105 | 13486 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       11 | 13487 | `						iDepth++;` |
|    70100 | 13488 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       11 | 13489 | `						iDepth--;` |
|    70090 | 13490 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|       17 | 13491 | `						break;` |
|        - | 13492 | `					}` |
|    70089 | 13493 | `					pArgStop++;` |
|        5 | 13494 | `				}` |
|     7841 | 13495 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|     7841 | 13496 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     7836 | 13497 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|     7820 | 13498 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|       28 | 13499 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        9 | 13500 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|       19 | 13501 | `					if( zN ){` |
|       19 | 13502 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|        9 | 13503 | `					}` |
|       19 | 13504 | `					pArgStart += 2;` |
|        9 | 13505 | `				}` |
|     7841 | 13506 | `				if( pArgStart < pArgStop ){` |
|        - | 13507 | `					SySet *pInstrContainer;` |
|     7841 | 13508 | `					pGen->pIn = pArgStart;` |
|     7841 | 13509 | `					pGen->pEnd = pArgStop;` |
|     7841 | 13510 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     7841 | 13511 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|     7841 | 13512 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|     7841 | 13513 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|     7841 | 13514 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     7841 | 13515 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 13516 | `						pGen->pIn = pSavedIn;` |
|      ! 0 | 13517 | `						pGen->pEnd = pSavedEnd;` |
|      ! 0 | 13518 | `						return SXERR_ABORT;` |
|        - | 13519 | `					}` |
|     7841 | 13520 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|     3918 | 13521 | `				}` |
|     7841 | 13522 | `				pIn = pArgStop;` |
|     7841 | 13523 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|       17 | 13524 | `					pIn++;` |
|        8 | 13525 | `				}` |
|        5 | 13526 | `			}` |
|     7827 | 13527 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|     3911 | 13528 | `		}` |
|     7929 | 13529 | `		SySetPut(pOut,(const void *)&sAttr);` |
|     7929 | 13530 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        5 | 13531 | `			pIn++;` |
|        5 | 13532 | `			continue;` |
|        - | 13533 | `		}` |
|     7925 | 13534 | `		break;` |
|      ! 0 | 13535 | `	}` |
|     7925 | 13536 | `	pGen->pIn = pSavedIn;` |
|     7925 | 13537 | `	pGen->pEnd = pSavedEnd;` |
|     7925 | 13538 | `	return SXRET_OK;` |
|     3965 | 13539 | `}` |
|        - | 13540 | `/*` |
|        - | 13541 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|        - | 13542 | ` * every recorded group into pOut and clear the pending list.` |
|        - | 13543 | ` */` |
|  2130850 | 13544 | `static sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|        5 | 13545 | `{` |
|  2130855 | 13546 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|        - | 13547 | `	sxu32 n;` |
|        - | 13548 | `	sxi32 rc;` |
|  2138763 | 13549 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|     7913 | 13550 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|     7913 | 13551 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 13552 | `			return SXERR_ABORT;` |
|        - | 13553 | `		}` |
|     3959 | 13554 | `	}` |
|  2130855 | 13555 | `	SySetReset(&pGen->aPendingAttrs);` |
|  2130855 | 13556 | `	return SXRET_OK;` |
|  1065430 | 13557 | `}` |
|        - | 13558 | `/*` |
|        - | 13559 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|        - | 13560 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|        - | 13561 | ` * the main token stream, so the sidecar indexes map directly.` |
|        - | 13562 | ` */` |
|   718188 | 13563 | `static sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|        5 | 13564 | `{` |
|   718193 | 13565 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   718193 | 13566 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   718193 | 13567 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|        - | 13568 | `	sxu32 nIdx, n;` |
|        - | 13569 | `	sxi32 rc;` |
|   718188 | 13570 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   194535 | 13571 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   523663 | 13572 | `		return SXRET_OK;` |
|        - | 13573 | `	}` |
|   194535 | 13574 | `	nIdx = (sxu32)(pTok - pBase);` |
|   583593 | 13575 | `	for( n = 0 ; n < nT ; n++ ){` |
|   389063 | 13576 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|       13 | 13577 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|       13 | 13578 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 13579 | `				return SXERR_ABORT;` |
|        - | 13580 | `			}` |
|        6 | 13581 | `		}` |
|   194534 | 13582 | `	}` |
|   194535 | 13583 | `	return SXRET_OK;` |
|   359099 | 13584 | `}` |
|  5876554 | 13585 | `static sxi32 GenStateCompileChunk(` |
|        - | 13586 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 13587 | `	sxi32 iFlags         /* Compile flags */` |
|        - | 13588 | `	)` |
|        5 | 13589 | `{` |
|        - | 13590 | `	ProcLangConstruct xCons;` |
|        - | 13591 | `	sxi32 rc;` |
|  5876559 | 13592 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  3354303 | 13593 | `	for(;;){` |
|  6292585 | 13594 | `		int bStmtIsDeclare = 0;` |
|  6292585 | 13595 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 13596 | `			/* No more input to process */` |
|    53355 | 13597 | `			break;` |
|        - | 13598 | `		}` |
|        - | 13599 | `		/* Bind a directly-preceding docblock to this statement */` |
|  6239235 | 13600 | `		GenStateSetPendingDoc(&(*pGen));` |
|  6239235 | 13601 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|        - | 13602 | `			/* php: a statement-position attribute group must be followed by a` |
|        - | 13603 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|        - | 13604 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|        - | 13605 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|        - | 13606 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|     7831 | 13607 | `			int bAttrTarget = 0;` |
|     7826 | 13608 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|     3947 | 13609 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|     7773 | 13610 | `				bAttrTarget = 1;` |
|     3943 | 13611 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       59 | 13612 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       58 | 13613 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|       15 | 13614 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|        4 | 13615 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|        4 | 13616 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|        1 | 13617 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|       59 | 13618 | `					bAttrTarget = 1;` |
|       29 | 13619 | `				}` |
|       29 | 13620 | `			}` |
|     7831 | 13621 | `			if( !bAttrTarget ){` |
|      ! 0 | 13622 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 13623 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|      ! 0 | 13624 | `					&pGen->pIn->sData);` |
|      ! 0 | 13625 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 13626 | `					break;` |
|        - | 13627 | `				}` |
|      ! 0 | 13628 | `				SySetReset(&pGen->aPendingAttrs);` |
|      ! 0 | 13629 | `			}` |
|     3913 | 13630 | `		}` |
|        - | 13631 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|        - | 13632 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  6239235 | 13633 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  3842055 | 13634 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  3842055 | 13635 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|       47 | 13636 | `				bStmtIsDeclare = 1;` |
|       21 | 13637 | `			}` |
|  1921025 | 13638 | `		}` |
|  6239235 | 13639 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|        - | 13640 | `			/* Any non-declare top-level statement locks the strict_types` |
|        - | 13641 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|   415999 | 13642 | `			pGen->bStrictTypesLocked = 1;` |
|   207997 | 13643 | `		}` |
|  6239235 | 13644 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|        - | 13645 | `			/* Compile block */` |
|     3907 | 13646 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|     3907 | 13647 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 13648 | `				break;` |
|        - | 13649 | `			}` |
|     1956 | 13650 | `		}else{` |
|  6235333 | 13651 | `			xCons = 0;` |
|  6235333 | 13652 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|        - | 13653 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|        - | 13654 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|        - | 13655 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|    27263 | 13656 | `				xCons = PH7_CompileClassModifiers;` |
|  6221704 | 13657 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|        - | 13658 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|        - | 13659 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|       33 | 13660 | `				xCons = PH7_CompileEnum;` |
|  6208061 | 13661 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  3814823 | 13662 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        - | 13663 | `				/* Try to extract a language construct handler */` |
|  3814823 | 13664 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  3814823 | 13665 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|       13 | 13666 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 13667 | `						"Syntax error: Unexpected keyword '%z'",` |
|        8 | 13668 | `						&pGen->pIn->sData);` |
|        9 | 13669 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 13670 | `						break;` |
|        - | 13671 | `					}` |
|        - | 13672 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|        - | 13673 | `					 * this erroneous statement.` |
|        - | 13674 | `					 */` |
|        9 | 13675 | `					xCons = PH7_ErrorRecover;` |
|        4 | 13676 | `				}` |
|  4300638 | 13677 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    66517 | 13678 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|        - | 13679 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|      117 | 13680 | `				xCons = PH7_CompileLabel;` |
|       56 | 13681 | `			}` |
|  6235333 | 13682 | `			if( xCons == 0 ){` |
|        - | 13683 | `				/* Assume an expression an try to compile it */` |
|  2405245 | 13684 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  2405245 | 13685 | `				if(  rc != SXERR_EMPTY ){` |
|        - | 13686 | `					/* Pop l-value */` |
|  2405095 | 13687 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  1202545 | 13688 | `				}` |
|  1202625 | 13689 | `			}else{` |
|        - | 13690 | `				/* Go compile the sucker */` |
|  3830093 | 13691 | `				rc = xCons(&(*pGen));` |
|        - | 13692 | `			}` |
|  6235333 | 13693 | `			if( rc == SXERR_ABORT ){` |
|        - | 13694 | `				/* Request to abort compilation */` |
|       13 | 13695 | `				break;` |
|        - | 13696 | `			}` |
|        - | 13697 | `		}` |
|        - | 13698 | `		/* Ignore trailing semi-colons ';' */` |
| 10671723 | 13699 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  4432503 | 13700 | `			pGen->pIn++;` |
|        5 | 13701 | `		}` |
|  6239225 | 13702 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|        - | 13703 | `			/* Compile a single statement and return */` |
|  5823199 | 13704 | `			break;` |
|        - | 13705 | `		}` |
|        - | 13706 | `		/* LOOP ONE */` |
|        - | 13707 | `		/* LOOP TWO */` |
|        - | 13708 | `		/* LOOP THREE */` |
|        - | 13709 | `		/* LOOP FOUR */` |
|        5 | 13710 | `	}` |
|        - | 13711 | `	/* Return compilation status */` |
|  5876559 | 13712 | `	return rc;` |
|        5 | 13713 | `}` |
|        - | 13714 | `/*` |
|        - | 13715 | ` * Compile a Raw PHP chunk.` |
|        - | 13716 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|        - | 13717 | ` * takes care of generating the appropriate error message.` |
|        - | 13718 | ` */` |
|    53362 | 13719 | `static sxi32 PH7_CompilePHP(` |
|        - | 13720 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - | 13721 | `	SySet *pTokenSet,     /* Token set */` |
|        - | 13722 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|        - | 13723 | `	)` |
|        5 | 13724 | `{` |
|    53367 | 13725 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|        - | 13726 | `	sxi32 rc;` |
|        - | 13727 | `	/* Reset the token set (and its trivia sidecar) */` |
|    53367 | 13728 | `	SySetReset(&(*pTokenSet));` |
|    53367 | 13729 | `	SySetReset(&pGen->aTrivia);` |
|        - | 13730 | `	/* Mark as the default token set */` |
|    53367 | 13731 | `	pGen->pTokenSet = &(*pTokenSet);` |
|        - | 13732 | `	/* Advance the stream cursor */` |
|    53367 | 13733 | `	pGen->pRawIn++;` |
|        - | 13734 | `	/* Tokenize the PHP chunk first */` |
|    53367 | 13735 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|        - | 13736 | `	/* Point to the head and tail of the token stream. */` |
|    53367 | 13737 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|    53367 | 13738 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|    53367 | 13739 | `	if( is_expr ){` |
|      ! 0 | 13740 | `		rc = SXERR_EMPTY;` |
|      ! 0 | 13741 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - | 13742 | `			/* A simple expression,compile it */` |
|      ! 0 | 13743 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|      ! 0 | 13744 | `		}` |
|        - | 13745 | `		/* Emit the DONE instruction */` |
|      ! 0 | 13746 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      ! 0 | 13747 | `		return SXRET_OK;` |
|        - | 13748 | `	}` |
|    53367 | 13749 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|        - | 13750 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|        - | 13751 | `		/*` |
|        - | 13752 | `		 * Shortcut syntax for the 'echo' language construct.` |
|        - | 13753 | `		 * According to the PHP reference manual:` |
|        - | 13754 | `		 *  echo() also has a shortcut syntax, where you can` |
|        - | 13755 | `		 *  immediately follow` |
|        - | 13756 | `		 *  the opening tag with an equals sign as follows:` |
|        - | 13757 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|        - | 13758 | `		 * Symisc extension:` |
|        - | 13759 | `		 *   This short syntax works with all PHP opening` |
|        - | 13760 | `		 *   tags unlike the default PHP engine that handle` |
|        - | 13761 | `		 *   only short tag.` |
|        - | 13762 | `		 */` |
|        - | 13763 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|        3 | 13764 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|        3 | 13765 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|        3 | 13766 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|        3 | 13767 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|        3 | 13768 | `		if( rc != SXERR_EMPTY ){` |
|        3 | 13769 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        1 | 13770 | `		}` |
|        3 | 13771 | `		return SXRET_OK;` |
|        - | 13772 | `	}` |
|        - | 13773 | `	/* Compile the PHP chunk */` |
|    53365 | 13774 | `	rc = GenStateCompileChunk(pGen,0);` |
|        - | 13775 | `	/* Fix exceptions jumps */` |
|    53365 | 13776 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        - | 13777 | `	/* Fix gotos now, the jump destination is resolved */` |
|    53365 | 13778 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|        3 | 13779 | `		rc = SXERR_ABORT;` |
|        1 | 13780 | `	}` |
|        - | 13781 | `	/* Reset container */` |
|    53365 | 13782 | `	SySetReset(&pGen->aGoto);` |
|    53365 | 13783 | `	SySetReset(&pGen->aLabel);` |
|    53365 | 13784 | `	SySetReset(&pGen->aNullsafeJmp);` |
|        - | 13785 | `	/* Compilation result */` |
|    53365 | 13786 | `	return rc;` |
|    26686 | 13787 | `}` |
|        - | 13788 | `/*` |
|        - | 13789 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|        - | 13790 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|        - | 13791 | ` * This is the only compile interface exported from this file.` |
|        - | 13792 | ` */` |
|    56422 | 13793 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|        - | 13794 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|        - | 13795 | `	SyString *pScript,  /* Script to compile */` |
|        - | 13796 | `	sxi32 iFlags        /* Compile flags */` |
|        - | 13797 | `	)` |
|        5 | 13798 | `{` |
|        - | 13799 | `	SySet aPhpToken,aRawToken;` |
|        - | 13800 | `	ph7_gen_state *pCodeGen;` |
|        - | 13801 | `	ph7_value *pRawObj;` |
|        - | 13802 | `	sxu32 nObjIdx;` |
|        - | 13803 | `	sxi32 nRawObj;` |
|        - | 13804 | `	int is_expr;` |
|        - | 13805 | `	sxi8 bSavedStrict;` |
|        - | 13806 | `	sxi8 bSavedStrictLocked;` |
|        - | 13807 | `	sxi32 rc;` |
|    56427 | 13808 | `	if( pScript->nByte < 1 ){` |
|        - | 13809 | `		/* Nothing to compile */` |
|      ! 0 | 13810 | `		return PH7_OK;` |
|        - | 13811 | `	}` |
|        - | 13812 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|        - | 13813 | `	 * file's flags so include/require restore them on return. */` |
|    56427 | 13814 | `	pCodeGen = &pVm->sCodeGen;` |
|    56427 | 13815 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|    56427 | 13816 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|    56427 | 13817 | `	pCodeGen->bStrictTypes = 0;` |
|    56427 | 13818 | `	pCodeGen->bStrictTypesLocked = 0;` |
|        - | 13819 | `	/* Initialize the tokens containers */` |
|    56427 | 13820 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|    56427 | 13821 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|    56427 | 13822 | `	SySetAlloc(&aPhpToken,0xc0);` |
|    56427 | 13823 | `	is_expr = 0;` |
|    56427 | 13824 | `	if( iFlags & PH7_PHP_ONLY ){` |
|        - | 13825 | `		SyToken sTmp;` |
|        - | 13826 | `		/* PHP only: -*/` |
|    42827 | 13827 | `		sTmp.nLine = 1;` |
|    42827 | 13828 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    42827 | 13829 | `		sTmp.pUserData = 0;` |
|    42827 | 13830 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    42827 | 13831 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    42827 | 13832 | `		if( iFlags & PH7_PHP_EXPR ){` |
|        - | 13833 | `			/* A simple PHP expression */` |
|      ! 0 | 13834 | `			is_expr = 1;` |
|      ! 0 | 13835 | `		}` |
|    21416 | 13836 | `	}else{` |
|        - | 13837 | `		/* Tokenize raw text */` |
|    13605 | 13838 | `		SySetAlloc(&aRawToken,32);` |
|    13605 | 13839 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|        - | 13840 | `	}` |
|        - | 13841 | `	/* Process high-level tokens */` |
|    56427 | 13842 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|    56427 | 13843 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|    56427 | 13844 | `	rc = PH7_OK;` |
|    56427 | 13845 | `	if( is_expr ){` |
|        - | 13846 | `		/* Compile the expression */` |
|      ! 0 | 13847 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|      ! 0 | 13848 | `		goto cleanup;` |
|        - | 13849 | `	}` |
|    56427 | 13850 | `	nObjIdx = 0;` |
|        - | 13851 | `	/* Each compilation unit starts in the global namespace.` |
|        - | 13852 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|        - | 13853 | `	 * preventing namespace bleeding across include()d files. */` |
|    56427 | 13854 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|        - | 13855 | `	/* Start the compilation process */` |
|    35017 | 13856 | `	for(;;){` |
|   123389 | 13857 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|    56415 | 13858 | `			break; /* No more tokens to process */` |
|        - | 13859 | `		}` |
|    66979 | 13860 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|        - | 13861 | `			/* Compile the PHP chunk */` |
|    53367 | 13862 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|    53367 | 13863 | `			if( rc == SXERR_ABORT ){` |
|       15 | 13864 | `				break;` |
|        - | 13865 | `			}` |
|    53355 | 13866 | `			continue;` |
|        - | 13867 | `		}` |
|        - | 13868 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|    13617 | 13869 | `		nRawObj = 0;` |
|    27271 | 13870 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|        - | 13871 | `			/* Consume the raw chunk without any processing */` |
|    13659 | 13872 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|    13659 | 13873 | `			if( pRawObj == 0 ){` |
|      ! 0 | 13874 | `				rc = SXERR_MEM;` |
|      ! 0 | 13875 | `				break;` |
|        - | 13876 | `			}` |
|        - | 13877 | `			/* Mark as constant and emit the load constant instruction */` |
|    13659 | 13878 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|    13659 | 13879 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|    13659 | 13880 | `			++nRawObj;` |
|    13659 | 13881 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|        5 | 13882 | `		}` |
|    13617 | 13883 | `		if( nRawObj > 0 ){` |
|        - | 13884 | `			/* Emit the consume instruction */` |
|    13617 | 13885 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     6806 | 13886 | `		}` |
|    28216 | 13887 | `	}` |
|    28211 | 13888 | `cleanup:` |
|    56427 | 13889 | `	SySetRelease(&aRawToken);` |
|    56427 | 13890 | `	SySetRelease(&aPhpToken);` |
|        - | 13891 | `	/* Restore outer file's strict_types scope */` |
|    56427 | 13892 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|    56427 | 13893 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|    56427 | 13894 | `	return rc;` |
|    28216 | 13895 | `}` |
|        - | 13896 | `/*` |
|        - | 13897 | ` * Utility routines.Initialize the code generator.` |
|        - | 13898 | ` */` |
|     3884 | 13899 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|        - | 13900 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 13901 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|        - | 13902 | `	void *pErrData     /* Last argument to xErr() */` |
|        - | 13903 | `	)` |
|        5 | 13904 | `{` |
|     3889 | 13905 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 13906 | `	/* Zero the structure */` |
|     3889 | 13907 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|        - | 13908 | `	/* Initial state */` |
|     3889 | 13909 | `	pGen->pVm  = &(*pVm);` |
|     3889 | 13910 | `	pGen->xErr = xErr;` |
|     3889 | 13911 | `	pGen->pErrData = pErrData;` |
|     3889 | 13912 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|     3889 | 13913 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|     3889 | 13914 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|     3889 | 13915 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|     3889 | 13916 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|     3889 | 13917 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|     3889 | 13918 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|        - | 13919 | `	/* Error log buffer */` |
|     3889 | 13920 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|        - | 13921 | `	/* General purpose working buffer */` |
|     3889 | 13922 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|        - | 13923 | `	/* Namespace state */` |
|     3889 | 13924 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     3889 | 13925 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|     3889 | 13926 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|     3889 | 13927 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|        - | 13928 | `	/* Create the global scope */` |
|     3889 | 13929 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|        - | 13930 | `	/* Point to the global scope */` |
|     3889 | 13931 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     3889 | 13932 | `	return SXRET_OK;` |
|        5 | 13933 | `}` |
|        - | 13934 | `/*` |
|        - | 13935 | ` * Utility routines. Reset the code generator to it's initial state.` |
|        - | 13936 | ` */` |
|    59926 | 13937 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|        - | 13938 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 13939 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|        - | 13940 | `	void *pErrData     /* Last argument to xErr() */` |
|        - | 13941 | `	)` |
|        5 | 13942 | `{` |
|    59931 | 13943 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 13944 | `	GenBlock *pBlock,*pParent;` |
|        - | 13945 | `	/* Reset state */` |
|    59931 | 13946 | `	SySetReset(&pGen->aLabel);` |
|    59931 | 13947 | `	SySetReset(&pGen->aGoto);` |
|    59931 | 13948 | `	SySetReset(&pGen->aNullsafeJmp);` |
|    59931 | 13949 | `	SySetReset(&pGen->aTrivia);` |
|    59931 | 13950 | `	SySetReset(&pGen->aPendingAttrs);` |
|    59931 | 13951 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|    59931 | 13952 | `	SyBlobRelease(&pGen->sErrBuf);` |
|    59931 | 13953 | `	SyBlobRelease(&pGen->sWorker);` |
|    59931 | 13954 | `	SyBlobRelease(&pGen->sNamespace);` |
|    59931 | 13955 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    59931 | 13956 | `	SyHashRelease(&pGen->hUseImports);` |
|    59931 | 13957 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    59931 | 13958 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|    59931 | 13959 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    59931 | 13960 | `	SyHashRelease(&pGen->hUseConstImports);` |
|    59931 | 13961 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|        - | 13962 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|        - | 13963 | `	 * They intern variable names and literal strings that are referenced by` |
|        - | 13964 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|        - | 13965 | `	 * Releasing them would either leak the interned strings or require freeing` |
|        - | 13966 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|        - | 13967 | `	 * number of unique names, which is acceptable. */` |
|        - | 13968 | `	/* Point to the global scope */` |
|    59931 | 13969 | `	pBlock = pGen->pCurrent;` |
|    59931 | 13970 | `	while( pBlock->pParent != 0 ){` |
|      ! 0 | 13971 | `		pParent = pBlock->pParent;` |
|      ! 0 | 13972 | `		GenStateFreeBlock(pBlock);` |
|      ! 0 | 13973 | `		pBlock = pParent;` |
|      ! 0 | 13974 | `	}` |
|    59931 | 13975 | `	pGen->xErr = xErr;` |
|    59931 | 13976 | `	pGen->pErrData = pErrData;` |
|    59931 | 13977 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    59931 | 13978 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|    59931 | 13979 | `	pGen->pIn = pGen->pEnd = 0;` |
|    59931 | 13980 | `	pGen->nErr = 0;` |
|    59931 | 13981 | `	return SXRET_OK;` |
|        5 | 13982 | `}` |
|        - | 13983 | `/*` |
|        - | 13984 | ` * Generate a compile-time error message.` |
|        - | 13985 | ` * If the error count limit is reached (usually 15 error message)` |
|        - | 13986 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|        - | 13987 | ` * abort compilation immediately.` |
|        - | 13988 | ` */` |
|      652 | 13989 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|        5 | 13990 | `{` |
|      657 | 13991 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|      657 | 13992 | `	const char *zErr = "Error";` |
|        - | 13993 | `	SyString *pFile;` |
|        - | 13994 | `	va_list ap;` |
|        - | 13995 | `	sxi32 rc;` |
|        - | 13996 | `	/* Reset the working buffer */` |
|      657 | 13997 | `	SyBlobReset(pWorker);` |
|        - | 13998 | `	/* Peek the processed file path if available */` |
|      657 | 13999 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|      657 | 14000 | `	if( nErrType == E_ERROR ){` |
|        - | 14001 | `		/* Increment the error counter */` |
|      543 | 14002 | `		pGen->nErr++;` |
|      543 | 14003 | `		if( pGen->nErr > 15 ){` |
|        - | 14004 | `			/* Error count limit reached */` |
|        6 | 14005 | `			if( pGen->xErr ){` |
|        6 | 14006 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|        6 | 14007 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|        6 | 14008 | `				if( pFile ){` |
|        6 | 14009 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|        2 | 14010 | `				}` |
|        6 | 14011 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|        6 | 14012 | `				if( SyBlobLength(pWorker) > 0 ){` |
|        6 | 14013 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|        2 | 14014 | `				}` |
|        2 | 14015 | `			}` |
|        - | 14016 | `			/* Abort immediately */` |
|        6 | 14017 | `			return SXERR_ABORT;` |
|        - | 14018 | `		}` |
|      267 | 14019 | `	}` |
|      653 | 14020 | `	if( pGen->xErr == 0 ){` |
|        - | 14021 | `		/* No available error consumer,return immediately */` |
|        3 | 14022 | `		return SXRET_OK;` |
|        - | 14023 | `	}` |
|      650 | 14024 | `	switch(nErrType){` |
|      536 | 14025 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|       32 | 14026 | `	case E_WARNING: zErr = "Warning";     break;` |
|       82 | 14027 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       12 | 14028 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|      ! 0 | 14029 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|      ! 0 | 14030 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|      ! 0 | 14031 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|      ! 0 | 14032 | `	default:` |
|      ! 0 | 14033 | `		break;` |
|        - | 14034 | `	}` |
|      650 | 14035 | `	rc = SXRET_OK;` |
|        - | 14036 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|      650 | 14037 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|      650 | 14038 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|      650 | 14039 | `	va_start(ap,zFormat);` |
|      650 | 14040 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|      650 | 14041 | `	va_end(ap);` |
|      650 | 14042 | `	if( pFile ){` |
|      650 | 14043 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|      323 | 14044 | `	}` |
|        - | 14045 | `	/* Append a new line */` |
|      650 | 14046 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|      650 | 14047 | `	if( SyBlobLength(pWorker) > 0 ){` |
|        - | 14048 | `		/* Consume the generated error message */` |
|      650 | 14049 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|      323 | 14050 | `	}` |
|      650 | 14051 | `	return rc;` |
|      331 | 14052 | `}` |
|        - | 14053 |  |
