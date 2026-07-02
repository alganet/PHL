# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5997/7417 lines (80.85%)

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
|     276 |   120 | `		if( SyStringCmp(&aLabel[n].sName,pName,SyMemcmp) == 0 ){` |
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
|    3970 |   137 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       5 |   138 | `{` |
|    3975 |   139 | `	GenBlock *pBlock = pCurrent;` |
|   11326 |   140 | `	for(;;){` |
|   22657 |   141 | `		if( pBlock->iFlags & iBlockType ){` |
|    3867 |   142 | `			iCount--; /* Decrement nesting level */` |
|    3867 |   143 | `			if( iCount < 1 ){` |
|       - |   144 | `				/* Block meet with the desired criteria */` |
|    3841 |   145 | `				return pBlock;` |
|       - |   146 | `			}` |
|      13 |   147 | `		}` |
|       - |   148 | `		/* Point to the upper block */` |
|   18821 |   149 | `		pBlock = pBlock->pParent;` |
|   18821 |   150 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |   151 | `			/* Forbidden */` |
|      72 |   152 | `			break;` |
|       - |   153 | `		}` |
|       5 |   154 | `	}` |
|       - |   155 | `	/* No such block */` |
|     139 |   156 | `	return 0;` |
|    1990 |   157 | `}` |
|       - |   158 | `/*` |
|       - |   159 | ` * Initialize a freshly allocated block instance.` |
|       - |   160 | ` */` |
|  875486 |   161 | `static void GenStateInitBlock(` |
|       - |   162 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   163 | `	GenBlock *pBlock,    /* Target block */` |
|       - |   164 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   165 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |   166 | `	void *pUserData      /* Upper layer private data */` |
|       - |   167 | `	)` |
|       5 |   168 | `{` |
|       - |   169 | `	/* Initialize block fields */` |
|  875491 |   170 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  875491 |   171 | `	pBlock->pUserData   = pUserData;` |
|  875491 |   172 | `	pBlock->pGen        = pGen;` |
|  875491 |   173 | `	pBlock->iFlags      = iType;` |
|  875491 |   174 | `	pBlock->pParent     = 0;` |
|  875491 |   175 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  875491 |   176 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  875491 |   177 | `}` |
|       - |   178 | `/*` |
|       - |   179 | ` * Allocate a new block instance.` |
|       - |   180 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |   181 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |   182 | ` * processing on failure.` |
|       - |   183 | ` */` |
|  871796 |   184 | `static sxi32 GenStateEnterBlock(` |
|       - |   185 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |   186 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   187 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |   188 | `	void *pUserData,      /* Upper layer private data */` |
|       - |   189 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |   190 | `	)` |
|       5 |   191 | `{` |
|       - |   192 | `	GenBlock *pBlock;` |
|       - |   193 | `	/* Allocate a new block instance */` |
|  871801 |   194 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  871801 |   195 | `	if( pBlock == 0 ){` |
|       - |   196 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |   197 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |   198 | `		 */` |
|     ! 0 |   199 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |   200 | `		/* Abort processing immediately */` |
|     ! 0 |   201 | `		return SXERR_ABORT;` |
|       - |   202 | `	}` |
|       - |   203 | `	/* Zero the structure */` |
|  871801 |   204 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  871801 |   205 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |   206 | `	/* Link to the parent block */` |
|  871801 |   207 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |   208 | `	/* Mark as the current block */` |
|  871801 |   209 | `	pGen->pCurrent = pBlock;` |
|  871801 |   210 | `	if( ppBlock ){` |
|       - |   211 | `		/* Write a pointer to the new instance */` |
|  421635 |   212 | `		*ppBlock = pBlock;` |
|  210815 |   213 | `	}` |
|  871801 |   214 | `	return SXRET_OK;` |
|  435903 |   215 | `}` |
|       - |   216 | `/*` |
|       - |   217 | ` * Release block fields without freeing the whole instance.` |
|       - |   218 | ` */` |
|  871788 |   219 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       5 |   220 | `{` |
|  871793 |   221 | `	SySetRelease(&pBlock->aPostContFix);` |
|  871793 |   222 | `	SySetRelease(&pBlock->aJumpFix);` |
|  871793 |   223 | `}` |
|       - |   224 | `/*` |
|       - |   225 | ` * Release a block.` |
|       - |   226 | ` */` |
|  871788 |   227 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       5 |   228 | `{` |
|  871793 |   229 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  871793 |   230 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |   231 | `	/* Free the instance */` |
|  871793 |   232 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  871793 |   233 | `}` |
|       - |   234 | `/*` |
|       - |   235 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |   236 | ` */` |
|  871788 |   237 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       5 |   238 | `{` |
|  871793 |   239 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  871793 |   240 | `	if( pBlock == 0 ){` |
|       - |   241 | `		/* No more block to pop */` |
|     ! 0 |   242 | `		return SXERR_EMPTY;` |
|       - |   243 | `	}` |
|       - |   244 | `	/* Point to the upper block */` |
|  871793 |   245 | `	pGen->pCurrent = pBlock->pParent;` |
|  871793 |   246 | `	if( ppBlock ){` |
|       - |   247 | `		/* Write a pointer to the popped block */` |
|     ! 0 |   248 | `		*ppBlock = pBlock;` |
|     ! 0 |   249 | `	}else{` |
|       - |   250 | `		/* Safely release the block */` |
|  871793 |   251 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |   252 | `	}` |
|  871793 |   253 | `	return SXRET_OK;` |
|  435899 |   254 | `}` |
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
|  249862 |   265 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       5 |   266 | `{` |
|       - |   267 | `	JumpFixup sJumpFix;` |
|       - |   268 | `	sxi32 rc;` |
|       - |   269 | `	/* Init the JumpFixup structure */` |
|  249867 |   270 | `	sJumpFix.nJumpType = nJumpType;` |
|  249867 |   271 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |   272 | `	/* Insert in the jump fixup table */` |
|  249867 |   273 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  249867 |   274 | `	return rc;` |
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
|  609038 |   287 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       5 |   288 | `{` |
|       - |   289 | `	JumpFixup *aFix;` |
|       - |   290 | `	VmInstr *pInstr;` |
|       - |   291 | `	sxu32 nFixed;` |
|       - |   292 | `	sxu32 n;` |
|       - |   293 | `	/* Point to the jump fixup table */` |
|  609043 |   294 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |   295 | `	/* Fix the desired jumps */` |
| 1097241 |   296 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  488203 |   297 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |   298 | `			/* Already fixed */` |
|  192941 |   299 | `			continue;` |
|       - |   300 | `		}` |
|  295267 |   301 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |   302 | `			/* Not of our interest */` |
|   45407 |   303 | `			continue;` |
|       - |   304 | `		}` |
|       - |   305 | `		/* Point to the instruction to fix */` |
|  249865 |   306 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  249865 |   307 | `		if( pInstr ){` |
|  249865 |   308 | `			pInstr->iP2 = nJumpDest;` |
|  249865 |   309 | `			nFixed++;` |
|       - |   310 | `			/* Mark as fixed */` |
|  249865 |   311 | `			aFix[n].nJumpType = -1;` |
|  124930 |   312 | `		}` |
|  124935 |   313 | `	}` |
|       - |   314 | `	/* Total number of fixed jumps */` |
|  609043 |   315 | `	return nFixed;` |
|       5 |   316 | `}` |
|       - |   317 | `/*` |
|       - |   318 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |   319 | ` * The goto statement can be used to jump to another section` |
|       - |   320 | ` * in the program.` |
|       - |   321 | ` * Refer to the routine responsible of compiling the goto` |
|       - |   322 | ` * statement for more information.` |
|       - |   323 | ` */` |
|  249354 |   324 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       5 |   325 | `{` |
|       - |   326 | `	JumpFixup *pJump,*aJumps;` |
|       - |   327 | `	Label *pLabel,*aLabel;` |
|       - |   328 | `	VmInstr *pInstr;` |
|       - |   329 | `	sxi32 rc;` |
|       - |   330 | `	sxu32 n;` |
|       - |   331 | `	/* Point to the goto table */` |
|  249359 |   332 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |   333 | `	/* Fix */` |
|  249505 |   334 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|      11 |   348 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' is unreachable",&pJump->sLabel);` |
|      11 |   349 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |   350 | `				return SXERR_ABORT;` |
|       - |   351 | `			}` |
|       4 |   352 | `		}` |
|       - |   353 | `		/* Fix the jump now the destination is resolved */` |
|      96 |   354 | `		pInstr = PH7_VmGetInstr(pGen->pVm,pJump->nInstrIdx);` |
|      96 |   355 | `		if( pInstr ){` |
|      96 |   356 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|      46 |   357 | `		}` |
|      50 |   358 | `	}` |
|  249357 |   359 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  249489 |   360 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     137 |   361 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |   362 | `			/* Emit a warning */` |
|      40 |   363 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |   364 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |   365 | `		}` |
|      71 |   366 | `	}` |
|  249357 |   367 | `	return SXRET_OK;` |
|  124682 |   368 | `}` |
|       - |   369 | `/*` |
|       - |   370 | ` * Check if a given token value is installed in the literal table.` |
|       - |   371 | ` */` |
|  793420 |   372 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       5 |   373 | `{` |
|       - |   374 | `	SyHashEntry *pEntry;` |
|  793425 |   375 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  793425 |   376 | `	if( pEntry == 0 ){` |
|  357391 |   377 | `		return SXERR_NOTFOUND;` |
|       - |   378 | `	}` |
|  436039 |   379 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  436039 |   380 | `	return SXRET_OK;` |
|  396715 |   381 | `}` |
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
|  357386 |   392 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       5 |   393 | `{` |
|  357391 |   394 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  357391 |   395 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  178693 |   396 | `	}` |
|  357391 |   397 | `	return SXRET_OK;` |
|       5 |   398 | `}` |
|       - |   399 | `/*` |
|       - |   400 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |   401 | ` * in the constant table.` |
|       - |   402 | ` */` |
|  129340 |   403 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       5 |   404 | `{` |
|       - |   405 | `	ph7_value *pObj;` |
|  129345 |   406 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |   407 | `	/* Reserve a new constant */` |
|  129345 |   408 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  129345 |   409 | `	if( pObj == 0 ){` |
|     ! 0 |   410 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   411 | `		return 0;` |
|       - |   412 | `	}` |
|  129345 |   413 | `	*pIdx = nIdx;` |
|       - |   414 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |   415 | `	 * the constant string iterals table [optimization purposes].` |
|       - |   416 | `	 */` |
|  129345 |   417 | `	return pObj;` |
|   64675 |   418 | `}` |
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
|  495372 |   433 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|       5 |   434 | `{` |
|       - |   435 | `	VmCallArgMap *pMap;` |
|  495377 |   436 | `	if( !pGen->bStrictTypes ) return p3;` |
|      33 |   437 | `	if( p3 == 0 ){` |
|      31 |   438 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|      31 |   439 | `		if( pMap == 0 ) return 0;` |
|      31 |   440 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|      31 |   441 | `		p3 = (void *)pMap;` |
|      14 |   442 | `	}` |
|      33 |   443 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|      33 |   444 | `	return p3;` |
|  247691 |   445 | `}` |
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
|  130098 |   504 | `static int GenStateFindBadNumericSeparator(` |
|       - |   505 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       5 |   506 | `{` |
|  130103 |   507 | `	const char *z = pRaw->zString;` |
|  130103 |   508 | `	sxu32 n = pRaw->nByte;` |
|  130103 |   509 | `	int base = 10;` |
|       - |   510 | `	sxu32 i, start;` |
|  130103 |   511 | `	if( n < 2 ) return 0;` |
|   10797 |   512 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |   513 | `		base = 16;` |
|   10762 |   514 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |   515 | `		base = 2;` |
|     139 |   516 | `	}` |
|   39009 |   517 | `	for( i = 0; i < n; ++i ){` |
|   28231 |   518 | `		if( z[i] != '_' ) continue;` |
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
|   10783 |   535 | `	return 0;` |
|   65054 |   536 | `}` |
|       - |   537 | `/*` |
|       - |   538 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   539 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   540 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   541 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   542 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   543 | ` * so callers can bail from the current construct).` |
|       - |   544 | ` */` |
|  130098 |   545 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       5 |   546 | `{` |
|  130103 |   547 | `	const char *zBad = 0;` |
|  130103 |   548 | `	sxu32 nBad = 0;` |
|       - |   549 | `	SyString sBad;` |
|       - |   550 | `	sxi32 rc;` |
|  130103 |   551 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  130089 |   552 | `		return SXRET_OK;` |
|       - |   553 | `	}` |
|      18 |   554 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      18 |   555 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |   556 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      18 |   557 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |   558 | `		return SXERR_ABORT;` |
|       - |   559 | `	}` |
|      18 |   560 | `	return SXERR_SYNTAX;` |
|   65054 |   561 | `}` |
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
|  130084 |   578 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |   579 | `	SyMemBackend *pAlloc,` |
|       - |   580 | `	const SyString *pToken,` |
|       - |   581 | `	char *zScratch, sxu32 nScratch,` |
|       - |   582 | `	SyString *pOut, char **pzAlloc)` |
|       5 |   583 | `{` |
|       - |   584 | `	sxu32 i, j;` |
|  130089 |   585 | `	int hasUnderscore = 0;` |
|       - |   586 | `	char *zBuf;` |
|  130089 |   587 | `	*pzAlloc = 0;` |
|  275541 |   588 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  145709 |   589 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   72731 |   590 | `	}` |
|  130089 |   591 | `	if( !hasUnderscore ){` |
|  129837 |   592 | `		SyStringDupPtr(pOut, pToken);` |
|  129837 |   593 | `		return SXRET_OK;` |
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
|   65047 |   610 | `}` |
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
|  130070 |   627 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   628 | `{` |
|  130075 |   629 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  130075 |   630 | `	sxu32 nIdx = 0;` |
|       - |   631 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  130075 |   632 | `	char *zAlloc = 0;` |
|       - |   633 | `	SyString sNum;` |
|       - |   634 | `	sxi32 rc;` |
|   65035 |   635 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  130075 |   636 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  130075 |   637 | `	if( rc != SXRET_OK ){` |
|      14 |   638 | `		return rc;` |
|       - |   639 | `	}` |
|  195095 |   640 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   65030 |   641 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  130065 |   642 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   643 | `		return SXERR_ABORT;` |
|       - |   644 | `	}` |
|  130065 |   645 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |   646 | `		ph7_value *pObj;` |
|       - |   647 | `		sxi64 iValue;` |
|  129345 |   648 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|  129345 |   649 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  129345 |   650 | `		if( pObj == 0 ){` |
|     ! 0 |   651 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   652 | `			return SXERR_ABORT;` |
|       - |   653 | `		}` |
|  129345 |   654 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   64675 |   655 | `	}else{` |
|       - |   656 | `		/* Real number */` |
|       - |   657 | `		ph7_value *pObj;` |
|       - |   658 | `		/* Reserve a new constant */` |
|     725 |   659 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     725 |   660 | `		if( pObj == 0 ){` |
|     ! 0 |   661 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   662 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   663 | `			return SXERR_ABORT;` |
|       - |   664 | `		}` |
|     725 |   665 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|     725 |   666 | `		PH7_MemObjToReal(pObj);` |
|       - |   667 | `	}` |
|  130065 |   668 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |   669 | `	/* Emit the load constant instruction */` |
|  130065 |   670 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   671 | `	/* Node successfully compiled */` |
|  130065 |   672 | `	return SXRET_OK;` |
|   65040 |   673 | `}` |
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
|  103646 |   685 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   686 | `{` |
|  103651 |   687 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |   688 | `	const char *zIn,*zCur,*zEnd;` |
|       - |   689 | `	ph7_value *pObj;` |
|       - |   690 | `	sxu32 nIdx;` |
|  103651 |   691 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |   692 | `	/* Delimit the string */` |
|  103651 |   693 | `	zIn  = pStr->zString;` |
|  103651 |   694 | `	zEnd = &zIn[pStr->nByte];` |
|  103651 |   695 | `	if( zIn >= zEnd ){` |
|       - |   696 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |   697 | `		 * rather than reserving a new object each time. */` |
|    7551 |   698 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    7551 |   699 | `		return SXRET_OK;` |
|       - |   700 | `	}` |
|   96105 |   701 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |   702 | `		/* Already processed,emit the load constant instruction` |
|       - |   703 | `		 * and return.` |
|       - |   704 | `		 */` |
|   36983 |   705 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   36983 |   706 | `		return SXRET_OK;` |
|       - |   707 | `	}` |
|       - |   708 | `	/* Reserve a new constant */` |
|   59127 |   709 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   59127 |   710 | `	if( pObj == 0 ){` |
|     ! 0 |   711 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   712 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   713 | `		return SXERR_ABORT;` |
|       - |   714 | `	}` |
|   59127 |   715 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |   716 | `	/* Compile the node */` |
|   59181 |   717 | `	for(;;){` |
|  118367 |   718 | `		if( zIn >= zEnd ){` |
|       - |   719 | `			/* End of input */` |
|   59127 |   720 | `			break;` |
|       - |   721 | `		}` |
|   59245 |   722 | `		zCur = zIn;` |
| 1012349 |   723 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  953109 |   724 | `			zIn++;` |
|       5 |   725 | `		}` |
|   59245 |   726 | `		if( zIn > zCur ){` |
|       - |   727 | `			/* Append raw contents*/` |
|   59221 |   728 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   29608 |   729 | `		}` |
|   59245 |   730 | `		zIn++;` |
|   59245 |   731 | `		if( zIn < zEnd ){` |
|     141 |   732 | `			if( zIn[0] == '\\' ){` |
|       - |   733 | `				/* A literal backslash */` |
|      28 |   734 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|     127 |   735 | `			}else if( zIn[0] == '\'' ){` |
|       - |   736 | `				/* A single quote */` |
|      11 |   737 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       6 |   738 | `			}else{` |
|       - |   739 | `				/* verbatim copy */` |
|     104 |   740 | `				zIn--;` |
|     104 |   741 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|     104 |   742 | `				zIn++;` |
|       - |   743 | `			}` |
|      69 |   744 | `		}` |
|       - |   745 | `		/* Advance the stream cursor */` |
|   59245 |   746 | `		zIn++;` |
|       5 |   747 | `	}` |
|       - |   748 | `	/* Emit the load constant instruction */` |
|   59127 |   749 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   59127 |   750 | `	if( pStr->nByte < 1024 ){` |
|       - |   751 | `		/* Install in the literal table */` |
|   59127 |   752 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   29561 |   753 | `	}` |
|       - |   754 | `	/* Node successfully compiled */` |
|   59127 |   755 | `	return SXRET_OK;` |
|   51828 |   756 | `}` |
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
|     114 |   775 | `static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)` |
|       5 |   776 | `{` |
|     119 |   777 | `	SyString *pIn = &pGen->pIn->sData;` |
|     119 |   778 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - |   779 | `	const char *zPrefix;` |
|       - |   780 | `	const char *z, *zEnd;` |
|       - |   781 | `	char *zBuf, *zDst;` |
|     119 |   782 | `	if( nIndent == 0 ){` |
|       - |   783 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|      72 |   784 | `		*pOut = *pIn;` |
|      72 |   785 | `		return SXRET_OK;` |
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
|      62 |   853 | `}` |
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
|      48 |   868 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |   869 | `{` |
|       - |   870 | `	SyString sStripped;` |
|       - |   871 | `	SyString *pStr;` |
|       - |   872 | `	ph7_value *pObj;` |
|       - |   873 | `	sxu32 nIdx;` |
|       - |   874 | `	sxi32 rc;` |
|      52 |   875 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      52 |   876 | `	if( rc != SXRET_OK ){` |
|       6 |   877 | `		return rc;` |
|       - |   878 | `	}` |
|      46 |   879 | `	pStr = &sStripped;` |
|      46 |   880 | `	nIdx = 0; /* Prevent compiler warning */` |
|      46 |   881 | `	if( pStr->nByte <= 0 ){` |
|       - |   882 | `		/* Empty string,load NULL */` |
|       7 |   883 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |   884 | `		return SXRET_OK;` |
|       - |   885 | `	}` |
|       - |   886 | `	/* Reserve a new constant */` |
|      40 |   887 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      40 |   888 | `	if( pObj == 0 ){` |
|     ! 0 |   889 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   890 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   891 | `		return SXERR_ABORT;` |
|       - |   892 | `	}` |
|       - |   893 | `	/* No processing is done here, simply a memcpy() operation */` |
|      40 |   894 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|       - |   895 | `	/* Emit the load constant instruction */` |
|      40 |   896 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   897 | `	/* Node successfully compiled */` |
|      40 |   898 | `	return SXRET_OK;` |
|      28 |   899 | `}` |
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
|    2284 |   922 | `static sxi32 GenStateProcessStringExpression(` |
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
|    2289 |   933 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |   934 | `	/* Preallocate some slots */` |
|    2289 |   935 | `	SySetAlloc(&sToken,0x08);` |
|       - |   936 | `	/* Tokenize the text */` |
|    2289 |   937 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |   938 | `	/* Swap delimiter */` |
|    2289 |   939 | `	pTmpIn  = pGen->pIn;` |
|    2289 |   940 | `	pTmpEnd = pGen->pEnd;` |
|    2289 |   941 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    2289 |   942 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |   943 | `	/* Compile the expression */` |
|    2289 |   944 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |   945 | `	/* Restore token stream */` |
|    2289 |   946 | `	pGen->pIn  = pTmpIn;` |
|    2289 |   947 | `	pGen->pEnd = pTmpEnd;` |
|       - |   948 | `	/* Release the token set */` |
|    2289 |   949 | `	SySetRelease(&sToken);` |
|       - |   950 | `	/* Compilation result */` |
|    2289 |   951 | `	return rc;` |
|       5 |   952 | `}` |
|       - |   953 | `/*` |
|       - |   954 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |   955 | ` */` |
|   25894 |   956 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       5 |   957 | `{` |
|       - |   958 | `	ph7_value *pConstObj;` |
|   25899 |   959 | `	sxu32 nIdx = 0;` |
|       - |   960 | `	/* Reserve a new constant */` |
|   25899 |   961 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   25899 |   962 | `	if( pConstObj == 0 ){` |
|     ! 0 |   963 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   964 | `		return 0;` |
|       - |   965 | `	}` |
|   25899 |   966 | `	(*pCount)++;` |
|   25899 |   967 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |   968 | `	/* Emit the load constant instruction */` |
|   25899 |   969 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   25899 |   970 | `	return pConstObj;` |
|   12952 |   971 | `}` |
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
|       - |  1000 | ` *  \e escape (ESC or 0x1B (27) in ASCII)` |
|       - |  1001 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|       - |  1002 | ` *  \\ backslash` |
|       - |  1003 | ` *  \$ dollar sign` |
|       - |  1004 | ` *  \" double-quote` |
|       - |  1005 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation,` |
|       - |  1006 | ` *      which silently overflows to fit in a byte (e.g. "\400" === "\000")` |
|       - |  1007 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|       - |  1008 | ` *  \u{[0-9A-Fa-f]+} 	the sequence of characters matching the regular expression is a Unicode codepoint,` |
|       - |  1009 | ` *      which will be output to the string as that codepoint's UTF-8 representation` |
|       - |  1010 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|       - |  1011 | ` * (The PH7-ism "\oNNN" octal form is gone: a literal "\o" now round-trips like php 8.)` |
|       - |  1012 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|       - |  1013 | ` * See string parsing for details.` |
|       - |  1014 | ` */` |
|       - |  1015 | `/*` |
|       - |  1016 | ` * Line number of an escape sequence inside the string body being compiled:` |
|       - |  1017 | ` * the token's line plus every newline before the escape (php reports the` |
|       - |  1018 | ` * escape's own line, not the string's opening line). A heredoc body starts` |
|       - |  1019 | ` * on the line after the '<<<' marker, hence the +1.` |
|       - |  1020 | ` */` |
|       6 |  1021 | `static sxu32 GenStateStringEscLine(ph7_gen_state *pGen,const char *zPos,int bHeredoc)` |
|       3 |  1022 | `{` |
|       9 |  1023 | `	const char *z = pGen->pIn->sData.zString;` |
|       9 |  1024 | `	sxu32 nLine = pGen->pIn->nLine + (bHeredoc ? 1 : 0);` |
|      15 |  1025 | `	for( ; z < zPos ; z++ ){` |
|       9 |  1026 | `		if( z[0] == '\n' ){` |
|     ! 0 |  1027 | `			nLine++;` |
|     ! 0 |  1028 | `		}` |
|       6 |  1029 | `	}` |
|       9 |  1030 | `	return nLine;` |
|       3 |  1031 | `}` |
|       - |  1032 | `/* bHeredoc: php strips the backslash from '\"' only when '"' is the active` |
|       - |  1033 | ` * quote character; a heredoc has none, so '\"' stays verbatim there. */` |
|   24396 |  1034 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen,int bHeredoc)` |
|       5 |  1035 | `{` |
|   24401 |  1036 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  1037 | `	const char *zIn,*zCur,*zEnd;` |
|   24401 |  1038 | `	ph7_value *pObj = 0;` |
|       - |  1039 | `	sxi32 iCons;` |
|       - |  1040 | `	sxi32 rc;` |
|       - |  1041 | `	/* Delimit the string */` |
|   24401 |  1042 | `	zIn  = pStr->zString;` |
|   24401 |  1043 | `	zEnd = &zIn[pStr->nByte];` |
|   24401 |  1044 | `	if( zIn >= zEnd ){` |
|       - |  1045 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  1046 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  1047 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  1048 | `		 */` |
|     313 |  1049 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     313 |  1050 | `		return SXRET_OK;` |
|       - |  1051 | `	}` |
|   24093 |  1052 | `	zCur = 0;` |
|       - |  1053 | `	/* Compile the node */` |
|   24093 |  1054 | `	iCons = 0;` |
|   13186 |  1055 | `	for(;;){` |
|   39443 |  1056 | `		zCur = zIn;` |
|  182887 |  1057 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  145733 |  1058 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      68 |  1059 | `				break;` |
|  145607 |  1060 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    2162 |  1061 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|    1082 |  1062 | `					break;` |
|       - |  1063 | `			}` |
|  143449 |  1064 | `			zIn++;` |
|       5 |  1065 | `		}` |
|   39443 |  1066 | `		if( zIn > zCur ){` |
|   18315 |  1067 | `			if( pObj == 0 ){` |
|   17803 |  1068 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   17803 |  1069 | `				if( pObj == 0 ){` |
|     ! 0 |  1070 | `					return SXERR_ABORT;` |
|       - |  1071 | `				}` |
|    8899 |  1072 | `			}` |
|   18315 |  1073 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    9155 |  1074 | `		}` |
|   39443 |  1075 | `		if( zIn >= zEnd ){` |
|   24091 |  1076 | `			break;` |
|       - |  1077 | `		}` |
|   15357 |  1078 | `		if( zIn[0] == '\\' ){` |
|   13073 |  1079 | `			const char *zPtr = 0;` |
|       - |  1080 | `			sxu32 n;` |
|   13073 |  1081 | `			zIn++;` |
|   13073 |  1082 | `			if( pObj == 0 ){` |
|    8101 |  1083 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    8101 |  1084 | `				if( pObj == 0 ){` |
|     ! 0 |  1085 | `					return SXERR_ABORT;` |
|       - |  1086 | `				}` |
|    4048 |  1087 | `			}` |
|   13073 |  1088 | `			if( zIn >= zEnd ){` |
|       - |  1089 | `				/* Lone backslash at the very end of the body: php keeps it */` |
|       3 |  1090 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|       3 |  1091 | `				break;` |
|       - |  1092 | `			}` |
|   13071 |  1093 | `			n = sizeof(char); /* size of conversion */` |
|   13071 |  1094 | `			switch( zIn[0] ){` |
|      10 |  1095 | `			case '$':` |
|       - |  1096 | `				/* Dollar sign */` |
|      22 |  1097 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|      22 |  1098 | `				break;` |
|      54 |  1099 | `			case '\\':` |
|       - |  1100 | `				/* A literal backslash */` |
|     112 |  1101 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|     112 |  1102 | `				break;` |
|       1 |  1103 | `			case 'e':` |
|       - |  1104 | `				/* Escape (ESC) ASCII code 27 */` |
|       3 |  1105 | `				PH7_MemObjStringAppend(pObj,"\x1b",sizeof(char));` |
|       3 |  1106 | `				break;` |
|       4 |  1107 | `			case 'f':` |
|       - |  1108 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|       9 |  1109 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|       9 |  1110 | `				break;` |
|    5989 |  1111 | `			case 'n':` |
|       - |  1112 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|   11983 |  1113 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|   11983 |  1114 | `				break;` |
|      20 |  1115 | `			case 'r':` |
|       - |  1116 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|      45 |  1117 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|      45 |  1118 | `				break;` |
|      25 |  1119 | `			case 't':` |
|       - |  1120 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      55 |  1121 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      55 |  1122 | `				break;` |
|       3 |  1123 | `			case 'v':` |
|       - |  1124 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|       7 |  1125 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|       7 |  1126 | `				break;` |
|     112 |  1127 | `			case '"':` |
|     229 |  1128 | `				if( bHeredoc ){` |
|       - |  1129 | `					/* No active quote char in a heredoc: php keeps \" verbatim */` |
|       5 |  1130 | `					PH7_MemObjStringAppend(pObj,"\\\"",sizeof(char)*2);` |
|       3 |  1131 | `				}else{` |
|       - |  1132 | `					/* Double quote */` |
|     225 |  1133 | `					PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|       - |  1134 | `				}` |
|     229 |  1135 | `				break;` |
|      24 |  1136 | `			case '0': case '1': case '2': case '3':` |
|       - |  1137 | `			case '4': case '5': case '6': case '7': {` |
|       - |  1138 | `				/* \[0-7]{1,3}: a character in octal notation. A value above \377` |
|       - |  1139 | `				 * warns and wraps to the low byte, matching php 8. */` |
|      50 |  1140 | `				int c = 0;` |
|       - |  1141 | `				char cOut;` |
|     144 |  1142 | `				for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|     122 |  1143 | `					if( zPtr >= zEnd \|\| zPtr[0] < '0' \|\| zPtr[0] > '7' ){` |
|      14 |  1144 | `						break;` |
|       - |  1145 | `					}` |
|      96 |  1146 | `					c = c * 8 + (zPtr[0] - '0');` |
|      49 |  1147 | `				}` |
|      50 |  1148 | `				if( c > 0xFF ){` |
|       - |  1149 | `					SyString sSeq;` |
|       3 |  1150 | `					SyStringInitFromBuf(&sSeq,zIn,(sxu32)(zPtr-zIn));` |
|       3 |  1151 | `					PH7_GenCompileError(&(*pGen),E_WARNING,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|       - |  1152 | `						"Octal escape sequence overflow \\%z is greater than \\377",&sSeq);` |
|       3 |  1153 | `					c &= 0xFF;` |
|       1 |  1154 | `				}` |
|      50 |  1155 | `				cOut = (char)c; /* value byte, independent of host endianness */` |
|      50 |  1156 | `				PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|      50 |  1157 | `				n = (sxu32)(zPtr-zIn);` |
|      50 |  1158 | `				break;` |
|       - |  1159 | `			}` |
|     270 |  1160 | `			case 'x':` |
|     809 |  1161 | `				if( &zIn[1] < zEnd && SyisHex((unsigned char)zIn[1]) ){` |
|       - |  1162 | `					/* \x[0-9A-Fa-f]{1,2}: a character in hexadecimal notation */` |
|     537 |  1163 | `					int c = SyHexToint(zIn[1]);` |
|       - |  1164 | `					char cOut;` |
|     537 |  1165 | `					n += sizeof(char);` |
|     537 |  1166 | `					if( &zIn[2] < zEnd && SyisHex((unsigned char)zIn[2]) ){` |
|     533 |  1167 | `						c = (c << 4) + SyHexToint(zIn[2]);` |
|     533 |  1168 | `						n += sizeof(char);` |
|     266 |  1169 | `					}` |
|     537 |  1170 | `					cOut = (char)c; /* value byte, independent of host endianness */` |
|     537 |  1171 | `					PH7_MemObjStringAppend(pObj,&cOut,sizeof(char));` |
|     269 |  1172 | `				}else{` |
|       - |  1173 | `					/* Not an escape: keep the backslash, as php does */` |
|       5 |  1174 | `					PH7_MemObjStringAppend(pObj,"\\x",sizeof(char)*2);` |
|       - |  1175 | `				}` |
|     541 |  1176 | `				break;` |
|       9 |  1177 | `			case 'u':` |
|      18 |  1178 | `				if( &zIn[1] < zEnd && zIn[1] == '{'` |
|      22 |  1179 | `				 && !(&zIn[2] < zEnd && zIn[2] == '$') ){` |
|       - |  1180 | `					/* \u{codepoint}: UTF-8 encoding of the given codepoint (php 7+).` |
|       - |  1181 | `					 * php encodes surrogates verbatim, so the only invalid value` |
|       - |  1182 | `					 * is > U+10FFFF; malformed/empty braces are a compile error.` |
|       - |  1183 | `					 * "\u{$..." is excluded above: php treats it as a literal \u` |
|       - |  1184 | `					 * followed by {$...} curly interpolation. */` |
|      15 |  1185 | `					sxu32 nCp = 0;` |
|      15 |  1186 | `					zPtr = &zIn[2];` |
|      59 |  1187 | `					while( zPtr < zEnd && SyisHex((unsigned char)zPtr[0]) ){` |
|      46 |  1188 | `						if( nCp <= 0x10FFFF ){` |
|       - |  1189 | `							/* stop accumulating once out of range: keeps a long` |
|       - |  1190 | `							 * digit run from wrapping sxu32 */` |
|      46 |  1191 | `							nCp = nCp * 16 + (sxu32)SyHexToint(zPtr[0]);` |
|      22 |  1192 | `						}` |
|      46 |  1193 | `						zPtr++;` |
|       2 |  1194 | `					}` |
|      15 |  1195 | `					if( zPtr == &zIn[2] \|\| zPtr >= zEnd \|\| zPtr[0] != '}' ){` |
|       - |  1196 | `						/* Error recorded (nErr>0 fails the whole compile); consume the` |
|       - |  1197 | `						 * malformed sequence so later errors are still reported. */` |
|       3 |  1198 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|       - |  1199 | `							"Invalid UTF-8 codepoint escape sequence");` |
|       3 |  1200 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  1201 | `							return SXERR_ABORT;` |
|       - |  1202 | `						}` |
|       3 |  1203 | `						n = (sxu32)(zPtr-zIn);` |
|       3 |  1204 | `						if( zPtr < zEnd && zPtr[0] == '}' ){` |
|       3 |  1205 | `							n += sizeof(char);` |
|       1 |  1206 | `						}` |
|       3 |  1207 | `						break;` |
|       - |  1208 | `					}` |
|      12 |  1209 | `					n = (sxu32)(&zPtr[1]-zIn); /* 'u{...}' incl. closing brace */` |
|      12 |  1210 | `					if( nCp > 0x10FFFF ){` |
|       3 |  1211 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,GenStateStringEscLine(&(*pGen),zIn,bHeredoc),` |
|       - |  1212 | `							"Invalid UTF-8 codepoint escape sequence: Codepoint too large");` |
|       3 |  1213 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  1214 | `							return SXERR_ABORT;` |
|       - |  1215 | `						}` |
|       3 |  1216 | `						break;` |
|       - |  1217 | `					}` |
|       - |  1218 | `					{` |
|       - |  1219 | `						char zUtf[4];` |
|       9 |  1220 | `						sxu8 *zOut = (sxu8 *)zUtf;` |
|       9 |  1221 | `						SX_WRITE_UTF8(zOut,nCp);` |
|       9 |  1222 | `						PH7_MemObjStringAppend(pObj,zUtf,(sxu32)(zOut-(sxu8 *)zUtf));` |
|       - |  1223 | `					}` |
|       5 |  1224 | `				}else{` |
|       - |  1225 | `					/* Not an escape: keep the backslash, as php does */` |
|       7 |  1226 | `					PH7_MemObjStringAppend(pObj,"\\u",sizeof(char)*2);` |
|       - |  1227 | `				}` |
|      15 |  1228 | `				break;` |
|      12 |  1229 | `			default:` |
|       - |  1230 | `				/* Unrecognized escape: keep the backslash, as php does.` |
|       - |  1231 | `				 * zIn[-1] is the backslash itself, so both bytes are contiguous` |
|       - |  1232 | `				 * in the source buffer — one batched append. */` |
|      25 |  1233 | `				PH7_MemObjStringAppend(pObj,&zIn[-1],sizeof(char)*2);` |
|      24 |  1234 | `				break;` |
|       - |  1235 | `			}` |
|       - |  1236 | `			/* Advance the stream cursor */` |
|   13071 |  1237 | `			zIn += n;` |
|   13071 |  1238 | `			continue;` |
|       - |  1239 | `		}` |
|    2289 |  1240 | `		if( zIn[0] == '{' ){` |
|       - |  1241 | `			/* Curly syntax */` |
|       - |  1242 | `			const char *zExpr;` |
|     133 |  1243 | `			sxi32 iNest = 1;` |
|     133 |  1244 | `			zIn++;` |
|     133 |  1245 | `			zExpr = zIn;` |
|       - |  1246 | `			/* Synchronize with the next closing curly braces */` |
|    1365 |  1247 | `			while( zIn < zEnd ){` |
|    1365 |  1248 | `				if( zIn[0] == '{' ){` |
|       - |  1249 | `					/* Increment nesting level */` |
|       9 |  1250 | `					iNest++;` |
|    1361 |  1251 | `				}else if(zIn[0] == '}' ){` |
|       - |  1252 | `					/* Decrement nesting level */` |
|     141 |  1253 | `					iNest--;` |
|     141 |  1254 | `					if( iNest <= 0 ){` |
|     133 |  1255 | `						break;` |
|       - |  1256 | `					}` |
|       4 |  1257 | `				}` |
|    1235 |  1258 | `				zIn++;` |
|       3 |  1259 | `			}` |
|       - |  1260 | `			/* Process the expression */` |
|     133 |  1261 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     133 |  1262 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1263 | `				return SXERR_ABORT;` |
|       - |  1264 | `			}` |
|     133 |  1265 | `			if( rc != SXERR_EMPTY ){` |
|     133 |  1266 | `				++iCons;` |
|      65 |  1267 | `			}` |
|     133 |  1268 | `			if( zIn < zEnd ){` |
|       - |  1269 | `				/* Jump the trailing curly */` |
|     133 |  1270 | `				zIn++;` |
|      65 |  1271 | `			}` |
|      68 |  1272 | `		}else{` |
|       - |  1273 | `			/* Simple syntax */` |
|    2159 |  1274 | `			const char *zExpr = zIn;` |
|       - |  1275 | `			/* Assemble variable name */` |
|    1087 |  1276 | `			for(;;){` |
|       - |  1277 | `				/* Jump leading dollars */` |
|    4333 |  1278 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    2159 |  1279 | `					zIn++;` |
|       5 |  1280 | `				}` |
|    1087 |  1281 | `				for(;;){` |
|   11932 |  1282 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    8671 |  1283 | `						zIn++;` |
|       5 |  1284 | `					}` |
|    2179 |  1285 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  1286 | `						/* UTF-8 stream */` |
|     ! 0 |  1287 | `						zIn++;` |
|     ! 0 |  1288 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1289 | `							zIn++;` |
|     ! 0 |  1290 | `						}` |
|     ! 0 |  1291 | `						continue;` |
|       - |  1292 | `					}` |
|    2179 |  1293 | `					break;` |
|     ! 0 |  1294 | `				}` |
|    2179 |  1295 | `				if( zIn >= zEnd ){` |
|     216 |  1296 | `					break;` |
|       - |  1297 | `				}` |
|    1967 |  1298 | `				if( zIn[0] == '[' ){` |
|      12 |  1299 | `					sxi32 iSquare = 1;` |
|      12 |  1300 | `					zIn++;` |
|      28 |  1301 | `					while( zIn < zEnd ){` |
|      28 |  1302 | `						if( zIn[0] == '[' ){` |
|     ! 0 |  1303 | `							iSquare++;` |
|      28 |  1304 | `						}else if (zIn[0] == ']' ){` |
|      12 |  1305 | `							iSquare--;` |
|      12 |  1306 | `							if( iSquare <= 0 ){` |
|      12 |  1307 | `								break;` |
|       - |  1308 | `							}` |
|     ! 0 |  1309 | `						}` |
|      18 |  1310 | `						zIn++;` |
|       2 |  1311 | `					}` |
|      12 |  1312 | `					if( zIn < zEnd ){` |
|      12 |  1313 | `						zIn++;` |
|       5 |  1314 | `					}` |
|      12 |  1315 | `					break;` |
|    1957 |  1316 | `				}else if(zIn[0] == '{' ){` |
|       6 |  1317 | `					sxi32 iCurly = 1;` |
|       6 |  1318 | `					zIn++;` |
|      18 |  1319 | `					while( zIn < zEnd ){` |
|      16 |  1320 | `						if( zIn[0] == '{' ){` |
|     ! 0 |  1321 | `							iCurly++;` |
|      16 |  1322 | `						}else if (zIn[0] == '}' ){` |
|       3 |  1323 | `							iCurly--;` |
|       3 |  1324 | `							if( iCurly <= 0 ){` |
|       3 |  1325 | `								break;` |
|       - |  1326 | `							}` |
|     ! 0 |  1327 | `						}` |
|      14 |  1328 | `						zIn++;` |
|       2 |  1329 | `					}` |
|       6 |  1330 | `					if( zIn < zEnd ){` |
|       3 |  1331 | `						zIn++;` |
|       1 |  1332 | `					}` |
|       6 |  1333 | `					break;` |
|    1953 |  1334 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  1335 | `					/* Member access operator '->' */` |
|      23 |  1336 | `					zIn += 2;` |
|    1943 |  1337 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  1338 | `					/* Static member access operator '::' */` |
|     ! 0 |  1339 | `					zIn += 2;` |
|     ! 0 |  1340 | `				}else{` |
|     969 |  1341 | `					break;` |
|       - |  1342 | `				}` |
|       3 |  1343 | `			}` |
|       - |  1344 | `			/* Process the expression */` |
|    2159 |  1345 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    2159 |  1346 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1347 | `				return SXERR_ABORT;` |
|       - |  1348 | `			}` |
|    2159 |  1349 | `			if( rc != SXERR_EMPTY ){` |
|    2157 |  1350 | `				++iCons;` |
|    1076 |  1351 | `			}` |
|       - |  1352 | `		}` |
|       - |  1353 | `		/* Invalidate the previously used constant */` |
|    2289 |  1354 | `		pObj = 0;` |
|       5 |  1355 | `	}/*for(;;)*/` |
|   24093 |  1356 | `	if( iCons > 1 ){` |
|       - |  1357 | `		/* Concatenate all compiled constants */` |
|    1699 |  1358 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     847 |  1359 | `	}` |
|       - |  1360 | `	/* Node successfully compiled */` |
|   24093 |  1361 | `	return SXRET_OK;` |
|   12203 |  1362 | `}` |
|       - |  1363 | `/*` |
|       - |  1364 | ` * Compile a double quoted string.` |
|       - |  1365 | ` *  See the block-comment above for more information.` |
|       - |  1366 | ` */` |
|   24334 |  1367 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1368 | `{` |
|       - |  1369 | `	sxi32 rc;` |
|   24339 |  1370 | `	rc = GenStateCompileString(&(*pGen),0/*bHeredoc*/);` |
|   12167 |  1371 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1372 | `	/* Compilation result */` |
|   24339 |  1373 | `	return rc;` |
|       5 |  1374 | `}` |
|       - |  1375 | `/*` |
|       - |  1376 | ` * Compile a Heredoc string.` |
|       - |  1377 | ` *  See the block-comment above for more information.` |
|       - |  1378 | ` */` |
|      66 |  1379 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  1380 | `{` |
|       - |  1381 | `	SyString sOrig, sStripped;` |
|       - |  1382 | `	sxi32 rc;` |
|      70 |  1383 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      70 |  1384 | `	if( rc != SXRET_OK ){` |
|       6 |  1385 | `		return rc;` |
|       - |  1386 | `	}` |
|       - |  1387 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|       - |  1388 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|       - |  1389 | `	 * Restore before returning so downstream code that references pIn is` |
|       - |  1390 | `	 * unaffected, including on the error path. */` |
|      64 |  1391 | `	sOrig = pGen->pIn->sData;` |
|      64 |  1392 | `	pGen->pIn->sData = sStripped;` |
|      64 |  1393 | `	rc = GenStateCompileString(&(*pGen),1/*bHeredoc*/);` |
|      64 |  1394 | `	pGen->pIn->sData = sOrig;` |
|      31 |  1395 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|      64 |  1396 | `	return rc;` |
|      37 |  1397 | `}` |
|       - |  1398 | `/*` |
|       - |  1399 | ` * Compile an array entry whether it is a key or a value.` |
|       - |  1400 | ` *  Notes on array entries.` |
|       - |  1401 | ` *  According to the PHP language reference manual` |
|       - |  1402 | ` *  An array can be created by the array() language construct.` |
|       - |  1403 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|       - |  1404 | ` *  array(  key =>  value` |
|       - |  1405 | ` *    , ...` |
|       - |  1406 | ` *    )` |
|       - |  1407 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|       - |  1408 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|       - |  1409 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|       - |  1410 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|       - |  1411 | ` *  contain integer and string indices.` |
|       - |  1412 | ` *  A value can be any PHP type.` |
|       - |  1413 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|       - |  1414 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|       - |  1415 | ` *  is specified, that value will be overwritten.` |
|       - |  1416 | ` */` |
|   22712 |  1417 | `static sxi32 GenStateCompileArrayEntry(` |
|       - |  1418 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  1419 | `	SyToken *pIn,        /* Token stream */` |
|       - |  1420 | `	SyToken *pEnd,       /* End of the token stream */` |
|       - |  1421 | `	sxi32 iFlags,        /* Compilation flags */` |
|       - |  1422 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|       - |  1423 | `	)` |
|       5 |  1424 | `{` |
|       - |  1425 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  1426 | `	sxi32 rc;` |
|       - |  1427 | `	/* Swap token stream */` |
|   22717 |  1428 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - |  1429 | `	/* Compile the expression*/` |
|   22717 |  1430 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - |  1431 | `	/* Restore token stream */` |
|   22717 |  1432 | `	RE_SWAP_DELIMITER(pGen);` |
|   22717 |  1433 | `	return rc;` |
|       5 |  1434 | `}` |
|       - |  1435 | `/*` |
|       - |  1436 | ` * Expression tree validator callback for the 'array' language construct.` |
|       - |  1437 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1438 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1439 | ` * error message.` |
|       - |  1440 | ` * See the routine responible of compiling the array language construct` |
|       - |  1441 | ` * for more inforation.` |
|       - |  1442 | ` */` |
|      36 |  1443 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       4 |  1444 | `{` |
|      40 |  1445 | `	sxi32 rc = SXRET_OK;` |
|      40 |  1446 | `	if( pRoot->pOp ){` |
|      14 |  1447 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|      12 |  1448 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|      16 |  1449 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - |  1450 | `			/* Unexpected expression */` |
|      13 |  1451 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1452 | `				"array(): Expecting a variable/array member/function call after reference operator '&'");` |
|      13 |  1453 | `			if( rc != SXERR_ABORT ){` |
|      13 |  1454 | `				rc = SXERR_INVALID;` |
|       5 |  1455 | `			}` |
|       9 |  1456 | `		}` |
|      31 |  1457 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1458 | `		/* Unexpected expression */` |
|       3 |  1459 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1460 | `			"array(): Expecting a variable after reference operator '&'");` |
|       3 |  1461 | `		if( rc != SXERR_ABORT ){` |
|       3 |  1462 | `			rc = SXERR_INVALID;` |
|       1 |  1463 | `		}` |
|       1 |  1464 | `	}` |
|      40 |  1465 | `	return rc;` |
|       4 |  1466 | `}` |
|       - |  1467 | `/*` |
|       - |  1468 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|       - |  1469 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|       - |  1470 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|       - |  1471 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|       - |  1472 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|       - |  1473 | ` */` |
|   25158 |  1474 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|       5 |  1475 | `{` |
|   25163 |  1476 | `	SyToken *pCur = pStart;` |
|   25163 |  1477 | `	sxi32 iNest = 0;` |
|   71427 |  1478 | `	while( pCur < pEnd ){` |
|   51935 |  1479 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    5667 |  1480 | `			return pCur;` |
|       - |  1481 | `		}` |
|       - |  1482 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - |  1483 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|       - |  1484 | `		 * not an entry separator. Skip past the signature.` |
|       - |  1485 | `		 */` |
|   46273 |  1486 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|      95 |  1487 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|      95 |  1488 | `			SyToken *pFn = pCur;` |
|      92 |  1489 | `			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd` |
|     ! 0 |  1490 | `				&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|       3 |  1491 | `				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  1492 | `				pFn = &pCur[1];` |
|     ! 0 |  1493 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  1494 | `			}` |
|      95 |  1495 | `			if( nKw == PH7_TKWRD_FN ){` |
|       5 |  1496 | `				pCur = pFn + 1; /* past 'fn' */` |
|       5 |  1497 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  1498 | `					pCur++;` |
|     ! 0 |  1499 | `				}` |
|       5 |  1500 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|       5 |  1501 | `					pCur++;` |
|       5 |  1502 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - |  1503 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       5 |  1504 | `					if( pCur < pEnd ){` |
|       5 |  1505 | `						pCur++;` |
|       2 |  1506 | `					}` |
|       2 |  1507 | `				}` |
|       5 |  1508 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_COLON) ){` |
|     ! 0 |  1509 | `					pCur++;` |
|     ! 0 |  1510 | `					if( pCur < pEnd && (pCur->nType & PH7_TK_OP)` |
|     ! 0 |  1511 | `						&& pCur->sData.nByte == 1` |
|     ! 0 |  1512 | `						&& pCur->sData.zString[0] == '?' ){` |
|     ! 0 |  1513 | `						pCur++;` |
|     ! 0 |  1514 | `					}` |
|     ! 0 |  1515 | `					if( pCur < pEnd` |
|     ! 0 |  1516 | `						&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  1517 | `						pCur++;` |
|     ! 0 |  1518 | `					}` |
|     ! 0 |  1519 | `				}` |
|       - |  1520 | `				/* The rest of the entry is the arrow-function body — no outer` |
|       - |  1521 | `				 * key to extract. */` |
|       5 |  1522 | `				return pEnd;` |
|       - |  1523 | `			}` |
|       - |  1524 | `			/* Match expression (PHP 8.0): the '=>' inside match arms is not an` |
|       - |  1525 | `			 * entry separator. Skip past the full match span. */` |
|      91 |  1526 | `			if( nKw == PH7_TKWRD_MATCH ){` |
|       3 |  1527 | `				pCur++; /* past 'match' */` |
|       3 |  1528 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|       3 |  1529 | `					pCur++;` |
|       3 |  1530 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - |  1531 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       3 |  1532 | `					if( pCur < pEnd ){` |
|       3 |  1533 | `						pCur++;` |
|       1 |  1534 | `					}` |
|       1 |  1535 | `				}` |
|       3 |  1536 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_OCB) ){` |
|       3 |  1537 | `					pCur++;` |
|       3 |  1538 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - |  1539 | `						PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|       3 |  1540 | `					if( pCur < pEnd ){` |
|       3 |  1541 | `						pCur++;` |
|       1 |  1542 | `					}` |
|       1 |  1543 | `				}` |
|       3 |  1544 | `				continue;` |
|       - |  1545 | `			}` |
|      43 |  1546 | `		}` |
|   46267 |  1547 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     403 |  1548 | `			iNest++;` |
|   46068 |  1549 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|       - |  1550 | `			/* Don't worry about mismatched brackets here, the expression` |
|       - |  1551 | `			 * parser will shortly detect any syntax error. */` |
|     403 |  1552 | `			iNest--;` |
|     199 |  1553 | `		}` |
|   46267 |  1554 | `		pCur++;` |
|       5 |  1555 | `	}` |
|   19497 |  1556 | `	return pEnd;` |
|   12584 |  1557 | `}` |
|       - |  1558 | `/*` |
|       - |  1559 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - |  1560 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - |  1561 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - |  1562 | ` */` |
|   32484 |  1563 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       5 |  1564 | `{` |
|       - |  1565 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - |  1566 | `	SyToken *pKey,*pCur;` |
|   32489 |  1567 | `	sxi32 iEmitRef = 0;` |
|   32489 |  1568 | `	sxi32 iSpread = 0;` |
|   32489 |  1569 | `	sxi32 nPair = 0;` |
|       - |  1570 | `	sxi32 rc;` |
|   32489 |  1571 | `	xValidator = 0;` |
|   26647 |  1572 | `	for(;;){` |
|       - |  1573 | `		/* Jump leading commas */` |
|   60531 |  1574 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    7237 |  1575 | `			pGen->pIn++;` |
|       5 |  1576 | `		}` |
|   53299 |  1577 | `		pCur = pGen->pIn;` |
|   53299 |  1578 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - |  1579 | `			/* No more entry to process */` |
|   32473 |  1580 | `			break;` |
|       - |  1581 | `		}` |
|   20831 |  1582 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 |  1583 | `			continue;` |
|       - |  1584 | `		}` |
|       - |  1585 | `		/* Compile the key if available */` |
|   20831 |  1586 | `		pKey = pCur;` |
|   20831 |  1587 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   20831 |  1588 | `		rc = SXERR_EMPTY;` |
|   20831 |  1589 | `		if( pCur < pGen->pIn ){` |
|    1697 |  1590 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - |  1591 | `				/* Missing value */` |
|      13 |  1592 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      13 |  1593 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1594 | `					return SXERR_ABORT;` |
|       - |  1595 | `				}` |
|      13 |  1596 | `				return SXRET_OK;` |
|       - |  1597 | `			}` |
|       - |  1598 | `			/* Compile the expression holding the key */` |
|    1687 |  1599 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - |  1600 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1687 |  1601 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1602 | `				return SXERR_ABORT;` |
|       - |  1603 | `			}` |
|    1687 |  1604 | `			pCur++; /* Jump the '=>' operator */` |
|   19980 |  1605 | `		}else if( pKey == pCur ){` |
|       - |  1606 | `			/* Key is omitted,emit a warning */` |
|     ! 0 |  1607 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 |  1608 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 |  1609 | `		}else{` |
|       - |  1610 | `			/* Reset back the cursor and point to the entry value */` |
|   19139 |  1611 | `			pCur = pKey;` |
|       - |  1612 | `		}` |
|   20821 |  1613 | `		if( rc == SXERR_EMPTY ){` |
|       - |  1614 | `			/* No available key,load NULL */` |
|   19141 |  1615 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    9568 |  1616 | `		}` |
|   20821 |  1617 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - |  1618 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      45 |  1619 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      45 |  1620 | `			iEmitRef = 1;` |
|      45 |  1621 | `			pCur++; /* Jump the '&' token */` |
|      45 |  1622 | `			if( pCur >= pGen->pIn ){` |
|       - |  1623 | `				/* Missing value */` |
|       3 |  1624 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|       3 |  1625 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1626 | `					return SXERR_ABORT;` |
|       - |  1627 | `				}` |
|       3 |  1628 | `				return SXRET_OK;` |
|       - |  1629 | `			}` |
|      19 |  1630 | `		}` |
|       - |  1631 | `		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with` |
|       - |  1632 | `		 * string-key support since PHP 8.1). The parser strips the '...' inside` |
|       - |  1633 | `		 * ExprExtractNode; we only need to know it's there so we can emit` |
|       - |  1634 | `		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the` |
|       - |  1635 | `		 * resulting hashmap rather than insert it as a scalar entry. */` |
|   20819 |  1636 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   20819 |  1637 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
|       - |  1638 | `			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the` |
|       - |  1639 | `			 * '...' token cannot follow either '=>' or '&' inside an array` |
|       - |  1640 | `			 * literal. Emit the same Parse-error wording PHP uses so the` |
|       - |  1641 | `			 * output is engine-portable. */` |
|       6 |  1642 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|       - |  1643 | `				"syntax error, unexpected token \"...\"");` |
|       6 |  1644 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1645 | `				return SXERR_ABORT;` |
|       - |  1646 | `			}` |
|       6 |  1647 | `			return SXRET_OK;` |
|       - |  1648 | `		}` |
|       - |  1649 | `		/* Compile indice value */` |
|   20815 |  1650 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   20815 |  1651 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1652 | `			return SXERR_ABORT;` |
|       - |  1653 | `		}` |
|   20815 |  1654 | `		if( iSpread ){` |
|       - |  1655 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|      65 |  1656 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   20784 |  1657 | `		}else if( iEmitRef ){` |
|       - |  1658 | `			/* Emit the load reference instruction */` |
|      40 |  1659 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      18 |  1660 | `		}` |
|   20815 |  1661 | `		xValidator = 0;` |
|   20815 |  1662 | `		iEmitRef = 0;` |
|   20815 |  1663 | `		iSpread = 0;` |
|   20815 |  1664 | `		nPair++;` |
|       5 |  1665 | `	}` |
|       - |  1666 | `	/* Emit the load map instruction */` |
|   32473 |  1667 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - |  1668 | `	/* Node successfully compiled */` |
|   32473 |  1669 | `	return SXRET_OK;` |
|   16247 |  1670 | `}` |
|       - |  1671 | `/*` |
|       - |  1672 | ` * Compile the 'array' language construct.` |
|       - |  1673 | ` *	 According to the PHP language reference manual` |
|       - |  1674 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - |  1675 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - |  1676 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - |  1677 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - |  1678 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - |  1679 | ` */` |
|   31326 |  1680 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1681 | `{` |
|       - |  1682 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   31331 |  1683 | `	pGen->pIn += 2;` |
|   31331 |  1684 | `	pGen->pEnd--;` |
|   15663 |  1685 | `	SXUNUSED(iCompileFlag);` |
|   31331 |  1686 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1687 | `}` |
|       - |  1688 | `/*` |
|       - |  1689 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - |  1690 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - |  1691 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - |  1692 | ` */` |
|    1158 |  1693 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1694 | `{` |
|       - |  1695 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|    1163 |  1696 | `	pGen->pIn++;` |
|    1163 |  1697 | `	pGen->pEnd--;` |
|     579 |  1698 | `	SXUNUSED(iCompileFlag);` |
|    1163 |  1699 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1700 | `}` |
|       - |  1701 | `/*` |
|       - |  1702 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - |  1703 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1704 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1705 | ` * error message.` |
|       - |  1706 | ` * See the routine responible of compiling the list language construct` |
|       - |  1707 | ` * for more inforation.` |
|       - |  1708 | ` */` |
|     178 |  1709 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       4 |  1710 | `{` |
|     182 |  1711 | `	sxi32 rc = SXRET_OK;` |
|     182 |  1712 | `	if( pRoot->pOp ){` |
|       4 |  1713 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|       2 |  1714 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - |  1715 | `				/* Unexpected expression */` |
|     ! 0 |  1716 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1717 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 |  1718 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 |  1719 | `					rc = SXERR_INVALID;` |
|     ! 0 |  1720 | `				}` |
|       1 |  1721 | `		}` |
|     180 |  1722 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1723 | `		/* Unexpected expression */` |
|       6 |  1724 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1725 | `			"list(): Expecting a variable not an expression");` |
|       6 |  1726 | `		if( rc != SXERR_ABORT ){` |
|       6 |  1727 | `			rc = SXERR_INVALID;` |
|       2 |  1728 | `		}` |
|       2 |  1729 | `	}` |
|     182 |  1730 | `	return rc;` |
|       4 |  1731 | `}` |
|       - |  1732 | `/*` |
|       - |  1733 | ` * Compile the 'list' language construct.` |
|       - |  1734 | ` *  According to the PHP language reference` |
|       - |  1735 | ` *  list(): Assign variables as if they were an array.` |
|       - |  1736 | ` *  list() is used to assign a list of variables in one operation.` |
|       - |  1737 | ` *  Description` |
|       - |  1738 | ` *   array list (mixed $varname [, mixed $... ] )` |
|       - |  1739 | ` *   Like array(), this is not really a function, but a language construct.` |
|       - |  1740 | ` *   list() is used to assign a list of variables in one operation.` |
|       - |  1741 | ` *  Parameters` |
|       - |  1742 | ` *   $varname: A variable.` |
|       - |  1743 | ` *  Return Values` |
|       - |  1744 | ` *   The assigned array.` |
|       - |  1745 | ` */` |
|       - |  1746 | `/* Nested list entry recorded during first pass of list body compilation */` |
|       - |  1747 | `struct NestedListEntry {` |
|       - |  1748 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|       - |  1749 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|       - |  1750 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|       - |  1751 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|       - |  1752 | `};` |
|       - |  1753 | `/*` |
|       - |  1754 | ` * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where` |
|       - |  1755 | `` * every entry has the form `keyExpr => target`. The source array is on the stack`` |
|       - |  1756 | ` * top on entry and remains there on exit, mirroring the positional LOAD_LIST` |
|       - |  1757 | ` * path so the caller's teardown is unchanged. For each entry: DUP the source,` |
|       - |  1758 | ` * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,` |
|       - |  1759 | ` * like a normal subscript read), then assign the fetched value to the target — a` |
|       - |  1760 | ` * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a` |
|       - |  1761 | ` * normal assignment (the value sits below the lvalue-load, exactly as in` |
|       - |  1762 | ` * GenStateEmitExprCode where the assignment RHS precedes the LHS load).` |
|       - |  1763 | ` */` |
|      28 |  1764 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|       2 |  1765 | `{` |
|       - |  1766 | `	SyToken *pNext;` |
|       - |  1767 | `	sxi32 rc;` |
|      66 |  1768 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       - |  1769 | `		SyToken *pArrow,*pTarget;` |
|       - |  1770 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|      38 |  1771 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|      38 |  1772 | `		pTarget = &pArrow[1];` |
|      38 |  1773 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|       - |  1774 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|       - |  1775 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|     ! 0 |  1776 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1777 | `				"Cannot use empty array entries in keyed array assignment");` |
|     ! 0 |  1778 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1779 | `		}` |
|       - |  1780 | `		/* DUP the source array (it is on the stack top) */` |
|      38 |  1781 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - |  1782 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|      38 |  1783 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|      38 |  1784 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1785 | `			return SXERR_ABORT;` |
|       - |  1786 | `		}` |
|       - |  1787 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|       - |  1788 | `		 * iP2=7 is the keyed-destructuring read context: an array source reads like` |
|       - |  1789 | ``		 * iP2=0 (missing key loads NULL silently, matching a normal `$arr[$k]` read;`` |
|       - |  1790 | `		 * PHP also emits an "Undefined array key" warning here, PHL omits it — §3.7),` |
|       - |  1791 | `		 * but a NON-array source yields NULL + a per-key "Cannot use <type> as array"` |
|       - |  1792 | `		 * warning instead of char-indexing a string (matching PHP's OP_LOAD_LIST path). */` |
|      38 |  1793 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,7,0,0);` |
|      38 |  1794 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|      34 |  1795 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|      18 |  1796 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|       - |  1797 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|       - |  1798 | `			 * Treat source[key] as the inner body's source, then drop the` |
|       - |  1799 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|       5 |  1800 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|       5 |  1801 | `			SyToken *pSavedIn = pGen->pIn;` |
|       5 |  1802 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       5 |  1803 | `			pGen->pIn = pTarget;` |
|       5 |  1804 | `			pGen->pEnd = pNext;` |
|       5 |  1805 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|       2 |  1806 | `			             : PH7_CompileList(&(*pGen),0);` |
|       5 |  1807 | `			pGen->pIn = pSavedIn;` |
|       5 |  1808 | `			pGen->pEnd = pSavedEnd;` |
|       5 |  1809 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1810 | `				return SXERR_ABORT;` |
|       - |  1811 | `			}` |
|       5 |  1812 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       3 |  1813 | `		}else{` |
|       - |  1814 | `			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]` |
|       - |  1815 | `			 * is already on the stack as the value; compiling the target appends` |
|       - |  1816 | `			 * its lvalue-load, which we fold into a STORE just as a normal` |
|       - |  1817 | `			 * assignment does. */` |
|       - |  1818 | `			VmInstr *pInstr;` |
|      34 |  1819 | `			sxi32 iVmOp = PH7_OP_STORE;` |
|      34 |  1820 | `			sxi32 iP1 = 0, iP2 = 0;` |
|      34 |  1821 | `			void *p3 = 0;` |
|      34 |  1822 | `			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,` |
|       - |  1823 | `				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|      34 |  1824 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  1825 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1826 | `			}` |
|      34 |  1827 | `			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|      34 |  1828 | `				if( pInstr->iOp == PH7_OP_MEMBER ){` |
|       3 |  1829 | `					iP2 = 1; /* member store: keep MEMBER, store value below it */` |
|      33 |  1830 | `				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       3 |  1831 | `					iVmOp = PH7_OP_STORE_IDX;` |
|       3 |  1832 | `					iP1 = pInstr->iP1;` |
|       3 |  1833 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       2 |  1834 | `				}else{` |
|      30 |  1835 | `					p3 = pInstr->p3; /* named store: $v = value */` |
|      30 |  1836 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - |  1837 | `				}` |
|      16 |  1838 | `			}` |
|      34 |  1839 | `			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|       - |  1840 | `			/* STORE leaves the assigned value on the stack top; drop it so the` |
|       - |  1841 | `			 * source array is back on top for the next entry. */` |
|      34 |  1842 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       - |  1843 | `		}` |
|      38 |  1844 | `		pGen->pIn = &pNext[1];` |
|       2 |  1845 | `	}` |
|      30 |  1846 | `	return SXRET_OK;` |
|      16 |  1847 | `}` |
|       - |  1848 | `/*` |
|       - |  1849 | ` * Shared body for list() and short list [...] compilation.` |
|       - |  1850 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|       - |  1851 | ` * the opening delimiter and before the closing delimiter.` |
|       - |  1852 | ` */` |
|     110 |  1853 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|       4 |  1854 | `{` |
|       - |  1855 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - |  1856 | `	SyToken *pNext;` |
|       - |  1857 | `	SyToken *pClassifyIn;` |
|     114 |  1858 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|       - |  1859 | `	sxi32 nExpr;` |
|       - |  1860 | `	sxi32 rc;` |
|       - |  1861 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|       - |  1862 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|       - |  1863 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|       - |  1864 | `	 * list. */` |
|     114 |  1865 | `	pClassifyIn = pGen->pIn;` |
|     322 |  1866 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     212 |  1867 | `		if( pGen->pIn >= pNext ){` |
|      13 |  1868 | `			nEmpty++;` |
|     206 |  1869 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|      38 |  1870 | `			nKeyed++;` |
|      20 |  1871 | `		}else{` |
|     164 |  1872 | `			nPositional++;` |
|       - |  1873 | `		}` |
|     212 |  1874 | `		pGen->pIn = &pNext[1];` |
|       4 |  1875 | `	}` |
|     114 |  1876 | `	pGen->pIn = pClassifyIn;` |
|     114 |  1877 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|     ! 0 |  1878 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1879 | `			"Cannot use empty array entries in keyed array assignment");` |
|     ! 0 |  1880 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1881 | `	}` |
|     114 |  1882 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|     ! 0 |  1883 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1884 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|     ! 0 |  1885 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1886 | `	}` |
|     114 |  1887 | `	if( nKeyed > 0 ){` |
|      30 |  1888 | `		return GenStateCompileKeyedListBody(pGen);` |
|       - |  1889 | `	}` |
|      86 |  1890 | `	nExpr = 0;` |
|      86 |  1891 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|     258 |  1892 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     176 |  1893 | `		if( pGen->pIn < pNext ){` |
|       - |  1894 | `			/* Check for nested list() */` |
|     164 |  1895 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       3 |  1896 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  1897 | `				/* Record this nested list for post-processing */` |
|       3 |  1898 | `				SyToken *pListEnd = 0;` |
|       3 |  1899 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|       3 |  1900 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       1 |  1901 | `				}` |
|       3 |  1902 | `				if( pListEnd ){` |
|       - |  1903 | `					struct NestedListEntry sEntry;` |
|       3 |  1904 | `					sEntry.nIndex = nExpr;` |
|       3 |  1905 | `					sEntry.pStart = pGen->pIn;` |
|       3 |  1906 | `					sEntry.pEnd = pListEnd + 1;` |
|       3 |  1907 | `					sEntry.isShort = 0;` |
|       3 |  1908 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       1 |  1909 | `				}` |
|       - |  1910 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       3 |  1911 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     163 |  1912 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  1913 | `				/* Nested short destructuring [...] */` |
|      13 |  1914 | `				SyToken *pBracketEnd = 0;` |
|      13 |  1915 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|      13 |  1916 | `				if( pBracketEnd ){` |
|       - |  1917 | `					struct NestedListEntry sEntry;` |
|      13 |  1918 | `					sEntry.nIndex = nExpr;` |
|      13 |  1919 | `					sEntry.pStart = pGen->pIn;` |
|      13 |  1920 | `					sEntry.pEnd = pBracketEnd + 1;` |
|      13 |  1921 | `					sEntry.isShort = 1;` |
|      13 |  1922 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       6 |  1923 | `				}` |
|       - |  1924 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|      13 |  1925 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |  1926 | `			}else{` |
|       - |  1927 | `				/* Compile the expression holding the variable */` |
|     150 |  1928 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     150 |  1929 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  1930 | `					SySetRelease(&sNested);` |
|     ! 0 |  1931 | `					return SXRET_OK;` |
|       - |  1932 | `				}` |
|       - |  1933 | `			}` |
|      84 |  1934 | `		}else{` |
|       - |  1935 | `			/* Empty entry,load NULL */` |
|      13 |  1936 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - |  1937 | `		}` |
|     176 |  1938 | `		nExpr++;` |
|       - |  1939 | `		/* Advance the stream cursor */` |
|     176 |  1940 | `		pGen->pIn = &pNext[1];` |
|       4 |  1941 | `	}` |
|       - |  1942 | `	/* Emit the LOAD_LIST instruction */` |
|      86 |  1943 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - |  1944 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - |  1945 | `	 * For each nested entry, emit code to extract the sub-array` |
|       - |  1946 | `	 * at the corresponding index and recursively destructure it.` |
|       - |  1947 | `	 */` |
|      86 |  1948 | `	if( SySetUsed(&sNested) > 0 ){` |
|      13 |  1949 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|       - |  1950 | `		sxu32 i;` |
|      27 |  1951 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|      15 |  1952 | `			SyToken *pSavedIn = pGen->pIn;` |
|      15 |  1953 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  1954 | `			ph7_value *pIdx;` |
|       - |  1955 | `			sxu32 nConstIdx;` |
|       - |  1956 | `			/* DUP the source array (it's on stack top) */` |
|      15 |  1957 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - |  1958 | `			/* Push the integer index for this nested entry */` |
|      15 |  1959 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|      15 |  1960 | `			if( pIdx == 0 ){` |
|     ! 0 |  1961 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  1962 | `				SySetRelease(&sNested);` |
|     ! 0 |  1963 | `				return SXERR_ABORT;` |
|       - |  1964 | `			}` |
|      15 |  1965 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|      15 |  1966 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|       - |  1967 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|       - |  1968 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|       - |  1969 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|       - |  1970 | `			 */` |
|      15 |  1971 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|       - |  1972 | `			/* Recursively compile the inner list */` |
|      15 |  1973 | `			pGen->pIn = apNested[i].pStart;` |
|      15 |  1974 | `			pGen->pEnd = apNested[i].pEnd;` |
|      15 |  1975 | `			if( apNested[i].isShort ){` |
|      13 |  1976 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|       7 |  1977 | `			}else{` |
|       3 |  1978 | `				rc = PH7_CompileList(&(*pGen),0);` |
|       - |  1979 | `			}` |
|      15 |  1980 | `			pGen->pIn = pSavedIn;` |
|      15 |  1981 | `			pGen->pEnd = pSavedEnd;` |
|      15 |  1982 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1983 | `				SySetRelease(&sNested);` |
|     ! 0 |  1984 | `				return SXERR_ABORT;` |
|       - |  1985 | `			}` |
|       - |  1986 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|      15 |  1987 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       8 |  1988 | `		}` |
|       6 |  1989 | `	}` |
|      86 |  1990 | `	SySetRelease(&sNested);` |
|       - |  1991 | `	/* Node successfully compiled */` |
|      86 |  1992 | `	return SXRET_OK;` |
|      59 |  1993 | `}` |
|      34 |  1994 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1995 | `{` |
|       - |  1996 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|      36 |  1997 | `	pGen->pIn += 2;` |
|      36 |  1998 | `	pGen->pEnd--;` |
|      17 |  1999 | `	SXUNUSED(iCompileFlag);` |
|      36 |  2000 | `	return GenStateCompileListBody(pGen);` |
|       2 |  2001 | `}` |
|      76 |  2002 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  2003 | `{` |
|       - |  2004 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      80 |  2005 | `	pGen->pIn++;` |
|      80 |  2006 | `	pGen->pEnd--;` |
|      38 |  2007 | `	SXUNUSED(iCompileFlag);` |
|      80 |  2008 | `	return GenStateCompileListBody(pGen);` |
|       4 |  2009 | `}` |
|       - |  2010 | `/* Forward declarations */` |
|       - |  2011 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|       - |  2012 | `static int GenStateIsReservedConstant(SyString *pName);` |
|       - |  2013 | `static int GenStateIsReadonly(SyToken *pTok);` |
|       - |  2014 | `static sxi32 GenStateValidateMemberType(ph7_gen_state *pGen,ph7_class *pClass,const SyString *pMemberName,` |
|       - |  2015 | `	sxu32 nType,const SyString *pTypeClass,const SyString *pTypeText,SySet *pUnionAlts,const char *zErrFmt,sxu32 nLine);` |
|       - |  2016 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|       - |  2017 | `/*` |
|       - |  2018 | ` * Compile an annoynmous function or a closure.` |
|       - |  2019 | ` * According to the PHP language reference` |
|       - |  2020 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - |  2021 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - |  2022 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - |  2023 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - |  2024 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - |  2025 | ` *  Example Anonymous function variable assignment example` |
|       - |  2026 | ` * <?php` |
|       - |  2027 | ` * $greet = function($name)` |
|       - |  2028 | ` * {` |
|       - |  2029 | ` *    printf("Hello %s\r\n", $name);` |
|       - |  2030 | ` * };` |
|       - |  2031 | ` * $greet('World');` |
|       - |  2032 | ` * $greet('PHP');` |
|       - |  2033 | ` * ?>` |
|       - |  2034 | ` * Note that the implementation of annoynmous function and closure under` |
|       - |  2035 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - |  2036 | ` */` |
|     294 |  2037 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2038 | `{` |
|       - |  2039 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - |  2040 | `	char zName[512];         /* Unique lambda name */` |
|       - |  2041 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - |  2042 | `							  * one thread is allowed to compile the script.` |
|       - |  2043 | `						      */` |
|       - |  2044 | `	SyString sName;` |
|       - |  2045 | `	sxu32 nLen;` |
|       - |  2046 | `	sxi32 rc;` |
|     147 |  2047 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2048 |  |
|     299 |  2049 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     299 |  2050 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 |  2051 | `		pGen->pIn++;` |
|     ! 0 |  2052 | `	}` |
|       - |  2053 | `	/* Generate a unique name */` |
|     299 |  2054 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - |  2055 | `	/* Make sure the generated name is unique */` |
|     299 |  2056 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  2057 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 |  2058 | `	}` |
|     299 |  2059 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - |  2060 | `	/* Compile the lambda body */` |
|     299 |  2061 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     299 |  2062 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2063 | `		return SXERR_ABORT;` |
|       - |  2064 | `	}` |
|       - |  2065 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|       - |  2066 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|       - |  2067 | `	 * the handler wraps either in a Closure instance. */` |
|     299 |  2068 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       - |  2069 | `	/* Node successfully compiled */` |
|     299 |  2070 | `	return SXRET_OK;` |
|     152 |  2071 | `}` |
|       - |  2072 | `/*` |
|       - |  2073 | ` * Add a free variable to the arrow function's closure environment, unless` |
|       - |  2074 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|       - |  2075 | ` * enclosing arrow level, or has already been captured.` |
|       - |  2076 | ` */` |
|     184 |  2077 | `static sxi32 GenStateArrowAddCapture(` |
|       - |  2078 | `	ph7_gen_state *pGen,` |
|       - |  2079 | `	ph7_vm_func *pFunc,` |
|       - |  2080 | `	const char *zName,` |
|       - |  2081 | `	sxu32 nByte,` |
|       - |  2082 | `	SyString *aShadow,` |
|       - |  2083 | `	sxu32 nShadow)` |
|       2 |  2084 | `{` |
|       - |  2085 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2086 | `	ph7_vm_func_closure_env *aEnv;` |
|       - |  2087 | `	sxu32 n, nEnv;` |
|       - |  2088 | `	char *zDup;` |
|     186 |  2089 | `	if( nByte == 0 ){` |
|     ! 0 |  2090 | `		return SXRET_OK;` |
|       - |  2091 | `	}` |
|     184 |  2092 | `	if( nByte == sizeof("this")-1` |
|     100 |  2093 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|       3 |  2094 | `		return SXRET_OK;` |
|       - |  2095 | `	}` |
|     232 |  2096 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|     172 |  2097 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|     165 |  2098 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|     126 |  2099 | `			return SXRET_OK;` |
|       - |  2100 | `		}` |
|      26 |  2101 | `	}` |
|      59 |  2102 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      59 |  2103 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|      87 |  2104 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|      28 |  2105 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|      27 |  2106 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|     ! 0 |  2107 | `			return SXRET_OK;` |
|       - |  2108 | `		}` |
|      15 |  2109 | `	}` |
|      59 |  2110 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|      59 |  2111 | `	if( zDup == 0 ){` |
|     ! 0 |  2112 | `		return SXERR_ABORT;` |
|       - |  2113 | `	}` |
|      59 |  2114 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      59 |  2115 | `	sEnv.iFlags = 0;` |
|      59 |  2116 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      59 |  2117 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|      59 |  2118 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      59 |  2119 | `	return SXRET_OK;` |
|      94 |  2120 | `}` |
|       - |  2121 | `/*` |
|       - |  2122 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|       - |  2123 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|       - |  2124 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|       - |  2125 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|       - |  2126 | ` */` |
|      36 |  2127 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|       - |  2128 | `	ph7_gen_state *pGen,` |
|       - |  2129 | `	ph7_vm_func *pFunc,` |
|       - |  2130 | `	const char *zIn,` |
|       - |  2131 | `	const char *zEnd,` |
|       - |  2132 | `	SyString *aShadow,` |
|       - |  2133 | `	sxu32 nShadow)` |
|       2 |  2134 | `{` |
|       - |  2135 | `	sxi32 rc;` |
|     302 |  2136 | `	while( zIn < zEnd ){` |
|     266 |  2137 | `		if( zIn[0] == '\\' ){` |
|       5 |  2138 | `			zIn++;` |
|       5 |  2139 | `			if( zIn < zEnd ){` |
|       5 |  2140 | `				zIn++;` |
|       2 |  2141 | `			}` |
|       5 |  2142 | `			continue;` |
|       - |  2143 | `		}` |
|     260 |  2144 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|      22 |  2145 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|      20 |  2146 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|       - |  2147 | `			const char *zName;` |
|      22 |  2148 | `			zIn++; /* skip '$' */` |
|      22 |  2149 | `			zName = zIn;` |
|      74 |  2150 | `			while( zIn < zEnd ){` |
|      70 |  2151 | `				unsigned char c = (unsigned char)zIn[0];` |
|      70 |  2152 | `				if( c >= 0xc0 ){` |
|     ! 0 |  2153 | `					zIn++;` |
|     ! 0 |  2154 | `					while( zIn < zEnd` |
|     ! 0 |  2155 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  2156 | `						zIn++;` |
|     ! 0 |  2157 | `					}` |
|     ! 0 |  2158 | `					continue;` |
|       - |  2159 | `				}` |
|      70 |  2160 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|      18 |  2161 | `					break;` |
|       - |  2162 | `				}` |
|      54 |  2163 | `				zIn++;` |
|       2 |  2164 | `			}` |
|      22 |  2165 | `			if( zIn > zName ){` |
|      32 |  2166 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|      20 |  2167 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|      22 |  2168 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2169 | `					return SXERR_ABORT;` |
|       - |  2170 | `				}` |
|      10 |  2171 | `			}` |
|      22 |  2172 | `			continue;` |
|       - |  2173 | `		}` |
|     242 |  2174 | `		zIn++;` |
|       2 |  2175 | `	}` |
|      38 |  2176 | `	return SXRET_OK;` |
|      20 |  2177 | `}` |
|       - |  2178 | `/*` |
|       - |  2179 | ` * Scan the body token range of an arrow function for free-variable` |
|       - |  2180 | ` * references and record them in pFunc's closure environment. Handles:` |
|       - |  2181 | ` *   - plain $<id> pairs` |
|       - |  2182 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|       - |  2183 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|       - |  2184 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|       - |  2185 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|       - |  2186 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|       - |  2187 | ` *     are never mistakenly captured.` |
|       - |  2188 | ` */` |
|     192 |  2189 | `static sxi32 GenStateArrowCaptureScan(` |
|       - |  2190 | `	ph7_gen_state *pGen,` |
|       - |  2191 | `	ph7_vm_func *pFunc,` |
|       - |  2192 | `	SyToken *pStart,` |
|       - |  2193 | `	SyToken *pEnd,` |
|       - |  2194 | `	SyString *aShadow,` |
|       - |  2195 | `	sxu32 nShadow)` |
|       3 |  2196 | `{` |
|     195 |  2197 | `	SyToken *pScan = pStart;` |
|       - |  2198 | `	sxi32 rc;` |
|     805 |  2199 | `	while( pScan < pEnd ){` |
|     613 |  2200 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|      56 |  2201 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|      18 |  2202 | `				pScan->sData.zString,` |
|      36 |  2203 | `				pScan->sData.zString + pScan->sData.nByte,` |
|      18 |  2204 | `				aShadow,nShadow);` |
|      38 |  2205 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2206 | `				return SXERR_ABORT;` |
|       - |  2207 | `			}` |
|      38 |  2208 | `			pScan++;` |
|      38 |  2209 | `			continue;` |
|       - |  2210 | `		}` |
|     577 |  2211 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|      24 |  2212 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|      24 |  2213 | `			SyToken *pFnKw = pScan;` |
|      22 |  2214 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|     ! 0 |  2215 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|       2 |  2216 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  2217 | `				pFnKw = &pScan[1];` |
|     ! 0 |  2218 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  2219 | `			}` |
|      24 |  2220 | `			if( nKw == PH7_TKWRD_FN ){` |
|       - |  2221 | `				SyToken *pInnerSigStart;` |
|       - |  2222 | `				SyToken *pInnerSigEnd;` |
|       - |  2223 | `				SyToken *pInnerBodyEnd;` |
|       - |  2224 | `				SyString *aInnerShadow;` |
|       - |  2225 | `				sxu32 nInnerShadow;` |
|       - |  2226 | `				sxu32 nInnerParamMax;` |
|       - |  2227 | `				SyToken *p;` |
|       - |  2228 | `				int iNestInner;` |
|      19 |  2229 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|      19 |  2230 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2231 | `					pScan++;` |
|     ! 0 |  2232 | `				}` |
|      19 |  2233 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2234 | `					pScan++;` |
|     ! 0 |  2235 | `					continue;` |
|       - |  2236 | `				}` |
|      19 |  2237 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|      19 |  2238 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|       - |  2239 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|      19 |  2240 | `				if( pInnerSigEnd >= pEnd ){` |
|     ! 0 |  2241 | `					pScan = pEnd;` |
|     ! 0 |  2242 | `					continue;` |
|       - |  2243 | `				}` |
|       - |  2244 | `				/* Build an augmented shadow list: inherited + inner params */` |
|      19 |  2245 | `				nInnerParamMax = 0;` |
|      57 |  2246 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2247 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|      13 |  2248 | `						nInnerParamMax++;` |
|       6 |  2249 | `					}` |
|      20 |  2250 | `				}` |
|      19 |  2251 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      18 |  2252 | `					&pGen->pVm->sAllocator,` |
|      18 |  2253 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|      19 |  2254 | `				if( aInnerShadow == 0 ){` |
|     ! 0 |  2255 | `					return SXERR_ABORT;` |
|       - |  2256 | `				}` |
|      19 |  2257 | `				nInnerShadow = 0;` |
|      25 |  2258 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|       7 |  2259 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|       4 |  2260 | `				}` |
|      57 |  2261 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2262 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      27 |  2263 | `						continue;` |
|       - |  2264 | `					}` |
|      13 |  2265 | `					if( &p[1] >= pInnerSigEnd ){` |
|     ! 0 |  2266 | `						break;` |
|       - |  2267 | `					}` |
|      13 |  2268 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2269 | `						continue;` |
|       - |  2270 | `					}` |
|      13 |  2271 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|       7 |  2272 | `				}` |
|      19 |  2273 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|      19 |  2274 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|     ! 0 |  2275 | `					pScan++;` |
|     ! 0 |  2276 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|     ! 0 |  2277 | `						&& pScan->sData.nByte == 1` |
|     ! 0 |  2278 | `						&& pScan->sData.zString[0] == '?' ){` |
|     ! 0 |  2279 | `						pScan++;` |
|     ! 0 |  2280 | `					}` |
|     ! 0 |  2281 | `					if( pScan < pEnd` |
|     ! 0 |  2282 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  2283 | `						pScan++;` |
|     ! 0 |  2284 | `					}` |
|     ! 0 |  2285 | `				}` |
|      19 |  2286 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|      19 |  2287 | `					pScan++; /* past '=>' */` |
|       9 |  2288 | `				}` |
|      19 |  2289 | `				pInnerBodyEnd = pScan;` |
|      19 |  2290 | `				iNestInner = 0;` |
|     131 |  2291 | `				while( pInnerBodyEnd < pEnd ){` |
|     113 |  2292 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|       - |  2293 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|       - |  2294 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|     ! 0 |  2295 | `						break;` |
|       - |  2296 | `					}` |
|     113 |  2297 | `					if( pInnerBodyEnd->nType &` |
|       - |  2298 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       3 |  2299 | `						iNestInner++;` |
|     112 |  2300 | `					}else if( pInnerBodyEnd->nType &` |
|       - |  2301 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       3 |  2302 | `						iNestInner--;` |
|       1 |  2303 | `					}` |
|     113 |  2304 | `					pInnerBodyEnd++;` |
|       1 |  2305 | `				}` |
|       - |  2306 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|       - |  2307 | `				 * the outer's body: a default value is evaluated at call time` |
|       - |  2308 | `				 * in the outer frame, so any free variable it references is` |
|       - |  2309 | `				 * an outer capture. We must NOT scan the parameter-name` |
|       - |  2310 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|       - |  2311 | `				 * or those names leak into the outer's closure environment.` |
|       - |  2312 | `				 *` |
|       - |  2313 | `				 * Walk the signature argument-by-argument, splitting on` |
|       - |  2314 | `				 * top-level commas, and for each argument scan only the token` |
|       - |  2315 | `				 * range after the '=' sign. */` |
|       - |  2316 | `				{` |
|      19 |  2317 | `					SyToken *pArgStart = pInnerSigStart;` |
|      31 |  2318 | `					while( pArgStart < pInnerSigEnd ){` |
|      13 |  2319 | `						SyToken *pArgEnd = pArgStart;` |
|      13 |  2320 | `						SyToken *pEq = 0;` |
|      13 |  2321 | `						int iNestArg = 0;` |
|      49 |  2322 | `						while( pArgEnd < pInnerSigEnd ){` |
|      38 |  2323 | `							if( iNestArg == 0` |
|      39 |  2324 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|       3 |  2325 | `								break;` |
|       - |  2326 | `							}` |
|      37 |  2327 | `							if( pArgEnd->nType &` |
|       - |  2328 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  2329 | `								iNestArg++;` |
|      37 |  2330 | `							}else if( pArgEnd->nType &` |
|       - |  2331 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  2332 | `								iNestArg--;` |
|     ! 0 |  2333 | `							}` |
|      36 |  2334 | `							if( pEq == 0 && iNestArg == 0` |
|      31 |  2335 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|       7 |  2336 | `								pEq = pArgEnd;` |
|       3 |  2337 | `							}` |
|      37 |  2338 | `							pArgEnd++;` |
|       1 |  2339 | `						}` |
|      13 |  2340 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|      10 |  2341 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       3 |  2342 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|       7 |  2343 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  2344 | `								return SXERR_ABORT;` |
|       - |  2345 | `							}` |
|       3 |  2346 | `						}` |
|      13 |  2347 | `						pArgStart = pArgEnd;` |
|      12 |  2348 | `						if( pArgStart < pInnerSigEnd` |
|       8 |  2349 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|       3 |  2350 | `							pArgStart++;` |
|       1 |  2351 | `						}` |
|       1 |  2352 | `					}` |
|       - |  2353 | `				}` |
|      28 |  2354 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       9 |  2355 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|      19 |  2356 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2357 | `					return SXERR_ABORT;` |
|       - |  2358 | `				}` |
|      19 |  2359 | `				pScan = pInnerBodyEnd;` |
|      19 |  2360 | `				continue;` |
|       - |  2361 | `			}` |
|       2 |  2362 | `		}` |
|     559 |  2363 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     395 |  2364 | `			pScan++;` |
|     395 |  2365 | `			continue;` |
|       - |  2366 | `		}` |
|       - |  2367 | `		{` |
|       - |  2368 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|     166 |  2369 | `			SyToken *pDollar = pScan;` |
|     246 |  2370 | `			while( &pDollar[1] < pEnd` |
|     166 |  2371 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 |  2372 | `				pDollar++;` |
|     ! 0 |  2373 | `			}` |
|     166 |  2374 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 |  2375 | `				break;` |
|       - |  2376 | `			}` |
|     166 |  2377 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2378 | `				pScan = pDollar + 1;` |
|     ! 0 |  2379 | `				continue;` |
|       - |  2380 | `			}` |
|     248 |  2381 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|     164 |  2382 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      82 |  2383 | `				aShadow,nShadow);` |
|     166 |  2384 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2385 | `				return SXERR_ABORT;` |
|       - |  2386 | `			}` |
|     166 |  2387 | `			pScan = pDollar + 2;` |
|       - |  2388 | `		}` |
|       2 |  2389 | `	}` |
|     195 |  2390 | `	return SXRET_OK;` |
|      99 |  2391 | `}` |
|       - |  2392 | `/*` |
|       - |  2393 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  2394 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  2395 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  2396 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  2397 | ` * $this is also made available.` |
|       - |  2398 | ` */` |
|     174 |  2399 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2400 | `{` |
|       - |  2401 | `	ph7_vm_func *pFunc;` |
|       - |  2402 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2403 | `	GenBlock *pBlock;` |
|       - |  2404 | `	SySet *pInstrContainer;` |
|       - |  2405 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|       - |  2406 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|       - |  2407 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|       - |  2408 | `	SyToken *pSavedEnd;` |
|       - |  2409 | `	ph7_vm_func_arg *aArgs;` |
|       - |  2410 | `	char zName[512];` |
|       - |  2411 | `	static int iCnt = 1;` |
|       - |  2412 | `	char *zDup;` |
|       - |  2413 | `	sxu32 nLen;` |
|       - |  2414 | `	sxu32 nLine;` |
|     179 |  2415 | `	sxi32 iFlags = 0;` |
|     179 |  2416 | `	int bStatic = 0;` |
|       - |  2417 | `	sxi32 rc;` |
|       - |  2418 | `	sxu32 n;` |
|      87 |  2419 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2420 |  |
|     179 |  2421 | `	nLine = pGen->pIn->nLine;` |
|       - |  2422 | `	/* Optional 'static' prefix */` |
|     174 |  2423 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     179 |  2424 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       3 |  2425 | `		bStatic = 1;` |
|       3 |  2426 | `		pGen->pIn++;` |
|       1 |  2427 | `	}` |
|       - |  2428 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|     174 |  2429 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     179 |  2430 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  2431 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2432 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  2433 | `		return SXERR_SYNTAX;` |
|       - |  2434 | `	}` |
|     179 |  2435 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  2436 | `	/* Optional '&' — return by reference */` |
|     179 |  2437 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2438 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  2439 | `		pGen->pIn++;` |
|     ! 0 |  2440 | `	}` |
|       - |  2441 | `	/* Expect '(' */` |
|     179 |  2442 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  2443 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2444 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2445 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|       2 |  2446 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2447 | `		}else{` |
|     ! 0 |  2448 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2449 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|       - |  2450 | `		}` |
|       3 |  2451 | `		return SXERR_SYNTAX;` |
|       - |  2452 | `	}` |
|     177 |  2453 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  2454 | `	/* Delimit the parameter list */` |
|     177 |  2455 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|     177 |  2456 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 |  2457 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2458 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 |  2459 | `		return SXERR_SYNTAX;` |
|       - |  2460 | `	}` |
|       - |  2461 | `	/* Allocate the function state */` |
|     174 |  2462 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|     174 |  2463 | `	if( pFunc == 0 ){` |
|     ! 0 |  2464 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2465 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2466 | `		return SXERR_ABORT;` |
|       - |  2467 | `	}` |
|       - |  2468 | `	/* Generate a unique lambda name */` |
|     174 |  2469 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     268 |  2470 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      96 |  2471 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       2 |  2472 | `	}` |
|     174 |  2473 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|     174 |  2474 | `	if( zDup == 0 ){` |
|     ! 0 |  2475 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2476 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2477 | `		return SXERR_ABORT;` |
|       - |  2478 | `	}` |
|     174 |  2479 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  2480 | `	/* Collect function arguments */` |
|     174 |  2481 | `	if( pGen->pIn < pSigEnd ){` |
|     103 |  2482 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|     103 |  2483 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2484 | `			return SXERR_ABORT;` |
|       - |  2485 | `		}` |
|      50 |  2486 | `	}` |
|       - |  2487 | `	/* Point past ')' and parse optional return type */` |
|     174 |  2488 | `	pGen->pIn = &pSigEnd[1];` |
|     174 |  2489 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|     174 |  2490 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2491 | `		return SXERR_ABORT;` |
|     174 |  2492 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  2493 | `		return SXERR_SYNTAX;` |
|       - |  2494 | `	}` |
|       - |  2495 | `	/* Expect '=>' */` |
|     174 |  2496 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2497 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2498 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2499 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|       2 |  2500 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2501 | `		}else{` |
|     ! 0 |  2502 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2503 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|       - |  2504 | `		}` |
|       3 |  2505 | `		return SXERR_SYNTAX;` |
|       - |  2506 | `	}` |
|     171 |  2507 | `	pGen->pIn++; /* Jump '=>' */` |
|     171 |  2508 | `	pBodyStart = pGen->pIn;` |
|     171 |  2509 | `	pBodyEnd = pGen->pEnd;` |
|       - |  2510 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  2511 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  2512 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  2513 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|     171 |  2514 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  2515 | `	{` |
|     171 |  2516 | `		SyString *aShadow = 0;` |
|     171 |  2517 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|     171 |  2518 | `		if( nShadow > 0 ){` |
|     100 |  2519 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      98 |  2520 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|     100 |  2521 | `			if( aShadow == 0 ){` |
|     ! 0 |  2522 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2523 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2524 | `				return SXERR_ABORT;` |
|       - |  2525 | `			}` |
|     224 |  2526 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|     126 |  2527 | `				aShadow[n] = aArgs[n].sName;` |
|      64 |  2528 | `			}` |
|      49 |  2529 | `		}` |
|     255 |  2530 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      84 |  2531 | `			aShadow,nShadow);` |
|     171 |  2532 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2533 | `			return SXERR_ABORT;` |
|       - |  2534 | `		}` |
|       - |  2535 | `	}` |
|       - |  2536 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  2537 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  2538 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  2539 | `	 * $this. */` |
|     171 |  2540 | `	if( !bStatic ){` |
|       - |  2541 | `		char *zThisDup;` |
|     169 |  2542 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|     169 |  2543 | `		if( zThisDup == 0 ){` |
|     ! 0 |  2544 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2545 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2546 | `			return SXERR_ABORT;` |
|       - |  2547 | `		}` |
|     169 |  2548 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     169 |  2549 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|     169 |  2550 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     169 |  2551 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|     169 |  2552 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      83 |  2553 | `	}` |
|       - |  2554 | `	/* Arrow functions are always closures */` |
|     171 |  2555 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       - |  2556 | `	/* Compile the body expression as an implicit return */` |
|     255 |  2557 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      84 |  2558 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|     171 |  2559 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2560 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2561 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  2562 | `		return SXERR_ABORT;` |
|       - |  2563 | `	}` |
|     171 |  2564 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     171 |  2565 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|     171 |  2566 | `	pSavedEnd = pGen->pEnd;` |
|     171 |  2567 | `	pGen->pIn = pBodyStart;` |
|     171 |  2568 | `	pGen->pEnd = pBodyEnd;` |
|     171 |  2569 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     171 |  2570 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2571 | `		return SXERR_ABORT;` |
|       - |  2572 | `	}` |
|       - |  2573 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|       - |  2574 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|       - |  2575 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|       - |  2576 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|     171 |  2577 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     171 |  2578 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     171 |  2579 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     171 |  2580 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     171 |  2581 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  2582 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|     171 |  2583 | `	pGen->pIn = pBodyEnd;` |
|     171 |  2584 | `	pGen->pEnd = pSavedEnd;` |
|       - |  2585 | `	/* Emit the load-closure instruction */` |
|     171 |  2586 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|     171 |  2587 | `	return SXRET_OK;` |
|      92 |  2588 | `}` |
|       - |  2589 | `/*` |
|       - |  2590 | ` * Compile a single arm's expression range into a freshly-allocated` |
|       - |  2591 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|       - |  2592 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|       - |  2593 | ` * expression's value.` |
|       - |  2594 | ` */` |
|     346 |  2595 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|       - |  2596 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|       3 |  2597 | `{` |
|       - |  2598 | `	SySet *pInstrContainer;` |
|       - |  2599 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  2600 | `	GenBlock *pArmBlock;` |
|       - |  2601 | `	sxi32 rc;` |
|     349 |  2602 | `	pTmpIn  = pGen->pIn;` |
|     349 |  2603 | `	pTmpEnd = pGen->pEnd;` |
|     349 |  2604 | `	pGen->pIn  = pStart;` |
|     349 |  2605 | `	pGen->pEnd = pStop;` |
|     349 |  2606 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     349 |  2607 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|       - |  2608 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|       - |  2609 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|       - |  2610 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|       - |  2611 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|       - |  2612 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|     522 |  2613 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|     173 |  2614 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|     349 |  2615 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2616 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  2617 | `		pGen->pIn  = pTmpIn;` |
|     ! 0 |  2618 | `		pGen->pEnd = pTmpEnd;` |
|     ! 0 |  2619 | `		return SXERR_ABORT;` |
|       - |  2620 | `	}` |
|     349 |  2621 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     349 |  2622 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     349 |  2623 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     349 |  2624 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     349 |  2625 | `	GenStateLeaveBlock(&(*pGen),0);` |
|     349 |  2626 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     349 |  2627 | `	pGen->pIn  = pTmpIn;` |
|     349 |  2628 | `	pGen->pEnd = pTmpEnd;` |
|     349 |  2629 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2630 | `		return SXERR_ABORT;` |
|       - |  2631 | `	}` |
|     349 |  2632 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  2633 | `		return SXERR_EMPTY;` |
|       - |  2634 | `	}` |
|     349 |  2635 | `	return SXRET_OK;` |
|     176 |  2636 | `}` |
|       - |  2637 | `/*` |
|       - |  2638 | ` * Compile a PHP 8.0 match expression:` |
|       - |  2639 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|       - |  2640 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|       - |  2641 | ` * Strict comparison (===) is used between the subject and each condition.` |
|       - |  2642 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|       - |  2643 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|       - |  2644 | ` */` |
|       - |  2645 | `/*` |
|       - |  2646 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|       - |  2647 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|       - |  2648 | ` * caller can bail out of the current expression.` |
|       - |  2649 | ` */` |
|       2 |  2650 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|       1 |  2651 | `{` |
|       - |  2652 | `	va_list ap;` |
|       - |  2653 | `	sxi32 rc;` |
|       - |  2654 | `	SyBlob sMsg;` |
|       3 |  2655 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       3 |  2656 | `	va_start(ap,zFmt);` |
|       3 |  2657 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|       3 |  2658 | `	va_end(ap);` |
|       3 |  2659 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|       3 |  2660 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|       3 |  2661 | `	SyBlobRelease(&sMsg);` |
|       3 |  2662 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2663 | `		return SXERR_ABORT;` |
|       - |  2664 | `	}` |
|       3 |  2665 | `	return SXERR_SYNTAX;` |
|       2 |  2666 | `}` |
|       - |  2667 | `/*` |
|       - |  2668 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|       - |  2669 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|       - |  2670 | ` * Returns the stop token pointer (or pEnd if none found).` |
|       - |  2671 | ` */` |
|     348 |  2672 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|       4 |  2673 | `{` |
|     352 |  2674 | `	SyToken *pCur = pStart;` |
|     352 |  2675 | `	int iNest = 0;` |
|     814 |  2676 | `	while( pCur < pEnd ){` |
|     780 |  2677 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      13 |  2678 | `			iNest++;` |
|     774 |  2679 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      13 |  2680 | `			iNest--;` |
|     762 |  2681 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|     317 |  2682 | `			return pCur;` |
|       - |  2683 | `		}` |
|     466 |  2684 | `		pCur++;` |
|       4 |  2685 | `	}` |
|      37 |  2686 | `	return pEnd;` |
|     178 |  2687 | `}` |
|      70 |  2688 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2689 | `{` |
|       - |  2690 | `	ph7_match *pMatch;` |
|       - |  2691 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|      75 |  2692 | `	int bHasDefault = 0;` |
|       - |  2693 | `	sxu32 nLine;` |
|       - |  2694 | `	sxi32 rc;` |
|      35 |  2695 | `	SXUNUSED(iCompileFlag);` |
|      75 |  2696 | `	nLine = pGen->pIn->nLine;` |
|      75 |  2697 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|       - |  2698 | `	/* Expect '(' */` |
|      75 |  2699 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2700 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2701 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|     ! 0 |  2702 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|       - |  2703 | `	}` |
|      75 |  2704 | `	pGen->pIn++; /* Jump '(' */` |
|      75 |  2705 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|      75 |  2706 | `	if( pSubjEnd >= pGen->pEnd ){` |
|     ! 0 |  2707 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2708 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       - |  2709 | `	}` |
|      75 |  2710 | `	if( pGen->pIn >= pSubjEnd ){` |
|     ! 0 |  2711 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2712 | `			"syntax error, unexpected \")\", expecting match subject");` |
|       - |  2713 | `	}` |
|       - |  2714 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|      75 |  2715 | `	pSavedEnd = pGen->pEnd;` |
|      75 |  2716 | `	pGen->pEnd = pSubjEnd;` |
|      75 |  2717 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      75 |  2718 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2719 | `		return SXERR_ABORT;` |
|       - |  2720 | `	}` |
|      75 |  2721 | `	pGen->pEnd = pSavedEnd;` |
|      75 |  2722 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|       - |  2723 | `	/* Expect '{' */` |
|      75 |  2724 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  2725 | `		return GenStateMatchError(pGen,` |
|     ! 0 |  2726 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  2727 | `			"syntax error, expecting \"{\" after match subject");` |
|       - |  2728 | `	}` |
|      75 |  2729 | `	pGen->pIn++; /* Jump '{' */` |
|      75 |  2730 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|      75 |  2731 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  2732 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2733 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|       - |  2734 | `	}` |
|       - |  2735 | `	/* Allocate ph7_match container */` |
|      75 |  2736 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|      75 |  2737 | `	if( pMatch == 0 ){` |
|     ! 0 |  2738 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2739 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2740 | `		return SXERR_ABORT;` |
|       - |  2741 | `	}` |
|      75 |  2742 | `	SyZero(pMatch,sizeof(ph7_match));` |
|      75 |  2743 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|       - |  2744 | `	/* Iterate arms */` |
|     253 |  2745 | `	while( pGen->pIn < pBodyEnd ){` |
|       - |  2746 | `		ph7_match_arm sArm;` |
|       - |  2747 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|     186 |  2748 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|     186 |  2749 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|     186 |  2750 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|     186 |  2751 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  2752 | `		/* 'default' arm? */` |
|     182 |  2753 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     105 |  2754 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|      22 |  2755 | `			if( bHasDefault ){` |
|       3 |  2756 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|       - |  2757 | `					"Match expressions may only contain one default arm");` |
|       4 |  2758 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  2759 | `			}` |
|      20 |  2760 | `			sArm.bDefault = 1;` |
|      20 |  2761 | `			bHasDefault = 1;` |
|      20 |  2762 | `			pGen->pIn++;` |
|      20 |  2763 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|     ! 0 |  2764 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2765 | `					"syntax error, expecting \"=>\" after 'default'");` |
|       - |  2766 | `			}` |
|      20 |  2767 | `			pGen->pIn++; /* Jump '=>' */` |
|      11 |  2768 | `		}else{` |
|       - |  2769 | `			/* Condition list: cond (',' cond)* '=>' */` |
|     166 |  2770 | `			pCondStart = pGen->pIn;` |
|     166 |  2771 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|       - |  2772 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|     174 |  2773 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|       - |  2774 | `				SySet sCondBc;` |
|       9 |  2775 | `				if( pCondStart >= pArrow ){` |
|     ! 0 |  2776 | `					return GenStateMatchError(pGen,nArmLine,` |
|       - |  2777 | `						"syntax error, empty match condition expression");` |
|       - |  2778 | `				}` |
|       9 |  2779 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       9 |  2780 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       9 |  2781 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2782 | `					return SXERR_ABORT;` |
|       - |  2783 | `				}` |
|       9 |  2784 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       9 |  2785 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|       9 |  2786 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|       - |  2787 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       1 |  2788 | `			}` |
|     166 |  2789 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2790 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2791 | `					"syntax error, expecting \"=>\" in match arm");` |
|       - |  2792 | `			}` |
|     163 |  2793 | `			if( pCondStart >= pArrow ){` |
|     ! 0 |  2794 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2795 | `					"syntax error, empty match condition expression");` |
|       - |  2796 | `			}` |
|       - |  2797 | `			{` |
|       - |  2798 | `				SySet sCondBc;` |
|     163 |  2799 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     163 |  2800 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|     163 |  2801 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2802 | `					return SXERR_ABORT;` |
|       - |  2803 | `				}` |
|     163 |  2804 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       - |  2805 | `			}` |
|     163 |  2806 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|       - |  2807 | `		}` |
|       - |  2808 | `		/* Compile result expression: up to top-level ',' or body end */` |
|     181 |  2809 | `		pResStart = pGen->pIn;` |
|     181 |  2810 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|     181 |  2811 | `		if( pResStart >= pResEnd ){` |
|     ! 0 |  2812 | `			return GenStateMatchError(pGen,nArmLine,` |
|       - |  2813 | `				"syntax error, expected expression after \"=>\"");` |
|       - |  2814 | `		}` |
|     181 |  2815 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|     181 |  2816 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2817 | `			return SXERR_ABORT;` |
|       - |  2818 | `		}` |
|     181 |  2819 | `		pGen->pIn = pResEnd;` |
|     181 |  2820 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|     149 |  2821 | `			pGen->pIn++; /* Skip trailing ',' */` |
|      73 |  2822 | `		}` |
|     181 |  2823 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|       3 |  2824 | `	}` |
|      69 |  2825 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|      69 |  2826 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|      69 |  2827 | `	return SXRET_OK;` |
|      40 |  2828 | `}` |
|       - |  2829 | `/*` |
|       - |  2830 | ` * Compile a backtick quoted string.` |
|       - |  2831 | ` */` |
|       4 |  2832 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2833 | `{` |
|       - |  2834 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - |  2835 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - |  2836 | `	 */` |
|       8 |  2837 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - |  2838 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 |  2839 | `		ph7_lib_version()` |
|       - |  2840 | `		);` |
|       - |  2841 | `	/* Load NULL */` |
|       6 |  2842 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2843 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2844 | `	/* Node successfully compiled */` |
|       6 |  2845 | `	return SXRET_OK;` |
|       2 |  2846 | `}` |
|       - |  2847 | `/*` |
|       - |  2848 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - |  2849 | ` * construct.` |
|       - |  2850 | ` */` |
|      82 |  2851 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2852 | `{` |
|       - |  2853 | `	SyString *pName;` |
|       - |  2854 | `	sxu32 nKeyID;` |
|       - |  2855 | `	sxi32 rc;` |
|       - |  2856 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      87 |  2857 | `	pName = &pGen->pIn->sData;` |
|      87 |  2858 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      87 |  2859 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      87 |  2860 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 |  2861 | `		SyToken *pTmp,*pNext = 0;` |
|       - |  2862 | `		/* Compile arguments one after one */` |
|       9 |  2863 | `		pTmp = pGen->pEnd;` |
|       - |  2864 | `		/* Symisc eXtension to the PHP programming language:` |
|       - |  2865 | `		 * 'echo' can be used in the context of a function which` |
|       - |  2866 | `		 *  mean that the following expression is valid:` |
|       - |  2867 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - |  2868 | `		 */` |
|       9 |  2869 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 |  2870 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 |  2871 | `			if( pGen->pIn < pNext ){` |
|       9 |  2872 | `				pGen->pEnd = pNext;` |
|       9 |  2873 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 |  2874 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2875 | `					return SXERR_ABORT;` |
|       - |  2876 | `				}` |
|       9 |  2877 | `				if( rc != SXERR_EMPTY ){` |
|       - |  2878 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - |  2879 | `					 * without the overhead of a function call.` |
|       - |  2880 | `					 * This is a very powerful optimization that improve` |
|       - |  2881 | `					 * performance greatly.` |
|       - |  2882 | `					 */` |
|       9 |  2883 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 |  2884 | `				}` |
|       4 |  2885 | `			}` |
|       - |  2886 | `			/* Jump trailing commas */` |
|       9 |  2887 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 |  2888 | `				pNext++;` |
|     ! 0 |  2889 | `			}` |
|       9 |  2890 | `			pGen->pIn = pNext;` |
|       1 |  2891 | `		}` |
|       - |  2892 | `		/* Restore token stream */` |
|       9 |  2893 | `		pGen->pEnd = pTmp;` |
|       5 |  2894 | `	}else{` |
|      79 |  2895 | `		sxi32 nArg = 0;` |
|      79 |  2896 | `		sxu32 nIdx = 0;` |
|      79 |  2897 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      79 |  2898 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2899 | `			return SXERR_ABORT;` |
|      79 |  2900 | `		}else if(rc != SXERR_EMPTY ){` |
|      79 |  2901 | `			nArg = 1;` |
|      37 |  2902 | `		}` |
|      79 |  2903 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - |  2904 | `			ph7_value *pObj;` |
|       - |  2905 | `			/* Emit the call instruction */` |
|      31 |  2906 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      31 |  2907 | `			if( pObj == 0 ){` |
|     ! 0 |  2908 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2909 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2910 | `				return SXERR_ABORT;` |
|       - |  2911 | `			}` |
|      31 |  2912 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - |  2913 | `			/* Install in the literal table */` |
|      31 |  2914 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      13 |  2915 | `		}` |
|       - |  2916 | `		/* Emit the call instruction */` |
|      79 |  2917 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      79 |  2918 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       - |  2919 | `	}` |
|       - |  2920 | `	/* Node successfully compiled */` |
|      87 |  2921 | `	return SXRET_OK;` |
|      46 |  2922 | `}` |
|       - |  2923 | `/*` |
|       - |  2924 | ` * Compile a node holding a variable declaration.` |
|       - |  2925 | ` * According to the PHP language reference` |
|       - |  2926 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - |  2927 | ` *  The variable name is case-sensitive.` |
|       - |  2928 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - |  2929 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  2930 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - |  2931 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - |  2932 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - |  2933 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - |  2934 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - |  2935 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - |  2936 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - |  2937 | ` *  the chapter on Expressions.` |
|       - |  2938 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - |  2939 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - |  2940 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - |  2941 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - |  2942 | ` *  is being assigned (the source variable).` |
|       - |  2943 | ` */` |
| 1180178 |  2944 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2945 | `{` |
| 1180183 |  2946 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  2947 | `	sxi32 iVv;` |
|       - |  2948 | `	sxi32 iP1;` |
|       - |  2949 | `	void *p3;` |
|       - |  2950 | `	sxi32 rc;` |
| 1180183 |  2951 | `	iVv = -1; /* Variable variable counter */` |
| 2360373 |  2952 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
| 1180195 |  2953 | `		pGen->pIn++;` |
| 1180195 |  2954 | `		iVv++;` |
|       5 |  2955 | `	}` |
| 1180183 |  2956 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - |  2957 | `		/* Invalid variable name */` |
|     ! 0 |  2958 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 |  2959 | `		if( rc == SXERR_ABORT ){` |
|       - |  2960 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2961 | `			return SXERR_ABORT;` |
|       - |  2962 | `		}` |
|     ! 0 |  2963 | `		return SXRET_OK;` |
|       - |  2964 | `	}` |
| 1180183 |  2965 | `	p3  = 0;` |
| 1180183 |  2966 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - |  2967 | `		/* Dynamic variable creation */` |
|      19 |  2968 | `		pGen->pIn++;  /* Jump the open curly */` |
|      19 |  2969 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      19 |  2970 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  2971 | `			/* Empty expression */` |
|       3 |  2972 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 |  2973 | `			return SXRET_OK;` |
|       - |  2974 | `		}` |
|       - |  2975 | `		/* Compile the expression holding the variable name */` |
|      16 |  2976 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 |  2977 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2978 | `			return SXERR_ABORT;` |
|      16 |  2979 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 |  2980 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 |  2981 | `			return SXRET_OK;` |
|       - |  2982 | `		}` |
|       7 |  2983 | `	}else{` |
|       - |  2984 | `		SyHashEntry *pEntry;` |
|       - |  2985 | `		SyString *pName;` |
| 1180167 |  2986 | `		char *zName = 0;` |
|       - |  2987 | `		/* Extract variable name */` |
| 1180167 |  2988 | `		pName = &pGen->pIn->sData;` |
|       - |  2989 | `		/* Advance the stream cursor */` |
| 1180167 |  2990 | `		pGen->pIn++;` |
| 1180167 |  2991 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
| 1180167 |  2992 | `		if( pEntry == 0 ){` |
|       - |  2993 | `			/* Duplicate name */` |
|  169891 |  2994 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  169891 |  2995 | `			if( zName == 0 ){` |
|     ! 0 |  2996 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2997 | `				return SXERR_ABORT;` |
|       - |  2998 | `			}` |
|       - |  2999 | `			/* Install in the hashtable */` |
|  169891 |  3000 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   84948 |  3001 | `		}else{` |
|       - |  3002 | `			/* Name already available */` |
| 1010281 |  3003 | `			zName = (char *)pEntry->pUserData;` |
|       - |  3004 | `		}` |
| 1180167 |  3005 | `		p3 = (void *)zName;` |
|       - |  3006 | `	}` |
| 1180179 |  3007 | `	iP1 = 0;` |
| 1180179 |  3008 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  460351 |  3009 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - |  3010 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  460333 |  3011 | `			iP1 = 1;` |
|  230164 |  3012 | `		}` |
|  230173 |  3013 | `	}` |
|       - |  3014 | `	/* Emit the load instruction */` |
| 1180179 |  3015 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
| 1180191 |  3016 | `	while( iVv > 0 ){` |
|      13 |  3017 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 |  3018 | `		iVv--;` |
|       1 |  3019 | `	}` |
|       - |  3020 | `	/* Node successfully compiled */` |
| 1180179 |  3021 | `	return SXRET_OK;` |
|  590094 |  3022 | `}` |
|       - |  3023 | `/*` |
|       - |  3024 | ` * Load a literal.` |
|       - |  3025 | ` */` |
|  814476 |  3026 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       5 |  3027 | `{` |
|  814481 |  3028 | `	SyToken *pToken = pGen->pIn;` |
|       - |  3029 | `	ph7_value *pObj;` |
|       - |  3030 | `	SyString *pStr;` |
|       - |  3031 | `	sxu32 nIdx;` |
|       - |  3032 | `	/* Extract token value */` |
|  814481 |  3033 | `	pStr = &pToken->sData;` |
|       - |  3034 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  814481 |  3035 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  172579 |  3036 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - |  3037 | `			/* NULL constant are always indexed at 0 */` |
|   63477 |  3038 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   63477 |  3039 | `			return SXRET_OK;` |
|  109107 |  3040 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - |  3041 | `			/* TRUE constant are always indexed at 1 */` |
|     833 |  3042 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     833 |  3043 | `			return SXRET_OK;` |
|       5 |  3044 | `		}` |
|  751184 |  3045 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  110280 |  3046 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - |  3047 | `			/* FALSE constant are always indexed at 2 */` |
|   48669 |  3048 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   48669 |  3049 | `			return SXRET_OK;` |
|  651034 |  3050 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|  115582 |  3051 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - |  3052 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|   11081 |  3053 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   11081 |  3054 | `			if( pObj == 0 ){` |
|     ! 0 |  3055 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3056 | `				return SXERR_ABORT;` |
|       - |  3057 | `			}` |
|   11081 |  3058 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - |  3059 | `			/* Emit the load constant instruction */` |
|   11081 |  3060 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   11081 |  3061 | `			return SXRET_OK;` |
|  600836 |  3062 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   37338 |  3063 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - |  3064 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       7 |  3065 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       7 |  3066 | `			if( pObj == 0 ){` |
|     ! 0 |  3067 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3068 | `				return SXERR_ABORT;` |
|       - |  3069 | `			}` |
|       7 |  3070 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - |  3071 | `				SyString sNs;` |
|       7 |  3072 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       7 |  3073 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       4 |  3074 | `			}else{` |
|     ! 0 |  3075 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  3076 | `			}` |
|       7 |  3077 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       7 |  3078 | `			return SXRET_OK;` |
|  589980 |  3079 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   25737 |  3080 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  592233 |  3081 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   20168 |  3082 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 |  3083 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - |  3084 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 |  3085 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - |  3086 | `				/* Point to the upper block */` |
|      11 |  3087 | `				pBlock = pBlock->pParent;` |
|       1 |  3088 | `			}` |
|      11 |  3089 | `			if( pBlock == 0 ){` |
|       - |  3090 | `				/* Called in the global scope,load NULL */` |
|       5 |  3091 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 |  3092 | `			}else{` |
|       - |  3093 | `				/* Extract the target function/method */` |
|       7 |  3094 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 |  3095 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - |  3096 | `					/* Not a class method,Load null */` |
|       3 |  3097 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  3098 | `				}else{` |
|       5 |  3099 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 |  3100 | `					if( pObj == 0 ){` |
|     ! 0 |  3101 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3102 | `						return SXERR_ABORT;` |
|       - |  3103 | `					}` |
|       5 |  3104 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - |  3105 | `					/* Emit the load constant instruction */` |
|       5 |  3106 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  3107 | `				}` |
|       - |  3108 | `			}` |
|      11 |  3109 | `			return SXRET_OK;` |
|       - |  3110 | `	}` |
|       - |  3111 | `	/* Query literal table */` |
|  690425 |  3112 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - |  3113 | `		ph7_value *pLitObj;` |
|       - |  3114 | `		/* Unknown literal,install it in the literal table */` |
|  294097 |  3115 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  294097 |  3116 | `		if( pLitObj == 0 ){` |
|     ! 0 |  3117 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3118 | `			return SXERR_ABORT;` |
|       - |  3119 | `		}` |
|  294097 |  3120 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  294097 |  3121 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  147046 |  3122 | `	}` |
|       - |  3123 | `	/* Emit the load constant instruction */` |
|  690425 |  3124 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  690425 |  3125 | `	return SXRET_OK;` |
|  407243 |  3126 | `}` |
|       - |  3127 | `/*` |
|       - |  3128 | ` * Resolve a namespace path or simply load a literal.` |
|       - |  3129 | ` * If the token stream contains namespace separators (backslashes),` |
|       - |  3130 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - |  3131 | ` * Otherwise, load the simple literal directly.` |
|       - |  3132 | ` */` |
|  818214 |  3133 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       5 |  3134 | `{` |
|       - |  3135 | `	sxi32 rc;` |
|  818219 |  3136 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3137 | `		return SXRET_OK;` |
|       - |  3138 | `	}` |
|       - |  3139 | `	/* Check if this is a multi-token namespace path */` |
|  818219 |  3140 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - |  3141 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|    3743 |  3142 | `		SyBlob *pWorker = &pGen->sWorker;` |
|    3743 |  3143 | `		int isAbsolute = 0;` |
|    3743 |  3144 | `		SyBlobReset(pWorker);` |
|       - |  3145 | `		/* Check for leading backslash (absolute path) */` |
|    3743 |  3146 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|    3741 |  3147 | `			isAbsolute = 1;` |
|    3741 |  3148 | `			pGen->pIn++; /* Skip leading backslash */` |
|    1868 |  3149 | `		}` |
|       - |  3150 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|    3743 |  3151 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 |  3152 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 |  3153 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 |  3154 | `		}` |
|       - |  3155 | `		/* Collect all path components */` |
|    3851 |  3156 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|    3851 |  3157 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      59 |  3158 | `				SyBlobAppend(pWorker,"\\",1);` |
|      32 |  3159 | `			}else{` |
|    3797 |  3160 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  3161 | `			}` |
|    3851 |  3162 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|    3743 |  3163 | `				pGen->pIn++;` |
|    3743 |  3164 | `				break;` |
|       - |  3165 | `			}` |
|     113 |  3166 | `			pGen->pIn++;` |
|       5 |  3167 | `		}` |
|    3743 |  3168 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - |  3169 | `			ph7_value *pObj;` |
|       - |  3170 | `			SyString sPath;` |
|       - |  3171 | `			sxu32 nIdx;` |
|    3743 |  3172 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - |  3173 | `			/* Install in the literal table */` |
|    3743 |  3174 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|    3715 |  3175 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    3715 |  3176 | `				if( pObj == 0 ){` |
|     ! 0 |  3177 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3178 | `					return SXERR_ABORT;` |
|       - |  3179 | `				}` |
|    3715 |  3180 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|    3715 |  3181 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|    1855 |  3182 | `			}` |
|       - |  3183 | `			/* Emit the load constant instruction.` |
|       - |  3184 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|       - |  3185 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|    5612 |  3186 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|    1869 |  3187 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|    1869 |  3188 | `				nIdx,0,0);` |
|    3743 |  3189 | `			return SXRET_OK;` |
|       - |  3190 | `		}` |
|     ! 0 |  3191 | `	}` |
|       - |  3192 | `	/* Single-token literal: load directly */` |
|  814481 |  3193 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  814481 |  3194 | `	return rc;` |
|  409112 |  3195 | `}` |
|       - |  3196 | `/*` |
|       - |  3197 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - |  3198 | ` */` |
|       - |  3199 | `/*` |
|       - |  3200 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|       - |  3201 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|       - |  3202 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|       - |  3203 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|       - |  3204 | ` */` |
|     ! 0 |  3205 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|     ! 0 |  3206 | `{` |
|     ! 0 |  3207 | `	SXUNUSED(iCompileFlag);` |
|     ! 0 |  3208 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|       - |  3209 | `		"Cannot use the first-class callable syntax '...' here");` |
|     ! 0 |  3210 | `	return SXERR_SYNTAX;` |
|     ! 0 |  3211 | `}` |
|  818214 |  3212 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  3213 | `{` |
|       - |  3214 | `	sxi32 rc;` |
|  818219 |  3215 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  818219 |  3216 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3217 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  3218 | `		return rc;` |
|       - |  3219 | `	}` |
|       - |  3220 | `	/* Node successfully compiled */` |
|  818219 |  3221 | `	return SXRET_OK;` |
|  409112 |  3222 | `}` |
|       - |  3223 | `/*` |
|       - |  3224 | ` * Recover from a compile-time error. In other words synchronize` |
|       - |  3225 | ` * the token stream cursor with the first semi-colon seen.` |
|       - |  3226 | ` */` |
|       8 |  3227 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 |  3228 | `{` |
|       - |  3229 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 |  3230 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 |  3231 | `		pGen->pIn++;` |
|       1 |  3232 | `	}` |
|       9 |  3233 | `	return SXRET_OK;` |
|       1 |  3234 | `}` |
|       - |  3235 | `/*` |
|       - |  3236 | ` * Check if the given identifier name is reserved or not.` |
|       - |  3237 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - |  3238 | ` */` |
|     128 |  3239 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       5 |  3240 | `{` |
|     133 |  3241 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      30 |  3242 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 |  3243 | `			return TRUE;` |
|      28 |  3244 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       6 |  3245 | `			return TRUE;` |
|       2 |  3246 | `		}` |
|     117 |  3247 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 |  3248 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 |  3249 | `			return TRUE;` |
|       - |  3250 | `		}` |
|     ! 0 |  3251 | `	}` |
|       - |  3252 | `	/* Not a reserved constant */` |
|     125 |  3253 | `	return FALSE;` |
|      69 |  3254 | `}` |
|       - |  3255 | `/*` |
|       - |  3256 | ` * Compile the 'const' statement.` |
|       - |  3257 | ` * According to the PHP language reference` |
|       - |  3258 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - |  3259 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - |  3260 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - |  3261 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - |  3262 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  3263 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - |  3264 | ` *  Syntax` |
|       - |  3265 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - |  3266 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - |  3267 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - |  3268 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - |  3269 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - |  3270 | ` *  to get a list of all defined constants.` |
|       - |  3271 | ` *` |
|       - |  3272 | ` * Symisc eXtension.` |
|       - |  3273 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - |  3274 | ` *  would allow only simple scalar value.` |
|       - |  3275 | ` *  Example` |
|       - |  3276 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  3277 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  3278 | ` */` |
|      38 |  3279 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       5 |  3280 | `{` |
|       - |  3281 | `	SySet *pConsCode,*pInstrContainer;` |
|      43 |  3282 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  3283 | `	SyString *pName;` |
|       - |  3284 | `	sxi32 rc;` |
|      43 |  3285 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      43 |  3286 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  3287 | `		/* Invalid constant name */` |
|       9 |  3288 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       9 |  3289 | `		if( rc == SXERR_ABORT ){` |
|       - |  3290 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3291 | `			return SXERR_ABORT;` |
|       - |  3292 | `		}` |
|       9 |  3293 | `		goto Synchronize;` |
|       - |  3294 | `	}` |
|       - |  3295 | `	/* Peek constant name */` |
|      37 |  3296 | `	pName = &pGen->pIn->sData;` |
|       - |  3297 | `	/* Make sure the constant name isn't reserved */` |
|      37 |  3298 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  3299 | `		/* Reserved constant */` |
|      10 |  3300 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|      10 |  3301 | `		if( rc == SXERR_ABORT ){` |
|       - |  3302 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3303 | `			return SXERR_ABORT;` |
|       - |  3304 | `		}` |
|      10 |  3305 | `		goto Synchronize;` |
|       - |  3306 | `	}` |
|      28 |  3307 | `	pGen->pIn++;` |
|      28 |  3308 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  3309 | `		/* Invalid statement*/` |
|       6 |  3310 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       6 |  3311 | `		if( rc == SXERR_ABORT ){` |
|       - |  3312 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3313 | `			return SXERR_ABORT;` |
|       - |  3314 | `		}` |
|       6 |  3315 | `		goto Synchronize;` |
|       - |  3316 | `	}` |
|      22 |  3317 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - |  3318 | `	/* Allocate a new constant value container */` |
|      22 |  3319 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      22 |  3320 | `	if( pConsCode == 0 ){` |
|     ! 0 |  3321 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3322 | `		return SXERR_ABORT;` |
|       - |  3323 | `	}` |
|      22 |  3324 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  3325 | `	/* Swap bytecode container */` |
|      22 |  3326 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      22 |  3327 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - |  3328 | `	/* Compile constant value */` |
|      22 |  3329 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3330 | `	/* Emit the done instruction */` |
|      22 |  3331 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      22 |  3332 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      22 |  3333 | `	if( rc == SXERR_ABORT ){` |
|       - |  3334 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  3335 | `		return SXERR_ABORT;` |
|       - |  3336 | `	}` |
|      22 |  3337 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - |  3338 | `	/* Register the constant with namespace-qualified name */` |
|       - |  3339 | `	{` |
|       - |  3340 | `		SyBlob sFQN;` |
|       - |  3341 | `		SyString sFQNStr;` |
|      22 |  3342 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      22 |  3343 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      22 |  3344 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      22 |  3345 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      22 |  3346 | `		SyBlobRelease(&sFQN);` |
|       - |  3347 | `	}` |
|      22 |  3348 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3349 | `		SySetRelease(pConsCode);` |
|     ! 0 |  3350 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 |  3351 | `	}` |
|      22 |  3352 | `	return SXRET_OK;` |
|       9 |  3353 | `Synchronize:` |
|       - |  3354 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      60 |  3355 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      41 |  3356 | `		pGen->pIn++;` |
|       3 |  3357 | `	}` |
|      22 |  3358 | `	return SXRET_OK;` |
|      24 |  3359 | `}` |
|       - |  3360 | `/*` |
|       - |  3361 | ` * Compile the 'continue' statement.` |
|       - |  3362 | ` * According to the PHP language reference` |
|       - |  3363 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - |  3364 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - |  3365 | ` *  iteration.` |
|       - |  3366 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - |  3367 | ` *  the purposes of continue.` |
|       - |  3368 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - |  3369 | ` *  of enclosing loops it should skip to the end of.` |
|       - |  3370 | ` *  Note:` |
|       - |  3371 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - |  3372 | ` */` |
|       - |  3373 | `/*` |
|       - |  3374 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - |  3375 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - |  3376 | ` * break/continue crosses a try boundary.` |
|       - |  3377 | ` *` |
|       - |  3378 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - |  3379 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - |  3380 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - |  3381 | ` */` |
|    3832 |  3382 | `static int GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       5 |  3383 | `{` |
|    3837 |  3384 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    3837 |  3385 | `	int nInlineTry = 0;` |
|   22503 |  3386 | `	while( pBlock && pBlock != pTarget ){` |
|   18671 |  3387 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       6 |  3388 | `			if( pBlock->pUserData ){` |
|       - |  3389 | `				/* A try block with an exception context. In a generator its catch/finally` |
|       - |  3390 | `				 * are inlined: count it so the caller emits a single OP_SET_FINALLY_JMP that` |
|       - |  3391 | `				 * runs each crossed finally (VmFinallyAdvance) before taking the loop jump.` |
|       - |  3392 | `				 * Legacy path: emit POP_EXCEPTION per crossed try as before. */` |
|       6 |  3393 | `				if( pGen->bInGenerator ){` |
|       3 |  3394 | `					nInlineTry++;` |
|       2 |  3395 | `				}else{` |
|       3 |  3396 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       - |  3397 | `				}` |
|       4 |  3398 | `			}else{` |
|       - |  3399 | `				/* A catch/finally block compiled into a separate bytecode container` |
|       - |  3400 | `				 * (legacy). Stop — cannot cross into the parent try from a sub-execution. */` |
|     ! 0 |  3401 | `				break;` |
|       - |  3402 | `			}` |
|       2 |  3403 | `		}` |
|   18671 |  3404 | `		pBlock = pBlock->pParent;` |
|       5 |  3405 | `	}` |
|    3837 |  3406 | `	return nInlineTry;` |
|       5 |  3407 | `}` |
|    3734 |  3408 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       5 |  3409 | `{` |
|       - |  3410 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3411 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3412 | `	sxu32 nLineLocal;` |
|       - |  3413 | `	sxi32 rc;` |
|    3739 |  3414 | `	nLineLocal = pGen->pIn->nLine;` |
|    3739 |  3415 | `	iLevel = 0;` |
|       - |  3416 | `	/* Jump the 'continue' keyword */` |
|    3739 |  3417 | `	pGen->pIn++;` |
|    3739 |  3418 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3419 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3420 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3421 | `		 */` |
|       - |  3422 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      17 |  3423 | `		char *zAlloc = 0;` |
|       - |  3424 | `		SyString sNum;` |
|      17 |  3425 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      17 |  3426 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3427 | `			return SXERR_ABORT;` |
|       - |  3428 | `		}` |
|      17 |  3429 | `		if( rc == SXRET_OK ){` |
|      20 |  3430 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3431 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3432 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3433 | `				return SXERR_ABORT;` |
|       - |  3434 | `			}` |
|      14 |  3435 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3436 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3437 | `		}` |
|      17 |  3438 | `		if( iLevel < 2 ){` |
|       3 |  3439 | `			iLevel = 0;` |
|       1 |  3440 | `		}` |
|      17 |  3441 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3442 | `	}` |
|       - |  3443 | `	/* Point to the target loop */` |
|    3739 |  3444 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    3739 |  3445 | `	if( pLoop == 0 ){` |
|       - |  3446 | `		/* Illegal continue */` |
|      13 |  3447 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      13 |  3448 | `		if( rc == SXERR_ABORT ){` |
|       - |  3449 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3450 | `			return SXERR_ABORT;` |
|       - |  3451 | `		}` |
|       8 |  3452 | `	}else{` |
|    3729 |  3453 | `		sxu32 nInstrIdx = 0;` |
|       - |  3454 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|    3729 |  3455 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|       - |  3456 | `		/* ROOT C: in a generator, a break/continue crossing inline trys must run their` |
|       - |  3457 | `		 * finallys first. OP_SET_FINALLY_JMP(iP1=count) does that then takes the loop jump. */` |
|    3729 |  3458 | `		sxi32 iJmpOp = nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP;` |
|    3729 |  3459 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - |  3460 | `			/* According to the PHP language reference manual` |
|       - |  3461 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - |  3462 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - |  3463 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - |  3464 | `			 */` |
|       5 |  3465 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,0,0,&nInstrIdx);` |
|       5 |  3466 | `			if( rc == SXRET_OK ){` |
|       5 |  3467 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 |  3468 | `			}` |
|       3 |  3469 | `		}else{` |
|       - |  3470 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    3725 |  3471 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,nCross,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    3725 |  3472 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  3473 | `				JumpFixup sJumpFix;` |
|       - |  3474 | `				/* Post-continue */` |
|      14 |  3475 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 |  3476 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 |  3477 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 |  3478 | `			}` |
|       - |  3479 | `		}` |
|       - |  3480 | `	}` |
|    3739 |  3481 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3482 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3483 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  3484 | `	}` |
|       - |  3485 | `	/* Statement successfully compiled */` |
|    3739 |  3486 | `	return SXRET_OK;` |
|    1872 |  3487 | `}` |
|       - |  3488 | `/*` |
|       - |  3489 | ` * Compile the 'break' statement.` |
|       - |  3490 | ` * According to the PHP language reference` |
|       - |  3491 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - |  3492 | ` *  structure.` |
|       - |  3493 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - |  3494 | ` *  enclosing structures are to be broken out of.` |
|       - |  3495 | ` */` |
|     124 |  3496 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       5 |  3497 | `{` |
|       - |  3498 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3499 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3500 | `	sxi32 rc;` |
|     129 |  3501 | `	iLevel = 0;` |
|       - |  3502 | `	/* Jump the 'break' keyword */` |
|     129 |  3503 | `	pGen->pIn++;` |
|     129 |  3504 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3505 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3506 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3507 | `		 */` |
|       - |  3508 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      18 |  3509 | `		char *zAlloc = 0;` |
|       - |  3510 | `		SyString sNum;` |
|      18 |  3511 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      18 |  3512 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3513 | `			return SXERR_ABORT;` |
|       - |  3514 | `		}` |
|      18 |  3515 | `		if( rc == SXRET_OK ){` |
|      21 |  3516 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3517 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      15 |  3518 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3519 | `				return SXERR_ABORT;` |
|       - |  3520 | `			}` |
|      15 |  3521 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      15 |  3522 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3523 | `		}` |
|      18 |  3524 | `		if( iLevel < 2 ){` |
|       3 |  3525 | `			iLevel = 0;` |
|       1 |  3526 | `		}` |
|      18 |  3527 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3528 | `	}` |
|       - |  3529 | `	/* Extract the target loop */` |
|     129 |  3530 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     129 |  3531 | `	if( pLoop == 0 ){` |
|       - |  3532 | `		/* Illegal break */` |
|      19 |  3533 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      19 |  3534 | `		if( rc == SXERR_ABORT ){` |
|       - |  3535 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3536 | `			return SXERR_ABORT;` |
|       - |  3537 | `		}` |
|      11 |  3538 | `	}else{` |
|       - |  3539 | `		sxu32 nInstrIdx;` |
|       - |  3540 | `		/* Emit POP_EXCEPTION (legacy) for crossed try blocks, or count them (generator). */` |
|     113 |  3541 | `		int nCross = GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|       - |  3542 | `		/* ROOT C: OP_SET_FINALLY_JMP runs the crossed inline finallys before the break jump. */` |
|     113 |  3543 | `		rc = PH7_VmEmitInstr(pGen->pVm,nCross > 0 ? PH7_OP_SET_FINALLY_JMP : PH7_OP_JMP,nCross,0,0,&nInstrIdx);` |
|     113 |  3544 | `		if( rc == SXRET_OK ){` |
|       - |  3545 | `			/* Fix the jump later when the jump destination is resolved */` |
|     113 |  3546 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      54 |  3547 | `		}` |
|       - |  3548 | `	}` |
|     129 |  3549 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3550 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3551 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 |  3552 | `	}` |
|       - |  3553 | `	/* Statement successfully compiled */` |
|     129 |  3554 | `	return SXRET_OK;` |
|      67 |  3555 | `}` |
|       - |  3556 | `/*` |
|       - |  3557 | ` * Compile or record a label.` |
|       - |  3558 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - |  3559 | ` * Example` |
|       - |  3560 | ` *  goto LABEL;` |
|       - |  3561 | ` *   echo 'Foo';` |
|       - |  3562 | ` *  LABEL:` |
|       - |  3563 | ` *   echo 'Bar';` |
|       - |  3564 | ` */` |
|     112 |  3565 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       5 |  3566 | `{` |
|       - |  3567 | `	GenBlock *pBlock;` |
|       - |  3568 | `	Label sLabel;` |
|       - |  3569 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     117 |  3570 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     117 |  3571 | `	if( pBlock ){` |
|       - |  3572 | `		sxi32 rc;` |
|       8 |  3573 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 |  3574 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       6 |  3575 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3576 | `			return SXERR_ABORT;` |
|       - |  3577 | `		}` |
|       4 |  3578 | `	}else{` |
|     113 |  3579 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3580 | `		char *zDup;` |
|       - |  3581 | `		/* Initialize label fields */` |
|     113 |  3582 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - |  3583 | `		/* Duplicate label name */` |
|     113 |  3584 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     113 |  3585 | `		if( zDup == 0 ){` |
|     ! 0 |  3586 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3587 | `			return SXERR_ABORT;` |
|       - |  3588 | `		}` |
|     113 |  3589 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     113 |  3590 | `		sLabel.bRef  = FALSE;` |
|     113 |  3591 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     113 |  3592 | `		pBlock = pGen->pCurrent;` |
|     221 |  3593 | `		while( pBlock ){` |
|     133 |  3594 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      23 |  3595 | `				break;` |
|       - |  3596 | `			}` |
|       - |  3597 | `			/* Point to the upper block */` |
|     113 |  3598 | `			pBlock = pBlock->pParent;` |
|       5 |  3599 | `		}` |
|     113 |  3600 | `		if( pBlock ){` |
|      23 |  3601 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      13 |  3602 | `		}else{` |
|      93 |  3603 | `			sLabel.pFunc = 0;` |
|       - |  3604 | `		}` |
|       - |  3605 | `		/* Insert in label set */` |
|     113 |  3606 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - |  3607 | `	}` |
|     117 |  3608 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     117 |  3609 | `	return SXRET_OK;` |
|      61 |  3610 | `}` |
|       - |  3611 | `/*` |
|       - |  3612 | ` * Compile the so hated 'goto' statement.` |
|       - |  3613 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - |  3614 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - |  3615 | ` * a compiler it has to do this.` |
|       - |  3616 | ` * According to the PHP language reference manual` |
|       - |  3617 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - |  3618 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - |  3619 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - |  3620 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - |  3621 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - |  3622 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - |  3623 | ` *   of a multi-level break` |
|       - |  3624 | ` */` |
|     152 |  3625 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       5 |  3626 | `{` |
|       - |  3627 | `	JumpFixup sJump;` |
|       - |  3628 | `	sxi32 rc;` |
|     157 |  3629 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     157 |  3630 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  3631 | `		/* Missing label */` |
|     ! 0 |  3632 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 |  3633 | `		if( rc == SXERR_ABORT ){` |
|       - |  3634 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3635 | `			return SXERR_ABORT;` |
|       - |  3636 | `		}` |
|     ! 0 |  3637 | `		return SXRET_OK;` |
|       - |  3638 | `	}` |
|     157 |  3639 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       6 |  3640 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       6 |  3641 | `		if( rc == SXERR_ABORT ){` |
|       - |  3642 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3643 | `			return SXERR_ABORT;` |
|       - |  3644 | `		}` |
|       4 |  3645 | `	}else{` |
|     153 |  3646 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3647 | `		GenBlock *pBlock;` |
|       - |  3648 | `		char *zDup;` |
|       - |  3649 | `		/* Prepare the jump destination */` |
|     153 |  3650 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     153 |  3651 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - |  3652 | `		/* Duplicate label name */` |
|     153 |  3653 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     153 |  3654 | `		if( zDup == 0 ){` |
|     ! 0 |  3655 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3656 | `			return SXERR_ABORT;` |
|       - |  3657 | `		}` |
|     153 |  3658 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     153 |  3659 | `		pBlock = pGen->pCurrent;` |
|     315 |  3660 | `		while( pBlock ){` |
|     199 |  3661 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      37 |  3662 | `				break;` |
|       - |  3663 | `			}` |
|       - |  3664 | `			/* Point to the upper block */` |
|     167 |  3665 | `			pBlock = pBlock->pParent;` |
|       5 |  3666 | `		}` |
|     153 |  3667 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       8 |  3668 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       8 |  3669 | `			if( rc == SXERR_ABORT ){` |
|       - |  3670 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3671 | `				return SXERR_ABORT;` |
|       - |  3672 | `			}` |
|       3 |  3673 | `		}` |
|     153 |  3674 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      30 |  3675 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      17 |  3676 | `		}else{` |
|     127 |  3677 | `			sJump.pFunc = 0;` |
|       - |  3678 | `		}` |
|       - |  3679 | `		/* Emit the unconditional jump */` |
|     153 |  3680 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     153 |  3681 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 |  3682 | `		}` |
|       - |  3683 | `	}` |
|     157 |  3684 | `	pGen->pIn++; /* Jump the label name */` |
|     157 |  3685 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 |  3686 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 |  3687 | `	}` |
|       - |  3688 | `	/* Statement successfully compiled */` |
|     157 |  3689 | `	return SXRET_OK;` |
|      81 |  3690 | `}` |
|       - |  3691 | `/*` |
|       - |  3692 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - |  3693 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - |  3694 | ` * failure.` |
|       - |  3695 | ` */` |
|      20 |  3696 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       1 |  3697 | `{` |
|       - |  3698 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - |  3699 | `	sxu32 nRawObj;` |
|      10 |  3700 | `	sxu32 nObjIdx;` |
|       - |  3701 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - |  3702 | `	 * a PHP block.` |
|       - |  3703 | `	 */` |
|      10 |  3704 | `Consume:` |
|      21 |  3705 | `	nRawObj = nObjIdx = 0;` |
|      21 |  3706 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 |  3707 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 |  3708 | `		if( pRawObj == 0 ){` |
|     ! 0 |  3709 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3710 | `			return SXERR_ABORT;` |
|       - |  3711 | `		}` |
|       - |  3712 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 |  3713 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 |  3714 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 |  3715 | `		++nRawObj;` |
|     ! 0 |  3716 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 |  3717 | `	}` |
|      21 |  3718 | `	if( nRawObj > 0 ){` |
|       - |  3719 | `		/* Emit the consume instruction */` |
|     ! 0 |  3720 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 |  3721 | `	}` |
|      21 |  3722 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 |  3723 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - |  3724 | `		/* Reset the token set */` |
|     ! 0 |  3725 | `		SySetReset(pTokenSet);` |
|       - |  3726 | `		/* Tokenize input */` |
|     ! 0 |  3727 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 |  3728 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - |  3729 | `		/* Point to the fresh token stream */` |
|     ! 0 |  3730 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 |  3731 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - |  3732 | `		/* Advance the stream cursor */` |
|     ! 0 |  3733 | `		pGen->pRawIn++;` |
|       - |  3734 | `		/* TICKET 1433-011 */` |
|     ! 0 |  3735 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - |  3736 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - |  3737 | `			sxi32 rc;` |
|       - |  3738 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 |  3739 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 |  3740 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 |  3741 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 |  3742 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 |  3743 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3744 | `				return SXERR_ABORT;` |
|     ! 0 |  3745 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 |  3746 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  3747 | `			}` |
|     ! 0 |  3748 | `			goto Consume;` |
|       - |  3749 | `		}` |
|     ! 0 |  3750 | `	}else{` |
|       - |  3751 | `		/* No more chunks to process */` |
|      21 |  3752 | `		pGen->pIn = pGen->pEnd;` |
|      21 |  3753 | `		return SXERR_EOF;` |
|       - |  3754 | `	}` |
|     ! 0 |  3755 | `	return SXRET_OK;` |
|      11 |  3756 | `}` |
|       - |  3757 | `/*` |
|       - |  3758 | ` * Compile a PHP block.` |
|       - |  3759 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - |  3760 | ` * optionally delimited by braces {}.` |
|       - |  3761 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  3762 | ` * and this function takes care of generating the appropriate error` |
|       - |  3763 | ` * message.` |
|       - |  3764 | ` */` |
|  451782 |  3765 | `static sxi32 PH7_CompileBlock(` |
|       - |  3766 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  3767 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  3768 | `	)` |
|       5 |  3769 | `{` |
|       - |  3770 | `	sxi32 rc;` |
|       - |  3771 | `	sxu32 nLine;` |
|  451787 |  3772 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  450171 |  3773 | `		nLine = pGen->pIn->nLine;` |
|  450171 |  3774 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  450171 |  3775 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3776 | `			return SXERR_ABORT;` |
|       - |  3777 | `		}` |
|  450171 |  3778 | `		pGen->pIn++;` |
|       - |  3779 | `		/* Compile until we hit the closing braces '}' */` |
|  615172 |  3780 | `		for(;;){` |
| 1230349 |  3781 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      21 |  3782 | `				rc = GenStateNextChunk(&(*pGen));` |
|      21 |  3783 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3784 | `			 	   return SXERR_ABORT;` |
|       - |  3785 | `				}` |
|      21 |  3786 | `				if( rc == SXERR_EOF ){` |
|       - |  3787 | `					/* No more token to process. Missing closing braces */` |
|      21 |  3788 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      21 |  3789 | `					break;` |
|       - |  3790 | `				}` |
|     ! 0 |  3791 | `			}` |
| 1230329 |  3792 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  3793 | `				/* Closing braces found,break immediately*/` |
|  450151 |  3794 | `				pGen->pIn++;` |
|  450151 |  3795 | `				break;` |
|       - |  3796 | `			}` |
|       - |  3797 | `			/* Compile a single statement */` |
|  780183 |  3798 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  780183 |  3799 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3800 | `				return SXERR_ABORT;` |
|       - |  3801 | `			}` |
|       5 |  3802 | `		}` |
|  450171 |  3803 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  226704 |  3804 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 |  3805 | `		pGen->pIn++;` |
|     ! 0 |  3806 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 |  3807 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3808 | `			return SXERR_ABORT;` |
|       - |  3809 | `		}` |
|       - |  3810 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 |  3811 | `		for(;;){` |
|     ! 0 |  3812 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3813 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 |  3814 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3815 | `			 	   return SXERR_ABORT;` |
|       - |  3816 | `				}` |
|     ! 0 |  3817 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - |  3818 | `					/* No more token to process */` |
|     ! 0 |  3819 | `					if( rc == SXERR_EOF ){` |
|     ! 0 |  3820 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - |  3821 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 |  3822 | `					}` |
|     ! 0 |  3823 | `					break;` |
|       - |  3824 | `				}` |
|     ! 0 |  3825 | `			}` |
|     ! 0 |  3826 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  3827 | `				sxi32 nKwrd;` |
|       - |  3828 | `				/* Keyword found */` |
|     ! 0 |  3829 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  3830 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 |  3831 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - |  3832 | `						/* Delimiter keyword found,break */` |
|     ! 0 |  3833 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 |  3834 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 |  3835 | `						}` |
|     ! 0 |  3836 | `						break;` |
|       - |  3837 | `				}` |
|     ! 0 |  3838 | `			}` |
|       - |  3839 | `			/* Compile a single statement */` |
|     ! 0 |  3840 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 |  3841 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3842 | `				return SXERR_ABORT;` |
|       - |  3843 | `			}` |
|     ! 0 |  3844 | `		}` |
|     ! 0 |  3845 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  3846 | `	}else{` |
|       - |  3847 | `		/* Compile a single statement */` |
|    1621 |  3848 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1621 |  3849 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3850 | `			return SXERR_ABORT;` |
|       - |  3851 | `		}` |
|       - |  3852 | `	}` |
|       - |  3853 | `	/* Jump trailing semi-colons ';' */` |
|  451787 |  3854 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3855 | `		pGen->pIn++;` |
|     ! 0 |  3856 | `	}` |
|  451787 |  3857 | `	return SXRET_OK;` |
|  225896 |  3858 | `}` |
|       - |  3859 | `/*` |
|       - |  3860 | ` * Compile the gentle 'while' statement.` |
|       - |  3861 | ` * According to the PHP language reference` |
|       - |  3862 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - |  3863 | ` *  The basic form of a while statement is:` |
|       - |  3864 | ` *  while (expr)` |
|       - |  3865 | ` *   statement` |
|       - |  3866 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - |  3867 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - |  3868 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - |  3869 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - |  3870 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - |  3871 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - |  3872 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - |  3873 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - |  3874 | ` *  while (expr):` |
|       - |  3875 | ` *    statement` |
|       - |  3876 | ` *   endwhile;` |
|       - |  3877 | ` */` |
|   14884 |  3878 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       5 |  3879 | `{` |
|   14889 |  3880 | `	GenBlock *pWhileBlock = 0;` |
|   14889 |  3881 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  3882 | `	sxu32 nFalseJump;` |
|       - |  3883 | `	sxu32 nLine;` |
|       - |  3884 | `	sxi32 rc;` |
|   14889 |  3885 | `	nLine = pGen->pIn->nLine;` |
|       - |  3886 | `	/* Jump the 'while' keyword */` |
|   14889 |  3887 | `	pGen->pIn++;` |
|   14889 |  3888 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3889 | `		/* Syntax error */` |
|     ! 0 |  3890 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3891 | `		if( rc == SXERR_ABORT ){` |
|       - |  3892 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3893 | `			return SXERR_ABORT;` |
|       - |  3894 | `		}` |
|     ! 0 |  3895 | `		goto Synchronize;` |
|       - |  3896 | `	}` |
|       - |  3897 | `	/* Jump the left parenthesis '(' */` |
|   14889 |  3898 | `	pGen->pIn++;` |
|       - |  3899 | `	/* Create the loop block */` |
|   14889 |  3900 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   14889 |  3901 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3902 | `		return SXERR_ABORT;` |
|       - |  3903 | `	}` |
|       - |  3904 | `	/* Delimit the condition */` |
|   14889 |  3905 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   14889 |  3906 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3907 | `		/* Empty expression */` |
|       3 |  3908 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 |  3909 | `		if( rc == SXERR_ABORT ){` |
|       - |  3910 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3911 | `			return SXERR_ABORT;` |
|       - |  3912 | `		}` |
|       1 |  3913 | `	}` |
|       - |  3914 | `	/* Swap token streams */` |
|   14889 |  3915 | `	pTmp = pGen->pEnd;` |
|   14889 |  3916 | `	pGen->pEnd = pEnd;` |
|       - |  3917 | `	/* Compile the expression */` |
|   14889 |  3918 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14889 |  3919 | `	if( rc == SXERR_ABORT ){` |
|       - |  3920 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3921 | `		return SXERR_ABORT;` |
|       - |  3922 | `	}` |
|       - |  3923 | `	/* Update token stream */` |
|   14889 |  3924 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3925 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3926 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3927 | `			return SXERR_ABORT;` |
|       - |  3928 | `		}` |
|     ! 0 |  3929 | `		pGen->pIn++;` |
|     ! 0 |  3930 | `	}` |
|       - |  3931 | `	/* Synchronize pointers */` |
|   14889 |  3932 | `	pGen->pIn  = &pEnd[1];` |
|   14889 |  3933 | `	pGen->pEnd = pTmp;` |
|       - |  3934 | `	/* Emit the false jump */` |
|   14889 |  3935 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3936 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   14889 |  3937 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  3938 | `	/* Compile the loop body */` |
|   14889 |  3939 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   14889 |  3940 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3941 | `		return SXERR_ABORT;` |
|       - |  3942 | `	}` |
|       - |  3943 | `	/* Emit the unconditional jump to the start of the loop */` |
|   14889 |  3944 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  3945 | `	/* Fix all jumps now the destination is resolved */` |
|   14889 |  3946 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3947 | `	/* Release the loop block */` |
|   14889 |  3948 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3949 | `	/* Statement successfully compiled */` |
|   14889 |  3950 | `	return SXRET_OK;` |
|     ! 0 |  3951 | `Synchronize:` |
|       - |  3952 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3953 | `	 * compiling this erroneous block.` |
|       - |  3954 | `	 */` |
|     ! 0 |  3955 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3956 | `		pGen->pIn++;` |
|     ! 0 |  3957 | `	}` |
|     ! 0 |  3958 | `	return SXRET_OK;` |
|    7447 |  3959 | `}` |
|       - |  3960 | `/*` |
|       - |  3961 | ` * Compile the ugly do..while() statement.` |
|       - |  3962 | ` * According to the PHP language reference` |
|       - |  3963 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - |  3964 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - |  3965 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - |  3966 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - |  3967 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - |  3968 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - |  3969 | ` *  would end immediately).` |
|       - |  3970 | ` *  There is just one syntax for do-while loops:` |
|       - |  3971 | ` *  <?php` |
|       - |  3972 | ` *  $i = 0;` |
|       - |  3973 | ` *  do {` |
|       - |  3974 | ` *   echo $i;` |
|       - |  3975 | ` *  } while ($i > 0);` |
|       - |  3976 | ` * ?>` |
|       - |  3977 | ` */` |
|       2 |  3978 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 |  3979 | `{` |
|       3 |  3980 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 |  3981 | `	GenBlock *pDoBlock = 0;` |
|       - |  3982 | `	sxu32 nLine;` |
|       - |  3983 | `	sxi32 rc;` |
|       3 |  3984 | `	nLine = pGen->pIn->nLine;` |
|       - |  3985 | `	/* Jump the 'do' keyword */` |
|       3 |  3986 | `	pGen->pIn++;` |
|       - |  3987 | `	/* Create the loop block */` |
|       3 |  3988 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 |  3989 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3990 | `		return SXERR_ABORT;` |
|       - |  3991 | `	}` |
|       - |  3992 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 |  3993 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 |  3994 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 |  3995 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3996 | `		return SXERR_ABORT;` |
|       - |  3997 | `	}` |
|       3 |  3998 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3999 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 |  4000 | `	}` |
|       3 |  4001 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 |  4002 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - |  4003 | `			/* Missing 'while' statement */` |
|       3 |  4004 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 |  4005 | `			if( rc == SXERR_ABORT ){` |
|       - |  4006 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4007 | `				return SXERR_ABORT;` |
|       - |  4008 | `			}` |
|       3 |  4009 | `			goto Synchronize;` |
|       - |  4010 | `	}` |
|       - |  4011 | `	/* Jump the 'while' keyword */` |
|     ! 0 |  4012 | `	pGen->pIn++;` |
|     ! 0 |  4013 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4014 | `		/* Syntax error */` |
|     ! 0 |  4015 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  4016 | `		if( rc == SXERR_ABORT ){` |
|       - |  4017 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4018 | `			return SXERR_ABORT;` |
|       - |  4019 | `		}` |
|     ! 0 |  4020 | `		goto Synchronize;` |
|       - |  4021 | `	}` |
|       - |  4022 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 |  4023 | `	pGen->pIn++;` |
|       - |  4024 | `	/* Delimit the condition */` |
|     ! 0 |  4025 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 |  4026 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4027 | `		/* Empty expression */` |
|     ! 0 |  4028 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 |  4029 | `		if( rc == SXERR_ABORT ){` |
|       - |  4030 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4031 | `			return SXERR_ABORT;` |
|       - |  4032 | `		}` |
|     ! 0 |  4033 | `		goto Synchronize;` |
|       - |  4034 | `	}` |
|       - |  4035 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 |  4036 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - |  4037 | `		JumpFixup *aPost;` |
|       - |  4038 | `		VmInstr *pInstr;` |
|       - |  4039 | `		sxu32 nJumpDest;` |
|       - |  4040 | `		sxu32 n;` |
|     ! 0 |  4041 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 |  4042 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 |  4043 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 |  4044 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 |  4045 | `			if( pInstr ){` |
|       - |  4046 | `				/* Fix */` |
|     ! 0 |  4047 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 |  4048 | `			}` |
|     ! 0 |  4049 | `		}` |
|     ! 0 |  4050 | `	}` |
|       - |  4051 | `	/* Swap token streams */` |
|     ! 0 |  4052 | `	pTmp = pGen->pEnd;` |
|     ! 0 |  4053 | `	pGen->pEnd = pEnd;` |
|       - |  4054 | `	/* Compile the expression */` |
|     ! 0 |  4055 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  4056 | `	if( rc == SXERR_ABORT ){` |
|       - |  4057 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4058 | `		return SXERR_ABORT;` |
|       - |  4059 | `	}` |
|       - |  4060 | `	/* Update token stream */` |
|     ! 0 |  4061 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  4062 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4063 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4064 | `			return SXERR_ABORT;` |
|       - |  4065 | `		}` |
|     ! 0 |  4066 | `		pGen->pIn++;` |
|     ! 0 |  4067 | `	}` |
|     ! 0 |  4068 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 |  4069 | `	pGen->pEnd = pTmp;` |
|       - |  4070 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 |  4071 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - |  4072 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 |  4073 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4074 | `	/* Release the loop block */` |
|     ! 0 |  4075 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4076 | `	/* Statement successfully compiled */` |
|     ! 0 |  4077 | `	return SXRET_OK;` |
|       1 |  4078 | `Synchronize:` |
|       - |  4079 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4080 | `	 * compiling this erroneous block.` |
|       - |  4081 | `	 */` |
|       3 |  4082 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4083 | `		pGen->pIn++;` |
|     ! 0 |  4084 | `	}` |
|       3 |  4085 | `	return SXRET_OK;` |
|       2 |  4086 | `}` |
|       - |  4087 | `/*` |
|       - |  4088 | ` * Compile the complex and powerful 'for' statement.` |
|       - |  4089 | ` * According to the PHP language reference` |
|       - |  4090 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - |  4091 | ` *  The syntax of a for loop is:` |
|       - |  4092 | ` *  for (expr1; expr2; expr3)` |
|       - |  4093 | ` *   statement` |
|       - |  4094 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - |  4095 | ` *  the beginning of the loop.` |
|       - |  4096 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - |  4097 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - |  4098 | ` *  to FALSE, the execution of the loop ends.` |
|       - |  4099 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - |  4100 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - |  4101 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - |  4102 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - |  4103 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - |  4104 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - |  4105 | ` *  of using the for truth expression.` |
|       - |  4106 | ` */` |
|   14884 |  4107 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       5 |  4108 | `{` |
|   14889 |  4109 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   14889 |  4110 | `	GenBlock *pForBlock = 0;` |
|       - |  4111 | `	sxu32 nFalseJump;` |
|       - |  4112 | `	sxu32 nLine;` |
|       - |  4113 | `	sxi32 rc;` |
|   14889 |  4114 | `	nLine = pGen->pIn->nLine;` |
|       - |  4115 | `	/* Jump the 'for' keyword */` |
|   14889 |  4116 | `	pGen->pIn++;` |
|   14889 |  4117 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4118 | `		/* Syntax error */` |
|     ! 0 |  4119 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 |  4120 | `		if( rc == SXERR_ABORT ){` |
|       - |  4121 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4122 | `			return SXERR_ABORT;` |
|       - |  4123 | `		}` |
|     ! 0 |  4124 | `		return SXRET_OK;` |
|       - |  4125 | `	}` |
|       - |  4126 | `	/* Jump the left parenthesis '(' */` |
|   14889 |  4127 | `	pGen->pIn++;` |
|       - |  4128 | `	/* Delimit the init-expr;condition;post-expr */` |
|   14889 |  4129 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   14889 |  4130 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4131 | `		/* Empty expression */` |
|     ! 0 |  4132 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 |  4133 | `		if( rc == SXERR_ABORT ){` |
|       - |  4134 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4135 | `			return SXERR_ABORT;` |
|       - |  4136 | `		}` |
|       - |  4137 | `		/* Synchronize */` |
|     ! 0 |  4138 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4139 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4140 | `			pGen->pIn++;` |
|     ! 0 |  4141 | `		}` |
|     ! 0 |  4142 | `		return SXRET_OK;` |
|       - |  4143 | `	}` |
|       - |  4144 | `	/* Swap token streams */` |
|   14889 |  4145 | `	pTmp = pGen->pEnd;` |
|   14889 |  4146 | `	pGen->pEnd = pEnd;` |
|       - |  4147 | `	/* Compile initialization expressions if available */` |
|   14889 |  4148 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4149 | `	/* Pop operand lvalues */` |
|   14889 |  4150 | `	if( rc == SXERR_ABORT ){` |
|       - |  4151 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4152 | `		return SXERR_ABORT;` |
|   14889 |  4153 | `	}else if( rc != SXERR_EMPTY ){` |
|   14887 |  4154 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    7441 |  4155 | `	}` |
|   14889 |  4156 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4157 | `		/* Syntax error */` |
|     ! 0 |  4158 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  4159 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 |  4160 | `		if( rc == SXERR_ABORT ){` |
|       - |  4161 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4162 | `			return SXERR_ABORT;` |
|       - |  4163 | `		}` |
|     ! 0 |  4164 | `		return SXRET_OK;` |
|       - |  4165 | `	}` |
|       - |  4166 | `	/* Jump the trailing ';' */` |
|   14889 |  4167 | `	pGen->pIn++;` |
|       - |  4168 | `	/* Create the loop block */` |
|   14889 |  4169 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   14889 |  4170 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4171 | `		return SXERR_ABORT;` |
|       - |  4172 | `	}` |
|       - |  4173 | `	/* Deffer continue jumps */` |
|   14889 |  4174 | `	pForBlock->bPostContinue = TRUE;` |
|       - |  4175 | `	/* Compile the condition */` |
|   14889 |  4176 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14889 |  4177 | `	if( rc == SXERR_ABORT ){` |
|       - |  4178 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4179 | `		return SXERR_ABORT;` |
|   14889 |  4180 | `	}else if( rc != SXERR_EMPTY ){` |
|       - |  4181 | `		/* Emit the false jump */` |
|   14887 |  4182 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  4183 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   14887 |  4184 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    7441 |  4185 | `	}` |
|   14889 |  4186 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4187 | `		/* Syntax error */` |
|       6 |  4188 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  4189 | `			"for: Expected ';' after conditionals expressions");` |
|       6 |  4190 | `		if( rc == SXERR_ABORT ){` |
|       - |  4191 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4192 | `			return SXERR_ABORT;` |
|       - |  4193 | `		}` |
|       6 |  4194 | `		return SXRET_OK;` |
|       - |  4195 | `	}` |
|       - |  4196 | `	/* Jump the trailing ';' */` |
|   14885 |  4197 | `	pGen->pIn++;` |
|       - |  4198 | `	/* Save the post condition stream */` |
|   14885 |  4199 | `	pPostStart = pGen->pIn;` |
|       - |  4200 | `	/* Compile the loop body */` |
|   14885 |  4201 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   14885 |  4202 | `	pGen->pEnd = pTmp;` |
|   14885 |  4203 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   14885 |  4204 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  4205 | `		return SXERR_ABORT;` |
|       - |  4206 | `	}` |
|       - |  4207 | `	/* Fix post-continue jumps */` |
|   14885 |  4208 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - |  4209 | `		JumpFixup *aPost;` |
|       - |  4210 | `		VmInstr *pInstr;` |
|       - |  4211 | `		sxu32 nJumpDest;` |
|       - |  4212 | `		sxu32 n;` |
|      14 |  4213 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      14 |  4214 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      26 |  4215 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|      14 |  4216 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      14 |  4217 | `			if( pInstr ){` |
|       - |  4218 | `				/* Fix jump */` |
|      14 |  4219 | `				pInstr->iP2 = nJumpDest;` |
|       6 |  4220 | `			}` |
|       8 |  4221 | `		}` |
|       6 |  4222 | `	}` |
|       - |  4223 | `	/* compile the post-expressions if available */` |
|   14885 |  4224 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  4225 | `		pPostStart++;` |
|     ! 0 |  4226 | `	}` |
|   14885 |  4227 | `	if( pPostStart < pEnd ){` |
|       - |  4228 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   14885 |  4229 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   14885 |  4230 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14885 |  4231 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - |  4232 | `			/* Syntax error */` |
|     ! 0 |  4233 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 |  4234 | `			if( rc == SXERR_ABORT ){` |
|       - |  4235 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4236 | `				return SXERR_ABORT;` |
|       - |  4237 | `			}` |
|     ! 0 |  4238 | `			return SXRET_OK;` |
|       - |  4239 | `		}` |
|   14885 |  4240 | `		RE_SWAP_DELIMITER(pGen);` |
|   14885 |  4241 | `		if( rc == SXERR_ABORT ){` |
|       - |  4242 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4243 | `			return SXERR_ABORT;` |
|   14885 |  4244 | `		}else if( rc != SXERR_EMPTY){` |
|       - |  4245 | `			/* Pop operand lvalue */` |
|   14885 |  4246 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    7440 |  4247 | `		}` |
|    7440 |  4248 | `	}` |
|       - |  4249 | `	/* Emit the unconditional jump to the start of the loop */` |
|   14885 |  4250 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - |  4251 | `	/* Fix all jumps now the destination is resolved */` |
|   14885 |  4252 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4253 | `	/* Release the loop block */` |
|   14885 |  4254 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4255 | `	/* Statement successfully compiled */` |
|   14885 |  4256 | `	return SXRET_OK;` |
|    7447 |  4257 | `}` |
|       - |  4258 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - |  4259 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - |  4260 | ` * are allowed.` |
|       - |  4261 | ` */` |
|    7980 |  4262 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  4263 | `{` |
|    7985 |  4264 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    7985 |  4265 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  4266 | `		/* Unexpected expression */` |
|     ! 0 |  4267 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  4268 | `			"foreach: Expecting a variable name");` |
|     ! 0 |  4269 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  4270 | `			rc = SXERR_INVALID;` |
|     ! 0 |  4271 | `		}` |
|     ! 0 |  4272 | `	}` |
|    7985 |  4273 | `	return rc;` |
|       5 |  4274 | `}` |
|       - |  4275 | `/*` |
|       - |  4276 | ` * Compile the 'foreach' statement.` |
|       - |  4277 | ` * According to the PHP language reference` |
|       - |  4278 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - |  4279 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - |  4280 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - |  4281 | ` *  is a minor but useful extension of the first:` |
|       - |  4282 | ` *  foreach (array_expression as $value)` |
|       - |  4283 | ` *    statement` |
|       - |  4284 | ` *  foreach (array_expression as $key => $value)` |
|       - |  4285 | ` *   statement` |
|       - |  4286 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - |  4287 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - |  4288 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - |  4289 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - |  4290 | ` *  to the variable $key on each loop.` |
|       - |  4291 | ` *  Note:` |
|       - |  4292 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - |  4293 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - |  4294 | ` *  Note:` |
|       - |  4295 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - |  4296 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - |  4297 | ` *  or after the foreach without resetting it.` |
|       - |  4298 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - |  4299 | ` *  of copying the value.` |
|       - |  4300 | ` */` |
|    4100 |  4301 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       5 |  4302 | `{` |
|    4105 |  4303 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    4105 |  4304 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    4105 |  4305 | `	GenBlock *pForeachBlock = 0;` |
|       - |  4306 | `	ph7_foreach_info *pInfo;` |
|       - |  4307 | `	sxu32 nFalseJump;` |
|       - |  4308 | `	VmInstr *pInstr;` |
|       - |  4309 | `	sxu32 nLine;` |
|       - |  4310 | `	sxi32 rc;` |
|    4105 |  4311 | `	nLine = pGen->pIn->nLine;` |
|       - |  4312 | `	/* Jump the 'foreach' keyword */` |
|    4105 |  4313 | `	pGen->pIn++;` |
|    4105 |  4314 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4315 | `		/* Syntax error */` |
|     ! 0 |  4316 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 |  4317 | `		if( rc == SXERR_ABORT ){` |
|       - |  4318 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4319 | `			return SXERR_ABORT;` |
|       - |  4320 | `		}` |
|     ! 0 |  4321 | `		goto Synchronize;` |
|       - |  4322 | `	}` |
|       - |  4323 | `	/* Jump the left parenthesis '(' */` |
|    4105 |  4324 | `	pGen->pIn++;` |
|       - |  4325 | `	/* Create the loop block */` |
|    4105 |  4326 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    4105 |  4327 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4328 | `		return SXERR_ABORT;` |
|       - |  4329 | `	}` |
|       - |  4330 | `	/* Delimit the expression */` |
|    4105 |  4331 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    4105 |  4332 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4333 | `		/* Empty expression */` |
|     ! 0 |  4334 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 |  4335 | `		if( rc == SXERR_ABORT ){` |
|       - |  4336 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4337 | `			return SXERR_ABORT;` |
|       - |  4338 | `		}` |
|       - |  4339 | `		/* Synchronize */` |
|     ! 0 |  4340 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4341 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4342 | `			pGen->pIn++;` |
|     ! 0 |  4343 | `		}` |
|     ! 0 |  4344 | `		return SXRET_OK;` |
|       - |  4345 | `	}` |
|       - |  4346 | `	/* Compile the array expression */` |
|    4105 |  4347 | `	pCur = pGen->pIn;` |
|   28147 |  4348 | `	while( pCur < pEnd ){` |
|   28147 |  4349 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    4119 |  4350 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    4119 |  4351 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - |  4352 | `				/* Break with the first 'as' found */` |
|    4105 |  4353 | `				break;` |
|       - |  4354 | `			}` |
|       7 |  4355 | `		}` |
|       - |  4356 | `		/* Advance the stream cursor */` |
|   24047 |  4357 | `		pCur++;` |
|       5 |  4358 | `	}` |
|    4105 |  4359 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 |  4360 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  4361 | `			"foreach: Missing array/object expression");` |
|     ! 0 |  4362 | `		if( rc == SXERR_ABORT ){` |
|       - |  4363 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4364 | `			return SXERR_ABORT;` |
|       - |  4365 | `		}` |
|     ! 0 |  4366 | `		goto Synchronize;` |
|       - |  4367 | `	}` |
|       - |  4368 | `	/* Swap token streams */` |
|    4105 |  4369 | `	pTmp = pGen->pEnd;` |
|    4105 |  4370 | `	pGen->pEnd = pCur;` |
|    4105 |  4371 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    4105 |  4372 | `	if( rc == SXERR_ABORT ){` |
|       - |  4373 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4374 | `		return SXERR_ABORT;` |
|       - |  4375 | `	}` |
|       - |  4376 | `	/* Update token stream */` |
|    4105 |  4377 | `	while(pGen->pIn < pCur ){` |
|     ! 0 |  4378 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4379 | `		if( rc == SXERR_ABORT ){` |
|       - |  4380 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4381 | `			return SXERR_ABORT;` |
|       - |  4382 | `		}` |
|     ! 0 |  4383 | `		pGen->pIn++;` |
|     ! 0 |  4384 | `	}` |
|    4105 |  4385 | `	pCur++; /* Jump the 'as' keyword */` |
|    4105 |  4386 | `	pGen->pIn = pCur;` |
|    4105 |  4387 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4388 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 |  4389 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4390 | `			return SXERR_ABORT;` |
|       - |  4391 | `		}` |
|     ! 0 |  4392 | `	}` |
|       - |  4393 | `	/* Create the foreach context */` |
|    4105 |  4394 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    4105 |  4395 | `	if( pInfo == 0 ){` |
|     ! 0 |  4396 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  4397 | `		return SXERR_ABORT;` |
|       - |  4398 | `	}` |
|       - |  4399 | `	/* Zero the structure */` |
|    4105 |  4400 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - |  4401 | `	/* Initialize structure fields */` |
|    4105 |  4402 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - |  4403 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|       - |  4404 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|       - |  4405 | `	 * '=>'. */` |
|    4105 |  4406 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    4105 |  4407 | `	if( pCur < pEnd ){` |
|       - |  4408 | `		/* Compile the expression holding the key name */` |
|    3903 |  4409 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 |  4410 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 |  4411 | `			if( rc == SXERR_ABORT ){` |
|       - |  4412 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4413 | `				return SXERR_ABORT;` |
|       - |  4414 | `			}` |
|     ! 0 |  4415 | `		}else{` |
|    3903 |  4416 | `			pGen->pEnd = pCur;` |
|    3903 |  4417 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3903 |  4418 | `			if( rc == SXERR_ABORT ){` |
|       - |  4419 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4420 | `				return SXERR_ABORT;` |
|       - |  4421 | `			}` |
|    3903 |  4422 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3903 |  4423 | `			if( pInstr->p3 ){` |
|       - |  4424 | `				/* Record key name */` |
|    3903 |  4425 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1949 |  4426 | `			}` |
|    3903 |  4427 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - |  4428 | `		}` |
|    3903 |  4429 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1949 |  4430 | `	}` |
|    4105 |  4431 | `	pGen->pEnd = pEnd;` |
|    4105 |  4432 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4433 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 |  4434 | `		if( rc == SXERR_ABORT ){` |
|       - |  4435 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4436 | `			return SXERR_ABORT;` |
|       - |  4437 | `		}` |
|     ! 0 |  4438 | `		goto Synchronize;` |
|       - |  4439 | `	}` |
|    4105 |  4440 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 |  4441 | `		pGen->pIn++;` |
|       - |  4442 | `		/* Pass by reference  */` |
|      11 |  4443 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 |  4444 | `	}` |
|       - |  4445 | `	/* Check if the value target is list() */` |
|    4105 |  4446 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 |  4447 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  4448 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - |  4449 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - |  4450 | `		 */` |
|       - |  4451 | `		static int iForeachListCnt = 0;` |
|       - |  4452 | `		char zTmp[128];` |
|       - |  4453 | `		sxu32 nLen;` |
|       - |  4454 | `		char *zDup;` |
|      10 |  4455 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 |  4456 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 |  4457 | `		if( zDup == 0 ){` |
|     ! 0 |  4458 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4459 | `			return SXERR_ABORT;` |
|       - |  4460 | `		}` |
|      10 |  4461 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4462 | `		/* Save list() token boundaries */` |
|      10 |  4463 | `		pListStart = pGen->pIn;` |
|       - |  4464 | `		/* Advance past list(...) — validate parentheses */` |
|      10 |  4465 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 |  4466 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  4467 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  4468 | `				"foreach: Expected '(' after 'list'");` |
|       3 |  4469 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4470 | `				return SXERR_ABORT;` |
|       - |  4471 | `			}` |
|       3 |  4472 | `			goto Synchronize;` |
|       - |  4473 | `		}` |
|       7 |  4474 | `		pGen->pIn++; /* Jump '(' */` |
|       7 |  4475 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 |  4476 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4477 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4478 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 |  4479 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4480 | `				return SXERR_ABORT;` |
|       - |  4481 | `			}` |
|     ! 0 |  4482 | `			goto Synchronize;` |
|       - |  4483 | `		}` |
|       7 |  4484 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 |  4485 | `		pListEnd = pGen->pIn;` |
|       7 |  4486 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    4100 |  4487 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  4488 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - |  4489 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - |  4490 | `		 */` |
|       - |  4491 | `		static int iForeachShortListCnt = 0;` |
|       - |  4492 | `		char zTmp[128];` |
|       - |  4493 | `		sxu32 nLen;` |
|       - |  4494 | `		char *zDup;` |
|      11 |  4495 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|      11 |  4496 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      11 |  4497 | `		if( zDup == 0 ){` |
|     ! 0 |  4498 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4499 | `			return SXERR_ABORT;` |
|       - |  4500 | `		}` |
|      11 |  4501 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4502 | `		/* Save [...] token boundaries */` |
|      11 |  4503 | `		pListStart = pGen->pIn;` |
|       - |  4504 | `		/* Advance past [...] */` |
|      11 |  4505 | `		pGen->pIn++; /* Jump '[' */` |
|      11 |  4506 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|      11 |  4507 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4508 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4509 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 |  4510 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4511 | `				return SXERR_ABORT;` |
|       - |  4512 | `			}` |
|     ! 0 |  4513 | `			goto Synchronize;` |
|       - |  4514 | `		}` |
|      11 |  4515 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|      11 |  4516 | `		pListEnd = pGen->pIn;` |
|      11 |  4517 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       6 |  4518 | `	}else{` |
|       - |  4519 | `		/* Compile the expression holding the value name */` |
|    4087 |  4520 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    4087 |  4521 | `		if( rc == SXERR_ABORT ){` |
|       - |  4522 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4523 | `			return SXERR_ABORT;` |
|       - |  4524 | `		}` |
|    4087 |  4525 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    4087 |  4526 | `		if( pInstr->p3 ){` |
|       - |  4527 | `			/* Record value name */` |
|    4087 |  4528 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    2041 |  4529 | `		}` |
|       - |  4530 | `	}` |
|       - |  4531 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    4103 |  4532 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - |  4533 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    4103 |  4534 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - |  4535 | `	/* Record the first instruction to execute */` |
|    4103 |  4536 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4537 | `	/* Emit the FOREACH_STEP instruction */` |
|    4103 |  4538 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - |  4539 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    4103 |  4540 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - |  4541 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    4103 |  4542 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - |  4543 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - |  4544 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - |  4545 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - |  4546 | `		 */` |
|      17 |  4547 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - |  4548 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - |  4549 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - |  4550 | `		 * picks up the delimiter and the variable names inside.` |
|       - |  4551 | `		 */` |
|      17 |  4552 | `		pSavedIn = pGen->pIn;` |
|      17 |  4553 | `		pSavedEnd = pGen->pEnd;` |
|      17 |  4554 | `		pGen->pIn = pListStart;` |
|      17 |  4555 | `		pGen->pEnd = pListEnd;` |
|      17 |  4556 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|      11 |  4557 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       6 |  4558 | `		}else{` |
|       7 |  4559 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - |  4560 | `		}` |
|      17 |  4561 | `		pGen->pIn = pSavedIn;` |
|      17 |  4562 | `		pGen->pEnd = pSavedEnd;` |
|      17 |  4563 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4564 | `			return SXERR_ABORT;` |
|       - |  4565 | `		}` |
|       - |  4566 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|      17 |  4567 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       8 |  4568 | `	}` |
|       - |  4569 | `	/* Compile the loop body */` |
|    4103 |  4570 | `	pGen->pIn = &pEnd[1];` |
|    4103 |  4571 | `	pGen->pEnd = pTmp;` |
|    4103 |  4572 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    4103 |  4573 | `	if( rc == SXERR_ABORT ){` |
|       - |  4574 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4575 | `		return SXERR_ABORT;` |
|       - |  4576 | `	}` |
|       - |  4577 | `	/* Emit the unconditional jump to the start of the loop */` |
|    4103 |  4578 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - |  4579 | `	/* Fix all jumps now the destination is resolved */` |
|    4103 |  4580 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4581 | `	/* Release the loop block */` |
|    4103 |  4582 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4583 | `	/* Statement successfully compiled */` |
|    4103 |  4584 | `	return SXRET_OK;` |
|       1 |  4585 | `Synchronize:` |
|       - |  4586 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4587 | `	 * compiling this erroneous block.` |
|       - |  4588 | `	 */` |
|       3 |  4589 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4590 | `		pGen->pIn++;` |
|     ! 0 |  4591 | `	}` |
|       3 |  4592 | `	return SXRET_OK;` |
|    2055 |  4593 | `}` |
|       - |  4594 | `/*` |
|       - |  4595 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - |  4596 | ` * According to the PHP language reference` |
|       - |  4597 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - |  4598 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - |  4599 | ` *  that is similar to that of C:` |
|       - |  4600 | ` *  if (expr)` |
|       - |  4601 | ` *   statement` |
|       - |  4602 | ` *  else construct:` |
|       - |  4603 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - |  4604 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - |  4605 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - |  4606 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - |  4607 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - |  4608 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - |  4609 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - |  4610 | ` *  elseif` |
|       - |  4611 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - |  4612 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - |  4613 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - |  4614 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - |  4615 | ` *   than b, a equal to b or a is smaller than b:` |
|       - |  4616 | ` *   <?php` |
|       - |  4617 | ` *    if ($a > $b) {` |
|       - |  4618 | ` *     echo "a is bigger than b";` |
|       - |  4619 | ` *    } elseif ($a == $b) {` |
|       - |  4620 | ` *     echo "a is equal to b";` |
|       - |  4621 | ` *    } else {` |
|       - |  4622 | ` *     echo "a is smaller than b";` |
|       - |  4623 | ` *    }` |
|       - |  4624 | ` *    ?>` |
|       - |  4625 | ` */` |
|  154456 |  4626 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       5 |  4627 | `{` |
|  154461 |  4628 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  154461 |  4629 | `	GenBlock *pCondBlock = 0;` |
|       - |  4630 | `	sxu32 nJumpIdx;` |
|       - |  4631 | `	sxu32 nKeyID;` |
|       - |  4632 | `	sxi32 rc;` |
|       - |  4633 | `	/* Jump the 'if' keyword */` |
|  154461 |  4634 | `	pGen->pIn++;` |
|  154461 |  4635 | `	pToken = pGen->pIn;` |
|       - |  4636 | `	/* Create the conditional block */` |
|  154461 |  4637 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  154461 |  4638 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4639 | `		return SXERR_ABORT;` |
|       - |  4640 | `	}` |
|       - |  4641 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   84665 |  4642 | `	for(;;){` |
|  169335 |  4643 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4644 | `			/* Syntax error */` |
|     ! 0 |  4645 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4646 | `				pToken--;` |
|     ! 0 |  4647 | `			}` |
|     ! 0 |  4648 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 |  4649 | `			if( rc == SXERR_ABORT ){` |
|       - |  4650 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4651 | `				return SXERR_ABORT;` |
|       - |  4652 | `			}` |
|     ! 0 |  4653 | `			goto Synchronize;` |
|       - |  4654 | `		}` |
|       - |  4655 | `		/* Jump the left parenthesis '(' */` |
|  169335 |  4656 | `		pToken++;` |
|       - |  4657 | `		/* Delimit the condition */` |
|  169335 |  4658 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  169335 |  4659 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - |  4660 | `			/* Syntax error */` |
|     ! 0 |  4661 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4662 | `				pToken--;` |
|     ! 0 |  4663 | `			}` |
|     ! 0 |  4664 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 |  4665 | `			if( rc == SXERR_ABORT ){` |
|       - |  4666 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4667 | `				return SXERR_ABORT;` |
|       - |  4668 | `			}` |
|     ! 0 |  4669 | `			goto Synchronize;` |
|       - |  4670 | `		}` |
|       - |  4671 | `		/* Swap token streams */` |
|  169335 |  4672 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - |  4673 | `		/* Compile the condition */` |
|  169335 |  4674 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4675 | `		/* Update token stream */` |
|  169335 |  4676 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 |  4677 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4678 | `			pGen->pIn++;` |
|     ! 0 |  4679 | `		}` |
|  169335 |  4680 | `		pGen->pIn  = &pEnd[1];` |
|  169335 |  4681 | `		pGen->pEnd = pTmp;` |
|  169335 |  4682 | `		if( rc == SXERR_ABORT ){` |
|       - |  4683 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4684 | `			return SXERR_ABORT;` |
|       - |  4685 | `		}` |
|       - |  4686 | `		/* Emit the false jump */` |
|  169335 |  4687 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - |  4688 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  169335 |  4689 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - |  4690 | `		/* Compile the body */` |
|  169335 |  4691 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  169335 |  4692 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4693 | `			return SXERR_ABORT;` |
|       - |  4694 | `		}` |
|  169335 |  4695 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   47178 |  4696 | `			break;` |
|       - |  4697 | `		}` |
|       - |  4698 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   74989 |  4699 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   74989 |  4700 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   48297 |  4701 | `			break;` |
|       - |  4702 | `		}` |
|       - |  4703 | `		/* Emit the unconditional jump */` |
|   26697 |  4704 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - |  4705 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   26697 |  4706 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   26697 |  4707 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   19203 |  4708 | `			pToken = &pGen->pIn[1];` |
|   19203 |  4709 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    7432 |  4710 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    5914 |  4711 | `					break;` |
|       - |  4712 | `			}` |
|    7385 |  4713 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    3690 |  4714 | `		}` |
|   14879 |  4715 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - |  4716 | `		/* Synchronize cursors */` |
|   14879 |  4717 | `		pToken = pGen->pIn;` |
|       - |  4718 | `		/* Fix the false jump */` |
|   14879 |  4719 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       5 |  4720 | `	} /* For(;;) */` |
|       - |  4721 | `	/* Fix the false jump */` |
|  154461 |  4722 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  154461 |  4723 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   60110 |  4724 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - |  4725 | `			/* Compile the else block */` |
|   11823 |  4726 | `			pGen->pIn++;` |
|   11823 |  4727 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   11823 |  4728 | `			if( rc == SXERR_ABORT ){` |
|       - |  4729 |  |
|     ! 0 |  4730 | `				return SXERR_ABORT;` |
|       - |  4731 | `			}` |
|    5909 |  4732 | `	}` |
|  154461 |  4733 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4734 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  154461 |  4735 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - |  4736 | `	/* Release the conditional block */` |
|  154461 |  4737 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4738 | `	/* Statement successfully compiled */` |
|  154461 |  4739 | `	return SXRET_OK;` |
|     ! 0 |  4740 | `Synchronize:` |
|       - |  4741 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - |  4742 | `	 */` |
|     ! 0 |  4743 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4744 | `		pGen->pIn++;` |
|     ! 0 |  4745 | `	}` |
|     ! 0 |  4746 | `	return SXRET_OK;` |
|   77233 |  4747 | `}` |
|       - |  4748 | `/*` |
|       - |  4749 | ` * Compile the global construct.` |
|       - |  4750 | ` * According to the PHP language reference` |
|       - |  4751 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - |  4752 | ` *  to be used in that function.` |
|       - |  4753 | ` *  Example #1 Using global` |
|       - |  4754 | ` *  <?php` |
|       - |  4755 | ` *   $a = 1;` |
|       - |  4756 | ` *   $b = 2;` |
|       - |  4757 | ` *   function Sum()` |
|       - |  4758 | ` *   {` |
|       - |  4759 | ` *    global $a, $b;` |
|       - |  4760 | ` *    $b = $a + $b;` |
|       - |  4761 | ` *   }` |
|       - |  4762 | ` *   Sum();` |
|       - |  4763 | ` *   echo $b;` |
|       - |  4764 | ` *  ?>` |
|       - |  4765 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - |  4766 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - |  4767 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - |  4768 | ` */` |
|      36 |  4769 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       5 |  4770 | `{` |
|      41 |  4771 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4772 | `	sxi32 nExpr;` |
|       - |  4773 | `	sxi32 rc;` |
|       - |  4774 | `	/* Jump the 'global' keyword */` |
|      41 |  4775 | `	pGen->pIn++;` |
|      41 |  4776 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - |  4777 | `		/* Nothing to process */` |
|     ! 0 |  4778 | `		return SXRET_OK;` |
|       - |  4779 | `	}` |
|      41 |  4780 | `	pTmp = pGen->pEnd;` |
|      41 |  4781 | `	nExpr = 0;` |
|      87 |  4782 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      51 |  4783 | `		if( pGen->pIn < pNext ){` |
|      51 |  4784 | `			pGen->pEnd = pNext;` |
|      51 |  4785 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4786 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 |  4787 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  4788 | `					return SXERR_ABORT;` |
|       - |  4789 | `				}` |
|     ! 0 |  4790 | `			}else{` |
|      51 |  4791 | `				pGen->pIn++;` |
|      51 |  4792 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4793 | `					/* Emit a warning */` |
|     ! 0 |  4794 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 |  4795 | `				}else{` |
|      51 |  4796 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      51 |  4797 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  4798 | `						return SXERR_ABORT;` |
|      51 |  4799 | `					}else if(rc != SXERR_EMPTY ){` |
|      51 |  4800 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      51 |  4801 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - |  4802 | `							/* Variable name, not a constant */` |
|      51 |  4803 | `							pLast->iP1 = 0;` |
|      23 |  4804 | `						}` |
|      51 |  4805 | `						nExpr++;` |
|      23 |  4806 | `					}` |
|       - |  4807 | `				}` |
|       - |  4808 | `			}` |
|      23 |  4809 | `		}` |
|       - |  4810 | `		/* Next expression in the stream */` |
|      51 |  4811 | `		pGen->pIn = pNext;` |
|       - |  4812 | `		/* Jump trailing commas */` |
|      61 |  4813 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      15 |  4814 | `			pGen->pIn++;` |
|       5 |  4815 | `		}` |
|       5 |  4816 | `	}` |
|       - |  4817 | `	/* Restore token stream */` |
|      41 |  4818 | `	pGen->pEnd = pTmp;` |
|      41 |  4819 | `	if( nExpr > 0 ){` |
|       - |  4820 | `		/* Emit the uplink instruction */` |
|      41 |  4821 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      18 |  4822 | `	}` |
|      41 |  4823 | `	return SXRET_OK;` |
|      23 |  4824 | `}` |
|       - |  4825 | `/*` |
|       - |  4826 | ` * Compile the return statement.` |
|       - |  4827 | ` * According to the PHP language reference` |
|       - |  4828 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - |  4829 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - |  4830 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - |  4831 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - |  4832 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - |  4833 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - |  4834 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - |  4835 | ` *  from within the main script file, then script execution end.` |
|       - |  4836 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - |  4837 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - |  4838 | ` *  should do so as PHP has less work to do in this case.` |
|       - |  4839 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - |  4840 | ` */` |
|  245008 |  4841 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       5 |  4842 | `{` |
|  245013 |  4843 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - |  4844 | `	sxi32 rc;` |
|  245013 |  4845 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  245013 |  4846 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|       - |  4847 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|       - |  4848 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|       - |  4849 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|       - |  4850 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|       - |  4851 | `	 * normally below so token processing stays consistent. */` |
|  630951 |  4852 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  385943 |  4853 | `		pFuncBlock = pFuncBlock->pParent;` |
|       5 |  4854 | `	}` |
|  245008 |  4855 | `	if( pFuncBlock && pFuncBlock->pUserData` |
|  244981 |  4856 | `	 && ((ph7_vm_func *)pFuncBlock->pUserData)->nReturnType == MEMOBJ_NEVER ){` |
|       3 |  4857 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  4858 | `			"A never-returning function must not return");` |
|       3 |  4859 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4860 | `			return SXERR_ABORT;` |
|       - |  4861 | `		}` |
|       1 |  4862 | `	}` |
|       - |  4863 | `	/* Jump the 'return' keyword */` |
|  245013 |  4864 | `	pGen->pIn++;` |
|  245013 |  4865 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4866 | `		/* Compile the expression */` |
|  244983 |  4867 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  244983 |  4868 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4869 | `			return SXERR_ABORT;` |
|  244983 |  4870 | `		}else if(rc != SXERR_EMPTY ){` |
|  244983 |  4871 | `			nRet = 1;` |
|  122489 |  4872 | `		}` |
|  122489 |  4873 | `	}` |
|       - |  4874 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|       - |  4875 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|       - |  4876 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|       - |  4877 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|  245013 |  4878 | `	if( pGen->bInGenerator ){` |
|      24 |  4879 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|      24 |  4880 | `		return SXRET_OK;` |
|       - |  4881 | `	}` |
|       - |  4882 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|       - |  4883 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|       - |  4884 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|       - |  4885 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|       - |  4886 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  244993 |  4887 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  244993 |  4888 | `	return SXRET_OK;` |
|  122509 |  4889 | `}` |
|       - |  4890 | `/*` |
|       - |  4891 | ` * Compile a yield expression.` |
|       - |  4892 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - |  4893 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - |  4894 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - |  4895 | ` */` |
|     232 |  4896 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       5 |  4897 | `{` |
|       - |  4898 | `	SyToken *pTmp, *pSplit;` |
|     237 |  4899 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     237 |  4900 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - |  4901 | `	sxi32 rc;` |
|     116 |  4902 | `	(void)iCompileFlag;` |
|       - |  4903 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     237 |  4904 | `	pGen->pIn++;` |
|       - |  4905 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - |  4906 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|       - |  4907 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|       - |  4908 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|       - |  4909 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     232 |  4910 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|     134 |  4911 | `		&& pGen->pIn->sData.nByte == 4` |
|      43 |  4912 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|      42 |  4913 | `		pGen->pIn++; /* Skip 'from' */` |
|      42 |  4914 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      42 |  4915 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4916 | `			return SXERR_ABORT;` |
|       - |  4917 | `		}` |
|      42 |  4918 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  4919 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|     ! 0 |  4920 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|       - |  4921 | `				"Missing expression after 'yield from'");` |
|     ! 0 |  4922 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4923 | `				return SXERR_ABORT;` |
|       - |  4924 | `			}` |
|     ! 0 |  4925 | `		}` |
|      42 |  4926 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|      42 |  4927 | `		return SXRET_OK;` |
|       - |  4928 | `	}` |
|     199 |  4929 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4930 | `		/* Bare yield — no value */` |
|       3 |  4931 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|       3 |  4932 | `		return SXRET_OK;` |
|       - |  4933 | `	}` |
|       - |  4934 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     197 |  4935 | `	pSplit = 0;` |
|       - |  4936 | `	{` |
|     197 |  4937 | `		SyToken *pCur = pGen->pIn;` |
|     197 |  4938 | `		sxi32 nNest = 0;` |
|     413 |  4939 | `		while( pCur < pGen->pEnd ){` |
|     235 |  4940 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  4941 | `				nNest++;` |
|     235 |  4942 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  4943 | `				nNest--;` |
|     235 |  4944 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|      16 |  4945 | `				pSplit = pCur;` |
|      16 |  4946 | `				break;` |
|       - |  4947 | `			}` |
|     221 |  4948 | `			pCur++;` |
|       5 |  4949 | `		}` |
|       - |  4950 | `	}` |
|     197 |  4951 | `	pTmp = pGen->pEnd;` |
|     197 |  4952 | `	if( pSplit ){` |
|       - |  4953 | `		/* yield $key => $value */` |
|      16 |  4954 | `		pGen->pEnd = pSplit;` |
|      16 |  4955 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4956 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4957 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|      16 |  4958 | `		pGen->pEnd = pTmp;` |
|      16 |  4959 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4960 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4961 | `		iP1 = 1;` |
|      16 |  4962 | `		iP2 = 1;` |
|       9 |  4963 | `	}else{` |
|       - |  4964 | `		/* yield $value */` |
|     183 |  4965 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     183 |  4966 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     183 |  4967 | `		if( rc != SXERR_EMPTY ){` |
|     183 |  4968 | `			iP1 = 1;` |
|      89 |  4969 | `		}` |
|       - |  4970 | `	}` |
|     197 |  4971 | `	pGen->pEnd = pTmp;` |
|     197 |  4972 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     197 |  4973 | `	return SXRET_OK;` |
|     121 |  4974 | `}` |
|       - |  4975 | `/*` |
|       - |  4976 | ` * Compile the die/exit language construct.` |
|       - |  4977 | ` * The role of these constructs is to terminate execution of the script.` |
|       - |  4978 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - |  4979 | ` */` |
|     120 |  4980 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       5 |  4981 | `{` |
|     125 |  4982 | `	sxi32 nExpr = 0;` |
|       - |  4983 | `	sxi32 rc;` |
|       - |  4984 | `	/* Jump the die/exit keyword */` |
|     125 |  4985 | `	pGen->pIn++;` |
|     125 |  4986 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4987 | `		/* Compile the expression */` |
|     125 |  4988 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     125 |  4989 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4990 | `			return SXERR_ABORT;` |
|     125 |  4991 | `		}else if(rc != SXERR_EMPTY ){` |
|     125 |  4992 | `			nExpr = 1;` |
|      60 |  4993 | `		}` |
|      60 |  4994 | `	}` |
|       - |  4995 | `	/* Emit the HALT instruction */` |
|     125 |  4996 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|     125 |  4997 | `	return SXRET_OK;` |
|      65 |  4998 | `}` |
|       - |  4999 | `/*` |
|       - |  5000 | ` * Compile the 'echo' language construct.` |
|       - |  5001 | ` */` |
|   14874 |  5002 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       5 |  5003 | `{` |
|   14879 |  5004 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  5005 | `	sxi32 rc;` |
|       - |  5006 | `	/* Jump the 'echo' keyword */` |
|   14879 |  5007 | `	pGen->pIn++;` |
|       - |  5008 | `	/* Compile arguments one after one */` |
|   14879 |  5009 | `	pTmp = pGen->pEnd;` |
|   33449 |  5010 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   18575 |  5011 | `		if( pGen->pIn < pNext ){` |
|   18575 |  5012 | `			pGen->pEnd = pNext;` |
|   18575 |  5013 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   18575 |  5014 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5015 | `				return SXERR_ABORT;` |
|   18575 |  5016 | `			}else if( rc != SXERR_EMPTY ){` |
|       - |  5017 | `				/* Emit the consume instruction */` |
|   18551 |  5018 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    9273 |  5019 | `			}` |
|    9285 |  5020 | `		}` |
|       - |  5021 | `		/* Jump trailing commas */` |
|   22271 |  5022 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    3701 |  5023 | `			pNext++;` |
|       5 |  5024 | `		}` |
|   18575 |  5025 | `		pGen->pIn = pNext;` |
|       5 |  5026 | `	}` |
|       - |  5027 | `	/* Restore token stream */` |
|   14879 |  5028 | `	pGen->pEnd = pTmp;` |
|   14879 |  5029 | `	return SXRET_OK;` |
|    7442 |  5030 | `}` |
|       - |  5031 | `/*` |
|       - |  5032 | ` * Compile the static statement.` |
|       - |  5033 | ` * According to the PHP language reference` |
|       - |  5034 | ` *  Another important feature of variable scoping is the static variable.` |
|       - |  5035 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - |  5036 | ` *  when program execution leaves this scope.` |
|       - |  5037 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - |  5038 | ` * Symisc eXtension.` |
|       - |  5039 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - |  5040 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  5041 | ` *  Example` |
|       - |  5042 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  5043 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  5044 | ` */` |
|       8 |  5045 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       3 |  5046 | `{` |
|       - |  5047 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - |  5048 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - |  5049 | `	GenBlock *pBlock;` |
|       - |  5050 | `	SyString *pName;` |
|       - |  5051 | `	char *zDup;` |
|       - |  5052 | `	sxu32 nLine;` |
|       - |  5053 | `	sxi32 rc;` |
|       - |  5054 | `	/* Jump the static keyword */` |
|      11 |  5055 | `	nLine = pGen->pIn->nLine;` |
|      11 |  5056 | `	pGen->pIn++;` |
|       - |  5057 | `	/* Extract the enclosing function if any */` |
|      11 |  5058 | `	pBlock = pGen->pCurrent;` |
|      19 |  5059 | `	while( pBlock ){` |
|      19 |  5060 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|      11 |  5061 | `			break;` |
|       - |  5062 | `		}` |
|       - |  5063 | `		/* Point to the upper block */` |
|      11 |  5064 | `		pBlock = pBlock->pParent;` |
|       3 |  5065 | `	}` |
|      11 |  5066 | `	if( pBlock == 0 ){` |
|       - |  5067 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 |  5068 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  5069 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 |  5070 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5071 | `				return SXERR_ABORT;` |
|       - |  5072 | `			}` |
|     ! 0 |  5073 | `			goto Synchronize;` |
|       - |  5074 | `		}` |
|       - |  5075 | `		/* Compile the expression holding the variable */` |
|     ! 0 |  5076 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  5077 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5078 | `			return SXERR_ABORT;` |
|     ! 0 |  5079 | `		}else if( rc != SXERR_EMPTY ){` |
|       - |  5080 | `			/* Emit the POP instruction */` |
|     ! 0 |  5081 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  5082 | `		}` |
|     ! 0 |  5083 | `		return SXRET_OK;` |
|       - |  5084 | `	}` |
|      11 |  5085 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - |  5086 | `	/* Make sure we are dealing with a valid statement */` |
|      11 |  5087 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       6 |  5088 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 |  5089 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 |  5090 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5091 | `				return SXERR_ABORT;` |
|       - |  5092 | `			}` |
|       3 |  5093 | `			goto Synchronize;` |
|       - |  5094 | `	}` |
|       8 |  5095 | `	pGen->pIn++;` |
|       - |  5096 | `	/* Extract variable name */` |
|       8 |  5097 | `	pName = &pGen->pIn->sData;` |
|       8 |  5098 | `	pGen->pIn++; /* Jump the var name */` |
|       8 |  5099 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 |  5100 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  5101 | `		goto Synchronize;` |
|       - |  5102 | `	}` |
|       - |  5103 | `	/* Initialize the structure describing the static variable */` |
|       8 |  5104 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       8 |  5105 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - |  5106 | `	/* Duplicate variable name */` |
|       8 |  5107 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       8 |  5108 | `	if( zDup == 0 ){` |
|     ! 0 |  5109 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  5110 | `		return SXERR_ABORT;` |
|       - |  5111 | `	}` |
|       8 |  5112 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - |  5113 | `	/* Check if we have an expression to compile */` |
|       8 |  5114 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - |  5115 | `		SySet *pInstrContainer;` |
|       - |  5116 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - |  5117 | `		 * Static variable can take any complex expression including function` |
|       - |  5118 | `		 * call as their initialization value.` |
|       - |  5119 | `		 * Example:` |
|       - |  5120 | `		 *		static $var = foo(1,4+5,bar());` |
|       - |  5121 | `		 */` |
|       8 |  5122 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - |  5123 | `		/* Swap bytecode container */` |
|       8 |  5124 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       8 |  5125 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - |  5126 | `		/* Compile the expression */` |
|       8 |  5127 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5128 | `		/* Emit the done instruction */` |
|       8 |  5129 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - |  5130 | `		/* Restore default bytecode container */` |
|       8 |  5131 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       3 |  5132 | `	}` |
|       - |  5133 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       8 |  5134 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       8 |  5135 | `	return SXRET_OK;` |
|       1 |  5136 | `Synchronize:` |
|       - |  5137 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - |  5138 | `	 * statement.` |
|       - |  5139 | `	 */` |
|       5 |  5140 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 |  5141 | `		pGen->pIn++;` |
|       1 |  5142 | `	}` |
|       3 |  5143 | `	return SXRET_OK;` |
|       7 |  5144 | `}` |
|       - |  5145 | `/*` |
|       - |  5146 | ` * Compile the var statement.` |
|       - |  5147 | ` * Symisc Extension:` |
|       - |  5148 | ` *      var statement can be used outside of a class definition.` |
|       - |  5149 | ` */` |
|       4 |  5150 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 |  5151 | `{` |
|       - |  5152 | `	sxu32 nLine;` |
|       - |  5153 | `	sxi32 rc;` |
|       5 |  5154 | `	nLine = pGen->pIn->nLine;` |
|       - |  5155 | `	/* Jump the 'var' keyword */` |
|       5 |  5156 | `	pGen->pIn++;` |
|       5 |  5157 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  5158 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - |  5159 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 |  5160 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 |  5161 | `			pGen->pIn++;` |
|     ! 0 |  5162 | `		}` |
|     ! 0 |  5163 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5164 | `			return SXERR_ABORT;` |
|       - |  5165 | `		}` |
|     ! 0 |  5166 | `	}else{` |
|       - |  5167 | `		/* Compile the expression */` |
|       5 |  5168 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 |  5169 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5170 | `			return SXERR_ABORT;` |
|       5 |  5171 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 |  5172 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 |  5173 | `		}` |
|       - |  5174 | `	}` |
|       5 |  5175 | `	return SXRET_OK;` |
|       3 |  5176 | `}` |
|       - |  5177 | `/*` |
|       - |  5178 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - |  5179 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - |  5180 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - |  5181 | ` */` |
|       - |  5182 | `/*` |
|       - |  5183 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - |  5184 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - |  5185 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - |  5186 | ` * qualified name and updates the instruction's operand index.` |
|       - |  5187 | ` *` |
|       - |  5188 | ` * Resolution order:` |
|       - |  5189 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - |  5190 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - |  5191 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - |  5192 | ` *` |
|       - |  5193 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - |  5194 | ` * came from an import (step 1) and 0 otherwise.` |
|       - |  5195 | ` * Returns the (possibly new) literal index.` |
|       - |  5196 | ` */` |
|  476036 |  5197 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       5 |  5198 | `{` |
|       - |  5199 | `	ph7_value *pLit;` |
|       - |  5200 | `	const char *zLit;` |
|       - |  5201 | `	SyString sQualified;` |
|       - |  5202 | `	sxu32 nLit;` |
|       - |  5203 | `	sxu32 k;` |
|       - |  5204 | `	sxu32 nNewIdx;` |
|       - |  5205 | `	int hasNsSep;` |
|       - |  5206 | `	SyHashEntry *pImport;` |
|       - |  5207 | `	ph7_value *pNew;` |
|  476041 |  5208 | `	if( pFromImport ){` |
|  455577 |  5209 | `		*pFromImport = 0;` |
|  227786 |  5210 | `	}` |
|  476041 |  5211 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  476041 |  5212 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 |  5213 | `		return nOrigIdx;` |
|       - |  5214 | `	}` |
|  476041 |  5215 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  476041 |  5216 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - |  5217 | `	/* Skip if already qualified (contains backslash) */` |
|  476041 |  5218 | `	hasNsSep = 0;` |
| 5256525 |  5219 | `	for( k = 0; k < nLit; k++ ){` |
| 4780497 |  5220 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 2390247 |  5221 | `	}` |
|  476041 |  5222 | `	if( hasNsSep ){` |
|      10 |  5223 | `		return nOrigIdx;` |
|       - |  5224 | `	}` |
|       - |  5225 | `	/* Check use imports first (works even outside namespaces) */` |
|  476033 |  5226 | `	SyBlobReset(&pGen->sWorker);` |
|  476033 |  5227 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  476033 |  5228 | `	if( pImport ){` |
|      41 |  5229 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      41 |  5230 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      41 |  5231 | `		if( pFromImport ){` |
|      18 |  5232 | `			*pFromImport = 1;` |
|       8 |  5233 | `		}` |
|      23 |  5234 | `	}else{` |
|  475997 |  5235 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  475907 |  5236 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - |  5237 | `		}` |
|       - |  5238 | `		/* Prepend current namespace */` |
|      95 |  5239 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      95 |  5240 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      95 |  5241 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - |  5242 | `	}` |
|       - |  5243 | `	/* Look up or create a new literal for the qualified name */` |
|     131 |  5244 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     131 |  5245 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      57 |  5246 | `		return nNewIdx; /* Already interned */` |
|       - |  5247 | `	}` |
|      79 |  5248 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      79 |  5249 | `	if( pNew == 0 ){` |
|     ! 0 |  5250 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - |  5251 | `	}` |
|      79 |  5252 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      79 |  5253 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      79 |  5254 | `	return nNewIdx;` |
|  238023 |  5255 | `}` |
|       - |  5256 | `/*` |
|       - |  5257 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - |  5258 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - |  5259 | ` */` |
|  100632 |  5260 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5261 | `{` |
|       - |  5262 | `	SyHashEntry *pImport;` |
|       - |  5263 | `	/* Check use imports first */` |
|  100637 |  5264 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|  100637 |  5265 | `	if( pImport ){` |
|      14 |  5266 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      14 |  5267 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      14 |  5268 | `		return;` |
|       - |  5269 | `	}` |
|       - |  5270 | `	/* Prepend current namespace if active */` |
|  100625 |  5271 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 |  5272 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  5273 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 |  5274 | `	}` |
|  100625 |  5275 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   50321 |  5276 | `}` |
|       - |  5277 | `/*` |
|       - |  5278 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - |  5279 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - |  5280 | ` * The caller must release pOut when done.` |
|       - |  5281 | ` */` |
|  149044 |  5282 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5283 | `{` |
|  149049 |  5284 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|    3753 |  5285 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|    3753 |  5286 | `		SyBlobAppend(pOut,"\\",1);` |
|    1874 |  5287 | `	}` |
|  149049 |  5288 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|  149049 |  5289 | `}` |
|       - |  5290 | `/*` |
|       - |  5291 | ` * Compile a namespace statement` |
|       - |  5292 | ` * According to the PHP language reference manual` |
|       - |  5293 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - |  5294 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - |  5295 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - |  5296 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - |  5297 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - |  5298 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - |  5299 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - |  5300 | ` *  programming world.` |
|       - |  5301 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - |  5302 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - |  5303 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - |  5304 | ` *  classes/functions/constants.` |
|       - |  5305 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - |  5306 | ` *  readability of source code.` |
|       - |  5307 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - |  5308 | ` *  Here is an example of namespace syntax in PHP:` |
|       - |  5309 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - |  5310 | ` *       class MyClass {}` |
|       - |  5311 | ` *       function myfunction() {}` |
|       - |  5312 | ` *       const MYCONST = 1;` |
|       - |  5313 | ` *       $a = new MyClass;` |
|       - |  5314 | ` *       $c = new \my\name\MyClass;` |
|       - |  5315 | ` *       $a = strlen('hi');` |
|       - |  5316 | ` *       $d = namespace\MYCONST;` |
|       - |  5317 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - |  5318 | ` *       echo constant($d);` |
|       - |  5319 | ` * NOTE` |
|       - |  5320 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5321 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5322 | ` */` |
|       - |  5323 | `/*` |
|       - |  5324 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - |  5325 | ` */` |
|      14 |  5326 | `static const char * TokenTypeName(sxu32 nType)` |
|       3 |  5327 | `{` |
|      17 |  5328 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|      10 |  5329 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|      10 |  5330 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|      10 |  5331 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|      10 |  5332 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|      10 |  5333 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 |  5334 | `	return "token";` |
|      10 |  5335 | `}` |
|    3796 |  5336 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       5 |  5337 | `{` |
|       - |  5338 | `	sxu32 nLine;` |
|       - |  5339 | `	sxi32 rc;` |
|    3801 |  5340 | `	nLine = pGen->pIn->nLine;` |
|    3801 |  5341 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - |  5342 | `	/* Reset namespace and clear previous use imports */` |
|    3801 |  5343 | `	SyBlobReset(&pGen->sNamespace);` |
|    3801 |  5344 | `	SyHashRelease(&pGen->hUseImports);` |
|    3801 |  5345 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|    3801 |  5346 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|    3801 |  5347 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|    3801 |  5348 | `	SyHashRelease(&pGen->hUseConstImports);` |
|    3801 |  5349 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|    3801 |  5350 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  5351 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 |  5352 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5353 | `		return SXRET_OK;` |
|       - |  5354 | `	}` |
|    3801 |  5355 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - |  5356 | `		/* namespace; — switch to global namespace */` |
|     ! 0 |  5357 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5358 | `		return SXRET_OK;` |
|       - |  5359 | `	}` |
|    3801 |  5360 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - |  5361 | `		/* namespace { } — global namespace block */` |
|     ! 0 |  5362 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5363 | `		return SXRET_OK;` |
|       - |  5364 | `	}` |
|       - |  5365 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|    7639 |  5366 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|    3843 |  5367 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - |  5368 | `			/* Append backslash separator */` |
|      27 |  5369 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      27 |  5370 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      11 |  5371 | `			}` |
|      16 |  5372 | `		}else{` |
|       - |  5373 | `			/* Append identifier */` |
|    3821 |  5374 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  5375 | `		}` |
|    3843 |  5376 | `		pGen->pIn++;` |
|       5 |  5377 | `	}` |
|       - |  5378 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - |  5379 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - |  5380 | `	{` |
|    3801 |  5381 | `		char *zNsDup = 0;` |
|    3801 |  5382 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|    5696 |  5383 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    3794 |  5384 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|    1897 |  5385 | `		}` |
|    3801 |  5386 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - |  5387 | `	}` |
|    3801 |  5388 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       8 |  5389 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  5390 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 |  5391 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       6 |  5392 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5393 | `			return SXERR_ABORT;` |
|       - |  5394 | `		}` |
|       2 |  5395 | `	}` |
|    3801 |  5396 | `	return SXRET_OK;` |
|    1903 |  5397 | `}` |
|       - |  5398 | `/*` |
|       - |  5399 | ` * Compile the 'use' statement` |
|       - |  5400 | ` * According to the PHP language reference manual` |
|       - |  5401 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - |  5402 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - |  5403 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - |  5404 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - |  5405 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - |  5406 | ` *  a function or constant is not supported.` |
|       - |  5407 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - |  5408 | ` * NOTE` |
|       - |  5409 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5410 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5411 | ` */` |
|      68 |  5412 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       5 |  5413 | `{` |
|       - |  5414 | `	sxu32 nLine;` |
|       - |  5415 | `	sxi32 rc;` |
|       - |  5416 | `	SyBlob sPath;` |
|       - |  5417 | `	SyString sAlias;` |
|       - |  5418 | `	SyToken *pLast;` |
|       - |  5419 | `	char *zDup;` |
|       - |  5420 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - |  5421 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - |  5422 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      73 |  5423 | `	nLine = pGen->pIn->nLine;` |
|      73 |  5424 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - |  5425 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      73 |  5426 | `	iUseType = 0;` |
|      73 |  5427 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 |  5428 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 |  5429 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 |  5430 | `			iUseType = 1;` |
|      16 |  5431 | `			pGen->pIn++;` |
|      23 |  5432 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 |  5433 | `			iUseType = 2;` |
|      16 |  5434 | `			pGen->pIn++;` |
|       7 |  5435 | `		}` |
|      14 |  5436 | `	}` |
|       - |  5437 | `	/* Select target hash tables based on import type */` |
|      73 |  5438 | `	switch( iUseType ){` |
|       7 |  5439 | `		case 1:` |
|      16 |  5440 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 |  5441 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 |  5442 | `			break;` |
|       7 |  5443 | `		case 2:` |
|      16 |  5444 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 |  5445 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 |  5446 | `			break;` |
|      20 |  5447 | `		default:` |
|      45 |  5448 | `			pGenHash = &pGen->hUseImports;` |
|      45 |  5449 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      40 |  5450 | `			break;` |
|       - |  5451 | `	}` |
|      73 |  5452 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - |  5453 | `	/* Process one or more use declarations separated by commas */` |
|      35 |  5454 | `	for(;;){` |
|      75 |  5455 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5456 | `			break;` |
|       - |  5457 | `		}` |
|      75 |  5458 | `		SyBlobReset(&sPath);` |
|      75 |  5459 | `		pLast = 0;` |
|       - |  5460 | `		/* Collect the full namespace path */` |
|     261 |  5461 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     191 |  5462 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     131 |  5463 | `				pLast = pGen->pIn;` |
|     131 |  5464 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      65 |  5465 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 |  5466 | `				}` |
|     131 |  5467 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      63 |  5468 | `			}` |
|     191 |  5469 | `			pGen->pIn++;` |
|       5 |  5470 | `		}` |
|      75 |  5471 | `		if( pLast == 0 ){` |
|       - |  5472 | `			/* Empty path */` |
|       5 |  5473 | `			break;` |
|       - |  5474 | `		}` |
|       - |  5475 | `		/* Default alias is the last component of the path */` |
|      71 |  5476 | `		sAlias = pLast->sData;` |
|       - |  5477 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      66 |  5478 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      46 |  5479 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      19 |  5480 | `			pGen->pIn++; /* Jump 'as' */` |
|      19 |  5481 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      19 |  5482 | `				sAlias = pGen->pIn->sData;` |
|      19 |  5483 | `				pGen->pIn++;` |
|       8 |  5484 | `			}` |
|       8 |  5485 | `		}` |
|       - |  5486 | `		/* Check for duplicate import alias (per-type) */` |
|      71 |  5487 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       8 |  5488 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  5489 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 |  5490 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       6 |  5491 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5492 | `				SyBlobRelease(&sPath);` |
|     ! 0 |  5493 | `				return SXERR_ABORT;` |
|       - |  5494 | `			}` |
|       2 |  5495 | `		}` |
|       - |  5496 | `		/* Register the import: alias -> FQN.` |
|       - |  5497 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - |  5498 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - |  5499 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|     104 |  5500 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      66 |  5501 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      71 |  5502 | `		if( zDup ){` |
|      71 |  5503 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      71 |  5504 | `			if( pVmHash ){` |
|       - |  5505 | `				/* Class imports: populate VM table directly (class resolution` |
|       - |  5506 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      43 |  5507 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      43 |  5508 | `				if( zAliasDup ){` |
|      43 |  5509 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      19 |  5510 | `				}` |
|      19 |  5511 | `			}` |
|      71 |  5512 | `			if( iUseType == 2 ){` |
|       - |  5513 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - |  5514 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 |  5515 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 |  5516 | `				if( zAliasDup ){` |
|       - |  5517 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - |  5518 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - |  5519 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 |  5520 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 |  5521 | `					if( azPair ){` |
|      16 |  5522 | `						azPair[0] = zAliasDup;` |
|      16 |  5523 | `						azPair[1] = zDup;` |
|      16 |  5524 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 |  5525 | `					}` |
|       7 |  5526 | `				}` |
|       7 |  5527 | `			}` |
|      33 |  5528 | `		}` |
|       - |  5529 | `		/* Check for comma (multiple use declarations) */` |
|      71 |  5530 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  5531 | `			pGen->pIn++;` |
|       2 |  5532 | `		}else{` |
|      37 |  5533 | `			break;` |
|       - |  5534 | `		}` |
|       1 |  5535 | `	}` |
|      73 |  5536 | `	SyBlobRelease(&sPath);` |
|      73 |  5537 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 |  5538 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 |  5539 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 |  5540 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5541 | `			return SXERR_ABORT;` |
|       - |  5542 | `		}` |
|       1 |  5543 | `	}` |
|      73 |  5544 | `	return SXRET_OK;` |
|      39 |  5545 | `}` |
|       - |  5546 | `/*` |
|       - |  5547 | ` * Compile the stupid 'declare' language construct.` |
|       - |  5548 | ` *` |
|       - |  5549 | ` * According to the PHP language reference manual.` |
|       - |  5550 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - |  5551 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - |  5552 | ` *  declare (directive)` |
|       - |  5553 | ` *   statement` |
|       - |  5554 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - |  5555 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - |  5556 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - |  5557 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - |  5558 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - |  5559 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - |  5560 | ` * <?php` |
|       - |  5561 | ` * // these are the same:` |
|       - |  5562 | ` * // you can use this:` |
|       - |  5563 | ` * declare(ticks=1) {` |
|       - |  5564 | ` *   // entire script here` |
|       - |  5565 | ` * }` |
|       - |  5566 | ` * // or you can use this:` |
|       - |  5567 | ` * declare(ticks=1);` |
|       - |  5568 | ` * // entire script here` |
|       - |  5569 | ` * ?>` |
|       - |  5570 | ` *` |
|       - |  5571 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - |  5572 | ` */` |
|       - |  5573 | `/*` |
|       - |  5574 | ` * Match a directive name against a known literal (case-insensitive).` |
|       - |  5575 | ` */` |
|      68 |  5576 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|       5 |  5577 | `{` |
|     103 |  5578 | `	return SyStringLength(pName) == nWant` |
|      68 |  5579 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|       5 |  5580 | `}` |
|       - |  5581 |  |
|      40 |  5582 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       5 |  5583 | `{` |
|      45 |  5584 | `	sxu32 nLine = pGen->pIn->nLine;` |
|      45 |  5585 | `	SyToken *pBodyEnd = 0;` |
|       - |  5586 | `	SyToken *pBodyStart;` |
|       - |  5587 | `	SyToken *pCursor;` |
|       - |  5588 | `	int bHasStrictTypes;` |
|       - |  5589 | `	int bBlockForm;` |
|       - |  5590 | `	int bPlacementOk;` |
|       - |  5591 | `	sxi32 rc;` |
|      45 |  5592 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|      45 |  5593 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       6 |  5594 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       6 |  5595 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5596 | `			return SXERR_ABORT;` |
|       - |  5597 | `		}` |
|       6 |  5598 | `		goto Synchro;` |
|       - |  5599 | `	}` |
|      41 |  5600 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|      41 |  5601 | `	pBodyStart = pGen->pIn;` |
|       - |  5602 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|      41 |  5603 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|      41 |  5604 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  5605 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 |  5606 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5607 | `			return SXERR_ABORT;` |
|       - |  5608 | `		}` |
|     ! 0 |  5609 | `		return SXRET_OK;` |
|       - |  5610 | `	}` |
|       - |  5611 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|       - |  5612 | `	 * now delimits the comma-separated directive list. */` |
|      41 |  5613 | `	pGen->pIn = &pBodyEnd[1];` |
|      41 |  5614 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|     ! 0 |  5615 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 |  5616 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5617 | `			return SXERR_ABORT;` |
|       - |  5618 | `		}` |
|     ! 0 |  5619 | `	}` |
|      41 |  5620 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|      41 |  5621 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|      41 |  5622 | `	bHasStrictTypes = 0;` |
|       - |  5623 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|       - |  5624 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|       - |  5625 | `	 * directive appears anywhere in the list, before validating values. */` |
|      41 |  5626 | `	pCursor = pBodyStart;` |
|      53 |  5627 | `	while( pCursor < pBodyEnd ){` |
|      49 |  5628 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      41 |  5629 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|      37 |  5630 | `				bHasStrictTypes = 1;` |
|      37 |  5631 | `				break;` |
|       - |  5632 | `			}` |
|       2 |  5633 | `		}` |
|      14 |  5634 | `		pCursor++;` |
|       2 |  5635 | `	}` |
|      41 |  5636 | `	if( bHasStrictTypes && bBlockForm ){` |
|       3 |  5637 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5638 | `			"strict_types declaration must not use block mode");` |
|       3 |  5639 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5640 | `		return SXRET_OK;` |
|       - |  5641 | `	}` |
|      39 |  5642 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|       6 |  5643 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5644 | `			"strict_types declaration must be the very first statement in the script");` |
|       6 |  5645 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       6 |  5646 | `		return SXRET_OK;` |
|       - |  5647 | `	}` |
|       - |  5648 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|      35 |  5649 | `	pCursor = pBodyStart;` |
|      65 |  5650 | `	while( pCursor < pBodyEnd ){` |
|       - |  5651 | `		SyToken *pNameTok;` |
|       - |  5652 | `		SyToken *pEqTok;` |
|       - |  5653 | `		SyToken *pValTok;` |
|       - |  5654 | `		SyString *pDirName;` |
|       - |  5655 | `		int bIsStrict;` |
|       - |  5656 | `		int iStrictValue;` |
|      37 |  5657 | `		pNameTok = pCursor;` |
|      37 |  5658 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5659 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5660 | `				"declare: Expecting a directive name");` |
|     ! 0 |  5661 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5662 | `			return SXRET_OK;` |
|       - |  5663 | `		}` |
|      37 |  5664 | `		pEqTok = pNameTok + 1;` |
|      37 |  5665 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|     ! 0 |  5666 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5667 | `				"declare: Expecting '=' after directive name");` |
|     ! 0 |  5668 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5669 | `			return SXRET_OK;` |
|       - |  5670 | `		}` |
|      37 |  5671 | `		pValTok = pEqTok + 1;` |
|      37 |  5672 | `		if( pValTok >= pBodyEnd ){` |
|     ! 0 |  5673 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5674 | `				"declare: Expecting value after '='");` |
|     ! 0 |  5675 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5676 | `			return SXRET_OK;` |
|       - |  5677 | `		}` |
|      37 |  5678 | `		pDirName = &pNameTok->sData;` |
|      37 |  5679 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|      37 |  5680 | `		if( bIsStrict ){` |
|       - |  5681 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|       - |  5682 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|      33 |  5683 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|     ! 0 |  5684 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5685 | `					"declare(strict_types) value must be a literal");` |
|     ! 0 |  5686 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5687 | `				return SXRET_OK;` |
|       - |  5688 | `			}` |
|      33 |  5689 | `			iStrictValue = -1;` |
|      33 |  5690 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|      33 |  5691 | `				const char *zv = SyStringData(&pValTok->sData);` |
|      33 |  5692 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|      33 |  5693 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|      31 |  5694 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|      14 |  5695 | `			}` |
|      33 |  5696 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|       3 |  5697 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5698 | `					"strict_types declaration must have 0 or 1 as its value");` |
|       3 |  5699 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5700 | `				return SXRET_OK;` |
|       - |  5701 | `			}` |
|      30 |  5702 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|      17 |  5703 | `		}else{` |
|       - |  5704 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|       - |  5705 | `			 * preserve the legacy notice so callers relying on the old` |
|       - |  5706 | `			 * behavior don't regress. */` |
|       8 |  5707 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|       - |  5708 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 |  5709 | `				ph7_lib_version()` |
|       - |  5710 | `				);` |
|       - |  5711 | `		}` |
|      35 |  5712 | `		pCursor = pValTok + 1;` |
|       - |  5713 | `		/* Consume separating comma (or end). */` |
|      35 |  5714 | `		if( pCursor < pBodyEnd ){` |
|       3 |  5715 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5716 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5717 | `					"declare: Expecting ',' or ')' after directive value");` |
|     ! 0 |  5718 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5719 | `				return SXRET_OK;` |
|       - |  5720 | `			}` |
|       3 |  5721 | `			pCursor++;` |
|       1 |  5722 | `		}` |
|       5 |  5723 | `	}` |
|       - |  5724 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|       - |  5725 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|       - |  5726 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|      33 |  5727 | `	return SXRET_OK;` |
|       2 |  5728 | `Synchro:` |
|       - |  5729 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      16 |  5730 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      12 |  5731 | `		pGen->pIn++;` |
|       2 |  5732 | `	}` |
|       6 |  5733 | `	return SXRET_OK;` |
|      25 |  5734 | `}` |
|       - |  5735 | `/*` |
|       - |  5736 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - |  5737 | ` * as follows:` |
|       - |  5738 | ` * function makecoffee($type = "cappuccino")` |
|       - |  5739 | ` * {` |
|       - |  5740 | ` *   return "Making a cup of $type.\n";` |
|       - |  5741 | ` * }` |
|       - |  5742 | ` * Symisc eXtension.` |
|       - |  5743 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - |  5744 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - |  5745 | ` *      Example: Work only with PH7,generate error under zend` |
|       - |  5746 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - |  5747 | ` *      {` |
|       - |  5748 | ` *       var_dump($a);` |
|       - |  5749 | ` *      }` |
|       - |  5750 | ` *     //call test without args` |
|       - |  5751 | ` *      test();` |
|       - |  5752 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - |  5753 | ` *      Example:` |
|       - |  5754 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - |  5755 | ` * 3 -) Function overloading!!` |
|       - |  5756 | ` *      Example:` |
|       - |  5757 | ` *      function foo($a) {` |
|       - |  5758 | ` *   	  return $a.PHP_EOL;` |
|       - |  5759 | ` *	    }` |
|       - |  5760 | ` *	    function foo($a, $b) {` |
|       - |  5761 | ` *   	  return $a + $b;` |
|       - |  5762 | ` *	    }` |
|       - |  5763 | ` *	    echo foo(5); // Prints "5"` |
|       - |  5764 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - |  5765 | ` *      // Same arg` |
|       - |  5766 | ` *	   function foo(string $a)` |
|       - |  5767 | ` *	   {` |
|       - |  5768 | ` *	     echo "a is a string\n";` |
|       - |  5769 | ` *	     var_dump($a);` |
|       - |  5770 | ` *	   }` |
|       - |  5771 | ` *	  function foo(int $a)` |
|       - |  5772 | ` *	  {` |
|       - |  5773 | ` *	    echo "a is integer\n";` |
|       - |  5774 | ` *	    var_dump($a);` |
|       - |  5775 | ` *	  }` |
|       - |  5776 | ` *	  function foo(array $a)` |
|       - |  5777 | ` *	  {` |
|       - |  5778 | ` * 	    echo "a is an array\n";` |
|       - |  5779 | ` * 	    var_dump($a);` |
|       - |  5780 | ` *	  }` |
|       - |  5781 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - |  5782 | ` *	  foo(52); // a is integer [second foo]` |
|       - |  5783 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - |  5784 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - |  5785 | ` * introduced by the PH7 engine.` |
|       - |  5786 | ` */` |
|   77532 |  5787 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       5 |  5788 | `{` |
|       - |  5789 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5790 | `	SySet *pInstrContainer;` |
|       - |  5791 | `	sxi32 rc;` |
|       - |  5792 | `	/* Swap token stream */` |
|   77537 |  5793 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   77537 |  5794 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   77537 |  5795 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - |  5796 | `	/* Compile the expression holding the argument value */` |
|   77537 |  5797 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5798 | `	/* Emit the done instruction */` |
|   77537 |  5799 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   77537 |  5800 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   77537 |  5801 | `	RE_SWAP_DELIMITER(pGen);` |
|   77537 |  5802 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  5803 | `		return SXERR_ABORT;` |
|       - |  5804 | `	}` |
|   77537 |  5805 | `	return SXRET_OK;` |
|   38771 |  5806 | `}` |
|       - |  5807 | `/*` |
|       - |  5808 | ` * Collect function arguments one after one.` |
|       - |  5809 | ` * According to the PHP language reference manual.` |
|       - |  5810 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - |  5811 | ` * list of expressions.` |
|       - |  5812 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - |  5813 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - |  5814 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - |  5815 | ` * for more information.` |
|       - |  5816 | ` * Example #1 Passing arrays to functions` |
|       - |  5817 | ` * <?php` |
|       - |  5818 | ` * function takes_array($input)` |
|       - |  5819 | ` * {` |
|       - |  5820 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - |  5821 | ` * }` |
|       - |  5822 | ` * ?>` |
|       - |  5823 | ` * Making arguments be passed by reference` |
|       - |  5824 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - |  5825 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - |  5826 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - |  5827 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - |  5828 | ` * to the argument name in the function definition:` |
|       - |  5829 | ` * Example #2 Passing function parameters by reference` |
|       - |  5830 | ` * <?php` |
|       - |  5831 | ` * function add_some_extra(&$string)` |
|       - |  5832 | ` * {` |
|       - |  5833 | ` *   $string .= 'and something extra.';` |
|       - |  5834 | ` * }` |
|       - |  5835 | ` * $str = 'This is a string, ';` |
|       - |  5836 | ` * add_some_extra($str);` |
|       - |  5837 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - |  5838 | ` * ?>` |
|       - |  5839 | ` *` |
|       - |  5840 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - |  5841 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - |  5842 | ` * on these extension.` |
|       - |  5843 | ` */` |
|  108418 |  5844 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|       5 |  5845 | `{` |
|       - |  5846 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - |  5847 | `	SyToken *pIn;  /* Token stream */` |
|       - |  5848 | `	SyBlob sSig;         /* Function signature */` |
|       - |  5849 | `	char *zDup;          /* Copy of argument name */` |
|       - |  5850 | `	sxi32 rc;` |
|       - |  5851 |  |
|  108423 |  5852 | `	pIn = pGen->pIn;` |
|  108423 |  5853 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - |  5854 | `	/* Process arguments one after one */` |
|  140155 |  5855 | `	for(;;){` |
|  280315 |  5856 | `		if( pIn >= pEnd ){` |
|       - |  5857 | `			/* No more arguments to process */` |
|  108407 |  5858 | `			break;` |
|       - |  5859 | `		}` |
|  171913 |  5860 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|  171913 |  5861 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|  171913 |  5862 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|  171913 |  5863 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|       - |  5864 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|       - |  5865 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|       - |  5866 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|       - |  5867 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|       - |  5868 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|       - |  5869 | `		{` |
|  171913 |  5870 | `			int bReadonly = 0, bVisSeen = 0;` |
|  171913 |  5871 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|  171913 |  5872 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       3 |  5873 | `				bReadonly = 1;` |
|       3 |  5874 | `				pIn++;` |
|       1 |  5875 | `			}` |
|  171913 |  5876 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|   66659 |  5877 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   66659 |  5878 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      71 |  5879 | `					bVisSeen = 1;` |
|      71 |  5880 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|      95 |  5881 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|      31 |  5882 | `						: PH7_CLASS_PROT_PUBLIC;` |
|      71 |  5883 | `					pIn++;` |
|      71 |  5884 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|      16 |  5885 | `						bReadonly = 1;` |
|      16 |  5886 | `						pIn++;` |
|       6 |  5887 | `					}` |
|      33 |  5888 | `				}` |
|   33327 |  5889 | `			}` |
|  171913 |  5890 | `			if( bVisSeen \|\| bReadonly ){` |
|      73 |  5891 | `				if( !bCtorCtx ){` |
|       6 |  5892 | `					if( bAbstractCtx ){` |
|       3 |  5893 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5894 | `							"Cannot declare promoted property in an abstract constructor");` |
|       2 |  5895 | `					}else{` |
|       3 |  5896 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5897 | `							"Cannot declare promoted property outside a constructor");` |
|       - |  5898 | `					}` |
|       6 |  5899 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  5900 | `						return SXERR_ABORT;` |
|       - |  5901 | `					}` |
|       6 |  5902 | `					return SXERR_SYNTAX;` |
|       - |  5903 | `				}` |
|      69 |  5904 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      69 |  5905 | `				sArg.iPromoteVis = iVis;` |
|      69 |  5906 | `				if( bReadonly ){` |
|      18 |  5907 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|       7 |  5908 | `				}` |
|      32 |  5909 | `			}` |
|       - |  5910 | `		}` |
|       - |  5911 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|  171904 |  5912 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|  130413 |  5913 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   87069 |  5914 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   81497 |  5915 | `			sxu32 nLineLocal = pIn->nLine;` |
|   81497 |  5916 | `			sxi32 iTFlags = 0;` |
|   81497 |  5917 | `			pGen->pIn = pIn;` |
|   81497 |  5918 | `			rc = GenStateParseUnionTypeDecl(` |
|   40746 |  5919 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   40746 |  5920 | `				&iTFlags, &sArg.sTypeName,` |
|       - |  5921 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|       - |  5922 | `				/* bAllowVoid */ 0,` |
|   40746 |  5923 | `						nLineLocal);` |
|   81497 |  5924 | `			pIn = pGen->pIn;` |
|   81497 |  5925 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5926 | `				return SXERR_ABORT;` |
|   81497 |  5927 | `			}else if( rc == SXERR_CORRUPT ){` |
|       - |  5928 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|       3 |  5929 | `				return SXERR_SYNTAX;` |
|   81495 |  5930 | `			}else if( rc == SXERR_SYNTAX ){` |
|      11 |  5931 | `				if( pIn < pEnd ){` |
|      15 |  5932 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|       - |  5933 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|       4 |  5934 | `						&pIn->sData);` |
|       7 |  5935 | `				}else{` |
|     ! 0 |  5936 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|       - |  5937 | `						"syntax error, unexpected end of file");` |
|       - |  5938 | `				}` |
|      11 |  5939 | `				return SXERR_SYNTAX;` |
|       - |  5940 | `			}` |
|   81487 |  5941 | `			sArg.iFlags \|= iTFlags;` |
|   40741 |  5942 | `		}` |
|  171899 |  5943 | `		if( pIn >= pEnd ){` |
|     ! 0 |  5944 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 |  5945 | `			return rc;` |
|       - |  5946 | `		}` |
|  171899 |  5947 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - |  5948 | `			/* Pass by reference,record that */` |
|    3723 |  5949 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    3723 |  5950 | `			pIn++;` |
|    1859 |  5951 | `		}` |
|  171899 |  5952 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - |  5953 | `			/* Variadic parameter: ...$args */` |
|    3739 |  5954 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|    3739 |  5955 | `			pIn++;` |
|    1867 |  5956 | `		}` |
|  171899 |  5957 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  5958 | `			/* Invalid argument */` |
|     ! 0 |  5959 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 |  5960 | `			return rc;` |
|       - |  5961 | `		}` |
|  171899 |  5962 | `		pIn++; /* Jump the dollar sign */` |
|       - |  5963 | `		/* Copy argument name */` |
|  171899 |  5964 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|  171899 |  5965 | `		if( zDup == 0 ){` |
|     ! 0 |  5966 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  5967 | `			return SXERR_ABORT;` |
|       - |  5968 | `		}` |
|  171899 |  5969 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|  171899 |  5970 | `		pIn++;` |
|  171899 |  5971 | `		if( pIn < pEnd ){` |
|  104125 |  5972 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - |  5973 | `				SyToken *pDefend;` |
|   77539 |  5974 | `				sxi32 iNest = 0;` |
|   77539 |  5975 | `				pIn++; /* Jump the equal sign */` |
|   77539 |  5976 | `				pDefend = pIn;` |
|       - |  5977 | `				/* Process the default value associated with this argument */` |
|  162457 |  5978 | `				while( pDefend < pEnd ){` |
|  121833 |  5979 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   36915 |  5980 | `						break;` |
|       - |  5981 | `					}` |
|   84923 |  5982 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - |  5983 | `						/* Increment nesting level */` |
|    3697 |  5984 | `						iNest++;` |
|   83077 |  5985 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - |  5986 | `						/* Decrement nesting level */` |
|    3697 |  5987 | `						iNest--;` |
|    1846 |  5988 | `					}` |
|   84923 |  5989 | `					pDefend++;` |
|       5 |  5990 | `				}` |
|   77539 |  5991 | `				if( pIn >= pDefend ){` |
|       3 |  5992 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 |  5993 | `					return rc;` |
|       - |  5994 | `				}` |
|       - |  5995 | `				/* Process default value */` |
|   77537 |  5996 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   77537 |  5997 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  5998 | `					return rc;` |
|       - |  5999 | `				}` |
|       - |  6000 | `				/* Point beyond the default value */` |
|   77537 |  6001 | `				pIn = pDefend;` |
|   38766 |  6002 | `			}` |
|  104123 |  6003 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  6004 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 |  6005 | `				return rc;` |
|       - |  6006 | `			}` |
|  104123 |  6007 | `			pIn++; /* Jump the trailing comma */` |
|   52059 |  6008 | `		}` |
|       - |  6009 | `		/* Append argument signature */` |
|  171897 |  6010 | `		if( sArg.nType > 0 ){` |
|   81433 |  6011 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - |  6012 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|   14821 |  6013 | `				int marker = 'o';` |
|   14821 |  6014 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|   14821 |  6015 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    7413 |  6016 | `			}else{` |
|       - |  6017 | `				int c;` |
|   66617 |  6018 | `				c = 'n'; /* cc warning */` |
|       - |  6019 | `				/* Type leading character */` |
|   66617 |  6020 | `				switch(sArg.nType){` |
|       3 |  6021 | `				case MEMOBJ_HASHMAP:` |
|       - |  6022 | `					/* Hashmap aka 'array' */` |
|       7 |  6023 | `					c = 'h';` |
|       7 |  6024 | `					break;` |
|    9281 |  6025 | `				case MEMOBJ_INT:` |
|       - |  6026 | `					/* Integer */` |
|   18567 |  6027 | `					c = 'i';` |
|   18567 |  6028 | `					break;` |
|       2 |  6029 | `				case MEMOBJ_BOOL:` |
|       - |  6030 | `					/* Bool */` |
|       5 |  6031 | `					c = 'b';` |
|       5 |  6032 | `					break;` |
|       2 |  6033 | `				case MEMOBJ_REAL:` |
|       - |  6034 | `					/* Float */` |
|       5 |  6035 | `					c = 'f';` |
|       5 |  6036 | `					break;` |
|   24010 |  6037 | `				case MEMOBJ_STRING:` |
|       - |  6038 | `					/* String */` |
|   48025 |  6039 | `					c = 's';` |
|   48025 |  6040 | `					break;` |
|       7 |  6041 | `				case MEMOBJ_OBJ:` |
|       - |  6042 | `					/* Object */` |
|      16 |  6043 | `					c = 'o';` |
|      14 |  6044 | `					break;` |
|       1 |  6045 | `				default:` |
|       2 |  6046 | `					break;` |
|       - |  6047 | `				}` |
|   66617 |  6048 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - |  6049 | `			}` |
|   40719 |  6050 | `		}else{` |
|       - |  6051 | `			/* No type is associated with this parameter which mean` |
|       - |  6052 | `			 * that this function is not condidate for overloading.` |
|       - |  6053 | `			 */` |
|   90469 |  6054 | `			SyBlobRelease(&sSig);` |
|       - |  6055 | `		}` |
|       - |  6056 | `		/* Save in the argument set */` |
|  171897 |  6057 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       5 |  6058 | `	}` |
|  108407 |  6059 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - |  6060 | `		/* Save function signature */` |
|   51879 |  6061 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   25937 |  6062 | `	}` |
|  108407 |  6063 | `	return SXRET_OK;` |
|   54214 |  6064 | `}` |
|       - |  6065 | `/*` |
|       - |  6066 | `` * ROOT C helper: from a `function`/`fn` keyword token, skip past the whole nested`` |
|       - |  6067 | `` * function/closure/arrow body so a `yield` inside it is NOT counted as belonging to`` |
|       - |  6068 | ` * the enclosing function. Returns the token just past the nested construct.` |
|       - |  6069 | ` */` |
|      14 |  6070 | `static SyToken * GenStateSkipNestedFunc(SyToken *pIn, SyToken *pEnd)` |
|       2 |  6071 | `{` |
|      16 |  6072 | `	sxi32 iParen = 0;` |
|      16 |  6073 | `	pIn++; /* past 'function'/'fn' */` |
|       - |  6074 | `	/* Advance to the body's opening '{', ignoring any '{' that could appear inside a` |
|       - |  6075 | ``	 * parenthesised signature (e.g. a `new class {}` parameter default). Stop early on a`` |
|       - |  6076 | `	 * ';' at paren-depth 0 (an abstract/interface method has no body). */` |
|      54 |  6077 | `	while( pIn < pEnd ){` |
|      54 |  6078 | `		sxu32 t = pIn->nType;` |
|      54 |  6079 | `		if( t & PH7_TK_LPAREN ){ iParen++; }` |
|      40 |  6080 | `		else if( t & PH7_TK_RPAREN ){ iParen--; }` |
|      26 |  6081 | `		else if( (t & PH7_TK_OCB) && iParen <= 0 ){ break; }` |
|      12 |  6082 | `		else if( (t & PH7_TK_SEMI) && iParen <= 0 ){ return pIn; }` |
|      40 |  6083 | `		pIn++;` |
|       2 |  6084 | `	}` |
|      16 |  6085 | `	if( pIn >= pEnd ){ return pIn; }` |
|       - |  6086 | `	/* pIn at the body '{' — skip the balanced brace block. */` |
|       - |  6087 | `	{` |
|      16 |  6088 | `		sxi32 d = 0;` |
|     108 |  6089 | `		while( pIn < pEnd ){` |
|     108 |  6090 | `			sxu32 t = pIn->nType;` |
|     108 |  6091 | `			if( t & PH7_TK_OCB ){ d++; }` |
|      94 |  6092 | `			else if( t & PH7_TK_CCB ){ d--; if( d <= 0 ){ pIn++; break; } }` |
|      94 |  6093 | `			pIn++;` |
|       2 |  6094 | `		}` |
|       - |  6095 | `	}` |
|      16 |  6096 | `	return pIn;` |
|       9 |  6097 | `}` |
|       - |  6098 | `/*` |
|       - |  6099 | ` * ROOT C helper: does the function body about to be compiled (pGen->pIn at its opening` |
|       - |  6100 | `` * '{') contain a `yield`/`yield from` at THIS function's own level (i.e. is it a`` |
|       - |  6101 | ` * generator)? Nested function/closure bodies are skipped so their yields don't count.` |
|       - |  6102 | ` * Used to gate inline try/catch/finally compilation: only generators need it (so a` |
|       - |  6103 | `` * `yield` inside a catch/finally can suspend); every other function keeps the legacy`` |
|       - |  6104 | ` * detached-mini-program path untouched.` |
|       - |  6105 | ` */` |
|  231268 |  6106 | `static int GenStateFuncBodyHasYield(ph7_gen_state *pGen)` |
|       5 |  6107 | `{` |
|  231273 |  6108 | `	SyToken *pIn = pGen->pIn;   /* expected at the body's opening '{' */` |
|  231273 |  6109 | `	SyToken *pEnd = pGen->pEnd;` |
|  231273 |  6110 | `	sxi32 iDepth = 0;` |
|  231273 |  6111 | `	int bStarted = 0;` |
| 7681671 |  6112 | `	while( pIn < pEnd ){` |
| 7681671 |  6113 | `		sxu32 t = pIn->nType;` |
| 7681671 |  6114 | `		if( t & PH7_TK_OCB ){ iDepth++; bStarted = 1; pIn++; continue; }` |
| 7239289 |  6115 | `		if( t & PH7_TK_CCB ){ iDepth--; pIn++; if( bStarted && iDepth <= 0 ){ break; } continue; }` |
| 6797123 |  6116 | `		if( t & PH7_TK_KEYWORD ){` |
|  539061 |  6117 | `			int kw = SX_PTR_TO_INT(pIn->pUserData);` |
|  539061 |  6118 | `			if( kw == PH7_TKWRD_YIELD ){ return TRUE; }` |
|  538917 |  6119 | `			if( kw == PH7_TKWRD_FUNCTION ){ pIn = GenStateSkipNestedFunc(pIn,pEnd); continue; }` |
|       - |  6120 | ``			/* `fn` arrow bodies are single expressions and cannot contain a valid yield. */`` |
|  269449 |  6121 | `		}` |
| 6796965 |  6122 | `		pIn++;` |
|       5 |  6123 | `	}` |
|  231129 |  6124 | `	return FALSE;` |
|  115639 |  6125 | `}` |
|       - |  6126 | `/*` |
|       - |  6127 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - |  6128 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  6129 | ` * and this routine takes care of generating the appropriate error message.` |
|       - |  6130 | ` */` |
|  231268 |  6131 | `static sxi32 GenStateCompileFuncBody(` |
|       - |  6132 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  6133 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - |  6134 | `	)` |
|       5 |  6135 | `{` |
|       - |  6136 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - |  6137 | `	GenBlock *pBlock;` |
|       - |  6138 | `	sxu32 nGotoOfft;` |
|       - |  6139 | `	sxi32 rc;` |
|       - |  6140 | `	/* Attach the new function */` |
|  231273 |  6141 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  231273 |  6142 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6143 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - |  6144 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6145 | `		return SXERR_ABORT;` |
|       - |  6146 | `	}` |
|  231273 |  6147 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - |  6148 | `	/* Swap bytecode containers */` |
|  231273 |  6149 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  231273 |  6150 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - |  6151 | `	/* Emit constructor property promotion prologue:` |
|       - |  6152 | `	 *   $this->NAME = $NAME;` |
|       - |  6153 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|       - |  6154 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|       - |  6155 | `	{` |
|  231273 |  6156 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|       - |  6157 | `		sxu32 i;` |
|  373497 |  6158 | `		for( i = 0; i < nArg; i++ ){` |
|  142229 |  6159 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|       - |  6160 | `			char *zSrc;` |
|       - |  6161 | `			sxu32 nSrc,nName;` |
|       - |  6162 | `			SySet sToken;` |
|       - |  6163 | `			SyToken *pTmpIn,*pTmpEnd;` |
|       - |  6164 | `			sxi32 rcPromote;` |
|  142229 |  6165 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  142175 |  6166 | `				continue;` |
|       - |  6167 | `			}` |
|       - |  6168 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|       - |  6169 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|       - |  6170 | `			 * copied), so it must outlive the function — never free it. The` |
|       - |  6171 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|       - |  6172 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      59 |  6173 | `			nName = SyStringLength(&pArg->sName);` |
|      59 |  6174 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      59 |  6175 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      59 |  6176 | `			if( zSrc == 0 ){` |
|     ! 0 |  6177 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  6178 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  6179 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  6180 | `				return SXERR_ABORT;` |
|       - |  6181 | `			}` |
|       - |  6182 | `			{` |
|      59 |  6183 | `				char *z = zSrc;` |
|      59 |  6184 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      59 |  6185 | `				z += sizeof("$this->")-1;` |
|      59 |  6186 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      59 |  6187 | `				z += nName;` |
|      59 |  6188 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      59 |  6189 | `				z += sizeof(" = $")-1;` |
|      59 |  6190 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      59 |  6191 | `				z += nName;` |
|      59 |  6192 | `				*z = 0;` |
|       - |  6193 | `			}` |
|      59 |  6194 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      59 |  6195 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken);` |
|      59 |  6196 | `			pTmpIn = pGen->pIn;` |
|      59 |  6197 | `			pTmpEnd = pGen->pEnd;` |
|      59 |  6198 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      59 |  6199 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      59 |  6200 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      59 |  6201 | `			pGen->pIn = pTmpIn;` |
|      59 |  6202 | `			pGen->pEnd = pTmpEnd;` |
|      59 |  6203 | `			SySetRelease(&sToken);` |
|      59 |  6204 | `			if( rcPromote == SXERR_ABORT ){` |
|     ! 0 |  6205 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  6206 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  6207 | `				return SXERR_ABORT;` |
|       - |  6208 | `			}` |
|       - |  6209 | `			/* Discard the assignment result — this is a statement expression. */` |
|      59 |  6210 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      32 |  6211 | `		}` |
|       - |  6212 | `	}` |
|       - |  6213 | `	/* ROOT C: detect a generator (yield at this function's own level) BEFORE compiling` |
|       - |  6214 | `	 * the body, so try/catch/finally inside it compile inline (yield-in-catch/finally` |
|       - |  6215 | `	 * suspends correctly). Saved/restored so a nested non-generator closure inside a` |
|       - |  6216 | `	 * generator — and vice versa — is classified independently. */` |
|       - |  6217 | `	{` |
|  231273 |  6218 | `		sxi8 bSavedGen = pGen->bInGenerator;` |
|  231273 |  6219 | `		pGen->bInGenerator = (sxi8)GenStateFuncBodyHasYield(&(*pGen));` |
|       - |  6220 | `		/* Compile the body */` |
|  231273 |  6221 | `		PH7_CompileBlock(&(*pGen),0);` |
|  231273 |  6222 | `		pGen->bInGenerator = bSavedGen;` |
|       - |  6223 | `	}` |
|       - |  6224 | `	/* Fix exception jumps now the destination is resolved */` |
|  231273 |  6225 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  6226 | `	/* Emit the final return if not yet done */` |
|  231273 |  6227 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  6228 | `	/* Fix gotos jumps now the destination is resolved */` |
|  231273 |  6229 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 |  6230 | `		rc = SXERR_ABORT;` |
|     ! 0 |  6231 | `	}` |
|  231273 |  6232 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - |  6233 | `	/* Restore the default container */` |
|  231273 |  6234 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  6235 | `	/* Leave function block */` |
|  231273 |  6236 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  231273 |  6237 | `	if( rc == SXERR_ABORT ){` |
|       - |  6238 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6239 | `		return SXERR_ABORT;` |
|       - |  6240 | `	}` |
|       - |  6241 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - |  6242 | `	{` |
|  231273 |  6243 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  6244 | `		sxu32 i;` |
| 4542383 |  6245 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 4311259 |  6246 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     149 |  6247 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     149 |  6248 | `				break;` |
|       - |  6249 | `			}` |
| 2155560 |  6250 | `		}` |
|       - |  6251 | `	}` |
|       - |  6252 | `	/* All done, function body compiled */` |
|  231273 |  6253 | `	return SXRET_OK;` |
|  115639 |  6254 | `}` |
|       - |  6255 | `/*` |
|       - |  6256 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - |  6257 | ` * According to the PHP language reference manual.` |
|       - |  6258 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - |  6259 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - |  6260 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - |  6261 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  6262 | ` *  Functions need not be defined before they are referenced.` |
|       - |  6263 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - |  6264 | ` *  a function even if they were defined inside and vice versa.` |
|       - |  6265 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - |  6266 | ` *  calls with over 32-64 recursion levels.` |
|       - |  6267 | ` *` |
|       - |  6268 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - |  6269 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - |  6270 | ` * on these extension.` |
|       - |  6271 | ` */` |
|       - |  6272 | `/*` |
|       - |  6273 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - |  6274 | ` */` |
|     510 |  6275 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       5 |  6276 | `{` |
|       - |  6277 | `	sxu32 i;` |
|    1453 |  6278 | `	for( i = 0; i < n; i++ ){` |
|    1247 |  6279 | `		int a = zA[i], b = zB[i];` |
|    1247 |  6280 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|    1247 |  6281 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|    1247 |  6282 | `		if( a != b ) return a - b;` |
|     474 |  6283 | `	}` |
|     211 |  6284 | `	return 0;` |
|     260 |  6285 | `}` |
|       - |  6286 | `/*` |
|       - |  6287 | ` * Internal type-atom kinds used during union type parsing.` |
|       - |  6288 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|       - |  6289 | ` * (which are positive bit values stored in sxu32).` |
|       - |  6290 | ` */` |
|       - |  6291 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|       - |  6292 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|       - |  6293 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|       - |  6294 |  |
|       - |  6295 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|       - |  6296 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|       - |  6297 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|       - |  6298 |  |
|       - |  6299 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|       - |  6300 | `struct PhlTypeAtom {` |
|       - |  6301 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|       - |  6302 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|       - |  6303 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|       - |  6304 | `	sxu32 nCanon;` |
|       - |  6305 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|       - |  6306 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|       - |  6307 | `};` |
|       - |  6308 |  |
|       - |  6309 | `/*` |
|       - |  6310 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|       - |  6311 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|       - |  6312 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|       - |  6313 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|       - |  6314 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|       - |  6315 | ` * already be consumed by the caller.` |
|       - |  6316 | ` */` |
|   82358 |  6317 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|       5 |  6318 | `{` |
|   82363 |  6319 | `	SyToken *pIn = pGen->pIn;` |
|   82363 |  6320 | `	SyZero(pOut, sizeof(*pOut));` |
|   82363 |  6321 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   82363 |  6322 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6323 | `		return SXERR_SYNTAX;` |
|       - |  6324 | `	}` |
|       - |  6325 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   82363 |  6326 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       8 |  6327 | `		pIn++;` |
|       8 |  6328 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6329 | `			return SXERR_SYNTAX;` |
|       - |  6330 | `		}` |
|       3 |  6331 | `	}` |
|   82363 |  6332 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6333 | `		return SXERR_SYNTAX;` |
|       - |  6334 | `	}` |
|   82363 |  6335 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|   67171 |  6336 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   67171 |  6337 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      32 |  6338 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   67157 |  6339 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|      71 |  6340 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   67110 |  6341 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|   18827 |  6342 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   57666 |  6343 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   48185 |  6344 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   24165 |  6345 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|      32 |  6346 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|      61 |  6347 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      27 |  6348 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      33 |  6349 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|      10 |  6350 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|      19 |  6351 | `			pOut->nType = SXU32_HIGH;` |
|      19 |  6352 | `			pOut->sClass = pIn->sData;` |
|      11 |  6353 | `		}else{` |
|       3 |  6354 | `			return SXERR_SYNTAX;` |
|       - |  6355 | `		}` |
|   67169 |  6356 | `		pIn++;` |
|   33587 |  6357 | `	}else{` |
|       - |  6358 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|       - |  6359 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|   15197 |  6360 | `		SyString *pT = &pIn->sData;` |
|   15197 |  6361 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|      32 |  6362 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|      32 |  6363 | `			pIn++;` |
|   15183 |  6364 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|     157 |  6365 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|     157 |  6366 | `			pIn++;` |
|   15093 |  6367 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|      24 |  6368 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|      24 |  6369 | `			pIn++;` |
|      14 |  6370 | `		}else{` |
|       - |  6371 | `			/* Class / interface name; consume namespace path a\b\c */` |
|   14997 |  6372 | `			SyToken *pFirst = pIn;` |
|   14997 |  6373 | `			SyToken *pLast = pIn;` |
|   14997 |  6374 | `			pOut->nType = SXU32_HIGH;` |
|   14997 |  6375 | `			pOut->sClass = pIn->sData;` |
|   14997 |  6376 | `			pIn++;` |
|   22491 |  6377 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|   15000 |  6378 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       3 |  6379 | `				pLast = &pIn[1];` |
|       3 |  6380 | `				pIn += 2;` |
|       1 |  6381 | `			}` |
|   14997 |  6382 | `			if( pLast != pFirst ){` |
|       3 |  6383 | `				const char *zFirst = pFirst->sData.zString;` |
|       3 |  6384 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 |  6385 | `				pOut->sClass.zString = zFirst;` |
|       3 |  6386 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       1 |  6387 | `			}` |
|       - |  6388 | `		}` |
|       - |  6389 | `	}` |
|   82361 |  6390 | `	pGen->pIn = pIn;` |
|   82361 |  6391 | `	return SXRET_OK;` |
|   41184 |  6392 | `}` |
|       - |  6393 |  |
|       - |  6394 | `/*` |
|       - |  6395 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|       - |  6396 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|       - |  6397 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|       - |  6398 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|       - |  6399 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|       - |  6400 | ` */` |
|   82198 |  6401 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|       5 |  6402 | `{` |
|       - |  6403 | `	int i;` |
|   82203 |  6404 | `	int nNonNull = 0;` |
|   82203 |  6405 | `	int bAnyIntersection = 0;` |
|       - |  6406 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|   82203 |  6407 | `	sxu32 nMaxGroup = 0;` |
| 2712539 |  6408 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|  164535 |  6409 | `	for( i = 0; i < nAtoms; i++ ){` |
|   82337 |  6410 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   82309 |  6411 | `			nNonNull++;` |
|   82309 |  6412 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|   82309 |  6413 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|   82309 |  6414 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|   41152 |  6415 | `			}` |
|   41152 |  6416 | `		}` |
|   41171 |  6417 | `	}` |
|  164501 |  6418 | `	for( i = 0; i < nAtoms; i++ ){` |
|   82319 |  6419 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|      20 |  6420 | `			bAnyIntersection = 1;` |
|      20 |  6421 | `			break;` |
|       - |  6422 | `		}` |
|   41154 |  6423 | `	}` |
|   82203 |  6424 | `	if( bAnyIntersection ){` |
|       - |  6425 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|       - |  6426 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|       - |  6427 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|      20 |  6428 | `		sxu32 g, nGroups = 0;` |
|      20 |  6429 | `		int bFirstGroup = 1;` |
|      40 |  6430 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|      40 |  6431 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|      24 |  6432 | `			int bFirstMember = 1;` |
|       - |  6433 | `			int bWrap;` |
|      24 |  6434 | `			if( aGroupCount[g] == 0 ) continue;` |
|       - |  6435 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|       - |  6436 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|       - |  6437 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|       - |  6438 | `			 * parens, matching PHP's canonical text. */` |
|      32 |  6439 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|      24 |  6440 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|      24 |  6441 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|      72 |  6442 | `			for( i = 0; i < nAtoms; i++ ){` |
|      52 |  6443 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|      40 |  6444 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|      40 |  6445 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      38 |  6446 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      21 |  6447 | `				}else{` |
|       3 |  6448 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6449 | `				}` |
|      40 |  6450 | `				bFirstMember = 0;` |
|      22 |  6451 | `			}` |
|      24 |  6452 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|      24 |  6453 | `			bFirstGroup = 0;` |
|      14 |  6454 | `		}` |
|      20 |  6455 | `		if( bNullable ){` |
|     ! 0 |  6456 | `			SyBlobAppend(pBlob, "\|", 1);` |
|     ! 0 |  6457 | `			SyBlobAppend(pBlob, "null", 4);` |
|     ! 0 |  6458 | `		}` |
|      58 |  6459 | `		return;` |
|       - |  6460 | `	}` |
|   82187 |  6461 | `	if( nNonNull == 1 && bNullable ){` |
|       - |  6462 | `		/* Shorthand: ?T */` |
|      81 |  6463 | `		for( i = 0; i < nAtoms; i++ ){` |
|      81 |  6464 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      81 |  6465 | `			SyBlobAppend(pBlob, "?", 1);` |
|      81 |  6466 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      21 |  6467 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      12 |  6468 | `			}else{` |
|      63 |  6469 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6470 | `			}` |
|      81 |  6471 | `			return;` |
|     ! 0 |  6472 | `		}` |
|     ! 0 |  6473 | `	}` |
|       - |  6474 | `	{` |
|   82111 |  6475 | `		int bFirst = 1;` |
|       - |  6476 | `		/* 1) Classes in declaration order */` |
|  164319 |  6477 | `		for( i = 0; i < nAtoms; i++ ){` |
|   82213 |  6478 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|   14961 |  6479 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   14961 |  6480 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|   14961 |  6481 | `				bFirst = 0;` |
|    7478 |  6482 | `			}` |
|   41109 |  6483 | `		}` |
|       - |  6484 | `		/* 2) Built-ins in canonical order */` |
|       - |  6485 | `		{` |
|       - |  6486 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|       - |  6487 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|       - |  6488 | `			int k;` |
|  574747 |  6489 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  918709 |  6490 | `				for( i = 0; i < nAtoms; i++ ){` |
|  493145 |  6491 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|   67077 |  6492 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   67077 |  6493 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|   67077 |  6494 | `						bFirst = 0;` |
|   67077 |  6495 | `						break;` |
|       - |  6496 | `					}` |
|  213039 |  6497 | `				}` |
|  246323 |  6498 | `			}` |
|       - |  6499 | `		}` |
|       - |  6500 | `		/* 3) null suffix */` |
|   82111 |  6501 | `		if( bNullable ){` |
|      20 |  6502 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|      20 |  6503 | `			SyBlobAppend(pBlob, "null", 4);` |
|       8 |  6504 | `		}` |
|       - |  6505 | `	}` |
|   41104 |  6506 | `}` |
|       - |  6507 |  |
|       - |  6508 | `/*` |
|       - |  6509 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|       - |  6510 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|       - |  6511 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|       - |  6512 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|       - |  6513 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|       - |  6514 | ` * whether it was parenthesized.` |
|       - |  6515 | ` *` |
|       - |  6516 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|       - |  6517 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|       - |  6518 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|       - |  6519 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|       - |  6520 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|       - |  6521 | ` */` |
|   82340 |  6522 | `static sxi32 GenStateParsePart(` |
|       - |  6523 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|       - |  6524 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|       5 |  6525 | `{` |
|       - |  6526 | `	sxi32 rc;` |
|   82345 |  6527 | `	int nMembers = 0;` |
|   82345 |  6528 | `	int bParen = 0;` |
|   82345 |  6529 | `	*pnMembers = 0;` |
|   82345 |  6530 | `	*pbParen = 0;` |
|   82345 |  6531 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       6 |  6532 | `		bParen = 1;` |
|       6 |  6533 | `		pGen->pIn++; /* skip '(' */` |
|       2 |  6534 | `	}` |
|   41170 |  6535 | `	for(;;){` |
|   82363 |  6536 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|     ! 0 |  6537 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6538 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|     ! 0 |  6539 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6540 | `		}` |
|   82363 |  6541 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|   82363 |  6542 | `		if( rc != SXRET_OK ){` |
|       3 |  6543 | `			return rc;` |
|       - |  6544 | `		}` |
|   82361 |  6545 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|   82361 |  6546 | `		(*pnAtoms)++;` |
|   82361 |  6547 | `		nMembers++;` |
|       - |  6548 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|   82361 |  6549 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      24 |  6550 | `			SyToken *pNext = &pGen->pIn[1];` |
|      20 |  6551 | `			if( pNext < pGen->pEnd` |
|      24 |  6552 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      22 |  6553 | `				pGen->pIn++; /* skip '&' */` |
|      22 |  6554 | `				continue;` |
|       - |  6555 | `			}` |
|       1 |  6556 | `		}` |
|   82343 |  6557 | `		break;` |
|     ! 0 |  6558 | `	}` |
|   82343 |  6559 | `	if( bParen ){` |
|       6 |  6560 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6561 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6562 | `				"Malformed DNF type: expecting ')'");` |
|     ! 0 |  6563 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6564 | `		}` |
|       6 |  6565 | `		pGen->pIn++; /* skip ')' */` |
|       6 |  6566 | `		if( nMembers < 2 ){` |
|     ! 0 |  6567 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6568 | `				"Parenthesized type must be an intersection of at least two types");` |
|     ! 0 |  6569 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6570 | `		}` |
|       2 |  6571 | `	}` |
|   82343 |  6572 | `	*pnMembers = nMembers;` |
|   82343 |  6573 | `	*pbParen = bParen;` |
|   82343 |  6574 | `	return SXRET_OK;` |
|   41175 |  6575 | `}` |
|       - |  6576 |  |
|       - |  6577 | `/*` |
|       - |  6578 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|       - |  6579 | ` *` |
|       - |  6580 | ` * Outputs:` |
|       - |  6581 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|       - |  6582 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|       - |  6583 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|       - |  6584 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|       - |  6585 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|       - |  6586 | ` *     already be initialized by the caller (allocator set, etc).` |
|       - |  6587 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|       - |  6588 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|       - |  6589 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|       - |  6590 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|       - |  6591 | ` *` |
|       - |  6592 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|       - |  6593 | ` * SXERR_ABORT on fatal compile errors.` |
|       - |  6594 | ` */` |
|   82214 |  6595 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |  6596 | `	ph7_gen_state *pGen,` |
|       - |  6597 | `	sxu32 *pnType,` |
|       - |  6598 | `	SyString *pClass,` |
|       - |  6599 | `	SySet *pAlts,` |
|       - |  6600 | `	sxi32 *piTypeFlags,` |
|       - |  6601 | `	SyString *pTypeText,` |
|       - |  6602 | `	int iNullableFlag,` |
|       - |  6603 | `	int iUnionFlag,` |
|       - |  6604 | `	int bAllowVoid,` |
|       - |  6605 | `	sxu32 nLine` |
|       5 |  6606 | `){` |
|       - |  6607 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   82219 |  6608 | `	int nAtoms = 0;` |
|   82219 |  6609 | `	int bShortNullable = 0;` |
|   82219 |  6610 | `	int bExplicitNull = 0;` |
|       - |  6611 | `	sxi32 rc;` |
|   82219 |  6612 | `	*pnType = 0;` |
|   82219 |  6613 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   82219 |  6614 | `	*piTypeFlags = 0;` |
|   82219 |  6615 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|       - |  6616 |  |
|   82219 |  6617 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6618 | `		return SXRET_OK;` |
|       - |  6619 | `	}` |
|       - |  6620 | ``	/* Optional `?` shorthand prefix */`` |
|   82214 |  6621 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      71 |  6622 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      71 |  6623 | `		bShortNullable = 1;` |
|      71 |  6624 | `		pGen->pIn++;` |
|      71 |  6625 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6626 | `			return SXERR_SYNTAX;` |
|       - |  6627 | `		}` |
|      33 |  6628 | `	}` |
|       - |  6629 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|       - |  6630 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|       - |  6631 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|       - |  6632 | `	{` |
|       - |  6633 | `		int nMembers, bParen;` |
|   82219 |  6634 | `		sxu32 iGroup = 0;` |
|   82219 |  6635 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|   82219 |  6636 | `		if( rc != SXRET_OK ){` |
|       4 |  6637 | `			return rc;` |
|       - |  6638 | `		}` |
|       - |  6639 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|       - |  6640 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|       - |  6641 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|       - |  6642 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|       - |  6643 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|  123509 |  6644 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   82408 |  6645 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|     133 |  6646 | `			if( bShortNullable ){` |
|       - |  6647 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|       - |  6648 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|       - |  6649 | `				 * already reported" so callers skip their own error emission. */` |
|       3 |  6650 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6651 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|       3 |  6652 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|       - |  6653 | `			}` |
|     131 |  6654 | `			if( nMembers >= 2 && !bParen ){` |
|     ! 0 |  6655 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|       - |  6656 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|     ! 0 |  6657 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6658 | `			}` |
|     131 |  6659 | ``			pGen->pIn++; /* skip `\|` */`` |
|     131 |  6660 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|     131 |  6661 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  6662 | `				return rc;` |
|       - |  6663 | `			}` |
|       5 |  6664 | `		}` |
|   82215 |  6665 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|     ! 0 |  6666 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6667 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|     ! 0 |  6668 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6669 | `		}` |
|       - |  6670 | `	}` |
|       - |  6671 | `	/* Validation pass.` |
|       - |  6672 | `	 *` |
|       - |  6673 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|       - |  6674 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|       - |  6675 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|       - |  6676 | `	 */` |
|       - |  6677 | `	{` |
|       - |  6678 | `		int i, j;` |
|   82215 |  6679 | `		int bHasNonNull = 0;` |
|   82215 |  6680 | `		int bAnyIntersection = 0;` |
|       - |  6681 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|       - |  6682 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|       - |  6683 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
| 2712935 |  6684 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|  164569 |  6685 | `		for( i = 0; i < nAtoms; i++ ){` |
|   82359 |  6686 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|   41182 |  6687 | `		}` |
|  164531 |  6688 | `		for( i = 0; i < nAtoms; i++ ){` |
|   82339 |  6689 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|   41163 |  6690 | `		}` |
|       - |  6691 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|       - |  6692 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|   82215 |  6693 | `		if( bShortNullable && bAnyIntersection ){` |
|     ! 0 |  6694 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6695 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|     ! 0 |  6696 | `			return SXERR_SYNTAX;` |
|       - |  6697 | `		}` |
|  164555 |  6698 | `		for( i = 0; i < nAtoms; i++ ){` |
|       - |  6699 | `			/* Intersection members must be class/interface types (PHP rejects` |
|       - |  6700 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|       - |  6701 | ``			 * `true`/`false` in an intersection). */`` |
|   82357 |  6702 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|      38 |  6703 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|      38 |  6704 | `				if( bClassLike ){` |
|      36 |  6705 | `					SyString *pC = &aAtoms[i].sClass;` |
|      32 |  6706 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|      32 |  6707 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|      32 |  6708 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|      36 |  6709 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|     ! 0 |  6710 | `						bClassLike = 0;` |
|     ! 0 |  6711 | `					}` |
|      16 |  6712 | `				}` |
|      38 |  6713 | `				if( !bClassLike ){` |
|       - |  6714 | `					const char *zName; sxu32 nName;` |
|       3 |  6715 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6716 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6717 | `					}else{` |
|       3 |  6718 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|       - |  6719 | `					}` |
|       4 |  6720 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6721 | `						"Type %.*s cannot be part of an intersection type",` |
|       1 |  6722 | `						(int)nName, zName);` |
|       3 |  6723 | `					return SXERR_SYNTAX;` |
|       - |  6724 | `				}` |
|      16 |  6725 | `			}` |
|   82355 |  6726 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|     157 |  6727 | `				if( nAtoms > 1 ){` |
|       3 |  6728 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6729 | `						"Void can only be used as a standalone type");` |
|       3 |  6730 | `					return SXERR_SYNTAX;` |
|       - |  6731 | `				}` |
|     155 |  6732 | `				if( !bAllowVoid ){` |
|     ! 0 |  6733 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6734 | `						"void cannot be used here");` |
|     ! 0 |  6735 | `					return SXERR_SYNTAX;` |
|       - |  6736 | `				}` |
|     155 |  6737 | `				if( bShortNullable ){` |
|     ! 0 |  6738 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6739 | `						"Void type cannot be nullable");` |
|     ! 0 |  6740 | `					return SXERR_SYNTAX;` |
|       - |  6741 | `				}` |
|      75 |  6742 | `			}` |
|   82353 |  6743 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|       - |  6744 | ``				/* `never` is a bottom type usable only as a standalone RETURN`` |
|       - |  6745 | `				 * type (never = the function does not return). Mirrors the void` |
|       - |  6746 | `				 * validation above; accepted here and enforced at compile time` |
|       - |  6747 | ``				 * (explicit `return` banned) and run time (fall-off TypeError). */`` |
|      24 |  6748 | `				if( nAtoms > 1 \|\| bShortNullable ){` |
|       - |  6749 | ``					/* `?never` is `never\|null`, a union — PHP reports it the`` |
|       - |  6750 | `					 * same as any other non-standalone use. */` |
|       5 |  6751 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6752 | `						"never can only be used as a standalone type");` |
|       5 |  6753 | `					return SXERR_SYNTAX;` |
|       - |  6754 | `				}` |
|      19 |  6755 | `				if( !bAllowVoid ){` |
|       - |  6756 | `					/* Return-only: params call with bAllowVoid=0. */` |
|       3 |  6757 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6758 | `						"never cannot be used as a parameter type");` |
|       3 |  6759 | `					return SXERR_SYNTAX;` |
|       - |  6760 | `				}` |
|       7 |  6761 | `			}` |
|   82347 |  6762 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|      32 |  6763 | `				bExplicitNull = 1;` |
|      18 |  6764 | `			}else{` |
|   82319 |  6765 | `				bHasNonNull = 1;` |
|       - |  6766 | `			}` |
|       - |  6767 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|       - |  6768 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|       - |  6769 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|       - |  6770 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|       - |  6771 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|   82527 |  6772 | `			for( j = 0; j < i; j++ ){` |
|     187 |  6773 | `				int bDup = 0;` |
|     187 |  6774 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|     359 |  6775 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|     182 |  6776 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|     187 |  6777 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|     179 |  6778 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      41 |  6779 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      34 |  6780 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|      38 |  6781 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|      16 |  6782 | `								aAtoms[j].sClass.zString,` |
|      32 |  6783 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|     ! 0 |  6784 | `							bDup = 1;` |
|     ! 0 |  6785 | `						}` |
|      22 |  6786 | `					}else{` |
|       3 |  6787 | `						bDup = 1;` |
|       - |  6788 | `					}` |
|      18 |  6789 | `				}` |
|     179 |  6790 | `				if( bDup ){` |
|       - |  6791 | `					const char *zName;` |
|       - |  6792 | `					sxu32 nName;` |
|       3 |  6793 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6794 | `						zName = aAtoms[i].sClass.zString;` |
|     ! 0 |  6795 | `						nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6796 | `					}else{` |
|       3 |  6797 | `						zName = aAtoms[i].zCanon;` |
|       3 |  6798 | `						nName = aAtoms[i].nCanon;` |
|       - |  6799 | `					}` |
|       4 |  6800 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       1 |  6801 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|       3 |  6802 | `					return SXERR_SYNTAX;` |
|       - |  6803 | `				}` |
|      91 |  6804 | `			}` |
|   41175 |  6805 | `		}` |
|   82203 |  6806 | `		if( !bHasNonNull && bExplicitNull ){` |
|       7 |  6807 | `			if( bShortNullable ){` |
|       - |  6808 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|     ! 0 |  6809 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6810 | `					"Null can not be used as a standalone type");` |
|     ! 0 |  6811 | `				return SXERR_SYNTAX;` |
|       - |  6812 | `			}` |
|       - |  6813 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|       - |  6814 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|       - |  6815 | `			 * path below leaves *pnType untouched when there is no non-null` |
|       - |  6816 | `			 * atom, so set it here. */` |
|       7 |  6817 | `			*pnType = MEMOBJ_NULL;` |
|       3 |  6818 | `		}` |
|       - |  6819 | `	}` |
|       - |  6820 | `	/* Compute nullability flag */` |
|   82203 |  6821 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      97 |  6822 | `		*piTypeFlags \|= iNullableFlag;` |
|      46 |  6823 | `	}` |
|       - |  6824 | `	/* Build canonical type text */` |
|   82203 |  6825 | `	if( pTypeText ){` |
|       - |  6826 | `		SyBlob sBlob;` |
|   82203 |  6827 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|  123270 |  6828 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   41099 |  6829 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   82203 |  6830 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|  123056 |  6831 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   82034 |  6832 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   82039 |  6833 | `			if( zDup ){` |
|   82039 |  6834 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   41017 |  6835 | `			}` |
|   41017 |  6836 | `		}` |
|   82203 |  6837 | `		SyBlobRelease(&sBlob);` |
|   41099 |  6838 | `	}` |
|       - |  6839 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|       - |  6840 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|       - |  6841 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|       - |  6842 | `	{` |
|   82203 |  6843 | `		int nNonNull = 0;` |
|   82203 |  6844 | `		int iNonNullIdx = -1;` |
|       - |  6845 | `		int i;` |
|  164535 |  6846 | `		for( i = 0; i < nAtoms; i++ ){` |
|   82337 |  6847 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   82309 |  6848 | `				nNonNull++;` |
|   82309 |  6849 | `				iNonNullIdx = i;` |
|   41152 |  6850 | `			}` |
|   41171 |  6851 | `		}` |
|   82203 |  6852 | `		if( nNonNull <= 1 ){` |
|       - |  6853 | `			/* Fast path: store as single type. */` |
|   82111 |  6854 | `			if( iNonNullIdx >= 0 ){` |
|   82105 |  6855 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   82105 |  6856 | `				if( pA->nType == SXU32_HIGH ){` |
|   22406 |  6857 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    7467 |  6858 | `						pA->sClass.zString, pA->sClass.nByte);` |
|   14939 |  6859 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|   14939 |  6860 | `					*pnType = SXU32_HIGH;` |
|   14939 |  6861 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   74638 |  6862 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|     155 |  6863 | `					*pnType = MEMOBJ_VOID;` |
|   67096 |  6864 | `				}else if( pA->nType == UTA_NEVER_FLAG ){` |
|      16 |  6865 | `					*pnType = MEMOBJ_NEVER;` |
|       9 |  6866 | `				}else{` |
|   67007 |  6867 | `					*pnType = pA->nType;` |
|       - |  6868 | `				}` |
|   41050 |  6869 | `			}` |
|   41058 |  6870 | `		}else{` |
|       - |  6871 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      97 |  6872 | `			*piTypeFlags \|= iUnionFlag;` |
|     311 |  6873 | `			for( i = 0; i < nAtoms; i++ ){` |
|       - |  6874 | `				ph7_type_alt sAlt;` |
|     219 |  6875 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     209 |  6876 | `				SyZero(&sAlt, sizeof(sAlt));` |
|     209 |  6877 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|     209 |  6878 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|     116 |  6879 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      37 |  6880 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      79 |  6881 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      79 |  6882 | `					sAlt.nType = SXU32_HIGH;` |
|      79 |  6883 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      42 |  6884 | `				}else{` |
|     135 |  6885 | `					sAlt.nType = aAtoms[i].nType;` |
|     135 |  6886 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|       - |  6887 | `				}` |
|     209 |  6888 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|     107 |  6889 | `			}` |
|       - |  6890 | `		}` |
|       - |  6891 | `	}` |
|   82203 |  6892 | `	return SXRET_OK;` |
|   41112 |  6893 | `}` |
|       - |  6894 |  |
|       - |  6895 | `/*` |
|       - |  6896 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - |  6897 | `` * pGen->pIn should point to the token after `)`.`` |
|       - |  6898 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - |  6899 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - |  6900 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|       - |  6901 | `` *          and union types `: T\|U`.`` |
|       - |  6902 | ` */` |
|  327452 |  6903 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       5 |  6904 | `{` |
|  327457 |  6905 | `	sxi32 iFlags = 0;` |
|       - |  6906 | `	sxi32 rc;` |
|       - |  6907 | `	sxu32 nLine;` |
|  327457 |  6908 | `	pFunc->nReturnType = 0;` |
|  327457 |  6909 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  327457 |  6910 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|  327457 |  6911 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  326957 |  6912 | `		return SXRET_OK;` |
|       - |  6913 | `	}` |
|     505 |  6914 | `	pGen->pIn++; /* Skip ':' */` |
|     505 |  6915 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6916 | `		return SXRET_OK;` |
|       - |  6917 | `	}` |
|     505 |  6918 | `	nLine = pGen->pIn->nLine;` |
|     505 |  6919 | `	rc = GenStateParseUnionTypeDecl(` |
|     250 |  6920 | `		pGen,` |
|     250 |  6921 | `		&pFunc->nReturnType,` |
|     250 |  6922 | `		&pFunc->sReturnClass,` |
|     250 |  6923 | `		&pFunc->aReturnUnion,` |
|       - |  6924 | `		&iFlags,` |
|     250 |  6925 | `		&pFunc->sReturnTypeName,` |
|       - |  6926 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|       - |  6927 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|       - |  6928 | `		/* iUnionFlag */ 0,` |
|       - |  6929 | `		/* bAllowVoid */ 1,` |
|     250 |  6930 | `		nLine);` |
|     505 |  6931 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6932 | `		return SXERR_ABORT;` |
|       - |  6933 | `	}` |
|     505 |  6934 | `	if( rc == SXERR_CORRUPT ){` |
|       - |  6935 | `		/* Error already reported */` |
|     ! 0 |  6936 | `		return SXERR_SYNTAX;` |
|       - |  6937 | `	}` |
|     505 |  6938 | `	if( rc == SXERR_SYNTAX ){` |
|       8 |  6939 | `		if( pGen->pIn < pGen->pEnd ){` |
|      11 |  6940 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6941 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       6 |  6942 | `				&pGen->pIn->sData);` |
|       5 |  6943 | `		}else{` |
|     ! 0 |  6944 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|       - |  6945 | `				"syntax error, unexpected end of file in return type declaration");` |
|       - |  6946 | `		}` |
|       8 |  6947 | `		return SXERR_SYNTAX;` |
|       - |  6948 | `	}` |
|     499 |  6949 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     499 |  6950 | `	return SXRET_OK;` |
|  163731 |  6951 | `}` |
|       - |  6952 |  |
|   49394 |  6953 | `static sxi32 GenStateCompileFunc(` |
|       - |  6954 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  6955 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - |  6956 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  6957 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - |  6958 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - |  6959 | `	)` |
|       5 |  6960 | `{` |
|       - |  6961 | `	ph7_vm_func *pFunc;` |
|       - |  6962 | `	SyToken *pEnd;` |
|       - |  6963 | `	sxu32 nLine;` |
|       - |  6964 | `	char *zName;` |
|       - |  6965 | `	sxi32 rc;` |
|       - |  6966 | `	/* Extract line number */` |
|   49399 |  6967 | `	nLine = pGen->pIn->nLine;` |
|       - |  6968 | `	/* Jump the left parenthesis '(' */` |
|   49399 |  6969 | `	pGen->pIn++;` |
|       - |  6970 | `	/* Delimit the function signature */` |
|   49399 |  6971 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   49399 |  6972 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  6973 | `		/* Syntax error */` |
|       8 |  6974 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       8 |  6975 | `		if( rc == SXERR_ABORT ){` |
|       - |  6976 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6977 | `			return SXERR_ABORT;` |
|       - |  6978 | `		}` |
|       8 |  6979 | `		pGen->pIn = pGen->pEnd;` |
|       8 |  6980 | `		return SXRET_OK;` |
|       - |  6981 | `	}` |
|       - |  6982 | `	/* Create the function state */` |
|   49393 |  6983 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   49393 |  6984 | `	if( pFunc == 0 ){` |
|     ! 0 |  6985 | `		goto OutOfMem;` |
|       - |  6986 | `	}` |
|       - |  6987 | `	/* Build the function name, prepending namespace if active */` |
|   49400 |  6988 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - |  6989 | `		SyBlob sFQN;` |
|       - |  6990 | `		sxu32 nLen;` |
|      16 |  6991 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 |  6992 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 |  6993 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 |  6994 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 |  6995 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 |  6996 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 |  6997 | `		SyBlobRelease(&sFQN);` |
|      16 |  6998 | `		if( zName == 0 ){` |
|     ! 0 |  6999 | `			goto OutOfMem;` |
|       - |  7000 | `		}` |
|      16 |  7001 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 |  7002 | `	}else{` |
|   49379 |  7003 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   49379 |  7004 | `		if( zName == 0 ){` |
|     ! 0 |  7005 | `			goto OutOfMem;` |
|       - |  7006 | `		}` |
|   49379 |  7007 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - |  7008 | `	}` |
|   49393 |  7009 | `	if( pGen->pIn < pEnd ){` |
|       - |  7010 | `		/* Collect function arguments */` |
|   34053 |  7011 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   34053 |  7012 | `		if( rc == SXERR_ABORT ){` |
|       - |  7013 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  7014 | `			return SXERR_ABORT;` |
|       - |  7015 | `		}` |
|   17024 |  7016 | `	}` |
|       - |  7017 | `	/* Point past ')' and parse optional return type ': type' */` |
|   49393 |  7018 | `	pGen->pIn = &pEnd[1];` |
|       - |  7019 | `	{` |
|   49393 |  7020 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   49393 |  7021 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  7022 | `			return SXERR_ABORT;` |
|   49393 |  7023 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       8 |  7024 | `			return SXERR_SYNTAX;` |
|       - |  7025 | `		}` |
|       - |  7026 | `	}` |
|   49387 |  7027 | `	if( bHandleClosure ){` |
|       - |  7028 | `		ph7_vm_func_closure_env sEnv;` |
|     299 |  7029 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     294 |  7030 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     161 |  7031 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      23 |  7032 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  7033 | `				/* Closure,record environment variable */` |
|      23 |  7034 | `				pGen->pIn++;` |
|      23 |  7035 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  7036 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 |  7037 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7038 | `						return SXERR_ABORT;` |
|       - |  7039 | `					}` |
|     ! 0 |  7040 | `				}` |
|      23 |  7041 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - |  7042 | `				/* Compile until we hit the first closing parenthesis */` |
|      45 |  7043 | `				while( pGen->pIn < pGen->pEnd ){` |
|      45 |  7044 | `					int iFlagsLocal = 0;` |
|      45 |  7045 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      23 |  7046 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      23 |  7047 | `						break;` |
|       - |  7048 | `					}` |
|      27 |  7049 | `					nLineLocal = pGen->pIn->nLine;` |
|      27 |  7050 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - |  7051 | `						/* Pass by reference,record that */` |
|     ! 0 |  7052 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - |  7053 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - |  7054 | `							);` |
|     ! 0 |  7055 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 |  7056 | `						pGen->pIn++;` |
|     ! 0 |  7057 | `					}` |
|      22 |  7058 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      27 |  7059 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  7060 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  7061 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 |  7062 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  7063 | `								return SXERR_ABORT;` |
|       - |  7064 | `							}` |
|       - |  7065 | `							/* Find the closing parenthesis */` |
|     ! 0 |  7066 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  7067 | `								pGen->pIn++;` |
|     ! 0 |  7068 | `							}` |
|     ! 0 |  7069 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 |  7070 | `								pGen->pIn++;` |
|     ! 0 |  7071 | `							}` |
|     ! 0 |  7072 | `							break;` |
|       - |  7073 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 |  7074 | `					}else{` |
|       - |  7075 | `						SyString *pNameLocal;` |
|       - |  7076 | `						char *zDup;` |
|       - |  7077 | `						/* Duplicate variable name */` |
|      27 |  7078 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      27 |  7079 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      27 |  7080 | `						if( zDup ){` |
|       - |  7081 | `							/* Zero the structure */` |
|      27 |  7082 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      27 |  7083 | `							sEnv.iFlags = iFlagsLocal;` |
|      27 |  7084 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      27 |  7085 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      27 |  7086 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 |  7087 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 |  7088 | `									got_this = 1;` |
|     ! 0 |  7089 | `							}` |
|       - |  7090 | `							/* Save imported variable */` |
|      27 |  7091 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      16 |  7092 | `						}else{` |
|     ! 0 |  7093 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7094 | `							 return SXERR_ABORT;` |
|       - |  7095 | `						}` |
|       - |  7096 | `					}` |
|      27 |  7097 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      33 |  7098 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7099 | `						/* Ignore trailing commas */` |
|       7 |  7100 | `						pGen->pIn++;` |
|       1 |  7101 | `					}` |
|       5 |  7102 | `				}` |
|      23 |  7103 | `				if( !got_this ){` |
|       - |  7104 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - |  7105 | `					 * available to the closure environment.` |
|       - |  7106 | `					 */` |
|      23 |  7107 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      23 |  7108 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      23 |  7109 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      23 |  7110 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      23 |  7111 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       9 |  7112 | `				}` |
|      23 |  7113 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - |  7114 | `					/* Mark as closure */` |
|      23 |  7115 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       9 |  7116 | `				}` |
|       9 |  7117 | `		}` |
|     147 |  7118 | `	}` |
|       - |  7119 | `	/* Compile the body */` |
|   49387 |  7120 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   49387 |  7121 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7122 | `		return SXERR_ABORT;` |
|       - |  7123 | `	}` |
|   49387 |  7124 | `	if( ppFunc ){` |
|     299 |  7125 | `		*ppFunc = pFunc;` |
|     147 |  7126 | `	}` |
|   49387 |  7127 | `	rc = SXRET_OK;` |
|   49387 |  7128 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - |  7129 | `		/* Finally register the function */` |
|   49369 |  7130 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   24682 |  7131 | `	}` |
|   49387 |  7132 | `	if( rc == SXRET_OK ){` |
|   49387 |  7133 | `		return SXRET_OK;` |
|       - |  7134 | `	}` |
|       - |  7135 | `	/* Fall through if something goes wrong */` |
|     ! 0 |  7136 | `OutOfMem:` |
|       - |  7137 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  7138 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  7139 | `	 */` |
|     ! 0 |  7140 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  7141 | `	return SXERR_ABORT;` |
|   24702 |  7142 | `}` |
|       - |  7143 | `/*` |
|       - |  7144 | ` * Compile a standard PHP function.` |
|       - |  7145 | ` *  Refer to the block-comment above for more information.` |
|       - |  7146 | ` */` |
|   49108 |  7147 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       5 |  7148 | `{` |
|       - |  7149 | `	SyString *pName;` |
|       - |  7150 | `	sxi32 iFlags;` |
|       - |  7151 | `	sxu32 nLine;` |
|       - |  7152 | `	sxi32 rc;` |
|       - |  7153 |  |
|   49113 |  7154 | `	nLine = pGen->pIn->nLine;` |
|   49113 |  7155 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   49113 |  7156 | `	iFlags = 0;` |
|   49113 |  7157 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  7158 | `		/* Return by reference,remember that */` |
|       7 |  7159 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  7160 | `		/* Jump the '&' token */` |
|       7 |  7161 | `		pGen->pIn++;` |
|       3 |  7162 | `	}` |
|   49113 |  7163 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  7164 | `		/* Invalid function name */` |
|       8 |  7165 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       8 |  7166 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7167 | `			return SXERR_ABORT;` |
|       - |  7168 | `		}` |
|       - |  7169 | `		/* Sychronize with the next semi-colon or braces*/` |
|      22 |  7170 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      16 |  7171 | `			pGen->pIn++;` |
|       2 |  7172 | `		}` |
|       8 |  7173 | `		return SXRET_OK;` |
|       - |  7174 | `	}` |
|   49107 |  7175 | `	pName = &pGen->pIn->sData;` |
|   49107 |  7176 | `	nLine = pGen->pIn->nLine;` |
|       - |  7177 | `	/* Jump the function name */` |
|   49107 |  7178 | `	pGen->pIn++;` |
|   49107 |  7179 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  7180 | `		/* Syntax error */` |
|       3 |  7181 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 |  7182 | `		if( rc == SXERR_ABORT ){` |
|       - |  7183 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7184 | `			return SXERR_ABORT;` |
|       - |  7185 | `		}` |
|       - |  7186 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 |  7187 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  7188 | `			pGen->pIn++;` |
|     ! 0 |  7189 | `		}` |
|       3 |  7190 | `		return SXRET_OK;` |
|       - |  7191 | `	}` |
|       - |  7192 | `	/* Compile function body */` |
|   49105 |  7193 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   49105 |  7194 | `	return rc;` |
|   24559 |  7195 | `}` |
|       - |  7196 | `/*` |
|       - |  7197 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  7198 | ` * According to the PHP language reference manual` |
|       - |  7199 | ` *  Visibility:` |
|       - |  7200 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  7201 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  7202 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  7203 | ` *  Members declared protected can be accessed only within the class` |
|       - |  7204 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  7205 | ` *  may only be accessed by the class that defines the member.` |
|       - |  7206 | ` */` |
|  356180 |  7207 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       5 |  7208 | `{` |
|  356185 |  7209 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|   22255 |  7210 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  333935 |  7211 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   48029 |  7212 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  7213 | `	}` |
|       - |  7214 | `	/* Assume public by default */` |
|  285911 |  7215 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  178095 |  7216 | `}` |
|       - |  7217 | `/*` |
|       - |  7218 | ` * Compile a class constant.` |
|       - |  7219 | ` * According to the PHP language reference manual` |
|       - |  7220 | ` *  Class Constants` |
|       - |  7221 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - |  7222 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - |  7223 | ` *   you don't use the $ symbol to declare or use them.` |
|       - |  7224 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - |  7225 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - |  7226 | ` *   It's also possible for interfaces to have constants.` |
|       - |  7227 | ` * Symisc eXtension.` |
|       - |  7228 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - |  7229 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  7230 | ` *  Example:` |
|       - |  7231 | ` *   class Test{` |
|       - |  7232 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  7233 | ` *   };` |
|       - |  7234 | ` *   var_dump(TEST::MyConst);` |
|       - |  7235 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  7236 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  7237 | ` */` |
|       - |  7238 | `/*` |
|       - |  7239 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|       - |  7240 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|       - |  7241 | ` * token immediately followed by '='. Anything else with a leading type token` |
|       - |  7242 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|       - |  7243 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|       - |  7244 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|       - |  7245 | ` */` |
|      94 |  7246 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|       5 |  7247 | `{` |
|       - |  7248 | `	SyToken *p0, *p1;` |
|      99 |  7249 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7250 | `		return 0;` |
|       - |  7251 | `	}` |
|      99 |  7252 | `	p0 = pGen->pIn;` |
|       - |  7253 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|      99 |  7254 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|     ! 0 |  7255 | `		return 1;` |
|       - |  7256 | `	}` |
|      99 |  7257 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|       5 |  7258 | `		return 1;` |
|       - |  7259 | `	}` |
|       - |  7260 | `	/* A name-like first token begins a type only when followed by another` |
|       - |  7261 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|       - |  7262 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|      95 |  7263 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      95 |  7264 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|      95 |  7265 | `		if( p1 ){` |
|      95 |  7266 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|      34 |  7267 | `				return 1;` |
|       - |  7268 | `			}` |
|      64 |  7269 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|       5 |  7270 | `				return 1;` |
|       - |  7271 | `			}` |
|      28 |  7272 | `		}` |
|      28 |  7273 | `	}` |
|      60 |  7274 | `	return 0;` |
|      52 |  7275 | `}` |
|       - |  7276 | `/*` |
|       - |  7277 | ` * TRUE when the class-constant initializer starting at pGen->pIn is a bare real` |
|       - |  7278 | `` * literal (e.g. `1.0`, `-1.0`, `2.0e3`), optionally preceded by unary sign(s).`` |
|       - |  7279 | `` * Used to reject `const int X = 1.0` at compile time: PHL's number model tags a`` |
|       - |  7280 | ` * whole-valued real MEMOBJ_REAL\|MEMOBJ_INT, so the runtime flag test would wrongly` |
|       - |  7281 | ` * accept it as an int. The literal shape is the only reliable signal that separates` |
|       - |  7282 | `` * the invalid `1.0` from the valid `4/2` (a computed whole-real PHP accepts as int).`` |
|       - |  7283 | ` * Peek only; never consumes tokens.` |
|       - |  7284 | ` */` |
|      24 |  7285 | `static int GenStateConstInitIsRealLiteral(ph7_gen_state *pGen)` |
|       4 |  7286 | `{` |
|      28 |  7287 | `	SyToken *p = pGen->pIn;` |
|      39 |  7288 | `	while( p < pGen->pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      20 |  7289 | `		&& (p->sData.zString[0] == '-' \|\| p->sData.zString[0] == '+') ){` |
|       3 |  7290 | `		p++; /* skip leading unary sign(s) */` |
|       1 |  7291 | `	}` |
|      28 |  7292 | `	if( p >= pGen->pEnd \|\| (p->nType & PH7_TK_REAL) == 0 ){` |
|      23 |  7293 | `		return 0; /* not a real literal (int literal, cast, call, ...) */` |
|       - |  7294 | `	}` |
|       6 |  7295 | `	p++;` |
|       - |  7296 | `	/* Must be the WHOLE initializer: the next token ends this constant. */` |
|       6 |  7297 | `	return ( p >= pGen->pEnd \|\| (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ) ? 1 : 0;` |
|      16 |  7298 | `}` |
|       - |  7299 | `/*` |
|       - |  7300 | `` * TRUE if the operator token *p is one of `::` / `->` / `?->` (member access).`` |
|       - |  7301 | `` * A `new` that immediately follows one of these is a member name (`A::new`,`` |
|       - |  7302 | `` * `$o->new`), not a `new` expression.`` |
|       - |  7303 | ` */` |
|       6 |  7304 | `static int GenStateTokenIsMemberOp(const SyToken *p)` |
|       3 |  7305 | `{` |
|       - |  7306 | `	sxi32 iOp;` |
|       9 |  7307 | `	if( (p->nType & PH7_TK_OP) == 0 \|\| p->pUserData == 0 ){` |
|     ! 0 |  7308 | `		return 0;` |
|       - |  7309 | `	}` |
|       9 |  7310 | `	iOp = ((const ph7_expr_op *)p->pUserData)->iOp;` |
|       9 |  7311 | `	return ( iOp == EXPR_OP_DC \|\| iOp == EXPR_OP_ARROW \|\| iOp == EXPR_OP_NULLSAFE_ARROW );` |
|       6 |  7312 | `}` |
|       - |  7313 | `/*` |
|       - |  7314 | ``  * Return TRUE if the initializer starting at the current token contains a `new` `` |
|       - |  7315 | `` * expression anywhere before it ends. PHP 8.5 forbids `new` in class-constant,`` |
|       - |  7316 | ` * interface-constant and (instance/static) property-default initializers` |
|       - |  7317 | ` * ("New expressions are not supported in this context") while still allowing it` |
|       - |  7318 | ` * in global constants, parameter defaults and static-local initializers (which` |
|       - |  7319 | ` * are compiled by different functions and left untouched). The scan is` |
|       - |  7320 | `` * bracket-depth aware so a nested `new` (e.g. `[new X()]`, `cond ? new X() : y`)`` |
|       - |  7321 | ` * is still caught and an inner comma does not end the scan prematurely; only a` |
|       - |  7322 | `` * `,` / `;` at depth 0 terminates the initializer.`` |
|       - |  7323 | ` *` |
|       - |  7324 | `` * A `new` inside a nested closure / arrow-function is NOT part of this constant`` |
|       - |  7325 | ` * expression (it runs when the closure is later invoked), so PHP permits it — a` |
|       - |  7326 | `` * `static function(){ return new X(); }` is a valid constant expression. The scan`` |
|       - |  7327 | `` * therefore skips over any `function`/`fn` construct rather than descending into`` |
|       - |  7328 | `` * it. A `new` used as a member name (`A::new`) is likewise ignored.`` |
|       - |  7329 | ` */` |
|   22732 |  7330 | `static int GenStateInitHasNewExpr(ph7_gen_state *pGen)` |
|       5 |  7331 | `{` |
|   22737 |  7332 | `	SyToken *p = pGen->pIn;` |
|   22737 |  7333 | `	int iDepth = 0;` |
|   68409 |  7334 | `	while( p < pGen->pEnd ){` |
|   68409 |  7335 | `		if( iDepth == 0 && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|   22729 |  7336 | `			break; /* end of this initializer */` |
|       - |  7337 | `		}` |
|   45680 |  7338 | `		if( (p->nType & PH7_TK_KEYWORD)` |
|   22850 |  7339 | `			&& ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FUNCTION` |
|      10 |  7340 | `				\|\| SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN ) ){` |
|       - |  7341 | `			/* Skip the whole closure/arrow-fn (signature defaults + body): any` |
|       - |  7342 | ``			 * `new` in there is deferred to call time, not part of this const`` |
|       - |  7343 | `			 * expression. */` |
|       3 |  7344 | `			int bArrow = ( SX_PTR_TO_INT(p->pUserData) == PH7_TKWRD_FN );` |
|       3 |  7345 | `			p++;` |
|       3 |  7346 | `			if( bArrow ){` |
|       - |  7347 | `				/* fn(params) => expr : skip to the end of the current element (a` |
|       - |  7348 | ``				 * `,`/`;` or a bracket closing an enclosing group, at base depth). */`` |
|       3 |  7349 | `				int iBase = iDepth;` |
|      17 |  7350 | `				while( p < pGen->pEnd ){` |
|      17 |  7351 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       5 |  7352 | `						iDepth++;` |
|      15 |  7353 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       5 |  7354 | `						if( iDepth <= iBase ){` |
|     ! 0 |  7355 | `							break; /* closes an enclosing group, not the fn's own */` |
|       - |  7356 | `						}` |
|       5 |  7357 | `						iDepth--;` |
|      11 |  7358 | `					}else if( iDepth <= iBase && (p->nType & (PH7_TK_SEMI\|PH7_TK_COMMA)) ){` |
|       3 |  7359 | `						break;` |
|       - |  7360 | `					}` |
|      15 |  7361 | `					p++;` |
|       1 |  7362 | `				}` |
|       2 |  7363 | `			}else{` |
|       - |  7364 | `				/* function(params)[use(...)][: type] { body } : skip the signature` |
|       - |  7365 | `				 * up to the body '{' (a '{' at closure-local depth 0, so a` |
|       - |  7366 | ``				 * `new class{}` default inside the parens is not mistaken for it),`` |
|       - |  7367 | `				 * then skip the balanced brace block. */` |
|     ! 0 |  7368 | `				int iLocal = 0;` |
|     ! 0 |  7369 | `				while( p < pGen->pEnd ){` |
|     ! 0 |  7370 | `					if( iLocal == 0 && (p->nType & PH7_TK_OCB) ){` |
|     ! 0 |  7371 | `						break; /* body brace */` |
|       - |  7372 | `					}` |
|     ! 0 |  7373 | `					if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  7374 | `						iLocal++;` |
|     ! 0 |  7375 | `					}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  7376 | `						if( iLocal > 0 ){` |
|     ! 0 |  7377 | `							iLocal--;` |
|     ! 0 |  7378 | `						}` |
|     ! 0 |  7379 | `					}` |
|     ! 0 |  7380 | `					p++;` |
|     ! 0 |  7381 | `				}` |
|     ! 0 |  7382 | `				if( p < pGen->pEnd ){` |
|     ! 0 |  7383 | `					int iBrace = 0; /* p is on the body '{' */` |
|     ! 0 |  7384 | `					while( p < pGen->pEnd ){` |
|     ! 0 |  7385 | `						if( p->nType & PH7_TK_OCB ){` |
|     ! 0 |  7386 | `							iBrace++;` |
|     ! 0 |  7387 | `						}else if( p->nType & PH7_TK_CCB ){` |
|     ! 0 |  7388 | `							iBrace--;` |
|     ! 0 |  7389 | `							if( iBrace == 0 ){` |
|     ! 0 |  7390 | `								p++;` |
|     ! 0 |  7391 | `								break;` |
|       - |  7392 | `							}` |
|     ! 0 |  7393 | `						}` |
|     ! 0 |  7394 | `						p++;` |
|     ! 0 |  7395 | `					}` |
|     ! 0 |  7396 | `				}` |
|       - |  7397 | `			}` |
|       3 |  7398 | `			continue;` |
|       - |  7399 | `		}` |
|   45683 |  7400 | `		if( p->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      69 |  7401 | `			iDepth++;` |
|   45651 |  7402 | `		}else if( p->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      67 |  7403 | `			if( iDepth > 0 ){` |
|      67 |  7404 | `				iDepth--;` |
|      31 |  7405 | `			}` |
|   45588 |  7406 | `		}else if( (p->nType & PH7_TK_OP) && p->pUserData` |
|   22713 |  7407 | `			&& ((const ph7_expr_op *)p->pUserData)->iOp == EXPR_OP_NEW ){` |
|       - |  7408 | ``			/* `new` is lexed as an alpha-stream operator (PH7_TK_ID\|PH7_TK_OP)`` |
|       - |  7409 | `			 * whose pUserData is the operator instance, not a keyword id. Ignore a` |
|       - |  7410 | ``			 * `new` used as a member name (`A::new`/`$o->new`). */`` |
|      11 |  7411 | `			if( p == pGen->pIn \|\| !GenStateTokenIsMemberOp(&p[-1]) ){` |
|      11 |  7412 | `				return 1;` |
|       - |  7413 | `			}` |
|     ! 0 |  7414 | `		}` |
|   45675 |  7415 | `		p++;` |
|       5 |  7416 | `	}` |
|   22729 |  7417 | `	return 0;` |
|   11371 |  7418 | `}` |
|       - |  7419 | `/*` |
|       - |  7420 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|       - |  7421 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|       - |  7422 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|       - |  7423 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|       - |  7424 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|       - |  7425 | ` * share the same backing.` |
|       - |  7426 | ` */` |
|     212 |  7427 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|       - |  7428 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|       5 |  7429 | `{` |
|     217 |  7430 | `	pAttr->nType = nType;` |
|     217 |  7431 | `	pAttr->sClass = *pClass;` |
|     217 |  7432 | `	pAttr->sTypeName = *pTypeName;` |
|     217 |  7433 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7434 | `		sxu32 i;` |
|      66 |  7435 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      46 |  7436 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|      46 |  7437 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      25 |  7438 | `		}` |
|      10 |  7439 | `	}` |
|     217 |  7440 | `}` |
|      94 |  7441 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7442 | `{` |
|      99 |  7443 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7444 | `	SySet *pInstrContainer;` |
|       - |  7445 | `	ph7_class_attr *pCons;` |
|       - |  7446 | `	SyString *pName;` |
|       - |  7447 | `	sxi32 rc;` |
|      99 |  7448 | `	sxu32 nType = 0;` |
|       - |  7449 | `	SyString sTypeClass;` |
|       - |  7450 | `	SyString sTypeText;` |
|       - |  7451 | `	SySet aUnionAlts;` |
|      99 |  7452 | `	sxi32 iTypeFlags = 0;` |
|      99 |  7453 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|      99 |  7454 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|      99 |  7455 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7456 | `	/* Extract visibility level */` |
|      99 |  7457 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7458 | `	/* Mark as constant */` |
|      99 |  7459 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      99 |  7460 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       - |  7461 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|       - |  7462 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|     118 |  7463 | `	if( GenStateClassConstHasType(pGen) ){` |
|      61 |  7464 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|      38 |  7465 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|       - |  7466 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|       - |  7467 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|       - |  7468 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|       - |  7469 | `		 * and success paths release. */` |
|      42 |  7470 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7471 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7472 | `			goto Synchronize;` |
|      42 |  7473 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7474 | `			return SXERR_ABORT;` |
|      42 |  7475 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7476 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  7477 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|     ! 0 |  7478 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7479 | `				return SXERR_ABORT;` |
|       - |  7480 | `			}` |
|     ! 0 |  7481 | `			goto Synchronize;` |
|       - |  7482 | `		}` |
|      42 |  7483 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      19 |  7484 | `	}` |
|      47 |  7485 | `loop:` |
|     101 |  7486 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7487 | `		/* Invalid constant name */` |
|     ! 0 |  7488 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  7489 | `		if( rc == SXERR_ABORT ){` |
|       - |  7490 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7491 | `			return SXERR_ABORT;` |
|       - |  7492 | `		}` |
|     ! 0 |  7493 | `		goto Synchronize;` |
|       - |  7494 | `	}` |
|       - |  7495 | `	/* Peek constant name */` |
|     101 |  7496 | `	pName = &pGen->pIn->sData;` |
|       - |  7497 | `	/* Make sure the constant name isn't reserved */` |
|     101 |  7498 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  7499 | `		/* Reserved constant name */` |
|     ! 0 |  7500 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 |  7501 | `		if( rc == SXERR_ABORT ){` |
|       - |  7502 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7503 | `			return SXERR_ABORT;` |
|       - |  7504 | `		}` |
|     ! 0 |  7505 | `		goto Synchronize;` |
|       - |  7506 | `	}` |
|       - |  7507 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|     101 |  7508 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      61 |  7509 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|      38 |  7510 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      19 |  7511 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|      42 |  7512 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7513 | `			return SXERR_ABORT;` |
|      42 |  7514 | `		}else if( rc != SXRET_OK ){` |
|       3 |  7515 | `			goto Synchronize;` |
|       - |  7516 | `		}` |
|      18 |  7517 | `	}` |
|       - |  7518 | `	/* Advance the stream cursor */` |
|      99 |  7519 | `	pGen->pIn++;` |
|      99 |  7520 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  7521 | `		/* Invalid declaration */` |
|     ! 0 |  7522 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  7523 | `		if( rc == SXERR_ABORT ){` |
|       - |  7524 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7525 | `			return SXERR_ABORT;` |
|       - |  7526 | `		}` |
|     ! 0 |  7527 | `		goto Synchronize;` |
|       - |  7528 | `	}` |
|      99 |  7529 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  7530 | ``	/* PHP 8.3: a bare float literal cannot initialize an `int` typed constant`` |
|       - |  7531 | ``	 * (`const int X = 1.0`). Runtime flag-testing can't distinguish it from the valid`` |
|       - |  7532 | ``	 * `const int X = 4/2` (both whole-reals in PHL's number model), so reject the`` |
|       - |  7533 | `	 * literal shape here, at definition time, matching PHP's eager fatal. */` |
|      94 |  7534 | `	if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) && !(iTypeFlags & PH7_CLASS_ATTR_UNION)` |
|      39 |  7535 | `		&& nType == MEMOBJ_INT && GenStateConstInitIsRealLiteral(pGen) ){` |
|       8 |  7536 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7537 | `			"Cannot use float as value for class constant %z::%z of type %z",` |
|       2 |  7538 | `			&pClass->sName,pName,&sTypeText);` |
|       6 |  7539 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7540 | `			return SXERR_ABORT;` |
|       - |  7541 | `		}` |
|       6 |  7542 | `		goto Synchronize;` |
|       - |  7543 | `	}` |
|       - |  7544 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a class/interface`` |
|       - |  7545 | `	 * constant initializer ("New expressions are not supported in this context").` |
|       - |  7546 | `	 * Reject it at definition time, matching PHP's compile-time fatal. */` |
|      95 |  7547 | `	if( GenStateInitHasNewExpr(pGen) ){` |
|       5 |  7548 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7549 | `			"New expressions are not supported in this context");` |
|       5 |  7550 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7551 | `			return SXERR_ABORT;` |
|       - |  7552 | `		}` |
|       5 |  7553 | `		goto Synchronize;` |
|       - |  7554 | `	}` |
|       - |  7555 | `	/* Allocate a new class attribute */` |
|      91 |  7556 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|      91 |  7557 | `	if( pCons == 0 ){` |
|     ! 0 |  7558 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7559 | `		return SXERR_ABORT;` |
|       - |  7560 | `	}` |
|      91 |  7561 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      35 |  7562 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      16 |  7563 | `	}` |
|       - |  7564 | `	/* Swap bytecode container */` |
|      91 |  7565 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      91 |  7566 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  7567 | `	/* Compile constant value.` |
|       - |  7568 | `	 */` |
|      91 |  7569 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      91 |  7570 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  7571 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  7572 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7573 | `			return SXERR_ABORT;` |
|       - |  7574 | `		}` |
|       1 |  7575 | `	}` |
|       - |  7576 | `	/* Emit the done instruction */` |
|      91 |  7577 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      91 |  7578 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      91 |  7579 | `	if( rc == SXERR_ABORT ){` |
|       - |  7580 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  7581 | `		return SXERR_ABORT;` |
|       - |  7582 | `	}` |
|       - |  7583 | `	/* All done,install the constant */` |
|      91 |  7584 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      91 |  7585 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7586 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7587 | `		return SXERR_ABORT;` |
|       - |  7588 | `	}` |
|      91 |  7589 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7590 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|       3 |  7591 | `		pGen->pIn++; /* Jump the comma */` |
|       3 |  7592 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  7593 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7594 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7595 | `				pTok--;` |
|     ! 0 |  7596 | `			}` |
|     ! 0 |  7597 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7598 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  7599 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7600 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7601 | `				return SXERR_ABORT;` |
|       - |  7602 | `			}` |
|     ! 0 |  7603 | `		}else{` |
|       3 |  7604 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       3 |  7605 | `				goto loop;` |
|       - |  7606 | `			}` |
|       - |  7607 | `		}` |
|     ! 0 |  7608 | `	}` |
|      89 |  7609 | `	SySetRelease(&aUnionAlts);` |
|      89 |  7610 | `	return SXRET_OK;` |
|       5 |  7611 | `Synchronize:` |
|      13 |  7612 | `	SySetRelease(&aUnionAlts);` |
|       - |  7613 | `	/* Synchronize with the first semi-colon */` |
|      45 |  7614 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      35 |  7615 | `		pGen->pIn++;` |
|       3 |  7616 | `	}` |
|      13 |  7617 | `	return SXERR_CORRUPT;` |
|      52 |  7618 | `}` |
|       - |  7619 | `/*` |
|       - |  7620 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  7621 | ` * According to the PHP language reference manual` |
|       - |  7622 | ` *  Properties` |
|       - |  7623 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  7624 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  7625 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  7626 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  7627 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  7628 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  7629 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  7630 | ` * Symisc eXtension.` |
|       - |  7631 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  7632 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  7633 | ` *  Example:` |
|       - |  7634 | ` *   class Test{` |
|       - |  7635 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  7636 | ` *   };` |
|       - |  7637 | ` *   var_dump(TEST::myVar);` |
|       - |  7638 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  7639 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  7640 | ` */` |
|       - |  7641 | `/*` |
|       - |  7642 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  7643 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  7644 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  7645 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  7646 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  7647 | ` */` |
|  193004 |  7648 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       5 |  7649 | `{` |
|  193009 |  7650 | `	SyToken *p = pStart;` |
|  193009 |  7651 | `	int bFirst = 1;` |
|  193009 |  7652 | `	if( p >= pEnd ) return 0;` |
|       - |  7653 | ``	/* Optional nullable `?` shorthand. */`` |
|  193009 |  7654 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      19 |  7655 | `		p++;` |
|      19 |  7656 | `		if( p >= pEnd ) return 0;` |
|       8 |  7657 | `	}` |
|       - |  7658 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|       - |  7659 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|       - |  7660 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|       - |  7661 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   96502 |  7662 | `	for(;;){` |
|  193027 |  7663 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|       - |  7664 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|       3 |  7665 | `			p++;` |
|       9 |  7666 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|       3 |  7667 | `			if( p >= pEnd ) return 0;` |
|       3 |  7668 | `			p++; /* skip ')' */` |
|       2 |  7669 | `		}else{` |
|       - |  7670 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|       - |  7671 | ``			 * then any `&`-joined intersection members. */`` |
|  193025 |  7672 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|  193025 |  7673 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  7674 | `				return 0;` |
|       - |  7675 | `			}` |
|       - |  7676 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|       - |  7677 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|       - |  7678 | `			 * may still appear at the initial dispatch site). */` |
|  193025 |  7679 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|  192979 |  7680 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  192974 |  7681 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|   11292 |  7682 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  192825 |  7683 | `					return 0;` |
|       - |  7684 | `				}` |
|      77 |  7685 | `			}` |
|     205 |  7686 | `			p++;` |
|     207 |  7687 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  7688 | `				p += 2;` |
|       1 |  7689 | `			}` |
|     303 |  7690 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|     208 |  7691 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  7692 | `				p++; /* skip '&' */` |
|       3 |  7693 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|       3 |  7694 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|       3 |  7695 | `				p++;` |
|       3 |  7696 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  7697 | `					p += 2;` |
|     ! 0 |  7698 | `				}` |
|       1 |  7699 | `			}` |
|       - |  7700 | `		}` |
|     207 |  7701 | `		bFirst = 0;` |
|     202 |  7702 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      23 |  7703 | `			&& p->sData.zString[0] == '\|' ){` |
|      22 |  7704 | ``			p++; /* next `\|`-separated part */`` |
|      22 |  7705 | `			continue;` |
|       - |  7706 | `		}` |
|     189 |  7707 | `		break;` |
|     ! 0 |  7708 | `	}` |
|     189 |  7709 | `	if( p >= pEnd ) return 0;` |
|     189 |  7710 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   96507 |  7711 | `}` |
|       - |  7712 |  |
|       - |  7713 | `/*` |
|       - |  7714 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  7715 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  7716 | ` * if not). Recognized forms:` |
|       - |  7717 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  7718 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  7719 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  7720 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  7721 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  7722 | ` * on unrecoverable error.` |
|       - |  7723 | ` *` |
|       - |  7724 | ` * When a type is parsed:` |
|       - |  7725 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  7726 | ` *   *pClass is set to the class name (for class types)` |
|       - |  7727 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  7728 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  7729 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  7730 | ` */` |
|     184 |  7731 | `static sxi32 GenStateParsePropertyType(` |
|       - |  7732 | `	ph7_gen_state *pGen,` |
|       - |  7733 | `	sxu32 *pnType,` |
|       - |  7734 | `	SyString *pClass,` |
|       - |  7735 | `	sxi32 *piTypeFlags,` |
|       - |  7736 | `	SyString *pTypeText,` |
|       - |  7737 | `	SySet *pAlts` |
|       5 |  7738 | `){` |
|     189 |  7739 | `	sxi32 iFlags = 0;` |
|       - |  7740 | `	sxi32 rc;` |
|     189 |  7741 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7742 | `		return SXRET_OK;` |
|       - |  7743 | `	}` |
|       - |  7744 | `	/* If the first token is '$', there's no type */` |
|     189 |  7745 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  7746 | `		return SXRET_OK;` |
|       - |  7747 | `	}` |
|     189 |  7748 | `	rc = GenStateParseUnionTypeDecl(` |
|      92 |  7749 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  7750 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  7751 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  7752 | `		/* bAllowVoid */ 0,` |
|     184 |  7753 | `		pGen->pIn->nLine);` |
|     189 |  7754 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7755 | `		return rc;` |
|       - |  7756 | `	}` |
|       - |  7757 | `	/* Verify next token is '$' (start of property name) */` |
|     189 |  7758 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7759 | `		return SXERR_SYNTAX;` |
|       - |  7760 | `	}` |
|     189 |  7761 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     189 |  7762 | `	return SXRET_OK;` |
|      97 |  7763 | `}` |
|       - |  7764 |  |
|       - |  7765 | `/*` |
|       - |  7766 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - |  7767 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - |  7768 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - |  7769 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - |  7770 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - |  7771 | ` * by the type parser itself before reaching here.` |
|       - |  7772 | ` *` |
|       - |  7773 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - |  7774 | ` * use in the error message.` |
|       - |  7775 | ` */` |
|     336 |  7776 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - |  7777 | `	sxu32 nType,` |
|       - |  7778 | `	const SyString *pClass,` |
|       - |  7779 | `	const char **pzName,` |
|       - |  7780 | `	sxu32 *pnName)` |
|       5 |  7781 | `{` |
|       - |  7782 | `	const char *z;` |
|       - |  7783 | `	sxu32 n;` |
|     341 |  7784 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     287 |  7785 | `		return 0;` |
|       - |  7786 | `	}` |
|      58 |  7787 | `	z = pClass->zString;` |
|      58 |  7788 | `	n = pClass->nByte;` |
|      58 |  7789 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       8 |  7790 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - |  7791 | `	}` |
|       - |  7792 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|       - |  7793 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|       - |  7794 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|      52 |  7795 | `	return 0;` |
|     173 |  7796 | `}` |
|       - |  7797 |  |
|       - |  7798 | `/*` |
|       - |  7799 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|       - |  7800 | ` * constant) — the main atom plus any union alternatives — against the` |
|       - |  7801 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|       - |  7802 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|       - |  7803 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|       - |  7804 | ` * type T" vs "Class constant C::X cannot have type T").` |
|       - |  7805 | ` *` |
|       - |  7806 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - |  7807 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - |  7808 | ` */` |
|     278 |  7809 | `static sxi32 GenStateValidateMemberType(` |
|       - |  7810 | `	ph7_gen_state *pGen,` |
|       - |  7811 | `	ph7_class *pClass,` |
|       - |  7812 | `	const SyString *pMemberName,` |
|       - |  7813 | `	sxu32 nType,` |
|       - |  7814 | `	const SyString *pTypeClass,` |
|       - |  7815 | `	const SyString *pTypeText,` |
|       - |  7816 | `	SySet *pUnionAlts,` |
|       - |  7817 | `	const char *zErrFmt,` |
|       - |  7818 | `	sxu32 nLine)` |
|       5 |  7819 | `{` |
|     283 |  7820 | `	const char *zBad = 0;` |
|     283 |  7821 | `	sxu32 nBad = 0;` |
|       - |  7822 | `	SyString sFallback;` |
|       - |  7823 | `	const SyString *pBad;` |
|       - |  7824 | `	sxi32 rc;` |
|     283 |  7825 | `	int bDisallowed = 0;` |
|     283 |  7826 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       5 |  7827 | `		bDisallowed = 1;` |
|     281 |  7828 | `	}else if( pUnionAlts ){` |
|       - |  7829 | `		sxu32 i;` |
|      88 |  7830 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      62 |  7831 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      62 |  7832 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 |  7833 | `				bDisallowed = 1;` |
|       3 |  7834 | `				break;` |
|       - |  7835 | `			}` |
|      32 |  7836 | `		}` |
|      14 |  7837 | `	}` |
|     283 |  7838 | `	if( !bDisallowed ){` |
|     277 |  7839 | `		return SXRET_OK;` |
|       - |  7840 | `	}` |
|       - |  7841 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - |  7842 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - |  7843 | `	 * canonical spelling if the type text is unavailable. */` |
|       8 |  7844 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       8 |  7845 | `		pBad = pTypeText;` |
|       5 |  7846 | `	}else{` |
|     ! 0 |  7847 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 |  7848 | `		pBad = &sFallback;` |
|       - |  7849 | `	}` |
|      11 |  7850 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       3 |  7851 | `		zErrFmt,` |
|       3 |  7852 | `		&pClass->sName,pMemberName,pBad);` |
|       8 |  7853 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7854 | `		return SXERR_ABORT;` |
|       - |  7855 | `	}` |
|       8 |  7856 | `	return SXERR_SYNTAX;` |
|     144 |  7857 | `}` |
|       - |  7858 | `/*` |
|       - |  7859 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|       - |  7860 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|       - |  7861 | ` * matched as a plain identifier in the class-member modifier position rather` |
|       - |  7862 | ` * than promoted to a lexer keyword.` |
|       - |  7863 | ` */` |
| 1715402 |  7864 | `static int GenStateIsReadonly(SyToken *pTok)` |
|       5 |  7865 | `{` |
| 1750342 |  7866 | `	return (pTok->nType & PH7_TK_ID)` |
|  892636 |  7867 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 1750337 |  7868 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|       5 |  7869 | `}` |
|   78190 |  7870 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7871 | `{` |
|   78195 |  7872 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7873 | `	ph7_class_attr *pAttr;` |
|       - |  7874 | `	SyString *pName;` |
|       - |  7875 | `	sxi32 rc;` |
|   78195 |  7876 | `	sxu32 nType = 0;` |
|       - |  7877 | `	SyString sTypeClass;` |
|       - |  7878 | `	SyString sTypeText;` |
|       - |  7879 | `	SySet aUnionAlts;` |
|   78195 |  7880 | `	sxi32 iTypeFlags = 0;` |
|   78195 |  7881 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   78195 |  7882 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   78195 |  7883 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7884 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|       - |  7885 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|       - |  7886 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   78195 |  7887 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|      21 |  7888 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  7889 | `	}` |
|       - |  7890 | `	/* Extract visibility level */` |
|   78195 |  7891 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7892 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   78287 |  7893 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     189 |  7894 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     189 |  7895 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7896 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7897 | `			goto Synchronize;` |
|     189 |  7898 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  7899 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7900 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 |  7901 | `				&pGen->pIn->sData);` |
|     ! 0 |  7902 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7903 | `				return SXERR_ABORT;` |
|       - |  7904 | `			}` |
|     ! 0 |  7905 | `			goto Synchronize;` |
|     189 |  7906 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7907 | `			return SXERR_ABORT;` |
|       - |  7908 | `		}` |
|      92 |  7909 | `	}` |
|     ! 0 |  7910 | `loop:` |
|   78199 |  7911 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7912 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 |  7913 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7914 | `			return SXERR_ABORT;` |
|       - |  7915 | `		}` |
|     ! 0 |  7916 | `		goto Synchronize;` |
|       - |  7917 | `	}` |
|   78199 |  7918 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   78199 |  7919 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - |  7920 | `		/* Invalid attribute name */` |
|     ! 0 |  7921 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 |  7922 | `		if( rc == SXERR_ABORT ){` |
|       - |  7923 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7924 | `			return SXERR_ABORT;` |
|       - |  7925 | `		}` |
|     ! 0 |  7926 | `		goto Synchronize;` |
|       - |  7927 | `	}` |
|       - |  7928 | `	/* Peek attribute name */` |
|   78199 |  7929 | `	pName = &pGen->pIn->sData;` |
|       - |  7930 | `	/* Advance the stream cursor */` |
|   78199 |  7931 | `	pGen->pIn++;` |
|   78199 |  7932 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - |  7933 | `		/* Invalid declaration */` |
|       3 |  7934 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 |  7935 | `		if( rc == SXERR_ABORT ){` |
|       - |  7936 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7937 | `			return SXERR_ABORT;` |
|       - |  7938 | `		}` |
|       3 |  7939 | `		goto Synchronize;` |
|       - |  7940 | `	}` |
|       - |  7941 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|       - |  7942 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   78197 |  7943 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|      39 |  7944 | `		const char *zRoErr = 0;` |
|      39 |  7945 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|       3 |  7946 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|      38 |  7947 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       6 |  7948 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|      35 |  7949 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|       6 |  7950 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|       2 |  7951 | `		}` |
|      39 |  7952 | `		if( zRoErr ){` |
|      13 |  7953 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|      13 |  7954 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7955 | `				return SXERR_ABORT;` |
|       - |  7956 | `			}` |
|      13 |  7957 | `			goto Synchronize;` |
|       - |  7958 | `		}` |
|      12 |  7959 | `	}` |
|       - |  7960 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - |  7961 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - |  7962 | `	 * by the type parser. */` |
|   78187 |  7963 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     278 |  7964 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - |  7965 | `			&sTypeText,` |
|     182 |  7966 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      91 |  7967 | `			"Property %z::$%z cannot have type %z",nLine);` |
|     187 |  7968 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7969 | `			return SXERR_ABORT;` |
|     187 |  7970 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7971 | `			goto Synchronize;` |
|       - |  7972 | `		}` |
|      91 |  7973 | `	}` |
|       - |  7974 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   78187 |  7975 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 |  7976 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7977 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 |  7978 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7979 | `			return SXERR_ABORT;` |
|       - |  7980 | `		}` |
|       3 |  7981 | `		goto Synchronize;` |
|       - |  7982 | `	}` |
|       - |  7983 | ``	/* PHP 8.5: a `new` expression is not allowed anywhere in a property default`` |
|       - |  7984 | `	 * initializer ("New expressions are not supported in this context"). Reject it` |
|       - |  7985 | `	 * here, before allocating the attribute, matching PHP's compile-time fatal and` |
|       - |  7986 | `	 * the class-constant path above. pGen->pIn is still on the '=' (the scan skips` |
|       - |  7987 | `	 * it and reads the initializer non-destructively); no '=' means no default, so` |
|       - |  7988 | `	 * the helper stops at the ';'/',' and returns 0. */` |
|   78185 |  7989 | `	if( (pGen->pIn->nType & PH7_TK_EQUAL /*'='*/) && GenStateInitHasNewExpr(pGen) ){` |
|       6 |  7990 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7991 | `			"New expressions are not supported in this context");` |
|       6 |  7992 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7993 | `			return SXERR_ABORT;` |
|       - |  7994 | `		}` |
|       6 |  7995 | `		goto Synchronize;` |
|       - |  7996 | `	}` |
|       - |  7997 | `	/* Allocate a new class attribute */` |
|   78181 |  7998 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   78181 |  7999 | `	if( pAttr == 0 ){` |
|     ! 0 |  8000 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  8001 | `		return SXERR_ABORT;` |
|       - |  8002 | `	}` |
|   78181 |  8003 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     185 |  8004 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      90 |  8005 | `	}` |
|   78181 |  8006 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - |  8007 | `		SySet *pInstrContainer;` |
|   22643 |  8008 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - |  8009 | `		/* Swap bytecode container */` |
|   22643 |  8010 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   22643 |  8011 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - |  8012 | `		/* Compile attribute value.` |
|       - |  8013 | `		 */` |
|   22643 |  8014 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   22643 |  8015 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  8016 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 |  8017 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8018 | `				return SXERR_ABORT;` |
|       - |  8019 | `			}` |
|     ! 0 |  8020 | `		}` |
|       - |  8021 | `		/* Emit the done instruction */` |
|   22643 |  8022 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   22643 |  8023 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   11319 |  8024 | `	}` |
|       - |  8025 | `	/* All done,install the attribute */` |
|   78181 |  8026 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   78181 |  8027 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8028 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8029 | `		return SXERR_ABORT;` |
|       - |  8030 | `	}` |
|   78181 |  8031 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  8032 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 |  8033 | `		pGen->pIn++; /* Jump the comma */` |
|       5 |  8034 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  8035 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  8036 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  8037 | `				pTok--;` |
|     ! 0 |  8038 | `			}` |
|     ! 0 |  8039 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8040 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8041 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  8042 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8043 | `				return SXERR_ABORT;` |
|       - |  8044 | `			}` |
|     ! 0 |  8045 | `		}else{` |
|       5 |  8046 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 |  8047 | `				goto loop;` |
|       - |  8048 | `			}` |
|       - |  8049 | `		}` |
|     ! 0 |  8050 | `	}` |
|   78177 |  8051 | `	SySetRelease(&aUnionAlts);` |
|   78177 |  8052 | `	return SXRET_OK;` |
|       9 |  8053 | `Synchronize:` |
|       - |  8054 | `	/* Synchronize with the first semi-colon */` |
|      56 |  8055 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      37 |  8056 | `		pGen->pIn++;` |
|       3 |  8057 | `	}` |
|      22 |  8058 | `	SySetRelease(&aUnionAlts);` |
|      22 |  8059 | `	return SXERR_CORRUPT;` |
|   39100 |  8060 | `}` |
|       - |  8061 | `/*` |
|       - |  8062 | ` * Compile a class method.` |
|       - |  8063 | ` *` |
|       - |  8064 | ` * Refer to the official documentation for more information` |
|       - |  8065 | ` * on the powerful extension introduced by the PH7 engine` |
|       - |  8066 | ` * to the OO subsystem such as full type hinting,method` |
|       - |  8067 | ` * overloading and many more.` |
|       - |  8068 | ` */` |
|  277896 |  8069 | `static sxi32 GenStateCompileClassMethod(` |
|       - |  8070 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  8071 | `	sxi32 iProtection,   /* Visibility level */` |
|       - |  8072 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - |  8073 | `	int doBody,          /* TRUE to process method body */` |
|       - |  8074 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - |  8075 | `	)` |
|       5 |  8076 | `{` |
|  277901 |  8077 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8078 | `	ph7_class_method *pMeth;` |
|       - |  8079 | `	sxi32 iFuncFlags;` |
|       - |  8080 | `	SyString *pName;` |
|       - |  8081 | `	SyToken *pEnd;` |
|       - |  8082 | `	sxi32 rc;` |
|       - |  8083 | `	/* Extract visibility level */` |
|  277901 |  8084 | `	iProtection = GetProtectionLevel(iProtection);` |
|  277901 |  8085 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  277901 |  8086 | `	iFuncFlags = 0;` |
|  277901 |  8087 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8088 | `		/* Invalid method name */` |
|     ! 0 |  8089 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  8090 | `		if( rc == SXERR_ABORT ){` |
|       - |  8091 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8092 | `			return SXERR_ABORT;` |
|       - |  8093 | `		}` |
|     ! 0 |  8094 | `		goto Synchronize;` |
|       - |  8095 | `	}` |
|  277901 |  8096 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  8097 | `		/* Return by reference,remember that */` |
|     ! 0 |  8098 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  8099 | `		/* Jump the '&' token */` |
|     ! 0 |  8100 | `		pGen->pIn++;` |
|     ! 0 |  8101 | `	}` |
|  277901 |  8102 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  8103 | `		/* Invalid method name */` |
|     ! 0 |  8104 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  8105 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8106 | `			return SXERR_ABORT;` |
|       - |  8107 | `		}` |
|     ! 0 |  8108 | `		goto Synchronize;` |
|       - |  8109 | `	}` |
|       - |  8110 | `	/* Peek method name */` |
|  277901 |  8111 | `	pName = &pGen->pIn->sData;` |
|  277901 |  8112 | `	nLine = pGen->pIn->nLine;` |
|       - |  8113 | `	/* Jump the method name */` |
|  277901 |  8114 | `	pGen->pIn++;` |
|  277901 |  8115 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - |  8116 | `		/* Abstract method */` |
|   96003 |  8117 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 |  8118 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8119 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 |  8120 | `				&pClass->sName,pName);` |
|     ! 0 |  8121 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8122 | `				return SXERR_ABORT;` |
|       - |  8123 | `			}` |
|     ! 0 |  8124 | `		}` |
|       - |  8125 | `		/* Assemble method signature only */` |
|   96003 |  8126 | `		doBody = FALSE;` |
|   47999 |  8127 | `	}` |
|  277901 |  8128 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  8129 | `		/* Syntax error */` |
|     ! 0 |  8130 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 |  8131 | `		if( rc == SXERR_ABORT ){` |
|       - |  8132 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8133 | `			return SXERR_ABORT;` |
|       - |  8134 | `		}` |
|     ! 0 |  8135 | `		goto Synchronize;` |
|       - |  8136 | `	}` |
|       - |  8137 | `	/* Allocate a new class_method instance */` |
|  277901 |  8138 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  277901 |  8139 | `	if( pMeth == 0 ){` |
|     ! 0 |  8140 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8141 | `		return SXERR_ABORT;` |
|       - |  8142 | `	}` |
|       - |  8143 | `	/* Jump the left parenthesis '(' */` |
|  277901 |  8144 | `	pGen->pIn++;` |
|  277901 |  8145 | `	pEnd = 0; /* cc warning */` |
|       - |  8146 | `	/* Delimit the method signature */` |
|  277901 |  8147 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  277901 |  8148 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8149 | `		/* Syntax error */` |
|       3 |  8150 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 |  8151 | `		if( rc == SXERR_ABORT ){` |
|       - |  8152 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8153 | `			return SXERR_ABORT;` |
|       - |  8154 | `		}` |
|       3 |  8155 | `		goto Synchronize;` |
|       - |  8156 | `	}` |
|       - |  8157 | `	{` |
|  277899 |  8158 | `		int bIsCtor = 0;` |
|  277899 |  8159 | `		int bAbstractCtor = 0;` |
|  277894 |  8160 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  164903 |  8161 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  266747 |  8162 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   22309 |  8163 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       3 |  8164 | `				bAbstractCtor = 1;` |
|       2 |  8165 | `			}else{` |
|   22307 |  8166 | `				bIsCtor = 1;` |
|       - |  8167 | `			}` |
|   11152 |  8168 | `		}` |
|  277899 |  8169 | `		if( pGen->pIn < pEnd ){` |
|       - |  8170 | `			/* Collect method arguments */` |
|   74275 |  8171 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   74275 |  8172 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8173 | `				return SXERR_ABORT;` |
|       - |  8174 | `			}` |
|   37135 |  8175 | `		}` |
|       - |  8176 | `	}` |
|       - |  8177 | `	/* Point past ')' and parse optional return type ': type' */` |
|  277899 |  8178 | `	pGen->pIn = &pEnd[1];` |
|       - |  8179 | `	{` |
|  277899 |  8180 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  277899 |  8181 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  8182 | `			return SXERR_ABORT;` |
|  277899 |  8183 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 |  8184 | `			goto Synchronize;` |
|       - |  8185 | `		}` |
|       - |  8186 | `	}` |
|       - |  8187 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - |  8188 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - |  8189 | `	 * since we mint real ph7_class_attr entries. */` |
|       - |  8190 | `	{` |
|  277899 |  8191 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - |  8192 | `		sxu32 i;` |
|  403939 |  8193 | `		for( i = 0; i < nArg; i++ ){` |
|  126055 |  8194 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - |  8195 | `			ph7_class_attr *pAttr;` |
|  126055 |  8196 | `			sxi32 iAttrFlags = 0;` |
|       - |  8197 | `			int bArgTyped;` |
|  126055 |  8198 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  125991 |  8199 | `				continue;` |
|       - |  8200 | `			}` |
|       - |  8201 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|       - |  8202 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|       - |  8203 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|      49 |  8204 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|      70 |  8205 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|      69 |  8206 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 |  8207 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8208 | `					"Cannot declare variadic promoted property");` |
|       3 |  8209 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8210 | `					return SXERR_ABORT;` |
|       - |  8211 | `				}` |
|       3 |  8212 | `				goto Synchronize;` |
|       - |  8213 | `			}` |
|       - |  8214 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - |  8215 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - |  8216 | `			 * appear as an alternative of a union type. */` |
|      67 |  8217 | `			if( bArgTyped ){` |
|      92 |  8218 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|      58 |  8219 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      58 |  8220 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      29 |  8221 | `					"Property %z::$%z cannot have type %z",nLine);` |
|      63 |  8222 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8223 | `					return SXERR_ABORT;` |
|      63 |  8224 | `				}else if( rc != SXRET_OK ){` |
|       6 |  8225 | `					goto Synchronize;` |
|       - |  8226 | `				}` |
|      27 |  8227 | `			}` |
|       - |  8228 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|      63 |  8229 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 |  8230 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  8231 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 |  8232 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8233 | `					return SXERR_ABORT;` |
|       - |  8234 | `				}` |
|       3 |  8235 | `				goto Synchronize;` |
|       - |  8236 | `			}` |
|      61 |  8237 | `			if( bArgTyped ){` |
|      57 |  8238 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      26 |  8239 | `			}` |
|      61 |  8240 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 |  8241 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 |  8242 | `			}` |
|      61 |  8243 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       8 |  8244 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       3 |  8245 | `			}` |
|      61 |  8246 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|       - |  8247 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|       - |  8248 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|      24 |  8249 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       4 |  8250 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  8251 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|       3 |  8252 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8253 | `						return SXERR_ABORT;` |
|       - |  8254 | `					}` |
|       3 |  8255 | `					goto Synchronize;` |
|       - |  8256 | `				}` |
|      22 |  8257 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  8258 | `			}` |
|      59 |  8259 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      59 |  8260 | `			if( pAttr == 0 ){` |
|     ! 0 |  8261 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8262 | `				return SXERR_ABORT;` |
|       - |  8263 | `			}` |
|      59 |  8264 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      57 |  8265 | `				pAttr->nType = pArg->nType;` |
|      57 |  8266 | `				pAttr->sClass = pArg->sClass;` |
|      57 |  8267 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      57 |  8268 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  8269 | `					sxu32 k;` |
|      20 |  8270 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|      14 |  8271 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|      14 |  8272 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|       8 |  8273 | `					}` |
|       3 |  8274 | `				}` |
|      26 |  8275 | `			}` |
|      59 |  8276 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      59 |  8277 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8278 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8279 | `				return SXERR_ABORT;` |
|       - |  8280 | `			}` |
|      32 |  8281 | `		}` |
|       - |  8282 | `	}` |
|  277889 |  8283 | `	if( doBody ){` |
|       - |  8284 | `		/* Compile method body */` |
|  181891 |  8285 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  181891 |  8286 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8287 | `			return SXERR_ABORT;` |
|       - |  8288 | `		}` |
|   90948 |  8289 | `	}else{` |
|       - |  8290 | `		/* Only method signature is allowed */` |
|   96003 |  8291 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 |  8292 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8293 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 |  8294 | `				if( rc == SXERR_ABORT ){` |
|       - |  8295 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8296 | `					return SXERR_ABORT;` |
|       - |  8297 | `				}` |
|     ! 0 |  8298 | `				return SXERR_CORRUPT;` |
|       - |  8299 | `			}` |
|       - |  8300 | `	}` |
|       - |  8301 | `	/* All done,install the method */` |
|  277889 |  8302 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  277889 |  8303 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8304 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8305 | `		return SXERR_ABORT;` |
|       - |  8306 | `	}` |
|  277889 |  8307 | `	return SXRET_OK;` |
|       6 |  8308 | `Synchronize:` |
|       - |  8309 | `	/* Synchronize with the first semi-colon */` |
|      40 |  8310 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      28 |  8311 | `		pGen->pIn++;` |
|       4 |  8312 | `	}` |
|      16 |  8313 | `	return SXERR_CORRUPT;` |
|  138953 |  8314 | `}` |
|       - |  8315 | `/*` |
|       - |  8316 | ` * Compile an object interface.` |
|       - |  8317 | ` *  According to the PHP language reference manual` |
|       - |  8318 | ` *   Object Interfaces:` |
|       - |  8319 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - |  8320 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - |  8321 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  8322 | ` *   class, but without any of the methods having their contents defined.` |
|       - |  8323 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  8324 | ` */` |
|   40674 |  8325 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       5 |  8326 | `{` |
|   40679 |  8327 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8328 | `	ph7_class *pClass,*pBase;` |
|       - |  8329 | `	SyToken *pEnd,*pTmp;` |
|       - |  8330 | `	SyString *pName;` |
|       - |  8331 | `	sxi32 nKwrd;` |
|       - |  8332 | `	sxi32 rc;` |
|       - |  8333 | `	/* Jump the 'interface' keyword */` |
|   40679 |  8334 | `	pGen->pIn++;` |
|       - |  8335 | `	/* Extract interface name */` |
|   40679 |  8336 | `	pName = &pGen->pIn->sData;` |
|       - |  8337 | `	/* Advance the stream cursor */` |
|   40679 |  8338 | `	pGen->pIn++;` |
|       - |  8339 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  8340 | `		SyBlob sFQN;` |
|       - |  8341 | `		SyString sFQNStr;` |
|   40679 |  8342 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   40679 |  8343 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   40679 |  8344 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   40679 |  8345 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   40679 |  8346 | `		SyBlobRelease(&sFQN);` |
|       - |  8347 | `	}` |
|   40679 |  8348 | `	if( pClass == 0 ){` |
|     ! 0 |  8349 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8350 | `		return SXERR_ABORT;` |
|       - |  8351 | `	}` |
|       - |  8352 | `	/* Mark as an interface */` |
|   40679 |  8353 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - |  8354 | `	/* Assume no base class is given */` |
|   40679 |  8355 | `	pBase = 0;` |
|   40679 |  8356 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   11083 |  8357 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   11083 |  8358 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - |  8359 | `			SyBlob sResolved;` |
|       - |  8360 | `			SyString sBaseName;` |
|       - |  8361 | `			sxu32 nRefLine;` |
|       - |  8362 | `			/* Extract base interface */` |
|   11083 |  8363 | `			pGen->pIn++;` |
|   11083 |  8364 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   11083 |  8365 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   11083 |  8366 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  8367 | `				SyBlobRelease(&sResolved);` |
|     ! 0 |  8368 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8369 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 |  8370 | `					pName);` |
|     ! 0 |  8371 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8372 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8373 | `					return SXERR_ABORT;` |
|       - |  8374 | `				}` |
|     ! 0 |  8375 | `				return SXRET_OK;` |
|       - |  8376 | `			}` |
|   16622 |  8377 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   11078 |  8378 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   11083 |  8379 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8380 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8381 | `			/* Only interfaces is allowed */` |
|   11083 |  8382 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8383 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8384 | `			}` |
|   11083 |  8385 | `			if( pBase == 0 ){` |
|     ! 0 |  8386 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8387 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|     ! 0 |  8388 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8389 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8390 | `					return SXERR_ABORT;` |
|       - |  8391 | `				}` |
|     ! 0 |  8392 | `			}` |
|   11083 |  8393 | `			SyBlobRelease(&sResolved);` |
|    5539 |  8394 | `		}` |
|    5539 |  8395 | `	}` |
|   40679 |  8396 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8397 | `		/* Syntax error */` |
|     ! 0 |  8398 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 |  8399 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8400 | `		if( rc == SXERR_ABORT ){` |
|       - |  8401 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8402 | `			return SXERR_ABORT;` |
|       - |  8403 | `		}` |
|     ! 0 |  8404 | `		return SXRET_OK;` |
|       - |  8405 | `	}` |
|   40679 |  8406 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   40679 |  8407 | `	pEnd = 0; /* cc warning */` |
|       - |  8408 | `	/* Delimit the interface body */` |
|   40679 |  8409 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   40679 |  8410 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8411 | `		/* Syntax error */` |
|     ! 0 |  8412 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 |  8413 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8414 | `		if( rc == SXERR_ABORT ){` |
|       - |  8415 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8416 | `			return SXERR_ABORT;` |
|       - |  8417 | `		}` |
|     ! 0 |  8418 | `		return SXRET_OK;` |
|       - |  8419 | `	}` |
|       - |  8420 | `	/* Swap token stream */` |
|   40679 |  8421 | `	pTmp = pGen->pEnd;` |
|   40679 |  8422 | `	pGen->pEnd = pEnd;` |
|       - |  8423 | `	/* Start the parse process` |
|       - |  8424 | `	 * Note (According to the PHP reference manual):` |
|       - |  8425 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - |  8426 | `	 *  Only 'public' visibility is allowed.` |
|       - |  8427 | `	 */` |
|   68331 |  8428 | `	for(;;){` |
|       - |  8429 | `		/* Jump leading/trailing semi-colons */` |
|  232655 |  8430 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   95993 |  8431 | `			pGen->pIn++;` |
|       5 |  8432 | `		}` |
|  136667 |  8433 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8434 | `			/* End of interface body */` |
|   40675 |  8435 | `			break;` |
|       - |  8436 | `		}` |
|   95997 |  8437 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8438 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8439 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 |  8440 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8441 | `			if( rc == SXERR_ABORT ){` |
|       - |  8442 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8443 | `				return SXERR_ABORT;` |
|       - |  8444 | `			}` |
|     ! 0 |  8445 | `			goto done;` |
|       - |  8446 | `		}` |
|       - |  8447 | `		/* Extract the current keyword */` |
|   95997 |  8448 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   95997 |  8449 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - |  8450 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - |  8451 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 |  8452 | `			const char *zKind = "member";` |
|       3 |  8453 | `			SyString *pMemberName = 0;` |
|       3 |  8454 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 |  8455 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 |  8456 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 |  8457 | `					zKind = "constant";` |
|       3 |  8458 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 |  8459 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 |  8460 | `					}` |
|       1 |  8461 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8462 | `					zKind = "method";` |
|     ! 0 |  8463 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 |  8464 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 |  8465 | `					}` |
|     ! 0 |  8466 | `				}` |
|       1 |  8467 | `			}` |
|       3 |  8468 | `			if( pMemberName ){` |
|       4 |  8469 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 |  8470 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 |  8471 | `			}else{` |
|     ! 0 |  8472 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8473 | `					"Access type for interface %s must be public",zKind);` |
|       - |  8474 | `			}` |
|       3 |  8475 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8476 | `				return SXERR_ABORT;` |
|       - |  8477 | `			}` |
|       3 |  8478 | `			goto done;` |
|       - |  8479 | `		}` |
|   95995 |  8480 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  8481 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8482 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  8483 | `			if( rc == SXERR_ABORT ){` |
|       - |  8484 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8485 | `				return SXERR_ABORT;` |
|       - |  8486 | `			}` |
|     ! 0 |  8487 | `			goto done;` |
|       - |  8488 | `		}` |
|   95995 |  8489 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - |  8490 | `			/* Advance the stream cursor */` |
|   95983 |  8491 | `			pGen->pIn++;` |
|   95983 |  8492 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8493 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8494 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  8495 | `				if( rc == SXERR_ABORT ){` |
|       - |  8496 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8497 | `					return SXERR_ABORT;` |
|       - |  8498 | `				}` |
|     ! 0 |  8499 | `				goto done;` |
|       - |  8500 | `			}` |
|   95983 |  8501 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   95983 |  8502 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  8503 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8504 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  8505 | `				if( rc == SXERR_ABORT ){` |
|       - |  8506 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8507 | `					return SXERR_ABORT;` |
|       - |  8508 | `				}` |
|     ! 0 |  8509 | `				goto done;` |
|       - |  8510 | `			}` |
|   47989 |  8511 | `		}` |
|   95995 |  8512 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8513 | `			/* Parse constant */` |
|      10 |  8514 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|      10 |  8515 | `			if( rc != SXRET_OK ){` |
|       3 |  8516 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8517 | `					return SXERR_ABORT;` |
|       - |  8518 | `				}` |
|       3 |  8519 | `				goto done;` |
|       - |  8520 | `			}` |
|       4 |  8521 | `		}else{` |
|   95987 |  8522 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   95987 |  8523 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8524 | `				/* Static method,record that */` |
|   11075 |  8525 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - |  8526 | `				/* Advance the stream cursor */` |
|   11075 |  8527 | `				pGen->pIn++;` |
|   11070 |  8528 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|   11075 |  8529 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8530 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8531 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  8532 | `						if( rc == SXERR_ABORT ){` |
|       - |  8533 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8534 | `							return SXERR_ABORT;` |
|       - |  8535 | `						}` |
|     ! 0 |  8536 | `						goto done;` |
|       - |  8537 | `				}` |
|    5535 |  8538 | `			}` |
|       - |  8539 | `			/* Process method signature (no body for interface methods) */` |
|   95987 |  8540 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   95987 |  8541 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8542 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8543 | `					return SXERR_ABORT;` |
|       - |  8544 | `				}` |
|     ! 0 |  8545 | `				goto done;` |
|       - |  8546 | `			}` |
|       - |  8547 | `		}` |
|       5 |  8548 | `	}` |
|       - |  8549 | `	/* Install the interface */` |
|   40675 |  8550 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   40675 |  8551 | `	if( rc == SXRET_OK && pBase ){` |
|       - |  8552 | `		/* Inherit from the base interface */` |
|   11083 |  8553 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|    5539 |  8554 | `	}` |
|   40675 |  8555 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8556 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8557 | `		return SXERR_ABORT;` |
|       - |  8558 | `	}` |
|   20335 |  8559 | `done:` |
|       - |  8560 | `	/* Point beyond the interface body */` |
|   40679 |  8561 | `	pGen->pIn  = &pEnd[1];` |
|   40679 |  8562 | `	pGen->pEnd = pTmp;` |
|   40679 |  8563 | `	return PH7_OK;` |
|   20342 |  8564 | `}` |
|       - |  8565 | `/*` |
|       - |  8566 | ` * Compile a user-defined class.` |
|       - |  8567 | ` * According to the PHP language reference manual` |
|       - |  8568 | ` *  class` |
|       - |  8569 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - |  8570 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - |  8571 | ` *  of the properties and methods belonging to the class.` |
|       - |  8572 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - |  8573 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - |  8574 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - |  8575 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  8576 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - |  8577 | ` *  (called "methods").` |
|       - |  8578 | ` */` |
|       - |  8579 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - |  8580 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - |  8581 | `struct TraitUseEntry {` |
|       - |  8582 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - |  8583 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - |  8584 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - |  8585 | `};` |
|       - |  8586 | `/*` |
|       - |  8587 | ` * Validate that methods implementing interface contracts have compatible` |
|       - |  8588 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - |  8589 | ` */` |
|  108272 |  8590 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8591 | `{` |
|       - |  8592 | `	ph7_class **apIface;` |
|       - |  8593 | `	sxu32 nIface,i;` |
|       - |  8594 | `	sxi32 rc;` |
|  108277 |  8595 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 |  8596 | `		return SXRET_OK;` |
|       - |  8597 | `	}` |
|  108277 |  8598 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|  108277 |  8599 | `	nIface = SySetUsed(&pClass->aInterface);` |
|  208161 |  8600 | `	for(i = 0; i < nIface; i++){` |
|   99889 |  8601 | `		ph7_class *pIface = apIface[i];` |
|       - |  8602 | `		SyHashEntry *pEntry;` |
|   99889 |  8603 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|  266393 |  8604 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|  166509 |  8605 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - |  8606 | `			ph7_class_method *pImplMeth;` |
|  166509 |  8607 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - |  8608 | `			/* Find the implementing method in the class */` |
|  166509 |  8609 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|  166509 |  8610 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      18 |  8611 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - |  8612 | `			}` |
|       - |  8613 | `			/* Check visibility: interface methods must be implemented as public */` |
|  166495 |  8614 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 |  8615 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8616 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 |  8617 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 |  8618 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8619 | `					return SXERR_ABORT;` |
|       - |  8620 | `				}` |
|       1 |  8621 | `			}` |
|       - |  8622 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - |  8623 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - |  8624 | `			 */` |
|       - |  8625 | `			{` |
|  166495 |  8626 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|  166495 |  8627 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|  166495 |  8628 | `				int sigError = 0;` |
|  166495 |  8629 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 |  8630 | `					sigError = 1;` |
|  166494 |  8631 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - |  8632 | `					/* Extra parameters must all have default values */` |
|       6 |  8633 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - |  8634 | `					sxu32 k;` |
|       8 |  8635 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       6 |  8636 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 |  8637 | `							sigError = 1;` |
|       3 |  8638 | `							break;` |
|       - |  8639 | `						}` |
|       2 |  8640 | `					}` |
|       2 |  8641 | `				}` |
|  166495 |  8642 | `				if( sigError ){` |
|       - |  8643 | `					SyBlob sImplSig, sIfaceSig;` |
|       - |  8644 | `					ph7_vm_func_arg *aArgs;` |
|       - |  8645 | `					sxu32 j;` |
|       6 |  8646 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       6 |  8647 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - |  8648 | `					/* Build implementing method signature */` |
|       6 |  8649 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      12 |  8650 | `					for(j = 0; j < nImplArgs; j++){` |
|       8 |  8651 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       8 |  8652 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       8 |  8653 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8654 | `					}` |
|       - |  8655 | `					/* Build interface method signature */` |
|       6 |  8656 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      12 |  8657 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       8 |  8658 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       8 |  8659 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       8 |  8660 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8661 | `					}` |
|       8 |  8662 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8663 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 |  8664 | `						&pClass->sName,pMName,` |
|       4 |  8665 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 |  8666 | `						&pIface->sName,pMName,` |
|       4 |  8667 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       6 |  8668 | `					SyBlobRelease(&sImplSig);` |
|       6 |  8669 | `					SyBlobRelease(&sIfaceSig);` |
|       6 |  8670 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8671 | `						return SXERR_ABORT;` |
|       - |  8672 | `					}` |
|       2 |  8673 | `				}` |
|       - |  8674 | `			}` |
|       5 |  8675 | `		}` |
|   49947 |  8676 | `	}` |
|  108277 |  8677 | `	return SXRET_OK;` |
|   54141 |  8678 | `}` |
|       - |  8679 | `/*` |
|       - |  8680 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - |  8681 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - |  8682 | ` */` |
|  108272 |  8683 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8684 | `{` |
|       - |  8685 | `	ph7_class_method *pMeth;` |
|       - |  8686 | `	SyHashEntry *pEntry;` |
|       - |  8687 | `	sxu32 nAbstract;` |
|       - |  8688 | `	SyBlob sMsg;` |
|       - |  8689 | `	sxi32 rc;` |
|       - |  8690 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|  108277 |  8691 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      37 |  8692 | `		return SXRET_OK;` |
|       - |  8693 | `	}` |
|       - |  8694 | `	/* Count abstract methods */` |
|  108245 |  8695 | `	nAbstract = 0;` |
|  108245 |  8696 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
| 1017721 |  8697 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  909481 |  8698 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  909481 |  8699 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      20 |  8700 | `			nAbstract++;` |
|       8 |  8701 | `		}` |
|       5 |  8702 | `	}` |
|  108245 |  8703 | `	if( nAbstract == 0 ){` |
|  108231 |  8704 | `		return SXRET_OK;` |
|       - |  8705 | `	}` |
|       - |  8706 | `	/* Build the error message listing all abstract methods with origins */` |
|      18 |  8707 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      18 |  8708 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - |  8709 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 |  8710 | `		&pClass->sName,nAbstract,` |
|       7 |  8711 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 |  8712 | `		(nAbstract > 1 ? "s" : ""));` |
|       - |  8713 | `	/* Second pass: list methods with origins */` |
|       - |  8714 | `	{` |
|      18 |  8715 | `		sxu32 nListed = 0;` |
|      18 |  8716 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      36 |  8717 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      22 |  8718 | `			ph7_class *pOrigin = 0;` |
|       - |  8719 | `			SyString *pMName;` |
|      22 |  8720 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      22 |  8721 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 |  8722 | `				continue;` |
|       - |  8723 | `			}` |
|      20 |  8724 | `			pMName = &pMeth->sFunc.sName;` |
|      20 |  8725 | `			if( nListed > 0 ){` |
|       3 |  8726 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 |  8727 | `			}` |
|       - |  8728 | `			/* Find the origin of this abstract method.` |
|       - |  8729 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - |  8730 | `			 * inheritance chains) take precedence for interface-declared` |
|       - |  8731 | `			 * methods. Abstract class methods only win when the class` |
|       - |  8732 | `			 * itself declared the abstract method (not inherited from` |
|       - |  8733 | `			 * an interface). Trait methods are adopted into the using` |
|       - |  8734 | `			 * class's namespace.` |
|       - |  8735 | `			 */` |
|       - |  8736 | `			{` |
|       - |  8737 | `				ph7_class **apIface;` |
|       - |  8738 | `				ph7_class **apTrait;` |
|       - |  8739 | `				ph7_class *pWalk;` |
|       - |  8740 | `				sxu32 i;` |
|       - |  8741 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - |  8742 | `				 * (one that was written in the class body, not inherited from an` |
|       - |  8743 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - |  8744 | `				 */` |
|      20 |  8745 | `				if( pClass->pBase ){` |
|      11 |  8746 | `					pWalk = pClass->pBase;` |
|      19 |  8747 | `					while( pWalk ){` |
|       - |  8748 | `						ph7_class_method *pParentMeth;` |
|      13 |  8749 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      13 |  8750 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - |  8751 | `							/* Exclude methods that came from an interface anywhere` |
|       - |  8752 | `							 * in this class's ancestor chain.` |
|       - |  8753 | `							 */` |
|      13 |  8754 | `							int fromIface = 0;` |
|      13 |  8755 | `							ph7_class *pAnc = pWalk;` |
|      17 |  8756 | `							while( pAnc ){` |
|       - |  8757 | `								ph7_class **apPI;` |
|       - |  8758 | `								sxu32 j;` |
|      15 |  8759 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      15 |  8760 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|      10 |  8761 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|      10 |  8762 | `										fromIface = 1;` |
|      10 |  8763 | `										break;` |
|       - |  8764 | `									}` |
|     ! 0 |  8765 | `								}` |
|      15 |  8766 | `								if( fromIface ) break;` |
|       6 |  8767 | `								pAnc = pAnc->pBase;` |
|       2 |  8768 | `							}` |
|      13 |  8769 | `							if( !fromIface ){` |
|       3 |  8770 | `								pOrigin = pWalk;` |
|       3 |  8771 | `								break;` |
|       - |  8772 | `							}` |
|       4 |  8773 | `						}` |
|      10 |  8774 | `						pWalk = pWalk->pBase;` |
|       2 |  8775 | `					}` |
|       4 |  8776 | `				}` |
|       - |  8777 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - |  8778 | `				 * each interface's own parent chain for the deepest origin.` |
|       - |  8779 | `				 */` |
|      20 |  8780 | `				if( !pOrigin ){` |
|      18 |  8781 | `					pWalk = pClass;` |
|      40 |  8782 | `					while( pWalk && !pOrigin ){` |
|      26 |  8783 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      26 |  8784 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      16 |  8785 | `							ph7_class *pIface = apIface[i];` |
|      16 |  8786 | `							ph7_class *pDeepest = 0;` |
|      28 |  8787 | `							while( pIface ){` |
|      16 |  8788 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      16 |  8789 | `									pDeepest = pIface;` |
|       6 |  8790 | `								}` |
|      16 |  8791 | `								pIface = pIface->pBase;` |
|       4 |  8792 | `							}` |
|      16 |  8793 | `							if( pDeepest ){` |
|      16 |  8794 | `								pOrigin = pDeepest;` |
|      16 |  8795 | `								break;` |
|       - |  8796 | `							}` |
|     ! 0 |  8797 | `						}` |
|      26 |  8798 | `						pWalk = pWalk->pBase;` |
|       4 |  8799 | `					}` |
|       7 |  8800 | `				}` |
|       - |  8801 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      20 |  8802 | `				if( !pOrigin ){` |
|       3 |  8803 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 |  8804 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 |  8805 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 |  8806 | `							pOrigin = pClass;` |
|       3 |  8807 | `							break;` |
|       - |  8808 | `						}` |
|     ! 0 |  8809 | `					}` |
|       1 |  8810 | `				}` |
|       - |  8811 | `			}` |
|      20 |  8812 | `			if( pOrigin ){` |
|      20 |  8813 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|      12 |  8814 | `			}else{` |
|       - |  8815 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 |  8816 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - |  8817 | `			}` |
|      20 |  8818 | `			nListed++;` |
|       4 |  8819 | `		}` |
|       - |  8820 | `	}` |
|      18 |  8821 | `	SyBlobAppend(&sMsg,")",1);` |
|      25 |  8822 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 |  8823 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      18 |  8824 | `	SyBlobRelease(&sMsg);` |
|      18 |  8825 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  8826 | `		return SXERR_ABORT;` |
|       - |  8827 | `	}` |
|      18 |  8828 | `	return SXRET_OK;` |
|   54141 |  8829 | `}` |
|       - |  8830 | `/*` |
|       - |  8831 | ` * Parse a class/interface name reference from the current token stream.` |
|       - |  8832 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|       - |  8833 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|       - |  8834 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|       - |  8835 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|       - |  8836 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|       - |  8837 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|       - |  8838 | ` */` |
|  104368 |  8839 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|       5 |  8840 | `{` |
|  104373 |  8841 | `	int isAbsolute = 0;` |
|  104373 |  8842 | `	SyToken *pStart = pGen->pIn;` |
|       - |  8843 | `	SyBlob sName;` |
|  104373 |  8844 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|    3801 |  8845 | `		isAbsolute = 1;` |
|    3801 |  8846 | `		pGen->pIn++;` |
|    1898 |  8847 | `	}` |
|  104373 |  8848 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       8 |  8849 | `		pGen->pIn = pStart;` |
|       8 |  8850 | `		return SXERR_INVALID;` |
|       - |  8851 | `	}` |
|  104367 |  8852 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|  104367 |  8853 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|  104367 |  8854 | `	pGen->pIn++;` |
|  156564 |  8855 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   52207 |  8856 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      16 |  8857 | `		SyBlobAppend(&sName,"\\",1);` |
|      16 |  8858 | `		pGen->pIn++;` |
|      16 |  8859 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      16 |  8860 | `		pGen->pIn++;` |
|       2 |  8861 | `	}` |
|  104367 |  8862 | `	if( isAbsolute ){` |
|    3799 |  8863 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|    1902 |  8864 | `	}else{` |
|       - |  8865 | `		SyString sRaw;` |
|  100573 |  8866 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|  100573 |  8867 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|       - |  8868 | `	}` |
|  104367 |  8869 | `	SyBlobRelease(&sName);` |
|  104367 |  8870 | `	return SXRET_OK;` |
|   52189 |  8871 | `}` |
|       - |  8872 | `/*` |
|       - |  8873 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|       - |  8874 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|       - |  8875 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|       - |  8876 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|       - |  8877 | ` * either direction cannot run unbounded.` |
|       - |  8878 | ` */` |
|       - |  8879 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|   11242 |  8880 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|       5 |  8881 | `{` |
|       - |  8882 | `	ph7_class **apParent;` |
|       - |  8883 | `	sxu32 n;` |
|   18831 |  8884 | `	while( pInterface ){` |
|   14979 |  8885 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|     ! 0 |  8886 | `			return FALSE;` |
|       - |  8887 | `		}` |
|   18683 |  8888 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    7408 |  8889 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|    7395 |  8890 | `			return TRUE;` |
|       - |  8891 | `		}` |
|    7589 |  8892 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    7589 |  8893 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|     ! 0 |  8894 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|     ! 0 |  8895 | `				return TRUE;` |
|       - |  8896 | `			}` |
|     ! 0 |  8897 | `		}` |
|    7589 |  8898 | `		pInterface = pInterface->pBase;` |
|    7589 |  8899 | `		iDepth++;` |
|       5 |  8900 | `	}` |
|    3857 |  8901 | `	return FALSE;` |
|    5626 |  8902 | `}` |
|   11242 |  8903 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|       5 |  8904 | `{` |
|   11247 |  8905 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|       5 |  8906 | `}` |
|       - |  8907 | `/*` |
|       - |  8908 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|       - |  8909 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|       - |  8910 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|       - |  8911 | ` */` |
|    7390 |  8912 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|       5 |  8913 | `{` |
|    7399 |  8914 | `	while( pBase ){` |
|      10 |  8915 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|       2 |  8916 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|       3 |  8917 | `			return TRUE;` |
|       - |  8918 | `		}` |
|      10 |  8919 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|       6 |  8920 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|       3 |  8921 | `			return TRUE;` |
|       - |  8922 | `		}` |
|       5 |  8923 | `		pBase = pBase->pBase;` |
|       1 |  8924 | `	}` |
|    7391 |  8925 | `	return FALSE;` |
|    3700 |  8926 | `}` |
|       - |  8927 | `/*` |
|       - |  8928 | ` * Compile a class declaration, named or anonymous.` |
|       - |  8929 | ` *` |
|       - |  8930 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|       - |  8931 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|       - |  8932 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|       - |  8933 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|       - |  8934 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|       - |  8935 | ` * implements, body, install) is shared by both paths.` |
|       - |  8936 | ` */` |
|  108312 |  8937 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|       - |  8938 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|       5 |  8939 | `{` |
|  108317 |  8940 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8941 | `	ph7_class *pClass,*pBase;` |
|       - |  8942 | `	SyToken *pEnd,*pTmp;` |
|       - |  8943 | `	sxi32 iProtection;` |
|       - |  8944 | `	SySet aInterfaces;` |
|       - |  8945 | `	SySet aUseEntries;` |
|       - |  8946 | `	sxi32 iAttrflags;` |
|       - |  8947 | `	SyString *pName;` |
|       - |  8948 | `	sxi32 nKwrd;` |
|       - |  8949 | `	sxi32 rc;` |
|       - |  8950 | `	/* Jump the 'class' keyword */` |
|  108317 |  8951 | `	pGen->pIn++;` |
|  108317 |  8952 | `	if( pAnonName ){` |
|       - |  8953 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|       - |  8954 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|       - |  8955 | `		 * then use the synthesized name. */` |
|      30 |  8956 | `		*ppArgStart = *ppArgEnd = 0;` |
|      30 |  8957 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       7 |  8958 | `			pGen->pIn++; /* Jump '(' */` |
|       7 |  8959 | `			*ppArgStart = pGen->pIn;` |
|      10 |  8960 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|       3 |  8961 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|       7 |  8962 | `			pGen->pIn = *ppArgEnd;` |
|       7 |  8963 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|       3 |  8964 | `		}` |
|      30 |  8965 | `		pName = pAnonName;` |
|      30 |  8966 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|      17 |  8967 | `	}else{` |
|  108291 |  8968 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  8969 | `			/* Syntax error */` |
|     ! 0 |  8970 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 |  8971 | `			if( rc == SXERR_ABORT ){` |
|       - |  8972 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8973 | `				return SXERR_ABORT;` |
|       - |  8974 | `			}` |
|       - |  8975 | `			/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 |  8976 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 |  8977 | `				pGen->pIn++;` |
|     ! 0 |  8978 | `			}` |
|     ! 0 |  8979 | `			return SXRET_OK;` |
|       - |  8980 | `		}` |
|       - |  8981 | `		/* Extract class name */` |
|  108291 |  8982 | `		pName = &pGen->pIn->sData;` |
|       - |  8983 | `		/* Advance the stream cursor */` |
|  108291 |  8984 | `		pGen->pIn++;` |
|       - |  8985 | `		/* Build FQN and obtain a raw class */ {` |
|       - |  8986 | `			SyBlob sFQN;` |
|       - |  8987 | `			SyString sFQNStr;` |
|  108291 |  8988 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|  108291 |  8989 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|  108291 |  8990 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|  108291 |  8991 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|  108291 |  8992 | `			SyBlobRelease(&sFQN);` |
|       - |  8993 | `		}` |
|       - |  8994 | `	}` |
|  108317 |  8995 | `	if( pClass == 0 ){` |
|     ! 0 |  8996 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8997 | `		return SXERR_ABORT;` |
|       - |  8998 | `	}` |
|       - |  8999 | `	/* implemented interfaces and per-use-statement trait containers */` |
|  108317 |  9000 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|  108317 |  9001 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - |  9002 | `	/* Assume a standalone class */` |
|  108317 |  9003 | `	pBase = 0;` |
|  108317 |  9004 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   92599 |  9005 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   92599 |  9006 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|       - |  9007 | `			SyBlob sResolved;` |
|       - |  9008 | `			SyString sBaseName;` |
|       - |  9009 | `			sxu32 nRefLine;` |
|   81375 |  9010 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   81375 |  9011 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   81375 |  9012 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   81375 |  9013 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       3 |  9014 | `				SyBlobRelease(&sResolved);` |
|       4 |  9015 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  9016 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|       1 |  9017 | `					pName);` |
|       3 |  9018 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       3 |  9019 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9020 | `					return SXERR_ABORT;` |
|       - |  9021 | `				}` |
|       3 |  9022 | `				return SXRET_OK;` |
|       - |  9023 | `			}` |
|  122057 |  9024 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   81368 |  9025 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   81373 |  9026 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  9027 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  9028 | `			/* Interfaces are not allowed */` |
|   81373 |  9029 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 |  9030 | `				pBase = pBase->pNextName;` |
|     ! 0 |  9031 | `			}` |
|   81373 |  9032 | `			if( pBase == 0 ){` |
|     ! 0 |  9033 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  9034 | `					"Nonexistent base class '%z'",&sBaseName);` |
|     ! 0 |  9035 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9036 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  9037 | `					return SXERR_ABORT;` |
|       - |  9038 | `				}` |
|     ! 0 |  9039 | `			}else{` |
|   81373 |  9040 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 |  9041 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  9042 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 |  9043 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9044 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  9045 | `						return SXERR_ABORT;` |
|       - |  9046 | `					}` |
|     ! 0 |  9047 | `				}` |
|       - |  9048 | `			}` |
|   81373 |  9049 | `			SyBlobRelease(&sResolved);` |
|   40684 |  9050 | `		}` |
|   92597 |  9051 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - |  9052 | `			ph7_class *pInterface;` |
|       - |  9053 | `			/* Interface implementation */` |
|   11237 |  9054 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    5626 |  9055 | `			for(;;){` |
|       - |  9056 | `				SyBlob sResolved;` |
|       - |  9057 | `				SyString sIntName;` |
|       - |  9058 | `				sxu32 nRefLine;` |
|   11247 |  9059 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   11247 |  9060 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   11247 |  9061 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  9062 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  9063 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  9064 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 |  9065 | `						pName);` |
|     ! 0 |  9066 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9067 | `						return SXERR_ABORT;` |
|       - |  9068 | `					}` |
|     ! 0 |  9069 | `					break;` |
|       - |  9070 | `				}` |
|   22489 |  9071 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|   11242 |  9072 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   11247 |  9073 | `				SyStringInitFromBuf(&sIntName,` |
|       - |  9074 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  9075 | `				/* Only interfaces are allowed */` |
|   11247 |  9076 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  9077 | `					pInterface = pInterface->pNextName;` |
|     ! 0 |  9078 | `				}` |
|   11247 |  9079 | `				if( pInterface == 0 ){` |
|     ! 0 |  9080 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  9081 | `						"Nonexistent base interface '%z'",&sIntName);` |
|     ! 0 |  9082 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9083 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  9084 | `						return SXERR_ABORT;` |
|       - |  9085 | `					}` |
|     ! 0 |  9086 | `				}else{` |
|       - |  9087 | `					/* Reject user classes that try to implement Throwable` |
|       - |  9088 | `					 * directly (or via an interface that extends Throwable)` |
|       - |  9089 | `					 * unless they already extend Exception or Error.` |
|       - |  9090 | `					 * Exception and Error themselves are compiled from the` |
|       - |  9091 | `					 * built-in library and are exempt by FQN — a namespaced` |
|       - |  9092 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|   11247 |  9093 | `					SyString *pFqn = &pClass->sName;` |
|   11247 |  9094 | `					int bIsExceptionOrError =` |
|    9315 |  9095 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   18712 |  9096 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    9404 |  9097 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|    3704 |  9098 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   14937 |  9099 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|   11088 |  9100 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|    3693 |  9101 | `						!bIsExceptionOrError ){` |
|      12 |  9102 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9103 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|       3 |  9104 | `							&pClass->sName);` |
|       9 |  9105 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9106 | `							SyBlobRelease(&sResolved);` |
|     ! 0 |  9107 | `							return SXERR_ABORT;` |
|       - |  9108 | `						}` |
|       - |  9109 | `						/* Skip registration so the follow-up abstract-method` |
|       - |  9110 | `						 * check does not produce a duplicate fatal. */` |
|       6 |  9111 | `					}else{` |
|   11241 |  9112 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - |  9113 | `					}` |
|       - |  9114 | `				}` |
|   11247 |  9115 | `				SyBlobRelease(&sResolved);` |
|   11247 |  9116 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    5621 |  9117 | `					break;` |
|       - |  9118 | `				}` |
|      14 |  9119 | `				pGen->pIn++;/* Jump the comma */` |
|       4 |  9120 | `			}` |
|    5616 |  9121 | `		}` |
|   46296 |  9122 | `	}` |
|  108315 |  9123 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  9124 | `		/* Syntax error */` |
|     ! 0 |  9125 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 |  9126 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9127 | `		if( rc == SXERR_ABORT ){` |
|       - |  9128 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9129 | `			return SXERR_ABORT;` |
|       - |  9130 | `		}` |
|     ! 0 |  9131 | `		return SXRET_OK;` |
|       - |  9132 | `	}` |
|  108315 |  9133 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|  108315 |  9134 | `	pEnd = 0; /* cc warning */` |
|       - |  9135 | `	/* Delimit the class body */` |
|  108315 |  9136 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|  108315 |  9137 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  9138 | `		/* Syntax error */` |
|     ! 0 |  9139 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 |  9140 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9141 | `		if( rc == SXERR_ABORT ){` |
|       - |  9142 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9143 | `			return SXERR_ABORT;` |
|       - |  9144 | `		}` |
|     ! 0 |  9145 | `		return SXRET_OK;` |
|       - |  9146 | `	}` |
|       - |  9147 | `	/* Swap token stream */` |
|  108315 |  9148 | `	pTmp = pGen->pEnd;` |
|  108315 |  9149 | `	pGen->pEnd = pEnd;` |
|       - |  9150 | `	/* Set the inherited flags */` |
|  108315 |  9151 | `	pClass->iFlags = iFlags;` |
|       - |  9152 | `	/* Start the parse process */` |
|  145111 |  9153 | `	for(;;){` |
|       - |  9154 | `		/* Jump leading/trailing semi-colons */` |
|  446727 |  9155 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   78293 |  9156 | `			pGen->pIn++;` |
|       5 |  9157 | `		}` |
|  368439 |  9158 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9159 | `			/* End of class body */` |
|  108277 |  9160 | `			break;` |
|       - |  9161 | `		}` |
|  260162 |  9162 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|  130086 |  9163 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|     ! 0 |  9164 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9165 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  9166 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  9167 | `			if( rc == SXERR_ABORT ){` |
|       - |  9168 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  9169 | `				return SXERR_ABORT;` |
|       - |  9170 | `			}` |
|     ! 0 |  9171 | `			goto done;` |
|       - |  9172 | `		}` |
|       - |  9173 | `		/* Assume public visibility */` |
|  260167 |  9174 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  260167 |  9175 | `		iAttrflags = 0;` |
|       - |  9176 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|       - |  9177 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|       - |  9178 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|       - |  9179 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  260167 |  9180 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  9181 | `			int bMod = 0;` |
|     ! 0 |  9182 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  9183 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       - |  9184 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|       - |  9185 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|       - |  9186 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|       - |  9187 | `			 * that the generic keyword dispatch would misread as a method. */` |
|     ! 0 |  9188 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     ! 0 |  9189 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  9190 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|     ! 0 |  9191 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|     ! 0 |  9192 | `			}` |
|     ! 0 |  9193 | `			if( !bMod ){` |
|     ! 0 |  9194 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9195 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9196 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9197 | `						return SXERR_ABORT;` |
|       - |  9198 | `					}` |
|     ! 0 |  9199 | `					goto done;` |
|       - |  9200 | `				}` |
|     ! 0 |  9201 | `				continue;` |
|       - |  9202 | `			}` |
|     ! 0 |  9203 | `		}` |
|  260167 |  9204 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9205 | `			/* Extract the current keyword */` |
|  260167 |  9206 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  260167 |  9207 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  9208 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - |  9209 | `				TraitUseEntry sUse;` |
|      57 |  9210 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      57 |  9211 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      57 |  9212 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      34 |  9213 | `				for(;;){` |
|       - |  9214 | `					ph7_class *pTrait;` |
|       - |  9215 | `					SyString *pTraitName;` |
|      65 |  9216 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9217 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9218 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 |  9219 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9220 | `							return SXERR_ABORT;` |
|       - |  9221 | `						}` |
|     ! 0 |  9222 | `						break;` |
|       - |  9223 | `					}` |
|      65 |  9224 | `					pTraitName = &pGen->pIn->sData;` |
|       - |  9225 | `					/* Resolve trait name through namespace/imports */ {` |
|       - |  9226 | `						SyBlob sResolved;` |
|      65 |  9227 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      65 |  9228 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     125 |  9229 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      60 |  9230 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      65 |  9231 | `						SyBlobRelease(&sResolved);` |
|       - |  9232 | `					}` |
|       - |  9233 | `					/* Only traits are allowed */` |
|      65 |  9234 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  9235 | `						pTrait = pTrait->pNextName;` |
|     ! 0 |  9236 | `					}` |
|      65 |  9237 | `					if( pTrait == 0 ){` |
|     ! 0 |  9238 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9239 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 |  9240 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9241 | `							return SXERR_ABORT;` |
|       - |  9242 | `						}` |
|     ! 0 |  9243 | `					}else{` |
|      65 |  9244 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - |  9245 | `					}` |
|      65 |  9246 | `					pGen->pIn++; /* Advance past trait name */` |
|      65 |  9247 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      31 |  9248 | `						break;` |
|       - |  9249 | `					}` |
|      10 |  9250 | `					pGen->pIn++; /* Jump the comma */` |
|       2 |  9251 | `				}` |
|       - |  9252 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      57 |  9253 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - |  9254 | `					SyToken *pBlock;` |
|      13 |  9255 | `					pGen->pIn++; /* Jump '{' */` |
|      13 |  9256 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|      13 |  9257 | `					sUse.pResolvStart = pGen->pIn;` |
|      13 |  9258 | `					sUse.pResolvEnd = pBlock;` |
|      13 |  9259 | `					if( pBlock < pGen->pEnd ){` |
|      13 |  9260 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       8 |  9261 | `					}else{` |
|     ! 0 |  9262 | `						pGen->pIn = pGen->pEnd;` |
|       - |  9263 | `					}` |
|       5 |  9264 | `				}` |
|      57 |  9265 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - |  9266 | `				/* The semicolon will be consumed by the outer loop */` |
|      57 |  9267 | `				continue;` |
|       - |  9268 | `			}` |
|  260115 |  9269 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  259803 |  9270 | `				iProtection = nKwrd;` |
|  259803 |  9271 | `				pGen->pIn++; /* Jump the visibility token */` |
|       - |  9272 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`. */`` |
|  259803 |  9273 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      20 |  9274 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      20 |  9275 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       8 |  9276 | `				}` |
|  259798 |  9277 | `				if( pGen->pIn >= pGen->pEnd` |
|  259803 |  9278 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9279 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9280 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  9281 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9282 | `					if( rc == SXERR_ABORT ){` |
|       - |  9283 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  9284 | `						return SXERR_ABORT;` |
|       - |  9285 | `					}` |
|     ! 0 |  9286 | `					goto done;` |
|       - |  9287 | `				}` |
|  259803 |  9288 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  9289 | `					/* Attribute declaration (untyped) */` |
|   77985 |  9290 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   77985 |  9291 | `					if( rc != SXRET_OK ){` |
|      11 |  9292 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9293 | `							return SXERR_ABORT;` |
|       - |  9294 | `						}` |
|      11 |  9295 | `						goto done;` |
|       - |  9296 | `					}` |
|   77977 |  9297 | `					continue;` |
|       - |  9298 | `				}` |
|  181823 |  9299 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  9300 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     173 |  9301 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     173 |  9302 | `					if( rc != SXRET_OK ){` |
|       8 |  9303 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9304 | `							return SXERR_ABORT;` |
|       - |  9305 | `						}` |
|       8 |  9306 | `						goto done;` |
|       - |  9307 | `					}` |
|     167 |  9308 | `					continue;` |
|       - |  9309 | `				}` |
|       - |  9310 | `				/* Extract the keyword */` |
|  181655 |  9311 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   90825 |  9312 | `			}` |
|  181967 |  9313 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  9314 | `				/* Process constant declaration */` |
|      81 |  9315 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      81 |  9316 | `				if( rc != SXRET_OK ){` |
|      11 |  9317 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9318 | `						return SXERR_ABORT;` |
|       - |  9319 | `					}` |
|      11 |  9320 | `					goto done;` |
|       - |  9321 | `				}` |
|      39 |  9322 | `			}else{` |
|  181891 |  9323 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  9324 | `					/* Static method or attribute,record that */` |
|   11139 |  9325 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|   11139 |  9326 | `					pGen->pIn++; /* Jump the static keyword */` |
|   11139 |  9327 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9328 | `						/* Extract the keyword */` |
|   11129 |  9329 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   11129 |  9330 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  9331 | `							iProtection = nKwrd;` |
|     ! 0 |  9332 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 |  9333 | `						}` |
|    5562 |  9334 | `					}` |
|       - |  9335 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|       - |  9336 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|       - |  9337 | `					 * than a generic "expecting method" parse error. */` |
|   11139 |  9338 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  9339 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  9340 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|     ! 0 |  9341 | `					}` |
|   11134 |  9342 | `					if( pGen->pIn >= pGen->pEnd` |
|   11139 |  9343 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9344 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9345 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 |  9346 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9347 | `						if( rc == SXERR_ABORT ){` |
|       - |  9348 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9349 | `							return SXERR_ABORT;` |
|       - |  9350 | `						}` |
|     ! 0 |  9351 | `						goto done;` |
|       - |  9352 | `					}` |
|   11139 |  9353 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  9354 | `						/* Attribute declaration */` |
|      11 |  9355 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      11 |  9356 | `						if( rc != SXRET_OK ){` |
|       3 |  9357 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9358 | `								return SXERR_ABORT;` |
|       - |  9359 | `							}` |
|       3 |  9360 | `							goto done;` |
|       - |  9361 | `						}` |
|       8 |  9362 | `						continue;` |
|       - |  9363 | `					}` |
|   11131 |  9364 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  9365 | `						/* Typed static attribute declaration */` |
|      15 |  9366 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      15 |  9367 | `						if( rc != SXRET_OK ){` |
|       3 |  9368 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9369 | `								return SXERR_ABORT;` |
|       - |  9370 | `							}` |
|       3 |  9371 | `							goto done;` |
|       - |  9372 | `						}` |
|      13 |  9373 | `						continue;` |
|       - |  9374 | `					}` |
|       - |  9375 | `					/* Extract the keyword */` |
|   11119 |  9376 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  176314 |  9377 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - |  9378 | `					/* Abstract method,record that */` |
|      15 |  9379 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - |  9380 | `					/* Mark the whole class as abstract */` |
|      15 |  9381 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - |  9382 | `					/* Advance the stream cursor */` |
|      15 |  9383 | `					pGen->pIn++;` |
|      15 |  9384 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      15 |  9385 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      15 |  9386 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      13 |  9387 | `							iProtection = nKwrd;` |
|      13 |  9388 | `							pGen->pIn++; /* Jump the visibility token */` |
|       5 |  9389 | `						}` |
|       6 |  9390 | `					}` |
|      15 |  9391 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      12 |  9392 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  9393 | `							/* Static method */` |
|     ! 0 |  9394 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  9395 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  9396 | `					}` |
|      15 |  9397 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      12 |  9398 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9399 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9400 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 |  9401 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  9402 | `							if( rc == SXERR_ABORT ){` |
|       - |  9403 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  9404 | `								return SXERR_ABORT;` |
|       - |  9405 | `							}` |
|     ! 0 |  9406 | `							goto done;` |
|       - |  9407 | `					}` |
|      15 |  9408 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  170751 |  9409 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - |  9410 | `					/* final method ,record that */` |
|      17 |  9411 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|      17 |  9412 | `					pGen->pIn++; /* Jump the final keyword */` |
|      17 |  9413 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9414 | `						/* Extract the keyword */` |
|      17 |  9415 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      17 |  9416 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       9 |  9417 | `							iProtection = nKwrd;` |
|       9 |  9418 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 |  9419 | `						}` |
|       7 |  9420 | `					}` |
|      17 |  9421 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      14 |  9422 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|       - |  9423 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|       - |  9424 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|       - |  9425 | `							 * child class is compiled (PH7_ClassInherit). */` |
|      12 |  9426 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  9427 | `							if( rc != SXRET_OK ){` |
|     ! 0 |  9428 | `								if( rc == SXERR_ABORT ){` |
|     ! 0 |  9429 | `									return SXERR_ABORT;` |
|       - |  9430 | `								}` |
|     ! 0 |  9431 | `								goto done;` |
|       - |  9432 | `							}` |
|      12 |  9433 | `							continue;` |
|       - |  9434 | `					}` |
|       6 |  9435 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 |  9436 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  9437 | `							/* Static method */` |
|     ! 0 |  9438 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  9439 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  9440 | `					}` |
|       6 |  9441 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  9442 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9443 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9444 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 |  9445 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  9446 | `							if( rc == SXERR_ABORT ){` |
|       - |  9447 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  9448 | `								return SXERR_ABORT;` |
|       - |  9449 | `							}` |
|     ! 0 |  9450 | `							goto done;` |
|       - |  9451 | `					}` |
|       6 |  9452 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  9453 | `				}` |
|  181861 |  9454 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  9455 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9456 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 |  9457 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9458 | `						if( rc == SXERR_ABORT ){` |
|       - |  9459 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9460 | `							return SXERR_ABORT;` |
|       - |  9461 | `						}` |
|     ! 0 |  9462 | `						goto done;` |
|       - |  9463 | `				}` |
|  181861 |  9464 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 |  9465 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 |  9466 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 |  9467 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9468 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  9469 | `						if( rc == SXERR_ABORT ){` |
|       - |  9470 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9471 | `							return SXERR_ABORT;` |
|       - |  9472 | `						}` |
|     ! 0 |  9473 | `						goto done;` |
|       - |  9474 | `					}` |
|       - |  9475 | `					/* Attribute declaration */` |
|       7 |  9476 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 |  9477 | `				}else{` |
|       - |  9478 | `					/* Process method declaration */` |
|  181855 |  9479 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9480 | `				}` |
|  181861 |  9481 | `				if( rc != SXRET_OK ){` |
|      16 |  9482 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9483 | `						return SXERR_ABORT;` |
|       - |  9484 | `					}` |
|      16 |  9485 | `					goto done;` |
|       - |  9486 | `				}` |
|       - |  9487 | `			}` |
|   90961 |  9488 | `		}else{` |
|       - |  9489 | `			/* Attribute declaration */` |
|     ! 0 |  9490 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9491 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9492 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9493 | `					return SXERR_ABORT;` |
|       - |  9494 | `				}` |
|     ! 0 |  9495 | `				goto done;` |
|       - |  9496 | `			}` |
|       - |  9497 | `		}` |
|       5 |  9498 | `	}` |
|       - |  9499 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - |  9500 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - |  9501 | `	 */` |
|       - |  9502 | `	{` |
|       - |  9503 | `		TraitUseEntry *apUse;` |
|       - |  9504 | `		sxu32 nU;` |
|  108277 |  9505 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|  108329 |  9506 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      57 |  9507 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      57 |  9508 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      57 |  9509 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      57 |  9510 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - |  9511 | `			sxu32 nT;` |
|      57 |  9512 | `			if( !hasResolution ){` |
|       - |  9513 | `				/* No conflict resolution block: use standard trait application */` |
|      95 |  9514 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      53 |  9515 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      53 |  9516 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9517 | `						break;` |
|       - |  9518 | `					}` |
|      29 |  9519 | `				}` |
|      26 |  9520 | `			}else{` |
|       - |  9521 | `				/* With resolution block: copy attributes, record traits,` |
|       - |  9522 | `				 * then use the block to resolve method conflicts.` |
|       - |  9523 | `				 */` |
|       - |  9524 | `				SyToken *pR;` |
|      25 |  9525 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      15 |  9526 | `					ph7_class *pTR = apTrait[nT];` |
|       - |  9527 | `					ph7_class_attr *pAR;` |
|       - |  9528 | `					SyHashEntry *pER;` |
|       - |  9529 | `					SyString *pNR;` |
|      15 |  9530 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      21 |  9531 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 |  9532 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 |  9533 | `						pNR = &pAR->sName;` |
|     ! 0 |  9534 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 |  9535 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 |  9536 | `						}` |
|     ! 0 |  9537 | `					}` |
|      15 |  9538 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       9 |  9539 | `				}` |
|       - |  9540 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|      13 |  9541 | `				pR = pUse->pResolvStart;` |
|      27 |  9542 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  9543 | `					SyString sTrait,sMethod;` |
|       - |  9544 | `					ph7_class *pSrcTrait;` |
|       - |  9545 | `					ph7_class_method *pMeth;` |
|       - |  9546 | `					sxi32 nRKwrd;` |
|      41 |  9547 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      27 |  9548 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      17 |  9549 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      17 |  9550 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      17 |  9551 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      17 |  9552 | `					sMethod = pR->sData;` |
|      17 |  9553 | `					pR++;` |
|      17 |  9554 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9555 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9556 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9557 | `							sTrait = sMethod;` |
|       7 |  9558 | `							pR++;` |
|       7 |  9559 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9560 | `							sMethod = pR->sData;` |
|       7 |  9561 | `							pR++;` |
|       3 |  9562 | `						}` |
|       3 |  9563 | `					}` |
|      17 |  9564 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9565 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  9566 | `						continue;` |
|       - |  9567 | `					}` |
|      17 |  9568 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      17 |  9569 | `					pR++;` |
|      17 |  9570 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 |  9571 | `						pSrcTrait = 0;` |
|       7 |  9572 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 |  9573 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 |  9574 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 |  9575 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 |  9576 | `								pSrcTrait = apTrait[nT];` |
|       5 |  9577 | `								break;` |
|       - |  9578 | `							}` |
|       2 |  9579 | `						}` |
|       5 |  9580 | `						if( pSrcTrait ){` |
|       5 |  9581 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 |  9582 | `							if( pMeth ){` |
|       5 |  9583 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 |  9584 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 |  9585 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 |  9586 | `								}` |
|       2 |  9587 | `							}` |
|       2 |  9588 | `						}` |
|       2 |  9589 | `					}` |
|      35 |  9590 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       3 |  9591 | `				}` |
|       - |  9592 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      25 |  9593 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - |  9594 | `					ph7_class_method *pMR;` |
|       - |  9595 | `					SyHashEntry *pER;` |
|       - |  9596 | `					SyString *pNR;` |
|      15 |  9597 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      41 |  9598 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      23 |  9599 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      23 |  9600 | `						pNR = &pMR->sFunc.sName;` |
|      23 |  9601 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      14 |  9602 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       6 |  9603 | `						}` |
|       3 |  9604 | `					}` |
|       9 |  9605 | `				}` |
|       - |  9606 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|      13 |  9607 | `				pR = pUse->pResolvStart;` |
|      27 |  9608 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  9609 | `					SyString sTrait,sMethod,sAlias;` |
|       - |  9610 | `					ph7_class *pSrcTrait;` |
|       - |  9611 | `					ph7_class_method *pMeth;` |
|      27 |  9612 | `					int hasQual = 0;` |
|       - |  9613 | `					sxi32 nRKwrd;` |
|      41 |  9614 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      27 |  9615 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      17 |  9616 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      17 |  9617 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      17 |  9618 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      17 |  9619 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      17 |  9620 | `					sMethod = pR->sData;` |
|      17 |  9621 | `					pR++;` |
|      17 |  9622 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9623 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9624 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9625 | `							sTrait = sMethod;` |
|       7 |  9626 | `							hasQual = 1;` |
|       7 |  9627 | `							pR++;` |
|       7 |  9628 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9629 | `							sMethod = pR->sData;` |
|       7 |  9630 | `							pR++;` |
|       3 |  9631 | `						}` |
|       3 |  9632 | `					}` |
|      17 |  9633 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9634 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  9635 | `						continue;` |
|       - |  9636 | `					}` |
|      17 |  9637 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      17 |  9638 | `					pR++;` |
|      17 |  9639 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|      13 |  9640 | `						sxi32 iNewVis = -1;` |
|      13 |  9641 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 |  9642 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 |  9643 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 |  9644 | `								iNewVis = nAK;` |
|       7 |  9645 | `								pR++;` |
|       3 |  9646 | `							}` |
|       3 |  9647 | `						}` |
|      13 |  9648 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|      11 |  9649 | `							sAlias = pR->sData;` |
|      11 |  9650 | `							pR++;` |
|       4 |  9651 | `						}` |
|      13 |  9652 | `						pMeth = 0;` |
|      13 |  9653 | `						if( hasQual ){` |
|       3 |  9654 | `							pSrcTrait = 0;` |
|       5 |  9655 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 |  9656 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 |  9657 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 |  9658 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 |  9659 | `									pSrcTrait = apTrait[nT];` |
|       3 |  9660 | `									break;` |
|       - |  9661 | `								}` |
|       2 |  9662 | `							}` |
|       3 |  9663 | `							if( pSrcTrait ){` |
|       3 |  9664 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 |  9665 | `							}` |
|       2 |  9666 | `						}else{` |
|      10 |  9667 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - |  9668 | `						}` |
|      13 |  9669 | `						if( pMeth ){` |
|      13 |  9670 | `							if( sAlias.nByte > 0 ){` |
|       - |  9671 | `								/* Create a shallow copy of the method struct for the alias` |
|       - |  9672 | `								 * so it can carry its own visibility without affecting the original.` |
|       - |  9673 | `								 */` |
|       - |  9674 | `								ph7_class_method *pAlias;` |
|       - |  9675 | `								char *zAliasDup;` |
|      11 |  9676 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|      11 |  9677 | `								if( pAlias ){` |
|      11 |  9678 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|      11 |  9679 | `									if( iNewVis >= 0 ){` |
|       5 |  9680 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9681 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9682 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 |  9683 | `									}` |
|      11 |  9684 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      11 |  9685 | `									if( zAliasDup ){` |
|      11 |  9686 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       4 |  9687 | `									}` |
|       7 |  9688 | `								}` |
|       7 |  9689 | `							}else if( iNewVis >= 0 ){` |
|       - |  9690 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - |  9691 | `								ph7_class_method *pCopy;` |
|       3 |  9692 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 |  9693 | `								if( pCopy ){` |
|       3 |  9694 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 |  9695 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 |  9696 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9697 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9698 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - |  9699 | `									/* Replace the method in the class hash */` |
|       3 |  9700 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 |  9701 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 |  9702 | `								}` |
|       1 |  9703 | `							}` |
|       5 |  9704 | `						}` |
|       5 |  9705 | `						SXUNUSED(hasQual);` |
|       5 |  9706 | `					}` |
|      21 |  9707 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       3 |  9708 | `				}` |
|       - |  9709 | `			}` |
|      57 |  9710 | `			SySetRelease(&pUse->aTraits);` |
|      31 |  9711 | `		}` |
|       - |  9712 | `	}` |
|       - |  9713 | `	/* Install the class */` |
|  108277 |  9714 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|  108277 |  9715 | `	if( rc == SXRET_OK ){` |
|       - |  9716 | `		ph7_class **apInterface;` |
|       - |  9717 | `		sxu32 n;` |
|  108277 |  9718 | `		if( pBase ){` |
|       - |  9719 | `			/* Inherit from base class and mark as a subclass */` |
|   81373 |  9720 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   40684 |  9721 | `		}` |
|  108277 |  9722 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|  119513 |  9723 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - |  9724 | `			/* Implements one or more interface */` |
|   11241 |  9725 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|   11241 |  9726 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9727 | `				break;` |
|       - |  9728 | `			}` |
|    5623 |  9729 | `		}` |
|       - |  9730 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|       - |  9731 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|  108272 |  9732 | `		if( rc == SXRET_OK` |
|  108272 |  9733 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|  108277 |  9734 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   88655 |  9735 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|       - |  9736 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   88655 |  9737 | `			if( pStringable ){` |
|   88655 |  9738 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   88655 |  9739 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|       - |  9740 | `				sxu32 i;` |
|   88655 |  9741 | `				int bAlready = 0;` |
|   96039 |  9742 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    7391 |  9743 | `					if( apImpl[i] == pStringable ){` |
|       3 |  9744 | `						bAlready = 1;` |
|       3 |  9745 | `						break;` |
|       - |  9746 | `					}` |
|    3697 |  9747 | `				}` |
|   88655 |  9748 | `				if( !bAlready ){` |
|   88653 |  9749 | `					PH7_ClassImplement(pClass,pStringable);` |
|   44324 |  9750 | `				}` |
|   44325 |  9751 | `			}` |
|   44325 |  9752 | `		}` |
|       - |  9753 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|  108277 |  9754 | `		if( rc == SXRET_OK ){` |
|  108277 |  9755 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|  108277 |  9756 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  9757 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  9758 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  9759 | `				return SXERR_ABORT;` |
|       - |  9760 | `			}` |
|   54136 |  9761 | `		}` |
|       - |  9762 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|  108277 |  9763 | `		if( rc == SXRET_OK ){` |
|  108277 |  9764 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|  108277 |  9765 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  9766 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  9767 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  9768 | `				return SXERR_ABORT;` |
|       - |  9769 | `			}` |
|   54136 |  9770 | `		}` |
|   54136 |  9771 | `	}` |
|  108277 |  9772 | `	SySetRelease(&aUseEntries);` |
|  108277 |  9773 | `	SySetRelease(&aInterfaces);` |
|  108277 |  9774 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9775 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9776 | `		return SXERR_ABORT;` |
|       - |  9777 | `	}` |
|   54136 |  9778 | `done:` |
|       - |  9779 | `	/* Point beyond the class body */` |
|  108315 |  9780 | `	pGen->pIn = &pEnd[1];` |
|  108315 |  9781 | `	pGen->pEnd = pTmp;` |
|  108315 |  9782 | `	return PH7_OK;` |
|   54161 |  9783 | `}` |
|       - |  9784 | `/* Compile a named class declaration (the common case). */` |
|  108286 |  9785 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       5 |  9786 | `{` |
|  108291 |  9787 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|       5 |  9788 | `}` |
|       - |  9789 | `/*` |
|       - |  9790 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|       - |  9791 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|       - |  9792 | ` * compile + install the class body once (at compile time, like every other` |
|       - |  9793 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|       - |  9794 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|       - |  9795 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|       - |  9796 | ` */` |
|      26 |  9797 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  9798 | `{` |
|       - |  9799 | `	char zName[128];         /* Synthesized class name */` |
|       - |  9800 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|       - |  9801 | `	SyString sName;` |
|       - |  9802 | `	SyToken *pArgStart,*pArgEnd;` |
|       - |  9803 | `	ph7_value *pObj;` |
|      30 |  9804 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9805 | `	sxu32 nIdx,nLen;` |
|       - |  9806 | `	sxi32 nArg,rc;` |
|      13 |  9807 | `	SXUNUSED(iCompileFlag);` |
|       - |  9808 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|      30 |  9809 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      30 |  9810 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  9811 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|     ! 0 |  9812 | `	}` |
|      30 |  9813 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - |  9814 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|       - |  9815 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|       - |  9816 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|      30 |  9817 | `	pArgStart = pArgEnd = 0;` |
|      30 |  9818 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|      30 |  9819 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9820 | `		return rc;` |
|       - |  9821 | `	}` |
|       - |  9822 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|       - |  9823 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|      30 |  9824 | `	nArg = 0;` |
|      30 |  9825 | `	if( pArgStart < pArgEnd ){` |
|       7 |  9826 | `		SyToken *pSavedIn = pGen->pIn;` |
|       7 |  9827 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  9828 | `		SyToken *pArgNext;` |
|       7 |  9829 | `		pGen->pIn = pArgStart;` |
|       7 |  9830 | `		pGen->pEnd = pArgEnd;` |
|      13 |  9831 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|       7 |  9832 | `			if( pGen->pIn < pArgNext ){` |
|       7 |  9833 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|       7 |  9834 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9835 | `					pGen->pIn = pSavedIn;` |
|     ! 0 |  9836 | `					pGen->pEnd = pSavedEnd;` |
|     ! 0 |  9837 | `					return SXERR_ABORT;` |
|       - |  9838 | `				}` |
|       7 |  9839 | `				nArg++;` |
|       3 |  9840 | `			}` |
|       7 |  9841 | `			pGen->pIn = &pArgNext[1];` |
|       1 |  9842 | `		}` |
|       7 |  9843 | `		pGen->pIn = pSavedIn;` |
|       7 |  9844 | `		pGen->pEnd = pSavedEnd;` |
|       3 |  9845 | `	}` |
|       - |  9846 | `	/* Load the synthesized class name */` |
|      30 |  9847 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      30 |  9848 | `	if( pObj == 0 ){` |
|     ! 0 |  9849 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9850 | `		return SXERR_ABORT;` |
|       - |  9851 | `	}` |
|      30 |  9852 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      30 |  9853 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  9854 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|      30 |  9855 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|      30 |  9856 | `	return SXRET_OK;` |
|      17 |  9857 | `}` |
|       - |  9858 | `/*` |
|       - |  9859 | ` * Compile a user-defined abstract class.` |
|       - |  9860 | ` *  According to the PHP language reference manual` |
|       - |  9861 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - |  9862 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - |  9863 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - |  9864 | ` *   the method's signature - they cannot define the implementation.` |
|       - |  9865 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - |  9866 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - |  9867 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - |  9868 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - |  9869 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - |  9870 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - |  9871 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - |  9872 | ` *   could differ.` |
|       - |  9873 | ` */` |
|       - |  9874 | `/*` |
|       - |  9875 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|       - |  9876 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|       - |  9877 | ` * receives the corresponding PH7_CLASS_* bit.` |
|       - |  9878 | ` */` |
| 1019794 |  9879 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|       5 |  9880 | `{` |
| 1019799 |  9881 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  684787 |  9882 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  684787 |  9883 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  677389 |  9884 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  338661 |  9885 | `	}` |
| 1012339 |  9886 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
| 1012279 |  9887 | `	return FALSE;` |
|  509902 |  9888 | `}` |
|       - |  9889 | `/*` |
|       - |  9890 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|       - |  9891 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|       - |  9892 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|       - |  9893 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|       - |  9894 | ` */` |
| 1012274 |  9895 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|       5 |  9896 | `{` |
| 1012279 |  9897 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
| 1012279 |  9898 | `	sxi32 iFlags = 0,iFlag;` |
| 1019799 |  9899 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|    7525 |  9900 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|       5 |  9901 | `			pDup = pIn;` |
|       2 |  9902 | `		}` |
|    7525 |  9903 | `		iFlags \|= iFlag;` |
|    7525 |  9904 | `		pIn++;` |
|       5 |  9905 | `	}` |
| 1012279 |  9906 | `	*ppIn = pIn;` |
| 1012279 |  9907 | `	if( ppDup ){ *ppDup = pDup; }` |
| 1012279 |  9908 | `	return iFlags;` |
|       5 |  9909 | `}` |
|       - |  9910 | `/*` |
|       - |  9911 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|       - |  9912 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|       - |  9913 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|       - |  9914 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|       - |  9915 | `` * `readonly`) to their existing handlers.`` |
|       - |  9916 | ` */` |
| 1008524 |  9917 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|       5 |  9918 | `{` |
| 1008529 |  9919 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  508019 |  9920 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
| 1010401 |  9921 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|       5 |  9922 | `}` |
|       - |  9923 | `/*` |
|       - |  9924 | ` * Compile a class declaration carrying one or more leading modifiers` |
|       - |  9925 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|       - |  9926 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|       - |  9927 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|       - |  9928 | `` * `abstract`+`final` pair, like PHP.`` |
|       - |  9929 | ` */` |
|    3750 |  9930 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|       5 |  9931 | `{` |
|       - |  9932 | `	SyToken *pDup;` |
|    3755 |  9933 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|       - |  9934 | `	sxi32 rc;` |
|    3755 |  9935 | `	if( pDup ){` |
|       4 |  9936 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|       2 |  9937 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|       3 |  9938 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9939 | `			return SXERR_ABORT;` |
|       - |  9940 | `		}` |
|       1 |  9941 | `	}` |
|    3750 |  9942 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|    1880 |  9943 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|       3 |  9944 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9945 | `			"Cannot use the final modifier on an abstract class");` |
|       3 |  9946 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9947 | `			return SXERR_ABORT;` |
|       - |  9948 | `		}` |
|       1 |  9949 | `	}` |
|    3755 |  9950 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|    1880 |  9951 | `}` |
|       - |  9952 | `/*` |
|       - |  9953 | ` * Compile a user-defined trait.` |
|       - |  9954 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - |  9955 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - |  9956 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - |  9957 | ` */` |
|      64 |  9958 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       5 |  9959 | `{` |
|      69 |  9960 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9961 | `	ph7_class *pClass;` |
|       - |  9962 | `	SyToken *pEnd,*pTmp;` |
|       - |  9963 | `	sxi32 iProtection;` |
|       - |  9964 | `	sxi32 iAttrflags;` |
|       - |  9965 | `	SyString *pName;` |
|       - |  9966 | `	sxi32 nKwrd;` |
|       - |  9967 | `	sxi32 rc;` |
|       - |  9968 | `	/* Jump the 'trait' keyword */` |
|      69 |  9969 | `	pGen->pIn++;` |
|      69 |  9970 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9971 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 |  9972 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9973 | `			return SXERR_ABORT;` |
|       - |  9974 | `		}` |
|     ! 0 |  9975 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 |  9976 | `			pGen->pIn++;` |
|     ! 0 |  9977 | `		}` |
|     ! 0 |  9978 | `		return SXRET_OK;` |
|       - |  9979 | `	}` |
|       - |  9980 | `	/* Extract trait name */` |
|      69 |  9981 | `	pName = &pGen->pIn->sData;` |
|      69 |  9982 | `	pGen->pIn++;` |
|       - |  9983 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  9984 | `		SyBlob sFQN;` |
|       - |  9985 | `		SyString sFQNStr;` |
|      69 |  9986 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      69 |  9987 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      69 |  9988 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      69 |  9989 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      69 |  9990 | `		SyBlobRelease(&sFQN);` |
|       - |  9991 | `	}` |
|      69 |  9992 | `	if( pClass == 0 ){` |
|     ! 0 |  9993 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9994 | `		return SXERR_ABORT;` |
|       - |  9995 | `	}` |
|       - |  9996 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      69 |  9997 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  9998 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 |  9999 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 10000 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10001 | `			return SXERR_ABORT;` |
|       - | 10002 | `		}` |
|     ! 0 | 10003 | `		return SXRET_OK;` |
|       - | 10004 | `	}` |
|      69 | 10005 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      69 | 10006 | `	pEnd = 0;` |
|      69 | 10007 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      69 | 10008 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 10009 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 | 10010 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 10011 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10012 | `			return SXERR_ABORT;` |
|       - | 10013 | `		}` |
|     ! 0 | 10014 | `		return SXRET_OK;` |
|       - | 10015 | `	}` |
|       - | 10016 | `	/* Swap token stream */` |
|      69 | 10017 | `	pTmp = pGen->pEnd;` |
|      69 | 10018 | `	pGen->pEnd = pEnd;` |
|       - | 10019 | `	/* Mark as trait */` |
|      69 | 10020 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - | 10021 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      64 | 10022 | `	for(;;){` |
|     177 | 10023 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      28 | 10024 | `			pGen->pIn++;` |
|       4 | 10025 | `		}` |
|     153 | 10026 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      69 | 10027 | `			break;` |
|       - | 10028 | `		}` |
|      89 | 10029 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 10030 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10031 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 10032 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 10033 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10034 | `				return SXERR_ABORT;` |
|       - | 10035 | `			}` |
|     ! 0 | 10036 | `			goto done;` |
|       - | 10037 | `		}` |
|      89 | 10038 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      89 | 10039 | `		iAttrflags = 0;` |
|      89 | 10040 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      89 | 10041 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      89 | 10042 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 10043 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 | 10044 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 | 10045 | `				for(;;){` |
|       - | 10046 | `					ph7_class *pUsedTrait;` |
|       - | 10047 | `					SyString *pUsedName;` |
|       5 | 10048 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 10049 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 10050 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 | 10051 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10052 | `							return SXERR_ABORT;` |
|       - | 10053 | `						}` |
|     ! 0 | 10054 | `						break;` |
|       - | 10055 | `					}` |
|       5 | 10056 | `					pUsedName = &pGen->pIn->sData;` |
|       - | 10057 | `					{` |
|       - | 10058 | `						SyBlob sResolved;` |
|       5 | 10059 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 | 10060 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 | 10061 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 | 10062 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 | 10063 | `						SyBlobRelease(&sResolved);` |
|       - | 10064 | `					}` |
|       5 | 10065 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 10066 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 | 10067 | `					}` |
|       5 | 10068 | `					if( pUsedTrait == 0 ){` |
|       4 | 10069 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 | 10070 | `							"'%z' is not a trait",pUsedName);` |
|       3 | 10071 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10072 | `							return SXERR_ABORT;` |
|       - | 10073 | `						}` |
|       2 | 10074 | `					}else{` |
|       3 | 10075 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - | 10076 | `					}` |
|       5 | 10077 | `					pGen->pIn++;` |
|       5 | 10078 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 | 10079 | `						break;` |
|       - | 10080 | `					}` |
|     ! 0 | 10081 | `					pGen->pIn++;` |
|     ! 0 | 10082 | `				}` |
|       5 | 10083 | `				continue;` |
|       - | 10084 | `			}` |
|      85 | 10085 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      73 | 10086 | `				iProtection = nKwrd;` |
|      73 | 10087 | `				pGen->pIn++;` |
|      68 | 10088 | `				if( pGen->pIn >= pGen->pEnd` |
|      73 | 10089 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 | 10090 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10091 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 10092 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 10093 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10094 | `						return SXERR_ABORT;` |
|       - | 10095 | `					}` |
|     ! 0 | 10096 | `					goto done;` |
|       - | 10097 | `				}` |
|      73 | 10098 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      12 | 10099 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 | 10100 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 10101 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10102 | `							return SXERR_ABORT;` |
|       - | 10103 | `						}` |
|     ! 0 | 10104 | `						goto done;` |
|       - | 10105 | `					}` |
|      12 | 10106 | `					continue;` |
|       - | 10107 | `				}` |
|      63 | 10108 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 | 10109 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 | 10110 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 10111 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10112 | `							return SXERR_ABORT;` |
|       - | 10113 | `						}` |
|     ! 0 | 10114 | `						goto done;` |
|       - | 10115 | `					}` |
|       5 | 10116 | `					continue;` |
|       - | 10117 | `				}` |
|      58 | 10118 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      27 | 10119 | `			}` |
|      71 | 10120 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 | 10121 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10122 | `					"Traits cannot have constants");` |
|     ! 0 | 10123 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10124 | `					return SXERR_ABORT;` |
|       - | 10125 | `				}` |
|     ! 0 | 10126 | `				goto done;` |
|     ! 0 | 10127 | `			}else{` |
|      71 | 10128 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 | 10129 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 | 10130 | `					pGen->pIn++;` |
|       5 | 10131 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 10132 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 10133 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 10134 | `							iProtection = nKwrd;` |
|     ! 0 | 10135 | `							pGen->pIn++;` |
|     ! 0 | 10136 | `						}` |
|       1 | 10137 | `					}` |
|       4 | 10138 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 | 10139 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 | 10140 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10141 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 | 10142 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 10143 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10144 | `							return SXERR_ABORT;` |
|       - | 10145 | `						}` |
|     ! 0 | 10146 | `						goto done;` |
|       - | 10147 | `					}` |
|       5 | 10148 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 | 10149 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 | 10150 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 10151 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 10152 | `								return SXERR_ABORT;` |
|       - | 10153 | `							}` |
|     ! 0 | 10154 | `							goto done;` |
|       - | 10155 | `						}` |
|       3 | 10156 | `						continue;` |
|       - | 10157 | `					}` |
|       3 | 10158 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 | 10159 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 10160 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 10161 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 10162 | `								return SXERR_ABORT;` |
|       - | 10163 | `							}` |
|     ! 0 | 10164 | `							goto done;` |
|       - | 10165 | `						}` |
|     ! 0 | 10166 | `						continue;` |
|       - | 10167 | `					}` |
|       3 | 10168 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      68 | 10169 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       6 | 10170 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       6 | 10171 | `					pGen->pIn++;` |
|       6 | 10172 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       6 | 10173 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       6 | 10174 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 | 10175 | `							iProtection = nKwrd;` |
|       6 | 10176 | `							pGen->pIn++;` |
|       2 | 10177 | `						}` |
|       2 | 10178 | `					}` |
|       6 | 10179 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 10180 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 10181 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10182 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 | 10183 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 10184 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10185 | `							return SXERR_ABORT;` |
|       - | 10186 | `						}` |
|     ! 0 | 10187 | `						goto done;` |
|       - | 10188 | `					}` |
|       6 | 10189 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 10190 | `				}` |
|      69 | 10191 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 10192 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10193 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 | 10194 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 10195 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10196 | `						return SXERR_ABORT;` |
|       - | 10197 | `					}` |
|     ! 0 | 10198 | `					goto done;` |
|       - | 10199 | `				}` |
|      69 | 10200 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 | 10201 | `					pGen->pIn++;` |
|     ! 0 | 10202 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 10203 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10204 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 10205 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 10206 | `							return SXERR_ABORT;` |
|       - | 10207 | `						}` |
|     ! 0 | 10208 | `						goto done;` |
|       - | 10209 | `					}` |
|     ! 0 | 10210 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 10211 | `				}else{` |
|      69 | 10212 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 10213 | `				}` |
|      69 | 10214 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10215 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10216 | `						return SXERR_ABORT;` |
|       - | 10217 | `					}` |
|     ! 0 | 10218 | `					goto done;` |
|       - | 10219 | `				}` |
|       - | 10220 | `			}` |
|      37 | 10221 | `		}else{` |
|     ! 0 | 10222 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 10223 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10224 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10225 | `					return SXERR_ABORT;` |
|       - | 10226 | `				}` |
|     ! 0 | 10227 | `				goto done;` |
|       - | 10228 | `			}` |
|       - | 10229 | `		}` |
|       5 | 10230 | `	}` |
|       - | 10231 | `	/* Install the trait */` |
|      69 | 10232 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      69 | 10233 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10234 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 10235 | `		return SXERR_ABORT;` |
|       - | 10236 | `	}` |
|      32 | 10237 | `done:` |
|       - | 10238 | `	/* Point beyond the trait body */` |
|      69 | 10239 | `	pGen->pIn = &pEnd[1];` |
|      69 | 10240 | `	pGen->pEnd = pTmp;` |
|      69 | 10241 | `	return PH7_OK;` |
|      37 | 10242 | `}` |
|       - | 10243 | `/*` |
|       - | 10244 | ` * Compile a user-defined class.` |
|       - | 10245 | ` *  According to the PHP language reference manual` |
|       - | 10246 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 10247 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 10248 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 10249 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 10250 | ` *   and functions (called "methods").` |
|       - | 10251 | ` */` |
|  104536 | 10252 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       5 | 10253 | `{` |
|       - | 10254 | `	sxi32 rc;` |
|  104541 | 10255 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|  104541 | 10256 | `	return rc;` |
|       5 | 10257 | `}` |
|       - | 10258 | `/*` |
|       - | 10259 | ` * Exception handling.` |
|       - | 10260 | ` *  According to the PHP language reference manual` |
|       - | 10261 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 10262 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 10263 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 10264 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 10265 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 10266 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 10267 | ` *    (or re-thrown) within a catch block.` |
|       - | 10268 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 10269 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 10270 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 10271 | ` *    been defined with set_exception_handler().` |
|       - | 10272 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 10273 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 10274 | ` */` |
|       - | 10275 | `/*` |
|       - | 10276 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 10277 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 10278 | ` * indicates failure.` |
|       - | 10279 | ` */` |
|   15112 | 10280 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 | 10281 | `{` |
|   15117 | 10282 | `	sxi32 rc = SXRET_OK;` |
|   15117 | 10283 | `	if( pRoot->pOp ){` |
|   15107 | 10284 | `		switch( pRoot->pOp->iOp ){` |
|    7551 | 10285 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|       - | 10286 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|       - | 10287 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|       - | 10288 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|       - | 10289 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|       - | 10290 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|   15107 | 10291 | `			break;` |
|     ! 0 | 10292 | `		default:` |
|       - | 10293 | `			/* Runtime will still reject non-Throwable values; the set above` |
|       - | 10294 | `			 * covers the common shapes and gives a friendlier compile error` |
|       - | 10295 | ``			 * for obvious mistakes like `throw 5`. */`` |
|     ! 0 | 10296 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 10297 | `				"throw: Expecting an exception class instance");` |
|     ! 0 | 10298 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 | 10299 | `				rc = SXERR_INVALID;` |
|     ! 0 | 10300 | `			}` |
|     ! 0 | 10301 | `			break;` |
|       - | 10302 | `		}` |
|    7566 | 10303 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 10304 | `		/* Unexpected expression */` |
|     ! 0 | 10305 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 10306 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10307 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 10308 | `			rc = SXERR_INVALID;` |
|     ! 0 | 10309 | `		}` |
|     ! 0 | 10310 | `	}` |
|   15117 | 10311 | `	return rc;` |
|       5 | 10312 | `}` |
|       - | 10313 | `/*` |
|       - | 10314 | ` * Compile a 'throw' statement.` |
|       - | 10315 | ` * throw: This is how you trigger an exception.` |
|       - | 10316 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 10317 | ` */` |
|   15076 | 10318 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       5 | 10319 | `{` |
|   15081 | 10320 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10321 | `	GenBlock *pBlock;` |
|       - | 10322 | `	sxu32 nIdx;` |
|       - | 10323 | `	sxi32 rc;` |
|   15081 | 10324 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 10325 | `	/* Compile the expression */` |
|   15081 | 10326 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   15081 | 10327 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 10328 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 10329 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10330 | `			return SXERR_ABORT;` |
|       - | 10331 | `		}` |
|     ! 0 | 10332 | `		return SXRET_OK;` |
|       - | 10333 | `	}` |
|   15081 | 10334 | `	pBlock = pGen->pCurrent;` |
|       - | 10335 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   59677 | 10336 | `	while(pBlock->pParent){` |
|   59673 | 10337 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   15077 | 10338 | `			break;` |
|       - | 10339 | `		}` |
|       - | 10340 | `		/* Point to the parent block */` |
|   44601 | 10341 | `		pBlock = pBlock->pParent;` |
|       5 | 10342 | `	}` |
|       - | 10343 | `	/* Emit the throw instruction */` |
|   15081 | 10344 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 10345 | `	/* Emit the jump */` |
|   15081 | 10346 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   15081 | 10347 | `	return SXRET_OK;` |
|    7543 | 10348 | `}` |
|       - | 10349 | `/*` |
|       - | 10350 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - | 10351 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - | 10352 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - | 10353 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - | 10354 | ` * the validator guarantees the operand is a valid exception target.` |
|       - | 10355 | ` */` |
|      36 | 10356 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 | 10357 | `{` |
|      38 | 10358 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10359 | `	GenBlock *pBlock;` |
|       - | 10360 | `	sxu32 nIdx;` |
|       - | 10361 | `	sxi32 rc;` |
|      18 | 10362 | `	(void)iCompileFlag;` |
|      38 | 10363 | `	pGen->pIn++; /* Skip 'throw' */` |
|      38 | 10364 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 10365 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10366 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10367 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10368 | `			return SXERR_ABORT;` |
|       - | 10369 | `		}` |
|     ! 0 | 10370 | `		return SXRET_OK;` |
|       - | 10371 | `	}` |
|      38 | 10372 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      38 | 10373 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10374 | `		return SXERR_ABORT;` |
|       - | 10375 | `	}` |
|      38 | 10376 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 10377 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10378 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10379 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10380 | `			return SXERR_ABORT;` |
|       - | 10381 | `		}` |
|     ! 0 | 10382 | `		return SXRET_OK;` |
|       - | 10383 | `	}` |
|       - | 10384 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      38 | 10385 | `	pBlock = pGen->pCurrent;` |
|      60 | 10386 | `	while( pBlock->pParent ){` |
|      49 | 10387 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      27 | 10388 | `			break;` |
|       - | 10389 | `		}` |
|      23 | 10390 | `		pBlock = pBlock->pParent;` |
|       1 | 10391 | `	}` |
|      38 | 10392 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      38 | 10393 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      38 | 10394 | `	return SXRET_OK;` |
|      20 | 10395 | `}` |
|       - | 10396 | `/*` |
|       - | 10397 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|       - | 10398 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|       - | 10399 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|       - | 10400 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|       - | 10401 | ` * compile error propagated from the parser.` |
|       - | 10402 | ` */` |
|      40 | 10403 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|       4 | 10404 | `{` |
|       - | 10405 | `	SyString sClassName;` |
|       - | 10406 | `	SyToken *pToken;` |
|       - | 10407 | `	SyString *pName;` |
|       - | 10408 | `	char *zDup;` |
|       - | 10409 | `	sxi32 rc;` |
|      44 | 10410 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|      44 | 10411 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|      44 | 10412 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|      44 | 10413 | `	SySetInit(&pCatch->sByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      44 | 10414 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 10415 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 10416 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10417 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10418 | `		return SXERR_INVALID;` |
|       - | 10419 | `	}` |
|      44 | 10420 | `	pGen->pIn++; /* '(' */` |
|      20 | 10421 | `	for(;;){` |
|       - | 10422 | `		SyBlob sResolved;` |
|      44 | 10423 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      44 | 10424 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 | 10425 | `			SyBlobRelease(&sResolved);` |
|     ! 0 | 10426 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 10427 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10428 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10429 | `			return SXERR_INVALID;` |
|       - | 10430 | `		}` |
|      64 | 10431 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      40 | 10432 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|      44 | 10433 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|      44 | 10434 | `		SyBlobRelease(&sResolved);` |
|      44 | 10435 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|      44 | 10436 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|      44 | 10437 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      40 | 10438 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|       4 | 10439 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|     ! 0 | 10440 | `			pGen->pIn++; continue;` |
|       - | 10441 | `		}` |
|      44 | 10442 | `		break;` |
|     ! 0 | 10443 | `	}` |
|      40 | 10444 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|      44 | 10445 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 10446 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 10447 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10448 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10449 | `		return SXERR_INVALID;` |
|       - | 10450 | `	}` |
|      44 | 10451 | `	pGen->pIn++; /* '$' */` |
|      44 | 10452 | `	pName = &pGen->pIn->sData;` |
|      44 | 10453 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      44 | 10454 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|      44 | 10455 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|      44 | 10456 | `	pGen->pIn++;` |
|      44 | 10457 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 10458 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|     ! 0 | 10459 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10460 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10461 | `		return SXERR_INVALID;` |
|       - | 10462 | `	}` |
|      44 | 10463 | `	pGen->pIn++; /* ')' */` |
|      44 | 10464 | `	return SXRET_OK;` |
|      24 | 10465 | `}` |
|       - | 10466 | `/*` |
|       - | 10467 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|       - | 10468 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|       - | 10469 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|       - | 10470 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|       - | 10471 | ` * VmThrowException):` |
|       - | 10472 | ` *` |
|       - | 10473 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|       - | 10474 | ` *    <try body>` |
|       - | 10475 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|       - | 10476 | ` *    JMP  -> finally\|end` |
|       - | 10477 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|       - | 10478 | ` *    <catch body>` |
|       - | 10479 | ` *    JMP  -> finally\|end` |
|       - | 10480 | ` *    ... more catches ...` |
|       - | 10481 | ` *  Lfin: <finally body>` |
|       - | 10482 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|       - | 10483 | ` *  Lend:` |
|       - | 10484 | ` */` |
|      62 | 10485 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|       4 | 10486 | `{` |
|      66 | 10487 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10488 | `	GenBlock *pTry;` |
|       - | 10489 | `	VmInstr *pInstr;` |
|      66 | 10490 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|       - | 10491 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|       - | 10492 | `	sxi32 rc;` |
|      66 | 10493 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|       - | 10494 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION) */` |
|      66 | 10495 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      66 | 10496 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      66 | 10497 | `	pTry->pUserData = pException;` |
|      66 | 10498 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|      66 | 10499 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|      66 | 10500 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      66 | 10501 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      66 | 10502 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|      66 | 10503 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|       - | 10504 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|      66 | 10505 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|      66 | 10506 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|      66 | 10507 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      66 | 10508 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10509 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|      66 | 10510 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|       - | 10511 | `	/* Catch clauses (inline) */` |
|      66 | 10512 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      62 | 10513 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|      44 | 10514 | `		sxu32 k = 0;` |
|      60 | 10515 | `		for(;;){` |
|       - | 10516 | `			ph7_exception_block sCatch;` |
|       - | 10517 | `			GenBlock *pCatchBlk;` |
|      84 | 10518 | `			sxu32 idxJmp = 0;` |
|      80 | 10519 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      77 | 10520 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|      24 | 10521 | `				break;` |
|       - | 10522 | `			}` |
|      44 | 10523 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|      44 | 10524 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      44 | 10525 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|      44 | 10526 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|      44 | 10527 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|      44 | 10528 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatchBlk);` |
|      44 | 10529 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       - | 10530 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|       - | 10531 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|       - | 10532 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump). */` |
|      44 | 10533 | `			pCatchBlk->pUserData = pException;` |
|      44 | 10534 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      44 | 10535 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      44 | 10536 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|      44 | 10537 | `			GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10538 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|       - | 10539 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|      44 | 10540 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      44 | 10541 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|      44 | 10542 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|      44 | 10543 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|      44 | 10544 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      44 | 10545 | `			k++;` |
|       4 | 10546 | `		}` |
|      20 | 10547 | `	}` |
|       - | 10548 | `	/* Finally (inline) */` |
|      66 | 10549 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      48 | 10550 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 10551 | `		GenBlock *pFinBlk;` |
|      28 | 10552 | `		pGen->pIn++; /* Jump 'finally' */` |
|      28 | 10553 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|      28 | 10554 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|      28 | 10555 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      28 | 10556 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      28 | 10557 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      28 | 10558 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|      28 | 10559 | `		GenStateLeaveBlock(&(*pGen),0);` |
|      28 | 10560 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|      28 | 10561 | `		pException->iHasFinally = 1;` |
|      12 | 10562 | `	}` |
|      66 | 10563 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|      66 | 10564 | `	pException->iInlined = 1;` |
|       - | 10565 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|       - | 10566 | `	{` |
|      66 | 10567 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|       - | 10568 | `		sxu32 *aJ; sxu32 n;` |
|      66 | 10569 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|      66 | 10570 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      66 | 10571 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|     106 | 10572 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|      44 | 10573 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|      44 | 10574 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      24 | 10575 | `		}` |
|       - | 10576 | `	}` |
|      66 | 10577 | `	SySetRelease(&aCatchJmp);` |
|      66 | 10578 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|     ! 0 | 10579 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|     ! 0 | 10580 | `	}` |
|      66 | 10581 | `	return SXRET_OK;` |
|      35 | 10582 | `}` |
|       - | 10583 | `/*` |
|       - | 10584 | ` * Compile a 'catch' block.` |
|       - | 10585 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 10586 | ` * an object containing the exception information.` |
|       - | 10587 | ` */` |
|     610 | 10588 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       5 | 10589 | `{` |
|     615 | 10590 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10591 | `	ph7_exception_block sCatch;` |
|       - | 10592 | `	SySet *pInstrContainer;` |
|       - | 10593 | `	SyString sClassName;` |
|       - | 10594 | `	GenBlock *pCatch;` |
|       - | 10595 | `	SyToken *pToken;` |
|       - | 10596 | `	SyString *pName;` |
|       - | 10597 | `	char *zDup;` |
|       - | 10598 | `	sxi32 rc;` |
|     615 | 10599 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 10600 | `	/* Zero the structure */` |
|     615 | 10601 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 10602 | `	/* Initialize fields */` |
|     615 | 10603 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     615 | 10604 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     615 | 10605 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - | 10606 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 10607 | `			pToken = pGen->pIn;` |
|     ! 0 | 10608 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10609 | `				pToken--;` |
|     ! 0 | 10610 | `			}` |
|     ! 0 | 10611 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10612 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10613 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10614 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10615 | `				return SXERR_ABORT;` |
|       - | 10616 | `			}` |
|     ! 0 | 10617 | `			return SXERR_INVALID;` |
|       - | 10618 | `	}` |
|       - | 10619 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     615 | 10620 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     319 | 10621 | `	for(;;){` |
|       - | 10622 | `		SyBlob sResolved;` |
|     643 | 10623 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     643 | 10624 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       6 | 10625 | `			SyBlobRelease(&sResolved);` |
|       6 | 10626 | `			pToken = pGen->pIn;` |
|       6 | 10627 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10628 | `				pToken--;` |
|     ! 0 | 10629 | `			}` |
|       8 | 10630 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10631 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 | 10632 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       6 | 10633 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10634 | `				return SXERR_ABORT;` |
|       - | 10635 | `			}` |
|       6 | 10636 | `			return SXERR_INVALID;` |
|       - | 10637 | `		}` |
|       - | 10638 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|       - | 10639 | `		 * transient SyBlob allocation. */` |
|     956 | 10640 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     634 | 10641 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     639 | 10642 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     639 | 10643 | `		SyBlobRelease(&sResolved);` |
|     639 | 10644 | `		if( zDup == 0 ){` |
|     ! 0 | 10645 | `			goto Mem;` |
|       - | 10646 | `		}` |
|     639 | 10647 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     639 | 10648 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10649 | `			goto Mem;` |
|       - | 10650 | `		}` |
|       - | 10651 | `		/* Check for '\|' (multi-catch separator) */` |
|     634 | 10652 | `		if( pGen->pIn < pGen->pEnd &&` |
|     634 | 10653 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      33 | 10654 | `			pGen->pIn->sData.nByte == 1 &&` |
|      28 | 10655 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      30 | 10656 | `			pGen->pIn++; /* Consume the '\|' */` |
|      30 | 10657 | `			continue;` |
|       - | 10658 | `		}` |
|     611 | 10659 | `		break;` |
|     ! 0 | 10660 | `	}` |
|     606 | 10661 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     611 | 10662 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 10663 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 10664 | `			pToken = pGen->pIn;` |
|     ! 0 | 10665 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10666 | `				pToken--;` |
|     ! 0 | 10667 | `			}` |
|     ! 0 | 10668 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10669 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10670 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10671 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10672 | `				return SXERR_ABORT;` |
|       - | 10673 | `			}` |
|     ! 0 | 10674 | `			return SXERR_INVALID;` |
|       - | 10675 | `	}` |
|     611 | 10676 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 10677 | `	/* Duplicate instance name */` |
|     611 | 10678 | `	pName = &pGen->pIn->sData;` |
|     611 | 10679 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     611 | 10680 | `	if( zDup == 0 ){` |
|     ! 0 | 10681 | `		goto Mem;` |
|       - | 10682 | `	}` |
|     611 | 10683 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     611 | 10684 | `	pGen->pIn++;` |
|     611 | 10685 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 10686 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 10687 | `		pToken = pGen->pIn;` |
|     ! 0 | 10688 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10689 | `			pToken--;` |
|     ! 0 | 10690 | `		}` |
|     ! 0 | 10691 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10692 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10693 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10694 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10695 | `			return SXERR_ABORT;` |
|       - | 10696 | `		}` |
|     ! 0 | 10697 | `		return SXERR_INVALID;` |
|       - | 10698 | `	}` |
|       - | 10699 | `	/* Compile the block */` |
|     611 | 10700 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 10701 | `	/* Create the catch block */` |
|     611 | 10702 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     611 | 10703 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10704 | `		return SXERR_ABORT;` |
|       - | 10705 | `	}` |
|       - | 10706 | `	/* Swap bytecode container */` |
|     611 | 10707 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     611 | 10708 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 10709 | `	/* Compile the block */` |
|     611 | 10710 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 10711 | `	/* Fix forward jumps now the destination is resolved  */` |
|     611 | 10712 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10713 | `	/* Emit the DONE instruction */` |
|     611 | 10714 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 10715 | `	/* Leave the block */` |
|     611 | 10716 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10717 | `	/* Restore the default container */` |
|     611 | 10718 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 10719 | `	/* Install the catch block */` |
|     611 | 10720 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     611 | 10721 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10722 | `		goto Mem;` |
|       - | 10723 | `	}` |
|     611 | 10724 | `	return SXRET_OK;` |
|     ! 0 | 10725 | `Mem:` |
|     ! 0 | 10726 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 10727 | `	return SXERR_ABORT;` |
|     310 | 10728 | `}` |
|       - | 10729 | `/*` |
|       - | 10730 | ` * Compile a 'try' block.` |
|       - | 10731 | ` * A function using an exception should be in a "try" block.` |
|       - | 10732 | ` * If the exception does not trigger, the code will continue` |
|       - | 10733 | ` * as normal. However if the exception triggers, an exception` |
|       - | 10734 | ` * is "thrown".` |
|       - | 10735 | ` */` |
|     716 | 10736 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       5 | 10737 | `{` |
|       - | 10738 | `	ph7_exception *pException;` |
|     721 | 10739 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10740 | `	GenBlock *pTry;` |
|       - | 10741 | `	sxu32 nJmpIdx;` |
|       - | 10742 | `	sxi32 rc;` |
|       - | 10743 | `	/* Create the exception container */` |
|     721 | 10744 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     721 | 10745 | `	if( pException == 0 ){` |
|     ! 0 | 10746 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 10747 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 10748 | `		return SXERR_ABORT;` |
|       - | 10749 | `	}` |
|       - | 10750 | `	/* Zero the structure */` |
|     721 | 10751 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 10752 | `	/* Initialize fields */` |
|     721 | 10753 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     721 | 10754 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     721 | 10755 | `	pException->iHasFinally = 0;` |
|     721 | 10756 | `	pException->iFinallyDone = 0;` |
|     721 | 10757 | `	pException->pVm = pGen->pVm;` |
|       - | 10758 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|       - | 10759 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the legacy path.`` |
|       - | 10760 | `	 * DORMANT until the inline VM handlers (OP_CATCH / OP_END_FINALLY dispatch,` |
|       - | 10761 | `	 * VmThrowException pc-redirect, return/break-through-finally threading, generator` |
|       - | 10762 | `	 * park of aFinallyAction) land — the compiler emits the layout but the VM cannot yet` |
|       - | 10763 | `	 * execute it. Guarded by pVm->bInlineTryCatch (default 0) so the tree stays green. */` |
|     721 | 10764 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|      66 | 10765 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|       - | 10766 | `	}` |
|       - | 10767 | `	/* Create the try block */` |
|     659 | 10768 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     659 | 10769 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10770 | `		return SXERR_ABORT;` |
|       - | 10771 | `	}` |
|       - | 10772 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     659 | 10773 | `	pTry->pUserData = pException;` |
|       - | 10774 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     659 | 10775 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 10776 | `	/* Fix the jump later when the destination is resolved */` |
|     659 | 10777 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     659 | 10778 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 10779 | `	/* Compile the block */` |
|     659 | 10780 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     659 | 10781 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10782 | `		return SXERR_ABORT;` |
|       - | 10783 | `	}` |
|       - | 10784 | `	/* Fix forward jumps now the destination is resolved */` |
|     659 | 10785 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10786 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     659 | 10787 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 10788 | `	/* Leave the block */` |
|     659 | 10789 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10790 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     659 | 10791 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     652 | 10792 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 10793 | `		/* Compile one or more catch blocks */` |
|     606 | 10794 | `		for(;;){` |
|    1212 | 10795 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     985 | 10796 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     306 | 10797 | `					break;` |
|       - | 10798 | `			}` |
|     615 | 10799 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     615 | 10800 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10801 | `				return SXERR_ABORT;` |
|       - | 10802 | `			}` |
|       5 | 10803 | `		}` |
|     301 | 10804 | `	}` |
|       - | 10805 | `	/* Compile optional finally block */` |
|     659 | 10806 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     366 | 10807 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 10808 | `		SySet *pInstrContainer;` |
|       - | 10809 | `		GenBlock *pFinBlock;` |
|     113 | 10810 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 10811 | `		/* Create the finally block for jump fixup bookkeeping */` |
|     113 | 10812 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|     113 | 10813 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10814 | `			return SXERR_ABORT;` |
|       - | 10815 | `		}` |
|       - | 10816 | `		/* Swap bytecode container */` |
|     113 | 10817 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     113 | 10818 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 10819 | `		/* Compile the finally body */` |
|     113 | 10820 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     113 | 10821 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10822 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 10823 | `			return SXERR_ABORT;` |
|       - | 10824 | `		}` |
|       - | 10825 | `		/* Fix forward jumps now the destination is resolved */` |
|     113 | 10826 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10827 | `		/* Emit DONE to terminate the finally block */` |
|     113 | 10828 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 10829 | `		/* Leave the block */` |
|     113 | 10830 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10831 | `		/* Restore the default container */` |
|     113 | 10832 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     113 | 10833 | `		pException->iHasFinally = 1;` |
|      54 | 10834 | `	}` |
|       - | 10835 | `	/* Must have at least one catch or finally */` |
|     659 | 10836 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       9 | 10837 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10838 | `			"Cannot use try without catch or finally");` |
|       9 | 10839 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10840 | `			return SXERR_ABORT;` |
|       - | 10841 | `		}` |
|       3 | 10842 | `	}` |
|     659 | 10843 | `	return SXRET_OK;` |
|     363 | 10844 | `}` |
|       - | 10845 | `/*` |
|       - | 10846 | ` * Compile a switch block.` |
|       - | 10847 | ` *  (See block-comment below for more information)` |
|       - | 10848 | ` */` |
|     112 | 10849 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       5 | 10850 | `{` |
|     117 | 10851 | `	sxi32 rc = SXRET_OK;` |
|     117 | 10852 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 10853 | `		/* Unexpected token */` |
|     ! 0 | 10854 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 10855 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10856 | `			return SXERR_ABORT;` |
|       - | 10857 | `		}` |
|     ! 0 | 10858 | `		pGen->pIn++;` |
|     ! 0 | 10859 | `	}` |
|     117 | 10860 | `	pGen->pIn++;` |
|       - | 10861 | `	/* First instruction to execute in this block. */` |
|     117 | 10862 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 10863 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 10864 | `	 * or the '}' token */` |
|     206 | 10865 | `	for(;;){` |
|     417 | 10866 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10867 | `			/* No more input to process */` |
|     ! 0 | 10868 | `			break;` |
|       - | 10869 | `		}` |
|     417 | 10870 | `		rc = SXRET_OK;` |
|     417 | 10871 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      85 | 10872 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      31 | 10873 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 10874 | `					/* Unexpected token */` |
|     ! 0 | 10875 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 10876 | `						&pGen->pIn->sData);` |
|     ! 0 | 10877 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10878 | `						return SXERR_ABORT;` |
|       - | 10879 | `					}` |
|       - | 10880 | `					/* FALL THROUGH */` |
|     ! 0 | 10881 | `				}` |
|      31 | 10882 | `				rc = SXERR_EOF;` |
|      31 | 10883 | `				break;` |
|       - | 10884 | `			}` |
|      32 | 10885 | `		}else{` |
|       - | 10886 | `			sxi32 nKwrd;` |
|       - | 10887 | `			/* Extract the keyword */` |
|     337 | 10888 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     337 | 10889 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      47 | 10890 | `				break;` |
|       - | 10891 | `			}` |
|     253 | 10892 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 10893 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 10894 | `					/* Unexpected token */` |
|     ! 0 | 10895 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 10896 | `						&pGen->pIn->sData);` |
|     ! 0 | 10897 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10898 | `						return SXERR_ABORT;` |
|       - | 10899 | `					}` |
|       - | 10900 | `					/* FALL THROUGH */` |
|     ! 0 | 10901 | `				}` |
|       - | 10902 | `				/* Block compiled */` |
|       3 | 10903 | `				break;` |
|       - | 10904 | `			}` |
|       - | 10905 | `		}` |
|       - | 10906 | `		/* Compile block */` |
|     305 | 10907 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     305 | 10908 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10909 | `			return SXERR_ABORT;` |
|       - | 10910 | `		}` |
|       5 | 10911 | `	}` |
|     117 | 10912 | `	return rc;` |
|      61 | 10913 | `}` |
|       - | 10914 | `/*` |
|       - | 10915 | ` * Compile a case eXpression.` |
|       - | 10916 | ` *  (See block-comment below for more information)` |
|       - | 10917 | ` */` |
|      92 | 10918 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       5 | 10919 | `{` |
|       - | 10920 | `	SySet *pInstrContainer;` |
|       - | 10921 | `	SyToken *pEnd,*pTmp;` |
|      97 | 10922 | `	sxi32 iNest = 0;` |
|       - | 10923 | `	sxi32 rc;` |
|       - | 10924 | `	/* Delimit the expression */` |
|      97 | 10925 | `	pEnd = pGen->pIn;` |
|     197 | 10926 | `	while( pEnd < pGen->pEnd ){` |
|     197 | 10927 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 10928 | `			/* Increment nesting level */` |
|       3 | 10929 | `			iNest++;` |
|     196 | 10930 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 10931 | `			/* Decrement nesting level */` |
|       3 | 10932 | `			iNest--;` |
|     194 | 10933 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      97 | 10934 | `			break;` |
|       - | 10935 | `		}` |
|     105 | 10936 | `		pEnd++;` |
|       5 | 10937 | `	}` |
|      97 | 10938 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 10939 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 10940 | `		if( rc == SXERR_ABORT ){` |
|       - | 10941 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10942 | `			return SXERR_ABORT;` |
|       - | 10943 | `		}` |
|     ! 0 | 10944 | `	}` |
|       - | 10945 | `	/* Swap token stream */` |
|      97 | 10946 | `	pTmp = pGen->pEnd;` |
|      97 | 10947 | `	pGen->pEnd = pEnd;` |
|      97 | 10948 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      97 | 10949 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      97 | 10950 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 10951 | `	/* Emit the done instruction */` |
|      97 | 10952 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      97 | 10953 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 10954 | `	/* Update token stream */` |
|      97 | 10955 | `	pGen->pIn  = pEnd;` |
|      97 | 10956 | `	pGen->pEnd = pTmp;` |
|      97 | 10957 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10958 | `		return SXERR_ABORT;` |
|       - | 10959 | `	}` |
|      97 | 10960 | `	return SXRET_OK;` |
|      51 | 10961 | `}` |
|       - | 10962 | `/*` |
|       - | 10963 | ` * Compile the smart switch statement.` |
|       - | 10964 | ` * According to the PHP language reference manual` |
|       - | 10965 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 10966 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 10967 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 10968 | ` *  This is exactly what the switch statement is for.` |
|       - | 10969 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 10970 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 10971 | ` *  of the outer loop, use continue 2.` |
|       - | 10972 | ` *  Note that switch/case does loose comparision.` |
|       - | 10973 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 10974 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 10975 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 10976 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 10977 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 10978 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 10979 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 10980 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 10981 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 10982 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 10983 | ` *  list for the next case.` |
|       - | 10984 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 10985 | ` *  or floating-point numbers and strings.` |
|       - | 10986 | ` */` |
|      28 | 10987 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       5 | 10988 | `{` |
|       - | 10989 | `	GenBlock *pSwitchBlock;` |
|       - | 10990 | `	SyToken *pTmp,*pEnd;` |
|       - | 10991 | `	ph7_switch *pSwitch;` |
|       - | 10992 | `	sxu32 nToken;` |
|       - | 10993 | `	sxu32 nLine;` |
|       - | 10994 | `	sxi32 rc;` |
|      33 | 10995 | `	nLine = pGen->pIn->nLine;` |
|       - | 10996 | `	/* Jump the 'switch' keyword */` |
|      33 | 10997 | `	pGen->pIn++;` |
|      33 | 10998 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 10999 | `		/* Syntax error */` |
|     ! 0 | 11000 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 11001 | `		if( rc == SXERR_ABORT ){` |
|       - | 11002 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 11003 | `			return SXERR_ABORT;` |
|       - | 11004 | `		}` |
|     ! 0 | 11005 | `		goto Synchronize;` |
|       - | 11006 | `	}` |
|       - | 11007 | `	/* Jump the left parenthesis '(' */` |
|      33 | 11008 | `	pGen->pIn++;` |
|      33 | 11009 | `	pEnd = 0; /* cc warning */` |
|       - | 11010 | `	/* Create the loop block */` |
|      47 | 11011 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 | 11012 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      33 | 11013 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 11014 | `		return SXERR_ABORT;` |
|       - | 11015 | `	}` |
|       - | 11016 | `	/* Delimit the condition */` |
|      33 | 11017 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      33 | 11018 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 11019 | `		/* Empty expression */` |
|     ! 0 | 11020 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 11021 | `		if( rc == SXERR_ABORT ){` |
|       - | 11022 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 11023 | `			return SXERR_ABORT;` |
|       - | 11024 | `		}` |
|     ! 0 | 11025 | `	}` |
|       - | 11026 | `	/* Swap token streams */` |
|      33 | 11027 | `	pTmp = pGen->pEnd;` |
|      33 | 11028 | `	pGen->pEnd = pEnd;` |
|       - | 11029 | `	/* Compile the expression */` |
|      33 | 11030 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      33 | 11031 | `	if( rc == SXERR_ABORT ){` |
|       - | 11032 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 11033 | `		return SXERR_ABORT;` |
|       - | 11034 | `	}` |
|       - | 11035 | `	/* Update token stream */` |
|      33 | 11036 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 11037 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 11038 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 11039 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 11040 | `			return SXERR_ABORT;` |
|       - | 11041 | `		}` |
|     ! 0 | 11042 | `		pGen->pIn++;` |
|     ! 0 | 11043 | `	}` |
|      33 | 11044 | `	pGen->pIn  = &pEnd[1];` |
|      33 | 11045 | `	pGen->pEnd = pTmp;` |
|      33 | 11046 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 | 11047 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 11048 | `			pTmp = pGen->pIn;` |
|     ! 0 | 11049 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 11050 | `				pTmp--;` |
|     ! 0 | 11051 | `			}` |
|       - | 11052 | `			/* Unexpected token */` |
|     ! 0 | 11053 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 11054 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11055 | `				return SXERR_ABORT;` |
|       - | 11056 | `			}` |
|     ! 0 | 11057 | `			goto Synchronize;` |
|       - | 11058 | `	}` |
|       - | 11059 | `	/* Set the delimiter token */` |
|      33 | 11060 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 11061 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 11062 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 11063 | `	}else{` |
|      31 | 11064 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 11065 | `	}` |
|      33 | 11066 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 11067 | `	/* Create the switch blocks container */` |
|      33 | 11068 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      33 | 11069 | `	if( pSwitch == 0 ){` |
|       - | 11070 | `		/* Abort compilation */` |
|     ! 0 | 11071 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 11072 | `		return SXERR_ABORT;` |
|       - | 11073 | `	}` |
|       - | 11074 | `	/* Zero the structure */` |
|      33 | 11075 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 11076 | `	/* Initialize fields */` |
|      33 | 11077 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 11078 | `	/* Emit the switch instruction */` |
|      33 | 11079 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 11080 | `	/* Compile case blocks */` |
|     100 | 11081 | `	for(;;){` |
|       - | 11082 | `		sxu32 nKwrd;` |
|     119 | 11083 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 11084 | `			/* No more input to process */` |
|     ! 0 | 11085 | `			break;` |
|       - | 11086 | `		}` |
|     119 | 11087 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 11088 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 11089 | `				/* Unexpected token */` |
|     ! 0 | 11090 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 11091 | `					&pGen->pIn->sData);` |
|     ! 0 | 11092 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 11093 | `					return SXERR_ABORT;` |
|       - | 11094 | `				}` |
|       - | 11095 | `				/* FALL THROUGH */` |
|     ! 0 | 11096 | `			}` |
|       - | 11097 | `			/* Block compiled */` |
|     ! 0 | 11098 | `			break;` |
|       - | 11099 | `		}` |
|       - | 11100 | `		/* Extract the keyword */` |
|     119 | 11101 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     119 | 11102 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 11103 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 11104 | `				/* Unexpected token */` |
|     ! 0 | 11105 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 11106 | `					&pGen->pIn->sData);` |
|     ! 0 | 11107 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 11108 | `					return SXERR_ABORT;` |
|       - | 11109 | `				}` |
|       - | 11110 | `				/* FALL THROUGH */` |
|     ! 0 | 11111 | `			}` |
|       - | 11112 | `			/* Block compiled */` |
|       3 | 11113 | `			break;` |
|       - | 11114 | `		}` |
|     117 | 11115 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 11116 | `			/*` |
|       - | 11117 | `			 * Accroding to the PHP language reference manual` |
|       - | 11118 | `			 *  A special case is the default case. This case matches anything` |
|       - | 11119 | `			 *  that wasn't matched by the other cases.` |
|       - | 11120 | `			 */` |
|      25 | 11121 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 11122 | `				/* Default case already compiled */` |
|     ! 0 | 11123 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 11124 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 11125 | `					return SXERR_ABORT;` |
|       - | 11126 | `				}` |
|     ! 0 | 11127 | `			}` |
|      25 | 11128 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 11129 | `			/* Compile the default block */` |
|      25 | 11130 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      25 | 11131 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 11132 | `				return SXERR_ABORT;` |
|      25 | 11133 | `			}else if( rc == SXERR_EOF ){` |
|      23 | 11134 | `				break;` |
|       1 | 11135 | `			}` |
|      98 | 11136 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 11137 | `			ph7_case_expr sCase;` |
|       - | 11138 | `			/* Standard case block */` |
|      97 | 11139 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 11140 | `			/* initialize the structure */` |
|      97 | 11141 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 11142 | `			/* Compile the case expression */` |
|      97 | 11143 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      97 | 11144 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11145 | `				return SXERR_ABORT;` |
|       - | 11146 | `			}` |
|       - | 11147 | `			/* Compile the case block */` |
|      97 | 11148 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 11149 | `			/* Insert in the switch container */` |
|      97 | 11150 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      97 | 11151 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 11152 | `				return SXERR_ABORT;` |
|      97 | 11153 | `			}else if( rc == SXERR_EOF ){` |
|       9 | 11154 | `				break;` |
|       - | 11155 | `			}` |
|      47 | 11156 | `		}else{` |
|       - | 11157 | `			/* Unexpected token */` |
|     ! 0 | 11158 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 11159 | `				&pGen->pIn->sData);` |
|     ! 0 | 11160 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11161 | `				return SXERR_ABORT;` |
|       - | 11162 | `			}` |
|     ! 0 | 11163 | `			break;` |
|       - | 11164 | `		}` |
|       5 | 11165 | `	}` |
|       - | 11166 | `	/* Fix all jumps now the destination is resolved */` |
|      33 | 11167 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      33 | 11168 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 11169 | `	/* Release the loop block */` |
|      33 | 11170 | `	GenStateLeaveBlock(pGen,0);` |
|      33 | 11171 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 11172 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      33 | 11173 | `		pGen->pIn++;` |
|      14 | 11174 | `	}` |
|       - | 11175 | `	/* Statement successfully compiled */` |
|      33 | 11176 | `	return SXRET_OK;` |
|     ! 0 | 11177 | `Synchronize:` |
|       - | 11178 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 11179 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 11180 | `		pGen->pIn++;` |
|     ! 0 | 11181 | `	}` |
|     ! 0 | 11182 | `	return SXRET_OK;` |
|      19 | 11183 | `}` |
|       - | 11184 | `/*` |
|       - | 11185 | ` * Chain operators participate in a postfix member-access chain.` |
|       - | 11186 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|       - | 11187 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|       - | 11188 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|       - | 11189 | ` */` |
|       - | 11190 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|       - | 11191 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|       - | 11192 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|       - | 11193 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|       - | 11194 |  |
|       - | 11195 | `/*` |
|       - | 11196 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|       - | 11197 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|       - | 11198 | ` * patched entries from the pending set.` |
|       - | 11199 | ` */` |
| 2740794 | 11200 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|       5 | 11201 | `{` |
| 2740799 | 11202 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|       - | 11203 | `	sxu32 nTarget;` |
|       - | 11204 | `	sxu32 *aIdx;` |
|       - | 11205 | `	sxu32 i;` |
| 2740799 | 11206 | `	if( nCur <= nBaseline ){` |
| 2740705 | 11207 | `		return;` |
|       - | 11208 | `	}` |
|      98 | 11209 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      98 | 11210 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|     200 | 11211 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|     106 | 11212 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|     106 | 11213 | `		if( pInstr ){` |
|     106 | 11214 | `			pInstr->iP2 = (sxi32)nTarget;` |
|      51 | 11215 | `		}` |
|      55 | 11216 | `	}` |
|      98 | 11217 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 1370402 | 11218 | `}` |
|       - | 11219 |  |
|       - | 11220 | `/*` |
|       - | 11221 | ` * By-reference out-parameters of builtin functions.` |
|       - | 11222 | ` *` |
|       - | 11223 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|       - | 11224 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|       - | 11225 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|       - | 11226 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|       - | 11227 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|       - | 11228 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|       - | 11229 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|       - | 11230 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|       - | 11231 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|       - | 11232 | ` * creates it" behaviour).` |
|       - | 11233 | ` *` |
|       - | 11234 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|       - | 11235 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|       - | 11236 | ` */` |
|  460928 | 11237 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|       5 | 11238 | `{` |
|       - | 11239 | `	static const struct {` |
|       - | 11240 | `		const char *zName;` |
|       - | 11241 | `		sxu32 nByte;` |
|       - | 11242 | `		sxu32 mask;` |
|       - | 11243 | `	} aByRef[] = {` |
|       - | 11244 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 11245 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 11246 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 11247 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 11248 | `	};` |
|       - | 11249 | `	sxu32 i;` |
|  460933 | 11250 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|    1771 | 11251 | `		return 0;` |
|       - | 11252 | `	}` |
| 2295543 | 11253 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 1836470 | 11254 | `		if( pName->nByte == aByRef[i].nByte` |
|  941436 | 11255 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|      99 | 11256 | `			return aByRef[i].mask;` |
|       - | 11257 | `		}` |
|  918193 | 11258 | `	}` |
|  459073 | 11259 | `	return 0;` |
|  230469 | 11260 | `}` |
|       - | 11261 | `/*` |
|       - | 11262 | ` * Recover the bare global-builtin name from a call's callee node.` |
|       - | 11263 | ` *` |
|       - | 11264 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|       - | 11265 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|       - | 11266 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|       - | 11267 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|       - | 11268 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|       - | 11269 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|       - | 11270 | ` */` |
|  460928 | 11271 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|       5 | 11272 | `{` |
|       - | 11273 | `	SyToken *p, *pEnd;` |
|  460933 | 11274 | `	pOut->zString = 0;` |
|  460933 | 11275 | `	pOut->nByte = 0;` |
|  460933 | 11276 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|     ! 0 | 11277 | `		return;` |
|       - | 11278 | `	}` |
|  460933 | 11279 | `	p = pLeft->pStart;` |
|  460933 | 11280 | `	pEnd = pLeft->pEnd;` |
|       - | 11281 | `	/* Optional single leading namespace separator (absolute path). */` |
|  460933 | 11282 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|    3723 | 11283 | `		p++;` |
|    1859 | 11284 | `	}` |
|  460933 | 11285 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|    1737 | 11286 | `		return;` |
|       - | 11287 | `	}` |
|       - | 11288 | `	/* Must be a single component: nothing follows the name token. */` |
|  459201 | 11289 | `	if( p + 1 != pEnd ){` |
|      39 | 11290 | `		return;` |
|       - | 11291 | `	}` |
|  459167 | 11292 | `	*pOut = p->sData;` |
|  230469 | 11293 | `}` |
|       - | 11294 | `/*` |
|       - | 11295 | ` * Generate bytecode for a given expression tree.` |
|       - | 11296 | ` * If something goes wrong while generating bytecode` |
|       - | 11297 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 11298 | ` * this function takes care of generating the appropriate` |
|       - | 11299 | ` * error message.` |
|       - | 11300 | ` */` |
| 3668484 | 11301 | `static sxi32 GenStateEmitExprCode(` |
|       - | 11302 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 11303 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 11304 | `	sxi32 iFlags /* Control flags */` |
|       - | 11305 | `	)` |
|       5 | 11306 | `{` |
|       - | 11307 | `	VmInstr *pInstr;` |
|       - | 11308 | `	sxu32 nJmpIdx;` |
| 3668489 | 11309 | `	sxi32 iP1 = 0;` |
| 3668489 | 11310 | `	sxu32 iP2 = 0;` |
| 3668489 | 11311 | `	void *p3  = 0;` |
|       - | 11312 | `	sxi32 iVmOp;` |
|       - | 11313 | `	sxi32 rc;` |
| 3668489 | 11314 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 3668489 | 11315 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
| 3668489 | 11316 | `	sxu32 nRhsNsBase = 0;` |
| 3668489 | 11317 | `	if( pNode->xCode ){` |
|       - | 11318 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 11319 | `		/* Compile node */` |
| 2290039 | 11320 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 2290039 | 11321 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 2290039 | 11322 | `		RE_SWAP_DELIMITER(pGen);` |
| 2290039 | 11323 | `		return rc;` |
|       - | 11324 | `	}` |
| 1378455 | 11325 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 11326 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 11327 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 11328 | `		return SXERR_ABORT;` |
|       - | 11329 | `	}` |
| 1378455 | 11330 | `	iVmOp = pNode->pOp->iVmOp;` |
| 1378455 | 11331 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|       - | 11332 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|       - | 11333 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|       - | 11334 | `		 * and later errors are still reported. */` |
|       3 | 11335 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 11336 | `			"The (unset) cast is no longer supported");` |
|       3 | 11337 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 11338 | `			return SXERR_ABORT;` |
|       - | 11339 | `		}` |
|       1 | 11340 | `	}` |
| 1378455 | 11341 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      65 | 11342 | `		sxu32 nJmp = 0;` |
|       - | 11343 | `		sxu32 nNcNsBase;` |
|       - | 11344 | `		VmInstr *pInstrFix;` |
|       - | 11345 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - | 11346 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - | 11347 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - | 11348 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - | 11349 | `		 * stack slot carries a writable nIdx. */` |
|      65 | 11350 | `		if( pNode->pRight ){` |
|      65 | 11351 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      65 | 11352 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|      65 | 11353 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11354 | `				return rc;` |
|       - | 11355 | `			}` |
|      65 | 11356 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       - | 11357 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - | 11358 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - | 11359 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - | 11360 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - | 11361 | `			 * the store, so the parent array does not need to be copied at` |
|       - | 11362 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - | 11363 | `			 * cascade for the actual write path stays correct. */` |
|      65 | 11364 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      65 | 11365 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      31 | 11366 | `				pInstrFix->iP2 = 3;` |
|      14 | 11367 | `			}` |
|      31 | 11368 | `		}` |
|       - | 11369 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      65 | 11370 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - | 11371 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      65 | 11372 | `		if( pNode->pLeft ){` |
|      65 | 11373 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      65 | 11374 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      65 | 11375 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11376 | `				return rc;` |
|       - | 11377 | `			}` |
|      65 | 11378 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|      31 | 11379 | `		}` |
|       - | 11380 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      65 | 11381 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - | 11382 | `		/* Patch the short-circuit jump to land after the store. */` |
|      65 | 11383 | `		if( nJmp > 0 ){` |
|      65 | 11384 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      65 | 11385 | `			if( pInstrFix ){` |
|      65 | 11386 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      31 | 11387 | `			}` |
|      31 | 11388 | `		}` |
|      65 | 11389 | `		return SXRET_OK;` |
|       - | 11390 | `	}` |
| 1378393 | 11391 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 11392 | `		sxu32 nJz,nJmp;` |
|       - | 11393 | `		sxu32 nTernaryNsBase;` |
|       - | 11394 | `		/* Ternary operator require special handling */` |
|       - | 11395 | `		/* Phase#1: Compile the condition */` |
|    2647 | 11396 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2647 | 11397 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    2647 | 11398 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 11399 | `			return rc;` |
|       - | 11400 | `		}` |
|       - | 11401 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|       - | 11402 | `		 * compiling the condition must short-circuit to the end of the` |
|       - | 11403 | `		 * condition expression, not leak past the ternary. */` |
|    2647 | 11404 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    2647 | 11405 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    2647 | 11406 | `		if( pNode->pLeft ){` |
|       - | 11407 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 11408 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    2579 | 11409 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 11410 | `			/* Phase#3: Compile the 'then' expression  */` |
|    2579 | 11411 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2579 | 11412 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    2579 | 11413 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11414 | `				return rc;` |
|       - | 11415 | `			}` |
|    2579 | 11416 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1292 | 11417 | `		}else{` |
|       - | 11418 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 11419 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 11420 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 11421 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 11422 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 11423 | `		}` |
|       - | 11424 | `		/* Phase#4: Emit the unconditional jump */` |
|    2647 | 11425 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 11426 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    2647 | 11427 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    2647 | 11428 | `		if( pInstr ){` |
|    2647 | 11429 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1321 | 11430 | `		}` |
|    2647 | 11431 | `		if( !pNode->pLeft ){` |
|       - | 11432 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 11433 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 11434 | `		}` |
|       - | 11435 | `		/* Phase#6: Compile the 'else' expression */` |
|    2647 | 11436 | `		if( pNode->pRight ){` |
|    2647 | 11437 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2647 | 11438 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    2647 | 11439 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 11440 | `				return rc;` |
|       - | 11441 | `			}` |
|    2647 | 11442 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1321 | 11443 | `		}` |
|    2647 | 11444 | `		if( nJmp > 0 ){` |
|       - | 11445 | `			/* Phase#7: Fix the unconditional jump */` |
|    2647 | 11446 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    2647 | 11447 | `			if( pInstr ){` |
|    2647 | 11448 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1321 | 11449 | `			}` |
|    1321 | 11450 | `		}` |
|       - | 11451 | `		/* All done */` |
|    2647 | 11452 | `		return SXRET_OK;` |
|       - | 11453 | `	}` |
| 1375751 | 11454 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|       - | 11455 | `	/* Generate code for the left tree */` |
| 1375751 | 11456 | `	if( pNode->pLeft ){` |
| 1375711 | 11457 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 1375711 | 11458 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 11459 | `			ph7_expr_node **apNode;` |
|  464765 | 11460 | `			int hasSpread = 0;` |
|  464765 | 11461 | `			int hasNamed = 0;` |
|  464765 | 11462 | `			int bAnySpread = 0;` |
|  464765 | 11463 | `			sxu32 byRefMask = 0;` |
|       - | 11464 | `			sxi32 nArgs;` |
|       - | 11465 | `			sxi32 n;` |
|       - | 11466 | `			/* Recurse and generate bytecodes for function arguments */` |
|  464765 | 11467 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  464765 | 11468 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 11469 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|       - | 11470 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|       - | 11471 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|  464765 | 11472 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|      65 | 11473 | `				bFcc = 1;` |
|      65 | 11474 | `				nArgs = 0;` |
|      32 | 11475 | `			}` |
|       - | 11476 | `			/* Validate: no positional arguments after named arguments */` |
|       - | 11477 | `			{` |
|  464765 | 11478 | `				int seenNamed = 0;` |
|  943101 | 11479 | `				for( n = 0; n < nArgs; ++n ){` |
|  478343 | 11480 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     216 | 11481 | `						seenNamed = 1;` |
|     216 | 11482 | `						hasNamed = 1;` |
|  478237 | 11483 | `					}else if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|    3719 | 11484 | `						bAnySpread = 1;` |
|  476274 | 11485 | `					}else if( seenNamed ){` |
|       3 | 11486 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|       - | 11487 | `							"Cannot use positional argument after named argument");` |
|       3 | 11488 | `						return SXERR_SYNTAX;` |
|       - | 11489 | `					}` |
|  239173 | 11490 | `				}` |
|       - | 11491 | `			}` |
|       - | 11492 | `			/* Read-only load */` |
|  464763 | 11493 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|       - | 11494 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|       - | 11495 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|       - | 11496 | `			 * objects dispatch to the right method (offsetExists for both;` |
|       - | 11497 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|  464763 | 11498 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|  464763 | 11499 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  464758 | 11500 | `				if( pCallName->nByte == 5` |
|  253719 | 11501 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|   22471 | 11502 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|  453530 | 11503 | `				}else if( pCallName->nByte == 5` |
|  231253 | 11504 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|      91 | 11505 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|      43 | 11506 | `				}` |
|       - | 11507 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|       - | 11508 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|       - | 11509 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|       - | 11510 | `				 * write back through. Skipped when spread/named args are present:` |
|       - | 11511 | `				 * the compile-time positional index no longer maps to the` |
|       - | 11512 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|  464763 | 11513 | `				if( !bAnySpread && !hasNamed ){` |
|       - | 11514 | `					SyString sBuiltin;` |
|  460933 | 11515 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|  460933 | 11516 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|  230464 | 11517 | `				}` |
|  232379 | 11518 | `			}` |
|  943097 | 11519 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  478339 | 11520 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  478339 | 11521 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|       - | 11522 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|       - | 11523 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|       - | 11524 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|       - | 11525 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|       - | 11526 | `				 * builtin to write back through. A plain $var target is unaffected` |
|       - | 11527 | `				 * (iP1=0 either way). See PLAN.md §2 for the full rationale. */` |
|  478339 | 11528 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|      55 | 11529 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|      55 | 11530 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|      25 | 11531 | `				}` |
|  478339 | 11532 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  478339 | 11533 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 11534 | `					return rc;` |
|       - | 11535 | `				}` |
|       - | 11536 | `				/* Each argument is an independent nullsafe scope. */` |
|  478339 | 11537 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  478339 | 11538 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 11539 | `					/* Emit spread opcode to unpack this array argument */` |
|    3719 | 11540 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|    3719 | 11541 | `					hasSpread = 1;` |
|    1857 | 11542 | `				}` |
|  239172 | 11543 | `			}` |
|       - | 11544 | `			/* Total number of given arguments */` |
|  464763 | 11545 | `			iP1 = nArgs;` |
|  464763 | 11546 | `			iP2 = hasSpread;` |
|       - | 11547 | `			/* Build VmCallArgMap if named arguments are present.` |
|       - | 11548 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  464763 | 11549 | `			if( hasNamed ){` |
|     119 | 11550 | `				sxu32 nStrBytes = 0;` |
|       - | 11551 | `				char *zBuf;` |
|     347 | 11552 | `				for( n = 0; n < nArgs; ++n ){` |
|     231 | 11553 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     213 | 11554 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|     105 | 11555 | `					}` |
|     117 | 11556 | `				}` |
|       - | 11557 | `				{` |
|     119 | 11558 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|     119 | 11559 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|     116 | 11560 | `					&pGen->pVm->sAllocator, mapSize);` |
|     119 | 11561 | `				if( pMap ){` |
|     119 | 11562 | `					SyZero(pMap, mapSize);` |
|     119 | 11563 | `					pMap->bHasNamed = 1;` |
|     119 | 11564 | `					pMap->nTotal = (sxu32)nArgs;` |
|     119 | 11565 | `					pMap->aNames = (SyString *)&pMap[1];` |
|     119 | 11566 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     347 | 11567 | `					for( n = 0; n < nArgs; ++n ){` |
|     231 | 11568 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     213 | 11569 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|     213 | 11570 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|     213 | 11571 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|     213 | 11572 | `							zBuf += nb;` |
|     105 | 11573 | `						}` |
|       - | 11574 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|     117 | 11575 | `					}` |
|     119 | 11576 | `					p3 = (void *)pMap;` |
|      58 | 11577 | `				}` |
|       - | 11578 | `				}` |
|      58 | 11579 | `			}` |
|       - | 11580 | `			/* Remove stale flags now */` |
|  464763 | 11581 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  232379 | 11582 | `		}` |
|       - | 11583 | `		{` |
|       - | 11584 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|       - | 11585 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|       - | 11586 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|       - | 11587 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|       - | 11588 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|       - | 11589 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|       - | 11590 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|       - | 11591 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
| 1375709 | 11592 | `			sxi32 iLeftFlags = iFlags;` |
| 1375704 | 11593 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
| 1050623 | 11594 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|  362796 | 11595 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  354596 | 11596 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   16597 | 11597 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|    8296 | 11598 | `			}` |
|       - | 11599 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|       - | 11600 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|       - | 11601 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|       - | 11602 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|       - | 11603 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|       - | 11604 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|       - | 11605 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
| 1375704 | 11606 | `			if( pNode->pOp` |
| 1971724 | 11607 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
| 1283918 | 11608 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
| 1192081 | 11609 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|  184017 | 11610 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   92006 | 11611 | `			}` |
| 1375709 | 11612 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|       - | 11613 | `		}` |
| 1375709 | 11614 | `		if( rc != SXRET_OK ){` |
|      34 | 11615 | `			return rc;` |
|       - | 11616 | `		}` |
| 1375679 | 11617 | `		if( !bIsChainOp ){` |
|       - | 11618 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|       - | 11619 | `			 * target the end of that LHS chain, which is right here. */` |
|  632027 | 11620 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  316011 | 11621 | `		}` |
| 1375679 | 11622 | `		if( iVmOp == PH7_OP_CALL ){` |
|  464763 | 11623 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  464763 | 11624 | `			if( pInstr ){` |
|  464763 | 11625 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  459295 | 11626 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 11627 | `					sxu32 nQual;` |
|  459295 | 11628 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 11629 | `					/* Prevent constant expansion but preserve the absolute flag` |
|       - | 11630 | `					 * so the later NEW handler (if any) can see it. */` |
|  459295 | 11631 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|       - | 11632 | `					/* Namespace-qualify the function name for CALL, unless the` |
|       - | 11633 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|       - | 11634 | `					 * imports — class imports must NOT affect function` |
|       - | 11635 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|       - | 11636 | `					 * before NEW; we store the original literal index in the` |
|       - | 11637 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|       - | 11638 | `					 * the unqualified name and re-qualify with class imports. */` |
|  459295 | 11639 | `					if( bAbsolute ){` |
|    3723 | 11640 | `						pInstr->iP2 = (sxi32)nOrig;` |
|    1864 | 11641 | `					}else{` |
|  455577 | 11642 | `						int fromImport = 0;` |
|  455577 | 11643 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  455577 | 11644 | `						pInstr->iP2 = (sxi32)nQual;` |
|  455577 | 11645 | `						if( nQual != nOrig ){` |
|       - | 11646 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 11647 | `							 * NEW handler can recover the unqualified name. */` |
|      77 | 11648 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      77 | 11649 | `							if( !fromImport ){` |
|       - | 11650 | `								/* Mark as namespace-qualified via VmCallArgMap */` |
|      67 | 11651 | `								if( p3 == 0 ){` |
|      67 | 11652 | `									VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      62 | 11653 | `										&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      67 | 11654 | `									if( pMap ){` |
|      67 | 11655 | `										SyZero(pMap, sizeof(VmCallArgMap));` |
|      67 | 11656 | `										p3 = (void *)pMap;` |
|      31 | 11657 | `									}` |
|      31 | 11658 | `								}` |
|      67 | 11659 | `								if( p3 ){` |
|      67 | 11660 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|      31 | 11661 | `								}` |
|      31 | 11662 | `							}` |
|      36 | 11663 | `						}` |
|       5 | 11664 | `					}` |
|  235118 | 11665 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 11666 | `					/* Method call,flag that */` |
|    1345 | 11667 | `					pInstr->iP2 = 1;` |
|     670 | 11668 | `				}` |
|  232384 | 11669 | `			}` |
| 1143300 | 11670 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 11671 | `			ph7_expr_node **apNode;` |
|       - | 11672 | `			sxi32 n;` |
|   94887 | 11673 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|       - | 11674 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|       - | 11675 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|       - | 11676 | `			/* Recurse and generate bytecodes for array index */` |
|   94887 | 11677 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  171219 | 11678 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   76337 | 11679 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   76337 | 11680 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   76337 | 11681 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 11682 | `					return rc;` |
|       - | 11683 | `				}` |
|       - | 11684 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   76337 | 11685 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   38171 | 11686 | `			}` |
|   94887 | 11687 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   76337 | 11688 | `				iP1 = 1; /* Node have an index associated with it */` |
|   38166 | 11689 | `			}` |
|   94887 | 11690 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       - | 11691 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|     241 | 11692 | `				iP2 = 4;` |
|   94769 | 11693 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       - | 11694 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|       - | 11695 | `				 * so the trailing unset() builtin can drop the slot. */` |
|      56 | 11696 | `				iP2 = 5;` |
|   94625 | 11697 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       - | 11698 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|       - | 11699 | `				 * short-circuit on missing keys without invoking offsetGet` |
|       - | 11700 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|      29 | 11701 | `				iP2 = 6;` |
|   94587 | 11702 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 11703 | `				/* Create an empty entry when the desired index is not found */` |
|   37407 | 11704 | `				iP2 = 1;` |
|   18706 | 11705 | `			}` |
|  863480 | 11706 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 11707 | `			/* POP the left node */` |
|      32 | 11708 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 11709 | `		}` |
|  687837 | 11710 | `	}` |
| 1375719 | 11711 | `	rc = SXRET_OK;` |
| 1375719 | 11712 | `	nJmpIdx = 0;` |
|       - | 11713 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 11714 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 11715 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 1375719 | 11716 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     381 | 11717 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     381 | 11718 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     381 | 11719 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     381 | 11720 | `			int isSpecial = 0;` |
|     381 | 11721 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     285 | 11722 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     285 | 11723 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     280 | 11724 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     280 | 11725 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     134 | 11726 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      98 | 11727 | `					isSpecial = 1;` |
|      47 | 11728 | `				}` |
|     164 | 11729 | `			}` |
|     429 | 11730 | `			pInstr->iP1 = 0;` |
|     429 | 11731 | `			if( !isSpecial ){` |
|     239 | 11732 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     117 | 11733 | `			}` |
|       - | 11734 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 11735 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     333 | 11736 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     239 | 11737 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     239 | 11738 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      46 | 11739 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      48 | 11740 | `					return SXRET_OK;` |
|       - | 11741 | `				}` |
|      95 | 11742 | `			}` |
|     142 | 11743 | `		}` |
|     223 | 11744 | `	}` |
|       - | 11745 | `	/* Generate code for the right tree */` |
| 1375637 | 11746 | `	if( pNode->pRight ){` |
|  742167 | 11747 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 11748 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|   11583 | 11749 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  736378 | 11750 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 11751 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    3871 | 11752 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  728656 | 11753 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 11754 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     129 | 11755 | `			iVmOp = 0; /* No binary operator to emit */` |
|     129 | 11756 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  726712 | 11757 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       - | 11758 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|       - | 11759 | `			 * the entire containing postfix chain to null. The jump target is` |
|       - | 11760 | `			 * patched later by the innermost non-chain ancestor (or by` |
|       - | 11761 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|       - | 11762 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|       - | 11763 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|     106 | 11764 | `			sxu32 nNsJmp = 0;` |
|     106 | 11765 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|     106 | 11766 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  726548 | 11767 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|       - | 11768 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|       - | 11769 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|       - | 11770 | `			 * auto-created — PHP auto-vivifies on write. */` |
|  308807 | 11771 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|  154401 | 11772 | `		}` |
|  742167 | 11773 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  742167 | 11774 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  742167 | 11775 | `		if( !bIsChainOp ){` |
|       - | 11776 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|       - | 11777 | `			 * operator instruction is emitted. */` |
|  558199 | 11778 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  279097 | 11779 | `		}` |
|  742167 | 11780 | `		if( iVmOp == PH7_OP_STORE ){` |
|  304855 | 11781 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  304824 | 11782 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 11783 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 11784 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 11785 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 11786 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 11787 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 11788 | `				 */` |
|      80 | 11789 | `				iVmOp = 0;` |
|  304817 | 11790 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  304779 | 11791 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 11792 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   81687 | 11793 | `					iP2 = 1;` |
|   40846 | 11794 | `				}else{` |
|  223097 | 11795 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 11796 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   37331 | 11797 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   37331 | 11798 | `						iP1 = pInstr->iP1;` |
|   18668 | 11799 | `					}else{` |
|  185771 | 11800 | `						p3 = pInstr->p3;` |
|       - | 11801 | `					}` |
|       - | 11802 | `					/* POP the last dynamic load instruction */` |
|  223097 | 11803 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 11804 | `				}` |
|  152392 | 11805 | `			}` |
|  589742 | 11806 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      54 | 11807 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      54 | 11808 | `			if( pInstr ){` |
|      54 | 11809 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 11810 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 11811 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 11812 | `					 */` |
|      17 | 11813 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      17 | 11814 | `					iP1 = pInstr->iP1;` |
|      17 | 11815 | `					iP2 = pInstr->iP2;` |
|      17 | 11816 | `					p3  = pInstr->p3;` |
|       9 | 11817 | `				}else{` |
|      38 | 11818 | `					p3 = pInstr->p3;` |
|       - | 11819 | `				}` |
|      26 | 11820 | `			}` |
|      26 | 11821 | `		}` |
|  371081 | 11822 | `	}` |
| 1375632 | 11823 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|   12012 | 11824 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|       - | 11825 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|       - | 11826 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|      30 | 11827 | `		iVmOp = 0;` |
|      13 | 11828 | `	}` |
| 1375637 | 11829 | `	if( iVmOp > 0 ){` |
| 1375381 | 11830 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   15161 | 11831 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 11832 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|   11095 | 11833 | `				iP1 = 1;` |
|    5550 | 11834 | `			}` |
| 1367803 | 11835 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 11836 | `			/* Namespace-qualify the class name for NEW */ {` |
|   23775 | 11837 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   23775 | 11838 | `				VmInstr *pCallInstr = 0;` |
|   23775 | 11839 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   23583 | 11840 | `					pCallInstr = pPeek;` |
|   23583 | 11841 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|   11789 | 11842 | `				}` |
|   23775 | 11843 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   23773 | 11844 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 11845 | `					sxu32 nLitForClass;` |
|       - | 11846 | `					/* If the CALL handler already qualified the name using` |
|       - | 11847 | `					 * function imports, recover the original unqualified` |
|       - | 11848 | `					 * literal so we can re-qualify with class imports. */` |
|   23773 | 11849 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      37 | 11850 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      21 | 11851 | `					}else{` |
|   23741 | 11852 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 11853 | `					}` |
|   23773 | 11854 | `					pPeek->iP1 = 0;` |
|   23773 | 11855 | `					if( !bAbsolute ){` |
|   20059 | 11856 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|   10032 | 11857 | `					}else{` |
|    3719 | 11858 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|       - | 11859 | `					}` |
|   11884 | 11860 | `				}` |
|       - | 11861 | `			}` |
|   23775 | 11862 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   23775 | 11863 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 11864 | `				VmInstr *pPrev;` |
|   23583 | 11865 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   23583 | 11866 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 11867 | `					/* Pop the call instruction, preserve named-arg map */` |
|   23583 | 11868 | `					iP1 = pInstr->iP1;` |
|   23583 | 11869 | `					if( pInstr->p3 ){` |
|      43 | 11870 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|      19 | 11871 | `					}` |
|   23583 | 11872 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|   11789 | 11873 | `				}` |
|   11794 | 11874 | `			}` |
| 1348340 | 11875 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 11876 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 11877 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     203 | 11878 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     203 | 11879 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     203 | 11880 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     203 | 11881 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     203 | 11882 | `				int isSpecialIs = 0;` |
|     203 | 11883 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     199 | 11884 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     199 | 11885 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     194 | 11886 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     199 | 11887 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      98 | 11888 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      12 | 11889 | `						isSpecialIs = 1;` |
|       5 | 11890 | `					}` |
|      98 | 11891 | `				}` |
|     205 | 11892 | `				pInstr->iP1 = 0;` |
|     205 | 11893 | `				if( !isSpecialIs && !bAbsolute ){` |
|     181 | 11894 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      88 | 11895 | `				}` |
|     103 | 11896 | `			}` |
| 1336359 | 11897 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 11898 | `			/* Prevent constant expansion for member/property names.` |
|       - | 11899 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 11900 | `			 * should not trigger constant lookup. */` |
|  183973 | 11901 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  183973 | 11902 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  183925 | 11903 | `				pInstr->iP1 = 0;` |
|   91960 | 11904 | `			}` |
|  183973 | 11905 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 11906 | `				/* Static member access,remember that */` |
|     299 | 11907 | `				iP1 = 1;` |
|     299 | 11908 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     299 | 11909 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      42 | 11910 | `					p3 = pInstr->p3;` |
|      42 | 11911 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      19 | 11912 | `				}` |
|     147 | 11913 | `			}` |
|       - | 11914 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|       - | 11915 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|       - | 11916 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|       - | 11917 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|  183973 | 11918 | `			if( iP2 == PH7_MEMBER_READ ){` |
|  183973 | 11919 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|      30 | 11920 | `					iP2 = PH7_MEMBER_UNSET;` |
|  183959 | 11921 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|      77 | 11922 | `					iP2 = PH7_MEMBER_ISSET;` |
|  183909 | 11923 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|      13 | 11924 | `					iP2 = PH7_MEMBER_EMPTY;` |
|  183867 | 11925 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|       - | 11926 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|   81767 | 11927 | `					iP2 = PH7_MEMBER_WRITE;` |
|   40881 | 11928 | `				}` |
|   91984 | 11929 | `			}` |
|   91984 | 11930 | `		}` |
|       - | 11931 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|       - | 11932 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|       - | 11933 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|       - | 11934 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|       - | 11935 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
| 1375379 | 11936 | `		if( bFcc ){` |
|      65 | 11937 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|      65 | 11938 | `			iP2 = 0;` |
|      65 | 11939 | `			p3 = 0;` |
|      65 | 11940 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      65 | 11941 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 11942 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|       - | 11943 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|       - | 11944 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|       - | 11945 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|      31 | 11946 | `				void *pMemberName = pInstr->p3;` |
|      31 | 11947 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|      31 | 11948 | `				if( pMemberName ){` |
|       3 | 11949 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|       1 | 11950 | `				}` |
|      31 | 11951 | `				iP1 = 2;` |
|      16 | 11952 | `			}else{` |
|      35 | 11953 | `				iP1 = 1;` |
|       - | 11954 | `			}` |
|      32 | 11955 | `		}` |
|       - | 11956 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|       - | 11957 | `		 * This is the primary emit path for user-visible calls. */` |
| 1375379 | 11958 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  488469 | 11959 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  244232 | 11960 | `		}` |
|       - | 11961 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 1375379 | 11962 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  687687 | 11963 | `	}` |
| 1375635 | 11964 | `	if( nJmpIdx > 0 ){` |
|       - | 11965 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   15573 | 11966 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   15573 | 11967 | `		if( pInstr ){` |
|   15573 | 11968 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    7784 | 11969 | `		}` |
|    7784 | 11970 | `	}` |
| 1375635 | 11971 | `	return rc;` |
| 1834227 | 11972 | `}` |
|       - | 11973 | `/*` |
|       - | 11974 | ` * Compile a PHP expression.` |
|       - | 11975 | ` * According to the PHP language reference manual:` |
|       - | 11976 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 11977 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 11978 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 11979 | ` *  is "anything that has a value".` |
|       - | 11980 | ` * If something goes wrong while compiling the expression,this` |
|       - | 11981 | ` * function takes care of generating the appropriate error` |
|       - | 11982 | ` * message.` |
|       - | 11983 | ` */` |
|  988122 | 11984 | `static sxi32 PH7_CompileExpr(` |
|       - | 11985 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 11986 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 11987 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 11988 | `	)` |
|       5 | 11989 | `{` |
|       - | 11990 | `	ph7_expr_node *pRoot;` |
|       - | 11991 | `	SySet sExprNode;` |
|       - | 11992 | `	SyToken *pEnd;` |
|       - | 11993 | `	sxi32 nExpr;` |
|       - | 11994 | `	sxi32 iNest;` |
|       - | 11995 | `	sxi32 rc;` |
|       - | 11996 | `	sxu32 nNullsafeBase;` |
|       - | 11997 | `	/* Initialize worker variables */` |
|  988127 | 11998 | `	nExpr = 0;` |
|  988127 | 11999 | `	pRoot = 0;` |
|       - | 12000 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|       - | 12001 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  988127 | 12002 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  988127 | 12003 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  988127 | 12004 | `	SySetAlloc(&sExprNode,0x10);` |
|  988127 | 12005 | `	rc = SXRET_OK;` |
|       - | 12006 | `	/* Delimit the expression */` |
|  988127 | 12007 | `	pEnd = pGen->pIn;` |
|  988127 | 12008 | `	iNest = 0;` |
| 6666265 | 12009 | `	while( pEnd < pGen->pEnd ){` |
| 6325985 | 12010 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 12011 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     517 | 12012 | `			iNest++;` |
| 6325729 | 12013 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     525 | 12014 | `			iNest--;` |
| 6325213 | 12015 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  648219 | 12016 | `			if( iNest <= 0 ){` |
|  647847 | 12017 | `				break;` |
|       - | 12018 | `			}` |
|     186 | 12019 | `		}` |
| 5678143 | 12020 | `		pEnd++;` |
|       5 | 12021 | `	}` |
|  988127 | 12022 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   22729 | 12023 | `		SyToken *pEnd2 = pGen->pIn;` |
|   22729 | 12024 | `		iNest = 0;` |
|       - | 12025 | `		/* Stop at the first comma */` |
|   45771 | 12026 | `		while( pEnd2 < pEnd ){` |
|   23053 | 12027 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      71 | 12028 | `				iNest++;` |
|   23020 | 12029 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      71 | 12030 | `				iNest--;` |
|   22954 | 12031 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      57 | 12032 | `				if( iNest <= 0 ){` |
|       7 | 12033 | `					break;` |
|       - | 12034 | `				}` |
|      23 | 12035 | `			}` |
|   23047 | 12036 | `			pEnd2++;` |
|       5 | 12037 | `		}` |
|   22729 | 12038 | `		if( pEnd2 <pEnd ){` |
|       7 | 12039 | `			pEnd = pEnd2;` |
|       3 | 12040 | `		}` |
|   11362 | 12041 | `	}` |
|  988127 | 12042 | `	if( pEnd > pGen->pIn ){` |
|  988117 | 12043 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 12044 | `		/* Swap delimiter */` |
|  988117 | 12045 | `		pGen->pEnd = pEnd;` |
|       - | 12046 | `		/* Try to get an expression tree */` |
|  988117 | 12047 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  988117 | 12048 | `		if( rc == SXRET_OK && pRoot ){` |
|  987935 | 12049 | `			rc = SXRET_OK;` |
|  987935 | 12050 | `			if( xTreeValidator ){` |
|       - | 12051 | `				/* Call the upper layer validator callback */` |
|   30119 | 12052 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   15057 | 12053 | `			}` |
|  987935 | 12054 | `			if( rc != SXERR_ABORT ){` |
|       - | 12055 | `				/* Generate code for the given tree */` |
|  987935 | 12056 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|       - | 12057 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|       - | 12058 | `				 * expression so they short-circuit to its end. */` |
|  987935 | 12059 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  493965 | 12060 | `			}` |
|  987935 | 12061 | `			nExpr = 1;` |
|  493965 | 12062 | `		}` |
|       - | 12063 | `		/* Release the whole tree */` |
|  988117 | 12064 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 12065 | `		/* Synchronize token stream */` |
|  988117 | 12066 | `		pGen->pEnd = pTmp;` |
|  988117 | 12067 | `		pGen->pIn  = pEnd;` |
|  988117 | 12068 | `		if( rc == SXERR_ABORT ){` |
|      13 | 12069 | `			SySetRelease(&sExprNode);` |
|      13 | 12070 | `			return SXERR_ABORT;` |
|       - | 12071 | `		}` |
|  494051 | 12072 | `	}` |
|  988117 | 12073 | `	SySetRelease(&sExprNode);` |
|  988117 | 12074 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  494066 | 12075 | `}` |
|       - | 12076 | `/*` |
|       - | 12077 | ` * Return a pointer to the node construct handler associated` |
|       - | 12078 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 12079 | ` */` |
|  258266 | 12080 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       5 | 12081 | `{` |
|  258271 | 12082 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 12083 | `		/* Numeric literal: Either real or integer */` |
|  130165 | 12084 | `		return PH7_CompileNumLiteral;` |
|  128111 | 12085 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 12086 | `		/* Double quoted string */` |
|   24345 | 12087 | `		return PH7_CompileString;` |
|  103771 | 12088 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 12089 | `		/* Single quoted string */` |
|  103651 | 12090 | `		return PH7_CompileSimpleString;` |
|     125 | 12091 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 12092 | `		/* Heredoc */` |
|      70 | 12093 | `		return PH7_CompileHereDoc;` |
|      59 | 12094 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 12095 | `		/* Nowdoc */` |
|      52 | 12096 | `		return PH7_CompileNowDoc;` |
|       8 | 12097 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 12098 | `		/* Backtick quoted string */` |
|       6 | 12099 | `		return PH7_CompileBacktic;` |
|       - | 12100 | `	}` |
|       3 | 12101 | `	return 0;` |
|  129138 | 12102 | `}` |
|       - | 12103 | `/*` |
|       - | 12104 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|       - | 12105 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|       - | 12106 | ` * in write context" parse error.` |
|       - | 12107 | ` */` |
|    6808 | 12108 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|       5 | 12109 | `{` |
|       - | 12110 | `	sxi32 rc;` |
|    6813 | 12111 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|    6811 | 12112 | `		return SXRET_OK;` |
|       - | 12113 | `	}` |
|       5 | 12114 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       2 | 12115 | `		pNode ? pNode->pStart->nLine : 1,` |
|       - | 12116 | `		"Can't use nullsafe operator in write context");` |
|       3 | 12117 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|    3409 | 12118 | `}` |
|       - | 12119 | `/*` |
|       - | 12120 | ` * Compile an unset() statement.` |
|       - | 12121 | ` * unset($var, $arr[$key], ...);` |
|       - | 12122 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 12123 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 12124 | ` * parent array before extracting the element to unset.` |
|       - | 12125 | ` */` |
|    2962 | 12126 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       5 | 12127 | `{` |
|    2967 | 12128 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2967 | 12129 | `	sxu32 nIdx = 0;` |
|       - | 12130 | `	SyString sName;` |
|       - | 12131 | `	sxi32 rc;` |
|       - | 12132 | `	/* Jump the 'unset' keyword */` |
|    2967 | 12133 | `	pGen->pIn++;` |
|       - | 12134 | `	/* Save delimiter */` |
|    2967 | 12135 | `	pTmp = pGen->pEnd;` |
|       - | 12136 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2967 | 12137 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2967 | 12138 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 12139 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 12140 | `		SyToken *pClose;` |
|    2967 | 12141 | `		pGen->pIn++;   /* Skip '(' */` |
|    2967 | 12142 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2967 | 12143 | `		pEnd = pClose; /* Stop at ')' */` |
|    1481 | 12144 | `	}` |
|    2967 | 12145 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 12146 | `	/* Resolve the 'unset' builtin name once */` |
|    2967 | 12147 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     367 | 12148 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     367 | 12149 | `		if( pObj == 0 ){` |
|     ! 0 | 12150 | `			return SXERR_ABORT;` |
|       - | 12151 | `		}` |
|     367 | 12152 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     367 | 12153 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     181 | 12154 | `	}` |
|       - | 12155 | `	/* Compile each comma-separated argument */` |
|    9777 | 12156 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6815 | 12157 | `		if( pGen->pIn < pNext ){` |
|    6815 | 12158 | `			pGen->pEnd = pNext;` |
|    6815 | 12159 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 12160 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|       - | 12161 | `				GenStateUnsetValidator);` |
|    6815 | 12162 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 12163 | `				return SXERR_ABORT;` |
|       - | 12164 | `			}` |
|    6815 | 12165 | `			if( rc != SXERR_EMPTY ){` |
|       - | 12166 | `				/* Emit call for this single argument */` |
|    6813 | 12167 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6813 | 12168 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|    6813 | 12169 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3404 | 12170 | `			}` |
|    3405 | 12171 | `		}` |
|       - | 12172 | `		/* Jump trailing commas */` |
|   10665 | 12173 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3855 | 12174 | `			pNext++;` |
|       5 | 12175 | `		}` |
|    6815 | 12176 | `		pGen->pIn = pNext;` |
|       5 | 12177 | `	}` |
|       - | 12178 | `	/* Skip past the closing ')' if present */` |
|    2967 | 12179 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2967 | 12180 | `		pGen->pIn++;` |
|    1481 | 12181 | `	}` |
|       - | 12182 | `	/* Restore token stream */` |
|    2967 | 12183 | `	pGen->pEnd = pTmp;` |
|    2967 | 12184 | `	return SXRET_OK;` |
|    1486 | 12185 | `}` |
|       - | 12186 | `/*` |
|       - | 12187 | ` * PHP Language construct table.` |
|       - | 12188 | ` */` |
|       - | 12189 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 12190 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 12191 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 12192 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 12193 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 12194 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 12195 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 12196 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 12197 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 12198 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 12199 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 12200 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 12201 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 12202 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 12203 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 12204 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 12205 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 12206 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 12207 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 12208 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 12209 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 12210 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 12211 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 12212 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 12213 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 12214 | `};` |
|       - | 12215 | `/*` |
|       - | 12216 | ` * Return a pointer to the statement handler routine associated` |
|       - | 12217 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 12218 | ` */` |
|  669826 | 12219 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 12220 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 12221 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 12222 | `	)` |
|       5 | 12223 | `{` |
|  669831 | 12224 | `	sxu32 n = 0;` |
| 3521313 | 12225 | `	for(;;){` |
| 7042631 | 12226 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|  145613 | 12227 | `			break;` |
|       - | 12228 | `		}` |
| 6897023 | 12229 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  524223 | 12230 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 12231 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 12232 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 12233 | `					/* 'static' (class context),return null */` |
|     ! 0 | 12234 | `					return 0;` |
|       - | 12235 | `				}` |
|     ! 0 | 12236 | `			}` |
|  524218 | 12237 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       8 | 12238 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       9 | 12239 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - | 12240 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 | 12241 | `				return 0;` |
|       - | 12242 | `			}` |
|       - | 12243 | `			/* Return a pointer to the handler.` |
|       - | 12244 | `			*/` |
|  524223 | 12245 | `			return aLangConstruct[n].xConstruct;` |
|       - | 12246 | `		}` |
| 6372805 | 12247 | `		n++;` |
|       5 | 12248 | `	}` |
|  145613 | 12249 | `	if( pLookahed ){` |
|  145613 | 12250 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|   40679 | 12251 | `			return PH7_CompileClassInterface;` |
|  104939 | 12252 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|  104541 | 12253 | `			return PH7_CompileClass;` |
|     403 | 12254 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      69 | 12255 | `			return PH7_CompileTrait;` |
|       - | 12256 | `		}` |
|       - | 12257 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|       - | 12258 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|       - | 12259 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|       - | 12260 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     167 | 12261 | `	}` |
|       - | 12262 | `	/* Not a language construct */` |
|     339 | 12263 | `	return 0;` |
|  334918 | 12264 | `}` |
|       - | 12265 | `/*` |
|       - | 12266 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 12267 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 12268 | ` */` |
|     334 | 12269 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       5 | 12270 | `{` |
|       - | 12271 | `	int rc;` |
|     339 | 12272 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     339 | 12273 | `	if( rc == FALSE ){` |
|     224 | 12274 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     223 | 12275 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 12276 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 12277 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 12278 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 12279 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 12280 | `			*/` |
|       - | 12281 | `			){` |
|     221 | 12282 | `				rc = TRUE;` |
|     108 | 12283 | `		}` |
|     112 | 12284 | `	}` |
|     339 | 12285 | `	return rc;` |
|       5 | 12286 | `}` |
|       - | 12287 | `/*` |
|       - | 12288 | ` * Compile a PHP chunk.` |
|       - | 12289 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 12290 | ` * takes care of generating the appropriate error message.` |
|       - | 12291 | ` */` |
|  799880 | 12292 | `static sxi32 GenStateCompileChunk(` |
|       - | 12293 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 12294 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 12295 | `	)` |
|       5 | 12296 | `{` |
|       - | 12297 | `	ProcLangConstruct xCons;` |
|       - | 12298 | `	sxi32 rc;` |
|  799885 | 12299 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  630368 | 12300 | `	for(;;){` |
| 1030313 | 12301 | `		int bStmtIsDeclare = 0;` |
| 1030313 | 12302 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 12303 | `			/* No more input to process */` |
|   18081 | 12304 | `			break;` |
|       - | 12305 | `		}` |
|       - | 12306 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|       - | 12307 | `		 * below doesn't fire before the directive has a chance to run. */` |
| 1012237 | 12308 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  673555 | 12309 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  673555 | 12310 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|      45 | 12311 | `				bStmtIsDeclare = 1;` |
|      20 | 12312 | `			}` |
|  336775 | 12313 | `		}` |
| 1012237 | 12314 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|       - | 12315 | `			/* Any non-declare top-level statement locks the strict_types` |
|       - | 12316 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|  230403 | 12317 | `			pGen->bStrictTypesLocked = 1;` |
|  115199 | 12318 | `		}` |
| 1012237 | 12319 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 12320 | `			/* Compile block */` |
|    3713 | 12321 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|    3713 | 12322 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 12323 | `				break;` |
|       - | 12324 | `			}` |
|    1859 | 12325 | `		}else{` |
| 1008529 | 12326 | `			xCons = 0;` |
| 1008529 | 12327 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|       - | 12328 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|       - | 12329 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|       - | 12330 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|    3755 | 12331 | `				xCons = PH7_CompileClassModifiers;` |
| 1006654 | 12332 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  669831 | 12333 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 12334 | `				/* Try to extract a language construct handler */` |
|  669831 | 12335 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  669831 | 12336 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 12337 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 12338 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 12339 | `						&pGen->pIn->sData);` |
|       9 | 12340 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 12341 | `						break;` |
|       - | 12342 | `					}` |
|       - | 12343 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 12344 | `					 * this erroneous statement.` |
|       - | 12345 | `					 */` |
|       9 | 12346 | `					xCons = PH7_ErrorRecover;` |
|       4 | 12347 | `				}` |
|  669866 | 12348 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   54943 | 12349 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 12350 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     117 | 12351 | `				xCons = PH7_CompileLabel;` |
|      56 | 12352 | `			}` |
| 1008529 | 12353 | `			if( xCons == 0 ){` |
|       - | 12354 | `				/* Assume an expression an try to compile it */` |
|  335167 | 12355 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  335167 | 12356 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 12357 | `					/* Pop l-value */` |
|  335017 | 12358 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  167506 | 12359 | `				}` |
|  167586 | 12360 | `			}else{` |
|       - | 12361 | `				/* Go compile the sucker */` |
|  673367 | 12362 | `				rc = xCons(&(*pGen));` |
|       - | 12363 | `			}` |
| 1008529 | 12364 | `			if( rc == SXERR_ABORT ){` |
|       - | 12365 | `				/* Request to abort compilation */` |
|      13 | 12366 | `				break;` |
|       - | 12367 | `			}` |
|       - | 12368 | `		}` |
|       - | 12369 | `		/* Ignore trailing semi-colons ';' */` |
| 1629697 | 12370 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  617475 | 12371 | `			pGen->pIn++;` |
|       5 | 12372 | `		}` |
| 1012227 | 12373 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 12374 | `			/* Compile a single statement and return */` |
|  781799 | 12375 | `			break;` |
|       - | 12376 | `		}` |
|       - | 12377 | `		/* LOOP ONE */` |
|       - | 12378 | `		/* LOOP TWO */` |
|       - | 12379 | `		/* LOOP THREE */` |
|       - | 12380 | `		/* LOOP FOUR */` |
|       5 | 12381 | `	}` |
|       - | 12382 | `	/* Return compilation status */` |
|  799885 | 12383 | `	return rc;` |
|       5 | 12384 | `}` |
|       - | 12385 | `/*` |
|       - | 12386 | ` * Compile a Raw PHP chunk.` |
|       - | 12387 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 12388 | ` * takes care of generating the appropriate error message.` |
|       - | 12389 | ` */` |
|   18088 | 12390 | `static sxi32 PH7_CompilePHP(` |
|       - | 12391 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 12392 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 12393 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 12394 | `	)` |
|       5 | 12395 | `{` |
|   18093 | 12396 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 12397 | `	sxi32 rc;` |
|       - | 12398 | `	/* Reset the token set */` |
|   18093 | 12399 | `	SySetReset(&(*pTokenSet));` |
|       - | 12400 | `	/* Mark as the default token set */` |
|   18093 | 12401 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 12402 | `	/* Advance the stream cursor */` |
|   18093 | 12403 | `	pGen->pRawIn++;` |
|       - | 12404 | `	/* Tokenize the PHP chunk first */` |
|   18093 | 12405 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 12406 | `	/* Point to the head and tail of the token stream. */` |
|   18093 | 12407 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   18093 | 12408 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   18093 | 12409 | `	if( is_expr ){` |
|     ! 0 | 12410 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 12411 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 12412 | `			/* A simple expression,compile it */` |
|     ! 0 | 12413 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 12414 | `		}` |
|       - | 12415 | `		/* Emit the DONE instruction */` |
|     ! 0 | 12416 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 12417 | `		return SXRET_OK;` |
|       - | 12418 | `	}` |
|   18093 | 12419 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 12420 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 12421 | `		/*` |
|       - | 12422 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 12423 | `		 * According to the PHP reference manual:` |
|       - | 12424 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 12425 | `		 *  immediately follow` |
|       - | 12426 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 12427 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 12428 | `		 * Symisc extension:` |
|       - | 12429 | `		 *   This short syntax works with all PHP opening` |
|       - | 12430 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 12431 | `		 *   only short tag.` |
|       - | 12432 | `		 */` |
|       - | 12433 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 12434 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 12435 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 12436 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 12437 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 12438 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 12439 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 12440 | `		}` |
|       3 | 12441 | `		return SXRET_OK;` |
|       - | 12442 | `	}` |
|       - | 12443 | `	/* Compile the PHP chunk */` |
|   18091 | 12444 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 12445 | `	/* Fix exceptions jumps */` |
|   18091 | 12446 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 12447 | `	/* Fix gotos now, the jump destination is resolved */` |
|   18091 | 12448 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 12449 | `		rc = SXERR_ABORT;` |
|       1 | 12450 | `	}` |
|       - | 12451 | `	/* Reset container */` |
|   18091 | 12452 | `	SySetReset(&pGen->aGoto);` |
|   18091 | 12453 | `	SySetReset(&pGen->aLabel);` |
|   18091 | 12454 | `	SySetReset(&pGen->aNullsafeJmp);` |
|       - | 12455 | `	/* Compilation result */` |
|   18091 | 12456 | `	return rc;` |
|    9049 | 12457 | `}` |
|       - | 12458 | `/*` |
|       - | 12459 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 12460 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 12461 | ` * This is the only compile interface exported from this file.` |
|       - | 12462 | ` */` |
|   21108 | 12463 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 12464 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 12465 | `	SyString *pScript,  /* Script to compile */` |
|       - | 12466 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 12467 | `	)` |
|       5 | 12468 | `{` |
|       - | 12469 | `	SySet aPhpToken,aRawToken;` |
|       - | 12470 | `	ph7_gen_state *pCodeGen;` |
|       - | 12471 | `	ph7_value *pRawObj;` |
|       - | 12472 | `	sxu32 nObjIdx;` |
|       - | 12473 | `	sxi32 nRawObj;` |
|       - | 12474 | `	int is_expr;` |
|       - | 12475 | `	sxi8 bSavedStrict;` |
|       - | 12476 | `	sxi8 bSavedStrictLocked;` |
|       - | 12477 | `	sxi32 rc;` |
|   21113 | 12478 | `	if( pScript->nByte < 1 ){` |
|       - | 12479 | `		/* Nothing to compile */` |
|     ! 0 | 12480 | `		return PH7_OK;` |
|       - | 12481 | `	}` |
|       - | 12482 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|       - | 12483 | `	 * file's flags so include/require restore them on return. */` |
|   21113 | 12484 | `	pCodeGen = &pVm->sCodeGen;` |
|   21113 | 12485 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|   21113 | 12486 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|   21113 | 12487 | `	pCodeGen->bStrictTypes = 0;` |
|   21113 | 12488 | `	pCodeGen->bStrictTypesLocked = 0;` |
|       - | 12489 | `	/* Initialize the tokens containers */` |
|   21113 | 12490 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   21113 | 12491 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   21113 | 12492 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   21113 | 12493 | `	is_expr = 0;` |
|   21113 | 12494 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 12495 | `		SyToken sTmp;` |
|       - | 12496 | `		/* PHP only: -*/` |
|    7453 | 12497 | `		sTmp.nLine = 1;` |
|    7453 | 12498 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    7453 | 12499 | `		sTmp.pUserData = 0;` |
|    7453 | 12500 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    7453 | 12501 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    7453 | 12502 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 12503 | `			/* A simple PHP expression */` |
|     ! 0 | 12504 | `			is_expr = 1;` |
|     ! 0 | 12505 | `		}` |
|    3729 | 12506 | `	}else{` |
|       - | 12507 | `		/* Tokenize raw text */` |
|   13665 | 12508 | `		SySetAlloc(&aRawToken,32);` |
|   13665 | 12509 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 12510 | `	}` |
|       - | 12511 | `	/* Process high-level tokens */` |
|   21113 | 12512 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   21113 | 12513 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   21113 | 12514 | `	rc = PH7_OK;` |
|   21113 | 12515 | `	if( is_expr ){` |
|       - | 12516 | `		/* Compile the expression */` |
|     ! 0 | 12517 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 12518 | `		goto cleanup;` |
|       - | 12519 | `	}` |
|   21113 | 12520 | `	nObjIdx = 0;` |
|       - | 12521 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 12522 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 12523 | `	 * preventing namespace bleeding across include()d files. */` |
|   21113 | 12524 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 12525 | `	/* Start the compilation process */` |
|   17390 | 12526 | `	for(;;){` |
|   52861 | 12527 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   21101 | 12528 | `			break; /* No more tokens to process */` |
|       - | 12529 | `		}` |
|   31765 | 12530 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 12531 | `			/* Compile the PHP chunk */` |
|   18093 | 12532 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   18093 | 12533 | `			if( rc == SXERR_ABORT ){` |
|      15 | 12534 | `				break;` |
|       - | 12535 | `			}` |
|   18081 | 12536 | `			continue;` |
|       - | 12537 | `		}` |
|       - | 12538 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   13677 | 12539 | `		nRawObj = 0;` |
|   27391 | 12540 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 12541 | `			/* Consume the raw chunk without any processing */` |
|   13719 | 12542 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   13719 | 12543 | `			if( pRawObj == 0 ){` |
|     ! 0 | 12544 | `				rc = SXERR_MEM;` |
|     ! 0 | 12545 | `				break;` |
|       - | 12546 | `			}` |
|       - | 12547 | `			/* Mark as constant and emit the load constant instruction */` |
|   13719 | 12548 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   13719 | 12549 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   13719 | 12550 | `			++nRawObj;` |
|   13719 | 12551 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       5 | 12552 | `		}` |
|   13677 | 12553 | `		if( nRawObj > 0 ){` |
|       - | 12554 | `			/* Emit the consume instruction */` |
|   13677 | 12555 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    6836 | 12556 | `		}` |
|   10559 | 12557 | `	}` |
|   10554 | 12558 | `cleanup:` |
|   21113 | 12559 | `	SySetRelease(&aRawToken);` |
|   21113 | 12560 | `	SySetRelease(&aPhpToken);` |
|       - | 12561 | `	/* Restore outer file's strict_types scope */` |
|   21113 | 12562 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|   21113 | 12563 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|   21113 | 12564 | `	return rc;` |
|   10559 | 12565 | `}` |
|       - | 12566 | `/*` |
|       - | 12567 | ` * Utility routines.Initialize the code generator.` |
|       - | 12568 | ` */` |
|    3690 | 12569 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 12570 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 12571 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 12572 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 12573 | `	)` |
|       5 | 12574 | `{` |
|    3695 | 12575 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 12576 | `	/* Zero the structure */` |
|    3695 | 12577 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 12578 | `	/* Initial state */` |
|    3695 | 12579 | `	pGen->pVm  = &(*pVm);` |
|    3695 | 12580 | `	pGen->xErr = xErr;` |
|    3695 | 12581 | `	pGen->pErrData = pErrData;` |
|    3695 | 12582 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    3695 | 12583 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    3695 | 12584 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|    3695 | 12585 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    3695 | 12586 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 12587 | `	/* Error log buffer */` |
|    3695 | 12588 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 12589 | `	/* General purpose working buffer */` |
|    3695 | 12590 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 12591 | `	/* Namespace state */` |
|    3695 | 12592 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    3695 | 12593 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    3695 | 12594 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    3695 | 12595 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 12596 | `	/* Create the global scope */` |
|    3695 | 12597 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 12598 | `	/* Point to the global scope */` |
|    3695 | 12599 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    3695 | 12600 | `	return SXRET_OK;` |
|       5 | 12601 | `}` |
|       - | 12602 | `/*` |
|       - | 12603 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 12604 | ` */` |
|   24428 | 12605 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 12606 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 12607 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 12608 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 12609 | `	)` |
|       5 | 12610 | `{` |
|   24433 | 12611 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 12612 | `	GenBlock *pBlock,*pParent;` |
|       - | 12613 | `	/* Reset state */` |
|   24433 | 12614 | `	SySetReset(&pGen->aLabel);` |
|   24433 | 12615 | `	SySetReset(&pGen->aGoto);` |
|   24433 | 12616 | `	SySetReset(&pGen->aNullsafeJmp);` |
|   24433 | 12617 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   24433 | 12618 | `	SyBlobRelease(&pGen->sWorker);` |
|   24433 | 12619 | `	SyBlobRelease(&pGen->sNamespace);` |
|   24433 | 12620 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   24433 | 12621 | `	SyHashRelease(&pGen->hUseImports);` |
|   24433 | 12622 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   24433 | 12623 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   24433 | 12624 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   24433 | 12625 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   24433 | 12626 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 12627 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 12628 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 12629 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 12630 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 12631 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 12632 | `	 * number of unique names, which is acceptable. */` |
|       - | 12633 | `	/* Point to the global scope */` |
|   24433 | 12634 | `	pBlock = pGen->pCurrent;` |
|   24433 | 12635 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 12636 | `		pParent = pBlock->pParent;` |
|     ! 0 | 12637 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 12638 | `		pBlock = pParent;` |
|     ! 0 | 12639 | `	}` |
|   24433 | 12640 | `	pGen->xErr = xErr;` |
|   24433 | 12641 | `	pGen->pErrData = pErrData;` |
|   24433 | 12642 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   24433 | 12643 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   24433 | 12644 | `	pGen->pIn = pGen->pEnd = 0;` |
|   24433 | 12645 | `	pGen->nErr = 0;` |
|   24433 | 12646 | `	return SXRET_OK;` |
|       5 | 12647 | `}` |
|       - | 12648 | `/*` |
|       - | 12649 | ` * Generate a compile-time error message.` |
|       - | 12650 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 12651 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 12652 | ` * abort compilation immediately.` |
|       - | 12653 | ` */` |
|     640 | 12654 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       5 | 12655 | `{` |
|     645 | 12656 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     645 | 12657 | `	const char *zErr = "Error";` |
|       - | 12658 | `	SyString *pFile;` |
|       - | 12659 | `	va_list ap;` |
|       - | 12660 | `	sxi32 rc;` |
|       - | 12661 | `	/* Reset the working buffer */` |
|     645 | 12662 | `	SyBlobReset(pWorker);` |
|       - | 12663 | `	/* Peek the processed file path if available */` |
|     645 | 12664 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     645 | 12665 | `	if( nErrType == E_ERROR ){` |
|       - | 12666 | `		/* Increment the error counter */` |
|     531 | 12667 | `		pGen->nErr++;` |
|     531 | 12668 | `		if( pGen->nErr > 15 ){` |
|       - | 12669 | `			/* Error count limit reached */` |
|       6 | 12670 | `			if( pGen->xErr ){` |
|       6 | 12671 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       6 | 12672 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       6 | 12673 | `				if( pFile ){` |
|       6 | 12674 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 12675 | `				}` |
|       6 | 12676 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       6 | 12677 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       6 | 12678 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 12679 | `				}` |
|       2 | 12680 | `			}` |
|       - | 12681 | `			/* Abort immediately */` |
|       6 | 12682 | `			return SXERR_ABORT;` |
|       - | 12683 | `		}` |
|     261 | 12684 | `	}` |
|     641 | 12685 | `	if( pGen->xErr == 0 ){` |
|       - | 12686 | `		/* No available error consumer,return immediately */` |
|       3 | 12687 | `		return SXRET_OK;` |
|       - | 12688 | `	}` |
|     638 | 12689 | `	switch(nErrType){` |
|     524 | 12690 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      32 | 12691 | `	case E_WARNING: zErr = "Warning";     break;` |
|      82 | 12692 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|      11 | 12693 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 12694 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 12695 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 12696 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 12697 | `	default:` |
|     ! 0 | 12698 | `		break;` |
|       - | 12699 | `	}` |
|     638 | 12700 | `	rc = SXRET_OK;` |
|       - | 12701 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     638 | 12702 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     638 | 12703 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     638 | 12704 | `	va_start(ap,zFormat);` |
|     638 | 12705 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     638 | 12706 | `	va_end(ap);` |
|     638 | 12707 | `	if( pFile ){` |
|     638 | 12708 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     317 | 12709 | `	}` |
|       - | 12710 | `	/* Append a new line */` |
|     638 | 12711 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     638 | 12712 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 12713 | `		/* Consume the generated error message */` |
|     638 | 12714 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     317 | 12715 | `	}` |
|     638 | 12716 | `	return rc;` |
|     325 | 12717 | `}` |
|       - | 12718 |  |
