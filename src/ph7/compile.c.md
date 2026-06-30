# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5697/7069 lines (80.59%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits |  Line | Source |
| ------: | ----: | :--- |
|       - |     1 | `/**` |
|       - |     2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |     3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |     4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |     5 | ` */` |
|       - |     6 | `#include "ph7int.h"` |
|       - |     7 | `/*` |
|       - |     8 | ` * This file implement a thread-safe and full-reentrant compiler for the PH7 engine.` |
|       - |     9 | ` * That is, routines defined in this file takes a stream of tokens and output` |
|       - |    10 | ` * PH7 bytecode instructions.` |
|       - |    11 | ` */` |
|       - |    12 | `/* Forward declaration */` |
|       - |    13 | `typedef struct LangConstruct LangConstruct;` |
|       - |    14 | `typedef struct JumpFixup     JumpFixup;` |
|       - |    15 | `typedef struct Label         Label;` |
|       - |    16 | `/* Block [i.e: set of statements] control flags */` |
|       - |    17 | `#define GEN_BLOCK_LOOP        0x001    /* Loop block [i.e: for,while,...] */` |
|       - |    18 | `#define GEN_BLOCK_PROTECTED   0x002    /* Protected block */` |
|       - |    19 | `#define GEN_BLOCK_COND        0x004    /* Conditional block [i.e: if(condition){} ]*/` |
|       - |    20 | `#define GEN_BLOCK_FUNC        0x008    /* Function body */` |
|       - |    21 | `#define GEN_BLOCK_GLOBAL      0x010    /* Global block (always set)*/` |
|       - |    22 | `#define GEN_BLOC_NESTED_FUNC  0x020    /* Nested function body */` |
|       - |    23 | `#define GEN_BLOCK_EXPR        0x040    /* Expression */` |
|       - |    24 | `#define GEN_BLOCK_STD         0x080    /* Standard block */` |
|       - |    25 | `#define GEN_BLOCK_EXCEPTION   0x100    /* Exception block [i.e: try{ } }*/` |
|       - |    26 | `#define GEN_BLOCK_SWITCH      0x200    /* Switch statement */` |
|       - |    27 | `/*` |
|       - |    28 | ` * Each label seen in the input is recorded in an instance` |
|       - |    29 | ` * of the following structure.` |
|       - |    30 | ` * A label is a target point [i.e: a jump destination] that is specified` |
|       - |    31 | ` * by an identifier followed by a colon.` |
|       - |    32 | ` * Example` |
|       - |    33 | ` *  LABEL:` |
|       - |    34 | ` *		echo "hello\n";` |
|       - |    35 | ` */` |
|       - |    36 | `struct Label` |
|       - |    37 |  |
|       - |    38 | `	ph7_vm_func *pFunc;  /* Compiled function where the label was declared.NULL otherwise */` |
|       - |    39 | `	sxu32 nJumpDest;     /* Jump destination */` |
|       - |    40 | `	SyString sName;      /* Label name */` |
|       - |    41 | `	sxu32 nLine;         /* Line number this label occurs */` |
|       - |    42 | `	sxu8 bRef;           /* True if the label was referenced */` |
|       - |    43 | `};` |
|       - |    44 | `/*` |
|       - |    45 | ` * Compilation of some PHP constructs such as if, for, while, the logical or` |
|       - |    46 | ` * (\|\|) and logical and (&&) operators in expressions requires the` |
|       - |    47 | ` * generation of forward jumps.` |
|       - |    48 | ` * Since the destination PC target of these jumps isn't known when the jumps` |
|       - |    49 | ` * are emitted, we record each forward jump in an instance of the following` |
|       - |    50 | ` * structure. Those jumps are fixed later when the jump destination is resolved.` |
|       - |    51 | ` */` |
|       - |    52 | `struct JumpFixup` |
|       - |    53 |  |
|       - |    54 | `	sxi32 nJumpType;     /* Jump type. Either TRUE jump, FALSE jump or Unconditional jump */` |
|       - |    55 | `	sxu32 nInstrIdx;     /* Instruction index to fix later when the jump destination is resolved. */` |
|       - |    56 | `	/* The following fields are only used by the goto statement */` |
|       - |    57 | `	SyString sLabel;    /* Label name */` |
|       - |    58 | `	ph7_vm_func *pFunc; /* Compiled function inside which the goto was emitted. NULL otherwise */` |
|       - |    59 | `	sxu32 nLine;        /* Track line number */` |
|       - |    60 | `};` |
|       - |    61 | `/*` |
|       - |    62 | ` * Each language construct is represented by an instance` |
|       - |    63 | ` * of the following structure.` |
|       - |    64 | ` */` |
|       - |    65 | `struct LangConstruct` |
|       - |    66 |  |
|       - |    67 | `	sxu32 nID;                     /* Language construct ID [i.e: PH7_TKWRD_WHILE,PH7_TKWRD_FOR,PH7_TKWRD_IF...] */` |
|       - |    68 | `	ProcLangConstruct xConstruct;  /* C function implementing the language construct */` |
|       - |    69 | `};` |
|       - |    70 | `/* Compilation flags */` |
|       - |    71 | `#define PH7_COMPILE_SINGLE_STMT 0x001 /* Compile a single statement */` |
|       - |    72 | `/* Token stream synchronization macros */` |
|       - |    73 | `#define SWAP_TOKEN_STREAM(GEN,START,END)\` |
|       - |    74 | `	pTmp  = GEN->pEnd;\` |
|       - |    75 | `	pGen->pIn  = START;\` |
|       - |    76 | `	pGen->pEnd = END` |
|       - |    77 | `#define UPDATE_TOKEN_STREAM(GEN)\` |
|       - |    78 | `	if( GEN->pIn < pTmp ){\` |
|       - |    79 | `	    GEN->pIn++;\` |
|       - |    80 | `	}\` |
|       - |    81 | `	GEN->pEnd = pTmp` |
|       - |    82 | `#define SWAP_DELIMITER(GEN,START,END)\` |
|       - |    83 | `	pTmpIn  = GEN->pIn;\` |
|       - |    84 | `	pTmpEnd = GEN->pEnd;\` |
|       - |    85 | `	GEN->pIn = START;\` |
|       - |    86 | `	GEN->pEnd = END` |
|       - |    87 | `#define RE_SWAP_DELIMITER(GEN)\` |
|       - |    88 | `	GEN->pIn  = pTmpIn;\` |
|       - |    89 | `	GEN->pEnd = pTmpEnd` |
|       - |    90 | `/* Flags related to expression compilation */` |
|       - |    91 | `#define EXPR_FLAG_LOAD_IDX_STORE    0x001 /* Set the iP2 flag when dealing with the LOAD_IDX instruction */` |
|       - |    92 | `#define EXPR_FLAG_RDONLY_LOAD       0x002 /* Read-only load, refer to the 'PH7_OP_LOAD' VM instruction for more information */` |
|       - |    93 | `#define EXPR_FLAG_COMMA_STATEMENT   0x004 /* Treat comma expression as a single statement (used by class attributes) */` |
|       - |    94 | `#define EXPR_FLAG_LOAD_IDX_ISSET    0x008 /* LOAD_IDX argument is the LHS of isset() — emit iP2=4 (offsetExists) */` |
|       - |    95 | `#define EXPR_FLAG_LOAD_IDX_UNSET    0x010 /* LOAD_IDX argument is the LHS of unset() — emit iP2=5 (offsetUnset) */` |
|       - |    96 | `#define EXPR_FLAG_LOAD_IDX_EMPTY    0x020 /* LOAD_IDX argument is the LHS of empty() — emit iP2=6 (offsetExists+offsetGet) */` |
|       - |    97 | `#define EXPR_FLAG_MEMBER_WRITE      0x040 /* Sub-tree is the write lvalue of an assignment: tag a target` |
|       - |    98 | `                                           * OP_MEMBER iP2=PH7_MEMBER_WRITE so the VM auto-creates a missing` |
|       - |    99 | ``                                           * property (e.g. `$o->arr[$k] = v`, `$o->p ??= v`). Propagated`` |
|       - |   100 | `                                           * from the precedence-18 lvalue through SUBSCRIPT to the base` |
|       - |   101 | ``                                            * member; stripped when descending into an intermediate `->` `` |
|       - |   102 | `                                           * container (the container is read, not the write target). */` |
|       - |   103 | `/* Forward declaration */` |
|       - |   104 | `static sxi32 PH7_CompileExpr(ph7_gen_state *pGen,sxi32 iFlags,sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *));` |
|       - |   105 | `/*` |
|       - |   106 | ` * Local utility routines used in the code generation phase.` |
|       - |   107 | ` */` |
|       - |   108 | `/*` |
|       - |   109 | ` * Check if the given name refer to a valid label.` |
|       - |   110 | ` * Return SXRET_OK and write a pointer to that label on success.` |
|       - |   111 | ` * Any other return value indicates no such label.` |
|       - |   112 | ` */` |
|     148 |   113 | `static sxi32 GenStateGetLabel(ph7_gen_state *pGen,SyString *pName,Label **ppOut)` |
|       5 |   114 |  |
|       - |   115 | `	Label *aLabel;` |
|       - |   116 | `	sxu32 n;` |
|       - |   117 | `	/* Perform a linear scan on the label table */` |
|     153 |   118 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|     333 |   119 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     277 |   120 | `		if( SyStringCmp(&aLabel[n].sName,pName,SyMemcmp) == 0 ){` |
|       - |   121 | `			/* Jump destination found */` |
|      97 |   122 | `			aLabel[n].bRef = TRUE;` |
|      97 |   123 | `			if( ppOut ){` |
|      97 |   124 | `				*ppOut = &aLabel[n];` |
|      46 |   125 | `			}` |
|      97 |   126 | `			return SXRET_OK;` |
|       - |   127 | `		}` |
|      92 |   128 | `	}` |
|       - |   129 | `	/* No such destination */` |
|      59 |   130 | `	return SXERR_NOTFOUND;` |
|      79 |   131 |  |
|       - |   132 | `/*` |
|       - |   133 | ` * Fetch a block that correspond to the given criteria from the stack of` |
|       - |   134 | ` * compiled blocks.` |
|       - |   135 | ` * Return a pointer to that block on success. NULL otherwise.` |
|       - |   136 | ` */` |
|    3834 |   137 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       5 |   138 |  |
|    3839 |   139 | `	GenBlock *pBlock = pCurrent;` |
|   10919 |   140 | `	for(;;){` |
|   21843 |   141 | `		if( pBlock->iFlags & iBlockType ){` |
|    3731 |   142 | `			iCount--; /* Decrement nesting level */` |
|    3731 |   143 | `			if( iCount < 1 ){` |
|       - |   144 | `				/* Block meet with the desired criteria */` |
|    3705 |   145 | `				return pBlock;` |
|       - |   146 | `			}` |
|      13 |   147 | `		}` |
|       - |   148 | `		/* Point to the upper block */` |
|   18143 |   149 | `		pBlock = pBlock->pParent;` |
|   18143 |   150 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |   151 | `			/* Forbidden */` |
|      72 |   152 | `			break;` |
|       - |   153 | `		}` |
|       5 |   154 | `	}` |
|       - |   155 | `	/* No such block */` |
|     139 |   156 | `	return 0;` |
|    1922 |   157 |  |
|       - |   158 | `/*` |
|       - |   159 | ` * Initialize a freshly allocated block instance.` |
|       - |   160 | ` */` |
|  840312 |   161 | `static void GenStateInitBlock(` |
|       - |   162 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   163 | `	GenBlock *pBlock,    /* Target block */` |
|       - |   164 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   165 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |   166 | `	void *pUserData      /* Upper layer private data */` |
|       - |   167 | `	)` |
|       5 |   168 |  |
|       - |   169 | `	/* Initialize block fields */` |
|  840317 |   170 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  840317 |   171 | `	pBlock->pUserData   = pUserData;` |
|  840317 |   172 | `	pBlock->pGen        = pGen;` |
|  840317 |   173 | `	pBlock->iFlags      = iType;` |
|  840317 |   174 | `	pBlock->pParent     = 0;` |
|  840317 |   175 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  840317 |   176 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  840317 |   177 |  |
|       - |   178 | `/*` |
|       - |   179 | ` * Allocate a new block instance.` |
|       - |   180 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |   181 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |   182 | ` * processing on failure.` |
|       - |   183 | ` */` |
|  836756 |   184 | `static sxi32 GenStateEnterBlock(` |
|       - |   185 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |   186 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   187 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |   188 | `	void *pUserData,      /* Upper layer private data */` |
|       - |   189 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |   190 | `	)` |
|       5 |   191 |  |
|       - |   192 | `	GenBlock *pBlock;` |
|       - |   193 | `	/* Allocate a new block instance */` |
|  836761 |   194 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  836761 |   195 | `	if( pBlock == 0 ){` |
|       - |   196 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |   197 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |   198 | `		 */` |
|     ! 0 |   199 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |   200 | `		/* Abort processing immediately */` |
|     ! 0 |   201 | `		return SXERR_ABORT;` |
|       - |   202 | `	}` |
|       - |   203 | `	/* Zero the structure */` |
|  836761 |   204 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  836761 |   205 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |   206 | `	/* Link to the parent block */` |
|  836761 |   207 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |   208 | `	/* Mark as the current block */` |
|  836761 |   209 | `	pGen->pCurrent = pBlock;` |
|  836761 |   210 | `	if( ppBlock ){` |
|       - |   211 | `		/* Write a pointer to the new instance */` |
|  406449 |   212 | `		*ppBlock = pBlock;` |
|  203222 |   213 | `	}` |
|  836761 |   214 | `	return SXRET_OK;` |
|  418383 |   215 |  |
|       - |   216 | `/*` |
|       - |   217 | ` * Release block fields without freeing the whole instance.` |
|       - |   218 | ` */` |
|  836748 |   219 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       5 |   220 |  |
|  836753 |   221 | `	SySetRelease(&pBlock->aPostContFix);` |
|  836753 |   222 | `	SySetRelease(&pBlock->aJumpFix);` |
|  836753 |   223 |  |
|       - |   224 | `/*` |
|       - |   225 | ` * Release a block.` |
|       - |   226 | ` */` |
|  836748 |   227 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       5 |   228 |  |
|  836753 |   229 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  836753 |   230 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |   231 | `	/* Free the instance */` |
|  836753 |   232 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  836753 |   233 |  |
|       - |   234 | `/*` |
|       - |   235 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |   236 | ` */` |
|  836748 |   237 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       5 |   238 |  |
|  836753 |   239 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  836753 |   240 | `	if( pBlock == 0 ){` |
|       - |   241 | `		/* No more block to pop */` |
|     ! 0 |   242 | `		return SXERR_EMPTY;` |
|       - |   243 | `	}` |
|       - |   244 | `	/* Point to the upper block */` |
|  836753 |   245 | `	pGen->pCurrent = pBlock->pParent;` |
|  836753 |   246 | `	if( ppBlock ){` |
|       - |   247 | `		/* Write a pointer to the popped block */` |
|     ! 0 |   248 | `		*ppBlock = pBlock;` |
|     ! 0 |   249 | `	}else{` |
|       - |   250 | `		/* Safely release the block */` |
|  836753 |   251 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |   252 | `	}` |
|  836753 |   253 | `	return SXRET_OK;` |
|  418379 |   254 |  |
|       - |   255 | `/*` |
|       - |   256 | ` * Emit a forward jump.` |
|       - |   257 | ` * Notes on forward jumps` |
|       - |   258 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|       - |   259 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|       - |   260 | ` *  generation of forward jumps.` |
|       - |   261 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|       - |   262 | ` *  are emitted, we record each forward jump in an instance of the following` |
|       - |   263 | ` *  structure. Those jumps are fixed later when the jump destination is resolved.` |
|       - |   264 | ` */` |
|  241044 |   265 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       5 |   266 |  |
|       - |   267 | `	JumpFixup sJumpFix;` |
|       - |   268 | `	sxi32 rc;` |
|       - |   269 | `	/* Init the JumpFixup structure */` |
|  241049 |   270 | `	sJumpFix.nJumpType = nJumpType;` |
|  241049 |   271 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |   272 | `	/* Insert in the jump fixup table */` |
|  241049 |   273 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  241049 |   274 | `	return rc;` |
|       5 |   275 |  |
|       - |   276 | `/*` |
|       - |   277 | ` * Fix a forward jump now the jump destination is resolved.` |
|       - |   278 | ` * Return the total number of fixed jumps.` |
|       - |   279 | ` * Notes on forward jumps:` |
|       - |   280 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|       - |   281 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|       - |   282 | ` *  generation of forward jumps.` |
|       - |   283 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|       - |   284 | ` *  are emitted, we record each forward jump in an instance of the following` |
|       - |   285 | ` *  structure.Those jumps are fixed later when the jump destination is resolved.` |
|       - |   286 | ` */` |
|  584128 |   287 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       5 |   288 |  |
|       - |   289 | `	JumpFixup *aFix;` |
|       - |   290 | `	VmInstr *pInstr;` |
|       - |   291 | `	sxu32 nFixed;` |
|       - |   292 | `	sxu32 n;` |
|       - |   293 | `	/* Point to the jump fixup table */` |
|  584133 |   294 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |   295 | `	/* Fix the desired jumps */` |
| 1055367 |   296 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  471239 |   297 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |   298 | `			/* Already fixed */` |
|  186387 |   299 | `			continue;` |
|       - |   300 | `		}` |
|  284857 |   301 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |   302 | `			/* Not of our interest */` |
|   43815 |   303 | `			continue;` |
|       - |   304 | `		}` |
|       - |   305 | `		/* Point to the instruction to fix */` |
|  241047 |   306 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  241047 |   307 | `		if( pInstr ){` |
|  241047 |   308 | `			pInstr->iP2 = nJumpDest;` |
|  241047 |   309 | `			nFixed++;` |
|       - |   310 | `			/* Mark as fixed */` |
|  241047 |   311 | `			aFix[n].nJumpType = -1;` |
|  120521 |   312 | `		}` |
|  120526 |   313 | `	}` |
|       - |   314 | `	/* Total number of fixed jumps */` |
|  584133 |   315 | `	return nFixed;` |
|       5 |   316 |  |
|       - |   317 | `/*` |
|       - |   318 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |   319 | ` * The goto statement can be used to jump to another section` |
|       - |   320 | ` * in the program.` |
|       - |   321 | ` * Refer to the routine responsible of compiling the goto` |
|       - |   322 | ` * statement for more information.` |
|       - |   323 | ` */` |
|  237158 |   324 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       5 |   325 |  |
|       - |   326 | `	JumpFixup *pJump,*aJumps;` |
|       - |   327 | `	Label *pLabel,*aLabel;` |
|       - |   328 | `	VmInstr *pInstr;` |
|       - |   329 | `	sxi32 rc;` |
|       - |   330 | `	sxu32 n;` |
|       - |   331 | `	/* Point to the goto table */` |
|  237163 |   332 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |   333 | `	/* Fix */` |
|  237309 |   334 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
|     153 |   335 | `		pJump = &aJumps[n];` |
|       - |   336 | `		/* Extract the target label */` |
|     153 |   337 | `		rc = GenStateGetLabel(&(*pGen),&pJump->sLabel,&pLabel);` |
|     153 |   338 | `		if( rc != SXRET_OK ){` |
|       - |   339 | `			/* No such label */` |
|      59 |   340 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' was referenced but not defined",&pJump->sLabel);` |
|      59 |   341 | `			if( rc == SXERR_ABORT ){` |
|       3 |   342 | `				return SXERR_ABORT;` |
|       - |   343 | `			}` |
|      57 |   344 | `			continue;` |
|       - |   345 | `		}` |
|       - |   346 | `		/* Make sure the target label is reachable */` |
|      97 |   347 | `		if( pLabel->pFunc != pJump->pFunc ){` |
|      11 |   348 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' is unreachable",&pJump->sLabel);` |
|      11 |   349 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |   350 | `				return SXERR_ABORT;` |
|       - |   351 | `			}` |
|       4 |   352 | `		}` |
|       - |   353 | `		/* Fix the jump now the destination is resolved */` |
|      97 |   354 | `		pInstr = PH7_VmGetInstr(pGen->pVm,pJump->nInstrIdx);` |
|      97 |   355 | `		if( pInstr ){` |
|      97 |   356 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|      46 |   357 | `		}` |
|      51 |   358 | `	}` |
|  237161 |   359 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  237293 |   360 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     137 |   361 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |   362 | `			/* Emit a warning */` |
|      39 |   363 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |   364 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |   365 | `		}` |
|      71 |   366 | `	}` |
|  237161 |   367 | `	return SXRET_OK;` |
|  118584 |   368 |  |
|       - |   369 | `/*` |
|       - |   370 | ` * Check if a given token value is installed in the literal table.` |
|       - |   371 | ` */` |
|  764816 |   372 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       5 |   373 |  |
|       - |   374 | `	SyHashEntry *pEntry;` |
|  764821 |   375 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  764821 |   376 | `	if( pEntry == 0 ){` |
|  344407 |   377 | `		return SXERR_NOTFOUND;` |
|       - |   378 | `	}` |
|  420419 |   379 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  420419 |   380 | `	return SXRET_OK;` |
|  382413 |   381 |  |
|       - |   382 | `/*` |
|       - |   383 | ` * Install a given constant index in the literal table.` |
|       - |   384 | ` * In order to be installed, the ph7_value must be of type string.` |
|       - |   385 | ` *` |
|       - |   386 | ` * NOTE: empty strings are deliberately omitted here.  The VM reserves a` |
|       - |   387 | ` * single shared constant for "" during initialization (pVm->nEmptyStringIdx)` |
|       - |   388 | ` * and the compiler emits a LOADC referencing that slot whenever an empty` |
|       - |   389 | ` * literal is encountered.  This keeps the literal hash from growing when` |
|       - |   390 | ` * many "" literals appear in user code.` |
|       - |   391 | ` */` |
|  344402 |   392 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       5 |   393 |  |
|  344407 |   394 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  344407 |   395 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  172201 |   396 | `	}` |
|  344407 |   397 | `	return SXRET_OK;` |
|       5 |   398 |  |
|       - |   399 | `/*` |
|       - |   400 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |   401 | ` * in the constant table.` |
|       - |   402 | ` */` |
|  124822 |   403 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       5 |   404 |  |
|       - |   405 | `	ph7_value *pObj;` |
|  124827 |   406 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |   407 | `	/* Reserve a new constant */` |
|  124827 |   408 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  124827 |   409 | `	if( pObj == 0 ){` |
|     ! 0 |   410 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   411 | `		return 0;` |
|       - |   412 | `	}` |
|  124827 |   413 | `	*pIdx = nIdx;` |
|       - |   414 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |   415 | `	 * the constant string iterals table [optimization purposes].` |
|       - |   416 | `	 */` |
|  124827 |   417 | `	return pObj;` |
|   62416 |   418 |  |
|       - |   419 | `/*` |
|       - |   420 | ` * Implementation of the PHP language constructs.` |
|       - |   421 | ` */` |
|       - |   422 | `/*` |
|       - |   423 | ` * Ensure the about-to-be-emitted CALL/NEW opcode carries a VmCallArgMap` |
|       - |   424 | ` * that reflects the caller file's strict_types mode. Returns the (possibly` |
|       - |   425 | ` * newly allocated and zero-initialized) map pointer. In weak-mode files` |
|       - |   426 | ` * this is a no-op and the caller's p3 is returned unchanged.` |
|       - |   427 | ` *` |
|       - |   428 | ` * NOTE: on allocation failure the call reverts to weak semantics rather` |
|       - |   429 | ` * than aborting compilation — out-of-memory during a map allocation is` |
|       - |   430 | ` * vanishingly unlikely and silently dropping to weak mode matches the` |
|       - |   431 | ` * surrounding callsites' zero-check fallback pattern.` |
|       - |   432 | ` */` |
|  477498 |   433 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|       5 |   434 |  |
|       - |   435 | `	VmCallArgMap *pMap;` |
|  477503 |   436 | `	if( !pGen->bStrictTypes ) return p3;` |
|      33 |   437 | `	if( p3 == 0 ){` |
|      31 |   438 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|      31 |   439 | `		if( pMap == 0 ) return 0;` |
|      31 |   440 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|      31 |   441 | `		p3 = (void *)pMap;` |
|      14 |   442 | `	}` |
|      33 |   443 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|      33 |   444 | `	return p3;` |
|  238754 |   445 |  |
|       - |   446 | `/* Forward declaration */` |
|       - |   447 | `static sxi32 GenStateCompileChunk(ph7_gen_state *pGen,sxi32 iFlags);` |
|       - |   448 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx);` |
|       - |   449 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn);` |
|       - |   450 | `/* Forward decl: union type parser is defined later in this file. */` |
|       - |   451 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |   452 | `	ph7_gen_state *pGen,` |
|       - |   453 | `	sxu32 *pnType,` |
|       - |   454 | `	SyString *pClass,` |
|       - |   455 | `	SySet *pAlts,` |
|       - |   456 | `	sxi32 *piTypeFlags,` |
|       - |   457 | `	SyString *pTypeText,` |
|       - |   458 | `	int iNullableFlag,` |
|       - |   459 | `	int iUnionFlag,` |
|       - |   460 | `	int bAllowVoid,` |
|       - |   461 | `	sxu32 nLine` |
|       - |   462 | `);` |
|       - |   463 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc);` |
|       - |   464 | `static const char * TokenTypeName(sxu32 nType);` |
|       - |   465 | `/*` |
|       - |   466 | ` * Stack-scratch size for stripping PHP 7.4 numeric separators. A typical` |
|       - |   467 | ` * literal (INT64_MAX decimal is 19 digits, binary 64-bit with per-nibble` |
|       - |   468 | ` * separators is ~80 chars) fits comfortably, so the fast path never touches` |
|       - |   469 | ` * the heap. The language itself imposes no upper bound on the length of a` |
|       - |   470 | ` * well-formed literal — the stripper falls back to a VM-allocator buffer` |
|       - |   471 | ` * for anything larger, so correctness is preserved even for pathological` |
|       - |   472 | ` * inputs like a thousand-digit number.` |
|       - |   473 | ` */` |
|       - |   474 | `#define GEN_NUM_SCRATCH 128` |
|       - |   475 | `/*` |
|       - |   476 | ` * Return TRUE if c is a valid digit for the given numeric base.` |
|       - |   477 | ` *   base 16 => SyisHex (0-9, a-f, A-F)` |
|       - |   478 | ` *   base  2 => 0 or 1` |
|       - |   479 | ` *   base 10 => SyisDigit (0-9, also used for octal literals which share the` |
|       - |   480 | ` *              decimal scan in the lexer)` |
|       - |   481 | ` */` |
|    1076 |   482 | `static int GenStateIsBaseDigit(int c, int base)` |
|       5 |   483 |  |
|    1081 |   484 | `	if( base == 16 ){ return SyisHex(c); }` |
|     982 |   485 | `	if( base == 2 ){ return c == '0' \|\| c == '1'; }` |
|     703 |   486 | `	return SyisDigit(c);` |
|     543 |   487 |  |
|       - |   488 | `/*` |
|       - |   489 | ` * Given the raw text of a numeric literal token, locate a misplaced PHP 7.4` |
|       - |   490 | ` * underscore separator so the caller can report the malformed portion with` |
|       - |   491 | ` * the exact wording PHP uses:` |
|       - |   492 | ` *` |
|       - |   493 | ` *   syntax error, unexpected identifier "X"` |
|       - |   494 | ` *` |
|       - |   495 | ` * The lexer guarantees that every underscore it consumed as a separator is` |
|       - |   496 | ` * surrounded by valid base digits; anything else sits in the trailing run` |
|       - |   497 | ` * absorbed by the lexer specifically to let this validator see and report` |
|       - |   498 | ` * it. That invariant means the malformed span is exactly [bad .. nByte) —` |
|       - |   499 | ` * no forward rescan needed.` |
|       - |   500 | ` *` |
|       - |   501 | ` * Returns 1 and fills pBadStart / pBadLen when the literal is malformed;` |
|       - |   502 | ` * returns 0 when it is well-formed.` |
|       - |   503 | ` */` |
|  125486 |   504 | `static int GenStateFindBadNumericSeparator(` |
|       - |   505 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       5 |   506 |  |
|  125491 |   507 | `	const char *z = pRaw->zString;` |
|  125491 |   508 | `	sxu32 n = pRaw->nByte;` |
|  125491 |   509 | `	int base = 10;` |
|       - |   510 | `	sxu32 i, start;` |
|  125491 |   511 | `	if( n < 2 ) return 0;` |
|   10413 |   512 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |   513 | `		base = 16;` |
|   10378 |   514 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |   515 | `		base = 2;` |
|     139 |   516 | `	}` |
|   37617 |   517 | `	for( i = 0; i < n; ++i ){` |
|   27223 |   518 | `		if( z[i] != '_' ) continue;` |
|     546 |   519 | `		if( i > 0 && i + 1 < n` |
|     543 |   520 | `			&& GenStateIsBaseDigit((unsigned char)z[i-1], base)` |
|     543 |   521 | `			&& GenStateIsBaseDigit((unsigned char)z[i+1], base) ){` |
|     533 |   522 | `			continue; /* well-placed separator */` |
|       - |   523 | `		}` |
|       - |   524 | `		/* First misplaced underscore — the lexer already absorbed the full` |
|       - |   525 | `		 * malformed tail, so it runs from here to the end of the token. */` |
|      18 |   526 | `		start = i;` |
|      23 |   527 | `		if( start > 0 && (z[start-1] == 'x' \|\| z[start-1] == 'X'` |
|      12 |   528 | `			\|\| z[start-1] == 'b' \|\| z[start-1] == 'B') ){` |
|       6 |   529 | `			start--; /* include the base letter for 0x_... / 0b_... */` |
|       2 |   530 | `		}` |
|      18 |   531 | `		*pBadStart = &z[start];` |
|      18 |   532 | `		*pBadLen = n - start;` |
|      18 |   533 | `		return 1;` |
|     ! 0 |   534 | `	}` |
|   10399 |   535 | `	return 0;` |
|   62748 |   536 |  |
|       - |   537 | `/*` |
|       - |   538 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   539 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   540 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   541 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   542 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   543 | ` * so callers can bail from the current construct).` |
|       - |   544 | ` */` |
|  125486 |   545 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       5 |   546 |  |
|  125491 |   547 | `	const char *zBad = 0;` |
|  125491 |   548 | `	sxu32 nBad = 0;` |
|       - |   549 | `	SyString sBad;` |
|       - |   550 | `	sxi32 rc;` |
|  125491 |   551 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  125477 |   552 | `		return SXRET_OK;` |
|       - |   553 | `	}` |
|      18 |   554 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      18 |   555 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |   556 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      18 |   557 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |   558 | `		return SXERR_ABORT;` |
|       - |   559 | `	}` |
|      18 |   560 | `	return SXERR_SYNTAX;` |
|   62748 |   561 |  |
|       - |   562 | `/*` |
|       - |   563 | ` * Strip PHP 7.4 numeric literal separators (underscores between digits) from` |
|       - |   564 | ` * a numeric token's text and yield a SyString suitable for the low-level` |
|       - |   565 | ` * converters (SyStrToInt64 / SyStrToReal / etc.).` |
|       - |   566 | ` *` |
|       - |   567 | ` * Fast path: if the token contains no '_', *pOut aliases pToken with no copy` |
|       - |   568 | ` * and *pzAlloc is set to NULL.` |
|       - |   569 | ` * Stack path: if the cleaned bytes fit in zScratch, they are written there` |
|       - |   570 | ` * and *pzAlloc is set to NULL.` |
|       - |   571 | ` * Heap path: for literals larger than the scratch buffer, a fresh buffer is` |
|       - |   572 | ` * allocated from pAlloc, returned via *pzAlloc, and must be released by the` |
|       - |   573 | ` * caller with SyMemBackendFree once the converter is done.` |
|       - |   574 | ` *` |
|       - |   575 | ` * Returns SXRET_OK on success, SXERR_ABORT on allocator failure (in which` |
|       - |   576 | ` * case *pOut is left untouched and the caller must not read it).` |
|       - |   577 | ` */` |
|  125472 |   578 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |   579 | `	SyMemBackend *pAlloc,` |
|       - |   580 | `	const SyString *pToken,` |
|       - |   581 | `	char *zScratch, sxu32 nScratch,` |
|       - |   582 | `	SyString *pOut, char **pzAlloc)` |
|       5 |   583 |  |
|       - |   584 | `	sxu32 i, j;` |
|  125477 |   585 | `	int hasUnderscore = 0;` |
|       - |   586 | `	char *zBuf;` |
|  125477 |   587 | `	*pzAlloc = 0;` |
|  265693 |   588 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  140473 |   589 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   70113 |   590 | `	}` |
|  125477 |   591 | `	if( !hasUnderscore ){` |
|  125225 |   592 | `		SyStringDupPtr(pOut, pToken);` |
|  125225 |   593 | `		return SXRET_OK;` |
|       - |   594 | `	}` |
|     253 |   595 | `	if( pToken->nByte <= nScratch ){` |
|     251 |   596 | `		zBuf = zScratch;` |
|     126 |   597 | `	}else{` |
|       3 |   598 | `		zBuf = (char *)SyMemBackendAlloc(pAlloc, pToken->nByte);` |
|       3 |   599 | `		if( zBuf == 0 ){` |
|     ! 0 |   600 | `			return SXERR_ABORT;` |
|       - |   601 | `		}` |
|       3 |   602 | `		*pzAlloc = zBuf;` |
|       - |   603 | `	}` |
|     253 |   604 | `	j = 0;` |
|    2895 |   605 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|    2643 |   606 | `		if( pToken->zString[i] != '_' ){ zBuf[j++] = pToken->zString[i]; }` |
|    1322 |   607 | `	}` |
|     253 |   608 | `	SyStringInitFromBuf(pOut, zBuf, j);` |
|     253 |   609 | `	return SXRET_OK;` |
|   62741 |   610 |  |
|       - |   611 | `/*` |
|       - |   612 | ` * Compile a numeric [i.e: integer or real] literal.` |
|       - |   613 | ` * Notes on the integer type.` |
|       - |   614 | ` *  According to the PHP language reference manual` |
|       - |   615 | ` *  Integers can be specified in decimal (base 10), hexadecimal (base 16), octal (base 8)` |
|       - |   616 | ` *  or binary (base 2) notation, optionally preceded by a sign (- or +).` |
|       - |   617 | ` *  To use octal notation, precede the number with a 0 (zero). To use hexadecimal` |
|       - |   618 | ` *  notation precede the number with 0x. To use binary notation precede the number with 0b.` |
|       - |   619 | ` * Symisc eXtension to the integer type.` |
|       - |   620 | ` *  PH7 introduced platform-independant 64-bit integer unlike the standard PHP engine` |
|       - |   621 | ` *  where the size of an integer is platform-dependent.That is,the size of an integer` |
|       - |   622 | ` *  is 8 bytes and the maximum integer size is 0x7FFFFFFFFFFFFFFF for all platforms` |
|       - |   623 | ` *  [i.e: either 32bit or 64bit].` |
|       - |   624 | ` *  For more information on this powerfull extension please refer to the official` |
|       - |   625 | ` *  documentation.` |
|       - |   626 | ` */` |
|  125458 |   627 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   628 |  |
|  125463 |   629 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  125463 |   630 | `	sxu32 nIdx = 0;` |
|       - |   631 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  125463 |   632 | `	char *zAlloc = 0;` |
|       - |   633 | `	SyString sNum;` |
|       - |   634 | `	sxi32 rc;` |
|   62729 |   635 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  125463 |   636 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  125463 |   637 | `	if( rc != SXRET_OK ){` |
|      14 |   638 | `		return rc;` |
|       - |   639 | `	}` |
|  188177 |   640 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   62724 |   641 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  125453 |   642 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   643 | `		return SXERR_ABORT;` |
|       - |   644 | `	}` |
|  125453 |   645 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |   646 | `		ph7_value *pObj;` |
|       - |   647 | `		sxi64 iValue;` |
|  124827 |   648 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|  124827 |   649 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  124827 |   650 | `		if( pObj == 0 ){` |
|     ! 0 |   651 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   652 | `			return SXERR_ABORT;` |
|       - |   653 | `		}` |
|  124827 |   654 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   62416 |   655 | `	}else{` |
|       - |   656 | `		/* Real number */` |
|       - |   657 | `		ph7_value *pObj;` |
|       - |   658 | `		/* Reserve a new constant */` |
|     630 |   659 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     630 |   660 | `		if( pObj == 0 ){` |
|     ! 0 |   661 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   662 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   663 | `			return SXERR_ABORT;` |
|       - |   664 | `		}` |
|     630 |   665 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|     630 |   666 | `		PH7_MemObjToReal(pObj);` |
|       - |   667 | `	}` |
|  125453 |   668 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |   669 | `	/* Emit the load constant instruction */` |
|  125453 |   670 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   671 | `	/* Node successfully compiled */` |
|  125453 |   672 | `	return SXRET_OK;` |
|   62734 |   673 |  |
|       - |   674 | `/*` |
|       - |   675 | ` * Compile a single quoted string.` |
|       - |   676 | ` * According to the PHP language reference manual:` |
|       - |   677 | ` *` |
|       - |   678 | ` *   The simplest way to specify a string is to enclose it in single quotes (the character ' ).` |
|       - |   679 | ` *   To specify a literal single quote, escape it with a backslash (\). To specify a literal` |
|       - |   680 | ` *   backslash, double it (\\). All other instances of backslash will be treated as a literal` |
|       - |   681 | ` *   backslash: this means that the other escape sequences you might be used to, such as \r` |
|       - |   682 | ` *   or \n, will be output literally as specified rather than having any special meaning.` |
|       - |   683 | ` *` |
|       - |   684 | ` */` |
|  100260 |   685 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   686 |  |
|  100265 |   687 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |   688 | `	const char *zIn,*zCur,*zEnd;` |
|       - |   689 | `	ph7_value *pObj;` |
|       - |   690 | `	sxu32 nIdx;` |
|  100265 |   691 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |   692 | `	/* Delimit the string */` |
|  100265 |   693 | `	zIn  = pStr->zString;` |
|  100265 |   694 | `	zEnd = &zIn[pStr->nByte];` |
|  100265 |   695 | `	if( zIn >= zEnd ){` |
|       - |   696 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |   697 | `		 * rather than reserving a new object each time. */` |
|    7283 |   698 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    7283 |   699 | `		return SXRET_OK;` |
|       - |   700 | `	}` |
|   92987 |   701 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |   702 | `		/* Already processed,emit the load constant instruction` |
|       - |   703 | `		 * and return.` |
|       - |   704 | `		 */` |
|   35957 |   705 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   35957 |   706 | `		return SXRET_OK;` |
|       - |   707 | `	}` |
|       - |   708 | `	/* Reserve a new constant */` |
|   57035 |   709 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   57035 |   710 | `	if( pObj == 0 ){` |
|     ! 0 |   711 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   712 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   713 | `		return SXERR_ABORT;` |
|       - |   714 | `	}` |
|   57035 |   715 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |   716 | `	/* Compile the node */` |
|   57085 |   717 | `	for(;;){` |
|  114175 |   718 | `		if( zIn >= zEnd ){` |
|       - |   719 | `			/* End of input */` |
|   57035 |   720 | `			break;` |
|       - |   721 | `		}` |
|   57145 |   722 | `		zCur = zIn;` |
|  976557 |   723 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  919417 |   724 | `			zIn++;` |
|       5 |   725 | `		}` |
|   57145 |   726 | `		if( zIn > zCur ){` |
|       - |   727 | `			/* Append raw contents*/` |
|   57121 |   728 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   28558 |   729 | `		}` |
|   57145 |   730 | `		zIn++;` |
|   57145 |   731 | `		if( zIn < zEnd ){` |
|     132 |   732 | `			if( zIn[0] == '\\' ){` |
|       - |   733 | `				/* A literal backslash */` |
|      23 |   734 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|     121 |   735 | `			}else if( zIn[0] == '\'' ){` |
|       - |   736 | `				/* A single quote */` |
|      11 |   737 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       6 |   738 | `			}else{` |
|       - |   739 | `				/* verbatim copy */` |
|     100 |   740 | `				zIn--;` |
|     100 |   741 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|     100 |   742 | `				zIn++;` |
|       - |   743 | `			}` |
|      65 |   744 | `		}` |
|       - |   745 | `		/* Advance the stream cursor */` |
|   57145 |   746 | `		zIn++;` |
|       5 |   747 | `	}` |
|       - |   748 | `	/* Emit the load constant instruction */` |
|   57035 |   749 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   57035 |   750 | `	if( pStr->nByte < 1024 ){` |
|       - |   751 | `		/* Install in the literal table */` |
|   57035 |   752 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   28515 |   753 | `	}` |
|       - |   754 | `	/* Node successfully compiled */` |
|   57035 |   755 | `	return SXRET_OK;` |
|   50135 |   756 |  |
|       - |   757 | `/*` |
|       - |   758 | ` * PHP 7.3 flexible heredoc/nowdoc closing-marker indent stripping.` |
|       - |   759 | ` *` |
|       - |   760 | ` * When the lexer matched the closing marker with leading whitespace on its` |
|       - |   761 | ` * own line, it stored the indent count in pGen->pIn->pUserData. The marker's` |
|       - |   762 | ` * indent prefix bytes sit immediately after the stripped body (at` |
|       - |   763 | ` * pIn->sData.zString + pIn->sData.nByte + 1 for LF, +2 for CRLF) in the` |
|       - |   764 | ` * original source buffer — the buffer is stable through compilation.` |
|       - |   765 | ` *` |
|       - |   766 | `` * For each body line, we remove exactly `nIndent` leading bytes that must`` |
|       - |   767 | ` * byte-for-byte match the marker's prefix. Empty lines (0 bytes or bare \r)` |
|       - |   768 | ` * bypass validation. Mismatches raise the exact PHP 7.3+ parse errors:` |
|       - |   769 | ` *   - "Invalid body indentation level (expecting an indentation level of` |
|       - |   770 | ` *     at least N)" — line too short, or first differing byte is not` |
|       - |   771 | ` *     whitespace.` |
|       - |   772 | ` *   - "Invalid indentation - tabs and spaces cannot be mixed" — first` |
|       - |   773 | ` *     differing byte is whitespace but differs from the marker prefix.` |
|       - |   774 | ` */` |
|     110 |   775 | `static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)` |
|       4 |   776 |  |
|     114 |   777 | `	SyString *pIn = &pGen->pIn->sData;` |
|     114 |   778 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - |   779 | `	const char *zPrefix;` |
|       - |   780 | `	const char *z, *zEnd;` |
|       - |   781 | `	char *zBuf, *zDst;` |
|     114 |   782 | `	if( nIndent == 0 ){` |
|       - |   783 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|      68 |   784 | `		*pOut = *pIn;` |
|      68 |   785 | `		return SXRET_OK;` |
|       - |   786 | `	}` |
|       - |   787 | `	/* Recover the marker indent prefix from the original source buffer.` |
|       - |   788 | `	 * Skip the terminator the lexer stripped: one '\n' plus an optional` |
|       - |   789 | `	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the` |
|       - |   790 | `	 * lexer stripped nothing, so this offset is one byte past the true` |
|       - |   791 | `	 * marker-indent start. That is harmless — the strip loop below never` |
|       - |   792 | `	 * runs (z == zEnd), and zPrefix is never dereferenced. */` |
|      47 |   793 | `	zPrefix = pIn->zString + pIn->nByte;` |
|      47 |   794 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|     ! 0 |   795 | `		zPrefix += 2;` |
|     ! 0 |   796 | `	}else{` |
|      47 |   797 | `		zPrefix += 1;` |
|       - |   798 | `	}` |
|       - |   799 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|      47 |   800 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|      47 |   801 | `	if( zBuf == 0 ){` |
|     ! 0 |   802 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   803 | `		return SXERR_ABORT;` |
|       - |   804 | `	}` |
|      47 |   805 | `	zDst = zBuf;` |
|      47 |   806 | `	z = pIn->zString;` |
|      47 |   807 | `	zEnd = z + pIn->nByte;` |
|     129 |   808 | `	while( z < zEnd ){` |
|      71 |   809 | `		const char *zLine = z;` |
|       - |   810 | `		sxu32 nLine;` |
|       - |   811 | `		int bEmpty;` |
|     799 |   812 | `		while( z < zEnd && z[0] != '\n' ){` |
|     731 |   813 | `			z++;` |
|       3 |   814 | `		}` |
|      71 |   815 | `		nLine = (sxu32)(z - zLine);` |
|      71 |   816 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|      71 |   817 | `		if( !bEmpty ){` |
|       - |   818 | `			sxu32 i;` |
|      67 |   819 | `			if( nLine < nIndent ){` |
|     ! 0 |   820 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   821 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|     ! 0 |   822 | `					nIndent);` |
|     ! 0 |   823 | `				return SXERR_ABORT;` |
|       - |   824 | `			}` |
|     269 |   825 | `			for( i = 0; i < nIndent; i++ ){` |
|     213 |   826 | `				if( zLine[i] != zPrefix[i] ){` |
|      10 |   827 | `					unsigned char c = (unsigned char)zLine[i];` |
|      10 |   828 | `					if( c == ' ' \|\| c == '\t' ){` |
|       5 |   829 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   830 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|       3 |   831 | `					}else{` |
|       7 |   832 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   833 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|       2 |   834 | `							nIndent);` |
|       - |   835 | `					}` |
|      10 |   836 | `					return SXERR_ABORT;` |
|       - |   837 | `				}` |
|     103 |   838 | `			}` |
|      57 |   839 | `			SyMemcpy((const void *)(zLine + nIndent), (void *)zDst, nLine - nIndent);` |
|      57 |   840 | `			zDst += nLine - nIndent;` |
|      33 |   841 | `		}else if( nLine == 1 ){` |
|       - |   842 | `			/* Preserve the stray '\r' on an otherwise empty line */` |
|     ! 0 |   843 | `			*zDst++ = '\r';` |
|     ! 0 |   844 | `		}` |
|      61 |   845 | `		if( z < zEnd ){` |
|      25 |   846 | `			*zDst++ = '\n';` |
|      25 |   847 | `			z++;` |
|      12 |   848 | `		}` |
|       1 |   849 | `	}` |
|      37 |   850 | `	pOut->zString = zBuf;` |
|      37 |   851 | `	pOut->nByte = (sxu32)(zDst - zBuf);` |
|      37 |   852 | `	return SXRET_OK;` |
|      59 |   853 |  |
|       - |   854 | `/*` |
|       - |   855 | ` * Compile a nowdoc string.` |
|       - |   856 | ` * According to the PHP language reference manual:` |
|       - |   857 | ` *` |
|       - |   858 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|       - |   859 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|       - |   860 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the` |
|       - |   861 | ` *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]>` |
|       - |   862 | ` *  construct, in that it declares a block of text which is not for parsing.` |
|       - |   863 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier` |
|       - |   864 | ` *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc` |
|       - |   865 | ` *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance` |
|       - |   866 | ` *  of the closing identifier.` |
|       - |   867 | ` */` |
|      46 |   868 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |   869 |  |
|       - |   870 | `	SyString sStripped;` |
|       - |   871 | `	SyString *pStr;` |
|       - |   872 | `	ph7_value *pObj;` |
|       - |   873 | `	sxu32 nIdx;` |
|       - |   874 | `	sxi32 rc;` |
|      50 |   875 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      50 |   876 | `	if( rc != SXRET_OK ){` |
|       6 |   877 | `		return rc;` |
|       - |   878 | `	}` |
|      44 |   879 | `	pStr = &sStripped;` |
|      44 |   880 | `	nIdx = 0; /* Prevent compiler warning */` |
|      44 |   881 | `	if( pStr->nByte <= 0 ){` |
|       - |   882 | `		/* Empty string,load NULL */` |
|       7 |   883 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |   884 | `		return SXRET_OK;` |
|       - |   885 | `	}` |
|       - |   886 | `	/* Reserve a new constant */` |
|      38 |   887 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      38 |   888 | `	if( pObj == 0 ){` |
|     ! 0 |   889 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   890 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   891 | `		return SXERR_ABORT;` |
|       - |   892 | `	}` |
|       - |   893 | `	/* No processing is done here, simply a memcpy() operation */` |
|      38 |   894 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|       - |   895 | `	/* Emit the load constant instruction */` |
|      38 |   896 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   897 | `	/* Node successfully compiled */` |
|      38 |   898 | `	return SXRET_OK;` |
|      27 |   899 |  |
|       - |   900 | `/*` |
|       - |   901 | ` * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.` |
|       - |   902 | ` * According to the PHP language reference manual` |
|       - |   903 | ` *   When a string is specified in double quotes or with heredoc,variables are parsed within it.` |
|       - |   904 | ` *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most` |
|       - |   905 | ` *  common and convenient. It provides a way to embed a variable, an array value, or an object` |
|       - |   906 | ` *  property in a string with a minimum of effort.` |
|       - |   907 | ` *  Simple syntax` |
|       - |   908 | ` *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible` |
|       - |   909 | ` *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify` |
|       - |   910 | ` *   the end of the name.` |
|       - |   911 | ` *   Similarly, an array index or an object property can be parsed. With array indices, the closing` |
|       - |   912 | ` *   square bracket (]) marks the end of the index. The same rules apply to object properties` |
|       - |   913 | ` *   as to simple variables.` |
|       - |   914 | ` *  Complex (curly) syntax` |
|       - |   915 | ` *   This isn't called complex because the syntax is complex, but because it allows for the use` |
|       - |   916 | ` *   of complex expressions.` |
|       - |   917 | ` *   Any scalar variable, array element or object property with a string representation can be` |
|       - |   918 | ` *   included via this syntax. Simply write the expression the same way as it would appear outside` |
|       - |   919 | ` *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only` |
|       - |   920 | ` *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$` |
|       - |   921 | ` */` |
|    2254 |   922 | `static sxi32 GenStateProcessStringExpression(` |
|       - |   923 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   924 | `	sxu32 nLine,         /* Line number */` |
|       - |   925 | `	const char *zIn,     /* Raw expression */` |
|       - |   926 | `	const char *zEnd     /* End of the expression */` |
|       - |   927 | `	)` |
|       5 |   928 |  |
|       - |   929 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |   930 | `	SySet sToken;` |
|       - |   931 | `	sxi32 rc;` |
|       - |   932 | `	/* Initialize the token set */` |
|    2259 |   933 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |   934 | `	/* Preallocate some slots */` |
|    2259 |   935 | `	SySetAlloc(&sToken,0x08);` |
|       - |   936 | `	/* Tokenize the text */` |
|    2259 |   937 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |   938 | `	/* Swap delimiter */` |
|    2259 |   939 | `	pTmpIn  = pGen->pIn;` |
|    2259 |   940 | `	pTmpEnd = pGen->pEnd;` |
|    2259 |   941 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    2259 |   942 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |   943 | `	/* Compile the expression */` |
|    2259 |   944 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |   945 | `	/* Restore token stream */` |
|    2259 |   946 | `	pGen->pIn  = pTmpIn;` |
|    2259 |   947 | `	pGen->pEnd = pTmpEnd;` |
|       - |   948 | `	/* Release the token set */` |
|    2259 |   949 | `	SySetRelease(&sToken);` |
|       - |   950 | `	/* Compilation result */` |
|    2259 |   951 | `	return rc;` |
|       5 |   952 |  |
|       - |   953 | `/*` |
|       - |   954 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |   955 | ` */` |
|   25128 |   956 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       5 |   957 |  |
|       - |   958 | `	ph7_value *pConstObj;` |
|   25133 |   959 | `	sxu32 nIdx = 0;` |
|       - |   960 | `	/* Reserve a new constant */` |
|   25133 |   961 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   25133 |   962 | `	if( pConstObj == 0 ){` |
|     ! 0 |   963 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   964 | `		return 0;` |
|       - |   965 | `	}` |
|   25133 |   966 | `	(*pCount)++;` |
|   25133 |   967 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |   968 | `	/* Emit the load constant instruction */` |
|   25133 |   969 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   25133 |   970 | `	return pConstObj;` |
|   12569 |   971 |  |
|       - |   972 | `/*` |
|       - |   973 | ` * Compile a double quoted/heredoc string.` |
|       - |   974 | ` * According to the PHP language reference manual` |
|       - |   975 | ` * Heredoc` |
|       - |   976 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|       - |   977 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|       - |   978 | ` *  to close the quotation.` |
|       - |   979 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|       - |   980 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|       - |   981 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|       - |   982 | ` *  Warning` |
|       - |   983 | ` *  It is very important to note that the line with the closing identifier must contain` |
|       - |   984 | ` *  no other characters, except possibly a semicolon (;). That means especially that the identifier` |
|       - |   985 | ` *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.` |
|       - |   986 | ` *  It's also important to realize that the first character before the closing identifier must` |
|       - |   987 | ` *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.` |
|       - |   988 | ` *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.` |
|       - |   989 | ` *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing` |
|       - |   990 | ` *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before` |
|       - |   991 | ` *  the end of the current file, a parse error will result at the last line.` |
|       - |   992 | ` *  Heredocs can not be used for initializing class properties.` |
|       - |   993 | ` * Double quoted` |
|       - |   994 | ` *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:` |
|       - |   995 | ` *  Escaped characters Sequence 	Meaning` |
|       - |   996 | ` *  \n linefeed (LF or 0x0A (10) in ASCII)` |
|       - |   997 | ` *  \r carriage return (CR or 0x0D (13) in ASCII)` |
|       - |   998 | ` *  \t horizontal tab (HT or 0x09 (9) in ASCII)` |
|       - |   999 | ` *  \v vertical tab (VT or 0x0B (11) in ASCII)` |
|       - |  1000 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|       - |  1001 | ` *  \\ backslash` |
|       - |  1002 | ` *  \$ dollar sign` |
|       - |  1003 | ` *  \" double-quote` |
|       - |  1004 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation` |
|       - |  1005 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|       - |  1006 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|       - |  1007 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|       - |  1008 | ` * See string parsing for details.` |
|       - |  1009 | ` */` |
|   23648 |  1010 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       5 |  1011 |  |
|   23653 |  1012 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  1013 | `	const char *zIn,*zCur,*zEnd;` |
|   23653 |  1014 | `	ph7_value *pObj = 0;` |
|       - |  1015 | `	sxi32 iCons;` |
|       - |  1016 | `	sxi32 rc;` |
|       - |  1017 | `	/* Delimit the string */` |
|   23653 |  1018 | `	zIn  = pStr->zString;` |
|   23653 |  1019 | `	zEnd = &zIn[pStr->nByte];` |
|   23653 |  1020 | `	if( zIn >= zEnd ){` |
|       - |  1021 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  1022 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  1023 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  1024 | `		 */` |
|     313 |  1025 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     313 |  1026 | `		return SXRET_OK;` |
|       - |  1027 | `	}` |
|   23345 |  1028 | `	zCur = 0;` |
|       - |  1029 | `	/* Compile the node */` |
|   23345 |  1030 | `	iCons = 0;` |
|   12797 |  1031 | `	for(;;){` |
|   38261 |  1032 | `		zCur = zIn;` |
|  178723 |  1033 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  142721 |  1034 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      67 |  1035 | `				break;` |
|  142597 |  1036 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    2134 |  1037 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|    1068 |  1038 | `					break;` |
|       - |  1039 | `			}` |
|  140467 |  1040 | `			zIn++;` |
|       5 |  1041 | `		}` |
|   38261 |  1042 | `		if( zIn > zCur ){` |
|   17771 |  1043 | `			if( pObj == 0 ){` |
|   17297 |  1044 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   17297 |  1045 | `				if( pObj == 0 ){` |
|     ! 0 |  1046 | `					return SXERR_ABORT;` |
|       - |  1047 | `				}` |
|    8646 |  1048 | `			}` |
|   17771 |  1049 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    8883 |  1050 | `		}` |
|   38261 |  1051 | `		if( zIn >= zEnd ){` |
|   23345 |  1052 | `			break;` |
|       - |  1053 | `		}` |
|   14921 |  1054 | `		if( zIn[0] == '\\' ){` |
|   12667 |  1055 | `			const char *zPtr = 0;` |
|       - |  1056 | `			sxu32 n;` |
|   12667 |  1057 | `			zIn++;` |
|   12667 |  1058 | `			if( zIn >= zEnd ){` |
|     ! 0 |  1059 | `				break;` |
|       - |  1060 | `			}` |
|   12667 |  1061 | `			if( pObj == 0 ){` |
|    7841 |  1062 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    7841 |  1063 | `				if( pObj == 0 ){` |
|     ! 0 |  1064 | `					return SXERR_ABORT;` |
|       - |  1065 | `				}` |
|    3918 |  1066 | `			}` |
|   12667 |  1067 | `			n = sizeof(char); /* size of conversion */` |
|   12667 |  1068 | `			switch( zIn[0] ){` |
|       7 |  1069 | `			case '$':` |
|       - |  1070 | `				/* Dollar sign */` |
|      15 |  1071 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|      15 |  1072 | `				break;` |
|      49 |  1073 | `			case '\\':` |
|       - |  1074 | `				/* A literal backslash */` |
|     103 |  1075 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|     103 |  1076 | `				break;` |
|       2 |  1077 | `			case 'a':` |
|       - |  1078 | `				/* The "alert" character (BEL)[ctrl+g] ASCII code 7 */` |
|       5 |  1079 | `				PH7_MemObjStringAppend(pObj,"\a",sizeof(char));` |
|       5 |  1080 | `				break;` |
|       2 |  1081 | `			case 'b':` |
|       - |  1082 | `				/* Backspace (BS)[ctrl+h] ASCII code 8 */` |
|       5 |  1083 | `				PH7_MemObjStringAppend(pObj,"\b",sizeof(char));` |
|       5 |  1084 | `				break;` |
|       4 |  1085 | `			case 'f':` |
|       - |  1086 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|       9 |  1087 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|       9 |  1088 | `				break;` |
|    5848 |  1089 | `			case 'n':` |
|       - |  1090 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|   11701 |  1091 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|   11701 |  1092 | `				break;` |
|      19 |  1093 | `			case 'r':` |
|       - |  1094 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|      43 |  1095 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|      43 |  1096 | `				break;` |
|      24 |  1097 | `			case 't':` |
|       - |  1098 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      53 |  1099 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      53 |  1100 | `				break;` |
|       3 |  1101 | `			case 'v':` |
|       - |  1102 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|       7 |  1103 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|       7 |  1104 | `				break;` |
|       1 |  1105 | `			case '\'':` |
|       - |  1106 | `				/* Single quote */` |
|       3 |  1107 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       3 |  1108 | `				break;` |
|     108 |  1109 | `			case '"':` |
|       - |  1110 | `				/* Double quote */` |
|     221 |  1111 | `				PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|     221 |  1112 | `				break;` |
|      10 |  1113 | `			case '0':` |
|       - |  1114 | `				/* NUL byte */` |
|      21 |  1115 | `				PH7_MemObjStringAppend(pObj,"\0",sizeof(char));` |
|      21 |  1116 | `				break;` |
|     228 |  1117 | `			case 'x':` |
|     457 |  1118 | `				if((unsigned char)zIn[1] < 0xc0 && SyisHex(zIn[1]) ){` |
|       - |  1119 | `					int c;` |
|       - |  1120 | `					/* Hex digit */` |
|     443 |  1121 | `					c = SyHexToint(zIn[1]) << 4;` |
|     443 |  1122 | `					if( &zIn[2] < zEnd ){` |
|     443 |  1123 | `						c +=  SyHexToint(zIn[2]);` |
|     221 |  1124 | `					}` |
|       - |  1125 | `					/* Output char */` |
|     443 |  1126 | `					PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|     443 |  1127 | `					n += sizeof(char) * 2;` |
|     222 |  1128 | `				}else{` |
|       - |  1129 | `					/* Output literal character  */` |
|      15 |  1130 | `					PH7_MemObjStringAppend(pObj,"x",sizeof(char));` |
|       - |  1131 | `				}` |
|     457 |  1132 | `				break;` |
|      15 |  1133 | `			case 'o':` |
|      31 |  1134 | `				if( &zIn[1] < zEnd && (unsigned char)zIn[1] < 0xc0 && SyisDigit(zIn[1]) && (zIn[1] - '0') < 8 ){` |
|       - |  1135 | `					/* Octal digit stream */` |
|       - |  1136 | `					int c;` |
|      21 |  1137 | `					c = 0;` |
|      21 |  1138 | `					zIn++;` |
|      61 |  1139 | `					for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|      55 |  1140 | `						if( zPtr >= zEnd \|\| (unsigned char)zPtr[0] >= 0xc0 \|\| !SyisDigit(zPtr[0]) \|\| (zPtr[0] - '0') > 7 ){` |
|       8 |  1141 | `							break;` |
|       - |  1142 | `						}` |
|      41 |  1143 | `						c = c * 8 + (zPtr[0] - '0');` |
|      21 |  1144 | `					}` |
|      21 |  1145 | `					if ( c > 0 ){` |
|      15 |  1146 | `						PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|       7 |  1147 | `					}` |
|      21 |  1148 | `					n = (sxu32)(zPtr-zIn);` |
|      11 |  1149 | `				}else{` |
|       - |  1150 | `					/* Output literal character  */` |
|      11 |  1151 | `					PH7_MemObjStringAppend(pObj,"o",sizeof(char));` |
|       - |  1152 | `				}` |
|      31 |  1153 | `				break;` |
|      11 |  1154 | `			default:` |
|       - |  1155 | `				/* Output without a slash */` |
|      23 |  1156 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char));` |
|      22 |  1157 | `				break;` |
|       - |  1158 | `			}` |
|       - |  1159 | `			/* Advance the stream cursor */` |
|   12667 |  1160 | `			zIn += n;` |
|   12667 |  1161 | `			continue;` |
|       - |  1162 | `		}` |
|    2259 |  1163 | `		if( zIn[0] == '{' ){` |
|       - |  1164 | `			/* Curly syntax */` |
|       - |  1165 | `			const char *zExpr;` |
|     131 |  1166 | `			sxi32 iNest = 1;` |
|     131 |  1167 | `			zIn++;` |
|     131 |  1168 | `			zExpr = zIn;` |
|       - |  1169 | `			/* Synchronize with the next closing curly braces */` |
|    1359 |  1170 | `			while( zIn < zEnd ){` |
|    1359 |  1171 | `				if( zIn[0] == '{' ){` |
|       - |  1172 | `					/* Increment nesting level */` |
|       9 |  1173 | `					iNest++;` |
|    1355 |  1174 | `				}else if(zIn[0] == '}' ){` |
|       - |  1175 | `					/* Decrement nesting level */` |
|     139 |  1176 | `					iNest--;` |
|     139 |  1177 | `					if( iNest <= 0 ){` |
|     131 |  1178 | `						break;` |
|       - |  1179 | `					}` |
|       4 |  1180 | `				}` |
|    1231 |  1181 | `				zIn++;` |
|       3 |  1182 | `			}` |
|       - |  1183 | `			/* Process the expression */` |
|     131 |  1184 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     131 |  1185 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1186 | `				return SXERR_ABORT;` |
|       - |  1187 | `			}` |
|     131 |  1188 | `			if( rc != SXERR_EMPTY ){` |
|     131 |  1189 | `				++iCons;` |
|      64 |  1190 | `			}` |
|     131 |  1191 | `			if( zIn < zEnd ){` |
|       - |  1192 | `				/* Jump the trailing curly */` |
|     131 |  1193 | `				zIn++;` |
|      64 |  1194 | `			}` |
|      67 |  1195 | `		}else{` |
|       - |  1196 | `			/* Simple syntax */` |
|    2131 |  1197 | `			const char *zExpr = zIn;` |
|       - |  1198 | `			/* Assemble variable name */` |
|    1073 |  1199 | `			for(;;){` |
|       - |  1200 | `				/* Jump leading dollars */` |
|    4277 |  1201 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    2131 |  1202 | `					zIn++;` |
|       5 |  1203 | `				}` |
|    1073 |  1204 | `				for(;;){` |
|   11874 |  1205 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    8655 |  1206 | `						zIn++;` |
|       5 |  1207 | `					}` |
|    2151 |  1208 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  1209 | `						/* UTF-8 stream */` |
|     ! 0 |  1210 | `						zIn++;` |
|     ! 0 |  1211 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1212 | `							zIn++;` |
|     ! 0 |  1213 | `						}` |
|     ! 0 |  1214 | `						continue;` |
|       - |  1215 | `					}` |
|    2151 |  1216 | `					break;` |
|     ! 0 |  1217 | `				}` |
|    2151 |  1218 | `				if( zIn >= zEnd ){` |
|     211 |  1219 | `					break;` |
|       - |  1220 | `				}` |
|    1945 |  1221 | `				if( zIn[0] == '[' ){` |
|      12 |  1222 | `					sxi32 iSquare = 1;` |
|      12 |  1223 | `					zIn++;` |
|      28 |  1224 | `					while( zIn < zEnd ){` |
|      28 |  1225 | `						if( zIn[0] == '[' ){` |
|     ! 0 |  1226 | `							iSquare++;` |
|      28 |  1227 | `						}else if (zIn[0] == ']' ){` |
|      12 |  1228 | `							iSquare--;` |
|      12 |  1229 | `							if( iSquare <= 0 ){` |
|      12 |  1230 | `								break;` |
|       - |  1231 | `							}` |
|     ! 0 |  1232 | `						}` |
|      18 |  1233 | `						zIn++;` |
|       2 |  1234 | `					}` |
|      12 |  1235 | `					if( zIn < zEnd ){` |
|      12 |  1236 | `						zIn++;` |
|       5 |  1237 | `					}` |
|      12 |  1238 | `					break;` |
|    1935 |  1239 | `				}else if(zIn[0] == '{' ){` |
|       6 |  1240 | `					sxi32 iCurly = 1;` |
|       6 |  1241 | `					zIn++;` |
|      18 |  1242 | `					while( zIn < zEnd ){` |
|      16 |  1243 | `						if( zIn[0] == '{' ){` |
|     ! 0 |  1244 | `							iCurly++;` |
|      16 |  1245 | `						}else if (zIn[0] == '}' ){` |
|       3 |  1246 | `							iCurly--;` |
|       3 |  1247 | `							if( iCurly <= 0 ){` |
|       3 |  1248 | `								break;` |
|       - |  1249 | `							}` |
|     ! 0 |  1250 | `						}` |
|      14 |  1251 | `						zIn++;` |
|       2 |  1252 | `					}` |
|       6 |  1253 | `					if( zIn < zEnd ){` |
|       3 |  1254 | `						zIn++;` |
|       1 |  1255 | `					}` |
|       6 |  1256 | `					break;` |
|    1931 |  1257 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  1258 | `					/* Member access operator '->' */` |
|      23 |  1259 | `					zIn += 2;` |
|    1921 |  1260 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  1261 | `					/* Static member access operator '::' */` |
|     ! 0 |  1262 | `					zIn += 2;` |
|     ! 0 |  1263 | `				}else{` |
|     958 |  1264 | `					break;` |
|       - |  1265 | `				}` |
|       3 |  1266 | `			}` |
|       - |  1267 | `			/* Process the expression */` |
|    2131 |  1268 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    2131 |  1269 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1270 | `				return SXERR_ABORT;` |
|       - |  1271 | `			}` |
|    2131 |  1272 | `			if( rc != SXERR_EMPTY ){` |
|    2129 |  1273 | `				++iCons;` |
|    1062 |  1274 | `			}` |
|       - |  1275 | `		}` |
|       - |  1276 | `		/* Invalidate the previously used constant */` |
|    2259 |  1277 | `		pObj = 0;` |
|       5 |  1278 | `	}/*for(;;)*/` |
|   23345 |  1279 | `	if( iCons > 1 ){` |
|       - |  1280 | `		/* Concatenate all compiled constants */` |
|    1675 |  1281 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     835 |  1282 | `	}` |
|       - |  1283 | `	/* Node successfully compiled */` |
|   23345 |  1284 | `	return SXRET_OK;` |
|   11829 |  1285 |  |
|       - |  1286 | `/*` |
|       - |  1287 | ` * Compile a double quoted string.` |
|       - |  1288 | ` *  See the block-comment above for more information.` |
|       - |  1289 | ` */` |
|   23588 |  1290 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1291 |  |
|       - |  1292 | `	sxi32 rc;` |
|   23593 |  1293 | `	rc = GenStateCompileString(&(*pGen));` |
|   11794 |  1294 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1295 | `	/* Compilation result */` |
|   23593 |  1296 | `	return rc;` |
|       5 |  1297 |  |
|       - |  1298 | `/*` |
|       - |  1299 | ` * Compile a Heredoc string.` |
|       - |  1300 | ` *  See the block-comment above for more information.` |
|       - |  1301 | ` */` |
|      64 |  1302 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  1303 |  |
|       - |  1304 | `	SyString sOrig, sStripped;` |
|       - |  1305 | `	sxi32 rc;` |
|      68 |  1306 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      68 |  1307 | `	if( rc != SXRET_OK ){` |
|       6 |  1308 | `		return rc;` |
|       - |  1309 | `	}` |
|       - |  1310 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|       - |  1311 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|       - |  1312 | `	 * Restore before returning so downstream code that references pIn is` |
|       - |  1313 | `	 * unaffected, including on the error path. */` |
|      62 |  1314 | `	sOrig = pGen->pIn->sData;` |
|      62 |  1315 | `	pGen->pIn->sData = sStripped;` |
|      62 |  1316 | `	rc = GenStateCompileString(&(*pGen));` |
|      62 |  1317 | `	pGen->pIn->sData = sOrig;` |
|      30 |  1318 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|      62 |  1319 | `	return rc;` |
|      36 |  1320 |  |
|       - |  1321 | `/*` |
|       - |  1322 | ` * Compile an array entry whether it is a key or a value.` |
|       - |  1323 | ` *  Notes on array entries.` |
|       - |  1324 | ` *  According to the PHP language reference manual` |
|       - |  1325 | ` *  An array can be created by the array() language construct.` |
|       - |  1326 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|       - |  1327 | ` *  array(  key =>  value` |
|       - |  1328 | ` *    , ...` |
|       - |  1329 | ` *    )` |
|       - |  1330 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|       - |  1331 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|       - |  1332 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|       - |  1333 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|       - |  1334 | ` *  contain integer and string indices.` |
|       - |  1335 | ` *  A value can be any PHP type.` |
|       - |  1336 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|       - |  1337 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|       - |  1338 | ` *  is specified, that value will be overwritten.` |
|       - |  1339 | ` */` |
|   21750 |  1340 | `static sxi32 GenStateCompileArrayEntry(` |
|       - |  1341 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  1342 | `	SyToken *pIn,        /* Token stream */` |
|       - |  1343 | `	SyToken *pEnd,       /* End of the token stream */` |
|       - |  1344 | `	sxi32 iFlags,        /* Compilation flags */` |
|       - |  1345 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|       - |  1346 | `	)` |
|       5 |  1347 |  |
|       - |  1348 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  1349 | `	sxi32 rc;` |
|       - |  1350 | `	/* Swap token stream */` |
|   21755 |  1351 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - |  1352 | `	/* Compile the expression*/` |
|   21755 |  1353 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - |  1354 | `	/* Restore token stream */` |
|   21755 |  1355 | `	RE_SWAP_DELIMITER(pGen);` |
|   21755 |  1356 | `	return rc;` |
|       5 |  1357 |  |
|       - |  1358 | `/*` |
|       - |  1359 | ` * Expression tree validator callback for the 'array' language construct.` |
|       - |  1360 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1361 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1362 | ` * error message.` |
|       - |  1363 | ` * See the routine responible of compiling the array language construct` |
|       - |  1364 | ` * for more inforation.` |
|       - |  1365 | ` */` |
|      36 |  1366 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  1367 |  |
|      41 |  1368 | `	sxi32 rc = SXRET_OK;` |
|      41 |  1369 | `	if( pRoot->pOp ){` |
|      14 |  1370 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|      12 |  1371 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|      17 |  1372 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - |  1373 | `			/* Unexpected expression */` |
|      14 |  1374 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1375 | `				"array(): Expecting a variable/array member/function call after reference operator '&'");` |
|      14 |  1376 | `			if( rc != SXERR_ABORT ){` |
|      14 |  1377 | `				rc = SXERR_INVALID;` |
|       5 |  1378 | `			}` |
|      10 |  1379 | `		}` |
|      31 |  1380 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1381 | `		/* Unexpected expression */` |
|       3 |  1382 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1383 | `			"array(): Expecting a variable after reference operator '&'");` |
|       3 |  1384 | `		if( rc != SXERR_ABORT ){` |
|       3 |  1385 | `			rc = SXERR_INVALID;` |
|       1 |  1386 | `		}` |
|       1 |  1387 | `	}` |
|      41 |  1388 | `	return rc;` |
|       5 |  1389 |  |
|       - |  1390 | `/*` |
|       - |  1391 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|       - |  1392 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|       - |  1393 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|       - |  1394 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|       - |  1395 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|       - |  1396 | ` */` |
|   24094 |  1397 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|       5 |  1398 |  |
|   24099 |  1399 | `	SyToken *pCur = pStart;` |
|   24099 |  1400 | `	sxi32 iNest = 0;` |
|   68179 |  1401 | `	while( pCur < pEnd ){` |
|   49561 |  1402 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    5477 |  1403 | `			return pCur;` |
|       - |  1404 | `		}` |
|       - |  1405 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - |  1406 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|       - |  1407 | `		 * not an entry separator. Skip past the signature.` |
|       - |  1408 | `		 */` |
|   44089 |  1409 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|      95 |  1410 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|      95 |  1411 | `			SyToken *pFn = pCur;` |
|      92 |  1412 | `			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd` |
|     ! 0 |  1413 | `				&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|       3 |  1414 | `				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  1415 | `				pFn = &pCur[1];` |
|     ! 0 |  1416 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  1417 | `			}` |
|      95 |  1418 | `			if( nKw == PH7_TKWRD_FN ){` |
|       5 |  1419 | `				pCur = pFn + 1; /* past 'fn' */` |
|       5 |  1420 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  1421 | `					pCur++;` |
|     ! 0 |  1422 | `				}` |
|       5 |  1423 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|       5 |  1424 | `					pCur++;` |
|       5 |  1425 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - |  1426 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       5 |  1427 | `					if( pCur < pEnd ){` |
|       5 |  1428 | `						pCur++;` |
|       2 |  1429 | `					}` |
|       2 |  1430 | `				}` |
|       5 |  1431 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_COLON) ){` |
|     ! 0 |  1432 | `					pCur++;` |
|     ! 0 |  1433 | `					if( pCur < pEnd && (pCur->nType & PH7_TK_OP)` |
|     ! 0 |  1434 | `						&& pCur->sData.nByte == 1` |
|     ! 0 |  1435 | `						&& pCur->sData.zString[0] == '?' ){` |
|     ! 0 |  1436 | `						pCur++;` |
|     ! 0 |  1437 | `					}` |
|     ! 0 |  1438 | `					if( pCur < pEnd` |
|     ! 0 |  1439 | `						&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  1440 | `						pCur++;` |
|     ! 0 |  1441 | `					}` |
|     ! 0 |  1442 | `				}` |
|       - |  1443 | `				/* The rest of the entry is the arrow-function body — no outer` |
|       - |  1444 | `				 * key to extract. */` |
|       5 |  1445 | `				return pEnd;` |
|       - |  1446 | `			}` |
|       - |  1447 | `			/* Match expression (PHP 8.0): the '=>' inside match arms is not an` |
|       - |  1448 | `			 * entry separator. Skip past the full match span. */` |
|      91 |  1449 | `			if( nKw == PH7_TKWRD_MATCH ){` |
|       3 |  1450 | `				pCur++; /* past 'match' */` |
|       3 |  1451 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|       3 |  1452 | `					pCur++;` |
|       3 |  1453 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - |  1454 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       3 |  1455 | `					if( pCur < pEnd ){` |
|       3 |  1456 | `						pCur++;` |
|       1 |  1457 | `					}` |
|       1 |  1458 | `				}` |
|       3 |  1459 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_OCB) ){` |
|       3 |  1460 | `					pCur++;` |
|       3 |  1461 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - |  1462 | `						PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|       3 |  1463 | `					if( pCur < pEnd ){` |
|       3 |  1464 | `						pCur++;` |
|       1 |  1465 | `					}` |
|       1 |  1466 | `				}` |
|       3 |  1467 | `				continue;` |
|       - |  1468 | `			}` |
|      43 |  1469 | `		}` |
|   44083 |  1470 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     325 |  1471 | `			iNest++;` |
|   43922 |  1472 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|       - |  1473 | `			/* Don't worry about mismatched brackets here, the expression` |
|       - |  1474 | `			 * parser will shortly detect any syntax error. */` |
|     325 |  1475 | `			iNest--;` |
|     161 |  1476 | `		}` |
|   44083 |  1477 | `		pCur++;` |
|       5 |  1478 | `	}` |
|   18623 |  1479 | `	return pEnd;` |
|   12052 |  1480 |  |
|       - |  1481 | `/*` |
|       - |  1482 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - |  1483 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - |  1484 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - |  1485 | ` */` |
|   31264 |  1486 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       5 |  1487 |  |
|       - |  1488 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - |  1489 | `	SyToken *pKey,*pCur;` |
|   31269 |  1490 | `	sxi32 iEmitRef = 0;` |
|   31269 |  1491 | `	sxi32 iSpread = 0;` |
|   31269 |  1492 | `	sxi32 nPair = 0;` |
|       - |  1493 | `	sxi32 rc;` |
|   31269 |  1494 | `	xValidator = 0;` |
|   25591 |  1495 | `	for(;;){` |
|       - |  1496 | `		/* Jump leading commas */` |
|   58063 |  1497 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    6881 |  1498 | `			pGen->pIn++;` |
|       5 |  1499 | `		}` |
|   51187 |  1500 | `		pCur = pGen->pIn;` |
|   51187 |  1501 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - |  1502 | `			/* No more entry to process */` |
|   31253 |  1503 | `			break;` |
|       - |  1504 | `		}` |
|   19939 |  1505 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 |  1506 | `			continue;` |
|       - |  1507 | `		}` |
|       - |  1508 | `		/* Compile the key if available */` |
|   19939 |  1509 | `		pKey = pCur;` |
|   19939 |  1510 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   19939 |  1511 | `		rc = SXERR_EMPTY;` |
|   19939 |  1512 | `		if( pCur < pGen->pIn ){` |
|    1641 |  1513 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - |  1514 | `				/* Missing value */` |
|      13 |  1515 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      13 |  1516 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1517 | `					return SXERR_ABORT;` |
|       - |  1518 | `				}` |
|      13 |  1519 | `				return SXRET_OK;` |
|       - |  1520 | `			}` |
|       - |  1521 | `			/* Compile the expression holding the key */` |
|    1631 |  1522 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - |  1523 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1631 |  1524 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1525 | `				return SXERR_ABORT;` |
|       - |  1526 | `			}` |
|    1631 |  1527 | `			pCur++; /* Jump the '=>' operator */` |
|   19116 |  1528 | `		}else if( pKey == pCur ){` |
|       - |  1529 | `			/* Key is omitted,emit a warning */` |
|     ! 0 |  1530 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 |  1531 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 |  1532 | `		}else{` |
|       - |  1533 | `			/* Reset back the cursor and point to the entry value */` |
|   18303 |  1534 | `			pCur = pKey;` |
|       - |  1535 | `		}` |
|   19929 |  1536 | `		if( rc == SXERR_EMPTY ){` |
|       - |  1537 | `			/* No available key,load NULL */` |
|   18305 |  1538 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    9150 |  1539 | `		}` |
|   19929 |  1540 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - |  1541 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      45 |  1542 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      45 |  1543 | `			iEmitRef = 1;` |
|      45 |  1544 | `			pCur++; /* Jump the '&' token */` |
|      45 |  1545 | `			if( pCur >= pGen->pIn ){` |
|       - |  1546 | `				/* Missing value */` |
|       3 |  1547 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|       3 |  1548 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1549 | `					return SXERR_ABORT;` |
|       - |  1550 | `				}` |
|       3 |  1551 | `				return SXRET_OK;` |
|       - |  1552 | `			}` |
|      19 |  1553 | `		}` |
|       - |  1554 | `		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with` |
|       - |  1555 | `		 * string-key support since PHP 8.1). The parser strips the '...' inside` |
|       - |  1556 | `		 * ExprExtractNode; we only need to know it's there so we can emit` |
|       - |  1557 | `		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the` |
|       - |  1558 | `		 * resulting hashmap rather than insert it as a scalar entry. */` |
|   19927 |  1559 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   19927 |  1560 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
|       - |  1561 | `			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the` |
|       - |  1562 | `			 * '...' token cannot follow either '=>' or '&' inside an array` |
|       - |  1563 | `			 * literal. Emit the same Parse-error wording PHP uses so the` |
|       - |  1564 | `			 * output is engine-portable. */` |
|       6 |  1565 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|       - |  1566 | `				"syntax error, unexpected token \"...\"");` |
|       6 |  1567 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1568 | `				return SXERR_ABORT;` |
|       - |  1569 | `			}` |
|       6 |  1570 | `			return SXRET_OK;` |
|       - |  1571 | `		}` |
|       - |  1572 | `		/* Compile indice value */` |
|   19923 |  1573 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   19923 |  1574 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1575 | `			return SXERR_ABORT;` |
|       - |  1576 | `		}` |
|   19923 |  1577 | `		if( iSpread ){` |
|       - |  1578 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|      64 |  1579 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   19892 |  1580 | `		}else if( iEmitRef ){` |
|       - |  1581 | `			/* Emit the load reference instruction */` |
|      41 |  1582 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      18 |  1583 | `		}` |
|   19923 |  1584 | `		xValidator = 0;` |
|   19923 |  1585 | `		iEmitRef = 0;` |
|   19923 |  1586 | `		iSpread = 0;` |
|   19923 |  1587 | `		nPair++;` |
|       5 |  1588 | `	}` |
|       - |  1589 | `	/* Emit the load map instruction */` |
|   31253 |  1590 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - |  1591 | `	/* Node successfully compiled */` |
|   31253 |  1592 | `	return SXRET_OK;` |
|   15637 |  1593 |  |
|       - |  1594 | `/*` |
|       - |  1595 | ` * Compile the 'array' language construct.` |
|       - |  1596 | ` *	 According to the PHP language reference manual` |
|       - |  1597 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - |  1598 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - |  1599 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - |  1600 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - |  1601 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - |  1602 | ` */` |
|   30256 |  1603 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1604 |  |
|       - |  1605 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   30261 |  1606 | `	pGen->pIn += 2;` |
|   30261 |  1607 | `	pGen->pEnd--;` |
|   15128 |  1608 | `	SXUNUSED(iCompileFlag);` |
|   30261 |  1609 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1610 |  |
|       - |  1611 | `/*` |
|       - |  1612 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - |  1613 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - |  1614 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - |  1615 | ` */` |
|    1008 |  1616 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1617 |  |
|       - |  1618 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|    1013 |  1619 | `	pGen->pIn++;` |
|    1013 |  1620 | `	pGen->pEnd--;` |
|     504 |  1621 | `	SXUNUSED(iCompileFlag);` |
|    1013 |  1622 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1623 |  |
|       - |  1624 | `/*` |
|       - |  1625 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - |  1626 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1627 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1628 | ` * error message.` |
|       - |  1629 | ` * See the routine responible of compiling the list language construct` |
|       - |  1630 | ` * for more inforation.` |
|       - |  1631 | ` */` |
|     164 |  1632 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       4 |  1633 |  |
|     168 |  1634 | `	sxi32 rc = SXRET_OK;` |
|     168 |  1635 | `	if( pRoot->pOp ){` |
|       4 |  1636 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|       2 |  1637 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - |  1638 | `				/* Unexpected expression */` |
|     ! 0 |  1639 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1640 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 |  1641 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 |  1642 | `					rc = SXERR_INVALID;` |
|     ! 0 |  1643 | `				}` |
|       1 |  1644 | `		}` |
|     166 |  1645 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1646 | `		/* Unexpected expression */` |
|       6 |  1647 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1648 | `			"list(): Expecting a variable not an expression");` |
|       6 |  1649 | `		if( rc != SXERR_ABORT ){` |
|       6 |  1650 | `			rc = SXERR_INVALID;` |
|       2 |  1651 | `		}` |
|       2 |  1652 | `	}` |
|     168 |  1653 | `	return rc;` |
|       4 |  1654 |  |
|       - |  1655 | `/*` |
|       - |  1656 | ` * Compile the 'list' language construct.` |
|       - |  1657 | ` *  According to the PHP language reference` |
|       - |  1658 | ` *  list(): Assign variables as if they were an array.` |
|       - |  1659 | ` *  list() is used to assign a list of variables in one operation.` |
|       - |  1660 | ` *  Description` |
|       - |  1661 | ` *   array list (mixed $varname [, mixed $... ] )` |
|       - |  1662 | ` *   Like array(), this is not really a function, but a language construct.` |
|       - |  1663 | ` *   list() is used to assign a list of variables in one operation.` |
|       - |  1664 | ` *  Parameters` |
|       - |  1665 | ` *   $varname: A variable.` |
|       - |  1666 | ` *  Return Values` |
|       - |  1667 | ` *   The assigned array.` |
|       - |  1668 | ` */` |
|       - |  1669 | `/* Nested list entry recorded during first pass of list body compilation */` |
|       - |  1670 | `struct NestedListEntry {` |
|       - |  1671 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|       - |  1672 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|       - |  1673 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|       - |  1674 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|       - |  1675 | `};` |
|       - |  1676 | `/*` |
|       - |  1677 | ` * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where` |
|       - |  1678 | `` * every entry has the form `keyExpr => target`. The source array is on the stack`` |
|       - |  1679 | ` * top on entry and remains there on exit, mirroring the positional LOAD_LIST` |
|       - |  1680 | ` * path so the caller's teardown is unchanged. For each entry: DUP the source,` |
|       - |  1681 | ` * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,` |
|       - |  1682 | ` * like a normal subscript read), then assign the fetched value to the target — a` |
|       - |  1683 | ` * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a` |
|       - |  1684 | ` * normal assignment (the value sits below the lvalue-load, exactly as in` |
|       - |  1685 | ` * GenStateEmitExprCode where the assignment RHS precedes the LHS load).` |
|       - |  1686 | ` */` |
|      28 |  1687 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|       2 |  1688 |  |
|       - |  1689 | `	SyToken *pNext;` |
|       - |  1690 | `	sxi32 rc;` |
|      66 |  1691 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       - |  1692 | `		SyToken *pArrow,*pTarget;` |
|       - |  1693 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|      38 |  1694 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|      38 |  1695 | `		pTarget = &pArrow[1];` |
|      38 |  1696 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|       - |  1697 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|       - |  1698 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|     ! 0 |  1699 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1700 | `				"Cannot use empty array entries in keyed array assignment");` |
|     ! 0 |  1701 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1702 | `		}` |
|       - |  1703 | `		/* DUP the source array (it is on the stack top) */` |
|      38 |  1704 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - |  1705 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|      38 |  1706 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|      38 |  1707 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1708 | `			return SXERR_ABORT;` |
|       - |  1709 | `		}` |
|       - |  1710 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|       - |  1711 | `		 * iP2=7 is the keyed-destructuring read context: an array source reads like` |
|       - |  1712 | ``		 * iP2=0 (missing key loads NULL silently, matching a normal `$arr[$k]` read;`` |
|       - |  1713 | `		 * PHP also emits an "Undefined array key" warning here, PHL omits it — §3.7),` |
|       - |  1714 | `		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"` |
|       - |  1715 | `		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */` |
|      38 |  1716 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);` |
|      38 |  1717 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|      34 |  1718 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|      18 |  1719 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|       - |  1720 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|       - |  1721 | `			 * Treat source[key] as the inner body's source, then drop the` |
|       - |  1722 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|       5 |  1723 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|       5 |  1724 | `			SyToken *pSavedIn = pGen->pIn;` |
|       5 |  1725 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       5 |  1726 | `			pGen->pIn = pTarget;` |
|       5 |  1727 | `			pGen->pEnd = pNext;` |
|       5 |  1728 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|       2 |  1729 | `			             : PH7_CompileList(&(*pGen),0);` |
|       5 |  1730 | `			pGen->pIn = pSavedIn;` |
|       5 |  1731 | `			pGen->pEnd = pSavedEnd;` |
|       5 |  1732 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1733 | `				return SXERR_ABORT;` |
|       - |  1734 | `			}` |
|       5 |  1735 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       3 |  1736 | `		}else{` |
|       - |  1737 | `			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]` |
|       - |  1738 | `			 * is already on the stack as the value; compiling the target appends` |
|       - |  1739 | `			 * its lvalue-load, which we fold into a STORE just as a normal` |
|       - |  1740 | `			 * assignment does. */` |
|       - |  1741 | `			VmInstr *pInstr;` |
|      34 |  1742 | `			sxi32 iVmOp = PH7_OP_STORE;` |
|      34 |  1743 | `			sxi32 iP1 = 0, iP2 = 0;` |
|      34 |  1744 | `			void *p3 = 0;` |
|      34 |  1745 | `			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,` |
|       - |  1746 | `				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|      34 |  1747 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  1748 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1749 | `			}` |
|      34 |  1750 | `			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|      34 |  1751 | `				if( pInstr->iOp == PH7_OP_MEMBER ){` |
|       3 |  1752 | `					iP2 = 1; /* member store: keep MEMBER, store value below it */` |
|      33 |  1753 | `				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       3 |  1754 | `					iVmOp = PH7_OP_STORE_IDX;` |
|       3 |  1755 | `					iP1 = pInstr->iP1;` |
|       3 |  1756 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       2 |  1757 | `				}else{` |
|      30 |  1758 | `					p3 = pInstr->p3; /* named store: $v = value */` |
|      30 |  1759 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - |  1760 | `				}` |
|      16 |  1761 | `			}` |
|      34 |  1762 | `			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|       - |  1763 | `			/* STORE leaves the assigned value on the stack top; drop it so the` |
|       - |  1764 | `			 * source array is back on top for the next entry. */` |
|      34 |  1765 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       - |  1766 | `		}` |
|      38 |  1767 | `		pGen->pIn = &pNext[1];` |
|       2 |  1768 | `	}` |
|      30 |  1769 | `	return SXRET_OK;` |
|      16 |  1770 |  |
|       - |  1771 | `/*` |
|       - |  1772 | ` * Shared body for list() and short list [...] compilation.` |
|       - |  1773 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|       - |  1774 | ` * the opening delimiter and before the closing delimiter.` |
|       - |  1775 | ` */` |
|     104 |  1776 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|       4 |  1777 |  |
|       - |  1778 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - |  1779 | `	SyToken *pNext;` |
|       - |  1780 | `	SyToken *pClassifyIn;` |
|     108 |  1781 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|       - |  1782 | `	sxi32 nExpr;` |
|       - |  1783 | `	sxi32 rc;` |
|       - |  1784 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|       - |  1785 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|       - |  1786 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|       - |  1787 | `	 * list. */` |
|     108 |  1788 | `	pClassifyIn = pGen->pIn;` |
|     302 |  1789 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     198 |  1790 | `		if( pGen->pIn >= pNext ){` |
|      13 |  1791 | `			nEmpty++;` |
|     192 |  1792 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|      38 |  1793 | `			nKeyed++;` |
|      20 |  1794 | `		}else{` |
|     150 |  1795 | `			nPositional++;` |
|       - |  1796 | `		}` |
|     198 |  1797 | `		pGen->pIn = &pNext[1];` |
|       4 |  1798 | `	}` |
|     108 |  1799 | `	pGen->pIn = pClassifyIn;` |
|     108 |  1800 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|     ! 0 |  1801 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1802 | `			"Cannot use empty array entries in keyed array assignment");` |
|     ! 0 |  1803 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1804 | `	}` |
|     108 |  1805 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|     ! 0 |  1806 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1807 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|     ! 0 |  1808 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1809 | `	}` |
|     108 |  1810 | `	if( nKeyed > 0 ){` |
|      30 |  1811 | `		return GenStateCompileKeyedListBody(pGen);` |
|       - |  1812 | `	}` |
|      80 |  1813 | `	nExpr = 0;` |
|      80 |  1814 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|     238 |  1815 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     162 |  1816 | `		if( pGen->pIn < pNext ){` |
|       - |  1817 | `			/* Check for nested list() */` |
|     150 |  1818 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       3 |  1819 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  1820 | `				/* Record this nested list for post-processing */` |
|       3 |  1821 | `				SyToken *pListEnd = 0;` |
|       3 |  1822 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|       3 |  1823 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       1 |  1824 | `				}` |
|       3 |  1825 | `				if( pListEnd ){` |
|       - |  1826 | `					struct NestedListEntry sEntry;` |
|       3 |  1827 | `					sEntry.nIndex = nExpr;` |
|       3 |  1828 | `					sEntry.pStart = pGen->pIn;` |
|       3 |  1829 | `					sEntry.pEnd = pListEnd + 1;` |
|       3 |  1830 | `					sEntry.isShort = 0;` |
|       3 |  1831 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       1 |  1832 | `				}` |
|       - |  1833 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       3 |  1834 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     149 |  1835 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  1836 | `				/* Nested short destructuring [...] */` |
|      13 |  1837 | `				SyToken *pBracketEnd = 0;` |
|      13 |  1838 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|      13 |  1839 | `				if( pBracketEnd ){` |
|       - |  1840 | `					struct NestedListEntry sEntry;` |
|      13 |  1841 | `					sEntry.nIndex = nExpr;` |
|      13 |  1842 | `					sEntry.pStart = pGen->pIn;` |
|      13 |  1843 | `					sEntry.pEnd = pBracketEnd + 1;` |
|      13 |  1844 | `					sEntry.isShort = 1;` |
|      13 |  1845 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       6 |  1846 | `				}` |
|       - |  1847 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|      13 |  1848 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |  1849 | `			}else{` |
|       - |  1850 | `				/* Compile the expression holding the variable */` |
|     136 |  1851 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     136 |  1852 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  1853 | `					SySetRelease(&sNested);` |
|     ! 0 |  1854 | `					return SXRET_OK;` |
|       - |  1855 | `				}` |
|       - |  1856 | `			}` |
|      77 |  1857 | `		}else{` |
|       - |  1858 | `			/* Empty entry,load NULL */` |
|      13 |  1859 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - |  1860 | `		}` |
|     162 |  1861 | `		nExpr++;` |
|       - |  1862 | `		/* Advance the stream cursor */` |
|     162 |  1863 | `		pGen->pIn = &pNext[1];` |
|       4 |  1864 | `	}` |
|       - |  1865 | `	/* Emit the LOAD_LIST instruction */` |
|      80 |  1866 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - |  1867 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - |  1868 | `	 * For each nested entry, emit code to extract the sub-array` |
|       - |  1869 | `	 * at the corresponding index and recursively destructure it.` |
|       - |  1870 | `	 */` |
|      80 |  1871 | `	if( SySetUsed(&sNested) > 0 ){` |
|      13 |  1872 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|       - |  1873 | `		sxu32 i;` |
|      27 |  1874 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|      15 |  1875 | `			SyToken *pSavedIn = pGen->pIn;` |
|      15 |  1876 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  1877 | `			ph7_value *pIdx;` |
|       - |  1878 | `			sxu32 nConstIdx;` |
|       - |  1879 | `			/* DUP the source array (it's on stack top) */` |
|      15 |  1880 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - |  1881 | `			/* Push the integer index for this nested entry */` |
|      15 |  1882 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|      15 |  1883 | `			if( pIdx == 0 ){` |
|     ! 0 |  1884 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  1885 | `				SySetRelease(&sNested);` |
|     ! 0 |  1886 | `				return SXERR_ABORT;` |
|       - |  1887 | `			}` |
|      15 |  1888 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|      15 |  1889 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|       - |  1890 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|       - |  1891 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|       - |  1892 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|       - |  1893 | `			 */` |
|      15 |  1894 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|       - |  1895 | `			/* Recursively compile the inner list */` |
|      15 |  1896 | `			pGen->pIn = apNested[i].pStart;` |
|      15 |  1897 | `			pGen->pEnd = apNested[i].pEnd;` |
|      15 |  1898 | `			if( apNested[i].isShort ){` |
|      13 |  1899 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|       7 |  1900 | `			}else{` |
|       3 |  1901 | `				rc = PH7_CompileList(&(*pGen),0);` |
|       - |  1902 | `			}` |
|      15 |  1903 | `			pGen->pIn = pSavedIn;` |
|      15 |  1904 | `			pGen->pEnd = pSavedEnd;` |
|      15 |  1905 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1906 | `				SySetRelease(&sNested);` |
|     ! 0 |  1907 | `				return SXERR_ABORT;` |
|       - |  1908 | `			}` |
|       - |  1909 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|      15 |  1910 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       8 |  1911 | `		}` |
|       6 |  1912 | `	}` |
|      80 |  1913 | `	SySetRelease(&sNested);` |
|       - |  1914 | `	/* Node successfully compiled */` |
|      80 |  1915 | `	return SXRET_OK;` |
|      56 |  1916 |  |
|      34 |  1917 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1918 |  |
|       - |  1919 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|      36 |  1920 | `	pGen->pIn += 2;` |
|      36 |  1921 | `	pGen->pEnd--;` |
|      17 |  1922 | `	SXUNUSED(iCompileFlag);` |
|      36 |  1923 | `	return GenStateCompileListBody(pGen);` |
|       2 |  1924 |  |
|      70 |  1925 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  1926 |  |
|       - |  1927 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      74 |  1928 | `	pGen->pIn++;` |
|      74 |  1929 | `	pGen->pEnd--;` |
|      35 |  1930 | `	SXUNUSED(iCompileFlag);` |
|      74 |  1931 | `	return GenStateCompileListBody(pGen);` |
|       4 |  1932 |  |
|       - |  1933 | `/* Forward declarations */` |
|       - |  1934 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|       - |  1935 | `static int GenStateIsReservedConstant(SyString *pName);` |
|       - |  1936 | `static int GenStateIsReadonly(SyToken *pTok);` |
|       - |  1937 | `static sxi32 GenStateValidateMemberType(ph7_gen_state *pGen,ph7_class *pClass,const SyString *pMemberName,` |
|       - |  1938 | `	sxu32 nType,const SyString *pTypeClass,const SyString *pTypeText,SySet *pUnionAlts,const char *zErrFmt,sxu32 nLine);` |
|       - |  1939 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|       - |  1940 | `/*` |
|       - |  1941 | ` * Compile an annoynmous function or a closure.` |
|       - |  1942 | ` * According to the PHP language reference` |
|       - |  1943 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - |  1944 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - |  1945 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - |  1946 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - |  1947 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - |  1948 | ` *  Example Anonymous function variable assignment example` |
|       - |  1949 | ` * <?php` |
|       - |  1950 | ` * $greet = function($name)` |
|       - |  1951 | ` * {` |
|       - |  1952 | ` *    printf("Hello %s\r\n", $name);` |
|       - |  1953 | ` * };` |
|       - |  1954 | ` * $greet('World');` |
|       - |  1955 | ` * $greet('PHP');` |
|       - |  1956 | ` * ?>` |
|       - |  1957 | ` * Note that the implementation of annoynmous function and closure under` |
|       - |  1958 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - |  1959 | ` */` |
|     292 |  1960 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1961 |  |
|       - |  1962 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - |  1963 | `	char zName[512];         /* Unique lambda name */` |
|       - |  1964 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - |  1965 | `							  * one thread is allowed to compile the script.` |
|       - |  1966 | `						      */` |
|       - |  1967 | `	SyString sName;` |
|       - |  1968 | `	sxu32 nLen;` |
|       - |  1969 | `	sxi32 rc;` |
|     146 |  1970 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1971 |  |
|     297 |  1972 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     297 |  1973 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 |  1974 | `		pGen->pIn++;` |
|     ! 0 |  1975 | `	}` |
|       - |  1976 | `	/* Generate a unique name */` |
|     297 |  1977 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - |  1978 | `	/* Make sure the generated name is unique */` |
|     297 |  1979 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  1980 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 |  1981 | `	}` |
|     297 |  1982 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - |  1983 | `	/* Compile the lambda body */` |
|     297 |  1984 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     297 |  1985 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  1986 | `		return SXERR_ABORT;` |
|       - |  1987 | `	}` |
|       - |  1988 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|       - |  1989 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|       - |  1990 | `	 * the handler wraps either in a Closure instance. */` |
|     297 |  1991 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       - |  1992 | `	/* Node successfully compiled */` |
|     297 |  1993 | `	return SXRET_OK;` |
|     151 |  1994 |  |
|       - |  1995 | `/*` |
|       - |  1996 | ` * Add a free variable to the arrow function's closure environment, unless` |
|       - |  1997 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|       - |  1998 | ` * enclosing arrow level, or has already been captured.` |
|       - |  1999 | ` */` |
|     172 |  2000 | `static sxi32 GenStateArrowAddCapture(` |
|       - |  2001 | `	ph7_gen_state *pGen,` |
|       - |  2002 | `	ph7_vm_func *pFunc,` |
|       - |  2003 | `	const char *zName,` |
|       - |  2004 | `	sxu32 nByte,` |
|       - |  2005 | `	SyString *aShadow,` |
|       - |  2006 | `	sxu32 nShadow)` |
|       2 |  2007 |  |
|       - |  2008 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2009 | `	ph7_vm_func_closure_env *aEnv;` |
|       - |  2010 | `	sxu32 n, nEnv;` |
|       - |  2011 | `	char *zDup;` |
|     174 |  2012 | `	if( nByte == 0 ){` |
|     ! 0 |  2013 | `		return SXRET_OK;` |
|       - |  2014 | `	}` |
|     172 |  2015 | `	if( nByte == sizeof("this")-1` |
|      92 |  2016 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|       3 |  2017 | `		return SXRET_OK;` |
|       - |  2018 | `	}` |
|     208 |  2019 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|     148 |  2020 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|     145 |  2021 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|     114 |  2022 | `			return SXRET_OK;` |
|       - |  2023 | `		}` |
|      19 |  2024 | `	}` |
|      59 |  2025 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      59 |  2026 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|      87 |  2027 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|      28 |  2028 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|      27 |  2029 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|     ! 0 |  2030 | `			return SXRET_OK;` |
|       - |  2031 | `		}` |
|      15 |  2032 | `	}` |
|      59 |  2033 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|      59 |  2034 | `	if( zDup == 0 ){` |
|     ! 0 |  2035 | `		return SXERR_ABORT;` |
|       - |  2036 | `	}` |
|      59 |  2037 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      59 |  2038 | `	sEnv.iFlags = 0;` |
|      59 |  2039 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      59 |  2040 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|      59 |  2041 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      59 |  2042 | `	return SXRET_OK;` |
|      88 |  2043 |  |
|       - |  2044 | `/*` |
|       - |  2045 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|       - |  2046 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|       - |  2047 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|       - |  2048 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|       - |  2049 | ` */` |
|      30 |  2050 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|       - |  2051 | `	ph7_gen_state *pGen,` |
|       - |  2052 | `	ph7_vm_func *pFunc,` |
|       - |  2053 | `	const char *zIn,` |
|       - |  2054 | `	const char *zEnd,` |
|       - |  2055 | `	SyString *aShadow,` |
|       - |  2056 | `	sxu32 nShadow)` |
|       1 |  2057 |  |
|       - |  2058 | `	sxi32 rc;` |
|     213 |  2059 | `	while( zIn < zEnd ){` |
|     183 |  2060 | `		if( zIn[0] == '\\' ){` |
|     ! 0 |  2061 | `			zIn++;` |
|     ! 0 |  2062 | `			if( zIn < zEnd ){` |
|     ! 0 |  2063 | `				zIn++;` |
|     ! 0 |  2064 | `			}` |
|     ! 0 |  2065 | `			continue;` |
|       - |  2066 | `		}` |
|     182 |  2067 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|      13 |  2068 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|      12 |  2069 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|       - |  2070 | `			const char *zName;` |
|      13 |  2071 | `			zIn++; /* skip '$' */` |
|      13 |  2072 | `			zName = zIn;` |
|      39 |  2073 | `			while( zIn < zEnd ){` |
|      35 |  2074 | `				unsigned char c = (unsigned char)zIn[0];` |
|      35 |  2075 | `				if( c >= 0xc0 ){` |
|     ! 0 |  2076 | `					zIn++;` |
|     ! 0 |  2077 | `					while( zIn < zEnd` |
|     ! 0 |  2078 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  2079 | `						zIn++;` |
|     ! 0 |  2080 | `					}` |
|     ! 0 |  2081 | `					continue;` |
|       - |  2082 | `				}` |
|      35 |  2083 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       9 |  2084 | `					break;` |
|       - |  2085 | `				}` |
|      27 |  2086 | `				zIn++;` |
|       1 |  2087 | `			}` |
|      13 |  2088 | `			if( zIn > zName ){` |
|      19 |  2089 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|      12 |  2090 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|      13 |  2091 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2092 | `					return SXERR_ABORT;` |
|       - |  2093 | `				}` |
|       6 |  2094 | `			}` |
|      13 |  2095 | `			continue;` |
|       - |  2096 | `		}` |
|     171 |  2097 | `		zIn++;` |
|       1 |  2098 | `	}` |
|      31 |  2099 | `	return SXRET_OK;` |
|      16 |  2100 |  |
|       - |  2101 | `/*` |
|       - |  2102 | ` * Scan the body token range of an arrow function for free-variable` |
|       - |  2103 | ` * references and record them in pFunc's closure environment. Handles:` |
|       - |  2104 | ` *   - plain $<id> pairs` |
|       - |  2105 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|       - |  2106 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|       - |  2107 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|       - |  2108 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|       - |  2109 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|       - |  2110 | ` *     are never mistakenly captured.` |
|       - |  2111 | ` */` |
|     178 |  2112 | `static sxi32 GenStateArrowCaptureScan(` |
|       - |  2113 | `	ph7_gen_state *pGen,` |
|       - |  2114 | `	ph7_vm_func *pFunc,` |
|       - |  2115 | `	SyToken *pStart,` |
|       - |  2116 | `	SyToken *pEnd,` |
|       - |  2117 | `	SyString *aShadow,` |
|       - |  2118 | `	sxu32 nShadow)` |
|       2 |  2119 |  |
|     180 |  2120 | `	SyToken *pScan = pStart;` |
|       - |  2121 | `	sxi32 rc;` |
|     686 |  2122 | `	while( pScan < pEnd ){` |
|     508 |  2123 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|      46 |  2124 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|      15 |  2125 | `				pScan->sData.zString,` |
|      30 |  2126 | `				pScan->sData.zString + pScan->sData.nByte,` |
|      15 |  2127 | `				aShadow,nShadow);` |
|      31 |  2128 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2129 | `				return SXERR_ABORT;` |
|       - |  2130 | `			}` |
|      31 |  2131 | `			pScan++;` |
|      31 |  2132 | `			continue;` |
|       - |  2133 | `		}` |
|     478 |  2134 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|      21 |  2135 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|      21 |  2136 | `			SyToken *pFnKw = pScan;` |
|      20 |  2137 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|     ! 0 |  2138 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|       1 |  2139 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  2140 | `				pFnKw = &pScan[1];` |
|     ! 0 |  2141 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  2142 | `			}` |
|      21 |  2143 | `			if( nKw == PH7_TKWRD_FN ){` |
|       - |  2144 | `				SyToken *pInnerSigStart;` |
|       - |  2145 | `				SyToken *pInnerSigEnd;` |
|       - |  2146 | `				SyToken *pInnerBodyEnd;` |
|       - |  2147 | `				SyString *aInnerShadow;` |
|       - |  2148 | `				sxu32 nInnerShadow;` |
|       - |  2149 | `				sxu32 nInnerParamMax;` |
|       - |  2150 | `				SyToken *p;` |
|       - |  2151 | `				int iNestInner;` |
|      19 |  2152 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|      19 |  2153 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2154 | `					pScan++;` |
|     ! 0 |  2155 | `				}` |
|      19 |  2156 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2157 | `					pScan++;` |
|     ! 0 |  2158 | `					continue;` |
|       - |  2159 | `				}` |
|      19 |  2160 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|      19 |  2161 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|       - |  2162 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|      19 |  2163 | `				if( pInnerSigEnd >= pEnd ){` |
|     ! 0 |  2164 | `					pScan = pEnd;` |
|     ! 0 |  2165 | `					continue;` |
|       - |  2166 | `				}` |
|       - |  2167 | `				/* Build an augmented shadow list: inherited + inner params */` |
|      19 |  2168 | `				nInnerParamMax = 0;` |
|      57 |  2169 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2170 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|      13 |  2171 | `						nInnerParamMax++;` |
|       6 |  2172 | `					}` |
|      20 |  2173 | `				}` |
|      19 |  2174 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      18 |  2175 | `					&pGen->pVm->sAllocator,` |
|      18 |  2176 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|      19 |  2177 | `				if( aInnerShadow == 0 ){` |
|     ! 0 |  2178 | `					return SXERR_ABORT;` |
|       - |  2179 | `				}` |
|      19 |  2180 | `				nInnerShadow = 0;` |
|      25 |  2181 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|       7 |  2182 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|       4 |  2183 | `				}` |
|      57 |  2184 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2185 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      27 |  2186 | `						continue;` |
|       - |  2187 | `					}` |
|      13 |  2188 | `					if( &p[1] >= pInnerSigEnd ){` |
|     ! 0 |  2189 | `						break;` |
|       - |  2190 | `					}` |
|      13 |  2191 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2192 | `						continue;` |
|       - |  2193 | `					}` |
|      13 |  2194 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|       7 |  2195 | `				}` |
|      19 |  2196 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|      19 |  2197 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|     ! 0 |  2198 | `					pScan++;` |
|     ! 0 |  2199 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|     ! 0 |  2200 | `						&& pScan->sData.nByte == 1` |
|     ! 0 |  2201 | `						&& pScan->sData.zString[0] == '?' ){` |
|     ! 0 |  2202 | `						pScan++;` |
|     ! 0 |  2203 | `					}` |
|     ! 0 |  2204 | `					if( pScan < pEnd` |
|     ! 0 |  2205 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  2206 | `						pScan++;` |
|     ! 0 |  2207 | `					}` |
|     ! 0 |  2208 | `				}` |
|      19 |  2209 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|      19 |  2210 | `					pScan++; /* past '=>' */` |
|       9 |  2211 | `				}` |
|      19 |  2212 | `				pInnerBodyEnd = pScan;` |
|      19 |  2213 | `				iNestInner = 0;` |
|     131 |  2214 | `				while( pInnerBodyEnd < pEnd ){` |
|     113 |  2215 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|       - |  2216 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|       - |  2217 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|     ! 0 |  2218 | `						break;` |
|       - |  2219 | `					}` |
|     113 |  2220 | `					if( pInnerBodyEnd->nType &` |
|       - |  2221 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       3 |  2222 | `						iNestInner++;` |
|     112 |  2223 | `					}else if( pInnerBodyEnd->nType &` |
|       - |  2224 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       3 |  2225 | `						iNestInner--;` |
|       1 |  2226 | `					}` |
|     113 |  2227 | `					pInnerBodyEnd++;` |
|       1 |  2228 | `				}` |
|       - |  2229 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|       - |  2230 | `				 * the outer's body: a default value is evaluated at call time` |
|       - |  2231 | `				 * in the outer frame, so any free variable it references is` |
|       - |  2232 | `				 * an outer capture. We must NOT scan the parameter-name` |
|       - |  2233 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|       - |  2234 | `				 * or those names leak into the outer's closure environment.` |
|       - |  2235 | `				 *` |
|       - |  2236 | `				 * Walk the signature argument-by-argument, splitting on` |
|       - |  2237 | `				 * top-level commas, and for each argument scan only the token` |
|       - |  2238 | `				 * range after the '=' sign. */` |
|       - |  2239 | `				{` |
|      19 |  2240 | `					SyToken *pArgStart = pInnerSigStart;` |
|      31 |  2241 | `					while( pArgStart < pInnerSigEnd ){` |
|      13 |  2242 | `						SyToken *pArgEnd = pArgStart;` |
|      13 |  2243 | `						SyToken *pEq = 0;` |
|      13 |  2244 | `						int iNestArg = 0;` |
|      49 |  2245 | `						while( pArgEnd < pInnerSigEnd ){` |
|      38 |  2246 | `							if( iNestArg == 0` |
|      39 |  2247 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|       3 |  2248 | `								break;` |
|       - |  2249 | `							}` |
|      37 |  2250 | `							if( pArgEnd->nType &` |
|       - |  2251 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  2252 | `								iNestArg++;` |
|      37 |  2253 | `							}else if( pArgEnd->nType &` |
|       - |  2254 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  2255 | `								iNestArg--;` |
|     ! 0 |  2256 | `							}` |
|      36 |  2257 | `							if( pEq == 0 && iNestArg == 0` |
|      31 |  2258 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|       7 |  2259 | `								pEq = pArgEnd;` |
|       3 |  2260 | `							}` |
|      37 |  2261 | `							pArgEnd++;` |
|       1 |  2262 | `						}` |
|      13 |  2263 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|      10 |  2264 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       3 |  2265 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|       7 |  2266 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  2267 | `								return SXERR_ABORT;` |
|       - |  2268 | `							}` |
|       3 |  2269 | `						}` |
|      13 |  2270 | `						pArgStart = pArgEnd;` |
|      12 |  2271 | `						if( pArgStart < pInnerSigEnd` |
|       8 |  2272 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|       3 |  2273 | `							pArgStart++;` |
|       1 |  2274 | `						}` |
|       1 |  2275 | `					}` |
|       - |  2276 | `				}` |
|      28 |  2277 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       9 |  2278 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|      19 |  2279 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2280 | `					return SXERR_ABORT;` |
|       - |  2281 | `				}` |
|      19 |  2282 | `				pScan = pInnerBodyEnd;` |
|      19 |  2283 | `				continue;` |
|       - |  2284 | `			}` |
|       1 |  2285 | `		}` |
|     460 |  2286 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     300 |  2287 | `			pScan++;` |
|     300 |  2288 | `			continue;` |
|       - |  2289 | `		}` |
|       - |  2290 | `		{` |
|       - |  2291 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|     162 |  2292 | `			SyToken *pDollar = pScan;` |
|     240 |  2293 | `			while( &pDollar[1] < pEnd` |
|     162 |  2294 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 |  2295 | `				pDollar++;` |
|     ! 0 |  2296 | `			}` |
|     162 |  2297 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 |  2298 | `				break;` |
|       - |  2299 | `			}` |
|     162 |  2300 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2301 | `				pScan = pDollar + 1;` |
|     ! 0 |  2302 | `				continue;` |
|       - |  2303 | `			}` |
|     242 |  2304 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|     160 |  2305 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      80 |  2306 | `				aShadow,nShadow);` |
|     162 |  2307 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2308 | `				return SXERR_ABORT;` |
|       - |  2309 | `			}` |
|     162 |  2310 | `			pScan = pDollar + 2;` |
|       - |  2311 | `		}` |
|       2 |  2312 | `	}` |
|     180 |  2313 | `	return SXRET_OK;` |
|      91 |  2314 |  |
|       - |  2315 | `/*` |
|       - |  2316 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  2317 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  2318 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  2319 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  2320 | ` * $this is also made available.` |
|       - |  2321 | ` */` |
|     160 |  2322 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  2323 |  |
|       - |  2324 | `	ph7_vm_func *pFunc;` |
|       - |  2325 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2326 | `	GenBlock *pBlock;` |
|       - |  2327 | `	SySet *pInstrContainer;` |
|       - |  2328 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|       - |  2329 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|       - |  2330 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|       - |  2331 | `	SyToken *pSavedEnd;` |
|       - |  2332 | `	ph7_vm_func_arg *aArgs;` |
|       - |  2333 | `	char zName[512];` |
|       - |  2334 | `	static int iCnt = 1;` |
|       - |  2335 | `	char *zDup;` |
|       - |  2336 | `	sxu32 nLen;` |
|       - |  2337 | `	sxu32 nLine;` |
|     164 |  2338 | `	sxi32 iFlags = 0;` |
|     164 |  2339 | `	int bStatic = 0;` |
|       - |  2340 | `	sxi32 rc;` |
|       - |  2341 | `	sxu32 n;` |
|      80 |  2342 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2343 |  |
|     164 |  2344 | `	nLine = pGen->pIn->nLine;` |
|       - |  2345 | `	/* Optional 'static' prefix */` |
|     160 |  2346 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     164 |  2347 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       3 |  2348 | `		bStatic = 1;` |
|       3 |  2349 | `		pGen->pIn++;` |
|       1 |  2350 | `	}` |
|       - |  2351 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|     160 |  2352 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     164 |  2353 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  2354 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2355 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  2356 | `		return SXERR_SYNTAX;` |
|       - |  2357 | `	}` |
|     164 |  2358 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  2359 | `	/* Optional '&' — return by reference */` |
|     164 |  2360 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2361 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  2362 | `		pGen->pIn++;` |
|     ! 0 |  2363 | `	}` |
|       - |  2364 | `	/* Expect '(' */` |
|     164 |  2365 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  2366 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2367 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2368 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|       2 |  2369 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2370 | `		}else{` |
|     ! 0 |  2371 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2372 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|       - |  2373 | `		}` |
|       3 |  2374 | `		return SXERR_SYNTAX;` |
|       - |  2375 | `	}` |
|     161 |  2376 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  2377 | `	/* Delimit the parameter list */` |
|     161 |  2378 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|     161 |  2379 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 |  2380 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2381 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 |  2382 | `		return SXERR_SYNTAX;` |
|       - |  2383 | `	}` |
|       - |  2384 | `	/* Allocate the function state */` |
|     158 |  2385 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|     158 |  2386 | `	if( pFunc == 0 ){` |
|     ! 0 |  2387 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2388 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2389 | `		return SXERR_ABORT;` |
|       - |  2390 | `	}` |
|       - |  2391 | `	/* Generate a unique lambda name */` |
|     158 |  2392 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     260 |  2393 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     104 |  2394 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       2 |  2395 | `	}` |
|     158 |  2396 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|     158 |  2397 | `	if( zDup == 0 ){` |
|     ! 0 |  2398 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2399 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2400 | `		return SXERR_ABORT;` |
|       - |  2401 | `	}` |
|     158 |  2402 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  2403 | `	/* Collect function arguments */` |
|     158 |  2404 | `	if( pGen->pIn < pSigEnd ){` |
|     100 |  2405 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|     100 |  2406 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2407 | `			return SXERR_ABORT;` |
|       - |  2408 | `		}` |
|      49 |  2409 | `	}` |
|       - |  2410 | `	/* Point past ')' and parse optional return type */` |
|     158 |  2411 | `	pGen->pIn = &pSigEnd[1];` |
|     158 |  2412 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|     158 |  2413 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2414 | `		return SXERR_ABORT;` |
|     158 |  2415 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  2416 | `		return SXERR_SYNTAX;` |
|       - |  2417 | `	}` |
|       - |  2418 | `	/* Expect '=>' */` |
|     158 |  2419 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2420 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2421 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2422 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|       2 |  2423 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2424 | `		}else{` |
|     ! 0 |  2425 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2426 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|       - |  2427 | `		}` |
|       3 |  2428 | `		return SXERR_SYNTAX;` |
|       - |  2429 | `	}` |
|     156 |  2430 | `	pGen->pIn++; /* Jump '=>' */` |
|     156 |  2431 | `	pBodyStart = pGen->pIn;` |
|     156 |  2432 | `	pBodyEnd = pGen->pEnd;` |
|       - |  2433 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  2434 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  2435 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  2436 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|     156 |  2437 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  2438 | `	{` |
|     156 |  2439 | `		SyString *aShadow = 0;` |
|     156 |  2440 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|     156 |  2441 | `		if( nShadow > 0 ){` |
|      98 |  2442 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      96 |  2443 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|      98 |  2444 | `			if( aShadow == 0 ){` |
|     ! 0 |  2445 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2446 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2447 | `				return SXERR_ABORT;` |
|       - |  2448 | `			}` |
|     216 |  2449 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|     120 |  2450 | `				aShadow[n] = aArgs[n].sName;` |
|      61 |  2451 | `			}` |
|      48 |  2452 | `		}` |
|     233 |  2453 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      77 |  2454 | `			aShadow,nShadow);` |
|     156 |  2455 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2456 | `			return SXERR_ABORT;` |
|       - |  2457 | `		}` |
|       - |  2458 | `	}` |
|       - |  2459 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  2460 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  2461 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  2462 | `	 * $this. */` |
|     156 |  2463 | `	if( !bStatic ){` |
|       - |  2464 | `		char *zThisDup;` |
|     154 |  2465 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|     154 |  2466 | `		if( zThisDup == 0 ){` |
|     ! 0 |  2467 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2468 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2469 | `			return SXERR_ABORT;` |
|       - |  2470 | `		}` |
|     154 |  2471 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     154 |  2472 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|     154 |  2473 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     154 |  2474 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|     154 |  2475 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      76 |  2476 | `	}` |
|       - |  2477 | `	/* Arrow functions are always closures */` |
|     156 |  2478 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       - |  2479 | `	/* Compile the body expression as an implicit return */` |
|     233 |  2480 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      77 |  2481 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|     156 |  2482 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2483 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2484 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  2485 | `		return SXERR_ABORT;` |
|       - |  2486 | `	}` |
|     156 |  2487 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     156 |  2488 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|     156 |  2489 | `	pSavedEnd = pGen->pEnd;` |
|     156 |  2490 | `	pGen->pIn = pBodyStart;` |
|     156 |  2491 | `	pGen->pEnd = pBodyEnd;` |
|     156 |  2492 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     156 |  2493 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2494 | `		return SXERR_ABORT;` |
|       - |  2495 | `	}` |
|       - |  2496 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|       - |  2497 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|       - |  2498 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|       - |  2499 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|     156 |  2500 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     156 |  2501 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     156 |  2502 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     156 |  2503 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     156 |  2504 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  2505 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|     156 |  2506 | `	pGen->pIn = pBodyEnd;` |
|     156 |  2507 | `	pGen->pEnd = pSavedEnd;` |
|       - |  2508 | `	/* Emit the load-closure instruction */` |
|     156 |  2509 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|     156 |  2510 | `	return SXRET_OK;` |
|      84 |  2511 |  |
|       - |  2512 | `/*` |
|       - |  2513 | ` * Compile a single arm's expression range into a freshly-allocated` |
|       - |  2514 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|       - |  2515 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|       - |  2516 | ` * expression's value.` |
|       - |  2517 | ` */` |
|     346 |  2518 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|       - |  2519 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|       3 |  2520 |  |
|       - |  2521 | `	SySet *pInstrContainer;` |
|       - |  2522 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  2523 | `	GenBlock *pArmBlock;` |
|       - |  2524 | `	sxi32 rc;` |
|     349 |  2525 | `	pTmpIn  = pGen->pIn;` |
|     349 |  2526 | `	pTmpEnd = pGen->pEnd;` |
|     349 |  2527 | `	pGen->pIn  = pStart;` |
|     349 |  2528 | `	pGen->pEnd = pStop;` |
|     349 |  2529 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     349 |  2530 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|       - |  2531 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|       - |  2532 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|       - |  2533 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|       - |  2534 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|       - |  2535 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|     522 |  2536 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|     173 |  2537 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|     349 |  2538 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2539 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  2540 | `		pGen->pIn  = pTmpIn;` |
|     ! 0 |  2541 | `		pGen->pEnd = pTmpEnd;` |
|     ! 0 |  2542 | `		return SXERR_ABORT;` |
|       - |  2543 | `	}` |
|     349 |  2544 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     349 |  2545 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     349 |  2546 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     349 |  2547 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     349 |  2548 | `	GenStateLeaveBlock(&(*pGen),0);` |
|     349 |  2549 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     349 |  2550 | `	pGen->pIn  = pTmpIn;` |
|     349 |  2551 | `	pGen->pEnd = pTmpEnd;` |
|     349 |  2552 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2553 | `		return SXERR_ABORT;` |
|       - |  2554 | `	}` |
|     349 |  2555 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  2556 | `		return SXERR_EMPTY;` |
|       - |  2557 | `	}` |
|     349 |  2558 | `	return SXRET_OK;` |
|     176 |  2559 |  |
|       - |  2560 | `/*` |
|       - |  2561 | ` * Compile a PHP 8.0 match expression:` |
|       - |  2562 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|       - |  2563 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|       - |  2564 | ` * Strict comparison (===) is used between the subject and each condition.` |
|       - |  2565 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|       - |  2566 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|       - |  2567 | ` */` |
|       - |  2568 | `/*` |
|       - |  2569 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|       - |  2570 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|       - |  2571 | ` * caller can bail out of the current expression.` |
|       - |  2572 | ` */` |
|       2 |  2573 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|       1 |  2574 |  |
|       - |  2575 | `	va_list ap;` |
|       - |  2576 | `	sxi32 rc;` |
|       - |  2577 | `	SyBlob sMsg;` |
|       3 |  2578 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       3 |  2579 | `	va_start(ap,zFmt);` |
|       3 |  2580 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|       3 |  2581 | `	va_end(ap);` |
|       3 |  2582 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|       3 |  2583 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|       3 |  2584 | `	SyBlobRelease(&sMsg);` |
|       3 |  2585 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2586 | `		return SXERR_ABORT;` |
|       - |  2587 | `	}` |
|       3 |  2588 | `	return SXERR_SYNTAX;` |
|       2 |  2589 |  |
|       - |  2590 | `/*` |
|       - |  2591 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|       - |  2592 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|       - |  2593 | ` * Returns the stop token pointer (or pEnd if none found).` |
|       - |  2594 | ` */` |
|     348 |  2595 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|       4 |  2596 |  |
|     352 |  2597 | `	SyToken *pCur = pStart;` |
|     352 |  2598 | `	int iNest = 0;` |
|     814 |  2599 | `	while( pCur < pEnd ){` |
|     780 |  2600 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      13 |  2601 | `			iNest++;` |
|     774 |  2602 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      13 |  2603 | `			iNest--;` |
|     762 |  2604 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|     317 |  2605 | `			return pCur;` |
|       - |  2606 | `		}` |
|     466 |  2607 | `		pCur++;` |
|       4 |  2608 | `	}` |
|      37 |  2609 | `	return pEnd;` |
|     178 |  2610 |  |
|      70 |  2611 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2612 |  |
|       - |  2613 | `	ph7_match *pMatch;` |
|       - |  2614 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|      75 |  2615 | `	int bHasDefault = 0;` |
|       - |  2616 | `	sxu32 nLine;` |
|       - |  2617 | `	sxi32 rc;` |
|      35 |  2618 | `	SXUNUSED(iCompileFlag);` |
|      75 |  2619 | `	nLine = pGen->pIn->nLine;` |
|      75 |  2620 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|       - |  2621 | `	/* Expect '(' */` |
|      75 |  2622 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2623 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2624 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|     ! 0 |  2625 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|       - |  2626 | `	}` |
|      75 |  2627 | `	pGen->pIn++; /* Jump '(' */` |
|      75 |  2628 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|      75 |  2629 | `	if( pSubjEnd >= pGen->pEnd ){` |
|     ! 0 |  2630 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2631 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       - |  2632 | `	}` |
|      75 |  2633 | `	if( pGen->pIn >= pSubjEnd ){` |
|     ! 0 |  2634 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2635 | `			"syntax error, unexpected \")\", expecting match subject");` |
|       - |  2636 | `	}` |
|       - |  2637 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|      75 |  2638 | `	pSavedEnd = pGen->pEnd;` |
|      75 |  2639 | `	pGen->pEnd = pSubjEnd;` |
|      75 |  2640 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      75 |  2641 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2642 | `		return SXERR_ABORT;` |
|       - |  2643 | `	}` |
|      75 |  2644 | `	pGen->pEnd = pSavedEnd;` |
|      75 |  2645 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|       - |  2646 | `	/* Expect '{' */` |
|      75 |  2647 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  2648 | `		return GenStateMatchError(pGen,` |
|     ! 0 |  2649 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  2650 | `			"syntax error, expecting \"{\" after match subject");` |
|       - |  2651 | `	}` |
|      75 |  2652 | `	pGen->pIn++; /* Jump '{' */` |
|      75 |  2653 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|      75 |  2654 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  2655 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2656 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|       - |  2657 | `	}` |
|       - |  2658 | `	/* Allocate ph7_match container */` |
|      75 |  2659 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|      75 |  2660 | `	if( pMatch == 0 ){` |
|     ! 0 |  2661 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2662 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2663 | `		return SXERR_ABORT;` |
|       - |  2664 | `	}` |
|      75 |  2665 | `	SyZero(pMatch,sizeof(ph7_match));` |
|      75 |  2666 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|       - |  2667 | `	/* Iterate arms */` |
|     253 |  2668 | `	while( pGen->pIn < pBodyEnd ){` |
|       - |  2669 | `		ph7_match_arm sArm;` |
|       - |  2670 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|     186 |  2671 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|     186 |  2672 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|     186 |  2673 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|     186 |  2674 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  2675 | `		/* 'default' arm? */` |
|     182 |  2676 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     105 |  2677 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|      22 |  2678 | `			if( bHasDefault ){` |
|       3 |  2679 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|       - |  2680 | `					"Match expressions may only contain one default arm");` |
|       4 |  2681 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  2682 | `			}` |
|      20 |  2683 | `			sArm.bDefault = 1;` |
|      20 |  2684 | `			bHasDefault = 1;` |
|      20 |  2685 | `			pGen->pIn++;` |
|      20 |  2686 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|     ! 0 |  2687 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2688 | `					"syntax error, expecting \"=>\" after 'default'");` |
|       - |  2689 | `			}` |
|      20 |  2690 | `			pGen->pIn++; /* Jump '=>' */` |
|      11 |  2691 | `		}else{` |
|       - |  2692 | `			/* Condition list: cond (',' cond)* '=>' */` |
|     166 |  2693 | `			pCondStart = pGen->pIn;` |
|     166 |  2694 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|       - |  2695 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|     174 |  2696 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|       - |  2697 | `				SySet sCondBc;` |
|       9 |  2698 | `				if( pCondStart >= pArrow ){` |
|     ! 0 |  2699 | `					return GenStateMatchError(pGen,nArmLine,` |
|       - |  2700 | `						"syntax error, empty match condition expression");` |
|       - |  2701 | `				}` |
|       9 |  2702 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       9 |  2703 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       9 |  2704 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2705 | `					return SXERR_ABORT;` |
|       - |  2706 | `				}` |
|       9 |  2707 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       9 |  2708 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|       9 |  2709 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|       - |  2710 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       1 |  2711 | `			}` |
|     166 |  2712 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2713 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2714 | `					"syntax error, expecting \"=>\" in match arm");` |
|       - |  2715 | `			}` |
|     163 |  2716 | `			if( pCondStart >= pArrow ){` |
|     ! 0 |  2717 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2718 | `					"syntax error, empty match condition expression");` |
|       - |  2719 | `			}` |
|       - |  2720 | `			{` |
|       - |  2721 | `				SySet sCondBc;` |
|     163 |  2722 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     163 |  2723 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|     163 |  2724 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2725 | `					return SXERR_ABORT;` |
|       - |  2726 | `				}` |
|     163 |  2727 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       - |  2728 | `			}` |
|     163 |  2729 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|       - |  2730 | `		}` |
|       - |  2731 | `		/* Compile result expression: up to top-level ',' or body end */` |
|     181 |  2732 | `		pResStart = pGen->pIn;` |
|     181 |  2733 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|     181 |  2734 | `		if( pResStart >= pResEnd ){` |
|     ! 0 |  2735 | `			return GenStateMatchError(pGen,nArmLine,` |
|       - |  2736 | `				"syntax error, expected expression after \"=>\"");` |
|       - |  2737 | `		}` |
|     181 |  2738 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|     181 |  2739 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2740 | `			return SXERR_ABORT;` |
|       - |  2741 | `		}` |
|     181 |  2742 | `		pGen->pIn = pResEnd;` |
|     181 |  2743 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|     149 |  2744 | `			pGen->pIn++; /* Skip trailing ',' */` |
|      73 |  2745 | `		}` |
|     181 |  2746 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|       3 |  2747 | `	}` |
|      69 |  2748 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|      69 |  2749 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|      69 |  2750 | `	return SXRET_OK;` |
|      40 |  2751 |  |
|       - |  2752 | `/*` |
|       - |  2753 | ` * Compile a backtick quoted string.` |
|       - |  2754 | ` */` |
|       4 |  2755 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2756 |  |
|       - |  2757 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - |  2758 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - |  2759 | `	 */` |
|       8 |  2760 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - |  2761 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 |  2762 | `		ph7_lib_version()` |
|       - |  2763 | `		);` |
|       - |  2764 | `	/* Load NULL */` |
|       6 |  2765 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2766 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2767 | `	/* Node successfully compiled */` |
|       6 |  2768 | `	return SXRET_OK;` |
|       2 |  2769 |  |
|       - |  2770 | `/*` |
|       - |  2771 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - |  2772 | ` * construct.` |
|       - |  2773 | ` */` |
|      80 |  2774 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2775 |  |
|       - |  2776 | `	SyString *pName;` |
|       - |  2777 | `	sxu32 nKeyID;` |
|       - |  2778 | `	sxi32 rc;` |
|       - |  2779 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      85 |  2780 | `	pName = &pGen->pIn->sData;` |
|      85 |  2781 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      85 |  2782 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      85 |  2783 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 |  2784 | `		SyToken *pTmp,*pNext = 0;` |
|       - |  2785 | `		/* Compile arguments one after one */` |
|       9 |  2786 | `		pTmp = pGen->pEnd;` |
|       - |  2787 | `		/* Symisc eXtension to the PHP programming language:` |
|       - |  2788 | `		 * 'echo' can be used in the context of a function which` |
|       - |  2789 | `		 *  mean that the following expression is valid:` |
|       - |  2790 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - |  2791 | `		 */` |
|       9 |  2792 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 |  2793 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 |  2794 | `			if( pGen->pIn < pNext ){` |
|       9 |  2795 | `				pGen->pEnd = pNext;` |
|       9 |  2796 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 |  2797 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2798 | `					return SXERR_ABORT;` |
|       - |  2799 | `				}` |
|       9 |  2800 | `				if( rc != SXERR_EMPTY ){` |
|       - |  2801 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - |  2802 | `					 * without the overhead of a function call.` |
|       - |  2803 | `					 * This is a very powerful optimization that improve` |
|       - |  2804 | `					 * performance greatly.` |
|       - |  2805 | `					 */` |
|       9 |  2806 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 |  2807 | `				}` |
|       4 |  2808 | `			}` |
|       - |  2809 | `			/* Jump trailing commas */` |
|       9 |  2810 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 |  2811 | `				pNext++;` |
|     ! 0 |  2812 | `			}` |
|       9 |  2813 | `			pGen->pIn = pNext;` |
|       1 |  2814 | `		}` |
|       - |  2815 | `		/* Restore token stream */` |
|       9 |  2816 | `		pGen->pEnd = pTmp;` |
|       5 |  2817 | `	}else{` |
|      77 |  2818 | `		sxi32 nArg = 0;` |
|      77 |  2819 | `		sxu32 nIdx = 0;` |
|      77 |  2820 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      77 |  2821 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2822 | `			return SXERR_ABORT;` |
|      77 |  2823 | `		}else if(rc != SXERR_EMPTY ){` |
|      77 |  2824 | `			nArg = 1;` |
|      36 |  2825 | `		}` |
|      77 |  2826 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - |  2827 | `			ph7_value *pObj;` |
|       - |  2828 | `			/* Emit the call instruction */` |
|      29 |  2829 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      29 |  2830 | `			if( pObj == 0 ){` |
|     ! 0 |  2831 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2832 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2833 | `				return SXERR_ABORT;` |
|       - |  2834 | `			}` |
|      29 |  2835 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - |  2836 | `			/* Install in the literal table */` |
|      29 |  2837 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      12 |  2838 | `		}` |
|       - |  2839 | `		/* Emit the call instruction */` |
|      77 |  2840 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      77 |  2841 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       - |  2842 | `	}` |
|       - |  2843 | `	/* Node successfully compiled */` |
|      85 |  2844 | `	return SXRET_OK;` |
|      45 |  2845 |  |
|       - |  2846 | `/*` |
|       - |  2847 | ` * Compile a node holding a variable declaration.` |
|       - |  2848 | ` * According to the PHP language reference` |
|       - |  2849 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - |  2850 | ` *  The variable name is case-sensitive.` |
|       - |  2851 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - |  2852 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  2853 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - |  2854 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - |  2855 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - |  2856 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - |  2857 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - |  2858 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - |  2859 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - |  2860 | ` *  the chapter on Expressions.` |
|       - |  2861 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - |  2862 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - |  2863 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - |  2864 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - |  2865 | ` *  is being assigned (the source variable).` |
|       - |  2866 | ` */` |
| 1138276 |  2867 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2868 |  |
| 1138281 |  2869 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  2870 | `	sxi32 iVv;` |
|       - |  2871 | `	sxi32 iP1;` |
|       - |  2872 | `	void *p3;` |
|       - |  2873 | `	sxi32 rc;` |
| 1138281 |  2874 | `	iVv = -1; /* Variable variable counter */` |
| 2276569 |  2875 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
| 1138293 |  2876 | `		pGen->pIn++;` |
| 1138293 |  2877 | `		iVv++;` |
|       5 |  2878 | `	}` |
| 1138281 |  2879 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - |  2880 | `		/* Invalid variable name */` |
|     ! 0 |  2881 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 |  2882 | `		if( rc == SXERR_ABORT ){` |
|       - |  2883 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2884 | `			return SXERR_ABORT;` |
|       - |  2885 | `		}` |
|     ! 0 |  2886 | `		return SXRET_OK;` |
|       - |  2887 | `	}` |
| 1138281 |  2888 | `	p3  = 0;` |
| 1138281 |  2889 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - |  2890 | `		/* Dynamic variable creation */` |
|      19 |  2891 | `		pGen->pIn++;  /* Jump the open curly */` |
|      19 |  2892 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      19 |  2893 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  2894 | `			/* Empty expression */` |
|       3 |  2895 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 |  2896 | `			return SXRET_OK;` |
|       - |  2897 | `		}` |
|       - |  2898 | `		/* Compile the expression holding the variable name */` |
|      16 |  2899 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 |  2900 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2901 | `			return SXERR_ABORT;` |
|      16 |  2902 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 |  2903 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 |  2904 | `			return SXRET_OK;` |
|       - |  2905 | `		}` |
|       7 |  2906 | `	}else{` |
|       - |  2907 | `		SyHashEntry *pEntry;` |
|       - |  2908 | `		SyString *pName;` |
| 1138265 |  2909 | `		char *zName = 0;` |
|       - |  2910 | `		/* Extract variable name */` |
| 1138265 |  2911 | `		pName = &pGen->pIn->sData;` |
|       - |  2912 | `		/* Advance the stream cursor */` |
| 1138265 |  2913 | `		pGen->pIn++;` |
| 1138265 |  2914 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
| 1138265 |  2915 | `		if( pEntry == 0 ){` |
|       - |  2916 | `			/* Duplicate name */` |
|  163767 |  2917 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  163767 |  2918 | `			if( zName == 0 ){` |
|     ! 0 |  2919 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2920 | `				return SXERR_ABORT;` |
|       - |  2921 | `			}` |
|       - |  2922 | `			/* Install in the hashtable */` |
|  163767 |  2923 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   81886 |  2924 | `		}else{` |
|       - |  2925 | `			/* Name already available */` |
|  974503 |  2926 | `			zName = (char *)pEntry->pUserData;` |
|       - |  2927 | `		}` |
| 1138265 |  2928 | `		p3 = (void *)zName;` |
|       - |  2929 | `	}` |
| 1138277 |  2930 | `	iP1 = 0;` |
| 1138277 |  2931 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  444219 |  2932 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - |  2933 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  444201 |  2934 | `			iP1 = 1;` |
|  222098 |  2935 | `		}` |
|  222107 |  2936 | `	}` |
|       - |  2937 | `	/* Emit the load instruction */` |
| 1138277 |  2938 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
| 1138289 |  2939 | `	while( iVv > 0 ){` |
|      13 |  2940 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 |  2941 | `		iVv--;` |
|       1 |  2942 | `	}` |
|       - |  2943 | `	/* Node successfully compiled */` |
| 1138277 |  2944 | `	return SXRET_OK;` |
|  569143 |  2945 |  |
|       - |  2946 | `/*` |
|       - |  2947 | ` * Load a literal.` |
|       - |  2948 | ` */` |
|  784624 |  2949 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       5 |  2950 |  |
|  784629 |  2951 | `	SyToken *pToken = pGen->pIn;` |
|       - |  2952 | `	ph7_value *pObj;` |
|       - |  2953 | `	SyString *pStr;` |
|       - |  2954 | `	sxu32 nIdx;` |
|       - |  2955 | `	/* Extract token value */` |
|  784629 |  2956 | `	pStr = &pToken->sData;` |
|       - |  2957 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  784629 |  2958 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  166279 |  2959 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - |  2960 | `			/* NULL constant are always indexed at 0 */` |
|   61207 |  2961 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   61207 |  2962 | `			return SXRET_OK;` |
|  105077 |  2963 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - |  2964 | `			/* TRUE constant are always indexed at 1 */` |
|     763 |  2965 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     763 |  2966 | `			return SXRET_OK;` |
|       5 |  2967 | `		}` |
|  723661 |  2968 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  106298 |  2969 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - |  2970 | `			/* FALSE constant are always indexed at 2 */` |
|   46919 |  2971 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   46919 |  2972 | `			return SXRET_OK;` |
|  627150 |  2973 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|  111418 |  2974 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - |  2975 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|   10679 |  2976 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   10679 |  2977 | `			if( pObj == 0 ){` |
|     ! 0 |  2978 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2979 | `				return SXERR_ABORT;` |
|       - |  2980 | `			}` |
|   10679 |  2981 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - |  2982 | `			/* Emit the load constant instruction */` |
|   10679 |  2983 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   10679 |  2984 | `			return SXRET_OK;` |
|  578764 |  2985 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   35994 |  2986 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - |  2987 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       8 |  2988 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       8 |  2989 | `			if( pObj == 0 ){` |
|     ! 0 |  2990 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2991 | `				return SXERR_ABORT;` |
|       - |  2992 | `			}` |
|       8 |  2993 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - |  2994 | `				SyString sNs;` |
|       8 |  2995 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  2996 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       5 |  2997 | `			}else{` |
|     ! 0 |  2998 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  2999 | `			}` |
|       8 |  3000 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       8 |  3001 | `			return SXRET_OK;` |
|  568257 |  3002 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   24639 |  3003 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  570381 |  3004 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   19264 |  3005 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 |  3006 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - |  3007 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 |  3008 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - |  3009 | `				/* Point to the upper block */` |
|      11 |  3010 | `				pBlock = pBlock->pParent;` |
|       1 |  3011 | `			}` |
|      11 |  3012 | `			if( pBlock == 0 ){` |
|       - |  3013 | `				/* Called in the global scope,load NULL */` |
|       5 |  3014 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 |  3015 | `			}else{` |
|       - |  3016 | `				/* Extract the target function/method */` |
|       7 |  3017 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 |  3018 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - |  3019 | `					/* Not a class method,Load null */` |
|       3 |  3020 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  3021 | `				}else{` |
|       5 |  3022 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 |  3023 | `					if( pObj == 0 ){` |
|     ! 0 |  3024 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3025 | `						return SXERR_ABORT;` |
|       - |  3026 | `					}` |
|       5 |  3027 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - |  3028 | `					/* Emit the load constant instruction */` |
|       5 |  3029 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  3030 | `				}` |
|       - |  3031 | `			}` |
|      11 |  3032 | `			return SXRET_OK;` |
|       - |  3033 | `	}` |
|       - |  3034 | `	/* Query literal table */` |
|  665065 |  3035 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - |  3036 | `		ph7_value *pLitObj;` |
|       - |  3037 | `		/* Unknown literal,install it in the literal table */` |
|  283347 |  3038 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  283347 |  3039 | `		if( pLitObj == 0 ){` |
|     ! 0 |  3040 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3041 | `			return SXERR_ABORT;` |
|       - |  3042 | `		}` |
|  283347 |  3043 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  283347 |  3044 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  141671 |  3045 | `	}` |
|       - |  3046 | `	/* Emit the load constant instruction */` |
|  665065 |  3047 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  665065 |  3048 | `	return SXRET_OK;` |
|  392317 |  3049 |  |
|       - |  3050 | `/*` |
|       - |  3051 | ` * Resolve a namespace path or simply load a literal.` |
|       - |  3052 | ` * If the token stream contains namespace separators (backslashes),` |
|       - |  3053 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - |  3054 | ` * Otherwise, load the simple literal directly.` |
|       - |  3055 | ` */` |
|  788220 |  3056 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       5 |  3057 |  |
|       - |  3058 | `	sxi32 rc;` |
|  788225 |  3059 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3060 | `		return SXRET_OK;` |
|       - |  3061 | `	}` |
|       - |  3062 | `	/* Check if this is a multi-token namespace path */` |
|  788225 |  3063 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - |  3064 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|    3601 |  3065 | `		SyBlob *pWorker = &pGen->sWorker;` |
|    3601 |  3066 | `		int isAbsolute = 0;` |
|    3601 |  3067 | `		SyBlobReset(pWorker);` |
|       - |  3068 | `		/* Check for leading backslash (absolute path) */` |
|    3601 |  3069 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|    3599 |  3070 | `			isAbsolute = 1;` |
|    3599 |  3071 | `			pGen->pIn++; /* Skip leading backslash */` |
|    1797 |  3072 | `		}` |
|       - |  3073 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|    3601 |  3074 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 |  3075 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 |  3076 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 |  3077 | `		}` |
|       - |  3078 | `		/* Collect all path components */` |
|    3697 |  3079 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|    3697 |  3080 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      53 |  3081 | `				SyBlobAppend(pWorker,"\\",1);` |
|      29 |  3082 | `			}else{` |
|    3649 |  3083 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  3084 | `			}` |
|    3697 |  3085 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|    3601 |  3086 | `				pGen->pIn++;` |
|    3601 |  3087 | `				break;` |
|       - |  3088 | `			}` |
|     101 |  3089 | `			pGen->pIn++;` |
|       5 |  3090 | `		}` |
|    3601 |  3091 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - |  3092 | `			ph7_value *pObj;` |
|       - |  3093 | `			SyString sPath;` |
|       - |  3094 | `			sxu32 nIdx;` |
|    3601 |  3095 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - |  3096 | `			/* Install in the literal table */` |
|    3601 |  3097 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|    3577 |  3098 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    3577 |  3099 | `				if( pObj == 0 ){` |
|     ! 0 |  3100 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3101 | `					return SXERR_ABORT;` |
|       - |  3102 | `				}` |
|    3577 |  3103 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|    3577 |  3104 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|    1786 |  3105 | `			}` |
|       - |  3106 | `			/* Emit the load constant instruction.` |
|       - |  3107 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|       - |  3108 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|    5399 |  3109 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|    1798 |  3110 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|    1798 |  3111 | `				nIdx,0,0);` |
|    3601 |  3112 | `			return SXRET_OK;` |
|       - |  3113 | `		}` |
|     ! 0 |  3114 | `	}` |
|       - |  3115 | `	/* Single-token literal: load directly */` |
|  784629 |  3116 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  784629 |  3117 | `	return rc;` |
|  394115 |  3118 |  |
|       - |  3119 | `/*` |
|       - |  3120 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - |  3121 | ` */` |
|       - |  3122 | `/*` |
|       - |  3123 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|       - |  3124 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|       - |  3125 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|       - |  3126 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|       - |  3127 | ` */` |
|     ! 0 |  3128 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|     ! 0 |  3129 |  |
|     ! 0 |  3130 | `	SXUNUSED(iCompileFlag);` |
|     ! 0 |  3131 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|       - |  3132 | `		"Cannot use the first-class callable syntax '...' here");` |
|     ! 0 |  3133 | `	return SXERR_SYNTAX;` |
|     ! 0 |  3134 |  |
|  788220 |  3135 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  3136 |  |
|       - |  3137 | `	sxi32 rc;` |
|  788225 |  3138 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  788225 |  3139 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3140 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  3141 | `		return rc;` |
|       - |  3142 | `	}` |
|       - |  3143 | `	/* Node successfully compiled */` |
|  788225 |  3144 | `	return SXRET_OK;` |
|  394115 |  3145 |  |
|       - |  3146 | `/*` |
|       - |  3147 | ` * Recover from a compile-time error. In other words synchronize` |
|       - |  3148 | ` * the token stream cursor with the first semi-colon seen.` |
|       - |  3149 | ` */` |
|       8 |  3150 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 |  3151 |  |
|       - |  3152 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 |  3153 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 |  3154 | `		pGen->pIn++;` |
|       1 |  3155 | `	}` |
|       9 |  3156 | `	return SXRET_OK;` |
|       1 |  3157 |  |
|       - |  3158 | `/*` |
|       - |  3159 | ` * Check if the given identifier name is reserved or not.` |
|       - |  3160 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - |  3161 | ` */` |
|     106 |  3162 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       5 |  3163 |  |
|     111 |  3164 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      29 |  3165 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 |  3166 | `			return TRUE;` |
|      27 |  3167 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       5 |  3168 | `			return TRUE;` |
|       2 |  3169 | `		}` |
|      95 |  3170 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 |  3171 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 |  3172 | `			return TRUE;` |
|       - |  3173 | `		}` |
|     ! 0 |  3174 | `	}` |
|       - |  3175 | `	/* Not a reserved constant */` |
|     103 |  3176 | `	return FALSE;` |
|      58 |  3177 |  |
|       - |  3178 | `/*` |
|       - |  3179 | ` * Compile the 'const' statement.` |
|       - |  3180 | ` * According to the PHP language reference` |
|       - |  3181 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - |  3182 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - |  3183 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - |  3184 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - |  3185 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  3186 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - |  3187 | ` *  Syntax` |
|       - |  3188 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - |  3189 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - |  3190 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - |  3191 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - |  3192 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - |  3193 | ` *  to get a list of all defined constants.` |
|       - |  3194 | ` *` |
|       - |  3195 | ` * Symisc eXtension.` |
|       - |  3196 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - |  3197 | ` *  would allow only simple scalar value.` |
|       - |  3198 | ` *  Example` |
|       - |  3199 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  3200 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  3201 | ` */` |
|      32 |  3202 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       5 |  3203 |  |
|       - |  3204 | `	SySet *pConsCode,*pInstrContainer;` |
|      37 |  3205 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  3206 | `	SyString *pName;` |
|       - |  3207 | `	sxi32 rc;` |
|      37 |  3208 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      37 |  3209 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  3210 | `		/* Invalid constant name */` |
|       8 |  3211 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       8 |  3212 | `		if( rc == SXERR_ABORT ){` |
|       - |  3213 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3214 | `			return SXERR_ABORT;` |
|       - |  3215 | `		}` |
|       8 |  3216 | `		goto Synchronize;` |
|       - |  3217 | `	}` |
|       - |  3218 | `	/* Peek constant name */` |
|      30 |  3219 | `	pName = &pGen->pIn->sData;` |
|       - |  3220 | `	/* Make sure the constant name isn't reserved */` |
|      30 |  3221 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  3222 | `		/* Reserved constant */` |
|       9 |  3223 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       9 |  3224 | `		if( rc == SXERR_ABORT ){` |
|       - |  3225 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3226 | `			return SXERR_ABORT;` |
|       - |  3227 | `		}` |
|       9 |  3228 | `		goto Synchronize;` |
|       - |  3229 | `	}` |
|      21 |  3230 | `	pGen->pIn++;` |
|      21 |  3231 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  3232 | `		/* Invalid statement*/` |
|       6 |  3233 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       6 |  3234 | `		if( rc == SXERR_ABORT ){` |
|       - |  3235 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3236 | `			return SXERR_ABORT;` |
|       - |  3237 | `		}` |
|       6 |  3238 | `		goto Synchronize;` |
|       - |  3239 | `	}` |
|      15 |  3240 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - |  3241 | `	/* Allocate a new constant value container */` |
|      15 |  3242 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      15 |  3243 | `	if( pConsCode == 0 ){` |
|     ! 0 |  3244 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3245 | `		return SXERR_ABORT;` |
|       - |  3246 | `	}` |
|      15 |  3247 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  3248 | `	/* Swap bytecode container */` |
|      15 |  3249 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      15 |  3250 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - |  3251 | `	/* Compile constant value */` |
|      15 |  3252 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3253 | `	/* Emit the done instruction */` |
|      15 |  3254 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      15 |  3255 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      15 |  3256 | `	if( rc == SXERR_ABORT ){` |
|       - |  3257 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  3258 | `		return SXERR_ABORT;` |
|       - |  3259 | `	}` |
|      15 |  3260 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - |  3261 | `	/* Register the constant with namespace-qualified name */` |
|       - |  3262 | `	{` |
|       - |  3263 | `		SyBlob sFQN;` |
|       - |  3264 | `		SyString sFQNStr;` |
|      15 |  3265 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      15 |  3266 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      15 |  3267 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      15 |  3268 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      15 |  3269 | `		SyBlobRelease(&sFQN);` |
|       - |  3270 | `	}` |
|      15 |  3271 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3272 | `		SySetRelease(pConsCode);` |
|     ! 0 |  3273 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 |  3274 | `	}` |
|      15 |  3275 | `	return SXRET_OK;` |
|       9 |  3276 | `Synchronize:` |
|       - |  3277 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      60 |  3278 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      41 |  3279 | `		pGen->pIn++;` |
|       3 |  3280 | `	}` |
|      22 |  3281 | `	return SXRET_OK;` |
|      21 |  3282 |  |
|       - |  3283 | `/*` |
|       - |  3284 | ` * Compile the 'continue' statement.` |
|       - |  3285 | ` * According to the PHP language reference` |
|       - |  3286 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - |  3287 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - |  3288 | ` *  iteration.` |
|       - |  3289 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - |  3290 | ` *  the purposes of continue.` |
|       - |  3291 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - |  3292 | ` *  of enclosing loops it should skip to the end of.` |
|       - |  3293 | ` *  Note:` |
|       - |  3294 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - |  3295 | ` */` |
|       - |  3296 | `/*` |
|       - |  3297 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - |  3298 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - |  3299 | ` * break/continue crosses a try boundary.` |
|       - |  3300 | ` *` |
|       - |  3301 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - |  3302 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - |  3303 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - |  3304 | ` */` |
|    3696 |  3305 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       5 |  3306 |  |
|    3701 |  3307 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   21689 |  3308 | `	while( pBlock && pBlock != pTarget ){` |
|   17993 |  3309 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       3 |  3310 | `			if( pBlock->pUserData ){` |
|       - |  3311 | `				/* This is a try block with an exception context — emit POP_EXCEPTION */` |
|       3 |  3312 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       2 |  3313 | `			}else{` |
|       - |  3314 | `				/* This is a catch/finally block compiled into a separate bytecode` |
|       - |  3315 | `				 * container. Stop here — we cannot cross into the parent try's` |
|       - |  3316 | `				 * exception context from a sub-execution.` |
|       - |  3317 | `				 */` |
|     ! 0 |  3318 | `				break;` |
|       - |  3319 | `			}` |
|       1 |  3320 | `		}` |
|   17993 |  3321 | `		pBlock = pBlock->pParent;` |
|       5 |  3322 | `	}` |
|    3701 |  3323 |  |
|    3600 |  3324 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       5 |  3325 |  |
|       - |  3326 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3327 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3328 | `	sxu32 nLineLocal;` |
|       - |  3329 | `	sxi32 rc;` |
|    3605 |  3330 | `	nLineLocal = pGen->pIn->nLine;` |
|    3605 |  3331 | `	iLevel = 0;` |
|       - |  3332 | `	/* Jump the 'continue' keyword */` |
|    3605 |  3333 | `	pGen->pIn++;` |
|    3605 |  3334 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3335 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3336 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3337 | `		 */` |
|       - |  3338 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      17 |  3339 | `		char *zAlloc = 0;` |
|       - |  3340 | `		SyString sNum;` |
|      17 |  3341 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      17 |  3342 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3343 | `			return SXERR_ABORT;` |
|       - |  3344 | `		}` |
|      17 |  3345 | `		if( rc == SXRET_OK ){` |
|      20 |  3346 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3347 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3348 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3349 | `				return SXERR_ABORT;` |
|       - |  3350 | `			}` |
|      14 |  3351 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3352 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3353 | `		}` |
|      17 |  3354 | `		if( iLevel < 2 ){` |
|       3 |  3355 | `			iLevel = 0;` |
|       1 |  3356 | `		}` |
|      17 |  3357 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3358 | `	}` |
|       - |  3359 | `	/* Point to the target loop */` |
|    3605 |  3360 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    3605 |  3361 | `	if( pLoop == 0 ){` |
|       - |  3362 | `		/* Illegal continue */` |
|      12 |  3363 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      12 |  3364 | `		if( rc == SXERR_ABORT ){` |
|       - |  3365 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3366 | `			return SXERR_ABORT;` |
|       - |  3367 | `		}` |
|       7 |  3368 | `	}else{` |
|    3595 |  3369 | `		sxu32 nInstrIdx = 0;` |
|       - |  3370 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    3595 |  3371 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    3595 |  3372 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - |  3373 | `			/* According to the PHP language reference manual` |
|       - |  3374 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - |  3375 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - |  3376 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - |  3377 | `			 */` |
|       5 |  3378 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 |  3379 | `			if( rc == SXRET_OK ){` |
|       5 |  3380 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 |  3381 | `			}` |
|       3 |  3382 | `		}else{` |
|       - |  3383 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    3591 |  3384 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    3591 |  3385 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  3386 | `				JumpFixup sJumpFix;` |
|       - |  3387 | `				/* Post-continue */` |
|      14 |  3388 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 |  3389 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 |  3390 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 |  3391 | `			}` |
|       - |  3392 | `		}` |
|       - |  3393 | `	}` |
|    3605 |  3394 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3395 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3396 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  3397 | `	}` |
|       - |  3398 | `	/* Statement successfully compiled */` |
|    3605 |  3399 | `	return SXRET_OK;` |
|    1805 |  3400 |  |
|       - |  3401 | `/*` |
|       - |  3402 | ` * Compile the 'break' statement.` |
|       - |  3403 | ` * According to the PHP language reference` |
|       - |  3404 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - |  3405 | ` *  structure.` |
|       - |  3406 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - |  3407 | ` *  enclosing structures are to be broken out of.` |
|       - |  3408 | ` */` |
|     122 |  3409 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       5 |  3410 |  |
|       - |  3411 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3412 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3413 | `	sxi32 rc;` |
|     127 |  3414 | `	iLevel = 0;` |
|       - |  3415 | `	/* Jump the 'break' keyword */` |
|     127 |  3416 | `	pGen->pIn++;` |
|     127 |  3417 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3418 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3419 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3420 | `		 */` |
|       - |  3421 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      17 |  3422 | `		char *zAlloc = 0;` |
|       - |  3423 | `		SyString sNum;` |
|      17 |  3424 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      17 |  3425 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3426 | `			return SXERR_ABORT;` |
|       - |  3427 | `		}` |
|      17 |  3428 | `		if( rc == SXRET_OK ){` |
|      20 |  3429 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3430 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3431 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3432 | `				return SXERR_ABORT;` |
|       - |  3433 | `			}` |
|      14 |  3434 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3435 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3436 | `		}` |
|      17 |  3437 | `		if( iLevel < 2 ){` |
|       3 |  3438 | `			iLevel = 0;` |
|       1 |  3439 | `		}` |
|      17 |  3440 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3441 | `	}` |
|       - |  3442 | `	/* Extract the target loop */` |
|     127 |  3443 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     127 |  3444 | `	if( pLoop == 0 ){` |
|       - |  3445 | `		/* Illegal break */` |
|      18 |  3446 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      18 |  3447 | `		if( rc == SXERR_ABORT ){` |
|       - |  3448 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3449 | `			return SXERR_ABORT;` |
|       - |  3450 | `		}` |
|      10 |  3451 | `	}else{` |
|       - |  3452 | `		sxu32 nInstrIdx;` |
|       - |  3453 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|     111 |  3454 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|     111 |  3455 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|     111 |  3456 | `		if( rc == SXRET_OK ){` |
|       - |  3457 | `			/* Fix the jump later when the jump destination is resolved */` |
|     111 |  3458 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      53 |  3459 | `		}` |
|       - |  3460 | `	}` |
|     127 |  3461 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3462 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3463 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 |  3464 | `	}` |
|       - |  3465 | `	/* Statement successfully compiled */` |
|     127 |  3466 | `	return SXRET_OK;` |
|      66 |  3467 |  |
|       - |  3468 | `/*` |
|       - |  3469 | ` * Compile or record a label.` |
|       - |  3470 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - |  3471 | ` * Example` |
|       - |  3472 | ` *  goto LABEL;` |
|       - |  3473 | ` *   echo 'Foo';` |
|       - |  3474 | ` *  LABEL:` |
|       - |  3475 | ` *   echo 'Bar';` |
|       - |  3476 | ` */` |
|     112 |  3477 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       5 |  3478 |  |
|       - |  3479 | `	GenBlock *pBlock;` |
|       - |  3480 | `	Label sLabel;` |
|       - |  3481 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     117 |  3482 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     117 |  3483 | `	if( pBlock ){` |
|       - |  3484 | `		sxi32 rc;` |
|       8 |  3485 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 |  3486 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       6 |  3487 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3488 | `			return SXERR_ABORT;` |
|       - |  3489 | `		}` |
|       4 |  3490 | `	}else{` |
|     113 |  3491 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3492 | `		char *zDup;` |
|       - |  3493 | `		/* Initialize label fields */` |
|     113 |  3494 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - |  3495 | `		/* Duplicate label name */` |
|     113 |  3496 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     113 |  3497 | `		if( zDup == 0 ){` |
|     ! 0 |  3498 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3499 | `			return SXERR_ABORT;` |
|       - |  3500 | `		}` |
|     113 |  3501 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     113 |  3502 | `		sLabel.bRef  = FALSE;` |
|     113 |  3503 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     113 |  3504 | `		pBlock = pGen->pCurrent;` |
|     221 |  3505 | `		while( pBlock ){` |
|     133 |  3506 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      23 |  3507 | `				break;` |
|       - |  3508 | `			}` |
|       - |  3509 | `			/* Point to the upper block */` |
|     113 |  3510 | `			pBlock = pBlock->pParent;` |
|       5 |  3511 | `		}` |
|     113 |  3512 | `		if( pBlock ){` |
|      23 |  3513 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      13 |  3514 | `		}else{` |
|      93 |  3515 | `			sLabel.pFunc = 0;` |
|       - |  3516 | `		}` |
|       - |  3517 | `		/* Insert in label set */` |
|     113 |  3518 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - |  3519 | `	}` |
|     117 |  3520 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     117 |  3521 | `	return SXRET_OK;` |
|      61 |  3522 |  |
|       - |  3523 | `/*` |
|       - |  3524 | ` * Compile the so hated 'goto' statement.` |
|       - |  3525 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - |  3526 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - |  3527 | ` * a compiler it has to do this.` |
|       - |  3528 | ` * According to the PHP language reference manual` |
|       - |  3529 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - |  3530 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - |  3531 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - |  3532 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - |  3533 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - |  3534 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - |  3535 | ` *   of a multi-level break` |
|       - |  3536 | ` */` |
|     152 |  3537 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       5 |  3538 |  |
|       - |  3539 | `	JumpFixup sJump;` |
|       - |  3540 | `	sxi32 rc;` |
|     157 |  3541 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     157 |  3542 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  3543 | `		/* Missing label */` |
|     ! 0 |  3544 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 |  3545 | `		if( rc == SXERR_ABORT ){` |
|       - |  3546 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3547 | `			return SXERR_ABORT;` |
|       - |  3548 | `		}` |
|     ! 0 |  3549 | `		return SXRET_OK;` |
|       - |  3550 | `	}` |
|     157 |  3551 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       6 |  3552 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       6 |  3553 | `		if( rc == SXERR_ABORT ){` |
|       - |  3554 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3555 | `			return SXERR_ABORT;` |
|       - |  3556 | `		}` |
|       4 |  3557 | `	}else{` |
|     153 |  3558 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3559 | `		GenBlock *pBlock;` |
|       - |  3560 | `		char *zDup;` |
|       - |  3561 | `		/* Prepare the jump destination */` |
|     153 |  3562 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     153 |  3563 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - |  3564 | `		/* Duplicate label name */` |
|     153 |  3565 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     153 |  3566 | `		if( zDup == 0 ){` |
|     ! 0 |  3567 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3568 | `			return SXERR_ABORT;` |
|       - |  3569 | `		}` |
|     153 |  3570 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     153 |  3571 | `		pBlock = pGen->pCurrent;` |
|     315 |  3572 | `		while( pBlock ){` |
|     199 |  3573 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      36 |  3574 | `				break;` |
|       - |  3575 | `			}` |
|       - |  3576 | `			/* Point to the upper block */` |
|     167 |  3577 | `			pBlock = pBlock->pParent;` |
|       5 |  3578 | `		}` |
|     153 |  3579 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       8 |  3580 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       8 |  3581 | `			if( rc == SXERR_ABORT ){` |
|       - |  3582 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3583 | `				return SXERR_ABORT;` |
|       - |  3584 | `			}` |
|       3 |  3585 | `		}` |
|     153 |  3586 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      30 |  3587 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      17 |  3588 | `		}else{` |
|     127 |  3589 | `			sJump.pFunc = 0;` |
|       - |  3590 | `		}` |
|       - |  3591 | `		/* Emit the unconditional jump */` |
|     153 |  3592 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     153 |  3593 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 |  3594 | `		}` |
|       - |  3595 | `	}` |
|     157 |  3596 | `	pGen->pIn++; /* Jump the label name */` |
|     157 |  3597 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 |  3598 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 |  3599 | `	}` |
|       - |  3600 | `	/* Statement successfully compiled */` |
|     157 |  3601 | `	return SXRET_OK;` |
|      81 |  3602 |  |
|       - |  3603 | `/*` |
|       - |  3604 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - |  3605 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - |  3606 | ` * failure.` |
|       - |  3607 | ` */` |
|      20 |  3608 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       1 |  3609 |  |
|       - |  3610 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - |  3611 | `	sxu32 nRawObj;` |
|      10 |  3612 | `	sxu32 nObjIdx;` |
|       - |  3613 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - |  3614 | `	 * a PHP block.` |
|       - |  3615 | `	 */` |
|      10 |  3616 | `Consume:` |
|      21 |  3617 | `	nRawObj = nObjIdx = 0;` |
|      21 |  3618 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 |  3619 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 |  3620 | `		if( pRawObj == 0 ){` |
|     ! 0 |  3621 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3622 | `			return SXERR_ABORT;` |
|       - |  3623 | `		}` |
|       - |  3624 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 |  3625 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 |  3626 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 |  3627 | `		++nRawObj;` |
|     ! 0 |  3628 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 |  3629 | `	}` |
|      21 |  3630 | `	if( nRawObj > 0 ){` |
|       - |  3631 | `		/* Emit the consume instruction */` |
|     ! 0 |  3632 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 |  3633 | `	}` |
|      21 |  3634 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 |  3635 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - |  3636 | `		/* Reset the token set */` |
|     ! 0 |  3637 | `		SySetReset(pTokenSet);` |
|       - |  3638 | `		/* Tokenize input */` |
|     ! 0 |  3639 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 |  3640 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - |  3641 | `		/* Point to the fresh token stream */` |
|     ! 0 |  3642 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 |  3643 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - |  3644 | `		/* Advance the stream cursor */` |
|     ! 0 |  3645 | `		pGen->pRawIn++;` |
|       - |  3646 | `		/* TICKET 1433-011 */` |
|     ! 0 |  3647 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - |  3648 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - |  3649 | `			sxi32 rc;` |
|       - |  3650 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 |  3651 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 |  3652 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 |  3653 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 |  3654 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 |  3655 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3656 | `				return SXERR_ABORT;` |
|     ! 0 |  3657 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 |  3658 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  3659 | `			}` |
|     ! 0 |  3660 | `			goto Consume;` |
|       - |  3661 | `		}` |
|     ! 0 |  3662 | `	}else{` |
|       - |  3663 | `		/* No more chunks to process */` |
|      21 |  3664 | `		pGen->pIn = pGen->pEnd;` |
|      21 |  3665 | `		return SXERR_EOF;` |
|       - |  3666 | `	}` |
|     ! 0 |  3667 | `	return SXRET_OK;` |
|      11 |  3668 |  |
|       - |  3669 | `/*` |
|       - |  3670 | ` * Compile a PHP block.` |
|       - |  3671 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - |  3672 | ` * optionally delimited by braces {}.` |
|       - |  3673 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  3674 | ` * and this function takes care of generating the appropriate error` |
|       - |  3675 | ` * message.` |
|       - |  3676 | ` */` |
|  431996 |  3677 | `static sxi32 PH7_CompileBlock(` |
|       - |  3678 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  3679 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  3680 | `	)` |
|       5 |  3681 |  |
|       - |  3682 | `	sxi32 rc;` |
|       - |  3683 | `	sxu32 nLine;` |
|  432001 |  3684 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  430317 |  3685 | `		nLine = pGen->pIn->nLine;` |
|  430317 |  3686 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  430317 |  3687 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3688 | `			return SXERR_ABORT;` |
|       - |  3689 | `		}` |
|  430317 |  3690 | `		pGen->pIn++;` |
|       - |  3691 | `		/* Compile until we hit the closing braces '}' */` |
|  589316 |  3692 | `		for(;;){` |
| 1178637 |  3693 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      21 |  3694 | `				rc = GenStateNextChunk(&(*pGen));` |
|      21 |  3695 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3696 | `			 	   return SXERR_ABORT;` |
|       - |  3697 | `				}` |
|      21 |  3698 | `				if( rc == SXERR_EOF ){` |
|       - |  3699 | `					/* No more token to process. Missing closing braces */` |
|      21 |  3700 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      21 |  3701 | `					break;` |
|       - |  3702 | `				}` |
|     ! 0 |  3703 | `			}` |
| 1178617 |  3704 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  3705 | `				/* Closing braces found,break immediately*/` |
|  430297 |  3706 | `				pGen->pIn++;` |
|  430297 |  3707 | `				break;` |
|       - |  3708 | `			}` |
|       - |  3709 | `			/* Compile a single statement */` |
|  748325 |  3710 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  748325 |  3711 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3712 | `				return SXERR_ABORT;` |
|       - |  3713 | `			}` |
|       5 |  3714 | `		}` |
|  430317 |  3715 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  216845 |  3716 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 |  3717 | `		pGen->pIn++;` |
|     ! 0 |  3718 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 |  3719 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3720 | `			return SXERR_ABORT;` |
|       - |  3721 | `		}` |
|       - |  3722 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 |  3723 | `		for(;;){` |
|     ! 0 |  3724 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3725 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 |  3726 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3727 | `			 	   return SXERR_ABORT;` |
|       - |  3728 | `				}` |
|     ! 0 |  3729 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - |  3730 | `					/* No more token to process */` |
|     ! 0 |  3731 | `					if( rc == SXERR_EOF ){` |
|     ! 0 |  3732 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - |  3733 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 |  3734 | `					}` |
|     ! 0 |  3735 | `					break;` |
|       - |  3736 | `				}` |
|     ! 0 |  3737 | `			}` |
|     ! 0 |  3738 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  3739 | `				sxi32 nKwrd;` |
|       - |  3740 | `				/* Keyword found */` |
|     ! 0 |  3741 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  3742 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 |  3743 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - |  3744 | `						/* Delimiter keyword found,break */` |
|     ! 0 |  3745 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 |  3746 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 |  3747 | `						}` |
|     ! 0 |  3748 | `						break;` |
|       - |  3749 | `				}` |
|     ! 0 |  3750 | `			}` |
|       - |  3751 | `			/* Compile a single statement */` |
|     ! 0 |  3752 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 |  3753 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3754 | `				return SXERR_ABORT;` |
|       - |  3755 | `			}` |
|     ! 0 |  3756 | `		}` |
|     ! 0 |  3757 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  3758 | `	}else{` |
|       - |  3759 | `		/* Compile a single statement */` |
|    1689 |  3760 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1689 |  3761 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3762 | `			return SXERR_ABORT;` |
|       - |  3763 | `		}` |
|       - |  3764 | `	}` |
|       - |  3765 | `	/* Jump trailing semi-colons ';' */` |
|  432001 |  3766 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3767 | `		pGen->pIn++;` |
|     ! 0 |  3768 | `	}` |
|  432001 |  3769 | `	return SXRET_OK;` |
|  216003 |  3770 |  |
|       - |  3771 | `/*` |
|       - |  3772 | ` * Compile the gentle 'while' statement.` |
|       - |  3773 | ` * According to the PHP language reference` |
|       - |  3774 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - |  3775 | ` *  The basic form of a while statement is:` |
|       - |  3776 | ` *  while (expr)` |
|       - |  3777 | ` *   statement` |
|       - |  3778 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - |  3779 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - |  3780 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - |  3781 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - |  3782 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - |  3783 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - |  3784 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - |  3785 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - |  3786 | ` *  while (expr):` |
|       - |  3787 | ` *    statement` |
|       - |  3788 | ` *   endwhile;` |
|       - |  3789 | ` */` |
|   14344 |  3790 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       5 |  3791 |  |
|   14349 |  3792 | `	GenBlock *pWhileBlock = 0;` |
|   14349 |  3793 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  3794 | `	sxu32 nFalseJump;` |
|       - |  3795 | `	sxu32 nLine;` |
|       - |  3796 | `	sxi32 rc;` |
|   14349 |  3797 | `	nLine = pGen->pIn->nLine;` |
|       - |  3798 | `	/* Jump the 'while' keyword */` |
|   14349 |  3799 | `	pGen->pIn++;` |
|   14349 |  3800 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3801 | `		/* Syntax error */` |
|     ! 0 |  3802 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3803 | `		if( rc == SXERR_ABORT ){` |
|       - |  3804 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3805 | `			return SXERR_ABORT;` |
|       - |  3806 | `		}` |
|     ! 0 |  3807 | `		goto Synchronize;` |
|       - |  3808 | `	}` |
|       - |  3809 | `	/* Jump the left parenthesis '(' */` |
|   14349 |  3810 | `	pGen->pIn++;` |
|       - |  3811 | `	/* Create the loop block */` |
|   14349 |  3812 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   14349 |  3813 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3814 | `		return SXERR_ABORT;` |
|       - |  3815 | `	}` |
|       - |  3816 | `	/* Delimit the condition */` |
|   14349 |  3817 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   14349 |  3818 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3819 | `		/* Empty expression */` |
|       3 |  3820 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 |  3821 | `		if( rc == SXERR_ABORT ){` |
|       - |  3822 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3823 | `			return SXERR_ABORT;` |
|       - |  3824 | `		}` |
|       1 |  3825 | `	}` |
|       - |  3826 | `	/* Swap token streams */` |
|   14349 |  3827 | `	pTmp = pGen->pEnd;` |
|   14349 |  3828 | `	pGen->pEnd = pEnd;` |
|       - |  3829 | `	/* Compile the expression */` |
|   14349 |  3830 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14349 |  3831 | `	if( rc == SXERR_ABORT ){` |
|       - |  3832 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3833 | `		return SXERR_ABORT;` |
|       - |  3834 | `	}` |
|       - |  3835 | `	/* Update token stream */` |
|   14349 |  3836 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3837 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3838 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3839 | `			return SXERR_ABORT;` |
|       - |  3840 | `		}` |
|     ! 0 |  3841 | `		pGen->pIn++;` |
|     ! 0 |  3842 | `	}` |
|       - |  3843 | `	/* Synchronize pointers */` |
|   14349 |  3844 | `	pGen->pIn  = &pEnd[1];` |
|   14349 |  3845 | `	pGen->pEnd = pTmp;` |
|       - |  3846 | `	/* Emit the false jump */` |
|   14349 |  3847 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3848 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   14349 |  3849 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  3850 | `	/* Compile the loop body */` |
|   14349 |  3851 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   14349 |  3852 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3853 | `		return SXERR_ABORT;` |
|       - |  3854 | `	}` |
|       - |  3855 | `	/* Emit the unconditional jump to the start of the loop */` |
|   14349 |  3856 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  3857 | `	/* Fix all jumps now the destination is resolved */` |
|   14349 |  3858 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3859 | `	/* Release the loop block */` |
|   14349 |  3860 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3861 | `	/* Statement successfully compiled */` |
|   14349 |  3862 | `	return SXRET_OK;` |
|     ! 0 |  3863 | `Synchronize:` |
|       - |  3864 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3865 | `	 * compiling this erroneous block.` |
|       - |  3866 | `	 */` |
|     ! 0 |  3867 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3868 | `		pGen->pIn++;` |
|     ! 0 |  3869 | `	}` |
|     ! 0 |  3870 | `	return SXRET_OK;` |
|    7177 |  3871 |  |
|       - |  3872 | `/*` |
|       - |  3873 | ` * Compile the ugly do..while() statement.` |
|       - |  3874 | ` * According to the PHP language reference` |
|       - |  3875 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - |  3876 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - |  3877 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - |  3878 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - |  3879 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - |  3880 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - |  3881 | ` *  would end immediately).` |
|       - |  3882 | ` *  There is just one syntax for do-while loops:` |
|       - |  3883 | ` *  <?php` |
|       - |  3884 | ` *  $i = 0;` |
|       - |  3885 | ` *  do {` |
|       - |  3886 | ` *   echo $i;` |
|       - |  3887 | ` *  } while ($i > 0);` |
|       - |  3888 | ` * ?>` |
|       - |  3889 | ` */` |
|       2 |  3890 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 |  3891 |  |
|       3 |  3892 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 |  3893 | `	GenBlock *pDoBlock = 0;` |
|       - |  3894 | `	sxu32 nLine;` |
|       - |  3895 | `	sxi32 rc;` |
|       3 |  3896 | `	nLine = pGen->pIn->nLine;` |
|       - |  3897 | `	/* Jump the 'do' keyword */` |
|       3 |  3898 | `	pGen->pIn++;` |
|       - |  3899 | `	/* Create the loop block */` |
|       3 |  3900 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 |  3901 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3902 | `		return SXERR_ABORT;` |
|       - |  3903 | `	}` |
|       - |  3904 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 |  3905 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 |  3906 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 |  3907 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3908 | `		return SXERR_ABORT;` |
|       - |  3909 | `	}` |
|       3 |  3910 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3911 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 |  3912 | `	}` |
|       3 |  3913 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 |  3914 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - |  3915 | `			/* Missing 'while' statement */` |
|       3 |  3916 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 |  3917 | `			if( rc == SXERR_ABORT ){` |
|       - |  3918 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3919 | `				return SXERR_ABORT;` |
|       - |  3920 | `			}` |
|       3 |  3921 | `			goto Synchronize;` |
|       - |  3922 | `	}` |
|       - |  3923 | `	/* Jump the 'while' keyword */` |
|     ! 0 |  3924 | `	pGen->pIn++;` |
|     ! 0 |  3925 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3926 | `		/* Syntax error */` |
|     ! 0 |  3927 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3928 | `		if( rc == SXERR_ABORT ){` |
|       - |  3929 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3930 | `			return SXERR_ABORT;` |
|       - |  3931 | `		}` |
|     ! 0 |  3932 | `		goto Synchronize;` |
|       - |  3933 | `	}` |
|       - |  3934 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 |  3935 | `	pGen->pIn++;` |
|       - |  3936 | `	/* Delimit the condition */` |
|     ! 0 |  3937 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 |  3938 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3939 | `		/* Empty expression */` |
|     ! 0 |  3940 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 |  3941 | `		if( rc == SXERR_ABORT ){` |
|       - |  3942 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3943 | `			return SXERR_ABORT;` |
|       - |  3944 | `		}` |
|     ! 0 |  3945 | `		goto Synchronize;` |
|       - |  3946 | `	}` |
|       - |  3947 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 |  3948 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - |  3949 | `		JumpFixup *aPost;` |
|       - |  3950 | `		VmInstr *pInstr;` |
|       - |  3951 | `		sxu32 nJumpDest;` |
|       - |  3952 | `		sxu32 n;` |
|     ! 0 |  3953 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 |  3954 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 |  3955 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 |  3956 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 |  3957 | `			if( pInstr ){` |
|       - |  3958 | `				/* Fix */` |
|     ! 0 |  3959 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 |  3960 | `			}` |
|     ! 0 |  3961 | `		}` |
|     ! 0 |  3962 | `	}` |
|       - |  3963 | `	/* Swap token streams */` |
|     ! 0 |  3964 | `	pTmp = pGen->pEnd;` |
|     ! 0 |  3965 | `	pGen->pEnd = pEnd;` |
|       - |  3966 | `	/* Compile the expression */` |
|     ! 0 |  3967 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  3968 | `	if( rc == SXERR_ABORT ){` |
|       - |  3969 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3970 | `		return SXERR_ABORT;` |
|       - |  3971 | `	}` |
|       - |  3972 | `	/* Update token stream */` |
|     ! 0 |  3973 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3974 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3975 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3976 | `			return SXERR_ABORT;` |
|       - |  3977 | `		}` |
|     ! 0 |  3978 | `		pGen->pIn++;` |
|     ! 0 |  3979 | `	}` |
|     ! 0 |  3980 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 |  3981 | `	pGen->pEnd = pTmp;` |
|       - |  3982 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 |  3983 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - |  3984 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 |  3985 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3986 | `	/* Release the loop block */` |
|     ! 0 |  3987 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3988 | `	/* Statement successfully compiled */` |
|     ! 0 |  3989 | `	return SXRET_OK;` |
|       1 |  3990 | `Synchronize:` |
|       - |  3991 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3992 | `	 * compiling this erroneous block.` |
|       - |  3993 | `	 */` |
|       3 |  3994 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3995 | `		pGen->pIn++;` |
|     ! 0 |  3996 | `	}` |
|       3 |  3997 | `	return SXRET_OK;` |
|       2 |  3998 |  |
|       - |  3999 | `/*` |
|       - |  4000 | ` * Compile the complex and powerful 'for' statement.` |
|       - |  4001 | ` * According to the PHP language reference` |
|       - |  4002 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - |  4003 | ` *  The syntax of a for loop is:` |
|       - |  4004 | ` *  for (expr1; expr2; expr3)` |
|       - |  4005 | ` *   statement` |
|       - |  4006 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - |  4007 | ` *  the beginning of the loop.` |
|       - |  4008 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - |  4009 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - |  4010 | ` *  to FALSE, the execution of the loop ends.` |
|       - |  4011 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - |  4012 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - |  4013 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - |  4014 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - |  4015 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - |  4016 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - |  4017 | ` *  of using the for truth expression.` |
|       - |  4018 | ` */` |
|   14344 |  4019 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       5 |  4020 |  |
|   14349 |  4021 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   14349 |  4022 | `	GenBlock *pForBlock = 0;` |
|       - |  4023 | `	sxu32 nFalseJump;` |
|       - |  4024 | `	sxu32 nLine;` |
|       - |  4025 | `	sxi32 rc;` |
|   14349 |  4026 | `	nLine = pGen->pIn->nLine;` |
|       - |  4027 | `	/* Jump the 'for' keyword */` |
|   14349 |  4028 | `	pGen->pIn++;` |
|   14349 |  4029 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4030 | `		/* Syntax error */` |
|     ! 0 |  4031 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 |  4032 | `		if( rc == SXERR_ABORT ){` |
|       - |  4033 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4034 | `			return SXERR_ABORT;` |
|       - |  4035 | `		}` |
|     ! 0 |  4036 | `		return SXRET_OK;` |
|       - |  4037 | `	}` |
|       - |  4038 | `	/* Jump the left parenthesis '(' */` |
|   14349 |  4039 | `	pGen->pIn++;` |
|       - |  4040 | `	/* Delimit the init-expr;condition;post-expr */` |
|   14349 |  4041 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   14349 |  4042 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4043 | `		/* Empty expression */` |
|     ! 0 |  4044 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 |  4045 | `		if( rc == SXERR_ABORT ){` |
|       - |  4046 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4047 | `			return SXERR_ABORT;` |
|       - |  4048 | `		}` |
|       - |  4049 | `		/* Synchronize */` |
|     ! 0 |  4050 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4051 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4052 | `			pGen->pIn++;` |
|     ! 0 |  4053 | `		}` |
|     ! 0 |  4054 | `		return SXRET_OK;` |
|       - |  4055 | `	}` |
|       - |  4056 | `	/* Swap token streams */` |
|   14349 |  4057 | `	pTmp = pGen->pEnd;` |
|   14349 |  4058 | `	pGen->pEnd = pEnd;` |
|       - |  4059 | `	/* Compile initialization expressions if available */` |
|   14349 |  4060 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4061 | `	/* Pop operand lvalues */` |
|   14349 |  4062 | `	if( rc == SXERR_ABORT ){` |
|       - |  4063 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4064 | `		return SXERR_ABORT;` |
|   14349 |  4065 | `	}else if( rc != SXERR_EMPTY ){` |
|   14347 |  4066 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    7171 |  4067 | `	}` |
|   14349 |  4068 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4069 | `		/* Syntax error */` |
|     ! 0 |  4070 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  4071 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 |  4072 | `		if( rc == SXERR_ABORT ){` |
|       - |  4073 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4074 | `			return SXERR_ABORT;` |
|       - |  4075 | `		}` |
|     ! 0 |  4076 | `		return SXRET_OK;` |
|       - |  4077 | `	}` |
|       - |  4078 | `	/* Jump the trailing ';' */` |
|   14349 |  4079 | `	pGen->pIn++;` |
|       - |  4080 | `	/* Create the loop block */` |
|   14349 |  4081 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   14349 |  4082 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4083 | `		return SXERR_ABORT;` |
|       - |  4084 | `	}` |
|       - |  4085 | `	/* Deffer continue jumps */` |
|   14349 |  4086 | `	pForBlock->bPostContinue = TRUE;` |
|       - |  4087 | `	/* Compile the condition */` |
|   14349 |  4088 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14349 |  4089 | `	if( rc == SXERR_ABORT ){` |
|       - |  4090 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4091 | `		return SXERR_ABORT;` |
|   14349 |  4092 | `	}else if( rc != SXERR_EMPTY ){` |
|       - |  4093 | `		/* Emit the false jump */` |
|   14347 |  4094 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  4095 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   14347 |  4096 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    7171 |  4097 | `	}` |
|   14349 |  4098 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4099 | `		/* Syntax error */` |
|       6 |  4100 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  4101 | `			"for: Expected ';' after conditionals expressions");` |
|       6 |  4102 | `		if( rc == SXERR_ABORT ){` |
|       - |  4103 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4104 | `			return SXERR_ABORT;` |
|       - |  4105 | `		}` |
|       6 |  4106 | `		return SXRET_OK;` |
|       - |  4107 | `	}` |
|       - |  4108 | `	/* Jump the trailing ';' */` |
|   14345 |  4109 | `	pGen->pIn++;` |
|       - |  4110 | `	/* Save the post condition stream */` |
|   14345 |  4111 | `	pPostStart = pGen->pIn;` |
|       - |  4112 | `	/* Compile the loop body */` |
|   14345 |  4113 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   14345 |  4114 | `	pGen->pEnd = pTmp;` |
|   14345 |  4115 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   14345 |  4116 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  4117 | `		return SXERR_ABORT;` |
|       - |  4118 | `	}` |
|       - |  4119 | `	/* Fix post-continue jumps */` |
|   14345 |  4120 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - |  4121 | `		JumpFixup *aPost;` |
|       - |  4122 | `		VmInstr *pInstr;` |
|       - |  4123 | `		sxu32 nJumpDest;` |
|       - |  4124 | `		sxu32 n;` |
|      14 |  4125 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      14 |  4126 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      26 |  4127 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|      14 |  4128 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      14 |  4129 | `			if( pInstr ){` |
|       - |  4130 | `				/* Fix jump */` |
|      14 |  4131 | `				pInstr->iP2 = nJumpDest;` |
|       6 |  4132 | `			}` |
|       8 |  4133 | `		}` |
|       6 |  4134 | `	}` |
|       - |  4135 | `	/* compile the post-expressions if available */` |
|   14345 |  4136 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  4137 | `		pPostStart++;` |
|     ! 0 |  4138 | `	}` |
|   14345 |  4139 | `	if( pPostStart < pEnd ){` |
|       - |  4140 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   14345 |  4141 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   14345 |  4142 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14345 |  4143 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - |  4144 | `			/* Syntax error */` |
|     ! 0 |  4145 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 |  4146 | `			if( rc == SXERR_ABORT ){` |
|       - |  4147 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4148 | `				return SXERR_ABORT;` |
|       - |  4149 | `			}` |
|     ! 0 |  4150 | `			return SXRET_OK;` |
|       - |  4151 | `		}` |
|   14345 |  4152 | `		RE_SWAP_DELIMITER(pGen);` |
|   14345 |  4153 | `		if( rc == SXERR_ABORT ){` |
|       - |  4154 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4155 | `			return SXERR_ABORT;` |
|   14345 |  4156 | `		}else if( rc != SXERR_EMPTY){` |
|       - |  4157 | `			/* Pop operand lvalue */` |
|   14345 |  4158 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    7170 |  4159 | `		}` |
|    7170 |  4160 | `	}` |
|       - |  4161 | `	/* Emit the unconditional jump to the start of the loop */` |
|   14345 |  4162 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - |  4163 | `	/* Fix all jumps now the destination is resolved */` |
|   14345 |  4164 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4165 | `	/* Release the loop block */` |
|   14345 |  4166 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4167 | `	/* Statement successfully compiled */` |
|   14345 |  4168 | `	return SXRET_OK;` |
|    7177 |  4169 |  |
|       - |  4170 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - |  4171 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - |  4172 | ` * are allowed.` |
|       - |  4173 | ` */` |
|    7694 |  4174 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  4175 |  |
|    7699 |  4176 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    7699 |  4177 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  4178 | `		/* Unexpected expression */` |
|     ! 0 |  4179 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  4180 | `			"foreach: Expecting a variable name");` |
|     ! 0 |  4181 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  4182 | `			rc = SXERR_INVALID;` |
|     ! 0 |  4183 | `		}` |
|     ! 0 |  4184 | `	}` |
|    7699 |  4185 | `	return rc;` |
|       5 |  4186 |  |
|       - |  4187 | `/*` |
|       - |  4188 | ` * Compile the 'foreach' statement.` |
|       - |  4189 | ` * According to the PHP language reference` |
|       - |  4190 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - |  4191 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - |  4192 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - |  4193 | ` *  is a minor but useful extension of the first:` |
|       - |  4194 | ` *  foreach (array_expression as $value)` |
|       - |  4195 | ` *    statement` |
|       - |  4196 | ` *  foreach (array_expression as $key => $value)` |
|       - |  4197 | ` *   statement` |
|       - |  4198 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - |  4199 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - |  4200 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - |  4201 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - |  4202 | ` *  to the variable $key on each loop.` |
|       - |  4203 | ` *  Note:` |
|       - |  4204 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - |  4205 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - |  4206 | ` *  Note:` |
|       - |  4207 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - |  4208 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - |  4209 | ` *  or after the foreach without resetting it.` |
|       - |  4210 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - |  4211 | ` *  of copying the value.` |
|       - |  4212 | ` */` |
|    3942 |  4213 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       5 |  4214 |  |
|    3947 |  4215 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    3947 |  4216 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    3947 |  4217 | `	GenBlock *pForeachBlock = 0;` |
|       - |  4218 | `	ph7_foreach_info *pInfo;` |
|       - |  4219 | `	sxu32 nFalseJump;` |
|       - |  4220 | `	VmInstr *pInstr;` |
|       - |  4221 | `	sxu32 nLine;` |
|       - |  4222 | `	sxi32 rc;` |
|    3947 |  4223 | `	nLine = pGen->pIn->nLine;` |
|       - |  4224 | `	/* Jump the 'foreach' keyword */` |
|    3947 |  4225 | `	pGen->pIn++;` |
|    3947 |  4226 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4227 | `		/* Syntax error */` |
|     ! 0 |  4228 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 |  4229 | `		if( rc == SXERR_ABORT ){` |
|       - |  4230 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4231 | `			return SXERR_ABORT;` |
|       - |  4232 | `		}` |
|     ! 0 |  4233 | `		goto Synchronize;` |
|       - |  4234 | `	}` |
|       - |  4235 | `	/* Jump the left parenthesis '(' */` |
|    3947 |  4236 | `	pGen->pIn++;` |
|       - |  4237 | `	/* Create the loop block */` |
|    3947 |  4238 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    3947 |  4239 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4240 | `		return SXERR_ABORT;` |
|       - |  4241 | `	}` |
|       - |  4242 | `	/* Delimit the expression */` |
|    3947 |  4243 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    3947 |  4244 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4245 | `		/* Empty expression */` |
|     ! 0 |  4246 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 |  4247 | `		if( rc == SXERR_ABORT ){` |
|       - |  4248 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4249 | `			return SXERR_ABORT;` |
|       - |  4250 | `		}` |
|       - |  4251 | `		/* Synchronize */` |
|     ! 0 |  4252 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4253 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4254 | `			pGen->pIn++;` |
|     ! 0 |  4255 | `		}` |
|     ! 0 |  4256 | `		return SXRET_OK;` |
|       - |  4257 | `	}` |
|       - |  4258 | `	/* Compile the array expression */` |
|    3947 |  4259 | `	pCur = pGen->pIn;` |
|   27087 |  4260 | `	while( pCur < pEnd ){` |
|   27087 |  4261 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    3961 |  4262 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    3961 |  4263 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - |  4264 | `				/* Break with the first 'as' found */` |
|    3947 |  4265 | `				break;` |
|       - |  4266 | `			}` |
|       7 |  4267 | `		}` |
|       - |  4268 | `		/* Advance the stream cursor */` |
|   23145 |  4269 | `		pCur++;` |
|       5 |  4270 | `	}` |
|    3947 |  4271 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 |  4272 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  4273 | `			"foreach: Missing array/object expression");` |
|     ! 0 |  4274 | `		if( rc == SXERR_ABORT ){` |
|       - |  4275 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4276 | `			return SXERR_ABORT;` |
|       - |  4277 | `		}` |
|     ! 0 |  4278 | `		goto Synchronize;` |
|       - |  4279 | `	}` |
|       - |  4280 | `	/* Swap token streams */` |
|    3947 |  4281 | `	pTmp = pGen->pEnd;` |
|    3947 |  4282 | `	pGen->pEnd = pCur;` |
|    3947 |  4283 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    3947 |  4284 | `	if( rc == SXERR_ABORT ){` |
|       - |  4285 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4286 | `		return SXERR_ABORT;` |
|       - |  4287 | `	}` |
|       - |  4288 | `	/* Update token stream */` |
|    3947 |  4289 | `	while(pGen->pIn < pCur ){` |
|     ! 0 |  4290 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4291 | `		if( rc == SXERR_ABORT ){` |
|       - |  4292 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4293 | `			return SXERR_ABORT;` |
|       - |  4294 | `		}` |
|     ! 0 |  4295 | `		pGen->pIn++;` |
|     ! 0 |  4296 | `	}` |
|    3947 |  4297 | `	pCur++; /* Jump the 'as' keyword */` |
|    3947 |  4298 | `	pGen->pIn = pCur;` |
|    3947 |  4299 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4300 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 |  4301 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4302 | `			return SXERR_ABORT;` |
|       - |  4303 | `		}` |
|     ! 0 |  4304 | `	}` |
|       - |  4305 | `	/* Create the foreach context */` |
|    3947 |  4306 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    3947 |  4307 | `	if( pInfo == 0 ){` |
|     ! 0 |  4308 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  4309 | `		return SXERR_ABORT;` |
|       - |  4310 | `	}` |
|       - |  4311 | `	/* Zero the structure */` |
|    3947 |  4312 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - |  4313 | `	/* Initialize structure fields */` |
|    3947 |  4314 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - |  4315 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|       - |  4316 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|       - |  4317 | `	 * '=>'. */` |
|    3947 |  4318 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    3947 |  4319 | `	if( pCur < pEnd ){` |
|       - |  4320 | `		/* Compile the expression holding the key name */` |
|    3769 |  4321 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 |  4322 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 |  4323 | `			if( rc == SXERR_ABORT ){` |
|       - |  4324 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4325 | `				return SXERR_ABORT;` |
|       - |  4326 | `			}` |
|     ! 0 |  4327 | `		}else{` |
|    3769 |  4328 | `			pGen->pEnd = pCur;` |
|    3769 |  4329 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3769 |  4330 | `			if( rc == SXERR_ABORT ){` |
|       - |  4331 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4332 | `				return SXERR_ABORT;` |
|       - |  4333 | `			}` |
|    3769 |  4334 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3769 |  4335 | `			if( pInstr->p3 ){` |
|       - |  4336 | `				/* Record key name */` |
|    3769 |  4337 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1882 |  4338 | `			}` |
|    3769 |  4339 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - |  4340 | `		}` |
|    3769 |  4341 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1882 |  4342 | `	}` |
|    3947 |  4343 | `	pGen->pEnd = pEnd;` |
|    3947 |  4344 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4345 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 |  4346 | `		if( rc == SXERR_ABORT ){` |
|       - |  4347 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4348 | `			return SXERR_ABORT;` |
|       - |  4349 | `		}` |
|     ! 0 |  4350 | `		goto Synchronize;` |
|       - |  4351 | `	}` |
|    3947 |  4352 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 |  4353 | `		pGen->pIn++;` |
|       - |  4354 | `		/* Pass by reference  */` |
|      11 |  4355 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 |  4356 | `	}` |
|       - |  4357 | `	/* Check if the value target is list() */` |
|    3947 |  4358 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 |  4359 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  4360 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - |  4361 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - |  4362 | `		 */` |
|       - |  4363 | `		static int iForeachListCnt = 0;` |
|       - |  4364 | `		char zTmp[128];` |
|       - |  4365 | `		sxu32 nLen;` |
|       - |  4366 | `		char *zDup;` |
|      10 |  4367 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 |  4368 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 |  4369 | `		if( zDup == 0 ){` |
|     ! 0 |  4370 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4371 | `			return SXERR_ABORT;` |
|       - |  4372 | `		}` |
|      10 |  4373 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4374 | `		/* Save list() token boundaries */` |
|      10 |  4375 | `		pListStart = pGen->pIn;` |
|       - |  4376 | `		/* Advance past list(...) — validate parentheses */` |
|      10 |  4377 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 |  4378 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  4379 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  4380 | `				"foreach: Expected '(' after 'list'");` |
|       3 |  4381 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4382 | `				return SXERR_ABORT;` |
|       - |  4383 | `			}` |
|       3 |  4384 | `			goto Synchronize;` |
|       - |  4385 | `		}` |
|       7 |  4386 | `		pGen->pIn++; /* Jump '(' */` |
|       7 |  4387 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 |  4388 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4389 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4390 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 |  4391 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4392 | `				return SXERR_ABORT;` |
|       - |  4393 | `			}` |
|     ! 0 |  4394 | `			goto Synchronize;` |
|       - |  4395 | `		}` |
|       7 |  4396 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 |  4397 | `		pListEnd = pGen->pIn;` |
|       7 |  4398 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    3942 |  4399 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  4400 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - |  4401 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - |  4402 | `		 */` |
|       - |  4403 | `		static int iForeachShortListCnt = 0;` |
|       - |  4404 | `		char zTmp[128];` |
|       - |  4405 | `		sxu32 nLen;` |
|       - |  4406 | `		char *zDup;` |
|       5 |  4407 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       5 |  4408 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       5 |  4409 | `		if( zDup == 0 ){` |
|     ! 0 |  4410 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4411 | `			return SXERR_ABORT;` |
|       - |  4412 | `		}` |
|       5 |  4413 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4414 | `		/* Save [...] token boundaries */` |
|       5 |  4415 | `		pListStart = pGen->pIn;` |
|       - |  4416 | `		/* Advance past [...] */` |
|       5 |  4417 | `		pGen->pIn++; /* Jump '[' */` |
|       5 |  4418 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       5 |  4419 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4420 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4421 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 |  4422 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4423 | `				return SXERR_ABORT;` |
|       - |  4424 | `			}` |
|     ! 0 |  4425 | `			goto Synchronize;` |
|       - |  4426 | `		}` |
|       5 |  4427 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       5 |  4428 | `		pListEnd = pGen->pIn;` |
|       5 |  4429 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       3 |  4430 | `	}else{` |
|       - |  4431 | `		/* Compile the expression holding the value name */` |
|    3935 |  4432 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3935 |  4433 | `		if( rc == SXERR_ABORT ){` |
|       - |  4434 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4435 | `			return SXERR_ABORT;` |
|       - |  4436 | `		}` |
|    3935 |  4437 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3935 |  4438 | `		if( pInstr->p3 ){` |
|       - |  4439 | `			/* Record value name */` |
|    3935 |  4440 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1965 |  4441 | `		}` |
|       - |  4442 | `	}` |
|       - |  4443 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    3945 |  4444 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - |  4445 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3945 |  4446 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - |  4447 | `	/* Record the first instruction to execute */` |
|    3945 |  4448 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4449 | `	/* Emit the FOREACH_STEP instruction */` |
|    3945 |  4450 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - |  4451 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3945 |  4452 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - |  4453 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    3945 |  4454 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - |  4455 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - |  4456 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - |  4457 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - |  4458 | `		 */` |
|      11 |  4459 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - |  4460 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - |  4461 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - |  4462 | `		 * picks up the delimiter and the variable names inside.` |
|       - |  4463 | `		 */` |
|      11 |  4464 | `		pSavedIn = pGen->pIn;` |
|      11 |  4465 | `		pSavedEnd = pGen->pEnd;` |
|      11 |  4466 | `		pGen->pIn = pListStart;` |
|      11 |  4467 | `		pGen->pEnd = pListEnd;` |
|      11 |  4468 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       5 |  4469 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       3 |  4470 | `		}else{` |
|       7 |  4471 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - |  4472 | `		}` |
|      11 |  4473 | `		pGen->pIn = pSavedIn;` |
|      11 |  4474 | `		pGen->pEnd = pSavedEnd;` |
|      11 |  4475 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4476 | `			return SXERR_ABORT;` |
|       - |  4477 | `		}` |
|       - |  4478 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|      11 |  4479 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       5 |  4480 | `	}` |
|       - |  4481 | `	/* Compile the loop body */` |
|    3945 |  4482 | `	pGen->pIn = &pEnd[1];` |
|    3945 |  4483 | `	pGen->pEnd = pTmp;` |
|    3945 |  4484 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    3945 |  4485 | `	if( rc == SXERR_ABORT ){` |
|       - |  4486 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4487 | `		return SXERR_ABORT;` |
|       - |  4488 | `	}` |
|       - |  4489 | `	/* Emit the unconditional jump to the start of the loop */` |
|    3945 |  4490 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - |  4491 | `	/* Fix all jumps now the destination is resolved */` |
|    3945 |  4492 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4493 | `	/* Release the loop block */` |
|    3945 |  4494 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4495 | `	/* Statement successfully compiled */` |
|    3945 |  4496 | `	return SXRET_OK;` |
|       1 |  4497 | `Synchronize:` |
|       - |  4498 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4499 | `	 * compiling this erroneous block.` |
|       - |  4500 | `	 */` |
|       3 |  4501 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4502 | `		pGen->pIn++;` |
|     ! 0 |  4503 | `	}` |
|       3 |  4504 | `	return SXRET_OK;` |
|    1976 |  4505 |  |
|       - |  4506 | `/*` |
|       - |  4507 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - |  4508 | ` * According to the PHP language reference` |
|       - |  4509 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - |  4510 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - |  4511 | ` *  that is similar to that of C:` |
|       - |  4512 | ` *  if (expr)` |
|       - |  4513 | ` *   statement` |
|       - |  4514 | ` *  else construct:` |
|       - |  4515 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - |  4516 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - |  4517 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - |  4518 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - |  4519 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - |  4520 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - |  4521 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - |  4522 | ` *  elseif` |
|       - |  4523 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - |  4524 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - |  4525 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - |  4526 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - |  4527 | ` *   than b, a equal to b or a is smaller than b:` |
|       - |  4528 | ` *   <?php` |
|       - |  4529 | ` *    if ($a > $b) {` |
|       - |  4530 | ` *     echo "a is bigger than b";` |
|       - |  4531 | ` *    } elseif ($a == $b) {` |
|       - |  4532 | ` *     echo "a is equal to b";` |
|       - |  4533 | ` *    } else {` |
|       - |  4534 | ` *     echo "a is smaller than b";` |
|       - |  4535 | ` *    }` |
|       - |  4536 | ` *    ?>` |
|       - |  4537 | ` */` |
|  149076 |  4538 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       5 |  4539 |  |
|  149081 |  4540 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  149081 |  4541 | `	GenBlock *pCondBlock = 0;` |
|       - |  4542 | `	sxu32 nJumpIdx;` |
|       - |  4543 | `	sxu32 nKeyID;` |
|       - |  4544 | `	sxi32 rc;` |
|       - |  4545 | `	/* Jump the 'if' keyword */` |
|  149081 |  4546 | `	pGen->pIn++;` |
|  149081 |  4547 | `	pToken = pGen->pIn;` |
|       - |  4548 | `	/* Create the conditional block */` |
|  149081 |  4549 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  149081 |  4550 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4551 | `		return SXERR_ABORT;` |
|       - |  4552 | `	}` |
|       - |  4553 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   81707 |  4554 | `	for(;;){` |
|  163419 |  4555 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4556 | `			/* Syntax error */` |
|     ! 0 |  4557 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4558 | `				pToken--;` |
|     ! 0 |  4559 | `			}` |
|     ! 0 |  4560 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 |  4561 | `			if( rc == SXERR_ABORT ){` |
|       - |  4562 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4563 | `				return SXERR_ABORT;` |
|       - |  4564 | `			}` |
|     ! 0 |  4565 | `			goto Synchronize;` |
|       - |  4566 | `		}` |
|       - |  4567 | `		/* Jump the left parenthesis '(' */` |
|  163419 |  4568 | `		pToken++;` |
|       - |  4569 | `		/* Delimit the condition */` |
|  163419 |  4570 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  163419 |  4571 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - |  4572 | `			/* Syntax error */` |
|     ! 0 |  4573 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4574 | `				pToken--;` |
|     ! 0 |  4575 | `			}` |
|     ! 0 |  4576 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 |  4577 | `			if( rc == SXERR_ABORT ){` |
|       - |  4578 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4579 | `				return SXERR_ABORT;` |
|       - |  4580 | `			}` |
|     ! 0 |  4581 | `			goto Synchronize;` |
|       - |  4582 | `		}` |
|       - |  4583 | `		/* Swap token streams */` |
|  163419 |  4584 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - |  4585 | `		/* Compile the condition */` |
|  163419 |  4586 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4587 | `		/* Update token stream */` |
|  163419 |  4588 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 |  4589 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4590 | `			pGen->pIn++;` |
|     ! 0 |  4591 | `		}` |
|  163419 |  4592 | `		pGen->pIn  = &pEnd[1];` |
|  163419 |  4593 | `		pGen->pEnd = pTmp;` |
|  163419 |  4594 | `		if( rc == SXERR_ABORT ){` |
|       - |  4595 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4596 | `			return SXERR_ABORT;` |
|       - |  4597 | `		}` |
|       - |  4598 | `		/* Emit the false jump */` |
|  163419 |  4599 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - |  4600 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  163419 |  4601 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - |  4602 | `		/* Compile the body */` |
|  163419 |  4603 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  163419 |  4604 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4605 | `			return SXERR_ABORT;` |
|       - |  4606 | `		}` |
|  163419 |  4607 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   45554 |  4608 | `			break;` |
|       - |  4609 | `		}` |
|       - |  4610 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   72321 |  4611 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   72321 |  4612 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   46551 |  4613 | `			break;` |
|       - |  4614 | `		}` |
|       - |  4615 | `		/* Emit the unconditional jump */` |
|   25775 |  4616 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - |  4617 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   25775 |  4618 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   25775 |  4619 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   18549 |  4620 | `			pToken = &pGen->pIn[1];` |
|   18549 |  4621 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    7164 |  4622 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    5721 |  4623 | `					break;` |
|       - |  4624 | `			}` |
|    7117 |  4625 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    3556 |  4626 | `		}` |
|   14343 |  4627 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - |  4628 | `		/* Synchronize cursors */` |
|   14343 |  4629 | `		pToken = pGen->pIn;` |
|       - |  4630 | `		/* Fix the false jump */` |
|   14343 |  4631 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       5 |  4632 | `	} /* For(;;) */` |
|       - |  4633 | `	/* Fix the false jump */` |
|  149081 |  4634 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  149081 |  4635 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   57978 |  4636 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - |  4637 | `			/* Compile the else block */` |
|   11437 |  4638 | `			pGen->pIn++;` |
|   11437 |  4639 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   11437 |  4640 | `			if( rc == SXERR_ABORT ){` |
|       - |  4641 |  |
|     ! 0 |  4642 | `				return SXERR_ABORT;` |
|       - |  4643 | `			}` |
|    5716 |  4644 | `	}` |
|  149081 |  4645 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4646 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  149081 |  4647 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - |  4648 | `	/* Release the conditional block */` |
|  149081 |  4649 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4650 | `	/* Statement successfully compiled */` |
|  149081 |  4651 | `	return SXRET_OK;` |
|     ! 0 |  4652 | `Synchronize:` |
|       - |  4653 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - |  4654 | `	 */` |
|     ! 0 |  4655 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4656 | `		pGen->pIn++;` |
|     ! 0 |  4657 | `	}` |
|     ! 0 |  4658 | `	return SXRET_OK;` |
|   74543 |  4659 |  |
|       - |  4660 | `/*` |
|       - |  4661 | ` * Compile the global construct.` |
|       - |  4662 | ` * According to the PHP language reference` |
|       - |  4663 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - |  4664 | ` *  to be used in that function.` |
|       - |  4665 | ` *  Example #1 Using global` |
|       - |  4666 | ` *  <?php` |
|       - |  4667 | ` *   $a = 1;` |
|       - |  4668 | ` *   $b = 2;` |
|       - |  4669 | ` *   function Sum()` |
|       - |  4670 | ` *   {` |
|       - |  4671 | ` *    global $a, $b;` |
|       - |  4672 | ` *    $b = $a + $b;` |
|       - |  4673 | ` *   }` |
|       - |  4674 | ` *   Sum();` |
|       - |  4675 | ` *   echo $b;` |
|       - |  4676 | ` *  ?>` |
|       - |  4677 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - |  4678 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - |  4679 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - |  4680 | ` */` |
|      36 |  4681 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       5 |  4682 |  |
|      41 |  4683 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4684 | `	sxi32 nExpr;` |
|       - |  4685 | `	sxi32 rc;` |
|       - |  4686 | `	/* Jump the 'global' keyword */` |
|      41 |  4687 | `	pGen->pIn++;` |
|      41 |  4688 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - |  4689 | `		/* Nothing to process */` |
|     ! 0 |  4690 | `		return SXRET_OK;` |
|       - |  4691 | `	}` |
|      41 |  4692 | `	pTmp = pGen->pEnd;` |
|      41 |  4693 | `	nExpr = 0;` |
|      87 |  4694 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      51 |  4695 | `		if( pGen->pIn < pNext ){` |
|      51 |  4696 | `			pGen->pEnd = pNext;` |
|      51 |  4697 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4698 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 |  4699 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  4700 | `					return SXERR_ABORT;` |
|       - |  4701 | `				}` |
|     ! 0 |  4702 | `			}else{` |
|      51 |  4703 | `				pGen->pIn++;` |
|      51 |  4704 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4705 | `					/* Emit a warning */` |
|     ! 0 |  4706 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 |  4707 | `				}else{` |
|      51 |  4708 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      51 |  4709 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  4710 | `						return SXERR_ABORT;` |
|      51 |  4711 | `					}else if(rc != SXERR_EMPTY ){` |
|      51 |  4712 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      51 |  4713 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - |  4714 | `							/* Variable name, not a constant */` |
|      51 |  4715 | `							pLast->iP1 = 0;` |
|      23 |  4716 | `						}` |
|      51 |  4717 | `						nExpr++;` |
|      23 |  4718 | `					}` |
|       - |  4719 | `				}` |
|       - |  4720 | `			}` |
|      23 |  4721 | `		}` |
|       - |  4722 | `		/* Next expression in the stream */` |
|      51 |  4723 | `		pGen->pIn = pNext;` |
|       - |  4724 | `		/* Jump trailing commas */` |
|      61 |  4725 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      15 |  4726 | `			pGen->pIn++;` |
|       5 |  4727 | `		}` |
|       5 |  4728 | `	}` |
|       - |  4729 | `	/* Restore token stream */` |
|      41 |  4730 | `	pGen->pEnd = pTmp;` |
|      41 |  4731 | `	if( nExpr > 0 ){` |
|       - |  4732 | `		/* Emit the uplink instruction */` |
|      41 |  4733 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      18 |  4734 | `	}` |
|      41 |  4735 | `	return SXRET_OK;` |
|      23 |  4736 |  |
|       - |  4737 | `/*` |
|       - |  4738 | ` * Compile the return statement.` |
|       - |  4739 | ` * According to the PHP language reference` |
|       - |  4740 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - |  4741 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - |  4742 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - |  4743 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - |  4744 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - |  4745 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - |  4746 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - |  4747 | ` *  from within the main script file, then script execution end.` |
|       - |  4748 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - |  4749 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - |  4750 | ` *  should do so as PHP has less work to do in this case.` |
|       - |  4751 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - |  4752 | ` */` |
|  236140 |  4753 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       5 |  4754 |  |
|  236145 |  4755 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - |  4756 | `	sxi32 rc;` |
|       - |  4757 | `	/* Jump the 'return' keyword */` |
|  236145 |  4758 | `	pGen->pIn++;` |
|  236145 |  4759 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4760 | `		/* Compile the expression */` |
|  236117 |  4761 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  236117 |  4762 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4763 | `			return SXERR_ABORT;` |
|  236117 |  4764 | `		}else if(rc != SXERR_EMPTY ){` |
|  236117 |  4765 | `			nRet = 1;` |
|  118056 |  4766 | `		}` |
|  118056 |  4767 | `	}` |
|       - |  4768 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|       - |  4769 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|       - |  4770 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|       - |  4771 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|       - |  4772 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  236145 |  4773 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  236145 |  4774 | `	return SXRET_OK;` |
|  118075 |  4775 |  |
|       - |  4776 | `/*` |
|       - |  4777 | ` * Compile a yield expression.` |
|       - |  4778 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - |  4779 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - |  4780 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - |  4781 | ` */` |
|     170 |  4782 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       5 |  4783 |  |
|       - |  4784 | `	SyToken *pTmp, *pSplit;` |
|     175 |  4785 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     175 |  4786 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - |  4787 | `	sxi32 rc;` |
|      85 |  4788 | `	(void)iCompileFlag;` |
|       - |  4789 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     175 |  4790 | `	pGen->pIn++;` |
|       - |  4791 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - |  4792 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|       - |  4793 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|       - |  4794 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|       - |  4795 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     170 |  4796 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|     102 |  4797 | `		&& pGen->pIn->sData.nByte == 4` |
|      41 |  4798 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|      40 |  4799 | `		pGen->pIn++; /* Skip 'from' */` |
|      40 |  4800 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      40 |  4801 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4802 | `			return SXERR_ABORT;` |
|       - |  4803 | `		}` |
|      40 |  4804 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  4805 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|     ! 0 |  4806 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|       - |  4807 | `				"Missing expression after 'yield from'");` |
|     ! 0 |  4808 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4809 | `				return SXERR_ABORT;` |
|       - |  4810 | `			}` |
|     ! 0 |  4811 | `		}` |
|      40 |  4812 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|      40 |  4813 | `		return SXRET_OK;` |
|       - |  4814 | `	}` |
|     139 |  4815 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4816 | `		/* Bare yield — no value */` |
|       3 |  4817 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|       3 |  4818 | `		return SXRET_OK;` |
|       - |  4819 | `	}` |
|       - |  4820 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     137 |  4821 | `	pSplit = 0;` |
|       - |  4822 | `	{` |
|     137 |  4823 | `		SyToken *pCur = pGen->pIn;` |
|     137 |  4824 | `		sxi32 nNest = 0;` |
|     285 |  4825 | `		while( pCur < pGen->pEnd ){` |
|     167 |  4826 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  4827 | `				nNest++;` |
|     167 |  4828 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  4829 | `				nNest--;` |
|     167 |  4830 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|      16 |  4831 | `				pSplit = pCur;` |
|      16 |  4832 | `				break;` |
|       - |  4833 | `			}` |
|     153 |  4834 | `			pCur++;` |
|       5 |  4835 | `		}` |
|       - |  4836 | `	}` |
|     137 |  4837 | `	pTmp = pGen->pEnd;` |
|     137 |  4838 | `	if( pSplit ){` |
|       - |  4839 | `		/* yield $key => $value */` |
|      16 |  4840 | `		pGen->pEnd = pSplit;` |
|      16 |  4841 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4842 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4843 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|      16 |  4844 | `		pGen->pEnd = pTmp;` |
|      16 |  4845 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4846 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4847 | `		iP1 = 1;` |
|      16 |  4848 | `		iP2 = 1;` |
|       9 |  4849 | `	}else{` |
|       - |  4850 | `		/* yield $value */` |
|     123 |  4851 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     123 |  4852 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     123 |  4853 | `		if( rc != SXERR_EMPTY ){` |
|     123 |  4854 | `			iP1 = 1;` |
|      59 |  4855 | `		}` |
|       - |  4856 | `	}` |
|     137 |  4857 | `	pGen->pEnd = pTmp;` |
|     137 |  4858 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     137 |  4859 | `	return SXRET_OK;` |
|      90 |  4860 |  |
|       - |  4861 | `/*` |
|       - |  4862 | ` * Compile the die/exit language construct.` |
|       - |  4863 | ` * The role of these constructs is to terminate execution of the script.` |
|       - |  4864 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - |  4865 | ` */` |
|     120 |  4866 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       5 |  4867 |  |
|     125 |  4868 | `	sxi32 nExpr = 0;` |
|       - |  4869 | `	sxi32 rc;` |
|       - |  4870 | `	/* Jump the die/exit keyword */` |
|     125 |  4871 | `	pGen->pIn++;` |
|     125 |  4872 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4873 | `		/* Compile the expression */` |
|     125 |  4874 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     125 |  4875 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4876 | `			return SXERR_ABORT;` |
|     125 |  4877 | `		}else if(rc != SXERR_EMPTY ){` |
|     125 |  4878 | `			nExpr = 1;` |
|      60 |  4879 | `		}` |
|      60 |  4880 | `	}` |
|       - |  4881 | `	/* Emit the HALT instruction */` |
|     125 |  4882 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|     125 |  4883 | `	return SXRET_OK;` |
|      65 |  4884 |  |
|       - |  4885 | `/*` |
|       - |  4886 | ` * Compile the 'echo' language construct.` |
|       - |  4887 | ` */` |
|   14710 |  4888 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       5 |  4889 |  |
|   14715 |  4890 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4891 | `	sxi32 rc;` |
|       - |  4892 | `	/* Jump the 'echo' keyword */` |
|   14715 |  4893 | `	pGen->pIn++;` |
|       - |  4894 | `	/* Compile arguments one after one */` |
|   14715 |  4895 | `	pTmp = pGen->pEnd;` |
|   32529 |  4896 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   17819 |  4897 | `		if( pGen->pIn < pNext ){` |
|   17819 |  4898 | `			pGen->pEnd = pNext;` |
|   17819 |  4899 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   17819 |  4900 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4901 | `				return SXERR_ABORT;` |
|   17819 |  4902 | `			}else if( rc != SXERR_EMPTY ){` |
|       - |  4903 | `				/* Emit the consume instruction */` |
|   17795 |  4904 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    8895 |  4905 | `			}` |
|    8907 |  4906 | `		}` |
|       - |  4907 | `		/* Jump trailing commas */` |
|   20923 |  4908 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    3109 |  4909 | `			pNext++;` |
|       5 |  4910 | `		}` |
|   17819 |  4911 | `		pGen->pIn = pNext;` |
|       5 |  4912 | `	}` |
|       - |  4913 | `	/* Restore token stream */` |
|   14715 |  4914 | `	pGen->pEnd = pTmp;` |
|   14715 |  4915 | `	return SXRET_OK;` |
|    7360 |  4916 |  |
|       - |  4917 | `/*` |
|       - |  4918 | ` * Compile the static statement.` |
|       - |  4919 | ` * According to the PHP language reference` |
|       - |  4920 | ` *  Another important feature of variable scoping is the static variable.` |
|       - |  4921 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - |  4922 | ` *  when program execution leaves this scope.` |
|       - |  4923 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - |  4924 | ` * Symisc eXtension.` |
|       - |  4925 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - |  4926 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  4927 | ` *  Example` |
|       - |  4928 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  4929 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  4930 | ` */` |
|       6 |  4931 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       2 |  4932 |  |
|       - |  4933 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - |  4934 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - |  4935 | `	GenBlock *pBlock;` |
|       - |  4936 | `	SyString *pName;` |
|       - |  4937 | `	char *zDup;` |
|       - |  4938 | `	sxu32 nLine;` |
|       - |  4939 | `	sxi32 rc;` |
|       - |  4940 | `	/* Jump the static keyword */` |
|       8 |  4941 | `	nLine = pGen->pIn->nLine;` |
|       8 |  4942 | `	pGen->pIn++;` |
|       - |  4943 | `	/* Extract the enclosing function if any */` |
|       8 |  4944 | `	pBlock = pGen->pCurrent;` |
|      14 |  4945 | `	while( pBlock ){` |
|      14 |  4946 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       8 |  4947 | `			break;` |
|       - |  4948 | `		}` |
|       - |  4949 | `		/* Point to the upper block */` |
|       8 |  4950 | `		pBlock = pBlock->pParent;` |
|       2 |  4951 | `	}` |
|       8 |  4952 | `	if( pBlock == 0 ){` |
|       - |  4953 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 |  4954 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4955 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 |  4956 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4957 | `				return SXERR_ABORT;` |
|       - |  4958 | `			}` |
|     ! 0 |  4959 | `			goto Synchronize;` |
|       - |  4960 | `		}` |
|       - |  4961 | `		/* Compile the expression holding the variable */` |
|     ! 0 |  4962 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  4963 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4964 | `			return SXERR_ABORT;` |
|     ! 0 |  4965 | `		}else if( rc != SXERR_EMPTY ){` |
|       - |  4966 | `			/* Emit the POP instruction */` |
|     ! 0 |  4967 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  4968 | `		}` |
|     ! 0 |  4969 | `		return SXRET_OK;` |
|       - |  4970 | `	}` |
|       8 |  4971 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - |  4972 | `	/* Make sure we are dealing with a valid statement */` |
|       8 |  4973 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       4 |  4974 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 |  4975 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 |  4976 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4977 | `				return SXERR_ABORT;` |
|       - |  4978 | `			}` |
|       3 |  4979 | `			goto Synchronize;` |
|       - |  4980 | `	}` |
|       5 |  4981 | `	pGen->pIn++;` |
|       - |  4982 | `	/* Extract variable name */` |
|       5 |  4983 | `	pName = &pGen->pIn->sData;` |
|       5 |  4984 | `	pGen->pIn++; /* Jump the var name */` |
|       5 |  4985 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 |  4986 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4987 | `		goto Synchronize;` |
|       - |  4988 | `	}` |
|       - |  4989 | `	/* Initialize the structure describing the static variable */` |
|       5 |  4990 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       5 |  4991 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - |  4992 | `	/* Duplicate variable name */` |
|       5 |  4993 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       5 |  4994 | `	if( zDup == 0 ){` |
|     ! 0 |  4995 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4996 | `		return SXERR_ABORT;` |
|       - |  4997 | `	}` |
|       5 |  4998 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - |  4999 | `	/* Check if we have an expression to compile */` |
|       5 |  5000 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - |  5001 | `		SySet *pInstrContainer;` |
|       - |  5002 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - |  5003 | `		 * Static variable can take any complex expression including function` |
|       - |  5004 | `		 * call as their initialization value.` |
|       - |  5005 | `		 * Example:` |
|       - |  5006 | `		 *		static $var = foo(1,4+5,bar());` |
|       - |  5007 | `		 */` |
|       5 |  5008 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - |  5009 | `		/* Swap bytecode container */` |
|       5 |  5010 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       5 |  5011 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - |  5012 | `		/* Compile the expression */` |
|       5 |  5013 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5014 | `		/* Emit the done instruction */` |
|       5 |  5015 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - |  5016 | `		/* Restore default bytecode container */` |
|       5 |  5017 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       2 |  5018 | `	}` |
|       - |  5019 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       5 |  5020 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       5 |  5021 | `	return SXRET_OK;` |
|       1 |  5022 | `Synchronize:` |
|       - |  5023 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - |  5024 | `	 * statement.` |
|       - |  5025 | `	 */` |
|       5 |  5026 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 |  5027 | `		pGen->pIn++;` |
|       1 |  5028 | `	}` |
|       3 |  5029 | `	return SXRET_OK;` |
|       5 |  5030 |  |
|       - |  5031 | `/*` |
|       - |  5032 | ` * Compile the var statement.` |
|       - |  5033 | ` * Symisc Extension:` |
|       - |  5034 | ` *      var statement can be used outside of a class definition.` |
|       - |  5035 | ` */` |
|       4 |  5036 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 |  5037 |  |
|       - |  5038 | `	sxu32 nLine;` |
|       - |  5039 | `	sxi32 rc;` |
|       5 |  5040 | `	nLine = pGen->pIn->nLine;` |
|       - |  5041 | `	/* Jump the 'var' keyword */` |
|       5 |  5042 | `	pGen->pIn++;` |
|       5 |  5043 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  5044 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - |  5045 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 |  5046 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 |  5047 | `			pGen->pIn++;` |
|     ! 0 |  5048 | `		}` |
|     ! 0 |  5049 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5050 | `			return SXERR_ABORT;` |
|       - |  5051 | `		}` |
|     ! 0 |  5052 | `	}else{` |
|       - |  5053 | `		/* Compile the expression */` |
|       5 |  5054 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 |  5055 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5056 | `			return SXERR_ABORT;` |
|       5 |  5057 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 |  5058 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 |  5059 | `		}` |
|       - |  5060 | `	}` |
|       5 |  5061 | `	return SXRET_OK;` |
|       3 |  5062 |  |
|       - |  5063 | `/*` |
|       - |  5064 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - |  5065 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - |  5066 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - |  5067 | ` */` |
|       - |  5068 | `/*` |
|       - |  5069 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - |  5070 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - |  5071 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - |  5072 | ` * qualified name and updates the instruction's operand index.` |
|       - |  5073 | ` *` |
|       - |  5074 | ` * Resolution order:` |
|       - |  5075 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - |  5076 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - |  5077 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - |  5078 | ` *` |
|       - |  5079 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - |  5080 | ` * came from an import (step 1) and 0 otherwise.` |
|       - |  5081 | ` * Returns the (possibly new) literal index.` |
|       - |  5082 | ` */` |
|  458680 |  5083 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       5 |  5084 |  |
|       - |  5085 | `	ph7_value *pLit;` |
|       - |  5086 | `	const char *zLit;` |
|       - |  5087 | `	SyString sQualified;` |
|       - |  5088 | `	sxu32 nLit;` |
|       - |  5089 | `	sxu32 k;` |
|       - |  5090 | `	sxu32 nNewIdx;` |
|       - |  5091 | `	int hasNsSep;` |
|       - |  5092 | `	SyHashEntry *pImport;` |
|       - |  5093 | `	ph7_value *pNew;` |
|  458685 |  5094 | `	if( pFromImport ){` |
|  438971 |  5095 | `		*pFromImport = 0;` |
|  219483 |  5096 | `	}` |
|  458685 |  5097 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  458685 |  5098 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 |  5099 | `		return nOrigIdx;` |
|       - |  5100 | `	}` |
|  458685 |  5101 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  458685 |  5102 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - |  5103 | `	/* Skip if already qualified (contains backslash) */` |
|  458685 |  5104 | `	hasNsSep = 0;` |
| 5066383 |  5105 | `	for( k = 0; k < nLit; k++ ){` |
| 4607711 |  5106 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 2303854 |  5107 | `	}` |
|  458685 |  5108 | `	if( hasNsSep ){` |
|      11 |  5109 | `		return nOrigIdx;` |
|       - |  5110 | `	}` |
|       - |  5111 | `	/* Check use imports first (works even outside namespaces) */` |
|  458677 |  5112 | `	SyBlobReset(&pGen->sWorker);` |
|  458677 |  5113 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  458677 |  5114 | `	if( pImport ){` |
|      41 |  5115 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      41 |  5116 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      41 |  5117 | `		if( pFromImport ){` |
|      18 |  5118 | `			*pFromImport = 1;` |
|       8 |  5119 | `		}` |
|      23 |  5120 | `	}else{` |
|  458641 |  5121 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  458551 |  5122 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - |  5123 | `		}` |
|       - |  5124 | `		/* Prepend current namespace */` |
|      95 |  5125 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      95 |  5126 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      95 |  5127 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - |  5128 | `	}` |
|       - |  5129 | `	/* Look up or create a new literal for the qualified name */` |
|     131 |  5130 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     131 |  5131 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      57 |  5132 | `		return nNewIdx; /* Already interned */` |
|       - |  5133 | `	}` |
|      79 |  5134 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      79 |  5135 | `	if( pNew == 0 ){` |
|     ! 0 |  5136 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - |  5137 | `	}` |
|      79 |  5138 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      79 |  5139 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      79 |  5140 | `	return nNewIdx;` |
|  229345 |  5141 |  |
|       - |  5142 | `/*` |
|       - |  5143 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - |  5144 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - |  5145 | ` */` |
|   96960 |  5146 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5147 |  |
|       - |  5148 | `	SyHashEntry *pImport;` |
|       - |  5149 | `	/* Check use imports first */` |
|   96965 |  5150 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   96965 |  5151 | `	if( pImport ){` |
|      14 |  5152 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      14 |  5153 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      14 |  5154 | `		return;` |
|       - |  5155 | `	}` |
|       - |  5156 | `	/* Prepend current namespace if active */` |
|   96953 |  5157 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 |  5158 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  5159 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 |  5160 | `	}` |
|   96953 |  5161 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   48485 |  5162 |  |
|       - |  5163 | `/*` |
|       - |  5164 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - |  5165 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - |  5166 | ` * The caller must release pOut when done.` |
|       - |  5167 | ` */` |
|  140086 |  5168 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5169 |  |
|  140091 |  5170 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      63 |  5171 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      63 |  5172 | `		SyBlobAppend(pOut,"\\",1);` |
|      29 |  5173 | `	}` |
|  140091 |  5174 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|  140091 |  5175 |  |
|       - |  5176 | `/*` |
|       - |  5177 | ` * Compile a namespace statement` |
|       - |  5178 | ` * According to the PHP language reference manual` |
|       - |  5179 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - |  5180 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - |  5181 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - |  5182 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - |  5183 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - |  5184 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - |  5185 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - |  5186 | ` *  programming world.` |
|       - |  5187 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - |  5188 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - |  5189 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - |  5190 | ` *  classes/functions/constants.` |
|       - |  5191 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - |  5192 | ` *  readability of source code.` |
|       - |  5193 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - |  5194 | ` *  Here is an example of namespace syntax in PHP:` |
|       - |  5195 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - |  5196 | ` *       class MyClass {}` |
|       - |  5197 | ` *       function myfunction() {}` |
|       - |  5198 | ` *       const MYCONST = 1;` |
|       - |  5199 | ` *       $a = new MyClass;` |
|       - |  5200 | ` *       $c = new \my\name\MyClass;` |
|       - |  5201 | ` *       $a = strlen('hi');` |
|       - |  5202 | ` *       $d = namespace\MYCONST;` |
|       - |  5203 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - |  5204 | ` *       echo constant($d);` |
|       - |  5205 | ` * NOTE` |
|       - |  5206 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5207 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5208 | ` */` |
|       - |  5209 | `/*` |
|       - |  5210 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - |  5211 | ` */` |
|      14 |  5212 | `static const char * TokenTypeName(sxu32 nType)` |
|       4 |  5213 |  |
|      18 |  5214 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|      11 |  5215 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|      11 |  5216 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|      11 |  5217 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|      11 |  5218 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|      11 |  5219 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 |  5220 | `	return "token";` |
|      11 |  5221 |  |
|     106 |  5222 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       5 |  5223 |  |
|       - |  5224 | `	sxu32 nLine;` |
|       - |  5225 | `	sxi32 rc;` |
|     111 |  5226 | `	nLine = pGen->pIn->nLine;` |
|     111 |  5227 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - |  5228 | `	/* Reset namespace and clear previous use imports */` |
|     111 |  5229 | `	SyBlobReset(&pGen->sNamespace);` |
|     111 |  5230 | `	SyHashRelease(&pGen->hUseImports);` |
|     111 |  5231 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5232 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     111 |  5233 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5234 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     111 |  5235 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5236 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  5237 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 |  5238 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5239 | `		return SXRET_OK;` |
|       - |  5240 | `	}` |
|     111 |  5241 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - |  5242 | `		/* namespace; — switch to global namespace */` |
|     ! 0 |  5243 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5244 | `		return SXRET_OK;` |
|       - |  5245 | `	}` |
|     111 |  5246 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - |  5247 | `		/* namespace { } — global namespace block */` |
|     ! 0 |  5248 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5249 | `		return SXRET_OK;` |
|       - |  5250 | `	}` |
|       - |  5251 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     259 |  5252 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     153 |  5253 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - |  5254 | `			/* Append backslash separator */` |
|      26 |  5255 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      26 |  5256 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      11 |  5257 | `			}` |
|      15 |  5258 | `		}else{` |
|       - |  5259 | `			/* Append identifier */` |
|     131 |  5260 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  5261 | `		}` |
|     153 |  5262 | `		pGen->pIn++;` |
|       5 |  5263 | `	}` |
|       - |  5264 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - |  5265 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - |  5266 | `	{` |
|     111 |  5267 | `		char *zNsDup = 0;` |
|     111 |  5268 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     161 |  5269 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     104 |  5270 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      52 |  5271 | `		}` |
|     111 |  5272 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - |  5273 | `	}` |
|     111 |  5274 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       8 |  5275 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  5276 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 |  5277 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       6 |  5278 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5279 | `			return SXERR_ABORT;` |
|       - |  5280 | `		}` |
|       2 |  5281 | `	}` |
|     111 |  5282 | `	return SXRET_OK;` |
|      58 |  5283 |  |
|       - |  5284 | `/*` |
|       - |  5285 | ` * Compile the 'use' statement` |
|       - |  5286 | ` * According to the PHP language reference manual` |
|       - |  5287 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - |  5288 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - |  5289 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - |  5290 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - |  5291 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - |  5292 | ` *  a function or constant is not supported.` |
|       - |  5293 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - |  5294 | ` * NOTE` |
|       - |  5295 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5296 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5297 | ` */` |
|      68 |  5298 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       5 |  5299 |  |
|       - |  5300 | `	sxu32 nLine;` |
|       - |  5301 | `	sxi32 rc;` |
|       - |  5302 | `	SyBlob sPath;` |
|       - |  5303 | `	SyString sAlias;` |
|       - |  5304 | `	SyToken *pLast;` |
|       - |  5305 | `	char *zDup;` |
|       - |  5306 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - |  5307 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - |  5308 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      73 |  5309 | `	nLine = pGen->pIn->nLine;` |
|      73 |  5310 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - |  5311 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      73 |  5312 | `	iUseType = 0;` |
|      73 |  5313 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 |  5314 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 |  5315 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 |  5316 | `			iUseType = 1;` |
|      16 |  5317 | `			pGen->pIn++;` |
|      23 |  5318 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 |  5319 | `			iUseType = 2;` |
|      16 |  5320 | `			pGen->pIn++;` |
|       7 |  5321 | `		}` |
|      14 |  5322 | `	}` |
|       - |  5323 | `	/* Select target hash tables based on import type */` |
|      73 |  5324 | `	switch( iUseType ){` |
|       7 |  5325 | `		case 1:` |
|      16 |  5326 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 |  5327 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 |  5328 | `			break;` |
|       7 |  5329 | `		case 2:` |
|      16 |  5330 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 |  5331 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 |  5332 | `			break;` |
|      20 |  5333 | `		default:` |
|      45 |  5334 | `			pGenHash = &pGen->hUseImports;` |
|      45 |  5335 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      40 |  5336 | `			break;` |
|       - |  5337 | `	}` |
|      73 |  5338 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - |  5339 | `	/* Process one or more use declarations separated by commas */` |
|      35 |  5340 | `	for(;;){` |
|      75 |  5341 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5342 | `			break;` |
|       - |  5343 | `		}` |
|      75 |  5344 | `		SyBlobReset(&sPath);` |
|      75 |  5345 | `		pLast = 0;` |
|       - |  5346 | `		/* Collect the full namespace path */` |
|     261 |  5347 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     191 |  5348 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     131 |  5349 | `				pLast = pGen->pIn;` |
|     131 |  5350 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      65 |  5351 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 |  5352 | `				}` |
|     131 |  5353 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      63 |  5354 | `			}` |
|     191 |  5355 | `			pGen->pIn++;` |
|       5 |  5356 | `		}` |
|      75 |  5357 | `		if( pLast == 0 ){` |
|       - |  5358 | `			/* Empty path */` |
|       5 |  5359 | `			break;` |
|       - |  5360 | `		}` |
|       - |  5361 | `		/* Default alias is the last component of the path */` |
|      71 |  5362 | `		sAlias = pLast->sData;` |
|       - |  5363 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      66 |  5364 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      46 |  5365 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      18 |  5366 | `			pGen->pIn++; /* Jump 'as' */` |
|      18 |  5367 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      18 |  5368 | `				sAlias = pGen->pIn->sData;` |
|      18 |  5369 | `				pGen->pIn++;` |
|       8 |  5370 | `			}` |
|       8 |  5371 | `		}` |
|       - |  5372 | `		/* Check for duplicate import alias (per-type) */` |
|      71 |  5373 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       8 |  5374 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  5375 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 |  5376 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       6 |  5377 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5378 | `				SyBlobRelease(&sPath);` |
|     ! 0 |  5379 | `				return SXERR_ABORT;` |
|       - |  5380 | `			}` |
|       2 |  5381 | `		}` |
|       - |  5382 | `		/* Register the import: alias -> FQN.` |
|       - |  5383 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - |  5384 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - |  5385 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|     104 |  5386 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      66 |  5387 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      71 |  5388 | `		if( zDup ){` |
|      71 |  5389 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      71 |  5390 | `			if( pVmHash ){` |
|       - |  5391 | `				/* Class imports: populate VM table directly (class resolution` |
|       - |  5392 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      43 |  5393 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      43 |  5394 | `				if( zAliasDup ){` |
|      43 |  5395 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      19 |  5396 | `				}` |
|      19 |  5397 | `			}` |
|      71 |  5398 | `			if( iUseType == 2 ){` |
|       - |  5399 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - |  5400 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 |  5401 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 |  5402 | `				if( zAliasDup ){` |
|       - |  5403 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - |  5404 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - |  5405 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 |  5406 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 |  5407 | `					if( azPair ){` |
|      16 |  5408 | `						azPair[0] = zAliasDup;` |
|      16 |  5409 | `						azPair[1] = zDup;` |
|      16 |  5410 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 |  5411 | `					}` |
|       7 |  5412 | `				}` |
|       7 |  5413 | `			}` |
|      33 |  5414 | `		}` |
|       - |  5415 | `		/* Check for comma (multiple use declarations) */` |
|      71 |  5416 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  5417 | `			pGen->pIn++;` |
|       2 |  5418 | `		}else{` |
|      37 |  5419 | `			break;` |
|       - |  5420 | `		}` |
|       1 |  5421 | `	}` |
|      73 |  5422 | `	SyBlobRelease(&sPath);` |
|      73 |  5423 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 |  5424 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 |  5425 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 |  5426 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5427 | `			return SXERR_ABORT;` |
|       - |  5428 | `		}` |
|       1 |  5429 | `	}` |
|      73 |  5430 | `	return SXRET_OK;` |
|      39 |  5431 |  |
|       - |  5432 | `/*` |
|       - |  5433 | ` * Compile the stupid 'declare' language construct.` |
|       - |  5434 | ` *` |
|       - |  5435 | ` * According to the PHP language reference manual.` |
|       - |  5436 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - |  5437 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - |  5438 | ` *  declare (directive)` |
|       - |  5439 | ` *   statement` |
|       - |  5440 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - |  5441 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - |  5442 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - |  5443 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - |  5444 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - |  5445 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - |  5446 | ` * <?php` |
|       - |  5447 | ` * // these are the same:` |
|       - |  5448 | ` * // you can use this:` |
|       - |  5449 | ` * declare(ticks=1) {` |
|       - |  5450 | ` *   // entire script here` |
|       - |  5451 | ` * }` |
|       - |  5452 | ` * // or you can use this:` |
|       - |  5453 | ` * declare(ticks=1);` |
|       - |  5454 | ` * // entire script here` |
|       - |  5455 | ` * ?>` |
|       - |  5456 | ` *` |
|       - |  5457 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - |  5458 | ` */` |
|       - |  5459 | `/*` |
|       - |  5460 | ` * Match a directive name against a known literal (case-insensitive).` |
|       - |  5461 | ` */` |
|      68 |  5462 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|       5 |  5463 |  |
|     103 |  5464 | `	return SyStringLength(pName) == nWant` |
|      68 |  5465 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|       5 |  5466 |  |
|       - |  5467 |  |
|      40 |  5468 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       5 |  5469 |  |
|      45 |  5470 | `	sxu32 nLine = pGen->pIn->nLine;` |
|      45 |  5471 | `	SyToken *pBodyEnd = 0;` |
|       - |  5472 | `	SyToken *pBodyStart;` |
|       - |  5473 | `	SyToken *pCursor;` |
|       - |  5474 | `	int bHasStrictTypes;` |
|       - |  5475 | `	int bBlockForm;` |
|       - |  5476 | `	int bPlacementOk;` |
|       - |  5477 | `	sxi32 rc;` |
|      45 |  5478 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|      45 |  5479 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       6 |  5480 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       6 |  5481 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5482 | `			return SXERR_ABORT;` |
|       - |  5483 | `		}` |
|       6 |  5484 | `		goto Synchro;` |
|       - |  5485 | `	}` |
|      41 |  5486 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|      41 |  5487 | `	pBodyStart = pGen->pIn;` |
|       - |  5488 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|      41 |  5489 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|      41 |  5490 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  5491 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 |  5492 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5493 | `			return SXERR_ABORT;` |
|       - |  5494 | `		}` |
|     ! 0 |  5495 | `		return SXRET_OK;` |
|       - |  5496 | `	}` |
|       - |  5497 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|       - |  5498 | `	 * now delimits the comma-separated directive list. */` |
|      41 |  5499 | `	pGen->pIn = &pBodyEnd[1];` |
|      41 |  5500 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|     ! 0 |  5501 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 |  5502 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5503 | `			return SXERR_ABORT;` |
|       - |  5504 | `		}` |
|     ! 0 |  5505 | `	}` |
|      41 |  5506 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|      41 |  5507 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|      41 |  5508 | `	bHasStrictTypes = 0;` |
|       - |  5509 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|       - |  5510 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|       - |  5511 | `	 * directive appears anywhere in the list, before validating values. */` |
|      41 |  5512 | `	pCursor = pBodyStart;` |
|      53 |  5513 | `	while( pCursor < pBodyEnd ){` |
|      49 |  5514 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      41 |  5515 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|      37 |  5516 | `				bHasStrictTypes = 1;` |
|      37 |  5517 | `				break;` |
|       - |  5518 | `			}` |
|       2 |  5519 | `		}` |
|      14 |  5520 | `		pCursor++;` |
|       2 |  5521 | `	}` |
|      41 |  5522 | `	if( bHasStrictTypes && bBlockForm ){` |
|       3 |  5523 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5524 | `			"strict_types declaration must not use block mode");` |
|       3 |  5525 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5526 | `		return SXRET_OK;` |
|       - |  5527 | `	}` |
|      39 |  5528 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|       6 |  5529 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5530 | `			"strict_types declaration must be the very first statement in the script");` |
|       6 |  5531 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       6 |  5532 | `		return SXRET_OK;` |
|       - |  5533 | `	}` |
|       - |  5534 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|      35 |  5535 | `	pCursor = pBodyStart;` |
|      65 |  5536 | `	while( pCursor < pBodyEnd ){` |
|       - |  5537 | `		SyToken *pNameTok;` |
|       - |  5538 | `		SyToken *pEqTok;` |
|       - |  5539 | `		SyToken *pValTok;` |
|       - |  5540 | `		SyString *pDirName;` |
|       - |  5541 | `		int bIsStrict;` |
|       - |  5542 | `		int iStrictValue;` |
|      37 |  5543 | `		pNameTok = pCursor;` |
|      37 |  5544 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5545 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5546 | `				"declare: Expecting a directive name");` |
|     ! 0 |  5547 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5548 | `			return SXRET_OK;` |
|       - |  5549 | `		}` |
|      37 |  5550 | `		pEqTok = pNameTok + 1;` |
|      37 |  5551 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|     ! 0 |  5552 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5553 | `				"declare: Expecting '=' after directive name");` |
|     ! 0 |  5554 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5555 | `			return SXRET_OK;` |
|       - |  5556 | `		}` |
|      37 |  5557 | `		pValTok = pEqTok + 1;` |
|      37 |  5558 | `		if( pValTok >= pBodyEnd ){` |
|     ! 0 |  5559 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5560 | `				"declare: Expecting value after '='");` |
|     ! 0 |  5561 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5562 | `			return SXRET_OK;` |
|       - |  5563 | `		}` |
|      37 |  5564 | `		pDirName = &pNameTok->sData;` |
|      37 |  5565 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|      37 |  5566 | `		if( bIsStrict ){` |
|       - |  5567 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|       - |  5568 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|      33 |  5569 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|     ! 0 |  5570 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5571 | `					"declare(strict_types) value must be a literal");` |
|     ! 0 |  5572 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5573 | `				return SXRET_OK;` |
|       - |  5574 | `			}` |
|      33 |  5575 | `			iStrictValue = -1;` |
|      33 |  5576 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|      33 |  5577 | `				const char *zv = SyStringData(&pValTok->sData);` |
|      33 |  5578 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|      33 |  5579 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|      31 |  5580 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|      14 |  5581 | `			}` |
|      33 |  5582 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|       3 |  5583 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5584 | `					"strict_types declaration must have 0 or 1 as its value");` |
|       3 |  5585 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5586 | `				return SXRET_OK;` |
|       - |  5587 | `			}` |
|      30 |  5588 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|      17 |  5589 | `		}else{` |
|       - |  5590 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|       - |  5591 | `			 * preserve the legacy notice so callers relying on the old` |
|       - |  5592 | `			 * behavior don't regress. */` |
|       8 |  5593 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|       - |  5594 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 |  5595 | `				ph7_lib_version()` |
|       - |  5596 | `				);` |
|       - |  5597 | `		}` |
|      35 |  5598 | `		pCursor = pValTok + 1;` |
|       - |  5599 | `		/* Consume separating comma (or end). */` |
|      35 |  5600 | `		if( pCursor < pBodyEnd ){` |
|       3 |  5601 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5602 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5603 | `					"declare: Expecting ',' or ')' after directive value");` |
|     ! 0 |  5604 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5605 | `				return SXRET_OK;` |
|       - |  5606 | `			}` |
|       3 |  5607 | `			pCursor++;` |
|       1 |  5608 | `		}` |
|       5 |  5609 | `	}` |
|       - |  5610 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|       - |  5611 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|       - |  5612 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|      33 |  5613 | `	return SXRET_OK;` |
|       2 |  5614 | `Synchro:` |
|       - |  5615 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      16 |  5616 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      12 |  5617 | `		pGen->pIn++;` |
|       2 |  5618 | `	}` |
|       6 |  5619 | `	return SXRET_OK;` |
|      25 |  5620 |  |
|       - |  5621 | `/*` |
|       - |  5622 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - |  5623 | ` * as follows:` |
|       - |  5624 | ` * function makecoffee($type = "cappuccino")` |
|       - |  5625 | ` * {` |
|       - |  5626 | ` *   return "Making a cup of $type.\n";` |
|       - |  5627 | ` * }` |
|       - |  5628 | ` * Symisc eXtension.` |
|       - |  5629 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - |  5630 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - |  5631 | ` *      Example: Work only with PH7,generate error under zend` |
|       - |  5632 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - |  5633 | ` *      {` |
|       - |  5634 | ` *       var_dump($a);` |
|       - |  5635 | ` *      }` |
|       - |  5636 | ` *     //call test without args` |
|       - |  5637 | ` *      test();` |
|       - |  5638 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - |  5639 | ` *      Example:` |
|       - |  5640 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - |  5641 | ` * 3 -) Function overloading!!` |
|       - |  5642 | ` *      Example:` |
|       - |  5643 | ` *      function foo($a) {` |
|       - |  5644 | ` *   	  return $a.PHP_EOL;` |
|       - |  5645 | ` *	    }` |
|       - |  5646 | ` *	    function foo($a, $b) {` |
|       - |  5647 | ` *   	  return $a + $b;` |
|       - |  5648 | ` *	    }` |
|       - |  5649 | ` *	    echo foo(5); // Prints "5"` |
|       - |  5650 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - |  5651 | ` *      // Same arg` |
|       - |  5652 | ` *	   function foo(string $a)` |
|       - |  5653 | ` *	   {` |
|       - |  5654 | ` *	     echo "a is a string\n";` |
|       - |  5655 | ` *	     var_dump($a);` |
|       - |  5656 | ` *	   }` |
|       - |  5657 | ` *	  function foo(int $a)` |
|       - |  5658 | ` *	  {` |
|       - |  5659 | ` *	    echo "a is integer\n";` |
|       - |  5660 | ` *	    var_dump($a);` |
|       - |  5661 | ` *	  }` |
|       - |  5662 | ` *	  function foo(array $a)` |
|       - |  5663 | ` *	  {` |
|       - |  5664 | ` * 	    echo "a is an array\n";` |
|       - |  5665 | ` * 	    var_dump($a);` |
|       - |  5666 | ` *	  }` |
|       - |  5667 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - |  5668 | ` *	  foo(52); // a is integer [second foo]` |
|       - |  5669 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - |  5670 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - |  5671 | ` * introduced by the PH7 engine.` |
|       - |  5672 | ` */` |
|   74716 |  5673 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       5 |  5674 |  |
|       - |  5675 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5676 | `	SySet *pInstrContainer;` |
|       - |  5677 | `	sxi32 rc;` |
|       - |  5678 | `	/* Swap token stream */` |
|   74721 |  5679 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   74721 |  5680 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   74721 |  5681 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - |  5682 | `	/* Compile the expression holding the argument value */` |
|   74721 |  5683 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5684 | `	/* Emit the done instruction */` |
|   74721 |  5685 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   74721 |  5686 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   74721 |  5687 | `	RE_SWAP_DELIMITER(pGen);` |
|   74721 |  5688 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  5689 | `		return SXERR_ABORT;` |
|       - |  5690 | `	}` |
|   74721 |  5691 | `	return SXRET_OK;` |
|   37363 |  5692 |  |
|       - |  5693 | `/*` |
|       - |  5694 | ` * Collect function arguments one after one.` |
|       - |  5695 | ` * According to the PHP language reference manual.` |
|       - |  5696 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - |  5697 | ` * list of expressions.` |
|       - |  5698 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - |  5699 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - |  5700 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - |  5701 | ` * for more information.` |
|       - |  5702 | ` * Example #1 Passing arrays to functions` |
|       - |  5703 | ` * <?php` |
|       - |  5704 | ` * function takes_array($input)` |
|       - |  5705 | ` * {` |
|       - |  5706 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - |  5707 | ` * }` |
|       - |  5708 | ` * ?>` |
|       - |  5709 | ` * Making arguments be passed by reference` |
|       - |  5710 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - |  5711 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - |  5712 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - |  5713 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - |  5714 | ` * to the argument name in the function definition:` |
|       - |  5715 | ` * Example #2 Passing function parameters by reference` |
|       - |  5716 | ` * <?php` |
|       - |  5717 | ` * function add_some_extra(&$string)` |
|       - |  5718 | ` * {` |
|       - |  5719 | ` *   $string .= 'and something extra.';` |
|       - |  5720 | ` * }` |
|       - |  5721 | ` * $str = 'This is a string, ';` |
|       - |  5722 | ` * add_some_extra($str);` |
|       - |  5723 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - |  5724 | ` * ?>` |
|       - |  5725 | ` *` |
|       - |  5726 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - |  5727 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - |  5728 | ` * on these extension.` |
|       - |  5729 | ` */` |
|  104508 |  5730 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|       5 |  5731 |  |
|       - |  5732 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - |  5733 | `	SyToken *pIn;  /* Token stream */` |
|       - |  5734 | `	SyBlob sSig;         /* Function signature */` |
|       - |  5735 | `	char *zDup;          /* Copy of argument name */` |
|       - |  5736 | `	sxi32 rc;` |
|       - |  5737 |  |
|  104513 |  5738 | `	pIn = pGen->pIn;` |
|  104513 |  5739 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - |  5740 | `	/* Process arguments one after one */` |
|  135103 |  5741 | `	for(;;){` |
|  270211 |  5742 | `		if( pIn >= pEnd ){` |
|       - |  5743 | `			/* No more arguments to process */` |
|  104499 |  5744 | `			break;` |
|       - |  5745 | `		}` |
|  165717 |  5746 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|  165717 |  5747 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|  165717 |  5748 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|  165717 |  5749 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|       - |  5750 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|       - |  5751 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|       - |  5752 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|       - |  5753 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|       - |  5754 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|       - |  5755 | `		{` |
|  165717 |  5756 | `			int bReadonly = 0, bVisSeen = 0;` |
|  165717 |  5757 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|  165717 |  5758 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       3 |  5759 | `				bReadonly = 1;` |
|       3 |  5760 | `				pIn++;` |
|       1 |  5761 | `			}` |
|  165717 |  5762 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|   64245 |  5763 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   64245 |  5764 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      71 |  5765 | `					bVisSeen = 1;` |
|      71 |  5766 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|      95 |  5767 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|      31 |  5768 | `						: PH7_CLASS_PROT_PUBLIC;` |
|      71 |  5769 | `					pIn++;` |
|      71 |  5770 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|      16 |  5771 | `						bReadonly = 1;` |
|      16 |  5772 | `						pIn++;` |
|       6 |  5773 | `					}` |
|      33 |  5774 | `				}` |
|   32120 |  5775 | `			}` |
|  165717 |  5776 | `			if( bVisSeen \|\| bReadonly ){` |
|      73 |  5777 | `				if( !bCtorCtx ){` |
|       6 |  5778 | `					if( bAbstractCtx ){` |
|       3 |  5779 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5780 | `							"Cannot declare promoted property in an abstract constructor");` |
|       2 |  5781 | `					}else{` |
|       3 |  5782 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5783 | `							"Cannot declare promoted property outside a constructor");` |
|       - |  5784 | `					}` |
|       6 |  5785 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  5786 | `						return SXERR_ABORT;` |
|       - |  5787 | `					}` |
|       6 |  5788 | `					return SXERR_SYNTAX;` |
|       - |  5789 | `				}` |
|      69 |  5790 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      69 |  5791 | `				sArg.iPromoteVis = iVis;` |
|      69 |  5792 | `				if( bReadonly ){` |
|      18 |  5793 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|       7 |  5794 | `				}` |
|      32 |  5795 | `			}` |
|       - |  5796 | `		}` |
|       - |  5797 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|  165708 |  5798 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|  125705 |  5799 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   83916 |  5800 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   78545 |  5801 | `			sxu32 nLineLocal = pIn->nLine;` |
|   78545 |  5802 | `			sxi32 iTFlags = 0;` |
|   78545 |  5803 | `			pGen->pIn = pIn;` |
|   78545 |  5804 | `			rc = GenStateParseUnionTypeDecl(` |
|   39270 |  5805 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   39270 |  5806 | `				&iTFlags, &sArg.sTypeName,` |
|       - |  5807 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|       - |  5808 | `				/* bAllowVoid */ 0,` |
|   39270 |  5809 | `						nLineLocal);` |
|   78545 |  5810 | `			pIn = pGen->pIn;` |
|   78545 |  5811 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5812 | `				return SXERR_ABORT;` |
|   78545 |  5813 | `			}else if( rc == SXERR_CORRUPT ){` |
|       - |  5814 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|       3 |  5815 | `				return SXERR_SYNTAX;` |
|   78543 |  5816 | `			}else if( rc == SXERR_SYNTAX ){` |
|       8 |  5817 | `				if( pIn < pEnd ){` |
|      11 |  5818 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|       - |  5819 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|       3 |  5820 | `						&pIn->sData);` |
|       5 |  5821 | `				}else{` |
|     ! 0 |  5822 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|       - |  5823 | `						"syntax error, unexpected end of file");` |
|       - |  5824 | `				}` |
|       8 |  5825 | `				return SXERR_SYNTAX;` |
|       - |  5826 | `			}` |
|   78537 |  5827 | `			sArg.iFlags \|= iTFlags;` |
|   39266 |  5828 | `		}` |
|  165705 |  5829 | `		if( pIn >= pEnd ){` |
|     ! 0 |  5830 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 |  5831 | `			return rc;` |
|       - |  5832 | `		}` |
|  165705 |  5833 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - |  5834 | `			/* Pass by reference,record that */` |
|    3589 |  5835 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    3589 |  5836 | `			pIn++;` |
|    1792 |  5837 | `		}` |
|  165705 |  5838 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - |  5839 | `			/* Variadic parameter: ...$args */` |
|    3605 |  5840 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|    3605 |  5841 | `			pIn++;` |
|    1800 |  5842 | `		}` |
|  165705 |  5843 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  5844 | `			/* Invalid argument */` |
|     ! 0 |  5845 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 |  5846 | `			return rc;` |
|       - |  5847 | `		}` |
|  165705 |  5848 | `		pIn++; /* Jump the dollar sign */` |
|       - |  5849 | `		/* Copy argument name */` |
|  165705 |  5850 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|  165705 |  5851 | `		if( zDup == 0 ){` |
|     ! 0 |  5852 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  5853 | `			return SXERR_ABORT;` |
|       - |  5854 | `		}` |
|  165705 |  5855 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|  165705 |  5856 | `		pIn++;` |
|  165705 |  5857 | `		if( pIn < pEnd ){` |
|  100363 |  5858 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - |  5859 | `				SyToken *pDefend;` |
|   74723 |  5860 | `				sxi32 iNest = 0;` |
|   74723 |  5861 | `				pIn++; /* Jump the equal sign */` |
|   74723 |  5862 | `				pDefend = pIn;` |
|       - |  5863 | `				/* Process the default value associated with this argument */` |
|  156551 |  5864 | `				while( pDefend < pEnd ){` |
|  117403 |  5865 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   35575 |  5866 | `						break;` |
|       - |  5867 | `					}` |
|   81833 |  5868 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - |  5869 | `						/* Increment nesting level */` |
|    3561 |  5870 | `						iNest++;` |
|   80055 |  5871 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - |  5872 | `						/* Decrement nesting level */` |
|    3561 |  5873 | `						iNest--;` |
|    1778 |  5874 | `					}` |
|   81833 |  5875 | `					pDefend++;` |
|       5 |  5876 | `				}` |
|   74723 |  5877 | `				if( pIn >= pDefend ){` |
|       3 |  5878 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 |  5879 | `					return rc;` |
|       - |  5880 | `				}` |
|       - |  5881 | `				/* Process default value */` |
|   74721 |  5882 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   74721 |  5883 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  5884 | `					return rc;` |
|       - |  5885 | `				}` |
|       - |  5886 | `				/* Point beyond the default value */` |
|   74721 |  5887 | `				pIn = pDefend;` |
|   37358 |  5888 | `			}` |
|  100361 |  5889 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5890 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 |  5891 | `				return rc;` |
|       - |  5892 | `			}` |
|  100361 |  5893 | `			pIn++; /* Jump the trailing comma */` |
|   50178 |  5894 | `		}` |
|       - |  5895 | `		/* Append argument signature */` |
|  165703 |  5896 | `		if( sArg.nType > 0 ){` |
|   78483 |  5897 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - |  5898 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|   14285 |  5899 | `				int marker = 'o';` |
|   14285 |  5900 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|   14285 |  5901 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    7145 |  5902 | `			}else{` |
|       - |  5903 | `				int c;` |
|   64203 |  5904 | `				c = 'n'; /* cc warning */` |
|       - |  5905 | `				/* Type leading character */` |
|   64203 |  5906 | `				switch(sArg.nType){` |
|       3 |  5907 | `				case MEMOBJ_HASHMAP:` |
|       - |  5908 | `					/* Hashmap aka 'array' */` |
|       7 |  5909 | `					c = 'h';` |
|       7 |  5910 | `					break;` |
|    8946 |  5911 | `				case MEMOBJ_INT:` |
|       - |  5912 | `					/* Integer */` |
|   17897 |  5913 | `					c = 'i';` |
|   17897 |  5914 | `					break;` |
|       2 |  5915 | `				case MEMOBJ_BOOL:` |
|       - |  5916 | `					/* Bool */` |
|       5 |  5917 | `					c = 'b';` |
|       5 |  5918 | `					break;` |
|       2 |  5919 | `				case MEMOBJ_REAL:` |
|       - |  5920 | `					/* Float */` |
|       5 |  5921 | `					c = 'f';` |
|       5 |  5922 | `					break;` |
|   23138 |  5923 | `				case MEMOBJ_STRING:` |
|       - |  5924 | `					/* String */` |
|   46281 |  5925 | `					c = 's';` |
|   46281 |  5926 | `					break;` |
|       7 |  5927 | `				case MEMOBJ_OBJ:` |
|       - |  5928 | `					/* Object */` |
|      16 |  5929 | `					c = 'o';` |
|      14 |  5930 | `					break;` |
|       1 |  5931 | `				default:` |
|       2 |  5932 | `					break;` |
|       - |  5933 | `				}` |
|   64203 |  5934 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - |  5935 | `			}` |
|   39244 |  5936 | `		}else{` |
|       - |  5937 | `			/* No type is associated with this parameter which mean` |
|       - |  5938 | `			 * that this function is not condidate for overloading.` |
|       - |  5939 | `			 */` |
|   87225 |  5940 | `			SyBlobRelease(&sSig);` |
|       - |  5941 | `		}` |
|       - |  5942 | `		/* Save in the argument set */` |
|  165703 |  5943 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       5 |  5944 | `	}` |
|  104499 |  5945 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - |  5946 | `		/* Save function signature */` |
|   50001 |  5947 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   24998 |  5948 | `	}` |
|  104499 |  5949 | `	return SXRET_OK;` |
|   52259 |  5950 |  |
|       - |  5951 | `/*` |
|       - |  5952 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - |  5953 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  5954 | ` * and this routine takes care of generating the appropriate error message.` |
|       - |  5955 | ` */` |
|  222880 |  5956 | `static sxi32 GenStateCompileFuncBody(` |
|       - |  5957 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  5958 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - |  5959 | `	)` |
|       5 |  5960 |  |
|       - |  5961 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - |  5962 | `	GenBlock *pBlock;` |
|       - |  5963 | `	sxu32 nGotoOfft;` |
|       - |  5964 | `	sxi32 rc;` |
|       - |  5965 | `	/* Attach the new function */` |
|  222885 |  5966 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  222885 |  5967 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  5968 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - |  5969 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5970 | `		return SXERR_ABORT;` |
|       - |  5971 | `	}` |
|  222885 |  5972 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - |  5973 | `	/* Swap bytecode containers */` |
|  222885 |  5974 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  222885 |  5975 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - |  5976 | `	/* Emit constructor property promotion prologue:` |
|       - |  5977 | `	 *   $this->NAME = $NAME;` |
|       - |  5978 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|       - |  5979 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|       - |  5980 | `	{` |
|  222885 |  5981 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|       - |  5982 | `		sxu32 i;` |
|  359993 |  5983 | `		for( i = 0; i < nArg; i++ ){` |
|  137113 |  5984 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|       - |  5985 | `			char *zSrc;` |
|       - |  5986 | `			sxu32 nSrc,nName;` |
|       - |  5987 | `			SySet sToken;` |
|       - |  5988 | `			SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5989 | `			sxi32 rcPromote;` |
|  137113 |  5990 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  137059 |  5991 | `				continue;` |
|       - |  5992 | `			}` |
|       - |  5993 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|       - |  5994 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|       - |  5995 | `			 * copied), so it must outlive the function — never free it. The` |
|       - |  5996 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|       - |  5997 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      59 |  5998 | `			nName = SyStringLength(&pArg->sName);` |
|      59 |  5999 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      59 |  6000 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      59 |  6001 | `			if( zSrc == 0 ){` |
|     ! 0 |  6002 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  6003 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  6004 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  6005 | `				return SXERR_ABORT;` |
|       - |  6006 | `			}` |
|       - |  6007 | `			{` |
|      59 |  6008 | `				char *z = zSrc;` |
|      59 |  6009 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      59 |  6010 | `				z += sizeof("$this->")-1;` |
|      59 |  6011 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      59 |  6012 | `				z += nName;` |
|      59 |  6013 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      59 |  6014 | `				z += sizeof(" = $")-1;` |
|      59 |  6015 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      59 |  6016 | `				z += nName;` |
|      59 |  6017 | `				*z = 0;` |
|       - |  6018 | `			}` |
|      59 |  6019 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      59 |  6020 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken);` |
|      59 |  6021 | `			pTmpIn = pGen->pIn;` |
|      59 |  6022 | `			pTmpEnd = pGen->pEnd;` |
|      59 |  6023 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      59 |  6024 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      59 |  6025 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      59 |  6026 | `			pGen->pIn = pTmpIn;` |
|      59 |  6027 | `			pGen->pEnd = pTmpEnd;` |
|      59 |  6028 | `			SySetRelease(&sToken);` |
|      59 |  6029 | `			if( rcPromote == SXERR_ABORT ){` |
|     ! 0 |  6030 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  6031 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  6032 | `				return SXERR_ABORT;` |
|       - |  6033 | `			}` |
|       - |  6034 | `			/* Discard the assignment result — this is a statement expression. */` |
|      59 |  6035 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      32 |  6036 | `		}` |
|       - |  6037 | `	}` |
|       - |  6038 | `	/* Compile the body */` |
|  222885 |  6039 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  6040 | `	/* Fix exception jumps now the destination is resolved */` |
|  222885 |  6041 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  6042 | `	/* Emit the final return if not yet done */` |
|  222885 |  6043 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  6044 | `	/* Fix gotos jumps now the destination is resolved */` |
|  222885 |  6045 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 |  6046 | `		rc = SXERR_ABORT;` |
|     ! 0 |  6047 | `	}` |
|  222885 |  6048 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - |  6049 | `	/* Restore the default container */` |
|  222885 |  6050 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  6051 | `	/* Leave function block */` |
|  222885 |  6052 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  222885 |  6053 | `	if( rc == SXERR_ABORT ){` |
|       - |  6054 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6055 | `		return SXERR_ABORT;` |
|       - |  6056 | `	}` |
|       - |  6057 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - |  6058 | `	{` |
|  222885 |  6059 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  6060 | `		sxu32 i;` |
| 4377787 |  6061 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 4155007 |  6062 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     105 |  6063 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     105 |  6064 | `				break;` |
|       - |  6065 | `			}` |
| 2077456 |  6066 | `		}` |
|       - |  6067 | `	}` |
|       - |  6068 | `	/* All done, function body compiled */` |
|  222885 |  6069 | `	return SXRET_OK;` |
|  111445 |  6070 |  |
|       - |  6071 | `/*` |
|       - |  6072 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - |  6073 | ` * According to the PHP language reference manual.` |
|       - |  6074 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - |  6075 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - |  6076 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - |  6077 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  6078 | ` *  Functions need not be defined before they are referenced.` |
|       - |  6079 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - |  6080 | ` *  a function even if they were defined inside and vice versa.` |
|       - |  6081 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - |  6082 | ` *  calls with over 32-64 recursion levels.` |
|       - |  6083 | ` *` |
|       - |  6084 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - |  6085 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - |  6086 | ` * on these extension.` |
|       - |  6087 | ` */` |
|       - |  6088 | `/*` |
|       - |  6089 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - |  6090 | ` */` |
|     492 |  6091 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       5 |  6092 |  |
|       - |  6093 | `	sxu32 i;` |
|    1345 |  6094 | `	for( i = 0; i < n; i++ ){` |
|    1157 |  6095 | `		int a = zA[i], b = zB[i];` |
|    1157 |  6096 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|    1157 |  6097 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|    1157 |  6098 | `		if( a != b ) return a - b;` |
|     429 |  6099 | `	}` |
|     193 |  6100 | `	return 0;` |
|     251 |  6101 |  |
|       - |  6102 | `/*` |
|       - |  6103 | ` * Internal type-atom kinds used during union type parsing.` |
|       - |  6104 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|       - |  6105 | ` * (which are positive bit values stored in sxu32).` |
|       - |  6106 | ` */` |
|       - |  6107 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|       - |  6108 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|       - |  6109 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|       - |  6110 |  |
|       - |  6111 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|       - |  6112 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|       - |  6113 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|       - |  6114 |  |
|       - |  6115 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|       - |  6116 | `struct PhlTypeAtom {` |
|       - |  6117 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|       - |  6118 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|       - |  6119 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|       - |  6120 | `	sxu32 nCanon;` |
|       - |  6121 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|       - |  6122 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|       - |  6123 | `};` |
|       - |  6124 |  |
|       - |  6125 | `/*` |
|       - |  6126 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|       - |  6127 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|       - |  6128 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|       - |  6129 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|       - |  6130 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|       - |  6131 | ` * already be consumed by the caller.` |
|       - |  6132 | ` */` |
|   79378 |  6133 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|       5 |  6134 |  |
|   79383 |  6135 | `	SyToken *pIn = pGen->pIn;` |
|   79383 |  6136 | `	SyZero(pOut, sizeof(*pOut));` |
|   79383 |  6137 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   79383 |  6138 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6139 | `		return SXERR_SYNTAX;` |
|       - |  6140 | `	}` |
|       - |  6141 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   79383 |  6142 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       8 |  6143 | `		pIn++;` |
|       8 |  6144 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6145 | `			return SXERR_SYNTAX;` |
|       - |  6146 | `		}` |
|       3 |  6147 | `	}` |
|   79383 |  6148 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6149 | `		return SXERR_SYNTAX;` |
|       - |  6150 | `	}` |
|   79383 |  6151 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|   64745 |  6152 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   64745 |  6153 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      32 |  6154 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   64731 |  6155 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|      71 |  6156 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   64684 |  6157 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|   18145 |  6158 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   55581 |  6159 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   46441 |  6160 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   23293 |  6161 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|      33 |  6162 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|      61 |  6163 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      27 |  6164 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      33 |  6165 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       9 |  6166 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|      19 |  6167 | `			pOut->nType = SXU32_HIGH;` |
|      19 |  6168 | `			pOut->sClass = pIn->sData;` |
|      11 |  6169 | `		}else{` |
|       3 |  6170 | `			return SXERR_SYNTAX;` |
|       - |  6171 | `		}` |
|   64743 |  6172 | `		pIn++;` |
|   32374 |  6173 | `	}else{` |
|       - |  6174 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|       - |  6175 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|   14643 |  6176 | `		SyString *pT = &pIn->sData;` |
|   14643 |  6177 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|      32 |  6178 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|      32 |  6179 | `			pIn++;` |
|   14629 |  6180 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|     157 |  6181 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|     157 |  6182 | `			pIn++;` |
|   14539 |  6183 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       3 |  6184 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       3 |  6185 | `			pIn++;` |
|       2 |  6186 | `		}else{` |
|       - |  6187 | `			/* Class / interface name; consume namespace path a\b\c */` |
|   14461 |  6188 | `			SyToken *pFirst = pIn;` |
|   14461 |  6189 | `			SyToken *pLast = pIn;` |
|   14461 |  6190 | `			pOut->nType = SXU32_HIGH;` |
|   14461 |  6191 | `			pOut->sClass = pIn->sData;` |
|   14461 |  6192 | `			pIn++;` |
|   21687 |  6193 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|   14464 |  6194 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       3 |  6195 | `				pLast = &pIn[1];` |
|       3 |  6196 | `				pIn += 2;` |
|       1 |  6197 | `			}` |
|   14461 |  6198 | `			if( pLast != pFirst ){` |
|       3 |  6199 | `				const char *zFirst = pFirst->sData.zString;` |
|       3 |  6200 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 |  6201 | `				pOut->sClass.zString = zFirst;` |
|       3 |  6202 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       1 |  6203 | `			}` |
|       - |  6204 | `		}` |
|       - |  6205 | `	}` |
|   79381 |  6206 | `	pGen->pIn = pIn;` |
|   79381 |  6207 | `	return SXRET_OK;` |
|   39694 |  6208 |  |
|       - |  6209 |  |
|       - |  6210 | `/*` |
|       - |  6211 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|       - |  6212 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|       - |  6213 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|       - |  6214 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|       - |  6215 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|       - |  6216 | ` */` |
|   79224 |  6217 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|       5 |  6218 |  |
|       - |  6219 | `	int i;` |
|   79229 |  6220 | `	int nNonNull = 0;` |
|   79229 |  6221 | `	int bAnyIntersection = 0;` |
|       - |  6222 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|   79229 |  6223 | `	sxu32 nMaxGroup = 0;` |
| 2614397 |  6224 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|  158587 |  6225 | `	for( i = 0; i < nAtoms; i++ ){` |
|   79363 |  6226 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   79335 |  6227 | `			nNonNull++;` |
|   79335 |  6228 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|   79335 |  6229 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|   79335 |  6230 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|   39665 |  6231 | `			}` |
|   39665 |  6232 | `		}` |
|   39684 |  6233 | `	}` |
|  158553 |  6234 | `	for( i = 0; i < nAtoms; i++ ){` |
|   79345 |  6235 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|      19 |  6236 | `			bAnyIntersection = 1;` |
|      19 |  6237 | `			break;` |
|       - |  6238 | `		}` |
|   39667 |  6239 | `	}` |
|   79229 |  6240 | `	if( bAnyIntersection ){` |
|       - |  6241 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|       - |  6242 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|       - |  6243 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|      19 |  6244 | `		sxu32 g, nGroups = 0;` |
|      19 |  6245 | `		int bFirstGroup = 1;` |
|      39 |  6246 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|      39 |  6247 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|      23 |  6248 | `			int bFirstMember = 1;` |
|       - |  6249 | `			int bWrap;` |
|      23 |  6250 | `			if( aGroupCount[g] == 0 ) continue;` |
|       - |  6251 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|       - |  6252 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|       - |  6253 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|       - |  6254 | `			 * parens, matching PHP's canonical text. */` |
|      31 |  6255 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|      23 |  6256 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|      23 |  6257 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|      71 |  6258 | `			for( i = 0; i < nAtoms; i++ ){` |
|      51 |  6259 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|      39 |  6260 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|      39 |  6261 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      37 |  6262 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      20 |  6263 | `				}else{` |
|       3 |  6264 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6265 | `				}` |
|      39 |  6266 | `				bFirstMember = 0;` |
|      21 |  6267 | `			}` |
|      23 |  6268 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|      23 |  6269 | `			bFirstGroup = 0;` |
|      13 |  6270 | `		}` |
|      19 |  6271 | `		if( bNullable ){` |
|     ! 0 |  6272 | `			SyBlobAppend(pBlob, "\|", 1);` |
|     ! 0 |  6273 | `			SyBlobAppend(pBlob, "null", 4);` |
|     ! 0 |  6274 | `		}` |
|      57 |  6275 | `		return;` |
|       - |  6276 | `	}` |
|   79213 |  6277 | `	if( nNonNull == 1 && bNullable ){` |
|       - |  6278 | `		/* Shorthand: ?T */` |
|      81 |  6279 | `		for( i = 0; i < nAtoms; i++ ){` |
|      81 |  6280 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      81 |  6281 | `			SyBlobAppend(pBlob, "?", 1);` |
|      81 |  6282 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      22 |  6283 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      13 |  6284 | `			}else{` |
|      63 |  6285 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6286 | `			}` |
|      81 |  6287 | `			return;` |
|     ! 0 |  6288 | `		}` |
|     ! 0 |  6289 | `	}` |
|       - |  6290 | `	{` |
|   79137 |  6291 | `		int bFirst = 1;` |
|       - |  6292 | `		/* 1) Classes in declaration order */` |
|  158371 |  6293 | `		for( i = 0; i < nAtoms; i++ ){` |
|   79239 |  6294 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|   14425 |  6295 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   14425 |  6296 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|   14425 |  6297 | `				bFirst = 0;` |
|    7210 |  6298 | `			}` |
|   39622 |  6299 | `		}` |
|       - |  6300 | `		/* 2) Built-ins in canonical order */` |
|       - |  6301 | `		{` |
|       - |  6302 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|       - |  6303 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|       - |  6304 | `			int k;` |
|  553929 |  6305 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  885445 |  6306 | `				for( i = 0; i < nAtoms; i++ ){` |
|  475301 |  6307 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|   64653 |  6308 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   64653 |  6309 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|   64653 |  6310 | `						bFirst = 0;` |
|   64653 |  6311 | `						break;` |
|       - |  6312 | `					}` |
|  205329 |  6313 | `				}` |
|  237401 |  6314 | `			}` |
|       - |  6315 | `		}` |
|       - |  6316 | `		/* 3) null suffix */` |
|   79137 |  6317 | `		if( bNullable ){` |
|      20 |  6318 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|      20 |  6319 | `			SyBlobAppend(pBlob, "null", 4);` |
|       8 |  6320 | `		}` |
|       - |  6321 | `	}` |
|   39617 |  6322 |  |
|       - |  6323 |  |
|       - |  6324 | `/*` |
|       - |  6325 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|       - |  6326 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|       - |  6327 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|       - |  6328 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|       - |  6329 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|       - |  6330 | ` * whether it was parenthesized.` |
|       - |  6331 | ` *` |
|       - |  6332 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|       - |  6333 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|       - |  6334 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|       - |  6335 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|       - |  6336 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|       - |  6337 | ` */` |
|   79360 |  6338 | `static sxi32 GenStateParsePart(` |
|       - |  6339 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|       - |  6340 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|       5 |  6341 |  |
|       - |  6342 | `	sxi32 rc;` |
|   79365 |  6343 | `	int nMembers = 0;` |
|   79365 |  6344 | `	int bParen = 0;` |
|   79365 |  6345 | `	*pnMembers = 0;` |
|   79365 |  6346 | `	*pbParen = 0;` |
|   79365 |  6347 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       6 |  6348 | `		bParen = 1;` |
|       6 |  6349 | `		pGen->pIn++; /* skip '(' */` |
|       2 |  6350 | `	}` |
|   39680 |  6351 | `	for(;;){` |
|   79383 |  6352 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|     ! 0 |  6353 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6354 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|     ! 0 |  6355 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6356 | `		}` |
|   79383 |  6357 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|   79383 |  6358 | `		if( rc != SXRET_OK ){` |
|       3 |  6359 | `			return rc;` |
|       - |  6360 | `		}` |
|   79381 |  6361 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|   79381 |  6362 | `		(*pnAtoms)++;` |
|   79381 |  6363 | `		nMembers++;` |
|       - |  6364 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|   79381 |  6365 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      24 |  6366 | `			SyToken *pNext = &pGen->pIn[1];` |
|      20 |  6367 | `			if( pNext < pGen->pEnd` |
|      24 |  6368 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      22 |  6369 | `				pGen->pIn++; /* skip '&' */` |
|      22 |  6370 | `				continue;` |
|       - |  6371 | `			}` |
|       1 |  6372 | `		}` |
|   79363 |  6373 | `		break;` |
|     ! 0 |  6374 | `	}` |
|   79363 |  6375 | `	if( bParen ){` |
|       6 |  6376 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6377 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6378 | `				"Malformed DNF type: expecting ')'");` |
|     ! 0 |  6379 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6380 | `		}` |
|       6 |  6381 | `		pGen->pIn++; /* skip ')' */` |
|       6 |  6382 | `		if( nMembers < 2 ){` |
|     ! 0 |  6383 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6384 | `				"Parenthesized type must be an intersection of at least two types");` |
|     ! 0 |  6385 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6386 | `		}` |
|       2 |  6387 | `	}` |
|   79363 |  6388 | `	*pnMembers = nMembers;` |
|   79363 |  6389 | `	*pbParen = bParen;` |
|   79363 |  6390 | `	return SXRET_OK;` |
|   39685 |  6391 |  |
|       - |  6392 |  |
|       - |  6393 | `/*` |
|       - |  6394 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|       - |  6395 | ` *` |
|       - |  6396 | ` * Outputs:` |
|       - |  6397 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|       - |  6398 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|       - |  6399 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|       - |  6400 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|       - |  6401 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|       - |  6402 | ` *     already be initialized by the caller (allocator set, etc).` |
|       - |  6403 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|       - |  6404 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|       - |  6405 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|       - |  6406 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|       - |  6407 | ` *` |
|       - |  6408 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|       - |  6409 | ` * SXERR_ABORT on fatal compile errors.` |
|       - |  6410 | ` */` |
|   79236 |  6411 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |  6412 | `	ph7_gen_state *pGen,` |
|       - |  6413 | `	sxu32 *pnType,` |
|       - |  6414 | `	SyString *pClass,` |
|       - |  6415 | `	SySet *pAlts,` |
|       - |  6416 | `	sxi32 *piTypeFlags,` |
|       - |  6417 | `	SyString *pTypeText,` |
|       - |  6418 | `	int iNullableFlag,` |
|       - |  6419 | `	int iUnionFlag,` |
|       - |  6420 | `	int bAllowVoid,` |
|       - |  6421 | `	sxu32 nLine` |
|       5 |  6422 | `){` |
|       - |  6423 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   79241 |  6424 | `	int nAtoms = 0;` |
|   79241 |  6425 | `	int bShortNullable = 0;` |
|   79241 |  6426 | `	int bExplicitNull = 0;` |
|       - |  6427 | `	sxi32 rc;` |
|   79241 |  6428 | `	*pnType = 0;` |
|   79241 |  6429 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   79241 |  6430 | `	*piTypeFlags = 0;` |
|   79241 |  6431 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|       - |  6432 |  |
|   79241 |  6433 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6434 | `		return SXRET_OK;` |
|       - |  6435 | `	}` |
|       - |  6436 | ``	/* Optional `?` shorthand prefix */`` |
|   79236 |  6437 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      71 |  6438 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      71 |  6439 | `		bShortNullable = 1;` |
|      71 |  6440 | `		pGen->pIn++;` |
|      71 |  6441 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6442 | `			return SXERR_SYNTAX;` |
|       - |  6443 | `		}` |
|      33 |  6444 | `	}` |
|       - |  6445 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|       - |  6446 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|       - |  6447 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|       - |  6448 | `	{` |
|       - |  6449 | `		int nMembers, bParen;` |
|   79241 |  6450 | `		sxu32 iGroup = 0;` |
|   79241 |  6451 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|   79241 |  6452 | `		if( rc != SXRET_OK ){` |
|       4 |  6453 | `			return rc;` |
|       - |  6454 | `		}` |
|       - |  6455 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|       - |  6456 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|       - |  6457 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|       - |  6458 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|       - |  6459 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|  119039 |  6460 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   79427 |  6461 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|     131 |  6462 | `			if( bShortNullable ){` |
|       - |  6463 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|       - |  6464 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|       - |  6465 | `				 * already reported" so callers skip their own error emission. */` |
|       3 |  6466 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6467 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|       3 |  6468 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|       - |  6469 | `			}` |
|     129 |  6470 | `			if( nMembers >= 2 && !bParen ){` |
|     ! 0 |  6471 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|       - |  6472 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|     ! 0 |  6473 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6474 | `			}` |
|     129 |  6475 | ``			pGen->pIn++; /* skip `\|` */`` |
|     129 |  6476 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|     129 |  6477 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  6478 | `				return rc;` |
|       - |  6479 | `			}` |
|       5 |  6480 | `		}` |
|   79237 |  6481 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|     ! 0 |  6482 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6483 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|     ! 0 |  6484 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6485 | `		}` |
|       - |  6486 | `	}` |
|       - |  6487 | `	/* Validation pass.` |
|       - |  6488 | `	 *` |
|       - |  6489 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|       - |  6490 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|       - |  6491 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|       - |  6492 | `	 */` |
|       - |  6493 | `	{` |
|       - |  6494 | `		int i, j;` |
|   79237 |  6495 | `		int bHasNonNull = 0;` |
|   79237 |  6496 | `		int bAnyIntersection = 0;` |
|       - |  6497 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|       - |  6498 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|       - |  6499 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
| 2614661 |  6500 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|  158611 |  6501 | `		for( i = 0; i < nAtoms; i++ ){` |
|   79379 |  6502 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|   39692 |  6503 | `		}` |
|  158573 |  6504 | `		for( i = 0; i < nAtoms; i++ ){` |
|   79359 |  6505 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|   39673 |  6506 | `		}` |
|       - |  6507 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|       - |  6508 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|   79237 |  6509 | `		if( bShortNullable && bAnyIntersection ){` |
|     ! 0 |  6510 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6511 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|     ! 0 |  6512 | `			return SXERR_SYNTAX;` |
|       - |  6513 | `		}` |
|  158601 |  6514 | `		for( i = 0; i < nAtoms; i++ ){` |
|       - |  6515 | `			/* Intersection members must be class/interface types (PHP rejects` |
|       - |  6516 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|       - |  6517 | ``			 * `true`/`false` in an intersection). */`` |
|   79377 |  6518 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|      38 |  6519 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|      38 |  6520 | `				if( bClassLike ){` |
|      35 |  6521 | `					SyString *pC = &aAtoms[i].sClass;` |
|      32 |  6522 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|      32 |  6523 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|      32 |  6524 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|      35 |  6525 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|     ! 0 |  6526 | `						bClassLike = 0;` |
|     ! 0 |  6527 | `					}` |
|      16 |  6528 | `				}` |
|      38 |  6529 | `				if( !bClassLike ){` |
|       - |  6530 | `					const char *zName; sxu32 nName;` |
|       3 |  6531 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6532 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6533 | `					}else{` |
|       3 |  6534 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|       - |  6535 | `					}` |
|       4 |  6536 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6537 | `						"Type %.*s cannot be part of an intersection type",` |
|       1 |  6538 | `						(int)nName, zName);` |
|       3 |  6539 | `					return SXERR_SYNTAX;` |
|       - |  6540 | `				}` |
|      16 |  6541 | `			}` |
|   79375 |  6542 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|     157 |  6543 | `				if( nAtoms > 1 ){` |
|       3 |  6544 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6545 | `						"Void can only be used as a standalone type");` |
|       3 |  6546 | `					return SXERR_SYNTAX;` |
|       - |  6547 | `				}` |
|     155 |  6548 | `				if( !bAllowVoid ){` |
|     ! 0 |  6549 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6550 | `						"void cannot be used here");` |
|     ! 0 |  6551 | `					return SXERR_SYNTAX;` |
|       - |  6552 | `				}` |
|     155 |  6553 | `				if( bShortNullable ){` |
|     ! 0 |  6554 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6555 | `						"Void type cannot be nullable");` |
|     ! 0 |  6556 | `					return SXERR_SYNTAX;` |
|       - |  6557 | `				}` |
|      75 |  6558 | `			}` |
|   79373 |  6559 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|       - |  6560 | ``				/* `never` is parsed but not yet implemented in the type`` |
|       - |  6561 | `				 * system. Reject it explicitly rather than silently aliasing` |
|       - |  6562 | ``				 * to `void` — the two have different semantics (never =`` |
|       - |  6563 | `				 * does not return), and folding them would mislead any` |
|       - |  6564 | `				 * future return-enforcement work. */` |
|       3 |  6565 | `				if( nAtoms > 1 ){` |
|       3 |  6566 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6567 | `						"never can only be used as a standalone type");` |
|       3 |  6568 | `					return SXERR_SYNTAX;` |
|       - |  6569 | `				}` |
|     ! 0 |  6570 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6571 | `					"never type is not yet implemented");` |
|     ! 0 |  6572 | `				return SXERR_SYNTAX;` |
|       - |  6573 | `			}` |
|   79371 |  6574 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|      32 |  6575 | `				bExplicitNull = 1;` |
|      18 |  6576 | `			}else{` |
|   79343 |  6577 | `				bHasNonNull = 1;` |
|       - |  6578 | `			}` |
|       - |  6579 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|       - |  6580 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|       - |  6581 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|       - |  6582 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|       - |  6583 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|   79551 |  6584 | `			for( j = 0; j < i; j++ ){` |
|     187 |  6585 | `				int bDup = 0;` |
|     187 |  6586 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|     359 |  6587 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|     182 |  6588 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|     187 |  6589 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|     179 |  6590 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      40 |  6591 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      34 |  6592 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|      37 |  6593 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|      16 |  6594 | `								aAtoms[j].sClass.zString,` |
|      32 |  6595 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|     ! 0 |  6596 | `							bDup = 1;` |
|     ! 0 |  6597 | `						}` |
|      21 |  6598 | `					}else{` |
|       3 |  6599 | `						bDup = 1;` |
|       - |  6600 | `					}` |
|      18 |  6601 | `				}` |
|     179 |  6602 | `				if( bDup ){` |
|       - |  6603 | `					const char *zName;` |
|       - |  6604 | `					sxu32 nName;` |
|       3 |  6605 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6606 | `						zName = aAtoms[i].sClass.zString;` |
|     ! 0 |  6607 | `						nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6608 | `					}else{` |
|       3 |  6609 | `						zName = aAtoms[i].zCanon;` |
|       3 |  6610 | `						nName = aAtoms[i].nCanon;` |
|       - |  6611 | `					}` |
|       4 |  6612 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       1 |  6613 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|       3 |  6614 | `					return SXERR_SYNTAX;` |
|       - |  6615 | `				}` |
|      91 |  6616 | `			}` |
|   39687 |  6617 | `		}` |
|   79229 |  6618 | `		if( !bHasNonNull && bExplicitNull ){` |
|       7 |  6619 | `			if( bShortNullable ){` |
|       - |  6620 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|     ! 0 |  6621 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6622 | `					"Null can not be used as a standalone type");` |
|     ! 0 |  6623 | `				return SXERR_SYNTAX;` |
|       - |  6624 | `			}` |
|       - |  6625 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|       - |  6626 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|       - |  6627 | `			 * path below leaves *pnType untouched when there is no non-null` |
|       - |  6628 | `			 * atom, so set it here. */` |
|       7 |  6629 | `			*pnType = MEMOBJ_NULL;` |
|       3 |  6630 | `		}` |
|       - |  6631 | `	}` |
|       - |  6632 | `	/* Compute nullability flag */` |
|   79229 |  6633 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      97 |  6634 | `		*piTypeFlags \|= iNullableFlag;` |
|      46 |  6635 | `	}` |
|       - |  6636 | `	/* Build canonical type text */` |
|   79229 |  6637 | `	if( pTypeText ){` |
|       - |  6638 | `		SyBlob sBlob;` |
|   79229 |  6639 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|  118809 |  6640 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   39612 |  6641 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   79229 |  6642 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|  118616 |  6643 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   79074 |  6644 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   79079 |  6645 | `			if( zDup ){` |
|   79079 |  6646 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   39537 |  6647 | `			}` |
|   39537 |  6648 | `		}` |
|   79229 |  6649 | `		SyBlobRelease(&sBlob);` |
|   39612 |  6650 | `	}` |
|       - |  6651 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|       - |  6652 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|       - |  6653 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|       - |  6654 | `	{` |
|   79229 |  6655 | `		int nNonNull = 0;` |
|   79229 |  6656 | `		int iNonNullIdx = -1;` |
|       - |  6657 | `		int i;` |
|  158587 |  6658 | `		for( i = 0; i < nAtoms; i++ ){` |
|   79363 |  6659 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   79335 |  6660 | `				nNonNull++;` |
|   79335 |  6661 | `				iNonNullIdx = i;` |
|   39665 |  6662 | `			}` |
|   39684 |  6663 | `		}` |
|   79229 |  6664 | `		if( nNonNull <= 1 ){` |
|       - |  6665 | `			/* Fast path: store as single type. */` |
|   79137 |  6666 | `			if( iNonNullIdx >= 0 ){` |
|   79131 |  6667 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   79131 |  6668 | `				if( pA->nType == SXU32_HIGH ){` |
|   21602 |  6669 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    7199 |  6670 | `						pA->sClass.zString, pA->sClass.nByte);` |
|   14403 |  6671 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|   14403 |  6672 | `					*pnType = SXU32_HIGH;` |
|   14403 |  6673 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   71932 |  6674 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|     155 |  6675 | `					*pnType = MEMOBJ_VOID;` |
|      80 |  6676 | `				}else{` |
|       - |  6677 | `					/* UTA_NEVER_FLAG never reaches here — the validation` |
|       - |  6678 | `					 * pass above rejects it as not-yet-implemented. */` |
|   64583 |  6679 | `					*pnType = pA->nType;` |
|       - |  6680 | `				}` |
|   39563 |  6681 | `			}` |
|   39571 |  6682 | `		}else{` |
|       - |  6683 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      97 |  6684 | `			*piTypeFlags \|= iUnionFlag;` |
|     311 |  6685 | `			for( i = 0; i < nAtoms; i++ ){` |
|       - |  6686 | `				ph7_type_alt sAlt;` |
|     219 |  6687 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     209 |  6688 | `				SyZero(&sAlt, sizeof(sAlt));` |
|     209 |  6689 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|     209 |  6690 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|     116 |  6691 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      37 |  6692 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      79 |  6693 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      79 |  6694 | `					sAlt.nType = SXU32_HIGH;` |
|      79 |  6695 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      42 |  6696 | `				}else{` |
|     135 |  6697 | `					sAlt.nType = aAtoms[i].nType;` |
|     135 |  6698 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|       - |  6699 | `				}` |
|     209 |  6700 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|     107 |  6701 | `			}` |
|       - |  6702 | `		}` |
|       - |  6703 | `	}` |
|   79229 |  6704 | `	return SXRET_OK;` |
|   39623 |  6705 |  |
|       - |  6706 |  |
|       - |  6707 | `/*` |
|       - |  6708 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - |  6709 | `` * pGen->pIn should point to the token after `)`.`` |
|       - |  6710 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - |  6711 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - |  6712 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|       - |  6713 | `` *          and union types `: T\|U`.`` |
|       - |  6714 | ` */` |
|  315560 |  6715 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       5 |  6716 |  |
|  315565 |  6717 | `	sxi32 iFlags = 0;` |
|       - |  6718 | `	sxi32 rc;` |
|       - |  6719 | `	sxu32 nLine;` |
|  315565 |  6720 | `	pFunc->nReturnType = 0;` |
|  315565 |  6721 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  315565 |  6722 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|  315565 |  6723 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  315081 |  6724 | `		return SXRET_OK;` |
|       - |  6725 | `	}` |
|     489 |  6726 | `	pGen->pIn++; /* Skip ':' */` |
|     489 |  6727 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6728 | `		return SXRET_OK;` |
|       - |  6729 | `	}` |
|     489 |  6730 | `	nLine = pGen->pIn->nLine;` |
|     489 |  6731 | `	rc = GenStateParseUnionTypeDecl(` |
|     242 |  6732 | `		pGen,` |
|     242 |  6733 | `		&pFunc->nReturnType,` |
|     242 |  6734 | `		&pFunc->sReturnClass,` |
|     242 |  6735 | `		&pFunc->aReturnUnion,` |
|       - |  6736 | `		&iFlags,` |
|     242 |  6737 | `		&pFunc->sReturnTypeName,` |
|       - |  6738 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|       - |  6739 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|       - |  6740 | `		/* iUnionFlag */ 0,` |
|       - |  6741 | `		/* bAllowVoid */ 1,` |
|     242 |  6742 | `		nLine);` |
|     489 |  6743 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6744 | `		return SXERR_ABORT;` |
|       - |  6745 | `	}` |
|     489 |  6746 | `	if( rc == SXERR_CORRUPT ){` |
|       - |  6747 | `		/* Error already reported */` |
|     ! 0 |  6748 | `		return SXERR_SYNTAX;` |
|       - |  6749 | `	}` |
|     489 |  6750 | `	if( rc == SXERR_SYNTAX ){` |
|       6 |  6751 | `		if( pGen->pIn < pGen->pEnd ){` |
|       8 |  6752 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6753 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       4 |  6754 | `				&pGen->pIn->sData);` |
|       4 |  6755 | `		}else{` |
|     ! 0 |  6756 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|       - |  6757 | `				"syntax error, unexpected end of file in return type declaration");` |
|       - |  6758 | `		}` |
|       6 |  6759 | `		return SXERR_SYNTAX;` |
|       - |  6760 | `	}` |
|     485 |  6761 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     485 |  6762 | `	return SXRET_OK;` |
|  157785 |  6763 |  |
|       - |  6764 |  |
|   47576 |  6765 | `static sxi32 GenStateCompileFunc(` |
|       - |  6766 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  6767 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - |  6768 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  6769 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - |  6770 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - |  6771 | `	)` |
|       5 |  6772 |  |
|       - |  6773 | `	ph7_vm_func *pFunc;` |
|       - |  6774 | `	SyToken *pEnd;` |
|       - |  6775 | `	sxu32 nLine;` |
|       - |  6776 | `	char *zName;` |
|       - |  6777 | `	sxi32 rc;` |
|       - |  6778 | `	/* Extract line number */` |
|   47581 |  6779 | `	nLine = pGen->pIn->nLine;` |
|       - |  6780 | `	/* Jump the left parenthesis '(' */` |
|   47581 |  6781 | `	pGen->pIn++;` |
|       - |  6782 | `	/* Delimit the function signature */` |
|   47581 |  6783 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   47581 |  6784 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  6785 | `		/* Syntax error */` |
|       9 |  6786 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       9 |  6787 | `		if( rc == SXERR_ABORT ){` |
|       - |  6788 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6789 | `			return SXERR_ABORT;` |
|       - |  6790 | `		}` |
|       9 |  6791 | `		pGen->pIn = pGen->pEnd;` |
|       9 |  6792 | `		return SXRET_OK;` |
|       - |  6793 | `	}` |
|       - |  6794 | `	/* Create the function state */` |
|   47575 |  6795 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   47575 |  6796 | `	if( pFunc == 0 ){` |
|     ! 0 |  6797 | `		goto OutOfMem;` |
|       - |  6798 | `	}` |
|       - |  6799 | `	/* Build the function name, prepending namespace if active */` |
|   47582 |  6800 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - |  6801 | `		SyBlob sFQN;` |
|       - |  6802 | `		sxu32 nLen;` |
|      16 |  6803 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 |  6804 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 |  6805 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 |  6806 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 |  6807 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 |  6808 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 |  6809 | `		SyBlobRelease(&sFQN);` |
|      16 |  6810 | `		if( zName == 0 ){` |
|     ! 0 |  6811 | `			goto OutOfMem;` |
|       - |  6812 | `		}` |
|      16 |  6813 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 |  6814 | `	}else{` |
|   47561 |  6815 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   47561 |  6816 | `		if( zName == 0 ){` |
|     ! 0 |  6817 | `			goto OutOfMem;` |
|       - |  6818 | `		}` |
|   47561 |  6819 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - |  6820 | `	}` |
|   47575 |  6821 | `	if( pGen->pIn < pEnd ){` |
|       - |  6822 | `		/* Collect function arguments */` |
|   32825 |  6823 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   32825 |  6824 | `		if( rc == SXERR_ABORT ){` |
|       - |  6825 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6826 | `			return SXERR_ABORT;` |
|       - |  6827 | `		}` |
|   16410 |  6828 | `	}` |
|       - |  6829 | `	/* Point past ')' and parse optional return type ': type' */` |
|   47575 |  6830 | `	pGen->pIn = &pEnd[1];` |
|       - |  6831 | `	{` |
|   47575 |  6832 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   47575 |  6833 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  6834 | `			return SXERR_ABORT;` |
|   47575 |  6835 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       6 |  6836 | `			return SXERR_SYNTAX;` |
|       - |  6837 | `		}` |
|       - |  6838 | `	}` |
|   47571 |  6839 | `	if( bHandleClosure ){` |
|       - |  6840 | `		ph7_vm_func_closure_env sEnv;` |
|     297 |  6841 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     292 |  6842 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     160 |  6843 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      23 |  6844 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  6845 | `				/* Closure,record environment variable */` |
|      23 |  6846 | `				pGen->pIn++;` |
|      23 |  6847 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  6848 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 |  6849 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  6850 | `						return SXERR_ABORT;` |
|       - |  6851 | `					}` |
|     ! 0 |  6852 | `				}` |
|      23 |  6853 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - |  6854 | `				/* Compile until we hit the first closing parenthesis */` |
|      45 |  6855 | `				while( pGen->pIn < pGen->pEnd ){` |
|      45 |  6856 | `					int iFlagsLocal = 0;` |
|      45 |  6857 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      23 |  6858 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      23 |  6859 | `						break;` |
|       - |  6860 | `					}` |
|      27 |  6861 | `					nLineLocal = pGen->pIn->nLine;` |
|      27 |  6862 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - |  6863 | `						/* Pass by reference,record that */` |
|     ! 0 |  6864 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - |  6865 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - |  6866 | `							);` |
|     ! 0 |  6867 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 |  6868 | `						pGen->pIn++;` |
|     ! 0 |  6869 | `					}` |
|      22 |  6870 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      27 |  6871 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6872 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  6873 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 |  6874 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  6875 | `								return SXERR_ABORT;` |
|       - |  6876 | `							}` |
|       - |  6877 | `							/* Find the closing parenthesis */` |
|     ! 0 |  6878 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6879 | `								pGen->pIn++;` |
|     ! 0 |  6880 | `							}` |
|     ! 0 |  6881 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 |  6882 | `								pGen->pIn++;` |
|     ! 0 |  6883 | `							}` |
|     ! 0 |  6884 | `							break;` |
|       - |  6885 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 |  6886 | `					}else{` |
|       - |  6887 | `						SyString *pNameLocal;` |
|       - |  6888 | `						char *zDup;` |
|       - |  6889 | `						/* Duplicate variable name */` |
|      27 |  6890 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      27 |  6891 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      27 |  6892 | `						if( zDup ){` |
|       - |  6893 | `							/* Zero the structure */` |
|      27 |  6894 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      27 |  6895 | `							sEnv.iFlags = iFlagsLocal;` |
|      27 |  6896 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      27 |  6897 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      27 |  6898 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 |  6899 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 |  6900 | `									got_this = 1;` |
|     ! 0 |  6901 | `							}` |
|       - |  6902 | `							/* Save imported variable */` |
|      27 |  6903 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      16 |  6904 | `						}else{` |
|     ! 0 |  6905 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6906 | `							 return SXERR_ABORT;` |
|       - |  6907 | `						}` |
|       - |  6908 | `					}` |
|      27 |  6909 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      33 |  6910 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6911 | `						/* Ignore trailing commas */` |
|       7 |  6912 | `						pGen->pIn++;` |
|       1 |  6913 | `					}` |
|       5 |  6914 | `				}` |
|      23 |  6915 | `				if( !got_this ){` |
|       - |  6916 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - |  6917 | `					 * available to the closure environment.` |
|       - |  6918 | `					 */` |
|      23 |  6919 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      23 |  6920 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      23 |  6921 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      23 |  6922 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      23 |  6923 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       9 |  6924 | `				}` |
|      23 |  6925 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - |  6926 | `					/* Mark as closure */` |
|      23 |  6927 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       9 |  6928 | `				}` |
|       9 |  6929 | `		}` |
|     146 |  6930 | `	}` |
|       - |  6931 | `	/* Compile the body */` |
|   47571 |  6932 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   47571 |  6933 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6934 | `		return SXERR_ABORT;` |
|       - |  6935 | `	}` |
|   47571 |  6936 | `	if( ppFunc ){` |
|     297 |  6937 | `		*ppFunc = pFunc;` |
|     146 |  6938 | `	}` |
|   47571 |  6939 | `	rc = SXRET_OK;` |
|   47571 |  6940 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - |  6941 | `		/* Finally register the function */` |
|   47553 |  6942 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   23774 |  6943 | `	}` |
|   47571 |  6944 | `	if( rc == SXRET_OK ){` |
|   47571 |  6945 | `		return SXRET_OK;` |
|       - |  6946 | `	}` |
|       - |  6947 | `	/* Fall through if something goes wrong */` |
|     ! 0 |  6948 | `OutOfMem:` |
|       - |  6949 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  6950 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  6951 | `	 */` |
|     ! 0 |  6952 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  6953 | `	return SXERR_ABORT;` |
|   23793 |  6954 |  |
|       - |  6955 | `/*` |
|       - |  6956 | ` * Compile a standard PHP function.` |
|       - |  6957 | ` *  Refer to the block-comment above for more information.` |
|       - |  6958 | ` */` |
|   47292 |  6959 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       5 |  6960 |  |
|       - |  6961 | `	SyString *pName;` |
|       - |  6962 | `	sxi32 iFlags;` |
|       - |  6963 | `	sxu32 nLine;` |
|       - |  6964 | `	sxi32 rc;` |
|       - |  6965 |  |
|   47297 |  6966 | `	nLine = pGen->pIn->nLine;` |
|   47297 |  6967 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   47297 |  6968 | `	iFlags = 0;` |
|   47297 |  6969 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  6970 | `		/* Return by reference,remember that */` |
|       7 |  6971 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  6972 | `		/* Jump the '&' token */` |
|       7 |  6973 | `		pGen->pIn++;` |
|       3 |  6974 | `	}` |
|   47297 |  6975 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  6976 | `		/* Invalid function name */` |
|       7 |  6977 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       7 |  6978 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6979 | `			return SXERR_ABORT;` |
|       - |  6980 | `		}` |
|       - |  6981 | `		/* Sychronize with the next semi-colon or braces*/` |
|      21 |  6982 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      15 |  6983 | `			pGen->pIn++;` |
|       1 |  6984 | `		}` |
|       7 |  6985 | `		return SXRET_OK;` |
|       - |  6986 | `	}` |
|   47291 |  6987 | `	pName = &pGen->pIn->sData;` |
|   47291 |  6988 | `	nLine = pGen->pIn->nLine;` |
|       - |  6989 | `	/* Jump the function name */` |
|   47291 |  6990 | `	pGen->pIn++;` |
|   47291 |  6991 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  6992 | `		/* Syntax error */` |
|       3 |  6993 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 |  6994 | `		if( rc == SXERR_ABORT ){` |
|       - |  6995 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6996 | `			return SXERR_ABORT;` |
|       - |  6997 | `		}` |
|       - |  6998 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 |  6999 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  7000 | `			pGen->pIn++;` |
|     ! 0 |  7001 | `		}` |
|       3 |  7002 | `		return SXRET_OK;` |
|       - |  7003 | `	}` |
|       - |  7004 | `	/* Compile function body */` |
|   47289 |  7005 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   47289 |  7006 | `	return rc;` |
|   23651 |  7007 |  |
|       - |  7008 | `/*` |
|       - |  7009 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  7010 | ` * According to the PHP language reference manual` |
|       - |  7011 | ` *  Visibility:` |
|       - |  7012 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  7013 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  7014 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  7015 | ` *  Members declared protected can be accessed only within the class` |
|       - |  7016 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  7017 | ` *  may only be accessed by the class that defines the member.` |
|       - |  7018 | ` */` |
|  343280 |  7019 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       5 |  7020 |  |
|  343285 |  7021 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|   21451 |  7022 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  321839 |  7023 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   46287 |  7024 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  7025 | `	}` |
|       - |  7026 | `	/* Assume public by default */` |
|  275557 |  7027 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  171645 |  7028 |  |
|       - |  7029 | `/*` |
|       - |  7030 | ` * Compile a class constant.` |
|       - |  7031 | ` * According to the PHP language reference manual` |
|       - |  7032 | ` *  Class Constants` |
|       - |  7033 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - |  7034 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - |  7035 | ` *   you don't use the $ symbol to declare or use them.` |
|       - |  7036 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - |  7037 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - |  7038 | ` *   It's also possible for interfaces to have constants.` |
|       - |  7039 | ` * Symisc eXtension.` |
|       - |  7040 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - |  7041 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  7042 | ` *  Example:` |
|       - |  7043 | ` *   class Test{` |
|       - |  7044 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  7045 | ` *   };` |
|       - |  7046 | ` *   var_dump(TEST::MyConst);` |
|       - |  7047 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  7048 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  7049 | ` */` |
|       - |  7050 | `/*` |
|       - |  7051 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|       - |  7052 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|       - |  7053 | ` * token immediately followed by '='. Anything else with a leading type token` |
|       - |  7054 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|       - |  7055 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|       - |  7056 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|       - |  7057 | ` */` |
|      78 |  7058 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|       5 |  7059 |  |
|       - |  7060 | `	SyToken *p0, *p1;` |
|      83 |  7061 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7062 | `		return 0;` |
|       - |  7063 | `	}` |
|      83 |  7064 | `	p0 = pGen->pIn;` |
|       - |  7065 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|      83 |  7066 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|     ! 0 |  7067 | `		return 1;` |
|       - |  7068 | `	}` |
|      83 |  7069 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|       5 |  7070 | `		return 1;` |
|       - |  7071 | `	}` |
|       - |  7072 | `	/* A name-like first token begins a type only when followed by another` |
|       - |  7073 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|       - |  7074 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|      79 |  7075 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      79 |  7076 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|      79 |  7077 | `		if( p1 ){` |
|      79 |  7078 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|      24 |  7079 | `				return 1;` |
|       - |  7080 | `			}` |
|      59 |  7081 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|       5 |  7082 | `				return 1;` |
|       - |  7083 | `			}` |
|      25 |  7084 | `		}` |
|      25 |  7085 | `	}` |
|      55 |  7086 | `	return 0;` |
|      44 |  7087 |  |
|       - |  7088 | `/*` |
|       - |  7089 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|       - |  7090 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|       - |  7091 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|       - |  7092 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|       - |  7093 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|       - |  7094 | ` * share the same backing.` |
|       - |  7095 | ` */` |
|     206 |  7096 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|       - |  7097 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|       5 |  7098 |  |
|     211 |  7099 | `	pAttr->nType = nType;` |
|     211 |  7100 | `	pAttr->sClass = *pClass;` |
|     211 |  7101 | `	pAttr->sTypeName = *pTypeName;` |
|     211 |  7102 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7103 | `		sxu32 i;` |
|      66 |  7104 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      46 |  7105 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|      46 |  7106 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      25 |  7107 | `		}` |
|      10 |  7108 | `	}` |
|     211 |  7109 |  |
|      78 |  7110 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7111 |  |
|      83 |  7112 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7113 | `	SySet *pInstrContainer;` |
|       - |  7114 | `	ph7_class_attr *pCons;` |
|       - |  7115 | `	SyString *pName;` |
|       - |  7116 | `	sxi32 rc;` |
|      83 |  7117 | `	sxu32 nType = 0;` |
|       - |  7118 | `	SyString sTypeClass;` |
|       - |  7119 | `	SyString sTypeText;` |
|       - |  7120 | `	SySet aUnionAlts;` |
|      83 |  7121 | `	sxi32 iTypeFlags = 0;` |
|      83 |  7122 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|      83 |  7123 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|      83 |  7124 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7125 | `	/* Extract visibility level */` |
|      83 |  7126 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7127 | `	/* Mark as constant */` |
|      83 |  7128 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      83 |  7129 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       - |  7130 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|       - |  7131 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|      97 |  7132 | `	if( GenStateClassConstHasType(pGen) ){` |
|      46 |  7133 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|      28 |  7134 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|       - |  7135 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|       - |  7136 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|       - |  7137 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|       - |  7138 | `		 * and success paths release. */` |
|      32 |  7139 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7140 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7141 | `			goto Synchronize;` |
|      32 |  7142 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7143 | `			return SXERR_ABORT;` |
|      32 |  7144 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7145 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  7146 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|     ! 0 |  7147 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7148 | `				return SXERR_ABORT;` |
|       - |  7149 | `			}` |
|     ! 0 |  7150 | `			goto Synchronize;` |
|       - |  7151 | `		}` |
|      32 |  7152 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      14 |  7153 | `	}` |
|      39 |  7154 | `loop:` |
|      85 |  7155 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7156 | `		/* Invalid constant name */` |
|     ! 0 |  7157 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  7158 | `		if( rc == SXERR_ABORT ){` |
|       - |  7159 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7160 | `			return SXERR_ABORT;` |
|       - |  7161 | `		}` |
|     ! 0 |  7162 | `		goto Synchronize;` |
|       - |  7163 | `	}` |
|       - |  7164 | `	/* Peek constant name */` |
|      85 |  7165 | `	pName = &pGen->pIn->sData;` |
|       - |  7166 | `	/* Make sure the constant name isn't reserved */` |
|      85 |  7167 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  7168 | `		/* Reserved constant name */` |
|     ! 0 |  7169 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 |  7170 | `		if( rc == SXERR_ABORT ){` |
|       - |  7171 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7172 | `			return SXERR_ABORT;` |
|       - |  7173 | `		}` |
|     ! 0 |  7174 | `		goto Synchronize;` |
|       - |  7175 | `	}` |
|       - |  7176 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|      85 |  7177 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      46 |  7178 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|      28 |  7179 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      14 |  7180 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|      32 |  7181 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7182 | `			return SXERR_ABORT;` |
|      32 |  7183 | `		}else if( rc != SXRET_OK ){` |
|       3 |  7184 | `			goto Synchronize;` |
|       - |  7185 | `		}` |
|      13 |  7186 | `	}` |
|       - |  7187 | `	/* Advance the stream cursor */` |
|      83 |  7188 | `	pGen->pIn++;` |
|      83 |  7189 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  7190 | `		/* Invalid declaration */` |
|     ! 0 |  7191 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  7192 | `		if( rc == SXERR_ABORT ){` |
|       - |  7193 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7194 | `			return SXERR_ABORT;` |
|       - |  7195 | `		}` |
|     ! 0 |  7196 | `		goto Synchronize;` |
|       - |  7197 | `	}` |
|      83 |  7198 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  7199 | `	/* Allocate a new class attribute */` |
|      83 |  7200 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|      83 |  7201 | `	if( pCons == 0 ){` |
|     ! 0 |  7202 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7203 | `		return SXERR_ABORT;` |
|       - |  7204 | `	}` |
|      83 |  7205 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      29 |  7206 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      13 |  7207 | `	}` |
|       - |  7208 | `	/* Swap bytecode container */` |
|      83 |  7209 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      83 |  7210 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  7211 | `	/* Compile constant value.` |
|       - |  7212 | `	 */` |
|      83 |  7213 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      83 |  7214 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  7215 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  7216 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7217 | `			return SXERR_ABORT;` |
|       - |  7218 | `		}` |
|       1 |  7219 | `	}` |
|       - |  7220 | `	/* Emit the done instruction */` |
|      83 |  7221 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      83 |  7222 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      83 |  7223 | `	if( rc == SXERR_ABORT ){` |
|       - |  7224 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  7225 | `		return SXERR_ABORT;` |
|       - |  7226 | `	}` |
|       - |  7227 | `	/* All done,install the constant */` |
|      83 |  7228 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      83 |  7229 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7230 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7231 | `		return SXERR_ABORT;` |
|       - |  7232 | `	}` |
|      83 |  7233 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7234 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|       3 |  7235 | `		pGen->pIn++; /* Jump the comma */` |
|       3 |  7236 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  7237 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7238 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7239 | `				pTok--;` |
|     ! 0 |  7240 | `			}` |
|     ! 0 |  7241 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7242 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  7243 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7244 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7245 | `				return SXERR_ABORT;` |
|       - |  7246 | `			}` |
|     ! 0 |  7247 | `		}else{` |
|       3 |  7248 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       3 |  7249 | `				goto loop;` |
|       - |  7250 | `			}` |
|       - |  7251 | `		}` |
|     ! 0 |  7252 | `	}` |
|      81 |  7253 | `	SySetRelease(&aUnionAlts);` |
|      81 |  7254 | `	return SXRET_OK;` |
|       1 |  7255 | `Synchronize:` |
|       3 |  7256 | `	SySetRelease(&aUnionAlts);` |
|       - |  7257 | `	/* Synchronize with the first semi-colon */` |
|       9 |  7258 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       7 |  7259 | `		pGen->pIn++;` |
|       1 |  7260 | `	}` |
|       3 |  7261 | `	return SXERR_CORRUPT;` |
|      44 |  7262 |  |
|       - |  7263 | `/*` |
|       - |  7264 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  7265 | ` * According to the PHP language reference manual` |
|       - |  7266 | ` *  Properties` |
|       - |  7267 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  7268 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  7269 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  7270 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  7271 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  7272 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  7273 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  7274 | ` * Symisc eXtension.` |
|       - |  7275 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  7276 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  7277 | ` *  Example:` |
|       - |  7278 | ` *   class Test{` |
|       - |  7279 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  7280 | ` *   };` |
|       - |  7281 | ` *   var_dump(TEST::myVar);` |
|       - |  7282 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  7283 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  7284 | ` */` |
|       - |  7285 | `/*` |
|       - |  7286 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  7287 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  7288 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  7289 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  7290 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  7291 | ` */` |
|  186030 |  7292 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       5 |  7293 |  |
|  186035 |  7294 | `	SyToken *p = pStart;` |
|  186035 |  7295 | `	int bFirst = 1;` |
|  186035 |  7296 | `	if( p >= pEnd ) return 0;` |
|       - |  7297 | ``	/* Optional nullable `?` shorthand. */`` |
|  186035 |  7298 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      18 |  7299 | `		p++;` |
|      18 |  7300 | `		if( p >= pEnd ) return 0;` |
|       8 |  7301 | `	}` |
|       - |  7302 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|       - |  7303 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|       - |  7304 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|       - |  7305 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   93015 |  7306 | `	for(;;){` |
|  186053 |  7307 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|       - |  7308 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|       3 |  7309 | `			p++;` |
|       9 |  7310 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|       3 |  7311 | `			if( p >= pEnd ) return 0;` |
|       3 |  7312 | `			p++; /* skip ')' */` |
|       2 |  7313 | `		}else{` |
|       - |  7314 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|       - |  7315 | ``			 * then any `&`-joined intersection members. */`` |
|  186051 |  7316 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|  186051 |  7317 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  7318 | `				return 0;` |
|       - |  7319 | `			}` |
|       - |  7320 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|       - |  7321 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|       - |  7322 | `			 * may still appear at the initial dispatch site). */` |
|  186051 |  7323 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|  186005 |  7324 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  186000 |  7325 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|   10888 |  7326 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  185851 |  7327 | `					return 0;` |
|       - |  7328 | `				}` |
|      77 |  7329 | `			}` |
|     205 |  7330 | `			p++;` |
|     207 |  7331 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  7332 | `				p += 2;` |
|       1 |  7333 | `			}` |
|     303 |  7334 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|     208 |  7335 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  7336 | `				p++; /* skip '&' */` |
|       3 |  7337 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|       3 |  7338 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|       3 |  7339 | `				p++;` |
|       3 |  7340 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  7341 | `					p += 2;` |
|     ! 0 |  7342 | `				}` |
|       1 |  7343 | `			}` |
|       - |  7344 | `		}` |
|     207 |  7345 | `		bFirst = 0;` |
|     202 |  7346 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      23 |  7347 | `			&& p->sData.zString[0] == '\|' ){` |
|      22 |  7348 | ``			p++; /* next `\|`-separated part */`` |
|      22 |  7349 | `			continue;` |
|       - |  7350 | `		}` |
|     189 |  7351 | `		break;` |
|     ! 0 |  7352 | `	}` |
|     189 |  7353 | `	if( p >= pEnd ) return 0;` |
|     189 |  7354 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   93020 |  7355 |  |
|       - |  7356 |  |
|       - |  7357 | `/*` |
|       - |  7358 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  7359 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  7360 | ` * if not). Recognized forms:` |
|       - |  7361 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  7362 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  7363 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  7364 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  7365 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  7366 | ` * on unrecoverable error.` |
|       - |  7367 | ` *` |
|       - |  7368 | ` * When a type is parsed:` |
|       - |  7369 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  7370 | ` *   *pClass is set to the class name (for class types)` |
|       - |  7371 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  7372 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  7373 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  7374 | ` */` |
|     184 |  7375 | `static sxi32 GenStateParsePropertyType(` |
|       - |  7376 | `	ph7_gen_state *pGen,` |
|       - |  7377 | `	sxu32 *pnType,` |
|       - |  7378 | `	SyString *pClass,` |
|       - |  7379 | `	sxi32 *piTypeFlags,` |
|       - |  7380 | `	SyString *pTypeText,` |
|       - |  7381 | `	SySet *pAlts` |
|       5 |  7382 | `){` |
|     189 |  7383 | `	sxi32 iFlags = 0;` |
|       - |  7384 | `	sxi32 rc;` |
|     189 |  7385 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7386 | `		return SXRET_OK;` |
|       - |  7387 | `	}` |
|       - |  7388 | `	/* If the first token is '$', there's no type */` |
|     189 |  7389 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  7390 | `		return SXRET_OK;` |
|       - |  7391 | `	}` |
|     189 |  7392 | `	rc = GenStateParseUnionTypeDecl(` |
|      92 |  7393 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  7394 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  7395 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  7396 | `		/* bAllowVoid */ 0,` |
|     184 |  7397 | `		pGen->pIn->nLine);` |
|     189 |  7398 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7399 | `		return rc;` |
|       - |  7400 | `	}` |
|       - |  7401 | `	/* Verify next token is '$' (start of property name) */` |
|     189 |  7402 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7403 | `		return SXERR_SYNTAX;` |
|       - |  7404 | `	}` |
|     189 |  7405 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     189 |  7406 | `	return SXRET_OK;` |
|      97 |  7407 |  |
|       - |  7408 |  |
|       - |  7409 | `/*` |
|       - |  7410 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - |  7411 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - |  7412 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - |  7413 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - |  7414 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - |  7415 | ` * by the type parser itself before reaching here.` |
|       - |  7416 | ` *` |
|       - |  7417 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - |  7418 | ` * use in the error message.` |
|       - |  7419 | ` */` |
|     326 |  7420 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - |  7421 | `	sxu32 nType,` |
|       - |  7422 | `	const SyString *pClass,` |
|       - |  7423 | `	const char **pzName,` |
|       - |  7424 | `	sxu32 *pnName)` |
|       5 |  7425 |  |
|       - |  7426 | `	const char *z;` |
|       - |  7427 | `	sxu32 n;` |
|     331 |  7428 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     277 |  7429 | `		return 0;` |
|       - |  7430 | `	}` |
|      59 |  7431 | `	z = pClass->zString;` |
|      59 |  7432 | `	n = pClass->nByte;` |
|      59 |  7433 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       8 |  7434 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - |  7435 | `	}` |
|       - |  7436 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|       - |  7437 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|       - |  7438 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|      52 |  7439 | `	return 0;` |
|     168 |  7440 |  |
|       - |  7441 |  |
|       - |  7442 | `/*` |
|       - |  7443 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|       - |  7444 | ` * constant) — the main atom plus any union alternatives — against the` |
|       - |  7445 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|       - |  7446 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|       - |  7447 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|       - |  7448 | ` * type T" vs "Class constant C::X cannot have type T").` |
|       - |  7449 | ` *` |
|       - |  7450 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - |  7451 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - |  7452 | ` */` |
|     268 |  7453 | `static sxi32 GenStateValidateMemberType(` |
|       - |  7454 | `	ph7_gen_state *pGen,` |
|       - |  7455 | `	ph7_class *pClass,` |
|       - |  7456 | `	const SyString *pMemberName,` |
|       - |  7457 | `	sxu32 nType,` |
|       - |  7458 | `	const SyString *pTypeClass,` |
|       - |  7459 | `	const SyString *pTypeText,` |
|       - |  7460 | `	SySet *pUnionAlts,` |
|       - |  7461 | `	const char *zErrFmt,` |
|       - |  7462 | `	sxu32 nLine)` |
|       5 |  7463 |  |
|     273 |  7464 | `	const char *zBad = 0;` |
|     273 |  7465 | `	sxu32 nBad = 0;` |
|       - |  7466 | `	SyString sFallback;` |
|       - |  7467 | `	const SyString *pBad;` |
|       - |  7468 | `	sxi32 rc;` |
|     273 |  7469 | `	int bDisallowed = 0;` |
|     273 |  7470 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       5 |  7471 | `		bDisallowed = 1;` |
|     271 |  7472 | `	}else if( pUnionAlts ){` |
|       - |  7473 | `		sxu32 i;` |
|      88 |  7474 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      62 |  7475 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      62 |  7476 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 |  7477 | `				bDisallowed = 1;` |
|       3 |  7478 | `				break;` |
|       - |  7479 | `			}` |
|      32 |  7480 | `		}` |
|      14 |  7481 | `	}` |
|     273 |  7482 | `	if( !bDisallowed ){` |
|     267 |  7483 | `		return SXRET_OK;` |
|       - |  7484 | `	}` |
|       - |  7485 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - |  7486 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - |  7487 | `	 * canonical spelling if the type text is unavailable. */` |
|       8 |  7488 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       8 |  7489 | `		pBad = pTypeText;` |
|       5 |  7490 | `	}else{` |
|     ! 0 |  7491 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 |  7492 | `		pBad = &sFallback;` |
|       - |  7493 | `	}` |
|      11 |  7494 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       3 |  7495 | `		zErrFmt,` |
|       3 |  7496 | `		&pClass->sName,pMemberName,pBad);` |
|       8 |  7497 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7498 | `		return SXERR_ABORT;` |
|       - |  7499 | `	}` |
|       8 |  7500 | `	return SXERR_SYNTAX;` |
|     139 |  7501 |  |
|       - |  7502 | `/*` |
|       - |  7503 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|       - |  7504 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|       - |  7505 | ` * matched as a plain identifier in the class-member modifier position rather` |
|       - |  7506 | ` * than promoted to a lexer keyword.` |
|       - |  7507 | ` */` |
| 1647004 |  7508 | `static int GenStateIsReadonly(SyToken *pTok)` |
|       5 |  7509 |  |
| 1680658 |  7510 | `	return (pTok->nType & PH7_TK_ID)` |
|  857151 |  7511 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 1680653 |  7512 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|       5 |  7513 |  |
|   75366 |  7514 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7515 |  |
|   75371 |  7516 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7517 | `	ph7_class_attr *pAttr;` |
|       - |  7518 | `	SyString *pName;` |
|       - |  7519 | `	sxi32 rc;` |
|   75371 |  7520 | `	sxu32 nType = 0;` |
|       - |  7521 | `	SyString sTypeClass;` |
|       - |  7522 | `	SyString sTypeText;` |
|       - |  7523 | `	SySet aUnionAlts;` |
|   75371 |  7524 | `	sxi32 iTypeFlags = 0;` |
|   75371 |  7525 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   75371 |  7526 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   75371 |  7527 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7528 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|       - |  7529 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|       - |  7530 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   75371 |  7531 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|      21 |  7532 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  7533 | `	}` |
|       - |  7534 | `	/* Extract visibility level */` |
|   75371 |  7535 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7536 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   75463 |  7537 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     189 |  7538 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     189 |  7539 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7540 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7541 | `			goto Synchronize;` |
|     189 |  7542 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  7543 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7544 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 |  7545 | `				&pGen->pIn->sData);` |
|     ! 0 |  7546 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7547 | `				return SXERR_ABORT;` |
|       - |  7548 | `			}` |
|     ! 0 |  7549 | `			goto Synchronize;` |
|     189 |  7550 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7551 | `			return SXERR_ABORT;` |
|       - |  7552 | `		}` |
|      92 |  7553 | `	}` |
|     ! 0 |  7554 | `loop:` |
|   75375 |  7555 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7556 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 |  7557 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7558 | `			return SXERR_ABORT;` |
|       - |  7559 | `		}` |
|     ! 0 |  7560 | `		goto Synchronize;` |
|       - |  7561 | `	}` |
|   75375 |  7562 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   75375 |  7563 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - |  7564 | `		/* Invalid attribute name */` |
|     ! 0 |  7565 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 |  7566 | `		if( rc == SXERR_ABORT ){` |
|       - |  7567 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7568 | `			return SXERR_ABORT;` |
|       - |  7569 | `		}` |
|     ! 0 |  7570 | `		goto Synchronize;` |
|       - |  7571 | `	}` |
|       - |  7572 | `	/* Peek attribute name */` |
|   75375 |  7573 | `	pName = &pGen->pIn->sData;` |
|       - |  7574 | `	/* Advance the stream cursor */` |
|   75375 |  7575 | `	pGen->pIn++;` |
|   75375 |  7576 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - |  7577 | `		/* Invalid declaration */` |
|       3 |  7578 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 |  7579 | `		if( rc == SXERR_ABORT ){` |
|       - |  7580 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7581 | `			return SXERR_ABORT;` |
|       - |  7582 | `		}` |
|       3 |  7583 | `		goto Synchronize;` |
|       - |  7584 | `	}` |
|       - |  7585 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|       - |  7586 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   75373 |  7587 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|      39 |  7588 | `		const char *zRoErr = 0;` |
|      39 |  7589 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|       3 |  7590 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|      38 |  7591 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       6 |  7592 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|      35 |  7593 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|       6 |  7594 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|       2 |  7595 | `		}` |
|      39 |  7596 | `		if( zRoErr ){` |
|      13 |  7597 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|      13 |  7598 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7599 | `				return SXERR_ABORT;` |
|       - |  7600 | `			}` |
|      13 |  7601 | `			goto Synchronize;` |
|       - |  7602 | `		}` |
|      12 |  7603 | `	}` |
|       - |  7604 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - |  7605 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - |  7606 | `	 * by the type parser. */` |
|   75363 |  7607 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     278 |  7608 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - |  7609 | `			&sTypeText,` |
|     182 |  7610 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      91 |  7611 | `			"Property %z::$%z cannot have type %z",nLine);` |
|     187 |  7612 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7613 | `			return SXERR_ABORT;` |
|     187 |  7614 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7615 | `			goto Synchronize;` |
|       - |  7616 | `		}` |
|      91 |  7617 | `	}` |
|       - |  7618 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   75363 |  7619 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 |  7620 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7621 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 |  7622 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7623 | `			return SXERR_ABORT;` |
|       - |  7624 | `		}` |
|       3 |  7625 | `		goto Synchronize;` |
|       - |  7626 | `	}` |
|       - |  7627 | `	/* Allocate a new class attribute */` |
|   75361 |  7628 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   75361 |  7629 | `	if( pAttr == 0 ){` |
|     ! 0 |  7630 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  7631 | `		return SXERR_ABORT;` |
|       - |  7632 | `	}` |
|   75361 |  7633 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     185 |  7634 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      90 |  7635 | `	}` |
|   75361 |  7636 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - |  7637 | `		SySet *pInstrContainer;` |
|   21833 |  7638 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - |  7639 | `		/* Swap bytecode container */` |
|   21833 |  7640 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   21833 |  7641 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - |  7642 | `		/* Compile attribute value.` |
|       - |  7643 | `		 */` |
|   21833 |  7644 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   21833 |  7645 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  7646 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 |  7647 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7648 | `				return SXERR_ABORT;` |
|       - |  7649 | `			}` |
|     ! 0 |  7650 | `		}` |
|       - |  7651 | `		/* Emit the done instruction */` |
|   21833 |  7652 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   21833 |  7653 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   10914 |  7654 | `	}` |
|       - |  7655 | `	/* All done,install the attribute */` |
|   75361 |  7656 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   75361 |  7657 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7658 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7659 | `		return SXERR_ABORT;` |
|       - |  7660 | `	}` |
|   75361 |  7661 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7662 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 |  7663 | `		pGen->pIn++; /* Jump the comma */` |
|       5 |  7664 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  7665 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7666 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7667 | `				pTok--;` |
|     ! 0 |  7668 | `			}` |
|     ! 0 |  7669 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7670 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 |  7671 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7672 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7673 | `				return SXERR_ABORT;` |
|       - |  7674 | `			}` |
|     ! 0 |  7675 | `		}else{` |
|       5 |  7676 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 |  7677 | `				goto loop;` |
|       - |  7678 | `			}` |
|       - |  7679 | `		}` |
|     ! 0 |  7680 | `	}` |
|   75357 |  7681 | `	SySetRelease(&aUnionAlts);` |
|   75357 |  7682 | `	return SXRET_OK;` |
|       7 |  7683 | `Synchronize:` |
|       - |  7684 | `	/* Synchronize with the first semi-colon */` |
|      31 |  7685 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      16 |  7686 | `		pGen->pIn++;` |
|       2 |  7687 | `	}` |
|      17 |  7688 | `	SySetRelease(&aUnionAlts);` |
|      17 |  7689 | `	return SXERR_CORRUPT;` |
|   37688 |  7690 |  |
|       - |  7691 | `/*` |
|       - |  7692 | ` * Compile a class method.` |
|       - |  7693 | ` *` |
|       - |  7694 | ` * Refer to the official documentation for more information` |
|       - |  7695 | ` * on the powerful extension introduced by the PH7 engine` |
|       - |  7696 | ` * to the OO subsystem such as full type hinting,method` |
|       - |  7697 | ` * overloading and many more.` |
|       - |  7698 | ` */` |
|  267836 |  7699 | `static sxi32 GenStateCompileClassMethod(` |
|       - |  7700 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  7701 | `	sxi32 iProtection,   /* Visibility level */` |
|       - |  7702 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - |  7703 | `	int doBody,          /* TRUE to process method body */` |
|       - |  7704 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - |  7705 | `	)` |
|       5 |  7706 |  |
|  267841 |  7707 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7708 | `	ph7_class_method *pMeth;` |
|       - |  7709 | `	sxi32 iFuncFlags;` |
|       - |  7710 | `	SyString *pName;` |
|       - |  7711 | `	SyToken *pEnd;` |
|       - |  7712 | `	sxi32 rc;` |
|       - |  7713 | `	/* Extract visibility level */` |
|  267841 |  7714 | `	iProtection = GetProtectionLevel(iProtection);` |
|  267841 |  7715 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  267841 |  7716 | `	iFuncFlags = 0;` |
|  267841 |  7717 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7718 | `		/* Invalid method name */` |
|     ! 0 |  7719 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7720 | `		if( rc == SXERR_ABORT ){` |
|       - |  7721 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7722 | `			return SXERR_ABORT;` |
|       - |  7723 | `		}` |
|     ! 0 |  7724 | `		goto Synchronize;` |
|       - |  7725 | `	}` |
|  267841 |  7726 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  7727 | `		/* Return by reference,remember that */` |
|     ! 0 |  7728 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  7729 | `		/* Jump the '&' token */` |
|     ! 0 |  7730 | `		pGen->pIn++;` |
|     ! 0 |  7731 | `	}` |
|  267841 |  7732 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  7733 | `		/* Invalid method name */` |
|     ! 0 |  7734 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7735 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7736 | `			return SXERR_ABORT;` |
|       - |  7737 | `		}` |
|     ! 0 |  7738 | `		goto Synchronize;` |
|       - |  7739 | `	}` |
|       - |  7740 | `	/* Peek method name */` |
|  267841 |  7741 | `	pName = &pGen->pIn->sData;` |
|  267841 |  7742 | `	nLine = pGen->pIn->nLine;` |
|       - |  7743 | `	/* Jump the method name */` |
|  267841 |  7744 | `	pGen->pIn++;` |
|  267841 |  7745 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - |  7746 | `		/* Abstract method */` |
|   92515 |  7747 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 |  7748 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7749 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 |  7750 | `				&pClass->sName,pName);` |
|     ! 0 |  7751 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7752 | `				return SXERR_ABORT;` |
|       - |  7753 | `			}` |
|     ! 0 |  7754 | `		}` |
|       - |  7755 | `		/* Assemble method signature only */` |
|   92515 |  7756 | `		doBody = FALSE;` |
|   46255 |  7757 | `	}` |
|  267841 |  7758 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  7759 | `		/* Syntax error */` |
|     ! 0 |  7760 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 |  7761 | `		if( rc == SXERR_ABORT ){` |
|       - |  7762 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7763 | `			return SXERR_ABORT;` |
|       - |  7764 | `		}` |
|     ! 0 |  7765 | `		goto Synchronize;` |
|       - |  7766 | `	}` |
|       - |  7767 | `	/* Allocate a new class_method instance */` |
|  267841 |  7768 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  267841 |  7769 | `	if( pMeth == 0 ){` |
|     ! 0 |  7770 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7771 | `		return SXERR_ABORT;` |
|       - |  7772 | `	}` |
|       - |  7773 | `	/* Jump the left parenthesis '(' */` |
|  267841 |  7774 | `	pGen->pIn++;` |
|  267841 |  7775 | `	pEnd = 0; /* cc warning */` |
|       - |  7776 | `	/* Delimit the method signature */` |
|  267841 |  7777 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  267841 |  7778 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7779 | `		/* Syntax error */` |
|       3 |  7780 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 |  7781 | `		if( rc == SXERR_ABORT ){` |
|       - |  7782 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7783 | `			return SXERR_ABORT;` |
|       - |  7784 | `		}` |
|       3 |  7785 | `		goto Synchronize;` |
|       - |  7786 | `	}` |
|       - |  7787 | `	{` |
|  267839 |  7788 | `		int bIsCtor = 0;` |
|  267839 |  7789 | `		int bAbstractCtor = 0;` |
|  267834 |  7790 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  158935 |  7791 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  257089 |  7792 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   21505 |  7793 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       3 |  7794 | `				bAbstractCtor = 1;` |
|       2 |  7795 | `			}else{` |
|   21503 |  7796 | `				bIsCtor = 1;` |
|       - |  7797 | `			}` |
|   10750 |  7798 | `		}` |
|  267839 |  7799 | `		if( pGen->pIn < pEnd ){` |
|       - |  7800 | `			/* Collect method arguments */` |
|   71595 |  7801 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   71595 |  7802 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7803 | `				return SXERR_ABORT;` |
|       - |  7804 | `			}` |
|   35795 |  7805 | `		}` |
|       - |  7806 | `	}` |
|       - |  7807 | `	/* Point past ')' and parse optional return type ': type' */` |
|  267839 |  7808 | `	pGen->pIn = &pEnd[1];` |
|       - |  7809 | `	{` |
|  267839 |  7810 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  267839 |  7811 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  7812 | `			return SXERR_ABORT;` |
|  267839 |  7813 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 |  7814 | `			goto Synchronize;` |
|       - |  7815 | `		}` |
|       - |  7816 | `	}` |
|       - |  7817 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - |  7818 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - |  7819 | `	 * since we mint real ph7_class_attr entries. */` |
|       - |  7820 | `	{` |
|  267839 |  7821 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - |  7822 | `		sxu32 i;` |
|  389323 |  7823 | `		for( i = 0; i < nArg; i++ ){` |
|  121499 |  7824 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - |  7825 | `			ph7_class_attr *pAttr;` |
|  121499 |  7826 | `			sxi32 iAttrFlags = 0;` |
|       - |  7827 | `			int bArgTyped;` |
|  121499 |  7828 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  121435 |  7829 | `				continue;` |
|       - |  7830 | `			}` |
|       - |  7831 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|       - |  7832 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|       - |  7833 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|      49 |  7834 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|      70 |  7835 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|      69 |  7836 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 |  7837 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7838 | `					"Cannot declare variadic promoted property");` |
|       3 |  7839 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7840 | `					return SXERR_ABORT;` |
|       - |  7841 | `				}` |
|       3 |  7842 | `				goto Synchronize;` |
|       - |  7843 | `			}` |
|       - |  7844 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - |  7845 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - |  7846 | `			 * appear as an alternative of a union type. */` |
|      67 |  7847 | `			if( bArgTyped ){` |
|      92 |  7848 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|      58 |  7849 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      58 |  7850 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      29 |  7851 | `					"Property %z::$%z cannot have type %z",nLine);` |
|      63 |  7852 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7853 | `					return SXERR_ABORT;` |
|      63 |  7854 | `				}else if( rc != SXRET_OK ){` |
|       6 |  7855 | `					goto Synchronize;` |
|       - |  7856 | `				}` |
|      27 |  7857 | `			}` |
|       - |  7858 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|      63 |  7859 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 |  7860 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7861 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 |  7862 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7863 | `					return SXERR_ABORT;` |
|       - |  7864 | `				}` |
|       3 |  7865 | `				goto Synchronize;` |
|       - |  7866 | `			}` |
|      61 |  7867 | `			if( bArgTyped ){` |
|      57 |  7868 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      26 |  7869 | `			}` |
|      61 |  7870 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 |  7871 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 |  7872 | `			}` |
|      61 |  7873 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       8 |  7874 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       3 |  7875 | `			}` |
|      61 |  7876 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|       - |  7877 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|       - |  7878 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|      24 |  7879 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       4 |  7880 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7881 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|       3 |  7882 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7883 | `						return SXERR_ABORT;` |
|       - |  7884 | `					}` |
|       3 |  7885 | `					goto Synchronize;` |
|       - |  7886 | `				}` |
|      22 |  7887 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  7888 | `			}` |
|      59 |  7889 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      59 |  7890 | `			if( pAttr == 0 ){` |
|     ! 0 |  7891 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7892 | `				return SXERR_ABORT;` |
|       - |  7893 | `			}` |
|      59 |  7894 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      57 |  7895 | `				pAttr->nType = pArg->nType;` |
|      57 |  7896 | `				pAttr->sClass = pArg->sClass;` |
|      57 |  7897 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      57 |  7898 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7899 | `					sxu32 k;` |
|      20 |  7900 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|      14 |  7901 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|      14 |  7902 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|       8 |  7903 | `					}` |
|       3 |  7904 | `				}` |
|      26 |  7905 | `			}` |
|      59 |  7906 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      59 |  7907 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7908 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7909 | `				return SXERR_ABORT;` |
|       - |  7910 | `			}` |
|      32 |  7911 | `		}` |
|       - |  7912 | `	}` |
|  267829 |  7913 | `	if( doBody ){` |
|       - |  7914 | `		/* Compile method body */` |
|  175319 |  7915 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  175319 |  7916 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7917 | `			return SXERR_ABORT;` |
|       - |  7918 | `		}` |
|   87662 |  7919 | `	}else{` |
|       - |  7920 | `		/* Only method signature is allowed */` |
|   92515 |  7921 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 |  7922 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7923 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 |  7924 | `				if( rc == SXERR_ABORT ){` |
|       - |  7925 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7926 | `					return SXERR_ABORT;` |
|       - |  7927 | `				}` |
|     ! 0 |  7928 | `				return SXERR_CORRUPT;` |
|       - |  7929 | `			}` |
|       - |  7930 | `	}` |
|       - |  7931 | `	/* All done,install the method */` |
|  267829 |  7932 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  267829 |  7933 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7934 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7935 | `		return SXERR_ABORT;` |
|       - |  7936 | `	}` |
|  267829 |  7937 | `	return SXRET_OK;` |
|       6 |  7938 | `Synchronize:` |
|       - |  7939 | `	/* Synchronize with the first semi-colon */` |
|      40 |  7940 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      28 |  7941 | `		pGen->pIn++;` |
|       4 |  7942 | `	}` |
|      16 |  7943 | `	return SXERR_CORRUPT;` |
|  133923 |  7944 |  |
|       - |  7945 | `/*` |
|       - |  7946 | ` * Compile an object interface.` |
|       - |  7947 | ` *  According to the PHP language reference manual` |
|       - |  7948 | ` *   Object Interfaces:` |
|       - |  7949 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - |  7950 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - |  7951 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  7952 | ` *   class, but without any of the methods having their contents defined.` |
|       - |  7953 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  7954 | ` */` |
|   39196 |  7955 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       5 |  7956 |  |
|   39201 |  7957 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7958 | `	ph7_class *pClass,*pBase;` |
|       - |  7959 | `	SyToken *pEnd,*pTmp;` |
|       - |  7960 | `	SyString *pName;` |
|       - |  7961 | `	sxi32 nKwrd;` |
|       - |  7962 | `	sxi32 rc;` |
|       - |  7963 | `	/* Jump the 'interface' keyword */` |
|   39201 |  7964 | `	pGen->pIn++;` |
|       - |  7965 | `	/* Extract interface name */` |
|   39201 |  7966 | `	pName = &pGen->pIn->sData;` |
|       - |  7967 | `	/* Advance the stream cursor */` |
|   39201 |  7968 | `	pGen->pIn++;` |
|       - |  7969 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  7970 | `		SyBlob sFQN;` |
|       - |  7971 | `		SyString sFQNStr;` |
|   39201 |  7972 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   39201 |  7973 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   39201 |  7974 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   39201 |  7975 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   39201 |  7976 | `		SyBlobRelease(&sFQN);` |
|       - |  7977 | `	}` |
|   39201 |  7978 | `	if( pClass == 0 ){` |
|     ! 0 |  7979 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7980 | `		return SXERR_ABORT;` |
|       - |  7981 | `	}` |
|       - |  7982 | `	/* Mark as an interface */` |
|   39201 |  7983 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - |  7984 | `	/* Assume no base class is given */` |
|   39201 |  7985 | `	pBase = 0;` |
|   39201 |  7986 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   10681 |  7987 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   10681 |  7988 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - |  7989 | `			SyBlob sResolved;` |
|       - |  7990 | `			SyString sBaseName;` |
|       - |  7991 | `			sxu32 nRefLine;` |
|       - |  7992 | `			/* Extract base interface */` |
|   10681 |  7993 | `			pGen->pIn++;` |
|   10681 |  7994 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   10681 |  7995 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   10681 |  7996 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  7997 | `				SyBlobRelease(&sResolved);` |
|     ! 0 |  7998 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7999 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 |  8000 | `					pName);` |
|     ! 0 |  8001 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8002 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8003 | `					return SXERR_ABORT;` |
|       - |  8004 | `				}` |
|     ! 0 |  8005 | `				return SXRET_OK;` |
|       - |  8006 | `			}` |
|   16019 |  8007 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   10676 |  8008 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   10681 |  8009 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8010 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8011 | `			/* Only interfaces is allowed */` |
|   10681 |  8012 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8013 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8014 | `			}` |
|   10681 |  8015 | `			if( pBase == 0 ){` |
|     ! 0 |  8016 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8017 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|     ! 0 |  8018 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8019 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8020 | `					return SXERR_ABORT;` |
|       - |  8021 | `				}` |
|     ! 0 |  8022 | `			}` |
|   10681 |  8023 | `			SyBlobRelease(&sResolved);` |
|    5338 |  8024 | `		}` |
|    5338 |  8025 | `	}` |
|   39201 |  8026 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8027 | `		/* Syntax error */` |
|     ! 0 |  8028 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 |  8029 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8030 | `		if( rc == SXERR_ABORT ){` |
|       - |  8031 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8032 | `			return SXERR_ABORT;` |
|       - |  8033 | `		}` |
|     ! 0 |  8034 | `		return SXRET_OK;` |
|       - |  8035 | `	}` |
|   39201 |  8036 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   39201 |  8037 | `	pEnd = 0; /* cc warning */` |
|       - |  8038 | `	/* Delimit the interface body */` |
|   39201 |  8039 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   39201 |  8040 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8041 | `		/* Syntax error */` |
|     ! 0 |  8042 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 |  8043 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8044 | `		if( rc == SXERR_ABORT ){` |
|       - |  8045 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8046 | `			return SXERR_ABORT;` |
|       - |  8047 | `		}` |
|     ! 0 |  8048 | `		return SXRET_OK;` |
|       - |  8049 | `	}` |
|       - |  8050 | `	/* Swap token stream */` |
|   39201 |  8051 | `	pTmp = pGen->pEnd;` |
|   39201 |  8052 | `	pGen->pEnd = pEnd;` |
|       - |  8053 | `	/* Start the parse process` |
|       - |  8054 | `	 * Note (According to the PHP reference manual):` |
|       - |  8055 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - |  8056 | `	 *  Only 'public' visibility is allowed.` |
|       - |  8057 | `	 */` |
|   65849 |  8058 | `	for(;;){` |
|       - |  8059 | `		/* Jump leading/trailing semi-colons */` |
|  224205 |  8060 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   92507 |  8061 | `			pGen->pIn++;` |
|       5 |  8062 | `		}` |
|  131703 |  8063 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8064 | `			/* End of interface body */` |
|   39199 |  8065 | `			break;` |
|       - |  8066 | `		}` |
|   92509 |  8067 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8068 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8069 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 |  8070 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8071 | `			if( rc == SXERR_ABORT ){` |
|       - |  8072 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8073 | `				return SXERR_ABORT;` |
|       - |  8074 | `			}` |
|     ! 0 |  8075 | `			goto done;` |
|       - |  8076 | `		}` |
|       - |  8077 | `		/* Extract the current keyword */` |
|   92509 |  8078 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   92509 |  8079 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - |  8080 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - |  8081 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 |  8082 | `			const char *zKind = "member";` |
|       3 |  8083 | `			SyString *pMemberName = 0;` |
|       3 |  8084 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 |  8085 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 |  8086 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 |  8087 | `					zKind = "constant";` |
|       3 |  8088 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 |  8089 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 |  8090 | `					}` |
|       1 |  8091 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8092 | `					zKind = "method";` |
|     ! 0 |  8093 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 |  8094 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 |  8095 | `					}` |
|     ! 0 |  8096 | `				}` |
|       1 |  8097 | `			}` |
|       3 |  8098 | `			if( pMemberName ){` |
|       4 |  8099 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 |  8100 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 |  8101 | `			}else{` |
|     ! 0 |  8102 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8103 | `					"Access type for interface %s must be public",zKind);` |
|       - |  8104 | `			}` |
|       3 |  8105 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8106 | `				return SXERR_ABORT;` |
|       - |  8107 | `			}` |
|       3 |  8108 | `			goto done;` |
|       - |  8109 | `		}` |
|   92507 |  8110 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  8111 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8112 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  8113 | `			if( rc == SXERR_ABORT ){` |
|       - |  8114 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8115 | `				return SXERR_ABORT;` |
|       - |  8116 | `			}` |
|     ! 0 |  8117 | `			goto done;` |
|       - |  8118 | `		}` |
|   92507 |  8119 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - |  8120 | `			/* Advance the stream cursor */` |
|   92497 |  8121 | `			pGen->pIn++;` |
|   92497 |  8122 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8123 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8124 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  8125 | `				if( rc == SXERR_ABORT ){` |
|       - |  8126 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8127 | `					return SXERR_ABORT;` |
|       - |  8128 | `				}` |
|     ! 0 |  8129 | `				goto done;` |
|       - |  8130 | `			}` |
|   92497 |  8131 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   92497 |  8132 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  8133 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8134 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  8135 | `				if( rc == SXERR_ABORT ){` |
|       - |  8136 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8137 | `					return SXERR_ABORT;` |
|       - |  8138 | `				}` |
|     ! 0 |  8139 | `				goto done;` |
|       - |  8140 | `			}` |
|   46246 |  8141 | `		}` |
|   92507 |  8142 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8143 | `			/* Parse constant */` |
|       7 |  8144 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       7 |  8145 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8146 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8147 | `					return SXERR_ABORT;` |
|       - |  8148 | `				}` |
|     ! 0 |  8149 | `				goto done;` |
|       - |  8150 | `			}` |
|       4 |  8151 | `		}else{` |
|   92501 |  8152 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   92501 |  8153 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8154 | `				/* Static method,record that */` |
|   10673 |  8155 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - |  8156 | `				/* Advance the stream cursor */` |
|   10673 |  8157 | `				pGen->pIn++;` |
|   10668 |  8158 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|   10673 |  8159 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8160 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8161 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  8162 | `						if( rc == SXERR_ABORT ){` |
|       - |  8163 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8164 | `							return SXERR_ABORT;` |
|       - |  8165 | `						}` |
|     ! 0 |  8166 | `						goto done;` |
|       - |  8167 | `				}` |
|    5334 |  8168 | `			}` |
|       - |  8169 | `			/* Process method signature (no body for interface methods) */` |
|   92501 |  8170 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   92501 |  8171 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8172 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8173 | `					return SXERR_ABORT;` |
|       - |  8174 | `				}` |
|     ! 0 |  8175 | `				goto done;` |
|       - |  8176 | `			}` |
|       - |  8177 | `		}` |
|       5 |  8178 | `	}` |
|       - |  8179 | `	/* Install the interface */` |
|   39199 |  8180 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   39199 |  8181 | `	if( rc == SXRET_OK && pBase ){` |
|       - |  8182 | `		/* Inherit from the base interface */` |
|   10681 |  8183 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|    5338 |  8184 | `	}` |
|   39199 |  8185 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8186 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8187 | `		return SXERR_ABORT;` |
|       - |  8188 | `	}` |
|   19597 |  8189 | `done:` |
|       - |  8190 | `	/* Point beyond the interface body */` |
|   39201 |  8191 | `	pGen->pIn  = &pEnd[1];` |
|   39201 |  8192 | `	pGen->pEnd = pTmp;` |
|   39201 |  8193 | `	return PH7_OK;` |
|   19603 |  8194 |  |
|       - |  8195 | `/*` |
|       - |  8196 | ` * Compile a user-defined class.` |
|       - |  8197 | ` * According to the PHP language reference manual` |
|       - |  8198 | ` *  class` |
|       - |  8199 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - |  8200 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - |  8201 | ` *  of the properties and methods belonging to the class.` |
|       - |  8202 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - |  8203 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - |  8204 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - |  8205 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  8206 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - |  8207 | ` *  (called "methods").` |
|       - |  8208 | ` */` |
|       - |  8209 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - |  8210 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - |  8211 | `struct TraitUseEntry {` |
|       - |  8212 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - |  8213 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - |  8214 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - |  8215 | `};` |
|       - |  8216 | `/*` |
|       - |  8217 | ` * Validate that methods implementing interface contracts have compatible` |
|       - |  8218 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - |  8219 | ` */` |
|  100808 |  8220 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8221 |  |
|       - |  8222 | `	ph7_class **apIface;` |
|       - |  8223 | `	sxu32 nIface,i;` |
|       - |  8224 | `	sxi32 rc;` |
|  100813 |  8225 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 |  8226 | `		return SXRET_OK;` |
|       - |  8227 | `	}` |
|  100813 |  8228 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|  100813 |  8229 | `	nIface = SySetUsed(&pClass->aInterface);` |
|  193519 |  8230 | `	for(i = 0; i < nIface; i++){` |
|   92711 |  8231 | `		ph7_class *pIface = apIface[i];` |
|       - |  8232 | `		SyHashEntry *pEntry;` |
|   92711 |  8233 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|  249625 |  8234 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|  156919 |  8235 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - |  8236 | `			ph7_class_method *pImplMeth;` |
|  156919 |  8237 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - |  8238 | `			/* Find the implementing method in the class */` |
|  156919 |  8239 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|  156919 |  8240 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      18 |  8241 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - |  8242 | `			}` |
|       - |  8243 | `			/* Check visibility: interface methods must be implemented as public */` |
|  156905 |  8244 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 |  8245 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8246 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 |  8247 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 |  8248 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8249 | `					return SXERR_ABORT;` |
|       - |  8250 | `				}` |
|       1 |  8251 | `			}` |
|       - |  8252 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - |  8253 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - |  8254 | `			 */` |
|       - |  8255 | `			{` |
|  156905 |  8256 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|  156905 |  8257 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|  156905 |  8258 | `				int sigError = 0;` |
|  156905 |  8259 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 |  8260 | `					sigError = 1;` |
|  156904 |  8261 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - |  8262 | `					/* Extra parameters must all have default values */` |
|       6 |  8263 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - |  8264 | `					sxu32 k;` |
|       8 |  8265 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       6 |  8266 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 |  8267 | `							sigError = 1;` |
|       3 |  8268 | `							break;` |
|       - |  8269 | `						}` |
|       2 |  8270 | `					}` |
|       2 |  8271 | `				}` |
|  156905 |  8272 | `				if( sigError ){` |
|       - |  8273 | `					SyBlob sImplSig, sIfaceSig;` |
|       - |  8274 | `					ph7_vm_func_arg *aArgs;` |
|       - |  8275 | `					sxu32 j;` |
|       6 |  8276 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       6 |  8277 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - |  8278 | `					/* Build implementing method signature */` |
|       6 |  8279 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      12 |  8280 | `					for(j = 0; j < nImplArgs; j++){` |
|       8 |  8281 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       8 |  8282 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       8 |  8283 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8284 | `					}` |
|       - |  8285 | `					/* Build interface method signature */` |
|       6 |  8286 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      12 |  8287 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       8 |  8288 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       8 |  8289 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       8 |  8290 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8291 | `					}` |
|       8 |  8292 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8293 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 |  8294 | `						&pClass->sName,pMName,` |
|       4 |  8295 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 |  8296 | `						&pIface->sName,pMName,` |
|       4 |  8297 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       6 |  8298 | `					SyBlobRelease(&sImplSig);` |
|       6 |  8299 | `					SyBlobRelease(&sIfaceSig);` |
|       6 |  8300 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8301 | `						return SXERR_ABORT;` |
|       - |  8302 | `					}` |
|       2 |  8303 | `				}` |
|       - |  8304 | `			}` |
|       5 |  8305 | `		}` |
|   46358 |  8306 | `	}` |
|  100813 |  8307 | `	return SXRET_OK;` |
|   50409 |  8308 |  |
|       - |  8309 | `/*` |
|       - |  8310 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - |  8311 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - |  8312 | ` */` |
|  100808 |  8313 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8314 |  |
|       - |  8315 | `	ph7_class_method *pMeth;` |
|       - |  8316 | `	SyHashEntry *pEntry;` |
|       - |  8317 | `	sxu32 nAbstract;` |
|       - |  8318 | `	SyBlob sMsg;` |
|       - |  8319 | `	sxi32 rc;` |
|       - |  8320 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|  100813 |  8321 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      35 |  8322 | `		return SXRET_OK;` |
|       - |  8323 | `	}` |
|       - |  8324 | `	/* Count abstract methods */` |
|  100783 |  8325 | `	nAbstract = 0;` |
|  100783 |  8326 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  945249 |  8327 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  844471 |  8328 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  844471 |  8329 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      20 |  8330 | `			nAbstract++;` |
|       8 |  8331 | `		}` |
|       5 |  8332 | `	}` |
|  100783 |  8333 | `	if( nAbstract == 0 ){` |
|  100769 |  8334 | `		return SXRET_OK;` |
|       - |  8335 | `	}` |
|       - |  8336 | `	/* Build the error message listing all abstract methods with origins */` |
|      18 |  8337 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      18 |  8338 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - |  8339 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 |  8340 | `		&pClass->sName,nAbstract,` |
|       7 |  8341 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 |  8342 | `		(nAbstract > 1 ? "s" : ""));` |
|       - |  8343 | `	/* Second pass: list methods with origins */` |
|       - |  8344 | `	{` |
|      18 |  8345 | `		sxu32 nListed = 0;` |
|      18 |  8346 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      36 |  8347 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      22 |  8348 | `			ph7_class *pOrigin = 0;` |
|       - |  8349 | `			SyString *pMName;` |
|      22 |  8350 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      22 |  8351 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 |  8352 | `				continue;` |
|       - |  8353 | `			}` |
|      20 |  8354 | `			pMName = &pMeth->sFunc.sName;` |
|      20 |  8355 | `			if( nListed > 0 ){` |
|       3 |  8356 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 |  8357 | `			}` |
|       - |  8358 | `			/* Find the origin of this abstract method.` |
|       - |  8359 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - |  8360 | `			 * inheritance chains) take precedence for interface-declared` |
|       - |  8361 | `			 * methods. Abstract class methods only win when the class` |
|       - |  8362 | `			 * itself declared the abstract method (not inherited from` |
|       - |  8363 | `			 * an interface). Trait methods are adopted into the using` |
|       - |  8364 | `			 * class's namespace.` |
|       - |  8365 | `			 */` |
|       - |  8366 | `			{` |
|       - |  8367 | `				ph7_class **apIface;` |
|       - |  8368 | `				ph7_class **apTrait;` |
|       - |  8369 | `				ph7_class *pWalk;` |
|       - |  8370 | `				sxu32 i;` |
|       - |  8371 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - |  8372 | `				 * (one that was written in the class body, not inherited from an` |
|       - |  8373 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - |  8374 | `				 */` |
|      20 |  8375 | `				if( pClass->pBase ){` |
|      11 |  8376 | `					pWalk = pClass->pBase;` |
|      19 |  8377 | `					while( pWalk ){` |
|       - |  8378 | `						ph7_class_method *pParentMeth;` |
|      13 |  8379 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      13 |  8380 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - |  8381 | `							/* Exclude methods that came from an interface anywhere` |
|       - |  8382 | `							 * in this class's ancestor chain.` |
|       - |  8383 | `							 */` |
|      13 |  8384 | `							int fromIface = 0;` |
|      13 |  8385 | `							ph7_class *pAnc = pWalk;` |
|      17 |  8386 | `							while( pAnc ){` |
|       - |  8387 | `								ph7_class **apPI;` |
|       - |  8388 | `								sxu32 j;` |
|      15 |  8389 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      15 |  8390 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|      10 |  8391 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|      10 |  8392 | `										fromIface = 1;` |
|      10 |  8393 | `										break;` |
|       - |  8394 | `									}` |
|     ! 0 |  8395 | `								}` |
|      15 |  8396 | `								if( fromIface ) break;` |
|       6 |  8397 | `								pAnc = pAnc->pBase;` |
|       2 |  8398 | `							}` |
|      13 |  8399 | `							if( !fromIface ){` |
|       3 |  8400 | `								pOrigin = pWalk;` |
|       3 |  8401 | `								break;` |
|       - |  8402 | `							}` |
|       4 |  8403 | `						}` |
|      10 |  8404 | `						pWalk = pWalk->pBase;` |
|       2 |  8405 | `					}` |
|       4 |  8406 | `				}` |
|       - |  8407 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - |  8408 | `				 * each interface's own parent chain for the deepest origin.` |
|       - |  8409 | `				 */` |
|      20 |  8410 | `				if( !pOrigin ){` |
|      18 |  8411 | `					pWalk = pClass;` |
|      40 |  8412 | `					while( pWalk && !pOrigin ){` |
|      26 |  8413 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      26 |  8414 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      16 |  8415 | `							ph7_class *pIface = apIface[i];` |
|      16 |  8416 | `							ph7_class *pDeepest = 0;` |
|      28 |  8417 | `							while( pIface ){` |
|      16 |  8418 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      16 |  8419 | `									pDeepest = pIface;` |
|       6 |  8420 | `								}` |
|      16 |  8421 | `								pIface = pIface->pBase;` |
|       4 |  8422 | `							}` |
|      16 |  8423 | `							if( pDeepest ){` |
|      16 |  8424 | `								pOrigin = pDeepest;` |
|      16 |  8425 | `								break;` |
|       - |  8426 | `							}` |
|     ! 0 |  8427 | `						}` |
|      26 |  8428 | `						pWalk = pWalk->pBase;` |
|       4 |  8429 | `					}` |
|       7 |  8430 | `				}` |
|       - |  8431 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      20 |  8432 | `				if( !pOrigin ){` |
|       3 |  8433 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 |  8434 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 |  8435 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 |  8436 | `							pOrigin = pClass;` |
|       3 |  8437 | `							break;` |
|       - |  8438 | `						}` |
|     ! 0 |  8439 | `					}` |
|       1 |  8440 | `				}` |
|       - |  8441 | `			}` |
|      20 |  8442 | `			if( pOrigin ){` |
|      20 |  8443 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|      12 |  8444 | `			}else{` |
|       - |  8445 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 |  8446 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - |  8447 | `			}` |
|      20 |  8448 | `			nListed++;` |
|       4 |  8449 | `		}` |
|       - |  8450 | `	}` |
|      18 |  8451 | `	SyBlobAppend(&sMsg,")",1);` |
|      25 |  8452 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 |  8453 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      18 |  8454 | `	SyBlobRelease(&sMsg);` |
|      18 |  8455 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  8456 | `		return SXERR_ABORT;` |
|       - |  8457 | `	}` |
|      18 |  8458 | `	return SXRET_OK;` |
|   50409 |  8459 |  |
|       - |  8460 | `/*` |
|       - |  8461 | ` * Parse a class/interface name reference from the current token stream.` |
|       - |  8462 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|       - |  8463 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|       - |  8464 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|       - |  8465 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|       - |  8466 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|       - |  8467 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|       - |  8468 | ` */` |
|   96994 |  8469 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|       5 |  8470 |  |
|   96999 |  8471 | `	int isAbsolute = 0;` |
|   96999 |  8472 | `	SyToken *pStart = pGen->pIn;` |
|       - |  8473 | `	SyBlob sName;` |
|   96999 |  8474 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      99 |  8475 | `		isAbsolute = 1;` |
|      99 |  8476 | `		pGen->pIn++;` |
|      47 |  8477 | `	}` |
|   96999 |  8478 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       8 |  8479 | `		pGen->pIn = pStart;` |
|       8 |  8480 | `		return SXERR_INVALID;` |
|       - |  8481 | `	}` |
|   96993 |  8482 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   96993 |  8483 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   96993 |  8484 | `	pGen->pIn++;` |
|  145500 |  8485 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   48517 |  8486 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      13 |  8487 | `		SyBlobAppend(&sName,"\\",1);` |
|      13 |  8488 | `		pGen->pIn++;` |
|      13 |  8489 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      13 |  8490 | `		pGen->pIn++;` |
|       1 |  8491 | `	}` |
|   96993 |  8492 | `	if( isAbsolute ){` |
|      97 |  8493 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      51 |  8494 | `	}else{` |
|       - |  8495 | `		SyString sRaw;` |
|   96901 |  8496 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   96901 |  8497 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|       - |  8498 | `	}` |
|   96993 |  8499 | `	SyBlobRelease(&sName);` |
|   96993 |  8500 | `	return SXRET_OK;` |
|   48502 |  8501 |  |
|       - |  8502 | `/*` |
|       - |  8503 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|       - |  8504 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|       - |  8505 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|       - |  8506 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|       - |  8507 | ` * either direction cannot run unbounded.` |
|       - |  8508 | ` */` |
|       - |  8509 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|   10840 |  8510 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|       5 |  8511 |  |
|       - |  8512 | `	ph7_class **apParent;` |
|       - |  8513 | `	sxu32 n;` |
|   18161 |  8514 | `	while( pInterface ){` |
|   14443 |  8515 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|     ! 0 |  8516 | `			return FALSE;` |
|       - |  8517 | `		}` |
|   18013 |  8518 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    7140 |  8519 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|    7127 |  8520 | `			return TRUE;` |
|       - |  8521 | `		}` |
|    7321 |  8522 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    7321 |  8523 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|     ! 0 |  8524 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|     ! 0 |  8525 | `				return TRUE;` |
|       - |  8526 | `			}` |
|     ! 0 |  8527 | `		}` |
|    7321 |  8528 | `		pInterface = pInterface->pBase;` |
|    7321 |  8529 | `		iDepth++;` |
|       5 |  8530 | `	}` |
|    3723 |  8531 | `	return FALSE;` |
|    5425 |  8532 |  |
|   10840 |  8533 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|       5 |  8534 |  |
|   10845 |  8535 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|       5 |  8536 |  |
|       - |  8537 | `/*` |
|       - |  8538 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|       - |  8539 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|       - |  8540 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|       - |  8541 | ` */` |
|    7122 |  8542 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|       5 |  8543 |  |
|    7131 |  8544 | `	while( pBase ){` |
|      10 |  8545 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|       2 |  8546 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|       3 |  8547 | `			return TRUE;` |
|       - |  8548 | `		}` |
|      10 |  8549 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|       6 |  8550 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|       3 |  8551 | `			return TRUE;` |
|       - |  8552 | `		}` |
|       5 |  8553 | `		pBase = pBase->pBase;` |
|       1 |  8554 | `	}` |
|    7123 |  8555 | `	return FALSE;` |
|    3566 |  8556 |  |
|       - |  8557 | `/*` |
|       - |  8558 | ` * Compile a class declaration, named or anonymous.` |
|       - |  8559 | ` *` |
|       - |  8560 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|       - |  8561 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|       - |  8562 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|       - |  8563 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|       - |  8564 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|       - |  8565 | ` * implements, body, install) is shared by both paths.` |
|       - |  8566 | ` */` |
|  100838 |  8567 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|       - |  8568 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|       5 |  8569 |  |
|  100843 |  8570 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8571 | `	ph7_class *pClass,*pBase;` |
|       - |  8572 | `	SyToken *pEnd,*pTmp;` |
|       - |  8573 | `	sxi32 iProtection;` |
|       - |  8574 | `	SySet aInterfaces;` |
|       - |  8575 | `	SySet aUseEntries;` |
|       - |  8576 | `	sxi32 iAttrflags;` |
|       - |  8577 | `	SyString *pName;` |
|       - |  8578 | `	sxi32 nKwrd;` |
|       - |  8579 | `	sxi32 rc;` |
|       - |  8580 | `	/* Jump the 'class' keyword */` |
|  100843 |  8581 | `	pGen->pIn++;` |
|  100843 |  8582 | `	if( pAnonName ){` |
|       - |  8583 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|       - |  8584 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|       - |  8585 | `		 * then use the synthesized name. */` |
|      29 |  8586 | `		*ppArgStart = *ppArgEnd = 0;` |
|      29 |  8587 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       7 |  8588 | `			pGen->pIn++; /* Jump '(' */` |
|       7 |  8589 | `			*ppArgStart = pGen->pIn;` |
|      10 |  8590 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|       3 |  8591 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|       7 |  8592 | `			pGen->pIn = *ppArgEnd;` |
|       7 |  8593 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|       3 |  8594 | `		}` |
|      29 |  8595 | `		pName = pAnonName;` |
|      29 |  8596 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|      16 |  8597 | `	}else{` |
|  100817 |  8598 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  8599 | `			/* Syntax error */` |
|     ! 0 |  8600 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 |  8601 | `			if( rc == SXERR_ABORT ){` |
|       - |  8602 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8603 | `				return SXERR_ABORT;` |
|       - |  8604 | `			}` |
|       - |  8605 | `			/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 |  8606 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 |  8607 | `				pGen->pIn++;` |
|     ! 0 |  8608 | `			}` |
|     ! 0 |  8609 | `			return SXRET_OK;` |
|       - |  8610 | `		}` |
|       - |  8611 | `		/* Extract class name */` |
|  100817 |  8612 | `		pName = &pGen->pIn->sData;` |
|       - |  8613 | `		/* Advance the stream cursor */` |
|  100817 |  8614 | `		pGen->pIn++;` |
|       - |  8615 | `		/* Build FQN and obtain a raw class */ {` |
|       - |  8616 | `			SyBlob sFQN;` |
|       - |  8617 | `			SyString sFQNStr;` |
|  100817 |  8618 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|  100817 |  8619 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|  100817 |  8620 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|  100817 |  8621 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|  100817 |  8622 | `			SyBlobRelease(&sFQN);` |
|       - |  8623 | `		}` |
|       - |  8624 | `	}` |
|  100843 |  8625 | `	if( pClass == 0 ){` |
|     ! 0 |  8626 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8627 | `		return SXERR_ABORT;` |
|       - |  8628 | `	}` |
|       - |  8629 | `	/* implemented interfaces and per-use-statement trait containers */` |
|  100843 |  8630 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|  100843 |  8631 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - |  8632 | `	/* Assume a standalone class */` |
|  100843 |  8633 | `	pBase = 0;` |
|  100843 |  8634 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   85689 |  8635 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   85689 |  8636 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|       - |  8637 | `			SyBlob sResolved;` |
|       - |  8638 | `			SyString sBaseName;` |
|       - |  8639 | `			sxu32 nRefLine;` |
|   74867 |  8640 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   74867 |  8641 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   74867 |  8642 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   74867 |  8643 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       3 |  8644 | `				SyBlobRelease(&sResolved);` |
|       4 |  8645 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8646 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|       1 |  8647 | `					pName);` |
|       3 |  8648 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       3 |  8649 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8650 | `					return SXERR_ABORT;` |
|       - |  8651 | `				}` |
|       3 |  8652 | `				return SXRET_OK;` |
|       - |  8653 | `			}` |
|  112295 |  8654 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   74860 |  8655 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   74865 |  8656 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8657 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8658 | `			/* Interfaces are not allowed */` |
|   74865 |  8659 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 |  8660 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8661 | `			}` |
|   74865 |  8662 | `			if( pBase == 0 ){` |
|     ! 0 |  8663 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8664 | `					"Nonexistent base class '%z'",&sBaseName);` |
|     ! 0 |  8665 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8666 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8667 | `					return SXERR_ABORT;` |
|       - |  8668 | `				}` |
|     ! 0 |  8669 | `			}else{` |
|   74865 |  8670 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 |  8671 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  8672 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 |  8673 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8674 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8675 | `						return SXERR_ABORT;` |
|       - |  8676 | `					}` |
|     ! 0 |  8677 | `				}` |
|       - |  8678 | `			}` |
|   74865 |  8679 | `			SyBlobRelease(&sResolved);` |
|   37430 |  8680 | `		}` |
|   85687 |  8681 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - |  8682 | `			ph7_class *pInterface;` |
|       - |  8683 | `			/* Interface implementation */` |
|   10835 |  8684 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    5425 |  8685 | `			for(;;){` |
|       - |  8686 | `				SyBlob sResolved;` |
|       - |  8687 | `				SyString sIntName;` |
|       - |  8688 | `				sxu32 nRefLine;` |
|   10845 |  8689 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   10845 |  8690 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   10845 |  8691 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  8692 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8693 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8694 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 |  8695 | `						pName);` |
|     ! 0 |  8696 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8697 | `						return SXERR_ABORT;` |
|       - |  8698 | `					}` |
|     ! 0 |  8699 | `					break;` |
|       - |  8700 | `				}` |
|   21685 |  8701 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|   10840 |  8702 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   10845 |  8703 | `				SyStringInitFromBuf(&sIntName,` |
|       - |  8704 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8705 | `				/* Only interfaces are allowed */` |
|   10845 |  8706 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8707 | `					pInterface = pInterface->pNextName;` |
|     ! 0 |  8708 | `				}` |
|   10845 |  8709 | `				if( pInterface == 0 ){` |
|     ! 0 |  8710 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8711 | `						"Nonexistent base interface '%z'",&sIntName);` |
|     ! 0 |  8712 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8713 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8714 | `						return SXERR_ABORT;` |
|       - |  8715 | `					}` |
|     ! 0 |  8716 | `				}else{` |
|       - |  8717 | `					/* Reject user classes that try to implement Throwable` |
|       - |  8718 | `					 * directly (or via an interface that extends Throwable)` |
|       - |  8719 | `					 * unless they already extend Exception or Error.` |
|       - |  8720 | `					 * Exception and Error themselves are compiled from the` |
|       - |  8721 | `					 * built-in library and are exempt by FQN — a namespaced` |
|       - |  8722 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|   10845 |  8723 | `					SyString *pFqn = &pClass->sName;` |
|   10845 |  8724 | `					int bIsExceptionOrError =` |
|    8980 |  8725 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   18042 |  8726 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    9069 |  8727 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|    3570 |  8728 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   14401 |  8729 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|   10686 |  8730 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|    3559 |  8731 | `						!bIsExceptionOrError ){` |
|      12 |  8732 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8733 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|       3 |  8734 | `							&pClass->sName);` |
|       9 |  8735 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8736 | `							SyBlobRelease(&sResolved);` |
|     ! 0 |  8737 | `							return SXERR_ABORT;` |
|       - |  8738 | `						}` |
|       - |  8739 | `						/* Skip registration so the follow-up abstract-method` |
|       - |  8740 | `						 * check does not produce a duplicate fatal. */` |
|       6 |  8741 | `					}else{` |
|   10839 |  8742 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - |  8743 | `					}` |
|       - |  8744 | `				}` |
|   10845 |  8745 | `				SyBlobRelease(&sResolved);` |
|   10845 |  8746 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    5420 |  8747 | `					break;` |
|       - |  8748 | `				}` |
|      13 |  8749 | `				pGen->pIn++;/* Jump the comma */` |
|       3 |  8750 | `			}` |
|    5415 |  8751 | `		}` |
|   42841 |  8752 | `	}` |
|  100841 |  8753 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8754 | `		/* Syntax error */` |
|     ! 0 |  8755 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 |  8756 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8757 | `		if( rc == SXERR_ABORT ){` |
|       - |  8758 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8759 | `			return SXERR_ABORT;` |
|       - |  8760 | `		}` |
|     ! 0 |  8761 | `		return SXRET_OK;` |
|       - |  8762 | `	}` |
|  100841 |  8763 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|  100841 |  8764 | `	pEnd = 0; /* cc warning */` |
|       - |  8765 | `	/* Delimit the class body */` |
|  100841 |  8766 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|  100841 |  8767 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8768 | `		/* Syntax error */` |
|     ! 0 |  8769 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 |  8770 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8771 | `		if( rc == SXERR_ABORT ){` |
|       - |  8772 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8773 | `			return SXERR_ABORT;` |
|       - |  8774 | `		}` |
|     ! 0 |  8775 | `		return SXRET_OK;` |
|       - |  8776 | `	}` |
|       - |  8777 | `	/* Swap token stream */` |
|  100841 |  8778 | `	pTmp = pGen->pEnd;` |
|  100841 |  8779 | `	pGen->pEnd = pEnd;` |
|       - |  8780 | `	/* Set the inherited flags */` |
|  100841 |  8781 | `	pClass->iFlags = iFlags;` |
|       - |  8782 | `	/* Start the parse process */` |
|  138083 |  8783 | `	for(;;){` |
|       - |  8784 | `		/* Jump leading/trailing semi-colons */` |
|  427021 |  8785 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   75463 |  8786 | `			pGen->pIn++;` |
|       5 |  8787 | `		}` |
|  351563 |  8788 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8789 | `			/* End of class body */` |
|  100813 |  8790 | `			break;` |
|       - |  8791 | `		}` |
|  250750 |  8792 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|  125380 |  8793 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|     ! 0 |  8794 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8795 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8796 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8797 | `			if( rc == SXERR_ABORT ){` |
|       - |  8798 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8799 | `				return SXERR_ABORT;` |
|       - |  8800 | `			}` |
|     ! 0 |  8801 | `			goto done;` |
|       - |  8802 | `		}` |
|       - |  8803 | `		/* Assume public visibility */` |
|  250755 |  8804 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  250755 |  8805 | `		iAttrflags = 0;` |
|       - |  8806 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|       - |  8807 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|       - |  8808 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|       - |  8809 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  250755 |  8810 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  8811 | `			int bMod = 0;` |
|     ! 0 |  8812 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  8813 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       - |  8814 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|       - |  8815 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|       - |  8816 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|       - |  8817 | `			 * that the generic keyword dispatch would misread as a method. */` |
|     ! 0 |  8818 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     ! 0 |  8819 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  8820 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|     ! 0 |  8821 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|     ! 0 |  8822 | `			}` |
|     ! 0 |  8823 | `			if( !bMod ){` |
|     ! 0 |  8824 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8825 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  8826 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8827 | `						return SXERR_ABORT;` |
|       - |  8828 | `					}` |
|     ! 0 |  8829 | `					goto done;` |
|       - |  8830 | `				}` |
|     ! 0 |  8831 | `				continue;` |
|       - |  8832 | `			}` |
|     ! 0 |  8833 | `		}` |
|  250755 |  8834 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8835 | `			/* Extract the current keyword */` |
|  250755 |  8836 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  250755 |  8837 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  8838 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - |  8839 | `				TraitUseEntry sUse;` |
|      57 |  8840 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      57 |  8841 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      57 |  8842 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      34 |  8843 | `				for(;;){` |
|       - |  8844 | `					ph7_class *pTrait;` |
|       - |  8845 | `					SyString *pTraitName;` |
|      65 |  8846 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8847 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8848 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 |  8849 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8850 | `							return SXERR_ABORT;` |
|       - |  8851 | `						}` |
|     ! 0 |  8852 | `						break;` |
|       - |  8853 | `					}` |
|      65 |  8854 | `					pTraitName = &pGen->pIn->sData;` |
|       - |  8855 | `					/* Resolve trait name through namespace/imports */ {` |
|       - |  8856 | `						SyBlob sResolved;` |
|      65 |  8857 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      65 |  8858 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     125 |  8859 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      60 |  8860 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      65 |  8861 | `						SyBlobRelease(&sResolved);` |
|       - |  8862 | `					}` |
|       - |  8863 | `					/* Only traits are allowed */` |
|      65 |  8864 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  8865 | `						pTrait = pTrait->pNextName;` |
|     ! 0 |  8866 | `					}` |
|      65 |  8867 | `					if( pTrait == 0 ){` |
|     ! 0 |  8868 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8869 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 |  8870 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8871 | `							return SXERR_ABORT;` |
|       - |  8872 | `						}` |
|     ! 0 |  8873 | `					}else{` |
|      65 |  8874 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - |  8875 | `					}` |
|      65 |  8876 | `					pGen->pIn++; /* Advance past trait name */` |
|      65 |  8877 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      31 |  8878 | `						break;` |
|       - |  8879 | `					}` |
|      10 |  8880 | `					pGen->pIn++; /* Jump the comma */` |
|       2 |  8881 | `				}` |
|       - |  8882 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      57 |  8883 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - |  8884 | `					SyToken *pBlock;` |
|      13 |  8885 | `					pGen->pIn++; /* Jump '{' */` |
|      13 |  8886 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|      13 |  8887 | `					sUse.pResolvStart = pGen->pIn;` |
|      13 |  8888 | `					sUse.pResolvEnd = pBlock;` |
|      13 |  8889 | `					if( pBlock < pGen->pEnd ){` |
|      13 |  8890 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       8 |  8891 | `					}else{` |
|     ! 0 |  8892 | `						pGen->pIn = pGen->pEnd;` |
|       - |  8893 | `					}` |
|       5 |  8894 | `				}` |
|      57 |  8895 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - |  8896 | `				/* The semicolon will be consumed by the outer loop */` |
|      57 |  8897 | `				continue;` |
|       - |  8898 | `			}` |
|  250703 |  8899 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  250411 |  8900 | `				iProtection = nKwrd;` |
|  250411 |  8901 | `				pGen->pIn++; /* Jump the visibility token */` |
|       - |  8902 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`. */`` |
|  250411 |  8903 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      20 |  8904 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      20 |  8905 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       8 |  8906 | `				}` |
|  250406 |  8907 | `				if( pGen->pIn >= pGen->pEnd` |
|  250411 |  8908 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  8909 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8910 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8911 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8912 | `					if( rc == SXERR_ABORT ){` |
|       - |  8913 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  8914 | `						return SXERR_ABORT;` |
|       - |  8915 | `					}` |
|     ! 0 |  8916 | `					goto done;` |
|       - |  8917 | `				}` |
|  250411 |  8918 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8919 | `					/* Attribute declaration (untyped) */` |
|   75163 |  8920 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   75163 |  8921 | `					if( rc != SXRET_OK ){` |
|       9 |  8922 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8923 | `							return SXERR_ABORT;` |
|       - |  8924 | `						}` |
|       9 |  8925 | `						goto done;` |
|       - |  8926 | `					}` |
|   75157 |  8927 | `					continue;` |
|       - |  8928 | `				}` |
|  175253 |  8929 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8930 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     173 |  8931 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     173 |  8932 | `					if( rc != SXRET_OK ){` |
|       8 |  8933 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8934 | `							return SXERR_ABORT;` |
|       - |  8935 | `						}` |
|       8 |  8936 | `						goto done;` |
|       - |  8937 | `					}` |
|     167 |  8938 | `					continue;` |
|       - |  8939 | `				}` |
|       - |  8940 | `				/* Extract the keyword */` |
|  175085 |  8941 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   87540 |  8942 | `			}` |
|  175377 |  8943 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8944 | `				/* Process constant declaration */` |
|      67 |  8945 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      67 |  8946 | `				if( rc != SXRET_OK ){` |
|       3 |  8947 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8948 | `						return SXERR_ABORT;` |
|       - |  8949 | `					}` |
|       3 |  8950 | `					goto done;` |
|       - |  8951 | `				}` |
|      35 |  8952 | `			}else{` |
|  175315 |  8953 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8954 | `					/* Static method or attribute,record that */` |
|   10733 |  8955 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|   10733 |  8956 | `					pGen->pIn++; /* Jump the static keyword */` |
|   10733 |  8957 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8958 | `						/* Extract the keyword */` |
|   10725 |  8959 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   10725 |  8960 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  8961 | `							iProtection = nKwrd;` |
|     ! 0 |  8962 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 |  8963 | `						}` |
|    5360 |  8964 | `					}` |
|       - |  8965 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|       - |  8966 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|       - |  8967 | `					 * than a generic "expecting method" parse error. */` |
|   10733 |  8968 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  8969 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  8970 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|     ! 0 |  8971 | `					}` |
|   10728 |  8972 | `					if( pGen->pIn >= pGen->pEnd` |
|   10733 |  8973 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  8974 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8975 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 |  8976 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8977 | `						if( rc == SXERR_ABORT ){` |
|       - |  8978 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8979 | `							return SXERR_ABORT;` |
|       - |  8980 | `						}` |
|     ! 0 |  8981 | `						goto done;` |
|       - |  8982 | `					}` |
|   10733 |  8983 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8984 | `						/* Attribute declaration */` |
|       8 |  8985 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       8 |  8986 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8987 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8988 | `								return SXERR_ABORT;` |
|       - |  8989 | `							}` |
|     ! 0 |  8990 | `							goto done;` |
|       - |  8991 | `						}` |
|       8 |  8992 | `						continue;` |
|       - |  8993 | `					}` |
|   10727 |  8994 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8995 | `						/* Typed static attribute declaration */` |
|      15 |  8996 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      15 |  8997 | `						if( rc != SXRET_OK ){` |
|       3 |  8998 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8999 | `								return SXERR_ABORT;` |
|       - |  9000 | `							}` |
|       3 |  9001 | `							goto done;` |
|       - |  9002 | `						}` |
|      13 |  9003 | `						continue;` |
|       - |  9004 | `					}` |
|       - |  9005 | `					/* Extract the keyword */` |
|   10715 |  9006 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  169942 |  9007 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - |  9008 | `					/* Abstract method,record that */` |
|      12 |  9009 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - |  9010 | `					/* Mark the whole class as abstract */` |
|      12 |  9011 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - |  9012 | `					/* Advance the stream cursor */` |
|      12 |  9013 | `					pGen->pIn++;` |
|      12 |  9014 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      12 |  9015 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      12 |  9016 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      10 |  9017 | `							iProtection = nKwrd;` |
|      10 |  9018 | `							pGen->pIn++; /* Jump the visibility token */` |
|       4 |  9019 | `						}` |
|       5 |  9020 | `					}` |
|      12 |  9021 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      10 |  9022 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  9023 | `							/* Static method */` |
|     ! 0 |  9024 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  9025 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  9026 | `					}` |
|      12 |  9027 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      10 |  9028 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9029 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9030 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 |  9031 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  9032 | `							if( rc == SXERR_ABORT ){` |
|       - |  9033 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  9034 | `								return SXERR_ABORT;` |
|       - |  9035 | `							}` |
|     ! 0 |  9036 | `							goto done;` |
|       - |  9037 | `					}` |
|      12 |  9038 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  164582 |  9039 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - |  9040 | `					/* final method ,record that */` |
|      17 |  9041 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|      17 |  9042 | `					pGen->pIn++; /* Jump the final keyword */` |
|      17 |  9043 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9044 | `						/* Extract the keyword */` |
|      17 |  9045 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      17 |  9046 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       8 |  9047 | `							iProtection = nKwrd;` |
|       8 |  9048 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 |  9049 | `						}` |
|       7 |  9050 | `					}` |
|      17 |  9051 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      14 |  9052 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|       - |  9053 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|       - |  9054 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|       - |  9055 | `							 * child class is compiled (PH7_ClassInherit). */` |
|      12 |  9056 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  9057 | `							if( rc != SXRET_OK ){` |
|     ! 0 |  9058 | `								if( rc == SXERR_ABORT ){` |
|     ! 0 |  9059 | `									return SXERR_ABORT;` |
|       - |  9060 | `								}` |
|     ! 0 |  9061 | `								goto done;` |
|       - |  9062 | `							}` |
|      12 |  9063 | `							continue;` |
|       - |  9064 | `					}` |
|       5 |  9065 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 |  9066 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  9067 | `							/* Static method */` |
|     ! 0 |  9068 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  9069 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  9070 | `					}` |
|       5 |  9071 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  9072 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9073 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9074 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 |  9075 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  9076 | `							if( rc == SXERR_ABORT ){` |
|       - |  9077 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  9078 | `								return SXERR_ABORT;` |
|       - |  9079 | `							}` |
|     ! 0 |  9080 | `							goto done;` |
|       - |  9081 | `					}` |
|       5 |  9082 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  9083 | `				}` |
|  175287 |  9084 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  9085 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9086 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 |  9087 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9088 | `						if( rc == SXERR_ABORT ){` |
|       - |  9089 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9090 | `							return SXERR_ABORT;` |
|       - |  9091 | `						}` |
|     ! 0 |  9092 | `						goto done;` |
|       - |  9093 | `				}` |
|  175287 |  9094 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 |  9095 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 |  9096 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 |  9097 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9098 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  9099 | `						if( rc == SXERR_ABORT ){` |
|       - |  9100 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9101 | `							return SXERR_ABORT;` |
|       - |  9102 | `						}` |
|     ! 0 |  9103 | `						goto done;` |
|       - |  9104 | `					}` |
|       - |  9105 | `					/* Attribute declaration */` |
|       7 |  9106 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 |  9107 | `				}else{` |
|       - |  9108 | `					/* Process method declaration */` |
|  175281 |  9109 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9110 | `				}` |
|  175287 |  9111 | `				if( rc != SXRET_OK ){` |
|      16 |  9112 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9113 | `						return SXERR_ABORT;` |
|       - |  9114 | `					}` |
|      16 |  9115 | `					goto done;` |
|       - |  9116 | `				}` |
|       - |  9117 | `			}` |
|   87670 |  9118 | `		}else{` |
|       - |  9119 | `			/* Attribute declaration */` |
|     ! 0 |  9120 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9121 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9122 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9123 | `					return SXERR_ABORT;` |
|       - |  9124 | `				}` |
|     ! 0 |  9125 | `				goto done;` |
|       - |  9126 | `			}` |
|       - |  9127 | `		}` |
|       5 |  9128 | `	}` |
|       - |  9129 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - |  9130 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - |  9131 | `	 */` |
|       - |  9132 | `	{` |
|       - |  9133 | `		TraitUseEntry *apUse;` |
|       - |  9134 | `		sxu32 nU;` |
|  100813 |  9135 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|  100865 |  9136 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      57 |  9137 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      57 |  9138 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      57 |  9139 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      57 |  9140 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - |  9141 | `			sxu32 nT;` |
|      57 |  9142 | `			if( !hasResolution ){` |
|       - |  9143 | `				/* No conflict resolution block: use standard trait application */` |
|      95 |  9144 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      53 |  9145 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      53 |  9146 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9147 | `						break;` |
|       - |  9148 | `					}` |
|      29 |  9149 | `				}` |
|      26 |  9150 | `			}else{` |
|       - |  9151 | `				/* With resolution block: copy attributes, record traits,` |
|       - |  9152 | `				 * then use the block to resolve method conflicts.` |
|       - |  9153 | `				 */` |
|       - |  9154 | `				SyToken *pR;` |
|      25 |  9155 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      15 |  9156 | `					ph7_class *pTR = apTrait[nT];` |
|       - |  9157 | `					ph7_class_attr *pAR;` |
|       - |  9158 | `					SyHashEntry *pER;` |
|       - |  9159 | `					SyString *pNR;` |
|      15 |  9160 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      21 |  9161 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 |  9162 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 |  9163 | `						pNR = &pAR->sName;` |
|     ! 0 |  9164 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 |  9165 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 |  9166 | `						}` |
|     ! 0 |  9167 | `					}` |
|      15 |  9168 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       9 |  9169 | `				}` |
|       - |  9170 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|      13 |  9171 | `				pR = pUse->pResolvStart;` |
|      27 |  9172 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  9173 | `					SyString sTrait,sMethod;` |
|       - |  9174 | `					ph7_class *pSrcTrait;` |
|       - |  9175 | `					ph7_class_method *pMeth;` |
|       - |  9176 | `					sxi32 nRKwrd;` |
|      41 |  9177 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      27 |  9178 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      17 |  9179 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      17 |  9180 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      17 |  9181 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      17 |  9182 | `					sMethod = pR->sData;` |
|      17 |  9183 | `					pR++;` |
|      17 |  9184 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9185 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9186 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9187 | `							sTrait = sMethod;` |
|       7 |  9188 | `							pR++;` |
|       7 |  9189 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9190 | `							sMethod = pR->sData;` |
|       7 |  9191 | `							pR++;` |
|       3 |  9192 | `						}` |
|       3 |  9193 | `					}` |
|      17 |  9194 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9195 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  9196 | `						continue;` |
|       - |  9197 | `					}` |
|      17 |  9198 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      17 |  9199 | `					pR++;` |
|      17 |  9200 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 |  9201 | `						pSrcTrait = 0;` |
|       7 |  9202 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 |  9203 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 |  9204 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 |  9205 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 |  9206 | `								pSrcTrait = apTrait[nT];` |
|       5 |  9207 | `								break;` |
|       - |  9208 | `							}` |
|       2 |  9209 | `						}` |
|       5 |  9210 | `						if( pSrcTrait ){` |
|       5 |  9211 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 |  9212 | `							if( pMeth ){` |
|       5 |  9213 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 |  9214 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 |  9215 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 |  9216 | `								}` |
|       2 |  9217 | `							}` |
|       2 |  9218 | `						}` |
|       2 |  9219 | `					}` |
|      35 |  9220 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       3 |  9221 | `				}` |
|       - |  9222 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      25 |  9223 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - |  9224 | `					ph7_class_method *pMR;` |
|       - |  9225 | `					SyHashEntry *pER;` |
|       - |  9226 | `					SyString *pNR;` |
|      15 |  9227 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      41 |  9228 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      23 |  9229 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      23 |  9230 | `						pNR = &pMR->sFunc.sName;` |
|      23 |  9231 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      14 |  9232 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       6 |  9233 | `						}` |
|       3 |  9234 | `					}` |
|       9 |  9235 | `				}` |
|       - |  9236 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|      13 |  9237 | `				pR = pUse->pResolvStart;` |
|      27 |  9238 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  9239 | `					SyString sTrait,sMethod,sAlias;` |
|       - |  9240 | `					ph7_class *pSrcTrait;` |
|       - |  9241 | `					ph7_class_method *pMeth;` |
|      27 |  9242 | `					int hasQual = 0;` |
|       - |  9243 | `					sxi32 nRKwrd;` |
|      41 |  9244 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      27 |  9245 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      17 |  9246 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      17 |  9247 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      17 |  9248 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      17 |  9249 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      17 |  9250 | `					sMethod = pR->sData;` |
|      17 |  9251 | `					pR++;` |
|      17 |  9252 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9253 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9254 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9255 | `							sTrait = sMethod;` |
|       7 |  9256 | `							hasQual = 1;` |
|       7 |  9257 | `							pR++;` |
|       7 |  9258 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9259 | `							sMethod = pR->sData;` |
|       7 |  9260 | `							pR++;` |
|       3 |  9261 | `						}` |
|       3 |  9262 | `					}` |
|      17 |  9263 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9264 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  9265 | `						continue;` |
|       - |  9266 | `					}` |
|      17 |  9267 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      17 |  9268 | `					pR++;` |
|      17 |  9269 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|      13 |  9270 | `						sxi32 iNewVis = -1;` |
|      13 |  9271 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 |  9272 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 |  9273 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 |  9274 | `								iNewVis = nAK;` |
|       7 |  9275 | `								pR++;` |
|       3 |  9276 | `							}` |
|       3 |  9277 | `						}` |
|      13 |  9278 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|      11 |  9279 | `							sAlias = pR->sData;` |
|      11 |  9280 | `							pR++;` |
|       4 |  9281 | `						}` |
|      13 |  9282 | `						pMeth = 0;` |
|      13 |  9283 | `						if( hasQual ){` |
|       3 |  9284 | `							pSrcTrait = 0;` |
|       5 |  9285 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 |  9286 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 |  9287 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 |  9288 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 |  9289 | `									pSrcTrait = apTrait[nT];` |
|       3 |  9290 | `									break;` |
|       - |  9291 | `								}` |
|       2 |  9292 | `							}` |
|       3 |  9293 | `							if( pSrcTrait ){` |
|       3 |  9294 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 |  9295 | `							}` |
|       2 |  9296 | `						}else{` |
|      10 |  9297 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - |  9298 | `						}` |
|      13 |  9299 | `						if( pMeth ){` |
|      13 |  9300 | `							if( sAlias.nByte > 0 ){` |
|       - |  9301 | `								/* Create a shallow copy of the method struct for the alias` |
|       - |  9302 | `								 * so it can carry its own visibility without affecting the original.` |
|       - |  9303 | `								 */` |
|       - |  9304 | `								ph7_class_method *pAlias;` |
|       - |  9305 | `								char *zAliasDup;` |
|      11 |  9306 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|      11 |  9307 | `								if( pAlias ){` |
|      11 |  9308 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|      11 |  9309 | `									if( iNewVis >= 0 ){` |
|       5 |  9310 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9311 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9312 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 |  9313 | `									}` |
|      11 |  9314 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      11 |  9315 | `									if( zAliasDup ){` |
|      11 |  9316 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       4 |  9317 | `									}` |
|       7 |  9318 | `								}` |
|       7 |  9319 | `							}else if( iNewVis >= 0 ){` |
|       - |  9320 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - |  9321 | `								ph7_class_method *pCopy;` |
|       3 |  9322 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 |  9323 | `								if( pCopy ){` |
|       3 |  9324 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 |  9325 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 |  9326 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9327 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9328 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - |  9329 | `									/* Replace the method in the class hash */` |
|       3 |  9330 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 |  9331 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 |  9332 | `								}` |
|       1 |  9333 | `							}` |
|       5 |  9334 | `						}` |
|       5 |  9335 | `						SXUNUSED(hasQual);` |
|       5 |  9336 | `					}` |
|      21 |  9337 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       3 |  9338 | `				}` |
|       - |  9339 | `			}` |
|      57 |  9340 | `			SySetRelease(&pUse->aTraits);` |
|      31 |  9341 | `		}` |
|       - |  9342 | `	}` |
|       - |  9343 | `	/* Install the class */` |
|  100813 |  9344 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|  100813 |  9345 | `	if( rc == SXRET_OK ){` |
|       - |  9346 | `		ph7_class **apInterface;` |
|       - |  9347 | `		sxu32 n;` |
|  100813 |  9348 | `		if( pBase ){` |
|       - |  9349 | `			/* Inherit from base class and mark as a subclass */` |
|   74865 |  9350 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   37430 |  9351 | `		}` |
|  100813 |  9352 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|  111647 |  9353 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - |  9354 | `			/* Implements one or more interface */` |
|   10839 |  9355 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|   10839 |  9356 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9357 | `				break;` |
|       - |  9358 | `			}` |
|    5422 |  9359 | `		}` |
|       - |  9360 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|       - |  9361 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|  100808 |  9362 | `		if( rc == SXRET_OK` |
|  100808 |  9363 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|  100813 |  9364 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   81879 |  9365 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|       - |  9366 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   81879 |  9367 | `			if( pStringable ){` |
|   81879 |  9368 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   81879 |  9369 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|       - |  9370 | `				sxu32 i;` |
|   81879 |  9371 | `				int bAlready = 0;` |
|   88995 |  9372 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    7123 |  9373 | `					if( apImpl[i] == pStringable ){` |
|       3 |  9374 | `						bAlready = 1;` |
|       3 |  9375 | `						break;` |
|       - |  9376 | `					}` |
|    3563 |  9377 | `				}` |
|   81879 |  9378 | `				if( !bAlready ){` |
|   81877 |  9379 | `					PH7_ClassImplement(pClass,pStringable);` |
|   40936 |  9380 | `				}` |
|   40937 |  9381 | `			}` |
|   40937 |  9382 | `		}` |
|       - |  9383 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|  100813 |  9384 | `		if( rc == SXRET_OK ){` |
|  100813 |  9385 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|  100813 |  9386 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  9387 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  9388 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  9389 | `				return SXERR_ABORT;` |
|       - |  9390 | `			}` |
|   50404 |  9391 | `		}` |
|       - |  9392 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|  100813 |  9393 | `		if( rc == SXRET_OK ){` |
|  100813 |  9394 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|  100813 |  9395 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  9396 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  9397 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  9398 | `				return SXERR_ABORT;` |
|       - |  9399 | `			}` |
|   50404 |  9400 | `		}` |
|   50404 |  9401 | `	}` |
|  100813 |  9402 | `	SySetRelease(&aUseEntries);` |
|  100813 |  9403 | `	SySetRelease(&aInterfaces);` |
|  100813 |  9404 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9405 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9406 | `		return SXERR_ABORT;` |
|       - |  9407 | `	}` |
|   50404 |  9408 | `done:` |
|       - |  9409 | `	/* Point beyond the class body */` |
|  100841 |  9410 | `	pGen->pIn = &pEnd[1];` |
|  100841 |  9411 | `	pGen->pEnd = pTmp;` |
|  100841 |  9412 | `	return PH7_OK;` |
|   50424 |  9413 |  |
|       - |  9414 | `/* Compile a named class declaration (the common case). */` |
|  100812 |  9415 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       5 |  9416 |  |
|  100817 |  9417 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|       5 |  9418 |  |
|       - |  9419 | `/*` |
|       - |  9420 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|       - |  9421 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|       - |  9422 | ` * compile + install the class body once (at compile time, like every other` |
|       - |  9423 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|       - |  9424 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|       - |  9425 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|       - |  9426 | ` */` |
|      26 |  9427 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       3 |  9428 |  |
|       - |  9429 | `	char zName[128];         /* Synthesized class name */` |
|       - |  9430 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|       - |  9431 | `	SyString sName;` |
|       - |  9432 | `	SyToken *pArgStart,*pArgEnd;` |
|       - |  9433 | `	ph7_value *pObj;` |
|      29 |  9434 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9435 | `	sxu32 nIdx,nLen;` |
|       - |  9436 | `	sxi32 nArg,rc;` |
|      13 |  9437 | `	SXUNUSED(iCompileFlag);` |
|       - |  9438 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|      29 |  9439 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      29 |  9440 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  9441 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|     ! 0 |  9442 | `	}` |
|      29 |  9443 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - |  9444 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|       - |  9445 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|       - |  9446 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|      29 |  9447 | `	pArgStart = pArgEnd = 0;` |
|      29 |  9448 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|      29 |  9449 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9450 | `		return rc;` |
|       - |  9451 | `	}` |
|       - |  9452 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|       - |  9453 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|      29 |  9454 | `	nArg = 0;` |
|      29 |  9455 | `	if( pArgStart < pArgEnd ){` |
|       7 |  9456 | `		SyToken *pSavedIn = pGen->pIn;` |
|       7 |  9457 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  9458 | `		SyToken *pArgNext;` |
|       7 |  9459 | `		pGen->pIn = pArgStart;` |
|       7 |  9460 | `		pGen->pEnd = pArgEnd;` |
|      13 |  9461 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|       7 |  9462 | `			if( pGen->pIn < pArgNext ){` |
|       7 |  9463 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|       7 |  9464 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9465 | `					pGen->pIn = pSavedIn;` |
|     ! 0 |  9466 | `					pGen->pEnd = pSavedEnd;` |
|     ! 0 |  9467 | `					return SXERR_ABORT;` |
|       - |  9468 | `				}` |
|       7 |  9469 | `				nArg++;` |
|       3 |  9470 | `			}` |
|       7 |  9471 | `			pGen->pIn = &pArgNext[1];` |
|       1 |  9472 | `		}` |
|       7 |  9473 | `		pGen->pIn = pSavedIn;` |
|       7 |  9474 | `		pGen->pEnd = pSavedEnd;` |
|       3 |  9475 | `	}` |
|       - |  9476 | `	/* Load the synthesized class name */` |
|      29 |  9477 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      29 |  9478 | `	if( pObj == 0 ){` |
|     ! 0 |  9479 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9480 | `		return SXERR_ABORT;` |
|       - |  9481 | `	}` |
|      29 |  9482 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      29 |  9483 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  9484 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|      29 |  9485 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|      29 |  9486 | `	return SXRET_OK;` |
|      16 |  9487 |  |
|       - |  9488 | `/*` |
|       - |  9489 | ` * Compile a user-defined abstract class.` |
|       - |  9490 | ` *  According to the PHP language reference manual` |
|       - |  9491 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - |  9492 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - |  9493 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - |  9494 | ` *   the method's signature - they cannot define the implementation.` |
|       - |  9495 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - |  9496 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - |  9497 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - |  9498 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - |  9499 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - |  9500 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - |  9501 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - |  9502 | ` *   could differ.` |
|       - |  9503 | ` */` |
|       - |  9504 | `/*` |
|       - |  9505 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|       - |  9506 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|       - |  9507 | ` * receives the corresponding PH7_CLASS_* bit.` |
|       - |  9508 | ` */` |
|  976530 |  9509 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|       5 |  9510 |  |
|  976535 |  9511 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  653451 |  9512 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  653451 |  9513 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  646321 |  9514 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  323129 |  9515 | `	}` |
|  969347 |  9516 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  969287 |  9517 | `	return FALSE;` |
|  488270 |  9518 |  |
|       - |  9519 | `/*` |
|       - |  9520 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|       - |  9521 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|       - |  9522 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|       - |  9523 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|       - |  9524 | ` */` |
|  969282 |  9525 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|       5 |  9526 |  |
|  969287 |  9527 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  969287 |  9528 | `	sxi32 iFlags = 0,iFlag;` |
|  976535 |  9529 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|    7253 |  9530 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|       5 |  9531 | `			pDup = pIn;` |
|       2 |  9532 | `		}` |
|    7253 |  9533 | `		iFlags \|= iFlag;` |
|    7253 |  9534 | `		pIn++;` |
|       5 |  9535 | `	}` |
|  969287 |  9536 | `	*ppIn = pIn;` |
|  969287 |  9537 | `	if( ppDup ){ *ppDup = pDup; }` |
|  969287 |  9538 | `	return iFlags;` |
|       5 |  9539 |  |
|       - |  9540 | `/*` |
|       - |  9541 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|       - |  9542 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|       - |  9543 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|       - |  9544 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|       - |  9545 | `` * `readonly`) to their existing handlers.`` |
|       - |  9546 | ` */` |
|  965668 |  9547 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|       5 |  9548 |  |
|  965673 |  9549 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  486455 |  9550 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  967477 |  9551 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|       5 |  9552 |  |
|       - |  9553 | `/*` |
|       - |  9554 | ` * Compile a class declaration carrying one or more leading modifiers` |
|       - |  9555 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|       - |  9556 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|       - |  9557 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|       - |  9558 | `` * `abstract`+`final` pair, like PHP.`` |
|       - |  9559 | ` */` |
|    3614 |  9560 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|       5 |  9561 |  |
|       - |  9562 | `	SyToken *pDup;` |
|    3619 |  9563 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|       - |  9564 | `	sxi32 rc;` |
|    3619 |  9565 | `	if( pDup ){` |
|       4 |  9566 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|       2 |  9567 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|       3 |  9568 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9569 | `			return SXERR_ABORT;` |
|       - |  9570 | `		}` |
|       1 |  9571 | `	}` |
|    3614 |  9572 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|    1812 |  9573 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|       3 |  9574 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9575 | `			"Cannot use the final modifier on an abstract class");` |
|       3 |  9576 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9577 | `			return SXERR_ABORT;` |
|       - |  9578 | `		}` |
|       1 |  9579 | `	}` |
|    3619 |  9580 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|    1812 |  9581 |  |
|       - |  9582 | `/*` |
|       - |  9583 | ` * Compile a user-defined trait.` |
|       - |  9584 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - |  9585 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - |  9586 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - |  9587 | ` */` |
|      64 |  9588 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       5 |  9589 |  |
|      69 |  9590 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9591 | `	ph7_class *pClass;` |
|       - |  9592 | `	SyToken *pEnd,*pTmp;` |
|       - |  9593 | `	sxi32 iProtection;` |
|       - |  9594 | `	sxi32 iAttrflags;` |
|       - |  9595 | `	SyString *pName;` |
|       - |  9596 | `	sxi32 nKwrd;` |
|       - |  9597 | `	sxi32 rc;` |
|       - |  9598 | `	/* Jump the 'trait' keyword */` |
|      69 |  9599 | `	pGen->pIn++;` |
|      69 |  9600 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9601 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 |  9602 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9603 | `			return SXERR_ABORT;` |
|       - |  9604 | `		}` |
|     ! 0 |  9605 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 |  9606 | `			pGen->pIn++;` |
|     ! 0 |  9607 | `		}` |
|     ! 0 |  9608 | `		return SXRET_OK;` |
|       - |  9609 | `	}` |
|       - |  9610 | `	/* Extract trait name */` |
|      69 |  9611 | `	pName = &pGen->pIn->sData;` |
|      69 |  9612 | `	pGen->pIn++;` |
|       - |  9613 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  9614 | `		SyBlob sFQN;` |
|       - |  9615 | `		SyString sFQNStr;` |
|      69 |  9616 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      69 |  9617 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      69 |  9618 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      69 |  9619 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      69 |  9620 | `		SyBlobRelease(&sFQN);` |
|       - |  9621 | `	}` |
|      69 |  9622 | `	if( pClass == 0 ){` |
|     ! 0 |  9623 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9624 | `		return SXERR_ABORT;` |
|       - |  9625 | `	}` |
|       - |  9626 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      69 |  9627 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  9628 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 |  9629 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9630 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9631 | `			return SXERR_ABORT;` |
|       - |  9632 | `		}` |
|     ! 0 |  9633 | `		return SXRET_OK;` |
|       - |  9634 | `	}` |
|      69 |  9635 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      69 |  9636 | `	pEnd = 0;` |
|      69 |  9637 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      69 |  9638 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 |  9639 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 |  9640 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9641 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9642 | `			return SXERR_ABORT;` |
|       - |  9643 | `		}` |
|     ! 0 |  9644 | `		return SXRET_OK;` |
|       - |  9645 | `	}` |
|       - |  9646 | `	/* Swap token stream */` |
|      69 |  9647 | `	pTmp = pGen->pEnd;` |
|      69 |  9648 | `	pGen->pEnd = pEnd;` |
|       - |  9649 | `	/* Mark as trait */` |
|      69 |  9650 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - |  9651 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      64 |  9652 | `	for(;;){` |
|     177 |  9653 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      28 |  9654 | `			pGen->pIn++;` |
|       4 |  9655 | `		}` |
|     153 |  9656 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      69 |  9657 | `			break;` |
|       - |  9658 | `		}` |
|      89 |  9659 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  9660 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9661 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  9662 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  9663 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9664 | `				return SXERR_ABORT;` |
|       - |  9665 | `			}` |
|     ! 0 |  9666 | `			goto done;` |
|       - |  9667 | `		}` |
|      89 |  9668 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      89 |  9669 | `		iAttrflags = 0;` |
|      89 |  9670 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      89 |  9671 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      89 |  9672 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  9673 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 |  9674 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 |  9675 | `				for(;;){` |
|       - |  9676 | `					ph7_class *pUsedTrait;` |
|       - |  9677 | `					SyString *pUsedName;` |
|       5 |  9678 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9679 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9680 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 |  9681 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9682 | `							return SXERR_ABORT;` |
|       - |  9683 | `						}` |
|     ! 0 |  9684 | `						break;` |
|       - |  9685 | `					}` |
|       5 |  9686 | `					pUsedName = &pGen->pIn->sData;` |
|       - |  9687 | `					{` |
|       - |  9688 | `						SyBlob sResolved;` |
|       5 |  9689 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 |  9690 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 |  9691 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 |  9692 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 |  9693 | `						SyBlobRelease(&sResolved);` |
|       - |  9694 | `					}` |
|       5 |  9695 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  9696 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 |  9697 | `					}` |
|       5 |  9698 | `					if( pUsedTrait == 0 ){` |
|       4 |  9699 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 |  9700 | `							"'%z' is not a trait",pUsedName);` |
|       3 |  9701 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9702 | `							return SXERR_ABORT;` |
|       - |  9703 | `						}` |
|       2 |  9704 | `					}else{` |
|       3 |  9705 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - |  9706 | `					}` |
|       5 |  9707 | `					pGen->pIn++;` |
|       5 |  9708 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 |  9709 | `						break;` |
|       - |  9710 | `					}` |
|     ! 0 |  9711 | `					pGen->pIn++;` |
|     ! 0 |  9712 | `				}` |
|       5 |  9713 | `				continue;` |
|       - |  9714 | `			}` |
|      85 |  9715 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      73 |  9716 | `				iProtection = nKwrd;` |
|      73 |  9717 | `				pGen->pIn++;` |
|      68 |  9718 | `				if( pGen->pIn >= pGen->pEnd` |
|      73 |  9719 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9720 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9721 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  9722 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9723 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9724 | `						return SXERR_ABORT;` |
|       - |  9725 | `					}` |
|     ! 0 |  9726 | `					goto done;` |
|       - |  9727 | `				}` |
|      73 |  9728 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      12 |  9729 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  9730 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9731 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9732 | `							return SXERR_ABORT;` |
|       - |  9733 | `						}` |
|     ! 0 |  9734 | `						goto done;` |
|       - |  9735 | `					}` |
|      12 |  9736 | `					continue;` |
|       - |  9737 | `				}` |
|      63 |  9738 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 |  9739 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  9740 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9741 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9742 | `							return SXERR_ABORT;` |
|       - |  9743 | `						}` |
|     ! 0 |  9744 | `						goto done;` |
|       - |  9745 | `					}` |
|       5 |  9746 | `					continue;` |
|       - |  9747 | `				}` |
|      58 |  9748 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      27 |  9749 | `			}` |
|      71 |  9750 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 |  9751 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9752 | `					"Traits cannot have constants");` |
|     ! 0 |  9753 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9754 | `					return SXERR_ABORT;` |
|       - |  9755 | `				}` |
|     ! 0 |  9756 | `				goto done;` |
|     ! 0 |  9757 | `			}else{` |
|      71 |  9758 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 |  9759 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 |  9760 | `					pGen->pIn++;` |
|       5 |  9761 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 |  9762 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 |  9763 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  9764 | `							iProtection = nKwrd;` |
|     ! 0 |  9765 | `							pGen->pIn++;` |
|     ! 0 |  9766 | `						}` |
|       1 |  9767 | `					}` |
|       4 |  9768 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 |  9769 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9770 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9771 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 |  9772 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9773 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9774 | `							return SXERR_ABORT;` |
|       - |  9775 | `						}` |
|     ! 0 |  9776 | `						goto done;` |
|       - |  9777 | `					}` |
|       5 |  9778 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 |  9779 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 |  9780 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9781 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9782 | `								return SXERR_ABORT;` |
|       - |  9783 | `							}` |
|     ! 0 |  9784 | `							goto done;` |
|       - |  9785 | `						}` |
|       3 |  9786 | `						continue;` |
|       - |  9787 | `					}` |
|       3 |  9788 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 |  9789 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9790 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9791 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9792 | `								return SXERR_ABORT;` |
|       - |  9793 | `							}` |
|     ! 0 |  9794 | `							goto done;` |
|       - |  9795 | `						}` |
|     ! 0 |  9796 | `						continue;` |
|       - |  9797 | `					}` |
|       3 |  9798 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      68 |  9799 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       6 |  9800 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       6 |  9801 | `					pGen->pIn++;` |
|       6 |  9802 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       6 |  9803 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       6 |  9804 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 |  9805 | `							iProtection = nKwrd;` |
|       6 |  9806 | `							pGen->pIn++;` |
|       2 |  9807 | `						}` |
|       2 |  9808 | `					}` |
|       6 |  9809 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  9810 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9811 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9812 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 |  9813 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9814 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9815 | `							return SXERR_ABORT;` |
|       - |  9816 | `						}` |
|     ! 0 |  9817 | `						goto done;` |
|       - |  9818 | `					}` |
|       6 |  9819 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  9820 | `				}` |
|      69 |  9821 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  9822 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9823 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 |  9824 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9825 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9826 | `						return SXERR_ABORT;` |
|       - |  9827 | `					}` |
|     ! 0 |  9828 | `					goto done;` |
|       - |  9829 | `				}` |
|      69 |  9830 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 |  9831 | `					pGen->pIn++;` |
|     ! 0 |  9832 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  9833 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9834 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  9835 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9836 | `							return SXERR_ABORT;` |
|       - |  9837 | `						}` |
|     ! 0 |  9838 | `						goto done;` |
|       - |  9839 | `					}` |
|     ! 0 |  9840 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9841 | `				}else{` |
|      69 |  9842 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9843 | `				}` |
|      69 |  9844 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9845 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9846 | `						return SXERR_ABORT;` |
|       - |  9847 | `					}` |
|     ! 0 |  9848 | `					goto done;` |
|       - |  9849 | `				}` |
|       - |  9850 | `			}` |
|      37 |  9851 | `		}else{` |
|     ! 0 |  9852 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9853 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9854 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9855 | `					return SXERR_ABORT;` |
|       - |  9856 | `				}` |
|     ! 0 |  9857 | `				goto done;` |
|       - |  9858 | `			}` |
|       - |  9859 | `		}` |
|       5 |  9860 | `	}` |
|       - |  9861 | `	/* Install the trait */` |
|      69 |  9862 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      69 |  9863 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9864 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9865 | `		return SXERR_ABORT;` |
|       - |  9866 | `	}` |
|      32 |  9867 | `done:` |
|       - |  9868 | `	/* Point beyond the trait body */` |
|      69 |  9869 | `	pGen->pIn = &pEnd[1];` |
|      69 |  9870 | `	pGen->pEnd = pTmp;` |
|      69 |  9871 | `	return PH7_OK;` |
|      37 |  9872 |  |
|       - |  9873 | `/*` |
|       - |  9874 | ` * Compile a user-defined class.` |
|       - |  9875 | ` *  According to the PHP language reference manual` |
|       - |  9876 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - |  9877 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - |  9878 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - |  9879 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - |  9880 | ` *   and functions (called "methods").` |
|       - |  9881 | ` */` |
|   97198 |  9882 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       5 |  9883 |  |
|       - |  9884 | `	sxi32 rc;` |
|   97203 |  9885 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   97203 |  9886 | `	return rc;` |
|       5 |  9887 |  |
|       - |  9888 | `/*` |
|       - |  9889 | ` * Exception handling.` |
|       - |  9890 | ` *  According to the PHP language reference manual` |
|       - |  9891 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - |  9892 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - |  9893 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - |  9894 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - |  9895 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - |  9896 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - |  9897 | ` *    (or re-thrown) within a catch block.` |
|       - |  9898 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - |  9899 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - |  9900 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - |  9901 | ` *    been defined with set_exception_handler().` |
|       - |  9902 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - |  9903 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - |  9904 | ` */` |
|       - |  9905 | `/*` |
|       - |  9906 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - |  9907 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - |  9908 | ` * indicates failure.` |
|       - |  9909 | ` */` |
|   14550 |  9910 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  9911 |  |
|   14555 |  9912 | `	sxi32 rc = SXRET_OK;` |
|   14555 |  9913 | `	if( pRoot->pOp ){` |
|   14545 |  9914 | `		switch( pRoot->pOp->iOp ){` |
|    7270 |  9915 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|       - |  9916 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|       - |  9917 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|       - |  9918 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|       - |  9919 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|       - |  9920 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|   14545 |  9921 | `			break;` |
|     ! 0 |  9922 | `		default:` |
|       - |  9923 | `			/* Runtime will still reject non-Throwable values; the set above` |
|       - |  9924 | `			 * covers the common shapes and gives a friendlier compile error` |
|       - |  9925 | ``			 * for obvious mistakes like `throw 5`. */`` |
|     ! 0 |  9926 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9927 | `				"throw: Expecting an exception class instance");` |
|     ! 0 |  9928 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 |  9929 | `				rc = SXERR_INVALID;` |
|     ! 0 |  9930 | `			}` |
|     ! 0 |  9931 | `			break;` |
|       - |  9932 | `		}` |
|    7285 |  9933 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  9934 | `		/* Unexpected expression */` |
|     ! 0 |  9935 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9936 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9937 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  9938 | `			rc = SXERR_INVALID;` |
|     ! 0 |  9939 | `		}` |
|     ! 0 |  9940 | `	}` |
|   14555 |  9941 | `	return rc;` |
|       5 |  9942 |  |
|       - |  9943 | `/*` |
|       - |  9944 | ` * Compile a 'throw' statement.` |
|       - |  9945 | ` * throw: This is how you trigger an exception.` |
|       - |  9946 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - |  9947 | ` */` |
|   14514 |  9948 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       5 |  9949 |  |
|   14519 |  9950 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9951 | `	GenBlock *pBlock;` |
|       - |  9952 | `	sxu32 nIdx;` |
|       - |  9953 | `	sxi32 rc;` |
|   14519 |  9954 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - |  9955 | `	/* Compile the expression */` |
|   14519 |  9956 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   14519 |  9957 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9958 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 |  9959 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9960 | `			return SXERR_ABORT;` |
|       - |  9961 | `		}` |
|     ! 0 |  9962 | `		return SXRET_OK;` |
|       - |  9963 | `	}` |
|   14519 |  9964 | `	pBlock = pGen->pCurrent;` |
|       - |  9965 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   57481 |  9966 | `	while(pBlock->pParent){` |
|   57477 |  9967 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   14515 |  9968 | `			break;` |
|       - |  9969 | `		}` |
|       - |  9970 | `		/* Point to the parent block */` |
|   42967 |  9971 | `		pBlock = pBlock->pParent;` |
|       5 |  9972 | `	}` |
|       - |  9973 | `	/* Emit the throw instruction */` |
|   14519 |  9974 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - |  9975 | `	/* Emit the jump */` |
|   14519 |  9976 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   14519 |  9977 | `	return SXRET_OK;` |
|    7262 |  9978 |  |
|       - |  9979 | `/*` |
|       - |  9980 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - |  9981 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - |  9982 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - |  9983 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - |  9984 | ` * the validator guarantees the operand is a valid exception target.` |
|       - |  9985 | ` */` |
|      36 |  9986 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 |  9987 |  |
|      38 |  9988 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9989 | `	GenBlock *pBlock;` |
|       - |  9990 | `	sxu32 nIdx;` |
|       - |  9991 | `	sxi32 rc;` |
|      18 |  9992 | `	(void)iCompileFlag;` |
|      38 |  9993 | `	pGen->pIn++; /* Skip 'throw' */` |
|      38 |  9994 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  9995 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9996 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9997 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9998 | `			return SXERR_ABORT;` |
|       - |  9999 | `		}` |
|     ! 0 | 10000 | `		return SXRET_OK;` |
|       - | 10001 | `	}` |
|      38 | 10002 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      38 | 10003 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10004 | `		return SXERR_ABORT;` |
|       - | 10005 | `	}` |
|      38 | 10006 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 10007 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10008 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10009 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10010 | `			return SXERR_ABORT;` |
|       - | 10011 | `		}` |
|     ! 0 | 10012 | `		return SXRET_OK;` |
|       - | 10013 | `	}` |
|       - | 10014 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      38 | 10015 | `	pBlock = pGen->pCurrent;` |
|      60 | 10016 | `	while( pBlock->pParent ){` |
|      49 | 10017 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      27 | 10018 | `			break;` |
|       - | 10019 | `		}` |
|      23 | 10020 | `		pBlock = pBlock->pParent;` |
|       1 | 10021 | `	}` |
|      38 | 10022 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      38 | 10023 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      38 | 10024 | `	return SXRET_OK;` |
|      20 | 10025 |  |
|       - | 10026 | `/*` |
|       - | 10027 | ` * Compile a 'catch' block.` |
|       - | 10028 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 10029 | ` * an object containing the exception information.` |
|       - | 10030 | ` */` |
|     588 | 10031 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       5 | 10032 |  |
|     593 | 10033 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10034 | `	ph7_exception_block sCatch;` |
|       - | 10035 | `	SySet *pInstrContainer;` |
|       - | 10036 | `	SyString sClassName;` |
|       - | 10037 | `	GenBlock *pCatch;` |
|       - | 10038 | `	SyToken *pToken;` |
|       - | 10039 | `	SyString *pName;` |
|       - | 10040 | `	char *zDup;` |
|       - | 10041 | `	sxi32 rc;` |
|     593 | 10042 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 10043 | `	/* Zero the structure */` |
|     593 | 10044 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 10045 | `	/* Initialize fields */` |
|     593 | 10046 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     593 | 10047 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     593 | 10048 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - | 10049 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 10050 | `			pToken = pGen->pIn;` |
|     ! 0 | 10051 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10052 | `				pToken--;` |
|     ! 0 | 10053 | `			}` |
|     ! 0 | 10054 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10055 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10056 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10057 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10058 | `				return SXERR_ABORT;` |
|       - | 10059 | `			}` |
|     ! 0 | 10060 | `			return SXERR_INVALID;` |
|       - | 10061 | `	}` |
|       - | 10062 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     593 | 10063 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     308 | 10064 | `	for(;;){` |
|       - | 10065 | `		SyBlob sResolved;` |
|     621 | 10066 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     621 | 10067 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       6 | 10068 | `			SyBlobRelease(&sResolved);` |
|       6 | 10069 | `			pToken = pGen->pIn;` |
|       6 | 10070 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10071 | `				pToken--;` |
|     ! 0 | 10072 | `			}` |
|       8 | 10073 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10074 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 | 10075 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       6 | 10076 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10077 | `				return SXERR_ABORT;` |
|       - | 10078 | `			}` |
|       6 | 10079 | `			return SXERR_INVALID;` |
|       - | 10080 | `		}` |
|       - | 10081 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|       - | 10082 | `		 * transient SyBlob allocation. */` |
|     923 | 10083 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     612 | 10084 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     617 | 10085 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     617 | 10086 | `		SyBlobRelease(&sResolved);` |
|     617 | 10087 | `		if( zDup == 0 ){` |
|     ! 0 | 10088 | `			goto Mem;` |
|       - | 10089 | `		}` |
|     617 | 10090 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     617 | 10091 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10092 | `			goto Mem;` |
|       - | 10093 | `		}` |
|       - | 10094 | `		/* Check for '\|' (multi-catch separator) */` |
|     612 | 10095 | `		if( pGen->pIn < pGen->pEnd &&` |
|     612 | 10096 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      33 | 10097 | `			pGen->pIn->sData.nByte == 1 &&` |
|      28 | 10098 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      30 | 10099 | `			pGen->pIn++; /* Consume the '\|' */` |
|      30 | 10100 | `			continue;` |
|       - | 10101 | `		}` |
|     589 | 10102 | `		break;` |
|     ! 0 | 10103 | `	}` |
|     584 | 10104 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     589 | 10105 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 10106 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 10107 | `			pToken = pGen->pIn;` |
|     ! 0 | 10108 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10109 | `				pToken--;` |
|     ! 0 | 10110 | `			}` |
|     ! 0 | 10111 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10112 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10113 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10114 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10115 | `				return SXERR_ABORT;` |
|       - | 10116 | `			}` |
|     ! 0 | 10117 | `			return SXERR_INVALID;` |
|       - | 10118 | `	}` |
|     589 | 10119 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 10120 | `	/* Duplicate instance name */` |
|     589 | 10121 | `	pName = &pGen->pIn->sData;` |
|     589 | 10122 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     589 | 10123 | `	if( zDup == 0 ){` |
|     ! 0 | 10124 | `		goto Mem;` |
|       - | 10125 | `	}` |
|     589 | 10126 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     589 | 10127 | `	pGen->pIn++;` |
|     589 | 10128 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 10129 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 10130 | `		pToken = pGen->pIn;` |
|     ! 0 | 10131 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10132 | `			pToken--;` |
|     ! 0 | 10133 | `		}` |
|     ! 0 | 10134 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10135 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10136 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10137 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10138 | `			return SXERR_ABORT;` |
|       - | 10139 | `		}` |
|     ! 0 | 10140 | `		return SXERR_INVALID;` |
|       - | 10141 | `	}` |
|       - | 10142 | `	/* Compile the block */` |
|     589 | 10143 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 10144 | `	/* Create the catch block */` |
|     589 | 10145 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     589 | 10146 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10147 | `		return SXERR_ABORT;` |
|       - | 10148 | `	}` |
|       - | 10149 | `	/* Swap bytecode container */` |
|     589 | 10150 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     589 | 10151 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 10152 | `	/* Compile the block */` |
|     589 | 10153 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 10154 | `	/* Fix forward jumps now the destination is resolved  */` |
|     589 | 10155 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10156 | `	/* Emit the DONE instruction */` |
|     589 | 10157 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 10158 | `	/* Leave the block */` |
|     589 | 10159 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10160 | `	/* Restore the default container */` |
|     589 | 10161 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 10162 | `	/* Install the catch block */` |
|     589 | 10163 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     589 | 10164 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10165 | `		goto Mem;` |
|       - | 10166 | `	}` |
|     589 | 10167 | `	return SXRET_OK;` |
|     ! 0 | 10168 | `Mem:` |
|     ! 0 | 10169 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 10170 | `	return SXERR_ABORT;` |
|     299 | 10171 |  |
|       - | 10172 | `/*` |
|       - | 10173 | ` * Compile a 'try' block.` |
|       - | 10174 | ` * A function using an exception should be in a "try" block.` |
|       - | 10175 | ` * If the exception does not trigger, the code will continue` |
|       - | 10176 | ` * as normal. However if the exception triggers, an exception` |
|       - | 10177 | ` * is "thrown".` |
|       - | 10178 | ` */` |
|     634 | 10179 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       5 | 10180 |  |
|       - | 10181 | `	ph7_exception *pException;` |
|     639 | 10182 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10183 | `	GenBlock *pTry;` |
|       - | 10184 | `	sxu32 nJmpIdx;` |
|       - | 10185 | `	sxi32 rc;` |
|       - | 10186 | `	/* Create the exception container */` |
|     639 | 10187 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     639 | 10188 | `	if( pException == 0 ){` |
|     ! 0 | 10189 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 10190 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 10191 | `		return SXERR_ABORT;` |
|       - | 10192 | `	}` |
|       - | 10193 | `	/* Zero the structure */` |
|     639 | 10194 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 10195 | `	/* Initialize fields */` |
|     639 | 10196 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     639 | 10197 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     639 | 10198 | `	pException->iHasFinally = 0;` |
|     639 | 10199 | `	pException->iFinallyDone = 0;` |
|     639 | 10200 | `	pException->pVm = pGen->pVm;` |
|       - | 10201 | `	/* Create the try block */` |
|     639 | 10202 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     639 | 10203 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10204 | `		return SXERR_ABORT;` |
|       - | 10205 | `	}` |
|       - | 10206 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     639 | 10207 | `	pTry->pUserData = pException;` |
|       - | 10208 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     639 | 10209 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 10210 | `	/* Fix the jump later when the destination is resolved */` |
|     639 | 10211 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     639 | 10212 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 10213 | `	/* Compile the block */` |
|     639 | 10214 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     639 | 10215 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10216 | `		return SXERR_ABORT;` |
|       - | 10217 | `	}` |
|       - | 10218 | `	/* Fix forward jumps now the destination is resolved */` |
|     639 | 10219 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10220 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     639 | 10221 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 10222 | `	/* Leave the block */` |
|     639 | 10223 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10224 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     639 | 10225 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     632 | 10226 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 10227 | `		/* Compile one or more catch blocks */` |
|     584 | 10228 | `		for(;;){` |
|    1168 | 10229 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     944 | 10230 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     295 | 10231 | `					break;` |
|       - | 10232 | `			}` |
|     593 | 10233 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     593 | 10234 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10235 | `				return SXERR_ABORT;` |
|       - | 10236 | `			}` |
|       5 | 10237 | `		}` |
|     290 | 10238 | `	}` |
|       - | 10239 | `	/* Compile optional finally block */` |
|     639 | 10240 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     348 | 10241 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 10242 | `		SySet *pInstrContainer;` |
|       - | 10243 | `		GenBlock *pFinBlock;` |
|     115 | 10244 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 10245 | `		/* Create the finally block for jump fixup bookkeeping */` |
|     115 | 10246 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|     115 | 10247 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10248 | `			return SXERR_ABORT;` |
|       - | 10249 | `		}` |
|       - | 10250 | `		/* Swap bytecode container */` |
|     115 | 10251 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     115 | 10252 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 10253 | `		/* Compile the finally body */` |
|     115 | 10254 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     115 | 10255 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10256 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 10257 | `			return SXERR_ABORT;` |
|       - | 10258 | `		}` |
|       - | 10259 | `		/* Fix forward jumps now the destination is resolved */` |
|     115 | 10260 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10261 | `		/* Emit DONE to terminate the finally block */` |
|     115 | 10262 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 10263 | `		/* Leave the block */` |
|     115 | 10264 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10265 | `		/* Restore the default container */` |
|     115 | 10266 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     115 | 10267 | `		pException->iHasFinally = 1;` |
|      55 | 10268 | `	}` |
|       - | 10269 | `	/* Must have at least one catch or finally */` |
|     639 | 10270 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       9 | 10271 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10272 | `			"Cannot use try without catch or finally");` |
|       9 | 10273 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10274 | `			return SXERR_ABORT;` |
|       - | 10275 | `		}` |
|       3 | 10276 | `	}` |
|     639 | 10277 | `	return SXRET_OK;` |
|     322 | 10278 |  |
|       - | 10279 | `/*` |
|       - | 10280 | ` * Compile a switch block.` |
|       - | 10281 | ` *  (See block-comment below for more information)` |
|       - | 10282 | ` */` |
|     112 | 10283 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       5 | 10284 |  |
|     117 | 10285 | `	sxi32 rc = SXRET_OK;` |
|     117 | 10286 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 10287 | `		/* Unexpected token */` |
|     ! 0 | 10288 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 10289 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10290 | `			return SXERR_ABORT;` |
|       - | 10291 | `		}` |
|     ! 0 | 10292 | `		pGen->pIn++;` |
|     ! 0 | 10293 | `	}` |
|     117 | 10294 | `	pGen->pIn++;` |
|       - | 10295 | `	/* First instruction to execute in this block. */` |
|     117 | 10296 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 10297 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 10298 | `	 * or the '}' token */` |
|     206 | 10299 | `	for(;;){` |
|     417 | 10300 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10301 | `			/* No more input to process */` |
|     ! 0 | 10302 | `			break;` |
|       - | 10303 | `		}` |
|     417 | 10304 | `		rc = SXRET_OK;` |
|     417 | 10305 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      85 | 10306 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      31 | 10307 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 10308 | `					/* Unexpected token */` |
|     ! 0 | 10309 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 10310 | `						&pGen->pIn->sData);` |
|     ! 0 | 10311 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10312 | `						return SXERR_ABORT;` |
|       - | 10313 | `					}` |
|       - | 10314 | `					/* FALL THROUGH */` |
|     ! 0 | 10315 | `				}` |
|      31 | 10316 | `				rc = SXERR_EOF;` |
|      31 | 10317 | `				break;` |
|       - | 10318 | `			}` |
|      32 | 10319 | `		}else{` |
|       - | 10320 | `			sxi32 nKwrd;` |
|       - | 10321 | `			/* Extract the keyword */` |
|     337 | 10322 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     337 | 10323 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      47 | 10324 | `				break;` |
|       - | 10325 | `			}` |
|     253 | 10326 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 10327 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 10328 | `					/* Unexpected token */` |
|     ! 0 | 10329 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 10330 | `						&pGen->pIn->sData);` |
|     ! 0 | 10331 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10332 | `						return SXERR_ABORT;` |
|       - | 10333 | `					}` |
|       - | 10334 | `					/* FALL THROUGH */` |
|     ! 0 | 10335 | `				}` |
|       - | 10336 | `				/* Block compiled */` |
|       3 | 10337 | `				break;` |
|       - | 10338 | `			}` |
|       - | 10339 | `		}` |
|       - | 10340 | `		/* Compile block */` |
|     305 | 10341 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     305 | 10342 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10343 | `			return SXERR_ABORT;` |
|       - | 10344 | `		}` |
|       5 | 10345 | `	}` |
|     117 | 10346 | `	return rc;` |
|      61 | 10347 |  |
|       - | 10348 | `/*` |
|       - | 10349 | ` * Compile a case eXpression.` |
|       - | 10350 | ` *  (See block-comment below for more information)` |
|       - | 10351 | ` */` |
|      92 | 10352 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       5 | 10353 |  |
|       - | 10354 | `	SySet *pInstrContainer;` |
|       - | 10355 | `	SyToken *pEnd,*pTmp;` |
|      97 | 10356 | `	sxi32 iNest = 0;` |
|       - | 10357 | `	sxi32 rc;` |
|       - | 10358 | `	/* Delimit the expression */` |
|      97 | 10359 | `	pEnd = pGen->pIn;` |
|     197 | 10360 | `	while( pEnd < pGen->pEnd ){` |
|     197 | 10361 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 10362 | `			/* Increment nesting level */` |
|       3 | 10363 | `			iNest++;` |
|     196 | 10364 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 10365 | `			/* Decrement nesting level */` |
|       3 | 10366 | `			iNest--;` |
|     194 | 10367 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      97 | 10368 | `			break;` |
|       - | 10369 | `		}` |
|     105 | 10370 | `		pEnd++;` |
|       5 | 10371 | `	}` |
|      97 | 10372 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 10373 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 10374 | `		if( rc == SXERR_ABORT ){` |
|       - | 10375 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10376 | `			return SXERR_ABORT;` |
|       - | 10377 | `		}` |
|     ! 0 | 10378 | `	}` |
|       - | 10379 | `	/* Swap token stream */` |
|      97 | 10380 | `	pTmp = pGen->pEnd;` |
|      97 | 10381 | `	pGen->pEnd = pEnd;` |
|      97 | 10382 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      97 | 10383 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      97 | 10384 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 10385 | `	/* Emit the done instruction */` |
|      97 | 10386 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      97 | 10387 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 10388 | `	/* Update token stream */` |
|      97 | 10389 | `	pGen->pIn  = pEnd;` |
|      97 | 10390 | `	pGen->pEnd = pTmp;` |
|      97 | 10391 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10392 | `		return SXERR_ABORT;` |
|       - | 10393 | `	}` |
|      97 | 10394 | `	return SXRET_OK;` |
|      51 | 10395 |  |
|       - | 10396 | `/*` |
|       - | 10397 | ` * Compile the smart switch statement.` |
|       - | 10398 | ` * According to the PHP language reference manual` |
|       - | 10399 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 10400 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 10401 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 10402 | ` *  This is exactly what the switch statement is for.` |
|       - | 10403 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 10404 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 10405 | ` *  of the outer loop, use continue 2.` |
|       - | 10406 | ` *  Note that switch/case does loose comparision.` |
|       - | 10407 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 10408 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 10409 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 10410 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 10411 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 10412 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 10413 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 10414 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 10415 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 10416 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 10417 | ` *  list for the next case.` |
|       - | 10418 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 10419 | ` *  or floating-point numbers and strings.` |
|       - | 10420 | ` */` |
|      28 | 10421 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       5 | 10422 |  |
|       - | 10423 | `	GenBlock *pSwitchBlock;` |
|       - | 10424 | `	SyToken *pTmp,*pEnd;` |
|       - | 10425 | `	ph7_switch *pSwitch;` |
|       - | 10426 | `	sxu32 nToken;` |
|       - | 10427 | `	sxu32 nLine;` |
|       - | 10428 | `	sxi32 rc;` |
|      33 | 10429 | `	nLine = pGen->pIn->nLine;` |
|       - | 10430 | `	/* Jump the 'switch' keyword */` |
|      33 | 10431 | `	pGen->pIn++;` |
|      33 | 10432 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 10433 | `		/* Syntax error */` |
|     ! 0 | 10434 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 10435 | `		if( rc == SXERR_ABORT ){` |
|       - | 10436 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10437 | `			return SXERR_ABORT;` |
|       - | 10438 | `		}` |
|     ! 0 | 10439 | `		goto Synchronize;` |
|       - | 10440 | `	}` |
|       - | 10441 | `	/* Jump the left parenthesis '(' */` |
|      33 | 10442 | `	pGen->pIn++;` |
|      33 | 10443 | `	pEnd = 0; /* cc warning */` |
|       - | 10444 | `	/* Create the loop block */` |
|      47 | 10445 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 | 10446 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      33 | 10447 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10448 | `		return SXERR_ABORT;` |
|       - | 10449 | `	}` |
|       - | 10450 | `	/* Delimit the condition */` |
|      33 | 10451 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      33 | 10452 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 10453 | `		/* Empty expression */` |
|     ! 0 | 10454 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 10455 | `		if( rc == SXERR_ABORT ){` |
|       - | 10456 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10457 | `			return SXERR_ABORT;` |
|       - | 10458 | `		}` |
|     ! 0 | 10459 | `	}` |
|       - | 10460 | `	/* Swap token streams */` |
|      33 | 10461 | `	pTmp = pGen->pEnd;` |
|      33 | 10462 | `	pGen->pEnd = pEnd;` |
|       - | 10463 | `	/* Compile the expression */` |
|      33 | 10464 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      33 | 10465 | `	if( rc == SXERR_ABORT ){` |
|       - | 10466 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 10467 | `		return SXERR_ABORT;` |
|       - | 10468 | `	}` |
|       - | 10469 | `	/* Update token stream */` |
|      33 | 10470 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 10471 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 10472 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 10473 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10474 | `			return SXERR_ABORT;` |
|       - | 10475 | `		}` |
|     ! 0 | 10476 | `		pGen->pIn++;` |
|     ! 0 | 10477 | `	}` |
|      33 | 10478 | `	pGen->pIn  = &pEnd[1];` |
|      33 | 10479 | `	pGen->pEnd = pTmp;` |
|      33 | 10480 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 | 10481 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 10482 | `			pTmp = pGen->pIn;` |
|     ! 0 | 10483 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 10484 | `				pTmp--;` |
|     ! 0 | 10485 | `			}` |
|       - | 10486 | `			/* Unexpected token */` |
|     ! 0 | 10487 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 10488 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10489 | `				return SXERR_ABORT;` |
|       - | 10490 | `			}` |
|     ! 0 | 10491 | `			goto Synchronize;` |
|       - | 10492 | `	}` |
|       - | 10493 | `	/* Set the delimiter token */` |
|      33 | 10494 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 10495 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 10496 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 10497 | `	}else{` |
|      31 | 10498 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 10499 | `	}` |
|      33 | 10500 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 10501 | `	/* Create the switch blocks container */` |
|      33 | 10502 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      33 | 10503 | `	if( pSwitch == 0 ){` |
|       - | 10504 | `		/* Abort compilation */` |
|     ! 0 | 10505 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 10506 | `		return SXERR_ABORT;` |
|       - | 10507 | `	}` |
|       - | 10508 | `	/* Zero the structure */` |
|      33 | 10509 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 10510 | `	/* Initialize fields */` |
|      33 | 10511 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 10512 | `	/* Emit the switch instruction */` |
|      33 | 10513 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 10514 | `	/* Compile case blocks */` |
|     100 | 10515 | `	for(;;){` |
|       - | 10516 | `		sxu32 nKwrd;` |
|     119 | 10517 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10518 | `			/* No more input to process */` |
|     ! 0 | 10519 | `			break;` |
|       - | 10520 | `		}` |
|     119 | 10521 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 10522 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 10523 | `				/* Unexpected token */` |
|     ! 0 | 10524 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10525 | `					&pGen->pIn->sData);` |
|     ! 0 | 10526 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10527 | `					return SXERR_ABORT;` |
|       - | 10528 | `				}` |
|       - | 10529 | `				/* FALL THROUGH */` |
|     ! 0 | 10530 | `			}` |
|       - | 10531 | `			/* Block compiled */` |
|     ! 0 | 10532 | `			break;` |
|       - | 10533 | `		}` |
|       - | 10534 | `		/* Extract the keyword */` |
|     119 | 10535 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     119 | 10536 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 10537 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 10538 | `				/* Unexpected token */` |
|     ! 0 | 10539 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10540 | `					&pGen->pIn->sData);` |
|     ! 0 | 10541 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10542 | `					return SXERR_ABORT;` |
|       - | 10543 | `				}` |
|       - | 10544 | `				/* FALL THROUGH */` |
|     ! 0 | 10545 | `			}` |
|       - | 10546 | `			/* Block compiled */` |
|       3 | 10547 | `			break;` |
|       - | 10548 | `		}` |
|     117 | 10549 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 10550 | `			/*` |
|       - | 10551 | `			 * Accroding to the PHP language reference manual` |
|       - | 10552 | `			 *  A special case is the default case. This case matches anything` |
|       - | 10553 | `			 *  that wasn't matched by the other cases.` |
|       - | 10554 | `			 */` |
|      25 | 10555 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 10556 | `				/* Default case already compiled */` |
|     ! 0 | 10557 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 10558 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10559 | `					return SXERR_ABORT;` |
|       - | 10560 | `				}` |
|     ! 0 | 10561 | `			}` |
|      25 | 10562 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 10563 | `			/* Compile the default block */` |
|      25 | 10564 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      25 | 10565 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 10566 | `				return SXERR_ABORT;` |
|      25 | 10567 | `			}else if( rc == SXERR_EOF ){` |
|      23 | 10568 | `				break;` |
|       1 | 10569 | `			}` |
|      98 | 10570 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 10571 | `			ph7_case_expr sCase;` |
|       - | 10572 | `			/* Standard case block */` |
|      97 | 10573 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 10574 | `			/* initialize the structure */` |
|      97 | 10575 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 10576 | `			/* Compile the case expression */` |
|      97 | 10577 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      97 | 10578 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10579 | `				return SXERR_ABORT;` |
|       - | 10580 | `			}` |
|       - | 10581 | `			/* Compile the case block */` |
|      97 | 10582 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 10583 | `			/* Insert in the switch container */` |
|      97 | 10584 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      97 | 10585 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 10586 | `				return SXERR_ABORT;` |
|      97 | 10587 | `			}else if( rc == SXERR_EOF ){` |
|       9 | 10588 | `				break;` |
|       - | 10589 | `			}` |
|      47 | 10590 | `		}else{` |
|       - | 10591 | `			/* Unexpected token */` |
|     ! 0 | 10592 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10593 | `				&pGen->pIn->sData);` |
|     ! 0 | 10594 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10595 | `				return SXERR_ABORT;` |
|       - | 10596 | `			}` |
|     ! 0 | 10597 | `			break;` |
|       - | 10598 | `		}` |
|       5 | 10599 | `	}` |
|       - | 10600 | `	/* Fix all jumps now the destination is resolved */` |
|      33 | 10601 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      33 | 10602 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10603 | `	/* Release the loop block */` |
|      33 | 10604 | `	GenStateLeaveBlock(pGen,0);` |
|      33 | 10605 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 10606 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      33 | 10607 | `		pGen->pIn++;` |
|      14 | 10608 | `	}` |
|       - | 10609 | `	/* Statement successfully compiled */` |
|      33 | 10610 | `	return SXRET_OK;` |
|     ! 0 | 10611 | `Synchronize:` |
|       - | 10612 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 10613 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 10614 | `		pGen->pIn++;` |
|     ! 0 | 10615 | `	}` |
|     ! 0 | 10616 | `	return SXRET_OK;` |
|      19 | 10617 |  |
|       - | 10618 | `/*` |
|       - | 10619 | ` * Chain operators participate in a postfix member-access chain.` |
|       - | 10620 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|       - | 10621 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|       - | 10622 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|       - | 10623 | ` */` |
|       - | 10624 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|       - | 10625 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|       - | 10626 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|       - | 10627 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|       - | 10628 |  |
|       - | 10629 | `/*` |
|       - | 10630 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|       - | 10631 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|       - | 10632 | ` * patched entries from the pending set.` |
|       - | 10633 | ` */` |
| 2643582 | 10634 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|       5 | 10635 |  |
| 2643587 | 10636 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|       - | 10637 | `	sxu32 nTarget;` |
|       - | 10638 | `	sxu32 *aIdx;` |
|       - | 10639 | `	sxu32 i;` |
| 2643587 | 10640 | `	if( nCur <= nBaseline ){` |
| 2643493 | 10641 | `		return;` |
|       - | 10642 | `	}` |
|      97 | 10643 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      97 | 10644 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|     199 | 10645 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|     105 | 10646 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|     105 | 10647 | `		if( pInstr ){` |
|     105 | 10648 | `			pInstr->iP2 = (sxi32)nTarget;` |
|      51 | 10649 | `		}` |
|      54 | 10650 | `	}` |
|      97 | 10651 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 1321796 | 10652 |  |
|       - | 10653 |  |
|       - | 10654 | `/*` |
|       - | 10655 | ` * By-reference out-parameters of builtin functions.` |
|       - | 10656 | ` *` |
|       - | 10657 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|       - | 10658 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|       - | 10659 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|       - | 10660 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|       - | 10661 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|       - | 10662 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|       - | 10663 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|       - | 10664 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|       - | 10665 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|       - | 10666 | ` * creates it" behaviour).` |
|       - | 10667 | ` *` |
|       - | 10668 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|       - | 10669 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|       - | 10670 | ` */` |
|  444000 | 10671 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|       5 | 10672 |  |
|       - | 10673 | `	static const struct {` |
|       - | 10674 | `		const char *zName;` |
|       - | 10675 | `		sxu32 nByte;` |
|       - | 10676 | `		sxu32 mask;` |
|       - | 10677 | `	} aByRef[] = {` |
|       - | 10678 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 10679 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 10680 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 10681 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 10682 | `	};` |
|       - | 10683 | `	sxu32 i;` |
|  444005 | 10684 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|    1583 | 10685 | `		return 0;` |
|       - | 10686 | `	}` |
| 2211895 | 10687 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 1769536 | 10688 | `		if( pName->nByte == aByRef[i].nByte` |
|  907060 | 10689 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|      73 | 10690 | `			return aByRef[i].mask;` |
|       - | 10691 | `		}` |
|  884739 | 10692 | `	}` |
|  442359 | 10693 | `	return 0;` |
|  222005 | 10694 |  |
|       - | 10695 | `/*` |
|       - | 10696 | ` * Recover the bare global-builtin name from a call's callee node.` |
|       - | 10697 | ` *` |
|       - | 10698 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|       - | 10699 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|       - | 10700 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|       - | 10701 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|       - | 10702 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|       - | 10703 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|       - | 10704 | ` */` |
|  444000 | 10705 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|       5 | 10706 |  |
|       - | 10707 | `	SyToken *p, *pEnd;` |
|  444005 | 10708 | `	pOut->zString = 0;` |
|  444005 | 10709 | `	pOut->nByte = 0;` |
|  444005 | 10710 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|     ! 0 | 10711 | `		return;` |
|       - | 10712 | `	}` |
|  444005 | 10713 | `	p = pLeft->pStart;` |
|  444005 | 10714 | `	pEnd = pLeft->pEnd;` |
|       - | 10715 | `	/* Optional single leading namespace separator (absolute path). */` |
|  444005 | 10716 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|    3583 | 10717 | `		p++;` |
|    1789 | 10718 | `	}` |
|  444005 | 10719 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|    1555 | 10720 | `		return;` |
|       - | 10721 | `	}` |
|       - | 10722 | `	/* Must be a single component: nothing follows the name token. */` |
|  442455 | 10723 | `	if( p + 1 != pEnd ){` |
|      32 | 10724 | `		return;` |
|       - | 10725 | `	}` |
|  442427 | 10726 | `	*pOut = p->sData;` |
|  222005 | 10727 |  |
|       - | 10728 | `/*` |
|       - | 10729 | ` * Generate bytecode for a given expression tree.` |
|       - | 10730 | ` * If something goes wrong while generating bytecode` |
|       - | 10731 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 10732 | ` * this function takes care of generating the appropriate` |
|       - | 10733 | ` * error message.` |
|       - | 10734 | ` */` |
| 3537360 | 10735 | `static sxi32 GenStateEmitExprCode(` |
|       - | 10736 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 10737 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 10738 | `	sxi32 iFlags /* Control flags */` |
|       - | 10739 | `	)` |
|       5 | 10740 |  |
|       - | 10741 | `	VmInstr *pInstr;` |
|       - | 10742 | `	sxu32 nJmpIdx;` |
| 3537365 | 10743 | `	sxi32 iP1 = 0;` |
| 3537365 | 10744 | `	sxu32 iP2 = 0;` |
| 3537365 | 10745 | `	void *p3  = 0;` |
|       - | 10746 | `	sxi32 iVmOp;` |
|       - | 10747 | `	sxi32 rc;` |
| 3537365 | 10748 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 3537365 | 10749 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
| 3537365 | 10750 | `	sxu32 nRhsNsBase = 0;` |
| 3537365 | 10751 | `	if( pNode->xCode ){` |
|       - | 10752 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 10753 | `		/* Compile node */` |
| 2208095 | 10754 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 2208095 | 10755 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 2208095 | 10756 | `		RE_SWAP_DELIMITER(pGen);` |
| 2208095 | 10757 | `		return rc;` |
|       - | 10758 | `	}` |
| 1329275 | 10759 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 10760 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 10761 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 10762 | `		return SXERR_ABORT;` |
|       - | 10763 | `	}` |
| 1329275 | 10764 | `	iVmOp = pNode->pOp->iVmOp;` |
| 1329275 | 10765 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      65 | 10766 | `		sxu32 nJmp = 0;` |
|       - | 10767 | `		sxu32 nNcNsBase;` |
|       - | 10768 | `		VmInstr *pInstrFix;` |
|       - | 10769 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - | 10770 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - | 10771 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - | 10772 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - | 10773 | `		 * stack slot carries a writable nIdx. */` |
|      65 | 10774 | `		if( pNode->pRight ){` |
|      65 | 10775 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      65 | 10776 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|      65 | 10777 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10778 | `				return rc;` |
|       - | 10779 | `			}` |
|      65 | 10780 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       - | 10781 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - | 10782 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - | 10783 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - | 10784 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - | 10785 | `			 * the store, so the parent array does not need to be copied at` |
|       - | 10786 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - | 10787 | `			 * cascade for the actual write path stays correct. */` |
|      65 | 10788 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      65 | 10789 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      31 | 10790 | `				pInstrFix->iP2 = 3;` |
|      14 | 10791 | `			}` |
|      31 | 10792 | `		}` |
|       - | 10793 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      65 | 10794 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - | 10795 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      65 | 10796 | `		if( pNode->pLeft ){` |
|      65 | 10797 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      65 | 10798 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      65 | 10799 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10800 | `				return rc;` |
|       - | 10801 | `			}` |
|      65 | 10802 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|      31 | 10803 | `		}` |
|       - | 10804 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      65 | 10805 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - | 10806 | `		/* Patch the short-circuit jump to land after the store. */` |
|      65 | 10807 | `		if( nJmp > 0 ){` |
|      65 | 10808 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      65 | 10809 | `			if( pInstrFix ){` |
|      65 | 10810 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      31 | 10811 | `			}` |
|      31 | 10812 | `		}` |
|      65 | 10813 | `		return SXRET_OK;` |
|       - | 10814 | `	}` |
| 1329213 | 10815 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 10816 | `		sxu32 nJz,nJmp;` |
|       - | 10817 | `		sxu32 nTernaryNsBase;` |
|       - | 10818 | `		/* Ternary operator require special handling */` |
|       - | 10819 | `		/* Phase#1: Compile the condition */` |
|    2667 | 10820 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2667 | 10821 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    2667 | 10822 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10823 | `			return rc;` |
|       - | 10824 | `		}` |
|       - | 10825 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|       - | 10826 | `		 * compiling the condition must short-circuit to the end of the` |
|       - | 10827 | `		 * condition expression, not leak past the ternary. */` |
|    2667 | 10828 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    2667 | 10829 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    2667 | 10830 | `		if( pNode->pLeft ){` |
|       - | 10831 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 10832 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    2599 | 10833 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 10834 | `			/* Phase#3: Compile the 'then' expression  */` |
|    2599 | 10835 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2599 | 10836 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    2599 | 10837 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10838 | `				return rc;` |
|       - | 10839 | `			}` |
|    2599 | 10840 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1302 | 10841 | `		}else{` |
|       - | 10842 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 10843 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 10844 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 10845 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 10846 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 10847 | `		}` |
|       - | 10848 | `		/* Phase#4: Emit the unconditional jump */` |
|    2667 | 10849 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 10850 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    2667 | 10851 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    2667 | 10852 | `		if( pInstr ){` |
|    2667 | 10853 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1331 | 10854 | `		}` |
|    2667 | 10855 | `		if( !pNode->pLeft ){` |
|       - | 10856 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 10857 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 10858 | `		}` |
|       - | 10859 | `		/* Phase#6: Compile the 'else' expression */` |
|    2667 | 10860 | `		if( pNode->pRight ){` |
|    2667 | 10861 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2667 | 10862 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    2667 | 10863 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10864 | `				return rc;` |
|       - | 10865 | `			}` |
|    2667 | 10866 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1331 | 10867 | `		}` |
|    2667 | 10868 | `		if( nJmp > 0 ){` |
|       - | 10869 | `			/* Phase#7: Fix the unconditional jump */` |
|    2667 | 10870 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    2667 | 10871 | `			if( pInstr ){` |
|    2667 | 10872 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1331 | 10873 | `			}` |
|    1331 | 10874 | `		}` |
|       - | 10875 | `		/* All done */` |
|    2667 | 10876 | `		return SXRET_OK;` |
|       - | 10877 | `	}` |
| 1326551 | 10878 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|       - | 10879 | `	/* Generate code for the left tree */` |
| 1326551 | 10880 | `	if( pNode->pLeft ){` |
| 1326511 | 10881 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 1326511 | 10882 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 10883 | `			ph7_expr_node **apNode;` |
|  447703 | 10884 | `			int hasSpread = 0;` |
|  447703 | 10885 | `			int hasNamed = 0;` |
|  447703 | 10886 | `			int bAnySpread = 0;` |
|  447703 | 10887 | `			sxu32 byRefMask = 0;` |
|       - | 10888 | `			sxi32 nArgs;` |
|       - | 10889 | `			sxi32 n;` |
|       - | 10890 | `			/* Recurse and generate bytecodes for function arguments */` |
|  447703 | 10891 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  447703 | 10892 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 10893 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|       - | 10894 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|       - | 10895 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|  447703 | 10896 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|      65 | 10897 | `				bFcc = 1;` |
|      65 | 10898 | `				nArgs = 0;` |
|      32 | 10899 | `			}` |
|       - | 10900 | `			/* Validate: no positional arguments after named arguments */` |
|       - | 10901 | `			{` |
|  447703 | 10902 | `				int seenNamed = 0;` |
|  908179 | 10903 | `				for( n = 0; n < nArgs; ++n ){` |
|  460483 | 10904 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     216 | 10905 | `						seenNamed = 1;` |
|     216 | 10906 | `						hasNamed = 1;` |
|  460377 | 10907 | `					}else if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|    3585 | 10908 | `						bAnySpread = 1;` |
|  458481 | 10909 | `					}else if( seenNamed ){` |
|       3 | 10910 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|       - | 10911 | `							"Cannot use positional argument after named argument");` |
|       3 | 10912 | `						return SXERR_SYNTAX;` |
|       - | 10913 | `					}` |
|  230243 | 10914 | `				}` |
|       - | 10915 | `			}` |
|       - | 10916 | `			/* Read-only load */` |
|  447701 | 10917 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|       - | 10918 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|       - | 10919 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|       - | 10920 | `			 * objects dispatch to the right method (offsetExists for both;` |
|       - | 10921 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|  447701 | 10922 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|  447701 | 10923 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  447696 | 10924 | `				if( pCallName->nByte == 5` |
|  244424 | 10925 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|   21669 | 10926 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|  436869 | 10927 | `				}else if( pCallName->nByte == 5` |
|  222760 | 10928 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|      91 | 10929 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|      43 | 10930 | `				}` |
|       - | 10931 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|       - | 10932 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|       - | 10933 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|       - | 10934 | `				 * write back through. Skipped when spread/named args are present:` |
|       - | 10935 | `				 * the compile-time positional index no longer maps to the` |
|       - | 10936 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|  447701 | 10937 | `				if( !bAnySpread && !hasNamed ){` |
|       - | 10938 | `					SyString sBuiltin;` |
|  444005 | 10939 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|  444005 | 10940 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|  222000 | 10941 | `				}` |
|  223848 | 10942 | `			}` |
|  908175 | 10943 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  460479 | 10944 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  460479 | 10945 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|       - | 10946 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|       - | 10947 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|       - | 10948 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|       - | 10949 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|       - | 10950 | `				 * builtin to write back through. A plain $var target is unaffected` |
|       - | 10951 | `				 * (iP1=0 either way). See PLAN.md §2 for the full rationale. */` |
|  460479 | 10952 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|      53 | 10953 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|      53 | 10954 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|      24 | 10955 | `				}` |
|  460479 | 10956 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  460479 | 10957 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10958 | `					return rc;` |
|       - | 10959 | `				}` |
|       - | 10960 | `				/* Each argument is an independent nullsafe scope. */` |
|  460479 | 10961 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  460479 | 10962 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 10963 | `					/* Emit spread opcode to unpack this array argument */` |
|    3585 | 10964 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|    3585 | 10965 | `					hasSpread = 1;` |
|    1790 | 10966 | `				}` |
|  230242 | 10967 | `			}` |
|       - | 10968 | `			/* Total number of given arguments */` |
|  447701 | 10969 | `			iP1 = nArgs;` |
|  447701 | 10970 | `			iP2 = hasSpread;` |
|       - | 10971 | `			/* Build VmCallArgMap if named arguments are present.` |
|       - | 10972 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  447701 | 10973 | `			if( hasNamed ){` |
|     119 | 10974 | `				sxu32 nStrBytes = 0;` |
|       - | 10975 | `				char *zBuf;` |
|     347 | 10976 | `				for( n = 0; n < nArgs; ++n ){` |
|     231 | 10977 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     213 | 10978 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|     105 | 10979 | `					}` |
|     117 | 10980 | `				}` |
|       - | 10981 | `				{` |
|     119 | 10982 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|     119 | 10983 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|     116 | 10984 | `					&pGen->pVm->sAllocator, mapSize);` |
|     119 | 10985 | `				if( pMap ){` |
|     119 | 10986 | `					SyZero(pMap, mapSize);` |
|     119 | 10987 | `					pMap->bHasNamed = 1;` |
|     119 | 10988 | `					pMap->nTotal = (sxu32)nArgs;` |
|     119 | 10989 | `					pMap->aNames = (SyString *)&pMap[1];` |
|     119 | 10990 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     347 | 10991 | `					for( n = 0; n < nArgs; ++n ){` |
|     231 | 10992 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     213 | 10993 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|     213 | 10994 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|     213 | 10995 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|     213 | 10996 | `							zBuf += nb;` |
|     105 | 10997 | `						}` |
|       - | 10998 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|     117 | 10999 | `					}` |
|     119 | 11000 | `					p3 = (void *)pMap;` |
|      58 | 11001 | `				}` |
|       - | 11002 | `				}` |
|      58 | 11003 | `			}` |
|       - | 11004 | `			/* Remove stale flags now */` |
|  447701 | 11005 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  223848 | 11006 | `		}` |
|       - | 11007 | `		{` |
|       - | 11008 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|       - | 11009 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|       - | 11010 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|       - | 11011 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|       - | 11012 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|       - | 11013 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|       - | 11014 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|       - | 11015 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
| 1326509 | 11016 | `			sxi32 iLeftFlags = iFlags;` |
| 1326504 | 11017 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
| 1013297 | 11018 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|  350070 | 11019 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  342210 | 11020 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   15913 | 11021 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|    7954 | 11022 | `			}` |
|       - | 11023 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|       - | 11024 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|       - | 11025 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|       - | 11026 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|       - | 11027 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|       - | 11028 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|       - | 11029 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
| 1326504 | 11030 | `			if( pNode->pOp` |
| 1901283 | 11031 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
| 1238077 | 11032 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
| 1149599 | 11033 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|  177279 | 11034 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   88637 | 11035 | `			}` |
| 1326509 | 11036 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|       - | 11037 | `		}` |
| 1326509 | 11038 | `		if( rc != SXRET_OK ){` |
|      34 | 11039 | `			return rc;` |
|       - | 11040 | `		}` |
| 1326479 | 11041 | `		if( !bIsChainOp ){` |
|       - | 11042 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|       - | 11043 | `			 * target the end of that LHS chain, which is right here. */` |
|  610001 | 11044 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  304998 | 11045 | `		}` |
| 1326479 | 11046 | `		if( iVmOp == PH7_OP_CALL ){` |
|  447701 | 11047 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  447701 | 11048 | `			if( pInstr ){` |
|  447701 | 11049 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  442549 | 11050 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 11051 | `					sxu32 nQual;` |
|  442549 | 11052 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 11053 | `					/* Prevent constant expansion but preserve the absolute flag` |
|       - | 11054 | `					 * so the later NEW handler (if any) can see it. */` |
|  442549 | 11055 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|       - | 11056 | `					/* Namespace-qualify the function name for CALL, unless the` |
|       - | 11057 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|       - | 11058 | `					 * imports — class imports must NOT affect function` |
|       - | 11059 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|       - | 11060 | `					 * before NEW; we store the original literal index in the` |
|       - | 11061 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|       - | 11062 | `					 * the unqualified name and re-qualify with class imports. */` |
|  442549 | 11063 | `					if( bAbsolute ){` |
|    3583 | 11064 | `						pInstr->iP2 = (sxi32)nOrig;` |
|    1794 | 11065 | `					}else{` |
|  438971 | 11066 | `						int fromImport = 0;` |
|  438971 | 11067 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  438971 | 11068 | `						pInstr->iP2 = (sxi32)nQual;` |
|  438971 | 11069 | `						if( nQual != nOrig ){` |
|       - | 11070 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 11071 | `							 * NEW handler can recover the unqualified name. */` |
|      77 | 11072 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      77 | 11073 | `							if( !fromImport ){` |
|       - | 11074 | `								/* Mark as namespace-qualified via VmCallArgMap */` |
|      67 | 11075 | `								if( p3 == 0 ){` |
|      67 | 11076 | `									VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      62 | 11077 | `										&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      67 | 11078 | `									if( pMap ){` |
|      67 | 11079 | `										SyZero(pMap, sizeof(VmCallArgMap));` |
|      67 | 11080 | `										p3 = (void *)pMap;` |
|      31 | 11081 | `									}` |
|      31 | 11082 | `								}` |
|      67 | 11083 | `								if( p3 ){` |
|      67 | 11084 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|      31 | 11085 | `								}` |
|      31 | 11086 | `							}` |
|      36 | 11087 | `						}` |
|       5 | 11088 | `					}` |
|  226429 | 11089 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 11090 | `					/* Method call,flag that */` |
|    1201 | 11091 | `					pInstr->iP2 = 1;` |
|     598 | 11092 | `				}` |
|  223853 | 11093 | `			}` |
| 1102631 | 11094 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 11095 | `			ph7_expr_node **apNode;` |
|       - | 11096 | `			sxi32 n;` |
|   91513 | 11097 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|       - | 11098 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|       - | 11099 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|       - | 11100 | `			/* Recurse and generate bytecodes for array index */` |
|   91513 | 11101 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  165141 | 11102 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   73633 | 11103 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   73633 | 11104 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   73633 | 11105 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 11106 | `					return rc;` |
|       - | 11107 | `				}` |
|       - | 11108 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   73633 | 11109 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   36819 | 11110 | `			}` |
|   91513 | 11111 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   73633 | 11112 | `				iP1 = 1; /* Node have an index associated with it */` |
|   36814 | 11113 | `			}` |
|   91513 | 11114 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       - | 11115 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|     243 | 11116 | `				iP2 = 4;` |
|   91394 | 11117 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       - | 11118 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|       - | 11119 | `				 * so the trailing unset() builtin can drop the slot. */` |
|      56 | 11120 | `				iP2 = 5;` |
|   91249 | 11121 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       - | 11122 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|       - | 11123 | `				 * short-circuit on missing keys without invoking offsetGet` |
|       - | 11124 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|      29 | 11125 | `				iP2 = 6;` |
|   91211 | 11126 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 11127 | `				/* Create an empty entry when the desired index is not found */` |
|   36067 | 11128 | `				iP2 = 1;` |
|   18036 | 11129 | `			}` |
|  833029 | 11130 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 11131 | `			/* POP the left node */` |
|      32 | 11132 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 11133 | `		}` |
|  663237 | 11134 | `	}` |
| 1326519 | 11135 | `	rc = SXRET_OK;` |
| 1326519 | 11136 | `	nJmpIdx = 0;` |
|       - | 11137 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 11138 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 11139 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 1326519 | 11140 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     361 | 11141 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     361 | 11142 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     361 | 11143 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     361 | 11144 | `			int isSpecial = 0;` |
|     361 | 11145 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     265 | 11146 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     265 | 11147 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     260 | 11148 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     260 | 11149 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     124 | 11150 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      99 | 11151 | `					isSpecial = 1;` |
|      47 | 11152 | `				}` |
|     154 | 11153 | `			}` |
|     409 | 11154 | `			pInstr->iP1 = 0;` |
|     409 | 11155 | `			if( !isSpecial ){` |
|     219 | 11156 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     107 | 11157 | `			}` |
|       - | 11158 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 11159 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     313 | 11160 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     219 | 11161 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     219 | 11162 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      46 | 11163 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      48 | 11164 | `					return SXRET_OK;` |
|       - | 11165 | `				}` |
|      85 | 11166 | `			}` |
|     132 | 11167 | `		}` |
|     213 | 11168 | `	}` |
|       - | 11169 | `	/* Generate code for the right tree */` |
| 1326437 | 11170 | `	if( pNode->pRight ){` |
|  716081 | 11171 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 11172 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|   11167 | 11173 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  710500 | 11174 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 11175 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    3739 | 11176 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  703052 | 11177 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 11178 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     129 | 11179 | `			iVmOp = 0; /* No binary operator to emit */` |
|     129 | 11180 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  701174 | 11181 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       - | 11182 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|       - | 11183 | `			 * the entire containing postfix chain to null. The jump target is` |
|       - | 11184 | `			 * patched later by the innermost non-chain ancestor (or by` |
|       - | 11185 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|       - | 11186 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|       - | 11187 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|     105 | 11188 | `			sxu32 nNsJmp = 0;` |
|     105 | 11189 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|     105 | 11190 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  701010 | 11191 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|       - | 11192 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|       - | 11193 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|       - | 11194 | `			 * auto-created — PHP auto-vivifies on write. */` |
|  297899 | 11195 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|  148947 | 11196 | `		}` |
|  716081 | 11197 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  716081 | 11198 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  716081 | 11199 | `		if( !bIsChainOp ){` |
|       - | 11200 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|       - | 11201 | `			 * operator instruction is emitted. */` |
|  538851 | 11202 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  269423 | 11203 | `		}` |
|  716081 | 11204 | `		if( iVmOp == PH7_OP_STORE ){` |
|  294085 | 11205 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  294054 | 11206 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 11207 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 11208 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 11209 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 11210 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 11211 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 11212 | `				 */` |
|      80 | 11213 | `				iVmOp = 0;` |
|  294047 | 11214 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  294009 | 11215 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 11216 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   78739 | 11217 | `					iP2 = 1;` |
|   39372 | 11218 | `				}else{` |
|  215275 | 11219 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 11220 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   35991 | 11221 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   35991 | 11222 | `						iP1 = pInstr->iP1;` |
|   17998 | 11223 | `					}else{` |
|  179289 | 11224 | `						p3 = pInstr->p3;` |
|       - | 11225 | `					}` |
|       - | 11226 | `					/* POP the last dynamic load instruction */` |
|  215275 | 11227 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 11228 | `				}` |
|  147007 | 11229 | `			}` |
|  569041 | 11230 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      54 | 11231 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      54 | 11232 | `			if( pInstr ){` |
|      54 | 11233 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 11234 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 11235 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 11236 | `					 */` |
|      17 | 11237 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      17 | 11238 | `					iP1 = pInstr->iP1;` |
|      17 | 11239 | `					iP2 = pInstr->iP2;` |
|      17 | 11240 | `					p3  = pInstr->p3;` |
|       9 | 11241 | `				}else{` |
|      38 | 11242 | `					p3 = pInstr->p3;` |
|       - | 11243 | `				}` |
|      26 | 11244 | `			}` |
|      26 | 11245 | `		}` |
|  358038 | 11246 | `	}` |
| 1326432 | 11247 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|   11577 | 11248 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|       - | 11249 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|       - | 11250 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|      29 | 11251 | `		iVmOp = 0;` |
|      13 | 11252 | `	}` |
| 1326437 | 11253 | `	if( iVmOp > 0 ){` |
| 1326181 | 11254 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   14621 | 11255 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 11256 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|   10693 | 11257 | `				iP1 = 1;` |
|    5349 | 11258 | `			}` |
| 1318873 | 11259 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 11260 | `			/* Namespace-qualify the class name for NEW */ {` |
|   22905 | 11261 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   22905 | 11262 | `				VmInstr *pCallInstr = 0;` |
|   22905 | 11263 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   22713 | 11264 | `					pCallInstr = pPeek;` |
|   22713 | 11265 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|   11354 | 11266 | `				}` |
|   22905 | 11267 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   22903 | 11268 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 11269 | `					sxu32 nLitForClass;` |
|       - | 11270 | `					/* If the CALL handler already qualified the name using` |
|       - | 11271 | `					 * function imports, recover the original unqualified` |
|       - | 11272 | `					 * literal so we can re-qualify with class imports. */` |
|   22903 | 11273 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      37 | 11274 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      21 | 11275 | `					}else{` |
|   22871 | 11276 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 11277 | `					}` |
|   22903 | 11278 | `					pPeek->iP1 = 0;` |
|   22903 | 11279 | `					if( !bAbsolute ){` |
|   19329 | 11280 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    9667 | 11281 | `					}else{` |
|    3579 | 11282 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|       - | 11283 | `					}` |
|   11449 | 11284 | `				}` |
|       - | 11285 | `			}` |
|   22905 | 11286 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   22905 | 11287 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 11288 | `				VmInstr *pPrev;` |
|   22713 | 11289 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   22713 | 11290 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 11291 | `					/* Pop the call instruction, preserve named-arg map */` |
|   22713 | 11292 | `					iP1 = pInstr->iP1;` |
|   22713 | 11293 | `					if( pInstr->p3 ){` |
|      43 | 11294 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|      19 | 11295 | `					}` |
|   22713 | 11296 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|   11354 | 11297 | `				}` |
|   11359 | 11298 | `			}` |
| 1300115 | 11299 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 11300 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 11301 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     201 | 11302 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     201 | 11303 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     201 | 11304 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     201 | 11305 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     201 | 11306 | `				int isSpecialIs = 0;` |
|     201 | 11307 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     197 | 11308 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     197 | 11309 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     192 | 11310 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     197 | 11311 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      97 | 11312 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      12 | 11313 | `						isSpecialIs = 1;` |
|       5 | 11314 | `					}` |
|      97 | 11315 | `				}` |
|     203 | 11316 | `				pInstr->iP1 = 0;` |
|     203 | 11317 | `				if( !isSpecialIs && !bAbsolute ){` |
|     181 | 11318 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      88 | 11319 | `				}` |
|     102 | 11320 | `			}` |
| 1288570 | 11321 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 11322 | `			/* Prevent constant expansion for member/property names.` |
|       - | 11323 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 11324 | `			 * should not trigger constant lookup. */` |
|  177235 | 11325 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  177235 | 11326 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  177189 | 11327 | `				pInstr->iP1 = 0;` |
|   88592 | 11328 | `			}` |
|  177235 | 11329 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 11330 | `				/* Static member access,remember that */` |
|     279 | 11331 | `				iP1 = 1;` |
|     279 | 11332 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     279 | 11333 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      40 | 11334 | `					p3 = pInstr->p3;` |
|      40 | 11335 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      18 | 11336 | `				}` |
|     137 | 11337 | `			}` |
|       - | 11338 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|       - | 11339 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|       - | 11340 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|       - | 11341 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|  177235 | 11342 | `			if( iP2 == PH7_MEMBER_READ ){` |
|  177235 | 11343 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|      30 | 11344 | `					iP2 = PH7_MEMBER_UNSET;` |
|  177221 | 11345 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|      77 | 11346 | `					iP2 = PH7_MEMBER_ISSET;` |
|  177171 | 11347 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|      13 | 11348 | `					iP2 = PH7_MEMBER_EMPTY;` |
|  177129 | 11349 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|       - | 11350 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|   78819 | 11351 | `					iP2 = PH7_MEMBER_WRITE;` |
|   39407 | 11352 | `				}` |
|   88615 | 11353 | `			}` |
|   88615 | 11354 | `		}` |
|       - | 11355 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|       - | 11356 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|       - | 11357 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|       - | 11358 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|       - | 11359 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
| 1326179 | 11360 | `		if( bFcc ){` |
|      65 | 11361 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|      65 | 11362 | `			iP2 = 0;` |
|      65 | 11363 | `			p3 = 0;` |
|      65 | 11364 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      65 | 11365 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 11366 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|       - | 11367 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|       - | 11368 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|       - | 11369 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|      31 | 11370 | `				void *pMemberName = pInstr->p3;` |
|      31 | 11371 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|      31 | 11372 | `				if( pMemberName ){` |
|       3 | 11373 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|       1 | 11374 | `				}` |
|      31 | 11375 | `				iP1 = 2;` |
|      16 | 11376 | `			}else{` |
|      35 | 11377 | `				iP1 = 1;` |
|       - | 11378 | `			}` |
|      32 | 11379 | `		}` |
|       - | 11380 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|       - | 11381 | `		 * This is the primary emit path for user-visible calls. */` |
| 1326179 | 11382 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  470537 | 11383 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  235266 | 11384 | `		}` |
|       - | 11385 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 1326179 | 11386 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  663087 | 11387 | `	}` |
| 1326435 | 11388 | `	if( nJmpIdx > 0 ){` |
|       - | 11389 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   15025 | 11390 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   15025 | 11391 | `		if( pInstr ){` |
|   15025 | 11392 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    7510 | 11393 | `		}` |
|    7510 | 11394 | `	}` |
| 1326435 | 11395 | `	return rc;` |
| 1768665 | 11396 |  |
|       - | 11397 | `/*` |
|       - | 11398 | ` * Compile a PHP expression.` |
|       - | 11399 | ` * According to the PHP language reference manual:` |
|       - | 11400 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 11401 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 11402 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 11403 | ` *  is "anything that has a value".` |
|       - | 11404 | ` * If something goes wrong while compiling the expression,this` |
|       - | 11405 | ` * function takes care of generating the appropriate error` |
|       - | 11406 | ` * message.` |
|       - | 11407 | ` */` |
|  952788 | 11408 | `static sxi32 PH7_CompileExpr(` |
|       - | 11409 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 11410 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 11411 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 11412 | `	)` |
|       5 | 11413 |  |
|       - | 11414 | `	ph7_expr_node *pRoot;` |
|       - | 11415 | `	SySet sExprNode;` |
|       - | 11416 | `	SyToken *pEnd;` |
|       - | 11417 | `	sxi32 nExpr;` |
|       - | 11418 | `	sxi32 iNest;` |
|       - | 11419 | `	sxi32 rc;` |
|       - | 11420 | `	sxu32 nNullsafeBase;` |
|       - | 11421 | `	/* Initialize worker variables */` |
|  952793 | 11422 | `	nExpr = 0;` |
|  952793 | 11423 | `	pRoot = 0;` |
|       - | 11424 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|       - | 11425 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  952793 | 11426 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  952793 | 11427 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  952793 | 11428 | `	SySetAlloc(&sExprNode,0x10);` |
|  952793 | 11429 | `	rc = SXRET_OK;` |
|       - | 11430 | `	/* Delimit the expression */` |
|  952793 | 11431 | `	pEnd = pGen->pIn;` |
|  952793 | 11432 | `	iNest = 0;` |
| 6427315 | 11433 | `	while( pEnd < pGen->pEnd ){` |
| 6099057 | 11434 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 11435 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     515 | 11436 | `			iNest++;` |
| 6098802 | 11437 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     523 | 11438 | `			iNest--;` |
| 6098288 | 11439 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  624905 | 11440 | `			if( iNest <= 0 ){` |
|  624535 | 11441 | `				break;` |
|       - | 11442 | `			}` |
|     185 | 11443 | `		}` |
| 5474527 | 11444 | `		pEnd++;` |
|       5 | 11445 | `	}` |
|  952793 | 11446 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   21911 | 11447 | `		SyToken *pEnd2 = pGen->pIn;` |
|   21911 | 11448 | `		iNest = 0;` |
|       - | 11449 | `		/* Stop at the first comma */` |
|   44111 | 11450 | `		while( pEnd2 < pEnd ){` |
|   22211 | 11451 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      67 | 11452 | `				iNest++;` |
|   22180 | 11453 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      67 | 11454 | `				iNest--;` |
|   22118 | 11455 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      57 | 11456 | `				if( iNest <= 0 ){` |
|       7 | 11457 | `					break;` |
|       - | 11458 | `				}` |
|      23 | 11459 | `			}` |
|   22205 | 11460 | `			pEnd2++;` |
|       5 | 11461 | `		}` |
|   21911 | 11462 | `		if( pEnd2 <pEnd ){` |
|       7 | 11463 | `			pEnd = pEnd2;` |
|       3 | 11464 | `		}` |
|   10953 | 11465 | `	}` |
|  952793 | 11466 | `	if( pEnd > pGen->pIn ){` |
|  952783 | 11467 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 11468 | `		/* Swap delimiter */` |
|  952783 | 11469 | `		pGen->pEnd = pEnd;` |
|       - | 11470 | `		/* Try to get an expression tree */` |
|  952783 | 11471 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  952783 | 11472 | `		if( rc == SXRET_OK && pRoot ){` |
|  952601 | 11473 | `			rc = SXRET_OK;` |
|  952601 | 11474 | `			if( xTreeValidator ){` |
|       - | 11475 | `				/* Call the upper layer validator callback */` |
|   29317 | 11476 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   14656 | 11477 | `			}` |
|  952601 | 11478 | `			if( rc != SXERR_ABORT ){` |
|       - | 11479 | `				/* Generate code for the given tree */` |
|  952601 | 11480 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|       - | 11481 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|       - | 11482 | `				 * expression so they short-circuit to its end. */` |
|  952601 | 11483 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  476298 | 11484 | `			}` |
|  952601 | 11485 | `			nExpr = 1;` |
|  476298 | 11486 | `		}` |
|       - | 11487 | `		/* Release the whole tree */` |
|  952783 | 11488 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 11489 | `		/* Synchronize token stream */` |
|  952783 | 11490 | `		pGen->pEnd = pTmp;` |
|  952783 | 11491 | `		pGen->pIn  = pEnd;` |
|  952783 | 11492 | `		if( rc == SXERR_ABORT ){` |
|      12 | 11493 | `			SySetRelease(&sExprNode);` |
|      12 | 11494 | `			return SXERR_ABORT;` |
|       - | 11495 | `		}` |
|  476384 | 11496 | `	}` |
|  952783 | 11497 | `	SySetRelease(&sExprNode);` |
|  952783 | 11498 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  476399 | 11499 |  |
|       - | 11500 | `/*` |
|       - | 11501 | ` * Return a pointer to the node construct handler associated` |
|       - | 11502 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 11503 | ` */` |
|  249518 | 11504 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       5 | 11505 |  |
|  249523 | 11506 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 11507 | `		/* Numeric literal: Either real or integer */` |
|  125553 | 11508 | `		return PH7_CompileNumLiteral;` |
|  123975 | 11509 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 11510 | `		/* Double quoted string */` |
|   23599 | 11511 | `		return PH7_CompileString;` |
|  100381 | 11512 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 11513 | `		/* Single quoted string */` |
|  100265 | 11514 | `		return PH7_CompileSimpleString;` |
|     121 | 11515 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 11516 | `		/* Heredoc */` |
|      68 | 11517 | `		return PH7_CompileHereDoc;` |
|      57 | 11518 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 11519 | `		/* Nowdoc */` |
|      50 | 11520 | `		return PH7_CompileNowDoc;` |
|       8 | 11521 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 11522 | `		/* Backtick quoted string */` |
|       6 | 11523 | `		return PH7_CompileBacktic;` |
|       - | 11524 | `	}` |
|       3 | 11525 | `	return 0;` |
|  124764 | 11526 |  |
|       - | 11527 | `/*` |
|       - | 11528 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|       - | 11529 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|       - | 11530 | ` * in write context" parse error.` |
|       - | 11531 | ` */` |
|    6868 | 11532 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|       5 | 11533 |  |
|       - | 11534 | `	sxi32 rc;` |
|    6873 | 11535 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|    6871 | 11536 | `		return SXRET_OK;` |
|       - | 11537 | `	}` |
|       5 | 11538 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       2 | 11539 | `		pNode ? pNode->pStart->nLine : 1,` |
|       - | 11540 | `		"Can't use nullsafe operator in write context");` |
|       3 | 11541 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|    3439 | 11542 |  |
|       - | 11543 | `/*` |
|       - | 11544 | ` * Compile an unset() statement.` |
|       - | 11545 | ` * unset($var, $arr[$key], ...);` |
|       - | 11546 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 11547 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 11548 | ` * parent array before extracting the element to unset.` |
|       - | 11549 | ` */` |
|    2980 | 11550 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       5 | 11551 |  |
|    2985 | 11552 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2985 | 11553 | `	sxu32 nIdx = 0;` |
|       - | 11554 | `	SyString sName;` |
|       - | 11555 | `	sxi32 rc;` |
|       - | 11556 | `	/* Jump the 'unset' keyword */` |
|    2985 | 11557 | `	pGen->pIn++;` |
|       - | 11558 | `	/* Save delimiter */` |
|    2985 | 11559 | `	pTmp = pGen->pEnd;` |
|       - | 11560 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2985 | 11561 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2985 | 11562 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 11563 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 11564 | `		SyToken *pClose;` |
|    2985 | 11565 | `		pGen->pIn++;   /* Skip '(' */` |
|    2985 | 11566 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2985 | 11567 | `		pEnd = pClose; /* Stop at ')' */` |
|    1490 | 11568 | `	}` |
|    2985 | 11569 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 11570 | `	/* Resolve the 'unset' builtin name once */` |
|    2985 | 11571 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     365 | 11572 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     365 | 11573 | `		if( pObj == 0 ){` |
|     ! 0 | 11574 | `			return SXERR_ABORT;` |
|       - | 11575 | `		}` |
|     365 | 11576 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     365 | 11577 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     180 | 11578 | `	}` |
|       - | 11579 | `	/* Compile each comma-separated argument */` |
|    9855 | 11580 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6875 | 11581 | `		if( pGen->pIn < pNext ){` |
|    6875 | 11582 | `			pGen->pEnd = pNext;` |
|    6875 | 11583 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 11584 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|       - | 11585 | `				GenStateUnsetValidator);` |
|    6875 | 11586 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11587 | `				return SXERR_ABORT;` |
|       - | 11588 | `			}` |
|    6875 | 11589 | `			if( rc != SXERR_EMPTY ){` |
|       - | 11590 | `				/* Emit call for this single argument */` |
|    6873 | 11591 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6873 | 11592 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|    6873 | 11593 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3434 | 11594 | `			}` |
|    3435 | 11595 | `		}` |
|       - | 11596 | `		/* Jump trailing commas */` |
|   10767 | 11597 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3897 | 11598 | `			pNext++;` |
|       5 | 11599 | `		}` |
|    6875 | 11600 | `		pGen->pIn = pNext;` |
|       5 | 11601 | `	}` |
|       - | 11602 | `	/* Skip past the closing ')' if present */` |
|    2985 | 11603 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2985 | 11604 | `		pGen->pIn++;` |
|    1490 | 11605 | `	}` |
|       - | 11606 | `	/* Restore token stream */` |
|    2985 | 11607 | `	pGen->pEnd = pTmp;` |
|    2985 | 11608 | `	return SXRET_OK;` |
|    1495 | 11609 |  |
|       - | 11610 | `/*` |
|       - | 11611 | ` * PHP Language construct table.` |
|       - | 11612 | ` */` |
|       - | 11613 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 11614 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 11615 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 11616 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 11617 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 11618 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 11619 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 11620 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 11621 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 11622 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 11623 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 11624 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 11625 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 11626 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 11627 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 11628 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 11629 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 11630 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 11631 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 11632 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 11633 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 11634 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 11635 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 11636 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 11637 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 11638 | `};` |
|       - | 11639 | `/*` |
|       - | 11640 | ` * Return a pointer to the statement handler routine associated` |
|       - | 11641 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 11642 | ` */` |
|  639034 | 11643 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 11644 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 11645 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 11646 | `	)` |
|       5 | 11647 |  |
|  639039 | 11648 | `	sxu32 n = 0;` |
| 3312878 | 11649 | `	for(;;){` |
| 6625761 | 11650 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|  136747 | 11651 | `			break;` |
|       - | 11652 | `		}` |
| 6489019 | 11653 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  502297 | 11654 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 11655 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 11656 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 11657 | `					/* 'static' (class context),return null */` |
|     ! 0 | 11658 | `					return 0;` |
|       - | 11659 | `				}` |
|     ! 0 | 11660 | `			}` |
|  502292 | 11661 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       6 | 11662 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       8 | 11663 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - | 11664 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 | 11665 | `				return 0;` |
|       - | 11666 | `			}` |
|       - | 11667 | `			/* Return a pointer to the handler.` |
|       - | 11668 | `			*/` |
|  502297 | 11669 | `			return aLangConstruct[n].xConstruct;` |
|       - | 11670 | `		}` |
| 5986727 | 11671 | `		n++;` |
|       5 | 11672 | `	}` |
|  136747 | 11673 | `	if( pLookahed ){` |
|  136747 | 11674 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|   39201 | 11675 | `			return PH7_CompileClassInterface;` |
|   97551 | 11676 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   97203 | 11677 | `			return PH7_CompileClass;` |
|     353 | 11678 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      69 | 11679 | `			return PH7_CompileTrait;` |
|       - | 11680 | `		}` |
|       - | 11681 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|       - | 11682 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|       - | 11683 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|       - | 11684 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     142 | 11685 | `	}` |
|       - | 11686 | `	/* Not a language construct */` |
|     289 | 11687 | `	return 0;` |
|  319522 | 11688 |  |
|       - | 11689 | `/*` |
|       - | 11690 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 11691 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 11692 | ` */` |
|     284 | 11693 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       5 | 11694 |  |
|       - | 11695 | `	int rc;` |
|     289 | 11696 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     289 | 11697 | `	if( rc == FALSE ){` |
|     174 | 11698 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     173 | 11699 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 11700 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 11701 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 11702 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 11703 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 11704 | `			*/` |
|       - | 11705 | `			){` |
|     171 | 11706 | `				rc = TRUE;` |
|      83 | 11707 | `		}` |
|      87 | 11708 | `	}` |
|     289 | 11709 | `	return rc;` |
|       5 | 11710 |  |
|       - | 11711 | `/*` |
|       - | 11712 | ` * Compile a PHP chunk.` |
|       - | 11713 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 11714 | ` * takes care of generating the appropriate error message.` |
|       - | 11715 | ` */` |
|  764282 | 11716 | `static sxi32 GenStateCompileChunk(` |
|       - | 11717 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 11718 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 11719 | `	)` |
|       5 | 11720 |  |
|       - | 11721 | `	ProcLangConstruct xCons;` |
|       - | 11722 | `	sxi32 rc;` |
|  764287 | 11723 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  597811 | 11724 | `	for(;;){` |
|  979957 | 11725 | `		int bStmtIsDeclare = 0;` |
|  979957 | 11726 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 11727 | `			/* No more input to process */` |
|   14273 | 11728 | `			break;` |
|       - | 11729 | `		}` |
|       - | 11730 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|       - | 11731 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  965689 | 11732 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  642627 | 11733 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  642627 | 11734 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|      45 | 11735 | `				bStmtIsDeclare = 1;` |
|      20 | 11736 | `			}` |
|  321311 | 11737 | `		}` |
|  965689 | 11738 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|       - | 11739 | `			/* Any non-declare top-level statement locks the strict_types` |
|       - | 11740 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|  215645 | 11741 | `			pGen->bStrictTypesLocked = 1;` |
|  107820 | 11742 | `		}` |
|  965689 | 11743 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 11744 | `			/* Compile block */` |
|      20 | 11745 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      20 | 11746 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11747 | `				break;` |
|       - | 11748 | `			}` |
|      12 | 11749 | `		}else{` |
|  965673 | 11750 | `			xCons = 0;` |
|  965673 | 11751 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|       - | 11752 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|       - | 11753 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|       - | 11754 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|    3619 | 11755 | `				xCons = PH7_CompileClassModifiers;` |
|  963866 | 11756 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  639039 | 11757 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 11758 | `				/* Try to extract a language construct handler */` |
|  639039 | 11759 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  639039 | 11760 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 11761 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 11762 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 11763 | `						&pGen->pIn->sData);` |
|       9 | 11764 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 11765 | `						break;` |
|       - | 11766 | `					}` |
|       - | 11767 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 11768 | `					 * this erroneous statement.` |
|       - | 11769 | `					 */` |
|       9 | 11770 | `					xCons = PH7_ErrorRecover;` |
|       4 | 11771 | `				}` |
|  642542 | 11772 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   52909 | 11773 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 11774 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     117 | 11775 | `				xCons = PH7_CompileLabel;` |
|      56 | 11776 | `			}` |
|  965673 | 11777 | `			if( xCons == 0 ){` |
|       - | 11778 | `				/* Assume an expression an try to compile it */` |
|  323189 | 11779 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  323189 | 11780 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 11781 | `					/* Pop l-value */` |
|  323039 | 11782 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  161517 | 11783 | `				}` |
|  161597 | 11784 | `			}else{` |
|       - | 11785 | `				/* Go compile the sucker */` |
|  642489 | 11786 | `				rc = xCons(&(*pGen));` |
|       - | 11787 | `			}` |
|  965673 | 11788 | `			if( rc == SXERR_ABORT ){` |
|       - | 11789 | `				/* Request to abort compilation */` |
|      12 | 11790 | `				break;` |
|       - | 11791 | `			}` |
|       - | 11792 | `		}` |
|       - | 11793 | `		/* Ignore trailing semi-colons ';' */` |
| 1561451 | 11794 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  595777 | 11795 | `			pGen->pIn++;` |
|       5 | 11796 | `		}` |
|  965679 | 11797 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 11798 | `			/* Compile a single statement and return */` |
|  750009 | 11799 | `			break;` |
|       - | 11800 | `		}` |
|       - | 11801 | `		/* LOOP ONE */` |
|       - | 11802 | `		/* LOOP TWO */` |
|       - | 11803 | `		/* LOOP THREE */` |
|       - | 11804 | `		/* LOOP FOUR */` |
|       5 | 11805 | `	}` |
|       - | 11806 | `	/* Return compilation status */` |
|  764287 | 11807 | `	return rc;` |
|       5 | 11808 |  |
|       - | 11809 | `/*` |
|       - | 11810 | ` * Compile a Raw PHP chunk.` |
|       - | 11811 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 11812 | ` * takes care of generating the appropriate error message.` |
|       - | 11813 | ` */` |
|   14280 | 11814 | `static sxi32 PH7_CompilePHP(` |
|       - | 11815 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 11816 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 11817 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 11818 | `	)` |
|       5 | 11819 |  |
|   14285 | 11820 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 11821 | `	sxi32 rc;` |
|       - | 11822 | `	/* Reset the token set */` |
|   14285 | 11823 | `	SySetReset(&(*pTokenSet));` |
|       - | 11824 | `	/* Mark as the default token set */` |
|   14285 | 11825 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 11826 | `	/* Advance the stream cursor */` |
|   14285 | 11827 | `	pGen->pRawIn++;` |
|       - | 11828 | `	/* Tokenize the PHP chunk first */` |
|   14285 | 11829 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 11830 | `	/* Point to the head and tail of the token stream. */` |
|   14285 | 11831 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   14285 | 11832 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   14285 | 11833 | `	if( is_expr ){` |
|     ! 0 | 11834 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 11835 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 11836 | `			/* A simple expression,compile it */` |
|     ! 0 | 11837 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 11838 | `		}` |
|       - | 11839 | `		/* Emit the DONE instruction */` |
|     ! 0 | 11840 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 11841 | `		return SXRET_OK;` |
|       - | 11842 | `	}` |
|   14285 | 11843 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 11844 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 11845 | `		/*` |
|       - | 11846 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 11847 | `		 * According to the PHP reference manual:` |
|       - | 11848 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 11849 | `		 *  immediately follow` |
|       - | 11850 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 11851 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 11852 | `		 * Symisc extension:` |
|       - | 11853 | `		 *   This short syntax works with all PHP opening` |
|       - | 11854 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 11855 | `		 *   only short tag.` |
|       - | 11856 | `		 */` |
|       - | 11857 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 11858 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 11859 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 11860 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 11861 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 11862 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 11863 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 11864 | `		}` |
|       3 | 11865 | `		return SXRET_OK;` |
|       - | 11866 | `	}` |
|       - | 11867 | `	/* Compile the PHP chunk */` |
|   14283 | 11868 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 11869 | `	/* Fix exceptions jumps */` |
|   14283 | 11870 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 11871 | `	/* Fix gotos now, the jump destination is resolved */` |
|   14283 | 11872 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 11873 | `		rc = SXERR_ABORT;` |
|       1 | 11874 | `	}` |
|       - | 11875 | `	/* Reset container */` |
|   14283 | 11876 | `	SySetReset(&pGen->aGoto);` |
|   14283 | 11877 | `	SySetReset(&pGen->aLabel);` |
|   14283 | 11878 | `	SySetReset(&pGen->aNullsafeJmp);` |
|       - | 11879 | `	/* Compilation result */` |
|   14283 | 11880 | `	return rc;` |
|    7145 | 11881 |  |
|       - | 11882 | `/*` |
|       - | 11883 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 11884 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 11885 | ` * This is the only compile interface exported from this file.` |
|       - | 11886 | ` */` |
|   17248 | 11887 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 11888 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 11889 | `	SyString *pScript,  /* Script to compile */` |
|       - | 11890 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 11891 | `	)` |
|       5 | 11892 |  |
|       - | 11893 | `	SySet aPhpToken,aRawToken;` |
|       - | 11894 | `	ph7_gen_state *pCodeGen;` |
|       - | 11895 | `	ph7_value *pRawObj;` |
|       - | 11896 | `	sxu32 nObjIdx;` |
|       - | 11897 | `	sxi32 nRawObj;` |
|       - | 11898 | `	int is_expr;` |
|       - | 11899 | `	sxi8 bSavedStrict;` |
|       - | 11900 | `	sxi8 bSavedStrictLocked;` |
|       - | 11901 | `	sxi32 rc;` |
|   17253 | 11902 | `	if( pScript->nByte < 1 ){` |
|       - | 11903 | `		/* Nothing to compile */` |
|     ! 0 | 11904 | `		return PH7_OK;` |
|       - | 11905 | `	}` |
|       - | 11906 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|       - | 11907 | `	 * file's flags so include/require restore them on return. */` |
|   17253 | 11908 | `	pCodeGen = &pVm->sCodeGen;` |
|   17253 | 11909 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|   17253 | 11910 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|   17253 | 11911 | `	pCodeGen->bStrictTypes = 0;` |
|   17253 | 11912 | `	pCodeGen->bStrictTypesLocked = 0;` |
|       - | 11913 | `	/* Initialize the tokens containers */` |
|   17253 | 11914 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   17253 | 11915 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   17253 | 11916 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   17253 | 11917 | `	is_expr = 0;` |
|   17253 | 11918 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 11919 | `		SyToken sTmp;` |
|       - | 11920 | `		/* PHP only: -*/` |
|    3629 | 11921 | `		sTmp.nLine = 1;` |
|    3629 | 11922 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    3629 | 11923 | `		sTmp.pUserData = 0;` |
|    3629 | 11924 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    3629 | 11925 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    3629 | 11926 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 11927 | `			/* A simple PHP expression */` |
|     ! 0 | 11928 | `			is_expr = 1;` |
|     ! 0 | 11929 | `		}` |
|    1817 | 11930 | `	}else{` |
|       - | 11931 | `		/* Tokenize raw text */` |
|   13629 | 11932 | `		SySetAlloc(&aRawToken,32);` |
|   13629 | 11933 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 11934 | `	}` |
|       - | 11935 | `	/* Process high-level tokens */` |
|   17253 | 11936 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   17253 | 11937 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   17253 | 11938 | `	rc = PH7_OK;` |
|   17253 | 11939 | `	if( is_expr ){` |
|       - | 11940 | `		/* Compile the expression */` |
|     ! 0 | 11941 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 11942 | `		goto cleanup;` |
|       - | 11943 | `	}` |
|   17253 | 11944 | `	nObjIdx = 0;` |
|       - | 11945 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 11946 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 11947 | `	 * preventing namespace bleeding across include()d files. */` |
|   17253 | 11948 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 11949 | `	/* Start the compilation process */` |
|   15442 | 11950 | `	for(;;){` |
|   45157 | 11951 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   17241 | 11952 | `			break; /* No more tokens to process */` |
|       - | 11953 | `		}` |
|   27921 | 11954 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 11955 | `			/* Compile the PHP chunk */` |
|   14285 | 11956 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   14285 | 11957 | `			if( rc == SXERR_ABORT ){` |
|      15 | 11958 | `				break;` |
|       - | 11959 | `			}` |
|   14273 | 11960 | `			continue;` |
|       - | 11961 | `		}` |
|       - | 11962 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   13641 | 11963 | `		nRawObj = 0;` |
|   27319 | 11964 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 11965 | `			/* Consume the raw chunk without any processing */` |
|   13683 | 11966 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   13683 | 11967 | `			if( pRawObj == 0 ){` |
|     ! 0 | 11968 | `				rc = SXERR_MEM;` |
|     ! 0 | 11969 | `				break;` |
|       - | 11970 | `			}` |
|       - | 11971 | `			/* Mark as constant and emit the load constant instruction */` |
|   13683 | 11972 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   13683 | 11973 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   13683 | 11974 | `			++nRawObj;` |
|   13683 | 11975 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       5 | 11976 | `		}` |
|   13641 | 11977 | `		if( nRawObj > 0 ){` |
|       - | 11978 | `			/* Emit the consume instruction */` |
|   13641 | 11979 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    6818 | 11980 | `		}` |
|    8629 | 11981 | `	}` |
|    8624 | 11982 | `cleanup:` |
|   17253 | 11983 | `	SySetRelease(&aRawToken);` |
|   17253 | 11984 | `	SySetRelease(&aPhpToken);` |
|       - | 11985 | `	/* Restore outer file's strict_types scope */` |
|   17253 | 11986 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|   17253 | 11987 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|   17253 | 11988 | `	return rc;` |
|    8629 | 11989 |  |
|       - | 11990 | `/*` |
|       - | 11991 | ` * Utility routines.Initialize the code generator.` |
|       - | 11992 | ` */` |
|    3556 | 11993 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 11994 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 11995 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 11996 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 11997 | `	)` |
|       5 | 11998 |  |
|    3561 | 11999 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 12000 | `	/* Zero the structure */` |
|    3561 | 12001 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 12002 | `	/* Initial state */` |
|    3561 | 12003 | `	pGen->pVm  = &(*pVm);` |
|    3561 | 12004 | `	pGen->xErr = xErr;` |
|    3561 | 12005 | `	pGen->pErrData = pErrData;` |
|    3561 | 12006 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    3561 | 12007 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    3561 | 12008 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|    3561 | 12009 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    3561 | 12010 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 12011 | `	/* Error log buffer */` |
|    3561 | 12012 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 12013 | `	/* General purpose working buffer */` |
|    3561 | 12014 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 12015 | `	/* Namespace state */` |
|    3561 | 12016 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    3561 | 12017 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    3561 | 12018 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    3561 | 12019 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 12020 | `	/* Create the global scope */` |
|    3561 | 12021 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 12022 | `	/* Point to the global scope */` |
|    3561 | 12023 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    3561 | 12024 | `	return SXRET_OK;` |
|       5 | 12025 |  |
|       - | 12026 | `/*` |
|       - | 12027 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 12028 | ` */` |
|   20458 | 12029 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 12030 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 12031 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 12032 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 12033 | `	)` |
|       5 | 12034 |  |
|   20463 | 12035 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 12036 | `	GenBlock *pBlock,*pParent;` |
|       - | 12037 | `	/* Reset state */` |
|   20463 | 12038 | `	SySetReset(&pGen->aLabel);` |
|   20463 | 12039 | `	SySetReset(&pGen->aGoto);` |
|   20463 | 12040 | `	SySetReset(&pGen->aNullsafeJmp);` |
|   20463 | 12041 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   20463 | 12042 | `	SyBlobRelease(&pGen->sWorker);` |
|   20463 | 12043 | `	SyBlobRelease(&pGen->sNamespace);` |
|   20463 | 12044 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   20463 | 12045 | `	SyHashRelease(&pGen->hUseImports);` |
|   20463 | 12046 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   20463 | 12047 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   20463 | 12048 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   20463 | 12049 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   20463 | 12050 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 12051 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 12052 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 12053 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 12054 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 12055 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 12056 | `	 * number of unique names, which is acceptable. */` |
|       - | 12057 | `	/* Point to the global scope */` |
|   20463 | 12058 | `	pBlock = pGen->pCurrent;` |
|   20463 | 12059 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 12060 | `		pParent = pBlock->pParent;` |
|     ! 0 | 12061 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 12062 | `		pBlock = pParent;` |
|     ! 0 | 12063 | `	}` |
|   20463 | 12064 | `	pGen->xErr = xErr;` |
|   20463 | 12065 | `	pGen->pErrData = pErrData;` |
|   20463 | 12066 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   20463 | 12067 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   20463 | 12068 | `	pGen->pIn = pGen->pEnd = 0;` |
|   20463 | 12069 | `	pGen->nErr = 0;` |
|   20463 | 12070 | `	return SXRET_OK;` |
|       5 | 12071 |  |
|       - | 12072 | `/*` |
|       - | 12073 | ` * Generate a compile-time error message.` |
|       - | 12074 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 12075 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 12076 | ` * abort compilation immediately.` |
|       - | 12077 | ` */` |
|     610 | 12078 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       5 | 12079 |  |
|     615 | 12080 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     615 | 12081 | `	const char *zErr = "Error";` |
|       - | 12082 | `	SyString *pFile;` |
|       - | 12083 | `	va_list ap;` |
|       - | 12084 | `	sxi32 rc;` |
|       - | 12085 | `	/* Reset the working buffer */` |
|     615 | 12086 | `	SyBlobReset(pWorker);` |
|       - | 12087 | `	/* Peek the processed file path if available */` |
|     615 | 12088 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     615 | 12089 | `	if( nErrType == E_ERROR ){` |
|       - | 12090 | `		/* Increment the error counter */` |
|     507 | 12091 | `		pGen->nErr++;` |
|     507 | 12092 | `		if( pGen->nErr > 15 ){` |
|       - | 12093 | `			/* Error count limit reached */` |
|       6 | 12094 | `			if( pGen->xErr ){` |
|       6 | 12095 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       6 | 12096 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       6 | 12097 | `				if( pFile ){` |
|       6 | 12098 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 12099 | `				}` |
|       6 | 12100 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       6 | 12101 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       6 | 12102 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 12103 | `				}` |
|       2 | 12104 | `			}` |
|       - | 12105 | `			/* Abort immediately */` |
|       6 | 12106 | `			return SXERR_ABORT;` |
|       - | 12107 | `		}` |
|     249 | 12108 | `	}` |
|     611 | 12109 | `	if( pGen->xErr == 0 ){` |
|       - | 12110 | `		/* No available error consumer,return immediately */` |
|       3 | 12111 | `		return SXRET_OK;` |
|       - | 12112 | `	}` |
|     608 | 12113 | `	switch(nErrType){` |
|     500 | 12114 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      29 | 12115 | `	case E_WARNING: zErr = "Warning";     break;` |
|      78 | 12116 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|      11 | 12117 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 12118 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 12119 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 12120 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 12121 | `	default:` |
|     ! 0 | 12122 | `		break;` |
|       - | 12123 | `	}` |
|     608 | 12124 | `	rc = SXRET_OK;` |
|       - | 12125 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     608 | 12126 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     608 | 12127 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     608 | 12128 | `	va_start(ap,zFormat);` |
|     608 | 12129 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     608 | 12130 | `	va_end(ap);` |
|     608 | 12131 | `	if( pFile ){` |
|     608 | 12132 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     302 | 12133 | `	}` |
|       - | 12134 | `	/* Append a new line */` |
|     608 | 12135 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     608 | 12136 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 12137 | `		/* Consume the generated error message */` |
|     608 | 12138 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     302 | 12139 | `	}` |
|     608 | 12140 | `	return rc;` |
|     310 | 12141 |  |
|       - | 12142 |  |
