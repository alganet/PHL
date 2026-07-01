# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5794/7189 lines (80.60%)

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
|       - |    37 | `{` |
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
|       - |    53 | `{` |
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
|       - |    66 | `{` |
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
|       5 |   114 | `{` |
|       - |   115 | `	Label *aLabel;` |
|       - |   116 | `	sxu32 n;` |
|       - |   117 | `	/* Perform a linear scan on the label table */` |
|     153 |   118 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|     333 |   119 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     277 |   120 | `		if( SyStringCmp(&aLabel[n].sName,pName,SyMemcmp) == 0 ){` |
|       - |   121 | `			/* Jump destination found */` |
|      96 |   122 | `			aLabel[n].bRef = TRUE;` |
|      96 |   123 | `			if( ppOut ){` |
|      96 |   124 | `				*ppOut = &aLabel[n];` |
|      46 |   125 | `			}` |
|      96 |   126 | `			return SXRET_OK;` |
|       - |   127 | `		}` |
|      93 |   128 | `	}` |
|       - |   129 | `	/* No such destination */` |
|      60 |   130 | `	return SXERR_NOTFOUND;` |
|      79 |   131 | `}` |
|       - |   132 | `/*` |
|       - |   133 | ` * Fetch a block that correspond to the given criteria from the stack of` |
|       - |   134 | ` * compiled blocks.` |
|       - |   135 | ` * Return a pointer to that block on success. NULL otherwise.` |
|       - |   136 | ` */` |
|    3890 |   137 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       5 |   138 | `{` |
|    3895 |   139 | `	GenBlock *pBlock = pCurrent;` |
|   11087 |   140 | `	for(;;){` |
|   22179 |   141 | `		if( pBlock->iFlags & iBlockType ){` |
|    3787 |   142 | `			iCount--; /* Decrement nesting level */` |
|    3787 |   143 | `			if( iCount < 1 ){` |
|       - |   144 | `				/* Block meet with the desired criteria */` |
|    3761 |   145 | `				return pBlock;` |
|       - |   146 | `			}` |
|      13 |   147 | `		}` |
|       - |   148 | `		/* Point to the upper block */` |
|   18423 |   149 | `		pBlock = pBlock->pParent;` |
|   18423 |   150 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |   151 | `			/* Forbidden */` |
|      72 |   152 | `			break;` |
|       - |   153 | `		}` |
|       5 |   154 | `	}` |
|       - |   155 | `	/* No such block */` |
|     139 |   156 | `	return 0;` |
|    1950 |   157 | `}` |
|       - |   158 | `/*` |
|       - |   159 | ` * Initialize a freshly allocated block instance.` |
|       - |   160 | ` */` |
|  853430 |   161 | `static void GenStateInitBlock(` |
|       - |   162 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   163 | `	GenBlock *pBlock,    /* Target block */` |
|       - |   164 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   165 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |   166 | `	void *pUserData      /* Upper layer private data */` |
|       - |   167 | `	)` |
|       5 |   168 | `{` |
|       - |   169 | `	/* Initialize block fields */` |
|  853435 |   170 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  853435 |   171 | `	pBlock->pUserData   = pUserData;` |
|  853435 |   172 | `	pBlock->pGen        = pGen;` |
|  853435 |   173 | `	pBlock->iFlags      = iType;` |
|  853435 |   174 | `	pBlock->pParent     = 0;` |
|  853435 |   175 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  853435 |   176 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  853435 |   177 | `}` |
|       - |   178 | `/*` |
|       - |   179 | ` * Allocate a new block instance.` |
|       - |   180 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |   181 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |   182 | ` * processing on failure.` |
|       - |   183 | ` */` |
|  849818 |   184 | `static sxi32 GenStateEnterBlock(` |
|       - |   185 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |   186 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   187 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |   188 | `	void *pUserData,      /* Upper layer private data */` |
|       - |   189 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |   190 | `	)` |
|       5 |   191 | `{` |
|       - |   192 | `	GenBlock *pBlock;` |
|       - |   193 | `	/* Allocate a new block instance */` |
|  849823 |   194 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  849823 |   195 | `	if( pBlock == 0 ){` |
|       - |   196 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |   197 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |   198 | `		 */` |
|     ! 0 |   199 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |   200 | `		/* Abort processing immediately */` |
|     ! 0 |   201 | `		return SXERR_ABORT;` |
|       - |   202 | `	}` |
|       - |   203 | `	/* Zero the structure */` |
|  849823 |   204 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  849823 |   205 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |   206 | `	/* Link to the parent block */` |
|  849823 |   207 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |   208 | `	/* Mark as the current block */` |
|  849823 |   209 | `	pGen->pCurrent = pBlock;` |
|  849823 |   210 | `	if( ppBlock ){` |
|       - |   211 | `		/* Write a pointer to the new instance */` |
|  412795 |   212 | `		*ppBlock = pBlock;` |
|  206395 |   213 | `	}` |
|  849823 |   214 | `	return SXRET_OK;` |
|  424914 |   215 | `}` |
|       - |   216 | `/*` |
|       - |   217 | ` * Release block fields without freeing the whole instance.` |
|       - |   218 | ` */` |
|  849810 |   219 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       5 |   220 | `{` |
|  849815 |   221 | `	SySetRelease(&pBlock->aPostContFix);` |
|  849815 |   222 | `	SySetRelease(&pBlock->aJumpFix);` |
|  849815 |   223 | `}` |
|       - |   224 | `/*` |
|       - |   225 | ` * Release a block.` |
|       - |   226 | ` */` |
|  849810 |   227 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       5 |   228 | `{` |
|  849815 |   229 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  849815 |   230 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |   231 | `	/* Free the instance */` |
|  849815 |   232 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  849815 |   233 | `}` |
|       - |   234 | `/*` |
|       - |   235 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |   236 | ` */` |
|  849810 |   237 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       5 |   238 | `{` |
|  849815 |   239 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  849815 |   240 | `	if( pBlock == 0 ){` |
|       - |   241 | `		/* No more block to pop */` |
|     ! 0 |   242 | `		return SXERR_EMPTY;` |
|       - |   243 | `	}` |
|       - |   244 | `	/* Point to the upper block */` |
|  849815 |   245 | `	pGen->pCurrent = pBlock->pParent;` |
|  849815 |   246 | `	if( ppBlock ){` |
|       - |   247 | `		/* Write a pointer to the popped block */` |
|     ! 0 |   248 | `		*ppBlock = pBlock;` |
|     ! 0 |   249 | `	}else{` |
|       - |   250 | `		/* Safely release the block */` |
|  849815 |   251 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |   252 | `	}` |
|  849815 |   253 | `	return SXRET_OK;` |
|  424910 |   254 | `}` |
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
|  244782 |   265 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       5 |   266 | `{` |
|       - |   267 | `	JumpFixup sJumpFix;` |
|       - |   268 | `	sxi32 rc;` |
|       - |   269 | `	/* Init the JumpFixup structure */` |
|  244787 |   270 | `	sJumpFix.nJumpType = nJumpType;` |
|  244787 |   271 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |   272 | `	/* Insert in the jump fixup table */` |
|  244787 |   273 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  244787 |   274 | `	return rc;` |
|       5 |   275 | `}` |
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
|  593108 |   287 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       5 |   288 | `{` |
|       - |   289 | `	JumpFixup *aFix;` |
|       - |   290 | `	VmInstr *pInstr;` |
|       - |   291 | `	sxu32 nFixed;` |
|       - |   292 | `	sxu32 n;` |
|       - |   293 | `	/* Point to the jump fixup table */` |
|  593113 |   294 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |   295 | `	/* Fix the desired jumps */` |
| 1071575 |   296 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  478467 |   297 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |   298 | `			/* Already fixed */` |
|  189205 |   299 | `			continue;` |
|       - |   300 | `		}` |
|  289267 |   301 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |   302 | `			/* Not of our interest */` |
|   44487 |   303 | `			continue;` |
|       - |   304 | `		}` |
|       - |   305 | `		/* Point to the instruction to fix */` |
|  244785 |   306 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  244785 |   307 | `		if( pInstr ){` |
|  244785 |   308 | `			pInstr->iP2 = nJumpDest;` |
|  244785 |   309 | `			nFixed++;` |
|       - |   310 | `			/* Mark as fixed */` |
|  244785 |   311 | `			aFix[n].nJumpType = -1;` |
|  122390 |   312 | `		}` |
|  122395 |   313 | `	}` |
|       - |   314 | `	/* Total number of fixed jumps */` |
|  593113 |   315 | `	return nFixed;` |
|       5 |   316 | `}` |
|       - |   317 | `/*` |
|       - |   318 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |   319 | ` * The goto statement can be used to jump to another section` |
|       - |   320 | ` * in the program.` |
|       - |   321 | ` * Refer to the routine responsible of compiling the goto` |
|       - |   322 | ` * statement for more information.` |
|       - |   323 | ` */` |
|  240756 |   324 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       5 |   325 | `{` |
|       - |   326 | `	JumpFixup *pJump,*aJumps;` |
|       - |   327 | `	Label *pLabel,*aLabel;` |
|       - |   328 | `	VmInstr *pInstr;` |
|       - |   329 | `	sxi32 rc;` |
|       - |   330 | `	sxu32 n;` |
|       - |   331 | `	/* Point to the goto table */` |
|  240761 |   332 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |   333 | `	/* Fix */` |
|  240907 |   334 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
|     153 |   335 | `		pJump = &aJumps[n];` |
|       - |   336 | `		/* Extract the target label */` |
|     153 |   337 | `		rc = GenStateGetLabel(&(*pGen),&pJump->sLabel,&pLabel);` |
|     153 |   338 | `		if( rc != SXRET_OK ){` |
|       - |   339 | `			/* No such label */` |
|      60 |   340 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' was referenced but not defined",&pJump->sLabel);` |
|      60 |   341 | `			if( rc == SXERR_ABORT ){` |
|       3 |   342 | `				return SXERR_ABORT;` |
|       - |   343 | `			}` |
|      58 |   344 | `			continue;` |
|       - |   345 | `		}` |
|       - |   346 | `		/* Make sure the target label is reachable */` |
|      96 |   347 | `		if( pLabel->pFunc != pJump->pFunc ){` |
|      10 |   348 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' is unreachable",&pJump->sLabel);` |
|      10 |   349 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |   350 | `				return SXERR_ABORT;` |
|       - |   351 | `			}` |
|       4 |   352 | `		}` |
|       - |   353 | `		/* Fix the jump now the destination is resolved */` |
|      96 |   354 | `		pInstr = PH7_VmGetInstr(pGen->pVm,pJump->nInstrIdx);` |
|      96 |   355 | `		if( pInstr ){` |
|      96 |   356 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|      46 |   357 | `		}` |
|      50 |   358 | `	}` |
|  240759 |   359 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  240891 |   360 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     137 |   361 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |   362 | `			/* Emit a warning */` |
|      40 |   363 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |   364 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |   365 | `		}` |
|      71 |   366 | `	}` |
|  240759 |   367 | `	return SXRET_OK;` |
|  120383 |   368 | `}` |
|       - |   369 | `/*` |
|       - |   370 | ` * Check if a given token value is installed in the literal table.` |
|       - |   371 | ` */` |
|  776838 |   372 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       5 |   373 | `{` |
|       - |   374 | `	SyHashEntry *pEntry;` |
|  776843 |   375 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  776843 |   376 | `	if( pEntry == 0 ){` |
|  349805 |   377 | `		return SXERR_NOTFOUND;` |
|       - |   378 | `	}` |
|  427043 |   379 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  427043 |   380 | `	return SXRET_OK;` |
|  388424 |   381 | `}` |
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
|  349800 |   392 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       5 |   393 | `{` |
|  349805 |   394 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  349805 |   395 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  174900 |   396 | `	}` |
|  349805 |   397 | `	return SXRET_OK;` |
|       5 |   398 | `}` |
|       - |   399 | `/*` |
|       - |   400 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |   401 | ` * in the constant table.` |
|       - |   402 | ` */` |
|  126786 |   403 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       5 |   404 | `{` |
|       - |   405 | `	ph7_value *pObj;` |
|  126791 |   406 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |   407 | `	/* Reserve a new constant */` |
|  126791 |   408 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  126791 |   409 | `	if( pObj == 0 ){` |
|     ! 0 |   410 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   411 | `		return 0;` |
|       - |   412 | `	}` |
|  126791 |   413 | `	*pIdx = nIdx;` |
|       - |   414 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |   415 | `	 * the constant string iterals table [optimization purposes].` |
|       - |   416 | `	 */` |
|  126791 |   417 | `	return pObj;` |
|   63398 |   418 | `}` |
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
|  484986 |   433 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|       5 |   434 | `{` |
|       - |   435 | `	VmCallArgMap *pMap;` |
|  484991 |   436 | `	if( !pGen->bStrictTypes ) return p3;` |
|      33 |   437 | `	if( p3 == 0 ){` |
|      31 |   438 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|      31 |   439 | `		if( pMap == 0 ) return 0;` |
|      31 |   440 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|      31 |   441 | `		p3 = (void *)pMap;` |
|      14 |   442 | `	}` |
|      33 |   443 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|      33 |   444 | `	return p3;` |
|  242498 |   445 | `}` |
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
|       5 |   483 | `{` |
|    1081 |   484 | `	if( base == 16 ){ return SyisHex(c); }` |
|     982 |   485 | `	if( base == 2 ){ return c == '0' \|\| c == '1'; }` |
|     703 |   486 | `	return SyisDigit(c);` |
|     543 |   487 | `}` |
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
|  127516 |   504 | `static int GenStateFindBadNumericSeparator(` |
|       - |   505 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       5 |   506 | `{` |
|  127521 |   507 | `	const char *z = pRaw->zString;` |
|  127521 |   508 | `	sxu32 n = pRaw->nByte;` |
|  127521 |   509 | `	int base = 10;` |
|       - |   510 | `	sxu32 i, start;` |
|  127521 |   511 | `	if( n < 2 ) return 0;` |
|   10617 |   512 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |   513 | `		base = 16;` |
|   10582 |   514 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |   515 | `		base = 2;` |
|     139 |   516 | `	}` |
|   38411 |   517 | `	for( i = 0; i < n; ++i ){` |
|   27813 |   518 | `		if( z[i] != '_' ) continue;` |
|     814 |   519 | `		if( i > 0 && i + 1 < n` |
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
|   10603 |   535 | `	return 0;` |
|   63763 |   536 | `}` |
|       - |   537 | `/*` |
|       - |   538 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   539 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   540 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   541 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   542 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   543 | ` * so callers can bail from the current construct).` |
|       - |   544 | ` */` |
|  127516 |   545 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       5 |   546 | `{` |
|  127521 |   547 | `	const char *zBad = 0;` |
|  127521 |   548 | `	sxu32 nBad = 0;` |
|       - |   549 | `	SyString sBad;` |
|       - |   550 | `	sxi32 rc;` |
|  127521 |   551 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  127507 |   552 | `		return SXRET_OK;` |
|       - |   553 | `	}` |
|      18 |   554 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      18 |   555 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |   556 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      18 |   557 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |   558 | `		return SXERR_ABORT;` |
|       - |   559 | `	}` |
|      18 |   560 | `	return SXERR_SYNTAX;` |
|   63763 |   561 | `}` |
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
|  127502 |   578 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |   579 | `	SyMemBackend *pAlloc,` |
|       - |   580 | `	const SyString *pToken,` |
|       - |   581 | `	char *zScratch, sxu32 nScratch,` |
|       - |   582 | `	SyString *pOut, char **pzAlloc)` |
|       5 |   583 | `{` |
|       - |   584 | `	sxu32 i, j;` |
|  127507 |   585 | `	int hasUnderscore = 0;` |
|       - |   586 | `	char *zBuf;` |
|  127507 |   587 | `	*pzAlloc = 0;` |
|  270139 |   588 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  142889 |   589 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   71321 |   590 | `	}` |
|  127507 |   591 | `	if( !hasUnderscore ){` |
|  127255 |   592 | `		SyStringDupPtr(pOut, pToken);` |
|  127255 |   593 | `		return SXRET_OK;` |
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
|   63756 |   610 | `}` |
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
|  127488 |   627 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   628 | `{` |
|  127493 |   629 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  127493 |   630 | `	sxu32 nIdx = 0;` |
|       - |   631 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  127493 |   632 | `	char *zAlloc = 0;` |
|       - |   633 | `	SyString sNum;` |
|       - |   634 | `	sxi32 rc;` |
|   63744 |   635 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  127493 |   636 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  127493 |   637 | `	if( rc != SXRET_OK ){` |
|      14 |   638 | `		return rc;` |
|       - |   639 | `	}` |
|  191222 |   640 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   63739 |   641 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  127483 |   642 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   643 | `		return SXERR_ABORT;` |
|       - |   644 | `	}` |
|  127483 |   645 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |   646 | `		ph7_value *pObj;` |
|       - |   647 | `		sxi64 iValue;` |
|  126791 |   648 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|  126791 |   649 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  126791 |   650 | `		if( pObj == 0 ){` |
|     ! 0 |   651 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   652 | `			return SXERR_ABORT;` |
|       - |   653 | `		}` |
|  126791 |   654 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   63398 |   655 | `	}else{` |
|       - |   656 | `		/* Real number */` |
|       - |   657 | `		ph7_value *pObj;` |
|       - |   658 | `		/* Reserve a new constant */` |
|     696 |   659 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     696 |   660 | `		if( pObj == 0 ){` |
|     ! 0 |   661 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   662 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   663 | `			return SXERR_ABORT;` |
|       - |   664 | `		}` |
|     696 |   665 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|     696 |   666 | `		PH7_MemObjToReal(pObj);` |
|       - |   667 | `	}` |
|  127483 |   668 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |   669 | `	/* Emit the load constant instruction */` |
|  127483 |   670 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   671 | `	/* Node successfully compiled */` |
|  127483 |   672 | `	return SXRET_OK;` |
|   63749 |   673 | `}` |
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
|  101818 |   685 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   686 | `{` |
|  101823 |   687 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |   688 | `	const char *zIn,*zCur,*zEnd;` |
|       - |   689 | `	ph7_value *pObj;` |
|       - |   690 | `	sxu32 nIdx;` |
|  101823 |   691 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |   692 | `	/* Delimit the string */` |
|  101823 |   693 | `	zIn  = pStr->zString;` |
|  101823 |   694 | `	zEnd = &zIn[pStr->nByte];` |
|  101823 |   695 | `	if( zIn >= zEnd ){` |
|       - |   696 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |   697 | `		 * rather than reserving a new object each time. */` |
|    7395 |   698 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    7395 |   699 | `		return SXRET_OK;` |
|       - |   700 | `	}` |
|   94433 |   701 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |   702 | `		/* Already processed,emit the load constant instruction` |
|       - |   703 | `		 * and return.` |
|       - |   704 | `		 */` |
|   36505 |   705 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   36505 |   706 | `		return SXRET_OK;` |
|       - |   707 | `	}` |
|       - |   708 | `	/* Reserve a new constant */` |
|   57933 |   709 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   57933 |   710 | `	if( pObj == 0 ){` |
|     ! 0 |   711 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   712 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   713 | `		return SXERR_ABORT;` |
|       - |   714 | `	}` |
|   57933 |   715 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |   716 | `	/* Compile the node */` |
|   57985 |   717 | `	for(;;){` |
|  115975 |   718 | `		if( zIn >= zEnd ){` |
|       - |   719 | `			/* End of input */` |
|   57933 |   720 | `			break;` |
|       - |   721 | `		}` |
|   58047 |   722 | `		zCur = zIn;` |
|  991703 |   723 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  933661 |   724 | `			zIn++;` |
|       5 |   725 | `		}` |
|   58047 |   726 | `		if( zIn > zCur ){` |
|       - |   727 | `			/* Append raw contents*/` |
|   58023 |   728 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   29009 |   729 | `		}` |
|   58047 |   730 | `		zIn++;` |
|   58047 |   731 | `		if( zIn < zEnd ){` |
|     136 |   732 | `			if( zIn[0] == '\\' ){` |
|       - |   733 | `				/* A literal backslash */` |
|      23 |   734 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|     125 |   735 | `			}else if( zIn[0] == '\'' ){` |
|       - |   736 | `				/* A single quote */` |
|      11 |   737 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       6 |   738 | `			}else{` |
|       - |   739 | `				/* verbatim copy */` |
|     104 |   740 | `				zIn--;` |
|     104 |   741 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|     104 |   742 | `				zIn++;` |
|       - |   743 | `			}` |
|      67 |   744 | `		}` |
|       - |   745 | `		/* Advance the stream cursor */` |
|   58047 |   746 | `		zIn++;` |
|       5 |   747 | `	}` |
|       - |   748 | `	/* Emit the load constant instruction */` |
|   57933 |   749 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   57933 |   750 | `	if( pStr->nByte < 1024 ){` |
|       - |   751 | `		/* Install in the literal table */` |
|   57933 |   752 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   28964 |   753 | `	}` |
|       - |   754 | `	/* Node successfully compiled */` |
|   57933 |   755 | `	return SXRET_OK;` |
|   50914 |   756 | `}` |
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
|       5 |   776 | `{` |
|     115 |   777 | `	SyString *pIn = &pGen->pIn->sData;` |
|     115 |   778 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - |   779 | `	const char *zPrefix;` |
|       - |   780 | `	const char *z, *zEnd;` |
|       - |   781 | `	char *zBuf, *zDst;` |
|     115 |   782 | `	if( nIndent == 0 ){` |
|       - |   783 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|      69 |   784 | `		*pOut = *pIn;` |
|      69 |   785 | `		return SXRET_OK;` |
|       - |   786 | `	}` |
|       - |   787 | `	/* Recover the marker indent prefix from the original source buffer.` |
|       - |   788 | `	 * Skip the terminator the lexer stripped: one '\n' plus an optional` |
|       - |   789 | `	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the` |
|       - |   790 | `	 * lexer stripped nothing, so this offset is one byte past the true` |
|       - |   791 | `	 * marker-indent start. That is harmless — the strip loop below never` |
|       - |   792 | `	 * runs (z == zEnd), and zPrefix is never dereferenced. */` |
|      48 |   793 | `	zPrefix = pIn->zString + pIn->nByte;` |
|      48 |   794 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|     ! 0 |   795 | `		zPrefix += 2;` |
|     ! 0 |   796 | `	}else{` |
|      48 |   797 | `		zPrefix += 1;` |
|       - |   798 | `	}` |
|       - |   799 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|      48 |   800 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|      48 |   801 | `	if( zBuf == 0 ){` |
|     ! 0 |   802 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   803 | `		return SXERR_ABORT;` |
|       - |   804 | `	}` |
|      48 |   805 | `	zDst = zBuf;` |
|      48 |   806 | `	z = pIn->zString;` |
|      48 |   807 | `	zEnd = z + pIn->nByte;` |
|     130 |   808 | `	while( z < zEnd ){` |
|      72 |   809 | `		const char *zLine = z;` |
|       - |   810 | `		sxu32 nLine;` |
|       - |   811 | `		int bEmpty;` |
|     800 |   812 | `		while( z < zEnd && z[0] != '\n' ){` |
|     732 |   813 | `			z++;` |
|       4 |   814 | `		}` |
|      72 |   815 | `		nLine = (sxu32)(z - zLine);` |
|      72 |   816 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|      72 |   817 | `		if( !bEmpty ){` |
|       - |   818 | `			sxu32 i;` |
|      68 |   819 | `			if( nLine < nIndent ){` |
|     ! 0 |   820 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   821 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|     ! 0 |   822 | `					nIndent);` |
|     ! 0 |   823 | `				return SXERR_ABORT;` |
|       - |   824 | `			}` |
|     270 |   825 | `			for( i = 0; i < nIndent; i++ ){` |
|     214 |   826 | `				if( zLine[i] != zPrefix[i] ){` |
|      11 |   827 | `					unsigned char c = (unsigned char)zLine[i];` |
|      11 |   828 | `					if( c == ' ' \|\| c == '\t' ){` |
|       6 |   829 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   830 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|       4 |   831 | `					}else{` |
|       8 |   832 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   833 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|       2 |   834 | `							nIndent);` |
|       - |   835 | `					}` |
|      11 |   836 | `					return SXERR_ABORT;` |
|       - |   837 | `				}` |
|     104 |   838 | `			}` |
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
|      60 |   853 | `}` |
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
|       4 |   869 | `{` |
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
|      27 |   899 | `}` |
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
|    2264 |   922 | `static sxi32 GenStateProcessStringExpression(` |
|       - |   923 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   924 | `	sxu32 nLine,         /* Line number */` |
|       - |   925 | `	const char *zIn,     /* Raw expression */` |
|       - |   926 | `	const char *zEnd     /* End of the expression */` |
|       - |   927 | `	)` |
|       5 |   928 | `{` |
|       - |   929 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |   930 | `	SySet sToken;` |
|       - |   931 | `	sxi32 rc;` |
|       - |   932 | `	/* Initialize the token set */` |
|    2269 |   933 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |   934 | `	/* Preallocate some slots */` |
|    2269 |   935 | `	SySetAlloc(&sToken,0x08);` |
|       - |   936 | `	/* Tokenize the text */` |
|    2269 |   937 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |   938 | `	/* Swap delimiter */` |
|    2269 |   939 | `	pTmpIn  = pGen->pIn;` |
|    2269 |   940 | `	pTmpEnd = pGen->pEnd;` |
|    2269 |   941 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    2269 |   942 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |   943 | `	/* Compile the expression */` |
|    2269 |   944 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |   945 | `	/* Restore token stream */` |
|    2269 |   946 | `	pGen->pIn  = pTmpIn;` |
|    2269 |   947 | `	pGen->pEnd = pTmpEnd;` |
|       - |   948 | `	/* Release the token set */` |
|    2269 |   949 | `	SySetRelease(&sToken);` |
|       - |   950 | `	/* Compilation result */` |
|    2269 |   951 | `	return rc;` |
|       5 |   952 | `}` |
|       - |   953 | `/*` |
|       - |   954 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |   955 | ` */` |
|   25396 |   956 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       5 |   957 | `{` |
|       - |   958 | `	ph7_value *pConstObj;` |
|   25401 |   959 | `	sxu32 nIdx = 0;` |
|       - |   960 | `	/* Reserve a new constant */` |
|   25401 |   961 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   25401 |   962 | `	if( pConstObj == 0 ){` |
|     ! 0 |   963 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   964 | `		return 0;` |
|       - |   965 | `	}` |
|   25401 |   966 | `	(*pCount)++;` |
|   25401 |   967 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |   968 | `	/* Emit the load constant instruction */` |
|   25401 |   969 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   25401 |   970 | `	return pConstObj;` |
|   12703 |   971 | `}` |
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
|   23916 |  1010 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       5 |  1011 | `{` |
|   23921 |  1012 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  1013 | `	const char *zIn,*zCur,*zEnd;` |
|   23921 |  1014 | `	ph7_value *pObj = 0;` |
|       - |  1015 | `	sxi32 iCons;` |
|       - |  1016 | `	sxi32 rc;` |
|       - |  1017 | `	/* Delimit the string */` |
|   23921 |  1018 | `	zIn  = pStr->zString;` |
|   23921 |  1019 | `	zEnd = &zIn[pStr->nByte];` |
|   23921 |  1020 | `	if( zIn >= zEnd ){` |
|       - |  1021 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  1022 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  1023 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  1024 | `		 */` |
|     319 |  1025 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     319 |  1026 | `		return SXRET_OK;` |
|       - |  1027 | `	}` |
|   23607 |  1028 | `	zCur = 0;` |
|       - |  1029 | `	/* Compile the node */` |
|   23607 |  1030 | `	iCons = 0;` |
|   12933 |  1031 | `	for(;;){` |
|   38611 |  1032 | `		zCur = zIn;` |
|  180643 |  1033 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  144301 |  1034 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      67 |  1035 | `				break;` |
|  144177 |  1036 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    2144 |  1037 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|    1073 |  1038 | `					break;` |
|       - |  1039 | `			}` |
|  142037 |  1040 | `			zIn++;` |
|       5 |  1041 | `		}` |
|   38611 |  1042 | `		if( zIn > zCur ){` |
|   18011 |  1043 | `			if( pObj == 0 ){` |
|   17523 |  1044 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   17523 |  1045 | `				if( pObj == 0 ){` |
|     ! 0 |  1046 | `					return SXERR_ABORT;` |
|       - |  1047 | `				}` |
|    8759 |  1048 | `			}` |
|   18011 |  1049 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    9003 |  1050 | `		}` |
|   38611 |  1051 | `		if( zIn >= zEnd ){` |
|   23607 |  1052 | `			break;` |
|       - |  1053 | `		}` |
|   15009 |  1054 | `		if( zIn[0] == '\\' ){` |
|   12745 |  1055 | `			const char *zPtr = 0;` |
|       - |  1056 | `			sxu32 n;` |
|   12745 |  1057 | `			zIn++;` |
|   12745 |  1058 | `			if( zIn >= zEnd ){` |
|     ! 0 |  1059 | `				break;` |
|       - |  1060 | `			}` |
|   12745 |  1061 | `			if( pObj == 0 ){` |
|    7883 |  1062 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    7883 |  1063 | `				if( pObj == 0 ){` |
|     ! 0 |  1064 | `					return SXERR_ABORT;` |
|       - |  1065 | `				}` |
|    3939 |  1066 | `			}` |
|   12745 |  1067 | `			n = sizeof(char); /* size of conversion */` |
|   12745 |  1068 | `			switch( zIn[0] ){` |
|       7 |  1069 | `			case '$':` |
|       - |  1070 | `				/* Dollar sign */` |
|      15 |  1071 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|      15 |  1072 | `				break;` |
|      56 |  1073 | `			case '\\':` |
|       - |  1074 | `				/* A literal backslash */` |
|     117 |  1075 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|     117 |  1076 | `				break;` |
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
|    5880 |  1089 | `			case 'n':` |
|       - |  1090 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|   11765 |  1091 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|   11765 |  1092 | `				break;` |
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
|   12745 |  1160 | `			zIn += n;` |
|   12745 |  1161 | `			continue;` |
|       - |  1162 | `		}` |
|    2269 |  1163 | `		if( zIn[0] == '{' ){` |
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
|    2141 |  1197 | `			const char *zExpr = zIn;` |
|       - |  1198 | `			/* Assemble variable name */` |
|    1078 |  1199 | `			for(;;){` |
|       - |  1200 | `				/* Jump leading dollars */` |
|    4297 |  1201 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    2141 |  1202 | `					zIn++;` |
|       5 |  1203 | `				}` |
|    1078 |  1204 | `				for(;;){` |
|   11921 |  1205 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    8687 |  1206 | `						zIn++;` |
|       5 |  1207 | `					}` |
|    2161 |  1208 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  1209 | `						/* UTF-8 stream */` |
|     ! 0 |  1210 | `						zIn++;` |
|     ! 0 |  1211 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1212 | `							zIn++;` |
|     ! 0 |  1213 | `						}` |
|     ! 0 |  1214 | `						continue;` |
|       - |  1215 | `					}` |
|    2161 |  1216 | `					break;` |
|     ! 0 |  1217 | `				}` |
|    2161 |  1218 | `				if( zIn >= zEnd ){` |
|     211 |  1219 | `					break;` |
|       - |  1220 | `				}` |
|    1955 |  1221 | `				if( zIn[0] == '[' ){` |
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
|    1945 |  1239 | `				}else if(zIn[0] == '{' ){` |
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
|    1941 |  1257 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  1258 | `					/* Member access operator '->' */` |
|      23 |  1259 | `					zIn += 2;` |
|    1931 |  1260 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  1261 | `					/* Static member access operator '::' */` |
|     ! 0 |  1262 | `					zIn += 2;` |
|     ! 0 |  1263 | `				}else{` |
|     963 |  1264 | `					break;` |
|       - |  1265 | `				}` |
|       3 |  1266 | `			}` |
|       - |  1267 | `			/* Process the expression */` |
|    2141 |  1268 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    2141 |  1269 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1270 | `				return SXERR_ABORT;` |
|       - |  1271 | `			}` |
|    2141 |  1272 | `			if( rc != SXERR_EMPTY ){` |
|    2139 |  1273 | `				++iCons;` |
|    1067 |  1274 | `			}` |
|       - |  1275 | `		}` |
|       - |  1276 | `		/* Invalidate the previously used constant */` |
|    2269 |  1277 | `		pObj = 0;` |
|       5 |  1278 | `	}/*for(;;)*/` |
|   23607 |  1279 | `	if( iCons > 1 ){` |
|       - |  1280 | `		/* Concatenate all compiled constants */` |
|    1681 |  1281 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     838 |  1282 | `	}` |
|       - |  1283 | `	/* Node successfully compiled */` |
|   23607 |  1284 | `	return SXRET_OK;` |
|   11963 |  1285 | `}` |
|       - |  1286 | `/*` |
|       - |  1287 | ` * Compile a double quoted string.` |
|       - |  1288 | ` *  See the block-comment above for more information.` |
|       - |  1289 | ` */` |
|   23856 |  1290 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1291 | `{` |
|       - |  1292 | `	sxi32 rc;` |
|   23861 |  1293 | `	rc = GenStateCompileString(&(*pGen));` |
|   11928 |  1294 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1295 | `	/* Compilation result */` |
|   23861 |  1296 | `	return rc;` |
|       5 |  1297 | `}` |
|       - |  1298 | `/*` |
|       - |  1299 | ` * Compile a Heredoc string.` |
|       - |  1300 | ` *  See the block-comment above for more information.` |
|       - |  1301 | ` */` |
|      64 |  1302 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  1303 | `{` |
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
|      63 |  1314 | `	sOrig = pGen->pIn->sData;` |
|      63 |  1315 | `	pGen->pIn->sData = sStripped;` |
|      63 |  1316 | `	rc = GenStateCompileString(&(*pGen));` |
|      63 |  1317 | `	pGen->pIn->sData = sOrig;` |
|      30 |  1318 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|      63 |  1319 | `	return rc;` |
|      36 |  1320 | `}` |
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
|   22260 |  1340 | `static sxi32 GenStateCompileArrayEntry(` |
|       - |  1341 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  1342 | `	SyToken *pIn,        /* Token stream */` |
|       - |  1343 | `	SyToken *pEnd,       /* End of the token stream */` |
|       - |  1344 | `	sxi32 iFlags,        /* Compilation flags */` |
|       - |  1345 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|       - |  1346 | `	)` |
|       5 |  1347 | `{` |
|       - |  1348 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  1349 | `	sxi32 rc;` |
|       - |  1350 | `	/* Swap token stream */` |
|   22265 |  1351 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - |  1352 | `	/* Compile the expression*/` |
|   22265 |  1353 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - |  1354 | `	/* Restore token stream */` |
|   22265 |  1355 | `	RE_SWAP_DELIMITER(pGen);` |
|   22265 |  1356 | `	return rc;` |
|       5 |  1357 | `}` |
|       - |  1358 | `/*` |
|       - |  1359 | ` * Expression tree validator callback for the 'array' language construct.` |
|       - |  1360 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1361 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1362 | ` * error message.` |
|       - |  1363 | ` * See the routine responible of compiling the array language construct` |
|       - |  1364 | ` * for more inforation.` |
|       - |  1365 | ` */` |
|      36 |  1366 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       4 |  1367 | `{` |
|      40 |  1368 | `	sxi32 rc = SXRET_OK;` |
|      40 |  1369 | `	if( pRoot->pOp ){` |
|      19 |  1370 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|      12 |  1371 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|      16 |  1372 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - |  1373 | `			/* Unexpected expression */` |
|      13 |  1374 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1375 | `				"array(): Expecting a variable/array member/function call after reference operator '&'");` |
|      13 |  1376 | `			if( rc != SXERR_ABORT ){` |
|      13 |  1377 | `				rc = SXERR_INVALID;` |
|       5 |  1378 | `			}` |
|       9 |  1379 | `		}` |
|      31 |  1380 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1381 | `		/* Unexpected expression */` |
|       3 |  1382 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1383 | `			"array(): Expecting a variable after reference operator '&'");` |
|       3 |  1384 | `		if( rc != SXERR_ABORT ){` |
|       3 |  1385 | `			rc = SXERR_INVALID;` |
|       1 |  1386 | `		}` |
|       1 |  1387 | `	}` |
|      40 |  1388 | `	return rc;` |
|       4 |  1389 | `}` |
|       - |  1390 | `/*` |
|       - |  1391 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|       - |  1392 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|       - |  1393 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|       - |  1394 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|       - |  1395 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|       - |  1396 | ` */` |
|   24648 |  1397 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|       5 |  1398 | `{` |
|   24653 |  1399 | `	SyToken *pCur = pStart;` |
|   24653 |  1400 | `	sxi32 iNest = 0;` |
|   69891 |  1401 | `	while( pCur < pEnd ){` |
|   50795 |  1402 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    5553 |  1403 | `			return pCur;` |
|       - |  1404 | `		}` |
|       - |  1405 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - |  1406 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|       - |  1407 | `		 * not an entry separator. Skip past the signature.` |
|       - |  1408 | `		 */` |
|   45247 |  1409 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
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
|   45241 |  1470 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     387 |  1471 | `			iNest++;` |
|   45050 |  1472 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|       - |  1473 | `			/* Don't worry about mismatched brackets here, the expression` |
|       - |  1474 | `			 * parser will shortly detect any syntax error. */` |
|     387 |  1475 | `			iNest--;` |
|     191 |  1476 | `		}` |
|   45241 |  1477 | `		pCur++;` |
|       5 |  1478 | `	}` |
|   19101 |  1479 | `	return pEnd;` |
|   12329 |  1480 | `}` |
|       - |  1481 | `/*` |
|       - |  1482 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - |  1483 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - |  1484 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - |  1485 | ` */` |
|   31810 |  1486 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       5 |  1487 | `{` |
|       - |  1488 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - |  1489 | `	SyToken *pKey,*pCur;` |
|   31815 |  1490 | `	sxi32 iEmitRef = 0;` |
|   31815 |  1491 | `	sxi32 iSpread = 0;` |
|   31815 |  1492 | `	sxi32 nPair = 0;` |
|       - |  1493 | `	sxi32 rc;` |
|   31815 |  1494 | `	xValidator = 0;` |
|   26105 |  1495 | `	for(;;){` |
|       - |  1496 | `		/* Jump leading commas */` |
|   59317 |  1497 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    7107 |  1498 | `			pGen->pIn++;` |
|       5 |  1499 | `		}` |
|   52215 |  1500 | `		pCur = pGen->pIn;` |
|   52215 |  1501 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - |  1502 | `			/* No more entry to process */` |
|   31799 |  1503 | `			break;` |
|       - |  1504 | `		}` |
|   20421 |  1505 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 |  1506 | `			continue;` |
|       - |  1507 | `		}` |
|       - |  1508 | `		/* Compile the key if available */` |
|   20421 |  1509 | `		pKey = pCur;` |
|   20421 |  1510 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   20421 |  1511 | `		rc = SXERR_EMPTY;` |
|   20421 |  1512 | `		if( pCur < pGen->pIn ){` |
|    1661 |  1513 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - |  1514 | `				/* Missing value */` |
|      12 |  1515 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      12 |  1516 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1517 | `					return SXERR_ABORT;` |
|       - |  1518 | `				}` |
|      12 |  1519 | `				return SXRET_OK;` |
|       - |  1520 | `			}` |
|       - |  1521 | `			/* Compile the expression holding the key */` |
|    1651 |  1522 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - |  1523 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1651 |  1524 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1525 | `				return SXERR_ABORT;` |
|       - |  1526 | `			}` |
|    1651 |  1527 | `			pCur++; /* Jump the '=>' operator */` |
|   19588 |  1528 | `		}else if( pKey == pCur ){` |
|       - |  1529 | `			/* Key is omitted,emit a warning */` |
|     ! 0 |  1530 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 |  1531 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 |  1532 | `		}else{` |
|       - |  1533 | `			/* Reset back the cursor and point to the entry value */` |
|   18765 |  1534 | `			pCur = pKey;` |
|       - |  1535 | `		}` |
|   20411 |  1536 | `		if( rc == SXERR_EMPTY ){` |
|       - |  1537 | `			/* No available key,load NULL */` |
|   18767 |  1538 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    9381 |  1539 | `		}` |
|   20411 |  1540 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - |  1541 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      44 |  1542 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      44 |  1543 | `			iEmitRef = 1;` |
|      44 |  1544 | `			pCur++; /* Jump the '&' token */` |
|      44 |  1545 | `			if( pCur >= pGen->pIn ){` |
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
|   20409 |  1559 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   20409 |  1560 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
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
|   20405 |  1573 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   20405 |  1574 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1575 | `			return SXERR_ABORT;` |
|       - |  1576 | `		}` |
|   20405 |  1577 | `		if( iSpread ){` |
|       - |  1578 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|      65 |  1579 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   20374 |  1580 | `		}else if( iEmitRef ){` |
|       - |  1581 | `			/* Emit the load reference instruction */` |
|      40 |  1582 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      18 |  1583 | `		}` |
|   20405 |  1584 | `		xValidator = 0;` |
|   20405 |  1585 | `		iEmitRef = 0;` |
|   20405 |  1586 | `		iSpread = 0;` |
|   20405 |  1587 | `		nPair++;` |
|       5 |  1588 | `	}` |
|       - |  1589 | `	/* Emit the load map instruction */` |
|   31799 |  1590 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - |  1591 | `	/* Node successfully compiled */` |
|   31799 |  1592 | `	return SXRET_OK;` |
|   15910 |  1593 | `}` |
|       - |  1594 | `/*` |
|       - |  1595 | ` * Compile the 'array' language construct.` |
|       - |  1596 | ` *	 According to the PHP language reference manual` |
|       - |  1597 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - |  1598 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - |  1599 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - |  1600 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - |  1601 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - |  1602 | ` */` |
|   30704 |  1603 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1604 | `{` |
|       - |  1605 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   30709 |  1606 | `	pGen->pIn += 2;` |
|   30709 |  1607 | `	pGen->pEnd--;` |
|   15352 |  1608 | `	SXUNUSED(iCompileFlag);` |
|   30709 |  1609 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1610 | `}` |
|       - |  1611 | `/*` |
|       - |  1612 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - |  1613 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - |  1614 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - |  1615 | ` */` |
|    1106 |  1616 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1617 | `{` |
|       - |  1618 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|    1111 |  1619 | `	pGen->pIn++;` |
|    1111 |  1620 | `	pGen->pEnd--;` |
|     553 |  1621 | `	SXUNUSED(iCompileFlag);` |
|    1111 |  1622 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1623 | `}` |
|       - |  1624 | `/*` |
|       - |  1625 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - |  1626 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1627 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1628 | ` * error message.` |
|       - |  1629 | ` * See the routine responible of compiling the list language construct` |
|       - |  1630 | ` * for more inforation.` |
|       - |  1631 | ` */` |
|     172 |  1632 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       4 |  1633 | `{` |
|     176 |  1634 | `	sxi32 rc = SXRET_OK;` |
|     176 |  1635 | `	if( pRoot->pOp ){` |
|       4 |  1636 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|       2 |  1637 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - |  1638 | `				/* Unexpected expression */` |
|     ! 0 |  1639 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1640 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 |  1641 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 |  1642 | `					rc = SXERR_INVALID;` |
|     ! 0 |  1643 | `				}` |
|       1 |  1644 | `		}` |
|     174 |  1645 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1646 | `		/* Unexpected expression */` |
|       6 |  1647 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1648 | `			"list(): Expecting a variable not an expression");` |
|       6 |  1649 | `		if( rc != SXERR_ABORT ){` |
|       6 |  1650 | `			rc = SXERR_INVALID;` |
|       2 |  1651 | `		}` |
|       2 |  1652 | `	}` |
|     176 |  1653 | `	return rc;` |
|       4 |  1654 | `}` |
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
|       2 |  1688 | `{` |
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
|      16 |  1770 | `}` |
|       - |  1771 | `/*` |
|       - |  1772 | ` * Shared body for list() and short list [...] compilation.` |
|       - |  1773 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|       - |  1774 | ` * the opening delimiter and before the closing delimiter.` |
|       - |  1775 | ` */` |
|     108 |  1776 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|       4 |  1777 | `{` |
|       - |  1778 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - |  1779 | `	SyToken *pNext;` |
|       - |  1780 | `	SyToken *pClassifyIn;` |
|     112 |  1781 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|       - |  1782 | `	sxi32 nExpr;` |
|       - |  1783 | `	sxi32 rc;` |
|       - |  1784 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|       - |  1785 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|       - |  1786 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|       - |  1787 | `	 * list. */` |
|     112 |  1788 | `	pClassifyIn = pGen->pIn;` |
|     314 |  1789 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     206 |  1790 | `		if( pGen->pIn >= pNext ){` |
|      13 |  1791 | `			nEmpty++;` |
|     200 |  1792 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|      38 |  1793 | `			nKeyed++;` |
|      20 |  1794 | `		}else{` |
|     158 |  1795 | `			nPositional++;` |
|       - |  1796 | `		}` |
|     206 |  1797 | `		pGen->pIn = &pNext[1];` |
|       4 |  1798 | `	}` |
|     112 |  1799 | `	pGen->pIn = pClassifyIn;` |
|     112 |  1800 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|     ! 0 |  1801 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1802 | `			"Cannot use empty array entries in keyed array assignment");` |
|     ! 0 |  1803 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1804 | `	}` |
|     112 |  1805 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|     ! 0 |  1806 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1807 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|     ! 0 |  1808 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1809 | `	}` |
|     112 |  1810 | `	if( nKeyed > 0 ){` |
|      30 |  1811 | `		return GenStateCompileKeyedListBody(pGen);` |
|       - |  1812 | `	}` |
|      84 |  1813 | `	nExpr = 0;` |
|      84 |  1814 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|     250 |  1815 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     170 |  1816 | `		if( pGen->pIn < pNext ){` |
|       - |  1817 | `			/* Check for nested list() */` |
|     158 |  1818 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
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
|     157 |  1835 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
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
|     144 |  1851 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     144 |  1852 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  1853 | `					SySetRelease(&sNested);` |
|     ! 0 |  1854 | `					return SXRET_OK;` |
|       - |  1855 | `				}` |
|       - |  1856 | `			}` |
|      81 |  1857 | `		}else{` |
|       - |  1858 | `			/* Empty entry,load NULL */` |
|      13 |  1859 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - |  1860 | `		}` |
|     170 |  1861 | `		nExpr++;` |
|       - |  1862 | `		/* Advance the stream cursor */` |
|     170 |  1863 | `		pGen->pIn = &pNext[1];` |
|       4 |  1864 | `	}` |
|       - |  1865 | `	/* Emit the LOAD_LIST instruction */` |
|      84 |  1866 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - |  1867 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - |  1868 | `	 * For each nested entry, emit code to extract the sub-array` |
|       - |  1869 | `	 * at the corresponding index and recursively destructure it.` |
|       - |  1870 | `	 */` |
|      84 |  1871 | `	if( SySetUsed(&sNested) > 0 ){` |
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
|      84 |  1913 | `	SySetRelease(&sNested);` |
|       - |  1914 | `	/* Node successfully compiled */` |
|      84 |  1915 | `	return SXRET_OK;` |
|      58 |  1916 | `}` |
|      34 |  1917 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1918 | `{` |
|       - |  1919 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|      36 |  1920 | `	pGen->pIn += 2;` |
|      36 |  1921 | `	pGen->pEnd--;` |
|      17 |  1922 | `	SXUNUSED(iCompileFlag);` |
|      36 |  1923 | `	return GenStateCompileListBody(pGen);` |
|       2 |  1924 | `}` |
|      74 |  1925 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  1926 | `{` |
|       - |  1927 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      78 |  1928 | `	pGen->pIn++;` |
|      78 |  1929 | `	pGen->pEnd--;` |
|      37 |  1930 | `	SXUNUSED(iCompileFlag);` |
|      78 |  1931 | `	return GenStateCompileListBody(pGen);` |
|       4 |  1932 | `}` |
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
|     294 |  1960 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1961 | `{` |
|       - |  1962 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - |  1963 | `	char zName[512];         /* Unique lambda name */` |
|       - |  1964 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - |  1965 | `							  * one thread is allowed to compile the script.` |
|       - |  1966 | `						      */` |
|       - |  1967 | `	SyString sName;` |
|       - |  1968 | `	sxu32 nLen;` |
|       - |  1969 | `	sxi32 rc;` |
|     147 |  1970 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1971 |  |
|     299 |  1972 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     299 |  1973 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 |  1974 | `		pGen->pIn++;` |
|     ! 0 |  1975 | `	}` |
|       - |  1976 | `	/* Generate a unique name */` |
|     299 |  1977 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - |  1978 | `	/* Make sure the generated name is unique */` |
|     299 |  1979 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  1980 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 |  1981 | `	}` |
|     299 |  1982 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - |  1983 | `	/* Compile the lambda body */` |
|     299 |  1984 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     299 |  1985 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  1986 | `		return SXERR_ABORT;` |
|       - |  1987 | `	}` |
|       - |  1988 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|       - |  1989 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|       - |  1990 | `	 * the handler wraps either in a Closure instance. */` |
|     299 |  1991 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       - |  1992 | `	/* Node successfully compiled */` |
|     299 |  1993 | `	return SXRET_OK;` |
|     152 |  1994 | `}` |
|       - |  1995 | `/*` |
|       - |  1996 | ` * Add a free variable to the arrow function's closure environment, unless` |
|       - |  1997 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|       - |  1998 | ` * enclosing arrow level, or has already been captured.` |
|       - |  1999 | ` */` |
|     184 |  2000 | `static sxi32 GenStateArrowAddCapture(` |
|       - |  2001 | `	ph7_gen_state *pGen,` |
|       - |  2002 | `	ph7_vm_func *pFunc,` |
|       - |  2003 | `	const char *zName,` |
|       - |  2004 | `	sxu32 nByte,` |
|       - |  2005 | `	SyString *aShadow,` |
|       - |  2006 | `	sxu32 nShadow)` |
|       3 |  2007 | `{` |
|       - |  2008 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2009 | `	ph7_vm_func_closure_env *aEnv;` |
|       - |  2010 | `	sxu32 n, nEnv;` |
|       - |  2011 | `	char *zDup;` |
|     187 |  2012 | `	if( nByte == 0 ){` |
|     ! 0 |  2013 | `		return SXRET_OK;` |
|       - |  2014 | `	}` |
|     184 |  2015 | `	if( nByte == sizeof("this")-1` |
|     101 |  2016 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|       3 |  2017 | `		return SXRET_OK;` |
|       - |  2018 | `	}` |
|     233 |  2019 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|     172 |  2020 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|     166 |  2021 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|     127 |  2022 | `			return SXRET_OK;` |
|       - |  2023 | `		}` |
|      26 |  2024 | `	}` |
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
|      95 |  2043 | `}` |
|       - |  2044 | `/*` |
|       - |  2045 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|       - |  2046 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|       - |  2047 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|       - |  2048 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|       - |  2049 | ` */` |
|      36 |  2050 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|       - |  2051 | `	ph7_gen_state *pGen,` |
|       - |  2052 | `	ph7_vm_func *pFunc,` |
|       - |  2053 | `	const char *zIn,` |
|       - |  2054 | `	const char *zEnd,` |
|       - |  2055 | `	SyString *aShadow,` |
|       - |  2056 | `	sxu32 nShadow)` |
|       2 |  2057 | `{` |
|       - |  2058 | `	sxi32 rc;` |
|     302 |  2059 | `	while( zIn < zEnd ){` |
|     266 |  2060 | `		if( zIn[0] == '\\' ){` |
|       5 |  2061 | `			zIn++;` |
|       5 |  2062 | `			if( zIn < zEnd ){` |
|       5 |  2063 | `				zIn++;` |
|       2 |  2064 | `			}` |
|       5 |  2065 | `			continue;` |
|       - |  2066 | `		}` |
|     260 |  2067 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|      22 |  2068 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|      20 |  2069 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|       - |  2070 | `			const char *zName;` |
|      22 |  2071 | `			zIn++; /* skip '$' */` |
|      22 |  2072 | `			zName = zIn;` |
|      74 |  2073 | `			while( zIn < zEnd ){` |
|      70 |  2074 | `				unsigned char c = (unsigned char)zIn[0];` |
|      70 |  2075 | `				if( c >= 0xc0 ){` |
|     ! 0 |  2076 | `					zIn++;` |
|     ! 0 |  2077 | `					while( zIn < zEnd` |
|     ! 0 |  2078 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  2079 | `						zIn++;` |
|     ! 0 |  2080 | `					}` |
|     ! 0 |  2081 | `					continue;` |
|       - |  2082 | `				}` |
|      70 |  2083 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|      18 |  2084 | `					break;` |
|       - |  2085 | `				}` |
|      54 |  2086 | `				zIn++;` |
|       2 |  2087 | `			}` |
|      22 |  2088 | `			if( zIn > zName ){` |
|      32 |  2089 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|      20 |  2090 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|      22 |  2091 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2092 | `					return SXERR_ABORT;` |
|       - |  2093 | `				}` |
|      10 |  2094 | `			}` |
|      22 |  2095 | `			continue;` |
|       - |  2096 | `		}` |
|     242 |  2097 | `		zIn++;` |
|       2 |  2098 | `	}` |
|      38 |  2099 | `	return SXRET_OK;` |
|      20 |  2100 | `}` |
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
|     192 |  2112 | `static sxi32 GenStateArrowCaptureScan(` |
|       - |  2113 | `	ph7_gen_state *pGen,` |
|       - |  2114 | `	ph7_vm_func *pFunc,` |
|       - |  2115 | `	SyToken *pStart,` |
|       - |  2116 | `	SyToken *pEnd,` |
|       - |  2117 | `	SyString *aShadow,` |
|       - |  2118 | `	sxu32 nShadow)` |
|       3 |  2119 | `{` |
|     195 |  2120 | `	SyToken *pScan = pStart;` |
|       - |  2121 | `	sxi32 rc;` |
|     805 |  2122 | `	while( pScan < pEnd ){` |
|     613 |  2123 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|      56 |  2124 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|      18 |  2125 | `				pScan->sData.zString,` |
|      36 |  2126 | `				pScan->sData.zString + pScan->sData.nByte,` |
|      18 |  2127 | `				aShadow,nShadow);` |
|      38 |  2128 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2129 | `				return SXERR_ABORT;` |
|       - |  2130 | `			}` |
|      38 |  2131 | `			pScan++;` |
|      38 |  2132 | `			continue;` |
|       - |  2133 | `		}` |
|     577 |  2134 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|      24 |  2135 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|      24 |  2136 | `			SyToken *pFnKw = pScan;` |
|      22 |  2137 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|     ! 0 |  2138 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|       2 |  2139 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  2140 | `				pFnKw = &pScan[1];` |
|     ! 0 |  2141 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  2142 | `			}` |
|      24 |  2143 | `			if( nKw == PH7_TKWRD_FN ){` |
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
|       2 |  2285 | `		}` |
|     559 |  2286 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     395 |  2287 | `			pScan++;` |
|     395 |  2288 | `			continue;` |
|       - |  2289 | `		}` |
|       - |  2290 | `		{` |
|       - |  2291 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|     167 |  2292 | `			SyToken *pDollar = pScan;` |
|     246 |  2293 | `			while( &pDollar[1] < pEnd` |
|     167 |  2294 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 |  2295 | `				pDollar++;` |
|     ! 0 |  2296 | `			}` |
|     167 |  2297 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 |  2298 | `				break;` |
|       - |  2299 | `			}` |
|     167 |  2300 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2301 | `				pScan = pDollar + 1;` |
|     ! 0 |  2302 | `				continue;` |
|       - |  2303 | `			}` |
|     249 |  2304 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|     164 |  2305 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      82 |  2306 | `				aShadow,nShadow);` |
|     167 |  2307 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2308 | `				return SXERR_ABORT;` |
|       - |  2309 | `			}` |
|     167 |  2310 | `			pScan = pDollar + 2;` |
|       - |  2311 | `		}` |
|       3 |  2312 | `	}` |
|     195 |  2313 | `	return SXRET_OK;` |
|      99 |  2314 | `}` |
|       - |  2315 | `/*` |
|       - |  2316 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  2317 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  2318 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  2319 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  2320 | ` * $this is also made available.` |
|       - |  2321 | ` */` |
|     174 |  2322 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2323 | `{` |
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
|     179 |  2338 | `	sxi32 iFlags = 0;` |
|     179 |  2339 | `	int bStatic = 0;` |
|       - |  2340 | `	sxi32 rc;` |
|       - |  2341 | `	sxu32 n;` |
|      87 |  2342 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2343 |  |
|     179 |  2344 | `	nLine = pGen->pIn->nLine;` |
|       - |  2345 | `	/* Optional 'static' prefix */` |
|     174 |  2346 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     179 |  2347 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       3 |  2348 | `		bStatic = 1;` |
|       3 |  2349 | `		pGen->pIn++;` |
|       1 |  2350 | `	}` |
|       - |  2351 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|     174 |  2352 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     179 |  2353 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  2354 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2355 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  2356 | `		return SXERR_SYNTAX;` |
|       - |  2357 | `	}` |
|     179 |  2358 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  2359 | `	/* Optional '&' — return by reference */` |
|     179 |  2360 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2361 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  2362 | `		pGen->pIn++;` |
|     ! 0 |  2363 | `	}` |
|       - |  2364 | `	/* Expect '(' */` |
|     179 |  2365 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
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
|     177 |  2376 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  2377 | `	/* Delimit the parameter list */` |
|     177 |  2378 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|     177 |  2379 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 |  2380 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2381 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 |  2382 | `		return SXERR_SYNTAX;` |
|       - |  2383 | `	}` |
|       - |  2384 | `	/* Allocate the function state */` |
|     174 |  2385 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|     174 |  2386 | `	if( pFunc == 0 ){` |
|     ! 0 |  2387 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2388 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2389 | `		return SXERR_ABORT;` |
|       - |  2390 | `	}` |
|       - |  2391 | `	/* Generate a unique lambda name */` |
|     174 |  2392 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     268 |  2393 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      96 |  2394 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       2 |  2395 | `	}` |
|     174 |  2396 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|     174 |  2397 | `	if( zDup == 0 ){` |
|     ! 0 |  2398 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2399 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2400 | `		return SXERR_ABORT;` |
|       - |  2401 | `	}` |
|     174 |  2402 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  2403 | `	/* Collect function arguments */` |
|     174 |  2404 | `	if( pGen->pIn < pSigEnd ){` |
|     104 |  2405 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|     104 |  2406 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2407 | `			return SXERR_ABORT;` |
|       - |  2408 | `		}` |
|      50 |  2409 | `	}` |
|       - |  2410 | `	/* Point past ')' and parse optional return type */` |
|     174 |  2411 | `	pGen->pIn = &pSigEnd[1];` |
|     174 |  2412 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|     174 |  2413 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2414 | `		return SXERR_ABORT;` |
|     174 |  2415 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  2416 | `		return SXERR_SYNTAX;` |
|       - |  2417 | `	}` |
|       - |  2418 | `	/* Expect '=>' */` |
|     174 |  2419 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
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
|     171 |  2430 | `	pGen->pIn++; /* Jump '=>' */` |
|     171 |  2431 | `	pBodyStart = pGen->pIn;` |
|     171 |  2432 | `	pBodyEnd = pGen->pEnd;` |
|       - |  2433 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  2434 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  2435 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  2436 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|     171 |  2437 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  2438 | `	{` |
|     171 |  2439 | `		SyString *aShadow = 0;` |
|     171 |  2440 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|     171 |  2441 | `		if( nShadow > 0 ){` |
|     101 |  2442 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      98 |  2443 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|     101 |  2444 | `			if( aShadow == 0 ){` |
|     ! 0 |  2445 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2446 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2447 | `				return SXERR_ABORT;` |
|       - |  2448 | `			}` |
|     225 |  2449 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|     127 |  2450 | `				aShadow[n] = aArgs[n].sName;` |
|      65 |  2451 | `			}` |
|      49 |  2452 | `		}` |
|     255 |  2453 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      84 |  2454 | `			aShadow,nShadow);` |
|     171 |  2455 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2456 | `			return SXERR_ABORT;` |
|       - |  2457 | `		}` |
|       - |  2458 | `	}` |
|       - |  2459 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  2460 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  2461 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  2462 | `	 * $this. */` |
|     171 |  2463 | `	if( !bStatic ){` |
|       - |  2464 | `		char *zThisDup;` |
|     169 |  2465 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|     169 |  2466 | `		if( zThisDup == 0 ){` |
|     ! 0 |  2467 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2468 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2469 | `			return SXERR_ABORT;` |
|       - |  2470 | `		}` |
|     169 |  2471 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     169 |  2472 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|     169 |  2473 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     169 |  2474 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|     169 |  2475 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      83 |  2476 | `	}` |
|       - |  2477 | `	/* Arrow functions are always closures */` |
|     171 |  2478 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       - |  2479 | `	/* Compile the body expression as an implicit return */` |
|     255 |  2480 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      84 |  2481 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|     171 |  2482 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2483 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2484 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  2485 | `		return SXERR_ABORT;` |
|       - |  2486 | `	}` |
|     171 |  2487 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     171 |  2488 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|     171 |  2489 | `	pSavedEnd = pGen->pEnd;` |
|     171 |  2490 | `	pGen->pIn = pBodyStart;` |
|     171 |  2491 | `	pGen->pEnd = pBodyEnd;` |
|     171 |  2492 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     171 |  2493 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2494 | `		return SXERR_ABORT;` |
|       - |  2495 | `	}` |
|       - |  2496 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|       - |  2497 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|       - |  2498 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|       - |  2499 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|     171 |  2500 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     171 |  2501 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     171 |  2502 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     171 |  2503 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     171 |  2504 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  2505 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|     171 |  2506 | `	pGen->pIn = pBodyEnd;` |
|     171 |  2507 | `	pGen->pEnd = pSavedEnd;` |
|       - |  2508 | `	/* Emit the load-closure instruction */` |
|     171 |  2509 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|     171 |  2510 | `	return SXRET_OK;` |
|      92 |  2511 | `}` |
|       - |  2512 | `/*` |
|       - |  2513 | ` * Compile a single arm's expression range into a freshly-allocated` |
|       - |  2514 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|       - |  2515 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|       - |  2516 | ` * expression's value.` |
|       - |  2517 | ` */` |
|     346 |  2518 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|       - |  2519 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|       3 |  2520 | `{` |
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
|     176 |  2559 | `}` |
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
|       1 |  2574 | `{` |
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
|       2 |  2589 | `}` |
|       - |  2590 | `/*` |
|       - |  2591 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|       - |  2592 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|       - |  2593 | ` * Returns the stop token pointer (or pEnd if none found).` |
|       - |  2594 | ` */` |
|     348 |  2595 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|       4 |  2596 | `{` |
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
|     178 |  2610 | `}` |
|      70 |  2611 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2612 | `{` |
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
|      40 |  2751 | `}` |
|       - |  2752 | `/*` |
|       - |  2753 | ` * Compile a backtick quoted string.` |
|       - |  2754 | ` */` |
|       4 |  2755 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2756 | `{` |
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
|       2 |  2769 | `}` |
|       - |  2770 | `/*` |
|       - |  2771 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - |  2772 | ` * construct.` |
|       - |  2773 | ` */` |
|      82 |  2774 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2775 | `{` |
|       - |  2776 | `	SyString *pName;` |
|       - |  2777 | `	sxu32 nKeyID;` |
|       - |  2778 | `	sxi32 rc;` |
|       - |  2779 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      87 |  2780 | `	pName = &pGen->pIn->sData;` |
|      87 |  2781 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      87 |  2782 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      87 |  2783 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
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
|      79 |  2818 | `		sxi32 nArg = 0;` |
|      79 |  2819 | `		sxu32 nIdx = 0;` |
|      79 |  2820 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      79 |  2821 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2822 | `			return SXERR_ABORT;` |
|      79 |  2823 | `		}else if(rc != SXERR_EMPTY ){` |
|      79 |  2824 | `			nArg = 1;` |
|      37 |  2825 | `		}` |
|      79 |  2826 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - |  2827 | `			ph7_value *pObj;` |
|       - |  2828 | `			/* Emit the call instruction */` |
|      31 |  2829 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      31 |  2830 | `			if( pObj == 0 ){` |
|     ! 0 |  2831 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2832 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2833 | `				return SXERR_ABORT;` |
|       - |  2834 | `			}` |
|      31 |  2835 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - |  2836 | `			/* Install in the literal table */` |
|      31 |  2837 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      13 |  2838 | `		}` |
|       - |  2839 | `		/* Emit the call instruction */` |
|      79 |  2840 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      79 |  2841 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       - |  2842 | `	}` |
|       - |  2843 | `	/* Node successfully compiled */` |
|      87 |  2844 | `	return SXRET_OK;` |
|      46 |  2845 | `}` |
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
| 1155780 |  2867 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2868 | `{` |
| 1155785 |  2869 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  2870 | `	sxi32 iVv;` |
|       - |  2871 | `	sxi32 iP1;` |
|       - |  2872 | `	void *p3;` |
|       - |  2873 | `	sxi32 rc;` |
| 1155785 |  2874 | `	iVv = -1; /* Variable variable counter */` |
| 2311577 |  2875 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
| 1155797 |  2876 | `		pGen->pIn++;` |
| 1155797 |  2877 | `		iVv++;` |
|       5 |  2878 | `	}` |
| 1155785 |  2879 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - |  2880 | `		/* Invalid variable name */` |
|     ! 0 |  2881 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 |  2882 | `		if( rc == SXERR_ABORT ){` |
|       - |  2883 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2884 | `			return SXERR_ABORT;` |
|       - |  2885 | `		}` |
|     ! 0 |  2886 | `		return SXRET_OK;` |
|       - |  2887 | `	}` |
| 1155785 |  2888 | `	p3  = 0;` |
| 1155785 |  2889 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
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
| 1155769 |  2909 | `		char *zName = 0;` |
|       - |  2910 | `		/* Extract variable name */` |
| 1155769 |  2911 | `		pName = &pGen->pIn->sData;` |
|       - |  2912 | `		/* Advance the stream cursor */` |
| 1155769 |  2913 | `		pGen->pIn++;` |
| 1155769 |  2914 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
| 1155769 |  2915 | `		if( pEntry == 0 ){` |
|       - |  2916 | `			/* Duplicate name */` |
|  166309 |  2917 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  166309 |  2918 | `			if( zName == 0 ){` |
|     ! 0 |  2919 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2920 | `				return SXERR_ABORT;` |
|       - |  2921 | `			}` |
|       - |  2922 | `			/* Install in the hashtable */` |
|  166309 |  2923 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   83157 |  2924 | `		}else{` |
|       - |  2925 | `			/* Name already available */` |
|  989465 |  2926 | `			zName = (char *)pEntry->pUserData;` |
|       - |  2927 | `		}` |
| 1155769 |  2928 | `		p3 = (void *)zName;` |
|       - |  2929 | `	}` |
| 1155781 |  2930 | `	iP1 = 0;` |
| 1155781 |  2931 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  450977 |  2932 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - |  2933 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  450959 |  2934 | `			iP1 = 1;` |
|  225477 |  2935 | `		}` |
|  225486 |  2936 | `	}` |
|       - |  2937 | `	/* Emit the load instruction */` |
| 1155781 |  2938 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
| 1155793 |  2939 | `	while( iVv > 0 ){` |
|      13 |  2940 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 |  2941 | `		iVv--;` |
|       1 |  2942 | `	}` |
|       - |  2943 | `	/* Node successfully compiled */` |
| 1155781 |  2944 | `	return SXRET_OK;` |
|  577895 |  2945 | `}` |
|       - |  2946 | `/*` |
|       - |  2947 | ` * Load a literal.` |
|       - |  2948 | ` */` |
|  797030 |  2949 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       5 |  2950 | `{` |
|  797035 |  2951 | `	SyToken *pToken = pGen->pIn;` |
|       - |  2952 | `	ph7_value *pObj;` |
|       - |  2953 | `	SyString *pStr;` |
|       - |  2954 | `	sxu32 nIdx;` |
|       - |  2955 | `	/* Extract token value */` |
|  797035 |  2956 | `	pStr = &pToken->sData;` |
|       - |  2957 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  797035 |  2958 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  168947 |  2959 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - |  2960 | `			/* NULL constant are always indexed at 0 */` |
|   62159 |  2961 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   62159 |  2962 | `			return SXRET_OK;` |
|  106793 |  2963 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - |  2964 | `			/* TRUE constant are always indexed at 1 */` |
|     801 |  2965 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     801 |  2966 | `			return SXRET_OK;` |
|       5 |  2967 | `		}` |
|  735073 |  2968 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  107968 |  2969 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - |  2970 | `			/* FALSE constant are always indexed at 2 */` |
|   47647 |  2971 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   47647 |  2972 | `			return SXRET_OK;` |
|  637029 |  2973 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|  113156 |  2974 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - |  2975 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|   10847 |  2976 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   10847 |  2977 | `			if( pObj == 0 ){` |
|     ! 0 |  2978 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2979 | `				return SXERR_ABORT;` |
|       - |  2980 | `			}` |
|   10847 |  2981 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - |  2982 | `			/* Emit the load constant instruction */` |
|   10847 |  2983 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   10847 |  2984 | `			return SXRET_OK;` |
|  587887 |  2985 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   36556 |  2986 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
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
|  587022 |  3002 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   15257 |  3003 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  579389 |  3004 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   19596 |  3005 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
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
|  675585 |  3035 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - |  3036 | `		ph7_value *pLitObj;` |
|       - |  3037 | `		/* Unknown literal,install it in the literal table */` |
|  287789 |  3038 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  287789 |  3039 | `		if( pLitObj == 0 ){` |
|     ! 0 |  3040 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3041 | `			return SXERR_ABORT;` |
|       - |  3042 | `		}` |
|  287789 |  3043 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  287789 |  3044 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  143892 |  3045 | `	}` |
|       - |  3046 | `	/* Emit the load constant instruction */` |
|  675585 |  3047 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  675585 |  3048 | `	return SXRET_OK;` |
|  398520 |  3049 | `}` |
|       - |  3050 | `/*` |
|       - |  3051 | ` * Resolve a namespace path or simply load a literal.` |
|       - |  3052 | ` * If the token stream contains namespace separators (backslashes),` |
|       - |  3053 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - |  3054 | ` * Otherwise, load the simple literal directly.` |
|       - |  3055 | ` */` |
|  800682 |  3056 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       5 |  3057 | `{` |
|       - |  3058 | `	sxi32 rc;` |
|  800687 |  3059 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3060 | `		return SXRET_OK;` |
|       - |  3061 | `	}` |
|       - |  3062 | `	/* Check if this is a multi-token namespace path */` |
|  800687 |  3063 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - |  3064 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|    3657 |  3065 | `		SyBlob *pWorker = &pGen->sWorker;` |
|    3657 |  3066 | `		int isAbsolute = 0;` |
|    3657 |  3067 | `		SyBlobReset(pWorker);` |
|       - |  3068 | `		/* Check for leading backslash (absolute path) */` |
|    3657 |  3069 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|    3655 |  3070 | `			isAbsolute = 1;` |
|    3655 |  3071 | `			pGen->pIn++; /* Skip leading backslash */` |
|    1825 |  3072 | `		}` |
|       - |  3073 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|    3657 |  3074 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 |  3075 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 |  3076 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 |  3077 | `		}` |
|       - |  3078 | `		/* Collect all path components */` |
|    3753 |  3079 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|    3753 |  3080 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      53 |  3081 | `				SyBlobAppend(pWorker,"\\",1);` |
|      29 |  3082 | `			}else{` |
|    3705 |  3083 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  3084 | `			}` |
|    3753 |  3085 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|    3657 |  3086 | `				pGen->pIn++;` |
|    3657 |  3087 | `				break;` |
|       - |  3088 | `			}` |
|     101 |  3089 | `			pGen->pIn++;` |
|       5 |  3090 | `		}` |
|    3657 |  3091 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - |  3092 | `			ph7_value *pObj;` |
|       - |  3093 | `			SyString sPath;` |
|       - |  3094 | `			sxu32 nIdx;` |
|    3657 |  3095 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - |  3096 | `			/* Install in the literal table */` |
|    3657 |  3097 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|    3633 |  3098 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    3633 |  3099 | `				if( pObj == 0 ){` |
|     ! 0 |  3100 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3101 | `					return SXERR_ABORT;` |
|       - |  3102 | `				}` |
|    3633 |  3103 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|    3633 |  3104 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|    1814 |  3105 | `			}` |
|       - |  3106 | `			/* Emit the load constant instruction.` |
|       - |  3107 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|       - |  3108 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|    5483 |  3109 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|    1826 |  3110 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|    1826 |  3111 | `				nIdx,0,0);` |
|    3657 |  3112 | `			return SXRET_OK;` |
|       - |  3113 | `		}` |
|     ! 0 |  3114 | `	}` |
|       - |  3115 | `	/* Single-token literal: load directly */` |
|  797035 |  3116 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  797035 |  3117 | `	return rc;` |
|  400346 |  3118 | `}` |
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
|     ! 0 |  3129 | `{` |
|     ! 0 |  3130 | `	SXUNUSED(iCompileFlag);` |
|     ! 0 |  3131 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|       - |  3132 | `		"Cannot use the first-class callable syntax '...' here");` |
|     ! 0 |  3133 | `	return SXERR_SYNTAX;` |
|     ! 0 |  3134 | `}` |
|  800682 |  3135 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  3136 | `{` |
|       - |  3137 | `	sxi32 rc;` |
|  800687 |  3138 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  800687 |  3139 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3140 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  3141 | `		return rc;` |
|       - |  3142 | `	}` |
|       - |  3143 | `	/* Node successfully compiled */` |
|  800687 |  3144 | `	return SXRET_OK;` |
|  400346 |  3145 | `}` |
|       - |  3146 | `/*` |
|       - |  3147 | ` * Recover from a compile-time error. In other words synchronize` |
|       - |  3148 | ` * the token stream cursor with the first semi-colon seen.` |
|       - |  3149 | ` */` |
|       8 |  3150 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 |  3151 | `{` |
|       - |  3152 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 |  3153 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 |  3154 | `		pGen->pIn++;` |
|       1 |  3155 | `	}` |
|       9 |  3156 | `	return SXRET_OK;` |
|       1 |  3157 | `}` |
|       - |  3158 | `/*` |
|       - |  3159 | ` * Check if the given identifier name is reserved or not.` |
|       - |  3160 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - |  3161 | ` */` |
|     122 |  3162 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       5 |  3163 | `{` |
|     127 |  3164 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      29 |  3165 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 |  3166 | `			return TRUE;` |
|      27 |  3167 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       6 |  3168 | `			return TRUE;` |
|       2 |  3169 | `		}` |
|     111 |  3170 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 |  3171 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 |  3172 | `			return TRUE;` |
|       - |  3173 | `		}` |
|     ! 0 |  3174 | `	}` |
|       - |  3175 | `	/* Not a reserved constant */` |
|     119 |  3176 | `	return FALSE;` |
|      66 |  3177 | `}` |
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
|      34 |  3202 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       5 |  3203 | `{` |
|       - |  3204 | `	SySet *pConsCode,*pInstrContainer;` |
|      39 |  3205 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  3206 | `	SyString *pName;` |
|       - |  3207 | `	sxi32 rc;` |
|      39 |  3208 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      39 |  3209 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  3210 | `		/* Invalid constant name */` |
|       8 |  3211 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       8 |  3212 | `		if( rc == SXERR_ABORT ){` |
|       - |  3213 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3214 | `			return SXERR_ABORT;` |
|       - |  3215 | `		}` |
|       8 |  3216 | `		goto Synchronize;` |
|       - |  3217 | `	}` |
|       - |  3218 | `	/* Peek constant name */` |
|      32 |  3219 | `	pName = &pGen->pIn->sData;` |
|       - |  3220 | `	/* Make sure the constant name isn't reserved */` |
|      32 |  3221 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  3222 | `		/* Reserved constant */` |
|      10 |  3223 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|      10 |  3224 | `		if( rc == SXERR_ABORT ){` |
|       - |  3225 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3226 | `			return SXERR_ABORT;` |
|       - |  3227 | `		}` |
|      10 |  3228 | `		goto Synchronize;` |
|       - |  3229 | `	}` |
|      23 |  3230 | `	pGen->pIn++;` |
|      23 |  3231 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  3232 | `		/* Invalid statement*/` |
|       6 |  3233 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       6 |  3234 | `		if( rc == SXERR_ABORT ){` |
|       - |  3235 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3236 | `			return SXERR_ABORT;` |
|       - |  3237 | `		}` |
|       6 |  3238 | `		goto Synchronize;` |
|       - |  3239 | `	}` |
|      18 |  3240 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - |  3241 | `	/* Allocate a new constant value container */` |
|      18 |  3242 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      18 |  3243 | `	if( pConsCode == 0 ){` |
|     ! 0 |  3244 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3245 | `		return SXERR_ABORT;` |
|       - |  3246 | `	}` |
|      18 |  3247 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  3248 | `	/* Swap bytecode container */` |
|      18 |  3249 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      18 |  3250 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - |  3251 | `	/* Compile constant value */` |
|      18 |  3252 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3253 | `	/* Emit the done instruction */` |
|      18 |  3254 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      18 |  3255 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      18 |  3256 | `	if( rc == SXERR_ABORT ){` |
|       - |  3257 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  3258 | `		return SXERR_ABORT;` |
|       - |  3259 | `	}` |
|      18 |  3260 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - |  3261 | `	/* Register the constant with namespace-qualified name */` |
|       - |  3262 | `	{` |
|       - |  3263 | `		SyBlob sFQN;` |
|       - |  3264 | `		SyString sFQNStr;` |
|      18 |  3265 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      18 |  3266 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      18 |  3267 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      18 |  3268 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      18 |  3269 | `		SyBlobRelease(&sFQN);` |
|       - |  3270 | `	}` |
|      18 |  3271 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3272 | `		SySetRelease(pConsCode);` |
|     ! 0 |  3273 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 |  3274 | `	}` |
|      18 |  3275 | `	return SXRET_OK;` |
|       9 |  3276 | `Synchronize:` |
|       - |  3277 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      60 |  3278 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      41 |  3279 | `		pGen->pIn++;` |
|       3 |  3280 | `	}` |
|      22 |  3281 | `	return SXRET_OK;` |
|      22 |  3282 | `}` |
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
|    3752 |  3305 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       5 |  3306 | `{` |
|    3757 |  3307 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   22025 |  3308 | `	while( pBlock && pBlock != pTarget ){` |
|   18273 |  3309 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
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
|   18273 |  3321 | `		pBlock = pBlock->pParent;` |
|       5 |  3322 | `	}` |
|    3757 |  3323 | `}` |
|    3656 |  3324 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       5 |  3325 | `{` |
|       - |  3326 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3327 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3328 | `	sxu32 nLineLocal;` |
|       - |  3329 | `	sxi32 rc;` |
|    3661 |  3330 | `	nLineLocal = pGen->pIn->nLine;` |
|    3661 |  3331 | `	iLevel = 0;` |
|       - |  3332 | `	/* Jump the 'continue' keyword */` |
|    3661 |  3333 | `	pGen->pIn++;` |
|    3661 |  3334 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
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
|    3661 |  3360 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    3661 |  3361 | `	if( pLoop == 0 ){` |
|       - |  3362 | `		/* Illegal continue */` |
|      13 |  3363 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      13 |  3364 | `		if( rc == SXERR_ABORT ){` |
|       - |  3365 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3366 | `			return SXERR_ABORT;` |
|       - |  3367 | `		}` |
|       8 |  3368 | `	}else{` |
|    3651 |  3369 | `		sxu32 nInstrIdx = 0;` |
|       - |  3370 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    3651 |  3371 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    3651 |  3372 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
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
|    3647 |  3384 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    3647 |  3385 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  3386 | `				JumpFixup sJumpFix;` |
|       - |  3387 | `				/* Post-continue */` |
|      14 |  3388 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 |  3389 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 |  3390 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 |  3391 | `			}` |
|       - |  3392 | `		}` |
|       - |  3393 | `	}` |
|    3661 |  3394 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3395 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3396 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  3397 | `	}` |
|       - |  3398 | `	/* Statement successfully compiled */` |
|    3661 |  3399 | `	return SXRET_OK;` |
|    1833 |  3400 | `}` |
|       - |  3401 | `/*` |
|       - |  3402 | ` * Compile the 'break' statement.` |
|       - |  3403 | ` * According to the PHP language reference` |
|       - |  3404 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - |  3405 | ` *  structure.` |
|       - |  3406 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - |  3407 | ` *  enclosing structures are to be broken out of.` |
|       - |  3408 | ` */` |
|     122 |  3409 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       5 |  3410 | `{` |
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
|      18 |  3422 | `		char *zAlloc = 0;` |
|       - |  3423 | `		SyString sNum;` |
|      18 |  3424 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      18 |  3425 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3426 | `			return SXERR_ABORT;` |
|       - |  3427 | `		}` |
|      18 |  3428 | `		if( rc == SXRET_OK ){` |
|      21 |  3429 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3430 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      15 |  3431 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3432 | `				return SXERR_ABORT;` |
|       - |  3433 | `			}` |
|      15 |  3434 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      15 |  3435 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3436 | `		}` |
|      18 |  3437 | `		if( iLevel < 2 ){` |
|       3 |  3438 | `			iLevel = 0;` |
|       1 |  3439 | `		}` |
|      18 |  3440 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3441 | `	}` |
|       - |  3442 | `	/* Extract the target loop */` |
|     127 |  3443 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     127 |  3444 | `	if( pLoop == 0 ){` |
|       - |  3445 | `		/* Illegal break */` |
|      19 |  3446 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      19 |  3447 | `		if( rc == SXERR_ABORT ){` |
|       - |  3448 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3449 | `			return SXERR_ABORT;` |
|       - |  3450 | `		}` |
|      11 |  3451 | `	}else{` |
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
|      66 |  3467 | `}` |
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
|       5 |  3478 | `{` |
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
|      61 |  3522 | `}` |
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
|       5 |  3538 | `{` |
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
|       5 |  3552 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       5 |  3553 | `		if( rc == SXERR_ABORT ){` |
|       - |  3554 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3555 | `			return SXERR_ABORT;` |
|       - |  3556 | `		}` |
|       3 |  3557 | `	}else{` |
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
|       9 |  3580 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       9 |  3581 | `			if( rc == SXERR_ABORT ){` |
|       - |  3582 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3583 | `				return SXERR_ABORT;` |
|       - |  3584 | `			}` |
|       3 |  3585 | `		}` |
|     153 |  3586 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      29 |  3587 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      16 |  3588 | `		}else{` |
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
|      81 |  3602 | `}` |
|       - |  3603 | `/*` |
|       - |  3604 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - |  3605 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - |  3606 | ` * failure.` |
|       - |  3607 | ` */` |
|      20 |  3608 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       2 |  3609 | `{` |
|       - |  3610 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - |  3611 | `	sxu32 nRawObj;` |
|      10 |  3612 | `	sxu32 nObjIdx;` |
|       - |  3613 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - |  3614 | `	 * a PHP block.` |
|       - |  3615 | `	 */` |
|      10 |  3616 | `Consume:` |
|      22 |  3617 | `	nRawObj = nObjIdx = 0;` |
|      22 |  3618 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
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
|      22 |  3630 | `	if( nRawObj > 0 ){` |
|       - |  3631 | `		/* Emit the consume instruction */` |
|     ! 0 |  3632 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 |  3633 | `	}` |
|      22 |  3634 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
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
|      22 |  3664 | `		pGen->pIn = pGen->pEnd;` |
|      22 |  3665 | `		return SXERR_EOF;` |
|       - |  3666 | `	}` |
|     ! 0 |  3667 | `	return SXRET_OK;` |
|      12 |  3668 | `}` |
|       - |  3669 | `/*` |
|       - |  3670 | ` * Compile a PHP block.` |
|       - |  3671 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - |  3672 | ` * optionally delimited by braces {}.` |
|       - |  3673 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  3674 | ` * and this function takes care of generating the appropriate error` |
|       - |  3675 | ` * message.` |
|       - |  3676 | ` */` |
|  438722 |  3677 | `static sxi32 PH7_CompileBlock(` |
|       - |  3678 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  3679 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  3680 | `	)` |
|       5 |  3681 | `{` |
|       - |  3682 | `	sxi32 rc;` |
|       - |  3683 | `	sxu32 nLine;` |
|  438727 |  3684 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  437033 |  3685 | `		nLine = pGen->pIn->nLine;` |
|  437033 |  3686 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  437033 |  3687 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3688 | `			return SXERR_ABORT;` |
|       - |  3689 | `		}` |
|  437033 |  3690 | `		pGen->pIn++;` |
|       - |  3691 | `		/* Compile until we hit the closing braces '}' */` |
|  598524 |  3692 | `		for(;;){` |
| 1197053 |  3693 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      22 |  3694 | `				rc = GenStateNextChunk(&(*pGen));` |
|      22 |  3695 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3696 | `			 	   return SXERR_ABORT;` |
|       - |  3697 | `				}` |
|      22 |  3698 | `				if( rc == SXERR_EOF ){` |
|       - |  3699 | `					/* No more token to process. Missing closing braces */` |
|      22 |  3700 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      22 |  3701 | `					break;` |
|       - |  3702 | `				}` |
|     ! 0 |  3703 | `			}` |
| 1197033 |  3704 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  3705 | `				/* Closing braces found,break immediately*/` |
|  437013 |  3706 | `				pGen->pIn++;` |
|  437013 |  3707 | `				break;` |
|       - |  3708 | `			}` |
|       - |  3709 | `			/* Compile a single statement */` |
|  760025 |  3710 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  760025 |  3711 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3712 | `				return SXERR_ABORT;` |
|       - |  3713 | `			}` |
|       5 |  3714 | `		}` |
|  437033 |  3715 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  220213 |  3716 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
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
|    1699 |  3760 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1699 |  3761 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3762 | `			return SXERR_ABORT;` |
|       - |  3763 | `		}` |
|       - |  3764 | `	}` |
|       - |  3765 | `	/* Jump trailing semi-colons ';' */` |
|  438727 |  3766 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3767 | `		pGen->pIn++;` |
|     ! 0 |  3768 | `	}` |
|  438727 |  3769 | `	return SXRET_OK;` |
|  219366 |  3770 | `}` |
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
|   14568 |  3790 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       5 |  3791 | `{` |
|   14573 |  3792 | `	GenBlock *pWhileBlock = 0;` |
|   14573 |  3793 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  3794 | `	sxu32 nFalseJump;` |
|       - |  3795 | `	sxu32 nLine;` |
|       - |  3796 | `	sxi32 rc;` |
|   14573 |  3797 | `	nLine = pGen->pIn->nLine;` |
|       - |  3798 | `	/* Jump the 'while' keyword */` |
|   14573 |  3799 | `	pGen->pIn++;` |
|   14573 |  3800 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3801 | `		/* Syntax error */` |
|     ! 0 |  3802 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3803 | `		if( rc == SXERR_ABORT ){` |
|       - |  3804 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3805 | `			return SXERR_ABORT;` |
|       - |  3806 | `		}` |
|     ! 0 |  3807 | `		goto Synchronize;` |
|       - |  3808 | `	}` |
|       - |  3809 | `	/* Jump the left parenthesis '(' */` |
|   14573 |  3810 | `	pGen->pIn++;` |
|       - |  3811 | `	/* Create the loop block */` |
|   14573 |  3812 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   14573 |  3813 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3814 | `		return SXERR_ABORT;` |
|       - |  3815 | `	}` |
|       - |  3816 | `	/* Delimit the condition */` |
|   14573 |  3817 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   14573 |  3818 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3819 | `		/* Empty expression */` |
|       3 |  3820 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 |  3821 | `		if( rc == SXERR_ABORT ){` |
|       - |  3822 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3823 | `			return SXERR_ABORT;` |
|       - |  3824 | `		}` |
|       1 |  3825 | `	}` |
|       - |  3826 | `	/* Swap token streams */` |
|   14573 |  3827 | `	pTmp = pGen->pEnd;` |
|   14573 |  3828 | `	pGen->pEnd = pEnd;` |
|       - |  3829 | `	/* Compile the expression */` |
|   14573 |  3830 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14573 |  3831 | `	if( rc == SXERR_ABORT ){` |
|       - |  3832 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3833 | `		return SXERR_ABORT;` |
|       - |  3834 | `	}` |
|       - |  3835 | `	/* Update token stream */` |
|   14573 |  3836 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3837 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3838 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3839 | `			return SXERR_ABORT;` |
|       - |  3840 | `		}` |
|     ! 0 |  3841 | `		pGen->pIn++;` |
|     ! 0 |  3842 | `	}` |
|       - |  3843 | `	/* Synchronize pointers */` |
|   14573 |  3844 | `	pGen->pIn  = &pEnd[1];` |
|   14573 |  3845 | `	pGen->pEnd = pTmp;` |
|       - |  3846 | `	/* Emit the false jump */` |
|   14573 |  3847 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3848 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   14573 |  3849 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  3850 | `	/* Compile the loop body */` |
|   14573 |  3851 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   14573 |  3852 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3853 | `		return SXERR_ABORT;` |
|       - |  3854 | `	}` |
|       - |  3855 | `	/* Emit the unconditional jump to the start of the loop */` |
|   14573 |  3856 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  3857 | `	/* Fix all jumps now the destination is resolved */` |
|   14573 |  3858 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3859 | `	/* Release the loop block */` |
|   14573 |  3860 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3861 | `	/* Statement successfully compiled */` |
|   14573 |  3862 | `	return SXRET_OK;` |
|     ! 0 |  3863 | `Synchronize:` |
|       - |  3864 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3865 | `	 * compiling this erroneous block.` |
|       - |  3866 | `	 */` |
|     ! 0 |  3867 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3868 | `		pGen->pIn++;` |
|     ! 0 |  3869 | `	}` |
|     ! 0 |  3870 | `	return SXRET_OK;` |
|    7289 |  3871 | `}` |
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
|       1 |  3891 | `{` |
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
|       2 |  3998 | `}` |
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
|   14568 |  4019 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       5 |  4020 | `{` |
|   14573 |  4021 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   14573 |  4022 | `	GenBlock *pForBlock = 0;` |
|       - |  4023 | `	sxu32 nFalseJump;` |
|       - |  4024 | `	sxu32 nLine;` |
|       - |  4025 | `	sxi32 rc;` |
|   14573 |  4026 | `	nLine = pGen->pIn->nLine;` |
|       - |  4027 | `	/* Jump the 'for' keyword */` |
|   14573 |  4028 | `	pGen->pIn++;` |
|   14573 |  4029 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4030 | `		/* Syntax error */` |
|     ! 0 |  4031 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 |  4032 | `		if( rc == SXERR_ABORT ){` |
|       - |  4033 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4034 | `			return SXERR_ABORT;` |
|       - |  4035 | `		}` |
|     ! 0 |  4036 | `		return SXRET_OK;` |
|       - |  4037 | `	}` |
|       - |  4038 | `	/* Jump the left parenthesis '(' */` |
|   14573 |  4039 | `	pGen->pIn++;` |
|       - |  4040 | `	/* Delimit the init-expr;condition;post-expr */` |
|   14573 |  4041 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   14573 |  4042 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
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
|   14573 |  4057 | `	pTmp = pGen->pEnd;` |
|   14573 |  4058 | `	pGen->pEnd = pEnd;` |
|       - |  4059 | `	/* Compile initialization expressions if available */` |
|   14573 |  4060 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4061 | `	/* Pop operand lvalues */` |
|   14573 |  4062 | `	if( rc == SXERR_ABORT ){` |
|       - |  4063 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4064 | `		return SXERR_ABORT;` |
|   14573 |  4065 | `	}else if( rc != SXERR_EMPTY ){` |
|   14571 |  4066 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    7283 |  4067 | `	}` |
|   14573 |  4068 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
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
|   14573 |  4079 | `	pGen->pIn++;` |
|       - |  4080 | `	/* Create the loop block */` |
|   14573 |  4081 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   14573 |  4082 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4083 | `		return SXERR_ABORT;` |
|       - |  4084 | `	}` |
|       - |  4085 | `	/* Deffer continue jumps */` |
|   14573 |  4086 | `	pForBlock->bPostContinue = TRUE;` |
|       - |  4087 | `	/* Compile the condition */` |
|   14573 |  4088 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14573 |  4089 | `	if( rc == SXERR_ABORT ){` |
|       - |  4090 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4091 | `		return SXERR_ABORT;` |
|   14573 |  4092 | `	}else if( rc != SXERR_EMPTY ){` |
|       - |  4093 | `		/* Emit the false jump */` |
|   14571 |  4094 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  4095 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   14571 |  4096 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    7283 |  4097 | `	}` |
|   14573 |  4098 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
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
|   14569 |  4109 | `	pGen->pIn++;` |
|       - |  4110 | `	/* Save the post condition stream */` |
|   14569 |  4111 | `	pPostStart = pGen->pIn;` |
|       - |  4112 | `	/* Compile the loop body */` |
|   14569 |  4113 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   14569 |  4114 | `	pGen->pEnd = pTmp;` |
|   14569 |  4115 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   14569 |  4116 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  4117 | `		return SXERR_ABORT;` |
|       - |  4118 | `	}` |
|       - |  4119 | `	/* Fix post-continue jumps */` |
|   14569 |  4120 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
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
|   14569 |  4136 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  4137 | `		pPostStart++;` |
|     ! 0 |  4138 | `	}` |
|   14569 |  4139 | `	if( pPostStart < pEnd ){` |
|       - |  4140 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   14569 |  4141 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   14569 |  4142 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14569 |  4143 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - |  4144 | `			/* Syntax error */` |
|     ! 0 |  4145 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 |  4146 | `			if( rc == SXERR_ABORT ){` |
|       - |  4147 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4148 | `				return SXERR_ABORT;` |
|       - |  4149 | `			}` |
|     ! 0 |  4150 | `			return SXRET_OK;` |
|       - |  4151 | `		}` |
|   14569 |  4152 | `		RE_SWAP_DELIMITER(pGen);` |
|   14569 |  4153 | `		if( rc == SXERR_ABORT ){` |
|       - |  4154 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4155 | `			return SXERR_ABORT;` |
|   14569 |  4156 | `		}else if( rc != SXERR_EMPTY){` |
|       - |  4157 | `			/* Pop operand lvalue */` |
|   14569 |  4158 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    7282 |  4159 | `		}` |
|    7282 |  4160 | `	}` |
|       - |  4161 | `	/* Emit the unconditional jump to the start of the loop */` |
|   14569 |  4162 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - |  4163 | `	/* Fix all jumps now the destination is resolved */` |
|   14569 |  4164 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4165 | `	/* Release the loop block */` |
|   14569 |  4166 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4167 | `	/* Statement successfully compiled */` |
|   14569 |  4168 | `	return SXRET_OK;` |
|    7289 |  4169 | `}` |
|       - |  4170 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - |  4171 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - |  4172 | ` * are allowed.` |
|       - |  4173 | ` */` |
|    7810 |  4174 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  4175 | `{` |
|    7815 |  4176 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    7815 |  4177 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  4178 | `		/* Unexpected expression */` |
|     ! 0 |  4179 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  4180 | `			"foreach: Expecting a variable name");` |
|     ! 0 |  4181 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  4182 | `			rc = SXERR_INVALID;` |
|     ! 0 |  4183 | `		}` |
|     ! 0 |  4184 | `	}` |
|    7815 |  4185 | `	return rc;` |
|       5 |  4186 | `}` |
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
|    4006 |  4213 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       5 |  4214 | `{` |
|    4011 |  4215 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    4011 |  4216 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    4011 |  4217 | `	GenBlock *pForeachBlock = 0;` |
|       - |  4218 | `	ph7_foreach_info *pInfo;` |
|       - |  4219 | `	sxu32 nFalseJump;` |
|       - |  4220 | `	VmInstr *pInstr;` |
|       - |  4221 | `	sxu32 nLine;` |
|       - |  4222 | `	sxi32 rc;` |
|    4011 |  4223 | `	nLine = pGen->pIn->nLine;` |
|       - |  4224 | `	/* Jump the 'foreach' keyword */` |
|    4011 |  4225 | `	pGen->pIn++;` |
|    4011 |  4226 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4227 | `		/* Syntax error */` |
|     ! 0 |  4228 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 |  4229 | `		if( rc == SXERR_ABORT ){` |
|       - |  4230 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4231 | `			return SXERR_ABORT;` |
|       - |  4232 | `		}` |
|     ! 0 |  4233 | `		goto Synchronize;` |
|       - |  4234 | `	}` |
|       - |  4235 | `	/* Jump the left parenthesis '(' */` |
|    4011 |  4236 | `	pGen->pIn++;` |
|       - |  4237 | `	/* Create the loop block */` |
|    4011 |  4238 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    4011 |  4239 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4240 | `		return SXERR_ABORT;` |
|       - |  4241 | `	}` |
|       - |  4242 | `	/* Delimit the expression */` |
|    4011 |  4243 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    4011 |  4244 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
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
|    4011 |  4259 | `	pCur = pGen->pIn;` |
|   27517 |  4260 | `	while( pCur < pEnd ){` |
|   27517 |  4261 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    4025 |  4262 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    4025 |  4263 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - |  4264 | `				/* Break with the first 'as' found */` |
|    4011 |  4265 | `				break;` |
|       - |  4266 | `			}` |
|       7 |  4267 | `		}` |
|       - |  4268 | `		/* Advance the stream cursor */` |
|   23511 |  4269 | `		pCur++;` |
|       5 |  4270 | `	}` |
|    4011 |  4271 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 |  4272 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  4273 | `			"foreach: Missing array/object expression");` |
|     ! 0 |  4274 | `		if( rc == SXERR_ABORT ){` |
|       - |  4275 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4276 | `			return SXERR_ABORT;` |
|       - |  4277 | `		}` |
|     ! 0 |  4278 | `		goto Synchronize;` |
|       - |  4279 | `	}` |
|       - |  4280 | `	/* Swap token streams */` |
|    4011 |  4281 | `	pTmp = pGen->pEnd;` |
|    4011 |  4282 | `	pGen->pEnd = pCur;` |
|    4011 |  4283 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    4011 |  4284 | `	if( rc == SXERR_ABORT ){` |
|       - |  4285 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4286 | `		return SXERR_ABORT;` |
|       - |  4287 | `	}` |
|       - |  4288 | `	/* Update token stream */` |
|    4011 |  4289 | `	while(pGen->pIn < pCur ){` |
|     ! 0 |  4290 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4291 | `		if( rc == SXERR_ABORT ){` |
|       - |  4292 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4293 | `			return SXERR_ABORT;` |
|       - |  4294 | `		}` |
|     ! 0 |  4295 | `		pGen->pIn++;` |
|     ! 0 |  4296 | `	}` |
|    4011 |  4297 | `	pCur++; /* Jump the 'as' keyword */` |
|    4011 |  4298 | `	pGen->pIn = pCur;` |
|    4011 |  4299 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4300 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 |  4301 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4302 | `			return SXERR_ABORT;` |
|       - |  4303 | `		}` |
|     ! 0 |  4304 | `	}` |
|       - |  4305 | `	/* Create the foreach context */` |
|    4011 |  4306 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    4011 |  4307 | `	if( pInfo == 0 ){` |
|     ! 0 |  4308 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  4309 | `		return SXERR_ABORT;` |
|       - |  4310 | `	}` |
|       - |  4311 | `	/* Zero the structure */` |
|    4011 |  4312 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - |  4313 | `	/* Initialize structure fields */` |
|    4011 |  4314 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - |  4315 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|       - |  4316 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|       - |  4317 | `	 * '=>'. */` |
|    4011 |  4318 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    4011 |  4319 | `	if( pCur < pEnd ){` |
|       - |  4320 | `		/* Compile the expression holding the key name */` |
|    3825 |  4321 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 |  4322 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 |  4323 | `			if( rc == SXERR_ABORT ){` |
|       - |  4324 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4325 | `				return SXERR_ABORT;` |
|       - |  4326 | `			}` |
|     ! 0 |  4327 | `		}else{` |
|    3825 |  4328 | `			pGen->pEnd = pCur;` |
|    3825 |  4329 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3825 |  4330 | `			if( rc == SXERR_ABORT ){` |
|       - |  4331 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4332 | `				return SXERR_ABORT;` |
|       - |  4333 | `			}` |
|    3825 |  4334 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3825 |  4335 | `			if( pInstr->p3 ){` |
|       - |  4336 | `				/* Record key name */` |
|    3825 |  4337 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1910 |  4338 | `			}` |
|    3825 |  4339 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - |  4340 | `		}` |
|    3825 |  4341 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1910 |  4342 | `	}` |
|    4011 |  4343 | `	pGen->pEnd = pEnd;` |
|    4011 |  4344 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4345 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 |  4346 | `		if( rc == SXERR_ABORT ){` |
|       - |  4347 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4348 | `			return SXERR_ABORT;` |
|       - |  4349 | `		}` |
|     ! 0 |  4350 | `		goto Synchronize;` |
|       - |  4351 | `	}` |
|    4011 |  4352 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 |  4353 | `		pGen->pIn++;` |
|       - |  4354 | `		/* Pass by reference  */` |
|      11 |  4355 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 |  4356 | `	}` |
|       - |  4357 | `	/* Check if the value target is list() */` |
|    4011 |  4358 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
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
|    4006 |  4399 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  4400 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - |  4401 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - |  4402 | `		 */` |
|       - |  4403 | `		static int iForeachShortListCnt = 0;` |
|       - |  4404 | `		char zTmp[128];` |
|       - |  4405 | `		sxu32 nLen;` |
|       - |  4406 | `		char *zDup;` |
|       9 |  4407 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       9 |  4408 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       9 |  4409 | `		if( zDup == 0 ){` |
|     ! 0 |  4410 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4411 | `			return SXERR_ABORT;` |
|       - |  4412 | `		}` |
|       9 |  4413 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4414 | `		/* Save [...] token boundaries */` |
|       9 |  4415 | `		pListStart = pGen->pIn;` |
|       - |  4416 | `		/* Advance past [...] */` |
|       9 |  4417 | `		pGen->pIn++; /* Jump '[' */` |
|       9 |  4418 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       9 |  4419 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4420 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4421 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 |  4422 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4423 | `				return SXERR_ABORT;` |
|       - |  4424 | `			}` |
|     ! 0 |  4425 | `			goto Synchronize;` |
|       - |  4426 | `		}` |
|       9 |  4427 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       9 |  4428 | `		pListEnd = pGen->pIn;` |
|       9 |  4429 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       5 |  4430 | `	}else{` |
|       - |  4431 | `		/* Compile the expression holding the value name */` |
|    3995 |  4432 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3995 |  4433 | `		if( rc == SXERR_ABORT ){` |
|       - |  4434 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4435 | `			return SXERR_ABORT;` |
|       - |  4436 | `		}` |
|    3995 |  4437 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3995 |  4438 | `		if( pInstr->p3 ){` |
|       - |  4439 | `			/* Record value name */` |
|    3995 |  4440 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1995 |  4441 | `		}` |
|       - |  4442 | `	}` |
|       - |  4443 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    4009 |  4444 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - |  4445 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    4009 |  4446 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - |  4447 | `	/* Record the first instruction to execute */` |
|    4009 |  4448 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4449 | `	/* Emit the FOREACH_STEP instruction */` |
|    4009 |  4450 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - |  4451 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    4009 |  4452 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - |  4453 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    4009 |  4454 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - |  4455 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - |  4456 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - |  4457 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - |  4458 | `		 */` |
|      15 |  4459 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - |  4460 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - |  4461 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - |  4462 | `		 * picks up the delimiter and the variable names inside.` |
|       - |  4463 | `		 */` |
|      15 |  4464 | `		pSavedIn = pGen->pIn;` |
|      15 |  4465 | `		pSavedEnd = pGen->pEnd;` |
|      15 |  4466 | `		pGen->pIn = pListStart;` |
|      15 |  4467 | `		pGen->pEnd = pListEnd;` |
|      15 |  4468 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       9 |  4469 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       5 |  4470 | `		}else{` |
|       7 |  4471 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - |  4472 | `		}` |
|      15 |  4473 | `		pGen->pIn = pSavedIn;` |
|      15 |  4474 | `		pGen->pEnd = pSavedEnd;` |
|      15 |  4475 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4476 | `			return SXERR_ABORT;` |
|       - |  4477 | `		}` |
|       - |  4478 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|      15 |  4479 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       7 |  4480 | `	}` |
|       - |  4481 | `	/* Compile the loop body */` |
|    4009 |  4482 | `	pGen->pIn = &pEnd[1];` |
|    4009 |  4483 | `	pGen->pEnd = pTmp;` |
|    4009 |  4484 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    4009 |  4485 | `	if( rc == SXERR_ABORT ){` |
|       - |  4486 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4487 | `		return SXERR_ABORT;` |
|       - |  4488 | `	}` |
|       - |  4489 | `	/* Emit the unconditional jump to the start of the loop */` |
|    4009 |  4490 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - |  4491 | `	/* Fix all jumps now the destination is resolved */` |
|    4009 |  4492 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4493 | `	/* Release the loop block */` |
|    4009 |  4494 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4495 | `	/* Statement successfully compiled */` |
|    4009 |  4496 | `	return SXRET_OK;` |
|       1 |  4497 | `Synchronize:` |
|       - |  4498 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4499 | `	 * compiling this erroneous block.` |
|       - |  4500 | `	 */` |
|       3 |  4501 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4502 | `		pGen->pIn++;` |
|     ! 0 |  4503 | `	}` |
|       3 |  4504 | `	return SXRET_OK;` |
|    2008 |  4505 | `}` |
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
|  151382 |  4538 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       5 |  4539 | `{` |
|  151387 |  4540 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  151387 |  4541 | `	GenBlock *pCondBlock = 0;` |
|       - |  4542 | `	sxu32 nJumpIdx;` |
|       - |  4543 | `	sxu32 nKeyID;` |
|       - |  4544 | `	sxi32 rc;` |
|       - |  4545 | `	/* Jump the 'if' keyword */` |
|  151387 |  4546 | `	pGen->pIn++;` |
|  151387 |  4547 | `	pToken = pGen->pIn;` |
|       - |  4548 | `	/* Create the conditional block */` |
|  151387 |  4549 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  151387 |  4550 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4551 | `		return SXERR_ABORT;` |
|       - |  4552 | `	}` |
|       - |  4553 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   82972 |  4554 | `	for(;;){` |
|  165949 |  4555 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
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
|  165949 |  4568 | `		pToken++;` |
|       - |  4569 | `		/* Delimit the condition */` |
|  165949 |  4570 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  165949 |  4571 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
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
|  165949 |  4584 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - |  4585 | `		/* Compile the condition */` |
|  165949 |  4586 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4587 | `		/* Update token stream */` |
|  165949 |  4588 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 |  4589 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4590 | `			pGen->pIn++;` |
|     ! 0 |  4591 | `		}` |
|  165949 |  4592 | `		pGen->pIn  = &pEnd[1];` |
|  165949 |  4593 | `		pGen->pEnd = pTmp;` |
|  165949 |  4594 | `		if( rc == SXERR_ABORT ){` |
|       - |  4595 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4596 | `			return SXERR_ABORT;` |
|       - |  4597 | `		}` |
|       - |  4598 | `		/* Emit the false jump */` |
|  165949 |  4599 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - |  4600 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  165949 |  4601 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - |  4602 | `		/* Compile the body */` |
|  165949 |  4603 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  165949 |  4604 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4605 | `			return SXERR_ABORT;` |
|       - |  4606 | `		}` |
|  165949 |  4607 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   46259 |  4608 | `			break;` |
|       - |  4609 | `		}` |
|       - |  4610 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   73441 |  4611 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   73441 |  4612 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   47279 |  4613 | `			break;` |
|       - |  4614 | `		}` |
|       - |  4615 | `		/* Emit the unconditional jump */` |
|   26167 |  4616 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - |  4617 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   26167 |  4618 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   26167 |  4619 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   18829 |  4620 | `			pToken = &pGen->pIn[1];` |
|   18829 |  4621 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    7276 |  4622 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    5805 |  4623 | `					break;` |
|       - |  4624 | `			}` |
|    7229 |  4625 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    3612 |  4626 | `		}` |
|   14567 |  4627 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - |  4628 | `		/* Synchronize cursors */` |
|   14567 |  4629 | `		pToken = pGen->pIn;` |
|       - |  4630 | `		/* Fix the false jump */` |
|   14567 |  4631 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       5 |  4632 | `	} /* For(;;) */` |
|       - |  4633 | `	/* Fix the false jump */` |
|  151387 |  4634 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  151387 |  4635 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   58874 |  4636 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - |  4637 | `			/* Compile the else block */` |
|   11605 |  4638 | `			pGen->pIn++;` |
|   11605 |  4639 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   11605 |  4640 | `			if( rc == SXERR_ABORT ){` |
|       - |  4641 |  |
|     ! 0 |  4642 | `				return SXERR_ABORT;` |
|       - |  4643 | `			}` |
|    5800 |  4644 | `	}` |
|  151387 |  4645 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4646 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  151387 |  4647 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - |  4648 | `	/* Release the conditional block */` |
|  151387 |  4649 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4650 | `	/* Statement successfully compiled */` |
|  151387 |  4651 | `	return SXRET_OK;` |
|     ! 0 |  4652 | `Synchronize:` |
|       - |  4653 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - |  4654 | `	 */` |
|     ! 0 |  4655 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4656 | `		pGen->pIn++;` |
|     ! 0 |  4657 | `	}` |
|     ! 0 |  4658 | `	return SXRET_OK;` |
|   75696 |  4659 | `}` |
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
|       5 |  4682 | `{` |
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
|      23 |  4736 | `}` |
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
|  239842 |  4753 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       5 |  4754 | `{` |
|  239847 |  4755 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - |  4756 | `	sxi32 rc;` |
|  239847 |  4757 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  239847 |  4758 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|       - |  4759 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|       - |  4760 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|       - |  4761 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|       - |  4762 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|       - |  4763 | `	 * normally below so token processing stays consistent. */` |
|  617639 |  4764 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  377797 |  4765 | `		pFuncBlock = pFuncBlock->pParent;` |
|       5 |  4766 | `	}` |
|  239842 |  4767 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|  239815 |  4768 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|       3 |  4769 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  4770 | `			"A never-returning function must not return");` |
|       3 |  4771 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4772 | `			return SXERR_ABORT;` |
|       - |  4773 | `		}` |
|       1 |  4774 | `	}` |
|       - |  4775 | `	/* Jump the 'return' keyword */` |
|  239847 |  4776 | `	pGen->pIn++;` |
|  239847 |  4777 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4778 | `		/* Compile the expression */` |
|  239817 |  4779 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  239817 |  4780 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4781 | `			return SXERR_ABORT;` |
|  239817 |  4782 | `		}else if(rc != SXERR_EMPTY ){` |
|  239817 |  4783 | `			nRet = 1;` |
|  119906 |  4784 | `		}` |
|  119906 |  4785 | `	}` |
|       - |  4786 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|       - |  4787 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|       - |  4788 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|       - |  4789 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|       - |  4790 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  239847 |  4791 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  239847 |  4792 | `	return SXRET_OK;` |
|  119926 |  4793 | `}` |
|       - |  4794 | `/*` |
|       - |  4795 | ` * Compile a yield expression.` |
|       - |  4796 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - |  4797 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - |  4798 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - |  4799 | ` */` |
|     170 |  4800 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       5 |  4801 | `{` |
|       - |  4802 | `	SyToken *pTmp, *pSplit;` |
|     175 |  4803 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     175 |  4804 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - |  4805 | `	sxi32 rc;` |
|      85 |  4806 | `	(void)iCompileFlag;` |
|       - |  4807 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     175 |  4808 | `	pGen->pIn++;` |
|       - |  4809 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - |  4810 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|       - |  4811 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|       - |  4812 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|       - |  4813 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     188 |  4814 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|     102 |  4815 | `		&& pGen->pIn->sData.nByte == 4` |
|      41 |  4816 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|      40 |  4817 | `		pGen->pIn++; /* Skip 'from' */` |
|      40 |  4818 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      40 |  4819 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4820 | `			return SXERR_ABORT;` |
|       - |  4821 | `		}` |
|      40 |  4822 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  4823 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|     ! 0 |  4824 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|       - |  4825 | `				"Missing expression after 'yield from'");` |
|     ! 0 |  4826 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4827 | `				return SXERR_ABORT;` |
|       - |  4828 | `			}` |
|     ! 0 |  4829 | `		}` |
|      40 |  4830 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|      40 |  4831 | `		return SXRET_OK;` |
|       - |  4832 | `	}` |
|     139 |  4833 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4834 | `		/* Bare yield — no value */` |
|       3 |  4835 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|       3 |  4836 | `		return SXRET_OK;` |
|       - |  4837 | `	}` |
|       - |  4838 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     137 |  4839 | `	pSplit = 0;` |
|       - |  4840 | `	{` |
|     137 |  4841 | `		SyToken *pCur = pGen->pIn;` |
|     137 |  4842 | `		sxi32 nNest = 0;` |
|     285 |  4843 | `		while( pCur < pGen->pEnd ){` |
|     167 |  4844 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  4845 | `				nNest++;` |
|     167 |  4846 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  4847 | `				nNest--;` |
|     167 |  4848 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|      16 |  4849 | `				pSplit = pCur;` |
|      16 |  4850 | `				break;` |
|       - |  4851 | `			}` |
|     153 |  4852 | `			pCur++;` |
|       5 |  4853 | `		}` |
|       - |  4854 | `	}` |
|     137 |  4855 | `	pTmp = pGen->pEnd;` |
|     137 |  4856 | `	if( pSplit ){` |
|       - |  4857 | `		/* yield $key => $value */` |
|      16 |  4858 | `		pGen->pEnd = pSplit;` |
|      16 |  4859 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4860 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4861 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|      16 |  4862 | `		pGen->pEnd = pTmp;` |
|      16 |  4863 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4864 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4865 | `		iP1 = 1;` |
|      16 |  4866 | `		iP2 = 1;` |
|       9 |  4867 | `	}else{` |
|       - |  4868 | `		/* yield $value */` |
|     123 |  4869 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     123 |  4870 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     123 |  4871 | `		if( rc != SXERR_EMPTY ){` |
|     123 |  4872 | `			iP1 = 1;` |
|      59 |  4873 | `		}` |
|       - |  4874 | `	}` |
|     137 |  4875 | `	pGen->pEnd = pTmp;` |
|     137 |  4876 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     137 |  4877 | `	return SXRET_OK;` |
|      90 |  4878 | `}` |
|       - |  4879 | `/*` |
|       - |  4880 | ` * Compile the die/exit language construct.` |
|       - |  4881 | ` * The role of these constructs is to terminate execution of the script.` |
|       - |  4882 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - |  4883 | ` */` |
|     120 |  4884 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       5 |  4885 | `{` |
|     125 |  4886 | `	sxi32 nExpr = 0;` |
|       - |  4887 | `	sxi32 rc;` |
|       - |  4888 | `	/* Jump the die/exit keyword */` |
|     125 |  4889 | `	pGen->pIn++;` |
|     125 |  4890 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4891 | `		/* Compile the expression */` |
|     125 |  4892 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     125 |  4893 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4894 | `			return SXERR_ABORT;` |
|     125 |  4895 | `		}else if(rc != SXERR_EMPTY ){` |
|     125 |  4896 | `			nExpr = 1;` |
|      60 |  4897 | `		}` |
|      60 |  4898 | `	}` |
|       - |  4899 | `	/* Emit the HALT instruction */` |
|     125 |  4900 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|     125 |  4901 | `	return SXRET_OK;` |
|      65 |  4902 | `}` |
|       - |  4903 | `/*` |
|       - |  4904 | ` * Compile the 'echo' language construct.` |
|       - |  4905 | ` */` |
|   14772 |  4906 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       5 |  4907 | `{` |
|   14777 |  4908 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4909 | `	sxi32 rc;` |
|       - |  4910 | `	/* Jump the 'echo' keyword */` |
|   14777 |  4911 | `	pGen->pIn++;` |
|       - |  4912 | `	/* Compile arguments one after one */` |
|   14777 |  4913 | `	pTmp = pGen->pEnd;` |
|   32711 |  4914 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   17939 |  4915 | `		if( pGen->pIn < pNext ){` |
|   17939 |  4916 | `			pGen->pEnd = pNext;` |
|   17939 |  4917 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   17939 |  4918 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4919 | `				return SXERR_ABORT;` |
|   17939 |  4920 | `			}else if( rc != SXERR_EMPTY ){` |
|       - |  4921 | `				/* Emit the consume instruction */` |
|   17915 |  4922 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    8955 |  4923 | `			}` |
|    8967 |  4924 | `		}` |
|       - |  4925 | `		/* Jump trailing commas */` |
|   21101 |  4926 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    3167 |  4927 | `			pNext++;` |
|       5 |  4928 | `		}` |
|   17939 |  4929 | `		pGen->pIn = pNext;` |
|       5 |  4930 | `	}` |
|       - |  4931 | `	/* Restore token stream */` |
|   14777 |  4932 | `	pGen->pEnd = pTmp;` |
|   14777 |  4933 | `	return SXRET_OK;` |
|    7391 |  4934 | `}` |
|       - |  4935 | `/*` |
|       - |  4936 | ` * Compile the static statement.` |
|       - |  4937 | ` * According to the PHP language reference` |
|       - |  4938 | ` *  Another important feature of variable scoping is the static variable.` |
|       - |  4939 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - |  4940 | ` *  when program execution leaves this scope.` |
|       - |  4941 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - |  4942 | ` * Symisc eXtension.` |
|       - |  4943 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - |  4944 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  4945 | ` *  Example` |
|       - |  4946 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  4947 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  4948 | ` */` |
|       8 |  4949 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       3 |  4950 | `{` |
|       - |  4951 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - |  4952 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - |  4953 | `	GenBlock *pBlock;` |
|       - |  4954 | `	SyString *pName;` |
|       - |  4955 | `	char *zDup;` |
|       - |  4956 | `	sxu32 nLine;` |
|       - |  4957 | `	sxi32 rc;` |
|       - |  4958 | `	/* Jump the static keyword */` |
|      11 |  4959 | `	nLine = pGen->pIn->nLine;` |
|      11 |  4960 | `	pGen->pIn++;` |
|       - |  4961 | `	/* Extract the enclosing function if any */` |
|      11 |  4962 | `	pBlock = pGen->pCurrent;` |
|      19 |  4963 | `	while( pBlock ){` |
|      19 |  4964 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|      11 |  4965 | `			break;` |
|       - |  4966 | `		}` |
|       - |  4967 | `		/* Point to the upper block */` |
|      11 |  4968 | `		pBlock = pBlock->pParent;` |
|       3 |  4969 | `	}` |
|      11 |  4970 | `	if( pBlock == 0 ){` |
|       - |  4971 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 |  4972 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4973 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 |  4974 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4975 | `				return SXERR_ABORT;` |
|       - |  4976 | `			}` |
|     ! 0 |  4977 | `			goto Synchronize;` |
|       - |  4978 | `		}` |
|       - |  4979 | `		/* Compile the expression holding the variable */` |
|     ! 0 |  4980 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  4981 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4982 | `			return SXERR_ABORT;` |
|     ! 0 |  4983 | `		}else if( rc != SXERR_EMPTY ){` |
|       - |  4984 | `			/* Emit the POP instruction */` |
|     ! 0 |  4985 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  4986 | `		}` |
|     ! 0 |  4987 | `		return SXRET_OK;` |
|       - |  4988 | `	}` |
|      11 |  4989 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - |  4990 | `	/* Make sure we are dealing with a valid statement */` |
|      11 |  4991 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       6 |  4992 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 |  4993 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 |  4994 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4995 | `				return SXERR_ABORT;` |
|       - |  4996 | `			}` |
|       3 |  4997 | `			goto Synchronize;` |
|       - |  4998 | `	}` |
|       8 |  4999 | `	pGen->pIn++;` |
|       - |  5000 | `	/* Extract variable name */` |
|       8 |  5001 | `	pName = &pGen->pIn->sData;` |
|       8 |  5002 | `	pGen->pIn++; /* Jump the var name */` |
|       8 |  5003 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 |  5004 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  5005 | `		goto Synchronize;` |
|       - |  5006 | `	}` |
|       - |  5007 | `	/* Initialize the structure describing the static variable */` |
|       8 |  5008 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       8 |  5009 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - |  5010 | `	/* Duplicate variable name */` |
|       8 |  5011 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       8 |  5012 | `	if( zDup == 0 ){` |
|     ! 0 |  5013 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  5014 | `		return SXERR_ABORT;` |
|       - |  5015 | `	}` |
|       8 |  5016 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - |  5017 | `	/* Check if we have an expression to compile */` |
|       8 |  5018 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - |  5019 | `		SySet *pInstrContainer;` |
|       - |  5020 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - |  5021 | `		 * Static variable can take any complex expression including function` |
|       - |  5022 | `		 * call as their initialization value.` |
|       - |  5023 | `		 * Example:` |
|       - |  5024 | `		 *		static $var = foo(1,4+5,bar());` |
|       - |  5025 | `		 */` |
|       8 |  5026 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - |  5027 | `		/* Swap bytecode container */` |
|       8 |  5028 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       8 |  5029 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - |  5030 | `		/* Compile the expression */` |
|       8 |  5031 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5032 | `		/* Emit the done instruction */` |
|       8 |  5033 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - |  5034 | `		/* Restore default bytecode container */` |
|       8 |  5035 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       3 |  5036 | `	}` |
|       - |  5037 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       8 |  5038 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       8 |  5039 | `	return SXRET_OK;` |
|       1 |  5040 | `Synchronize:` |
|       - |  5041 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - |  5042 | `	 * statement.` |
|       - |  5043 | `	 */` |
|       5 |  5044 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 |  5045 | `		pGen->pIn++;` |
|       1 |  5046 | `	}` |
|       3 |  5047 | `	return SXRET_OK;` |
|       7 |  5048 | `}` |
|       - |  5049 | `/*` |
|       - |  5050 | ` * Compile the var statement.` |
|       - |  5051 | ` * Symisc Extension:` |
|       - |  5052 | ` *      var statement can be used outside of a class definition.` |
|       - |  5053 | ` */` |
|       4 |  5054 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 |  5055 | `{` |
|       - |  5056 | `	sxu32 nLine;` |
|       - |  5057 | `	sxi32 rc;` |
|       5 |  5058 | `	nLine = pGen->pIn->nLine;` |
|       - |  5059 | `	/* Jump the 'var' keyword */` |
|       5 |  5060 | `	pGen->pIn++;` |
|       5 |  5061 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  5062 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - |  5063 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 |  5064 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 |  5065 | `			pGen->pIn++;` |
|     ! 0 |  5066 | `		}` |
|     ! 0 |  5067 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5068 | `			return SXERR_ABORT;` |
|       - |  5069 | `		}` |
|     ! 0 |  5070 | `	}else{` |
|       - |  5071 | `		/* Compile the expression */` |
|       5 |  5072 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 |  5073 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5074 | `			return SXERR_ABORT;` |
|       5 |  5075 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 |  5076 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 |  5077 | `		}` |
|       - |  5078 | `	}` |
|       5 |  5079 | `	return SXRET_OK;` |
|       3 |  5080 | `}` |
|       - |  5081 | `/*` |
|       - |  5082 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - |  5083 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - |  5084 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - |  5085 | ` */` |
|       - |  5086 | `/*` |
|       - |  5087 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - |  5088 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - |  5089 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - |  5090 | ` * qualified name and updates the instruction's operand index.` |
|       - |  5091 | ` *` |
|       - |  5092 | ` * Resolution order:` |
|       - |  5093 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - |  5094 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - |  5095 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - |  5096 | ` *` |
|       - |  5097 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - |  5098 | ` * came from an import (step 1) and 0 otherwise.` |
|       - |  5099 | ` * Returns the (possibly new) literal index.` |
|       - |  5100 | ` */` |
|  465966 |  5101 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       5 |  5102 | `{` |
|       - |  5103 | `	ph7_value *pLit;` |
|       - |  5104 | `	const char *zLit;` |
|       - |  5105 | `	SyString sQualified;` |
|       - |  5106 | `	sxu32 nLit;` |
|       - |  5107 | `	sxu32 k;` |
|       - |  5108 | `	sxu32 nNewIdx;` |
|       - |  5109 | `	int hasNsSep;` |
|       - |  5110 | `	SyHashEntry *pImport;` |
|       - |  5111 | `	ph7_value *pNew;` |
|  465971 |  5112 | `	if( pFromImport ){` |
|  445941 |  5113 | `		*pFromImport = 0;` |
|  222968 |  5114 | `	}` |
|  465971 |  5115 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  465971 |  5116 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 |  5117 | `		return nOrigIdx;` |
|       - |  5118 | `	}` |
|  465971 |  5119 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  465971 |  5120 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - |  5121 | `	/* Skip if already qualified (contains backslash) */` |
|  465971 |  5122 | `	hasNsSep = 0;` |
| 5145869 |  5123 | `	for( k = 0; k < nLit; k++ ){` |
| 4679911 |  5124 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 2339954 |  5125 | `	}` |
|  465971 |  5126 | `	if( hasNsSep ){` |
|      11 |  5127 | `		return nOrigIdx;` |
|       - |  5128 | `	}` |
|       - |  5129 | `	/* Check use imports first (works even outside namespaces) */` |
|  465963 |  5130 | `	SyBlobReset(&pGen->sWorker);` |
|  465963 |  5131 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  465963 |  5132 | `	if( pImport ){` |
|      41 |  5133 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      41 |  5134 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      41 |  5135 | `		if( pFromImport ){` |
|      18 |  5136 | `			*pFromImport = 1;` |
|       8 |  5137 | `		}` |
|      23 |  5138 | `	}else{` |
|  465927 |  5139 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  465837 |  5140 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - |  5141 | `		}` |
|       - |  5142 | `		/* Prepend current namespace */` |
|      95 |  5143 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      95 |  5144 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      95 |  5145 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - |  5146 | `	}` |
|       - |  5147 | `	/* Look up or create a new literal for the qualified name */` |
|     131 |  5148 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     131 |  5149 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      57 |  5150 | `		return nNewIdx; /* Already interned */` |
|       - |  5151 | `	}` |
|      79 |  5152 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      79 |  5153 | `	if( pNew == 0 ){` |
|     ! 0 |  5154 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - |  5155 | `	}` |
|      79 |  5156 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      79 |  5157 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      79 |  5158 | `	return nNewIdx;` |
|  232988 |  5159 | `}` |
|       - |  5160 | `/*` |
|       - |  5161 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - |  5162 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - |  5163 | ` */` |
|   98480 |  5164 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5165 | `{` |
|       - |  5166 | `	SyHashEntry *pImport;` |
|       - |  5167 | `	/* Check use imports first */` |
|   98485 |  5168 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   98485 |  5169 | `	if( pImport ){` |
|      15 |  5170 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      15 |  5171 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      15 |  5172 | `		return;` |
|       - |  5173 | `	}` |
|       - |  5174 | `	/* Prepend current namespace if active */` |
|   98473 |  5175 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 |  5176 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  5177 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 |  5178 | `	}` |
|   98473 |  5179 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   49245 |  5180 | `}` |
|       - |  5181 | `/*` |
|       - |  5182 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - |  5183 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - |  5184 | ` * The caller must release pOut when done.` |
|       - |  5185 | ` */` |
|  142302 |  5186 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5187 | `{` |
|  142307 |  5188 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      63 |  5189 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      63 |  5190 | `		SyBlobAppend(pOut,"\\",1);` |
|      29 |  5191 | `	}` |
|  142307 |  5192 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|  142307 |  5193 | `}` |
|       - |  5194 | `/*` |
|       - |  5195 | ` * Compile a namespace statement` |
|       - |  5196 | ` * According to the PHP language reference manual` |
|       - |  5197 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - |  5198 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - |  5199 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - |  5200 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - |  5201 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - |  5202 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - |  5203 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - |  5204 | ` *  programming world.` |
|       - |  5205 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - |  5206 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - |  5207 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - |  5208 | ` *  classes/functions/constants.` |
|       - |  5209 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - |  5210 | ` *  readability of source code.` |
|       - |  5211 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - |  5212 | ` *  Here is an example of namespace syntax in PHP:` |
|       - |  5213 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - |  5214 | ` *       class MyClass {}` |
|       - |  5215 | ` *       function myfunction() {}` |
|       - |  5216 | ` *       const MYCONST = 1;` |
|       - |  5217 | ` *       $a = new MyClass;` |
|       - |  5218 | ` *       $c = new \my\name\MyClass;` |
|       - |  5219 | ` *       $a = strlen('hi');` |
|       - |  5220 | ` *       $d = namespace\MYCONST;` |
|       - |  5221 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - |  5222 | ` *       echo constant($d);` |
|       - |  5223 | ` * NOTE` |
|       - |  5224 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5225 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5226 | ` */` |
|       - |  5227 | `/*` |
|       - |  5228 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - |  5229 | ` */` |
|      14 |  5230 | `static const char * TokenTypeName(sxu32 nType)` |
|       4 |  5231 | `{` |
|      18 |  5232 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|      11 |  5233 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|      11 |  5234 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|      11 |  5235 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|      11 |  5236 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|      11 |  5237 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 |  5238 | `	return "token";` |
|      11 |  5239 | `}` |
|     106 |  5240 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       5 |  5241 | `{` |
|       - |  5242 | `	sxu32 nLine;` |
|       - |  5243 | `	sxi32 rc;` |
|     111 |  5244 | `	nLine = pGen->pIn->nLine;` |
|     111 |  5245 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - |  5246 | `	/* Reset namespace and clear previous use imports */` |
|     111 |  5247 | `	SyBlobReset(&pGen->sNamespace);` |
|     111 |  5248 | `	SyHashRelease(&pGen->hUseImports);` |
|     111 |  5249 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5250 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     111 |  5251 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5252 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     111 |  5253 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5254 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  5255 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 |  5256 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5257 | `		return SXRET_OK;` |
|       - |  5258 | `	}` |
|     111 |  5259 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - |  5260 | `		/* namespace; — switch to global namespace */` |
|     ! 0 |  5261 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5262 | `		return SXRET_OK;` |
|       - |  5263 | `	}` |
|     111 |  5264 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - |  5265 | `		/* namespace { } — global namespace block */` |
|     ! 0 |  5266 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5267 | `		return SXRET_OK;` |
|       - |  5268 | `	}` |
|       - |  5269 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     259 |  5270 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     153 |  5271 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - |  5272 | `			/* Append backslash separator */` |
|      26 |  5273 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      26 |  5274 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      11 |  5275 | `			}` |
|      15 |  5276 | `		}else{` |
|       - |  5277 | `			/* Append identifier */` |
|     131 |  5278 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  5279 | `		}` |
|     153 |  5280 | `		pGen->pIn++;` |
|       5 |  5281 | `	}` |
|       - |  5282 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - |  5283 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - |  5284 | `	{` |
|     111 |  5285 | `		char *zNsDup = 0;` |
|     111 |  5286 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     161 |  5287 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     104 |  5288 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      52 |  5289 | `		}` |
|     111 |  5290 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - |  5291 | `	}` |
|     111 |  5292 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       8 |  5293 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  5294 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 |  5295 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       6 |  5296 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5297 | `			return SXERR_ABORT;` |
|       - |  5298 | `		}` |
|       2 |  5299 | `	}` |
|     111 |  5300 | `	return SXRET_OK;` |
|      58 |  5301 | `}` |
|       - |  5302 | `/*` |
|       - |  5303 | ` * Compile the 'use' statement` |
|       - |  5304 | ` * According to the PHP language reference manual` |
|       - |  5305 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - |  5306 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - |  5307 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - |  5308 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - |  5309 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - |  5310 | ` *  a function or constant is not supported.` |
|       - |  5311 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - |  5312 | ` * NOTE` |
|       - |  5313 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5314 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5315 | ` */` |
|      68 |  5316 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       5 |  5317 | `{` |
|       - |  5318 | `	sxu32 nLine;` |
|       - |  5319 | `	sxi32 rc;` |
|       - |  5320 | `	SyBlob sPath;` |
|       - |  5321 | `	SyString sAlias;` |
|       - |  5322 | `	SyToken *pLast;` |
|       - |  5323 | `	char *zDup;` |
|       - |  5324 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - |  5325 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - |  5326 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      73 |  5327 | `	nLine = pGen->pIn->nLine;` |
|      73 |  5328 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - |  5329 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      73 |  5330 | `	iUseType = 0;` |
|      73 |  5331 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 |  5332 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 |  5333 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 |  5334 | `			iUseType = 1;` |
|      16 |  5335 | `			pGen->pIn++;` |
|      23 |  5336 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 |  5337 | `			iUseType = 2;` |
|      16 |  5338 | `			pGen->pIn++;` |
|       7 |  5339 | `		}` |
|      14 |  5340 | `	}` |
|       - |  5341 | `	/* Select target hash tables based on import type */` |
|      73 |  5342 | `	switch( iUseType ){` |
|       7 |  5343 | `		case 1:` |
|      16 |  5344 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 |  5345 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 |  5346 | `			break;` |
|       7 |  5347 | `		case 2:` |
|      16 |  5348 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 |  5349 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 |  5350 | `			break;` |
|      20 |  5351 | `		default:` |
|      45 |  5352 | `			pGenHash = &pGen->hUseImports;` |
|      45 |  5353 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      40 |  5354 | `			break;` |
|       - |  5355 | `	}` |
|      73 |  5356 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - |  5357 | `	/* Process one or more use declarations separated by commas */` |
|      35 |  5358 | `	for(;;){` |
|      75 |  5359 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5360 | `			break;` |
|       - |  5361 | `		}` |
|      75 |  5362 | `		SyBlobReset(&sPath);` |
|      75 |  5363 | `		pLast = 0;` |
|       - |  5364 | `		/* Collect the full namespace path */` |
|     261 |  5365 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     191 |  5366 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     131 |  5367 | `				pLast = pGen->pIn;` |
|     131 |  5368 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      65 |  5369 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 |  5370 | `				}` |
|     131 |  5371 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      63 |  5372 | `			}` |
|     191 |  5373 | `			pGen->pIn++;` |
|       5 |  5374 | `		}` |
|      75 |  5375 | `		if( pLast == 0 ){` |
|       - |  5376 | `			/* Empty path */` |
|       5 |  5377 | `			break;` |
|       - |  5378 | `		}` |
|       - |  5379 | `		/* Default alias is the last component of the path */` |
|      71 |  5380 | `		sAlias = pLast->sData;` |
|       - |  5381 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      66 |  5382 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      46 |  5383 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      18 |  5384 | `			pGen->pIn++; /* Jump 'as' */` |
|      18 |  5385 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      18 |  5386 | `				sAlias = pGen->pIn->sData;` |
|      18 |  5387 | `				pGen->pIn++;` |
|       8 |  5388 | `			}` |
|       8 |  5389 | `		}` |
|       - |  5390 | `		/* Check for duplicate import alias (per-type) */` |
|      71 |  5391 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       8 |  5392 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  5393 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 |  5394 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       6 |  5395 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5396 | `				SyBlobRelease(&sPath);` |
|     ! 0 |  5397 | `				return SXERR_ABORT;` |
|       - |  5398 | `			}` |
|       2 |  5399 | `		}` |
|       - |  5400 | `		/* Register the import: alias -> FQN.` |
|       - |  5401 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - |  5402 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - |  5403 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|     104 |  5404 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      66 |  5405 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      71 |  5406 | `		if( zDup ){` |
|      71 |  5407 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      71 |  5408 | `			if( pVmHash ){` |
|       - |  5409 | `				/* Class imports: populate VM table directly (class resolution` |
|       - |  5410 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      43 |  5411 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      43 |  5412 | `				if( zAliasDup ){` |
|      43 |  5413 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      19 |  5414 | `				}` |
|      19 |  5415 | `			}` |
|      71 |  5416 | `			if( iUseType == 2 ){` |
|       - |  5417 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - |  5418 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 |  5419 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 |  5420 | `				if( zAliasDup ){` |
|       - |  5421 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - |  5422 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - |  5423 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 |  5424 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 |  5425 | `					if( azPair ){` |
|      16 |  5426 | `						azPair[0] = zAliasDup;` |
|      16 |  5427 | `						azPair[1] = zDup;` |
|      16 |  5428 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 |  5429 | `					}` |
|       7 |  5430 | `				}` |
|       7 |  5431 | `			}` |
|      33 |  5432 | `		}` |
|       - |  5433 | `		/* Check for comma (multiple use declarations) */` |
|      71 |  5434 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  5435 | `			pGen->pIn++;` |
|       2 |  5436 | `		}else{` |
|      37 |  5437 | `			break;` |
|       - |  5438 | `		}` |
|       1 |  5439 | `	}` |
|      73 |  5440 | `	SyBlobRelease(&sPath);` |
|      73 |  5441 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 |  5442 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 |  5443 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 |  5444 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5445 | `			return SXERR_ABORT;` |
|       - |  5446 | `		}` |
|       1 |  5447 | `	}` |
|      73 |  5448 | `	return SXRET_OK;` |
|      39 |  5449 | `}` |
|       - |  5450 | `/*` |
|       - |  5451 | ` * Compile the stupid 'declare' language construct.` |
|       - |  5452 | ` *` |
|       - |  5453 | ` * According to the PHP language reference manual.` |
|       - |  5454 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - |  5455 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - |  5456 | ` *  declare (directive)` |
|       - |  5457 | ` *   statement` |
|       - |  5458 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - |  5459 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - |  5460 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - |  5461 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - |  5462 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - |  5463 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - |  5464 | ` * <?php` |
|       - |  5465 | ` * // these are the same:` |
|       - |  5466 | ` * // you can use this:` |
|       - |  5467 | ` * declare(ticks=1) {` |
|       - |  5468 | ` *   // entire script here` |
|       - |  5469 | ` * }` |
|       - |  5470 | ` * // or you can use this:` |
|       - |  5471 | ` * declare(ticks=1);` |
|       - |  5472 | ` * // entire script here` |
|       - |  5473 | ` * ?>` |
|       - |  5474 | ` *` |
|       - |  5475 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - |  5476 | ` */` |
|       - |  5477 | `/*` |
|       - |  5478 | ` * Match a directive name against a known literal (case-insensitive).` |
|       - |  5479 | ` */` |
|      68 |  5480 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|       5 |  5481 | `{` |
|     103 |  5482 | `	return SyStringLength(pName) == nWant` |
|      68 |  5483 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|       5 |  5484 | `}` |
|       - |  5485 |  |
|      40 |  5486 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       5 |  5487 | `{` |
|      45 |  5488 | `	sxu32 nLine = pGen->pIn->nLine;` |
|      45 |  5489 | `	SyToken *pBodyEnd = 0;` |
|       - |  5490 | `	SyToken *pBodyStart;` |
|       - |  5491 | `	SyToken *pCursor;` |
|       - |  5492 | `	int bHasStrictTypes;` |
|       - |  5493 | `	int bBlockForm;` |
|       - |  5494 | `	int bPlacementOk;` |
|       - |  5495 | `	sxi32 rc;` |
|      45 |  5496 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|      45 |  5497 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       6 |  5498 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       6 |  5499 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5500 | `			return SXERR_ABORT;` |
|       - |  5501 | `		}` |
|       6 |  5502 | `		goto Synchro;` |
|       - |  5503 | `	}` |
|      41 |  5504 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|      41 |  5505 | `	pBodyStart = pGen->pIn;` |
|       - |  5506 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|      41 |  5507 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|      41 |  5508 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  5509 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 |  5510 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5511 | `			return SXERR_ABORT;` |
|       - |  5512 | `		}` |
|     ! 0 |  5513 | `		return SXRET_OK;` |
|       - |  5514 | `	}` |
|       - |  5515 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|       - |  5516 | `	 * now delimits the comma-separated directive list. */` |
|      41 |  5517 | `	pGen->pIn = &pBodyEnd[1];` |
|      41 |  5518 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|     ! 0 |  5519 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 |  5520 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5521 | `			return SXERR_ABORT;` |
|       - |  5522 | `		}` |
|     ! 0 |  5523 | `	}` |
|      41 |  5524 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|      41 |  5525 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|      41 |  5526 | `	bHasStrictTypes = 0;` |
|       - |  5527 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|       - |  5528 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|       - |  5529 | `	 * directive appears anywhere in the list, before validating values. */` |
|      41 |  5530 | `	pCursor = pBodyStart;` |
|      53 |  5531 | `	while( pCursor < pBodyEnd ){` |
|      49 |  5532 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      41 |  5533 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|      37 |  5534 | `				bHasStrictTypes = 1;` |
|      37 |  5535 | `				break;` |
|       - |  5536 | `			}` |
|       2 |  5537 | `		}` |
|      14 |  5538 | `		pCursor++;` |
|       2 |  5539 | `	}` |
|      41 |  5540 | `	if( bHasStrictTypes && bBlockForm ){` |
|       3 |  5541 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5542 | `			"strict_types declaration must not use block mode");` |
|       3 |  5543 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5544 | `		return SXRET_OK;` |
|       - |  5545 | `	}` |
|      39 |  5546 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|       6 |  5547 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5548 | `			"strict_types declaration must be the very first statement in the script");` |
|       6 |  5549 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       6 |  5550 | `		return SXRET_OK;` |
|       - |  5551 | `	}` |
|       - |  5552 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|      35 |  5553 | `	pCursor = pBodyStart;` |
|      65 |  5554 | `	while( pCursor < pBodyEnd ){` |
|       - |  5555 | `		SyToken *pNameTok;` |
|       - |  5556 | `		SyToken *pEqTok;` |
|       - |  5557 | `		SyToken *pValTok;` |
|       - |  5558 | `		SyString *pDirName;` |
|       - |  5559 | `		int bIsStrict;` |
|       - |  5560 | `		int iStrictValue;` |
|      37 |  5561 | `		pNameTok = pCursor;` |
|      37 |  5562 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5563 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5564 | `				"declare: Expecting a directive name");` |
|     ! 0 |  5565 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5566 | `			return SXRET_OK;` |
|       - |  5567 | `		}` |
|      37 |  5568 | `		pEqTok = pNameTok + 1;` |
|      37 |  5569 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|     ! 0 |  5570 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5571 | `				"declare: Expecting '=' after directive name");` |
|     ! 0 |  5572 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5573 | `			return SXRET_OK;` |
|       - |  5574 | `		}` |
|      37 |  5575 | `		pValTok = pEqTok + 1;` |
|      37 |  5576 | `		if( pValTok >= pBodyEnd ){` |
|     ! 0 |  5577 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5578 | `				"declare: Expecting value after '='");` |
|     ! 0 |  5579 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5580 | `			return SXRET_OK;` |
|       - |  5581 | `		}` |
|      37 |  5582 | `		pDirName = &pNameTok->sData;` |
|      37 |  5583 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|      37 |  5584 | `		if( bIsStrict ){` |
|       - |  5585 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|       - |  5586 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|      33 |  5587 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|     ! 0 |  5588 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5589 | `					"declare(strict_types) value must be a literal");` |
|     ! 0 |  5590 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5591 | `				return SXRET_OK;` |
|       - |  5592 | `			}` |
|      33 |  5593 | `			iStrictValue = -1;` |
|      33 |  5594 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|      33 |  5595 | `				const char *zv = SyStringData(&pValTok->sData);` |
|      33 |  5596 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|      33 |  5597 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|      31 |  5598 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|      14 |  5599 | `			}` |
|      33 |  5600 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|       3 |  5601 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5602 | `					"strict_types declaration must have 0 or 1 as its value");` |
|       3 |  5603 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5604 | `				return SXRET_OK;` |
|       - |  5605 | `			}` |
|      30 |  5606 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|      17 |  5607 | `		}else{` |
|       - |  5608 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|       - |  5609 | `			 * preserve the legacy notice so callers relying on the old` |
|       - |  5610 | `			 * behavior don't regress. */` |
|       8 |  5611 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|       - |  5612 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 |  5613 | `				ph7_lib_version()` |
|       - |  5614 | `				);` |
|       - |  5615 | `		}` |
|      34 |  5616 | `		pCursor = pValTok + 1;` |
|       - |  5617 | `		/* Consume separating comma (or end). */` |
|      34 |  5618 | `		if( pCursor < pBodyEnd ){` |
|       3 |  5619 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5620 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5621 | `					"declare: Expecting ',' or ')' after directive value");` |
|     ! 0 |  5622 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5623 | `				return SXRET_OK;` |
|       - |  5624 | `			}` |
|       3 |  5625 | `			pCursor++;` |
|       1 |  5626 | `		}` |
|       4 |  5627 | `	}` |
|       - |  5628 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|       - |  5629 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|       - |  5630 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|      32 |  5631 | `	return SXRET_OK;` |
|       2 |  5632 | `Synchro:` |
|       - |  5633 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      16 |  5634 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      12 |  5635 | `		pGen->pIn++;` |
|       2 |  5636 | `	}` |
|       6 |  5637 | `	return SXRET_OK;` |
|      25 |  5638 | `}` |
|       - |  5639 | `/*` |
|       - |  5640 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - |  5641 | ` * as follows:` |
|       - |  5642 | ` * function makecoffee($type = "cappuccino")` |
|       - |  5643 | ` * {` |
|       - |  5644 | ` *   return "Making a cup of $type.\n";` |
|       - |  5645 | ` * }` |
|       - |  5646 | ` * Symisc eXtension.` |
|       - |  5647 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - |  5648 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - |  5649 | ` *      Example: Work only with PH7,generate error under zend` |
|       - |  5650 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - |  5651 | ` *      {` |
|       - |  5652 | ` *       var_dump($a);` |
|       - |  5653 | ` *      }` |
|       - |  5654 | ` *     //call test without args` |
|       - |  5655 | ` *      test();` |
|       - |  5656 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - |  5657 | ` *      Example:` |
|       - |  5658 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - |  5659 | ` * 3 -) Function overloading!!` |
|       - |  5660 | ` *      Example:` |
|       - |  5661 | ` *      function foo($a) {` |
|       - |  5662 | ` *   	  return $a.PHP_EOL;` |
|       - |  5663 | ` *	    }` |
|       - |  5664 | ` *	    function foo($a, $b) {` |
|       - |  5665 | ` *   	  return $a + $b;` |
|       - |  5666 | ` *	    }` |
|       - |  5667 | ` *	    echo foo(5); // Prints "5"` |
|       - |  5668 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - |  5669 | ` *      // Same arg` |
|       - |  5670 | ` *	   function foo(string $a)` |
|       - |  5671 | ` *	   {` |
|       - |  5672 | ` *	     echo "a is a string\n";` |
|       - |  5673 | ` *	     var_dump($a);` |
|       - |  5674 | ` *	   }` |
|       - |  5675 | ` *	  function foo(int $a)` |
|       - |  5676 | ` *	  {` |
|       - |  5677 | ` *	    echo "a is integer\n";` |
|       - |  5678 | ` *	    var_dump($a);` |
|       - |  5679 | ` *	  }` |
|       - |  5680 | ` *	  function foo(array $a)` |
|       - |  5681 | ` *	  {` |
|       - |  5682 | ` * 	    echo "a is an array\n";` |
|       - |  5683 | ` * 	    var_dump($a);` |
|       - |  5684 | ` *	  }` |
|       - |  5685 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - |  5686 | ` *	  foo(52); // a is integer [second foo]` |
|       - |  5687 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - |  5688 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - |  5689 | ` * introduced by the PH7 engine.` |
|       - |  5690 | ` */` |
|   75894 |  5691 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       5 |  5692 | `{` |
|       - |  5693 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5694 | `	SySet *pInstrContainer;` |
|       - |  5695 | `	sxi32 rc;` |
|       - |  5696 | `	/* Swap token stream */` |
|   75899 |  5697 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   75899 |  5698 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   75899 |  5699 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - |  5700 | `	/* Compile the expression holding the argument value */` |
|   75899 |  5701 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5702 | `	/* Emit the done instruction */` |
|   75899 |  5703 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   75899 |  5704 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   75899 |  5705 | `	RE_SWAP_DELIMITER(pGen);` |
|   75899 |  5706 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  5707 | `		return SXERR_ABORT;` |
|       - |  5708 | `	}` |
|   75899 |  5709 | `	return SXRET_OK;` |
|   37952 |  5710 | `}` |
|       - |  5711 | `/*` |
|       - |  5712 | ` * Collect function arguments one after one.` |
|       - |  5713 | ` * According to the PHP language reference manual.` |
|       - |  5714 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - |  5715 | ` * list of expressions.` |
|       - |  5716 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - |  5717 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - |  5718 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - |  5719 | ` * for more information.` |
|       - |  5720 | ` * Example #1 Passing arrays to functions` |
|       - |  5721 | ` * <?php` |
|       - |  5722 | ` * function takes_array($input)` |
|       - |  5723 | ` * {` |
|       - |  5724 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - |  5725 | ` * }` |
|       - |  5726 | ` * ?>` |
|       - |  5727 | ` * Making arguments be passed by reference` |
|       - |  5728 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - |  5729 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - |  5730 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - |  5731 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - |  5732 | ` * to the argument name in the function definition:` |
|       - |  5733 | ` * Example #2 Passing function parameters by reference` |
|       - |  5734 | ` * <?php` |
|       - |  5735 | ` * function add_some_extra(&$string)` |
|       - |  5736 | ` * {` |
|       - |  5737 | ` *   $string .= 'and something extra.';` |
|       - |  5738 | ` * }` |
|       - |  5739 | ` * $str = 'This is a string, ';` |
|       - |  5740 | ` * add_some_extra($str);` |
|       - |  5741 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - |  5742 | ` * ?>` |
|       - |  5743 | ` *` |
|       - |  5744 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - |  5745 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - |  5746 | ` * on these extension.` |
|       - |  5747 | ` */` |
|  106146 |  5748 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|       5 |  5749 | `{` |
|       - |  5750 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - |  5751 | `	SyToken *pIn;  /* Token stream */` |
|       - |  5752 | `	SyBlob sSig;         /* Function signature */` |
|       - |  5753 | `	char *zDup;          /* Copy of argument name */` |
|       - |  5754 | `	sxi32 rc;` |
|       - |  5755 |  |
|  106151 |  5756 | `	pIn = pGen->pIn;` |
|  106151 |  5757 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - |  5758 | `	/* Process arguments one after one */` |
|  137218 |  5759 | `	for(;;){` |
|  274441 |  5760 | `		if( pIn >= pEnd ){` |
|       - |  5761 | `			/* No more arguments to process */` |
|  106135 |  5762 | `			break;` |
|       - |  5763 | `		}` |
|  168311 |  5764 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|  168311 |  5765 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|  168311 |  5766 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|  168311 |  5767 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|       - |  5768 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|       - |  5769 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|       - |  5770 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|       - |  5771 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|       - |  5772 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|       - |  5773 | `		{` |
|  168311 |  5774 | `			int bReadonly = 0, bVisSeen = 0;` |
|  168311 |  5775 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|  168311 |  5776 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       3 |  5777 | `				bReadonly = 1;` |
|       3 |  5778 | `				pIn++;` |
|       1 |  5779 | `			}` |
|  168311 |  5780 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|   65255 |  5781 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   65255 |  5782 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      71 |  5783 | `					bVisSeen = 1;` |
|      71 |  5784 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|      95 |  5785 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|      31 |  5786 | `						: PH7_CLASS_PROT_PUBLIC;` |
|      71 |  5787 | `					pIn++;` |
|      71 |  5788 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|      16 |  5789 | `						bReadonly = 1;` |
|      16 |  5790 | `						pIn++;` |
|       6 |  5791 | `					}` |
|      33 |  5792 | `				}` |
|   32625 |  5793 | `			}` |
|  168311 |  5794 | `			if( bVisSeen \|\| bReadonly ){` |
|      73 |  5795 | `				if( !bCtorCtx ){` |
|       6 |  5796 | `					if( bAbstractCtx ){` |
|       3 |  5797 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5798 | `							"Cannot declare promoted property in an abstract constructor");` |
|       2 |  5799 | `					}else{` |
|       3 |  5800 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5801 | `							"Cannot declare promoted property outside a constructor");` |
|       - |  5802 | `					}` |
|       6 |  5803 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  5804 | `						return SXERR_ABORT;` |
|       - |  5805 | `					}` |
|       6 |  5806 | `					return SXERR_SYNTAX;` |
|       - |  5807 | `				}` |
|      69 |  5808 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      69 |  5809 | `				sArg.iPromoteVis = iVis;` |
|      69 |  5810 | `				if( bReadonly ){` |
|      18 |  5811 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|       7 |  5812 | `				}` |
|      32 |  5813 | `			}` |
|       - |  5814 | `		}` |
|       - |  5815 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|  210008 |  5816 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|  127676 |  5817 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   85236 |  5818 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   79781 |  5819 | `			sxu32 nLineLocal = pIn->nLine;` |
|   79781 |  5820 | `			sxi32 iTFlags = 0;` |
|   79781 |  5821 | `			pGen->pIn = pIn;` |
|   79781 |  5822 | `			rc = GenStateParseUnionTypeDecl(` |
|   39888 |  5823 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   39888 |  5824 | `				&iTFlags, &sArg.sTypeName,` |
|       - |  5825 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|       - |  5826 | `				/* bAllowVoid */ 0,` |
|   39888 |  5827 | `						nLineLocal);` |
|   79781 |  5828 | `			pIn = pGen->pIn;` |
|   79781 |  5829 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5830 | `				return SXERR_ABORT;` |
|   79781 |  5831 | `			}else if( rc == SXERR_CORRUPT ){` |
|       - |  5832 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|       3 |  5833 | `				return SXERR_SYNTAX;` |
|   79779 |  5834 | `			}else if( rc == SXERR_SYNTAX ){` |
|      12 |  5835 | `				if( pIn < pEnd ){` |
|      16 |  5836 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|       - |  5837 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|       4 |  5838 | `						&pIn->sData);` |
|       8 |  5839 | `				}else{` |
|     ! 0 |  5840 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|       - |  5841 | `						"syntax error, unexpected end of file");` |
|       - |  5842 | `				}` |
|      12 |  5843 | `				return SXERR_SYNTAX;` |
|       - |  5844 | `			}` |
|   79771 |  5845 | `			sArg.iFlags \|= iTFlags;` |
|   39883 |  5846 | `		}` |
|  168297 |  5847 | `		if( pIn >= pEnd ){` |
|     ! 0 |  5848 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 |  5849 | `			return rc;` |
|       - |  5850 | `		}` |
|  168297 |  5851 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - |  5852 | `			/* Pass by reference,record that */` |
|    3645 |  5853 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    3645 |  5854 | `			pIn++;` |
|    1820 |  5855 | `		}` |
|  168297 |  5856 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - |  5857 | `			/* Variadic parameter: ...$args */` |
|    3661 |  5858 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|    3661 |  5859 | `			pIn++;` |
|    1828 |  5860 | `		}` |
|  168297 |  5861 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  5862 | `			/* Invalid argument */` |
|     ! 0 |  5863 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 |  5864 | `			return rc;` |
|       - |  5865 | `		}` |
|  168297 |  5866 | `		pIn++; /* Jump the dollar sign */` |
|       - |  5867 | `		/* Copy argument name */` |
|  168297 |  5868 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|  168297 |  5869 | `		if( zDup == 0 ){` |
|     ! 0 |  5870 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  5871 | `			return SXERR_ABORT;` |
|       - |  5872 | `		}` |
|  168297 |  5873 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|  168297 |  5874 | `		pIn++;` |
|  168297 |  5875 | `		if( pIn < pEnd ){` |
|  101937 |  5876 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - |  5877 | `				SyToken *pDefend;` |
|   75901 |  5878 | `				sxi32 iNest = 0;` |
|   75901 |  5879 | `				pIn++; /* Jump the equal sign */` |
|   75901 |  5880 | `				pDefend = pIn;` |
|       - |  5881 | `				/* Process the default value associated with this argument */` |
|  159025 |  5882 | `				while( pDefend < pEnd ){` |
|  119259 |  5883 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   36135 |  5884 | `						break;` |
|       - |  5885 | `					}` |
|   83129 |  5886 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - |  5887 | `						/* Increment nesting level */` |
|    3619 |  5888 | `						iNest++;` |
|   81322 |  5889 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - |  5890 | `						/* Decrement nesting level */` |
|    3619 |  5891 | `						iNest--;` |
|    1807 |  5892 | `					}` |
|   83129 |  5893 | `					pDefend++;` |
|       5 |  5894 | `				}` |
|   75901 |  5895 | `				if( pIn >= pDefend ){` |
|       3 |  5896 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 |  5897 | `					return rc;` |
|       - |  5898 | `				}` |
|       - |  5899 | `				/* Process default value */` |
|   75899 |  5900 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   75899 |  5901 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  5902 | `					return rc;` |
|       - |  5903 | `				}` |
|       - |  5904 | `				/* Point beyond the default value */` |
|   75899 |  5905 | `				pIn = pDefend;` |
|   37947 |  5906 | `			}` |
|  101935 |  5907 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5908 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 |  5909 | `				return rc;` |
|       - |  5910 | `			}` |
|  101935 |  5911 | `			pIn++; /* Jump the trailing comma */` |
|   50965 |  5912 | `		}` |
|       - |  5913 | `		/* Append argument signature */` |
|  168295 |  5914 | `		if( sArg.nType > 0 ){` |
|   79717 |  5915 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - |  5916 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|   14509 |  5917 | `				int marker = 'o';` |
|   14509 |  5918 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|   14509 |  5919 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    7257 |  5920 | `			}else{` |
|       - |  5921 | `				int c;` |
|   65213 |  5922 | `				c = 'n'; /* cc warning */` |
|       - |  5923 | `				/* Type leading character */` |
|   65213 |  5924 | `				switch(sArg.nType){` |
|       3 |  5925 | `				case MEMOBJ_HASHMAP:` |
|       - |  5926 | `					/* Hashmap aka 'array' */` |
|       7 |  5927 | `					c = 'h';` |
|       7 |  5928 | `					break;` |
|    9086 |  5929 | `				case MEMOBJ_INT:` |
|       - |  5930 | `					/* Integer */` |
|   18177 |  5931 | `					c = 'i';` |
|   18177 |  5932 | `					break;` |
|       2 |  5933 | `				case MEMOBJ_BOOL:` |
|       - |  5934 | `					/* Bool */` |
|       5 |  5935 | `					c = 'b';` |
|       5 |  5936 | `					break;` |
|       2 |  5937 | `				case MEMOBJ_REAL:` |
|       - |  5938 | `					/* Float */` |
|       5 |  5939 | `					c = 'f';` |
|       5 |  5940 | `					break;` |
|   23503 |  5941 | `				case MEMOBJ_STRING:` |
|       - |  5942 | `					/* String */` |
|   47011 |  5943 | `					c = 's';` |
|   47011 |  5944 | `					break;` |
|       7 |  5945 | `				case MEMOBJ_OBJ:` |
|       - |  5946 | `					/* Object */` |
|      16 |  5947 | `					c = 'o';` |
|      14 |  5948 | `					break;` |
|       1 |  5949 | `				default:` |
|       2 |  5950 | `					break;` |
|       - |  5951 | `				}` |
|   65213 |  5952 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - |  5953 | `			}` |
|   39861 |  5954 | `		}else{` |
|       - |  5955 | `			/* No type is associated with this parameter which mean` |
|       - |  5956 | `			 * that this function is not condidate for overloading.` |
|       - |  5957 | `			 */` |
|   88583 |  5958 | `			SyBlobRelease(&sSig);` |
|       - |  5959 | `		}` |
|       - |  5960 | `		/* Save in the argument set */` |
|  168295 |  5961 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       5 |  5962 | `	}` |
|  106135 |  5963 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - |  5964 | `		/* Save function signature */` |
|   50787 |  5965 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   25391 |  5966 | `	}` |
|  106135 |  5967 | `	return SXRET_OK;` |
|   53078 |  5968 | `}` |
|       - |  5969 | `/*` |
|       - |  5970 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - |  5971 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  5972 | ` * and this routine takes care of generating the appropriate error message.` |
|       - |  5973 | ` */` |
|  226374 |  5974 | `static sxi32 GenStateCompileFuncBody(` |
|       - |  5975 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  5976 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - |  5977 | `	)` |
|       5 |  5978 | `{` |
|       - |  5979 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - |  5980 | `	GenBlock *pBlock;` |
|       - |  5981 | `	sxu32 nGotoOfft;` |
|       - |  5982 | `	sxi32 rc;` |
|       - |  5983 | `	/* Attach the new function */` |
|  226379 |  5984 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  226379 |  5985 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  5986 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - |  5987 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5988 | `		return SXERR_ABORT;` |
|       - |  5989 | `	}` |
|  226379 |  5990 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - |  5991 | `	/* Swap bytecode containers */` |
|  226379 |  5992 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  226379 |  5993 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - |  5994 | `	/* Emit constructor property promotion prologue:` |
|       - |  5995 | `	 *   $this->NAME = $NAME;` |
|       - |  5996 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|       - |  5997 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|       - |  5998 | `	{` |
|  226379 |  5999 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|       - |  6000 | `		sxu32 i;` |
|  365625 |  6001 | `		for( i = 0; i < nArg; i++ ){` |
|  139251 |  6002 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|       - |  6003 | `			char *zSrc;` |
|       - |  6004 | `			sxu32 nSrc,nName;` |
|       - |  6005 | `			SySet sToken;` |
|       - |  6006 | `			SyToken *pTmpIn,*pTmpEnd;` |
|       - |  6007 | `			sxi32 rcPromote;` |
|  139251 |  6008 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  139197 |  6009 | `				continue;` |
|       - |  6010 | `			}` |
|       - |  6011 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|       - |  6012 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|       - |  6013 | `			 * copied), so it must outlive the function — never free it. The` |
|       - |  6014 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|       - |  6015 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      59 |  6016 | `			nName = SyStringLength(&pArg->sName);` |
|      59 |  6017 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      59 |  6018 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      59 |  6019 | `			if( zSrc == 0 ){` |
|     ! 0 |  6020 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  6021 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  6022 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  6023 | `				return SXERR_ABORT;` |
|       - |  6024 | `			}` |
|       - |  6025 | `			{` |
|      59 |  6026 | `				char *z = zSrc;` |
|      59 |  6027 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      59 |  6028 | `				z += sizeof("$this->")-1;` |
|      59 |  6029 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      59 |  6030 | `				z += nName;` |
|      59 |  6031 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      59 |  6032 | `				z += sizeof(" = $")-1;` |
|      59 |  6033 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      59 |  6034 | `				z += nName;` |
|      59 |  6035 | `				*z = 0;` |
|       - |  6036 | `			}` |
|      59 |  6037 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      59 |  6038 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken);` |
|      59 |  6039 | `			pTmpIn = pGen->pIn;` |
|      59 |  6040 | `			pTmpEnd = pGen->pEnd;` |
|      59 |  6041 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      59 |  6042 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      59 |  6043 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      59 |  6044 | `			pGen->pIn = pTmpIn;` |
|      59 |  6045 | `			pGen->pEnd = pTmpEnd;` |
|      59 |  6046 | `			SySetRelease(&sToken);` |
|      59 |  6047 | `			if( rcPromote == SXERR_ABORT ){` |
|     ! 0 |  6048 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  6049 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  6050 | `				return SXERR_ABORT;` |
|       - |  6051 | `			}` |
|       - |  6052 | `			/* Discard the assignment result — this is a statement expression. */` |
|      59 |  6053 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      32 |  6054 | `		}` |
|       - |  6055 | `	}` |
|       - |  6056 | `	/* Compile the body */` |
|  226379 |  6057 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  6058 | `	/* Fix exception jumps now the destination is resolved */` |
|  226379 |  6059 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  6060 | `	/* Emit the final return if not yet done */` |
|  226379 |  6061 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  6062 | `	/* Fix gotos jumps now the destination is resolved */` |
|  226379 |  6063 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 |  6064 | `		rc = SXERR_ABORT;` |
|     ! 0 |  6065 | `	}` |
|  226379 |  6066 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - |  6067 | `	/* Restore the default container */` |
|  226379 |  6068 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  6069 | `	/* Leave function block */` |
|  226379 |  6070 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  226379 |  6071 | `	if( rc == SXERR_ABORT ){` |
|       - |  6072 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6073 | `		return SXERR_ABORT;` |
|       - |  6074 | `	}` |
|       - |  6075 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - |  6076 | `	{` |
|  226379 |  6077 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  6078 | `		sxu32 i;` |
| 4446521 |  6079 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 4220247 |  6080 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     105 |  6081 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     105 |  6082 | `				break;` |
|       - |  6083 | `			}` |
| 2110076 |  6084 | `		}` |
|       - |  6085 | `	}` |
|       - |  6086 | `	/* All done, function body compiled */` |
|  226379 |  6087 | `	return SXRET_OK;` |
|  113192 |  6088 | `}` |
|       - |  6089 | `/*` |
|       - |  6090 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - |  6091 | ` * According to the PHP language reference manual.` |
|       - |  6092 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - |  6093 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - |  6094 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - |  6095 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  6096 | ` *  Functions need not be defined before they are referenced.` |
|       - |  6097 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - |  6098 | ` *  a function even if they were defined inside and vice versa.` |
|       - |  6099 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - |  6100 | ` *  calls with over 32-64 recursion levels.` |
|       - |  6101 | ` *` |
|       - |  6102 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - |  6103 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - |  6104 | ` * on these extension.` |
|       - |  6105 | ` */` |
|       - |  6106 | `/*` |
|       - |  6107 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - |  6108 | ` */` |
|     510 |  6109 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       5 |  6110 | `{` |
|       - |  6111 | `	sxu32 i;` |
|    1453 |  6112 | `	for( i = 0; i < n; i++ ){` |
|    1247 |  6113 | `		int a = zA[i], b = zB[i];` |
|    1247 |  6114 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|    1247 |  6115 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|    1247 |  6116 | `		if( a != b ) return a - b;` |
|     474 |  6117 | `	}` |
|     211 |  6118 | `	return 0;` |
|     260 |  6119 | `}` |
|       - |  6120 | `/*` |
|       - |  6121 | ` * Internal type-atom kinds used during union type parsing.` |
|       - |  6122 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|       - |  6123 | ` * (which are positive bit values stored in sxu32).` |
|       - |  6124 | ` */` |
|       - |  6125 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|       - |  6126 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|       - |  6127 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|       - |  6128 |  |
|       - |  6129 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|       - |  6130 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|       - |  6131 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|       - |  6132 |  |
|       - |  6133 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|       - |  6134 | `struct PhlTypeAtom {` |
|       - |  6135 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|       - |  6136 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|       - |  6137 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|       - |  6138 | `	sxu32 nCanon;` |
|       - |  6139 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|       - |  6140 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|       - |  6141 | `};` |
|       - |  6142 |  |
|       - |  6143 | `/*` |
|       - |  6144 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|       - |  6145 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|       - |  6146 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|       - |  6147 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|       - |  6148 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|       - |  6149 | ` * already be consumed by the caller.` |
|       - |  6150 | ` */` |
|   80642 |  6151 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|       5 |  6152 | `{` |
|   80647 |  6153 | `	SyToken *pIn = pGen->pIn;` |
|   80647 |  6154 | `	SyZero(pOut, sizeof(*pOut));` |
|   80647 |  6155 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   80647 |  6156 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6157 | `		return SXERR_SYNTAX;` |
|       - |  6158 | `	}` |
|       - |  6159 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   80647 |  6160 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       8 |  6161 | `		pIn++;` |
|       8 |  6162 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6163 | `			return SXERR_SYNTAX;` |
|       - |  6164 | `		}` |
|       3 |  6165 | `	}` |
|   80647 |  6166 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6167 | `		return SXERR_SYNTAX;` |
|       - |  6168 | `	}` |
|   80647 |  6169 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|   65767 |  6170 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   65767 |  6171 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      32 |  6172 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   65753 |  6173 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|      71 |  6174 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   65706 |  6175 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|   18437 |  6176 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   56457 |  6177 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   47171 |  6178 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   23658 |  6179 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|      33 |  6180 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|      61 |  6181 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      27 |  6182 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      33 |  6183 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       9 |  6184 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|      19 |  6185 | `			pOut->nType = SXU32_HIGH;` |
|      19 |  6186 | `			pOut->sClass = pIn->sData;` |
|      11 |  6187 | `		}else{` |
|       3 |  6188 | `			return SXERR_SYNTAX;` |
|       - |  6189 | `		}` |
|   65765 |  6190 | `		pIn++;` |
|   32885 |  6191 | `	}else{` |
|       - |  6192 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|       - |  6193 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|   14885 |  6194 | `		SyString *pT = &pIn->sData;` |
|   14885 |  6195 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|      32 |  6196 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|      32 |  6197 | `			pIn++;` |
|   14871 |  6198 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|     157 |  6199 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|     157 |  6200 | `			pIn++;` |
|   14781 |  6201 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|      24 |  6202 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|      24 |  6203 | `			pIn++;` |
|      14 |  6204 | `		}else{` |
|       - |  6205 | `			/* Class / interface name; consume namespace path a\b\c */` |
|   14685 |  6206 | `			SyToken *pFirst = pIn;` |
|   14685 |  6207 | `			SyToken *pLast = pIn;` |
|   14685 |  6208 | `			pOut->nType = SXU32_HIGH;` |
|   14685 |  6209 | `			pOut->sClass = pIn->sData;` |
|   14685 |  6210 | `			pIn++;` |
|   22023 |  6211 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|   14688 |  6212 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       3 |  6213 | `				pLast = &pIn[1];` |
|       3 |  6214 | `				pIn += 2;` |
|       1 |  6215 | `			}` |
|   14685 |  6216 | `			if( pLast != pFirst ){` |
|       3 |  6217 | `				const char *zFirst = pFirst->sData.zString;` |
|       3 |  6218 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 |  6219 | `				pOut->sClass.zString = zFirst;` |
|       3 |  6220 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       1 |  6221 | `			}` |
|       - |  6222 | `		}` |
|       - |  6223 | `	}` |
|   80645 |  6224 | `	pGen->pIn = pIn;` |
|   80645 |  6225 | `	return SXRET_OK;` |
|   40326 |  6226 | `}` |
|       - |  6227 |  |
|       - |  6228 | `/*` |
|       - |  6229 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|       - |  6230 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|       - |  6231 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|       - |  6232 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|       - |  6233 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|       - |  6234 | ` */` |
|   80482 |  6235 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|       5 |  6236 | `{` |
|       - |  6237 | `	int i;` |
|   80487 |  6238 | `	int nNonNull = 0;` |
|   80487 |  6239 | `	int bAnyIntersection = 0;` |
|       - |  6240 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|   80487 |  6241 | `	sxu32 nMaxGroup = 0;` |
| 2655911 |  6242 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|  161103 |  6243 | `	for( i = 0; i < nAtoms; i++ ){` |
|   80621 |  6244 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   80593 |  6245 | `			nNonNull++;` |
|   80593 |  6246 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|   80593 |  6247 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|   80593 |  6248 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|   40294 |  6249 | `			}` |
|   40294 |  6250 | `		}` |
|   40313 |  6251 | `	}` |
|  161069 |  6252 | `	for( i = 0; i < nAtoms; i++ ){` |
|   80603 |  6253 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|      20 |  6254 | `			bAnyIntersection = 1;` |
|      20 |  6255 | `			break;` |
|       - |  6256 | `		}` |
|   40296 |  6257 | `	}` |
|   80487 |  6258 | `	if( bAnyIntersection ){` |
|       - |  6259 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|       - |  6260 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|       - |  6261 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|      20 |  6262 | `		sxu32 g, nGroups = 0;` |
|      20 |  6263 | `		int bFirstGroup = 1;` |
|      40 |  6264 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|      40 |  6265 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|      24 |  6266 | `			int bFirstMember = 1;` |
|       - |  6267 | `			int bWrap;` |
|      24 |  6268 | `			if( aGroupCount[g] == 0 ) continue;` |
|       - |  6269 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|       - |  6270 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|       - |  6271 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|       - |  6272 | `			 * parens, matching PHP's canonical text. */` |
|      32 |  6273 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|      24 |  6274 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|      24 |  6275 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|      72 |  6276 | `			for( i = 0; i < nAtoms; i++ ){` |
|      52 |  6277 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|      40 |  6278 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|      40 |  6279 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      38 |  6280 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      21 |  6281 | `				}else{` |
|       3 |  6282 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6283 | `				}` |
|      40 |  6284 | `				bFirstMember = 0;` |
|      22 |  6285 | `			}` |
|      24 |  6286 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|      24 |  6287 | `			bFirstGroup = 0;` |
|      14 |  6288 | `		}` |
|      20 |  6289 | `		if( bNullable ){` |
|     ! 0 |  6290 | `			SyBlobAppend(pBlob, "\|", 1);` |
|     ! 0 |  6291 | `			SyBlobAppend(pBlob, "null", 4);` |
|     ! 0 |  6292 | `		}` |
|      58 |  6293 | `		return;` |
|       - |  6294 | `	}` |
|   80471 |  6295 | `	if( nNonNull == 1 && bNullable ){` |
|       - |  6296 | `		/* Shorthand: ?T */` |
|      81 |  6297 | `		for( i = 0; i < nAtoms; i++ ){` |
|      81 |  6298 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      81 |  6299 | `			SyBlobAppend(pBlob, "?", 1);` |
|      81 |  6300 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      21 |  6301 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      12 |  6302 | `			}else{` |
|      62 |  6303 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6304 | `			}` |
|      81 |  6305 | `			return;` |
|     ! 0 |  6306 | `		}` |
|     ! 0 |  6307 | `	}` |
|       - |  6308 | `	{` |
|   80395 |  6309 | `		int bFirst = 1;` |
|       - |  6310 | `		/* 1) Classes in declaration order */` |
|  160887 |  6311 | `		for( i = 0; i < nAtoms; i++ ){` |
|   80497 |  6312 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|   14649 |  6313 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   14649 |  6314 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|   14649 |  6315 | `				bFirst = 0;` |
|    7322 |  6316 | `			}` |
|   40251 |  6317 | `		}` |
|       - |  6318 | `		/* 2) Built-ins in canonical order */` |
|       - |  6319 | `		{` |
|       - |  6320 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|       - |  6321 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|       - |  6322 | `			int k;` |
|  562735 |  6323 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  899521 |  6324 | `				for( i = 0; i < nAtoms; i++ ){` |
|  482849 |  6325 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|   65673 |  6326 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   65673 |  6327 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|   65673 |  6328 | `						bFirst = 0;` |
|   65673 |  6329 | `						break;` |
|       - |  6330 | `					}` |
|  208593 |  6331 | `				}` |
|  241175 |  6332 | `			}` |
|       - |  6333 | `		}` |
|       - |  6334 | `		/* 3) null suffix */` |
|   80395 |  6335 | `		if( bNullable ){` |
|      20 |  6336 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|      20 |  6337 | `			SyBlobAppend(pBlob, "null", 4);` |
|       8 |  6338 | `		}` |
|       - |  6339 | `	}` |
|   40246 |  6340 | `}` |
|       - |  6341 |  |
|       - |  6342 | `/*` |
|       - |  6343 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|       - |  6344 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|       - |  6345 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|       - |  6346 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|       - |  6347 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|       - |  6348 | ` * whether it was parenthesized.` |
|       - |  6349 | ` *` |
|       - |  6350 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|       - |  6351 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|       - |  6352 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|       - |  6353 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|       - |  6354 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|       - |  6355 | ` */` |
|   80624 |  6356 | `static sxi32 GenStateParsePart(` |
|       - |  6357 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|       - |  6358 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|       5 |  6359 | `{` |
|       - |  6360 | `	sxi32 rc;` |
|   80629 |  6361 | `	int nMembers = 0;` |
|   80629 |  6362 | `	int bParen = 0;` |
|   80629 |  6363 | `	*pnMembers = 0;` |
|   80629 |  6364 | `	*pbParen = 0;` |
|   80629 |  6365 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       6 |  6366 | `		bParen = 1;` |
|       6 |  6367 | `		pGen->pIn++; /* skip '(' */` |
|       2 |  6368 | `	}` |
|   40312 |  6369 | `	for(;;){` |
|   80647 |  6370 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|     ! 0 |  6371 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6372 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|     ! 0 |  6373 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6374 | `		}` |
|   80647 |  6375 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|   80647 |  6376 | `		if( rc != SXRET_OK ){` |
|       3 |  6377 | `			return rc;` |
|       - |  6378 | `		}` |
|   80645 |  6379 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|   80645 |  6380 | `		(*pnAtoms)++;` |
|   80645 |  6381 | `		nMembers++;` |
|       - |  6382 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|   80645 |  6383 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      24 |  6384 | `			SyToken *pNext = &pGen->pIn[1];` |
|      20 |  6385 | `			if( pNext < pGen->pEnd` |
|      24 |  6386 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      22 |  6387 | `				pGen->pIn++; /* skip '&' */` |
|      22 |  6388 | `				continue;` |
|       - |  6389 | `			}` |
|       1 |  6390 | `		}` |
|   80627 |  6391 | `		break;` |
|     ! 0 |  6392 | `	}` |
|   80627 |  6393 | `	if( bParen ){` |
|       6 |  6394 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6395 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6396 | `				"Malformed DNF type: expecting ')'");` |
|     ! 0 |  6397 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6398 | `		}` |
|       6 |  6399 | `		pGen->pIn++; /* skip ')' */` |
|       6 |  6400 | `		if( nMembers < 2 ){` |
|     ! 0 |  6401 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6402 | `				"Parenthesized type must be an intersection of at least two types");` |
|     ! 0 |  6403 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6404 | `		}` |
|       2 |  6405 | `	}` |
|   80627 |  6406 | `	*pnMembers = nMembers;` |
|   80627 |  6407 | `	*pbParen = bParen;` |
|   80627 |  6408 | `	return SXRET_OK;` |
|   40317 |  6409 | `}` |
|       - |  6410 |  |
|       - |  6411 | `/*` |
|       - |  6412 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|       - |  6413 | ` *` |
|       - |  6414 | ` * Outputs:` |
|       - |  6415 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|       - |  6416 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|       - |  6417 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|       - |  6418 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|       - |  6419 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|       - |  6420 | ` *     already be initialized by the caller (allocator set, etc).` |
|       - |  6421 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|       - |  6422 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|       - |  6423 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|       - |  6424 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|       - |  6425 | ` *` |
|       - |  6426 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|       - |  6427 | ` * SXERR_ABORT on fatal compile errors.` |
|       - |  6428 | ` */` |
|   80498 |  6429 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |  6430 | `	ph7_gen_state *pGen,` |
|       - |  6431 | `	sxu32 *pnType,` |
|       - |  6432 | `	SyString *pClass,` |
|       - |  6433 | `	SySet *pAlts,` |
|       - |  6434 | `	sxi32 *piTypeFlags,` |
|       - |  6435 | `	SyString *pTypeText,` |
|       - |  6436 | `	int iNullableFlag,` |
|       - |  6437 | `	int iUnionFlag,` |
|       - |  6438 | `	int bAllowVoid,` |
|       - |  6439 | `	sxu32 nLine` |
|       5 |  6440 | `){` |
|       - |  6441 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   80503 |  6442 | `	int nAtoms = 0;` |
|   80503 |  6443 | `	int bShortNullable = 0;` |
|   80503 |  6444 | `	int bExplicitNull = 0;` |
|       - |  6445 | `	sxi32 rc;` |
|   80503 |  6446 | `	*pnType = 0;` |
|   80503 |  6447 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   80503 |  6448 | `	*piTypeFlags = 0;` |
|   80503 |  6449 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|       - |  6450 |  |
|   80503 |  6451 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6452 | `		return SXRET_OK;` |
|       - |  6453 | `	}` |
|       - |  6454 | ``	/* Optional `?` shorthand prefix */`` |
|   80498 |  6455 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      71 |  6456 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      71 |  6457 | `		bShortNullable = 1;` |
|      71 |  6458 | `		pGen->pIn++;` |
|      71 |  6459 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6460 | `			return SXERR_SYNTAX;` |
|       - |  6461 | `		}` |
|      33 |  6462 | `	}` |
|       - |  6463 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|       - |  6464 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|       - |  6465 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|       - |  6466 | `	{` |
|       - |  6467 | `		int nMembers, bParen;` |
|   80503 |  6468 | `		sxu32 iGroup = 0;` |
|   80503 |  6469 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|   80503 |  6470 | `		if( rc != SXRET_OK ){` |
|       4 |  6471 | `			return rc;` |
|       - |  6472 | `		}` |
|       - |  6473 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|       - |  6474 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|       - |  6475 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|       - |  6476 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|       - |  6477 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|  120935 |  6478 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   80692 |  6479 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|     133 |  6480 | `			if( bShortNullable ){` |
|       - |  6481 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|       - |  6482 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|       - |  6483 | `				 * already reported" so callers skip their own error emission. */` |
|       3 |  6484 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6485 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|       3 |  6486 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|       - |  6487 | `			}` |
|     131 |  6488 | `			if( nMembers >= 2 && !bParen ){` |
|     ! 0 |  6489 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|       - |  6490 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|     ! 0 |  6491 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6492 | `			}` |
|     131 |  6493 | ``			pGen->pIn++; /* skip `\|` */`` |
|     131 |  6494 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|     131 |  6495 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  6496 | `				return rc;` |
|       - |  6497 | `			}` |
|       5 |  6498 | `		}` |
|   80499 |  6499 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|     ! 0 |  6500 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6501 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|     ! 0 |  6502 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6503 | `		}` |
|       - |  6504 | `	}` |
|       - |  6505 | `	/* Validation pass.` |
|       - |  6506 | `	 *` |
|       - |  6507 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|       - |  6508 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|       - |  6509 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|       - |  6510 | `	 */` |
|       - |  6511 | `	{` |
|       - |  6512 | `		int i, j;` |
|   80499 |  6513 | `		int bHasNonNull = 0;` |
|   80499 |  6514 | `		int bAnyIntersection = 0;` |
|       - |  6515 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|       - |  6516 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|       - |  6517 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
| 2656307 |  6518 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|  161137 |  6519 | `		for( i = 0; i < nAtoms; i++ ){` |
|   80643 |  6520 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|   40324 |  6521 | `		}` |
|  161099 |  6522 | `		for( i = 0; i < nAtoms; i++ ){` |
|   80623 |  6523 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|   40305 |  6524 | `		}` |
|       - |  6525 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|       - |  6526 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|   80499 |  6527 | `		if( bShortNullable && bAnyIntersection ){` |
|     ! 0 |  6528 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6529 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|     ! 0 |  6530 | `			return SXERR_SYNTAX;` |
|       - |  6531 | `		}` |
|  161123 |  6532 | `		for( i = 0; i < nAtoms; i++ ){` |
|       - |  6533 | `			/* Intersection members must be class/interface types (PHP rejects` |
|       - |  6534 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|       - |  6535 | ``			 * `true`/`false` in an intersection). */`` |
|   80641 |  6536 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|      38 |  6537 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|      38 |  6538 | `				if( bClassLike ){` |
|      36 |  6539 | `					SyString *pC = &aAtoms[i].sClass;` |
|      32 |  6540 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|      32 |  6541 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|      32 |  6542 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|      36 |  6543 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|     ! 0 |  6544 | `						bClassLike = 0;` |
|     ! 0 |  6545 | `					}` |
|      16 |  6546 | `				}` |
|      38 |  6547 | `				if( !bClassLike ){` |
|       - |  6548 | `					const char *zName; sxu32 nName;` |
|       3 |  6549 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6550 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6551 | `					}else{` |
|       3 |  6552 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|       - |  6553 | `					}` |
|       4 |  6554 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6555 | `						"Type %.*s cannot be part of an intersection type",` |
|       1 |  6556 | `						(int)nName, zName);` |
|       3 |  6557 | `					return SXERR_SYNTAX;` |
|       - |  6558 | `				}` |
|      16 |  6559 | `			}` |
|   80639 |  6560 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|     157 |  6561 | `				if( nAtoms > 1 ){` |
|       3 |  6562 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6563 | `						"Void can only be used as a standalone type");` |
|       3 |  6564 | `					return SXERR_SYNTAX;` |
|       - |  6565 | `				}` |
|     155 |  6566 | `				if( !bAllowVoid ){` |
|     ! 0 |  6567 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6568 | `						"void cannot be used here");` |
|     ! 0 |  6569 | `					return SXERR_SYNTAX;` |
|       - |  6570 | `				}` |
|     155 |  6571 | `				if( bShortNullable ){` |
|     ! 0 |  6572 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6573 | `						"Void type cannot be nullable");` |
|     ! 0 |  6574 | `					return SXERR_SYNTAX;` |
|       - |  6575 | `				}` |
|      75 |  6576 | `			}` |
|   80637 |  6577 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|       - |  6578 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|       - |  6579 | `				 * type (never = the function does not return). Mirrors the void` |
|       - |  6580 | `				 * validation above; accepted here and enforced at compile time` |
|       - |  6581 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|      24 |  6582 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|       - |  6583 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|       - |  6584 | `					 * same as any other non-standalone use. */` |
|       5 |  6585 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6586 | `						"never can only be used as a standalone type");` |
|       5 |  6587 | `					return SXERR_SYNTAX;` |
|       - |  6588 | `				}` |
|      19 |  6589 | `				if( !bAllowVoid ){` |
|       - |  6590 | `					/* Return-only: params call with bAllowVoid=0. */` |
|       3 |  6591 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6592 | `						"never cannot be used as a parameter type");` |
|       3 |  6593 | `					return SXERR_SYNTAX;` |
|       - |  6594 | `				}` |
|       7 |  6595 | `			}` |
|   80631 |  6596 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|      32 |  6597 | `				bExplicitNull = 1;` |
|      18 |  6598 | `			}else{` |
|   80603 |  6599 | `				bHasNonNull = 1;` |
|       - |  6600 | `			}` |
|       - |  6601 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|       - |  6602 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|       - |  6603 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|       - |  6604 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|       - |  6605 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|   80811 |  6606 | `			for( j = 0; j < i; j++ ){` |
|     187 |  6607 | `				int bDup = 0;` |
|     187 |  6608 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|     359 |  6609 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|     182 |  6610 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|     187 |  6611 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|     179 |  6612 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      41 |  6613 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      34 |  6614 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|      38 |  6615 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|      16 |  6616 | `								aAtoms[j].sClass.zString,` |
|      32 |  6617 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|     ! 0 |  6618 | `							bDup = 1;` |
|     ! 0 |  6619 | `						}` |
|      22 |  6620 | `					}else{` |
|       3 |  6621 | `						bDup = 1;` |
|       - |  6622 | `					}` |
|      18 |  6623 | `				}` |
|     179 |  6624 | `				if( bDup ){` |
|       - |  6625 | `					const char *zName;` |
|       - |  6626 | `					sxu32 nName;` |
|       3 |  6627 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6628 | `						zName = aAtoms[i].sClass.zString;` |
|     ! 0 |  6629 | `						nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6630 | `					}else{` |
|       3 |  6631 | `						zName = aAtoms[i].zCanon;` |
|       3 |  6632 | `						nName = aAtoms[i].nCanon;` |
|       - |  6633 | `					}` |
|       4 |  6634 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       1 |  6635 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|       3 |  6636 | `					return SXERR_SYNTAX;` |
|       - |  6637 | `				}` |
|      91 |  6638 | `			}` |
|   40317 |  6639 | `		}` |
|   80487 |  6640 | `		if( !bHasNonNull && bExplicitNull ){` |
|       7 |  6641 | `			if( bShortNullable ){` |
|       - |  6642 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|     ! 0 |  6643 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6644 | `					"Null can not be used as a standalone type");` |
|     ! 0 |  6645 | `				return SXERR_SYNTAX;` |
|       - |  6646 | `			}` |
|       - |  6647 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|       - |  6648 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|       - |  6649 | `			 * path below leaves *pnType untouched when there is no non-null` |
|       - |  6650 | `			 * atom, so set it here. */` |
|       7 |  6651 | `			*pnType = MEMOBJ_NULL;` |
|       3 |  6652 | `		}` |
|       - |  6653 | `	}` |
|       - |  6654 | `	/* Compute nullability flag */` |
|   80487 |  6655 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      97 |  6656 | `		*piTypeFlags \|= iNullableFlag;` |
|      46 |  6657 | `	}` |
|       - |  6658 | `	/* Build canonical type text */` |
|   80487 |  6659 | `	if( pTypeText ){` |
|       - |  6660 | `		SyBlob sBlob;` |
|   80487 |  6661 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|  120696 |  6662 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   40241 |  6663 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   80487 |  6664 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|  120482 |  6665 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   80318 |  6666 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   80323 |  6667 | `			if( zDup ){` |
|   80323 |  6668 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   40159 |  6669 | `			}` |
|   40159 |  6670 | `		}` |
|   80487 |  6671 | `		SyBlobRelease(&sBlob);` |
|   40241 |  6672 | `	}` |
|       - |  6673 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|       - |  6674 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|       - |  6675 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|       - |  6676 | `	{` |
|   80487 |  6677 | `		int nNonNull = 0;` |
|   80487 |  6678 | `		int iNonNullIdx = -1;` |
|       - |  6679 | `		int i;` |
|  161103 |  6680 | `		for( i = 0; i < nAtoms; i++ ){` |
|   80621 |  6681 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   80593 |  6682 | `				nNonNull++;` |
|   80593 |  6683 | `				iNonNullIdx = i;` |
|   40294 |  6684 | `			}` |
|   40313 |  6685 | `		}` |
|   80487 |  6686 | `		if( nNonNull <= 1 ){` |
|       - |  6687 | `			/* Fast path: store as single type. */` |
|   80395 |  6688 | `			if( iNonNullIdx >= 0 ){` |
|   80389 |  6689 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   80389 |  6690 | `				if( pA->nType == SXU32_HIGH ){` |
|   21938 |  6691 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    7311 |  6692 | `						pA->sClass.zString, pA->sClass.nByte);` |
|   14627 |  6693 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|   14627 |  6694 | `					*pnType = SXU32_HIGH;` |
|   14627 |  6695 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   73078 |  6696 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|     155 |  6697 | `					*pnType = MEMOBJ_VOID;` |
|   65692 |  6698 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|      16 |  6699 | `					*pnType = MEMOBJ_NEVER;` |
|       9 |  6700 | `				}else{` |
|   65603 |  6701 | `					*pnType = pA->nType;` |
|       - |  6702 | `				}` |
|   40192 |  6703 | `			}` |
|   40200 |  6704 | `		}else{` |
|       - |  6705 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      97 |  6706 | `			*piTypeFlags \|= iUnionFlag;` |
|     311 |  6707 | `			for( i = 0; i < nAtoms; i++ ){` |
|       - |  6708 | `				ph7_type_alt sAlt;` |
|     219 |  6709 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     209 |  6710 | `				SyZero(&sAlt, sizeof(sAlt));` |
|     209 |  6711 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|     209 |  6712 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|     116 |  6713 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      37 |  6714 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      79 |  6715 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      79 |  6716 | `					sAlt.nType = SXU32_HIGH;` |
|      79 |  6717 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      42 |  6718 | `				}else{` |
|     135 |  6719 | `					sAlt.nType = aAtoms[i].nType;` |
|     135 |  6720 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|       - |  6721 | `				}` |
|     209 |  6722 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|     107 |  6723 | `			}` |
|       - |  6724 | `		}` |
|       - |  6725 | `	}` |
|   80487 |  6726 | `	return SXRET_OK;` |
|   40254 |  6727 | `}` |
|       - |  6728 |  |
|       - |  6729 | `/*` |
|       - |  6730 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - |  6731 | `` * pGen->pIn should point to the token after `)`.`` |
|       - |  6732 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - |  6733 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - |  6734 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|       - |  6735 | `` *          and union types `: T\|U`.`` |
|       - |  6736 | ` */` |
|  320530 |  6737 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       5 |  6738 | `{` |
|  320535 |  6739 | `	sxi32 iFlags = 0;` |
|       - |  6740 | `	sxi32 rc;` |
|       - |  6741 | `	sxu32 nLine;` |
|  320535 |  6742 | `	pFunc->nReturnType = 0;` |
|  320535 |  6743 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  320535 |  6744 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|  320535 |  6745 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  320035 |  6746 | `		return SXRET_OK;` |
|       - |  6747 | `	}` |
|     505 |  6748 | `	pGen->pIn++; /* Skip ':' */` |
|     505 |  6749 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6750 | `		return SXRET_OK;` |
|       - |  6751 | `	}` |
|     505 |  6752 | `	nLine = pGen->pIn->nLine;` |
|     505 |  6753 | `	rc = GenStateParseUnionTypeDecl(` |
|     250 |  6754 | `		pGen,` |
|     250 |  6755 | `		&pFunc->nReturnType,` |
|     250 |  6756 | `		&pFunc->sReturnClass,` |
|     250 |  6757 | `		&pFunc->aReturnUnion,` |
|       - |  6758 | `		&iFlags,` |
|     250 |  6759 | `		&pFunc->sReturnTypeName,` |
|       - |  6760 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|       - |  6761 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|       - |  6762 | `		/* iUnionFlag */ 0,` |
|       - |  6763 | `		/* bAllowVoid */ 1,` |
|     250 |  6764 | `		nLine);` |
|     505 |  6765 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6766 | `		return SXERR_ABORT;` |
|       - |  6767 | `	}` |
|     505 |  6768 | `	if( rc == SXERR_CORRUPT ){` |
|       - |  6769 | `		/* Error already reported */` |
|     ! 0 |  6770 | `		return SXERR_SYNTAX;` |
|       - |  6771 | `	}` |
|     505 |  6772 | `	if( rc == SXERR_SYNTAX ){` |
|       8 |  6773 | `		if( pGen->pIn < pGen->pEnd ){` |
|      11 |  6774 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6775 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       6 |  6776 | `				&pGen->pIn->sData);` |
|       5 |  6777 | `		}else{` |
|     ! 0 |  6778 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|       - |  6779 | `				"syntax error, unexpected end of file in return type declaration");` |
|       - |  6780 | `		}` |
|       8 |  6781 | `		return SXERR_SYNTAX;` |
|       - |  6782 | `	}` |
|     499 |  6783 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     499 |  6784 | `	return SXRET_OK;` |
|  160270 |  6785 | `}` |
|       - |  6786 |  |
|   48326 |  6787 | `static sxi32 GenStateCompileFunc(` |
|       - |  6788 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  6789 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - |  6790 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  6791 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - |  6792 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - |  6793 | `	)` |
|       5 |  6794 | `{` |
|       - |  6795 | `	ph7_vm_func *pFunc;` |
|       - |  6796 | `	SyToken *pEnd;` |
|       - |  6797 | `	sxu32 nLine;` |
|       - |  6798 | `	char *zName;` |
|       - |  6799 | `	sxi32 rc;` |
|       - |  6800 | `	/* Extract line number */` |
|   48331 |  6801 | `	nLine = pGen->pIn->nLine;` |
|       - |  6802 | `	/* Jump the left parenthesis '(' */` |
|   48331 |  6803 | `	pGen->pIn++;` |
|       - |  6804 | `	/* Delimit the function signature */` |
|   48331 |  6805 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   48331 |  6806 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  6807 | `		/* Syntax error */` |
|       8 |  6808 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       8 |  6809 | `		if( rc == SXERR_ABORT ){` |
|       - |  6810 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6811 | `			return SXERR_ABORT;` |
|       - |  6812 | `		}` |
|       8 |  6813 | `		pGen->pIn = pGen->pEnd;` |
|       8 |  6814 | `		return SXRET_OK;` |
|       - |  6815 | `	}` |
|       - |  6816 | `	/* Create the function state */` |
|   48325 |  6817 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   48325 |  6818 | `	if( pFunc == 0 ){` |
|     ! 0 |  6819 | `		goto OutOfMem;` |
|       - |  6820 | `	}` |
|       - |  6821 | `	/* Build the function name, prepending namespace if active */` |
|   48332 |  6822 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - |  6823 | `		SyBlob sFQN;` |
|       - |  6824 | `		sxu32 nLen;` |
|      16 |  6825 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 |  6826 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 |  6827 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 |  6828 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 |  6829 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 |  6830 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 |  6831 | `		SyBlobRelease(&sFQN);` |
|      16 |  6832 | `		if( zName == 0 ){` |
|     ! 0 |  6833 | `			goto OutOfMem;` |
|       - |  6834 | `		}` |
|      16 |  6835 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 |  6836 | `	}else{` |
|   48311 |  6837 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   48311 |  6838 | `		if( zName == 0 ){` |
|     ! 0 |  6839 | `			goto OutOfMem;` |
|       - |  6840 | `		}` |
|   48311 |  6841 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - |  6842 | `	}` |
|   48325 |  6843 | `	if( pGen->pIn < pEnd ){` |
|       - |  6844 | `		/* Collect function arguments */` |
|   33341 |  6845 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   33341 |  6846 | `		if( rc == SXERR_ABORT ){` |
|       - |  6847 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6848 | `			return SXERR_ABORT;` |
|       - |  6849 | `		}` |
|   16668 |  6850 | `	}` |
|       - |  6851 | `	/* Point past ')' and parse optional return type ': type' */` |
|   48325 |  6852 | `	pGen->pIn = &pEnd[1];` |
|       - |  6853 | `	{` |
|   48325 |  6854 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   48325 |  6855 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  6856 | `			return SXERR_ABORT;` |
|   48325 |  6857 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       8 |  6858 | `			return SXERR_SYNTAX;` |
|       - |  6859 | `		}` |
|       - |  6860 | `	}` |
|   48319 |  6861 | `	if( bHandleClosure ){` |
|       - |  6862 | `		ph7_vm_func_closure_env sEnv;` |
|     299 |  6863 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     294 |  6864 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     161 |  6865 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      23 |  6866 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  6867 | `				/* Closure,record environment variable */` |
|      23 |  6868 | `				pGen->pIn++;` |
|      23 |  6869 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  6870 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 |  6871 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  6872 | `						return SXERR_ABORT;` |
|       - |  6873 | `					}` |
|     ! 0 |  6874 | `				}` |
|      23 |  6875 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - |  6876 | `				/* Compile until we hit the first closing parenthesis */` |
|      45 |  6877 | `				while( pGen->pIn < pGen->pEnd ){` |
|      45 |  6878 | `					int iFlagsLocal = 0;` |
|      45 |  6879 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      23 |  6880 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      23 |  6881 | `						break;` |
|       - |  6882 | `					}` |
|      27 |  6883 | `					nLineLocal = pGen->pIn->nLine;` |
|      27 |  6884 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - |  6885 | `						/* Pass by reference,record that */` |
|     ! 0 |  6886 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - |  6887 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - |  6888 | `							);` |
|     ! 0 |  6889 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 |  6890 | `						pGen->pIn++;` |
|     ! 0 |  6891 | `					}` |
|      22 |  6892 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      27 |  6893 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6894 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  6895 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 |  6896 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  6897 | `								return SXERR_ABORT;` |
|       - |  6898 | `							}` |
|       - |  6899 | `							/* Find the closing parenthesis */` |
|     ! 0 |  6900 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6901 | `								pGen->pIn++;` |
|     ! 0 |  6902 | `							}` |
|     ! 0 |  6903 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 |  6904 | `								pGen->pIn++;` |
|     ! 0 |  6905 | `							}` |
|     ! 0 |  6906 | `							break;` |
|       - |  6907 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 |  6908 | `					}else{` |
|       - |  6909 | `						SyString *pNameLocal;` |
|       - |  6910 | `						char *zDup;` |
|       - |  6911 | `						/* Duplicate variable name */` |
|      27 |  6912 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      27 |  6913 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      27 |  6914 | `						if( zDup ){` |
|       - |  6915 | `							/* Zero the structure */` |
|      27 |  6916 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      27 |  6917 | `							sEnv.iFlags = iFlagsLocal;` |
|      27 |  6918 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      27 |  6919 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      27 |  6920 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 |  6921 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 |  6922 | `									got_this = 1;` |
|     ! 0 |  6923 | `							}` |
|       - |  6924 | `							/* Save imported variable */` |
|      27 |  6925 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      16 |  6926 | `						}else{` |
|     ! 0 |  6927 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6928 | `							 return SXERR_ABORT;` |
|       - |  6929 | `						}` |
|       - |  6930 | `					}` |
|      27 |  6931 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      33 |  6932 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6933 | `						/* Ignore trailing commas */` |
|       7 |  6934 | `						pGen->pIn++;` |
|       1 |  6935 | `					}` |
|       5 |  6936 | `				}` |
|      23 |  6937 | `				if( !got_this ){` |
|       - |  6938 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - |  6939 | `					 * available to the closure environment.` |
|       - |  6940 | `					 */` |
|      23 |  6941 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      23 |  6942 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      23 |  6943 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      23 |  6944 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      23 |  6945 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       9 |  6946 | `				}` |
|      23 |  6947 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - |  6948 | `					/* Mark as closure */` |
|      23 |  6949 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       9 |  6950 | `				}` |
|       9 |  6951 | `		}` |
|     147 |  6952 | `	}` |
|       - |  6953 | `	/* Compile the body */` |
|   48319 |  6954 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   48319 |  6955 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6956 | `		return SXERR_ABORT;` |
|       - |  6957 | `	}` |
|   48319 |  6958 | `	if( ppFunc ){` |
|     299 |  6959 | `		*ppFunc = pFunc;` |
|     147 |  6960 | `	}` |
|   48319 |  6961 | `	rc = SXRET_OK;` |
|   48319 |  6962 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - |  6963 | `		/* Finally register the function */` |
|   48301 |  6964 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   24148 |  6965 | `	}` |
|   48319 |  6966 | `	if( rc == SXRET_OK ){` |
|   48319 |  6967 | `		return SXRET_OK;` |
|       - |  6968 | `	}` |
|       - |  6969 | `	/* Fall through if something goes wrong */` |
|     ! 0 |  6970 | `OutOfMem:` |
|       - |  6971 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  6972 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  6973 | `	 */` |
|     ! 0 |  6974 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  6975 | `	return SXERR_ABORT;` |
|   24168 |  6976 | `}` |
|       - |  6977 | `/*` |
|       - |  6978 | ` * Compile a standard PHP function.` |
|       - |  6979 | ` *  Refer to the block-comment above for more information.` |
|       - |  6980 | ` */` |
|   48040 |  6981 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       5 |  6982 | `{` |
|       - |  6983 | `	SyString *pName;` |
|       - |  6984 | `	sxi32 iFlags;` |
|       - |  6985 | `	sxu32 nLine;` |
|       - |  6986 | `	sxi32 rc;` |
|       - |  6987 |  |
|   48045 |  6988 | `	nLine = pGen->pIn->nLine;` |
|   48045 |  6989 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   48045 |  6990 | `	iFlags = 0;` |
|   48045 |  6991 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  6992 | `		/* Return by reference,remember that */` |
|       7 |  6993 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  6994 | `		/* Jump the '&' token */` |
|       7 |  6995 | `		pGen->pIn++;` |
|       3 |  6996 | `	}` |
|   48045 |  6997 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  6998 | `		/* Invalid function name */` |
|       8 |  6999 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       8 |  7000 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7001 | `			return SXERR_ABORT;` |
|       - |  7002 | `		}` |
|       - |  7003 | `		/* Sychronize with the next semi-colon or braces*/` |
|      22 |  7004 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      16 |  7005 | `			pGen->pIn++;` |
|       2 |  7006 | `		}` |
|       8 |  7007 | `		return SXRET_OK;` |
|       - |  7008 | `	}` |
|   48039 |  7009 | `	pName = &pGen->pIn->sData;` |
|   48039 |  7010 | `	nLine = pGen->pIn->nLine;` |
|       - |  7011 | `	/* Jump the function name */` |
|   48039 |  7012 | `	pGen->pIn++;` |
|   48039 |  7013 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  7014 | `		/* Syntax error */` |
|       3 |  7015 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 |  7016 | `		if( rc == SXERR_ABORT ){` |
|       - |  7017 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7018 | `			return SXERR_ABORT;` |
|       - |  7019 | `		}` |
|       - |  7020 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 |  7021 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  7022 | `			pGen->pIn++;` |
|     ! 0 |  7023 | `		}` |
|       3 |  7024 | `		return SXRET_OK;` |
|       - |  7025 | `	}` |
|       - |  7026 | `	/* Compile function body */` |
|   48037 |  7027 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   48037 |  7028 | `	return rc;` |
|   24025 |  7029 | `}` |
|       - |  7030 | `/*` |
|       - |  7031 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  7032 | ` * According to the PHP language reference manual` |
|       - |  7033 | ` *  Visibility:` |
|       - |  7034 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  7035 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  7036 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  7037 | ` *  Members declared protected can be accessed only within the class` |
|       - |  7038 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  7039 | ` *  may only be accessed by the class that defines the member.` |
|       - |  7040 | ` */` |
|  348684 |  7041 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       5 |  7042 | `{` |
|  348689 |  7043 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|   21787 |  7044 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  326907 |  7045 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   47015 |  7046 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  7047 | `	}` |
|       - |  7048 | `	/* Assume public by default */` |
|  279897 |  7049 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  174347 |  7050 | `}` |
|       - |  7051 | `/*` |
|       - |  7052 | ` * Compile a class constant.` |
|       - |  7053 | ` * According to the PHP language reference manual` |
|       - |  7054 | ` *  Class Constants` |
|       - |  7055 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - |  7056 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - |  7057 | ` *   you don't use the $ symbol to declare or use them.` |
|       - |  7058 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - |  7059 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - |  7060 | ` *   It's also possible for interfaces to have constants.` |
|       - |  7061 | ` * Symisc eXtension.` |
|       - |  7062 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - |  7063 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  7064 | ` *  Example:` |
|       - |  7065 | ` *   class Test{` |
|       - |  7066 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  7067 | ` *   };` |
|       - |  7068 | ` *   var_dump(TEST::MyConst);` |
|       - |  7069 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  7070 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  7071 | ` */` |
|       - |  7072 | `/*` |
|       - |  7073 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|       - |  7074 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|       - |  7075 | ` * token immediately followed by '='. Anything else with a leading type token` |
|       - |  7076 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|       - |  7077 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|       - |  7078 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|       - |  7079 | ` */` |
|      92 |  7080 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|       5 |  7081 | `{` |
|       - |  7082 | `	SyToken *p0, *p1;` |
|      97 |  7083 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7084 | `		return 0;` |
|       - |  7085 | `	}` |
|      97 |  7086 | `	p0 = pGen->pIn;` |
|       - |  7087 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|      97 |  7088 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|     ! 0 |  7089 | `		return 1;` |
|       - |  7090 | `	}` |
|      97 |  7091 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|       5 |  7092 | `		return 1;` |
|       - |  7093 | `	}` |
|       - |  7094 | `	/* A name-like first token begins a type only when followed by another` |
|       - |  7095 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|       - |  7096 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|      93 |  7097 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      93 |  7098 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|      93 |  7099 | `		if( p1 ){` |
|      93 |  7100 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|      34 |  7101 | `				return 1;` |
|       - |  7102 | `			}` |
|      62 |  7103 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|       5 |  7104 | `				return 1;` |
|       - |  7105 | `			}` |
|      27 |  7106 | `		}` |
|      27 |  7107 | `	}` |
|      58 |  7108 | `	return 0;` |
|      51 |  7109 | `}` |
|       - |  7110 | `/*` |
|       - |  7111 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|       - |  7112 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|       - |  7113 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|       - |  7114 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|       - |  7115 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|       - |  7116 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|       - |  7117 | ` * Peek only; never consumes tokens.` |
|       - |  7118 | ` */` |
|      24 |  7119 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|       4 |  7120 | `{` |
|      28 |  7121 | `	SyToken *p = pGen->pIn;` |
|      39 |  7122 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      20 |  7123 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|       3 |  7124 | `		p++; /* skip leading unary sign(s) */` |
|       1 |  7125 | `	}` |
|      28 |  7126 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|      23 |  7127 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|       - |  7128 | `	}` |
|       6 |  7129 | `	p++;` |
|       - |  7130 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|       6 |  7131 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|      16 |  7132 | `}` |
|       - |  7133 | `/*` |
|       - |  7134 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|       - |  7135 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|       - |  7136 | `` * `$o->new`), not a `new` expression.`` |
|       - |  7137 | ` */` |
|       6 |  7138 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|       3 |  7139 | `{` |
|       - |  7140 | `	sxi32 iOp;` |
|       9 |  7141 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|     ! 0 |  7142 | `		return 0;` |
|       - |  7143 | `	}` |
|       9 |  7144 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|       9 |  7145 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|       6 |  7146 | `}` |
|       - |  7147 | `/*` |
|       - |  7148 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|       - |  7149 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|       - |  7150 | ` * interface-constant and (instance/static) property-default initializers` |
|       - |  7151 | ` * ("New expressions are not supported in this context") while still allowing it` |
|       - |  7152 | ` * in global constants, parameter defaults and static-local initializers (which` |
|       - |  7153 | ` * are compiled by different functions and left untouched). The scan is` |
|       - |  7154 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|       - |  7155 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|       - |  7156 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|       - |  7157 | ` *` |
|       - |  7158 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|       - |  7159 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|       - |  7160 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|       - |  7161 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|       - |  7162 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|       - |  7163 | ` */` |
|   22260 |  7164 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|       5 |  7165 | `{` |
|   22265 |  7166 | `	SyToken *p = pGen->pIn;` |
|   22265 |  7167 | `	int iDepth = 0;` |
|   66995 |  7168 | `	while( p < pGen->pEnd ){` |
|   66995 |  7169 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|   22257 |  7170 | `			break; /* end of this initializer */` |
|       - |  7171 | `		}` |
|   44743 |  7172 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|   22379 |  7173 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      10 |  7174 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|       - |  7175 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|       - |  7176 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|       - |  7177 | `			 * expression. */` |
|       3 |  7178 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|       3 |  7179 | `			p++;` |
|       3 |  7180 | `			if( bArrow ){` |
|       - |  7181 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|       - |  7182 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|       3 |  7183 | `				int iBase = iDepth;` |
|      17 |  7184 | `				while( p < pGen->pEnd ){` |
|      17 |  7185 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       5 |  7186 | `						iDepth++;` |
|      15 |  7187 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       5 |  7188 | `						if( iDepth <= iBase ){` |
|     ! 0 |  7189 | `							break; /* closes an enclosing group, not the fn's own */` |
|       - |  7190 | `						}` |
|       5 |  7191 | `						iDepth--;` |
|      11 |  7192 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|       3 |  7193 | `						break;` |
|       - |  7194 | `					}` |
|      15 |  7195 | `					p++;` |
|       1 |  7196 | `				}` |
|       2 |  7197 | `			}else{` |
|       - |  7198 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|       - |  7199 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|       - |  7200 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|       - |  7201 | `				 * then skip the balanced brace block. */` |
|     ! 0 |  7202 | `				int iLocal = 0;` |
|     ! 0 |  7203 | `				while( p < pGen->pEnd ){` |
|     ! 0 |  7204 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|     ! 0 |  7205 | `						break; /* body brace */` |
|       - |  7206 | `					}` |
|     ! 0 |  7207 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  7208 | `						iLocal++;` |
|     ! 0 |  7209 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  7210 | `						if( iLocal > 0 ){` |
|     ! 0 |  7211 | `							iLocal--;` |
|     ! 0 |  7212 | `						}` |
|     ! 0 |  7213 | `					}` |
|     ! 0 |  7214 | `					p++;` |
|     ! 0 |  7215 | `				}` |
|     ! 0 |  7216 | `				if( p < pGen->pEnd ){` |
|     ! 0 |  7217 | `					int iBrace = 0; /* p is on the body '{' */` |
|     ! 0 |  7218 | `					while( p < pGen->pEnd ){` |
|     ! 0 |  7219 | `						if( p->nType & PH7_TK_OCB ){` |
|     ! 0 |  7220 | `							iBrace++;` |
|     ! 0 |  7221 | `						}else if( p->nType & PH7_TK_CCB ){` |
|     ! 0 |  7222 | `							iBrace--;` |
|     ! 0 |  7223 | `							if( iBrace == 0 ){` |
|     ! 0 |  7224 | `								p++;` |
|     ! 0 |  7225 | `								break;` |
|       - |  7226 | `							}` |
|     ! 0 |  7227 | `						}` |
|     ! 0 |  7228 | `						p++;` |
|     ! 0 |  7229 | `					}` |
|     ! 0 |  7230 | `				}` |
|       - |  7231 | `			}` |
|       3 |  7232 | `			continue;` |
|       - |  7233 | `		}` |
|   44741 |  7234 | `		if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      69 |  7235 | `			iDepth++;` |
|   44709 |  7236 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      67 |  7237 | `			if( iDepth > 0 ){` |
|      67 |  7238 | `				iDepth--;` |
|      31 |  7239 | `			}` |
|   44646 |  7240 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|   22243 |  7241 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|       - |  7242 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|       - |  7243 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|       - |  7244 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|      11 |  7245 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|      11 |  7246 | `				return 1;` |
|       - |  7247 | `			}` |
|     ! 0 |  7248 | `		}` |
|   44733 |  7249 | `		p++;` |
|       5 |  7250 | `	}` |
|   22257 |  7251 | `	return 0;` |
|   11135 |  7252 | `}` |
|       - |  7253 | `/*` |
|       - |  7254 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|       - |  7255 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|       - |  7256 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|       - |  7257 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|       - |  7258 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|       - |  7259 | ` * share the same backing.` |
|       - |  7260 | ` */` |
|     212 |  7261 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|       - |  7262 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|       5 |  7263 | `{` |
|     217 |  7264 | `	pAttr->nType = nType;` |
|     217 |  7265 | `	pAttr->sClass = *pClass;` |
|     217 |  7266 | `	pAttr->sTypeName = *pTypeName;` |
|     217 |  7267 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7268 | `		sxu32 i;` |
|      66 |  7269 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      46 |  7270 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|      46 |  7271 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      25 |  7272 | `		}` |
|      10 |  7273 | `	}` |
|     217 |  7274 | `}` |
|      92 |  7275 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7276 | `{` |
|      97 |  7277 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7278 | `	SySet *pInstrContainer;` |
|       - |  7279 | `	ph7_class_attr *pCons;` |
|       - |  7280 | `	SyString *pName;` |
|       - |  7281 | `	sxi32 rc;` |
|      97 |  7282 | `	sxu32 nType = 0;` |
|       - |  7283 | `	SyString sTypeClass;` |
|       - |  7284 | `	SyString sTypeText;` |
|       - |  7285 | `	SySet aUnionAlts;` |
|      97 |  7286 | `	sxi32 iTypeFlags = 0;` |
|      97 |  7287 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|      97 |  7288 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|      97 |  7289 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7290 | `	/* Extract visibility level */` |
|      97 |  7291 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7292 | `	/* Mark as constant */` |
|      97 |  7293 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      97 |  7294 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       - |  7295 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|       - |  7296 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|     116 |  7297 | `	if( GenStateClassConstHasType(pGen) ){` |
|      61 |  7298 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|      38 |  7299 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|       - |  7300 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|       - |  7301 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|       - |  7302 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|       - |  7303 | `		 * and success paths release. */` |
|      42 |  7304 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7305 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7306 | `			goto Synchronize;` |
|      42 |  7307 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7308 | `			return SXERR_ABORT;` |
|      42 |  7309 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7310 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  7311 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|     ! 0 |  7312 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7313 | `				return SXERR_ABORT;` |
|       - |  7314 | `			}` |
|     ! 0 |  7315 | `			goto Synchronize;` |
|       - |  7316 | `		}` |
|      42 |  7317 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      19 |  7318 | `	}` |
|      46 |  7319 | `loop:` |
|      99 |  7320 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7321 | `		/* Invalid constant name */` |
|     ! 0 |  7322 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  7323 | `		if( rc == SXERR_ABORT ){` |
|       - |  7324 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7325 | `			return SXERR_ABORT;` |
|       - |  7326 | `		}` |
|     ! 0 |  7327 | `		goto Synchronize;` |
|       - |  7328 | `	}` |
|       - |  7329 | `	/* Peek constant name */` |
|      99 |  7330 | `	pName = &pGen->pIn->sData;` |
|       - |  7331 | `	/* Make sure the constant name isn't reserved */` |
|      99 |  7332 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  7333 | `		/* Reserved constant name */` |
|     ! 0 |  7334 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 |  7335 | `		if( rc == SXERR_ABORT ){` |
|       - |  7336 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7337 | `			return SXERR_ABORT;` |
|       - |  7338 | `		}` |
|     ! 0 |  7339 | `		goto Synchronize;` |
|       - |  7340 | `	}` |
|       - |  7341 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|      99 |  7342 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      61 |  7343 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|      38 |  7344 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      19 |  7345 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|      42 |  7346 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7347 | `			return SXERR_ABORT;` |
|      42 |  7348 | `		}else if( rc != SXRET_OK ){` |
|       3 |  7349 | `			goto Synchronize;` |
|       - |  7350 | `		}` |
|      18 |  7351 | `	}` |
|       - |  7352 | `	/* Advance the stream cursor */` |
|      97 |  7353 | `	pGen->pIn++;` |
|      97 |  7354 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  7355 | `		/* Invalid declaration */` |
|     ! 0 |  7356 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  7357 | `		if( rc == SXERR_ABORT ){` |
|       - |  7358 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7359 | `			return SXERR_ABORT;` |
|       - |  7360 | `		}` |
|     ! 0 |  7361 | `		goto Synchronize;` |
|       - |  7362 | `	}` |
|      97 |  7363 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  7364 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|       - |  7365 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|       - |  7366 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|       - |  7367 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|     104 |  7368 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|      39 |  7369 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|       8 |  7370 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7371 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|       2 |  7372 | `			&pClass->sName,pName,&sTypeText);` |
|       6 |  7373 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7374 | `			return SXERR_ABORT;` |
|       - |  7375 | `		}` |
|       6 |  7376 | `		goto Synchronize;` |
|       - |  7377 | `	}` |
|       - |  7378 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|       - |  7379 | `	 * constant initializer ("New expressions are not supported in this context").` |
|       - |  7380 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|      93 |  7381 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|       5 |  7382 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7383 | `			"New expressions are not supported in this context");` |
|       5 |  7384 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7385 | `			return SXERR_ABORT;` |
|       - |  7386 | `		}` |
|       5 |  7387 | `		goto Synchronize;` |
|       - |  7388 | `	}` |
|       - |  7389 | `	/* Allocate a new class attribute */` |
|      89 |  7390 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|      89 |  7391 | `	if( pCons == 0 ){` |
|     ! 0 |  7392 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7393 | `		return SXERR_ABORT;` |
|       - |  7394 | `	}` |
|      89 |  7395 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      35 |  7396 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      16 |  7397 | `	}` |
|       - |  7398 | `	/* Swap bytecode container */` |
|      89 |  7399 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      89 |  7400 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  7401 | `	/* Compile constant value.` |
|       - |  7402 | `	 */` |
|      89 |  7403 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      89 |  7404 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  7405 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  7406 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7407 | `			return SXERR_ABORT;` |
|       - |  7408 | `		}` |
|       1 |  7409 | `	}` |
|       - |  7410 | `	/* Emit the done instruction */` |
|      89 |  7411 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      89 |  7412 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      89 |  7413 | `	if( rc == SXERR_ABORT ){` |
|       - |  7414 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  7415 | `		return SXERR_ABORT;` |
|       - |  7416 | `	}` |
|       - |  7417 | `	/* All done,install the constant */` |
|      89 |  7418 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      89 |  7419 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7420 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7421 | `		return SXERR_ABORT;` |
|       - |  7422 | `	}` |
|      89 |  7423 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7424 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|       3 |  7425 | `		pGen->pIn++; /* Jump the comma */` |
|       3 |  7426 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  7427 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7428 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7429 | `				pTok--;` |
|     ! 0 |  7430 | `			}` |
|     ! 0 |  7431 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7432 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  7433 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7434 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7435 | `				return SXERR_ABORT;` |
|       - |  7436 | `			}` |
|     ! 0 |  7437 | `		}else{` |
|       3 |  7438 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       3 |  7439 | `				goto loop;` |
|       - |  7440 | `			}` |
|       - |  7441 | `		}` |
|     ! 0 |  7442 | `	}` |
|      87 |  7443 | `	SySetRelease(&aUnionAlts);` |
|      87 |  7444 | `	return SXRET_OK;` |
|       5 |  7445 | `Synchronize:` |
|      13 |  7446 | `	SySetRelease(&aUnionAlts);` |
|       - |  7447 | `	/* Synchronize with the first semi-colon */` |
|      45 |  7448 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      35 |  7449 | `		pGen->pIn++;` |
|       3 |  7450 | `	}` |
|      13 |  7451 | `	return SXERR_CORRUPT;` |
|      51 |  7452 | `}` |
|       - |  7453 | `/*` |
|       - |  7454 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  7455 | ` * According to the PHP language reference manual` |
|       - |  7456 | ` *  Properties` |
|       - |  7457 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  7458 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  7459 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  7460 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  7461 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  7462 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  7463 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  7464 | ` * Symisc eXtension.` |
|       - |  7465 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  7466 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  7467 | ` *  Example:` |
|       - |  7468 | ` *   class Test{` |
|       - |  7469 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  7470 | ` *   };` |
|       - |  7471 | ` *   var_dump(TEST::myVar);` |
|       - |  7472 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  7473 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  7474 | ` */` |
|       - |  7475 | `/*` |
|       - |  7476 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  7477 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  7478 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  7479 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  7480 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  7481 | ` */` |
|  188946 |  7482 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       5 |  7483 | `{` |
|  188951 |  7484 | `	SyToken *p = pStart;` |
|  188951 |  7485 | `	int bFirst = 1;` |
|  188951 |  7486 | `	if( p >= pEnd ) return 0;` |
|       - |  7487 | ``	/* Optional nullable `?` shorthand. */`` |
|  188951 |  7488 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      19 |  7489 | `		p++;` |
|      19 |  7490 | `		if( p >= pEnd ) return 0;` |
|       8 |  7491 | `	}` |
|       - |  7492 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|       - |  7493 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|       - |  7494 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|       - |  7495 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   94473 |  7496 | `	for(;;){` |
|  188969 |  7497 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|       - |  7498 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|       3 |  7499 | `			p++;` |
|       9 |  7500 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|       3 |  7501 | `			if( p >= pEnd ) return 0;` |
|       3 |  7502 | `			p++; /* skip ')' */` |
|       2 |  7503 | `		}else{` |
|       - |  7504 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|       - |  7505 | ``			 * then any `&`-joined intersection members. */`` |
|  188967 |  7506 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|  188967 |  7507 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  7508 | `				return 0;` |
|       - |  7509 | `			}` |
|       - |  7510 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|       - |  7511 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|       - |  7512 | `			 * may still appear at the initial dispatch site). */` |
|  188967 |  7513 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|  188921 |  7514 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  188993 |  7515 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|   11058 |  7516 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  188767 |  7517 | `					return 0;` |
|       - |  7518 | `				}` |
|      77 |  7519 | `			}` |
|     205 |  7520 | `			p++;` |
|     207 |  7521 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  7522 | `				p += 2;` |
|       1 |  7523 | `			}` |
|     303 |  7524 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|     208 |  7525 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  7526 | `				p++; /* skip '&' */` |
|       3 |  7527 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|       3 |  7528 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|       3 |  7529 | `				p++;` |
|       3 |  7530 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  7531 | `					p += 2;` |
|     ! 0 |  7532 | `				}` |
|       1 |  7533 | `			}` |
|       - |  7534 | `		}` |
|     207 |  7535 | `		bFirst = 0;` |
|     202 |  7536 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      23 |  7537 | `			&& p->sData.zString[0] == '\|' ){` |
|      22 |  7538 | ``			p++; /* next `\|`-separated part */`` |
|      22 |  7539 | `			continue;` |
|       - |  7540 | `		}` |
|     189 |  7541 | `		break;` |
|     ! 0 |  7542 | `	}` |
|     189 |  7543 | `	if( p >= pEnd ) return 0;` |
|     189 |  7544 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   94478 |  7545 | `}` |
|       - |  7546 |  |
|       - |  7547 | `/*` |
|       - |  7548 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  7549 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  7550 | ` * if not). Recognized forms:` |
|       - |  7551 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  7552 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  7553 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  7554 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  7555 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  7556 | ` * on unrecoverable error.` |
|       - |  7557 | ` *` |
|       - |  7558 | ` * When a type is parsed:` |
|       - |  7559 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  7560 | ` *   *pClass is set to the class name (for class types)` |
|       - |  7561 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  7562 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  7563 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  7564 | ` */` |
|     184 |  7565 | `static sxi32 GenStateParsePropertyType(` |
|       - |  7566 | `	ph7_gen_state *pGen,` |
|       - |  7567 | `	sxu32 *pnType,` |
|       - |  7568 | `	SyString *pClass,` |
|       - |  7569 | `	sxi32 *piTypeFlags,` |
|       - |  7570 | `	SyString *pTypeText,` |
|       - |  7571 | `	SySet *pAlts` |
|       5 |  7572 | `){` |
|     189 |  7573 | `	sxi32 iFlags = 0;` |
|       - |  7574 | `	sxi32 rc;` |
|     189 |  7575 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7576 | `		return SXRET_OK;` |
|       - |  7577 | `	}` |
|       - |  7578 | `	/* If the first token is '$', there's no type */` |
|     189 |  7579 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  7580 | `		return SXRET_OK;` |
|       - |  7581 | `	}` |
|     189 |  7582 | `	rc = GenStateParseUnionTypeDecl(` |
|      92 |  7583 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  7584 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  7585 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  7586 | `		/* bAllowVoid */ 0,` |
|     184 |  7587 | `		pGen->pIn->nLine);` |
|     189 |  7588 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7589 | `		return rc;` |
|       - |  7590 | `	}` |
|       - |  7591 | `	/* Verify next token is '$' (start of property name) */` |
|     189 |  7592 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7593 | `		return SXERR_SYNTAX;` |
|       - |  7594 | `	}` |
|     189 |  7595 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     189 |  7596 | `	return SXRET_OK;` |
|      97 |  7597 | `}` |
|       - |  7598 |  |
|       - |  7599 | `/*` |
|       - |  7600 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - |  7601 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - |  7602 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - |  7603 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - |  7604 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - |  7605 | ` * by the type parser itself before reaching here.` |
|       - |  7606 | ` *` |
|       - |  7607 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - |  7608 | ` * use in the error message.` |
|       - |  7609 | ` */` |
|     336 |  7610 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - |  7611 | `	sxu32 nType,` |
|       - |  7612 | `	const SyString *pClass,` |
|       - |  7613 | `	const char **pzName,` |
|       - |  7614 | `	sxu32 *pnName)` |
|       5 |  7615 | `{` |
|       - |  7616 | `	const char *z;` |
|       - |  7617 | `	sxu32 n;` |
|     341 |  7618 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     287 |  7619 | `		return 0;` |
|       - |  7620 | `	}` |
|      59 |  7621 | `	z = pClass->zString;` |
|      59 |  7622 | `	n = pClass->nByte;` |
|      59 |  7623 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       8 |  7624 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - |  7625 | `	}` |
|       - |  7626 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|       - |  7627 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|       - |  7628 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|      53 |  7629 | `	return 0;` |
|     173 |  7630 | `}` |
|       - |  7631 |  |
|       - |  7632 | `/*` |
|       - |  7633 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|       - |  7634 | ` * constant) — the main atom plus any union alternatives — against the` |
|       - |  7635 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|       - |  7636 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|       - |  7637 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|       - |  7638 | ` * type T" vs "Class constant C::X cannot have type T").` |
|       - |  7639 | ` *` |
|       - |  7640 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - |  7641 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - |  7642 | ` */` |
|     278 |  7643 | `static sxi32 GenStateValidateMemberType(` |
|       - |  7644 | `	ph7_gen_state *pGen,` |
|       - |  7645 | `	ph7_class *pClass,` |
|       - |  7646 | `	const SyString *pMemberName,` |
|       - |  7647 | `	sxu32 nType,` |
|       - |  7648 | `	const SyString *pTypeClass,` |
|       - |  7649 | `	const SyString *pTypeText,` |
|       - |  7650 | `	SySet *pUnionAlts,` |
|       - |  7651 | `	const char *zErrFmt,` |
|       - |  7652 | `	sxu32 nLine)` |
|       5 |  7653 | `{` |
|     283 |  7654 | `	const char *zBad = 0;` |
|     283 |  7655 | `	sxu32 nBad = 0;` |
|       - |  7656 | `	SyString sFallback;` |
|       - |  7657 | `	const SyString *pBad;` |
|       - |  7658 | `	sxi32 rc;` |
|     283 |  7659 | `	int bDisallowed = 0;` |
|     283 |  7660 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       5 |  7661 | `		bDisallowed = 1;` |
|     281 |  7662 | `	}else if( pUnionAlts ){` |
|       - |  7663 | `		sxu32 i;` |
|      88 |  7664 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      62 |  7665 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      62 |  7666 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 |  7667 | `				bDisallowed = 1;` |
|       3 |  7668 | `				break;` |
|       - |  7669 | `			}` |
|      32 |  7670 | `		}` |
|      14 |  7671 | `	}` |
|     283 |  7672 | `	if( !bDisallowed ){` |
|     277 |  7673 | `		return SXRET_OK;` |
|       - |  7674 | `	}` |
|       - |  7675 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - |  7676 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - |  7677 | `	 * canonical spelling if the type text is unavailable. */` |
|       8 |  7678 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       8 |  7679 | `		pBad = pTypeText;` |
|       5 |  7680 | `	}else{` |
|     ! 0 |  7681 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 |  7682 | `		pBad = &sFallback;` |
|       - |  7683 | `	}` |
|      11 |  7684 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       3 |  7685 | `		zErrFmt,` |
|       3 |  7686 | `		&pClass->sName,pMemberName,pBad);` |
|       8 |  7687 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7688 | `		return SXERR_ABORT;` |
|       - |  7689 | `	}` |
|       8 |  7690 | `	return SXERR_SYNTAX;` |
|     144 |  7691 | `}` |
|       - |  7692 | `/*` |
|       - |  7693 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|       - |  7694 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|       - |  7695 | ` * matched as a plain identifier in the class-member modifier position rather` |
|       - |  7696 | ` * than promoted to a lexer keyword.` |
|       - |  7697 | ` */` |
| 1672554 |  7698 | `static int GenStateIsReadonly(SyToken *pTok)` |
|       5 |  7699 | `{` |
| 1706755 |  7700 | `	return (pTok->nType & PH7_TK_ID)` |
|  870473 |  7701 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 1706750 |  7702 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|       5 |  7703 | `}` |
|   76550 |  7704 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7705 | `{` |
|   76555 |  7706 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7707 | `	ph7_class_attr *pAttr;` |
|       - |  7708 | `	SyString *pName;` |
|       - |  7709 | `	sxi32 rc;` |
|   76555 |  7710 | `	sxu32 nType = 0;` |
|       - |  7711 | `	SyString sTypeClass;` |
|       - |  7712 | `	SyString sTypeText;` |
|       - |  7713 | `	SySet aUnionAlts;` |
|   76555 |  7714 | `	sxi32 iTypeFlags = 0;` |
|   76555 |  7715 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   76555 |  7716 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   76555 |  7717 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7718 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|       - |  7719 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|       - |  7720 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   76555 |  7721 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|      21 |  7722 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  7723 | `	}` |
|       - |  7724 | `	/* Extract visibility level */` |
|   76555 |  7725 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7726 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   76647 |  7727 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     189 |  7728 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     189 |  7729 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7730 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7731 | `			goto Synchronize;` |
|     189 |  7732 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  7733 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7734 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 |  7735 | `				&pGen->pIn->sData);` |
|     ! 0 |  7736 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7737 | `				return SXERR_ABORT;` |
|       - |  7738 | `			}` |
|     ! 0 |  7739 | `			goto Synchronize;` |
|     189 |  7740 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7741 | `			return SXERR_ABORT;` |
|       - |  7742 | `		}` |
|      92 |  7743 | `	}` |
|     ! 0 |  7744 | `loop:` |
|   76559 |  7745 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7746 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 |  7747 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7748 | `			return SXERR_ABORT;` |
|       - |  7749 | `		}` |
|     ! 0 |  7750 | `		goto Synchronize;` |
|       - |  7751 | `	}` |
|   76559 |  7752 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   76559 |  7753 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - |  7754 | `		/* Invalid attribute name */` |
|     ! 0 |  7755 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 |  7756 | `		if( rc == SXERR_ABORT ){` |
|       - |  7757 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7758 | `			return SXERR_ABORT;` |
|       - |  7759 | `		}` |
|     ! 0 |  7760 | `		goto Synchronize;` |
|       - |  7761 | `	}` |
|       - |  7762 | `	/* Peek attribute name */` |
|   76559 |  7763 | `	pName = &pGen->pIn->sData;` |
|       - |  7764 | `	/* Advance the stream cursor */` |
|   76559 |  7765 | `	pGen->pIn++;` |
|   76559 |  7766 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - |  7767 | `		/* Invalid declaration */` |
|       3 |  7768 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 |  7769 | `		if( rc == SXERR_ABORT ){` |
|       - |  7770 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7771 | `			return SXERR_ABORT;` |
|       - |  7772 | `		}` |
|       3 |  7773 | `		goto Synchronize;` |
|       - |  7774 | `	}` |
|       - |  7775 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|       - |  7776 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   76557 |  7777 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|      39 |  7778 | `		const char *zRoErr = 0;` |
|      39 |  7779 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|       3 |  7780 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|      38 |  7781 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       6 |  7782 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|      35 |  7783 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|       6 |  7784 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|       2 |  7785 | `		}` |
|      39 |  7786 | `		if( zRoErr ){` |
|      13 |  7787 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|      13 |  7788 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7789 | `				return SXERR_ABORT;` |
|       - |  7790 | `			}` |
|      13 |  7791 | `			goto Synchronize;` |
|       - |  7792 | `		}` |
|      12 |  7793 | `	}` |
|       - |  7794 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - |  7795 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - |  7796 | `	 * by the type parser. */` |
|   76547 |  7797 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     278 |  7798 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - |  7799 | `			&sTypeText,` |
|     182 |  7800 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      91 |  7801 | `			"Property %z::$%z cannot have type %z",nLine);` |
|     187 |  7802 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7803 | `			return SXERR_ABORT;` |
|     187 |  7804 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7805 | `			goto Synchronize;` |
|       - |  7806 | `		}` |
|      91 |  7807 | `	}` |
|       - |  7808 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   76547 |  7809 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 |  7810 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7811 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 |  7812 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7813 | `			return SXERR_ABORT;` |
|       - |  7814 | `		}` |
|       3 |  7815 | `		goto Synchronize;` |
|       - |  7816 | `	}` |
|       - |  7817 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|       - |  7818 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|       - |  7819 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|       - |  7820 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|       - |  7821 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|       - |  7822 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|   76545 |  7823 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|       6 |  7824 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7825 | `			"New expressions are not supported in this context");` |
|       6 |  7826 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7827 | `			return SXERR_ABORT;` |
|       - |  7828 | `		}` |
|       6 |  7829 | `		goto Synchronize;` |
|       - |  7830 | `	}` |
|       - |  7831 | `	/* Allocate a new class attribute */` |
|   76541 |  7832 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   76541 |  7833 | `	if( pAttr == 0 ){` |
|     ! 0 |  7834 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  7835 | `		return SXERR_ABORT;` |
|       - |  7836 | `	}` |
|   76541 |  7837 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     185 |  7838 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      90 |  7839 | `	}` |
|   76541 |  7840 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - |  7841 | `		SySet *pInstrContainer;` |
|   22173 |  7842 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - |  7843 | `		/* Swap bytecode container */` |
|   22173 |  7844 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   22173 |  7845 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - |  7846 | `		/* Compile attribute value.` |
|       - |  7847 | `		 */` |
|   22173 |  7848 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   22173 |  7849 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  7850 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 |  7851 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7852 | `				return SXERR_ABORT;` |
|       - |  7853 | `			}` |
|     ! 0 |  7854 | `		}` |
|       - |  7855 | `		/* Emit the done instruction */` |
|   22173 |  7856 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   22173 |  7857 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   11084 |  7858 | `	}` |
|       - |  7859 | `	/* All done,install the attribute */` |
|   76541 |  7860 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   76541 |  7861 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7862 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7863 | `		return SXERR_ABORT;` |
|       - |  7864 | `	}` |
|   76541 |  7865 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7866 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 |  7867 | `		pGen->pIn++; /* Jump the comma */` |
|       5 |  7868 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  7869 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7870 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7871 | `				pTok--;` |
|     ! 0 |  7872 | `			}` |
|     ! 0 |  7873 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7874 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 |  7875 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7876 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7877 | `				return SXERR_ABORT;` |
|       - |  7878 | `			}` |
|     ! 0 |  7879 | `		}else{` |
|       5 |  7880 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 |  7881 | `				goto loop;` |
|       - |  7882 | `			}` |
|       - |  7883 | `		}` |
|     ! 0 |  7884 | `	}` |
|   76537 |  7885 | `	SySetRelease(&aUnionAlts);` |
|   76537 |  7886 | `	return SXRET_OK;` |
|       9 |  7887 | `Synchronize:` |
|       - |  7888 | `	/* Synchronize with the first semi-colon */` |
|      56 |  7889 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      37 |  7890 | `		pGen->pIn++;` |
|       3 |  7891 | `	}` |
|      22 |  7892 | `	SySetRelease(&aUnionAlts);` |
|      22 |  7893 | `	return SXERR_CORRUPT;` |
|   38280 |  7894 | `}` |
|       - |  7895 | `/*` |
|       - |  7896 | ` * Compile a class method.` |
|       - |  7897 | ` *` |
|       - |  7898 | ` * Refer to the official documentation for more information` |
|       - |  7899 | ` * on the powerful extension introduced by the PH7 engine` |
|       - |  7900 | ` * to the OO subsystem such as full type hinting,method` |
|       - |  7901 | ` * overloading and many more.` |
|       - |  7902 | ` */` |
|  272042 |  7903 | `static sxi32 GenStateCompileClassMethod(` |
|       - |  7904 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  7905 | `	sxi32 iProtection,   /* Visibility level */` |
|       - |  7906 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - |  7907 | `	int doBody,          /* TRUE to process method body */` |
|       - |  7908 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - |  7909 | `	)` |
|       5 |  7910 | `{` |
|  272047 |  7911 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7912 | `	ph7_class_method *pMeth;` |
|       - |  7913 | `	sxi32 iFuncFlags;` |
|       - |  7914 | `	SyString *pName;` |
|       - |  7915 | `	SyToken *pEnd;` |
|       - |  7916 | `	sxi32 rc;` |
|       - |  7917 | `	/* Extract visibility level */` |
|  272047 |  7918 | `	iProtection = GetProtectionLevel(iProtection);` |
|  272047 |  7919 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  272047 |  7920 | `	iFuncFlags = 0;` |
|  272047 |  7921 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7922 | `		/* Invalid method name */` |
|     ! 0 |  7923 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7924 | `		if( rc == SXERR_ABORT ){` |
|       - |  7925 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7926 | `			return SXERR_ABORT;` |
|       - |  7927 | `		}` |
|     ! 0 |  7928 | `		goto Synchronize;` |
|       - |  7929 | `	}` |
|  272047 |  7930 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  7931 | `		/* Return by reference,remember that */` |
|     ! 0 |  7932 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  7933 | `		/* Jump the '&' token */` |
|     ! 0 |  7934 | `		pGen->pIn++;` |
|     ! 0 |  7935 | `	}` |
|  272047 |  7936 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  7937 | `		/* Invalid method name */` |
|     ! 0 |  7938 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7939 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7940 | `			return SXERR_ABORT;` |
|       - |  7941 | `		}` |
|     ! 0 |  7942 | `		goto Synchronize;` |
|       - |  7943 | `	}` |
|       - |  7944 | `	/* Peek method name */` |
|  272047 |  7945 | `	pName = &pGen->pIn->sData;` |
|  272047 |  7946 | `	nLine = pGen->pIn->nLine;` |
|       - |  7947 | `	/* Jump the method name */` |
|  272047 |  7948 | `	pGen->pIn++;` |
|  272047 |  7949 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - |  7950 | `		/* Abstract method */` |
|   93975 |  7951 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 |  7952 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7953 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 |  7954 | `				&pClass->sName,pName);` |
|     ! 0 |  7955 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7956 | `				return SXERR_ABORT;` |
|       - |  7957 | `			}` |
|     ! 0 |  7958 | `		}` |
|       - |  7959 | `		/* Assemble method signature only */` |
|   93975 |  7960 | `		doBody = FALSE;` |
|   46985 |  7961 | `	}` |
|  272047 |  7962 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  7963 | `		/* Syntax error */` |
|     ! 0 |  7964 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 |  7965 | `		if( rc == SXERR_ABORT ){` |
|       - |  7966 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7967 | `			return SXERR_ABORT;` |
|       - |  7968 | `		}` |
|     ! 0 |  7969 | `		goto Synchronize;` |
|       - |  7970 | `	}` |
|       - |  7971 | `	/* Allocate a new class_method instance */` |
|  272047 |  7972 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  272047 |  7973 | `	if( pMeth == 0 ){` |
|     ! 0 |  7974 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7975 | `		return SXERR_ABORT;` |
|       - |  7976 | `	}` |
|       - |  7977 | `	/* Jump the left parenthesis '(' */` |
|  272047 |  7978 | `	pGen->pIn++;` |
|  272047 |  7979 | `	pEnd = 0; /* cc warning */` |
|       - |  7980 | `	/* Delimit the method signature */` |
|  272047 |  7981 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  272047 |  7982 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7983 | `		/* Syntax error */` |
|       3 |  7984 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 |  7985 | `		if( rc == SXERR_ABORT ){` |
|       - |  7986 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7987 | `			return SXERR_ABORT;` |
|       - |  7988 | `		}` |
|       3 |  7989 | `		goto Synchronize;` |
|       - |  7990 | `	}` |
|       - |  7991 | `	{` |
|  272045 |  7992 | `		int bIsCtor = 0;` |
|  272045 |  7993 | `		int bAbstractCtor = 0;` |
|  397142 |  7994 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  161430 |  7995 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  261127 |  7996 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   21841 |  7997 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       3 |  7998 | `				bAbstractCtor = 1;` |
|       2 |  7999 | `			}else{` |
|   21839 |  8000 | `				bIsCtor = 1;` |
|       - |  8001 | `			}` |
|   10918 |  8002 | `		}` |
|  272045 |  8003 | `		if( pGen->pIn < pEnd ){` |
|       - |  8004 | `			/* Collect method arguments */` |
|   72715 |  8005 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   72715 |  8006 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8007 | `				return SXERR_ABORT;` |
|       - |  8008 | `			}` |
|   36355 |  8009 | `		}` |
|       - |  8010 | `	}` |
|       - |  8011 | `	/* Point past ')' and parse optional return type ': type' */` |
|  272045 |  8012 | `	pGen->pIn = &pEnd[1];` |
|       - |  8013 | `	{` |
|  272045 |  8014 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  272045 |  8015 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  8016 | `			return SXERR_ABORT;` |
|  272045 |  8017 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 |  8018 | `			goto Synchronize;` |
|       - |  8019 | `		}` |
|       - |  8020 | `	}` |
|       - |  8021 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - |  8022 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - |  8023 | `	 * since we mint real ph7_class_attr entries. */` |
|       - |  8024 | `	{` |
|  272045 |  8025 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - |  8026 | `		sxu32 i;` |
|  395433 |  8027 | `		for( i = 0; i < nArg; i++ ){` |
|  123403 |  8028 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - |  8029 | `			ph7_class_attr *pAttr;` |
|  123403 |  8030 | `			sxi32 iAttrFlags = 0;` |
|       - |  8031 | `			int bArgTyped;` |
|  123403 |  8032 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  123339 |  8033 | `				continue;` |
|       - |  8034 | `			}` |
|       - |  8035 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|       - |  8036 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|       - |  8037 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|      49 |  8038 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|      70 |  8039 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|      69 |  8040 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 |  8041 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8042 | `					"Cannot declare variadic promoted property");` |
|       3 |  8043 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8044 | `					return SXERR_ABORT;` |
|       - |  8045 | `				}` |
|       3 |  8046 | `				goto Synchronize;` |
|       - |  8047 | `			}` |
|       - |  8048 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - |  8049 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - |  8050 | `			 * appear as an alternative of a union type. */` |
|      67 |  8051 | `			if( bArgTyped ){` |
|      92 |  8052 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|      58 |  8053 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      58 |  8054 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      29 |  8055 | `					"Property %z::$%z cannot have type %z",nLine);` |
|      63 |  8056 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8057 | `					return SXERR_ABORT;` |
|      63 |  8058 | `				}else if( rc != SXRET_OK ){` |
|       6 |  8059 | `					goto Synchronize;` |
|       - |  8060 | `				}` |
|      27 |  8061 | `			}` |
|       - |  8062 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|      63 |  8063 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 |  8064 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  8065 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 |  8066 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8067 | `					return SXERR_ABORT;` |
|       - |  8068 | `				}` |
|       3 |  8069 | `				goto Synchronize;` |
|       - |  8070 | `			}` |
|      61 |  8071 | `			if( bArgTyped ){` |
|      57 |  8072 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      26 |  8073 | `			}` |
|      61 |  8074 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 |  8075 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 |  8076 | `			}` |
|      61 |  8077 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       8 |  8078 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       3 |  8079 | `			}` |
|      61 |  8080 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|       - |  8081 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|       - |  8082 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|      24 |  8083 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       4 |  8084 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  8085 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|       3 |  8086 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8087 | `						return SXERR_ABORT;` |
|       - |  8088 | `					}` |
|       3 |  8089 | `					goto Synchronize;` |
|       - |  8090 | `				}` |
|      22 |  8091 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  8092 | `			}` |
|      59 |  8093 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      59 |  8094 | `			if( pAttr == 0 ){` |
|     ! 0 |  8095 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8096 | `				return SXERR_ABORT;` |
|       - |  8097 | `			}` |
|      59 |  8098 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      57 |  8099 | `				pAttr->nType = pArg->nType;` |
|      57 |  8100 | `				pAttr->sClass = pArg->sClass;` |
|      57 |  8101 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      57 |  8102 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  8103 | `					sxu32 k;` |
|      20 |  8104 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|      14 |  8105 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|      14 |  8106 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|       8 |  8107 | `					}` |
|       3 |  8108 | `				}` |
|      26 |  8109 | `			}` |
|      59 |  8110 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      59 |  8111 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8112 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8113 | `				return SXERR_ABORT;` |
|       - |  8114 | `			}` |
|      32 |  8115 | `		}` |
|       - |  8116 | `	}` |
|  272035 |  8117 | `	if( doBody ){` |
|       - |  8118 | `		/* Compile method body */` |
|  178065 |  8119 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  178065 |  8120 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8121 | `			return SXERR_ABORT;` |
|       - |  8122 | `		}` |
|   89035 |  8123 | `	}else{` |
|       - |  8124 | `		/* Only method signature is allowed */` |
|   93975 |  8125 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 |  8126 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8127 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 |  8128 | `				if( rc == SXERR_ABORT ){` |
|       - |  8129 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8130 | `					return SXERR_ABORT;` |
|       - |  8131 | `				}` |
|     ! 0 |  8132 | `				return SXERR_CORRUPT;` |
|       - |  8133 | `			}` |
|       - |  8134 | `	}` |
|       - |  8135 | `	/* All done,install the method */` |
|  272035 |  8136 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  272035 |  8137 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8138 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8139 | `		return SXERR_ABORT;` |
|       - |  8140 | `	}` |
|  272035 |  8141 | `	return SXRET_OK;` |
|       6 |  8142 | `Synchronize:` |
|       - |  8143 | `	/* Synchronize with the first semi-colon */` |
|      40 |  8144 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      28 |  8145 | `		pGen->pIn++;` |
|       4 |  8146 | `	}` |
|      16 |  8147 | `	return SXERR_CORRUPT;` |
|  136026 |  8148 | `}` |
|       - |  8149 | `/*` |
|       - |  8150 | ` * Compile an object interface.` |
|       - |  8151 | ` *  According to the PHP language reference manual` |
|       - |  8152 | ` *   Object Interfaces:` |
|       - |  8153 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - |  8154 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - |  8155 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  8156 | ` *   class, but without any of the methods having their contents defined.` |
|       - |  8157 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  8158 | ` */` |
|   39816 |  8159 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       5 |  8160 | `{` |
|   39821 |  8161 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8162 | `	ph7_class *pClass,*pBase;` |
|       - |  8163 | `	SyToken *pEnd,*pTmp;` |
|       - |  8164 | `	SyString *pName;` |
|       - |  8165 | `	sxi32 nKwrd;` |
|       - |  8166 | `	sxi32 rc;` |
|       - |  8167 | `	/* Jump the 'interface' keyword */` |
|   39821 |  8168 | `	pGen->pIn++;` |
|       - |  8169 | `	/* Extract interface name */` |
|   39821 |  8170 | `	pName = &pGen->pIn->sData;` |
|       - |  8171 | `	/* Advance the stream cursor */` |
|   39821 |  8172 | `	pGen->pIn++;` |
|       - |  8173 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  8174 | `		SyBlob sFQN;` |
|       - |  8175 | `		SyString sFQNStr;` |
|   39821 |  8176 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   39821 |  8177 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   39821 |  8178 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   39821 |  8179 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   39821 |  8180 | `		SyBlobRelease(&sFQN);` |
|       - |  8181 | `	}` |
|   39821 |  8182 | `	if( pClass == 0 ){` |
|     ! 0 |  8183 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8184 | `		return SXERR_ABORT;` |
|       - |  8185 | `	}` |
|       - |  8186 | `	/* Mark as an interface */` |
|   39821 |  8187 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - |  8188 | `	/* Assume no base class is given */` |
|   39821 |  8189 | `	pBase = 0;` |
|   39821 |  8190 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   10849 |  8191 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   10849 |  8192 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - |  8193 | `			SyBlob sResolved;` |
|       - |  8194 | `			SyString sBaseName;` |
|       - |  8195 | `			sxu32 nRefLine;` |
|       - |  8196 | `			/* Extract base interface */` |
|   10849 |  8197 | `			pGen->pIn++;` |
|   10849 |  8198 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   10849 |  8199 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   10849 |  8200 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  8201 | `				SyBlobRelease(&sResolved);` |
|     ! 0 |  8202 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8203 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 |  8204 | `					pName);` |
|     ! 0 |  8205 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8206 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8207 | `					return SXERR_ABORT;` |
|       - |  8208 | `				}` |
|     ! 0 |  8209 | `				return SXRET_OK;` |
|       - |  8210 | `			}` |
|   16271 |  8211 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   10844 |  8212 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   10849 |  8213 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8214 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8215 | `			/* Only interfaces is allowed */` |
|   10849 |  8216 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8217 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8218 | `			}` |
|   10849 |  8219 | `			if( pBase == 0 ){` |
|     ! 0 |  8220 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8221 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|     ! 0 |  8222 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8223 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8224 | `					return SXERR_ABORT;` |
|       - |  8225 | `				}` |
|     ! 0 |  8226 | `			}` |
|   10849 |  8227 | `			SyBlobRelease(&sResolved);` |
|    5422 |  8228 | `		}` |
|    5422 |  8229 | `	}` |
|   39821 |  8230 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8231 | `		/* Syntax error */` |
|     ! 0 |  8232 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 |  8233 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8234 | `		if( rc == SXERR_ABORT ){` |
|       - |  8235 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8236 | `			return SXERR_ABORT;` |
|       - |  8237 | `		}` |
|     ! 0 |  8238 | `		return SXRET_OK;` |
|       - |  8239 | `	}` |
|   39821 |  8240 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   39821 |  8241 | `	pEnd = 0; /* cc warning */` |
|       - |  8242 | `	/* Delimit the interface body */` |
|   39821 |  8243 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   39821 |  8244 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8245 | `		/* Syntax error */` |
|     ! 0 |  8246 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 |  8247 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8248 | `		if( rc == SXERR_ABORT ){` |
|       - |  8249 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8250 | `			return SXERR_ABORT;` |
|       - |  8251 | `		}` |
|     ! 0 |  8252 | `		return SXRET_OK;` |
|       - |  8253 | `	}` |
|       - |  8254 | `	/* Swap token stream */` |
|   39821 |  8255 | `	pTmp = pGen->pEnd;` |
|   39821 |  8256 | `	pGen->pEnd = pEnd;` |
|       - |  8257 | `	/* Start the parse process` |
|       - |  8258 | `	 * Note (According to the PHP reference manual):` |
|       - |  8259 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - |  8260 | `	 *  Only 'public' visibility is allowed.` |
|       - |  8261 | `	 */` |
|   66888 |  8262 | `	for(;;){` |
|       - |  8263 | `		/* Jump leading/trailing semi-colons */` |
|  227741 |  8264 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   93965 |  8265 | `			pGen->pIn++;` |
|       5 |  8266 | `		}` |
|  133781 |  8267 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8268 | `			/* End of interface body */` |
|   39817 |  8269 | `			break;` |
|       - |  8270 | `		}` |
|   93969 |  8271 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8272 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8273 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 |  8274 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8275 | `			if( rc == SXERR_ABORT ){` |
|       - |  8276 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8277 | `				return SXERR_ABORT;` |
|       - |  8278 | `			}` |
|     ! 0 |  8279 | `			goto done;` |
|       - |  8280 | `		}` |
|       - |  8281 | `		/* Extract the current keyword */` |
|   93969 |  8282 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   93969 |  8283 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - |  8284 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - |  8285 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 |  8286 | `			const char *zKind = "member";` |
|       3 |  8287 | `			SyString *pMemberName = 0;` |
|       3 |  8288 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 |  8289 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 |  8290 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 |  8291 | `					zKind = "constant";` |
|       3 |  8292 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 |  8293 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 |  8294 | `					}` |
|       1 |  8295 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8296 | `					zKind = "method";` |
|     ! 0 |  8297 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 |  8298 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 |  8299 | `					}` |
|     ! 0 |  8300 | `				}` |
|       1 |  8301 | `			}` |
|       3 |  8302 | `			if( pMemberName ){` |
|       4 |  8303 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 |  8304 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 |  8305 | `			}else{` |
|     ! 0 |  8306 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8307 | `					"Access type for interface %s must be public",zKind);` |
|       - |  8308 | `			}` |
|       3 |  8309 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8310 | `				return SXERR_ABORT;` |
|       - |  8311 | `			}` |
|       3 |  8312 | `			goto done;` |
|       - |  8313 | `		}` |
|   93967 |  8314 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  8315 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8316 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  8317 | `			if( rc == SXERR_ABORT ){` |
|       - |  8318 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8319 | `				return SXERR_ABORT;` |
|       - |  8320 | `			}` |
|     ! 0 |  8321 | `			goto done;` |
|       - |  8322 | `		}` |
|   93967 |  8323 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - |  8324 | `			/* Advance the stream cursor */` |
|   93955 |  8325 | `			pGen->pIn++;` |
|   93955 |  8326 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8327 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8328 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  8329 | `				if( rc == SXERR_ABORT ){` |
|       - |  8330 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8331 | `					return SXERR_ABORT;` |
|       - |  8332 | `				}` |
|     ! 0 |  8333 | `				goto done;` |
|       - |  8334 | `			}` |
|   93955 |  8335 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   93955 |  8336 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  8337 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8338 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  8339 | `				if( rc == SXERR_ABORT ){` |
|       - |  8340 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8341 | `					return SXERR_ABORT;` |
|       - |  8342 | `				}` |
|     ! 0 |  8343 | `				goto done;` |
|       - |  8344 | `			}` |
|   46975 |  8345 | `		}` |
|   93967 |  8346 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8347 | `			/* Parse constant */` |
|      10 |  8348 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|      10 |  8349 | `			if( rc != SXRET_OK ){` |
|       3 |  8350 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8351 | `					return SXERR_ABORT;` |
|       - |  8352 | `				}` |
|       3 |  8353 | `				goto done;` |
|       - |  8354 | `			}` |
|       4 |  8355 | `		}else{` |
|   93959 |  8356 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   93959 |  8357 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8358 | `				/* Static method,record that */` |
|   10841 |  8359 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - |  8360 | `				/* Advance the stream cursor */` |
|   10841 |  8361 | `				pGen->pIn++;` |
|   10836 |  8362 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|   10841 |  8363 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8364 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8365 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  8366 | `						if( rc == SXERR_ABORT ){` |
|       - |  8367 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8368 | `							return SXERR_ABORT;` |
|       - |  8369 | `						}` |
|     ! 0 |  8370 | `						goto done;` |
|       - |  8371 | `				}` |
|    5418 |  8372 | `			}` |
|       - |  8373 | `			/* Process method signature (no body for interface methods) */` |
|   93959 |  8374 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   93959 |  8375 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8376 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8377 | `					return SXERR_ABORT;` |
|       - |  8378 | `				}` |
|     ! 0 |  8379 | `				goto done;` |
|       - |  8380 | `			}` |
|       - |  8381 | `		}` |
|       5 |  8382 | `	}` |
|       - |  8383 | `	/* Install the interface */` |
|   39817 |  8384 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   39817 |  8385 | `	if( rc == SXRET_OK && pBase ){` |
|       - |  8386 | `		/* Inherit from the base interface */` |
|   10849 |  8387 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|    5422 |  8388 | `	}` |
|   39817 |  8389 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8390 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8391 | `		return SXERR_ABORT;` |
|       - |  8392 | `	}` |
|   19906 |  8393 | `done:` |
|       - |  8394 | `	/* Point beyond the interface body */` |
|   39821 |  8395 | `	pGen->pIn  = &pEnd[1];` |
|   39821 |  8396 | `	pGen->pEnd = pTmp;` |
|   39821 |  8397 | `	return PH7_OK;` |
|   19913 |  8398 | `}` |
|       - |  8399 | `/*` |
|       - |  8400 | ` * Compile a user-defined class.` |
|       - |  8401 | ` * According to the PHP language reference manual` |
|       - |  8402 | ` *  class` |
|       - |  8403 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - |  8404 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - |  8405 | ` *  of the properties and methods belonging to the class.` |
|       - |  8406 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - |  8407 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - |  8408 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - |  8409 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  8410 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - |  8411 | ` *  (called "methods").` |
|       - |  8412 | ` */` |
|       - |  8413 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - |  8414 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - |  8415 | `struct TraitUseEntry {` |
|       - |  8416 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - |  8417 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - |  8418 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - |  8419 | `};` |
|       - |  8420 | `/*` |
|       - |  8421 | ` * Validate that methods implementing interface contracts have compatible` |
|       - |  8422 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - |  8423 | ` */` |
|  102392 |  8424 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8425 | `{` |
|       - |  8426 | `	ph7_class **apIface;` |
|       - |  8427 | `	sxu32 nIface,i;` |
|       - |  8428 | `	sxi32 rc;` |
|  102397 |  8429 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 |  8430 | `		return SXRET_OK;` |
|       - |  8431 | `	}` |
|  102397 |  8432 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|  102397 |  8433 | `	nIface = SySetUsed(&pClass->aInterface);` |
|  196559 |  8434 | `	for(i = 0; i < nIface; i++){` |
|   94167 |  8435 | `		ph7_class *pIface = apIface[i];` |
|       - |  8436 | `		SyHashEntry *pEntry;` |
|   94167 |  8437 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|  253545 |  8438 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|  159383 |  8439 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - |  8440 | `			ph7_class_method *pImplMeth;` |
|  159383 |  8441 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - |  8442 | `			/* Find the implementing method in the class */` |
|  159383 |  8443 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|  159383 |  8444 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      18 |  8445 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - |  8446 | `			}` |
|       - |  8447 | `			/* Check visibility: interface methods must be implemented as public */` |
|  159369 |  8448 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 |  8449 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8450 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 |  8451 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 |  8452 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8453 | `					return SXERR_ABORT;` |
|       - |  8454 | `				}` |
|       1 |  8455 | `			}` |
|       - |  8456 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - |  8457 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - |  8458 | `			 */` |
|       - |  8459 | `			{` |
|  159369 |  8460 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|  159369 |  8461 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|  159369 |  8462 | `				int sigError = 0;` |
|  159369 |  8463 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 |  8464 | `					sigError = 1;` |
|  159368 |  8465 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - |  8466 | `					/* Extra parameters must all have default values */` |
|       6 |  8467 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - |  8468 | `					sxu32 k;` |
|       8 |  8469 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       6 |  8470 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 |  8471 | `							sigError = 1;` |
|       3 |  8472 | `							break;` |
|       - |  8473 | `						}` |
|       2 |  8474 | `					}` |
|       2 |  8475 | `				}` |
|  159369 |  8476 | `				if( sigError ){` |
|       - |  8477 | `					SyBlob sImplSig, sIfaceSig;` |
|       - |  8478 | `					ph7_vm_func_arg *aArgs;` |
|       - |  8479 | `					sxu32 j;` |
|       6 |  8480 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       6 |  8481 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - |  8482 | `					/* Build implementing method signature */` |
|       6 |  8483 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      12 |  8484 | `					for(j = 0; j < nImplArgs; j++){` |
|       8 |  8485 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       8 |  8486 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       8 |  8487 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8488 | `					}` |
|       - |  8489 | `					/* Build interface method signature */` |
|       6 |  8490 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      12 |  8491 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       8 |  8492 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       8 |  8493 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       8 |  8494 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8495 | `					}` |
|       8 |  8496 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8497 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 |  8498 | `						&pClass->sName,pMName,` |
|       4 |  8499 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 |  8500 | `						&pIface->sName,pMName,` |
|       4 |  8501 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       6 |  8502 | `					SyBlobRelease(&sImplSig);` |
|       6 |  8503 | `					SyBlobRelease(&sIfaceSig);` |
|       6 |  8504 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8505 | `						return SXERR_ABORT;` |
|       - |  8506 | `					}` |
|       2 |  8507 | `				}` |
|       - |  8508 | `			}` |
|       5 |  8509 | `		}` |
|   47086 |  8510 | `	}` |
|  102397 |  8511 | `	return SXRET_OK;` |
|   51201 |  8512 | `}` |
|       - |  8513 | `/*` |
|       - |  8514 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - |  8515 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - |  8516 | ` */` |
|  102392 |  8517 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8518 | `{` |
|       - |  8519 | `	ph7_class_method *pMeth;` |
|       - |  8520 | `	SyHashEntry *pEntry;` |
|       - |  8521 | `	sxu32 nAbstract;` |
|       - |  8522 | `	SyBlob sMsg;` |
|       - |  8523 | `	sxi32 rc;` |
|       - |  8524 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|  102397 |  8525 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      37 |  8526 | `		return SXRET_OK;` |
|       - |  8527 | `	}` |
|       - |  8528 | `	/* Count abstract methods */` |
|  102365 |  8529 | `	nAbstract = 0;` |
|  102365 |  8530 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  960105 |  8531 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  857745 |  8532 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  857745 |  8533 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      20 |  8534 | `			nAbstract++;` |
|       8 |  8535 | `		}` |
|       5 |  8536 | `	}` |
|  102365 |  8537 | `	if( nAbstract == 0 ){` |
|  102351 |  8538 | `		return SXRET_OK;` |
|       - |  8539 | `	}` |
|       - |  8540 | `	/* Build the error message listing all abstract methods with origins */` |
|      18 |  8541 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      18 |  8542 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - |  8543 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 |  8544 | `		&pClass->sName,nAbstract,` |
|       7 |  8545 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 |  8546 | `		(nAbstract > 1 ? "s" : ""));` |
|       - |  8547 | `	/* Second pass: list methods with origins */` |
|       - |  8548 | `	{` |
|      18 |  8549 | `		sxu32 nListed = 0;` |
|      18 |  8550 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      36 |  8551 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      22 |  8552 | `			ph7_class *pOrigin = 0;` |
|       - |  8553 | `			SyString *pMName;` |
|      22 |  8554 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      22 |  8555 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 |  8556 | `				continue;` |
|       - |  8557 | `			}` |
|      20 |  8558 | `			pMName = &pMeth->sFunc.sName;` |
|      20 |  8559 | `			if( nListed > 0 ){` |
|       3 |  8560 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 |  8561 | `			}` |
|       - |  8562 | `			/* Find the origin of this abstract method.` |
|       - |  8563 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - |  8564 | `			 * inheritance chains) take precedence for interface-declared` |
|       - |  8565 | `			 * methods. Abstract class methods only win when the class` |
|       - |  8566 | `			 * itself declared the abstract method (not inherited from` |
|       - |  8567 | `			 * an interface). Trait methods are adopted into the using` |
|       - |  8568 | `			 * class's namespace.` |
|       - |  8569 | `			 */` |
|       - |  8570 | `			{` |
|       - |  8571 | `				ph7_class **apIface;` |
|       - |  8572 | `				ph7_class **apTrait;` |
|       - |  8573 | `				ph7_class *pWalk;` |
|       - |  8574 | `				sxu32 i;` |
|       - |  8575 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - |  8576 | `				 * (one that was written in the class body, not inherited from an` |
|       - |  8577 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - |  8578 | `				 */` |
|      20 |  8579 | `				if( pClass->pBase ){` |
|      11 |  8580 | `					pWalk = pClass->pBase;` |
|      19 |  8581 | `					while( pWalk ){` |
|       - |  8582 | `						ph7_class_method *pParentMeth;` |
|      13 |  8583 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      13 |  8584 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - |  8585 | `							/* Exclude methods that came from an interface anywhere` |
|       - |  8586 | `							 * in this class's ancestor chain.` |
|       - |  8587 | `							 */` |
|      13 |  8588 | `							int fromIface = 0;` |
|      13 |  8589 | `							ph7_class *pAnc = pWalk;` |
|      17 |  8590 | `							while( pAnc ){` |
|       - |  8591 | `								ph7_class **apPI;` |
|       - |  8592 | `								sxu32 j;` |
|      15 |  8593 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      15 |  8594 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|      10 |  8595 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|      10 |  8596 | `										fromIface = 1;` |
|      10 |  8597 | `										break;` |
|       - |  8598 | `									}` |
|     ! 0 |  8599 | `								}` |
|      15 |  8600 | `								if( fromIface ) break;` |
|       6 |  8601 | `								pAnc = pAnc->pBase;` |
|       2 |  8602 | `							}` |
|      13 |  8603 | `							if( !fromIface ){` |
|       3 |  8604 | `								pOrigin = pWalk;` |
|       3 |  8605 | `								break;` |
|       - |  8606 | `							}` |
|       4 |  8607 | `						}` |
|      10 |  8608 | `						pWalk = pWalk->pBase;` |
|       2 |  8609 | `					}` |
|       4 |  8610 | `				}` |
|       - |  8611 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - |  8612 | `				 * each interface's own parent chain for the deepest origin.` |
|       - |  8613 | `				 */` |
|      20 |  8614 | `				if( !pOrigin ){` |
|      18 |  8615 | `					pWalk = pClass;` |
|      40 |  8616 | `					while( pWalk && !pOrigin ){` |
|      26 |  8617 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      26 |  8618 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      16 |  8619 | `							ph7_class *pIface = apIface[i];` |
|      16 |  8620 | `							ph7_class *pDeepest = 0;` |
|      28 |  8621 | `							while( pIface ){` |
|      16 |  8622 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      16 |  8623 | `									pDeepest = pIface;` |
|       6 |  8624 | `								}` |
|      16 |  8625 | `								pIface = pIface->pBase;` |
|       4 |  8626 | `							}` |
|      16 |  8627 | `							if( pDeepest ){` |
|      16 |  8628 | `								pOrigin = pDeepest;` |
|      16 |  8629 | `								break;` |
|       - |  8630 | `							}` |
|     ! 0 |  8631 | `						}` |
|      26 |  8632 | `						pWalk = pWalk->pBase;` |
|       4 |  8633 | `					}` |
|       7 |  8634 | `				}` |
|       - |  8635 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      20 |  8636 | `				if( !pOrigin ){` |
|       3 |  8637 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 |  8638 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 |  8639 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 |  8640 | `							pOrigin = pClass;` |
|       3 |  8641 | `							break;` |
|       - |  8642 | `						}` |
|     ! 0 |  8643 | `					}` |
|       1 |  8644 | `				}` |
|       - |  8645 | `			}` |
|      20 |  8646 | `			if( pOrigin ){` |
|      20 |  8647 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|      12 |  8648 | `			}else{` |
|       - |  8649 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 |  8650 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - |  8651 | `			}` |
|      20 |  8652 | `			nListed++;` |
|       4 |  8653 | `		}` |
|       - |  8654 | `	}` |
|      18 |  8655 | `	SyBlobAppend(&sMsg,")",1);` |
|      25 |  8656 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 |  8657 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      18 |  8658 | `	SyBlobRelease(&sMsg);` |
|      18 |  8659 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  8660 | `		return SXERR_ABORT;` |
|       - |  8661 | `	}` |
|      18 |  8662 | `	return SXRET_OK;` |
|   51201 |  8663 | `}` |
|       - |  8664 | `/*` |
|       - |  8665 | ` * Parse a class/interface name reference from the current token stream.` |
|       - |  8666 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|       - |  8667 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|       - |  8668 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|       - |  8669 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|       - |  8670 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|       - |  8671 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|       - |  8672 | ` */` |
|   98516 |  8673 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|       5 |  8674 | `{` |
|   98521 |  8675 | `	int isAbsolute = 0;` |
|   98521 |  8676 | `	SyToken *pStart = pGen->pIn;` |
|       - |  8677 | `	SyBlob sName;` |
|   98521 |  8678 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|     101 |  8679 | `		isAbsolute = 1;` |
|     101 |  8680 | `		pGen->pIn++;` |
|      48 |  8681 | `	}` |
|   98521 |  8682 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       9 |  8683 | `		pGen->pIn = pStart;` |
|       9 |  8684 | `		return SXERR_INVALID;` |
|       - |  8685 | `	}` |
|   98515 |  8686 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   98515 |  8687 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   98515 |  8688 | `	pGen->pIn++;` |
|  147783 |  8689 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   49278 |  8690 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      13 |  8691 | `		SyBlobAppend(&sName,"\\",1);` |
|      13 |  8692 | `		pGen->pIn++;` |
|      13 |  8693 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      13 |  8694 | `		pGen->pIn++;` |
|       1 |  8695 | `	}` |
|   98515 |  8696 | `	if( isAbsolute ){` |
|      99 |  8697 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      52 |  8698 | `	}else{` |
|       - |  8699 | `		SyString sRaw;` |
|   98421 |  8700 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   98421 |  8701 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|       - |  8702 | `	}` |
|   98515 |  8703 | `	SyBlobRelease(&sName);` |
|   98515 |  8704 | `	return SXRET_OK;` |
|   49263 |  8705 | `}` |
|       - |  8706 | `/*` |
|       - |  8707 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|       - |  8708 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|       - |  8709 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|       - |  8710 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|       - |  8711 | ` * either direction cannot run unbounded.` |
|       - |  8712 | ` */` |
|       - |  8713 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|   11008 |  8714 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|       5 |  8715 | `{` |
|       - |  8716 | `	ph7_class **apParent;` |
|       - |  8717 | `	sxu32 n;` |
|   18441 |  8718 | `	while( pInterface ){` |
|   14667 |  8719 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|     ! 0 |  8720 | `			return FALSE;` |
|       - |  8721 | `		}` |
|   18293 |  8722 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    7252 |  8723 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|    7239 |  8724 | `			return TRUE;` |
|       - |  8725 | `		}` |
|    7433 |  8726 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    7433 |  8727 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|     ! 0 |  8728 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|     ! 0 |  8729 | `				return TRUE;` |
|       - |  8730 | `			}` |
|     ! 0 |  8731 | `		}` |
|    7433 |  8732 | `		pInterface = pInterface->pBase;` |
|    7433 |  8733 | `		iDepth++;` |
|       5 |  8734 | `	}` |
|    3779 |  8735 | `	return FALSE;` |
|    5509 |  8736 | `}` |
|   11008 |  8737 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|       5 |  8738 | `{` |
|   11013 |  8739 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|       5 |  8740 | `}` |
|       - |  8741 | `/*` |
|       - |  8742 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|       - |  8743 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|       - |  8744 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|       - |  8745 | ` */` |
|    7234 |  8746 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|       5 |  8747 | `{` |
|    7243 |  8748 | `	while( pBase ){` |
|      10 |  8749 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|       2 |  8750 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|       3 |  8751 | `			return TRUE;` |
|       - |  8752 | `		}` |
|      10 |  8753 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|       6 |  8754 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|       3 |  8755 | `			return TRUE;` |
|       - |  8756 | `		}` |
|       5 |  8757 | `		pBase = pBase->pBase;` |
|       1 |  8758 | `	}` |
|    7235 |  8759 | `	return FALSE;` |
|    3622 |  8760 | `}` |
|       - |  8761 | `/*` |
|       - |  8762 | ` * Compile a class declaration, named or anonymous.` |
|       - |  8763 | ` *` |
|       - |  8764 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|       - |  8765 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|       - |  8766 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|       - |  8767 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|       - |  8768 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|       - |  8769 | ` * implements, body, install) is shared by both paths.` |
|       - |  8770 | ` */` |
|  102432 |  8771 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|       - |  8772 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|       5 |  8773 | `{` |
|  102437 |  8774 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8775 | `	ph7_class *pClass,*pBase;` |
|       - |  8776 | `	SyToken *pEnd,*pTmp;` |
|       - |  8777 | `	sxi32 iProtection;` |
|       - |  8778 | `	SySet aInterfaces;` |
|       - |  8779 | `	SySet aUseEntries;` |
|       - |  8780 | `	sxi32 iAttrflags;` |
|       - |  8781 | `	SyString *pName;` |
|       - |  8782 | `	sxi32 nKwrd;` |
|       - |  8783 | `	sxi32 rc;` |
|       - |  8784 | `	/* Jump the 'class' keyword */` |
|  102437 |  8785 | `	pGen->pIn++;` |
|  102437 |  8786 | `	if( pAnonName ){` |
|       - |  8787 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|       - |  8788 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|       - |  8789 | `		 * then use the synthesized name. */` |
|      30 |  8790 | `		*ppArgStart = *ppArgEnd = 0;` |
|      30 |  8791 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       7 |  8792 | `			pGen->pIn++; /* Jump '(' */` |
|       7 |  8793 | `			*ppArgStart = pGen->pIn;` |
|      10 |  8794 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|       3 |  8795 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|       7 |  8796 | `			pGen->pIn = *ppArgEnd;` |
|       7 |  8797 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|       3 |  8798 | `		}` |
|      30 |  8799 | `		pName = pAnonName;` |
|      30 |  8800 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|      17 |  8801 | `	}else{` |
|  102411 |  8802 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  8803 | `			/* Syntax error */` |
|     ! 0 |  8804 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 |  8805 | `			if( rc == SXERR_ABORT ){` |
|       - |  8806 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8807 | `				return SXERR_ABORT;` |
|       - |  8808 | `			}` |
|       - |  8809 | `			/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 |  8810 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 |  8811 | `				pGen->pIn++;` |
|     ! 0 |  8812 | `			}` |
|     ! 0 |  8813 | `			return SXRET_OK;` |
|       - |  8814 | `		}` |
|       - |  8815 | `		/* Extract class name */` |
|  102411 |  8816 | `		pName = &pGen->pIn->sData;` |
|       - |  8817 | `		/* Advance the stream cursor */` |
|  102411 |  8818 | `		pGen->pIn++;` |
|       - |  8819 | `		/* Build FQN and obtain a raw class */ {` |
|       - |  8820 | `			SyBlob sFQN;` |
|       - |  8821 | `			SyString sFQNStr;` |
|  102411 |  8822 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|  102411 |  8823 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|  102411 |  8824 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|  102411 |  8825 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|  102411 |  8826 | `			SyBlobRelease(&sFQN);` |
|       - |  8827 | `		}` |
|       - |  8828 | `	}` |
|  102437 |  8829 | `	if( pClass == 0 ){` |
|     ! 0 |  8830 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8831 | `		return SXERR_ABORT;` |
|       - |  8832 | `	}` |
|       - |  8833 | `	/* implemented interfaces and per-use-statement trait containers */` |
|  102437 |  8834 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|  102437 |  8835 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - |  8836 | `	/* Assume a standalone class */` |
|  102437 |  8837 | `	pBase = 0;` |
|  102437 |  8838 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   87033 |  8839 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   87033 |  8840 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|       - |  8841 | `			SyBlob sResolved;` |
|       - |  8842 | `			SyString sBaseName;` |
|       - |  8843 | `			sxu32 nRefLine;` |
|   76043 |  8844 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   76043 |  8845 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   76043 |  8846 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   76043 |  8847 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       3 |  8848 | `				SyBlobRelease(&sResolved);` |
|       4 |  8849 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8850 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|       1 |  8851 | `					pName);` |
|       3 |  8852 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       3 |  8853 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8854 | `					return SXERR_ABORT;` |
|       - |  8855 | `				}` |
|       3 |  8856 | `				return SXRET_OK;` |
|       - |  8857 | `			}` |
|  114059 |  8858 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   76036 |  8859 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   76041 |  8860 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8861 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8862 | `			/* Interfaces are not allowed */` |
|   76041 |  8863 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 |  8864 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8865 | `			}` |
|   76041 |  8866 | `			if( pBase == 0 ){` |
|     ! 0 |  8867 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8868 | `					"Nonexistent base class '%z'",&sBaseName);` |
|     ! 0 |  8869 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8870 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8871 | `					return SXERR_ABORT;` |
|       - |  8872 | `				}` |
|     ! 0 |  8873 | `			}else{` |
|   76041 |  8874 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 |  8875 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  8876 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 |  8877 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8878 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8879 | `						return SXERR_ABORT;` |
|       - |  8880 | `					}` |
|     ! 0 |  8881 | `				}` |
|       - |  8882 | `			}` |
|   76041 |  8883 | `			SyBlobRelease(&sResolved);` |
|   38018 |  8884 | `		}` |
|   87031 |  8885 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - |  8886 | `			ph7_class *pInterface;` |
|       - |  8887 | `			/* Interface implementation */` |
|   11003 |  8888 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    5509 |  8889 | `			for(;;){` |
|       - |  8890 | `				SyBlob sResolved;` |
|       - |  8891 | `				SyString sIntName;` |
|       - |  8892 | `				sxu32 nRefLine;` |
|   11013 |  8893 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   11013 |  8894 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   11013 |  8895 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  8896 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8897 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8898 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 |  8899 | `						pName);` |
|     ! 0 |  8900 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8901 | `						return SXERR_ABORT;` |
|       - |  8902 | `					}` |
|     ! 0 |  8903 | `					break;` |
|       - |  8904 | `				}` |
|   22021 |  8905 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|   11008 |  8906 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   11013 |  8907 | `				SyStringInitFromBuf(&sIntName,` |
|       - |  8908 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8909 | `				/* Only interfaces are allowed */` |
|   11013 |  8910 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8911 | `					pInterface = pInterface->pNextName;` |
|     ! 0 |  8912 | `				}` |
|   11013 |  8913 | `				if( pInterface == 0 ){` |
|     ! 0 |  8914 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8915 | `						"Nonexistent base interface '%z'",&sIntName);` |
|     ! 0 |  8916 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8917 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8918 | `						return SXERR_ABORT;` |
|       - |  8919 | `					}` |
|     ! 0 |  8920 | `				}else{` |
|       - |  8921 | `					/* Reject user classes that try to implement Throwable` |
|       - |  8922 | `					 * directly (or via an interface that extends Throwable)` |
|       - |  8923 | `					 * unless they already extend Exception or Error.` |
|       - |  8924 | `					 * Exception and Error themselves are compiled from the` |
|       - |  8925 | `					 * built-in library and are exempt by FQN — a namespaced` |
|       - |  8926 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|   11013 |  8927 | `					SyString *pFqn = &pClass->sName;` |
|   11013 |  8928 | `					int bIsExceptionOrError =` |
|    9120 |  8929 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   18322 |  8930 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    9209 |  8931 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|    3626 |  8932 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   18240 |  8933 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|   10854 |  8934 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|    3615 |  8935 | `						!bIsExceptionOrError ){` |
|      12 |  8936 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8937 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|       3 |  8938 | `							&pClass->sName);` |
|       9 |  8939 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8940 | `							SyBlobRelease(&sResolved);` |
|     ! 0 |  8941 | `							return SXERR_ABORT;` |
|       - |  8942 | `						}` |
|       - |  8943 | `						/* Skip registration so the follow-up abstract-method` |
|       - |  8944 | `						 * check does not produce a duplicate fatal. */` |
|       6 |  8945 | `					}else{` |
|   11007 |  8946 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - |  8947 | `					}` |
|       - |  8948 | `				}` |
|   11013 |  8949 | `				SyBlobRelease(&sResolved);` |
|   11013 |  8950 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    5504 |  8951 | `					break;` |
|       - |  8952 | `				}` |
|      14 |  8953 | `				pGen->pIn++;/* Jump the comma */` |
|       4 |  8954 | `			}` |
|    5499 |  8955 | `		}` |
|   43513 |  8956 | `	}` |
|  102435 |  8957 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8958 | `		/* Syntax error */` |
|     ! 0 |  8959 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 |  8960 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8961 | `		if( rc == SXERR_ABORT ){` |
|       - |  8962 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8963 | `			return SXERR_ABORT;` |
|       - |  8964 | `		}` |
|     ! 0 |  8965 | `		return SXRET_OK;` |
|       - |  8966 | `	}` |
|  102435 |  8967 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|  102435 |  8968 | `	pEnd = 0; /* cc warning */` |
|       - |  8969 | `	/* Delimit the class body */` |
|  102435 |  8970 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|  102435 |  8971 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8972 | `		/* Syntax error */` |
|     ! 0 |  8973 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 |  8974 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8975 | `		if( rc == SXERR_ABORT ){` |
|       - |  8976 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8977 | `			return SXERR_ABORT;` |
|       - |  8978 | `		}` |
|     ! 0 |  8979 | `		return SXRET_OK;` |
|       - |  8980 | `	}` |
|       - |  8981 | `	/* Swap token stream */` |
|  102435 |  8982 | `	pTmp = pGen->pEnd;` |
|  102435 |  8983 | `	pGen->pEnd = pEnd;` |
|       - |  8984 | `	/* Set the inherited flags */` |
|  102435 |  8985 | `	pClass->iFlags = iFlags;` |
|       - |  8986 | `	/* Start the parse process */` |
|  140257 |  8987 | `	for(;;){` |
|       - |  8988 | `		/* Jump leading/trailing semi-colons */` |
|  433737 |  8989 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   76651 |  8990 | `			pGen->pIn++;` |
|       5 |  8991 | `		}` |
|  357091 |  8992 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8993 | `			/* End of class body */` |
|  102397 |  8994 | `			break;` |
|       - |  8995 | `		}` |
|  254694 |  8996 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|  127352 |  8997 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|     ! 0 |  8998 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8999 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  9000 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  9001 | `			if( rc == SXERR_ABORT ){` |
|       - |  9002 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  9003 | `				return SXERR_ABORT;` |
|       - |  9004 | `			}` |
|     ! 0 |  9005 | `			goto done;` |
|       - |  9006 | `		}` |
|       - |  9007 | `		/* Assume public visibility */` |
|  254699 |  9008 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  254699 |  9009 | `		iAttrflags = 0;` |
|       - |  9010 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|       - |  9011 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|       - |  9012 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|       - |  9013 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  254699 |  9014 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  9015 | `			int bMod = 0;` |
|     ! 0 |  9016 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  9017 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       - |  9018 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|       - |  9019 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|       - |  9020 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|       - |  9021 | `			 * that the generic keyword dispatch would misread as a method. */` |
|     ! 0 |  9022 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     ! 0 |  9023 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  9024 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|     ! 0 |  9025 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|     ! 0 |  9026 | `			}` |
|     ! 0 |  9027 | `			if( !bMod ){` |
|     ! 0 |  9028 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9029 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9030 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9031 | `						return SXERR_ABORT;` |
|       - |  9032 | `					}` |
|     ! 0 |  9033 | `					goto done;` |
|       - |  9034 | `				}` |
|     ! 0 |  9035 | `				continue;` |
|       - |  9036 | `			}` |
|     ! 0 |  9037 | `		}` |
|  254699 |  9038 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9039 | `			/* Extract the current keyword */` |
|  254699 |  9040 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  254699 |  9041 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  9042 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - |  9043 | `				TraitUseEntry sUse;` |
|      57 |  9044 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      57 |  9045 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      57 |  9046 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      34 |  9047 | `				for(;;){` |
|       - |  9048 | `					ph7_class *pTrait;` |
|       - |  9049 | `					SyString *pTraitName;` |
|      65 |  9050 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9051 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9052 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 |  9053 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9054 | `							return SXERR_ABORT;` |
|       - |  9055 | `						}` |
|     ! 0 |  9056 | `						break;` |
|       - |  9057 | `					}` |
|      65 |  9058 | `					pTraitName = &pGen->pIn->sData;` |
|       - |  9059 | `					/* Resolve trait name through namespace/imports */ {` |
|       - |  9060 | `						SyBlob sResolved;` |
|      65 |  9061 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      65 |  9062 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     125 |  9063 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      60 |  9064 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      65 |  9065 | `						SyBlobRelease(&sResolved);` |
|       - |  9066 | `					}` |
|       - |  9067 | `					/* Only traits are allowed */` |
|      65 |  9068 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  9069 | `						pTrait = pTrait->pNextName;` |
|     ! 0 |  9070 | `					}` |
|      65 |  9071 | `					if( pTrait == 0 ){` |
|     ! 0 |  9072 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9073 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 |  9074 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9075 | `							return SXERR_ABORT;` |
|       - |  9076 | `						}` |
|     ! 0 |  9077 | `					}else{` |
|      65 |  9078 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - |  9079 | `					}` |
|      65 |  9080 | `					pGen->pIn++; /* Advance past trait name */` |
|      65 |  9081 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      31 |  9082 | `						break;` |
|       - |  9083 | `					}` |
|      10 |  9084 | `					pGen->pIn++; /* Jump the comma */` |
|       2 |  9085 | `				}` |
|       - |  9086 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      57 |  9087 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - |  9088 | `					SyToken *pBlock;` |
|      13 |  9089 | `					pGen->pIn++; /* Jump '{' */` |
|      13 |  9090 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|      13 |  9091 | `					sUse.pResolvStart = pGen->pIn;` |
|      13 |  9092 | `					sUse.pResolvEnd = pBlock;` |
|      13 |  9093 | `					if( pBlock < pGen->pEnd ){` |
|      13 |  9094 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       8 |  9095 | `					}else{` |
|     ! 0 |  9096 | `						pGen->pIn = pGen->pEnd;` |
|       - |  9097 | `					}` |
|       5 |  9098 | `				}` |
|      57 |  9099 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - |  9100 | `				/* The semicolon will be consumed by the outer loop */` |
|      57 |  9101 | `				continue;` |
|       - |  9102 | `			}` |
|  254647 |  9103 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  254341 |  9104 | `				iProtection = nKwrd;` |
|  254341 |  9105 | `				pGen->pIn++; /* Jump the visibility token */` |
|       - |  9106 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`. */`` |
|  254341 |  9107 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      20 |  9108 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      20 |  9109 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       8 |  9110 | `				}` |
|  254336 |  9111 | `				if( pGen->pIn >= pGen->pEnd` |
|  254341 |  9112 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9113 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9114 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  9115 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9116 | `					if( rc == SXERR_ABORT ){` |
|       - |  9117 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  9118 | `						return SXERR_ABORT;` |
|       - |  9119 | `					}` |
|     ! 0 |  9120 | `					goto done;` |
|       - |  9121 | `				}` |
|  254341 |  9122 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  9123 | `					/* Attribute declaration (untyped) */` |
|   76345 |  9124 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   76345 |  9125 | `					if( rc != SXRET_OK ){` |
|      11 |  9126 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9127 | `							return SXERR_ABORT;` |
|       - |  9128 | `						}` |
|      11 |  9129 | `						goto done;` |
|       - |  9130 | `					}` |
|   76337 |  9131 | `					continue;` |
|       - |  9132 | `				}` |
|  178001 |  9133 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  9134 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     173 |  9135 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     173 |  9136 | `					if( rc != SXRET_OK ){` |
|       8 |  9137 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9138 | `							return SXERR_ABORT;` |
|       - |  9139 | `						}` |
|       8 |  9140 | `						goto done;` |
|       - |  9141 | `					}` |
|     167 |  9142 | `					continue;` |
|       - |  9143 | `				}` |
|       - |  9144 | `				/* Extract the keyword */` |
|  177833 |  9145 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   88914 |  9146 | `			}` |
|  178139 |  9147 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  9148 | `				/* Process constant declaration */` |
|      79 |  9149 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      79 |  9150 | `				if( rc != SXRET_OK ){` |
|      11 |  9151 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9152 | `						return SXERR_ABORT;` |
|       - |  9153 | `					}` |
|      11 |  9154 | `					goto done;` |
|       - |  9155 | `				}` |
|      38 |  9156 | `			}else{` |
|  178065 |  9157 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  9158 | `					/* Static method or attribute,record that */` |
|   10903 |  9159 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|   10903 |  9160 | `					pGen->pIn++; /* Jump the static keyword */` |
|   10903 |  9161 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9162 | `						/* Extract the keyword */` |
|   10893 |  9163 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   10893 |  9164 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  9165 | `							iProtection = nKwrd;` |
|     ! 0 |  9166 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 |  9167 | `						}` |
|    5444 |  9168 | `					}` |
|       - |  9169 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|       - |  9170 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|       - |  9171 | `					 * than a generic "expecting method" parse error. */` |
|   10903 |  9172 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  9173 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  9174 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|     ! 0 |  9175 | `					}` |
|   10898 |  9176 | `					if( pGen->pIn >= pGen->pEnd` |
|   10903 |  9177 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9178 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9179 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 |  9180 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9181 | `						if( rc == SXERR_ABORT ){` |
|       - |  9182 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9183 | `							return SXERR_ABORT;` |
|       - |  9184 | `						}` |
|     ! 0 |  9185 | `						goto done;` |
|       - |  9186 | `					}` |
|   10903 |  9187 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  9188 | `						/* Attribute declaration */` |
|      11 |  9189 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      11 |  9190 | `						if( rc != SXRET_OK ){` |
|       3 |  9191 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9192 | `								return SXERR_ABORT;` |
|       - |  9193 | `							}` |
|       3 |  9194 | `							goto done;` |
|       - |  9195 | `						}` |
|       8 |  9196 | `						continue;` |
|       - |  9197 | `					}` |
|   10895 |  9198 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  9199 | `						/* Typed static attribute declaration */` |
|      15 |  9200 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      15 |  9201 | `						if( rc != SXRET_OK ){` |
|       3 |  9202 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9203 | `								return SXERR_ABORT;` |
|       - |  9204 | `							}` |
|       3 |  9205 | `							goto done;` |
|       - |  9206 | `						}` |
|      13 |  9207 | `						continue;` |
|       - |  9208 | `					}` |
|       - |  9209 | `					/* Extract the keyword */` |
|   10883 |  9210 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  172606 |  9211 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - |  9212 | `					/* Abstract method,record that */` |
|      15 |  9213 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - |  9214 | `					/* Mark the whole class as abstract */` |
|      15 |  9215 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - |  9216 | `					/* Advance the stream cursor */` |
|      15 |  9217 | `					pGen->pIn++;` |
|      15 |  9218 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      15 |  9219 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      15 |  9220 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      13 |  9221 | `							iProtection = nKwrd;` |
|      13 |  9222 | `							pGen->pIn++; /* Jump the visibility token */` |
|       5 |  9223 | `						}` |
|       6 |  9224 | `					}` |
|      15 |  9225 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      12 |  9226 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  9227 | `							/* Static method */` |
|     ! 0 |  9228 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  9229 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  9230 | `					}` |
|      15 |  9231 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      12 |  9232 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9233 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9234 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 |  9235 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  9236 | `							if( rc == SXERR_ABORT ){` |
|       - |  9237 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  9238 | `								return SXERR_ABORT;` |
|       - |  9239 | `							}` |
|     ! 0 |  9240 | `							goto done;` |
|       - |  9241 | `					}` |
|      15 |  9242 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  167161 |  9243 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - |  9244 | `					/* final method ,record that */` |
|      16 |  9245 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|      16 |  9246 | `					pGen->pIn++; /* Jump the final keyword */` |
|      16 |  9247 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9248 | `						/* Extract the keyword */` |
|      16 |  9249 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      16 |  9250 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       8 |  9251 | `							iProtection = nKwrd;` |
|       8 |  9252 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 |  9253 | `						}` |
|       7 |  9254 | `					}` |
|      16 |  9255 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      14 |  9256 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|       - |  9257 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|       - |  9258 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|       - |  9259 | `							 * child class is compiled (PH7_ClassInherit). */` |
|      12 |  9260 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  9261 | `							if( rc != SXRET_OK ){` |
|     ! 0 |  9262 | `								if( rc == SXERR_ABORT ){` |
|     ! 0 |  9263 | `									return SXERR_ABORT;` |
|       - |  9264 | `								}` |
|     ! 0 |  9265 | `								goto done;` |
|       - |  9266 | `							}` |
|      12 |  9267 | `							continue;` |
|       - |  9268 | `					}` |
|       5 |  9269 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 |  9270 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  9271 | `							/* Static method */` |
|     ! 0 |  9272 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  9273 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  9274 | `					}` |
|       5 |  9275 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  9276 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9277 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9278 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 |  9279 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  9280 | `							if( rc == SXERR_ABORT ){` |
|       - |  9281 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  9282 | `								return SXERR_ABORT;` |
|       - |  9283 | `							}` |
|     ! 0 |  9284 | `							goto done;` |
|       - |  9285 | `					}` |
|       5 |  9286 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  9287 | `				}` |
|  178035 |  9288 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  9289 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9290 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 |  9291 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9292 | `						if( rc == SXERR_ABORT ){` |
|       - |  9293 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9294 | `							return SXERR_ABORT;` |
|       - |  9295 | `						}` |
|     ! 0 |  9296 | `						goto done;` |
|       - |  9297 | `				}` |
|  178035 |  9298 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 |  9299 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 |  9300 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 |  9301 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9302 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  9303 | `						if( rc == SXERR_ABORT ){` |
|       - |  9304 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9305 | `							return SXERR_ABORT;` |
|       - |  9306 | `						}` |
|     ! 0 |  9307 | `						goto done;` |
|       - |  9308 | `					}` |
|       - |  9309 | `					/* Attribute declaration */` |
|       7 |  9310 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 |  9311 | `				}else{` |
|       - |  9312 | `					/* Process method declaration */` |
|  178029 |  9313 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9314 | `				}` |
|  178035 |  9315 | `				if( rc != SXRET_OK ){` |
|      16 |  9316 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9317 | `						return SXERR_ABORT;` |
|       - |  9318 | `					}` |
|      16 |  9319 | `					goto done;` |
|       - |  9320 | `				}` |
|       - |  9321 | `			}` |
|   89047 |  9322 | `		}else{` |
|       - |  9323 | `			/* Attribute declaration */` |
|     ! 0 |  9324 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9325 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9326 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9327 | `					return SXERR_ABORT;` |
|       - |  9328 | `				}` |
|     ! 0 |  9329 | `				goto done;` |
|       - |  9330 | `			}` |
|       - |  9331 | `		}` |
|       5 |  9332 | `	}` |
|       - |  9333 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - |  9334 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - |  9335 | `	 */` |
|       - |  9336 | `	{` |
|       - |  9337 | `		TraitUseEntry *apUse;` |
|       - |  9338 | `		sxu32 nU;` |
|  102397 |  9339 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|  102449 |  9340 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      57 |  9341 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      57 |  9342 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      57 |  9343 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      57 |  9344 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - |  9345 | `			sxu32 nT;` |
|      57 |  9346 | `			if( !hasResolution ){` |
|       - |  9347 | `				/* No conflict resolution block: use standard trait application */` |
|      95 |  9348 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      53 |  9349 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      53 |  9350 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9351 | `						break;` |
|       - |  9352 | `					}` |
|      29 |  9353 | `				}` |
|      26 |  9354 | `			}else{` |
|       - |  9355 | `				/* With resolution block: copy attributes, record traits,` |
|       - |  9356 | `				 * then use the block to resolve method conflicts.` |
|       - |  9357 | `				 */` |
|       - |  9358 | `				SyToken *pR;` |
|      25 |  9359 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      15 |  9360 | `					ph7_class *pTR = apTrait[nT];` |
|       - |  9361 | `					ph7_class_attr *pAR;` |
|       - |  9362 | `					SyHashEntry *pER;` |
|       - |  9363 | `					SyString *pNR;` |
|      15 |  9364 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      21 |  9365 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 |  9366 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 |  9367 | `						pNR = &pAR->sName;` |
|     ! 0 |  9368 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 |  9369 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 |  9370 | `						}` |
|     ! 0 |  9371 | `					}` |
|      15 |  9372 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       9 |  9373 | `				}` |
|       - |  9374 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|      13 |  9375 | `				pR = pUse->pResolvStart;` |
|      27 |  9376 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  9377 | `					SyString sTrait,sMethod;` |
|       - |  9378 | `					ph7_class *pSrcTrait;` |
|       - |  9379 | `					ph7_class_method *pMeth;` |
|       - |  9380 | `					sxi32 nRKwrd;` |
|      41 |  9381 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      27 |  9382 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      17 |  9383 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      17 |  9384 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      17 |  9385 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      17 |  9386 | `					sMethod = pR->sData;` |
|      17 |  9387 | `					pR++;` |
|      17 |  9388 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9389 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9390 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9391 | `							sTrait = sMethod;` |
|       7 |  9392 | `							pR++;` |
|       7 |  9393 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9394 | `							sMethod = pR->sData;` |
|       7 |  9395 | `							pR++;` |
|       3 |  9396 | `						}` |
|       3 |  9397 | `					}` |
|      17 |  9398 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9399 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  9400 | `						continue;` |
|       - |  9401 | `					}` |
|      17 |  9402 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      17 |  9403 | `					pR++;` |
|      17 |  9404 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 |  9405 | `						pSrcTrait = 0;` |
|       7 |  9406 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 |  9407 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 |  9408 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 |  9409 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 |  9410 | `								pSrcTrait = apTrait[nT];` |
|       5 |  9411 | `								break;` |
|       - |  9412 | `							}` |
|       2 |  9413 | `						}` |
|       5 |  9414 | `						if( pSrcTrait ){` |
|       5 |  9415 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 |  9416 | `							if( pMeth ){` |
|       5 |  9417 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 |  9418 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 |  9419 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 |  9420 | `								}` |
|       2 |  9421 | `							}` |
|       2 |  9422 | `						}` |
|       2 |  9423 | `					}` |
|      35 |  9424 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       3 |  9425 | `				}` |
|       - |  9426 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      25 |  9427 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - |  9428 | `					ph7_class_method *pMR;` |
|       - |  9429 | `					SyHashEntry *pER;` |
|       - |  9430 | `					SyString *pNR;` |
|      15 |  9431 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      41 |  9432 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      23 |  9433 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      23 |  9434 | `						pNR = &pMR->sFunc.sName;` |
|      23 |  9435 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      14 |  9436 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       6 |  9437 | `						}` |
|       3 |  9438 | `					}` |
|       9 |  9439 | `				}` |
|       - |  9440 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|      13 |  9441 | `				pR = pUse->pResolvStart;` |
|      27 |  9442 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  9443 | `					SyString sTrait,sMethod,sAlias;` |
|       - |  9444 | `					ph7_class *pSrcTrait;` |
|       - |  9445 | `					ph7_class_method *pMeth;` |
|      27 |  9446 | `					int hasQual = 0;` |
|       - |  9447 | `					sxi32 nRKwrd;` |
|      41 |  9448 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      27 |  9449 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      17 |  9450 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      17 |  9451 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      17 |  9452 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      17 |  9453 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      17 |  9454 | `					sMethod = pR->sData;` |
|      17 |  9455 | `					pR++;` |
|      17 |  9456 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9457 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9458 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9459 | `							sTrait = sMethod;` |
|       7 |  9460 | `							hasQual = 1;` |
|       7 |  9461 | `							pR++;` |
|       7 |  9462 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9463 | `							sMethod = pR->sData;` |
|       7 |  9464 | `							pR++;` |
|       3 |  9465 | `						}` |
|       3 |  9466 | `					}` |
|      17 |  9467 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9468 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  9469 | `						continue;` |
|       - |  9470 | `					}` |
|      17 |  9471 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      17 |  9472 | `					pR++;` |
|      17 |  9473 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|      13 |  9474 | `						sxi32 iNewVis = -1;` |
|      13 |  9475 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 |  9476 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 |  9477 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 |  9478 | `								iNewVis = nAK;` |
|       7 |  9479 | `								pR++;` |
|       3 |  9480 | `							}` |
|       3 |  9481 | `						}` |
|      13 |  9482 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|      11 |  9483 | `							sAlias = pR->sData;` |
|      11 |  9484 | `							pR++;` |
|       4 |  9485 | `						}` |
|      13 |  9486 | `						pMeth = 0;` |
|      13 |  9487 | `						if( hasQual ){` |
|       3 |  9488 | `							pSrcTrait = 0;` |
|       5 |  9489 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 |  9490 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 |  9491 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 |  9492 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 |  9493 | `									pSrcTrait = apTrait[nT];` |
|       3 |  9494 | `									break;` |
|       - |  9495 | `								}` |
|       2 |  9496 | `							}` |
|       3 |  9497 | `							if( pSrcTrait ){` |
|       3 |  9498 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 |  9499 | `							}` |
|       2 |  9500 | `						}else{` |
|      10 |  9501 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - |  9502 | `						}` |
|      13 |  9503 | `						if( pMeth ){` |
|      13 |  9504 | `							if( sAlias.nByte > 0 ){` |
|       - |  9505 | `								/* Create a shallow copy of the method struct for the alias` |
|       - |  9506 | `								 * so it can carry its own visibility without affecting the original.` |
|       - |  9507 | `								 */` |
|       - |  9508 | `								ph7_class_method *pAlias;` |
|       - |  9509 | `								char *zAliasDup;` |
|      11 |  9510 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|      11 |  9511 | `								if( pAlias ){` |
|      11 |  9512 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|      11 |  9513 | `									if( iNewVis >= 0 ){` |
|       5 |  9514 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9515 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9516 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 |  9517 | `									}` |
|      11 |  9518 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      11 |  9519 | `									if( zAliasDup ){` |
|      11 |  9520 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       4 |  9521 | `									}` |
|       7 |  9522 | `								}` |
|       7 |  9523 | `							}else if( iNewVis >= 0 ){` |
|       - |  9524 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - |  9525 | `								ph7_class_method *pCopy;` |
|       3 |  9526 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 |  9527 | `								if( pCopy ){` |
|       3 |  9528 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 |  9529 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 |  9530 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9531 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9532 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - |  9533 | `									/* Replace the method in the class hash */` |
|       3 |  9534 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 |  9535 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 |  9536 | `								}` |
|       1 |  9537 | `							}` |
|       5 |  9538 | `						}` |
|       5 |  9539 | `						SXUNUSED(hasQual);` |
|       5 |  9540 | `					}` |
|      21 |  9541 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       3 |  9542 | `				}` |
|       - |  9543 | `			}` |
|      57 |  9544 | `			SySetRelease(&pUse->aTraits);` |
|      31 |  9545 | `		}` |
|       - |  9546 | `	}` |
|       - |  9547 | `	/* Install the class */` |
|  102397 |  9548 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|  102397 |  9549 | `	if( rc == SXRET_OK ){` |
|       - |  9550 | `		ph7_class **apInterface;` |
|       - |  9551 | `		sxu32 n;` |
|  102397 |  9552 | `		if( pBase ){` |
|       - |  9553 | `			/* Inherit from base class and mark as a subclass */` |
|   76041 |  9554 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   38018 |  9555 | `		}` |
|  102397 |  9556 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|  113399 |  9557 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - |  9558 | `			/* Implements one or more interface */` |
|   11007 |  9559 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|   11007 |  9560 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9561 | `				break;` |
|       - |  9562 | `			}` |
|    5506 |  9563 | `		}` |
|       - |  9564 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|       - |  9565 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|  153588 |  9566 | `		if( rc == SXRET_OK` |
|  102392 |  9567 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|  102397 |  9568 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   83167 |  9569 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|       - |  9570 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   83167 |  9571 | `			if( pStringable ){` |
|   83167 |  9572 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   83167 |  9573 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|       - |  9574 | `				sxu32 i;` |
|   83167 |  9575 | `				int bAlready = 0;` |
|   90395 |  9576 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    7235 |  9577 | `					if( apImpl[i] == pStringable ){` |
|       3 |  9578 | `						bAlready = 1;` |
|       3 |  9579 | `						break;` |
|       - |  9580 | `					}` |
|    3619 |  9581 | `				}` |
|   83167 |  9582 | `				if( !bAlready ){` |
|   83165 |  9583 | `					PH7_ClassImplement(pClass,pStringable);` |
|   41580 |  9584 | `				}` |
|   41581 |  9585 | `			}` |
|   41581 |  9586 | `		}` |
|       - |  9587 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|  102397 |  9588 | `		if( rc == SXRET_OK ){` |
|  102397 |  9589 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|  102397 |  9590 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  9591 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  9592 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  9593 | `				return SXERR_ABORT;` |
|       - |  9594 | `			}` |
|   51196 |  9595 | `		}` |
|       - |  9596 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|  102397 |  9597 | `		if( rc == SXRET_OK ){` |
|  102397 |  9598 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|  102397 |  9599 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  9600 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  9601 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  9602 | `				return SXERR_ABORT;` |
|       - |  9603 | `			}` |
|   51196 |  9604 | `		}` |
|   51196 |  9605 | `	}` |
|  102397 |  9606 | `	SySetRelease(&aUseEntries);` |
|  102397 |  9607 | `	SySetRelease(&aInterfaces);` |
|  102397 |  9608 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9609 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9610 | `		return SXERR_ABORT;` |
|       - |  9611 | `	}` |
|   51196 |  9612 | `done:` |
|       - |  9613 | `	/* Point beyond the class body */` |
|  102435 |  9614 | `	pGen->pIn = &pEnd[1];` |
|  102435 |  9615 | `	pGen->pEnd = pTmp;` |
|  102435 |  9616 | `	return PH7_OK;` |
|   51221 |  9617 | `}` |
|       - |  9618 | `/* Compile a named class declaration (the common case). */` |
|  102406 |  9619 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       5 |  9620 | `{` |
|  102411 |  9621 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|       5 |  9622 | `}` |
|       - |  9623 | `/*` |
|       - |  9624 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|       - |  9625 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|       - |  9626 | ` * compile + install the class body once (at compile time, like every other` |
|       - |  9627 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|       - |  9628 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|       - |  9629 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|       - |  9630 | ` */` |
|      26 |  9631 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  9632 | `{` |
|       - |  9633 | `	char zName[128];         /* Synthesized class name */` |
|       - |  9634 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|       - |  9635 | `	SyString sName;` |
|       - |  9636 | `	SyToken *pArgStart,*pArgEnd;` |
|       - |  9637 | `	ph7_value *pObj;` |
|      30 |  9638 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9639 | `	sxu32 nIdx,nLen;` |
|       - |  9640 | `	sxi32 nArg,rc;` |
|      13 |  9641 | `	SXUNUSED(iCompileFlag);` |
|       - |  9642 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|      30 |  9643 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      30 |  9644 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  9645 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|     ! 0 |  9646 | `	}` |
|      30 |  9647 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - |  9648 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|       - |  9649 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|       - |  9650 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|      30 |  9651 | `	pArgStart = pArgEnd = 0;` |
|      30 |  9652 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|      30 |  9653 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9654 | `		return rc;` |
|       - |  9655 | `	}` |
|       - |  9656 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|       - |  9657 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|      30 |  9658 | `	nArg = 0;` |
|      30 |  9659 | `	if( pArgStart < pArgEnd ){` |
|       7 |  9660 | `		SyToken *pSavedIn = pGen->pIn;` |
|       7 |  9661 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  9662 | `		SyToken *pArgNext;` |
|       7 |  9663 | `		pGen->pIn = pArgStart;` |
|       7 |  9664 | `		pGen->pEnd = pArgEnd;` |
|      13 |  9665 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|       7 |  9666 | `			if( pGen->pIn < pArgNext ){` |
|       7 |  9667 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|       7 |  9668 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9669 | `					pGen->pIn = pSavedIn;` |
|     ! 0 |  9670 | `					pGen->pEnd = pSavedEnd;` |
|     ! 0 |  9671 | `					return SXERR_ABORT;` |
|       - |  9672 | `				}` |
|       7 |  9673 | `				nArg++;` |
|       3 |  9674 | `			}` |
|       7 |  9675 | `			pGen->pIn = &pArgNext[1];` |
|       1 |  9676 | `		}` |
|       7 |  9677 | `		pGen->pIn = pSavedIn;` |
|       7 |  9678 | `		pGen->pEnd = pSavedEnd;` |
|       3 |  9679 | `	}` |
|       - |  9680 | `	/* Load the synthesized class name */` |
|      30 |  9681 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      30 |  9682 | `	if( pObj == 0 ){` |
|     ! 0 |  9683 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9684 | `		return SXERR_ABORT;` |
|       - |  9685 | `	}` |
|      30 |  9686 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      30 |  9687 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  9688 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|      30 |  9689 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|      30 |  9690 | `	return SXRET_OK;` |
|      17 |  9691 | `}` |
|       - |  9692 | `/*` |
|       - |  9693 | ` * Compile a user-defined abstract class.` |
|       - |  9694 | ` *  According to the PHP language reference manual` |
|       - |  9695 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - |  9696 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - |  9697 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - |  9698 | ` *   the method's signature - they cannot define the implementation.` |
|       - |  9699 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - |  9700 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - |  9701 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - |  9702 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - |  9703 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - |  9704 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - |  9705 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - |  9706 | ` *   could differ.` |
|       - |  9707 | ` */` |
|       - |  9708 | `/*` |
|       - |  9709 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|       - |  9710 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|       - |  9711 | ` * receives the corresponding PH7_CLASS_* bit.` |
|       - |  9712 | ` */` |
|  991558 |  9713 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|       5 |  9714 | `{` |
|  991563 |  9715 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  663467 |  9716 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  663467 |  9717 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  656225 |  9718 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  328079 |  9719 | `	}` |
|  984259 |  9720 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  984199 |  9721 | `	return FALSE;` |
|  495784 |  9722 | `}` |
|       - |  9723 | `/*` |
|       - |  9724 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|       - |  9725 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|       - |  9726 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|       - |  9727 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|       - |  9728 | ` */` |
|  984194 |  9729 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|       5 |  9730 | `{` |
|  984199 |  9731 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  984199 |  9732 | `	sxi32 iFlags = 0,iFlag;` |
|  991563 |  9733 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|    7369 |  9734 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|       5 |  9735 | `			pDup = pIn;` |
|       2 |  9736 | `		}` |
|    7369 |  9737 | `		iFlags \|= iFlag;` |
|    7369 |  9738 | `		pIn++;` |
|       5 |  9739 | `	}` |
|  984199 |  9740 | `	*ppIn = pIn;` |
|  984199 |  9741 | `	if( ppDup ){ *ppDup = pDup; }` |
|  984199 |  9742 | `	return iFlags;` |
|       5 |  9743 | `}` |
|       - |  9744 | `/*` |
|       - |  9745 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|       - |  9746 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|       - |  9747 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|       - |  9748 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|       - |  9749 | `` * `readonly`) to their existing handlers.`` |
|       - |  9750 | ` */` |
|  980522 |  9751 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|       5 |  9752 | `{` |
|  980527 |  9753 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  493940 |  9754 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  982360 |  9755 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|       5 |  9756 | `}` |
|       - |  9757 | `/*` |
|       - |  9758 | ` * Compile a class declaration carrying one or more leading modifiers` |
|       - |  9759 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|       - |  9760 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|       - |  9761 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|       - |  9762 | `` * `abstract`+`final` pair, like PHP.`` |
|       - |  9763 | ` */` |
|    3672 |  9764 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|       5 |  9765 | `{` |
|       - |  9766 | `	SyToken *pDup;` |
|    3677 |  9767 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|       - |  9768 | `	sxi32 rc;` |
|    3677 |  9769 | `	if( pDup ){` |
|       4 |  9770 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|       2 |  9771 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|       3 |  9772 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9773 | `			return SXERR_ABORT;` |
|       - |  9774 | `		}` |
|       1 |  9775 | `	}` |
|    5508 |  9776 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|    1841 |  9777 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|       3 |  9778 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9779 | `			"Cannot use the final modifier on an abstract class");` |
|       3 |  9780 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9781 | `			return SXERR_ABORT;` |
|       - |  9782 | `		}` |
|       1 |  9783 | `	}` |
|    3677 |  9784 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|    1841 |  9785 | `}` |
|       - |  9786 | `/*` |
|       - |  9787 | ` * Compile a user-defined trait.` |
|       - |  9788 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - |  9789 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - |  9790 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - |  9791 | ` */` |
|      64 |  9792 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       5 |  9793 | `{` |
|      69 |  9794 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9795 | `	ph7_class *pClass;` |
|       - |  9796 | `	SyToken *pEnd,*pTmp;` |
|       - |  9797 | `	sxi32 iProtection;` |
|       - |  9798 | `	sxi32 iAttrflags;` |
|       - |  9799 | `	SyString *pName;` |
|       - |  9800 | `	sxi32 nKwrd;` |
|       - |  9801 | `	sxi32 rc;` |
|       - |  9802 | `	/* Jump the 'trait' keyword */` |
|      69 |  9803 | `	pGen->pIn++;` |
|      69 |  9804 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9805 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 |  9806 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9807 | `			return SXERR_ABORT;` |
|       - |  9808 | `		}` |
|     ! 0 |  9809 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 |  9810 | `			pGen->pIn++;` |
|     ! 0 |  9811 | `		}` |
|     ! 0 |  9812 | `		return SXRET_OK;` |
|       - |  9813 | `	}` |
|       - |  9814 | `	/* Extract trait name */` |
|      69 |  9815 | `	pName = &pGen->pIn->sData;` |
|      69 |  9816 | `	pGen->pIn++;` |
|       - |  9817 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  9818 | `		SyBlob sFQN;` |
|       - |  9819 | `		SyString sFQNStr;` |
|      69 |  9820 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      69 |  9821 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      69 |  9822 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      69 |  9823 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      69 |  9824 | `		SyBlobRelease(&sFQN);` |
|       - |  9825 | `	}` |
|      69 |  9826 | `	if( pClass == 0 ){` |
|     ! 0 |  9827 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9828 | `		return SXERR_ABORT;` |
|       - |  9829 | `	}` |
|       - |  9830 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      69 |  9831 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  9832 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 |  9833 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9834 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9835 | `			return SXERR_ABORT;` |
|       - |  9836 | `		}` |
|     ! 0 |  9837 | `		return SXRET_OK;` |
|       - |  9838 | `	}` |
|      69 |  9839 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      69 |  9840 | `	pEnd = 0;` |
|      69 |  9841 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      69 |  9842 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 |  9843 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 |  9844 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9845 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9846 | `			return SXERR_ABORT;` |
|       - |  9847 | `		}` |
|     ! 0 |  9848 | `		return SXRET_OK;` |
|       - |  9849 | `	}` |
|       - |  9850 | `	/* Swap token stream */` |
|      69 |  9851 | `	pTmp = pGen->pEnd;` |
|      69 |  9852 | `	pGen->pEnd = pEnd;` |
|       - |  9853 | `	/* Mark as trait */` |
|      69 |  9854 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - |  9855 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      64 |  9856 | `	for(;;){` |
|     177 |  9857 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      28 |  9858 | `			pGen->pIn++;` |
|       4 |  9859 | `		}` |
|     153 |  9860 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      69 |  9861 | `			break;` |
|       - |  9862 | `		}` |
|      89 |  9863 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  9864 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9865 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  9866 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  9867 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9868 | `				return SXERR_ABORT;` |
|       - |  9869 | `			}` |
|     ! 0 |  9870 | `			goto done;` |
|       - |  9871 | `		}` |
|      89 |  9872 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      89 |  9873 | `		iAttrflags = 0;` |
|      89 |  9874 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      89 |  9875 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      89 |  9876 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  9877 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 |  9878 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 |  9879 | `				for(;;){` |
|       - |  9880 | `					ph7_class *pUsedTrait;` |
|       - |  9881 | `					SyString *pUsedName;` |
|       5 |  9882 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9883 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9884 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 |  9885 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9886 | `							return SXERR_ABORT;` |
|       - |  9887 | `						}` |
|     ! 0 |  9888 | `						break;` |
|       - |  9889 | `					}` |
|       5 |  9890 | `					pUsedName = &pGen->pIn->sData;` |
|       - |  9891 | `					{` |
|       - |  9892 | `						SyBlob sResolved;` |
|       5 |  9893 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 |  9894 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 |  9895 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 |  9896 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 |  9897 | `						SyBlobRelease(&sResolved);` |
|       - |  9898 | `					}` |
|       5 |  9899 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  9900 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 |  9901 | `					}` |
|       5 |  9902 | `					if( pUsedTrait == 0 ){` |
|       4 |  9903 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 |  9904 | `							"'%z' is not a trait",pUsedName);` |
|       3 |  9905 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9906 | `							return SXERR_ABORT;` |
|       - |  9907 | `						}` |
|       2 |  9908 | `					}else{` |
|       3 |  9909 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - |  9910 | `					}` |
|       5 |  9911 | `					pGen->pIn++;` |
|       5 |  9912 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 |  9913 | `						break;` |
|       - |  9914 | `					}` |
|     ! 0 |  9915 | `					pGen->pIn++;` |
|     ! 0 |  9916 | `				}` |
|       5 |  9917 | `				continue;` |
|       - |  9918 | `			}` |
|      85 |  9919 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      73 |  9920 | `				iProtection = nKwrd;` |
|      73 |  9921 | `				pGen->pIn++;` |
|      68 |  9922 | `				if( pGen->pIn >= pGen->pEnd` |
|      73 |  9923 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9924 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9925 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  9926 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9927 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9928 | `						return SXERR_ABORT;` |
|       - |  9929 | `					}` |
|     ! 0 |  9930 | `					goto done;` |
|       - |  9931 | `				}` |
|      73 |  9932 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      12 |  9933 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  9934 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9935 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9936 | `							return SXERR_ABORT;` |
|       - |  9937 | `						}` |
|     ! 0 |  9938 | `						goto done;` |
|       - |  9939 | `					}` |
|      12 |  9940 | `					continue;` |
|       - |  9941 | `				}` |
|      63 |  9942 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 |  9943 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  9944 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9945 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9946 | `							return SXERR_ABORT;` |
|       - |  9947 | `						}` |
|     ! 0 |  9948 | `						goto done;` |
|       - |  9949 | `					}` |
|       5 |  9950 | `					continue;` |
|       - |  9951 | `				}` |
|      58 |  9952 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      27 |  9953 | `			}` |
|      71 |  9954 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 |  9955 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9956 | `					"Traits cannot have constants");` |
|     ! 0 |  9957 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9958 | `					return SXERR_ABORT;` |
|       - |  9959 | `				}` |
|     ! 0 |  9960 | `				goto done;` |
|     ! 0 |  9961 | `			}else{` |
|      71 |  9962 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 |  9963 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 |  9964 | `					pGen->pIn++;` |
|       5 |  9965 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 |  9966 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 |  9967 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  9968 | `							iProtection = nKwrd;` |
|     ! 0 |  9969 | `							pGen->pIn++;` |
|     ! 0 |  9970 | `						}` |
|       1 |  9971 | `					}` |
|       4 |  9972 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 |  9973 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9974 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9975 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 |  9976 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9977 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9978 | `							return SXERR_ABORT;` |
|       - |  9979 | `						}` |
|     ! 0 |  9980 | `						goto done;` |
|       - |  9981 | `					}` |
|       5 |  9982 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 |  9983 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 |  9984 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9985 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9986 | `								return SXERR_ABORT;` |
|       - |  9987 | `							}` |
|     ! 0 |  9988 | `							goto done;` |
|       - |  9989 | `						}` |
|       3 |  9990 | `						continue;` |
|       - |  9991 | `					}` |
|       3 |  9992 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 |  9993 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9994 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9995 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9996 | `								return SXERR_ABORT;` |
|       - |  9997 | `							}` |
|     ! 0 |  9998 | `							goto done;` |
|       - |  9999 | `						}` |
|     ! 0 | 10000 | `						continue;` |
|       - | 10001 | `					}` |
|       3 | 10002 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      68 | 10003 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       6 | 10004 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       6 | 10005 | `					pGen->pIn++;` |
|       6 | 10006 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       6 | 10007 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       6 | 10008 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 | 10009 | `							iProtection = nKwrd;` |
|       6 | 10010 | `							pGen->pIn++;` |
|       2 | 10011 | `						}` |
|       2 | 10012 | `					}` |
|       6 | 10013 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 10014 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 10015 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10016 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 | 10017 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 10018 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10019 | `							return SXERR_ABORT;` |
|       - | 10020 | `						}` |
|     ! 0 | 10021 | `						goto done;` |
|       - | 10022 | `					}` |
|       6 | 10023 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 10024 | `				}` |
|      69 | 10025 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 10026 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10027 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 | 10028 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 10029 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10030 | `						return SXERR_ABORT;` |
|       - | 10031 | `					}` |
|     ! 0 | 10032 | `					goto done;` |
|       - | 10033 | `				}` |
|      69 | 10034 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 | 10035 | `					pGen->pIn++;` |
|     ! 0 | 10036 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 10037 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10038 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 10039 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10040 | `							return SXERR_ABORT;` |
|       - | 10041 | `						}` |
|     ! 0 | 10042 | `						goto done;` |
|       - | 10043 | `					}` |
|     ! 0 | 10044 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 10045 | `				}else{` |
|      69 | 10046 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 10047 | `				}` |
|      69 | 10048 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10049 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10050 | `						return SXERR_ABORT;` |
|       - | 10051 | `					}` |
|     ! 0 | 10052 | `					goto done;` |
|       - | 10053 | `				}` |
|       - | 10054 | `			}` |
|      37 | 10055 | `		}else{` |
|     ! 0 | 10056 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 10057 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10058 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10059 | `					return SXERR_ABORT;` |
|       - | 10060 | `				}` |
|     ! 0 | 10061 | `				goto done;` |
|       - | 10062 | `			}` |
|       - | 10063 | `		}` |
|       5 | 10064 | `	}` |
|       - | 10065 | `	/* Install the trait */` |
|      69 | 10066 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      69 | 10067 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10068 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 10069 | `		return SXERR_ABORT;` |
|       - | 10070 | `	}` |
|      32 | 10071 | `done:` |
|       - | 10072 | `	/* Point beyond the trait body */` |
|      69 | 10073 | `	pGen->pIn = &pEnd[1];` |
|      69 | 10074 | `	pGen->pEnd = pTmp;` |
|      69 | 10075 | `	return PH7_OK;` |
|      37 | 10076 | `}` |
|       - | 10077 | `/*` |
|       - | 10078 | ` * Compile a user-defined class.` |
|       - | 10079 | ` *  According to the PHP language reference manual` |
|       - | 10080 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 10081 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 10082 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 10083 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 10084 | ` *   and functions (called "methods").` |
|       - | 10085 | ` */` |
|   98734 | 10086 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       5 | 10087 | `{` |
|       - | 10088 | `	sxi32 rc;` |
|   98739 | 10089 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   98739 | 10090 | `	return rc;` |
|       5 | 10091 | `}` |
|       - | 10092 | `/*` |
|       - | 10093 | ` * Exception handling.` |
|       - | 10094 | ` *  According to the PHP language reference manual` |
|       - | 10095 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 10096 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 10097 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 10098 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 10099 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 10100 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 10101 | ` *    (or re-thrown) within a catch block.` |
|       - | 10102 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 10103 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 10104 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 10105 | ` *    been defined with set_exception_handler().` |
|       - | 10106 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 10107 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 10108 | ` */` |
|       - | 10109 | `/*` |
|       - | 10110 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 10111 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 10112 | ` * indicates failure.` |
|       - | 10113 | ` */` |
|   14780 | 10114 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 | 10115 | `{` |
|   14785 | 10116 | `	sxi32 rc = SXRET_OK;` |
|   14785 | 10117 | `	if( pRoot->pOp ){` |
|   14775 | 10118 | `		switch( pRoot->pOp->iOp ){` |
|    7385 | 10119 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|       - | 10120 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|       - | 10121 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|       - | 10122 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|       - | 10123 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|       - | 10124 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|   14775 | 10125 | `			break;` |
|     ! 0 | 10126 | `		default:` |
|       - | 10127 | `			/* Runtime will still reject non-Throwable values; the set above` |
|       - | 10128 | `			 * covers the common shapes and gives a friendlier compile error` |
|       - | 10129 | ``			 * for obvious mistakes like `throw 5`. */`` |
|     ! 0 | 10130 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 10131 | `				"throw: Expecting an exception class instance");` |
|     ! 0 | 10132 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 | 10133 | `				rc = SXERR_INVALID;` |
|     ! 0 | 10134 | `			}` |
|     ! 0 | 10135 | `			break;` |
|       - | 10136 | `		}` |
|    7400 | 10137 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 10138 | `		/* Unexpected expression */` |
|     ! 0 | 10139 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 10140 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10141 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 10142 | `			rc = SXERR_INVALID;` |
|     ! 0 | 10143 | `		}` |
|     ! 0 | 10144 | `	}` |
|   14785 | 10145 | `	return rc;` |
|       5 | 10146 | `}` |
|       - | 10147 | `/*` |
|       - | 10148 | ` * Compile a 'throw' statement.` |
|       - | 10149 | ` * throw: This is how you trigger an exception.` |
|       - | 10150 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 10151 | ` */` |
|   14744 | 10152 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       5 | 10153 | `{` |
|   14749 | 10154 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10155 | `	GenBlock *pBlock;` |
|       - | 10156 | `	sxu32 nIdx;` |
|       - | 10157 | `	sxi32 rc;` |
|   14749 | 10158 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 10159 | `	/* Compile the expression */` |
|   14749 | 10160 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   14749 | 10161 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 10162 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 10163 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10164 | `			return SXERR_ABORT;` |
|       - | 10165 | `		}` |
|     ! 0 | 10166 | `		return SXRET_OK;` |
|       - | 10167 | `	}` |
|   14749 | 10168 | `	pBlock = pGen->pCurrent;` |
|       - | 10169 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   58389 | 10170 | `	while(pBlock->pParent){` |
|   58385 | 10171 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   14745 | 10172 | `			break;` |
|       - | 10173 | `		}` |
|       - | 10174 | `		/* Point to the parent block */` |
|   43645 | 10175 | `		pBlock = pBlock->pParent;` |
|       5 | 10176 | `	}` |
|       - | 10177 | `	/* Emit the throw instruction */` |
|   14749 | 10178 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 10179 | `	/* Emit the jump */` |
|   14749 | 10180 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   14749 | 10181 | `	return SXRET_OK;` |
|    7377 | 10182 | `}` |
|       - | 10183 | `/*` |
|       - | 10184 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - | 10185 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - | 10186 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - | 10187 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - | 10188 | ` * the validator guarantees the operand is a valid exception target.` |
|       - | 10189 | ` */` |
|      36 | 10190 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 | 10191 | `{` |
|      38 | 10192 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10193 | `	GenBlock *pBlock;` |
|       - | 10194 | `	sxu32 nIdx;` |
|       - | 10195 | `	sxi32 rc;` |
|      18 | 10196 | `	(void)iCompileFlag;` |
|      38 | 10197 | `	pGen->pIn++; /* Skip 'throw' */` |
|      38 | 10198 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 10199 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10200 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10201 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10202 | `			return SXERR_ABORT;` |
|       - | 10203 | `		}` |
|     ! 0 | 10204 | `		return SXRET_OK;` |
|       - | 10205 | `	}` |
|      38 | 10206 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      38 | 10207 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10208 | `		return SXERR_ABORT;` |
|       - | 10209 | `	}` |
|      38 | 10210 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 10211 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10212 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10213 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10214 | `			return SXERR_ABORT;` |
|       - | 10215 | `		}` |
|     ! 0 | 10216 | `		return SXRET_OK;` |
|       - | 10217 | `	}` |
|       - | 10218 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      38 | 10219 | `	pBlock = pGen->pCurrent;` |
|      60 | 10220 | `	while( pBlock->pParent ){` |
|      49 | 10221 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      27 | 10222 | `			break;` |
|       - | 10223 | `		}` |
|      23 | 10224 | `		pBlock = pBlock->pParent;` |
|       1 | 10225 | `	}` |
|      38 | 10226 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      38 | 10227 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      38 | 10228 | `	return SXRET_OK;` |
|      20 | 10229 | `}` |
|       - | 10230 | `/*` |
|       - | 10231 | ` * Compile a 'catch' block.` |
|       - | 10232 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 10233 | ` * an object containing the exception information.` |
|       - | 10234 | ` */` |
|     598 | 10235 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       5 | 10236 | `{` |
|     603 | 10237 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10238 | `	ph7_exception_block sCatch;` |
|       - | 10239 | `	SySet *pInstrContainer;` |
|       - | 10240 | `	SyString sClassName;` |
|       - | 10241 | `	GenBlock *pCatch;` |
|       - | 10242 | `	SyToken *pToken;` |
|       - | 10243 | `	SyString *pName;` |
|       - | 10244 | `	char *zDup;` |
|       - | 10245 | `	sxi32 rc;` |
|     603 | 10246 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 10247 | `	/* Zero the structure */` |
|     603 | 10248 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 10249 | `	/* Initialize fields */` |
|     603 | 10250 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     603 | 10251 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     603 | 10252 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - | 10253 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 10254 | `			pToken = pGen->pIn;` |
|     ! 0 | 10255 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10256 | `				pToken--;` |
|     ! 0 | 10257 | `			}` |
|     ! 0 | 10258 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10259 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10260 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10261 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10262 | `				return SXERR_ABORT;` |
|       - | 10263 | `			}` |
|     ! 0 | 10264 | `			return SXERR_INVALID;` |
|       - | 10265 | `	}` |
|       - | 10266 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     603 | 10267 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     313 | 10268 | `	for(;;){` |
|       - | 10269 | `		SyBlob sResolved;` |
|     631 | 10270 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     631 | 10271 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       6 | 10272 | `			SyBlobRelease(&sResolved);` |
|       6 | 10273 | `			pToken = pGen->pIn;` |
|       6 | 10274 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10275 | `				pToken--;` |
|     ! 0 | 10276 | `			}` |
|       8 | 10277 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10278 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 | 10279 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       6 | 10280 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10281 | `				return SXERR_ABORT;` |
|       - | 10282 | `			}` |
|       6 | 10283 | `			return SXERR_INVALID;` |
|       - | 10284 | `		}` |
|       - | 10285 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|       - | 10286 | `		 * transient SyBlob allocation. */` |
|     938 | 10287 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     622 | 10288 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     627 | 10289 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     627 | 10290 | `		SyBlobRelease(&sResolved);` |
|     627 | 10291 | `		if( zDup == 0 ){` |
|     ! 0 | 10292 | `			goto Mem;` |
|       - | 10293 | `		}` |
|     627 | 10294 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     627 | 10295 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10296 | `			goto Mem;` |
|       - | 10297 | `		}` |
|       - | 10298 | `		/* Check for '\|' (multi-catch separator) */` |
|     636 | 10299 | `		if( pGen->pIn < pGen->pEnd &&` |
|     622 | 10300 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      33 | 10301 | `			pGen->pIn->sData.nByte == 1 &&` |
|      28 | 10302 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      30 | 10303 | `			pGen->pIn++; /* Consume the '\|' */` |
|      30 | 10304 | `			continue;` |
|       - | 10305 | `		}` |
|     599 | 10306 | `		break;` |
|     ! 0 | 10307 | `	}` |
|     891 | 10308 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     599 | 10309 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 10310 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 10311 | `			pToken = pGen->pIn;` |
|     ! 0 | 10312 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10313 | `				pToken--;` |
|     ! 0 | 10314 | `			}` |
|     ! 0 | 10315 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10316 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10317 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10318 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10319 | `				return SXERR_ABORT;` |
|       - | 10320 | `			}` |
|     ! 0 | 10321 | `			return SXERR_INVALID;` |
|       - | 10322 | `	}` |
|     599 | 10323 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 10324 | `	/* Duplicate instance name */` |
|     599 | 10325 | `	pName = &pGen->pIn->sData;` |
|     599 | 10326 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     599 | 10327 | `	if( zDup == 0 ){` |
|     ! 0 | 10328 | `		goto Mem;` |
|       - | 10329 | `	}` |
|     599 | 10330 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     599 | 10331 | `	pGen->pIn++;` |
|     599 | 10332 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 10333 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 10334 | `		pToken = pGen->pIn;` |
|     ! 0 | 10335 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10336 | `			pToken--;` |
|     ! 0 | 10337 | `		}` |
|     ! 0 | 10338 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10339 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10340 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10341 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10342 | `			return SXERR_ABORT;` |
|       - | 10343 | `		}` |
|     ! 0 | 10344 | `		return SXERR_INVALID;` |
|       - | 10345 | `	}` |
|       - | 10346 | `	/* Compile the block */` |
|     599 | 10347 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 10348 | `	/* Create the catch block */` |
|     599 | 10349 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     599 | 10350 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10351 | `		return SXERR_ABORT;` |
|       - | 10352 | `	}` |
|       - | 10353 | `	/* Swap bytecode container */` |
|     599 | 10354 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     599 | 10355 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 10356 | `	/* Compile the block */` |
|     599 | 10357 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 10358 | `	/* Fix forward jumps now the destination is resolved  */` |
|     599 | 10359 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10360 | `	/* Emit the DONE instruction */` |
|     599 | 10361 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 10362 | `	/* Leave the block */` |
|     599 | 10363 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10364 | `	/* Restore the default container */` |
|     599 | 10365 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 10366 | `	/* Install the catch block */` |
|     599 | 10367 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     599 | 10368 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10369 | `		goto Mem;` |
|       - | 10370 | `	}` |
|     599 | 10371 | `	return SXRET_OK;` |
|     ! 0 | 10372 | `Mem:` |
|     ! 0 | 10373 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 10374 | `	return SXERR_ABORT;` |
|     304 | 10375 | `}` |
|       - | 10376 | `/*` |
|       - | 10377 | ` * Compile a 'try' block.` |
|       - | 10378 | ` * A function using an exception should be in a "try" block.` |
|       - | 10379 | ` * If the exception does not trigger, the code will continue` |
|       - | 10380 | ` * as normal. However if the exception triggers, an exception` |
|       - | 10381 | ` * is "thrown".` |
|       - | 10382 | ` */` |
|     644 | 10383 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       5 | 10384 | `{` |
|       - | 10385 | `	ph7_exception *pException;` |
|     649 | 10386 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10387 | `	GenBlock *pTry;` |
|       - | 10388 | `	sxu32 nJmpIdx;` |
|       - | 10389 | `	sxi32 rc;` |
|       - | 10390 | `	/* Create the exception container */` |
|     649 | 10391 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     649 | 10392 | `	if( pException == 0 ){` |
|     ! 0 | 10393 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 10394 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 10395 | `		return SXERR_ABORT;` |
|       - | 10396 | `	}` |
|       - | 10397 | `	/* Zero the structure */` |
|     649 | 10398 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 10399 | `	/* Initialize fields */` |
|     649 | 10400 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     649 | 10401 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     649 | 10402 | `	pException->iHasFinally = 0;` |
|     649 | 10403 | `	pException->iFinallyDone = 0;` |
|     649 | 10404 | `	pException->pVm = pGen->pVm;` |
|       - | 10405 | `	/* Create the try block */` |
|     649 | 10406 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     649 | 10407 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10408 | `		return SXERR_ABORT;` |
|       - | 10409 | `	}` |
|       - | 10410 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     649 | 10411 | `	pTry->pUserData = pException;` |
|       - | 10412 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     649 | 10413 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 10414 | `	/* Fix the jump later when the destination is resolved */` |
|     649 | 10415 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     649 | 10416 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 10417 | `	/* Compile the block */` |
|     649 | 10418 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     649 | 10419 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10420 | `		return SXERR_ABORT;` |
|       - | 10421 | `	}` |
|       - | 10422 | `	/* Fix forward jumps now the destination is resolved */` |
|     649 | 10423 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10424 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     649 | 10425 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 10426 | `	/* Leave the block */` |
|     649 | 10427 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10428 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     649 | 10429 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     642 | 10430 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 10431 | `		/* Compile one or more catch blocks */` |
|     594 | 10432 | `		for(;;){` |
|    1188 | 10433 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     962 | 10434 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     300 | 10435 | `					break;` |
|       - | 10436 | `			}` |
|     603 | 10437 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     603 | 10438 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10439 | `				return SXERR_ABORT;` |
|       - | 10440 | `			}` |
|       5 | 10441 | `		}` |
|     295 | 10442 | `	}` |
|       - | 10443 | `	/* Compile optional finally block */` |
|     649 | 10444 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     354 | 10445 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 10446 | `		SySet *pInstrContainer;` |
|       - | 10447 | `		GenBlock *pFinBlock;` |
|     115 | 10448 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 10449 | `		/* Create the finally block for jump fixup bookkeeping */` |
|     115 | 10450 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|     115 | 10451 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10452 | `			return SXERR_ABORT;` |
|       - | 10453 | `		}` |
|       - | 10454 | `		/* Swap bytecode container */` |
|     115 | 10455 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     115 | 10456 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 10457 | `		/* Compile the finally body */` |
|     115 | 10458 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     115 | 10459 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10460 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 10461 | `			return SXERR_ABORT;` |
|       - | 10462 | `		}` |
|       - | 10463 | `		/* Fix forward jumps now the destination is resolved */` |
|     115 | 10464 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10465 | `		/* Emit DONE to terminate the finally block */` |
|     115 | 10466 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 10467 | `		/* Leave the block */` |
|     115 | 10468 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10469 | `		/* Restore the default container */` |
|     115 | 10470 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     115 | 10471 | `		pException->iHasFinally = 1;` |
|      55 | 10472 | `	}` |
|       - | 10473 | `	/* Must have at least one catch or finally */` |
|     649 | 10474 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       8 | 10475 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10476 | `			"Cannot use try without catch or finally");` |
|       8 | 10477 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10478 | `			return SXERR_ABORT;` |
|       - | 10479 | `		}` |
|       3 | 10480 | `	}` |
|     649 | 10481 | `	return SXRET_OK;` |
|     327 | 10482 | `}` |
|       - | 10483 | `/*` |
|       - | 10484 | ` * Compile a switch block.` |
|       - | 10485 | ` *  (See block-comment below for more information)` |
|       - | 10486 | ` */` |
|     112 | 10487 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       5 | 10488 | `{` |
|     117 | 10489 | `	sxi32 rc = SXRET_OK;` |
|     117 | 10490 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 10491 | `		/* Unexpected token */` |
|     ! 0 | 10492 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 10493 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10494 | `			return SXERR_ABORT;` |
|       - | 10495 | `		}` |
|     ! 0 | 10496 | `		pGen->pIn++;` |
|     ! 0 | 10497 | `	}` |
|     117 | 10498 | `	pGen->pIn++;` |
|       - | 10499 | `	/* First instruction to execute in this block. */` |
|     117 | 10500 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 10501 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 10502 | `	 * or the '}' token */` |
|     206 | 10503 | `	for(;;){` |
|     417 | 10504 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10505 | `			/* No more input to process */` |
|     ! 0 | 10506 | `			break;` |
|       - | 10507 | `		}` |
|     417 | 10508 | `		rc = SXRET_OK;` |
|     417 | 10509 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      85 | 10510 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      31 | 10511 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 10512 | `					/* Unexpected token */` |
|     ! 0 | 10513 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 10514 | `						&pGen->pIn->sData);` |
|     ! 0 | 10515 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10516 | `						return SXERR_ABORT;` |
|       - | 10517 | `					}` |
|       - | 10518 | `					/* FALL THROUGH */` |
|     ! 0 | 10519 | `				}` |
|      31 | 10520 | `				rc = SXERR_EOF;` |
|      31 | 10521 | `				break;` |
|       - | 10522 | `			}` |
|      32 | 10523 | `		}else{` |
|       - | 10524 | `			sxi32 nKwrd;` |
|       - | 10525 | `			/* Extract the keyword */` |
|     337 | 10526 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     337 | 10527 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      47 | 10528 | `				break;` |
|       - | 10529 | `			}` |
|     253 | 10530 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 10531 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 10532 | `					/* Unexpected token */` |
|     ! 0 | 10533 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 10534 | `						&pGen->pIn->sData);` |
|     ! 0 | 10535 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10536 | `						return SXERR_ABORT;` |
|       - | 10537 | `					}` |
|       - | 10538 | `					/* FALL THROUGH */` |
|     ! 0 | 10539 | `				}` |
|       - | 10540 | `				/* Block compiled */` |
|       3 | 10541 | `				break;` |
|       - | 10542 | `			}` |
|       - | 10543 | `		}` |
|       - | 10544 | `		/* Compile block */` |
|     305 | 10545 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     305 | 10546 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10547 | `			return SXERR_ABORT;` |
|       - | 10548 | `		}` |
|       5 | 10549 | `	}` |
|     117 | 10550 | `	return rc;` |
|      61 | 10551 | `}` |
|       - | 10552 | `/*` |
|       - | 10553 | ` * Compile a case eXpression.` |
|       - | 10554 | ` *  (See block-comment below for more information)` |
|       - | 10555 | ` */` |
|      92 | 10556 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       5 | 10557 | `{` |
|       - | 10558 | `	SySet *pInstrContainer;` |
|       - | 10559 | `	SyToken *pEnd,*pTmp;` |
|      97 | 10560 | `	sxi32 iNest = 0;` |
|       - | 10561 | `	sxi32 rc;` |
|       - | 10562 | `	/* Delimit the expression */` |
|      97 | 10563 | `	pEnd = pGen->pIn;` |
|     197 | 10564 | `	while( pEnd < pGen->pEnd ){` |
|     197 | 10565 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 10566 | `			/* Increment nesting level */` |
|       3 | 10567 | `			iNest++;` |
|     196 | 10568 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 10569 | `			/* Decrement nesting level */` |
|       3 | 10570 | `			iNest--;` |
|     194 | 10571 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      97 | 10572 | `			break;` |
|       - | 10573 | `		}` |
|     105 | 10574 | `		pEnd++;` |
|       5 | 10575 | `	}` |
|      97 | 10576 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 10577 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 10578 | `		if( rc == SXERR_ABORT ){` |
|       - | 10579 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10580 | `			return SXERR_ABORT;` |
|       - | 10581 | `		}` |
|     ! 0 | 10582 | `	}` |
|       - | 10583 | `	/* Swap token stream */` |
|      97 | 10584 | `	pTmp = pGen->pEnd;` |
|      97 | 10585 | `	pGen->pEnd = pEnd;` |
|      97 | 10586 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      97 | 10587 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      97 | 10588 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 10589 | `	/* Emit the done instruction */` |
|      97 | 10590 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      97 | 10591 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 10592 | `	/* Update token stream */` |
|      97 | 10593 | `	pGen->pIn  = pEnd;` |
|      97 | 10594 | `	pGen->pEnd = pTmp;` |
|      97 | 10595 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10596 | `		return SXERR_ABORT;` |
|       - | 10597 | `	}` |
|      97 | 10598 | `	return SXRET_OK;` |
|      51 | 10599 | `}` |
|       - | 10600 | `/*` |
|       - | 10601 | ` * Compile the smart switch statement.` |
|       - | 10602 | ` * According to the PHP language reference manual` |
|       - | 10603 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 10604 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 10605 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 10606 | ` *  This is exactly what the switch statement is for.` |
|       - | 10607 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 10608 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 10609 | ` *  of the outer loop, use continue 2.` |
|       - | 10610 | ` *  Note that switch/case does loose comparision.` |
|       - | 10611 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 10612 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 10613 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 10614 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 10615 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 10616 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 10617 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 10618 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 10619 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 10620 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 10621 | ` *  list for the next case.` |
|       - | 10622 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 10623 | ` *  or floating-point numbers and strings.` |
|       - | 10624 | ` */` |
|      28 | 10625 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       5 | 10626 | `{` |
|       - | 10627 | `	GenBlock *pSwitchBlock;` |
|       - | 10628 | `	SyToken *pTmp,*pEnd;` |
|       - | 10629 | `	ph7_switch *pSwitch;` |
|       - | 10630 | `	sxu32 nToken;` |
|       - | 10631 | `	sxu32 nLine;` |
|       - | 10632 | `	sxi32 rc;` |
|      33 | 10633 | `	nLine = pGen->pIn->nLine;` |
|       - | 10634 | `	/* Jump the 'switch' keyword */` |
|      33 | 10635 | `	pGen->pIn++;` |
|      33 | 10636 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 10637 | `		/* Syntax error */` |
|     ! 0 | 10638 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 10639 | `		if( rc == SXERR_ABORT ){` |
|       - | 10640 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10641 | `			return SXERR_ABORT;` |
|       - | 10642 | `		}` |
|     ! 0 | 10643 | `		goto Synchronize;` |
|       - | 10644 | `	}` |
|       - | 10645 | `	/* Jump the left parenthesis '(' */` |
|      33 | 10646 | `	pGen->pIn++;` |
|      33 | 10647 | `	pEnd = 0; /* cc warning */` |
|       - | 10648 | `	/* Create the loop block */` |
|      47 | 10649 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 | 10650 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      33 | 10651 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10652 | `		return SXERR_ABORT;` |
|       - | 10653 | `	}` |
|       - | 10654 | `	/* Delimit the condition */` |
|      33 | 10655 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      33 | 10656 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 10657 | `		/* Empty expression */` |
|     ! 0 | 10658 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 10659 | `		if( rc == SXERR_ABORT ){` |
|       - | 10660 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10661 | `			return SXERR_ABORT;` |
|       - | 10662 | `		}` |
|     ! 0 | 10663 | `	}` |
|       - | 10664 | `	/* Swap token streams */` |
|      33 | 10665 | `	pTmp = pGen->pEnd;` |
|      33 | 10666 | `	pGen->pEnd = pEnd;` |
|       - | 10667 | `	/* Compile the expression */` |
|      33 | 10668 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      33 | 10669 | `	if( rc == SXERR_ABORT ){` |
|       - | 10670 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 10671 | `		return SXERR_ABORT;` |
|       - | 10672 | `	}` |
|       - | 10673 | `	/* Update token stream */` |
|      33 | 10674 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 10675 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 10676 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 10677 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10678 | `			return SXERR_ABORT;` |
|       - | 10679 | `		}` |
|     ! 0 | 10680 | `		pGen->pIn++;` |
|     ! 0 | 10681 | `	}` |
|      33 | 10682 | `	pGen->pIn  = &pEnd[1];` |
|      33 | 10683 | `	pGen->pEnd = pTmp;` |
|      33 | 10684 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 | 10685 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 10686 | `			pTmp = pGen->pIn;` |
|     ! 0 | 10687 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 10688 | `				pTmp--;` |
|     ! 0 | 10689 | `			}` |
|       - | 10690 | `			/* Unexpected token */` |
|     ! 0 | 10691 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 10692 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10693 | `				return SXERR_ABORT;` |
|       - | 10694 | `			}` |
|     ! 0 | 10695 | `			goto Synchronize;` |
|       - | 10696 | `	}` |
|       - | 10697 | `	/* Set the delimiter token */` |
|      33 | 10698 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 10699 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 10700 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 10701 | `	}else{` |
|      31 | 10702 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 10703 | `	}` |
|      33 | 10704 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 10705 | `	/* Create the switch blocks container */` |
|      33 | 10706 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      33 | 10707 | `	if( pSwitch == 0 ){` |
|       - | 10708 | `		/* Abort compilation */` |
|     ! 0 | 10709 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 10710 | `		return SXERR_ABORT;` |
|       - | 10711 | `	}` |
|       - | 10712 | `	/* Zero the structure */` |
|      33 | 10713 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 10714 | `	/* Initialize fields */` |
|      33 | 10715 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 10716 | `	/* Emit the switch instruction */` |
|      33 | 10717 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 10718 | `	/* Compile case blocks */` |
|     100 | 10719 | `	for(;;){` |
|       - | 10720 | `		sxu32 nKwrd;` |
|     119 | 10721 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10722 | `			/* No more input to process */` |
|     ! 0 | 10723 | `			break;` |
|       - | 10724 | `		}` |
|     119 | 10725 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 10726 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 10727 | `				/* Unexpected token */` |
|     ! 0 | 10728 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10729 | `					&pGen->pIn->sData);` |
|     ! 0 | 10730 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10731 | `					return SXERR_ABORT;` |
|       - | 10732 | `				}` |
|       - | 10733 | `				/* FALL THROUGH */` |
|     ! 0 | 10734 | `			}` |
|       - | 10735 | `			/* Block compiled */` |
|     ! 0 | 10736 | `			break;` |
|       - | 10737 | `		}` |
|       - | 10738 | `		/* Extract the keyword */` |
|     119 | 10739 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     119 | 10740 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 10741 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 10742 | `				/* Unexpected token */` |
|     ! 0 | 10743 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10744 | `					&pGen->pIn->sData);` |
|     ! 0 | 10745 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10746 | `					return SXERR_ABORT;` |
|       - | 10747 | `				}` |
|       - | 10748 | `				/* FALL THROUGH */` |
|     ! 0 | 10749 | `			}` |
|       - | 10750 | `			/* Block compiled */` |
|       3 | 10751 | `			break;` |
|       - | 10752 | `		}` |
|     117 | 10753 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 10754 | `			/*` |
|       - | 10755 | `			 * Accroding to the PHP language reference manual` |
|       - | 10756 | `			 *  A special case is the default case. This case matches anything` |
|       - | 10757 | `			 *  that wasn't matched by the other cases.` |
|       - | 10758 | `			 */` |
|      25 | 10759 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 10760 | `				/* Default case already compiled */` |
|     ! 0 | 10761 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 10762 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10763 | `					return SXERR_ABORT;` |
|       - | 10764 | `				}` |
|     ! 0 | 10765 | `			}` |
|      25 | 10766 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 10767 | `			/* Compile the default block */` |
|      25 | 10768 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      25 | 10769 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 10770 | `				return SXERR_ABORT;` |
|      25 | 10771 | `			}else if( rc == SXERR_EOF ){` |
|      23 | 10772 | `				break;` |
|       1 | 10773 | `			}` |
|      98 | 10774 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 10775 | `			ph7_case_expr sCase;` |
|       - | 10776 | `			/* Standard case block */` |
|      97 | 10777 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 10778 | `			/* initialize the structure */` |
|      97 | 10779 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 10780 | `			/* Compile the case expression */` |
|      97 | 10781 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      97 | 10782 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10783 | `				return SXERR_ABORT;` |
|       - | 10784 | `			}` |
|       - | 10785 | `			/* Compile the case block */` |
|      97 | 10786 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 10787 | `			/* Insert in the switch container */` |
|      97 | 10788 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      97 | 10789 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 10790 | `				return SXERR_ABORT;` |
|      97 | 10791 | `			}else if( rc == SXERR_EOF ){` |
|       9 | 10792 | `				break;` |
|       - | 10793 | `			}` |
|      47 | 10794 | `		}else{` |
|       - | 10795 | `			/* Unexpected token */` |
|     ! 0 | 10796 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10797 | `				&pGen->pIn->sData);` |
|     ! 0 | 10798 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10799 | `				return SXERR_ABORT;` |
|       - | 10800 | `			}` |
|     ! 0 | 10801 | `			break;` |
|       - | 10802 | `		}` |
|       5 | 10803 | `	}` |
|       - | 10804 | `	/* Fix all jumps now the destination is resolved */` |
|      33 | 10805 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      33 | 10806 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10807 | `	/* Release the loop block */` |
|      33 | 10808 | `	GenStateLeaveBlock(pGen,0);` |
|      33 | 10809 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 10810 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      33 | 10811 | `		pGen->pIn++;` |
|      14 | 10812 | `	}` |
|       - | 10813 | `	/* Statement successfully compiled */` |
|      33 | 10814 | `	return SXRET_OK;` |
|     ! 0 | 10815 | `Synchronize:` |
|       - | 10816 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 10817 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 10818 | `		pGen->pIn++;` |
|     ! 0 | 10819 | `	}` |
|     ! 0 | 10820 | `	return SXRET_OK;` |
|      19 | 10821 | `}` |
|       - | 10822 | `/*` |
|       - | 10823 | ` * Chain operators participate in a postfix member-access chain.` |
|       - | 10824 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|       - | 10825 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|       - | 10826 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|       - | 10827 | ` */` |
|       - | 10828 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|       - | 10829 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|       - | 10830 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|       - | 10831 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|       - | 10832 |  |
|       - | 10833 | `/*` |
|       - | 10834 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|       - | 10835 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|       - | 10836 | ` * patched entries from the pending set.` |
|       - | 10837 | ` */` |
| 2684480 | 10838 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|       5 | 10839 | `{` |
| 2684485 | 10840 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|       - | 10841 | `	sxu32 nTarget;` |
|       - | 10842 | `	sxu32 *aIdx;` |
|       - | 10843 | `	sxu32 i;` |
| 2684485 | 10844 | `	if( nCur <= nBaseline ){` |
| 2684391 | 10845 | `		return;` |
|       - | 10846 | `	}` |
|      98 | 10847 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      98 | 10848 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|     200 | 10849 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|     106 | 10850 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|     106 | 10851 | `		if( pInstr ){` |
|     106 | 10852 | `			pInstr->iP2 = (sxi32)nTarget;` |
|      51 | 10853 | `		}` |
|      55 | 10854 | `	}` |
|      98 | 10855 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 1342245 | 10856 | `}` |
|       - | 10857 |  |
|       - | 10858 | `/*` |
|       - | 10859 | ` * By-reference out-parameters of builtin functions.` |
|       - | 10860 | ` *` |
|       - | 10861 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|       - | 10862 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|       - | 10863 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|       - | 10864 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|       - | 10865 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|       - | 10866 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|       - | 10867 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|       - | 10868 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|       - | 10869 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|       - | 10870 | ` * creates it" behaviour).` |
|       - | 10871 | ` *` |
|       - | 10872 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|       - | 10873 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|       - | 10874 | ` */` |
|  451076 | 10875 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|       5 | 10876 | `{` |
|       - | 10877 | `	static const struct {` |
|       - | 10878 | `		const char *zName;` |
|       - | 10879 | `		sxu32 nByte;` |
|       - | 10880 | `		sxu32 mask;` |
|       - | 10881 | `	} aByRef[] = {` |
|       - | 10882 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 10883 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 10884 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 10885 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 10886 | `	};` |
|       - | 10887 | `	sxu32 i;` |
|  451081 | 10888 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|    1633 | 10889 | `		return 0;` |
|       - | 10890 | `	}` |
| 2246973 | 10891 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 1797614 | 10892 | `		if( pName->nByte == aByRef[i].nByte` |
|  921470 | 10893 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|      99 | 10894 | `			return aByRef[i].mask;` |
|       - | 10895 | `		}` |
|  898765 | 10896 | `	}` |
|  449359 | 10897 | `	return 0;` |
|  225543 | 10898 | `}` |
|       - | 10899 | `/*` |
|       - | 10900 | ` * Recover the bare global-builtin name from a call's callee node.` |
|       - | 10901 | ` *` |
|       - | 10902 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|       - | 10903 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|       - | 10904 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|       - | 10905 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|       - | 10906 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|       - | 10907 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|       - | 10908 | ` */` |
|  451076 | 10909 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|       5 | 10910 | `{` |
|       - | 10911 | `	SyToken *p, *pEnd;` |
|  451081 | 10912 | `	pOut->zString = 0;` |
|  451081 | 10913 | `	pOut->nByte = 0;` |
|  451081 | 10914 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|     ! 0 | 10915 | `		return;` |
|       - | 10916 | `	}` |
|  451081 | 10917 | `	p = pLeft->pStart;` |
|  451081 | 10918 | `	pEnd = pLeft->pEnd;` |
|       - | 10919 | `	/* Optional single leading namespace separator (absolute path). */` |
|  451081 | 10920 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|    3639 | 10921 | `		p++;` |
|    1817 | 10922 | `	}` |
|  451081 | 10923 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|    1605 | 10924 | `		return;` |
|       - | 10925 | `	}` |
|       - | 10926 | `	/* Must be a single component: nothing follows the name token. */` |
|  449481 | 10927 | `	if( p + 1 != pEnd ){` |
|      32 | 10928 | `		return;` |
|       - | 10929 | `	}` |
|  449453 | 10930 | `	*pOut = p->sData;` |
|  225543 | 10931 | `}` |
|       - | 10932 | `/*` |
|       - | 10933 | ` * Generate bytecode for a given expression tree.` |
|       - | 10934 | ` * If something goes wrong while generating bytecode` |
|       - | 10935 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 10936 | ` * this function takes care of generating the appropriate` |
|       - | 10937 | ` * error message.` |
|       - | 10938 | ` */` |
| 3592352 | 10939 | `static sxi32 GenStateEmitExprCode(` |
|       - | 10940 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 10941 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 10942 | `	sxi32 iFlags /* Control flags */` |
|       - | 10943 | `	)` |
|       5 | 10944 | `{` |
|       - | 10945 | `	VmInstr *pInstr;` |
|       - | 10946 | `	sxu32 nJmpIdx;` |
| 3592357 | 10947 | `	sxi32 iP1 = 0;` |
| 3592357 | 10948 | `	sxu32 iP2 = 0;` |
| 3592357 | 10949 | `	void *p3  = 0;` |
|       - | 10950 | `	sxi32 iVmOp;` |
|       - | 10951 | `	sxi32 rc;` |
| 3592357 | 10952 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 3592357 | 10953 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
| 3592357 | 10954 | `	sxu32 nRhsNsBase = 0;` |
| 3592357 | 10955 | `	if( pNode->xCode ){` |
|       - | 10956 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 10957 | `		/* Compile node */` |
| 2242481 | 10958 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 2242481 | 10959 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 2242481 | 10960 | `		RE_SWAP_DELIMITER(pGen);` |
| 2242481 | 10961 | `		return rc;` |
|       - | 10962 | `	}` |
| 1349881 | 10963 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 10964 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 10965 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 10966 | `		return SXERR_ABORT;` |
|       - | 10967 | `	}` |
| 1349881 | 10968 | `	iVmOp = pNode->pOp->iVmOp;` |
| 1349881 | 10969 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      65 | 10970 | `		sxu32 nJmp = 0;` |
|       - | 10971 | `		sxu32 nNcNsBase;` |
|       - | 10972 | `		VmInstr *pInstrFix;` |
|       - | 10973 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - | 10974 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - | 10975 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - | 10976 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - | 10977 | `		 * stack slot carries a writable nIdx. */` |
|      65 | 10978 | `		if( pNode->pRight ){` |
|      65 | 10979 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      65 | 10980 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|      65 | 10981 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10982 | `				return rc;` |
|       - | 10983 | `			}` |
|      65 | 10984 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       - | 10985 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - | 10986 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - | 10987 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - | 10988 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - | 10989 | `			 * the store, so the parent array does not need to be copied at` |
|       - | 10990 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - | 10991 | `			 * cascade for the actual write path stays correct. */` |
|      65 | 10992 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      65 | 10993 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      31 | 10994 | `				pInstrFix->iP2 = 3;` |
|      14 | 10995 | `			}` |
|      31 | 10996 | `		}` |
|       - | 10997 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      65 | 10998 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - | 10999 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      65 | 11000 | `		if( pNode->pLeft ){` |
|      65 | 11001 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      65 | 11002 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      65 | 11003 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11004 | `				return rc;` |
|       - | 11005 | `			}` |
|      65 | 11006 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|      31 | 11007 | `		}` |
|       - | 11008 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      65 | 11009 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - | 11010 | `		/* Patch the short-circuit jump to land after the store. */` |
|      65 | 11011 | `		if( nJmp > 0 ){` |
|      65 | 11012 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      65 | 11013 | `			if( pInstrFix ){` |
|      65 | 11014 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      31 | 11015 | `			}` |
|      31 | 11016 | `		}` |
|      65 | 11017 | `		return SXRET_OK;` |
|       - | 11018 | `	}` |
| 1349819 | 11019 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 11020 | `		sxu32 nJz,nJmp;` |
|       - | 11021 | `		sxu32 nTernaryNsBase;` |
|       - | 11022 | `		/* Ternary operator require special handling */` |
|       - | 11023 | `		/* Phase#1: Compile the condition */` |
|    2669 | 11024 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2669 | 11025 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    2669 | 11026 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 11027 | `			return rc;` |
|       - | 11028 | `		}` |
|       - | 11029 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|       - | 11030 | `		 * compiling the condition must short-circuit to the end of the` |
|       - | 11031 | `		 * condition expression, not leak past the ternary. */` |
|    2669 | 11032 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    2669 | 11033 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    2669 | 11034 | `		if( pNode->pLeft ){` |
|       - | 11035 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 11036 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    2601 | 11037 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 11038 | `			/* Phase#3: Compile the 'then' expression  */` |
|    2601 | 11039 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2601 | 11040 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    2601 | 11041 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11042 | `				return rc;` |
|       - | 11043 | `			}` |
|    2601 | 11044 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1303 | 11045 | `		}else{` |
|       - | 11046 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 11047 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 11048 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 11049 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 11050 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 11051 | `		}` |
|       - | 11052 | `		/* Phase#4: Emit the unconditional jump */` |
|    2669 | 11053 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 11054 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    2669 | 11055 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    2669 | 11056 | `		if( pInstr ){` |
|    2669 | 11057 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1332 | 11058 | `		}` |
|    2669 | 11059 | `		if( !pNode->pLeft ){` |
|       - | 11060 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 11061 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 11062 | `		}` |
|       - | 11063 | `		/* Phase#6: Compile the 'else' expression */` |
|    2669 | 11064 | `		if( pNode->pRight ){` |
|    2669 | 11065 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2669 | 11066 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    2669 | 11067 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11068 | `				return rc;` |
|       - | 11069 | `			}` |
|    2669 | 11070 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1332 | 11071 | `		}` |
|    2669 | 11072 | `		if( nJmp > 0 ){` |
|       - | 11073 | `			/* Phase#7: Fix the unconditional jump */` |
|    2669 | 11074 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    2669 | 11075 | `			if( pInstr ){` |
|    2669 | 11076 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1332 | 11077 | `			}` |
|    1332 | 11078 | `		}` |
|       - | 11079 | `		/* All done */` |
|    2669 | 11080 | `		return SXRET_OK;` |
|       - | 11081 | `	}` |
| 1347155 | 11082 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|       - | 11083 | `	/* Generate code for the left tree */` |
| 1347155 | 11084 | `	if( pNode->pLeft ){` |
| 1347115 | 11085 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 1347115 | 11086 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 11087 | `			ph7_expr_node **apNode;` |
|  454835 | 11088 | `			int hasSpread = 0;` |
|  454835 | 11089 | `			int hasNamed = 0;` |
|  454835 | 11090 | `			int bAnySpread = 0;` |
|  454835 | 11091 | `			sxu32 byRefMask = 0;` |
|       - | 11092 | `			sxi32 nArgs;` |
|       - | 11093 | `			sxi32 n;` |
|       - | 11094 | `			/* Recurse and generate bytecodes for function arguments */` |
|  454835 | 11095 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  454835 | 11096 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 11097 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|       - | 11098 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|       - | 11099 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|  454835 | 11100 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|      65 | 11101 | `				bFcc = 1;` |
|      65 | 11102 | `				nArgs = 0;` |
|      32 | 11103 | `			}` |
|       - | 11104 | `			/* Validate: no positional arguments after named arguments */` |
|       - | 11105 | `			{` |
|  454835 | 11106 | `				int seenNamed = 0;` |
|  922917 | 11107 | `				for( n = 0; n < nArgs; ++n ){` |
|  468089 | 11108 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     216 | 11109 | `						seenNamed = 1;` |
|     216 | 11110 | `						hasNamed = 1;` |
|  467983 | 11111 | `					}else if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|    3641 | 11112 | `						bAnySpread = 1;` |
|  466059 | 11113 | `					}else if( seenNamed ){` |
|       3 | 11114 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|       - | 11115 | `							"Cannot use positional argument after named argument");` |
|       3 | 11116 | `						return SXERR_SYNTAX;` |
|       - | 11117 | `					}` |
|  234046 | 11118 | `				}` |
|       - | 11119 | `			}` |
|       - | 11120 | `			/* Read-only load */` |
|  454833 | 11121 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|       - | 11122 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|       - | 11123 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|       - | 11124 | `			 * objects dispatch to the right method (offsetExists for both;` |
|       - | 11125 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|  454833 | 11126 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|  454833 | 11127 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  454828 | 11128 | `				if( pCallName->nByte == 5` |
|  248321 | 11129 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|   22005 | 11130 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|  443833 | 11131 | `				}else if( pCallName->nByte == 5` |
|  226321 | 11132 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|      91 | 11133 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|      43 | 11134 | `				}` |
|       - | 11135 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|       - | 11136 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|       - | 11137 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|       - | 11138 | `				 * write back through. Skipped when spread/named args are present:` |
|       - | 11139 | `				 * the compile-time positional index no longer maps to the` |
|       - | 11140 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|  454833 | 11141 | `				if( !bAnySpread && !hasNamed ){` |
|       - | 11142 | `					SyString sBuiltin;` |
|  451081 | 11143 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|  451081 | 11144 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|  225538 | 11145 | `				}` |
|  227414 | 11146 | `			}` |
|  922913 | 11147 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  468085 | 11148 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  468085 | 11149 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|       - | 11150 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|       - | 11151 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|       - | 11152 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|       - | 11153 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|       - | 11154 | `				 * builtin to write back through. A plain $var target is unaffected` |
|       - | 11155 | `				 * (iP1=0 either way). See PLAN.md §2 for the full rationale. */` |
|  468085 | 11156 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|      55 | 11157 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|      55 | 11158 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|      25 | 11159 | `				}` |
|  468085 | 11160 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  468085 | 11161 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 11162 | `					return rc;` |
|       - | 11163 | `				}` |
|       - | 11164 | `				/* Each argument is an independent nullsafe scope. */` |
|  468085 | 11165 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  468085 | 11166 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 11167 | `					/* Emit spread opcode to unpack this array argument */` |
|    3641 | 11168 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|    3641 | 11169 | `					hasSpread = 1;` |
|    1818 | 11170 | `				}` |
|  234045 | 11171 | `			}` |
|       - | 11172 | `			/* Total number of given arguments */` |
|  454833 | 11173 | `			iP1 = nArgs;` |
|  454833 | 11174 | `			iP2 = hasSpread;` |
|       - | 11175 | `			/* Build VmCallArgMap if named arguments are present.` |
|       - | 11176 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  454833 | 11177 | `			if( hasNamed ){` |
|     119 | 11178 | `				sxu32 nStrBytes = 0;` |
|       - | 11179 | `				char *zBuf;` |
|     347 | 11180 | `				for( n = 0; n < nArgs; ++n ){` |
|     231 | 11181 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     213 | 11182 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|     105 | 11183 | `					}` |
|     117 | 11184 | `				}` |
|       - | 11185 | `				{` |
|     119 | 11186 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|     119 | 11187 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|     116 | 11188 | `					&pGen->pVm->sAllocator, mapSize);` |
|     119 | 11189 | `				if( pMap ){` |
|     119 | 11190 | `					SyZero(pMap, mapSize);` |
|     119 | 11191 | `					pMap->bHasNamed = 1;` |
|     119 | 11192 | `					pMap->nTotal = (sxu32)nArgs;` |
|     119 | 11193 | `					pMap->aNames = (SyString *)&pMap[1];` |
|     119 | 11194 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     347 | 11195 | `					for( n = 0; n < nArgs; ++n ){` |
|     231 | 11196 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     213 | 11197 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|     213 | 11198 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|     213 | 11199 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|     213 | 11200 | `							zBuf += nb;` |
|     105 | 11201 | `						}` |
|       - | 11202 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|     117 | 11203 | `					}` |
|     119 | 11204 | `					p3 = (void *)pMap;` |
|      58 | 11205 | `				}` |
|       - | 11206 | `				}` |
|      58 | 11207 | `			}` |
|       - | 11208 | `			/* Remove stale flags now */` |
|  454833 | 11209 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  227414 | 11210 | `		}` |
|       - | 11211 | `		{` |
|       - | 11212 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|       - | 11213 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|       - | 11214 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|       - | 11215 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|       - | 11216 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|       - | 11217 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|       - | 11218 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|       - | 11219 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
| 1347113 | 11220 | `			sxi32 iLeftFlags = iFlags;` |
| 1520787 | 11221 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  855217 | 11222 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|  355367 | 11223 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  347388 | 11224 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   16153 | 11225 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|    8074 | 11226 | `			}` |
|       - | 11227 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|       - | 11228 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|       - | 11229 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|       - | 11230 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|       - | 11231 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|       - | 11232 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|       - | 11233 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
| 1930802 | 11234 | `			if( pNode->pOp` |
| 1347113 | 11235 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
| 1257299 | 11236 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
| 1167439 | 11237 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|  180059 | 11238 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   90027 | 11239 | `			}` |
| 1347113 | 11240 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|       - | 11241 | `		}` |
| 1347113 | 11242 | `		if( rc != SXRET_OK ){` |
|      34 | 11243 | `			return rc;` |
|       - | 11244 | `		}` |
| 1347083 | 11245 | `		if( !bIsChainOp ){` |
|       - | 11246 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|       - | 11247 | `			 * target the end of that LHS chain, which is right here. */` |
|  619291 | 11248 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  309643 | 11249 | `		}` |
| 1347083 | 11250 | `		if( iVmOp == PH7_OP_CALL ){` |
|  454833 | 11251 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  454833 | 11252 | `			if( pInstr ){` |
|  454833 | 11253 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  449575 | 11254 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 11255 | `					sxu32 nQual;` |
|  449575 | 11256 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 11257 | `					/* Prevent constant expansion but preserve the absolute flag` |
|       - | 11258 | `					 * so the later NEW handler (if any) can see it. */` |
|  449575 | 11259 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|       - | 11260 | `					/* Namespace-qualify the function name for CALL, unless the` |
|       - | 11261 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|       - | 11262 | `					 * imports — class imports must NOT affect function` |
|       - | 11263 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|       - | 11264 | `					 * before NEW; we store the original literal index in the` |
|       - | 11265 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|       - | 11266 | `					 * the unqualified name and re-qualify with class imports. */` |
|  449575 | 11267 | `					if( bAbsolute ){` |
|    3639 | 11268 | `						pInstr->iP2 = (sxi32)nOrig;` |
|    1822 | 11269 | `					}else{` |
|  445941 | 11270 | `						int fromImport = 0;` |
|  445941 | 11271 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  445941 | 11272 | `						pInstr->iP2 = (sxi32)nQual;` |
|  445941 | 11273 | `						if( nQual != nOrig ){` |
|       - | 11274 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 11275 | `							 * NEW handler can recover the unqualified name. */` |
|      77 | 11276 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      77 | 11277 | `							if( !fromImport ){` |
|       - | 11278 | `								/* Mark as namespace-qualified via VmCallArgMap */` |
|      67 | 11279 | `								if( p3 == 0 ){` |
|      67 | 11280 | `									VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      62 | 11281 | `										&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      67 | 11282 | `									if( pMap ){` |
|      67 | 11283 | `										SyZero(pMap, sizeof(VmCallArgMap));` |
|      67 | 11284 | `										p3 = (void *)pMap;` |
|      31 | 11285 | `									}` |
|      31 | 11286 | `								}` |
|      67 | 11287 | `								if( p3 ){` |
|      67 | 11288 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|      31 | 11289 | `								}` |
|      31 | 11290 | `							}` |
|      36 | 11291 | `						}` |
|       5 | 11292 | `					}` |
|  230048 | 11293 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 11294 | `					/* Method call,flag that */` |
|    1213 | 11295 | `					pInstr->iP2 = 1;` |
|     604 | 11296 | `				}` |
|  227419 | 11297 | `			}` |
| 1119669 | 11298 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 11299 | `			ph7_expr_node **apNode;` |
|       - | 11300 | `			sxi32 n;` |
|   92915 | 11301 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|       - | 11302 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|       - | 11303 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|       - | 11304 | `			/* Recurse and generate bytecodes for array index */` |
|   92915 | 11305 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  167665 | 11306 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   74755 | 11307 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   74755 | 11308 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   74755 | 11309 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 11310 | `					return rc;` |
|       - | 11311 | `				}` |
|       - | 11312 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   74755 | 11313 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   37380 | 11314 | `			}` |
|   92915 | 11315 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   74755 | 11316 | `				iP1 = 1; /* Node have an index associated with it */` |
|   37375 | 11317 | `			}` |
|   92915 | 11318 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       - | 11319 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|     243 | 11320 | `				iP2 = 4;` |
|   92796 | 11321 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       - | 11322 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|       - | 11323 | `				 * so the trailing unset() builtin can drop the slot. */` |
|      56 | 11324 | `				iP2 = 5;` |
|   92651 | 11325 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       - | 11326 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|       - | 11327 | `				 * short-circuit on missing keys without invoking offsetGet` |
|       - | 11328 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|      29 | 11329 | `				iP2 = 6;` |
|   92613 | 11330 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 11331 | `				/* Create an empty entry when the desired index is not found */` |
|   36627 | 11332 | `				iP2 = 1;` |
|   18316 | 11333 | `			}` |
|  845800 | 11334 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 11335 | `			/* POP the left node */` |
|      32 | 11336 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 11337 | `		}` |
|  673539 | 11338 | `	}` |
| 1347123 | 11339 | `	rc = SXRET_OK;` |
| 1347123 | 11340 | `	nJmpIdx = 0;` |
|       - | 11341 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 11342 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 11343 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 1347123 | 11344 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     377 | 11345 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     377 | 11346 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     377 | 11347 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     377 | 11348 | `			int isSpecial = 0;` |
|     377 | 11349 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     281 | 11350 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     281 | 11351 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     293 | 11352 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     259 | 11353 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     132 | 11354 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      98 | 11355 | `					isSpecial = 1;` |
|      47 | 11356 | `				}` |
|     162 | 11357 | `			}` |
|     425 | 11358 | `			pInstr->iP1 = 0;` |
|     425 | 11359 | `			if( !isSpecial ){` |
|     235 | 11360 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     115 | 11361 | `			}` |
|       - | 11362 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 11363 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     329 | 11364 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     235 | 11365 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     235 | 11366 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      46 | 11367 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      48 | 11368 | `					return SXRET_OK;` |
|       - | 11369 | `				}` |
|      93 | 11370 | `			}` |
|     140 | 11371 | `		}` |
|     221 | 11372 | `	}` |
|       - | 11373 | `	/* Generate code for the right tree */` |
| 1347041 | 11374 | `	if( pNode->pRight ){` |
|  727005 | 11375 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 11376 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|   11343 | 11377 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  721336 | 11378 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 11379 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    3795 | 11380 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  713772 | 11381 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 11382 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     129 | 11383 | `			iVmOp = 0; /* No binary operator to emit */` |
|     129 | 11384 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  711866 | 11385 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       - | 11386 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|       - | 11387 | `			 * the entire containing postfix chain to null. The jump target is` |
|       - | 11388 | `			 * patched later by the innermost non-chain ancestor (or by` |
|       - | 11389 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|       - | 11390 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|       - | 11391 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|     106 | 11392 | `			sxu32 nNsJmp = 0;` |
|     106 | 11393 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|     106 | 11394 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  711702 | 11395 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|       - | 11396 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|       - | 11397 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|       - | 11398 | `			 * auto-created — PHP auto-vivifies on write. */` |
|  302455 | 11399 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|  151225 | 11400 | `		}` |
|  727005 | 11401 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  727005 | 11402 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  727005 | 11403 | `		if( !bIsChainOp ){` |
|       - | 11404 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|       - | 11405 | `			 * operator instruction is emitted. */` |
|  546995 | 11406 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  273495 | 11407 | `		}` |
|  727005 | 11408 | `		if( iVmOp == PH7_OP_STORE ){` |
|  298585 | 11409 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  298554 | 11410 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 11411 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 11412 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 11413 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 11414 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 11415 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 11416 | `				 */` |
|      80 | 11417 | `				iVmOp = 0;` |
|  298547 | 11418 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  298509 | 11419 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 11420 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   79971 | 11421 | `					iP2 = 1;` |
|   39988 | 11422 | `				}else{` |
|  218543 | 11423 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 11424 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   36551 | 11425 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   36551 | 11426 | `						iP1 = pInstr->iP1;` |
|   18278 | 11427 | `					}else{` |
|  181997 | 11428 | `						p3 = pInstr->p3;` |
|       - | 11429 | `					}` |
|       - | 11430 | `					/* POP the last dynamic load instruction */` |
|  218543 | 11431 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 11432 | `				}` |
|  149257 | 11433 | `			}` |
|  577715 | 11434 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      54 | 11435 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      54 | 11436 | `			if( pInstr ){` |
|      54 | 11437 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 11438 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 11439 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 11440 | `					 */` |
|      17 | 11441 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      17 | 11442 | `					iP1 = pInstr->iP1;` |
|      17 | 11443 | `					iP2 = pInstr->iP2;` |
|      17 | 11444 | `					p3  = pInstr->p3;` |
|       9 | 11445 | `				}else{` |
|      38 | 11446 | `					p3 = pInstr->p3;` |
|       - | 11447 | `				}` |
|      26 | 11448 | `			}` |
|      26 | 11449 | `		}` |
|  363500 | 11450 | `	}` |
| 1347036 | 11451 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|   11755 | 11452 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|       - | 11453 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|       - | 11454 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|      30 | 11455 | `		iVmOp = 0;` |
|      13 | 11456 | `	}` |
| 1347041 | 11457 | `	if( iVmOp > 0 ){` |
| 1346785 | 11458 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   14845 | 11459 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 11460 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|   10861 | 11461 | `				iP1 = 1;` |
|    5433 | 11462 | `			}` |
| 1339365 | 11463 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 11464 | `			/* Namespace-qualify the class name for NEW */ {` |
|   23261 | 11465 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   23261 | 11466 | `				VmInstr *pCallInstr = 0;` |
|   23261 | 11467 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   23069 | 11468 | `					pCallInstr = pPeek;` |
|   23069 | 11469 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|   11532 | 11470 | `				}` |
|   23261 | 11471 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   23259 | 11472 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 11473 | `					sxu32 nLitForClass;` |
|       - | 11474 | `					/* If the CALL handler already qualified the name using` |
|       - | 11475 | `					 * function imports, recover the original unqualified` |
|       - | 11476 | `					 * literal so we can re-qualify with class imports. */` |
|   23259 | 11477 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      37 | 11478 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      21 | 11479 | `					}else{` |
|   23227 | 11480 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 11481 | `					}` |
|   23259 | 11482 | `					pPeek->iP1 = 0;` |
|   23259 | 11483 | `					if( !bAbsolute ){` |
|   19629 | 11484 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    9817 | 11485 | `					}else{` |
|    3635 | 11486 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|       - | 11487 | `					}` |
|   11627 | 11488 | `				}` |
|       - | 11489 | `			}` |
|   23261 | 11490 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   23261 | 11491 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 11492 | `				VmInstr *pPrev;` |
|   23069 | 11493 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   23069 | 11494 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 11495 | `					/* Pop the call instruction, preserve named-arg map */` |
|   23069 | 11496 | `					iP1 = pInstr->iP1;` |
|   23069 | 11497 | `					if( pInstr->p3 ){` |
|      43 | 11498 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|      19 | 11499 | `					}` |
|   23069 | 11500 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|   11532 | 11501 | `				}` |
|   11537 | 11502 | `			}` |
| 1320317 | 11503 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 11504 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 11505 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     201 | 11506 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     201 | 11507 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     201 | 11508 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     201 | 11509 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     201 | 11510 | `				int isSpecialIs = 0;` |
|     201 | 11511 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     197 | 11512 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     197 | 11513 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     197 | 11514 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     192 | 11515 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      97 | 11516 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      12 | 11517 | `						isSpecialIs = 1;` |
|       5 | 11518 | `					}` |
|      97 | 11519 | `				}` |
|     203 | 11520 | `				pInstr->iP1 = 0;` |
|     203 | 11521 | `				if( !isSpecialIs && !bAbsolute ){` |
|     181 | 11522 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      88 | 11523 | `				}` |
|     102 | 11524 | `			}` |
| 1308594 | 11525 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 11526 | `			/* Prevent constant expansion for member/property names.` |
|       - | 11527 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 11528 | `			 * should not trigger constant lookup. */` |
|  180015 | 11529 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  180015 | 11530 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  179967 | 11531 | `				pInstr->iP1 = 0;` |
|   89981 | 11532 | `			}` |
|  180015 | 11533 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 11534 | `				/* Static member access,remember that */` |
|     295 | 11535 | `				iP1 = 1;` |
|     295 | 11536 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     295 | 11537 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      42 | 11538 | `					p3 = pInstr->p3;` |
|      42 | 11539 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      19 | 11540 | `				}` |
|     145 | 11541 | `			}` |
|       - | 11542 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|       - | 11543 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|       - | 11544 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|       - | 11545 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|  180015 | 11546 | `			if( iP2 == PH7_MEMBER_READ ){` |
|  180015 | 11547 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|      30 | 11548 | `					iP2 = PH7_MEMBER_UNSET;` |
|  180001 | 11549 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|      77 | 11550 | `					iP2 = PH7_MEMBER_ISSET;` |
|  179951 | 11551 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|      13 | 11552 | `					iP2 = PH7_MEMBER_EMPTY;` |
|  179909 | 11553 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|       - | 11554 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|   80051 | 11555 | `					iP2 = PH7_MEMBER_WRITE;` |
|   40023 | 11556 | `				}` |
|   90005 | 11557 | `			}` |
|   90005 | 11558 | `		}` |
|       - | 11559 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|       - | 11560 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|       - | 11561 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|       - | 11562 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|       - | 11563 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
| 1346783 | 11564 | `		if( bFcc ){` |
|      65 | 11565 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|      65 | 11566 | `			iP2 = 0;` |
|      65 | 11567 | `			p3 = 0;` |
|      65 | 11568 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      65 | 11569 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 11570 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|       - | 11571 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|       - | 11572 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|       - | 11573 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|      31 | 11574 | `				void *pMemberName = pInstr->p3;` |
|      31 | 11575 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|      31 | 11576 | `				if( pMemberName ){` |
|       3 | 11577 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|       1 | 11578 | `				}` |
|      31 | 11579 | `				iP1 = 2;` |
|      16 | 11580 | `			}else{` |
|      35 | 11581 | `				iP1 = 1;` |
|       - | 11582 | `			}` |
|      32 | 11583 | `		}` |
|       - | 11584 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|       - | 11585 | `		 * This is the primary emit path for user-visible calls. */` |
| 1346783 | 11586 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  478025 | 11587 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  239010 | 11588 | `		}` |
|       - | 11589 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 1346783 | 11590 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  673389 | 11591 | `	}` |
| 1347039 | 11592 | `	if( nJmpIdx > 0 ){` |
|       - | 11593 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   15257 | 11594 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   15257 | 11595 | `		if( pInstr ){` |
|   15257 | 11596 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    7626 | 11597 | `		}` |
|    7626 | 11598 | `	}` |
| 1347039 | 11599 | `	return rc;` |
| 1796161 | 11600 | `}` |
|       - | 11601 | `/*` |
|       - | 11602 | ` * Compile a PHP expression.` |
|       - | 11603 | ` * According to the PHP language reference manual:` |
|       - | 11604 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 11605 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 11606 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 11607 | ` *  is "anything that has a value".` |
|       - | 11608 | ` * If something goes wrong while compiling the expression,this` |
|       - | 11609 | ` * function takes care of generating the appropriate error` |
|       - | 11610 | ` * message.` |
|       - | 11611 | ` */` |
|  967518 | 11612 | `static sxi32 PH7_CompileExpr(` |
|       - | 11613 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 11614 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 11615 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 11616 | `	)` |
|       5 | 11617 | `{` |
|       - | 11618 | `	ph7_expr_node *pRoot;` |
|       - | 11619 | `	SySet sExprNode;` |
|       - | 11620 | `	SyToken *pEnd;` |
|       - | 11621 | `	sxi32 nExpr;` |
|       - | 11622 | `	sxi32 iNest;` |
|       - | 11623 | `	sxi32 rc;` |
|       - | 11624 | `	sxu32 nNullsafeBase;` |
|       - | 11625 | `	/* Initialize worker variables */` |
|  967523 | 11626 | `	nExpr = 0;` |
|  967523 | 11627 | `	pRoot = 0;` |
|       - | 11628 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|       - | 11629 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  967523 | 11630 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  967523 | 11631 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  967523 | 11632 | `	SySetAlloc(&sExprNode,0x10);` |
|  967523 | 11633 | `	rc = SXRET_OK;` |
|       - | 11634 | `	/* Delimit the expression */` |
|  967523 | 11635 | `	pEnd = pGen->pIn;` |
|  967523 | 11636 | `	iNest = 0;` |
| 6527909 | 11637 | `	while( pEnd < pGen->pEnd ){` |
| 6194661 | 11638 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 11639 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     517 | 11640 | `			iNest++;` |
| 6194405 | 11641 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     525 | 11642 | `			iNest--;` |
| 6193889 | 11643 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  634647 | 11644 | `			if( iNest <= 0 ){` |
|  634275 | 11645 | `				break;` |
|       - | 11646 | `			}` |
|     186 | 11647 | `		}` |
| 5560391 | 11648 | `		pEnd++;` |
|       5 | 11649 | `	}` |
|  967523 | 11650 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   22257 | 11651 | `		SyToken *pEnd2 = pGen->pIn;` |
|   22257 | 11652 | `		iNest = 0;` |
|       - | 11653 | `		/* Stop at the first comma */` |
|   44827 | 11654 | `		while( pEnd2 < pEnd ){` |
|   22581 | 11655 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      71 | 11656 | `				iNest++;` |
|   22548 | 11657 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      71 | 11658 | `				iNest--;` |
|   22482 | 11659 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      57 | 11660 | `				if( iNest <= 0 ){` |
|       7 | 11661 | `					break;` |
|       - | 11662 | `				}` |
|      23 | 11663 | `			}` |
|   22575 | 11664 | `			pEnd2++;` |
|       5 | 11665 | `		}` |
|   22257 | 11666 | `		if( pEnd2 <pEnd ){` |
|       7 | 11667 | `			pEnd = pEnd2;` |
|       3 | 11668 | `		}` |
|   11126 | 11669 | `	}` |
|  967523 | 11670 | `	if( pEnd > pGen->pIn ){` |
|  967513 | 11671 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 11672 | `		/* Swap delimiter */` |
|  967513 | 11673 | `		pGen->pEnd = pEnd;` |
|       - | 11674 | `		/* Try to get an expression tree */` |
|  967513 | 11675 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  967513 | 11676 | `		if( rc == SXRET_OK && pRoot ){` |
|  967331 | 11677 | `			rc = SXRET_OK;` |
|  967331 | 11678 | `			if( xTreeValidator ){` |
|       - | 11679 | `				/* Call the upper layer validator callback */` |
|   29669 | 11680 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   14832 | 11681 | `			}` |
|  967331 | 11682 | `			if( rc != SXERR_ABORT ){` |
|       - | 11683 | `				/* Generate code for the given tree */` |
|  967331 | 11684 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|       - | 11685 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|       - | 11686 | `				 * expression so they short-circuit to its end. */` |
|  967331 | 11687 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  483663 | 11688 | `			}` |
|  967331 | 11689 | `			nExpr = 1;` |
|  483663 | 11690 | `		}` |
|       - | 11691 | `		/* Release the whole tree */` |
|  967513 | 11692 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 11693 | `		/* Synchronize token stream */` |
|  967513 | 11694 | `		pGen->pEnd = pTmp;` |
|  967513 | 11695 | `		pGen->pIn  = pEnd;` |
|  967513 | 11696 | `		if( rc == SXERR_ABORT ){` |
|      13 | 11697 | `			SySetRelease(&sExprNode);` |
|      13 | 11698 | `			return SXERR_ABORT;` |
|       - | 11699 | `		}` |
|  483749 | 11700 | `	}` |
|  967513 | 11701 | `	SySetRelease(&sExprNode);` |
|  967513 | 11702 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  483764 | 11703 | `}` |
|       - | 11704 | `/*` |
|       - | 11705 | ` * Return a pointer to the node construct handler associated` |
|       - | 11706 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 11707 | ` */` |
|  253374 | 11708 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       5 | 11709 | `{` |
|  253379 | 11710 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 11711 | `		/* Numeric literal: Either real or integer */` |
|  127583 | 11712 | `		return PH7_CompileNumLiteral;` |
|  125801 | 11713 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 11714 | `		/* Double quoted string */` |
|   23867 | 11715 | `		return PH7_CompileString;` |
|  101939 | 11716 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 11717 | `		/* Single quoted string */` |
|  101823 | 11718 | `		return PH7_CompileSimpleString;` |
|     121 | 11719 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 11720 | `		/* Heredoc */` |
|      68 | 11721 | `		return PH7_CompileHereDoc;` |
|      57 | 11722 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 11723 | `		/* Nowdoc */` |
|      50 | 11724 | `		return PH7_CompileNowDoc;` |
|       9 | 11725 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 11726 | `		/* Backtick quoted string */` |
|       6 | 11727 | `		return PH7_CompileBacktic;` |
|       - | 11728 | `	}` |
|       3 | 11729 | `	return 0;` |
|  126692 | 11730 | `}` |
|       - | 11731 | `/*` |
|       - | 11732 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|       - | 11733 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|       - | 11734 | ` * in write context" parse error.` |
|       - | 11735 | ` */` |
|    6866 | 11736 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|       5 | 11737 | `{` |
|       - | 11738 | `	sxi32 rc;` |
|    6871 | 11739 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|    6869 | 11740 | `		return SXRET_OK;` |
|       - | 11741 | `	}` |
|       5 | 11742 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       2 | 11743 | `		pNode ? pNode->pStart->nLine : 1,` |
|       - | 11744 | `		"Can't use nullsafe operator in write context");` |
|       3 | 11745 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|    3438 | 11746 | `}` |
|       - | 11747 | `/*` |
|       - | 11748 | ` * Compile an unset() statement.` |
|       - | 11749 | ` * unset($var, $arr[$key], ...);` |
|       - | 11750 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 11751 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 11752 | ` * parent array before extracting the element to unset.` |
|       - | 11753 | ` */` |
|    2978 | 11754 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       5 | 11755 | `{` |
|    2983 | 11756 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2983 | 11757 | `	sxu32 nIdx = 0;` |
|       - | 11758 | `	SyString sName;` |
|       - | 11759 | `	sxi32 rc;` |
|       - | 11760 | `	/* Jump the 'unset' keyword */` |
|    2983 | 11761 | `	pGen->pIn++;` |
|       - | 11762 | `	/* Save delimiter */` |
|    2983 | 11763 | `	pTmp = pGen->pEnd;` |
|       - | 11764 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2983 | 11765 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2983 | 11766 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 11767 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 11768 | `		SyToken *pClose;` |
|    2983 | 11769 | `		pGen->pIn++;   /* Skip '(' */` |
|    2983 | 11770 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2983 | 11771 | `		pEnd = pClose; /* Stop at ')' */` |
|    1489 | 11772 | `	}` |
|    2983 | 11773 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 11774 | `	/* Resolve the 'unset' builtin name once */` |
|    2983 | 11775 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     365 | 11776 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     365 | 11777 | `		if( pObj == 0 ){` |
|     ! 0 | 11778 | `			return SXERR_ABORT;` |
|       - | 11779 | `		}` |
|     365 | 11780 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     365 | 11781 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     180 | 11782 | `	}` |
|       - | 11783 | `	/* Compile each comma-separated argument */` |
|    9851 | 11784 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6873 | 11785 | `		if( pGen->pIn < pNext ){` |
|    6873 | 11786 | `			pGen->pEnd = pNext;` |
|    6873 | 11787 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 11788 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|       - | 11789 | `				GenStateUnsetValidator);` |
|    6873 | 11790 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11791 | `				return SXERR_ABORT;` |
|       - | 11792 | `			}` |
|    6873 | 11793 | `			if( rc != SXERR_EMPTY ){` |
|       - | 11794 | `				/* Emit call for this single argument */` |
|    6871 | 11795 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6871 | 11796 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|    6871 | 11797 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3433 | 11798 | `			}` |
|    3434 | 11799 | `		}` |
|       - | 11800 | `		/* Jump trailing commas */` |
|   10765 | 11801 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3897 | 11802 | `			pNext++;` |
|       5 | 11803 | `		}` |
|    6873 | 11804 | `		pGen->pIn = pNext;` |
|       5 | 11805 | `	}` |
|       - | 11806 | `	/* Skip past the closing ')' if present */` |
|    2983 | 11807 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2983 | 11808 | `		pGen->pIn++;` |
|    1489 | 11809 | `	}` |
|       - | 11810 | `	/* Restore token stream */` |
|    2983 | 11811 | `	pGen->pEnd = pTmp;` |
|    2983 | 11812 | `	return SXRET_OK;` |
|    1494 | 11813 | `}` |
|       - | 11814 | `/*` |
|       - | 11815 | ` * PHP Language construct table.` |
|       - | 11816 | ` */` |
|       - | 11817 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 11818 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 11819 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 11820 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 11821 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 11822 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 11823 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 11824 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 11825 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 11826 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 11827 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 11828 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 11829 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 11830 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 11831 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 11832 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 11833 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 11834 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 11835 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 11836 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 11837 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 11838 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 11839 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 11840 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 11841 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 11842 | `};` |
|       - | 11843 | `/*` |
|       - | 11844 | ` * Return a pointer to the statement handler routine associated` |
|       - | 11845 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 11846 | ` */` |
|  648818 | 11847 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 11848 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 11849 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 11850 | `	)` |
|       5 | 11851 | `{` |
|  648823 | 11852 | `	sxu32 n = 0;` |
| 3364251 | 11853 | `	for(;;){` |
| 6728507 | 11854 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|  138903 | 11855 | `			break;` |
|       - | 11856 | `		}` |
| 6589609 | 11857 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  509925 | 11858 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 11859 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 11860 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 11861 | `					/* 'static' (class context),return null */` |
|     ! 0 | 11862 | `					return 0;` |
|       - | 11863 | `				}` |
|     ! 0 | 11864 | `			}` |
|  509920 | 11865 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       8 | 11866 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       9 | 11867 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - | 11868 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 | 11869 | `				return 0;` |
|       - | 11870 | `			}` |
|       - | 11871 | `			/* Return a pointer to the handler.` |
|       - | 11872 | `			*/` |
|  509925 | 11873 | `			return aLangConstruct[n].xConstruct;` |
|       - | 11874 | `		}` |
| 6079689 | 11875 | `		n++;` |
|       5 | 11876 | `	}` |
|  138903 | 11877 | `	if( pLookahed ){` |
|  138903 | 11878 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|   39821 | 11879 | `			return PH7_CompileClassInterface;` |
|   99087 | 11880 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   98739 | 11881 | `			return PH7_CompileClass;` |
|     353 | 11882 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      69 | 11883 | `			return PH7_CompileTrait;` |
|       - | 11884 | `		}` |
|       - | 11885 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|       - | 11886 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|       - | 11887 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|       - | 11888 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     142 | 11889 | `	}` |
|       - | 11890 | `	/* Not a language construct */` |
|     289 | 11891 | `	return 0;` |
|  324414 | 11892 | `}` |
|       - | 11893 | `/*` |
|       - | 11894 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 11895 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 11896 | ` */` |
|     284 | 11897 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       5 | 11898 | `{` |
|       - | 11899 | `	int rc;` |
|     289 | 11900 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     289 | 11901 | `	if( rc == FALSE ){` |
|     174 | 11902 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     173 | 11903 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 11904 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 11905 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 11906 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 11907 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 11908 | `			*/` |
|       - | 11909 | `			){` |
|     171 | 11910 | `				rc = TRUE;` |
|      83 | 11911 | `		}` |
|      87 | 11912 | `	}` |
|     289 | 11913 | `	return rc;` |
|       5 | 11914 | `}` |
|       - | 11915 | `/*` |
|       - | 11916 | ` * Compile a PHP chunk.` |
|       - | 11917 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 11918 | ` * takes care of generating the appropriate error message.` |
|       - | 11919 | ` */` |
|  776096 | 11920 | `static sxi32 GenStateCompileChunk(` |
|       - | 11921 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 11922 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 11923 | `	)` |
|       5 | 11924 | `{` |
|       - | 11925 | `	ProcLangConstruct xCons;` |
|       - | 11926 | `	sxi32 rc;` |
|  776101 | 11927 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  606864 | 11928 | `	for(;;){` |
|  994917 | 11929 | `		int bStmtIsDeclare = 0;` |
|  994917 | 11930 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 11931 | `			/* No more input to process */` |
|   14377 | 11932 | `			break;` |
|       - | 11933 | `		}` |
|       - | 11934 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|       - | 11935 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  980545 | 11936 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  652469 | 11937 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  652469 | 11938 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|      45 | 11939 | `				bStmtIsDeclare = 1;` |
|      20 | 11940 | `			}` |
|  326232 | 11941 | `		}` |
|  980545 | 11942 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|       - | 11943 | `			/* Any non-declare top-level statement locks the strict_types` |
|       - | 11944 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|  218791 | 11945 | `			pGen->bStrictTypesLocked = 1;` |
|  109393 | 11946 | `		}` |
|  980545 | 11947 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 11948 | `			/* Compile block */` |
|      23 | 11949 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      23 | 11950 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11951 | `				break;` |
|       - | 11952 | `			}` |
|      14 | 11953 | `		}else{` |
|  980527 | 11954 | `			xCons = 0;` |
|  980527 | 11955 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|       - | 11956 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|       - | 11957 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|       - | 11958 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|    3677 | 11959 | `				xCons = PH7_CompileClassModifiers;` |
|  978691 | 11960 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  648823 | 11961 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 11962 | `				/* Try to extract a language construct handler */` |
|  648823 | 11963 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  648823 | 11964 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 11965 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 11966 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 11967 | `						&pGen->pIn->sData);` |
|       9 | 11968 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 11969 | `						break;` |
|       - | 11970 | `					}` |
|       - | 11971 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 11972 | `					 * this erroneous statement.` |
|       - | 11973 | `					 */` |
|       9 | 11974 | `					xCons = PH7_ErrorRecover;` |
|       4 | 11975 | `				}` |
|  652446 | 11976 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   53777 | 11977 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 11978 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     117 | 11979 | `				xCons = PH7_CompileLabel;` |
|      56 | 11980 | `			}` |
|  980527 | 11981 | `			if( xCons == 0 ){` |
|       - | 11982 | `				/* Assume an expression an try to compile it */` |
|  328201 | 11983 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  328201 | 11984 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 11985 | `					/* Pop l-value */` |
|  328051 | 11986 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  164023 | 11987 | `				}` |
|  164103 | 11988 | `			}else{` |
|       - | 11989 | `				/* Go compile the sucker */` |
|  652331 | 11990 | `				rc = xCons(&(*pGen));` |
|       - | 11991 | `			}` |
|  980527 | 11992 | `			if( rc == SXERR_ABORT ){` |
|       - | 11993 | `				/* Request to abort compilation */` |
|      13 | 11994 | `				break;` |
|       - | 11995 | `			}` |
|       - | 11996 | `		}` |
|       - | 11997 | `		/* Ignore trailing semi-colons ';' */` |
| 1585371 | 11998 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  604841 | 11999 | `			pGen->pIn++;` |
|       5 | 12000 | `		}` |
|  980535 | 12001 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 12002 | `			/* Compile a single statement and return */` |
|  761719 | 12003 | `			break;` |
|       - | 12004 | `		}` |
|       - | 12005 | `		/* LOOP ONE */` |
|       - | 12006 | `		/* LOOP TWO */` |
|       - | 12007 | `		/* LOOP THREE */` |
|       - | 12008 | `		/* LOOP FOUR */` |
|       5 | 12009 | `	}` |
|       - | 12010 | `	/* Return compilation status */` |
|  776101 | 12011 | `	return rc;` |
|       5 | 12012 | `}` |
|       - | 12013 | `/*` |
|       - | 12014 | ` * Compile a Raw PHP chunk.` |
|       - | 12015 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 12016 | ` * takes care of generating the appropriate error message.` |
|       - | 12017 | ` */` |
|   14384 | 12018 | `static sxi32 PH7_CompilePHP(` |
|       - | 12019 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 12020 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 12021 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 12022 | `	)` |
|       5 | 12023 | `{` |
|   14389 | 12024 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 12025 | `	sxi32 rc;` |
|       - | 12026 | `	/* Reset the token set */` |
|   14389 | 12027 | `	SySetReset(&(*pTokenSet));` |
|       - | 12028 | `	/* Mark as the default token set */` |
|   14389 | 12029 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 12030 | `	/* Advance the stream cursor */` |
|   14389 | 12031 | `	pGen->pRawIn++;` |
|       - | 12032 | `	/* Tokenize the PHP chunk first */` |
|   14389 | 12033 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 12034 | `	/* Point to the head and tail of the token stream. */` |
|   14389 | 12035 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   14389 | 12036 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   14389 | 12037 | `	if( is_expr ){` |
|     ! 0 | 12038 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 12039 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 12040 | `			/* A simple expression,compile it */` |
|     ! 0 | 12041 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 12042 | `		}` |
|       - | 12043 | `		/* Emit the DONE instruction */` |
|     ! 0 | 12044 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 12045 | `		return SXRET_OK;` |
|       - | 12046 | `	}` |
|   14389 | 12047 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 12048 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 12049 | `		/*` |
|       - | 12050 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 12051 | `		 * According to the PHP reference manual:` |
|       - | 12052 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 12053 | `		 *  immediately follow` |
|       - | 12054 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 12055 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 12056 | `		 * Symisc extension:` |
|       - | 12057 | `		 *   This short syntax works with all PHP opening` |
|       - | 12058 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 12059 | `		 *   only short tag.` |
|       - | 12060 | `		 */` |
|       - | 12061 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 12062 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 12063 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 12064 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 12065 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 12066 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 12067 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 12068 | `		}` |
|       3 | 12069 | `		return SXRET_OK;` |
|       - | 12070 | `	}` |
|       - | 12071 | `	/* Compile the PHP chunk */` |
|   14387 | 12072 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 12073 | `	/* Fix exceptions jumps */` |
|   14387 | 12074 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 12075 | `	/* Fix gotos now, the jump destination is resolved */` |
|   14387 | 12076 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 12077 | `		rc = SXERR_ABORT;` |
|       1 | 12078 | `	}` |
|       - | 12079 | `	/* Reset container */` |
|   14387 | 12080 | `	SySetReset(&pGen->aGoto);` |
|   14387 | 12081 | `	SySetReset(&pGen->aLabel);` |
|   14387 | 12082 | `	SySetReset(&pGen->aNullsafeJmp);` |
|       - | 12083 | `	/* Compilation result */` |
|   14387 | 12084 | `	return rc;` |
|    7197 | 12085 | `}` |
|       - | 12086 | `/*` |
|       - | 12087 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 12088 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 12089 | ` * This is the only compile interface exported from this file.` |
|       - | 12090 | ` */` |
|   17384 | 12091 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 12092 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 12093 | `	SyString *pScript,  /* Script to compile */` |
|       - | 12094 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 12095 | `	)` |
|       5 | 12096 | `{` |
|       - | 12097 | `	SySet aPhpToken,aRawToken;` |
|       - | 12098 | `	ph7_gen_state *pCodeGen;` |
|       - | 12099 | `	ph7_value *pRawObj;` |
|       - | 12100 | `	sxu32 nObjIdx;` |
|       - | 12101 | `	sxi32 nRawObj;` |
|       - | 12102 | `	int is_expr;` |
|       - | 12103 | `	sxi8 bSavedStrict;` |
|       - | 12104 | `	sxi8 bSavedStrictLocked;` |
|       - | 12105 | `	sxi32 rc;` |
|   17389 | 12106 | `	if( pScript->nByte < 1 ){` |
|       - | 12107 | `		/* Nothing to compile */` |
|     ! 0 | 12108 | `		return PH7_OK;` |
|       - | 12109 | `	}` |
|       - | 12110 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|       - | 12111 | `	 * file's flags so include/require restore them on return. */` |
|   17389 | 12112 | `	pCodeGen = &pVm->sCodeGen;` |
|   17389 | 12113 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|   17389 | 12114 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|   17389 | 12115 | `	pCodeGen->bStrictTypes = 0;` |
|   17389 | 12116 | `	pCodeGen->bStrictTypesLocked = 0;` |
|       - | 12117 | `	/* Initialize the tokens containers */` |
|   17389 | 12118 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   17389 | 12119 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   17389 | 12120 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   17389 | 12121 | `	is_expr = 0;` |
|   17389 | 12122 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 12123 | `		SyToken sTmp;` |
|       - | 12124 | `		/* PHP only: -*/` |
|    3685 | 12125 | `		sTmp.nLine = 1;` |
|    3685 | 12126 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    3685 | 12127 | `		sTmp.pUserData = 0;` |
|    3685 | 12128 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    3685 | 12129 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    3685 | 12130 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 12131 | `			/* A simple PHP expression */` |
|     ! 0 | 12132 | `			is_expr = 1;` |
|     ! 0 | 12133 | `		}` |
|    1845 | 12134 | `	}else{` |
|       - | 12135 | `		/* Tokenize raw text */` |
|   13709 | 12136 | `		SySetAlloc(&aRawToken,32);` |
|   13709 | 12137 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 12138 | `	}` |
|       - | 12139 | `	/* Process high-level tokens */` |
|   17389 | 12140 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   17389 | 12141 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   17389 | 12142 | `	rc = PH7_OK;` |
|   17389 | 12143 | `	if( is_expr ){` |
|       - | 12144 | `		/* Compile the expression */` |
|     ! 0 | 12145 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 12146 | `		goto cleanup;` |
|       - | 12147 | `	}` |
|   17389 | 12148 | `	nObjIdx = 0;` |
|       - | 12149 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 12150 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 12151 | `	 * preventing namespace bleeding across include()d files. */` |
|   17389 | 12152 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 12153 | `	/* Start the compilation process */` |
|   15550 | 12154 | `	for(;;){` |
|   45477 | 12155 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   17377 | 12156 | `			break; /* No more tokens to process */` |
|       - | 12157 | `		}` |
|   28105 | 12158 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 12159 | `			/* Compile the PHP chunk */` |
|   14389 | 12160 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   14389 | 12161 | `			if( rc == SXERR_ABORT ){` |
|      15 | 12162 | `				break;` |
|       - | 12163 | `			}` |
|   14377 | 12164 | `			continue;` |
|       - | 12165 | `		}` |
|       - | 12166 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   13721 | 12167 | `		nRawObj = 0;` |
|   27479 | 12168 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 12169 | `			/* Consume the raw chunk without any processing */` |
|   13763 | 12170 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   13763 | 12171 | `			if( pRawObj == 0 ){` |
|     ! 0 | 12172 | `				rc = SXERR_MEM;` |
|     ! 0 | 12173 | `				break;` |
|       - | 12174 | `			}` |
|       - | 12175 | `			/* Mark as constant and emit the load constant instruction */` |
|   13763 | 12176 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   13763 | 12177 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   13763 | 12178 | `			++nRawObj;` |
|   13763 | 12179 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       5 | 12180 | `		}` |
|   13721 | 12181 | `		if( nRawObj > 0 ){` |
|       - | 12182 | `			/* Emit the consume instruction */` |
|   13721 | 12183 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    6858 | 12184 | `		}` |
|    8697 | 12185 | `	}` |
|    8692 | 12186 | `cleanup:` |
|   17389 | 12187 | `	SySetRelease(&aRawToken);` |
|   17389 | 12188 | `	SySetRelease(&aPhpToken);` |
|       - | 12189 | `	/* Restore outer file's strict_types scope */` |
|   17389 | 12190 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|   17389 | 12191 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|   17389 | 12192 | `	return rc;` |
|    8697 | 12193 | `}` |
|       - | 12194 | `/*` |
|       - | 12195 | ` * Utility routines.Initialize the code generator.` |
|       - | 12196 | ` */` |
|    3612 | 12197 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 12198 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 12199 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 12200 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 12201 | `	)` |
|       5 | 12202 | `{` |
|    3617 | 12203 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 12204 | `	/* Zero the structure */` |
|    3617 | 12205 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 12206 | `	/* Initial state */` |
|    3617 | 12207 | `	pGen->pVm  = &(*pVm);` |
|    3617 | 12208 | `	pGen->xErr = xErr;` |
|    3617 | 12209 | `	pGen->pErrData = pErrData;` |
|    3617 | 12210 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    3617 | 12211 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    3617 | 12212 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|    3617 | 12213 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    3617 | 12214 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 12215 | `	/* Error log buffer */` |
|    3617 | 12216 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 12217 | `	/* General purpose working buffer */` |
|    3617 | 12218 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 12219 | `	/* Namespace state */` |
|    3617 | 12220 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    3617 | 12221 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    3617 | 12222 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    3617 | 12223 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 12224 | `	/* Create the global scope */` |
|    3617 | 12225 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 12226 | `	/* Point to the global scope */` |
|    3617 | 12227 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    3617 | 12228 | `	return SXRET_OK;` |
|       5 | 12229 | `}` |
|       - | 12230 | `/*` |
|       - | 12231 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 12232 | ` */` |
|   20632 | 12233 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 12234 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 12235 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 12236 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 12237 | `	)` |
|       5 | 12238 | `{` |
|   20637 | 12239 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 12240 | `	GenBlock *pBlock,*pParent;` |
|       - | 12241 | `	/* Reset state */` |
|   20637 | 12242 | `	SySetReset(&pGen->aLabel);` |
|   20637 | 12243 | `	SySetReset(&pGen->aGoto);` |
|   20637 | 12244 | `	SySetReset(&pGen->aNullsafeJmp);` |
|   20637 | 12245 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   20637 | 12246 | `	SyBlobRelease(&pGen->sWorker);` |
|   20637 | 12247 | `	SyBlobRelease(&pGen->sNamespace);` |
|   20637 | 12248 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   20637 | 12249 | `	SyHashRelease(&pGen->hUseImports);` |
|   20637 | 12250 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   20637 | 12251 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   20637 | 12252 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   20637 | 12253 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   20637 | 12254 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 12255 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 12256 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 12257 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 12258 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 12259 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 12260 | `	 * number of unique names, which is acceptable. */` |
|       - | 12261 | `	/* Point to the global scope */` |
|   20637 | 12262 | `	pBlock = pGen->pCurrent;` |
|   20637 | 12263 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 12264 | `		pParent = pBlock->pParent;` |
|     ! 0 | 12265 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 12266 | `		pBlock = pParent;` |
|     ! 0 | 12267 | `	}` |
|   20637 | 12268 | `	pGen->xErr = xErr;` |
|   20637 | 12269 | `	pGen->pErrData = pErrData;` |
|   20637 | 12270 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   20637 | 12271 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   20637 | 12272 | `	pGen->pIn = pGen->pEnd = 0;` |
|   20637 | 12273 | `	pGen->nErr = 0;` |
|   20637 | 12274 | `	return SXRET_OK;` |
|       5 | 12275 | `}` |
|       - | 12276 | `/*` |
|       - | 12277 | ` * Generate a compile-time error message.` |
|       - | 12278 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 12279 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 12280 | ` * abort compilation immediately.` |
|       - | 12281 | ` */` |
|     632 | 12282 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       5 | 12283 | `{` |
|     637 | 12284 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     637 | 12285 | `	const char *zErr = "Error";` |
|       - | 12286 | `	SyString *pFile;` |
|       - | 12287 | `	va_list ap;` |
|       - | 12288 | `	sxi32 rc;` |
|       - | 12289 | `	/* Reset the working buffer */` |
|     637 | 12290 | `	SyBlobReset(pWorker);` |
|       - | 12291 | `	/* Peek the processed file path if available */` |
|     637 | 12292 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     637 | 12293 | `	if( nErrType == E_ERROR ){` |
|       - | 12294 | `		/* Increment the error counter */` |
|     525 | 12295 | `		pGen->nErr++;` |
|     525 | 12296 | `		if( pGen->nErr > 15 ){` |
|       - | 12297 | `			/* Error count limit reached */` |
|       5 | 12298 | `			if( pGen->xErr ){` |
|       5 | 12299 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 12300 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 12301 | `				if( pFile ){` |
|       5 | 12302 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 12303 | `				}` |
|       5 | 12304 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 12305 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 12306 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 12307 | `				}` |
|       2 | 12308 | `			}` |
|       - | 12309 | `			/* Abort immediately */` |
|       5 | 12310 | `			return SXERR_ABORT;` |
|       - | 12311 | `		}` |
|     258 | 12312 | `	}` |
|     633 | 12313 | `	if( pGen->xErr == 0 ){` |
|       - | 12314 | `		/* No available error consumer,return immediately */` |
|       3 | 12315 | `		return SXRET_OK;` |
|       - | 12316 | `	}` |
|     630 | 12317 | `	switch(nErrType){` |
|     518 | 12318 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      30 | 12319 | `	case E_WARNING: zErr = "Warning";     break;` |
|      82 | 12320 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|      11 | 12321 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 12322 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 12323 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 12324 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 12325 | `	default:` |
|     ! 0 | 12326 | `		break;` |
|       - | 12327 | `	}` |
|     630 | 12328 | `	rc = SXRET_OK;` |
|       - | 12329 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     630 | 12330 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     630 | 12331 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     630 | 12332 | `	va_start(ap,zFormat);` |
|     630 | 12333 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     630 | 12334 | `	va_end(ap);` |
|     630 | 12335 | `	if( pFile ){` |
|     630 | 12336 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     313 | 12337 | `	}` |
|       - | 12338 | `	/* Append a new line */` |
|     630 | 12339 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     630 | 12340 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 12341 | `		/* Consume the generated error message */` |
|     630 | 12342 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     313 | 12343 | `	}` |
|     630 | 12344 | `	return rc;` |
|     321 | 12345 | `}` |
|       - | 12346 |  |
